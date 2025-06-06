/**
 * EQEmulator: Everquest Server Emulator
 * Copyright (C) 2001-2019 EQEmulator Development Team (https://github.com/EQEmu/Server)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY except by those people which sell it, which
 * are required to give you total support for your newly bought product;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef EQEMU_WORLD_CONTENT_SERVICE_H
#define EQEMU_WORLD_CONTENT_SERVICE_H

#include <string>
#include <vector>
#include "../loottable.h"
#include "../repositories/content_flags_repository.h"

class Database;

struct ContentFlags {
	float       min_expansion;
	float       max_expansion;
	std::string content_flags;
	std::string content_flags_disabled;
};

namespace Expansion {
	static const int EXPANSION_ALL = -1.0f;
	static const int EXPANSION_FILTER_MAX = 99;

	enum ExpansionNumber {
		Classic = 0,
		TheRuinsOfKunark,
		TheScarsOfVelious,
		TheShadowsOfLuclin,
		ThePlanesOfPower,
		MaxId
	};

	/**
	 * If you add to this, make sure you update LogCategory
	 */
	static const char *ExpansionName[ExpansionNumber::MaxId+1] = {
		"Classic",
		"The Ruins of Kunark",
		"The Scars of Velious",
		"The Shadows of Luclin",
		"The Planes of Power",
		"Post Planes of Power",
	};
}

class WorldContentService {
public:

	WorldContentService();

	std::string GetCurrentExpansionName();

	float GetCurrentExpansion() const;
	void SetCurrentExpansion(float current_expansion);

	bool IsClassicEnabled() { return (int)floor(GetCurrentExpansion()) >= (int)Expansion::ExpansionNumber::Classic || (int)floor(GetCurrentExpansion()) == (int)Expansion::EXPANSION_ALL; }
	bool IsTheRuinsOfKunarkEnabled() { return (int)floor(GetCurrentExpansion()) >= (int)Expansion::ExpansionNumber::TheRuinsOfKunark || (int)floor(GetCurrentExpansion()) == (int)Expansion::EXPANSION_ALL; }
	bool IsTheScarsOfVeliousEnabled() { return (int)floor(GetCurrentExpansion()) >= (int)Expansion::ExpansionNumber::TheScarsOfVelious || (int)floor(GetCurrentExpansion()) == (int)Expansion::EXPANSION_ALL; }
	bool IsTheShadowsOfLuclinEnabled() { return (int)floor(GetCurrentExpansion()) >= (int)Expansion::ExpansionNumber::TheShadowsOfLuclin || (int)floor(GetCurrentExpansion()) == (int)Expansion::EXPANSION_ALL; }
	bool IsThePlanesOfPowerEnabled() { return (int)floor(GetCurrentExpansion()) >= (int)Expansion::ExpansionNumber::ThePlanesOfPower || (int)floor(GetCurrentExpansion()) == (int)Expansion::EXPANSION_ALL; }

	bool IsCurrentExpansionClassic() { return (int)floor(current_expansion) == (int)Expansion::ExpansionNumber::Classic; }
	bool IsCurrentExpansionTheRuinsOfKunark() { return (int)floor(current_expansion) == (int)Expansion::ExpansionNumber::TheRuinsOfKunark; }
	bool IsCurrentExpansionTheScarsOfVelious() { return (int)floor(current_expansion) == (int)Expansion::ExpansionNumber::TheScarsOfVelious; }
	bool IsCurrentExpansionTheShadowsOfLuclin() { return (int)floor(current_expansion) == (int)Expansion::ExpansionNumber::TheShadowsOfLuclin; }
	bool IsCurrentExpansionThePlanesOfPower() { return (int)floor(current_expansion) == (int)Expansion::ExpansionNumber::ThePlanesOfPower; }

	const std::vector<ContentFlagsRepository::ContentFlags> &GetContentFlags() const;
	std::vector<std::string> GetContentFlagsEnabled();
	std::vector<std::string> GetContentFlagsDisabled();
	bool IsContentFlagEnabled(const std::string &content_flag);
	bool IsContentFlagDisabled(const std::string &content_flag);
	void SetContentFlags(const std::vector<ContentFlagsRepository::ContentFlags>& content_flags);
	void ReloadContentFlags();
	WorldContentService *SetExpansionContext();

	bool DoesPassContentFiltering(const ContentFlags &f);

	WorldContentService *SetDatabase(Database *database);
	Database *GetDatabase() const;

	void SetContentFlag(const std::string &content_flag_name, bool enabled);

private:
	float current_expansion{};
	std::vector<ContentFlagsRepository::ContentFlags> content_flags;

	// reference to database
	Database *m_database;
};

extern WorldContentService content_service;

#endif //EQEMU_WORLD_CONTENT_SERVICE_H