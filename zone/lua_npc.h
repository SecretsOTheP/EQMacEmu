#ifndef EQEMU_LUA_NPC_H
#define EQEMU_LUA_NPC_H
#ifdef LUA_EQEMU

#include "lua_mob.h"

class NPC;
class Lua_Mob;
class Lua_NPC;
class Lua_Client;

namespace luabind {
	struct scope;
}

luabind::scope lua_register_npc();

class Lua_NPC : public Lua_Mob
{
	typedef NPC NativeType;
public:
	Lua_NPC() { SetLuaPtrData(nullptr); }
	Lua_NPC(NPC *d) { SetLuaPtrData(reinterpret_cast<Entity*>(d)); }
	virtual ~Lua_NPC() { }

	operator NPC*() {
		return reinterpret_cast<NPC*>(GetLuaPtrData());
	}

	void Signal(int id);
	int CheckNPCFactionAlly(int faction);
	void AddItem(int item_id, int charges);
	void AddItem(int item_id, int charges, bool equip);
	void AddLootTable();
	void AddLootTable(int id);
	void RemoveItem(int item_id);
	void RemoveItem(int item_id, int slot);
	void ClearLootItems();
	void AddLootCash(int copper, int silver, int gold, int platinum);
	void RemoveLootCash();
	int CountLoot();
	int GetLoottableID();
	uint32 GetCopper();
	uint32 GetSilver();
	uint32 GetGold();
	uint32 GetPlatinum();
	void SetCopper(uint32 amt);
	void SetSilver(uint32 amt);
	void SetGold(uint32 amt);
	void SetPlatinum(uint32 amt);
	void SetGrid(int grid);
	void SetSaveWaypoint(int wp);
	void SetSp2(int sg2);
	int GetWaypointMax();
	int GetGrid();
	uint32 GetSp2();
	int GetNPCFactionID();
	int GetPrimaryFaction();
	int GetNPCHate(Lua_Mob ent);
	bool IsOnHatelist(Lua_Mob ent);
	void SetNPCFactionID(int id);
	uint32 GetMaxDMG();
	uint32 GetMinDMG();
	bool IsAnimal();
	int GetPetSpellID();
	void SetPetSpellID(int id);
	uint32 GetMaxDamage(int level);
	void SetTaunting(bool t);
	void PickPocket(Lua_Client thief);
	void StartSwarmTimer(uint32 duration);
	void DoClassAttacks(Lua_Mob target);
	int GetMaxWp();
	void DisplayWaypointInfo(Lua_Client to);
	void CalculateNewWaypoint();
	void AssignWaypoints(int grid);
	void RemoveWaypoints();
	void SetWaypointPause();
	void UpdateWaypoint(int wp);
	void EditWaypoint(int wp, float x, float y, float z, float h, int pause, bool centerpoint);
	void AddWaypoint(float x, float y, float z, float h, int pause, bool centerpoint);
	void SetWanderType(int wt);
	void SetPauseType(int pt);
	void StopWandering();
	void ResumeWandering();
	void PauseWandering(int pause_time);
	void SetNoQuestPause(bool state);
	void MoveTo(float x, float y, float z, float h, bool save);
	void MoveTo(float x, float y, float z, float h, bool save, uint32 delay);
	void StopQuestMove();
	void StopQuestMove(bool save_guard_spot);
	void NextGuardPosition();
	void SaveGuardSpot();
	void SaveGuardSpot(bool clear);
	void SetGuardSpot(float x, float y, float z, float h);
	bool IsGuarding();
	void AI_SetRoambox(float max_x, float min_x, float max_y, float min_y);
	void AI_SetRoambox(float max_x, float min_x, float max_y, float min_y, uint32 delay, uint32 mindelay);
	int GetNPCSpellsID();
	int GetSpawnPointID();
	float GetSpawnPointX();
	float GetSpawnPointY();
	float GetSpawnPointZ();
	float GetSpawnPointH();
	float GetGuardPointX();
	float GetGuardPointY();
	float GetGuardPointZ();
	void SetPrimSkill(int skill_id);
	void SetSecSkill(int skill_id);
	int GetPrimSkill();
	int GetSecSkill();
	int GetSwarmOwner();
	int GetSwarmTarget();
	void SetSwarmTarget(int target);
	void ModifyNPCStat(const char *stat, const char *value);
	void AddAISpell(int priority, int spell_id, int type, int mana_cost, int recast_delay, int resist_adjust);
	void RemoveAISpell(int spell_id);
	void SetCastRateDetrimental(int rate);
	void SetCastRateBeneficial(int rate);
	void SetSpellFocusDMG(int focus);
	void SetSpellFocusHeal(int focus);
	int GetSpellFocusDMG();
	int GetSpellFocusHeal();
	float GetSlowMitigation();
	int GetAccuracyRating();
	int GetSpawnKillCount();
	void MerchantOpenShop();
	void MerchantCloseShop();
	void AddQuestLoot(int itemid);
	void AddQuestLoot(int itemid, int charges);
	void AddQuestLoot(int itemid, int charges, std::string item_custom_data_string);
	void AddPetLoot(int itemid);
	void AddPetLoot(int itemid, int charges);
	void AddPetLoot(int itemid, int charges, std::string item_custom_data_string);
	bool GetQuestLoot(int itemid);
	bool GetPetLoot(int itemid);
	bool HasQuestLoot();
	void DeleteQuestLoot();
	void DeleteQuestLoot(int itemid1);
	void DeleteQuestLoot(int itemid1, int itemid2);
	void DeleteQuestLoot(int itemid1, int itemid2, int itemid3);
	void DeleteQuestLoot(int itemid1, int itemid2, int itemid3, int itemid4);
	bool HasRequiredQuestLoot(int itemid1, int itemid2, int itemid3, int itemid4);
	int QuestLootCount(int itemid);
	bool CanTalk();
	void ForceRepop();
	void SetNPCAggro(bool state);
	void SetBaseHP(uint32 new_hp);
	void SetSpawnPoint(float x, float y, float z, float h);
	void SetClass(int classNum);
	void SetMaxDamage(uint32 new_max_damage);
	void SetMinDamage(uint32 new_min_damage);
};

#endif
#endif
