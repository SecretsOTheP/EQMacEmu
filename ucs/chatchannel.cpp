/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2008 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/eqemu_logsys.h"
#include "../common/strings.h"
#include "../common/discord/discord_manager.h"
#include "../common/servertalk.h"
#include "../common/races.h"
#include "chatchannel.h"
#include "clientlist.h"
#include "database.h"
#include "worldserver.h"
#include <cstdlib>
#include <algorithm>

extern UCSDatabase database;
extern uint32 ChatMessagesSent;
extern DiscordManager discord_manager;
extern WorldServerList *worldserverlist;

ChatChannel::ChatChannel(std::string inName, std::string inOwner, std::string inPassword, bool inPermanent, int inMinimumStatus)
	:
	m_delete_timer(0) {

	m_name = inName;

	m_owner = inOwner;

	m_password = inPassword;

	m_permanent = inPermanent;

	m_minimum_status = inMinimumStatus;

	m_moderated = false;

	LogDebug(
		"New ChatChannel created: Name: [{}] Owner: [{}] Password: [{}] MinStatus: [{}]",
		m_name.c_str(),
		m_owner.c_str(),
		m_password.c_str(),
		m_minimum_status
	);

}

ChatChannel::~ChatChannel() {

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements())
		iterator.RemoveCurrent(false);
}

ChatChannel *ChatChannelList::CreateChannel(
	const std::string &name,
	const std::string &owner,
	const std::string &password,
	bool permanent,
	int minimum_status
)
{

	auto *new_channel = new ChatChannel(CapitaliseName(name), owner, password, permanent, minimum_status);

	ChatChannels.Insert(new_channel);

	return new_channel;
}

ChatChannel *ChatChannelList::FindChannel(std::string Name) {

	std::string normalized_name = CapitaliseName(Name);

	LinkedListIterator<ChatChannel *> iterator(ChatChannels);

	iterator.Reset();

	while (iterator.MoreElements()) {

		auto *current_channel = iterator.GetData();

		if (current_channel && (current_channel->m_name == normalized_name))
			return iterator.GetData();

		iterator.Advance();
	}

	return nullptr;
}

void ChatChannelList::SendAllChannels(Client *c) {

	if (!c) {
		return;
	}

	if (!c->CanListAllChannels()) {
		c->GeneralChannelMessage("You do not have permission to list all the channels.");
		return;
	}

	c->GeneralChannelMessage("All current channels:");

	int ChannelsInLine = 0;

	LinkedListIterator<ChatChannel *> iterator(ChatChannels);

	iterator.Reset();

	std::string Message;

	char CountString[10];

	while (iterator.MoreElements()) {

		ChatChannel *CurrentChannel = iterator.GetData();

		if (!CurrentChannel || (CurrentChannel->GetMinStatus() > c->GetAccountStatus())) {

			iterator.Advance();

			continue;
		}

		if (ChannelsInLine > 0) {
			Message += ", ";
		}

		sprintf(CountString, "(%i)", CurrentChannel->MemberCount(c->GetAccountStatus()));

		Message += CurrentChannel->GetName();

		Message += CountString;

		ChannelsInLine++;

		if (ChannelsInLine == 6) {

			c->GeneralChannelMessage(Message);

			ChannelsInLine = 0;

			Message.clear();
		}

		iterator.Advance();
	}

	if (ChannelsInLine > 0) {
		c->GeneralChannelMessage(Message);
	}

}

void ChatChannelList::RemoveChannel(ChatChannel *Channel) {

	LogDebug("Remove channel [{}]", Channel->GetName().c_str());

	LinkedListIterator<ChatChannel *> iterator(ChatChannels);

	iterator.Reset();

	while (iterator.MoreElements()) {

		if (iterator.GetData() == Channel) {

			iterator.RemoveCurrent();

			return;
		}

		iterator.Advance();
	}
}

void ChatChannelList::RemoveAllChannels() {

	LogDebug("RemoveAllChannels");

	LinkedListIterator<ChatChannel *> iterator(ChatChannels);

	iterator.Reset();

	while (iterator.MoreElements()) {
		iterator.RemoveCurrent();
	}
}

