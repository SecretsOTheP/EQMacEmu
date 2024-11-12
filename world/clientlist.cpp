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
#include "clientlist.h"
#include "zoneserver.h"
#include "zonelist.h"
#include "client.h"
#include "console.h"
#include "worlddb.h"
#include "../common/strings.h"
#include "../common/guilds.h"
#include "../common/races.h"
#include "../common/classes.h"
#include "../common/packet_dump.h"
#include "wguild_mgr.h"
#include "../zone/string_ids.h"

#include <set>

extern ConsoleList		console_list;
extern ZSList			zoneserver_list;
uint32 numplayers = 0;	//this really wants to be a member variable of ClientList...

ClientList::ClientList()
: CLStale_timer(RuleI(World, WorldClientLinkdeadMS))
{
	NextCLEID = 1;
}

ClientList::~ClientList() {
}

void ClientList::Process() {

	if (CLStale_timer.Check())
		CLCheckStale();

	LinkedListIterator<Client*> iterator(list);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (!iterator.GetData()->Process()) {
			struct in_addr in;
			in.s_addr = iterator.GetData()->GetIP();
			Log(Logs::Detail, Logs::WorldServer,"Removing client from %s:%d", inet_ntoa(in), iterator.GetData()->GetPort());
			uint32 accountid = iterator.GetData()->GetAccountID();
			iterator.RemoveCurrent();

			if(!ActiveConnection(accountid))
				database.ClearAccountActive(accountid);
		}
		else
			iterator.Advance();
	}
}

bool ClientList::ActiveConnection(uint32 account_id) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->AccountID() == account_id && iterator.GetData()->Online() > CLE_Status_Offline) {
			struct in_addr in;
			in.s_addr = iterator.GetData()->GetIP();
			Log(Logs::Detail, Logs::WorldServer,"Client with account %d exists on %s", iterator.GetData()->AccountID(), inet_ntoa(in));
			return true;
		}
		iterator.Advance();
	}
	return false;
}

bool ClientList::ActiveConnection(uint32 account_id, uint32 character_id) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while (iterator.MoreElements()) {
		if (iterator.GetData()->AccountID() == account_id && iterator.GetData()->CharID() == character_id && iterator.GetData()->Online() > CLE_Status_CharSelect) {
			struct in_addr in;
			in.s_addr = iterator.GetData()->GetIP();
			Log(Logs::Detail, Logs::WorldServer, "Client with account %d exists on %s", iterator.GetData()->AccountID(), inet_ntoa(in));
			return true;
		}
		iterator.Advance();
	}
	return false;
}

void ClientList::CLERemoveZSRef(ZoneServer* iZS) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->Server() == iZS) {
			iterator.GetData()->ClearServer(); // calling this before LeavingZone() makes CLE not update the number of players in a zone
			iterator.GetData()->LeavingZone();
		}
		iterator.Advance();
	}
}

ClientListEntry* ClientList::GetCLE(uint32 iID) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->GetID() == iID) {
			return iterator.GetData();
		}
		iterator.Advance();
	}
	return 0;
}

//Account Limiting Code to limit the number of characters allowed on from a single account at once.
bool ClientList::EnforceSessionLimit(uint32 iLSAccountID) {

	ClientListEntry* ClientEntry = 0;

	LinkedListIterator<ClientListEntry*> iterator(clientlist, BACKWARD);

	int CharacterCount = 1;

	iterator.Reset();

	while(iterator.MoreElements()) {

		ClientEntry = iterator.GetData();

		if ((ClientEntry->LSAccountID() == iLSAccountID) &&
			((ClientEntry->Admin() <= (RuleI(World, ExemptAccountLimitStatus))) || (RuleI(World, ExemptAccountLimitStatus) < 0))) 
		{

			if(strlen(ClientEntry->name()) && !ClientEntry->LD()) 
			{
				CharacterCount++;
			}

			if (CharacterCount > (RuleI(World, AccountSessionLimit)))
			{
				Log(Logs::Detail, Logs::WorldServer,"LSAccount %d has a CharacterCount of: %d.", iLSAccountID, CharacterCount);
				return true;
			}
		}
		iterator.Advance();
	}

	return false;
}


//Check current CLE Entry IPs against incoming connection

void ClientList::GetCLEIP(uint32 iIP) {

	ClientListEntry* countCLEIPs = 0;
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	int IPInstances = 0;
	iterator.Reset();

	while(iterator.MoreElements()) {

		countCLEIPs = iterator.GetData();
		int exemptcount = database.CheckExemption(iterator.GetData()->AccountID());

		// If the IP matches, and the connection admin status is below the exempt status,
		// or exempt status is less than 0 (no-one is exempt)
		if ((countCLEIPs->GetIP() == iIP) &&
			((countCLEIPs->Admin() < (RuleI(World, ExemptMaxClientsStatus))) ||
			(RuleI(World, ExemptMaxClientsStatus) < 0))) {

			// Increment the occurrences of this IP address
			IPInstances++;

			// If the number of connections exceeds the lower limit divided by number of exemptions allowed.
			// Set exemptions in account/ip_exemption_multiplier, default is 1.
			// 1 means 1 set of MaxClientsPerIP online allowed.
			// example MaxClientsPerIP set to 3 and ip_exemption_multiplier 1, only 3 accounts will be allowed.
			// Whereas MaxClientsPerIP set to 3 and ip_exemption_multiplier 2 = a max of 6 accounts allowed.
			if (IPInstances / exemptcount > (RuleI(World, MaxClientsPerIP))) {

				// If MaxClientsSetByStatus is set to True, override other IP Limit Rules
				if (RuleB(World, MaxClientsSetByStatus)) {

					// The IP Limit is set by the status of the account if status > MaxClientsPerIP
					if (IPInstances > countCLEIPs->Admin()) {

						if(RuleB(World, IPLimitDisconnectAll)) {
							DisconnectByIP(iIP);
							return;
						} else {
							// Remove the connection
							countCLEIPs->SetOnline(CLE_Status_Offline);
							iterator.RemoveCurrent();
							continue;
						}
					}
				}
				// Else if the Admin status of the connection is not eligible for the higher limit,
				// or there is no higher limit (AddMaxClientStatus<0)
				else if ((countCLEIPs->Admin() < (RuleI(World, AddMaxClientsStatus)) ||
						(RuleI(World, AddMaxClientsStatus) < 0))) {

					if(RuleB(World, IPLimitDisconnectAll)) {
						DisconnectByIP(iIP);
						return;
					} else {
						// Remove the connection
						countCLEIPs->SetOnline(CLE_Status_Offline);
						iterator.RemoveCurrent();
						continue;
					}
				}
				// else they are eligible for the higher limit, but if they exceed that
				else if (IPInstances > RuleI(World, AddMaxClientsPerIP)) {

					if(RuleB(World, IPLimitDisconnectAll)) {
						DisconnectByIP(iIP);
						return;
					} else {
						// Remove the connection
						countCLEIPs->SetOnline(CLE_Status_Offline);
						iterator.RemoveCurrent();
						continue;
					}
				}
			}
		}
		iterator.Advance();
	}
}

