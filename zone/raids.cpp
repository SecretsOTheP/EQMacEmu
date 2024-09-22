/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2005 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/strings.h"

#include "client.h"
#include "entity.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"
#include "string_ids.h"

#include "worldserver.h"

extern EntityList entity_list;
extern WorldServer worldserver;

Raid::Raid(uint32 raidID)
: GroupIDConsumer(raidID)
{
	memset(members ,0, (sizeof(RaidMember)*MAX_RAID_MEMBERS));
	leader = nullptr;
	memset(leadername, 0, 64);
	currentleaderguildid = GUILD_NONE;
	raid_engage_check_result = true;
	LootType = 1;
	leader_charid = 0;
	disbandCheck = false;
	forceDisband = false;
}

Raid::Raid(Client* nLeader)
: GroupIDConsumer()
{
	memset(members ,0, (sizeof(RaidMember)*MAX_RAID_MEMBERS));
	leader = nLeader;
	memset(leadername, 0, 64);
	strn0cpy(leadername, nLeader->GetName(), 64);
	currentleaderguildid = nLeader->GuildID();
	LootType = 1;
	leader_charid = nLeader->CharacterID();
	disbandCheck = false;
	forceDisband = false;
	raid_engage_check_result = true;
}

Raid::~Raid()
{
}

bool Raid::Process()
{
	if(forceDisband)
		return false;
	if(disbandCheck)
	{
		int count = 0;
		for(int x = 0; x < MAX_RAID_MEMBERS; x++)
		{
			if(strlen(members[x].membername) == 0)
				continue;
			else
				count++;
		}
		if(count == 0)
			return false;
	}
	return true;
}

