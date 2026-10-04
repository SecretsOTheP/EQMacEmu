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
#include "rolling_restart.h"
#include "launcher_list.h"
#include "main.h"
#include "world_config.h"
#include "worlddb.h"
#include "zonelist.h"
#include "zoneserver.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

extern ZSList       zoneserver_list;
extern LauncherList launcher_list;

namespace fs = std::filesystem;

namespace {
	// Give zones a moment to deliver the "shutting down" broadcast before they are told to exit.
	constexpr time_t ZoneSignalDelay = 2;
	// How long to wait for pm2 to stop (or restart) us before acting on our own.
	constexpr time_t PM2StopTimeout  = 30;

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

WorldShutdown::RestartMethod WorldShutdown::GetRestartMethod() const
{
	const auto config = WorldConfig::get();
	if (config->ShutdownUsePM2 && !config->RestartPM2Command.empty()) {
		return RestartMethod::PM2;
	}
	if (!config->RestartCommand.empty()) {
		return RestartMethod::Command;
	}
	return RestartMethod::InPlace;
}

void WorldShutdown::Announce(uint32 remaining)
{
	std::string advice;
	if (remaining <= 300) {
		advice = m_restart ?
			" Please find a safe place and log out. The server will be back shortly." :
			" Please find a safe place and log out.";
	}

	const auto message = fmt::format(
		"[SYSTEM] The server will {} in {}.{}",
		Verb(),
		ShutdownDuration::FormatCountdown(remaining),
		advice
	);

	LogInfo("{}", message);
	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, remaining <= 60 ? Chat::Red : Chat::Yellow, message.c_str());
	m_announced = true;
}

std::string WorldShutdown::GetMOTD() const
{
	std::string motd = RuleS(World, MOTD);
	if (motd.empty()) {
		database.GetVariable("MOTD", motd);
	}

	const char *label = m_restart ? "SERVER RESTART" : "SERVER SHUTDOWN";
	std::string notice;
	if (m_phase == Phase::Countdown) {
		// Only the absolute time: zones keep this MOTD until the next push, so a countdown would go stale.
		notice = fmt::format("{}: The server will {} at {}.", label, Verb(), ShutdownDuration::Eastern::Format(m_deadline));
	}
	else if (IsShuttingDown()) {
		notice = fmt::format("{}: The server is {} now.", label, m_restart ? "restarting" : "shutting down");
	}
	else {
		notice = RollingRestart::Instance().MOTDNotice();
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

std::string WorldShutdown::Schedule(uint32 seconds, const std::string &requested_by, bool restart)
{
	if (IsShuttingDown()) {
		return fmt::format("The world is already {}.", m_restart ? "restarting" : "shutting down");
	}
	if (m_phase == Phase::Restarting) {
		return "The world is coming back up from a restart; try again once it's up.";
	}

	if (seconds == 0) {
		return ShutdownNow(requested_by, restart);
	}

	const bool rescheduled = m_phase == Phase::Countdown;
	if (!rescheduled) {
		m_announced = false;
	}

	m_phase           = Phase::Countdown;
	m_restart         = restart;
	m_deadline        = std::time(nullptr) + seconds;
	m_requested_by    = requested_by;
	m_next_checkpoint = ShutdownDuration::NextCountdownMark(seconds);
	StartTicking();

	LogInfo(
		"World {} {} by [{}] for [{}] from now ([{}])",
		Noun(),
		rescheduled ? "rescheduled" : "scheduled",
		requested_by,
		ShutdownDuration::Format(seconds),
		ShutdownDuration::Eastern::Format(m_deadline)
	);

	// More than an hour out, the MOTD is the only notice. Players who already heard a countdown
	// are told it moved.
	if (seconds <= ShutdownDuration::AnnounceStart) {
		Announce(seconds);
	}
	else if (m_announced) {
		const auto message = fmt::format("[SYSTEM] The server {} has been moved to {}.", Noun(), ShutdownDuration::Eastern::Format(m_deadline));
		zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, message.c_str());
	}
	PushMOTD();

	return fmt::format(
		"World {} {} for {} ({} from now). Warnings: MOTD only until 60 minutes remain, then every 5 minutes, then every minute from 15 minutes.",
		Noun(),
		rescheduled ? "rescheduled" : "scheduled",
		ShutdownDuration::Eastern::Format(m_deadline),
		ShutdownDuration::Format(seconds)
	);
}

std::string WorldShutdown::ShutdownNow(const std::string &requested_by, bool restart)
{
	if (IsShuttingDown()) {
		return fmt::format("The world is already {}.", m_restart ? "restarting" : "shutting down");
	}
	if (m_phase == Phase::Restarting) {
		return "The world is coming back up from a restart; try again once it's up.";
	}

	m_restart      = restart;
	m_requested_by = requested_by;
	LogInfo("Immediate world {} requested by [{}]", Noun(), requested_by);
	BeginDrain();
	return fmt::format("World is {} now.", m_restart ? "restarting" : "shutting down");
}

std::string WorldShutdown::Cancel(const std::string &requested_by)
{
	if (IsShuttingDown() || m_phase == Phase::Restarting) {
		return fmt::format("The {} is already in progress and can't be cancelled.", Noun());
	}

	if (m_phase != Phase::Countdown) {
		return "No world shutdown or restart is scheduled.";
	}

	m_phase    = Phase::Idle;
	m_deadline = 0;
	StopTicking();

	LogInfo("Scheduled world {} cancelled by [{}]", Noun(), requested_by);
	// Players only heard about it if the countdown got within an hour; otherwise the MOTD was the notice.
	if (m_announced) {
		const auto message = fmt::format("[SYSTEM] The scheduled server {} has been cancelled.", Noun());
		zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, message.c_str());
	}
	PushMOTD();

	return fmt::format("Scheduled world {} cancelled.", Noun());
}