void ClientList::DisconnectByIP(uint32 iIP) {
	ClientListEntry* countCLEIPs = 0;
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	iterator.Reset();

	while(iterator.MoreElements()) {
		countCLEIPs = iterator.GetData();
		if ((countCLEIPs->GetIP() == iIP)) {
			if(strlen(countCLEIPs->name())) {
				auto pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
				ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*) pack->pBuffer;
				strcpy(skp->adminname, "SessionLimit");
				strcpy(skp->name, countCLEIPs->name());
				skp->adminrank = 255;
				zoneserver_list.SendPacket(pack);
				safe_delete(pack);
			}
			countCLEIPs->SetOnline(CLE_Status_Offline);
			iterator.RemoveCurrent();
			continue;
		}
		iterator.Advance();
	}
}

bool ClientList::CheckIPLimit(uint32 iAccID, uint32 iIP, uint16 admin, ClientListEntry* cle) {

	ClientListEntry* countCLEIPs = 0;
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	int exemptcount = database.CheckExemption(iAccID);
	int exemptadd = 0;
	if (RuleI(World, AddMaxClientsPerIP) > 0)
		exemptadd = RuleI(World, AddMaxClientsPerIP);
	int IPInstances = 1;
	iterator.Reset();

	while(iterator.MoreElements()) {

		countCLEIPs = iterator.GetData();
		
		// If the IP matches, and the connection admin status is below the exempt status,
		// or exempt status is less than 0 (no-one is exempt)
		if ((countCLEIPs != nullptr && countCLEIPs->GetIP() == iIP && !countCLEIPs->mule()) &&
			((admin < (RuleI(World, ExemptMaxClientsStatus))) ||
			(RuleI(World, ExemptMaxClientsStatus) < 0))) {

			// Increment the occurrences of this IP address
			if (countCLEIPs->Online() >= CLE_Status_Zoning && (cle == nullptr || cle != countCLEIPs))
				IPInstances++;
		}
		iterator.Advance();
	}
	// Thie ip_exemption_multiplier modifies the World:MaxClientsPerIP
	// example MaxClientsPerIP set to 3 and ip_exemption_multiplier 1, only 3 accounts will be allowed.
	// Whereas MaxClientsPerIP set to 3 and ip_exemption_multiplier 2 = a max of 6 accounts allowed.
	if (IPInstances > (exemptcount * (RuleI(World, MaxClientsPerIP)))) {

		// If MaxClientsSetByStatus is set to True, override other IP Limit Rules
		if (RuleB(World, MaxClientsSetByStatus)) {

			// The IP Limit is set by the status of the account if status > MaxClientsPerIP
			if (IPInstances > admin) {
				return false;
			}
		}
		// Else if the Admin status of the connection is not eligible for the higher limit,
		// or there is no higher limit (AddMaxClientStatus<0)
		else if ((admin < (RuleI(World, AddMaxClientsPerIP)) ||
				(RuleI(World, AddMaxClientsStatus) < 0) || (RuleI(World, AddMaxClientsPerIP) < 0))) {
			return false;
		}
		// else they are eligible for the higher limit, but if they exceed that
		else if (IPInstances > (exemptcount * (RuleI(World, MaxClientsPerIP) + RuleI(World, AddMaxClientsPerIP)))) {

			return false;
		}
	}
	return true;
}

bool ClientList::CheckMuleLimit(uint32 iAccID, uint32 iIP, uint16 admin, ClientListEntry *cle) {

	ClientListEntry *curCLE = 0;
	LinkedListIterator<ClientListEntry *> iterator(clientlist);
	int MuleInstances = 1;
	iterator.Reset();

	while (iterator.MoreElements()) {

		curCLE = iterator.GetData();

		// If the forum name matches, and the connection admin status is below the exempt status,
		// or exempt status is less than 0 (no-one is exempt)
		if ((curCLE != nullptr && curCLE->GetIP() == iIP && curCLE->mule()) &&
			((admin < (RuleI(World, ExemptMaxClientsStatus))) ||
			(RuleI(World, ExemptMaxClientsStatus) < 0))) {

			// Increment the occurrences of this forum name
			if (curCLE->Online() >= CLE_Status_Zoning && (cle == nullptr || cle != curCLE))
				MuleInstances++;
		}
		iterator.Advance();
	}

	if (MuleInstances > (RuleI(World, MaxMulesPerIP))) {

		// If MaxClientsSetByStatus is set to True, override other IP Limit Rules
		if (RuleB(World, MaxClientsSetByStatus)) {

			// The IP Limit is set by the status of the account if status > MaxClientsPerIP
			if (MuleInstances > admin) {
				return false;
			}
		}
		else
		{
			return false; // limit exceeded
		}
	}
	return true;
}

bool ClientList::CheckAccountActive(uint32 iAccID, ClientListEntry *cle) {

	ClientListEntry* countCLEIPs = 0;
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	iterator.Reset();

	while(iterator.MoreElements()) {
		if (iterator.GetData()->AccountID() == iAccID && iterator.GetData()->Online() >= CLE_Status_Zoning && (cle == nullptr || cle != iterator.GetData())) {
			return true;
		}
		iterator.Advance();
	}
	return false;
}

ClientListEntry* ClientList::FindCharacter(const char* name) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements())
	{
		if (strcasecmp(iterator.GetData()->name(), name) == 0) {
			return iterator.GetData();
		}
		iterator.Advance();
	}
	return 0;
}


ClientListEntry* ClientList::FindCLEByAccountID(uint32 iAccID) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->AccountID() == iAccID) {
			return iterator.GetData();
		}
		iterator.Advance();
	}
	return 0;
}

ClientListEntry* ClientList::FindCLEByCharacterID(uint32 iCharID) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->CharID() == iCharID) {
			return iterator.GetData();
		}
		iterator.Advance();
	}
	return 0;
}

void ClientList::ClearGroup(uint32 group_id) {
	if (group_id == 0)
		return;
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->GroupID() == group_id)
			iterator.GetData()->SetGroupID(0);
		iterator.Advance();
	}
}

void ClientList::SendCLEList(const int16& admin, const char* to, WorldTCPConnection* connection, const char* iName) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	int x = 0, y = 0;
	int namestrlen = iName == 0 ? 0 : strlen(iName);
	bool addnewline = false;
	char newline[3];
	if (connection->IsConsole())
		strcpy(newline, "\r\n");
	else
		strcpy(newline, "^");
	std::vector<char> out;

	iterator.Reset();
	while(iterator.MoreElements()) {
		ClientListEntry* cle = iterator.GetData();
		if (admin >= cle->Admin() && (iName == 0 || namestrlen == 0 || strncasecmp(cle->name(), iName, namestrlen) == 0 || strncasecmp(cle->AccountName(), iName, namestrlen) == 0 || strncasecmp(cle->LSName(), iName, namestrlen) == 0)) {
			struct in_addr in;
			in.s_addr = cle->GetIP();
			if (addnewline) {
				fmt::format_to(std::back_inserter(out), fmt::runtime(newline));
			}
			if (cle->AccountID() && cle->AccountName() && cle->AccountName()[0] != 0 && inet_ntoa(in) && inet_ntoa(in)[0] != 0 && strlen(inet_ntoa(in)) > 0)
			{
				fmt::format_to(std::back_inserter(out), "ID: {}  Acc# {}  AccName: {}  IP: {}", cle->GetID(), cle->AccountID(), cle->AccountName(), inet_ntoa(in));
				fmt::format_to(std::back_inserter(out), "{}  Stale: {}  Online: {}  Admin: {}", newline, cle->GetStaleCounter(), cle->Online(), cle->Admin());
				if (cle->LSID())
					fmt::format_to(std::back_inserter(out), "{}  LSID: {}  LSName: {}  WorldAdmin: {}", newline, cle->LSID(), cle->LSName(), cle->WorldAdmin());
				if (cle->CharID())
					fmt::format_to(std::back_inserter(out), "{}  CharID: {}  CharName: {}  Zone: {} ({})", newline, cle->CharID(), cle->name(), database.GetZoneName(cle->zone()), cle->zone());
				if (out.size() >= 3072) {
					connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
				}
				addnewline = false;
				out.clear();
			}
			else
			{
				addnewline = true;
			}
			y++;
		}
		iterator.Advance();
		x++;
	}
	fmt::format_to(std::back_inserter(out), "{}{} CLEs in memory. {} CLEs listed. numplayers = {}.", newline, x, y, numplayers);
	connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
}

