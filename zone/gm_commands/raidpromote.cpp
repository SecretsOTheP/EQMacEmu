#include "../client.h"
#include "../raids.h"
#include "../worldserver.h"
#include "../../common/servertalk.h"

extern WorldServer worldserver;

// rate limit
static std::map<uint32, time_t> raidpromote_cooldowns;
static const int RAIDPROMOTE_COOLDOWN_SECONDS = 1;

void command_raidpromote(Client *c, const Seperator *sep)
{
	uint32 char_id = c->CharacterID();
	time_t now = time(nullptr);
	auto it = raidpromote_cooldowns.find(char_id);
	if (it != raidpromote_cooldowns.end()) {
		time_t elapsed = now - it->second;
		if (elapsed < RAIDPROMOTE_COOLDOWN_SECONDS) {
			c->Message(Chat::Red, "Please wait %d second(s) before sending another raid promote.",
				(int)(RAIDPROMOTE_COOLDOWN_SECONDS - elapsed));
			return;
		}
	}

	if (!sep->arg[1][0]) {
		c->Message(Chat::White, "Usage: #raidpromote [playername]");
		c->Message(Chat::White, "Promotes a raid member to leader of their group.");
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

	uint32 player_index = 0xFFFFFFFF;
	for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
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

	uint32 gid = r->members[player_index].GroupNumber;
	if (gid == 0xFFFFFFFF)
	{
		c->Message(Chat::Red, "%s is not in a raid group.", target_name.c_str());
		return;
	}

	if (r->members[player_index].IsGroupLeader)
	{
		c->Message(Chat::Red, "%s is already the group leader.", target_name.c_str());
		return;
	}

	if (!worldserver.Connected()) {
		c->Message(Chat::Red, "World server is not connected.");
		return;
	}

	auto pack = new ServerPacket(ServerOP_RaidPromote, sizeof(ServerRaidPromote_Struct));
	ServerRaidPromote_Struct* srp = (ServerRaidPromote_Struct*)pack->pBuffer;
	strn0cpy(srp->requester_name, c->GetName(), 64);
	strn0cpy(srp->target_name, target_name.c_str(), 64);
	srp->rid = r->GetID();
	worldserver.SendPacket(pack);
	safe_delete(pack);

	raidpromote_cooldowns[char_id] = now;

	c->Message(Chat::White, "Sending promote request for %s...", target_name.c_str());
}
