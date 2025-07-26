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
#include <algorithm>
#include <vector>

#include "../common/version.h"
#include "../common/servertalk.h"
#include "../common/queue_packets.h"  // Queue-specific opcodes and structures
#include "../common/misc_functions.h"
#include "../common/eq_packet_structs.h"
#include "../common/packet_dump.h"
#include <fmt/format.h>
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
#include "world_queue.h"
#include <mutex>
extern ZSList        zoneserver_list;
extern ClientList    client_list;
extern uint32        numzones;
extern volatile bool RunLoops;
extern std::mutex ipMutex;
extern std::unordered_set<uint32> ipWhitelist;

// Global pointer for other files to access the primary LoginServer instance
LoginServer* loginserver = nullptr;

LoginServer::LoginServer(const char* iAddress, uint16 iPort, const char* Account, const char* Password, uint8 Type)
{
	strn0cpy(m_loginserver_address, iAddress, 256);
	m_loginserver_port = iPort;
	m_login_account = Account;
	m_login_password = Password;
	m_can_account_update = false;
	m_is_legacy = Type == 1;
	
	// Set global pointer to first LoginServer instance for other files to access
	if (!loginserver) {
		loginserver = this;
	}
	
	Connect();
}

LoginServer::~LoginServer() {
	// Clear global pointer if it was pointing to this instance
	if (loginserver == this) {
		loginserver = nullptr;
	}
}

