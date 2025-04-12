
#include "../common/eqemu_logsys.h"
#include "../common/extprofile.h"
#include "../common/item_instance.h"
#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/zone_store.h"

#include "client.h"
#include "corpse.h"
#include "groups.h"
#include "zone.h"
#include "zonedb.h"
#include "../common/repositories/criteria/content_filter_criteria.h"
#include "../common/repositories/npc_types_repository.h"
#include "../common/repositories/character_bind_repository.h"
#include "../common/repositories/character_buffs_repository.h"
#include "../common/repositories/character_pet_buffs_repository.h"
#include "../common/repositories/character_pet_inventory_repository.h"
#include "../common/repositories/character_pet_info_repository.h"

#include <ctime>
#include <iostream>
#include <fmt/format.h>

extern Zone* zone;

ZoneDatabase database;

ZoneDatabase::ZoneDatabase()
: SharedDatabase()
{
	ZDBInitVars();
}

ZoneDatabase::ZoneDatabase(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
: SharedDatabase(host, user, passwd, database, port)
{
	ZDBInitVars();
}

void ZoneDatabase::ZDBInitVars() {
	memset(door_isopen_array, 0, sizeof(door_isopen_array));
	npc_spellseffects_cache = 0;
	npc_spellseffects_loadtried = 0;
	max_faction = 0;
	faction_array = nullptr;
}

ZoneDatabase::~ZoneDatabase() {
	if (npc_spellseffects_cache) {
		for (int x=0; x<=npc_spellseffects_maxid; x++) {
			safe_delete_array(npc_spellseffects_cache[x]);
		}
		safe_delete_array(npc_spellseffects_cache);
	}
	safe_delete_array(npc_spellseffects_loadtried);

	if (faction_array != nullptr) {
		for (int x=0; x <= max_faction; x++) {
			if (faction_array[x] != 0)
				safe_delete(faction_array[x]);
		}
		safe_delete_array(faction_array);
	}
}

bool ZoneDatabase::SaveZoneCFG(uint32 zoneid, NewZone_Struct* zd) {
	auto query = fmt::format(
			"UPDATE zone SET underworld = {:.2f}, minclip = {:.2f}, "
			"maxclip = {:.2f}, fog_minclip = {:.2f}, fog_maxclip = {:.2f}, "
			"fog_blue = {}, fog_red = {}, fog_green = {}, "
			"sky = {}, ztype = {}, zone_exp_multiplier = {:.2f}, "
			"safe_x = {:.2f}, safe_y = {:.2f}, safe_z = {:.2f} "
			"WHERE zoneidnumber = {}",
			zd->underworld, 
			zd->minclip,
			zd->maxclip,
			zd->fog_minclip[0], 
			zd->fog_maxclip[0],
			zd->fog_blue[0], 
			zd->fog_red[0], 
			zd->fog_green[0],
			zd->sky, 
			zd->ztype, 
			zd->zone_exp_multiplier,
			zd->safe_x, 
			zd->safe_y, 
			zd->safe_z,
			zoneid
	);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return false;
	}

	return true;
}

bool ZoneDatabase::GetZoneCFG(uint32 zoneid, NewZone_Struct *zone_data, bool &can_bind, bool &can_combat, bool &can_levitate, bool &can_castoutdoor, bool &is_city, uint8 &zone_type, int &ruleset, char **map_filename, bool &can_bind_others, bool &skip_los, bool &drag_aggro, bool &can_castdungeon, uint16 &pull_limit, bool &reducedspawntimers, bool& trivial_loot_code, bool &is_hotzone) {


	*map_filename = new char[100];
	zone_data->zone_id = zoneid;

	auto query = fmt::format(
		"SELECT ztype, fog_red, fog_green, fog_blue, fog_minclip, fog_maxclip, " // 5
        "fog_red2, fog_green2, fog_blue2, fog_minclip2, fog_maxclip2, " // 5
        "fog_red3, fog_green3, fog_blue3, fog_minclip3, fog_maxclip3, " // 5
        "fog_red4, fog_green4, fog_blue4, fog_minclip4, fog_maxclip4, " // 5
        "fog_density, sky, zone_exp_multiplier, safe_x, safe_y, safe_z, underworld, " // 7
        "minclip, maxclip, time_type, canbind, cancombat, canlevitate, " // 6
        "castoutdoor, hotzone, ruleset, suspendbuffs, map_file_name, short_name, " // 5
        "rain_chance1, rain_chance2, rain_chance3, rain_chance4, " // 4
        "rain_duration1, rain_duration2, rain_duration3, rain_duration4, " // 4
        "snow_chance1, snow_chance2, snow_chance3, snow_chance4, " // 4
        "snow_duration1, snow_duration2, snow_duration3, snow_duration4, " // 4
        "skylock, skip_los, music, expansion, dragaggro, never_idle, castdungeon, " 
        "pull_limit, graveyard_time, max_z, reducedspawntimers, trivial_loot_code " // 8
        "FROM zone WHERE zoneidnumber = {} {}",
        zoneid,
        ContentFilterCriteria::apply().c_str()
	);
	
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        strcpy(*map_filename, "default");
		return false;
    }

	if (results.RowCount() == 0) {
        strcpy(*map_filename, "default");
        return false;
    }

    auto& row = results.begin();

    memset(zone_data, 0, sizeof(NewZone_Struct));
    zone_data->ztype = atoi(row[0]);
    zone_type = zone_data->ztype;

    int index;
    for(index = 0; index < 4; index++) {
        zone_data->fog_red[index]=atoi(row[1 + index * 5]);
        zone_data->fog_green[index]=atoi(row[2 + index * 5]);
        zone_data->fog_blue[index]=atoi(row[3 + index * 5]);
        zone_data->fog_minclip[index]=atof(row[4 + index * 5]);
        zone_data->fog_maxclip[index]=atof(row[5 + index * 5]);
    }

    zone_data->fog_density = atof(row[21]);
    zone_data->sky=atoi(row[22]);
    zone_data->zone_exp_multiplier=atof(row[23]);
    zone_data->safe_x=atof(row[24]);
    zone_data->safe_y=atof(row[25]);
    zone_data->safe_z=atof(row[26]);
    zone_data->underworld=atof(row[27]);
    zone_data->minclip=atof(row[28]);
    zone_data->maxclip=atof(row[29]);
    zone_data->time_type=atoi(row[30]);

    //not in the DB yet:
    zone_data->gravity = 0.4;

    int bindable = 0;
    bindable = atoi(row[31]);

    can_bind = bindable == 0? false: true;
    is_city = bindable == 2? true: false;
	can_bind_others = bindable == 3? true: false;
    can_combat = atoi(row[32]) == 0? false: true;
    can_levitate = atoi(row[33]) == 0? false: true;
    can_castoutdoor = atoi(row[34]) == 0? false: true;
	is_hotzone = atoi(row[35]) == 0 ? false : true;

    ruleset = atoi(row[36]);
    zone_data->SuspendBuffs = atoi(row[37]);

    char *file = row[38];
    if(file)
        strcpy(*map_filename, file);
    else
        strcpy(*map_filename, row[39]);

    for(index = 0; index < 4; index++)
        zone_data->rain_chance[index]=atoi(row[40 + index]);

	for(index = 0; index < 4; index++)
        zone_data->rain_duration[index]=atoi(row[44 + index]);

	for(index = 0; index < 4; index++)
        zone_data->snow_chance[index]=atoi(row[48 + index]);

	for(index = 0; index < 4; index++)
        zone_data->snow_duration[index]=atoi(row[52 + index]);

	zone_data->skylock = atoi(row[56]);
	skip_los = atoi(row[57]) == 0? false: true;
	zone_data->normal_music_day = atoi(row[58]);
	
	// These never change in packet
	zone_data->water_music = 4;
	zone_data->normal_music_night = 0;

	uint8 zone_expansion = atoi(row[59]);
	if(zone_expansion == 1)
		zone_data->expansion = KunarkEQ;

	else if(zone_expansion == 2)
		zone_data->expansion = VeliousEQ;

	else if(zone_expansion == 3)
		zone_data->expansion = LuclinEQ;

	else if(zone_expansion == 4)
		zone_data->expansion = PlanesEQ;

	else
		zone_data->expansion = ClassicEQ;

	drag_aggro = atoi(row[60]) == 0 ? false : true;
	zone_data->never_idle = atoi(row[61]) == 0 ? false : true;
	can_castdungeon = atoi(row[62]) == 0 ? false : true;
	pull_limit = atoi(row[63]);
	zone_data->graveyard_time = atoi(row[64]);
	zone_data->max_z = atof(row[65]);
	reducedspawntimers = atoi(row[66]);
	trivial_loot_code = atoi(row[67]);
	return true;
}

bool ZoneDatabase::logevents(const char* accountname,uint32 accountid,uint8 status,const char* charname, const char* target,const char* descriptiontype, const char* description,int event_nid){

	uint32 len = strlen(description);
	uint32 len2 = strlen(target);
	auto descriptiontext = new char[2 * len + 1];
	auto targetarr = new char[2 * len2 + 1];
	memset(descriptiontext, 0, 2*len+1);
	memset(targetarr, 0, 2*len2+1);
	DoEscapeString(descriptiontext, description, len);
	DoEscapeString(targetarr, target, len2);

	std::string query = StringFormat("INSERT INTO eventlog (accountname, accountid, status, "
                                    "charname, target, descriptiontype, description, event_nid) "
                                    "VALUES('%s', %i, %i, '%s', '%s', '%s', '%s', '%i')",
                                    accountname, accountid, status, charname, targetarr,
                                    descriptiontype, descriptiontext, event_nid);
    safe_delete_array(descriptiontext);
	safe_delete_array(targetarr);
	auto results = QueryDatabase(query);
	if (!results.Success())	{
		return false;
	}

	return true;
}

void ZoneDatabase::UpdateBug(BugReport_Struct* bug_report, uint32 clienttype) 
{
	if (!bug_report)
		return;
	size_t len = 0;
	char* name_ = nullptr;
	char* category_name_ = nullptr;
	char* reporter_name_ = nullptr;
	char* target_name_ = nullptr;
	char* bug_report_ = nullptr;

	len = strlen(bug_report->category_name);
	if (len) {
		if (len > 63) // check against db column size
			len = 63;
		category_name_ = new char[(2 * len + 1)];
		memset(category_name_, 0, (2 * len + 1));
		DoEscapeString(category_name_, bug_report->category_name, len);
	}

	len = strlen(bug_report->reporter_name);
	if (len) {
		if (len > 63)
			len = 63;
		reporter_name_ = new char[(2 * len + 1)];
		memset(reporter_name_, 0, (2 * len + 1));
		DoEscapeString(reporter_name_, bug_report->reporter_name, len);
	}

	len = strlen(bug_report->target_name);
	if (len) {
		if (len > 63)
			len = 63;
		target_name_ = new char[(2 * len + 1)];
		memset(target_name_, 0, (2 * len + 1));
		DoEscapeString(target_name_, bug_report->target_name, len);
	}

	len = strlen(bug_report->bug_report);
	if (len) {
		if (len > 1023)
			len = 1023;
		bug_report_ = new char[(2 * len + 1)];
		memset(bug_report_, 0, (2 * len + 1));
		DoEscapeString(bug_report_, bug_report->bug_report, len);
	}

	char uitext[16];
	if (clienttype == EQ::versions::bit_MacPC)
		strcpy(uitext, "PC");
	else if (clienttype == EQ::versions::bit_MacIntel)
		strcpy(uitext, "Intel");
	else if (clienttype == EQ::versions::bit_MacPPC)
		strcpy(uitext, "PPC");

	std::string query = StringFormat("INSERT INTO bugs (zone, name, ui, x, y, z, type, flag, target, bug, _can_duplicate, _crash_bug, _target_info, _character_flags, date) "
		"VALUES('%s', '%s', '%s', '%.2f', '%.2f', '%.2f', '%s', %d, '%s', '%s', '%u', '%u', '%u', '%u', CURDATE())",
		zone->GetShortName(),
		(reporter_name_ ? reporter_name_ : ""),
		uitext,
		bug_report->pos_x,
		bug_report->pos_y,
		bug_report->pos_z,
		(category_name_ ? category_name_ : "Other"),
		bug_report->optional_info_mask,
		(target_name_ ? target_name_ : ""),
		(bug_report_ ? bug_report_ : ""),
		((bug_report->optional_info_mask & EQ::bug::infoCanDuplicate) != 0 ? 1 : 0),
		((bug_report->optional_info_mask & EQ::bug::infoCrashBug) != 0 ? 1 : 0),
		((bug_report->optional_info_mask & EQ::bug::infoTargetInfo) != 0 ? 1 : 0),
		((bug_report->optional_info_mask & EQ::bug::infoCharacterFlags) != 0 ? 1 : 0)
	);
	safe_delete_array(category_name_);
	safe_delete_array(reporter_name_);
	safe_delete_array(target_name_);
	safe_delete_array(bug_report_);
	QueryDatabase(query);
}

void ZoneDatabase::UpdateFeedback(Feedback_Struct* feedback) {

	uint32 len = strlen(feedback->name);
	char* name = nullptr;
	if (len > 0)
	{
		name = new char[2 * len + 1];
		memset(name, 0, 2 * len + 1);
		DoEscapeString(name, feedback->name, len);
	}

	len = strlen(feedback->message);
	char* message = nullptr;
	if (len > 0)
	{
		message = new char[2 * len + 1];
		memset(message, 0, 2 * len + 1);
		DoEscapeString(message, feedback->message, len);
	}

	std::string query = StringFormat("INSERT INTO feedback (name, message, zone, date) "
		"VALUES('%s', '%s', '%s', CURDATE())",
		name, message, zone->GetShortName());

	QueryDatabase(query);
	safe_delete_array(name);
	safe_delete_array(message);

}

void ZoneDatabase::AddSoulMark(uint32 charid, const char* charname, const char* accname, const char* gmname, const char* gmacctname, uint32 utime, uint32 type, const char* desc) {

	std::string query = StringFormat("INSERT INTO character_soulmarks (charid, charname, acctname, gmname, gmacctname, utime, type, `desc`) "
		"VALUES(%i, '%s', '%s','%s','%s', %i, %i, '%s')", charid, charname, accname, gmname, gmacctname, utime, type, desc);
	auto results = QueryDatabase(query);
	if (!results.Success())
		std::cerr << "Error in AddSoulMark '" << query << "' " << results.ErrorMessage() << std::endl;

}

int ZoneDatabase::RemoveSoulMark(uint32 charid) {

	std::string query = StringFormat("DELETE FROM character_soulmarks where charid=%i", charid);
	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		std::cerr << "Error in DeleteSoulMark '" << query << "' " << results.ErrorMessage() << std::endl;
		return 0;
	}
	int res = 0;

	if(results.RowsAffected() >= 0 && results.RowsAffected() <= 11)
	{
		res = results.RowsAffected();
	}

	return res;

}

bool ZoneDatabase::DoorIsOpen(uint8 door_id,const char* zone_name)
{
	if(door_isopen_array[door_id] == 0) {
		SetDoorPlace(1,door_id,zone_name);
		return false;
	}
	else {
		SetDoorPlace(0,door_id,zone_name);
		return true;
	}
}

void ZoneDatabase::SetDoorPlace(uint8 value,uint8 door_id,const char* zone_name)
{
	door_isopen_array[door_id] = value;
}

// Load child objects for a world container (i.e., forge, bag dropped to ground, etc)
void ZoneDatabase::LoadWorldContainer(uint32 parentid, EQ::ItemInstance* container)
{
	if (!container) {
		Log(Logs::General, Logs::Error, "Programming error: LoadWorldContainer passed nullptr pointer");
		return;
	}

	std::string query = StringFormat("SELECT bagidx, itemid, charges "
                                    "FROM object_contents WHERE parentid = %i", parentid);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        Log(Logs::General, Logs::Error, "Error in DB::LoadWorldContainer: %s", results.ErrorMessage().c_str());
        return;
    }

    for (auto& row = results.begin(); row != results.end(); ++row) {
        uint8 index = (uint8)atoi(row[0]);
        uint32 item_id = (uint32)atoi(row[1]);
        int8 charges = (int8)atoi(row[2]);
        QuarmItemData quarm_item_data = EmptyQuarmItemData; // NYI: object_contents persistence for QuarmItemData

        EQ::ItemInstance* inst = database.CreateItem(item_id, charges, quarm_item_data);
        if (inst) {
            // Put item inside world container
            container->PutItem(index, *inst);
        }
		safe_delete(inst);
	}
}

// Save child objects for a world container (i.e., forge, bag dropped to ground, etc)
void ZoneDatabase::SaveWorldContainer(uint32 zone_id, uint32 parent_id, const EQ::ItemInstance* container)
{
	// Since state is not saved for each world container action, we'll just delete
	// all and save from scratch .. we may come back later to optimize
	if (!container)
		return;

	//Delete all items from container
	DeleteWorldContainer(parent_id,zone_id);

	// Save all 10 items, if they exist
	for (uint8 index = EQ::invbag::SLOT_BEGIN; index <= EQ::invbag::SLOT_END; index++) {

		EQ::ItemInstance* inst = container->GetItem(index);
		if (!inst)
            continue;
		else
		{
			if(inst->GetItem()->NoDrop == 0)
				continue;
		}

        uint32 item_id = inst->GetItem()->ID;
        // NYI: object_contents persistence for QuarmItemData

        std::string query = StringFormat("REPLACE INTO object_contents "
                                        "(zoneid, parentid, bagidx, itemid, charges, droptime) "
                                        "VALUES (%i, %i, %i, %i, %i, now())",
                                        zone_id, parent_id, index, item_id, inst->GetCharges());
        auto results = QueryDatabase(query);
        if (!results.Success())
            Log(Logs::General, Logs::Error, "Error in ZoneDatabase::SaveWorldContainer: %s", results.ErrorMessage().c_str());

    }

}

// Remove all child objects inside a world container (i.e., forge, bag dropped to ground, etc)
void ZoneDatabase::DeleteWorldContainer(uint32 parent_id, uint32 zone_id)
{
	std::string query = StringFormat("DELETE FROM object_contents WHERE parentid = %i AND zoneid = %i", parent_id, zone_id);
    auto results = QueryDatabase(query);
	if (!results.Success())
		Log(Logs::General, Logs::Error, "Error in ZoneDatabase::DeleteWorldContainer: %s", results.ErrorMessage().c_str());

}

Trader_Struct* ZoneDatabase::LoadTraderItem(uint32 char_id)
{
	auto loadti = new Trader_Struct;
	memset(loadti,0,sizeof(Trader_Struct));

	std::string query = StringFormat("SELECT * FROM trader WHERE char_id = %i ORDER BY slot_id LIMIT 80", char_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::Detail, Logs::Bazaar, "Failed to load trader information!\n");
		return loadti;
	}

	loadti->Code = BazaarTrader_ShowItems;
	for (auto& row = results.begin(); row != results.end(); ++row) {
		if (atoi(row[5]) >= 80 || atoi(row[4]) < 0) {
			Log(Logs::Detail, Logs::Bazaar, "Bad Slot number when trying to load trader information!\n");
			continue;
		}

		loadti->Items[atoi(row[5])] = atoi(row[1]);
		loadti->ItemCost[atoi(row[5])] = atoi(row[4]);
	}
	return loadti;
}

