#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#if defined(_MSC_VER) && _MSC_VER >= 1800
	#include <algorithm>
#endif

#include "shareddb.h"
#include "mysql.h"
#include "inventory_profile.h"
#include "classes.h"
#include "rulesys.h"
#include "seperator.h"
#include "strings.h"
#include "eq_packet_structs.h"
#include "guilds.h"
#include "extprofile.h"
#include "memory_mapped_file.h"
#include "ipc_mutex.h"
#include "eqemu_exception.h"
#include "faction.h"
#include "features.h"
#include "eqemu_config.h"
#include "data_verification.h"
#include "repositories/criteria/content_filter_criteria.h"
#include "path_manager.h"

namespace ItemField
{
	enum
	{
		source = 0,
#define F(x) x,
#include "item_fieldlist.h"
#undef F
		updated
	};
};

SharedDatabase::SharedDatabase()
: Database()
{
}

SharedDatabase::SharedDatabase(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
: Database(host, user, passwd, database, port)
{
}

SharedDatabase::~SharedDatabase() {
}

bool SharedDatabase::SetHideMe(uint32 account_id, uint8 hideme)
{
	std::string query = StringFormat("UPDATE account SET hideme = %i WHERE id = %i", hideme, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

uint8 SharedDatabase::GetGMSpeed(uint32 account_id)
{
	std::string query = StringFormat("SELECT gmspeed FROM account WHERE id = '%i'", account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

    if (results.RowCount() != 1)
        return 0;

    auto row = results.begin();

	return atoi(row[0]);
}

bool SharedDatabase::SetGMSpeed(uint32 account_id, uint8 gmspeed)
{
	std::string query = StringFormat("UPDATE account SET gmspeed = %i WHERE id = %i", gmspeed, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::SetGMInvul(uint32 account_id, bool gminvul)
{
	std::string query = StringFormat("UPDATE account SET gminvul = %i WHERE id = %i", gminvul, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::SetGMFlymode(uint32 account_id, uint8 flymode)
{
	std::string query = StringFormat("UPDATE account SET flymode = %i WHERE id = %i", flymode, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::SetGMIgnoreTells(uint32 account_id, uint8 ignoretells)
{
	std::string query = StringFormat("UPDATE account SET ignore_tells = %i WHERE id = %i", ignoretells, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::GetCharCreateFullInfo(uint32 class_id, uint32 race_id, uint32 diety_id, uint32 player_choice_city, uint32& expansions_req, RaceClassAllocation& out, BindStruct& start_zone_bind)
{
	std::string query = StringFormat(
		"SELECT c.expansions_req"
		", a.base_str"
		", a.base_dex"
		", a.base_agi"
		", a.base_sta"
		", a.base_int"
		", a.base_wis"
		", a.base_cha"
		", a.alloc_str"
		", a.alloc_dex"
		", a.alloc_agi"
		", a.alloc_sta"
		", a.alloc_int"
		", a.alloc_wis"
		", a.alloc_cha"
		", sz.zone_id"
		", sz.bind_x"
		", sz.bind_y"
		", sz.bind_z"
		", sz.heading"
		" FROM char_create_combinations c"
		" JOIN char_create_point_allocations a ON c.allocation_id = a.id"
		" JOIN start_zones sz ON c.start_zone = sz.start_zone AND c.race = sz.player_race AND c.class = sz.player_class AND c.deity = sz.player_deity"
		" WHERE c.race = %u AND c.class = %u AND c.deity = %u AND sz.player_choice = %u"
		, race_id, class_id, diety_id, player_choice_city);

	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		expansions_req = atoi(row[0]); // expansions_req
		out.BaseStats[0] = atoi(row[1]); // str
		out.BaseStats[1] = atoi(row[2]); // dex
		out.BaseStats[2] = atoi(row[3]); // agi
		out.BaseStats[3] = atoi(row[4]); // sta
		out.BaseStats[4] = atoi(row[5]); // int
		out.BaseStats[5] = atoi(row[6]); // wis
		out.BaseStats[6] = atoi(row[7]); // cha
		out.DefaultPointAllocation[0] = atoi(row[8]); // str
		out.DefaultPointAllocation[1] = atoi(row[9]); // dex
		out.DefaultPointAllocation[2] = atoi(row[10]); // agi
		out.DefaultPointAllocation[3] = atoi(row[11]); // sta
		out.DefaultPointAllocation[4] = atoi(row[12]); // int
		out.DefaultPointAllocation[5] = atoi(row[13]); // wis
		out.DefaultPointAllocation[6] = atoi(row[14]); // cha
		start_zone_bind.zoneId = atoi(row[15]); // zone_id
		start_zone_bind.x = atof(row[16]); // bind_x
		start_zone_bind.y = atof(row[17]); // bind_y
		start_zone_bind.z = atof(row[18]); // bind_z
		start_zone_bind.heading = atof(row[19]); // heading
		return true;
	}

	return false;
}

bool SharedDatabase::GetCharacterCombinationUnlock(uint32 character_id, uint32 class_id, uint32 race_id, uint32 deity_id, int player_choice_city, BindStruct& out_bind) {

	std::string query = StringFormat(
		"SELECT home_zone, home_x, home_y, home_z, home_heading"
		" FROM `character_combination_unlocks`"
		" WHERE `charid` = %u AND `class` = %u AND `race` = %u AND `deity` = %u AND `home_choice` = %i",
		character_id, class_id, race_id, deity_id, player_choice_city
	);

	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		out_bind.zoneId = atoi(row[0]);
		out_bind.x = atof(row[1]);
		out_bind.y = atof(row[2]);
		out_bind.z = atof(row[3]);
		out_bind.heading = atof(row[4]);
		return true;
	}

	return false;
}

void SharedDatabase::SaveCharacterCombinationUnlock(uint32 character_id, uint32 in_class, uint32 in_race, uint32 in_deity, int player_choice_city, const BindStruct& start_zone) {

	std::string query = StringFormat(
		"REPLACE INTO `character_combination_unlocks`"
		" (charid, class, race, deity, home_choice, home_zone, home_x, home_y, home_z, home_heading)"
		" VALUES(%u, %u, %u, %u, %i, %u, %f, %f, %f, %f)",
		character_id, in_class, in_race, in_deity, player_choice_city, start_zone.zoneId, start_zone.x, start_zone.y, start_zone.z, start_zone.heading
	);

	auto results = QueryDatabase(query);
}

bool SharedDatabase::GetCharCreateStats(uint32 class_id, uint32 race_id, RaceClassAllocation& out)
{
	std::string query = StringFormat(
		"SELECT a.id"
		", a.base_str"
		", a.base_dex"
		", a.base_agi"
		", a.base_sta"
		", a.base_int"
		", a.base_wis"
		", a.base_cha"
		", a.alloc_str"
		", a.alloc_dex"
		", a.alloc_agi"
		", a.alloc_sta"
		", a.alloc_int"
		", a.alloc_wis"
		", a.alloc_cha"
		" FROM char_create_combinations c"
		" JOIN char_create_point_allocations a ON c.allocation_id = a.id"
		" WHERE c.race = %u AND c.class = %u"
		" LIMIT 1"
		, race_id, class_id);

	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		out.BaseStats[0] = atoi(row[1]); // str
		out.BaseStats[1] = atoi(row[2]); // dex
		out.BaseStats[2] = atoi(row[3]); // agi
		out.BaseStats[3] = atoi(row[4]); // sta
		out.BaseStats[4] = atoi(row[5]); // int
		out.BaseStats[5] = atoi(row[6]); // wis
		out.BaseStats[6] = atoi(row[7]); // cha
		out.DefaultPointAllocation[0] = atoi(row[8]); // str
		out.DefaultPointAllocation[1] = atoi(row[9]); // dex
		out.DefaultPointAllocation[2] = atoi(row[10]); // agi
		out.DefaultPointAllocation[3] = atoi(row[11]); // sta
		out.DefaultPointAllocation[4] = atoi(row[12]); // int
		out.DefaultPointAllocation[5] = atoi(row[13]); // wis
		out.DefaultPointAllocation[6] = atoi(row[14]); // cha
		return true;
	}

	return false;
}

uint32 SharedDatabase::GetTotalTimeEntitledOnAccount(uint32 AccountID)
{
	uint32 EntitledTime = 0;
	std::string query = StringFormat("SELECT `time_played` FROM `character_data` WHERE `account_id` = %u", AccountID);
	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		EntitledTime += atoi(row[0]);
	}
	return EntitledTime;
}

void SharedDatabase::SetMailKey(int CharID, int IPAddress, int MailKey)
{
	char MailKeyString[17];

	if (RuleB(Chat, EnableMailKeyIPVerification) == true)
		sprintf(MailKeyString, "%08X%08X", IPAddress, MailKey);
	else
		sprintf(MailKeyString, "%08X", MailKey);

	std::string query = StringFormat("UPDATE character_data SET mailkey = '%s' WHERE id = '%i'",
		MailKeyString, CharID);
	auto results = QueryDatabase(query);
	if (!results.Success())
		Log(Logs::General, Logs::Error, "SharedDatabase::SetMailKey(%i, %s) : %s", CharID, MailKeyString, results.ErrorMessage().c_str());

}

std::string SharedDatabase::GetMailKey(int CharID, bool key_only)
{
	std::string query = StringFormat("SELECT `mailkey` FROM `character_data` WHERE `id`='%i' LIMIT 1", CharID);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		LogClientLogin("Error retrieving mailkey from database: {}", results.ErrorMessage().c_str());
		return std::string();
	}

	auto row = results.begin();
	std::string mail_key = row[0];

	if (mail_key.length() > 8 && key_only)
		return mail_key.substr(8);
	else
		return mail_key;
}

bool SharedDatabase::SaveInventory(uint32 account_id, uint32 char_id, const EQ::ItemInstance* inst, int16 slot_id)
{
	if (!inst) // All other inventory
        return DeleteInventorySlot(account_id, char_id, slot_id);

    return UpdateInventorySlot(account_id, char_id, inst, slot_id);
}

bool SharedDatabase::UpdateInventorySlot(uint32 account_id, uint32 char_id, const EQ::ItemInstance* inst, int16 slot_id)
{

    uint16 charges = 0;
	if(inst->GetCharges() >= 0)
		charges = inst->GetCharges();
	else
		charges = 0x7FFF;

	std::string table = "character_inventory";
	uint32 id = char_id;
	if (slot_id >= EQ::invslot::SHARED_BANK_BEGIN && slot_id <= EQ::invbag::SHARED_BANK_BAGS_END)
	{
		table = "account_inventory";
		id = account_id;
	}

	// Update/Insert item
	std::string query = StringFormat("REPLACE INTO %s "
		"(id, slotid, itemid, charges, custom_data)"
		" VALUES(%lu,%lu,%lu,%lu,'%s')",
		table.c_str(),
		(unsigned long)id, (unsigned long)slot_id, (unsigned long)inst->GetItem()->ID,
		(unsigned long)charges,
		inst->GetCustomDataString().c_str());
	auto results = QueryDatabase(query);

    // Save bag contents, if slot supports bag contents
	if (inst && inst->IsClassBag() && EQ::InventoryProfile::SupportsContainers(slot_id))
		for (uint8 idx = EQ::invbag::SLOT_BEGIN; idx <= EQ::invbag::SLOT_END; idx++) {
			const EQ::ItemInstance* baginst = inst->GetItem(idx);
			SaveInventory(account_id, char_id, baginst, EQ::InventoryProfile::CalcSlotId(slot_id, idx));
		}

    if (!results.Success()) {
        return false;
    }

	return true;
}

bool SharedDatabase::DeleteInventorySlot(uint32 account_id, uint32 char_id, int16 slot_id)
{
	std::string table = "character_inventory";
	uint32 id = char_id;
	if (slot_id >= EQ::invslot::SHARED_BANK_BEGIN && slot_id <= EQ::invbag::SHARED_BANK_BAGS_END)
	{
		table = "account_inventory";
		id = account_id;
	}

	// Delete item
	std::string query = StringFormat("DELETE FROM %s WHERE id = %i AND slotid = %i", table.c_str(), id, slot_id);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

    // Delete bag slots, if need be
    if (!EQ::InventoryProfile::SupportsContainers(slot_id))
        return true;

    int16 base_slot_id = EQ::InventoryProfile::CalcSlotId(slot_id, EQ::invbag::SLOT_BEGIN);
    query = StringFormat("DELETE FROM %s WHERE id = %i AND slotid >= %i AND slotid < %i",
                        table.c_str(), id, base_slot_id, (base_slot_id+10));
    results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

    // @merth: need to delete augments here
    return true;
}

bool SharedDatabase::SetStartingItems(PlayerProfile_Struct* pp, EQ::InventoryProfile* inv, uint32 si_race, uint32 si_class, uint32 si_deity, uint32 si_current_zone, char* si_name, int admin_level)
{
	const EQ::ItemData* myitem;

    std::string query = StringFormat(
		"SELECT itemid, item_charges, slot FROM starting_items "
        "WHERE (race = %i or race = 0) AND (class = %i or class = 0) AND "
        "(deityid = %i or deityid = 0) AND (zoneid = %i or zoneid = 0) AND "
        "gm <= %i %s ORDER BY id",
        si_race, 
		si_class,
		si_deity,
		si_current_zone,
		admin_level,
		ContentFilterCriteria::apply().c_str()
	);

    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}
	
	for (auto row = results.begin(); row != results.end(); ++row) {
		int32 itemid = atoi(row[0]);
		int32 charges = atoi(row[1]);
		int32 slot = atoi(row[2]);
		myitem = GetItem(itemid);

		if (!myitem) {
			continue;
		}

		EQ::ItemInstance* myinst = CreateBaseItem(myitem, charges);

		if (slot < 0) {
			slot = inv->FindFreeSlot(0, 0);
		}

		inv->PutItem(slot, *myinst);
		safe_delete(myinst);
	}

	return true;
}

// Overloaded: Retrieve character inventory based on character id
bool SharedDatabase::GetInventory(uint32 account_id, uint32 char_id, EQ::InventoryProfile* inv)
{
	for(int i = 0; i < 2; i++)
	{
		std::string table;
		unsigned long id;
		if (i == 0)
		{
			table = "character_inventory";
			id = char_id;
		}
		else if (i == 1)
		{
			table = "account_inventory";
			id = account_id;
		}
		else
		{
			break;
		}

	// Retrieve character inventory
	std::string query = StringFormat("SELECT slotid, itemid, charges, custom_data "
                                    "FROM %s WHERE id = %i ORDER BY slotid", table.c_str(), id);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        LogError("Error loading character items.");
        return false;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        int16 slot_id	= atoi(row[0]);
        uint32 item_id	= atoi(row[1]);
        uint16 charges	= atoi(row[2]);

        const EQ::ItemData* item = GetItem(item_id);

        if (!item) {
            Log(Logs::General, Logs::Error,"Warning: charid %i has an invalid item_id %i in inventory slot %i", char_id, item_id, slot_id);
            continue;
        }

        int16 put_slot_id = INVALID_INDEX;

		EQ::ItemInstance* inst = CreateBaseItem(item, charges);

		if (inst == nullptr)
			continue;

        if(row[3]) {
            std::string data_str(row[3]);
            std::string idAsString;
            std::string value;
            bool use_id = true;

            for(int i = 0; i < data_str.length(); ++i) {
                if(data_str[i] == '^') {
                    if(!use_id) {
                        inst->SetCustomData(idAsString, value);
                        idAsString.clear();
                        value.clear();
                    }

                    use_id = !use_id;
                    continue;
                }

                char v = data_str[i];
                if(use_id)
                    idAsString.push_back(v);
                else
                    value.push_back(v);
            }
        }

        if(charges==0x7FFF)
            inst->SetCharges(-1);
        else
            inst->SetCharges(charges);

        if (slot_id >= EQ::invslot::CURSOR_QUEUE_BEGIN && slot_id <= EQ::invslot::CURSOR_QUEUE_END)
            put_slot_id = inv->PushCursor(*inst);
        else if (slot_id >= 3110 && slot_id <= 3179) {
            // Admins: please report any occurrences of this error
            LogError("Warning: Defunct location for item in inventory: charid={}, item_id={}, slot_id={} .. pushing to cursor...", char_id, item_id, slot_id);
            put_slot_id = inv->PushCursor(*inst);
        } else
            put_slot_id = inv->PutItem(slot_id, *inst);

        safe_delete(inst);

        // Save ptr to item in inventory
        if (put_slot_id == INVALID_INDEX) {
            LogError("Warning: Invalid slot_id for item in inventory: charid={}, item_id={}, slot_id={}",char_id, item_id, slot_id);
        }
    }
	}
	return true;
}

// Overloaded: Retrieve character inventory based on account_id and character name
bool SharedDatabase::GetInventory(uint32 account_id, char* name, EQ::InventoryProfile* inv)
{
	for (int i = 0; i < 2; i++)
	{
		std::string query;
		if (i == 0)
		{
			// Retrieve character inventory
			query = StringFormat("SELECT ci.slotid, ci.itemid, ci.charges, ci.custom_data "
				"FROM character_inventory ci INNER JOIN character_data ch "
				"ON ch.id = ci.id WHERE ch.name = '%s' AND ch.account_id = %i ORDER BY ci.slotid",
				name, account_id);
		}
		else if (i == 1)
		{
			// Retrieve shared bank
			query = StringFormat("SELECT slotid, itemid, charges, custom_data "
				"FROM account_inventory "
				"WHERE id = %i ORDER BY slotid",
				account_id);
		}
		else
		{
			break;
		}
	
    auto results = QueryDatabase(query);
    if (!results.Success()){
		Log(Logs::General, Logs::Error, "Error loading character items.");
        return false;
	}


    for (auto row = results.begin(); row != results.end(); ++row) {
        int16 slot_id	= atoi(row[0]);
        uint32 item_id	= atoi(row[1]);
        int8 charges	= atoi(row[2]);

        const EQ::ItemData* item = GetItem(item_id);
        int16 put_slot_id = INVALID_INDEX;
        if(!item)
            continue;

		EQ::ItemInstance* inst = CreateBaseItem(item, charges);

		if (inst == nullptr)
			continue;

        if(row[3]) {
            std::string data_str(row[3]);
            std::string idAsString;
            std::string value;
            bool use_id = true;

            for(int i = 0; i < data_str.length(); ++i) {
                if(data_str[i] == '^') {
                    if(!use_id) {
                        inst->SetCustomData(idAsString, value);
                        idAsString.clear();
                        value.clear();
                    }

                    use_id = !use_id;
                    continue;
                }

                char v = data_str[i];
                if(use_id)
                    idAsString.push_back(v);
                else
                    value.push_back(v);

            }
        }

        inst->SetCharges(charges);

        if (slot_id>=EQ::invslot::CURSOR_QUEUE_BEGIN && slot_id <= EQ::invslot::CURSOR_QUEUE_END)
            put_slot_id = inv->PushCursor(*inst);
        else
            put_slot_id = inv->PutItem(slot_id, *inst);

        safe_delete(inst);

        // Save ptr to item in inventory
        if (put_slot_id == INVALID_INDEX)
            Log(Logs::General, Logs::Error, "Warning: Invalid slot_id for item in inventory: name=%s, acctid=%i, item_id=%i, slot_id=%i", name, account_id, item_id, slot_id);

    }
	}
	return true;
}


void SharedDatabase::GetItemsCount(int32 &item_count, uint32 &max_id) 
{
	item_count = -1;
	max_id = 0;

	const std::string query = "SELECT MAX(id), count(*) FROM items";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return;
	}

	if (results.RowCount() == 0)
        return;

    auto row = results.begin();

    if(row[0])
        max_id = atoi(row[0]);

    if (row[1])
		item_count = atoi(row[1]);
}

bool SharedDatabase::LoadItems(const std::string &prefix)
{
	items_mmf.reset(nullptr);

	try {
		auto Config = EQEmuConfig::get();
		EQ::IPCMutex mutex("items");
		mutex.Lock();
		std::string file_name = fmt::format("{}/{}{}", path.GetSharedMemoryPath(), prefix, std::string("items"));
		items_mmf = std::unique_ptr<EQ::MemoryMappedFile>(new EQ::MemoryMappedFile(file_name));
		items_hash = std::unique_ptr<EQ::FixedMemoryHashSet<EQ::ItemData>>(new EQ::FixedMemoryHashSet<EQ::ItemData>(reinterpret_cast<uint8*>(items_mmf->Get()), items_mmf->Size()));
		mutex.Unlock();

	} catch(std::exception& ex) {
		LogError("Error Loading Items: {0}", ex.what());
		return false;
	}

	return true;
}

void SharedDatabase::LoadItems(void* data, uint32 size, int32 items, uint32 max_item_id)
{
	EQ::FixedMemoryHashSet<EQ::ItemData> hash(reinterpret_cast<uint8*>(data), size, items, max_item_id);

	std::string variable_buffer;

	bool disable_no_drop = RuleB(Items, DisableNoDrop);
	bool disable_no_rent = RuleB(Items, DisableNoRent);

	if (GetVariable("disablenodrop", variable_buffer)) {
		if (variable_buffer == "1") {
			disable_no_drop = true;
		}
	}

	if (GetVariable("disablenorent", variable_buffer)) {
		if (variable_buffer == "1") {
			disable_no_rent = true;
		}
	}

	EQ::ItemData item;

	const std::string query = "SELECT source,"
#define F(x) "`"#x"`,"
#include "item_fieldlist.h"
#undef F
		"updated FROM items ORDER BY id";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row) {
		memset(&item, 0, sizeof(EQ::ItemData));

		float enabled_era = RuleR(World, CurrentExpansion);

		// Unique Identifier
		item.ID = std::stoul(row[ItemField::id]);

		// Name and Lore
		strn0cpy(item.Name, row[ItemField::name], sizeof(item.Name));
		strn0cpy(item.Lore, row[ItemField::lore], sizeof(item.Lore));

		// Flags
		item.GMFlag = std::stoi(row[ItemField::gmflag]);
		item.FVNoDrop = std::stoi(row[ItemField::fvnodrop]) ? true : false;
		item.Magic = std::stoi(row[ItemField::magic]) ? true : false;
		item.NoRent = disable_no_rent ? static_cast<uint8>(255) : static_cast<uint8>(std::stoul(row[ItemField::norent]));
		item.NoDrop = disable_no_drop ? static_cast<uint8>(255) : static_cast<uint8>(std::stoul(row[ItemField::nodrop]));
		item.QuestItemFlag = std::stoi(row[ItemField::questitemflag]) ? true : false;
		item.Tradeskills = std::stoi(row[ItemField::tradeskills]) ? true : false;
		item.Stackable_ = static_cast<int8>(std::stoi(row[ItemField::stackable]));
		item.Stackable = (std::stoi(row[ItemField::stackable]) == 1) ? true : false;

		// Type
		item.ItemType = static_cast<uint8>(std::stoul(row[ItemField::itemtype]));

		// Miscellaneous
		item.Light = static_cast<int8>(std::stoi(row[ItemField::light]));
		item.MaxCharges = static_cast<int8>(std::stoi(row[ItemField::maxcharges]));
		item.Size = static_cast<uint8>(std::stoul(row[ItemField::size]));
		item.Soulbound = static_cast<int8>(std::stoi(row[ItemField::soulbound]));
		//item.StackSize = static_cast<int16>(std::stoul(row[ItemField::stacksize])); // note - this is overwritten below
		item.Weight = std::stoul(row[ItemField::weight]);

		// Merchant
		item.Price = std::stoi(row[ItemField::price]);
		item.SellRate = std::stof(row[ItemField::sellrate]);

		// Display
		item.Color = std::stoul(row[ItemField::color]);
		item.Icon = static_cast<uint16>(std::stoul(row[ItemField::icon]));
		strn0cpy(item.IDFile, row[ItemField::idfile], sizeof(item.IDFile));
		item.Material = static_cast<uint8>(std::stoul(row[ItemField::material]));

		// Resists
		item.CR = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::cr]), -128, 127));
		item.DR = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::dr]), -128, 127));
		item.FR = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::fr]), -128, 127));
		item.MR = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::mr]), -128, 127));
		item.PR = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::pr]), -128, 127));

		// Stats
		item.AAgi = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::aagi]), -128, 127));
		item.ACha = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::acha]), -128, 127));
		item.ADex = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::adex]), -128, 127));
		item.AInt = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::aint]), -128, 127));
		item.ASta = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::asta]), -128, 127));
		item.AStr = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::astr]), -128, 127));
		item.AWis = static_cast<int8>(EQ::Clamp(std::stoi(row[ItemField::awis]), -128, 127));

		// Health and Mana
		item.HP = std::stoi(row[ItemField::hp]);
		item.Mana = std::stoi(row[ItemField::mana]);
		item.AC = std::stoi(row[ItemField::ac]);
		if (RuleB(Expansion, UseItemExpansionSetting) && !content_service.IsTheShadowsOfLuclinEnabled()) {
			//Bane Damage
			item.BaneDmgAmt = 0;
			item.BaneDmgBody = 0;
			item.BaneDmgRace = 0;

			// Elemental Damage
			item.ElemDmgType = 0;
			item.ElemDmgAmt = 0;
		}
		else {
			//Bane Damage
			item.BaneDmgAmt = static_cast<uint8>(std::stoul(row[ItemField::banedmgamt]));
			item.BaneDmgBody = std::stoi(row[ItemField::banedmgbody]);
			item.BaneDmgRace = std::stoi(row[ItemField::banedmgrace]);

			// Elemental Damage
			item.ElemDmgType = static_cast<uint8>(std::stoul(row[ItemField::elemdmgtype]));
			item.ElemDmgAmt = static_cast<uint8>(std::stoul(row[ItemField::elemdmgamt]));
		}

		// Combat
		item.Damage = static_cast<uint8>(std::stoul(row[ItemField::damage]));
		item.Delay = static_cast<uint8>(std::stoul(row[ItemField::delay]));
		item.Range = static_cast<uint8>(std::stoul(row[ItemField::range]));

		// Restrictions
		item.Classes = std::stoi(row[ItemField::classes]);
		item.Deity = std::stoul(row[ItemField::deity]);
		item.ItemClass = std::stoi(row[ItemField::itemclass]);
		item.Races = std::stoi(row[ItemField::races]);
		item.RecLevel = static_cast<uint8>(std::stoul(row[ItemField::reclevel]));
		item.RecSkill = static_cast<uint8>(std::stoul(row[ItemField::recskill]));
		item.ReqLevel = static_cast<uint8>(std::stoul(row[ItemField::reqlevel]));
		item.Slots = std::stoi(row[ItemField::slots]);

		// Skill Modifier
		if (RuleB(Expansion, UseItemExpansionSetting) && !content_service.IsTheShadowsOfLuclinEnabled()) {
			item.SkillModValue = 0;
			item.SkillModType = 0;
		}
		else {
			item.SkillModValue = std::stoi(row[ItemField::skillmodvalue]);
			item.SkillModType = std::stoul(row[ItemField::skillmodtype]);
		}

		// Bard
		item.BardType = std::stoul(row[ItemField::bardtype]);
		item.BardValue = std::stoi(row[ItemField::bardvalue]);

		// Faction
		item.FactionAmt1 = std::stoi(row[ItemField::factionamt1]);
		item.FactionMod1 = std::stoi(row[ItemField::factionmod1]);
		item.FactionAmt2 = std::stoi(row[ItemField::factionamt2]);
		item.FactionMod2 = std::stoi(row[ItemField::factionmod2]);
		item.FactionAmt3 = std::stoi(row[ItemField::factionamt3]);
		item.FactionMod3 = std::stoi(row[ItemField::factionmod3]);
		item.FactionAmt4 = std::stoi(row[ItemField::factionamt4]);
		item.FactionMod4 = std::stoi(row[ItemField::factionmod4]);

		// Bag
		item.BagSize = static_cast<uint8>(std::stoul(row[ItemField::bagsize]));
		item.BagSlots = static_cast<uint8>(EQ::Clamp(std::stoi(row[ItemField::bagslots]), 0, 10));
		item.BagType = static_cast<uint8>(std::stoul(row[ItemField::bagtype]));
		item.BagWR = static_cast<uint8>(EQ::Clamp(std::stoi(row[ItemField::bagwr]), 0, 100));

		// Bard Effect
		item.Bard.Effect = std::stoi(row[ItemField::bardeffect]);
		item.Bard.Type = std::stoi(row[ItemField::bardtype]);
		item.Bard.Level = static_cast<uint8>(std::stoul(row[ItemField::bardlevel]));
		item.Bard.Level2 = static_cast<uint8>(std::stoul(row[ItemField::bardlevel2]));

		// Book
		item.Book = static_cast<uint8>(std::stoul(row[ItemField::book]));
		item.BookType = std::stoul(row[ItemField::booktype]);
		strn0cpy(item.Filename, row[ItemField::filename], sizeof(item.Filename));

		// Click Effect
		item.CastTime = std::stoi(row[ItemField::casttime]);
		item.CastTime_ = std::stoi(row[ItemField::casttime_]);
		item.Click.Effect = std::stoi(row[ItemField::clickeffect]);
		item.Click.Type = std::stoi(row[ItemField::clicktype]);
		item.Click.Level = static_cast<uint8>(std::stoul(row[ItemField::clicklevel]));
		item.Click.Level2 = static_cast<uint8>(std::stoul(row[ItemField::clicklevel2]));
		item.RecastDelay = std::stoul(row[ItemField::recastdelay]);
		item.RecastType = std::stoul(row[ItemField::recasttype]);

		// Focus Effect
		if (RuleB(Expansion, UseItemExpansionSetting) && !content_service.IsTheShadowsOfLuclinEnabled()) {
			item.Focus.Effect = 0;
			item.Focus.Type = 0;
			item.Focus.Level = 0;
			item.Focus.Level2 = 0;
		}
		else {
			item.Focus.Effect = std::stoi(row[ItemField::focuseffect]);
			item.Focus.Type = std::stoi(row[ItemField::focustype]);
			item.Focus.Level = static_cast<uint8>(std::stoul(row[ItemField::focuslevel]));
			item.Focus.Level2 = static_cast<uint8>(std::stoul(row[ItemField::focuslevel2]));
		}

		// Proc Effect
		item.Proc.Effect = std::stoi(row[ItemField::proceffect]);
		item.Proc.Type = std::stoi(row[ItemField::proctype]);
		item.Proc.Level = static_cast<uint8>(std::stoul(row[ItemField::proclevel]));
		item.Proc.Level2 = static_cast<uint8>(std::stoul(row[ItemField::proclevel2]));
		item.ProcRate = std::stoi(row[ItemField::procrate]);

		// Scroll Effect
		item.Scroll.Effect = std::stoi(row[ItemField::scrolleffect]);
		item.Scroll.Type = std::stoi(row[ItemField::scrolltype]);
		item.Scroll.Level = static_cast<uint8>(std::stoul(row[ItemField::scrolllevel]));
		item.Scroll.Level2 = static_cast<uint8>(std::stoul(row[ItemField::scrolllevel2]));

		// Worn Effect
		item.Worn.Effect = std::stoi(row[ItemField::worneffect]);
		item.Worn.Type = std::stoi(row[ItemField::worntype]);
		item.Worn.Level = static_cast<uint8>(std::stoul(row[ItemField::wornlevel]));
		item.Worn.Level2 = static_cast<uint8>(std::stoul(row[ItemField::wornlevel2]));

		//Expansion appears in
		item.min_expansion = std::stof(row[ItemField::min_expansion]);
		item.max_expansion = std::stof(row[ItemField::max_expansion]);

		//Is Legacy Item?
		item.legacy_item = static_cast<uint8>(std::stoul(row[ItemField::legacy_item]));

		// solar - fixup StackSize data
		// TODO: this StackSize field in the database is not needed, and code referencing StackSize should be rewritten to determine stackability based on the following conditions.  stack size is always 20.
		if
			(
				item.ItemClass == EQ::item::ItemClass::ItemClassCommon &&
				item.MaxCharges > 0 &&
				(
					item.ItemType == EQ::item::ItemTypeFood ||
					item.ItemType == EQ::item::ItemTypeDrink ||
					item.ItemType == EQ::item::ItemTypeCombinable ||
					item.ItemType == EQ::item::ItemTypeBandage ||
					item.ItemType == EQ::item::ItemTypeSmallThrowing ||
					item.ItemType == EQ::item::ItemTypeArrow ||
					item.ItemType == EQ::item::ItemTypeUnknown4 ||
					item.ItemType == EQ::item::ItemTypeFishingBait ||
					item.ItemType == EQ::item::ItemTypeAlcohol
					)
				)
		{
			// item is stackable
			item.StackSize = 20;
		}
		else
		{
			// item is not stackable
			item.StackSize = 1;
		}

		// solar - fixup StackSize data
		// TODO: this StackSize field in the database is not needed, and code referencing StackSize should be rewritten to determine stackability based on the following conditions.  stack size is always 20.
		if
			(
				item.ItemClass == EQ::item::ItemClass::ItemClassCommon &&
				item.MaxCharges > 0 &&
				(
					item.ItemType == EQ::item::ItemTypeFood ||
					item.ItemType == EQ::item::ItemTypeDrink ||
					item.ItemType == EQ::item::ItemTypeCombinable ||
					item.ItemType == EQ::item::ItemTypeBandage ||
					item.ItemType == EQ::item::ItemTypeSmallThrowing ||
					item.ItemType == EQ::item::ItemTypeArrow ||
					item.ItemType == EQ::item::ItemTypeUnknown4 ||
					item.ItemType == EQ::item::ItemTypeFishingBait ||
					item.ItemType == EQ::item::ItemTypeAlcohol
					)
				)
		{
			// item is stackable
			item.StackSize = 20;
		}
		else
		{
			// item is not stackable
			item.StackSize = 1;
		}

		try {
			hash.insert(item.ID, item);
		}
		catch (std::exception& ex) {
			LogError("Database::LoadItems: {0}", ex.what());
			break;
		}
	}
}

