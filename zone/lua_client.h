#ifndef EQEMU_LUA_CLIENT_H
#define EQEMU_LUA_CLIENT_H
#ifdef LUA_EQEMU

#include "lua_mob.h"

class Client;
class Lua_Group;
class Lua_Raid;
class Lua_Inventory;
class Lua_Packet;

namespace luabind {
	struct scope;
}

luabind::scope lua_register_client();
luabind::scope lua_register_inventory_where();

class Lua_Client : public Lua_Mob
{
	typedef Client NativeType;
public:
	Lua_Client() { SetLuaPtrData(nullptr); }
	Lua_Client(Client *d) { SetLuaPtrData(reinterpret_cast<Entity*>(d)); }
	virtual ~Lua_Client() { }

	operator Client*() {
		return reinterpret_cast<Client*>(GetLuaPtrData());
	}

	void SendSound(unsigned int soundid);
	void Save();
	void Save(int commit_now);
	bool Connected();
	bool InZone();
	void Kick();
	void Disconnect();
	bool IsLD();
	void WorldKick();
	bool GetAnon();
	void Duck();
	void Stand();
	void SetGM(bool v);
	void SetPVP(bool v);
	bool GetPVP();
	bool GetGM();
	void SetBaseClass(int v);
	void SetBaseRace(int v);
	void SetBaseGender(int v);
	bool HasCharacterCreateCombination(uint16 in_class, uint16 in_race, uint16 in_deity, int in_player_choice_city);
	void AddCharacterCreateCombinationUnlock(uint16 in_class, uint16 in_race, uint16 in_deity, int player_home_choice, uint16 home_zone_id, float bind_x, float bind_y, float bind_z, float bind_heading);
	bool PermaStats(uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA, bool check_cooldown);
	bool PermaRace(
		uint16 new_race, uint16 new_deity, int player_choice_city,
		uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA
	);
	bool ForcePermaRace(
		uint16 new_race, uint16 new_deity, int player_choice_city,
		uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA
	);
	bool PermaClass(
		uint16 new_class, uint16 new_deity, int player_choice_city,
		uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA
	);
	bool ForcePermaClass(
		uint16 new_class, uint16 new_deity, int player_choice_city,
		uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA
	);
	int GetBaseFace();
	int GetLanguageSkill(int skill_id);
	const char *GetLastName();
	const char* GetProfileLastName();
	int GetBaseSTR();
	int GetBaseSTA();
	int GetBaseCHA();
	int GetBaseDEX();
	int GetBaseINT();
	int GetBaseAGI();
	int GetBaseWIS();
	int GetWeight();
	uint32 GetEXP();
	uint32 GetAAExp();
	void SetDeity(int v);
	void AddEXP(uint32 add_exp, int conlevel, Lua_Mob other);
	void SetEXP(uint32 set_exp, uint32 set_aaxp);
	void SetEXP(uint32 set_exp, uint32 set_aaxp, bool resexp);
	void AddEXPPercent(uint8 percent, uint8 level);
	void SetBindPoint();
	void SetBindPoint(int to_zone);
	void SetBindPoint(int to_zone, float new_x);
	void SetBindPoint(int to_zone, float new_x, float new_y);
	void SetBindPoint(int to_zone, float new_x, float new_y, float new_z);
	void SetBindPoint(int to_zone, float new_x, float new_y, float new_z, float new_heading);
	float GetBindX();
	float GetBindX(int index);
	float GetBindY();
	float GetBindY(int index);
	float GetBindZ();
	float GetBindZ(int index);
	float GetBindHeading();
	float GetBindHeading(int index);
	uint32 GetBindZoneID();
	uint32 GetBindZoneID(int index);
	uint32 GetTimesRebirthed();
	void MovePC(int zone, float x, float y, float z, float heading);
	void ChangeLastName(const char *in);
	int GetFactionLevel(uint32 char_id, uint32 race, uint32 class_, uint32 deity, uint32 faction, Lua_NPC npc);
	int GetFactionValue(Lua_NPC npc);
	void SetFactionLevel(uint32 char_id, uint32 npc_id);
	void SetFactionLevel2(uint32 char_id, int faction_id, int value, int temp);
	int GetRawItemAC();
	uint32 AccountID();
	const char *AccountName();
	int Admin();
	uint32 CharacterID();
	int GuildRank();
	uint32 GuildID();
	const char *GetGuildName();
	int GetFace();
	bool TakeMoneyFromPP(uint64 copper);
	bool TakeMoneyFromPP(uint64 copper, bool update_client);
	void AddMoneyToPP(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, bool update_client);
	bool TGB();
	int GetSkillPoints();
	void SetSkillPoints(int skill);
	void IncreaseSkill(int skill_id);
	void IncreaseSkill(int skill_id, int value);
	void IncreaseLanguageSkill(int skill_id);
	void IncreaseLanguageSkill(int skill_id, int value);
	int GetRawSkill(int skill_id);
	bool HasSkill(int skill_id);
	bool CanHaveSkill(int skill_id);
	void SetSkill(int skill_id, int value);
	void AddSkill(int skill_id, int value);
	void CheckSpecializeIncrease(int spell_id);
	void CheckIncreaseSkill(int skill_id, Lua_Mob target);
	void CheckIncreaseSkill(int skill_id, Lua_Mob target, int chance_mod);
	void SetLanguageSkill(int language, int value);
	int MaxSkill(int skill_id);
	bool IsMedding();
	int GetDuelTarget();
	bool IsDueling();
	void SetDuelTarget(int c);
	void SetDueling(bool v);
	void ResetAA();
	void MemSpell(int spell_id, int slot);
	void MemSpell(int spell_id, int slot, bool update_client);
	void UnmemSpell(int slot);
	void UnmemSpell(int slot, bool update_client);
	void UnmemSpellAll();
	void UnmemSpellAll(bool update_client);
	luabind::object GetMemmedSpells(lua_State* L);
	luabind::object GetScribedSpells(lua_State* L);
	luabind::object GetScribeableSpells(lua_State* L);
	luabind::object GetScribeableSpells(lua_State* L, uint8 min_level);
	luabind::object GetScribeableSpells(lua_State* L, uint8 min_level, uint8 max_level);
	void ScribeSpell(int spell_id, int slot);
	void ScribeSpell(int spell_id, int slot, bool update_client);
	uint16 ScribeSpells(uint8 min_level, uint8 max_level);
	void UnscribeSpell(int slot);
	void UnscribeSpell(int slot, bool update_client);
	void UnscribeSpellAll();
	void UnscribeSpellAll(bool update_client);
	bool IsSitting();
	void SetFeigned(bool v);
	bool GetFeigned();
	bool IsFeigned();
	bool AutoSplitEnabled();
	void SetHorseId(int id);
	int GetHorseId();
	void NukeItem(uint32 item_num);
	void NukeItem(uint32 item_num, int where_to_check);
	void SetMaterial(int slot_id, uint32 item_id);
	int GetItemIDAt(int slot_id);
	bool FindOnCursor(uint32 item_id);
	void DeleteItemInInventory(int slot_id, int quantity);
	void DeleteItemInInventory(int slot_id, int quantity, bool update_client);
	void SummonItem(uint32 item_id);
	void SummonItem(uint32 item_id, int charges);
	void SummonItem(uint32 item_id, int charges, int to_slot);
	void SummonItem(uint32 item_id, int charges, int to_slot, bool force_charges);
	void SummonCursorItem(uint32 item_id);
	void SummonCursorItem(uint32 item_id, int charges);
	void DropItem(int slot_id);
	void BreakInvis();
	void LeaveGroup();
	bool IsGrouped();
	bool IsRaidGrouped();
	bool Hungry();
	bool Thirsty();
	int GetInstrumentMod(int spell_id);
	bool DecreaseByID(uint32 type, int amt);
	void Escape();
	void GoFish();
	void ForageItem();
	void ForageItem(bool guarantee);
	float CalcPriceMod(Lua_Mob other, bool reverse);
	void ResetTrade();
	bool UseDiscipline(int spell_id);
	int GetCharacterFactionLevel(int faction_id);
	void SetZoneFlag(int zone_id);
	void ClearZoneFlag(int zone_id);
	bool HasZoneFlag(int zone_id);
	void SendZoneFlagInfo(Lua_Client to);
	void SetAATitle(const char *title);
	int GetClientVersion();
	uint32 GetClientVersionBit();
	void SetTitleSuffix(const char *text);
	void SetAAPoints(int points);
	int GetAAPoints();
	int GetSpentAA();
	void AddAAPoints(int points);
	void RefundAA();
	void ExpendAATimer(int aaid);
	void SetHardcore(int hardcore);
	int IsHardcore();
	void SetSelfFound(int self_found);
	int IsSelfFound();
	void SetSoloOnly(int solo_only);
	int IsSoloOnly();

