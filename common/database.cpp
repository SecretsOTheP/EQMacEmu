/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2015 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/rulesys.h"
#include "../common/servertalk.h"
#include <ctype.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <algorithm>
#include <mysqld_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Disgrace: for windows compile
#ifdef _WINDOWS
#include <windows.h>
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else
#include "unix.h"
#include <netinet/in.h>
#include <sys/time.h>
#endif

#include "database.h"
#include "eq_packet_structs.h"
#include "extprofile.h"
#include "strings.h"
#include "random.h"

extern Client client;
EQ::Random emudb_random;

Database::Database () {
}

/*
Establish a connection to a mysql database with the supplied parameters
*/

Database::Database(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	Connect(host, user, passwd, database, port);
}

bool Database::Connect(const char* host, const char* user, const char* passwd, const char* database, uint32 port) {
	uint32 errnum= 0;
	char errbuf[MYSQL_ERRMSG_SIZE];
	if (!Open(host, user, passwd, database, port, &errnum, errbuf)) {
		LogError("[MySQL] Failed to connect to database: Error: [{}] ", errbuf); 
		return false; 
	}
	else {
		LogInfo("[MySQL] Using database [{}] at [{}] : [{}] ", database, host, port);
		return true;
	}
}

/*
	Close the connection to the database
*/

Database::~Database()
{
}

/*
	Check if there is an account with name "name" and password "password"
	Return the account id or zero if no account matches.
	Zero will also be returned if there is a database error.
*/
uint32 Database::CheckLogin(const char* name, const char* password, int16* oStatus, int8* oRevoked) {

	if(strlen(name) >= 50 || strlen(password) >= 50)
		return(0);

	char temporary_username[100];
	char temporary_password[100];

	DoEscapeString(temporary_username, name, strlen(name));
	DoEscapeString(temporary_password, password, strlen(password));

	std::string query = fmt::format(
		"SELECT id, status, revoked FROM account WHERE name= '{}' AND password is not null "
		"and length(password) > 0 and (password = '{}' or password = MD5('{}'))",
		temporary_username, 
		temporary_password, 
		temporary_password
	);

	auto results = QueryDatabase(query);

	if (!results.Success() || !results.RowCount()) {
		return 0;
	}

	auto row = results.begin();

	auto id = std::stoul(row[0]);

	if (oStatus)
		*oStatus = std::stoi(row[1]);

	if (oRevoked)
		*oRevoked = std::stoi(row[2]);

	return id;
}

//Get Banned IP Address List - Only return false if the incoming connection's IP address is not present in the banned_ips table.
bool Database::CheckBannedIPs(std::string login_ip)
{
	auto query = fmt::format(
		"SELECT ip_address FROM Banned_IPs WHERE ip_address= '{}'", 
		login_ip
	);

	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() != 0) 
	{
		return true;
	}

	return false;
}

bool Database::AddBannedIP(std::string banned_ip, std::string notes) {
	auto query = fmt::format(
		"INSERT into Banned_IPs SET ip_address= '{}', notes = '{}'", 
		Strings::Escape(banned_ip),
		Strings::Escape(notes)
	);
	auto results = QueryDatabase(query); 

	if (!results.Success()) {
		return false;
	} 

	return true;
}

 bool Database::CheckGMIPs(std::string login_ip, uint32 account_id) {
	auto query = fmt::format(
		"SELECT * FROM `gm_ips` WHERE `ip_address` = '{}' AND `account_id` = {} ",
		login_ip, 
		account_id
	); 
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	if (results.RowCount() == 1)
	{
		return true;
	}

	return false;
}

bool Database::AddGMIP(char* ip_address, char* name) {
	std::string query = StringFormat("INSERT into `gm_ips` SET `ip_address` = '%s', `name` = '%s'", ip_address, name); 
	auto results = QueryDatabase(query); 
	return results.Success();
}

void Database::LoginIP(uint32 account_id, std::string login_ip) {
	auto query = fmt::format(
		"INSERT INTO account_ip SET accid = {}, ip = '{}' ON DUPLICATE KEY UPDATE count=count+1, lastused=now()", 
		account_id, 
		login_ip
	); 
	QueryDatabase(query); 
}

int16 Database::CheckStatus(uint32 account_id) {
	auto query = fmt::format(
		"SELECT `status`, UNIX_TIMESTAMP(`suspendeduntil`) as `suspendeduntil`, UNIX_TIMESTAMP() as `current` FROM `account` WHERE `id` = {} ", 
		account_id
	);

	auto results = QueryDatabase(query); 
	if (!results.Success() || results.RowCount() != 1) {
		return 0;
	}

	
	auto row = results.begin(); 
	int16 status = std::stoi(row[0]);
	int32 suspendeduntil = 0;

	// MariaDB initalizes with NULL if unix_timestamp() is out of range
	if (row[1] != nullptr) {
		suspendeduntil = std::stoi(row[1]);
	}

	int32 current = atoi(row[2]);

	if(suspendeduntil > current)
		return -1;

	return status;
}

uint8 Database::CheckRevoked(uint32 account_id) {
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

int16 Database::CheckExemption(uint32 account_id)
{
	std::string query = StringFormat("SELECT `ip_exemption_multiplier` FROM `account` WHERE `id` = %i", account_id);
	Log(Logs::General, Logs::WorldServer, "Checking exemption on account ID: '%i'.", account_id);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		return 1;
	}

	if (results.RowCount() != 1)
	{
		return 1;
	}

	auto row = results.begin();
	int16 exemption = atoi(row[0]);

	return exemption;
}

uint32 Database::CreateAccount(const char* name, const char* password, int16 status, uint32 lsaccount_id) {
	std::string query;

	uint8 expansions = status >= 100 ? AllEQ : RuleI(Character, DefaultExpansions);
	if (password)
		query = StringFormat("INSERT INTO account SET name='%s', password='%s', status=%i, lsaccount_id=%i, expansion=%i, time_creation=UNIX_TIMESTAMP();",name,password,status, lsaccount_id, expansions);
	else
		query = StringFormat("INSERT INTO account SET name='%s', status=%i, lsaccount_id=%i, expansion=%i, time_creation=UNIX_TIMESTAMP();",name, status, lsaccount_id, expansions);

	Log(Logs::General, Logs::WorldServer, "Account Attempting to be created: '%s' status: %i", name, status);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.LastInsertedID() == 0)
	{
		return 0;
	}

	return results.LastInsertedID();
}

bool Database::DeleteAccount(const char* name) {
	std::string query = StringFormat("DELETE FROM account WHERE name='%s';",name); 
	Log(Logs::General, Logs::WorldServer, "Account Attempting to be deleted:'%s'", name);

	auto results = QueryDatabase(query); 
	if (!results.Success()) {
		return false;
	}

	return results.RowsAffected() == 1;
}

bool Database::SetLocalPassword(uint32 accid, const char* password) {
	std::string query = StringFormat("UPDATE account SET password=MD5('%s') where id=%i;", Strings::Escape(password).c_str(), accid);

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	return true;
}

bool Database::SetAccountStatus(const char* name, int16 status) {
	std::string query = StringFormat("UPDATE account SET status=%i WHERE name='%s';", status, name);

	std::cout << "Account being GM Flagged:" << name << ", Level: " << (int16) status << std::endl;

	auto results = QueryDatabase(query);

	if (!results.Success())
		return false;

	if (results.RowsAffected() == 0)
	{
		std::cout << "Account: " << name << " does not exist, therefore it cannot be flagged\n";
		return false; 
	}

	return true;
}

bool Database::SetAccountStatus(const std::string& account_name, int16 status)
{
	LogInfo("Account [{}] is attempting to be set to status [{}]", account_name, status);

	std::string query = fmt::format(
		SQL(
			UPDATE account SET status = {} WHERE name = '{}'
		),
		status,
		account_name
	);

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0) {
		LogWarning("Account [{}] does not exist!", account_name);
		return false;
	}

	return true;
}

/* This initially creates the character during character create */
bool Database::ReserveName(uint32 account_id, char* name) {
	std::string query = StringFormat("SELECT `account_id`, `name` FROM `character_data` WHERE `name` = '%s'", name);
	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		if (row[0] && atoi(row[0]) > 0){
			Log(Logs::General, Logs::WorldServer, "Account: %i tried to request name: %s, but it is already taken...", account_id, name);
			return false;
		}
	}

	query = StringFormat("INSERT INTO `character_data` SET `account_id` = %i, `name` = '%s'", account_id, name); 
	results = QueryDatabase(query);
	if (!results.Success() || results.ErrorMessage() != ""){ return false; } 
	return true;
}

bool Database::MarkCharacterDeleted(char *name) {

	std::string unix_timestamp_name = Strings::Escape(name);

	uint64 current_time = Timer::GetTimeSeconds();

	unix_timestamp_name += std::to_string(current_time);

	std::string query = StringFormat("UPDATE character_data SET is_deleted = 1, name='%s' where name='%s'", unix_timestamp_name.c_str(), Strings::Escape(name).c_str());

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0) {
		return false;
	}

	return true;
}

bool Database::UnDeleteCharacter(const char *name) {
	std::string query = StringFormat("UPDATE character_data SET is_deleted = 0 where name='%s'", Strings::Escape(name).c_str());

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0) {
		return false;
	}

	return true;
}

bool Database::SetIPExemption(const char *name, uint8 amount) {
	std::string query = StringFormat("UPDATE account SET ip_exemption_multiplier = %d where name='%s'", amount, Strings::Escape(name).c_str());

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0) {
		return false;
	}

	return true;
}