const EQ::ItemData* SharedDatabase::GetItem(uint32 id)
{
	if(!items_hash || id > items_hash->max_key()) {
		return nullptr;
	}


	if(items_hash->exists(id)) {
		EQ::ItemData* returned_item = &(items_hash->at(id));

		if (returned_item != nullptr)
		{
			bool expansion_enabled = RuleR(World, CurrentExpansion) >= returned_item->min_expansion && RuleR(World, CurrentExpansion) < returned_item->max_expansion;
			bool expansion_all = returned_item->min_expansion == AllEQErasFloat && returned_item->max_expansion == AllEQErasFloat;
			if (expansion_enabled || expansion_all)
			{
				return returned_item;
			}
		}
	}

	return nullptr;
}

const EQ::ItemData* SharedDatabase::IterateItems(uint32* id) 
{
	if(!items_hash || !id) {
		return nullptr;
	}

	for(;;) {
		if(*id > items_hash->max_key()) {
			break;
		}

		if(items_hash->exists(*id)) {
			EQ::ItemData* returned_item = &(items_hash->at((*id)));

			if (returned_item != nullptr)
			{
				bool expansion_enabled = RuleR(World, CurrentExpansion) >= returned_item->min_expansion && RuleR(World, CurrentExpansion) < returned_item->max_expansion;
				bool expansion_all = returned_item->min_expansion == AllEQErasFloat && returned_item->max_expansion == AllEQErasFloat;
				if (expansion_enabled || expansion_all)
				{
					++(*id);
					return returned_item;
				}
				else
				{
					++(*id);
				}
			}
		} else {
			++(*id);
		}
	}

	return nullptr;
}

