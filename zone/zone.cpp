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

#include <float.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WINDOWS
#define	snprintf	_snprintf
#define	vsnprintf	_vsnprintf
#else
#include <pthread.h>
#include "../common/unix.h"
#endif

#include "../common/global_define.h"
#include "../common/features.h"
#include "../common/rulesys.h"
#include "../common/seperator.h"
#include "../common/strings.h"
#include "../common/eqemu_logsys.h"
#include "../common/data_verification.h"

#include "guild_mgr.h"
#include "map.h"
#include "npc.h"
#include "object.h"
#include "pathfinder_null.h"
#include "pathfinder_nav_mesh.h"
#include "pathfinder_waypoint.h"
#include "mob_movement_manager.h"
#include "petitions.h"
#include "quest_parser_collection.h"
#include "spawn2.h"
#include "spawngroup.h"
#include "water_map.h"
#include "worldserver.h"
#include "zone.h"
#include "zone_config.h"
#include "zone_reload.h"
#include "../common/repositories/criteria/content_filter_criteria.h"
#include "../common/repositories/content_flags_repository.h"
#include "../common/repositories/rule_sets_repository.h"
#include "../common/repositories/zone_points_repository.h"
#include "../common/serverinfo.h"

#include <time.h>

#ifdef _WINDOWS
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#endif


extern bool staticzone;
extern PetitionList petition_list;
extern QuestParserCollection* parse;
extern uint32 numclients;
extern WorldServer worldserver;
extern Zone* zone;

Mutex MZoneShutdown;

volatile bool is_zone_loaded = false;
Zone* zone = 0;
const static std::set<std::string> arrClassicPlanes = { "hateplane", "airplane", "fearplane" };
void UpdateWindowTitle(char* iNewTitle);

bool Zone::Bootup(uint32 iZoneID, bool is_static, uint32 iGuildID) {
	const char* zonename = ZoneName(iZoneID);

	if (iZoneID == 0 || zonename == 0)
		return false;
	if (zone != 0 || is_zone_loaded) {
		std::cerr << "Error: Zone::Bootup call when zone already booted!" << std::endl;
		worldserver.SetZoneData(0, 0);
		return false;
	}

	LogInfo("Booting {} ({}) ({})", zonename, iZoneID, iGuildID);

	numclients = 0;
	zone = new Zone(iZoneID, zonename, iGuildID);

	//init the zone, loads all the data, etc
	if (!zone->Init(is_static)) {
		safe_delete(zone);
		std::cerr << "Zone->Init failed" << std::endl;
		worldserver.SetZoneData(0, 0);
		return false;
	}
	
	std::string tmp;
	if (database.GetVariable("loglevel", tmp)) {
		int log_levels[4];
		int tmp_i = atoi(tmp.c_str());
		if (tmp_i>9){ //Server is using the new code
			for(int i=0;i<4;i++){
				if (((int)tmp[i]>=48) && ((int)tmp[i]<=57))
					log_levels[i]=(int)tmp[i]-48; //get the value to convert it to an int from the ascii value
				else
					log_levels[i]=0; //set to zero on a bogue char
			}
			zone->loglevelvar = log_levels[0];
			LogInfo("General logging level: {} ", zone->loglevelvar);
			zone->merchantvar = log_levels[1];
			LogInfo("Merchant logging level: {} ", zone->merchantvar);
			zone->tradevar = log_levels[2];
			LogInfo("Trade logging level: {} ", zone->tradevar);
			zone->lootvar = log_levels[3];
			LogInfo("Loot logging level: {} ", zone->lootvar);
		}
		else {
			zone->loglevelvar = uint8(tmp_i); //continue supporting only command logging (for now)
			zone->merchantvar = 0;
			zone->tradevar = 0;
			zone->lootvar = 0;
		}
	}

	is_zone_loaded = true;

	worldserver.SetZoneData(iZoneID, iGuildID);

	LogInfo("---- Zone server [{}], listening on port:[{}] ----", zonename, ZoneConfig::get()->ZonePort);
	LogInfo("Zone Bootup: [{}] [{}] ([{}]) ([{}])",
		(is_static) ? "Static" : "Dynamic", zonename, iZoneID, iGuildID);
	UpdateWindowTitle(nullptr);

	if (!is_static) {
		zone->GetTimeSync();
	}

	zone->StartShutdownTimer();

	zone->RequestUCSServerStatus();

	/* Set Logging */

	LogSys.StartFileLogs(StringFormat("%s_port_%u", zone->GetShortName(), ZoneConfig::get()->ZonePort));

	return true;
}

//this really loads the objects into entity_list
bool Zone::LoadZoneObjects() {

	std::string query = StringFormat("SELECT id, zoneid, xpos, ypos, zpos, heading, "
                                    "itemid, charges, objectname, type, icon, size, "
                                    "solid, incline FROM object "
                                    "WHERE zoneid = %i %s",
                                    zoneid, ContentFilterCriteria::apply().c_str());
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
		LogError("Error Loading Objects from DB: {} ", results.ErrorMessage().c_str());
		return false;
    }

    LogInfo("Loading Objects from DB...");
    for (auto row = results.begin(); row != results.end(); ++row) {
        if (atoi(row[9]) == 0)
        {
            // Type == 0 - Static Object
            const char* shortname = ZoneName(atoi(row[1]), false); // zoneid -> zone_shortname

            if (!shortname)
                continue;

			// todo: clean up duplicate code with command_object
			auto d = DoorsRepository::NewEntity();

            d.zone = shortname;
            d.id = 1000000000 + atoi(row[0]); // Out of range of normal use for doors.id
            d.doorid = -1; // Client doesn't care if these are all the same door_id
            d.pos_x = atof(row[2]); // xpos
            d.pos_y = atof(row[3]); // ypos
            d.pos_z = atof(row[4]); // zpos
            d.heading = atof(row[5]); // heading

            d.name, row[8]; // objectname

            // Strip trailing "_ACTORDEF" if present. Client won't accept it for doors.
			int pos = d.name.size() - strlen("_ACTORDEF");
			if (pos > 0 && d.name.compare(pos, std::string::npos, "_ACTORDEF") == 0)
			{
				d.name.erase(pos);
			}

            d.dest_zone = "NONE";

            if ((d.size = atoi(row[11])) == 0) // optional size percentage
                d.size = 100;

            switch (d.opentype = atoi(row[12])) // optional request_nonsolid (0 or 1 or experimental number)
            {
                case 0:
                    d.opentype = 31;
                    break;
                case 1:
                    d.opentype = 9;
                    break;
            }

            d.incline = atoi(row[13]); // optional model incline value
            d.client_version_mask = 0xFFFFFFFF; //We should load the mask from the zone.

	    auto door = new Doors(d);
	    entity_list.AddDoor(door);
	}

	Object_Struct data = {0};
	uint32 id = 0;
        uint32 icon = 0;
        uint32 type = 0;
        uint32 itemid = 0;
        uint32 idx = 0;
        int16 charges = 0;

        id	= (uint32)atoi(row[0]);
        data.zone_id = atoi(row[1]);
        data.x = atof(row[2]);
        data.y = atof(row[3]);
        data.z = atof(row[4]);
        data.heading = atof(row[5]);
		itemid = (uint32)atoi(row[6]);
		charges	= (int16)atoi(row[7]);
        strcpy(data.object_name, row[8]);
        type = (uint8)atoi(row[9]);
        icon = (uint32)atoi(row[10]);
		data.object_type = type;
		data.linked_list_addr[0] = 0;
        data.linked_list_addr[1] = 0;
		data.charges = charges;
		data.maxcharges = charges;
			

        EQ::ItemInstance* inst = nullptr;
        //FatherNitwit: this dosent seem to work...
        //tradeskill containers do not have an itemid of 0... at least what I am seeing
        if (itemid == 0) {
            // Generic tradeskill container
            inst = new EQ::ItemInstance(ItemInstWorldContainer);
        }
        else {
            // Groundspawn object
            inst = database.CreateItem(itemid);
        }

		// Load child objects if container
		if (inst && inst->IsType(EQ::item::ItemClassBag)) {
			database.LoadWorldContainer(id, inst);
			for (uint8 i=0; i<10; i++) {
				const EQ::ItemInstance* b_inst = inst->GetItem(i);
				if (b_inst) {
					data.itemsinbag[i] = b_inst->GetID();
				}
			}
		}

		auto object = new Object(id, type, icon, data, inst);
		entity_list.AddObject(object, false);

	safe_delete(inst);
    }

	return true;
}

//this also just loads into entity_list, not really into zone
bool Zone::LoadGroundSpawns() {
	Ground_Spawns groundspawn;

	memset(&groundspawn, 0, sizeof(groundspawn));
	int gsindex=0;
	LogInfo("Loading Ground Spawns from DB...");
	database.LoadGroundSpawns(zoneid, &groundspawn);
	uint32 ix=0;
	char* name = nullptr;
	uint32 gsnumber=0;
	for(gsindex=0;gsindex<50;gsindex++){
		if(groundspawn.spawn[gsindex].item>0 && groundspawn.spawn[gsindex].item<500000){
			EQ::ItemInstance* inst = nullptr;
			inst = database.CreateItem(groundspawn.spawn[gsindex].item);
			gsnumber=groundspawn.spawn[gsindex].max_allowed;
			ix=0;
			if(inst){
				name = groundspawn.spawn[gsindex].name;
				for(ix=0;ix<gsnumber;ix++){
					auto object = new Object(
					    inst, name, groundspawn.spawn[gsindex].max_x,
					    groundspawn.spawn[gsindex].min_x, groundspawn.spawn[gsindex].max_y,
					    groundspawn.spawn[gsindex].min_y, groundspawn.spawn[gsindex].max_z,
					    groundspawn.spawn[gsindex].heading,
					    groundspawn.spawn[gsindex].respawntimer); // new object with id of 10000+
					entity_list.AddObject(object, false);
				}
				safe_delete(inst);
			}
		}
	}
	return(true);
}

int Zone::SaveTempItem(uint32 merchantid, uint32 npcid, uint32 item, int32 charges, bool sold, uint32 self_found_character_id) {
	int freeslot = 0;
	std::list<MerchantList> merlist = merchanttable[merchantid];
	std::list<MerchantList>::const_iterator itr;
	uint32 i = 1;
	for (itr = merlist.begin(); itr != merlist.end(); ++itr) {
		MerchantList ml = *itr;
		if (ml.item == item && ml.quantity <= 0)
			return 0;

		// Account for merchant lists with gaps in them.
		if (ml.slot >= i)
			i = ml.slot + 1;
	}
	std::list<TempMerchantList> tmp_merlist = tmpmerchanttable[npcid];
	std::list<TempMerchantList>::const_iterator tmp_itr;
	bool update_charges = false;
	TempMerchantList ml;
	while (freeslot == 0 && !update_charges) {
		freeslot = i;
		for (tmp_itr = tmp_merlist.begin(); tmp_itr != tmp_merlist.end(); ++tmp_itr) {
			ml = *tmp_itr;
			if (ml.item == item) {
				update_charges = true;
				freeslot = 0;
				break;
			}
			if ((ml.slot == i) || (ml.origslot == i)) {
				freeslot = 0;
			}
		}
		i++;
	}
	if (update_charges)
	{
		tmp_merlist.clear();
		std::list<TempMerchantList> oldtmp_merlist = tmpmerchanttable[npcid];
		for (tmp_itr = oldtmp_merlist.begin(); tmp_itr != oldtmp_merlist.end(); ++tmp_itr) 
		{
			TempMerchantList ml2 = *tmp_itr;
			if(ml2.item != item)
			{
				//Push the items not affected back into the list. 
				tmp_merlist.push_back(ml2);
			}
			else 
			{
				bool deleted = false;
				if (sold)
				{
					if(database.ItemQuantityType(item) != EQ::item::Quantity_Stacked)
					{
						++ml.quantity;
						if (RuleB(SelfFound, TempMerchantSupport))
						{
							if (self_found_character_id)
								IncreaseSelfFoundPurchaseLimit(npcid, item, self_found_character_id, 1);
							CapSelfFoundPurchaseLimitsByAvailableQuantity(npcid, item, ml.quantity);
						}
						else
						{
							CapSelfFoundPurchaseLimitsByAvailableQuantity(npcid, item, 0);
						}
					}

					// The client has an internal limit of items per stack.
					uint32 new_charges = ml.charges + charges > MERCHANT_CHARGE_CAP ? MERCHANT_CHARGE_CAP : ml.charges + charges;
					ml.charges = new_charges;
				}
				else
				{
					if(database.ItemQuantityType(item) != EQ::item::Quantity_Stacked)
					{
						if (ml.quantity > 0)
						{
							--ml.quantity;
							if (RuleB(SelfFound, TempMerchantSupport))
							{
								if (self_found_character_id)
									DecreaseSelfFoundPurchaseLimit(npcid, item, self_found_character_id, 1);
								CapSelfFoundPurchaseLimitsByAvailableQuantity(npcid, item, ml.quantity);
							}
							else
							{
								CapSelfFoundPurchaseLimitsByAvailableQuantity(npcid, item, 0);
							}
						}
						else
						{
							deleted = true;
						}
					}
					ml.charges = charges;
				}

				if (!ml.origslot)
					ml.origslot = ml.slot;

				if (!deleted && ((database.ItemQuantityType(item) != EQ::item::Quantity_Stacked && ml.quantity > 0) || charges > 0)) //This is a save
				{
					database.SaveMerchantTemp(npcid, ml.origslot, item, ml.charges, ml.quantity);
					tmp_merlist.push_back(ml);
					Log(Logs::General, Logs::Trading, "%d SAVED to temp in slot %d with charges/qty %d/%d", item, ml.origslot, ml.charges, ml.quantity);
				}
				else //This is a delete
				{
					database.DeleteMerchantTemp(npcid, ml.origslot);
					Log(Logs::General, Logs::Trading, "%d DELETED from temp in slot %d", item, ml.origslot);
				}
			}
		}
		tmpmerchanttable[npcid] = tmp_merlist;

		if (sold)
			return ml.slot;

	}
	if (freeslot) {
		if (charges < 0) //sanity check only, shouldnt happen
			charges = 0x7FFF;
		database.SaveMerchantTemp(npcid, freeslot, item, charges, 1);
		Log(Logs::General, Logs::Trading, "%d ADDED to temp in slot %d with charges/qty %d/%d", item, freeslot, charges, 1);

		tmp_merlist = tmpmerchanttable[npcid];
		TempMerchantList ml2;
		ml2.charges = charges;
		ml2.item = item;
		ml2.npcid = npcid;
		ml2.slot = freeslot;
		ml2.origslot = ml2.slot;
		ml2.quantity = 1;
		if (RuleB(SelfFound, TempMerchantSupport))
		{
			if (self_found_character_id && database.ItemQuantityType(item) != EQ::item::Quantity_Stacked)
			{
				IncreaseSelfFoundPurchaseLimit(npcid, item, self_found_character_id, 1);
			}
		}
		tmp_merlist.push_back(ml2);
		tmpmerchanttable[npcid] = tmp_merlist;
	}
	return freeslot;
}


