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


#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <map>

// Disgrace: for windows compile
#ifdef _WINDOWS
#include <windows.h>
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else
#include "../common/unix.h"
#include <netinet/in.h>
#endif

#include "database.h"
#include "../common/eq_packet_structs.h"
#include "../common/misc_functions.h"
#include "../common/strings.h"
#include "chatchannel.h"

extern Clientlist *g_Clientlist;
extern ChatChannelList *ChannelList;

Database::Database ()
{
	DBInitVars();
}

/*
Establish a connection to a mysql database with the supplied parameters
*/

Database::Database(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	DBInitVars();
	Connect(host, user, passwd, database, port);
}

bool Database::Connect(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	uint32 errnum= 0;
	char errbuf[MYSQL_ERRMSG_SIZE];
	if (!Open(host, user, passwd, database, port, &errnum, errbuf))
	{
		LogError("Failed to connect to database: Error: [{}] ", errbuf);
		HandleMysqlError(errnum);

		return false;
	}
	else
	{
		LogStatus("Using database [{}] at [{}]:[{}]",database,host,port);
		return true;
	}
}

void Database::DBInitVars() {

}



void Database::HandleMysqlError(uint32 errnum) {
}

/*

Close the connection to the database
*/
Database::~Database()
{
}

void Database::GetAccountStatus(Client *client) {

	std::string query = StringFormat("SELECT `status`, `hideme`, `karma`, `revoked` "
                                    "FROM `account` WHERE `id` = %i LIMIT 1",
                                    client->GetAccountID());
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		LogMySQLError("Unable to get account status for character [{0}], error [{1}]", client->GetName().c_str(), results.ErrorMessage().c_str());
		return;
	}

	LogInfo("GetAccountStatus Query: [{0}]", query.c_str());

	if(results.RowCount() != 1)
	{
		LogMySQLError("Error in GetAccountStatus");
		return;
	}

	auto row = results.begin();

	client->SetAccountStatus(atoi(row[0]));
	client->SetHideMe(atoi(row[1]) != 0);
	client->SetKarma(atoi(row[2]));

	uint8 revoked = CheckUCSRevoked(client->GetAccountID());
	client->SetRevoked(revoked);

	LogDebug("Set account status to [{0}], hideme to [{1}] and karma to [{2}] for [{3}]", client->GetAccountStatus(), client->GetHideMe(), client->GetKarma(), client->GetName().c_str());

}

void Database::LogUCSPlayerSpeech(
	const char* from,
	const char* to,
	const char* message,
	uint16 minstatus,
	uint32 guilddbid,
	uint8 type,
	uint32 characterid,
	uint32 groupid
)
{

	auto escapedFrom = new char[strlen(from) * 2 + 1];
	auto escapedTo = new char[strlen(to) * 2 + 1];
	auto escapedMessage = new char[strlen(message) * 2 + 1];
	DoEscapeString(escapedFrom, from, strlen(from));
	DoEscapeString(escapedTo, to, strlen(to));
	DoEscapeString(escapedMessage, message, strlen(message));

	std::string query = StringFormat(
		"INSERT INTO `qs_player_speech` "
		"SET `from` = '%s', `to` = '%s', `message`='%s', "
		"`minstatus`='%i', `guilddbid`='%i', `type`='%i', "
		"`characterid`='%i', `groupid`='%i'",
		escapedFrom, escapedTo, escapedMessage, minstatus, guilddbid, type, characterid, groupid
	);
	safe_delete_array(escapedFrom);
	safe_delete_array(escapedTo);
	safe_delete_array(escapedMessage);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		LogInfo("Failed Speech Entry Insert: [{}]", results.ErrorMessage().c_str());
		LogInfo("[{}]", query.c_str());
	}


}

uint8 Database::CheckUCSRevoked(uint32 account_id) {
	auto query = fmt::format(
		"SELECT `revoked`, UNIX_TIMESTAMP(`revokeduntil`) as `revokeduntil`, UNIX_TIMESTAMP() as `current` FROM `account` WHERE `id` = {} ",
		account_id
	);

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() != 1) {
		return 0;
	}


	auto row = results.begin();
	uint8 status = std::stoi(row[0]);
	int32 suspendeduntil = 0;

	// MariaDB initalizes with NULL if unix_timestamp() is out of range
	if (row[1] != nullptr) {
		suspendeduntil = std::stoi(row[1]);
	}

	if (suspendeduntil == 0) // legacy behavior. Don't use timestamp in this case
		return status;

	int32 current = atoi(row[2]);

	if (suspendeduntil > current)
		return status;

	return 0;
}