std::string SharedDatabase::GetBook(const char *txtfile)
{
	char txtfile2[20];
	std::string txtout;
	strcpy(txtfile2, txtfile);

	std::string query = StringFormat("SELECT txtfile FROM books WHERE name = '%s'", txtfile2);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		txtout.assign(" ",1);
		return txtout;
	}

    if (results.RowCount() == 0) {
		LogError("No book to send, ({0})", txtfile);
        txtout.assign(" ",1);
        return txtout;
    }

    auto row = results.begin();
    txtout.assign(row[0],strlen(row[0]));

    return txtout;
}

// Create appropriate ItemInst class
EQ::ItemInstance* SharedDatabase::CreateItem(uint32 item_id, int8 charges, const QuarmItemData& quarm_item_data)
{
	const EQ::ItemData* item = nullptr;
	EQ::ItemInstance* inst = nullptr;

	item = GetItem(item_id);
	if (item) {
		inst = CreateBaseItem(item, charges, quarm_item_data);

		if (inst == nullptr) {
			LogError("Error: valid item data returned a null reference for EQ::ItemInstance creation in SharedDatabase::CreateItem()");
			LogError("Item Data = ID: {0}, Name: {1}, Charges: {2}", item->ID, item->Name, charges);
			return nullptr;
		}
	}

	return inst;
}


