#ifndef WORLD_SHUTDOWN_H
#define WORLD_SHUTDOWN_H

#include "../common/types.h"
#include "../common/event/timer.h"
#include <ctime>
#include <memory>
#include <set>
#include <string>

// Drives a scheduled or immediate world shutdown or restart:
//   countdown (announcements + MOTD notice) -> drain (zones save and exit, launchers stop
//   restarting them) -> finalize (exit world, or hand off to pm2 when configured).
// A restart finalizes differently so the server comes back up (see RestartMethod):
//   pm2 restarts everything, or a configured restart command runs and world exits, or world stays
//   up, resumes its launchers and lets players back in once the zones have reconnected.
//
// Requests come from #worldshutdown/#worldrestart/#rollingrestart (via zone), the telnet console,
// or the `world world:shutdown|restart|rolling-restart` CLI, which drops a request file next to
// eqemu_config.json that the running world picks up. Rolling zone restarts are handled by
// RollingRestart; this class only routes their requests.
// Everything runs on its own event-loop timers, not the main world loop.
class WorldShutdown {
public:
	enum class Phase { Idle, Countdown, Draining, Finalizing, Restarting };
	enum class RestartMethod { PM2, Command, InPlace };

	static constexpr const char *RequestFile  = "world_shutdown.request";
	static constexpr const char *ResponseFile = "world_shutdown.response";

	// Request file check. Kept off the main loop and infrequent; world is busy and this is rare.
	static constexpr uint32 PollIntervalMS = 5000;
	// Countdown/drain step. Only runs while a shutdown is scheduled or in progress.
	static constexpr uint32 TickIntervalMS = 1000;

	static WorldShutdown &Instance();

	// restart = true brings the server back up after the drain instead of leaving it down.
	std::string Schedule(uint32 seconds, uint32 interval_seconds, const std::string &requested_by, bool restart = false);
	std::string ShutdownNow(const std::string &requested_by, bool restart = false);
	std::string Cancel(const std::string &requested_by);
	std::string Status() const;

	// Called once at boot: discards requests left over from before this boot and starts polling.
	void Start();

	bool IsShuttingDown() const { return m_phase == Phase::Draining || m_phase == Phase::Finalizing; }
	// Anything scheduled or running, including a restart that is waiting for zones to come back.
	bool IsActive() const { return m_phase != Phase::Idle; }
	// What a launcher connecting mid-drain should be told: exit with world, or idle.
	bool LaunchersShouldExit() const { return m_launchers_exit; }

	// The MOTD a logging-in player should see: the configured MOTD, prefixed with a shutdown,
	// restart or rolling zone restart notice while one is coming. Never written to the database,
	// so nothing has to be restored.
	std::string GetMOTD() const;
	// Sends GetMOTD() to every zone; call whenever a notice starts, changes or ends.
	void PushMOTD();

private:
	WorldShutdown() = default;

	void   Tick();
	void   StartTicking();
	void   StopTicking();
	uint32 Remaining() const;
	void   Announce(uint32 remaining);
	void   BeginDrain();
	void   Finalize();
	void   FinalizeRestart();
	void   ResumeInPlace();
	bool   RunDetached(const std::string &command, const char *log_name) const;
	RestartMethod GetRestartMethod() const;
	const char *Verb() const { return m_restart ? "restart" : "shut down"; }
	const char *Noun() const { return m_restart ? "restart" : "shutdown"; }
	void   PollRequestFile();
	std::string HandleRequest(const std::string &line);

	Phase       m_phase            = Phase::Idle;
	bool        m_restart          = false;
	time_t      m_deadline         = 0;
	uint32      m_interval         = 0;
	uint32      m_next_checkpoint  = 0;
	std::string m_requested_by;
	time_t      m_drain_started    = 0;
	bool        m_zones_signalled  = false;
	time_t      m_finalize_deadline = 0;
	std::set<std::string> m_draining_zones;
	// In-place restart: zones to wait for, and whether world was locked before the drain locked it.
	size_t      m_zones_before_drain = 0;
	bool        m_was_locked       = false;
	time_t      m_resume_started   = 0;
	bool        m_launchers_exit   = true;

	std::unique_ptr<EQ::Timer> m_poll_timer;
	std::unique_ptr<EQ::Timer> m_tick_timer;
};

#endif
