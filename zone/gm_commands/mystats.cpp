#include "../client.h"

void command_mystats(Client *c, const Seperator *sep){
	if (c->Admin() < AccountStatus::Guide)
		c->SendStatsToPlayer(c);
	else if (c->GetTarget() && c->GetPet()) {
		if (c->GetTarget()->IsPet() && c->GetTarget() == c->GetPet())
			c->GetTarget()->ShowStats(c);
		else
			c->ShowStats(c);
	}
	else
		c->ShowStats(c);
}

