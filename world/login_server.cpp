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
#include <stdlib.h>
#include <unordered_set>

#include "../common/version.h"
#include "../common/servertalk.h"
#include "../common/misc_functions.h"
#include "../common/eq_packet_structs.h"
#include "../common/packet_dump.h"
#include "../common/strings.h"
#include "../common/eqemu_logsys.h"
#include "login_server.h"
#include "login_server_list.h"
#include "zoneserver.h"
#include "worlddb.h"
#include "zonelist.h"
#include "clientlist.h"
#include "cliententry.h"
#include "world_config.h"
#include <mutex>
extern ZSList        zoneserver_list;
extern ClientList    client_list;
extern uint32        numzones;
extern uint32        numplayers;
extern volatile bool RunLoops;
extern std::mutex ipMutex;
extern std::unordered_set<uint32> ipWhitelist;

LoginServer::LoginServer(const char* iAddress, uint16 iPort, const char* Account, const char* Password, uint8 Type)
{
	strn0cpy(m_loginserver_address, iAddress, 256);
	m_loginserver_port = iPort;
	m_login_account = Account;
	m_login_password = Password;
	m_can_account_update = false;
	m_is_legacy = Type == 1;
	Connect();
}

LoginServer::~LoginServer() {

}

void LoginServer::ProcessUsertoWorldReq(uint16_t opcode, EQ::Net::Packet& p)
{
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	UsertoWorldRequest* utwr = (UsertoWorldRequest*)p.Data();
	uint32                     id = database.GetAccountIDFromLSID(utwr->lsaccountid);
	int16                      status = database.CheckStatus(id);
	
	bool mule = false;
	uint16 expansion = 0;
	uint32 force_guild_id = 0;
	bool does_account_exist = true;
	char forum_name[31] = { 0 };
	database.GetAccountRestriction(id, forum_name, expansion, mule, force_guild_id);

	if (id == 0) {
		LogInfo("No world account found for LS account [{}] - will be created during authentication", utwr->lsaccountid);

		id = utwr->lsaccountid;  // Temporary fallback for new accounts
		status = 0; // Default status for new accounts
		does_account_exist = false;
	}

	if (does_account_exist && utwr->forum_name[0] != '\0')
	{
		if(forum_name[0] == '\0' || forum_name[0] != '\0' && strcmp(forum_name, utwr->forum_name) != 0)
			database.SetForumName(id, utwr->forum_name);
	}

	auto outpack = new ServerPacket;
	outpack->opcode = ServerOP_UsertoWorldResp;
	outpack->size = sizeof(UsertoWorldResponse);
	outpack->pBuffer = new uchar[outpack->size];
	memset(outpack->pBuffer, 0, outpack->size);
	UsertoWorldResponse* utwrs = (UsertoWorldResponse*)outpack->pBuffer;
	utwrs->lsaccountid = utwr->lsaccountid;
	utwrs->ToID = utwr->FromID;

	if (Config->Locked == true)
	{
		if (status < 80 && status > -1)
			utwrs->response = 0;
		if (status >= 80)
			utwrs->response = 1;
	}
	else {
		utwrs->response = 1;
	}

	int32 x = Config->MaxClients;
	if ((int32)client_list.GetClientCount() >= x && x != -1 && x != 255 && status < 80)
		utwrs->response = -3;

	if (status == -1)
		utwrs->response = -1;
	if (status == -2)
		utwrs->response = -2;

	if (utwrs->response == 1)
	{
		// active account checks
		if (RuleI(World, AccountSessionLimit) >= 0 && status < (RuleI(World, ExemptAccountLimitStatus)) && (RuleI(World, ExemptAccountLimitStatus) != -1) && does_account_exist && client_list.ActiveConnectionKickStale(id))
			utwrs->response = -4;
	}
	if (utwrs->response == 1)
	{
		// ip limit checks
		if (!mule && RuleI(World, MaxClientsPerIP) >= 0 && !client_list.CheckIPLimit(id, utwr->ip, status))
		{
			utwrs->response = -5;
		}
	}

	if (client_list.GetClientCount() /* + client_queue.Count()*/ >= RuleI(Quarm, PlayerPopulationCap) && status == 0)
	{
		utwrs->response = -3; // Queue player, don't allow entry
		//We should really tell the WorldServer how much players are remaining in queue to determine this, but we can make that a world <-> login communication
		//TODO: Implement queue logic
	}

	ipMutex.lock();
	ipWhitelist.insert(utwr->ip);
	ipMutex.unlock();
				

	utwrs->worldid = utwr->worldid;
	SendPacket(outpack);
	delete outpack;
}

