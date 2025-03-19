#include "../client.h"

void command_leaderboard(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] == 0) {
		c->Message(Chat::White, "Usage: #leaderboard [SFHC|SSFHC|SFHCOnly|HC]");
		return;
	}

	std::string query = "SELECT level, class, name, race, e_solo_only, e_self_found FROM character_data WHERE ";

	if(strncasecmp(sep->arg[1], "SFHCOnly", 8) == 0)
	{
		query += "e_solo_only = 0 AND e_self_found >= 1 AND e_hardcore >= 1";
	}
	else if(strncasecmp(sep->arg[1], "SFHC", 4) == 0)
	{
		query += "(e_self_found >= 1 OR e_solo_only >= 1) AND e_hardcore >= 1";
	}
	else if(strncasecmp(sep->arg[1], "SSFHC", 5) == 0)
	{
		query += "e_solo_only >= 1 AND e_hardcore >= 1";
	}
	else if(strncasecmp(sep->arg[1], "HC", 2) == 0)
	{
		query += "e_hardcore >= 1";
	}
	else
	{
		c->Message(Chat::White, "Usage: #leaderboard [SFHC|SSFHC|SFHCOnly|HC]");
		return;
	}

	query += " ORDER BY level DESC LIMIT 25;";

	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		c->Message(Chat::White, "Error querying database.");
		c->Message(Chat::White, query.c_str());
		return;
	}

	int count = 0;
	const int maxrows = 25;

	c->Message(Chat::White, "---------------------------");

	for (auto row = results.begin(); row != results.end(); ++row) {
		if (++count > maxrows) {
			c->Message(Chat::White, "%i players shown.", maxrows);
			break;
		}

		bool IsSolo = strcmp(row[4], "1") == 0; 
		bool IsSelfFound = strcmp(row[5], "1") == 0; 

		std::string SSFTag = "";
		if(IsSolo)
			SSFTag += "Solo ";
		if(IsSelfFound)
			SSFTag += "Self Found";

		// Convert race and class to strings
		c->Message(Chat::White, "[%s %s] %s (%s) - %s", row[0], GetClassIDName(std::stoi(row[1]), std::stoi(row[0])), row[2], GetRaceIDName(std::stoi(row[3])), SSFTag.c_str());
	}

	if (count <= maxrows)
		c->Message(Chat::White, "Query complete. %i rows shown.", count);
	else if (count == 0)
		c->Message(Chat::White, "No matches found for %s.", sep->arg[1]);
}

