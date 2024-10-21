#include "../common/global_define.h"
#include "../common/data_verification.h"

#include "../common/loot.h"
#include "client.h"
#include "entity.h"
#include "mob.h"
#include "npc.h"
#include "zonedb.h"
#include "global_loot_manager.h"
#include "../common/repositories/criteria/content_filter_criteria.h"
#include "../common/repositories/global_loot_repository.h"

#include <iostream>
#include <stdlib.h>
#include <vector>

#ifdef _WINDOWS
#define snprintf	_snprintf
#endif

void NPC::AddLootTable(uint32 loottable_id, bool is_global) 
{
	// check if it's a GM spawn
	if (!npctype_id) {
		return;
	}

	if (!is_global) {
		m_loot_copper   = 0;
		m_loot_silver   = 0;
		m_loot_gold     = 0;
		m_loot_platinum = 0;
	}

	zone->LoadLootTable(loottable_id);

	const auto *l = zone->GetLootTable(loottable_id);
	if (!l) {
		return;
	}

	LogLootDetail(
		"Attempting to load loot [{}] loottable [{}] ({}) is_global [{}]",
		GetCleanName(),
		loottable_id,
		l->name,
		is_global
	);

	auto content_flags = ContentFlags{
		.min_expansion = l->min_expansion,
		.max_expansion = l->max_expansion,
		.content_flags = l->content_flags,
		.content_flags_disabled = l->content_flags_disabled
	};

	if (!content_service.DoesPassContentFiltering(content_flags)) {
		return;
	}

	uint32 min_cash = l->mincash;
	uint32 max_cash = l->maxcash;
	if (min_cash > max_cash) {
		const uint32 t = min_cash;
		min_cash = max_cash;
		max_cash = t;
	}

	uint32 cash = 0;
	if (!is_global) {
		if (max_cash > 0 && l->avgcoin > 0 && EQ::ValueWithin(l->avgcoin, min_cash, max_cash)) {
			float upper_chance = (float)(l->avgcoin - min_cash) / (float)(max_cash - min_cash);
			float avg_cash_roll = (float)zone->random.Real(0.0, 1.0);

			if (avg_cash_roll < upper_chance) {
				cash = zone->random.Int(l->avgcoin, max_cash);
			}
			else {
				cash = zone->random.Int(min_cash, l->avgcoin);
			}
		}
		else {
			cash = zone->random.Int(min_cash, max_cash);
		}
	}

	if (cash != 0) {
		m_loot_platinum = cash / 1000;
		cash -= m_loot_platinum * 1000;

		m_loot_gold = cash / 100;
		cash -= m_loot_gold * 100;

		m_loot_silver = cash / 10;
		cash -= m_loot_silver * 10;

		m_loot_copper = cash;
	}

	const uint32 global_loot_multiplier = RuleI(Zone, GlobalLootMultiplier);
	for (auto &lte: zone->GetLootTableEntries(loottable_id)) {
		uint8 multiplier_count = 0;
		for (uint32 k = 1; k <= lte.multiplier * global_loot_multiplier; k++) {
			const uint8 drop_limit = lte.droplimit;
			const uint8 minimum_drop = lte.mindrop;
			const uint8 multiplier_min = lte.multiplier_min;
			const float probability = lte.probability;

			float drop_chance = 0.0f;
			if (EQ::ValueWithin(probability, 0.0f, 100.0f) && multiplier_count >= multiplier_min) {
				drop_chance = static_cast<float>(zone->random.Real(0.0, 100.0));
			}
			else if (multiplier_count < multiplier_min)
			{
				drop_chance = 0.0f;
			}

			if (probability != 0.0 && (probability == 100.0 || drop_chance <= probability)) {
				AddLootDropTable(lte.lootdrop_id, drop_limit, minimum_drop);
			}

			++multiplier_count;
		}
	}

	LogLootDetail(
		"Loaded [{}] Loot Table [{}] is_global [{}]",
		GetCleanName(),
		loottable_id,
		is_global
	);
}