// Create appropriate ItemInst class
EQ::ItemInstance* SharedDatabase::CreateItem(const EQ::ItemData* item, int8 charges, const QuarmItemData& quarm_item_data)
{
	EQ::ItemInstance* inst = nullptr;
	if (item) {
		inst = CreateBaseItem(item, charges, quarm_item_data);

		if (inst == nullptr) {
			LogError("Error: valid item data returned a null reference for EQ::ItemInstance creation in SharedDatabase::CreateItem()");
			LogError("Item Data = ID: {0}, Name: {1}, Charges: {2}", item->ID, item->Name, charges);
			return nullptr;
		}
	}

	return inst;
}

EQ::ItemInstance* SharedDatabase::CreateBaseItem(const EQ::ItemData* item, int8 charges, const QuarmItemData& quarm_item_data)
{
	EQ::ItemInstance* inst = nullptr;
	if (item) {
		// if maxcharges is -1 that means it is an unlimited use item.
		// set it to 1 charge so that it is usable on creation
		if (charges == 0 && item->MaxCharges == -1)
			charges = 1;

		inst = new EQ::ItemInstance(item, charges, quarm_item_data);

		if (inst == nullptr) {
			LogError("Error: valid item data returned a null reference for EQ::ItemInstance creation in SharedDatabase::CreateBaseItem()");
			LogError("Item Data = ID: {0}, Name: {1}, Charges: {2}", item->ID, item->Name, charges);
			return nullptr;
		}

		inst->Initialize(this);
	}
	return inst;
}

