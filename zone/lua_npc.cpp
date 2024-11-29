#ifdef LUA_EQEMU

#include "lua.hpp"
#include <luabind/luabind.hpp>

#include "npc.h"
#include "lua_npc.h"
#include "lua_client.h"

void Lua_NPC::Signal(int id) {
	Lua_Safe_Call_Void();
	self->SignalNPC(id);
}

int Lua_NPC::CheckNPCFactionAlly(int faction) {
	Lua_Safe_Call_Int();
	return static_cast<int>(self->CheckNPCFactionAlly(faction));
}

void Lua_NPC::AddItem(int item_id, int charges) {
	Lua_Safe_Call_Void();
	self->AddItem(item_id, charges);
}

void Lua_NPC::AddItem(int item_id, int charges, bool equip) {
	Lua_Safe_Call_Void();
	self->AddItem(item_id, charges, equip);
}

void Lua_NPC::AddLootTable() {
	Lua_Safe_Call_Void();
	self->AddLootTable();
}

void Lua_NPC::AddLootTable(int id) {
	Lua_Safe_Call_Void();
	self->AddLootTable(id);
}

void Lua_NPC::RemoveItem(int item_id) {
	Lua_Safe_Call_Void();
	LootItem* sitem = self->GetItemByID(item_id);
	self->RemoveItem(sitem);
}

void Lua_NPC::RemoveItem(int item_id, int slot) {
	Lua_Safe_Call_Void();
	LootItem* sitem = self->GetItem(slot, item_id);
	self->RemoveItem(sitem);
}

void Lua_NPC::ClearLootItems() {
	Lua_Safe_Call_Void();
	self->ClearLootItems();
}

void Lua_NPC::AddLootCash(int copper, int silver, int gold, int platinum) {
	Lua_Safe_Call_Void();
	self->AddLootCash(copper, silver, gold, platinum);
}

void Lua_NPC::RemoveLootCash() {
	Lua_Safe_Call_Void();
	self->RemoveLootCash();
}

int Lua_NPC::CountLoot() {
	Lua_Safe_Call_Int();
	return self->CountLoot();
}

int Lua_NPC::GetLoottableID() {
	Lua_Safe_Call_Int();
	return self->GetLoottableID();
}

uint32 Lua_NPC::GetCopper() {
	Lua_Safe_Call_Int();
	return self->GetCopper();
}

uint32 Lua_NPC::GetSilver() {
	Lua_Safe_Call_Int();
	return self->GetSilver();
}

uint32 Lua_NPC::GetGold() {
	Lua_Safe_Call_Int();
	return self->GetGold();
}

uint32 Lua_NPC::GetPlatinum() {
	Lua_Safe_Call_Int();
	return self->GetPlatinum();
}

void Lua_NPC::SetCopper(uint32 amt) {
	Lua_Safe_Call_Void();
	self->SetCopper(amt);
}

void Lua_NPC::SetSilver(uint32 amt) {
	Lua_Safe_Call_Void();
	self->SetSilver(amt);
}

void Lua_NPC::SetGold(uint32 amt) {
	Lua_Safe_Call_Void();
	self->SetGold(amt);
}

void Lua_NPC::SetPlatinum(uint32 amt) {
	Lua_Safe_Call_Void();
	self->SetPlatinum(amt);
}

void Lua_NPC::SetGrid(int grid) {
	Lua_Safe_Call_Void();
	self->SetGrid(grid);
}

void Lua_NPC::SetSaveWaypoint(int wp) {
	Lua_Safe_Call_Void();
	self->SetSaveWaypoint(wp);
}

void Lua_NPC::SetSp2(int sg2) {
	Lua_Safe_Call_Void();
	self->SetSp2(sg2);
}

int Lua_NPC::GetWaypointMax() {
	Lua_Safe_Call_Int();
	return self->GetMaxWp();
}

int Lua_NPC::GetGrid() {
	Lua_Safe_Call_Int();
	return self->GetGrid();
}

uint32 Lua_NPC::GetSp2() {
	Lua_Safe_Call_Int();
	return self->GetSp2();
}

int Lua_NPC::GetNPCFactionID() {
	Lua_Safe_Call_Int();
	return self->GetNPCFactionID();
}

int Lua_NPC::GetPrimaryFaction() {
	Lua_Safe_Call_Int();
	return self->GetPrimaryFaction();
}