TraderCharges_Struct* ZoneDatabase::LoadTraderItemWithCharges(uint32 char_id)
{
	auto loadti = new TraderCharges_Struct;
	memset(loadti,0,sizeof(TraderCharges_Struct));

	std::string query = StringFormat("SELECT * FROM trader WHERE char_id=%i ORDER BY slot_id LIMIT 80", char_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::Detail, Logs::Bazaar, "Failed to load trader information!\n");
		return loadti;
	}

	for (auto& row = results.begin(); row != results.end(); ++row) {
		if (atoi(row[5]) >= 80 || atoi(row[5]) < 0) {
			Log(Logs::Detail, Logs::Bazaar, "Bad Slot number when trying to load trader information!\n");
			continue;
		}

		loadti->ItemID[atoi(row[5])] = atoi(row[1]);
		loadti->SlotID[atoi(row[5])] = atoi(row[2]);
		loadti->Charges[atoi(row[5])] = atoi(row[3]);
		loadti->ItemCost[atoi(row[5])] = atoi(row[4]);
	}
	return loadti;
}

EQ::ItemInstance* ZoneDatabase::LoadSingleTraderItem(uint32 CharID, int16 islotid, uint16 slotid) {
	std::string query = StringFormat("SELECT * FROM trader WHERE char_id = %i AND slot_id = %i AND i_slotid = %i"
                                    " ORDER BY slot_id LIMIT 80", CharID, slotid, islotid);
    auto results = QueryDatabase(query);
    if (!results.Success())
        return nullptr;

	if (results.RowCount() == 0) {
        Log(Logs::Detail, Logs::Bazaar, "LoadSingleTraderItem: No items found."); fflush(stdout);
        return nullptr;
    }

    auto& row = results.begin();

    int ItemID = atoi(row[1]);
	int Charges = atoi(row[3]);
	int Cost = atoi(row[4]);

    const EQ::ItemData *item = database.GetItem(ItemID);

	if(!item) {
		Log(Logs::Detail, Logs::Bazaar, "Unable to create item\n");
		fflush(stdout);
		return nullptr;
	}

    if (item->NoDrop == 0)
        return nullptr;

    EQ::ItemInstance* inst = database.CreateItem(item);
	if(!inst) {
		Log(Logs::Detail, Logs::Bazaar, "Unable to create item instance\n");
		fflush(stdout);
		return nullptr;
	}

    inst->SetCharges(Charges);
	inst->SetMerchantSlot(slotid);
	inst->SetPrice(Cost);

	if(inst->IsStackable())
		inst->SetMerchantCount(Charges);

	return inst;
}

int16 ZoneDatabase::GetTraderItemBySlot(uint32 char_id, int8 slotid)
{
	std::string query = StringFormat("SELECT i_slotid FROM trader WHERE char_id = %i AND slot_id = %i", char_id, slotid);
    auto results = QueryDatabase(query);
    if (!results.Success())
        return -1;

	if (results.RowCount() == 0) 
	{
        return -1;
    }

	auto& row = results.begin();
    return atoi(row[0]);
}

void ZoneDatabase::SaveTraderItem(uint32 CharID, uint32 ItemID, int16 islotid, int32 Charges, uint32 ItemCost, uint8 Slot){

	std::string query = StringFormat("REPLACE INTO trader (char_id, item_id, i_slotid, charges, item_cost, slot_id) VALUES(%i, %i, %i, %i, %i, %i)",
                                    CharID, ItemID, islotid, Charges, ItemCost, Slot);
    auto results = QueryDatabase(query);
    if (!results.Success())
        Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to save trader item: %i for char_id: %i, the error was: %s\n", ItemID, CharID, results.ErrorMessage().c_str());

}

void ZoneDatabase::UpdateTraderItemCharges(int CharID, int16 SlotID, int32 Charges) {
	Log(Logs::Detail, Logs::Bazaar, "ZoneDatabase::UpdateTraderItemCharges(%i, %i, %i)", CharID, SlotID, Charges);

	std::string query = StringFormat("UPDATE trader SET charges = %i WHERE char_id = %i AND slot_id = %i",
                                    Charges, CharID, SlotID);
    auto results = QueryDatabase(query);
    if (!results.Success())
		Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to update charges for trader item in inventory slot: %i for char_id: %i, the error was: %s\n",
                                SlotID, CharID, results.ErrorMessage().c_str());

}

void ZoneDatabase::UpdateTraderItemPrice(int CharID, uint32 ItemID, uint32 Charges, uint32 NewPrice) {

	Log(Logs::Detail, Logs::Bazaar, "ZoneDatabase::UpdateTraderPrice(%i, %i, %i, %i)", CharID, ItemID, Charges, NewPrice);

	const EQ::ItemData *item = database.GetItem(ItemID);

	if(!item)
		return;

	if(NewPrice == 0) {
		Log(Logs::Detail, Logs::Bazaar, "Removing Trader items from the DB for CharID %i, ItemID %i", CharID, ItemID);

        std::string query = StringFormat("DELETE FROM trader WHERE char_id = %i AND item_id = %i",CharID, ItemID);
        auto results = QueryDatabase(query);
        if (!results.Success())
			Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to remove trader item(s): %i for char_id: %i, the error was: %s\n", ItemID, CharID, results.ErrorMessage().c_str());

		return;
	}

    if(!item->Stackable) {
        std::string query = StringFormat("UPDATE trader SET item_cost = %i "
                                        "WHERE char_id = %i AND item_id = %i AND charges=%i",
                                        NewPrice, CharID, ItemID, Charges);
        auto results = QueryDatabase(query);
        if (!results.Success())
            Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to update price for trader item: %i for char_id: %i, the error was: %s\n", ItemID, CharID, results.ErrorMessage().c_str());

        return;
    }

    std::string query = StringFormat("UPDATE trader SET item_cost = %i "
                                    "WHERE char_id = %i AND item_id = %i",
                                    NewPrice, CharID, ItemID);
    auto results = QueryDatabase(query);
    if (!results.Success())
            Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to update price for trader item: %i for char_id: %i, the error was: %s\n", ItemID, CharID, results.ErrorMessage().c_str());
}

void ZoneDatabase::DeleteTraderItem(uint32 char_id){

	if(char_id==0) {
        const std::string query = "DELETE FROM trader";
        auto results = QueryDatabase(query);
		if (!results.Success())
			Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to delete all trader items data, the error was: %s\n", results.ErrorMessage().c_str());

        return;
	}

	std::string query = StringFormat("DELETE FROM trader WHERE char_id = %i", char_id);
	auto results = QueryDatabase(query);
    if (!results.Success())
        Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to delete trader item data for char_id: %i, the error was: %s\n", char_id, results.ErrorMessage().c_str());

}
void ZoneDatabase::DeleteTraderItem(uint32 CharID,uint16 SlotID) {

	std::string query = StringFormat("DELETE FROM trader WHERE char_id = %i AND slot_id = %i", CharID, SlotID);
	auto results = QueryDatabase(query);
	if (!results.Success())
		Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to delete trader item data for char_id: %i, the error was: %s\n",CharID, results.ErrorMessage().c_str());
}

void ZoneDatabase::IncreaseTraderSlot(uint32 CharID,uint16 SlotID) {

	uint16 newslot = SlotID+1;
	std::string query = StringFormat("UPDATE trader set slot_id = %i WHERE char_id = %i AND slot_id = %i", newslot, CharID, SlotID);
	auto results = QueryDatabase(query);
	if (!results.Success())
		Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Failed to increase trader item slot for char_id: %i, the error was: %s\n",CharID, results.ErrorMessage().c_str());
}


bool ZoneDatabase::LoadCharacterData(uint32 character_id, PlayerProfile_Struct* pp, ExtendedProfile_Struct* m_epp){
	std::string query = StringFormat(
		"SELECT                     "
		"`name`,                    "
		"last_name,                 "
		"gender,                    "
		"race,                      "
		"class,                     "
		"`level`,                   "
		"deity,                     "
		"birthday,                  "
		"last_login,                "
		"time_played,               "
		"pvp_status,                "
		"level2,                    "
		"anon,                      "
		"gm,                        "
		"intoxication,              "
		"hair_color,                "
		"beard_color,               "
		"eye_color_1,               "
		"eye_color_2,               "
		"hair_style,                "
		"beard,                     "
		"title,                     "
		"suffix,                    "
		"exp,                       "
		"points,                    "
		"mana,                      "
		"cur_hp,                    "
		"str,                       "
		"sta,                       "
		"cha,                       "
		"dex,                       "
		"`int`,                     "
		"agi,                       "
		"wis,                       "
		"face,                      "
		"y,                         "
		"x,                         "
		"z,                         "
		"heading,                   "
		"autosplit_enabled,         "
		"zone_change_count,         "
		"hunger_level,              "
		"thirst_level,              "
		"zone_id,                   "
		"air_remaining,             "
		"aa_points_spent,           "
		"aa_exp,                    "
		"aa_points,                 "
		"boatid,					"
		"`boatname`,				"
		"showhelm,					"
		"fatigue,					"
		"`e_aa_effects`,			"
		"`e_percent_to_aa`,			"
		"`e_expended_aa_spent`,		"
		"`e_self_found`,			"
		"`e_solo_only`,				"
		"`e_hardcore`,				"
		"`e_hardcore_death_time`,	"
		"`e_betabuff_gear_flag`,    "
		"`e_zone_guild_id`,		    "
		"`e_temp_last_name`,		"
		"`e_married_character_id`,	"
		"`e_char_export_flag`,		"
		"`e_times_rebirthed`		"
		"FROM                       "
		"character_data             "
		"WHERE `id` = %i         ", character_id);
	auto results = database.QueryDatabase(query); int r = 0;
	for (auto& row = results.begin(); row != results.end(); ++row) {
		strcpy(pp->name, row[r]); r++;											 // "`name`,                    "
		strcpy(pp->last_name, row[r]); r++;										 // "last_name,                 "
		pp->gender = atoi(row[r]); r++;											 // "gender,                    "
		pp->race = atoi(row[r]); r++;											 // "race,                      "
		pp->class_ = atoi(row[r]); r++;											 // "class,                     "
		pp->level = atoi(row[r]); r++;											 // "`level`,                   "
		pp->deity = atoi(row[r]); r++;											 // "deity,                     "
		pp->birthday = atoi(row[r]); r++;										 // "birthday,                  "
		pp->lastlogin = atoi(row[r]); r++;										 // "last_login,                "
		pp->timePlayedMin = atoi(row[r]); r++;									 // "time_played,               "
		pp->pvp = atoi(row[r]); r++;											 // "pvp_status,                "
		pp->level2 = atoi(row[r]); r++;											 // "level2,                    "
		pp->anon = atoi(row[r]); r++;											 // "anon,                      "
		pp->gm = atoi(row[r]); r++;												 // "gm,                        "
		pp->intoxication = atoi(row[r]); r++;									 // "intoxication,              "
		pp->haircolor = atoi(row[r]); r++;										 // "hair_color,                "
		pp->beardcolor = atoi(row[r]); r++;										 // "beard_color,               "
		pp->eyecolor1 = atoi(row[r]); r++;										 // "eye_color_1,               "
		pp->eyecolor2 = atoi(row[r]); r++;										 // "eye_color_2,               "
		pp->hairstyle = atoi(row[r]); r++;										 // "hair_style,                "
		pp->beard = atoi(row[r]); r++;											 // "beard,                     "
		strcpy(pp->title, row[r]); r++;											 // "title,                     "
		strcpy(pp->suffix, row[r]); r++;										 // "suffix,                    "
		pp->exp = atoi(row[r]); r++;											 // "exp,                       "
		pp->points = atoi(row[r]); r++;											 // "points,                    "
		pp->mana = atoi(row[r]); r++;											 // "mana,                      "
		pp->cur_hp = atoi(row[r]); r++;											 // "cur_hp,                    "
		pp->STR = atoi(row[r]); r++;											 // "str,                       "
		pp->STA = atoi(row[r]); r++;											 // "sta,                       "
		pp->CHA = atoi(row[r]); r++;											 // "cha,                       "
		pp->DEX = atoi(row[r]); r++;											 // "dex,                       "
		pp->INT = atoi(row[r]); r++;											 // "`int`,                     "
		pp->AGI = atoi(row[r]); r++;											 // "agi,                       "
		pp->WIS = atoi(row[r]); r++;											 // "wis,                       "
		pp->face = atoi(row[r]); r++;											 // "face,                      "
		pp->y = atof(row[r]); r++;												 // "y,                         "
		pp->x = atof(row[r]); r++;												 // "x,                         "
		pp->z = atof(row[r]); r++;												 // "z,                         "
		pp->heading = atof(row[r]); r++;										 // "heading,                   "
		pp->autosplit = atoi(row[r]); r++;										 // "autosplit_enabled,         "
		pp->zone_change_count = atoi(row[r]); r++;								 // "zone_change_count,         "
		pp->hunger_level = atoi(row[r]); r++;									 // "hunger_level,              "
		pp->thirst_level = atoi(row[r]); r++;									 // "thirst_level,              "
		pp->zone_id = atoi(row[r]); r++;										 // "zone_id,                   "
		pp->air_remaining = atoi(row[r]); r++;									 // "air_remaining,             "
		pp->aapoints_spent = atoi(row[r]); r++;									 // "aa_points_spent,           "
		pp->expAA = atoi(row[r]); r++;											 // "aa_exp,                    "
		pp->aapoints = atoi(row[r]); r++;										 // "aa_points,                 "
		pp->boatid = atoi(row[r]); r++;											 // "boatid,					"
		strncpy(pp->boat, row[r], 32); r++;										 // "boatname					"
		pp->showhelm = atobool(row[r]); r++;									 // "showhelm,					"
		pp->fatigue = atoi(row[r]); r++;										 // "fatigue,					"
		m_epp->aa_effects = atoi(row[r]); r++;									 // "`e_aa_effects`,			"
		m_epp->perAA = atoi(row[r]); r++;										 // "`e_percent_to_aa`,			"
		m_epp->expended_aa = atoi(row[r]); r++;									 // "`e_expended_aa_spent`,		"
		m_epp->self_found = atoi(row[r]); r++;									 // "`e_self_found`,			"
		m_epp->solo_only = atoi(row[r]); r++;									 // "`e_solo_only`,				"
		m_epp->hardcore = atoi(row[r]); r++;									 // "`e_hardcore`,				"
		m_epp->hardcore_death_time = atoll(row[r]); r++;						 // "`e_hardcore_death_time",	"
		m_epp->betabuff_gear_flag = atoi(row[r]); r++;							 // "`e_betabuff_gear_flag"		"
		m_epp->zone_guild_id = atoi(row[r]); r++;								 // "`e_zone_guild_id"			"
		strcpy(m_epp->temp_last_name, row[r]); r++;								 // "e_temp_last_name,          "
		m_epp->married_character_id = atoi(row[r]); r++;						 // "`e_married_character_id"	"
		m_epp->char_export_flag = atoi(row[r]); r++;									 // "`e_char_export_flag`			"
		m_epp->e_times_rebirthed = atoi(row[r]); r++;									 // "`e_times_rebirthed`			"
	}
	return true;
}

bool ZoneDatabase::LoadCharacterFactionValues(uint32 character_id, faction_map & val_list) {
	std::string query = StringFormat("SELECT `faction_id`, `current_value` FROM `character_faction_values` WHERE `id` = %i", character_id);
	auto results = database.QueryDatabase(query);
	for (auto& row = results.begin(); row != results.end(); ++row) 
	{ 
		val_list[atoi(row[0])] = atoi(row[1]); 
	}
	return true;
}

bool ZoneDatabase::LoadCharacterMemmedSpells(uint32 character_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat(
		"SELECT							"
		"slot_id,						"
		"`spell_id`						"
		"FROM							"
		"`character_memmed_spells`		"
		"WHERE `id` = %u ORDER BY `slot_id`", character_id);
	auto results = database.QueryDatabase(query);
	int i = 0;
	/* Initialize Spells */
	for (i = 0; i < MAX_PP_MEMSPELL; i++){
		pp->mem_spells[i] = 0xFFFF;
	}
	for (auto& row = results.begin(); row != results.end(); ++row) {
		i = atoi(row[0]);
		if (i < MAX_PP_MEMSPELL && atoi(row[1]) <= SPDAT_RECORDS){
			pp->mem_spells[i] = atoi(row[1]);
		}
	}
	return true;
}

bool ZoneDatabase::LoadCharacterSpellBook(uint32 character_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat(
		"SELECT					"
		"slot_id,				"
		"`spell_id`				"
		"FROM					"
		"`character_spells`		"
		"WHERE `id` = %u ORDER BY `slot_id`", character_id);
	auto results = database.QueryDatabase(query);
	int i = 0;
	/* Initialize Spells */
	for (i = 0; i < MAX_PP_SPELLBOOK; i++){
		pp->spell_book[i] = 0xFFFF;
	}
	for (auto& row = results.begin(); row != results.end(); ++row) {
		i = atoi(row[0]);
		if (i < MAX_PP_SPELLBOOK && atoi(row[1]) <= SPDAT_RECORDS){
			pp->spell_book[i] = atoi(row[1]);
		}
	}
	return true;
}

bool ZoneDatabase::LoadCharacterLanguages(uint32 character_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat(
		"SELECT					"
		"lang_id,				"
		"`value`				"
		"FROM					"
		"`character_languages`	"
		"WHERE `id` = %u ORDER BY `lang_id`", character_id);
	auto results = database.QueryDatabase(query); int i = 0;
	/* Initialize Languages */
	for (i = 0; i < MAX_PP_LANGUAGE; i++){
		pp->languages[i] = 0;
	}
	for (auto& row = results.begin(); row != results.end(); ++row) {
		i = atoi(row[0]);
		if (i < MAX_PP_LANGUAGE){
			pp->languages[i] = atoi(row[1]);
		}
	}
	return true;
}

bool ZoneDatabase::LoadCharacterSkills(uint32 character_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat(
		"SELECT				"
		"skill_id,			"
		"`value`			"
		"FROM				"
		"`character_skills` "
		"WHERE `id` = %u ORDER BY `skill_id`", character_id);
	auto results = database.QueryDatabase(query); int i = 0;
	/* Initialize Skill */
	for (i = 0; i < MAX_PP_SKILL; i++){
		pp->skills[i] = 0;
	}
	for (auto& row = results.begin(); row != results.end(); ++row) {
		i = atoi(row[0]);
		if (i < MAX_PP_SKILL){
			pp->skills[i] = atoi(row[1]);
		}
	}
	return true;
}

bool ZoneDatabase::LoadCharacterLootLockouts(std::map<uint32, LootLockout>& loot_lockout_list, uint32 character_id)
{
	std::string query = StringFormat(
		"SELECT                  "
		"character_id,                   "
		"expiry,                  "
		"npctype_id,                  "
		"npc_name                  "
		"FROM `character_loot_lockouts` WHERE (character_id = %u)", character_id);
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {

		LootLockout lootLockout;
		memset(&lootLockout, 0, sizeof(LootLockout));
		uint32 npctype_id = atoi(row[2]);
		lootLockout.character_id = atoi(row[0]);
		lootLockout.expirydate = atoll(row[1]);
		lootLockout.npctype_id = npctype_id;
		strncpy(lootLockout.npc_name, row[3], 64);
		loot_lockout_list[npctype_id] = lootLockout;
	}

	return true;
}

bool ZoneDatabase::LoadCharacterReimbursements(std::list<TempMerchantList>& reimbursement_list, uint32 character_id)
{
	std::string query = StringFormat(
		"SELECT                   "
		"slot,                    "
		"itemid,                  "
		"charges,                 "
		"quantity                 "
		"FROM `character_reimbursements` WHERE (charid = %u)", character_id);
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {

		TempMerchantList tmpReimbursement = TempMerchantList();
		uint32 charid_id = atoi(row[2]);
		tmpReimbursement.slot = atoi(row[0]);
		tmpReimbursement.item = atoi(row[1]);
		tmpReimbursement.charges = atoi(row[2]);
		tmpReimbursement.origslot = tmpReimbursement.slot;
		tmpReimbursement.quantity = atoi(row[3]);
		tmpReimbursement.npcid = character_id;
		reimbursement_list.emplace_back(tmpReimbursement);
	}

	return true;
}

