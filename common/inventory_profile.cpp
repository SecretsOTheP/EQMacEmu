/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2016 EQEMu Development Team (http://eqemulator.net)
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
#include "textures.h"
#include "eqemu_logsys.h"
#include "strings.h"

#include "../common/light_source.h"

//#include <limits.h>

#include <iostream>

std::list<EQ::ItemInstance*> dirty_inst;

//
// class ItemInstQueue
//
ItemInstQueue::~ItemInstQueue() {
	for (auto iter = m_list.begin(); iter != m_list.end(); ++iter) {
		safe_delete(*iter);
	}
	m_list.clear();
}

// Put item onto back of queue
void ItemInstQueue::push(EQ::ItemInstance* inst)
{
	m_list.push_back(inst);
}

// Put item onto front of queue
void ItemInstQueue::push_front(EQ::ItemInstance* inst)
{
	m_list.push_front(inst);
}

// Remove item from front of queue
EQ::ItemInstance* ItemInstQueue::pop()
{
	if (m_list.empty())
		return nullptr;

	EQ::ItemInstance* inst = m_list.front();
	m_list.pop_front();
	return inst;
}

// Look at item at front of queue
EQ::ItemInstance* ItemInstQueue::peek_front() const
{
	return (m_list.empty()) ? nullptr : m_list.front();
}


//
// class EQ::InventoryProfile
//
EQ::InventoryProfile::~InventoryProfile() {
	for (auto iter = m_worn.begin(); iter != m_worn.end(); ++iter) {
		safe_delete(iter->second);
	}
	m_worn.clear();

	for (auto iter = m_inv.begin(); iter != m_inv.end(); ++iter) {
		safe_delete(iter->second);
	}
	m_inv.clear();

	for (auto iter = m_bank.begin(); iter != m_bank.end(); ++iter) {
		safe_delete(iter->second);
	}
	m_bank.clear();

	for (auto iter = m_trade.begin(); iter != m_trade.end(); ++iter) {
		safe_delete(iter->second);
	}
	m_trade.clear();
}

bool EQ::InventoryProfile::SetInventoryVersion(versions::MobVersion inventory_version) {
	if (!m_mob_version_set) {
		m_mob_version = versions::ValidateMobVersion(inventory_version);
		m_lookup = inventory::Lookup(m_mob_version);
		m_mob_version_set = true;
		return true;
	}
	else {
		m_lookup = inventory::Lookup(versions::MobVersion::Unknown);
		Log(Logs::General, Logs::Error, "InventoryVersion set request after initial set (old: %u, new: %u)",
			static_cast<uint32>(m_mob_version), static_cast<uint32>(inventory_version));
		return false;
	}
}

void EQ::InventoryProfile::CleanDirty() {
	auto iter = dirty_inst.begin();
	while (iter != dirty_inst.end()) {
		delete (*iter);
		++iter;
	}
	dirty_inst.clear();
}

void EQ::InventoryProfile::MarkDirty(ItemInstance* inst) {
	if (inst) {
		dirty_inst.push_back(inst);
	}
}

// Retrieve item at specified slot; returns false if item not found
EQ::ItemInstance* EQ::InventoryProfile::GetItem(int16 slot_id) const
{
	ItemInstance* result = nullptr;

	// Cursor
	if (slot_id == invslot::slotCursor) {
		// Cursor slot
		result = m_cursor.peek_front();
	}

	// Non bag slots
	else if (slot_id >= invslot::TRADE_BEGIN && slot_id <= invslot::TRADE_END) {
		result = _GetItem(m_trade, slot_id);
	}
	else if (slot_id >= invslot::BANK_BEGIN && slot_id <= invslot::BANK_END) {
		// Bank slots
		result = _GetItem(m_bank, slot_id);
	}
	else if ((slot_id >= invslot::GENERAL_BEGIN && slot_id <= invslot::GENERAL_END)) {
		// Personal inventory slots
		result = _GetItem(m_inv, slot_id);
	}
	else if ((slot_id >= invslot::EQUIPMENT_BEGIN && slot_id <= invslot::EQUIPMENT_END)) {
		// Equippable slots (on body)
		result = _GetItem(m_worn, slot_id);
	}

	// Inner bag slots
	else if (slot_id >= invbag::TRADE_BAGS_BEGIN && slot_id <= invbag::TRADE_BAGS_END) {
		// Trade bag slots
		ItemInstance* inst = _GetItem(m_trade, InventoryProfile::CalcSlotId(slot_id));
		if (inst && inst->IsClassBag()) {
			result = inst->GetItem(InventoryProfile::CalcBagIdx(slot_id));
		}
	}
	else if (slot_id >= invbag::BANK_BAGS_BEGIN && slot_id <= invbag::BANK_BAGS_END) {
		// Bank bag slots
		ItemInstance* inst = _GetItem(m_bank, InventoryProfile::CalcSlotId(slot_id));
		if (inst && inst->IsClassBag()) {
			result = inst->GetItem(InventoryProfile::CalcBagIdx(slot_id));
		}
	}
	else if (slot_id >= invbag::CURSOR_BAG_BEGIN && slot_id <= invbag::CURSOR_BAG_END) {
		// Cursor bag slots
		ItemInstance* inst = m_cursor.peek_front();
		if (inst && inst->IsClassBag()) {
			result = inst->GetItem(InventoryProfile::CalcBagIdx(slot_id));
		}
	}
	else if (slot_id >= invbag::GENERAL_BAGS_BEGIN && slot_id <= invbag::GENERAL_BAGS_END) {
		// Personal inventory bag slots
		ItemInstance* inst = _GetItem(m_inv, InventoryProfile::CalcSlotId(slot_id));
		if (inst && inst->IsClassBag()) {
			result = inst->GetItem(InventoryProfile::CalcBagIdx(slot_id));
		}
	}

	return result;
}

// Retrieve item at specified position within bag
EQ::ItemInstance* EQ::InventoryProfile::GetItem(int16 slot_id, uint8 bagidx) const
{
	return GetItem(InventoryProfile::CalcSlotId(slot_id, bagidx));
}

// Put an item snto specified slot
int16 EQ::InventoryProfile::PutItem(int16 slot_id, const ItemInstance& inst)
{
	// Clean up item already in slot (if exists)
	DeleteItem(slot_id);

	if (!inst) {
		// User is effectively deleting the item
		// in the slot, why hold a null ptr in map<>?
		return slot_id;
	}

	// Delegate to internal method
	return _PutItem(slot_id, inst.Clone());
}

// Put an item snto specified slot
int16 EQ::InventoryProfile::RefPutItem(int16 slot_id, ItemInstance* inst)
{
	// Clean up item already in slot (if exists)
	DeleteItem(slot_id);

	if (!inst) {
		// User is effectively deleting the item
		// in the slot, why hold a null ptr in map<>?
		return slot_id;
	}

	// Delegate to internal method
	return _PutItem(slot_id, inst);
}


int16 EQ::InventoryProfile::PushCursor(const ItemInstance& inst, bool push_front)
{
	if (push_front)
		m_cursor.push_front(inst.Clone());
	else
		m_cursor.push(inst.Clone());

	return invslot::slotCursor;
}

// Swap items in inventory
bool EQ::InventoryProfile::SwapItem(int16 slot_a, int16 slot_b)
{
	// Temp holding areas for a and b
	ItemInstance* inst_a = GetItem(slot_a);
	ItemInstance* inst_b = GetItem(slot_b);

	if (inst_a) { if (!inst_a->IsSlotAllowed(slot_b, *this)) { return false; } }
	if (inst_b) { if (!inst_b->IsSlotAllowed(slot_a, *this)) { return false; } }

	_PutItem(slot_a, inst_b); // Copy b->a
	_PutItem(slot_b, inst_a); // Copy a->b

	return true;
}

// Remove item from inventory (with memory delete)
bool EQ::InventoryProfile::DeleteItem(int16 slot_id, uint8 quantity)
{
	// Pop item out of inventory map (or queue)
	ItemInstance* item_to_delete = PopItem(slot_id);

	// Determine if object should be fully deleted, or
	// just a quantity of charges of the item can be deleted
	if (item_to_delete && (quantity > 0)) {

		item_to_delete->SetCharges(item_to_delete->GetCharges() - quantity);

		// If there are no charges left on the item,
		if (item_to_delete->GetCharges() <= 0) {
			// If the item is stackable (e.g arrows), or
			// the item is not stackable, and is not a charged item, or is expendable, delete it
			if (item_to_delete->IsStackable() ||
				(!item_to_delete->IsStackable() &&
					((item_to_delete->GetItem()->MaxCharges == 0) || item_to_delete->IsExpendable()))) {
				// Item can now be destroyed
				InventoryProfile::MarkDirty(item_to_delete);
				return true;
			}
		}

		// Charges still exist, or it is a charged item that is not expendable. Put back into inventory
		_PutItem(slot_id, item_to_delete);
		return false;
	}

	InventoryProfile::MarkDirty(item_to_delete);
	return true;

}

// Checks All items in a bag for No Drop
bool EQ::InventoryProfile::CheckNoDrop(int16 slot_id) {
	ItemInstance* inst = GetItem(slot_id);
	if (!inst) 
		return false;

	if (!inst->GetItem()->NoDrop) 
		return true;

	if (inst->GetItem()->ItemClass == 1) {
		for (uint8 i = invbag::SLOT_BEGIN; i <= invbag::SLOT_END; i++) {
			ItemInstance* bagitem = GetItem(InventoryProfile::CalcSlotId(slot_id, i));
			if (bagitem && !bagitem->GetItem()->NoDrop)
				return true;
		}
	}
	return false;
}

// Remove item from bucket without memory delete
// Returns item pointer if full delete was successful
EQ::ItemInstance* EQ::InventoryProfile::PopItem(int16 slot_id)
{
	ItemInstance* p = nullptr;

	if (slot_id == invslot::slotCursor) {
		p = m_cursor.pop();
	}
	else if ((slot_id >= invslot::EQUIPMENT_BEGIN && slot_id <= invslot::EQUIPMENT_END)) {
		p = m_worn[slot_id];
		m_worn.erase(slot_id);
	}
	else if ((slot_id >= invslot::GENERAL_BEGIN && slot_id <= invslot::GENERAL_END)) {
		p = m_inv[slot_id];
		m_inv.erase(slot_id);
	}
	else if (slot_id >= invslot::BANK_BEGIN && slot_id <= invslot::BANK_END) {
		p = m_bank[slot_id];
		m_bank.erase(slot_id);
	}
	else if (slot_id >= invslot::TRADE_BEGIN && slot_id <= invslot::TRADE_END) {
		p = m_trade[slot_id];
		m_trade.erase(slot_id);
	}
	else {
		// Is slot inside bag?
		ItemInstance* baginst = GetItem(InventoryProfile::CalcSlotId(slot_id));
		if (baginst != nullptr && baginst->IsClassBag()) {
			p = baginst->PopItem(InventoryProfile::CalcBagIdx(slot_id));
		}
	}

	// Return pointer that needs to be deleted (or otherwise managed)
	return p;
}

bool EQ::InventoryProfile::HasSpaceForItem(const ItemData* ItemToTry, int16 Quantity) {

	if (ItemToTry->Stackable) {

		for (int16 i = invslot::GENERAL_BEGIN; i <= invslot::GENERAL_END; i++) {

			ItemInstance* InvItem = GetItem(i);

			if (InvItem && (InvItem->GetItem()->ID == ItemToTry->ID) && (InvItem->GetCharges() < InvItem->GetItem()->StackSize)) {

				int ChargeSlotsLeft = InvItem->GetItem()->StackSize - InvItem->GetCharges();

				if (Quantity <= ChargeSlotsLeft)
					return true;

				Quantity -= ChargeSlotsLeft;

			}
			if (InvItem && InvItem->IsClassBag()) {

				int16 BaseSlotID = InventoryProfile::CalcSlotId(i, invbag::SLOT_BEGIN);
				uint8 BagSize = InvItem->GetItem()->BagSlots;
				for (uint8 BagSlot = invbag::SLOT_BEGIN; BagSlot < BagSize; BagSlot++) {

					InvItem = GetItem(BaseSlotID + BagSlot);

					if (InvItem && (InvItem->GetItem()->ID == ItemToTry->ID) &&
						(InvItem->GetCharges() < InvItem->GetItem()->StackSize)) {

						int ChargeSlotsLeft = InvItem->GetItem()->StackSize - InvItem->GetCharges();

						if (Quantity <= ChargeSlotsLeft)
							return true;

						Quantity -= ChargeSlotsLeft;
					}
				}
			}
		}
	}

	for (int16 i = invslot::GENERAL_BEGIN; i <= invslot::GENERAL_END; i++) {

		ItemInstance* InvItem = GetItem(i);

		if (!InvItem) {

			if (!ItemToTry->Stackable) {

				if (Quantity == 1)
					return true;
				else
					Quantity--;
			}
			else {
				if (Quantity <= ItemToTry->StackSize)
					return true;
				else
					Quantity -= ItemToTry->StackSize;
			}

		}
		else if (InvItem->IsClassBag() && CanItemFitInContainer(ItemToTry, InvItem->GetItem())) {

			int16 BaseSlotID = InventoryProfile::CalcSlotId(i, invbag::SLOT_BEGIN);

			uint8 BagSize = InvItem->GetItem()->BagSlots;

			for (uint8 BagSlot = invbag::SLOT_BEGIN; BagSlot < BagSize; BagSlot++) {

				InvItem = GetItem(BaseSlotID + BagSlot);

				if (!InvItem) {
					if (!ItemToTry->Stackable) {

						if (Quantity == 1)
							return true;
						else
							Quantity--;
					}
					else {
						if (Quantity <= ItemToTry->StackSize)
							return true;
						else
							Quantity -= ItemToTry->StackSize;
					}
				}
			}
		}
	}

	return false;

}

// Checks that user has at least 'quantity' number of items in a given inventory slot
// Returns first slot it was found in, or INVALID_INDEX if not found

//This function has a flaw in that it only returns the last stack that it looked at
//when quantity is greater than 1 and not all of quantity can be found in 1 stack.
int16 EQ::InventoryProfile::HasItem(int16 item_id, uint8 quantity, uint8 where)
{
	int16 slot_id = INVALID_INDEX;

	//Altered by Father Nitwit to support a specification of
	//where to search, with a default value to maintain compatibility

	// Check each inventory bucket
	if (where & invWhereWorn) {
		slot_id = _HasItem(m_worn, item_id, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWherePersonal) {
		slot_id = _HasItem(m_inv, item_id, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWhereBank) {
		slot_id = _HasItem(m_bank, item_id, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWhereTrading) {
		slot_id = _HasItem(m_trade, item_id, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWhereCursor) {
		// Check cursor queue
		slot_id = _HasItem(m_cursor, item_id, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	return slot_id;
}

int16 EQ::InventoryProfile::HasArtifactItem()
{
	int16 slot_id = INVALID_INDEX;

	for (int16 i = invslot::slotCursor; i <= invslot::GENERAL_END; i++)
	{
		if (GetItem(i) && GetItem(i)->GetItem()->Lore[0] == '#')
			return i;
	}

	for (int16 i = invbag::GENERAL_BAGS_BEGIN; i <= invbag::GENERAL_BAGS_END; i++)
	{
		if (GetItem(i) && GetItem(i)->GetItem()->Lore[0] == '#')
			return i;
	}

	for (int16 i = invslot::BANK_BEGIN; i <= invslot::BANK_END; i++)
	{
		if (GetItem(i) && GetItem(i)->GetItem()->Lore[0] == '#')
			return i;
	}

	for (int16 i = invbag::BANK_BAGS_BEGIN; i <= invbag::BANK_BAGS_END; i++)
	{
		if (GetItem(i) && GetItem(i)->GetItem()->Lore[0] == '#')
			return i;
	}

	for (int16 i = invslot::TRADE_BEGIN; i <= invslot::TRADE_END; i++)
	{
		if (GetItem(i) && GetItem(i)->GetItem()->Lore[0] == '#')
			return i;
	}

	for (int16 i = invbag::TRADE_BAGS_BEGIN; i <= invbag::TRADE_BAGS_END; i++)
	{
		if (GetItem(i) && GetItem(i)->GetItem()->Lore[0] == '#')
			return i;
	}

	return slot_id;
}

//this function has the same quantity flaw mentioned above in HasItem()
int16 EQ::InventoryProfile::HasItemByUse(uint8 use, uint8 quantity, uint8 where)
{
	int16 slot_id = INVALID_INDEX;

	// Check each inventory bucket
	if (where & invWhereWorn) {
		slot_id = _HasItemByUse(m_worn, use, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWherePersonal) {
		slot_id = _HasItemByUse(m_inv, use, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWhereBank) {
		slot_id = _HasItemByUse(m_bank, use, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWhereTrading) {
		slot_id = _HasItemByUse(m_trade, use, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	if (where & invWhereCursor) {
		// Check cursor queue
		slot_id = _HasItemByUse(m_cursor, use, quantity);
		if (slot_id != INVALID_INDEX)
			return slot_id;
	}

	return slot_id;
}

// Locate an available inventory slot
// Returns slot_id when there's one available, else INVALID_INDEX
int16 EQ::InventoryProfile::FindFreeSlot(bool for_bag, bool try_cursor, uint8 min_size, bool is_arrow)
{
	// Check basic inventory
	for (int16 i = invslot::GENERAL_BEGIN; i <= invslot::GENERAL_END; i++) {
		if (!GetItem(i))
			// Found available slot in personal inventory
			return i;
	}

	if (!for_bag) {
		for (int16 i = invslot::GENERAL_BEGIN; i <= invslot::GENERAL_END; i++) {
			const ItemInstance* inst = GetItem(i);
			if (inst && inst->IsClassBag()
				&& inst->GetItem()->BagSize >= min_size)
			{
				if (is_arrow && inst->GetItem()->BagType == item::BagTypeQuiver)
				{
					int16 base_slot_id = InventoryProfile::CalcSlotId(i, invslot::SLOT_BEGIN);

					uint8 slots = inst->GetItem()->BagSlots;
					uint8 j;
					for (j = invslot::SLOT_BEGIN; j < slots; j++) {
						if (!GetItem(base_slot_id + j))
							// Found available slot within bag
							return (base_slot_id + j);
					}
				}
				if (inst->GetItem()->BagType == item::BagTypeQuiver && inst->GetItem()->ItemType != item::ItemTypeArrow)
				{
					continue;
				}

				int16 base_slot_id = InventoryProfile::CalcSlotId(i, invslot::SLOT_BEGIN);

				uint8 slots = inst->GetItem()->BagSlots;
				uint8 j;
				for (j = invslot::SLOT_BEGIN; j < slots; j++) {
					if (!GetItem(base_slot_id + j))
						// Found available slot within bag
						return (base_slot_id + j);
				}
			}
		}
	}

	if (try_cursor)
		// Always room on cursor (it's a queue)
		// (we may wish to cap this in the future)
		return invslot::slotCursor;

	// No available slots
	return INVALID_INDEX;
}


// This is a mix of HasSpaceForItem and FindFreeSlot..due to existing coding behavior, it was better to add a new helper function...
int16 EQ::InventoryProfile::FindFreeSlotForTradeItem(const ItemInstance* inst) {
	// Do not arbitrarily use this function..it is designed for use with Client::ResetTrade() and Client::FinishTrade().
	// If you have a need, use it..but, understand it is not a compatible replacement for Inventory::FindFreeSlot().
	//
	// I'll probably implement a bitmask in the new inventory system to avoid having to adjust stack bias -U

	if (!inst || !inst->GetID())
		return INVALID_INDEX;

	// step 1: find room for bags (caller should really ask for slots for bags first to avoid sending them to cursor..and bag item loss)
	if (inst->IsClassBag()) {
		for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot)
			if (!m_inv[free_slot])
				return free_slot;

		// Do not queue on cursor.
		if (!GetItem(invslot::slotCursor))
			return invslot::slotCursor;
		else
			return INVALID_INDEX;
	}

	// step 2: find partial room for stackables
	if (inst->IsStackable()) {
		for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot) {
			const ItemInstance* main_inst = m_inv[free_slot];

			if (!main_inst)
				continue;

			if ((main_inst->GetID() == inst->GetID()) && (main_inst->GetCharges() < main_inst->GetItem()->StackSize))
				return free_slot;
		}

		for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot) {
			const ItemInstance* main_inst = m_inv[free_slot];

			if (!main_inst)
				continue;

			if (main_inst->IsClassBag()) { // if item-specific containers already have bad items, we won't fix it here...
				for (uint8 free_bag_slot = invbag::SLOT_BEGIN; (free_bag_slot < main_inst->GetItem()->BagSlots) && (free_bag_slot <= invbag::SLOT_END); ++free_bag_slot) {
					const ItemInstance* sub_inst = main_inst->GetItem(free_bag_slot);

					if (!sub_inst)
						continue;

					if ((sub_inst->GetID() == inst->GetID()) && (sub_inst->GetCharges() < sub_inst->GetItem()->StackSize))
						return InventoryProfile::CalcSlotId(free_slot, free_bag_slot);
				}
			}
		}
	}

	// step 3a: find room for container-specific items (ItemClassArrow)
	if (inst->GetItem()->ItemType == item::ItemTypeArrow) {
		for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot) {
			const ItemInstance* main_inst = m_inv[free_slot];

			if (!main_inst || (main_inst->GetItem()->BagType != item::BagTypeQuiver) || !main_inst->IsClassBag() || main_inst->GetItem()->BagSize < inst->GetItem()->Size)
				continue;

			for (uint8 free_bag_slot = invbag::SLOT_BEGIN; (free_bag_slot < main_inst->GetItem()->BagSlots) && (free_bag_slot <= invbag::SLOT_END); ++free_bag_slot)
				if (!main_inst->GetItem(free_bag_slot))
					return InventoryProfile::CalcSlotId(free_slot, free_bag_slot);
		}
	}

	// step 3b: find room for container-specific items (ItemClassSmallThrowing)
	if (inst->GetItem()->ItemType == item::ItemTypeSmallThrowing) {
		for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot) {
			const ItemInstance* main_inst = m_inv[free_slot];

			if (!main_inst || !main_inst->IsClassBag() || main_inst->GetItem()->BagSize < inst->GetItem()->Size)
				continue;

			for (uint8 free_bag_slot = invbag::SLOT_BEGIN; (free_bag_slot < main_inst->GetItem()->BagSlots) && (free_bag_slot <= invbag::SLOT_END); ++free_bag_slot)
				if (!main_inst->GetItem(free_bag_slot))
					return InventoryProfile::CalcSlotId(free_slot, free_bag_slot);
		}
	}

	// step 4: just find an empty slot
	for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot) {
		const ItemInstance* main_inst = m_inv[free_slot];

		if (!main_inst)
			return free_slot;
	}

	for (int16 free_slot = invslot::GENERAL_BEGIN; free_slot <= invslot::GENERAL_END; ++free_slot) {
		const ItemInstance* main_inst = m_inv[free_slot];

		if (main_inst && main_inst->IsClassBag()) {
			if ((main_inst->GetItem()->BagSize < inst->GetItem()->Size) || (main_inst->GetItem()->BagType == item::BagTypeQuiver))
				continue;

			for (uint8 free_bag_slot = invbag::SLOT_BEGIN; (free_bag_slot < main_inst->GetItem()->BagSlots) && (free_bag_slot <= invbag::SLOT_END); ++free_bag_slot)
				if (!main_inst->GetItem(free_bag_slot))
					return InventoryProfile::CalcSlotId(free_slot, free_bag_slot);
		}
	}

	// Do not queue on cursor.
	if (!GetItem(invslot::slotCursor))
		return invslot::slotCursor;
	else
		return INVALID_INDEX;
}

// Opposite of below: Get parent bag slot_id from a slot inside of bag
int16 EQ::InventoryProfile::CalcSlotId(int16 slot_id) {
	int16 parent_slot_id = INVALID_INDEX;

	if (slot_id >= invbag::GENERAL_BAGS_BEGIN && slot_id <= invbag::GENERAL_BAGS_END)
		parent_slot_id = invslot::GENERAL_BEGIN + (slot_id - invbag::GENERAL_BAGS_BEGIN) / invbag::SLOT_COUNT;

	else if (slot_id >= invbag::CURSOR_BAG_BEGIN && slot_id <= invbag::CURSOR_BAG_END)
		parent_slot_id = invslot::slotCursor;

	/*
	// this is not a bag range... using this risks over-writing existing items
	else if (slot_id >= invslot::BANK_BEGIN && slot_id <= invslot::BANK_END)
		parent_slot_id = invslot::BANK_BEGIN + (slot_id - invslot::BANK_BEGIN) / invbag::SLOT_COUNT;
	*/

	else if (slot_id >= invbag::BANK_BAGS_BEGIN && slot_id <= invbag::BANK_BAGS_END)
		parent_slot_id = invslot::BANK_BEGIN + (slot_id - invbag::BANK_BAGS_BEGIN) / invbag::SLOT_COUNT;

	//else if (slot_id >= 3100 && slot_id <= 3179) should be {3031..3110}..where did this range come from!!? (verified db save range)
	else if (slot_id >= invbag::TRADE_BAGS_BEGIN && slot_id <= invbag::TRADE_BAGS_END)
		parent_slot_id = invslot::TRADE_BEGIN + (slot_id - invbag::TRADE_BAGS_BEGIN) / invbag::SLOT_COUNT;

	return parent_slot_id;
}

// Calculate slot_id for an item within a bag
int16 EQ::InventoryProfile::CalcSlotId(int16 bagslot_id, uint8 bagidx) {
	if (!InventoryProfile::SupportsContainers(bagslot_id))
		return INVALID_INDEX;

	int16 slot_id = INVALID_INDEX;

	if (bagslot_id == invslot::slotCursor || bagslot_id == invslot::CURSOR_QUEUE_BEGIN)
		slot_id = invbag::CURSOR_BAG_BEGIN + bagidx;

	else if (bagslot_id >= invslot::GENERAL_BEGIN && bagslot_id <= invslot::GENERAL_END)
		slot_id = invbag::GENERAL_BAGS_BEGIN + (bagslot_id - invslot::GENERAL_BEGIN) * invbag::SLOT_COUNT + bagidx;

	else if (bagslot_id >= invslot::BANK_BEGIN && bagslot_id <= invslot::BANK_END)
		slot_id = invbag::BANK_BAGS_BEGIN + (bagslot_id - invslot::BANK_BEGIN) * invbag::SLOT_COUNT + bagidx;

	else if (bagslot_id >= invslot::TRADE_BEGIN && bagslot_id <= invslot::TRADE_END)
		slot_id = invbag::TRADE_BAGS_BEGIN + (bagslot_id - invslot::TRADE_BEGIN) * invbag::SLOT_COUNT + bagidx;

	return slot_id;
}

uint8 EQ::InventoryProfile::CalcBagIdx(int16 slot_id) {
	uint8 index = 0;

	if (slot_id >= invbag::GENERAL_BAGS_BEGIN && slot_id <= invbag::GENERAL_BAGS_END)
		index = (slot_id - invbag::GENERAL_BAGS_BEGIN) % invbag::SLOT_COUNT;

	else if (slot_id >= invbag::CURSOR_BAG_BEGIN && slot_id <= invbag::CURSOR_BAG_END)
		index = (slot_id - invbag::CURSOR_BAG_BEGIN); // % invbag::SLOT_COUNT; - not needed since range is 10 slots

	/*
	// this is not a bag range... using this risks over-writing existing items
	else if (slot_id >= invslot::BANK_BEGIN && slot_id <= invslot::BANK_END)
		index = (slot_id - invslot::BANK_BEGIN) % invbag::SLOT_COUNT;
	*/

	else if (slot_id >= invbag::BANK_BAGS_BEGIN && slot_id <= invbag::BANK_BAGS_END)
		index = (slot_id - invbag::BANK_BAGS_BEGIN) % invbag::SLOT_COUNT;

	else if (slot_id >= invbag::TRADE_BAGS_BEGIN && slot_id <= invbag::TRADE_BAGS_END)
		index = (slot_id - invbag::TRADE_BAGS_BEGIN) % invbag::SLOT_COUNT;

	// odd..but, ok... (probably a range-slot conversion for ItemInstance* Object::item
	else if (slot_id >= invslot::WORLD_BEGIN && slot_id <= invslot::WORLD_END)
		index = (slot_id - invslot::WORLD_BEGIN); // % invbag::SLOT_COUNT; - not needed since range is 10 slots

	return index;
}

int16 EQ::InventoryProfile::CalcSlotFromMaterial(uint8 material)
{
	switch (material)
	{
	case textures::armorHead:
		return invslot::slotHead;
	case textures::armorChest:
		return invslot::slotChest;
	case textures::armorArms:
		return invslot::slotArms;
	case textures::armorWrist:
		return invslot::slotWrist1;	// Always prefer wrist1. 
	case textures::armorHands:
		return invslot::slotHands;
	case textures::armorLegs:
		return invslot::slotLegs;
	case textures::armorFeet:
		return invslot::slotFeet;
	case textures::weaponPrimary:
		return invslot::slotPrimary;
	case textures::weaponSecondary:
		return invslot::slotSecondary;
	default:
		return INVALID_INDEX;
	}
}

uint8 EQ::InventoryProfile::CalcMaterialFromSlot(int16 equipslot)
{
	switch (equipslot)
	{
	case invslot::slotHead:
		return textures::armorHead;
	case invslot::slotChest:
		return textures::armorChest;
	case invslot::slotArms:
		return textures::armorArms;
	case invslot::slotWrist1:
	case invslot::slotWrist2:
		return textures::armorWrist; // Both wrists share a single texture slot.
	case invslot::slotHands:
		return textures::armorHands;
	case invslot::slotLegs:
		return textures::armorLegs;
	case invslot::slotFeet:
		return textures::armorFeet;
	case invslot::slotPrimary:
		return textures::weaponPrimary;
	case invslot::slotSecondary:
		return textures::weaponSecondary;
	default:
		return textures::materialInvalid;
	}
}

bool EQ::InventoryProfile::CanItemFitInContainer(const ItemData* ItemToTry, const ItemData* Container) {

	if (!ItemToTry || !Container) return false;

	if (ItemToTry->Size > Container->BagSize) return false;

	if ((Container->BagType == item::BagTypeQuiver) && (ItemToTry->ItemType != item::ItemTypeArrow)) return false;

	return true;
}

bool EQ::InventoryProfile::SupportsClickCasting(int16 slot_id)
{
	// there are a few non-potion items that identify as ItemTypePotion..so, we still need to ubiquitously include the equipment range
	if (slot_id >= invslot::EQUIPMENT_BEGIN && slot_id <= invslot::EQUIPMENT_END) {
		return true;
	}
	else if (slot_id >= invslot::GENERAL_BEGIN && slot_id <= invslot::GENERAL_END) {
		return true;
	}
	else if (slot_id >= invbag::GENERAL_BAGS_BEGIN && slot_id <= invbag::GENERAL_BAGS_END) {
		if (inventory::Lookup(m_mob_version)->AllowClickCastFromBag)
			return true;
	}

	return false;
}

// Test whether a given slot can support a container item
bool EQ::InventoryProfile::SupportsContainers(int16 slot_id)
{
	if ((slot_id == invslot::slotCursor) ||
		(slot_id >= invslot::GENERAL_BEGIN && slot_id <= invslot::GENERAL_END) ||
		(slot_id >= invslot::BANK_BEGIN && slot_id <= invslot::BANK_END) ||
		(slot_id >= invslot::TRADE_BEGIN && slot_id <= invslot::TRADE_END))
		return true;
	return false;
}

int EQ::InventoryProfile::GetSlotByItemInst(ItemInstance* inst) {
	if (!inst)
		return INVALID_INDEX;

	int i = GetSlotByItemInstCollection(m_worn, inst);
	if (i != INVALID_INDEX) {
		return i;
	}

	i = GetSlotByItemInstCollection(m_inv, inst);
	if (i != INVALID_INDEX) {
		return i;
	}

	i = GetSlotByItemInstCollection(m_bank, inst);
	if (i != INVALID_INDEX) {
		return i;
	}

	i = GetSlotByItemInstCollection(m_trade, inst);
	if (i != INVALID_INDEX) {
		return i;
	}

	if (m_cursor.peek_front() == inst) {
		return invslot::slotCursor;
	}

	return INVALID_INDEX;
}

uint8 EQ::InventoryProfile::FindBrightestLightType()
{
	uint8 brightest_light_type = 0;

	for (auto iter = m_worn.begin(); iter != m_worn.end(); ++iter) {
		if ((iter->first < invslot::EQUIPMENT_BEGIN || iter->first > invslot::EQUIPMENT_END)) { continue; }
		if (iter->first == invslot::slotAmmo) { continue; }

		auto inst = iter->second;
		if (inst == nullptr) { continue; }
		auto item = inst->GetItem();
		if (item == nullptr) { continue; }

		if (lightsource::IsLevelGreater(item->Light, brightest_light_type))
			brightest_light_type = item->Light;
	}

	uint8 general_light_type = 0;
	for (auto iter = m_inv.begin(); iter != m_inv.end(); ++iter) {
		if (iter->first < invslot::GENERAL_BEGIN || iter->first > invslot::GENERAL_END) { continue; }

		auto inst = iter->second;
		if (inst == nullptr) { continue; }
		auto item = inst->GetItem();
		if (item == nullptr) { continue; }

		if (item->ItemClass != item::ItemClassCommon) { continue; }

		// Custom Quarm - Allow any item with a light on it to shine light from general inventory slots
		if (item->Light == 0) { continue; }

		if (lightsource::TypeToLevel(item->Light))
			general_light_type = item->Light;
	}

	if (lightsource::IsLevelGreater(general_light_type, brightest_light_type))
		brightest_light_type = general_light_type;

	return brightest_light_type;
}

void EQ::InventoryProfile::dumpEntireInventory() {

	dumpWornItems();
	dumpInventory();
	dumpBankItems();

	std::cout << std::endl;
}

void EQ::InventoryProfile::dumpWornItems() {
	std::cout << "Worn items:" << std::endl;
	dumpItemCollection(m_worn);
}

void EQ::InventoryProfile::dumpInventory() {
	std::cout << "Inventory items:" << std::endl;
	dumpItemCollection(m_inv);
}

void EQ::InventoryProfile::dumpBankItems() {

	std::cout << "Bank items:" << std::endl;
	dumpItemCollection(m_bank);
}

int EQ::InventoryProfile::GetSlotByItemInstCollection(const std::map<int16, ItemInstance*>& collection, ItemInstance* inst) {
	for (auto iter = collection.begin(); iter != collection.end(); ++iter) {
		ItemInstance* t_inst = iter->second;
		if (t_inst == inst) {
			return iter->first;
		}

		if (t_inst && !t_inst->IsClassBag()) {
			for (auto b_iter = t_inst->_cbegin(); b_iter != t_inst->_cend(); ++b_iter) {
				if (b_iter->second == inst) {
					return InventoryProfile::CalcSlotId(iter->first, b_iter->first);
				}
			}
		}
	}

	return -1;
}

void EQ::InventoryProfile::dumpItemCollection(const std::map<int16, ItemInstance*>& collection)
{
	for (auto it = collection.cbegin(); it != collection.cend(); ++it) {
		auto inst = it->second;
		if (!inst || !inst->GetItem())
			continue;

		std::string slot = StringFormat("Slot %d: %s (%d)", it->first, it->second->GetItem()->Name, (inst->GetCharges() <= 0) ? 1 : inst->GetCharges());
		std::cout << slot << std::endl;

		dumpBagContents(inst, &it);
	}
}

void EQ::InventoryProfile::dumpBagContents(ItemInstance* inst, std::map<int16, ItemInstance*>::const_iterator* it)
{

	if (!inst || !inst->IsClassBag())
		return;

	// Go through bag, if bag
	for (auto itb = inst->_cbegin(); itb != inst->_cend(); ++itb) {
		ItemInstance* baginst = itb->second;
		if (!baginst || !baginst->GetItem())
			continue;

		std::string subSlot = StringFormat("	Slot %d: %s (%d)", InventoryProfile::CalcSlotId((*it)->first, itb->first),
			baginst->GetItem()->Name, (baginst->GetCharges() <= 0) ? 1 : baginst->GetCharges());
		std::cout << subSlot << std::endl;
	}

}

// Internal Method: Retrieves item within an inventory bucket
EQ::ItemInstance* EQ::InventoryProfile::_GetItem(const std::map<int16, ItemInstance*>& bucket, int16 slot_id) const
{
	auto it = bucket.find(slot_id);
	if (it != bucket.end()) {
		return it->second;
	}

	// Not found!
	return nullptr;
}

// Internal Method: "put" item into bucket, without regard for what is currently in bucket
// Assumes item has already been allocated
int16 EQ::InventoryProfile::_PutItem(int16 slot_id, ItemInstance* inst)
{
	// If putting a nullptr into slot, we need to remove slot without memory delete
	if (inst == nullptr) {
		//Why do we not delete the poped item here????
		PopItem(slot_id);
		return slot_id;
	}

	int16 result = INVALID_INDEX;

	if (slot_id == invslot::slotCursor) {
		// Replace current item on cursor, if exists
		m_cursor.pop(); // no memory delete, clients of this function know what they are doing
		m_cursor.push_front(inst);
		result = slot_id;
	}
	else if ((slot_id >= invslot::EQUIPMENT_BEGIN && slot_id <= invslot::EQUIPMENT_END)) {
		m_worn[slot_id] = inst;
		result = slot_id;
	}
	else if ((slot_id >= invslot::GENERAL_BEGIN && slot_id <= invslot::GENERAL_END)) {
		m_inv[slot_id] = inst;
		result = slot_id;
	}
	else if (slot_id >= invslot::BANK_BEGIN && slot_id <= invslot::BANK_END) {
		m_bank[slot_id] = inst;
		result = slot_id;
	}
	else if (slot_id >= invslot::TRADE_BEGIN && slot_id <= invslot::TRADE_END) {
		m_trade[slot_id] = inst;
		result = slot_id;
	}
	else {
		// Slot must be within a bag
		ItemInstance* baginst = GetItem(InventoryProfile::CalcSlotId(slot_id)); // Get parent bag
		if (baginst && baginst->IsClassBag()) {
			baginst->_PutItem(InventoryProfile::CalcBagIdx(slot_id), inst);
			result = slot_id;
		}
	}

	if (result == INVALID_INDEX) {
		LogError("Inventory::_PutItem: Invalid slot_id specified ({})", slot_id);
		InventoryProfile::MarkDirty(inst); // Slot not found, clean up
	}

	return result;
}

// Internal Method: Checks an inventory bucket for a particular item
int16 EQ::InventoryProfile::_HasItem(std::map<int16, ItemInstance*>& bucket, int16 item_id, uint8 quantity)
{
	uint8 quantity_found = 0;

	for (auto iter = bucket.begin(); iter != bucket.end(); ++iter) {
		auto inst = iter->second;
		if (inst == nullptr) { continue; }

		if (inst->GetID() == item_id) {
			quantity_found += (inst->GetCharges() <= 0) ? 1 : inst->GetCharges();
			if (quantity_found >= quantity)
				return iter->first;
		}

		if (!inst->IsClassBag()) { continue; }

		for (auto bag_iter = inst->_cbegin(); bag_iter != inst->_cend(); ++bag_iter) {
			auto bag_inst = bag_iter->second;
			if (bag_inst == nullptr) { continue; }

			if (bag_inst->GetID() == item_id) {
				quantity_found += (bag_inst->GetCharges() <= 0) ? 1 : bag_inst->GetCharges();
				if (quantity_found >= quantity)
					return InventoryProfile::CalcSlotId(iter->first, bag_iter->first);
			}
		}
	}

	return INVALID_INDEX;
}

// Internal Method: Checks an inventory queue type bucket for a particular item
int16 EQ::InventoryProfile::_HasItem(ItemInstQueue& iqueue, int16 item_id, uint8 quantity)
{
	// The downfall of this (these) queue procedure is that callers presume that when an item is
	// found, it is presented as being available on the cursor. In cases of a parity check, this
	// is sufficient. However, in cases where referential criteria is considered, this can lead
	// to unintended results. Functionality should be observed when referencing the return value
	// of this query

	uint8 quantity_found = 0;

	for (auto iter = iqueue.cbegin(); iter != iqueue.cend(); ++iter) {
		auto inst = *iter;
		if (inst == nullptr) { continue; }

		if (inst->GetID() == item_id) {
			quantity_found += (inst->GetCharges() <= 0) ? 1 : inst->GetCharges();
			if (quantity_found >= quantity)
				return invslot::slotCursor;
		}

		if (!inst->IsClassBag()) { continue; }

		for (auto bag_iter = inst->_cbegin(); bag_iter != inst->_cend(); ++bag_iter) {
			auto bag_inst = bag_iter->second;
			if (bag_inst == nullptr) { continue; }

			if (bag_inst->GetID() == item_id) {
				quantity_found += (bag_inst->GetCharges() <= 0) ? 1 : bag_inst->GetCharges();
				if (quantity_found >= quantity)
					return InventoryProfile::CalcSlotId(invslot::slotCursor, bag_iter->first);
			}
		}

		// We only check the visible cursor due to lack of queue processing ability (client allows duplicate in limbo)
		break;
	}

	return INVALID_INDEX;
}

// Internal Method: Checks an inventory bucket for a particular item
int16 EQ::InventoryProfile::_HasItemByUse(std::map<int16, ItemInstance*>& bucket, uint8 use, uint8 quantity)
{
	uint8 quantity_found = 0;

	for (auto iter = bucket.begin(); iter != bucket.end(); ++iter) {
		auto inst = iter->second;
		if (inst == nullptr) { continue; }

		if (inst->IsClassCommon() && inst->GetItem()->ItemType == use) {
			quantity_found += (inst->GetCharges() <= 0) ? 1 : inst->GetCharges();
			if (quantity_found >= quantity)
				return iter->first;
		}

		if (!inst->IsClassBag()) { continue; }

		for (auto bag_iter = inst->_cbegin(); bag_iter != inst->_cend(); ++bag_iter) {
			auto bag_inst = bag_iter->second;
			if (bag_inst == nullptr) { continue; }

			if (bag_inst->IsClassCommon() && bag_inst->GetItem()->ItemType == use) {
				quantity_found += (bag_inst->GetCharges() <= 0) ? 1 : bag_inst->GetCharges();
				if (quantity_found >= quantity)
					return InventoryProfile::CalcSlotId(iter->first, bag_iter->first);
			}
		}
	}

	return INVALID_INDEX;
}

// Internal Method: Checks an inventory queue type bucket for a particular item
int16 EQ::InventoryProfile::_HasItemByUse(ItemInstQueue& iqueue, uint8 use, uint8 quantity)
{
	uint8 quantity_found = 0;

	for (auto iter = iqueue.cbegin(); iter != iqueue.cend(); ++iter) {
		auto inst = *iter;
		if (inst == nullptr) { continue; }

		if (inst->IsClassCommon() && inst->GetItem()->ItemType == use) {
			quantity_found += (inst->GetCharges() <= 0) ? 1 : inst->GetCharges();
			if (quantity_found >= quantity)
				return invslot::slotCursor;
		}

		if (!inst->IsClassBag()) { continue; }

		for (auto bag_iter = inst->_cbegin(); bag_iter != inst->_cend(); ++bag_iter) {
			auto bag_inst = bag_iter->second;
			if (bag_inst == nullptr) { continue; }

			if (bag_inst->IsClassCommon() && bag_inst->GetItem()->ItemType == use) {
				quantity_found += (bag_inst->GetCharges() <= 0) ? 1 : bag_inst->GetCharges();
				if (quantity_found >= quantity)
					return InventoryProfile::CalcSlotId(invslot::slotCursor, bag_iter->first);
			}
		}

		// We only check the visible cursor due to lack of queue processing ability (client allows duplicate in limbo)
		break;
	}

	return INVALID_INDEX;
}
