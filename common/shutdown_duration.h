#ifndef EQEMU_SHUTDOWN_DURATION_H
#define EQEMU_SHUTDOWN_DURATION_H

#include "types.h"
#include <cctype>
#include <ctime>
#include <string>
#include <vector>

// Duration handling shared by #worldshutdown, the world console and the world:shutdown CLI.
namespace ShutdownDuration {

	// 30 days. WorldShutDown_Struct carries milliseconds in a uint32, which tops out near 49 days.
	constexpr uint32 MaxSeconds = 30 * 86400;

	// Accepts "90", "90s", "15m", "1h30m", "2d 4h", "1d2h3m4s" and long unit names
	// ("2 hours 30 minutes"). A bare number is seconds, which keeps the old syntax working.
	inline bool Parse(const std::string &in, uint32 &out_seconds)
	{
		uint64 total = 0;
		bool   any   = false;
		size_t i     = 0;

		while (i < in.size()) {
			if (isspace(static_cast<unsigned char>(in[i])) || in[i] == ',') {
				i++;
				continue;
			}

			if (!isdigit(static_cast<unsigned char>(in[i]))) {
				return false;
			}

			uint64 value = 0;
			while (i < in.size() && isdigit(static_cast<unsigned char>(in[i]))) {
				value = value * 10 + (in[i] - '0');
				if (value > MaxSeconds) {
					return false;
				}
				i++;
			}

			while (i < in.size() && isspace(static_cast<unsigned char>(in[i]))) {
				i++;
			}

			std::string unit;
			while (i < in.size() && isalpha(static_cast<unsigned char>(in[i]))) {
				unit += static_cast<char>(tolower(static_cast<unsigned char>(in[i])));
				i++;
			}

			uint64 scale;
			if (unit.empty() || unit == "s" || unit == "sec" || unit == "secs" || unit == "second" || unit == "seconds") {
				scale = 1;
			}
			else if (unit == "m" || unit == "min" || unit == "mins" || unit == "minute" || unit == "minutes") {
				scale = 60;
			}
			else if (unit == "h" || unit == "hr" || unit == "hrs" || unit == "hour" || unit == "hours") {
				scale = 3600;
			}
			else if (unit == "d" || unit == "day" || unit == "days") {
				scale = 86400;
			}
			else {
				return false;
			}

			total += value * scale;
			if (total > MaxSeconds) {
				return false;
			}
			any = true;
		}

		if (!any) {
			return false;
		}

		out_seconds = static_cast<uint32>(total);
		return true;
	}

	// Joins several arguments ("1h", "30m") so callers can pass split words straight through.
	inline bool Parse(const std::vector<std::string> &parts, uint32 &out_seconds)
	{
		std::string joined;
		for (const auto &p : parts) {
			joined += p;
			joined += ' ';
		}
		return Parse(joined, out_seconds);
	}

	// "1 day, 2 hours, 5 minutes and 3 seconds"
	inline std::string Format(uint32 seconds)
	{
		if (seconds == 0) {
			return "0 seconds";
		}

		const uint32 parts[4] = { seconds / 86400, (seconds % 86400) / 3600, (seconds % 3600) / 60, seconds % 60 };
		const char  *names[4] = { "day", "hour", "minute", "second" };

		std::vector<std::string> out;
		for (int i = 0; i < 4; i++) {
			if (parts[i]) {
				out.push_back(std::to_string(parts[i]) + " " + names[i] + (parts[i] == 1 ? "" : "s"));
			}
		}

		std::string s;
		for (size_t i = 0; i < out.size(); i++) {
			if (i > 0) {
				s += (i + 1 == out.size()) ? " and " : ", ";
			}
			s += out[i];
		}
		return s;
	}

	// Countdown announcements (world shutdown, world restart, rolling zone restart). Above
	// EscalationStart they follow the interval; below it they escalate through FinalMarks.
	constexpr uint32 EscalationStart = 15 * 60;
	constexpr uint32 FinalMarks[]    = { 900, 600, 300, 240, 180, 120, 60, 30, 10 };

	// Largest announcement mark strictly below `remaining`; 0 when there are none left.
	// interval_seconds 0 picks a spacing from how far out the deadline is.
	inline uint32 NextCountdownMark(uint32 remaining, uint32 interval_seconds)
	{
		if (remaining > EscalationStart) {
			uint32 step = interval_seconds;
			if (step == 0) {
				step = remaining > 86400 ? 21600 :
					   remaining > 21600 ? 10800 :
					   remaining > 3600  ? 3600  : 900;
			}

			const uint32 mark = ((remaining - 1) / step) * step;
			return mark > EscalationStart ? mark : EscalationStart;
		}

		for (const auto mark : FinalMarks) {
			if (mark < remaining) {
				return mark;
			}
		}

		return 0;
	}

	// Whole minutes once we're past a minute out; seconds are noise in a MOTD or a broadcast.
	inline std::string FormatCountdown(uint32 seconds)
	{
		return Format(seconds >= 60 ? seconds - (seconds % 60) : seconds);
	}

	// "2026-10-04 21:00 UTC"
	inline std::string FormatUTC(time_t t)
	{
		char buf[32] = { 0 };
		std::tm tm{};
#ifdef _WINDOWS
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tm);
		return buf;
	}
}

#endif
