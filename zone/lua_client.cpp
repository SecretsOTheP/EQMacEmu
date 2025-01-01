#ifdef LUA_EQEMU

#include "lua.hpp"
#include <luabind/luabind.hpp>

#include "client.h"
#include "guild_mgr.h"
#include "lua_client.h"
#include "lua_npc.h"
#include "lua_item.h"
#include "lua_iteminst.h"
#include "lua_inventory.h"
#include "lua_group.h"
#include "lua_raid.h"
#include "lua_packet.h"

struct InventoryWhere { };

void Lua_Client::SendSound(unsigned int soundid) {
	Lua_Safe_Call_Void();
	self->SendSound(soundid);
}
void Lua_Client::Save() {
	Lua_Safe_Call_Void();
	self->Save();
}

void Lua_Client::Save(int commit_now) {
	Lua_Safe_Call_Void();
	self->Save(commit_now);
}

bool Lua_Client::Connected() {
	Lua_Safe_Call_Bool();
	return self->Connected();
}

bool Lua_Client::InZone() {
	Lua_Safe_Call_Bool();
	return self->InZone();
}

void Lua_Client::Kick() {
	Lua_Safe_Call_Void();
	self->Kick();
}

void Lua_Client::Disconnect() {
	Lua_Safe_Call_Void();
	self->Disconnect();
}

bool Lua_Client::IsLD() {
	Lua_Safe_Call_Bool();
	return self->IsLD();
}

void Lua_Client::WorldKick() {
	Lua_Safe_Call_Void();
	self->WorldKick();
}

bool Lua_Client::GetAnon() {
	Lua_Safe_Call_Bool();
	return self->GetAnon() != 0;
}

void Lua_Client::Duck() {
	Lua_Safe_Call_Void();
	self->Duck();
}

void Lua_Client::Stand() {
	Lua_Safe_Call_Void();
	self->Stand();
}

void Lua_Client::SetGM(bool v) {
	Lua_Safe_Call_Void();
	self->SetGM(v);
}

void Lua_Client::SetPVP(bool v) {
	Lua_Safe_Call_Void();
	self->SetPVP(v);
}

bool Lua_Client::GetPVP() {
	Lua_Safe_Call_Bool();
	return self->GetPVP();
}

bool Lua_Client::GetGM() {
	Lua_Safe_Call_Bool();
	return self->GetGM();
}

void Lua_Client::SetBaseClass(int v) {
	Lua_Safe_Call_Void();
	self->SetBaseClass(v);
}

void Lua_Client::SetBaseRace(int v) {
	Lua_Safe_Call_Void();
	self->SetBaseRace(v);
}

void Lua_Client::SetBaseGender(int v) {
	Lua_Safe_Call_Void();
	self->SetBaseGender(v);
}

bool Lua_Client::HasCharacterCreateCombination(uint16 in_class, uint16 in_race, uint16 in_deity, int in_player_choice_city) {
	Lua_Safe_Call_Bool();
	return self->HasCharacterCreateCombination(in_class, in_race, in_deity, in_player_choice_city);
}

void Lua_Client::AddCharacterCreateCombinationUnlock(
	uint16 in_class, uint16 in_race, uint16 in_deity, int player_home_choice,
	uint16 home_zone_id, float bind_x, float bind_y, float bind_z, float bind_heading) {
	Lua_Safe_Call_Void();
	self->AddCharacterCreateCombinationUnlock(in_class, in_race, in_deity, player_home_choice, home_zone_id, bind_x, bind_y, bind_z, bind_heading);
}

bool Lua_Client::PermaStats(
	uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA, bool check_cooldown) {
	Lua_Safe_Call_Bool();
	return self->PermaStats(self, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, check_cooldown);
}

bool Lua_Client::PermaRace(
	uint16 new_race, uint16 new_deity, int player_choice_city, 
	uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA) {
	Lua_Safe_Call_Bool();
	return self->PermaRace(self, new_race, new_deity, player_choice_city, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, false);
}

bool Lua_Client::ForcePermaRace(
	uint16 new_race, uint16 new_deity, int player_choice_city,
	uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA) {
	Lua_Safe_Call_Bool();
	return self->PermaRace(self, new_race, new_deity, player_choice_city, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, true);
}

bool Lua_Client::PermaClass(
	uint16 new_class, uint16 new_deity, int player_choice_city,
	uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA) {
	Lua_Safe_Call_Bool();
	return self->PermaClass(self, new_class, new_deity, player_choice_city, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, false);
}

bool Lua_Client::ForcePermaClass(
	uint16 new_class, uint16 new_deity, int player_choice_city,
	uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA) {
	Lua_Safe_Call_Bool();
	return self->PermaClass(self, new_class, new_deity, player_choice_city, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, true);
}

int Lua_Client::GetBaseFace() {
	Lua_Safe_Call_Int();
	return self->GetBaseFace();
}

int Lua_Client::GetLanguageSkill(int skill_id) {
	Lua_Safe_Call_Int();
	return self->GetLanguageSkill(skill_id);
}

const char *Lua_Client::GetLastName() {
	Lua_Safe_Call_String();
	return self->GetLastName();
}

const char* Lua_Client::GetProfileLastName() {
	Lua_Safe_Call_String();
	return self->GetProfileLastName();
}

void Lua_Client::SetMarried(const char* temporary_name) {
	Lua_Safe_Call_Void();
	self->SetMarried(temporary_name);
}

bool Lua_Client::IsMarried() {
	Lua_Safe_Call_Bool();
	return self->IsMarried();
}

void Lua_Client::SetTemporaryLastName(char* temporary_name) {
	Lua_Safe_Call_Void();
	self->SetTemporaryLastName(temporary_name);
}

void Lua_Client::SetTemporaryCustomizedLastName(char* temporary_name) {
	Lua_Safe_Call_Void();
	self->SetTemporaryCustomizedLastName(temporary_name);
}

bool Lua_Client::HasTemporaryLastName() {
	Lua_Safe_Call_Bool();
	return self->HasTemporaryLastName();
}

int Lua_Client::GetBaseSTR() {
	Lua_Safe_Call_Int();
	return self->GetBaseSTR();
}

int Lua_Client::GetBaseSTA() {
	Lua_Safe_Call_Int();
	return self->GetBaseSTA();
}

int Lua_Client::GetBaseCHA() {
	Lua_Safe_Call_Int();
	return self->GetBaseCHA();
}

int Lua_Client::GetBaseDEX() {
	Lua_Safe_Call_Int();
	return self->GetBaseDEX();
}

int Lua_Client::GetBaseINT() {
	Lua_Safe_Call_Int();
	return self->GetBaseINT();
}

int Lua_Client::GetBaseAGI() {
	Lua_Safe_Call_Int();
	return self->GetBaseAGI();
}

int Lua_Client::GetBaseWIS() {
	Lua_Safe_Call_Int();
	return self->GetBaseWIS();
}

int Lua_Client::GetWeight() {
	Lua_Safe_Call_Int();
	return self->GetWeight();
}

uint32 Lua_Client::GetEXP() {
	Lua_Safe_Call_Int();
	return self->GetEXP();
}

uint32 Lua_Client::GetAAExp() {
	Lua_Safe_Call_Int();
	return self->GetAAXP();
}

void Lua_Client::SetDeity(int v) {
	Lua_Safe_Call_Void();
	self->SetDeity(v);
}

void Lua_Client::AddEXP(uint32 add_exp, int conlevel, Lua_Mob other) {
	Lua_Safe_Call_Void();
	
	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (self->GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}
	self->AddEXP(add_exp, conlevel, other);
}

void Lua_Client::SetEXP(uint32 set_exp, uint32 set_aaxp) {
	Lua_Safe_Call_Void();
	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (self->GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}
	self->SetEXP(set_exp, set_aaxp);
}

void Lua_Client::SetEXP(uint32 set_exp, uint32 set_aaxp, bool resexp) {
	Lua_Safe_Call_Void();
	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (self->GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}
	self->SetEXP(set_exp, set_aaxp, resexp);
}

void Lua_Client::AddEXPPercent(uint8 percent, uint8 level) {
	Lua_Safe_Call_Void();
	
	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (self->GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}

	self->AddEXPPercent(percent, level);
}

void Lua_Client::SetBindPoint() {
	Lua_Safe_Call_Void();
	self->SetBindPoint();
}

void Lua_Client::SetBindPoint(int to_zone) {
	Lua_Safe_Call_Void();
	self->SetBindPoint(to_zone);
}

void Lua_Client::SetBindPoint(int to_zone, float new_x) {
	Lua_Safe_Call_Void();
	self->SetBindPoint(to_zone, glm::vec3(new_x,0.0f,0.0f));
}

void Lua_Client::SetBindPoint(int to_zone, float new_x, float new_y) {
	Lua_Safe_Call_Void();
	self->SetBindPoint(to_zone, glm::vec3(new_x, new_y, 0.0f));
}

