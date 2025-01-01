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
#include "../common/eqemu_logsys.h"
#include "../common/fastmath.h"
#include "../common/misc_functions.h"
#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/zone_store.h"

#include "entity.h"
#include "forage.h"
#include "client.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "titles.h"
#include "water_map.h"
#include "zonedb.h"
#include "../common/events/player_event_logs.h"
#include "worldserver.h"
#include "../common/repositories/criteria/content_filter_criteria.h"

extern WorldServer worldserver;

#include <iostream>

#ifdef _WINDOWS
#define snprintf	_snprintf
#endif

struct NPCType;

//max number of items which can be in the foraging table
//for a given zone.
#define FORAGE_ITEM_LIMIT 50

uint32 ZoneDatabase::GetZoneForage(uint32 ZoneID, uint8 skill) {

	uint32 item[FORAGE_ITEM_LIMIT];
	uint32 chance[FORAGE_ITEM_LIMIT];
	uint32 ret;

	for (int c=0; c < FORAGE_ITEM_LIMIT; c++) {
		item[c] = 0;
	}

	uint32 chancepool = 0;
    std::string query = fmt::format(
		SQL(
			SELECT
			  itemid,
			  chance 
			FROM
			  forage
			WHERE
			  zoneid = '{}' 
			AND 
			  level <= '{}'
	          {}
			LIMIT
			  {}
		), 
		ZoneID, 
	    skill, 
		ContentFilterCriteria::apply(),
		FORAGE_ITEM_LIMIT
	);
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

	uint8 index = 0;
    for (auto row = results.begin(); row != results.end(); ++row, ++index) {
        if (index >= FORAGE_ITEM_LIMIT)
            break;

        item[index] = atoi(row[0]);
        chance[index] = atoi(row[1]) + chancepool;
		Log(Logs::General, Logs::General, "Possible Forage: %d with a %d chance total to %d chancepool", item[index], chance[index] - chancepool, chance[index]);
        chancepool = chance[index];
    }

	if(chancepool == 0 || index < 1)
		return 0;

	if(index == 1) {
		return item[0];
	}

	ret = 0;

	uint32 rindex = zone->random.Int(1, chancepool);

	for(int i = 0; i < index; i++) {
		if(rindex <= chance[i]) {
			ret = item[i];
			break;
		}
	}

	return ret;
}

