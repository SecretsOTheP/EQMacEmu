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
#ifndef NPC_H
#define NPC_H

#include "../common/rulesys.h"

#include "mob.h"
#include "qglobals.h"
#include "zonedb.h"
#include "zonedump.h"
#include "../common/repositories/loottable_repository.h"
#include "../common/repositories/loottable_entries_repository.h"
#include "../common/repositories/lootdrop_repository.h"
#include "../common/repositories/lootdrop_entries_repository.h"

#include <deque>
#include <list>

typedef struct {
	float min_x;
	float max_x;
	float min_y;
	float max_y;
	float min_z;
	float max_z;
	bool say;
} NPCProximity;

struct AISpells_Struct {
	uint16	type;			// 0 = never, must be one (and only one) of the defined values
	uint16	spellid;		// <= 0 = no spell
	int16	manacost;		// -1 = use spdat, -2 = no cast time
	uint32	time_cancast;	// when we can cast this spell next
	int32	recast_delay;
	int16	priority;
	int16	resist_adjust;
};

struct AISpellsEffects_Struct {
	uint16	spelleffectid;
	int32	base;
	int32	limit;
	int32	max;
};

struct AISpellsVar_Struct {
	uint32  fail_recast;
	uint32	engaged_no_sp_recast_min;
	uint32	engaged_no_sp_recast_max;
	uint8	engaged_beneficial_self_chance;
	uint8	engaged_beneficial_other_chance;
	uint8	engaged_detrimental_chance;
	uint32  idle_no_sp_recast_min;
	uint32  idle_no_sp_recast_max;
	uint8	idle_beneficial_chance;
};

struct DeathCreditDamage
{
	Mob* ent;
	int32 damage;
};

class AA_SwarmPetInfo;
class SwarmPet;
class Client;
class Group;
class Raid;
class Spawn2;
struct ItemData;

class NPC : public Mob
{
public:
	static NPC* SpawnNPC(const char* spawncommand, const glm::vec4& position, Client* client = nullptr);

	NPC(const NPCType *npc_type_data, Spawn2* respawn, const glm::vec4& position, GravityBehavior iflymode, bool IsCorpse = false);

	virtual ~NPC();

	static void SpawnGridNodeNPC(const glm::vec4& position, int32 grid_number, int32 zoffset);

