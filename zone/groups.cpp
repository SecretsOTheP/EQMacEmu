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
#include "../common/eqemu_logsys.h"
#include "masterentity.h"
#include "../common/packet_functions.h"
#include "../common/packet_dump.h"
#include "../common/strings.h"
#include "../common/misc_functions.h"
#include "worldserver.h"
#include "string_ids.h"
#include "queryserv.h"
#include "../common/events/player_event_logs.h"

extern EntityList entity_list;
extern WorldServer worldserver;
extern QueryServ* QServ;

/*

note about how groups work:
A group contains 2 list, a list of pointers to members and a
list of member names. All members of a group should have their
name in the membername array, whether they are in the zone or not.
Only members in this zone will have non-null pointers in the
members array.

*/

//create a group which should already exist in the database
Group::Group(uint32 gid)
: GroupIDConsumer(gid)
{
	leader = nullptr;
	memset(members,0,sizeof(Mob*) * MAX_GROUP_MEMBERS);
	disbandcheck = false;
	uint32 i;
	for(i=0;i<MAX_GROUP_MEMBERS;i++)
	{
		memset(membername[i],0,64);
	}
	memset(leadername, 0, 64);
	memset(oldleadername, 0, 64);

	if(gid != 0) {
		if(!LearnMembers())
			SetID(0);

		if(GetLeader() != nullptr)
		{
			SetOldLeaderName(GetLeaderName());
		}
	}
}

//creating a new group
Group::Group(Mob* leader)
: GroupIDConsumer()
{
	memset(members, 0, sizeof(members));
	members[0] = leader;
	leader->SetGrouped(true);
	SetLeader(leader);
	SetOldLeaderName(leader->GetName());
	Log(Logs::Detail, Logs::Group, "Group:Group() Setting OldLeader to: %s and Leader to: %s", GetOldLeaderName(), leader->GetName());
	disbandcheck = false;
	maxlevel = 1;
	minlevel = 65;
	uint32 i;
	for(i=0;i<MAX_GROUP_MEMBERS;i++)
	{
		memset(membername[i],0,64);
	}
	strcpy(membername[0],leader->GetName());

	if(leader->IsClient())
		strcpy(leader->CastToClient()->GetPP().groupMembers[0],leader->GetName());

}

Group::~Group()
{

}

void Group::SplitMoney(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, Client *splitter, bool share) {
	if (!copper && !silver && !gold && !platinum) {
		return;
	}

	if (share && splitter && splitter->IsSelfFound()) {
		splitter->Message(Chat::Red, "The /split function is not allowed in a self found group.");
		return;
	}

	uint8 member_count = 0;

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] && members[i]->IsClient()) {
			member_count++;
		}
	}

	if (!member_count) {
		return;
	}

	// Calculate split and remainder for each coin type
	uint32 copper_split = copper / member_count;
	uint32 copper_remainder = copper % member_count;
	uint32 silver_split = silver / member_count;
	uint32 silver_remainder = silver % member_count;
	uint32 gold_split = gold / member_count;
	uint32 gold_remainder = gold % member_count;
	uint32 platinum_split = platinum / member_count;
	uint32 platinum_remainder = platinum % member_count;
	
	uint8 random_member = zone->random.Int(0, member_count - 1);



	// Loop through the group members to split the coins.
	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] && members[i]->IsClient()) { // If Group Member is Client
			Client* member_client = members[i]->CastToClient();

			uint32 receive_copper = copper_split;
			uint32 receive_silver = silver_split;
			uint32 receive_gold = gold_split;
			uint32 receive_platinum = platinum_split;

			// if /split is used then splitter gets the remainder + split.
			// if /autosplit is used then random players in the group will get the remainder + split.
			if(share ? member_client == splitter : member_client == members[random_member]) {
				receive_copper += copper_remainder;
				receive_silver += silver_remainder;
				receive_gold += gold_remainder;
				receive_platinum += platinum_remainder;
			}

			// the group member other than the character doing the /split only gets this message "(splitter) shares the money with the group"
			if (share && member_client != splitter) {
				member_client->Message_StringID(Chat::Green, StringID::SHARE_MONEY, splitter->GetCleanName());
			}

			// Check if there are any coins to add to the player's purse.
			if (receive_copper || receive_silver || receive_gold || receive_platinum) {
				member_client->AddMoneyToPP(receive_copper, receive_silver, receive_gold, receive_platinum, true);
				member_client->Message_StringID(Chat::Green, StringID::YOU_RECEIVE_AS_SPLIT, Strings::Money(receive_platinum, receive_gold, receive_silver, receive_copper).c_str());
			}

			// If logging of player money transactions is enabled, record the transaction.
			if (player_event_logs.IsEventEnabled(PlayerEvent::SPLIT_MONEY)) {
				auto e = PlayerEvent::SplitMoneyEvent{
					.copper = receive_copper,
					.silver = receive_silver,
					.gold = receive_gold,
					.platinum = receive_platinum,
					.player_money_balance = member_client->GetCarriedMoney(),
				};
				RecordPlayerEventLogWithClient(member_client, PlayerEvent::SPLIT_MONEY, e);
			}

			if (RuleB(QueryServ, PlayerLogMoneyTransactions))
			{
				uint32 fromid = splitter ? splitter->CharacterID() : 0;
				uint32 copper = (platinum_split * 1000) + (gold_split * 100) + (silver_split * 10) + copper_split;
				QServ->QSCoinMove(fromid, members[i]->CastToClient()->CharacterID(), 0, 100, copper);
			}
		}
	}
}

