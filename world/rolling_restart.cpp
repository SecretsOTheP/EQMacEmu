#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/eq_constants.h"
#include "../common/emu_constants.h"
#include "../common/servertalk.h"
#include "../common/shutdown_duration.h"
#include "rolling_restart.h"
#include "world_shutdown.h"
#include "launcher_list.h"
#include "world_config.h"
#include "zonelist.h"
#include "zoneserver.h"

#include <algorithm>
#include <vector>

extern ZSList       zoneserver_list;
extern LauncherList launcher_list;

namespace {
	// Give a zone a moment to deliver its "restarting now" message before it is told to exit.
	constexpr time_t ZoneSignalDelay = 2;

	ZoneServer *FindZoneServer(const std::string &uuid)
	{
		for (const auto &zs : zoneserver_list.getZoneServerList()) {
			if (zs->GetUUID() == uuid) {
				return zs.get();
			}
		}
		return nullptr;
	}
}

RollingRestart &RollingRestart::Instance()
{
	static RollingRestart instance;
	return instance;
}

uint32 RollingRestart::Remaining() const
{
	const time_t now = std::time(nullptr);
	return m_deadline > now ? static_cast<uint32>(m_deadline - now) : 0;
}

// Empty when a rolling restart can start; otherwise the reason it can't.
std::string RollingRestart::CanStart() const
{
	if (m_phase == Phase::Restarting) {
		return "A rolling zone restart is already in progress.";
	}
	if (WorldShutdown::Instance().IsActive()) {
		return "A world shutdown or restart is scheduled or in progress; it restarts every zone anyway.";
	}
	// Something has to start each zone back up after it exits.
	if (launcher_list.GetLauncherCount() == 0 && !WorldConfig::get()->ShutdownUsePM2) {
		return "No launcher is connected and pm2 isn't configured, so restarted zones would stay down.";
	}
	return "";
}

void RollingRestart::Announce(uint32 remaining)
{
	const auto message = fmt::format(
		"[SYSTEM] Zones will restart in {}. You'll be disconnected briefly when your zone restarts; log back in right away.",
		ShutdownDuration::FormatCountdown(remaining)
	);

	LogInfo("{}", message);
	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, remaining <= 60 ? Chat::Red : Chat::Yellow, message.c_str());
	m_announced = true;
}

std::string RollingRestart::MOTDNotice() const
{
	if (m_phase == Phase::Countdown) {
		// Only the absolute time: zones keep this MOTD until the next push, so a countdown would go stale.
		return fmt::format(
			"ZONE RESTARTS: Zones will restart at {}. Expect a brief disconnect.",
			ShutdownDuration::Eastern::Format(m_deadline)
		);
	}
	if (m_phase == Phase::Restarting) {
		return "ZONE RESTARTS: Zones are restarting now. If you're disconnected, log back in right away.";
	}
	return "";
}

std::string RollingRestart::Schedule(uint32 seconds, const std::string &requested_by)
{
	const auto blocked = CanStart();
	if (!blocked.empty()) {
		return blocked;
	}

	if (seconds == 0) {
		return RestartNow(requested_by);
	}

	const bool rescheduled = m_phase == Phase::Countdown;
	if (!rescheduled) {
		m_announced = false;
	}

	m_phase           = Phase::Countdown;
	m_deadline        = std::time(nullptr) + seconds;
	m_requested_by    = requested_by;
	m_next_checkpoint = ShutdownDuration::NextCountdownMark(seconds);
	StartTicking();

	LogInfo(
		"Rolling zone restart {} by [{}] for [{}] from now ([{}])",
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
		const auto message = fmt::format("[SYSTEM] The zone restarts have been moved to {}.", ShutdownDuration::Eastern::Format(m_deadline));
		zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, message.c_str());
	}
	WorldShutdown::Instance().PushMOTD();

	return fmt::format(
		"Rolling zone restart {} for {} ({} from now). Zones restart {} at a time. Warnings: MOTD only until 60 minutes remain, then every 5 minutes, then every minute from 15 minutes.",
		rescheduled ? "rescheduled" : "scheduled",
		ShutdownDuration::Eastern::Format(m_deadline),
		ShutdownDuration::Format(seconds),
		std::max<uint32>(WorldConfig::get()->RollingRestartBatchSize, 1)
	);
}

