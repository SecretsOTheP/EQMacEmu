#ifndef CLIENTLIST_H_
#define CLIENTLIST_H_

#include "../common/eq_packet_structs.h"
#include "../common/linked_list.h"
#include "../common/json/json.h"
#include "../common/timer.h"
#include "../common/rulesys.h"
#include "../common/servertalk.h"
#include "../common/event/timer.h"
#include "../common/net/console_server_connection.h"
#include "world_queue.h"  // For server population management
#include <vector>
#include <string>
#include <mutex>

class Client;
class ZoneServer;
class WorldTCPConnection;
class ClientListEntry;
class ServerPacket;
struct ServerClientList_Struct;

class ClientList {
public:
	ClientList();
	~ClientList();

	void Process();

	//from old ClientList
	void	Add(Client* client);
	Client*	Get(uint32 ip, uint16 port);
	Client* FindByAccountID(uint32 account_id);
	Client* FindByName(char* charname);

	void	ZoneBootup(ZoneServer* zs);
	void	RemoveCLEReferances(ClientListEntry* cle);


	//from ZSList

	void	SendWhoAll(uint32 fromid,const char* to, int16 admin, Who_All_Struct* whom);
	void	SendFriendsWho(ServerFriendsWho_Struct *FriendsWho, WorldTCPConnection* connection);
	void	SendOnlineGuildMembers(uint32 FromID, uint32 GuildID);
	void	SendClientVersionSummary(const char *Name);
	void	ConsoleClientVersionSummary(const char* to, WorldTCPConnection* connection);
	void	ConsoleSendWhoAll(const char* to, int16 admin, Who_All_Struct* whom, WorldTCPConnection* connection);
	void	SendCLEList(const int16& admin, const char* to, WorldTCPConnection* connection, const char* iName = 0);
	void	ConsoleTraderCount(const char* to, WorldTCPConnection* connection);

	bool	SendPacket(const char* to, ServerPacket* pack);
	void	SendGuildPacket(uint32 guild_id, ServerPacket* pack);

	void	ClientUpdate(ZoneServer* zoneserver, ServerClientList_Struct* scl);
	void	CLERemoveZSRef(ZoneServer* iZS);
	ClientListEntry* CheckAuth(uint32 iLSID, const char* iKey);
	ClientListEntry* CheckAuth(const char* iName, const char* iPassword);
	ClientListEntry* CheckAuth(uint32 id, const char* iKey, uint32 ip);
	ClientListEntry* FindCharacter(const char* name);
	ClientListEntry* FindCLEByAccountID(uint32 iAccID);
	ClientListEntry* FindCLEByCharacterID(uint32 iCharID);
	ClientListEntry* GetCLE(uint32 iID);
	void	ClearGroup(uint32 group_id);
	ClientListEntry* RemoveCLEByAccountID(uint32 accountID);
	bool	CheckIPLimit(uint32 iAccID, uint32 iIP, uint16 admin, ClientListEntry* cle = nullptr);
	bool	CheckMuleLimit(uint32 iAccID, uint32 iIP, uint16 admin, ClientListEntry * cle = nullptr);
	bool	CheckAccountActive(uint32 iAccID, ClientListEntry* cle = nullptr);
	void	GetCLEIP(uint32 iIP);
	void	DisconnectByIP(uint32 iIP);
	bool	EnforceSessionLimit(uint32 iLSAccountID);
	void	CLCheckStale();
	void	CLEKeepAlive(uint32 numupdates, uint32* wid);
	void	CLEAdd(uint32 iLSID, const char* iLoginName, const char* iForumName, const char* iLoginKey, int16 iWorldAdmin = 0, uint32 ip = 0, uint8 local=0, uint8 version=0);
	void	UpdateClientGuild(uint32 char_id, uint32 guild_id);
	bool	ActiveConnection(uint32 iAccID);
	bool	ActiveConnection(uint32 iAccID, uint32 iCharID);
	bool    IsAccountInGame(uint32 iLSID);
	std::string GetClientKeyByLSID(uint32 iLSID);  // Get client key for queue authorization matching

	int GetClientCount(); // Fallback population counter for startup
	
	void GetClients(const char *zone_name, std::vector<ClientListEntry *> &into);
	bool WhoAllFilter(ClientListEntry* client, Who_All_Struct* whom, int16 admin, int whomlen);

	void GetClientList(Json::Value &response);

	std::string AppendChallengeModeFlagsToName(ClientListEntry* cle);
	
private:
	void OnTick(EQ::Timer* t);
	inline uint32 GetNextCLEID() { return NextCLEID++; }

	//this is the list of people actively connected to zone
	LinkedList<Client*> list;

	//this is the list of people in any zone, not nescesarily connected to world
	Timer	CLStale_timer;
	uint32 NextCLEID;
	LinkedList<ClientListEntry *> clientlist;

	std::unique_ptr<EQ::Timer> m_tick;
};

#endif /*CLIENTLIST_H_*/

