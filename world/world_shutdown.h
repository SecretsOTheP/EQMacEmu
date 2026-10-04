#ifndef WORLD_SHUTDOWN_H
#define WORLD_SHUTDOWN_H

#include "../common/types.h"
#include "../common/event/timer.h"
#include <ctime>
#include <memory>
#include <set>
#include <string>

// Drives a scheduled or immediate world shutdown:
//   countdown (announcements + MOTD notice) -> drain (zones save and exit, launchers stop
//   restarting them) -> finalize (exit world, or hand off to pm2 when configured).
//
// Requests come from #worldshutdown (via zone), the telnet console, or the `world world:shutdown`
// CLI, which drops a request file next to eqemu_config.json that the running world picks up.
// Everything runs on its own event-loop timers, not the main world loop.
class WorldShutdown {
public:
	enum class Phase { Idle, Countdown, Draining, Finalizing };

	static constexpr const char *RequestFile  = "world_shutdown.request";
	static constexpr const char *ResponseFile = "world_shutdown.response";

	// Request file check. Kept off the main loop and infrequent; world is busy and this is rare.
	static constexpr uint32 PollIntervalMS = 5000;
	// Countdown/drain step. Only runs while a shutdown is scheduled or in progress.
	static constexpr uint32 TickIntervalMS = 1000;

	static WorldShutdown &Instance();

	std::string Schedule(uint32 seconds, uint32 interval_seconds, const std::string &requested_by);
	std::string ShutdownNow(const std::string &requested_by);
	std::string Cancel(const std::string &requested_by);
	std::string Status() const;

	// Called once at boot: discards requests left over from before this boot and starts polling.
	void Start();

	bool IsShuttingDown() const { return m_phase == Phase::Draining || m_phase == Phase::Finalizing; }

	// The MOTD a logging-in player should see: the configured MOTD, prefixed with a shutdown notice
	// while a countdown is running. Never written to the database, so nothing has to be restored.
	std::string GetMOTD() const;

private:
	WorldShutdown() = default;

	void   Tick();
	void   StartTicking();
	void   StopTicking();
	uint32 Remaining() const;
	uint32 NextCheckpoint(uint32 remaining) const;
	void   Announce(uint32 remaining);
	void   PushMOTD();
	void   BeginDrain();
	void   Finalize();
	void   PollRequestFile();
	std::string HandleRequest(const std::string &line);

	Phase       m_phase            = Phase::Idle;
	time_t      m_deadline         = 0;
	uint32      m_interval         = 0;
	uint32      m_next_checkpoint  = 0;
	std::string m_requested_by;
	time_t      m_drain_started    = 0;
	bool        m_zones_signalled  = false;
	time_t      m_finalize_deadline = 0;
	std::set<std::string> m_draining_zones;

	std::unique_ptr<EQ::Timer> m_poll_timer;
	std::unique_ptr<EQ::Timer> m_tick_timer;
};

#endif