int Zone::SaveReimbursementItem(std::list<TempMerchantList>& reimbursement_list, uint32 charid, uint32 item, int32 charges, bool sold) 
{
	int freeslot = 0;
	std::list<TempMerchantList> tmp_merlist = reimbursement_list;
	std::list<TempMerchantList>::const_iterator tmp_itr;
	bool update_charges = false;
	TempMerchantList ml;
	uint32 i = 0;
	while (freeslot == 0 && !update_charges) {
		freeslot = i;
		for (tmp_itr = tmp_merlist.begin(); tmp_itr != tmp_merlist.end(); ++tmp_itr) {
			ml = *tmp_itr;
			if (ml.item == item) {
				update_charges = true;
				freeslot = 0;
				break;
			}
			if ((ml.slot == i) || (ml.origslot == i)) {
				freeslot = 0;
			}
		}
		i++;
	}
	if (update_charges)
	{
		tmp_merlist.clear();
		std::list<TempMerchantList> oldtmp_merlist = reimbursement_list;
		for (tmp_itr = oldtmp_merlist.begin(); tmp_itr != oldtmp_merlist.end(); ++tmp_itr)
		{
			TempMerchantList ml2 = *tmp_itr;
			if (ml2.item != item)
			{
				//Push the items not affected back into the list. 
				tmp_merlist.push_back(ml2);
			}
			else
			{
				bool deleted = false;
				if (sold)
				{
					if (database.ItemQuantityType(item) != EQ::item::Quantity_Stacked)
					{
						++ml.quantity;
					}

					// The client has an internal limit of items per stack.
					uint32 new_charges = ml.charges + charges > MERCHANT_CHARGE_CAP ? MERCHANT_CHARGE_CAP : ml.charges + charges;
					ml.charges = new_charges;
				}
				else
				{
					if (database.ItemQuantityType(item) != EQ::item::Quantity_Stacked)
					{
						if (ml.quantity > 0)
							--ml.quantity;
						else
							deleted = true;
					}
					ml.charges = charges;
				}

				if (!ml.origslot)
					ml.origslot = ml.slot;

				if (!deleted && ((database.ItemQuantityType(item) != EQ::item::Quantity_Stacked && ml.quantity > 0) || charges > 0)) //This is a save
				{
					database.SaveReimbursementItem(charid, ml.origslot, item, ml.charges, ml.quantity);
					tmp_merlist.push_back(ml);
					Log(Logs::General, Logs::Trading, "%d SAVED to temp in slot %d with charges/qty %d/%d", item, ml.origslot, ml.charges, ml.quantity);
				}
				else //This is a delete
				{
					database.DeleteReimbursementItem(charid, ml.origslot);
					Log(Logs::General, Logs::Trading, "%d DELETED from temp in slot %d", item, ml.origslot);
				}
			}
		}
		reimbursement_list = tmp_merlist;

		if (sold)
			return ml.slot;

	}
	if (freeslot) {
		if (charges < 0) //sanity check only, shouldnt happen
			charges = 0x7FFF;
		database.SaveReimbursementItem(charid, freeslot, item, charges, 1);
		Log(Logs::General, Logs::Trading, "%d ADDED to temp in slot %d with charges/qty %d/%d", item, freeslot, charges, 1);

		tmp_merlist = reimbursement_list;
		TempMerchantList ml2;
		ml2.charges = charges;
		ml2.item = item;
		ml2.npcid = charid;
		ml2.slot = freeslot;
		ml2.origslot = ml2.slot;
		ml2.quantity = 1;
		tmp_merlist.push_back(ml2);
		reimbursement_list = tmp_merlist;
	}
	return freeslot;
}

void Zone::SaveMerchantItem(uint32 merchantid, int16 item, int8 charges, int8 slot) 
{
	std::list<MerchantList> merlist = merchanttable[merchantid];
	std::list<MerchantList>::const_iterator itr;
	bool update_charges = false;
	MerchantList ml;
	for (itr = merlist.begin(); itr != merlist.end(); ++itr) {
		ml = *itr;
		if (ml.item == item && ml.slot == slot)
		{
			update_charges = true;
			break;
		}
	}

	if (update_charges)
	{
		merlist.clear();
		std::list<MerchantList> old_merlist = merchanttable[merchantid];
		for (itr = old_merlist.begin(); itr != old_merlist.end(); ++itr) {
			MerchantList ml2 = *itr;
			if(ml2.item != item && ml2.slot != slot)
			{
				merlist.push_back(ml2);
			}
			else
			{
				Log(Logs::General, Logs::Trading, "Merchant %d is saving item %d with %d charges", merchantid, item, charges);
				ml2.qty_left = charges;
				merlist.push_back(ml2);
			}
		}
		merchanttable[merchantid] = merlist;
	}
}

void Zone::ResetMerchantQuantity(uint32 merchantid) 
{
	std::list<MerchantList> merlist = merchanttable[merchantid];
	merlist.clear();
	merchanttable[merchantid] = db_merchanttable[merchantid];
}

uint32 Zone::GetTempMerchantQuantity(uint32 NPCID, uint32 Slot) {

	std::list<TempMerchantList> TmpMerchantList = tmpmerchanttable[NPCID];
	std::list<TempMerchantList>::const_iterator Iterator;

	for (Iterator = TmpMerchantList.begin(); Iterator != TmpMerchantList.end(); ++Iterator)
		if ((*Iterator).slot == Slot)
			return (*Iterator).charges;

	return 0;
}

int32 Zone::GetTempMerchantQtyNoSlot(uint32 NPCID, int16 itemid) {

	std::list<TempMerchantList> TmpMerchantList = tmpmerchanttable[NPCID];
	std::list<TempMerchantList>::const_iterator Iterator;

	for (Iterator = TmpMerchantList.begin(); Iterator != TmpMerchantList.end(); ++Iterator)
	{
		if ((*Iterator).item == itemid && (*Iterator).quantity > 0) {
			return (*Iterator).charges / (*Iterator).quantity;
		}
	}

	return -1;
}

void Zone::LoadTempMerchantData() {
	LogInfo("Loading Temporary Merchant Lists...");
	std::string query = StringFormat(
		"SELECT								   "
		"DISTINCT ml.npcid,					   "
		"ml.slot,							   "
		"ml.charges,						   "
		"ml.itemid,							   "
		"ml.quantity						   "
		"FROM								   "
		"merchantlist_temp ml,				   "
		"spawnentry se,						   "
		"spawn2 s2							   "
		"WHERE								   "
		"ml.npcid = se.npcid				   "
		"AND se.spawngroupid = s2.spawngroupid "
		"AND s2.zone = '%s' "
		"ORDER BY ml.slot					   ", GetShortName());
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}
	std::map<uint32, std::list<TempMerchantList> >::iterator cur;
	uint32 npcid = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		TempMerchantList ml;
		ml.npcid = atoul(row[0]);
		if (npcid != ml.npcid){
			cur = tmpmerchanttable.find(ml.npcid);
			if (cur == tmpmerchanttable.end()) {
				std::list<TempMerchantList> empty;
				tmpmerchanttable[ml.npcid] = empty;
				cur = tmpmerchanttable.find(ml.npcid);
			}
			npcid = ml.npcid;
		}
		ml.slot = atoul(row[1]);
		ml.charges = atoul(row[2]);
		ml.item = atoul(row[3]);
		ml.origslot = ml.slot;
		ml.quantity = atoul(row[4]);
		if (ml.charges > MERCHANT_CHARGE_CAP)
		{
			// The client has an internal limit of items per stack.
			ml.charges = MERCHANT_CHARGE_CAP;
			database.SaveMerchantTemp(npcid, ml.origslot, ml.item, ml.charges, ml.quantity);
			LogInfo("Adjusted item {} on temporary merchant list ({}) to quantity {}.", ml.item, npcid, MERCHANT_CHARGE_CAP);
		}
		cur->second.push_back(ml);
	}
	pQueuedMerchantsWorkID = 0;
}

void Zone::LoadNewMerchantData(uint32 merchantid) {

	LogInfo("Merchant: {} is loading...", merchantid);

	std::list<MerchantList> merchant_list;

	auto query = fmt::format(
		SQL(
			SELECT 
				item, 
				slot, 
				faction_required, 
				level_required,
				classes_required, 
				quantity
			FROM 
				merchantlist 
			WHERE 
				merchantid = {}
				{}
			ORDER BY slot
			),
			merchantid,
			ContentFilterCriteria::apply()
	);

    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for(auto row : results) {
        MerchantList ml;
        ml.id               = merchantid;
        ml.item             = std::stoul(row[0]);
        ml.slot             = std::stoul(row[1]);
		ml.faction_required = static_cast<int16>(std::stoi(row[2]));
		ml.level_required   = static_cast<int8>(std::stoul(row[3]));
        ml.classes_required = std::stoul(row[4]);
		ml.quantity         = static_cast<int8>(std::stoul(row[5]));

		if (ml.quantity > 0) {
			ml.qty_left = ml.quantity;
		} else {
			ml.qty_left = 0;
		}
		merchant_list.push_back(ml);
    }

	db_merchanttable[merchantid] = merchant_list;
	merchanttable[merchantid] = merchant_list;
}

void Zone::GetMerchantDataForZoneLoad() {
	LogInfo("Loading Merchant Lists...");

	auto query = fmt::format(
		SQL(
			SELECT
				merchantlist.merchantid,
				merchantlist.slot,
				merchantlist.item,
				merchantlist.faction_required,
				merchantlist.level_required,
				merchantlist.classes_required,
				merchantlist.quantity
			FROM
				merchantlist,
				npc_types,
				spawnentry,
				spawn2
			WHERE
				merchantlist.merchantid = npc_types.merchant_id
				AND npc_types.id = spawnentry.npcID
				AND spawn2.spawngroupID = spawnentry.spawngroupID
				and spawn2.zone = '{}'
				{}
			ORDER BY
			merchantlist.slot
		),
		GetShortName(),
		ContentFilterCriteria::apply("merchantlist")
	);

	auto results = database.QueryDatabase(query);

	std::map<uint32, std::list<MerchantList> >::iterator merchant_list;

	uint32 npcid = 0;
	if (!results.Success() || !results.RowCount()) {
		LogDebug("No Merchant Data found for [{}]", GetShortName());
		return;
	}

	for (auto row : results) {
		MerchantList mle{};
		mle.id = std::stoul(row[0]);
		if (npcid != mle.id) {
			merchant_list = merchanttable.find(mle.id);
			if (merchant_list == merchanttable.end()) {
				std::list<MerchantList> empty;
				merchanttable[mle.id] = empty;
				merchant_list = merchanttable.find(mle.id);
			}
			npcid = mle.id;
		}

		bool found = false;
		for (const auto& m : merchant_list->second) {
			if (m.item == mle.id) {
				found = true;
				break;
			}
		}

		if (found) {
			continue;
		}

		mle.slot = std::stoul(row[1]);
		mle.item = std::stoul(row[2]);
		mle.faction_required = static_cast<int16>(std::stoi(row[3]));
		mle.level_required = static_cast<int8>(std::stoul(row[4]));
		mle.classes_required = std::stoul(row[5]);
		mle.quantity = static_cast<int8>(std::stoul(row[6]));
		if(mle.quantity > 0)
			mle.qty_left = mle.quantity;
		else
			mle.qty_left = 0;
		merchant_list->second.push_back(mle);
	}
}

