/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2008 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/eqemu_logsys.h"
#include "../common/global_define.h"
#include "clientlist.h"
#include "../common/opcodemgr.h"
#include "../common/eq_stream_factory.h"
#include "../common/rulesys.h"
#include "../common/servertalk.h"
#include "../common/platform.h"
#include "../common/crash.h"
#include "../common/strings.h"
#include "../common/event/timer.h"
#include "../common/path_manager.h"
#include "database.h"
#include "ucsconfig.h"
#include "chatchannel.h"
#include "worldserver.h"
#include <list>
#include <signal.h>
#include <mutex>
#include <unordered_set>

ChatChannelList *ChannelList;
Clientlist *g_Clientlist;
EQEmuLogSys LogSys;
TimeoutManager timeout_manager;
Database database;
WorldServer *worldserver = nullptr;
PathManager path;
std::unordered_set<uint32> ipWhitelist;
std::mutex		ipMutex;
bool bSkipFactoryAuth = true;

const ucsconfig *Config;

std::string WorldShortName;

uint32 ChatMessagesSent = 0;

volatile bool RunLoops = true;

void CatchSignal(int sig_num) {

	RunLoops = false;

	if(worldserver)
		worldserver->Disconnect();
}

int main() {
	RegisterExecutablePlatform(ExePlatformUCS);
	LogSys.LoadLogSettingsDefaults();
	set_exception_handler();

	path.LoadPaths();

	// Check every minute for unused channels we can delete
	//
	Timer ChannelListProcessTimer(60000);
	Timer ClientConnectionPruneTimer(60000);

	Timer InterserverTimer(INTERSERVER_TIMER); // does auto-reconnect

	LogInfo("Starting EQEmu Universal Chat Server.");

	if (!ucsconfig::LoadConfig()) { 
		LogInfo("Loading server configuration failed."); 
		return 1;
	}

	Config = ucsconfig::get(); 

	WorldShortName = Config->ShortName;

	LogInfo("Connecting to MySQL");

	if (!database.Connect(
		Config->DatabaseHost.c_str(),
		Config->DatabaseUsername.c_str(),
		Config->DatabasePassword.c_str(),
		Config->DatabaseDB.c_str(),
		Config->DatabasePort)) {
		LogInfo("Cannot continue without a database connection.");
		return 1;
	}

	LogSys.SetDatabase(&database)
		->SetLogPath(path.GetLogPath())
		->LoadLogDatabaseSettings()
		->StartFileLogs();

	char tmp[64];

	if (database.GetVariable("RuleSet", tmp, sizeof(tmp)-1)) {
		LogInfo("Loading rule set '[{0}]'", tmp);
		if(!RuleManager::Instance()->LoadRules(&database, tmp)) {
			LogInfo("Failed to load ruleset '[{0}]', falling back to defaults.", tmp);
		}
	} else {
		if(!RuleManager::Instance()->LoadRules(&database, "default")) {
			LogInfo("No rule set configured, using default rules");
		} else {
			LogInfo("Loaded default rule set 'default'", tmp);
		}
	}

	g_Clientlist = new Clientlist(Config->ChatPort);

	ChannelList = new ChatChannelList();

	database.LoadChatChannels();

	if (signal(SIGINT, CatchSignal) == SIG_ERR)	{
		LogInfo("Could not set signal handler");
		return 1;
	}
	if (signal(SIGTERM, CatchSignal) == SIG_ERR)	{
		LogInfo("Could not set signal handler");
		return 1;
	}

	worldserver = new WorldServer;

	worldserver->Connect();

	auto loop_fn = [&](EQ::Timer* t) {

		Timer::SetCurrentTime();

		ipMutex.lock();
		ipWhitelist.clear();
		ipMutex.unlock();

		if (!RunLoops) {
			EQ::EventLoop::Get().Shutdown();
			return;
		}

		g_Clientlist->Process();

		if (ChannelListProcessTimer.Check()) {
			ChannelList->Process();
		}

		if (ClientConnectionPruneTimer.Check()) {
			g_Clientlist->CheckForStaleConnectionsAll();
		}

		if (InterserverTimer.Check()) {
			if (worldserver->TryReconnect() && (!worldserver->Connected()))
				worldserver->AsyncConnect();
		}
		worldserver->Process();

		timeout_manager.CheckTimeouts();

	};

	EQ::Timer process_timer(loop_fn);
	process_timer.Start(32, true);

	EQ::EventLoop::Get().Run();

	ChannelList->RemoveAllChannels();

	g_Clientlist->CloseAllConnections();

	LogSys.CloseFileLogs();

}

void UpdateWindowTitle(char* iNewTitle)
{
#ifdef _WINDOWS
	std::string title;
	if (iNewTitle)
	{
		title = StringFormat("UCS: %s", iNewTitle);
	}
	else
	{
		title = "UCS";
	}
	SetConsoleTitle(title.c_str());
#endif
}
