#include "../client.h"

#include "../../common/strings.h"

#include <array>
#include <ctime>
#include <string>
#include <vector>

namespace {
constexpr uint32 TimeControllerNPCTypeID = 223077;
constexpr uint32 TimeBZoneID = 223;
constexpr uint8 TimePhaseCount = 6;

const std::array<std::vector<const char *>, TimePhaseCount> TimeBosses = {{
	{"Terlok of Earth", "Neimon of Air", "Rythor of the Undead", "Anar of Water", "Kazrok of Fire"},
	{"Windshapen Warlord of Air", "Earthen Overseer", "Ralthos Enrok", "War Shapen Emissary", "Gutripping War Beast"},
	{
		"A Ferocious Warboar", "Deathbringer Blackheart", "Xeroan Xi`Geruonask", "Kraksmaal Fir`Dethsin",
		"A Deadly Warboar", "Deathbringer Skullsmash", "Herlsoakian", "Sinrunal Gorgedreal",
		"Deathbringer Rianit", "A Needletusk Warboar", "Xerskel Gerodnsal", "Dersool Fal`Giersnaol",
		"Undead Squad Leader", "Dark Knight of Terris", "Champion of Torment", "Dreamwarp",
		"Avatar of the Elements", "Supernatural Guardian"
	},
	{"Saryrn", "Terris Thule", "Tallon Zek", "Vallon Zek"},
	{"Cazic Thule", "Bertoxxulous", "Rallos Zek", "Innoruuk"},
	{"Quarm"}
}};

uint8 TimeTimerIndex(uint8 phase, size_t boss_index)
{
	if (phase <= 3) {
		return phase - 1;
	}
	if (phase == 4) {
		return static_cast<uint8>(boss_index + 3);
	}
	if (phase == 5) {
		return static_cast<uint8>(boss_index + 7);
	}
	return 11;
}

uint64 TimeDeadline(const std::vector<std::string> &timers, uint8 phase, size_t boss_index)
{
	const auto timer_index = TimeTimerIndex(phase, boss_index);
	if (timer_index >= timers.size()) {
		return 0;
	}
	return Strings::ToUnsignedBigInt(timers[timer_index]) * 100;
}

bool TimeBossAvailable(
	const std::vector<std::string> &kills,
	const std::vector<std::string> &timers,
	uint8 phase,
	size_t boss_index,
	uint64 now
)
{
	if (phase == 0 || phase > kills.size() || boss_index >= kills[phase - 1].size()) {
		return false;
	}
	if (kills[phase - 1][boss_index] != '1') {
		return true;
	}
	const auto deadline = TimeDeadline(timers, phase, boss_index);
	return deadline == 0 || deadline <= now;
}

std::string TimeRemaining(uint64 deadline, uint64 now)
{
	if (deadline <= now) {
		return "now";
	}
	return Strings::SecondsToTime(static_cast<int>(deadline - now));
}

uint8 TimeCurrentPhase(
	const std::vector<std::string> &kills,
	const std::vector<std::string> &timers,
	uint64 now
)
{
	for (uint8 phase = 1; phase <= TimePhaseCount; ++phase) {
		for (size_t boss = 0; boss < TimeBosses[phase - 1].size(); ++boss) {
			if (TimeBossAvailable(kills, timers, phase, boss, now)) {
				return phase;
			}
		}
	}
	return 0;
}
}