uint32 Zone::GetSelfFoundPurchaseLimit(uint32 npctype_id, uint32 item, uint32 self_found_character_id)
{
	if (self_found_character_id == 0)
		return 0;

	auto merchant_iter = tmpmerchanttable_ssf_purchase_limits.find(npctype_id);
	if (merchant_iter == tmpmerchanttable_ssf_purchase_limits.end())
		return 0;

	MerchantSsfPurchaseLimits& merchant_limits = merchant_iter->second;

	SsfItemKey key(item, self_found_character_id);
	auto item_iter = merchant_limits.find(key);
	if (item_iter == merchant_limits.end())
		return 0;

	return item_iter->second;
}

uint32 Zone::IncreaseSelfFoundPurchaseLimit(uint32 npctype_id, uint32 item, uint32 self_found_character_id, uint32 increment_by)
{
	if (self_found_character_id == 0)
		return 0;

	MerchantSsfPurchaseLimits& merchant_limits = tmpmerchanttable_ssf_purchase_limits[npctype_id];

	SsfItemKey key(item, self_found_character_id);
	auto item_iter = merchant_limits.find(key);

	if (item_iter == merchant_limits.end()) {
		if (increment_by > 0)
			merchant_limits[key] = increment_by;
		return increment_by;
	}

	uint32 newqty = item_iter->second + increment_by;
	if (newqty < item_iter->second) // handle overflow
		newqty = UINT32_MAX;
	item_iter->second = newqty;
	return newqty;
}

uint32 Zone::DecreaseSelfFoundPurchaseLimit(uint32 npctype_id, uint32 item, uint32 self_found_character_id, uint32 decrement_by)
{
	if (self_found_character_id == 0)
		return 0;

	auto merchant_iter = tmpmerchanttable_ssf_purchase_limits.find(npctype_id);
	if (merchant_iter == tmpmerchanttable_ssf_purchase_limits.end())
		return 0;

	MerchantSsfPurchaseLimits& merchant_limits = merchant_iter->second;

	SsfItemKey key(item, self_found_character_id);
	auto item_iter = merchant_limits.find(key);
	if (item_iter == merchant_limits.end())
		return 0;

	if (decrement_by >= item_iter->second) {
		merchant_limits.erase(item_iter);
		return 0;
	}

	item_iter->second -= decrement_by;
	return item_iter->second;
}

void Zone::CapSelfFoundPurchaseLimitsByAvailableQuantity(uint32 npctype_id, uint32 item, uint32 quantity_in_stock)
{
	auto merchant_iter = tmpmerchanttable_ssf_purchase_limits.find(npctype_id);
	if (merchant_iter == tmpmerchanttable_ssf_purchase_limits.end())
		return;

	MerchantSsfPurchaseLimits& merchant_limits = merchant_iter->second;

	for (auto item_iter = merchant_limits.begin(); item_iter != merchant_limits.end();) {
		if (item_iter->first.item_id == item) {
			if (quantity_in_stock == 0)
			{
				item_iter = merchant_limits.erase(item_iter);
				continue;
			}
			else if (quantity_in_stock < item_iter->second)
			{
				item_iter->second = quantity_in_stock;
			}
		}
		item_iter++;
	}
}

void Zone::ClearMerchantLists()
{
	std::string query = StringFormat("SELECT nt.id "
									" FROM npc_types AS nt, "
									" merchantlist AS ml, "
									" spawnentry AS se, "
									" spawn2 AS s2 "
									" WHERE nt.merchant_id = ml.merchantid "
									" AND nt.id = se.npcid AND se.spawngroupid = s2.spawngroupid "
									" AND s2.zone = '%s' "
									" GROUP by nt.id", GetShortName());

	auto results = database.QueryDatabase(query);
	if (results.RowCount() == 0) {
		LogInfo("No Merchant Data found.");
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row) 
	{
		int32 id = atoul(row[0]);
		merchanttable[id].clear();
		tmpmerchanttable[id].clear();
		tmpmerchanttable_ssf_purchase_limits[id].clear();
	}
}

void Zone::LoadLevelEXPMods(){
	level_exp_mod.clear();
    const std::string query = "SELECT level, exp_mod, aa_exp_mod FROM level_exp_mods";
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        LogError("Error in ZoneDatabase::LoadEXPLevelMods()");
        return;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        uint32 index = atoi(row[0]);
		float exp_mod = atof(row[1]);
		float aa_exp_mod = atof(row[2]);
		level_exp_mod[index].ExpMod = exp_mod;
		level_exp_mod[index].AAExpMod = aa_exp_mod;
    }

}


bool Zone::IsLoaded() {
	return is_zone_loaded;
}

void Zone::Shutdown(bool quiet)
{
	if (!is_zone_loaded) {
		return;
	}

	entity_list.StopMobAI();

	std::map<uint32,NPCType *>::iterator itr;
	while(!zone->npctable.empty()) {
		itr=zone->npctable.begin();
		delete itr->second;
		itr->second = nullptr;
		zone->npctable.erase(itr);
	}

	LogInfo("Zone Shutdown: {} ({})", zone->GetShortName(), zone->GetZoneID());
	petition_list.ClearPetitions();
	zone->SetZoneHasCurrentTime(false);

	if (!quiet) {
		LogInfo("Zone shutdown: going to sleep");
	}

	is_zone_loaded = false;

	zone->ResetAuth();
	safe_delete(zone);
	entity_list.ClearAreas();
	parse->ReloadQuests(true);
	UpdateWindowTitle(nullptr);

	LogSys.CloseFileLogs();

	if (RuleB(Zone, KillProcessOnDynamicShutdown)) {
		LogInfo("Shutting down");
		EQ::EventLoop::Get().Shutdown();
	}
}

void Zone::LoadZoneDoors()
{
	LogInfo("Loading doors for {} ...", GetShortName());

	auto door_entries = database.LoadDoors(GetShortName());
	if (door_entries.empty()) {
		LogInfo("... No doors loaded.");
		return;
	}
	
	for (const auto &entry : door_entries)
	{
		auto newdoor = new Doors(entry);
		
		if (GetGuildID() == GUILD_NONE && newdoor->IsInstanceOnly()) 
		{
			safe_delete(newdoor);
			continue;
		}
		entity_list.AddDoor(newdoor);
		LogInfo("Door added to entity list, db id: [{}], door_id: [{}]", entry.id, entry.doorid);
	}
}

Zone::Zone(uint32 in_zoneid, const char* in_short_name, uint32 in_guildid)
:	initgrids_timer(10000),
	autoshutdown_timer((RuleI(Zone, AutoShutdownDelay))),
	clientauth_timer(AUTHENTICATION_TIMEOUT * 1000),
	spawn2_timer(1000),
	hot_reload_timer(1000),
	qglobal_purge_timer(30000),
	m_safe_point(0.0f,0.0f,0.0f,0.0f),
	m_graveyard(0.0f,0.0f,0.0f,0.0f)
{
	zoneid = in_zoneid;
	guildid = in_guildid;
	zonemap = nullptr;
	watermap = nullptr;
	pathing = nullptr;
	qGlobals = nullptr;
	default_ruleset = 0;

	is_zone_time_localized = false;

	process_mobs_while_empty = false;

	loglevelvar = 0;
	merchantvar = 0;
	tradevar = 0;
	lootvar = 0;

	memset(&last_quake_struct, 0, sizeof(ServerEarthquakeImminent_Struct));
	memset(&zone_banish_point, 0, sizeof(ZoneBanishPoint));

	short_name = strcpy(new char[strlen(in_short_name)+1], in_short_name);
	strlwr(short_name);
	memset(file_name, 0, sizeof(file_name));

	long_name = 0;
	aggroed_npcs = 0;
	m_graveyard_id = 0;
	m_graveyard_zoneid = 0;
	m_graveyard_timer = 0;
	m_max_clients = 0;
	pQueuedMerchantsWorkID = 0;
	pvpzone = false;
	if(database.GetServerType() == 1)
		pvpzone = true;

	database.GetZoneLongName(short_name, &long_name, file_name, &m_safe_point.x, &m_safe_point.y, &m_safe_point.z, &m_graveyard_id, &m_graveyard_timer, &m_max_clients);
	if(graveyard_id() > 0)
	{
		LogInfo("Graveyard ID is {}.", graveyard_id());

		if (guildid == 1)
			m_graveyard_timer = RuleI(Quarm, PVPInstanceGraveyardTime);

		bool GraveYardLoaded = database.GetZoneGraveyard(graveyard_id(), &m_graveyard_zoneid, &m_graveyard.x, &m_graveyard.y, &m_graveyard.z, &m_graveyard.w);
		if (GraveYardLoaded)
		{
			LogInfo("Loaded a graveyard for zone {}: graveyard zoneid is {} at {}. graveyard timer is {} minutes before corpse is sent to graveyard.", short_name, graveyard_zoneid(), to_string(m_graveyard).c_str(), graveyard_timer());
		}
		else
		{
			LogError("Unable to load the graveyard id {} for zone {}.", graveyard_id(), short_name);
		}
	}
	if (long_name == 0) {
		long_name = strcpy(new char[18], "Long zone missing");
	}
	Weather_Timer = new Timer(60000);
	Weather_Timer->Start();
	EndQuake_Timer = new Timer(60000);
	EndQuake_Timer->Disable();
	LogInfo("The next weather check for zone: {} will be in {} seconds.", short_name, Weather_Timer->GetRemainingTime() / 1000);
	zone_weather = 0;
	weather_intensity = 0;
	blocked_spells = nullptr;
	zone_total_blocked_spells = 0;
	reducedspawntimers = false;
	trivial_loot_code = false;
	aas = nullptr;
	totalAAs = 0;
	zone_has_current_time = false;
	idle = false;

	map_name = nullptr;
	database.QGlobalPurge();
	mMovementManager = &MobMovementManager::Get();
	HasCharmedNPC = false;
	SetQuestHotReloadQueued(false);
	nexus_timer_step = 0;
	velious_timer_step = 0;
	velious_active = true;
	Nexus_Scion_Timer = nullptr;
	Nexus_Portal_Timer = nullptr;
	if(RuleB(Zone, EnableNexusPortals) || (RuleB(Zone, EnableNexusPortalsOnExpansion) && content_service.IsTheShadowsOfLuclinEnabled())) {
		if(GetZoneID() == Zones::NEXUS) {
			Nexus_Scion_Timer = new Timer(RuleI(Zone, NexusTimer));
			Nexus_Scion_Timer->Start();
			Log(Logs::General, Logs::Nexus, "Setting Velious portal timer to %d", RuleI(Zone, NexusTimer));

			Nexus_Portal_Timer = new Timer(RuleI(Zone, NexusTimer));
			if(RuleI(Zone, NexusTimer) < 900000) {
				Nexus_Portal_Timer->Start();
				Log(Logs::General, Logs::Nexus, "Starting Nexus timer without delay, due to timer being set to %d", RuleI(Zone, NexusTimer));
			} else {
				Nexus_Portal_Timer->Disable();
			}

		} else if(IsNexusScionZone()) {
			Nexus_Scion_Timer = new Timer(RuleI(Zone, NexusScionTimer));
			Nexus_Scion_Timer->Start();
			Log(Logs::General, Logs::Nexus, "Setting Nexus scion timer to %d", RuleI(Zone, NexusScionTimer));
		}
	}

	m_ucss_available = false;
	m_last_ucss_update = 0;

}

Zone::~Zone() {

	spawn2_list.Clear();
	safe_delete(zonemap);
	safe_delete(watermap);
	safe_delete(pathing);
	if (worldserver.Connected()) {
		worldserver.SetZoneData(0, guildid);
	}
	safe_delete_array(short_name);
	safe_delete_array(long_name);
	safe_delete(Weather_Timer);
	safe_delete(EndQuake_Timer);
	ClearNPCEmotes(&npc_emote_list);
	zone_point_list.Clear();
	entity_list.Clear();
	ClearBlockedSpells();

	safe_delete(qGlobals);
	safe_delete_array(map_name);
	safe_delete(Nexus_Scion_Timer);
	safe_delete(Nexus_Portal_Timer);

	if(aas != nullptr) {
		int r;
		for(r = 0; r < totalAAs; r++) {
			uchar *data = (uchar *) aas[r];
			safe_delete_array(data);
		}
		safe_delete_array(aas);
	}

}