int ChatChannel::MemberCount(int Status) {

	int Count = 0;

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		Client *ChannelClient = iterator.GetData();

		if (ChannelClient && (!ChannelClient->GetHideMe() || (ChannelClient->GetAccountStatus() < Status)))
			Count++;

		iterator.Advance();
	}

	return Count;
}

void ChatChannel::SetPassword(const std::string &in_password) {

	m_password = in_password;

	if (m_permanent) {
		RemoveApostrophes(m_password);
		database.SetChannelPassword(m_name, m_password);
	}
}

void ChatChannel::SetOwner(std::string &in_owner) {
	m_owner = in_owner;

	if (m_permanent) {
		database.SetChannelOwner(m_name, m_owner);
	}
}

void ChatChannel::AddClient(Client *c) {

	if (!c) {
		return;
	}

	m_delete_timer.Disable();

	if (IsClientInChannel(c)) {

		LogInfo("Client [{0}] already in channel [{1}]", c->GetFQName().c_str(), GetName().c_str());

		return;
	}

	bool HideMe = c->GetHideMe();

	int AccountStatus = c->GetAccountStatus();

	LogDebug("Adding [{0}] to channel [{1}]", c->GetFQName().c_str(), m_name.c_str());

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		Client *CurrentClient = iterator.GetData();

		if (CurrentClient && CurrentClient->IsAnnounceOn()) {
			if (!HideMe || (CurrentClient->GetAccountStatus() > AccountStatus)) {
				CurrentClient->AnnounceJoin(this, c);
			}
		}

		iterator.Advance();
	}

	m_clients_in_channel.Insert(c);

}

bool ChatChannel::RemoveClient(Client *c) {

	if (!c) return false;

	LogDebug("Remove client [{0}] from channel [{1}]", c->GetFQName().c_str(), GetName().c_str());

	bool hide_me = c->GetHideMe();

	int account_status = c->GetAccountStatus();

	int players_in_channel = 0;

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		auto *current_client = iterator.GetData();

		if (current_client == c) {
			iterator.RemoveCurrent(false);
		}
		else if (current_client) {

			players_in_channel++;

			if (current_client->IsAnnounceOn())
				if (!hide_me || (current_client->GetAccountStatus() > account_status))
					current_client->AnnounceLeave(this, c);

			iterator.Advance();
		}

	}

	if ((players_in_channel == 0) && !m_permanent) {

		if ((m_password.length() == 0) || (RuleI(Channels, DeleteTimer) == 0)) {
			return false;
		}

		LogDebug("Starting delete timer for empty password protected channel [{0}]", m_name.c_str());

		m_delete_timer.Start(RuleI(Channels, DeleteTimer) * 60000);
	}

	return true;
}

void ChatChannel::SendOPList(Client *c)
{
	if (!c) {
		return;
	}

	c->GeneralChannelMessage("Channel " + m_name + " op-list: (Owner=" + m_owner + ")");

	for (auto &&m : m_moderators) {
		c->GeneralChannelMessage(m);
	}
}

void ChatChannel::SendChannelMembers(Client *c) {

	if (!c) {
		return;
	}

	char CountString[10];

	sprintf(CountString, "(%i)", MemberCount(c->GetAccountStatus()));

	std::string Message = "Channel " + GetName();

	Message += CountString;

	Message += " members:";

	c->GeneralChannelMessage(Message);

	int AccountStatus = c->GetAccountStatus();

	Message.clear();

	int MembersInLine = 0;

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		Client *ChannelClient = iterator.GetData();

		// Don't list hidden characters with status higher or equal than the character requesting the list.
		//
		if (!ChannelClient || (ChannelClient->GetHideMe() && (ChannelClient->GetAccountStatus() >= AccountStatus))) {
			iterator.Advance();
			continue;
		}

		if (MembersInLine > 0)
			Message += ", ";

		if (ChannelClient->GetWorldShortName().compare(c->GetWorldShortName()))
			Message += ChannelClient->GetFQName();
		else
			Message += ChannelClient->GetName();

		MembersInLine++;

		if (MembersInLine == 6) {

			c->GeneralChannelMessage(Message);

			MembersInLine = 0;

			Message.clear();
		}

		iterator.Advance();
	}

	if (MembersInLine > 0) {
		c->GeneralChannelMessage(Message);
	}

}

