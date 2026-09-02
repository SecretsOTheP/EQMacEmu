#include "../../client.h"

void ShowHateList(Client* c, const Seperator* sep)
{
	if (!c->GetTarget()) {
		c->Message(Chat::White, "You must target an NPC or player to use this command.");
		return;
	}

	const auto t = c->GetTarget();
	if (t->IsNPC()) {
		t->PrintHateListToClient(c);
		return;
	}

	if (!t->IsClient()) {
		c->Message(Chat::White, "You must target an NPC or player to use this command.");
		return;
	}

	uint32 count = 0;
	for (const auto &[entity_id, npc] : entity_list.GetNPCList()) {
		if (!npc || !npc->IsOnHatelist(t)) {
			continue;
		}

		c->Message(
			Chat::White,
			"%s (Entity ID: %u, NPC Type ID: %u)",
			npc->GetCleanName(),
			entity_id,
			npc->GetNPCTypeID()
		);
		count++;
	}

	if (count == 0) {
		c->Message(Chat::White, "%s is not on any NPC hate lists in this zone.", t->GetCleanName());
	}
	else {
		c->Message(Chat::White, "%u NPC%s currently have %s on their hate list.", count, count == 1 ? "" : "s", t->GetCleanName());
	}
}
