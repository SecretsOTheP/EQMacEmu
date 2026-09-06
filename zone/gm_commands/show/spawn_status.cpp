#include "../../client.h"

void ShowSpawnStatus(Client* c, const Seperator* sep)
{
	// command_show passes the full command: #show spawn_status [filter].
	if (!sep->arg[2][0])
	{
		zone->SpawnStatus(c, 'u');
	}
	else if (sep->IsNumber(2) && atoi(sep->arg[2]) > 0)
	{
		// show spawn status by spawn2 id
		zone->SpawnStatus(c, 'a', atoi(sep->arg[2]));
	}
	else if (Strings::EqualFold(sep->arg[2], "help"))
	{
		c->Message(Chat::White, "Usage: #show spawn_status <[u]nspawned (default) | [a]ll | [s]pawned | [d]isabled | [e]nabled | {Spawn2 ID}>");
	}
	else {
		const std::string filter = Strings::ToLower(sep->arg[2]);
		if (filter == "all" || filter == "a" ||
			filter == "unspawned" || filter == "u" ||
			filter == "spawned" || filter == "s" ||
			filter == "disabled" || filter == "d" ||
			filter == "enabled" || filter == "e") {
			zone->SpawnStatus(c, filter[0]);
		}
		else {
			c->Message(Chat::White, "Unknown spawn filter. Use #show spawn_status help.");
		}
	}
}