void NPC::AddLootDropTable(uint32 lootdrop_id, uint8 drop_limit, uint8 min_drop)
{
	const auto l = zone->GetLootdrop(lootdrop_id);
	const auto le = zone->GetLootdropEntries(lootdrop_id);

	if (l.id == 0 || le.empty()) {
		return;
	}

	// if this lootdrop is droplimit=0 and mindrop 0, scan list once and return
	if (drop_limit == 0 && min_drop == 0) {
		for (const auto &e : le) {
			LogLootDetail(
				"(First Pass) NPC [{}] Lootdrop [{}] Item [{}] ({}_ Chance [{}] Multiplier [{}]",
				GetCleanName(),
				lootdrop_id,
				database.GetItem(e.item_id) ? database.GetItem(e.item_id)->Name : "Unknown",
				e.item_id,
				e.chance,
				e.multiplier
			);
			for (int j = 0; j < e.multiplier; ++j) {
				if (zone->random.Real(0.0, 100.0) <= e.chance) {
					auto loot_drop_entry = LootdropEntriesRepository::NewNpcEntity();
					int8 charges = e.item_charges;
					const EQ::ItemData *database_item = database.GetItem(e.item_id);
					if (database.ItemQuantityType(e.item_id) == EQ::item::Quantity_Charges) {
						if (charges <= 2) {
							charges = database_item->MaxCharges;
						}
					}
					bool equipitem = e.equip_item > 0 ? true : false;
					bool force_equip = e.equip_item == 2 ? true : false;
					loot_drop_entry = e;
					loot_drop_entry.item_charges = charges;
					AddLootDrop(database_item, loot_drop_entry, equipitem, false, false, false, force_equip);
				}
			}
		}
		return;
	}

	if (le.size() > 100 && drop_limit == 0) {
		drop_limit = 10;
	}

	if (drop_limit < min_drop) {
		drop_limit = min_drop;
	}

	float roll_t = 0.0f;
	float no_loot_prob = 1.0f;
	bool  roll_table_chance_bypass = false;
	bool active_item_list = false;

	for (const auto &e : le) {
		const EQ::ItemData *db_item = database.GetItem(e.item_id);

		if (db_item) {
			roll_t += e.chance;
			if (e.chance >= 100) {
				roll_table_chance_bypass = true;
			}
			else {
				no_loot_prob *= (100 - e.chance) / 100.0f;
			}

			active_item_list = true;
		}
	}

	if (!active_item_list) {
		return;
	}

	// This will pick one item per iteration until mindrop.
	// Don't let the compare against chance fool you.
	// The roll isn't 0-100, its 0-total and it picks the item, we're just
	// looping to find the lucky item, descremening otherwise. This is ok,
	// items with chance 60 are 6 times more likely than items chance 10.
	int drops = 0;

	// translate above for loop using l and le
	for (int i = 0; i < drop_limit; ++i) {
		if (drops < min_drop || roll_table_chance_bypass || (float)zone->random.Real(0.0, 1.0) >= no_loot_prob) {
			float roll = (float)zone->random.Real(0.0, roll_t);
			for (const auto& e : le) {
				LogLootDetail(
					"(Second Pass) NPC [{}] Lootdrop [{}] Item [{}] ({}_ Chance [{}] Multiplier [{}]",
					GetCleanName(),
					lootdrop_id,
					database.GetItem(e.item_id) ? database.GetItem(e.item_id)->Name : "Unknown",
					e.item_id,
					e.chance,
					e.multiplier
				);
				auto loot_drop_entry = LootdropEntriesRepository::NewNpcEntity();
				int8 charges = e.item_charges;
				const EQ::ItemData* db_item = database.GetItem(e.item_id);
				if (database.ItemQuantityType(e.item_id) == EQ::item::Quantity_Charges) {
					if (charges <= 2) {
						charges = db_item->MaxCharges;
					}
				}

				if (db_item) {
					if (roll < e.chance) {
						bool equipitem = e.equip_item > 0 ? true : false;
						bool force_equip = e.equip_item == 2 ? true : false;
						loot_drop_entry = e;
						loot_drop_entry.item_charges = charges;
						AddLootDrop(db_item, loot_drop_entry, equipitem, false, false, false, force_equip);
						drops++;

						uint8 multiplier = e.multiplier;
						multiplier = EQ::ClampLower(multiplier, static_cast<uint8>(1));

						for (int k = 1; k < multiplier; ++k) {
							float c_roll = static_cast<float>(zone->random.Real(0.0, 100.0));
							if (c_roll <= e.chance) {
								bool equipitem = e.equip_item > 0 ? true : false;
								bool force_equip = e.equip_item == 2;
								AddLootDrop(db_item, loot_drop_entry, equipitem, false, false, false, force_equip);
							}
						}
						break;
					}
					else {
						roll -= e.chance;
					}
				}
			}
		}
	}

	UpdateEquipmentLight();
}

