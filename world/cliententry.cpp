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
#include "../common/eqemu_logsys.h"
#include "cliententry.h"
#include "clientlist.h"
#include "zonelist.h"
#include "login_server.h"
#include "login_server_list.h"
#include "worlddb.h"
#include "zoneserver.h"
#include "world_config.h"
#include "../common/guilds.h"
#include "../common/strings.h"

extern uint32 numplayers;
extern LoginServerList loginserverlist;
extern ZSList zoneserver_list;
extern ClientList		client_list;
extern volatile bool RunLoops;

ClientListEntry::ClientListEntry(uint32 in_id, uint32 iLSID, const char* iLoginName, const char* iLoginKey, int16 iWorldAdmin, uint32 ip, uint8 local, uint8 version, int8 revoked)
: id(in_id)
{
	ClearVars(true);
	incremented_player_count = false;
	pIP = ip;
	pLSID = iLSID;
	if(iLSID > 0)
		paccountid = database.GetAccountIDFromLSID(iLSID, paccountname, &padmin, 0, &pmule);
	strn0cpy(plsname, iLoginName, sizeof(plsname));
	strn0cpy(plskey, iLoginKey, sizeof(plskey));
	pworldadmin = iWorldAdmin;
	plocal=(local==1);
	pversion = version;
	pRevoked = revoked;
}

ClientListEntry::ClientListEntry(uint32 in_id, ZoneServer* iZS, ServerClientList_Struct* scl, int8 iOnline)
: id(in_id)
{
	ClearVars(true);
	incremented_player_count = false;
	pIP = 0;
	pLSID = scl->LSAccountID;
	strn0cpy(plsname, scl->name, sizeof(plsname));
	strn0cpy(plskey, scl->lskey, sizeof(plskey));
	pworldadmin = 0;

	paccountid = scl->AccountID;
	strn0cpy(paccountname, scl->AccountName, sizeof(paccountname));
	padmin = scl->Admin;
	//THIS IS FOR AN ALTERNATE LOGIN METHOD FOR RAPID TESTING. Hardcoded to the PC client because only PCs should be using this 'hackish' login method. Requires password field set in the database.
	pversion = 2;
	pRevoked = scl->Revoked;
	pmule = scl->mule;

	if (iOnline >= CLE_Status_Zoning)
		Update(iZS, scl, iOnline);
	else
		SetOnline(iOnline);
}

ClientListEntry::~ClientListEntry() {
	if (RunLoops) {
		Camp(); // updates zoneserver's numplayers
		client_list.RemoveCLEReferances(this);
	}

	if (incremented_player_count)
	{
		numplayers--;
		incremented_player_count = false;
	}

	SetOnline(CLE_Status_Offline);
	SetAccountID(0);
	for (auto &elem : tell_queue)
		safe_delete_array(elem);
	tell_queue.clear();
}

void ClientListEntry::SetChar(uint32 iCharID, const char* iCharName) {
	pcharid = iCharID;
	strn0cpy(pname, iCharName, sizeof(pname));
}

void ClientListEntry::SetOnline(ZoneServer* iZS, int8 iOnline) {
	if (iZS == this->Server())
		SetOnline(iOnline);
}