	//abstract virtual function implementations required by base abstract class
	virtual bool Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby = 0, bool bufftic = false);
	void IdleDeath(Mob* killer);
	virtual void Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false);
	virtual bool Attack(Mob* other, int hand = EQ::invslot::slotPrimary, int damagePct = 100);
	virtual bool HasRaid() { return false; }
	virtual bool HasGroup() { return false; }
	virtual Raid* GetRaid() { return 0; }
	virtual Group* GetGroup() { return 0; }
	virtual uint8 Disarm(float chance);

	virtual bool IsNPC() const { return true; }

	virtual bool Process();
	virtual void	AI_Init();
	virtual void	AI_Start();
	virtual void	AI_Stop();
	void			AI_DoMovement();
	void			AI_SetupNextWaypoint();
	bool			AI_AddNPCSpells(uint32 iDBSpellsID);
	bool			AI_AddNPCSpellsEffects(uint32 iDBSpellsEffectsID);
	virtual bool	AI_EngagedCastCheck();
	bool			AI_HasSpells() { return HasAISpell; }
	bool			AI_HasSpellsEffects() { return HasAISpellEffects; }
	void			ApplyAISpellEffects(StatBonuses* newbon);
	void			CallForHelp(Mob* attacker = nullptr, bool ignoreTimer = false);
	bool			IsAssisting() { return assisting; }	// if this is true, NPC will not call for help.  sets to false if NPC gets any hate
	void			SetAssisting(bool value) { assisting = value; }

	virtual bool	AI_IdleCastCheck();
	virtual void	AI_Event_SpellCastFinished(bool iCastSucceeded, uint16 slot);
	void			TriggerAutoCastTimer() { if (!IsCasting() && AIautocastspell_timer) AIautocastspell_timer->Trigger(); }

	void LevelScale();
	void CalcNPCResists();
	void CalcNPCRegen();
	void CalcNPCDamage();

	int32 GetActSpellDamage(uint16 spell_id, int32 value, Mob* target = nullptr);
	int32 GetActSpellHealing(uint16 spell_id, int32 value, Mob* target = nullptr, bool hot = false);

	virtual void SetTarget(Mob* mob);
	virtual uint16 GetSkill(EQ::skills::SkillType skill_num) const { if (skill_num <= EQ::skills::HIGHEST_SKILL) { return skills[skill_num]; } return 0; }
	void SetSkill(EQ::skills::SkillType skill_num, uint16 value);

	void CalcItemBonuses(StatBonuses *newbon);
	virtual void CalcBonuses();
	virtual int GetMaxBuffSlots() const { return RuleI(Spells, MaxBuffSlotsNPC); }
	virtual int GetMaxTotalSlots() const { return RuleI(Spells, MaxTotalSlotsNPC); }
	virtual int GetPetMaxTotalSlots() const { return RuleI(Spells, MaxTotalSlotsPET); }
	virtual void InitializeBuffSlots();
	virtual void UninitializeBuffSlots();

	virtual void	SetAttackTimer(bool trigger = false);
	void	DisplayAttackTimer(Client* sender);
	virtual void	RangedAttack(Mob* other);
	virtual void	ThrowingAttack(Mob* other) { }
	virtual bool	CombatRange(Mob* other, float dist_squared = 0.0f, bool check_z = false, bool pseudo_pos = false);
	virtual float	GetSpellRange(uint16 spell_id, float range) { return range * 1.2f; }		// NPCs get a range bonus.  Found in decompile

	int32 GetNumberOfAttacks() const { return attack_count; }

	bool	DatabaseCastAccepted(int spell_id);
	bool	IsFactionListAlly(uint32 other_faction);
	FACTION_VALUE CheckNPCFactionAlly(int32 other_faction);
	virtual FACTION_VALUE GetReverseFactionCon(Mob* iOther);
	void	DescribeAggro(Client *towho, Mob *mob, bool verbose);

	void	GoToBind(uint8 bindnum = 0)	{ GMMove(m_SpawnPoint.x, m_SpawnPoint.y, m_SpawnPoint.z, m_SpawnPoint.w); }
	void	Gate();

	void	GetPetState(SpellBuff_Struct *buffs, uint32 *items, char *name);
	void	SetPetState(SpellBuff_Struct *buffs, uint32 *items);
	virtual void SpellProcess();
	virtual void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho);

	//loot
	void AddItem(const EQ::ItemData *item, int8 charges, bool equip_item = true, bool quest = false,  bool pet = false, bool force_equip = false, uint8 min_looter_level = 0, uint32 item_loot_lockout_timer = 0);
	void AddItem(uint32 itemid, int8 charges, bool equipitem = true, bool quest = false);
	void AddLootTable();
	void AddLootTable(uint32 loottable_id, bool is_global = false);
	void AddLootDropTable(uint32 lootdrop_id, uint8 drop_limit, uint8 min_drop);
	void CheckGlobalLootTables();
	void RemoveItem(LootItem *item_data, uint8 quantity = 0);
	void 	ClearLootItems();
	inline const LootItems &GetLootItems() { return m_loot_items; }
	LootItem *GetItem(int slot_id, int16 itemid = 0);
	LootItem *GetItemByID(int16 itemid);
	void	AddLootCash(uint16 in_copper, uint16 in_silver, uint16 in_gold, uint16 in_platinum);
	void	AddLootCash();
	void	RemoveLootCash();
	void	QueryLoot(Client *to);
	
	uint32	CountLoot();
	bool	MoveItemToGeneralInventory(LootItem *item);
	void	CheckMinMaxLevel(Mob *them);
	bool	HasQuestLootItem(int16 itemid);
	bool	HasPetLootItem(int16 itemid);
	bool	HasQuestLoot(); 
	bool	RemoveQuestLootItems(int16 itemid);
	bool	RemovePetLootItems(int16 itemid);
	bool	HasRequiredQuestLoot(int16 itemid1, int16 itemid2, int16 itemid3, int16 itemid4);
	void	CleanQuestLootItems();
	uint8	CountQuestItem(uint16 itemid);
	uint8	CountQuestItems();
	bool	AddQuestLoot(int16 itemid, int8 charges = 1);
	bool	AddPetLoot(int16 itemid, int8 charges = 1, bool fromquest = false);
	void	DeleteQuestLoot(int16 itemid1, int16 itemid2 = 0, int16 itemid3 = 0, int16 itemid4 = 0);
	void	DeleteInvalidQuestLoot();

	inline uint32 GetLoottableID()	const { return m_loottable_id; }
	inline bool DropsGlobalLoot() const { return !m_skip_global_loot; }
	inline uint32 GetCopper() const { return m_loot_copper; }
	inline uint32 GetSilver() const { return m_loot_silver; }
	inline uint32 GetGold() const { return m_loot_gold; }
	inline uint32 GetPlatinum() const { return m_loot_platinum; }
	inline void SetCopper(uint32 amt) { m_loot_copper = amt; }
	inline void SetSilver(uint32 amt) { m_loot_silver = amt; }
	inline void SetGold(uint32 amt) { m_loot_gold = amt; }
	inline void SetPlatinum(uint32 amt) { m_loot_platinum = amt; }

	void AddLootDrop(
		const EQ::ItemData *item2, 
		LootdropEntriesRepository::LootdropEntries loot_drop, 
		bool equipit = false,
		bool wearchange = false, 
		bool quest = false, 
		bool pet = false, 
		bool force_equip = false
	);

	void	DeleteEquipment(int16 slotid);
	virtual void UpdateEquipmentLight();
	uint32	GetEquipment(uint8 material_slot) const;	// returns item id
	int32	GetEquipmentMaterial(uint8 material_slot) const;

	virtual int32 CalcMaxMana();
	void SetGrid(int32 grid_){ grid=grid_; }
	void SetSp2(uint32 sg2){ spawn_group=sg2; }
	void SetSaveWaypoint(uint16 wp_){ save_wp=wp_; }

	int32 GetGrid() const { return grid; }
	uint32 GetSp2() const { return spawn_group; }
	uint32 GetSpawnPointID() const;

	void ClearPathing();

	glm::vec4 const GetSpawnPoint() const { return m_SpawnPoint; }
	glm::vec4 const GetGuardPoint() const { return m_GuardPoint; }
	EmuAppearance GetGuardPointAnim() const { return guard_anim; }
	void SaveGuardPointAnim(EmuAppearance anim) { guard_anim = anim; }

	uint8 GetPrimSkill()	const { return prim_melee_type; }
	uint8 GetSecSkill()	const { return sec_melee_type; }
	uint8 GetRangedSkill() const { return ranged_type; }
	void SetPrimSkill(uint8 skill_type)	{ prim_melee_type = skill_type; }
	void SetSecSkill(uint8 skill_type)	{ sec_melee_type = skill_type; }
	void SetRangedSkill(uint8 skill_type)	{ ranged_type = skill_type; }

	uint32	MerchantType;
	bool	merchant_open;
	inline void	MerchantOpenShop() { merchant_open = true; }
	inline void	MerchantCloseShop() { entity_list.SendMerchantEnd(this); merchant_open = false; }
	inline bool	IsMerchantOpen() { return merchant_open; }
	void	Depop(bool StartSpawnTimer = false);
	void	ForceRepop();
	void	Stun(int duration, Mob* attacker);
	void	UnStun();
	uint32	GetSwarmOwner();
	uint32	GetSwarmTarget();
	void	SetSwarmTarget(int target_id = 0);
	void	DepopSwarmPets();

	void	SignalNPC(int _signal_id, const char* data = nullptr);

	inline int32	GetNPCFactionID()	const { return npc_faction_id; }
	inline int32	GetPreCharmNPCFactionID()	const { return precharm_npc_faction_id; }
	inline int32	GetPrimaryFaction()	const { return primary_faction; }
	void	AddHate(Mob* other, int32 hate = 0, int32 damage = 0, bool bFrenzy = false, bool iAddIfNotExist = true) { hate_list.Add(other, hate, damage, bFrenzy, iAddIfNotExist); SetAssisting(false); }
	int32	GetNPCHate(Mob* in_ent, bool includeBonus = true) {return hate_list.GetEntHate(in_ent, includeBonus);}
	bool	IsOnHatelist(Mob*p) { return hate_list.IsOnHateList(p);}
	void	SetRememberDistantMobs(bool state) { hate_list.SetRememberDistantMobs(state); }

	void	SetNPCFactionID(int32 in) { npc_faction_id = in; database.GetFactionIdsForNPC(npc_faction_id, &faction_list, &primary_faction); }
	void	SetPreCharmNPCFactionID(int32 in) { precharm_npc_faction_id = in; }
	void	RestoreNPCFactionID() { npc_faction_id = precharm_npc_faction_id; database.GetFactionIdsForNPC(npc_faction_id, &faction_list, &primary_faction); }

    glm::vec4 m_SpawnPoint;

	uint32	GetMaxDMG() const {return max_dmg;}
	uint32	GetMinDMG() const {return min_dmg;}
	int		GetDamageBonus();
	int		GetBaseDamage(Mob* defender = nullptr, int slot = EQ::invslot::slotPrimary);
	inline void	TriggerClassAtkTimer() { classattack_timer.Trigger(); }
	int16	GetSlowMitigation() const {return slow_mitigation;}
	bool	IsAnimal() const { return(bodytype == BT_Animal); }
	uint16	GetPetSpellID() const {return pet_spell_id;}
	void	SetPetSpellID(uint16 amt) {pet_spell_id = amt;}
	uint32	GetMaxDamage(uint8 tlevel);
	void	SetTaunting(bool tog) {taunting = tog;}
	bool	IsTaunting() { return taunting; }
	void	PickPocket(Client* thief);
	void	StartSwarmTimer(uint32 duration) { swarm_timer.Start(duration); }
	virtual void DoClassAttacks(Mob *target);
	void	DoBackstab(Mob* defender = nullptr);
	void	CheckSignal();
	int32 GetNPCHPRegen() const { return hp_regen + itembonuses.HPRegen + spellbonuses.HPRegen; }
	int32 GetNPCManaRegen() const { return mana_regen + itembonuses.ManaRegen + spellbonuses.ManaRegen; }
	int32 GetHPRegen();
	int32 GetManaRegen();
	uint16	GetInnateProcSpellId() const { return innateProcSpellId;  }
	void	AddPush(float heading, float magnitude);		// adds push to the push vector; call this for every melee hit
	float	ApplyPushVector(bool noglance = false);			// actually push the mob and reset the push vector. checks map collision
	bool	CheckPushTimer() { return push_timer.Check(); }
	void	TriggerPushTimer() { push_timer.Trigger(); }
	void	ResetPushVector() { push_vector = glm::vec3(0.0f); }
	bool	IsCornered() { return corner_x || corner_y; }
	void	SetNotCornered() { wall_normal1_x = wall_normal1_y = wall_normal2_x = wall_normal2_y = corner_x = corner_y = 0.0f; }
	float	GetWallAngle1(float x, float y) { return wall_normal1_x * x + wall_normal1_y * y; };
	float	GetWallAngle2(float x, float y) { return wall_normal2_x * x + wall_normal2_y * y; };
	bool	IsWalled() { if (wall_normal1_x || wall_normal1_y || wall_normal2_x || wall_normal2_y) return true; else return false; };
	void	SetBaseHP(uint32 new_hp) { base_hp = new_hp; CalcMaxHP(); }
	void	SetMaxDamage(uint32 new_max_damage) { max_dmg = new_max_damage; }
	void	SetMinDamage(uint32 new_min_damage) { min_dmg = new_min_damage; }

	//waypoint crap
	int					GetMaxWp() const { return max_wp; }
	void				DisplayWaypointInfo(Client *to);
	void				CalculateNewWaypoint();
	void				AssignWaypoints(int32 grid_id, int start_wp = 0);
	void				RemoveWaypoints();
	void				SetWaypointPause();
	void				UpdateWaypoint(int wp_index);
	void				EditWaypoint(int wp, float x, float y, float z, float h, int pause, bool centerpoint);
	void				AddWaypoint(float x, float y, float z, float h, int pause, bool centerpoint);
	void				SetWanderType(int wt) { wandertype = wt; }
	void				SetPauseType(int pt) { pausetype = pt; }

	// quest wandering commands
	void				StopWandering();
	void				ResumeWandering();
	void				PauseWandering(int pausetime);
	void				SetNoQuestPause(bool state) { noQuestPause = state; }
	void				MoveTo(const glm::vec4& position, bool saveguardspot, uint32 delay = 5);
	void				GetClosestWaypoints(std::list<wplist> &wp_list, int count, const glm::vec3& location);
	int					GetClosestWaypoint(const glm::vec3& location);
	void				StopQuestMove(bool setGuardSpot = false);

	void				NextGuardPosition();
	void				SaveGuardSpot(bool iClearGuardSpot = false);
	void				SetGuardSpot(float x, float y, float z, float h);
	inline bool			IsGuarding() const { return(m_GuardPoint.w != 0.0f); }
	void				SaveGuardSpotCharm();
	void				RestoreGuardSpotCharm();
	void				AI_SetRoambox(float iRoamDist, uint32 iDelay = 2500, uint32 iMinDelay = 2500);
	void				AI_SetRoambox(float iMaxX, float iMinX, float iMaxY, float iMinY, uint32 iDelay = 2500, uint32 iMinDelay = 2500);
	void				SetSpawnPoint(float x, float y, float z, float h);

	inline bool WillAggroNPCs() const { return(npc_aggro); }

	inline const uint32 GetNPCSpellsID()	const { return npc_spells_id; }
	inline const uint32 GetNPCSpellsEffectsID()	const { return npc_spells_effects_id; }

	NPCProximity* proximity;
	Spawn2*	respawn2;
	QGlobalCache *GetQGlobals() { return qGlobals; }
	QGlobalCache *CreateQGlobals() { qGlobals = new QGlobalCache(); return qGlobals; }

	SwarmPet *GetSwarmInfo() { return (swarmInfoPtr); }
	void SetSwarmInfo(SwarmPet *mSwarmInfo) { swarmInfoPtr = mSwarmInfo; }

	int		GetHasteCap();
	int32	GetAccuracyRating() const { return (accuracy_rating); }
	void	SetAccuracyRating(int32 d) { accuracy_rating = d;}
	int32 GetRawAC() const { return AC; }
	void	SetIgnoreDistance(float distance) { ignore_distance = distance; }
	float	GetIgnoreDistance() { return ignore_distance; }

	void	ModifyNPCStat(const char *identifier, const char *newValue);
	virtual void SetLevel(uint8 in_level, bool command = false);
	inline void SetClass(uint8 classNum) { if (classNum <= PLAYER_CLASS_COUNT && classNum > 0) class_ = static_cast<uint8>(classNum); }; // for custom scripts

	const bool GetCombatEvent() const { return combat_event; }
	void SetCombatEvent(bool b) { combat_event = b; }

	/* Only allows players that killed corpse to loot */
	const bool HasPrivateCorpse() const { return private_corpse; }
	const bool IsAggroOnPC() const { return aggro_pc; }
	const bool IsUnderwaterOnly() const { return underwater; }
	const char* GetRawNPCTypeName() const { return NPCTypedata.name; }

	inline bool GetNPCAggro() { return npc_aggro; }
	inline void SetNPCAggro(bool state) { npc_aggro = state; }
	inline bool GetIgnoreDespawn() { return ignore_despawn; }

	virtual int GetStuckBehavior() const { return stuck_behavior; }

	bool GetDepop() { return p_depop; }

	void NPCSlotTexture(uint8 slot, uint16 texture);	// Sets new material values for slots

	void AddSpellToNPCList(int16 iPriority, int16 iSpellID, uint16 iType, int16 iManaCost, int32 iRecastDelay, int16 iResistAdjust);
	void AddSpellEffectToNPCList(uint16 iSpellEffectID, int32 base, int32 limit, int32 max);
	void RemoveSpellFromNPCList(int16 spell_id);
	Timer *GetRefaceTimer() const { return reface_timer; }

	NPC_Emote_Struct* GetNPCEmote(uint32 emoteid, uint8 event_);
	void DoNPCEmote(uint8 event_, uint32 emoteid, Mob* target = nullptr);
	void DoFactionEmote();
	bool CanTalk();
	void DoQuestPause(Mob *other);

	inline void SetSpellScale(float amt)		{ spellscale = amt; }
	inline float GetSpellScale()				{ return spellscale; }

	inline void SetHealScale(float amt)		{ healscale = amt; }
	inline float GetHealScale()					{ return healscale; }

	inline void SetSpellFocusDMG(int32 NewSpellFocusDMG) {SpellFocusDMG = NewSpellFocusDMG;}
	inline int32 GetSpellFocusDMG() const { return SpellFocusDMG;}
	inline void SetCastRateDetrimental(uint8 rate) { AISpellVar.engaged_detrimental_chance = rate; }
	inline void SetCastRateBeneficial(uint8 rate) { AISpellVar.engaged_beneficial_other_chance = rate; if (AISpellVar.engaged_beneficial_self_chance < rate) AISpellVar.engaged_beneficial_self_chance = rate; }

	inline void SetSpellFocusHeal(int32 NewSpellFocusHeal) {SpellFocusHeal = NewSpellFocusHeal;}
	inline int32 GetSpellFocusHeal() const {return SpellFocusHeal;}

	uint32	GetSpawnKillCount();
	void	AISpellsList(Client *c);

	bool IsRaidTarget() const { return raid_target; }

	uint16 GetPrimaryMeleeTexture() { return d_melee_texture1; }
	uint16 GetSecondaryMeleeTexture() { return d_melee_texture2; }

	bool IsBoat();
	void ShowQuickStats(Client* client);
	uint8 GetBaseTexture() const { return base_texture; }
	bool IgnoreDespawn() { return ignore_despawn; }
	bool HasEngageNotice() { return engage_notice;  }

	bool HasRoambox() { return roambox_distance; }
	uint32 shop_count;	// Number of times a player has bought or sold an item to/from a merchant.
	uint8 greed;		// merchant greed; default value of 0 means a 25% price markup
	uint8 GetGreedPercent();

	void CreateCorpse(Mob* killer, int32 dmg_total, bool &corpse);
	void GiveExp(Client* killer, bool &xp);
	uint16 GetExpPercent() { return exp_pct; }
	float GetPBAoEReduction(uint8 killer_level);

	// IDs of the first group or player who aggroed this NPC.
	uint32 raid_fte;
	uint32 group_fte;
	uint32 fte_charid;

	uint32 guild_fte;

	uint32 loot_lockout_timer;
	uint32 instance_spawn_timer_override;
	
	// IDs of the guilds who are currently locked out from triggering FTE messages for this NPC and their last disengage time.
	std::map<uint32, uint32> guild_fte_lockouts;

	bool IsGuildInFTELockout(uint32 in_guild_id);

	void InsertGuildFTELockout(uint32 in_guild_id);

	void ProcessFTE();

	// IDs of the first group or player who aggroed this NPC.
	uint32 solo_raid_fte;
	uint32 solo_group_fte;
	uint32 solo_fte_charid;

	// names of first raid members that may be later permitted to loot who aggroed this NPC
	std::vector<std::string> sf_fte_list;

	bool ValidateFTE();

	std::string GetSpawnedString();