bool ZoneDatabase::SaveCharacterLootLockout(uint32 character_id, uint32 expiry, uint32 npctype_id, const char* npc_name)
{
	auto zone_id_number = zone ? zone->GetZoneID() : 0;

	std::string query = StringFormat("REPLACE INTO `character_loot_lockouts` (character_id, expiry, npctype_id, npc_name) VALUES (%u, %u, %u, '%s')", character_id, expiry, npctype_id, Strings::Escape(npc_name).c_str());
	auto results = QueryDatabase(query);
	return true;
}

bool ZoneDatabase::DeleteCharacterSkills(uint32 character_id, PlayerProfile_Struct* pp) {
	std::string query = StringFormat(
		"DELETE FROM "
		"`character_skills` "
		"WHERE `id` = %u", character_id);
	auto results = database.QueryDatabase(query);
	return true;
}

bool ZoneDatabase::DeleteCharacterSkill(uint32 character_id, uint32 skill_id) {
	std::string query = StringFormat(
		"DELETE FROM "
		"`character_skills` "
		"WHERE `id` = %u AND skill_id = %u", character_id, skill_id);
	auto results = database.QueryDatabase(query);
	return true;
}

bool ZoneDatabase::LoadCharacterCurrency(uint32 character_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat(
		"SELECT                  "
		"platinum,               "
		"gold,                   "
		"silver,                 "
		"copper,                 "
		"platinum_bank,          "
		"gold_bank,              "
		"silver_bank,            "
		"copper_bank,            "
		"platinum_cursor,        "
		"gold_cursor,            "
		"silver_cursor,          "
		"copper_cursor           "
		"FROM                    "
		"character_currency      "
		"WHERE `id` = %i         ", character_id);
	auto results = database.QueryDatabase(query);
	for (auto& row = results.begin(); row != results.end(); ++row) {
		pp->platinum = atoi(row[0]);
		pp->gold = atoi(row[1]);
		pp->silver = atoi(row[2]);
		pp->copper = atoi(row[3]);
		pp->platinum_bank = atoi(row[4]);
		pp->gold_bank = atoi(row[5]);
		pp->silver_bank = atoi(row[6]);
		pp->copper_bank = atoi(row[7]);
		pp->platinum_cursor = atoi(row[8]);
		pp->gold_cursor = atoi(row[9]);
		pp->silver_cursor = atoi(row[10]);
		pp->copper_cursor = atoi(row[11]);

	}
	return true;
}

bool ZoneDatabase::LoadCharacterBindPoint(uint32 character_id, PlayerProfile_Struct* pp){
	std::string query = StringFormat("SELECT `zone_id`, `x`, `y`, `z`, `heading`, `is_home` FROM `character_bind` WHERE `id` = %u LIMIT 2", character_id);
	auto results = database.QueryDatabase(query); int i = 0;
	for (auto& row = results.begin(); row != results.end(); ++row) {
		i = 0;
		/* Is home bind */
		if (atoi(row[5]) == 1){
			pp->binds[4].zoneId = atoi(row[i++]);
			pp->binds[4].x = atof(row[i++]);
			pp->binds[4].y = atof(row[i++]);
			pp->binds[4].z = atof(row[i++]);
			pp->binds[4].heading = atof(row[i++]);
		}
		/* Is regular bind point */
		else{
			pp->binds[0].zoneId = atoi(row[i++]);
			pp->binds[0].x = atof(row[i++]);
			pp->binds[0].y = atof(row[i++]);
			pp->binds[0].z = atof(row[i++]);
			pp->binds[0].heading = atof(row[i++]);
		}
	}
	return true;
}

bool ZoneDatabase::SaveCharacterLanguage(uint32 character_id, uint32 lang_id, uint32 value){
	std::string query = StringFormat("REPLACE INTO `character_languages` (id, lang_id, value) VALUES (%u, %u, %u)", character_id, lang_id, value); QueryDatabase(query);
	Log(Logs::General, Logs::Character, "ZoneDatabase::SaveCharacterLanguage for character ID: %i, lang_id:%u value:%u done", character_id, lang_id, value);
	return true;
}

void ZoneDatabase::SaveCharacterBinds(Client* c) 
{
	// bulk save character binds
	std::vector<CharacterBindRepository::CharacterBind> binds = {};
	CharacterBindRepository::CharacterBind bind = {};

	// count character binds
	int bind_count = 0;
	for (auto& b : c->GetPP().binds) {
		if (b.zoneId) {
			bind_count++;
		}
	}

	LogDebug("bind count is [{}]", bind_count);

	// allocate memory for binds
	binds.reserve(bind_count);

	// copy binds to vector
	int i = 0;
	for (auto& b : c->GetPP().binds) {
		if (b.zoneId) {
			// copy bind data
			bind.id = c->CharacterID();
			bind.zone_id = b.zoneId;
			bind.x = b.x;
			bind.y = b.y;
			bind.z = b.z;
			bind.heading = b.heading;
			bind.is_home = i;

			binds.emplace_back(bind);

			i++;
		}
	}

	//save binds
	if (bind_count > 0) {
		// delete old binds
		CharacterBindRepository::DeleteWhere(database, fmt::format("id = {}", c->CharacterID()));
		// save new binds
		CharacterBindRepository::InsertMany(database, binds);
	}
}

bool ZoneDatabase::SaveCharacterSkill(uint32 character_id, uint32 skill_id, uint32 value){
	std::string query = StringFormat("REPLACE INTO `character_skills` (id, skill_id, value) VALUES (%u, %u, %u)", character_id, skill_id, value); auto results = QueryDatabase(query);
	Log(Logs::General, Logs::Character, "ZoneDatabase::SaveCharacterSkill for character ID: %i, skill_id:%u value:%u done", character_id, skill_id, value);
	return true;
}

bool ZoneDatabase::SaveCharacterData(uint32 character_id, uint32 account_id, PlayerProfile_Struct* pp, ExtendedProfile_Struct* m_epp)
{
	std::string mail_key = database.GetMailKey(character_id);

	clock_t t = std::clock(); /* Function timer start */
	
	std::string query = StringFormat(
		"REPLACE INTO `character_data` ("
		" id,                        "
		" account_id,                "
		" `name`,                    "
		" last_name,                 "
		" gender,                    "
		" race,                      "
		" class,                     "
		" `level`,                   "
		" deity,                     "
		" birthday,                  "
		" last_login,                "
		" time_played,               "
		" pvp_status,                "
		" level2,                    "
		" anon,                      "
		" gm,                        "
		" intoxication,              "
		" hair_color,                "
		" beard_color,               "
		" eye_color_1,               "
		" eye_color_2,               "
		" hair_style,                "
		" beard,                     "
		" title,                     "
		" suffix,                    "
		" exp,                       "
		" points,                    "
		" mana,                      "
		" cur_hp,                    "
		" str,                       "
		" sta,                       "
		" cha,                       "
		" dex,                       "
		" `int`,                     "
		" agi,                       "
		" wis,                       "
		" face,                      "
		" y,                         "
		" x,                         "
		" z,                         "
		" heading,                   "
		" autosplit_enabled,         "
		" zone_change_count,         "
		" hunger_level,              "
		" thirst_level,              "
		" zone_id,                   "
		" air_remaining,             "
		" aa_points_spent,           "
		" aa_exp,                    "
		" aa_points,                 "
		" boatid,					 "
		" `boatname`,				 "
		" showhelm,					 "
		" fatigue,					 "
		" e_aa_effects,				 "
		" e_percent_to_aa,			 "
		" e_expended_aa_spent,		 "
		" e_self_found,				 "
		" e_solo_only,				 "
		" e_hardcore,				 "
		" e_hardcore_death_time,     "
		" e_betabuff_gear_flag,		 "
		" e_zone_guild_id,			 "
		" e_temp_last_name,			 "
		" e_married_character_id,	 "
		" e_char_export_flag,		 "
		" mailkey,					 "
		" e_times_rebirthed			 "
		")							 "
		"VALUES ("
		"%u,"  // id																" id,                        "
		"%u,"  // account_id														" account_id,                "
		"'%s',"  // `name`					  pp->name,								" `name`,                    "
		"'%s',"  // last_name					pp->last_name,						" last_name,                 "
		"%u,"  // gender					  pp->gender,							" gender,                    "
		"%u,"  // race						  pp->race,								" race,                      "
		"%u,"  // class						  pp->class_,							" class,                     "
		"%u,"  // `level`					  pp->level,							" `level`,                   "
		"%u,"  // deity						  pp->deity,							" deity,                     "
		"%u,"  // birthday					  pp->birthday,							" birthday,                  "
		"%u,"  // last_login				  pp->lastlogin,						" last_login,                "
		"%u,"  // time_played				  pp->timePlayedMin,					" time_played,               "
		"%u,"  // pvp_status				  pp->pvp,								" pvp_status,                "
		"%u,"  // level2					  pp->level2,							" level2,                    "
		"%u,"  // anon						  pp->anon,								" anon,                      "
		"%u,"  // gm						  pp->gm,								" gm,                        "
		"%u,"  // intoxication				  pp->intoxication,						" intoxication,              "
		"%u,"  // hair_color				  pp->haircolor,						" hair_color,                "
		"%u,"  // beard_color				  pp->beardcolor,						" beard_color,               "
		"%u,"  // eye_color_1				  pp->eyecolor1,						" eye_color_1,               "
		"%u,"  // eye_color_2				  pp->eyecolor2,						" eye_color_2,               "
		"%u,"  // hair_style				  pp->hairstyle,						" hair_style,                "
		"%u,"  // beard						  pp->beard,							" beard,                     "
		"'%s',"  // title						  pp->title,						" title,                     "   "
		"'%s',"  // suffix					  pp->suffix,							" suffix,                    "
		"%u,"  // exp						  pp->exp,								" exp,                       "
		"%u,"  // points					  pp->points,							" points,                    "
		"%u,"  // mana						  pp->mana,								" mana,                      "
		"%u,"  // cur_hp					  pp->cur_hp,							" cur_hp,                    "
		"%u,"  // str						  pp->STR,								" str,                       "
		"%u,"  // sta						  pp->STA,								" sta,                       "
		"%u,"  // cha						  pp->CHA,								" cha,                       "
		"%u,"  // dex						  pp->DEX,								" dex,                       "
		"%u,"  // `int`						  pp->INT,								" `int`,                     "
		"%u,"  // agi						  pp->AGI,								" agi,                       "
		"%u,"  // wis						  pp->WIS,								" wis,                       "
		"%u,"  // face						  pp->face,								" face,                      "
		"%f,"  // y							  pp->y,								" y,                         "
		"%f,"  // x							  pp->x,								" x,                         "
		"%f,"  // z							  pp->z,								" z,                         "
		"%f,"  // heading					  pp->heading,							" heading,                   "
		"%u,"  // autosplit_enabled			  pp->autosplit,						" autosplit_enabled,         "
		"%u,"  // zone_change_count			  pp->zone_change_count,				" zone_change_count,         "
		"%i,"  // hunger_level				  pp->hunger_level,						" hunger_level,              "
		"%i,"  // thirst_level				  pp->thirst_level,						" thirst_level,              "
		"%u,"  // zone_id					  pp->zone_id,							" zone_id,                   "
		"%u,"  // air_remaining				  pp->air_remaining,					" air_remaining,             "
		"%u,"  // aa_points_spent			  pp->aapoints_spent,					" aa_points_spent,           "
		"%u,"  // aa_exp					  pp->expAA,							" aa_exp,                    "
		"%u,"  // aa_points					  pp->aapoints,							" aa_points,                 "
		"%u,"  // boatid					  pp->boatid,							" boatid					 "
		"'%s'," // `boatname`				  pp->boat,								" `boatname`,                "
		"%u,"	//showhelm					  pp->showhelm							" showhelm					 "
		"%i,"	//fatigue					  pp->fatigue							" fatigue					 "
		"%u,"  // e_aa_effects
		"%u,"  // e_percent_to_aa
		"%u,"  // e_expended_aa_spent
		"%u,"  // e_self_found
		"%u,"  // e_solo_only
		"%u,"  // e_hardcore
		"%lld," // e_hardcore_death_time
		"%u,"   // e_betabuff_gear_flag
		"%lu,"   // e_zone_guild_id
		"'%s',"  // e_temp_last_name
		"%u,"  // e_married_character_id
		"%u,"  // e_char_export_flag
		"'%s'," // mailkey
		"%u"  // e_times_rebirthed
		")",
		character_id,					  // " id,                        "
		account_id,						  // " account_id,                "
		Strings::Escape(pp->name).c_str(),						  // " `name`,                    "
		Strings::Escape(pp->last_name).c_str(),					  // " last_name,                 "
		pp->gender,						  // " gender,                    "
		pp->race,						  // " race,                      "
		pp->class_,						  // " class,                     "
		pp->level,						  // " `level`,                   "
		pp->deity,						  // " deity,                     "
		pp->birthday,					  // " birthday,                  "
		pp->lastlogin,					  // " last_login,                "
		pp->timePlayedMin,				  // " time_played,               "
		pp->pvp,						  // " pvp_status,                "
		pp->level2,						  // " level2,                    "
		pp->anon,						  // " anon,                      "
		pp->gm,							  // " gm,                        "
		pp->intoxication,				  // " intoxication,              "
		pp->haircolor,					  // " hair_color,                "
		pp->beardcolor,					  // " beard_color,               "
		pp->eyecolor1,					  // " eye_color_1,               "
		pp->eyecolor2,					  // " eye_color_2,               "
		pp->hairstyle,					  // " hair_style,                "
		pp->beard,						  // " beard,                     "
		Strings::Escape(pp->title).c_str(),						  // " title,                     "
		Strings::Escape(pp->suffix).c_str(),						  // " suffix,                    "
		pp->exp,						  // " exp,                       "
		pp->points,						  // " points,                    "
		pp->mana,						  // " mana,                      "
		pp->cur_hp,						  // " cur_hp,                    "
		pp->STR,						  // " str,                       "
		pp->STA,						  // " sta,                       "
		pp->CHA,						  // " cha,                       "
		pp->DEX,						  // " dex,                       "
		pp->INT,						  // " `int`,                     "
		pp->AGI,						  // " agi,                       "
		pp->WIS,						  // " wis,                       "
		pp->face,						  // " face,                      "
		pp->y,							  // " y,                         "
		pp->x,							  // " x,                         "
		pp->z,							  // " z,                         "
		pp->heading,					  // " heading,                   "
		pp->autosplit,					  // " autosplit_enabled,         "
		pp->zone_change_count,			  // " zone_change_count,         "
		pp->hunger_level,				  // " hunger_level,              "
		pp->thirst_level,				  // " thirst_level,              "
		pp->zone_id,					  // " zone_id,                   "
		pp->air_remaining,				  // " air_remaining,             "
		pp->aapoints_spent,				  // " aa_points_spent,           "
		pp->expAA,						  // " aa_exp,                    "
		pp->aapoints,					  // " aa_points,                 "
		pp->boatid,						  // "boatid,					  "
		Strings::Escape(pp->boat).c_str(),	  // " boatname                   "
		pp->showhelm,					  // " showhelm					  "
		pp->fatigue,					  // " fatigue					  "
		m_epp->aa_effects,
		m_epp->perAA,
		m_epp->expended_aa,
		m_epp->self_found,
		m_epp->solo_only,
		m_epp->hardcore,
		m_epp->hardcore_death_time,
		m_epp->betabuff_gear_flag,
		(unsigned long)m_epp->zone_guild_id,
		Strings::Escape(m_epp->temp_last_name).c_str(),
		m_epp->married_character_id,
		m_epp->char_export_flag,
		mail_key.c_str(),
		m_epp->e_times_rebirthed
	);
	auto results = database.QueryDatabase(query);
	Log(Logs::General, Logs::Character, "ZoneDatabase::SaveCharacterData %i, done... Took %f seconds", character_id, ((float)(std::clock() - t)) / CLOCKS_PER_SEC);
	return true;
}

bool ZoneDatabase::SaveCharacterCurrency(uint32 character_id, PlayerProfile_Struct* pp){
	if (pp->copper < 0) { pp->copper = 0; }
	if (pp->silver < 0) { pp->silver = 0; }
	if (pp->gold < 0) { pp->gold = 0; }
	if (pp->platinum < 0) { pp->platinum = 0; }
	if (pp->copper_bank < 0) { pp->copper_bank = 0; }
	if (pp->silver_bank < 0) { pp->silver_bank = 0; }
	if (pp->gold_bank < 0) { pp->gold_bank = 0; }
	if (pp->platinum_bank < 0) { pp->platinum_bank = 0; }
	if (pp->platinum_cursor < 0) { pp->platinum_cursor = 0; }
	if (pp->gold_cursor < 0) { pp->gold_cursor = 0; }
	if (pp->silver_cursor < 0) { pp->silver_cursor = 0; }
	if (pp->copper_cursor < 0) { pp->copper_cursor = 0; }
	std::string query = StringFormat(
		"REPLACE INTO `character_currency` (id, platinum, gold, silver, copper,"
		"platinum_bank, gold_bank, silver_bank, copper_bank,"
		"platinum_cursor, gold_cursor, silver_cursor, copper_cursor)"
		"VALUES (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u)",
		character_id,
		pp->platinum,
		pp->gold,
		pp->silver,
		pp->copper,
		pp->platinum_bank,
		pp->gold_bank,
		pp->silver_bank,
		pp->copper_bank,
		pp->platinum_cursor,
		pp->gold_cursor,
		pp->silver_cursor,
		pp->copper_cursor);
	auto results = database.QueryDatabase(query);
	Log(Logs::General, Logs::Character, "Saving Currency for character ID: %i, done", character_id);
	return true;
}

bool ZoneDatabase::SaveCharacterAA(uint32 character_id, uint32 aa_id, uint32 current_level){
	std::string rquery = StringFormat("REPLACE INTO `character_alternate_abilities` (id, aa_id, aa_value)"
		" VALUES (%u, %u, %u)",
		character_id, aa_id, current_level);
	auto results = QueryDatabase(rquery);
	Log(Logs::General, Logs::Character, "Saving AA for character ID: %u, aa_id: %u current_level: %u", character_id, aa_id, current_level);
	return true;
}

bool ZoneDatabase::SaveCharacterMemorizedSpell(uint32 character_id, uint32 spell_id, uint32 slot_id){
	if (spell_id > SPDAT_RECORDS){ return false; }
	std::string query = StringFormat("REPLACE INTO `character_memmed_spells` (id, slot_id, spell_id) VALUES (%u, %u, %u)", character_id, slot_id, spell_id);
	QueryDatabase(query);
	return true;
}

bool ZoneDatabase::SaveCharacterSpell(uint32 character_id, uint32 spell_id, uint32 slot_id){
	if (spell_id > SPDAT_RECORDS){ return false; }
	std::string query = StringFormat("REPLACE INTO `character_spells` (id, slot_id, spell_id) VALUES (%u, %u, %u)", character_id, slot_id, spell_id);
	QueryDatabase(query);
	return true;
}

bool ZoneDatabase::SaveCharacterConsent(char grantname[64], char ownername[64])
{
	std::string escaped_gname = Strings::Escape(grantname);
	std::string escaped_oname = Strings::Escape(ownername);
	std::string delete_query = StringFormat("DELETE FROM `character_consent` WHERE name = '%s' AND consenter_name = '%s'", escaped_gname.c_str(), escaped_oname.c_str());
	QueryDatabase(delete_query);

	std::string query = StringFormat("SELECT id FROM character_corpses WHERE charname = '%s'", escaped_oname.c_str());
	auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	if (results.RowCount() == 0)
		return false;

	for (auto& row = results.begin(); row != results.end(); ++row)
	{
		uint32 corpse_id = atoul(row[0]);
		std::string query = StringFormat("INSERT INTO `character_consent` (name, consenter_name, corpse_id) VALUES ('%s', '%s', %u)", escaped_gname.c_str(), escaped_oname.c_str(), corpse_id);
		QueryDatabase(query);
	}

	return true;
}

