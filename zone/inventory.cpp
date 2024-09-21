/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"

#include "../common/strings.h"
#include "quest_parser_collection.h"
#include "worldserver.h"
#include "zonedb.h"
#include "queryserv.h"
#include "string_ids.h"

extern WorldServer worldserver;
extern QueryServ* QServ;

// @merth: this needs to be touched up
uint32 Client::NukeItem(uint32 itemnum, uint8 where_to_check) {
	if (itemnum == 0)
		return 0;
	uint32 x = 0;
	EQ::ItemInstance *cur = nullptr;

	int i;
	if(where_to_check & invWhereWorn) {
		for (i = EQ::invslot::EQUIPMENT_BEGIN; i <= EQ::invslot::EQUIPMENT_END; i++) {
			if (GetItemIDAt(i) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(i) != INVALID_ID)) {
				cur = m_inv.GetItem(i);
				if(cur && cur->GetItem()->Stackable) {
					x += cur->GetCharges();
				} else {
					x++;
				}

				DeleteItemInInventory(i, 0, true);
			}
		}
	}

	if(where_to_check & invWhereCursor) {
		if (GetItemIDAt(EQ::invslot::slotCursor) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(EQ::invslot::slotCursor) != INVALID_ID)) {
			cur = m_inv.GetItem(EQ::invslot::slotCursor);
			if(cur && cur->GetItem()->Stackable) {
				x += cur->GetCharges();
			} else {
				x++;
			}

			DeleteItemInInventory(EQ::invslot::slotCursor, 0, true);
		}

		for (i = EQ::invbag::CURSOR_BAG_BEGIN; i <= EQ::invbag::CURSOR_BAG_END; i++) {
			if (GetItemIDAt(i) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(i) != INVALID_ID)) {
				cur = m_inv.GetItem(i);
				if(cur && cur->GetItem()->Stackable) {
					x += cur->GetCharges();
				} else {
					x++;
				}

				DeleteItemInInventory(i, 0, true);
			}
		}
	}

	if(where_to_check & invWherePersonal) {
		for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++) {
			if (GetItemIDAt(i) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(i) != INVALID_ID)) {
				cur = m_inv.GetItem(i);
				if(cur && cur->GetItem()->Stackable) {
					x += cur->GetCharges();
				} else {
					x++;
				}

				DeleteItemInInventory(i, 0, true);
			}
		}

		for (i = EQ::invbag::GENERAL_BAGS_BEGIN; i <= EQ::invbag::GENERAL_BAGS_END; i++) {
			if (GetItemIDAt(i) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(i) != INVALID_ID)) {
				cur = m_inv.GetItem(i);
				if(cur && cur->GetItem()->Stackable) {
					x += cur->GetCharges();
				} else {
					x++;
				}

				DeleteItemInInventory(i, 0, true);
			}
		}
	}

	if(where_to_check & invWhereBank) {
		for (i = EQ::invslot::BANK_BEGIN; i <= EQ::invslot::BANK_END; i++) {
			if (GetItemIDAt(i) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(i) != INVALID_ID)) {
				cur = m_inv.GetItem(i);
				if(cur && cur->GetItem()->Stackable) {
					x += cur->GetCharges();
				} else {
					x++;
				}

				DeleteItemInInventory(i, 0, true);
			}
		}

		for (i = EQ::invbag::BANK_BAGS_BEGIN; i <= EQ::invbag::BANK_BAGS_END; i++) {
			if (GetItemIDAt(i) == itemnum || (itemnum == 0xFFFE && GetItemIDAt(i) != INVALID_ID)) {
				cur = m_inv.GetItem(i);
				if(cur && cur->GetItem()->Stackable) {
					x += cur->GetCharges();
				} else {
					x++;
				}

				DeleteItemInInventory(i, 0, true);
			}
		}
	}

	return x;
}


bool Client::CheckLoreConflict(const EQ::ItemData* item) {
	if (!item)
		return false;
	if (item->Lore[0] != '*' && item->Lore[0] != '#')
		return false;

	if (item->Lore[0] == '*')	// Standard lore items; look everywhere except unused, return the result
		return (m_inv.HasItem(item->ID, 0, ~invWhereUnused) != INVALID_INDEX);

	else if(item->Lore[0] == '#')
		return (m_inv.HasArtifactItem() != INVALID_INDEX);

	return false;

}

bool Client::SummonItem(uint32 item_id, int8 quantity, uint16 to_slot, bool force_charges) {
	this->EVENT_ITEM_ScriptStopReturn();

	// TODO: update calling methods and script apis to handle a failure return

	const EQ::ItemData* item = database.GetItem(item_id);

	// make sure the item exists
	if(item == nullptr) {
		Message(Chat::Red, "Item %u does not exist.", item_id);
		Log(Logs::Detail, Logs::Inventory, "Player %s on account %s attempted to create an item with an invalid id.\n(Item: %u)\n",
			GetName(), account_name, item_id);

		return false;
	} 
	// check that there is not a lore conflict between base item and existing inventory
	else if(CheckLoreConflict(item)) {
		// DuplicateLoreMessage(item_id);
		if(item->Lore[0] == '#')
			Message(Chat::Red, "You already have an artifact item in your inventory.");
		else
			Message(Chat::Red, "You already have a lore %s (%i) in your inventory.", item->Name, item_id);

		return false;
	}

	// This code is ready to implement once the item load code is changed to process the 'minstatus' field.
	// Checking #iteminfo in-game verfies that item->MinStatus is set to '0' regardless of field value.
	// An optional sql script will also need to be added, once this goes live, to allow changing of the min status.

	// check to make sure we are a GM if the item is GM-only
	else if(item->GMFlag == -1 && this->Admin() < RuleI(GM, MinStatusToUseGMItem)) {
		Message(Chat::Red, "You are not a GM or do not have the status to summon this item.");
		Log(Logs::Detail, Logs::Inventory, "Player %s on account %s attempted to create a GM-only item with a status of %i.\n(Item: %u, GMFlag: %u)\n",
			GetName(), account_name, this->Admin(), item->ID, item->GMFlag);

		return false;
	}

	uint32 classes	= item->Classes;
	uint32 races	= item->Races;
	uint32 slots	= item->Slots;

	// validation passed..so, set the quantity and create the actual item

	if(quantity < 0)
	{
		quantity = 1;
	}
	else if (quantity == 0)
	{
		if(database.ItemQuantityType(item_id) == EQ::item::Quantity_Normal)
		{ 
			quantity = 1;
		}
		else if(database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges)
		{
			if(!force_charges)
				quantity = item->MaxCharges;
		}
		else if(database.ItemQuantityType(item_id) == EQ::item::Quantity_Stacked)
		{
			//If no value is set coming from a quest method, only summon a single item.
			if(to_slot == EQ::legacy::SLOT_QUEST)
			{
				quantity = 1;
			}
			else
			{
				quantity = item->StackSize;
			}
		}
	}	
	// in any other situation just use quantity as passed

	EQ::ItemInstance* inst = database.CreateItem(item, quantity);

	if(inst == nullptr) {
		Message(Chat::Red, "An unknown server error has occurred and your item was not created.");
		// this goes to logfile since this is a major error
		Log(Logs::General, Logs::Error, "Player %s on account %s encountered an unknown item creation error.\n(Item: %u)\n",
			GetName(), account_name, item->ID);

		return false;
	}

	// check to see if item is usable in requested slot
	if(to_slot != EQ::legacy::SLOT_QUEST && ((to_slot >= EQ::invslot::slotEar1) && (to_slot <= EQ::invslot::slotAmmo))) {
		uint32 slottest = 22; // can't change '22' just yet...

		if(!(slots & ((uint32)1 << slottest))) {
			Message(Chat::White, "This item is not equipable at slot %u - moving to cursor.", to_slot);
			Log(Logs::Detail, Logs::Inventory, "Player %s on account %s attempted to equip an item unusable in slot %u - moved to cursor.\n(Item: %u)\n",
				GetName(), account_name, to_slot, item->ID);

			to_slot = EQ::invslot::slotCursor;
		}
	}

	//We're coming from a quest method.
	if(to_slot == EQ::legacy::SLOT_QUEST)
	{
		bool stacking = TryStacking(inst);
		//If we were able to stack, there is no need to continue on as we're set.
		if(stacking)
		{
			safe_delete(inst);
			return true;
		}
		else
		{
			bool bag = false;
			if(inst->IsType(EQ::item::ItemClassBag))
			{
				bag = true;
			}
			to_slot = m_inv.FindFreeSlot(bag, true, item->Size);

			//make sure we are not completely full...
			if(to_slot == EQ::invslot::slotCursor || to_slot == INVALID_INDEX)
			{
				if (inst->GetItem()->NoDrop == 0 || zone && zone->GetGuildID() != GUILD_NONE)
				{
					//If it's no drop, force it to the cursor. This carries the risk of deletion if the player already has this item on their cursor
					// or if the cursor queue is full. But in this situation, we have little other recourse.
					PushItemOnCursorWithoutQueue(inst);
					Log(Logs::General, Logs::Inventory, "%s has a full inventory and %s is a no drop item. Forcing to cursor.", GetName(), inst->GetItem()->Name);
					safe_delete(inst);
					return true;
				}
				else if(m_inv.GetItem(EQ::invslot::slotCursor) != nullptr || to_slot == INVALID_INDEX)
				{
					CreateGroundObject(inst, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
					safe_delete(inst);
					return true;
				}
			}
		}
	}

	if (to_slot == EQ::invslot::slotCursor) {
		PushItemOnCursorWithoutQueue(inst);
	}
	else
		PutItemInInventory(to_slot, *inst, true);

	safe_delete(inst);

	// discover item
	if((RuleB(Character, EnableDiscoveredItems)) && !GetGM()) {
		if(!IsDiscovered(item_id))
			DiscoverItem(item_id);
	}

	return true;
}

// Drop item from inventory to ground (generally only dropped from SlotCursor)
void Client::DropItem(int16 slot_id)
{
	if(GetInv().CheckNoDrop(slot_id) && RuleI(World, FVNoDropFlag) == 0 ||
		RuleI(Character, MinStatusForNoDropExemptions) < Admin() && RuleI(World, FVNoDropFlag) == 2) {
		database.SetHackerFlag(this->AccountName(), this->GetCleanName(), "Tried to drop an item on the ground that was nodrop!");
		GetInv().DeleteItem(slot_id);
		return;
	}

	// Take control of item in client inventory
	EQ::ItemInstance *inst = m_inv.PopItem(slot_id);
	if(inst) {
		int i = parse->EventItem(EVENT_DROP_ITEM, this, inst, nullptr, "", 0);
		if(i != 0) {
			safe_delete(inst);
		}
	} else {
		// Item doesn't exist in inventory!
		Log(Logs::General, Logs::Error, "Item not found in slot %i", slot_id);
		return;
	}

	// Save client inventory change to database
	if (slot_id == EQ::invslot::slotCursor) {
		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		database.SaveCursor(this, s, e);
	} else {
		database.SaveInventory(CharacterID(), nullptr, slot_id);
	}

	if(!inst)
		return;

	CreateGroundObject(inst, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, DecayTime));

	safe_delete(inst);
}

//This differs from EntityList::CreateGroundObject by using the inst, so bag contents are
//preserved. EntityList creates a new instance using ID, so bag contents are lost. Also,
//EntityList can be used by NPCs for things like disarm.
void Client::CreateGroundObject(const EQ::ItemInstance* inst_in, glm::vec4 coords, uint32 decay_time, bool message)
{

	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		auto broken_string = fmt::format("You cannot drop items in the ground. item {} (qty {} ).This item is eligible for reimbursement via petition at a cost of 1500 platinum per item.", inst_in->GetID(), inst_in->GetCharges());
		Message(Chat::Red, broken_string.c_str());
		if (RuleB(QueryServ, PlayerLogItemDesyncs))
		{
			QServ->QSItemDesyncs(CharacterID(), broken_string.c_str(), GetZoneID());
		}
		return;
	}

	if (!inst_in) {
		// Item doesn't exist in inventory!
		Message(Chat::Red, "Error: Item not found");
		return;
	}

	// make a copy so we can remove no drop items if we need to
	EQ::ItemInstance *inst = new EQ::ItemInstance(*inst_in);
	if (inst->GetItem()->NoDrop == 0)
	{
		Message(Chat::Red, "This item is NODROP. Deleting.");
		auto msg = fmt::format("Dropped item is NODROP. Deleting. ({} {}) This item is eligible for reimbursement via petition at a cost of 1500 platinum per item.", inst->GetCharges(), inst->GetItem()->Name);
		QServ->QSItemDesyncs(CharacterID(), msg.c_str(), GetZoneID());
		Message(Chat::Red, msg.c_str());
		if (inst->IsType(EQ::item::ItemClassBag))
		{
			for (uint8 sub_slot = EQ::invbag::SLOT_BEGIN; (sub_slot <= EQ::invbag::SLOT_END); ++sub_slot)
			{
				const EQ::ItemInstance *bag_inst = inst->GetItem(sub_slot);
				if (bag_inst)
				{
					msg = fmt::format("Dropped bag was NODROP. Deleting contents. ({} {}) This item is eligible for reimbursement via petition at a cost of 1500 platinum per item.", bag_inst->GetCharges(), bag_inst->GetItem()->Name);
					QServ->QSItemDesyncs(CharacterID(), msg.c_str(), GetZoneID());
					Message(Chat::Red, msg.c_str());
				}
			}
		}
		safe_delete(inst);
		return;
	}
	if (inst->IsType(EQ::item::ItemClassBag))
	{
		for (uint8 sub_slot = EQ::invbag::SLOT_BEGIN; (sub_slot <= EQ::invbag::SLOT_END); ++sub_slot)
		{
			const EQ::ItemInstance *bag_inst = inst->GetItem(sub_slot);
			if (bag_inst && bag_inst->GetItem()->NoDrop == 0)
			{
				auto msg = fmt::format("Dropped bag contains item that is NODROP. Deleting. ({} {}) This item is eligible for reimbursement via petition at a cost of 1500 platinum per item.", bag_inst->GetCharges(), bag_inst->GetItem()->Name);
				Message(Chat::Red, msg.c_str());
				inst->DeleteItem(sub_slot);
			}
		}
	}


	if (RuleB(QueryServ, PlayerLogGroundSpawn) && inst)
	{
		QServ->QSGroundSpawn(CharacterID(), inst->GetID(), inst->GetCharges(), 0, GetZoneID(), true, message);
		if (inst->IsType(EQ::item::ItemClassBag))
		{
			for (uint8 sub_slot = EQ::invbag::SLOT_BEGIN; (sub_slot <= EQ::invbag::SLOT_END); ++sub_slot)
			{
				const EQ::ItemInstance* bag_inst = inst->GetItem(sub_slot);
				if (bag_inst)
				{
					QServ->QSGroundSpawn(CharacterID(), bag_inst->GetID(), bag_inst->GetCharges(), inst->GetID(), GetZoneID(), true, message);
				}
			}
		}
	}

	if (message)
	{
		Message_StringID(Chat::Yellow, DROPPED_ITEM);
	}

	// Package as zone object
	Object *object = new Object(inst, coords.x, coords.y, coords.z, coords.w ,decay_time, true, this);
	entity_list.AddObject(object, true);
	object->Save();

	safe_delete(inst);
}

