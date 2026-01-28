#include "../client.h"
#include "../worldserver.h"
#include "../raids.h"
#include "../../common/servertalk.h"

extern WorldServer worldserver;

void command_raidaccept(Client *c, const Seperator *sep)
{
	if (!c->HasPendingCrossZoneRaidInvite()) {
		c->Message(Chat::Red, "You don't have any pending cross-zone raid invites.");
		return;
	}

	std::string inviter_name = c->GetPendingCrossZoneRaidInviter();
	ChallengeRules::RuleSet inviter_ruleset = c->GetPendingCrossZoneRaidRuleset();

	Group *g = c->GetGroup();
	if (g) {
		c->Message(Chat::Red, "Cross-zone raid invites for grouped players are not currently supported.");
		c->ClearPendingCrossZoneRaidInvite();
		return;
	}

	if (c->GetRaid()) {
		c->Message(Chat::Red, "You are already in a raid.");
		c->ClearPendingCrossZoneRaidInvite();
		return;
	}

	if (!c->CanGroupWith(inviter_ruleset)) {
		c->Message(Chat::Red, "Your challenge mode does not match the raid's. You cannot join.");
		c->ClearPendingCrossZoneRaidInvite();
		return;
	}

	if (!worldserver.Connected()) {
		c->Message(Chat::Red, "Cannot accept cross-zone invite: world server is not connected.");
		return;
	}

	auto pack = new ServerPacket(ServerOP_RaidInviteResponse, sizeof(ServerRaidInvite_Struct));
	ServerRaidInvite_Struct* sris = (ServerRaidInvite_Struct*)pack->pBuffer;
	strn0cpy(sris->inviter_name, inviter_name.c_str(), 64);
	strn0cpy(sris->invitee_name, c->GetName(), 64);
	sris->rid = 0;
	sris->invite_id = 0;
	sris->raid_ruleset = c->GetRuleSet();
	sris->is_acceptance = true;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	c->Message(Chat::Yellow, "Accepting raid invite from %s...", inviter_name.c_str());
	c->ClearPendingCrossZoneRaidInvite();
}
