/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2006 EQEMu Development Team (http://eqemulator.net)

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

#include "worlddb.h"
#include "../common/strings.h"
#include "../common/eq_packet_structs.h"
#include "../common/inventory_profile.h"
#include "../common/rulesys.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include "char_create_data.h"

WorldDatabase database;
extern std::vector<RaceClassAllocation> character_create_allocations;
extern std::vector<RaceClassCombos> character_create_race_class_combos;


// solar: the current stuff is at the bottom of this function
void WorldDatabase::GetCharSelectInfo(uint32 account_id, CharacterSelect_Struct* cs, uint32 ClientVersion, uint8 &charcount, bool mule) {
	EQ::InventoryProfile *inv;
	uint8 has_home = 0;
	uint8 has_bind = 0;

	/* Initialize Variables */
	for (int i=0; i<10; i++) {
		strcpy(cs->name[i], "<none>");
		cs->zone[i] = 0;
		cs->level[i] = 0;
	}

	uint8 limit = mule ? RuleI(World, MuleToonLimit) : 8; //Mule accounts are limited by their rule, everybody else gets 8.

	/* Get Character Info */
	std::string cquery = StringFormat(
		"SELECT                     "
		"`id`,                      "  // 0
		"name,                      "  // 1
		"gender,                    "  // 2
		"race,                      "  // 3
		"class,                     "  // 4
		"`level`,                   "  // 5
		"deity,                     "  // 6
		"last_login,                "  // 7
		"time_played,               "  // 8
		"hair_color,                "  // 9
		"beard_color,               "  // 10
		"eye_color_1,               "  // 11
		"eye_color_2,               "  // 12
		"hair_style,                "  // 13
		"beard,                     "  // 14
		"face,                      "  // 15
		"zone_id,		            "  // 16
		"showhelm					"  // 17
		"FROM                       "
		"character_data             "
		"WHERE `account_id` = %i AND is_deleted = 0 ORDER BY `name` LIMIT %d   ", account_id, limit);
	auto results = database.QueryDatabase(cquery); int char_num = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		PlayerProfile_Struct pp;
		memset(&pp, 0, sizeof(PlayerProfile_Struct));

		uint32 character_id = atoi(row[0]);
		strcpy(cs->name[char_num], row[1]);
		uint8 lvl = atoi(row[5]);
		cs->level[char_num] = lvl;
		cs->class_[char_num] = atoi(row[4]);
		cs->race[char_num] = atoi(row[3]);
		cs->gender[char_num] = atoi(row[2]);
		cs->deity[char_num] = atoi(row[6]);
		cs->zone[char_num] = database.GetClientZoneID((uint16)atoi(row[16]));
		cs->face[char_num] = atoi(row[15]);
		cs->haircolor[char_num] = atoi(row[9]);
		cs->beardcolor[char_num] = atoi(row[10]);
		cs->eyecolor2[char_num] = atoi(row[12]);
		cs->eyecolor1[char_num] = atoi(row[11]);
		cs->hairstyle[char_num] = atoi(row[13]);
		cs->beard[char_num] = atoi(row[14]);
		bool showhelm = atobool(row[17]);


		/* Set Bind Point Data for any character that may possibly be missing it for any reason */
		cquery = StringFormat("SELECT `zone_id`, `x`, `y`, `z`, `heading`, `is_home` FROM `character_bind`  WHERE `id` = %i LIMIT 2", character_id);
		auto results_bind = database.QueryDatabase(cquery); has_home = 0; has_bind = 0;
		for (auto row_b = results_bind.begin(); row_b != results_bind.end(); ++row_b) {
			if (row_b[5] && atoi(row_b[5]) == 1){ has_home = 1; }
			if (row_b[5] && atoi(row_b[5]) == 0){ has_bind = 1; }
		}

		if (has_home == 0 || has_bind == 0){
			cquery = StringFormat("SELECT `zone_id`, `bind_id`, `x`, `y`, `z` FROM `start_zones` WHERE `player_class` = %i AND `player_deity` = %i AND `player_race` = %i",
				cs->class_[char_num], cs->deity[char_num], cs->race[char_num]);
			auto results_bind = database.QueryDatabase(cquery);
			for (auto row_d = results_bind.begin(); row_d != results_bind.end(); ++row_d) {
				/* If a bind_id is specified, make them start there */
				if (atoi(row_d[1]) != 0) {
					pp.binds[4].zoneId = (uint32)atoi(row_d[1]);
					GetSafePoints(pp.binds[4].zoneId, &pp.binds[4].x, &pp.binds[4].y, &pp.binds[4].z, &pp.binds[4].heading);
				}
				/* Otherwise, use the zone and coordinates given */
				else {
					pp.binds[4].zoneId = (uint32)atoi(row_d[0]);
					float x = atof(row_d[2]);
					float y = atof(row_d[3]);
					float z = atof(row_d[4]);
					float heading = atof(row_d[5]);
					if (x == 0 && y == 0 && z == 0 && heading == 0){ GetSafePoints(pp.binds[4].zoneId, &x, &y, &z, &heading); }
					pp.binds[4].x = x; 
					pp.binds[4].y = y; 
					pp.binds[4].z = z; 
					pp.binds[4].heading;
				}
			}
			pp.binds[0] = pp.binds[4];
			/* If no home bind set, set it */
			if (has_home == 0){
				std::string query = StringFormat("REPLACE INTO `character_bind` (id, zone_id, x, y, z, heading, is_home)"
					" VALUES (%u, %u, %f, %f, %f, %f, %i)",
					character_id, pp.binds[4].zoneId, pp.binds[4].x, pp.binds[4].y, pp.binds[4].z, pp.binds[4].heading, 1);
				auto results_bset = QueryDatabase(query); 
			}
			/* If no regular bind set, set it */
			if (has_bind == 0){
				std::string query = StringFormat("REPLACE INTO `character_bind` (id, zone_id, x, y, z, heading, is_home)"
					" VALUES (%u, %u, %f, %f, %f, %f, %i)",
					character_id, pp.binds[0].zoneId, pp.binds[0].x, pp.binds[0].y, pp.binds[0].z, pp.binds[0].heading, 0);
				auto results_bset = QueryDatabase(query); 
			}
		}
		/* Bind End */

		/*
			Character's equipped items
			@merth: Haven't done bracer01/bracer02 yet.
			Also: this needs a second look after items are a little more solid
			NOTE: items don't have a color, players MAY have a tint, if the
			use_tint part is set. otherwise use the regular color
		*/

		/* Load Inventory */
		inv = new EQ::InventoryProfile;
		bool deletenorent = database.NoRentExpired(cs->name[char_num]);
		if (GetInventory(account_id, cs->name[char_num], inv)) {
			for (uint8 material = EQ::textures::textureBegin; material <= EQ::textures::LastTexture; material++) {
				uint32 color = 0;
				EQ::ItemInstance *item = inv->GetItem(EQ::InventoryProfile::CalcSlotFromMaterial(material));

				// NoRent is 0.
				if (item && !item->GetItem()->NoRent && deletenorent)
				{
					continue;
				}

				if (item == 0)
				{
					if (material == EQ::textures::armorWrist)
					{
						// We don't have an item in Wrist1, check Wrist2.
						item = inv->GetItem(EQ::invslot::slotWrist2);
					}

					if (item == 0)
						continue;
				}

				uint32 final_color = item->GetItem()->Color;
				uint32 final_mat = item->GetItem()->Material;

				/* Weapons are handled a bit differently */
				if ((material == EQ::textures::weaponPrimary) || (material == EQ::textures::weaponSecondary)) {
					if (strlen(item->GetItem()->IDFile) > 2) {
						uint32 idfile;
						idfile = atoi(&item->GetItem()->IDFile[2]);

						if (material == EQ::textures::weaponPrimary)
							cs->primary[char_num] = idfile;
						else if (material == EQ::textures::weaponSecondary)
							cs->secondary[char_num] = idfile;
					}
				}
				else if (material == EQ::textures::armorHead)
				{
					if (!showhelm)
					{
						final_mat = 0;
						final_color = 0;
					}
					else if (strlen(item->GetItem()->IDFile) > 2)
					{
						uint32 idfile;
						idfile = atoi(&item->GetItem()->IDFile[2]);
						if (idfile == 240 && item->GetItem()->Material == TextureCloth)
							final_mat = idfile;
					}
				}
				
				cs->cs_colors[char_num].Slot[material].Color = final_color;
				cs->equip[char_num].Slot[material].Material = final_mat;
			}
		}
		else {
			Log(Logs::General, Logs::Inventory, "Error loading inventory for %s\n", cs->name[char_num]);
		}
		safe_delete(inv);

		++char_num;
		if (char_num >= limit)
		{
			charcount = limit;
			break;
		}
		else
		{
			charcount = char_num;
		}
	}

	return;
}

