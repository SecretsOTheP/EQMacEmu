/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemu.org)

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
#ifndef ZONE_H
#define ZONE_H

#include "../common/eqtime.h"
#include "../common/linked_list.h"
#include "../common/random.h"
#include "../common/rulesys.h"
#include "../common/types.h"
#include "../common/strings.h"
#include "zonedb.h"
#include "../common/repositories/grid_repository.h"
#include "../common/repositories/grid_entries_repository.h"
#include "qglobals.h"
#include "spawn2.h"
#include "spawngroup.h"
#include "pathfinder_interface.h"
#include "position.h"

extern uint32 numclients;

struct ZonePoint
{
	float x;
	float y;
	float z;
	float heading;
	uint16 number;
	float target_x;
	float target_y;
	float target_z;
	float target_heading;
	uint16 target_zone_id;
	uint32 client_version_mask;
};

struct ZoneBanishPoint
{
	float x;
	float y;
	float z;
	float heading;
	uint16 target_zone_id;
};

struct ZoneClientAuth_Struct {
	uint32	ip;			// client's IP address
	uint32	wid;		// client's WorldID#
	uint32	accid;
	int16	admin;
	uint32	charid;
	bool	tellsoff;
	char	charname[64];
	char	lskey[30];
	bool	stale;
	uint32	version;
	uint32	entity_id;
};

struct ZoneEXPModInfo {
	float ExpMod;
	float AAExpMod;
};

struct SkillDifficulty {
	float	difficulty;
	char	name[32];
};

struct item_tick_struct {
    uint32       itemid;
    uint32       chance;
    uint32       level;
    int16        bagslot;
    std::string qglobal;
};

class Client;
class Map;
class Mob;
class PathManager;
class WaterMap;
extern EntityList entity_list;
struct NPCType;
struct ServerZoneIncomingClient_Struct;
class MobMovementManager;

class Zone
{
public:
	static bool Bootup(uint32 iZoneID, bool iStaticZone = false, uint32 iGuildID = 0xFFFFFFFF);
	static void Shutdown(bool quite = false);

	Zone(uint32 in_zoneid, const char* in_short_name, uint32 guild_id = 0xFFFFFFFF);
	~Zone();
	bool	Init(bool iStaticZone);
	bool	LoadZoneCFG(const char* filename, bool DontLoadDefault = false);
	bool	SaveZoneCFG();
	bool	IsLoaded();
	bool	IsPVPZone() { return pvpzone; }
	inline const char*	GetLongName()	{ return long_name; }
	inline const char*	GetFileName()	{ return file_name; }
	inline const char*	GetShortName()	{ return short_name; }
	inline const uint32	GetZoneID() const { return zoneid; }
	inline const uint32	GetGuildID() const { return guildid; }
	inline const uint8	GetZoneType() const { return zone_type; }

    inline glm::vec4 GetSafePoint() { return m_SafePoint; }
	inline const uint32& graveyard_zoneid()	{ return pgraveyard_zoneid; }
	inline glm::vec4 GetGraveyardPoint() { return m_Graveyard; }
	inline const uint32& graveyard_id()	{ return pgraveyard_id; }
	inline const uint16& graveyard_timer() { return pgraveyard_timer;  }
	inline const uint32& GetMaxClients() { return pMaxClients; }

	inline const bool IsReducedSpawnTimersZone() { return reducedspawntimers;  }
	inline const bool IsTrivialLootCodeEnabled() { return trivial_loot_code; }
	inline bool IsReducedSpawnTimersEnabled();
	void	LoadAAs();
	int		GetTotalAAs() { return totalAAs; }
	SendAA_Struct*	GetAABySequence(uint32 seq) { return aas[seq]; }
	SendAA_Struct*	FindAA(uint32 id, bool searchParent);
	uint8	EmuToEQMacAA(uint32 id);
	uint8	GetTotalAALevels(uint32 skill_id);
	void	LoadZoneDoors(const char* zone);
	bool	LoadZoneObjects();
	bool	LoadGroundSpawns();
	void	ReloadStaticData();

