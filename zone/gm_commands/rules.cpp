#include "../client.h"
#include "../command.h"

#include "../../common/repositories/rule_sets_repository.h"
#include "../../common/repositories/rule_values_repository.h"
#include "../data_bucket.h"
#include "../../common/rulesys.h"
#include "../../common/strings.h"
#include <cstdlib>

#include <map>
#include <set>
#include <sstream>

void command_rules(Client *c, const Seperator *sep)
{
	auto arguments = sep->argnum;
	bool is_help = !strcasecmp(sep->arg[1], "help");
	if (!arguments || is_help) {
		SendRuleSubCommands(c);
		return;
	}

	bool is_current = !strcasecmp(sep->arg[1], "current");
	bool is_get = !strcasecmp(sep->arg[1], "get");
	bool is_list = !strcasecmp(sep->arg[1], "list");
	bool is_list_sets = !strcasecmp(sep->arg[1], "listsets");
	bool is_load = !strcasecmp(sep->arg[1], "load");
	bool is_reload = !strcasecmp(sep->arg[1], "reload");
	bool is_reset = !strcasecmp(sep->arg[1], "reset");
	bool is_set = !strcasecmp(sep->arg[1], "set");
	bool is_set_db = !strcasecmp(sep->arg[1], "setdb");
	bool is_store = !strcasecmp(sep->arg[1], "store");
	bool is_switch = !strcasecmp(sep->arg[1], "switch");
	bool is_values = !strcasecmp(sep->arg[1], "values");
	if (
		!is_current &&
		!is_get &&
		!is_list &&
		!is_list_sets &&
		!is_load &&
		!is_reload &&
		!is_reset &&
		!is_set &&
		!is_set_db &&
		!is_store &&
		!is_switch &&
		!is_values
		) {
		SendRuleSubCommands(c);
		return;
	}

	if (is_current) {
		c->Message(
			Chat::White,
			fmt::format(
				"Currently running Rule Set {} ({}).",
				RuleManager::Instance()->GetActiveRuleset(),
				RuleManager::Instance()->GetActiveRulesetID()
			).c_str()
		);
	}
	else if (is_list_sets) {
		std::map<int, std::string> m;
		if (!RuleManager::Instance()->ListRulesets(&database, m)) {
			c->Message(Chat::White, "Failed to list Rule Sets!");
			return;
		}

		if (m.empty()) {
			c->Message(Chat::White, "There are no available Rule Sets!");
			return;
		}

		c->Message(Chat::White, "Available Rule Sets:");

		auto rule_set_count = 0;
		auto rule_set_number = 1;

		for (const auto &e : m) {
			c->Message(
				Chat::White,
				fmt::format(
					"Rule Set {} ({})",
					e.second,
					e.first
				).c_str()
			);
		}

		c->Message(
			Chat::White,
			fmt::format(
				"There are {} available Rule Set{}.",
				rule_set_count,
				rule_set_count != 1 ? "s" : ""
			).c_str()
		);
	}
	else if (is_reload) {
		RuleManager::Instance()->LoadRules(&database, RuleManager::Instance()->GetActiveRuleset(), true);
		c->Message(
			Chat::White,
			fmt::format(
				"Active Rule Set {} ({}) has been reloaded.",
				RuleManager::Instance()->GetActiveRuleset(),
				RuleManager::Instance()->GetActiveRulesetID()
			).c_str()
		);
	}
	else if (is_switch) {
		//make sure this is a valid rule set..
		const auto rsid = RuleSetsRepository::GetRuleSetID(database, sep->arg[2]);
		if (rsid < 0) {
			c->Message(
				Chat::White,
				fmt::format(
					"Rule Set '{}' does not exist or is invalid.",
					sep->arg[2]
				).c_str()
			);
			return;
		}

		if (!database.SetVariable("RuleSet", sep->arg[2])) {
			c->Message(Chat::White, "Failed to update variables table to change selected Rule Set.");
			return;
		}

		RuleManager::Instance()->LoadRules(&database, sep->arg[2], true);

		c->Message(
			Chat::White,
			"The selected ruleset has been changed to {} ({}) and reloaded locally.",
			sep->arg[2],
			rsid
		);
	}
	else if (is_load) {
		const auto rsid = RuleSetsRepository::GetRuleSetID(database, sep->arg[2]);
		if (rsid < 0) {
			c->Message(
				Chat::White,
				fmt::format(
					"Rule Set '{}' does not exist or is invalid.",
					sep->arg[2]
				).c_str()
			);
			return;
		}

		RuleManager::Instance()->LoadRules(&database, sep->arg[2], true);
		c->Message(
			Chat::White,
			fmt::format(
				"Loaded Rule Set {} ({}) locally.",
				sep->arg[2],
				rsid
			).c_str()
		);
	}
	else if (is_store) {
		if (arguments == 1) {
			RuleManager::Instance()->SaveRules(&database, "");
			c->Message(Chat::White, "Rules saved.");
		}
		else if (arguments == 2) {
			RuleManager::Instance()->SaveRules(&database, sep->arg[2]);
			const auto prersid = RuleManager::Instance()->GetActiveRulesetID();
			const auto rsid = RuleSetsRepository::GetRuleSetID(database, sep->arg[2]);
			if (rsid < 0) {
				c->Message(Chat::White, "Unable to query Rule Set ID after store.");
			}
			else {
				c->Message(
					Chat::White,
					fmt::format(
						"Stored rules as Rule Set {} ({}).",
						sep->arg[2],
						rsid
					).c_str()
				);

				if (prersid != rsid) {
					c->Message(
						Chat::White,
						fmt::format(
							"Rule Set {} ({}) is now active locally.",
							sep->arg[2],
							rsid
						).c_str()
					);
				}
			}
		}
		else {
			SendRuleSubCommands(c);
			return;
		}
	}
	else if (is_reset) {
		RuleManager::Instance()->ResetRules(true);
		c->Message(
			Chat::White,
			fmt::format(
				"Rule Set {} ({}) has been set to defaults.",
				RuleManager::Instance()->GetActiveRuleset(),
				RuleManager::Instance()->GetActiveRulesetID()
			).c_str()
		);
	}
	else if (is_get) {
		if (arguments == 2) {
			std::string value;
			if (!RuleManager::Instance()->GetRule(sep->arg[2], value)) {
				c->Message(
					Chat::White,
					fmt::format(
						"Unable to find rule '{}'.",
						sep->arg[2]
					).c_str()
				);
			}
			else {
				c->Message(
					Chat::White,
					fmt::format(
						"{} has a value of {}.",
						sep->arg[2],
						value
					).c_str()
				);
			}
		}
		else {
			SendRuleSubCommands(c);
			return;
		}
	}
	else if (is_set) {
		if (arguments == 3) {
			if (!RuleManager::Instance()->SetRule(sep->arg[2], sep->arg[3], nullptr, false, true)) {
				c->Message(
					Chat::White,
					fmt::format(
						"Failed to modify Rule {} to a value of {}.",
						sep->arg[2],
						sep->arg[3]
					).c_str()
				);
			}
			else {
				c->Message(
					Chat::White,
					fmt::format(
						"Rule {} modified locally to a value of {}.",
						sep->arg[2],
						sep->arg[3]
					).c_str()
				);
			}
		}
		else {
			SendRuleSubCommands(c);
			return;
		}
	}
	else if (is_set_db) {
		if (arguments == 3) {
			if (!RuleManager::Instance()->SetRule(sep->arg[2], sep->arg[3], &database, true, true)) {
				c->Message(
					Chat::White,
					fmt::format(
						"Failed to modify Rule {} to a value of {}.",
						sep->arg[2],
						sep->arg[3]
					).c_str()
				);
			}
			else {
				c->Message(
					Chat::White,
					fmt::format(
						"Rule {} modified locally and in database to a value of {}.",
						sep->arg[2],
						sep->arg[3]
					).c_str()
				);
			}
		}
		else {
			SendRuleSubCommands(c);
			return;
		}
	}
	else if (is_list) {
		if (arguments == 1) {
			std::vector<std::string> l;
			if (!RuleManager::Instance()->ListCategories(l)) {
				c->Message(Chat::White, "Failed to list Rule Categories!");
				return;
			}

			if (l.empty()) {
				c->Message(Chat::White, "There are no Rule Categories to list!");
				return;
			}

			c->Message(Chat::White, "Rule Categories:");

			auto rule_category_count = 0;
			auto rule_category_number = 1;

			for (const auto &e : l) {
				c->Message(
					Chat::White,
					fmt::format(
						"Rule Category {} | {}",
						rule_category_number,
						e
					).c_str()
				);

				rule_category_count++;
				rule_category_number++;
			}

			c->Message(
				Chat::White,
				fmt::format(
					"There {} {} available Rule Categor{}.",
					rule_category_count != 1 ? "are" : "is",
					rule_category_count,
					rule_category_count != 1 ? "ies" : "y"
				).c_str()
			);
		}
		else if (arguments == 2) {
			std::string category_name;
			if (std::string("all") != sep->arg[2]) {
				category_name = sep->arg[2];
			}

			std::vector<std::string> l;
			if (!RuleManager::Instance()->ListRules(category_name, l)) {
				c->Message(Chat::White, "Failed to list rules!");
				return;
			}

			c->Message(
				Chat::White,
				fmt::format(
					"Rules in {} Category:",
					category_name
				).c_str()
			);

			auto rule_count = 0;
			auto rule_number = 1;

			for (const auto &e : l) {
				c->Message(
					Chat::White,
					fmt::format(
						"Rule {} | {}",
						rule_number,
						e
					).c_str()
				);

				rule_count++;
				rule_number++;
			}

			c->Message(
				Chat::White,
				fmt::format(
					"There {} {} available Rule{} in the {} Category.",
					rule_count != 1 ? "are" : "is",
					rule_count,
					rule_count != 1 ? "s" : "",
					category_name
				).c_str()
			);
		}
		else {
			SendRuleSubCommands(c);
			return;
		}
	}
	else if (is_values) {
		if (arguments == 2) {
			std::string category_name;
			if (std::string("all") != sep->arg[2]) {
				category_name = sep->arg[2];
			}

			std::vector<std::string> l;
			if (!RuleManager::Instance()->ListRules(category_name, l)) {
				c->Message(Chat::White, "Failed to list rules!");
				return;
			}

			c->Message(
				Chat::White,
				fmt::format(
					"Rule Values in {} Category:",
					category_name
				).c_str()
			);

			auto rule_count = 0;
			auto rule_number = 1;
			std::string rule_value;

			for (const auto &e : l) {
				if (RuleManager::Instance()->GetRule(e, rule_value)) {
					c->Message(
						Chat::White,
						fmt::format(
							"Rule {} | Name: {} Value: {}",
							rule_number,
							e,
							rule_value
						).c_str()
					);

					rule_count++;
					rule_number++;
				}
			}

			c->Message(
				Chat::White,
				fmt::format(
					"There {} {} available Rule{} in the {} Category.",
					rule_count != 1 ? "are" : "is",
					rule_count,
					rule_count != 1 ? "s" : "",
					category_name
				).c_str()
			);
		}
		else {
			SendRuleSubCommands(c);
			return;
		}
	}
}

