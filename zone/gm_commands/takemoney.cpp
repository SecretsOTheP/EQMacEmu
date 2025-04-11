#include "../client.h"

void command_takemoney(Client *c, const Seperator *sep){
	if (!sep->IsNumber(1)) {	//as long as the first one is a number, we'll just let atoi convert the rest to 0 or a number
		c->Message(Chat::Red, "Usage: #Usage: #takemoney [pp] [gp] [sp] [cp] [reason] - Reason is required");
	}
	else if (sep->IsNumber(1) && sep->arg[2][0] == 0) {
		c->Message(Chat::Red, "Reason is required.");
	}
	else if (sep->IsNumber(2) && sep->arg[3][0] == 0) {
		c->Message(Chat::Red, "Reason is required.");
	}
	else if (sep->IsNumber(3) && sep->arg[4][0] == 0) {
		c->Message(Chat::Red, "Reason is required.");
	}
	else if (sep->IsNumber(4) && sep->arg[5][0] == 0) {
		c->Message(Chat::Red, "Reason is required.");
	}
	else if (sep->IsNumber(5)) {
		c->Message(Chat::Red, "Reason is required.");
	}
	else if (c->GetTarget() == nullptr) {
		c->Message(Chat::Red, "You must target a player to take money from.");
	}
	else if (!c->GetTarget()->IsClient()) {
		c->Message(Chat::Red, "You can only take money from players with this command.");
	}
	else {
		//TODO: update this to the client, otherwise the client doesn't show any weight change until you zone, move an item, etc
		c->GetTarget()->CastToClient()->TakeMoneyFromPP(atoi(sep->arg[4]), atoi(sep->arg[3]), atoi(sep->arg[2]), atoi(sep->arg[1]), true);
	}
}