// Returns a slot's item ID (returns INVALID_ID if not found)
int32 Client::GetItemIDAt(int16 slot_id) {
	const EQ::ItemInstance* inst = m_inv[slot_id];
	if (inst)
		return inst->GetItem()->ID;

	// None found
	return INVALID_ID;
}

bool Client::FindOnCursor(uint32 item_id) {
	if (m_inv.CursorSize() > 1) {
		for (auto iter = m_inv.cursor_cbegin(); iter != m_inv.cursor_cend(); ++iter) {
			// m_inv.cursor_begin() is referenced as SlotCursor
			if (iter == m_inv.cursor_cbegin())
				continue;

			EQ::ItemInstance* inst = *iter;
			if (inst != nullptr && inst->GetItem()->ID == item_id)
				return true;
		}
	}
	return false;
}

void Client::ClearMoney()
{


	SendClientMoneyUpdate(0, -GetCopper());
	SendClientMoneyUpdate(1, -GetSilver());
	SendClientMoneyUpdate(2, -GetGold());
	SendClientMoneyUpdate(3, -GetPlatinum());



	m_pp.copper = 0;
	m_pp.copper_bank = 0;
	m_pp.copper_cursor = 0;
	m_pp.silver = 0;
	m_pp.silver_bank = 0;
	m_pp.silver_cursor = 0;
	m_pp.gold = 0;
	m_pp.gold_bank = 0;
	m_pp.gold_cursor = 0;
	m_pp.platinum = 0;
	m_pp.platinum_bank = 0;
	m_pp.platinum_cursor = 0;
	SaveCurrency();
}

void Client::ResetStartingSkills()
{
	m_pp.level2 = 1;
	m_pp.points = 5;

	//Set all skills to 0.
	for (int s = 0; s <= EQ::skills::HIGHEST_SKILL; s++)
	{
		m_pp.skills[s] = 0;
	}
	//Set all languages to 0.
	for (int l = 0; l <= MAX_PP_LANGUAGE; l++)
	{
		m_pp.languages[l] = 0;
	}
	
	/* Set Racial and Class specific language and skills */
	RemoveAllSkills();
	SetRacialLanguages();
	SetRaceStartingSkills();
	SetClassLanguages();
}

void Client::ClearPlayerInfoAndGrantStartingItems(bool goto_death)
{

	//Clear player's money.
	ClearMoney();

	//Revert player's bind location to default starting
	SetBindPoint(m_pp.binds[4].zoneId, glm::vec3(m_pp.binds[4].x, m_pp.binds[4].y, m_pp.binds[4].z));

	//Remove spells.
	UnscribeSpellAll(false);
	UnmemSpellAll(false);

	//Fade all buffs.
	BuffFadeAll(false, true);

	//Clear AAs.
	RefundAA();
	SetAAPoints(0);
	m_pp.aapoints_spent = 0;
	
	//Remove all factions.
	database.RemoveAllFactions(this);
	factionvalues.clear();

	//Remove starting skills.
	ResetStartingSkills();

	//Remove all items from their trade if they're doing one.
	Mob *Other = trade->With();
	if (Other)
	{
		FinishTrade(this);

		if (Other->IsClient())
			Other->CastToClient()->FinishTrade(Other);

		/* Reset both sides of the trade */
		trade->Reset();
		Other->trade->Reset();
	}

	//Delete all items from their inventory.
	for (int16 i = EQ::invslot::SLOT_BEGIN; i <= EQ::invbag::BANK_BAGS_END;)
	{
		const EQ::ItemInstance* newinv = m_inv.GetItem(i);

		if (i == EQ::invslot::GENERAL_END + 1) {
			i = EQ::invbag::GENERAL_BAGS_BEGIN;
			continue;
		}
		else if (i == EQ::invbag::CURSOR_BAG_END + 1) {
			i = EQ::invslot::BANK_BEGIN;
			continue;
		}
		else if (i == EQ::invslot::BANK_END + 1) {
			i = EQ::invbag::BANK_BAGS_BEGIN;
			continue;
		}

		if (newinv)
		{
			DeleteItemInInventory(i, 0, true, true);
		}

		i++;
	}

	//Grant starting items to the player again, since we just removed their inventory.
	int return_zone_id = 0;

	database.ResetStartingItems(this, m_pp.race, m_pp.class_, m_pp.deity, m_pp.binds[4].zoneId, m_pp.name, Admin(), return_zone_id );

	//Set Level / EXP to 0.
	SetLevel(1, true);
	SetEXP(0, 0);

	SetHardcoreDeathTimeStamp(0);
	//Their state is likely all sorts of messed up. Commit immediately (Save) and then...
	Save(1);
	if (goto_death)
	{
		ForceGoToDeath();
	}
}

// Remove item from inventory
void Client::DeleteItemInInventory(int16 slot_id, int8 quantity, bool client_update, bool update_db) 
{
	Log(Logs::Detail, Logs::Inventory, "DeleteItemInInventory(%i, %i, %s)", slot_id, quantity, (client_update) ? "true":"false");

	// Added 'IsSlotValid(slot_id)' check to both segments of client packet processing.
	// - cursor queue slots were slipping through and crashing client
	if(!m_inv[slot_id]) {
		// Make sure the client deletes anything in this slot to match the server.
		if(client_update && IsValidSlot(slot_id)) {
			auto outapp = new EQApplicationPacket(OP_MoveItem, sizeof(MoveItem_Struct));
			MoveItem_Struct* delitem	= (MoveItem_Struct*)outapp->pBuffer;
			delitem->from_slot			= slot_id;
			delitem->to_slot			= 0xFFFFFFFF;
			delitem->number_in_stack	= 0xFFFFFFFF;
			QueuePacket(outapp);
			safe_delete(outapp);
			
		}
		return;
	}

	// start QS code
	if(RuleB(QueryServ, PlayerLogDeletes)) {
		uint16 delete_count = 0;

		if(m_inv[slot_id]) { delete_count += m_inv.GetItem(slot_id)->GetTotalItemCount(); }

		auto pack = new ServerPacket(ServerOP_QSPlayerLogItemDeletes, sizeof(QSPlayerLogItemDelete_Struct) * delete_count);
		QSPlayerLogItemDelete_Struct* QS = (QSPlayerLogItemDelete_Struct*)pack->pBuffer;

		QS->char_id = character_id;
		QS->stack_size = quantity;
		QS->char_count = delete_count;

		QS->char_slot = slot_id;
		QS->item_id = m_inv[slot_id]->GetID();
		QS->charges = m_inv[slot_id]->GetCharges();

		if(m_inv[slot_id]->IsType(EQ::item::ItemClassBag)) {
			for(uint8 bag_idx = EQ::invbag::SLOT_BEGIN; bag_idx < m_inv[slot_id]->GetItem()->BagSlots; bag_idx++) {
				EQ::ItemInstance* bagitem = m_inv[slot_id]->GetItem(bag_idx);

				if(bagitem) {
					int16 bagslot_id = EQ::InventoryProfile::CalcSlotId(slot_id, bag_idx);
					QS++;
					QS->char_id = character_id;
					QS->stack_size = quantity;
					QS->char_count = delete_count;

					QS->char_slot = bagslot_id;
					QS->item_id = bagitem->GetID();
					QS->charges = bagitem->GetCharges();
				}
			}
		}

		pack->Deflate();
		if(worldserver.Connected()) { worldserver.SendPacket(pack); }
		safe_delete(pack);
	}
	// end QS code

	bool isDeleted = m_inv.DeleteItem(slot_id, quantity);

	const EQ::ItemInstance* inst=nullptr;
	if (slot_id == EQ::invslot::slotCursor) {
		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		if (update_db)
			database.SaveCursor(this, s, e);
	}
	else {
		// Save change to database
		inst = m_inv[slot_id];
		if(update_db)
			database.SaveInventory(character_id, inst, slot_id);
	}

	bool returnitem = false;
	if(inst && !isDeleted)
	{
		if(inst->GetCharges() <= 0)
		{
			if (inst->IsStackable() 
			|| (!inst->IsStackable() && ((inst->GetItem()->MaxCharges == 0) || inst->IsExpendable()))) 
			{
				returnitem = false;
			}
			else 
				returnitem = true;
		}
	}

	if(client_update && IsValidSlot(slot_id)) 
	{
		EQApplicationPacket* outapp;
		if(inst) {
			if(!isDeleted){
				// Non stackable item with charges = Item with clicky spell effect ? Delete a charge.
				outapp = new EQApplicationPacket(OP_DeleteCharge, sizeof(MoveItem_Struct));
				MoveItem_Struct* delitem = (MoveItem_Struct*)outapp->pBuffer;
				delitem->from_slot			= slot_id;
				delitem->to_slot			= 0xFFFFFFFF;
				delitem->number_in_stack	= 0xFFFFFFFF;
				QueuePacket(outapp);
				safe_delete(outapp);
				if(returnitem)
				{
					SendItemPacket(slot_id, inst, ItemPacketTrade);
				}
				return;
			}
		}

		outapp = new EQApplicationPacket(OP_MoveItem, sizeof(MoveItem_Struct));
		MoveItem_Struct* delitem	= (MoveItem_Struct*)outapp->pBuffer;
		delitem->from_slot			= slot_id;
		delitem->to_slot			= 0xFFFFFFFF;
		delitem->number_in_stack	= 0xFFFFFFFF;
		QueuePacket(outapp);
		safe_delete(outapp);

	}
}

// Puts an item into the person's inventory
// Any items already there will be removed from user's inventory
// (Also saves changes back to the database: this may be optimized in the future)
// client_update: Sends packet to client
bool Client::PushItemOnCursor(const EQ::ItemInstance& inst, bool client_update)
{
	if(inst.GetItem()->GMFlag == -1 && this->Admin() < RuleI(GM, MinStatusToUseGMItem)) {
		Message(Chat::Red, "You are not a GM or do not have the status to this item. Please relog to avoid a desync.");
		Log(Logs::Detail, Logs::Inventory, "Player %s on account %s attempted to create a GM-only item with a status of %i.\n(Item: %u, GMFlag: %u)\n",
			GetName(), account_name, this->Admin(), inst.GetID(), inst.GetItem()->GMFlag);

		return false;
	}

	Log(Logs::Detail, Logs::Inventory, "Putting item %s (%d) on the cursor", inst.GetItem()->Name, inst.GetItem()->ID);
	m_inv.PushCursor(inst);

	if (client_update) {
		SendItemPacket(EQ::invslot::slotCursor, &inst, ItemPacketSummonItem);
	}

	auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
	return database.SaveCursor(this, s, e);
}

bool Client::PushItemOnCursorWithoutQueue(EQ::ItemInstance* inst, bool drop)
{
	const EQ::ItemData* item = database.GetItem(inst->GetID());
	if (item && CheckLoreConflict(item))
	{
		Message_StringID(Chat::White, DUP_LORE);
		return false;
	}

	if (zone && zone->GetGuildID() != GUILD_NONE)
		drop = false;

	EQ::ItemInstance* cursoritem = m_inv.GetItem(EQ::invslot::slotCursor);
	if (cursoritem == nullptr)
	{
		// If the cursor is empty, we have to send an item packet or we'll desync.
		if (!PushItemOnCursor(*inst))
		{
			if (inst)
			{
				auto broken_string = fmt::format("Cursor queue full or item {} (qty {} ) is a duplicate. This item is eligible for reimbursement.", inst->GetID(), inst->GetCharges());
				Log(Logs::General, Logs::Inventory, "Cursor queue has too many items or item %d (qty %d ) is a duplicate. It will be deleted.", inst->GetID(), inst->GetCharges());
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), broken_string.c_str(), GetZoneID()); }
				Message(Chat::Red, broken_string.c_str());
				Log(Logs::General, Logs::Inventory, "Saving item %d to the cursor queue failed.", inst->GetID());
			}
			return false;
		}

		SendItemPacket(EQ::invslot::slotCursor, inst, ItemPacketSummonItem);
	}
	else
	{
		if (Admin() == 0)
		{
			// There are items on the cursor, so we don't want to send packet updates. But,
			// we do want to enforce the client's rules about items on the cursor. No dupe items,
			// and a max count of 10.
			int16 inv_slot_id = GetInv().HasItem(inst->GetID(), 1, invWhereCursor);
			if (inv_slot_id != INVALID_INDEX || m_inv.CursorSize() >= 10)
			{
				if (drop)
				{
					CreateGroundObject(inst, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
					return true;
				}
				else
				{
					if (RuleB(Quarm, CursorAllowDuplicateItems)) {
						if (m_inv.CursorSize() >= 10) {
							auto broken_string = fmt::format("Cursor queue full. This item is eligible for reimbursement.");
							Log(Logs::General, Logs::Inventory, "Cursor queue has too many items. It will be deleted.");
							if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), broken_string.c_str(), GetZoneID()); }
							Message(Chat::Red, broken_string.c_str());
							return false;
						}
					}
					else
					{
						auto broken_string = fmt::format("Cursor queue full or item {} (qty {} ) is a duplicate. This item is eligible for reimbursement.", inst->GetID(), inst->GetCharges());
						Log(Logs::General, Logs::Inventory, "Cursor queue has too many items or item %d (qty %d ) is a duplicate. It will be deleted.", inst->GetID(), inst->GetCharges());
						if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), broken_string.c_str(), GetZoneID()); }
						Message(Chat::Red, broken_string.c_str());
						return false;
					}
				}
			}
		}

		inst->SetCursorQueue(false);
		if (!PushItemOnCursor(*inst))
		{
			if (inst)
			{
				Log(Logs::General, Logs::Inventory, "Saving item %d to the cursor queue failed.", inst->GetID());
				auto broken_string = fmt::format("Cursor queue full or item {} (qty {} ) is a duplicate. This item is eligible for reimbursement.", inst->GetID(), inst->GetCharges());
				Log(Logs::General, Logs::Inventory, "Cursor queue has too many items or item %d (qty %d ) is a duplicate. It will be deleted.", inst->GetID(), inst->GetCharges());
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), broken_string.c_str(), GetZoneID()); }
				Message(Chat::Red, broken_string.c_str());
			}
			return false;
		}
	}

	return true;
}

