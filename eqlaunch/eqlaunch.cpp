/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2006 EQEMu Development Team (http://eqemulator.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/proc_launcher.h"
#include "../common/eqemu_config.h"
#include "../common/servertalk.h"
#include "../common/path_manager.h"
#include "../common/platform.h"
#include "../common/crash.h"
#include "../common/unix.h"
#include "worldserver.h"
#include "zone_launch.h"
#include <vector>
#include <map>
#include <set>
#include <signal.h>
#include <time.h>
#include <chrono>

EQEmuLogSys LogSys;
PathManager path;

volatile bool RunLoops = false;
static volatile sig_atomic_t pending_shutdown_signal = 0;

// After world asks for a shutdown, how long zones get to exit on their own.
static const uint32 ShutdownGraceMS = 30000;
// When the launcher itself is stopped, how long zones get before being killed.
static const uint32 ExitStopWaitMS = 15000;

void CatchSignal(int sig_num);

int main(int argc, char *argv[]) {
	RegisterExecutablePlatform(ExePlatformLaunch);
	LogSys.LoadLogSettingsDefaults();
	set_exception_handler();

	path.LoadPaths();

	std::string launcher_name;
	if(argc == 2) {
		launcher_name = argv[1];
	}
	if(launcher_name.length() < 1) {
		Log(Logs::Detail, Logs::Launcher, "You must specfify a launcher name as the first argument to this program.");
		return 1;
	}

	Log(Logs::Detail, Logs::Launcher, "Loading server configuration..");
	if (!EQEmuConfig::LoadConfig()) {
		Log(Logs::Detail, Logs::Launcher, "Loading server configuration failed.");
		return 1;
	}
	auto Config = EQEmuConfig::get();

	/*
	* Setup nice signal handlers
	*/
	if (signal(SIGINT, CatchSignal) == SIG_ERR)	{
		Log(Logs::Detail, Logs::Launcher, "Could not set signal handler");
		return 1;
	}
	if (signal(SIGTERM, CatchSignal) == SIG_ERR)	{
		Log(Logs::Detail, Logs::Launcher, "Could not set signal handler");
		return 1;
	}
	#ifndef WIN32
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)	{
		Log(Logs::Detail, Logs::Launcher, "Could not set signal handler");
		return 1;
	}

	/*
	* Add '.' to LD_LIBRARY_PATH
	*/
	//the storage passed to putenv must remain valid... crazy unix people
	const char *pv = getenv("LD_LIBRARY_PATH");
	if(pv == nullptr) {
		putenv(strdup("LD_LIBRARY_PATH=."));
	} else {
		char *v = (char *) malloc(strlen(pv) + 19);
		sprintf(v, "LD_LIBRARY_PATH=.:%s", pv);
		putenv(v);
	}
	#endif

	std::map<std::string, ZoneLaunch *> zones;
	WorldServer world(zones, launcher_name.c_str(), Config);
	
	std::map<std::string, ZoneLaunch *>::iterator zone, zend;
	std::set<std::string> to_remove;

	Timer InterserverTimer(INTERSERVER_TIMER); // does auto-reconnect

	Log(Logs::Detail, Logs::Launcher, "Starting main loop...");

	ProcLauncher *launch = ProcLauncher::get();

	// World tells zones to save and exit at the same time it tells us; give them that long to go
	// on their own before we start signalling the stragglers.
	Timer shutdown_grace(ShutdownGraceMS);
	bool shutdown_started = false;
	bool shutdown_stops_sent = false;

	RunLoops = true;
	auto loop_fn = [&](EQ::Timer* t) {
		//Advance the timer to our current point in time
		Timer::SetCurrentTime();

		if (!RunLoops) {
			EQ::EventLoop::Get().Shutdown();
			return;
		}
				
		/*
		* Let the process manager look for dead children
		*/
		launch->Process();

		/*
		* Give all zones a chance to process.
		*/
		zone = zones.begin();
		zend = zones.end();
		for (; zone != zend; ++zone) {
			if (!zone->second->Process()) {
				to_remove.insert(zone->first);
			}
		}

		/*
		* Kill off any zones which have stopped
		*/
		while (!to_remove.empty()) {
			std::string rem = *to_remove.begin();
			to_remove.erase(rem);
			zone = zones.find(rem);
			if (zone == zones.end()) {
				//wtf...
				continue;
			}
			delete zone->second;
			zones.erase(rem);
		}

		if (world.ShutdownRequested()) {
			if (!shutdown_started) {
				shutdown_started = true;
				shutdown_grace.Start(ShutdownGraceMS);
			}

			if (zones.empty()) {
				if (world.ExitAfterShutdown()) {
					LogInfo("All zones are down. Launcher exiting");
					RunLoops = false;
				}
			}
			else if (!shutdown_stops_sent && shutdown_grace.Check(false)) {
				LogInfo("[{}] zone(s) still running after shutdown grace period. Stopping them", zones.size());
				for (auto &z : zones) {
					z.second->Stop();
				}
				shutdown_stops_sent = true;
			}
		}
		else if (shutdown_started) {
			// World resumed us after an in-place restart; a later shutdown starts fresh.
			shutdown_started = false;
			shutdown_stops_sent = false;
		}
	};

	EQ::Timer process_timer(loop_fn);
	process_timer.Start(32, true);

	EQ::EventLoop::Get().Run();

	if (pending_shutdown_signal) {
		LogInfo("Received signal [{}]; stopping zones", (int) pending_shutdown_signal);
	}

	// Ask every zone to stop and give them time to save and exit. This used to force-kill them
	// about 2ms after asking, which could cut off a zone mid-save.
	ZoneLaunch::SetShuttingDown();
	for (auto &z : zones) {
		z.second->Stop();
	}

	const auto stop_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ExitStopWaitMS);
	while (!zones.empty() && std::chrono::steady_clock::now() < stop_deadline) {
		Timer::SetCurrentTime();
		launch->Process();	// reaps exited zones

		// Process() also escalates to a hard kill for zones that ignore the stop.
		for (auto it = zones.begin(); it != zones.end();) {
			if (!it->second->Process()) {
				delete it->second;
				it = zones.erase(it);
			} else {
				++it;
			}
		}

		if (!zones.empty()) {
			Sleep(100);
		}
	}

	if (!zones.empty()) {
		LogInfo("[{}] zone(s) did not stop in time. Killing them", zones.size());
	}

	//kill anybody left
	launch->TerminateAll(true);
	for (auto &z : zones) {
		delete z.second;
	}
	zones.clear();

	LogSys.CloseFileLogs();

	return 0;
}


// Only flag the main loop. Logging from a signal handler can deadlock on the allocator lock if the
// signal lands inside malloc/free.
void CatchSignal(int sig_num) {
	pending_shutdown_signal = sig_num;
	RunLoops = false;
}






















