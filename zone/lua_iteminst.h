#ifndef EQEMU_LUA_ITEMINST_H
#define EQEMU_LUA_ITEMINST_H
#ifdef LUA_EQEMU

#include "lua_ptr.h"

namespace EQ
{
	class ItemInstance;
}
class Lua_Item;

namespace luabind {
	struct scope;
}

luabind::scope lua_register_iteminst();

class Lua_ItemInst : public Lua_Ptr<EQ::ItemInstance>
{
	typedef EQ::ItemInstance NativeType;
public:
	Lua_ItemInst(int item_id);
	Lua_ItemInst(int item_id, int charges);
	Lua_ItemInst() : Lua_Ptr(nullptr), cloned_(false) { }
	Lua_ItemInst(EQ::ItemInstance *d) : Lua_Ptr(d), cloned_(false) { }
	Lua_ItemInst(EQ::ItemInstance *d, bool cloned) : Lua_Ptr(d), cloned_(cloned) { }
	Lua_ItemInst& operator=(const Lua_ItemInst& o);
	Lua_ItemInst(const Lua_ItemInst& o);
	virtual ~Lua_ItemInst() { if(cloned_) { EQ::ItemInstance *ptr = GetLuaPtrData(); if(ptr) { delete ptr; } } }

	operator EQ::ItemInstance*() {
		return reinterpret_cast<EQ::ItemInstance*>(GetLuaPtrData());
	}

	bool IsType(int item_class);
	bool IsStackable();
	bool IsEquipable(int race, int class_);
	bool IsEquipable(int slot_id);
	bool IsExpendable();
	Lua_ItemInst GetItem(int slot);
	Lua_Item GetItem();
	std::string GetName();
	int16 GetItemID(int slot);
	int GetTotalItemCount();
	bool IsWeapon();
	bool IsAmmo();
	uint32 GetID();
	int GetCharges();
	void SetCharges(int charges);
	uint32 GetPrice();
	void SetPrice(uint32 price);
	uint32 GetSelfFoundCharacterID();
	void SetSelfFoundCharacter(uint32 self_found_character_id, std::string name);
	std::string GetCustomDataString();
	void SetCustomData(std::string identifier, std::string value);
	void SetCustomData(std::string identifier, int value);
	void SetCustomData(std::string identifier, float value);
	void SetCustomData(std::string identifier, bool value);
	std::string GetCustomData(std::string identifier);
	void DeleteCustomData(std::string identifier);
	Lua_ItemInst Clone();
	void SetTimer(std::string name, uint32 time);
	void StopTimer(std::string name);
	void ClearTimers();

private:
	bool cloned_;
};

#endif
#endif