void ClientListEntry::SetOnline(int8 iOnline) {
	Log(Logs::Detail, Logs::WorldServer,"SetOnline Account: %i %i -> %i",AccountID(), pOnline, iOnline);

	// this counting method, counts players connected to world.
	if (iOnline >= CLE_Status_Online && pOnline < CLE_Status_Online) {
		if (!incremented_player_count) {
			incremented_player_count = true;
			numplayers++;
		}
	}
	else if (iOnline < CLE_Status_Online && pOnline >= CLE_Status_Online) {
		if (incremented_player_count) {
			incremented_player_count = false;
			numplayers--;
		}
	}

	if (pmule && incremented_player_count && !RuleB(Quarm, IncludeMulesInServerCount))
	{
		incremented_player_count = false;
		numplayers--;
	}

	// this counting method, counts players in zones.
	//if (iOnline >= CLE_Status_Zoning && pOnline < CLE_Status_Zoning)
	//	numplayers++;
	//else if (iOnline < CLE_Status_Zoning && iOnline != CLE_Status_Online && pOnline >= CLE_Status_Zoning) {
	//	numplayers--;
	if (pOnline >= CLE_Status_Zoning && iOnline < CLE_Status_Zoning) {
		pOnline = iOnline;
		Camp();
	}

	if (iOnline != CLE_Status_Online || pOnline < CLE_Status_Online)
		pOnline = iOnline;

	if (pOnline >= CLE_Status_Online)
		stale = 0;
}
void ClientListEntry::LSUpdate(ZoneServer* iZS){
	if(WorldConfig::get()->UpdateStats){
		auto pack = new ServerPacket;
		pack->opcode = ServerOP_LSZoneInfo;
		pack->size = sizeof(ZoneInfo_Struct);
		pack->pBuffer = new uchar[pack->size];
		ZoneInfo_Struct* zone =(ZoneInfo_Struct*)pack->pBuffer;
		zone->count=iZS->NumPlayers();
		zone->zone = iZS->GetZoneID();
		zone->zone_wid = iZS->GetID();
		loginserverlist.SendPacket(pack);
		safe_delete(pack);
	}
}
void ClientListEntry::LSZoneChange(ZoneToZone_Struct* ztz){
	if(WorldConfig::get()->UpdateStats){
		auto pack = new ServerPacket;
		pack->opcode = ServerOP_LSPlayerZoneChange;
		pack->size = sizeof(ServerLSPlayerZoneChange_Struct);
		pack->pBuffer = new uchar[pack->size];
		ServerLSPlayerZoneChange_Struct* zonechange =(ServerLSPlayerZoneChange_Struct*)pack->pBuffer;
		zonechange->lsaccount_id = LSID();
		zonechange->from = ztz->current_zone_id;
		zonechange->to = ztz->requested_zone_id;
		loginserverlist.SendPacket(pack);
		safe_delete(pack);
	}
}
void ClientListEntry::Update(ZoneServer* iZS, ServerClientList_Struct* scl, int8 iOnline) {
	if (pzoneserver != iZS) {
		if (pzoneserver){
			pzoneserver->RemovePlayer();
			LSUpdate(pzoneserver);
		}
		if (iZS){
			iZS->AddPlayer();
			LSUpdate(iZS);
		}
	}
	pzoneserver = iZS;
	pzone = scl->zone;
	pcharid = scl->charid;

	strcpy(pname, scl->name);
	if (paccountid == 0) {
		paccountid = scl->AccountID;
		strcpy(paccountname, scl->AccountName);
		strcpy(plsname, scl->AccountName);
		pIP = scl->IP;
		pLSID = scl->LSAccountID;
		strn0cpy(plskey, scl->lskey, sizeof(plskey));
	}
	padmin = scl->Admin;
	plevel = scl->level;
	pclass_ = scl->class_;
	prace = scl->race;
	panon = scl->anon;
	ptellsoff = scl->tellsoff;
	pguild_id = scl->guild_id;
	pzoneguildid = scl->zoneguildid;
	pLFG = scl->LFG;
	gm = scl->gm;
	pClientVersion = scl->ClientVersion;
	pLD = scl->LD;
	pbaserace = scl->baserace;

	pmule = scl->mule;
	pAFK = scl->AFK;
	pTrader = scl->Trader;
	pRevoked = scl->Revoked;
	pSelfFound = scl->selffound;
	pHardcore = scl->hardcore;
	pSolo = scl->solo;

	// Fields from the LFG Window
	if((scl->LFGFromLevel != 0) && (scl->LFGToLevel != 0)) {
		pLFGFromLevel = scl->LFGFromLevel;
		pLFGToLevel = scl->LFGToLevel;
		pLFGMatchFilter = scl->LFGMatchFilter;
		memcpy(pLFGComments, scl->LFGComments, sizeof(pLFGComments));
	}

	SetOnline(iOnline);
}

void ClientListEntry::LeavingZone(ZoneServer* iZS, int8 iOnline) {
	if (iZS != 0 && iZS != pzoneserver)
		return;
	SetOnline(iOnline);

	if (pzoneserver){
		pzoneserver->RemovePlayer();
		LSUpdate(pzoneserver);
	}
	pzoneserver = 0;
	pzone = 0;
}

void ClientListEntry::ClearVars(bool iAll) {
	if (iAll) {
		pOnline = CLE_Status_Never;
		stale = 0;

		pLSID = 0;
		memset(plsname, 0, sizeof(plsname));
		memset(plskey, 0, sizeof(plskey));
		pworldadmin = 0;

		paccountid = 0;
		memset(paccountname, 0, sizeof(paccountname));
		padmin = AccountStatus::Player;

		for (auto &elem : tell_queue)
			safe_delete_array(elem);
		tell_queue.clear();
	}
	pzoneserver = 0;
	pzone = 0;
	pcharid = 0;
	pgroupid = 0;
	memset(pname, 0, sizeof(pname));
	plevel = 0;
	pclass_ = 0;
	prace = 0;
	panon = 0;
	ptellsoff = 0;
	pguild_id = GUILD_NONE;
	pLFG = false;
	gm = 0;
	pClientVersion = 0;
	pLD;
	pbaserace = 0;
	pAFK = false;
	pTrader = false;
	pRevoked = 0;
}

