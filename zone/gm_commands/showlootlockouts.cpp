#include "../client.h"

void command_showlootlockouts(Client *c, const Seperator *sep)
{
	if (!c)
		return;

	int64_t curTime = Timer::GetTimeSeconds();


	c->Message(Chat::Lime, "=== Current Loot Lockouts ===");

	for (auto lockout : c->loot_lockouts)
	{
		int64_t time_remaining = lockout.second.expirydate - curTime;
		if (time_remaining >= 1)
		{
			c->Message(Chat::Red, "== %s: Expires in %s", lockout.second.npc_name, Strings::SecondsToTime((int)time_remaining).c_str());
		}
		else // Available
		{
			if (time_remaining >= -2592000) // Shows lockouts that ended within the last 30 days
			{
				c->Message(Chat::Lime, "== %s: Available", lockout.second.npc_name);
			}
		}
	}


	c->Message(Chat::Lime, "=== Current Legacy Item Lockouts ===");

	for (auto legacyitem : c->looted_legacy_items)
	{
		const EQ::ItemData* item = database.GetItem(legacyitem.first);

		if (!item)
			continue;

		if (!item->Name[0])
			continue;

		int64_t time_remaining = 0xFFFFFFFFFFFFFFFF;
		if (legacyitem.second.expirydate != 0)
		{
			time_remaining = legacyitem.second.expirydate - curTime;
		}

		//Magic number
		if (time_remaining == 0xFFFFFFFFFFFFFFFF) // Never unlocks
		{
			c->Message(Chat::Red, "== %s: Never Expires", item->Name);
		}
		else if (time_remaining >= 1) // Lockout present
		{
			c->Message(Chat::Red, "== %s: Expires in %s", item->Name, Strings::SecondsToTime((int)time_remaining).c_str());
		}
		else //Available
		{
			if (time_remaining >= -2592000) // Shows lockouts that ended within the last 30 days
			{
				c->Message(Chat::Lime, "== %s: Available", item->Name);
			}
		}
	}
}