bool Group::AddMember(Mob* newmember, const char *NewMemberName, uint32 CharacterID)
{
	bool InZone = true;

	// This method should either be passed a Mob*, if the new member is in this zone, or a nullptr Mob*
	// and the name and CharacterID of the new member, if they are out of zone.
	//
	if (!newmember && !NewMemberName)
		return false;

	if (GroupCount() >= MAX_GROUP_MEMBERS) //Sanity check for merging groups together.
	{
		if (newmember)
			newmember->Message_StringID(Chat::White, StringID::GROUP_IS_FULL);
		return false;
	}

	if (!newmember)
		InZone = false;
	else
	{
		NewMemberName = newmember->GetCleanName();

		if (newmember->IsClient())
			CharacterID = newmember->CastToClient()->CharacterID();
	}

	uint32 i = 0;

	// See if they are already in the group
	//
	for (i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if (!strcasecmp(membername[i], NewMemberName))
			return false;

	// Put them in the group
	for (i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if (membername[i][0] == '\0')
		{
			if (InZone)
				members[i] = newmember;

			break;
		}
	}

	if (i == MAX_GROUP_MEMBERS)
		return false;

	strcpy(membername[i], NewMemberName);

	int x = 1;

	//build the template join packet
	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gj = (GroupJoin_Struct*)outapp->pBuffer;
	strcpy(gj->membername, NewMemberName);
	gj->action = groupActJoin;

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr && members[i] != newmember) {
			//fill in group join & send it
			strcpy(gj->yourname, members[i]->GetCleanName());
			if (members[i]->IsClient()) {
				members[i]->CastToClient()->QueuePacket(outapp);

				//put new member into existing person's list
				strcpy(members[i]->CastToClient()->GetPP().groupMembers[this->GroupCount() - 1], NewMemberName);
			}

			//put this existing person into the new member's list
			if (InZone && newmember && newmember->IsClient()) {
				if (IsLeader(members[i]))
					strcpy(newmember->CastToClient()->GetPP().groupMembers[0], members[i]->GetCleanName());
				else {
					strcpy(newmember->CastToClient()->GetPP().groupMembers[x], members[i]->GetCleanName());
					++x;
				}
			}
		}
	}

	if (InZone && newmember)
	{
		//put new member in his own list.
		newmember->SetGrouped(true);

		if (newmember->IsClient())
		{
			strcpy(newmember->CastToClient()->GetPP().groupMembers[x], NewMemberName);
			newmember->CastToClient()->Save();
			newmember->CastToClient()->UpdateGroupID(GetID());
		}
	}
	else {
		uint32 AccountID = database.GetAccountIDByChar(CharacterID);
		database.SetGroupID(NewMemberName, GetID(), CharacterID, AccountID);
		// send update to worldserver, to allow updating ClientListEntry
		auto pack = new ServerPacket(ServerOP_GroupSetID, sizeof(GroupSetID_Struct));
		GroupSetID_Struct* gsid = (GroupSetID_Struct*) pack->pBuffer;
		gsid->char_id = CharacterID;
		gsid->group_id = GetID();
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	safe_delete(outapp);

	return true;
}