bool Client::PutItemInInventory(int16 slot_id, const EQ::ItemInstance& inst, bool client_update) {
	Log(Logs::Detail, Logs::Inventory, "Putting item %s (%d) into slot %d", inst.GetItem()->Name, inst.GetItem()->ID, slot_id);

	if(inst.GetItem()->GMFlag == -1 && this->Admin() < RuleI(GM, MinStatusToUseGMItem)) {
		Message(Chat::Red, "You are not a GM or do not have the status to this item. Please relog to avoid a desync.");
		Log(Logs::Detail, Logs::Inventory, "Player %s on account %s attempted to create a GM-only item with a status of %i.\n(Item: %u, GMFlag: %u)\n",
			GetName(), account_name, this->Admin(), inst.GetID(), inst.GetItem()->GMFlag);

		return false;
	}

	if (slot_id == EQ::invslot::slotCursor)
		return PushItemOnCursor(inst, client_update);
	else
		m_inv.PutItem(slot_id, inst);

	if (client_update)
		SendItemPacket(slot_id, &inst, ((slot_id == EQ::invslot::slotCursor) ? ItemPacketSummonItem : ItemPacketTrade));

	if (slot_id == EQ::invslot::slotCursor) {
		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		return database.SaveCursor(this, s, e);
	}
	else {
		return database.SaveInventory(this->CharacterID(), &inst, slot_id);
	}

	CalcBonuses();
}

void Client::PutLootInInventory(int16 slot_id, const EQ::ItemInstance &inst, LootItem** bag_item_data)
{
	Log(Logs::Detail, Logs::Inventory, "Putting loot item %s (%d) into slot %d", inst.GetItem()->Name, inst.GetItem()->ID, slot_id);
	m_inv.PutItem(slot_id, inst);

	SendLootItemInPacket(&inst, slot_id);

	if (slot_id == EQ::invslot::slotCursor) {
		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		database.SaveCursor(this, s, e);
	} else
		database.SaveInventory(this->CharacterID(), &inst, slot_id);

	if(bag_item_data)	// bag contents
	{
		int16 interior_slot;
		// solar: our bag went into slot_id, now let's pack the contents in
		for(int i = EQ::invbag::SLOT_BEGIN; i <= EQ::invbag::SLOT_END; i++)
		{
			if(bag_item_data[i] == nullptr)
				continue;

			const EQ::ItemData* sub_item_item_data = database.GetItem(bag_item_data[i]->item_id);
			if (sub_item_item_data == nullptr)
				continue;

			const EQ::ItemInstance *bagitem = database.CreateItem(bag_item_data[i]->item_id, bag_item_data[i]->charges);
			interior_slot = EQ::InventoryProfile::CalcSlotId(slot_id, i);
			Log(Logs::Detail, Logs::Inventory, "Putting bag loot item %s (%d) into slot %d (bag slot %d)", inst.GetItem()->Name, inst.GetItem()->ID, interior_slot, i);
			PutLootInInventory(interior_slot, *bagitem);
			safe_delete(bagitem);
		}
	}

	CalcBonuses();
}

bool Client::TryStacking(EQ::ItemInstance* item, uint8 type, bool try_worn, bool try_cursor)
{

	if(!item || !item->IsStackable())
		return false;
	int16 i;
	uint32 item_id = item->GetItem()->ID;
	// Do all we can get to arrows to go to quiver first.
	if(item->GetItem()->ItemType == EQ::item::ItemTypeArrow)
	{
		for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
		{
			EQ::ItemInstance* bag = m_inv.GetItem(i);
			if(bag)
			{
				if(bag->GetItem()->BagType == EQ::item::BagTypeQuiver)
				{
					int8 slots = bag->GetItem()->BagSlots;
					uint16 emptyslot = 0;
					for (uint8 j = EQ::invbag::SLOT_BEGIN; j < slots; j++)
					{
						uint16 slotid = EQ::InventoryProfile::CalcSlotId(i, j);
						EQ::ItemInstance* tmp_inst = m_inv.GetItem(slotid);
						if(!tmp_inst)
						{
							emptyslot = slotid;
						}
						// Partial stack found use this first
						if(tmp_inst && tmp_inst->GetItem()->ID == item_id && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize){
							MoveItemCharges(*item, slotid, type);
							CalcBonuses();
							if(item->GetCharges())	// we didn't get them all
								return AutoPutLootInInventory(*item, try_worn, try_cursor, 0);
							return true;
						}
					}
					// Use empty slot if no partial stacks
					if(emptyslot != 0)
					{
						PutItemInInventory(emptyslot, *item, true);
						return true;
					}
				}
			}
		}
	}
	for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		EQ::ItemInstance* tmp_inst = m_inv.GetItem(i);
		if(tmp_inst && tmp_inst->GetItem()->ID == item_id && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize){
			MoveItemCharges(*item, i, type);
			CalcBonuses();
			if(item->GetCharges())	// we didn't get them all
				return AutoPutLootInInventory(*item, try_worn, try_cursor, 0);
			return true;
		}
	}
	for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		for (uint8 j = EQ::invbag::SLOT_BEGIN; j <= EQ::invbag::SLOT_END; j++)
		{
			uint16 slotid = EQ::InventoryProfile::CalcSlotId(i, j);
			EQ::ItemInstance* tmp_inst = m_inv.GetItem(slotid);

			if(tmp_inst && tmp_inst->GetItem()->ID == item_id && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize){
				MoveItemCharges(*item, slotid, type);
				CalcBonuses();
				if(item->GetCharges())	// we didn't get them all
					return AutoPutLootInInventory(*item, try_worn, try_cursor, 0);
				return true;
			}
		}
	}
	return false;
}

int16 Client::GetStackSlot(EQ::ItemInstance* item, bool try_worn, bool try_cursor)
{

	if(!item || !item->IsStackable())
		return -1;
	int16 i;
	uint32 item_id = item->GetItem()->ID;
	// Do all we can get to arrows to go to quiver first.
	if(item->GetItem()->ItemType == EQ::item::ItemTypeArrow)
	{
		for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
		{
			EQ::ItemInstance* bag = m_inv.GetItem(i);
			if(bag)
			{
				if(bag->GetItem()->BagType == EQ::item::BagTypeQuiver)
				{
					int8 slots = bag->GetItem()->BagSlots;
					uint16 emptyslot = 0;
					for (uint8 j = EQ::invbag::SLOT_BEGIN; j < slots; j++)
					{
						uint16 slotid = EQ::InventoryProfile::CalcSlotId(i, j);
						EQ::ItemInstance* tmp_inst = m_inv.GetItem(slotid);
						if(!tmp_inst)
						{
							emptyslot = slotid;
						}
						// Partial stack found use this first
						if(tmp_inst && tmp_inst->GetItem()->ID == item_id && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize){
							return slotid;
						}
					}
					// Use empty slot if no partial stacks
					if(emptyslot != 0)
					{
						return emptyslot;
					}
				}
			}
		}
	}
	for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		EQ::ItemInstance* tmp_inst = m_inv.GetItem(i);
		if(tmp_inst && tmp_inst->GetItem()->ID == item_id && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize){
			return i;
		}
	}
	for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		for (uint8 j = EQ::invbag::SLOT_BEGIN; j <= EQ::invbag::SLOT_END; j++)
		{
			uint16 slotid = EQ::InventoryProfile::CalcSlotId(i, j);
			EQ::ItemInstance* tmp_inst = m_inv.GetItem(slotid);

			if(tmp_inst && tmp_inst->GetItem()->ID == item_id && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize){
				return slotid;
			}
		}
	}
	return -1;
}

// Locate an available space in inventory to place an item
// and then put the item there
// The change will be saved to the database
bool Client::AutoPutLootInInventory(EQ::ItemInstance& inst, bool try_worn, bool try_cursor, LootItem** bag_item_data)
{
	// #1: Try to auto equip
	if (try_worn && inst.IsEquipable(GetBaseRace(), GetClass()) && inst.GetItem()->ReqLevel<=level)
	{
		// too messy as-is... <watch>
		for (int16 i = EQ::invslot::EQUIPMENT_BEGIN; i <= EQ::invslot::EQUIPMENT_END; i++) // originally (i < 22)
		{
			if (i == EQ::invslot::GENERAL_BEGIN) {
				break;
			}

			if (!m_inv[i])
			{
				if( i == EQ::invslot::slotPrimary && inst.IsWeapon() ) // If item is primary slot weapon
				{
					if( (inst.GetItem()->ItemType == EQ::item::ItemType2HSlash) || (inst.GetItem()->ItemType == EQ::item::ItemType2HBlunt) || (inst.GetItem()->ItemType == EQ::item::ItemType2HPiercing) ) // and uses 2hs \ 2hb \ 2hp
					{
						if( m_inv[EQ::invslot::slotSecondary] ) // and if secondary slot is not empty
						{
							continue; // Can't auto-equip
						}
					}
				}
				if( i == EQ::invslot::slotPrimary && m_inv[EQ::invslot::slotSecondary] )
				{
					uint8 instrument = m_inv[EQ::invslot::slotSecondary]->GetItem()->ItemType;
					if( 
						instrument == EQ::item::ItemTypeWindInstrument ||
						instrument == EQ::item::ItemTypeStringedInstrument ||
						instrument == EQ::item::ItemTypeBrassInstrument ||
						instrument == EQ::item::ItemTypePercussionInstrument
					) {
						Log(Logs::Detail, Logs::Inventory, "Cannot equip a primary item with %s already in the secondary.", m_inv[EQ::invslot::slotSecondary]->GetItem()->Name);
						continue; // Do not auto-equip Primary when instrument is in Secondary
					}
				}
				if( i== EQ::invslot::slotSecondary && m_inv[EQ::invslot::slotPrimary]) // check to see if primary slot is a two hander
				{
					uint8 instrument = inst.GetItem()->ItemType;
					if( 
						instrument == EQ::item::ItemTypeWindInstrument ||
						instrument == EQ::item::ItemTypeStringedInstrument ||
						instrument == EQ::item::ItemTypeBrassInstrument ||
						instrument == EQ::item::ItemTypePercussionInstrument
					) {
						Log(Logs::Detail, Logs::Inventory, "Cannot equip a secondary instrument with %s already in the primary.", m_inv[EQ::invslot::slotPrimary]->GetItem()->Name);
						continue; // Do not auto-equip instrument in Secondary when Primary is equipped.	
					}

					uint8 use = m_inv[EQ::invslot::slotPrimary]->GetItem()->ItemType;
					if(use == EQ::item::ItemType2HSlash || use == EQ::item::ItemType2HBlunt || use == EQ::item::ItemType2HPiercing)
						continue;
				}
				if
				(
					i == EQ::invslot::slotSecondary &&
					inst.IsWeapon() &&
					!CanDualWield()
				)
				{
					continue;
				}

				if (inst.IsEquipable(i))	// Equippable at this slot?
				{
					//send worn to everyone...
					PutLootInInventory(i, inst);
					uint8 worn_slot_material = EQ::InventoryProfile::CalcMaterialFromSlot(i);
					if(worn_slot_material != EQ::textures::materialInvalid &&
						(i != EQ::invslot::slotWrist2 || (i == EQ::invslot::slotWrist2 && !m_inv.GetItem(EQ::invslot::slotWrist1))))
					{
						SendWearChange(worn_slot_material, nullptr, false, true);
					}
					
					parse->EventItem(EVENT_EQUIP_ITEM, this, &inst, nullptr, "", i);
					UpdateItemHP(&inst);
					return true;
				}
			}
		}
	}

	// #2: Stackable item?
	if (inst.IsStackable())
	{
		if(TryStacking(&inst, ItemPacketTrade, try_worn, try_cursor))
			return true;
	}

	// #3: put it in inventory
	bool is_arrow = (inst.GetItem()->ItemType == EQ::item::ItemTypeArrow) ? true : false;
	int16 slot_id = m_inv.FindFreeSlot(inst.IsType(EQ::item::ItemClassBag), try_cursor, inst.GetItem()->Size, is_arrow);
	if (slot_id != INVALID_INDEX)
	{
		PutLootInInventory(slot_id, inst, bag_item_data);
		return true;
	}

	return false;
}