void Raid::AddMember(Client *c, uint32 group, bool rleader, bool groupleader, bool looter, bool skip){
	if(!c || c->GetID() == 0)
		return;

	std::string query = StringFormat("INSERT INTO raid_members SET raidid = %lu, charid = %lu, "
                                    "groupid = %lu, _class = %d, level = %d, name = '%s', "
                                    "isgroupleader = %d, israidleader = %d, islooter = %d, guild_id=%lu, is_officer=%d ",
                                    (unsigned long)GetID(), (unsigned long)c->CharacterID(),
                                    (unsigned long)group, c->GetClass(), c->GetLevel(),
                                    c->GetName(), groupleader, rleader, looter, (unsigned long)c->GuildID(), c->GuildRank());
    auto results = database.QueryDatabase(query);

	if(!results.Success()) {
		Log(Logs::General, Logs::Error, "Error inserting into raid members: %s", results.ErrorMessage().c_str());
	}

	c->SetRaidGrouped(group != 0xFFFFFFFF);

	LearnMembers();
	VerifyRaid();

	Client* skipped = skip ? c : nullptr;
	SendRaidAddAll(c->GetName(), skipped);

	auto pack = new ServerPacket(ServerOP_RaidAdd, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	strn0cpy(rga->playername, c->GetName(), 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::AddGroupToRaid(Client* inviter, Client* invitee, Group* group, uint32 freegroup_id)
{

	if (!invitee)
	{
		return;
	}

	if (!inviter || !group)
	{
		// this will reset the raid for the invited, but will not give them a message their raid was disbanded.
		auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
		RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
		rg->action = RaidCommandSendDisband;
		strcpy(rg->leader_name, invitee->GetCleanName());
		strcpy(rg->player_name, invitee->GetCleanName());
		invitee->QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	Mob* gleader = entity_list.GetMob(group->GetLeaderName());
	std::string ooz_member;
	if (!gleader || gleader->GetID() == 0 || group->HasOOZMember(ooz_member))
	{
		if (!gleader)
		{
			ooz_member = group->GetLeaderName();
		}
		else if (gleader->GetID() == 0)
		{
			ooz_member = "Leader";
			group->SetLeader(nullptr);
		}

		inviter->Message_StringID(Chat::DefaultText, RAID_OOR);
		invitee->Message_StringID(Chat::DefaultText, CANNOT_JOIN_RAID, ooz_member.c_str());

		// this will reset the raid for the invited, but will not give them a message their raid was disbanded.
		auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
		RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
		rg->action = RaidCommandSendDisband;
		strcpy(rg->leader_name, invitee->GetCleanName());
		strcpy(rg->player_name, invitee->GetCleanName());
		invitee->QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	AddMember(gleader->CastToClient(), freegroup_id, false, true, GetLootType() == 2);
	SendRaidMembers(gleader->CastToClient());

	//now add the rest of the group
	for (int x = 0; x < MAX_GROUP_MEMBERS; ++x)
	{
		if (group->members[x] && group->members[x] != gleader) 
		{
			Client *c = nullptr;
			if (group->members[x]->IsClient())
				c = group->members[x]->CastToClient();
			else
				continue;

			AddMember(c, freegroup_id);
			SendRaidMembers(c);
		}
	}

	group->DisbandGroup(true);
	GroupUpdate(freegroup_id);

	return;
}

void Raid::RemoveMember(const char *characterName)
{
	std::string query = StringFormat("DELETE FROM raid_members where name='%s'", characterName);
	auto results = database.QueryDatabase(query);

	Client *client = entity_list.GetClientByName(characterName);
	disbandCheck = true;

	if (client) {
		SendRaidDisband(client);
	}
	SendRaidRemoveAll(characterName, client);

	LearnMembers();
	VerifyRaid();

	// now send to the world server, to remove them in other zones.
	auto pack = new ServerPacket(ServerOP_RaidRemove, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	strn0cpy(rga->playername, characterName, 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::DisbandRaid()
{
	std::string query = StringFormat("DELETE FROM raid_members WHERE raidid = %lu", (unsigned long)GetID());
	auto results = database.QueryDatabase(query);

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member)
		{
			SendGroupDisband(members[x].member);
		}
	}

	SendRaidDisbandAll();
	LearnMembers();
	VerifyRaid();
	GetRaidDetails();
	
	auto pack = new ServerPacket(ServerOP_RaidDisband, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	strn0cpy(rga->playername, " ", 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);

	forceDisband = true;
}

void Raid::DisbandRaidMember(const char *name, Client *who)
{
	// this will reassign group and raid leaders as applicable
	// if who is nul, and it is raid leader, leader will be assigned to someone else.
	uint32 i = GetPlayerIndex(name);
	
	if (i == -1)
		return;

	// there will only be one left, so nuke the raid
	if (RaidCount() < 3) {
		DisbandRaid();
		return;
	}

	if (members[i].IsRaidLeader) {
	// reassign new raid leader
		for (int x = 0; x < MAX_RAID_MEMBERS; x++){
			if (strlen(members[x].membername) > 0 && x != i)
			{
				SetRaidLeader(members[i].membername, members[x].membername);
				break;
			}
		}
	}
	// remove from looters
	if (members[i].IsLooter) {
		RemoveRaidLooter(name);

		auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
		RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
		rg->action = RaidCommandRemoveLooter;
		strn0cpy(rg->leader_name, name, 64);
		strn0cpy(rg->player_name, leadername, 64);
		QueuePacket(outapp);

		// this sends message out that the looter was added
		rg->action = RaidCommandRaidMessage;
		rg->parameter = 5105; // 5105 %1 was removed from the loot list
		QueuePacket(outapp);
		safe_delete(outapp);
	}

	uint32 grp = members[i].GroupNumber;
	uint8 cnt = 0;

	if (grp >= 0 && grp < MAX_RAID_GROUPS) {
		cnt = GroupCount(grp);
		if (members[i].IsGroupLeader){ 
			SetGroupLeader(members[i].membername, grp, false);
			//assign group leader to someone else
			if (cnt > 2) {
				// we will have at least 2 left in group.  So leave group intact
				for (int x = 0; x < MAX_RAID_MEMBERS; x++){
					if (strlen(members[x].membername) > 0 && members[x].GroupNumber == grp && i != x){
						SetGroupLeader(members[x].membername, grp, true);
						break;
					}
				}
			}
		}
	}

	if (grp >= 0 && grp < MAX_RAID_GROUPS) {
		SendRaidGroupRemove(name, grp);
		if (cnt == 2) {
			// only one left in group, so move them to ungrouped.
			for (int x = 0; x < MAX_RAID_MEMBERS; x++){
				if (strlen(members[x].membername) > 0 && members[x].GroupNumber == grp){
					if (strcmp(members[x].membername, leadername) != 0) {
						// we are not the raid leader.
						if (members[x].IsGroupLeader) {
							// to unset group leader flag, we send the name over of a different group leader
							for (int x = 0; x < MAX_RAID_MEMBERS; x++)
							{
								if (members[x].GroupNumber != grp)
								{
									if (strlen(members[x].membername) > 0 && members[x].IsGroupLeader)
									{
										UnSetGroupLeader(name, members[x].membername, grp);
										break;
									}
								}
							}
							//SetGroupLeader(members[x].membername, 0xFFFFFFFF, false);
						}
						// this moves them out of group in raid window.
						MoveMember(members[x].membername, 0xFFFFFFFF);
						// this removes them from group window
						SendRaidGroupRemove(members[x].membername, grp);
					} else {
						// we are also the raid leader, so assign us as group leader, dont disband group
						SetGroupLeader(members[x].membername, grp, true);
						//GroupUpdate(grp);
					}
				}
			}
		}
	}
	RemoveMember(name);

}

void Raid::MoveMember(const char *name, uint32 newGroup)
{
	std::string query = StringFormat("UPDATE raid_members SET groupid = %lu WHERE name = '%s'",
                                    (unsigned long)newGroup, name);
    auto results = database.QueryDatabase(query);

	LearnMembers();
	VerifyRaid();
	SendRaidChangeGroup(name, newGroup);

	auto pack = new ServerPacket(ServerOP_RaidChangeGroup, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	strn0cpy(rga->playername, name, 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	rga->gid = newGroup;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::SetGroupLeader(const char *who, uint32 gid, bool flag)
{
	std::string query = StringFormat("UPDATE raid_members SET isgroupleader = %lu WHERE name = '%s'",
                                    (unsigned long)flag, who);
    auto results = database.QueryDatabase(query);

	LearnMembers();
	VerifyRaid();
	if (!flag)
		return;
	SendMakeGroupLeaderPacket(who, gid);
	auto pack = new ServerPacket(ServerOP_RaidGroupLeader, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->gid = gid;
	strn0cpy(rga->playername, who, 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::UnSetGroupLeader(const char *who, const char *other, uint32 gid)
{
	std::string query = StringFormat("UPDATE raid_members SET isgroupleader = 0 WHERE name = '%s'", who);
    auto results = database.QueryDatabase(query);

	LearnMembers();
	VerifyRaid();

	SendMakeGroupLeaderPacket(other, gid);

	auto pack = new ServerPacket(ServerOP_RaidGroupLeader, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->gid = gid;
	strn0cpy(rga->playername, other, 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::SetRaidLeader(const char *wasLead, const char *name)
{
	if (strlen(wasLead) > 0) {
		std::string query = StringFormat("UPDATE raid_members SET israidleader = 0 WHERE name = '%s'", wasLead);
		auto results = database.QueryDatabase(query);
		if (!results.Success())
			Log(Logs::General, Logs::Error, "Set Raid Leader error: %s\n", results.ErrorMessage().c_str());
	}
	if (strlen(name) > 0) {
		std::string query = StringFormat("UPDATE raid_members SET israidleader = 1, islooter = 1 WHERE name = '%s'", name);
		auto results = database.QueryDatabase(query);
		if (!results.Success())
			Log(Logs::General, Logs::Error, "Set Raid Leader error: %s\n", results.ErrorMessage().c_str());
	}
	strn0cpy(leadername, name, 64);

	uint32 leaderCharID = database.GetCharacterID(name);

	Client *c = entity_list.GetClientByName(name);
	if (c)
	{
		SetLeader(c);
		SetRaidDetails(leaderCharID);
	}
	LearnMembers();
	VerifyRaid();
	SendMakeLeaderPacket(name);

	auto pack = new ServerPacket(ServerOP_RaidLeader, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	strn0cpy(rga->playername, name, 64);
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	rga->gleader = leaderCharID;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

bool Raid::IsGroupLeader(const char *who)
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(who, members[x].membername) == 0){
			return members[x].IsGroupLeader;
		}
	}

	return false;
}

void Raid::UpdateLevel(const char *name, int newLevel)
{
	std::string query = StringFormat("UPDATE raid_members SET level = %lu WHERE name = '%s'",
                                    (unsigned long)newLevel, name);
    auto results = database.QueryDatabase(query);

	LearnMembers();
	VerifyRaid();
}

uint32 Raid::GetFreeGroup()
{
	//check each group return the first one with 0 members assigned...
	for(int x = 0; x < MAX_RAID_GROUPS; x++)
	{
		int count = 0;
		for(int y = 0; y < MAX_RAID_MEMBERS; y++)
		{
			if(members[y].GroupNumber == x && (strlen(members[y].membername)>0))
				count++;
		}
		if(count == 0)
			return x;
	}
	//if we get to here then there were no free groups so we added the group as free floating members.
	return 0xFFFFFFFF;
}

uint8 Raid::GroupCount(uint32 gid)
{
	uint8 count = 0;
	if(gid >= 0 && gid < MAX_RAID_GROUPS)
	{
		for(int x = 0; x < MAX_RAID_MEMBERS; x++)
		{
			if(members[x].GroupNumber == gid && strlen(members[x].membername)>0)
			{
				count++;
			}
		}
	}
	else
	{
		for(int x = 0; x < MAX_RAID_MEMBERS; x++)
		{
			if(members[x].GroupNumber == 0xFFFFFFFF && strlen(members[x].membername)>0)
			{
				count++;
			}
		}
	}
	return count;
}

uint8 Raid::RaidCount()
{
	int count = 0;
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strlen(members[x].membername) > 0)
			count++;
	}
	return count;
}

uint32 Raid::GetLeaderGuildID()
{
	return currentleaderguildid;
}

bool Raid::IsGuildOfficerInRaidOfGuild(uint32 guild_id)
{
	if (guild_id == GUILD_NONE)
	{
		return false;
	}

	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (members[x].IsGuildOfficer && members[x].guildid == guild_id)
		{
			return true;
		}
	}
	return false;
}

bool Raid::CanRaidEngageRaidTarget(uint32 guild_id)
{
	//Guild system. Always allowed to engage now.
	//bool has_raid_officer = IsGuildOfficerInRaidOfGuild(guild_id);

	//if (!has_raid_officer)
	//	return false;

	//uint32 present_members = GetPresentMembersFromGuildID(guild_id);
	//uint32 raid_count = RaidCount();

	//if (present_members < RuleI(Quarm, AutomatedRaidRotationRaidGuildMemberCountRequirement))
	//	return false;

	//if (raid_count < RuleI(Quarm, AutomatedRaidRotationRaidNonMemberCountRequirement))
	//	return false;

	return true;
}

bool Raid::MeetsInstancedRaidRequirements()
{
	uint32 membercount = 0;

	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (strlen(members[x].membername) > 0)
		{
			uint32 member_level = members[x].level;
			if (member_level < RuleI(Quarm, AutomatedRaidRotationRaidGuildLevelRequirement))
				return false;
			membercount++;
		}
	}

	if (membercount < RuleI(Quarm, AutomatedRaidRotationRaidNonMemberCountRequirement))
		return false;
	
	return true;
}

uint32 Raid::GetGroup(const char *name)
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(members[x].membername, name) == 0)
			return members[x].GroupNumber;
	}
	return 0xFFFFFFFF;
}

uint32 Raid::GetGroup(Client *c)
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member && members[x].member == c)
			return members[x].GroupNumber;
	}
	return 0xFFFFFFFF;
}

void Raid::RaidSay(const char *msg, Client *c, uint8 language, uint8 lang_skill)
{
	if(!c)
		return;

	auto pack = new ServerPacket(ServerOP_RaidSay, sizeof(ServerRaidMessage_Struct) + strlen(msg) + 1);
	ServerRaidMessage_Struct *rga = (ServerRaidMessage_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->gid = 0xFFFFFFFF;
	rga->language = language;
	rga->lang_skill = lang_skill;
	strn0cpy(rga->from, c->GetName(), 64);

	strcpy(rga->message, msg); // this is safe because we are allocating enough space for the entire msg above

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::RaidGroupSay(const char *msg, Client *c, uint8 language, uint8 lang_skill)
{
	if(!c)
		return;

	uint32 groupToUse = GetGroup(c->GetName());

	if(groupToUse >= MAX_RAID_GROUPS)
		return;

	auto pack = new ServerPacket(ServerOP_RaidGroupSay, sizeof(ServerRaidMessage_Struct) + strlen(msg) + 1);
	ServerRaidMessage_Struct *rga = (ServerRaidMessage_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->gid = groupToUse;
	rga->language = language;
	rga->lang_skill = lang_skill;
	strn0cpy(rga->from, c->GetName(), 64);

	strcpy(rga->message, msg); // this is safe because we are allocating enough space for the entire msg above

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

uint32 Raid::GetPlayerIndex(const char *name){
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(name, members[x].membername) == 0){
			return x;
		}
	}
	return -1; //should never get to here if we do everything else right, set it to 0 so we never crash things that rely on it.
}

Client *Raid::GetClientByIndex(uint16 index)
{
	if(index > MAX_RAID_MEMBERS)
		return nullptr;

	return members[index].member;
}

void Raid::CastGroupSpell(Mob* caster, uint16 spellid, uint32 gid, bool isrecourse, int recourse_level)
{
	float range, distance;

	if(!caster)
		return;

	Log(Logs::Detail, Logs::Spells, "%s is casting spell %d on raid %d", caster->GetName(), spellid, GetID());

	range = caster->GetAOERange(spellid);

	float range2 = range*range;

	bool hitcaster = false;
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member && members[x].member == caster) {
			caster->SpellOnTarget(spellid, caster);
			hitcaster = true;
#ifdef GROUP_BUFF_PETS
			if(caster->GetPet() && caster->HasPetAffinity() && !caster->GetPet()->IsCharmedPet())
				caster->SpellOnTarget(spellid, caster->GetPet());
#endif
		}
		else if(members[x].member != nullptr && members[x].member->Connected())
		{
			if(members[x].GroupNumber == gid){
				distance = DistanceSquared(caster->GetPosition(), members[x].member->GetPosition());
				if(distance <= range2){
					caster->SpellOnTarget(spellid, members[x].member);
#ifdef GROUP_BUFF_PETS
					if(members[x].member->GetPet() && members[x].member->HasPetAffinity() && !members[x].member->GetPet()->IsCharmedPet())
						caster->SpellOnTarget(spellid, members[x].member->GetPet());
#endif
				}
				else{
					Log(Logs::Detail, Logs::Spells, "Raid spell: %s is out of range %f at distance %f from %s", members[x].member->GetName(), range, distance, caster->GetName());
				}
			}
		}
	}

	if (!hitcaster && caster->IsClient() && caster->CastToClient()->TGB())
		caster->SpellOnTarget(spellid, caster);
}

int32 Raid::GetTotalRaidDamage(Mob* other)
{
	int32 total = 0;
	for (int i = 0; i < MAX_RAID_MEMBERS; i++) 
	{
		if(!members[i].member)
			continue;

		total += other->GetDamageAmount(members[i].member, true);
	}
	return total;
}

void Raid::HealGroup(uint32 heal_amt, Mob* caster, uint32 gid, float range)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	float distance;
	float range2 = range*range;

	int numMem = 0;
	unsigned int gi = 0;
	for(; gi < MAX_RAID_MEMBERS; gi++)
	{
		if(members[gi].member){
			if(members[gi].GroupNumber == gid)
			{
				distance = DistanceSquared(caster->GetPosition(), members[gi].member->GetPosition());
				if(distance <= range2){
					numMem += 1;
				}
			}
		}
	}

	heal_amt /= numMem;
	for(gi = 0; gi < MAX_RAID_MEMBERS; gi++)
	{
		if(members[gi].member){
			if(members[gi].GroupNumber == gid)
			{
				distance = DistanceSquared(caster->GetPosition(), members[gi].member->GetPosition());
				if(distance <= range2){
					members[gi].member->SetHP(members[gi].member->GetHP() + heal_amt);
					members[gi].member->SendHPUpdate();
				}
			}
		}
	}
}


void Raid::BalanceHP(int32 penalty, uint32 gid, float range, Mob* caster, int32 limit)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	int dmgtaken = 0, numMem = 0, dmgtaken_tmp = 0;
	int gi = 0;

	float distance;
	float range2 = range*range;

	for(; gi < MAX_RAID_MEMBERS; gi++)
	{
		if(members[gi].member){
			if(members[gi].GroupNumber == gid)
			{
				distance = DistanceSquared(caster->GetPosition(), members[gi].member->GetPosition());
				if(distance <= range2){

					dmgtaken_tmp = members[gi].member->GetMaxHP() - members[gi].member->GetHP();
					if (limit && (dmgtaken_tmp > limit))
						dmgtaken_tmp = limit;

					dmgtaken += (dmgtaken_tmp);
					numMem += 1;
				}
			}
		}
	}

	dmgtaken += dmgtaken * penalty / 100;
	dmgtaken /= numMem;
	int old_hp, new_hp;

	for(gi = 0; gi < MAX_RAID_MEMBERS; gi++)
	{
		if(members[gi].member){
			if(members[gi].GroupNumber == gid)
			{
				distance = DistanceSquared(caster->GetPosition(), members[gi].member->GetPosition());
				if(distance <= range2)
				{
					old_hp = members[gi].member->GetHP();
					new_hp = members[gi].member->GetMaxHP() - dmgtaken;
					if (new_hp < 1)
						new_hp = 1;	// can divine arb kill people?

					if (new_hp > old_hp)
					{
						members[gi].member->Message_StringID(Chat::Spells, DIV_ARB_TAKE);
						members[gi].member->Message_StringID(Chat::Spells, YOU_HEALED, itoa(new_hp - old_hp));
					}
					else
						members[gi].member->Message_StringID(Chat::Spells, DIV_ARB_GIVE);

					members[gi].member->SetHP(new_hp);
					members[gi].member->SendHPUpdate();
				}
			}
		}
	}
}

void Raid::BalanceMana(int32 penalty, uint32 gid, float range, Mob* caster, int32 limit)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	float distance;
	float range2 = range*range;

	int manataken = 0, numMem = 0, manataken_tmp = 0;
	int gi = 0;
	for(; gi < MAX_RAID_MEMBERS; gi++)
	{
		if(members[gi].member){
			if(members[gi].GroupNumber == gid)
			{
				if (members[gi].member->GetMaxMana() > 0) {
					distance = DistanceSquared(caster->GetPosition(), members[gi].member->GetPosition());
					if(distance <= range2){

						manataken_tmp = members[gi].member->GetMaxMana() - members[gi].member->GetMana();
						if (limit && (manataken_tmp > limit))
							manataken_tmp = limit;

						manataken += (manataken_tmp);
						numMem += 1;
					}
				}
			}
		}
	}

	manataken += manataken * penalty / 100;
	manataken /= numMem;

	for(gi = 0; gi < MAX_RAID_MEMBERS; gi++)
	{
		if(members[gi].member){
			if(members[gi].GroupNumber == gid)
			{
				distance = DistanceSquared(caster->GetPosition(), members[gi].member->GetPosition());
				if(distance <= range2){
					if((members[gi].member->GetMaxMana() - manataken) < 1){
						members[gi].member->SetMana(1);
						if (members[gi].member->IsClient())
							members[gi].member->CastToClient()->SendManaUpdate();
					}
					else{
						members[gi].member->SetMana(members[gi].member->GetMaxMana() - manataken);
						if (members[gi].member->IsClient())
							members[gi].member->CastToClient()->SendManaUpdate();
					}
				}
			}
		}
	}
}

//basically the same as Group's version just with more people like a lot of non group specific raid stuff
void Raid::SplitMoney(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, Client *splitter){
	//avoid unneeded work
	if(!copper && !silver && !gold && !platinum)
		return;

	uint8 member_count = 0;
	for (uint32 i = 0; i < MAX_RAID_MEMBERS; i++) {
		if (members[i].member) {
			member_count++;
		}
	}

	if (!member_count)
		return;

	uint32 modifier;
	//try to handle round off error a little better
	if(member_count > 1) {
		modifier = platinum % member_count;

		if(modifier) {
			platinum -= modifier;
			gold += 10 * modifier;
		}

		modifier = gold % member_count;

		if(modifier) {
			gold -= modifier;
			silver += 10 * modifier;
		}

		modifier = silver % member_count;

		if(modifier) {
			silver -= modifier;
			copper += 10 * modifier;
		}
	}

	auto copper_split = copper / member_count;
	auto silver_split = silver / member_count;
	auto gold_split = gold / member_count;
	auto platinum_split = platinum / member_count;

	for (uint32 i = 0; i < MAX_RAID_MEMBERS; i++) {
		if (members[i].member) { // If Group Member is Client
		//I could not get MoneyOnCorpse to work, so we use this
		members[i].member->AddMoneyToPP(copper_split, silver_split, gold_split, platinum_split, true);
		members[i].member->Message_StringID(Chat::Green, YOU_RECEIVE_AS_SPLIT, Strings::Money(platinum_split, gold_split, silver_split, copper_split).c_str());
		}
	}
}

void Raid::TeleportGroup(Mob* sender, uint32 zoneID, float x, float y, float z, float heading, uint32 gid)
{
	for(int i = 0; i < MAX_RAID_MEMBERS; i++)
	{
		if(members[i].member)
		{
			if(members[i].GroupNumber == gid)
			{
				members[i].member->MovePC(zoneID, x, y, z, heading, 0, ZoneSolicited);
			}
		}

	}
}

void Raid::TeleportRaid(Mob* sender, uint32 zoneID, float x, float y, float z, float heading)
{
	for(int i = 0; i < MAX_RAID_MEMBERS; i++)
	{
		if(members[i].member)
		{
			members[i].member->MovePC(zoneID, x, y, z, heading, 0, ZoneSolicited);
		}
	}
}

void Raid::ChangeLootType(uint32 type)
{
	std::string query = StringFormat("UPDATE raid_details SET loottype = %lu WHERE raidid = %lu",
                                    (unsigned long)type, (unsigned long)GetID());
    auto results = database.QueryDatabase(query);

	if (LootType == 3 && type <= 1) {
		std::string query = StringFormat("UPDATE raid_members SET islooter = 0 WHERE raidid = '%lu'", (unsigned long)GetID());
		auto results = database.QueryDatabase(query);
	}

	LootType = type;

	auto pack = new ServerPacket(ServerOP_RaidTypeChange, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	rga->looter = type;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::AddRaidLooter(const char* looter)
{
	std::string query = StringFormat("UPDATE raid_members SET islooter = 1 WHERE name = '%s'", looter);
	auto results = database.QueryDatabase(query);

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(looter, members[x].membername) == 0)
		{
			members[x].IsLooter = 1;
			break;
		}
	}
	auto pack = new ServerPacket(ServerOP_RaidAddLooter, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	strn0cpy(rga->playername, looter, 64);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::RemoveRaidLooter(const char* looter)
{
	std::string query = StringFormat("UPDATE raid_members SET islooter = 0 WHERE name = '%s'", looter);
	auto results = database.QueryDatabase(query);

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
		if(strcmp(looter, members[x].membername) == 0) {
			members[x].IsLooter = 0;
			break;
		}

	auto pack = new ServerPacket(ServerOP_RemoveRaidLooter, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->zoneid = zone->GetZoneID();
	rga->zoneguildid = zone->GetGuildID();
	strn0cpy(rga->playername, looter, 64);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

bool Raid::IsRaidLooter(const char* name) {
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (members[x].IsLooter && members[x].membername[0] && strcmp(members[x].membername, name) == 0)
		{
			return true;
		}
	}
	return false;
}

bool Raid::IsRaidLeader(const char* name) {
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (members[x].IsRaidLeader && members[x].membername[0] && strcmp(members[x].membername, name) == 0)
		{
			return true;
		}
	}
	return false;
}

uint32 Raid::GetRaidLeaderCharacterID() 
{
	return leader_charid;
}

bool Raid::IsRaidLeaderOrLooter(const char* name) {
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if ((members[x].IsLooter || members[x].IsRaidLeader) && members[x].membername[0] && strcmp(members[x].membername, name) == 0)
		{
			return true;
		}
	}
	return false;
}

bool Raid::IsRaidLeaderOrGroupLeader(const char* name) {
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if ((members[x].IsGroupLeader || members[x].IsRaidLeader) && members[x].membername[0] && strcmp(members[x].membername, name) == 0)
		{
			return true;
		}
	}
	return false;
}


bool Raid::IsRaidMember(const char *name){
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(name, members[x].membername) == 0)
			return true;
	}
	return false;
}

