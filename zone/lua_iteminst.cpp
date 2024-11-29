#ifdef LUA_EQEMU

#include "lua.hpp"
#include <luabind/luabind.hpp>
#include <luabind/object.hpp>

#include "masterentity.h"
#include "lua_iteminst.h"
#include "lua_item.h"

Lua_ItemInst::Lua_ItemInst(int item_id) {
	SetLuaPtrData(database.CreateItem(item_id));
	cloned_ = true;
}

Lua_ItemInst::Lua_ItemInst(int item_id, int charges) {
	SetLuaPtrData(database.CreateItem(item_id, charges));
	cloned_ = true;
}

Lua_ItemInst& Lua_ItemInst::operator=(const Lua_ItemInst& o) {
	if(o.cloned_) {
		cloned_ = true;
		d_ = new EQ::ItemInstance(*o.d_);
	} else {
		cloned_ = false;
		d_ = o.d_;
	}
	return *this;
}

Lua_ItemInst::Lua_ItemInst(const Lua_ItemInst& o) {
	if(o.cloned_) {
		cloned_ = true;
		d_ = new EQ::ItemInstance(*o.d_);
	} else {
		cloned_ = false;
		d_ = o.d_;
	}
}

bool Lua_ItemInst::IsType(int item_class) {
	Lua_Safe_Call_Bool();
	return self->IsType(static_cast<EQ::item::ItemClass>(item_class));
}

bool Lua_ItemInst::IsStackable() {
	Lua_Safe_Call_Bool();
	return self->IsStackable();
}

bool Lua_ItemInst::IsEquipable(int race, int class_) {
	Lua_Safe_Call_Bool();
	return self->IsEquipable(race, class_);
}

bool Lua_ItemInst::IsEquipable(int slot_id) {
	Lua_Safe_Call_Bool();
	return self->IsEquipable(slot_id);
}

bool Lua_ItemInst::IsExpendable() {
	Lua_Safe_Call_Bool();
	return self->IsExpendable();
}

Lua_ItemInst Lua_ItemInst::GetItem(int slot) {
	Lua_Safe_Call_Class(Lua_ItemInst);
	return Lua_ItemInst(self->GetItem(slot));
}

Lua_Item Lua_ItemInst::GetItem() {
	Lua_Safe_Call_Class(Lua_Item);
	return Lua_Item(self->GetItem());
}

std::string Lua_ItemInst::GetName() {
	Lua_Safe_Call_String();
	return self->GetItem()->Name;
}

int16 Lua_ItemInst::GetItemID(int slot) {
	Lua_Safe_Call_Int();
	return self->GetItemID(slot);
}

int Lua_ItemInst::GetTotalItemCount() {
	Lua_Safe_Call_Int();
	return self->GetTotalItemCount();
}

bool Lua_ItemInst::IsWeapon() {
	Lua_Safe_Call_Bool();
	return self->IsWeapon();
}

bool Lua_ItemInst::IsAmmo() {
	Lua_Safe_Call_Bool();
	return self->IsAmmo();
}

uint32 Lua_ItemInst::GetID() {
	Lua_Safe_Call_Int();
	return self->GetID();
}

int Lua_ItemInst::GetCharges() {
	Lua_Safe_Call_Int();
	return self->GetCharges();
}

void Lua_ItemInst::SetCharges(int charges) {
	Lua_Safe_Call_Void();
	return self->SetCharges(charges);
}

uint32 Lua_ItemInst::GetPrice() {
	Lua_Safe_Call_Int();
	return self->GetPrice();
}

void Lua_ItemInst::SetPrice(uint32 price) {
	Lua_Safe_Call_Void();
	return self->SetPrice(price);
}

uint32 Lua_ItemInst::GetSelfFoundCharacterID() {
	Lua_Safe_Call_Int();
	return self->GetSelfFoundCharacterID();
}

void Lua_ItemInst::SetSelfFoundCharacter(uint32 self_found_character_id, std::string name) {
	Lua_Safe_Call_Void();
	self->SetSelfFoundCharacter(self_found_character_id, name);
}

std::string Lua_ItemInst::GetCustomDataString() {
	Lua_Safe_Call_String();
	return self->GetCustomDataString(true); // We always copy the full map between instances.
}

std::string Lua_ItemInst::GetCustomDataString(bool include_transient_keys) {
	Lua_Safe_Call_String();
	return self->GetCustomDataString(include_transient_keys);
}

void Lua_ItemInst::SetCustomData(std::string identifier, std::string value) {
	Lua_Safe_Call_Void();
	self->SetCustomData(identifier, value);
}

void Lua_ItemInst::SetCustomData(std::string identifier, int value) {
	Lua_Safe_Call_Void();
	self->SetCustomData(identifier, value);
}

void Lua_ItemInst::SetCustomData(std::string identifier, float value) {
	Lua_Safe_Call_Void();
	self->SetCustomData(identifier, value);
}

void Lua_ItemInst::SetCustomData(std::string identifier, bool value) {
	Lua_Safe_Call_Void();
	self->SetCustomData(identifier, value);
}

