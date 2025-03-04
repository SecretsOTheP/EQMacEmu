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
#include "../common/zone_store.h"
#include "zonedb.h"
#include "../common/repositories/grid_repository.h"
#include "../common/repositories/grid_entries_repository.h"
#include "../common/repositories/loottable_repository.h"
#include "../common/repositories/loottable_entries_repository.h"
#include "../common/repositories/lootdrop_repository.h"
#include "../common/repositories/lootdrop_entries_repository.h"
#include "../common/repositories/zone_points_repository.h"
#include "../common/repositories/npc_faction_repository.h"
#include "../common/repositories/npc_faction_entries_repository.h"
#include "../common/repositories/skill_caps_repository.h"
#include "qglobals.h"
#include "spawn2.h"
#include "spawngroup.h"
#include "pathfinder_interface.h"
#include "position.h"
#include "global_loot_manager.h"
#include "queryserv.h"
#include "../common/discord/discord.h"

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
	bool   is_virtual;
	int    height;
	int    width;
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
	uint32  lsid;
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

struct SsfItemKey {
	uint32 item_id;
	uint32 character_id;
	SsfItemKey(uint32 _item_id, uint32 _character_id)
	{
		item_id = _item_id;
		character_id = _character_id;
	}
	bool operator==(const SsfItemKey& other) const {
		return (item_id == other.item_id && character_id == other.character_id);
	}
};

struct SsfItemKeyHash {
	uint64_t operator() (const SsfItemKey& key) const {
		const uint64_t item_id = key.item_id;
		const uint64_t char_id = key.character_id;
		return (item_id << 32) | char_id;
	}
};