bool Database::SetMule(const char* charname) {
	// get account for the character with charname
	std::string query = StringFormat("SELECT `account_id`, `name` FROM `character_data` WHERE `name` = '%s'", charname);

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() != 1) {
		return false;
	}
	auto row = results.begin();
	uint32 account_id = std::stoul(row[0]);
	
	// iterate over every character associated with account and verify they're all level 1 (exclude char with charname)
	query = StringFormat("SELECT `account_id` FROM `character_data` WHERE `account_id` = %u", account_id);
	results = QueryDatabase(query);
	if (results.RowCount() != 1) {
		Log(Logs::General, Logs::WorldServer, "Can not set mule status on account because more than one character exists on account.");
		return false;
	}

	// finally set account to mule status
	query = StringFormat("UPDATE account SET mule = %d where id = %u AND status < 80", 1, account_id);
	results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}
	if (results.RowsAffected() == 0) {
		return false;
	}

	return true;
}

bool Database::SetMule(const char *name, uint8 toggle) {
	std::string query = StringFormat("UPDATE account SET mule = %d, expansion = 12 where name='%s' AND status < 80", toggle, Strings::Escape(name).c_str());

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0) {
		return false;
	}

	return true;
}

/*
	Delete the character with the name "name"
	returns false on failure, true otherwise
*/
bool Database::DeleteCharacter(char *name) {
	uint32 charid = 0;
	if(!name ||	!strlen(name)) {
		Log(Logs::General, Logs::WorldServer, "DeleteCharacter: request to delete without a name (empty char slot)");
		return false;
	}
	Log(Logs::General, Logs::WorldServer, "Database::DeleteCharacter name : '%s'", name);

	/* Get id from character_data before deleting record so we can clean up the rest of the tables */
	std::string query = StringFormat("SELECT `id` from `character_data` WHERE `name` = '%s'", name);
	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) { charid = atoi(row[0]); }
	if (charid <= 0){ 
		LogError("Database::DeleteCharacter :: Character ({}) not found, stopping delete...", name);
		return false; 
	}

	query = StringFormat("DELETE FROM `quest_globals` WHERE `charid` = '%d'", charid); QueryDatabase(query);			   
	query = StringFormat("DELETE FROM `friends` WHERE `charid` = '%d'", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `mail` WHERE `charid` = '%d'", charid); QueryDatabase(query);					  		  			  
	query = StringFormat("DELETE FROM `titles` WHERE `char_id` = '%d'", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `player_titlesets` WHERE `char_id` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_inventory` WHERE `id` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_faction_values` WHERE `id` = '%d'", charid); QueryDatabase(query);		   
	query = StringFormat("DELETE FROM `character_data` WHERE `id` = '%d'", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `character_skills` WHERE `id` = %u", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `character_languages` WHERE `id` = %u", charid); QueryDatabase(query);			  
	query = StringFormat("DELETE FROM `character_bind` WHERE `id` = %u", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `character_alternate_abilities` WHERE `id` = %u", charid); QueryDatabase(query);  
	query = StringFormat("DELETE FROM `character_currency` WHERE `id` = %u", charid); QueryDatabase(query);			  
	query = StringFormat("DELETE FROM `character_data` WHERE `id` = %u", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `character_spells` WHERE `id` = %u", charid); QueryDatabase(query);				  
	query = StringFormat("DELETE FROM `character_memmed_spells` WHERE `id` = %u", charid); QueryDatabase(query);		  	  	  
	query = StringFormat("DELETE FROM `character_inspect_messages` WHERE `id` = %u", charid); QueryDatabase(query);	  
	query = StringFormat("DELETE FROM `character_zone_flags` WHERE `id` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_keyring` WHERE `id` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_buffs` WHERE `id` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_consent` WHERE `name` = '%s'", name); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_soulmarks` WHERE `id` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_timers` WHERE `id` = '%d'", charid); QueryDatabase(query);		
	query = StringFormat("DELETE FROM `guild_members` WHERE `char_id` = '%d'", charid); QueryDatabase(query);
	DeleteCharacterCorpses(charid);
	
	return true;
}

void Database::DeleteCharacterCorpses(uint32 charid) 
{

	std::string query = StringFormat("SELECT `id` from `character_corpses` WHERE `charid` = '%d'", charid);
	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) 
	{ 
		uint32 corpseid = atoi(row[0]); 
		std::string delete_query = StringFormat("DELETE FROM `character_corpse_items` WHERE `corpse_id` = '%d'", corpseid); QueryDatabase(delete_query);	
		delete_query = StringFormat("DELETE FROM `character_corpse_items_backup` WHERE `corpse_id` = '%d'", corpseid); QueryDatabase(delete_query);	
	}

	query = StringFormat("DELETE FROM `character_corpses` WHERE `charid` = '%d'", charid); QueryDatabase(query);	
	query = StringFormat("DELETE FROM `character_corpses_backup` WHERE `charid` = '%d'", charid); QueryDatabase(query);	

}

bool Database::SaveCharacterCreate(uint32 character_id, uint32 account_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat(
		"REPLACE INTO `character_data` ("
		"id,"
		"account_id,"
		"`name`,"
		"last_name,"
		"gender,"
		"race,"
		"class,"
		"`level`,"
		"deity,"
		"birthday,"
		"last_login,"
		"time_played,"
		"pvp_status,"
		"level2,"
		"anon,"
		"gm,"
		"intoxication,"
		"hair_color,"
		"beard_color,"
		"eye_color_1,"
		"eye_color_2,"
		"hair_style,"
		"beard,"
		"title,"
		"suffix,"
		"exp,"
		"points,"
		"mana,"
		"cur_hp,"
		"str,"
		"sta,"
		"cha,"
		"dex,"
		"`int`,"
		"agi,"
		"wis,"
		"face,"
		"y,"
		"x,"
		"z,"
		"heading,"
		"autosplit_enabled,"
		"zone_change_count,"
		"hunger_level,"
		"thirst_level,"
		"zone_id,"
		"air_remaining,"
		"aa_points_spent,"
		"aa_exp,"
		"aa_points,"
		"boatid,"
		"boatname,"
		"fatigue)"
		"VALUES ("
		"%u,"  // id					
		"%u,"  // account_id			
		"'%s',"  // `name`				
		"'%s',"  // last_name			
		"%u,"  // gender				
		"%u,"  // race					
		"%u,"  // class					
		"%u,"  // `level`				
		"%u,"  // deity					
		"%u,"  // birthday				
		"%u,"  // last_login			
		"%u,"  // time_played			
		"%u,"  // pvp_status			
		"%u,"  // level2				
		"%u,"  // anon					
		"%u,"  // gm					
		"%u,"  // intoxication			
		"%u,"  // hair_color			
		"%u,"  // beard_color			
		"%u,"  // eye_color_1			
		"%u,"  // eye_color_2			
		"%u,"  // hair_style			
		"%u,"  // beard					
		"'%s',"  // title				
		"'%s',"  // suffix				
		"%u,"  // exp					
		"%u,"  // points				
		"%u,"  // mana					
		"%u,"  // cur_hp				
		"%u,"  // str					
		"%u,"  // sta					
		"%u,"  // cha					
		"%u,"  // dex					
		"%u,"  // `int`					
		"%u,"  // agi					
		"%u,"  // wis					
		"%u,"  // face					
		"%f,"  // y						
		"%f,"  // x						
		"%f,"  // z						
		"%f,"  // heading				
		"%u,"  // autosplit_enabled		
		"%u,"  // zone_change_count					
		"%i,"  // hunger_level			
		"%i,"  // thirst_level					
		"%u,"  // zone_id						
		"%u,"  // air_remaining			
		"%u,"  // aa_points_spent		
		"%u,"  // aa_exp				
		"%u,"  // aa_points				
		"%u,"  // boatid		
		"'%s',"// boatname	
		"%i"   // fatigue
		")",
		character_id,					  // " id,                        "
		account_id,						  // " account_id,                "
		Strings::Escape(pp->name).c_str(),	  // " `name`,                    "
		Strings::Escape(pp->last_name).c_str(), // " last_name,              "
		pp->gender,						  // " gender,                    "
		pp->race,						  // " race,                      "
		pp->class_,						  // " class,                     "
		pp->level,						  // " `level`,                   "
		pp->deity,						  // " deity,                     "
		pp->birthday,					  // " birthday,                  "
		pp->lastlogin,					  // " last_login,                "
		pp->timePlayedMin,				  // " time_played,               "
		pp->pvp,						  // " pvp_status,                "
		pp->level2,						  // " level2,                    "
		pp->anon,						  // " anon,                      "
		pp->gm,							  // " gm,                        "
		pp->intoxication,				  // " intoxication,              "
		pp->haircolor,					  // " hair_color,                "
		pp->beardcolor,					  // " beard_color,               "
		pp->eyecolor1,					  // " eye_color_1,               "
		pp->eyecolor2,					  // " eye_color_2,               "
		pp->hairstyle,					  // " hair_style,                "
		pp->beard,						  // " beard,                     "
		Strings::Escape(pp->title).c_str(),  // " title,                     "
		Strings::Escape(pp->suffix).c_str(), // " suffix,                    "
		pp->exp,						  // " exp,                       "
		pp->points,						  // " points,                    "
		pp->mana,						  // " mana,                      "
		pp->cur_hp,						  // " cur_hp,                    "
		pp->STR,						  // " str,                       "
		pp->STA,						  // " sta,                       "
		pp->CHA,						  // " cha,                       "
		pp->DEX,						  // " dex,                       "
		pp->INT,						  // " `int`,                     "
		pp->AGI,						  // " agi,                       "
		pp->WIS,						  // " wis,                       "
		pp->face,						  // " face,                      "
		pp->y,							  // " y,                         "
		pp->x,							  // " x,                         "
		pp->z,							  // " z,                         "
		pp->heading,					  // " heading,                   "
		pp->autosplit,					  // " autosplit_enabled,         "
		pp->zone_change_count,			  // " zone_change_count,         "
		pp->hunger_level,				  // " hunger_level,              "
		pp->thirst_level,				  // " thirst_level,              "
		pp->zone_id,					  // " zone_id,                   "
		pp->air_remaining,				  // " air_remaining,             "
		pp->aapoints_spent,				  // " aa_points_spent,           "
		pp->expAA,						  // " aa_exp,                    "
		pp->aapoints,					  // " aa_points,                 "
		pp->boatid,						  // " boatid,					  "
		Strings::Escape(pp->boat).c_str(),// " boatname,                  "
		pp->fatigue						  // " fatigue					  "
	);
	auto results = QueryDatabase(query);
	/* Save Bind Points */
	query = StringFormat("REPLACE INTO `character_bind` (id, zone_id, x, y, z, heading, is_home)"
		" VALUES (%u, %u, %f, %f, %f, %f, %i), "
		"(%u, %u, %f, %f, %f, %f, %i)",
		character_id, pp->binds[0].zoneId, pp->binds[0].x, pp->binds[0].y, pp->binds[0].z, pp->binds[0].heading, 0,
		character_id, pp->binds[4].zoneId, pp->binds[4].x, pp->binds[4].y, pp->binds[4].z, pp->binds[4].heading, 1
	); results = QueryDatabase(query); 

	/* Save Skills */
	int firstquery = 0;
	for (int i = 0; i < MAX_PP_SKILL; i++){
		if (pp->skills[i] > 0){
			if (firstquery != 1){
				firstquery = 1;
				query = StringFormat("REPLACE INTO `character_skills` (id, skill_id, value) VALUES (%u, %u, %u)", character_id, i, pp->skills[i]);
			}
			else{
				query = query + StringFormat(", (%u, %u, %u)", character_id, i, pp->skills[i]);
			}
		}
	}
	results = QueryDatabase(query); 

	/* Save Language */
	firstquery = 0;
	for (int i = 0; i < MAX_PP_LANGUAGE; i++){
		if (pp->languages[i] > 0){
			if (firstquery != 1){
				firstquery = 1;
				query = StringFormat("REPLACE INTO `character_languages` (id, lang_id, value) VALUES (%u, %u, %u)", character_id, i, pp->languages[i]);
			}
			else{
				query = query + StringFormat(", (%u, %u, %u)", character_id, i, pp->languages[i]);
			}
		}
	}
	results = QueryDatabase(query); 

	return true;
}

/* This only for new Character creation storing */
bool Database::StoreCharacter(uint32 account_id, PlayerProfile_Struct* pp, EQ::InventoryProfile* inv) {
	uint32 charid = 0; 
	char zone[50]; 
	float x, y, z; 
	charid = GetCharacterID(pp->name);

	if(!charid) {
		LogError("StoreCharacter: no character id");
		return false;
	}

	const char *zname = GetZoneName(pp->zone_id);
	if(zname == nullptr) {
		/* Zone not in the DB, something to prevent crash... */
		strn0cpy(zone, "bazaar", 49);
		pp->zone_id = bazaar;
	}
	else{ strn0cpy(zone, zname, 49); }

	x = pp->x;
	y = pp->y;
	z = pp->z;

	/* Saves Player Profile Data */
	SaveCharacterCreate(charid, account_id, pp); 

	/* Insert starting inventory... */
	std::string invquery;
	for (int16 i= EQ::invslot::SLOT_BEGIN; i<= EQ::invbag::BANK_BAGS_END;)
	{
		const EQ::ItemInstance* newinv = inv->GetItem(i);
		if (newinv) {
			invquery = StringFormat("INSERT INTO `character_inventory` (id, slotid, itemid, charges) VALUES (%u, %i, %u, %i)",
				charid, i, newinv->GetItem()->ID, newinv->GetCharges()); 
			
			auto results = QueryDatabase(invquery); 
		}

		if (i == EQ::invslot::GENERAL_END) {
			i = EQ::invbag::GENERAL_BAGS_BEGIN;
			continue;
		}
		else if (i == EQ::invbag::CURSOR_BAG_END) {
			i = EQ::invslot::BANK_BEGIN;
			continue; 
		}
		else if (i == EQ::invslot::BANK_END) {
			i = EQ::invbag::BANK_BAGS_BEGIN;
			continue; 
		} 
		i++;
	}
	return true;
}

uint32 Database::GetCharacterID(const char *name) {
	std::string query = StringFormat("SELECT `id` FROM `character_data` WHERE `name` = '%s'", name);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		std::cerr << "Error in GetAccountIDByChar query '" << query << "' " << results.ErrorMessage() << std::endl;
		return 0;
	}

	if (results.RowCount() != 1)
		return 0; 

	auto row = results.begin();
	if (row[0]){ return atoi(row[0]); }
	return 0; 
}

/*
	This function returns the account_id that owns the character with
	the name "name" or zero if no character with that name was found
	Zero will also be returned if there is a database error.
*/
uint32 Database::GetAccountIDByChar(const char* charname, uint32* oCharID) {
	std::string query = StringFormat("SELECT `account_id`, `id` FROM `character_data` WHERE name='%s'", Strings::Escape(charname).c_str());

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return 0;
	}

	if (results.RowCount() != 1)
		return 0; 

	auto row = results.begin();

	uint32 accountId = atoi(row[0]);

	if (oCharID)
		*oCharID = atoi(row[1]);

	return accountId;
}