//Modified for timezones.
bool Zone::Init(bool is_static) {
	SetStaticZone(is_static);

	//load the zone config file.
	if (!LoadZoneCFG(GetShortName())) { // try loading the zone name...
		LoadZoneCFG(GetFileName());
	}// if that fails, try the file name, then load defaults
	if (guildid == 1)
	{
		pvpzone = true;
		reducedspawntimers = false;
	}

	if (RuleManager::Instance()->GetActiveRulesetID() != default_ruleset)
	{
		std::string r_name = RuleSetsRepository::GetRuleSetName(database, default_ruleset);
		if (r_name.size() > 0)
		{
			RuleManager::Instance()->LoadRules(&database, r_name.c_str());
		}
	}

	if (!map_name) {
		LogError("No map name found for zone [{}]", GetShortName());
		return false;
	}

	zonemap = Map::LoadMapFile(map_name);
	watermap = WaterMap::LoadWaterMapfile(map_name);
	pathing = IPathfinder::Load(map_name);

	update_range = 1000.0f;

	LogInfo("Loading timezone data...");
	zone_time.setEQTimeZone(database.GetZoneTZ(zoneid));

	database.LoadGlobalLoot();

	LoadGrids();

	if (RuleB(Zone, LevelBasedEXPMods)) {
		LoadLevelEXPMods();
	}

	LogInfo("Flushing old respawn timers...");
	database.QueryDatabase("DELETE FROM `respawn_times` WHERE (`start` + `duration`) < UNIX_TIMESTAMP(NOW())");

	// make sure that anything that needs to be loaded prior to scripts is loaded before here
	// this is to ensure that the scripts have access to the data they need
	parse->ReloadQuests(true);

	LogInfo("Loading spawn conditions...");
	if(!spawn_conditions.LoadSpawnConditions(short_name, GetGuildID())) {
		LogError("Loading spawn conditions failed, continuing without them.");
	}

	if (!database.LoadStaticZonePoints(&zone_point_list, short_name)) {
		LogError("Loading static zone points failed.");
		return false;
	}

	if (!database.LoadSpawnGroups(short_name, &spawn_group_list)) {
		LogError("Loading spawn groups failed.");
		return false;
	}

	LogInfo("Loading spawn2 points...");
	if (!database.PopulateZoneSpawnList(zoneid, spawn2_list, GetGuildID()))
	{
		LogError("Loading spawn2 points failed.");
		return false;
	}

	if (!database.PopulateRandomZoneSpawnList(zoneid, spawn2_list))
	{
		LogError("Loading random box spawns failed (Possibly over ID limit.)");
	}

	LogInfo("Loading player corpses...");
	if (!database.LoadCharacterCorpses(zoneid, GetGuildID())) {
		LogError("Loading player corpses failed.");
		return false;
	}

	LogInfo("Loading traps...");
	if (!database.LoadTraps(short_name))
	{
		LogError("Loading traps failed.");
		return false;
	}

	LogInfo("Loading ground spawns...");
	if (!LoadGroundSpawns())
	{
		LogError("Loading ground spawns failed. continuing.");
	}

	LogInfo("Loading World Objects from DB...");
	if (!LoadZoneObjects())
	{
		LogError("Loading World Objects failed. continuing.");
	}

	//load up the zone's doors (prints inside)
	LoadZoneDoors();
	LoadZoneBlockedSpells();
	LoadZoneBanishPoint(zone->GetShortName());
	LoadNPCEmotes(&npc_emote_list);
	LoadAlternateAdvancement();
	GetMerchantDataForZoneLoad();
	LoadTempMerchantData();
	LoadKeyRingData(&key_ring_data_list);
	LoadTickItems();

	skill_difficulty.clear();
	LoadSkillDifficulty();

	//clear trader items if we are loading the bazaar
	if(strncasecmp(short_name,"bazaar",6)==0) {
		database.DeleteTraderItem(0);
	}
	
	petition_list.ClearPetitions();
	petition_list.ReadDatabase();

	LogInfo("Zone booted successfully zone_id [{}] time_offset [{}]", zoneid, zone_time.getEQTimeZone());

	//database.LoadQuakeData(zone->last_quake_struct);
	if (newzone_data.maxclip > 0.0f) {
		update_range = std::max(250.0f, newzone_data.maxclip + 50.0f);
	}

	update_range *= update_range;

	// logging origination information
	LogSys.origination_info.zone_short_name = zone->short_name;
	LogSys.origination_info.zone_long_name = zone->long_name;

	return true;
}

void Zone::ReloadStaticData() {
	LogInfo("Reloading Zone Static Data...");

	LogInfo("Reloading static zone points...");
	zone_point_list.Clear();

	if (!database.LoadStaticZonePoints(&zone_point_list, GetShortName())) {
		LogError("Loading static zone points failed.");
	}

	LogInfo("Reloading traps...");
	entity_list.RemoveAllTraps();
	if (!database.LoadTraps(GetShortName()))
	{
		LogError("Reloading traps failed.");
	}

	entity_list.RemoveAllObjects();

	LogInfo("Reloading ground spawns...");
	if (!LoadGroundSpawns())
	{
		LogError("Reloading ground spawns failed. continuing.");
	}

	LogInfo("Reloading World Objects from DB...");
	if (!LoadZoneObjects())
	{
		LogError("Reloading World Objects failed. continuing.");
	}

	entity_list.RemoveAllDoors();
	LoadZoneDoors();
	entity_list.RespawnAllDoors();

	LogInfo("Reloading NPC Emote Data...");
	LoadNPCEmotes(&npc_emote_list);

	LogInfo("Reloading KeyRing Data...");
	key_ring_data_list.Clear();
	LoadKeyRingData(&key_ring_data_list);

	LogInfo("Reloading Zone Data...");
	database.GetZoneLongName(short_name, &long_name, file_name, &m_safe_point.x, &m_safe_point.y, &m_safe_point.z, &m_graveyard_id, &m_graveyard_timer, &m_max_clients);
	if (guildid == 1 && m_graveyard_id != 0)
		m_graveyard_timer = RuleI(Quarm, PVPInstanceGraveyardTime);
	//load the zone config file.
	if (!LoadZoneCFG(GetShortName())) { // try loading the zone name...
		LoadZoneCFG(GetFileName());
	} // if that fails, try the file name, then load defaults

	content_service.SetExpansionContext()->ReloadContentFlags();

	ReloadLootTables();

	LogInfo("Zone Static Data Reloaded.");
}

bool Zone::LoadZoneCFG(const char* filename)
{
	memset(&newzone_data, 0, sizeof(NewZone_Struct));

	safe_delete_array(map_name);

	if (!database.GetZoneCFG(ZoneID(filename), &newzone_data, can_bind,
		can_combat, can_levitate, can_castoutdoor, is_city, zone_type, default_ruleset, &map_name, can_bind_others, skip_los, drag_aggro, can_castdungeon, pull_limit,reducedspawntimers, trivial_loot_code, is_hotzone))
	{
		LogError("Error loading the Zone Config.");
		return false;
	}

	//overwrite with our internal variables
	strcpy(newzone_data.zone_short_name, GetShortName());
	strcpy(newzone_data.zone_long_name, GetLongName());
	strcpy(newzone_data.zone_short_name2, GetShortName());

	LogInfo("Successfully loaded Zone Config.");
	return true;
}

bool Zone::SaveZoneCFG() {
	return database.SaveZoneCFG(GetZoneID(), &newzone_data);
}

void Zone::SetIsHotzone(bool is_hotzone)
{
	Zone::is_hotzone = is_hotzone;
}

void Zone::AddAuth(ServerZoneIncomingClient_Struct* szic) {
	auto zca = new ZoneClientAuth_Struct;
	memset(zca, 0, sizeof(ZoneClientAuth_Struct));
	zca->ip = szic->ip;
	zca->wid = szic->wid;
	zca->accid = szic->accid;
	zca->admin = szic->admin;
	zca->charid = szic->charid;
	zca->tellsoff = szic->tellsoff;
	strn0cpy(zca->charname, szic->charname, sizeof(zca->charname));
	strn0cpy(zca->lskey, szic->lskey, sizeof(zca->lskey));
	zca->stale = false;
	client_auth_list.Insert(zca);
	zca->version = szic->version;
}

void Zone::RemoveAuth(const char* iCharName, uint32 entity_id)
{
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (strcasecmp(zca->charname, iCharName) == 0 && zca->entity_id == entity_id) {
			iterator.RemoveCurrent();
			return;
		}
		iterator.Advance();
	}
}

void Zone::RemoveAuth(const char* iCharName, const char* iLSKey)
{
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (strcasecmp(zca->charname, iCharName) == 0 && strcasecmp(zca->lskey, iLSKey) == 0) {
			iterator.RemoveCurrent();
			return;
		}
		iterator.Advance();
	}
}

void Zone::RemoveAuth(uint32 lsid)
{
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (zca->lsid == lsid) {
			iterator.RemoveCurrent();
			continue;
		}
		iterator.Advance();
	}
}

void Zone::ResetAuth()
{
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		iterator.RemoveCurrent();
	}
}

bool Zone::CheckAuth(const char* iCharName) {
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (strcasecmp(zca->charname, iCharName) == 0) {
			return true;
		}
		iterator.Advance();
	}
	return false;
}

bool Zone::GetAuth(uint32 iIP, const char* iCharName, uint32* oWID, uint32* oAccID, uint32* oCharID, int16* oStatus, char* oLSKey, bool* oTellsOff, uint32* oVersionbit, uint32 entity_id) {
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (strcasecmp(zca->charname, iCharName) == 0) {
			if (oWID)
				*oWID = zca->wid;
			if (oAccID)
				*oAccID = zca->accid;
			if (oCharID)
				*oCharID = zca->charid;
			if (oStatus)
				*oStatus = zca->admin;
			if (oTellsOff)
				*oTellsOff = zca->tellsoff;
			zca->stale = true;
			if (oVersionbit)
				*oVersionbit = zca->version;
			if (entity_id)
				zca->entity_id = entity_id;
			return true;
		}
		iterator.Advance();
	}
	return false;
}

uint32 Zone::CountAuth() {
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	int x = 0;
	iterator.Reset();
	while (iterator.MoreElements()) {
		x++;
		iterator.Advance();
	}
	return x;
}

bool Zone::Process() {
	spawn_conditions.Process();

	if(spawn2_timer.Check()) {
		LinkedListIterator<Spawn2*> iterator(spawn2_list);

		EQ::InventoryProfile::CleanDirty();

		LogSpawns("Running Zone::Process -> Spawn2::Process");

		iterator.Reset();
		while (iterator.MoreElements()) {
			if (iterator.GetData()->Process()) {
				iterator.Advance();
			}
			else {
				iterator.RemoveCurrent();
			}
		}
	}

	if (hot_reload_timer.Check() && IsQuestHotReloadQueued()) {

		LogHotReloadDetail("Hot reload timer check...");

		bool perform_reload = true;
		
		if (RuleB(HotReload, QuestsRepopWhenPlayersNotInCombat)) {
			for (auto& it : entity_list.GetClientList()) {
				auto client = it.second;
				if (client->GetAggroCount() > 0) {
					perform_reload = false;
					break;
				}
			}
		}

		if (perform_reload) {
			ZoneReload::HotReloadQuests();
		}
	}

	if (initgrids_timer.Check()) {
		//delayed grid loading stuff.
		initgrids_timer.Disable();
		LinkedListIterator<Spawn2*> iterator(spawn2_list);

		iterator.Reset();
		while (iterator.MoreElements()) {
			iterator.GetData()->LoadGrid();
			iterator.Advance();
		}
	}

	if(!staticzone) {
		if (autoshutdown_timer.Check()) {
			ResetShutdownTimer();
			if (numclients == 0) {
				return false;
			}
		}
	}

	if(Weather_Timer->Check())
	{
		Weather_Timer->Disable();
		this->ChangeWeather();
	}

	if (EndQuake_Timer->Check())
	{
		uint32 cur_time = Timer::GetTimeSeconds();
		bool should_broadcast_notif = zone->ResetEngageNotificationTargets((RuleI(Quarm, QuakeMaxVariance) * 2) * 1000, true); // if we reset at least one, this is true
		if (should_broadcast_notif)
		{
			entity_list.Message(Chat::Default, Chat::Yellow, "The quake has concluded. Rules 9.x and 10.x will once again apply where relevant.");
		}
		entity_list.TogglePVPForQuake();
		EndQuake_Timer->Disable();
		memset(&last_quake_struct, 0, sizeof(ServerEarthquakeImminent_Struct));

	}

	if(qGlobals)
	{
		if(qglobal_purge_timer.Check())
		{
			qGlobals->PurgeExpiredGlobals();
		}
	}

	if (clientauth_timer.Check()) {
		LinkedListIterator<ZoneClientAuth_Struct*> iterator2(client_auth_list);

		iterator2.Reset();
		while (iterator2.MoreElements()) {
			if (iterator2.GetData()->stale)
				iterator2.RemoveCurrent();
			else {
				iterator2.GetData()->stale = true;
				iterator2.Advance();
			}
		}
	}

	if((RuleB(Zone, EnableNexusPortals) || (RuleB(Zone, EnableNexusPortalsOnExpansion) && content_service.IsTheShadowsOfLuclinEnabled())) && (IsNexusScionZone() || GetZoneID() == Zones::NEXUS)) {
		NexusProcess();
	}

	mMovementManager->Process();

	return true;
}