void Group::AddMember(const char *NewMemberName)
{
	// This method should be called when both the new member and the group leader are in a different zone to this one.
	//
	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if (!strcasecmp(membername[i], NewMemberName))
		{
			return;
		}

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if (membername[i][0] == '\0')
		{
			strcpy(membername[i], NewMemberName);
			break;
		}
	}
}


void Group::QueuePacket(const EQApplicationPacket *app, bool ack_req)
{
	uint32 i;
	for(i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if(members[i] && members[i]->IsClient())
		{
			members[i]->CastToClient()->QueuePacket(app, ack_req);
		}
	}
}

// sends the rest of the group's hps to member. this is useful when
// someone first joins a group, but otherwise there shouldn't be a need to
// call it
void Group::SendHPPacketsTo(Mob *member)
{
	if(member && member->IsClient())
	{
		EQApplicationPacket hpapp;

		for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++)
		{
			if(members[i] && members[i] != member)
			{
				members[i]->CreateHPPacket(&hpapp);
				member->CastToClient()->QueuePacket(&hpapp, false);
				safe_delete_array(hpapp.pBuffer);
				hpapp.size = 0;
			}
		}
	}
}

void Group::SendHPPacketsFrom(Mob *member)
{
	EQApplicationPacket hp_app;
	if(!member)
		return;

	member->CreateHPPacket(&hp_app);

	uint32 i;
	for(i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(members[i] && members[i] != member && members[i]->IsClient())
		{
			members[i]->CastToClient()->QueuePacket(&hp_app);
		}
	}
}

//updates a group member's client pointer when they zone in
//if the group was in the zone already
bool Group::UpdatePlayer(Mob* update)
{
	VerifyGroup();

	uint32 i=0;
	if(update->IsClient()) {
		//update their player profile
		PlayerProfile_Struct &pp = update->CastToClient()->GetPP();
		for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if(membername[i][0] == '\0')
				memset(pp.groupMembers[i], 0, 64);
			else
				strn0cpy(pp.groupMembers[i], membername[i], 64);
		}
	}

	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (!strcasecmp(membername[i],update->GetName()))
		{
			members[i] = update;
			members[i]->SetGrouped(true);
			return true;
		}
	}
	return false;
}


void Group::MemberZoned(Mob* removemob) {
	uint32 i;

	if (removemob == nullptr)
		return;

	if(removemob == GetLeader())
		SetLeader(nullptr);

	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] == removemob)
		{
			members[i] = nullptr;
			//should NOT clear the name, it is used for world communication.
			break;
		}
	}
}

void Group::SendGroupJoinOOZ(Mob* NewMember) {

	if (NewMember == nullptr)
	{
		return;
	}
	
	if (!NewMember->HasGroup())
	{
		return;
	}

	//send updates to clients out of zone...
	auto pack = new ServerPacket(ServerOP_GroupJoin, sizeof(ServerGroupJoin_Struct));
	ServerGroupJoin_Struct* gj = (ServerGroupJoin_Struct*)pack->pBuffer;
	gj->gid = GetID();
	gj->zoneguildid = zone->GetGuildID();
	gj->zoneid = zone->GetZoneID();
	strcpy(gj->member_name, NewMember->GetName());
	worldserver.SendPacket(pack);
	safe_delete(pack);

}

bool Group::DelMemberOOZ(const char *Name, bool checkleader) {

	if(!Name) return false;

	bool removed = false;
	// If a member out of zone has disbanded, clear out their name.
	for(unsigned int i = 0; i < MAX_GROUP_MEMBERS; i++) 
	{
		if(!strcasecmp(Name, membername[i]))
		{
			// This shouldn't be called if the member is in this zone.
			if(!members[i]) {
				memset(membername[i], 0, 64);
				removed = true;
				Log(Logs::Detail, Logs::Group, "DelMemberOOZ: Removed Member: %s", Name);
				break;
			}
		}
	}

	if(GroupCount() < 2)
	{
		DisbandGroup();
		return true;
	}

	if(checkleader)
	{
		Log(Logs::Detail, Logs::Group, "DelMemberOOZ: Checking leader...");
		if (strcmp(GetOldLeaderName(),Name) == 0 && GroupCount() >= 2)
		{
			for(uint32 nl = 0; nl < MAX_GROUP_MEMBERS; nl++)
			{
				if(members[nl]) 
				{
					if (members[nl]->IsClient())
					{
						ChangeLeader(members[nl]);
						break;
					}
				}
			}
		}
	}

	return removed;
}