uint32 Database::GetLevelByChar(const char* charname) {
	std::string query = StringFormat("SELECT `level` FROM `character_data` WHERE name='%s'", Strings::Escape(charname).c_str());
	auto results = QueryDatabase(query); 
	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin(); 
	return atoi(row[0]);
}

uint32 Database::GetHardcoreStatus(const char* charname) {
	std::string query = StringFormat("SELECT `e_hardcore` FROM `character_data` WHERE name='%s'", Strings::Escape(charname).c_str());
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();
	return atoi(row[0]);
}


// Retrieve account_id for a given char_id
uint32 Database::GetAccountIDByChar(uint32 char_id) {
	std::string query = StringFormat("SELECT `account_id` FROM `character_data` WHERE `id` = %i LIMIT 1", char_id); 
	auto results = QueryDatabase(query); 
	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin(); 
	return atoi(row[0]);
}

uint32 Database::GetAccountIDByName(std::string account_name, int16* status, uint32* lsid) {
	if (!isAlphaNumeric(account_name.c_str()))
		return 0;

	auto query = fmt::format(
		"SELECT `id`, `status`, `lsaccount_id` FROM `account` WHERE `name` = '{}' LIMIT 1",
		Strings::Escape(account_name)
	);
	auto results = QueryDatabase(query);

	if (!results.Success() || !results.RowCount()) {
		return 0;
	}

	auto row = results.begin();
	auto account_id = std::stoul(row[0]);

	if (status)
		*status = static_cast<int16>(std::stoi(row[1]));

	if (lsid) {
		*lsid = row[2] ? std::stoul(row[2]) : 0;
	}

	return account_id;
}

void Database::GetAccountName(uint32 accountid, char* name, uint32* oLSAccountID) {
	std::string query = StringFormat("SELECT `name`, `lsaccount_id` FROM `account` WHERE `id` = '%i'", accountid); 
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return;
	}

	if (results.RowCount() != 1)
		return;

	auto row = results.begin();

	strcpy(name, row[0]);
	if (row[1] && oLSAccountID) {
		*oLSAccountID = atoi(row[1]);
	}

}

void Database::GetCharName(uint32 char_id, char* name) {
	std::string query = StringFormat("SELECT `name` FROM `character_data` WHERE id='%i'", char_id);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return; 
	}

	auto row = results.begin();
	for (auto row = results.begin(); row != results.end(); ++row) {
		strcpy(name, row[0]);
	}
}

/**
 * Checks if the input name is reserved relative to the input account_id
 */
bool Database::IsCharacterNameReserved(uint32 account_id, const char* input_character_name) {
	std::string query = StringFormat("SELECT account_id FROM `account_name_reservation` WHERE name='%s'", input_character_name);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return true; // be defensive on error condition
	}

	if (results.RowCount() == 0) {
		return false; // no reservation for name found
	}

	auto row = results.begin();
	uint32 reserved_by = static_cast<uint32>(atoul(row[0]));
	if (account_id == reserved_by) {
		return false; // name is reserved for this account_id so we can return false
	} else {
		return true; // name is reserved but not for this account_id so we return true
	};
}

bool Database::LoadVariables() {
	auto results = QueryDatabase(StringFormat("SELECT varname, value, unix_timestamp() FROM variables where unix_timestamp(ts) >= %d", varcache.last_update));

	if (!results.Success())
		return false;

	if (results.RowCount() == 0)
		return true;

	LockMutex lock(&Mvarcache);

	std::string key, value;
	for (auto row = results.begin(); row != results.end(); ++row) {
		varcache.last_update = atoi(row[2]); // ahh should we be comparing if this is newer?
		key = row[0];
		value = row[1];
		std::transform(std::begin(key), std::end(key), std::begin(key), ::tolower); // keys are lower case, DB doesn't have to be
		varcache.Add(key, value);
	}

	return true;
}

