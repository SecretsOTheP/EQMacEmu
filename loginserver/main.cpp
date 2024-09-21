/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2010 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/types.h"
#include "../common/opcodemgr.h"
#include "../common/eq_stream_factory.h"
#include "../common/timer.h"
#include "../common/platform.h"
#include "../common/crash.h"
#include "../common/eqemu_logsys.h"
#include "../common/event/timer.h"
#include "../common/path_manager.h"

#include "eq_crypto.h"
#include "login_server.h"
#include <time.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <mutex>
#include <unordered_set>

std::unordered_set<uint32> ipWhitelist;
std::mutex		ipMutex;
bool bSkipFactoryAuth = true;

TimeoutManager timeout_manager;
LoginServer server;
EQEmuLogSys LogSys;
EQCrypto eq_crypto;
bool run_server = true;
PathManager path;

void CatchSignal(int sig_num)
{
}

void LoadDatabaseConnection()
{
	LogInfo("MySQL Database Init.");
	server.db = (Database *)new Database(
		server.config.GetVariableString("database", "user", "user"),
		server.config.GetVariableString("database", "password", "password"),
		server.config.GetVariableString("database", "host", "127.0.0.1"),
		server.config.GetVariableString("database", "port", "3306"),
		server.config.GetVariableString("database", "db", "eqemu")
	);
}

int main()
{
	RegisterExecutablePlatform(ExePlatformLogin);
	LogSys.LoadLogSettingsDefaults();
	set_exception_handler();
	LogInfo("Logging System Init.");

	LogSys.log_settings[Logs::Error].log_to_console = Logs::General;

	path.LoadPaths();

	server.config = EQ::JsonConfigFile::Load(
		fmt::format("{}/login.json", path.GetServerPath())
	);
	LogInfo("Config System Init.");

	server.options.AutoCreateAccounts(server.config.GetVariableBool("account", "auto_create_accounts", true));
	server.options.RejectDuplicateServers(server.config.GetVariableBool("worldservers", "reject_duplicate_servers", false));

	server.options.AllowUnregistered(server.config.GetVariableBool("security", "unregistered_allowed", true));
	server.options.AllowTokenLogin(server.config.GetVariableBool("security", "allow_token_login", false));
	server.options.AllowPasswordLogin(server.config.GetVariableBool("security", "allow_password_login", true));
	server.options.EncryptionMode(server.config.GetVariableInt("security", "mode", 5));

	server.options.LocalNetwork(server.config.GetVariableString("client_configuration", "local_network", "127.0.0.1"));
	server.options.NetworkIP(server.config.GetVariableString("client_configuration", "network_ip", "127.0.0.1"));

	server.options.AccountTable(server.config.GetVariableString("schema", "account_table", "tblLoginServerAccounts"));
	server.options.WorldRegistrationTable(server.config.GetVariableString("schema", "world_registration_table", "tblWorldServerRegistration"));
	server.options.WorldAdminRegistrationTable(server.config.GetVariableString("schema", "world_admin_registration_table", "tblServerAdminRegistration"));
	server.options.WorldServerTypeTable(server.config.GetVariableString("schema", "world_server_type_table", "tblServerListType"));
	server.options.LoginSettingTable(server.config.GetVariableString("schema", "loginserver_setting_table", "tblloginserversettings"));

	/* Create database connection */
	if (server.config.GetVariableString("database", "subsystem", "MySQL").compare("MySQL") == 0) {
		LoadDatabaseConnection();
	}

	LogSys.SetDatabase(server.db)
		->SetLogPath("logs")
		->LoadLogDatabaseSettings()
		->StartFileLogs();

	/* Make sure our database got created okay, otherwise cleanup and exit. */
	if (!server.db) {
		LogError("Database Initialization Failure.");
		LogInfo("Log System Shutdown.");
		return 1;
	}

	//create our server manager.
	LogInfo("Server Manager Initialize.");
	server.server_manager = new ServerManager();
	if (!server.server_manager) {
		//We can't run without a server manager, cleanup and exit.
		LogError("Server Manager Failed to Start.");

		LogInfo("Database System Shutdown.");
		delete server.db;
		return 1;
	}

	//create our client manager.
	LogInfo("Client Manager Initialize.");
	server.client_manager = new ClientManager();
	if (!server.client_manager) {
		//We can't run without a client manager, cleanup and exit.
		LogError("Client Manager Failed to Start.");
		LogInfo("Server Manager Shutdown.");
		delete server.server_manager;

		LogInfo("Database System Shutdown.");
		delete server.db;
		return 1;
	}

#ifdef WIN32
#ifdef UNICODE
	SetConsoleTitle(L"EQEmu Login Server");
#else
	SetConsoleTitle("EQEmu Login Server");
#endif
#endif

	LogInfo("Server Started.");
	
	auto loop_fun = [&](EQ::Timer* t) {
		Timer::SetCurrentTime();
		
		if (!run_server) {
			EQ::EventLoop::Get().Shutdown();
			return;
		}
		ipMutex.lock();
		ipWhitelist.clear();
		ipMutex.unlock();
		
		server.client_manager->Process();
		server.server_manager->Process();
		timeout_manager.CheckTimeouts();
	};

	EQ::Timer proccess_timer(loop_fun);
	proccess_timer.Start(32, true);

	EQ::EventLoop::Get().Run();

	LogInfo("Server Shutdown.");

	LogInfo("Client Manager Shutdown.");
	delete server.client_manager;

	LogInfo("Server Manager Shutdown.");
	delete server.server_manager;

	LogInfo("Database System Shutdown.");
	delete server.db;

	LogSys.CloseFileLogs();

	return 0;
}

