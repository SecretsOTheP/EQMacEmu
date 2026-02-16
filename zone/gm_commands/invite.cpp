#include "../client.h"
#include "../../common/eq_packet_structs.h"

// rate limit
static std::map<uint32, time_t> invite_cooldowns;
static const int INVITE_COOLDOWN_SECONDS = 1;

void command_invite(Client *c, const Seperator *sep)
{
	uint32 char_id = c->CharacterID();
	time_t now = time(nullptr);
	auto it = invite_cooldowns.find(char_id);
	if (it != invite_cooldowns.end()) {
		time_t elapsed = now - it->second;
		if (elapsed < INVITE_COOLDOWN_SECONDS) {
			c->Message(Chat::Red, "Please wait %d second(s) before sending another group invite.",
				(int)(INVITE_COOLDOWN_SECONDS - elapsed));
			return;
		}
	}

	if (!sep->arg[1][0]) {
		c->Message(Chat::White, "Usage: #invite [playername]");
		c->Message(Chat::White, "Sends a group invite to a player in this zone (no target required).");
		return;
	}

	Client *localInvitee = entity_list.GetClientByName(sep->arg[1]);

	if (!localInvitee) {
		c->Message(Chat::Red, "%s is not in this zone.", sep->arg[1]);
		return;
	}
	else if (localInvitee->GetGroup()) {
		c->Message(Chat::Red, "%s is already in a group.", sep->arg[1]);
		return;
	}

	Group *g = c->GetGroup();
	if (g) {
		if (!g->IsLeader(c)) {
			c->Message(Chat::Red, "You must be the group leader to invite others.");
			return;
		}
		if (g->GroupCount() >= MAX_GROUP_MEMBERS) {
			c->Message(Chat::Red, "Your group is full.");
			return;
		}
	}

	Raid *r = c->GetRaid();
	if (r && !r->IsGroupLeader(c->GetName())) {
		c->Message(Chat::Red, "You must be a raid group leader to invite others.");
		return;
	}

	c->Message(Chat::White, "You invite %s to join your group.", localInvitee->GetName());

	auto app = new EQApplicationPacket(OP_GroupInvite, sizeof(GroupInvite_Struct));
	GroupInvite_Struct* gis = (GroupInvite_Struct*)app->pBuffer;
	memset(gis, 0, sizeof(GroupInvite_Struct));
	strn0cpy(gis->inviter_name, c->GetName(), 64);
	strn0cpy(gis->invitee_name, sep->arg[1], 64);

	c->Handle_OP_GroupInvite(app);

	safe_delete(app);

	invite_cooldowns[char_id] = now;
}
