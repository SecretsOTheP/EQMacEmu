#ifndef EQEMU_SHUTDOWN_DURATION_H
#define EQEMU_SHUTDOWN_DURATION_H

#include "types.h"
#include <cctype>
#include <ctime>
#include <regex>
#include <string>
#include <vector>

// Duration and schedule handling shared by #worldshutdown, #worldrestart, #rollingrestart, the world
// console and the world CLI.
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

	// Countdown announcements (world shutdown, world restart, rolling zone restart). Nothing is
	// broadcast until AnnounceStart; until then the MOTD carries the notice. From there, every 5
	// minutes, then every minute once MinuteWarnings is reached.
	constexpr uint32 AnnounceStart  = 60 * 60;
	constexpr uint32 MinuteWarnings = 15 * 60;

	// Largest announcement mark strictly below `remaining`; 0 when there are none left.
	inline uint32 NextCountdownMark(uint32 remaining)
	{
		if (remaining > AnnounceStart) {
			return AnnounceStart;
		}

		const uint32 step = remaining > MinuteWarnings ? 300 : 60;
		return remaining ? ((remaining - 1) / step) * step : 0;
	}

	// Whole minutes once we're past a minute out; seconds are noise in a broadcast.
	inline std::string FormatCountdown(uint32 seconds)
	{
		return Format(seconds >= 60 ? seconds - (seconds % 60) : seconds);
	}

	// US Eastern time without a timezone database: EST is UTC-5, EDT is UTC-4, and daylight time
	// runs from 2:00 AM on the second Sunday of March to 2:00 AM on the first Sunday of November.
	namespace Eastern {
		// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
		inline int64 DaysFromCivil(int64 y, uint32 m, uint32 d)
		{
			y -= m <= 2;
			const int64  era = (y >= 0 ? y : y - 399) / 400;
			const uint32 yoe = static_cast<uint32>(y - era * 400);
			const uint32 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
			const uint32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
			return era * 146097 + static_cast<int64>(doe) - 719468;
		}

		inline void CivilFromDays(int64 z, int64 &y, uint32 &m, uint32 &d)
		{
			z += 719468;
			const int64  era = (z >= 0 ? z : z - 146096) / 146097;
			const uint32 doe = static_cast<uint32>(z - era * 146097);
			const uint32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
			const uint32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
			const uint32 mp  = (5 * doy + 2) / 153;
			d = doy - (153 * mp + 2) / 5 + 1;
			m = mp < 10 ? mp + 3 : mp - 9;
			y = static_cast<int64>(yoe) + era * 400 + (m <= 2);
		}

		inline int64 FloorDays(int64 seconds)
		{
			return seconds >= 0 ? seconds / 86400 : (seconds - 86399) / 86400;
		}

		// 0 = Sunday.
		inline uint32 Weekday(int64 days)
		{
			return static_cast<uint32>(((days % 7) + 7 + 4) % 7);
		}

		inline uint32 DaysInMonth(int64 y, uint32 m)
		{
			static const uint32 days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
			return m == 2 && leap ? 29 : days[m - 1];
		}

		// Day number of the nth Sunday of a month.
		inline int64 NthSunday(int64 y, uint32 m, uint32 n)
		{
			const int64 first = DaysFromCivil(y, m, 1);
			return first + (7 - Weekday(first)) % 7 + 7 * (n - 1);
		}

		// Daylight time for an Eastern wall-clock time given as seconds since the epoch as if it were
		// UTC. The repeated hour in November counts as daylight time.
		inline bool IsDSTLocal(int64 local, int64 year)
		{
			return local >= NthSunday(year, 3, 2) * 86400 + 2 * 3600 && local < NthSunday(year, 11, 1) * 86400 + 2 * 3600;
		}

		inline bool IsDST(time_t utc)
		{
			int64  y;
			uint32 m, d;
			CivilFromDays(FloorDays(static_cast<int64>(utc) - 5 * 3600), y, m, d);
			return utc >= NthSunday(y, 3, 2) * 86400 + 7 * 3600 && utc < NthSunday(y, 11, 1) * 86400 + 6 * 3600;
		}

		inline int64 ToLocal(time_t utc)
		{
			return static_cast<int64>(utc) - (IsDST(utc) ? 4 : 5) * 3600;
		}

		inline time_t ToUTC(int64 y, uint32 m, uint32 d, uint32 hour, uint32 minute)
		{
			const int64 local = DaysFromCivil(y, m, d) * 86400 + hour * 3600 + minute * 60;
			return static_cast<time_t>(local + (IsDSTLocal(local, y) ? 4 : 5) * 3600);
		}

		// "Mon, Oct 5 2026 7:00 AM EDT"
		inline std::string Format(time_t t)
		{
			static const char *weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
			static const char *months[]   = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

			const int64 local = ToLocal(t);
			const int64 days  = FloorDays(local);
			const int64 secs  = local - days * 86400;

			int64  y;
			uint32 m, d;
			CivilFromDays(days, y, m, d);

			const uint32 hour   = static_cast<uint32>(secs / 3600);
			const uint32 minute = static_cast<uint32>((secs % 3600) / 60);
			const uint32 hour12 = hour % 12 == 0 ? 12 : hour % 12;

			return std::string(weekdays[Weekday(days)]) + ", " + months[m - 1] + " " + std::to_string(d) + " " +
				std::to_string(y) + " " + std::to_string(hour12) + ":" + (minute < 10 ? "0" : "") +
				std::to_string(minute) + (hour < 12 ? " AM " : " PM ") + (IsDST(t) ? "EDT" : "EST");
		}
	}

	namespace detail {
		inline bool MonthFromName(const std::string &name, uint32 &out)
		{
			static const char *months[] = {
				"january", "february", "march", "april", "may", "june",
				"july", "august", "september", "october", "november", "december"
			};
			if (name.size() < 3) {
				return false;
			}
			for (uint32 i = 0; i < 12; i++) {
				const std::string full = months[i];
				// "oct", "octo", "october"; also "sept".
				if (full.compare(0, name.size(), name) == 0 || (i == 8 && name == "sept")) {
					out = i + 1;
					return true;
				}
			}
			return false;
		}

		// "7am", "7 am", "7:30pm", "07:00", "19:00", "noon", "midnight". A bare hour needs am/pm.
		inline bool ParseClock(const std::string &in, uint32 &hour, uint32 &minute)
		{
			if (in == "noon") {
				hour   = 12;
				minute = 0;
				return true;
			}
			if (in == "midnight") {
				hour   = 0;
				minute = 0;
				return true;
			}

			static const std::regex re(R"(^(\d{1,2})(?::(\d{2}))?\s*(am|pm|a\.m\.|p\.m\.)?$)");
			std::smatch m;
			if (!std::regex_match(in, m, re) || (!m[2].matched && !m[3].matched)) {
				return false;
			}

			hour   = static_cast<uint32>(std::stoul(m[1].str()));
			minute = m[2].matched ? static_cast<uint32>(std::stoul(m[2].str())) : 0;
			if (minute > 59) {
				return false;
			}

			if (m[3].matched) {
				if (hour < 1 || hour > 12) {
					return false;
				}
				hour = (hour % 12) + (m[3].str()[0] == 'p' ? 12 : 0);
			}
			return hour <= 23;
		}

		// "2026-10-05", "10/5/2026", "10/5/26", "10/5", "oct 5 2026", "october 5th", "5 oct 2026".
		// year is -1 when it was left out.
		inline bool ParseDate(const std::string &in, int64 &year, uint32 &month, uint32 &day)
		{
			static const std::regex iso(R"(^(\d{4})-(\d{1,2})-(\d{1,2})$)");
			static const std::regex us(R"(^(\d{1,2})/(\d{1,2})(?:/(\d{4}|\d{2}))?$)");
			static const std::regex named(R"(^([a-z]+)\.?\s+(\d{1,2})(?:st|nd|rd|th)?(?:\s+(\d{4}))?$)");
			static const std::regex day_first(R"(^(\d{1,2})(?:st|nd|rd|th)?\s+([a-z]+)\.?(?:\s+(\d{4}))?$)");

			std::smatch m;
			year = -1;
			if (std::regex_match(in, m, iso)) {
				year  = std::stoll(m[1].str());
				month = static_cast<uint32>(std::stoul(m[2].str()));
				day   = static_cast<uint32>(std::stoul(m[3].str()));
			}
			else if (std::regex_match(in, m, us)) {
				month = static_cast<uint32>(std::stoul(m[1].str()));
				day   = static_cast<uint32>(std::stoul(m[2].str()));
				if (m[3].matched) {
					year = std::stoll(m[3].str());
					if (year < 100) {
						year += 2000;
					}
				}
			}
			else if (std::regex_match(in, m, named)) {
				if (!MonthFromName(m[1].str(), month)) {
					return false;
				}
				day = static_cast<uint32>(std::stoul(m[2].str()));
				if (m[3].matched) {
					year = std::stoll(m[3].str());
				}
			}
			else if (std::regex_match(in, m, day_first)) {
				if (!MonthFromName(m[2].str(), month)) {
					return false;
				}
				day = static_cast<uint32>(std::stoul(m[1].str()));
				if (m[3].matched) {
					year = std::stoll(m[3].str());
				}
			}
			else {
				return false;
			}

			// With no year, Feb 29 is checked against a leap year here and rejected later if needed.
			return month >= 1 && month <= 12 && day >= 1 && day <= Eastern::DaysInMonth(year < 0 ? 2000 : year, month);
		}
	}

	// Turns a schedule into seconds from `now`. Accepts:
	//   a delay:           90, 90s, 15m, 1h30m, "2 hours 30 minutes" (a bare number is seconds)
	//   an Eastern time:   7am, 7:30pm, 19:00, noon (the next time it comes around)
	//   a date and time:   10/5/2026 7am, 2026-10-05 07:00, "Oct 5 2026 at 7:00 AM", "7am October 5th ET"
	//                      (Eastern unless it ends in UTC; with no year, the next time that date comes around)
	//   a Unix timestamp:  1759662000
	// On failure, error says why.
	inline bool ParseWhen(const std::string &in, time_t now, uint32 &out_seconds, std::string &error)
	{
		uint32 delay = 0;
		if (Parse(in, delay)) {
			if (delay == 0) {
				error = "The delay must be more than 0 seconds.";
				return false;
			}
			out_seconds = delay;
			return true;
		}

		// Lower case, commas and runs of whitespace to single spaces.
		std::string s;
		for (char c : in) {
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
			if (c == ',' || isspace(static_cast<unsigned char>(c))) {
				c = ' ';
			}
			if (c == ' ' && (s.empty() || s.back() == ' ')) {
				continue;
			}
			s += c;
		}
		while (!s.empty() && s.back() == ' ') {
			s.pop_back();
		}

		static const std::regex iso_t(R"(^(\d{4}-\d{1,2}-\d{1,2})t)");
		s = std::regex_replace(s, iso_t, "$1 ");

		time_t      deadline = 0;
		std::smatch m;

		static const std::regex epoch(R"(^@?(\d{9,11})$)");
		if (std::regex_match(s, m, epoch)) {
			deadline = static_cast<time_t>(std::stoll(m[1].str()));
		}
		else {
			// Everything is Eastern unless it ends in UTC.
			bool utc = false;
			static const std::regex zone(R"(^(.*?)\s*\b(et|est|edt|eastern(?: time)?|utc|gmt)$)");
			if (std::regex_match(s, m, zone)) {
				utc = m[2].str() == "utc" || m[2].str() == "gmt";
				s   = m[1].str();
			}

			static const std::string clock = R"((\d{1,2}(?::\d{2})?\s*(?:am|pm|a\.m\.|p\.m\.)?|noon|midnight))";
			static const std::regex  date_then_time("^(?:on\\s+)?(.+?)\\s+(?:at\\s+)?" + clock + "$");
			static const std::regex  time_then_date("^(?:at\\s+)?" + clock + "\\s+(?:on\\s+)?(.+)$");
			static const std::regex  time_only("^(?:at\\s+)?" + clock + "$");

			uint32 hour = 0, minute = 0, month = 0, day = 0;
			int64  year      = -1;
			bool   have_date = false;

			if (std::regex_match(s, m, date_then_time) && detail::ParseClock(m[2].str(), hour, minute) &&
				detail::ParseDate(m[1].str(), year, month, day)) {
				have_date = true;
			}
			else if (std::regex_match(s, m, time_then_date) && detail::ParseClock(m[1].str(), hour, minute) &&
					 detail::ParseDate(m[2].str(), year, month, day)) {
				have_date = true;
			}
			else if (!(std::regex_match(s, m, time_only) && detail::ParseClock(m[1].str(), hour, minute))) {
				error = "Couldn't understand '" + in + "'. Use a delay (90m, 1h30m), an Eastern time "
					"(7am, 10/5/2026 7am, Oct 5 2026 7:00am, 2026-10-05 07:00) or a Unix timestamp.";
				return false;
			}

			auto to_utc = [&](int64 y, uint32 mo, uint32 d) {
				if (utc) {
					return static_cast<time_t>(Eastern::DaysFromCivil(y, mo, d) * 86400 + hour * 3600 + minute * 60);
				}
				return Eastern::ToUTC(y, mo, d, hour, minute);
			};

			// Today in the zone the time was given in.
			int64  ty;
			uint32 tm, td;
			Eastern::CivilFromDays(Eastern::FloorDays(utc ? static_cast<int64>(now) : Eastern::ToLocal(now)), ty, tm, td);

			if (!have_date) {
				// The next time this clock time comes around.
				deadline = to_utc(ty, tm, td);
				if (deadline <= now) {
					int64  y;
					uint32 mo, d;
					Eastern::CivilFromDays(Eastern::DaysFromCivil(ty, tm, td) + 1, y, mo, d);
					deadline = to_utc(y, mo, d);
				}
			}
			else if (year < 0) {
				// The next time this date comes around.
				if (month == 2 && day == 29) {
					error = "Give a year for February 29.";
					return false;
				}
				deadline = to_utc(ty, month, day);
				if (deadline <= now) {
					deadline = to_utc(ty + 1, month, day);
				}
			}
			else {
				deadline = to_utc(year, month, day);
			}
		}

		if (deadline <= now) {
			error = "That time has already passed (" + Eastern::Format(deadline) + ").";
			return false;
		}
		if (static_cast<int64>(deadline - now) > MaxSeconds) {
			error = "That's more than 30 days away (" + Eastern::Format(deadline) + ").";
			return false;
		}

		out_seconds = static_cast<uint32>(deadline - now);
		return true;
	}
}

#endif