uint32 Raid::GetHighestLevel()
{
	uint32 highlvl = 0;
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strlen(members[x].membername))
		{
			if(members[x].level > highlvl)
				highlvl = members[x].level;
		}
	}
	return highlvl;
}

uint32 Raid::GetHighestLevel2()
{
	uint32 highlvl = 0;
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (members[x].member != nullptr && members[x].member->IsClient())
		{
			if (members[x].member->CastToClient()->GetLevel2() > highlvl)
				highlvl = members[x].member->CastToClient()->GetLevel2();
		}
	}
	return highlvl;
}

uint32 Raid::GetLowestLevel()
{
	uint32 lowlvl = 1000;
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strlen(members[x].membername))
		{
			if(members[x].level < lowlvl)
				lowlvl = members[x].level;
		}
	}
	return lowlvl;
}

/*
 * Packet Functions Start
 */
void Raid::SendRaidCreate(Client *to){
	if(!to)
		return;

	auto outapp = new EQApplicationPacket(OP_RaidUpdate,sizeof(RaidCreate_Struct));
	RaidCreate_Struct *rc = (RaidCreate_Struct*)outapp->pBuffer;
	rc->action = RaidCommandFormedRaid;
	strn0cpy(rc->leader_name, leadername, 64);
	rc->leader_id = GetID();
	to->QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::SendRaidMembers(Client *to){
	if(!to)
		return;

	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;

	// send out a set loot type 1, this clears array on client for list of looters
	rg->action = RaidCommandSetLootType;
	rg->parameter = 1;
	to->QueuePacket(outapp);
	safe_delete(outapp);

	outapp = new EQApplicationPacket();
	outapp->SetOpcode(OP_RaidUpdate);

	uint8 count = RaidCount();
	int buffsize = 72 * (int)(count + 1) + 652;
	uchar *buff = new uchar[buffsize];
	memset(buff,0,buffsize);
	uchar *bufptr = buff;
	uchar *looters = nullptr;
	uint8 temp = 0;
	int size = 0;

	uint32 raid_command = (uint32)RaidCommandSendMembers;
	memcpy(bufptr, &raid_command, sizeof(uint32));
	bufptr+=4;
	size+=4;

	memcpy(bufptr, &leadername, strlen(leadername));
	bufptr+=strlen(leadername);
	size+=strlen(leadername);
	size++;
	bufptr++;

	uint32 raidid = GetID();
	memcpy(bufptr, &raidid, sizeof(uint32));
	bufptr+=4;
	size+=4;
	
	bufptr+=3;
	temp = count;
	memcpy(bufptr, &temp, sizeof(uint8));
	bufptr++;
	size+=4;

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strlen(members[x].membername) > 0) {
			size += strlen(members[x].membername);
			size += 9;

			// group #
			
			if(members[x].GroupNumber == 0xFFFFFFFF) {
				memcpy(bufptr, &members[x].GroupNumber, sizeof(uint32));
				bufptr+=4;
			} else {
				bufptr+=3;
				temp = (uint8)members[x].GroupNumber;			
				memcpy(bufptr, &temp, sizeof(uint8));
				bufptr+=1;
			}

			// member name
			memcpy(bufptr, &members[x].membername, strlen(members[x].membername));
			bufptr+=strlen(members[x].membername);
			bufptr++; // null terminator

			// class
			memcpy(bufptr, &members[x]._class, sizeof(uint8));
			bufptr++;
			// level
			memcpy(bufptr, &members[x].level, sizeof(uint8));
			bufptr++;

			// not sure what this is
			temp = 0;
			memcpy(bufptr, &temp, sizeof(uint8));
			bufptr++;

			// if group leader, this is 1
			if (members[x].IsGroupLeader) {
				temp = 1;
				memcpy(bufptr, &temp, sizeof(uint8));
			}
			bufptr++;
		}
	}
	// loot type
	bufptr+=3;
	temp = (uint8)GetLootType();
	memcpy(bufptr, &temp, sizeof(uint8));
	bufptr+=1;

	// number of looters
	bufptr+=3;
	looters = bufptr;

	// mark the place to put number of looters
	temp = 0;
	bufptr+=1;

	// loot type & # looters
	size+=8;
	// list of looters
	if (GetLootType() == 3) {
		temp = 0;
		for(int x = 0; x < MAX_RAID_MEMBERS && temp < 9; x++)
		{
			if(strlen(members[x].membername) > 0) {
				if (members[x].IsLooter && !members[x].IsRaidLeader) {
					temp++;
					memcpy(bufptr, &members[x].membername, strlen(members[x].membername));
					size+=strlen(members[x].membername)+1;
					bufptr+=strlen(members[x].membername)+1;
				}
			}
		}
		// copy number of looters
		if (temp > 0)
			memcpy(looters, &temp, sizeof(uint8));
	}
	// raid leader after the of list of looters
	memcpy(bufptr, &leadername, strlen(leadername));
	bufptr+=strlen(leadername)+1;
	size+=strlen(leadername)+1;

	outapp->pBuffer = new uchar[size];
	memcpy(outapp->pBuffer, buff, size);
	safe_delete_array(buff);
	outapp->size = size;
	to->QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::SendRaidAddAll(const char *who, Client *skip)
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(members[x].membername, who) == 0)
		{
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidAddMember_Struct));
			RaidAddMember_Struct *ram = (RaidAddMember_Struct*)outapp->pBuffer;
			ram->raidGen.action = RaidCommandInviteIntoExisting;
			ram->raidGen.parameter = members[x].GroupNumber;
			strcpy(ram->raidGen.leader_name, members[x].membername);
			strcpy(ram->raidGen.player_name, members[x].membername);
			ram->_class = members[x]._class;
			ram->level = members[x].level;
			ram->isGroupLeader = members[x].IsGroupLeader;
			this->QueuePacket(outapp, skip);
			safe_delete(outapp);
			return;
		}
	}
}

