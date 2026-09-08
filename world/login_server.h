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
#ifndef LOGINSERVER_H
#define LOGINSERVER_H

#include "../common/servertalk.h"
#include "../common/linked_list.h"
#include "../common/timer.h"
#include "../common/queue.h"
#include "../common/eq_packet_structs.h"
#include "../common/mutex.h"
#include "../common/net/servertalk_client_connection.h"
#include "../common/net/servertalk_legacy_client_connection.h"
#include "../common/event/timer.h"
#include <memory>

struct QueueDecision;

class LoginServer {
public:
	LoginServer(const char*, uint16, const char*, const char*, uint8);
	~LoginServer();

	bool Connect();
	void SendInfo();
	void SendNewInfo();
	void SendStatus();

	void SendPacket(ServerPacket* pack);
	void SendAccountUpdate(ServerPacket* pack);
	// ServerOP_UsertoWorldQueueInfo, sent right before a -3 response for an account that was queued.
	void SendQueueInfo(uint32 lsaccountid, uint32 worldid, const QueueDecision& decision);
	// The queue is used toward this login server only when the rule is on and it announced support;
	// otherwise the old raw-count refusal runs, so nobody is queued without seeing it.
	bool QueueCapable() const { return m_queue_capable; }
	bool QueueActive() const;
	bool Connected() 
	{ 
		if (m_is_legacy) {
			if (m_legacy_client) {
				return m_legacy_client->Connected();
			}
		} 
		else {
			if (m_client) {
				return m_client->Connected();
			}
		}

		return false;
	}
	bool CanUpdate() { return m_can_account_update; }

private:
	void ProcessUsertoWorldReq(uint16_t opcode, EQ::Net::Packet& p);
	void ProcessLSClientAuth(uint16_t opcode, EQ::Net::Packet& p);
	void ProcessLSFatalError(uint16_t opcode, EQ::Net::Packet& p);
	void ProcessSystemwideMessage(uint16_t opcode, EQ::Net::Packet& p);
	void ProcessLSRemoteAddr(uint16_t opcode, EQ::Net::Packet& p);
	void ProcessLSAccountUpdate(uint16_t opcode, EQ::Net::Packet& p);
	void ProcessLSQueueCapable(uint16_t opcode, EQ::Net::Packet& p); // login server announced it displays queue info

	std::unique_ptr<EQ::Net::ServertalkClient>       m_client;
	std::unique_ptr<EQ::Net::ServertalkLegacyClient> m_legacy_client;
	std::unique_ptr<EQ::Timer>                       m_statusupdate_timer;
	char	                                         m_loginserver_address[256];
	uint32	                                         m_loginserver_ip;
	uint16	                                         m_loginserver_port;
	std::string	                                     m_login_account;
	std::string                                      m_login_password;
	bool	                                         m_can_account_update;
	bool	                                         m_is_legacy;
	bool                                             m_queue_capable = false;
};
#endif