void LoginServer::ProcessLSClientAuth(uint16_t opcode, EQ::Net::Packet& p) {
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	try {
		auto slsca = p.GetSerialize<ClientAuth>(0);

		slsca.forum_name[30] = '\0'; // Ensure null-termination
		client_list.CLEAdd(slsca.loginserver_account_id, slsca.account_name, slsca.forum_name, slsca.key, slsca.is_world_admin, slsca.ip_address, slsca.is_client_from_local_network, slsca.version);
	}
	catch (std::exception& ex) {
		LogError("Error parsing LSClientAuth packet from world.\n{0}", ex.what());
	}
}

void LoginServer::ProcessLSFatalError(uint16_t opcode, EQ::Net::Packet& p) {
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	if (p.Length() > 1) {
		LogInfo("     {}", (const char*)p.Data());
	}
	database.LSDisconnect();
}

void LoginServer::ProcessSystemwideMessage(uint16_t opcode, EQ::Net::Packet& p) {
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	ServerSystemwideMessage* swm = (ServerSystemwideMessage*)p.Data();
	zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, swm->type, swm->message);
}

void LoginServer::ProcessLSRemoteAddr(uint16_t opcode, EQ::Net::Packet& p) {
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	if (!Config->WorldAddress.length()) {
		WorldConfig::SetWorldAddress((char*)p.Data());
		LogInfo("Loginserver provided {} as world address", (const char*)p.Data());
	}
}

void LoginServer::ProcessLSAccountUpdate(uint16_t opcode, EQ::Net::Packet& p) {
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	LogInfo("Received ServerOP_LSAccountUpdate packet from loginserver");
	m_can_account_update = true;
}

bool LoginServer::Connect() {

	char errbuf[1024];
	if ((m_loginserver_ip = ResolveIP(m_loginserver_address, errbuf)) == 0) {
		LogInfo("Unable to resolve [{}] to an IP", m_loginserver_address);
		database.LSDisconnect();
		return false;
	}

	if (m_loginserver_ip == 0 || m_loginserver_port == 0) {
		LogInfo("Connect info incomplete, cannot connect: [{0}:{1}]", m_loginserver_address, m_loginserver_port);
		database.LSDisconnect();
		return false;
	}

	if (m_is_legacy) {
		m_legacy_client.reset(new EQ::Net::ServertalkLegacyClient(m_loginserver_address, m_loginserver_port, false));

		m_legacy_client->OnConnect(
			[this](EQ::Net::ServertalkLegacyClient* client) {
			if (client) {
				LogInfo(
					"Connected to Legacy Loginserver: [{0}:{1}]",
					m_loginserver_address,
					m_loginserver_port
				);

				SendNewInfo();
				SendStatus();
				zoneserver_list.SendLSZones();
				m_statusupdate_timer.reset(new EQ::Timer(LoginServer_StatusUpdateInterval, true, [this](EQ::Timer* t) {
					SendStatus();
				}
				));
			}
			else {
				LogInfo(
					"Could not connect to Legacy Loginserver: [{0}:{1}]",
					m_loginserver_address,
					m_loginserver_port
				);
			}
		}
		);

		m_legacy_client->OnMessage(ServerOP_UsertoWorldReq, std::bind(&LoginServer::ProcessUsertoWorldReq, this, std::placeholders::_1, std::placeholders::_2));
		m_legacy_client->OnMessage(ServerOP_LSClientAuth, std::bind(&LoginServer::ProcessLSClientAuth, this, std::placeholders::_1, std::placeholders::_2));
		m_legacy_client->OnMessage(ServerOP_LSFatalError, std::bind(&LoginServer::ProcessLSFatalError, this, std::placeholders::_1, std::placeholders::_2));
		m_legacy_client->OnMessage(ServerOP_SystemwideMessage, std::bind(&LoginServer::ProcessSystemwideMessage, this, std::placeholders::_1, std::placeholders::_2));
		m_legacy_client->OnMessage(ServerOP_LSRemoteAddr, std::bind(&LoginServer::ProcessLSRemoteAddr, this, std::placeholders::_1, std::placeholders::_2));
		m_legacy_client->OnMessage(ServerOP_LSAccountUpdate, std::bind(&LoginServer::ProcessLSAccountUpdate, this, std::placeholders::_1, std::placeholders::_2));
	}
	else {
		m_client.reset(new EQ::Net::ServertalkClient(m_loginserver_address, m_loginserver_port, false, "World", ""));
		m_client->OnConnect([this](EQ::Net::ServertalkClient* client) {
			if (client) {
				LogInfo("Connected to Loginserver: {}:{}", m_loginserver_address, m_loginserver_port);
				SendNewInfo();
				SendStatus();
				zoneserver_list.SendLSZones();
				m_statusupdate_timer.reset(new EQ::Timer(LoginServer_StatusUpdateInterval, true, [this](EQ::Timer* t) {
					SendStatus();
				}
				));
			}
			else {
				LogInfo("Could not connect to Loginserver: {}:{}", m_loginserver_address, m_loginserver_port);
			}
		});

		m_client->OnMessage(ServerOP_UsertoWorldReq, std::bind(&LoginServer::ProcessUsertoWorldReq, this, std::placeholders::_1, std::placeholders::_2));
		m_client->OnMessage(ServerOP_LSClientAuth, std::bind(&LoginServer::ProcessLSClientAuth, this, std::placeholders::_1, std::placeholders::_2));
		m_client->OnMessage(ServerOP_LSFatalError, std::bind(&LoginServer::ProcessLSFatalError, this, std::placeholders::_1, std::placeholders::_2));
		m_client->OnMessage(ServerOP_SystemwideMessage, std::bind(&LoginServer::ProcessSystemwideMessage, this, std::placeholders::_1, std::placeholders::_2));
		m_client->OnMessage(ServerOP_LSRemoteAddr, std::bind(&LoginServer::ProcessLSRemoteAddr, this, std::placeholders::_1, std::placeholders::_2));
		m_client->OnMessage(ServerOP_LSAccountUpdate, std::bind(&LoginServer::ProcessLSAccountUpdate, this, std::placeholders::_1, std::placeholders::_2));
	}
	return true;
}