void Raid::SendRaidRemoveAll(const char *who, Client *skip)
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strcmp(members[x].membername, who) == 0)
		{
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandRemoveMember;
			strn0cpy(rg->leader_name, who, 64);
			strn0cpy(rg->player_name, who, 64);
			rg->parameter = 0;
			QueuePacket(outapp, skip);
			safe_delete(outapp);
			return;
		}
	}
}

void Raid::SendRaidDisband(Client *to)
{
	if(!to)
		return;

	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
	rg->action = RaidCommandRemoveMember;
	strn0cpy(rg->leader_name, to->GetName(), 64);
	strn0cpy(rg->player_name, to->GetName(), 64);
	rg->parameter = 0;
	to->QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::SendRaidDisbandAll()
{
	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
	rg->action = RaidCommandRaidDisband;
	strn0cpy(rg->leader_name, "RaidMember", 64);
	strn0cpy(rg->player_name, "RaidMember", 64);
	rg->parameter = 0;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::SendRaidChangeGroup(const char* name, uint32 gid)
{
	Client *c = entity_list.GetClientByName(name);
	if (c && gid == 0xFFFFFFFF)
		SendGroupDisband(c);
	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
	rg->action = raidCommandChangeRaidGroup;
	strcpy(rg->leader_name, name);
	strcpy(rg->player_name, name);
	rg->parameter = gid;
	this->QueuePacket(outapp);
	safe_delete(outapp);

}

void Raid::QueuePacket(const EQApplicationPacket *app, Client *skip, bool ack_req)
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member && members[x].member->Connected() && members[x].member != skip)
		{
			members[x].member->QueuePacket(app, ack_req);
		}
	}
}