uint32 ZoneDatabase::GetZoneFishing(uint32 ZoneID, uint8 skill)
{
	uint32 item[50];
	uint32 chance[50];
	uint32 chancepool = 0;
	uint32 ret = 0;

	for (int c=0; c<50; c++) {
		item[c]=0;
		chance[c]=0;
	}

    std::string query = fmt::format(
		SQL(
			SELECT 
			  itemid,
			  chance
			FROM 
			  fishing 
			WHERE 
			  zoneid = '{}'
			AND 
			  skill_level <= '{}'
			  {}
			),
			ZoneID, 
			skill,
			ContentFilterCriteria::apply()
		);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return 0;
    }

    uint8 index = 0;
    for (auto row = results.begin(); row != results.end(); ++row, ++index) {
        if (index >= 50)
            break;

        item[index] = atoi(row[0]);
        chance[index] = atoi(row[1])+chancepool;
		chancepool = chance[index];
    }

	if (index <= 0)
        return 0;

    uint32 random = zone->random.Int(1, chancepool);
    for (int i = 0; i < index; i++)
    {
        if (random > chance[i])
            continue;

        ret = item[i];
        break;
    }

	return ret;
}
//we need this function to immediately determine, after we receive OP_Fishing, if we can even try to fish, otherwise we have to wait a while to get the failure
bool Client::CanFish() {

	if (fishing_timer.Enabled()) {
		Message_StringID(Chat::White, StringID::ALREADY_FISHING);	//You are already fishing!
		return false;
	}

	if (m_inv.GetItem(EQ::invslot::slotCursor)) {
		Message_StringID(Chat::Skills, StringID::FISHING_HANDS_FULL);
		return false;
	}

	//make sure we still have a fishing pole on:
	const EQ::ItemInstance *Pole = m_inv[EQ::invslot::slotPrimary];
	int32 bslot = m_inv.HasItemByUse(EQ::item::ItemTypeFishingBait, 1, invWhereWorn | invWherePersonal);
	const EQ::ItemInstance *Bait = nullptr;
	if (bslot != INVALID_INDEX) {
		Bait = m_inv.GetItem(bslot);
	}

	if (!Pole || !Pole->IsClassCommon() || Pole->GetItem()->ItemType != EQ::item::ItemTypeFishingPole) {
		if (m_inv.HasItemByUse(EQ::item::ItemTypeFishingPole, 1, invWhereWorn | invWherePersonal | invWhereBank | invWhereTrading | invWhereCursor))	//We have a fishing pole somewhere, just not equipped
			Message_StringID(Chat::Skills, StringID::FISHING_EQUIP_POLE);	//You need to put your fishing pole in your primary hand.
		else	//We don't have a fishing pole anywhere
			Message_StringID(Chat::Skills, StringID::FISHING_NO_POLE);	//You can't fish without a fishing pole, go buy one.
		return false;
	}

	if (!Bait || !Bait->IsClassCommon() || Bait->GetItem()->ItemType != EQ::item::ItemTypeFishingBait) {
		Message_StringID(Chat::Skills, StringID::FISHING_NO_BAIT);	//You can't fish without fishing bait, go buy some.
		return false;
	}

	if (zone->zonemap != nullptr && zone->watermap != nullptr && RuleB(Watermap, CheckForWaterWhenFishing)) {
		glm::vec3 rod_position;
		// Tweak Rod and LineLength if required
		const float rod_length = RuleR(Watermap, FishingRodLength);
		const float line_length = RuleR(Watermap, FishingLineLength);
		float client_heading = GetHeading() * 2;
		int heading_degrees;

		heading_degrees = (int)((client_heading * 360) / 512);
		heading_degrees = heading_degrees % 360;

		LogMaps("Heading is at {}, GetHeading() is {:.f}", heading_degrees, client_heading);

		rod_position.x = m_Position.x + rod_length * sin(heading_degrees * M_PI / 180.0f);
		rod_position.y = m_Position.y + rod_length * cos(heading_degrees * M_PI / 180.0f);

		float bestz = zone->zonemap->FindBestZ(rod_position, nullptr);
		float len = m_Position.z - bestz;
		if (len > line_length || len < 0.0f) {
			Message_StringID(Chat::Skills, StringID::FISHING_LAND);
			return false;
		}

		glm::vec3 dest;
		dest.x = rod_position.x;
		dest.y = rod_position.y;
		dest.z = rod_position.z;

		if (!CheckLosFN(dest.x, dest.y, dest.z, 0.0f)) {
			LogMaps("Failing to fish because of CheckLosFN");
			// fishing into a wall to reach water on other side?
			Message_StringID(Chat::Skills, StringID::FISHING_LAND);	//Trying to catch land sharks perhaps?
			return false;
		}

		float step_size = RuleR(Watermap, FishingLineStepSize);

		for (float i = 0.0f; i < line_length; i += step_size) {
			glm::vec3 dest(rod_position.x, rod_position.y, m_Position.z - i);

			bool in_lava = zone->watermap->InLava(dest);
			bool in_water = zone->watermap->InWater(dest) || zone->watermap->InVWater(dest);

			if (GetZoneID() == Zones::POWATER) {
				if (zone->IsWaterZone(rod_position.z)) {
					in_water = true;
				}
				else {
					in_water = false;
				}
			}

			if (in_lava) {
				Message_StringID(Chat::Skills, StringID::FISHING_LAVA);	//Trying to catch a fire elemental or something?
				return false;
			}

			if (in_water) {
				LogMaps("fishing is in water : region [{}]", in_water);
				return true;
			}
		}

		Message_StringID(Chat::Skills, StringID::FISHING_LAND);
		return false;
	}
	return true;
}