void WorldDatabase::ClearHardcoreCharacters(int AccID) {
	/* Get Character Info */
	std::string cquery = StringFormat(
		"SELECT                     "
		"`id`,                      "  // 0
		"name                      "  // 1
		"FROM                       "
		"character_data             "
		"WHERE `account_id` = %i AND e_hardcore_death_time != 0 ORDER BY `name`", AccID);
	auto results = database.QueryDatabase(cquery); int char_num = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		database.DeleteCharacter(row[1]);
	}
}

int WorldDatabase::MoveCharacterToBind(int CharID, uint8 bindnum) {
	/*  if an invalid bind point is specified, use the primary bind */
	if (bindnum > 4)
	{
		bindnum = 0;
	}

	std::string query = StringFormat("SELECT zone_id, x, y, z, heading FROM character_bind WHERE id = %u AND is_home = %u LIMIT 1", CharID, bindnum == 4 ? 1 : 0);
	auto results = database.QueryDatabase(query);
	if(!results.Success() || results.RowCount() == 0) {
		return 0;
	}

	int zone_id = 0;
	double x = 0;
	double y = 0;
	double z = 0;
	double heading = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		zone_id = atoi(row[0]);
		x = atof(row[1]);
		y = atof(row[2]);
		z = atof(row[3]);
		heading = atof(row[4]);
	}

	query = StringFormat("UPDATE character_data SET zone_id = '%d', x = '%f', y = '%f', z = '%f', heading = '%f' WHERE id = %u", 
						 zone_id, x, y, z, heading, CharID);

	results = database.QueryDatabase(query);
	if(!results.Success()) {
		return 0;
	}

	return zone_id;
}

