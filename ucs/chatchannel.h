#ifndef CHATCHANNEL_H
#define CHATCHANNEL_H

//#include "clientlist.h"
#include "../common/linked_list.h"
#include "../common/timer.h"
#include <string>
#include <vector>

class Client;

class ChatChannel {

public:

	ChatChannel(std::string inName, std::string inOwner, std::string inPassword, bool inPermanent, int inMinimumStatus = 0);
	~ChatChannel();

	void AddClient(Client *c);
	bool RemoveClient(Client *c);
	bool IsClientInChannel(Client *c);

	int MemberCount(int Status);
	const std::string &GetName() { return m_name; }
	void SendMessageToChannel(const std::string &Message, Client* Sender);
	bool CheckPassword(const std::string &in_password) { return m_password.empty() || m_password == in_password; }
	void SetPassword(const std::string &in_password);
	bool IsOwner(const std::string &name) { return (m_owner == name); }
	const std::string &GetPassword() { return m_password; }
	void SetOwner(std::string& inOwner);
	void SendChannelMembers(Client *c);
	int GetMinStatus() { return m_minimum_status; }
	bool ReadyToDelete() { return m_delete_timer.Check(); }
	void SendOPList(Client *c);
	void AddInvitee(const std::string &Invitee);
	void RemoveInvitee(std::string Invitee);
	bool IsInvitee(std::string Invitee);
	void AddModerator(const std::string &Moderator);
	void RemoveModerator(const std::string &Moderator);
	bool IsModerator(std::string Moderator);
	void AddVoice(const std::string &Voiced);
	void RemoveVoice(const std::string &Voiced);
	bool HasVoice(std::string Voiced);
	inline bool IsModerated() { return m_moderated; }
	void SetModerated(bool inModerated);

	friend class ChatChannelList;

private:

	std::string m_name;
	std::string m_owner;
	std::string m_password;

	bool m_permanent;
	bool m_moderated;

	int m_minimum_status;

	Timer m_delete_timer;

	LinkedList<Client*> m_clients_in_channel;

	std::vector<std::string> m_moderators;
	std::vector<std::string> m_invitees;
	std::vector<std::string> m_voiced;

};

class ChatChannelList {

public:
	ChatChannel* CreateChannel(const std::string& name, const std::string& owner, const std::string& password, bool permanent, int minimum_status);
	ChatChannel* FindChannel(std::string name);
	ChatChannel* AddClientToChannel(std::string channel_name, Client* c, bool& should_join_lfg);
	ChatChannel* RemoveClientFromChannel(const std::string& in_channel_name, Client* c);
	void RemoveChannel(ChatChannel *Channel);
	void RemoveAllChannels();
	void SendAllChannels(Client *c);
	void Process();
	void ChatChannelDiscordRelay(ChatChannel *channel, Client *client, const char *message);

private:

	LinkedList<ChatChannel*> ChatChannels;

};

std::string CapitaliseName(std::string inString);

#endif
