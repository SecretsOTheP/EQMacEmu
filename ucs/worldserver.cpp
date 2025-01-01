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
#include "../common/servertalk.h"
#include "../common/misc_functions.h"
#include "../common/packet_functions.h"
#include "../common/md5.h"
#include "../common/json_config.h"
#include "worldserver.h"
#include "clientlist.h"
#include "ucsconfig.h"
#include "database.h"
#include "../common/discord/discord_manager.h"
#include "../common/events/player_event_logs.h"

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <iomanip>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>

extern WorldServer worldserver;
extern Clientlist *g_Clientlist;
extern const ucsconfig *Config;
extern UCSDatabase database;
extern DiscordManager  discord_manager;
extern PathManager path;

WorldServerList::WorldServerList()
{
	// main worldserver
	WorldServerConfig *config = new WorldServerConfig();
	config->WorldShortName = Config->ShortName;
	config->WorldIP = Config->WorldIP;
	config->WorldTCPPort = Config->WorldTCPPort;
	config->SharedKey = Config->SharedKey;
	config->DatabaseHost = Config->DatabaseHost;
	config->DatabaseUsername = Config->DatabaseUsername;
	config->DatabasePassword = Config->DatabasePassword;
	config->DatabaseDB = Config->DatabaseDB;
	config->DatabasePort = Config->DatabasePort;
	LogInfo("Configured primary world server {0} at {1}:{2} with database at {3}:{4}", config->WorldShortName, config->WorldIP, config->WorldTCPPort, config->DatabaseHost, config->DatabasePort);
	WorldServer *worldserver = new WorldServer(config);
	m_worldservers[config->WorldShortName] = worldserver;
	m_mainshortname = config->WorldShortName;

	// additional worldservers
	char str[50];
	int worlds = 0;
	auto jsconf = EQ::JsonConfigFile::Load(
		fmt::format("{}/ucs_multiserver.json", path.GetServerPath())
	);
	auto c = jsconf.RawHandle();
	Json::Value *ucs = nullptr;;
	if (c.isMember("server"))
	{
		if (c["server"].isMember("ucs"))
			ucs = &c["server"]["ucs"];
		else if (c["server"].isMember("chatserver"))
			ucs = &c["server"]["chatserver"];
	}
	if (ucs != nullptr)
	{
		do
		{
			sprintf(str, "addworld%i", ++worlds);
			if (ucs->isMember(str))
			{
				auto wc = (*ucs)[str];

				config = new WorldServerConfig();
				config->WorldShortName = wc["shortname"].asString();
				config->WorldIP = wc["worldip"].asString();
				config->WorldTCPPort = Strings::ToUnsignedInt(wc["worldtcpport"].asString());
				config->SharedKey = wc["sharedkey"].asString();
				if (wc.isMember("database"))
				{
					auto db = wc["database"];
					config->DatabaseHost = db["host"].asString();
					config->DatabaseUsername = db["username"].asString();
					config->DatabasePassword = db["password"].asString();;
					config->DatabaseDB = db["db"].asString();
					config->DatabasePort = Strings::ToUnsignedInt(db["port"].asString());

					LogInfo("Configured additional world server {0} at {1}:{2} with database at {3}:{4}", config->WorldShortName, config->WorldIP, config->WorldTCPPort, config->DatabaseHost, config->DatabasePort);
					worldserver = new WorldServer(config);
					m_worldservers[config->WorldShortName] = worldserver;
				}
			}
			else
				break;
		} while (worlds < 100);
	}
}

void WorldServerList::Process()
{
	for (auto const &[WorldShortName, worldserver] : m_worldservers)
	{
		worldserver->PingUCSDatabase();
	}
}

WorldServer::WorldServer(WorldServerConfig *wsconfig)
{
	m_config = wsconfig;

	m_connection = std::make_unique<EQ::Net::ServertalkClient>(m_config->WorldIP, m_config->WorldTCPPort, false, "UCS", m_config->SharedKey);
	m_connection->OnMessage(std::bind(&WorldServer::ProcessMessage, this, std::placeholders::_1, std::placeholders::_2));

	if (!m_database.Connect(
		wsconfig->DatabaseHost.c_str(),
		wsconfig->DatabaseUsername.c_str(),
		wsconfig->DatabasePassword.c_str(),
		wsconfig->DatabaseDB.c_str(),
		wsconfig->DatabasePort)) {
		LogInfo("World server {0} database connection failed.", m_config->WorldShortName);
		// TODO mark this connection bad but go on with the other ones
	}

	m_dbping_timer.Start(INTERSERVER_TIMER);
}

WorldServer::~WorldServer()
{
}

void WorldServer::ProcessMessage(uint16 opcode, EQ::Net::Packet &p) {
	ServerPacket tpack(opcode, p);
	ServerPacket *pack = &tpack;

	LogNetcode("Received Opcode: {:#04x}", opcode);

	switch(opcode) {
		case 0: {
			break;
		}
		case ServerOP_KeepAlive: {
			break;
		}
		case ServerOP_ReloadLogs: {
			LogSys.LoadLogDatabaseSettings();
			player_event_logs.ReloadSettings();
			break;
		}
		case ServerOP_PlayerEvent: {
			auto n = PlayerEvent::PlayerEventContainer{};
			auto s = (ServerSendPlayerEvent_Struct *)pack->pBuffer;
			EQ::Util::MemoryStreamReader ss(s->cereal_data, s->cereal_size);
			cereal::BinaryInputArchive archive(ss);
			archive(n);

			discord_manager.QueuePlayerEventMessage(n);

			break;
		}		
		case ServerOP_DiscordWebhookMessage: {
			auto* q = (DiscordWebhookMessage_Struct*)p.Data();

			discord_manager.QueueWebhookMessage(
				q->webhook_id,
				q->message
			);

			break;
		}
		case ServerOP_UCSMessage: {
			char *Buffer = (char *)pack->pBuffer;

			auto From = new char[strlen(Buffer) + 1];

			VARSTRUCT_DECODE_STRING(From, Buffer);

			std::string Message = Buffer;

			LogInfo("Player: [{0}], Sent Message: [{1}]", From, Message.c_str());

			Client *c = g_Clientlist->FindCharacter(From);

			safe_delete_array(From);

			if (Message.length() < 2) {
				break;
			}

			if (!c) {
				LogInfo("Client not found");
				break;
			}

			if (Message[0] == ';') {
				c->SendChannelMessageByNumber(Message.substr(1, std::string::npos));
			}
			else if (Message[0] == '[') {
				g_Clientlist->ProcessOPChatCommand(c, Message.substr(1, std::string::npos));
			}

			break;
		}
	}
}

void WorldServer::PingUCSDatabase()
{
	if (m_dbping_timer.Check())
		m_database.ping();
}