int Lua_NPC::GetNPCHate(Lua_Mob ent) {
	Lua_Safe_Call_Int();
	return self->GetNPCHate(ent);
}

bool Lua_NPC::IsOnHatelist(Lua_Mob ent) {
	Lua_Safe_Call_Bool();
	return self->IsOnHatelist(ent);
}

void Lua_NPC::SetNPCFactionID(int id) {
	Lua_Safe_Call_Void();
	self->SetNPCFactionID(id);
}

uint32 Lua_NPC::GetMaxDMG() {
	Lua_Safe_Call_Int();
	return self->GetMaxDMG();
}

uint32 Lua_NPC::GetMinDMG() {
	Lua_Safe_Call_Int();
	return self->GetMinDMG();
}

bool Lua_NPC::IsAnimal() {
	Lua_Safe_Call_Bool();
	return self->IsAnimal();
}

int Lua_NPC::GetPetSpellID() {
	Lua_Safe_Call_Int();
	return self->GetPetSpellID();
}

void Lua_NPC::SetPetSpellID(int id) {
	Lua_Safe_Call_Void();
	self->SetPetSpellID(id);
}

uint32 Lua_NPC::GetMaxDamage(int level) {
	Lua_Safe_Call_Int();
	return self->GetMaxDamage(level);
}

void Lua_NPC::SetTaunting(bool t) {
	Lua_Safe_Call_Void();
	self->SetTaunting(t);
}

void Lua_NPC::PickPocket(Lua_Client thief) {
	Lua_Safe_Call_Void();
	self->PickPocket(thief);
}

void Lua_NPC::StartSwarmTimer(uint32 duration) {
	Lua_Safe_Call_Void();
	self->StartSwarmTimer(duration);
}

void Lua_NPC::DoClassAttacks(Lua_Mob target) {
	Lua_Safe_Call_Void();
	self->DoClassAttacks(target);
}

int Lua_NPC::GetMaxWp() {
	Lua_Safe_Call_Int();
	return self->GetMaxWp();
}

void Lua_NPC::DisplayWaypointInfo(Lua_Client to) {
	Lua_Safe_Call_Void();
	self->DisplayWaypointInfo(to);
}

void Lua_NPC::CalculateNewWaypoint() {
	Lua_Safe_Call_Void();
	self->CalculateNewWaypoint();
}

void Lua_NPC::AssignWaypoints(int grid) {
	Lua_Safe_Call_Void();
	self->AssignWaypoints(grid);
}

void Lua_NPC::RemoveWaypoints() {
	Lua_Safe_Call_Void();
	self->RemoveWaypoints();
}

void Lua_NPC::SetWaypointPause() {
	Lua_Safe_Call_Void();
	self->SetWaypointPause();
}

void Lua_NPC::UpdateWaypoint(int wp) {
	Lua_Safe_Call_Void();
	self->UpdateWaypoint(wp);
}

void Lua_NPC::EditWaypoint(int wp, float x, float y, float z, float h, int pause, bool centerpoint) {
	Lua_Safe_Call_Void();
	self->EditWaypoint(wp, x, y, z, h, pause, centerpoint);
}

void Lua_NPC::AddWaypoint(float x, float y, float z, float h, int pause, bool centerpoint) {
	Lua_Safe_Call_Void();
	self->AddWaypoint(x, y, z, h, pause, centerpoint);
}

void Lua_NPC::SetWanderType(int wt) {
	Lua_Safe_Call_Void();
	self->SetWanderType(wt);
}

void Lua_NPC::SetPauseType(int pt) {
	Lua_Safe_Call_Void();
	self->SetPauseType(pt);
}

void Lua_NPC::StopWandering() {
	Lua_Safe_Call_Void();
	self->StopWandering();
}

void Lua_NPC::ResumeWandering() {
	Lua_Safe_Call_Void();
	self->ResumeWandering();
}

void Lua_NPC::PauseWandering(int pause_time) {
	Lua_Safe_Call_Void();
	self->PauseWandering(pause_time);
}

void Lua_NPC::SetNoQuestPause(bool state) {
	Lua_Safe_Call_Void();
	self->SetNoQuestPause(state);
}

void Lua_NPC::MoveTo(float x, float y, float z, float h, bool save) {
	Lua_Safe_Call_Void();
	auto position = glm::vec4(x, y, z, h);
	self->MoveTo(position, save);
}

