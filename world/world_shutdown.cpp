#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/eq_constants.h"
#include "../common/emu_constants.h"
#include "../common/path_manager.h"
#include "../common/rulesys.h"
#include "../common/servertalk.h"
#include "../common/shutdown_duration.h"
#include "../common/strings.h"
#include "world_shutdown.h"
#include "launcher_list.h"
#include "main.h"
#include "world_config.h"
#include "worlddb.h"
#include "zonelist.h"
#include "zoneserver.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

extern ZSList       zoneserver_list;
extern LauncherList launcher_list;

namespace fs = std::filesystem;

namespace {
	// Below this the countdown no longer follows the interval and escalates on its own.
	constexpr uint32 EscalationStart = 15 * 60;
	constexpr uint32 FinalMarks[]    = { 900, 600, 300, 240, 180, 120, 60, 30, 10 };

	// Give zones a moment to deliver the "shutting down" broadcast before they are told to exit.
	constexpr time_t ZoneSignalDelay = 2;
	// How long to wait for pm2 to stop us before exiting on our own.
	constexpr time_t PM2StopTimeout  = 30;

	std::string FormatUTC(time_t t)
	{
		char buf[32] = { 0 };
		std::tm tm{};
#ifdef _WINDOWS
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tm);
		return buf;
	}

	// Whole minutes once we're past a minute out; seconds are noise in a MOTD or a broadcast.
	std::string FormatRemaining(uint32 seconds)
	{
		return ShutdownDuration::Format(seconds >= 60 ? seconds - (seconds % 60) : seconds);
	}

	fs::path ServerFile(const char *name)
	{
		return fs::path(path.GetServerPath()) / name;
	}
}

WorldShutdown &WorldShutdown::Instance()
{
	static WorldShutdown instance;
	return instance;
}

uint32 WorldShutdown::Remaining() const
{
	const time_t now = std::time(nullptr);
	return m_deadline > now ? static_cast<uint32>(m_deadline - now) : 0;
}

// Largest announcement mark strictly below `remaining`; 0 when there are none left.
uint32 WorldShutdown::NextCheckpoint(uint32 remaining) const
{
	if (remaining > EscalationStart) {
		uint32 step = m_interval;
		if (step == 0) {
			step = remaining > 86400 ? 21600 :
				   remaining > 21600 ? 10800 :
				   remaining > 3600  ? 3600  : 900;
		}

		const uint32 mark = ((remaining - 1) / step) * step;
		return mark > EscalationStart ? mark : EscalationStart;
	}

	for (const auto mark : FinalMarks) {
		if (mark < remaining) {
			return mark;
		}
	}

	return 0;
}

void WorldShutdown::Announce(uint32 remaining)
{
	const auto message = fmt::format(
		"[SYSTEM] The server will shut down in {}.{}",
		FormatRemaining(remaining),
		remaining <= 300 ? " Please find a safe place and log out." : ""
	);

	LogInfo("{}", message);
	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, remaining <= 60 ? Chat::Red : Chat::Yellow, message.c_str());
}

std::string WorldShutdown::GetMOTD() const
{
	std::string motd = RuleS(World, MOTD);
	if (motd.empty()) {
		database.GetVariable("MOTD", motd);
	}

	std::string notice;
	if (m_phase == Phase::Countdown) {
		notice = fmt::format(
			"SERVER SHUTDOWN: The server will shut down in {} (at {}).",
			FormatRemaining(Remaining()),
			FormatUTC(m_deadline)
		);
	}
	else if (IsShuttingDown()) {
		notice = "SERVER SHUTDOWN: The server is shutting down now.";
	}

	if (!notice.empty()) {
		motd = motd.empty() ? notice : notice + " " + motd;

		// ServerMotd_Struct holds 512 bytes; the notice goes first so it survives the cut.
		if (motd.length() > 511) {
			motd.resize(511);
		}
	}

	return motd;
}

void WorldShutdown::PushMOTD()
{
	ServerPacket pack(ServerOP_Motd, sizeof(ServerMotd_Struct));
	auto m = (ServerMotd_Struct *) pack.pBuffer;
	strn0cpy(m->myname, "SYSTEM", sizeof(m->myname));
	strn0cpy(m->motd, GetMOTD().c_str(), sizeof(m->motd));
	zoneserver_list.SendPacket(&pack);
}