// solar: helper function for AutoPutLootInInventory
void Client::MoveItemCharges(EQ::ItemInstance &from, int16 to_slot, uint8 type)
{
	EQ::ItemInstance *tmp_inst = m_inv.GetItem(to_slot);

	if(tmp_inst && tmp_inst->GetCharges() < tmp_inst->GetItem()->StackSize)
	{
		// this is how much room is left on the item we're stacking onto
		int charge_slots_left = tmp_inst->GetItem()->StackSize - tmp_inst->GetCharges();
		// this is how many charges we can move from the looted item to
		// the item in the inventory
		int charges_to_move =
			from.GetCharges() < charge_slots_left ?
				from.GetCharges() :
				charge_slots_left;

		tmp_inst->SetCharges(tmp_inst->GetCharges() + charges_to_move);
		from.SetCharges(from.GetCharges() - charges_to_move);
		SendLootItemInPacket(tmp_inst, to_slot);
		if (to_slot == EQ::invslot::slotCursor){
			auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
			database.SaveCursor(this, s, e);
		} else
			database.SaveInventory(this->CharacterID(), tmp_inst, to_slot);
	}
}

void Client::SendLootItemInPacket(const EQ::ItemInstance* inst, int16 slot_id)
{
	SendItemPacket(slot_id,inst, ItemPacketTrade);
}

bool Client::IsValidSlot(uint32 slot) {
	if ((slot == (uint32)INVALID_INDEX) ||
		(slot >= EQ::invslot::SLOT_BEGIN && slot < EQ::invslot::POSSESSIONS_COUNT) ||
		(slot >= EQ::invbag::GENERAL_BAGS_BEGIN && slot <= EQ::invbag::CURSOR_BAG_END) ||
		(slot >= EQ::invslot::BANK_BEGIN && slot <= EQ::invslot::BANK_END) ||
		(slot >= EQ::invbag::BANK_BAGS_BEGIN && slot <= EQ::invbag::BANK_BAGS_END) ||
		(slot >= EQ::invslot::TRADE_BEGIN && slot <= EQ::invslot::TRADE_END) ||
		(slot >= EQ::invslot::WORLD_BEGIN && slot <= EQ::invslot::WORLD_END))
		return true;
	else
		return false;
}

bool Client::IsBankSlot(uint32 slot)
{
	if ((slot >= EQ::invslot::BANK_BEGIN && slot <= EQ::invslot::BANK_END) ||
		(slot >= EQ::invbag::BANK_BAGS_BEGIN && slot <= EQ::invbag::BANK_BAGS_END))
	{
		return true;
	}

	return false;
}