typedef std::unordered_map<SsfItemKey, uint32, SsfItemKeyHash> MerchantSsfPurchaseLimits; // (item_id,ssf_character_id) -> purchase_limit

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
	static bool Bootup(uint32 iZoneID, bool is_static = false, uint32 iGuildID = 0xFFFFFFFF);
	static void Shutdown(bool quite = false);

	Zone(uint32 in_zoneid, const char* in_short_name, uint32 guild_id = 0xFFFFFFFF);
	~Zone();
	
	/* When zone has its own version of time */
	bool is_zone_time_localized;

	bool	Init(bool is_static);
	bool	LoadZoneCFG(const char* filename);
	bool	SaveZoneCFG();
	bool	IsLoaded();
	bool	IsPVPZone() { return pvpzone; }
	inline const char*	GetLongName()	{ return long_name; }
	inline const char*	GetFileName()	{ return file_name; }
	inline const char*	GetShortName()	{ return short_name; }
	inline const uint32	GetZoneID() const { return zoneid; }
	inline const uint32	GetGuildID() const { return guildid; }
	inline const uint8	GetZoneType() const { return zone_type; }

	inline const bool IsReducedSpawnTimersZone() { return reducedspawntimers;  }
	inline const bool IsTrivialLootCodeEnabled() { return trivial_loot_code; }
	inline bool IsReducedSpawnTimersEnabled();
    inline glm::vec4 GetSafePoint() { return m_safe_point; }
	inline const uint32& graveyard_zoneid()	{ return m_graveyard_zoneid; }
	inline glm::vec4 GetGraveyardPoint() { return m_graveyard; }
	inline const uint32& graveyard_id()	{ return m_graveyard_id; }
	inline const uint16& graveyard_timer() { return m_graveyard_timer;  }
	inline const uint32& GetMaxClients() { return m_max_clients; }
	void	LoadAlternateAdvancement();
	int		GetTotalAAs() { return totalAAs; }
	SendAA_Struct*	GetAABySequence(uint32 seq) { return aas[seq]; }
	SendAA_Struct*	FindAA(uint32 id, bool searchParent);
	uint8	EmuToEQMacAA(uint32 id);
	uint8	GetTotalAALevels(uint32 skill_id);
	void	LoadZoneDoors();
	bool	LoadZoneObjects();
	bool	LoadGroundSpawns();
	void	ReloadStaticData();

	void SetIsHotzone(bool is_hotzone);

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
	void	StartShutdownTimer(uint32 set_time = (RuleI(Zone, AutoShutdownDelay)));
	void	ResetShutdownTimer();
	void    StopShutdownTimer();
	void    ChangeWeather();
	bool	HasWeather();
	bool	IsSpecialWeatherZone();

	std::string GetZoneDescription();
	void SendReloadMessage(std::string reload_type);

	void	AddAuth(ServerZoneIncomingClient_Struct* szic);
	void	RemoveAuth(const char* iCharName, uint32 entity_id);
	void	RemoveAuth(const char* iCharName, const char* iLSKey);
	void	RemoveAuth(uint32 lsid);
	void	ResetAuth();
	bool	GetAuth(uint32 iIP, const char* iCharName, uint32* oWID = 0, uint32* oAccID = 0, uint32* oCharID = 0, int16* oStatus = 0, char* oLSKey = 0, bool* oTellsOff = 0, uint32* oVersionbit = 0, uint32 entity_id = 0);
	bool	CheckAuth(const char* iCharName);
	uint32	CountAuth();

	uint16	GetNumAggroedNPCs() { return aggroed_npcs; }
	void	SetNumAggroedNPCs(uint16 count) { aggroed_npcs = count; }
	void		SetStaticZone(bool sz)	{ staticzone = sz; }
	inline bool	IsStaticZone()			{ return staticzone; }
	inline void	SetZoneHasCurrentTime(bool time)	{ zone_has_current_time = time; }

	void	SpawnConditionChanged(const SpawnCondition &c, int16 old_value);
	void	UpdateGroupTimers(SpawnGroup* sg, uint32 newtimer);
	uint16	GetAvailPointCount(SpawnGroup* sg);
	uint16	GetGroupActiveTimers(uint32 spawngroupid, uint32& remaining_time);

	void	GetMerchantDataForZoneLoad();
	void	LoadNewMerchantData(uint32 merchantid);
	void	LoadTempMerchantData();
	uint32	GetTempMerchantQuantity(uint32 NPCID, uint32 Slot);
	int32	GetTempMerchantQtyNoSlot(uint32 NPCID, int16 itemid);
	int		SaveTempItem(uint32 merchantid, uint32 npcid, uint32 item, int32 charges, bool sold=false, uint32 self_found_character_id = 0);
	int		SaveReimbursementItem(std::list<TempMerchantList>& reimbursement_list, uint32 charid, uint32 item, int32 charges, bool sold = false);
	void	SaveMerchantItem(uint32 merchantid, int16 item, int8 charges, int8 slot);
	void	ResetMerchantQuantity(uint32 merchantid);
	uint32  GetSelfFoundPurchaseLimit(uint32 npctype_id, uint32 item, uint32 self_found_character_id);
	uint32  IncreaseSelfFoundPurchaseLimit(uint32 npctype_id, uint32 item, uint32 self_found_character_id, uint32 increment_by);
	uint32  DecreaseSelfFoundPurchaseLimit(uint32 npctype_id, uint32 item, uint32 self_found_character_id, uint32 decrement_by);
	void    CapSelfFoundPurchaseLimitsByAvailableQuantity(uint32 npctype_id, uint32 item, uint32 quantity_in_stock);
	void	ClearMerchantLists();

	uint8	GetZoneExpansion() { return newzone_data.expansion; }
	uint16	GetPullLimit();

	void	LoadLevelEXPMods();
	void	LoadSkillDifficulty();

	std::map<uint32,NPCType *> npctable;
	std::map<uint32,std::list<MerchantList> > merchanttable; //This may be dynamic if items are sold out (Crows' Brew)
	std::map<uint32,std::list<MerchantList> > db_merchanttable; //This is static and should never be changed. 
	std::map<uint32,std::list<TempMerchantList> > tmpmerchanttable;
	std::unordered_map<uint32, MerchantSsfPurchaseLimits> tmpmerchanttable_ssf_purchase_limits;
	std::map<uint32, ZoneEXPModInfo> level_exp_mod;
	std::map<uint32, SkillDifficulty> skill_difficulty;

	void ClearNPCEmotes(std::vector<NPC_Emote_Struct*>* NPCEmoteList);
	void LoadNPCEmotes(std::vector<NPC_Emote_Struct*>* NPCEmoteList);
	void LoadKeyRingData(LinkedList<KeyRing_Data_Struct*>* KeyRingDataList);
	void ReloadWorld(uint8 global_repop);
	void ClearSpawnTimers();
	bool IsQuestHotReloadQueued() const;
	void SetQuestHotReloadQueued(bool in_quest_hot_reload_queued);

	Map*	zonemap;
	WaterMap* watermap;
	IPathfinder *pathing;
	NewZone_Struct	newzone_data;

	SpawnConditionManager spawn_conditions;

	EQTime	zone_time;
	void	GetTimeSync();
	void	SetDate(uint16 year, uint8 month, uint8 day, uint8 hour, uint8 minute);
	void	SetTime(uint8 hour, uint8 minute, bool update_world = true);

	bool	process_mobs_while_empty;
	void	weatherSend(uint32 timer = 0);
	bool	CanBind() const { return(can_bind); }
	bool	IsCity() const { return(is_city); }
	bool	IsHotzone() const { return (is_hotzone); }
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
	Timer  hot_reload_timer;

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
	void		LoadZoneBlockedSpells();
	void		ClearBlockedSpells();
	bool		IsSpellBlocked(uint32 spell_id, const glm::vec3& location);
	const char *GetSpellBlockedMessage(uint32 spell_id, const glm::vec3& location);
	int			GetTotalBlockedSpells() { return zone_total_blocked_spells; }
	inline bool HasMap() { return zonemap != nullptr; }
	inline bool HasWaterMap() { return watermap != nullptr; }

	QGlobalCache *GetQGlobals() { return qGlobals; }
	QGlobalCache *CreateQGlobals() { qGlobals = new QGlobalCache(); return qGlobals; }
	void	UpdateQGlobal(uint32 qid, QGlobal newGlobal);
	void	DeleteQGlobal(std::string name, uint32 npcID, uint32 charID, uint32 zoneID);

	LinkedList<Spawn2*> spawn2_list;
	LinkedList<ZonePoint*> zone_point_list;
	std::vector<ZonePointsRepository::ZonePoints> virtual_zone_point_list;
	uint32	numzonepoints;
	float	update_range;

	std::vector<NPC_Emote_Struct*> npc_emote_list;
	LinkedList<KeyRing_Data_Struct*> key_ring_data_list;

	void LoadTickItems();
	void LoadGrids();
	uint32  GetSpawnKillCount(uint32 in_spawnid);
	std::unordered_map<int, item_tick_struct> tick_items;

	void ApplyRandomLoc(uint32 zoneid, float& x, float& y);

	bool CanClientEngage(Client * initiator, Mob * target);
	inline std::vector<int> GetGlobalLootTables(NPC *mob) const { return m_global_loot.GetGlobalLootTables(mob); }
	inline void AddGlobalLootEntry(GlobalLootEntry &in) { return m_global_loot.AddEntry(in); }
	inline void ShowZoneGlobalLoot(Client *to) { m_global_loot.ShowZoneGlobalLoot(to); }
	inline void ShowNPCGlobalLoot(Client *to, NPC *who) { m_global_loot.ShowNPCGlobalLoot(to, who); }

	void RequestUCSServerStatus();
	void SetUCSServerAvailable(bool ucss_available, uint32 update_timestamp);
	bool IsUCSServerAvailable() { return m_ucss_available; }

	// random object that provides random values for the zone
	EQ::Random random;

	static void GMSayHookCallBackProcess(uint16 log_category, const char *func, std::string message) {
		// we don't want to loop up with chat messages
		if (message.find("OP_SpecialMesg") != std::string::npos) {
			return;
		}

		/* Cut messages down to 2600 max to prevent client crash */
		if (!message.empty()) {
			message = message.substr(0, 2600);
		}

		/* Replace Occurrences of % or MessageStatus will crash */
		Strings::Replace(message, std::string("%"), std::string("."));

		if (message.find("\n") != std::string::npos){
			auto message_split = Strings::Split(message, '\n');
			entity_list.MessageStatus(
				0,
				AccountStatus::QuestTroupe,
				LogSys.GetGMSayColorFromCategory(log_category),
				message_split[0].c_str()
			);

			for (size_t iter = 1; iter < message_split.size(); ++iter) {
				entity_list.MessageStatus(
					0,
					AccountStatus::QuestTroupe,
					LogSys.GetGMSayColorFromCategory(log_category),
					fmt::format(
						"--- {}",
						message_split[iter]
					).c_str()
				);
			}
		}
		else{
			entity_list.MessageStatus(
				0,
				AccountStatus::QuestTroupe,
				LogSys.GetGMSayColorFromCategory(log_category),
				fmt::format("[{}] [{}] {}", Logs::LogCategoryName[log_category], func, message).c_str()
			);
		}
	}

	static void SendDiscordMessage(int webhook_id, const std::string& message);
	static void SendDiscordMessage(const std::string& webhook_name, const std::string& message);
	static void DiscordWebhookMessageHandler(uint16 log_category, int webhook_id, const std::string& message)
	{
		std::string message_prefix;
		if (!LogSys.origination_info.zone_short_name.empty()) {
			message_prefix = fmt::format(
				"[**{}**] **Zone** [**{}**] ",
				Logs::LogCategoryName[log_category],
				LogSys.origination_info.zone_short_name
			);
		}

		SendDiscordMessage(webhook_id, message_prefix + Discord::FormatDiscordMessage(log_category, message));
	};


	bool	idle;

	void	NexusProcess();
	uint8	velious_timer_step;
	uint8	nexus_timer_step;
	bool	IsNexusScionZone();
	bool	velious_active;	

	bool	HasCharmedNPC;
	bool	quest_hot_reload_queued;

	// loot
	void LoadLootTable(const uint32 loottable_id);
	void LoadLootTables(const std::vector<uint32> in_loottable_ids);
	void ClearLootTables();
	void ReloadLootTables();
	LoottableRepository::Loottable *GetLootTable(const uint32 loottable_id);
	std::vector<LoottableEntriesRepository::LoottableEntries> GetLootTableEntries(const uint32 loottable_id) const;
	LootdropRepository::Lootdrop GetLootdrop(const uint32 lootdrop_id) const;
	std::vector<LootdropEntriesRepository::LootdropEntries> GetLootdropEntries(const uint32 lootdrop_id) const;

	void LoadNPCFaction(const uint32 npc_faction_id);
	void LoadNPCFactions(const std::vector<uint32>& npc_faction_ids);
	void ClearNPCFactions();
	void ReloadNPCFactions();
	NpcFactionRepository::NpcFaction* GetNPCFaction(const uint32 npc_faction_id);
	std::vector<NpcFactionEntriesRepository::NpcFactionEntries> GetNPCFactionEntries(const uint32 npc_faction_id) const;