bool Group::DelMember(Mob* oldmember)
{
	if (oldmember == nullptr)
	{
		return false;
	}

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] == oldmember)
		{
			members[i] = nullptr;
			membername[i][0] = '\0';
			memset(membername[i],0,64);
			Log(Logs::Detail, Logs::Group, "DelMember: Removed Member: %s", oldmember->GetCleanName());
			break;
		}
	}

	if(GroupCount() < 2)
	{
		DisbandGroup();
		return true;
	}

	// If the leader has quit and we have 2 or more players left in group, we want to first check the zone the old leader was in for a new leader. 
	// If a suitable replacement cannot be found, we need to go out of zone. If checkleader remains true after this method completes, another
	// loop will be run in DelMemberOOZ.
	bool checkleader = true;
	if (strcmp(GetOldLeaderName(),oldmember->GetCleanName()) == 0 && GroupCount() >= 2)
	{
		for(uint32 nl = 0; nl < MAX_GROUP_MEMBERS; nl++)
		{
			if(members[nl]) 
			{
				if (members[nl]->IsClient())
				{
					ChangeLeader(members[nl]);
					checkleader = false;
					break;
				}
			}
		}
	}
			
	auto pack = new ServerPacket(ServerOP_GroupLeave, sizeof(ServerGroupLeave_Struct));
	ServerGroupLeave_Struct* gl = (ServerGroupLeave_Struct*)pack->pBuffer;
	gl->gid = GetID();
	gl->zoneid = zone->GetZoneID();
	gl->zoneguildid = zone->GetGuildID();
	strcpy(gl->member_name, oldmember->GetName());
	gl->checkleader = checkleader;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gu = (GroupJoin_Struct*)outapp->pBuffer;
	gu->action = groupActLeave;
	strcpy(gu->membername, oldmember->GetCleanName());
	strcpy(gu->yourname, oldmember->GetCleanName());

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] == nullptr) {
			//if (DEBUG>=5) Log(Logs::Detail, Logs::Group, "Group::DelMember() null member at slot %i", i);
			continue;
		}
		if (members[i] != oldmember) {
			strcpy(gu->yourname, members[i]->GetCleanName());
			if(members[i]->IsClient())
				members[i]->CastToClient()->QueuePacket(outapp);
		}
	}
	safe_delete(outapp);

	outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupGeneric_Struct2));

	GroupGeneric_Struct2* gr = (GroupGeneric_Struct2*) outapp->pBuffer;
	gr->action = groupActDisband2;
	gr->param = StringID::GROUP_REMOVED;

	strcpy(gr->membername,oldmember->GetCleanName());
	strcpy(gr->yourname,oldmember->GetCleanName());

	if(oldmember->IsClient())
		oldmember->CastToClient()->QueuePacket(outapp);

	safe_delete(outapp);

	if(oldmember->IsClient())
	{
		oldmember->CastToClient()->UpdateGroupID(0);
	}
	
	oldmember->SetGrouped(false);
	disbandcheck = true;

	safe_delete(outapp);

	return true;
}

// does the caster + group
void Group::CastGroupSpell(Mob* caster, uint16 spell_id, bool isrecourse, int recourse_level) {

	uint32 z;
	float range, distance;

	if(!caster)
		return;

	Log(Logs::Detail, Logs::Spells, "%s is casting spell %d on group %d", caster->GetName(), spell_id, GetID());

	castspell = true;
	range = caster->GetAOERange(spell_id);

	float range2 = range*range;

	bool hitcaster = false;
	for(z=0; z < MAX_GROUP_MEMBERS; z++)
	{
		if(members[z] == caster) 
		{
			caster->SpellOnTarget(spell_id, caster, false, false, 0, false, 0, true, recourse_level);
			hitcaster = true;
#ifdef GROUP_BUFF_PETS
			if(caster->GetPet() && caster->HasPetAffinity() && !caster->GetPet()->IsCharmedPet())
				caster->SpellOnTarget(spell_id, caster->GetPet());
#endif
		}
		else if(members[z] != nullptr)
		{
			distance = DistanceSquared(caster->GetPosition(), members[z]->GetPosition());
			if(distance <= range2) {
				caster->SpellOnTarget(spell_id, members[z], false, false, 0, false, 0, isrecourse, recourse_level);
#ifdef GROUP_BUFF_PETS
				if(members[z]->GetPet() && members[z]->HasPetAffinity() && !members[z]->GetPet()->IsCharmedPet())
					caster->SpellOnTarget(spell_id, members[z]->GetPet(), false, false, 0, false, 0, isrecourse, recourse_level);
#endif
			} else
				Log(Logs::Detail, Logs::Spells, "Group spell: %s is out of range %f at distance %f from %s", members[z]->GetName(), range, distance, caster->GetName());
		}
	}

	if (!hitcaster && caster->IsClient() && caster->CastToClient()->TGB())
		caster->SpellOnTarget(spell_id, caster, false, false, 0, false, 0, isrecourse, recourse_level);

	castspell = false;
	disbandcheck = true;
}