protected:

	NPCType	NPCTypedata;

	friend class EntityList;
	std::list<struct NPCFaction*> faction_list;

	int32	grid;
	uint32	spawn_group;
	void	InitializeGrid(int start_wp);

	// loot
	uint32    m_loot_copper;
	uint32    m_loot_silver;
	uint32    m_loot_gold;
	uint32    m_loot_platinum;
	LootItems m_loot_items;

	int32	npc_faction_id;
	int32	precharm_npc_faction_id;
	int32	primary_faction;

	Timer	attacked_timer;		//running while we are being attacked (damaged)
	Timer	swarm_timer;
	Timer	classattack_timer;
	Timer	knightattack_timer;
	Timer	call_help_timer;
	Timer	qglobal_purge_timer;
	Timer	push_timer;			// melee push vector and map collision LoS check

	bool	combat_event;	//true if we are in combat, false otherwise
	Timer	sendhpupdate_timer;
	Timer	enraged_timer;
	Timer *reface_timer;

	uint32	npc_spells_id;
	uint8	casting_spell_AIindex;
	std::unique_ptr<Timer> AIautocastspell_timer;
	uint32*	pDontCastBefore_casting_spell;
	std::vector<AISpells_Struct> AIspells;
	bool HasAISpell;
	virtual bool AICastSpell(Mob* tar, uint8 iChance, uint16 iSpellTypes, bool zeroPriorityOnly = false);
	virtual bool AIDoSpellCast(uint8 i, Mob* tar, int32 mana_cost, uint32* oDontDoAgainBefore = 0);
	AISpellsVar_Struct AISpellVar;

	uint32	npc_spells_effects_id;
	std::vector<AISpellsEffects_Struct> AIspellsEffects;
	bool HasAISpellEffects;
	uint16	innateProcSpellId;
	int8	innateProcChance;
	bool	TryInnateProc(Mob* target);

	uint32	max_dmg;
	uint32	min_dmg;
	int32	accuracy_rating;
	int16	attack_count;
	uint32	npc_mana;
	float	spellscale;
	float	healscale;
	int32 SpellFocusDMG;
	int32 SpellFocusHeal;
	uint16	exp_pct;			// individualized experience multiplier; default is 100

	//pet crap:
	uint16	pet_spell_id;
	bool	taunting;
	Timer	taunt_timer;		//for pet taunting
	Timer	despawn_timer;

	bool npc_aggro;
	bool assisting;

	std::deque<int> signal_q;
	std::deque<std::string> signal_strq;

	//waypoint crap:
	std::vector<wplist> Waypoints;
	int max_wp;
	int save_wp;
	glm::vec4 m_GuardPoint;
	glm::vec4 m_GuardPointSaved;
	EmuAppearance guard_anim;
	float roambox_max_x;
	float roambox_max_y;
	float roambox_min_x;
	float roambox_min_y;
	float roambox_ceil;
	bool roambox_distance;
	float roambox_movingto_x;
	float roambox_movingto_y;
	float roambox_movingto_z;
	uint32 roambox_delay;
	uint32 roambox_min_delay;

	uint16	skills[EQ::skills::HIGHEST_SKILL+1];

	uint32	equipment[EQ::invslot::EQUIPMENT_COUNT];	//this is an array of item IDs
	uint16	d_melee_texture1;			//this is an item Material value
	uint16	d_melee_texture2;			//this is an item Material value (offhand)
	uint8	prim_melee_type;			//Sets the Primary Weapon attack message and animation
	uint8	sec_melee_type;				//Sets the Secondary Weapon attack message and animation
	uint8   ranged_type;				//Sets the Ranged Weapon attack message and animation

	SwarmPet *swarmInfoPtr;

	QGlobalCache *qGlobals;

	bool raid_target;
	bool hasZeroPrioritySpells;
	float ignore_distance;
	uint8 base_texture;
	bool ignore_despawn;		// NPCs with this set to 1 will ignore the despawn value in spawngroup
	bool noQuestPause;			// if true, do not stop moving on say events.  used for escort NPCs

	bool private_corpse; 
	bool aggro_pc;
	bool underwater = false;

	bool engage_notice;

	uint8 stuck_behavior;

private:
	uint32	m_loottable_id;
	bool	m_skip_global_loot;
	bool	p_depop;
	glm::vec3 push_vector;
	float wall_normal1_x;
	float wall_normal1_y;
	float wall_normal2_x;
	float wall_normal2_y;
	float corner_x;
	float corner_y;
};

#endif