void Zone::ChangeWeather()
{
	if(!HasWeather())
	{
		zone_weather = 0;
		weather_intensity = 0;
		weatherSend();
		Weather_Timer->Disable();
		return;
	}

	int chance = zone->random.Int(0, 3);
	uint8 rainchance = zone->newzone_data.rain_chance[chance];
	uint8 rainduration = zone->newzone_data.rain_duration[chance];
	uint8 snowchance = zone->newzone_data.snow_chance[chance];
	uint8 snowduration = zone->newzone_data.snow_duration[chance];
	uint32 weathertimer = 0;
	uint16 tmpweather = zone->random.Int(0, 100);
	uint8 duration = 0;
	uint8 tmpOldWeather = zone->zone_weather;
	bool changed = false;

	if(tmpOldWeather == 0)
	{
		if(rainchance > 0 || snowchance > 0)
		{
			uint8 intensity = zone->random.Int(1, 3);
			if((rainchance > snowchance) || (rainchance == snowchance))
			{
				//It's gunna rain!
				if(rainchance >= tmpweather)
				{
					if(rainduration == 0)
						duration = 1;
					else
						duration = rainduration*3; //Duration is 1 EQ hour which is 3 earth minutes.

					weathertimer = (duration*60)*1000;
					Weather_Timer->Start(weathertimer);
					zone->zone_weather = 1;
					zone->weather_intensity = intensity;
					changed = true;
				}
			}
			else
			{
				//It's gunna snow!
				if(snowchance >= tmpweather)
				{
					if(snowduration == 0)
						duration = 1;
					else
						duration = snowduration*3;
					weathertimer = (duration*60)*1000;
					Weather_Timer->Start(weathertimer);
					zone->zone_weather = 2;
					zone->weather_intensity = intensity;
					changed = true;
				}
			}
		}
	}
	else
	{
		changed = true;
		//We've had weather, now taking a break
		if(tmpOldWeather == 1)
		{
			if(rainduration == 0)
				duration = 1;
			else
				duration = rainduration*3; //Duration is 1 EQ hour which is 3 earth minutes.

			weathertimer = (duration*60)*1000;
			Weather_Timer->Start(weathertimer);
			zone->zone_weather = 0;
			zone->weather_intensity = 0;
		}
		else if(tmpOldWeather == 2)
		{
			if(snowduration == 0)
				duration = 1;
			else
				duration = snowduration*3; //Duration is 1 EQ hour which is 3 earth minutes.

			weathertimer = (duration*60)*1000;
			Weather_Timer->Start(weathertimer);
			zone->zone_weather = 0;
			zone->weather_intensity = 0;
		}
	}

	if(changed == false)
	{
		if(weathertimer == 0)
		{
			uint32 weatherTimerRule = RuleI(Zone, WeatherTimer);
			weathertimer = weatherTimerRule*1000;
			Weather_Timer->Start(weathertimer);
		}
		LogInfo("The next weather check for zone: {} will be in {} seconds.", zone->GetShortName(), Weather_Timer->GetRemainingTime()/1000);
	}
	else
	{
		LogInfo("The weather for zone: {} has changed. Old weather was = {}. New weather is = {} The next check will be in {} seconds. Rain chance: {}, Rain duration: {}, Snow chance {}, Snow duration: {} ", zone->GetShortName(), tmpOldWeather, zone_weather,Weather_Timer->GetRemainingTime()/1000,rainchance,rainduration,snowchance,snowduration);
		this->weatherSend();
	}
}

bool Zone::HasWeather()
{
	uint8 rain1 = zone->newzone_data.rain_chance[0];
	uint8 rain2 = zone->newzone_data.rain_chance[1];
	uint8 rain3 = zone->newzone_data.rain_chance[2];
	uint8 rain4 = zone->newzone_data.rain_chance[3];
	uint8 snow1 = zone->newzone_data.snow_chance[0];
	uint8 snow2 = zone->newzone_data.snow_chance[1];
	uint8 snow3 = zone->newzone_data.snow_chance[2];
	uint8 snow4 = zone->newzone_data.snow_chance[3];

	if(rain1 == 0 && rain2 == 0 && rain3 == 0 && rain4 == 0 && snow1 == 0 && snow2 == 0 && snow3 == 0 && snow4 == 0)
		return false;
	else
		return true;
}

bool Zone::IsSpecialWeatherZone()
{
	if(GetZoneID() == Zones::OOT || GetZoneID() >= Zones::SHADOWHAVEN) {
		return true;
	}

	return false;
}

void Zone::StartShutdownTimer(uint32 set_time)
{
	// if we pass in the default value, we should pull from the zone and use it is different
	std::string loaded_from = "rules";
	if (set_time == (RuleI(Zone, AutoShutdownDelay))) {
		auto delay = database.getZoneShutDownDelay(GetZoneID());

		if (delay != RuleI(Zone, AutoShutdownDelay)) {
			set_time = delay;
			loaded_from = "zone table";
		}
	}

	if (set_time != autoshutdown_timer.GetDuration()) {
		LogInfo(
			"[StartShutdownTimer] Reset to [{}] {} from original remaining time [{}] duration [{}] zone [{}]",
			Strings::SecondsToTime(set_time, true),
			!loaded_from.empty() ? fmt::format("(Loaded from [{}])", loaded_from) : "",
			Strings::SecondsToTime(autoshutdown_timer.GetRemainingTime(), true),
			Strings::SecondsToTime(autoshutdown_timer.GetDuration(), true),
			zone->GetZoneDescription()
		);
	}
	autoshutdown_timer.Start(set_time, false);
}

void Zone::ResetShutdownTimer() {
	LogInfo(
		"[ResetShutdownTimer] Reset to [{}] from original remaining time [{}] duration [{}] zone [{}]",
		Strings::SecondsToTime(autoshutdown_timer.GetDuration(), true),
		Strings::SecondsToTime(autoshutdown_timer.GetRemainingTime(), true),
		Strings::SecondsToTime(autoshutdown_timer.GetDuration(), true),
		zone->GetZoneDescription()
	);
	autoshutdown_timer.Start(autoshutdown_timer.GetDuration());
}

void Zone::StopShutdownTimer() {
	LogInfo("Stopping zone shutdown timer");
	autoshutdown_timer.Disable();
}
bool Zone::IsReducedSpawnTimersEnabled() 
{
	if (!RuleB(Quarm, EnableRespawnReductionSystem))
	{
		return false;
	}
	return reducedspawntimers;
}

uint16 Zone::GetPullLimit()
{
	if (!RuleB(Quarm, EnableRespawnReductionSystem))
	{
		return pull_limit;
	}

	if (IsReducedSpawnTimersEnabled())
	{
		if (CanCastDungeon())
		{
			return RuleI(Quarm, RespawnReductionDungeonPullLimit);
		}
		return RuleI(Quarm, RespawnReductionNewbiePullLimit);
	}


	return RuleI(Quarm, RespawnReductionStandardPullLimit);
}

bool Zone::Depop(bool StartSpawnTimer) {
	std::map<uint32,NPCType *>::iterator itr;
	entity_list.Depop(StartSpawnTimer);
	entity_list.ClearTrapPointers();
	entity_list.UpdateAllTraps(false);

	/* Refresh npctable (cache), getting current info from database. */
	while (!npctable.empty()) {
		itr = npctable.begin();
		delete itr->second;
		itr->second = nullptr;
		npctable.erase(itr);
	}

	// clear spell cache
	database.ClearNPCSpells();

	zone->spawn_group_list.ReloadSpawnGroups();

	return true;
}

void Zone::ClearNPCTypeCache(int id) {
	if (id <= 0) {
		auto iter = npctable.begin();
		while (iter != npctable.end()) {
			delete iter->second;
			++iter;
		}
		npctable.clear();
	}
	else {
		auto iter = npctable.begin();
		while (iter != npctable.end()) {
			if (iter->first == (uint32)id) {
				delete iter->second;
				npctable.erase(iter);
				return;
			}
			++iter;
		}
	}
}

void Zone::RepopClose(const glm::vec4& client_position, uint32 repop_distance)
{

	if (!Depop())
		return;

	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		iterator.RemoveCurrent();
	}

	entity_list.ClearTrapPointers();

	quest_manager.ClearAllTimers();

	if (!database.PopulateZoneSpawnListClose(zoneid, spawn2_list, client_position, repop_distance, GetGuildID()))
		Log(Logs::General, Logs::None, "Error in Zone::Repop: database.PopulateZoneSpawnList failed");

	entity_list.UpdateAllTraps(true, true);
}

bool Zone::ResetEngageNotificationTargets(uint32 in_respawn_timer, bool update_respawn_in_db)
{
	bool reset_at_least_one_spawn2 = false;
	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		Spawn2* pSpawn2 = iterator.GetData();
		if (pSpawn2 && pSpawn2->IsRaidTargetSpawnpoint())
		{
			reset_at_least_one_spawn2 = true;
			if (update_respawn_in_db)
			{
				pSpawn2->QuakeReset();
			}
			else
			{
				pSpawn2->Repop(in_respawn_timer); // milliseconds
			}
		}
		iterator.Advance();
	}
	return reset_at_least_one_spawn2;
}

void Zone::Repop() {

	if(!Depop())
		return;

	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		iterator.RemoveCurrent();
	}

	entity_list.ClearTrapPointers();

	quest_manager.ClearAllTimers();

	LogInfo("Loading spawn groups");
	if (!database.LoadSpawnGroups(short_name, &spawn_group_list)) {
		LogError("Loading spawn groups failed");
	}

	LogInfo("Loading spawn conditions");
	if (!spawn_conditions.LoadSpawnConditions(short_name, GetGuildID())) {
		LogError("Loading spawn conditions failed, continuing without them");
	}

	if (!database.PopulateZoneSpawnList(zoneid, spawn2_list, GetGuildID())) {
		LogError("Error in Zone::Repop: database.PopulateZoneSpawnList failed");
	}

	if (!database.PopulateRandomZoneSpawnList(zoneid, spawn2_list)) {
		LogError("Error in Zone::Repop: database.PopulateRandomZoneSpawnList failed");
	}

	LoadGrids();

	entity_list.UpdateAllTraps(true, true);
}

