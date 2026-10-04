#include "../client.h"
#include "../command.h"
#include "../zone.h"
#include "../data_bucket.h"
#include "../../common/strings.h"
#include <map>

void command_zonelos(Client* c, const Seperator* sep)
{
	if (!c || !zone) return;
	const bool list = sep->argnum == 0 ||
		(sep->argnum == 1 && !strcasecmp(sep->arg[1], "status"));
	if (!list && (sep->argnum != 2 ||
		(strcasecmp(sep->arg[2], "status") && strcasecmp(sep->arg[2], "on") &&
		 strcasecmp(sep->arg[2], "off") && strcasecmp(sep->arg[2], "default")))) {
		c->Message(Chat::White, "Usage: #zonelos status | <shortname> <status|on|off|default>");
		return;
	}
	const std::string name = list ? "" : Strings::ToLower(sep->arg[1]);
	auto zones = database.QueryDatabase(
		"SELECT DISTINCT short_name, skip_los FROM zone" +
		(list ? std::string("") : " WHERE short_name = '" + Strings::Escape(name) + "'") +
		" ORDER BY short_name");
	if (!zones.Success()) {
		c->Message(Chat::Red, "Could not read zone settings.");
		return;
	}
	if (!zones.RowCount()) {
		c->Message(Chat::Red, "Unknown zone shortname.");
		return;
	}
	if (!list && strcasecmp(sep->arg[2], "status")) {
		const std::string key = "zone_skip_los:" + name;
		if (!strcasecmp(sep->arg[2], "default")) {
			DataBucket::DeleteData(key);
		} else {
			DataBucket::SetData(key, !strcasecmp(sep->arg[2], "on") ? "0" : "1");
		}
		zone->RefreshSkipLoSOverride();
	}
	auto saved = database.QueryDatabase(
		"SELECT SUBSTRING(`key`, 15), value FROM data_buckets WHERE LEFT(`key`, 14) = 'zone_skip_los:'");
	if (!saved.Success()) {
		c->Message(Chat::Red, "Could not verify saved overrides.");
		return;
	}
	std::map<std::string, std::string> overrides;
	for (auto row : saved) {
		if (row[0] && row[1]) overrides[row[0]] = row[1];
	}
	if (!list && strcasecmp(sep->arg[2], "status")) {
		std::string actual;
		const auto found = overrides.find(name);
		if (found != overrides.end()) actual = found->second;
		const std::string expected = !strcasecmp(sep->arg[2], "default") ? "" :
			(!strcasecmp(sep->arg[2], "on") ? "0" : "1");
		if (actual != expected) {
			c->Message(Chat::Red, "Saved setting did not match the request. Check database errors.");
			return;
		}
	}
	c->Message(Chat::White, "ON enables line-of-sight checks; OFF disables them. Applies to open world and Guild 2+; Guild 1 PvP is unchanged.");
	for (auto row : zones) {
		const std::string shortname = row[0];
		std::string value = row[1] && std::string(row[1]) != "0" ? "1" : "0";
		bool overridden = false;
		const auto found = overrides.find(shortname);
		if (found != overrides.end() && (found->second == "0" || found->second == "1")) {
			value = found->second;
			overridden = true;
		}
		c->Message(Chat::White, "%s: LOS checks %s (%s). Guild 1 PvP: LOS checks %s (database setting).",
			shortname.c_str(), value == "0" ? "ENABLED" : "DISABLED",
			overridden ? "saved override" : "database default",
			row[1] && std::string(row[1]) != "0" ? "DISABLED" : "ENABLED");
	}
	c->Message(Chat::White, "Saved overrides persist through downtime; active zones pick them up on LOS checks within five seconds. 'default' removes an override.");
}