void Lua_Client::SetBindPoint(int to_zone, float new_x, float new_y, float new_z) {
	Lua_Safe_Call_Void();
	self->SetBindPoint(to_zone, glm::vec3(new_x, new_y, new_z));
}

void Lua_Client::SetBindPoint(int to_zone, float new_x, float new_y, float new_z, float new_heading) {
	Lua_Safe_Call_Void();
	self->SetBindPoint2(to_zone, glm::vec4(new_x, new_y, new_z, new_heading));
}

float Lua_Client::GetBindX() {
	Lua_Safe_Call_Real();
	return self->GetBindX();
}

float Lua_Client::GetBindX(int index) {
	Lua_Safe_Call_Real();
	return self->GetBindX(index);
}

float Lua_Client::GetBindY() {
	Lua_Safe_Call_Real();
	return self->GetBindY();
}

float Lua_Client::GetBindY(int index) {
	Lua_Safe_Call_Real();
	return self->GetBindY(index);
}

float Lua_Client::GetBindZ() {
	Lua_Safe_Call_Real();
	return self->GetBindZ();
}

float Lua_Client::GetBindZ(int index) {
	Lua_Safe_Call_Real();
	return self->GetBindZ(index);
}

float Lua_Client::GetBindHeading() {
	Lua_Safe_Call_Real();
	return self->GetBindHeading();
}

float Lua_Client::GetBindHeading(int index) {
	Lua_Safe_Call_Real();
	return self->GetBindHeading(index);
}

uint32 Lua_Client::GetBindZoneID() {
	Lua_Safe_Call_Int();
	return self->GetBindZoneID();
}

uint32 Lua_Client::GetBindZoneID(int index) {
	Lua_Safe_Call_Int();
	return self->GetBindZoneID(index);
}

uint32 Lua_Client::GetTimesRebirthed() {
	Lua_Safe_Call_Int();
	return self->GetTimesRebirthed();
}

void Lua_Client::MovePC(int zone_id, float x, float y, float z, float heading) {
	Lua_Safe_Call_Void();
	self->MovePCQuest(zone_id, x, y, z, heading);
}

void Lua_Client::ChangeLastName(const char *in) {
	Lua_Safe_Call_Void();
	self->ChangeLastName(in);
}

int Lua_Client::GetFactionLevel(uint32 char_id, uint32 race, uint32 class_, uint32 deity, uint32 faction, Lua_NPC npc) {
	Lua_Safe_Call_Int();
	return static_cast<int>(self->GetFactionLevel(char_id, race, class_, deity, faction, npc, true));
}

int Lua_Client::GetFactionValue(Lua_NPC npc) {
	Lua_Safe_Call_Int();
	return static_cast<int>(self->GetFactionValue(npc));
}

void Lua_Client::SetFactionLevel(uint32 char_id, uint32 npc_id) {
	Lua_Safe_Call_Void();
	self->SetFactionLevel(char_id, npc_id);
}

void Lua_Client::SetFactionLevel2(uint32 char_id, int faction_id, int value, int temp) {
	Lua_Safe_Call_Void();
	self->SetFactionLevel2(char_id, faction_id, value, temp);
}

int Lua_Client::GetRawItemAC() {
	Lua_Safe_Call_Int();
	return self->GetRawItemAC();
}

uint32 Lua_Client::AccountID() {
	Lua_Safe_Call_Int();
	return self->AccountID();
}

const char *Lua_Client::AccountName() {
	Lua_Safe_Call_String();
	return self->AccountName();
}

int Lua_Client::Admin() {
	Lua_Safe_Call_Bool();
	return self->Admin();
}

uint32 Lua_Client::CharacterID() {
	Lua_Safe_Call_Int();
	return self->CharacterID();
}

int Lua_Client::GuildRank() {
	Lua_Safe_Call_Int();
	return self->GuildRank();
}

uint32 Lua_Client::GuildID() {
	Lua_Safe_Call_Int();
	return self->GuildID();
}

const char *Lua_Client::GetGuildName() {
	Lua_Safe_Call_String();
	return guild_mgr.GetGuildName(self->GuildID());
}

int Lua_Client::GetFace() {
	Lua_Safe_Call_Int();
	return self->GetFace();
}

bool Lua_Client::TakeMoneyFromPP(uint64 copper) {
	Lua_Safe_Call_Bool();
	return self->TakeMoneyFromPP(copper);
}

bool Lua_Client::TakeMoneyFromPP(uint64 copper, bool update_client) {
	Lua_Safe_Call_Bool();
	return self->TakeMoneyFromPP(copper, update_client);
}

void Lua_Client::AddMoneyToPP(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, bool update_client) {
	Lua_Safe_Call_Void();
	self->AddMoneyToPP(copper, silver, gold, platinum, update_client);
}

bool Lua_Client::TGB() {
	Lua_Safe_Call_Bool();
	return self->TGB();
}

int Lua_Client::GetSkillPoints() {
	Lua_Safe_Call_Int();
	return self->GetSkillPoints();
}

void Lua_Client::SetSkillPoints(int skill) {
	Lua_Safe_Call_Void();
	self->SetSkillPoints(skill);
}

void Lua_Client::IncreaseSkill(int skill_id) {
	Lua_Safe_Call_Void();
	self->IncreaseSkill(skill_id);
}

void Lua_Client::IncreaseSkill(int skill_id, int value) {
	Lua_Safe_Call_Void();
	self->IncreaseSkill(skill_id, value);
}

void Lua_Client::IncreaseLanguageSkill(int skill_id) {
	Lua_Safe_Call_Void();
	self->IncreaseLanguageSkill(skill_id);
}

void Lua_Client::IncreaseLanguageSkill(int skill_id, int value) {
	Lua_Safe_Call_Void();
	self->IncreaseLanguageSkill(skill_id, value);
}

int Lua_Client::GetRawSkill(int skill_id) {
	Lua_Safe_Call_Int();
	return self->GetRawSkill(static_cast<EQ::skills::SkillType>(skill_id));
}

bool Lua_Client::HasSkill(int skill_id) {
	Lua_Safe_Call_Bool();
	return self->HasSkill(static_cast<EQ::skills::SkillType>(skill_id));
}

bool Lua_Client::CanHaveSkill(int skill_id) {
	Lua_Safe_Call_Bool();
	return self->CanHaveSkill(static_cast<EQ::skills::SkillType>(skill_id));
}

void Lua_Client::SetSkill(int skill_id, int value) {
	Lua_Safe_Call_Void();
	self->SetSkill(static_cast<EQ::skills::SkillType>(skill_id), value);
}

void Lua_Client::AddSkill(int skill_id, int value) {
	Lua_Safe_Call_Void();
	self->AddSkill(static_cast<EQ::skills::SkillType>(skill_id), value);
}

void Lua_Client::CheckSpecializeIncrease(int spell_id) {
	Lua_Safe_Call_Void();
	self->CheckSpecializeIncrease(spell_id);
}

void Lua_Client::CheckIncreaseSkill(int skill_id, Lua_Mob target) {
	Lua_Safe_Call_Void();
	self->CheckIncreaseSkill(static_cast<EQ::skills::SkillType>(skill_id), target);
}

void Lua_Client::CheckIncreaseSkill(int skill_id, Lua_Mob target, int chance_mod) {
	Lua_Safe_Call_Void();
	self->CheckIncreaseSkill(static_cast<EQ::skills::SkillType>(skill_id), target, chance_mod);
}

void Lua_Client::SetLanguageSkill(int language, int value) {
	Lua_Safe_Call_Void();
	self->SetLanguageSkill(language, value);
}

int Lua_Client::MaxSkill(int skill_id) {
	Lua_Safe_Call_Int();
	return self->MaxSkill(static_cast<EQ::skills::SkillType>(skill_id));
}

bool Lua_Client::IsMedding() {
	Lua_Safe_Call_Bool();
	return self->IsMedding();
}

int Lua_Client::GetDuelTarget() {
	Lua_Safe_Call_Int();
	return self->GetDuelTarget();
}

bool Lua_Client::IsDueling() {
	Lua_Safe_Call_Bool();
	return self->IsDueling();
}

void Lua_Client::SetDuelTarget(int c) {
	Lua_Safe_Call_Void();
	self->SetDuelTarget(c);
}

void Lua_Client::SetDueling(bool v) {
	Lua_Safe_Call_Void();
	self->SetDueling(v);
}

void Lua_Client::ResetAA() {
	Lua_Safe_Call_Void();
	self->ResetAA();
}

void Lua_Client::MemSpell(int spell_id, int slot) {
	Lua_Safe_Call_Void();
	self->MemSpell(spell_id, slot);
}

void Lua_Client::MemSpell(int spell_id, int slot, bool update_client) {
	Lua_Safe_Call_Void();
	self->MemSpell(spell_id, slot, update_client);
}

void Lua_Client::UnmemSpell(int slot) {
	Lua_Safe_Call_Void();
	self->UnmemSpell(slot);
}