void Lua_NPC::MoveTo(float x, float y, float z, float h, bool save, uint32 delay) {
	Lua_Safe_Call_Void();
	auto position = glm::vec4(x, y, z, h);
	self->MoveTo(position, save, delay);
}

void Lua_NPC::StopQuestMove() {
	Lua_Safe_Call_Void();
	self->StopQuestMove();
}

void Lua_NPC::StopQuestMove(bool save_guard_spot) {
	Lua_Safe_Call_Void();
	self->StopQuestMove(save_guard_spot);
}

void Lua_NPC::NextGuardPosition() {
	Lua_Safe_Call_Void();
	self->NextGuardPosition();
}

void Lua_NPC::SaveGuardSpot() {
	Lua_Safe_Call_Void();
	self->SaveGuardSpot();
}

void Lua_NPC::SaveGuardSpot(bool clear) {
	Lua_Safe_Call_Void();
	self->SaveGuardSpot(clear);
}

void Lua_NPC::SetGuardSpot(float x, float y, float z, float h) {
	Lua_Safe_Call_Void();
	self->SetGuardSpot(x, y, z, h);
}

bool Lua_NPC::IsGuarding() {
	Lua_Safe_Call_Bool();
	return self->IsGuarding();
}

void Lua_NPC::AI_SetRoambox(float max_x, float min_x, float max_y, float min_y) {
	Lua_Safe_Call_Void();
	self->AI_SetRoambox(max_x, min_x, max_y, min_y);
}

void Lua_NPC::AI_SetRoambox(float max_x, float min_x, float max_y, float min_y, uint32 delay, uint32 mindelay) {
	Lua_Safe_Call_Void();
	self->AI_SetRoambox(max_x, min_x, max_y, min_y, delay, mindelay);
}

int Lua_NPC::GetNPCSpellsID() {
	Lua_Safe_Call_Int();
	return self->GetNPCSpellsID();
}

int Lua_NPC::GetSpawnPointID() {
	Lua_Safe_Call_Int();
	return self->GetSpawnPointID();
}

float Lua_NPC::GetSpawnPointX() {
	Lua_Safe_Call_Real();
	return self->GetSpawnPoint().x;
}

float Lua_NPC::GetSpawnPointY() {
	Lua_Safe_Call_Real();
	return self->GetSpawnPoint().y;
}

float Lua_NPC::GetSpawnPointZ() {
	Lua_Safe_Call_Real();
	return self->GetSpawnPoint().z;
}

float Lua_NPC::GetSpawnPointH() {
	Lua_Safe_Call_Real();
	return self->GetSpawnPoint().w;
}

float Lua_NPC::GetGuardPointX() {
	Lua_Safe_Call_Real();
	return self->GetGuardPoint().x;
}

float Lua_NPC::GetGuardPointY() {
	Lua_Safe_Call_Real();
	return self->GetGuardPoint().y;
}

float Lua_NPC::GetGuardPointZ() {
	Lua_Safe_Call_Real();
	return self->GetGuardPoint().z;
}

void Lua_NPC::SetPrimSkill(int skill_id) {
	Lua_Safe_Call_Void();
	self->SetPrimSkill(skill_id);
}

void Lua_NPC::SetSecSkill(int skill_id) {
	Lua_Safe_Call_Void();
	self->SetSecSkill(skill_id);
}

int Lua_NPC::GetPrimSkill() {
	Lua_Safe_Call_Int();
	return self->GetPrimSkill();
}

int Lua_NPC::GetSecSkill() {
	Lua_Safe_Call_Int();
	return self->GetSecSkill();
}

int Lua_NPC::GetSwarmOwner() {
	Lua_Safe_Call_Int();
	return self->GetSwarmOwner();
}

int Lua_NPC::GetSwarmTarget() {
	Lua_Safe_Call_Int();
	return self->GetSwarmTarget();
}

void Lua_NPC::SetSwarmTarget(int target) {
	Lua_Safe_Call_Void();
	self->SetSwarmTarget(target);
}

void Lua_NPC::ModifyNPCStat(const char *stat, const char *value) {
	Lua_Safe_Call_Void();
	self->ModifyNPCStat(stat, value);
}

