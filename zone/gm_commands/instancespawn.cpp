#include "../client.h"
#include "../worldserver.h"
#include "../../common/rulesys.h"

extern WorldServer worldserver;

void command_instancespawn(Client *c, const Seperator *sep)
{
	if (sep->argnum >= 1 && !strcasecmp(sep->arg[1], "kite")) {
		if (sep->argnum != 2 || !strcasecmp(sep->arg[2], "status")) {
			c->Message(Chat::White, "Guild 2+ PoP kite-limit handling is %s.", RuleB(Quarm, EnableGuildInstanceKiteLimit) ? "ENABLED" : "DISABLED");
			return;
		}

		const bool kite_enabled = !strcasecmp(sep->arg[2], "on");
		if (!kite_enabled && strcasecmp(sep->arg[2], "off")) {
			c->Message(Chat::White, "Usage: #instancespawn kite <on|off|status>");
			return;
		}

		RuleManager::Instance()->SetRule("Quarm:EnableGuildInstanceKiteLimit", kite_enabled ? "true" : "false", &database, true, true);
		c->Message(Chat::Yellow, kite_enabled ? "Guild 2+ PoP instances now use existing kite-limit handling." : "Guild 2+ PoP kite-limit handling is now OFF.");
		return;
	}

	if (sep->argnum != 1 || !strcasecmp(sep->arg[1], "status")) {
		c->Message(Chat::White, "Timekeeper controls are %s.", RuleB(Quarm, EnableGuildInstanceRespawnControl) ? "ENABLED" : "DISABLED");
		c->Message(Chat::White, "Guild 2+ PoP kite-limit handling is %s.", RuleB(Quarm, EnableGuildInstanceKiteLimit) ? "ENABLED" : "DISABLED");
		return;
	}

	const bool enabled = !strcasecmp(sep->arg[1], "on");
	if (!enabled && strcasecmp(sep->arg[1], "off")) {
		c->Message(Chat::White, "Usage: #instancespawn <on|off|status|kite on|off|status>");
		return;
	}

	RuleManager::Instance()->SetRule("Quarm:EnableGuildInstanceRespawnControl", enabled ? "true" : "false", &database, true, true);
	RuleManager::Instance()->SetRule("Quarm:InstanceAlwaysHasMinimumSpawnTime", enabled ? "true" : "false", &database, true, true);
	auto pack = new ServerPacket(ServerOP_InstanceRespawnToggle, sizeof(ServerInstanceRespawnToggle_Struct));
	auto *toggle = reinterpret_cast<ServerInstanceRespawnToggle_Struct *>(pack->pBuffer);
	toggle->guild_id = 0;
	toggle->enabled = 0;
	worldserver.SendPacket(pack);
	safe_delete(pack);
	c->Message(Chat::Yellow, enabled ? "Guild 2+ instance 18-hour spawn minimum is now ON. Guild officers may use the Timekeeper." : "Guild 2+ instance 18-hour spawn minimum is now OFF. Timekeeper controls are disabled.");
}
