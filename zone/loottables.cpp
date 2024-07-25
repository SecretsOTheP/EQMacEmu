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

#include "../common/global_define.h"
#include "../common/loottable.h"
#include "../common/misc_functions.h"
#include "../common/data_verification.h"

#include "client.h"
#include "entity.h"
#include "mob.h"
#include "npc.h"
#include "zonedb.h"
#include "global_loot_manager.h"
#include "../common/repositories/criteria/content_filter_criteria.h"

#include <iostream>
#include <stdlib.h>

#ifdef _WINDOWS
#define snprintf	_snprintf
#endif


// Queries the loottable: adds item & coin to the npc
void ZoneDatabase::AddLootTableToNPC(NPC* npc, uint32 loottable_id, ItemList* itemlist, uint32* copper, uint32* silver, uint32* gold, uint32* plat) {
	const LootTable_Struct* lts = nullptr;
	// global loot passes nullptr for these
	bool bGlobal = copper == nullptr && silver == nullptr && gold == nullptr && plat == nullptr;
	if (!bGlobal) {
		*copper = 0;
		*silver = 0;
		*gold = 0;
		*plat = 0;
	}


	lts = database.GetLootTable(loottable_id);
	if (!lts) {
		return;
	}

	if (!content_service.DoesPassContentFiltering(lts->content_flags)) {
		return;
	}

	uint32 min_cash = lts->mincash;
	uint32 max_cash = lts->maxcash;
	if (min_cash > max_cash) {
		uint32 t = min_cash;
		min_cash = max_cash;
		max_cash = t;
	}

	uint32 cash = 0;
	if (!bGlobal) {
		if (max_cash > 0 && lts->avgcoin > 0 && EQ::ValueWithin(lts->avgcoin, min_cash, max_cash)) {
			float upper_chance = (float)(lts->avgcoin - min_cash) / (float)(max_cash - min_cash);
			float avg_cash_roll = (float)zone->random.Real(0.0, 1.0);

			if (avg_cash_roll < upper_chance) {
				cash = zone->random.Int(lts->avgcoin, max_cash);
			}
			else {
				cash = zone->random.Int(min_cash, lts->avgcoin);
			}
		}
		else {
			cash = zone->random.Int(min_cash, max_cash);
		}
	}

	if (cash != 0) {
		*plat = cash / 1000;
		cash -= *plat * 1000;

		*gold = cash / 100;
		cash -= *gold * 100;

		*silver = cash / 10;
		cash -= *silver * 10;

		*copper = cash;
	}

	// Do items
	for (uint32 i = 0; i<lts->NumEntries; i++) {
		uint8 multiplier_count = 0;
		for (uint32 k = 1; k <= lts->Entries[i].multiplier; k++) {
			uint8 droplimit = lts->Entries[i].droplimit;
			uint8 mindrop = lts->Entries[i].mindrop;
			uint8 multiplier_min = lts->Entries[i].multiplier_min;

			//LootTable Entry probability
			float ltchance = 0.0f;
			ltchance = lts->Entries[i].probability;

			float drop_chance = 0.0f;
			if (ltchance > 0.0 && ltchance < 100.0 && multiplier_count >= multiplier_min) {
				drop_chance = (float)zone->random.Real(0.0, 100.0);
			}
			else if (multiplier_count < multiplier_min)
			{
				drop_chance = 0.0f;
			}

			if (ltchance != 0.0 && (ltchance == 100.0 || drop_chance <= ltchance)) {
				AddLootDropToNPC(npc, lts->Entries[i].lootdrop_id, itemlist, droplimit, mindrop);
			}

			++multiplier_count;
		}
	}
}

