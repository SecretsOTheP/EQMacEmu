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
		if (RuleB(Quarm, EnableGuildInstanceRespawnControl)) {
			c->Message(Chat::White, "Guild instance respawn control is ON. In PoP, Slow Down uses the Quarm minimum and Restore uses database timers; Luclin and earlier always use the Quarm minimum.");
		}
		else {
			c->Message(Chat::White, "Guild instance respawn control is OFF. All guild instances use the Quarm minimum (18 hours or any longer database timer); Timekeeper choices are cleared.");
		}
		c->Message(Chat::White, "Guild 2+ PoP kite-limit handling is %s.", RuleB(Quarm, EnableGuildInstanceKiteLimit) ? "ENABLED" : "DISABLED");
		c->Message(Chat::White, "Usage: #instancespawn <on|off|status|kite on|off|status>");
		return;
	}

	const bool enabled = !strcasecmp(sep->arg[1], "on");
	if (!enabled && strcasecmp(sep->arg[1], "off")) {
		c->Message(Chat::White, "Usage: #instancespawn <on|off|status|kite on|off|status>");
		return;
	}

	if (!enabled) {
		auto reset_guild_choices = database.QueryDatabase(
			"DELETE FROM data_buckets WHERE LEFT(`key`, 24) = 'guild_instance_respawns:'"
		);
		if (!reset_guild_choices.Success()) {
			c->Message(Chat::Red, "Could not reset saved guild respawn choices; instance mode was not changed.");
			return;
		}
	}

	if (!RuleManager::Instance()->SetRule("Quarm:EnableGuildInstanceRespawnControl", enabled ? "true" : "false", &database, true, true) ||
		!RuleManager::Instance()->SetRule("Quarm:InstanceAlwaysHasMinimumSpawnTime", enabled ? "false" : "true", &database, true, true)) {
		c->Message(Chat::Red, "Could not update guild-instance respawn mode.");
		return;
	}

	auto pack = new ServerPacket(ServerOP_InstanceRespawnToggle, sizeof(ServerInstanceRespawnToggle_Struct));
	auto *toggle = reinterpret_cast<ServerInstanceRespawnToggle_Struct *>(pack->pBuffer);
	toggle->guild_id = 0;
	toggle->enabled = enabled;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	c->Message(Chat::Yellow, enabled
		? "Guild instance respawn control is ON. In PoP, Slow Down uses the Quarm minimum and Restore uses database timers; Luclin and earlier always use the Quarm minimum."
		: "Guild instance respawn control is OFF. All guild instances use the Quarm minimum (18 hours or any longer database timer); Timekeeper choices are cleared and shorter pending timers are extended.");
}
