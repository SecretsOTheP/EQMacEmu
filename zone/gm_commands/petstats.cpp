#include "../client.h"

void command_petstats(Client* c, const Seperator* sep)
{
	Mob* pet = c->GetPet();

	if (!pet)
	{
		c->Message(Chat::White, "You do not have a pet.");
		return;
	}

	if (!pet->IsNPC())
	{
		c->Message(Chat::White, "Your pet is not a valid target for this command.");
		return;
	}

	NPC* pet_npc = pet->CastToNPC();
	c->Message(Chat::White, "-- %s's Stats --", pet->GetCleanName());
	c->Message(Chat::White, "HP: %d / %d", pet->GetHP(), pet->GetMaxHP());
	c->Message(Chat::White, "AC: %d", pet->GetAC());
	c->Message(Chat::White, "ATK: %d", pet->GetATK());

	uint32 min_dmg = pet_npc->GetMinDMG();
	uint32 max_dmg = pet_npc->GetMaxDMG();
	double avg_dmg = (min_dmg + max_dmg) / 2.0;
	int32 delay_ms = pet->GetAttackTimer().GetDuration();

	c->Message(Chat::White, "Attack Damage: %u - %u (avg %.1f)", min_dmg, max_dmg, avg_dmg);
	c->Message(Chat::White, "Attack Delay: %d ms (%.2fs)", delay_ms, delay_ms / 1000.0);

	if (delay_ms > 0)
		c->Message(Chat::White, "Melee DPS: %.1f", avg_dmg / (delay_ms / 1000.0));
	else
		c->Message(Chat::White, "Melee DPS: n/a");

	c->Message(Chat::White, "Resists: Magic %d  Fire %d  Cold %d  Poison %d  Disease %d",
		pet->GetMR(), pet->GetFR(), pet->GetCR(), pet->GetPR(), pet->GetDR());

	c->Message(Chat::White, "-- Equipped Inventory --");
	for (int16 inv_slot = EQ::invslot::EQUIPMENT_BEGIN; inv_slot <= EQ::invslot::EQUIPMENT_END; inv_slot++)
	{
		uint32 item_id = pet_npc->GetEquipmentByInvSlot(inv_slot);
		const char* slot_name = EQ::invslot::GetInvPossessionsSlotName(inv_slot);

		if (item_id == 0)
		{
			c->Message(Chat::White, "%s: (Empty)", slot_name);
			continue;
		}

		const EQ::ItemData* item = database.GetItem(item_id);
		c->Message(Chat::White, "%s: %s", slot_name, item ? item->Name : "Unknown Item");
	}
}