bool Group::IsGroupMember(Mob* client)
{
	bool Result = false;

	if(client) {
		for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if (members[i] == client)
				Result = true;
		}
	}

	return Result;
}

bool Group::IsGroupMember(const char *Name)
{
	if(Name)
		for(uint32 i = 0; i < MAX_GROUP_MEMBERS; i++)
			if((strlen(Name) == strlen(membername[i])) && !strncmp(membername[i], Name, strlen(Name)))
				return true;

	return false;
}

void Group::GroupMessage(Mob* sender, uint8 language, uint8 lang_skill, const char* message) {
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(!members[i])
			continue;

		if (members[i]->IsClient() && members[i]->CastToClient()->GetFilter(FilterGroupChat)!=0)
			members[i]->CastToClient()->ChannelMessageSend(sender->GetName(), members[i]->GetName(), ChatChannel_Group, language, lang_skill, message);
	}

	auto pack =
	    new ServerPacket(ServerOP_OOZGroupMessage, sizeof(ServerGroupChannelMessage_Struct) + strlen(message) + 1);
	ServerGroupChannelMessage_Struct* gcm = (ServerGroupChannelMessage_Struct*)pack->pBuffer;
	gcm->zoneid = zone->GetZoneID();
	gcm->zoneguildid = zone->GetGuildID();
	gcm->groupid = GetID();
	gcm->language = language;
	gcm->lang_skill = lang_skill;
	strcpy(gcm->from, sender->GetName());
	strcpy(gcm->message, message);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

int32 Group::GetTotalGroupDamage(Mob* other) 
{
	int32 total = 0;
	for (int i = 0; i < MAX_GROUP_MEMBERS; i++) 
	{
		if(!members[i])
			continue;
		
		total += other->GetDamageAmount(members[i], true);
	}
	return total;
}

void Group::DisbandGroup(bool alt_msg, uint32 msg) {
	auto outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupGeneric_Struct2));

	GroupGeneric_Struct2* gu = (GroupGeneric_Struct2*) outapp->pBuffer;
	gu->action = alt_msg ? groupActDisband2 : groupActDisband;
	gu->param = msg;
	Client *Leader = nullptr;

	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] == nullptr)
		{
			continue;
		}

		if (members[i]->IsClient())
		{
			if(IsLeader(members[i]))
			{
				Leader = members[i]->CastToClient();
			}

			strcpy(gu->yourname, members[i]->GetName());
			members[i]->CastToClient()->UpdateGroupID(0);
			members[i]->CastToClient()->QueuePacket(outapp);
		}
		
		members[i]->SetGrouped(false);
		members[i] = nullptr;
		membername[i][0] = '\0';
	}

	uint32 group_id = GetID();

	auto pack = new ServerPacket(ServerOP_DisbandGroup, sizeof(ServerDisbandGroup_Struct));
	ServerDisbandGroup_Struct* dg = (ServerDisbandGroup_Struct*)pack->pBuffer;
	dg->zoneid = zone->GetZoneID();
	dg->zoneguildid = zone->GetGuildID();
	dg->groupid = group_id;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	entity_list.RemoveGroup(group_id); // will delete 'this'
	if(group_id != 0)
	{
		database.ClearGroup(group_id);
	}

	safe_delete(outapp);
}

bool Group::Process() {
	if(disbandcheck && !GroupCount())
		return false;
	else if(disbandcheck && GroupCount())
		disbandcheck = false;
	return true;
}