void SendRuleSubCommands(Client *c)
{
	c->Message(Chat::White, "Usage: #rules listsets - List available rule sets");
	c->Message(Chat::White, "Usage: #rules current - gives the name of the ruleset currently running in this zone");
	c->Message(Chat::White, "Usage: #rules reload - Reload the selected ruleset in this zone");
	c->Message(Chat::White, "Usage: #rules switch [Ruleset Name] - Change the selected ruleset and load it");
	c->Message(
		Chat::White,
		"Usage: #rules load [Ruleset Name] - Load a ruleset in just this zone without changing the selected set"
	);
	c->Message(Chat::White, "Usage: #rules store [Ruleset Name] - Store the running ruleset as the specified name");
	c->Message(Chat::White, "Usage: #rules reset - Reset all rules to their default values");
	c->Message(Chat::White, "Usage: #rules get [Rule] - Get the specified rule's local value");
	c->Message(
		Chat::White,
		"Usage: #rules set [Rule] [Value] - Set the specified rule to the specified value locally only"
	);
	c->Message(
		Chat::White,
		"Usage: #rules setdb [Rule] [Value] - Set the specified rule to the specified value locally and in the DB"
	);
	c->Message(
		Chat::White,
		"Usage: #rules list [Category Name] - List all rules in the specified category (or all categiries if omitted)"
	);
	c->Message(
		Chat::White,
		"Usage: #rules values [Category Name] - List the value of all rules in the specified category"
	);
	return;
}
namespace {
constexpr const char *PVP_ZONE_BUCKET = "pvpzone_active_shortnames";
constexpr const char *PVP_NORMAL_LOOT_BUCKET = "pvpzone_normal_loot_shortnames";
constexpr const char *PVP_RAID_LOOT_BUCKET = "pvpzone_raid_loot_shortnames";
constexpr const char *PVP_XP_BUCKET = "pvpzone_xp_zem";
constexpr const char *PVP_RAID_SPAWN_TIER_BUCKET = "pvpzone_raid_spawn_tier";
constexpr const char *PVP_TIMED_RAID_ZONES_BUCKET = "pvpzone_timed_raid_shortnames";

const std::set<std::string> &AllowedPVPZones()
{
	static const std::set<std::string> zones = {
		"acrylia", "air_instanced", "akheva", "bothunder", "cazicthule", "charasis", "chardok", "citymist",
		"cobaltscar", "codecay", "crushbone", "dreadlands", "eastwastes", "emeraldjungle", "fear_instanced",
		"fungusgrove", "greatdivide", "griegsend", "growthplane", "gukbottom", "hate_instanced", "hohonora",
		"hohonorb", "hole", "iceclad", "kael", "karnor", "katta", "kedge", "kithicor", "mischiefplane",
		"mistmoore", "necropolis", "nightmareb", "permafrost", "poair", "podisease", "poeartha", "poearthb",
		"pofire", "poinnovation", "pojustice", "ponightmare", "postorms", "potactics", "potimea", "potimeb",
		"potorment", "povalor", "powater", "sebilis", "skyfire", "skyshrine", "sleeper", "soldungb", "solrotower",
		"sseru", "ssratemple", "templeveeshan", "thedeep", "thurgadinb", "timorous", "umbral", "unrest",
		"veeshan", "veksar", "velketor", "vexthal", "wakening", "westwastes"
	};
	return zones;
}

const std::set<std::string> &PlanesOfPowerPVPZones()
{
	static const std::set<std::string> zones = {
		"bothunder", "codecay", "hohonora", "hohonorb", "nightmareb", "poair", "podisease",
		"poeartha", "poearthb", "pofire", "poinnovation", "pojustice", "ponightmare", "postorms",
		"potactics", "potimea", "potimeb", "potorment", "povalor", "powater", "solrotower"
	};
	return zones;
}

std::set<std::string> PVPZonesThroughTier(const std::string &tier)
{
	std::set<std::string> zones;
	for (const auto &short_name : AllowedPVPZones()) {
		// Veksar opened after Planes of Power and remains an individual toggle.
		if (short_name == "veksar") {
			continue;
		}
		if (tier == "pop" || !PlanesOfPowerPVPZones().count(short_name)) {
			zones.insert(short_name);
		}
	}
	return zones;
}

std::set<std::string> LoadActivePVPZones()
{
	std::set<std::string> active;
	std::stringstream values(DataBucket::GetData(PVP_ZONE_BUCKET));
	std::string value;
	while (std::getline(values, value, ',')) {
		value = Strings::ToLower(value);
		if (AllowedPVPZones().count(value)) active.insert(value);
	}
	return active;
}

void SavePVPZoneSet(const char *bucket, const std::set<std::string> &zones)
{
	std::string value;
	for (const auto &short_name : zones) {
		if (!value.empty()) value += ",";
		value += short_name;
	}
	if (value.empty()) DataBucket::DeleteData(bucket);
	else DataBucket::SetData(bucket, value);
}

void SaveActivePVPZones(const std::set<std::string> &active)
{
	SavePVPZoneSet(PVP_ZONE_BUCKET, active);
}

std::set<std::string> LoadPVPZoneSet(const char *bucket)
{
	std::set<std::string> zones;
	std::stringstream values(DataBucket::GetData(bucket));
	std::string value;
	while (std::getline(values, value, ',')) {
		value = Strings::ToLower(value);
		if (AllowedPVPZones().count(value)) zones.insert(value);
	}
	return zones;
}

bool ParsePVPToggle(const char *value, bool &enabled)
{
	if (!value) return false;
	if (!strcasecmp(value, "on") || !strcasecmp(value, "true") || !strcmp(value, "1")) {
		enabled = true;
		return true;
	}
	if (!strcasecmp(value, "off") || !strcasecmp(value, "false") || !strcmp(value, "0")) {
		enabled = false;
		return true;
	}
	return false;
}

std::map<std::string, int> LoadPVPZoneXP()
{
	std::map<std::string, int> values;
	std::stringstream entries(DataBucket::GetData(PVP_XP_BUCKET));
	std::string entry;
	while (std::getline(entries, entry, ',')) {
		const auto separator = entry.find('=');
		if (separator == std::string::npos) continue;
		const auto short_name = Strings::ToLower(entry.substr(0, separator));
		const auto zem_value = entry.substr(separator + 1);
		const int zem = Strings::IsNumber(zem_value) ? Strings::ToInt(zem_value) : 0;
		if (AllowedPVPZones().count(short_name) && zem >= 110 && zem <= 150) values[short_name] = zem;
	}
	return values;
}

void SavePVPZoneXP(const std::map<std::string, int> &values)
{
	std::string data;
	for (const auto &[short_name, zem] : values) {
		if (!data.empty()) data += ",";
		data += fmt::format("{}={}", short_name, zem);
	}
	if (data.empty()) DataBucket::DeleteData(PVP_XP_BUCKET);
	else DataBucket::SetData(PVP_XP_BUCKET, data);
}

void ShowPVPZoneList(Client *c)
{
	const auto active = LoadActivePVPZones();
	c->Message(Chat::Lime, fmt::format("Active PVP zones ({}/{}):", active.size(), AllowedPVPZones().size()).c_str());
	if (active.empty()) {
		c->Message(Chat::White, "None");
		return;
	}
	std::string line;
	for (const auto &short_name : active) {
		if (!line.empty() && line.size() + short_name.size() + 2 > 90) {
			c->Message(Chat::White, line.c_str());
			line.clear();
		}
		if (!line.empty()) line += ", ";
		line += short_name;
	}
	if (!line.empty()) c->Message(Chat::White, line.c_str());
}

void ShowPVPZoneStatus(Client *c)
{
	const auto active = LoadActivePVPZones();
	c->Message(Chat::White, fmt::format("Active PVP zones: {} of {}", active.size(), AllowedPVPZones().size()).c_str());
}

void ShowPVPZoneUsage(Client *c)
{
	c->Message(Chat::White, "#pvpzone <shortname> <on|off>");
	c->Message(Chat::White, "#pvpzone <shortname> xp <off|110-150>");
	c->Message(Chat::White, "#pvpzone <shortname> <1|normal|2|raid|3|both> <on|off>");
	c->Message(Chat::White, "#pvpzone <shortname> status");
	c->Message(Chat::White, "#pvpzone status | list | all off");
	c->Message(Chat::White, "#pvpzone quakeon | quakeoff (automatic timer only; server-wide)");
	c->Message(Chat::White, "#pvpzone <luclin|pop> <on|off> (batch zone access and Guild 1 timed raid spawns)");
}
}