void Zone::GetTimeSync()
{
	if (!zone_has_current_time) {
		LogInfo("Requesting world time");
		auto pack = new ServerPacket(ServerOP_GetWorldTime, 0);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Zone::SetDate(uint16 year, uint8 month, uint8 day, uint8 hour, uint8 minute)
{
	if (worldserver.Connected()) {
		auto pack = new ServerPacket(ServerOP_SetWorldTime, sizeof(eqTimeOfDay));
		eqTimeOfDay* eqtod = (eqTimeOfDay*)pack->pBuffer;
		eqtod->start_eqtime.minute=minute;
		eqtod->start_eqtime.hour=hour;
		eqtod->start_eqtime.day=day;
		eqtod->start_eqtime.month=month;
		eqtod->start_eqtime.year=year;
		eqtod->start_realtime=time(0);
		LogInfo("Setting master date on world server to: {}-{}-{} {}:{} ({})\n", year, month, day, hour, minute, (int)eqtod->start_realtime);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Zone::SetTime(uint8 hour, uint8 minute, bool update_world /*= true*/)
{
	if (worldserver.Connected()) {
		auto pack = new ServerPacket(ServerOP_SetWorldTime, sizeof(eqTimeOfDay));
		eqTimeOfDay *eq_time_of_day = (eqTimeOfDay*)pack->pBuffer;

		zone_time.GetCurrentEQTimeOfDay(time(0), &eq_time_of_day->start_eqtime);
		eq_time_of_day->start_eqtime.minute=minute;
		eq_time_of_day->start_eqtime.hour=hour;
		eq_time_of_day->start_realtime=time(0);

		/* By Default we update worlds time, but we can optionally no update world which updates the rest of the zone servers */
		if (update_world) {
			LogInfo("Setting master time on world server to: {}:{} ({})\n", hour, minute, (int)eq_time_of_day->start_realtime);
			worldserver.SendPacket(pack);
			/* Set Time Localization Flag */
			zone->is_zone_time_localized = false;
		} 
		/* When we don't update world, we are localizing ourselves, we become disjointed from normal syncs and set time locally */
		else {
			LogInfo("Setting zone localized time...");

			zone->zone_time.SetCurrentEQTimeOfDay(eq_time_of_day->start_eqtime, eq_time_of_day->start_realtime);
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_TimeOfDay, sizeof(TimeOfDay_Struct));
			TimeOfDay_Struct* time_of_day = (TimeOfDay_Struct*)outapp->pBuffer;
			zone->zone_time.GetCurrentEQTimeOfDay(time(0), time_of_day);
			entity_list.QueueClients(0, outapp, false);
			safe_delete(outapp);

			/* Set Time Localization Flag */
			zone->is_zone_time_localized = true;
		}
		safe_delete(pack);
	}
}

ZonePoint* Zone::GetClosestZonePoint(const glm::vec3& location, uint32 to, Client* client, float max_distance) {
	LinkedListIterator<ZonePoint*> iterator(zone_point_list);
	ZonePoint* closest_zp = 0;
	float closest_dist = FLT_MAX;
	float max_distance2 = max_distance * max_distance;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* zp = iterator.GetData();
		uint32 mask_test = client->ClientVersionBit();
		if(!(zp->client_version_mask & mask_test))
		{
			iterator.Advance();
			continue;
		}

		if (zp->target_zone_id == to)
		{
            auto dist = Distance(glm::vec2(zp->x, zp->y), glm::vec2(location));
			if ((zp->x == -BEST_Z_INVALID || zp->x == BEST_Z_INVALID) && (zp->y == -BEST_Z_INVALID || zp->y == BEST_Z_INVALID))
				dist = 0;

			if (dist < closest_dist)
			{
				closest_zp = zp;
				closest_dist = dist;
			}
		}
		iterator.Advance();
	}

	// solar 20231008 commented out, this is spamming up the log for legitimate ports/gates
	// TODO: there are some issues with the client side things like succor and gate failures that may be making this worse and this fallthrough log spam could be useful to find error cases in the above code.
	/*
	if(closest_dist > 400.0f && closest_dist < max_distance2)
	{
		if(client)
			client->CheatDetected(MQZoneUnknownDest, location.x, location.y, location.z); // Someone is trying to use /zone
		LogInfo("WARNING: Closest zone point for zone id {} is {}, you might need to update your zone_points table if you dont arrive at the right spot.", to, closest_dist);
		LogInfo("<Real Zone Points>. {} ", to_string(location).c_str());
	}
	*/

	if(closest_dist > max_distance2)
		closest_zp = nullptr;

	if(!closest_zp)
		closest_zp = GetClosestZonePointWithoutZone(location.x, location.y, location.z, client);

	return closest_zp;
}

ZonePoint* Zone::GetClosestZonePoint(const glm::vec3& location, const char* to_name, Client* client, float max_distance) {
	if(to_name == nullptr)
		return GetClosestZonePointWithoutZone(location.x, location.y, location.z, client, max_distance);
	return GetClosestZonePoint(location, ZoneID(to_name), client, max_distance);
}

ZonePoint* Zone::GetClosestTargetZonePointSameZone(float x, float y, float z, Client* client, float max_distance) {
	LinkedListIterator<ZonePoint*> iterator(zone_point_list);
	ZonePoint* closest_zp = nullptr;
	float closest_dist = FLT_MAX;
	float max_distance2 = max_distance * max_distance;
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZonePoint* zp = iterator.GetData();
		uint32 mask_test = client->ClientVersionBit();

		if (!(zp->client_version_mask & mask_test))
		{
			iterator.Advance();
			continue;
		}

		if (zp->target_zone_id != GetZoneID())
		{
			iterator.Advance();
			continue;
		}

		if (zp->target_x == -BEST_Z_INVALID || zp->target_x == BEST_Z_INVALID || zp->target_y == -BEST_Z_INVALID || zp->target_y == BEST_Z_INVALID)
		{
			iterator.Advance();
			continue;
		}

		float delta_x = zp->target_x - x;
		float delta_y = zp->target_y - y;

		float dist = delta_x * delta_x + delta_y * delta_y;///*+(zp->z-z)*(zp->z-z)*/;
		if (dist < closest_dist)
		{
			closest_zp = zp;
			closest_dist = dist;
		}
		iterator.Advance();
	}
	if (closest_dist > max_distance2)
		closest_zp = nullptr;

	return closest_zp;
}


ZonePoint* Zone::GetClosestZonePointSameZone(float x, float y, float z, Client* client, float max_distance) {
	LinkedListIterator<ZonePoint*> iterator(zone_point_list);
	ZonePoint* closest_zp = nullptr;
	float closest_dist = FLT_MAX;
	float max_distance2 = max_distance * max_distance;
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZonePoint* zp = iterator.GetData();
		uint32 mask_test = client->ClientVersionBit();

		if (!(zp->client_version_mask & mask_test))
		{
			iterator.Advance();
			continue;
		}

		if (zp->target_zone_id != GetZoneID())
		{
			iterator.Advance();
			continue;
		}

		if (zp->x == -BEST_Z_INVALID || zp->x == BEST_Z_INVALID || zp->y == -BEST_Z_INVALID || zp->y == BEST_Z_INVALID)
		{
			iterator.Advance();
			continue;
		}

		float delta_x = zp->x - x;
		float delta_y = zp->y - y;

		float dist = delta_x * delta_x + delta_y * delta_y;///*+(zp->z-z)*(zp->z-z)*/;
		if (dist < closest_dist)
		{
			closest_zp = zp;
			closest_dist = dist;
		}
		iterator.Advance();
	}
	if (closest_dist > max_distance2)
		closest_zp = nullptr;

	return closest_zp;
}

ZonePoint* Zone::GetClosestZonePointWithoutZone(float x, float y, float z, Client* client, float max_distance) {
	LinkedListIterator<ZonePoint*> iterator(zone_point_list);
	ZonePoint* closest_zp = nullptr;
	float closest_dist = FLT_MAX;
	float max_distance2 = max_distance*max_distance;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* zp = iterator.GetData();
		uint32 mask_test = client->ClientVersionBit();

		if(!(zp->client_version_mask & mask_test))
		{
			iterator.Advance();
			continue;
		}

		float delta_x = zp->x - x;
		float delta_y = zp->y - y;
		if(zp->x == -BEST_Z_INVALID || zp->x == BEST_Z_INVALID)
			delta_x = 0;
		if(zp->y == -BEST_Z_INVALID || zp->y == BEST_Z_INVALID)
			delta_y = 0;

		float dist = delta_x*delta_x+delta_y*delta_y;///*+(zp->z-z)*(zp->z-z)*/;
		if (dist < closest_dist)
		{
			closest_zp = zp;
			closest_dist = dist;
		}
		iterator.Advance();
	}
	if(closest_dist > max_distance2)
		closest_zp = nullptr;

	return closest_zp;
}

bool ZoneDatabase::LoadStaticZonePoints(LinkedList<ZonePoint *> *zone_point_list, const char* zonename)
{
	zone_point_list->Clear();
	zone->numzonepoints = 0;
	zone->virtual_zone_point_list.clear();

	auto zone_points = ZonePointsRepository::GetWhere(database,
		fmt::format(
			"zone = '{}' {} ORDER BY number",
			zonename,
			ContentFilterCriteria::apply()
		)
	);

	for (auto& zone_point : zone_points) {
		auto zp = new ZonePoint;

		zp->x = zone_point.x;
		zp->y = zone_point.y;
		zp->z = zone_point.z;
		zp->target_x = zone_point.target_x;
		zp->target_y = zone_point.target_y;
		zp->target_z = zone_point.target_z;
		zp->target_zone_id = zone_point.target_zone_id;
		zp->heading = zone_point.heading;
		zp->target_heading = zone_point.target_heading;
		zp->number = zone_point.number;
		zp->client_version_mask = zone_point.client_version_mask;
		zp->is_virtual = zone_point.is_virtual > 0;
		zp->height = zone_point.height;
		zp->width = zone_point.width;

		LogZonePoints(
			"Loading ZP x [{}] y [{}] z [{}] heading [{}] target x y z zone_id instance_id [{}] [{}] [{}] [{}] number [{}] is_virtual [{}] height [{}] width [{}]",
			zp->x,
			zp->y,
			zp->z,
			zp->heading,
			zp->target_x,
			zp->target_y,
			zp->target_z,
			zp->target_zone_id,
			zp->number,
			zp->is_virtual ? "true" : "false",
			zp->height,
			zp->width
		);

		if (zone_point.is_virtual) {
			zone->virtual_zone_point_list.emplace_back(zone_point);
			continue;
		}

		zone_point_list->Insert(zp);
		zone->numzonepoints++;
	}

	return true;
}

void Zone::SpawnStatus(Mob* client, char filter, uint32 spawnid)
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	NPC* npc;
	uint32 remaining;
	int sec;

	uint32 x = 0;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		if (spawnid > 0 && iterator.GetData()->GetID() != spawnid)
		{
			iterator.Advance();
			continue;
		}

		if ((filter == 'e' || filter == 'E') && !iterator.GetData()->Enabled())
		{
			iterator.Advance();
			continue;
		}
		if ((filter == 'd' || filter == 'D') && iterator.GetData()->Enabled())
		{
			iterator.Advance();
			continue;
		}

		npc = iterator.GetData()->GetNPCPointer();

		if ((filter == 's' || filter == 'S') && !npc)
		{
			iterator.Advance();
			continue;
		}
		if ((filter == 'u' || filter == 'U') && npc)
		{
			iterator.Advance();
			continue;
		}

		remaining = iterator.GetData()->timer.GetRemainingTime();
		if (remaining == 0xFFFFFFFF)
		{
			remaining = 0;
			sec = -1;
		}
		else
			sec = (remaining / 1000) % 60;

		remaining /= 1000;

		client->Message(Chat::White, "  %d: %s%s - NPC Type ID: %u - X:%1.1f, Y:%1.1f, Z:%1.1f - Spawn Timer: %u hrs %u mins %i sec",
			iterator.GetData()->GetID(),
			!iterator.GetData()->Enabled() ? "(disabled) " : "", npc ? npc->GetCleanName() : "(unspawned)",
			iterator.GetData()->CurrentNPCID(),
			iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ(), 
			remaining / (60 * 60), (remaining / 60) % 60, sec);

		x++;
		iterator.Advance();
	}
	client->Message(Chat::White, "%i spawns listed.", x);
}

void Zone::weatherSend(uint32 timer)
{
	if (timer > 0)
	{
		Weather_Timer->Disable();
		Weather_Timer->Start(timer);
	}

	if (!HasWeather() && !Weather_Timer->Enabled())
	{
		Weather_Timer->Start(60000);
	}

	auto outapp = new EQApplicationPacket(OP_Weather, sizeof(Weather_Struct));
	Weather_Struct* ws = (Weather_Struct*)outapp->pBuffer;

	if(zone_weather>0)
		ws->type = zone_weather-1;
	if(zone_weather>0)
		ws->intensity = zone->weather_intensity;
	entity_list.QueueClients(0, outapp);
	safe_delete(outapp);
}

bool Zone::HasGraveyard() {
	bool Result = false;

	if(graveyard_zoneid() > 0)
		Result = true;

	return Result;
}

void Zone::SetGraveyard(uint32 zoneid, const glm::vec4& graveyardPosition) {
	m_graveyard_zoneid = zoneid;
	m_graveyard = graveyardPosition;
}

void Zone::LoadZoneBanishPoint(const char* zone) {
	database.GetZoneBanishPoint(zone_banish_point, zone);
}

void Zone::LoadZoneBlockedSpells()
{
	if(!blocked_spells)
	{
		zone_total_blocked_spells = database.GetBlockedSpellsCount(zoneid);
		if(zone_total_blocked_spells > 0){
			blocked_spells = new ZoneSpellsBlocked[zone_total_blocked_spells];
			if(!database.LoadBlockedSpells(zone_total_blocked_spells, blocked_spells, GetZoneID()))
			{
				LogError("... Failed to load blocked spells.");
				ClearBlockedSpells();
			}
		}
	}
}

void Zone::ClearBlockedSpells()
{
	safe_delete_array(blocked_spells);

	zone_total_blocked_spells = 0;
}

bool Zone::IsSpellBlocked(uint32 spell_id, const glm::vec3& location)
{
	if (blocked_spells)
	{
		bool exception = false;
		bool block_all = false;
		for (int x = 0; x < zone_total_blocked_spells; x++)
		{
			if (blocked_spells[x].spellid == spell_id)
			{
				exception = true;
			}

			if (blocked_spells[x].spellid == 0)
			{
				block_all = true;
			}
		}

		for (int x = 0; x < zone_total_blocked_spells; x++)
		{
			// If spellid is 0, block all spells in the zone
			if (block_all)
			{
				// If the same zone has entries other than spellid 0, they act as exceptions and are allowed
				if (exception)
				{
					return false;
				}
				else
				{
					return true;
				}
			}
			else
			{
				if (spell_id != blocked_spells[x].spellid)
				{
					continue;
				}

				switch (blocked_spells[x].type)
				{
					case 1:
					{
						return true;
						break;
					}
					case 2:
					{
						if (IsWithinAxisAlignedBox(location, blocked_spells[x].m_Location - blocked_spells[x].m_Difference, blocked_spells[x].m_Location + blocked_spells[x].m_Difference))
							return true;
						break;
					}
					default:
					{
						continue;
						break;
					}
				}
			}
		}
	}
	return false;
}

