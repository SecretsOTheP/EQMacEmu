#include "../client.h"
#include "../zone.h"

namespace {
const char *RallosianGloryTitle(uint8 rank)
{
	static const char *titles[] = {
		"Unproven",
		"Blooded",
		"Blood Seeker",
		"Blooded Champion",
		"Warbringer",
		"Conqueror",
		"Rallosian Ravager",
		"The Warlord's Chosen",
		"Herald of Slaughter",
		"Scourge of Norrath",
		"Chosen of the Warlord"
	};
	static_assert(Client::RallosianGloryMaxRank == 10, "Rallosian Glory titles must match the rank cap");
	return titles[std::min<uint8>(rank, Client::RallosianGloryMaxRank)];
}
}

void command_glory(Client *c, const Seperator *sep)
{
	if (!c)
		return;

	if (!zone || zone->GetGuildID() != 1) {
		c->Message(Chat::Yellow, "Rallosian Glory may only be claimed upon the battlefields of Guild 1.");
		return;
	}

	const uint8 rank = c->GetRallosianGlory();
	c->Message(Chat::Lime, "=== Rallosian Glory ===");
	c->Message(
		Chat::White, "Rank: %u of %u - %s",
		static_cast<unsigned>(rank), static_cast<unsigned>(Client::RallosianGloryMaxRank), RallosianGloryTitle(rank));
	c->Message(
		Chat::White, "Experience Bonus: +%u%% from Glory",
		static_cast<unsigned>(rank * Client::RallosianGloryRankXPBonus));
	c->Message(
		Chat::White, "Current Battlefield Bonus: +%u%%",
		static_cast<unsigned>(Client::RallosianGloryZoneXPBonus));
	c->Message(
		Chat::White, "Total Battlefield Bonus: +%u%%",
		static_cast<unsigned>(Client::RallosianGloryZoneXPBonus + rank * Client::RallosianGloryRankXPBonus));

	if (rank == 0) {
		c->Message(Chat::Yellow, "You bear no Rallosian Glory. Prove your strength against a worthy opponent, and the Warlord may turn his gaze upon you.");
	} else if (rank == Client::RallosianGloryMaxRank) {
		c->Message(Chat::Yellow, "Rallos Zek's laughter follows in your wake. Your name has been written in blood across the battlefield.");
	} else {
		c->Message(Chat::Yellow, "Rallos Zek watches your victories with growing approval. Spill worthy blood to rise further - but flee, and your Glory will be forfeit.");
	}
}
