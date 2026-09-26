#include "../client.h"

// Shared by #blockbuff, #blockbuffif and #allowbuff. if_spell_id 0 = always blocked.
static void SetBuffBlock(Client *c, const Seperator *sep, bool conditional, bool block)
{
	uint16 spell_id = Strings::ToUnsignedInt(sep->arg[1]);
	uint16 if_spell_id = conditional ? Strings::ToUnsignedInt(sep->arg[2]) : 0;
	if (!IsValidSpell(spell_id) || (conditional && !IsValidSpell(if_spell_id))) {
		c->Message(Chat::White, "Usage: #%s", block ? (conditional ? "blockbuffif [spell_id] [active_spell_id]" : "blockbuff [spell_id]") : "allowbuff [spell_id] [active_spell_id]");
		return;
	}

	auto range = c->blocked_buffs.equal_range(spell_id);
	auto it = std::find_if(range.first, range.second, [&](const auto &e) { return e.second == if_spell_id; });
	std::string cond = if_spell_id ? fmt::format(" while {} ({}) is on you", spells[if_spell_id].name, if_spell_id) : "";

	if (block) {
		if (it == range.second) {
			c->blocked_buffs.emplace(spell_id, if_spell_id);
			database.QueryDatabase(StringFormat("REPLACE INTO `character_blocked_buffs` (character_id, spell_id, if_spell_id) VALUES (%u, %u, %u)", c->CharacterID(), spell_id, if_spell_id));
		}
		c->Message(Chat::Yellow, "Blocking %s (%u) from other players%s.", spells[spell_id].name, spell_id, cond.c_str());
	}
	else if (it != range.second) {
		c->blocked_buffs.erase(it);
		database.QueryDatabase(StringFormat("DELETE FROM `character_blocked_buffs` WHERE character_id = %u AND spell_id = %u AND if_spell_id = %u", c->CharacterID(), spell_id, if_spell_id));
		c->Message(Chat::Yellow, "No longer blocking %s (%u)%s.", spells[spell_id].name, spell_id, cond.c_str());
	}
	else {
		c->Message(Chat::White, "You are not blocking %s (%u)%s.", spells[spell_id].name, spell_id, cond.c_str());
	}
}

void command_blockbuff(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] == 0) {
		if (c->blocked_buffs.empty()) {
			c->Message(Chat::White, "You are not blocking any buffs.");
		}
		for (const auto &e : c->blocked_buffs) {
			c->Message(Chat::White, "%s (%u)%s", spells[e.first].name, e.first,
				e.second ? fmt::format(" while {} ({}) is on you", spells[e.second].name, e.second).c_str() : "");
		}
		return;
	}
	SetBuffBlock(c, sep, false, true);
}

void command_blockbuffif(Client *c, const Seperator *sep)
{
	SetBuffBlock(c, sep, true, true);
}

void command_allowbuff(Client *c, const Seperator *sep)
{
	SetBuffBlock(c, sep, sep->arg[2][0] != 0, false);
}
