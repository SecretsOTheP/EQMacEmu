#include "../client.h"
#include "../command.h"
#include "../data_bucket.h"
#include "../worldserver.h"

#include "../../common/rulesys.h"
#include "../../common/strings.h"

#include <string>

extern WorldServer worldserver;

namespace {
constexpr const char *BardEnabledRule = "Quarm:EnableBardInstagibLimit";
constexpr const char *BardLimitRule = "Quarm:BardInstagibPullLimit";
constexpr const char *TakpStackedRule = "Quarm:EnableTAKPStackedMobAntiKite";
constexpr int MaxKiteLimit = 100;

void Usage(Client *c)
{
	c->Message(Chat::White, "#kitelimit commands:");
	c->Message(Chat::White, "  status - Show the global bard limit and TAKP setting.");
	c->Message(Chat::White, "  on | off - Enable or disable the global bard kite limit.");
	c->Message(Chat::White, "  <number> - Set the global bard limit (1-100); summoning starts at limit + 1.");
	c->Message(Chat::White, "  <zone> status - Show that zone's override or inherited global setting.");
	c->Message(Chat::White, "  <zone> <number> - Set that zone's bard limit (1-100).");
	c->Message(Chat::White, "  <zone> on | off - Use the global limit, or disable it in that zone.");
	c->Message(Chat::White, "  <zone> default - Remove that zone's override and use global settings.");
	c->Message(Chat::White, "  default - Clear all zone overrides and use global settings everywhere.");
	c->Message(Chat::White, "  takp on | off | status - Enable, disable, or check TAKP stacked-mob behavior.");
	c->Message(Chat::White, "Guild instances: bard limits apply to Guild 2+ PoP on Restore; not Guild 1 or Slow Down.");
}

bool SetRuleAndReload(const char *rule_name, const std::string &value)
{
	if (!RuleManager::Instance()->SetRule(rule_name, value, &database, true, true)) {
		return false;
	}

	auto *pack = new ServerPacket(ServerOP_ReloadRules, 0);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	return true;
}

void ShowStatus(Client *c)
{
	c->Message(Chat::White, "Global bard kite limit: %s; %d mobs allowed (summoning starts at %d).",
		RuleB(Quarm, EnableBardInstagibLimit) ? "ON" : "OFF",
		RuleI(Quarm, BardInstagibPullLimit), RuleI(Quarm, BardInstagibPullLimit) + 1);
	c->Message(Chat::White, "TAKP stacked-mob behavior: %s.",
		RuleB(Quarm, EnableTAKPStackedMobAntiKite) ? "ON" : "OFF");
}

void HandleTakp(Client *c, const Seperator *sep)
{
	if (sep->argnum != 2 || !strcasecmp(sep->arg[2], "status")) {
		c->Message(Chat::White, "TAKP stacked-mob behavior is %s.",
			RuleB(Quarm, EnableTAKPStackedMobAntiKite) ? "ON" : "OFF");
		c->Message(Chat::White, "Usage: #kitelimit takp <on|off|status>");
		return;
	}

	const bool enable = !strcasecmp(sep->arg[2], "on");
	if (!enable && strcasecmp(sep->arg[2], "off")) {
		Usage(c);
		return;
	}
	if (!SetRuleAndReload(TakpStackedRule, enable ? "true" : "false")) {
		c->Message(Chat::Red, "Could not update the TAKP stacked-mob setting.");
		return;
	}
	c->Message(Chat::Yellow, "TAKP stacked-mob behavior is now %s.", enable ? "ON" : "OFF");
}

void HandleZone(Client *c, const Seperator *sep)
{
	if (sep->argnum != 2) {
		Usage(c);
		return;
	}

	const auto zone_name = Strings::ToLower(sep->arg[1]);
	if (database.GetZoneID(zone_name.c_str()) == 0) {
		c->Message(Chat::Red, "Unknown zone short name: %s", zone_name.c_str());
		return;
	}
	const auto key = "bardlimit_" + zone_name;
	const std::string value = Strings::ToLower(sep->arg[2]);

	if (value == "status") {
		const auto override = DataBucket::GetData(key);
		if (override.empty()) {
			c->Message(Chat::White, "%s uses global settings: %s, %d allowed.", zone_name.c_str(),
				RuleB(Quarm, EnableBardInstagibLimit) ? "ON" : "OFF", RuleI(Quarm, BardInstagibPullLimit));
		} else if (override == "off" || override == "0") {
			c->Message(Chat::White, "%s bard kite limit is OFF.", zone_name.c_str());
		} else if (override == "on") {
			c->Message(Chat::White, "%s bard kite limit is ON, using the global limit of %d.",
				zone_name.c_str(), RuleI(Quarm, BardInstagibPullLimit));
		} else {
			c->Message(Chat::White, "%s allows %s mobs; summoning starts at %d.", zone_name.c_str(),
				override.c_str(), Strings::ToInt(override) + 1);
		}
		return;
	}

	if (value == "default") {
		if (!DataBucket::DeleteData(key)) {
			c->Message(Chat::Red, "Could not clear the bard kite override for %s.", zone_name.c_str());
			return;
		}
		c->Message(Chat::Yellow, "%s now uses the global server kite-limit rule.", zone_name.c_str());
		return;
	}

	if (value == "on" || value == "off") {
		DataBucket::SetData(key, value);
		if (value == "on") {
			c->Message(Chat::Yellow, "%s bard kite limit is ON using the global limit of %d.",
				zone_name.c_str(), RuleI(Quarm, BardInstagibPullLimit));
		} else {
			c->Message(Chat::Yellow, "%s bard kite limit is OFF.", zone_name.c_str());
		}
		return;
	}

	if (!Strings::IsNumber(sep->arg[2])) {
		Usage(c);
		return;
	}
	const int limit = Strings::ToInt(sep->arg[2]);
	if (limit < 1 || limit > MaxKiteLimit) {
		c->Message(Chat::Red, "Zone kite limit must be between 1 and %d; use 'off' to disable it.", MaxKiteLimit);
		return;
	}

	DataBucket::SetData(key, std::to_string(limit));
	c->Message(Chat::Yellow, "%s allows %d bard-kited mobs; summoning starts at %d.",
		zone_name.c_str(), limit, limit + 1);
}
}

