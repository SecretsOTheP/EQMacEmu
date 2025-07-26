/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2016 EQEMu Development Team (http://eqemulator.net)

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
#ifndef EQEMU_DATABASE_H
#define EQEMU_DATABASE_H

#define AUTHENTICATION_TIMEOUT	60
#define INVALID_ID				0xFFFFFFFF

#include "global_define.h"
#include "eqemu_logsys.h"
#include "types.h"
#include "dbcore.h"
#include "linked_list.h"
#include "servertalk.h"
#include "eq_packet_structs.h"

#include <cmath>
#include <string>
#include <vector>
#include <map>

//atoi is not uint32 or uint32 safe!!!!
#define atoul(str) strtoul(str, nullptr, 10)

class MySQLRequestResult;
class Client;

namespace EQ
{
	class InventoryProfile;
}

struct EventLogDetails_Struct {
	uint32	id;
	char	accountname[64];
	uint32	account_id;
	int16	status;
	char	charactername[64];
	char	targetname[64];
	char	timestamp[64];
	char	descriptiontype[64];
	char	details[128];
};

struct CharacterEventLog_Struct {
	uint32	count;
	uint8	eventid;
	EventLogDetails_Struct eld[255];
};

struct VarCache_Struct {
	std::map<std::string, std::string> m_cache;
	uint32 last_update;
	VarCache_Struct() : last_update(0) { }
	void Add(const std::string &key, const std::string &value) { m_cache[key] = value; }
	const std::string *Get(const std::string &key) {
		auto it = m_cache.find(key);
		return (it != m_cache.end() ? &it->second : nullptr);
	}
};

class PTimerList;

#ifdef _WINDOWS
#if _MSC_VER > 1700 // greater than 2012 (2013+)
#	define _ISNAN_(a) std::isnan(a)
#else
#	include <float.h>
#	define _ISNAN_(a) _isnan(a)
#endif
#else
#	define _ISNAN_(a) std::isnan(a)
#endif

#define SQL(...) #__VA_ARGS__

class LogSettings;
class Database : public DBcore {
public:
	Database();
	Database(const char* host, const char* user, const char* passwd, const char* database,uint32 port);
	bool Connect(const char* host, const char* user, const char* passwd, const char* database,uint32 port);
	~Database();
	
	bool	CharacterJoin(uint32 char_id, char* char_name);
	bool	CharacterQuit(uint32 char_id);
	bool	ZoneConnected(uint32 id, const char* name);
	bool	ZoneDisconnect(uint32 id);
	bool	LSConnected(uint32 port);
	bool	LSDisconnect();

	/* Character Creation */
	bool	SaveCharacterCreate(uint32 character_id, uint32 account_id, PlayerProfile_Struct* pp);
	
	bool    IsCharacterNameReserved(uint32 account_id, const char* input_character_name);
	bool	MoveCharacterToZone(const char* charname, const char* zonename);
	bool	MoveCharacterToZone(const char* charname, const char* zonename,uint32 zoneid);
	bool	MoveCharacterToZone(uint32 iCharID, const char* iZonename);
	bool	MoveCharacterToZone(const char* charname, uint32 zone_id);
	bool	MoveCharacterToZone(uint32 character_id, uint32 zone_id);
	uint16	MoveCharacterToBind(uint32 iCharID);
	
	bool	UpdateName(const char* oldname, const char* newname);
	bool	AddToNameFilter(std::string name);
	bool	ReserveName(uint32 account_id, char* name);
	bool	StoreCharacter(uint32 account_id, PlayerProfile_Struct* pp, EQ::InventoryProfile* inv);
	bool	DeleteCharacter(char* name);
	bool	MarkCharacterDeleted(char* name);
	bool	UnDeleteCharacter(const char* name);
	void	DeleteCharacterCorpses(uint32 charid);

	/* General Information Queries */

	bool	AddGMIP(char* ip_address, char* name);
	bool	CheckBannedIPs(std::string login_ip); //Check incoming connection against banned IP table.
	bool	CheckGMIPs(std::string login_ip, uint32 account_id);
	bool	CheckNameFilter(const char* name, bool surname = false);
	bool	CheckUsedName(std::string name, uint32 charid = 0);
	bool	NoRentExpired(const char* name);
	int		GetNumCharacters(uint32 account_id);

	uint32	GetAccountIDByChar(const char* charname, uint32* oCharID = 0);
	uint32	GetAccountIDByChar(uint32 char_id);
	uint32	GetAccountIDByName(std::string account_name, int16* status = 0, uint32* lsid = 0);
	uint32	GetCharacterID(const char* name);
	uint32	GetGuildIDByCharID(uint32 char_id);
	uint32  GetGroupIDByCharID(uint32 char_id);
	uint32	GetLevelByChar(const char* charname);

	void	GetAccountName(uint32 accountid, char* name, uint32* oLSAccountID = 0);
	void	GetCharName(uint32 char_id, char* name);
	uint32	GetCharacterInfo(const char* iName, uint32* oAccID = 0, uint32* oZoneID = 0, uint32* oGuildID = 0, float* oX = 0, float* oY = 0, float* oZ = 0, uint64* oDeathTime = 0);
	bool	AddBannedIP(std::string banned_ip, std::string notes); //Add IP address to the banned_ips table.
	std::string GetCharNameByID(uint32 char_id); 
	std::string GetNPCNameByID(uint32 npc_id);
	void	LoginIP(uint32 account_id, std::string loginIP);
	void	ClearAllActive();
	void	ClearAccountActive(uint32 AccountID);
	void	SetAccountActive(uint32 AccountID);
	bool AdjustPVPSpawnTimes();
	uint32	GetHardcoreStatus(const char* charname);

	/*
	* Account Related
	*/
	void	GetAccountFromID(uint32 id, char* oAccountName, int16* oStatus, int8* oRevoked = 0);
	uint32	CheckLogin(const char* name, const char* password, int16* oStatus = 0, int8* oRevoked = 0);
	int16	CheckStatus(uint32 account_id);
	uint8	CheckRevoked(uint32 account_id);
	int16	CheckExemption(uint32 account_id);
	uint32	CreateAccount(const char* name, const char* password, int16 status, uint32 lsaccount_id = 0);
	bool	DeleteAccount(const char* name);
	bool	SetAccountStatus(const char* name, int16 status);
	bool	SetAccountStatus(const std::string& account_name, int16 status);
	bool	SetLocalPassword(uint32 accid, const char* password);
	uint32	GetAccountIDFromLSID(uint32 iLSID, char* oAccountName = 0, int16* oStatus = 0, int8* oRevoked = 0, bool* isMule = nullptr);
	bool	UpdateLiveChar(char* charname,uint32 lsaccount_id);
	bool	GetLiveChar(uint32 account_id, char* cname);
	bool	GetLiveCharByLSID(uint32 ls_id, char* cname);
	bool	GetAccountRestriction(uint32 acctid, char(&forum_name)[31], uint16& expansion, bool& mule, uint32& force_guild);
	void	ClearAllConsented();
	void	ClearAllConsented(char* oname, uint32 corpse_id, LinkedList<ConsentDenied_Struct*>* purged);
	bool	SetIPExemption(const char* accountname, uint8 amount);
	bool	SetMule(const char* charname);
	bool	SetMule(const char* accountname, uint8 toggle);
	bool	SetExpansion(const char* accountname, uint8 toggle);
	bool SetForumName(uint32 account_id, const char* forum_name);
	std::string	GetForumNameByAccountName(const char* account_name, bool bReplaceSpaces);

	/*
	* Groups
	*/
	uint32	GetGroupID(const char* name);
	uint32  GetGroupIDByAccount(uint32 accountid, std::string& charname);
	void	SetGroupID(const char* name, uint32 id, uint32 charid, uint32 accountid);
	void	ClearGroup(uint32 gid = 0);
	char*	GetGroupLeaderForLogin(const char* name,char* leaderbuf);

	void	SetGroupLeaderName(uint32 gid, const char* name);
	void	SetGroupOldLeaderName(uint32 gid, const char* name);
	char*	GetGroupLeadershipInfo(uint32 gid, char* leaderbuf);
	std::string	GetGroupOldLeaderName(uint32 gid);
	void	ClearGroupLeader(uint32 gid = 0);
	bool	GetGroupMemberNames(uint32 group_id, char membername[MAX_GROUP_MEMBERS][64]);
	

	/*
	* Raids
	*/
	void	ClearRaid(uint32 rid = 0);
	void	ClearRaidDetails(uint32 rid = 0);
	uint32	GetRaidID(const char* name);
	bool	GetRaidGroupID(const char *name, uint32 *raidid, uint32 *groupid);
	const char *GetRaidLeaderName(uint32 rid);

	/*
	* Database Variables
	*/
	bool	GetVariable(std::string varname, std::string &varvalue);
	bool	SetVariable(const std::string& varname, const std::string &varvalue);
	bool	LoadVariables();

	/*
	* General Queries
	*/
	bool	LoadZoneNames();
	bool	LoadZoneFileNames();
	bool	GetZoneLongName(const char* short_name, char** long_name, char* file_name = 0, float* safe_x = 0, float* safe_y = 0, float* safe_z = 0, uint32* graveyard_id = 0, uint16* graveyard_time = 0, uint32* maxclients = 0);
	bool	GetZoneGraveyard(const uint32 graveyard_id, uint32* graveyard_zoneid = 0, float* graveyard_x = 0, float* graveyard_y = 0, float* graveyard_z = 0, float* graveyard_heading = 0);
	uint32	GetZoneGraveyardID(uint32 zone_id);
	uint32	GetZoneID(const char* zonename);
	uint16	GetGraveyardTime(uint16 zone_id);
	uint8	GetPEQZone(uint32 zoneID);
	uint8	GetMinStatus(uint32 zone_id);
	
	const char*	GetZoneName(uint32 zoneID, bool ErrorUnknown = false);
	uint32 GetClientZoneID(uint32 zoneID);
	const char* GetClientZoneName(const char* zone_name);
	
	uint8	GetServerType();
	bool	GetSafePoints(const char* short_name, float* safe_x = 0, float* safe_y = 0, float* safe_z = 0, float* safe_heading = 0, int16* minstatus = 0, uint8* minlevel = 0, char *flag_needed = nullptr, uint8* expansion = 0);
	uint8	GetRaceSkill(uint8 skillid, uint8 in_race);
	void	ClearMerchantTemp();
	void	ClearSayLink();
	bool	SetHackerFlag(const char* accountname, const char* charactername, const char* hacked);
	void	SetFirstLogon(uint32 CharID, uint8 firstlogon);
	void	AddReport(std::string who, std::string against, std::string lines);
	struct TimeOfDay_Struct		LoadTime(time_t &realtime);
	void LoadQuakeData(ServerEarthquakeImminent_Struct & earthquake_struct);
	bool LoadNextQuakeTime(ServerEarthquakeImminent_Struct &realtime);
	bool SaveNextQuakeTime(ServerEarthquakeImminent_Struct & earthquake_struct, QuakeType in_quake_type = QuakeType::QuakeNormal);
	bool	SaveTime(int8 minute, int8 hour, int8 day, int8 month, int16 year);
	bool	AdjustSpawnTimes();
	uint8   GetZoneRandomLoc(uint32 zoneid);
	void	PurgeAllDeletedDataBuckets();

private:

	std::map<uint32,std::string>	zonename_array;
	std::map<std::string, std::string>	zonename_filename_array;
	Mutex Mvarcache;
	VarCache_Struct varcache;

	/* Groups, utility methods. */
	void    ClearAllGroupLeaders();
	void    ClearAllGroups();

	/* Raid, utility methods. */
	void ClearAllRaids();
	void ClearAllRaidDetails();
};

#endif