const char* Zone::GetSpellBlockedMessage(uint32 spell_id, const glm::vec3& location)
{
	if(blocked_spells)
	{
		for(int x = 0; x < zone_total_blocked_spells; x++)
		{
			if(spell_id != blocked_spells[x].spellid && blocked_spells[x].spellid != 0)
				continue;

			switch(blocked_spells[x].type)
			{
				case 1:
				{
					return blocked_spells[x].message;
					break;
				}
				case 2:
				{
					if(IsWithinAxisAlignedBox(location, blocked_spells[x].m_Location - blocked_spells[x].m_Difference, blocked_spells[x].m_Location + blocked_spells[x].m_Difference))
						return blocked_spells[x].message;
					break;
				}
				default:
				{
					continue;
					break;
				}
			}
		}
	}
	return "Error: Message String Not Found\0";
}

void Zone::UpdateQGlobal(uint32 qid, QGlobal newGlobal)
{
	if(newGlobal.npc_id != 0)
		return;

	if(newGlobal.char_id != 0)
		return;

	if(newGlobal.zone_id == GetZoneID() || newGlobal.zone_id == 0)
	{
		if(qGlobals)
		{
			qGlobals->AddGlobal(qid, newGlobal);
		}
		else
		{
			qGlobals = new QGlobalCache();
			qGlobals->AddGlobal(qid, newGlobal);
		}
	}
}

void Zone::DeleteQGlobal(std::string name, uint32 npcID, uint32 charID, uint32 zoneID)
{
	if(qGlobals)
	{
		qGlobals->RemoveGlobal(name, npcID, charID, zoneID);
	}
}

void Zone::ClearNPCEmotes(std::vector<NPC_Emote_Struct*>* NPCEmoteList)
{
	if (NPCEmoteList)
	{
		for (auto& i : *NPCEmoteList)
		{
			safe_delete(i);
		}
		NPCEmoteList->clear();
	}
}

void Zone::LoadNPCEmotes(std::vector<NPC_Emote_Struct*>* NPCEmoteList)
{
	ClearNPCEmotes(NPCEmoteList);
	const std::string query = StringFormat("SELECT ne.emoteid, ne.event_, ne.type, ne.text "
		"FROM npc_emotes ne "
		"JOIN npc_types nt ON nt.emoteid = ne.emoteid "
		"WHERE (nt.id > %d and nt.id < %d) or nt.id < 1000 "
		"GROUP by ne.emoteid, ne.text", (GetZoneID()*1000)-1, (GetZoneID() * 1000)+1000);

    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for (auto row = results.begin(); row != results.end(); ++row)
    {
        NPC_Emote_Struct* nes = new NPC_Emote_Struct;
        nes->emoteid = atoi(row[0]);
        nes->event_ = atoi(row[1]);
        nes->type = atoi(row[2]);
        strn0cpy(nes->text, row[3], sizeof(nes->text));
        NPCEmoteList->push_back(nes);
    }

}

void Zone::LoadKeyRingData(LinkedList<KeyRing_Data_Struct*>* KeyRingDataList)
{

	KeyRingDataList->Clear();
	const std::string query = "SELECT key_item, zoneid, stage, key_name FROM keyring_data";
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		KeyRing_Data_Struct* krd = new KeyRing_Data_Struct;
		krd->keyitem = atoi(row[0]);
		krd->zoneid = atoi(row[1]);
		krd->stage = atoi(row[2]);
		krd->name = row[3];
		KeyRingDataList->Insert(krd);
	}
}

void Zone::LoadSkillDifficulty()
{
    const std::string query = "SELECT skillid, difficulty, name FROM skill_difficulty order by skillid";
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

	int i = 0;
    for (auto row = results.begin(); row != results.end(); ++row)
    {
        uint8 skillid = atoi(row[0]);

		while (i < skillid && i < EQ::skills::SkillCount)
		{
			skill_difficulty[i].difficulty = 7.5;
			strncpy(skill_difficulty[i].name, "SkillUnknown", 32);
			LogError("Skill {} is not in the database!", i);
			++i;
		}

        skill_difficulty[skillid].difficulty = atof(row[1]);
		strncpy(skill_difficulty[skillid].name, row[2], 32);
		++i;
    }

}

void Zone::ReloadWorld(uint8 global_repop){
	entity_list.ClearAreas();
	parse->ReloadQuests();

	if (global_repop) {
		if (global_repop == ReloadWorld::ForceRepop) {
			zone->ClearSpawnTimers();
		}
		zone->Repop();
	}

	worldserver.SendEmoteMessage(
		0,
		0,
		AccountStatus::GMAdmin,
		Chat::Yellow,
		fmt::format(
			"Quests reloaded {} for {}.",
			(
				global_repop ?
				(
					global_repop == ReloadWorld::Repop ?
					"and repopped NPCs " :
					"and forcefully repopped NPCs "
					) :
				""
				),
			fmt::format(
				"{} ({})",
				GetLongName(),
				GetZoneID()
			)
		).c_str()
	);
}

void Zone::ClearSpawnTimers()
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	iterator.Reset();
	while (iterator.MoreElements()) {
		auto query = fmt::format(
			"DELETE FROM respawn_times WHERE id = {} and guild_id = {}",
			iterator.GetData()->GetID(), zone->GetGuildID()
		);
		auto results = database.QueryDatabase(query);

		iterator.Advance();
	}
}

void Zone::LoadTickItems()
{
	tick_items.clear();

    const std::string query = "SELECT it_itemid, it_chance, it_level, it_qglobal, it_bagslot FROM item_tick";
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }


    for (auto row = results.begin(); row != results.end(); ++row) {
        if(atoi(row[0]) == 0)
            continue;

        item_tick_struct ti_tmp;
		ti_tmp.itemid = atoi(row[0]);
		ti_tmp.chance = atoi(row[1]);
		ti_tmp.level = atoi(row[2]);
		ti_tmp.bagslot = (int16)atoi(row[4]);
		ti_tmp.qglobal = std::string(row[3]);
		tick_items[atoi(row[0])] = ti_tmp;

    }

}

uint32 Zone::GetSpawnKillCount(uint32 in_spawnid) {
	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while(iterator.MoreElements())
	{
		if(iterator.GetData()->GetID() == in_spawnid)
		{
			return(iterator.GetData()->killcount);
		}
		iterator.Advance();
	}
	return 0;
}

bool Zone::IsBoatZone()
{
	// This only returns true for zones that contain actual boats or rafts. It should not be used for zones that only have 
	// controllable boats, or intrazone rafts (Halas).

	static const int16 boatzones[] = { 
		Zones::QEYNOS, 
		Zones::FREPORTE, 
		Zones::ERUDNEXT, 
		Zones::BUTCHER, 
		Zones::OOT, 
		Zones::ERUDSXING, 
		Zones::TIMOROUS, 
		Zones::FIRIONA, 
		Zones::OASIS, 
		Zones::OVERTHERE, 
		Zones::NRO, 
		Zones::ICECLAD
	};

	int8 boatzonessize = sizeof(boatzones) / sizeof(boatzones[0]);
	for (int i = 0; i < boatzonessize; i++) {
		if (GetZoneID() == boatzones[i]) {
			return true;
		}
	}

	return false;
}

bool Zone::IsBindArea(float x_coord, float y_coord, float z_coord)
{
	// Coords pulled from a client decompile.

	if(CanBindOthers()) {
		// NK gypsies
		if(GetZoneID() == Zones::NORTHKARANA) {
			if (x_coord >= -260 && x_coord <= -73 && y_coord >= -720 && y_coord <= -550) {
				return true;
			}
			else {
				return false;
			}
		}
		// Ruins of Kaesora
		else if(GetZoneID() == Zones::FIELDOFBONE) {
			if(x_coord >= -550 && x_coord <= 275 && y_coord >= -2250 && y_coord <= -1200) {
				return true;
			}
			else {
				return false;
			}
		}
		// Docks
		else if(GetZoneID() == Zones::FIRIONA) {
			if(x_coord >= 1194 && x_coord <= 3492 && y_coord >= -4410 && y_coord <= -2397) {
				return true;
			}
			else {
				return false;
			}
		}
		// Empty Ruins
		else if(GetZoneID() == Zones::FRONTIERMTNS) {
			if(x_coord >= 1180 && x_coord <= 1560 && y_coord >= -2250 && y_coord <= -2100) {
				return true;
			}
			else {
				return false;
			}
		}
		// Docks
		else if(GetZoneID() == Zones::OVERTHERE) {
			if(x_coord >= 2011 && x_coord <= 3852 && y_coord >= 2344 && y_coord <= 3538) {
				return true;
			}
			else {
				return false;
			}
		}
		// Dock
		else if(GetZoneID() == Zones::ICECLAD) {
			if(x_coord >= 325 && x_coord <= 510 && y_coord >= 5270 && y_coord <= 5365) {
				return true;
			}
			else {
				return false;
			}
		}
		// Entrances
		else if(GetZoneID() == Zones::KAEL) {
			if((x_coord >= -657 && x_coord <= -603 && y_coord <= 17 && y_coord >= -176) ||
				(x_coord >= 3139 && y_coord <= -102 && y_coord >= -287)) {
				return true;
			}
			else {
				return false;
			}
		}
		else if(GetZoneID() == Zones::SKYSHRINE) {
			if((x_coord >= -930 && x_coord <= -200 && y_coord >= -320 && y_coord <= 535 && z_coord >= -10 && z_coord <= 60) ||
				(x_coord >= -800 && x_coord <= -200 && y_coord >= 535 && y_coord <= 1000 && z_coord >= -10 && z_coord <= 120)) {
				return true;
			}
			else {
				return false;
			}
		}
		else {
			return true;
		}
	}

	return false;
}

bool Zone::IsWaterZone(float z)
{
	if (GetZoneID() == Zones::KEDGE) {
		return true;
	}

	if (GetZoneID() == Zones::POWATER && z < 0.0f) {
		return true;
	}

	return false;
}

