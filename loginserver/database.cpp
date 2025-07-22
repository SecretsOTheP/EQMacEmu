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
#include "database.h"
#include "login_server.h"
#include "../common/eqemu_logsys.h"
#include "../common/strings.h"

extern EQEmuLogSys LogSys;
extern LoginServer server;

Database::Database(std::string user, std::string pass, std::string host, std::string port, std::string name)
{
	this->user = user;
	this->pass = pass;
	this->host = host;
	this->name = name;

	uint32 errnum = 0;
	char   errbuf[MYSQL_ERRMSG_SIZE];
	if (!Open(
		host.c_str(),
		user.c_str(),
		pass.c_str(),
		name.c_str(),
		atoi(port.c_str()),
		&errnum,
		errbuf
	)
		) {
		Log(Logs::General, Logs::Error, "Failed to connect to database: Error: %s", errbuf);
		exit(1);
	}
	else {
		Log(Logs::General, Logs::Status, "Using database '%s' at %s:%d", m_database, host.c_str(), atoi(port.c_str()));
	}
}

Database::~Database()
{
	if(m_database)
	{
		mysql_close(m_database);
	}
}

bool Database::GetLoginDataFromAccountName(std::string &name, std::string &password, unsigned int &id)
{
	auto query = fmt::format("SELECT LoginServerID, AccountPassword FROM {} WHERE AccountName = '{}';",
		server.options.GetAccountTable(),
		Strings::Escape(name)
	);

	auto results = QueryDatabase(query);

	if (results.RowCount() != 1) {
		LogDebug("Could not find account for name [{0}]", name);
		return false;
	}

	if (!results.Success()) {
		return false;
	}

	auto row = results.begin();

	id = atoi(row[0]);
	password = row[1];

	LogDebug("Found account for name [{0}]",
		name
	);

	return true;
}

bool Database::GetLoginTokenDataFromToken(const std::string& token, const std::string& ip, unsigned int& db_account_id, std::string& user)
{
	auto query = fmt::format(
		"SELECT tbllogintokens.Id, tbllogintokens.IpAddress, tbllogintokenclaims.Name, tbllogintokenclaims.Value FROM tbllogintokens "
		"JOIN tbllogintokenclaims ON tbllogintokens.Id = tbllogintokenclaims.TokenId WHERE tbllogintokens.Expires > NOW() "
		"AND tbllogintokens.Id='{0}' AND tbllogintokens.IpAddress='{1}'",
		Strings::Escape(token),
		Strings::Escape(ip)
	);
	auto results = QueryDatabase(query);
	if (results.RowCount() == 0 || !results.Success()) {
		return false;
	}

	bool found_username = false;
	bool found_login_id = false;
	for (auto row = results.begin(); row != results.end(); ++row) {
		if (strcmp(row[2], "username") == 0) {
			user = row[3];
			found_username = true;
			continue;
		}

		if (strcmp(row[2], "login_server_id") == 0) {
			db_account_id = atoi(row[3]);
			found_login_id = true;
			continue;
		}
	}

	return found_username && found_login_id;
}

bool Database::CreateLoginData(const std::string &name, const std::string &password, unsigned int &id)
{
	auto query = fmt::format("SELECT ifnull(max(LoginServerID),0) + 1 FROM {0}",
		server.options.GetAccountTable());

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() != 1) {
		return false;
	}

	auto row = results.begin();

	id = atoi(row[0]);

	auto insert_query = fmt::format("INSERT INTO {0} (AccountName, AccountPassword, AccountEmail, LastLoginDate, LastIPAddress, creationIP) "
		" VALUES('{1}', SHA('{2}'), 'local_creation', NOW(), '127.0.0.1', '127.0.0.1'); ",
		server.options.GetAccountTable(),
		Strings::Escape(name),
		Strings::Escape(password)
	);

	auto insert_results = QueryDatabase(insert_query);

	if (!insert_results.Success()) {
		LogError("Account did not existed in the database for {0} ",
			Strings::Escape(name)
		);

		return false;
	}

	return true;
}

bool Database::GetWorldRegistration(std::string long_name, std::string short_name, unsigned int &id, std::string &desc, unsigned int &list_id,
	unsigned int &trusted, std::string &list_desc, std::string &account, std::string &password)
{
	auto query = fmt::format(
		"SELECT\n"
		"  ifnull(WSR.ServerID, 999999) AS ServerID,\n"
		"  WSR.ServerTagDescription,\n"
		"  ifnull(WSR.ServerTrusted, 0) AS ServerTrusted,\n"
		"  ifnull(SLT.ServerListTypeID, 3) AS ServerListTypeID,\n"
		"  SLT.ServerListTypeDescription,\n"
		"  ifnull(WSR.ServerAdminID, 0) AS ServerAdminID\n"
		"FROM\n"
		"  {0} AS WSR\n"
		"  JOIN {1} AS SLT ON WSR.ServerListTypeID = SLT.ServerListTypeID\n"
		"WHERE\n"
		"  WSR.ServerShortName = '{2}' LIMIT 1",
		server.options.GetWorldRegistrationTable(),
		server.options.GetWorldServerTypeTable(),
		Strings::Escape(short_name)
	);

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() != 1) {
		return false;
	}

	auto row = results.begin();

	id = atoi(row[0]);
	desc = row[1];
	trusted = atoi(row[2]);
	list_id = atoi(row[3]);
	list_desc = row[4];

	int db_account_id = atoi(row[5]);
	if (db_account_id > 0) {

		auto world_registration_query = fmt::format(
			"SELECT AccountName, AccountPassword FROM {0} WHERE ServerAdminID = {1} LIMIT 1",
			server.options.GetWorldAdminRegistrationTable(),
			db_account_id
		);

		auto world_registration_results = QueryDatabase(world_registration_query);
		if (!world_registration_results.Success() || world_registration_results.RowCount() != 1) {
			return false;
		}

		auto world_registration_row = world_registration_results.begin();

		account = world_registration_row[0];
		password = world_registration_row[1];
	}

	return true;
}

