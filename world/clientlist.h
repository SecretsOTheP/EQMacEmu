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
#include "world_queue.h"
#include <vector>
#include <string>

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
	bool	CheckIPLimit(uint32 iAccID, uint32 iIP, const char* ForumName, uint16 admin, ClientListEntry* cle = nullptr);
	void	ClearGroup(uint32 group_id);
	bool	CheckAccountActive(uint32 iAccID, ClientListEntry* cle = nullptr);
	void	CLCheckStale();
	void	CLEKeepAlive(uint32 numupdates, uint32* wid);
	void	CLEAdd(uint32 iLSID, const char* iLoginName, const char* iForumName, const char* iLoginKey, int16 iWorldAdmin = 0, uint32 ip = 0, uint8 local=0, uint8 version=0, int16 exemptioncount = 1, bool queue_active = false);
	void	UpdateClientGuild(uint32 char_id, uint32 guild_id);
	bool ActiveConnectionIncludingStale(uint32 account_id);
	bool ActiveConnectionKickStale(uint32 account_id);
	bool	ActiveConnection(uint32 iAccID, uint32 iCharID);
	bool    IsAccountInGame(uint32 iLSID);


	int GetClientCount();
	void GetClients(const char *zone_name, std::vector<ClientListEntry *> &into);
	bool WhoAllFilter(ClientListEntry* client, Who_All_Struct* whom, int16 admin, int whomlen);

	void GetClientList(Json::Value &response);

	std::string AppendChallengeModeFlagsToName(ClientListEntry* cle);

	// Login queue (Quarm:EnableLoginQueue). Population is counted per account: in a zone, holding a
	// reservation, or in grace. Status > 0 and offline traders never count, as with GetClientCount().
	WorldQueue&		Queue() { return m_queue; }
	bool			QueueActive(); // rule is on and at least one login server can display queue info
	QueuePopulation	Population();  // accounts in zone and accounts at character select, from the entry list
	uint32			EffectivePopulation(); // Queue().EffectivePopulation() against the current population
	QueueDecision	QueueDecide(uint32 iLSID, uint32 iAccID, uint32 ip); // answer a Play request from the login server
	bool			QueueHoldsSlot(uint32 iAccID);                      // used by CLEAdd when the world is full
	bool			QueueClaimSlot(uint32 iLSID, uint32 iAccID, uint32 ip); // Enter World: take a slot or queue the account
	void			QueueTick(); // expire and consume; every 5 s from OnTick
	void			OnLeftZone(ClientListEntry* cle);   // entry dropped below Zoning: start grace if it was the account's last
	void			OnZoneUpdate(ClientListEntry* cle); // zone reported the entry: track the linkdead flag
	void			SendQueueStatus(const char* to, WorldTCPConnection* connection); // console "queue" and #show queue

private:
	void RefreshQueueConfig(); // copy the current rule values into the queue
	static uint32 QueueNow();  // unix seconds, the queue's clock

	WorldQueue m_queue;

	void OnTick(EQ::Timer* t);
	inline uint32 GetNextCLEID() { return NextCLEID++; }

	//this is the list of people actively connected to zone
	LinkedList<Client*> list;

	//this is the list of people in any zone, not nescesarily connected to world
	Timer	CLStale_timer;
	uint32 NextCLEID;
	LinkedList<ClientListEntry *> clientlist;
	uint32 cached_gm_count;
	uint32 cached_trader_count;

	std::unique_ptr<EQ::Timer> m_tick;
};

#endif /*CLIENTLIST_H_*/

