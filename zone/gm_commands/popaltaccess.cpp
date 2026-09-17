#include "../client.h"
#include "../data_bucket.h"

namespace {
constexpr const char* PopAlternateAccessBucket = "pop_alt_access_enabled";

void PopAlternateAccessUsage(Client* c)
{
	c->Message(Chat::White, "Usage: #popaltaccess <on|off|status>");
}
}

void command_popaltaccess(Client* c, const Seperator* sep)
{
	if (!c || sep->argnum != 1 || !strcasecmp(sep->arg[1], "help")) {
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
		return;
	}

	if (!strcasecmp(sep->arg[1], "on") || !strcasecmp(sep->arg[1], "off")) {
		const bool enabled = !strcasecmp(sep->arg[1], "on");
		DataBucket::SetData(PopAlternateAccessBucket, enabled ? "1" : "0");
		c->Message(
			Chat::Yellow,
			enabled
				? "Plane of Power alternate access is now enabled."
				: "Plane of Power alternate access is now disabled."
		);
		return;
	}

	PopAlternateAccessUsage(c);
}
