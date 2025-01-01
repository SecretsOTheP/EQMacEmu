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
#include "../common/md5.h"
#include "../common/packet_dump.h"
#include "../common/packet_functions.h"
#include "../common/servertalk.h"
#include "../common/net/packet.h"

#include "database.h"
#include "queryservconfig.h"
#include "worldserver.h"
#include "../common/events/player_events.h"
#include "../common/events/player_event_logs.h"
#include <iomanip>
#include <iostream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern WorldServer           worldserver;
extern const queryservconfig* Config;
extern QSDatabase              database;

WorldServer::WorldServer()
{
}

WorldServer::~WorldServer()
{
}

void WorldServer::Connect()
{
	m_connection = std::make_unique<EQ::Net::ServertalkClient>(
		Config->WorldIP,
		Config->WorldTCPPort,
		false,
		"QueryServ",
		Config->SharedKey
	);
	m_connection->OnMessage(std::bind(&WorldServer::HandleMessage, this, std::placeholders::_1, std::placeholders::_2));
}

bool WorldServer::SendPacket(ServerPacket* pack)
{
	m_connection->SendPacket(pack);
	return true;
}
std::string WorldServer::GetIP() const
{
	return m_connection->Handle()->RemoteIP();
}

uint16 WorldServer::GetPort() const
{
	return m_connection->Handle()->RemotePort();
}

bool WorldServer::Connected() const
{
	return m_connection->Connected();
}

void WorldServer::HandleMessage(uint16 opcode, const EQ::Net::Packet& p)
{
	switch (opcode) {
		case 0: {
			break;
		}
		case ServerOP_PlayerEvent: {
			auto                         n = PlayerEvent::PlayerEventContainer{};
			auto                         s = (ServerSendPlayerEvent_Struct *)p.Data();
			EQ::Util::MemoryStreamReader ss(s->cereal_data, s->cereal_size);
			cereal::BinaryInputArchive   archive(ss);
			archive(n);

			player_event_logs.AddToQueue(n.player_event_log);

			break;
		}
		case ServerOP_KeepAlive: {
			break;
		}
		
		case ServerOP_Speech: {
			Server_Speech_Struct* SSS = (Server_Speech_Struct*)p.Data();
			std::string          tmp1 = SSS->from;
			std::string          tmp2 = SSS->to;
			database.LogPlayerSpeech(tmp1.c_str(), tmp2.c_str(), SSS->message, SSS->minstatus, SSS->guilddbid, SSS->type, SSS->characterid, SSS->groupid);
			break;
		}
		case ServerOP_QSPlayerLogItemDeletes: {
			if (p.Length() < sizeof(QSPlayerLogItemDelete_Struct)) {
				LogInfoDetail("Received malformed ServerOP_QSPlayerLogItemDeletes");
				break;
			}
			QSPlayerLogItemDelete_Struct* QS = (QSPlayerLogItemDelete_Struct*)p.Data();
			uint32 Items = QS->char_count;
			if (p.Length() < sizeof(QSPlayerLogItemDelete_Struct) * Items) {
				LogInfoDetail("Received malformed ServerOP_QSPlayerLogItemDeletes");
				break;
			}
			database.LogPlayerItemDelete(QS, Items);
			break;
		}
		case ServerOP_QSPlayerLogItemMoves: {
			QSPlayerLogItemMove_Struct* QS = (QSPlayerLogItemMove_Struct*)p.Data();
			uint32 Items = QS->char_count;
			database.LogPlayerItemMove(QS, Items);
			break;
		}

		case ServerOP_QSPlayerLogMerchantTransactions: {
			QSMerchantLogTransaction_Struct* QS = (QSMerchantLogTransaction_Struct*)p.Data();
			uint32 Items = QS->char_count + QS->merchant_count;
			database.LogMerchantTransaction(QS, Items);
			break;
		}
		case ServerOP_QSPlayerAARateHourly: {
			QSPlayerAARateHourly_Struct* QS = (QSPlayerAARateHourly_Struct*)p.Data();
			uint32 Items = QS->charid;
			database.LogPlayerAARateHourly(QS, Items);
			break;
		}
		case ServerOP_QSPlayerAAPurchase: {
			QSPlayerAAPurchase_Struct* QS = (QSPlayerAAPurchase_Struct*)p.Data();
			uint32 Items = QS->charid;
			database.LogPlayerAAPurchase(QS, Items);
			break;
		}
		case ServerOP_QSPlayerTSEvents: {
			QSPlayerTSEvents_Struct* QS = (QSPlayerTSEvents_Struct*)p.Data();
			uint32 Items = QS->charid;
			database.LogPlayerTSEvents(QS, Items);
			break;
		}
		case ServerOP_QSPlayerQGlobalUpdates: {
			QSPlayerQGlobalUpdate_Struct* QS = (QSPlayerQGlobalUpdate_Struct*)p.Data();
			uint32 Items = QS->charid;
			database.LogPlayerQGlobalUpdates(QS, Items);
			break;
		}
		case ServerOP_QSPlayerLootRecords: {
			QSPlayerLootRecords_struct* QS = (QSPlayerLootRecords_struct*)p.Data();
			uint32 Items = QS->charid;
			database.LogPlayerLootRecords(QS, Items);
			break;
		}
		case ServerOP_QueryServGeneric: {
			/*
				The purpose of ServerOP_QueryServerGeneric is so that we don't have to add code to world just to relay packets
				each time we add functionality to queryserv.

				A ServerOP_QueryServGeneric packet has the following format:

				uint32 SourceZoneID
				char OriginatingCharacterName[0]
					- Null terminated name of the character this packet came from. This could be just
					- an empty string if it has no meaning in the context of a particular packet.
				uint32 Type

				The 'Type' field is a 'sub-opcode'. A value of 0 is used for the LFGuild packets. The next feature to be added
				to queryserv would use 1, etc.

				Obviously, any fields in the packet following the 'Type' will be unique to the particular type of packet. The
				'Generic' in the name of this ServerOP code relates to the four header fields.
			*/

			auto from = p.GetCString(8);
			uint32 Type = p.GetUInt32(8 + from.length() + 1);

			switch (Type) {
				case 0: {
					break;
				}
				default: {
					LogInfoDetail("Received unhandled ServerOP_QueryServGeneric [{}]", Type);
					break;
				}
			}
			break;
		}
		case ServerOP_QSSendQuery: {
			/* Process all packets here */
			ServerPacket pack;
			pack.pBuffer = (uchar*)p.Data();
			pack.opcode = opcode;
			pack.size = (uint32)p.Length();

			database.GeneralQueryReceive(&pack);
			pack.pBuffer = nullptr;
			break;
		}
	}
}
