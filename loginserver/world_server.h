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
#ifndef EQEMU_WORLDSERVER_H
#define EQEMU_WORLDSERVER_H

#include "../common/global_define.h"
#include "../common/net/servertalk_server_connection.h"
#include "../common/servertalk.h"
#include "../common/packet_dump.h"
#include "../../world/world_queue.h"        // For shared QUEUE_DEBUG_LEVEL
#include <string>
#include <memory>
#include "../common/emu_opcodes.h"

// Forward declarations
class EQApplicationPacket;
class Client;
struct UsertoWorldResponse;

/**
 * World server class, controls the connected server processing.
 */
class WorldServer
{
public:
	/**
	* Constructor, sets our connection to c.
	*/
	WorldServer(std::shared_ptr<EQ::Net::ServertalkServerConnection> c);

	/**
	* Destructor, frees our connection if it exists.
	*/
	~WorldServer();

	/**
	* Resets the basic stats of this server.
	*/
	void Reset();

	/**
	* Accesses connection, it is intentional that this is not const (trust me).
	*/
	std::shared_ptr<EQ::Net::ServertalkServerConnection> GetConnection() { return m_connection; }

	/**
	* Sets the connection to c.
	*/
	void SetConnection(std::shared_ptr<EQ::Net::ServertalkServerConnection> c) { m_connection = c; }

	/**
	 * @return
	 */
	unsigned int GetServerId() const { return m_server_id; }
	void SetServerId(unsigned int id) { m_server_id = id; }

	/**
	 * @return
	 */
	std::string GetServerLongName() const { return m_long_name; }
	std::string GetServerShortName() const { return m_short_name; }

	/**
	* Gets whether the server is authorized to show up on the server list or not.
	*/
	bool IsAuthorized() const { return m_is_server_authorized; }

	/**
	* Gets the local ip of the server.
	*/
	std::string GetLocalIP() const { return m_local_ip; }

	/**
	* Gets the remote ip of the server.
	*/
	std::string GetRemoteIP() const { return m_remote_ip_address; }

	/**
	* Gets what kind of server this server is (legends, preferred, normal)
	*/
	unsigned int GetServerListID() const { return m_server_list_type_id; }
	WorldServer* SetServerListTypeId(unsigned int in_server_list_id);

	/**
	* Gets the status of the server.
	*/
	int GetStatus() const { return m_server_status; }

	/**
	* Gets the number of zones online on the server.
	*/
	unsigned int GetZonesBooted() const { return m_zones_booted; }

	/**
	* Gets the number of players on the server.
	*/
	unsigned int GetPlayersOnline() const { return m_players_online; }

	/**
	* Takes the info struct we received from world and processes it.
	*/
	void Handle_NewLSInfo(LoginserverNewWorldRequest * i);

	/**
	* Takes the status struct we received from world and processes it.
	*/
	void Handle_LSStatus(ServerLSStatus_Struct *server_login_status);

	/**
	* Informs world that there is a client incoming with the following data.
	*/
	void SendClientAuth(std::string ip, std::string account, std::string key, unsigned int account_id, uint8 version = 0);
	void ProcessQueueAutoConnect(uint16_t opcode, const EQ::Net::Packet& p);
	void ProcessQueueDirectUpdate(uint16_t opcode, const EQ::Net::Packet& p);
	void ProcessQueueBatchUpdate(uint16_t opcode, const EQ::Net::Packet& p);
	void ProcessWorldListUpdate(uint16_t opcode, const EQ::Net::Packet& p);

private:
	bool RuleB_Get(const std::string& rule_name, bool default_value);
	
	/**
	* Packet processing functions:
	*/
	void ProcessNewLSInfo(uint16_t opcode, const EQ::Net::Packet& p);
	void ProcessLSStatus(uint16_t opcode, const EQ::Net::Packet& p);
	void ProcessUsertoWorldResp(uint16_t opcode, const EQ::Net::Packet& p);
	void ProcessLSAccountUpdate(uint16_t opcode, const EQ::Net::Packet& p);

	std::shared_ptr<EQ::Net::ServertalkServerConnection> m_connection;

	unsigned int m_zones_booted;
	unsigned int m_players_online;
	int m_server_status;
	unsigned int m_server_id;
	unsigned int m_server_list_type_id;
	unsigned int m_server_process_type;
	std::string m_server_description;
	std::string m_long_name;
	std::string m_short_name;
	std::string m_account_name;
	std::string m_account_password;
	std::string m_remote_ip_address;
	std::string m_local_ip;
	std::string m_protocol;
	std::string m_version;
	bool m_is_server_authorized;
	bool m_is_server_logged_in;
	bool m_is_server_trusted;
};

#endif
