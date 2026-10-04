#ifndef WORLD_ROLLING_RESTART_H
#define WORLD_ROLLING_RESTART_H

#include "../common/types.h"
#include "../common/event/timer.h"
#include <ctime>
#include <deque>
#include <memory>
#include <set>
#include <string>

// Restarts every zone process without taking the server down:
//   countdown (same announcements as a world shutdown) -> restart zones a batch at a time.
// Each zone in a batch saves its players and exits; eqlaunch or pm2 starts a fresh process, which
// picks up a new zone binary, quests and content. World and logins stay up the whole time, and
// players in a restarting zone are disconnected briefly and can log straight back in.
//
// Requests come from #rollingrestart (via zone), the telnet console, or the
// `world world:rolling-restart` CLI through the world shutdown request file.
class RollingRestart {
public:
	enum class Phase { Idle, Countdown, Restarting };

	static RollingRestart &Instance();

	std::string Schedule(uint32 seconds, uint32 interval_seconds, const std::string &requested_by);
	std::string RestartNow(const std::string &requested_by);
	std::string Cancel(const std::string &requested_by);
	std::string Status() const;

	bool IsActive() const { return m_phase != Phase::Idle; }

	// A world shutdown or restart is taking over; stop quietly, it restarts every zone anyway.
	void Abort();

	// Prefix for the MOTD while a countdown or restart is running; empty otherwise.
	std::string MOTDNotice() const;

private:
	RollingRestart() = default;

	std::string CanStart() const;
	void   Tick();
	void   StartTicking();
	void   StopTicking();
	uint32 Remaining() const;
	void   Announce(uint32 remaining);
	void   Begin();
	void   StartNextBatch();
	void   Finish(const std::string &message);

	Phase       m_phase           = Phase::Idle;
	time_t      m_deadline        = 0;
	uint32      m_interval        = 0;
	uint32      m_next_checkpoint = 0;
	std::string m_requested_by;

	std::deque<std::string> m_queue;          // zone server UUIDs still to restart, fewest players first
	std::set<std::string>   m_batch;          // zone server UUIDs restarting now
	bool                    m_batch_signalled = false;
	time_t                  m_batch_started   = 0;
	size_t                  m_baseline        = 0; // zone servers connected when the restart began
	uint32                  m_total           = 0;
	uint32                  m_restarted       = 0;
	time_t                  m_started         = 0;
	bool                    m_stop_requested  = false;

	std::unique_ptr<EQ::Timer> m_tick_timer;
};

#endif