	uint32	CountSpawn2();
	ZonePoint* GetClosestZonePoint(const glm::vec3& location, const char* to_name, Client *client, float max_distance = 40000.0f);
	ZonePoint* GetClosestZonePoint(const glm::vec3& location, uint32	to, Client *client, float max_distance = 40000.0f);
	ZonePoint* GetClosestZonePointWithoutZone(float x, float y, float z, Client *client, float max_distance = 40000.0f);
	ZonePoint* GetClosestTargetZonePointSameZone(float x, float y, float z, Client *client, float max_distance = 40000.0f);
	ZonePoint* GetClosestZonePointSameZone(float x, float y, float z, Client *client, float max_distance = 40000.0f);
	SpawnGroupList spawn_group_list;

	Timer GetInitgridsTimer();

	bool AllowManastoneClick();

	bool RemoveSpawnEntry(uint32 spawnid);
	bool RemoveSpawnGroup(uint32 in_id);

	ZoneBanishPoint& GetZoneBanishPoint() { return zone_banish_point; }

	bool	Process();
	void	Despawn(uint32 spawngroupID);
	bool	ResetEngageNotificationTargets(uint32 in_respawn_timer, bool update_respawn_in_db = false);
	bool	Depop(bool StartSpawnTimer = false);
	void	Repop();
	void	RepopClose(const glm::vec4& client_position, uint32 repop_distance);
	void	ClearNPCTypeCache(int id);
	void	SpawnStatus(Mob* client, char filter = 'a', uint32 spawnid = 0);
	void	StartShutdownTimer(uint32 set_time = 0);
	void    ChangeWeather();
	bool	HasWeather();
	bool	IsSpecialWeatherZone();
	void	AddAuth(ServerZoneIncomingClient_Struct* szic);
	void	RemoveAuth(const char* iCharName, uint32 entity_id);
	void	ResetAuth();
	bool	GetAuth(uint32 iIP, const char* iCharName, uint32* oWID = 0, uint32* oAccID = 0, uint32* oCharID = 0, int16* oStatus = 0, char* oLSKey = 0, bool* oTellsOff = 0, uint32* oVersionbit = 0, uint32 entity_id = 0);
	bool	CheckAuth(const char* iCharName);
	uint32	CountAuth();

	uint16	GetNumAggroedNPCs() { return aggroed_npcs; }
	void	SetNumAggroedNPCs(uint16 count) { aggroed_npcs = count; }
	void		SetStaticZone(bool sz)	{ staticzone = sz; }
	inline bool	IsStaticZone()			{ return staticzone; }
	inline void	GotCurTime(bool time)	{ gottime = time; }

	void	SpawnConditionChanged(const SpawnCondition &c, int16 old_value);
	void	UpdateGroupTimers(SpawnGroup* sg, uint32 newtimer);
	uint16	GetAvailPointCount(SpawnGroup* sg);
	uint16	GetGroupActiveTimers(uint32 spawngroupid, uint32& remaining_time);

	void	GetMerchantDataForZoneLoad();
	void	LoadNewMerchantData(uint32 merchantid);
	void	LoadTempMerchantData();
	uint32	GetTempMerchantQuantity(uint32 NPCID, uint32 Slot);
	int32	GetTempMerchantQtyNoSlot(uint32 NPCID, int16 itemid);
	int		SaveTempItem(uint32 merchantid, uint32 npcid, uint32 item, int32 charges, bool sold=false);
	int		SaveReimbursementItem(std::list<TempMerchantList>& reimbursement_list, uint32 charid, uint32 item, int32 charges, bool sold = false);
	void	SaveMerchantItem(uint32 merchantid, int16 item, int8 charges, int8 slot);
	void	ResetMerchantQuantity(uint32 merchantid);
	void	ClearMerchantLists();

