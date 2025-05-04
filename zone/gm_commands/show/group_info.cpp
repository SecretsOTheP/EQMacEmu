#include "../../client.h"
#include "../../groups.h"
#include "../../raids.h"

void ShowGroupInfo(Client* c, const Seperator* sep)
{
	auto t = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		t = c->GetTarget()->CastToClient();
	}

	auto g = t->GetGroup();
	if (!g) {
		auto r = t->GetRaid();
		if (r) {
			uint32 t_group_id = r->GetGroup(t);
			c->Message(Chat::White, 
				fmt::format(
					"Client {} is in a raid.  Raid ID: {};  Raid leader: {};  Raid count: {};  Raid group ID: {}",
					t->GetName(), 
					r->GetID(), 
					r->leadername, 
					r->RaidCount(), 
					t_group_id
				) .c_str()
			);

			if (t->IsRaidGrouped()) {
				for (int i = 0; i < MAX_RAID_MEMBERS; i++)
				{
					Client* rmember = r->members[i].GetMember();
					if (r->members[i].GroupNumber == t_group_id) {
						if (rmember) {
							c->Message(Chat::White,
								fmt::format(
								"- name: {} ({}) {} {} HP: {} / {} MP: {} / {}", 
									rmember->GetName(), 
									GetClassIDName(rmember->GetClass()),
									r->members[i].IsGroupLeader ? " (group leader)" : "", 
									r->members[i].IsLooter ? " (looter)" : "",
									rmember->GetHP(), 
									rmember->GetMaxHP(), 
									rmember->GetMana(), 
									rmember->GetMaxMana()
								).c_str()
							);
						}
						else if (r->members[i].membername[0] != '\0')
						{
							c->Message(Chat::White, 
								fmt::format(
									"- name: {} ({}) {} {} (not in zone)", 
									r->members[i].membername, 
									GetClassIDName(r->members[i]._class),
									r->members[i].IsGroupLeader ? " (group leader)" : "", 
									r->members[i].IsLooter ? " (looter)" : ""
								).c_str()
							);
						}
					}
				}
			}
			else
				c->Message(Chat::White, "Client is not in a raid group");
		}
		else
			c->Message(Chat::White, fmt::format("Client {} is not in a group or raid", t->GetName()).c_str());

		return;
	}

	c->Message(Chat::White, 
		fmt::format(
			"Player {} is in group ID {} with {} members.  Group leader is: {}", 
			t->GetName(), 
			g->GetID(), 
			g->GroupCount(), 
			g->GetLeaderName()
		).c_str()
	);

	uint32 r;
	for (r = 0; r < MAX_GROUP_MEMBERS; r++) {
		if (g->members[r] == nullptr) {
			if (g->membername[r][0] == '\0')
				continue;
			c->Message(Chat::White, fmt::format("- Zoned Member: {}", g->membername[r]).c_str());
		}
		else {
			c->Message(Chat::White, 
				fmt::format(
					"- In-Zone Member: {} ({}) level: {} HP: {} / {} MP: {} / {}", 
					g->membername[r], 
					GetClassIDName(g->members[r]->GetClass()), 
					g->members[r]->GetLevel(), 
					g->members[r]->GetHP(), 
					g->members[r]->GetMaxHP(), 
					g->members[r]->GetMana(), 
					g->members[r]->GetMaxMana()
				).c_str()
			);
		}
	}
}