ClientListEntry* ClientList::RemoveCLEByAccountID(uint32 accountID) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while (iterator.MoreElements())
	{
		if (iterator.GetData()->AccountID() == accountID) {
			iterator.RemoveCurrent();
			continue;
		}
		iterator.Advance();
	}
	return 0;
}


void ClientList::CLEAdd(uint32 iLSID, const char* iLoginName, const char* iLoginKey, int16 iWorldAdmin, uint32 ip, uint8 local, uint8 version) {
	auto tmp = new ClientListEntry(GetNextCLEID(), iLSID, iLoginName, iLoginKey, iWorldAdmin, ip, local, version, 0);

	clientlist.Append(tmp);
}

void ClientList::CLCheckStale() {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->CheckStale()) {
			struct in_addr in;
			in.s_addr = iterator.GetData()->GetIP();
			Log(Logs::Detail, Logs::WorldServer,"Removing stale client on account %d from %s", iterator.GetData()->AccountID(), inet_ntoa(in));
			uint32 accountid = iterator.GetData()->AccountID();
			iterator.RemoveCurrent();
			if(!ActiveConnection(accountid))
				database.ClearAccountActive(accountid);
		}
		else
			iterator.Advance();
	}
}

void ClientList::ClientUpdate(ZoneServer* zoneserver, ServerClientList_Struct* scl) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	ClientListEntry* cle;
	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->GetID() == scl->wid) {
			cle = iterator.GetData();
			if (scl->remove == 2){
				cle->LeavingZone(zoneserver, CLE_Status_Offline);
			}
			else if (scl->remove == 1)
				cle->LeavingZone(zoneserver, CLE_Status_Zoning);
			else
				cle->Update(zoneserver, scl);
			return;
		}
		iterator.Advance();
	}
	if (scl->remove == 2)
		cle = new ClientListEntry(GetNextCLEID(), zoneserver, scl, CLE_Status_Online);
	else if (scl->remove == 1)
		cle = new ClientListEntry(GetNextCLEID(), zoneserver, scl, CLE_Status_Zoning);
	else
		cle = new ClientListEntry(GetNextCLEID(), zoneserver, scl, CLE_Status_InZone);
	clientlist.Insert(cle);
	zoneserver->ChangeWID(scl->charid, cle->GetID());
}

void ClientList::CLEKeepAlive(uint32 numupdates, uint32* wid) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	uint32 i;

	iterator.Reset();
	while(iterator.MoreElements()) {
		for (i=0; i<numupdates; i++) {
			if (wid[i] == iterator.GetData()->GetID())
				iterator.GetData()->KeepAlive();
		}
		iterator.Advance();
	}
}


ClientListEntry* ClientList::CheckAuth(uint32 id, const char* iKey, uint32 ip ) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->CheckAuth(id, iKey, ip))
			return iterator.GetData();
		iterator.Advance();
	}
	return 0;
}
ClientListEntry* ClientList::CheckAuth(uint32 iLSID, const char* iKey) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->CheckAuth(iLSID, iKey))
			return iterator.GetData();
		iterator.Advance();
	}
	return 0;
}

ClientListEntry* ClientList::CheckAuth(const char* iName, const char* iPassword) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	MD5 tmpMD5(iPassword);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->CheckAuth(iName, tmpMD5))
			return iterator.GetData();
		iterator.Advance();
	}
	int16 tmpadmin;
	int8 tmprevoked = false;

	LogDebug("Login with {} and {}", iName, iPassword);

	uint32 accid = database.CheckLogin(iName, iPassword, &tmpadmin, &tmprevoked);
	if (accid) {
		tmprevoked = database.CheckRevoked(accid);
		uint32 lsid = 0;
		database.GetAccountIDByName(iName, &tmpadmin, &lsid);
		auto tmp = new ClientListEntry(GetNextCLEID(), lsid, iName, iPassword, tmpadmin, 0, 0, 2, tmprevoked);
		clientlist.Append(tmp);
		return tmp;
	}
	return 0;
}

void ClientList::SendOnlineGuildMembers(uint32 FromID, uint32 GuildID)
{
	int PacketLength = 8;

	uint32 Count = 0;
	ClientListEntry* from = this->FindCLEByCharacterID(FromID);

	if(!from)
	{
		Log(Logs::Detail, Logs::WorldServer,"Invalid client. FromID=%i GuildID=%i", FromID, GuildID);
		return;
	}

	LinkedListIterator<ClientListEntry*> Iterator(clientlist);

	Iterator.Reset();

	while(Iterator.MoreElements())
	{
		ClientListEntry* CLE = Iterator.GetData();

		if(CLE && (CLE->GuildID() == GuildID))
		{
			PacketLength += (strlen(CLE->name()) + 5);
			++Count;
		}

		Iterator.Advance();

	}

	Iterator.Reset();

	auto pack = new ServerPacket(ServerOP_OnlineGuildMembersResponse, PacketLength);

	char *Buffer = (char *)pack->pBuffer;

	VARSTRUCT_ENCODE_TYPE(uint32, Buffer, FromID);
	VARSTRUCT_ENCODE_TYPE(uint32, Buffer, Count);

	while(Iterator.MoreElements())
	{
		ClientListEntry* CLE = Iterator.GetData();

		if(CLE && (CLE->GuildID() == GuildID))
		{
			VARSTRUCT_ENCODE_STRING(Buffer, CLE->name());
			VARSTRUCT_ENCODE_TYPE(uint32, Buffer, CLE->zone());
		}

		Iterator.Advance();
	}
	zoneserver_list.SendPacket(from->zone(), from->GuildID(), pack);
	safe_delete(pack);
}