void ChatChannel::SendMessageToChannel(const std::string &Message, Client *Sender) {

	if (!Sender) {
		return;
	}

	if (Sender->GetLevel() < RuleI(Quarm, AllianceChannelLevelRequirement) && CapitaliseName(GetName()).compare(RuleS(Quarm, AllianceChannelReplacementName)) == 0)
	{
		Sender->GeneralChannelMessage("Channel " + GetName() + " requires level " + std::to_string(RuleI(Quarm, AllianceChannelLevelRequirement)) + " to participate in chat.");
		return;
	}
	ChatMessagesSent++;

	std::string MessageToSend = Message;

	Strings::SanitizeChatString(MessageToSend);
	
	if(MessageToSend.length() && Sender->GetName().length() && GetName().length())
		database.LogUCSPlayerSpeech(Sender->GetName().c_str(), GetName().c_str(), MessageToSend.c_str(), Sender->GetAccountStatus(), 0, 200, Sender->GetCharID(), 0);
	
	LinkedListIterator<Client*> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		auto *channel_client = iterator.GetData();

		if (channel_client) {
			LogDebug("Sending message to [{0}] from [{1}]",
				channel_client->GetFQName().c_str(), Sender->GetFQName().c_str());

			channel_client->SendChannelMessage(m_name, MessageToSend, Sender);
		}

		iterator.Advance();
	}
}

void ChatChannel::SetModerated(bool inModerated) {

	m_moderated = inModerated;

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		Client *ChannelClient = iterator.GetData();

		if (ChannelClient) {

			if (m_moderated) {
				ChannelClient->GeneralChannelMessage("Channel " + m_name + " is now moderated.");
			}
			else {
				ChannelClient->GeneralChannelMessage("Channel " + m_name + " is no longer moderated.");
			}
		}

		iterator.Advance();
	}

}

bool ChatChannel::IsClientInChannel(Client *c) {

	if (!c) {
		return false;
	}

	LinkedListIterator<Client *> iterator(m_clients_in_channel);

	iterator.Reset();

	while (iterator.MoreElements()) {

		if (iterator.GetData() == c) {
			return true;
		}

		iterator.Advance();
	}

	return false;
}

ChatChannel *ChatChannelList::AddClientToChannel(std::string channel_name, Client *c) {

	if (!c) {
		return nullptr;
	}

	if ((channel_name.length() > 0) && (isdigit(channel_name[0]))) {

		c->GeneralChannelMessage("The channel name can not begin with a number.");

		return nullptr;
	}

	std::string normalized_name, password;

	std::string::size_type Colon = channel_name.find_first_of(":");

	if (Colon == std::string::npos) {
		normalized_name = CapitaliseName(channel_name);
	}
	else {
		normalized_name = CapitaliseName(channel_name.substr(0, Colon));

		password = channel_name.substr(Colon + 1);
	}

	if (normalized_name.compare(RuleS(Quarm, AllianceChannelName)) == 0 || normalized_name.compare(RuleS(Quarm, AllianceChannelReplacementName)) == 0)
	{
		normalized_name = RuleS(Quarm, AllianceChannelReplacementName);
		password = "";
	}

	if((normalized_name.length() > 64) || (password.length() > 64)) {
		c->GeneralChannelMessage("The channel name or password cannot exceed 64 characters.");
		return nullptr;
	}

	LogDebug("AddClient to channel [[{0}]] with password [[{1}]]", normalized_name.c_str(), password.c_str());

	ChatChannel *RequiredChannel = FindChannel(normalized_name);

	if (!RequiredChannel) {
		RequiredChannel = CreateChannel(normalized_name, c->GetFQName(), password, false, 0);
	}

	if (RequiredChannel->GetMinStatus() > c->GetAccountStatus()) {

		std::string Message = "You do not have the required account status to join channel " + normalized_name;

		c->GeneralChannelMessage(Message);

		return nullptr;
	}

	if (RequiredChannel->IsClientInChannel(c)) {
		return nullptr;
	}

	if (RequiredChannel->IsInvitee(c->GetFQName())) {

		RequiredChannel->AddClient(c);

		RequiredChannel->RemoveInvitee(c->GetFQName());

		return RequiredChannel;
	}

	if (RequiredChannel->CheckPassword(password) || RequiredChannel->IsOwner(c->GetFQName()) || RequiredChannel->IsModerator(c->GetFQName()) ||
		c->IsChannelAdmin()) {

		RequiredChannel->AddClient(c);

		return RequiredChannel;
	}

	c->GeneralChannelMessage("Incorrect password for channel " + (normalized_name));

	return nullptr;
}