std::string RollingRestart::RestartNow(const std::string &requested_by)
{
	const auto blocked = CanStart();
	if (!blocked.empty()) {
		return blocked;
	}

	m_requested_by = requested_by;
	LogInfo("Immediate rolling zone restart requested by [{}]", requested_by);
	Begin();
	return fmt::format("Rolling zone restart started: {} zone(s) to restart.", m_total);
}

std::string RollingRestart::Cancel(const std::string &requested_by)
{
	if (m_phase == Phase::Restarting) {
		if (m_stop_requested) {
			return "The rolling zone restart is already stopping after the current batch.";
		}
		m_stop_requested = true;
		m_queue.clear();
		LogInfo("Rolling zone restart stopped by [{}]; finishing the current batch", requested_by);
		return fmt::format("Stopping after the current batch. {} of {} zone(s) restarted so far.", m_restarted, m_total);
	}

	if (m_phase != Phase::Countdown) {
		return "No rolling zone restart is scheduled.";
	}

	m_phase    = Phase::Idle;
	m_deadline = 0;
	StopTicking();

	LogInfo("Scheduled rolling zone restart cancelled by [{}]", requested_by);
	// Players only heard about it if the countdown got within an hour; otherwise the MOTD was the notice.
	if (m_announced) {
		zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, "[SYSTEM] The scheduled zone restarts have been cancelled.");
	}
	WorldShutdown::Instance().PushMOTD();

	return "Scheduled rolling zone restart cancelled.";
}

std::string RollingRestart::Status() const
{
	switch (m_phase) {
		case Phase::Countdown:
			return fmt::format(
				"Rolling zone restart scheduled by {} for {} ({} from now).",
				m_requested_by,
				ShutdownDuration::Eastern::Format(m_deadline),
				ShutdownDuration::Format(Remaining())
			);
		case Phase::Restarting:
			return fmt::format(
				"Rolling zone restart in progress: {} of {} zone(s) restarted, {} restarting now{}.",
				m_restarted,
				m_total,
				m_batch.size(),
				m_stop_requested ? "; stopping after this batch" : ""
			);
		default:
			return "No rolling zone restart is scheduled.";
	}
}

void RollingRestart::Abort()
{
	if (m_phase == Phase::Idle) {
		return;
	}

	LogInfo("Rolling zone restart abandoned: a world shutdown or restart is taking over");
	m_phase = Phase::Idle;
	m_queue.clear();
	m_batch.clear();
	StopTicking();
}

void RollingRestart::Begin()
{
	// Empty zones first, then the quietest, so the fewest players are disconnected while
	// players are still logging back in to zones that haven't restarted yet.
	std::vector<std::pair<uint32, std::string>> zones;
	for (const auto &zs : zoneserver_list.getZoneServerList()) {
		zones.emplace_back(zs->NumPlayers(), zs->GetUUID());
	}
	std::stable_sort(zones.begin(), zones.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

	m_queue.clear();
	for (const auto &z : zones) {
		m_queue.push_back(z.second);
	}

	m_phase          = Phase::Restarting;
	m_batch.clear();
	m_baseline       = zones.size();
	m_total          = static_cast<uint32>(zones.size());
	m_restarted      = 0;
	m_started        = std::time(nullptr);
	m_stop_requested = false;
	StartTicking();

	LogInfo("Rolling zone restart started: [{}] zone(s), [{}] per batch", m_total, std::max<uint32>(WorldConfig::get()->RollingRestartBatchSize, 1));
	zoneserver_list.SendEmoteMessageRaw(
		0, 0, AccountStatus::Player, Chat::Yellow,
		"[SYSTEM] Zones are restarting now, a few at a time. If you're disconnected, log back in right away."
	);
	WorldShutdown::Instance().PushMOTD();

	StartNextBatch();
}

void RollingRestart::StartNextBatch()
{
	m_batch.clear();
	m_batch_signalled = false;
	m_batch_started   = std::time(nullptr);

	const uint32 batch_size = std::max<uint32>(WorldConfig::get()->RollingRestartBatchSize, 1);
	while (!m_queue.empty() && m_batch.size() < batch_size) {
		const auto uuid = m_queue.front();
		m_queue.pop_front();

		// Zones that went away on their own since the restart began are already fresh.
		auto zs = FindZoneServer(uuid);
		if (!zs) {
			continue;
		}

		if (zs->NumPlayers() > 0) {
			zs->SendEmoteMessageRaw(
				0, 0, AccountStatus::Player, Chat::Red,
				"[SYSTEM] This zone is restarting now. Your character is being saved; log back in right away."
			);
		}
		m_batch.insert(uuid);
	}
}

void RollingRestart::Finish(const std::string &message)
{
	LogInfo(
		"Rolling zone restart finished: [{}] of [{}] zone(s) restarted in [{}]",
		m_restarted,
		m_total,
		ShutdownDuration::Format(static_cast<uint32>(std::time(nullptr) - m_started))
	);

	// Leave the tick timer running idle: Finish() is called from inside its own callback.
	m_phase = Phase::Idle;
	m_queue.clear();
	m_batch.clear();

	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, message.c_str());
	WorldShutdown::Instance().PushMOTD();
}