void Raid::SendMakeLeaderPacket(const char *who)
{
	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
	rg->action = RaidCommandChangeRaidLeader;
	strn0cpy(rg->leader_name, who, 64);
	strn0cpy(rg->player_name, who, 64);
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::SendMakeLeaderPacketTo(const char *who, Client *to)
{
	if(!to)
		return;

	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
	rg->action = RaidCommandChangeRaidLeader;
	strn0cpy(rg->leader_name, who, 64);
	strn0cpy(rg->player_name, who, 64);
	to->QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::SendMakeGroupLeaderPacket(const char *who, uint32 gid) //13
{
	auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
	RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
	rg->action = RaidCommandChangeGroupLeader;
	strn0cpy(rg->leader_name, who, 64);
	strn0cpy(rg->player_name, who, 64);
	rg->parameter = gid;
	QueuePacket(outapp);
	safe_delete(outapp);

	outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
	gj->action = groupActMakeLeader;
	strcpy(gj->membername, who);
					//strcpy(gj->yourname, name);

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member)
		{
			if(strlen(members[x].membername) > 0 && members[x].member && (members[x].GroupNumber == gid))
			{
				members[x].member->QueuePacket(outapp);
			}
		}
	}
	safe_delete(outapp);
}

void Raid::SendGroupUpdate(Client *to)
{
	if(!to)
		return;

	auto outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupUpdate_Struct));
	GroupUpdate_Struct* gu = (GroupUpdate_Struct*)outapp->pBuffer;
	gu->action = groupActUpdate;
	int index = 0;
	uint32 grp = GetGroup(to->GetName());
	if(grp >= MAX_RAID_GROUPS)
	{
		safe_delete(outapp);
		return;
	}
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].GroupNumber == grp && strlen(members[x].membername) > 0)
		{
			if(members[x].IsGroupLeader){
				strn0cpy(gu->leadersname, members[x].membername, 64);
				if(strcmp(to->GetName(), members[x].membername) != 0){
					strn0cpy(gu->membername[index], members[x].membername, 64);
					index++;
				}
			}
			else{
				if(strcmp(to->GetName(), members[x].membername) != 0){
					strn0cpy(gu->membername[index], members[x].membername, 64);
					index++;
				}
			}
		}
	}
	if(strlen(gu->leadersname) < 1){
		strn0cpy(gu->leadersname, to->GetName(), 64);
	}
	strn0cpy(gu->yourname, to->GetName(), 64);

	to->FastQueuePacket(&outapp);
}