// Moves items around both internally and in the database
// In the future, this can be optimized by pushing all changes through one database REPLACE call
bool Client::SwapItem(MoveItem_Struct* move_in) {

	uint32 src_slot_check = move_in->from_slot;
	uint32 dst_slot_check = move_in->to_slot;
	uint32 stack_count_check = move_in->number_in_stack;

	// This could be expounded upon at some point to let the server know that
	// the client has moved a buffered cursor item onto the active cursor -U
	if (move_in->from_slot == move_in->to_slot) { // Item summon, no further processing needed
		if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in); } // QS Audit
		return true;
	}

	if (move_in->to_slot == (uint32)INVALID_INDEX) {
		if (move_in->from_slot == (uint32)EQ::invslot::slotCursor) {
			Log(Logs::Detail, Logs::Inventory, "Client destroyed item from cursor slot %d", move_in->from_slot);
			if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in); } // QS Audit

			EQ::ItemInstance *inst = m_inv.GetItem(EQ::invslot::slotCursor);
			if(inst) {
				parse->EventItem(EVENT_DESTROY_ITEM, this, inst, nullptr, "", 0);
			}

			DeleteItemInInventory(move_in->from_slot);
			return true; // Item destroyed by client
		}
		else {
			Log(Logs::Detail, Logs::Inventory, "Deleted item from slot %d as a result of an inventory container tradeskill combine.", move_in->from_slot);
			if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in); } // QS Audit
			DeleteItemInInventory(move_in->from_slot);
			return true; // Item deletetion
		}
	}
	if(auto_attack && (move_in->from_slot == EQ::invslot::slotPrimary || move_in->from_slot == EQ::invslot::slotSecondary || move_in->from_slot == EQ::invslot::slotRange))
		SetAttackTimer();
	else if(auto_attack && (move_in->to_slot == EQ::invslot::slotPrimary || move_in->to_slot == EQ::invslot::slotSecondary || move_in->to_slot == EQ::invslot::slotRange))
		SetAttackTimer();
	// Step 1: Variables
	int16 src_slot_id = (int16)move_in->from_slot;
	int16 dst_slot_id = (int16)move_in->to_slot;

	if(IsBankSlot(src_slot_id) ||
		IsBankSlot(dst_slot_id) ||
		IsBankSlot(src_slot_check) ||
		IsBankSlot(dst_slot_check))
	{
		uint32 distance = 0;
		NPC *banker = entity_list.GetClosestBanker(this, distance);

		if(!banker || distance > USE_BANKER_RANGE)
		{
			float dist = sqrtf((float)distance);
			auto hacked_string = fmt::format("Player tried to make use of a banker(items) but {} is non-existant or too far away ( {:.1f} units - window closes at 20).",
				banker ? banker->GetName() : "UNKNOWN NPC", dist);
			database.SetMQDetectionFlag(AccountName(), GetName(), hacked_string, zone->GetShortName());
			if (distance > USE_NPC_RANGE2) {
				Kick();	// Kicking player to avoid item loss do to client and server inventories not being sync'd
				std::string error = "Banker error.";
				Log(Logs::General, Logs::Inventory, error.c_str());
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error.c_str(), GetZoneID()); }
				return false;
			}
		}
	}

	//Setup
	uint32 srcitemid = 0;
	uint32 dstitemid = 0;
	EQ::ItemInstance* src_inst = m_inv.GetItem(src_slot_id);
	EQ::ItemInstance* dst_inst = m_inv.GetItem(dst_slot_id);
	if (src_inst){
		Log(Logs::Detail, Logs::Inventory, "Src slot %d has item %s (%d) with %d charges in it.", src_slot_id, src_inst->GetItem()->Name, src_inst->GetItem()->ID, src_inst->GetCharges());
		srcitemid = src_inst->GetItem()->ID;

		if (src_inst->GetCharges() > 0 && (src_inst->GetCharges() < (int16)move_in->number_in_stack || move_in->number_in_stack > src_inst->GetItem()->StackSize))
		{
			std::string error = "Insufficent number in stack.";
			Log(Logs::General, Logs::Inventory, error.c_str());
			if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error.c_str(), GetZoneID()); }
			return false;
		}
	}
	if (dst_inst) {
		Log(Logs::Detail, Logs::Inventory, "Dest slot %d has item %s (%d) with %d charges in it.", dst_slot_id, dst_inst->GetItem()->Name, dst_inst->GetItem()->ID, dst_inst->GetCharges());
		dstitemid = dst_inst->GetItem()->ID;
	}
	if (Trader && srcitemid>0){
		EQ::ItemInstance* srcbag;
		EQ::ItemInstance* dstbag;
		uint32 srcbagid =0;
		uint32 dstbagid = 0;

		//if (src_slot_id >= 250 && src_slot_id < 339) {
		if (src_slot_id >= EQ::invbag::GENERAL_BAGS_BEGIN && src_slot_id <= EQ::invbag::CURSOR_BAG_END) {
			srcbag = m_inv.GetItem(((int)(src_slot_id / 10)) - 3);
			if (srcbag)
				srcbagid = srcbag->GetItem()->ID;
		}
		//if (dst_slot_id >= 250 && dst_slot_id < 339) {
		if (dst_slot_id >= EQ::invbag::GENERAL_BAGS_BEGIN && dst_slot_id <= EQ::invbag::CURSOR_BAG_END) {
			dstbag = m_inv.GetItem(((int)(dst_slot_id / 10)) - 3);
			if (dstbag)
				dstbagid = dstbag->GetItem()->ID;
		}
		if ((srcbagid && srcbag->GetItem()->BagType== EQ::item::BagTypeTradersSatchel) ||
			(dstbagid && dstbag->GetItem()->BagType == EQ::item::BagTypeTradersSatchel) ||
			(srcitemid && src_inst && src_inst->GetItem()->BagType == EQ::item::BagTypeTradersSatchel) ||
			(dstitemid && dst_inst && dst_inst->GetItem()->BagType == EQ::item::BagTypeTradersSatchel)) {
			this->Trader_EndTrader();
			this->Message(Chat::Red,"You cannot move your Trader Satchels, or items inside them, while Trading.");
		}
	}

	bool recursive_si = false;
	char error[512];
	error[0] = 0;
	// Step 2: Validate item in from_slot
	// After this, we can assume src_inst is a valid ptr
	if (!src_inst && (src_slot_id < EQ::invslot::WORLD_BEGIN || src_slot_id > EQ::invslot::WORLD_END)) {
		if (dst_inst) {
			// If there is no source item, but there is a destination item,
			// move the slots around before deleting the invalid source slot item,
			// which is now in the destination slot.
			move_in->from_slot = dst_slot_check;
			move_in->to_slot = src_slot_check;
			move_in->number_in_stack = dst_inst->GetCharges();
			if(!SwapItem(move_in))
			{
				sprintf(error, "Recursive SwapItem call failed due to non-existent destination item (sourceslot: %i, destslot: %i)", src_slot_id, dst_slot_id);
				Log(Logs::General, Logs::Inventory, error);
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
				return false;
			}
			else
				recursive_si = true;
		}
		if(!recursive_si)
		{
			sprintf(error, "Source slot is invalid and Recursive SwapItem call failed. (sourceslot: %i, destslot: %i) Item in source slot may have been deleted without sending the client an update.", src_slot_id, dst_slot_id); 
			Log(Logs::General, Logs::Inventory, error);
			if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
			return false;
		}
	}

	// Check for GM only items.
	if(src_inst)
	{
		if(src_inst->GetItem()->GMFlag == -1 && this->Admin() < RuleI(GM, MinStatusToUseGMItem)) 
		{
			Message(Chat::Red, "You are not a GM or do not have the status to this item. Please relog to avoid a desync.");
			sprintf(error, "Player %s on account %s attempted to create a GM-only item with a status of %i.\n(Item: %u, GMFlag: %u)\n",
				GetName(), account_name, this->Admin(), src_inst->GetID(), src_inst->GetItem()->GMFlag);
			Log(Logs::General, Logs::Inventory, error);
			if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
			DeleteItemInInventory(src_slot_id,1,true);
			return false;
		}
	}
	// Check for No Drop Hacks
	Mob* with = trade->With();
	if ((with && with->IsClient() && !with->CastToClient()->IsBecomeNPC() && dst_slot_id >= EQ::invslot::TRADE_BEGIN && dst_slot_id <= EQ::invslot::TRADE_END)
	&& GetInv().CheckNoDrop(src_slot_id)
	&& RuleI(World, FVNoDropFlag) == 0 || RuleI(Character, MinStatusForNoDropExemptions) < Admin() && RuleI(World, FVNoDropFlag) == 2) {
		DeleteItemInInventory(src_slot_id);
		WorldKick();
		std::string error = "A no drop item was placed into a trade slot. Rejecting.";
		Log(Logs::General, Logs::Inventory, error.c_str());
		if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error.c_str(), GetZoneID()); }
		return false;
	}

	// Step 3: Check for interaction with World Container (tradeskills)
	if(m_tradeskill_object != nullptr) {
		if (src_slot_id >= EQ::invslot::WORLD_BEGIN && src_slot_id <= EQ::invslot::WORLD_END) {
			// Picking up item from world container
			EQ::ItemInstance* inst = m_tradeskill_object->PopItem(EQ::InventoryProfile::CalcBagIdx(src_slot_id));
			if (inst) {
				const EQ::ItemData* src_item = inst->GetItem();
				if (CheckLoreConflict(src_item))
				{
					auto outapp = new EQApplicationPacket(OP_MoveItem, sizeof(MoveItem_Struct));
					MoveItem_Struct* delitem = (MoveItem_Struct*)outapp->pBuffer;
					delitem->from_slot = dst_slot_id;
					delitem->to_slot = 0xFFFFFFFF;
					delitem->number_in_stack = 0xFFFFFFFF;
					QueuePacket(outapp);
					safe_delete(outapp);

					m_tradeskill_object->PutItem(EQ::InventoryProfile::CalcBagIdx(src_slot_id), inst);
					Message_StringID(Chat::White, DUP_LORE);
					safe_delete(inst);

					return true;
				}
				else
				{
					PutItemInInventory(dst_slot_id, *inst, false);
					safe_delete(inst);
				}
			}

			if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in, true); } // QS Audit

			return true;
		}
		else if (dst_slot_id >= EQ::invslot::WORLD_BEGIN && dst_slot_id <= EQ::invslot::WORLD_END) {
			// Putting item into world container, which may swap (or pile onto) with existing item
			uint8 world_idx = EQ::InventoryProfile::CalcBagIdx(dst_slot_id);
			EQ::ItemInstance* world_inst = m_tradeskill_object->PopItem(world_idx);

			// Case 1: No item in container, unidirectional "Put"
			if (world_inst == nullptr) {
				m_tradeskill_object->PutItem(world_idx, src_inst);
				m_inv.DeleteItem(src_slot_id);
			}
			else {
				const EQ::ItemData* world_item = world_inst->GetItem();
				const EQ::ItemData* src_item = src_inst->GetItem();
				if (world_item && src_item) {
					// Case 2: Same item on cursor, stacks, transfer of charges needed
					if ((world_item->ID == src_item->ID) && src_inst->IsStackable()) {
						int16 world_charges = world_inst->GetCharges();
						int16 src_charges = src_inst->GetCharges();

						// Fill up destination stack as much as possible
						world_charges += src_charges;
						if (world_charges > world_inst->GetItem()->StackSize) {
							src_charges = world_charges - world_inst->GetItem()->StackSize;
							world_charges = world_inst->GetItem()->StackSize;
						}
						else {
							src_charges = 0;
						}

						world_inst->SetCharges(world_charges);
						m_tradeskill_object->PutItem(world_idx, world_inst);
						m_tradeskill_object->Save();

						if (src_charges == 0) {
							m_inv.DeleteItem(src_slot_id); // DB remove will occur below
						}
						else {
							src_inst->SetCharges(src_charges);
						}
					}
					else {
						// Case 3: Swap the item on user with item in world container
						// World containers don't follow normal rules for swapping
						EQ::ItemInstance* inv_inst = m_inv.PopItem(src_slot_id);
						m_tradeskill_object->PutItem(world_idx, inv_inst);
						if (src_slot_id == EQ::invslot::slotCursor) {
							// push the item onto the front of the cursor queue to stay in sync with client
							m_inv.PushCursor(*world_inst, true);
						}
						else {
							// can this actually happen? swap between a world container slot and a slot that's not the cursor?
							m_inv.PutItem(src_slot_id, *world_inst);
						}
						safe_delete(inv_inst);
					}
				}
			}

			safe_delete(world_inst);
			if (src_slot_id == EQ::invslot::slotCursor) {
				auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
				database.SaveCursor(this, s, e);
			} else
				database.SaveInventory(character_id, m_inv[src_slot_id], src_slot_id);

			if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in, true); } // QS Audit

			return true;
		}
	}

	// Step 4: Check for entity trade
	if (dst_slot_id >= EQ::invslot::TRADE_BEGIN && dst_slot_id <= EQ::invslot::TRADE_END) {
		if (src_slot_id != EQ::invslot::slotCursor) {
			Kick();
			std::string error = "Trading item not on cursor.";
			Log(Logs::General, Logs::Inventory, error.c_str());
			if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error.c_str(), GetZoneID()); }
			return false;
		}
		if (with && trade->state != TradeNone && trade->state != Requesting) 
		{
			Log(Logs::Detail, Logs::Inventory, "Trade item move from slot %d to slot %d (trade with %s)", src_slot_id, dst_slot_id, with->GetName());
			// Fill Trade list with items from cursor
			if (!m_inv[EQ::invslot::slotCursor]) {
				Message(Chat::Red, "Error: Cursor item not located on server!");
				std::string error = "Error: Cursor item not located on server!";
				Log(Logs::General, Logs::Inventory, error.c_str());
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error.c_str(), GetZoneID()); }
				return false;
			}

			if(with->IsNPC() && with->IsEngaged())
			{
				if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in); } // QS Audit
				trade->AddEntity(dst_slot_id, move_in->number_in_stack);

				SendCancelTrade(with);
				Log(Logs::General, Logs::Trading, "Cancelled in-progress trade due to %s being in combat.", with->GetCleanName());
				return true;
			}

			// Add cursor item to trade bucket
			// Also sends trade information to other client of trade session
			if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in); } // QS Audit

			trade->AddEntity(dst_slot_id, move_in->number_in_stack);

			return true;
		} else {
			if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in); } // QS Audit
			
			if (src_inst) {
				int new_charges = 0;
				if (!dst_inst) {
					// Move item on cursor to the trade slots
					PutItemInInventory(dst_slot_id, *src_inst);
				}
				else
				{
					new_charges = (dst_inst->GetCharges()+src_inst->GetCharges());
					if (new_charges < dst_inst->GetItem()->StackSize)
					{
						dst_inst->SetCharges(new_charges);
						new_charges = 0;
					}
					else
					{
						new_charges = src_inst->GetCharges()-(dst_inst->GetItem()->StackSize-dst_inst->GetCharges()); //Leftover charges = charges - difference
						dst_inst->SetCharges(dst_inst->GetItem()->StackSize);
					}

				}
				if (new_charges > 0)
					GetInv().GetItem(src_slot_id)->SetCharges(new_charges);
				else
					DeleteItemInInventory(src_slot_id);
			}

			// now close out anything else associated with trade
			FinishTrade(this);
			auto canceltrade = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
			CancelTrade_Struct* ct = (CancelTrade_Struct*) canceltrade->pBuffer;
			ct->fromid = 0;
			ct->action = 1;
			if (with && with->IsClient()) {
				with->CastToClient()->QueuePacket(canceltrade);
				with->CastToClient()->FinishTrade(with);
				with->CastToClient()->trade->Reset();
			}
			FastQueuePacket(&canceltrade);
			trade->Reset();

			// SummonItem(src_inst->GetID(), src_inst->GetCharges());
			// DeleteItemInInventory(SlotCursor);

			return true;
		}
	}

	// Step 5: Swap (or stack) items
	if (move_in->number_in_stack > 0) {
		// Determine if charged items can stack
		if(src_inst && !src_inst->IsStackable()) {
			sprintf(error, "Move from %d to %d with stack size %d. %s is not a stackable item. (charname: %s)", src_slot_id, dst_slot_id, move_in->number_in_stack, src_inst->GetItem()->Name, GetName());
			Log(Logs::General, Logs::Inventory, error);
			if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
			return false;
		}

		if (src_inst && dst_inst) {
			if(src_inst->GetID() != dst_inst->GetID()) {
				sprintf(error, "Move from %d to %d with stack size %d. Incompatible item types: %d != %d", src_slot_id, dst_slot_id, move_in->number_in_stack, src_inst->GetID(), dst_inst->GetID());
				Log(Logs::General, Logs::Inventory, error);
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
				return false;
			}
			if(dst_inst->GetCharges() < dst_inst->GetItem()->StackSize) {
				//we have a chance of stacking.
				Log(Logs::Detail, Logs::Inventory, "Move from %d to %d with stack size %d. dest has %d/%d charges", src_slot_id, dst_slot_id, move_in->number_in_stack, dst_inst->GetCharges(), dst_inst->GetItem()->StackSize);
				// Charges can be emptied into dst
				uint16 usedcharges = dst_inst->GetItem()->StackSize - dst_inst->GetCharges();
				if (usedcharges > move_in->number_in_stack)
					usedcharges = move_in->number_in_stack;

				dst_inst->SetCharges(dst_inst->GetCharges() + usedcharges);
				src_inst->SetCharges(src_inst->GetCharges() - usedcharges);

				// Depleted all charges?
				if (src_inst->GetCharges() < 1)
				{
					Log(Logs::Detail, Logs::Inventory, "Dest (%d) now has %d charges, source (%d) was entirely consumed. (%d moved)", dst_slot_id, dst_inst->GetCharges(), src_slot_id, usedcharges);
					database.SaveInventory(CharacterID(),nullptr,src_slot_id);
					m_inv.DeleteItem(src_slot_id);
				} else {
					Log(Logs::Detail, Logs::Inventory, "Dest (%d) now has %d charges, source (%d) has %d (%d moved)", dst_slot_id, dst_inst->GetCharges(), src_slot_id, src_inst->GetCharges(), usedcharges);
				}
			} else {
				sprintf(error, "Move from %d to %d with stack size %d. Exceeds dest maximum stack size: %d/%d", src_slot_id, dst_slot_id, move_in->number_in_stack, (src_inst->GetCharges()+dst_inst->GetCharges()), dst_inst->GetItem()->StackSize);
				Log(Logs::General, Logs::Inventory, error);
				if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
				return false;
			}
		
		}
		else if(!src_inst)
		{
			return false;
		}
		else {
			// Nothing in destination slot: split stack into two
			if ((int8)move_in->number_in_stack >= src_inst->GetCharges()) {
				// Move entire stack
				if(!m_inv.SwapItem(src_slot_id, dst_slot_id)) { 
					sprintf(error, "Could not move entire stack from %d to %d with stack size %d. Dest empty.", src_slot_id, dst_slot_id, move_in->number_in_stack);
					Log(Logs::General, Logs::Inventory, error);
					if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
					return false; 
				}
				Log(Logs::Detail, Logs::Inventory, "Move entire stack from %d to %d with stack size %d. Dest empty.", src_slot_id, dst_slot_id, move_in->number_in_stack);
			}
			else {
				// Split into two
				src_inst->SetCharges(src_inst->GetCharges() - move_in->number_in_stack);
				Log(Logs::Detail, Logs::Inventory, "Split stack of %s (%d) from slot %d to %d with stack size %d. Src keeps %d.", src_inst->GetItem()->Name, src_inst->GetItem()->ID, src_slot_id, dst_slot_id, move_in->number_in_stack, src_inst->GetCharges());
				EQ::ItemInstance* inst = database.CreateItem(src_inst->GetItem(), move_in->number_in_stack);
				m_inv.PutItem(dst_slot_id, *inst);
				safe_delete(inst);
			}
		}
	}
	else {
		// Not dealing with charges - just do direct swap
		if(src_inst && dst_slot_id <= EQ::invslot::EQUIPMENT_END && dst_slot_id >= EQ::invslot::EQUIPMENT_BEGIN) {
			SetMaterial(dst_slot_id,src_inst->GetItem()->ID);
		}
		if(!m_inv.SwapItem(src_slot_id, dst_slot_id)) {
			sprintf(error, "Destination slot is not valid for item %s from slot %d to slot %d", src_inst->GetItem()->Name, src_slot_id, dst_slot_id);
			Log(Logs::General, Logs::Inventory, error);
			if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error, GetZoneID()); }
			return false;
		}
		Log(Logs::Detail, Logs::Inventory, "Moving entire item from slot %d to slot %d", src_slot_id, dst_slot_id);

		if (src_slot_id >= EQ::invslot::EQUIPMENT_BEGIN && src_slot_id <= EQ::invslot::EQUIPMENT_END) {
			if(src_inst) {
				parse->EventItem(EVENT_UNEQUIP_ITEM, this, src_inst, nullptr, "", src_slot_id);
				UpdateItemHP(src_inst, false);
			}

			if(dst_inst) {
				parse->EventItem(EVENT_EQUIP_ITEM, this, dst_inst, nullptr, "", src_slot_id);
				UpdateItemHP(dst_inst);
			}
		}

		if (dst_slot_id >= EQ::invslot::EQUIPMENT_BEGIN && dst_slot_id <= EQ::invslot::EQUIPMENT_END) {
			if(dst_inst) {
				parse->EventItem(EVENT_UNEQUIP_ITEM, this, dst_inst, nullptr, "", dst_slot_id);
				UpdateItemHP(dst_inst, false);
			}

			if(src_inst) {
				parse->EventItem(EVENT_EQUIP_ITEM, this, src_inst, nullptr, "", dst_slot_id);
				UpdateItemHP(src_inst);
			}
		}
	}

	if (dst_slot_id == EQ::invslot::slotCursor && (src_slot_id == EQ::invslot::slotWrist1 || src_slot_id == EQ::invslot::slotWrist2))
	{
		// If we remove an item in wrist1, our arms will appear naked to our own client even if we have an item in wrist2.
		// Also handle removing wrist2 when nothing is in wrist1, since we are not handling wrist wearchanges initiated by the client after wrist texture has been set once.
		// Finally, if wrist2 is removed and it's the same material as wrist1, then arms will appear naked.
		const EQ::ItemInstance* wrist1 = m_inv.GetItem(EQ::invslot::slotWrist1);
		if (src_slot_id == EQ::invslot::slotWrist1 ||
			(src_slot_id == EQ::invslot::slotWrist2 && (!wrist1 || (wrist1 && src_inst && wrist1->GetItem()->Material == src_inst->GetItem()->Material))))
		{
			pc_bracertexture = 0;
			bracercolor = 0;
			WearChange(EQ::textures::armorWrist, 0, 0);
		}
	}
	else
	{
		int8 matslot = EQ::InventoryProfile::CalcMaterialFromSlot(dst_slot_id);
		if (matslot != INVALID_INDEX)
		{
			SendWearChange(matslot, nullptr, false, true);
			if (matslot == EQ::textures::armorHead && !ShowHelm())
			{
				WearChange(EQ::textures::armorHead, 0, 0, this);
			}
		}
	}

	// Step 7: Save change to the database
	if (src_slot_id == EQ::invslot::slotCursor){
		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		database.SaveCursor(this, s, e);
	} else
		database.SaveInventory(character_id, m_inv.GetItem(src_slot_id), src_slot_id);

	if (dst_slot_id == EQ::invslot::slotCursor) {
		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		database.SaveCursor(this, s, e);

	} 
	else
	{
		database.SaveInventory(character_id, m_inv.GetItem(dst_slot_id), dst_slot_id);

		// When we have a bag on the cursor filled with items that is new (zoned with it, summoned it, picked it up from the ground)
		// the client is only aware of the bag. So, we have to send packets for each item within the bag once it is placed in the inventory.
		// This is a workaround hack until we can figure out how to send these items to the client while they are still on the cursor.
		const EQ::ItemInstance* baginst = m_inv[dst_slot_id];
		if(src_slot_id == EQ::invslot::slotCursor && baginst && baginst->IsType(EQ::item::ItemClassBag))
		{
			for (int16 trade_bag_slot = EQ::invbag::GENERAL_BAGS_BEGIN + (dst_slot_id - EQ::invslot::GENERAL_BEGIN) * EQ::invbag::SLOT_COUNT;
				trade_bag_slot <= EQ::invbag::GENERAL_BAGS_BEGIN + (dst_slot_id - EQ::invslot::GENERAL_BEGIN) * EQ::invbag::SLOT_COUNT + 9;
				++trade_bag_slot) {
				const EQ::ItemInstance* inst = m_inv[trade_bag_slot];
				if(inst)
				{
					Log(Logs::Detail, Logs::Inventory, "Sending out packet for bagged item: %s in slot: %i bag slot: %i", inst->GetItem()->Name, trade_bag_slot, dst_slot_id);
					SendItemPacket(trade_bag_slot, inst, ItemPacketTrade);
				}
			}
		}
	}

	if(RuleB(QueryServ, PlayerLogMoves)) { QSSwapItemAuditor(move_in, true); } // QS Audit

	// Step 8: Re-calc stats
	CalcBonuses();
	return true;
}

