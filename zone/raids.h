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
#ifndef RAIDS_H
#define RAIDS_H

#include "../common/types.h"
#include "groups.h"

class Client;
class EQApplicationPacket;
class Mob;

// raid command types
// some are incoming and outgoing, some do different things incoming and outgoing.
enum {
	raidCommandChangeRaidGroup = 3, // This changes position of a raid member in the raid list - param is group number to move into a group, or -1, to move them out of a group
};

enum { 
	RaidCommandInviteIntoExisting = 0, //in use
	RaidCommandRemoveMember = 1,
	RaidCommandRenameMember = 2,
	RaidCommandInvite = 3, //in use
	RaidCommandAcceptInvite = 4, //in use
	RaidCommandRaidDisband = 5, // 5 ?
	RaidCommandSendMembers = 6, //in use - send to people when they join the raid
	RaidCommandNoLeadershipAssigned = 7,
	RaidCommandFormedRaid = 8, // eqmac - send to leader when they form a raid
	RaidCommandSendDisband = 10,
	RaidCommandLootTypeResponse = 11,
	RaidCommandRaidMessage = 12,
	RaidCommandChangeGroupLeader = 13,
	RaidCommandChangeRaidLeader = 20, //in use
	RaidCommandDeclineInvite = 21, // eqmac confirmed - decline or rejected
	RaidCommandSetLootType = 22, //
	RaidCommandAddLooter = 23, // eqmac confirmed
	RaidCommandRemoveLooter = 24, //in use
};

#define MAX_RAID_GROUPS 12
#define MAX_RAID_MEMBERS 72

struct RaidMember{
	char membername[64];
	Client *member;
	uint32 GroupNumber;
	uint32 guildid;
	uint8 _class;
	uint8 level;
	bool IsGroupLeader;
	bool IsGuildOfficer;
	bool IsRaidLeader;
	bool IsLooter;
};

class Raid : public GroupIDConsumer {
public:
	Raid(Client *nLeader);
	Raid(uint32 raidID);
	~Raid();

	void SetLeader(Client *newLeader) { leader = newLeader; }
	Client* GetLeader() { return leader; }
	bool IsLeader(Client *c) { return leader==c; }
	bool IsLeader(const char* name) { return (strcmp(leadername, name)==0); }
	void SetRaidLeader(const char *wasLead, const char *name);
	uint32 GetLeaderGuildID();
	bool CanRaidEnterPersonalInstance();
	inline bool GetEngageCachedResult() { return raid_engage_check_result;	}

	bool	Process();
	bool	IsRaid() { return true; }

	void	AddMember(Client *c, uint32 group = 0xFFFFFFFF, bool rleader=false, bool groupleader=false, bool looter=false, bool skip_new_member=false);
	void	AddGroupToRaid(Client* inviter, Client* invitee, Group* group, uint32 freegroup_id);
	void	RemoveMember(const char *c);
	void	DisbandRaid();
	void	DisbandRaidMember(const char *name, Client *who = nullptr);
	void	MoveMember(const char *name, uint32 newGroup);
	void	SetGroupLeader(const char *who, uint32 gid, bool flag = true);
	void	UnSetGroupLeader(const char *who, const char *other, uint32 gid);
	bool	IsGroupLeader(const char *who);
	bool	IsRaidLeaderOrGroupLeader(const char * name);
	bool	IsRaidMember(const char *name);
	void	UpdateLevel(const char *name, int newLevel);
	void	UpdatePlayer(Client* update);

	uint32	GetFreeGroup();
	uint8	GroupCount(uint32 gid);
	uint8	RaidCount();
	uint32	GetPresentMembersFromGuildID(uint32 guild_id);
	bool	IsGuildOfficerInRaidOfGuild(uint32 guild_id);
	bool	CanRaidEngageRaidTarget(uint32 guild_id);
	bool MeetsInstancedRaidRequirements();
	uint32	GetHighestLevel();
	uint32	GetHighestLevel2();
	uint32	GetLowestLevel();
	uint32	GetGroup(const char *name);
	uint32	GetGroup(Client *c);
	uint16	GetAvgLevel();

	uint32	GetRaidLeaderCharacterID();
	inline void	SetRaidLeaderCharacterID(uint32 charid) { leader_charid = charid; }