void command_kitelimit(Client *c, const Seperator *sep)
{
	if (!c) {
		return;
	}
	if (sep->argnum == 0 || !strcasecmp(sep->arg[1], "help")) {
		Usage(c);
		ShowStatus(c);
		return;
	}

	const std::string first = Strings::ToLower(sep->arg[1]);
	if (first == "status" && sep->argnum == 1) {
		ShowStatus(c);
		return;
	}
	if (first == "takp") {
		HandleTakp(c, sep);
		return;
	}
	if (first == "default" && sep->argnum == 1) {
		const auto result = database.QueryDatabase(
			"DELETE FROM data_buckets WHERE LEFT(`key`, 10) = 'bardlimit_'"
		);
		if (!result.Success()) {
			c->Message(Chat::Red, "Could not reset zone kite limits.");
			return;
		}
		c->Message(Chat::Yellow, "All zone bard kite overrides were cleared; zones now use the global server rule.");
		return;
	}

	if (sep->argnum == 1 && (first == "on" || first == "off")) {
		const bool enable = first == "on";
		if (!SetRuleAndReload(BardEnabledRule, enable ? "true" : "false")) {
			c->Message(Chat::Red, "Could not update the global bard kite setting.");
			return;
		}
		c->Message(Chat::Yellow, "Global bard kite limit is now %s.", enable ? "ON" : "OFF");
		return;
	}

	if (sep->argnum == 1 && Strings::IsNumber(sep->arg[1])) {
		const int limit = Strings::ToInt(sep->arg[1]);
		if (limit < 1 || limit > MaxKiteLimit) {
			c->Message(Chat::Red, "Global kite limit must be between 1 and %d.", MaxKiteLimit);
			return;
		}
		if (!SetRuleAndReload(BardLimitRule, std::to_string(limit))) {
			c->Message(Chat::Red, "Could not update the global bard kite limit.");
			return;
		}
		c->Message(Chat::Yellow, "Global bard kite limit is %d; summoning starts at %d mobs.", limit, limit + 1);
		return;
	}

	HandleZone(c, sep);
}