int Database::FindAccount(const char *characterName, Client *client) {

	LogInfo("FindAccount for character [{0}]", characterName);


	client->ClearCharacters();
    std::string query = StringFormat("SELECT `id`, `account_id`, `level`, `race`, `class` "
                                    "FROM `character_data` WHERE `name` = '%s' AND `is_deleted` = 0 LIMIT 1",
                                    characterName);
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		LogMySQLError("FindAccount query failed: [{0}]", query.c_str());
		return -1;
	}

	if (results.RowCount() != 1) {
		LogMySQLError("Bad result from query");
		return -1;
	}

	auto row = results.begin();
	client->AddCharacter(atoi(row[0]), characterName, atoi(row[2]), atoi(row[3]), atoi(row[4]));

	int accountID = atoi(row[1]);

	LogMySQLQuery("Account ID for [{0}] is [{1}]", characterName, accountID);

    query = StringFormat("SELECT `id`, `name`, `level`, `race`, `class` FROM `character_data` "
                        "WHERE `account_id` = %i AND `name` != '%s' AND `is_deleted` = 0",
						accountID, characterName);
    results = QueryDatabase(query);
	if (!results.Success())
		return accountID;

	for (auto row = results.begin(); row != results.end(); ++row)
		client->AddCharacter(atoi(row[0]), row[1], atoi(row[2]), atoi(row[3]), atoi(row[4]));

	return accountID;
}

int Database::FindCharacter(const char *characterName)
{
	char *safeCharName = RemoveApostrophes(characterName);
	std::string query = StringFormat("SELECT `id` FROM `character_data` WHERE `name`='%s' LIMIT 1", safeCharName);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		safe_delete_array(safeCharName);
		return -1;
	}

	safe_delete_array(safeCharName);

	if (results.RowCount() != 1) {
		LogMySQLError("Bad result from FindCharacter query for character [{0}]",
			characterName);
		return -1;
	}

	auto row = results.begin();

	int characterID = atoi(row[0]);

	return characterID;
}

bool Database::GetVariable(const char* varname, char* varvalue, uint16 varvalue_len) {

	std::string query = StringFormat("SELECT `value` FROM `variables` WHERE `varname` = '%s'", varname);
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();

	snprintf(varvalue, varvalue_len, "%s", row[0]);

	return true;
}

bool Database::LoadChatChannels() {

	LogInfo("Loading chat channels from the database.");

	const std::string query = "SELECT `name`, `owner`, `password`, `minstatus` FROM `chatchannels`";
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	for (auto row = results.begin();row != results.end(); ++row) {
		std::string channelName = row[0];
		std::string channelOwner = row[1];
		std::string channelPassword = row[2];

		ChannelList->CreateChannel(channelName, channelOwner, channelPassword, true, atoi(row[3]));
	}

	return true;
}

void Database::SetChannelPassword(std::string channelName, std::string password) {

	LogInfo("Database::SetChannelPassword([{0}], [{1}])", channelName.c_str(), password.c_str());

	std::string query = StringFormat("UPDATE `chatchannels` SET `password` = '%s' WHERE `name` = '%s'",
                                    password.c_str(), channelName.c_str());
    QueryDatabase(query);
}

void Database::SetChannelOwner(std::string channelName, std::string owner) {

	LogInfo("Database::SetChannelOwner([{0}], [{1}])", channelName.c_str(), owner.c_str());

	std::string query = StringFormat("UPDATE `chatchannels` SET `owner` = '%s' WHERE `name` = '%s'",
                                    owner.c_str(), channelName.c_str());
    QueryDatabase(query);
}

void Database::SetMessageStatus(int messageNumber, int status) {

	LogInfo("SetMessageStatus [{0}] [{1}]", messageNumber, status);

	if(status == 0) {
        std::string query = StringFormat("DELETE FROM `mail` WHERE `msgid` = %i", messageNumber);
		auto results = QueryDatabase(query);
        return;
    }

    std::string query = StringFormat("UPDATE `mail` SET `status` = %i WHERE `msgid`=%i", status, messageNumber);
    QueryDatabase(query);
}

void Database::AddFriendOrIgnore(int charID, int type, std::string name) {

    std::string query = StringFormat("INSERT INTO `friends` (`charid`, `type`, `name`) "
                                    "VALUES('%i', %i, '%s')",
                                    charID, type, CapitaliseName(name).c_str());
    auto results = QueryDatabase(query);
	if(results.Success())
		LogMySQLQuery("Wrote Friend/Ignore entry for charid [{0}], type [{1}], name [{2}] to database.", charID, type, name.c_str());

}

void Database::RemoveFriendOrIgnore(int charID, int type, std::string name) {

	std::string query = StringFormat("DELETE FROM `friends` WHERE `charid` = %i "
                                    "AND `type` = %i AND `name` = '%s'",
                                    charID, type, CapitaliseName(name).c_str());
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		LogMySQLError("Error removing friend/ignore, query was [{0}]", query.c_str());
	}
	else {
		LogMySQLQuery("Removed Friend/Ignore entry for charid [{0}], type [{1}], name [{2}] from database.", charID, type, name.c_str());
	}
}

void Database::GetFriendsAndIgnore(int charID, std::vector<std::string> &friends, std::vector<std::string> &ignorees) {

	std::string query = StringFormat("select `type`, `name` FROM `friends` WHERE `charid`=%i", charID);
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return;
	}


	for (auto row = results.begin(); row != results.end(); ++row) {
		std::string name = row[1];

		if(atoi(row[0]) == 0)
		{
			ignorees.push_back(name);
			LogMySQLQuery("Added Ignoree from DB [{0}]", name.c_str());
			continue;
		}

        friends.push_back(name);
        LogMySQLQuery("Added Friend from DB [{0}]", name.c_str());
	}

}