void Database::UpdateLSAccountData(unsigned int id, std::string ip_address)
{
	auto query = fmt::format(
		"UPDATE {0} SET LastIPAddress = '{1}', LastLoginDate = NOW() where LoginServerId = {2}",
		server.options.GetAccountTable(),
		ip_address,
		id
	);

	QueryDatabase(query);
}


void Database::UpdateLSAccountInfo(unsigned int id, std::string name, std::string password, std::string email)
{
	auto query = fmt::format(
		"REPLACE {0} SET LoginServerID = {1}, AccountName = '{2}', AccountPassword = sha('{3}'), AccountCreateDate = now(), "
		"AccountEmail = '{4}', LastIPAddress = '0.0.0.0', LastLoginDate = now()",
		server.options.GetAccountTable(),
		id,
		Strings::Escape(name),
		Strings::Escape(password),
		Strings::Escape(email)
	);

	QueryDatabase(query);
}

void Database::UpdateWorldRegistration(unsigned int id, std::string long_name, std::string ip_address)
{
	auto query = fmt::format(
		"UPDATE {0} SET ServerLastLoginDate = NOW(), ServerLastIPAddr = '{1}', ServerLongName = '{2}' WHERE ServerID = {3}",
		server.options.GetWorldRegistrationTable(),
		ip_address,
		Strings::Escape(long_name),
		id
	);

	QueryDatabase(query);
}

bool Database::CreateWorldRegistration(std::string long_name, std::string short_name, unsigned int &id)
{
	auto query = fmt::format(
		"SELECT ifnull(max(ServerID),0) + 1 FROM {0}",
		server.options.GetWorldRegistrationTable()
	);

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() != 1) {
		return false;
	}

	auto row = results.begin();

	id = atoi(row[0]);

	auto insert_query = fmt::format(
		"INSERT INTO {0} SET ServerID = {1}, ServerLongName = '{2}', ServerShortName = '{3}', \n"
		"ServerListTypeID = 3, ServerAdminID = 0, ServerTrusted = 0, ServerTagDescription = ''",
		server.options.GetWorldRegistrationTable(),
		id,
		long_name,
		short_name
	);

	auto insert_results = QueryDatabase(insert_query);
	if (!insert_results.Success()) {
		LogF(
			Logs::General,
			Logs::Error,
			"World registration did not exist in the database for {0} - {1}",
			long_name,
			short_name
		);

		return false;
	}

	return true;
}

bool Database::GetLoginSettings(std::string type, std::string value)
{
	auto query = fmt::format(
		"SELECT * FROM {0} ",
		server.options.GetLoginSettingTable()
	);

	auto results = QueryDatabase(query);

	auto row = results.begin();

	type = row[0];
	value = row[1];

	return true;
}

bool Database::GetWorldPreferredStatus(int id)
{
	auto query = fmt::format("SELECT ServerListTypeID FROM {} WHERE ServerID = {} ",
		server.options.GetWorldRegistrationTable(),
		id);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}
	return true;
}

std::string Database::LoginSettings(std::string type)
{
	std::string query;
	std::string value;

	query = fmt::format(
		"SELECT * FROM {0} "
		"WHERE "
		"type = '{1}' ",
		server.options.GetLoginSettingTable(),
		type
	);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		LogError("LoadServerSettings Mysql query return no results: {0}", query);
	}
	auto row = results.begin();
	if (row[1] != nullptr)
	{
		value = row[1];
	}
	return value.c_str();
}

bool Database::CheckSettings(int type)
{
	if (type == 1)
	{
		auto query = fmt::format("SHOW TABLES LIKE '{}' ", server.options.GetLoginSettingTable());

		auto results = QueryDatabase(query);

		if (!results.Success()) {
			return false;
		}
		LogInfo("tblloginserversettings exists sending continue.");
	}
	else if (type == 2)
	{
		auto query = fmt::format("SELECT * FROM {} ", server.options.GetLoginSettingTable());

		auto results = QueryDatabase(query);

		if (!results.Success()) {
			return false;
		}
		LogInfo("tblloginserversettings entries exist sending continue.");
	}
	else
	{
		return false;
	}

	return true;
}

bool Database::CheckExtraSettings(std::string type)
{
	LogInfo("Entered CheckExtraSettings using type: [{0}].", type.c_str());

	auto query = fmt::format("SELECT * FROM `{0}` WHERE `type` = '{1}';", server.options.GetLoginSettingTable(), type);

	auto check_results = QueryDatabase(query);

	if (!check_results.Success()) {
		return false;
	}

	LogInfo("CheckExtraSettings type: [{0}] exists.", type);

	return true;
}