ChatChannel *ChatChannelList::RemoveClientFromChannel(const std::string &in_channel_name, Client *c) {

	if (!c) {
		return nullptr;
	}

	std::string channel_name = in_channel_name;

	if ((in_channel_name.length() > 0) && isdigit(channel_name[0])) {
		channel_name = c->ChannelSlotName(atoi(in_channel_name.c_str()));
	}

	auto *required_channel = FindChannel(channel_name);

	if (!required_channel) {
		return nullptr;
	}

	// RemoveClient will return false if there is no-one left in the channel, and the channel is not permanent and has
	// no password.
	//
	if (!required_channel->RemoveClient(c)) {
		RemoveChannel(required_channel);
	}

	return required_channel;
}

void ChatChannelList::Process() {

	LinkedListIterator<ChatChannel *> iterator(ChatChannels);

	iterator.Reset();

	while (iterator.MoreElements()) {

		ChatChannel *CurrentChannel = iterator.GetData();

		if (CurrentChannel && CurrentChannel->ReadyToDelete()) {

			LogDebug("Empty temporary password protected channel [{0}] being destroyed",
				CurrentChannel->GetName().c_str());

			iterator.Advance();
			RemoveChannel(CurrentChannel);
			continue;
		}

		iterator.Advance();

	}
}

void ChatChannel::AddInvitee(const std::string &Invitee)
{
	if (!IsInvitee(Invitee)) {
		m_invitees.push_back(Invitee);

		LogDebug("Added [{0}] as invitee to channel [{1}]", Invitee.c_str(), m_name.c_str());
	}

}

void ChatChannel::RemoveInvitee(std::string Invitee)
{
	auto it = std::find(std::begin(m_invitees), std::end(m_invitees), Invitee);

	if (it != std::end(m_invitees)) {
		m_invitees.erase(it);
		LogDebug("Removed [{0}] as invitee to channel [{1}]", Invitee.c_str(), m_name.c_str());
	}
}

bool ChatChannel::IsInvitee(std::string Invitee)
{
	return std::find(std::begin(m_invitees), std::end(m_invitees), Invitee) != std::end(m_invitees);
}

void ChatChannel::AddModerator(const std::string &Moderator)
{
	if (!IsModerator(Moderator)) {
		m_moderators.push_back(Moderator);

		LogInfo("Added [{0}] as moderator to channel [{1}]", Moderator.c_str(), m_name.c_str());
	}

}

void ChatChannel::RemoveModerator(const std::string &Moderator)
{
	auto it = std::find(std::begin(m_moderators), std::end(m_moderators), Moderator);

	if (it != std::end(m_moderators)) {
		m_moderators.erase(it);
		LogInfo("Removed [{0}] as moderator to channel [{1}]", Moderator.c_str(), m_name.c_str());
	}
}

bool ChatChannel::IsModerator(std::string Moderator)
{
	return std::find(std::begin(m_moderators), std::end(m_moderators), Moderator) != std::end(m_moderators);
}

void ChatChannel::AddVoice(const std::string &inVoiced)
{
	if (!HasVoice(inVoiced)) {
		m_voiced.push_back(inVoiced);

		LogInfo("Added [{0}] as voiced to channel [{1}]", inVoiced.c_str(), m_name.c_str());
	}
}

