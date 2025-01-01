/*	EQEMu: Everquest Server Emulator
Copyright (C) 2001-2014 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/servertalk.h"
#include "../common/strings.h"
#include "queryserv.h"
#include "worldserver.h"
#include "zonedb.h"
#include "guild_mgr.h"

extern WorldServer worldserver;
extern QueryServ *QServ;

QueryServ::QueryServ() 
{
}

QueryServ::~QueryServ() 
{
}

void QueryServ::SendQuery(std::string Query)
{
	auto pack = new ServerPacket(ServerOP_QSSendQuery, static_cast<uint32> (Query.length() + 5));
	pack->WriteUInt32(static_cast<uint32> (Query.length())); /* Pack Query String Size so it can be dynamically broken out at queryserv */
	pack->WriteString(Query.c_str()); /* Query */
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QueryServ::PlayerLogEvent(int Event_Type, int Character_ID, std::string Event_Desc)
{
	std::string query = StringFormat(
		"INSERT INTO `qs_player_events` (event, char_id, event_desc, time) VALUES(% i, % i, '%s', UNIX_TIMESTAMP(now()))",
		Event_Type,
		Character_ID,
		Strings::Escape(Event_Desc).c_str());
	SendQuery(query);
}

void QueryServ::QSFirstToEngageEvent(uint32 char_id, std::string guild_name, std::string mob_name, bool engaged)
{
	int engageValue = engaged ? 1 : 0;
	std::string query = StringFormat(
			"INSERT INTO `qs_player_fte_events` (char_id, guild_name, mob_name, engaged, time) VALUES (%i, '%s', '%s', %i, now())",
			char_id, Strings::Escape(guild_name).c_str(), Strings::Escape(mob_name).c_str(), engageValue);
	SendQuery(query);
}

