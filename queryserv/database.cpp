/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2008 EQEMu Development Team (http://eqemulator.net)

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
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysqld_error.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <map>
#include <vector>

// Disgrace: for windows compile
#ifdef _WINDOWS
#include <windows.h>
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else
#include "../common/unix.h"
#include <netinet/in.h>
#endif

#include "database.h"
#include "../common/eq_packet_structs.h"
#include "../common/strings.h"
#include "../common/servertalk.h"

// The client sends chat as Windows-1252. Smart quotes and dashes (0x80-0x9F) are not valid
// UTF-8 and were rejected by the database, so convert to UTF-8, keeping at most max_chars
// characters (one byte per character before conversion).
static std::string Windows1252ToUTF8(const char *text, size_t max_chars)
{
	// Unicode code points for 0x80-0x9F; 0 marks bytes Windows-1252 leaves undefined.
	static const uint16 high_controls[32] = {
		0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
		0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178
	};

	std::string out;
	const size_t length = std::min(strlen(text), max_chars);
	out.reserve(length);
	for (size_t i = 0; i < length; i++) {
		uint32 c = static_cast<unsigned char>(text[i]);
		if (c >= 0x80 && c <= 0x9F) {
			c = high_controls[c - 0x80] ? high_controls[c - 0x80] : '?';
		}

		if (c < 0x80) {
			out += static_cast<char>(c);
		}
		else if (c < 0x800) {
			out += static_cast<char>(0xC0 | (c >> 6));
			out += static_cast<char>(0x80 | (c & 0x3F));
		}
		else {
			out += static_cast<char>(0xE0 | (c >> 12));
			out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (c & 0x3F));
		}
	}
	return out;
}

void QSDatabase::LogPlayerSpeech(
	const char *from,
	const char *to,
	const char *message,
	uint16 minstatus,
	uint32 guilddbid,
	uint8 type,
	uint32 characterid,
	uint32 groupid
)
{
	// qs_player_speech.message is varchar(256); longer messages are cut rather than lost.
	const std::string utf8_message = Windows1252ToUTF8(message, 256);

	auto escapedFrom = new char[strlen(from) * 2 + 1];
	auto escapedTo = new char[strlen(to) * 2 + 1];
	auto escapedMessage = new char[utf8_message.length() * 2 + 1];
	DoEscapeString(escapedFrom, from, strlen(from));
	DoEscapeString(escapedTo, to, strlen(to));
	DoEscapeString(escapedMessage, utf8_message.c_str(), utf8_message.length());

	std::string query = StringFormat(
		"INSERT INTO `qs_player_speech` "
		"SET `from` = '%s', `to` = '%s', `message`='%s', "
		"`minstatus`='%i', `guilddbid`='%i', `type`='%i', "
		"`characterid`='%i', `groupid`='%i'",
		escapedFrom, escapedTo, escapedMessage, minstatus, guilddbid, type, characterid, groupid
	);
	safe_delete_array(escapedFrom);
	safe_delete_array(escapedTo);
	safe_delete_array(escapedMessage);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		LogInfo("Failed Speech Entry Insert: [{}]", results.ErrorMessage().c_str());
		LogInfo("[{}]", query.c_str());
	}


}

