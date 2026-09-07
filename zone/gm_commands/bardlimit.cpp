#include "../client.h"
#include "../command.h"
#include "../data_bucket.h"

#include "../../common/strings.h"
#include "../../common/rulesys.h"

namespace {
void BardLimitUsage(Client* c)
{
	c->Message(Chat::White, "#bardlimit <shortname> <0-15> - Set the allowed mob count (0 disables)");
	c->Message(Chat::White, "#bardlimit <shortname> status | default");
	c->Message(Chat::White, "#bardlimit list");
}
}

void command_bardlimit(Client* c, const Seperator* sep)
{
	if (!c || sep->argnum < 1 || !strcasecmp(sep->arg[1], "help")) {
		if (c) BardLimitUsage(c);
		return;
	}

	if (!strcasecmp(sep->arg[1], "list")) {
		auto results = database.QueryDatabase(
			"SELECT SUBSTRING(`key`, 11), value FROM data_buckets "
			"WHERE LEFT(`key`, 10) = 'bardlimit_' ORDER BY `key`"
		);
		if (!results.Success()) {
			c->Message(Chat::Red, "Unable to list bard limits.");
			return;
		}
		if (results.RowCount() == 0) {
			c->Message(Chat::White, "No per-zone bard limits are configured. Global default: %d.", RuleI(Quarm, BardInstagibPullLimit));
			return;
		}
		for (auto row : results) {
			c->Message(Chat::White, "%s: %s", row[0], !strcmp(row[1], "0") ? "disabled" : StringFormat("%s allowed; summons at %d", row[1], atoi(row[1]) + 1).c_str());
		}
		return;
	}

	const auto short_name = Strings::ToLower(sep->arg[1]);
	if (database.GetZoneID(short_name.c_str()) == 0) {
		c->Message(Chat::Red, "Unknown zone short name: %s", short_name.c_str());
		return;
	}
	const auto key = "bardlimit_" + short_name;
	if (sep->argnum != 2) {
		BardLimitUsage(c);
		return;
	}

	if (!strcasecmp(sep->arg[2], "status")) {
		const auto value = DataBucket::GetData(key);
		if (value.empty()) {
			c->Message(Chat::White, "%s uses the global bard limit: %d allowed; summons at %d.", short_name.c_str(), RuleI(Quarm, BardInstagibPullLimit), RuleI(Quarm, BardInstagibPullLimit) + 1);
		} else if (value == "0") {
			c->Message(Chat::White, "%s bard limit is disabled.", short_name.c_str());
		} else {
			const int limit = Strings::ToInt(value);
			c->Message(Chat::White, "%s allows %d mobs; summoning starts at %d.", short_name.c_str(), limit, limit + 1);
		}
		return;
	}

	if (!strcasecmp(sep->arg[2], "default")) {
		if (DataBucket::DeleteData(key)) {
			c->Message(Chat::Yellow, "%s now uses the global bard limit of %d.", short_name.c_str(), RuleI(Quarm, BardInstagibPullLimit));
		} else {
			c->Message(Chat::Red, "Unable to clear the bard limit for %s.", short_name.c_str());
		}
		return;
	}

	if (!Strings::IsNumber(sep->arg[2])) {
		BardLimitUsage(c);
		return;
	}
	const int limit = Strings::ToInt(sep->arg[2]);
	if (limit < 0 || limit > 15) {
		c->Message(Chat::Red, "Bard limit must be between 0 and 15.");
		return;
	}
	DataBucket::SetData(key, std::to_string(limit));
	c->Message(Chat::Yellow, limit == 0 ? "%s bard limit disabled." : "%s now allows %d mobs; summoning starts at %d.", short_name.c_str(), limit, limit + 1);
}