void Client::SwapItemResync(MoveItem_Struct* move_slots) {
	// wow..this thing created a helluva memory leak...
	// with any luck..this won't be needed in the future

	// re sync the 'from' and 'to' slots on an as-needed basis
	// Not as effective as the full process, but less intrusive to game play -U

	bool resync = false;
	Log(Logs::Detail, Logs::Inventory, "Inventory desynchronization. (charname: %s, source: %i, destination: %i)", GetName(), move_slots->from_slot, move_slots->to_slot);
	Message(Chat::Yellow, "Inventory Desynchronization detected: Resending slot data...");

	if((move_slots->from_slot >= EQ::invslot::EQUIPMENT_BEGIN && move_slots->from_slot <= EQ::invbag::CURSOR_BAG_END)) {
		int16 resync_slot = (EQ::InventoryProfile::CalcSlotId(move_slots->from_slot) == INVALID_INDEX) ? move_slots->from_slot : EQ::InventoryProfile::CalcSlotId(move_slots->from_slot);
		if (IsValidSlot(resync_slot) && resync_slot != INVALID_INDEX) {
			// This prevents the client from crashing when closing any 'phantom' bags -U
			const EQ::ItemData* token_struct = database.GetItem(22292); // 'Copper Coin'
			EQ::ItemInstance* token_inst = database.CreateItem(token_struct, 1);

			SendItemPacket(resync_slot, token_inst, ItemPacketTrade);

			if(m_inv[resync_slot]) { SendItemPacket(resync_slot, m_inv[resync_slot], ItemPacketTrade); }
			else {
				auto outapp		= new EQApplicationPacket(OP_MoveItem, sizeof(MoveItem_Struct));
				MoveItem_Struct* delete_slot	= (MoveItem_Struct*)outapp->pBuffer;
				delete_slot->from_slot			= resync_slot;
				delete_slot->to_slot			= 0xFFFFFFFF;
				delete_slot->number_in_stack	= 0xFFFFFFFF;

				QueuePacket(outapp);
				safe_delete(outapp);
			}
			safe_delete(token_inst);
			Message(Chat::Lime, "Source slot %i resynchronized.", move_slots->from_slot);
			resync = true;
		}
		else { Message(Chat::Red, "Could not resynchronize source slot %i.", move_slots->from_slot); }
	}
	else {
		int16 resync_slot = (EQ::InventoryProfile::CalcSlotId(move_slots->from_slot) == INVALID_INDEX) ? move_slots->from_slot : EQ::InventoryProfile::CalcSlotId(move_slots->from_slot);
		if (IsValidSlot(resync_slot) && resync_slot != INVALID_INDEX) {
			if(m_inv[resync_slot]) {
				const EQ::ItemData* token_struct = database.GetItem(22292); // 'Copper Coin'
				EQ::ItemInstance* token_inst = database.CreateItem(token_struct, 1);

				SendItemPacket(resync_slot, token_inst, ItemPacketTrade);
				SendItemPacket(resync_slot, m_inv[resync_slot], ItemPacketTrade);

				safe_delete(token_inst);
				Message(Chat::Lime, "Source slot %i resynchronized.", move_slots->from_slot);
				resync = true;
			}
			else { Message(Chat::Red, "Could not resynchronize source slot %i.", move_slots->from_slot); }
		}
		else { Message(Chat::Red, "Could not resynchronize source slot %i.", move_slots->from_slot); }
	}

	if((move_slots->to_slot >= EQ::invslot::EQUIPMENT_BEGIN && move_slots->to_slot <= EQ::invbag::CURSOR_BAG_END)) {
		int16 resync_slot = (EQ::InventoryProfile::CalcSlotId(move_slots->to_slot) == INVALID_INDEX) ? move_slots->to_slot : EQ::InventoryProfile::CalcSlotId(move_slots->to_slot);
		if (IsValidSlot(resync_slot) && resync_slot != INVALID_INDEX) {
			const EQ::ItemData* token_struct = database.GetItem(22292); // 'Copper Coin'
			EQ::ItemInstance* token_inst = database.CreateItem(token_struct, 1);

			SendItemPacket(resync_slot, token_inst, ItemPacketTrade);

			if(m_inv[resync_slot]) { SendItemPacket(resync_slot, m_inv[resync_slot], ItemPacketTrade); }
			else {
				auto outapp		= new EQApplicationPacket(OP_MoveItem, sizeof(MoveItem_Struct));
				MoveItem_Struct* delete_slot	= (MoveItem_Struct*)outapp->pBuffer;
				delete_slot->from_slot			= resync_slot;
				delete_slot->to_slot			= 0xFFFFFFFF;
				delete_slot->number_in_stack	= 0xFFFFFFFF;

				QueuePacket(outapp);
				safe_delete(outapp);
			}
			safe_delete(token_inst);
			Message(Chat::Lime, "Destination slot %i resynchronized.", move_slots->to_slot);
			resync = true;
		}
		else { Message(Chat::Red, "Could not resynchronize destination slot %i.", move_slots->to_slot); }
	}
	else {
		int16 resync_slot = (EQ::InventoryProfile::CalcSlotId(move_slots->to_slot) == INVALID_INDEX) ? move_slots->to_slot : EQ::InventoryProfile::CalcSlotId(move_slots->to_slot);
		if (IsValidSlot(resync_slot) && resync_slot != INVALID_INDEX) {
			if(m_inv[resync_slot]) {
				const EQ::ItemData* token_struct = database.GetItem(22292); // 'Copper Coin'
				EQ::ItemInstance* token_inst = database.CreateItem(token_struct, 1);

				SendItemPacket(resync_slot, token_inst, ItemPacketTrade);
				SendItemPacket(resync_slot, m_inv[resync_slot], ItemPacketTrade);

				safe_delete(token_inst);
				Message(Chat::Lime, "Destination slot %i resynchronized.", move_slots->to_slot);
				resync = true;
			}
			else { Message(Chat::Red, "Could not resynchronize destination slot %i.", move_slots->to_slot); }
		}
		else { Message(Chat::Red, "Could not resynchronize destination slot %i.", move_slots->to_slot); }
	}

	if(resync)
	{
		std::string error = "Item successfully resycned.";
		if (RuleB(QueryServ, PlayerLogItemDesyncs)) { QServ->QSItemDesyncs(CharacterID(), error.c_str(), GetZoneID()); }
	}

	if (Admin() < 10)
	{
		Message(Chat::Red, "Disconnecting to avoid item loss, please relog.");
		Kick();
	}
}

void Client::QSSwapItemAuditor(MoveItem_Struct* move_in, bool postaction_call) {
	int16 from_slot_id = static_cast<int16>(move_in->from_slot);
	int16 to_slot_id	= static_cast<int16>(move_in->to_slot);
	int16 move_amount	= static_cast<int16>(move_in->number_in_stack);

	if(!m_inv[from_slot_id] && !m_inv[to_slot_id]) { return; }

	uint16 move_count = 0;

	if(m_inv[from_slot_id]) { move_count += m_inv[from_slot_id]->GetTotalItemCount(); }
	if(to_slot_id != from_slot_id) { if(m_inv[to_slot_id]) { move_count += m_inv[to_slot_id]->GetTotalItemCount(); } }

	auto pack = new ServerPacket(ServerOP_QSPlayerLogItemMoves, sizeof(QSPlayerLogItemMove_Struct) + (sizeof(QSMoveItems_Struct) * move_count));
	QSPlayerLogItemMove_Struct* QS = (QSPlayerLogItemMove_Struct*)pack->pBuffer;

	QS->char_id = character_id;
	QS->stack_size = move_amount;
	QS->char_count = move_count;
	QS->postaction = postaction_call;
	QS->from_slot = from_slot_id;
	QS->to_slot = to_slot_id;

	move_count = 0;

	const EQ::ItemInstance* from_inst = m_inv[postaction_call?to_slot_id:from_slot_id];

	if(from_inst) {
		QS->items[move_count].from_slot = from_slot_id;
		QS->items[move_count].to_slot = to_slot_id;
		QS->items[move_count].item_id = from_inst->GetID();
		QS->items[move_count++].charges = from_inst->GetCharges();

		if(from_inst->IsType(EQ::item::ItemClassBag)) {
			for(uint8 bag_idx = EQ::invbag::SLOT_BEGIN; bag_idx < from_inst->GetItem()->BagSlots; bag_idx++) {
				const EQ::ItemInstance* from_baginst = from_inst->GetItem(bag_idx);

				if(from_baginst) {
					QS->items[move_count].from_slot = EQ::InventoryProfile::CalcSlotId(from_slot_id, bag_idx);
					QS->items[move_count].to_slot = EQ::InventoryProfile::CalcSlotId(to_slot_id, bag_idx);
					QS->items[move_count].item_id = from_baginst->GetID();
					QS->items[move_count++].charges = from_baginst->GetCharges();
				}
			}
		}
	}

	if(to_slot_id != from_slot_id) {
		const EQ::ItemInstance* to_inst = m_inv[postaction_call?from_slot_id:to_slot_id];

		if(to_inst) {
			QS->items[move_count].from_slot = to_slot_id;
			QS->items[move_count].to_slot = from_slot_id;
			QS->items[move_count].item_id = to_inst->GetID();
			QS->items[move_count++].charges = to_inst->GetCharges();

			if(to_inst->IsType(EQ::item::ItemClassBag)) {
				for(uint8 bag_idx = EQ::invbag::SLOT_BEGIN; bag_idx < to_inst->GetItem()->BagSlots; bag_idx++) {
					const EQ::ItemInstance* to_baginst = to_inst->GetItem(bag_idx);

					if(to_baginst) {
						QS->items[move_count].from_slot = EQ::InventoryProfile::CalcSlotId(to_slot_id, bag_idx);
						QS->items[move_count].to_slot = EQ::InventoryProfile::CalcSlotId(from_slot_id, bag_idx);
						QS->items[move_count].item_id = to_baginst->GetID();
						QS->items[move_count++].charges = to_baginst->GetCharges();
					}
				}
			}
		}
	}

	if(move_count && worldserver.Connected()) {
		pack->Deflate();
		worldserver.SendPacket(pack);
	}

	safe_delete(pack);
}

bool Client::DecreaseByID(uint32 type, uint8 amt) {
	const EQ::ItemData* TempItem = nullptr;
	EQ::ItemInstance* ins;
	int x;
	int num = 0;
	for(x = EQ::invslot::EQUIPMENT_BEGIN; x <= EQ::invbag::GENERAL_BAGS_END; x++)
	{
		if (x == EQ::invslot::slotCursor + 31)
			x = EQ::invbag::GENERAL_BAGS_BEGIN;
		TempItem = nullptr;
		ins = GetInv().GetItem(x);
		if (ins)
			TempItem = ins->GetItem();
		if (TempItem && TempItem->ID == type)
		{
			num += ins->GetCharges();
			if (num >= amt)
				break;
		}
	}
	if (num < amt)
		return false;
	for(x = EQ::invslot::EQUIPMENT_BEGIN; x <= EQ::invbag::GENERAL_BAGS_END; x++) // should this be CURSOR_BAG_END?
	{
		if (x == EQ::invslot::slotCursor + 31)
			x = EQ::invbag::GENERAL_BAGS_BEGIN;
		TempItem = nullptr;
		ins = GetInv().GetItem(x);
		if (ins)
			TempItem = ins->GetItem();
		if (TempItem && TempItem->ID == type)
		{
			if (ins->GetCharges() < amt)
			{
				amt -= ins->GetCharges();
				DeleteItemInInventory(x,amt,true);
			}
			else
			{
				DeleteItemInInventory(x,amt,true);
				amt = 0;
			}
			if (amt < 1)
				break;
		}
	}
	return true;
}

void Client::RemoveNoRent(bool client_update)
{
	for (auto slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::EQUIPMENT_END; ++slot_id) {
		auto inst = m_inv[slot_id];
		if(inst && !inst->GetItem()->NoRent) {
			Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			DeleteItemInInventory(slot_id, 0, client_update);
		}
	}

	// general
	for (auto slot_id = EQ::invslot::GENERAL_BEGIN; slot_id <= EQ::invslot::GENERAL_END; ++slot_id) {
		auto inst = m_inv[slot_id];
		if (inst && !inst->GetItem()->NoRent) {
			Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			if (inst->IsType(EQ::item::ItemClassBag)) {
				// we have a no rent bag
				for(uint8 bag_idx = EQ::invbag::SLOT_BEGIN; bag_idx < m_inv[slot_id]->GetItem()->BagSlots; bag_idx++) {
					EQ::ItemInstance* bagitem = m_inv[slot_id]->GetItem(bag_idx);
					if(bagitem) {
						int16 bagslot_id = EQ::InventoryProfile::CalcSlotId(slot_id, bag_idx);
						Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting bagitem %s from slot %i", bagitem->GetItem()->Name, bagslot_id);
						DeleteItemInInventory(bagslot_id, 0, client_update);
					}
				}
			}
			DeleteItemInInventory(slot_id, 0, client_update);
		}
	}
	// containers
	for(auto slot_id = EQ::invbag::GENERAL_BAGS_BEGIN; slot_id <= EQ::invbag::CURSOR_BAG_END; ++slot_id) {
		auto inst = m_inv[slot_id];
		if(inst && !inst->GetItem()->NoRent) {
			Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			if (inst->IsType(EQ::item::ItemClassBag)) {
				// we have a no rent bag
				for(uint8 bag_idx = EQ::invbag::SLOT_BEGIN; bag_idx < m_inv[slot_id]->GetItem()->BagSlots; bag_idx++) {
					EQ::ItemInstance* bagitem = m_inv[slot_id]->GetItem(bag_idx);
					if(bagitem) {
						int16 bagslot_id = EQ::InventoryProfile::CalcSlotId(slot_id, bag_idx);
						Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting bagitem %s from slot %i", bagitem->GetItem()->Name, bagslot_id);
						DeleteItemInInventory(bagslot_id, 0, client_update);
					}
				}
			}
			DeleteItemInInventory(slot_id, 0, client_update);
		}
	}

	// bank
	for(auto slot_id = EQ::invslot::BANK_BEGIN; slot_id <= EQ::invslot::BANK_END; ++slot_id) {
		auto inst = m_inv[slot_id];
		if(inst && !inst->GetItem()->NoRent) {
			Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			if (inst->IsType(EQ::item::ItemClassBag)) {
				// we have a no rent bag
				for(uint8 bag_idx = EQ::invbag::SLOT_BEGIN; bag_idx < m_inv[slot_id]->GetItem()->BagSlots; bag_idx++) {
					EQ::ItemInstance* bagitem = m_inv[slot_id]->GetItem(bag_idx);
					if(bagitem) {
						int16 bagslot_id = EQ::InventoryProfile::CalcSlotId(slot_id, bag_idx);
						Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting bagitem %s from slot %i", bagitem->GetItem()->Name, bagslot_id);
						DeleteItemInInventory(bagslot_id, 0, client_update);
					}
				}
			}
			DeleteItemInInventory(slot_id, 0, client_update);
		}
	}

	// bank containers
	for(auto slot_id = EQ::invbag::BANK_BAGS_BEGIN; slot_id <= EQ::invbag::BANK_BAGS_END; ++slot_id) {
		auto inst = m_inv[slot_id];
		if(inst && !inst->GetItem()->NoRent) {
			Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			DeleteItemInInventory(slot_id, 0, false); // Can't delete from client Bank Container slots
		}
	}

	// cursor & limbo
	if (!m_inv.CursorEmpty()) {
		std::list<EQ::ItemInstance*> local;

		while (!m_inv.CursorEmpty()) {
			auto inst = m_inv.PopItem(EQ::invslot::slotCursor);
			if (inst == nullptr) { continue; }
				local.push_back(inst);
		}

		for (auto iter = local.begin(); iter != local.end(); ++iter) {
			auto inst = *iter;
			if (inst == nullptr) { continue; }
			if (!inst->GetItem()->NoRent)
			{
				Log(Logs::Detail, Logs::Inventory, "NoRent Timer Lapse: Deleting %s from `Limbo`", inst->GetItem()->Name);
			}
			else
			{
				m_inv.PushCursor(*inst);
			}

			safe_delete(inst);
		}
		local.clear();

		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		database.SaveCursor(this, s, e);
		local.clear();
	}
}