void QueryServ::QSQGlobalUpdate(uint32 char_id, uint32 zone_id, const char* varname, const char* newvalue)
{
	char action[8];
	if (strcmp(newvalue, "deleted") == 0) { strncpy(action, "Deleted", 8); }
	else { strncpy(action, "Updated", 8); }

	auto pack = new ServerPacket(ServerOP_QSPlayerQGlobalUpdates, sizeof(QSPlayerQGlobalUpdate_Struct));
	QSPlayerQGlobalUpdate_Struct* QS = (QSPlayerQGlobalUpdate_Struct*)pack->pBuffer;
	QS->charid = char_id;
	strncpy(QS->action, action, 8);
	QS->zone_id = zone_id;
	strncpy(QS->varname, varname, 32);
	strncpy(QS->newvalue, newvalue, 32);
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSAAPurchases(uint32 char_id, uint32 zone_id, char aa_type[8], char aa_name[128], uint32 aa_id, uint32 cost)
{
	auto pack = new ServerPacket(ServerOP_QSPlayerAAPurchase, sizeof(QSPlayerAAPurchase_Struct));
	QSPlayerAAPurchase_Struct* QS = (QSPlayerAAPurchase_Struct*)pack->pBuffer;
	QS->charid = char_id;
	strncpy(QS->aatype, aa_type, 8);
	strncpy(QS->aaname, aa_name, 128);
	QS->aaid = aa_id;
	QS->cost = cost;
	QS->zone_id = zone_id;
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSAARate(uint32 char_id, uint32 aapoints, uint32 last_unspentAA)
{
	auto pack = new ServerPacket(ServerOP_QSPlayerAARateHourly, sizeof(QSPlayerAARateHourly_Struct));
	QSPlayerAARateHourly_Struct* QS = (QSPlayerAARateHourly_Struct*)pack->pBuffer;
	QS->charid = char_id;
	QS->add_points = aapoints - last_unspentAA;
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSDeathBy(uint32 char_id, uint32 zone_id, std::string killer_name, uint16 spell, int32 damage, uint8 killedby)
{
	std::string killedby_str = "OTHER";
	if (killedby == 1)
	{
		killedby_str = "NPC";
	}
	else if (killedby == 2)
	{
		killedby_str = "ENVIRONMENT";
	}
	else if (killedby == 3)
	{
		killedby_str = "DUEL";
	}
	else if (killedby == 4)
	{
		killedby_str = "PVP";
	}
	else if (killedby == 5)
	{
		killedby_str = "SACRIFICE";
	}
	else if (killedby == 6)
	{
		killedby_str = "CLIENT";
	}
	else if (killedby == 7)
	{
		killedby_str = "SELF/GM";
	}

	if (spell == SPELL_UNKNOWN)
	{
		spell = 0;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_killed_by_log` SET "
		"`char_id` = '%i', "
		"`zone_id` = '%i', "
		"`killed_by` = '%s', "
		"`spell` = '%i', "
		"`damage` = '%i', "
		"`type` = '%s', "
		"`time` = now()",
		char_id,
		zone_id,
		Strings::Escape(killer_name).c_str(),
		spell,
		damage,
		killedby_str.c_str());

	SendQuery(query);
}

void QueryServ::QSTSEvents(uint32 char_id, uint32 zone_id, const char results[8], uint32 recipe, uint32 tradeskill, uint16 trivial, float chance)
{
	auto pack = new ServerPacket(ServerOP_QSPlayerTSEvents, sizeof(QSPlayerTSEvents_Struct));
	QSPlayerTSEvents_Struct* QS = (QSPlayerTSEvents_Struct*)pack->pBuffer;
	QS->charid = char_id;
	QS->zone_id = zone_id;
	strncpy(QS->results, results, 8);
	QS->recipe_id = recipe;
	QS->tradeskill = tradeskill;
	QS->trivial = trivial;
	QS->chance = chance;
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSMerchantTransactions(uint32 char_id, uint32 zone_id, int16 slot_id, uint32 item_id, uint8 charges,
	uint32 merchant_id, int32 merchant_plat, int32 merchant_gold, int32 merchant_silver, int32 merchant_copper, uint16 merchant_count,
	int32 char_plat, int32 char_gold, int32 char_silver, int32 char_copper, uint16 char_count)
{
	auto pack = new ServerPacket(ServerOP_QSPlayerLogMerchantTransactions, sizeof(QSMerchantLogTransaction_Struct));
	QSMerchantLogTransaction_Struct* QS = (QSMerchantLogTransaction_Struct*)pack->pBuffer;

	QS->char_id = char_id;
	QS->char_slot = slot_id == INVALID_INDEX ? 0 : slot_id;
	QS->item_id = item_id;
	QS->charges = charges;
	QS->zone_id = zone_id;
	QS->merchant_id = merchant_id;
	QS->merchant_money.platinum = merchant_plat;
	QS->merchant_money.gold = merchant_gold;
	QS->merchant_money.silver = merchant_silver;
	QS->merchant_money.copper = merchant_copper;
	QS->merchant_count = merchant_count;
	QS->char_money.platinum = char_plat;
	QS->char_money.gold = char_gold;
	QS->char_money.silver = char_silver;
	QS->char_money.copper = char_copper;
	QS->char_count = char_count;

	if (slot_id == INVALID_INDEX) {}

	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSLootRecords(uint32 char_id, const char* corpsename, const char* type, uint32 zone_id, uint32 item_id, const char* item_name, int16 charges, int32 platinum, int32 gold, int32 silver, int32 copper)
{
	auto pack = new ServerPacket(ServerOP_QSPlayerLootRecords, sizeof(QSPlayerLootRecords_struct));
	QSPlayerLootRecords_struct* QS = (QSPlayerLootRecords_struct*)pack->pBuffer;
	QS->charid = char_id;
	strncpy(QS->corpse_name, corpsename, 64);
	strncpy(QS->type, type, 12),
	QS->zone_id = zone_id;
	QS->item_id = item_id;
	strncpy(QS->item_name, item_name, 64);
	QS->charges = charges;
	QS->money.platinum = platinum;
	QS->money.gold = gold;
	QS->money.silver = silver;
	QS->money.copper = copper;
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSItemDesyncs(uint32 char_id, const char error[512], uint32 zoneid)
{
	std::string query = StringFormat(
		"INSERT INTO `qs_player_item_desyncs_log` SET `char_id` = '%i', `error` = '%s', `time` = now(), `zone_id` = %i",
		char_id, error, zoneid);

	SendQuery(query);
}

void QueryServ::QSBazaarAudit(const char *seller, const char *buyer, const char *itemName, int itemid, int quantity, int totalCost) {

	std::string query = StringFormat("INSERT INTO `qs_trader_audit` "
                                    "(`time`, `seller`, `buyer`, `itemname`, `quantity`, `totalcost`, `itemid`) "
                                    "VALUES (NOW(), '%s', '%s', '%s', %i, %i, %i)",
                                    seller, buyer, itemName, quantity, totalCost, itemid);
	SendQuery(query);
}

void QueryServ::QSNPCKills(uint32 npcid, uint32 zoneid, uint8 type, std::list<uint32> &charids, uint32 kill_steal)
{
	std::string query = StringFormat(
		"INSERT INTO `qs_player_npc_kill_log` (`char_id`, `npc_id`, `type`, `aggro_charid`, `zone_id`, `time`) VALUES ");

	for (std::list<uint32>::iterator iter = charids.begin(); iter != charids.end(); ++iter)
	{
		query += StringFormat(
			"(%i, %i, %i, %i, %i, now()),", *iter, npcid, type, kill_steal, zoneid);
	}

	query.erase(query.size() - 1);
	SendQuery(query);
}

void QueryServ::QSCoinMove(uint32 from_id, uint32 to_id, uint32 npcid, int32 to_slot, uint32 amount, uint32 cointype)
{
	std::string type_str = "";
	if (to_slot == -1)
	{
		type_str = "DELETE";
	}
	else if (to_slot == 3)
	{
		type_str = "TRADE BUCKET";
	}
	else if (to_slot == 2)
	{
		type_str = "TO BANK";
	}
	else if (to_slot == 99)
	{
		type_str = "FROM BANK";
	}
	else if (to_slot == 100)
	{
		type_str = "SPLIT";
	}
	else if (to_slot == 0 || to_slot == 1)
	{
		type_str = "UNLOGGED REQUEST";
	}
	else
	{
		type_str = "INVALID REQUEST";
	}


	std::string coin_type_str = "COPPER";
	if (cointype == 1)
	{
		coin_type_str = "SILVER";
	}
	else if (cointype == 2)
	{
		coin_type_str = "GOLD";
	}
	else if (cointype == 3)
	{
		coin_type_str = "PLATINUM";
	}

	std::string query = StringFormat("INSERT INTO `qs_player_coin_move_log` "
		"(`time`, `from_char`, `to_char`, `to_npc`, `type`, `amount`, `coin_type`) "
		"VALUES (NOW(), %i, %i, %i, '%s', %i, '%s')",
		from_id, to_id, npcid, type_str.c_str(), amount, coin_type_str.c_str());
	SendQuery(query);
}

void QueryServ::QSGroundSpawn(uint32 characterid, int16 itemid, uint8 quantity, int16 in_bag, uint16 zoneid, bool dropped, bool forced)
{
	std::string type = dropped ? "DROPPED" : "PICKED-UP";
	if (dropped && forced)
		type = "DROPPED (FULL)";
	std::string query = StringFormat("INSERT INTO `qs_player_ground_spawns_log` "
		"(`time`, `characterid`, `itemid`, `quantity`, `bagged`, `zone`, `type`) "
		"VALUES (NOW(), %i, %i, %i, %i, %i, '%s')",
		characterid, itemid, quantity, in_bag, zoneid, type.c_str());
	SendQuery(query);
}

void QueryServ::QSTradeItems(uint32 from_id, uint32 to_id, uint32 from_slot, uint32 to_slot, int16 item_id, uint8 charges, bool bagged, bool pc_trade) 
{
	std::string type = pc_trade ? "PC TRADE" : "NPC-HANDIN";
	if (bagged && !pc_trade)
		type = "*EATEN* NPC-HANDIN";
	else if (charges > 1 && database.ItemQuantityType(item_id) == EQ::item::Quantity_Stacked && !pc_trade)
		type = "NPC-HANDIN (Extra stacked items will be lost if NPC accepts.)";
	std::string query = StringFormat("INSERT INTO `qs_player_trade_items_log` "
		"(`from_id`, `from_slot`, `to_id`, `to_slot`, `item_id`, `charges`, `bagged`, `type`, `time`) "
		"VALUES (%i, %i, %i, %i, %i, %i, %i, '%s', NOW())",
		from_id, from_slot, to_id, to_slot, item_id, charges, bagged, type.c_str());
	SendQuery(query);
}

void QueryServ::QSPlayerTrade(QSPlayerLogTrade_Struct *QS)
{
	std::string query = StringFormat(
		"INSERT INTO `qs_player_trade_log` SET "
		"`char1_id` = '%i', "
		"`char1_pp` = '%i', "
		"`char1_gp` = '%i', "
		"`char1_sp` = '%i', "
		"`char1_cp` = '%i', "
		"`char1_items` = '%i', "
		"`char2_id` = '%i', "
		"`char2_pp` = '%i', "
		"`char2_gp` = '%i', "
		"`char2_sp` = '%i', "
		"`char2_cp` = '%i', "
		"`char2_items` = '%i', "
		"`time` = NOW();",
		QS->char1_id,
		QS->char1_money.platinum,
		QS->char1_money.gold,
		QS->char1_money.silver,
		QS->char1_money.copper,
		QS->char1_count,
		QS->char2_id,
		QS->char2_money.platinum,
		QS->char2_money.gold,
		QS->char2_money.silver,
		QS->char2_money.copper,
		QS->char2_count);
	SendQuery(query);
}


void QueryServ::QSLogCommands(Client* c, const char* Command, const char* Arguments, Mob* Target)
{
	if (!Target) Target = c->GetTarget();

	const char* targetType = "notarget";
	std::string message = StringFormat("%s %s", Command, Arguments ? Arguments : "");;

	if (c->GetTarget())
	{
		if (c->GetTarget()->IsClient()) targetType = "player";
		else if (c->GetTarget()->IsPet()) targetType = "pet";
		else if (c->GetTarget()->IsNPC()) targetType = "NPC";
		else if (c->GetTarget()->IsCorpse()) targetType = "Corpse";
		database.CommandLogs(c->GetName(), c->AccountName(), c->GetY(), c->GetX(), c->GetZ(), message.c_str(), targetType, c->GetTarget()->GetName(), c->GetTarget()->GetY(), c->GetTarget()->GetX(), c->GetTarget()->GetZ(), c->GetZoneID(), zone->GetShortName());
	}
	else
	{
		database.CommandLogs(c->GetName(), c->AccountName(), c->GetY(), c->GetX(), c->GetZ(), message.c_str(), targetType, targetType, 0, 0, 0, c->GetZoneID(), zone->GetShortName());
	}
}

void QueryServ::QSHandinItems(struct QSPlayerLogHandin_Struct* QS)
{
	std::string query = StringFormat(
		"INSERT INTO `qs_player_handin_log` SET "
		"`char_id` = %i, "
		"`char_pp` = %i, "
		"`char_gp` = %i, "
		"`char_sp` = %i, "
		"`char_cp` = %i, "
		"`char_items` = %i, "
		"`npc_id` = %i, "
		"`time` = NOW(); ",
		QS->char_id,
		QS->char_money.platinum,
		QS->char_money.gold,
		QS->char_money.silver,
		QS->char_money.copper,
		QS->char_count,
		QS->npc_id);
	SendQuery(query);
}

//TODO: Need to figure out how to convert the delete reporting to this function.
void QueryServ::QSDeleteItems() {}

//TODO: Need to figure out how to convert the move reporting to this function.
void QueryServ::QSMoveItems() {}

void QueryServ::QSLogKillSteal(NPC* const npc, uint32 zoneid, Client* const client, const SInitialEngageEntry& engageEntry) {
	const auto& items = npc->GetLootItems();
	Json::Value loot;
	loot["items"] = Json::Value(Json::arrayValue);
	for (const auto& item : items) {
		loot["items"].append(item->item_id);
	}
	loot["platinum"] = npc->GetPlatinum();
	loot["gold"] = npc->GetGold();
	loot["silver"] = npc->GetSilver();
	loot["copper"] = npc->GetCopper();

  uint32 groupid = client->GetGroup() ? client->GetGroup()->GetID() : 0;
  uint32 raidid = client->GetRaid() ? client->GetRaid()->GetID() : 0;

	std::string query = StringFormat(
		"INSERT INTO `qs_player_ks_log` SET "
		"`npc_type_id` = %i, "
		"`zone_id` = %i, "
		"`killed_by_id` = %i, "
		"`killed_by_group_id` = %i, "
		"`killed_by_raid_id` = %i, "
		"`initial_engage_ids` = '%s', "
		"`npc_lootables` = '%s'; ",
		npc->GetNPCTypeID(),
		zoneid,
		client->CharacterID(),
		groupid,
		raidid,
		engageEntry.ToJson().c_str(),
		loot.toOptimizedString().c_str()
	);
	SendQuery(query);
}