void Group::SendUpdate(uint32 type, Mob* member)
{
	if(!member->IsClient())
		return;

	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupUpdate_Struct));
	GroupUpdate_Struct* gu = (GroupUpdate_Struct*)outapp->pBuffer;
	gu->action = type;
	strcpy(gu->yourname,member->GetName());

	int x = 0;

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if((members[i] != nullptr) && IsLeader(members[i]))
		{
			strcpy(gu->leadersname, members[i]->GetName());
			break;
		}

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if (members[i] != nullptr && members[i] != member)
			strcpy(gu->membername[x++], members[i]->GetName());

	member->CastToClient()->QueuePacket(outapp);

	safe_delete(outapp);
}

uint8 Group::GroupCount() {

	uint8 MemberCount = 0;

	for(uint8 i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if(membername[i][0])
		{
			++MemberCount;
		}
	}

	return MemberCount;
}

uint32 Group::GetHighestLevel()
{
	uint32 level = 1;
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i])
		{
			if (members[i]->GetLevel() > level)
				level = members[i]->GetLevel();
		}
	}
	return level;
}

uint32 Group::GetHighestLevel2()
{
	uint32 level = 1;
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] && members[i]->IsClient())
		{
			if (members[i]->CastToClient()->GetLevel2() > level)
				level = members[i]->CastToClient()->GetLevel2();
		}
	}
	return level;
}

uint32 Group::GetLowestLevel()
{
uint32 level = 255;
uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i])
		{
			if(members[i]->GetLevel() < level)
				level = members[i]->GetLevel();
		}
	}
	return level;
}

void Group::TeleportGroup(Mob* sender, uint32 zoneID, float x, float y, float z, float heading)
{
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] != nullptr && members[i]->IsClient() && members[i] != sender)
		{
			members[i]->CastToClient()->MovePC(zoneID, x, y, z, heading, 0, ZoneSolicited);
		}
	}
}

bool Group::LearnMembers() {
	std::string query = StringFormat("SELECT name FROM group_id WHERE groupid = %lu", (unsigned long)GetID());
	auto results = database.QueryDatabase(query);
	if (!results.Success())
        return false;

    if (results.RowCount() == 0) {
        Log(Logs::General, Logs::Error, "Error getting group members for group %lu: %s", (unsigned long)GetID(), results.ErrorMessage().c_str());
			return false;
    }

	int memberIndex = 0;
    for(auto row = results.begin(); row != results.end(); ++row) {
		if(!row[0])
			continue;

		members[memberIndex] = nullptr;
		strn0cpy(membername[memberIndex], row[0], 64);

		memberIndex++;
	}

	return true;
}

void Group::VerifyGroup() 
{
	/*
		The purpose of this method is to make sure that a group
		is in a valid state, to prevent dangling pointers.
		Only called every once in a while (on member re-join for now).
	*/

	uint32 i;

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) 
	{
		if (membername[i][0] == '\0') 
		{
			Log(Logs::General, Logs::Group, "Group %lu: Verify %d: Empty.", (unsigned long)GetID(), i);
			members[i] = nullptr;
			continue;
		}

		bool valid = true;
		std::string query = StringFormat("SELECT count(*) FROM group_id WHERE groupid = %lu and name = '%s'", (unsigned long)GetID(), membername[i]);
		auto results = database.QueryDatabase(query);

		if (!results.Success())
			valid = false;

		if (results.RowCount() == 0)
			valid = false;

		for (auto row = results.begin(); row != results.end(); ++row)
		{
			uint8 count = atoi(row[0]);

			if (count < 1)
				valid = false;
		}

		if (!valid)
		{
			Log(Logs::General, Logs::Group, "Member of group %lu named '%s' was not found in the database.", (unsigned long)GetID(), membername[i]);
			membername[i][0] = '\0';
			members[i] = nullptr;
			continue;
		}

		//it should be safe to use GetClientByName, but Group is trying
		//to be generic, so we'll go for general Mob
		Mob *them = entity_list.GetMob(membername[i]);
		if(them == nullptr && members[i] != nullptr) 
		{	//they aren't here anymore...
			Log(Logs::General, Logs::Group, "Member of group %lu named '%s' has left the zone.", (unsigned long)GetID(), membername[i]);
			//membername[i][0] = '\0';
			members[i] = nullptr;
			continue;
		}

		if(them != nullptr && members[i] != them) 
		{	//our pointer is out of date... update it.
			Log(Logs::General, Logs::Group, "Member of group %lu named '%s' had their pointer updated.", (unsigned long)GetID(), membername[i]);
			members[i] = them;
			continue;
		}

		Log(Logs::General, Logs::Group, "Member of group %lu named '%s' is valid.", (unsigned long)GetID(), membername[i]);
	}
}