void ClientListEntry::Camp(ZoneServer* iZS) {
	// this is called when our new state is at char select or less
	// and our current state is zoning or greater
	if (iZS != 0 && iZS != pzoneserver)
		return;

	if (pzoneserver) {
		pzoneserver->RemovePlayer();
		LSUpdate(pzoneserver);
	}

	if (!client_list.ActiveConnection(paccountid, pcharid)) {
		database.ClearAccountActive(paccountid);
		// remove from groups
		Log(Logs::Detail, Logs::WorldServer, "Camp() Removing from groups: %s", this->pname);

		uint32 groupid = database.GetGroupID(this->pname);
		if (groupid > 0) {
			auto pack = new ServerPacket(ServerOP_GroupLeave, sizeof(ServerGroupLeave_Struct));
			ServerGroupLeave_Struct* gl = (ServerGroupLeave_Struct*)pack->pBuffer;
			gl->gid = groupid;
			gl->zoneid = 0;
			gl->zoneguildid = 0;
			strcpy(gl->member_name, this->pname);
			gl->checkleader = true;
			zoneserver_list.SendPacket(pack);
			safe_delete(pack);
		}
		database.SetGroupID(this->pname, 0, CharID(), this->paccountid);

		// remove from raids
		uint32 raidid = database.GetRaidID(this->pname);
		if (raidid > 0) {
			auto pack = new ServerPacket(ServerOP_RaidRemoveLD, sizeof(ServerRaidGeneralAction_Struct));
			ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
			rga->rid = 0;

			std::string query = StringFormat("SELECT groupid, isgroupleader, israidleader, islooter "
				"FROM raid_members WHERE name='%s' and raidid=%lu",
				this->pname, (unsigned long)raidid);
			auto results = database.QueryDatabase(query);
			if (results.Success() && results.RowCount() == 1) {
				auto row = results.begin();
				if (row != results.end()) {
					int groupNum = atoi(row[0]);
					if (groupNum > 11)
						groupNum = 0xFFFFFFFF;
					bool GroupLeader = atoi(row[1]);
					bool RaidLeader = atoi(row[2]);
					bool RaidLooter = atoi(row[3]);

					rga->rid = raidid;
					rga->gid = groupNum;
					rga->zoneid = RaidLeader;
					rga->zoneguildid = 0;
					rga->gleader = GroupLeader;
					rga->looter = RaidLooter;
					strn0cpy(rga->playername, this->pname, 64);
				}
			}
			// delete them from the raid in the db
			query = StringFormat("DELETE FROM raid_members where name='%s'", this->pname);
			results = database.QueryDatabase(query);
			if (rga->rid > 0) {
				// server expects db to already be updated with member removed
				// when ServerOP_RaidRemoveLD arrives at zoneservers
				zoneserver_list.SendPacket(pack);
			}
			safe_delete(pack);
		}
	}
	if (pOnline < CLE_Status_Online)
		stale = RuleI(World, ClientTimeoutStaleAmount); // new state is offline
	else
		stale = 0; // new state is online or charselect

	// this nukes all the values in the cle 
	ClearVars();
}

bool ClientListEntry::CheckStale() {
	stale++;
	if (stale >= RuleI(World, ClientTimeoutStaleAmount)) {
		if (pOnline > CLE_Status_Offline) {
			SetOnline(CLE_Status_Offline);
		}
		else
			return true;
	}
	return false;
}

bool ClientListEntry::CheckAuth(uint32 iLSID, const char* iKey) {
	if (strncmp(plskey, iKey,10) == 0) {
		if (paccountid == 0 && LSID()>0) {
			int16 tmpStatus = WorldConfig::get()->DefaultStatus;
			paccountid = database.CreateAccount(plsname, 0, tmpStatus, LSID());
			if (!paccountid) {
				Log(Logs::Detail, Logs::WorldServer,"Error adding local account for LS login: '%s', duplicate name?" ,plsname);
				return false;
			}
			strn0cpy(paccountname, plsname, sizeof(paccountname));
			padmin = tmpStatus;
		}
		std::string lsworldadmin;
		if (database.GetVariable("honorlsworldadmin", lsworldadmin))
			if (atoi(lsworldadmin.c_str()) == 1 && pworldadmin != 0 && (padmin < pworldadmin || padmin == AccountStatus::Player))
				padmin = pworldadmin;
		return true;
	}
	return false;
}

bool ClientListEntry::CheckAuth(const char* iName, const MD5& iMD5Password) {
	if (LSAccountID() == 0 && strcmp(paccountname, iName) == 0 && pMD5Pass == iMD5Password)
		return true;
	return false;
}

bool ClientListEntry::CheckAuth(uint32 id, const char* iKey, uint32 ip) {
	if (pIP==ip && strncmp(plskey, iKey,10) == 0){
		paccountid = id;
		database.GetAccountFromID(id,paccountname,&padmin,&pRevoked);
		pRevoked = database.CheckRevoked(id);
		return true;
	}
	return false;
}

void ClientListEntry::ProcessTellQueue()
{
	if (!Server())
		return;

	ServerPacket *pack;
	auto it = tell_queue.begin();
	while (it != tell_queue.end()) {
		pack = new ServerPacket(ServerOP_ChannelMessage, sizeof(ServerChannelMessage_Struct) + strlen((*it)->message) + 1);
		memcpy(pack->pBuffer, *it, pack->size);
		pack->Deflate();
		Server()->SendPacket(pack);
		safe_delete(pack);
		safe_delete_array(*it);
		it = tell_queue.erase(it);
	}
	return;
}