//if itemlist is null, just send wear changes
void NPC::AddLootDrop(
	const EQ::ItemData *item2, 
	LootdropEntriesRepository::LootdropEntries loot_drop, 
	bool equipit, 
	bool wearchange, 
	bool quest, 
	bool pet, 
	bool force_equip,
	const EQ::ItemCustomData& item_custom_data)
{
	if (!item2) {
		return;
	}

	if (CountQuestItems() >= MAX_NPC_QUEST_INVENTORY) {
		return;
	}

	if (LogSys.log_settings[Logs::Loot].is_category_enabled == 1) {
		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemData);
		linker.SetItemData(item2);

		LogLoot(
			"NPC [{}] Item ({}) [{}] charges [{}] chance [{}] minimum level [{}] maximum level [{}]",
			GetName(),
			item2->ID,
			linker.GenerateLink(),
			loot_drop.item_charges,
			loot_drop.chance,
			loot_drop.minlevel,
			loot_drop.maxlevel
		);
	}

	if (quest || pet) {
		uint32 self_found_character_id = EQ::ItemInstance::GetSelfFoundCharacterID(item_custom_data);
		LogLoot("Adding {} to npc: {}. Wearchange: {} Equipit: {} Multiquest: {} Pet: {} SsfOwner: {}", item2->Name, GetName(), wearchange, equipit, quest, pet, self_found_character_id);
	}

	EQApplicationPacket* outapp = nullptr;
	WearChange_Struct*p_wear_change_struct = nullptr;
	if(wearchange) {
		outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
		p_wear_change_struct = (WearChange_Struct*)outapp->pBuffer;
		p_wear_change_struct->spawn_id = GetID();
		p_wear_change_struct->material = 0;
	}

	std::shared_ptr<LootItem> item = std::make_shared<LootItem>();
	item->item_id = item2->ID;
	item->charges = loot_drop.item_charges;
	item->min_level = loot_drop.minlevel;
	item->max_level = loot_drop.maxlevel;
	item->quest = quest;
	item->equip_slot = EQ::invslot::slotGeneral1; //Set default slot to general inventory. NPCs can have multiple items in the same slot.
	item->pet = pet;
	item->forced = false;
	item->item_loot_lockout_timer = loot_drop.item_loot_lockout_timer;
	item->min_looter_level = loot_drop.min_looter_level;
	item->custom_data = item_custom_data; // map copy

	// unsure if required to equip, YOLO for now
	if (item2->ItemType == EQ::item::ItemTypeBow) {
		SetBowEquipped(true);
	}

	if (item2->ItemType == EQ::item::ItemTypeArrow) {
		SetArrowEquipped(true);
	}

	if(pet && quest) {
		LogLoot("Error: Item {} is being added to {} as both a pet and a quest.", item2->Name, GetName());
		item->pet = 0;
	}

	bool  found = false; // track if we found an empty slot we fit into
	int   found_slot = INVALID_INDEX; // for multi-slot items
	bool  send_wearchange = false;

	auto *inst = database.CreateItem(
		item2->ID,
		loot_drop.item_charges,
		item_custom_data
	);

	if (!inst && !wearchange) {
		return;
	}

	uint8 equipment_slot = UINT8_MAX;
	const EQ::ItemData *compitem = nullptr;

	if (equipment[EQ::invslot::slotPrimary] > 0) {
		compitem = database.GetItem(equipment[EQ::invslot::slotPrimary]);
	}

	if (item2->ID == 11543 && IsPet()) { // Sony hardcoded pets to not equip A Weighted Axe
		equipit = false;
	}

	bool mainhandIs1h = compitem && compitem->IsType1HWeapon();
	bool itemIs2h = item2->IsType2HWeapon();
	bool itemIs1h = item2->IsType1HWeapon();
	bool offhandEmpty = GetEquipment(EQ::textures::weaponSecondary) == 0 ? true : false;

	if (equipit && item2->IsClassCommon() && !GetSpecialAbility(DO_NOT_EQUIP)) {
		bool upgrade = false;
		bool special_slot = false; // Ear, Ring, and Wrist only allows 1 item for NPCs.
		
		// Equip rules are as follows:
		// If the item has the NoPet flag set it will not be equipped.
		// An empty slot takes priority. The first empty one that an item can
		// fit into will be the one picked for the item.
		// AC is the primary choice for which item gets picked for a slot.
		// If AC is identical HP is considered next.
		// If an item can fit into multiple slots we'll pick the last one where
		// it is an improvement.

		for (int i = 0; !found && i <= EQ::invslot::EQUIPMENT_END; i++) {
			const uint32 slots = (1 << i);
				
			// If the item is ranged, and has other equip slots prefer to use another slot so
			// the weapon graphic shows in-game.
			if (i == EQ::invslot::slotRange && item2->Slots != slots) {
				continue;
			}
				
			if (item2->Slots & slots) {
				if (equipment[i]) {
					auto sitem = GetItem(i);
					if (sitem && sitem->forced)	{
						// Existing item is forced. Try another slot.
						sitem = nullptr;
						continue;
					}

					if (item2->ItemType == EQ::item::ItemTypeShield) {	// prevent shields from replacing weapons
						break;
					}

					compitem = database.GetItem(equipment[i]);

					if (!compitem) {
						continue;
					}

					if ((item->pet == 0 && (i != EQ::invslot::slotPrimary && i != EQ::invslot::slotSecondary && i != EQ::invslot::slotRange && (item2->AC > compitem->AC || (item2->AC == compitem->AC && item2->HP > compitem->HP))))
						|| (i == EQ::invslot::slotPrimary || i == EQ::invslot::slotSecondary || i == EQ::invslot::slotRange) || force_equip
						) {
						// item would be an upgrade
						// check if we're multi-slot, if yes then we have to keep
						// looking in case any of the other slots we can fit into are empty.
						if (item2->Slots != slots) {
							found_slot = i;
							upgrade = true;
							if (itemIs1h && !mainhandIs1h) {
								found = true;
							}
						}
						else {
							found_slot = i;
							found = true;
							upgrade = true;
						}
					}
				}
				else {
					found_slot = i;
					found = (item2->Slots == slots) || i == EQ::invslot::slotPrimary;
				}
			}
		}

		bool range_forced = false;
		LootItem* sitem = GetItem(EQ::invslot::slotRange);
		if (sitem && sitem->forced)	{
			range_forced = true;
			sitem = nullptr;
		}

		if (found_slot == EQ::invslot::slotPrimary && !range_forced) {
			if (mainhandIs1h && itemIs1h && offhandEmpty && GetSpecialAbility(INNATE_DUAL_WIELD)) {
				found_slot = EQ::invslot::slotSecondary;	// level 66+ NPCs seem to ignore Primary/Secondary restrictions for 1handers. (e.g. elite diakus)  if MH is full, check offhand
				upgrade = false;
			}
			else if (!itemIs2h || (offhandEmpty && !GetSpecialAbility(INNATE_DUAL_WIELD)))
			{
				if (d_melee_texture1 > 0) {
					d_melee_texture1 = 0;
					WearChange(EQ::textures::weaponPrimary, 0, 0);
				}

				if (equipment[EQ::invslot::slotRange]) {
					sitem = GetItem(EQ::invslot::slotRange);
					if (sitem) {
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}

				equipment_slot = EQ::textures::weaponPrimary;
				item->equip_slot = EQ::invslot::slotPrimary;
			}
		}

		else if (found_slot == EQ::invslot::slotSecondary && !range_forced) {
			if (!GetSpecialAbility(INNATE_DUAL_WIELD) || !item2->IsTypeShield()) {
				// some 2handers are erroneously flagged as secondary; don't want to equip these in offhand
				bool itemIs2h = item2->IsType2HWeapon();
				if (!itemIs2h && (GetSkill(EQ::skills::SkillDualWield) || (!item2->IsType1HWeapon())))
				{
					const EQ::ItemData *mainHand = nullptr;
					if (equipment[EQ::invslot::slotPrimary] > 0)
						mainHand = database.GetItem(equipment[EQ::invslot::slotPrimary]);

					if (!mainHand || !mainHand->IsType2HWeapon()) {
						if (item2->IsTypeShield()) {
							ShieldEquiped(true);
						}

						if (d_melee_texture2 > 0) {
							d_melee_texture2 = 0;
							WearChange(EQ::textures::weaponSecondary, 0, 0);
						}

						if (equipment[EQ::invslot::slotRange]) {
							sitem = GetItem(EQ::invslot::slotRange);
							if (sitem) {
								MoveItemToGeneralInventory(sitem);
								sitem = nullptr;
							}
						}

						equipment_slot = EQ::textures::weaponSecondary;
						item->equip_slot = EQ::invslot::slotSecondary;
					}
				}
			}
		}

		else if (found_slot == EQ::invslot::slotRange) {
			if (force_equip) {
				if (equipment[EQ::invslot::slotPrimary]) {
					sitem = GetItem(EQ::invslot::slotPrimary);
					if (sitem) {
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}

				if (equipment[EQ::invslot::slotSecondary]) {
					sitem = GetItem(EQ::invslot::slotSecondary);
					if (sitem) {
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}
			}

			equipment_slot = EQ::textures::textureRange;
			item->equip_slot = EQ::invslot::slotRange;
		}

		else if (found_slot == EQ::invslot::slotHead) {
			equipment_slot = EQ::textures::armorHead;
			item->equip_slot = EQ::invslot::slotHead;
		}
		else if (found_slot == EQ::invslot::slotChest) {
			equipment_slot = EQ::textures::armorChest;
			item->equip_slot = EQ::invslot::slotChest;
		}
		else if (found_slot == EQ::invslot::slotArms) {
			equipment_slot = EQ::textures::armorArms;
			item->equip_slot = EQ::invslot::slotArms;
		}
		else if (EQ::ValueWithin(found_slot, EQ::invslot::slotWrist1, EQ::invslot::slotWrist2)) {
			found_slot = EQ::invslot::slotWrist1;
			special_slot = true;
			equipment_slot = EQ::textures::armorWrist;
			item->equip_slot = EQ::invslot::slotWrist1;
		}
		else if (found_slot == EQ::invslot::slotHands) {
			equipment_slot = EQ::textures::armorHands;
			item->equip_slot = EQ::invslot::slotHands;
		}
		else if (found_slot == EQ::invslot::slotLegs) {
			equipment_slot = EQ::textures::armorLegs;
			item->equip_slot = EQ::invslot::slotLegs;
		}
		else if (found_slot == EQ::invslot::slotFeet) {
			equipment_slot = EQ::textures::armorFeet;
			item->equip_slot = EQ::invslot::slotFeet;
		}

		else if (found_slot == EQ::invslot::slotEar1 || found_slot == EQ::invslot::slotEar2) {
			found_slot = EQ::invslot::slotEar1;
			special_slot = true;
			item->equip_slot = EQ::invslot::slotEar1;
		}
		else if (found_slot == EQ::invslot::slotFinger1 || found_slot == EQ::invslot::slotFinger2) {
			found_slot = EQ::invslot::slotFinger1;
			special_slot = true;
			item->equip_slot = EQ::invslot::slotFinger1;
		}
		else if (found_slot == EQ::invslot::slotFace || found_slot == EQ::invslot::slotNeck || found_slot == EQ::invslot::slotShoulders ||
		found_slot == EQ::invslot::slotBack || found_slot == EQ::invslot::slotWaist || found_slot == EQ::invslot::slotAmmo) {
			item->equip_slot = found_slot;
		}

		if (found_slot != INVALID_INDEX && item->equip_slot <= EQ::invslot::EQUIPMENT_COUNT) {
			if (upgrade || special_slot) {
				sitem = GetItem(found_slot);
				if (sitem) {
					MoveItemToGeneralInventory(sitem);
					sitem = nullptr;
				}
			}

			if (force_equip) {
				item->forced = true;
			}
		}

		uint16 equipment_material = 0;
		if (equipment_slot != EQ::textures::materialInvalid && wearchange) {
			// Don't equip a ranged item if we already have primary or secondary.
			if (equipment_slot == EQ::textures::textureRange && (equipment[EQ::invslot::slotPrimary] || equipment[EQ::invslot::slotSecondary])) {
				send_wearchange = false;
			}
			else {
				if (equipment_slot == EQ::textures::textureRange) {
					equipment_slot = EQ::textures::weaponPrimary;
				}
					
				if (item2->Material <= 0 ||
					equipment_slot == EQ::textures::weaponPrimary ||
					equipment_slot == EQ::textures::weaponSecondary) {
					if (strlen(item2->IDFile) > 2) {
						equipment_material = atoi(&item2->IDFile[2]);
					}
				}
					
				// Hack to force custom crowns to show correctly.
				if (equipment_slot == EQ::textures::armorHead) {
					if (strlen(item2->IDFile) > 2) {
						uint16 customcrown = atoi(&item2->IDFile[2]);
						if (customcrown == 240 && item2->Material == TextureCloth) {
							equipment_material = customcrown;
						}
					}
				}

				if (equipment_material == 0) {
					equipment_material = item2->Material;
				}
			}


			p_wear_change_struct->wear_slot_id = equipment_slot;
			p_wear_change_struct->material = equipment_material;
			p_wear_change_struct->color.Color = item2->Color;
			send_wearchange = true;
		}
	}

	if (item->equip_slot <= EQ::invslot::EQUIPMENT_COUNT) {
		equipment[item->equip_slot] = item2->ID;
		CalcBonuses();
	}

	m_loot_items.push_back(item);


	if (wearchange && outapp) {
		entity_list.QueueClients(this, outapp);
		safe_delete(outapp);
	}

	UpdateEquipmentLight();

	if (UpdateActiveLight()) {
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
	}

	safe_delete(inst);
}

void NPC::AddItem(const EQ::ItemData *item, int8 charges, bool equipitem, bool quest, bool pet, bool force_equip, uint8 min_looter_level, uint32 item_loot_lockout_timer)
{
	auto l = LootdropEntriesRepository::NewNpcEntity();

	l.equip_item = static_cast<uint8>(equipitem ? 1 : 0);
	l.item_charges = charges;

	AddLootDrop(item, l, equipitem, equipitem, quest, pet, force_equip);
}

void NPC::AddItem(uint32 itemid, int8 charges, bool equipitem, bool quest) {
	//slot isnt needed, its determined from the item.
	const EQ::ItemData * item = database.GetItem(itemid);


	if (!item) {
		return;
	}

	auto l = LootdropEntriesRepository::NewNpcEntity();

	l.equip_item = static_cast<uint8>(equipitem ? 1 : 0);
	l.item_charges = charges;

	AddLootDrop(item, l, equipitem, equipitem, quest);
}

void NPC::AddLootTable() {
	AddLootTable(m_loottable_id);
}

void NPC::CheckGlobalLootTables()
{
	const auto &l = zone->GetGlobalLootTables(this);
	for (const auto &e : l) {
		AddLootTable(e, true);
	}
}

void ZoneDatabase::LoadGlobalLoot()
{
	const auto &l = GlobalLootRepository::GetWhere(
		*this,
		fmt::format(
			"`enabled` = 1 {}",
			ContentFilterCriteria::apply()
		)
	);

	if (l.empty()) {
		return;
	}

	LogInfo(
		"Loaded [{}] Global Loot Entr{}.",
		Strings::Commify(l.size()),
		l.size() != 1 ? "ies" : "y"
	);

	const std::string &zone_id = std::to_string(zone->GetZoneID());

	for (const auto &e : l) {
		if (!e.zone.empty()) {
			const auto &zones = Strings::Split(e.zone, "|");

			if (!Strings::Contains(zones, zone_id)) {
				continue;
			}
		}

		GlobalLootEntry gle(e.id, e.loottable_id, e.description);

		if (e.min_level) {
			gle.AddRule(GlobalLoot::RuleTypes::LevelMin, e.min_level);
		}

		if (e.max_level) {
			gle.AddRule(GlobalLoot::RuleTypes::LevelMax, e.max_level);
		}

		if (e.rare) {
			gle.AddRule(GlobalLoot::RuleTypes::Rare, e.rare);
		}

		if (e.raid) {
			gle.AddRule(GlobalLoot::RuleTypes::Raid, e.raid);
		}

		if (!e.race.empty()) {
			const auto &races = Strings::Split(e.race, "|");

			for (const auto &r : races) {
				gle.AddRule(GlobalLoot::RuleTypes::Race, Strings::ToInt(r));
			}
		}

		if (!e.class_.empty()) {
			const auto &classes = Strings::Split(e.class_, "|");

			for (const auto &c : classes) {
				gle.AddRule(GlobalLoot::RuleTypes::Class, Strings::ToInt(c));
			}
		}

		if (!e.bodytype.empty()) {
			const auto &bodytypes = Strings::Split(e.bodytype, "|");

			for (const auto &b : bodytypes) {
				gle.AddRule(GlobalLoot::RuleTypes::BodyType, Strings::ToInt(b));
			}
		}

		zone->AddGlobalLootEntry(gle);
	}
}

// itemid only needs to be passed here if the item is in general inventory (slot 22) since that can contain multiple items.
LootItem* NPC::GetItem(int slot_id, int16 itemid) 
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for (; cur != end; ++cur) {
		LootItem* item = cur->get();
		if (item->equip_slot == slot_id && (itemid == 0 || itemid == item->item_id)) {
			return item;
		}
	}
	return(nullptr);
}

LootItem* NPC::GetItemByID(int16 itemid)
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for (; cur != end; ++cur) {
		LootItem* item = cur->get();
		if (item->item_id == itemid) {
			return item;
		}
	}
	return(nullptr);
}

