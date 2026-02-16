#include "../client.h"
#include "../raids.h"
#include "../worldserver.h"
#include "../../common/servertalk.h"

extern WorldServer worldserver;

// rate limit
static std::map<uint32, time_t> raidmove_cooldowns;
static const int RAIDMOVE_COOLDOWN_SECONDS = 1;

void command_raidmove(Client *c, const Seperator *sep)
{
	uint32 char_id = c->CharacterID();
	time_t now = time(nullptr);
	auto it = raidmove_cooldowns.find(char_id);
	if (it != raidmove_cooldowns.end()) {
		time_t elapsed = now - it->second;
		if (elapsed < RAIDMOVE_COOLDOWN_SECONDS) {
			c->Message(Chat::Red, "Please wait %d second(s) before sending another raid move.",
				(int)(RAIDMOVE_COOLDOWN_SECONDS - elapsed));
			return;
		}
	}

	if (!sep->arg[1][0] || !sep->arg[2][0]) {
		c->Message(Chat::White, "Usage: #raidmove [playername] [groupnumber]");
		c->Message(Chat::White, "Moves a raid member to a different group.");
		c->Message(Chat::White, "Group number 1-12, or 0 for ungrouped.");
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

	Raid *r = c->GetRaid();

	if (!r) {
		c->Message(Chat::Red, "You must be in a raid to use this command.");
		return;
	}

	if (!r->IsRaidLeader(c->GetName())) {
		c->Message(Chat::Red, "You must be the raid leader to use this command.");
		return;
	}

	std::string target_name = sep->arg[1];
	char* end;
    long group_input = std::strtol(sep->arg[2], &end, 10);

    if (end == sep->arg[2] || *end != '\0') {
        c->Message(Chat::Red, "Group number is invalid.");
		return;
    } else if (group_input < 0 || group_input > 12) {
        c->Message(Chat::Red, "Group number must be 0 (ungrouped) or 1-%d.", MAX_RAID_GROUPS);
		return;
    }
	
	// convert user input to internal group number
	uint32 new_group = (group_input == 0) ? 0xFFFFFFFF : (group_input - 1);

	uint32 player_index = 0xFFFFFFFF;
	for (int i = 0; i < MAX_RAID_MEMBERS; i++)
	{
		if (strlen(r->members[i].membername) > 0 && strcasecmp(target_name.c_str(), r->members[i].membername) == 0) {
			player_index = i;
			target_name = r->members[i].membername;
			break;
		}
	}

	if (player_index == 0xFFFFFFFF)
	{
		c->Message(Chat::Red, "%s is not in your raid.", target_name.c_str());
		return;
	}

	uint32 old_group = r->members[player_index].GroupNumber;

	if (old_group == new_group)
	{
		if (new_group == 0xFFFFFFFF) {
			c->Message(Chat::Red, "%s is already ungrouped.", target_name.c_str());
		} else {
			c->Message(Chat::Red, "%s is already in group %d.", target_name.c_str(), group_input);
		}
		return;
	}

	if (new_group != 0xFFFFFFFF) {
		uint8 group_count = r->GroupCount(new_group);
		if (group_count >= MAX_GROUP_MEMBERS) {
			c->Message(Chat::Red, "Group %d is full.", group_input);
			return;
		}
	}

	if (!worldserver.Connected()) {
		c->Message(Chat::Red, "World server is not connected.");
		return;
	}

	auto pack = new ServerPacket(ServerOP_RaidMove, sizeof(ServerRaidMove_Struct));
	ServerRaidMove_Struct* srm = (ServerRaidMove_Struct*)pack->pBuffer;
	strn0cpy(srm->requester_name, c->GetName(), 64);
	strn0cpy(srm->target_name, target_name.c_str(), 64);
	srm->rid = r->GetID();
	srm->old_group = old_group;
	srm->new_group = new_group;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	raidmove_cooldowns[char_id] = now;

	c->Message(Chat::White, "Sending move request for %s...", target_name.c_str());
}