// Gets variable from 'variables' table
bool Database::GetVariable(std::string varname, std::string &varvalue)
{
	varvalue.clear();

	LockMutex lock(&Mvarcache);

	if (varname.empty())
		return false;

	std::transform(std::begin(varname), std::end(varname), std::begin(varname), ::tolower); // all keys are lower case
	auto tmp = varcache.Get(varname);
	if (tmp) {
		varvalue = *tmp;
		return true;
	}
	return false;
}

bool Database::SetVariable(const std::string varname, const std::string &varvalue)
{
	std::string escaped_name = Strings::Escape(varname);
	std::string escaped_value = Strings::Escape(varvalue);
	std::string query = StringFormat("Update variables set value='%s' WHERE varname like '%s'", escaped_value.c_str(), escaped_name.c_str());
	auto results = QueryDatabase(query);

	if (!results.Success())
		return false;

	if (results.RowsAffected() == 1)
	{
		LoadVariables(); // refresh cache
		return true;
	}

	query = StringFormat("Insert Into variables (varname, value) values ('%s', '%s')", escaped_name.c_str(), escaped_value.c_str());
	results = QueryDatabase(query);

	if (results.RowsAffected() != 1)
		return false;

	LoadVariables(); // refresh cache
	return true;
}

// Get zone starting points from DB
bool Database::GetSafePoints(const char* short_name, float* safe_x, float* safe_y, float* safe_z, float* safe_heading, int16* minstatus, uint8* minlevel, char *flag_needed, uint8* expansion) {
	
	if (short_name == nullptr)
		return false;

	std::string query = StringFormat("SELECT safe_x, safe_y, safe_z, safe_heading, min_status, min_level, flag_needed, expansion FROM zone WHERE short_name='%s'", short_name);
	auto results = QueryDatabase(query);

	if (!results.Success())
		return false;

	if (results.RowCount() == 0)
		return false;

	auto row = results.begin();

	if (safe_x != nullptr)
		*safe_x = atof(row[0]);
	if (safe_y != nullptr)
		*safe_y = atof(row[1]);
	if (safe_z != nullptr)
		*safe_z = atof(row[2]);
	if (safe_heading != nullptr)
		*safe_heading = atof(row[3]);
	if (minstatus != nullptr)
		*minstatus = atoi(row[4]);
	if (minlevel != nullptr)
		*minlevel = atoi(row[5]);
	if (flag_needed != nullptr)
		strcpy(flag_needed, row[6]);
	if(expansion != nullptr)
	{
		uint8 zone_expansion = atoi(row[7]);
		if(zone_expansion == 1)
			*expansion = ClassicEQ;

		else if(zone_expansion == 2)
			*expansion = KunarkEQ;

		else if(zone_expansion == 3)
			*expansion = VeliousEQ;

		else if(zone_expansion == 4)
			*expansion = LuclinEQ;

		else if(zone_expansion == 5)
			*expansion = PlanesEQ;

		else
			*expansion = 0;
	}

	return true;
}

bool Database::GetZoneLongName(const char* short_name, char** long_name, char* file_name, float* safe_x, float* safe_y, float* safe_z, uint32* graveyard_id, uint16* graveyard_time, uint32* maxclients) {
	
	std::string query = StringFormat("SELECT long_name, file_name, safe_x, safe_y, safe_z, graveyard_id, graveyard_time, maxclients FROM zone WHERE short_name='%s'", short_name);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowCount() == 0)
		return false;

	auto row = results.begin();

	if (long_name != nullptr)
		*long_name = strcpy(new char[strlen(row[0])+1], row[0]);

	if (file_name != nullptr) {
		if (row[1] == nullptr)
			strcpy(file_name, short_name);
		else
			strcpy(file_name, row[1]);
	}

	if (safe_x != nullptr)
		*safe_x = atof(row[2]);
	if (safe_y != nullptr)
		*safe_y = atof(row[3]);
	if (safe_z != nullptr)
		*safe_z = atof(row[4]);
	if (graveyard_id != nullptr)
		*graveyard_id = atoi(row[5]);
	if (graveyard_time != nullptr)
		*graveyard_time = atoi(row[6]);
	if (maxclients != nullptr)
		*maxclients = atoi(row[7]);

	return true;
}

uint32 Database::GetZoneGraveyardID(uint32 zone_id) {

	std::string query = StringFormat("SELECT graveyard_id FROM zone WHERE zoneidnumber='%u'", zone_id);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return 0;
	}

	if (results.RowCount() == 0)
		return 0;

	auto row = results.begin();
	return atoi(row[0]);
}

uint16 Database::GetGraveyardTime(uint16 zone_id) {

	std::string query = StringFormat("SELECT graveyard_time FROM zone WHERE zoneidnumber='%u'", zone_id);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return 0;
	}

	if (results.RowCount() == 0)
		return 0;

	auto row = results.begin();
	return atoi(row[0]);
}

bool Database::GetZoneGraveyard(const uint32 graveyard_id, uint32* graveyard_zoneid, float* graveyard_x, float* graveyard_y, float* graveyard_z, float* graveyard_heading) {
	
	std::string query = StringFormat("SELECT zone_id, x, y, z, heading FROM graveyard WHERE id=%i", graveyard_id);
	auto results = QueryDatabase(query);

	if (!results.Success()){
		return false;
	}

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();

	if(graveyard_zoneid != nullptr)
		*graveyard_zoneid = atoi(row[0]);
	if(graveyard_x != nullptr)
		*graveyard_x = atof(row[1]);
	if(graveyard_y != nullptr)
		*graveyard_y = atof(row[2]);
	if(graveyard_z != nullptr)
		*graveyard_z = atof(row[3]);
	if(graveyard_heading != nullptr)
		*graveyard_heading = atof(row[4]);

	return true;
}

bool Database::LoadZoneNames() {
	std::string query("SELECT zoneidnumber, short_name FROM zone");

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	for (auto row= results.begin();row != results.end();++row)
	{
		uint32 zoneid = atoi(row[0]);
		std::string zonename = row[1];
		zonename_array.insert(std::pair<uint32,std::string>(zoneid,zonename));
	}

	return true;
}

uint32 Database::GetZoneID(const char* zonename) {

	if (zonename == nullptr)
		return 0;

	for (auto iter = zonename_array.begin(); iter != zonename_array.end(); ++iter)
		if (strcasecmp(iter->second.c_str(), zonename) == 0)
			return iter->first;

	return 0;
}

const char* Database::GetZoneName(uint32 zoneID, bool ErrorUnknown) {
	auto iter = zonename_array.find(zoneID);

	if (iter != zonename_array.end())
		return iter->second.c_str();

	if (ErrorUnknown)
		return "UNKNOWN";

	return 0;
}

uint8 Database::GetPEQZone(uint32 zoneID){
	
	std::string query = StringFormat("SELECT peqzone from zone where zoneidnumber='%i'", zoneID);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() == 0)
		return 0;

	auto row = results.begin();

	return atoi(row[0]);
}

uint8 Database::GetMinStatus(uint32 zone_id)
{
	auto query = fmt::format(
		"SELECT min_status FROM zone WHERE zoneidnumber = {} ",
		zone_id
	);
	auto results = QueryDatabase(query);
	if (!results.Success() || !results.RowCount()) {
		return 0;
	}

	auto row = results.begin();

	return static_cast<uint8>(std::stoul(row[0]));
}

uint8 Database::GetZoneRandomLoc(uint32 zoneID) {

	std::string query = StringFormat("SELECT random_loc from zone where zoneidnumber='%i'", zoneID);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() == 0)
		return 0;

	auto row = results.begin();

	return atoi(row[0]);
}

bool Database::CheckNameFilter(const char* name, bool surname) {
	// Rule 1: Name length constraints.
	//         - If surname: at least 3 characters and must not start with a '`'.
	//         - Otherwise: between 4 and 15 characters (inclusive).
	// Rule 2: For surnames only: One '`' character is allowed but not more.
	//         Otherwise, only alphabet characters are allowed in the name.
	// Rule 3: No more than two consecutive repeating characters are allowed.
	// Rule 4: Name must not match any restricted names in the database (case-insensitive).

	std::string str_name = name;

	// Rule 1: Check the length constraints.
	if (surname) {
		if (!name || strlen(name) < 3) {
			return false;
		}
	}
	else {
		// The minimum 4 is enforced by the client.
		if (!name || strlen(name) < 4 || strlen(name) > 15) {
			return false;
		}
	}

	// Ensure a surname doesn't start with a backtick.
	if (surname && !str_name.empty() && str_name[0] == '`') {
		return false;
	}

	// Rule 2: Check for allowed characters and optional backtick for surnames.
	bool special_char_found = false;
	for (size_t i = 0; i < str_name.size(); i++) {
		if (str_name[i] == '`' || str_name[i] == '\'') {
			// For non-surnames, if special char already found, or if it's at the start/end of the string.
			if (!surname || special_char_found || i == 0 || i == str_name.size() - 1) {
				return false;
			}
			special_char_found = true;
		}
		else if (!isalpha(str_name[i])) {
			return false;
		}
	}

	// Convert the entire name to lowercase for further checks.
	for (size_t x = 0; x < str_name.size(); ++x) {
		str_name[x] = tolower(str_name[x]);
	}

	// Rule 3: Check for consecutive repeating characters.
	char c = '\0';
	uint8 num_c = 0;
	for (size_t x = 0; x < str_name.size(); ++x) {
		if (str_name[x] == c) {
			num_c++;
		}
		else {
			num_c = 1;
			c = str_name[x];
		}
		if (num_c > 2) {
			return false;
		}
	}

	// Rule 4: Check against restricted names in the database.
	std::string query("SELECT name FROM name_filter");
	auto results = QueryDatabase(query);

	// If the database query fails, default to true (based on original behavior).
	if (!results.Success())
	{
		return true;
	}

	// Compare each restricted name from the database to the input name.
	for (auto row = results.begin(); row != results.end(); ++row)
	{
		std::string current_row = row[0];
		for (size_t x = 0; x < current_row.size(); ++x)
			current_row[x] = tolower(current_row[x]);

		if (str_name.find(current_row) != std::string::npos)
			return false;
	}

	// If the name has passed all the checks, return true.
	return true;
}