void Lua_Client::UnmemSpell(int slot, bool update_client) {
	Lua_Safe_Call_Void();
	self->UnmemSpell(slot, update_client);
}

void Lua_Client::UnmemSpellAll() {
	Lua_Safe_Call_Void();
	self->UnmemSpellAll();
}

void Lua_Client::UnmemSpellAll(bool update_client) {
	Lua_Safe_Call_Void();
	self->UnmemSpellAll(update_client);
}

luabind::object Lua_Client::GetMemmedSpells(lua_State* L) {
	auto lua_table = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType*>(d_);
		auto memmed_spells = self->GetMemmedSpells();
		int index = 0;
		for (auto spell_id : memmed_spells) {
			lua_table[index] = spell_id;
			index++;
		}
	}
	return lua_table;
}

luabind::object Lua_Client::GetScribeableSpells(lua_State* L) {
	auto lua_table = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType*>(d_);
		auto scribeable_spells = self->GetScribeableSpells();
		int index = 0;
		for (auto spell_id : scribeable_spells) {
			lua_table[index] = spell_id;
			index++;
		}
	}
	return lua_table;
}

luabind::object Lua_Client::GetScribeableSpells(lua_State* L, uint8 min_level) {
	auto lua_table = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType*>(d_);
		auto scribeable_spells = self->GetScribeableSpells(min_level);
		int index = 0;
		for (auto spell_id : scribeable_spells) {
			lua_table[index] = spell_id;
			index++;
		}
	}
	return lua_table;
}

luabind::object Lua_Client::GetScribeableSpells(lua_State* L, uint8 min_level, uint8 max_level) {
	auto lua_table = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType*>(d_);
		auto scribeable_spells = self->GetScribeableSpells(min_level, max_level);
		int index = 0;
		for (auto spell_id : scribeable_spells) {
			lua_table[index] = spell_id;
			index++;
		}
	}
	return lua_table;
}

luabind::object Lua_Client::GetScribedSpells(lua_State* L) {
	auto lua_table = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType*>(d_);
		auto scribed_spells = self->GetScribedSpells();
		int index = 0;
		for (auto spell_id : scribed_spells) {
			lua_table[index] = spell_id;
			index++;
		}
	}
	return lua_table;
}

void Lua_Client::ScribeSpell(int spell_id, int slot) {
	Lua_Safe_Call_Void();
	self->ScribeSpell(spell_id, slot);
}

void Lua_Client::ScribeSpell(int spell_id, int slot, bool update_client) {
	Lua_Safe_Call_Void();
	self->ScribeSpell(spell_id, slot, update_client);
}

uint16 Lua_Client::ScribeSpells(uint8 min_level, uint8 max_level) {
	Lua_Safe_Call_Int();
	return self->ScribeSpells(min_level, max_level);
}

void Lua_Client::UnscribeSpell(int slot) {
	Lua_Safe_Call_Void();
	self->UnscribeSpell(slot);
}

void Lua_Client::UnscribeSpell(int slot, bool update_client) {
	Lua_Safe_Call_Void();
	self->UnscribeSpell(slot, update_client);
}

void Lua_Client::UnscribeSpellAll() {
	Lua_Safe_Call_Void();
	self->UnscribeSpellAll();
}

void Lua_Client::UnscribeSpellAll(bool update_client) {
	Lua_Safe_Call_Void();
	self->UnscribeSpellAll(update_client);
}

bool Lua_Client::IsSitting() {
	Lua_Safe_Call_Bool();
	return self->IsSitting();
}

void Lua_Client::SetFeigned(bool v) {
	Lua_Safe_Call_Void();
	self->SetFeigned(v);
}

bool Lua_Client::GetFeigned() {
	Lua_Safe_Call_Bool();
	return self->IsFeigned();
}

bool Lua_Client::IsFeigned() {
	Lua_Safe_Call_Bool();
	return self->IsFeigned();
}

bool Lua_Client::AutoSplitEnabled() {
	Lua_Safe_Call_Bool();
	return self->AutoSplitEnabled();
}

void Lua_Client::SetHorseId(int id) {
	Lua_Safe_Call_Void();
	self->SetHorseId(id);
}

int Lua_Client::GetHorseId() {
	Lua_Safe_Call_Int();
	return self->GetHorseId();
}

void Lua_Client::NukeItem(uint32 item_num) {
	Lua_Safe_Call_Void();
	self->NukeItem(item_num, 0xFF);
}

void Lua_Client::NukeItem(uint32 item_num, int where_to_check) {
	Lua_Safe_Call_Void();
	self->NukeItem(item_num, where_to_check);
}

void Lua_Client::SetMaterial(int slot_id, uint32 item_id) {
	Lua_Safe_Call_Void();
	self->SetMaterial(slot_id, item_id);
}

int Lua_Client::GetItemIDAt(int slot_id) {
	Lua_Safe_Call_Int();
	return self->GetItemIDAt(slot_id);
}

bool Lua_Client::FindOnCursor(uint32 item_id) {
	Lua_Safe_Call_Bool();
	return self->FindOnCursor(item_id);
}

void Lua_Client::DeleteItemInInventory(int slot_id, int quantity) {
	Lua_Safe_Call_Void();
	self->DeleteItemInInventory(slot_id, quantity);
}

void Lua_Client::DeleteItemInInventory(int slot_id, int quantity, bool update_client) {
	Lua_Safe_Call_Void();
	self->DeleteItemInInventory(slot_id, quantity, update_client);
}

void Lua_Client::SummonItem(uint32 item_id) {
	Lua_Safe_Call_Void();
	self->SummonItem(item_id, 0, EQ::legacy::SLOT_QUEST);
}

void Lua_Client::SummonItem(uint32 item_id, int charges) {
	Lua_Safe_Call_Void();
	self->SummonItem(item_id, charges, EQ::legacy::SLOT_QUEST);
}

void Lua_Client::SummonItem(uint32 item_id, int charges, int to_slot) {
	Lua_Safe_Call_Void();
	self->SummonItem(item_id, charges, to_slot);
}

void Lua_Client::SummonItem(uint32 item_id, int charges, int to_slot, bool force_charges) {
	Lua_Safe_Call_Void();
	self->SummonItem(item_id, charges, to_slot, force_charges);
}

void Lua_Client::SummonCursorItem(uint32 item_id) {
	Lua_Safe_Call_Void();
	self->SummonItem(item_id, 0, EQ::invslot::slotCursor);
}

void Lua_Client::SummonCursorItem(uint32 item_id, int charges) {
	Lua_Safe_Call_Void();
	self->SummonItem(item_id, charges, EQ::invslot::slotCursor);
}

void Lua_Client::DropItem(int slot_id) {
	Lua_Safe_Call_Void();
	self->DropItem(slot_id);
}

void Lua_Client::BreakInvis() {
	Lua_Safe_Call_Void();
	self->CommonBreakInvisible();
}

void Lua_Client::LeaveGroup() {
	Lua_Safe_Call_Void();
	self->LeaveGroup();
}

bool Lua_Client::IsGrouped() {
	Lua_Safe_Call_Bool();
	return self->IsGrouped();
}

bool Lua_Client::IsRaidGrouped() {
	Lua_Safe_Call_Bool();
	return self->IsRaidGrouped();
}

bool Lua_Client::Hungry() {
	Lua_Safe_Call_Bool();
	return self->Hungry();
}

bool Lua_Client::Thirsty() {
	Lua_Safe_Call_Bool();
	return self->Thirsty();
}

int Lua_Client::GetInstrumentMod(int spell_id) {
	Lua_Safe_Call_Int();
	return self->GetInstrumentMod(spell_id);
}

bool Lua_Client::DecreaseByID(uint32 type, int amt) {
	Lua_Safe_Call_Bool();
	return self->DecreaseByID(type, amt);
}

void Lua_Client::Escape() {
	Lua_Safe_Call_Void();
	self->Escape();
}

void Lua_Client::GoFish() {
	Lua_Safe_Call_Void();
	self->GoFish();
}

void Lua_Client::ForageItem() {
	Lua_Safe_Call_Void();
	self->ForageItem();
}

void Lua_Client::ForageItem(bool guarantee) {
	Lua_Safe_Call_Void();
	self->ForageItem(guarantee);
}

float Lua_Client::CalcPriceMod(Lua_Mob other, bool reverse) {
	Lua_Safe_Call_Real();
	return self->CalcPriceMod(other, reverse);
}

void Lua_Client::ResetTrade() {
	Lua_Safe_Call_Void();
	self->ResetTrade();
}

bool Lua_Client::UseDiscipline(int spell_id) {
	Lua_Safe_Call_Bool();
	return self->UseDiscipline(spell_id);
}

int Lua_Client::GetCharacterFactionLevel(int faction_id) {
	Lua_Safe_Call_Int();
	return self->GetCharacterFactionLevel(faction_id);
}