std::string WorldShutdown::Status() const
{
	switch (m_phase) {
		case Phase::Countdown:
			return fmt::format(
				"World {} scheduled by {} for {} ({} from now).",
				Noun(),
				m_requested_by,
				ShutdownDuration::Eastern::Format(m_deadline),
				ShutdownDuration::Format(Remaining())
			);
		case Phase::Draining:
			return fmt::format(
				"World is {}; waiting on {} zone(s) to save and exit.",
				m_restart ? "restarting" : "shutting down",
				m_draining_zones.size()
			);
		case Phase::Finalizing:
			return m_restart ? "World is restarting; handing off to the process manager." : "World is shutting down; stopping processes.";
		case Phase::Restarting:
			return fmt::format(
				"World is coming back up: {} of {} zone server(s) connected; logins open once they're back.",
				zoneserver_list.getZoneServerList().size(),
				m_zones_before_drain
			);
		default:
			return "No world shutdown or restart is scheduled.";
	}
}

void WorldShutdown::BeginDrain()
{
	// A world restart or shutdown restarts every zone anyway.
	RollingRestart::Instance().Abort();

	m_phase           = Phase::Draining;
	m_drain_started   = std::time(nullptr);
	m_zones_signalled = false;
	StartTicking();

	const auto message = fmt::format(
		"[SYSTEM] The server is {} now. Your character is being saved.",
		m_restart ? "restarting" : "shutting down"
	);
	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Red, message.c_str());

	// Keep new players out while zones go down. An in-place restart reopens logins only if they
	// were open before.
	m_was_locked = WorldConfig::get()->Locked;
	WorldConfig::LockWorld();

	m_draining_zones.clear();
	for (const auto &zs : zoneserver_list.getZoneServerList()) {
		m_draining_zones.insert(zs->GetUUID());
	}
	m_zones_before_drain = m_draining_zones.size();

	// Launchers must stop restarting zones before those zones start exiting. They exit with world
	// only when nothing else will manage them: a shutdown without pm2, or a restart handed to a
	// restart command (which starts them again). Otherwise they idle until pm2 stops them or world
	// resumes them.
	bool launchers_exit = !WorldConfig::get()->ShutdownUsePM2;
	if (m_restart) {
		launchers_exit = GetRestartMethod() == RestartMethod::Command;
	}
	m_launchers_exit = launchers_exit;
	launcher_list.ShutdownAll(launchers_exit);

	LogInfo("World {} started; [{}] zone(s) to drain", Noun(), m_draining_zones.size());
}