void Lua_NPC::AddAISpell(int priority, int spell_id, int type, int mana_cost, int recast_delay, int resist_adjust) {
	Lua_Safe_Call_Void();
	self->AddSpellToNPCList(priority, spell_id, type, mana_cost, recast_delay, resist_adjust);
}

void Lua_NPC::RemoveAISpell(int spell_id) {
	Lua_Safe_Call_Void();
	self->RemoveSpellFromNPCList(spell_id);
}

void Lua_NPC::SetCastRateDetrimental(int rate) {
	Lua_Safe_Call_Void();
	if (rate > 100)
		rate = 100;
	if (rate < 0)
		rate = 0;
	self->SetCastRateDetrimental(static_cast<uint8>(rate));
}

void Lua_NPC::SetCastRateBeneficial(int rate) {
	Lua_Safe_Call_Void();
	if (rate > 100)
		rate = 100;
	if (rate < 0)
		rate = 0;
	self->SetCastRateBeneficial(static_cast<uint8>(rate));
}

void Lua_NPC::SetSpellFocusDMG(int focus) {
	Lua_Safe_Call_Void();
	self->SetSpellFocusDMG(focus);
}

void Lua_NPC::SetSpellFocusHeal(int focus) {
	Lua_Safe_Call_Void();
	self->SetSpellFocusHeal(focus);
}

int Lua_NPC::GetSpellFocusDMG() {
	Lua_Safe_Call_Int();
	return self->GetSpellFocusDMG();
}

int Lua_NPC::GetSpellFocusHeal() {
	Lua_Safe_Call_Int();
	return self->GetSpellFocusHeal();
}

float Lua_NPC::GetSlowMitigation() {
	Lua_Safe_Call_Real();
	return self->GetSlowMitigation();
}

int Lua_NPC::GetAccuracyRating() {
	Lua_Safe_Call_Int();
	return self->GetAccuracyRating();
}

int Lua_NPC::GetSpawnKillCount() {
	Lua_Safe_Call_Int();
	return self->GetSpawnKillCount();
}

void Lua_NPC::MerchantOpenShop() {
	Lua_Safe_Call_Void();
	self->MerchantOpenShop();
}

void Lua_NPC::MerchantCloseShop() {
	Lua_Safe_Call_Void();
	self->MerchantCloseShop();
}

void Lua_NPC::AddQuestLoot(int itemid)
{
	Lua_Safe_Call_Void();
	self->AddQuestLoot(itemid);
}

void Lua_NPC::AddQuestLoot(int itemid, int charges)
{
	Lua_Safe_Call_Void();
	self->AddQuestLoot(itemid, charges);
}

void Lua_NPC::AddQuestLoot(int itemid, int charges, std::string item_custom_data_string)
{
	Lua_Safe_Call_Void();
	EQ::ItemCustomData item_custom_data;
	SharedDatabase::ParseCustomDataFromString(item_custom_data, item_custom_data_string.c_str());
	self->AddQuestLoot(itemid, charges, item_custom_data);
}

void Lua_NPC::AddPetLoot(int itemid)
{
	Lua_Safe_Call_Void();
	self->AddPetLoot(itemid, 1, true);
}

void Lua_NPC::AddPetLoot(int itemid, int charges)
{
	Lua_Safe_Call_Void();
	self->AddPetLoot(itemid, charges, true);
}

void Lua_NPC::AddPetLoot(int itemid, int charges, std::string item_custom_data_string)
{
	Lua_Safe_Call_Void();
	EQ::ItemCustomData item_custom_data;
	SharedDatabase::ParseCustomDataFromString(item_custom_data, item_custom_data_string.c_str());
	self->AddPetLoot(itemid, charges, true, item_custom_data);
}

bool Lua_NPC::GetQuestLoot(int itemid)
{
	Lua_Safe_Call_Bool();
	return self->HasQuestLootItem(itemid);
}

bool Lua_NPC::GetPetLoot(int itemid)
{
	Lua_Safe_Call_Bool();
	return self->HasPetLootItem(itemid);
}

bool Lua_NPC::HasQuestLoot()
{
	Lua_Safe_Call_Bool();
	return self->HasQuestLoot();
}

void Lua_NPC::DeleteQuestLoot()
{
	Lua_Safe_Call_Void();
	self->DeleteQuestLoot(0);
}