void Lua_Client::SetZoneFlag(int zone_id) {
	Lua_Safe_Call_Void();
	self->SetZoneFlag(zone_id);
}

void Lua_Client::ClearZoneFlag(int zone_id) {
	Lua_Safe_Call_Void();
	self->ClearZoneFlag(zone_id);
}

bool Lua_Client::HasZoneFlag(int zone_id) {
	Lua_Safe_Call_Bool();
	return self->HasZoneFlag(zone_id);
}

void Lua_Client::SendZoneFlagInfo(Lua_Client to) {
	Lua_Safe_Call_Void();
	self->SendZoneFlagInfo(to);
}

void Lua_Client::SetAATitle(const char *title) {
	Lua_Safe_Call_Void();
	self->SetAATitle(title);
}

int Lua_Client::GetClientVersion() {
	Lua_Safe_Call_Int();
	return self->ClientVersion();
}

uint32 Lua_Client::GetClientVersionBit() {
	Lua_Safe_Call_Int();
	return self->ClientVersionBit();
}

void Lua_Client::SetTitleSuffix(const char *text) {
	Lua_Safe_Call_Void();
	self->SetTitleSuffix(text);
}

void Lua_Client::SetAAPoints(int points) {
	Lua_Safe_Call_Void();
	self->SetAAPoints(points);
}

int Lua_Client::GetAAPoints() {
	Lua_Safe_Call_Int();
	return self->GetAAPoints();
}

int Lua_Client::GetSpentAA() {
	Lua_Safe_Call_Int();
	return self->GetSpentAA();
}

void Lua_Client::AddAAPoints(int points) {
	Lua_Safe_Call_Void();
	self->AddAAPoints(points);
}

void Lua_Client::RefundAA() {
	Lua_Safe_Call_Void();
	self->RefundAA();
}

void Lua_Client::ExpendAATimer(int aaid) {
	Lua_Safe_Call_Void();
	self->ExpendAATimer(aaid);
}

void Lua_Client::SetHardcore(int hardcore) {
	Lua_Safe_Call_Void();
	self->SetHardcore(hardcore);
}

int Lua_Client::IsHardcore() {
	Lua_Safe_Call_Int();
	return self->IsHardcore();
}

void Lua_Client::SetSelfFound(int self_found) {
	Lua_Safe_Call_Void();
	self->SetSelfFound(self_found);
}

int Lua_Client::IsSelfFound() {
	Lua_Safe_Call_Int();
	return self->IsSelfFound();
}

void Lua_Client::SetSoloOnly(int solo_only) {
	Lua_Safe_Call_Void();
	self->SetSoloOnly(solo_only);
}

int Lua_Client::IsSoloOnly() {
	Lua_Safe_Call_Int();
	return self->IsSoloOnly();
}

void Lua_Client::ClearPlayerInfoAndGrantStartingItems()
{
	Lua_Safe_Call_Void();
	self->ClearPlayerInfoAndGrantStartingItems();
}

void Lua_Client::ResetPlayerForNewGamePlus(uint8 new_level, uint8 new_level2, bool reset_skill_points)
{
	Lua_Safe_Call_Void();
	self->ResetPlayerForNewGamePlus(new_level, new_level2, reset_skill_points);
}

int Lua_Client::GetModCharacterFactionLevel(int faction) {
	Lua_Safe_Call_Int();
	return self->GetModCharacterFactionLevel(faction);
}

int Lua_Client::GetStartZone() {
	Lua_Safe_Call_Int();
	return self->GetStartZone();
}

void Lua_Client::KeyRingAdd(uint32 item) {
	Lua_Safe_Call_Void();
	self->KeyRingAdd(item);
}

bool Lua_Client::KeyRingCheck(uint32 item) {
	Lua_Safe_Call_Bool();
	return self->KeyRingCheck(item);
}

void Lua_Client::QuestReadBook(const char *text, int type) {
	Lua_Safe_Call_Void();
	self->QuestReadBook(text, type);
}

void Lua_Client::SendOPTranslocateConfirm(Lua_Mob caster, int spell_id) {
	Lua_Safe_Call_Void();
	self->SendOPTranslocateConfirm(caster, spell_id);
}

uint32 Lua_Client::GetIP() {
	Lua_Safe_Call_Int();
	return self->GetIP();
}

void Lua_Client::AddLevelBasedExp(int exp_pct) {
	Lua_Safe_Call_Void();
	
	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (self->GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}

	self->AddLevelBasedExp(exp_pct);
}

void Lua_Client::AddLevelBasedExp(int exp_pct, int max_level) {
	Lua_Safe_Call_Void();

	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (self->GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}

	self->AddLevelBasedExp(exp_pct, max_level);
}

void Lua_Client::IncrementAA(int aa) {
	Lua_Safe_Call_Void();
	self->IncrementAA(aa);
}

int Lua_Client::GetNextAvailableSpellBookSlot() {
	Lua_Safe_Call_Int();
	return self->GetNextAvailableSpellBookSlot();
}

int Lua_Client::GetNextAvailableSpellBookSlot(int start) {
	Lua_Safe_Call_Int();
	return self->GetNextAvailableSpellBookSlot(start);
}

int Lua_Client::FindSpellBookSlotBySpellID(int spell_id) {
	Lua_Safe_Call_Int();
	return self->FindSpellBookSlotBySpellID(spell_id);
}

int Lua_Client::GetCorpseCount() {
	Lua_Safe_Call_Int();
	return self->GetCorpseCount();
}

int Lua_Client::GetCorpseID(int corpse) {
	Lua_Safe_Call_Int();
	return self->GetCorpseID(corpse);
}

int Lua_Client::GetCorpseItemAt(int corpse, int slot) {
	Lua_Safe_Call_Int();
	return self->GetCorpseItemAt(corpse, slot);
}

void Lua_Client::Freeze() {
	Lua_Safe_Call_Void();
	self->SendAppearancePacket(AppearanceType::Animation, Animation::Freeze);
}

void Lua_Client::UnFreeze() {
	Lua_Safe_Call_Void();
	self->SendAppearancePacket(AppearanceType::Animation, Animation::Standing);
}

int Lua_Client::GetAggroCount() {
	Lua_Safe_Call_Int();
	return self->GetAggroCount();
}

uint64 Lua_Client::GetCarriedMoney() {
	Lua_Safe_Call_Int();
	return self->GetCarriedMoney();
}

uint64 Lua_Client::GetAllMoney() {
	Lua_Safe_Call_Int();
	return self->GetAllMoney();
}

void Lua_Client::Signal(uint32 id) {
	Lua_Safe_Call_Void();
	self->Signal(id);
}

bool Lua_Client::HasSpellScribed(int spell_id) {
	Lua_Safe_Call_Bool();
	return self->HasSpellScribed(spell_id);
}

void Lua_Client::SetAccountFlag(std::string flag, std::string val) {
	Lua_Safe_Call_Void();
	self->SetAccountFlag(flag, val);
}

std::string Lua_Client::GetAccountFlag(std::string flag) {
	Lua_Safe_Call_String();
	return self->GetAccountFlag(flag);
}

Lua_Group Lua_Client::GetGroup() {
	Lua_Safe_Call_Class(Lua_Group);
	return self->GetGroup();
}

Lua_Raid Lua_Client::GetRaid() {
	Lua_Safe_Call_Class(Lua_Raid);
	return self->GetRaid();
}

luabind::object Lua_Client::GetRaidOrGroupOrSelf(lua_State *L)
{
	auto t = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType *>(d_);
		auto l = self->GetRaidOrGroupOrSelf();
		int i = 1;
		for (const auto &e : l) {
			t[i] = Lua_Mob(e);
			i++;
		}
	}

	return t;
}
luabind::object Lua_Client::GetRaidOrGroupOrSelf(lua_State *L, bool clients_only)
{
	auto t = luabind::newtable(L);
	if (d_) {
		auto self = reinterpret_cast<NativeType *>(d_);
		auto l = self->GetRaidOrGroupOrSelf(clients_only);
		int i = 1;
		for (const auto &e : l) {
			t[i] = Lua_Mob(e);
			i++;
		}
	}

	return t;
}


bool Lua_Client::PutItemInInventory(int slot_id, Lua_ItemInst inst) {
	Lua_Safe_Call_Bool();
	EQ::ItemInstance *rinst = inst;
	return self->PutItemInInventory(slot_id, *rinst, true);
}

bool Lua_Client::PushItemOnCursor(Lua_ItemInst inst) {
	Lua_Safe_Call_Bool();
	EQ::ItemInstance *rinst = inst;
	return self->PushItemOnCursor(*rinst, true);
}

bool Lua_Client::PushItemOnCursorWithoutQueue(Lua_ItemInst inst) {
	Lua_Safe_Call_Bool();
	EQ::ItemInstance *rinst = inst;
	return self->PushItemOnCursorWithoutQueue(rinst, true);
}

Lua_Inventory Lua_Client::GetInventory() {
	Lua_Safe_Call_Class(Lua_Inventory);
	return &self->GetInv();
}

