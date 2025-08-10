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
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <iomanip>
#include <stdarg.h>

#ifdef _WINDOWS
	#include <process.h>

	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#endif

#include "../common/eq_packet_structs.h"
#include "../common/misc_functions.h"
#include "../common/rulesys.h"
#include "../common/say_link.h"
#include "../common/servertalk.h"
#include "../common/profanity_manager.h"

#include "client.h"
#include "command.h"
#include "corpse.h"
#include "entity.h"
#include "quest_parser_collection.h"
#include "guild_mgr.h"
#include "mob.h"
#include "petitions.h"
#include "raids.h"
#include "string_ids.h"
#include "titles.h"
#include "worldserver.h"
#include "zone.h"
#include "zone_config.h"
#include "queryserv.h"
#include "../common/patches/patches.h"
#include "../common/skill_caps.h"
#include "queryserv.h"
#include "../common/events/player_event_logs.h"

extern EntityList entity_list;
extern Zone* zone;
extern volatile bool is_zone_loaded;
extern void Shutdown();
extern WorldServer worldserver;
extern PetitionList petition_list;
extern uint32 numclients;
extern QuestParserCollection* parse;

WorldServer::WorldServer()
{
	cur_groupid = 0;
	last_groupid = 0;
	oocmuted = false;
}

WorldServer::~WorldServer() {
}