// Two new methods to alleviate perpetual login desyncs
void Client::RemoveDuplicateLore(bool client_update) {
	int16 slot_id = 0;

	// equipment
	for(auto slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::EQUIPMENT_END; ++slot_id) {
		auto inst = m_inv.PopItem(slot_id);
		if (inst == nullptr) { continue;}
		if(CheckLoreConflict(inst->GetItem())) {
			Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			database.SaveInventory(character_id, nullptr, slot_id);
		}
		else {
			m_inv.PutItem(slot_id, *inst);
		}
		safe_delete(inst);
	}

	// general
	for (auto slot_id = EQ::invslot::GENERAL_BEGIN; slot_id <= EQ::invslot::GENERAL_END; ++slot_id) {
		auto inst = m_inv.PopItem(slot_id);
		if (inst == nullptr) { continue; }
		if (CheckLoreConflict(inst->GetItem())) {
			Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			database.SaveInventory(character_id, nullptr, slot_id);
		}
		else {
			m_inv.PutItem(slot_id, *inst);
		}
		safe_delete(inst);
	}

	// containers
	for(auto slot_id = EQ::invbag::GENERAL_BAGS_BEGIN; slot_id <= EQ::invbag::CURSOR_BAG_END; ++slot_id) {
		auto inst = m_inv.PopItem(slot_id);
		if (inst == nullptr) { continue; }
		if(CheckLoreConflict(inst->GetItem())) {
			Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			database.SaveInventory(character_id, nullptr, slot_id);
		}
		else {
			m_inv.PutItem(slot_id, *inst);
		}
		safe_delete(inst);
	}

	// bank
	for(auto slot_id = EQ::invslot::BANK_BEGIN; slot_id <= EQ::invslot::BANK_END; ++slot_id) {
		auto inst = m_inv.PopItem(slot_id);
		if (inst == nullptr) { continue; }
		if(CheckLoreConflict(inst->GetItem())) {
			Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			database.SaveInventory(character_id, nullptr, slot_id);
		}
		else {
			m_inv.PutItem(slot_id, *inst);
		}
		safe_delete(inst);
	}

	// bank containers
	for(auto slot_id = EQ::invbag::BANK_BAGS_BEGIN; slot_id <= EQ::invbag::BANK_BAGS_END; ++slot_id) {
		auto inst = m_inv.PopItem(slot_id);
		if (inst == nullptr) { continue; }
		if(CheckLoreConflict(inst->GetItem())) {
			Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from slot %i", inst->GetItem()->Name, slot_id);
			database.SaveInventory(character_id, nullptr, slot_id);
		}
		else {
			m_inv.PutItem(slot_id, *inst);
		}
		safe_delete(inst);
	}

	// cursor & limbo
	if (!m_inv.CursorEmpty()) {
		std::list<EQ::ItemInstance*> local_1;
		std::list<EQ::ItemInstance*> local_2;

		while (!m_inv.CursorEmpty()) {
			auto inst = m_inv.PopItem(EQ::invslot::slotCursor);
			if (inst == nullptr) { continue; }
			local_1.push_back(inst);
		}

		for (auto iter = local_1.begin(); iter != local_1.end(); ++iter) {
			auto inst = *iter;
			if (inst == nullptr) { continue; }
			if (CheckLoreConflict(inst->GetItem())) {
				Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from `Limbo`", inst->GetItem()->Name);
				safe_delete(inst);
			}
			else {
				local_2.push_back(inst);
			}
		}
		local_1.clear();

		for (auto iter = local_2.begin(); iter != local_2.end(); ++iter) {
			auto inst = *iter;
			if ((inst->GetItem()->Lore[0] != '*' && inst->GetItem()->Lore[0] != '#') ||
				((inst->GetItem()->Lore[0] == '*') && (m_inv.HasItem(inst->GetID(), 0, invWhereCursor) == INVALID_INDEX)) ||
				((inst->GetItem()->Lore[0] == '#') && (m_inv.HasArtifactItem() == INVALID_INDEX))) {
				
				m_inv.PushCursor(*inst);
			}
			else {
				Log(Logs::Detail, Logs::Inventory, "Lore Duplication Error: Deleting %s from `Limbo`", inst->GetItem()->Name);
			}

			safe_delete(inst);
		}
		local_2.clear();

		auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
		database.SaveCursor(this, s, e);
		
	}
}

void Client::MoveSlotNotAllowed(bool client_update)
{
	// equipment
	for(auto slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::EQUIPMENT_END; ++slot_id) {
		if(m_inv[slot_id] && !m_inv[slot_id]->IsSlotAllowed(slot_id, m_inv)) {
			auto inst = m_inv.PopItem(slot_id);
			bool is_arrow = (inst->GetItem()->ItemType == EQ::item::ItemTypeArrow) ? true : false;
			int16 free_slot_id = m_inv.FindFreeSlot(inst->IsType(EQ::item::ItemClassBag), true, inst->GetItem()->Size, is_arrow);
			Log(Logs::Detail, Logs::Inventory, "Slot Assignment Error: Moving %s from slot %i to %i", inst->GetItem()->Name, slot_id, free_slot_id);
			PutItemInInventory(free_slot_id, *inst, client_update);
			database.SaveInventory(character_id, nullptr, slot_id);
			safe_delete(inst);
		}
	}

	// No need to check inventory, cursor, bank since they allow max item size and containers -U
	// Code can be added to check item size vs. container size, but it is left to attrition for now.
}

// these functions operate with a material slot, which is from 0 to 8
uint32 Client::GetEquipment(uint8 material_slot) const
{
	int invslot;
	const EQ::ItemInstance *item;

	if(material_slot > EQ::textures::LastTexture)
	{
		return 0;
	}

	invslot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);
	if(invslot == INVALID_INDEX)
	{
		return 0;
	}

	item = m_inv.GetItem(invslot);

	// We don't have an item in Wrist1, check Wrist2.
	if (!item && invslot == EQ::invslot::slotWrist1)
	{
		item = m_inv.GetItem(EQ::invslot::slotWrist2);
	}

	if(item && item->GetItem())
	{
		return item->GetItem()->ID;
	}

	return 0;
}

uint32 Client::GetEquipmentColor(uint8 material_slot) const
{
	const EQ::ItemData *item;

	if(material_slot > EQ::textures::LastTexture)
	{
		return 0;
	}

	item = database.GetItem(GetEquipment(material_slot));
	if(item != 0)
	{
		return item->Color;
	}

	return 0;
}

// Send an item packet (including all subitems of the item)
void Client::SendItemPacket(int16 slot_id, const EQ::ItemInstance* inst, ItemPacketType packet_type, int16 fromid, int16 toid, int16 skill)
{
	if (!inst)
		return;

	// Serialize item into |-delimited string
	std::string packet = inst->Serialize(slot_id);

	EmuOpcode opcode = OP_Unknown;
	EQApplicationPacket* outapp = nullptr;
	ItemPacket_Struct* itempacket = nullptr;

	// Construct packet
	if(packet_type==ItemPacketViewLink)
		opcode = OP_ItemLinkResponse;
	else if(packet_type==ItemPacketTradeView)
		opcode = OP_TradeItemPacket;
	else if(packet_type==ItemPacketStolenItem)
		opcode = OP_PickPocket;
	else
		opcode = OP_ItemPacket;
	//opcode = (packet_type==ItemPacketViewLink) ? OP_ItemLinkResponse : OP_ItemPacket;
	outapp = new EQApplicationPacket(opcode, packet.length()+sizeof(ItemPacket_Struct));
	itempacket = (ItemPacket_Struct*)outapp->pBuffer;
	memcpy(itempacket->SerializedItem, packet.c_str(), packet.length());
	itempacket->PacketType = packet_type;
	itempacket->fromid = fromid;
	itempacket->toid = toid;
	itempacket->skill = skill;

#if EQDEBUG >= 9
		DumpPacket(outapp);
#endif
	FastQueuePacket(&outapp);
}

bool Client::MoveItemToInventory(EQ::ItemInstance *ItemToReturn, bool UpdateClient) {
	// This is a support function for Client::SetBandolier, however it can be used anywhere it's functionality is required.
	//
	// When the client moves items around as Bandolier sets are activated, it does not send details to the
	// server of what item it has moved to which slot. It assumes the server knows what it will do.
	//
	// The standard EQEmu auto inventory routines do not behave as the client does when manipulating bandoliers.
	// The client will look in each main inventory slot. If it finds a bag in a slot, it will then look inside
	// the bag for a free slot.
	//
	// This differs from the standard EQEmu method of looking in all 8 inventory slots first to find an empty slot, and
	// then going back and looking in bags. There are also other differences related to how it moves stackable items back
	// to inventory.
	//
	// Rather than alter the current auto inventory behaviour, just in case something
	// depends on current behaviour, this routine operates the same as the client when moving items back to inventory when
	// swapping bandolier sets.

	if(!ItemToReturn) return false;

	Log(Logs::Detail, Logs::Inventory,"Char: %s Returning %s to inventory", GetName(), ItemToReturn->GetItem()->Name);

	uint32 ItemID = ItemToReturn->GetItem()->ID;

	// If the item is stackable (ammo in range slot), try stacking it with other items of the same type
	//
	if(ItemToReturn->IsStackable()) {

		for (int16 i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++) { 

			EQ::ItemInstance* InvItem = m_inv.GetItem(i);

			if(InvItem && (InvItem->GetItem()->ID == ItemID) && (InvItem->GetCharges() < InvItem->GetItem()->StackSize)) {

				int ChargeSlotsLeft = InvItem->GetItem()->StackSize - InvItem->GetCharges();

				int ChargesToMove = ItemToReturn->GetCharges() < ChargeSlotsLeft ? ItemToReturn->GetCharges() :
													ChargeSlotsLeft;

				InvItem->SetCharges(InvItem->GetCharges() + ChargesToMove);

				if(UpdateClient)
					SendItemPacket(i, InvItem, ItemPacketTrade);

				database.SaveInventory(character_id, m_inv.GetItem(i), i);

				ItemToReturn->SetCharges(ItemToReturn->GetCharges() - ChargesToMove);

				if(!ItemToReturn->GetCharges())
					return true;
			}
			// If there is a bag in this slot, look inside it.
			//
			if (InvItem && InvItem->IsType(EQ::item::ItemClassBag)) {

				int16 BaseSlotID = EQ::InventoryProfile::CalcSlotId(i, EQ::invbag::SLOT_BEGIN);

				uint8 BagSize=InvItem->GetItem()->BagSlots;

				uint8 BagSlot;
				for (BagSlot = EQ::invbag::SLOT_BEGIN; BagSlot < BagSize; BagSlot++) {
					InvItem = m_inv.GetItem(BaseSlotID + BagSlot);
					if (InvItem && (InvItem->GetItem()->ID == ItemID) &&
						(InvItem->GetCharges() < InvItem->GetItem()->StackSize)) {

						int ChargeSlotsLeft = InvItem->GetItem()->StackSize - InvItem->GetCharges();

						int ChargesToMove = ItemToReturn->GetCharges() < ChargeSlotsLeft
									? ItemToReturn->GetCharges() : ChargeSlotsLeft;

						InvItem->SetCharges(InvItem->GetCharges() + ChargesToMove);

						if(UpdateClient)
							SendItemPacket(BaseSlotID + BagSlot, m_inv.GetItem(BaseSlotID + BagSlot),
										ItemPacketTrade);

						database.SaveInventory(character_id, m_inv.GetItem(BaseSlotID + BagSlot),
										BaseSlotID + BagSlot);

						ItemToReturn->SetCharges(ItemToReturn->GetCharges() - ChargesToMove);

						if(!ItemToReturn->GetCharges())
							return true;
					}
				}
			}
		}
	}

	// We have tried stacking items, now just try and find an empty slot.

	for (int16 i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++) { 

		EQ::ItemInstance* InvItem = m_inv.GetItem(i);

		if (!InvItem) {
			// Found available slot in personal inventory
			m_inv.PutItem(i, *ItemToReturn);

			if(UpdateClient)
				SendItemPacket(i, ItemToReturn, ItemPacketTrade);

			database.SaveInventory(character_id, m_inv.GetItem(i), i);

			Log(Logs::Detail, Logs::Inventory, "Char: %s Storing in main inventory slot %i", GetName(), i);

			return true;
		}
		if(InvItem->IsType(EQ::item::ItemClassBag) && EQ::InventoryProfile::CanItemFitInContainer(ItemToReturn->GetItem(), InvItem->GetItem())) {

			int16 BaseSlotID = EQ::InventoryProfile::CalcSlotId(i, EQ::invbag::SLOT_BEGIN);

			uint8 BagSize=InvItem->GetItem()->BagSlots;

			for (uint8 BagSlot = EQ::invbag::SLOT_BEGIN; BagSlot < BagSize; BagSlot++) {

				InvItem = m_inv.GetItem(BaseSlotID + BagSlot);

				if (!InvItem) {
					// Found available slot within bag
					m_inv.PutItem(BaseSlotID + BagSlot, *ItemToReturn);

					if(UpdateClient)
						SendItemPacket(BaseSlotID + BagSlot, ItemToReturn, ItemPacketTrade);

					database.SaveInventory(character_id, m_inv.GetItem(BaseSlotID + BagSlot), BaseSlotID + BagSlot);

					Log(Logs::Detail, Logs::Inventory, "Char: %s Storing in bag slot %i", GetName(), BaseSlotID + BagSlot);

					return true;
				}
			}
		}
	}

	// Store on the cursor
	//
	Log(Logs::Detail, Logs::Inventory, "Char: %s No space, putting on the cursor", GetName());

	PushItemOnCursor(*ItemToReturn, UpdateClient);

	return true;
}