int32 SharedDatabase::DeleteStalePlayerCorpses() 
{
	int32 rows_affected = 0;
	if(RuleB(Zone, EnableShadowrest)) {
        std::string query = StringFormat(
			"UPDATE `character_corpses` SET `is_buried` = 1 WHERE `is_buried` = 0 AND "
            "(UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) > %d AND NOT time_of_death = 0",
             (RuleI(Character, CorpseDecayTimeMS) / 1000));
        auto results = QueryDatabase(query);
		if (!results.Success())
			return -1;

		rows_affected += results.RowsAffected();

		std::string sr_query = StringFormat(
			"DELETE FROM `character_corpses` WHERE `is_buried` = 1 AND (UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) > %d "
			"AND NOT time_of_death = 0", (RuleI(Character, CorpseDecayTimeMS) / 1000)*2);
		 auto sr_results = QueryDatabase(sr_query);
		 if (!sr_results.Success())
			 return -1;

		rows_affected += sr_results.RowsAffected();

	}
	else
	{
		std::string query = StringFormat(
			"DELETE FROM `character_corpses` WHERE (UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) > %d "
			"AND NOT time_of_death = 0", (RuleI(Character, CorpseDecayTimeMS) / 1000));
		auto results = QueryDatabase(query);
		if (!results.Success())
			return -1;

		rows_affected += results.RowsAffected();
	}

	//if(RuleB(Character, UsePlayerCorpseBackups))
	//{
	//	std::string cb_query = StringFormat(
	//		"SELECT id FROM `character_corpses_backup`");
	//	auto cb_results = QueryDatabase(cb_query);
	//	for (auto row = cb_results.begin(); row != cb_results.end(); ++row) {
	//		uint32 corpse_id = atoi(row[0]);
	//		std::string cbd_query = StringFormat(
	//			"DELETE from character_corpses_backup where id = %d AND ( "
	//			"SELECT COUNT(*) from character_corpse_items_backup where corpse_id = %d) "
	//			" = 0", corpse_id, corpse_id);
	//		auto cbd_results = QueryDatabase(cbd_query);
	//		if(!cbd_results.Success())
	//			return -1;

	//		rows_affected += cbd_results.RowsAffected();
	//	}
	//}

    return rows_affected;
}