void WorldServer::Connect()
{
	m_connection = std::make_unique<EQ::Net::ServertalkClient>(Config->WorldIP, Config->WorldTCPPort, false, "Zone", Config->SharedKey);
	m_connection->OnConnect([this](EQ::Net::ServertalkClient* client) {
		OnConnected();
	});

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

void WorldServer::SetZoneData(uint32 iZoneID, uint32 iZoneGuildID) {
	auto pack = new ServerPacket(ServerOP_SetZone, sizeof(SetZone_Struct));
	SetZone_Struct* szs = (SetZone_Struct*) pack->pBuffer;

	if (iZoneGuildID == 0)
	{
		iZoneGuildID = 0xFFFFFFFF;
	}
	szs->zoneid = iZoneID;
	szs->zoneguildid = iZoneGuildID;
	if (zone) {
		szs->staticzone = zone->IsStaticZone();
	}
	SendPacket(pack);
	safe_delete(pack);
}

void WorldServer::OnConnected() {
	ServerPacket* pack;

	/* Tell the launcher what our information is */
	pack = new ServerPacket(ServerOP_SetLaunchName,sizeof(LaunchName_Struct));
	LaunchName_Struct* ln = (LaunchName_Struct*)pack->pBuffer;
	strn0cpy(ln->launcher_name, m_launcherName.c_str(), 32);
	strn0cpy(ln->zone_name, m_launchedName.c_str(), 16);
	SendPacket(pack);
	safe_delete(pack);

	/* Tell the Worldserver basic information about this zone process */
	pack = new ServerPacket(ServerOP_SetConnectInfo, sizeof(ServerConnectInfo));
	ServerConnectInfo* sci = (ServerConnectInfo*) pack->pBuffer;

	auto config = ZoneConfig::get();
	sci->port = ZoneConfig::get()->ZonePort;
	if(config->WorldAddress.length() > 0) {
		strn0cpy(sci->address, config->WorldAddress.c_str(), 250);
	}
	if(config->LocalAddress.length() > 0) {
		strn0cpy(sci->local_address, config->LocalAddress.c_str(), 250);
	}

	/* Fetch process ID */
	if (getpid()) {
		sci->process_id = getpid();
	}
	else {
		sci->process_id = 0;
	}

	SendPacket(pack);
	safe_delete(pack);

	if (is_zone_loaded) {

		SetZoneData(zone->GetZoneID(), zone->GetGuildID());
		entity_list.UpdateWho(true);
		/*SendEmoteMessage(
			0,
			0,
			Chat::Yellow,
			fmt::format(
				"Zone Connected | {}",
				zone->GetZoneDescription()
			).c_str()
		);*/
		zone->GetTimeSync();
	}
	else {
		SetZoneData(0, GUILD_NONE);
	}

	pack = new ServerPacket(ServerOP_LSZoneBoot,sizeof(ZoneBoot_Struct));
	ZoneBoot_Struct* zbs = (ZoneBoot_Struct*)pack->pBuffer;
	strcpy(zbs->compile_time,LAST_MODIFIED);
	SendPacket(pack);
	safe_delete(pack);
}

/* Zone Process Packets from World */
void WorldServer::HandleMessage(uint16 opcode, const EQ::Net::Packet& p)
{
	ServerPacket tpack(opcode, p);
	ServerPacket* pack = &tpack;

	switch(opcode) {
		case 0:
		case ServerOP_KeepAlive: {
			break;
		}
		// World is tellins us what port to use.
		case ServerOP_SetConnectInfo: {
			if (pack->size != sizeof(ServerConnectInfo)) {
				break;
			}

			ServerConnectInfo* sci = (ServerConnectInfo*) pack->pBuffer;

			if (sci->port == 0) {
				LogCritical("World did not have a port to assign from this server, the port range was not large enough.");
				Shutdown();
			}
			else {
				LogInfo("World assigned Port: [{}] for this zone", sci->port);
				ZoneConfig::SetZonePort(sci->port);

				LogSys.SetDiscordHandler(&Zone::DiscordWebhookMessageHandler);
			}

			if (is_zone_loaded) {
				SendEmoteMessage(
					0,
					0,
					Chat::Yellow,
					fmt::format(
						"Zone Connected : {}",
						ZoneName(zone->GetZoneID())
					).c_str()
				);
			}

			break;
		}
		case ServerOP_ChannelMessage: {
			if (!is_zone_loaded) {
				break;
			}

			ServerChannelMessage_Struct* scm = (ServerChannelMessage_Struct*) pack->pBuffer;
			if (scm->deliverto[0] == 0) {
				// this is a non directed message

				entity_list.ChannelMessageFromWorld(scm->from, scm->to, scm->chan_num, scm->guilddbid, scm->language, scm->lang_skill, scm->message);
			} 
			else {
				// there's a deliverto so it's directed to just one client - could be a tell or a queued global channel message

				// locate target client by name
				Client* client = entity_list.GetClientByName(scm->deliverto);

				if (client && client->Connected()) {
					if (scm->chan_num == ChatChannel_TellEcho) {
						if (scm->queued == 1) { // tell was queued
							client->Tell_StringID(StringID::QUEUED_TELL, scm->to, scm->message);
						}
						else if (scm->queued == 2) { // tell queue was full
							client->Tell_StringID(StringID::QUEUE_TELL_FULL, scm->to, scm->message);
						}
						else if (scm->queued == 3) { // person was offline
							client->Message_StringID(Chat::EchoTell, StringID::TOLD_NOT_ONLINE, scm->to);
						}
						else { // normal tell echo "You told Soanso, 'something'"
							// tell echo doesn't use language, so it looks normal to you even if nobody can understand your tells
							client->ChannelMessageSend(scm->from, scm->to, scm->chan_num, 0, 100, scm->message);
						}
					}
					else if (scm->chan_num == ChatChannel_Tell) {
						client->ChannelMessageSend(scm->from, scm->to, scm->chan_num, scm->language, scm->lang_skill, scm->message);
						if (scm->queued == 0) { // this is not a queued tell
							// if it's a tell, echo back to acknowledge it and make it show on the sender's client
							scm->chan_num = ChatChannel_TellEcho;
							memset(scm->deliverto, 0, sizeof(scm->deliverto));
							strcpy(scm->deliverto, scm->from);
							SendPacket(pack);
						}
					}
					else {
						client->ChannelMessageSend(scm->from, scm->to, scm->chan_num, scm->language, scm->lang_skill, scm->message);
					}
				}
			}
			break;
		}
		case ServerOP_SpawnCondition: {
			if (pack->size != sizeof(ServerSpawnCondition_Struct)) {
				break;
			}

			if (!is_zone_loaded) {
				break;
			}

			ServerSpawnCondition_Struct* ssc = (ServerSpawnCondition_Struct*) pack->pBuffer;

			zone->spawn_conditions.SetCondition(zone->GetShortName(), zone->GetGuildID(), ssc->condition_id, ssc->value, true);
			break;
		}
		case ServerOP_SpawnEvent: {
			if (pack->size != sizeof(ServerSpawnEvent_Struct)) {
				break;
			}

			if (!is_zone_loaded) {
				break;
			}

			ServerSpawnEvent_Struct* sse = (ServerSpawnEvent_Struct*) pack->pBuffer;

			zone->spawn_conditions.ReloadEvent(sse->event_id);

			break;
		}
		case ServerOP_AcceptWorldEntrance: {
			if (pack->size != sizeof(WorldToZone_Struct)) {
				break;
			}

			if (!is_zone_loaded) {
				break;
			}

			WorldToZone_Struct* wtz = (WorldToZone_Struct*) pack->pBuffer;

			if (zone->GetMaxClients() != 0 && numclients >= zone->GetMaxClients() && !RuleB(Quarm, AllowBypassMaxClientsOnWorldEnter)) {
				wtz->response = -1;
			}
			else {
				wtz->response = 1;
			}

			SendPacket(pack);
			break;
		}
		case ServerOP_ZoneToZoneRequest: {
			if (pack->size != sizeof(ZoneToZone_Struct)) {
				break;
			}

			if (!is_zone_loaded) {
				break;
			}

			ZoneToZone_Struct* ztz = (ZoneToZone_Struct*) pack->pBuffer;

			if(ztz->current_zone_id == zone->GetZoneID() && ztz->current_zone_guild_id == zone->GetGuildID()) {
				// it's a response
				Entity* entity = entity_list.GetClientByName(ztz->name);
				if (entity == 0) {
					break;
				}

				auto outapp = new EQApplicationPacket(OP_ZoneChange,sizeof(ZoneChange_Struct));
				ZoneChange_Struct* zc2=(ZoneChange_Struct*)outapp->pBuffer;
				if(ztz->response <= 0) {
					zc2->success = ZoningMessage::ZoneNotReady;
					entity->CastToMob()->SetZone(ztz->current_zone_id, ztz->current_zone_guild_id);
					entity->CastToClient()->SetZoning(false);
					entity->CastToClient()->SetLockSavePosition(false);
				}
				else {
					entity->CastToClient()->UpdateWho(1);
					strn0cpy(zc2->char_name,entity->CastToMob()->GetName(),64);
					zc2->zoneID=ztz->requested_zone_id == zone->GetZoneID() ? 185 : ztz->requested_zone_id;
					zc2->success = 1;

					entity->CastToMob()->SetZone(ztz->requested_zone_id, ztz->requested_zone_guild_id);

					if(ztz->ignorerestrictions == 3){
						entity->CastToClient()->GoToSafeCoords(ztz->requested_zone_id, ztz->requested_zone_guild_id);
					}
				}
				outapp->priority = 6;
				entity->CastToClient()->QueuePacket(outapp, true, Mob::ZONING);
				safe_delete(outapp);
				if (ztz->response <= 0) {
					entity->CastToClient()->Reconnect();
				}
				else {
					entity->CastToClient()->PreDisconnect();
				}
				switch(ztz->response) {
					case -2: {
						entity->CastToClient()->Message(Chat::Red,"You do not own the required locations to enter this zone.");
						break;
					}
					case -1: {
						entity->CastToClient()->Message(Chat::Red,"The zone is currently full, please try again later.");
						break;
					}
					case 0:	{
						entity->CastToClient()->Message(Chat::Red,"All zone servers are taken at this time, please try again later.");
						break;
					}
				}
			}
			else {
				// it's a request
				ztz->response = 0;

				if(zone->GetMaxClients() != 0 && numclients >= zone->GetMaxClients())
					ztz->response = -1;
				else {
					ztz->response = 1;
				}

				SendPacket(pack);
				break;
			}
			break;
		}
		case ServerOP_WhoAllReply:{
			if(!is_zone_loaded)
				break;


			WhoAllReturnStruct* wars= (WhoAllReturnStruct*)pack->pBuffer;
			if (wars && wars->id!=0 && wars->id<0xFFFFFFFF){
				Client* client = entity_list.GetClientByID(wars->id);
				if (client) {
					if (pack->size == 58) {//no results
						client->Message_StringID(Chat::White, StringID::WHOALL_NO_RESULTS);
					}
					else {
						auto outapp = new EQApplicationPacket(OP_WhoAllResponse, pack->size);
						memcpy(outapp->pBuffer, pack->pBuffer, pack->size);
						client->QueuePacket(outapp);
						safe_delete(outapp);
					}
				}
				else {
					LogDebug("[CLIENT] id=[{}], playerineqstring=[{}], playersinzonestring=[{}]. Dumping WhoAllReturnStruct:",
						wars->id, wars->playerineqstring, wars->playersinzonestring);
				}
			}
			else {
				LogError("WhoAllReturnStruct: Could not get return struct!");
			}
			break;
		}
		case ServerOP_EmoteMessage: {
			if (!is_zone_loaded)
				break;
			ServerEmoteMessage_Struct* sem = (ServerEmoteMessage_Struct*) pack->pBuffer;
			if (sem->to[0] != 0) {
				if (strcasecmp(sem->to, zone->GetShortName()) == 0) {
					entity_list.MessageStatus(sem->guilddbid, sem->minstatus, sem->type, (char*)sem->message);
				}
				else {
					Client* client = entity_list.GetClientByName(sem->to);
					if (client != 0){
						char* newmessage=0;
						if (strstr(sem->message, "^") == 0) {
							client->Message(sem->type, (char*)sem->message);
						}
						else{
							for (newmessage = strtok((char*)sem->message, "^"); newmessage != nullptr; newmessage = strtok(nullptr, "^")) {
								client->Message(sem->type, newmessage);
							}
						}
					}
				}
			}
			else{
				char* newmessage=0;
				if (strstr(sem->message, "^") == 0) {
					entity_list.MessageStatus(sem->guilddbid, sem->minstatus, sem->type, sem->message);
				}
				else{
					for (newmessage = strtok((char*)sem->message, "^"); newmessage != nullptr; newmessage = strtok(nullptr, "^")) {
						entity_list.MessageStatus(sem->guilddbid, sem->minstatus, sem->type, newmessage);
					}
				}
			}
			break;
		}
		case ServerOP_Motd: {
			if (pack->size != sizeof(ServerMotd_Struct)) {
				break;
			}

			ServerMotd_Struct* smotd = (ServerMotd_Struct*) pack->pBuffer;

			auto outapp = new EQApplicationPacket(OP_MOTD);
			char tmp[512] = {0};
			sprintf(tmp, "%s", smotd->motd);

			outapp->size = strlen(tmp)+1;
			outapp->pBuffer = new uchar[outapp->size];
			memset(outapp->pBuffer,0,outapp->size);
			strcpy((char*)outapp->pBuffer, tmp);

			entity_list.QueueClients(0, outapp);
			safe_delete(outapp);

			break;
		}
		case ServerOP_ShutdownAll: {
			entity_list.Save();
			Shutdown();
			break;
		}
		case ServerOP_ZoneShutdown: {
			if (pack->size != sizeof(ServerZoneStateChange_struct)) {
				LogError("Wrong size on ServerOP_ZoneShutdown. Got: [{}] Expected: [{}]", pack->size, sizeof(ServerZoneStateChange_struct));
				break;
			}
			// Annouce the change to the world
			if (!is_zone_loaded) {
				SetZoneData(0, GUILD_NONE);
			}
			else {
				SendEmoteMessage(
					0,
					0,
					Chat::Yellow,
					fmt::format(
						"Zone Shutdown | {}",
						zone->GetZoneDescription()
					).c_str()
				);

				ServerZoneStateChange_struct* zst = (ServerZoneStateChange_struct *) pack->pBuffer;
				LogInfo("Zone shutdown by {}.", zst->adminname);
				Zone::Shutdown();
			}
			break;
		}
		case ServerOP_ZoneBootup: {
			if (pack->size != sizeof(ServerZoneStateChange_struct)) {
				LogError("Wrong size on ServerOP_ZoneBootup. Got: [{}] Expected: [{}]", pack->size, sizeof(ServerZoneStateChange_struct));
				break;
			}

			auto* s = (ServerZoneStateChange_struct *) pack->pBuffer;
			if (is_zone_loaded) {
				SetZoneData(zone->GetZoneID(), zone->GetGuildID());
				if (s->zoneid != zone->GetZoneID()) {
					SendEmoteMessage(
						s->adminname,
						0,
						Chat::White,
						fmt::format(
							"Zone Bootup Failed | {} Already running",
							zone->GetZoneDescription()
						).c_str()
					);
				}
				break;
			}

			if (s->adminname[0] != 0) {
				LogInfo("Zone bootup by {}.", s->adminname);
			}

			Zone::Bootup(s->zoneid, s->makestatic, s->ZoneServerGuildID);
			break;
		}
		case ServerOP_ZoneIncClient: {
			if (pack->size != sizeof(ServerZoneIncomingClient_Struct)) {
				std::cout << "Wrong size on ServerOP_ZoneIncClient. Got: " << pack->size << ", Expected: " << sizeof(ServerZoneIncomingClient_Struct) << std::endl;
				break;
			}
			ServerZoneIncomingClient_Struct* szic = (ServerZoneIncomingClient_Struct*) pack->pBuffer;
			if (is_zone_loaded) {
					SetZoneData(zone->GetZoneID(), zone->GetGuildID());

				if (szic->zoneid == zone->GetZoneID()) {
					auto client = entity_list.GetClientByLSID(szic->lsid);
					if (client) {
						client->Kick("Dropped by world CLE subsystem");
						client->Save();
					}

					zone->RemoveAuth(szic->lsid);
					zone->AddAuth(szic);
				}
			}
			else {
				if ((Zone::Bootup(szic->zoneid, 0, szic->zoneguildid))) {
					zone->AddAuth(szic);
				}
			}

			break;
		}
		case ServerOP_ZonePlayer: {
			ServerZonePlayer_Struct* szp = (ServerZonePlayer_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(szp->name);
			Log(Logs::Detail, Logs::Status, "Zoning %s to %s(%u)\n", client != nullptr ? client->GetCleanName() : "Unknown", szp->zone, ZoneID(szp->zone));
			if (client != 0) {
				if (strcasecmp(szp->adminname, szp->name) == 0)
					client->Message(Chat::White, "Zoning to: %s", szp->zone);
				//If #hideme is on, prevent being summoned by a lower GM.
				else if (client->GetAnon() == 1 && client->Admin() > szp->adminrank)
				{
					client->Message(Chat::Red, "%s's attempt to summon you was prevented due to lack of status.", szp->adminname);
					SendEmoteMessage(szp->adminname, 0, Chat::Red, "You cannot summon a GM with a higher status than you.", szp->name);
					break;
				}
				else {
					SendEmoteMessage(szp->adminname, 0, 0, "Summoning %s to %s %1.1f, %1.1f, %1.1f", szp->name, szp->zone, szp->x_pos, szp->y_pos, szp->z_pos);
				}
				client->MovePCGuildID(ZoneID(szp->zone), szp->zoneguildid, szp->x_pos, szp->y_pos, szp->z_pos, client->GetHeading(), szp->ignorerestrictions, GMSummon);
			}
			break;
		}
		case ServerOP_KickPlayer: {
			ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(skp->name);
			if (client != 0) {
				if (skp->adminrank >= client->Admin()) {
					client->WorldKick();
					if (is_zone_loaded) {
						SendEmoteMessage(skp->adminname, 0, 0, "Remote Kick: %s booted in zone %s.", skp->name, zone->GetShortName());
					}
					else {
						SendEmoteMessage(skp->adminname, 0, 0, "Remote Kick: %s booted.", skp->name);
					}
				}
				else if (client->GetAnon() != 1) {
					SendEmoteMessage(skp->adminname, 0, 0, "Remote Kick: Your avatar level is not high enough to kick %s", skp->name);
				}
			}
			break;
		}

		case ServerOP_KickPlayerAccount: {
			ServerKickPlayerAccount_Struct* skp = (ServerKickPlayerAccount_Struct*)pack->pBuffer;
			Client* client = entity_list.GetClientByAccID(skp->AccountID);
			if (client != nullptr) {
				client->WorldKick();
			}
			break;
		}

		case ServerOP_KillPlayer: {
			ServerKillPlayer_Struct* skp = (ServerKillPlayer_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(skp->target);
			if (client != 0) {
				if (skp->admin >= client->Admin()) {
					client->GMKill();
					if (is_zone_loaded) {
						SendEmoteMessage(skp->gmname, 0, 0, "Remote Kill: %s killed in zone %s.", skp->target, zone->GetShortName());
					}
					else {
						SendEmoteMessage(skp->gmname, 0, 0, "Remote Kill: %s killed.", skp->target);
					}
				}
				else if (client->GetAnon() != 1) {
					SendEmoteMessage(skp->gmname, 0, 0, "Remote Kill: Your avatar level is not high enough to kill %s", skp->target);
				}
			}
			break;
		}
		//hand all the guild related packets to the guild manager for processing.
		case ServerOP_OnlineGuildMembersResponse:
		case ServerOP_RefreshGuild:
		case ServerOP_DeleteGuild:
		case ServerOP_GuildCharRefresh:
		case ServerOP_GuildRankUpdate: {

			guild_mgr.ProcessWorldPacket(pack);
			break;
		}
		case ServerOP_FlagUpdate: {
			Client* client = entity_list.GetClientByAccID(*((uint32*) pack->pBuffer));
			if (client != 0) {
				client->UpdateAdmin();
			}

			break;
		}
		case ServerOP_GMGoto: {
			if (pack->size != sizeof(ServerGMGoto_Struct)) {
				std::cout << "Wrong size on ServerOP_GMGoto. Got: " << pack->size << ", Expected: " << sizeof(ServerGMGoto_Struct) << std::endl;
				break;
			}
			if (!is_zone_loaded) {
				break;
			}

			ServerGMGoto_Struct* gmg = (ServerGMGoto_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(gmg->gotoname);
			if (client != 0) {
				SendEmoteMessage(gmg->myname, 0, 13, "Summoning you to: %s @ %s, %1.1f, %1.1f, %1.1f", client->GetName(), zone->GetShortName(), client->GetX(), client->GetY(), client->GetZ());
				auto outpack = new ServerPacket(ServerOP_ZonePlayer, sizeof(ServerZonePlayer_Struct));
				ServerZonePlayer_Struct* szp = (ServerZonePlayer_Struct*) outpack->pBuffer;
				strcpy(szp->adminname, gmg->myname);
				strcpy(szp->name, gmg->myname);
				strcpy(szp->zone, zone->GetShortName());
				szp->x_pos = client->GetX();
				szp->y_pos = client->GetY();
				szp->z_pos = client->GetZ();
				szp->zoneguildid = gmg->guildinstanceid;
				SendPacket(outpack);
				safe_delete(outpack);
			}
			else {
				SendEmoteMessage(gmg->myname, 0, 13, "Error: %s not found", gmg->gotoname);
			}
			break;
		}
		case ServerOP_MultiLineMsg: {
			ServerMultiLineMsg_Struct* mlm = (ServerMultiLineMsg_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(mlm->to);
			if (client) {
				auto outapp = new EQApplicationPacket(OP_MultiLineMsg, strlen(mlm->message));
				strcpy((char*) outapp->pBuffer, mlm->message);
				client->QueuePacket(outapp);
				safe_delete(outapp);
			}
			break;
		}
		case ServerOP_Uptime: {
			if (pack->size != sizeof(ServerUptime_Struct)) {
				std::cout << "Wrong size on ServerOP_Uptime. Got: " << pack->size << ", Expected: " << sizeof(ServerUptime_Struct) << std::endl;
				break;
			}

			ServerUptime_Struct* sus = (ServerUptime_Struct*) pack->pBuffer;
			uint32 ms = Timer::GetCurrentTime();
			std::string time_string = Strings::MillisecondsToTime(ms);
			SendEmoteMessage(sus->adminname, 0, Chat::White, fmt::format("Zoneserver {} | Uptime: {}", sus->zoneserverid, time_string).c_str());

			break;
		}
		case ServerOP_Petition: {
			std::cout << "Got Server Requested Petition List Refresh" << std::endl;
			ServerPetitionUpdate_Struct* sus = (ServerPetitionUpdate_Struct*) pack->pBuffer;
			// solar: this was typoed to = instead of ==, not that it acts any different now though..
			if (sus->status == 0) {
				petition_list.ReadDatabase();
			}
			else if (sus->status == 1) {
				petition_list.ReadDatabase(); // Until I fix this to be better....
			}

			break;
		}
		case ServerOP_RezzPlayer: {
			RezzPlayer_Struct* srs = (RezzPlayer_Struct*) pack->pBuffer;
			if (srs->rezzopcode == OP_RezzRequest)
			{
				// The Rezz request has arrived in the zone the player to be rezzed is currently in,
				// so we send the request to their client which will bring up the confirmation box.
				Client* client = entity_list.GetClientByName(srs->rez.your_name);
				if (client && client->CharacterID() == srs->corpse_character_id)
				{
					if(client->IsRezzPending())
					{
						auto Response = new ServerPacket(ServerOP_RezzPlayerReject,
										 strlen(srs->rez.rezzer_name) + 1);

						char *Buffer = (char *)Response->pBuffer;
						sprintf(Buffer, "%s", srs->rez.rezzer_name);
						worldserver.SendPacket(Response);
						safe_delete(Response);
						break;
					}
					//pendingrezexp is the amount of XP on the corpse. Setting it to a value >= 0
					//also serves to inform Client::OPRezzAnswer to expect a packet.
					client->SetPendingRezzData(srs->corpse_zone_id, srs->corpse_zone_guild_id, srs->exp, srs->dbid, srs->rez.spellid, srs->rez.corpse_name);
							Log(Logs::Detail, Logs::Spells, "OP_RezzRequest in zone %s for %s, spellid:%i",
							zone->GetShortName(), client->GetName(), srs->rez.spellid);

					if (zone->GetGuildID() == 1)
					{
						client->OPRezzAnswer(1, srs->rez.spellid, srs->corpse_zone_guild_id, 0, srs->rez.x, srs->rez.y, srs->rez.z);
						Mob* mypet = client->GetPet();
						if (mypet)
						{
							if (mypet->IsCharmedPet())
								client->FadePetCharmBuff();
							else
								client->DepopPet();
						}

						entity_list.ClearAggro(client);

						EQApplicationPacket* outapp = new EQApplicationPacket(OP_RezzComplete,
							sizeof(Resurrect_Struct));
						memcpy(outapp->pBuffer, &srs->rez, sizeof(Resurrect_Struct));
						worldserver.RezzPlayer(outapp, 0, 0, 0, OP_RezzComplete);
						safe_delete(outapp);
					}
					else
					{
						auto outapp = new EQApplicationPacket(OP_RezzRequest,
							sizeof(Resurrect_Struct));
						memcpy(outapp->pBuffer, &srs->rez, sizeof(Resurrect_Struct));
						client->QueuePacket(outapp);
						safe_delete(outapp);
					}
					break;
				}
			}
			if (srs->rezzopcode == OP_RezzComplete){
				// We get here when the Rezz complete packet has come back via the world server
				// to the zone that the corpse is in.
				Corpse* corpse = entity_list.GetCorpseByName(srs->rez.corpse_name);
				if (corpse && corpse->IsCorpse()) {
					Log(Logs::Detail, Logs::Spells, "OP_RezzComplete received in zone %s for corpse %s",
								zone->GetShortName(), srs->rez.corpse_name);

					Log(Logs::Detail, Logs::Spells, "Found corpse. Marking corpse as rezzed.");
					corpse->CompleteResurrection();
				}
			}

			break;
		}
		case ServerOP_RezzPlayerReject:
		{
			char *Rezzer = (char *)pack->pBuffer;

			Client* c = entity_list.GetClientByName(Rezzer);

			if (c) {
				c->Message_StringID(Chat::SpellWornOff, StringID::REZZ_ALREADY_PENDING);
			}

			break;
		}
		case ServerOP_ZoneReboot: {
			std::cout << "Got Server Requested Zone reboot" << std::endl;
			ServerZoneReboot_Struct* zb = (ServerZoneReboot_Struct*) pack->pBuffer;
			break;
		}
		case ServerOP_SyncWorldTime: {
			if(zone != 0 && !zone->is_zone_time_localized) {
				Log(Logs::Detail, Logs::ZoneServer, "%s Received Message SyncWorldTime", __FUNCTION__);

				eqTimeOfDay* newtime = (eqTimeOfDay*) pack->pBuffer;
				zone->zone_time.SetCurrentEQTimeOfDay(newtime->start_eqtime, newtime->start_realtime);
				auto outapp = new EQApplicationPacket(OP_TimeOfDay, sizeof(TimeOfDay_Struct));
				TimeOfDay_Struct* time_of_day = (TimeOfDay_Struct*)outapp->pBuffer;
				zone->zone_time.GetCurrentEQTimeOfDay(time(0), time_of_day);
				entity_list.QueueClients(0, outapp, false);
				safe_delete(outapp);
				
				/* Buffer garbage to generate debug message */
				time_t timeCurrent = time(nullptr);
				TimeOfDay_Struct eqTime;
				zone->zone_time.GetCurrentEQTimeOfDay( timeCurrent, &eqTime);

				auto time_string = fmt::format("EQTime {}:{}{} {}",
					((eqTime.hour) % 12) == 0 ? 12 : ((eqTime.hour) % 12),
					(eqTime.minute < 10) ? "0" : "",
					eqTime.minute,
					(eqTime.hour >= 12 && eqTime.hour < 24) ? "PM" : "AM"
				);

				LogInfo("Time Broadcast Packet: {}", time_string);
				zone->SetZoneHasCurrentTime(true);

			}
			if (zone->is_zone_time_localized) {
				LogInfo("Received request to sync time from world, but our time is localized currently");
			}
			break;
		}
		case ServerOP_RefreshCensorship: {
			if (!EQ::ProfanityManager::LoadProfanityList(&database)) {
				LogInfo("Received request to refresh the profanity list..but, the action failed");
			}
			break;
		}
		case ServerOP_ChangeWID: {
			if (pack->size != sizeof(ServerChangeWID_Struct)) {
				std::cout << "Wrong size on ServerChangeWID_Struct. Got: " << pack->size << ", Expected: " << sizeof(ServerChangeWID_Struct) << std::endl;
				break;
			}
			ServerChangeWID_Struct* scw = (ServerChangeWID_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByCharID(scw->charid);
			if (client) {
				client->SetWID(scw->newwid);
			}
			break;
		}
		case ServerOP_OOCMute: {
			oocmuted = *(pack->pBuffer);
			break;
		}
		case ServerOP_Revoke: {
			RevokeStruct* rev = (RevokeStruct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(rev->name);
			if (client) {
				SendEmoteMessage(rev->adminname, 0, 0, "%s: %srevoking %s", zone->GetShortName(), rev->toggle?"":"un", client->GetName());
				client->SetRevoked(rev->toggle);
			}
			break;
		}
		case ServerOP_GroupIDReply: {
			ServerGroupIDReply_Struct* ids = (ServerGroupIDReply_Struct*) pack->pBuffer;
			cur_groupid = ids->start;
			last_groupid = ids->end;
			break;
		}
		case ServerOP_GroupLeave: {
			ServerGroupLeave_Struct* gl = (ServerGroupLeave_Struct*)pack->pBuffer;
			if(zone){
				if(gl->zoneid == zone->GetZoneID() && gl->zoneguildid == zone->GetGuildID())
				{
					break;
				}

				entity_list.SendGroupLeave(gl->gid, gl->member_name, gl->checkleader);
			}
			break;
		}
		case ServerOP_GroupInvite: {
			// A player in another zone invited a player in this zone to join their group.
			ServerGroupInvite_Struct* gis = (ServerGroupInvite_Struct*)pack->pBuffer;

			Mob *Invitee = entity_list.GetMob(gis->invitee_name);

			uint8 is_null_flag = 0;

			if (Invitee && Invitee->IsClient() && Invitee->CastToClient()->GetBaseClass() == 0)
				is_null_flag = 1;

			if (Invitee && Invitee->IsClient() && !Invitee->IsRaidGrouped() && Invitee->CastToClient()->CanGroupWith(gis->group_ruleset))
			{
				auto outapp = new EQApplicationPacket(OP_GroupInvite, sizeof(GroupInvite_Struct));
				memcpy(outapp->pBuffer, gis, sizeof(GroupInvite_Struct));
				Invitee->CastToClient()->QueuePacket(outapp);
				safe_delete(outapp);
			}

			break;
		}
		case ServerOP_GroupFollow: {
			// Player in another zone accepted a group invitation from a player in this zone.
			ServerGroupFollow_Struct* sgfs = (ServerGroupFollow_Struct*) pack->pBuffer;

			Mob* Inviter = entity_list.GetClientByName(sgfs->gf.name1);

			if(Inviter && Inviter->IsClient()) {
				Group* group = entity_list.GetGroupByClient(Inviter->CastToClient());

				if(!group) {
					//Make new group
					group = new Group(Inviter);

					if (!group) {
						break;
					}

					entity_list.AddGroup(group);

					if(group->GetID() == 0) {
						Inviter->Message(Chat::Red, "Unable to get new group id. Cannot create group.");
						break;
					}
					Inviter->CastToClient()->UpdateGroupID(group->GetID());
					database.SetGroupLeaderName(group->GetID(), Inviter->GetName());
					database.SetGroupOldLeaderName(group->GetID(), Inviter->GetName());

						auto outapp =
						    new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
						GroupJoin_Struct* outgj=(GroupJoin_Struct*)outapp->pBuffer;
						strcpy(outgj->membername, Inviter->GetName());
						strcpy(outgj->yourname, Inviter->GetName());
						outgj->action = groupActInviteInitial; // 'You have formed the group'.
						Inviter->CastToClient()->QueuePacket(outapp);
						safe_delete(outapp);
				}

				if(!group) {
					break;
				}

				auto outapp = new EQApplicationPacket(OP_GroupFollow, sizeof(GroupGeneric_Struct));
				GroupGeneric_Struct *gg = (GroupGeneric_Struct *)outapp->pBuffer;
				strn0cpy(gg->name1, sgfs->gf.name1, sizeof(gg->name1));
				strn0cpy(gg->name2, sgfs->gf.name2, sizeof(gg->name2));
				Inviter->CastToClient()->QueuePacket(outapp);
				safe_delete(outapp);

				if (!group->AddMember(nullptr, sgfs->gf.name2, sgfs->CharacterID)) {
					break;
				}

				auto pack2 = new ServerPacket(ServerOP_GroupJoin, sizeof(ServerGroupJoin_Struct));
				ServerGroupJoin_Struct* gj = (ServerGroupJoin_Struct*)pack2->pBuffer;
				gj->gid = group->GetID();
				gj->zoneguildid = zone->GetGuildID();
				gj->zoneid = zone->GetZoneID();
				strn0cpy(gj->member_name, sgfs->gf.name2, sizeof(gj->member_name));
				worldserver.SendPacket(pack2);
				safe_delete(pack2);
				
				

				// Send acknowledgement back to the Invitee to let them know we have added them to the group.
				auto pack3 =
				    new ServerPacket(ServerOP_GroupFollowAck, sizeof(ServerGroupFollowAck_Struct));
				ServerGroupFollowAck_Struct* sgfas = (ServerGroupFollowAck_Struct*)pack3->pBuffer;
				strn0cpy(sgfas->Name, sgfs->gf.name2, sizeof(sgfas->Name));
				worldserver.SendPacket(pack3);
				safe_delete(pack3);
			}
			break;
		}
		case ServerOP_GroupFollowAck: {
			// The Inviter (in another zone) has successfully added the Invitee (in this zone) to the group.
			ServerGroupFollowAck_Struct* sgfas = (ServerGroupFollowAck_Struct*)pack->pBuffer;

			Client *client = entity_list.GetClientByName(sgfas->Name);

			if (!client) {
				break;
			}

			uint32 groupid = database.GetGroupID(client->GetName());

			Group* group = nullptr;

			if(groupid > 0)	{
				group = entity_list.GetGroupByID(groupid);

				if(!group) {	//nobody from our group is here... start a new group
					group = new Group(groupid);

					if (group->GetID() != 0) {
						entity_list.AddGroup(group, groupid);
					}
					else {
						group = nullptr;
					}
				}

				if (group) {
					group->UpdatePlayer(client);
				}
				else {
					client->UpdateGroupID(0); //cannot re-establish group, kill it
				}
			}

			if(group) {
				database.RefreshGroupFromDB(client);

				group->SendHPPacketsTo(client);

				// If the group leader is not set, pull the group leader information from the database.
				if(!group->GetLeader()) {
					char ln[64];
					memset(ln, 0, 64);
					strcpy(ln, database.GetGroupLeadershipInfo(group->GetID(), ln));
					Client *lc = entity_list.GetClientByName(ln);
					if (lc) {
						group->SetLeader(lc);
					}
				}
			}

			break;

		}
		case ServerOP_GroupCancelInvite: {

			GroupCancel_Struct* sgcs = (GroupCancel_Struct*) pack->pBuffer;

			Mob* Inviter = entity_list.GetClientByName(sgcs->name1);

			if(Inviter && Inviter->IsClient()) {
				auto outapp = new EQApplicationPacket(OP_GroupCancelInvite, sizeof(GroupCancel_Struct));
				memcpy(outapp->pBuffer, sgcs, sizeof(GroupCancel_Struct));
				Inviter->CastToClient()->QueuePacket(outapp);
				safe_delete(outapp);
			}
			break;
		}
		case ServerOP_GroupJoin: {
			ServerGroupJoin_Struct* gj = (ServerGroupJoin_Struct*)pack->pBuffer;
			if(zone){
				if(gj->zoneid == zone->GetZoneID() && gj->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Group* g = entity_list.GetGroupByID(gj->gid);
				if (g) {
					g->AddMember(gj->member_name);
				}

				entity_list.SendGroupJoin(gj->gid, gj->member_name);
			}
			break;
		}
		case ServerOP_RaidGroupJoin: {
			ServerRaidGroupJoin_Struct* gj = (ServerRaidGroupJoin_Struct*)pack->pBuffer;
			if (zone) {
				if (gj->zoneid == zone->GetZoneID() && gj->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid* r = entity_list.GetRaidByID(gj->rid);
				if (r) {
					r->GroupJoin(gj->member_name, gj->gid);
				}
			}
			break;
		}
		case ServerOP_ForceGroupUpdate: {
			ServerForceGroupUpdate_Struct* fgu = (ServerForceGroupUpdate_Struct*)pack->pBuffer;
			if(zone){
				if(fgu->origZoneID == zone->GetZoneID() && fgu->origZoneGuildID == zone->GetGuildID()) {
					break;
				}

				entity_list.ForceGroupUpdate(fgu->gid);
			}
			break;
		}
		case ServerOP_ChangeGroupLeader: {
			ServerGroupLeader_Struct* fgu = (ServerGroupLeader_Struct*)pack->pBuffer;
			if(zone){
				if(fgu->zoneid == zone->GetZoneID() && fgu->zoneguildid == zone->GetGuildID()) {
					break;
				}

				entity_list.SendGroupLeader(fgu->gid, fgu->leader_name, fgu->oldleader_name);
			}
			break;
		}
		case ServerOP_CheckGroupLeader: {
			ServerGroupLeader_Struct* fgu = (ServerGroupLeader_Struct*)pack->pBuffer;
			entity_list.SendGroupLeader(fgu->gid, fgu->leader_name, fgu->oldleader_name, fgu->leaderset);
			break;
		}
		case ServerOP_IsOwnerOnline: {
			ServerIsOwnerOnline_Struct* online = (ServerIsOwnerOnline_Struct*) pack->pBuffer;
			if(zone)
			{
				if(online->zoneid != zone->GetZoneID() && online->zoneguildid != zone->GetGuildID()) {
					break;
				}

				Corpse* corpse = entity_list.GetCorpseByID(online->corpseid);
				if (corpse && online->online == 1) {
					corpse->SetOwnerOnline(true);
				}
				else if (corpse) {
					corpse->SetOwnerOnline(false);
				}
			}
			break;
		}
		case ServerOP_OOZGroupMessage: {
			ServerGroupChannelMessage_Struct* gcm = (ServerGroupChannelMessage_Struct*)pack->pBuffer;
			if(zone){
				if(gcm->zoneid == zone->GetZoneID() && gcm->zoneguildid == zone->GetGuildID()) {
					break;
				}

				entity_list.GroupMessage(gcm->groupid, gcm->from, gcm->message, gcm->language, gcm->lang_skill);
			}
			break;
		}
		case ServerOP_DisbandGroup: {
			ServerDisbandGroup_Struct* sd = (ServerDisbandGroup_Struct*)pack->pBuffer;
			if(zone){
				if(sd->zoneid == zone->GetZoneID() && sd->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Group *g = entity_list.GetGroupByID(sd->groupid);
				if(g) {
					g->DisbandGroup();
				}
			}
			break;
		}
		case ServerOP_RaidAdd:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					r->LearnMembers();
					r->VerifyRaid();
					r->GetRaidDetails();
					r->SendRaidAddAll(rga->playername);
				}
			}
			break;
		}
		case ServerOP_RaidRemove:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if(rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					Client *rem = entity_list.GetClientByName(rga->playername);
					if (rem) {
						r->SendRaidDisband(rem);
					}

					r->LearnMembers();
					r->VerifyRaid();
					r->GetRaidDetails();
					
					auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
					RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
					rg->action = RaidCommandRemoveMember;
					strn0cpy(rg->leader_name, rga->playername, 64);
					strn0cpy(rg->player_name, rga->playername, 64);
					rg->parameter = 0;
					r->QueuePacket(outapp, rem);
					safe_delete(outapp);

				}
			}
			break;
		}
		case ServerOP_RaidRemoveLD: {
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					// db should already be updated with their removal
					r->LearnMembers();
					r->VerifyRaid();
					r->GetRaidDetails();
					bool noleader = true;
					int group_members = 0;
					if (rga->gleader) {
						// they were a group leader
						for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
							if(strlen(r->members[x].membername) > 0 && rga->gid == r->members[x].GroupNumber)
							{
								group_members++;
								if (r->members[x].IsGroupLeader) {
									noleader = false;
									break;
								}
							}
						}
						if (noleader && group_members > 0) {
							if (group_members > 1) {
								// still have a functional group, so reassign leader
								for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
									if(strlen(r->members[x].membername) > 0 && rga->gid == r->members[x].GroupNumber) {
										r->SetGroupLeader(r->members[x].membername, rga->gid, true);
										break;
									}
								}
							} else {
								// only one member left, so move them down to ungrouped.
								for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
									if(strlen(r->members[x].membername) > 0 && rga->gid == r->members[x].GroupNumber) {
										r->MoveMember(r->members[x].membername, 0xFFFFFFFF);
									}
								}
							}
						}
					}
					noleader = true;
					if (rga->zoneid) {
						// they were a raid leader, see if they still are
						for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
							if(strlen(r->members[x].membername) > 0 && r->members[x].IsRaidLeader) {
								noleader = false;
								break;
							}
						}
						if (noleader) {
							for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
								if(strlen(r->members[x].membername)) {
									r->SetRaidLeader(rga->playername, r->members[x].membername);
									break;
								}
							}
						}
					}
					if (rga->looter) {
						r->RemoveRaidLooter(rga->playername);

						auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
						RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
						rg->action = RaidCommandRemoveLooter;
						strn0cpy(rg->leader_name, rga->playername, 64);

						if (strlen(r->leadername) > 0) {
							strn0cpy(rg->player_name, r->leadername, 64);
						}
						else {
							strn0cpy(rg->player_name, rga->playername, 64);
						}

						r->QueuePacket(outapp);

						// this sends message out that the looter was added
						rg->action = RaidCommandRaidMessage;
						rg->parameter = 5105; // 5105 %1 was removed from the loot list
						r->QueuePacket(outapp);
						safe_delete(outapp);
					}

					if (rga->gid >= 0 && rga->gid < MAX_RAID_GROUPS) {
						r->SendGroupLeave(rga->playername, rga->gid);
					}

					auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
					RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
					rg->action = RaidCommandRemoveMember;
					strn0cpy(rg->leader_name, rga->playername, 64);
					strn0cpy(rg->player_name, rga->playername, 64);
					rg->parameter = 0;
					r->QueuePacket(outapp);
					safe_delete(outapp);
				} 
			}
			break;
		}
		case ServerOP_RaidDisband:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
						if(r->members[x].member) {
							r->SendGroupDisband(r->members[x].member);
						}
					}
					r->SendRaidDisbandAll();
					r->LearnMembers();
					r->VerifyRaid();
					r->GetRaidDetails();
				}
			}
			break;
		}
		case ServerOP_RaidChangeGroup:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					r->LearnMembers();
					r->VerifyRaid();
					r->SendRaidChangeGroup(rga->playername, rga->gid);
				}
			}
			break;
		}
		case ServerOP_UpdateGroup:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if (r) {
					r->LearnMembers();
					r->VerifyRaid();
					r->GroupUpdate(rga->gid, false);
				}
			}
			break;
		}
		case ServerOP_RaidGroupLeader:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if (r) {
					r->LearnMembers();
					r->VerifyRaid();
					r->SendMakeGroupLeaderPacket(rga->playername, rga->gid);
				}
			}
			break;
		}
		case ServerOP_RaidLeader:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					Client *c = entity_list.GetClientByName(rga->playername);
					strn0cpy(r->leadername, rga->playername, 64);
					if(c){
						r->SetLeader(c);
					}
					r->LearnMembers();
					r->VerifyRaid();
					r->SendMakeLeaderPacket(rga->playername);
				}
			}
			break;
		}
		case ServerOP_RaidAddLooter: {
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if (zone) {
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if (r) {
					r->GetRaidDetails();
					r->LearnMembers();
					r->VerifyRaid();

					// send to raid members in zone
					auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
					RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
					rg->action = RaidCommandAddLooter;
					strn0cpy(rg->leader_name, rga->playername, 64);
					strn0cpy(rg->player_name, r->leadername, 64);
					r->QueuePacket(outapp);

					// this sends message out that the looter was added
					rg->action = RaidCommandRaidMessage;
					rg->parameter = 5104; // 5104 %1 was added to raid loot list.
					r->QueuePacket(outapp);

					// send out a set loot type, to force an update the options window
					rg->action = RaidCommandSetLootType;
					rg->parameter = 3;
					r->QueuePacket(outapp);
					safe_delete(outapp);

				}
			}
			break;
		}
		case ServerOP_RemoveRaidLooter: {
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if (zone) {
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if (r) {
					r->GetRaidDetails();
					r->LearnMembers();
					r->VerifyRaid();

					// send to raid members in zone
					auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
					RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
					rg->action = RaidCommandRemoveLooter;
					strn0cpy(rg->leader_name, rga->playername, 64);
					strn0cpy(rg->player_name, r->leadername, 64);
					r->QueuePacket(outapp);

					// this sends message out that the looter was added
					rg->action = RaidCommandRaidMessage;
					rg->parameter = 5105; // 5105 %1 was removed from the loot list
					r->QueuePacket(outapp);

					// send out a set loot type, to force an update the options window
					rg->action = RaidCommandSetLootType;
					rg->parameter = 3;
					r->QueuePacket(outapp);
					safe_delete(outapp);

				}
			}
			break;
		}
		case ServerOP_DetailsChange:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					r->GetRaidDetails();
					r->LearnMembers();
					r->VerifyRaid();
				}
			}
			break;
		}
		case ServerOP_RaidTypeChange: {
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if (zone) {
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if (r) {
					r->GetRaidDetails();
					r->LearnMembers();
					r->VerifyRaid();

					// this sends the message only for loot type being set.
					auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
					RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
					rg->action = RaidCommandLootTypeResponse;
					rg->parameter = rga->looter;

					r->QueuePacket(outapp);

					// now send out to update loot setting on other clients
					rg->action = RaidCommandSetLootType;
					r->QueuePacket(outapp);
					safe_delete(outapp);
				}
			}
			break;
		}
		case ServerOP_RaidGroupDisband:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Client *c = entity_list.GetClientByName(rga->playername);
				if(c) {
					auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupGeneric_Struct2));
					GroupGeneric_Struct2* gu = (GroupGeneric_Struct2*) outapp->pBuffer;
					gu->action = groupActDisband2;
					strn0cpy(gu->membername, c->GetName(), 64);
					strn0cpy(gu->yourname, c->GetName(), 64);
					c->FastQueuePacket(&outapp);
					c->SetRaidGrouped(false);
				}
			}
			break;
		}
		case ServerOP_RaidGroupAdd:{
			ServerRaidGroupAction_Struct* rga = (ServerRaidGroupAction_Struct*)pack->pBuffer;
			if(zone){
				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					r->LearnMembers();
					r->VerifyRaid();
					auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					strn0cpy(gj->membername, rga->membername, 64);
					gj->action = groupActJoin;

					for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
						if(r->members[x].member) {
							if(strcmp(r->members[x].member->GetName(), rga->membername) != 0){
								if((rga->gid >= 0 && rga->gid < MAX_RAID_GROUPS) && rga->gid == r->members[x].GroupNumber) {
									strn0cpy(gj->yourname, r->members[x].member->GetName(), 64);
									r->members[x].member->QueuePacket(outapp);
								}
							}
						}
					}
					safe_delete(outapp);
				}
			}
			break;
		}
		case ServerOP_RaidGroupRemove:{
			ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			if(zone){
				if (rga->zoneid == zone->GetZoneID() && rga->zoneguildid == zone->GetGuildID()) {
					break;
				}

				Raid *r = entity_list.GetRaidByID(rga->rid);
				if(r){
					r->LearnMembers();
					r->VerifyRaid();
					Client *c = entity_list.GetClientByName(rga->playername);
					if (c) {
						r->SendGroupDisband(c);
					}
					r->SendGroupLeave(rga->playername, rga->gid);
				}
			}
			break;
		}
		case ServerOP_RaidGroupSay:{
			ServerRaidMessage_Struct* rmsg = (ServerRaidMessage_Struct*)pack->pBuffer;
			if(zone){
				Raid *r = entity_list.GetRaidByID(rmsg->rid);
				if(r) {
					for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
						if(r->members[x].member) {
							if(strcmp(rmsg->from, r->members[x].member->GetName()) != 0) {
								if(r->members[x].GroupNumber == rmsg->gid){
									if(r->members[x].member->GetFilter(FilterGroupChat)!=0) {
										r->members[x].member->ChannelMessageSend(rmsg->from, r->members[x].member->GetName(), ChatChannel_Group, rmsg->language, rmsg->lang_skill, rmsg->message);
									}
								}
							}
						}
					}
				}
			}
			break;
		}
		case ServerOP_RaidSay:{
			ServerRaidMessage_Struct* rmsg = (ServerRaidMessage_Struct*)pack->pBuffer;
			if(zone) {
				Raid *r = entity_list.GetRaidByID(rmsg->rid);
				if(r) {
					for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
						if(r->members[x].member) {
							if(strcmp(rmsg->from, r->members[x].member->GetName()) != 0) {
								if(r->members[x].member->GetFilter(FilterGroupChat)!=0) {
									r->members[x].member->ChannelMessageSend(rmsg->from, r->members[x].member->GetName(), ChatChannel_Raid, rmsg->language, rmsg->lang_skill, rmsg->message);
								}
							}
						}
					}
				}
			}
			break;
		}
		case ServerOP_SpawnPlayerCorpse: {
			SpawnPlayerCorpse_Struct* s = (SpawnPlayerCorpse_Struct*)pack->pBuffer;
			Corpse* NewCorpse = database.LoadCharacterCorpse(s->player_corpse_id);

			if (NewCorpse) {
				NewCorpse->Spawn();
			}
			else {
				Log(Logs::General, Logs::Error, "Unable to load player corpse id %u for zone %s.", s->player_corpse_id, zone->GetShortName());
			}

			break;
		}
		case ServerOP_Consent: {
			ServerOP_Consent_Struct* s = (ServerOP_Consent_Struct*)pack->pBuffer;
			Client* client = entity_list.GetClientByName(s->grantname);
			if(client) {
				client->Consent(s->permission, s->ownername, s->grantname);

				auto outapp =
				    new EQApplicationPacket(OP_ConsentResponse, sizeof(ConsentResponse_Struct));
				ConsentResponse_Struct* crs = (ConsentResponse_Struct*)outapp->pBuffer;
				strcpy(crs->grantname, s->grantname);
				strcpy(crs->ownername, s->ownername);
				crs->permission = s->permission;
				strcpy(crs->zonename,"all zones");
				client->QueuePacket(outapp);
				safe_delete(outapp);
			}
			else {
				auto scs_pack =
				    new ServerPacket(ServerOP_Consent_Response, sizeof(ServerOP_Consent_Struct));
				ServerOP_Consent_Struct* scs = (ServerOP_Consent_Struct*)scs_pack->pBuffer;
				strcpy(scs->grantname, s->grantname);
				strcpy(scs->ownername, s->ownername);
				scs->permission = s->permission;
				scs->zone_id = s->zone_id;
				scs->GuildID = s->GuildID;
				scs->message_string_id = StringID::TARGET_NOT_FOUND;
				scs->corpse_id = 0;
				worldserver.SendPacket(scs_pack);
				safe_delete(scs_pack);
			}
			break;
		}
		case ServerOP_Consent_Response: {
			ServerOP_Consent_Struct* s = (ServerOP_Consent_Struct*)pack->pBuffer;
			Client* owner = entity_list.GetClientByName(s->ownername);
			Client* grant = entity_list.GetClientByName(s->grantname);

			// Consent has completed successfully in ServerOP_Consent. Send the success message to the owner.
			if(owner && s->message_string_id == StringID::CONSENT_GIVEN) {
				owner->Message_StringID(Chat::White, s->message_string_id, s->grantname);
			}
			// Revoke consent.
			else if(grant && s->message_string_id == StringID::CONSENT_BEEN_DENIED) {
				grant->Consent(0, s->ownername, s->grantname, false, s->corpse_id);
				if (s->corpse_id == 0) {
					grant->Message_StringID(Chat::White, s->message_string_id, s->ownername);
				}
			}
			// Granted player is not online or doesn't exist. Consent them in the DB.
			else if(owner) {
				char ownername[64];
				strcpy(ownername, owner->GetName());
				owner->Consent(1, ownername, s->grantname, true);
				owner->Message_StringID(Chat::White, StringID::CONSENT_GIVEN, s->grantname);
			}
			break;
		}
		case ServerOP_ConsentDeny: {
			ServerOP_ConsentDeny_Struct* s = (ServerOP_ConsentDeny_Struct*)pack->pBuffer;
			Client* client = entity_list.GetClientByName(s->grantname);
			if (client) {
				client->Message_StringID(Chat::White, StringID::CONSENT_BEEN_DENIED, s->ownername);
				client->Consent(0, s->ownername, s->grantname);
			}
			break;
		}
		case ServerOP_UpdateSpawn: {
			if(zone) {
				UpdateSpawnTimer_Struct *ust = (UpdateSpawnTimer_Struct*)pack->pBuffer;
				LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
				iterator.Reset();
				while (iterator.MoreElements()) {
					if(iterator.GetData()->GetID() == ust->id) {
						if(!iterator.GetData()->NPCPointerValid()) {
							iterator.GetData()->SetTimer(ust->duration);
						}
						break;
					}
					iterator.Advance();
				}
			}
			break;
		}
		case ServerOP_DepopAllPlayersCorpses: {
			ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct *)pack->pBuffer;

			if(zone && !((zone->GetZoneID() == sdapcs->ZoneID && zone->GetGuildID() == sdapcs->GuildID))) {
				entity_list.RemoveAllCorpsesByCharID(sdapcs->CharacterID);
			}

			break;

		}
		case ServerOP_DepopPlayerCorpse: {
			ServerDepopPlayerCorpse_Struct *sdpcs = (ServerDepopPlayerCorpse_Struct *)pack->pBuffer;

			if(zone && !((zone->GetZoneID() == sdpcs->ZoneID && zone->GetGuildID() == sdpcs->GuildID))) {
				entity_list.RemoveCorpseByDBID(sdpcs->DBID);
			}

			break;

		}
		case ServerOP_SpawnStatusChange: {
			if(zone) {
				ServerSpawnStatusChange_Struct *ssc = (ServerSpawnStatusChange_Struct*)pack->pBuffer;
				LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
				iterator.Reset();
				Spawn2 *found_spawn = nullptr;
				while(iterator.MoreElements()) {
					Spawn2* cur = iterator.GetData();
					if(cur->GetID() == ssc->id) {
						found_spawn = cur;
						break;
					}
					iterator.Advance();
				}

				if(found_spawn) {
					if(ssc->new_status == 0) {
						found_spawn->Disable();
					}
					else {
						found_spawn->Enable();
					}
				}
			}
			break;
		}
		case ServerOP_QGlobalUpdate: {
			if(pack->size != sizeof(ServerQGlobalUpdate_Struct)) {
				break;
			}

			if(zone) {
				ServerQGlobalUpdate_Struct *qgu = (ServerQGlobalUpdate_Struct*)pack->pBuffer;
				if(qgu->from_zone_id != zone->GetZoneID()) {
					QGlobal temp;
					temp.npc_id = qgu->npc_id;
					temp.char_id = qgu->char_id;
					temp.zone_id = qgu->zone_id;
					temp.expdate = qgu->expdate;
					temp.name.assign(qgu->name);
					temp.value.assign(qgu->value);
					entity_list.UpdateQGlobal(qgu->id, temp);
					zone->UpdateQGlobal(qgu->id, temp);
				}
			}
			break;
		}
		case ServerOP_QGlobalDelete: {
			if(pack->size != sizeof(ServerQGlobalDelete_Struct)) {
				break;
			}

			if(zone) {
				ServerQGlobalDelete_Struct *qgd = (ServerQGlobalDelete_Struct*)pack->pBuffer;
				if(qgd->from_zone_id != zone->GetZoneID()) {
					entity_list.DeleteQGlobal(std::string((char*)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);
					zone->DeleteQGlobal(std::string((char*)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);
				}
			}
			break;
		}
		case ServerOP_UpdateSchedulerEvents: {

			LogScheduler("Received signal from world to update");
			if (GetScheduler()) {
				m_zone_scheduler->LoadScheduledEvents();
			}

			break;
		}
		
		case ServerOP_ReloadSpellModifiers: {
			database.LoadSpellModifiers(spellModifiers);
			break;
		}
		
		case ServerOP_QueryServGeneric: {
			pack->SetReadPosition(8);
			char From[64];
			pack->ReadString(From);

			Client *c = entity_list.GetClientByName(From);

			if (!c) {
				return;
			}

			uint32 Type = pack->ReadUInt32();

			break;
		}
		case ServerOP_UCSServerStatusReply: {
			if (zone && zone->IsLoaded()) {
				auto ucsss = (UCSServerStatus_Struct*)pack->pBuffer;
				zone->SetUCSServerAvailable((ucsss->available != 0), ucsss->timestamp);
			}
			break;
		}
		case ServerOP_CZSetEntityVariableByNPCTypeID: {
			CZSetEntVarByNPCTypeID_Struct* CZM = (CZSetEntVarByNPCTypeID_Struct*)pack->pBuffer;
			NPC* n = entity_list.GetNPCByNPCTypeID(CZM->npctype_id);
			if (n != 0) {
				n->SetEntityVariable(CZM->id, CZM->m_var);
			}
			break;
		}
		case ServerOP_CZSignalNPC: {
			CZNPCSignal_Struct* CZCN = (CZNPCSignal_Struct*)pack->pBuffer;
			NPC* n = entity_list.GetNPCByNPCTypeID(CZCN->npctype_id); 
			if (n != 0) {
				n->SignalNPC(CZCN->num, CZCN->data);
			}
			break;
		}
		case ServerOP_CZSignalClient: {
			CZClientSignal_Struct* CZCS = (CZClientSignal_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByCharID(CZCS->charid);
			if (client != 0) {
				client->Signal(CZCS->data);
			}
			break;
		}
		case ServerOP_CZSignalClientByName: {
			CZClientSignalByName_Struct* CZCS = (CZClientSignalByName_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(CZCS->Name);
			if (client != 0) {
				client->Signal(CZCS->data);
			}
			break;
		}
		case ServerOP_CZMessagePlayer: {
			CZMessagePlayer_Struct* CZCS = (CZMessagePlayer_Struct*) pack->pBuffer;
			Client* client = entity_list.GetClientByName(CZCS->CharName);
			if (client != 0) {
				client->Message(CZCS->Type, CZCS->Message);
			}
			break;
		}
		case ServerOP_HotReloadQuests: {
			if (!zone) {
				break;
			}

			auto* hot_reload_quests = (HotReloadQuestsStruct*)pack->pBuffer;

			LogHotReloadDetail(
				"Receiving request [HotReloadQuests] | request_zone [{}] current_zone [{}]",
				hot_reload_quests->zone_short_name,
				zone->GetShortName()
			);

			std::string request_zone_short_name = hot_reload_quests->zone_short_name;
			std::string local_zone_short_name = zone->GetShortName();

			if (request_zone_short_name == local_zone_short_name || request_zone_short_name == "all") {
				zone->SetQuestHotReloadQueued(true);
			}

			break;
		}
		case ServerOP_ReloadOpcodes: {
			zone->SendReloadMessage("Opcodes");
			ReloadAllPatches();
			break;
		}
		case ServerOP_ReloadAAData: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Alternate Advancement Data");
				zone->LoadAlternateAdvancement();
			}
			break;
		}
		case ServerOP_ReloadBlockedSpells: {
			if (zone && zone->IsLoaded()) {
			zone->SendReloadMessage("Blocked Spells");
			zone->ClearBlockedSpells();
			zone->LoadZoneBlockedSpells();
			}
			break;
		}
		case ServerOP_ReloadCommands: {
			zone->SendReloadMessage("Commands");
			command_init();
			break;
		}
		case ServerOP_ReloadContentFlags: {
			zone->SendReloadMessage("Content Flags");
			content_service.SetExpansionContext()->ReloadContentFlags();
			break;
		}
		case ServerOP_ReloadDoors: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Doors");
				entity_list.RemoveAllDoors();
				zone->LoadZoneDoors();
				entity_list.RespawnAllDoors();
			}
			break;
		}
		case ServerOP_ReloadFactions: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Factions");
				database.LoadFactionData();
				zone->ReloadNPCFactions();
			}
			break;
		}
		case ServerOP_ReloadGroundSpawns: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Ground Spawns");
				zone->LoadGroundSpawns();
			}
			break;
		}
		case ServerOP_ReloadLevelEXPMods: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Level Based Experience Modifiers");
				zone->LoadLevelEXPMods();
			}
			break;
		}
		case ServerOP_ReloadLogs: {
			zone->SendReloadMessage("Log Settings");
			LogSys.LoadLogDatabaseSettings();
			player_event_logs.ReloadSettings();
			break;
		}
		case ServerOP_ReloadLoot: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Loot");
				zone->ReloadLootTables();
			}
			break;
		}
		case ServerOP_ReloadKeyRings: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Key Rings");
				zone->key_ring_data_list.Clear();
				zone->LoadKeyRingData(&zone->key_ring_data_list);
			}
			break;
		}
		case ServerOP_ReloadMerchants: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Merchants");
				entity_list.ReloadMerchants();
			}
			break;
		}
		case ServerOP_ReloadNPCEmotes: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("NPC Emotes");
				zone->LoadNPCEmotes(&zone->npc_emote_list);
			}
			break;
		}
		case ServerOP_ReloadNPCSpells: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("NPC Spells");
				database.ClearNPCSpells();
				for (auto& e : entity_list.GetNPCList()) {
					e.second->ReloadSpells();
				}
			}
			break;
		}
		case ServerOP_ReloadObjects: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Objects");
				entity_list.RemoveAllObjects();
				zone->LoadZoneObjects();
			}
			break;
		}
		case ServerOP_ReloadRules: {
			zone->SendReloadMessage("Rules");
			database.LoadZoneNames();
			database.LoadZoneFileNames();
			RuleManager::Instance()->LoadRules(&database, RuleManager::Instance()->GetActiveRuleset());
			break;
		}
		case ServerOP_ReloadSkillCaps: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Skill Caps");
				skill_caps.ReloadSkillCaps();
			}
			break;
		}
		case ServerOP_ReloadStaticZoneData: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Static Zone Data");
				zone->ReloadStaticData();
			}
			break;
		}
		case ServerOP_ReloadTitles: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Titles");
				title_manager.LoadTitles();
			}
			break;
		}
		case ServerOP_ReloadTraps: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Traps");
				entity_list.UpdateAllTraps(true, true);
			}
			break;
		}
		case ServerOP_ReloadVariables: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Variables");
				database.LoadVariables();
			}
			break;
		}
		case ServerOP_ReloadWorld: {
			auto* reload_world = (ReloadWorld_Struct*)pack->pBuffer;
			if (zone) {
				zone->ReloadWorld(reload_world->global_repop);
			}
			break;
		}
		case ServerOP_ReloadZonePoints: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Zone Points");
				database.LoadStaticZonePoints(&zone->zone_point_list, zone->GetShortName());
			}
			break;
		}
		case ServerOP_ReloadZoneData: {
			zone_store.LoadZones(database);
			database.LoadZoneNames();
			database.LoadZoneFileNames();
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Zone Data");
				zone->LoadZoneCFG(zone->GetShortName());
			}
			break;
		}
		case ServerOP_Soulmark: {
			ServerRequestSoulMark_Struct* SM = (ServerRequestSoulMark_Struct*) pack->pBuffer;
			if(zone){
				
				Client* client = entity_list.GetClientByName(SM->name);
				if(client) {
					client->SendSoulMarks(&SM->entry);
				}
			}
			break;
		}
		case ServerOP_ChangeSharedMem: {
			std::string hotfix_name = std::string((char*)pack->pBuffer);
			LogInfo("Loading items");
			if(!database.LoadItems(hotfix_name)) {
				LogError("Loading items FAILED!");
			}

			LogInfo("Loading spells");
			if(!database.LoadSpells(hotfix_name, &SPDAT_RECORDS, &spells)) {
				LogError("Loading spells FAILED!");
			}
			break;
		}
		case ServerOP_ReloadSkills: {
			if (zone && zone->IsLoaded()) {
				zone->SendReloadMessage("Skill Difficulty");
				zone->skill_difficulty.clear();
				zone->LoadSkillDifficulty();
			}
			break;
		}
		case ServerOP_Weather: {
			if (zone) {
				// Stop any weather first.
				if (zone->zone_weather > 0)	{
					zone->zone_weather = 0;
					zone->weather_intensity = 0;
					zone->weatherSend();
				}

				ServerWeather_Struct* ww = (ServerWeather_Struct*)pack->pBuffer;
				zone->zone_weather = ww->type;
				zone->weather_intensity = ww->intensity;
				zone->weatherSend(ww->timer * 1000);
			}
			break;
		}
		
		case ServerOP_QuakeEnded:
		{
			if (zone && zone->GetGuildID() == 1)
			{

				ServerEarthquakeImminent_Struct* seis = (ServerEarthquakeImminent_Struct*)pack->pBuffer;
				memcpy(&zone->last_quake_struct, seis, sizeof(ServerEarthquakeImminent_Struct));
				//entity_list.TogglePVPForQuake();
			}
			else if (zone)
			{
				zone->last_quake_struct.quake_type = QuakeDisabled;
			}
			break;
		}

		case ServerOP_QuakeImminent:
		{
			if (zone && zone->GetGuildID() == 1)
			{

				ServerEarthquakeImminent_Struct* seis = (ServerEarthquakeImminent_Struct*)pack->pBuffer;
				memcpy(&zone->last_quake_struct, seis, sizeof(ServerEarthquakeImminent_Struct));

				uint32 cur_time = Timer::GetTimeSeconds();
				bool should_broadcast_notif = false;
				if (zone->last_quake_struct.start_timestamp >= cur_time)
				{
					should_broadcast_notif = zone->ResetEngageNotificationTargets((RuleI(Quarm, QuakeRepopDelay)) * 1000); // if we reset at least one, this is true
					if (should_broadcast_notif)
					{
						entity_list.Message(Chat::Default, Chat::Yellow, "Creatures in this zone will repop!");
						//entity_list.EvacAllPlayers();
					}
				}
				if (should_broadcast_notif == false)
				{
					zone->last_quake_struct.quake_type = QuakeDisabled;
				}
				//entity_list.TogglePVPForQuake();
				if (zone->EndQuake_Timer)
				{
					zone->EndQuake_Timer->Enable();
					zone->EndQuake_Timer->Start((RuleI(Quarm, QuakeRepopDelay) + RuleI(Quarm, QuakeEndTimeDuration)) * 1000);
				}
			}
			else if (zone)
			{
				zone->last_quake_struct.quake_type = QuakeDisabled;
			}
			break;
		}

		default: {
			std::cout << " Unknown ZSopcode:" << (int)pack->opcode;
			std::cout << " size:" << pack->size << std::endl;
			break;
		}
	}

}