void Group::GroupMessage_StringID(Mob* sender, uint32 type, uint32 string_id, const char* message,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9, uint32 distance) {
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(members[i] == nullptr)
			continue;

		if(members[i] == sender)
			continue;

		members[i]->Message_StringID(type, string_id, message, message2, message3, message4, message5, message6, message7, message8, message9, 0);
	}
}



void Client::LeaveGroup() {
	Group *g = GetGroup();

	if(g)
	{
		int32 MemberCount = g->GroupCount();
		if(MemberCount <= 2)
		{
			g->DisbandGroup();
		}
		else
		{
			g->DelMember(this);
		}

		Log(Logs::General, Logs::Group, "%s has left the group.", GetName());
	}
	else
	{
		//force things a little
		UpdateGroupID(0);
	}

	isgrouped = false;
}

void Group::HealGroup(uint32 heal_amt, Mob* caster, float range)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	float distance;
	float range2 = range*range;


	int numMem = 0;
	unsigned int gi = 0;
	for(; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				numMem += 1;
			}
		}
	}

	heal_amt /= numMem;
	for(gi = 0; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				members[gi]->HealDamage(heal_amt, caster);
				members[gi]->SendHPUpdate();
			}
		}
	}
}


void Group::BalanceHP(int32 penalty, float range, Mob* caster, int32 limit)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	int dmgtaken = 0, numMem = 0, dmgtaken_tmp = 0;

	float distance;
	float range2 = range*range;

	unsigned int gi = 0;
	for(; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){

				dmgtaken_tmp = members[gi]->GetMaxHP() - members[gi]->GetHP();
				if (limit && (dmgtaken_tmp > limit))
					dmgtaken_tmp = limit;

				dmgtaken += (dmgtaken_tmp);
				numMem += 1;
			}
		}
	}

	dmgtaken += dmgtaken * penalty / 100;
	dmgtaken /= numMem;
	int old_hp, new_hp;

	for(gi = 0; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){

				old_hp = members[gi]->GetHP();
				new_hp = members[gi]->GetMaxHP() - dmgtaken;
				if (new_hp < 1)
					new_hp = 1;	// can divine arb kill people?

				if (new_hp > old_hp)
				{
					members[gi]->Message_StringID(Chat::Spells, StringID::DIV_ARB_TAKE);
					members[gi]->Message_StringID(Chat::Spells, StringID::YOU_HEALED, itoa(new_hp - old_hp));
				}
				else
					members[gi]->Message_StringID(Chat::Spells, StringID::DIV_ARB_GIVE);

				members[gi]->SetHP(new_hp);
				members[gi]->SendHPUpdate();
			}
		}
	}
}

void Group::BalanceMana(int32 penalty, float range, Mob* caster, int32 limit)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	float distance;
	float range2 = range*range;

	int manataken = 0, numMem = 0, manataken_tmp = 0;
	unsigned int gi = 0;
	for(; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi] && (members[gi]->GetMaxMana() > 0)){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){

				manataken_tmp = members[gi]->GetMaxMana() - members[gi]->GetMana();
				if (limit && (manataken_tmp > limit))
					manataken_tmp = limit;

				manataken += (manataken_tmp);
				numMem += 1;
			}
		}
	}

	manataken += manataken * penalty / 100;
	manataken /= numMem;

	if (limit && (manataken > limit))
		manataken = limit;

	for(gi = 0; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				if((members[gi]->GetMaxMana() - manataken) < 1){
					members[gi]->SetMana(1);
					if (members[gi]->IsClient())
						members[gi]->CastToClient()->SendManaUpdate();
				}
				else{
					members[gi]->SetMana(members[gi]->GetMaxMana() - manataken);
					if (members[gi]->IsClient())
						members[gi]->CastToClient()->SendManaUpdate();
				}
			}
		}
	}
}

uint16 Group::GetAvgLevel()
{
	double levelHolder = 0;
	uint8 i = 0;
	uint8 numMem = 0;
	while(i < MAX_GROUP_MEMBERS)
	{
		if (members[i])
		{
			numMem++;
			levelHolder = levelHolder + (members[i]->GetLevel());
		}
		i++;
	}
	levelHolder = ((levelHolder/numMem)+.5); // total levels divided by num of characters
	return (uint16(levelHolder));
}