std::string Lua_ItemInst::GetCustomData(std::string identifier) {
	Lua_Safe_Call_String();
	return self->GetCustomData(identifier);
}

void Lua_ItemInst::DeleteCustomData(std::string identifier) {
	Lua_Safe_Call_Void();
	self->DeleteCustomData(identifier);
}

Lua_ItemInst Lua_ItemInst::Clone() {
	Lua_Safe_Call_Class(Lua_ItemInst);
	return Lua_ItemInst(self->Clone(), true);
}

void Lua_ItemInst::SetTimer(std::string name, uint32 time) {
	Lua_Safe_Call_Void();
	self->SetTimer(name, time);
}

void Lua_ItemInst::StopTimer(std::string name) {
	Lua_Safe_Call_Void();
	self->StopTimer(name);
}

void Lua_ItemInst::ClearTimers() {
	Lua_Safe_Call_Void();
	self->ClearTimers();
}

luabind::scope lua_register_iteminst() {
	return luabind::class_<Lua_ItemInst>("ItemInst")
		.def(luabind::constructor<>())
		.def(luabind::constructor<int>())
		.def(luabind::constructor<int,int>())
		.property("null", &Lua_ItemInst::Null)
		.property("valid", &Lua_ItemInst::Valid)
		.def("IsType", (bool(Lua_ItemInst::*)(int))&Lua_ItemInst::IsType)
		.def("IsStackable", (bool(Lua_ItemInst::*)(void))&Lua_ItemInst::IsStackable)
		.def("IsEquipable", (bool(Lua_ItemInst::*)(int,int))&Lua_ItemInst::IsEquipable)
		.def("IsEquipable", (bool(Lua_ItemInst::*)(int))&Lua_ItemInst::IsEquipable)
		.def("IsExpendable", (bool(Lua_ItemInst::*)(void))&Lua_ItemInst::IsExpendable)
		.def("GetItem", (Lua_ItemInst(Lua_ItemInst::*)(int))&Lua_ItemInst::GetItem)
		.def("GetItemID", (int16(Lua_ItemInst::*)(int))&Lua_ItemInst::GetItemID)
		.def("GetName", (std::string(Lua_ItemInst::*)(void))&Lua_ItemInst::GetName)
		.def("GetTotalItemCount", (int(Lua_ItemInst::*)(void))&Lua_ItemInst::GetTotalItemCount)
		.def("IsWeapon", (bool(Lua_ItemInst::*)(void))&Lua_ItemInst::IsWeapon)
		.def("IsAmmo", (bool(Lua_ItemInst::*)(void))&Lua_ItemInst::IsAmmo)
		.def("GetID", (uint32(Lua_ItemInst::*)(void))&Lua_ItemInst::GetID)
		.def("GetItem", (Lua_Item(Lua_ItemInst::*)(void))&Lua_ItemInst::GetItem)
		.def("GetCharges", (int(Lua_ItemInst::*)(void))&Lua_ItemInst::GetCharges)
		.def("SetCharges", (void(Lua_ItemInst::*)(int))&Lua_ItemInst::SetCharges)
		.def("GetPrice", (uint32(Lua_ItemInst::*)(void))&Lua_ItemInst::GetPrice)
		.def("SetPrice", (void(Lua_ItemInst::*)(uint32))&Lua_ItemInst::SetPrice)
		.def("GetSelfFoundCharacterID", (uint32(Lua_ItemInst::*)(void)) & Lua_ItemInst::GetSelfFoundCharacterID)
		.def("SetSelfFoundCharacter", (void(Lua_ItemInst::*)(uint32,std::string))&Lua_ItemInst::SetSelfFoundCharacter)
		.def("GetCustomDataString", (std::string(Lua_ItemInst::*)(void))&Lua_ItemInst::GetCustomDataString)
		.def("GetCustomDataString", (std::string(Lua_ItemInst::*)(bool))&Lua_ItemInst::GetCustomDataString)
		.def("SetCustomData", (void(Lua_ItemInst::*)(std::string,std::string))&Lua_ItemInst::SetCustomData)
		.def("SetCustomData", (void(Lua_ItemInst::*)(std::string,int))&Lua_ItemInst::SetCustomData)
		.def("SetCustomData", (void(Lua_ItemInst::*)(std::string,float))&Lua_ItemInst::SetCustomData)
		.def("SetCustomData", (void(Lua_ItemInst::*)(std::string,bool))&Lua_ItemInst::SetCustomData)
		.def("GetCustomData", (std::string(Lua_ItemInst::*)(std::string))&Lua_ItemInst::GetCustomData)
		.def("DeleteCustomData", (void(Lua_ItemInst::*)(std::string))&Lua_ItemInst::DeleteCustomData)
		.def("Clone", (Lua_ItemInst(Lua_ItemInst::*)(void))&Lua_ItemInst::Clone)
		.def("SetTimer", (void(Lua_ItemInst::*)(std::string,uint32))&Lua_ItemInst::SetTimer)
		.def("StopTimer", (void(Lua_ItemInst::*)(std::string))&Lua_ItemInst::StopTimer)
		.def("ClearTimers", (void(Lua_ItemInst::*)(void))&Lua_ItemInst::ClearTimers);
}

#endif