bool NPC::HasQuestLootItem(int16 itemid)
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for(; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if(sitem && sitem->quest == 1 && sitem->item_id == itemid) 
		{
			return true;
		}
	}

	return false;
}

bool NPC::HasPetLootItem(int16 itemid)
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for (; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if (sitem && sitem->pet == 1 && sitem->item_id == itemid)
		{
			return true;
		}
	}

	return false;
}

bool NPC::HasRequiredQuestLoot(int16 itemid1, int16 itemid2, int16 itemid3, int16 itemid4)
{
	if (itemid2 == 0 && itemid3 == 0 && itemid4 == 0) {
		return true;
	}

	uint8 item2count = 0, item3count = 0, item4count = 0, item1npc = 0, item2npc = 0, item3npc = 0, item4npc = 0;
	uint8 item1count = 1; 
	if (itemid2 > 0) {
		item2count = 1;
	}
	if (itemid3 > 0) {
		item3count = 1;
	}
	if (itemid4 > 0) {
		item4count = 1;
	}

	if(itemid1 == itemid2 && itemid2 > 0) {
		item2count = item1count;
		++item1count;
		++item2count;
	}
	if(itemid1 == itemid3 && itemid3 > 0) {
		item3count = item1count;
		++item1count;
		++item3count;
	}
	if(itemid1 == itemid4 && itemid4 > 0) {
		item4count = item1count;
		++item1count;
		++item4count;
	}

	if(itemid2 == itemid3  && itemid2 > 0 && itemid3 > 0) {
		item3count = item2count;
		++item2count;
		++item3count;
	}
	if(itemid2 == itemid4  && itemid2 > 0 && itemid4 > 0) {
		item4count = item2count;
		++item2count;
		++item4count;
	}
	if(itemid3 == itemid4 && itemid3 > 0 && itemid4 > 0) {
		item4count = item3count;
		++item3count;
		++item4count;
	}

	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for(; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if(sitem && sitem->quest == 1) {
			if (sitem->item_id == itemid1) {
				++item1npc;
			}

			if (sitem->item_id == itemid2 && itemid2 > 0) {
				++item2npc;
			}

			if (sitem->item_id == itemid3 && itemid3 > 0) {
				++item3npc;
			}

			if (sitem->item_id == itemid4 && itemid4 > 0) {
				++item4npc;
			}
		}
	}

	if(item1npc < item1count) {
		return false;
	}

	if (itemid2 > 0 && item2npc < item2count) {
		return false;
	}

	if (itemid3 > 0 && item3npc < item3count) {
		return false;
	}

	if (itemid4 > 0 && item4npc < item4count) {
		return false;
	}

	return true;
}

