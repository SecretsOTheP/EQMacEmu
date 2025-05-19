#include "../client.h"
#include "../raids.h"
#include "../raids.h"
void command_raidloot(Client* c, const Seperator* sep) {
	if (!sep->arg[1][0]) {
		c->Message(Chat::White, "Usage: #raidloot [LEADER/GROUPLEADER/SELECTED/ALL]");
		return;
	}

	Raid* r = c->GetRaid();
	if (r)
	{
		for (int x = 0; x < 72; ++x)
		{
			if (r->members[x].membername.compare(c->GetCleanName()) == 0)
			{
				if (r->members[x].IsRaidLeader == 0)
				{
					c->Message(Chat::White, "You must be the raid leader to use this command.");
				}
				else
				{
					break;
				}
			}
		}

		if (strcasecmp(sep->arg[1], "LEADER") == 0)
		{
			c->Message(Chat::Yellow, "Loot type changed to: 1");
			r->ChangeLootType(1);
		}
		else if (strcasecmp(sep->arg[1], "GROUPLEADER") == 0)
		{
			c->Message(Chat::Yellow, "Loot type changed to: 2");
			r->ChangeLootType(2);
		}
		else if (strcasecmp(sep->arg[1], "SELECTED") == 0)
		{
			c->Message(Chat::Yellow, "Loot type changed to: 3");
			r->ChangeLootType(3);
		}
		else if (strcasecmp(sep->arg[1], "ALL") == 0)
		{
			c->Message(Chat::Yellow, "Loot type changed to: 4");
			r->ChangeLootType(4);
		}
		else
		{
			c->Message(Chat::White, "Usage: #raidloot [LEADER/GROUPLEADER/SELECTED/ALL]");
		}
	}
	else
	{
		c->Message(Chat::White, "You must be in a raid to use that command.");
	}
}