bool WorldServer::SendChannelMessage(Client* from, const char* to, uint8 chan_num, uint32 guilddbid, uint8 language, uint8 lang_skill, const char* message, ...) {
	if(!worldserver.Connected())
		return false;
	char buffer[512];

	memcpy(buffer, message, 512);
	buffer[511] = '\0';

	auto pack = new ServerPacket(ServerOP_ChannelMessage, sizeof(ServerChannelMessage_Struct) + strlen(buffer) + 1);
	ServerChannelMessage_Struct* scm = (ServerChannelMessage_Struct*) pack->pBuffer;

	if (from == 0) {
		strcpy(scm->from, "ZServer");
		scm->fromadmin = 0;
	} else {
		strcpy(scm->from, from->GetName());
		scm->fromadmin = from->Admin();
	}
	if (to == 0) {
		scm->to[0] = 0;
		scm->deliverto[0] = '\0';
	} else {
		strn0cpy(scm->to, to, sizeof(scm->to));
		strn0cpy(scm->deliverto, to, sizeof(scm->deliverto));
	}
	scm->chan_num = chan_num;
	scm->guilddbid = guilddbid;
	scm->language = language;
	scm->lang_skill = lang_skill;
	strcpy(scm->message, buffer);

	bool ret = SendPacket(pack);
	safe_delete(pack);
	return ret;
}

