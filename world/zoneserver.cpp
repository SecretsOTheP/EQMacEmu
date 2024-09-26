/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2005 EQEMu Development Team (http://eqemulator.net)

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
#include "zoneserver.h"
#include "clientlist.h"
#include "login_server.h"
#include "login_server_list.h"
#include "zonelist.h"
#include "worlddb.h"
#include "console.h"
#include "client.h"
#include "../common/md5.h"
#include "world_config.h"
#include "../common/guilds.h"
#include "../common/packet_dump.h"
#include "../common/misc.h"
#include "../common/strings.h"
#include "cliententry.h"
#include "wguild_mgr.h"
#include "ucs.h"
#include "queryserv.h"
#include "../common/content/world_content_service.h"

extern ClientList client_list;
extern ZSList zoneserver_list;
extern ConsoleList console_list;
extern LoginServerList loginserverlist;
extern volatile bool RunLoops;
extern UCSConnection UCSLink;
extern QueryServConnection QSLink;
void CatchSignal(int sig_num);

ZoneServer::ZoneServer(EmuTCPConnection* itcpc)
: WorldTCPConnection(), tcpc(itcpc), zone_boot_timer(5000) {

	/* Set Process tracking variable defaults */

	memset(zone_name, 0, sizeof(zone_name));
	memset(compiled, 0, sizeof(compiled));
	memset(client_address, 0, sizeof(client_address));
	memset(client_local_address, 0, sizeof(client_local_address));

	zone_server_id = zoneserver_list.GetNextID();
	zone_server_previous_zone_id = 0;
	zone_server_previous_guild_id = 0xFFFFFFFF;
	zone_server_zone_id = 0;
	zone_server_guild_id = 0xFFFFFFFF;
	zone_os_process_id = 0;
	client_port = 0;
	is_booting_up = false;
	ucs_connected = false;
	is_authenticated = false;
	is_static_zone = false;
	zone_player_count = 0;
}

ZoneServer::~ZoneServer() {
	if (RunLoops)
		client_list.CLERemoveZSRef(this);
	tcpc->Free();
}

bool ZoneServer::SetZone(uint32 iZoneID, bool iStaticZone, uint32 iGuildID) {
	is_booting_up = false;
	ucs_connected = UCSLink.Connected();

	const char* zn = MakeLowerString(database.GetZoneName(iZoneID));
	char*	longname;

	if (iZoneID)
		Log(Logs::Detail, Logs::WorldServer,"Setting to '%s' (%d:%d)%s",(zn) ? zn : "",iZoneID,iGuildID,
			iStaticZone ? " (Static)" : "");

	zone_server_zone_id = iZoneID;
	zone_server_guild_id = iGuildID;
	if (iZoneID != 0)
	{
		zone_server_previous_zone_id = iZoneID;
		zone_server_previous_guild_id = iGuildID;
	}
	if (zone_server_zone_id == 0) {
		client_list.CLERemoveZSRef(this);
		zone_player_count = 0;
		if(zone_server_previous_guild_id != 0xFFFFFFFF)
			LSSleepUpdate(GetPrevZoneID());
	}

	is_static_zone = iStaticZone;

	if (zn)
	{
		strn0cpy(zone_name, zn, sizeof(zone_name));
		if( database.GetZoneLongName( (char*)zone_name, &longname, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) )
		{
			strn0cpy(long_name, longname, sizeof(long_name));
			safe_delete_array( longname );
		}
		else
			strcpy(long_name, "");
	}
	else
	{
		strcpy(zone_name, "");
		strcpy(long_name, "");
	}

	client_list.ZoneBootup(this);
	zone_boot_timer.Start();

	return true;
}

void ZoneServer::LSShutDownUpdate(uint32 zoneid){
	if(WorldConfig::get()->UpdateStats){
		auto pack = new ServerPacket;
		pack->opcode = ServerOP_LSZoneShutdown;
		pack->size = sizeof(ZoneShutdown_Struct);
		pack->pBuffer = new uchar[pack->size];
		memset(pack->pBuffer,0,pack->size);
		ZoneShutdown_Struct* zsd =(ZoneShutdown_Struct*)pack->pBuffer;
		if(zoneid==0)
			zsd->zone = GetPrevZoneID();
		else
			zsd->zone = zoneid;
		zsd->zone_wid = GetID();
		loginserverlist.SendPacket(pack);
		safe_delete(pack);
	}
}

void ZoneServer::LSBootUpdate(uint32 zoneid, bool startup){
	if(WorldConfig::get()->UpdateStats){
		auto pack = new ServerPacket;
		if(startup)
			pack->opcode = ServerOP_LSZoneStart;
		else
			pack->opcode = ServerOP_LSZoneBoot;
		pack->size = sizeof(ZoneBoot_Struct);
		pack->pBuffer = new uchar[pack->size];
		memset(pack->pBuffer,0,pack->size);
		ZoneBoot_Struct* bootup =(ZoneBoot_Struct*)pack->pBuffer;
		if(startup)
			strcpy(bootup->compile_time,GetCompileTime());
		bootup->zone = zoneid;
		bootup->zone_wid = GetID();
		loginserverlist.SendPacket(pack);
		safe_delete(pack);
	}
}

void ZoneServer::LSSleepUpdate(uint32 zoneid){
	if(WorldConfig::get()->UpdateStats){
		auto pack = new ServerPacket;
		pack->opcode = ServerOP_LSZoneSleep;
		pack->size = sizeof(ServerLSZoneSleep_Struct);
		pack->pBuffer = new uchar[pack->size];
		memset(pack->pBuffer,0,pack->size);
		ServerLSZoneSleep_Struct* sleep =(ServerLSZoneSleep_Struct*)pack->pBuffer;
		sleep->zone = zoneid;
		sleep->zone_wid = GetID();
		loginserverlist.SendPacket(pack);
		safe_delete(pack);
	}
}

extern void TriggerManualQuake(QuakeType in_quake_type);

bool ZoneServer::Process() {
	if (!tcpc->Connected())
		return false;
	if(zone_boot_timer.Check()){
		LSBootUpdate(GetZoneID(), true);
		zone_boot_timer.Disable();
	}
	if (is_authenticated && !IsBootingUp()) {
		bool was_connected = ucs_connected;
		ucs_connected = UCSLink.Connected();
		if (was_connected != ucs_connected) {
			if (was_connected) {
				this->SendEmoteMessage(0, 0, AccountStatus::Player, Chat::ChatChannel, "The Universal Chat service is temporarily unavailable. You will be notified when it is restored.");
			}
			else {
				this->SendEmoteMessage(0, 0, AccountStatus::Player, Chat::ChatChannel, "The Universal Chat service has been restored.  You must zone to re-join channels.");
			}
		}
	}
	ServerPacket *pack = 0;
	while((pack = tcpc->PopPacket())) {
		if (!is_authenticated) {
			if (WorldConfig::get()->SharedKey.length() > 0) {
				if (pack->opcode == ServerOP_ZAAuth && pack->size == 16) {
					uint8 tmppass[16];
					MD5::Generate((const uchar*) WorldConfig::get()->SharedKey.c_str(), WorldConfig::get()->SharedKey.length(), tmppass);
					if (memcmp(pack->pBuffer, tmppass, 16) == 0)
						is_authenticated = true;
					else {
						struct in_addr in;
						in.s_addr = GetIP();
						Log(Logs::Detail, Logs::WorldServer,"Zone authorization failed.");
						auto pack = new ServerPacket(ServerOP_ZAAuthFailed);
						SendPacket(pack);
						safe_delete(pack);
						Disconnect();
						return false;
					}
				}
				else {
					struct in_addr in;
					in.s_addr = GetIP();
					Log(Logs::Detail, Logs::WorldServer,"Zone authorization failed.");
					auto pack = new ServerPacket(ServerOP_ZAAuthFailed);
					SendPacket(pack);
					safe_delete(pack);
					Disconnect();
					return false;
				}
			}
			else
			{
				Log(Logs::Detail, Logs::WorldServer,"**WARNING** You have not configured a world shared key in your config file. You should add a <key>STRING</key> element to your <world> element to prevent unauthroized zone access.");
				is_authenticated = true;
			}
		}
		switch(pack->opcode) {
			case 0:
				break;
			case ServerOP_KeepAlive: {
				// ignore this
				break;
			}
			case ServerOP_ZAAuth: {
				break;
			}
			case ServerOP_LSZoneBoot:{
				if(pack->size==sizeof(ZoneBoot_Struct)){
					ZoneBoot_Struct* zbs= (ZoneBoot_Struct*)pack->pBuffer;
					SetCompile(zbs->compile_time);
				}
				break;
			}
			case ServerOP_GroupInvite: {
				if(pack->size != sizeof(GroupInvite_Struct))
					break;

				ServerGroupInvite_Struct* gis = (ServerGroupInvite_Struct*) pack->pBuffer;

				client_list.SendPacket(gis->invitee_name, pack);
				break;
			}
			case ServerOP_GroupFollow: {
				if(pack->size != sizeof(ServerGroupFollow_Struct))
					break;

				ServerGroupFollow_Struct *sgfs = (ServerGroupFollow_Struct *) pack->pBuffer;

				client_list.SendPacket(sgfs->gf.name1, pack);
				break;
			}
			case ServerOP_GroupFollowAck: {
				if(pack->size != sizeof(ServerGroupFollowAck_Struct))
					break;

				ServerGroupFollowAck_Struct *sgfas = (ServerGroupFollowAck_Struct *) pack->pBuffer;

				client_list.SendPacket(sgfas->Name, pack);
				break;
			}
			case ServerOP_GroupCancelInvite: {
				if(pack->size != sizeof(GroupCancel_Struct))
					break;

				GroupCancel_Struct *gcs = (GroupCancel_Struct *) pack->pBuffer;

				client_list.SendPacket(gcs->name1, pack);
				break;
			}
			case ServerOP_GroupSetID: {
				if(pack->size != sizeof(GroupSetID_Struct))
					break;
				GroupSetID_Struct *gsi = (GroupSetID_Struct *) pack->pBuffer;
				// set groupid in CLE
				uint32 char_id = gsi->char_id;
				ClientListEntry *client_cle = client_list.FindCLEByCharacterID(gsi->char_id);
				if (client_cle) 
					client_cle->SetGroupID(gsi->group_id);
				break;
			}
			case ServerOP_GroupIDReq: {
				SendGroupIDs();
				break;
			}
			case ServerOP_GroupLeave: {
				if(pack->size != sizeof(ServerGroupLeave_Struct))
					break;
				zoneserver_list.SendPacket(pack); //bounce it to all zones
				break;
			}

			case ServerOP_GroupJoin: {
				if(pack->size != sizeof(ServerGroupJoin_Struct))
					break;
				zoneserver_list.SendPacket(pack); //bounce it to all zones
				break;
			}

			case ServerOP_RaidGroupJoin: {
				if (pack->size != sizeof(ServerRaidGroupJoin_Struct))
					break;
				zoneserver_list.SendPacket(pack); //bounce it to all zones
				break;
			}

			case ServerOP_ForceGroupUpdate: {
				if(pack->size != sizeof(ServerForceGroupUpdate_Struct))
					break;
				zoneserver_list.SendPacket(pack); //bounce it to all zones
				break;
			}

			case ServerOP_OOZGroupMessage: {
				zoneserver_list.SendPacket(pack); //bounce it to all zones

				// check for characters zoning and queue messages
				ServerGroupChannelMessage_Struct* gcm = (ServerGroupChannelMessage_Struct*)pack->pBuffer; 
				if (gcm->groupid != 0)
				{
					char membername[MAX_GROUP_MEMBERS][64]; // group member names
					bool groupLoaded = false; // only query the database if there are clients zoning

					// iterate client_list looking for clients that are zoning
					std::vector<ClientListEntry *> vec;
					client_list.GetClients("", vec);
					for (auto it = vec.begin(); it != vec.end(); ++it)
					{
						ClientListEntry *cle = *it;
						if (cle)
						{
							uint32 groupid = cle->GroupID();
							if (cle->Online() == CLE_Status_Zoning && groupid == gcm->groupid && !cle->TellQueueFull())
							{
								// queue group chat message
								size_t struct_size = sizeof(ServerChannelMessage_Struct) + strlen(gcm->message) + 1;
								ServerChannelMessage_Struct *scm = (ServerChannelMessage_Struct *) new uchar[struct_size];
								memset(scm, 0, struct_size);
								strcpy(scm->deliverto, cle->name());
								strcpy(scm->from, gcm->from);
								scm->chan_num = ChatChannel_Group;
								scm->language = gcm->language;
								scm->lang_skill = gcm->lang_skill;
								strcpy(scm->message, gcm->message);
								scm->queued = 1;
								cle->PushToTellQueue(scm); // deallocation is handled in processing or deconstructor
							}
						}
					}
				}

				break;
			}

			case ServerOP_DisbandGroup: {
				if(pack->size != sizeof(ServerDisbandGroup_Struct))
					break;
				ServerDisbandGroup_Struct *sdbg = (ServerDisbandGroup_Struct *) pack->pBuffer;
				if (sdbg->groupid > 0)
					client_list.ClearGroup(sdbg->groupid); // clear groupid from all matching CLE's
				zoneserver_list.SendPacket(pack); //bounce it to all zones
				break;
			}

			case ServerOP_ChangeGroupLeader: {
				if(pack->size != sizeof(ServerGroupLeader_Struct))
					break;
				zoneserver_list.SendPacket(pack); //bounce it to all zones
				break;
			}

			case ServerOP_CheckGroupLeader: {
				if (pack->size != sizeof(ServerGroupLeader_Struct))
					break;

				ServerGroupLeader_Struct *sgls = (ServerGroupLeader_Struct *)pack->pBuffer;
				ClientListEntry* cle = client_list.FindCharacter(sgls->leader_name);

				if (cle)
				{
					zoneserver_list.SendPacket(pack); //bounce it to all zones
				}
				else
				{
					Log(Logs::General, Logs::WorldServer, "Character %s is not found, group ownership will not be transferred.", sgls->leader_name);
				}

				break;
			}

			case ServerOP_RaidAdd:{
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidRemove: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_RaidRemoveLD: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidDisband: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidChangeGroup: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_UpdateGroup: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidGroupDisband: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidGroupAdd: {
				if(pack->size != sizeof(ServerRaidGroupAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidGroupRemove: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidGroupSay:
			case ServerOP_RaidSay: {
				zoneserver_list.SendPacket(pack); // broadcast to zones

				// Temp disabled: implement raid / raid group say in tell queue.
				// check for characters zoning and queue messages
				//ServerRaidMessage_Struct* srm = (ServerRaidMessage_Struct*)pack->pBuffer;
				//if (srm->rid != 0)
				//{
				//	std::vector<ClientListEntry *> vec;
				//	client_list.GetClients("", vec);
				//	for (auto it = vec.begin(); it != vec.end(); ++it)
				//	{
				//		ClientListEntry *cle = *it;
				//		if (cle->Online() == CLE_Status_Zoning && !cle->TellQueueFull())
				//		{
				//			uint32 raidid, groupid;
				//			if (database.GetRaidGroupID(cle->name(), &raidid, &groupid))
				//			{
				//				if (raidid == srm->rid && (pack->opcode == ServerOP_RaidSay || groupid == srm->gid))
				//				{
				//					// queue chat message
				//					size_t struct_size = sizeof(ServerChannelMessage_Struct) + strlen(srm->message) + 1;
				//					ServerChannelMessage_Struct *scm = (ServerChannelMessage_Struct *) new uchar[struct_size];
				//					memset(scm, 0, struct_size);
				//					strcpy(scm->deliverto, cle->name());
				//					strcpy(scm->from, srm->from);
				//					strcpy(scm->message, srm->message);
				//					scm->language = srm->language;
				//					scm->lang_skill = srm->lang_skill;
				//					scm->chan_num = pack->opcode == ServerOP_RaidGroupSay ? ChatChannel_Group : ChatChannel_Raid;
				//					scm->queued = 1;
				//					cle->PushToTellQueue(scm); // deallocation is handled in processing or deconstructor
				//				}
				//			}
				//		}
				//	}
				//}

				break;
			}

			case ServerOP_RaidGroupLeader: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidLeader: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidTypeChange: {
				if (pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RaidAddLooter: {
				if (pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RemoveRaidLooter: {
				if (pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_DetailsChange: {
				if(pack->size != sizeof(ServerRaidGeneralAction_Struct))
					break;

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_SpawnCondition: {
				if(pack->size != sizeof(ServerSpawnCondition_Struct))
					break;
				//bounce the packet to the correct zone server, if its up
				ServerSpawnCondition_Struct* ssc = (ServerSpawnCondition_Struct*)pack->pBuffer;
				zoneserver_list.SendPacket(ssc->zoneID, 0, pack);
				break;
			}
			case ServerOP_SpawnEvent: {
				if(pack->size != sizeof(ServerSpawnEvent_Struct))
					break;
				//bounce the packet to the correct zone server, if its up
				ServerSpawnEvent_Struct* sse = (ServerSpawnEvent_Struct*)pack->pBuffer;
				zoneserver_list.SendPacket(sse->zoneID, 0, pack);
				break;
			}
			case ServerOP_ChannelMessage: {
				if (pack->size < sizeof(ServerChannelMessage_Struct))
					break;
				ServerChannelMessage_Struct* scm = (ServerChannelMessage_Struct*) pack->pBuffer;
				if(scm->chan_num == 20)
				{
					UCSLink.SendMessage(scm->from, scm->message);
					break;
				}
				// tells get a reply echo
				if (scm->chan_num == ChatChannel_Tell || scm->chan_num == ChatChannel_TellEcho)
				{
					// deliver to console user
					if (scm->deliverto[0] == '*') {
						Console* con = 0;
						con = console_list.FindByAccountName(&scm->deliverto[1]);
						if (((!con) || (!con->SendChannelMessage(scm))) && (scm->chan_num == ChatChannel_Tell && scm->queued == 0))
							zoneserver_list.SendEmoteMessage(scm->from, 0, AccountStatus::Player, Chat::White, fmt::format(" {} is not online at this time ", scm->to).c_str());
						break;
					}

					// deliver to client
					ClientListEntry* cle = client_list.FindCharacter(scm->deliverto);

					if (cle == 0 || cle->Online() < CLE_Status_Zoning ||
							(cle->TellsOff() && (scm->fromadmin < cle->Admin() || scm->fromadmin < 80))) {
						// client not found or has tells off
						if (scm->chan_num == ChatChannel_Tell) {
							ClientListEntry* sender = client_list.FindCharacter(scm->from);
							if (!sender || !sender->Server())
								break;
							scm->chan_num = ChatChannel_TellEcho;
							scm->queued = 3; // offline
							strcpy(scm->deliverto, scm->from);
							// ideally this would be trimming off the message too, oh well
							sender->Server()->SendPacket(pack);
						}
					} else if (cle->Online() == CLE_Status_Zoning) {
						if (scm->chan_num == ChatChannel_Tell) {
							ClientListEntry* sender = client_list.FindCharacter(scm->from);
							if (cle->TellQueueFull()) {
								if (!sender || !sender->Server())
									break;
								scm->chan_num = ChatChannel_TellEcho;
								scm->queued = 2; // queue full
								strcpy(scm->deliverto, scm->from);
								sender->Server()->SendPacket(pack);
							} else {
								if (cle && sender->Revoked() && cle->Admin() <= 0)
									break;

								size_t struct_size = sizeof(ServerChannelMessage_Struct) + strlen(scm->message) + 1;
								ServerChannelMessage_Struct *temp = (ServerChannelMessage_Struct *) new uchar[struct_size];
								memset(temp, 0, struct_size); // just in case, was seeing some corrupt messages, but it shouldn't happen
								memcpy(temp, scm, struct_size);
								temp->queued = 1;
								cle->PushToTellQueue(temp); // deallocation is handled in processing or deconstructor

								if (!sender || !sender->Server())
									break;
								scm->chan_num = ChatChannel_TellEcho;
								scm->queued = 1; // queued
								strcpy(scm->deliverto, scm->from);
								sender->Server()->SendPacket(pack);
							}
						}
					}
					else if (cle->Server() == 0) {
						if (scm->chan_num == ChatChannel_Tell)
							zoneserver_list.SendEmoteMessage(scm->from, 0, AccountStatus::Player, Chat::Default, fmt::format(" {} is not contactable at this time'", scm->to).c_str());
					}
					else
					{
						if (scm->chan_num == ChatChannel_Tell) {
							ClientListEntry* sender = client_list.FindCharacter(scm->from);
							if (cle && sender && sender->Revoked() && cle->Admin() <= 0)
							{
								if (scm->chan_num == ChatChannel_Tell)
									zoneserver_list.SendEmoteMessage(scm->from, 0, AccountStatus::Player, Chat::Default, "You are server muted, and aren't able to send a message to anyone but a CSR.");
								break;
							}
						}
						cle->Server()->SendPacket(pack);
					}
				}
				else {
					if (scm->chan_num == ChatChannel_OOC || scm->chan_num == ChatChannel_Broadcast || scm->chan_num == ChatChannel_GMSAY) {
						console_list.SendChannelMessage(scm);
					}
					zoneserver_list.SendPacket(pack); // broadcast to zones

					if (scm->chan_num == ChatChannel_Guild || scm->chan_num == ChatChannel_GMSAY || scm->chan_num == ChatChannel_Broadcast)
					{
						// check for characters zoning and see if they would be eligible for this message, and if so, queue message
						std::vector<ClientListEntry *> vec;
						client_list.GetClients("", vec);
						for (auto it = vec.begin(); it != vec.end(); ++it)
						{
							ClientListEntry *cle = *it;
							if (cle->Online() == CLE_Status_Zoning && !cle->TellQueueFull())
							{
								// is this client one of the targets of this message?
								if
								( 
									(scm->chan_num == ChatChannel_Guild && scm->guilddbid == cle->GuildID()) ||
									(scm->chan_num == ChatChannel_GMSAY && cle->Admin() >= 80 ) ||
									scm->chan_num == ChatChannel_Broadcast
								)
								{
									// queue chat message
									size_t struct_size = sizeof(ServerChannelMessage_Struct) + strlen(scm->message) + 1;
									ServerChannelMessage_Struct *scm2 = (ServerChannelMessage_Struct *) new uchar[struct_size];
									memset(scm2, 0, struct_size);
									strcpy(scm2->deliverto, cle->name());
									strcpy(scm2->from, scm->from);
									scm2->chan_num = scm->chan_num;
									scm2->language = scm->language;
									scm2->lang_skill = scm->lang_skill;
									strcpy(scm2->message, scm->message);
									scm2->queued = 1;
									cle->PushToTellQueue(scm2); // deallocation is handled in processing or deconstructor
								}
							}
						}
					}
				}
				break;
			}
			case ServerOP_EmoteMessage: {
				ServerEmoteMessage_Struct* sem = (ServerEmoteMessage_Struct*) pack->pBuffer;
				zoneserver_list.SendEmoteMessageRaw(sem->to, sem->guilddbid, sem->minstatus, sem->type, sem->message);
				break;
			}

			case ServerOP_RezzPlayerAccept: {
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_RezzPlayer: {

				RezzPlayer_Struct* sRezz = (RezzPlayer_Struct*) pack->pBuffer;
				if (zoneserver_list.SendPacket(pack)){
					Log(Logs::Detail, Logs::WorldServer,"Sent Rez packet for %s",sRezz->rez.your_name);
				}
				else {
					Log(Logs::Detail, Logs::WorldServer,"Could not send Rez packet for %s",sRezz->rez.your_name);
				}
				break;
			}
			case ServerOP_RezzPlayerReject:
			{
				char *Recipient = (char *)pack->pBuffer;
				client_list.SendPacket(Recipient, pack);
				break;
			}

			case ServerOP_MultiLineMsg: {
				ServerMultiLineMsg_Struct* mlm = (ServerMultiLineMsg_Struct*) pack->pBuffer;
				client_list.SendPacket(mlm->to, pack);
				break;
			}
			case ServerOP_SetZone: {
				if(pack->size != sizeof(SetZone_Struct))
					break;

				SetZone_Struct* szs = (SetZone_Struct*) pack->pBuffer;
				if (szs->zoneid != 0) {
					if(database.GetZoneName(szs->zoneid))
						SetZone(szs->zoneid, szs->staticzone, szs->zoneguildid);
					else
						SetZone(0);
				}
				else
					SetZone(0);

				break;
			}
			case ServerOP_SetConnectInfo: {
				if (pack->size != sizeof(ServerConnectInfo))
						break;
				ServerConnectInfo* sci = (ServerConnectInfo*) pack->pBuffer;

				if (!sci->port) {
					client_port = zoneserver_list.GetAvailableZonePort();

					ServerPacket p(ServerOP_SetConnectInfo, sizeof(ServerConnectInfo));
					memset(p.pBuffer,0,sizeof(ServerConnectInfo));
					ServerConnectInfo* sci = (ServerConnectInfo*) p.pBuffer;
					sci->port = client_port;
					SendPacket(&p);
					Log(Logs::Detail, Logs::WorldServer,"Auto zone port configuration. Telling zone to use port %d",client_port);
				} else {
					client_port = sci->port;
					Log(Logs::Detail, Logs::WorldServer,"Zone specified port %d.",client_port);
				}

				if(sci->address[0]) {
					strn0cpy(client_address, sci->address, 250);
					Log(Logs::Detail, Logs::WorldServer, "Zone specified address %s.", sci->address);
				}

				if(sci->local_address[0]) {
					strn0cpy(client_local_address, sci->local_address, 250);
					Log(Logs::Detail, Logs::WorldServer, "Zone specified local address %s.", sci->address);
				}

				if (sci->process_id){
					zone_os_process_id = sci->process_id;
				}
				break;
			}
			case ServerOP_SetLaunchName: {
				if(pack->size != sizeof(LaunchName_Struct))
					break;
				const LaunchName_Struct* ln = (const LaunchName_Struct*)pack->pBuffer;
				launcher_name = ln->launcher_name;
				launched_name = ln->zone_name;
				database.ZoneConnected(database.GetZoneID(ln->zone_name), ln->zone_name);
				Log(Logs::Detail, Logs::WorldServer, "Zone started with name %s by launcher %s", launched_name.c_str(), launcher_name.c_str());
				break;
			}
			case ServerOP_ShutdownAll: {
				if(pack->size==0){
					zoneserver_list.SendPacket(pack);
					zoneserver_list.Process();
					CatchSignal(2);
				}
				else{
					WorldShutDown_Struct* wsd=(WorldShutDown_Struct*)pack->pBuffer;
					if(wsd->time==0 && wsd->interval==0 && zoneserver_list.shutdowntimer->Enabled()){
						zoneserver_list.shutdowntimer->Disable();
						zoneserver_list.reminder->Disable();
					}
					else{
						zoneserver_list.shutdowntimer->Start(wsd->time);
						zoneserver_list.reminder->Start(wsd->interval - 1000);
						zoneserver_list.reminder->SetDuration(wsd->interval);
					}
				}
				break;
			}
			case ServerOP_ZoneShutdown: {
				ServerZoneStateChange_struct* s = (ServerZoneStateChange_struct *) pack->pBuffer;
				ZoneServer* zs = 0;
				if (s->ZoneServerID != 0)
					zs = zoneserver_list.FindByID(s->ZoneServerID);
				else if (s->zoneid != 0)
					zs = zoneserver_list.FindByName(database.GetZoneName(s->zoneid));
				else
					zoneserver_list.SendEmoteMessage(s->adminname, 0, AccountStatus::Player, Chat::White, "Error: SOP_ZoneShutdown: neither ID nor name specified");

				if (zs == 0)
					zoneserver_list.SendEmoteMessage(s->adminname, 0, AccountStatus::Player, Chat::White, "Error: SOP_ZoneShutdown: zoneserver not found");
				else
					zs->SendPacket(pack);
				break;
			}
			case ServerOP_ZoneBootup: {
				ServerZoneStateChange_struct* s = (ServerZoneStateChange_struct *) pack->pBuffer;
				uint32 ZoneServerID = s->ZoneServerID;
				if (ZoneServerID == 0)
				{
					ZoneServerID = zoneserver_list.GetAvailableZoneID();
					if (ZoneServerID == 0)
					{
						break;
					}
				}
				zoneserver_list.SOPZoneBootup(s->adminname, ZoneServerID, s->ZoneServerGuildID, database.GetZoneName(s->zoneid), s->makestatic);
				break;
			}
			case ServerOP_ZoneStatus: {
				if (pack->size >= 1)
					zoneserver_list.SendZoneStatus((char *) &pack->pBuffer[1], (uint8) pack->pBuffer[0], this);
				break;

			}
			case ServerOP_AcceptWorldEntrance: {
				if(pack->size != sizeof(WorldToZone_Struct))
					break;

				WorldToZone_Struct* wtz = (WorldToZone_Struct*) pack->pBuffer;
				Client* client = 0;
				client = client_list.FindByAccountID(wtz->account_id);
				if(client != 0)
					client->Clearance(wtz->response);
				break;
			}
			case ServerOP_ZoneToZoneRequest: {
				//
				// solar: ZoneChange is received by the zone the player is in, then the
				// zone sends a ZTZ which ends up here. This code then find the target
				// (ingress point) and boots it if needed, then sends the ZTZ to it.
				// The ingress server will decide wether the player can enter, then will
				// send back the ZTZ to here. This packet is passed back to the egress
				// server, which will send a ZoneChange response back to the client
				// which can be an error, or a success, in which case the client will
				// disconnect, and their zone location will be saved when ~Client is
				// called, so it will be available when they ask to zone.
				//


				if(pack->size != sizeof(ZoneToZone_Struct))
					break;
				ZoneToZone_Struct* ztz = (ZoneToZone_Struct*) pack->pBuffer;
				ClientListEntry* client = nullptr;
				if(WorldConfig::get()->UpdateStats)
					client = client_list.FindCharacter(ztz->name);

				Log(Logs::Detail, Logs::WorldServer,"ZoneToZone request for %s current zone %d req zone %d",
					ztz->name, ztz->current_zone_id, ztz->requested_zone_id);

				/* This is a request from the egress zone */
				if(GetZoneID() == ztz->current_zone_id && GetZoneGuildID() == ztz->current_zone_guild_id) {
					Log(Logs::Detail, Logs::WorldServer,"Processing ZTZ for egress from zone for client %s", ztz->name);

					if (ztz->admin < 80 && ztz->ignorerestrictions < 2 && zoneserver_list.IsZoneLocked(ztz->requested_zone_id)) {
						ztz->response = 0;
						SendPacket(pack);
						break;
					}

					ZoneServer *ingress_server = nullptr;
					ingress_server = zoneserver_list.FindByZoneID(ztz->requested_zone_id, ztz->requested_zone_guild_id); 

					/* Zone was already running*/
					if(ingress_server) {
						Log(Logs::Detail, Logs::WorldServer,"Found a zone already booted for %s", ztz->name);
						ztz->response = 1;
					}
					/* Boot the Zone*/
					else {
						int server_id;
						if (!RuleB(World, DontBootDynamics) || ztz->requested_zone_guild_id != 0xFFFFFFFF)
						{
							if ((server_id = zoneserver_list.TriggerBootup(ztz->requested_zone_id, ztz->requested_zone_guild_id))) {
								Log(Logs::Detail, Logs::WorldServer, "Successfully booted a zone for %s", ztz->name);
								// bootup successful, ready to rock
								ztz->response = 1;
								ingress_server = zoneserver_list.FindByID(server_id);
							}
							else 
							{
								Log(Logs::Detail, Logs::WorldServer, "FAILED to boot a zone for %s", ztz->name);
								// bootup failed, send back error code 0
								ztz->response = 0;
							}
						}
						else {
							Log(Logs::Detail, Logs::WorldServer,"Zone for %s won't boot due to rule.", ztz->name);
							// bootup failed, send back error code 0
							ztz->response = 0;
						}
					}
					if(ztz->response!=0 && client)
						client->LSZoneChange(ztz);
						SendPacket(pack);	// send back to egress server
						if(ingress_server) {
							ingress_server->SendPacket(pack);	// inform target server
						}
				}
				/* Response from Ingress server, route back to egress */
				else{
				
					Log(Logs::Detail, Logs::WorldServer,"Processing ZTZ for ingress to zone for client %s", ztz->name);
					ZoneServer *egress_server = nullptr;
					egress_server = zoneserver_list.FindByZoneID(ztz->current_zone_id, ztz->current_zone_guild_id);

					if(egress_server) {
						egress_server->SendPacket(pack);
					}
				}

				break;
			}
			case ServerOP_ClientList: {
				if (pack->size != sizeof(ServerClientList_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_ClientList. Got: %d, Expected: %d",pack->size,sizeof(ServerClientList_Struct));
					break;
				}
				client_list.ClientUpdate(this, (ServerClientList_Struct*) pack->pBuffer);
				break;
			}
			case ServerOP_ClientListKA: {
				ServerClientListKeepAlive_Struct* sclka = (ServerClientListKeepAlive_Struct*) pack->pBuffer;
				if (pack->size < 4 || pack->size != 4 + (4 * sclka->numupdates)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_ClientListKA. Got: %d, Expected: %d",pack->size, (4 + (4 * sclka->numupdates)));
					break;
				}
				client_list.CLEKeepAlive(sclka->numupdates, sclka->wid);
				break;
			}
			case ServerOP_Who: {
				ServerWhoAll_Struct* whoall = (ServerWhoAll_Struct*) pack->pBuffer;
				auto whom = new Who_All_Struct;
				memset(whom,0,sizeof(Who_All_Struct));
				whom->gmlookup = whoall->gmlookup;
				whom->lvllow = whoall->lvllow;
				whom->lvlhigh = whoall->lvlhigh;
				whom->wclass = whoall->wclass;
				whom->wrace = whoall->wrace;
				whom->guildid = whoall->guildid;
				strcpy(whom->whom,whoall->whom);
				client_list.SendWhoAll(whoall->fromid,whoall->from, whoall->admin, whom);
				safe_delete(whom);
				break;
			}
			case ServerOP_RequestOnlineGuildMembers: {
				ServerRequestOnlineGuildMembers_Struct *srogms = (ServerRequestOnlineGuildMembers_Struct*) pack->pBuffer;
				Log(Logs::Detail, Logs::Guilds, "ServerOP_RequestOnlineGuildMembers Recieved. FromID=%i GuildID=%i", srogms->FromID, srogms->GuildID);
				client_list.SendOnlineGuildMembers(srogms->FromID, srogms->GuildID);
				break;
			}
			case ServerOP_ClientVersionSummary: {
				ServerRequestClientVersionSummary_Struct *srcvss = (ServerRequestClientVersionSummary_Struct*) pack->pBuffer;
				client_list.SendClientVersionSummary(srcvss->Name);
				break;
			}
			case ServerOP_ReloadLogs: {
				zoneserver_list.SendPacket(pack);
				LogSys.LoadLogDatabaseSettings();
				break;
			}
			case ServerOP_ReloadRules: {
				zoneserver_list.SendPacket(pack);
				database.LoadZoneFileNames();
				RuleManager::Instance()->LoadRules(&database, "default");
				break;
			}
			case ServerOP_ReloadSpellModifiers: {
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_ReloadContentFlags: {
				zoneserver_list.SendPacket(pack);
				content_service.SetExpansionContext()->ReloadContentFlags();
				break;
			}
			case ServerOP_ReloadRulesWorld:
			{
				database.LoadZoneFileNames();
				RuleManager::Instance()->LoadRules(&database, "default");
				break;
			}
			case ServerOP_FriendsWho: {
				ServerFriendsWho_Struct* FriendsWho = (ServerFriendsWho_Struct*) pack->pBuffer;
				client_list.SendFriendsWho(FriendsWho, this);
				break;
			}
			case ServerOP_ZonePlayer: {
				//ServerZonePlayer_Struct* szp = (ServerZonePlayer_Struct*) pack->pBuffer;
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_KickPlayer: {
				ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)pack->pBuffer;
				ClientListEntry* cle = client_list.FindCLEByAccountID(skp->AccountID);
				if (cle) {
					cle->SetOnline(CLE_Status_Offline);
					client_list.RemoveCLEByAccountID(cle->AccountID());
				}

				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_KillPlayer: {
				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_GuildRankUpdate:
			{
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_IsOwnerOnline: {
				if(pack->size != sizeof(ServerIsOwnerOnline_Struct))
					break;
				ServerIsOwnerOnline_Struct* online = (ServerIsOwnerOnline_Struct*) pack->pBuffer;
				ClientListEntry* cle = client_list.FindCLEByAccountID(online->accountid);
				if(cle)
					online->online = 1;
				else
					online->online = 0;
				auto target_zone = zoneserver_list.FindByZoneID(online->zoneid, online->zoneguildid);
				if (target_zone)
					SendPacket(pack);
				break;
			}
			//these opcodes get processed by the guild manager.
			case ServerOP_RefreshGuild:
			case ServerOP_DeleteGuild:
			case ServerOP_GuildCharRefresh:{
				guild_mgr.ProcessZonePacket(pack);
				break;
			}

			case ServerOP_FlagUpdate: {
				ClientListEntry* cle = client_list.FindCLEByAccountID(*((uint32*) pack->pBuffer));
				if (cle)
					cle->SetAdmin(*((int16*) &pack->pBuffer[4]));
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_GMGoto: {
				if (pack->size != sizeof(ServerGMGoto_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_GMGoto. Got: %d, Expected: %d",pack->size,sizeof(ServerGMGoto_Struct));
					break;
				}
				ServerGMGoto_Struct* gmg = (ServerGMGoto_Struct*) pack->pBuffer;
				ClientListEntry* cle = client_list.FindCharacter(gmg->gotoname);
				if (cle != 0) {
					if (cle->Server() == 0)
						this->SendEmoteMessage(gmg->myname, 0, AccountStatus::Player, Chat::Red, fmt::format("Error: Cannot identify {}'s zoneserver.", gmg->gotoname).c_str());
					else if (cle->Anon() == 1 && cle->Admin() > gmg->admin) // no snooping for anon GMs
						this->SendEmoteMessage(gmg->myname, 0, AccountStatus::Player, Chat::Red, fmt::format("Error: {} not found", gmg->gotoname).c_str());
					else
					{
						gmg->guildinstanceid = cle->GetZoneGuildID();
						cle->Server()->SendPacket(pack);
					}
				}
				else {
					this->SendEmoteMessage(gmg->myname, 0, AccountStatus::Player, Chat::Red, fmt::format("Error: {} not found", gmg->gotoname).c_str());
				}
				break;
			}
			case ServerOP_Lock: {
				if (pack->size != sizeof(ServerLock_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_Lock. Got: %d, Expected: %d",pack->size,sizeof(ServerLock_Struct));
					break;
				}
				auto l = (ServerLock_Struct*) pack->pBuffer;
				if (l->is_locked)
					WorldConfig::LockWorld();
				else
					WorldConfig::UnlockWorld();
				if (loginserverlist.Connected()) {
					loginserverlist.SendStatus();
					SendEmoteMessage(l->character_name, 0, AccountStatus::Player, Chat::Yellow, fmt::format("World {}.", l->is_locked ? "locked" : "unlocked").c_str());
				}
				else {
					SendEmoteMessage(l->character_name, 0, AccountStatus::Player, Chat::Yellow, fmt::format("World {}, but login server not connected.", l->is_locked ? "locked" : "unlocked").c_str());
				}
				break;
								}
			case ServerOP_Motd: {
				if (pack->size != sizeof(ServerMotd_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_Motd. Got: %d, Expected: %d",pack->size,sizeof(ServerMotd_Struct));
					break;
				}
				ServerMotd_Struct* smotd = (ServerMotd_Struct*) pack->pBuffer;
				database.SetVariable("MOTD",smotd->motd);
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_Uptime: {
				if (pack->size != sizeof(ServerUptime_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_Uptime. Got: %d, Expected: %d",pack->size,sizeof(ServerUptime_Struct));
					break;
				}
				ServerUptime_Struct* sus = (ServerUptime_Struct*) pack->pBuffer;
				if (sus->zoneserverid == 0) {
					ZSList::ShowUpTime(this, sus->adminname);
				}
				else {
					ZoneServer* zs = zoneserver_list.FindByID(sus->zoneserverid);
					if (zs)
						zs->SendPacket(pack);
				}
				break;
			}
			case ServerOP_Petition: {
				zoneserver_list.SendPacket(pack);
				QSLink.SendPacket(pack);
				break;
			}
			case ServerOP_GetWorldTime: {
				Log(Logs::Detail, Logs::WorldServer,"Broadcasting a world time update");
				auto outpack = new ServerPacket;

				outpack->opcode = ServerOP_SyncWorldTime;
				outpack->size = sizeof(eqTimeOfDay);
				outpack->pBuffer = new uchar[outpack->size];
				memset(outpack->pBuffer, 0, outpack->size);
				eqTimeOfDay* tod = (eqTimeOfDay*) outpack->pBuffer;
				tod->start_eqtime=zoneserver_list.worldclock.getStartEQTime();
				tod->start_realtime=zoneserver_list.worldclock.getStartRealTime();
				SendPacket(outpack);
				safe_delete(outpack);
				break;
			}
			case ServerOP_SetWorldTime: {
				Log(Logs::Detail, Logs::WorldServer,"Received SetWorldTime");
				eqTimeOfDay* newtime = (eqTimeOfDay*) pack->pBuffer;
				zoneserver_list.worldclock.setEQTimeOfDay(newtime->start_eqtime, newtime->start_realtime);
				Log(Logs::Detail, Logs::WorldServer,"New time = %d-%d-%d %d:%d (%d)\n", newtime->start_eqtime.year, newtime->start_eqtime.month, (int)newtime->start_eqtime.day, (int)newtime->start_eqtime.hour, (int)newtime->start_eqtime.minute, (int)newtime->start_realtime);
				database.SaveTime((int)newtime->start_eqtime.minute, (int)newtime->start_eqtime.hour, (int)newtime->start_eqtime.day, newtime->start_eqtime.month, newtime->start_eqtime.year);
				zoneserver_list.SendTimeSync();
				break;
			}
			case ServerOP_IPLookup: {
				if (pack->size < sizeof(ServerGenericWorldQuery_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_IPLookup. Got: %d, Expected (at least): %d",pack->size,sizeof(ServerGenericWorldQuery_Struct));
					break;
				}
				ServerGenericWorldQuery_Struct* sgwq = (ServerGenericWorldQuery_Struct*) pack->pBuffer;
				if (pack->size == sizeof(ServerGenericWorldQuery_Struct))
					client_list.SendCLEList(sgwq->admin, sgwq->from, this);
				else
					client_list.SendCLEList(sgwq->admin, sgwq->from, this, sgwq->query);
				break;
			}
			case ServerOP_LockZone: {
				if (pack->size < sizeof(ServerLockZone_Struct)) {
					Log(Logs::Detail, Logs::WorldServer,"Wrong size on ServerOP_LockZone. Got: %d, Expected: %d",pack->size,sizeof(ServerLockZone_Struct));
					break;
				}
				ServerLockZone_Struct* lock_zone = (ServerLockZone_Struct*) pack->pBuffer;
				switch (lock_zone->op) {
					case ServerLockType::List:
						zoneserver_list.ListLockedZones(lock_zone->adminname, this);
						break;
					case ServerLockType::Lock:
						if (zoneserver_list.SetLockedZone(lock_zone->zoneID, true))
							zoneserver_list.SendEmoteMessage(0, 0, AccountStatus::QuestTroupe, Chat::Yellow, fmt::format("Zone locked: {} ", database.GetZoneName(lock_zone->zoneID)).c_str());
						else
							this->SendEmoteMessageRaw(lock_zone->adminname, 0, AccountStatus::Player, Chat::White, "Failed to change lock");
						break;
					case ServerLockType::Unlock:
						if (zoneserver_list.SetLockedZone(lock_zone->zoneID, false))
							zoneserver_list.SendEmoteMessage(0, 0, AccountStatus::QuestTroupe, Chat::Yellow, fmt::format("Zone unlocked: {} ", database.GetZoneName(lock_zone->zoneID)).c_str());
						else
							this->SendEmoteMessageRaw(lock_zone->adminname, 0, AccountStatus::Player, Chat::White, "Failed to change lock");
						break;
				}
				break;
			}
			case ServerOP_ItemStatus: {
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_OOCMute: {
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_Revoke: {
				RevokeStruct* rev = (RevokeStruct*)pack->pBuffer;
				ClientListEntry* cle = client_list.FindCharacter(rev->name);
				if (cle != 0 && cle->Server() != 0)
				{
					cle->Server()->SendPacket(pack);
					cle->SetRevoked(rev->toggle);
				}
				break;
			}
			case ServerOP_SpawnPlayerCorpse: {
				SpawnPlayerCorpse_Struct* s = (SpawnPlayerCorpse_Struct*)pack->pBuffer;
				ZoneServer* zs = zoneserver_list.FindByZoneID(s->zone_id, s->GuildID);
				if(zs) {
					if (zs->SendPacket(pack)) {
						Log(Logs::Detail, Logs::WorldServer,"Sent request to spawn player corpse id %i in zone %u.",s->player_corpse_id, s->zone_id);
					}
					else {
						Log(Logs::Detail, Logs::WorldServer,"Could not send request to spawn player corpse id %i in zone %u.",s->player_corpse_id, s->zone_id);
					}
				}
				break;
			}
			case ServerOP_Consent: 
			{
				bool success = false;
				ZoneServer* zs;
				ServerOP_Consent_Struct* s = (ServerOP_Consent_Struct*)pack->pBuffer;
				ClientListEntry* cle = client_list.FindCharacter(s->grantname);
				if (cle)
				{
					zs = zoneserver_list.FindByZoneID(cle->zone(), cle->GetZoneGuildID());
					if (zs)
					{
						//Sends packet to player granted consent. This causes zone to call Consent() and sends the granted player a success message.
						if (zs->SendPacket(pack))
						{
							success = true;
							ClientListEntry* cle_reply = client_list.FindCharacter(s->ownername);
							if (cle_reply)
							{
								auto reply = new ServerPacket(ServerOP_Consent_Response, sizeof(ServerOP_Consent_Struct));
								ServerOP_Consent_Struct* scs = (ServerOP_Consent_Struct*)reply->pBuffer;
								strcpy(scs->grantname, s->grantname);
								strcpy(scs->ownername, s->ownername);
								scs->permission = s->permission;
								scs->zone_id = s->zone_id;
								scs->message_string_id = 1427; //CONSENT_GIVEN
								scs->corpse_id = s->corpse_id;
								zs = zoneserver_list.FindByZoneID(cle_reply->zone(), cle_reply->GetZoneGuildID());
								if (zs)
								{
									// Sends packet to owner so they get the success message. If this fails, consent will still occur the owner just won't get a message.
									if(zs->SendPacket(reply))
										Log(Logs::Detail, Logs::WorldServer, "Sent consent packet from player %s to player %s in zone %u.", s->ownername, s->grantname, cle->zone());
								}
								safe_delete(reply);
							}
						}
					}
				}

				if (!success)
				{
					// Sends packet back to owner so they can save the consent to the DB. (Granted player is not online or doesn't exist.)
					auto reply = new ServerPacket(ServerOP_Consent_Response, sizeof(ServerOP_Consent_Struct));
					ServerOP_Consent_Struct* scs = (ServerOP_Consent_Struct*)reply->pBuffer;
					strcpy(scs->grantname, s->grantname);
					strcpy(scs->ownername, s->ownername);
					scs->permission = s->permission;
					scs->zone_id = s->zone_id;
					scs->message_string_id = 101; //TARGET_NOT_FOUND
					scs->corpse_id = s->corpse_id;
					zs = zoneserver_list.FindByZoneID(s->zone_id, s->GuildID);
					if (zs)
					{
						if (!zs->SendPacket(reply))
							Log(Logs::Detail, Logs::WorldServer, "ServerOP_Consent: Unable to send consent response back to player %s in zone %s.", s->ownername, zs->GetZoneName());
					}
					else
					{
						Log(Logs::Detail, Logs::WorldServer, "ServerOP_Consent: Unable to locate zone record for zone id %u in zoneserver list for ServerOP_Consent_Response operation.", s->zone_id);
					}
					safe_delete(reply);
				}
				break;
			}
			case ServerOP_Consent_Response: 
			{
				// This just relays the packet back to the owner's zone. 
				ServerOP_Consent_Struct* s = (ServerOP_Consent_Struct*)pack->pBuffer;
				ZoneServer* zs = zoneserver_list.FindByZoneID(s->zone_id, s->GuildID);
				if(zs) 
				{
					if(!zs->SendPacket(pack))
						Log(Logs::Detail, Logs::WorldServer, "ServerOP_Consent_Response: Unable to send consent response back to player %s in zone %s.", s->ownername, zs->GetZoneName());
				}
				else {
					Log(Logs::Detail, Logs::WorldServer, "ServerOP_Consent_Response: Unable to locate zone record for zone id %u in zoneserver list for ServerOP_Consent_Response operation.", s->zone_id);
				}
				break;
			}
			case ServerOP_ConsentDeny:
			{
				if (pack->size != sizeof(ServerOP_ConsentDeny_Struct))
				{
					break;
				}

				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_ConsentDenyByID:
			{
				if (pack->size != sizeof(ServerOP_ConsentDenyByID_Struct))
				{
					Log(Logs::General, Logs::Error, "ServerOP_ConsentDenyByID wrong struct size. Got: %d", pack->size);
					break;
				}

				ServerOP_ConsentDenyByID_Struct* s = (ServerOP_ConsentDenyByID_Struct*)pack->pBuffer;

				LinkedList<ConsentDenied_Struct*> purged_consent;
				database.ClearAllConsented(s->ownername, s->corpse_id, &purged_consent);

				//Creates a packet telling each player their consent has expired, and if they're online also updates their consent_list.
				LinkedListIterator<ConsentDenied_Struct*> iterator(purged_consent);
				iterator.Reset();
				while (iterator.MoreElements())
				{
					ConsentDenied_Struct* cd = iterator.GetData();
					ClientListEntry* cle = client_list.FindCharacter(cd->gname);
					if (cle)
					{
						char gname[64];
						strncpy(gname, cle->name(), 64);
						ServerPacket *scs_pack = new ServerPacket(ServerOP_Consent_Response, sizeof(ServerOP_Consent_Struct));
						ServerOP_Consent_Struct* scs = (ServerOP_Consent_Struct*)scs_pack->pBuffer;
						strcpy(scs->grantname, gname);
						strcpy(scs->ownername, cd->oname);
						scs->permission = 0;
						scs->message_string_id = 2103; //You have been denied permission to drag %1's corpse.
						scs->corpse_id = cd->corpse_id;

						Log(Logs::Detail, Logs::WorldServer, "Created ServerOP_Consent_Response packet. Owner: %s Granted: %s CorpseID: %d", scs->ownername, scs->grantname, scs->corpse_id);
						ZoneServer* zs = zoneserver_list.FindByZoneID(cle->zone(), cle->GetZoneGuildID());
						if (zs)
						{
							scs->zone_id = zs->GetZoneID();
							scs->GuildID = zs->GetZoneGuildID();
							zs->SendPacket(scs_pack);
						}

						safe_delete(scs_pack);
					}
					else
						Log(Logs::Detail, Logs::WorldServer, "Granted player %s not found to send denied consent message.", cd->gname);

					iterator.Advance();
				}

				purged_consent.Clear();
				break;
			}

			case ServerOP_QGlobalUpdate:
			{
				if(pack->size != sizeof(ServerQGlobalUpdate_Struct))
				{
					break;
				}

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_QGlobalDelete:
			{
				if(pack->size != sizeof(ServerQGlobalDelete_Struct))
				{
					break;
				}

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_LSAccountUpdate:
			{
				Log(Logs::Detail, Logs::WorldServer, "Received ServerOP_LSAccountUpdate packet from zone");
				loginserverlist.SendAccountUpdate(pack);
				break;
			}

			case ServerOP_QuakeRequest:
			{
				if (pack->size != sizeof(ServerEarthquakeRequest_Struct))
				{
					break;
				}
				ServerEarthquakeRequest_Struct* s = (ServerEarthquakeRequest_Struct*)pack->pBuffer;
				TriggerManualQuake(s->type);
				break;
			}

			case ServerOP_UCSMailMessage:
			{
				UCSLink.SendPacket(pack);
				break;
			}
			case ServerOP_QSSendQuery:
			case ServerOP_QueryServGeneric:
			case ServerOP_Speech:
			case ServerOP_QSPlayerAARateHourly:
			case ServerOP_QSPlayerAAPurchase:
			case ServerOP_QSPlayerTSEvents:
			case ServerOP_QSPlayerQGlobalUpdates:
			case ServerOP_QSPlayerLogItemDeletes:
			case ServerOP_QSPlayerLogItemMoves:
			case ServerOP_QSPlayerLogMerchantTransactions:
			{
				QSLink.SendPacket(pack);
				break;
			}
			case ServerOP_QSPlayerLootRecords:
			{
				QSLink.SendPacket(pack);
				break;
			}
			case ServerOP_CZSignalClientByName:
			case ServerOP_CZMessagePlayer: 
			case ServerOP_CZSignalClient:
			case ServerOP_DepopAllPlayersCorpses:
			case ServerOP_DepopPlayerCorpse:
			case ServerOP_ReloadTitles:
			case ServerOP_SpawnStatusChange:
			case ServerOP_ReloadWorld:
			case ServerOP_UpdateSpawn:
			{
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_CZSignalNPC:
			{
				CZNPCSignal_Struct* s = (CZNPCSignal_Struct*)pack->pBuffer;
				uint32 zone_id = s->npctype_id / 1000u;						// NPC IDs have the zone IDs in them.  who cares about pets
				ZoneServer* zs = zoneserver_list.FindByZoneID(zone_id, GUILD_NONE);
				if (zs)
					zs->SendPacket(pack);
				break;
			}
			case ServerOP_CZSetEntityVariableByNPCTypeID:
			{
				CZSetEntVarByNPCTypeID_Struct* s = (CZSetEntVarByNPCTypeID_Struct*)pack->pBuffer;
				uint32 zone_id = s->npctype_id / 1000u;
				ZoneServer* zs = zoneserver_list.FindByZoneID(zone_id, GUILD_NONE);
				if (zs)
					zs->SendPacket(pack);
				break;
			}
			case ServerOP_Soulmark:
			{
				ServerRequestSoulMark_Struct *sss = (ServerRequestSoulMark_Struct*)pack->pBuffer;
				ClientListEntry *cle = client_list.FindCharacter(sss->name);
				if (!cle || cle && !cle->Server())
				{
					break;
				}

				std::vector<SoulMarkEntry_Struct> vec;
				database.LoadSoulMarksForClient(database.GetCharacterID(sss->entry.interrogatename), vec);

				if(!vec.empty())
				{
					std::vector<SoulMarkEntry_Struct>::iterator it = vec.begin();
					int i = 0;
					while(it != vec.end() && i < 12)
					{
						sss->entry.entries[i] = (*it);
						i++;
						it++;
					}
					vec.clear();
					cle->Server()->SendPacket(pack);
				}
				break;
			}
			case ServerOP_ChangeSharedMem: {
				std::string hotfix_name = std::string((char*)pack->pBuffer);

				Log(Logs::General, Logs::WorldServer, "Loading items...");
				if(!database.LoadItems(hotfix_name)) {
					Log(Logs::General, Logs::WorldServer, "Error: Could not load item data. But ignoring");
				}

				Log(Logs::General, Logs::WorldServer, "Loading skill caps...");
				if(!database.LoadSkillCaps(hotfix_name)) {
					Log(Logs::General, Logs::WorldServer, "Error: Could not load skill cap data. But ignoring");
				}

				zoneserver_list.SendPacket(pack);
				break;
			}

			case ServerOP_RequestTellQueue:
			{
				ServerRequestTellQueue_Struct* rtq = (ServerRequestTellQueue_Struct*) pack->pBuffer;
				ClientListEntry *cle = client_list.FindCharacter(rtq->name);
				if (!cle || cle->TellQueueEmpty())
					break;

				cle->ProcessTellQueue();
				break;
			}
			case ServerOP_ReloadSkills: {
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_Weather: {
				zoneserver_list.SendPacket(pack);
				break;
			}
			case ServerOP_BootDownZones:
			{
				ServerDownZoneBoot_struct* s = (ServerDownZoneBoot_struct *)pack->pBuffer;
				std::string query = StringFormat("SELECT zone FROM launcher_zones WHERE enabled = 1");
				auto results = database.QueryDatabase(query);
				if (!results.Success()) {
					Log(Logs::General, Logs::Error, "BootDownZones: %s", results.ErrorMessage().c_str());
					break;
				}

				for (auto row = results.begin(); row != results.end(); ++row) 
				{
					std::string zone_name = row[0];
					ZoneServer* zs = zoneserver_list.FindByName(zone_name.c_str());
					if (zs == nullptr)
					{
						uint32 zoneserverid = zoneserver_list.GetAvailableZoneID();
						if (zoneserverid == 0)
						{
							break;
						}

						zoneserver_list.SOPZoneBootup(s->adminname, zoneserverid, 0xFFFFFFFF, zone_name.c_str(), true);
					}
				}

				break;
			}
			default:
			{
				Log(Logs::Detail, Logs::WorldServer, "Unknown ServerOPcode from zone 0x%04x, size %d", pack->opcode, pack->size);
				DumpPacket(pack->pBuffer, pack->size);
				break;
			}
		}
		if (pack) {
			safe_delete(pack);
		}
		else {
			Log(Logs::Detail, Logs::WorldServer, "Zoneserver process attempted to delete pack when pack does not exist.");
		}
	}
	return true;
}

void ZoneServer::SendEmoteMessage(const char* to, uint32 to_guilddbid, int16 to_minstatus, uint32 type, const char* message, ...) {
	if (!message)
		return;
	va_list argptr;
	char buffer[1024];

	va_start(argptr, message);
	vsnprintf(buffer, sizeof(buffer), message, argptr);
	va_end(argptr);
	SendEmoteMessageRaw(to, to_guilddbid, to_minstatus, type, buffer);
}

void ZoneServer::SendEmoteMessageRaw(const char* to, uint32 to_guilddbid, int16 to_minstatus, uint32 type, const char* message) {
	if (!message)
		return;
	auto pack = new ServerPacket;

	pack->opcode = ServerOP_EmoteMessage;
	pack->size = sizeof(ServerEmoteMessage_Struct)+strlen(message)+1;
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerEmoteMessage_Struct* sem = (ServerEmoteMessage_Struct*) pack->pBuffer;

	if (to != 0) {
		strcpy((char *) sem->to, to);
	}
	else {
		sem->to[0] = 0;
	}

	sem->guilddbid = to_guilddbid;
	sem->minstatus = to_minstatus;
	sem->type = type;
	strcpy(&sem->message[0], message);

	pack->Deflate();
	SendPacket(pack);
	delete pack;
}

void ZoneServer::SendGroupIDs() {
	auto pack = new ServerPacket(ServerOP_GroupIDReply, sizeof(ServerGroupIDReply_Struct));
	ServerGroupIDReply_Struct* sgi = (ServerGroupIDReply_Struct*)pack->pBuffer;
	zoneserver_list.NextGroupIDs(sgi->start, sgi->end);
	SendPacket(pack);
	delete pack;
}

void ZoneServer::SendKeepAlive() {
	ServerPacket pack(ServerOP_KeepAlive, 0);
	SendPacket(&pack);
}

void ZoneServer::ChangeWID(uint32 iCharID, uint32 iWID) {
	auto pack = new ServerPacket(ServerOP_ChangeWID, sizeof(ServerChangeWID_Struct));
	ServerChangeWID_Struct* scw = (ServerChangeWID_Struct*) pack->pBuffer;
	scw->charid = iCharID;
	scw->newwid = iWID;
	zoneserver_list.SendPacket(pack);
	delete pack;
}

void ZoneServer::TriggerBootup(uint32 iZoneID, const char* adminname, bool iMakeStatic, uint32 iGuildID) {
	is_booting_up = true;
	zone_server_zone_id = iZoneID;
	zone_server_guild_id = iGuildID;

	auto pack = new ServerPacket(ServerOP_ZoneBootup, sizeof(ServerZoneStateChange_struct));
	ServerZoneStateChange_struct* s = (ServerZoneStateChange_struct *) pack->pBuffer;
	s->ZoneServerID = zone_server_id;
	if (adminname != 0)
		strcpy(s->adminname, adminname);

	if (iZoneID == 0)
		s->zoneid = this->GetZoneID();
	else
		s->zoneid = iZoneID;

	s->ZoneServerGuildID = iGuildID;

	s->makestatic = iMakeStatic;
	SendPacket(pack);
	delete pack;
	if(iGuildID == GUILD_NONE)
		LSBootUpdate(iZoneID);
}

void ZoneServer::IncomingClient(Client* client) {
	is_booting_up = true;
	auto pack = new ServerPacket(ServerOP_ZoneIncClient, sizeof(ServerZoneIncomingClient_Struct));
	ServerZoneIncomingClient_Struct* s = (ServerZoneIncomingClient_Struct*) pack->pBuffer;
	s->zoneid = GetZoneID();
	s->zoneguildid = GetZoneGuildID();
	s->wid = client->GetWID();
	s->ip = client->GetIP();
	s->accid = client->GetAccountID();
	s->admin = client->GetAdmin();
	s->charid = client->GetCharID();
	if (client->GetCLE())
		s->tellsoff = client->GetCLE()->TellsOff();
	strn0cpy(s->charname, client->GetCharName(), sizeof(s->charname));
	strn0cpy(s->lskey, client->GetLSKey(), sizeof(s->lskey));
	s->version = client->GetClientVersionBit();
	SendPacket(pack);
	delete pack;
}
