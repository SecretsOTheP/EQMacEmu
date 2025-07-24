/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2010 EQEMu Development Team (http://eqemulator.net)

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
#include "server_manager.h"
#include "login_server.h"
#include "login_types.h"
#include <stdlib.h>

#include "../common/eqemu_logsys.h"
#include "../common/ip_util.h"
#include <fmt/format.h>
#include <arpa/inet.h>
#include <vector>
#include <tuple>
#include <map>
#include "../common/queue_packets.h"

extern EQEmuLogSys LogSys;
extern LoginServer server;
extern bool run_server;

// Global cache for server effective population by server ID
extern std::map<uint32, uint32> g_server_populations;

ServerManager::ServerManager()
{
	int listen_port = server.config.GetVariableInt("client_configuration", "listen_port", 5998);

	m_server_connection = std::make_unique<EQ::Net::ServertalkServer>();
	EQ::Net::ServertalkServerOptions opts;
	opts.port = listen_port;
	opts.ipv6 = false;
	m_server_connection->Listen(opts);

	LogInfo("Loginserver now listening on port [{0}]", listen_port);

	m_server_connection->OnConnectionIdentified(
		"World", [this](std::shared_ptr<EQ::Net::ServertalkServerConnection> world_connection) {
			LogInfo(
				"New World Server connection from {0}:{1}",
				world_connection->Handle()->RemoteIP(),
				world_connection->Handle()->RemotePort()
			);
				
			auto iter = m_world_servers.begin();
			while (iter != m_world_servers.end()) {
				if ((*iter)->GetConnection()->Handle()->RemoteIP().compare(world_connection->Handle()->RemoteIP()) ==
					0 &&
					(*iter)->GetConnection()->Handle()->RemotePort() == world_connection->Handle()->RemotePort()) {

					LogInfo(
						"World server already existed for {0}:{1}, removing existing connection.",
						world_connection->Handle()->RemoteIP(),
						world_connection->Handle()->RemotePort()
					);

					m_world_servers.erase(iter);
					break;
				}

				++iter;
			}

			m_world_servers.push_back(std::make_unique<WorldServer>(world_connection));
		}
	);

	m_server_connection->OnConnectionRemoved(
		"World", [this](std::shared_ptr<EQ::Net::ServertalkServerConnection> c) {
			auto iter = m_world_servers.begin();
			while (iter != m_world_servers.end()) {
				if ((*iter)->GetConnection()->GetUUID() == c->GetUUID()) {
					LogInfo(
						"World server {0} has been disconnected, removing.",
						(*iter)->GetServerLongName()
					);
					m_world_servers.erase(iter);
					return;
				}

				++iter;
			}
		}
	);
}

ServerManager::~ServerManager() = default;


WorldServer* ServerManager::GetServerByAddress(const std::string& addr, int port)
{
	auto iter = m_world_servers.begin();
	while (iter != m_world_servers.end()) {
		if ((*iter)->GetConnection()->Handle()->RemoteIP() == addr && (*iter)->GetConnection()->Handle()->RemotePort()) {
			return (*iter).get();
		}
		++iter;
	}

	return nullptr;
}

EQApplicationPacket* ServerManager::CreateServerListPacket(Client* c)
{
	unsigned int packet_size = sizeof(ServerList_Struct);
	unsigned int server_count = 0;
	in_addr in;
	in.s_addr = c->GetConnection()->GetRemoteIP();
	std::string client_ip = inet_ntoa(in);
	auto iter = m_world_servers.begin();
	while (iter != m_world_servers.end()) {
		if ((*iter)->IsAuthorized() == false) {
			++iter;
			continue;
		}

		std::string world_ip = (*iter)->GetConnection()->Handle()->RemoteIP();

		std::string servername = (*iter)->GetServerLongName().c_str();
		servername.append(" Server");

		if(world_ip.compare(client_ip) == 0) {
			packet_size += servername.size() + 1 + (*iter)->GetLocalIP().size() + 1 + sizeof(ServerListServerFlags_Struct);
		}
		else if (IpUtil::IsIpInPrivateRfc1918(client_ip)) {
			LogInfo("Client is requesting server list from a local address [{0}]", client_ip);
			packet_size += servername.size() + 1 + (*iter)->GetLocalIP().size() + 1 + sizeof(ServerListServerFlags_Struct);
		}
		else {
			packet_size += servername.size() + 1 + (*iter)->GetRemoteIP().size() + 1 + sizeof(ServerListServerFlags_Struct);
		}

		server_count++;
		++iter;
	}

	packet_size += sizeof(ServerListEndFlags_Struct); // flags and unknowns
	packet_size += 1; // flags and unknowns
	auto outapp = new EQApplicationPacket(OP_ServerListRequest, packet_size);
	ServerList_Struct* sl = (ServerList_Struct*)outapp->pBuffer;
	sl->numservers = server_count;
	uint8 showcount = 0x0;
	if (server.db->CheckExtraSettings("pop_count"))	{
		if (server.db->LoginSettings("pop_count") == "1") {
			showcount = 0xFF;
		}
	}
	sl->showusercount = showcount;

	unsigned char* data_ptr = outapp->pBuffer;
	data_ptr += sizeof(ServerList_Struct);

	iter = m_world_servers.begin();
	while (iter != m_world_servers.end()) {
		if ((*iter)->IsAuthorized() == false) {
			++iter;
			continue;
		}

		std::string world_ip = (*iter)->GetConnection()->Handle()->RemoteIP();

		std::string servername = (*iter)->GetServerLongName().c_str();
		servername.append(" Server");

		memcpy(data_ptr, servername.c_str(), servername.size());
		data_ptr += (servername.size() + 1);

		if (world_ip.compare(client_ip) == 0) {
			memcpy(data_ptr, (*iter)->GetLocalIP().c_str(), (*iter)->GetLocalIP().size());
			data_ptr += ((*iter)->GetLocalIP().size() + 1);
		}
		else if (IpUtil::IsIpInPrivateRfc1918(client_ip)) {
			memcpy(data_ptr, (*iter)->GetLocalIP().c_str(), (*iter)->GetLocalIP().size());
			data_ptr += ((*iter)->GetLocalIP().size() + 1);
		}
		else {
			memcpy(data_ptr, (*iter)->GetRemoteIP().c_str(), (*iter)->GetRemoteIP().size());
			data_ptr += ((*iter)->GetRemoteIP().size() + 1);
		}

		ServerListServerFlags_Struct* slsf = (ServerListServerFlags_Struct*)data_ptr;
		slsf->greenname = 0;
		if (server.db->GetWorldPreferredStatus((*iter)->GetServerId())) {
			slsf->greenname = 1;
		}
		slsf->flags = 0x1;
		slsf->worldid = (*iter)->GetServerId();
		slsf->usercount = (*iter)->GetStatus();
		// Check if client has a queue position for this server - show queue position
		if (c->HasQueuePosition() && c->GetQueueServerID() == (*iter)->GetServerId()) {
			slsf->usercount = c->GetQueuePosition();
		}  else if (g_server_populations.find((*iter) ->GetServerId()) != g_server_populations.end()) /* INF Only uses it if it's initated so won't impact TAKP */{
			slsf->usercount = g_server_populations[(*iter)->GetServerId()];
		}
		
		data_ptr += sizeof(ServerListServerFlags_Struct);
		++iter;
	}
	ServerListEndFlags_Struct* slef = (ServerListEndFlags_Struct*)data_ptr;
	slef->admin = 0;
	return outapp;
}

void ServerManager::SendUserToWorldRequest(const char* server_id, unsigned int client_account_id, uint32 ip, bool is_auto_connect, const std::string& client_key)
{
	auto iter = m_world_servers.begin();
	bool found = false;
	while (iter != m_world_servers.end()) {
		if ((*iter)->GetRemoteIP() == server_id || (*iter)->GetLocalIP() == server_id) {
			EQ::Net::DynamicPacket outapp;
			outapp.Resize(sizeof(UsertoWorldRequest));
			UsertoWorldRequest* utwr = (UsertoWorldRequest*)outapp.Data();

			//utwr->worldid = (*iter)->GetServerListID(); //This pulls preffered status instead of actual ID? That does not seem right.
			utwr->worldid = (*iter)->GetServerId();

			utwr->lsaccountid = client_account_id;
			utwr->ip = ip;
			
			// Encode auto-connect status in FromID field
			// 0 = manual PLAY request, 1 = auto-connect request  
			utwr->FromID = is_auto_connect ? 1 : 0;
			utwr->ToID = 0; // Not used
			
			// Store client key in forum_name field (repurposing for queue system)
			strncpy(utwr->forum_name, client_key.c_str(), sizeof(utwr->forum_name) - 1);
			utwr->forum_name[sizeof(utwr->forum_name) - 1] = '\0';
			
			(*iter)->GetConnection()->Send(ServerOP_UsertoWorldReq, outapp);
			found = true;

			LogPacketServerClient(
				"[{}]",
				outapp.ToString()
			);
		}

		++iter;
	}

	if (!found) {
		LogError("Client requested a user to world but supplied an invalid id of {}.", server_id);
	}
}

bool ServerManager::ServerExists(std::string l_name, std::string s_name, WorldServer *ignore)
{
	auto iter = m_world_servers.begin();
	while (iter != m_world_servers.end()) {
		if ((*iter).get() == ignore) {
			++iter;
			continue;
		}

		if ((*iter)->GetServerLongName().compare(l_name) == 0 && (*iter)->GetServerShortName().compare(s_name) == 0) {
			return true;
		}

		++iter;
	}
	return false;
}

void ServerManager::DestroyServerByName(std::string l_name, std::string s_name, WorldServer *ignore)
{
	auto iter = m_world_servers.begin();
	while (iter != m_world_servers.end()) {
		if ((*iter).get() == ignore) {
			++iter;
			continue;
		}

		if ((*iter)->GetServerLongName().compare(l_name) == 0 && (*iter)->GetServerShortName().compare(s_name) == 0) {
			(*iter)->GetConnection()->Handle()->Disconnect();
			iter = m_world_servers.erase(iter);
			continue;
		}

		++iter;
	}
}