void QSDatabase::LogPlayerItemDelete(QSPlayerLogItemDelete_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

    for(uint32 i = 0; i < items; i++)
	{
		std::string query = StringFormat(
			"INSERT INTO `qs_player_item_delete_log` SET "
				"`char_id` = '%i', "
				"`char_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`stack_size` = '%i', "
				"`char_items` = '%i', "
				"`time` = now();",
				QS->char_id,
				QS->char_slot,
				QS->item_id,
				QS->charges,
				QS->stack_size,
				QS->char_count);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log(Logs::Detail, Logs::QSServer, "Failed Delete Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
		QS++;
    }
}

void QSDatabase::LogPlayerItemMove(QSPlayerLogItemMove_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

    for(uint32 i = 0; i < items; i++)
	{
        std::string query = StringFormat(
			"INSERT INTO `qs_player_item_move_log` SET "
				"`char_id` = '%i', "
				"`from_slot` = '%i', "
				"`to_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`stack_size` = '%i', "
				"`char_items` = '%i', "
				"`postaction` = '%i', "
				"`time` = now()",
				QS->char_id,
				QS->items[i].from_slot,
				QS->items[i].to_slot,
				QS->items[i].item_id,
				QS->items[i].charges,
				QS->stack_size,
				QS->char_count,
				QS->postaction);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log(Logs::Detail, Logs::QSServer, "Failed Move Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void QSDatabase::LogMerchantTransaction(QSMerchantLogTransaction_Struct* QS, uint32 items)
{
	/* Merchant transactions are from the perspective of the merchant, not the player -U */
	if (items == 0)
	{
		return;
	}

    for(uint32 i = 0; i < items; i++)
	{
		std::string query = StringFormat(
			"INSERT INTO `qs_merchant_transaction_log` SET "
				"`char_id` = '%i', "
				"`char_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`zone_id` = '%i', "
				"`merchant_id` = '%i', "
				"`merchant_pp` = '%i', "
				"`merchant_gp` = '%i', "
				"`merchant_sp` = '%i', "
				"`merchant_cp` = '%i', "
				"`merchant_items` = '%i', "
				"`char_pp` = '%i', "
				"`char_gp` = '%i', "
				"`char_sp` = '%i', "
				"`char_cp` = '%i', "
				"`char_items` = '%i', "
				"`time` = NOW()",
				QS->char_id,
				QS->char_slot,
				QS->item_id,
				QS->charges,
				QS->zone_id,
				QS->merchant_id,
				QS->merchant_money.platinum,
				QS->merchant_money.gold,
				QS->merchant_money.silver,
				QS->merchant_money.copper,
				QS->merchant_count,
				QS->char_money.platinum,
				QS->char_money.gold,
				QS->char_money.silver,
				QS->char_money.copper,
				QS->char_count);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log(Logs::Detail, Logs::QSServer, "Failed Transaction Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void QSDatabase::LogPlayerAARateHourly(QSPlayerAARateHourly_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_aa_rate_hourly` (char_id, aa_count, hour_time) "
			"VALUES "
			"(%i, %i, UNIX_TIMESTAMP() - MOD(UNIX_TIMESTAMP(), 3600)) "
			"ON DUPLICATE KEY UPDATE "
			"`aa_count` = `aa_count` + %i",
			QS->charid,
			QS->add_points,
			QS->add_points);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::Detail, Logs::QSServer, "Failed AA Rate Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void QSDatabase::LogPlayerAAPurchase(QSPlayerAAPurchase_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_aa_purchase_log` SET "
			"`char_id` = '%i', "
			"`aa_type` = '%s', "
			"`aa_name` = '%s', "
			"`aa_id` = '%i', "
			"`aa_cost` = '%i', "
			"`zone_id` = '%i', "
			"`time` = now()",
			QS->charid,
			QS->aatype,
			QS->aaname,
			QS->aaid,
			QS->cost,
			QS->zone_id);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::Detail, Logs::QSServer, "Failed AA Purchase Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void QSDatabase::LogPlayerTSEvents(QSPlayerTSEvents_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_ts_event_log` SET "
			"`char_id` = '%i', "
			"`zone_id` = '%i', "
			"`results` = '%s', "
			"`recipe_id` = '%i', "
			"`tradeskill` = '%i', "
			"`trivial` = '%i', "
			"`chance` = '%f', "
			"`time` = now()",
			QS->charid,
			QS->zone_id,
			QS->results,
			QS->recipe_id,
			QS->tradeskill,
			QS->trivial,
			QS->chance);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::Detail, Logs::QSServer, "Failed TS Event Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void QSDatabase::LogPlayerQGlobalUpdates(QSPlayerQGlobalUpdate_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_qglobal_updates_log` SET "
			"`char_id` = '%i', "
			"`action` = '%s', "
			"`zone_id` = '%i', "
			"`varname` = '%s', "
			"`newvalue` = '%s', "
			"`time` = now()",
			QS->charid,
			QS->action,
			QS->zone_id,
			QS->varname,
			QS->newvalue);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::Detail, Logs::QSServer, "Failed QGlobal Update Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void QSDatabase::LogPlayerLootRecords(QSPlayerLootRecords_struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	Log(Logs::General, Logs::QSServer, "Inserting loot record");
	std::string query = StringFormat(
		"INSERT INTO `qs_player_loot_records_log` SET "
		"`char_id` = '%i', "
		"`corpse_name` = '%s', "
		"`type` = '%s', "
		"`zone_id` = '%i', "
		"`item_id` = '%i', "
		"`item_name` = '%s', "
		"`charges` = '%i', "
		"`platinum` = '%i', "
		"`gold` = '%i', "
		"`silver` = '%i', "
		"`copper` = '%i', "
		"`time` = now()",
		QS->charid,
		Strings::Escape(QS->corpse_name).c_str(),
		QS->type,
		QS->zone_id,
		QS->item_id,
		Strings::Escape(QS->item_name).c_str(),
		QS->charges,
		QS->money.platinum,
		QS->money.gold,
		QS->money.silver,
		QS->money.copper);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::General, Logs::QSServer, "Failed Loot Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void QSDatabase::GeneralQueryReceive(ServerPacket *pack)
{
	/*
	 * These are general queries passed from anywhere in zone instead of packing structures and breaking them down again and again
	 */
	auto queryBuffer = new char[pack->ReadUInt32() + 1];
	pack->ReadString(queryBuffer);

	std::string query(queryBuffer);
	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::Detail, Logs::QSServer, "Failed Delete Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
	safe_delete_array(queryBuffer);
}