void ClientList::SendWhoAll(uint32 fromid,const char* to, int16 admin, Who_All_Struct* whom) 
{
	try
	{
		LinkedListIterator<ClientListEntry*> iterator(clientlist, BACKWARD);
		LinkedListIterator<ClientListEntry*> countclients(clientlist, BACKWARD);

		ClientListEntry* cle = 0;
		ClientListEntry* countcle = 0;
		char line[300] = "";
		int whomlen = 0;

		if (whom) 
		{
			whomlen = strlen(whom->whom);
		}

		uint16 totalusers = 0;
		uint16 totallength = 0;
		uint8 gmwholist = RuleI(GM, GMWhoList);
		uint8 wholimit = RuleI(World, WhoListLimit);
		bool noguildlimit = RuleB(AlKabor, NoMaxWhoGuild);

		// This loop grabs the player count.
		countclients.Reset();
		while (countclients.MoreElements()) 
		{
			countcle = countclients.GetData();
			if (WhoAllFilter(countcle, whom, admin, whomlen))
			{
				// Count for GMs.
				if ((countcle->Anon() > 0 && admin >= countcle->Admin() && admin >= AccountStatus::QuestTroupe) || countcle->Anon() == 0)
				{
					++totalusers;
					if (totalusers <= wholimit || admin >= gmwholist || (whom->guildid >= 0 && noguildlimit))
					{
						totallength = totallength + strlen(countcle->name()) + strlen(countcle->AccountName()) + strlen(guild_mgr.GetGuildName(countcle->GuildID())) + 5;
					}
				}
				// Count for Players. We want to exclude anon players from the count.
				else if ((countcle->Anon() > 0 && admin <= countcle->Admin()) || (countcle->Anon() == 0 && !countcle->GetGM()))
				{
					// We want a total count, so we know if we need to send WHOALL_CUT_SHORT
					++totalusers;
					if (totalusers <= wholimit || admin >= gmwholist || (whom->guildid >= 0 && noguildlimit))
					{
						totallength = totallength + strlen(countcle->name()) + strlen(guild_mgr.GetGuildName(countcle->GuildID())) + 5;
					}
				}
			}
			countclients.Advance();
		}

		// This is the packet header data.
		uint32 plid = fromid;
		uint16 playerineqstring = WHOALL_PLAYERS;
		const char line2[] = "---------------------------";
		uint8 unknown35 = 0x0A;
		uint16 unknown36 = 0;
		uint16 playersinzonestring = WHOALL_SINGLE;
		uint16 unknown44[5];

		if (totalusers > wholimit && admin < gmwholist && (!noguildlimit || (whom->guildid < 0)))
		{
			totalusers = wholimit;
			playersinzonestring = WHOALL_CUT_SHORT;
		}
		else if (totalusers > 1)
		{
			playersinzonestring = WHOALL_COUNT;
		}

		unknown44[0] = 0;
		unknown44[1] = 0;
		unknown44[2] = 0;
		unknown44[3] = 0;
		unknown44[4] = 0;
		uint32 unknown52 = totalusers;
		uint32 unknown56 = 1;

		auto pack2 = new ServerPacket(ServerOP_WhoAllReply, 58 + totallength + (30 * totalusers));
		memset(pack2->pBuffer, 0, pack2->size);
		uchar *buffer = pack2->pBuffer;
		uchar *bufptr = buffer;

		memcpy(bufptr, &plid, sizeof(uint32));
		bufptr += sizeof(uint32);
		memcpy(bufptr, &playerineqstring, sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &line2, strlen(line2));
		bufptr += strlen(line2);
		memcpy(bufptr, &unknown35, sizeof(uint8));
		bufptr += sizeof(uint8);
		memcpy(bufptr, &unknown36, sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &playersinzonestring, sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &unknown44[0], sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &unknown44[1], sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &unknown44[2], sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &unknown44[3], sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &unknown44[4], sizeof(uint16));
		bufptr += sizeof(uint16);
		memcpy(bufptr, &unknown52, sizeof(uint32));
		bufptr += sizeof(uint32);
		memcpy(bufptr, &unknown56, sizeof(uint32));
		bufptr += sizeof(uint32);
		memcpy(bufptr, &totalusers, sizeof(uint16));
		bufptr += sizeof(uint16);
		// End packet header

		// This loop fills out the packet with the whoall player data.
		iterator.Reset();
		int idx = 0;
		while (iterator.MoreElements())
		{
			cle = iterator.GetData();
			if (WhoAllFilter(cle, whom, admin, whomlen))
			{
				line[0] = 0;
				uint16 rankstring = 0xFFFF;
				//hide gms that are anon from lesser gms and normal players, cut off at 20
				if ((cle->Anon() == 1 && cle->GetGM() && cle->Admin() > admin) ||
					(idx >= wholimit && admin < gmwholist && (!noguildlimit || (whom->guildid < 0))))
				{
					rankstring = 0;
					iterator.Advance();
					continue;
				}
				else if (cle->GetGM())
				{
					if (cle->Admin() >= AccountStatus::GMImpossible)
						rankstring = WHOALL_IMPOSSIBRU;
					else if (cle->Admin() >= AccountStatus::GMMgmt)
						rankstring = WHOALL_MGMT;
					else if (cle->Admin() >= AccountStatus::GMCoder)
						rankstring = WHOALL_CODER;
					else if (cle->Admin() >= AccountStatus::GMAreas)
						rankstring = WHOALL_AREAS;
					else if (cle->Admin() >= AccountStatus::QuestMaster)
						rankstring = WHOALL_QUESTMASTER;
					else if (cle->Admin() >= AccountStatus::GMLeadAdmin)
						rankstring = WHOALL_LEAD;
					else if (cle->Admin() >= AccountStatus::GMAdmin)
						rankstring = WHOALL_ADMIN;
					else if (cle->Admin() >= AccountStatus::GMStaff)
						rankstring = WHOALL_STAFF;
					else if (cle->Admin() >= AccountStatus::EQSupport)
						rankstring = WHOALL_EQSUPPORT;
					else if (cle->Admin() >= AccountStatus::GMTester)
						rankstring = WHOALL_TESTER;
					else if (cle->Admin() >= AccountStatus::SeniorGuide)
						rankstring = WHOALL_SENIOR;
					else if (cle->Admin() >= AccountStatus::QuestTroupe)
						rankstring = WHOALL_QUESTTROUPE;
					else if (cle->Admin() >= AccountStatus::Guide)
						rankstring = WHOALL_GUIDE;
					else if (cle->Admin() >= AccountStatus::ApprenticeGuide)
						rankstring = WHOALL_APPRENTICE;
					else if (cle->Admin() >= AccountStatus::Steward)
						rankstring = WHOALL_STEWARD;
				}
				++idx;

				char guildbuffer[67] = { 0 };
				if (cle->GuildID() != GUILD_NONE && cle->GuildID() > 0 && (cle->Anon() != 1 || admin >= cle->Admin()))
					sprintf(guildbuffer, "<%s>", guild_mgr.GetGuildName(cle->GuildID()));
				uint16 formatstring = WHOALL_ALL;
				if (cle->Anon() == 1 && (admin < cle->Admin() || admin == AccountStatus::Player))
					formatstring = WHOALL_ANON;
				else if (cle->Anon() == 1 && admin >= cle->Admin() && admin >= AccountStatus::QuestTroupe)
					formatstring = WHOALL_GM;
				else if (cle->Anon() == 2 && (admin < cle->Admin() || admin == AccountStatus::Player))
					formatstring = WHOALL_ROLE;//display guild
				else if (cle->Anon() == 2 && admin >= cle->Admin() && admin >= AccountStatus::QuestTroupe)
					formatstring = WHOALL_GM;//display everything

				uint16 plclass_ = 0;
				uint16 pllevel = 0;
				uint16 pidstring = 0xFFFF;
				uint16 plrace = 0;
				uint16 zonestring = 0xFFFF;
				uint32 plzone = 0;
				uint16 plgm = 0xFFFF;
				uint16 unknown80 = 0xFFFF;
				uint16 plflag = 0xFFFF;

				if (cle->Anon() == 0 || (admin >= cle->Admin() && admin >= AccountStatus::QuestTroupe))
				{
					if (!cle->GetGM() && cle->AFK())
						rankstring = WHOALL_AFK;

					plclass_ = cle->class_();
					pllevel = cle->level();
					if (admin >= gmwholist)
						pidstring = WHOALL_USERPID;
					plrace = cle->baserace();
					zonestring = WHOALL_ZONE;
					plzone = database.GetClientZoneID(cle->zone());
					
					if (cle->LFG())
						plflag = WHOALL_LFG;
				}

				if (admin >= cle->Admin() && admin >= AccountStatus::QuestTroupe)
					plgm = cle->Admin();

				char plname[64] = { 0 };
				strcpy(plname, cle->name());

				char placcount[30] = { 0 };
				if (admin >= cle->Admin() && admin >= AccountStatus::QuestTroupe)
					strcpy(placcount, cle->AccountName());
				else if (admin >= AccountStatus::QuestTroupe)
					strcpy(placcount, "NA");

				memcpy(bufptr, &formatstring, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &pidstring, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &plname, strlen(plname) + 1);
				bufptr += strlen(plname) + 1;
				memcpy(bufptr, &rankstring, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &guildbuffer, strlen(guildbuffer) + 1);
				bufptr += strlen(guildbuffer) + 1;
				memcpy(bufptr, &plgm, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &unknown80, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &plflag, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &zonestring, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &plzone, sizeof(uint32));
				bufptr += sizeof(uint32);
				memcpy(bufptr, &plclass_, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &pllevel, sizeof(uint16));
				bufptr += sizeof(uint16);
				memcpy(bufptr, &plrace, sizeof(uint16));
				bufptr += sizeof(uint16);
				uint16 ending = 0;
				memcpy(bufptr, &placcount, strlen(placcount) + 1);
				bufptr += strlen(placcount) + 1;
				ending = 211;
				memcpy(bufptr, &ending, sizeof(uint16));
				bufptr += sizeof(uint16);
			}
			iterator.Advance();
		}

		pack2->Deflate();
		SendPacket(to,pack2);
		safe_delete(pack2);
	}
	catch(...)
	{
		Log(Logs::Detail, Logs::WorldServer, "Unknown error in world's SendWhoAll (probably mem error), ignoring... Player id is: %i, Name is: %s", fromid, to);
		return;
	}
}