bool WorldServer::SendChannelMessage(const char* from, uint8 chan_num, uint32 guilddbid, uint8 language, uint8 lang_skill, const char* message, ...) {
	if (!worldserver.Connected())
		return false;
	char buffer[512];

	memcpy(buffer, message, 512);
	buffer[511] = '\0';

	auto pack = new ServerPacket(ServerOP_ChannelMessage, sizeof(ServerChannelMessage_Struct) + strlen(buffer) + 1);
	ServerChannelMessage_Struct* scm = (ServerChannelMessage_Struct*)pack->pBuffer;

	if (from == 0) {
		strcpy(scm->from, "ZServer");
		scm->fromadmin = 0;
	}
	else {
		strn0cpy(scm->from, from, sizeof(scm->from));
	}

	scm->to[0] = 0;
	scm->deliverto[0] = '\0';
	scm->chan_num = chan_num;
	scm->guilddbid = guilddbid;
	scm->language = language;
	scm->lang_skill = lang_skill;
	strcpy(scm->message, buffer);

	bool ret = SendPacket(pack);
	safe_delete(pack);
	return ret;
}


bool WorldServer::SendEmoteMessage(const char* to, uint32 to_guilddbid, uint32 type, const char* message, ...) {
	va_list argptr;
	char buffer[256];

	va_start(argptr, message);
	vsnprintf(buffer, 256, message, argptr);
	va_end(argptr);

	return SendEmoteMessage(to, to_guilddbid, 0, type, buffer);
}