void Raid::SendGroupLeader(uint32 gid, Client *client) 
{
	if (!client)
		return;

	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gu = (GroupJoin_Struct*)outapp->pBuffer;
	gu->action = groupActMakeLeader;

	strcpy(gu->yourname, client->GetName());

	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (members[x].GroupNumber == gid && strlen(members[x].membername) > 0)
		{
			if (members[x].IsGroupLeader) 
			{
				strn0cpy(gu->membername, members[x].membername, 64);
				break;
			}
		}
	}
	if (strlen(gu->membername) < 1) {
		strn0cpy(gu->membername, client->GetName(), 64);
	}

	client->QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::GroupUpdate(uint32 gid, bool initial)
{
	if(gid > 11) //ungrouped member doesn't need grouping.
		return;
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strlen(members[x].membername) > 0){
			if(members[x].GroupNumber == gid){
				if(members[x].member)
					// this updates group window
					SendGroupUpdate(members[x].member);
			}
		}
	}
	if(initial){
		auto pack = new ServerPacket(ServerOP_UpdateGroup, sizeof(ServerRaidGeneralAction_Struct));
		ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
		rga->gid = gid;
		rga->rid = GetID();
		rga->zoneid = zone->GetZoneID();
		rga->zoneguildid = zone->GetGuildID();
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Raid::GroupJoin(const char *who, uint32 gid, Client* exclude, bool initial)
{
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (strlen(members[x].membername) > 0) 
		{
			if (members[x].GroupNumber == gid) 
			{
				if (members[x].member && members[x].member != exclude)
					// this updates group window
					SendGroupJoin(members[x].member, who);
			}
		}
	}

	if (initial)
	{
		auto pack = new ServerPacket(ServerOP_RaidGroupJoin, sizeof(ServerRaidGroupJoin_Struct));
		ServerRaidGroupJoin_Struct* sgj = (ServerRaidGroupJoin_Struct*)pack->pBuffer;
		sgj->gid = gid;
		sgj->rid = GetID();
		sgj->zoneid = zone->GetZoneID();
		sgj->zoneguildid = zone->GetGuildID();
		strcpy(sgj->member_name, who);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Raid::SendGroupJoin(Client* client, const char *who)
{
	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gj = (GroupJoin_Struct*)outapp->pBuffer;
	strcpy(gj->membername, who);
	gj->action = groupActJoin;
	strcpy(gj->yourname, client->GetName());
	client->QueuePacket(outapp);
	safe_delete(outapp);
}

void Raid::UpdateGuildRank(Client* update)
{
	if (!update)
		return;


	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (strcmp(update->GetCleanName(), members[x].membername) == 0)
		{
			if (update->GuildID() != GUILD_NONE)
				members[x].IsGuildOfficer = update->GuildRank() >= 1;
			else
				members[x].IsGuildOfficer = 0;
		}
	}
}

void Raid::UpdatePlayer(Client* update) 
{

	if (!update)
		return;

	uint32 grp = GetGroup(update->GetName());
	//update their player profile
	PlayerProfile_Struct &pp = update->GetPP();
	int index = 0;
	for (int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if (members[x].GroupNumber == grp)
		{
			if (strlen(members[x].membername) > 0)
			{
				strn0cpy(pp.groupMembers[index], members[x].membername, 64);
				members[x].IsGuildOfficer = update->GuildRank() >= 1;
				++index;
			}
			else
			{
				memset(pp.groupMembers[index], 0, 64);
				++index;
			}

			if (index >= MAX_GROUP_MEMBERS)
				return;
		}
	}

	return;
}

void Raid::SendGroupDisband(Client *to)
{
	if(!to)
		return;
	to->SetRaidGrouped(false);
	auto outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupGeneric_Struct2));
	GroupGeneric_Struct2* gu = (GroupGeneric_Struct2*) outapp->pBuffer;
	gu->action = groupActDisband2;
	strn0cpy(gu->membername, to->GetName(), 64);
	strn0cpy(gu->yourname, to->GetName(), 64);
	gu->param = GROUP_REMOVED;
	to->FastQueuePacket(&outapp);
}

