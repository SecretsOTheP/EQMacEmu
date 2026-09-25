#include "../client.h"
#include "../worldserver.h"
#include "../../common/rulesys.h"

extern WorldServer worldserver;

void command_takpkite(Client *c, const Seperator *sep)
{
	if (sep->argnum != 1 || !strcasecmp(sep->arg[1], "status")) {
		c->Message(
			Chat::White,
			"TAKP stacked-mob anti-kite is %s.",
			RuleB(Quarm, EnableTAKPStackedMobAntiKite) ? "ON" : "OFF"
		);
		c->Message(Chat::White, "Usage: #takpkite <on|off|status>");
		return;
	}

	const bool enabled = !strcasecmp(sep->arg[1], "on");
	if (!enabled && strcasecmp(sep->arg[1], "off")) {
		c->Message(Chat::White, "Usage: #takpkite <on|off|status>");
		return;
	}

	if (!RuleManager::Instance()->SetRule(
		"Quarm:EnableTAKPStackedMobAntiKite",
		enabled ? "true" : "false",
		&database,
		true,
		true
	)) {
		c->Message(Chat::Red, "Could not update the TAKP stacked-mob anti-kite rule.");
		return;
	}

	// Persist the setting in the active ruleset, then reload it in every zone.
	auto pack = new ServerPacket(ServerOP_ReloadRules, 0);
	worldserver.SendPacket(pack);
	safe_delete(pack);

	c->Message(
		Chat::Yellow,
		"TAKP stacked-mob anti-kite is now %s.",
		enabled ? "ON" : "OFF"
	);
}
