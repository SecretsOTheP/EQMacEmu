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

#ifdef _WINDOWS
	#include <process.h>
	#include <winsock2.h>
	#include <windows.h>

	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#else // Pyro: fix for linux
	#include <sys/socket.h>
#ifdef FREEBSD //Timothy Whitman - January 7, 2003
	#include <sys/types.h>
#endif
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <pthread.h>
	#include <unistd.h>
	#include <errno.h>

	#include "../common/unix.h"

	#define SOCKET_ERROR -1
	#define INVALID_SOCKET -1
	extern int errno;
#endif

#define IGNORE_LS_FATAL_ERROR

#include "../common/servertalk.h"
#include "login_server.h"
#include "login_server_list.h"
#include "../common/eq_packet_structs.h"
#include "../common/packet_dump.h"
#include "../common/strings.h"
#include "zoneserver.h"
#include "worlddb.h"
#include "zonelist.h"
#include "clientlist.h"
#include "world_config.h"
#include <mutex>
extern ZSList zoneserver_list;
extern ClientList client_list;
extern uint32 numzones;
extern uint32 numplayers;
extern volatile bool	RunLoops;
extern std::mutex ipMutex;
extern std::unordered_set<uint32> ipWhitelist;

LoginServer::LoginServer(const char* iAddress, uint16 iPort, const char* Account, const char* Password, uint8 Type)
: statusupdate_timer(LoginServer_StatusUpdateInterval)
{
	strn0cpy(LoginServerAddress,iAddress,256);
	LoginServerPort = iPort;
	strn0cpy(LoginAccount,Account,16);
	strn0cpy(LoginPassword,Password,16);
	LoginServerType = Type;
	CanAccountUpdate = false;
	tcpc = new EmuTCPConnection(true);
	tcpc->SetPacketMode(EmuTCPConnection::packetModeLogin);
}

LoginServer::~LoginServer() {
	delete tcpc;
}

bool LoginServer::Process() {
	const WorldConfig *Config=WorldConfig::get();

	if (statusupdate_timer.Check()) {
		this->SendStatus();
	}

	/************ Get all packets from packet manager out queue and process them ************/
	ServerPacket *pack = 0;
	while((pack = tcpc->PopPacket()))
	{
		LogNetcode("Received ServerPacket from LS OpCode {:#04x}", pack->opcode);

		switch(pack->opcode) {
			case 0:
				break;
			case ServerOP_KeepAlive: {
				// ignore this
				break;
			}
			case ServerOP_UsertoWorldReq: {
				UsertoWorldRequest_Struct* utwr = (UsertoWorldRequest_Struct*) pack->pBuffer;
				uint32 id = database.GetAccountIDFromLSID(utwr->lsaccountid);
				int16 status = database.CheckStatus(id);
				bool mule = false;
				uint16 expansion = 0;
				database.GetAccountRestriction(id, expansion, mule);

				auto outpack = new ServerPacket;
				outpack->opcode = ServerOP_UsertoWorldResp;
				outpack->size = sizeof(UsertoWorldResponse_Struct);
				outpack->pBuffer = new uchar[outpack->size];
				memset(outpack->pBuffer, 0, outpack->size);
				UsertoWorldResponse_Struct* utwrs = (UsertoWorldResponse_Struct*) outpack->pBuffer;
				utwrs->lsaccountid = utwr->lsaccountid;
				utwrs->ToID = utwr->FromID;

				if(Config->Locked == true)
				{
					if(status < 80 && status > -1)
						utwrs->response = 0;
					if(status >= 80)
						utwrs->response = 1;
				}
				else {
					utwrs->response = 1;
				}

				int32 x = Config->MaxClients;
				if( (int32)numplayers >= x && x != -1 && x != 255 && status < 80)
					utwrs->response = -3;

				if(status == -1)
					utwrs->response = -1;
				if(status == -2)
					utwrs->response = -2;

				if (utwrs->response == 1)
				{
					// active account checks
					if (RuleI(World, AccountSessionLimit) >= 0 && status < (RuleI(World, ExemptAccountLimitStatus)) && (RuleI(World, ExemptAccountLimitStatus) != -1) && client_list.CheckAccountActive(id))
						utwrs->response = -4;
				}
				if (utwrs->response == 1)
				{
					// ip limit checks
					if (!mule && RuleI(World, MaxClientsPerIP) >= 0 && !client_list.CheckIPLimit(id, utwr->ip, status))
						utwrs->response = -5;
				}
				
				if (utwrs->response == 1)
				{
					// ip limit checks
					if (mule && RuleI(World, MaxMulesPerIP) >= 0 && !client_list.CheckMuleLimit(id, utwr->ip, status))
						utwrs->response = -5;
				}

				ipMutex.lock();
				ipWhitelist.insert(utwr->ip);
				ipMutex.unlock();
				

				utwrs->worldid = utwr->worldid;
				SendPacket(outpack);
				delete outpack;
				break;
			}
			case ServerOP_LSClientAuth: {
				ServerLSClientAuth* slsca = (ServerLSClientAuth*) pack->pBuffer;

				client_list.CLEAdd(slsca->lsaccount_id, slsca->name, slsca->key, slsca->worldadmin, slsca->ip, slsca->local, slsca->version);
				break;
			}
			case ServerOP_LSFatalError: {
	#ifndef IGNORE_LS_FATAL_ERROR
				WorldConfig::DisableLoginserver();
				Log(Logs::Detail, Logs::WorldServer, "Login server responded with FatalError. Disabling reconnect.");
	#else
			Log(Logs::Detail, Logs::WorldServer, "Login server responded with FatalError.");
	#endif
				if (pack->size > 1) {
					Log(Logs::Detail, Logs::WorldServer, "     %s",pack->pBuffer);
				}
				database.LSDisconnect();
				break;
			}
			case ServerOP_SystemwideMessage: {
				ServerSystemwideMessage* swm = (ServerSystemwideMessage*) pack->pBuffer;
				zoneserver_list.SendEmoteMessageRaw(0, 0, AccountStatus::Player, swm->type, swm->message);
				break;
			}
			case ServerOP_LSRemoteAddr: {
				if (!Config->WorldAddress.length()) {
					WorldConfig::SetWorldAddress((char *)pack->pBuffer);
					Log(Logs::Detail, Logs::WorldServer, "Loginserver provided %s as world address",pack->pBuffer);
				}
				break;
			}
			case ServerOP_LSAccountUpdate: {
				Log(Logs::Detail, Logs::WorldServer, "Received ServerOP_LSAccountUpdate packet from loginserver");
				CanAccountUpdate = true;
				break;
			}
			default:
			{
				Log(Logs::Detail, Logs::WorldServer, "Unknown LSOpCode: 0x%04x size=%d",(int)pack->opcode,pack->size);
				DumpPacket(pack->pBuffer, pack->size);
				database.LSDisconnect();
				break;
			}
		}
		delete pack;
	}

	return true;
}