bool NPC::HasQuestLoot() 
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for(; cur != end; ++cur) {
		LootItem* questitem = cur->get();
		if(questitem && questitem->quest == 1) {
			return true;
		}
	}

	return false;
}

void NPC::CleanQuestLootItems() 
{
	//Removes nodrop or multiple quest loot items from a NPC before sending the corpse items to the client.

	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	uint8 count = 0;
	for(; cur != end; ++cur) {
		LootItem* quest_item = cur->get();
		if(quest_item && (quest_item->quest == 1 || quest_item->pet == 1)) {
			uint8 count = CountQuestItem(quest_item->item_id);
			if(count > 1 && quest_item->pet != 1) {
				RemoveItem(quest_item);
				return;
			}
			else {
				const EQ::ItemData* item = database.GetItem(quest_item->item_id);
				if(item && item->NoDrop == 0) {
					RemoveItem(quest_item);
					return;
				}
			}
		}
	}
}

uint8 NPC::CountQuestItem(uint16 itemid)
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	uint8 count = 0;
	for(; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if(sitem && sitem->item_id == itemid) {
			++count;
		}
	}

	return count;
}

uint8 NPC::CountQuestItems()
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	uint8 count = 0;
	for(; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if(sitem && sitem->quest == 1) {
			++count;
		}
	}

	return count;
}