bool ZoneDatabase::SaveCharacterConsent(char grantname[64], char ownername[64], std::list<CharacterConsent> &consent_list)
{
	std::string escaped_gname = Strings::Escape(grantname);
	std::string escaped_oname = Strings::Escape(ownername);
	std::string delete_query = StringFormat("DELETE FROM `character_consent` WHERE name = '%s' AND consenter_name = '%s'", escaped_gname.c_str(), escaped_oname.c_str());
	QueryDatabase(delete_query);

	std::string query = StringFormat("SELECT id FROM character_corpses WHERE charname = '%s'", escaped_oname.c_str());
	auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	if (results.RowCount() == 0)
		return false;

	for (auto& row = results.begin(); row != results.end(); ++row)
	{
		uint32 corpse_id = atoul(row[0]);
		std::string query = StringFormat("INSERT INTO `character_consent` (name, consenter_name, corpse_id) VALUES ('%s', '%s', %u)", escaped_gname.c_str(), escaped_oname.c_str(), corpse_id);
		QueryDatabase(query);

		Log(Logs::Detail, Logs::Corpse, "Adding %s (%u) to %s consent list...", ownername, corpse_id, grantname);
		CharacterConsent cc;
		cc.consenter = ownername;
		cc.corpse_id = corpse_id;
		consent_list.push_back(cc);
	}

	return true;
}

bool ZoneDatabase::SaveAccountShowHelm(uint32 account_id, bool value) 
{
	std::string query = StringFormat("UPDATE `character_data` SET showhelm = %d WHERE account_id = %u", value, account_id);
	QueryDatabase(query);
	Log(Logs::General, Logs::Character, "ZoneDatabase::SaveAccountShowHelm for Account ID: %u, value:%d done", account_id, value);
	return true;
}

bool ZoneDatabase::DeleteCharacterSpell(uint32 character_id, uint32 spell_id, uint32 slot_id){
	std::string query = StringFormat("DELETE FROM `character_spells` WHERE `slot_id` = %u AND `id` = %u", slot_id, character_id);
	QueryDatabase(query);
	return true;
}

bool ZoneDatabase::DeleteCharacterAAs(uint32 character_id){
	std::string query = StringFormat("DELETE FROM `character_alternate_abilities` WHERE `id` = %u", character_id);
	QueryDatabase(query);
	return true;
}

bool ZoneDatabase::DeleteCharacterMemorizedSpell(uint32 character_id, uint32 spell_id, uint32 slot_id){
	std::string query = StringFormat("DELETE FROM `character_memmed_spells` WHERE `slot_id` = %u AND `id` = %u", slot_id, character_id);
	QueryDatabase(query);
	return true;
}

bool ZoneDatabase::DeleteCharacterConsent(char grantname[64], char ownername[64], uint32 corpse_id)
{
	if (corpse_id == 0)
	{
		std::string query = StringFormat("DELETE FROM `character_consent` WHERE `consenter_name` = '%s' AND `name` = '%s'", ownername, grantname);
		QueryDatabase(query);
		return true;
	}
	else
	{
		std::string query = StringFormat("DELETE FROM `character_consent` WHERE `consenter_name` = '%s' AND `name` = '%s' AND corpse_id = %u", ownername, grantname, corpse_id);
		QueryDatabase(query);
		return true;
	}
}

bool ZoneDatabase::LoadSpellModifiers(std::map<std::tuple<int,int,int>, SpellModifier_Struct> &spellModifiers)
{
	std::string query = StringFormat(
		"SELECT "
		"client_cast, "
		"spell_match_id, "
		"zone_id, "
		"tic_duration, "
		"tic_multiplier, "
		"tic_add "
		"FROM `spell_modifiers`");
		
	auto results = database.QueryDatabase(query);
	
	for (auto row = results.begin(); row != results.end(); ++row) {
		SpellModifier_Struct spellModifier;
		memset(&spellModifier, 0, sizeof(SpellModifier_Struct));
		
		spellModifier.client_cast = atoi(row[0]);
		spellModifier.spell_match_id = atoi(row[1]);
		spellModifier.zone_id = atoi(row[2]);
		spellModifier.tic_duration = atoi(row[3]);
		spellModifier.tic_multiplier = atof(row[4]);
		spellModifier.tic_add = atoi(row[5]);
		spellModifiers[std::make_tuple(spellModifier.client_cast,spellModifier.spell_match_id,spellModifier.zone_id)] = spellModifier;
	}

	return true;
}

/* Searches npctable for matching id, and returns the item if found,
 * or nullptr otherwise. If id passed is 0, loads all npc_types for
 * the current zone, returning the last item added.
 */
const NPCType* ZoneDatabase::LoadNPCTypesData(uint32 id, bool bulk_load)
{
	const NPCType *npc = nullptr;

	/* If there is a cached NPC entry, load it */
	auto itr = zone->npctable.find(id);
	if (itr != zone->npctable.end()) {
		return itr->second;
	}

	std::string filter = fmt::format("id = {}", id);

	if (bulk_load) {
		LogDebug("Performing bulk NPC Types load");

		filter = fmt::format(
			SQL(
				id IN(
					select npcID from spawnentry where spawngroupID IN(
						select spawngroupID from spawn2 where `zone` = '{}'
					)
				)
			),
			zone->GetShortName()
		);
	}

	std::vector<uint32> npc_faction_ids;
	std::vector<uint32> loottable_ids;

    // Otherwise, get NPCs from database.
	for (NpcTypesRepository::NpcTypes &n : NpcTypesRepository::GetWhere((Database &)database, filter))
	{
		NPCType *t;
		t = new NPCType;
		memset(t, 0, sizeof * t);

		t->npc_id = n.id;

		strn0cpy(t->name, n.name.c_str(), 50);

		t->level = n.level;
		t->race = n.race;
		t->class_ = n.class_;
		t->max_hp = n.hp;
		t->cur_hp = t->max_hp;
		t->Mana = n.mana;
		t->gender = n.gender;
		t->texture = n.texture;
		t->helmtexture = n.helmtexture;
		t->size = n.size;
		t->loottable_id = n.loottable_id;
		t->merchanttype = n.merchant_id;
		t->attack_delay = n.attack_delay;
		t->STR = n.STR;
		t->STA = n.STA;
		t->DEX = n.DEX;
		t->AGI = n.AGI;
		t->INT = n._INT;
		t->WIS = n.WIS;
		t->CHA = n.CHA;
		t->MR = n.MR;
		t->CR = n.CR;
		t->DR = n.DR;
		t->FR = n.FR;
		t->PR = n.PR;

		t->ignore_despawn = n.ignore_despawn == 1 ? true : false;
		t->min_dmg = n.mindmg;
		t->max_dmg = n.maxdmg;
		t->attack_count = n.attack_count;
		if (!n.special_abilities.empty()) {
			strn0cpy(t->special_abilities, n.special_abilities.c_str(), 512);
		}
		else {
			t->special_abilities[0] = '\0';
		}

		if (n.loottable_id > 0) {
			// check if we already have this loottable_id before inserting it
			if (std::find(loottable_ids.begin(), loottable_ids.end(), n.loottable_id) == loottable_ids.end()) {
				loottable_ids.emplace_back(n.loottable_id);
			}
		}

		t->npc_spells_id = n.npc_spells_id;
		t->npc_spells_effects_id = n.npc_spells_effects_id;
		t->d_melee_texture1 = n.d_melee_texture1;
		t->d_melee_texture2 = n.d_melee_texture2;
		t->walkspeed = n.walkspeed;
		t->prim_melee_type = n.prim_melee_type;
		t->sec_melee_type = n.sec_melee_type;
		t->ranged_type = n.ranged_type;
		t->runspeed = n.runspeed;
		t->aggro_pc = n.aggro_pc == 1 ? true : false;
		t->ignore_distance = n.ignore_distance;
		t->hp_regen = n.hp_regen_rate;
		t->mana_regen = n.mana_regen_rate;

		// set defaultvalue for aggroradius
		t->aggroradius = n.aggroradius;
		if (t->aggroradius <= 0) {
			t->aggroradius = 70;
		}

		t->assistradius = n.assistradius;
		if (t->assistradius <= 0) {
			t->assistradius = t->aggroradius;
		}

		if (n.bodytype > 0) {
			t->bodytype = n.bodytype;
		}
		else {
			t->bodytype = 0;
		}

		t->npc_faction_id = n.npc_faction_id;

		t->luclinface = n.face;
		t->hairstyle = n.luclin_hairstyle;
		t->haircolor = n.luclin_haircolor;
		t->eyecolor1 = n.luclin_eyecolor;
		t->eyecolor2 = n.luclin_eyecolor2;
		t->beardcolor = n.luclin_beardcolor;
		t->beard = n.luclin_beard;

		if (t->npc_faction_id > 0) {
			if (
				std::find(
					npc_faction_ids.begin(),
					npc_faction_ids.end(),
					t->npc_faction_id
				) == npc_faction_ids.end()
				) {
				npc_faction_ids.emplace_back(t->npc_faction_id);
			}
		}

		uint32 armor_tint_id = n.armortint_id;

		t->armor_tint.Head.Color = (n.armortint_red & 0xFF) << 16;
		t->armor_tint.Head.Color |= (n.armortint_green & 0xFF) << 8;
		t->armor_tint.Head.Color |= (n.armortint_blue & 0xFF);
		t->armor_tint.Head.Color |= (t->armor_tint.Head.Color) ? (0xFF << 24) : 0;

		if (armor_tint_id == 0)
			for (int index = EQ::textures::armorChest; index <= EQ::textures::LastTexture; index++)
				t->armor_tint.Slot[index].Color = t->armor_tint.Slot[0].Color;
		else if (t->armor_tint.Slot[0].Color == 0)
		{
			std::string armortint_query = StringFormat(
				"SELECT red1h, grn1h, blu1h, "
				"red2c, grn2c, blu2c, "
				"red3a, grn3a, blu3a, "
				"red4b, grn4b, blu4b, "
				"red5g, grn5g, blu5g, "
				"red6l, grn6l, blu6l, "
				"red7f, grn7f, blu7f, "
				"red8x, grn8x, blu8x, "
				"red9x, grn9x, blu9x "
				"FROM npc_types_tint WHERE id = %d",
				armor_tint_id);

			auto armortint_results = QueryDatabase(armortint_query);
			if (!armortint_results.Success() || armortint_results.RowCount() == 0) {
				armor_tint_id = 0;
			}
			else {
				auto &armorTint_row = armortint_results.begin();

				for (int index = EQ::textures::textureBegin; index <= EQ::textures::LastTexture; index++) {
					t->armor_tint.Slot[index].Color = atoi(armorTint_row[index * 3]) << 16;
					t->armor_tint.Slot[index].Color |= atoi(armorTint_row[index * 3 + 1]) << 8;
					t->armor_tint.Slot[index].Color |= atoi(armorTint_row[index * 3 + 2]);
					t->armor_tint.Slot[index].Color |= (t->armor_tint.Slot[index].Color) ? (0xFF << 24) : 0;
				}
			}
		}
		else {
			armor_tint_id = 0;
		}

		t->see_invis = n.see_invis;
		t->see_invis_undead = n.see_invis_undead == 0 ? false : true;	// Set see_invis_undead flag
		if (!n.lastname.empty()) {
			strn0cpy(t->lastname, n.lastname.c_str(), 32);
		}

		t->qglobal = n.qglobal == 0 ? false : true;	// qglobal
		t->AC = n.AC;
		t->npc_aggro = n.npc_aggro == 0 ? false : true;
		t->spawn_limit = n.spawn_limit;
		t->see_sneak = (uint8)n.see_sneak;
		t->see_improved_hide = (uint8)n.see_improved_hide;
		t->ATK = n.ATK;
		t->accuracy_rating = n.Accuracy;
		t->raid_target = n.raid_target == 0 ? false : true;
		t->slow_mitigation = n.slow_mitigation;
		t->maxlevel = n.maxlevel;
		t->scalerate = n.scalerate;
		t->private_corpse = n.private_corpse == 1 ? true : false;
		t->unique_spawn_by_name = n.unique_spawn_by_name == 1 ? true : false;
		t->underwater = n.underwater == 1 ? true : false;
		t->emoteid = n.emoteid;
		t->spellscale = n.spellscale;
		t->healscale = n.healscale;
		t->light = (n.light & 0x0F);
		t->combat_hp_regen = n.combat_hp_regen;
		t->combat_mana_regen = n.combat_mana_regen;
		t->armtexture = n.armtexture;
		t->bracertexture = n.bracertexture;
		t->handtexture = n.handtexture;
		t->legtexture = n.legtexture;
		t->feettexture = n.feettexture;
		t->chesttexture = n.chesttexture;
		t->avoidance = n.avoidance;
		t->exp_pct = n.exp_pct;
		t->greed = n.greed;
		t->engage_notice = n.engage_notice == 1 ? true : false;
		t->stuck_behavior = n.stuck_behavior;
		t->flymode = n.flymode;
		if (t->flymode < 0 || t->flymode > 3) {
			t->flymode = GravityBehavior::Water;
		}
		
		t->skip_global_loot = n.skip_global_loot;
		t->rare_spawn = n.rare_spawn;
		t->loot_lockout			= n.loot_lockout;
		t->instance_spawn_timer_override = n.instance_spawn_timer_override;

		// If NPC with duplicate NPC id already in table,
		// free item we attempted to add.
		if (zone->npctable.find(t->npc_id) != zone->npctable.end()) {
			std::cerr << "Error loading duplicate NPC " << t->npc_id << std::endl;
			delete t;
			return nullptr;
		}

		zone->npctable[t->npc_id] = t;
		npc = t;

	}

	if (!npc_faction_ids.empty()) {
		zone->LoadNPCFactions(npc_faction_ids);
	}
	
	zone->LoadLootTables(loottable_ids);
	return npc;
}

uint8 ZoneDatabase::GetGridType(uint32 grid, uint32 zoneid) {

	std::string query = StringFormat("SELECT type FROM grid WHERE id = %i AND zoneid = %i", grid, zoneid);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto& row = results.begin();

	return atoi(row[0]);
}

void ZoneDatabase::SaveMerchantTemp(uint32 npcid, uint32 slot, uint32 item, uint32 charges, uint32 quantity){

	std::string query = StringFormat("REPLACE INTO merchantlist_temp (npcid, slot, itemid, charges, quantity) "
                                    "VALUES(%d, %d, %d, %d, %d)", npcid, slot, item, charges, quantity);
    QueryDatabase(query);
}

void ZoneDatabase::DeleteReimbursementItem(uint32 charid, uint32 slot){
	std::string query = StringFormat("DELETE FROM character_reimbursements WHERE charid=%d AND slot=%d", charid, slot);
	QueryDatabase(query);
}

void ZoneDatabase::SaveReimbursementItem(uint32 charid, uint32 slot, uint32 item, uint32 charges, uint32 quantity) 
{
	std::string query = StringFormat("REPLACE INTO character_reimbursements (charid, slot, itemid, charges, quantity) "
		"VALUES(%d, %d, %d, %d, %d)", charid, slot, item, charges, quantity);
	QueryDatabase(query);
}

void ZoneDatabase::DeleteMerchantTemp(uint32 npcid, uint32 slot)
{
	std::string query = StringFormat("DELETE FROM character_reimbursement WHERE charid=%d AND slot=%d", npcid, slot);
	QueryDatabase(query);
}


void ZoneDatabase::DeleteMerchantTempList(uint32 npcid) {
	std::string query = StringFormat("DELETE FROM merchantlist_temp WHERE npcid=%d", npcid);
	QueryDatabase(query);
}


bool ZoneDatabase::UpdateZoneSafeCoords(const char* zonename, const glm::vec3& location) {

	std::string query = StringFormat("UPDATE zone SET safe_x='%f', safe_y='%f', safe_z='%f' "
                                    "WHERE short_name='%s';",
                                    location.x, location.y, location.z, zonename);
	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowsAffected() == 0)
		return false;

	return true;
}

uint8 ZoneDatabase::GetUseCFGSafeCoords()
{
	const std::string query = "SELECT value FROM variables WHERE varname='UseCFGSafeCoords'";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
        return 0;

	auto& row = results.begin();

    return atoi(row[0]);
}

//New functions for timezone
uint32 ZoneDatabase::GetZoneTZ(uint32 zoneid) {

	std::string query = StringFormat("SELECT timezone FROM zone WHERE zoneidnumber = %i",
                                    zoneid);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return 0;
    }

    if (results.RowCount() == 0)
        return 0;

    auto& row = results.begin();
    return atoi(row[0]);
}

bool ZoneDatabase::SetZoneTZ(uint32 zoneid, uint32 tz) {

	std::string query = StringFormat("UPDATE zone SET timezone = %i WHERE zoneidnumber = %i",
                                    tz, zoneid);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return false;
    }

    return results.RowsAffected() == 1;
}

void ZoneDatabase::RefreshGroupFromDB(Client *client){
	if (!client) {
		return;
	}

	Group *group = client->GetGroup();

	if (!group) {
		return;
	}

	auto outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupUpdate_Struct));
	GroupUpdate_Struct* gu = (GroupUpdate_Struct*)outapp->pBuffer;
	gu->action = groupActUpdate;

	strcpy(gu->yourname, client->GetName());
	GetGroupLeadershipInfo(group->GetID(), gu->leadersname);

	int index = 0;

	auto query = fmt::format("SELECT name FROM group_id WHERE groupid = {} ", group->GetID());
	auto results = QueryDatabase(query);

	if (results.Success()) {
		for (auto row : results) {
			if (index >= 6) {
				continue;
			}

			if (!strcmp(client->GetName(), row[0])) {
				continue;
			}

			strcpy(gu->membername[index], row[0]);
			index++;
		}
	}

	client->QueuePacket(outapp);
	safe_delete(outapp);
}

void ZoneDatabase::RefreshGroupLeaderFromDB(Client *client) {
	if (!client)
		return;

	Group *group = client->GetGroup();

	if (!group)
		return;

	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gu = (GroupJoin_Struct*)outapp->pBuffer;
	gu->action = groupActMakeLeader;

	strcpy(gu->yourname, client->GetName());
	GetGroupLeadershipInfo(group->GetID(), gu->membername);

	client->QueuePacket(outapp);
	safe_delete(outapp);
}


uint8 ZoneDatabase::GroupCount(uint32 groupid) {

	std::string query = StringFormat("SELECT count(charid) FROM group_id WHERE groupid = %d", groupid);
	auto results = QueryDatabase(query);
    if (!results.Success()) {
        return 0;
    }

    if (results.RowCount() == 0)
        return 0;

    auto& row = results.begin();

	return atoi(row[0]);
}

uint8 ZoneDatabase::RaidGroupCount(uint32 raidid, uint32 groupid) {

	std::string query = StringFormat("SELECT count(charid) FROM raid_members "
                                    "WHERE raidid = %d AND groupid = %d;", raidid, groupid);
    auto results = QueryDatabase(query);

    if (!results.Success()) {
        return 0;
    }

    if (results.RowCount() == 0)
        return 0;

    auto& row = results.begin();

	return atoi(row[0]);
 }

int32 ZoneDatabase::GetBlockedSpellsCount(uint32 zoneid)
{
	std::string query = StringFormat("SELECT count(*) FROM blocked_spells WHERE zoneid = %d", zoneid);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return -1;
	}

	if (results.RowCount() == 0)
        return -1;

    auto& row = results.begin();

	return atoi(row[0]);
}