bool WorldDatabase::GetStartZone(PlayerProfile_Struct* in_pp, CharCreate_Struct* in_cc, bool mule)
{
	if(!in_pp || !in_cc)
		return false;

	if (mule)
	{
		if (RuleB(Quarm, EastCommonMules)) {
			Log(Logs::General, Logs::Status, "%s: Starting mule character in EC", in_pp->name);
			in_pp->x = in_pp->binds[0].x = -164;
			in_pp->y = in_pp->binds[0].y = -1651;
			in_pp->z = in_pp->binds[0].z = 4;
			in_pp->zone_id = in_pp->binds[0].zoneId = ecommons;
			return true;
		}
		else {
			Log(Logs::General, Logs::Status, "%s: Starting mule character in Bazaar", in_pp->name);
			in_pp->x = in_pp->binds[0].x = 140;
			in_pp->y = in_pp->binds[0].y = -821;
			in_pp->z = in_pp->binds[0].z = 5;
			in_pp->zone_id = in_pp->binds[0].zoneId = bazaar;
			return true;
		}
	}

	in_pp->x = in_pp->y = in_pp->z = in_pp->heading = in_pp->zone_id = 0;
	in_pp->binds[0].x = in_pp->binds[0].y = in_pp->binds[0].z = in_pp->binds[0].zoneId = 0;

    std::string query = StringFormat("SELECT x, y, z, heading, bind_id,bind_x,bind_y,bind_z FROM start_zones WHERE zone_id = %i "
                                    "AND player_class = %i AND player_deity = %i AND player_race = %i",
                                    in_cc->start_zone, in_cc->class_, in_cc->deity, in_cc->race);
    auto results = QueryDatabase(query);
	if(!results.Success()) {
		return false;
	}

	Log(Logs::General, Logs::Status, "Start zone query: %s", query.c_str());

    if (results.RowCount() == 0) {
        Log(Logs::General, Logs::Status, "%s: No start_zones entry in database, using defaults", in_pp->name);
		in_pp->x = in_pp->binds[0].x = 140;
		in_pp->y = in_pp->binds[0].y = -821;
		in_pp->z = in_pp->binds[0].z = 5;
		in_pp->zone_id = in_pp->binds[0].zoneId = bazaar;
    }
    else
	{
		Log(Logs::General, Logs::Status, "%s: Found starting location in start_zones", in_pp->name);
		auto row = results.begin();
		in_pp->x = atof(row[0]);
		in_pp->y = atof(row[1]);
		in_pp->z = atof(row[2]);
		in_pp->heading = atof(row[3]);
		in_pp->zone_id = in_cc->start_zone;
		in_pp->binds[0].zoneId = atoi(row[4]);
		in_pp->binds[0].x = atoi(row[5]);
		in_pp->binds[0].y = atoi(row[6]);
		in_pp->binds[0].z = atoi(row[7]);
	}

	if(in_pp->x == 0 && in_pp->y == 0 && in_pp->z == 0 && in_pp->heading == 0)
		database.GetSafePoints(in_pp->zone_id, &in_pp->x, &in_pp->y, &in_pp->z, &in_pp->heading);

	if(in_pp->binds[0].x == 0 && in_pp->binds[0].y == 0 && in_pp->binds[0].z == 0 && in_pp->binds[0].heading == 0)
		database.GetSafePoints(in_pp->binds[0].zoneId, &in_pp->binds[0].x, &in_pp->binds[0].y, &in_pp->binds[0].z, &in_pp->binds[0].heading);

	return true;
}