void Raid::SendGroupLeave(const char* who, uint32 gid)
{
	auto outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupGeneric_Struct2));
	GroupGeneric_Struct2* gj = (GroupGeneric_Struct2*) outapp->pBuffer;
	strcpy(gj->membername, who);
	gj->action = groupActLeave;

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].GroupNumber == gid && strlen(members[x].membername) > 0 && members[x].member && strcmp(who, members[x].membername) != 0)
		{
			//strcpy(gj->yourname, members[x].membername);
			members[x].member->QueuePacket(outapp);
		}
	}
	safe_delete(outapp);
}

void Raid::SendGroupLeave(Client *to)
{
	if(!to)
		return;

	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupUpdate_Struct));
	GroupUpdate_Struct* gu = (GroupUpdate_Struct*) outapp->pBuffer;
	gu->action = groupActLeave;
	strn0cpy(gu->leadersname, leadername, 64);
	strn0cpy(gu->yourname, to->GetName(), 64);
	to->FastQueuePacket(&outapp);
}

void Raid::SendRaidGroupAdd(const char *who, uint32 gid)
{
	auto pack = new ServerPacket(ServerOP_RaidGroupAdd, sizeof(ServerRaidGroupAction_Struct));
	ServerRaidGroupAction_Struct * rga = (ServerRaidGroupAction_Struct*)pack->pBuffer;
	rga->rid = GetID();
	rga->gid = gid;
	strn0cpy(rga->membername, who, 64);
	safe_delete(pack);
}