bool ZoneDatabase::LoadBlockedSpells(int32 blockedSpellsCount, ZoneSpellsBlocked* into, uint32 zoneid)
{
	Log(Logs::General, Logs::Status, "Loading Blocked Spells from database...");

	std::string query = StringFormat("SELECT id, spellid, type, x, y, z, x_diff, y_diff, z_diff, message "
                                    "FROM blocked_spells WHERE zoneid = %d ORDER BY id ASC", zoneid);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return false;
    }

    if (results.RowCount() == 0)
		return true;

    int32 index = 0;
    for(auto& row = results.begin(); row != results.end(); ++row, ++index) {
        if(index >= blockedSpellsCount) {
            std::cerr << "Error, Blocked Spells Count of " << blockedSpellsCount << " exceeded." << std::endl;
            break;
        }

        memset(&into[index], 0, sizeof(ZoneSpellsBlocked));
        into[index].spellid = atoi(row[1]);
        into[index].type = atoi(row[2]);
        into[index].m_Location = glm::vec3(atof(row[3]), atof(row[4]), atof(row[5]));
        into[index].m_Difference = glm::vec3(atof(row[6]), atof(row[7]), atof(row[8]));
        strn0cpy(into[index].message, row[9], 255);
    }

	return true;
}

int ZoneDatabase::getZoneShutDownDelay(uint32 zoneID)
{
	std::string query = StringFormat("SELECT shutdowndelay FROM zone WHERE zoneidnumber = %i", zoneID);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return (RuleI(Zone, AutoShutdownDelay));
    }

    if (results.RowCount() == 0) {
        std::cerr << "Error in getZoneShutDownDelay no result '" << query << "' " << std::endl;
        return (RuleI(Zone, AutoShutdownDelay));
    }

    auto& row = results.begin();

    return atoi(row[0]);
}

uint32 ZoneDatabase::GetKarma(uint32 acct_id)
{
    std::string query = StringFormat("SELECT `karma` FROM `account` WHERE `id` = '%i' LIMIT 1", acct_id);
    auto results = QueryDatabase(query);
	if (!results.Success())
		return 0;

	for (auto& row = results.begin(); row != results.end(); ++row) {
		return atoi(row[0]);
	}

	return 0;
}

void ZoneDatabase::UpdateKarma(uint32 acct_id, uint32 amount)
{
	std::string query = StringFormat("UPDATE account SET karma = %i WHERE id = %i", amount, acct_id);
    QueryDatabase(query);
}

void ZoneDatabase::QGlobalPurge()
{
	const std::string query = "DELETE FROM quest_globals WHERE expdate < UNIX_TIMESTAMP()";
	database.QueryDatabase(query);
}

void ZoneDatabase::CommandLogs(const char* char_name, const char* acct_name, float y, float x, float z, const char* command, const char* targetType, const char* target, float tar_y, float tar_x, float tar_z, uint32 zone_id, const char* zone_name)
{
	std::string rquery = StringFormat("SHOW TABLES LIKE 'commands_log'");
	auto results = QueryDatabase(rquery);
	if (results.RowCount() == 0){
		rquery = StringFormat(
			"CREATE TABLE	`commands_log` (								"
			"`entry_id`		int(11) NOT NULL AUTO_INCREMENT,				"
			"`char_name`	varchar(64) DEFAULT NULL,						"
			"`acct_name`	varchar(64) DEFAULT NULL,						"
			"`y`			float NOT NULL DEFAULT '0',						"
			"`x`			float NOT NULL DEFAULT '0',						"
			"`z`			float NOT NULL DEFAULT '0',						"
			"`command`		varchar(100) DEFAULT NULL,						"
			"`target_type`	varchar(30) DEFAULT NULL,						"
			"`target`		varchar(64) DEFAULT NULL,						"
			"`tar_y`		float NOT NULL DEFAULT '0',						"
			"`tar_x`		float NOT NULL DEFAULT '0',						"
			"`tar_z`		float NOT NULL DEFAULT '0',						"
			"`zone_id`		int(11) DEFAULT NULL,							"
			"`zone_name`	varchar(30) DEFAULT NULL,						"
			"`time`			datetime DEFAULT NULL,							"
			"PRIMARY KEY(`entry_id`)										"
			") ENGINE = InnoDB AUTO_INCREMENT = 8 DEFAULT CHARSET = latin1;	"
			);
		auto results = QueryDatabase(rquery);
	}
	std::string query = StringFormat("INSERT INTO `commands_log` (char_name, acct_name, y, x, z, command, target_type, target, tar_y, tar_x, tar_z, zone_id, zone_name, time) "
									"VALUES('%s', '%s', '%f', '%f', '%f', '%s', '%s', '%s', '%f', '%f', '%f', '%i', '%s', now())",
			Strings::Escape(char_name).c_str(), acct_name, y, x, z, Strings::Escape(command).c_str(), Strings::Escape(targetType).c_str(), Strings::Escape(target).c_str(), tar_y, tar_x, tar_z, zone_id, zone_name, time);
	auto log_results = QueryDatabase(query);
	if (!log_results.Success())
		Log(Logs::General, Logs::Error, "Error in CommandLogs query '%s': %s", query.c_str(), results.ErrorMessage().c_str());
}

void ZoneDatabase::SaveBuffs(Client *client) {

	// delete the character buffs
	CharacterBuffsRepository::DeleteWhere(database, fmt::format("id = {}", client->CharacterID()));

	// get the character buffs
	uint32 buff_count = client->GetMaxBuffSlots();
	Buffs_Struct *buffs = client->GetBuffs();

	// character buffs struct
	auto b = CharacterBuffsRepository::NewEntity();

	// vector of character buffs
	std::vector<CharacterBuffsRepository::CharacterBuffs> character_buffs = {};

	// count the number of buffs that are valid
	int character_buff_count = 0;
	for (int index = 0; index < buff_count; index++) {
		if (!IsValidSpell(buffs[index].spellid)) {
			continue;
		}
		character_buff_count++;
	}

	// allocate memory for the character buffs
	character_buffs.reserve(character_buff_count);

	for (int index = 0; index < buff_count; index++) {
		if (!IsValidSpell(buffs[index].spellid)) {
			continue;
		}

		// fill in the buff struct
		b.id				= client->CharacterID();
		b.slot_id			= index;
		b.spell_id			= buffs[index].spellid;
		b.caster_level		= buffs[index].realcasterlevel;
		b.caster_name		= buffs[index].caster_name;
		b.ticsremaining		= buffs[index].ticsremaining;
		b.counters			= buffs[index].counters;
		b.melee_rune		= buffs[index].melee_rune;
		b.magic_rune		= buffs[index].magic_rune;
		b.persistent		= buffs[index].persistant_buff;
		b.ExtraDIChance		= buffs[index].ExtraDIChance;
		b.bard_modifier		= buffs[index].instrumentmod;
		b.bufftype			= buffs[index].bufftype;

		// add the buff to the vector

		character_buffs.emplace_back(b);
	}

	// insert the buffs into the database
	if (!character_buffs.empty()) {
		CharacterBuffsRepository::InsertMany(database, character_buffs);
	}
}

void ZoneDatabase::LoadBuffs(Client *client) {

	Buffs_Struct *buffs = client->GetBuffs();
	uint32 max_slots = client->GetMaxBuffSlots();

	for(int index = 0; index < max_slots; ++index)
		buffs[index].spellid = SPELL_UNKNOWN;

	std::string query = StringFormat("SELECT spell_id, slot_id, caster_level, caster_name, ticsremaining, "
                                    "counters, melee_rune, magic_rune, persistent, "
                                    "ExtraDIChance, bard_modifier, bufftype "
                                    "FROM `character_buffs` WHERE `id` = '%u'", client->CharacterID());
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return;
    }

    for (auto& row = results.begin(); row != results.end(); ++row) {
        uint32 slot_id = atoul(row[1]);
		if(slot_id >= client->GetMaxBuffSlots())
			continue;

        uint32 spell_id = atoul(row[0]);
		if(!IsValidSpell(spell_id))
            continue;

        Client *caster = entity_list.GetClientByName(row[3]);
		uint32 caster_level = atoi(row[2]);
		uint32 ticsremaining = atoul(row[4]);
		uint32 counters = atoul(row[5]);
		uint32 melee_rune = atoul(row[6]);
		uint32 magic_rune = atoul(row[7]);
		uint8 persistent = atoul(row[8]);
		int32 ExtraDIChance = atoul(row[9]);
		uint8 instmod = atoul(row[10]);
		int32 bufftype = atoul(row[11]);

		buffs[slot_id].spellid = spell_id;
        buffs[slot_id].casterlevel = caster_level;
		buffs[slot_id].realcasterlevel = caster_level;
        if(caster) {
            buffs[slot_id].casterid = caster->GetID();
			buffs[slot_id].casterlevel = caster->GetLevel();
			buffs[slot_id].realcasterlevel = caster->GetLevel();
            strcpy(buffs[slot_id].caster_name, caster->GetName());
            buffs[slot_id].client = true;
        } else {
			buffs[slot_id].casterid = 0;
			if (strlen(row[3]) > 0) {
				strcpy(buffs[slot_id].caster_name, row[3]);
				buffs[slot_id].client = true;
			}
			else {
				strcpy(buffs[slot_id].caster_name, "");
				buffs[slot_id].client = false;
			}
        }

        buffs[slot_id].ticsremaining = ticsremaining;
		buffs[slot_id].counters = counters;
		buffs[slot_id].melee_rune = melee_rune;
		buffs[slot_id].magic_rune = magic_rune;
		buffs[slot_id].persistant_buff = persistent? true: false;
        buffs[slot_id].ExtraDIChance = ExtraDIChance;
        buffs[slot_id].RootBreakChance = 0;
        buffs[slot_id].UpdateClient = false;
		buffs[slot_id].isdisc = IsDisc(spell_id);
		buffs[slot_id].instrumentmod = instmod;
		buffs[slot_id].bufftype = bufftype;

    }

	max_slots = client->GetMaxBuffSlots();
	for(int index = 0; index < max_slots; ++index) {
		if(!IsValidSpell(buffs[index].spellid))
			continue;

		for(int effectIndex = 0; effectIndex < EFFECT_COUNT; ++effectIndex) {

			if (spells[buffs[index].spellid].effectid[effectIndex] == SE_Charm) {
                buffs[index].spellid = SPELL_UNKNOWN;
                break;
            }

            if (spells[buffs[index].spellid].effectid[effectIndex] == SE_Illusion) {
                if(buffs[index].persistant_buff)
                    break;

                buffs[index].spellid = SPELL_UNKNOWN;
				break;
			}
		}
	}
}

void ZoneDatabase::SavePetInfo(Client *client)
{
	PetInfo *petinfo = nullptr;

	// Pet Info
	std::vector<CharacterPetInfoRepository::CharacterPetInfo> pet_infos = {};
	CharacterPetInfoRepository::CharacterPetInfo pet_info = {};

	// Pet buffs
	std::vector<CharacterPetBuffsRepository::CharacterPetBuffs> pet_buffs = {};
	CharacterPetBuffsRepository::CharacterPetBuffs pet_buff = {};

	// Pet inventory
	std::vector<CharacterPetInventoryRepository::CharacterPetInventory> inventory = {};
	CharacterPetInventoryRepository::CharacterPetInventory item = {};

	// Loop through pet types
	for (int pet = 0; pet < 2; pet++) {
		petinfo = client->GetPetInfo(pet);
		if (!petinfo) {
			continue;
		}

		// build pet info into struct
		pet_info.char_id = client->CharacterID();
		pet_info.pet = pet;
		pet_info.petname = petinfo->Name;
		pet_info.petpower = petinfo->petpower;
		pet_info.spell_id = petinfo->SpellID;
		pet_info.hp = petinfo->HP;
		pet_info.mana = petinfo->Mana;
		pet_info.size = petinfo->size;

		// add pet info to vector
		pet_infos.push_back(pet_info);

		// build pet buffs into struct
		int pet_buff_count = 0;
		int max_slots = RuleI(Spells, MaxTotalSlotsPET);

		// count pet buffs
		for (int index = 0; index < max_slots; index++) {
			if (!IsValidSpell(petinfo->Buffs[index].spellid)) {
				continue;
			}
			pet_buff_count++;
		}
		// reserve space for pet buffs
		pet_buffs.reserve(pet_buff_count);

		// loop through pet buffs
		for (int index = 0; index < max_slots; index++) {
			if (!IsValidSpell(petinfo->Buffs[index].spellid)) {
				continue;
			}

			pet_buff.char_id = client->CharacterID();
			pet_buff.pet = pet;
			pet_buff.slot = index;
			pet_buff.spell_id = petinfo->Buffs[index].spellid;
			pet_buff.caster_level = petinfo->Buffs[index].level;
			pet_buff.ticsremaining = petinfo->Buffs[index].duration;
			pet_buff.counters = petinfo->Buffs[index].counters;

			// add pet buffs to vector
			pet_buffs.push_back(pet_buff);
		}

		// build pet inventory into struct
		int pet_inventory_count = 0;
		for (int index = EQ::invslot::EQUIPMENT_BEGIN; index < EQ::invslot::EQUIPMENT_END; index++) {
			if (!petinfo->Items[index]) {
				continue;
			}
			pet_inventory_count++;
		}

		// loop through pet inventory
		for (int index = EQ::invslot::EQUIPMENT_BEGIN; index <= EQ::invslot::EQUIPMENT_END; index++) {
			if (!petinfo->Items[index]) {
				continue;
			}
			item.char_id = client->CharacterID();
			item.pet = pet;
			item.slot = index;
			item.item_id = petinfo->Items[index];

			// add pet inventory to vector
			inventory.push_back(item);
		}

		// insert pet info into database
		if (!pet_infos.empty()) {
			// Delete existing pet info
			CharacterPetInfoRepository::DeleteWhere(database, fmt::format("char_id = {}", client->CharacterID()));

			// Insert new pet info
			CharacterPetInfoRepository::InsertMany(database, pet_infos);
		}

		// insert pet buffs into database
		if (!pet_buffs.empty()) {
			// Delete existing pet buffs
			CharacterPetBuffsRepository::DeleteWhere(database, fmt::format("char_id = {}", client->CharacterID()));

			// Insert new pet buffs
			CharacterPetBuffsRepository::InsertMany(database, pet_buffs);
		}

		// insert pet inventory into database
		if (!inventory.empty()) {
			// Delete existing pet inventory
			CharacterPetInventoryRepository::DeleteWhere(database, fmt::format("char_id = {}", client->CharacterID()));

			// Insert new pet inventory
			CharacterPetInventoryRepository::InsertMany(database, inventory);
		}
	}
}

void ZoneDatabase::RemoveAllFactions(Client *client) {

	std::string query = StringFormat("DELETE FROM character_faction_values WHERE "
		"id = %u",
		client->CharacterID());
	QueryDatabase(query);
}


void ZoneDatabase::RemoveTempFactions(Client *client) {

	std::string query = StringFormat("DELETE FROM character_faction_values "
                                    "WHERE temp = 1 AND id = %u",
                                    client->CharacterID());
	QueryDatabase(query);
}

void ZoneDatabase::LoadPetInfo(Client *client) {

	// Load current pet and suspended pet
	PetInfo *petinfo = client->GetPetInfo(0);
	PetInfo *suspended = client->GetPetInfo(1);

	memset(petinfo, 0, sizeof(PetInfo));
	memset(suspended, 0, sizeof(PetInfo));

    std::string query = StringFormat("SELECT `pet`, `petname`, `petpower`, `spell_id`, "
                                    "`hp`, `mana`, `size` FROM `character_pet_info` "
                                    "WHERE `char_id` = %u", client->CharacterID());
    auto results = database.QueryDatabase(query);
	if(!results.Success()) {
		return;
	}

    PetInfo *pi;
	for (auto& row = results.begin(); row != results.end(); ++row) {
        uint16 pet = atoi(row[0]);

		if (pet == 0)
			pi = petinfo;
		else if (pet == 1)
			pi = suspended;
		else
			continue;

		strncpy(pi->Name,row[1],64);
		pi->petpower = atoi(row[2]);
		pi->SpellID = atoi(row[3]);
		pi->HP = atoul(row[4]);
		pi->Mana = atoul(row[5]);
		pi->size = atof(row[6]);
	}

    query = StringFormat("SELECT `pet`, `slot`, `spell_id`, `caster_level`, `castername`, "
                        "`ticsremaining`, `counters` FROM `character_pet_buffs` "
                        "WHERE `char_id` = %u", client->CharacterID());
    results = QueryDatabase(query);
    if (!results.Success()) {
		return;
    }

    for (auto& row = results.begin(); row != results.end(); ++row) {
        uint16 pet = atoi(row[0]);
        if (pet == 0)
            pi = petinfo;
        else if (pet == 1)
            pi = suspended;
        else
            continue;

        uint32 slot_id = atoul(row[1]);
        if(slot_id >= RuleI(Spells, MaxTotalSlotsPET))
				continue;

        uint32 spell_id = atoul(row[2]);
        if(!IsValidSpell(spell_id))
            continue;

        uint32 caster_level = atoi(row[3]);
        int caster_id = 0;
        // The castername field is currently unused
        uint16 ticsremaining = atoul(row[5]);
        uint16 counters = atoul(row[6]);

        pi->Buffs[slot_id].spellid = spell_id;
        pi->Buffs[slot_id].level = caster_level;
        pi->Buffs[slot_id].player_id = caster_id;
        pi->Buffs[slot_id].bufftype = 2;	// TODO - don't hardcode this, it can be 4 for reversed effects

        pi->Buffs[slot_id].duration = ticsremaining;
        pi->Buffs[slot_id].counters = counters;
    }

    query = StringFormat("SELECT `pet`, `slot`, `item_id` "
                        "FROM `character_pet_inventory` "
                        "WHERE `char_id`=%u",client->CharacterID());
    results = database.QueryDatabase(query);
    if (!results.Success()) {
		return;
	}

    for(auto& row = results.begin(); row != results.end(); ++row) {
        uint16 pet = atoi(row[0]);
		if (pet == 0)
			pi = petinfo;
		else if (pet == 1)
			pi = suspended;
		else
			continue;

		int slot = atoi(row[1]);
		if (slot < EQ::invslot::EQUIPMENT_BEGIN || slot > EQ::invslot::EQUIPMENT_END)
            continue;

        pi->Items[slot] = atoul(row[2]);
    }

}