void Lua_Client::QueuePacket(Lua_Packet app) {
	Lua_Safe_Call_Void();
	self->QueuePacket(app);
}

void Lua_Client::QueuePacket(Lua_Packet app, bool ack_req) {
	Lua_Safe_Call_Void();
	self->QueuePacket(app, ack_req);
}

void Lua_Client::QueuePacket(Lua_Packet app, bool ack_req, int client_connection_status) {
	Lua_Safe_Call_Void();
	self->QueuePacket(app, ack_req, static_cast<Mob::CLIENT_CONN_STATUS>(client_connection_status));
}

void Lua_Client::QueuePacket(Lua_Packet app, bool ack_req, int client_connection_status, int filter) {
	Lua_Safe_Call_Void();
	self->QueuePacket(app, ack_req, static_cast<Mob::CLIENT_CONN_STATUS>(client_connection_status), static_cast<eqFilterType>(filter));
}

int Lua_Client::GetHunger() {
	Lua_Safe_Call_Int();
	return self->GetHunger();
}

int Lua_Client::GetThirst() {
	Lua_Safe_Call_Int();
	return self->GetThirst();
}

void Lua_Client::SetHunger(int in_hunger) {
	Lua_Safe_Call_Void();
	self->SetHunger(in_hunger);
}

void Lua_Client::SetThirst(int in_thirst) {
	Lua_Safe_Call_Void();
	self->SetThirst(in_thirst);
}

void Lua_Client::SetConsumption(int in_hunger, int in_thirst) {
	Lua_Safe_Call_Void();
	self->SetConsumption(in_hunger, in_thirst);
}

int Lua_Client::GetBoatID() 
{
	Lua_Safe_Call_Int();
	return self->GetBoatNPCID();
}

void Lua_Client::SetBoatID(uint32 in_boatid) 
{
	Lua_Safe_Call_Void();
	self->SetBoatID(in_boatid);
}

char* Lua_Client::GetBoatName() 
{
	Lua_Safe_Call_Int();
	return self->GetBoatName();
}

void Lua_Client::SetBoatName(const char* in_boatname) 
{
	Lua_Safe_Call_Void();
	self->SetBoatName(in_boatname);
}

void Lua_Client::QuestReward(Lua_Mob target) {
	Lua_Safe_Call_Void();
	self->QuestReward(target);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper, int32 silver) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper, silver);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper, silver, gold);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper, silver, gold, platinum);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper, silver, gold, platinum, itemid);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid, int32 exp) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper, silver, gold, platinum, itemid, exp);
}

void Lua_Client::QuestReward(Lua_Mob target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid, int32 exp, bool faction) {
	Lua_Safe_Call_Void();
	self->QuestReward(target, copper, silver, gold, platinum, itemid, exp, faction);
}