	void ClearPlayerInfoAndGrantStartingItems();
	void ResetPlayerForNewGamePlus(uint8 new_level, uint8 new_level2, bool reset_skill_points);
	int GetModCharacterFactionLevel(int faction);
	int GetStartZone();
	void KeyRingAdd(uint32 item);
	bool KeyRingCheck(uint32 item);
	void QuestReadBook(const char *text, int type);
	void SendOPTranslocateConfirm(Lua_Mob caster, int spell_id);
	uint32 GetIP();
	void AddLevelBasedExp(int exp_pct);
	void AddLevelBasedExp(int exp_pct, int max_level);
	void IncrementAA(int aa);
	int GetNextAvailableSpellBookSlot();
	int GetNextAvailableSpellBookSlot(int start);
	int FindSpellBookSlotBySpellID(int spell_id);
	int GetCorpseCount();
	int GetCorpseID(int corpse);
	int GetCorpseItemAt(int corpse, int slot);
	void Freeze();
	void UnFreeze();
	int GetAggroCount();
	uint64 GetCarriedMoney();
	uint64 GetAllMoney();
	void Signal(uint32 id);
	bool HasSpellScribed(int spell_id);
	void SetAccountFlag(std::string flag, std::string val);
	std::string GetAccountFlag(std::string flag);
	Lua_Group GetGroup();
	Lua_Raid GetRaid();
	bool PutItemInInventory(int slot_id, Lua_ItemInst inst);
	bool PushItemOnCursor(Lua_ItemInst inst);
	bool PushItemOnCursorWithoutQueue(Lua_ItemInst inst);
	Lua_Inventory GetInventory();
	void QueuePacket(Lua_Packet app);
	void QueuePacket(Lua_Packet app, bool ack_req);
	void QueuePacket(Lua_Packet app, bool ack_req, int client_connection_status);
	void QueuePacket(Lua_Packet app, bool ack_req, int client_connection_status, int filter);
	int GetHunger();
	int GetThirst();
	void SetHunger(int in_hunger);
	void SetThirst(int in_thirst);
	void SetConsumption(int in_hunger, int in_thirst);
	int GetBoatID();
	void SetBoatID(uint32 in_boatid);
	char* GetBoatName();
	void SetBoatName(const char* in_boatname);
	void QuestReward(Lua_Mob target);
	void QuestReward(Lua_Mob target, int32 copper);
	void QuestReward(Lua_Mob target, int32 copper, int32 silver);
	void QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold);
	void QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum);
	void QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid);
	void QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid, int32 exp);
	void QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid, int32 exp, bool faction);
	void QuestReward(Lua_Mob target, luabind::adl::object reward);
	int GetHandToHandDamage();
	int GetHandToHandDelay();

	void PermaGender(uint32 in_Gender);
	bool IsMarried();
	void SetMarried(const char*);
	void SetTemporaryLastName(char*);
	void SetTemporaryCustomizedLastName(char* lastname);
	bool HasTemporaryLastName();

	uint8 GetSkillTrainLevel(int skill_id);
	void MaxSkills();
	bool SendGMCommand(std::string message);
	bool SendGMCommand(std::string message, bool ignore_status);
	luabind::object GetRaidOrGroupOrSelf(lua_State *L);
	luabind::object GetRaidOrGroupOrSelf(lua_State *L, bool clients_only);

	void SetClientMaxLevel(uint8 max_level);
	uint8 GetClientMaxLevel();
};

#endif
#endif