bool SharedDatabase::GetCommandSettings(std::map<std::string, std::pair<uint8, std::vector<std::string>>>& command_settings) 
{
	command_settings.clear();

	const std::string& query = "SELECT `command`, `access`, `aliases` FROM `command_settings`";
	auto results = QueryDatabase(query);
	if (!results.Success() || !results.RowCount()) {
		return false;
	}

	for (auto row : results) {
		command_settings[row[0]].first = Strings::ToUnsignedInt(row[1]);
		if (row[2][0] == 0) {
			continue;
		}

		std::vector<std::string> aliases = Strings::Split(row[2], '|');
		for (const auto& e : aliases) {
			if (e.empty()) {
				continue;
			}

			command_settings[row[0]].second.push_back(e);
		}
	}

    return true;
}

bool SharedDatabase::UpdateInjectedCommandSettings(const std::vector<std::pair<std::string, uint8>>& injected)
{
	if (injected.size()) {

		std::string query = fmt::format(
			"REPLACE INTO `command_settings`(`command`, `access`) VALUES {}",
			Strings::ImplodePair(
				",",
				std::pair<char, char>('(', ')'),
				join_pair(",", std::pair<char, char>('\'', '\''), injected)
			)
		);

		auto results = QueryDatabase(query);
		if (!results.Success()) {
			return false;
		}

		LogInfo(
			"[{}] New Command{} Added",
			injected.size(),
			injected.size() != 1 ? "s" : ""
		);
	}

	return true;
}

bool SharedDatabase::UpdateOrphanedCommandSettings(const std::vector<std::string>& orphaned)
{

	if (orphaned.size()) {
		std::string query = fmt::format(
			"DELETE FROM `command_settings` WHERE `command` IN ({})",
			Strings::ImplodePair(",", std::pair<char, char>('\'', '\''), orphaned)
		);

		auto results = QueryDatabase(query);
		if (!results.Success()) {
			return false;
		}

		query = fmt::format(
			"DELETE FROM `command_subsettings` WHERE `parent_command` IN ({})",
			Strings::ImplodePair(",", std::pair<char, char>('\'', '\''), orphaned)
		);

		auto results_two = QueryDatabase(query);
		if (!results_two.Success()) {
			return false;
		}

		LogInfo(
			"{} Orphaned Command{} Deleted | {} Orphaned Subcommand{} Deleted",
			orphaned.size(),
			orphaned.size() != 1 ? "s" : "",
			results_two.RowsAffected(),
			results_two.RowsAffected() != 1 ? "s" : ""
		);
	}

	return true;
}