void Client::GoFish()
{

	fishing_timer.Disable();

	//we're doing this a second time (1st in Client::Handle_OP_Fishing) to make sure that, between when we started fishing & now, we're still able to fish (in case we move, change equip, etc)
	if (!CanFish())	//if we can't fish here, we don't need to bother with the rest
		return;

	//multiple entries yeilds higher probability of dropping...
	uint32 common_fish_ids[MAX_COMMON_FISH_IDS] = {
		1038, // Tattered Cloth Sandals
		1038, // Tattered Cloth Sandals
		1038, // Tattered Cloth Sandals
		13019, // Fresh Fish
		13076, // Fish Scales
		13076, // Fish Scales
		7007, // Rusty Dagger
		7007, // Rusty Dagger
		7007 // Rusty Dagger

	};

	//success formula is not researched at all

	int fishing_skill = GetSkill(EQ::skills::SkillFishing);	//will take into account skill bonuses on pole & bait
	uint16 fishing_mod = 0;

	//make sure we still have a fishing pole on:
	int32 bslot = m_inv.HasItemByUse(EQ::item::ItemTypeFishingBait, 1, invWhereWorn|invWherePersonal);
	const EQ::ItemInstance* Bait = nullptr;
	if (bslot != INVALID_INDEX)
		Bait = m_inv.GetItem(bslot);

	//if the bait isnt equipped, need to add its skill bonus
	if(bslot >= EQ::invslot::GENERAL_BEGIN && Bait != nullptr && Bait->GetItem()->SkillModType == EQ::skills::SkillFishing) {
		fishing_skill += fishing_skill * (Bait->GetItem()->SkillModValue/100);

		if (fishing_skill > HARD_SKILL_CAP)
		{
			fishing_skill = HARD_SKILL_CAP;
		}
	}

	if (fishing_skill > 100)
		fishing_mod = 100 + ((fishing_skill - 100) / 2);
	else
		fishing_mod = fishing_skill;

	uint8 success = SKILLUP_FAILURE;
	if (zone->random.Int(0,175) < fishing_mod) {
		uint32 food_id = 0;

		if (zone->random.Int(0, 299) <= fishing_mod)
		{
			food_id = database.GetZoneFishing(m_pp.zone_id, fishing_skill);
		}

		//consume bait, should we always consume bait on success?
		DeleteItemInInventory(bslot, 1, true);	//do we need client update?

		if(food_id == 0) {
			int index = zone->random.Int(0, MAX_COMMON_FISH_IDS-1);
			food_id = common_fish_ids[index];
		}

		const EQ::ItemData* food_item = database.GetItem(food_id);

		EQ::ItemInstance* inst = database.CreateItem(food_item, 1);
		if(inst != nullptr) {
			if(CheckLoreConflict(inst->GetItem()))
			{
				Message_StringID(Chat::White, StringID::DUP_LORE);
				safe_delete(inst);
			}
			else
			{
				if (food_item->ItemType == EQ::item::ItemTypeFood)
				{
					Message_StringID(Chat::Skills, StringID::FISHING_SUCCESS, food_item->Name);
				}
				else
				{
					Message_StringID(Chat::Skills, StringID::FISHING_SUCCESS_SOMETHING);
				}

				PushItemOnCursorWithoutQueue(inst);

				safe_delete(inst);
				inst = m_inv.GetItem(EQ::invslot::slotCursor);
				success = SKILLUP_SUCCESS;
			}

			if(inst) {
				std::vector<std::any> args;
				args.push_back(inst);
				parse->EventPlayer(EVENT_FISH_SUCCESS, this, "", inst->GetID(), &args);

				if (player_event_logs.IsEventEnabled(PlayerEvent::FISH_SUCCESS)) {
					auto e = PlayerEvent::FishSuccessEvent{
						.item_id = inst->GetItem()->ID,
						.item_name = inst->GetItem()->Name,
					};

					RecordPlayerEventLog(PlayerEvent::FISH_SUCCESS, e);
				}
			}
		}
	}
	else
	{
		//chance to use bait when you dont catch anything...
		if (zone->random.Int(0, 4) == 1) 
		{
			DeleteItemInInventory(bslot, 1, true);
			Message_StringID(Chat::Skills, StringID::FISHING_LOST_BAIT);	//You lost your bait!
		} 
		else 
		{
			bool spilled_beer = false;
			if (zone->random.Int(0, 15) == 1)	//give about a 1 in 15 chance to spill your beer.
			{
				if (SpillBeer())
				{
					Message_StringID(Chat::Skills, StringID::FISHING_SPILL_BEER);	//You spill your beer while bringing in your line.
					spilled_beer = true;
				}
			}
			
			if (!spilled_beer)
			{
				Message_StringID(Chat::Skills, StringID::FISHING_FAILED);	//You didn't catch anything.
			}
		}

		parse->EventPlayer(EVENT_FISH_FAILURE, this, "", 0);
		RecordPlayerEventLog(PlayerEvent::FISH_FAILURE, PlayerEvent::EmptyEvent{});
	}

	//chance to break fishing pole...
	uint16 break_chance = 49;
	if(fishing_skill > 49)
		break_chance = fishing_skill;
	if (zone->random.Int(0, break_chance) == 1) {
		Message_StringID(Chat::Skills, StringID::FISHING_POLE_BROKE);	//Your fishing pole broke!
		DeleteItemInInventory(EQ::invslot::slotPrimary, 0, true);
	}

	if(CheckIncreaseSkill(EQ::skills::SkillFishing, nullptr, zone->skill_difficulty[EQ::skills::SkillFishing].difficulty[GetClass()]), success)
	{
		if(title_manager.IsNewTradeSkillTitleAvailable(EQ::skills::SkillFishing, GetRawSkill(EQ::skills::SkillFishing)))
			NotifyNewTitlesAvailable();
	}
}

