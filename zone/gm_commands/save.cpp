#include "../client.h"
#include "../corpse.h"

void command_save(Client *c, const Seperator *sep)
{
	if (
		!c->GetTarget() ||
		(
			c->GetTarget() &&
			!c->GetTarget()->IsClient() &&
			!c->GetTarget()->IsPlayerCorpse()
			)
		) {
		c->Message(Chat::White, "You must target a player or player corpse to use this command.");
		return;
	}

	auto target = c->GetTarget();

	if (target->IsClient()) 
	{
		if (target->CastToClient()->Save(2))
		{
			c->Message_StringID(Chat::White, StringID::PC_SAVED, target->GetName());
		}
		else
			c->Message(Chat::White, "Manual save for %s failed.", target->GetName());
	}
	else if (target->IsPlayerCorpse())
	{
		if (target->CastToMob()->Save())
			c->Message(Chat::White, "%s saved. (dbid=%u)", target->GetName(), target->CastToCorpse()->GetCorpseDBID());
		else
			c->Message(Chat::White, "Manual save for %s failed.", target->GetName());
	}
	else
		c->Message(Chat::White, "Error: target not a Client/PlayerCorpse");
}