bool SharedDatabase::GetCommandSubSettings(std::vector<CommandSubsettingsRepository::CommandSubsettings>& command_subsettings)
{
	command_subsettings.clear();

	const auto& l = CommandSubsettingsRepository::GetAll(*this);

	if (l.empty()) {
		return false;
	}

	command_subsettings.reserve(l.size());

	for (const auto& e : l) {
		command_subsettings.emplace_back(e);
	}

	return true;
}

void SharedDatabase::LoadDamageShieldTypes(SPDat_Spell_Struct* sp, int32 iMaxSpellID)
{

	std::string query = StringFormat("SELECT `spellid`, `type` FROM `damageshieldtypes` WHERE `spellid` > 0 "
                                    "AND `spellid` <= %i", iMaxSpellID);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for(auto row = results.begin(); row != results.end(); ++row) {
        int spellID = atoi(row[0]);
        if((spellID > 0) && (spellID <= iMaxSpellID))
            sp[spellID].DamageShieldType = atoi(row[1]);
    }

}

int SharedDatabase::GetMaxSpellID()
{
	std::string query = "SELECT MAX(id) FROM spells_new";
	auto results = QueryDatabase(query);
    if (!results.Success()) {
        return -1;
    }

    auto row = results.begin();

	return atoi(row[0]);
}

bool SharedDatabase::LoadSpells(const std::string &prefix, int32 *records, const SPDat_Spell_Struct **sp) 
{
	spells_mmf.reset(nullptr);

	try {
		auto Config = EQEmuConfig::get();
		EQ::IPCMutex mutex("spells");
		mutex.Lock();
	
		std::string file_name = fmt::format("{}/{}{}", path.GetSharedMemoryPath(), prefix, std::string("spells"));
		spells_mmf = std::unique_ptr<EQ::MemoryMappedFile>(new EQ::MemoryMappedFile(file_name));
		LogInfo("Loading [{}]", file_name);
		*records = *reinterpret_cast<uint32*>(spells_mmf->Get());
		*sp = reinterpret_cast<const SPDat_Spell_Struct*>((char*)spells_mmf->Get() + 4);
		mutex.Unlock();

		LogInfo("Loaded [{}] spells via shared memory", Strings::Commify(m_shared_spells_count));
	}
	catch(std::exception& ex) {
		LogError("Error Loading Spells: {0}", ex.what());
		return false;
	}

	return true;
}