std::string WorldShutdown::Schedule(uint32 seconds, uint32 interval_seconds, const std::string &requested_by)
{
	if (IsShuttingDown()) {
		return "The world is already shutting down.";
	}

	if (seconds == 0) {
		return ShutdownNow(requested_by);
	}

	const bool rescheduled = m_phase == Phase::Countdown;

	m_phase           = Phase::Countdown;
	m_deadline        = std::time(nullptr) + seconds;
	m_interval        = interval_seconds;
	m_requested_by    = requested_by;
	m_next_checkpoint = NextCheckpoint(seconds);
	StartTicking();

	LogInfo(
		"World shutdown {} by [{}] for [{}] ([{}]), interval [{}]",
		rescheduled ? "rescheduled" : "scheduled",
		requested_by,
		ShutdownDuration::Format(seconds),
		FormatUTC(m_deadline),
		interval_seconds ? ShutdownDuration::Format(interval_seconds) : "automatic"
	);

	Announce(seconds);
	PushMOTD();

	return fmt::format(
		"World shutdown {} for {} from now ({}). Announcements: {} until 15 minutes remain, then escalating.",
		rescheduled ? "rescheduled" : "scheduled",
		ShutdownDuration::Format(seconds),
		FormatUTC(m_deadline),
		interval_seconds ? "every " + ShutdownDuration::Format(interval_seconds) : "automatic"
	);
}

std::string WorldShutdown::ShutdownNow(const std::string &requested_by)
{
	if (IsShuttingDown()) {
		return "The world is already shutting down.";
	}

	m_requested_by = requested_by;
	LogInfo("Immediate world shutdown requested by [{}]", requested_by);
	BeginDrain();
	return "World is shutting down now.";
}

std::string WorldShutdown::Cancel(const std::string &requested_by)
{
	if (IsShuttingDown()) {
		return "The shutdown is already in progress and can't be cancelled.";
	}

	if (m_phase != Phase::Countdown) {
		return "No world shutdown is scheduled.";
	}

	m_phase    = Phase::Idle;
	m_deadline = 0;
	StopTicking();

	LogInfo("Scheduled world shutdown cancelled by [{}]", requested_by);
	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, "[SYSTEM] The scheduled server shutdown has been cancelled.");
	PushMOTD();

	return "Scheduled world shutdown cancelled.";
}

std::string WorldShutdown::Status() const
{
	switch (m_phase) {
		case Phase::Countdown:
			return fmt::format(
				"World shutdown scheduled by {} in {} ({}).",
				m_requested_by,
				ShutdownDuration::Format(Remaining()),
				FormatUTC(m_deadline)
			);
		case Phase::Draining:
			return fmt::format("World is shutting down; waiting on {} zone(s) to save and exit.", m_draining_zones.size());
		case Phase::Finalizing:
			return "World is shutting down; stopping processes.";
		default:
			return "No world shutdown is scheduled.";
	}
}

void WorldShutdown::BeginDrain()
{
	m_phase           = Phase::Draining;
	m_drain_started   = std::time(nullptr);
	m_zones_signalled = false;
	StartTicking();

	zoneserver_list.SendEmoteMessageRaw(
		0, 0, AccountStatus::Player, Chat::Red,
		"[SYSTEM] The server is shutting down now. Your character is being saved."
	);

	// Keep new players out while zones go down.
	WorldConfig::LockWorld();

	m_draining_zones.clear();
	for (const auto &zs : zoneserver_list.getZoneServerList()) {
		m_draining_zones.insert(zs->GetUUID());
	}

	// Launchers must stop restarting zones before those zones start exiting.
	launcher_list.ShutdownAll(!WorldConfig::get()->ShutdownUsePM2);

	LogInfo("World shutdown started; [{}] zone(s) to drain", m_draining_zones.size());
}

void WorldShutdown::Finalize()
{
	m_phase             = Phase::Finalizing;
	m_finalize_deadline = std::time(nullptr) + PM2StopTimeout;

	const auto config = WorldConfig::get();
	if (config->ShutdownUsePM2 && !config->ShutdownPM2Command.empty()) {
		// Detach so the command isn't a child of world. pm2 kills the whole process tree of an app it
		// stops; if this were our child it could be killed before it got through every process.
#ifdef _WINDOWS
		const auto command = fmt::format("start \"\" /B cmd /C \"{}\"", config->ShutdownPM2Command);
#else
		const auto command = fmt::format(
			"( {} ) </dev/null >>'{}/world_shutdown_pm2.log' 2>&1 &",
			config->ShutdownPM2Command,
			path.GetLogPath()
		);
#endif
		LogInfo("Zones drained; running [{}] to stop all managed processes", config->ShutdownPM2Command);

		if (std::system(command.c_str()) == 0) {
			// pm2 will SIGINT us shortly. If it never does, Tick() exits after PM2StopTimeout.
			return;
		}

		LogError("Failed to run [{}]; exiting world directly. pm2 may restart processes that exited.", config->ShutdownPM2Command);
	}

	LogInfo("Zones drained; world exiting");
	CatchSignal(2);
}

void WorldShutdown::StartTicking()
{
	if (!m_tick_timer) {
		m_tick_timer = std::make_unique<EQ::Timer>(TickIntervalMS, true, [this](EQ::Timer *) { Tick(); });
	}
}

void WorldShutdown::StopTicking()
{
	m_tick_timer.reset();
}