void WorldDatabase::GetLauncherList(std::vector<std::string> &rl) {
	rl.clear();

    const std::string query = "SELECT name FROM launcher";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        LogError("WorldDatabase::GetLauncherList: {} ", results.ErrorMessage().c_str());
        return;
    }

    for (auto row = results.begin(); row != results.end(); ++row)
        rl.push_back(row[0]);

}

void WorldDatabase::SetMailKey(int CharID, int IPAddress, int MailKey) {

	char MailKeyString[17];

	if(RuleB(Chat, EnableMailKeyIPVerification) == true)
		sprintf(MailKeyString, "%08X%08X", IPAddress, MailKey);
	else
		sprintf(MailKeyString, "%08X", MailKey);

    std::string query = StringFormat("UPDATE character_data SET mailkey = '%s' WHERE id = '%i'",
                                    MailKeyString, CharID);
    auto results = QueryDatabase(query);
	if (!results.Success())
		Log(Logs::General, Logs::Error, "WorldDatabase::SetMailKey(%i, %s) : %s", CharID, MailKeyString, results.ErrorMessage().c_str());

}

bool WorldDatabase::GetCharacterLevel(const char *name, int &level)
{
	std::string query = StringFormat("SELECT level FROM character_data WHERE name = '%s'", name);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        LogError("WorldDatabase::GetCharacterLevel: {} ", results.ErrorMessage().c_str());
        return false;
	}

	if (results.RowCount() == 0)
        return false;

    auto row = results.begin();
    level = atoi(row[0]);

    return true;
}