	uint32	GetLootType() { return LootType; }
	void	ChangeLootType(uint32 type);
	void	AddRaidLooter(const char* looter);
	void	RemoveRaidLooter(const char* looter);
	bool	IsRaidLooter(const char* name);
	bool	IsRaidLeader(const char* name);
	bool	IsRaidLeaderOrLooter(const char* name);

	//util func
	//keeps me from having to keep iterating through the list
	//when I want lots of data from the same entry
	uint32	GetPlayerIndex(const char *name);
	//for perl interface
	Client *GetClientByIndex(uint16 index);
	const char *GetClientNameByIndex(uint8 index);

	//Actual Implementation Stuff

	void	RaidMessage_StringID(Mob* sender, uint32 type, uint32 string_id, const char* message,const char* message2=0,const char* message3=0,const char* message4=0,const char* message5=0,const char* message6=0,const char* message7=0,const char* message8=0,const char* message9=0, uint32 distance = 0);
	void	CastGroupSpell(Mob* caster,uint16 spellid, uint32 gid, bool isrecourse=false, int recourse_level=-1);
	void	SplitExp(uint32 exp, Mob* killed_mob);
	int32	GetTotalRaidDamage(Mob* other);
	void	BalanceHP(int32 penalty, uint32 gid, float range = 0, Mob* caster = nullptr, int32 limit = 0);
	void	BalanceMana(int32 penalty, uint32 gid,  float range = 0, Mob* caster = nullptr, int32 limit = 0);
	void	HealGroup(uint32 heal_amt, Mob* caster, uint32 gid, float range = 0);
	void	SplitMoney(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, Client *splitter = nullptr);

	void	TeleportGroup(Mob* sender, uint32 zoneID, float x, float y, float z, float heading, uint32 gid);
	void	TeleportRaid(Mob* sender, uint32 zoneID, float x, float y, float z, float heading);

	//updates the list of Client* objects based on who's in and not in the zone.
	//also learns raid structure based on db.
	void	SetRaidDetails(uint32 character_id);
	void	GetRaidDetails();
	bool	LearnMembers();
	void	VerifyRaid();
	void	MemberZoned(Client *c);
	void	SendHPPacketsTo(Client *c);
	void	SendHPPacketsFrom(Mob *m);
	void	RaidSay(const char *msg, Client *c, uint8 language, uint8 lang_skill);
	void	RaidGroupSay(const char *msg, Client *c, uint8 language, uint8 lang_skill);

	//Packet Functions
	void	SendRaidCreate(Client *to);
	void	SendRaidMembers(Client *to);
	void	SendRaidAddAll(const char *who, Client *skip = nullptr);
	void	SendRaidRemoveAll(const char *who, Client *skip = nullptr);
	void	SendRaidDisband(Client *to);
	void	SendRaidDisbandAll();
	void	SendRaidChangeGroup(const char* who, uint32 gid);

	void	GroupUpdate(uint32 gid, bool initial = true);
	void	GroupJoin(const char *who, uint32 gid, Client* exclude = nullptr, bool initial = false);
	void	SendGroupJoin(Client* to, const char *who);
	void	UpdateGuildRank(Client * update);
	void	SendGroupUpdate(Client *to);
	void	SendGroupLeader(uint32 gid, Client *to);
	void	SendGroupDisband(Client *to);
	void	SendGroupLeave(Client *to);
	void	SendGroupLeave(const char *who, uint32 gid);
	void	SendRaidGroupAdd(const char *who, uint32 gid = 0xFFFFFFFF);
	void	SendRaidGroupRemove(const char *who, uint32 gid = 0xFFFFFFFF, bool skip_removed = false);
	void	SendMakeLeaderPacket(const char *who); //30
	void	SendMakeLeaderPacketTo(const char *who, Client *to);
	void	SendMakeGroupLeaderPacket(const char *who, uint32 gid = 0xFFFFFFFF); //13

	void	QueuePacket(const EQApplicationPacket *app, Client *skip = nullptr, bool ack_req = true);

	RaidMember members[MAX_RAID_MEMBERS];
	char leadername[64];
	uint32 currentleaderguildid;

protected:
	Client *leader;
	uint32 leader_charid;
	uint32 LootType;
	bool disbandCheck;
	bool forceDisband;
	bool raid_engage_check_result;
};


#endif