bool ZoneDatabase::GetFactionData(FactionMods* fm, uint32 class_mod, uint32 race_mod, uint32 deity_mod, int32 faction_id, uint8 texture_mod, uint8 gender_mod, uint32 base_race, bool skip_illusions)
{
	if (faction_id <= 0 || faction_id > (int32) max_faction)
		return false;

	if (faction_array[faction_id] == 0){
		return false;
	}

	bool isPlayableRace = GetPlayerRaceBit(race_mod) & PLAYER_RACE_ALL_MASK;
	bool isNeutralIllusion = race_mod == MINOR_ILLUSION || race_mod == TREEFORM;

	// skip_illusions will allow the faction to see through ALL illusions
	// see_illusion only allows them to see non playable race illusions
	// this makes little sense but welcome to open source code
	uint32 myRace = race_mod;
	if (skip_illusions || (!isPlayableRace && faction_array[faction_id]->see_illusion))
		myRace = base_race;
	else if (isNeutralIllusion)
		myRace = 0;

	fm->class_mod = 0;
	if(class_mod > 0)
	{
		char str[32];
		sprintf(str, "c%u", class_mod);

		std::map<std::string, int16>::const_iterator iter = faction_array[faction_id]->mods.find(str);
		if(iter != faction_array[faction_id]->mods.end()) 
		{
			fm->class_mod = iter->second;
		}
	} 

	fm->base = 0;	// 'base' is something eqemu made up and we later found out Sony's system doesn't have it.  currently using it as a shortcut way to apply a racial value to all races
	int32 base = faction_array[faction_id]->base;

	fm->race_mod = 0;
	if(myRace > 0)
	{
		fm->race_mod = base;

		char str[32];
		sprintf(str, "r%u", myRace);

		if(myRace == WOLF)
		{
			sprintf(str, "r%um%u", myRace, gender_mod);
		}
		else if(myRace == ELEMENTAL)
		{
			sprintf(str, "r%um%u", myRace, texture_mod);
		}

		auto iter = faction_array[faction_id]->mods.find(str);
		if(iter != faction_array[faction_id]->mods.end())
		{
			fm->race_mod = iter->second + base;
		}
		else if(myRace == ELEMENTAL || myRace == WOLF)
		{
			sprintf(str, "r%u", myRace);
			std::map<std::string, int16>::iterator iter = faction_array[faction_id]->mods.find(str);
			if(iter != faction_array[faction_id]->mods.end())
			{
				fm->race_mod = iter->second + base;
			}
		}
	}

	if(deity_mod > 0)
	{
		char str[32];
		sprintf(str, "d%u", deity_mod);
		fm->deity_mod = 0;

		auto iter = faction_array[faction_id]->mods.find(str);
		if(iter != faction_array[faction_id]->mods.end()) 
		{
			fm->deity_mod = iter->second;
		}
	} 
	else
	{
		fm->deity_mod = 0;
	}

	Log(Logs::Detail, Logs::Faction, "Race: %d BaseRace: %d UsedRace: %d RaceBit: %d Class: %d Deity: %d BaseMod: %i RaceMod: %d ClassMod: %d DeityMod: %d NeutralIllusion: %d PlayableRace: %d", 
		race_mod, base_race, myRace, GetPlayerRaceBit(race_mod), class_mod, deity_mod, fm->base, fm->race_mod, fm->class_mod, fm->deity_mod, isNeutralIllusion, isPlayableRace);
	return true;
}

//o--------------------------------------------------------------
//| Name: GetFactionName; Dec. 16
//o--------------------------------------------------------------
//| Notes: Retrieves the name of the specified faction .Returns false on failure.
//o--------------------------------------------------------------
bool ZoneDatabase::GetFactionName(int32 faction_id, char* name, uint32 buflen) {
	if ((faction_id <= 0) || faction_id > int32(max_faction) ||(faction_array[faction_id] == 0))
		return false;
	if (faction_array[faction_id]->name[0] != 0) {
		strn0cpy(name, faction_array[faction_id]->name, buflen);
		return true;
	}
	return false;

}

std::string ZoneDatabase::GetFactionName(int32 faction_id)
{
	std::string faction_name;
	if (
		faction_id <= 0 ||
		faction_id > static_cast<int>(max_faction) ||
		!faction_array[faction_id]
		) {
		return faction_name;
	}

	faction_name = faction_array[faction_id]->name;

	return faction_name;
}

//o--------------------------------------------------------------
//| Name: SetCharacterFactionLevel; Dec. 20, 2001
//o--------------------------------------------------------------
//| Purpose: Update characters faction level with specified faction_id to specified value. Returns false on failure.
//o--------------------------------------------------------------
bool ZoneDatabase::SetCharacterFactionLevel(uint32 char_id, int32 faction_id, int32 value, uint8 temp, faction_map &val_list)
{

	std::string query;

	if(temp == 2)
		temp = 0;

	if(temp == 3)
		temp = 1;

	query = StringFormat("INSERT INTO `character_faction_values` "
						"(`id`, `faction_id`, `current_value`, `temp`) "
						"VALUES (%i, %i, %i, %i) "
						"ON DUPLICATE KEY UPDATE `current_value`=%i,`temp`=%i",
						char_id, faction_id, value, temp, value, temp);
    auto results = QueryDatabase(query);
	
	if (!results.Success())
		return false;
	else
		val_list[faction_id] = value;

	return true;
}

bool ZoneDatabase::LoadFactionData()
{
	std::string query = "SELECT MAX(id) FROM faction_list";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

    if (results.RowCount() == 0)
        return false;

    auto row = results.begin();

	max_faction = row[0] ? atoi(row[0]) : 0;
    faction_array = new Faction*[max_faction+1];
    for(unsigned int index=0; index<max_faction; index++)
        faction_array[index] = nullptr;

    query = "SELECT id, name, base, see_illusion, min_cap, max_cap FROM faction_list";
    results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

    for (row = results.begin(); row != results.end(); ++row) {
        uint32 index = atoi(row[0]);
		faction_array[index] = new Faction;
		strn0cpy(faction_array[index]->name, row[1], 50);
		faction_array[index]->base = atoi(row[2]);
		faction_array[index]->see_illusion = atobool(row[3]);
		faction_array[index]->min_cap = atoi(row[4]);
		faction_array[index]->max_cap = atoi(row[5]);

        query = StringFormat("SELECT `mod`, `mod_name` FROM `faction_list_mod` WHERE faction_id = %u", index);
        auto modResults = QueryDatabase(query);
        if (!modResults.Success())
            continue;

		for (auto modRow = modResults.begin(); modRow != modResults.end(); ++modRow)
		{
            faction_array[index]->mods[modRow[1]] = atoi(modRow[0]);
		}

    }

	return true;
}

bool ZoneDatabase::GetFactionIDsForNPC(
	uint32 npc_faction_id,
	std::list<NpcFactionEntriesRepository::NpcFactionEntries>* faction_list,
	int32* primary_faction
)
{
	if (npc_faction_id <= 0) {
		faction_list->clear();

		if (primary_faction) {
			*primary_faction = npc_faction_id;
		}

		return true;
	}

	const auto& npcf = zone->GetNPCFaction(npc_faction_id);
	if (!npcf) {
		LogError("No NPC faction entry for [{}]", npc_faction_id);
		return false;
	}

	const auto& l = zone->GetNPCFactionEntries(npc_faction_id);

	if (primary_faction) {
		*primary_faction = npcf->primaryfaction;
	}

	faction_list->clear();

	for (const auto& e: l) {
		faction_list->emplace_back(e);
	}
	return true;
}

bool ZoneDatabase::SameFactions(uint32 npcfaction_id1, uint32 npcfaction_id2) 
{
	if (npcfaction_id1 == 0 || npcfaction_id2 == 0)
		return false;

	if (npcfaction_id1 == npcfaction_id2)
		return true;

	const auto nfl1 = zone->GetNPCFaction(npcfaction_id1);
	const auto nfl2 = zone->GetNPCFaction(npcfaction_id2);

	if (!nfl1 || !nfl2) {
		return false;
	}

	if (nfl1->primaryfaction != nfl2->primaryfaction) {
		return false;
	}

	const auto& l1 = zone->GetNPCFactionEntries(npcfaction_id1);
	const auto& l2 = zone->GetNPCFactionEntries(npcfaction_id2);


	for (const auto& e1 : l1) {
		uint32 faction = nfl1->id;
		// Skip primary faction, since it will cause NPCs to assist, but may not have a faction hit (templeveeshan mobs as an example.)
		if (faction != nfl1->primaryfaction){
			bool found = false;
			for (const auto& e2 : l2) {
				if (faction == nfl2->id) {
					found = true;
					break;
				}
			}

			if (!found)	{
				return false;
			}
		}
	}

	for (const auto& e2 : l2) {
		uint32 faction = nfl2->id;
		if (faction != nfl2->primaryfaction) {
			bool found = false;
			for (const auto& e1 : l1) {
				if (faction == nfl1->id) {
					found = true;
					break;
				}
			}

			if (!found)	{
				return false;
			}
		}
	}

	return true;
}

bool ZoneDatabase::SeeIllusion(int32 faction_id)
{
	if (faction_array[faction_id] == 0)
	{
		return false;
	}
	else
	{
		return faction_array[faction_id]->see_illusion;
	}
}

int16 ZoneDatabase::MinFactionCap(int32 faction_id)
{
	if (faction_array[faction_id] == 0)
	{
		return MIN_PERSONAL_FACTION;
	}
	else
	{
		return faction_array[faction_id]->min_cap;
	}
}

int16 ZoneDatabase::MaxFactionCap(int32 faction_id)
{
	if (faction_array[faction_id] == 0)
	{
		return MAX_PERSONAL_FACTION;
	}
	else
	{
		return faction_array[faction_id]->max_cap;
	}
}


/*  Corpse Queries */

bool ZoneDatabase::DeleteGraveyard(uint32 zone_id, uint32 graveyard_id) {
	std::string query = StringFormat( "UPDATE `zone` SET `graveyard_id` = 0 WHERE `zone_idnumber` = %u", zone_id);
	auto results = QueryDatabase(query);

	query = StringFormat("DELETE FROM `graveyard` WHERE `id` = %u",  graveyard_id);
	auto results2 = QueryDatabase(query);

	if (results.Success() && results2.Success()){
		return true;
	}

	return false;
}

uint32 ZoneDatabase::AddGraveyardIDToZone(uint32 zone_id, uint32 graveyard_id) {
	std::string query = StringFormat(
		"UPDATE `zone` SET `graveyard_id` = %u WHERE `zone_idnumber` = %u",
		graveyard_id, zone_id
	);
	auto results = QueryDatabase(query);
	return zone_id;
}

uint32 ZoneDatabase::CreateGraveyardRecord(uint32 graveyard_zone_id, const glm::vec4& position) {
	std::string query = StringFormat("INSERT INTO `graveyard` "
                                    "SET `zone_id` = %u, `x` = %1.1f, `y` = %1.1f, `z` = %1.1f, `heading` = %1.1f",
                                    graveyard_zone_id, position.x, position.y, position.z, position.w);
	auto results = QueryDatabase(query);
	if (results.Success())
		return results.LastInsertedID();

	return 0;
}
uint32 ZoneDatabase::SendCharacterCorpseToGraveyard(uint32 dbid, uint32 zone_id, uint32 zone_guild_id, const glm::vec4& position) {

	double xcorpse = (position.x + zone->random.Real(-20, 20));
	double ycorpse = (position.y + zone->random.Real(-20, 20));

	std::string query = StringFormat("UPDATE `character_corpses` "
                                    "SET `zone_id` = %u, `zone_guild_id` = %lu,"
                                    "`x` = %1.1f, `y` = %1.1f, `z` = %1.1f, `heading` = %1.1f, "
                                    "`was_at_graveyard` = 1 "
                                    "WHERE `id` = %d",
                                    zone_id, (unsigned long)zone_guild_id, xcorpse, ycorpse, position.z, position.w, dbid);
	QueryDatabase(query);
	return dbid;
}

uint32 ZoneDatabase::GetCharacterCorpseDecayTimer(uint32 corpse_db_id){
	std::string query = StringFormat("SELECT(UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) FROM `character_corpses` WHERE `id` = %d AND NOT `time_of_death` = 0", corpse_db_id);
	auto results = QueryDatabase(query);
	auto& row = results.begin();
	if (results.Success() && results.RowsAffected() != 0){
		return atoul(row[0]); 
	}
	return 0;
}
uint32 ZoneDatabase::UpdateCharacterCorpse(uint32 db_id, uint32 char_id, const char* char_name, uint32 zone_id, uint32 zone_guild_id, CharacterCorpseEntry* corpse, const glm::vec4& position, bool is_rezzed) {
	std::string query = StringFormat("UPDATE `character_corpses` SET \n"
		"`charname` =		  '%s',\n"
		"`zone_id` =				%u,\n"
		"`charid` =				%d,\n"
		"`x` =					%1.1f,\n"
		"`y` =					%1.1f,\n"
		"`z` =					%1.1f,\n"
		"`heading` =			%1.1f,\n"
		"`is_locked` =          %d,\n"
		"`exp` =                 %u,\n"
		"`gmexp` =				%u,\n"
		"`size` =               %f,\n"
		"`level` =              %u,\n"
		"`race` =               %u,\n"
		"`gender` =             %u,\n"
		"`class` =              %u,\n"
		"`deity` =              %u,\n"
		"`texture` =            %u,\n"
		"`helm_texture` =       %u,\n"
		"`copper` =             %u,\n"
		"`silver` =             %u,\n"
		"`gold` =               %u,\n"
		"`platinum` =           %u,\n"
		"`hair_color`  =        %u,\n"
		"`beard_color` =        %u,\n"
		"`eye_color_1` =        %u,\n"
		"`eye_color_2` =        %u,\n"
		"`hair_style`  =        %u,\n"
		"`face` =               %u,\n"
		"`beard` =              %u,\n"
		"`wc_1` =               %u,\n"
		"`wc_2` =               %u,\n"
		"`wc_3` =               %u,\n"
		"`wc_4` =               %u,\n"
		"`wc_5` =               %u,\n"
		"`wc_6` =               %u,\n"
		"`wc_7` =               %u,\n"
		"`wc_8` =               %u,\n"
		"`wc_9`	=               %u,\n"
		"`killedby` =			%u,\n"
		"`rezzable` =			%d,\n"
		"`rez_time` =			%u,\n"
		"`zone_guild_id` =		%lu,\n"
		"`is_rezzed` =			%u \n"
		"WHERE `id` = %u",
		Strings::Escape(char_name).c_str(),
		zone_id,
		char_id,
		position.x,
		position.y,
		position.z,
		position.w,
		corpse->locked,
		corpse->exp,
		corpse->gmexp,
		corpse->size,
		corpse->level,
		corpse->race,
		corpse->gender,
		corpse->class_,
		corpse->deity,
		corpse->texture,
		corpse->helmtexture,
		corpse->copper,
		corpse->silver,
		corpse->gold,
		corpse->plat,
		corpse->haircolor,
		corpse->beardcolor,
		corpse->eyecolor1,
		corpse->eyecolor2,
		corpse->hairstyle,
		corpse->face,
		corpse->beard,
		corpse->item_tint.Head.Color,
		corpse->item_tint.Chest.Color,
		corpse->item_tint.Arms.Color,
		corpse->item_tint.Wrist.Color,
		corpse->item_tint.Hands.Color,
		corpse->item_tint.Legs.Color,
		corpse->item_tint.Feet.Color,
		corpse->item_tint.Primary.Color,
		corpse->item_tint.Secondary.Color,
		corpse->killedby,
		corpse->rezzable,
		corpse->rez_time,
		(unsigned long)zone_guild_id,
		is_rezzed,
		db_id
	);
	auto results = QueryDatabase(query);

	return db_id;
}

bool ZoneDatabase::UpdateCharacterCorpseBackup(uint32 db_id, uint32 char_id, const char* char_name, uint32 zone_id, uint32 zone_guild_id, CharacterCorpseEntry* corpse, const glm::vec4& position, bool is_rezzed) {
	std::string query = StringFormat("UPDATE `character_corpses_backup` SET \n"
		"`charname` =		  '%s',\n"
		"`charid` =				%d,\n"
		"`exp` =                 %u,\n"
		"`gmexp` =				%u,\n"
		"`copper` =             %u,\n"
		"`silver` =             %u,\n"
		"`gold` =               %u,\n"
		"`platinum` =           %u,\n"
		"`rezzable` =           %d,\n"
		"`rez_time` =           %u,\n"
		"`is_rezzed` =          %u \n"
		"WHERE `id` = %u",
		Strings::Escape(char_name).c_str(),
		char_id, 
		corpse->exp,
		corpse->gmexp,
		corpse->copper,
		corpse->silver,
		corpse->gold,
		corpse->plat,
		corpse->rezzable,
		corpse->rez_time,
		is_rezzed,
		db_id
	);
	auto results = QueryDatabase(query);
	if (!results.Success()){
		Log(Logs::Detail, Logs::Error, "UpdateCharacterCorpseBackup query '%s' %s", query.c_str(), results.ErrorMessage().c_str());
		return false;
	}

	return true;
}


void ZoneDatabase::MarkCorpseAsRezzed(uint32 db_id) {
	std::string query = StringFormat("UPDATE `character_corpses` SET `is_rezzed` = 1 WHERE `id` = %i", db_id);
	auto results = QueryDatabase(query);
}

void ZoneDatabase::MarkAllCharacterCorpsesNotRezzable(uint32 character_id) {
	std::string query = StringFormat("UPDATE `character_corpses` SET `rezzable` = 0 WHERE `charid` = %u", character_id);
	auto results = QueryDatabase(query);
}

uint32 ZoneDatabase::SaveCharacterCorpse(uint32 charid, const char* charname, uint32 zoneid, uint32 zoneguildid, CharacterCorpseEntry* corpse, const glm::vec4& position) {
	/* Dump Basic Corpse Data */
	std::string query = StringFormat("INSERT INTO `character_corpses` SET \n"
		"`charname` =		  '%s',\n"
		"`zone_id` =				%u,\n"
		"`charid` =				%d,\n"
		"`x` =					%1.1f,\n"
		"`y` =					%1.1f,\n"
		"`z` =					%1.1f,\n"
		"`heading` =			%1.1f,\n"
		"`time_of_death` =		NOW(),\n"
		"`is_buried` =				0,"
		"`is_locked` =          %d,\n"
		"`exp` =                 %u,\n"
		"`gmexp` =				%u,\n"
		"`size` =               %f,\n"
		"`level` =              %u,\n"
		"`race` =               %u,\n"
		"`gender` =             %u,\n"
		"`class` =              %u,\n"
		"`deity` =              %u,\n"
		"`texture` =            %u,\n"
		"`helm_texture` =       %u,\n"
		"`copper` =             %u,\n"
		"`silver` =             %u,\n"
		"`gold` =               %u,\n"
		"`platinum` =           %u,\n"
		"`hair_color`  =        %u,\n"
		"`beard_color` =        %u,\n"
		"`eye_color_1` =        %u,\n"
		"`eye_color_2` =        %u,\n"
		"`hair_style`  =        %u,\n"
		"`face` =               %u,\n"
		"`beard` =              %u,\n"
		"`wc_1` =               %u,\n"
		"`wc_2` =               %u,\n"
		"`wc_3` =               %u,\n"
		"`wc_4` =               %u,\n"
		"`wc_5` =               %u,\n"
		"`wc_6` =               %u,\n"
		"`wc_7` =               %u,\n"
		"`wc_8` =               %u,\n"
		"`wc_9`	=               %u,\n"
		"`killedby` =			%u,\n"
		"`rezzable` =			%d,\n"
		"`rez_time` =			%u,\n"
		"`zone_guild_id` =		%lu \n",
		Strings::Escape(charname).c_str(),
		zoneid,
		charid,
		position.x,
		position.y,
		position.z,
		position.w,
		corpse->locked,
		corpse->exp,
		corpse->gmexp,
		corpse->size,
		corpse->level,
		corpse->race,
		corpse->gender,
		corpse->class_,
		corpse->deity,
		corpse->texture,
		corpse->helmtexture,
		corpse->copper,
		corpse->silver,
		corpse->gold,
		corpse->plat,
		corpse->haircolor,
		corpse->beardcolor,
		corpse->eyecolor1,
		corpse->eyecolor2,
		corpse->hairstyle,
		corpse->face,
		corpse->beard,
		corpse->item_tint.Head.Color,
		corpse->item_tint.Chest.Color,
		corpse->item_tint.Arms.Color,
		corpse->item_tint.Wrist.Color,
		corpse->item_tint.Hands.Color,
		corpse->item_tint.Legs.Color,
		corpse->item_tint.Feet.Color,
		corpse->item_tint.Primary.Color,
		corpse->item_tint.Secondary.Color,
		corpse->killedby,
		corpse->rezzable,
		corpse->rez_time,
		(unsigned long)zoneguildid
	);
	auto results = QueryDatabase(query);
	uint32 last_insert_id = results.LastInsertedID();

	std::string corpse_items_query;
	/* Dump Items from Inventory */
	if (corpse->itemcount == 0)
	{
		// clear out any stale items that might be left in the database for this corpse if we're saving it with 0 items
		corpse_items_query = StringFormat("DELETE FROM `character_corpse_items` WHERE `corpse_id` = %d", last_insert_id);
	}
	else
	{
		uint8 first_entry = 0;
		for (unsigned int i = 0; i < corpse->itemcount; i++) {
			if (first_entry != 1) {
				corpse_items_query = StringFormat("REPLACE INTO `character_corpse_items` \n"
					" (corpse_id, equip_slot, item_id, charges) \n"
					" VALUES (%u, %u, %u, %u) \n",
					last_insert_id,
					corpse->items[i].equip_slot,
					corpse->items[i].item_id,
					corpse->items[i].charges
				);
				first_entry = 1;
			}
			else {
				corpse_items_query = corpse_items_query + StringFormat(", (%u, %u, %u, %u) \n",
					last_insert_id,
					corpse->items[i].equip_slot,
					corpse->items[i].item_id,
					corpse->items[i].charges
				);
			}
		}
	}

	if (!corpse_items_query.empty())
		QueryDatabase(corpse_items_query);

	return last_insert_id;
}