private:
	uint32	zoneid;
	uint32	guildid;
	char*	short_name;
	char	file_name[16];
	char*	long_name;
	char*	map_name;
	bool pvpzone;
	glm::vec4 m_safe_point;
	uint32	m_max_clients;
	bool	can_bind;
	bool	is_city;
	bool    is_hotzone;
	bool	can_bind_others; //Zone is not a city, but has areas where others can be bound.
	bool	can_combat;
	bool	can_castoutdoor;
	bool	can_castdungeon;
	bool	can_levitate;
	bool	skip_los; // Zone does not do a LOS spell check.
	bool	drag_aggro;
	uint8	zone_type;
	uint32	m_graveyard_id, m_graveyard_zoneid;
	uint16	m_graveyard_timer;
	glm::vec4 m_graveyard;
	int		default_ruleset;

	int	zone_total_blocked_spells;
	ZoneSpellsBlocked *blocked_spells;

	ZoneBanishPoint zone_banish_point;

	int		totalAAs;
	SendAA_Struct **aas;	//array of AA structs

	uint16	aggroed_npcs;
	uint16	pull_limit;

	bool	reducedspawntimers;

	bool	trivial_loot_code;

	bool	staticzone;
	bool	zone_has_current_time;

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

	GlobalLootManager m_global_loot;

	bool m_ucss_available;
	uint32 m_last_ucss_update;

	// loot
	std::vector<LoottableRepository::Loottable>               m_loottables = {};
	std::vector<LoottableEntriesRepository::LoottableEntries> m_loottable_entries = {};
	std::vector<LootdropRepository::Lootdrop>                 m_lootdrops = {};
	std::vector<LootdropEntriesRepository::LootdropEntries>   m_lootdrop_entries = {};

	// Factions
	std::vector<NpcFactionRepository::NpcFaction>                 m_npc_factions = { };
	std::vector<NpcFactionEntriesRepository::NpcFactionEntries>   m_npc_faction_entries = { };

};

#endif