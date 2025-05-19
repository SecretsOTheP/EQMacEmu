#include "../client.h"
#include "../raids.h"
#include "../raids.h"

void command_raidmove(Client* c, const Seperator* sep) {

	if (!c) {
		return;
	}

	if (!sep->arg[1][0] || sep->argnum < 2) {
		c->Message(Chat::White, "Usage: #raidmove [name] [gid] (-1 for no group)");
		return;
	}

	if (!sep->IsNumber(2))
	{
		c->Message(Chat::White, "Usage: #raidmove [name] [gid] (-1 for no group)");
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

		std::string membername = sep->arg[1];
		unsigned int groupid = atoul(sep->arg[2]) - 1;
		if (groupid < 0 || groupid > MAX_RAID_GROUPS)
		{
			c->Message(Chat::White, "Group ID must be between 1 and 12.");
			return;
		}

		for (int x = 0; x < 72; ++x)
		{
			if (r->members[x].membername.compare(membername.c_str()) == 0)
			{
				if (r->members[x].GroupNumber != groupid)
				{
					Client* target_client = entity_list.GetClientByName(membername.c_str());
					if (groupid == 0xFFFFFFFF)
					{
						c->Message(Chat::White, "%s is now un-grouped.", membername.c_str());
						if (!r->members[x].membername.empty() && r->members[x].IsGroupLeader)
						{
							r->UnSetGroupLeader(membername.c_str(), r->members[x].membername.c_str(), r->members[x].GroupNumber);

							for (int j = 0; j < MAX_RAID_MEMBERS; j++)
							{
								if (r->members[j].GroupNumber == r->members[x].GroupNumber)
								{
									if (!r->members[x].membername.empty() && r->members[x].membername.compare(membername) != 0)
									{
										r->SetGroupLeader(r->members[x].membername.c_str(), r->members[x].GroupNumber);
										break;
									}
								}
							}
						}
						r->MoveMember(membername.c_str(), groupid);
					}
					else
					{
						if (!r->members[x].membername.empty() && r->members[x].IsGroupLeader)
						{
							r->UnSetGroupLeader(membername.c_str(), r->members[x].membername.c_str(), r->members[x].GroupNumber);

							for (int j = 0; j < MAX_RAID_MEMBERS; j++)
							{
								if (r->members[j].GroupNumber == r->members[x].GroupNumber)
								{
									if (!r->members[x].membername.empty() && r->members[x].membername.compare(membername) != 0)
									{
										r->SetGroupLeader(r->members[x].membername.c_str(), r->members[x].GroupNumber);
										break;
									}
								}
							}
						}
						r->MoveMember(membername.c_str(), groupid);

						c->Message(Chat::White, "%s is now in group %d.", membername.c_str(), groupid);
					}
				}
				else
				{
					if (r->members[x].GroupNumber == 0xFFFFFFFF)
						c->Message(Chat::White, "%s is already un-grouped.", membername.c_str());
					else
						c->Message(Chat::White, "%s is already in group %d.", membername.c_str(), r->members[x].GroupNumber);
				}

				Client* c = entity_list.GetClientByName(membername.c_str());
				return;
			}
		}
		c->Message(Chat::White, "Usage: #raidmove [name] [gid] (-1 for no group)");
	}
	else
	{
		c->Message(Chat::White, "You must be in a raid to use that command.");
	}
}