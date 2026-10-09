#include "../client.h"

void command_showquake(Client *c, const Seperator *sep)
{
	if (!c) return;
	if (c->GuildID() == GUILD_NONE) {
		c->Message(Chat::White, "You must be part of a guild to use this command.");
		return;
	}
	uint32 deadline = 0;
	if (!database.GetAutomaticQuakeTime(deadline)) {
		c->Message(Chat::Red, "The automatic earthquake schedule is unavailable.");
		return;
	}
	if (!RuleB(Quarm, EnableQuakes) || !deadline) {
		c->Message(Chat::Yellow, "Automatic earthquakes are currently disabled.");
		return;
	}
	const uint32 now = Timer::GetTimeSeconds();
	if (deadline <= now) {
		c->Message(Chat::Yellow, "An automatic earthquake is due; awaiting confirmation from world.");
		return;
	}
	c->Message(Chat::Yellow, "The next automatic earthquake is scheduled in %s.", Strings::SecondsToTime(deadline - now).c_str());
}