bool Client::InterrogateInventory(Client* requester, bool log, bool silent, bool allowtrip, bool& error, bool autolog)
{
	if (!requester)
		return false;

	std::map<int16, const EQ::ItemInstance*> instmap;

	// build reference map
	for (int16 index = EQ::invslot::SLOT_BEGIN; index < EQ::invslot::POSSESSIONS_COUNT; ++index)
	{
		auto inst = m_inv[index];
		if (inst == nullptr) { continue; }
		instmap[index] = inst;
	}
	for (int16 index = EQ::invslot::BANK_BEGIN; index <= EQ::invslot::BANK_END; ++index)
	{
		auto inst = m_inv[index];
		if (inst == nullptr) { continue; }
		instmap[index] = inst;
	}
	for (int16 index = EQ::invslot::TRADE_BEGIN; index <= EQ::invslot::TRADE_END; ++index)
	{
		auto inst = m_inv[index];
		if (inst == nullptr) { continue; }
		instmap[index] = inst;
	}

	auto tsobject = GetTradeskillObject();
	if (tsobject != nullptr)
	{
		for (int16 index = EQ::invslot::SLOT_BEGIN; index < EQ::invtype::WORLD_SIZE; ++index)
		{
			auto inst = tsobject->GetItem(index);
			if (inst == nullptr) { continue; }
			instmap[EQ::invslot::WORLD_BEGIN + index] = inst;
		}
	}

	int limbo = 0;
	for (auto cursor_itr = m_inv.cursor_cbegin(); cursor_itr != m_inv.cursor_cend(); ++cursor_itr, ++limbo) {
		// m_inv.cursor_begin() is referenced as SlotCursor in MapPossessions above
		if (cursor_itr == m_inv.cursor_cbegin())
			continue;

		instmap[EQ::invslot::CURSOR_QUEUE_BEGIN + limbo] = *cursor_itr;
	}

	// call InterrogateInventory_ for error check
	for (auto instmap_itr = instmap.begin(); (instmap_itr != instmap.end()) && (!error); ++instmap_itr)
		InterrogateInventory_(true, requester, instmap_itr->first, INVALID_INDEX, instmap_itr->second, nullptr, log, silent, error, 0);

	if (autolog && error && (!log))
		log = true;

	if (!silent)
		requester->Message(1, "--- Inventory Interrogation Report for %s (requested by: %s, error state: %s) ---", GetName(), requester->GetName(), (error ? "TRUE" : "FALSE"));

	// call InterrogateInventory_ for report
	for (auto instmap_itr = instmap.begin(); (instmap_itr != instmap.end()); ++instmap_itr)
		InterrogateInventory_(false, requester, instmap_itr->first, INVALID_INDEX, instmap_itr->second, nullptr, log, silent, error, 0);

	if (error) {
		Message(Chat::Red, "An error has been discovered in your inventory!");
		Message(Chat::Red, "Do not log out, zone or re-arrange items until this");
		Message(Chat::Red, "issue has been resolved or item loss may occur!");

		if (allowtrip)
			TripInterrogateInvState();
	}

	if (log) {
		Log(Logs::General, Logs::Error, "Target interrogate inventory flag: %s", (GetInterrogateInvState() ? "TRUE" : "FALSE"));
		Log(Logs::Detail, Logs::None, "[CLIENT] Client::InterrogateInventory() -- End");
	}
	if (!silent) {
		requester->Message(1, "Target interrogation flag: %s", (GetInterrogateInvState() ? "TRUE" : "FALSE"));
		requester->Message(1, "--- End of Interrogation Report ---");
	}

	instmap.clear();

	return true;
}

void Client::InterrogateInventory_(bool errorcheck, Client* requester, int16 head, int16 index, const EQ::ItemInstance* inst, const EQ::ItemInstance* parent, bool log, bool silent, bool &error, int depth)
{
	if (depth >= 10) {
		Log(Logs::Detail, Logs::None, "[CLIENT] Client::InterrogateInventory_() - Recursion count has exceeded the maximum allowable (You have a REALLY BIG PROBLEM!!)");
		return;
	}

	if (errorcheck) {
		if (InterrogateInventory_error(head, index, inst, parent, depth)) {
			error = true;
		}
		else {
			if (inst)
				for (int16 sub = EQ::invbag::SLOT_BEGIN; (sub <= EQ::invbag::SLOT_END) && (!error); ++sub) // treat any EQ::ItemInstance as having the max internal slots available
					if (inst->GetItem(sub))
						InterrogateInventory_(true, requester, head, sub, inst->GetItem(sub), inst, log, silent, error, depth + 1);
		}
	}
	else {
		bool localerror = InterrogateInventory_error(head, index, inst, parent, depth);
		std::string i;
		std::string p;
		std::string e;

		if (inst) { i = StringFormat("%s (class: %u)", inst->GetItem()->Name, inst->GetItem()->ItemClass); }
		else { i = "NONE"; }
		if (parent) { p = StringFormat("%s (class: %u), index: %i", parent->GetItem()->Name, parent->GetItem()->ItemClass,index); }
		else { p = "NONE"; }
		if (localerror) { e = " [ERROR]"; }
		else { e = ""; }

		if (log)
			Log(Logs::General, Logs::Error, "Head: %i, Depth: %i, Instance: %s, Parent: %s%s",
			head, depth, i.c_str(), p.c_str(), e.c_str());
		if (!silent)
			requester->Message(1, "%i:%i - inst: %s - parent: %s%s",
			head, depth, i.c_str(), p.c_str(), e.c_str());

		if (inst)
			for (int16 sub = EQ::invbag::SLOT_BEGIN; (sub <= EQ::invbag::SLOT_END); ++sub)
				if (inst->GetItem(sub))
					InterrogateInventory_(false, requester, head, sub, inst->GetItem(sub), inst, log, silent, error, depth + 1);
	}

	return;
}

bool Client::InterrogateInventory_error(int16 head, int16 index, const EQ::ItemInstance* inst, const EQ::ItemInstance* parent, int depth)
{
	// very basic error checking - can be elaborated upon if more in-depth testing is needed...

	if ((head == EQ::invslot::slotCursor) ||
		(head >= EQ::invslot::EQUIPMENT_BEGIN && head <= EQ::invslot::EQUIPMENT_END) ||
		(head >= EQ::invslot::WORLD_BEGIN && head <= EQ::invslot::WORLD_END) ||
		(head >= EQ::invslot::CURSOR_QUEUE_BEGIN && head <= EQ::invslot::CURSOR_QUEUE_END)) {
		switch (depth)
		{
		case 0: // requirement: inst is extant
			if (!inst)
				return true;
			break;
		case 1: // requirement: parent is common and inst is augment
			if ((!parent) || (!inst))
				return true;
			if (!parent->IsType(EQ::item::ItemClassCommon))
				return true;
			break;
		default: // requirement: none (something bad happened...)
			return true;
		}
	}
	else if (
		(head >= EQ::invslot::GENERAL_BEGIN && head <= EQ::invslot::GENERAL_END) ||
		(head >= EQ::invslot::BANK_BEGIN && head <= EQ::invslot::BANK_END) ||
		(head >= EQ::invslot::TRADE_BEGIN && head <= EQ::invslot::TRADE_END)) {
		switch (depth)
		{
		case 0: // requirement: inst is extant
			if (!inst)
				return true;
			break;
		case 1: // requirement: parent is common and inst is augment ..or.. parent is container and inst is extant
			if ((!parent) || (!inst))
				return true;
			if (parent->IsType(EQ::item::ItemClassBag))
				break;
			if (parent->IsType(EQ::item::ItemClassBook))
				return true;
			if (parent->IsType(EQ::item::ItemClassCommon))
				return true;
			break;
		case 2: // requirement: parent is common and inst is augment
			if ((!parent) || (!inst))
				return true;
			if (parent->IsType(EQ::item::ItemClassBag))
				return true;
			if (parent->IsType(EQ::item::ItemClassBook))
				return true;
			if (parent->IsType(EQ::item::ItemClassCommon))
				return true;
			break;
		default: // requirement: none (something bad happened again...)
			return true;
		}
	}
	else {
		return true;
	}

	return false;
}

void EQ::InventoryProfile::SetCustomItemData(uint32 character_id, int16 slot_id, std::string identifier, std::string value) {
	ItemInstance *inst = GetItem(slot_id);
	if(inst) {
		inst->SetCustomData(identifier, value);
		database.SaveInventory(character_id, inst, slot_id);
	}
}

void EQ::InventoryProfile::SetCustomItemData(uint32 character_id, int16 slot_id, std::string identifier, int value) {
	ItemInstance *inst = GetItem(slot_id);
	if(inst) {
		inst->SetCustomData(identifier, value);
		database.SaveInventory(character_id, inst, slot_id);
	}
}

void EQ::InventoryProfile::SetCustomItemData(uint32 character_id, int16 slot_id, std::string identifier, float value) {
	ItemInstance *inst = GetItem(slot_id);
	if(inst) {
		inst->SetCustomData(identifier, value);
		database.SaveInventory(character_id, inst, slot_id);
	}
}

void EQ::InventoryProfile::SetCustomItemData(uint32 character_id, int16 slot_id, std::string identifier, bool value) {
	ItemInstance *inst = GetItem(slot_id);
	if(inst) {
		inst->SetCustomData(identifier, value);
		database.SaveInventory(character_id, inst, slot_id);
	}
}

std::string EQ::InventoryProfile::GetCustomItemData(int16 slot_id, std::string identifier) {
	ItemInstance *inst = GetItem(slot_id);
	if(inst) {
		return inst->GetCustomData(identifier);
	}
	return "";
}

void Client::UpdateItemHP(EQ::ItemInstance* item, bool equip)
{
	if (!item)
		return;

	int32 old_hp = GetHP();
	int32 old_max = GetMaxHP();
	int32 new_HP = 0;
	int32 item_hp = item->GetItem()->HP;

	CalcBonuses();

	if(item_hp != 0)
	{
		if(equip)
		{
			new_HP = GetHP() + item_hp;
		}
		else
		{
			new_HP = old_hp - item_hp;
			if (GetMaxHP() <= item_hp)
			{
				if(new_HP < 1)
					new_HP = GetHP() - (old_max - old_hp);
			}
		}

		new_HP = new_HP > GetMaxHP() ? GetMaxHP() : new_HP;

		Log(Logs::General, Logs::Inventory, "Item %s was %s. Setting HP to %d/%d. It was %d/%d", item->GetItem()->Name, equip ? "equipped" : "unequipped", new_HP, GetMaxHP(), old_hp, old_max);

		SetHP(new_HP);
		SendHPUpdate();
	}
}


void Client::UpdateHP(int32 item_hp, bool equip)
{
	int32 old_hp = GetHP();
	int32 old_max = GetMaxHP();
	int32 new_HP = 0;

	CalcMaxHP();

	if (item_hp != 0)
	{
		if (equip)
		{
			new_HP = GetHP() + item_hp;
		}
		else
		{
			new_HP = old_hp - item_hp;
			if (GetMaxHP() <= item_hp)
			{
				if (new_HP < 1)
					new_HP = GetHP() - (old_max - old_hp);
			}
		}

		new_HP = new_HP > GetMaxHP() ? GetMaxHP() : new_HP;

		Log(Logs::General, Logs::Inventory, "HP bonus was %s. Setting HP to %d/%d. It was %d/%d", equip ? "equipped" : "unequipped", new_HP, GetMaxHP(), old_hp, old_max);

		SetHP(new_HP);
		SendHPUpdate();
	}
}

bool Client::SpillBeer()
{
	for (int16 i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++) 
	{
		const EQ::ItemInstance* inst = m_inv[i];

		if (inst && inst->GetItem() && inst->GetItem()->ItemType == EQ::item::ItemTypeAlcohol)
		{
			DeleteItemInInventory(i, 1, true);
			return true;
		}
	}

	for (int16 i = EQ::invbag::GENERAL_BAGS_BEGIN; i <= EQ::invbag::GENERAL_BAGS_END; i++)
	{
		const EQ::ItemInstance* inst = m_inv[i];

		if (inst && inst->GetItem() && inst->GetItem()->ItemType == EQ::item::ItemTypeAlcohol)
		{
			DeleteItemInInventory(i, 1, true);
			return true;
		}
	}

	return false;
}