void command_pvpzone(Client *c, const Seperator *sep)
{
	if (!c || sep->argnum < 1 || !strcasecmp(sep->arg[1], "help")) {
		if (c) ShowPVPZoneUsage(c);
		return;
	}

	if (!strcasecmp(sep->arg[1], "quakeon") || !strcasecmp(sep->arg[1], "quakeoff")) {
		if (c->Admin() < AccountStatus::GMAdmin || sep->argnum != 1) {
			c->Message(Chat::Red, "GM Admin required. Usage: #pvpzone quakeon | quakeoff");
			return;
		}
		if (!strcasecmp(sep->arg[1], "quakeoff")) {
			auto result = database.QueryDatabase("DELETE FROM data_buckets WHERE `key` = 'pvpzone_quake_next'");
			c->Message(result.Success() ? Chat::Yellow : Chat::Red, result.Success()
				? "Automatic quake timer disabled. Current quake and boss expiry timers are unchanged."
				: "Could not disable the automatic quake timer.");
			return;
		}
		if (!RuleB(Quarm, EnableQuakes)) {
			c->Message(Chat::Red, "EnableQuakes is disabled. Enable that rule in world and zone before starting the timer.");
			return;
		}
		auto current = database.QueryDatabase("SELECT value FROM data_buckets WHERE `key` = 'pvpzone_quake_next' LIMIT 1");
		if (!current.Success()) {
			c->Message(Chat::Red, "Could not read the automatic quake timer.");
			return;
		}
		if (current.RowCount() > 0) {
			c->Message(Chat::Yellow, "Automatic quake timer is already enabled; its deadline was not reset.");
			return;
		}
		const int minimum = RuleI(Quarm, QuakeMinVariance);
		const int maximum = RuleI(Quarm, QuakeMaxVariance);
		if (minimum <= 0 || maximum < minimum || maximum > 4294967) {
			c->Message(Chat::Red, "Invalid quake variance settings; timer not started.");
			return;
		}
		const uint32 delay = zone->random.Int(minimum, maximum);
		const uint32 deadline = Timer::GetTimeSeconds() + delay;
		auto result = database.QueryDatabase(StringFormat("INSERT INTO data_buckets (`key`, value, expires) VALUES ('pvpzone_quake_next', '%u', 0)", deadline));
		if (result.Success()) {
			c->Message(Chat::Yellow, "Automatic quake timer enabled: first trigger in %u hours %u minutes. No quake triggered now.", delay / 3600, (delay % 3600) / 60);
		} else {
			c->Message(Chat::Red, "Could not enable the automatic quake timer.");
		}
		return;
	}
	if (!strcasecmp(sep->arg[1], "luclin") || !strcasecmp(sep->arg[1], "pop")) {
		bool enabled = false;
		if (c->Admin() < AccountStatus::GMAdmin || sep->argnum != 2 || !ParsePVPToggle(sep->arg[2], enabled)) {
			c->Message(Chat::Red, "GM Admin required. Usage: #pvpzone <luclin|pop> <on|off>");
			return;
		}
		const auto tier = Strings::ToLower(sep->arg[1]);
		const auto tier_zones = PVPZonesThroughTier(tier);
		if (enabled) {
			DataBucket::SetData(PVP_RAID_SPAWN_TIER_BUCKET, tier);
			SavePVPZoneSet(PVP_TIMED_RAID_ZONES_BUCKET, tier_zones);

			auto active = LoadActivePVPZones();
			active.insert(tier_zones.begin(), tier_zones.end());
			SaveActivePVPZones(active);

			auto result = database.QueryDatabase(
				"DELETE rt FROM respawn_times rt "
				"INNER JOIN spawn2 s2 ON s2.id = rt.id "
				"WHERE rt.guild_id = 1 AND s2.raid_target_spawnpoint = 1");
			const char *expansion_name = tier == "pop" ? "Planes of Power" : "Luclin";
			if (result.Success()) {
				c->Message(
					Chat::Yellow,
					"Enabled %zu PVP zones through %s with Guild 1 timed raid spawns. Dormant timers were cleared; later expansions remain quake-only.",
					tier_zones.size(), expansion_name);
			} else {
				c->Message(
					Chat::Red,
					"Enabled %zu PVP zones through %s, but dormant raid timers could not be cleared.",
					tier_zones.size(), expansion_name);
			}
		} else {
			DataBucket::DeleteData(PVP_RAID_SPAWN_TIER_BUCKET);
			DataBucket::DeleteData(PVP_TIMED_RAID_ZONES_BUCKET);

			auto active = LoadActivePVPZones();
			for (const auto &short_name : tier_zones) {
				active.erase(short_name);
			}
			SaveActivePVPZones(active);
			c->Message(
				Chat::Yellow,
				"Disabled %zu PVP zones through %s. Guild 1 raid targets are quake-only.",
				tier_zones.size(), tier == "pop" ? "Planes of Power" : "Luclin");
		}
		c->Message(Chat::White, "Active zone servers will notice the change within five seconds; use #repop if an immediate fresh spawn cycle is needed.");
		return;
	}
	if (!strcasecmp(sep->arg[1], "status")) {
		ShowPVPZoneStatus(c);
		const auto raid_tier = Strings::ToLower(DataBucket::GetData(PVP_RAID_SPAWN_TIER_BUCKET));
		c->Message(Chat::White, fmt::format("Guild 1 timed raid spawns: {}.", raid_tier.empty() ? "off" : (raid_tier == "pop" ? "PoP and earlier" : "Luclin and earlier")).c_str());
		auto result = database.QueryDatabase("SELECT value FROM data_buckets WHERE `key` = 'pvpzone_quake_next' LIMIT 1");
		if (result.Success() && result.RowCount() > 0) {
			auto row = result.begin();
			const uint32 deadline = row[0] ? static_cast<uint32>(strtoul(row[0], nullptr, 10)) : 0;
			const uint32 now = Timer::GetTimeSeconds();
			const uint32 remaining = deadline > now ? deadline - now : 0;
			c->Message(Chat::White, "Automatic quakes: ON. Next trigger in %u hours %u minutes.", remaining / 3600, (remaining % 3600) / 60);
		} else {
			c->Message(Chat::White, result.Success() ? "Automatic quakes: OFF." : "Automatic quake status unavailable.");
		}
		return;
	}
	if (!strcasecmp(sep->arg[1], "list")) {
		ShowPVPZoneList(c);
		return;
	}
	if (!strcasecmp(sep->arg[1], "all") && sep->argnum >= 2 && !strcasecmp(sep->arg[2], "off")) {
		SaveActivePVPZones({});
		DataBucket::DeleteData(PVP_RAID_SPAWN_TIER_BUCKET);
		DataBucket::DeleteData(PVP_TIMED_RAID_ZONES_BUCKET);
		c->Message(Chat::Yellow, "All PVP zones and Guild 1 timed raid spawns are now disabled.");
		return;
	}

	if (!strcasecmp(sep->arg[1], "loot")) {
		c->Message(Chat::Red, "Loot settings require a zone shortname; for example: #pvpzone fear_instanced 1 on");
		return;
	}

	if (!strcasecmp(sep->arg[1], "xp")) {
		c->Message(Chat::Red, "XP settings require a zone shortname; for example: #pvpzone fear_instanced xp 125");
		return;
	}

	const auto short_name = Strings::ToLower(sep->arg[1]);
	if (!AllowedPVPZones().count(short_name)) {
		c->Message(Chat::Red, fmt::format("{} is not an approved PVP zone shortname.", short_name).c_str());
		return;
	}
	const auto normal_loot = LoadPVPZoneSet(PVP_NORMAL_LOOT_BUCKET);
	const auto raid_loot = LoadPVPZoneSet(PVP_RAID_LOOT_BUCKET);
	const auto xp_zems = LoadPVPZoneXP();
	if (sep->argnum >= 2 && !strcasecmp(sep->arg[2], "status")) {
		const auto active = LoadActivePVPZones();
		const auto xp = xp_zems.find(short_name);
		const auto xp_status = xp == xp_zems.end() ? std::string("off") : fmt::format("{} ZEM", xp->second);
		c->Message(Chat::White, fmt::format("{}: access {}, normal loot {}, raid loot {}, XP {}", short_name, active.count(short_name) ? "on" : "off", normal_loot.count(short_name) ? "on" : "off", raid_loot.count(short_name) ? "on" : "off", xp_status).c_str());
		return;
	}
	if (sep->argnum >= 2 && !strcasecmp(sep->arg[2], "xp")) {
		if (sep->argnum < 3) {
			ShowPVPZoneUsage(c);
			return;
		}
		auto values = xp_zems;
		if (!strcasecmp(sep->arg[3], "off")) {
			values.erase(short_name);
			SavePVPZoneXP(values);
			c->Message(Chat::Yellow, fmt::format("{} PVP experience bonus is now off.", short_name).c_str());
			return;
		}
		const int zem = Strings::IsNumber(sep->arg[3]) ? Strings::ToInt(sep->arg[3]) : 0;
		if (zem < 110 || zem > 150) {
			c->Message(Chat::Red, "PVP final ZEM must be from 110 through 150, or off.");
			return;
		}
		values[short_name] = zem;
		SavePVPZoneXP(values);
		c->Message(Chat::Yellow, fmt::format("{} PVP experience is enabled with a final ZEM of {}.", short_name, zem).c_str());
		return;
	}
	const bool normal = sep->argnum >= 2 && (!strcmp(sep->arg[2], "1") || !strcasecmp(sep->arg[2], "normal"));
	const bool raid = sep->argnum >= 2 && (!strcmp(sep->arg[2], "2") || !strcasecmp(sep->arg[2], "raid"));
	const bool both = sep->argnum >= 2 && (!strcmp(sep->arg[2], "3") || !strcasecmp(sep->arg[2], "both"));
	if (normal || raid || both) {
		bool enabled = false;
		if (sep->argnum < 3 || !ParsePVPToggle(sep->arg[3], enabled)) {
			ShowPVPZoneUsage(c);
			return;
		}
		if (normal || both) {
			auto zones = normal_loot;
			if (enabled) zones.insert(short_name); else zones.erase(short_name);
			SavePVPZoneSet(PVP_NORMAL_LOOT_BUCKET, zones);
		}
		if (raid || both) {
			auto zones = raid_loot;
			if (enabled) zones.insert(short_name); else zones.erase(short_name);
			SavePVPZoneSet(PVP_RAID_LOOT_BUCKET, zones);
		}
		c->Message(Chat::Yellow, fmt::format("{} {} double loot is now {}.", short_name, both ? "normal and raid" : (raid ? "raid" : "normal"), enabled ? "on" : "off").c_str());
		return;
	}
	bool enabled = false;
	if (sep->argnum < 2 || !ParsePVPToggle(sep->arg[2], enabled)) {
		ShowPVPZoneUsage(c);
		return;
	}
	auto active = LoadActivePVPZones();
	if (enabled) active.insert(short_name); else active.erase(short_name);
	SaveActivePVPZones(active);
	c->Message(Chat::Yellow, fmt::format("PVP zone {} is now {}.", short_name, enabled ? "enabled" : "disabled").c_str());
}