void Client::ForageItem(bool guarantee) {

	int skill_level = GetSkill(EQ::skills::SkillForage);
	uint8 success = SKILLUP_FAILURE;

	// these may need to be fine tuned, I am just guessing here
	if (guarantee || zone->random.Int(0,199) < skill_level) 
	{
		uint32 foragedfood = 0;
		uint32 stringid = StringID::FORAGE_NOEAT;

		if (RuleB(Character, ForageNeedFoodorDrink))
		{
			// If we're hungry or thirsty, we want to prefer food or water.
			if(Hungry() && Thirsty())
			{
				if(m_pp.hunger_level <= m_pp.thirst_level)
					foragedfood = 13047;
				else
					foragedfood = 13044;
			}
			// We only need food.
			else if(Hungry())
			{
				foragedfood = 13047;
			}
			// We only need water.
			else if(Thirsty())
			{
				foragedfood = 13044;
			}
		}

		if (foragedfood == 0)
			foragedfood = database.GetZoneForage(m_pp.zone_id, skill_level);

		if (foragedfood > 0)
		{
			int16 inv_slot_id = GetInv().HasItem(foragedfood, 1, invWhereCursor);
			if (inv_slot_id != INVALID_INDEX && !GetGM()) {
				if (RuleB(Quarm, CursorAllowDuplicateItems)) {
					if (m_inv.CursorSize() >= 10) {
						auto broken_string = fmt::format("Forage failed. Cursor queue full.");
						Message(Chat::Red, broken_string.c_str());
						return;
					}
				}
				else
				{
					// we already have this item on the cursor - so stop sending it here.
					Message_StringID(Chat::Skills, FORAGE_FAILED);
					return;
				}
			}
		}

		const EQ::ItemData* food_item = database.GetItem(foragedfood);

		if(!food_item)
		{
			Log(Logs::General, Logs::Error, "nullptr returned from database.GetItem (%d) in ClientForageItem", foragedfood);
			return;
		}

		if(foragedfood == 13106)
			stringid = StringID::FORAGE_GRUBS;
		else
		{
			switch(food_item->ItemType) 
			{
				case EQ::item::ItemTypeFood:
					stringid = StringID::FORAGE_FOOD;
					break;

				case EQ::item::ItemTypeDrink:
					if(strstr(food_item->Name, "Water"))
						stringid = StringID::FORAGE_WATER;
					else
						stringid = StringID::FORAGE_DRINK;
					break;

				default:
					break;
			}
		}

		EQ::ItemInstance* inst = database.CreateItem(food_item, 1);
		if(inst != nullptr) 
		{
			// check to make sure it isn't a foraged lore item
			if(CheckLoreConflict(inst->GetItem()))
			{
				Message_StringID(Chat::White, StringID::DUP_LORE);
				safe_delete(inst);
			}
			else
			{
				Message_StringID(Chat::Skills, stringid);
				SummonItem(inst->GetID(), inst->GetCharges());
				safe_delete(inst);
				inst = m_inv.GetItem(EQ::invslot::slotCursor);
			}

			if(inst) {
				std::vector<std::any> args;
				args.push_back(inst);
				parse->EventPlayer(EVENT_FORAGE_SUCCESS, this, "", inst->GetID(), &args);

				if (player_event_logs.IsEventEnabled(PlayerEvent::FORAGE_SUCCESS)) {
					auto e = PlayerEvent::ForageSuccessEvent{
						.item_id = inst->GetItem()->ID,
						.item_name = inst->GetItem()->Name
					};
					RecordPlayerEventLog(PlayerEvent::FORAGE_SUCCESS, e);
				}
			}

			success = SKILLUP_SUCCESS;
		}
	} 
	else
	{
		Message_StringID(Chat::Skills, StringID::FORAGE_FAILED);
		parse->EventPlayer(EVENT_FORAGE_FAILURE, this, "", 0);
		RecordPlayerEventLog(PlayerEvent::FORAGE_FAILURE, PlayerEvent::EmptyEvent{});
	}

	CheckIncreaseSkill(EQ::skills::SkillForage, nullptr, zone->skill_difficulty[EQ::skills::SkillForage].difficulty[GetClass()], success);

}

