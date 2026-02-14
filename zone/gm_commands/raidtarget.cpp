#include "../client.h"
#include "../raids.h"

void command_raidtarget(Client *c, const Seperator *sep)
{
	if (!sep->arg[1][0])
	{
		c->Message(Chat::White, "Usage: #raidtarget [playername]");
		c->Message(Chat::White, "Targets a raid member in your zone.");
		return;
	}

	Raid *r = c->GetRaid();
	if (!r)
	{
		c->Message(Chat::Red, "You must be in a raid to use this command.");
		return;
	}

	std::string target_name = sep->arg[1];

	Client *target = nullptr;
	for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
		if (strlen(r->members[i].membername) > 0 &&
		    strcasecmp(target_name.c_str(), r->members[i].membername) == 0) {
			target = r->members[i].member;
			target_name = r->members[i].membername;
			break;
		}
	}

	if (!target) {
		c->Message(Chat::Red, "%s is not in your raid or is not in this zone.", target_name.c_str());
		return;
	}

	c->SetTarget(target);
	c->SendTargetCommand(target->GetID());
}
