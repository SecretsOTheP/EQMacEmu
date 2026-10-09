#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace QuakeTiming {

constexpr uint32_t MaxTimerSeconds = std::numeric_limits<uint32_t>::max() / 1000;

inline bool ParseDeadline(const std::string &value, uint32_t &deadline)
{
	deadline = 0;
	if (value.empty()) return false;
	uint64_t parsed = 0;
	for (char c : value) {
		if (c < '0' || c > '9') return false;
		parsed = parsed * 10 + (c - '0');
		if (parsed > std::numeric_limits<uint32_t>::max()) return false;
	}
	deadline = static_cast<uint32_t>(parsed);
	return deadline != 0;
}

inline uint32_t MillisecondsUntil(uint32_t deadline, uint32_t now)
{
	return deadline > now ? std::min(deadline - now, MaxTimerSeconds) * 1000 : 1;
}

struct Notification {
	bool apply = false;
	uint32_t spawn_delay_ms = 0;
	uint32_t expiry_delay_ms = 0;
};

inline Notification PlanNotification(uint32_t start, uint32_t duration, uint32_t last_applied, uint32_t now)
{
	if (!start || start <= last_applied || !duration || duration > MaxTimerSeconds) return {};
	const uint64_t end = static_cast<uint64_t>(start) + duration;
	if (end <= now || end - now > MaxTimerSeconds) return {};
	return {true, start > now ? (start - now) * 1000 : 0,
		static_cast<uint32_t>(end - now) * 1000};
}

} // namespace QuakeTiming