void RollingRestart::StartTicking()
{
	if (!m_tick_timer) {
		m_tick_timer = std::make_unique<EQ::Timer>(1000, true, [this](EQ::Timer *) { Tick(); });
	}
}

void RollingRestart::StopTicking()
{
	m_tick_timer.reset();
}

void RollingRestart::Tick()
{
	const time_t now = std::time(nullptr);

	switch (m_phase) {
		case Phase::Countdown: {
			const uint32 remaining = Remaining();
			if (remaining == 0) {
				LogInfo("Rolling zone restart timer expired");
				// Re-check: the server may have lost its launchers since this was scheduled.
				const auto blocked = CanStart();
				if (!blocked.empty()) {
					LogError("Rolling zone restart not started: {}", blocked);
					m_phase = Phase::Idle;
					if (m_announced) {
						zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, Chat::Yellow, "[SYSTEM] The scheduled zone restarts have been cancelled.");
					}
					WorldShutdown::Instance().PushMOTD();
					break;
				}
				Begin();
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
		case Phase::Restarting: {
			if (!m_batch_signalled) {
				if (!m_batch.empty() && now - m_batch_started < ZoneSignalDelay) {
					break;
				}

				// Zones save every client and exit on a zero-size ServerOP_ShutdownAll.
				for (const auto &uuid : m_batch) {
					if (auto zs = FindZoneServer(uuid)) {
						ServerPacket pack(ServerOP_ShutdownAll);
						zs->SendPacket(&pack);
					}
				}
				m_batch_signalled = true;
				break;
			}

			std::set<std::string> connected;
			for (const auto &zs : zoneserver_list.getZoneServerList()) {
				connected.insert(zs->GetUUID());
			}

			for (auto it = m_batch.begin(); it != m_batch.end();) {
				if (connected.count(*it)) {
					++it;
				}
				else {
					m_restarted++;
					it = m_batch.erase(it);
				}
			}

			// The batch is done once its zones have exited and their replacements have connected.
			const bool batch_exited  = m_batch.empty();
			const bool replaced      = connected.size() >= m_baseline;
			const bool timed_out     = now - m_batch_started >= static_cast<time_t>(WorldConfig::get()->RollingRestartBatchSeconds + ZoneSignalDelay);
			if (!(batch_exited && replaced) && !timed_out) {
				break;
			}

			if (timed_out && !(batch_exited && replaced)) {
				LogWarning(
					"Rolling restart batch not done after [{}] seconds ([{}] zone(s) still up, [{}] of [{}] zone servers connected); moving on",
					WorldConfig::get()->RollingRestartBatchSeconds,
					m_batch.size(),
					connected.size(),
					m_baseline
				);
			}

			if (m_queue.empty()) {
				Finish(
					m_stop_requested ?
						fmt::format("[SYSTEM] Zone restarts were stopped after {} of {} zones.", m_restarted, m_total) :
						std::string("[SYSTEM] Zone restarts are complete. Thanks for your patience.")
				);
				break;
			}

			StartNextBatch();
			break;
		}
		default:
			// Idle: Cancel()/Abort() stop this timer; Finish() leaves it idle because it runs inside it.
			break;
	}
}
