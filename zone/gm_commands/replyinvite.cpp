#include "../client.h"
#include "../command.h"
#include "../../common/seperator.h"

void command_replyinvite(Client *c, const Seperator *sep)
{
	if (!c->HasLastTellFrom())
	{
		c->Message(Chat::Red, "No one has sent you a tell yet in this zone.");
		return;
	}

	std::string last_teller = c->GetLastTellFrom();

	std::string cmd = "#invite " + last_teller;

	Seperator invite_sep(cmd.c_str());
	command_invite(c, &invite_sep);
}