// Runs a shell command detached from world, so a process manager that kills world's process tree
// can't kill the command partway through.
bool WorldShutdown::RunDetached(const std::string &command, const char *log_name) const
{
#ifdef _WINDOWS
	const auto shell = fmt::format("start \"\" /B cmd /C \"{}\"", command);
#else
	const auto shell = fmt::format(
		"( {} ) </dev/null >>'{}/{}' 2>&1 &",
		command,
		path.GetLogPath(),
		log_name
	);
#endif
	return std::system(shell.c_str()) == 0;
}

void WorldShutdown::Finalize()
{
	if (m_restart) {
		FinalizeRestart();
		return;
	}

	m_phase             = Phase::Finalizing;
	m_finalize_deadline = std::time(nullptr) + PM2StopTimeout;

	const auto config = WorldConfig::get();
	if (config->ShutdownUsePM2 && !config->ShutdownPM2Command.empty()) {
		LogInfo("Zones drained; running [{}] to stop all managed processes", config->ShutdownPM2Command);

		if (RunDetached(config->ShutdownPM2Command, "world_shutdown_pm2.log")) {
			// pm2 will SIGINT us shortly. If it never does, Tick() exits after PM2StopTimeout.
			return;
		}

		LogError("Failed to run [{}]; exiting world directly. pm2 may restart processes that exited.", config->ShutdownPM2Command);
	}

	LogInfo("Zones drained; world exiting");
	CatchSignal(2);
}

void WorldShutdown::FinalizeRestart()
{
	const auto config = WorldConfig::get();

	switch (GetRestartMethod()) {
		case RestartMethod::PM2:
			LogInfo("Zones drained; running [{}] to restart all managed processes", config->RestartPM2Command);
			if (RunDetached(config->RestartPM2Command, "world_restart_pm2.log")) {
				// pm2 restarts world shortly. If it never does, Tick() resumes in place after PM2StopTimeout.
				m_phase             = Phase::Finalizing;
				m_finalize_deadline = std::time(nullptr) + PM2StopTimeout;
				return;
			}
			LogError("Failed to run [{}]; restarting zones in place instead", config->RestartPM2Command);
			break;
		case RestartMethod::Command:
			LogInfo("Zones drained; running [{}] and exiting so it can start the server again", config->RestartCommand);
			if (RunDetached(config->RestartCommand, "world_restart.log")) {
				m_phase = Phase::Finalizing;
				CatchSignal(2);
				return;
			}
			// The launchers were told to exit with world, so there may be nothing left to restart zones.
			LogError("Failed to run [{}]; restarting zones in place instead. Check that eqlaunch is still running.", config->RestartCommand);
			break;
		default:
			break;
	}

	ResumeInPlace();
}