bool ZoneDatabase::SaveCharacterCorpseBackup(uint32 corpse_id, uint32 charid, const char* charname, uint32 zoneid, uint32 zoneguildid, CharacterCorpseEntry* corpse, const glm::vec4& position) {
	/* Dump Basic Corpse Data */
	std::string query = StringFormat("INSERT INTO `character_corpses_backup` SET \n"
		"`id` =						%u,\n"
		"`charname` =		  '%s',\n"
		"`zone_id` =				%u,\n"
		"`charid` =				%d,\n"
		"`x` =					%1.1f,\n"
		"`y` =					%1.1f,\n"
		"`z` =					%1.1f,\n"
		"`heading` =			%1.1f,\n"
		"`time_of_death` =		NOW(),\n"
		"`is_buried` =				0,"
		"`is_locked` =          %d,\n"
		"`exp` =                 %u,\n"
		"`gmexp` =				%u,\n"
		"`size` =               %f,\n"
		"`level` =              %u,\n"
		"`race` =               %u,\n"
		"`gender` =             %u,\n"
		"`class` =              %u,\n"
		"`deity` =              %u,\n"
		"`texture` =            %u,\n"
		"`helm_texture` =       %u,\n"
		"`copper` =             %u,\n"
		"`silver` =             %u,\n"
		"`gold` =               %u,\n"
		"`platinum` =           %u,\n"
		"`hair_color`  =        %u,\n"
		"`beard_color` =        %u,\n"
		"`eye_color_1` =        %u,\n"
		"`eye_color_2` =        %u,\n"
		"`hair_style`  =        %u,\n"
		"`face` =               %u,\n"
		"`beard` =              %u,\n"
		"`wc_1` =               %u,\n"
		"`wc_2` =               %u,\n"
		"`wc_3` =               %u,\n"
		"`wc_4` =               %u,\n"
		"`wc_5` =               %u,\n"
		"`wc_6` =               %u,\n"
		"`wc_7` =               %u,\n"
		"`wc_8` =               %u,\n"
		"`wc_9`	=               %u,\n"
		"`killedby` =			%u,\n"
		"`rezzable` =			%d,\n"
		"`rez_time` =			%u,\n"
		"`zone_guild_id` =		%lu \n",
		corpse_id,
		Strings::Escape(charname).c_str(),
		zoneid,
		charid,
		position.x,
		position.y,
		position.z,
		position.w,
		corpse->locked,
		corpse->exp,
		corpse->gmexp,
		corpse->size,
		corpse->level,
		corpse->race,
		corpse->gender,
		corpse->class_,
		corpse->deity,
		corpse->texture,
		corpse->helmtexture,
		corpse->copper,
		corpse->silver,
		corpse->gold,
		corpse->plat,
		corpse->haircolor,
		corpse->beardcolor,
		corpse->eyecolor1,
		corpse->eyecolor2,
		corpse->hairstyle,
		corpse->face,
		corpse->beard,
		corpse->item_tint.Head.Color,
		corpse->item_tint.Chest.Color,
		corpse->item_tint.Arms.Color,
		corpse->item_tint.Wrist.Color,
		corpse->item_tint.Hands.Color,
		corpse->item_tint.Legs.Color,
		corpse->item_tint.Feet.Color,
		corpse->item_tint.Primary.Color,
		corpse->item_tint.Secondary.Color,
		corpse->killedby,
		corpse->rezzable,
		corpse->rez_time,
		(unsigned long)zoneguildid
	);
	auto results = QueryDatabase(query); 
	if (!results.Success()){
		Log(Logs::Detail, Logs::Error, "Error inserting character_corpses_backup.");
		return false;
	}

	/* Dump Items from Inventory */
	uint8 first_entry = 0;
	for (unsigned int i = 0; i < corpse->itemcount; i++) {
		if (first_entry != 1){
			query = StringFormat("REPLACE INTO `character_corpse_items_backup` \n"
				" (corpse_id, equip_slot, item_id, charges) \n"
				" VALUES (%u, %u, %u, %u) \n",
				corpse_id, 
				corpse->items[i].equip_slot,
				corpse->items[i].item_id,
				corpse->items[i].charges
			);
			first_entry = 1;
		}
		else{ 
			query = query + StringFormat(", (%u, %u, %u, %u) \n",
				corpse_id,
				corpse->items[i].equip_slot,
				corpse->items[i].item_id,
				corpse->items[i].charges
			);
		}
	}
	auto sc_results = QueryDatabase(query); 
	if (!sc_results.Success()){
		Log(Logs::Detail, Logs::Error, "Error inserting character_corpse_items_backup.");
		return false;
	}
	return true;
}

uint32 ZoneDatabase::GetCharacterBuriedCorpseCount(uint32 char_id) {
	std::string query = StringFormat("SELECT COUNT(*) FROM `character_corpses` WHERE `charid` = '%u' AND `is_buried` = 1", char_id);
	auto results = QueryDatabase(query);

	for (auto& row = results.begin(); row != results.end(); ++row) {
		return atoi(row[0]);
	}
	return 0;
}

uint32 ZoneDatabase::GetCharacterCorpseCount(uint32 char_id) {
	std::string query = StringFormat("SELECT COUNT(*) FROM `character_corpses` WHERE `charid` = '%u' AND `is_buried` = 0", char_id);
	auto results = QueryDatabase(query);

	for (auto& row = results.begin(); row != results.end(); ++row) {
		return atoi(row[0]);
	}
	return 0;
}

uint32 ZoneDatabase::GetCharacterCorpseID(uint32 char_id, uint8 corpse) {
	std::string query = StringFormat("SELECT `id` FROM `character_corpses` WHERE `charid` = '%u'", char_id);
	auto results = QueryDatabase(query);

	for (auto& row = results.begin(); row != results.end(); ++row) {
		for (int i = 0; i < corpse; i++) {
			return atoul(row[0]);
		}
	}
	return 0;
}

uint32 ZoneDatabase::GetCharacterCorpseItemCount(uint32 corpse_id) {
	std::string query = StringFormat("SELECT COUNT(*) FROM character_corpse_items WHERE `corpse_id` = %u",
		corpse_id
	);
	auto results = QueryDatabase(query);
	auto& row = results.begin();
	if (results.Success() && results.RowsAffected() != 0) {
		return atoi(row[0]);
	}
	return 0;
}

uint32 ZoneDatabase::GetCharacterCorpseItemAt(uint32 corpse_id, uint16 slotid) {
	Corpse* tmp = LoadCharacterCorpse(corpse_id);
	uint32 itemid = 0;

	if (tmp) {
		itemid = tmp->GetWornItem(slotid);
		tmp->DepopPlayerCorpse();
	}
	return itemid;
}

bool ZoneDatabase::LoadCharacterCorpseRezData(uint32 corpse_id, uint32 *exp, uint32 *gmexp, bool *rezzable, bool *is_rezzed)
{
	std::string query = StringFormat(
		"SELECT           \n"
		"exp,             \n"
		"gmexp,			  \n"
		"rezzable,		  \n"
		"is_rezzed		  \n"
		"FROM             \n"
		"character_corpses\n"
		"WHERE `id` = %u  LIMIT 1\n",
		corpse_id
	);
	auto results = QueryDatabase(query);
	uint16 i = 0;
	for (auto& row = results.begin(); row != results.end(); ++row) 
	{
		*exp = atoul(row[i++]);							// exp,
		*gmexp = atoul(row[i++]);						// gmexp,
		*rezzable = atoi(row[i++]);						// rezzable
		*is_rezzed = atoi(row[i++]);					// is_rezzed

		return true;
	}
	return false;
}

bool ZoneDatabase::LoadCharacterCorpseData(uint32 corpse_id, CharacterCorpseEntry* corpse){
	std::string query = StringFormat(
		"SELECT           \n"
		"is_locked,       \n"
		"exp,             \n"
		"gmexp,			  \n"
		"size,            \n"
		"`level`,         \n"
		"race,            \n"
		"gender,          \n"
		"class,           \n"
		"deity,           \n"
		"texture,         \n"
		"helm_texture,    \n"
		"copper,          \n"
		"silver,          \n"
		"gold,            \n"
		"platinum,        \n"
		"hair_color,      \n"
		"beard_color,     \n"
		"eye_color_1,     \n"
		"eye_color_2,     \n"
		"hair_style,      \n"
		"face,            \n"
		"beard,           \n"
		"wc_1,            \n"
		"wc_2,            \n"
		"wc_3,            \n"
		"wc_4,            \n"
		"wc_5,            \n"
		"wc_6,            \n"
		"wc_7,            \n"
		"wc_8,            \n"
		"wc_9,             \n"
		"killedby,		  \n"
		"rezzable,		  \n"
		"rez_time		  \n"
		"FROM             \n"
		"character_corpses\n"
		"WHERE `id` = %u  LIMIT 1\n",
		corpse_id
	);
	auto results = QueryDatabase(query);
	uint16 i = 0;
	for (auto& row = results.begin(); row != results.end(); ++row) {
		corpse->locked = atoi(row[i++]);						// is_locked,
		corpse->exp = atoul(row[i++]);							// exp,
		corpse->gmexp = atoul(row[i++]);						// gmexp,
		corpse->size = atof(row[i++]);							// size,
		corpse->level = atoi(row[i++]);						// `level`,
		corpse->race = atoi(row[i++]);							// race,
		corpse->gender = atoi(row[i++]);						// gender,
		corpse->class_ = atoi(row[i++]);						// class,
		corpse->deity = atoi(row[i++]);						// deity,
		corpse->texture = atoi(row[i++]);						// texture,
		corpse->helmtexture = atoi(row[i++]);					// helm_texture,
		corpse->copper = atoul(row[i++]);						// copper,
		corpse->silver = atoul(row[i++]);						// silver,
		corpse->gold = atoul(row[i++]);						// gold,
		corpse->plat = atoul(row[i++]);						// platinum,
		corpse->haircolor = atoi(row[i++]);					// hair_color,
		corpse->beardcolor = atoi(row[i++]);					// beard_color,
		corpse->eyecolor1 = atoi(row[i++]);					// eye_color_1,
		corpse->eyecolor2 = atoi(row[i++]);					// eye_color_2,
		corpse->hairstyle = atoi(row[i++]);					// hair_style,
		corpse->face = atoi(row[i++]);							// face,
		corpse->beard = atoi(row[i++]);						// beard,
		corpse->item_tint.Head.Color = atoul(row[i++]);		// wc_1,
		corpse->item_tint.Chest.Color = atoul(row[i++]);		// wc_2,
		corpse->item_tint.Arms.Color = atoul(row[i++]);		// wc_3,
		corpse->item_tint.Wrist.Color = atoul(row[i++]);		// wc_4,
		corpse->item_tint.Hands.Color = atoul(row[i++]);		// wc_5,
		corpse->item_tint.Legs.Color = atoul(row[i++]);		// wc_6,
		corpse->item_tint.Feet.Color = atoul(row[i++]);		// wc_7,
		corpse->item_tint.Primary.Color = atoul(row[i++]);		// wc_8,
		corpse->item_tint.Secondary.Color = atoul(row[i++]);	// wc_9
		corpse->killedby = atoi(row[i++]);						// killedby
		corpse->rezzable = atoi(row[i++]);						// rezzable
		corpse->rez_time = atoul(row[i++]);					// rez_time
	}
	query = StringFormat(
		"SELECT                       \n"
		"equip_slot,                  \n"
		"item_id,                     \n"
		"charges                      \n"
		"FROM                         \n"
		"character_corpse_items       \n"
		"WHERE `corpse_id` = %u\n"
		,
		corpse_id
	);
	results = QueryDatabase(query);

	i = 0;
	corpse->itemcount = results.RowCount();
	uint16 r = 0;
	for (auto& row = results.begin(); row != results.end(); ++row) {
		memset(&corpse->items[i], 0, sizeof(LootItem));
		corpse->items[i].equip_slot = atoi(row[r++]);		// equip_slot,
		corpse->items[i].item_id = atoul(row[r++]); 		// item_id,
		corpse->items[i].charges = atoi(row[r++]); 		// charges,
		corpse->items[i].min_looter_level = 0;
		corpse->items[i].item_loot_lockout_timer = 0;
		corpse->items[i].quarm_item_data = EmptyQuarmItemData; // NYI: Corpse Persistence for QuarmItemData
		r = 0;
		i++;
	}

	return true;
}

Corpse* ZoneDatabase::SummonBuriedCharacterCorpses(uint32 char_id, uint32 dest_zone_id, uint32 dest_zone_guild_id, const glm::vec4& position) {
	Corpse* corpse = nullptr;
	std::string query = StringFormat("SELECT `id`, `charname`, UNIX_TIMESTAMP(time_of_death), `is_rezzed` "
                                    "FROM `character_corpses` "
                                    "WHERE `charid` = '%u' AND `is_buried` = 1 "
                                    "ORDER BY `time_of_death` LIMIT 1",
                                    char_id);
	auto results = QueryDatabase(query);

	for (auto& row = results.begin(); row != results.end(); ++row) {
		corpse = Corpse::LoadCharacterCorpseEntity(
			atoul(row[0]), 			 // uint32 in_dbid
			char_id, 				 // uint32 in_charid
			row[1], 				 // char* in_charname
			position,
			atoul(row[2]), 				 // char* time_of_death
			atoi(row[3]) == 1, 		 // bool rezzed
			false					 // bool was_at_graveyard
		);
		if (corpse) {
			entity_list.AddCorpse(corpse);
			int32 corpse_decay = 0;
			if(corpse->IsEmpty())
			{
				corpse_decay = RuleI(Character, EmptyCorpseDecayTimeMS);
			}
			else
			{
				corpse_decay = RuleI(Character, CorpseDecayTimeMS);
			}
			corpse->SetDecayTimer(corpse_decay);
			corpse->Spawn();
			if (!UnburyCharacterCorpse(corpse->GetCorpseDBID(), dest_zone_id, dest_zone_guild_id, position))
				Log(Logs::Detail, Logs::Error, "Unable to unbury a summoned player corpse for character id %u.", char_id);
		}
	}

	return corpse;
}

Corpse* ZoneDatabase::SummonCharacterCorpse(uint32 corpse_id, uint32 char_id, uint32 dest_zone_id, uint32 dest_zone_guild_id, const glm::vec4& position) {
	Corpse* NewCorpse = 0;
	std::string query = StringFormat(
		"SELECT `id`, `charid`, UNIX_TIMESTAMP(time_of_death), `is_rezzed`, `charname` FROM `character_corpses` WHERE `id` = %u", 
		corpse_id
	);
	auto results = QueryDatabase(query);

	for (auto& row = results.begin(); row != results.end(); ++row) {
		NewCorpse = Corpse::LoadCharacterCorpseEntity(
			atoul(row[0]), 			 // uint32 in_dbid
			atoul(row[1]), 				 // uint32 in_charid
			row[4], 				 // char* in_charname
			position,
			atoul(row[2]), 				 // char* time_of_death
			atoi(row[3]) == 1, 		 // bool rezzed
			false					 // bool was_at_graveyard
		);
		if (NewCorpse) { 
			entity_list.AddCorpse(NewCorpse);
			int32 corpse_decay = 0;
			if(NewCorpse->IsEmpty())
			{
				corpse_decay = RuleI(Character, EmptyCorpseDecayTimeMS);
			}
			else
			{
				corpse_decay = RuleI(Character, CorpseDecayTimeMS);
			}
			NewCorpse->SetDecayTimer(corpse_decay);
			NewCorpse->Spawn();
		}
	}

	return NewCorpse;
}

bool ZoneDatabase::SummonAllCharacterCorpses(uint32 char_id, uint32 dest_zone_id, uint32 dest_zone_guild_id, const glm::vec4& position) {
	Corpse* NewCorpse = 0;
	int CorpseCount = 0;

	std::string update_query = StringFormat(
		"UPDATE character_corpses SET zone_id = %i, zone_guild_id=%lu, x = %f, y = %f, z = %f, heading = %f, is_buried = 0, was_at_graveyard = 0 WHERE charid = %i",
		dest_zone_id, (unsigned long)dest_zone_guild_id, position.x, position.y, position.z, position.w, char_id
	);
	auto results = QueryDatabase(update_query);

	std::string select_query = StringFormat(
		"SELECT `id`, `charname`, UNIX_TIMESTAMP(time_of_death), `is_rezzed` FROM `character_corpses` WHERE `charid` = '%u'"
		"ORDER BY time_of_death",
		char_id);
	results = QueryDatabase(select_query);

	for (auto& row = results.begin(); row != results.end(); ++row) {
		NewCorpse = Corpse::LoadCharacterCorpseEntity(
			atoul(row[0]),
			char_id,
			row[1],
			position,
			atoul(row[2]),
			atoi(row[3]) == 1,
			false);
		if (NewCorpse) {
			entity_list.AddCorpse(NewCorpse);
			int32 corpse_decay = 0;
			if(NewCorpse->IsEmpty())
			{
				corpse_decay = RuleI(Character, EmptyCorpseDecayTimeMS);
			}
			else
			{
				corpse_decay = RuleI(Character, CorpseDecayTimeMS);
			}
			NewCorpse->SetDecayTimer(corpse_decay);
			NewCorpse->Spawn();
			++CorpseCount;
		}
		else{
			Log(Logs::General, Logs::Error, "Unable to construct a player corpse for character id %u.", char_id);
		}
	}

	return (CorpseCount > 0);
}

bool ZoneDatabase::UnburyCharacterCorpse(uint32 db_id, uint32 new_zone_id, uint32 new_zone_guild_id, const glm::vec4& position) {
	std::string query = StringFormat("UPDATE `character_corpses` "
                                    "SET `is_buried` = 0, `zone_id` = %u, zone_guild_id = %lu, "
                                    "`x` = %f, `y` = %f, `z` = %f, `heading` = %f, "
                                    "`time_of_death` = Now(), `was_at_graveyard` = 0 "
                                    "WHERE `id` = %u",
                                    new_zone_id, (unsigned long)new_zone_guild_id,
                                    position.x, position.y, position.z, position.w, db_id);
	auto results = QueryDatabase(query);
	if (results.Success() && results.RowsAffected() != 0)
		return true;

	return false;
}