void Lua_Client::QuestReward(Lua_Mob target, luabind::adl::object reward) {
	Lua_Safe_Call_Void();

	if (luabind::type(reward) != LUA_TTABLE) {
		return;
	}

	QuestReward_Struct quest_reward;
	quest_reward.mob_id = 0;
	quest_reward.target_id = self->GetID();
	quest_reward.copper = luabind::type(reward["copper"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["copper"]) : 0;
	quest_reward.silver = luabind::type(reward["silver"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["silver"]) : 0;
	quest_reward.gold = luabind::type(reward["gold"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["gold"]) : 0;
	quest_reward.platinum = luabind::type(reward["platinum"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["platinum"]) : 0;
	quest_reward.exp_reward = luabind::type(reward["exp"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["exp"]) : 0;
	quest_reward.faction = luabind::type(reward["factionid"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["factionid"]) : 0;
	quest_reward.faction_mod = luabind::type(reward["factionmod"]) != LUA_TNIL ? luabind::object_cast<uint32>(reward["factionmod"]) : 0;
	std::fill(std::begin(quest_reward.item_id), std::end(quest_reward.item_id), -1);


	auto item_id = reward["itemid"];
	if (luabind::type(item_id) != LUA_TNIL) {
			quest_reward.item_id[0] = luabind::object_cast<uint32>(item_id);
	}

	// if you define both an itemid and items table, the itemid is thrown away
	// should we error?
	auto items = reward["items"];
	if (luabind::type(items) == LUA_TTABLE) {
			// assume they defined a compatible table
			for (int i = 1; i <= QUESTREWARD_COUNT; ++i) {
				auto item = items[i];
				int cur_value = -1;
				if (luabind::type(item) != LUA_TNIL) {
						cur_value = luabind::object_cast<uint32>(item);
				}
				else {
					break;
				}
				quest_reward.item_id[i - 1] = cur_value;
			}
	}

	self->QuestReward(target, quest_reward);
}

int Lua_Client::GetHandToHandDamage() {
	Lua_Safe_Call_Int();
	return self->GetHandToHandDamage();
}

int Lua_Client::GetHandToHandDelay() {
	Lua_Safe_Call_Int();
	return self->GetHandToHandDelay();
}

void Lua_Client::PermaGender(uint32 in_Gender) {
	Lua_Safe_Call_Void();
	self->PermaGender(in_Gender);
}

bool Lua_Client::SendGMCommand(std::string message) {
	Lua_Safe_Call_Bool();
	return self->SendGMCommand(message);
}

bool Lua_Client::SendGMCommand(std::string message, bool ignore_status) {
	Lua_Safe_Call_Bool();
	return self->SendGMCommand(message, ignore_status);
}

void Lua_Client::SetClientMaxLevel(uint8 max_level) {
	Lua_Safe_Call_Void();
	self->SetClientMaxLevel(max_level);
}

uint8 Lua_Client::GetClientMaxLevel() {
	Lua_Safe_Call_Int();
	return self->GetClientMaxLevel();
}

uint8 Lua_Client::GetSkillTrainLevel(int skill_id)
{
	Lua_Safe_Call_Int();
	return self->GetSkillTrainLevel(static_cast<EQ::skills::SkillType>(skill_id), self->GetClass());
}

void Lua_Client::MaxSkills()
{
	Lua_Safe_Call_Void();
	self->MaxSkills();
}

luabind::scope lua_register_client() {
	return luabind::class_<Lua_Client, Lua_Mob>("Client")
		.def(luabind::constructor<>())
		.def("SendSound", (void(Lua_Client::*)(void))&Lua_Client::SendSound)
		.def("Save", (void(Lua_Client::*)(void))&Lua_Client::Save)
		.def("Save", (void(Lua_Client::*)(int))&Lua_Client::Save)
		.def("Connected", (bool(Lua_Client::*)(void))&Lua_Client::Connected)
		.def("InZone", (bool(Lua_Client::*)(void))&Lua_Client::InZone)
		.def("Kick", (void(Lua_Client::*)(void))&Lua_Client::Kick)
		.def("Disconnect", (void(Lua_Client::*)(void))&Lua_Client::Disconnect)
		.def("IsLD", (bool(Lua_Client::*)(void))&Lua_Client::IsLD)
		.def("WorldKick", (void(Lua_Client::*)(void))&Lua_Client::WorldKick)
		.def("GetAnon", (bool(Lua_Client::*)(void))&Lua_Client::GetAnon)
		.def("Duck", (void(Lua_Client::*)(void))&Lua_Client::Duck)
		.def("Stand", (void(Lua_Client::*)(void))&Lua_Client::Stand)
		.def("SetGM", (void(Lua_Client::*)(bool))&Lua_Client::SetGM)
		.def("SetPVP", (void(Lua_Client::*)(bool))&Lua_Client::SetPVP)
		.def("GetPVP", (bool(Lua_Client::*)(void))&Lua_Client::GetPVP)
		.def("GetGM", (bool(Lua_Client::*)(void))&Lua_Client::GetGM)
		.def("SetBaseClass", (void(Lua_Client::*)(int))&Lua_Client::SetBaseClass)
		.def("SetBaseRace", (void(Lua_Client::*)(int))&Lua_Client::SetBaseRace)
		.def("SetBaseGender", (void(Lua_Client::*)(int))&Lua_Client::SetBaseGender)
		.def("HasCharacterCreateCombination", (bool(Lua_Client::*)(uint16,uint16,uint16,int))&Lua_Client::HasCharacterCreateCombination)
		.def("AddCharacterCreateCombinationUnlock", (void(Lua_Client::*)(uint16,uint16,uint16,int,uint16,float,float,float,float))&Lua_Client::AddCharacterCreateCombinationUnlock)
		.def("PermaStats", (bool(Lua_Client::*)(uint16,uint16,uint16,uint16,uint16,uint16,uint16,bool))&Lua_Client::PermaStats)
		.def("PermaRace", (bool(Lua_Client::*)(uint16,uint16,int,uint16,uint16,uint16,uint16,uint16,uint16,uint16))&Lua_Client::PermaRace)
		.def("ForcePermaRace", (bool(Lua_Client::*)(uint16, uint16, int, uint16, uint16, uint16, uint16, uint16, uint16, uint16))&Lua_Client::ForcePermaRace)
		.def("PermaClass", (bool(Lua_Client::*)(uint16, uint16, int, uint16, uint16, uint16, uint16, uint16, uint16, uint16)) & Lua_Client::PermaClass)
		.def("ForcePermaClass", (bool(Lua_Client::*)(uint16, uint16, int, uint16, uint16, uint16, uint16, uint16, uint16, uint16)) & Lua_Client::ForcePermaClass)
		.def("GetBaseFace", (int(Lua_Client::*)(void))&Lua_Client::GetBaseFace)
		.def("GetLanguageSkill", (int(Lua_Client::*)(int))&Lua_Client::GetLanguageSkill)
		.def("GetLastName", (const char *(Lua_Client::*)(void))&Lua_Client::GetLastName)
		.def("GetProfileLastName", (const char*(Lua_Client::*)(void))&Lua_Client::GetProfileLastName)
		.def("GetBaseSTR", (int(Lua_Client::*)(void))&Lua_Client::GetBaseSTR)
		.def("GetBaseSTA", (int(Lua_Client::*)(void))&Lua_Client::GetBaseSTA)
		.def("GetBaseCHA", (int(Lua_Client::*)(void))&Lua_Client::GetBaseCHA)
		.def("GetBaseDEX", (int(Lua_Client::*)(void))&Lua_Client::GetBaseDEX)
		.def("GetBaseINT", (int(Lua_Client::*)(void))&Lua_Client::GetBaseINT)
		.def("GetBaseAGI", (int(Lua_Client::*)(void))&Lua_Client::GetBaseAGI)
		.def("GetBaseWIS", (int(Lua_Client::*)(void))&Lua_Client::GetBaseWIS)
		.def("GetWeight", (int(Lua_Client::*)(void))&Lua_Client::GetWeight)
		.def("GetEXP", (uint32(Lua_Client::*)(void))&Lua_Client::GetEXP)
		.def("GetAAExp", (uint32(Lua_Client::*)(void))&Lua_Client::GetAAExp)
		.def("SetDeity", (void(Lua_Client::*)(int))&Lua_Client::SetDeity)
		.def("AddEXP", (void(Lua_Client::*)(uint32,int,Lua_Mob))&Lua_Client::AddEXP)
		.def("SetEXP", (void(Lua_Client::*)(uint32,uint32))&Lua_Client::SetEXP)
		.def("SetEXP", (void(Lua_Client::*)(uint32,uint32,bool))&Lua_Client::SetEXP)
		.def("AddEXPPercent", (void(Lua_Client::*)(uint8,uint8))&Lua_Client::AddEXPPercent)
		.def("SetBindPoint", (void(Lua_Client::*)(void))&Lua_Client::SetBindPoint)
		.def("SetBindPoint", (void(Lua_Client::*)(int))&Lua_Client::SetBindPoint)
		.def("SetBindPoint", (void(Lua_Client::*)(int,float))&Lua_Client::SetBindPoint)
		.def("SetBindPoint", (void(Lua_Client::*)(int,float,float))&Lua_Client::SetBindPoint)
		.def("SetBindPoint", (void(Lua_Client::*)(int,float,float, float))&Lua_Client::SetBindPoint)
		.def("SetBindPoint", (void(Lua_Client::*)(int, float, float, float, float)) & Lua_Client::SetBindPoint)
		.def("GetBindX", (float(Lua_Client::*)(void))&Lua_Client::GetBindX)
		.def("GetBindX", (float(Lua_Client::*)(int))&Lua_Client::GetBindX)
		.def("GetBindY", (float(Lua_Client::*)(void))&Lua_Client::GetBindY)
		.def("GetBindY", (float(Lua_Client::*)(int))&Lua_Client::GetBindY)
		.def("GetBindZ", (float(Lua_Client::*)(void))&Lua_Client::GetBindZ)
		.def("GetBindZ", (float(Lua_Client::*)(int))&Lua_Client::GetBindZ)
		.def("GetBindHeading", (float(Lua_Client::*)(void))&Lua_Client::GetBindHeading)
		.def("GetBindHeading", (float(Lua_Client::*)(int))&Lua_Client::GetBindHeading)
		.def("GetBindZoneID", (uint32(Lua_Client::*)(void))&Lua_Client::GetBindZoneID)
		.def("GetBindZoneID", (uint32(Lua_Client::*)(int))&Lua_Client::GetBindZoneID)
		.def("GetTimesRebirthed", (uint32(Lua_Client::*)(void))&Lua_Client::GetTimesRebirthed)
		.def("MovePC", (void(Lua_Client::*)(int,float,float,float,float))&Lua_Client::MovePC)
		.def("ChangeLastName", (void(Lua_Client::*)(const char *in))&Lua_Client::ChangeLastName)
		.def("GetFactionLevel", (int(Lua_Client::*)(uint32,uint32,uint32,uint32,uint32,Lua_NPC))&Lua_Client::GetFactionLevel)
		.def("GetFactionValue", (int(Lua_Client::*)(Lua_NPC))&Lua_Client::GetFactionValue)
		.def("SetFactionLevel", (void(Lua_Client::*)(uint32,uint32))&Lua_Client::SetFactionLevel)
		.def("SetFactionLevel2", (void(Lua_Client::*)(uint32,int,int,int))&Lua_Client::SetFactionLevel2)
		.def("GetRawItemAC", (int(Lua_Client::*)(void))&Lua_Client::GetRawItemAC)
		.def("AccountID", (uint32(Lua_Client::*)(void))&Lua_Client::AccountID)
		.def("AccountName", (const char *(Lua_Client::*)(void))&Lua_Client::AccountName)
		.def("Admin", (int(Lua_Client::*)(void))&Lua_Client::Admin)
		.def("CharacterID", (uint32(Lua_Client::*)(void))&Lua_Client::CharacterID)
		.def("GuildRank", (int(Lua_Client::*)(void))&Lua_Client::GuildRank)
		.def("GuildID", (uint32(Lua_Client::*)(void))&Lua_Client::GuildID)
		.def("GetGuildName", &Lua_Client::GetGuildName)
		.def("GetFace", (int(Lua_Client::*)(void))&Lua_Client::GetFace)
		.def("TakeMoneyFromPP", (bool(Lua_Client::*)(uint64))&Lua_Client::TakeMoneyFromPP)
		.def("TakeMoneyFromPP", (bool(Lua_Client::*)(uint64,bool))&Lua_Client::TakeMoneyFromPP)
		.def("AddMoneyToPP", (void(Lua_Client::*)(uint32,uint32,uint32,uint32,bool))&Lua_Client::AddMoneyToPP)
		.def("TGB", (bool(Lua_Client::*)(void))&Lua_Client::TGB)
		.def("GetSkillPoints", (int(Lua_Client::*)(void))&Lua_Client::GetSkillPoints)
		.def("SetSkillPoints", (void(Lua_Client::*)(int))&Lua_Client::SetSkillPoints)
		.def("IncreaseSkill", (void(Lua_Client::*)(int))&Lua_Client::IncreaseSkill)
		.def("IncreaseSkill", (void(Lua_Client::*)(int,int))&Lua_Client::IncreaseSkill)
		.def("IncreaseLanguageSkill", (void(Lua_Client::*)(int))&Lua_Client::IncreaseLanguageSkill)
		.def("IncreaseLanguageSkill", (void(Lua_Client::*)(int,int))&Lua_Client::IncreaseLanguageSkill)
		.def("GetRawSkill", (int(Lua_Client::*)(int))&Lua_Client::GetRawSkill)
		.def("HasSkill", (bool(Lua_Client::*)(int))&Lua_Client::HasSkill)
		.def("CanHaveSkill", (bool(Lua_Client::*)(int))&Lua_Client::CanHaveSkill)
		.def("SetSkill", (void(Lua_Client::*)(int,int))&Lua_Client::SetSkill)
		.def("AddSkill", (void(Lua_Client::*)(int,int))&Lua_Client::AddSkill)
		.def("CheckSpecializeIncrease", (void(Lua_Client::*)(int))&Lua_Client::CheckSpecializeIncrease)
		.def("CheckIncreaseSkill", (void(Lua_Client::*)(int,Lua_Mob))&Lua_Client::CheckIncreaseSkill)
		.def("CheckIncreaseSkill", (void(Lua_Client::*)(int,Lua_Mob,int))&Lua_Client::CheckIncreaseSkill)
		.def("SetLanguageSkill", (void(Lua_Client::*)(int,int))&Lua_Client::SetLanguageSkill)
		.def("MaxSkill", (int(Lua_Client::*)(int))&Lua_Client::MaxSkill)
		.def("IsMedding", (bool(Lua_Client::*)(void))&Lua_Client::IsMedding)
		.def("GetDuelTarget", (int(Lua_Client::*)(void))&Lua_Client::GetDuelTarget)
		.def("IsDueling", (bool(Lua_Client::*)(void))&Lua_Client::IsDueling)
		.def("SetDuelTarget", (void(Lua_Client::*)(int))&Lua_Client::SetDuelTarget)
		.def("SetDueling", (void(Lua_Client::*)(bool))&Lua_Client::SetDueling)
		.def("ResetAA", (void(Lua_Client::*)(void))&Lua_Client::ResetAA)
		.def("MemSpell", (void(Lua_Client::*)(int,int))&Lua_Client::MemSpell)
		.def("MemSpell", (void(Lua_Client::*)(int,int,bool))&Lua_Client::MemSpell)
		.def("UnmemSpell", (void(Lua_Client::*)(int))&Lua_Client::UnmemSpell)
		.def("UnmemSpell", (void(Lua_Client::*)(int,bool))&Lua_Client::UnmemSpell)
		.def("UnmemSpellAll", (void(Lua_Client::*)(void))&Lua_Client::UnmemSpellAll)
		.def("UnmemSpellAll", (void(Lua_Client::*)(bool))&Lua_Client::UnmemSpellAll)
		.def("GetMemmedSpells", (luabind::object(Lua_Client::*)(lua_State* L))& Lua_Client::GetMemmedSpells)
		.def("GetScribedSpells", (luabind::object(Lua_Client::*)(lua_State* L))& Lua_Client::GetScribedSpells)
		.def("GetScribeableSpells", (luabind::object(Lua_Client::*)(lua_State* L))& Lua_Client::GetScribeableSpells)
		.def("GetScribeableSpells", (luabind::object(Lua_Client::*)(lua_State* L, uint8))& Lua_Client::GetScribeableSpells)
		.def("GetScribeableSpells", (luabind::object(Lua_Client::*)(lua_State* L, uint8, uint8))& Lua_Client::GetScribeableSpells)
		.def("ScribeSpell", (void(Lua_Client::*)(int,int))&Lua_Client::ScribeSpell)
		.def("ScribeSpell", (void(Lua_Client::*)(int,int,bool))&Lua_Client::ScribeSpell)
		.def("UnscribeSpell", (void(Lua_Client::*)(int))&Lua_Client::UnscribeSpell)
		.def("UnscribeSpell", (void(Lua_Client::*)(int,bool))&Lua_Client::UnscribeSpell)
		.def("UnscribeSpellAll", (void(Lua_Client::*)(void))&Lua_Client::UnscribeSpellAll)
		.def("UnscribeSpellAll", (void(Lua_Client::*)(bool))&Lua_Client::UnscribeSpellAll)
		.def("IsSitting", (bool(Lua_Client::*)(void))&Lua_Client::IsSitting)
		.def("SetFeigned", (void(Lua_Client::*)(bool))&Lua_Client::SetFeigned)
		.def("GetFeigned", (bool(Lua_Client::*)(void))&Lua_Client::GetFeigned)
		.def("IsFeigned", (bool(Lua_Client::*)(void))&Lua_Client::IsFeigned)
		.def("AutoSplitEnabled", (bool(Lua_Client::*)(void))&Lua_Client::AutoSplitEnabled)
		.def("SetHorseId", (void(Lua_Client::*)(int))&Lua_Client::SetHorseId)
		.def("GetHorseId", (int(Lua_Client::*)(void))&Lua_Client::GetHorseId)
		.def("NukeItem", (void(Lua_Client::*)(uint32))&Lua_Client::NukeItem)
		.def("NukeItem", (void(Lua_Client::*)(uint32,int))&Lua_Client::NukeItem)
		.def("SetMaterial", (void(Lua_Client::*)(int,uint32))&Lua_Client::SetMaterial)
		.def("GetItemIDAt", (int(Lua_Client::*)(int))&Lua_Client::GetItemIDAt)
		.def("FindOnCursor", (bool(Lua_Client::*)(uint32))&Lua_Client::FindOnCursor)
		.def("DeleteItemInInventory", (void(Lua_Client::*)(int,int))&Lua_Client::DeleteItemInInventory)
		.def("DeleteItemInInventory", (void(Lua_Client::*)(int,int,bool))&Lua_Client::DeleteItemInInventory)
		.def("SummonItem", (void(Lua_Client::*)(uint32))&Lua_Client::SummonItem)
		.def("SummonItem", (void(Lua_Client::*)(uint32,int))&Lua_Client::SummonItem)
		.def("SummonItem", (void(Lua_Client::*)(uint32, int, int))&Lua_Client::SummonItem)
		.def("SummonItem", (void(Lua_Client::*)(uint32, int, int, bool))&Lua_Client::SummonItem)
		.def("SummonCursorItem", (void(Lua_Client::*)(uint32))& Lua_Client::SummonCursorItem)
		.def("SummonCursorItem", (void(Lua_Client::*)(uint32, int))& Lua_Client::SummonCursorItem)
		.def("DropItem", (void(Lua_Client::*)(int))&Lua_Client::DropItem)
		.def("BreakInvis", (void(Lua_Client::*)(void))&Lua_Client::BreakInvis)
		.def("LeaveGroup", (void(Lua_Client::*)(void))&Lua_Client::LeaveGroup)
		.def("IsGrouped", (bool(Lua_Client::*)(void))&Lua_Client::IsGrouped)
		.def("IsRaidGrouped", (bool(Lua_Client::*)(void))&Lua_Client::IsRaidGrouped)
		.def("Hungry", (bool(Lua_Client::*)(void))&Lua_Client::Hungry)
		.def("Thirsty", (bool(Lua_Client::*)(void))&Lua_Client::Thirsty)
		.def("GetInstrumentMod", (int(Lua_Client::*)(int))&Lua_Client::GetInstrumentMod)
		.def("DecreaseByID", (bool(Lua_Client::*)(uint32, int))&Lua_Client::DecreaseByID)
		.def("Escape", (void(Lua_Client::*)(void))&Lua_Client::Escape)
		.def("GoFish", (void(Lua_Client::*)(void))&Lua_Client::GoFish)
		.def("ForageItem", (void(Lua_Client::*)(void))&Lua_Client::ForageItem)
		.def("ForageItem", (void(Lua_Client::*)(bool))&Lua_Client::ForageItem)
		.def("CalcPriceMod", (float(Lua_Client::*)(Lua_Mob, bool))&Lua_Client::CalcPriceMod)
		.def("ResetTrade", (void(Lua_Client::*)(void))&Lua_Client::ResetTrade)
		.def("UseDiscipline", (bool(Lua_Client::*)(int))&Lua_Client::UseDiscipline)
		.def("GetCharacterFactionLevel", (int(Lua_Client::*)(int))&Lua_Client::GetCharacterFactionLevel)
		.def("SetZoneFlag", (void(Lua_Client::*)(int))&Lua_Client::SetZoneFlag)
		.def("ClearZoneFlag", (void(Lua_Client::*)(int))&Lua_Client::ClearZoneFlag)
		.def("HasZoneFlag", (bool(Lua_Client::*)(int))&Lua_Client::HasZoneFlag)
		.def("SendZoneFlagInfo", (void(Lua_Client::*)(Lua_Client))&Lua_Client::SendZoneFlagInfo)
		.def("SetAATitle", (void(Lua_Client::*)(const char *))&Lua_Client::SetAATitle)
		.def("GetClientVersion", (int(Lua_Client::*)(void))&Lua_Client::GetClientVersion)
		.def("GetClientVersionBit", (uint32(Lua_Client::*)(void))&Lua_Client::GetClientVersionBit)
		.def("SetTitleSuffix", (void(Lua_Client::*)(const char *))&Lua_Client::SetTitleSuffix)
		.def("SetAAPoints", (void(Lua_Client::*)(int))&Lua_Client::SetAAPoints)
		.def("GetAAPoints", (int(Lua_Client::*)(void))&Lua_Client::GetAAPoints)
		.def("GetSpentAA", (int(Lua_Client::*)(void))&Lua_Client::GetSpentAA)
		.def("AddAAPoints", (void(Lua_Client::*)(int))&Lua_Client::AddAAPoints)
		.def("RefundAA", (void(Lua_Client::*)(void))&Lua_Client::RefundAA)
		.def("GetModCharacterFactionLevel", (int(Lua_Client::*)(int))&Lua_Client::GetModCharacterFactionLevel)
		.def("GetStartZone", (int(Lua_Client::*)(void))&Lua_Client::GetStartZone)
		.def("KeyRingAdd", (void(Lua_Client::*)(uint32))&Lua_Client::KeyRingAdd)
		.def("KeyRingCheck", (bool(Lua_Client::*)(uint32))&Lua_Client::KeyRingCheck)
		.def("QuestReadBook", (void(Lua_Client::*)(const char *, int))&Lua_Client::QuestReadBook)
		.def("SendOPTranslocateConfirm", (void(Lua_Client::*)(Lua_Mob, int))&Lua_Client::SendOPTranslocateConfirm)
		.def("GetIP", (uint32(Lua_Client::*)(void))&Lua_Client::GetIP)
		.def("AddLevelBasedExp", (void(Lua_Client::*)(int))&Lua_Client::AddLevelBasedExp)
		.def("AddLevelBasedExp", (void(Lua_Client::*)(int, int))&Lua_Client::AddLevelBasedExp)
		.def("IncrementAA", (void(Lua_Client::*)(int))&Lua_Client::IncrementAA)
		.def("GetNextAvailableSpellBookSlot", (int(Lua_Client::*)(void))&Lua_Client::GetNextAvailableSpellBookSlot)
		.def("GetNextAvailableSpellBookSlot", (int(Lua_Client::*)(int))&Lua_Client::GetNextAvailableSpellBookSlot)
		.def("FindSpellBookSlotBySpellID", (int(Lua_Client::*)(int))&Lua_Client::FindSpellBookSlotBySpellID)
		.def("GetCorpseCount", (int(Lua_Client::*)(void))&Lua_Client::GetCorpseCount)
		.def("GetCorpseID", (int(Lua_Client::*)(int))&Lua_Client::GetCorpseID)
		.def("GetCorpseItemAt", (int(Lua_Client::*)(int, int))&Lua_Client::GetCorpseItemAt)
		.def("Freeze", (void(Lua_Client::*)(void))&Lua_Client::Freeze)
		.def("UnFreeze", (void(Lua_Client::*)(void))&Lua_Client::UnFreeze)
		.def("GetAggroCount", (int(Lua_Client::*)(void))&Lua_Client::GetAggroCount)
		.def("GetCarriedMoney", (uint64(Lua_Client::*)(void))&Lua_Client::GetCarriedMoney)
		.def("GetAllMoney", (uint64(Lua_Client::*)(void))&Lua_Client::GetAllMoney)
		.def("Signal", (void(Lua_Client::*)(uint32))&Lua_Client::Signal)
		.def("HasSpellScribed", (bool(Lua_Client::*)(int))&Lua_Client::HasSpellScribed)
		.def("SetAccountFlag", (void(Lua_Client::*)(std::string, std::string))&Lua_Client::SetAccountFlag)
		.def("GetAccountFlag", (std::string(Lua_Client::*)(std::string))&Lua_Client::GetAccountFlag)
		.def("GetGroup", (Lua_Group(Lua_Client::*)(void))&Lua_Client::GetGroup)
		.def("GetRaid", (Lua_Raid(Lua_Client::*)(void))&Lua_Client::GetRaid)
		.def("GetRaidOrGroupOrSelf", (luabind::object(Lua_Client:: *)(lua_State *)) &Lua_Client::GetRaidOrGroupOrSelf)
		.def("GetRaidOrGroupOrSelf", (luabind::object(Lua_Client:: *)(lua_State *, bool)) &Lua_Client::GetRaidOrGroupOrSelf)
		.def("PutItemInInventory", (bool(Lua_Client::*)(int,Lua_ItemInst))&Lua_Client::PutItemInInventory)
		.def("PushItemOnCursor", (bool(Lua_Client::*)(Lua_ItemInst))&Lua_Client::PushItemOnCursor)
		.def("PushItemOnCursorWithoutQueue", (bool(Lua_Client::*)(Lua_ItemInst))&Lua_Client::PushItemOnCursorWithoutQueue)
		.def("GetInventory", (Lua_Inventory(Lua_Client::*)(void))&Lua_Client::GetInventory)
		.def("QueuePacket", (void(Lua_Client::*)(Lua_Packet))&Lua_Client::QueuePacket)
		.def("QueuePacket", (void(Lua_Client::*)(Lua_Packet, bool))&Lua_Client::QueuePacket)
		.def("QueuePacket", (void(Lua_Client::*)(Lua_Packet, bool, int))&Lua_Client::QueuePacket)
		.def("QueuePacket", (void(Lua_Client::*)(Lua_Packet, bool, int, int))&Lua_Client::QueuePacket)
		.def("GetHunger", (int(Lua_Client::*)(void))&Lua_Client::GetHunger)
		.def("GetThirst", (int(Lua_Client::*)(void))&Lua_Client::GetThirst)
		.def("SetHunger", (void(Lua_Client::*)(int))&Lua_Client::SetHunger)
		.def("SetThirst", (void(Lua_Client::*)(int))&Lua_Client::SetThirst)
		.def("SetConsumption", (void(Lua_Client::*)(int, int))&Lua_Client::SetConsumption)
		.def("GetBoatID", (int(Lua_Client::*)(void))&Lua_Client::GetBoatID)
		.def("SetBoatID", (void(Lua_Client::*)(uint32))&Lua_Client::SetBoatID)
		.def("GetBoatName", (char *(Lua_Client::*)(void))&Lua_Client::GetBoatName)
		.def("SetBoatName", (void(Lua_Client::*)(const char *))&Lua_Client::SetBoatName)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32, int32))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32, int32, int32))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32, int32, int32, int32))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32, int32, int32, int32, int16))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32, int32, int32, int32, int16, int32))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, int32, int32, int32, int32, int16, int32, bool))&Lua_Client::QuestReward)
		.def("QuestReward", (void(Lua_Client::*)(Lua_Mob, luabind::adl::object))& Lua_Client::QuestReward)
		.def("GetMonkHandToHandDamage", (int(Lua_Client::*)(void))&Lua_Client::GetHandToHandDamage)
		.def("GetMonkHandToHandDelay", (int(Lua_Client::*)(void))&Lua_Client::GetHandToHandDelay)
		.def("SendSound", (void(Lua_Client::*)(uint16))&Lua_Client::SendSound)
		.def("ExpendAATimer", (void(Lua_Client::*)(int))& Lua_Client::ExpendAATimer)
		.def("SetHardcore", (void(Lua_Client::*)(int))&Lua_Client::SetHardcore)
		.def("IsHardcore", (int(Lua_Client::*)(void))&Lua_Client::IsHardcore)
		.def("SetSelfFound", (void(Lua_Client::*)(int))&Lua_Client::SetSelfFound)
		.def("IsSelfFound", (int(Lua_Client::*)(void))&Lua_Client::IsSelfFound)
		.def("SetSoloOnly", (void(Lua_Client::*)(int))&Lua_Client::SetSoloOnly)
		.def("IsSoloOnly", (int(Lua_Client::*)(void))&Lua_Client::IsSoloOnly)
		.def("ClearPlayerInfoAndGrantStartingItems", (void(Lua_Client::*)(void))&Lua_Client::ClearPlayerInfoAndGrantStartingItems)
		.def("ResetPlayerForNewGamePlus", (void(Lua_Client::*)(uint8,uint8,bool))&Lua_Client::ResetPlayerForNewGamePlus)
		.def("ScribeSpells", (uint16(Lua_Client::*)(uint8, uint8))& Lua_Client::ScribeSpells)
		.def("IsMarried", (bool(Lua_Client::*)())&Lua_Client::IsMarried)
		.def("SetMarried", (void(Lua_Client::*)(const char*))&Lua_Client::SetMarried)
		.def("SetTemporaryLastName", (void(Lua_Client::*)(const char*))&Lua_Client::SetTemporaryLastName)
		.def("SetTemporaryCustomizedLastName", (void(Lua_Client::*)(const char*))&Lua_Client::SetTemporaryCustomizedLastName)
		.def("HasTemporaryLastName", (bool(Lua_Client::*)(void))&Lua_Client::HasTemporaryLastName)
		.def("PermaGender", (void(Lua_Client::*)(uint32))&Lua_Client::PermaGender)
		.def("ScribeSpells", (uint16(Lua_Client::*)(uint8, uint8))& Lua_Client::ScribeSpells)
		.def("SendGMCommand", (bool(Lua_Client:: *)(std::string)) &Lua_Client::SendGMCommand)
		.def("SendGMCommand", (bool(Lua_Client:: *)(std::string, bool)) &Lua_Client::SendGMCommand)
		.def("SetClientMaxLevel", (void(Lua_Client::*)(int))& Lua_Client::SetClientMaxLevel)
		.def("GetClientMaxLevel", (int(Lua_Client::*)(void))& Lua_Client::GetClientMaxLevel)
		.def("GetSkillTrainLevel", (uint8(Lua_Client::*)(int))& Lua_Client::GetSkillTrainLevel)
		.def("MaxSkills", (void(Lua_Client::*)(void))& Lua_Client::MaxSkills);

}

luabind::scope lua_register_inventory_where() {
	return luabind::class_<InventoryWhere>("InventoryWhere")
		.enum_("constants")
		[
			luabind::value("Personal", static_cast<int>(invWherePersonal)),
			luabind::value("Bank", static_cast<int>(invWhereBank)),
			luabind::value("Trading", static_cast<int>(invWhereTrading)),
			luabind::value("Cursor", static_cast<int>(invWhereCursor))
		];
}



#endif