// World restarts every zone itself: the launchers start restarting zones again and logins reopen
// once the zones have reconnected. World's own process isn't restarted, so a new world binary
// needs pm2 or restart_command.
void WorldShutdown::ResumeInPlace()
{
	m_phase          = Phase::Restarting;
	m_resume_started = std::time(nullptr);

	if (launcher_list.GetLauncherCount() == 0 && !WorldConfig::get()->ShutdownUsePM2) {
		LogWarning("World restart: no launcher is connected, so zones may not come back on their own");
	}

	LogInfo("Zones drained; resuming launchers and waiting for [{}] zone(s) to reconnect", m_zones_before_drain);
	launcher_list.ResumeAll();
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
				LogInfo("{} timer expired", m_restart ? "Restart" : "Shutdown");
				BeginDrain();
				break;
			}

			// A long hitch can skip several marks; announce only the latest one.
			uint32 announce = 0;
			while (m_next_checkpoint != 0 && remaining <= m_next_checkpoint) {
				announce          = m_next_checkpoint;
				m_next_checkpoint = ShutdownDuration::NextCountdownMark(m_next_checkpoint);
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
				LogWarning("[{}] zone(s) did not exit within [{}] seconds; continuing {}", m_draining_zones.size(), drain_limit, Noun());
				Finalize();
			}
			break;
		}
		case Phase::Finalizing:
			if (now >= m_finalize_deadline) {
				if (m_restart) {
					LogWarning("Process manager did not restart world within [{}] seconds; restarting zones in place", PM2StopTimeout);
					ResumeInPlace();
					break;
				}
				LogWarning("Process manager did not stop world within [{}] seconds; exiting", PM2StopTimeout);
				CatchSignal(2);
				m_finalize_deadline = now + PM2StopTimeout;
			}
			break;
		case Phase::Restarting: {
			const size_t connected = zoneserver_list.getZoneServerList().size();
			const bool   timed_out = now - m_resume_started >= static_cast<time_t>(WorldConfig::get()->RestartZoneWaitSeconds);
			if (connected < m_zones_before_drain && !timed_out) {
				break;
			}

			if (connected < m_zones_before_drain) {
				LogWarning(
					"Only [{}] of [{}] zone(s) reconnected within [{}] seconds of the restart; opening logins anyway",
					connected,
					m_zones_before_drain,
					WorldConfig::get()->RestartZoneWaitSeconds
				);
			}

			if (!m_was_locked) {
				WorldConfig::UnlockWorld();
			}
			LogInfo(
				"World restart complete: [{}] zone(s) connected after [{}]{}",
				connected,
				ShutdownDuration::Format(static_cast<uint32>(now - m_drain_started)),
				m_was_locked ? "; world stays locked as it was before the restart" : ""
			);

			// Leave the tick timer running idle: this runs inside its callback.
			m_phase   = Phase::Idle;
			m_restart = false;
			PushMOTD();
			break;
		}
		default:
			// Idle: Cancel() stops this timer. Never stop it from here: that would destroy the timer
			// inside its own callback, so a finished in-place restart leaves it ticking idle.
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

// Request lines, written by `world world:shutdown`, `world:restart` and `world:rolling-restart`:
//   schedule|restart|rolling <deadline (Unix time)> <requested_by>
//   now|restartnow|rollingnow <requested_by>
//   cancel|rollingcancel <requested_by>
//   status|rollingstatus
std::string WorldShutdown::HandleRequest(const std::string &line)
{
	auto args = Strings::Split(line, ' ');
	if (args.empty() || args[0].empty()) {
		return "ERROR: empty shutdown request";
	}

	const auto verb      = Strings::ToLower(args[0]);
	const bool scheduled = verb == "schedule" || verb == "restart" || verb == "rolling";
	auto       by_index  = scheduled ? 2 : 1;
	std::string requested_by;
	for (size_t i = by_index; i < args.size(); i++) {
		requested_by += (requested_by.empty() ? "" : " ") + args[i];
	}
	if (requested_by.empty()) {
		requested_by = "command line";
	}

	if (scheduled) {
		if (args.size() < 2 || args[1].empty() || args[1].size() > 12 || !Strings::IsNumber(args[1])) {
			return fmt::format("ERROR: malformed request [{}]", line);
		}
		// The CLI sends the deadline rather than a delay, so the few seconds before world picks the
		// request up don't push a "7am" schedule past 7am.
		const int64 now      = std::time(nullptr);
		const int64 deadline = std::stoll(args[1]);
		if (deadline - now > ShutdownDuration::MaxSeconds) {
			return "ERROR: that's more than 30 days away";
		}
		const auto seconds = static_cast<uint32>(std::max<int64>(deadline - now, 1));
		if (verb == "rolling") {
			return RollingRestart::Instance().Schedule(seconds, requested_by);
		}
		return Schedule(seconds, requested_by, verb == "restart");
	}
	if (verb == "now") {
		return ShutdownNow(requested_by);
	}
	if (verb == "restartnow") {
		return ShutdownNow(requested_by, true);
	}
	if (verb == "cancel") {
		return Cancel(requested_by);
	}
	if (verb == "status") {
		return Status();
	}
	if (verb == "rollingnow") {
		return RollingRestart::Instance().RestartNow(requested_by);
	}
	if (verb == "rollingcancel") {
		return RollingRestart::Instance().Cancel(requested_by);
	}
	if (verb == "rollingstatus") {
		return RollingRestart::Instance().Status();
	}

	return fmt::format("ERROR: unknown shutdown request [{}]", line);
}
