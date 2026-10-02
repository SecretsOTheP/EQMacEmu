#pragma once

#include <set>
#include <string>

namespace PVPZoneTiers {

struct QuakeScope {
	bool luclin_and_earlier;
	bool pop;
};

inline QuakeScope ParseQuakeScope(const std::string &scope, const std::string &legacy_tier)
{
	if (scope == "both") return { true, true };
	if (scope == "luclin") return { true, false };
	if (scope == "pop") return { false, true };
	if (scope == "off") return { false, false };
	// Preserve the existing tier selection until an independent switch is used.
	return { true, legacy_tier == "pop" };
}

inline const char *QuakeScopeValue(const QuakeScope &scope)
{
	if (scope.luclin_and_earlier) return scope.pop ? "both" : "luclin";
	return scope.pop ? "pop" : "off";
}

inline const std::set<std::string> &PlanesOfPowerZones()
{
	static const std::set<std::string> zones = {
		"bothunder", "codecay", "hohonora", "hohonorb", "nightmareb", "poair", "podisease",
		"poeartha", "poearthb", "pofire", "poinnovation", "pojustice", "ponightmare", "postorms",
		"potactics", "potimea", "potimeb", "potorment", "povalor", "powater", "solrotower"
	};
	return zones;
}

} // namespace PVPZoneTiers