void Lua_NPC::DeleteQuestLoot(int itemid1)
{
	Lua_Safe_Call_Void();
	self->DeleteQuestLoot(itemid1);
}

void Lua_NPC::DeleteQuestLoot(int itemid1, int itemid2)
{
	Lua_Safe_Call_Void();
	self->DeleteQuestLoot(itemid1, itemid2);
}

void Lua_NPC::DeleteQuestLoot(int itemid1, int itemid2, int itemid3)
{
	Lua_Safe_Call_Void();
	self->DeleteQuestLoot(itemid1, itemid2, itemid3);
}

void Lua_NPC::DeleteQuestLoot(int itemid1, int itemid2, int itemid3, int itemid4)
{
	Lua_Safe_Call_Void();
	self->DeleteQuestLoot(itemid1, itemid2, itemid3, itemid4);
}

bool Lua_NPC::HasRequiredQuestLoot(int itemid1, int itemid2, int itemid3, int itemid4)
{
	Lua_Safe_Call_Bool();
	return self->HasRequiredQuestLoot(itemid1, itemid2, itemid3, itemid4);
}

int Lua_NPC::QuestLootCount(int itemid)
{
	Lua_Safe_Call_Int();
	return self->CountQuestItem(itemid);
}

bool Lua_NPC::CanTalk()
{
	Lua_Safe_Call_Bool();
	return self->CanTalk();
}

void Lua_NPC::ForceRepop()
{
	Lua_Safe_Call_Void();
	self->ForceRepop();
}

void Lua_NPC::SetNPCAggro(bool state)
{
	Lua_Safe_Call_Void();
	self->SetNPCAggro(state);
}

void Lua_NPC::SetBaseHP(uint32 new_hp) {
	Lua_Safe_Call_Void();
	self->SetBaseHP(new_hp);
}

void Lua_NPC::SetMaxDamage(uint32 new_max_damage) {
	Lua_Safe_Call_Void();
	self->SetMaxDamage(new_max_damage);
}

void Lua_NPC::SetMinDamage(uint32 new_min_damage) {
	Lua_Safe_Call_Void();
	self->SetMinDamage(new_min_damage);
}

void Lua_NPC::SetSpawnPoint(float x, float y, float z, float h) {
	Lua_Safe_Call_Void();
	self->SetSpawnPoint(x, y, z, h);
}

void Lua_NPC::SetClass(int classNum) {
	Lua_Safe_Call_Void();
	self->SetClass(classNum);
}