	uint8	GetZoneExpansion() { return newzone_data.expansion; }
	uint16	GetPullLimit();

	void	LoadLevelEXPMods();
	void	LoadSkillDifficulty();

	std::map<uint32,NPCType *> npctable;
	std::map<uint32,std::list<MerchantList> > merchanttable; //This may be dynamic if items are sold out (Crows' Brew)
	std::map<uint32,std::list<MerchantList> > db_merchanttable; //This is static and should never be changed. 
	std::map<uint32,std::list<TempMerchantList> > tmpmerchanttable;
	std::map<uint32, ZoneEXPModInfo> level_exp_mod;
	std::map<uint32, SkillDifficulty> skill_difficulty;

	void	ClearNPCEmotes(std::vector<NPC_Emote_Struct*>* NPCEmoteList);
	void	LoadNPCEmotes(std::vector<NPC_Emote_Struct*>* NPCEmoteList);
	void	LoadKeyRingData(LinkedList<KeyRing_Data_Struct*>* KeyRingDataList);
	void	ReloadWorld(uint32 Option);

	Map*	zonemap;
	WaterMap* watermap;
	IPathfinder *pathing;
	NewZone_Struct	newzone_data;

	SpawnConditionManager spawn_conditions;

	EQTime	zone_time;
	void	GetTimeSync();
	void	SetDate(uint16 year, uint8 month, uint8 day, uint8 hour, uint8 minute);
	void	SetTime(uint8 hour, uint8 minute);

	bool	process_mobs_while_empty;
	void	weatherSend(uint32 timer = 0);
	bool	CanBind() const { return(can_bind); }
	bool	IsCity() const { return(is_city); }
	bool	CanBindOthers() const { return(can_bind_others); }
	bool	CanDoCombat() const { return(can_combat); }
	bool	CanDoCombat(Mob* current, Mob* other, bool process = false);
	bool	CanLevitate() const {return(can_levitate); } // Magoth78
	bool	CanCastOutdoor() const {return(can_castoutdoor);} //qadar
	bool	CanCastDungeon() const { return(can_castdungeon); }
	bool	DragAggro() const {return(drag_aggro);}
	bool	IsBoatZone();
	bool	IsBindArea(float x_coord, float y_coord, float z_coord);
	bool	SkipLoS() const { return(skip_los); }
	bool	IsWaterZone(float z);
	bool	ZoneWillNotIdle() { return newzone_data.never_idle; };
	bool	IsIdling() { return (idle || (numclients <= 0 && ZoneWillNotIdle())); };
	inline	bool BuffTimersSuspended() const { return newzone_data.SuspendBuffs != 0; };

	std::vector<GridRepository::Grid> grids;
	std::vector<GridEntriesRepository::GridEntries> grid_entries;

	time_t	weather_timer;
	uint8	weather_intensity;
	uint8	zone_weather;

	ServerEarthquakeImminent_Struct last_quake_struct;
	Timer*	EndQuake_Timer;

	uint8 loglevelvar;
	uint8 merchantvar;
	uint8 tradevar;
	uint8 lootvar;

	bool	HasGraveyard();
	void	SetGraveyard(uint32 zoneid, const glm::vec4& graveyardPosition);

	void		LoadZoneBanishPoint(const char* zone);
	void		LoadBlockedSpells(uint32 zoneid);
	void		ClearBlockedSpells();
	bool		IsSpellBlocked(uint32 spell_id, const glm::vec3& location);
	const char *GetSpellBlockedMessage(uint32 spell_id, const glm::vec3& location);
	int			GetTotalBlockedSpells() { return totalBS; }
	inline bool HasMap() { return zonemap != nullptr; }
	inline bool HasWaterMap() { return watermap != nullptr; }

	QGlobalCache *GetQGlobals() { return qGlobals; }
	QGlobalCache *CreateQGlobals() { qGlobals = new QGlobalCache(); return qGlobals; }
	void	UpdateQGlobal(uint32 qid, QGlobal newGlobal);
	void	DeleteQGlobal(std::string name, uint32 npcID, uint32 charID, uint32 zoneID);

