#include "../client.h"
#include "../command.h"
#include "../../common/seperator.h"

void command_replyraidtarget(Client *c, const Seperator *sep)
{
	if (!c->HasLastTellFrom())
	{
		c->Message(Chat::Red, "No one has sent you a tell yet in this zone.");
		return;
	}

	std::string last_teller = c->GetLastTellFrom();
	std::string cmd = "#raidtarget " + last_teller;

	Seperator raidtarget_sep(cmd.c_str());
	command_raidtarget(c, &raidtarget_sep);
}