bool WorldDatabase::LoadCharacterCreateAllocations() {
	character_create_allocations.clear();

	std::string query = "SELECT * FROM char_create_point_allocations ORDER BY id";
	auto results = QueryDatabase(query);
	if (!results.Success())
        return false;

    for (auto row = results.begin(); row != results.end(); ++row) {
        RaceClassAllocation allocate;
		allocate.Index = atoi(row[0]);
		allocate.BaseStats[0] = atoi(row[1]);
		allocate.BaseStats[3] = atoi(row[2]);
		allocate.BaseStats[1] = atoi(row[3]);
		allocate.BaseStats[2] = atoi(row[4]);
		allocate.BaseStats[4] = atoi(row[5]);
		allocate.BaseStats[5] = atoi(row[6]);
		allocate.BaseStats[6] = atoi(row[7]);
		allocate.DefaultPointAllocation[0] = atoi(row[8]);
		allocate.DefaultPointAllocation[3] = atoi(row[9]);
		allocate.DefaultPointAllocation[1] = atoi(row[10]);
		allocate.DefaultPointAllocation[2] = atoi(row[11]);
		allocate.DefaultPointAllocation[4] = atoi(row[12]);
		allocate.DefaultPointAllocation[5] = atoi(row[13]);
		allocate.DefaultPointAllocation[6] = atoi(row[14]);

		character_create_allocations.push_back(allocate);
    }

	return true;
}

bool WorldDatabase::LoadCharacterCreateCombos() {
	character_create_race_class_combos.clear();

	std::string query = "SELECT * FROM char_create_combinations ORDER BY race, class, deity, start_zone";
	auto results = QueryDatabase(query);
	if (!results.Success())
        return false;

	for (auto row = results.begin(); row != results.end(); ++row) {
		RaceClassCombos combo;
		combo.AllocationIndex = atoi(row[0]);
		combo.Race = atoi(row[1]);
		combo.Class = atoi(row[2]);
		combo.Deity = atoi(row[3]);
		combo.Zone = atoi(row[4]);
		combo.ExpansionRequired = atoi(row[5]);

		character_create_race_class_combos.push_back(combo);
	}

	return true;
}

bool WorldDatabase::LoadSoulMarksForClient(uint32 charid, std::vector<SoulMarkEntry_Struct>& outData) {

	std::string query = StringFormat("SELECT charname, acctname, gmname, gmacctname, utime, type, `desc` FROM character_soulmarks where charid=%i", charid);
	auto results = QueryDatabase(query);
	if (!results.Success())
        return false;

	for (auto row = results.begin(); row != results.end(); ++row) {
		SoulMarkEntry_Struct entry;
		strncpy(entry.name, row[0], 64);
		strncpy(entry.accountname, row[1], 32);
		strncpy(entry.gmname, row[2], 64);
		strncpy(entry.gmaccountname, row[3], 32);
		entry.unix_timestamp = atoi(row[4]);
		entry.type = atoi(row[5]);
		strncpy(entry.description, row[6], 256);
		outData.push_back(entry);
	}

	return true;
}

