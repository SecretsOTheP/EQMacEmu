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

#ifndef HATELIST_H
#define HATELIST_H

class Client;
class Group;
class Mob;
class Raid;

struct tHateEntry
{
	Mob *ent;
	int32 damage, hate;
	bool bFrenzy;
	float dist_squared;
	uint32 last_damage;
	uint32 last_hate;
};

struct SInitialEngageEntry {
	bool m_hasInitialEngageIds;
	uint32 m_raidId;
	uint32 m_groupId;
	std::vector<uint32> m_ids;
	std::vector<uint32> m_disbanded;

	SInitialEngageEntry() : m_hasInitialEngageIds(false), m_raidId(0), m_groupId(0) {}
	void Reset();
	std::string ToJson() const;
	void SetHasInitialEngageIds(bool hasInitialEngageIds);
	bool HasInitialEngageIds() const;
	void AddEngagerIds(const std::vector<uint32>& ids);
	void RemoveEngagerIds(const std::vector<uint32>& ids);
	void EngagerIdsAreHistory(const std::vector<uint32>& ids);
};

class HateList
{
public:
	HateList();
	~HateList();

	// adds a mob to the hatelist
	void Add(Mob *ent, int32 in_hate=0, int32 in_dam=0, bool bFrenzy = false, bool iAddIfNotExist = true);
	// sets existing hate
	void Set(Mob *other, int32 in_hate, int32 in_dam);
	// removes mobs from hatelist
	bool RemoveEnt(Mob *ent);
	void RemoveFeigned();
	// Remove all
	void Wipe(bool from_memblur = false);
	// ???
	void DoFactionHits(int32 nfl_id, bool &success);
	// Gets Hate amount for mob
	int32 GetEntHate(Mob *ent, bool includeBonus = true);
	// get the top damage single mob (including pets and npcs), no group or raid
	Mob *GetDamageTopSingleMob(int32 &return_dmg);
	// gets top hated mob
	Mob *GetTop();
	// gets any on the list
	Mob *GetRandom();
	Client *GetRandomClient(int32 max_dist = 0);
	// get closest mob or nullptr if list empty
	Mob *GetClosest(Mob *hater = nullptr);
	// get closest client
	Mob *GetClosestClient(Mob *hater = nullptr);
	// get closest npc
	Mob *GetClosestNPC(Mob *hater = nullptr);
	// Gets Hate amount for mob
	int32 GetEntDamage(Mob *ent, bool combine_pet_dmg = false);
	// get the top damage single mob (including pets and npcs), no group or raid
	Mob *GetDamageTopSingleMob(int32& return_dmg);
	// gets top mob or nullptr if hate list empty
	Mob *GetDamageTop(int32& return_dmg, bool combine_pet_dmg = true, bool clients_only = false);
	// used to check if mob is on hatelist
	bool IsOnHateList(Mob *);
	bool IsClientOnHateList();
	bool IsCharacterOnHateList(uint32 character_id);
	bool IsGroupOnHateList(uint32 group_id);
	bool IsRaidOnHateList(uint32 raid_id);
	bool IsGuildOnHateList(uint32 guild_id);
	// used to remove or add frenzy hate
	void CheckFrenzyHate();
	//Gets the target with the most hate regardless of things like frenzy etc.
	Mob* GetMostHate(bool includeBonus = true);
	// Count 'Summoned' pets on hatelist
	int SummonedPetCount(Mob *hater);
	// setting this true will allow the NPC to persue targets outside the NPC's distance limit
	void SetRememberDistantMobs(bool state) { rememberDistantMobs = state; }
	// get hate for the Nth player on the list, in descending order.  Does not include bonuses
	int GetHateN(int n);
	// return the number of hated entities
	int GetNumHaters() { return list.size(); }
	// For prox aggro NPCs, gets first hater in combat range; else returns first to aggro in range; null if nobody in range
	Mob* GetFirstMobInRange();
	// time since aggro started (if engaged) or time since aggro ended (if not engaged) or 0xFFFFFFFF if never aggroed
	uint32 GetAggroDeaggroTime() { return aggroDeaggroTime == 0xFFFFFFFF ? 0xFFFFFFFF : Timer::GetCurrentTime() - aggroDeaggroTime; }
	// time since aggro started (if engaged) or 0xFFFFFFFF if never aggroed
	uint32 GetAggroTime() { return aggroTime == 0xFFFFFFFF ? 0xFFFFFFFF : aggroTime; }


	void SetAggroTime(uint32 in_time) { aggroTime = in_time; }



	int AreaRampage(Mob *caster, Mob *target, int count, int damagePct = 100);

	void SpellCast(Mob *caster, uint32 spell_id, float range, Mob *ae_center  = nullptr);

	bool IsEmpty();
	bool IsIgnoringAllHaters() { return allHatersIgnored; }
	bool HasFeignedHaters() { return hasFeignedHaters; }
	bool HasLandHaters() { return hasLandHaters; } // true if NPC is ignoring a hater because it is outside of water; excluding feigned and out of range players
	uint32 GetIgnoreStuckCount() { return ignoreStuckCount; }
	void PrintToClient(Client *c);

	//For accessing the hate list via perl; don't use for anything else
	std::list<tHateEntry*>& GetHateList() { return list; }

	//setting owner
	void SetOwner(Mob *newOwner);

	void ReportDmgTotals(Mob* mob, bool corpse, bool xp, bool faction, int32 dmg_amt);

	void HandleFTEEngage(Client* client);
	void HandleFTEDisengage();

	bool KillerIsNotInitialEngager(Mob* const ent);
	void LogInitialEngageIdResult(Client* const killedBy = nullptr);

protected:
	tHateEntry* Find(Mob *ent);
	int32 GetHateBonus(tHateEntry *entry, bool combatRange, bool firstInRange = false, float distSquared = -1.0f);
	int32 GetEntPetDamage(Mob* ent);
	void RecordInitialClientHateIds(Mob* const ent);
	void UpdateInitialClientHateIds(Mob* const ent);
	void RemoveInitialClientHateIds(Mob* const ent);

private:
	std::list<tHateEntry*> list;
	Mob *owner;
	int32 combatRangeBonus;
	int32 sitInsideBonus;
	int32 sitOutsideBonus;
	bool rememberDistantMobs;
	bool nobodyInMeleeRange;
	uint32 aggroTime;
	uint32 aggroDeaggroTime;
	bool allHatersIgnored;
	bool hasFeignedHaters;
	bool hasLandHaters;
	uint32 ignoreStuckCount;
	SInitialEngageEntry m_initialEngageEntry;
};

#endif