Corpse* ZoneDatabase::LoadCharacterCorpse(uint32 player_corpse_id) {
	Corpse* NewCorpse = 0;
	std::string query = StringFormat(
		"SELECT `id`, `charid`, `charname`, `x`, `y`, `z`, `heading`, UNIX_TIMESTAMP(time_of_death), `is_rezzed`, `was_at_graveyard` FROM `character_corpses` WHERE `id` = '%u' LIMIT 1",
		player_corpse_id
	);
	auto results = QueryDatabase(query);
	for (auto& row = results.begin(); row != results.end(); ++row) {
        auto position = glm::vec4(atof(row[3]), atof(row[4]), atof(row[5]), atof(row[6]));
		NewCorpse = Corpse::LoadCharacterCorpseEntity(
				atoul(row[0]), 		 // id					  uint32 in_dbid
				atoul(row[1]),		 // charid				  uint32 in_charid
				row[2], 			 //	char_name
				position,
				atoul(row[7]),				 // time_of_death		  char* time_of_death
				atoi(row[8]) == 1, 	 // is_rezzed			  bool rezzed
				atoi(row[9])		 // was_at_graveyard	  bool was_at_graveyard
			);
		entity_list.AddCorpse(NewCorpse);
	}
	return NewCorpse;
}

bool ZoneDatabase::LoadCharacterCorpses(uint32 zone_id, uint32 zone_guild_id) {
	std::string query;
	query = StringFormat("SELECT id, charid, charname, x, y, z, heading, UNIX_TIMESTAMP(time_of_death), is_rezzed, was_at_graveyard FROM character_corpses WHERE zone_id='%u' and zone_guild_id = '%lu'", zone_id, (unsigned long)zone_guild_id);

	auto results = QueryDatabase(query);
	for (auto& row = results.begin(); row != results.end(); ++row) {
        auto position = glm::vec4(atof(row[3]), atof(row[4]), atof(row[5]), atof(row[6]));
		entity_list.AddCorpse(
			 Corpse::LoadCharacterCorpseEntity(
				atoul(row[0]), 		  // id					  uint32 in_dbid
				atoul(row[1]), 		  // charid				  uint32 in_charid
				row[2], 			  //					  char_name
				position,
				atoul(row[7]), 			  // time_of_death		  char* time_of_death
				atoi(row[8]) == 1, 	  // is_rezzed			  bool rezzed
				atoi(row[9]))
		);
	}

	return true;
}

uint32 ZoneDatabase::GetFirstCorpseID(uint32 char_id) {
	std::string query = StringFormat("SELECT `id` FROM `character_corpses` WHERE `charid` = '%u' AND `is_buried` = 0 ORDER BY `time_of_death` LIMIT 1", char_id);
	auto results = QueryDatabase(query);
	for (auto& row = results.begin(); row != results.end(); ++row) {
		return atoi(row[0]);
	}
	return 0;
}

bool ZoneDatabase::DeleteItemOffCharacterCorpse(uint32 db_id, uint32 equip_slot, uint32 item_id){
	std::string query = StringFormat("DELETE FROM `character_corpse_items` WHERE `corpse_id` = %u AND equip_slot = %u AND item_id = %u", db_id, equip_slot, item_id);
	auto results = QueryDatabase(query);
	if (!results.Success()){
		return false;
	}
	if(RuleB(Character, UsePlayerCorpseBackups))
	{
		std::string query = StringFormat("DELETE FROM `character_corpse_items_backup` WHERE `corpse_id` = %u AND equip_slot = %u AND item_id = %u", db_id, equip_slot, item_id);
		auto results = QueryDatabase(query);
		if (!results.Success()){
			return false;
		}
	}
	return true;
}

bool ZoneDatabase::BuryCharacterCorpse(uint32 db_id) {
	std::string query = StringFormat("UPDATE `character_corpses` SET `is_buried` = 1 WHERE `id` = %u", db_id);
	auto results = QueryDatabase(query);
	if (results.Success() && results.RowsAffected() != 0){
		return true;
	}
	return false;
}

bool ZoneDatabase::BuryAllCharacterCorpses(uint32 char_id) {
	std::string query = StringFormat("SELECT `id` FROM `character_corpses` WHERE `charid` = %u", char_id);
	auto results = QueryDatabase(query);
	for (auto& row = results.begin(); row != results.end(); ++row) {
		BuryCharacterCorpse(atoi(row[0]));
		return true;
	}
	return false;
}

bool ZoneDatabase::DeleteCharacterCorpse(uint32 db_id) {
	std::string query = StringFormat("DELETE FROM `character_corpses` WHERE `id` = %d", db_id);
	auto results = QueryDatabase(query);
	if (!results.Success()){ 
		return false;
	}
	std::string ci_query = StringFormat("DELETE FROM `character_corpse_items` WHERE `corpse_id` = %d", db_id);
	auto ci_results = QueryDatabase(ci_query);
	if (!ci_results.Success()){ 
		return false;
	}
	return true;
}

bool ZoneDatabase::IsValidCorpseBackup(uint32 corpse_id) {
	std::string query = StringFormat("SELECT COUNT(*) FROM `character_corpses_backup` WHERE `id` = %d", corpse_id);
	auto results = QueryDatabase(query);
	auto& row = results.begin();
	if(atoi(row[0]) == 1)
		return true;

	return false;
}

bool ZoneDatabase::IsValidCorpse(uint32 corpse_id) {
	std::string query = StringFormat("SELECT COUNT(*) FROM `character_corpses` WHERE `id` = %d", corpse_id);
	auto results = QueryDatabase(query);
	auto& row = results.begin();
	if(atoi(row[0]) == 1)
		return true;

	return false;
}

bool ZoneDatabase::CopyBackupCorpse(uint32 corpse_id) {
	std::string query = StringFormat("INSERT INTO `character_corpses` SELECT * from `character_corpses_backup` WHERE `id` = %d", corpse_id);
	auto results = QueryDatabase(query);
	if (!results.Success()){ 
		return false;
	}
	std::string tod_query = StringFormat("UPDATE `character_corpses` SET `time_of_death` = NOW() WHERE `id` = %d", corpse_id);
	auto tod_results = QueryDatabase(tod_query);
	if (!tod_results.Success()){ 
		return false;
	}
	std::string ci_query = StringFormat("REPLACE INTO `character_corpse_items` SELECT * from `character_corpse_items_backup` WHERE `corpse_id` = %d", corpse_id);
	auto ci_results = QueryDatabase(ci_query);
	if (!ci_results.Success()){ 
		Log(Logs::Detail, Logs::Error, "CopyBackupCorpse() Error replacing items.");
	}

	return true;
}

bool ZoneDatabase::IsCorpseBackupOwner(uint32 corpse_id, uint32 char_id) {
	std::string query = StringFormat("SELECT COUNT(*) FROM `character_corpses_backup` WHERE `id` = %d AND `charid` = %d", corpse_id, char_id);
	auto results = QueryDatabase(query);
	auto& row = results.begin();
	if(atoi(row[0]) == 1)
		return true;

	return false;
}

int8 ZoneDatabase::ItemQuantityType(int16 item_id)
{
	const EQ::ItemData* item = database.GetItem(item_id);
	if(item)
	{
		// Item does not have quantity and is not stackable or is lore. (Normal item.)
		if ((item->MaxCharges < 1 && (item->StackSize <= 1 || !item->Stackable)) || 
			(item->MaxCharges == 1 && item->StackSize > 1 && item->Stackable && (item->Lore[0] == '*' || item->Lore[0] == '#')))
		{ 
			return EQ::item::Quantity_Normal;
		}
		//Item is not stackable, and uses charges.
		else if(item->StackSize < 1 || !item->Stackable) 
		{
			return EQ::item::Quantity_Charges;
		}
		//Due to the previous checks, item has to stack.
		else
		{
			return EQ::item::Quantity_Stacked;
		}
	}

	return EQ::item::Quantity_Unknown;
}

bool ZoneDatabase::RetrieveMBMessages(uint16 category, std::vector<MBMessageRetrievalGen_Struct>& outData) {
	std::string query = StringFormat("SELECT id, date, language, author, subject, category from mb_messages WHERE category=%i ORDER BY time DESC LIMIT %i",category, 500);
	auto results = QueryDatabase(query);
	if(!results.Success())
	{
		return false; // no messages
	}

	for (auto& row = results.begin(); row != results.end(); ++row) {
		MBMessageRetrievalGen_Struct message;
		message.id = atoi(row[0]);			
		strncpy(message.date, row[1], sizeof(message.date));		
		message.language = atoi(row[2]);
		strncpy(message.author, row[3],  sizeof(message.author));
		strncpy(message.subject, row[4], sizeof(message.subject));
		message.category = atoi(row[5]);
		outData.push_back(message);
	}
	return false;
}

bool ZoneDatabase::PostMBMessage(uint32 charid, const char* charName, MBMessageRetrievalGen_Struct* inData) {
	std::string query = StringFormat("INSERT INTO mb_messages (charid, date, language, author, subject, category, message) VALUES (%i, '%s', %i, '%s', '%s', %i, '%s')", charid, Strings::EscapePair(inData->date, sizeof(inData->date)).c_str(), inData->language, charName, Strings::EscapePair(inData->subject, strlen(inData->subject)).c_str(), inData->category, Strings::EscapePair(inData->message, strlen(inData->message)).c_str());
	auto results = QueryDatabase(query);
	if(!results.Success())
	{
		return false; // no messages
	}
	return true;
}

bool ZoneDatabase::EraseMBMessage(uint32 id, uint32 charid)
{
	std::string query = StringFormat("DELETE FROM mb_messages WHERE id=%i AND charid=%i", id, charid);
	auto results = QueryDatabase(query);
	if(!results.Success())
	{
		return false; // no messages
	}
	return true;
}

bool ZoneDatabase::ViewMBMessage(uint32 id, char* outData) {
	std::string query = StringFormat("SELECT message from mb_messages WHERE id=%i", id);
	auto results = QueryDatabase(query);
	if(!results.Success())
	{
		return false; // no messages
	}

	if(results.RowCount() == 0 || results.RowCount() > 1)
	{
		return false;
	}
	auto& row = results.begin();
	strncpy(outData, row[0], 2048);
	return true;
}


bool ZoneDatabase::SaveSoulboundItems(Client* client, std::list<EQ::ItemInstance*>::const_iterator &start, std::list<EQ::ItemInstance*>::const_iterator &end)
{
		// Delete cursor items
	std::string query = StringFormat("DELETE FROM character_inventory WHERE id = %i "
                                    "AND ((slotid >= %i AND slotid <= %i) "
                                    "OR slotid = %i OR (slotid >= %i AND slotid <= %i) )",
                                    client->CharacterID(), EQ::invslot::CURSOR_QUEUE_BEGIN, EQ::invslot::CURSOR_QUEUE_END, 
		EQ::invslot::slotCursor, EQ::invbag::CURSOR_BAG_BEGIN, EQ::invbag::CURSOR_BAG_END);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        std::cout << "Clearing cursor failed: " << results.ErrorMessage() << std::endl;
        return false;
    }

	int8 count = 0;
	int i = EQ::invslot::CURSOR_QUEUE_BEGIN;
    for(auto& it = start; it != end; ++it) {
        EQ::ItemInstance *inst = *it;
		if(inst && inst->GetItem()->Soulbound)
		{
			int16 newslot = client->GetInv().FindFreeSlot(false, false, inst->GetItem()->Size);
			if(newslot != INVALID_INDEX)
			{
				client->PutItemInInventory(newslot, *inst);
				++count;
			}
		}
		else if(inst)
		{
			int16 use_slot = (i == EQ::invslot::CURSOR_QUEUE_BEGIN) ? EQ::invslot::slotCursor : i;
			if (SaveInventory(client->AccountID(), client->CharacterID(), inst, use_slot)) 
			{
				++i;
			}
		}
    }

	if(count > 0)
		return true;

	return false;
}

bool ZoneDatabase::UpdateSkillDifficulty(uint16 skillid, float difficulty)
{
		std::string query = StringFormat("UPDATE skill_difficulty SET difficulty = %0.2f "
                                    "WHERE skillid = %u;", difficulty, skillid);
    auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	return results.RowsAffected() > 0;
}

bool ZoneDatabase::SaveCursor(Client* client, std::list<EQ::ItemInstance*>::const_iterator &start, std::list<EQ::ItemInstance*>::const_iterator &end)
{
	uint32 account_id = client->AccountID();
	uint32 char_id = client->CharacterID();

	// Delete cursor items
	std::string query = StringFormat("DELETE FROM character_inventory WHERE id = %i "
		"AND ((slotid >= %i AND slotid <= %i) "
		"OR slotid = %i OR (slotid >= %i AND slotid <= %i) )",
		char_id, EQ::invslot::CURSOR_QUEUE_BEGIN, EQ::invslot::CURSOR_QUEUE_END,
		EQ::invslot::slotCursor, EQ::invbag::CURSOR_BAG_BEGIN, EQ::invbag::CURSOR_BAG_END);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		std::cout << "Clearing cursor failed: " << results.ErrorMessage() << std::endl;
		return false;
	}

	int i = EQ::invslot::CURSOR_QUEUE_BEGIN;
	for (auto& it = start; it != end; ++it, i++) {
		EQ::ItemInstance *inst = *it;
		int16 use_slot = (i == EQ::invslot::CURSOR_QUEUE_BEGIN) ? EQ::invslot::slotCursor : i;
		if (inst)
		{
			Log(Logs::Detail, Logs::Inventory, "SaveCursor: Attempting to save item %s for char %d in slot %d", inst->GetItem()->Name, char_id, use_slot);
		}
		else
			Log(Logs::Detail, Logs::Inventory, "SaveCursor: No inst found. This is either an error, or we've reached the end of the list.");

		if (!SaveInventory(account_id, char_id, inst, use_slot)) 
		{
			return false;
		}
		else if(inst && !inst->IsOnCursorQueue() && use_slot == EQ::invslot::slotCursor)
		{
			client->SendItemPacket(EQ::invslot::slotCursor, inst, ItemPacketSummonItem);
			inst->SetCursorQueue(true);
			Log(Logs::Detail, Logs::Inventory, "SaveCursor: Sending out ItemPacket for non-queued cursor item %s", inst->GetItem()->Name);
		}
	}
	return true;
}


bool ZoneDatabase::ResetStartingFaction(Client* c, uint32 si_race, uint32 si_class, uint32 si_deity, uint32 si_current_zone)
{
	std::string starting_zone_query = StringFormat("SELECT faction_id, faction_amount FROM start_faction_hits "
		"WHERE (player_race = %i) AND (player_class = %i) AND "
		"(player_deity = %i) AND (player_choice = %i)",
		si_race, si_class, si_deity, si_current_zone);
	auto starting_zone_query_results = QueryDatabase(starting_zone_query);
	if (!starting_zone_query_results.Success())
		return false;

	if (starting_zone_query_results.RowCount() == 0)
		return false;


	for (auto row = starting_zone_query_results.begin(); row != starting_zone_query_results.end(); ++row) {
		auto starting_faction_id = atoi(row[0]);
		auto starting_faction_amount = atoi(row[1]);
		SetCharacterFactionLevel(c->CharacterID(), starting_faction_id, starting_faction_amount, 0, c->GetFactionList());
	}


	return true;
}

bool ZoneDatabase::ResetStartingItems(Client* c, uint32 si_race, uint32 si_class, uint32 si_deity, uint32 si_current_zone, char* si_name, int admin_level, int& return_zone_id)
{
	std::string starting_zone_query = StringFormat("SELECT zone_id FROM start_zones "
		"WHERE (player_race = %i) AND (player_class = %i) AND "
		"(player_deity = %i) AND (bind_id = %i)",
		si_race, si_class, si_deity, si_current_zone);
	auto starting_zone_query_results = QueryDatabase(starting_zone_query);
	if (!starting_zone_query_results.Success())
		return false;

	if (starting_zone_query_results.RowCount() == 0)
		return false;

	auto starting_zone_id = atoi(starting_zone_query_results.begin()[0]);
	
	if (starting_zone_id == 0)
		return false;

	return_zone_id = starting_zone_id;

	std::string query = StringFormat(
		"SELECT itemid, item_charges, slot FROM starting_items "
		"WHERE (race = %i or race = 0) AND (class = %i or class = 0) AND "
		"(deityid = %i or deityid = 0) AND (zoneid = %i or zoneid = 0) AND "
		"gm <= %i %s ORDER BY id",
		si_race,
		si_class,
		si_deity,
		starting_zone_id,
		admin_level,
		ContentFilterCriteria::apply().c_str()
	);

	auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	const EQ::ItemData* myitem;

	for (auto row = results.begin(); row != results.end(); ++row) {
		int32 itemid = atoi(row[0]);
		int32 charges = atoi(row[1]);
		int32 slot = atoi(row[2]);
		myitem = GetItem(itemid);

		if (!myitem)
			continue;

		if (slot < 0)
			slot = c->GetInv().FindFreeSlot(0, 0);

		c->SummonItem(itemid, charges, slot);
	}
	return true;
}

int16 ZoneDatabase::GetTimerFromSkill(EQ::skills::SkillType skillid)
{
	uint32 timer = INVALID_INDEX;
	switch (skillid)
	{
	case EQ::skills::SkillFeignDeath:
		timer = pTimerFeignDeath;
		break;
	case EQ::skills::SkillSneak:
		timer = pTimerSneak;
		break;
	case EQ::skills::SkillHide:
		timer = pTimerHide;
		break;
	case EQ::skills::SkillTaunt:
		timer = pTimerTaunt;
		break;
	case EQ::skills::SkillIntimidation:
		timer = pTimerInstillDoubt;
		break;
	case EQ::skills::SkillFishing:
		timer = pTimerFishing;
		break;
	case EQ::skills::SkillForage:
		timer = pTimerForaging;
		break;
	case EQ::skills::SkillMend:
		timer = pTimerMend;
		break;
	case EQ::skills::SkillTracking:
		timer = pTimerTracking;
		break;
	case EQ::skills::SkillSenseTraps:
		timer = pTimerSenseTraps;
		break;
	case EQ::skills::SkillDisarmTraps:
		timer = pTimerDisarmTraps;
		break;
	case EQ::skills::SkillBegging:
	case EQ::skills::SkillPickPockets:
		timer = pTimerBeggingPickPocket;
		break;
	case EQ::skills::SkillThrowing:
	case EQ::skills::SkillArchery:
	case EQ::skills::SkillBash:
	case EQ::skills::SkillKick:
	case EQ::skills::SkillFlyingKick:
	case EQ::skills::SkillDragonPunch:
	case EQ::skills::SkillEagleStrike:
	case EQ::skills::SkillTigerClaw:
	case EQ::skills::SkillRoundKick:
	case EQ::skills::SkillBackstab:
		timer = pTimerCombatAbility;
		break;
	case EQ::skills::SkillSenseHeading:
		timer = pTimerSenseHeading;
		break;
	case EQ::skills::SkillBindWound:
		timer = pTimerBindWound;
		break;
	case EQ::skills::SkillApplyPoison:
		timer = pTimerApplyPoison;
		break;
	case EQ::skills::SkillDisarm:
		timer = pTimerDisarm;
		break;
	}

	return timer;
}

bool ZoneDatabase::GetZoneBanishPoint(ZoneBanishPoint& into_zbp, const char* dest_zone) {
	std::string query = StringFormat("SELECT banish_zone_id, banish_x, banish_y, banish_z, banish_heading "
		"FROM zone WHERE short_name = '%s' "
		"LIMIT 1",
		dest_zone);

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() != 1) {
		return false;
	}

	auto row = results.begin();
	into_zbp.target_zone_id = atoi(row[0]);
	into_zbp.x = atof(row[1]);
	into_zbp.y = atof(row[2]);
	into_zbp.z = atof(row[3]);
	into_zbp.heading = atof(row[4]);

	return true;
}