void Raid::SendRaidGroupRemove(const char *who, uint32 gid, bool skip_removed)
{
	Client *c = entity_list.GetClientByName(who);
	if (c) {
		if (!skip_removed)
		{
			SendGroupDisband(c);
		}
		c->SetRaidGrouped(false);
	}
	SendGroupLeave(who, gid);
		
	auto pack = new ServerPacket(ServerOP_RaidGroupRemove, sizeof(ServerRaidGeneralAction_Struct));
	ServerRaidGeneralAction_Struct * rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
	rga->zoneid = zone->GetZoneID();
	rga->rid = GetID();
	rga->gid = gid;
	rga->zoneguildid = zone->GetGuildID();
	strn0cpy(rga->playername, who, 64);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Raid::SetRaidDetails(uint32 character_id)
{
	std::string query = StringFormat("INSERT INTO raid_details SET raidid = %lu, loottype = 1, locked = 0, leader_charid = %lu",
                                    (unsigned long)GetID(), character_id);
    auto results = database.QueryDatabase(query);
}

void Raid::GetRaidDetails()
{
	std::string query = StringFormat("SELECT loottype, leader_charid FROM raid_details WHERE raidid = %lu",
		(unsigned long)GetID());
	auto results = database.QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::General, Logs::Error, "Error getting raid details for raid %lu: %s", (unsigned long)GetID(), results.ErrorMessage().c_str());
		return;
	}

	if (results.RowCount() == 0) {
		return;
	}

	auto row = results.begin();

	LootType = atoi(row[0]);
	leader_charid = atoi(row[1]);
}

bool Raid::LearnMembers()
{
	memset(members, 0, (sizeof(RaidMember)*MAX_RAID_MEMBERS));

	std::string query = StringFormat("SELECT name, groupid, _class, level, "
                                    "isgroupleader, israidleader, islooter, guild_id, is_officer "
                                    "FROM raid_members WHERE raidid = %lu",
                                    (unsigned long)GetID());
    auto results = database.QueryDatabase(query);
	if (!results.Success())
	{
		Log(Logs::General, Logs::Error, "Error getting raid members for raid %lu: %s", (unsigned long)GetID(), results.ErrorMessage().c_str());
		return false;
	}

	if(results.RowCount() == 0) {
        disbandCheck = true;
        return false;
    }

    int index = 0;
    for(auto row = results.begin(); row != results.end(); ++row) {
        if(!row[0])
            continue;

        members[index].member = nullptr;
        strn0cpy(members[index].membername, row[0], 64);
        int groupNum = atoi(row[1]);
        if(groupNum > 11)
            members[index].GroupNumber = 0xFFFFFFFF;
        else
            members[index].GroupNumber = groupNum;

        members[index]._class = atoi(row[2]);
        members[index].level = atoi(row[3]);
        members[index].IsGroupLeader = atoi(row[4]);
        members[index].IsRaidLeader = atoi(row[5]);
        members[index].IsLooter = atoi(row[6]);
		members[index].guildid = atoi(row[7]);
		members[index].IsGuildOfficer = atoi(row[8]);
        ++index;
    }

	return true;
}

void Raid::VerifyRaid()
{
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(strlen(members[x].membername) == 0){
			members[x].member = nullptr;
		}
		else{
			Client *c = entity_list.GetClientByName(members[x].membername);
			if(c){
				members[x].member = c;
			}
			else{
				members[x].member = nullptr;
			}
		}
		if(members[x].IsRaidLeader)
		{
			if(strlen(members[x].membername) > 0){
				SetLeader(members[x].member);
				currentleaderguildid = members[x].guildid;
				strn0cpy(leadername, members[x].membername, 64);
			}
		}
	}

	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		raid_engage_check_result = CanRaidEngageRaidTarget(zone->GetGuildID());
	}

}

void Raid::MemberZoned(Client *c)
{
	if(!c)
		return;

	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member && members[x].member == c)
		{
			members[x].member = nullptr;
		}
	}
}

void Raid::SendHPPacketsTo(Client *c)
{
	if(!c)
		return;

	if (!c->Connected())
		return;

	uint32 gid = this->GetGroup(c);
	EQApplicationPacket hpapp;
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member && members[x].member->Connected())
		{
			if((members[x].member != c) && (members[x].GroupNumber == gid))
			{
				members[x].member->CreateHPPacket(&hpapp);
				c->QueuePacket(&hpapp, false);
				safe_delete_array(hpapp.pBuffer);
				hpapp.size = 0;
			}
		}
	}
}

void Raid::SendHPPacketsFrom(Mob *m)
{
	if(!m)
		return;

	if (m->IsClient() && !m->CastToClient()->Connected())
		return;

	uint32 gid = 0;
	if(m->IsClient())
		gid = this->GetGroup(m->CastToClient());
	EQApplicationPacket hpapp;

	m->CreateHPPacket(&hpapp);
	for(int x = 0; x < MAX_RAID_MEMBERS; x++)
	{
		if(members[x].member && members[x].member->Connected())
		{
			if(!m->IsClient() || ((members[x].member != m->CastToClient()) && (members[x].GroupNumber == gid)))
			{
				members[x].member->QueuePacket(&hpapp, true);
			}
		}
	}
}

uint16 Raid::GetAvgLevel()
{
	double levelHolder = 0;
	uint8 i = 0;
	uint8 numMem = 0;
	while(i < MAX_RAID_MEMBERS)
	{
		if(strlen(members[i].membername))
		{
			levelHolder = levelHolder + members[i].level;
			numMem++;
		}
		i++;
	}
	levelHolder = ((levelHolder/(numMem))+.5); // total levels divided by num of characters
	return (uint16(levelHolder));
}

const char *Raid::GetClientNameByIndex(uint8 index)
{
	return members[index].membername;
}

void Raid::RaidMessage_StringID(Mob* sender, uint32 type, uint32 string_id, const char* message,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9, uint32 distance) {
	uint32 i;
	for (i = 0; i < MAX_RAID_MEMBERS; i++) {
		if(members[i].member && members[i].member->Connected()) {
			if(members[i].member != sender)
				members[i].member->Message_StringID(type, string_id, message, message2, message3, message4, message5, message6, message7, message8, message9, distance);
		}
	}
}