bool WorldServer::SendEmoteMessage(const char* to, uint32 to_guilddbid, int16 to_minstatus, uint32 type, const char* message, ...) {
	va_list argptr;
	char buffer[256];

	va_start(argptr, message);
	vsnprintf(buffer, 256, message, argptr);
	va_end(argptr);

	if (!Connected() && to == 0) {
		entity_list.MessageStatus(to_guilddbid, to_minstatus, type, buffer);
		return false;
	}

	auto pack = new ServerPacket(ServerOP_EmoteMessage, sizeof(ServerEmoteMessage_Struct) + strlen(buffer) + 1);
	ServerEmoteMessage_Struct* sem = (ServerEmoteMessage_Struct*) pack->pBuffer;
	sem->type = type;
	if (to != 0)
		strcpy(sem->to, to);
	sem->guilddbid = to_guilddbid;
	sem->minstatus = to_minstatus;
	strcpy(sem->message, buffer);

	bool ret = SendPacket(pack);
	safe_delete(pack);
	return ret;
}

bool WorldServer::RezzPlayer(EQApplicationPacket* rpack, uint32 rezzexp, uint32 dbid, uint32 char_id, uint16 opcode)
{
	Log(Logs::Detail, Logs::Spells, "WorldServer::RezzPlayer rezzexp is %i (0 is normal for RezzComplete", rezzexp);
	auto pack = new ServerPacket(ServerOP_RezzPlayer, sizeof(RezzPlayer_Struct));
	RezzPlayer_Struct* sem = (RezzPlayer_Struct*) pack->pBuffer;
	sem->rezzopcode = opcode;
	sem->rez = *(Resurrect_Struct*) rpack->pBuffer;
	sem->exp = rezzexp;
	sem->dbid = dbid;
	sem->corpse_character_id = char_id;
	sem->corpse_zone_id = zone->GetZoneID();
	sem->corpse_zone_guild_id = zone->GetGuildID();
	bool ret = SendPacket(pack);
	if (ret) {
		Log(Logs::Detail, Logs::Spells, "Sending player rezz packet to world spellid:%i", sem->rez.spellid);
	}
	else {
		Log(Logs::Detail, Logs::Spells, "NOT Sending player rezz packet to world");
	}

	safe_delete(pack);
	return ret;
}