void ChatChannel::RemoveVoice(const std::string &inVoiced)
{
	auto it = std::find(std::begin(m_voiced), std::end(m_voiced), inVoiced);

	if (it != std::end(m_voiced)) {
		m_voiced.erase(it);

		LogInfo("Removed [{0}] as voiced to channel [{1}]", inVoiced.c_str(), m_name.c_str());
	}
}

bool ChatChannel::HasVoice(std::string inVoiced)
{
	return std::find(std::begin(m_voiced), std::end(m_voiced), inVoiced) != std::end(m_voiced);
}

std::string CapitaliseName(std::string inString) {

	std::string NormalisedName = inString;

	for (unsigned int i = 0; i < NormalisedName.length(); i++) {

		if (i == 0) {
			NormalisedName[i] = toupper(NormalisedName[i]);
		}
		else {
			NormalisedName[i] = tolower(NormalisedName[i]);
		}
	}

	return NormalisedName;
}

int32 EncodeDiscordChatItemLinkMsg(const char *Parms, char *Msg, char **Ret) {
	int32 MsgLen = strlen(Msg);
	bool in_link = false;
	std::string itemid = "";
	int32 item_count = 0;
	std::string New = Parms;
	char Temp[12];
	bool finished_link = false;

	for (int32 i = 0; i < MsgLen; i++) {
		if (finished_link) {
			finished_link = false;
			if (Msg[i] != 0x20)
				New.append(" ");
		}
		if (Msg[i] == 0x12) {
			if (in_link) {
				// we finished an item link
				// "](https://www.takproject.net/allaclone/item.php?id="
				New.append("](https://www.pqdi.cc/item/");
				New.append(itemid);
				New.append(")");
				finished_link = true;
				in_link = false;
				itemid = "";
				item_count = 0;
			}
			else {
				// beginning a new item link
				New.append("["); // "["
				in_link = true;
			}
		}
		else {
			if (in_link) {
				if (item_count < 7) {
					item_count++;
					if (Msg[i] >= 48 && Msg[i] < 58) { // 0-9
						itemid.push_back(Msg[i]);
					}
				}
				else {
					New.push_back(Msg[i]);
				}
			}
			else {
				New.push_back(Msg[i]);
			}
		}
	}

	*Ret = new char[New.length() + 1];
	strcpy(*Ret, New.c_str());

	return New.length();
}

void ChatChannelList::ChatChannelDiscordRelay(ChatChannel *channel, Client *client, const char *message)
{
	// expected format for rule Chat::ChatChannelDiscordRelayConfig is Channel1:webhook_id1,Channel2:webhook_id2
	std::string config_string = RuleS(Chat, ChatChannelDiscordRelayConfig);
	if (!config_string.empty()) {
		const auto list = Strings::Split(config_string, ",");
		for (const auto &channel_config : list) {
			size_t colon;
			if ((colon = channel_config.find(":")) != std::string::npos) {
				std::string channel_name = CapitaliseName(channel_config.substr(0, colon));
				uint32 webhook_id = atoi(channel_config.substr(colon + 1).c_str());

				if (channel_name.length() > 0 && channel_name.compare(CapitaliseName(channel->GetName())) == 0) {
					// queue discord webhook
					DiscordWebhookMessage_Struct q;
					q.webhook_id = webhook_id;
					std::string wsn = "";
					if (worldserverlist->GetServerCount() > 1) // only add the world short name if the UCS is configured with multiple servers
						wsn = " **" + client->GetWorldShortName() + "**";
					snprintf(q.message, sizeof(q.message), "**%s** [%d %s %s]%s\n%s", client->GetName().c_str(), client->GetLevel(), GetRaceIDName(client->GetRace()), GetClassIDName(client->GetClass(), 1), wsn.c_str(), message);
					char *discordFormattedMessage = 0;
					EncodeDiscordChatItemLinkMsg("", q.message, &discordFormattedMessage);
					discord_manager.QueueWebhookMessage(
						q.webhook_id,
						discordFormattedMessage
					);
					safe_delete_array(discordFormattedMessage);
				}
			}
		}
	}
}