int8 Group::GetNumberNeedingHealedInGroup(int8 hpr, bool includePets) {
	int8 needHealed = 0;

	for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
		if(members[i] && !members[i]->qglobal) {

			if(members[i]->GetHPRatio() <= hpr)
				needHealed++;

			if(includePets) {
				if(members[i]->GetPet() && members[i]->GetPet()->GetHPRatio() <= hpr) {
					needHealed++;
				}
			}
		}
	}


	return needHealed;
}

void Group::ChangeLeader(Mob* newleader)
{
	// this changes the current group leader, notifies other members, and updates leadship AA

	// if the new leader is invalid, do nothing
	if (!newleader)
		return;

	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gu = (GroupJoin_Struct*) outapp->pBuffer;
	gu->action = groupActMakeLeader;

	strcpy(gu->membername, newleader->GetName());
	strcpy(gu->yourname, GetOldLeaderName());
	SetLeader(newleader);
	database.SetGroupLeaderName(GetID(), newleader->GetName());
	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] && members[i]->IsClient())
		{
			members[i]->CastToClient()->QueuePacket(outapp);
			Log(Logs::Detail, Logs::Group, "ChangeLeader(): Local leader update packet sent to: %s .", members[i]->GetName());
		}
	}
	safe_delete(outapp);

	Log(Logs::Detail, Logs::Group, "ChangeLeader(): Old Leader is: %s New leader is: %s", GetOldLeaderName(), newleader->GetName());

	auto pack = new ServerPacket(ServerOP_ChangeGroupLeader, sizeof(ServerGroupLeader_Struct));
	ServerGroupLeader_Struct* fgu = (ServerGroupLeader_Struct*)pack->pBuffer;
	fgu->zoneid = zone->GetZoneID();
	fgu->zoneguildid = zone->GetGuildID();
	fgu->gid = GetID();
	strcpy(fgu->leader_name, newleader->GetName());
	strcpy(fgu->oldleader_name, GetOldLeaderName());
	fgu->leaderset = true;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	SetOldLeaderName(newleader->GetName());
	database.SetGroupOldLeaderName(GetID(), newleader->GetName());
}

void Group::ChangeLeaderByName(std::string newleader)
{
	Log(Logs::Detail, Logs::Group, "ChangeLeaderByName(): Old Leader is: %s New leader is: %s. Sending to world for confirmation.", GetOldLeaderName(), newleader.c_str());

	auto pack = new ServerPacket(ServerOP_CheckGroupLeader, sizeof(ServerGroupLeader_Struct));
	ServerGroupLeader_Struct* fgu = (ServerGroupLeader_Struct*)pack->pBuffer;
	fgu->zoneid = 0;
	fgu->zoneguildid = 0;
	fgu->gid = GetID();
	strcpy(fgu->leader_name, newleader.c_str());
	strcpy(fgu->oldleader_name, GetOldLeaderName());
	fgu->leaderset = false;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}


const char *Group::GetClientNameByIndex(uint8 index)
{
	return membername[index];
}

void Group::SetLevels()
{
	maxlevel = 1;
	minlevel = 65;
	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) 
	{
		if (members[i] && members[i]->IsClient())
		{
			if (members[i]->GetLevel() > maxlevel)
				maxlevel = members[i]->GetLevel();

			if (members[i]->GetLevel() < minlevel)
				minlevel = members[i]->GetLevel();
		}
	}

	Log(Logs::General, Logs::Group, "Group: Maxlevel is %d minlevel is %d", maxlevel, minlevel);
}

bool Group::HasOOZMember(std::string& member)
{
	for (uint8 i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if (members[i] == nullptr) 
		{
			if (membername[i][0] == '\0')
				continue;

			member = membername[i];
			return true;
		}
	}

	return false;
}

std::string Group::GetMemberNamesAsCsv(const std::vector<std::string>& excludes)
{
	std::string names;
	for (uint8 i = 0; i < MAX_GROUP_MEMBERS; ++i) {
		if (membername[i][0] == '\0')
			continue;

		if (std::find(excludes.begin(), excludes.end(), membername[i]) != excludes.end())
			continue;

		if (!names.empty())
			names += ",";

		names += membername[i];
	}
	return names;
}