// Called by AddLootTableToNPC
// maxdrops = size of the array npcd
void ZoneDatabase::AddLootDropToNPC(NPC *npc, uint32 lootdrop_id, ItemList* itemlist, uint8 droplimit, uint8 mindrop) {
	const LootDrop_Struct *loot_drop = GetLootDrop(lootdrop_id);
	if (!loot_drop) {
		return;
	}

	if (loot_drop->NumEntries == 0) {
		return;
	}

	if (!content_service.DoesPassContentFiltering(loot_drop->content_flags)) {
		return;
	}

	if (droplimit == 0 && mindrop == 0) {
		for (uint32 i = 0; i < loot_drop->NumEntries; ++i) {

			bool min_expansion_enabled = RuleR(World, CurrentExpansion) >= loot_drop->Entries[i].min_expansion;
			bool max_expansion_enabled = RuleR(World, CurrentExpansion) < loot_drop->Entries[i].max_expansion;
			bool expansion_all = loot_drop->Entries[i].min_expansion == ExpansionEras::AllEQEras;
			if (!expansion_all && (!min_expansion_enabled || !max_expansion_enabled))
				continue;

			int multiplier = loot_drop->Entries[i].multiplier;
			for (int j = 0; j < multiplier; ++j) {
				if (zone->random.Real(0.0, 100.0) <= loot_drop->Entries[i].chance) {
					uint32 itemid = loot_drop->Entries[i].item_id;
					int8 charges = loot_drop->Entries[i].item_charges;
					const EQ::ItemData* db_item = GetItem(itemid);
					if (database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
					{
						if (charges <= 1)
							charges = db_item->MaxCharges;
					}
					bool force_equip = loot_drop->Entries[i].equip_item == 2;
					npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[i].minlevel,
						loot_drop->Entries[i].maxlevel, loot_drop->Entries[i].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[i].min_looter_level, loot_drop->Entries[i].item_loot_lockout_timer);
				}
			}
		}
		return;
	}

	if (loot_drop->NumEntries > 100 && droplimit == 0) {
		droplimit = 10;
	}

	if (droplimit < mindrop) {
		droplimit = mindrop;
	}

	float roll_t = 0.0f;
	float roll_t_min = 0.0f;
	bool active_item_list = false;

	for (uint32 i = 0; i < loot_drop->NumEntries; ++i) {
		bool min_expansion_enabled =  RuleR(World, CurrentExpansion) >= loot_drop->Entries[i].min_expansion;
		bool max_expansion_enabled = RuleR(World, CurrentExpansion) < loot_drop->Entries[i].max_expansion;
		bool expansion_all = loot_drop->Entries[i].min_expansion == ExpansionEras::AllEQEras;
		if (!expansion_all && (!min_expansion_enabled || !max_expansion_enabled))
			continue;

		uint32 itemid = loot_drop->Entries[i].item_id;
		int8 charges = loot_drop->Entries[i].item_charges;
		const EQ::ItemData* db_item = GetItem(itemid);
		if (database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
		{
			if (charges <= 1)
				charges = db_item->MaxCharges;
		}
		if (db_item) {
			roll_t += loot_drop->Entries[i].chance;
			active_item_list = true;
		}
	}

	roll_t_min = roll_t;
	roll_t = EQ::ClampLower(roll_t, 100.0f);

	if (!active_item_list) {
		return;
	}

	for(int i = 0; i < mindrop; ++i) {
		float roll = (float)zone->random.Real(0.0, roll_t_min);
		for(uint32 j = 0; j < loot_drop->NumEntries; ++j) {
			bool min_expansion_enabled = RuleR(World, CurrentExpansion) >= loot_drop->Entries[j].min_expansion;
			bool max_expansion_enabled = RuleR(World, CurrentExpansion) < loot_drop->Entries[j].max_expansion;
			bool expansion_all = loot_drop->Entries[j].min_expansion == ExpansionEras::AllEQEras;
			if (!expansion_all && (!min_expansion_enabled || !max_expansion_enabled))
				continue;

			uint32 itemid = loot_drop->Entries[j].item_id;
			int8 charges = loot_drop->Entries[j].item_charges;
			const EQ::ItemData* db_item = GetItem(itemid);
			if (database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
			{
				if (charges <= 1)
					charges = db_item->MaxCharges;
			}
			if (db_item) {
				if(roll < loot_drop->Entries[j].chance) {
					bool force_equip = loot_drop->Entries[j].equip_item == 2;
					npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[j].minlevel,
									 loot_drop->Entries[j].maxlevel, loot_drop->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[j].min_looter_level, loot_drop->Entries[j].item_loot_lockout_timer);

					int multiplier = (int)loot_drop->Entries[j].multiplier;
					multiplier = EQ::ClampLower(multiplier, 1);

					for (int k = 1; k < multiplier; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if(c_roll <= loot_drop->Entries[j].chance) {
							bool force_equip = loot_drop->Entries[j].equip_item == 2;
							npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[j].minlevel,
											 loot_drop->Entries[j].maxlevel, loot_drop->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[j].min_looter_level, loot_drop->Entries[j].item_loot_lockout_timer);
						}
					}

					j = loot_drop->NumEntries;
					break;
				}
				else {
					roll -= loot_drop->Entries[j].chance;
				}
			}
		}
	}

	// drop limit handling has two loops to ensure that the table drop %s are indicative of what is seen in-game
	if ((droplimit - mindrop) == 1)
	{
		float roll = (float)zone->random.Real(0.0, roll_t);
		for (uint32 j = 0; j < loot_drop->NumEntries; ++j)
		{
			bool min_expansion_enabled = RuleR(World, CurrentExpansion) >= loot_drop->Entries[j].min_expansion;
			bool max_expansion_enabled = RuleR(World, CurrentExpansion) < loot_drop->Entries[j].max_expansion;
			bool expansion_all = loot_drop->Entries[j].min_expansion == ExpansionEras::AllEQEras;
			
			if (!expansion_all && (!min_expansion_enabled || !max_expansion_enabled))
				continue;

			uint32 itemid = loot_drop->Entries[j].item_id;
			int8 charges = loot_drop->Entries[j].item_charges;
			const EQ::ItemData* db_item = GetItem(itemid);
			if (database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
			{
				if (charges <= 1)
					charges = db_item->MaxCharges;
			}
			if (db_item) {
				if (roll < loot_drop->Entries[j].chance) {
					bool force_equip = loot_drop->Entries[j].equip_item == 2;
					npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[j].minlevel,
						loot_drop->Entries[j].maxlevel, loot_drop->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[j].min_looter_level, loot_drop->Entries[j].item_loot_lockout_timer);

					int multiplier = (int)loot_drop->Entries[j].multiplier;
					multiplier = EQ::ClampLower(multiplier, 1);

					for (int k = 1; k < multiplier; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if (c_roll <= loot_drop->Entries[j].chance) {
							bool force_equip = loot_drop->Entries[j].equip_item == 2;
							npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[j].minlevel,
								loot_drop->Entries[j].maxlevel, loot_drop->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[j].min_looter_level, loot_drop->Entries[j].item_loot_lockout_timer);
						}
					}
					break;
				}
				else {
					roll -= loot_drop->Entries[j].chance;
				}
			}
		}
	}
	else if (droplimit > mindrop)
	{
		// If droplimit is 2 or more higher than mindrop, then we run this other loop.
		// This can't be used on droplimit=mindrop+1 without actual drop rates being lower than what the table %s
		// would indicate; however the above solution doesn't work well for droplimits greater than 1 above mindrop.
		// This will not be as precise as the above loop but the deviation is greatly reduced as droplimit increases.
		int dropCount = mindrop;
		int i = zone->random.Int(0, loot_drop->NumEntries);
		int loops = 0;

		while (loops < loot_drop->NumEntries)
		{
			if (dropCount >= droplimit)
				break;
			bool min_expansion_enabled = RuleR(World, CurrentExpansion) >= loot_drop->Entries[i].min_expansion;
			bool max_expansion_enabled = RuleR(World, CurrentExpansion) < loot_drop->Entries[i].max_expansion;
			bool expansion_all = loot_drop->Entries[i].min_expansion == ExpansionEras::AllEQEras;

			uint32 itemid = loot_drop->Entries[i].item_id;
			int8 charges = loot_drop->Entries[i].item_charges;
			const EQ::ItemData* db_item = GetItem(itemid);
			if (database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
			{
				if (charges <= 1)
					charges = db_item->MaxCharges;
			}
			if (db_item && min_expansion_enabled && max_expansion_enabled || db_item && expansion_all)
			{
				if (zone->random.Real(0.0, 100.0) <= loot_drop->Entries[i].chance)
				{
					bool force_equip = loot_drop->Entries[i].equip_item == 2;
					npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[i].minlevel,
						loot_drop->Entries[i].maxlevel, loot_drop->Entries[i].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[i].min_looter_level, loot_drop->Entries[i].item_loot_lockout_timer);

					int multiplier = (int)loot_drop->Entries[i].multiplier;
					multiplier = EQ::ClampLower(multiplier, 1);

					for (int k = 1; k < multiplier; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if (c_roll <= loot_drop->Entries[i].chance) {
							bool force_equip = loot_drop->Entries[i].equip_item == 2;
							npc->AddLootDrop(db_item, itemlist, charges, loot_drop->Entries[i].minlevel,
								loot_drop->Entries[i].maxlevel, loot_drop->Entries[i].equip_item > 0 ? true : false, false, false, false, force_equip, loot_drop->Entries[i].min_looter_level, loot_drop->Entries[i].item_loot_lockout_timer);
						}
					}
					dropCount = dropCount + 1;
				}
			}
			i++;
			if (i > loot_drop->NumEntries)
				i = 0;
			loops++;
		}
	}

	npc->UpdateEquipmentLight();
	// no wearchange associated with this function..so, this should not be needed
	//if (npc->UpdateActiveLightValue())
	//	npc->SendAppearancePacket(AT_Light, npc->GetActiveLightValue());
}

