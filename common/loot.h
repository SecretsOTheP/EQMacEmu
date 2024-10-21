#ifndef CODE_LOOT_H
#define CODE_LOOT_H

#include <list>
#include <string>
#include <memory>
#include <unordered_map>
#include "types.h"
#include "item_instance.h"

struct LootItem {
	uint32	item_id;	  // uint32	item_id;
	int16	equip_slot;	  // int16	equip_slot;
	int8	charges;	  // int8	charges; 
	uint16	lootslot;	  // uint16	lootslot;
	uint8	min_level;		  // 
	uint8	max_level;		  // 
	uint8	quest;
	uint8	pet;
	bool	forced;
	uint8	min_looter_level;
	uint32	item_loot_lockout_timer;
	EQ::ItemCustomData custom_data;

	LootItem()
	{
		item_id = 0;
		equip_slot = 0;
		charges = 0;
		lootslot = 0;
		min_level = 0;
		max_level = 0;
		quest = 0;
		pet = 0;
		forced = false;
		min_looter_level = 0;
		item_loot_lockout_timer = 0;
	}

	uint32 GetSelfFoundCharacterID() const { return EQ::ItemInstance::GetSelfFoundCharacterID(custom_data); }

};

typedef std::list<std::shared_ptr<LootItem>> LootItems;
typedef std::unordered_map<int, std::shared_ptr<LootItem>> BagLootItems; // k = 0..9

const BagLootItems EmptyBagLootItems;

#endif