uint32 WorldServer::NextGroupID() {
	//this system wastes a lot of potential group IDs (~5%), but
	//if you are creating 2 billion groups in 1 run of the emu,
	//something else is wrong...
	if(cur_groupid >= last_groupid) {
		//this is an error... This means that 50 groups were created before
		//1 packet could make the zone->world->zone trip... so let it error.
		Log(Logs::General, Logs::Error, "Ran out of group IDs before the server sent us more.");
		return(0);
	}
	if(cur_groupid > (last_groupid - /*50*/995)) {
		//running low, request more
		auto pack = new ServerPacket(ServerOP_GroupIDReq);
		SendPacket(pack);
		safe_delete(pack);
	}
	Log(Logs::General, Logs::Group, "Handing out new group id %d", cur_groupid);
	return(cur_groupid++);
}

void WorldServer::RequestTellQueue(const char *who)
{
	if (!who)
		return;

	auto pack = new ServerPacket(ServerOP_RequestTellQueue, sizeof(ServerRequestTellQueue_Struct));
	ServerRequestTellQueue_Struct* rtq = (ServerRequestTellQueue_Struct*) pack->pBuffer;

	strn0cpy(rtq->name, who, sizeof(rtq->name));

	SendPacket(pack);
	safe_delete(pack);
	return;
}

ZoneEventScheduler *WorldServer::GetScheduler() const
{
	return m_zone_scheduler;
}

void WorldServer::SetScheduler(ZoneEventScheduler *scheduler)
{
	WorldServer::m_zone_scheduler = scheduler;
}