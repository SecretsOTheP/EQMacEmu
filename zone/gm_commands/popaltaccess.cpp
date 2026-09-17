#include "../client.h"
#include "../data_bucket.h"

#include <cstdlib>
#include <string>

namespace {
constexpr const char* PopAlternateAccessBucket = "pop_alt_access_enabled";
constexpr const char* PopRaidFlagPercentBucket = "pop_raid_flagged_percent";

uint8 PopRaidFlagRequirement()
{
	const auto configured = atoi(DataBucket::GetData(PopRaidFlagPercentBucket).c_str());
	return configured >= 1 && configured <= 100 ? configured : 85;
}

void PopAlternateAccessUsage(Client* c)
{
	c->Message(Chat::White, "Usage: #popaltaccess <on|off|status|threshold 1-100>");
}
}

void command_popaltaccess(Client* c, const Seperator* sep)
{
	if (!c || sep->argnum < 1 || !strcasecmp(sep->arg[1], "help")) {
		if (c) {
			PopAlternateAccessUsage(c);
		}
		return;
	}

	if (!strcasecmp(sep->arg[1], "status")) {
		const bool enabled = DataBucket::GetData(PopAlternateAccessBucket) == "1";
		c->Message(
			Chat::White,
			enabled
				? "Plane of Power alternate access is enabled."
				: "Plane of Power alternate access is disabled."
		);
		c->Message(
			Chat::White,
			"Raid willing requirement: %u%% of the entire raid must hold each destination flag.",
			PopRaidFlagRequirement()
		);
		return;
	}

	if (!strcasecmp(sep->arg[1], "threshold")) {
		if (sep->argnum != 2) {
			PopAlternateAccessUsage(c);
			return;
		}

		const auto required_percent = atoi(sep->arg[2]);
		if (required_percent < 1 || required_percent > 100) {
			c->Message(Chat::Red, "The raid willing threshold must be from 1 through 100.");
			return;
		}

		DataBucket::SetData(PopRaidFlagPercentBucket, std::to_string(required_percent));
		c->Message(
			Chat::Yellow,
			"Raid willing now requires %u%% of the entire raid to hold each destination flag.",
			required_percent
		);
		return;
	}

	if (sep->argnum == 1 && (!strcasecmp(sep->arg[1], "on") || !strcasecmp(sep->arg[1], "off"))) {
		const bool enabled = !strcasecmp(sep->arg[1], "on");
		DataBucket::SetData(PopAlternateAccessBucket, enabled ? "1" : "0");
		c->Message(
			Chat::Yellow,
			enabled
				? "Plane of Power alternate access and raid willing are now enabled."
				: "Plane of Power alternate access and raid willing are now disabled."
		);
		return;
	}

	PopAlternateAccessUsage(c);
}