void command_timelockout(Client *c, const Seperator *sep)
{
	if (!c) {
		return;
	}

	uint8 requested_phase = 0;
	if (sep->argnum >= 1) {
		if (!sep->IsNumber(1)) {
			c->Message(Chat::White, "Usage: #timelockout [phase 1-6]");
			return;
		}
		const auto parsed_phase = Strings::ToUnsignedInt(sep->arg[1]);
		if (parsed_phase < 1 || parsed_phase > TimePhaseCount) {
			c->Message(Chat::Red, "Plane of Time phases are numbered 1 through 6.");
			return;
		}
		requested_phase = static_cast<uint8>(parsed_phase);
	}

	auto player_save = database.QueryDatabase(fmt::format(
		"SELECT `name`, `value`, `expdate` FROM `quest_globals` "
		"WHERE `charid` = {} AND `npcid` = 0 AND `zoneid` = 0 "
		"AND `name` IN ('time_instance', 'time_instance_guild')",
		c->CharacterID()
	));
	if (!player_save.Success()) {
		c->Message(Chat::Red, "Your Plane of Time timeline could not be read. Please try again.");
		return;
	}

	uint32 instance_id = 0;
	uint32 saved_guild_id = 0;
	uint64 player_expiration = 0;
	for (auto row : player_save) {
		const std::string name = row[0] ? row[0] : "";
		if (name == "time_instance") {
			instance_id = Strings::ToUnsignedInt(row[1] ? row[1] : "0");
			player_expiration = Strings::ToUnsignedBigInt(row[2] ? row[2] : "0");
		} else if (name == "time_instance_guild") {
			saved_guild_id = Strings::ToUnsignedInt(row[1] ? row[1] : "0");
		}
	}

	const uint64 now = static_cast<uint64>(std::time(nullptr));
	if (instance_id == 0 || (player_expiration > 0 && player_expiration <= now)) {
		c->Message(Chat::Yellow, "You are not currently bound to a Plane of Time timeline.");
		return;
	}

	auto timeline = database.QueryDatabase(fmt::format(
		"SELECT `name`, `value`, `expdate` FROM `quest_globals` "
		"WHERE `charid` = 0 AND `npcid` = {} AND `zoneid` = {} "
		"AND `name` IN ('time_kills_{}', 'time_timers_{}', 'time_guild_{}', 'time_expires_{}')",
		TimeControllerNPCTypeID,
		TimeBZoneID,
		instance_id,
		instance_id,
		instance_id,
		instance_id
	));
	if (!timeline.Success()) {
		c->Message(Chat::Red, "Your Plane of Time timeline could not be read. Please try again.");
		return;
	}

	std::string kill_data;
	std::string timer_data;
	uint32 owner_guild_id = 0;
	uint64 timeline_expiration = 0;
	uint64 retirement_deadline = 0;
	for (auto row : timeline) {
		const std::string name = row[0] ? row[0] : "";
		if (name == fmt::format("time_kills_{}", instance_id)) {
			kill_data = row[1] ? row[1] : "";
			timeline_expiration = Strings::ToUnsignedBigInt(row[2] ? row[2] : "0");
		} else if (name == fmt::format("time_timers_{}", instance_id)) {
			timer_data = row[1] ? row[1] : "";
		} else if (name == fmt::format("time_guild_{}", instance_id)) {
			owner_guild_id = Strings::ToUnsignedInt(row[1] ? row[1] : "0");
		} else if (name == fmt::format("time_expires_{}", instance_id)) {
			retirement_deadline = Strings::ToUnsignedBigInt(row[1] ? row[1] : "0");
		}
	}

	if (kill_data.empty() || timer_data.empty()) {
		c->Message(Chat::Red, "Your saved thread of time has faded and can no longer be restored.");
		return;
	}

	const auto kills = Strings::Split(kill_data, ';');
	const auto timers = Strings::Split(timer_data, ';');
	if (kills.size() != TimePhaseCount || timers.size() != 12) {
		c->Message(Chat::Red, "Your Plane of Time timeline is damaged. Please contact support.");
		return;
	}
	for (uint8 phase = 1; phase <= TimePhaseCount; ++phase) {
		if (kills[phase - 1].size() != TimeBosses[phase - 1].size()) {
			c->Message(Chat::Red, "Your Plane of Time timeline is damaged. Please contact support.");
			return;
		}
	}

	if (owner_guild_id == 0) {
		owner_guild_id = saved_guild_id;
	}
	const auto current_phase = TimeCurrentPhase(kills, timers, now);

	c->Message(Chat::Lime, "=== Plane of Time Timeline ===");
	c->Message(Chat::White, "Timeline: %u", instance_id);
	if (owner_guild_id > 0) {
		c->Message(Chat::White, "Guild instance: %u", owner_guild_id);
		if (saved_guild_id > 0 && saved_guild_id != owner_guild_id) {
			c->Message(Chat::Red, "Warning: Your character is bound to a different guild's copy of this timeline.");
		}
	} else {
		c->Message(Chat::Yellow, "Guild instance: Legacy save; ownership will be recorded when restored.");
	}
	if (retirement_deadline > now) {
		c->Message(Chat::White, "Timeline retires in: %s", TimeRemaining(retirement_deadline, now).c_str());
	} else if (retirement_deadline > 0) {
		c->Message(Chat::Yellow, "This timeline will retire after the active run ends. Your next entry will begin a fresh timeline.");
	} else if (timeline_expiration > now) {
		c->Message(Chat::White, "Timeline record expires in: %s", TimeRemaining(timeline_expiration, now).c_str());
	}

	if (requested_phase > 0) {
		c->Message(Chat::Yellow, "Phase %u encounter status:", requested_phase);
		for (size_t boss = 0; boss < TimeBosses[requested_phase - 1].size(); ++boss) {
			if (current_phase > 0 && requested_phase > current_phase) {
				c->Message(Chat::White, "%s: Not yet accessible", TimeBosses[requested_phase - 1][boss]);
				continue;
			}
			if (TimeBossAvailable(kills, timers, requested_phase, boss, now)) {
				c->Message(Chat::Lime, "%s: Available", TimeBosses[requested_phase - 1][boss]);
				continue;
			}
			const auto deadline = TimeDeadline(timers, requested_phase, boss);
			c->Message(
				Chat::Yellow,
				"%s: Defeated - available again in %s",
				TimeBosses[requested_phase - 1][boss],
				TimeRemaining(deadline, now).c_str()
			);
		}
		return;
	}

	for (uint8 phase = 1; phase <= TimePhaseCount; ++phase) {
		uint32 available = 0;
		uint64 next_deadline = 0;
		for (size_t boss = 0; boss < TimeBosses[phase - 1].size(); ++boss) {
			if (TimeBossAvailable(kills, timers, phase, boss, now)) {
				++available;
			} else {
				const auto deadline = TimeDeadline(timers, phase, boss);
				if (next_deadline == 0 || deadline < next_deadline) {
					next_deadline = deadline;
				}
			}
		}

		if (current_phase > 0 && phase > current_phase) {
			c->Message(Chat::White, "Phase %u: Not yet accessible", phase);
		} else if (available > 0) {
			c->Message(Chat::Lime, "Phase %u: %u of %zu encounters available", phase, available, TimeBosses[phase - 1].size());
		} else if (next_deadline > now) {
			c->Message(Chat::Yellow, "Phase %u: Complete - first encounter returns in %s", phase, TimeRemaining(next_deadline, now).c_str());
		} else {
			c->Message(Chat::Yellow, "Phase %u: Complete", phase);
		}
	}
	c->Message(Chat::White, "Use #timelockout <1-6> to list a phase's encounters.");
}