bool NPC::RemoveQuestLootItems(int16 itemid) 
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for (; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if (sitem && sitem->quest == 1) {
			if(itemid == 0 || itemid == sitem->item_id) {
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

bool NPC::RemovePetLootItems(int16 itemid)
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for (; cur != end; ++cur) {
		LootItem* sitem = cur->get();
		if (sitem && sitem->pet == 1) {
			if (itemid == 0 || itemid == sitem->item_id) {
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

bool NPC::MoveItemToGeneralInventory(LootItem* weapon)
{
	auto l = LootdropEntriesRepository::NewNpcEntity();

	if (!weapon) {
		return false;
	}

	uint32 weaponid = weapon->item_id;
	int16 slot = weapon->equip_slot;
	int8 weaponcharges = weapon->charges;
	bool pet = weapon->pet;
	bool quest = weapon->quest;
	uint8_t weapon_min_level = weapon->min_level;
	uint8_t weapon_max_level = weapon->max_level;
	uint8_t weapon_min_looter_level = weapon->min_looter_level;
	uint32_t loot_lockout_timer = weapon->item_loot_lockout_timer;

	l.item_id = weaponid;
	l.item_charges = weaponcharges;
	l.minlevel = weapon_min_level;
	l.maxlevel = weapon_max_level;
	l.min_looter_level = weapon_min_looter_level;
	l.item_loot_lockout_timer = loot_lockout_timer;

	const EQ::ItemData* item = database.GetItem(weaponid);

	if (item) {
		RemoveItem(weapon);

		Log(Logs::Detail, Logs::Trading, "%s is moving %d in slot %d to general inventory. quantity: %d", GetCleanName(), weaponid, slot, weaponcharges);
		AddLootDrop(item, l, false, false, quest, pet, false);

		return true;
	}

	return false;
}

void NPC::RemoveItem(LootItem* item_data, uint8 quantity) {

	if (!item_data)	{
		return;
	}

	for (auto iter = m_loot_items.begin(); iter != m_loot_items.end(); ++iter) {
		LootItem* sitem = iter->get();
		if (sitem != item_data) { continue; }

		if (!sitem)
			return;

		int16 eslot = sitem->equip_slot;
		if (sitem->charges <= 1 || quantity == 0) {
			DeleteEquipment(eslot);

			Log(Logs::General, Logs::Trading, "%s is deleting item %d from slot %d", GetName(), sitem->item_id, eslot);
			m_loot_items.erase(iter);

			UpdateEquipmentLight();
			if (UpdateActiveLight())
				SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
		}
		else {
			sitem->charges -= quantity;
			Log(Logs::General, Logs::Trading, "%s is deleting %d charges from item %d in slot %d", GetName(), quantity, sitem->item_id, eslot);
		}

		return;
	}
}

void NPC::CheckMinMaxLevel(Mob *killer)
{
	if (killer == nullptr || !killer->IsClient()) {
		return;
	}

	uint16 killer_level = killer->GetLevel();
	int16 equipment_slot;

	auto cur = m_loot_items.begin();
	while (cur != m_loot_items.end()) {
		if (!(*cur)) {
			return;
		}

		uint8 min_level = (*cur)->min_level;
		uint8 max_level = (*cur)->max_level;
		bool fits_level_criteria = (
			(min_level > 0 && killer_level < min_level) ||
			(max_level > 0 && killer_level > max_level)
		);

		if (fits_level_criteria) {
			equipment_slot = (*cur)->equip_slot;
			DeleteEquipment(equipment_slot);
			cur = m_loot_items.erase(cur);
			continue;
		}
		++cur;
	}

	UpdateEquipmentLight();
	if (UpdateActiveLight()) {
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
	}
}

void NPC::ClearLootItems()
{
	LootItems::iterator cur, end;
	cur = m_loot_items.begin();
	end = m_loot_items.end();
	for (; cur != end; ++cur) {
		if (!(*cur)) {
			return;
		}

		int16 eslot = (*cur)->equip_slot;
		DeleteEquipment(eslot);
	}

	m_loot_items.clear();

	UpdateEquipmentLight();
	if (UpdateActiveLight()) {
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
	}
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
	if (!m_loot_items.empty()) {
		to->Message(
			Chat::White,
			fmt::format(
				"Loot | Name: {} ID: {} Loottable ID: {}",
				GetName(),
				GetNPCTypeID(),
				GetLoottableID()
			).c_str()
		);

		int item_count = 0;

		for (auto current_item : m_loot_items) {
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
			linker.SetLootData(current_item.get());

			to->Message(
				Chat::White,
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
		to->Message(Chat::White, "NPC::QueryLoot() - Does not have any item loot");
	}

	bool has_money = (
		m_loot_platinum > 0 ||
		m_loot_gold > 0 ||
		m_loot_silver > 0 ||
		m_loot_copper > 0
		);
	if (has_money) {
		to->Message(
			Chat::White,
			fmt::format(
				"Money | {}",
				Strings::Money(
					m_loot_platinum,
					m_loot_gold,
					m_loot_silver,
					m_loot_copper
				)
			).c_str()
		);
	}
}

void NPC::AddLootCash(uint16 in_copper, uint16 in_silver, uint16 in_gold, uint16 in_platinum) {
	m_loot_copper = in_copper >= 0 ? in_copper : 0;
	m_loot_silver = in_silver >= 0 ? in_silver : 0;
	m_loot_gold = in_gold >= 0 ? in_gold : 0;
	m_loot_platinum = in_platinum >= 0 ? in_platinum : 0;
}

void NPC::AddLootCash() {
	m_loot_copper = zone->random.Int(1, 100);
	m_loot_silver = zone->random.Int(1, 50);
	m_loot_gold = zone->random.Int(1, 10);
	m_loot_platinum = zone->random.Int(1, 5);
}

void NPC::RemoveLootCash() {
	m_loot_copper = 0;
	m_loot_silver = 0;
	m_loot_gold = 0;
	m_loot_platinum = 0;
}

bool NPC::AddQuestLoot(int16 itemid, int8 charges, const EQ::ItemCustomData& item_custom_data) {
	auto l = LootdropEntriesRepository::NewNpcEntity();

	const EQ::ItemData* item = database.GetItem(itemid);
	if (item) {
		l.item_charges = charges;
		l.equip_item = 0;
		AddLootDrop(item, l, false, false, true, false, false, item_custom_data);
		Log(Logs::General, Logs::Trading, "Adding item %d to the NPC's loot marked as quest.", itemid);
		if (itemid > 0 && HasPetLootItem(itemid)) {
			Log(Logs::General, Logs::Trading, "Deleting quest item %d from NPC's pet loot.", itemid);
			RemovePetLootItems(itemid);
		}
	}
	else
		return false;

	return true;
}

bool NPC::AddPetLoot(int16 itemid, int8 charges, bool fromquest, const EQ::ItemCustomData& item_custom_data) {
	auto l = LootdropEntriesRepository::NewNpcEntity();

	const EQ::ItemData* item = database.GetItem(itemid);

	if (!item) {
		return false;
	}

	bool valid = (item->NoDrop != 0 && (!IsCharmedPet() || (IsCharmedPet() && CountQuestItem(item->ID) == 0)));
	if (!fromquest || valid) {
		if (item) {
			l.item_charges = charges;
			AddLootDrop(item, l, true, true, false, true, false, item_custom_data);
			Log(Logs::General, Logs::Trading, "Adding item %d to the NPC's loot marked as pet.", itemid);
			return true;
		}
	}
	else {
		Log(Logs::General, Logs::Trading, "Item %d is a duplicate or no drop. Deleting...", itemid);
		return false;
	}

	return false;
}

void NPC::DeleteQuestLoot(int16 itemid1, int16 itemid2, int16 itemid3, int16 itemid4)
{
	int16 items = m_loot_items.size();
	for (int i = 0; i < items; ++i) {
		if (itemid1 == 0) {
			if (!RemoveQuestLootItems(itemid1))
				break;
		}
		else {
			if (itemid1 != 0) {
				RemoveQuestLootItems(itemid1);
			}
			if (itemid2 != 0) {
				RemoveQuestLootItems(itemid2);
			}
			if (itemid3 != 0) {
				RemoveQuestLootItems(itemid3);
			}
			if (itemid4 != 0) {
				RemoveQuestLootItems(itemid4);
			}
		}
	}
}

void NPC::DeleteInvalidQuestLoot()
{
	int16 items = m_loot_items.size();
	for (int i = 0; i < items; ++i) {
		CleanQuestLootItems();
	}
}

uint32 NPC::GetEquipment(uint8 material_slot) const
{
	if (material_slot > 8) {
		return 0;
	}
	int invslot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);

	if (material_slot == EQ::textures::weaponPrimary && !equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange]) {
		invslot = EQ::invslot::slotRange;
	}

	if (invslot == INVALID_INDEX) {
		return 0;
	}
	return equipment[invslot];
}

int32 NPC::GetEquipmentMaterial(uint8 material_slot) const
{
	if (material_slot >= EQ::textures::materialCount) {
		return 0;
	}

	int inv_slot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);

	if (material_slot == EQ::textures::weaponPrimary && !equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange]) {
		inv_slot = EQ::invslot::slotRange;
	}

	if (inv_slot == -1) {
		return 0;
	}
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