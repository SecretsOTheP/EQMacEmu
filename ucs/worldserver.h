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
#ifndef WORLDSERVER_H
#define WORLDSERVER_H

#include "../common/net/servertalk_client_connection.h"
#include "../common/eq_packet_structs.h"
#include "../common/timer.h"
#include "database.h"
#include <memory>

class EQApplicationPacket;

struct WorldServerConfig
{
	std::string WorldShortName;

	std::string WorldIP;
	uint16 WorldTCPPort;
	std::string SharedKey;

	std::string DatabaseHost;
	std::string DatabaseUsername;
	std::string DatabasePassword;
	std::string DatabaseDB;
	uint16 DatabasePort;
};

class WorldServer
{
public:
	WorldServer(WorldServerConfig *config);
	~WorldServer();
	void ProcessMessage(uint16 opcode, EQ::Net::Packet &);

	UCSDatabase &GetUCSDatabase() { return m_database; }
	void PingUCSDatabase();

private:
	std::unique_ptr<EQ::Net::ServertalkClient> m_connection;

	WorldServerConfig *m_config;
	UCSDatabase m_database;
	Timer m_dbping_timer;
};

class WorldServerList
{
public:
	WorldServerList();
	void Process();
	WorldServer *GetWorldServer(std::string WorldShortName) { return m_worldservers.count(WorldShortName) == 1 ? m_worldservers[WorldShortName] : nullptr; }
	WorldServer *GetMainWorldServer() { return GetWorldServer(m_mainshortname); }
	size_t GetServerCount() { return m_worldservers.size(); }

private:
	std::string m_mainshortname;
	std::map<std::string, WorldServer *> m_worldservers;
};

#endif