void ClientList::SendFriendsWho(ServerFriendsWho_Struct *FriendsWho, WorldTCPConnection* connection) {

	std::vector<ClientListEntry*> FriendsCLEs;
	FriendsCLEs.reserve(100);

	char Friend_[65];

	char *FriendsPointer = FriendsWho->FriendsString;

	// FriendsString is a comma delimited list of names.

	char *Seperator = nullptr;

	Seperator = strchr(FriendsPointer, ',');
	if(!Seperator) Seperator = strchr(FriendsPointer, '\0');

	uint32 TotalLength=0;

	while(Seperator != nullptr) {

		if((Seperator - FriendsPointer) > 64) return;

		strncpy(Friend_, FriendsPointer, Seperator - FriendsPointer);
		Friend_[Seperator - FriendsPointer] = 0;

		ClientListEntry* CLE = FindCharacter(Friend_);
		if(CLE && CLE->name() && (CLE->Online() >= CLE_Status_Zoning) && (CLE->level() > 0) && !(CLE->GetGM() && CLE->Anon())) {
			FriendsCLEs.push_back(CLE);
			TotalLength += strlen(CLE->name());
			int GuildNameLength = strlen(guild_mgr.GetGuildName(CLE->GuildID()));
			if(GuildNameLength>0)
				TotalLength += (GuildNameLength + 2);
		}

		if(Seperator[0] == '\0') break;

		FriendsPointer = Seperator + 1;
		Seperator = strchr(FriendsPointer, ',');
		if(!Seperator) Seperator = strchr(FriendsPointer, '\0');
	}


	try{
		ClientListEntry* cle;
		int FriendsOnline = FriendsCLEs.size();
		int PacketLength = sizeof(WhoAllReturnStruct) + (47 * FriendsOnline) + TotalLength;
		auto pack2 = new ServerPacket(ServerOP_WhoAllReply, PacketLength);
		memset(pack2->pBuffer,0,pack2->size);
		uchar *buffer=pack2->pBuffer;
		uchar *bufptr=buffer;

		WhoAllReturnStruct *WARS = (WhoAllReturnStruct *)bufptr;

		WARS->id = FriendsWho->FromID;
		WARS->playerineqstring = 0xffff;
		strcpy(WARS->line, "");
		WARS->unknown35 = 0x0a;
		WARS->unknown36 = 0x00;

		if(FriendsCLEs.size() == 1)
			WARS->playersinzonestring = 5028; // 5028 There is %1 player in EverQuest.
		else
			WARS->playersinzonestring = 5036; // 5036 There are %1 players in EverQuest.

		WARS->unknown44[0] = 0;
		WARS->unknown44[1] = 0;
		WARS->unknown44[2] = 0;
		WARS->unknown44[3] = 0;
		WARS->unknown44[4] = 0;
		WARS->unknown52 = FriendsOnline;
		WARS->unknown56 = 1;
		WARS->playercount = FriendsOnline;

		bufptr+=sizeof(WhoAllReturnStruct);

		for(int CLEEntry = 0; CLEEntry < FriendsOnline; CLEEntry++) {

			cle = FriendsCLEs[CLEEntry];

			char GuildName[67]={0};
			if (cle->GuildID() != GUILD_NONE && cle->GuildID()>0)
				sprintf(GuildName,"<%s>", guild_mgr.GetGuildName(cle->GuildID()));
			uint16 FormatMSGID=5025; // 5025 %T1[%2 %3] %4 (%5) %6 %7 %8 %9
			if(cle->Anon()==1)
				FormatMSGID=5024; // 5024 %T1[ANONYMOUS] %2 %3
			else if(cle->Anon()==2)
				FormatMSGID=5023; // 5023 %T1[ANONYMOUS] %2 %3 %4

			uint16 PlayerClass=0;
			uint16 PlayerLevel=0;
			uint16 PlayerRace=0;
			uint16 ZoneMSGID=0xffff;
			uint16 PlayerZone=0;

			if(cle->Anon()==0) {
				PlayerClass=cle->class_();
				PlayerLevel=cle->level();
				PlayerRace=cle->race();
				ZoneMSGID=5006; // 5006 ZONE: %1
				PlayerZone=cle->zone();
			}

			char PlayerName[64]={0};
			strcpy(PlayerName,cle->name());

			WhoAllPlayerPart1* WAPP1 = (WhoAllPlayerPart1*)bufptr;

			WAPP1->FormatMSGID = FormatMSGID;
			WAPP1->PIDMSGID = 0xffff;
			strcpy(WAPP1->Name, PlayerName);

			bufptr += sizeof(WhoAllPlayerPart1) + strlen(PlayerName);
			WhoAllPlayerPart2* WAPP2 = (WhoAllPlayerPart2*)bufptr;

			WAPP2->RankMSGID = 0xffff;
			strcpy(WAPP2->Guild, GuildName);

			bufptr += sizeof(WhoAllPlayerPart2) + strlen(GuildName);
			WhoAllPlayerPart3* WAPP3 = (WhoAllPlayerPart3*)bufptr;

			WAPP3->Unknown80[0] = 0xffff;
			WAPP3->Unknown80[1] = 0xffff;
			WAPP3->Unknown80[2] = 0xffff;
			WAPP3->ZoneMSGID = ZoneMSGID;
			WAPP3->Zone = PlayerZone;
			WAPP3->Class_ = PlayerClass;
			WAPP3->Level = PlayerLevel;
			WAPP3->Race = PlayerRace;
			WAPP3->Account[0] = 0;

			bufptr += sizeof(WhoAllPlayerPart3);

			WhoAllPlayerPart4* WAPP4 = (WhoAllPlayerPart4*)bufptr;
			WAPP4->Unknown100 = 211;

			bufptr += sizeof(WhoAllPlayerPart4);

		}
		pack2->Deflate();
		SendPacket(FriendsWho->FromName,pack2);
		safe_delete(pack2);
	}
	catch(...){
		Log(Logs::Detail, Logs::WorldServer,"Unknown error in world's SendFriendsWho (probably mem error), ignoring...");
		return;
	}
}

