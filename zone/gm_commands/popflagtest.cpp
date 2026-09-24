#include "../client.h"

#include <array>

namespace {
constexpr std::array<uint32, 8> PopProgressionZoneFlags = { 200, 207, 208, 209, 210, 211, 212, 214 };
constexpr std::array<uint32, 4> PopElementalZoneFlags = { 215, 216, 217, 218 };
constexpr std::array<uint32, 1> PopTimeZoneFlags = { 219 };

void PopFlagTestUsage(Client* c)
{
	c->Message(Chat::White, "Usage: target a character, then #popflagtest <grant|clear> <progression|elemental|time|all>");
}

template <size_t N>
void SetPopFlags(Client* target, const std::array<uint32, N>& zone_flags, bool grant)
{
	for (const auto zone_id : zone_flags) {
		if (grant) {
			target->SetZoneFlag(zone_id);
		}
		else {
			target->ClearZoneFlag(zone_id);
		}
	}
}

bool SetPopFlagTier(Client* target, const char* tier, bool grant)
{
	if (!strcasecmp(tier, "progression")) {
		SetPopFlags(target, PopProgressionZoneFlags, grant);
	}
	else if (!strcasecmp(tier, "elemental")) {
		SetPopFlags(target, PopElementalZoneFlags, grant);
	}
	else if (!strcasecmp(tier, "time")) {
		SetPopFlags(target, PopTimeZoneFlags, grant);
	}
	else if (!strcasecmp(tier, "all")) {
		SetPopFlags(target, PopProgressionZoneFlags, grant);
		SetPopFlags(target, PopElementalZoneFlags, grant);
		SetPopFlags(target, PopTimeZoneFlags, grant);
	}
	else {
		return false;
	}

	return true;
}
}

void command_popflagtest(Client* c, const Seperator* sep)
{
	if (!c || sep->argnum != 2 || !strcasecmp(sep->arg[1], "help")) {
		if (c) {
			PopFlagTestUsage(c);
		}
		return;
	}

	auto* target = c->GetTarget();
	if (!target || !target->IsClient()) {
		c->Message(Chat::Red, "Target a character first.");
		return;
	}

	const bool grant = !strcasecmp(sep->arg[1], "grant");
	if (!grant && strcasecmp(sep->arg[1], "clear")) {
		PopFlagTestUsage(c);
		return;
	}

	auto* client_target = target->CastToClient();
	if (!SetPopFlagTier(client_target, sep->arg[2], grant)) {
		PopFlagTestUsage(c);
		return;
	}

	c->Message(
		Chat::Yellow,
		"%s PoP %s zone flags %s.",
		client_target->GetCleanName(),
		sep->arg[2],
		grant ? "granted" : "cleared"
	);
}