void Zone::NexusProcess()
{
	if(zoneid == Zones::NEXUS) {
		NPC* mystic = entity_list.GetNPCByNPCTypeID(152019);

		uint32 time_remaining = 0;
		if(Nexus_Portal_Timer->Enabled()) {
			time_remaining = Nexus_Portal_Timer->GetRemainingTime();
		}

		uint32 velious_time_remaining = Nexus_Scion_Timer->GetRemainingTime();

		if(velious_time_remaining >= 600000 && velious_time_remaining <= 600050 && !Nexus_Portal_Timer->Enabled()) {
			Nexus_Portal_Timer = new Timer(RuleI(Zone, NexusTimer));
			Nexus_Portal_Timer->Start();
			LogNexus("Starting delayed portal timer. Time remaining until Velious ports: [{}]", velious_time_remaining);
			return;
		}
		else if (velious_time_remaining >= 540000 && velious_time_remaining <= 540050 && velious_timer_step != 1) {
			if (mystic) {
				mystic->SignalNPC(20);
			}
			velious_timer_step = 1;
			LogNexusDetail("Velious time remaining: [{}] on step [{}]", velious_time_remaining, velious_timer_step);
		}
		else if(velious_time_remaining >= 180000 && velious_time_remaining <= 180050 && velious_timer_step != 2) {
			if (mystic) {
				mystic->SignalNPC(21);
			}
			velious_timer_step = 2;
			LogNexusDetail("Velious time remaining: [{}] on step [{}]", velious_time_remaining, velious_timer_step);
		}
		else if(velious_time_remaining >= 120000 && velious_time_remaining <= 120050 && velious_timer_step != 3) {
			if (mystic) {
				mystic->SignalNPC(22);
			}
			velious_timer_step = 3;
			LogNexusDetail("Velious time remaining: [{}] on step [{}]", velious_time_remaining, velious_timer_step);
		}
		else if(velious_time_remaining >= 60000 && velious_time_remaining <= 60050 && velious_timer_step != 4) {
			if (mystic) {
				mystic->SignalNPC(23);
			}
			velious_timer_step = 4;
			LogNexusDetail("Velious time remaining: [{}] on step [{}]", velious_time_remaining, velious_timer_step);
		}
		else if(Nexus_Scion_Timer->Check() && velious_active) {
			Mob* velious_spire = entity_list.GetMobByNpcTypeID(152027);
			Mob* velious_target = entity_list.GetMobByNpcTypeID(152035);

			if(velious_spire && velious_target) {
				velious_spire->CastSpell(2062,velious_target->GetID());
			}
			else {
				LogNexus("velious_spire is null. Players will not be ported.");
				velious_active = false;
				return;
			}

			LogNexus("Velious spires in Nexus are beginning to cast the port spell. velious_active is [{}]", velious_active);
			velious_active = false;
		}

		if(time_remaining >= 300000 && time_remaining <= 300050 && nexus_timer_step != 1) {
			if (mystic) {
				mystic->SignalNPC(27);
			}
			nexus_timer_step = 1;
			LogNexusDetail("Time remaining [{}] on step [{}]", time_remaining, nexus_timer_step);
		}
		else if(time_remaining >= 180000 && time_remaining <= 180050 && nexus_timer_step != 2) {
			if (mystic) {
				mystic->SignalNPC(24);
			}
			nexus_timer_step = 2;
			LogNexusDetail("Time remaining [{}] on step [{}]", time_remaining, nexus_timer_step);
		}
		else if(time_remaining >= 120000 && time_remaining <= 120050 && nexus_timer_step != 3) {
			if (mystic) {
				mystic->SignalNPC(25);
			}
			nexus_timer_step = 3;
			LogNexusDetail("Time remaining [{}] on step [{}]", time_remaining, nexus_timer_step);
		}
		else if(time_remaining >= 60000 && time_remaining <= 60050 && nexus_timer_step != 4) {
			if (mystic) {
				mystic->SignalNPC(26);
			}
			nexus_timer_step = 4;
			LogNexusDetail("Time remaining [{}] on step [{}]", time_remaining, nexus_timer_step);
		}
		else if(Nexus_Portal_Timer->Check() && !velious_active) {

			bool failed = false;
			Mob* antonica_spire = entity_list.GetMobByNpcTypeID(152024);
			Mob* faydwer_spire = entity_list.GetMobByNpcTypeID(152023);
			Mob* kunark_spire = entity_list.GetMobByNpcTypeID(152022);
			Mob* odus_spire = entity_list.GetMobByNpcTypeID(152033);

			Mob* antonica_target = entity_list.GetMobByNpcTypeID(152034);
			Mob* faydwer_target = entity_list.GetMobByNpcTypeID(152001);
			Mob* kunark_target = entity_list.GetMobByNpcTypeID(152036);
			Mob* odus_target = entity_list.GetMobByNpcTypeID(152037);

			if(antonica_spire && antonica_target) {
				antonica_spire->CastSpell(2708,antonica_target->GetID());
			}
			else {
				failed = true;
			}

			if(faydwer_spire && faydwer_target) {
				faydwer_spire->CastSpell(2706,faydwer_target->GetID());
			}
			else {
				failed = true;
			}

			if(odus_spire && odus_target) {
				odus_spire->CastSpell(2707,odus_target->GetID());
			}
			else {
				failed = true;
			}

			if(kunark_spire && kunark_target) {
				kunark_spire->CastSpell(2709,kunark_target->GetID());
			}
			else {
				failed = true;
			}

			if(failed) {
				LogNexus("One of the spires is null. Some or all players will not be ported.");
			}

			LogNexus("General spires in Nexus are beginning to cast the port spell. velious_active is [{}]", velious_active);
			velious_active = true;
		}
	}
	else if(IsNexusScionZone())	{
		NPC* mystic = nullptr;
		Mob* spires = nullptr;

		if(GetZoneID() == Zones::NORTHKARANA) {
			mystic = entity_list.GetNPCByNPCTypeID(13114);
			spires = entity_list.GetMobByNpcTypeID(13106);	
		}
		else if(GetZoneID() == Zones::GFAYDARK)
		{
			mystic = entity_list.GetNPCByNPCTypeID(54301);
			spires = entity_list.GetMobByNpcTypeID(54344);
		}
		else if(GetZoneID() == Zones::TOX) {
			mystic = entity_list.GetNPCByNPCTypeID(38163);
			spires = entity_list.GetMobByNpcTypeID(38149);
		}
		else if(GetZoneID() == Zones::DREADLANDS) {
			mystic = entity_list.GetNPCByNPCTypeID(86153);
			spires = entity_list.GetMobByNpcTypeID(86151);
		}
		else if(GetZoneID() == Zones::GREATDIVIDE) {
			mystic = entity_list.GetNPCByNPCTypeID(118165);
			spires = entity_list.GetMobByNpcTypeID(118314);
		}
	
		uint32 time_remaining = Nexus_Scion_Timer->GetRemainingTime();
		if(time_remaining >= 300000 && time_remaining <= 300050 && nexus_timer_step != 1) {
			if (mystic) {
				mystic->SignalNPC(1);
			}
			nexus_timer_step = 1;
			LogNexusDetail("Time remaining: [{}] on step: [{}] in zone: [{}]", time_remaining, nexus_timer_step, GetShortName());
		}
		else if(time_remaining >= 240000 && time_remaining <= 240050 && nexus_timer_step != 2)
		{
			if (mystic) {
				mystic->SignalNPC(2);
			}
			nexus_timer_step = 2;
			LogNexusDetail("Time remaining: [{}] on step: [{}] in zone: [{}]", time_remaining, nexus_timer_step, GetShortName());
		}
		else if(time_remaining >= 180000 && time_remaining <= 180050 && nexus_timer_step != 3)
		{
			if (mystic) {
				mystic->SignalNPC(3);
			}
			nexus_timer_step = 3;
			LogNexusDetail("Time remaining: [{}] on step: [{}] in zone: [{}]", time_remaining, nexus_timer_step, GetShortName());
		}
		else if(time_remaining >= 120000 && time_remaining <= 120050 && nexus_timer_step != 4)
		{
			if (mystic) {
				mystic->SignalNPC(4);
			}
			nexus_timer_step = 4;
			LogNexusDetail("Time remaining: [{}] on step: [{}] in zone: [{}]", time_remaining, nexus_timer_step, GetShortName());
		}
		else if(time_remaining >= 60000 && time_remaining <= 60050 && nexus_timer_step != 5)
		{
			if (mystic) {
				mystic->SignalNPC(5);
			}
			nexus_timer_step = 5;
			LogNexusDetail("Time remaining: [{}] on step: [{}] in zone: [{}]", time_remaining, nexus_timer_step, GetShortName());
		}
		else if(Nexus_Scion_Timer->Check())
		{
			if(spires) {
				//Todo: Figure out animation
				spires->CastSpell(2935,spires->GetID());
				LogNexus("Spires in [{}] are beginning to cast the port spell to nexus.", GetShortName());
			}
			else {
				LogNexus("Spires is null. Players will not be ported in [{}]", GetShortName());
				return;
			}
		}
	}
}

bool Zone::IsNexusScionZone()
{
	if(GetZoneID() == Zones::NORTHKARANA || 
		GetZoneID() == Zones::GFAYDARK || 
		GetZoneID() == Zones::TOX || 
		GetZoneID() == Zones::GREATDIVIDE || 
		GetZoneID() == Zones::DREADLANDS
	) {
		return true;
	}

	return false;
}

void Zone::ApplyRandomLoc(uint32 zoneid, float& x, float& y)
{
	// Easiest to use a database call here, because we're grabbing data for another zone.
	uint8 random_loc = database.GetZoneRandomLoc(zoneid);
	if (random_loc <= 0)
		return;

	float tmp_x, tmp_y;
	tmp_x = zone->random.Int(0, (random_loc * 2)) - random_loc;
	tmp_y = zone->random.Int(0, (random_loc * 2)) - random_loc;

	Log(Logs::General, Logs::EQMac, "random loc: %d Original coords are %0.1f, %0.1f. Updated diffs are %0.1f, %0.1f. FINAL coords are %0.1f, %0.1f", random_loc, x, y, tmp_x, tmp_y, x + tmp_x, y + tmp_y);

	x = x + tmp_x;
	y = y + tmp_y;
	return;
}

bool Zone::CanClientEngage(Client* initiator, Mob* target)
{
	if (!initiator || !target)
	{
		return false;
	}

	if (initiator->Admin() >= RuleI(Quarm, MinStatusToZoneIntoAnyGuildZone))
		return(true);

	if (GetGuildID() == GUILD_NONE)
		return true;

	if (GetGuildID() == 1)
		return true;

	Raid* raid = initiator->GetRaid();
	if (!raid)
		return false;

	return raid->GetEngageCachedResult();
}

bool Zone::CanDoCombat(Mob* current, Mob* other, bool process)
{
	if (current && other && zone->GetGuildID() != GUILD_NONE)
	{
		if (current->IsClient() && current->CastToClient()->InstanceBootGraceTimerExpired())
		{
			bool bCanEngage = CanClientEngage(current->CastToClient(), other);
			if (!bCanEngage)
			{
				current->CastToClient()->BootFromGuildInstance();
				return false;
			}
		}
		if (other->IsClient() && other->CastToClient()->InstanceBootGraceTimerExpired())
		{
			bool bCanEngage = CanClientEngage(other->CastToClient(), current);
			if (!bCanEngage)
			{
				other->CastToClient()->BootFromGuildInstance();
				return false;
			}
		}
	}

	if (CanDoCombat())
	{
		return true;
	}
	else if(GetZoneID() == Zones::BAZAAR) {// bazaar seems to be the only no combat zone with a PVP area.
		// If we're doing a PVP check and one of us is a non-pet NPC disallow combat.
		if ((current && current->IsNPC() && !current->IsPlayerOwned()) ||
			(other && other->IsNPC() && !other->IsPlayerOwned())) {
			return false;
		}

		if (current && other) {
			if (zone->watermap != nullptr) {
				glm::vec3 mypos(current->GetX(), current->GetY(), current->GetZ());
				glm::vec3 opos(other->GetX(), other->GetY(), other->GetZ());
				if (zone->watermap->InPVP(mypos) && zone->watermap->InPVP(opos)) {
					return true;
				}
			}
		}
		else if (current && process) {
			if (zone->watermap != nullptr) {
				glm::vec3 mypos(current->GetX(), current->GetY(), current->GetZ());
				if (zone->watermap->InPVP(mypos)) {
					return true;
				}
			}
		}
	}

	return false;
}

void Zone::LoadGrids()
{
	grids = GridRepository::GetZoneGrids(database, GetZoneID());
	grid_entries = GridEntriesRepository::GetZoneGridEntries(database, GetZoneID());
}

Timer Zone::GetInitgridsTimer()
{
	return initgrids_timer;
}

bool Zone::AllowManastoneClick()
{
	if (GetZoneExpansion() != ClassicEQ) {
		return false;
	}
	if (arrClassicPlanes.find(GetShortName()) != arrClassicPlanes.end()) {
		return false;
	}
	return true;
}

bool Zone::IsQuestHotReloadQueued() const
{
	return quest_hot_reload_queued;
}

void Zone::SetQuestHotReloadQueued(bool in_quest_hot_reload_queued)
{
	quest_hot_reload_queued = in_quest_hot_reload_queued;
}

std::string Zone::GetZoneDescription()
{
	if (!IsLoaded()) {
		return fmt::format(
			"PID ({})",
			EQ::GetPID()
		);
	}

	return fmt::format(
		"PID ({}) {} ({})",
		EQ::GetPID(),
		GetLongName(),
		GetZoneID()
	);
}

void Zone::SendReloadMessage(std::string reload_type)
{
	LogInfo("Reloaded [{}]", reload_type);

	worldserver.SendEmoteMessage(
		0,
		0,
		AccountStatus::GMAdmin,
		Chat::Yellow,
		fmt::format(
			"{} reloaded for {}.",
			reload_type,
			GetZoneDescription()
		).c_str()
	);
}

void Zone::RequestUCSServerStatus() {
	auto outapp = new ServerPacket(ServerOP_UCSServerStatusRequest, sizeof(UCSServerStatus_Struct));
	auto ucsss = (UCSServerStatus_Struct*)outapp->pBuffer;
	ucsss->available = 0;
	ucsss->port = Config->ZonePort;
	ucsss->unused = 0;
	worldserver.SendPacket(outapp);
	safe_delete(outapp);
}

void Zone::SetUCSServerAvailable(bool ucss_available, uint32 update_timestamp) 
{
	if (m_last_ucss_update == update_timestamp && m_ucss_available != ucss_available) {
		m_ucss_available = false;
		RequestUCSServerStatus();
		return;
	}

	if (m_last_ucss_update < update_timestamp)
		m_ucss_available = ucss_available;
}

void Zone::SendDiscordMessage(int webhook_id, const std::string& message)
{
	if (worldserver.Connected()) {
		auto pack = new ServerPacket(ServerOP_DiscordWebhookMessage, sizeof(DiscordWebhookMessage_Struct) + 1);
		auto* q = (DiscordWebhookMessage_Struct*)pack->pBuffer;

		strn0cpy(q->message, message.c_str(), 2000);
		q->webhook_id = webhook_id;

		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}
void Zone::SendDiscordMessage(const std::string& webhook_name, const std::string& message)
{
	bool not_found = true;

	for (int i = 0; i < MAX_DISCORD_WEBHOOK_ID; i++) {
		auto& w = LogSys.GetDiscordWebhooks()[i];
		if (w.webhook_name == webhook_name) {
			SendDiscordMessage(w.id, message + "\n");
			not_found = false;
		}
	}

	if (not_found) {
		LogDiscord("[SendDiscordMessage] Did not find valid webhook by webhook name [{}]", webhook_name);
	}
}

#include "zone_loot.cpp"