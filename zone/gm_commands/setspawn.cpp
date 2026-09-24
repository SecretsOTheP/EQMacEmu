#include "../client.h"
#include "../data_bucket.h"

#include "../../common/eq_constants.h"
#include "../../common/strings.h"

#include <cstdlib>

void command_setspawn(Client *c, const Seperator *sep)
{
	if (sep->argnum == 1 && !strcasecmp(sep->arg[1], "status")) {
		auto results = database.QueryDatabase(
			"SELECT z.short_name, COALESCE(b.value, 'database timer') "
			"FROM zone AS z LEFT JOIN data_buckets AS b "
			"ON b.`key` = CONCAT('pop_spawn_minutes_', z.short_name) "
			"WHERE z.expansion = 4 ORDER BY z.short_name"
		);
		if (!results.Success()) {
			c->Message(Chat::Red, "Unable to list Planes of Power spawn settings.");
			return;
		}

		c->Message(Chat::White, "Planes of Power normal-trash respawn settings:");
		for (auto row : results) {
			c->Message(Chat::White, "%s: %s", row[0], row[1]);
		}
		return;
	}

	if (sep->argnum != 2) {
		c->Message(Chat::White, "Usage: #setspawn <PoP shortname> <minutes|status|default>");
		c->Message(Chat::White, "       #setspawn status");
		return;
	}

	const auto short_name = Strings::ToLower(sep->arg[1]);
	const auto zone_id = database.GetZoneID(short_name.c_str());
	if (zone_id == 0) {
		c->Message(Chat::Red, "Unknown zone short name: %s", short_name.c_str());
		return;
	}

	auto result = database.QueryDatabase(
		fmt::format("SELECT expansion FROM zone WHERE zoneidnumber = {}", zone_id)
	);
	if (!result.Success() || result.RowCount() != 1 || atoi(result.begin()[0]) != PlanesEQEra) {
		c->Message(Chat::Red, "%s is not a Planes of Power zone.", short_name.c_str());
		return;
	}

	const auto key = fmt::format("pop_spawn_minutes_{}", short_name);
	if (!strcasecmp(sep->arg[2], "status")) {
		const auto value = DataBucket::GetData(key);
		if (value.empty()) {
			c->Message(Chat::White, "%s uses its database respawn timers.", short_name.c_str());
		}
		else {
			c->Message(Chat::White, "%s normal trash respawns in %s minute(s).", short_name.c_str(), value.c_str());
		}
		return;
	}

	if (!strcasecmp(sep->arg[2], "default")) {
		DataBucket::DeleteData(key);
		c->Message(Chat::Yellow, "%s now uses its database respawn timers.", short_name.c_str());
		return;
	}

	char *end = nullptr;
	const float minutes = std::strtof(sep->arg[2], &end);
	if (!end || *end != '\0' || minutes < 1.0f || minutes > 120.0f) {
		c->Message(Chat::Red, "Respawn minutes must be between 1 and 120.");
		return;
	}

	DataBucket::SetData(key, fmt::format("{:.2f}", minutes));
	c->Message(
		Chat::Yellow,
		"%s normal trash will respawn in %.2f minute(s) after future deaths.",
		short_name.c_str(),
		minutes
	);
}