bool Database::AddToNameFilter(const char* name) {
	
	std::string query = StringFormat("INSERT INTO name_filter (name) values ('%s')", name);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	if (results.RowsAffected() == 0)
		return false;

	return true;
}

uint32 Database::GetAccountIDFromLSID(uint32 iLSID, char* oAccountName, int16* oStatus, int8* oRevoked, bool* isMule) {
	uint32 account_id = 0;
	std::string query = StringFormat("SELECT id, name, status, revoked, mule FROM account WHERE lsaccount_id=%i", iLSID);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	for (auto row = results.begin(); row != results.end(); ++row) {
		account_id = atoi(row[0]);

		if (oAccountName)
			strcpy(oAccountName, row[1]);
		if (oStatus)
			*oStatus = atoi(row[2]);
		if (oRevoked)
			*oRevoked = atoi(row[3]);
		if (isMule)
			*isMule = atoi(row[4]);
	}

	return account_id;
}

void Database::GetAccountFromID(uint32 id, char* oAccountName, int16* oStatus, int8* oRevoked) {
	
	std::string query = StringFormat("SELECT name, status, revoked FROM account WHERE id=%i", id);
	auto results = QueryDatabase(query);

	if (!results.Success()){
		return;
	}

	if (results.RowCount() != 1)
		return;

	auto row = results.begin();

	if (oAccountName)
		strcpy(oAccountName, row[0]);
	if (oStatus)
		*oStatus = atoi(row[1]);
	if (oRevoked)
		*oRevoked = atoi(row[2]);
}

void Database::ClearMerchantTemp(){
	QueryDatabase("DELETE FROM merchantlist_temp");
}

bool Database::UpdateName(const char* oldname, const char* newname) { 
	std::cout << "Renaming " << oldname << " to " << newname << "..." << std::endl; 
	std::string query = StringFormat("UPDATE `character_data` SET `name` = '%s' WHERE `name` = '%s';", newname, oldname);
	auto results = QueryDatabase(query);

	if (!results.Success())
		return false;

	if (results.RowsAffected() == 0)
		return false;

	return true;
}

// If the name is used or an error occurs, it returns false, otherwise it returns true
bool Database::CheckUsedName(const char* name, uint32 charid) {
	std::string query = StringFormat("SELECT `id` FROM `character_data` WHERE `name` = '%s' AND id != %d", name, charid);
	auto results = QueryDatabase(query); 
	if (!results.Success()) {
		return false;
	}

	if (results.RowCount() > 0)
		return false;

	return true;
}

uint8 Database::GetServerType() {
	std::string query("SELECT `value` FROM `variables` WHERE `varname` = 'ServerType' LIMIT 1");
	auto results = QueryDatabase(query); 
	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();
	return atoi(row[0]);
}

bool Database::MoveCharacterToZone(const char* charname, const char* zonename, uint32 zoneid) 
{
	if(zonename == nullptr || strlen(zonename) == 0)
		return false;

	float safe_x = 0, safe_y = 0, safe_z = 0;
	GetSafePoints(zoneid, &safe_x, &safe_y, &safe_z);
	std::string query = StringFormat("UPDATE `character_data` SET `zone_id` = %i, `x` = %0.2f, `y` = %0.2f, `z` = %0.2f WHERE `name` = '%s'", zoneid, safe_x, safe_y, safe_z, charname);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0)
		return false;

	return true;
}

bool Database::MoveCharacterToZone(const char* charname, const char* zonename) 
{
	return MoveCharacterToZone(charname, zonename, GetZoneID(zonename));
}

bool Database::MoveCharacterToZone(uint32 iCharID, const char* iZonename) 
{ 
	float safe_x = 0, safe_y = 0, safe_z = 0;
	GetSafePoints(iZonename, &safe_x, &safe_y, &safe_z);
	std::string query = StringFormat("UPDATE `character_data` SET `zone_id` = %i, `x` = %0.2f, `y` = %0.2f, `z` = %0.2f WHERE `id` = %i", GetZoneID(iZonename), safe_x, safe_y, safe_z, iCharID);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	return results.RowsAffected() != 0;
}

uint16 Database::MoveCharacterToBind(uint32 CharID) 
{

	std::string query = StringFormat("SELECT zone_id, x, y, z, heading FROM character_bind WHERE id = %u AND is_home = 0 LIMIT 1", CharID);
	auto results = QueryDatabase(query);
	if(!results.Success() || results.RowCount() == 0) 
	{
		return 0;
	}

	int zone_id = 0;
	double x = 0;
	double y = 0;
	double z = 0;
	double heading = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		zone_id = atoi(row[0]);
		x = atof(row[2]);
		y = atof(row[3]);
		z = atof(row[3]);
		heading = atof(row[4]);
	}

	query = StringFormat("UPDATE character_data SET zone_id = '%d', x = '%f', y = '%f', z = '%f', heading = '%f' WHERE id = %u", 
						 zone_id, x, y, z, heading, CharID);

	results = QueryDatabase(query);
	if(!results.Success()) {
		return 0;
	}

	return zone_id;
}

bool Database::SetHackerFlag(const char* accountname, const char* charactername, const char* hacked) { 
	std::string new_hacked = std::string(hacked);
	Strings::FindReplace(new_hacked, "'", "_");
	std::string query = StringFormat("INSERT INTO `hackers` (account, name, hacked) values('%s','%s','%s')", accountname, charactername, new_hacked.c_str());
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	return results.RowsAffected() != 0;
}

bool Database::SetMQDetectionFlag(const char* accountname, const char* charactername, const char* hacked, const char* zone) { 
	//Utilize the "hacker" table, but also give zone information.
	std::string new_hacked = std::string(hacked);
	Strings::FindReplace(new_hacked, "'", "_");
	std::string query = StringFormat("INSERT INTO hackers(account,name,hacked,zone) values('%s','%s','%s','%s')", accountname, charactername, new_hacked.c_str(), zone);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	return results.RowsAffected() != 0;
}

bool Database::SetMQDetectionFlag(const char* accountname, const char* charactername, const std::string& hacked, const char* zone) {
	//Utilize the "hacker" table, but also give zone information.
	auto query = fmt::format("INSERT INTO hackers(account, name, hacked, zone) values('{}', '{}', '{}', '{}')",
		accountname, charactername, hacked, zone);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	return results.RowsAffected() != 0;
}

uint8 Database::GetRaceSkill(uint8 skillid, uint8 in_race)
{
	//Check for a racial cap!
	std::string query = StringFormat("SELECT skillcap from race_skillcaps where skill = %i && race = %i", skillid, in_race);
	auto results = QueryDatabase(query);

	if (!results.Success())
		return 0;

	if (results.RowCount() == 0)
		return 0;

	auto row = results.begin();
	return atoi(row[0]);
}

uint8 Database::GetSkillCap(uint8 skillid, uint8 in_race, uint8 in_class, uint16 in_level)
{
	uint8 skill_level = 0, skill_formula = 0;
	uint16 base_cap = 0, skill_cap = 0, skill_cap2 = 0, skill_cap3 = 0;
	

	//Fetch the data from DB.
	std::string query = StringFormat("SELECT level, formula, pre50cap, post50cap, post60cap from skillcaps where skill = %i && class = %i", skillid, in_class);
	auto results = QueryDatabase(query);

	if (results.Success() && results.RowsAffected() != 0)
	{
		auto row = results.begin();
		skill_level = atoi(row[0]);
		skill_formula = atoi(row[1]);
		skill_cap = atoi(row[2]);
		if (atoi(row[3]) > skill_cap)
			skill_cap2 = (atoi(row[3])-skill_cap)/10; //Split the post-50 skill cap into difference between pre-50 cap and post-50 cap / 10 to determine amount of points per level.
		skill_cap3 = atoi(row[4]);
	}

	int race_skill = GetRaceSkill(skillid,in_race);

	if (race_skill > 0 && (race_skill > skill_cap || skill_cap == 0 || in_level < skill_level))
		return race_skill;

	if (skill_cap == 0) //Can't train this skill at all.
		return 255; //Untrainable

	if (in_level < skill_level)
		return 254; //Untrained

	//Determine pre-51 level-based cap
	if (skill_formula > 0)
		base_cap = in_level*skill_formula+skill_formula;
	if (base_cap > skill_cap || skill_formula == 0)
		base_cap = skill_cap;

	//If post 50, add post 50 cap to base cap.
	if (in_level > 50 && skill_cap2 > 0)
		base_cap += skill_cap2*(in_level-50);

	//No cap should ever go above its post50cap
	if (skill_cap3 > 0 && base_cap > skill_cap3)
		base_cap = skill_cap3;

	//Base cap is now the max value at the person's level, return it!
	return base_cap;
}