void ClientList::ConsoleSendWhoAll(const char* to, int16 admin, Who_All_Struct* whom, WorldTCPConnection* connection) 
{
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	ClientListEntry* cle = 0;
	char tmpgm[25] = "";
	char accinfo[150] = "";
	char line[450] = "";
	char tmpguild[50] = "";
	char LFG[10] = "";
	char Trader[10] = "";
	uint32 x = 0;
	int whomlen = 0;
	if (whom)
		whomlen = strlen(whom->whom);

	std::vector<char> out;
	fmt::format_to(std::back_inserter(out), "Players on server:\r\n");
	iterator.Reset();
	while(iterator.MoreElements()) 
	{
		cle = iterator.GetData();
		const char* tmpZone = database.GetZoneName(cle->zone());
		if (
			(cle->Online() >= CLE_Status_Zoning)
				&& (whom == 0 || (
				((cle->Admin() >= 80 && cle->GetGM()) || whom->gmlookup == -1) &&
				(whom->lvllow == -1 || (cle->level() >= whom->lvllow && cle->level() <= whom->lvlhigh)) &&
				(whom->wclass == -1 || cle->class_() == whom->wclass) &&
				(whom->wrace == -1 || cle->baserace() == whom->wrace) &&
				(whom->guildid == -1 || cle->GuildID() == whom->guildid) &&
				(whomlen == 0 || (
				(tmpZone != 0 && strncasecmp(tmpZone, whom->whom, whomlen) == 0) ||
				strncasecmp(cle->name(),whom->whom, whomlen) == 0 ||
				(strncasecmp(guild_mgr.GetGuildName(cle->GuildID()), whom->whom, whomlen) == 0) ||
				(admin >= 100 && strncasecmp(cle->AccountName(), whom->whom, whomlen) == 0)))))
			) 
		{
			line[0] = 0;
			if (cle->Admin() >= AccountStatus::GMImpossible)
				strcpy(tmpgm, "* GM-Impossible * ");
			else if (cle->Admin() >= AccountStatus::GMMgmt)
				strcpy(tmpgm, "* GM-Mgmt * ");
			else if (cle->Admin() >= AccountStatus::GMCoder)
				strcpy(tmpgm, "* GM-Coder * ");
			else if (cle->Admin() >= AccountStatus::GMAreas)
				strcpy(tmpgm, "* GM-Areas * ");
			else if (cle->Admin() >= AccountStatus::QuestMaster)
				strcpy(tmpgm, "* QuestMaster * ");
			else if (cle->Admin() >= AccountStatus::GMLeadAdmin)
				strcpy(tmpgm, "* GM-Lead Admin * ");
			else if (cle->Admin() >= AccountStatus::GMAdmin)
				strcpy(tmpgm, "* GM-Admin * ");
			else if (cle->Admin() >= AccountStatus::GMStaff)
				strcpy(tmpgm, "* GM-Staff * ");
			else if (cle->Admin() >= AccountStatus::EQSupport)
				strcpy(tmpgm, "* EQ Support * ");
			else if (cle->Admin() >= AccountStatus::GMTester)
				strcpy(tmpgm, "* GM-Tester * ");
			else if (cle->Admin() >= AccountStatus::SeniorGuide)
				strcpy(tmpgm, "* Senior Guide * ");
			else if (cle->Admin() >= AccountStatus::QuestTroupe)
				strcpy(tmpgm, "* Guide * ");
			else if (cle->Admin() >= AccountStatus::Guide)
				strcpy(tmpgm, "* Novice Guide * ");
			else if (cle->Admin() >= AccountStatus::ApprenticeGuide)
				strcpy(tmpgm, "* Apprentice Guide * ");
			else if (cle->Admin() >= AccountStatus::Steward)
				strcpy(tmpgm, "* Steward * ");
			else if(cle->AFK())
				strcpy(tmpgm, "AFK ");
			else
				tmpgm[0] = 0;

			if (guild_mgr.GuildExists(cle->GuildID())) {
				snprintf(tmpguild, 36, " <%s>", guild_mgr.GetGuildName(cle->GuildID()));
			} else
				tmpguild[0] = 0;

			if (cle->LFG())
				strcpy(LFG, " LFG");
			else
				LFG[0] = 0;

			if (cle->Trader())
				strcpy(Trader, " TRADER");
			else
				Trader[0] = 0;

			if (admin >= 150 && admin >= cle->Admin()) {
				sprintf(accinfo, " AccID: %i AccName: %s LSID: %i Status: %i", cle->AccountID(), cle->AccountName(), cle->LSAccountID(), cle->Admin());
			}
			else
				accinfo[0] = 0;

			if (cle->Anon() == 2) { // Roleplay
				if (admin >= 100 && admin >= cle->Admin())
					sprintf(line, "  %s[RolePlay %i %s] %s (%s)%s zone: %s%s%s%s", tmpgm, cle->level(), GetClassIDName(cle->class_(),cle->level()), cle->name(), GetRaceIDName(cle->race()), tmpguild, tmpZone, LFG, Trader, accinfo);
				else if (cle->Admin() >= AccountStatus::QuestTroupe && admin < AccountStatus::QuestTroupe && cle->GetGM()) {
					iterator.Advance();
					continue;
				}
				else
					sprintf(line, "  %s[ANONYMOUS] %s%s%s%s%s", tmpgm, cle->name(), tmpguild, LFG, Trader, accinfo);
			}
			else if (cle->Anon() == 1) { // Anon
				if (admin >= 100 && admin >= cle->Admin())
					sprintf(line, "  %s[ANON %i %s] %s (%s)%s zone: %s%s%s%s", tmpgm, cle->level(), GetClassIDName(cle->class_(),cle->level()), cle->name(), GetRaceIDName(cle->race()), tmpguild, tmpZone, LFG, Trader, accinfo);
				else if (cle->Admin() >= AccountStatus::QuestTroupe && cle->GetGM()) {
					iterator.Advance();
					continue;
				}
				else
					sprintf(line, "  %s[ANONYMOUS] %s%s%s%s", tmpgm, cle->name(), LFG, Trader, accinfo);
			}
			else
				sprintf(line, "  %s[%i %s] %s (%s)%s zone: %s%s%s%s", tmpgm, cle->level(), GetClassIDName(cle->class_(),cle->level()), cle->name(), GetRaceIDName(cle->race()), tmpguild, tmpZone, LFG, Trader, accinfo);

			fmt::format_to(std::back_inserter(out), fmt::runtime(line));
			if (out.size() >= 3584) {
				connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
				out.clear();
			}
			else {
				if (connection->IsConsole())
					fmt::format_to(std::back_inserter(out), "\r\n");
				else
					fmt::format_to(std::back_inserter(out), "\n");
			}
			x++;
			if (x >= 20 && admin < AccountStatus::QuestTroupe)
				break;
		}
		iterator.Advance();
	}

	if (x >= 20 && admin < AccountStatus::QuestTroupe)
		fmt::format_to(std::back_inserter(out), "too many results...20 players shown");
	else
		fmt::format_to(std::back_inserter(out), "{} players online\r\n", x);
	if (admin >= 150 && (whom == 0 || whom->gmlookup != -1)) 
	{
		if (connection->IsConsole())
			
			fmt::format_to(std::back_inserter(out), "\r\n");
		else
			fmt::format_to(std::back_inserter(out), "\n");
		
		//console_list.SendConsoleWho(connection, to, admin);
	}
	connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
}

