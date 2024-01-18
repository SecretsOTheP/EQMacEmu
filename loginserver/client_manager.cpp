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
#include "client_manager.h"
#include "login_server.h"

extern LoginServer server;
extern bool run_server;

#include "../common/eqemu_logsys.h"
extern EQEmuLogSys Log;

ClientManager::ClientManager()
{
	int old_port = server.config.GetVariableInt("Old", "port", 6000);
	old_stream = new EQStreamFactory(OldStream, old_port);
	old_ops = new RegularOpcodeManager;
	if (!old_ops->LoadOpcodes(server.config.GetVariableString("Old", "opcodes", "login_opcodes_oldver.conf").c_str()))
	{
		LogError("ClientManager fatal error: couldn't load opcodes for Old file %s.",
			server.config.GetVariableString("Old", "opcodes", "login_opcodes_oldver.conf").c_str());
		run_server = false;
	}
	else if (old_stream->Open())
	{
		LogInfo("ClientManager listening on Old stream.");
	}
	else
	{
		LogError("ClientManager fatal error: couldn't open Old stream.");
		run_server = false;
	}
}

ClientManager::~ClientManager()
{
	if (old_stream)
	{
		old_stream->Close();
		delete old_stream;
	}

	if (old_ops)
	{
		delete old_ops;
	}
}

void ClientManager::Process()
{
	ProcessDisconnect();
	if (old_stream) {
		std::shared_ptr<EQOldStream> oldcur = old_stream->PopOld();
		while (oldcur)
		{
			struct in_addr in;
			in.s_addr = oldcur->GetRemoteIP();
			LogInfo("New client connection from {0}:{1}", inet_ntoa(in), ntohs(oldcur->GetRemotePort()));

			oldcur->SetOpcodeManager(&old_ops);
			Client* c = new Client(oldcur, cv_old);
			clients.push_back(c);
			oldcur = old_stream->PopOld();
		}
	}

	list<Client*>::iterator iter = clients.begin();
	while (iter != clients.end())
	{
		if ((*iter)->Process() == false)
		{
			Log(Logs::General, Logs::LoginServer, "Client had a fatal error and had to be removed from the login.");
			delete (*iter);
			iter = clients.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void ClientManager::ProcessDisconnect()
{
	list<Client*>::iterator iter = clients.begin();
	while(iter != clients.end())
	{
		std::shared_ptr<EQStreamInterface> c = (*iter)->GetConnection();
		if (c->CheckState(CLOSED))
		{
			c->ReleaseFromUse();
			LogInfo("Client disconnected from the server, removing client.");
			delete (*iter);
			iter = clients.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void ClientManager::RemoveExistingClient(unsigned int account_id)
{
	list<Client*>::iterator iter = clients.begin();
	while(iter != clients.end())
	{
		if((*iter)->GetAccountID() == account_id)
		{
			Log(Logs::General, Logs::LoginServer, "Client attempting to log in and existing client already logged in, removing existing client.");
			delete (*iter);
			iter = clients.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void ClientManager::UpdateServerList()
{
	list<Client*>::iterator iter = clients.begin();
	while (iter != clients.end())
	{
		(*iter)->SendServerListPacket();
		++iter;
	}
}

Client *ClientManager::GetClient(unsigned int account_id)
{
	Client *cur = nullptr;
	int count = 0;
	list<Client*>::iterator iter = clients.begin();
	while(iter != clients.end())
	{
		if((*iter)->GetAccountID() == account_id)
		{
			cur = (*iter);
			count++;
		}
		++iter;
	}

	if(count > 1)
	{
		Log(Logs::General, Logs::Error, "More than one client with a given account_id existed in the client list.");
	}
	return cur;
}