void SharedDatabase::LoadSpells(void *data, int max_spells)
{
	*(uint32*)data = max_spells;
	SPDat_Spell_Struct *sp = reinterpret_cast<SPDat_Spell_Struct*>((char*)data + sizeof(uint32));

	const std::string query = "SELECT * FROM spells_new ORDER BY id ASC";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    if(results.ColumnCount() < SPELL_LOAD_FIELD_COUNT) {
		LogSpells("Fatal error loading spells: Spell field count < SPELL_LOAD_FIELD_COUNT([{0}])", SPELL_LOAD_FIELD_COUNT);
		return;
    }

    int tempid = 0;
    int counter = 0;

	for (auto row = results.begin(); row != results.end(); ++row) {
		tempid = atoi(row[0]);
		if (tempid >= max_spells) {
			LogSpells("Non fatal error: spell.id >= max_spells, ignoring.");
			continue;
		}

		++counter;
		sp[tempid].id = tempid;
		strn0cpy(sp[tempid].name, row[1], sizeof(sp[tempid].name));
		strn0cpy(sp[tempid].player_1, row[2], sizeof(sp[tempid].player_1));
		strn0cpy(sp[tempid].teleport_zone, row[3], sizeof(sp[tempid].teleport_zone));
		strn0cpy(sp[tempid].you_cast, row[4], sizeof(sp[tempid].you_cast));
		strn0cpy(sp[tempid].other_casts, row[5], sizeof(sp[tempid].other_casts));
		strn0cpy(sp[tempid].cast_on_you, row[6], sizeof(sp[tempid].cast_on_you));
		strn0cpy(sp[tempid].cast_on_other, row[7], sizeof(sp[tempid].cast_on_other));
		strn0cpy(sp[tempid].spell_fades, row[8], sizeof(sp[tempid].spell_fades));
		sp[tempid].range = static_cast<float>(atof(row[9]));
		sp[tempid].aoerange = static_cast<float>(atof(row[10]));
		sp[tempid].pushback = atof(row[11]);
		sp[tempid].pushup = atof(row[12]);
		sp[tempid].cast_time = atoi(row[13]);
		sp[tempid].recovery_time = atoi(row[14]);
		sp[tempid].recast_time = atoi(row[15]);
		sp[tempid].buffdurationformula = atoi(row[16]);
		sp[tempid].buffduration = atoi(row[17]);
		sp[tempid].AEDuration = atoi(row[18]);
		sp[tempid].mana = atoi(row[19]);

		int y = 0;
		for (y = 0; y < EFFECT_COUNT; y++)
			sp[tempid].base[y] = atoi(row[20 + y]); // effect_base_value

		for (y = 0; y < EFFECT_COUNT; y++)
			sp[tempid].base2[y] = atoi(row[32 + y]); // effect_limit_value

		for (y = 0; y < EFFECT_COUNT; y++)
			sp[tempid].max[y] = atoi(row[44 + y]);

		sp[tempid].icon = atoi(row[56]);
		sp[tempid].memicon = atoi(row[57]);

		for (y = 0; y < 4; y++)
			sp[tempid].components[y] = atoi(row[58 + y]);

		for (y = 0; y < 4; y++)
			sp[tempid].component_counts[y] = atoi(row[62 + y]);

		for (y = 0; y < 4; y++)
			sp[tempid].NoexpendReagent[y] = atoi(row[66 + y]);

		for (y = 0; y < EFFECT_COUNT; y++)
			sp[tempid].formula[y] = atoi(row[70 + y]);

		sp[tempid].LightType = atoi(row[82]);
		sp[tempid].goodEffect = atoi(row[83]);
		sp[tempid].Activated = atoi(row[84]);
		sp[tempid].resisttype = atoi(row[85]);

		for (y = 0; y < EFFECT_COUNT; y++)
			sp[tempid].effectid[y] = atoi(row[86 + y]);

		sp[tempid].targettype = (SpellTargetType)atoi(row[98]);
		sp[tempid].basediff = atoi(row[99]);

		int tmp_skill = atoi(row[100]);

		if (tmp_skill < 0 || tmp_skill > EQ::skills::HIGHEST_SKILL)
			sp[tempid].skill = EQ::skills::SkillBegging; /* not much better we can do. */ // can probably be changed to client-based 'SkillNone' once activated
		else
			sp[tempid].skill = (EQ::skills::SkillType)tmp_skill;

		sp[tempid].zonetype = atoi(row[101]);
		sp[tempid].EnvironmentType = atoi(row[102]);
		sp[tempid].TimeOfDay = atoi(row[103]);

		for (y = 0; y < Class::PLAYER_CLASS_COUNT; y++)
			sp[tempid].classes[y] = atoi(row[104 + y]);

		sp[tempid].CastingAnim = atoi(row[119]);
		sp[tempid].TargetAnim = atoi(row[120]);
		sp[tempid].TravelType = atoi(row[121]);
		sp[tempid].SpellAffectIndex = atoi(row[122]);
		sp[tempid].disallow_sit = atoi(row[123]);
		sp[tempid].deity_agnostic = atoi(row[124]);

		for (y = 0; y < 16; y++)
			sp[tempid].deities[y] = atoi(row[125 + y]);

		sp[tempid].npc_no_cast = atoi(row[141]);
		sp[tempid].ai_pt_bonus = atoi(row[142]);
		sp[tempid].new_icon = atoi(row[143]);
		sp[tempid].spellanim = atoi(row[144]);
		sp[tempid].uninterruptable = atoi(row[145]) != 0;
		sp[tempid].ResistDiff = atoi(row[146]);
		sp[tempid].dot_stacking_exempt = atoi(row[147]);
		sp[tempid].deletable = atoi(row[148]);
		sp[tempid].RecourseLink = atoi(row[149]);
		sp[tempid].no_partial_resist = atoi(row[150]) != 0;
		sp[tempid].small_targets_only = atoi(row[151]);
		sp[tempid].use_persistent_particles = atoi(row[152]);
		sp[tempid].short_buff_box = atoi(row[153]);
		sp[tempid].descnum = atoi(row[154]);
		sp[tempid].typedescnum = atoi(row[155]);
		sp[tempid].effectdescnum = atoi(row[156]);
		sp[tempid].effectdescnum2 = atoi(row[157]);
		sp[tempid].npc_no_los = atoi(row[158]) != 0;

		/* Below this point are custom fields for our client*/
		sp[tempid].reflectable = atoi(row[159]) != 0;
		sp[tempid].resist_per_level = atoi(row[160]);
		sp[tempid].resist_cap = atoi(row[161]);
		sp[tempid].EndurCost = atoi(row[162]);
		sp[tempid].EndurTimerIndex = atoi(row[163]);
		sp[tempid].IsDisciplineBuff = atoi(row[164]) != 0;
		sp[tempid].HateAdded = atoi(row[165]);
		sp[tempid].EndurUpkeep = atoi(row[166]);
		sp[tempid].pvpresistbase = atoi(row[167]);
		sp[tempid].pvpresistcalc = atoi(row[168]);
		sp[tempid].pvpresistcap = atoi(row[169]);
		sp[tempid].spell_category = atoi(row[170]);
		sp[tempid].pvp_duration = atoi(row[171]);
		sp[tempid].pvp_duration_cap = atoi(row[172]);
		sp[tempid].cast_not_standing = atoi(row[173]) != 0;
		sp[tempid].can_mgb = atoi(row[174]);
		sp[tempid].dispel_flag = atoi(row[175]);
		sp[tempid].npc_category = atoi(row[176]);
		sp[tempid].npc_usefulness = atoi(row[177]);
		sp[tempid].wear_off_message = atoi(row[178]) != 0;
		sp[tempid].suspendable = atoi(row[179]) != 0;
		sp[tempid].spellgroup = atoi(row[180]);
		sp[tempid].allow_spellscribe = atoi(row[181]) != 0;
		sp[tempid].AllowRest = atoi(row[182]) != 0;
		sp[tempid].custom_icon = atoi(row[183]);
		sp[tempid].not_player_spell = atoi(row[184]) != 0;
		sp[tempid].DamageShieldType = 0;
		sp[tempid].disabled = atoi(row[185]) != 0;
		sp[tempid].persist_through_death = atoi(row[186]) != 0;

		// other effects associated with spells, to allow quick access
		sp[tempid].min_castinglevel = 0;
		sp[tempid].bardsong = false;
		sp[tempid].poison_counters = 0;
		sp[tempid].disease_counters = 0;
		sp[tempid].curse_counters = 0;
		sp[tempid].manatapspell = false;
		sp[tempid].hasrecourse = false;
		sp[tempid].contains_se_currentmana = false;

		int r, min = 255;
		for (r = 0; r < Class::PLAYER_CLASS_COUNT; r++)
			if (sp[tempid].classes[r] < min)
				min = sp[tempid].classes[r];

		// if we can't cast the spell return 0
		// just so it wont screw up calculations used in other areas of the code
		// seen 127, 254, 255
		if (min < 127)
			sp[tempid].min_castinglevel = min;

		if (sp[tempid].classes[Class::Bard - 1] < 255)
			sp[tempid].bardsong = true;

		if (sp[tempid].RecourseLink != 0 && sp[tempid].RecourseLink != 1 && sp[tempid].RecourseLink != 0xFFFFFFFF)
			sp[tempid].hasrecourse = true;

		for (int i = 0; i < EFFECT_COUNT; i++) {
			if (sp[tempid].effectid[i] == SE_PoisonCounter &&
				sp[tempid].base[i] > 0)
				sp[tempid].poison_counters += sp[tempid].base[i];
			if (sp[tempid].effectid[i] == SE_DiseaseCounter &&
				sp[tempid].base[i] > 0)
				sp[tempid].disease_counters += sp[tempid].base[i];
			if (sp[tempid].effectid[i] == SE_CurseCounter &&
				sp[tempid].base[i] > 0)
				sp[tempid].curse_counters += sp[tempid].base[i];
			if (sp[tempid].effectid[i] == SE_CurrentMana &&
				sp[tempid].targettype == ST_Tap)
				sp[tempid].manatapspell = true;
			if (sp[tempid].effectid[i] == SE_CurrentMana)
				sp[tempid].contains_se_currentmana = true;
		}

    }

    LoadDamageShieldTypes(sp, max_spells);
}

bool SharedDatabase::VerifyToken(std::string token, int& status)
{
	status = 0;
	if (token.length() > 64) 
	{
		token = token.substr(0, 64);
	}

	token = Strings::Escape(token);

	std::string query = StringFormat("SELECT status FROM tokens WHERE token='%s'", token.c_str());
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0)
	{
		std::cerr << "Error in SharedDatabase::VerifyToken" << std::endl;
	}

	auto row = results.begin();

	status = atoi(row[0]);

	return results.Success();
}

uint32 SharedDatabase::GetSpellsCount()
{
	auto results = QueryDatabase("SELECT count(*) FROM spells_new");
	if (!results.Success() || !results.RowCount()) {
		return 0;
	}

	auto& row = results.begin();

	if (row[0]) {
		return atoul(row[0]);
	}

	return 0;
}

uint32 SharedDatabase::GetItemsCount()
{
	auto results = QueryDatabase("SELECT count(*) FROM items");
	if (!results.Success() || !results.RowCount()) {
		return 0;
	}

	auto& row = results.begin();

	if (row[0]) {
		return atoul(row[0]);
	}

	return 0;
}

void SharedDatabase::SetSharedItemsCount(uint32 shared_items_count)
{
	SharedDatabase::m_shared_items_count = shared_items_count;
}

void SharedDatabase::SetSharedSpellsCount(uint32 shared_spells_count)
{
	SharedDatabase::m_shared_spells_count = shared_spells_count;
}