uint32 Database::GetCharacterInfo(const char* iName, uint32* oAccID, uint32* oZoneID, uint32* oZoneGuildID, float* oX, float* oY, float* oZ, uint64* oDeathTime) {
	std::string query = StringFormat("SELECT `id`, `account_id`, `zone_id`, `x`, `y`, `z`, `e_hardcore_death_time`, `e_zone_guild_id` FROM `character_data` WHERE `name` = '%s'", iName);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();
	uint32 charid = atoi(row[0]);
	if (oAccID){ *oAccID = atoi(row[1]); }
	if (oZoneID){ *oZoneID = atoi(row[2]); }
	if (oX){ *oX = atof(row[3]); }
	if (oY){ *oY = atof(row[4]); }
	if (oZ){ *oZ = atof(row[5]); }
	if (oDeathTime) { *oDeathTime = atoll(row[6]); }
	if (oZoneGuildID) { *oZoneGuildID = (unsigned long)atoll(row[7]); }
	return charid;
}

bool Database::UpdateLiveChar(char* charname,uint32 lsaccount_id) {

	std::string query = StringFormat("UPDATE account SET charname='%s' WHERE id=%i;",charname, lsaccount_id);
	auto results = QueryDatabase(query);
	if (!results.Success()){
		return false;
	}
	return true;
}

bool Database::CharacterJoin(uint32 char_id, char* char_name) {
	std::string join_query = StringFormat(
		"INSERT INTO `webdata_character` (	"
		"id,								" // char_id
		"name,								" // char_name
		"last_login,						" // time(nullptr)
		"last_seen							" // 0
		") VALUES (							"
		"%i,								" // id	
		"'%s',								" // name
		"%i,								" // last_login
		"0									" // last_seen
		") ON DUPLICATE KEY UPDATE			"
		"name='%s',							" // char_name
		"last_login=%i,						" // time(nullptr)
		"last_seen=0						" // 0
		,
		char_id,							  // id	
		char_name,							  // name
		time(nullptr),						  // last_login
		char_name,							  // name
		time(nullptr)						  // last_login
		);
	auto join_results = QueryDatabase(join_query);
	Log(Logs::Detail, Logs::WebInterfaceServer, "CharacterJoin should have written to database for %s with ID %i at %i and last_seen should be zero.", char_name, char_id, time(nullptr));

	if (!join_results.Success()){
		return false;
	}
	return true;
}

bool Database::CharacterQuit(uint32 char_id) {
	std::string query = StringFormat("UPDATE `webdata_character` SET `last_seen`='%i' WHERE `id` = '%i'", time(nullptr), char_id);
	auto results = QueryDatabase(query);
	
	if (!results.Success()){
		Log(Logs::Detail, Logs::Error, "Error updating character_data table from CharacterQuit.");
		return false;
	}
	Log(Logs::Detail, Logs::WebInterfaceServer, "CharacterQuit should have written to database for %i at %i", char_id, time(nullptr));
	return true;
}

bool Database::ZoneConnected(uint32 id, const char* name) {
	std::string connect_query = StringFormat(
		"INSERT INTO `webdata_servers` (	"
		"id,								" // id
		"name,								" // name
		"connected							" // 1
		") VALUES (							"
		"%i,								" // id
		"'%s',								" // name
		"1									" // connected
		") ON DUPLICATE KEY UPDATE			"
		"name='%s',							" // name
		"connected=1						" // 1
		,
		id,									// id
		name,								// name
		name								// name
		);
	auto connect_results = QueryDatabase(connect_query);
	Log(Logs::Detail, Logs::WebInterfaceServer, "ZoneConnected should have written id %i to webdata_servers for %s with connected status 1.", id, name);

	if (!connect_results.Success()){
		Log(Logs::Detail, Logs::Error, "Error updating zone status in webdata_servers table from ZoneConnected.");
		return false;
	}
	return true;
}

bool Database::ZoneDisconnect(uint32 id) {
	std::string query = StringFormat("UPDATE `webdata_servers` SET `connected`='0' WHERE `id` = '%i'", id);
	auto results = QueryDatabase(query);
	Log(Logs::Detail, Logs::WebInterfaceServer, "ZoneDisconnect should have written '0' to webdata_servers for %i.", id);
	if (!results.Success()){
		Log(Logs::Detail, Logs::Error, "Error updating webdata_servers table from ZoneConnected.");
		return false;
	}
	Log(Logs::Detail, Logs::WebInterfaceServer, "Updated webdata_servers table from ZoneDisconnected.");
	return true;
}

bool Database::LSConnected(uint32 port) {
	std::string connect_query = StringFormat(
		"INSERT INTO `webdata_servers` (	"
		"id,								" // id
		"name,								" // name
		"connected							" // 1
		") VALUES (							"
		"%i,								" // id
		"'LoginServer',						" // name
		"1									" // connected
		") ON DUPLICATE KEY UPDATE			"
		"connected=1						" // 1
		,
		port								// id
		);
	auto connect_results = QueryDatabase(connect_query);
	Log(Logs::Detail, Logs::WebInterfaceServer, "LSConnected should have written id %i to webdata_servers for LoginServer with connected status 1.", port);

	if (!connect_results.Success()){
		Log(Logs::Detail, Logs::Error, "Error updating LoginServer status in webdata_servers table from LSConnected.");
		return false;
	}
	return true;
}

bool Database::LSDisconnect() {
	std::string query = StringFormat("UPDATE `webdata_servers` SET `connected`='0' WHERE `name` = 'LoginServer'");
	auto results = QueryDatabase(query);
	Log(Logs::Detail, Logs::WebInterfaceServer, "LSConnected should have written to webdata_servers for LoginServer connected status 0.");
	if (!results.Success()){
		Log(Logs::Detail, Logs::Error, "Error updating webdata_servers table from LSDisconnect.");
		return false;
	}
	Log(Logs::Detail, Logs::WebInterfaceServer, "Updated webdata_servers table from LSDisconnect.");
	return true;
}

bool Database::GetLiveChar(uint32 account_id, char* cname) {

	std::string query = StringFormat("SELECT charname FROM account WHERE id=%i", account_id);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();
	strcpy(cname,row[0]);

	return true;
}

bool Database::GetLiveCharByLSID(uint32 ls_id, char* cname) {

	std::string query = StringFormat("SELECT charname FROM account WHERE lsaccount_id=%i", ls_id);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();
	strcpy(cname, row[0]);

	return true;
}

void Database::SetFirstLogon(uint32 CharID, uint8 firstlogon) { 
	std::string query = StringFormat( "UPDATE `character_data` SET `firstlogon` = %i WHERE `id` = %i",firstlogon, CharID);
	QueryDatabase(query); 
}

void Database::AddReport(std::string who, std::string against, std::string lines)
{
	auto escape_str = new char[lines.size() * 2 + 1];
	DoEscapeString(escape_str, lines.c_str(), lines.size());

	std::string query = StringFormat("INSERT INTO reports (name, reported, reported_text) VALUES('%s', '%s', '%s')", who.c_str(), against.c_str(), escape_str);
	QueryDatabase(query);
	safe_delete_array(escape_str);
}

void Database::SetGroupID(const char* name, uint32 id, uint32 charid, uint32 accountid){
	
	std::string query;
	if (id == 0) {
		// removing from group
		query = StringFormat("delete from group_id where charid=%i and name='%s'",charid, name);
		auto results = QueryDatabase(query);

		if (!results.Success())
			LogError("Error deleting character from group id: {} ", results.ErrorMessage().c_str());

		return;
	}

	/* Add to the Group */
	query = StringFormat("REPLACE INTO  `group_id` SET `charid` = %i, `groupid` = %i, `name` = '%s', `accountid` = %i", charid, id, name, accountid);
	QueryDatabase(query);
}

void Database::ClearAllGroups(void)
{
	std::string query("DELETE FROM `group_id`");
	QueryDatabase(query);
	return;
}

void Database::ClearGroup(uint32 gid) {
	ClearGroupLeader(gid);
	
	if(gid == 0)
	{
		//clear all groups
		ClearAllGroups();
		return;
	}

	//clear a specific group
	std::string query = StringFormat("delete from group_id where groupid = %lu", (unsigned long)gid);
	QueryDatabase(query);
}

uint32 Database::GetGroupID(const char* name){ 
	std::string query = StringFormat("SELECT groupid from group_id where name='%s'", name);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() == 0)
	{
		// Commenting this out until logging levels can prevent this from going to console
		//Log(Logs::General, Logs::Group,, "Character not in a group: %s", name);
		return 0;
	}

	auto row = results.begin();

	return atoi(row[0]);
}

bool Database::GetGroupMemberNames(uint32 group_id, char membername[6][64])
{
	std::string query = StringFormat("SELECT name FROM group_id WHERE groupid = %lu", (unsigned long)group_id);
	auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	if (results.RowCount() == 0) {
		Log(Logs::General, Logs::Error, "Error getting group members for group %lu: %s", (unsigned long)group_id, results.ErrorMessage().c_str());
		return false;
	}

	int memberIndex = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		if (!row[0])
			continue;

		strn0cpy(membername[memberIndex], row[0], 64);

		memberIndex++;
	}

	return true;
}

uint32 Database::GetGroupIDByAccount(uint32 accountid, std::string &charname) {
	std::string query = StringFormat("SELECT groupid, name from group_id where accountid='%d'", accountid);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() == 0)
	{
		return 0;
	}

	auto row = results.begin();

	charname = row[1];
	return atoi(row[0]);
}