	LinkedList<Spawn2*> spawn2_list;
	LinkedList<ZonePoint*> zone_point_list;
	uint32	numzonepoints;
	float	update_range;

	std::vector<NPC_Emote_Struct*> NPCEmoteList;
	LinkedList<KeyRing_Data_Struct*> KeyRingDataList;

	void LoadTickItems();
	void LoadGrids();
	uint32  GetSpawnKillCount(uint32 in_spawnid);
	std::unordered_map<int, item_tick_struct> tick_items;

	void ApplyRandomLoc(uint32 zoneid, float& x, float& y);

	bool CanClientEngage(Client * initiator, Mob * target);

	// random object that provides random values for the zone
	EQ::Random random;

	static void GMSayHookCallBackProcess(uint16 log_category, std::string message){
		/* Cut messages down to 2600 max to prevent client crash */
		if (!message.empty())
			message = message.substr(0, 2600);

		/* Replace Occurrences of % or MessageStatus will crash */
		Strings::Replace(message, std::string("%"), std::string("."));

		if (message.find("\n") != std::string::npos){
			auto message_split = Strings::Split(message, '\n');
			entity_list.MessageStatus(0, 80, LogSys.GetGMSayColorFromCategory(log_category), "%s", message_split[0].c_str());
			for (size_t iter = 1; iter < message_split.size(); ++iter) {
				entity_list.MessageStatus(0, 80, LogSys.GetGMSayColorFromCategory(log_category), "--- %s", message_split[iter].c_str());
			}
		}
		else{
			entity_list.MessageStatus(0, 80, LogSys.GetGMSayColorFromCategory(log_category), "%s", message.c_str());
		}
	}

	bool	idle;

	void	NexusProcess();
	uint8	velious_timer_step;
	uint8	nexus_timer_step;
	bool	IsNexusScionZone();
	bool	velious_active;	

	bool	HasCharmedNPC;

private:
	uint32	zoneid;
	uint32	guildid;
	char*	short_name;
	char	file_name[16];
	char*	long_name;
	char*	map_name;
	bool pvpzone;
	glm::vec4 m_SafePoint;
	uint32	pMaxClients;
	bool	can_bind;
	bool	is_city;
	bool	can_bind_others; //Zone is not a city, but has areas where others can be bound.
	bool	can_combat;
	bool	can_castoutdoor;
	bool	can_castdungeon;
	bool	can_levitate;
	bool	skip_los; // Zone does not do a LOS spell check.
	bool	drag_aggro;
	uint8	zone_type;
	uint32	pgraveyard_id, pgraveyard_zoneid;
	uint16	pgraveyard_timer;
	glm::vec4 m_Graveyard;
	int		default_ruleset;

	int	totalBS;
	ZoneSpellsBlocked *blocked_spells;

	ZoneBanishPoint zone_banish_point;

	int		totalAAs;
	SendAA_Struct **aas;	//array of AA structs

	uint16	aggroed_npcs;
	uint16	pull_limit;

	bool	reducedspawntimers;

	bool	trivial_loot_code;

	bool	staticzone;
	bool	gottime;

	uint32 pQueuedMerchantsWorkID;
	uint32 pQueuedTempMerchantsWorkID;

	Timer	autoshutdown_timer;
	Timer	clientauth_timer;
	Timer	initgrids_timer;
	Timer	spawn2_timer;
	Timer	qglobal_purge_timer;
	Timer*	Weather_Timer;
	Timer*	Nexus_Portal_Timer;
	Timer*	Nexus_Scion_Timer; //Also used for Velious in Nexus
	LinkedList<ZoneClientAuth_Struct*> client_auth_list;
	QGlobalCache *qGlobals;
	MobMovementManager* mMovementManager;

};

#endif