bool LoginServer::InitLoginServer() {
	if(Connected() == false) {
		if(ConnectReady()) {
			LogInfo("Connecting to login server: [{0}:{1}]",LoginServerAddress,LoginServerPort);
			Connect();
		} else {
			LogInfo("Not connected but not ready to connect, this is bad: [{0}:{1}]",
			LoginServerAddress, LoginServerPort);
			database.LSDisconnect();
		}
	}
	return true;
}

bool LoginServer::Connect() {

	char errbuf[TCPConnection_ErrorBufferSize];
	if ((LoginServerIP = ResolveIP(LoginServerAddress, errbuf)) == 0) {
		LogInfo("Unable to resolve [{}] to an IP", LoginServerAddress);
		database.LSDisconnect();
		return false;
	}

	if (LoginServerIP == 0 || LoginServerPort == 0) {
		LogInfo("Connect info incomplete, cannot connect: [{0}:{1}]",LoginServerAddress,LoginServerPort);
		database.LSDisconnect();
		return false;
	}

	if (tcpc->ConnectIP(LoginServerIP, LoginServerPort, errbuf)) {
		LogInfo("Connected to Loginserver: [{0}:{1}]",LoginServerAddress,LoginServerPort);
		database.LSConnected(LoginServerPort);
		SendNewInfo();
		SendStatus();
		zoneserver_list.SendLSZones();
		return true;
	}
	else {
		LogInfo("Could not connect to login server: [{0}]:[{1}] {2}",LoginServerAddress,LoginServerPort,errbuf);
		database.LSDisconnect();
		return false;
	}
}
void LoginServer::SendInfo() {
	const WorldConfig *Config=WorldConfig::get();

	auto pack = new ServerPacket;
	pack->opcode = ServerOP_LSInfo;
	pack->size = sizeof(ServerLSInfo_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerLSInfo_Struct* lsi = (ServerLSInfo_Struct*) pack->pBuffer;
	strcpy(lsi->protocolversion, EQEMU_PROTOCOL_VERSION);
	strcpy(lsi->serverversion, LOGIN_VERSION);
	strcpy(lsi->name, Config->LongName.c_str());
	strcpy(lsi->account, LoginAccount);
	strcpy(lsi->password, LoginPassword);
	strcpy(lsi->address, Config->WorldAddress.c_str());
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendNewInfo() {
	uint16 port;
	const WorldConfig *Config=WorldConfig::get();

	auto pack = new ServerPacket;
	pack->opcode = ServerOP_NewLSInfo;
	pack->size = sizeof(ServerNewLSInfo_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerNewLSInfo_Struct* lsi = (ServerNewLSInfo_Struct*) pack->pBuffer;
	strcpy(lsi->protocolversion, EQEMU_PROTOCOL_VERSION);
	strcpy(lsi->serverversion, LOGIN_VERSION);
	strcpy(lsi->name, Config->LongName.c_str());
	strcpy(lsi->shortname, Config->ShortName.c_str());
	strcpy(lsi->account, LoginAccount);
	strcpy(lsi->password, LoginPassword);
	if (Config->WorldAddress.length())
			strcpy(lsi->remote_address, Config->WorldAddress.c_str());
	if (Config->LocalAddress.length())
			strcpy(lsi->local_address, Config->LocalAddress.c_str());
	else {
			tcpc->GetSockName(lsi->local_address,&port);
			WorldConfig::SetLocalAddress(lsi->local_address);
	}
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendStatus() {
	statusupdate_timer.Start();
	auto pack = new ServerPacket;
	pack->opcode = ServerOP_LSStatus;
	pack->size = sizeof(ServerLSStatus_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerLSStatus_Struct* lss = (ServerLSStatus_Struct*) pack->pBuffer;

	if (WorldConfig::get()->Locked)
		lss->status = -2;
	else if (numzones <= 0)
		lss->status = -1;
	else
		lss->status = numplayers > 0 ? numplayers : 0;

	lss->num_zones = numzones;
	lss->num_players = numplayers;
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendAccountUpdate(ServerPacket* pack) {
	ServerLSAccountUpdate_Struct* s = (ServerLSAccountUpdate_Struct *) pack->pBuffer;
	if(CanUpdate()) {
		Log(Logs::Detail, Logs::WorldServer, "Sending ServerOP_LSAccountUpdate packet to loginserver: %s:%d",LoginServerAddress,LoginServerPort);
		strn0cpy(s->worldaccount, LoginAccount, 30);
		strn0cpy(s->worldpassword, LoginPassword, 30);
		SendPacket(pack);
	}
}