void LoginServer::SendInfo() {
	const WorldConfig* Config = WorldConfig::get();

	auto pack = new ServerPacket;
	pack->opcode = ServerOP_LSInfo;
	pack->size = sizeof(ServerLSInfo_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerLSInfo_Struct* l = (ServerLSInfo_Struct*)pack->pBuffer;
	strcpy(l->protocolversion, EQEMU_PROTOCOL_VERSION);
	strcpy(l->serverversion, LOGIN_VERSION);
	strcpy(l->name, Config->LongName.c_str());
	strn0cpy(l->account, m_login_account.c_str(), 30);
	strn0cpy(l->password, m_login_password.c_str(), 30);
	strcpy(l->address, Config->WorldAddress.c_str());
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendNewInfo() {
	uint16 port;
	const WorldConfig* Config = WorldConfig::get();

	auto pack = new ServerPacket;
	pack->opcode = ServerOP_NewLSInfo;
	pack->size = sizeof(LoginserverNewWorldRequest);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	LoginserverNewWorldRequest * lsi = (LoginserverNewWorldRequest *) pack->pBuffer;
	strcpy(lsi->protocolversion, EQEMU_PROTOCOL_VERSION);
	strcpy(lsi->serverversion, LOGIN_VERSION);
	strcpy(lsi->name, Config->LongName.c_str());
	strcpy(lsi->shortname, Config->ShortName.c_str());
	strn0cpy(lsi->account, m_login_account.c_str(), 30);
	strn0cpy(lsi->password, m_login_password.c_str(), 30);
	if (Config->WorldAddress.length()) {
		strcpy(lsi->remote_address, Config->WorldAddress.c_str());
	}
	if (Config->LocalAddress.length()) {
		strcpy(lsi->local_address, Config->LocalAddress.c_str());
	}
	else {
		auto local_addr = m_is_legacy ? m_legacy_client->Handle()->LocalIP() : m_client->Handle()->LocalIP();
		strcpy(lsi->local_address, local_addr.c_str());
		WorldConfig::SetLocalAddress(lsi->local_address);
	}

	SendPacket(pack);
	delete pack;
}

void LoginServer::SendStatus() {
	auto pack = new ServerPacket;
	pack->opcode = ServerOP_LSStatus;
	pack->size = sizeof(ServerLSStatus_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerLSStatus_Struct* lss = (ServerLSStatus_Struct*)pack->pBuffer;

	if (WorldConfig::get()->Locked)
		lss->status = -2;
	else if (numzones <= 0)
		lss->status = -1;
	else
		lss->status = client_list.GetClientCount() > 0 ? client_list.GetClientCount() : 0;

	lss->num_zones = numzones;
	lss->num_players = client_list.GetClientCount();
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendPacket(ServerPacket* pack)
{
	if (m_legacy_client) {
		m_legacy_client->SendPacket(pack);
	}
	else if (m_client) {
		m_client->SendPacket(pack);
	}
}

void LoginServer::SendAccountUpdate(ServerPacket* pack) 
{
	if (m_client == nullptr && m_legacy_client == nullptr) {
		LogDebug("No client to send account update to loginserver");
		return;
	}

	auto* ls_account_update = (ServerLSAccountUpdate_Struct*)pack->pBuffer;
	if (CanUpdate()) {
		LogInfo("Sending ServerOP_LSAccountUpdate packet to loginserver: [{}]:[{}]", m_loginserver_address, m_loginserver_port);
		strn0cpy(ls_account_update->worldaccount, m_login_account.c_str(), 30);
		strn0cpy(ls_account_update->worldpassword, m_login_password.c_str(), 30);
		SendPacket(pack);
	}
}
