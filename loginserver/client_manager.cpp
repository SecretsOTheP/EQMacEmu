#include "client_manager.h"
#include "login_server.h"

extern LoginServer server;
extern bool run_server;

#include "../common/eqemu_logsys.h"
#include "../common/misc.h"
#include "../common/path_manager.h"
#include "../common/file.h"

void CheckOldOpcodeFile(const std::string& path) {
	if (File::Exists(path)) {
		return;
	}
	auto f = fopen(path.c_str(), "w");
	if (f) {
		fprintf(f, "#EQEmu Public Login Server OPCodes\n");
		fprintf(f, "OP_SessionReady=0x5900\n");
		fprintf(f, "OP_LoginOSX=0x8e00\n");
		fprintf(f, "OP_LoginPC=0x0100\n");
		fprintf(f, "OP_ClientError=0x0200\n");
		fprintf(f, "OP_LoginDisconnect=0x0500\n");
		fprintf(f, "OP_ServerListRequest=0x4600\n");
		fprintf(f, "OP_PlayEverquestRequest=0x4700\n");
		fprintf(f, "OP_LoginUnknown1=0x4800\n");
		fprintf(f, "OP_LoginUnknown2=0x4A00\n");
		fprintf(f, "OP_LoginAccepted=0x0400\n");
		fprintf(f, "OP_LoginComplete=0x8800\n");
		fprintf(f, "OP_ServerName=0x4900\n");
		fprintf(f, "OP_LoginBanner=0x5200\n");
		fclose(f);
	}
}

ClientManager::ClientManager()
{
	int old_port = server.config.GetVariableInt("Old", "port", 6000);
	old_stream = new EQStreamFactory(OldStream, old_port);
	old_ops = new RegularOpcodeManager;

	std::string opcodes_path = fmt::format(
		"{}/{}",
		path.GetOpcodePath(),
		"login_opcodes_oldver.conf"
	);

	CheckOldOpcodeFile(opcodes_path);

	if (!old_ops->LoadOpcodes(opcodes_path.c_str())) {
		LogError("ClientManager fatal error: couldn't load opcodes for Old file [{}].",
			server.config.GetVariableString("Old", "opcodes", "login_opcodes_oldver.conf"));
		run_server = false;
	}
	else if (old_stream->Open()) {
		LogInfo("ClientManager listening on Old stream.");
	}
	else {
		LogError("ClientManager fatal error: couldn't open Old stream.");
		run_server = false;
	}
}

ClientManager::~ClientManager()
{
	if (old_stream)	{
		old_stream->Close();
		delete old_stream;
	}

	if (old_ops) {
		delete old_ops;
	}
}

void ClientManager::Process()
{
	ProcessDisconnect();
	if (old_stream) {
		std::shared_ptr<EQStreamInterface> oldcur = old_stream->PopOld();
		while (oldcur) {
			struct in_addr in;
			in.s_addr = oldcur->GetRemoteIP();
			LogInfo("New client connection from {0}:{1}", inet_ntoa(in), ntohs(oldcur->GetRemotePort()));

			oldcur->SetOpcodeManager(&old_ops);
			Client* c = new Client(oldcur, cv_old);
			clients.push_back(c);
			oldcur = old_stream->PopOld();
		}
	}

	auto iter = clients.begin();
	while (iter != clients.end()) {
		if ((*iter)->Process() == false) {
			Log(Logs::General, Logs::LoginServer, "Client had a fatal error and had to be removed from the login.");
			delete (*iter);
			iter = clients.erase(iter);
		}
		else {
			++iter;
		}
	}
}

void ClientManager::ProcessDisconnect()
{
	auto iter = clients.begin();
	while (iter != clients.end()) {
		std::shared_ptr<EQStreamInterface> c = (*iter)->GetConnection();
		if (c->CheckState(CLOSED)) {
			c->ReleaseFromUse();
			// Get client account ID before deletion for queue removal
			uint32 account_id = (*iter)->GetAccountID();
			
			LogInfo("Client disconnected from the server, removing client.");
			
			// Remove from queue on all world servers if account ID is valid
			if ((*iter)->HasQueuePosition() && (*iter)->GetAccountID() > 0 && server.server_manager) {
				server.server_manager->RemovePlayerFromAllQueues((*iter)->GetAccountID());
			}
			
			delete (*iter);
			iter = clients.erase(iter);
		}
		else {
			++iter;
		}
	}
}

void ClientManager::RemoveExistingClient(unsigned int account_id)
{
	auto iter = clients.begin();
	while (iter != clients.end()){
		if ((*iter)->GetAccountID() == account_id) {
			Log(Logs::General, Logs::LoginServer, "Client attempting to log in and existing client already logged in, removing existing client.");
			delete (*iter);
			iter = clients.erase(iter);
		}
		else{
			++iter;
		}
	}
}

void ClientManager::UpdateServerList()
{
	auto iter = clients.begin();
	while (iter != clients.end()) {
		(*iter)->SendServerListPacket();
		++iter;
	}
}

Client *ClientManager::GetClient(unsigned int account_id)
{
	Client *cur = nullptr;
	int count = 0;
	auto iter = clients.begin();
	while(iter != clients.end()) {
		if((*iter)->GetAccountID() == account_id) {
			cur = (*iter);
			count++;
		}
		++iter;
	}

	if(count > 1) {
		Log(Logs::General, Logs::Error, "More than one client with a given account_id existed in the client list.");
	}
	return cur;
}

Client *ClientManager::GetClientByKey(const std::string& key)
{
	for (Client* client : clients) {
		if (client && client->GetKey() == key) {
			return client;
		}
	}
	return nullptr;
}