/* Is this really getting used properly... A half implementation ? Akkadius */
char* Database::GetGroupLeaderForLogin(const char* name, char* leaderbuf) {
	strcpy(leaderbuf, "");
	uint32 group_id = 0;

	std::string query = StringFormat("SELECT `groupid` FROM `group_id` WHERE `name` = '%s'", name);
	auto results = QueryDatabase(query);

	for (auto row = results.begin(); row != results.end(); ++row)
		if (row[0])
			group_id = atoi(row[0]);

	if (group_id == 0)
		return leaderbuf;

	query = StringFormat("SELECT `leadername` FROM `group_leaders` WHERE `gid` = '%u' LIMIT 1", group_id);
	results = QueryDatabase(query);

	for (auto row = results.begin(); row != results.end(); ++row)
		if (row[0])
			strcpy(leaderbuf, row[0]);

	return leaderbuf;
}

void Database::SetGroupLeaderName(uint32 gid, const char* name) { 
	std::string query = StringFormat("REPLACE INTO `group_leaders` SET `gid` = %lu, `leadername` = '%s'",(unsigned long)gid,name);
	auto results = QueryDatabase(query);

	if (!results.Success())
		Log(Logs::General, Logs::Group, "Unable to set group leader:", results.ErrorMessage().c_str());
}

void Database::SetGroupOldLeaderName(uint32 gid, const char* name) {
	std::string query = StringFormat("UPDATE `group_leaders` SET `oldleadername` = '%s' WHERE gid = %d", name, (unsigned long)gid);
	auto results = QueryDatabase(query);

	if (!results.Success())
		Log(Logs::General, Logs::Group, "Unable to set old group old leader:", results.ErrorMessage().c_str());
}

char *Database::GetGroupLeadershipInfo(uint32 gid, char* leaderbuf){ 
	std::string query = StringFormat("SELECT `leadername` FROM `group_leaders` WHERE `gid` = %lu",(unsigned long)gid);
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0) {
		if(leaderbuf)
			strcpy(leaderbuf, "UNKNOWN");

		return leaderbuf;
	}

	auto row = results.begin();

	if(leaderbuf)
		strcpy(leaderbuf, row[0]);

	return leaderbuf;
}

std::string Database::GetGroupOldLeaderName(uint32 gid) 
{
	std::string query = StringFormat("SELECT `oldleadername` FROM `group_leaders` WHERE `gid` = %lu", (unsigned long)gid);
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0) {
	
		return "UNKNOWN";
	}

	auto row = results.begin();
	return row[0];
}

// Clearing all group leaders
void Database::ClearAllGroupLeaders(void) {
	std::string query("DELETE from group_leaders");
	auto results = QueryDatabase(query);

	if (!results.Success())
		std::cout << "Unable to clear group leaders: " << results.ErrorMessage() << std::endl;

	return;
}

void Database::ClearGroupLeader(uint32 gid) {
	
	if(gid == 0)
	{
		ClearAllGroupLeaders();
		return;
	}

	std::string query = StringFormat("DELETE from group_leaders where gid = %lu", (unsigned long)gid);
	auto results = QueryDatabase(query);

	if (!results.Success())
		std::cout << "Unable to clear group leader: " << results.ErrorMessage() << std::endl;
}

bool Database::GetAccountRestriction(uint32 acctid, uint16& expansion, bool& mule)
{
	std::string query = StringFormat("SELECT expansion, mule FROM account WHERE id=%i",acctid);
	auto results = QueryDatabase(query);

	if (!results.Success())
		return false;

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();

	expansion = atoi(row[0]);
	mule = atoi(row[1]);

	return true;
}

bool Database::SetExpansion(const char *name, uint8 toggle) {
	std::string query = StringFormat("UPDATE account SET expansion = %d where name='%s'", toggle, Strings::Escape(name).c_str());

	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	if (results.RowsAffected() == 0) {
		return false;
	}

	return true;
}

void Database::ClearRaid(uint32 rid) {
	if(rid == 0)
	{
		//clear all raids
		ClearAllRaids();
		return;
	}

	//clear a specific group
	std::string query = StringFormat("delete from raid_members where raidid = %lu", (unsigned long)rid);
	auto results = QueryDatabase(query);

	if (!results.Success())
		std::cout << "Unable to clear raids: " << results.ErrorMessage() << std::endl;
}

void Database::ClearAllRaids(void) {

	std::string query("delete from raid_members");
	auto results = QueryDatabase(query);

	if (!results.Success())
		std::cout << "Unable to clear raids: " << results.ErrorMessage() << std::endl;
}

void Database::ClearAllRaidDetails(void)
{

	std::string query("delete from raid_details");
	auto results = QueryDatabase(query);

	if (!results.Success())
		std::cout << "Unable to clear raid details: " << results.ErrorMessage() << std::endl;
}

void Database::ClearRaidDetails(uint32 rid) {
	
	if(rid == 0)
	{
		//clear all raids
		ClearAllRaidDetails();
		return;
	}

	//clear a specific group
	std::string query = StringFormat("delete from raid_details where raidid = %lu", (unsigned long)rid);
	auto results = QueryDatabase(query);

	if (!results.Success())
		std::cout << "Unable to clear raid details: " << results.ErrorMessage() << std::endl;
}

// returns 0 on error or no raid for that character, or
// the raid id that the character is a member of.
uint32 Database::GetRaidID(const char* name)
{ 
	std::string query = StringFormat("SELECT `raidid` FROM `raid_members` WHERE `name` = '%s'", name);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return 0;
	}

	auto row = results.begin(); 
	if (row == results.end()) {
		return 0;
	}

	if (row[0]) // would it ever be possible to have a null here?
		return atoi(row[0]);

	return 0;
}

bool Database::GetRaidGroupID(const char *name, uint32 *raidid, uint32 *groupid)
{
	std::string query = StringFormat("SELECT `raidid`, `groupid` FROM `raid_members` WHERE `name` = '%s'", name);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	auto row = results.begin();
	if (row == results.end()) {
		return false;
	}

	if (raidid != nullptr && row[0]) {
		*raidid = atoi(row[0]);
	}
	if (groupid != nullptr && row[1]) {
		*groupid = atoi(row[1]);
	}

	return true;
}

const char* Database::GetRaidLeaderName(uint32 raid_id)
{
	// Would be a good idea to fix this to be a passed in variable and
	// make the caller responsible. static local variables like this are
	// not guaranteed to be thread safe (nor is the internal guard
	// variable). C++0x standard states this should be thread safe
	// but may not be fully supported in some compilers.
	static char name[128];
	
	std::string query = StringFormat("SELECT `name` FROM `raid_members` WHERE `raidid` = %u AND `israidleader` = 1", raid_id);
	auto results = QueryDatabase(query);

	if (!results.Success()) {
		Log(Logs::General, Logs::Debug, "Unable to get Raid Leader Name for Raid ID: %u", raid_id);
		return "UNKNOWN";
	}

	auto row = results.begin();

	if (row == results.end()) {
		return "UNKNOWN";
	}

	memset(name, 0, sizeof(name));
	strcpy(name, row[0]);

	return name;
}

struct TimeOfDay_Struct Database::LoadTime(time_t &realtime)
{

	TimeOfDay_Struct eqTime;
	std::string query = StringFormat("SELECT minute,hour,day,month,year,realtime FROM eqtime limit 1");
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0)
	{
		Log(Logs::Detail, Logs::WorldServer, "Loading EQ time of day failed. Using defaults.");
		eqTime.minute = 0;
		eqTime.hour = 9;
		eqTime.day = 1;
		eqTime.month = 1;
		eqTime.year = 3100;
		realtime = time(0);
	}

	auto row = results.begin();

	uint8 hour = atoi(row[1]);
	time_t realtime_ = atoi(row[5]);
	if(RuleI(World, BootHour) > 0 && RuleI(World, BootHour) <= 24)
	{
		hour = RuleI(World, BootHour);
		realtime_ = time(0);
		Log(Logs::Detail, Logs::WorldServer, "EQTime: Setting hour to: %d", hour);
	}

	eqTime.minute = atoi(row[0]);
	eqTime.hour = hour;
	eqTime.day = atoi(row[2]);
	eqTime.month = atoi(row[3]);
	eqTime.year = atoi(row[4]);
	realtime = realtime_;

	return eqTime;
}


void Database::LoadQuakeData(ServerEarthquakeImminent_Struct& earthquake_struct)
{

	memset(&earthquake_struct, 0, sizeof(ServerEarthquakeImminent_Struct));

	std::string query = StringFormat("SELECT start_timestamp, next_timestamp, ruleset FROM quake_data");
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0)
	{
		return;
	}

	auto row = results.begin();
	uint32 realtime_ = atoi(row[0]);
	uint32 next_realtime_ = atoi(row[1]);
	earthquake_struct.start_timestamp = realtime_;
	earthquake_struct.next_start_timestamp = next_realtime_;
	earthquake_struct.quake_type = (QuakeType)atoi(row[2]);
}