//if itemlist is null, just send wear changes
void NPC::AddLootDrop(const EQ::ItemData *item2, ItemList* itemlist, int8 charges, uint8 minlevel, uint8 maxlevel, bool equipit, bool wearchange, bool quest, bool pet, bool force_equip, uint8 min_looter_level, uint32 item_loot_lockout_timer) {
	if(item2 == nullptr)
		return;

	//make sure we are doing something...
	if(!itemlist && !wearchange)
		return;

	
	if(CountQuestItems() >= MAX_NPC_QUEST_INVENTORY)
		return;

	auto item = new ServerLootItem_Struct;

	if(quest || pet)
		Log(Logs::Detail, Logs::Trading, "Adding %s to npc: %s. Wearchange: %d Equipit: %d Multiquest: %d Pet: %d", item2->Name, GetName(), wearchange, equipit, quest, pet);

	EQApplicationPacket* outapp = nullptr;
	WearChange_Struct* wc = nullptr;
	if(wearchange) {
		outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
		wc = (WearChange_Struct*)outapp->pBuffer;
		wc->spawn_id = GetID();
		wc->material=0;
	}

	item->item_id = item2->ID;
	item->charges = charges;
	item->min_level = minlevel;
	item->max_level = maxlevel;
	item->quest = quest;
	item->equip_slot = EQ::invslot::slotGeneral1; //Set default slot to general inventory. NPCs can have multiple items in the same slot.
	item->pet = pet;
	item->forced = false;
	item->min_looter_level = min_looter_level;
	item->item_loot_lockout_timer = item_loot_lockout_timer;

	// unsure if required to equip, YOLO for now
	if (item2->ItemType == EQ::item::ItemTypeBow) {
		SetBowEquipped(true);
	}

	if (item2->ItemType == EQ::item::ItemTypeArrow) {
		SetArrowEquipped(true);
	}

	if(pet && quest)
	{
		Log(Logs::Detail, Logs::Trading, "Error: Item %s is being added to %s as both a pet and a quest.", item2->Name, GetName());
		item->pet = 0;
	}

	const EQ::ItemData* mainHand = nullptr;
	if (equipment[EQ::invslot::slotPrimary] > 0)
		mainHand = database.GetItem(equipment[EQ::invslot::slotPrimary]);

	if (item2->ID == 11543 && IsPet()) // Sony hardcoded pets to not equip A Weighted Axe
		equipit = false;

	bool mainhandIs1h = mainHand && (mainHand->ItemType == EQ::item::ItemType1HSlash || mainHand->ItemType == EQ::item::ItemType1HBlunt || mainHand->ItemType == EQ::item::ItemType1HPiercing);
	bool itemIs2h = item2->ItemType == EQ::item::ItemType2HBlunt || item2->ItemType == EQ::item::ItemType2HPiercing || item2->ItemType == EQ::item::ItemType2HSlash;
	bool itemIs1h = item2->ItemType == EQ::item::ItemType1HBlunt || item2->ItemType == EQ::item::ItemType1HPiercing || item2->ItemType == EQ::item::ItemType1HSlash;
	bool offhandEmpty = GetEquipment(EQ::textures::weaponSecondary) == 0 ? true : false;

	bool send_wearchange = false;
	if (equipit && item2->ItemClass == EQ::item::ItemClassCommon && !GetSpecialAbility(DO_NOT_EQUIP)) {
		uint8 eslot = EQ::textures::materialInvalid;
		const EQ::ItemData* compitem = nullptr; 
		ServerLootItem_Struct* sitem = nullptr;
		bool found = false; // track if we found an empty slot we fit into
		int32 foundslot = INVALID_INDEX; // for multi-slot items
		bool upgrade = false;
		bool special_slot = false; // Ear, Ring, and Wrist only allows 1 item for NPCs.

		// Equip rules are as follows:
		// An empty slot takes priority. The first empty one that an item can
		// fit into will be the one picked for the item, unless it is a multi slot
		// ranged item.
		// AC is the primary choice for which armor gets picked for a slot.
		// If AC is identical HP is considered next.
		// Damage is used for weapons.
		// If an item can fit into multiple slots we'll pick the last one where
		// it is an improvement.

		for (int i = 0; !found && i <= EQ::invslot::EQUIPMENT_COUNT; ++i)
		{
			uint32 slots = (1 << i);

			// If the item is ranged, and has other equip slots prefer to use another slot so
			// the weapon graphic shows in-game.
			if (i == EQ::invslot::slotRange && item2->Slots != slots)
			{
				continue;
			}

			if (item2->Slots & slots)
			{
				if (equipment[i])
				{
					sitem = GetItem(i);
					if (sitem && sitem->forced)
					{
						// Existing item is forced. Try another slot.
						sitem = nullptr;
						continue;
					}

					if (item2->ItemType == EQ::item::ItemTypeShield)	// prevent shields from replacing weapons
						break;

					compitem = database.GetItem(equipment[i]);
					if (!compitem)
						continue;

					// If we're not a pet and this is an armor item, or if this is a weapon for any npc, or if this is a forced item check then for upgrade slot.
					if ((item->pet == 0 && (i != EQ::invslot::slotPrimary && i != EQ::invslot::slotSecondary && i != EQ::invslot::slotRange && (item2->AC > compitem->AC || (item2->AC == compitem->AC && item2->HP > compitem->HP))))
						|| (i == EQ::invslot::slotPrimary || i == EQ::invslot::slotSecondary || i == EQ::invslot::slotRange) || force_equip
					)
					{
						// item would be an upgrade
						// check if we're multi-slot, if yes then we have to keep
						// looking in case any of the other slots we can fit into are empty.
						if (item2->Slots != slots)
						{
							foundslot = i;
							upgrade = true;
							if (itemIs1h && !mainhandIs1h)
								found = true;
						}
						else
						{
							foundslot = i;
							found = true;
							upgrade = true;
						}
					}
				}
				else
				{
					foundslot = i;
					found = (item2->Slots == slots) || i == EQ::invslot::slotPrimary;
				}
			}
		}

		bool range_forced = false;
		sitem = GetItem(EQ::invslot::slotRange);
		if (sitem && sitem->forced)
		{
			range_forced = true;
			sitem = nullptr;
		}

		if (foundslot == EQ::invslot::slotPrimary && !range_forced)
		{
			if (mainhandIs1h && itemIs1h && offhandEmpty && GetSpecialAbility(INNATE_DUAL_WIELD))
			{
				foundslot = EQ::invslot::slotSecondary;	// level 66+ NPCs seem to ignore Primary/Secondary restrictions for 1handers. (e.g. elite diakus)  if MH is full, check offhand
				upgrade = false;
			}
			else if (!itemIs2h || (offhandEmpty && !GetSpecialAbility(INNATE_DUAL_WIELD)))
			{
				if (d_melee_texture1 > 0)
				{
					d_melee_texture1 = 0;
					WearChange(EQ::textures::weaponPrimary, 0, 0);
				}

				if (equipment[EQ::invslot::slotRange])
				{
					sitem = GetItem(EQ::invslot::slotRange);
					if (sitem)
					{
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}

				eslot = EQ::textures::weaponPrimary;
				item->equip_slot = EQ::invslot::slotPrimary;
			}
		}

		if (foundslot == EQ::invslot::slotSecondary && !range_forced)
		{
			if (!GetSpecialAbility(INNATE_DUAL_WIELD) || item2->ItemType != EQ::item::ItemTypeShield)
			{
				// some 2handers are erroneously flagged as secondary; don't want to equip these in offhand
				bool itemIs2h = item2->ItemType == EQ::item::ItemType2HBlunt || item2->ItemType == EQ::item::ItemType2HPiercing || item2->ItemType == EQ::item::ItemType2HSlash;

				if (!itemIs2h && (GetSkill(EQ::skills::SkillDualWield) || (item2->ItemType != EQ::item::ItemType1HSlash && item2->ItemType != EQ::item::ItemType1HBlunt && item2->ItemType != EQ::item::ItemType1HPiercing)))
				{
					const EQ::ItemData* mainHand = nullptr;
					if (equipment[EQ::invslot::slotPrimary] > 0)
						mainHand = database.GetItem(equipment[EQ::invslot::slotPrimary]);

					if (!mainHand || (mainHand->ItemType != EQ::item::ItemType2HSlash && mainHand->ItemType != EQ::item::ItemType2HBlunt && mainHand->ItemType != EQ::item::ItemType2HPiercing))
					{
						if (item2->ItemType == EQ::item::ItemTypeShield)
						{
							ShieldEquiped(true);
						}

						if (d_melee_texture2 > 0)
						{
							d_melee_texture2 = 0;
							WearChange(EQ::textures::weaponSecondary, 0, 0);
						}

						if (equipment[EQ::invslot::slotRange])
						{
							sitem = GetItem(EQ::invslot::slotRange);
							if (sitem)
							{
								MoveItemToGeneralInventory(sitem);
								sitem = nullptr;
							}
						}

						eslot = EQ::textures::weaponSecondary;
						item->equip_slot = EQ::invslot::slotSecondary;
					}
				}
			}
		}
		else if (foundslot == EQ::invslot::slotRange)
		{
			if (force_equip)
			{
				if (equipment[EQ::invslot::slotPrimary])
				{
					sitem = GetItem(EQ::invslot::slotPrimary);
					if (sitem)
					{
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}

				if (equipment[EQ::invslot::slotSecondary])
				{
					sitem = GetItem(EQ::invslot::slotSecondary);
					if (sitem)
					{
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}
			}

			eslot = EQ::textures::textureRange;
			item->equip_slot = EQ::invslot::slotRange;
		}
		else if (foundslot == EQ::invslot::slotHead)
		{
			eslot = EQ::textures::armorHead;
			item->equip_slot = EQ::invslot::slotHead;
		}
		else if (foundslot == EQ::invslot::slotChest)
		{
			eslot = EQ::textures::armorChest;
			item->equip_slot = EQ::invslot::slotChest;
		}
		else if (foundslot == EQ::invslot::slotArms)
		{
			eslot = EQ::textures::armorArms;
			item->equip_slot = EQ::invslot::slotArms;
		}
		else if (foundslot == EQ::invslot::slotWrist1 || foundslot == EQ::invslot::slotWrist2)
		{
			foundslot = EQ::invslot::slotWrist1;
			special_slot = true;
			eslot = EQ::textures::armorWrist;
			item->equip_slot = EQ::invslot::slotWrist1;
		}
		else if (foundslot == EQ::invslot::slotHands)
		{
			eslot = EQ::textures::armorHands;
			item->equip_slot = EQ::invslot::slotHands;
		}
		else if (foundslot == EQ::invslot::slotLegs)
		{
			eslot = EQ::textures::armorLegs;
			item->equip_slot = EQ::invslot::slotLegs;
		}
		else if (foundslot == EQ::invslot::slotFeet)
		{
			eslot = EQ::textures::armorFeet;
			item->equip_slot = EQ::invslot::slotFeet;
		}
		else if (foundslot == EQ::invslot::slotEar1 || foundslot == EQ::invslot::slotEar2)
		{
			foundslot = EQ::invslot::slotEar1;
			special_slot = true;
			item->equip_slot = EQ::invslot::slotEar1;
		}
		else if (foundslot == EQ::invslot::slotFinger1 || foundslot == EQ::invslot::slotFinger2)
		{
			foundslot = EQ::invslot::slotFinger1;
			special_slot = true;
			item->equip_slot = EQ::invslot::slotFinger1;
		}
		else if (foundslot == EQ::invslot::slotFace || foundslot == EQ::invslot::slotNeck || foundslot == EQ::invslot::slotShoulders ||
			foundslot == EQ::invslot::slotBack || foundslot == EQ::invslot::slotWaist || foundslot == EQ::invslot::slotAmmo)
		{
			item->equip_slot = foundslot;
		}

		// We found an item, handle unequipping old items and forced column here.
		if (foundslot != INVALID_INDEX && item->equip_slot <= EQ::invslot::EQUIPMENT_COUNT)
		{
			if (upgrade || special_slot)
			{
				sitem = GetItem(foundslot);
				if (sitem)
				{
					MoveItemToGeneralInventory(sitem);
					sitem = nullptr;
				}
			}

			if (force_equip)
			{
				item->forced = true;
			}
		}

		// We found an item, handle wearchange variables here.
		if(eslot != EQ::textures::materialInvalid && wearchange)
		{
			// Don't equip a ranged item if we already have primary or secondary.
			if (eslot == EQ::textures::textureRange && (equipment[EQ::invslot::slotPrimary] || equipment[EQ::invslot::slotSecondary]))
			{
				send_wearchange = false;
			}
			else
			{
				if (eslot == EQ::textures::textureRange)
				{
					eslot = EQ::textures::weaponPrimary;
				}

				uint16 emat = 0;
				if (item2->Material <= 0 ||
					eslot == EQ::textures::weaponPrimary ||
					eslot == EQ::textures::weaponSecondary)
				{
					if (strlen(item2->IDFile) > 2)
					{
						emat = atoi(&item2->IDFile[2]);
					}
				}

				// Hack to force custom crowns to show correctly.
				if (eslot == EQ::textures::armorHead)
				{
					if (strlen(item2->IDFile) > 2)
					{
						uint16 hmat = atoi(&item2->IDFile[2]);
						if (hmat == 240 && item2->Material == TextureCloth)
						{
							emat = hmat;
						}
					}
				}

				if (emat == 0)
				{
					emat = item2->Material;
				}

				wc->wear_slot_id = eslot;
				wc->material = emat;
				wc->color.Color = item2->Color;
				send_wearchange = true;
			}
		}
	}

	if (item->equip_slot <= EQ::invslot::EQUIPMENT_COUNT)
	{
		equipment[item->equip_slot] = item2->ID;
		CalcBonuses();
	}

	if(itemlist != nullptr)
		itemlist->push_back(item);
	else
		safe_delete(item);

	if(wearchange && outapp)
	{
		if (send_wearchange)
		{
			Log(Logs::Detail, Logs::Trading, "%s is sending a wearchange to the zone for slot %d with material %d", GetName(), wc->wear_slot_id, wc->material);
			entity_list.QueueClients(this, outapp);
		}
		safe_delete(outapp);
	}

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
}

void NPC::AddItem(uint32 itemid, int8 charges, bool equipitem, bool quest) {
	//slot isnt needed, its determined from the item.
	const EQ::ItemData * i = database.GetItem(itemid);
	if(i == nullptr)
		return;
	AddLootDrop(i, &itemlist, charges, 0, 255, equipitem, equipitem, quest);
}

void NPC::AddLootTable() {
	if (npctype_id != 0) { // check if it's a GM spawn
		database.AddLootTableToNPC(this,loottable_id, &itemlist, &copper, &silver, &gold, &platinum);
	}
}

void NPC::AddLootTable(uint32 ldid) {
	if (npctype_id != 0) { // check if it's a GM spawn
	  database.AddLootTableToNPC(this,ldid, &itemlist, &copper, &silver, &gold, &platinum);
	}
}

void NPC::CheckGlobalLootTables()
{
	auto tables = zone->GetGlobalLootTables(this);

	for (auto &id : tables)
		database.AddLootTableToNPC(this, id, &itemlist, nullptr, nullptr, nullptr, nullptr);
}

void ZoneDatabase::LoadGlobalLoot()
{
	auto query = fmt::format(
		SQL(
			SELECT
			id,
			loottable_id,
			description,
			min_level,
			max_level,
			rare,
			raid,
			race,
			class,
			bodytype,
			zone
			FROM
			global_loot
			WHERE
			enabled = 1
			{}
		),
		ContentFilterCriteria::apply()
	);

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() == 0) {
		return;
	}

	// we might need this, lets not keep doing it in a loop
	auto zoneid = std::to_string(zone->GetZoneID());
	for (auto row = results.begin(); row != results.end(); ++row) {
		// checking zone limits
		if (row[10]) {
			auto zones = Strings::Split(row[10], '|');

			auto it = std::find(zones.begin(), zones.end(), zoneid);
			if (it == zones.end()) {  // not in here, skip
				continue;
			}
		}

		GlobalLootEntry e(atoi(row[0]), atoi(row[1]), row[2] ? row[2] : "");

		auto min_level = atoi(row[3]);
		if (min_level) {
			e.AddRule(GlobalLoot::RuleTypes::LevelMin, min_level);
		}

		auto max_level = atoi(row[4]);
		if (max_level) {
			e.AddRule(GlobalLoot::RuleTypes::LevelMax, max_level);
		}

		// null is not used
		if (row[5]) {
			e.AddRule(GlobalLoot::RuleTypes::Rare, atoi(row[5]));
		}

		// null is not used
		if (row[6]) {
			e.AddRule(GlobalLoot::RuleTypes::Raid, atoi(row[6]));
		}

		if (row[7]) {
			auto races = Strings::Split(row[7], '|');

			for (auto &r : races)
				e.AddRule(GlobalLoot::RuleTypes::Race, std::stoi(r));
		}

		if (row[8]) {
			auto classes = Strings::Split(row[8], '|');

			for (auto &c : classes)
				e.AddRule(GlobalLoot::RuleTypes::Class, std::stoi(c));
		}

		if (row[9]) {
			auto bodytypes = Strings::Split(row[9], '|');

			for (auto &b : bodytypes)
				e.AddRule(GlobalLoot::RuleTypes::BodyType, std::stoi(b));
		}

		zone->AddGlobalLootEntry(e);
	}
}

// itemid only needs to be passed here if the item is in general inventory (slot 22) since that can contain multiple items.
ServerLootItem_Struct* NPC::GetItem(int slot_id, int16 itemid) 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* item = *cur;
		if (item->equip_slot == slot_id && (itemid == 0 || itemid == item->item_id)) 
		{
			return item;
		}
	}
	return(nullptr);
}

