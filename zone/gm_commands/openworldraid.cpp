#include "../client.h"
#include "../command.h"
#include "../data_bucket.h"
#include "../../common/timer.h"

#include <cstdlib>
#include <string>

void command_openworldraid(Client *c, const Seperator *sep)
{
	if (!c) {
		return;
	}
	const char *bucket = "open_world_raid_respawns";
	const char *label = "through Luclin";
	const char *action = nullptr;
	if (sep->argnum == 1) {
		action = sep->arg[1];
	} else if (sep->argnum == 2) {
		if (!strcasecmp(sep->arg[1], "luclin")) {
			action = sep->arg[2];
		} else if (!strcasecmp(sep->arg[1], "pop")) {
			bucket = "open_world_raid_respawns_pop";
			label = "for Planes of Power";
			action = sep->arg[2];
		}
	}
	if (sep->argnum == 0 || (action && !strcasecmp(action, "status"))) {
		c->Message(Chat::White, "Natural open-world raid respawns %s: %s.", label,
			strtoul(DataBucket::GetData(bucket).c_str(), nullptr, 10) > 1 ? "ON" : "OFF");
		if (sep->argnum <= 1) {
			c->Message(Chat::White, "Natural open-world raid respawns for Planes of Power: %s.",
				strtoul(DataBucket::GetData("open_world_raid_respawns_pop").c_str(), nullptr, 10) > 1 ? "ON" : "OFF");
		}
		return;
	}
	if (!action || (strcasecmp(action, "on") && strcasecmp(action, "off"))) {
		c->Message(Chat::White, "Usage: #openworldraid <luclin|pop> <on|off|status> (omitting the tier selects Luclin)");
		return;
	}
	const bool enabled = !strcasecmp(action, "on");
	const auto current = DataBucket::GetData(bucket);
	const auto started_at = enabled
		? (strtoul(current.c_str(), nullptr, 10) > 1 ? current : std::to_string(Timer::GetTimeSeconds()))
		: "0";
	DataBucket::SetData(bucket, started_at);
	if (DataBucket::GetData(bucket) != started_at) {
		c->Message(Chat::Red, "Could not save the open-world raid respawn setting.");
		return;
	}
	c->Message(Chat::Yellow, "Natural open-world raid respawns %s: %s. Active zones update within five seconds.", label,
		enabled ? "ON" : "OFF");
}