bool WorldDatabase::GITInfo()
{
	std::string check_query1 = StringFormat("SELECT * FROM `variables` WHERE `varname`='git-HEAD-hash'");
	auto results1 = QueryDatabase(check_query1);
	if (results1.RowCount() == 0)
	{
		std::string check_query2 = StringFormat("INSERT INTO `variables` (`varname`, `value`, `information`) VALUES ('git-HEAD-hash', '', 'git rev-parse HEAD')");
		auto results2 = QueryDatabase(check_query2);
		if (!results2.Success())
		{
			Log(Logs::Detail, Logs::Error, "Error creating git-HEAD-hash field.");
			return false;
		}
	}
	std::string check_query3 = StringFormat("SELECT * FROM `variables` WHERE `varname`='git-BRANCH'");
	auto results3 = QueryDatabase(check_query3);
	if (results3.RowCount() == 0)
	{
		std::string check_query4 = StringFormat("INSERT INTO `variables` (`varname`, `value`, `information`) VALUES ('git-BRANCH', '', 'git rev-parse --abbrev-ref HEAD')");
		auto results4 = QueryDatabase(check_query4);
		if (!results4.Success())
		{
			Log(Logs::Detail, Logs::Error, "Error creating git-BRANCH field.");
			return false;
		}
	}
	const char* hash;
	const char* branch;
	FILE *fhash;
	FILE *fbranch;
#ifdef GITSOURCE
	const char * source_path = nullptr;
	if (GITSOURCE != nullptr)
		source_path = GITSOURCE;
	else
		source_path = "./source/.git";
#else
	const char * source_path = "./source/.git";
#endif
#ifdef _WINDOWS
	char buf[1024];
	std::string gitParseHead = StringFormat("C:\\\"Program Files (x86)\"\\Git\\bin\\git --git-dir=%s rev-parse HEAD", source_path);
	fhash = _popen(gitParseHead.c_str(), "r"), source_path;
	while (fgets(buf, 1024, fhash))
	{
		const char * output = (const char *)buf;
		hash = output;

		std::string hashstring = std::string(hash); int pos;
		if ((pos = hashstring.find('\n')) != std::string::npos)
			hashstring.erase(pos);

		const char * hash = hashstring.c_str();

		std::string queryhash = StringFormat("UPDATE `variables` SET `value`='%s' WHERE(`varname`='git-HEAD-hash')", hash);
		auto resultshash = QueryDatabase(queryhash);
		if (!resultshash.Success())
		{
			Log(Logs::Detail, Logs::Error, "Error entering hash to variables.");
			fclose(fhash);
			return false;
		}
	}
	fclose(fhash);

	char buf2[1024];
	std::string gitParseHeadabbrev = StringFormat("C:\\\"Program Files (x86)\"\\Git\\bin\\git --git-dir=%s rev-parse --abbrev-ref HEAD", source_path);
	fbranch = _popen(gitParseHeadabbrev.c_str(), "r"), source_path;
	while (fgets(buf2, 1024, fbranch))
	{
		const char * output = (const char *)buf2;
		branch = output;

		std::string branchstring = std::string(branch); int pos;
		if ((pos = branchstring.find('\n')) != std::string::npos)
			branchstring.erase(pos);

		const char * branch = branchstring.c_str();

		std::string querybranch = StringFormat("UPDATE `variables` SET `value`='%s' WHERE(`varname`='git-BRANCH')", branch);
		auto resultsbranch = QueryDatabase(querybranch);
		if (!resultsbranch.Success())
		{
			Log(Logs::Detail, Logs::Error, "Error entering branch to variables.");
			fclose(fbranch);
			return false;
		}
	}
	fclose(fbranch);
#else
	char* buf = NULL;
	size_t len = 0;
	fflush(NULL);
	std::string gitParseHead = StringFormat("git --git-dir=%s rev-parse HEAD", source_path);
	fhash = popen(gitParseHead.c_str(), "r");

	while (getline(&buf, &len, fhash) != -1)
	{
		const char * output = (const char *)buf;
		hash = output;

		std::string hashstring = std::string(hash); int pos;
		if ((pos = hashstring.find('\n')) != std::string::npos)
			hashstring.erase(pos);

		const char * hash = hashstring.c_str();


		std::string queryhash = StringFormat("UPDATE `variables` SET `value`='%s' WHERE(`varname`='git-HEAD-hash')", hash);
		auto resultshash = QueryDatabase(queryhash);
		if (!resultshash.Success())
		{
			Log(Logs::Detail, Logs::Error, "Error entering hash to variables.");
			free(buf);
			fflush(fhash);
			return false;
		}
	}
	free(buf);
	fflush(fhash);

	char* buf2 = NULL;
	len = 0;
	std::string gitParseHeadabbrev = StringFormat("git --git-dir=%s rev-parse --abbrev-ref HEAD", source_path);
	fbranch = popen(gitParseHeadabbrev.c_str(), "r");
	while (getline(&buf2, &len, fbranch) != -1)
	{
		const char * output = (const char *)buf2;
		branch = output;

		std::string branchstring = std::string(branch); int pos;
		if ((pos = branchstring.find('\n')) != std::string::npos)
			branchstring.erase(pos);

		const char * branch = branchstring.c_str();

		std::string querybranch = StringFormat("UPDATE `variables` SET `value`='%s' WHERE(`varname`='git-BRANCH')", branch);
		auto resultsbranch = QueryDatabase(querybranch);
		if (!resultsbranch.Success())
		{
			Log(Logs::Detail, Logs::Error, "Error entering branch to variables.");
			free(buf2);
			fflush(fbranch);
			return false;
		}
	}
	free(buf2);
	fflush(fbranch);
#endif
	return true;
}