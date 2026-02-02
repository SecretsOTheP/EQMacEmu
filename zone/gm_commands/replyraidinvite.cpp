#include "../client.h"
#include "../command.h"
#include "../../common/seperator.h"

void command_replyraidinvite(Client *c, const Seperator *sep)
{
	if (!c->HasLastTellFrom())
	{
		c->Message(Chat::Red, "No one has sent you a tell yet in this zone.");
		return;
	}

	std::string last_teller = c->GetLastTellFrom();

	std::string cmd = "#raidinvite " + last_teller;

	// optional group number argument
	if (sep->arg[1][0]) {
		cmd += " ";
		cmd += sep->arg[1];
	}

	Seperator raidinvite_sep(cmd.c_str());
	command_raidinvite(c, &raidinvite_sep);
}