bool Database::LoadNextQuakeTime(ServerEarthquakeImminent_Struct& earthquake_struct)
{


	std::string query = StringFormat("SELECT start_timestamp, next_timestamp, ruleset FROM quake_data");
	auto results = QueryDatabase(query);
	EQ::Random random;

	if (!results.Success() || results.RowCount() == 0)
	{
		random.Reseed();
		uint32 random_timestamp = random.Int(RuleI(Quarm, QuakeMinVariance), RuleI(Quarm, QuakeMaxVariance));
		Log(Logs::Detail, Logs::WorldServer, "Loading quake time failed.  Using default rules values and a random ruleset...");
		earthquake_struct.start_timestamp = Timer::GetTimeSeconds() + RuleI(Quarm, QuakeRepopDelay);
		earthquake_struct.next_start_timestamp = earthquake_struct.start_timestamp + random_timestamp;
		earthquake_struct.quake_type = QuakeNormal;

		std::string query1 = StringFormat("DELETE FROM quake_data");
		auto results1 = QueryDatabase(query1);

		std::string query2 = StringFormat("REPLACE INTO quake_data (start_timestamp, next_timestamp, ruleset) VALUES (%i, %i, %i)", earthquake_struct.start_timestamp, earthquake_struct.next_start_timestamp, earthquake_struct.quake_type);
		auto results2 = QueryDatabase(query2);
		return false;
	}

	auto row = results.begin();
	uint32 realtime_ = atoi(row[0]);
	uint32 next_realtime_ = atoi(row[1]);
	earthquake_struct.start_timestamp = realtime_;
	earthquake_struct.next_start_timestamp = next_realtime_;
	earthquake_struct.quake_type = (QuakeType)atoi(row[2]);

	//Force-Prep the next quake update in 15 minutes, if we are recovering from a server downtime during a quake. Ideally this would never happen but patch days do happen.
	if (RuleB(Quarm, EnableQuakeDowntimeRecovery))
	{
		if (Timer::GetTimeSeconds() < earthquake_struct.start_timestamp + RuleI(Quarm, QuakeEndTimeDuration))
		{
			Log(Logs::Detail, Logs::WorldServer, "Recovering-from-downtime within 24 hour window. Using default rules values and a random ruleset...");
			QuakeType ruleset = QuakeNormal;
			uint32 random_timestamp = random.Int(RuleI(Quarm, QuakeMinVariance), RuleI(Quarm, QuakeMaxVariance));
			earthquake_struct.start_timestamp = Timer::GetTimeSeconds() + RuleI(Quarm, QuakeRepopDelay);
			earthquake_struct.quake_type = ruleset;
			earthquake_struct.next_start_timestamp = earthquake_struct.start_timestamp + random_timestamp;


			std::string query1 = StringFormat("DELETE FROM quake_data");
			auto results1 = QueryDatabase(query1);

			std::string query2 = StringFormat("REPLACE INTO quake_data (start_timestamp, next_timestamp, ruleset) VALUES (%i, %i, %i)", earthquake_struct.start_timestamp, earthquake_struct.next_start_timestamp, earthquake_struct.quake_type);
			auto results2 = QueryDatabase(query2);
			return false;
		}
	}

	//Don't act on this data. Keep timer as-is.
	return true;
}

//For usage on quake trigger. Does the 'fail logic' from above in the load process to set the next timer and current timer.
bool Database::SaveNextQuakeTime(ServerEarthquakeImminent_Struct& earthquake_struct, QuakeType in_quake_type)
{
	EQ::Random random;
	random.Reseed();
	uint32 random_timestamp = random.Int(RuleI(Quarm, QuakeMinVariance), RuleI(Quarm, QuakeMaxVariance));
	earthquake_struct.start_timestamp = Timer::GetTimeSeconds() + RuleI(Quarm, QuakeRepopDelay);
	earthquake_struct.next_start_timestamp = earthquake_struct.start_timestamp + random_timestamp;
	earthquake_struct.quake_type = in_quake_type;


	std::string query1 = StringFormat("DELETE FROM quake_data");
	auto results1 = QueryDatabase(query1);

	std::string query = StringFormat("REPLACE INTO quake_data (start_timestamp, next_timestamp, ruleset) VALUES (%i, %i, %i)", earthquake_struct.start_timestamp, earthquake_struct.next_start_timestamp, earthquake_struct.quake_type);
	auto results = QueryDatabase(query);
	return results.Success();
}

bool Database::SaveTime(int8 minute, int8 hour, int8 day, int8 month, int16 year)
{
	std::string query = StringFormat("UPDATE eqtime set minute = %d, hour = %d, day = %d, month = %d, year = %d, realtime = %d limit 1", minute, hour, day, month, year, time(0));
	auto results = QueryDatabase(query);

	return results.Success();

}

void Database::ClearAllActive() {
	std::string query = "UPDATE `account` set active = 0";
	auto results = QueryDatabase(query);

	return;
}

void Database::ClearAccountActive(uint32 account_id) {
	std::string query = StringFormat("UPDATE `account` set active = 0 WHERE id = %i", account_id);
	auto results = QueryDatabase(query);

	return;
}

void Database::SetAccountActive(uint32 account_id)
{
	std::string query = StringFormat("UPDATE `account` SET active = 1 WHERE id = %i", account_id);
    QueryDatabase(query);

	return;
}

bool Database::AdjustSpawnTimes() 
{

	std::string dquery = StringFormat("DELETE rt FROM respawn_times AS rt inner join spawn2 AS s ON rt.id = s.id WHERE s.clear_timer_onboot = 1");
	auto dresults = QueryDatabase(dquery);

	if (!dresults.Success()) 
	{
		return false;
	}
	Log(Logs::General, Logs::WorldServer, "World has reset %d spawn timers.", dresults.RowsAffected());

	std::string query = StringFormat("SELECT id, boot_respawntime, boot_variance FROM spawn2 WHERE boot_respawntime > 0 or boot_variance > 0");
	auto results = QueryDatabase(query); 

	if (!results.Success()) 
	{
		return false;
	}

	if(results.RowCount() == 0) 
	{
		return true;
	}

	for (auto row = results.begin(); row != results.end(); ++row) 
	{
		uint32 rspawn = atoi(row[1]);
		uint32 variance = atoi(row[2]);
		int var = 0;

		if (rspawn < 0)
			rspawn = 0;

		if (variance > 0)
		{
			if (rspawn == 0)
			{
				rspawn = emudb_random.Int(0, variance);
			}
			else
			{
				var = variance / 2;
				rspawn = emudb_random.Int(rspawn - var, rspawn + var);
			}
		}

		std::string iquery = StringFormat("REPLACE INTO respawn_times SET id = %d, start = %d, duration = %d", atoi(row[0]), time(0), rspawn);
		auto iresults = QueryDatabase(iquery);

		if (!iresults.Success()) 
		{
			return false;
		}
		Log(Logs::General, Logs::WorldServer, "Boot time respawn timer adjusted for id: %d duration is: %d (base: %d var: %d)", atoi(row[0]), rspawn, atoi(row[1]), atoi(row[2]));
	}
	return true;
}

void Database::ClearAllConsented() {
	std::string query = "DELETE FROM character_consent";
	auto results = QueryDatabase(query);

	return;
}

void Database::ClearAllConsented(char* oname, uint32 corpse_id, LinkedList<ConsentDenied_Struct*>* purged)
{
	purged->Clear();
	std::string query = StringFormat("SELECT name FROM character_consent WHERE consenter_name = '%s' AND corpse_id = %u", oname, corpse_id);
	auto results = QueryDatabase(query);
	if (!results.Success())
		return;

	if (results.RowCount() == 0)
		return;

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		std::string gname = row[0];
		ConsentDenied_Struct* cd = new ConsentDenied_Struct;
		strn0cpy(cd->oname, oname, 64);
		strn0cpy(cd->gname, gname.c_str(), 64);
		cd->corpse_id = corpse_id;
		purged->Insert(cd);

		std::string delete_query = StringFormat("DELETE FROM character_consent WHERE consenter_name = '%s' AND name = '%s' AND corpse_id = %u", oname, gname.c_str(), corpse_id);
		auto delete_results = QueryDatabase(delete_query);

		if (delete_results.Success())
		{
			Log(Logs::General, Logs::Corpse, "%s (%u) cleared consent for character %s", oname, corpse_id, gname.c_str());
		}
	}
}

bool Database::NoRentExpired(const char* name) {
	std::string query = StringFormat("SELECT (UNIX_TIMESTAMP(NOW()) - last_login) FROM `character_data` WHERE name = '%s'", name);
	auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();
	uint32 seconds = atoi(row[0]);

	return (seconds>1800);
}

bool Database::LoadZoneFileNames() {

	zonename_filename_array.clear();

	std::string query("SELECT short_name, file_name FROM zone");

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		return false;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		if (row[1] && row[0] && strlen(row[1]) > 0 && strlen(row[1]) > 0)
		{
			std::string zoneshort = row[1];
			std::string zonefile = row[0];
			if (!zoneshort.empty() && !zonefile.empty())
				zonename_filename_array.insert(std::pair<std::string, std::string>(zonefile, zoneshort));
		}
	}

	return true;
}

uint32 Database::GetClientZoneID(uint32 zoneID) {
	auto iter = zonename_array.find(zoneID);

	if (iter != zonename_array.end())
	{
		auto iter2 = zonename_filename_array.find(iter->second.c_str());
		if (iter2 != zonename_filename_array.end())
		{
			if (!iter2->second.empty())
				return GetZoneID(iter2->second.c_str());
		}
	}

	return zoneID;
}

const char* Database::GetClientZoneName(const char* zone_name) {
	auto iter = zonename_filename_array.find(zone_name);

	if (iter != zonename_filename_array.end())
	{
		if (!iter->second.empty())
			return iter->second.c_str();
	}

	return zone_name;
}