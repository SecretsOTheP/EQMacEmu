#include "../client.h"
#include "../worldserver.h"
#include "../raids.h"
#include "../guild_mgr.h"
#include "../../common/servertalk.h"
#include "../../common/guilds.h"
#include <map>
#include <ctime>

extern WorldServer worldserver;

// rate limit
static std::map<uint32, time_t> raidinvite_cooldowns;
static const int RAIDINVITE_COOLDOWN_SECONDS = 2;

void command_raidinvite(Client *c, const Seperator *sep)
{
	uint32 char_id = c->CharacterID();
	time_t now = time(nullptr);
	auto it = raidinvite_cooldowns.find(char_id);
	if (it != raidinvite_cooldowns.end()) {
		time_t elapsed = now - it->second;
		if (elapsed < RAIDINVITE_COOLDOWN_SECONDS) {
			c->Message(Chat::Red, "Please wait %d second(s) before sending another raid invite.",
				(int)(RAIDINVITE_COOLDOWN_SECONDS - elapsed));
			return;
		}
	}

	if (!sep->arg[1][0]) {
		c->Message(Chat::White, "Usage: #raidinvite [playername] [group]");
		c->Message(Chat::White, "Sends a cross-zone raid invite to a player by name.");
		c->Message(Chat::White, "Optional: Specify group 1-12 to invite them as leader of that group.");
		c->Message(Chat::White, "The invitee must type #raidaccept to join.");
		return;
	}

	std::string invitee_name = sep->arg[1];

	// parse optional group number
	uint32 requested_group = 0xFFFFFFFF;  // ungrouped by default
	if (sep->arg[2][0]) {
		int group_num = atoi(sep->arg[2]);
		if (group_num >= 1 && group_num <= 12) {
			requested_group = group_num - 1;
		}
		else {
			c->Message(Chat::Red, "Group number must be between 1 and 12.");
			return;
		}
	}

	if (strcasecmp(c->GetName(), invitee_name.c_str()) == 0) {
		c->Message(Chat::Red, "You cannot invite yourself to a raid.");
		return;
	}

	if (!c->IsInAGuild()) {
		c->Message(Chat::Red, "You must be in a guild with raiding enabled to use this command.");
		return;
	}

	if (!guild_mgr.IsGuildRaidEnabled(c->GuildID())) {
		c->Message(Chat::Red, "You must be in a guild with raiding enabled to use this command.");
		return;
	}

	if (c->GuildRank() < GUILD_OFFICER) {
		c->Message(Chat::Red, "You must be an officer or leader of your guild to use this command.");
		return;
	}

	Raid* raid = c->GetRaid();

	if (raid) {
		if (!raid->IsRaidLeader(c->GetName())) {
			c->Message(Chat::Red, "You must be the raid leader to use this command.");
			return;
		}
		else {
			if (requested_group != 0xFFFFFFFF) {
				if (raid->GroupCount(requested_group) != 0) {
					c->Message(Chat::Red, "Group %d already has a leader.", requested_group+1);
					return;
				}
			}
		}
	}
	else {
		if (requested_group == 0) {
			c->Message(Chat::Red, "You cannot form a raid with invitee as leader of group %d", requested_group+1);
			return;
		}
	}

	if (!worldserver.Connected()) {
		c->Message(Chat::Red, "Cannot send cross-zone invite: world server is not connected.");
		return;
	}

	ChallengeRules::RuleSet raidGroupType = raid ? raid->GetRuleSet() : c->GetRuleSet();

	auto pack = new ServerPacket(ServerOP_RaidInvite, sizeof(ServerRaidInvite_Struct));
	ServerRaidInvite_Struct* sris = (ServerRaidInvite_Struct*)pack->pBuffer;
	strn0cpy(sris->inviter_name, c->GetName(), 64);
	strn0cpy(sris->invitee_name, invitee_name.c_str(), 64);
	sris->rid = raid ? raid->GetID() : 0;
	sris->invite_id = 0;
	sris->raid_ruleset = raidGroupType;
	sris->is_acceptance = false;
	sris->requested_group = requested_group;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	raidinvite_cooldowns[char_id] = now;

	if (requested_group != 0xFFFFFFFF) {
		c->Message(Chat::White, "Cross-zone raid invite sent to %s to lead group %d.", invitee_name.c_str(), requested_group + 1);
	} else {
		c->Message(Chat::White, "Cross-zone raid invite sent to %s.", invitee_name.c_str());
	}

	c->Message(Chat::White, "They will need to type #raidaccept to join.");

	if (!raid) {
		c->Message(Chat::White, " A raid will be formed when they accept.");
	}
}
