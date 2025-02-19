#include "../client.h"

void command_summonitem(Client *c, const Seperator *sep)
{
	uint32       item_id    = 0;
	int16        charges    = 0;
	const uint16 arguments  = sep->argnum;
	std::string  cmd_msg    = sep->msg;
	size_t       link_open  = cmd_msg.find('\x12');
	size_t       link_close = cmd_msg.find_last_of('\x12');

	if (link_open != link_close && (cmd_msg.length() - link_open) > EQ::constants::SAY_LINK_BODY_SIZE) {
		EQ::SayLinkBody_Struct link_body;
		EQ::saylink::DegenerateLinkBody(link_body, cmd_msg.substr(link_open + 1, EQ::constants::SAY_LINK_BODY_SIZE));
		item_id = link_body.item_id;
	}
	else if (!sep->IsNumber(1)) {
		c->Message(Chat::White, "Usage: #summonitem [item id] [charges], charges are optional");
		return;
	}
	else {
		item_id = Strings::ToUnsignedInt(sep->arg[1]);
	}
	
	if (!item_id) {
		c->Message(Chat::White, "Enter a valid item ID.");
		return;
	}

	uint8       item_status = 0;
	const uint8 current_status = c->Admin();

	const auto *item = database.GetItem(item_id);
	if (!item) {
		c->Message(
			Chat::White,
			fmt::format(
				"Item ID {} does not exist.",
				item_id
			).c_str()
		);
		return;
	}

	item_status = item->MinStatus;

	if (item_status > current_status || (c->Admin() < 80 && item_id == 2661)) {
		c->Message(
			Chat::White,
			fmt::format(
				"Insufficient status to summon this item, current status is {}, required status is {}.",
				current_status,
				item_status
			).c_str()
		);
		return;
	}

	if (arguments < 2 || sep->IsNumber(2)) {
		if(item && database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges) {
			charges = item->MaxCharges;
		}
		else {
			charges = 0;
		}
	}
	else {
		charges = static_cast<int16>(Strings::ToInt(sep->arg[2]));
	}


	c->SummonItem(
		item_id, 
		charges, 
		0, 
		true
	);

	const auto *new_item = database.CreateItem(
		item_id,
		charges
	);

	EQ::SayLinkEngine linker;
	linker.SetLinkType(EQ::saylink::SayLinkItemInst);
	linker.SetItemInst(new_item);

	const std::string &item_link = linker.GenerateLink();

	c->Message(
		Chat::White,
		fmt::format(
			"You have summoned {}.",
			item_link
		).c_str()
	);

	safe_delete(new_item);
}