void ClientList::ConsoleTraderCount(const char* to, WorldTCPConnection* connection)
{
	LinkedListIterator<ClientListEntry*> iterator(clientlist);
	ClientListEntry* cle = 0;

	uint32 tradercount = 0;

	std::vector<char> out;

	iterator.Reset();
	while (iterator.MoreElements())
	{
		cle = iterator.GetData();

		if (cle->Trader())
		{
			++tradercount;
		}

		iterator.Advance();
	}

	fmt::format_to(std::back_inserter(out), " {} traders online\r\n", tradercount);
	connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
}

void ClientList::Add(Client* client) {
	list.Insert(client);
}

Client* ClientList::FindByAccountID(uint32 account_id) {
	LinkedListIterator<Client*> iterator(list);

	iterator.Reset();
	while(iterator.MoreElements()) {
		Log(Logs::Detail, Logs::WorldServer, "ClientList[0x%08x]::FindByAccountID(%p) iterator.GetData()[%p]", this, account_id, iterator.GetData());
		if (iterator.GetData()->GetAccountID() == account_id) {
			Client* tmp = iterator.GetData();
			return tmp;
		}
		iterator.Advance();
	}
	return 0;
}

Client* ClientList::FindByName(char* charname) {
	LinkedListIterator<Client*> iterator(list);

	iterator.Reset();
	while(iterator.MoreElements()) {
		Log(Logs::Detail, Logs::WorldServer, "ClientList[0x%08x]::FindByName(\"%s\") iterator.GetData()[%p]", this, charname, iterator.GetData());
		if (iterator.GetData()->GetCharName() == charname) {
			Client* tmp = iterator.GetData();
			return tmp;
		}
		iterator.Advance();
	}
	return 0;
}

Client* ClientList::Get(uint32 ip, uint16 port) {
	LinkedListIterator<Client*> iterator(list);

	iterator.Reset();
	while(iterator.MoreElements())
	{
		if (iterator.GetData()->GetIP() == ip && iterator.GetData()->GetPort() == port)
		{
			Client* tmp = iterator.GetData();
			return tmp;
		}
		iterator.Advance();
	}
	return 0;
}

void ClientList::ZoneBootup(ZoneServer* zs) {
	LinkedListIterator<Client*> iterator(list);

	iterator.Reset();
	while(iterator.MoreElements())
	{
		if (iterator.GetData()->WaitingForBootup()) {
			if (iterator.GetData()->GetZoneID() == zs->GetZoneID() && iterator.GetData()->GetZoneGuildID()) {
				iterator.GetData()->EnterWorld(false);
			}
			else if (iterator.GetData()->WaitingForBootup() == zs->GetID()) {
				iterator.GetData()->ZoneUnavail();
			}
		}
		iterator.Advance();
	}
}

void ClientList::RemoveCLEReferances(ClientListEntry* cle) {
	LinkedListIterator<Client*> iterator(list);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->GetCLE() == cle) {
			iterator.GetData()->SetCLE(0);
		}
		iterator.Advance();
	}
}


bool ClientList::SendPacket(const char* to, ServerPacket* pack) {
	if (to == 0 || to[0] == 0) {
		zoneserver_list.SendPacket(pack);
		return true;
	}
	else if (to[0] == '*') {
		// Cant send a packet to a console....
		return false;
	}
	else {
		ClientListEntry* cle = FindCharacter(to);
		if (cle != nullptr) {
			if (cle->Server() != nullptr) {
				cle->Server()->SendPacket(pack);
				return true;
			}
			return false;
		} else {
			ZoneServer* zs = zoneserver_list.FindByName(to);
			if (zs != nullptr) {
				zs->SendPacket(pack);
				return true;
			}
			return false;
		}
	}
	return false;
}

void ClientList::SendGuildPacket(uint32 guild_id, ServerPacket* pack) {
	std::set<std::pair<uint32, uint32>> zone_ids;

	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		if (iterator.GetData()->GuildID() == guild_id) {
			zone_ids.emplace(iterator.GetData()->zone(), iterator.GetData()->GetZoneGuildID());
		}
		iterator.Advance();
	}

	//now we know all the zones, send it to each one... this is kinda a shitty way to do this
	//since its basically O(n^2)
	std::set<std::pair<uint32, uint32>>::iterator cur, end;
	cur = zone_ids.begin();
	end = zone_ids.end();
	for(; cur != end; cur++) {
		zoneserver_list.SendPacket(cur->first, cur->second, pack);
	}
}

void ClientList::UpdateClientGuild(uint32 char_id, uint32 guild_id) {
	LinkedListIterator<ClientListEntry*> iterator(clientlist);

	iterator.Reset();
	while(iterator.MoreElements()) {
		ClientListEntry *cle = iterator.GetData();
		if (cle->CharID() == char_id) {
			cle->SetGuild(guild_id);
		}
		iterator.Advance();
	}
}



int ClientList::GetClientCount() {
	return(numplayers);
}

void ClientList::GetClients(const char *zone_name, std::vector<ClientListEntry *> &res) {
	LinkedListIterator<ClientListEntry *> iterator(clientlist);
	iterator.Reset();

	if(zone_name[0] == '\0') {
		while(iterator.MoreElements()) {
			ClientListEntry* tmp = iterator.GetData();
			res.push_back(tmp);
			iterator.Advance();
		}
	} else {
		uint32 zoneid = database.GetZoneID(zone_name);
		while(iterator.MoreElements()) {
			ClientListEntry* tmp = iterator.GetData();
			if(tmp->zone() == zoneid)
				res.push_back(tmp);
			iterator.Advance();
		}
	}
}