void WorldShutdown::Tick()
{
	const time_t now = std::time(nullptr);

	switch (m_phase) {
		case Phase::Countdown: {
			const uint32 remaining = Remaining();
			if (remaining == 0) {
				LogInfo("Shutdown timer expired");
				BeginDrain();
				break;
			}

			// A long hitch can skip several marks; announce only the latest one.
			uint32 announce = 0;
			while (m_next_checkpoint != 0 && remaining <= m_next_checkpoint) {
				announce          = m_next_checkpoint;
				m_next_checkpoint = NextCheckpoint(m_next_checkpoint);
			}

			if (announce) {
				Announce(announce);
			}
			break;
		}
		case Phase::Draining: {
			if (!m_zones_signalled) {
				if (now - m_drain_started < ZoneSignalDelay) {
					break;
				}

				// Zones save every client and exit on a zero-size ServerOP_ShutdownAll.
				ServerPacket pack(ServerOP_ShutdownAll);
				zoneserver_list.SendPacket(&pack);
				m_zones_signalled = true;
				break;
			}

			std::set<std::string> connected;
			for (const auto &zs : zoneserver_list.getZoneServerList()) {
				connected.insert(zs->GetUUID());
			}

			for (auto it = m_draining_zones.begin(); it != m_draining_zones.end();) {
				it = connected.count(*it) ? std::next(it) : m_draining_zones.erase(it);
			}

			const uint32 drain_limit = WorldConfig::get()->ShutdownDrainSeconds;
			if (m_draining_zones.empty()) {
				Finalize();
			}
			else if (now - m_drain_started >= static_cast<time_t>(drain_limit + ZoneSignalDelay)) {
				LogWarning("[{}] zone(s) did not exit within [{}] seconds; continuing shutdown", m_draining_zones.size(), drain_limit);
				Finalize();
			}
			break;
		}
		case Phase::Finalizing:
			if (now >= m_finalize_deadline) {
				LogWarning("Process manager did not stop world within [{}] seconds; exiting", PM2StopTimeout);
				CatchSignal(2);
				m_finalize_deadline = now + PM2StopTimeout;
			}
			break;
		default:
			// Idle is only entered through Cancel(), which stops this timer. Never stop it from here:
			// that would destroy the timer inside its own callback.
			break;
	}
}

void WorldShutdown::Start()
{
	std::error_code ec;
	if (fs::remove(ServerFile(RequestFile), ec)) {
		LogInfo("Discarded a world shutdown request left over from before this boot");
	}
	fs::remove(ServerFile(ResponseFile), ec);

	if (!m_poll_timer) {
		m_poll_timer = std::make_unique<EQ::Timer>(PollIntervalMS, true, [this](EQ::Timer *) { PollRequestFile(); });
	}
}

void WorldShutdown::PollRequestFile()
{
	std::error_code ec;
	const auto request = ServerFile(RequestFile);
	if (!fs::exists(request, ec)) {
		return;
	}

	std::string line;
	{
		std::ifstream in(request);
		std::getline(in, line);
	}
	fs::remove(request, ec);

	const auto response = HandleRequest(Strings::Trim(line));

	// Write then rename so the CLI never reads a partial response.
	const auto tmp = ServerFile(ResponseFile).string() + ".tmp";
	{
		std::ofstream out(tmp, std::ios::trunc);
		out << response << "\n";
	}
	fs::rename(tmp, ServerFile(ResponseFile), ec);
	if (ec) {
		LogError("Could not write world shutdown response: [{}]", ec.message());
	}
}

// Request lines, written by `world world:shutdown`:
//   schedule <seconds> <interval_seconds> <requested_by>
//   now <requested_by>
//   cancel <requested_by>
//   status
std::string WorldShutdown::HandleRequest(const std::string &line)
{
	auto args = Strings::Split(line, ' ');
	if (args.empty() || args[0].empty()) {
		return "ERROR: empty shutdown request";
	}

	const auto verb     = Strings::ToLower(args[0]);
	auto       by_index = verb == "schedule" ? 3 : 1;
	std::string requested_by;
	for (size_t i = by_index; i < args.size(); i++) {
		requested_by += (requested_by.empty() ? "" : " ") + args[i];
	}
	if (requested_by.empty()) {
		requested_by = "command line";
	}

	if (verb == "schedule" && args.size() >= 3 && Strings::IsNumber(args[1]) && Strings::IsNumber(args[2])) {
		return Schedule(Strings::ToUnsignedInt(args[1]), Strings::ToUnsignedInt(args[2]), requested_by);
	}
	if (verb == "now") {
		return ShutdownNow(requested_by);
	}
	if (verb == "cancel") {
		return Cancel(requested_by);
	}
	if (verb == "status") {
		return Status();
	}

	return fmt::format("ERROR: unknown shutdown request [{}]", line);
}