ServerLootItem_Struct* NPC::GetItemByID(int16 itemid) 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* item = *cur;
		if (item->item_id == itemid) {
			return item;
		}
	}
	return(nullptr);
}

bool NPC::HasQuestLootItem(int16 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1 && sitem->item_id == itemid) 
		{
			return true;
		}
	}

	return false;
}

bool NPC::HasPetLootItem(int16 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->pet == 1 && sitem->item_id == itemid)
		{
			return true;
		}
	}

	return false;
}

bool NPC::HasRequiredQuestLoot(int16 itemid1, int16 itemid2, int16 itemid3, int16 itemid4)
{
	if(itemid2 == 0 && itemid3 == 0 && itemid4 == 0)
		return true;

	uint8 item2count = 0, item3count = 0, item4count = 0, item1npc = 0, item2npc = 0, item3npc = 0, item4npc = 0;
	uint8 item1count = 1; 
	if(itemid2 > 0)
		item2count = 1;
	if(itemid3 > 0)
		item3count = 1;
	if(itemid4 > 0)
		item4count = 1;

	if(itemid1 == itemid2 && itemid2 > 0)
	{
		item2count = item1count;
		++item1count;
		++item2count;
	}
	if(itemid1 == itemid3 && itemid3 > 0)
	{
		item3count = item1count;
		++item1count;
		++item3count;
	}
	if(itemid1 == itemid4 && itemid4 > 0)
	{
		item4count = item1count;
		++item1count;
		++item4count;
	}

	if(itemid2 == itemid3  && itemid2 > 0 && itemid3 > 0)
	{
		item3count = item2count;
		++item2count;
		++item3count;
	}
	if(itemid2 == itemid4  && itemid2 > 0 && itemid4 > 0)
	{
		item4count = item2count;
		++item2count;
		++item4count;
	}

	if(itemid3 == itemid4 && itemid3 > 0 && itemid4 > 0)
	{
		item4count = item3count;
		++item3count;
		++item4count;
	}

	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1) 
		{
			if(sitem->item_id == itemid1)
				++item1npc;

			if(sitem->item_id == itemid2 && itemid2 > 0)
				++item2npc;

			if(sitem->item_id == itemid3 && itemid3 > 0)
				++item3npc;

			if(sitem->item_id == itemid4 && itemid4 > 0)
				++item4npc;
		}
	}

	if(item1npc < item1count)
	{
		return false;
	}

	if(itemid2 > 0 && item2npc < item2count)
		return false;

	if(itemid3 > 0 && item3npc < item3count)
		return false;

	if(itemid4 > 0 && item4npc < item4count)
		return false;

	return true;
}