void ClientList::SendClientVersionSummary(const char *Name)
{
	std::vector<uint32> unique_ips;
	std::map<EQ::versions::ClientVersionBit, int> client_count = {
		{ EQ::versions::ClientVersionBit::bit_MacPC, 0 },
		{ EQ::versions::ClientVersionBit::bit_MacIntel, 0 },
		{ EQ::versions::ClientVersionBit::bit_MacPPC, 0 }
	};

	LinkedListIterator<ClientListEntry*> Iterator(clientlist);
	Iterator.Reset();

	while(Iterator.MoreElements())
	{
		auto CLE = Iterator.GetData();

		if(CLE && CLE->zone()) 
		{
			auto  client_version = CLE->GetMacClientVersion();
			if (client_version >= EQ::versions::ClientVersionBit::bit_MacPC && client_version <= EQ::versions::ClientVersionBit::bit_MacPPC)
			{
				client_count[(EQ::versions::ClientVersionBit)client_version]++;
			}

			if (std::find(unique_ips.begin(), unique_ips.end(), CLE->GetIP()) == unique_ips.end()) {
				unique_ips.push_back(CLE->GetIP());
			}
		}
		Iterator.Advance();
	}

	uint32 total_clients = (
		client_count[EQ::versions::ClientVersionBit::bit_MacPC] +
		client_count[EQ::versions::ClientVersionBit::bit_MacIntel] +
		client_count[EQ::versions::ClientVersionBit::bit_MacPPC]
		);

	if (client_count[EQ::versions::ClientVersionBit::bit_MacPC]) {
		zoneserver_list.SendEmoteMessage(
			Name,
			0,
			AccountStatus::Player,
			Chat::NPCQuestSay,
			fmt::format(
				"Client Counts | PC: {} ",
				client_count[EQ::versions::ClientVersionBit::bit_MacPC]
			).c_str()
		);
	}

	if (client_count[EQ::versions::ClientVersionBit::bit_MacIntel]) {
		zoneserver_list.SendEmoteMessage(
			Name,
			0,
			AccountStatus::Player,
			Chat::NPCQuestSay,
			fmt::format(
				"Client Counts | Intel: {} ",
				client_count[EQ::versions::ClientVersionBit::bit_MacIntel]
			).c_str()
		);
	}

	if (client_count[EQ::versions::ClientVersionBit::bit_MacPPC]) {
		zoneserver_list.SendEmoteMessage(
			Name,
			0,
			AccountStatus::Player,
			Chat::NPCQuestSay,
			fmt::format(
				"Client Counts | PPC: {} ",
				client_count[EQ::versions::ClientVersionBit::bit_MacPPC]
			).c_str()
		);
	}

	zoneserver_list.SendEmoteMessage(Name, 0, AccountStatus::Player, Chat::NPCQuestSay, fmt::format(
		"Client Counts | Total: {} , Unique IPs: {} ",
		total_clients,
		unique_ips.size()
		).c_str()
	);
}

void ClientList::ConsoleClientVersionSummary(const char* to, WorldTCPConnection* connection)
{
	uint32 ClientPCCount = 0;
	uint32 ClientIntelCount = 0;

	std::vector<char> out;

	LinkedListIterator<ClientListEntry*> Iterator(clientlist);

	Iterator.Reset();

	while (Iterator.MoreElements())
	{
		ClientListEntry* CLE = Iterator.GetData();

		if (CLE && CLE->zone())
		{
			switch (CLE->GetClientVersion())
			{
			case 5:
			{
				switch (CLE->GetMacClientVersion())
				{
				case 2:
				{
					++ClientPCCount;
					break;
				}
				case 4:
				{
					++ClientIntelCount;
					break;
				}
				}
				break;
			}
			default:
				break;
			}
		}

		Iterator.Advance();

	}

	fmt::format_to(std::back_inserter(out), " {} PC {} Intel clients online.\r\n", ClientPCCount, ClientIntelCount);
	connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
}

bool ClientList::WhoAllFilter(ClientListEntry* client, Who_All_Struct* whom, int16 admin, int whomlen)
{
	uint8 gmwholist = RuleI(GM, GMWhoList);
	const char* tmpZone = database.GetZoneName(client->zone());
	bool not_anon = client->Anon() == 0 || (admin >= client->Admin() && admin >= gmwholist);
	bool guild_not_anon = client->Anon() != 1 || (admin >= client->Admin() && admin >= gmwholist);
	if (
		(client->Online() >= CLE_Status_Zoning) && // Client is zoning or in a zone
		(client->level() > 0) && // initial zoning in level is not updated yet
		(!client->GetGM() || client->Anon() != 1 || (admin >= client->Admin() && admin >= gmwholist)) && // Client is not a GM, OR does not have hideme on, 
																										// OR is higher than GM list rule and is equal or higher status to the GM on who
		(whom == 0 || 
		((whom->gmlookup == -1 || client->Admin() >= gmwholist) && // gm
		(whom->lvllow == -1 || (client->level() >= whom->lvllow && client->level() <= whom->lvlhigh && not_anon)) && // level
		(whom->wclass == -1 || (client->class_() == whom->wclass && not_anon)) && // class
		(whom->wrace == -1 || (client->baserace() == whom->wrace && not_anon)) && // race
		(whom->guildid == -1 || 
		(whom->guildid >= 0 && client->GuildID() == whom->guildid && guild_not_anon) || // guild#
		(whom->guildid == -3 && client->LFG() && not_anon)))) && // lfg
		(whomlen == 0 || 
		((tmpZone != 0 && strncasecmp(tmpZone, whom->whom, whomlen) == 0 && not_anon) || //zone (GM only)
		strncasecmp(client->name(),whom->whom, whomlen) == 0 || // name
		(strncasecmp(guild_mgr.GetGuildName(client->GuildID()), whom->whom, whomlen) == 0 && guild_not_anon)|| // This is used by who all guild
		(admin >= gmwholist && strncasecmp(client->AccountName(), whom->whom, whomlen) == 0)))) // account (GM only)
	{
		return true;
	}
		
	else
	{
		return false;
	}

}

/**
 * @param response
 */
void ClientList::GetClientList(Json::Value &response)
{
	LinkedListIterator<ClientListEntry *> Iterator(clientlist);

	Iterator.Reset();

	while (Iterator.MoreElements()) {
		ClientListEntry *cle = Iterator.GetData();

		Json::Value row;

		row["online"] = cle->Online();
		row["id"] = cle->GetID();
		row["ip"] = cle->GetIP();
		row["loginserver_id"] = cle->LSID();
		row["loginserver_account_id"] = cle->LSAccountID();
		row["loginserver_name"] = cle->LSName();
		row["world_admin"] = cle->WorldAdmin();
		row["account_id"] = cle->AccountID();
		row["account_name"] = cle->AccountName();
		row["admin"] = cle->Admin();

		auto server = cle->Server();
		if (server) {
			row["server"]["client_address"] = server->GetCAddress();
			row["server"]["client_local_address"] = server->GetCLocalAddress();
			row["server"]["compile_time"] = server->GetCompileTime();
			row["server"]["client_port"] = server->GetCPort();
			row["server"]["id"] = server->GetID();
			row["server"]["ip"] = server->GetIP();
			row["server"]["launched_name"] = server->GetLaunchedName();
			row["server"]["launch_name"] = server->GetLaunchName();
			row["server"]["port"] = server->GetPort();
			row["server"]["previous_zone_id"] = server->GetPrevZoneID();
			row["server"]["zone_id"] = server->GetZoneID();
			row["server"]["zone_long_name"] = server->GetZoneLongName();
			row["server"]["zone_name"] = server->GetZoneName();
			row["server"]["zone_os_pid"] = server->GetZoneOSProcessID();
			row["server"]["number_players"] = server->NumPlayers();
			row["server"]["is_booting"] = server->IsBootingUp();
			row["server"]["static_zone"] = server->IsStaticZone();
		}
		else {
			row["server"] = Json::Value();
		}

		row["character_id"] = cle->CharID();
		row["name"] = cle->name();
		row["zone"] = cle->zone();
		row["level"] = cle->level();
		row["class"] = cle->class_();
		row["race"] = cle->race();
		row["anon"] = cle->Anon();

		row["tells_off"] = cle->TellsOff();
		row["guild_id"] = cle->GuildID();
		row["lfg"] = cle->LFG();
		row["gm"] = cle->GetGM();
		row["is_local_client"] = cle->IsLocalClient();
		row["lfg_from_level"] = cle->GetLFGFromLevel();
		row["lfg_to_level"] = cle->GetLFGToLevel();
		row["lfg_match_filter"] = cle->GetLFGMatchFilter();
		row["lfg_comments"] = cle->GetLFGComments();
		row["client_version"] = cle->GetClientVersion();

		response.append(row);

		Iterator.Advance();
	}
}