void LoginServer::ProcessUsertoWorldReq(uint16_t opcode, EQ::Net::Packet& p)
{
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	QueueDebugLog(1, "ProcessUsertoWorldReq called - checking if queue logic is triggered");

	UsertoWorldRequest* utwr = (UsertoWorldRequest*)p.Data();
	uint32                     id = database.GetAccountIDFromLSID(utwr->lsaccountid);
	int16                      status = database.CheckStatus(id);
	bool check_forum_name = true;
	// Handle new accounts that don't have world accounts yet
	if (id == 0) {
		LogInfo("No world account found for LS account [{}] - will be created during authentication", utwr->lsaccountid);
		// For queue purposes, we'll use the LS account ID temporarily
		// The actual world account will be created during the authentication process
		id = utwr->lsaccountid;  // Temporary fallback for new accounts
		status = 0; // Default status for new accounts
		check_forum_name = false;
	}
	
	bool mule = false;
	uint16 expansion = 0;
	uint32 force_guild_id = 0;
	char forum_name[31] = { 0 };
	database.GetAccountRestriction(id, forum_name, expansion, mule, force_guild_id);


	if (p.Length() == sizeof(UsertoWorldRequest))
	{
		if (check_forum_name && forum_name[0] == '\0' && utwr->forum_name[0] != '\0')
		{
			database.SetForumName(id, utwr->forum_name);
		}
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
	if (status == -1) // Suspended
		utwrs->response = -1;
	if (status == -2) // Banned
		utwrs->response = -2;

	if (utwrs->response == 1)
	{
		// Active account?
		if (RuleI(World, AccountSessionLimit) >= 0 && status < (RuleI(World, ExemptAccountLimitStatus)) && (RuleI(World, ExemptAccountLimitStatus) != -1) && client_list.CheckAccountActive(id))
			utwrs->response = -4;
	}
	if (utwrs->response == 1)
	{
		// Client IP limit?
		if (!mule && RuleI(World, MaxClientsPerIP) >= 0 && !client_list.CheckIPLimit(id, utwr->ip, status))
			utwrs->response = -5;
	}

	if (utwrs->response == 1)
	{
		// Mule IP limit?
		if (mule && RuleI(World, MaxMulesPerIP) >= 0 && !client_list.CheckMuleLimit(id, utwr->ip, status))
			utwrs->response = -5;
	}

	// Check if queue bypass is enabled for certain accounts
	bool auto_connect = false;
	
	// Correct auto-connect detection: FromID = 1 means auto-connect, FromID = 0 means manual PLAY
	if (utwr->FromID == 1) {
		auto_connect = true;
		LogInfo("Auto-connect request detected from LS account [{}]", utwr->lsaccountid);
	} else {
		QueueDebugLog(2, "Manual PLAY request from LS account [{}]", utwr->lsaccountid);
	}
	
	// Get queue capacity from rules
	uint32 effective_population = GetWorldPop();
	uint32 queue_cap = RuleI(Quarm, PlayerPopulationCap);
	
	// Check if queue system is disabled via rules
	if (!RuleB(Quarm, EnableQueue)) {
		LogInfo("Queue system disabled via rules.");
		if (effective_population >= queue_cap){
			LogInfo("SERVER AT CAPACITY - but queue disabled - rejecting connection (pop: {}/{})", effective_population, queue_cap);
			utwrs->response = -3; // Population too high
		}
	}
	else { // Queue enabled
		QueueDebugLog(1, "Capacity check: {} >= {}", effective_population, queue_cap);
		
		// At capacity check - use centralized decision logic
		if (effective_population >= queue_cap) {
			LogInfo("SERVER AT CAPACITY - using centralized queue decision logic");
			
			// Build connection request for centralized evaluation
			ConnectionRequest request = {};
			request.account_id = id;                    // world account ID
			request.ls_account_id = utwr->lsaccountid; // login server account ID
			request.ip_address = utwr->ip;
			request.status = status;                    // GM level for bypass checks
			request.is_auto_connect = auto_connect;    // Auto-connect vs manual PLAY
			request.is_mule = mule;
			request.ip_str = inet_ntoa(*(struct in_addr*)&utwr->ip);
			if (p.Length() == sizeof(UsertoWorldRequest))
			{
				request.forum_name = utwr->forum_name;
				request.client_key = utwr->client_key;
			}
			request.world_account_id = id;
			
			// CENTRALIZED DECISION: Let queue manager handle ALL queue logic
			bool should_override_capacity = queue_manager.EvaluateConnectionRequest(request, queue_cap, utwrs, nullptr);
			
			if (should_override_capacity) {
				LogInfo("APPROVED bypass for account [{}] - allowing connection", id);
				// Connection approved by queue manager - allow through
			} else {
				LogInfo("QueueManager decision for account [{}] - response code [{}]", id, utwrs->response);
				// Queue manager already set the appropriate response code (-6 for queue, -7 for toggle)
			}
		} else {
			// Final anti-spam check: If queue logic approved them BUT there are still queued players, force to queue
			// This prevents timing attacks where players slip through during brief capacity windows
			if (queue_manager.GetTotalQueueSize() > 0 && !auto_connect && RuleB(Quarm, EnableQueue) && utwrs->response == 1 ) {
				LogInfo("ANTI-SPAM: Account [{}] approved by queue logic but {} players still waiting - forcing to queue to prevent timing attack", 
					id, queue_manager.GetTotalQueueSize());
				utwrs->response = -6; // Force to queue
			}
			LogInfo("Server NOT at capacity - allowing connection (pop: {}/{})", effective_population, queue_cap);
		}
	}
	// Only add to IP whitelist and log success if connection is actually allowed
	if (utwrs->response == 1) {
		QueueDebugLog(2, "Connection approved - adding IP to whitelist");
		ipMutex.lock();
		ipWhitelist.insert(utwr->ip);
		ipMutex.unlock();
	} else {
		QueueDebugLog(1, "Connection not approved (response: {})", utwrs->response);
	}

	utwrs->worldid = utwr->worldid;
	
	QueueDebugLog(1, "About to send response [{}] to login server for LS account [{}]", utwrs->response, utwr->lsaccountid);
	SendPacket(outpack);
	QueueDebugLog(1, "Response packet sent successfully");
	delete outpack;
}

void LoginServer::ProcessLSClientAuth(uint16_t opcode, EQ::Net::Packet& p) {
	const WorldConfig* Config = WorldConfig::get();
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	try {
		auto slsca = p.GetSerialize<ClientAuth>(0);
		client_list.CLEAdd(slsca.loginserver_account_id, slsca.account_name, nullptr, slsca.key, slsca.is_world_admin, slsca.ip_address, slsca.is_client_from_local_network, slsca.version);
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


void LoginServer::ProcessQueueRemoval(uint16_t opcode, EQ::Net::Packet& p) {
	LogNetcode("Received ServerPacket from LS OpCode {:#04x}", opcode);

	if (p.Length() < sizeof(ServerQueueRemoval_Struct)) {
		LogError("Received ServerOP_RemoveFromQueue packet that was too small");
		return;
	}
	
	if (!RuleB(Quarm, EnableQueue)) {
		LogDebug("Queue system is disabled - ignoring single removal request");
		return;
	}

	ServerQueueRemoval_Struct* removal = (ServerQueueRemoval_Struct*)p.Data();
	
	LogInfo("Received single removal request from login server for disconnected client [{}]", removal->ls_account_id);
	
	// Use queue manager's single removal function
	queue_manager.RemoveFromQueue(removal->ls_account_id);
	
	LogInfo("Completed single removal for disconnected client [{}]", removal->ls_account_id);
}

uint32 LoginServer::CalculateSmartWaitTime(uint32 queue_position, uint32 current_population, uint32 max_capacity) const
{
	// Base calculation: 60 seconds per position (fallback)
	uint32 base_wait = queue_position * 60;
	
	// Calculate server utilization percentage (0-100)
	uint32 utilization_percent = (current_population * 100) / max_capacity;
	
	// Adjust wait time based on how full the server is
	float capacity_multiplier = 1.0f;
	if (utilization_percent >= 95) {
		// Server is nearly full - slower movement
		capacity_multiplier = 1.8f;  // 80% longer wait
	} else if (utilization_percent >= 90) {
		// Server is very full - moderately slower
		capacity_multiplier = 1.4f;  // 40% longer wait
	} else if (utilization_percent >= 80) {
		// Server is quite full - slightly slower  
		capacity_multiplier = 1.2f;  // 20% longer wait
	} else {
		// Server has room - normal or faster movement
		capacity_multiplier = 0.8f;  // 20% shorter wait
	}
	
	// Apply queue position scaling (larger queues move more slowly)
	float queue_scale = 1.0f;
	if (queue_position > 50) {
		queue_scale = 1.3f;  // Large queue penalty
	} else if (queue_position > 20) {
		queue_scale = 1.1f;  // Medium queue penalty  
	}
	
	// Calculate final wait time with bounds checking
	uint32 smart_wait = (uint32)(base_wait * capacity_multiplier * queue_scale);
	
	// Reasonable bounds: minimum 30 seconds, maximum 20 minutes per position
	uint32 min_wait = queue_position * 30;
	uint32 max_wait = queue_position * 1200; // 20 minutes max per position
	
	smart_wait = std::max(min_wait, std::min(smart_wait, max_wait));
	
	return smart_wait;
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
		m_legacy_client->OnMessage(ServerOP_RemoveFromQueue, std::bind(&LoginServer::ProcessQueueRemoval, this, std::placeholders::_1, std::placeholders::_2));
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
		m_client->OnMessage(ServerOP_RemoveFromQueue, std::bind(&LoginServer::ProcessQueueRemoval, this, std::placeholders::_1, std::placeholders::_2));
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

	uint32 population = GetWorldPop();
	
	if (WorldConfig::get()->Locked)
		lss->status = -2;
	else if (numzones <= 0)
		lss->status = -1;
	else
		lss->status = population;

	lss->num_zones = numzones;
	lss->num_players = population;
	
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