bool NPC::HasQuestLoot() 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1) 
		{
			return true;
		}
	}

	return false;
}

void NPC::CleanQuestLootItems() 
{
	//Removes nodrop or multiple quest loot items from a NPC before sending the corpse items to the client.

	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && (sitem->quest == 1 || sitem->pet == 1))
		{
			uint8 count = CountQuestItem(sitem->item_id);
			if(count > 1 && sitem->pet != 1)
			{
				RemoveItem(sitem);
				return;
			}
			else
			{
				const EQ::ItemData* item = database.GetItem(sitem->item_id);
				if(item && item->NoDrop == 0)
				{
					RemoveItem(sitem);
					return;
				}
			}
		}
	}
}

uint8 NPC::CountQuestItem(uint16 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for(; cur != end; ++cur) 
	{
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->item_id == itemid)
		{
			++count;
		}
	}

	return count;
}

uint8 NPC::CountQuestItems()
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for(; cur != end; ++cur) 
	{
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1)
		{
			++count;
		}
	}

	return count;
}

bool NPC::RemoveQuestLootItems(int16 itemid) 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1) {
			if(itemid == 0 || itemid == sitem->item_id)
			{
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

bool NPC::RemovePetLootItems(int16 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->pet == 1) {
			if (itemid == 0 || itemid == sitem->item_id)
			{
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

bool NPC::MoveItemToGeneralInventory(ServerLootItem_Struct* weapon)
{

	if (!weapon)
		return false;

	uint16 weaponid = weapon->item_id;
	int16 slot = weapon->equip_slot;
	int8 charges = weapon->charges;
	bool pet = weapon->pet;
	bool quest = weapon->quest;
	uint8 min_level = weapon->min_level;
	uint8 max_level = weapon->max_level;

	const EQ::ItemData* item = database.GetItem(weaponid);

	if (item)
	{
		RemoveItem(weapon);

		Log(Logs::Detail, Logs::Trading, "%s is moving %d in slot %d to general inventory. quantity: %d", GetCleanName(), weaponid, slot, charges);
		AddLootDrop(item, &itemlist, charges, min_level, max_level, false, false, quest, pet, false, weapon->min_looter_level, weapon->item_loot_lockout_timer);

		return true;
	}

	return false;
}

void NPC::RemoveItem(ServerLootItem_Struct* item_data, uint8 quantity)
{

	if (!item_data)
	{
		return;
	}

	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter) 
	{
		ServerLootItem_Struct* sitem = *iter;
		if (sitem != item_data) { continue; }

		if (!sitem)
			return;

		int16 eslot = sitem->equip_slot;
		if (sitem->charges <= 1 || quantity == 0)
		{
			DeleteEquipment(eslot);

			Log(Logs::General, Logs::Trading, "%s is deleting item %d from slot %d", GetName(), sitem->item_id, eslot);
			itemlist.erase(iter);

			UpdateEquipmentLight();
			if (UpdateActiveLight())
				SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
		}
		else
		{
			sitem->charges -= quantity;
			Log(Logs::General, Logs::Trading, "%s is deleting %d charges from item %d in slot %d", GetName(), quantity, sitem->item_id, eslot);
		}

		return;
	}
}

void NPC::CheckMinMaxLevel(Mob *them)
{
	if (them == nullptr || !them->IsClient())
		return;

	uint16 themlevel = them->GetLevel();

	for (auto iter = itemlist.begin(); iter != itemlist.end();)
	{
		if (!(*iter))
			return;

		if (themlevel < (*iter)->min_level || themlevel >(*iter)->max_level)
		{
			int16 eslot = (*iter)->equip_slot;
			DeleteEquipment(eslot);
			iter = itemlist.erase(iter);
			continue;
		}

		++iter;
	}

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
}

void NPC::ClearItemList()
{
	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter)
	{
		if (!(*iter))
			return;

		int16 eslot = (*iter)->equip_slot;
		DeleteEquipment(eslot);
	}

	itemlist.clear();

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
}

void NPC::DeleteEquipment(int16 slotid)
{
	if (slotid <= EQ::invslot::EQUIPMENT_COUNT)
	{
		equipment[slotid] = 0;
		uint8 material = EQ::InventoryProfile::CalcMaterialFromSlot(slotid);

		if (slotid == EQ::invslot::slotRange && material == EQ::textures::materialInvalid)
		{
			material = EQ::textures::weaponPrimary;
		}

		if (material != EQ::textures::materialInvalid)
		{
			WearChange(material, 0, 0);
			if (!equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange])
			{
				SendWearChange(EQ::textures::weaponPrimary);
			}
		}

		CalcBonuses();
	}
}

void NPC::QueryLoot(Client* to)
{
	if (itemlist.size() > 0) {
		to->Message(
			CC_Default,
			fmt::format(
				"Loot | Name: {} ID: {} Loottable ID: {}",
				GetName(),
				GetNPCTypeID(),
				GetLoottableID()
			).c_str()
		);

		int item_count = 0;
		for (auto current_item : itemlist) {
			int item_number = (item_count + 1);
			if (!current_item) {
				LogError("NPC::QueryLoot() - ItemList error, null item.");
				continue;
			}

			if (!current_item->item_id || !database.GetItem(current_item->item_id)) {
				LogError("NPC::QueryLoot() - Database error, invalid item.");
				continue;
			}

			EQ::SayLinkEngine linker;
			linker.SetLinkType(EQ::saylink::SayLinkLootItem);
			linker.SetLootData(current_item);

			to->Message(
				CC_Default,
				fmt::format(
					"Item {} | Name: {} ({}){}",
					item_number,
					linker.GenerateLink().c_str(),
					current_item->item_id,
					(
						current_item->charges > 1 ?
						fmt::format(
							" Amount: {}",
							current_item->charges
						) :
						""
						)
				).c_str()
			);
			item_count++;
		}
	}
	else
	{
		to->Message(CC_Default, "NPC::QueryLoot() - Does not have any item loot");
	}

	bool has_money = (
		platinum > 0 ||
		gold > 0 ||
		silver > 0 ||
		copper > 0
		);
	if (has_money) {
		to->Message(
			CC_Default,
			fmt::format(
				"Money | {}",
				Strings::Money(
					platinum,
					gold,
					silver,
					copper
				)
			).c_str()
		);
	}
}

void NPC::AddCash(uint16 in_copper, uint16 in_silver, uint16 in_gold, uint16 in_platinum) {
	if (in_copper >= 0)
		copper = in_copper;
	else
		copper = 0;

	if (in_silver >= 0)
		silver = in_silver;
	else
		silver = 0;

	if (in_gold >= 0)
		gold = in_gold;
	else
		gold = 0;

	if (in_platinum >= 0)
		platinum = in_platinum;
	else
		platinum = 0;
}

void NPC::AddCash() {
	copper = zone->random.Int(1, 100);
	silver = zone->random.Int(1, 50);
	gold = zone->random.Int(1, 10);
	platinum = zone->random.Int(1, 5);
}

void NPC::RemoveCash() {
	copper = 0;
	silver = 0;
	gold = 0;
	platinum = 0;
}

bool NPC::AddQuestLoot(int16 itemid, int8 charges)
{
	const EQ::ItemData* item = database.GetItem(itemid);
	if (item)
	{
		AddLootDrop(item, &itemlist, charges, 0, 255, false, false, true);
		Log(Logs::General, Logs::Trading, "Adding item %d to the NPC's loot marked as quest.", itemid);
		if (itemid > 0 && HasPetLootItem(itemid))
		{
			Log(Logs::General, Logs::Trading, "Deleting quest item %d from NPC's pet loot.", itemid);
			RemovePetLootItems(itemid);
		}
	}
	else
		return false;

	return true;
}

bool NPC::AddPetLoot(int16 itemid, int8 charges, bool fromquest)
{
	const EQ::ItemData* item = database.GetItem(itemid);

	if (!item)
		return false;

	bool valid = (item->NoDrop != 0 && (!IsCharmedPet() || (IsCharmedPet() && CountQuestItem(item->ID) == 0)));
	if (!fromquest || valid)
	{
		if (item)
		{
			AddLootDrop(item, &itemlist, charges, 0, 255, true, true, false, true);
			Log(Logs::General, Logs::Trading, "Adding item %d to the NPC's loot marked as pet.", itemid);
			return true;
		}
	}
	else
	{
		Log(Logs::General, Logs::Trading, "Item %d is a duplicate or no drop. Deleting...", itemid);
		return false;
	}

	return false;
}

void NPC::DeleteQuestLoot(int16 itemid1, int16 itemid2, int16 itemid3, int16 itemid4)
{
	int16 items = itemlist.size();
	for (int i = 0; i < items; ++i)
	{
		if (itemid1 == 0)
		{
			if (!RemoveQuestLootItems(itemid1))
				break;
		}
		else
		{
			if (itemid1 != 0)
			{
				RemoveQuestLootItems(itemid1);
			}
			if (itemid2 != 0)
			{
				RemoveQuestLootItems(itemid2);
			}
			if (itemid3 != 0)
			{
				RemoveQuestLootItems(itemid3);
			}
			if (itemid4 != 0)
			{
				RemoveQuestLootItems(itemid4);
			}
		}
	}
}

void NPC::DeleteInvalidQuestLoot()
{
	int16 items = itemlist.size();
	for (int i = 0; i < items; ++i)
	{
		CleanQuestLootItems();
	}
}

uint32 NPC::GetEquipment(uint8 material_slot) const
{
	if (material_slot > 8)
		return 0;
	int invslot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);

	if (material_slot == EQ::textures::weaponPrimary && !equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange])
	{
		invslot = EQ::invslot::slotRange;
	}

	if (invslot == INVALID_INDEX)
		return 0;
	return equipment[invslot];
}

int32 NPC::GetEquipmentMaterial(uint8 material_slot) const
{
	if (material_slot >= EQ::textures::materialCount)
		return 0;

	int inv_slot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);

	if (material_slot == EQ::textures::weaponPrimary && !equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange])
	{
		inv_slot = EQ::invslot::slotRange;
	}

	if (inv_slot == -1)
		return 0;
	if (equipment[inv_slot] == 0) {
		switch (material_slot) {
		case EQ::textures::armorHead:
			return helmtexture;
		case EQ::textures::armorChest:
			return chesttexture;
		case EQ::textures::armorArms:
			return armtexture;
		case EQ::textures::armorWrist:
			return bracertexture;
		case EQ::textures::armorHands:
			return handtexture;
		case EQ::textures::armorLegs:
			return legtexture;
		case EQ::textures::armorFeet:
			return feettexture;
		case EQ::textures::weaponPrimary:
			return d_melee_texture1;
		case EQ::textures::weaponSecondary:
			return d_melee_texture2;
		default:
			//they have nothing in the slot, and its not a special slot... they get nothing.
			return(0);
		}
	}

	//they have some loot item in this slot, pass it up to the default handler
	return(Mob::GetEquipmentMaterial(material_slot));
}