luabind::scope lua_register_npc() {
	return luabind::class_<Lua_NPC, Lua_Mob>("NPC")
		.def(luabind::constructor<>())
		.def("Signal", (void(Lua_NPC::*)(int))&Lua_NPC::Signal)
		.def("CheckNPCFactionAlly", (int(Lua_NPC::*)(int))&Lua_NPC::CheckNPCFactionAlly)
		.def("AddItem", (void(Lua_NPC::*)(int, int))&Lua_NPC::AddItem)
		.def("AddItem", (void(Lua_NPC::*)(int, int, bool))&Lua_NPC::AddItem)
		.def("AddLootTable", (void(Lua_NPC::*)(void))&Lua_NPC::AddLootTable)
		.def("AddLootTable", (void(Lua_NPC::*)(int))&Lua_NPC::AddLootTable)
		.def("RemoveItem", (void(Lua_NPC::*)(int))&Lua_NPC::RemoveItem)
		.def("RemoveItem", (void(Lua_NPC::*)(int, int))&Lua_NPC::RemoveItem)
		.def("ClearItemList", (void(Lua_NPC::*)(void))&Lua_NPC::ClearLootItems)
		.def("AddCash", (void(Lua_NPC::*)(int, int, int, int))&Lua_NPC::AddLootCash)
		.def("RemoveCash", (void(Lua_NPC::*)(void))&Lua_NPC::RemoveLootCash)
		.def("CountLoot", (int(Lua_NPC::*)(void))&Lua_NPC::CountLoot)
		.def("GetLoottableID", (int(Lua_NPC::*)(void))&Lua_NPC::GetLoottableID)
		.def("GetCopper", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetCopper)
		.def("GetSilver", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetSilver)
		.def("GetGold", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetGold)
		.def("GetPlatinum", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetPlatinum)
		.def("SetCopper", (void(Lua_NPC::*)(uint32))&Lua_NPC::SetCopper)
		.def("SetSilver", (void(Lua_NPC::*)(uint32))&Lua_NPC::SetSilver)
		.def("SetGold", (void(Lua_NPC::*)(uint32))&Lua_NPC::SetGold)
		.def("SetPlatinum", (void(Lua_NPC::*)(uint32))&Lua_NPC::SetPlatinum)
		.def("SetGrid", (void(Lua_NPC::*)(int))&Lua_NPC::SetGrid)
		.def("SetSaveWaypoint", (void(Lua_NPC::*)(int))&Lua_NPC::SetSaveWaypoint)
		.def("SetSp2", (void(Lua_NPC::*)(int))&Lua_NPC::SetSp2)
		.def("GetWaypointMax", (int(Lua_NPC::*)(void))&Lua_NPC::GetWaypointMax)
		.def("GetGrid", (int(Lua_NPC::*)(void))&Lua_NPC::GetGrid)
		.def("GetSp2", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetSp2)
		.def("GetNPCFactionID", (int(Lua_NPC::*)(void))&Lua_NPC::GetNPCFactionID)
		.def("GetPrimaryFaction", (int(Lua_NPC::*)(void))&Lua_NPC::GetPrimaryFaction)
		.def("GetNPCHate", (int(Lua_NPC::*)(Lua_Mob))&Lua_NPC::GetNPCHate)
		.def("IsOnHatelist", (bool(Lua_NPC::*)(Lua_Mob))&Lua_NPC::IsOnHatelist)
		.def("SetNPCFactionID", (void(Lua_NPC::*)(int))&Lua_NPC::SetNPCFactionID)
		.def("GetMaxDMG", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetMaxDMG)
		.def("GetMinDMG", (uint32(Lua_NPC::*)(void))&Lua_NPC::GetMinDMG)
		.def("IsAnimal", (bool(Lua_NPC::*)(void))&Lua_NPC::IsAnimal)
		.def("GetPetSpellID", (int(Lua_NPC::*)(void))&Lua_NPC::GetPetSpellID)
		.def("SetPetSpellID", (void(Lua_NPC::*)(int))&Lua_NPC::SetPetSpellID)
		.def("GetMaxDamage", (uint32(Lua_NPC::*)(int))&Lua_NPC::GetMaxDamage)
		.def("SetTaunting", (void(Lua_NPC::*)(bool))&Lua_NPC::SetTaunting)
		.def("PickPocket", (void(Lua_NPC::*)(Lua_Client))&Lua_NPC::PickPocket)
		.def("StartSwarmTimer", (void(Lua_NPC::*)(uint32))&Lua_NPC::StartSwarmTimer)
		.def("DoClassAttacks", (void(Lua_NPC::*)(Lua_Mob))&Lua_NPC::DoClassAttacks)
		.def("GetMaxWp", (int(Lua_NPC::*)(void))&Lua_NPC::GetMaxWp)
		.def("DisplayWaypointInfo", (void(Lua_NPC::*)(Lua_Client))&Lua_NPC::DisplayWaypointInfo)
		.def("CalculateNewWaypoint", (void(Lua_NPC::*)(void))&Lua_NPC::CalculateNewWaypoint)
		.def("AssignWaypoints", (void(Lua_NPC::*)(int))&Lua_NPC::AssignWaypoints)
		.def("RemoveWaypoints", (void(Lua_NPC::*)(void))&Lua_NPC::RemoveWaypoints)
		.def("SetWaypointPause", (void(Lua_NPC::*)(void))&Lua_NPC::SetWaypointPause)
		.def("UpdateWaypoint", (void(Lua_NPC::*)(int))&Lua_NPC::UpdateWaypoint)
		.def("EditWaypoint", (void(Lua_NPC::*)(int, float, float, float, float, int, bool))&Lua_NPC::EditWaypoint)
		.def("AddWaypoint", (void(Lua_NPC::*)(float, float, float, float, int, bool))&Lua_NPC::AddWaypoint)
		.def("SetWanderType", (void(Lua_NPC::*)(int))&Lua_NPC::SetWanderType)
		.def("SetPauseType", (void(Lua_NPC::*)(int))&Lua_NPC::SetPauseType)
		.def("StopWandering", (void(Lua_NPC::*)(void))&Lua_NPC::StopWandering)
		.def("ResumeWandering", (void(Lua_NPC::*)(void))&Lua_NPC::ResumeWandering)
		.def("PauseWandering", (void(Lua_NPC::*)(int))&Lua_NPC::PauseWandering)
		.def("SetNoQuestPause", (void(Lua_NPC::*)(bool))&Lua_NPC::SetNoQuestPause)
		.def("MoveTo", (void(Lua_NPC::*)(float, float, float, float, bool))&Lua_NPC::MoveTo)
		.def("MoveTo", (void(Lua_NPC::*)(float, float, float, float, bool, uint32))&Lua_NPC::MoveTo)
		.def("NextGuardPosition", (void(Lua_NPC::*)(void))&Lua_NPC::NextGuardPosition)
		.def("SaveGuardSpot", (void(Lua_NPC::*)(void))&Lua_NPC::SaveGuardSpot)
		.def("SaveGuardSpot", (void(Lua_NPC::*)(bool)) & Lua_NPC::SaveGuardSpot)
		.def("SetGuardSpot", (void(Lua_NPC::*)(float, float, float, float)) & Lua_NPC::SetGuardSpot)
		.def("IsGuarding", (bool(Lua_NPC::*)(void))&Lua_NPC::IsGuarding)
		.def("AI_SetRoambox", (void(Lua_NPC::*)(float, float, float, float))&Lua_NPC::AI_SetRoambox)
		.def("AI_SetRoambox", (void(Lua_NPC::*)(float, float, float, float, uint32, uint32))&Lua_NPC::AI_SetRoambox)
		.def("GetNPCSpellsID", (int(Lua_NPC::*)(void))&Lua_NPC::GetNPCSpellsID)
		.def("GetSpawnPointID", (int(Lua_NPC::*)(void))&Lua_NPC::GetSpawnPointID)
		.def("GetSpawnPointX", (float(Lua_NPC::*)(void))&Lua_NPC::GetSpawnPointX)
		.def("GetSpawnPointY", (float(Lua_NPC::*)(void))&Lua_NPC::GetSpawnPointY)
		.def("GetSpawnPointZ", (float(Lua_NPC::*)(void))&Lua_NPC::GetSpawnPointZ)
		.def("GetSpawnPointH", (float(Lua_NPC::*)(void))&Lua_NPC::GetSpawnPointH)
		.def("GetGuardPointX", (float(Lua_NPC::*)(void))&Lua_NPC::GetGuardPointX)
		.def("GetGuardPointY", (float(Lua_NPC::*)(void))&Lua_NPC::GetGuardPointY)
		.def("GetGuardPointZ", (float(Lua_NPC::*)(void))&Lua_NPC::GetGuardPointZ)
		.def("SetPrimSkill", (void(Lua_NPC::*)(int))&Lua_NPC::SetPrimSkill)
		.def("SetSecSkill", (void(Lua_NPC::*)(int))&Lua_NPC::SetSecSkill)
		.def("GetPrimSkill", (int(Lua_NPC::*)(void))&Lua_NPC::GetPrimSkill)
		.def("GetSecSkill", (int(Lua_NPC::*)(void))&Lua_NPC::GetSecSkill)
		.def("GetSwarmOwner", (int(Lua_NPC::*)(void))&Lua_NPC::GetSwarmOwner)
		.def("GetSwarmTarget", (int(Lua_NPC::*)(void))&Lua_NPC::GetSwarmTarget)
		.def("SetSwarmTarget", (void(Lua_NPC::*)(int))&Lua_NPC::SetSwarmTarget)
		.def("ModifyNPCStat", (void(Lua_NPC::*)(const char*, const char*))&Lua_NPC::ModifyNPCStat)
		.def("AddAISpell", (void(Lua_NPC::*)(int, int, int, int, int, int))&Lua_NPC::AddAISpell)
		.def("RemoveAISpell", (void(Lua_NPC::*)(int))&Lua_NPC::RemoveAISpell)
		.def("SetCastRateDetrimental", (void(Lua_NPC::*)(int))&Lua_NPC::SetCastRateDetrimental)
		.def("SetCastRateBeneficial", (void(Lua_NPC::*)(int))&Lua_NPC::SetCastRateBeneficial)
		.def("SetSpellFocusDMG", (void(Lua_NPC::*)(int))&Lua_NPC::SetSpellFocusDMG)
		.def("SetSpellFocusHeal", (void(Lua_NPC::*)(int))&Lua_NPC::SetSpellFocusHeal)
		.def("GetSpellFocusDMG", (void(Lua_NPC::*)(int))&Lua_NPC::GetSpellFocusDMG)
		.def("GetSpellFocusHeal", (void(Lua_NPC::*)(int))&Lua_NPC::GetSpellFocusHeal)
		.def("GetSlowMitigation", (int(Lua_NPC::*)(void))&Lua_NPC::GetSlowMitigation)
		.def("GetAccuracyRating", (int(Lua_NPC::*)(void))&Lua_NPC::GetAccuracyRating)
		.def("GetSpawnKillCount", (int(Lua_NPC::*)(void))&Lua_NPC::GetSpawnKillCount)
		.def("MerchantOpenShop", (void(Lua_NPC::*)(void))&Lua_NPC::MerchantOpenShop)
		.def("MerchantCloseShop", (void(Lua_NPC::*)(void))&Lua_NPC::MerchantCloseShop)
		.def("AddQuestLoot", (void(Lua_NPC::*)(int))&Lua_NPC::AddQuestLoot)
		.def("AddQuestLoot", (void(Lua_NPC::*)(int,int))&Lua_NPC::AddQuestLoot)
		.def("AddQuestLoot", (void(Lua_NPC::*)(int,int,std::string))&Lua_NPC::AddQuestLoot)
		.def("AddPetLoot", (void(Lua_NPC::*)(int))&Lua_NPC::AddPetLoot)
		.def("AddPetLoot", (void(Lua_NPC::*)(int,int))&Lua_NPC::AddPetLoot)
		.def("AddPetLoot", (void(Lua_NPC::*)(int,int,std::string))&Lua_NPC::AddPetLoot)
		.def("GetQuestLoot", (bool(Lua_NPC::*)(int))&Lua_NPC::GetQuestLoot)
		.def("GetPetLoot", (bool(Lua_NPC::*)(int))&Lua_NPC::GetPetLoot)
		.def("HasQuestLoot", (bool(Lua_NPC::*)(void))&Lua_NPC::HasQuestLoot)
		.def("DeleteQuestLoot", (void(Lua_NPC::*)(void))&Lua_NPC::DeleteQuestLoot)
		.def("DeleteQuestLoot", (void(Lua_NPC::*)(int))&Lua_NPC::DeleteQuestLoot)
		.def("DeleteQuestLoot", (void(Lua_NPC::*)(int, int))&Lua_NPC::DeleteQuestLoot)
		.def("DeleteQuestLoot", (void(Lua_NPC::*)(int, int, int))&Lua_NPC::DeleteQuestLoot)
		.def("DeleteQuestLoot", (void(Lua_NPC::*)(int, int, int, int))&Lua_NPC::DeleteQuestLoot)
		.def("HasRequiredQuestLoot", (bool(Lua_NPC::*)(int, int, int, int))&Lua_NPC::HasRequiredQuestLoot)
		.def("QuestLootCount", (int(Lua_NPC::*)(int))&Lua_NPC::QuestLootCount)
		.def("CanTalk", (bool(Lua_NPC::*)(void))&Lua_NPC::CanTalk)
		.def("ForceRepop", (void(Lua_NPC::*)(bool))&Lua_NPC::ForceRepop)
		.def("SetNPCAggro", (void(Lua_NPC::*)(bool))&Lua_NPC::SetNPCAggro)
		.def("SetBaseHP", (void(Lua_NPC::*)(uint32))&Lua_NPC::SetBaseHP)
		.def("SetSpawnPoint", (void(Lua_NPC::*)(float, float, float, float))& Lua_NPC::SetSpawnPoint)
		.def("StopQuestMove", (void(Lua_NPC::*)(void))& Lua_NPC::StopQuestMove)
		.def("StopQuestMove", (void(Lua_NPC::*)(bool))& Lua_NPC::StopQuestMove)
		.def("SetClass", (void(Lua_NPC::*)(int))& Lua_NPC::SetClass)
		.def("SetMaxDamage", (void(Lua_NPC:: *)(uint32)) &Lua_NPC::SetMaxDamage)
		.def("SetMinDamage", (void(Lua_NPC:: *)(uint32)) &Lua_NPC::SetMinDamage);
}

#endif
