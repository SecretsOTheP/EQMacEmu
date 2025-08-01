// for folly stuff
/*
 * Copyright 2013 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _STRINGS_H_
#define _STRINGS_H_

#include <sstream>
#include <string.h>
#include <vector>
#include <cstdarg>
#include <tuple>
#include <type_traits>

#include <fmt/format.h>

#ifndef _WIN32
 // this doesn't appear to affect linux-based systems..need feedback for _WIN64
#endif

#ifdef _WINDOWS
#include <ctype.h>
#include <functional>
#include <algorithm>
#endif

#include "types.h"

class Strings {
public:
	static bool Contains(std::vector<std::string> container, std::string element);
	static bool Contains(const std::string &subject, const std::string &search);
	static int ToInt(const std::string &s, int fallback = 0);
	static int64 ToBigInt(const std::string &s, int64 fallback = 0);
	static uint32 ToUnsignedInt(const std::string &s, uint32 fallback = 0);
	static uint64 ToUnsignedBigInt(const std::string &s, uint64 fallback = 0);
	static float ToFloat(const std::string &s, float fallback = 0.0f);
	static bool IsNumber(const std::string &s);
	static std::string RemoveNumbers(std::string s);
	static bool IsFloat(const std::string &s);
	static const std::string ToLower(std::string s);
	static const std::string ToUpper(std::string s);
	static const std::string UcFirst(std::string s);
	static std::string &LTrim(std::string &str, const std::string &chars = "\t\n\v\f\r ");
	static std::string &RTrim(std::string &str, const std::string &chars = "\t\n\v\f\r ");
	static std::string &Trim(std::string &str, const std::string &chars = "\t\n\v\f\r ");
	static std::string Commify(const std::string &number);
	static std::string Commify(uint16 number) { return Strings::Commify(std::to_string(number)); };
	static std::string Commify(uint32 number) { return Strings::Commify(std::to_string(number)); };
	static std::string Commify(uint64 number) { return Strings::Commify(std::to_string(number)); };
	static std::string Commify(int16 number) { return Strings::Commify(std::to_string(number)); };
	static std::string Commify(int32 number) { return Strings::Commify(std::to_string(number)); };
	static std::string Commify(int64 number) { return Strings::Commify(std::to_string(number)); };
	static std::string ConvertToDigit(int n, std::string suffix);
	static std::string Escape(const std::string &s);
	static std::string EscapePair(const char *src, size_t sz);
	static std::string Implode(std::string glue, std::vector<std::string> src);
	static std::string Join(const std::vector<std::string> &ar, const std::string &delim);
	static std::string Join(const std::vector<uint32_t> &ar, const std::string &delim);
	static std::string MillisecondsToTime(int duration);
	static std::string Money(uint32 platinum, uint32 gold = 0, uint32 silver = 0, uint32 copper = 0);
	static std::string NumberToWords(unsigned long long int n);
	static std::string Repeat(std::string s, int n);
	static std::string Replace(std::string subject, const std::string &search, const std::string &replace);
	static std::string SecondsToTime(int duration, bool is_milliseconds = false);
	static std::string::size_type SearchDelim(const std::string &haystack, const std::string &needle, const char deliminator = ',');
	static std::vector<std::string> Split(const std::string &s, const char delim = ',');
	static std::vector<std::string> Split(std::string s, std::string delimiter);
	static std::vector<std::string> Wrap(std::vector<std::string> &src, std::string character);
	static void FindReplace(std::string &string_subject, const std::string &search_string, const std::string &replace_string);
	static uint32 TimeToSeconds(std::string time_string);
	static bool ToBool(std::string bool_string);
	static inline bool EqualFold(const std::string &string_one, const std::string &string_two) { return strcasecmp(string_one.c_str(), string_two.c_str()) == 0; }
	static std::string Random(size_t length);
	
	static bool BeginsWith(const std::string& subject, const std::string& search);
	static bool EndsWith(const std::string& subject, const std::string& search);
	static std::string ZoneTime(const uint8 hours, const uint8 minutes);

	static std::string IncrementRoman(const std::string& roman);
	static std::string IntToRoman(int num);
	static int RomanToInt(const std::string& roman);
	
	static bool SanitizeChatString(std::string &in_string);

	template <typename T>
	static std::string ImplodePair(const std::string &glue, const std::pair<char, char> &encapsulation, const std::vector<T> &src)
	{
		if (src.empty()) {
			return {};
		}

		std::ostringstream oss;

		for (const T& src_iter : src) {
			oss << encapsulation.first << src_iter << encapsulation.second << glue;
		}

		std::string output(oss.str());
		output.resize(output.size() - glue.size());

		return output;
	}
};

const std::string StringFormat(const char *format, ...);
const std::string vStringFormat(const char *format, va_list args);


// For converstion of numerics into English
// Used for grid nodes, as NPC names remove numerals.
// But general purpose

const std::string NUM_TO_ENGLISH_X[] = { "", "One ", "Two ", "Three ", "Four ",
				"Five ", "Six ", "Seven ", "Eight ", "Nine ", "Ten ", "Eleven ",
				"Twelve ", "Thirteen ", "Fourteen ", "Fifteen ",
				"Sixteen ", "Seventeen ", "Eighteen ", "Nineteen " };

const std::string NUM_TO_ENGLISH_Y[] = { "", "", "Twenty ", "Thirty ", "Forty ",
				"Fifty ", "Sixty ", "Seventy ", "Eighty ", "Ninety " };


// _WIN32 builds require that #include<fmt/format.h> be included in whatever code file the invocation is made from (no header files)
template <typename T1, typename T2>
std::vector<std::string> join_pair(const std::string &glue, const std::pair<char, char> &encapsulation, const std::vector<std::pair<T1, T2>> &src)
{
	if (src.empty()) {
		return {};
	}

	std::vector<std::string> output;

	for (const std::pair<T1, T2> &src_iter : src) {
		output.emplace_back(

			fmt::format(
				"{}{}{}{}{}{}{}",
				encapsulation.first,
				src_iter.first,
				encapsulation.second,
				glue,
				encapsulation.first,
				src_iter.second,
				encapsulation.second
			)
		);
	}

	return output;
}

// _WIN32 builds require that #include<fmt/format.h> be included in whatever code file the invocation is made from (no header files)
template <typename T1, typename T2, typename T3, typename T4>
std::vector<std::string> join_tuple(const std::string &glue, const std::pair<char, char> &encapsulation, const std::vector<std::tuple<T1, T2, T3, T4>> &src)
{
	if (src.empty()) {
		return {};
	}

	std::vector<std::string> output;

	for (const std::tuple<T1, T2, T3, T4> &src_iter : src) {

		output.emplace_back(

			fmt::format(
				"{}{}{}{}{}{}{}{}{}{}{}{}{}{}{}",
				encapsulation.first,
				std::get<0>(src_iter),
				encapsulation.second,
				glue,
				encapsulation.first,
				std::get<1>(src_iter),
				encapsulation.second,
				glue,
				encapsulation.first,
				std::get<2>(src_iter),
				encapsulation.second,
				glue,
				encapsulation.first,
				std::get<3>(src_iter),
				encapsulation.second
			)
		);
	}

	return output;
}

void ParseAccountString(const std::string &s, std::string &account, std::string &loginserver);

//const char based

bool atobool(const char *iBool);
bool isAlphaNumeric(const char *text);
bool strn0cpyt(char* dest, const char *source, uint32 size);
char* CleanMobName(const char* in, char *out);
char* CleanMobNameWithSpaces(const char *in, char *out);
char* RemoveApostrophes(const char *s);
char* strn0cpy(char* dest, const char *source, uint32 size);
const char* ConvertArray(int input, char *returnchar);
const char* ConvertArrayF(float input, char *returnchar);
const char* MakeLowerString(const char *source);
uint32 hextoi(const char *num);
uint64 hextoi64(const char *num);
void MakeLowerString(const char *source, char *target);
void RemoveApostrophes(std::string &s);
std::string FormatName(const std::string &char_name);

#endif
