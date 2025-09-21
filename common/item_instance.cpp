/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2022 EQEMu Development Team (http://eqemulator.net)

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

#include "inventory_profile.h"
#include "rulesys.h"
#include "shareddb.h"
#include "strings.h"

#include <limits.h>

//#include <iostream>

int32 NextItemInstSerialNumber = 1;

static inline int32 GetNextItemInstSerialNumber() {

	// The Bazaar relies on each item a client has up for Trade having a unique
	// identifier. This 'SerialNumber' is sent in Serialized item packets and
	// is used in Bazaar packets to identify the item a player is buying or inspecting.
	//
	// E.g. A trader may have 3 Five dose cloudy potions, each with a different number of remaining charges
	// up for sale with different prices.
	//
	// NextItemInstSerialNumber is the next one to hand out.
	//
	// It is very unlikely to reach 2,147,483,647. Maybe we should call abort(), rather than wrapping back to 1.
	if (NextItemInstSerialNumber >= INT_MAX) {
		NextItemInstSerialNumber = 1;
	}
	else {
		NextItemInstSerialNumber++;
	}

	return NextItemInstSerialNumber;
}

//
// class EQ::ItemInstance
//
EQ::ItemInstance::ItemInstance(const EQ::ItemData* item, int8 charges, const QuarmItemData& quarm_item_data) {
	if(item) {
		m_item = new EQ::ItemData(*item);
	}

	m_charges = charges;

	if (m_item && m_item->IsClassCommon()) {
		m_color = m_item->Color;
	}

	m_SerialNumber = GetNextItemInstSerialNumber();
	m_quarm_item_data = quarm_item_data;
}

EQ::ItemInstance::ItemInstance(SharedDatabase *db, int16 item_id, int8 charges, const QuarmItemData& quarm_item_data) {

	m_item = db->GetItem(item_id);
	if(m_item) {
		m_item = new EQ::ItemData(*m_item);
	}

	m_charges = charges;

	if (m_item && m_item->IsClassCommon()) {
		m_color = m_item->Color;
	}
	else {
		m_color = 0;
	}

	m_SerialNumber = GetNextItemInstSerialNumber();
	m_quarm_item_data = quarm_item_data;
}

EQ::ItemInstance::ItemInstance(ItemInstTypes use_type) {
	m_use_type = use_type;
}

// Make a copy of an EQ::ItemInstance object
EQ::ItemInstance::ItemInstance(const ItemInstance& copy)
{
	m_use_type=copy.m_use_type;
	if (copy.m_item) {
		m_item = new EQ::ItemData(*copy.m_item);
	}
	else {
		m_item = nullptr;
	}

	m_charges		= copy.m_charges;
	m_price			= copy.m_price;
	m_color			= copy.m_color;
	m_merchantslot	= copy.m_merchantslot;
	m_currentslot	= copy.m_currentslot;
	m_merchantcount	= copy.m_merchantcount;
	// Copy container contents

	for (auto it = copy.m_contents.begin(); it != copy.m_contents.end(); ++it) {
		ItemInstance* inst_old = it->second;
		ItemInstance* inst_new = nullptr;

		if (inst_old) {
			inst_new = inst_old->Clone();
		}

		if (inst_new) {
			m_contents[it->first] = inst_new;
		}
	}
	std::map<std::string, std::string>::const_iterator iter;
	for (iter = copy.m_custom_data.begin(); iter != copy.m_custom_data.end(); ++iter) {
		m_custom_data[iter->first] = iter->second;
	}

	m_SerialNumber	= copy.m_SerialNumber;
	m_custom_data	= copy.m_custom_data;
	m_timers		= copy.m_timers;
	m_cursorqueue	= copy.m_cursorqueue;
	m_quarm_item_data = copy.m_quarm_item_data;
}

// Clean up container contents
EQ::ItemInstance::~ItemInstance()
{
	Clear();
	safe_delete(m_item);
}

// Query item type
bool EQ::ItemInstance::IsType(item::ItemClass item_class) const
{
	// Check usage type
	if ((m_use_type == ItemInstWorldContainer) && (item_class == item::ItemClassBag)) {
		return true;
	}

	if (!m_item) {
		return false;
	}

	return (m_item->ItemClass == item_class);
}

bool EQ::ItemInstance::IsClassCommon()
{
	return (m_item && m_item->IsClassCommon());
}

bool EQ::ItemInstance::IsClassBag()
{
	return (m_item && m_item->IsClassBag());
}

bool EQ::ItemInstance::IsClassBook()
{
	return (m_item && m_item->IsClassBook());
}

// Is item stackable?
bool EQ::ItemInstance::IsStackable() const
{
	if (m_item->Lore[0] == '*' || m_item->Lore[0] == '#') {
		return false;
	}

	return (m_item && m_item->Stackable);
}

// Can item be equipped?
bool EQ::ItemInstance::IsEquipable(uint16 race, uint16 class_) const
{
	if (!m_item || !m_item->Slots) {
		return false;
	}

	return m_item->IsEquipable(race, class_);
}

// Can equip at this slot?
bool EQ::ItemInstance::IsEquipable(int16 slot_id) const
{
	if (!m_item)
		return false;

	// another "shouldn't do" fix..will be fixed in future updates (requires code and database work)
	int16 use_slot = INVALID_INDEX;
	if ((uint16)slot_id <= invslot::EQUIPMENT_END) { use_slot = slot_id; }

	if (use_slot != INVALID_INDEX) 
	{
		// Some 2H items were collected equippable in the secondary slot.
		if (use_slot == invslot::slotSecondary &&
		(m_item->ItemType == item::ItemType2HSlash || m_item->ItemType == item::ItemType2HBlunt || m_item->ItemType == item::ItemType2HPiercing))
		{ return false; }

		if (m_item->Slots & (1 << use_slot)) { return true; }
		else if (slot_id == invslot::slotAmmo && (m_item->ItemType == item::ItemTypeSmallThrowing)) { return true; }
	}

	return false;
}

// Retrieve item inside container
EQ::ItemInstance* EQ::ItemInstance::GetItem(uint8 index) const
{
	auto it = m_contents.find(index);
	if (it != m_contents.end()) {
		return it->second;
	}

	return nullptr;
}

int16 EQ::ItemInstance::GetItemID(uint8 slot) const
{
	const auto item = GetItem(slot);
	if (item) {
		return item->GetID();
	}

	return 0;
}

void EQ::ItemInstance::PutItem(uint8 index, const ItemInstance& inst)
{
	// Clean up item already in slot (if exists)
	DeleteItem(index);


	// Delegate to internal method
	_PutItem(index, inst.Clone());
}

// Remove item inside container
void EQ::ItemInstance::DeleteItem(uint8 index)
{
	ItemInstance* inst = PopItem(index);
	safe_delete(inst);
}

// Remove item from container without memory delete
// Hands over memory ownership to client of this function call
EQ::ItemInstance* EQ::ItemInstance::PopItem(uint8 index)
{
	auto iter = m_contents.find(index);
	if (iter != m_contents.end()) {
		ItemInstance* inst = iter->second;
		m_contents.erase(index);
		return inst; // Return pointer that needs to be deleted (or otherwise managed)
	}

	return nullptr;
}

// Remove all items from container
void EQ::ItemInstance::Clear()
{
	// Destroy container contents
	for (auto iter = m_contents.begin(); iter != m_contents.end(); ++iter) {
		safe_delete(iter->second);
	}
	m_contents.clear();
}

// Remove all items from container
void EQ::ItemInstance::ClearByFlags(byFlagSetting is_nodrop, byFlagSetting is_norent)
{
	// Destroy container contents
	std::map<uint8, ItemInstance*>::const_iterator cur, end, del;
	cur = m_contents.begin();
	end = m_contents.end();
	for (; cur != end;)
	{
		ItemInstance* inst = cur->second;
		if (inst != nullptr)
		{
			const ItemData* item = inst->GetItem();
			del = cur;
			++cur;

			switch (is_nodrop)
			{
			case byFlagSet:
				if (item->NoDrop == 0)
				{
					safe_delete(inst);
					m_contents.erase(del->first);
					continue;
				}
			default:
				break;
			}

			switch (is_norent)
			{
			case byFlagSet:
				if (item->NoRent == 0)
				{
					safe_delete(inst);
					m_contents.erase(del->first);
					continue;
				}
			default:
				break;
			}
		}
	}
}

uint8 EQ::ItemInstance::FirstOpenSlot() const
{
	if (!m_item) {
		return INVALID_INDEX;
	}

	uint8 slots = m_item->BagSlots, i;
	for (i = invbag::SLOT_BEGIN; i < slots; i++) {
		if (!GetItem(i)) {
			break;
		}
	}

	return (i < slots) ? i : INVALID_INDEX;
}

uint8 EQ::ItemInstance::GetTotalItemCount() const
{
	if (!m_item) {
		return 0;
	}

	uint8 item_count = 1;

	if (!m_item->IsClassBag()) { 
		return item_count; 
	}

	for (int index = invbag::SLOT_BEGIN; index < m_item->BagSlots; ++index) {
		if (GetItem(index)) { 
			++item_count; 
		} 
	}

	return item_count;
}

bool EQ::ItemInstance::IsNoneEmptyContainer()
{
	if (!m_item || !m_item->IsClassBag()) {
		return false;
	}

	for (int index = invbag::SLOT_BEGIN; index < m_item->BagSlots; ++index){
		if (GetItem(index)) {
			return true;
		}
	}

	return false;
}

// Has attack/delay?
bool EQ::ItemInstance::IsWeapon() const
{
	if (!m_item || !m_item->IsClassCommon()) {
		return false;
	}

	if (m_item->ItemType == item::ItemTypeArrow && m_item->Damage != 0) {
		return true;
	}
	else {
		return ((m_item->Damage != 0) && (m_item->Delay != 0));
	}
}

bool EQ::ItemInstance::IsAmmo() const {

	if (!m_item) {
		return false;
	}

	if ((m_item->ItemType == item::ItemTypeArrow) ||
		(m_item->ItemType == item::ItemTypeLargeThrowing) ||
		(m_item->ItemType == item::ItemTypeSmallThrowing)) {
		return true;
	}

	return false;

}

const EQ::ItemData* EQ::ItemInstance::GetItem() const 
{
	if (!m_item) {
		return nullptr;
	}

	return m_item;
}

std::string EQ::ItemInstance::GetCustomDataString() const {
	std::string ret_val;
	auto iter = m_custom_data.begin();
	while (iter != m_custom_data.end()) {
		if (ret_val.length() > 0) {
			ret_val += "^";
		}
		ret_val += iter->first;
		ret_val += "^";
		ret_val += iter->second;
		++iter;

		if (ret_val.length() > 0) {
			ret_val += "^";
		}
	}
	return ret_val;
}

std::string EQ::ItemInstance::GetCustomData(std::string identifier) {
	std::map<std::string, std::string>::const_iterator iter = m_custom_data.find(identifier);
	if (iter != m_custom_data.end()) {
		return iter->second;
	}

	return "";
}

void EQ::ItemInstance::SetCustomData(std::string identifier, std::string value) {
	DeleteCustomData(identifier);
	m_custom_data[identifier] = value;
}

void EQ::ItemInstance::SetCustomData(std::string identifier, int value) {
	DeleteCustomData(identifier);
	std::stringstream ss;
	ss << value;
	m_custom_data[identifier] = ss.str();
}

void EQ::ItemInstance::SetCustomData(std::string identifier, float value) {
	DeleteCustomData(identifier);
	std::stringstream ss;
	ss << value;
	m_custom_data[identifier] = ss.str();
}

void EQ::ItemInstance::SetCustomData(std::string identifier, bool value) {
	DeleteCustomData(identifier);
	std::stringstream ss;
	ss << value;
	m_custom_data[identifier] = ss.str();
}

void EQ::ItemInstance::DeleteCustomData(std::string identifier) {
	auto iter = m_custom_data.find(identifier);
	if (iter != m_custom_data.end()) {
		m_custom_data.erase(iter);
	}
}

void EQ::ItemInstance::SetContentsSelfFoundCharacter(uint32 self_found_character_id, const char* name) {
	if (self_found_character_id && m_item && IsClassBag())
	{
		for (auto it = m_contents.begin(); it != m_contents.end(); ++it) {
			ItemInstance* inst = it->second;
			if (inst)
				inst->SetSelfFoundCharacter(self_found_character_id, name);
		}
	}
}

bool EQ::ItemInstance::IsMatchingSelfFoundCharacterID(uint32 self_found_character_id, bool check_all_bag_contents) const
{
	if (!self_found_character_id)
		return false;

	// We don't track self-found stackables
	if (IsStackable())
		return false;

	// Check that all bag contents are also self found
	if (check_all_bag_contents && IsClassBag())
	{
		for (auto it = m_contents.begin(); it != m_contents.end(); ++it) {
			ItemInstance* inst = it->second;
			if (inst && !inst->IsMatchingSelfFoundCharacterID(self_found_character_id, false)) {
				return false;
			}
		}
	}

	return GetSelfFoundCharacterID() == self_found_character_id;
}

// Clone a type of EQ::ItemInstance object
// c++ doesn't allow a polymorphic copy constructor,
// so we have to resort to a polymorphic Clone()
EQ::ItemInstance* EQ::ItemInstance::Clone() const
{
	// Pseudo-polymorphic copy constructor
	return new ItemInstance(*this);
}

bool EQ::ItemInstance::IsSlotAllowed(int16 slot_id, EQ::InventoryProfile &inv) const
{
	if (!m_item) { return false; }
	if (InventoryProfile::SupportsContainers(slot_id)) { return true; }
	if (slot_id > invslot::EQUIPMENT_END) { return true; }

	if (slot_id >= invslot::EQUIPMENT_BEGIN && slot_id <= invslot::EQUIPMENT_END)
	{
		if (m_item->ItemClass == EQ::item::ItemClassBag || m_item->ItemClass == EQ::item::ItemClassBook)
			return false;

		if (slot_id == invslot::slotRange)
		{
			if (m_item->ItemType == item::ItemTypeArrow || m_item->ItemType == item::ItemTypeUnknown4) // type 28 isn't actually used on any items but this logic is in the client
				return false;
			if (m_item->Range)
				return true;
		}
		else if (slot_id == invslot::slotAmmo)
		{
			if (m_item->ItemType == item::ItemTypeArrow || m_item->ItemType == item::ItemTypeSmallThrowing)
				return true;
		}
		if (m_item->ItemType == item::ItemTypeUnknown4) // type 28 isn't actually used on any items but this logic is in the client
			return true;

		if ((m_item->Slots & (1 << slot_id)) == 0) { return false; }

		if (slot_id == invslot::slotPrimary)
		{
			auto secondary_item = inv[invslot::slotSecondary];
			if (secondary_item)
			{
				if (m_item->ItemType == item::ItemType2HBlunt ||
					m_item->ItemType == item::ItemType2HSlash ||
					m_item->ItemType == item::ItemType2HPiercing ||
					secondary_item->m_item->ItemType == item::ItemTypeWindInstrument ||
					secondary_item->m_item->ItemType == item::ItemTypeStringedInstrument ||
					secondary_item->m_item->ItemType == item::ItemTypeBrassInstrument ||
					secondary_item->m_item->ItemType == item::ItemTypePercussionInstrument)
				{
					return false;
				}
			}
		}
		else if (slot_id == invslot::slotSecondary)
		{
			if (m_item->ItemType != item::ItemType2HBlunt &&
				m_item->ItemType != item::ItemType2HSlash &&
				m_item->ItemType != item::ItemType2HPiercing)
			{
				// EQ_Equipment::IsWeapon
				bool IsWeapon = true;
				if (m_item->Damage == 0 || m_item->ItemType == item::ItemTypeUnknown6 || m_item->ItemType == item::ItemTypeUnknown7 || m_item->ItemType == item::ItemTypeArrow || m_item->ItemType == item::ItemTypeUnknown4)
					IsWeapon = false;

				bool CanDualWield = true; // TODO - get this value from the character

				if (!IsWeapon || CanDualWield)
				{
					auto primary_item = inv[invslot::slotPrimary];
					if (!primary_item)
						return true;

					if (m_item->ItemType != item::ItemTypeWindInstrument &&
						m_item->ItemType != item::ItemTypeStringedInstrument &&
						m_item->ItemType != item::ItemTypeBrassInstrument &&
						m_item->ItemType != item::ItemTypePercussionInstrument)
					{
						if (primary_item->m_item->ItemType != item::ItemType2HBlunt &&
							primary_item->m_item->ItemType != item::ItemType2HSlash &&
							primary_item->m_item->ItemType != item::ItemType2HPiercing)
						{
							return true;
						}
					}
				}

				return false;
			}
		}
	}

	return true;
}

void EQ::ItemInstance::Initialize(SharedDatabase *db) {
	// if there's no actual item, don't do anything
	if (!m_item)
		return;
}

void EQ::ItemInstance::SetTimer(std::string name, uint32 time) {
	Timer t(time);
	t.Start(time, false);
	m_timers[name] = t;
}

void EQ::ItemInstance::StopTimer(std::string name) {
	auto iter = m_timers.find(name);
	if(iter != m_timers.end()) {
		m_timers.erase(iter);
	}
}

void EQ::ItemInstance::ClearTimers() {
	m_timers.clear();
}

bool EQ::ItemInstance::IsFlesh() const
{
	if (!m_item || m_item->ItemClass != EQ::item::ItemClassCommon || !m_item->NoRent)
		return false;

	if (m_item->Icon == 797 ||
		m_item->Icon == 814 ||
		m_item->Icon == 815 ||
		m_item->Icon == 816 ||
		m_item->Icon == 817 ||
		m_item->Icon == 924 ||
		m_item->Icon == 925 ||
		m_item->Icon == 1003 ||
		m_item->Icon == 1139 ||
		m_item->Icon == 1199 ||
		m_item->Icon == 1238 ||
		m_item->Icon == 1202)
		return true;

	return false;
}

bool EQ::ItemInstance::IsTieredPotion() const
{
	if (!m_item || m_item->ItemType != item::ItemTypePotion)
	{
		return false;
	}
	else if (m_item->Click.Effect == 49 || (m_item->Click.Effect >= 1252 && m_item->Click.Effect <= 1266) || m_item->Click.Effect == 3999)
	{
		return true;
	}

	return false;
}