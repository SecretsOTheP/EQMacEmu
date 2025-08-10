/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemu.org)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/misc_functions.h"
#include "../common/events/player_event_logs.h"

#include "client.h"
#include "entity.h"
#include "mob.h"

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "queryserv.h"

#include "worldserver.h"

class QueryServ;

extern QueryServ* QServ;
extern WorldServer worldserver;

// The maximum amount of a single bazaar transaction expressed in copper.
// Equivalent to 2 Million plat
#define MAX_TRANSACTION_VALUE 2000000000
// ##########################################
// Trade implementation
// ##########################################

Trade::Trade(Mob* in_owner)
{
	owner = in_owner;
	Reset();
}

Trade::~Trade()
{
	Reset();
}

void Trade::Reset()
{
	state = TradeNone;
	with_id = 0;
	pp=0; gp=0; sp=0; cp=0;
}

// Initiate a trade with another mob
// initiate_with specifies whether to start trade with other mob as well
void Trade::Request(uint32 mob_id)
{
	Reset();
	state = Requesting;
	with_id = mob_id;
}

// Initiate a trade with another mob
// initiate_with specifies whether to start trade with other mob as well
void Trade::Start(uint32 mob_id, bool initiate_with)
{
	Reset();
	state = Trading;
	with_id = mob_id;

	// Autostart on other mob?
	if (initiate_with) {
		Mob* with = With();
		if (with)
			with->trade->Start(owner->GetID(), false);
	}
}

// Add item from a given slot to trade bucket (automatically does bag data too)
void Trade::AddEntity(uint16 trade_slot_id, uint32 stack_size) {
	// TODO: review for inventory saves / consider changing return type to bool so failure can be passed to desync handler

	if (!owner || !owner->IsClient()) {
		// This should never happen
		Log(Logs::General, Logs::Trading, "Programming error: NPC's should not call Trade::AddEntity()");
		return;
	}

	// If one party accepted the trade then an item was added, their state needs to be reset
	owner->trade->state = Trading;
	Mob* with = With();
	if (with)
		with->trade->state = Trading;

	// Item always goes into trade bucket from cursor
	Client* client = owner->CastToClient();
	EQ::ItemInstance* inst = client->GetInv().GetItem(EQ::invslot::slotCursor);

	if (!inst) {
		client->Message(Chat::Red, "Error: Could not find item on your cursor!");
		return;
	}

	EQ::ItemInstance* inst2 = client->GetInv().GetItem(trade_slot_id);

	// it looks like the original code attempted to allow stacking...
	// (it just didn't handle partial stack move actions -U)
	if (stack_size > 0) {
		if (!inst->IsStackable() || !inst2 || !inst2->GetItem() || (inst->GetID() != inst2->GetID()) || (stack_size > inst->GetCharges())) {
			client->Kick();
			return;
		}

		uint32 _stack_size = 0;

		if ((stack_size + inst2->GetCharges()) > inst2->GetItem()->StackSize) {
			_stack_size = (stack_size + inst2->GetCharges()) - inst->GetItem()->StackSize;
			inst2->SetCharges(inst2->GetItem()->StackSize);
		}
		else {
			_stack_size = inst->GetCharges() - stack_size;
			inst2->SetCharges(stack_size + inst2->GetCharges());
		}

		Log(Logs::Detail, Logs::Trading, "%s added partial item '%s' stack (qty: %i) to trade slot %i", owner->GetName(), inst->GetItem()->Name, stack_size, trade_slot_id);

		// if something is already occupying the trade slot on the destination client, sending another item update adds to the quantity instead of replacing it
		SendItemData(inst, trade_slot_id);

		if (_stack_size > 0)
			inst->SetCharges(_stack_size);
		else
			client->DeleteItemInInventory(EQ::invslot::slotCursor);
	}
	else {
		if (inst2 && inst2->GetID()) {
			client->Kick();
			return;
		}

		Log(Logs::Detail, Logs::Trading, "%s added item '%s' to trade slot %i", owner->GetName(), inst->GetItem()->Name, trade_slot_id);
		client->PutItemInInventory(trade_slot_id, *inst);
		client->DeleteItemInInventory(EQ::invslot::slotCursor);
		SendItemData(inst, trade_slot_id);
	}
}

// Retrieve mob the owner is trading with
// Done like this in case 'with' mob goes LD and Mob* becomes invalid
Mob* Trade::With()
{
	return entity_list.GetMob(with_id);
}

// Private Method: Send item data for trade item to other person involved in trade
void Trade::SendItemData(const EQ::ItemInstance* inst, int16 dest_slot_id)
{
	if (inst == nullptr)
		return;
	
	// @merth: This needs to be redone with new item classes
	Mob* mob = With();
	if (!mob->IsClient())
		return; // Not sending packets to NPCs!

	Client* with = mob->CastToClient();
	Client* trader = owner->CastToClient();
	if (with && with->IsClient()) {
		int16 fromid = 0;
		fromid = trader->GetID(); 
		with->SendItemPacket(dest_slot_id - EQ::invslot::TRADE_BEGIN, inst, ItemPacketTradeView, fromid);
		Log(Logs::Detail, Logs::Trading, "Sending trade item packet for item in slot %d", dest_slot_id - EQ::invslot::TRADE_BEGIN);
		if (inst->GetItem()->ItemClass == 1) {
			for (uint16 i = EQ::invbag::SLOT_BEGIN; i <= EQ::invbag::SLOT_END; i++) {
				uint16 bagslot_id = EQ::InventoryProfile::CalcSlotId(dest_slot_id, i);
				const EQ::ItemInstance* bagitem = trader->GetInv().GetItem(bagslot_id);
				if (bagitem) {
					with->SendItemPacket(bagslot_id - EQ::invslot::TRADE_BEGIN, bagitem, ItemPacketTradeView);
					Log(Logs::Detail, Logs::Trading, "Sending bagged trade item packet for item in slot %d", bagslot_id - EQ::invslot::TRADE_BEGIN);
				}
			}
		}

		//safe_delete(outapp);
	}
}

Mob *Trade::GetOwner() const
{
	return owner;
}

void Client::ResetTrade() {
	AddMoneyToPP(trade->cp, trade->sp, trade->gp, trade->pp, true);

	// step 1: process bags
	for (int16 trade_slot = EQ::invslot::TRADE_BEGIN; trade_slot <= EQ::invslot::TRADE_END; ++trade_slot) {
		const EQ::ItemInstance* inst = m_inv[trade_slot];

		if (inst && inst->IsClassBag()) {
			int16 free_slot = m_inv.FindFreeSlotForTradeItem(inst);

			if (free_slot != INVALID_INDEX) {
				PutItemInInventory(free_slot, *inst);
				SendItemPacket(free_slot, inst, ItemPacketTrade);
			}
			else {
				CreateGroundObject(inst, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
			}

			DeleteItemInInventory(trade_slot);
		}
	}

	// step 2a: process stackables
	for (int16 trade_slot = EQ::invslot::TRADE_BEGIN; trade_slot <= EQ::invslot::TRADE_END; ++trade_slot) {
		EQ::ItemInstance* inst = GetInv().GetItem(trade_slot);

		if (inst && inst->IsStackable()) {
			while (true) {
				// there's no built-in safety check against an infinite loop..but, it should break on one of the conditional checks
				int16 free_slot = m_inv.FindFreeSlotForTradeItem(inst);

				if ((free_slot == EQ::invslot::slotCursor) || (free_slot == INVALID_INDEX))
					break;

				EQ::ItemInstance* partial_inst = GetInv().GetItem(free_slot);

				if (!partial_inst)
					break;

				if (partial_inst->GetID() != inst->GetID()) {
					Log(Logs::Detail, Logs::Trading, "[CLIENT] Client::ResetTrade() - an incompatible location reference was returned by Inventory::FindFreeSlotForTradeItem()");

					break;
				}

				if ((partial_inst->GetCharges() + inst->GetCharges()) > partial_inst->GetItem()->StackSize) {
					int16 new_charges = (partial_inst->GetCharges() + inst->GetCharges()) - partial_inst->GetItem()->StackSize;

					partial_inst->SetCharges(partial_inst->GetItem()->StackSize);
					inst->SetCharges(new_charges);
				}
				else {
					partial_inst->SetCharges(partial_inst->GetCharges() + inst->GetCharges());
					inst->SetCharges(0);
				}

				PutItemInInventory(free_slot, *partial_inst);
				SendItemPacket(free_slot, partial_inst, ItemPacketTrade);

				if (inst->GetCharges() == 0) {
					DeleteItemInInventory(trade_slot);

					break;
				}
			}
		}
	}

	// step 2b: adjust trade stack bias
	// (if any partial stacks exist before the final stack, FindFreeSlotForTradeItem() will return that slot in step 3 and an overwrite will occur)
	for (int16 trade_slot = EQ::invslot::TRADE_END; trade_slot >= EQ::invslot::TRADE_BEGIN; --trade_slot) {
		EQ::ItemInstance* inst = GetInv().GetItem(trade_slot);

		if (inst && inst->IsStackable()) {
			for (int16 bias_slot = EQ::invslot::TRADE_BEGIN; bias_slot <= EQ::invslot::TRADE_END; ++bias_slot) {
				if (bias_slot >= trade_slot)
					break;

				EQ::ItemInstance* bias_inst = GetInv().GetItem(bias_slot);

				if (!bias_inst || (bias_inst->GetID() != inst->GetID()) || (bias_inst->GetCharges() >= bias_inst->GetItem()->StackSize))
					continue;

				if ((bias_inst->GetCharges() + inst->GetCharges()) > bias_inst->GetItem()->StackSize) {
					int16 new_charges = (bias_inst->GetCharges() + inst->GetCharges()) - bias_inst->GetItem()->StackSize;

					bias_inst->SetCharges(bias_inst->GetItem()->StackSize);
					inst->SetCharges(new_charges);
				}
				else {
					bias_inst->SetCharges(bias_inst->GetCharges() + inst->GetCharges());
					inst->SetCharges(0);
				}

				if (inst->GetCharges() == 0) {
					DeleteItemInInventory(trade_slot);

					break;
				}
			}
		}
	}

	// step 3: process everything else
	for (int16 trade_slot = EQ::invslot::TRADE_BEGIN; trade_slot <= EQ::invslot::TRADE_END; ++trade_slot) {
		const EQ::ItemInstance* inst = m_inv[trade_slot];

		if (inst) {
			int16 free_slot = m_inv.FindFreeSlotForTradeItem(inst);

			if (free_slot != INVALID_INDEX) {
				PutItemInInventory(free_slot, *inst);
				SendItemPacket(free_slot, inst, ItemPacketTrade);
			}
			else {
				CreateGroundObject(inst, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
			}

			DeleteItemInInventory(trade_slot);
		}
	}
}

void Client::FinishTrade(Mob *tradingWith, bool finalizer, void *event_entry)
{
	bool qs_log = false;
	uint16 item_count = 0;
	if (tradingWith && tradingWith->IsClient()) {
		Client *other = tradingWith->CastToClient();

		if (other) {
			Log(Logs::Detail, Logs::Trading, "%s is finishing trade with client %s", GetName(), other->GetName());

			this->AddMoneyToPP(other->trade->cp, other->trade->sp, other->trade->gp, other->trade->pp, true);

			// QS code
			if (RuleB(QueryServ, PlayerLogTrades))
			{
				qs_log = true;
			}

			// step 1: process bags
			for (int16 trade_slot = EQ::invslot::TRADE_BEGIN; trade_slot <= EQ::invslot::TRADE_END; ++trade_slot) {
				const EQ::ItemInstance *inst = m_inv[trade_slot];
				bool dropitem = false;

				if (inst && inst->IsClassBag()) {
					Log(Logs::Detail, Logs::Trading, "Giving container %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot, other->GetName());

					if (inst->GetItem()->NoDrop != 0 || Admin() >= RuleI(Character, MinStatusForNoDropExemptions) || RuleI(World, FVNoDropFlag) == 1 || other == this) {
						int16 free_slot = other->GetInv().FindFreeSlotForTradeItem(inst);

						if (free_slot != INVALID_INDEX)
						{
							if (other->PutItemInInventory(free_slot, *inst, true))
							{
								Log(Logs::Detail, Logs::Trading, "Container %s (%d) successfully transferred, deleting from trade slot %d.", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot);

								if (qs_log)
								{
									QServ->QSTradeItems(this->character_id, other->CharacterID(), trade_slot, free_slot, inst->GetID(), inst->GetCharges(), false);
									++item_count;
								}

								// step 1a: process items in bags
								uint8 bagidx = 0;
								for (int16 trade_bag_slot = EQ::invbag::TRADE_BAGS_BEGIN + (trade_slot - EQ::invslot::TRADE_BEGIN) * EQ::invbag::SLOT_COUNT;
									trade_bag_slot <= EQ::invbag::TRADE_BAGS_BEGIN + (trade_slot - EQ::invslot::TRADE_BEGIN) * EQ::invbag::SLOT_COUNT + 9;
									++trade_bag_slot)
								{
									const EQ::ItemInstance *inst = m_inv[trade_bag_slot];

									if (inst) {
										Log(Logs::Detail, Logs::Trading, "Giving item in container %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_bag_slot, other->GetName());

										if (inst->GetItem()->NoDrop != 0 || Admin() >= RuleI(Character, MinStatusForNoDropExemptions) || RuleI(World, FVNoDropFlag) == 1 || other == this)
										{
											int16 free_bag_slot = other->GetInv().CalcSlotId(free_slot, bagidx);
											Log(Logs::Detail, Logs::Trading, "Free slot is: %i. Slot bag is in: %i Index is: %i", free_bag_slot, free_slot, bagidx);
											if (free_bag_slot != INVALID_INDEX)
											{
												if (other->PutItemInInventory(free_bag_slot, *inst, true))
												{
													Log(Logs::Detail, Logs::Trading, "Container item %s (%d) successfully transferred, deleting from trade slot %d.", inst->GetItem()->Name, inst->GetItem()->ID, trade_bag_slot);

													if (qs_log)
													{
														QServ->QSTradeItems(this->character_id, other->CharacterID(), trade_bag_slot, free_bag_slot, inst->GetID(), inst->GetCharges(), true);
														++item_count;
													}
												}

												else {
													Log(Logs::Detail, Logs::Trading, "Transfer of container item %s (%d) to %s failed, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID, other->GetName());
													dropitem = true;
												}
											}
											else {
												Log(Logs::Detail, Logs::Trading, "%s's inventory is full, returning container item %s (%d) to giver.", other->GetName(), inst->GetItem()->Name, inst->GetItem()->ID);
												dropitem = true;
											}
										}
										else
										{
											Log(Logs::Detail, Logs::Trading, "Container item %s (%d) is NoDrop, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID);
											PushItemOnCursor(*inst, true);
										}
										DeleteItemInInventory(trade_bag_slot);
									}
									if (dropitem)
									{
										other->CreateGroundObject(inst, glm::vec4(other->GetX(), other->GetY(), other->GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
									}
									bagidx++;
								}
							}
							else {
								Log(Logs::Detail, Logs::Trading, "Transfer of container %s (%d) to %s failed, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID, other->GetName());
								dropitem = true;
							}
						}
						else {
							Log(Logs::Detail, Logs::Trading, "%s's inventory is full, returning container %s (%d) to giver.", other->GetName(), inst->GetItem()->Name, inst->GetItem()->ID);
							dropitem = true;
						}
					}
					else {
						Log(Logs::Detail, Logs::Trading, "Container %s (%d) is NoDrop, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID);
						PushItemOnCursor(*inst, true);
					}

					DeleteItemInInventory(trade_slot);
				}
				if (dropitem)
				{
					other->CreateGroundObject(inst, glm::vec4(other->GetX(), other->GetY(), other->GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
				}
			}

			// step 2a: process stackables
			for (int16 trade_slot = EQ::invslot::TRADE_BEGIN; trade_slot <= EQ::invslot::TRADE_END; ++trade_slot) {
				EQ::ItemInstance *inst = GetInv().GetItem(trade_slot);

				if (inst && inst->IsStackable()) {
					while (true) {
						// there's no built-in safety check against an infinite loop..but, it should break on one of the conditional checks
						int16 partial_slot = other->GetInv().FindFreeSlotForTradeItem(inst);

						if ((partial_slot == EQ::invslot::slotCursor) || (partial_slot == INVALID_INDEX))
							break;

						EQ::ItemInstance *partial_inst = other->GetInv().GetItem(partial_slot);

						if (!partial_inst)
							break;

						if (partial_inst->GetID() != inst->GetID()) {
							Log(Logs::Detail, Logs::Trading, "[CLIENT] Client::ResetTrade() - an incompatible location reference was returned by Inventory::FindFreeSlotForTradeItem()");
							break;
						}

						int16 old_charges = inst->GetCharges();
						int16 partial_charges = partial_inst->GetCharges();

						if ((partial_inst->GetCharges() + inst->GetCharges()) > partial_inst->GetItem()->StackSize) {
							int16 new_charges = (partial_inst->GetCharges() + inst->GetCharges()) - partial_inst->GetItem()->StackSize;

							partial_inst->SetCharges(partial_inst->GetItem()->StackSize);
							inst->SetCharges(new_charges);
						}
						else {
							partial_inst->SetCharges(partial_inst->GetCharges() + inst->GetCharges());
							inst->SetCharges(0);
						}

						Log(Logs::Detail, Logs::Trading, "Transferring partial stack %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot, other->GetName());

						if (other->PutItemInInventory(partial_slot, *partial_inst, true))
						{
							Log(Logs::Detail, Logs::Trading, "Partial stack %s (%d) successfully transferred, deleting %i charges from trade slot.",
								inst->GetItem()->Name, inst->GetItem()->ID, (old_charges - inst->GetCharges()));

							if (qs_log)
							{
								QServ->QSTradeItems(this->character_id, other->CharacterID(), trade_slot, partial_slot, partial_inst->GetID(), partial_inst->GetCharges(), false);
								++item_count;
							}
						}
						else {
							Log(Logs::Detail, Logs::Trading, "Transfer of partial stack %s (%d) to %s failed, returning %i charges to trade slot.",
								inst->GetItem()->Name, inst->GetItem()->ID, other->GetName(), (old_charges - inst->GetCharges()));

							inst->SetCharges(old_charges);
							partial_inst->SetCharges(partial_charges);
							break;
						}

						if (inst->GetCharges() == 0) {
							DeleteItemInInventory(trade_slot);
							break;
						}
					}
				}
			}

			// step 2b: adjust trade stack bias
			// (if any partial stacks exist before the final stack, FindFreeSlotForTradeItem() will return that slot in step 3 and an overwrite will occur)
			for (int16 trade_slot = EQ::invslot::TRADE_END; trade_slot >= EQ::invslot::TRADE_BEGIN; --trade_slot) {
				EQ::ItemInstance *inst = GetInv().GetItem(trade_slot);

				if (inst && inst->IsStackable()) {
					for (int16 bias_slot = EQ::invslot::TRADE_BEGIN; bias_slot <= EQ::invslot::TRADE_END; ++bias_slot) {
						if (bias_slot >= trade_slot)
							break;

						EQ::ItemInstance *bias_inst = GetInv().GetItem(bias_slot);

						if (!bias_inst || (bias_inst->GetID() != inst->GetID()) || (bias_inst->GetCharges() >= bias_inst->GetItem()->StackSize))
							continue;

						int16 old_charges = inst->GetCharges();

						if ((bias_inst->GetCharges() + inst->GetCharges()) > bias_inst->GetItem()->StackSize) {
							int16 new_charges = (bias_inst->GetCharges() + inst->GetCharges()) - bias_inst->GetItem()->StackSize;

							bias_inst->SetCharges(bias_inst->GetItem()->StackSize);
							inst->SetCharges(new_charges);
						}
						else {
							bias_inst->SetCharges(bias_inst->GetCharges() + inst->GetCharges());
							inst->SetCharges(0);
						}

						if (inst->GetCharges() == 0) {
							DeleteItemInInventory(trade_slot);
							break;
						}
					}
				}
			}

			// step 3: process everything else
			for (int16 trade_slot = EQ::invslot::TRADE_BEGIN; trade_slot <= EQ::invslot::TRADE_END; ++trade_slot) {
				const EQ::ItemInstance *inst = m_inv[trade_slot];
				bool dropitem = false;

				if (inst) {
					Log(Logs::Detail, Logs::Trading, "Giving item %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot, other->GetName());

					if (inst->GetItem()->NoDrop != 0 || Admin() >= RuleI(Character, MinStatusForNoDropExemptions) || RuleI(World, FVNoDropFlag) == 1 || other == this) {
						int16 free_slot = other->GetInv().FindFreeSlotForTradeItem(inst);

						if (free_slot != INVALID_INDEX) {
							if (other->PutItemInInventory(free_slot, *inst, true))
							{
								Log(Logs::Detail, Logs::Trading, "Item %s (%d) successfully transferred, deleting from trade slot %d.", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot);
								if (qs_log)
								{
									QServ->QSTradeItems(this->character_id, other->CharacterID(), trade_slot, free_slot, inst->GetID(), inst->GetCharges(), false);
									++item_count;
								}
							}
							else {
								Log(Logs::Detail, Logs::Trading, "Transfer of Item %s (%d) to %s failed, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID, other->GetName());
								dropitem = true;
							}
						}
						else {
							Log(Logs::Detail, Logs::Trading, "%s's inventory is full, returning item %s (%d) to giver.", other->GetName(), inst->GetItem()->Name, inst->GetItem()->ID);
							dropitem = true;
						}
					}
					else {
						Log(Logs::Detail, Logs::Trading, "Item %s (%d) is NoDrop, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID);
						PushItemOnCursor(*inst, true);
					}

					DeleteItemInInventory(trade_slot);
				}
				if (dropitem)
				{
					other->CreateGroundObject(inst, glm::vec4(other->GetX(), other->GetY(), other->GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
				}
			}

			if (qs_log && event_entry)
			{
				QSPlayerLogTrade_Struct *qs_audit = (QSPlayerLogTrade_Struct *)event_entry;
				if (finalizer)
				{
					qs_audit->char2_id = this->character_id;
					qs_audit->char2_money.platinum = this->trade->pp;
					qs_audit->char2_money.gold = this->trade->gp;
					qs_audit->char2_money.silver = this->trade->sp;
					qs_audit->char2_money.copper = this->trade->cp;
					qs_audit->char2_count = item_count;
				}
				else
				{
					qs_audit->char1_id = this->character_id;
					qs_audit->char1_money.platinum = this->trade->pp;
					qs_audit->char1_money.gold = this->trade->gp;
					qs_audit->char1_money.silver = this->trade->sp;
					qs_audit->char1_money.copper = this->trade->cp;
					qs_audit->char1_count = item_count;
				}
			}

			//Do not reset the trade here, done by the caller.
		}
	}
	else if (tradingWith && tradingWith->IsNPC()) {
		NPCHandinEventLog(trade, tradingWith->CastToNPC());

		NPC *npc = tradingWith->CastToNPC();

		if (!npc)
			return;

		bool badfaction = false;

		// QS code
		if (RuleB(QueryServ, PlayerLogTrades))
		{
			qs_log = true;
		}

		bool quest_npc = false;
		if (parse->HasQuestSub(tradingWith->GetNPCTypeID(), EVENT_TRADE)) {
			// This is a quest NPC
			quest_npc = true;
		}

		std::vector<std::any> item_list;
		uint32 items[4] = { 0 };
		for (int i = EQ::invslot::TRADE_BEGIN; i <= EQ::invslot::TRADE_NPC_END; ++i) {
			EQ::ItemInstance *inst = m_inv.GetItem(i);
			if (inst) {
				items[i - EQ::invslot::TRADE_BEGIN] = inst->GetItem()->ID;
				item_list.push_back(inst);
			}
			else {
				item_list.push_back((EQ::ItemInstance *)nullptr);
				continue;
			}

			const EQ::ItemData *item = inst->GetItem();
			if (npc->GetSpecialAbility(SpecialAbility::BadFactionBlockHandin) && item && quest_npc == true && !GetGM())
			{
				int primaryfaction = npc->GetPrimaryFaction();
				int factionlvl = GetFactionLevel(CharacterID(), GetRace(), GetClass(), GetDeity(), primaryfaction, tradingWith);

				if (factionlvl >= FACTION_THREATENINGLY)
				{
					badfaction = true;

					if (RuleB(NPC, ReturnNonQuestItems))
					{
						DeleteItemInInventory(i);
						if (npc->CanTalk())
						npc->Say_StringID(zone->random.Int(StringID::TRADE_BAD_FACTION1, StringID::TRADE_BAD_FACTION4));
						SummonItem(inst->GetID(), inst->GetCharges(), EQ::legacy::SLOT_QUEST, true, inst->GetQuarmItemData());
						Log(Logs::General, Logs::Trading, "Quest NPC %s is returning %s because the faction check has failed.", npc->GetName(), item->Name);

					}
					// Add items to loottable without equipping and mark as quest.
					else
					{
						if (item->NoDrop != 0 && npc->CountQuestItem(item->ID) == 0)
						{
							auto loot_drop_entry = LootdropEntriesRepository::NewNpcEntity();
							loot_drop_entry.item_charges = static_cast<int8>(inst->GetCharges());
							bool pet = true; // setting pet(traded) flag to prevent self-found from looting this like a normal drop
							npc->AddLootDrop(item, loot_drop_entry, true, true, false, pet, false, inst->GetQuarmItemData());
							Log(Logs::General, Logs::Trading, "Adding loot item %s to Quest NPC %s due to bad faction.", item->Name, npc->GetName());
						}
					}
				}
			}
			else if (item && quest_npc == false)
			{
				if (Admin() == 0)
				{
				// if it was not a NO DROP (or if a GM is trading), let the pet have it
					if (GetGM() || npc->IsPet())
					{
						// pets need to look inside bags and try to equip items found there
						if (item->IsClassBag() && item->BagSlots > 0)
						{
							for (int16 bslot = EQ::invbag::SLOT_BEGIN; bslot < item->BagSlots; bslot++)
							{
								const EQ::ItemInstance* baginst = inst->GetItem(bslot);
								if (baginst) {
									const EQ::ItemData* bagitem = baginst->GetItem();
									if (bagitem)
									{
										if (GetGM())
										{
											auto loot_drop_entry = LootdropEntriesRepository::NewNpcEntity();
											loot_drop_entry.item_charges = static_cast<int8>(baginst->GetCharges());
											npc->AddLootDrop(bagitem, loot_drop_entry, true, true);
											Log(Logs::General, Logs::Trading, "GM: Adding loot item %s (bag) to non-Quest NPC %s", bagitem->Name, npc->GetName());
										}
										// Destroy duplicate and nodrop items on charmed pets.
										else if (bagitem->NoDrop != 0 &&
											(!npc->IsCharmedPet() || (npc->IsCharmedPet() && npc->CountQuestItem(bagitem->ID) == 0)))
										{
											npc->AddPetLoot(bagitem->ID, baginst->GetCharges(), false, baginst->GetQuarmItemData());
											Log(Logs::General, Logs::Trading, "Adding loot item %s (bag) to non-Quest pet %s", bagitem->Name, npc->GetName());
										}
									}
									else if (RuleB(NPC, ReturnNonQuestItems))
									{
										SummonItem(baginst->GetID(), baginst->GetCharges(), EQ::legacy::SLOT_QUEST, true, baginst->GetQuarmItemData());
										if (npc->CanTalk())
											npc->Say_StringID(StringID::NO_NEED_FOR_ITEM, GetName());
										Log(Logs::General, Logs::Trading, "Non-Quest NPC %s is returning %s (bag) because it does not require it.", npc->GetName(), bagitem->Name);
									}
									else
									{
										if (bagitem->NoDrop != 0 && npc->CountQuestItem(bagitem->ID) == 0)
										{
											npc->AddQuestLoot(bagitem->ID, baginst->GetCharges(), baginst->GetQuarmItemData());
											Log(Logs::General, Logs::Trading, "Adding loot item %s (bag) to non-Quest NPC %s", bagitem->Name, npc->GetName());
										}
									}
								}
							}
						}
					}
					if (GetGM())
					{
						auto loot_drop_entry = LootdropEntriesRepository::NewNpcEntity();
						loot_drop_entry.item_charges = static_cast<int8>(inst->GetCharges());
						npc->AddLootDrop(item, loot_drop_entry, true, true);
						Log(Logs::General, Logs::Trading, "GM: Adding loot item %s to non-Quest NPC %s", item->Name, npc->GetName());
					}
					// Destroy duplicate and nodrop items on charmed pets.
					else if (item->NoDrop != 0 &&
						(!npc->IsCharmedPet() || (npc->IsCharmedPet() && npc->CountQuestItem(item->ID) == 0)))
					{
						npc->AddPetLoot(item->ID, inst->GetCharges(), false, inst->GetQuarmItemData());
						Log(Logs::General, Logs::Trading, "Adding loot item %s to non-Quest pet %s", item->Name, npc->GetName());
					}
				}
				// Return items being handed into a non-quest NPC if the rule is true
				else if (RuleB(NPC, ReturnNonQuestItems))
				{
					DeleteItemInInventory(i);
					SummonItem(inst->GetID(), inst->GetCharges(), EQ::legacy::SLOT_QUEST, true, inst->GetQuarmItemData());
					if(npc->CanTalk())
						npc->Say_StringID(StringID::NO_NEED_FOR_ITEM, GetName());
					Log(Logs::General, Logs::Trading, "Non-Quest NPC %s is returning %s because it does not require it.", npc->GetName(), item->Name);

				}
				// Add items to loottable without equipping and mark as quest.
				else
				{
					if (Admin() == 0)
					{
						if (GetGM() || (item->NoDrop != 0 && npc->CountQuestItem(item->ID) == 0))
						{
							npc->AddQuestLoot(item->ID, inst->GetCharges(), inst->GetQuarmItemData());
							Log(Logs::General, Logs::Trading, "Adding loot item %s to non-Quest NPC %s", item->Name, npc->GetName());
						}
					}
				}
			}

			if (!badfaction && qs_log)
			{
				QServ->QSTradeItems(this->character_id, npc->GetNPCTypeID(), i, 0, inst->GetID(), inst->GetCharges(), false, false);
				++item_count;
				if (inst->IsClassBag())
				{
					for (uint8 sub_slot = EQ::invbag::SLOT_BEGIN; (sub_slot <= EQ::invbag::SLOT_END); ++sub_slot)
					{
						const EQ::ItemInstance *bag_inst = inst->GetItem(sub_slot);
						if (bag_inst)
						{
							QServ->QSTradeItems(this->character_id, npc->GetNPCTypeID(), m_inv.CalcSlotId(i, sub_slot), 0, bag_inst->GetID(), bag_inst->GetCharges(), true, false);
							++item_count;
						}
					}
				}
			}
		}

		if (!badfaction)
		{
			if (qs_log)
			{
				QSPlayerLogHandin_Struct *qs_struct = new struct QSPlayerLogHandin_Struct;

				qs_struct->char_id = character_id;
				qs_struct->char_money.platinum = trade->pp;
				qs_struct->char_money.gold = trade->gp;
				qs_struct->char_money.silver = trade->sp;
				qs_struct->char_money.copper = trade->cp;
				qs_struct->char_count = item_count;
				qs_struct->npc_id = npc->GetNPCTypeID();

				QServ->QSHandinItems(qs_struct);
				safe_delete(qs_struct);
			}

			char temp1[100] = { 0 };
			char temp2[100] = { 0 };
			snprintf(temp1, 100, "copper.%d", tradingWith->GetNPCTypeID());
			snprintf(temp2, 100, "%u", trade->cp);
			parse->AddVar(temp1, temp2);
			snprintf(temp1, 100, "silver.%d", tradingWith->GetNPCTypeID());
			snprintf(temp2, 100, "%u", trade->sp);
			parse->AddVar(temp1, temp2);
			snprintf(temp1, 100, "gold.%d", tradingWith->GetNPCTypeID());
			snprintf(temp2, 100, "%u", trade->gp);
			parse->AddVar(temp1, temp2);
			snprintf(temp1, 100, "platinum.%d", tradingWith->GetNPCTypeID());
			snprintf(temp2, 100, "%u", trade->pp);
			parse->AddVar(temp1, temp2);

			EQ::ItemInstance *insts[4] = { 0 };
			for (int i = EQ::invslot::TRADE_BEGIN; i <= EQ::invslot::TRADE_NPC_END; ++i) {
				insts[i - EQ::invslot::TRADE_BEGIN] = m_inv.PopItem(i);
				database.SaveInventory(AccountID(), CharacterID(), nullptr, i);
			}

			parse->EventNPC(EVENT_TRADE, tradingWith->CastToNPC(), this, "", 0, &item_list);

			for (int i = 0; i < 4; ++i) {
				if (insts[i]) {
					safe_delete(insts[i]);
				}
			}
		}
	}
}



bool Client::CheckTradeLoreConflict(Client* other)
{
	if (!other)
		return true;

	// Move each trade slot into free inventory slot
	for (int16 index = EQ::invslot::TRADE_BEGIN; index <= EQ::invslot::TRADE_END; index++) {
		const EQ::ItemInstance* inst = m_inv[index];

		if (inst && inst->GetItem()) {
			if (other->CheckLoreConflict(inst->GetItem()))
				return true;
		}
	}

	for (int16 index = EQ::invbag::TRADE_BAGS_BEGIN; index <= EQ::invbag::TRADE_BAGS_END; index++){
		const EQ::ItemInstance* inst = m_inv[index];

		if (inst && inst->GetItem()) {
			if (other->CheckLoreConflict(inst->GetItem()))
				return true;
		}
	}

	return false;
}

/* Bazaar code begins here */

void Client::Trader_ShowItems(){
	auto outapp = new EQApplicationPacket(OP_Trader, sizeof(Trader_Struct));

	Trader_Struct* outints = (Trader_Struct*)outapp->pBuffer;
	Trader_Struct* TraderItems = database.LoadTraderItem(this->CharacterID());

	for(int i = 0; i < 80; i++){
		outints->ItemCost[i] = TraderItems->ItemCost[i];
		outints->Items[i] = TraderItems->Items[i];
	}
	outints->Code = BazaarTrader_ShowItems;

	QueuePacket(outapp);
	safe_delete(outapp);
	safe_delete(TraderItems);
}

void Client::SendTraderPacket(Client* Trader)
{
	if(!Trader)
		return;

	auto outapp = new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = BazaarTrader_StartTraderMode;
	bts->ID = Trader->GetID();
	bts->unknown = 51;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::Trader_CustomerBrowsing(Client *Customer) {

	if(ClientVersionBit() == EQ::versions::bit_MacPC)
	{
		auto outapp= new EQApplicationPacket(OP_Trader, sizeof(BuyerBrowse_Struct));
		BuyerBrowse_Struct* sis = (BuyerBrowse_Struct*)outapp->pBuffer;
		sis->Code = BazaarTrader_CustomerBrowsing;
		sis->TraderID = Customer->GetID();
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	else
	{
		auto outapp= new EQApplicationPacket(OP_Trader, sizeof(TraderStatus_Struct));
		TraderStatus_Struct* sis = (TraderStatus_Struct*)outapp->pBuffer;
		sis->Code = BazaarTrader_CustomerBrowsing;
		sis->TraderID = Customer->GetID();
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::Trader_CustomerBought(Client *Customer,uint32 Price, uint32 ItemID, uint32 Quantity, const char* ItemName, uint8 SlotID) {

	auto outapp= new EQApplicationPacket(OP_Trader, sizeof(TraderBuy_Struct));
	TraderBuy_Struct* sis = (TraderBuy_Struct*)outapp->pBuffer;

	sis->Action = BazaarBuyItem;
	sis->TraderID = Customer->GetID();
	sis->ItemID = ItemID;
	sis->Price = Price;
	sis->Quantity = Quantity;
	sis->Slot = SlotID;
	strcpy(sis->ItemName,ItemName);
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::BecomeOfflineTrader() {
	
	if (eqs)
	{
		eqs->Close();
		eqs->ReleaseFromUse();
	}

	Trader = true;

	eqs = nullptr;
	linkdead_timer.Disable();
	client_ld_timer.Disable();
	kick_timer.Disable();
	BuffFadeByEffect(SE_Levitate);

	// Notify other clients we are now in trader mode
	auto outapp = new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = BazaarTrader_StartTraderMode;
	bts->ID = this->GetID();

	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);
	client_state = CLIENT_OFFLINE_TRADER;
	UpdateWho();
}

void Client::Trader_StartTrader() {

	if (RuleB(Quarm, EnableOfflineTraders))
		return BecomeOfflineTrader();

	Trader=true;
	UpdateWho();

	auto outapp = new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* sis = (Trader_ShowItems_Struct*)outapp->pBuffer;
	sis->Code = BazaarTrader_StartTraderMode;
	sis->TraderID = this->GetID();
	sis->SubAction = BazaarTrader_StartTraderMode; //Todo: Sending this as 0 sends a message about being in a trader area. Implement that server side.

	QueuePacket(outapp);
	safe_delete(outapp);

	BuffFadeByEffect(SE_Levitate);

	// Notify other clients we are now in trader mode
	outapp= new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = BazaarTrader_StartTraderMode;
	bts->ID = this->GetID();

	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);
}

void Client::Trader_EndTrader() {

	// If someone is looking at our wares, remove all the items from the window.
	//
	if(WithCustomer && IsTrader()) 
	{
		entity_list.SendTraderEnd(this);
	}

	database.DeleteTraderItem(this->CharacterID());

	// Notify other clients we are no longer in trader mode.
	auto outapp = new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = 0;
	bts->ID = this->GetID();

	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);

	outapp= new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* sis = (Trader_ShowItems_Struct*)outapp->pBuffer;
	sis->Code = BazaarTrader_EndTraderMode;

	QueuePacket(outapp);
	safe_delete(outapp);

	WithCustomer = false;
	this->Trader = false;
	UpdateWho();
}

void Client::SendTraderItem(uint32 ItemID, uint16 Quantity) {

	std::string Packet;
	int16 FreeSlotID=0;

	const EQ::ItemData* item = database.GetItem(ItemID);

	if(!item){
		Log(Logs::Detail, Logs::Bazaar, "Bogus item deleted in Client::SendTraderItem!\n");
		return;
	}

	EQ::ItemInstance* inst = database.CreateItem(item, Quantity);

	if (inst)
	{
		bool stacked = TryStacking(inst);

		if(!stacked)
		{
			bool is_arrow = (inst->GetItem()->ItemType == EQ::item::ItemTypeArrow) ? true : false;
			FreeSlotID = m_inv.FindFreeSlot(false, true, inst->GetItem()->Size, is_arrow);

			PutItemInInventory(FreeSlotID, *inst);
			SendItemPacket(FreeSlotID, inst, ItemPacketTrade);
		}

		safe_delete(inst);
	}
}

void Client::SendSingleTraderItem(uint32 CharID, int16 islotid, uint16 slotid) 
{
	EQ::ItemInstance* inst= database.LoadSingleTraderItem(CharID, islotid, slotid);
}

void Client::BulkSendTraderInventory(uint32 char_id) {
	const EQ::ItemData *item;

	TraderCharges_Struct* TraderItems = database.LoadTraderItemWithCharges(char_id);

	uint32 size = 0;
	std::map<uint16, std::string> ser_items;
	std::map<uint16, std::string>::iterator mer_itr;
	for (uint8 i = 0; i < 80; i++) 
	{
		if((TraderItems->ItemID[i] == 0) || (TraderItems->ItemCost[i] <= 0))
			continue;
		else
			item=database.GetItem(TraderItems->ItemID[i]);
		if (item && (item->NoDrop!=0)) 
		{
			EQ::ItemInstance* inst = database.CreateItem(item);
			int32 charges = 1;
			if (inst) 
			{
				if (database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Charges)
					charges = TraderItems->Charges[i];

				if(TraderItems->Charges[i] > 0)
					inst->SetCharges(charges);

				if(inst->IsStackable()) {
					inst->SetMerchantCount(charges);
				}
				inst->SetMerchantSlot(i);
				inst->SetPrice(TraderItems->ItemCost[i]);
				std::string packet = inst->Serialize(30);
				ser_items[i] = packet;
				size += packet.length();
			}
			else
				Log(Logs::Detail, Logs::Bazaar, "Client::BulkSendTraderInventory nullptr inst pointer");
		}
		else
			Log(Logs::Detail, Logs::Bazaar, "Client::BulkSendTraderInventory nullptr item pointer or item is NODROP %8X",item);
	}

	int8 count = 0;
	auto outapp = new EQApplicationPacket(OP_ShopInventoryPacket, size);
	uchar* ptr = outapp->pBuffer;
	for(mer_itr = ser_items.begin(); mer_itr != ser_items.end(); mer_itr++)
	{
		int length = mer_itr->second.length();
		if(length > 5) 
		{
			memcpy(ptr, mer_itr->second.c_str(), length);
			ptr += length;
			count++;
		}
		if(count >= 80)
			break;
	}

	QueuePacket(outapp);
	safe_delete(outapp);
	safe_delete(TraderItems);
}

void Client::DisplayTraderInventory(Client* client) 
{

	if (!client || GetZoneID() != Zones::BAZAAR)
		return;

	if (!client->IsTrader())
	{
		Message(Chat::White, "%s is not a trader", client->GetName());
		return;
	}

	const EQ::ItemData *item;
	TraderCharges_Struct* TraderItems = database.LoadTraderItemWithCharges(client->CharacterID());

	uint8 count = 0;
	for (uint8 i = 0; i < 80; i++)
	{
		if ((TraderItems->ItemID[i] == 0) || (TraderItems->ItemCost[i] <= 0))
			continue;
		else
			item = database.GetItem(TraderItems->ItemID[i]);
		if (item && (item->NoDrop != 0))
		{
			EQ::ItemInstance* inst = database.CreateItem(item);
			int32 charges = 1;
			if (inst)
			{
				uint32 cost = TraderItems->ItemCost[i];
				if (database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Charges)
					charges = TraderItems->Charges[i];

				if (database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Stacked)
				{
					charges = GrabStackedCharges(inst->GetID());
				}

				Message(Chat::White, "Slot %d: %s(%d) price: %d", i, inst->GetItem()->Name, charges, cost);
				++count;
			}
		}
	}

	Message(Chat::White, "%s has %d items up for sale", client->GetName(), count);
	safe_delete(TraderItems);
}

EQ::ItemInstance* Client::FindTraderItemByIDAndSlot(int32 ItemID, int16 slotid){

	EQ::ItemInstance* item = nullptr;
	uint16 SlotID = 0;
	for(int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899) //Traders Satchel
		{ 
			for(int x = EQ::invbag::SLOT_BEGIN; x <= EQ::invbag::SLOT_END; x++)
			{
				SlotID = EQ::InventoryProfile::CalcSlotId(i, x);
				item = this->GetInv().GetItem(SlotID);
				if(item && item->GetItem()->ID == ItemID && SlotID == slotid)
					return item;
			}
		}
	}
	Log(Logs::Detail, Logs::Bazaar, "Client::FindTraderItemByIDAndSlot Couldn't find item! Item No. was %i Slot was %d", ItemID, slotid);

	return nullptr;
}

GetItems_Struct* Client::GetTraderItems(bool skip_stackable)
{

	const EQ::ItemInstance* item = nullptr;
	uint16 SlotID = 0;
	auto gis = new GetItems_Struct;
	memset(gis,0,sizeof(GetItems_Struct));
	uint8 ndx = 0;

	for(int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++) {
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899){ //Traders Satchel
			for(int x = EQ::invbag::SLOT_BEGIN; x <= EQ::invbag::SLOT_END; x++) {
				SlotID = EQ::InventoryProfile::CalcSlotId(i, x);

				item = this->GetInv().GetItem(SlotID);

				if(item)
				{
					if (!skip_stackable || (skip_stackable && database.ItemQuantityType(item->GetItem()->ID) != EQ::item::Quantity_Stacked))
					{
						gis->Items[ndx] = item->GetItem()->ID;
						gis->SlotID[ndx] = SlotID;
						gis->Charges[ndx] = item->GetCharges();
						++ndx;
					}
					else
					{
						bool newitem = true;
						for (int y = 0; y < 80; ++y)
						{
							if (gis->Items[y] > 0 && gis->Items[y] == item->GetItem()->ID)
							{
								newitem = false;
								break;
							}
						}

						if (newitem)
						{
							gis->Items[ndx] = item->GetItem()->ID;
							gis->SlotID[ndx] = SlotID;
							gis->Charges[ndx] = GrabStackedCharges(item->GetItem()->ID);
							++ndx;
						}
					}
				}
			}
		}
	}
	return gis;
}

GetItem_Struct Client::GrabItem(uint16 item_id)
{

	const EQ::ItemInstance* item = nullptr;
	uint16 SlotID = 0;
	GetItem_Struct gi;
	TraderCharges_Struct* TraderItems = database.LoadTraderItemWithCharges(this->CharacterID());

	gi.Items = 0;
	gi.SlotID = 0;
	gi.Charges = 0;

	for(int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++) 
	{
		bool founditem = false;
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899) //Traders Satchel
		{
			for(int x = EQ::invbag::SLOT_BEGIN; x <= EQ::invbag::SLOT_END; x++)
			{
				bool freeslot = true;
				SlotID = EQ::InventoryProfile::CalcSlotId(i, x);
				item = this->GetInv().GetItem(SlotID);

				if(!item)
					continue;

				if(item->GetID() != item_id)
					continue;

				for(int i = 0; i < 80; i++)
				{
					if(SlotID == TraderItems->SlotID[i])
					{
						freeslot = false;
						break;
					}
				}

				if(item && freeslot)
				{
					gi.Items = item->GetItem()->ID;
					gi.SlotID = SlotID;
					gi.Charges = item->GetCharges();
					Log(Logs::Detail, Logs::Bazaar, "GrabItem: Item %d in slot %d is unused and will be added to the trader list.", gi.Items, gi.SlotID);
					safe_delete(TraderItems);
					return gi;
				}
			}
		}
	}

	safe_delete(TraderItems);
	return gi;
}

GetItem_Struct Client::GrabStackedItem(uint16 item_id)
{

	const EQ::ItemInstance* bagitem = nullptr;
	const EQ::ItemInstance* item = nullptr;
	const EQ::ItemInstance* cur_item = nullptr;
	uint16 SlotID = 0;
	uint16 final_slot = 0;
	uint16 charges = 0;
	GetItem_Struct gi;
	TraderCharges_Struct* TraderItems = database.LoadTraderItemWithCharges(this->CharacterID());

	gi.Items = 0;
	gi.SlotID = 0;
	gi.Charges = 0;

	bool existing_item = false;
	for (int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		bagitem = this->GetInv().GetItem(i);
		if (bagitem && bagitem->GetItem()->ID == 17899) //Traders Satchel
		{
			// Determine if this item was already saved to the DB, or if it is a new entry.
			for (int x = EQ::invbag::SLOT_BEGIN; x <= EQ::invbag::SLOT_END; x++)
			{
				SlotID = EQ::InventoryProfile::CalcSlotId(i, x);
				item = this->GetInv().GetItem(SlotID);

				if (!item)
					continue;

				if (item->GetID() != item_id)
				{
					continue;
				}
				else
				{
					for (int s = 0; s < 80; s++)
					{
						if (item_id == TraderItems->ItemID[s])
						{
							final_slot = 0;
							existing_item = true;
							cur_item = nullptr;
							break;
						}
						else
						{
							final_slot = SlotID;
							cur_item = item;
						}
					}
				}

				if (existing_item)
				{
					break;
				}
			}

			if (existing_item)
			{
				break;
			}
		}
	}

	if (cur_item && final_slot > 0)
	{
		for (int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
		{
			bagitem = this->GetInv().GetItem(i);
			if (bagitem && bagitem->GetItem()->ID == 17899) //Traders Satchel
			{
				// Tally up total charges for this item found in the player's satchels
				for (int y = EQ::invbag::SLOT_BEGIN; y <= EQ::invbag::SLOT_END; y++)
				{
					SlotID = EQ::InventoryProfile::CalcSlotId(i, y);
					item = this->GetInv().GetItem(SlotID);

					if (!item)
						continue;

					if (item->GetID() != item_id)
					{
						continue;
					}
					else
					{
						charges += item->GetCharges();
					}
				}
			}
		}
	}

	if (cur_item && final_slot > 0)
	{
		gi.Items = cur_item->GetItem()->ID;
		gi.SlotID = final_slot;
		gi.Charges = charges;
		Log(Logs::Detail, Logs::Bazaar, "GrabStackedItem: Item %d in slot %d with %d charges is unused and will be added to the trader list.", gi.Items, gi.SlotID, gi.Charges);
	}

	safe_delete(TraderItems);
	return gi;
}

int16 Client::GrabStackedSlot(uint16 item_id, uint8 charges)
{
	const EQ::ItemInstance* bagitem = nullptr;
	const EQ::ItemInstance* item = nullptr;
	int16 SlotID = 0;
	int16 RetSlot = INVALID_INDEX;
	uint16 newcharges = 0;

	for (int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		bagitem = this->GetInv().GetItem(i);
		if (bagitem && bagitem->GetItem()->ID == 17899) //Traders Satchel
		{
			// Determine if this item was already saved to the DB, or if it is a new entry.
			for (int x = EQ::invbag::SLOT_BEGIN; x <= EQ::invbag::SLOT_END; x++)
			{
				SlotID = EQ::InventoryProfile::CalcSlotId(i, x);
				item = this->GetInv().GetItem(SlotID);

				if (!item)
					continue;

				if (item->GetID() != item_id)
				{
					continue;
				}
				else
				{
					if (item->GetCharges() >= charges)
					{
						return SlotID;
					}
					else if(item->GetCharges() > newcharges)
					{
						RetSlot = SlotID;
						newcharges = item->GetCharges();
					}
				}
			}
		}
	}

	return RetSlot;
}

uint16 Client::GrabStackedCharges(uint16 item_id)
{
	const EQ::ItemInstance* bagitem = nullptr;
	const EQ::ItemInstance* item = nullptr;
	int16 SlotID = 0;
	uint16 newcharges = 0;

	for (int i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		bagitem = this->GetInv().GetItem(i);
		if (bagitem && bagitem->GetItem()->ID == 17899) //Traders Satchel
		{
			// Determine if this item was already saved to the DB, or if it is a new entry.
			for (int x = EQ::invbag::SLOT_BEGIN; x <= EQ::invbag::SLOT_END; x++)
			{
				SlotID = EQ::InventoryProfile::CalcSlotId(i, x);
				item = this->GetInv().GetItem(SlotID);

				if (!item)
					continue;

				if (item->GetID() != item_id)
				{
					continue;
				}
				else
				{
					newcharges += item->GetCharges();
				}
			}
		}
	}

	return newcharges;
}

void Client::NukeTraderItem(uint16 Slot,int16 Charges,uint16 Quantity,Client* Customer,uint16 TraderSlot, uint32 SellerID) 
{

	if(!Customer) return;

	Log(Logs::Detail, Logs::Bazaar, "NukeTraderItem(Slot %i, Trader Quantity %i, Requested Quantity %i TraderSlot %i", Slot, Charges, Quantity, TraderSlot);

	bool stacked = false;
	if(Quantity < Charges) 
	{
		Customer->SendSingleTraderItem(this->CharacterID(), Slot, TraderSlot);
		stacked = true;
	}
	else
	{
		entity_list.NukeTraderItem(this, TraderSlot);
	}

	if (stacked)
	{
		if (Quantity == 20)
		{
			DeleteItemInInventory(Slot, 0, true);
		}
		else
		{
			for (int i = 0; i < Quantity; ++i)
			{
				DeleteItemInInventory(Slot, 1, true);
			}
		}
	}
	else
	{
		DeleteItemInInventory(Slot, 0, true);
	}

	entity_list.SendTraderInventory(this);
}

void Client::FindAndNukeTraderItem(int32 ItemID, uint16 Quantity, Client* Customer, uint16 SlotID, int8 TraderSlot){

	uint32 SellerID = this->GetID();
	const EQ::ItemInstance* item= nullptr;
	bool Stackable = false;
	int16 Charges=0;

	if(SlotID > 0)
	{

		item = this->GetInv().GetItem(SlotID);
		if (item)
		{
			if (database.ItemQuantityType(item->GetID()) == EQ::item::Quantity_Stacked)
			{
				Charges = GrabStackedCharges(item->GetID());
				Stackable = true;
			}
			else
			{
				Charges = this->GetInv().GetItem(SlotID)->GetCharges();
				Quantity = (Charges > 0) ? Charges : 1;
			}

			Log(Logs::Detail, Logs::Bazaar, "FindAndNuke %s in slot %d, Merchant Quantity %i, Requested Quantity %i", item->GetItem()->Name, SlotID, Charges, Quantity);
		}

		if(item && (Charges <= Quantity || (Charges <= 0 && Quantity==1) || !Stackable))
		{
			database.DeleteTraderItem(this->CharacterID(),TraderSlot);
			NukeTraderItem(SlotID, Charges, Quantity, Customer,TraderSlot,SellerID);
			return;
		}
		else if(item) 
		{
			database.UpdateTraderItemCharges(this->CharacterID(), TraderSlot, Charges-Quantity);
			NukeTraderItem(SlotID, Charges, Quantity, Customer,TraderSlot,SellerID);
			return;
		}
	}

	Log(Logs::Detail, Logs::Bazaar, "Could NOT find a match for Item: %i with a quantity of: %i on Trader: %s\n",ItemID,
					Quantity,this->GetName());
}

void Client::ReturnTraderReq(const EQApplicationPacket* app, int16 TraderItemCharges, int TraderSlot, uint32 Price){

	TraderBuy_Struct* tbs = (TraderBuy_Struct*)app->pBuffer;
	auto outapp = new EQApplicationPacket(OP_TraderBuy, sizeof(TraderBuy_Struct));
	TraderBuy_Struct* outtbs = (TraderBuy_Struct*)outapp->pBuffer;
	memcpy(outtbs, tbs, app->size);
	outtbs->Price = Price;
	outtbs->Quantity = TraderItemCharges;
	outtbs->TraderID = this->GetID();
	outtbs->Slot = TraderSlot;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::TradeRequestFailed(const EQApplicationPacket* app) {

	TraderBuy_Struct* tbs = (TraderBuy_Struct*)app->pBuffer;
	auto outapp = new EQApplicationPacket(OP_TraderBuy, sizeof(TraderBuy_Struct));
	TraderBuy_Struct* outtbs = (TraderBuy_Struct*)outapp->pBuffer;
	memcpy(outtbs, tbs, app->size);
	outtbs->Slot = 0xFFFF;
	outtbs->TraderID = 0xFFFF;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::BuyTraderItem(TraderBuy_Struct* tbs,Client* Trader,const EQApplicationPacket* app){

	if(!Trader) return;

	if(!Trader->IsTrader()) {
		TradeRequestFailed(app);
		return;
	}

	auto outapp = new EQApplicationPacket(OP_Trader,sizeof(TraderBuy_Struct));
	TraderBuy_Struct* outtbs = (TraderBuy_Struct*)outapp->pBuffer;
	outtbs->ItemID = tbs->ItemID;

	const EQ::ItemInstance* BuyItem;
	int16 SlotID = 0;
	if (database.ItemQuantityType(tbs->ItemID) == EQ::item::Quantity_Stacked)
	{
		SlotID = Trader->GrabStackedSlot(tbs->ItemID, tbs->Quantity);
		if(SlotID != INVALID_INDEX)
			Log(Logs::Detail, Logs::Bazaar, "Found stackable item %d in slot %d", tbs->ItemID, SlotID);
	}
	else
	{
		SlotID = database.GetTraderItemBySlot(Trader->CharacterID(), tbs->Slot);
	}

	if(SlotID == INVALID_INDEX)
	{
		Log(Logs::Detail, Logs::Bazaar, "Unable to find a valid item slot on trader.");
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	BuyItem = Trader->FindTraderItemByIDAndSlot(tbs->ItemID, SlotID);

	if(!BuyItem) {
		Log(Logs::Detail, Logs::Bazaar, "Unable to find item on trader.");
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	uint32 priceper = tbs->Price / tbs->Quantity;
	outtbs->Price = tbs->Price;
	Log(Logs::Detail, Logs::Bazaar, "%s Buyitem: Name: %s, IsStackable: %i, Requested Quantity: %i, Trader Quantity %i Price: %i TraderSlot: %i InvSlot: %d Price per item: %i",
					GetName(), BuyItem->GetItem()->Name, BuyItem->IsStackable(), tbs->Quantity, BuyItem->GetCharges(), tbs->Price, tbs->Slot, SlotID, priceper);
	// If the item is not stackable, then we can only be buying one of them.
	if(!BuyItem->IsStackable())
		outtbs->Quantity = tbs->Quantity;
	else {
		// Stackable items, arrows, diamonds, etc
		int ItemCharges = BuyItem->GetCharges();

		// ItemCharges for stackables should not be <= 0
		if(ItemCharges <= 0)
			outtbs->Quantity = 1;
		// If the purchaser requested more than is in the stack, just sell them how many are actually in the stack.
		else if(ItemCharges < (int16)tbs->Quantity)
		{
			outtbs->Price =  tbs->Price - ((tbs->Quantity - ItemCharges) * priceper);
			outtbs->Quantity = ItemCharges;
		}
		else
			outtbs->Quantity = tbs->Quantity;
	}

	Log(Logs::Detail, Logs::Bazaar, "Actual quantity that will be traded is %i for cost: %i", outtbs->Quantity, outtbs->Price);

	if(outtbs->Price <= 0) {
		Message(Chat::Red, "Internal error. Aborting trade. Please report this to the ServerOP. Error code is 1");
		Trader->Message(Chat::Red, "Internal error. Aborting trade. Please report this to the ServerOP. Error code is 1");
		Log(Logs::General, Logs::Error, "Bazaar: Zero price transaction between %s and %s aborted."
						"Item: %s, Charges: %i, TBS: Qty %i, Price: %i",
						GetName(), Trader->GetName(),
						BuyItem->GetItem()->Name, BuyItem->GetCharges(), outtbs->Quantity, outtbs->Price);
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	if(outtbs->Price > MAX_TRANSACTION_VALUE) {
		Message(Chat::Red, "That would exceed the single transaction limit of %u platinum.", MAX_TRANSACTION_VALUE / 1000);
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	// This cannot overflow assuming MAX_TRANSACTION_VALUE, checked above, is the default of 2000000000
	uint32 TotalCost = outtbs->Price;
	if (!this->TakeMoneyFromPP(TotalCost)) {
		RecordPlayerEventLog(PlayerEvent::POSSIBLE_HACK, PlayerEvent::PossibleHackEvent{ .message = "Attempted to buy something in bazaar but did not have enough money." });
		TradeRequestFailed(app);
		safe_delete(outapp);
		Kick();
		return;
	}

	int TraderSlot = tbs->Slot;
	ReturnTraderReq(app, outtbs->Quantity, TraderSlot, outtbs->Price);

	outtbs->TraderID = this->GetID();
	outtbs->Action = BazaarBuyItem;
	strn0cpy(outtbs->ItemName, BuyItem->GetItem()->Name, 64);

	if(BuyItem->IsStackable())
		SendTraderItem(BuyItem->GetItem()->ID, outtbs->Quantity);
	else
		SendTraderItem(BuyItem->GetItem()->ID, BuyItem->GetCharges());

	auto outapp2 = new EQApplicationPacket(OP_MoneyUpdate,sizeof(MoneyUpdate_Struct));
	MoneyUpdate_Struct* mus= (MoneyUpdate_Struct*)outapp2->pBuffer;

	mus->platinum = TotalCost / 1000;
	TotalCost -= (mus->platinum * 1000);
	mus->gold = TotalCost / 100;
	TotalCost -= (mus->gold * 100);
	mus->silver = TotalCost / 10;
	TotalCost -= (mus->silver * 10);
	mus->copper = TotalCost;

	bool updateclient = true;
	Trader->AddMoneyToPP(mus->copper,mus->silver,mus->gold,mus->platinum,updateclient);

	mus->platinum = Trader->GetPlatinum();
	mus->gold = Trader->GetGold();
	mus->silver = Trader->GetSilver();
	mus->copper = Trader->GetCopper();

	if(RuleB(QueryServ, BazaarAuditTrail))
		QServ->QSBazaarAudit(Trader->GetName(), GetName(), BuyItem->GetItem()->Name, BuyItem->GetID(), outtbs->Quantity, outtbs->Price);

	Trader->FindAndNukeTraderItem(tbs->ItemID, outtbs->Quantity, this, SlotID, tbs->Slot);
	Trader->Trader_CustomerBought(this,outtbs->Price,tbs->ItemID,outtbs->Quantity,BuyItem->GetItem()->Name, tbs->Slot);

	safe_delete(outapp);
	safe_delete(outapp2);

}

void Client::SendBazaarWelcome()
{
	const std::string query = "SELECT COUNT(DISTINCT char_id), count(char_id) FROM trader";
	auto results = database.QueryDatabase(query);
	if (results.Success() && results.RowCount() == 1){
		auto row = results.begin();

		auto outapp = new EQApplicationPacket(OP_BazaarSearch, sizeof(BazaarWelcome_Struct));
		memset(outapp->pBuffer,0,outapp->size);
		BazaarWelcome_Struct* bws = (BazaarWelcome_Struct*)outapp->pBuffer;
		bws->Beginning.Action = BazaarWelcome;
		bws->Traders = atoi(row[0]);
		bws->Items = atoi(row[1]);

		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::SendBazaarResults(uint32 TraderID, uint32 Class_, uint32 Race, uint32 ItemStat, uint32 Slot, uint32 Type,
					char Name[64], uint32 MinPrice, uint32 MaxPrice) {

	std::string searchValues = " COUNT(item_id), trader.char_id, trader.item_id, trader.item_cost, items.name, SUM(charges) ";
	std::string searchCriteria = " WHERE trader.item_id = items.id ";

	if(TraderID > 0) {
		Client* trader = entity_list.GetClientByID(TraderID);

		if(trader)
			searchCriteria.append(StringFormat(" AND trader.char_id = %i", trader->CharacterID()));
	}

	if(MinPrice != 0)
		searchCriteria.append(StringFormat(" AND trader.item_cost >= %i", MinPrice));

	if(MaxPrice != 0)
		searchCriteria.append(StringFormat(" AND trader.item_cost <= %i", MaxPrice));

	if(strlen(Name) > 0) {
		char *safeName = RemoveApostrophes(Name);
		searchCriteria.append(StringFormat(" AND items.name LIKE '%%%s%%'", safeName));
		safe_delete_array(safeName);
	}

	if(Class_ != 0xFFFF)
		searchCriteria.append(StringFormat(" AND MID(REVERSE(BIN(items.classes)), %i, 1) = 1", Class_));

	if(Race != 0xFFFF)
		searchCriteria.append(StringFormat(" AND MID(REVERSE(BIN(items.races)), %i, 1) = 1", Race));

	if(Slot != 0xFFFF)
		searchCriteria.append(StringFormat(" AND MID(REVERSE(BIN(items.slots)), %i, 1) = 1", Slot + 1));

	switch(Type){
        case 0xFFFF:
            break;
        case 0:
            // 1H Slashing
            searchCriteria.append(" AND items.itemtype = 0 AND damage > 0");
            break;
        case 31:
            searchCriteria.append(" AND items.itemclass = 2");
            break;
        case 46: // All Effects
            searchCriteria.append(" AND ((items.clickeffect > 0 AND items.clickeffect < 4700) OR (items.worneffect > 0 AND items.worneffect < 4700) OR (items.proceffect > 0 AND items.proceffect < 4700) OR (items.focuseffect > 0 AND items.focuseffect < 4700))");
            break;
        case 47: // Haste
            searchCriteria.append(" AND items.worneffect = 998");
            break;
        case 48: // Flowing Thought
            searchCriteria.append(" AND items.worneffect >= 1298 AND items.worneffect <= 1307");
            break;
        case 49: // Focus Effect
            searchCriteria.append(" AND items.focuseffect > 0");
            break;

        default:
            searchCriteria.append(StringFormat(" AND items.itemtype = %i", Type));
    }

	switch(ItemStat) {

		case STAT_AC:
			searchCriteria.append(" AND items.ac > 0");
			searchValues.append(", items.ac");
			break;

		case STAT_AGI:
			searchCriteria.append(" AND items.aagi > 0");
			searchValues.append(", items.aagi");
			break;

		case STAT_CHA:
			searchCriteria.append(" AND items.acha > 0");
			searchValues.append(", items.acha");
			break;

		case STAT_DEX:
			searchCriteria.append(" AND items.adex > 0");
			searchValues.append(", items.adex");
			break;

		case STAT_INT:
			searchCriteria.append(" AND items.aint > 0");
			searchValues.append(", items.aint");
			break;

		case STAT_STA:
			searchCriteria.append(" AND items.asta > 0");
			searchValues.append(", items.asta");
			break;

		case STAT_STR:
			searchCriteria.append(" AND items.astr > 0");
			searchValues.append(", items.astr");
			break;

		case STAT_WIS:
			searchCriteria.append(" AND items.awis > 0");
			searchValues.append(", items.awis");
			break;

		case STAT_COLD:
			searchCriteria.append(" AND items.cr > 0");
			searchValues.append(", items.cr");
			break;

		case STAT_DISEASE:
			searchCriteria.append(" AND items.dr > 0");
			searchValues.append(", items.dr");
			break;

		case STAT_FIRE:
			searchCriteria.append(" AND items.fr > 0");
			searchValues.append(", items.fr");
			break;

		case STAT_MAGIC:
            searchCriteria.append(" AND items.mr > 0");
			searchValues.append(", items.mr");
			break;

		case STAT_POISON:
			searchCriteria.append(" AND items.pr > 0");
			searchValues.append(", items.pr");
			break;

		case STAT_HP:
			searchCriteria.append(" AND items.hp > 0");
			searchValues.append(", items.hp");
			break;

		case STAT_MANA:
			searchCriteria.append(" AND items.mana > 0");
			searchValues.append(", items.mana");
			break;

		case STAT_ENDURANCE:
			searchCriteria.append(" AND items.endur > 0");
			searchValues.append(", items.endur");
			break;

		case STAT_ATTACK:
			searchCriteria.append(" AND items.attack > 0");
			searchValues.append(", items.attack");
			break;

		case STAT_HP_REGEN:
			searchCriteria.append(" AND items.regen > 0");
			searchValues.append(", items.regen");
			break;

		case STAT_MANA_REGEN:
			searchCriteria.append(" AND items.manaregen > 0");
			searchValues.append(", items.manaregen");
			break;

		case STAT_HASTE:
			searchCriteria.append(" AND items.worneffect = 998");
			searchValues.append(", items.worneffect");
			break;

		case STAT_DAMAGE_SHIELD:
			searchCriteria.append(" AND items.damageshield > 0");
			searchValues.append(", items.damageshield");
			break;

		default:
			searchValues.append(", 0");
			break;
	}

    std::string query = StringFormat("SELECT %s FROM trader, items %s GROUP BY items.id, char_id LIMIT %i",
                                    searchValues.c_str(), searchCriteria.c_str(), RuleI(Bazaar, MaxSearchResults));
    auto results = database.QueryDatabase(query);

	Log(Logs::Detail, Logs:: Bazaar, "Query was: %s", query.c_str());

    if (!results.Success()) {
		return;
	}

    int Size = 0;
    uint32 ID = 0;

    if (results.RowCount() == static_cast<unsigned long>(RuleI(Bazaar, MaxSearchResults)))
			Message(Chat::Yellow, "Your search reached the limit of %i results. Please narrow your search down by selecting more options.",
					RuleI(Bazaar, MaxSearchResults));

    if(results.RowCount() == 0) {
		auto outapp2 = new EQApplicationPacket(OP_BazaarSearch, sizeof(BazaarReturnDone_Struct));
		BazaarReturnDone_Struct* brds = (BazaarReturnDone_Struct*)outapp2->pBuffer;
		brds->Type = BazaarSearchDone;
		brds->Unknown002 = 0;
		brds->Unknown004 = 0xFFFFFFFF;
		brds->Unknown008 = 0xFFFFFFFF;
		brds->Unknown012 = 0xFFFF;
		this->QueuePacket(outapp2);
		safe_delete(outapp2);
		return;
	}

	Size = sizeof(BazaarSearchResults_Struct);
	auto buffer = new uchar[Size];

	BazaarSearchResults_Struct* bsrs = (BazaarSearchResults_Struct*)buffer;

	for (auto row = results.begin(); row != results.end(); ++row) {
		memset(buffer, 0, Size);

		bsrs->Action = BazaarSearchResults;
		int16 itemid = atoi(row[2]);
		bsrs->NumItems = atoi(row[0]); 
		Client* Trader2=entity_list.GetClientByCharID(atoi(row[1]));
		if(Trader2)
		{
			bsrs->SellerID = Trader2->GetID();
		}
		else
		{
			Log(Logs::Detail, Logs::Bazaar, "Unable to find trader: %i\n",atoi(row[1]));
		}

		bsrs->ItemID = itemid;
		bsrs->Cost = atoul(row[3]);

		//Yes, AK appended the item quantity to the end of the name string :I
		uint16 number = database.ItemQuantityType(itemid) == EQ::item::Quantity_Stacked ? atoi(row[5]) : atoi(row[0]);
		std::string itemname = row[4];
		itemname += ' '; itemname += '('; itemname += std::to_string(number); itemname += ')';
		memcpy(bsrs->ItemName, itemname.c_str(), itemname.length());

		Log(Logs::Detail, Logs:: Bazaar, "Adding item: %s (%d) with count: %d cost: %d to results.", bsrs->ItemName, bsrs->ItemID, bsrs->NumItems, bsrs->Cost);
		auto outapp = new EQApplicationPacket(OP_BazaarSearch, Size);
		memcpy(outapp->pBuffer, buffer, Size);

		this->QueuePacket(outapp);
		safe_delete(outapp);
	}
	safe_delete_array(buffer);

	auto outapp2 = new EQApplicationPacket(OP_BazaarSearch, sizeof(BazaarReturnDone_Struct));
	BazaarReturnDone_Struct* brds = (BazaarReturnDone_Struct*)outapp2->pBuffer;

	brds->Type = BazaarSearchDone;
	brds->Unknown002 = 0;
	brds->Unknown004 = 0xFFFFFFFF;
	brds->Unknown008 = 0xFFFFFFFF;
	brds->Unknown012 = 0xFFFF;

	this->QueuePacket(outapp2);
	safe_delete(outapp2);
}

void Client::UpdateTraderCustomerItemsAdded(TraderCharges_Struct* gis, uint32 ItemID) {

	// Send Item packets to the customer to update the Merchant window with the
	// new items for sale, and give them a message in their chat window.

	const EQ::ItemData *item = database.GetItem(ItemID);

	if(!item) return;

	EQ::ItemInstance* inst = database.CreateItem(item);

	if(!inst) return;

	entity_list.SendTraderUpdateMessage(this, item, 1);

	for(int i = 0; i < 80; i++) {

		if(gis->ItemID[i] == ItemID) {

			inst->SetCharges(gis->Charges[i]);
			inst->SetPrice(gis->ItemCost[i]);
			inst->SetMerchantSlot(gis->SlotID[i]);

			if(inst->IsStackable())
				inst->SetMerchantCount(gis->Charges[i]);

			Log(Logs::Detail, Logs::Bazaar, "Sending price update for %s, SlotID. %i with %i charges",
							item->Name, gis->SlotID[i], gis->Charges[i]);
		}
	}

	entity_list.SendTraderInventory(this);
	safe_delete(inst);
}

void Client::UpdateTraderCustomerPriceChanged(TraderCharges_Struct* gis, uint32 ItemID, int32 Charges, uint32 NewPrice) {

	// Send ItemPackets to update the customer's Merchant window with the new price (or remove the item if
	// the new price is 0) and inform them with a chat message.

	const EQ::ItemData *item = database.GetItem(ItemID);

	if(!item) return;

	if(NewPrice == 0) 
	{
		// If the new price is 0, remove the item(s) from the window.
		entity_list.NukeTraderItemByID(this, gis, ItemID);
		entity_list.SendTraderUpdateMessage(this, item, 2);
		return;
	}

	EQ::ItemInstance* inst = database.CreateItem(item);

	if(!inst) return;

	if(Charges > 0)
		inst->SetCharges(Charges);

	inst->SetPrice(NewPrice);

	if(inst->IsStackable())
		inst->SetMerchantCount(Charges);

	// Let the customer know the price in the window has suddenly just changed on them.
	entity_list.SendTraderUpdateMessage(this, item, 0);

	for(int i = 0; i < 80; i++) {
		if((gis->ItemID[i] != ItemID) ||
			((!item->Stackable) && (gis->Charges[i] != Charges)))
			continue;

		inst->SetMerchantSlot(gis->SlotID[i]);

		Log(Logs::Detail, Logs::Bazaar, "Sending price update for %s, SlotID %i with %i charges",
						item->Name, gis->SlotID[i], gis->Charges[i]);
	}

	entity_list.SendTraderInventory(this);
	safe_delete(inst);
}

void Client::HandleTraderPriceUpdate(const EQApplicationPacket *app) 
{

	// Handle price updates from the Trader and update a customer browsing our stuff if necessary
	// This method also handles removing items from sale and adding them back up whilst still in
	// Trader mode.
	TraderPriceUpdate_Struct* tpus = (TraderPriceUpdate_Struct*)app->pBuffer;

	const EQ::ItemData* item = database.GetItem(tpus->ItemID);
	if(item && item->NoDrop == 0)
	{
		Log(Logs::Detail, Logs::Bazaar, "Received Price Update for %s which is a no drop item. Ignoring.", item->Name);
		return;
	}

	auto outapp = new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* tsis=(Trader_ShowItems_Struct*)outapp->pBuffer;

	tsis->TraderID = this->GetID();
	tsis->Code = tpus->Action;

	Log(Logs::Detail, Logs::Bazaar, "Received Price Update for %s, Item No. %i, New Price %i",
					GetName(), tpus->ItemID, tpus->NewPrice);

	// Pull the items this Trader currently has for sale from the trader table.
	TraderCharges_Struct* gis = database.LoadTraderItemWithCharges(CharacterID());

	if(!gis) {
		Log(Logs::Detail, Logs::Bazaar, "[CLIENT] Error retrieving Trader items details to update price.");
		return;
	}

	// The client only sends a single update with the ItemID of the item whose price has been updated.
	// We must update the price for all the Trader's items that are identical to that one item, i.e.
	// if it is a stackable item like arrows, update the price for all stacks. If it is not stackable, then
	// update the prices for all items that have the same number of charges.
	uint32 IDOfItemToUpdate = 0;
	int32 ChargesOnItemToUpdate = 0;
	uint32 OldPrice = 0;
	uint32 SellerID = this->GetID();

	for(int i = 0; i < 80; i++) 
	{
		if((gis->ItemID[i] > 0) && (gis->ItemID[i] == tpus->ItemID)) 
		{
			// We found the item that the Trader wants to change the price of (or add back up for sale).
			Log(Logs::Detail, Logs::Bazaar, "ItemID is %i, Charges is %i", gis->ItemID[i], gis->Charges[i]);

			IDOfItemToUpdate = gis->ItemID[i];
			ChargesOnItemToUpdate = gis->Charges[i];
			OldPrice = gis->ItemCost[i];

			break;
		}
	}

	if(IDOfItemToUpdate == 0) 
	{
		// If the item is not currently in the trader table for this Trader, then they must have removed it from sale while
		// still in Trader mode. Check if the item is in their Trader Satchels, and if so, put it back up.

		// Quick Sanity check. If the item is not currently up for sale, and the new price is zero, just ack the packet
		// and do nothing.
		if(tpus->NewPrice == 0) {
			tsis->SubAction = BazaarPriceChange_RemoveItem;
			QueuePacket(outapp);
			safe_delete(outapp);
			safe_delete(gis);
			return ;
		}

		// Find what is in their Trader Satchels
		GetItems_Struct* newgis=GetTraderItems(true);

		uint32 IDOfItemToAdd = 0;
		int32 ChargesOnItemToAdd = 0;

		for(int i = 0; i < 80; i++) {

			if((newgis->Items[i] > 0) && (newgis->Items[i] == tpus->ItemID)) {

				Log(Logs::Detail, Logs::Bazaar, "Found new Item to Add, ItemID is %i, Charges is %i", newgis->Items[i],
								newgis->Charges[i]);

				IDOfItemToAdd = newgis->Items[i];
				ChargesOnItemToAdd = newgis->Charges[i];

				break;
			}
		}


		const EQ::ItemData *item = 0;

		if(IDOfItemToAdd)
			item = database.GetItem(IDOfItemToAdd);

		if(!IDOfItemToAdd || !item) {

			Log(Logs::Detail, Logs::Bazaar, "Item not found in Trader Satchels either.");
			tsis->SubAction = BazaarPriceChange_Fail;
			QueuePacket(outapp);
			safe_delete(outapp);
			Trader_EndTrader();
			safe_delete(gis);
			safe_delete(newgis);
			return;
		}

		// It is a limitation of the client that if you have multiple of the same item, but with different charges,
		// although you can set different prices for them before entering Trader mode. If you Remove them and then
		// add them back whilst still in Trader mode, they all go up for the same price. We check for this situation
		// and give the Trader a warning message.
		if(!item->Stackable) {

			bool SameItemWithDifferingCharges = false;

			for(int i = 0; i < 80; i++) {
				if((newgis->Items[i] == IDOfItemToAdd) && (newgis->Charges[i] != ChargesOnItemToAdd)) {

					SameItemWithDifferingCharges = true;
					break;
				}
			}

			if(SameItemWithDifferingCharges)
				Message(Chat::Red, "Warning: You have more than one %s with different charges. They have all been added for sale "
						"at the same price.", item->Name);
		}

		// Now put all Items with a matching ItemID up for trade.
		for(int i = 0; i < 80; ++i) 
		{
			if(newgis->Items[i] == IDOfItemToAdd) 
			{
				int16 SlotID = database.GetTraderItemBySlot(this->CharacterID(), i);
				if(SlotID == INVALID_INDEX)
				{
					Log(Logs::Detail, Logs::Bazaar, "There is no item in slot %d, so item %d will be inserted.", i, IDOfItemToAdd);
				}
				else
				{
					for (int x = 79; x >= i; --x)
					{
						int16 checkslot = database.GetTraderItemBySlot(this->CharacterID(), x);
						if(checkslot != INVALID_INDEX)
						{
							if(x == 79)
							{
								//This shouldn't happen, but check to be sure.
								database.DeleteTraderItem(this->CharacterID(), 79);
								Log(Logs::Detail, Logs::Bazaar, "Removing the item in trader slot 79 to make room for a new item!");
							}
							else
							{
								database.IncreaseTraderSlot(this->CharacterID(), x);
								Log(Logs::Detail, Logs::Bazaar, "Increasing the item in slot %d to slot %d to make room for item %d", x, x+1, IDOfItemToAdd);
							}
						}
					}
				}

				database.SaveTraderItem(CharacterID(), newgis->Items[i], newgis->SlotID[i], newgis->Charges[i], tpus->NewPrice, i);
				Log(Logs::Detail, Logs::Bazaar, "Adding new item for %s. ItemID %i, iSlotID %i, Charges %i, Price: %i, Slot %i",
							GetName(), newgis->Items[i], newgis->SlotID[i], newgis->Charges[i], tpus->NewPrice, i);
			}
		}

		// If we have a customer currently browsing, update them with the new items.
		if(WithCustomer)
		{
			UpdateTraderCustomerItemsAdded(gis, IDOfItemToAdd);
		}

		safe_delete(gis);
		safe_delete(newgis);

		// Acknowledge to the client.
		tsis->SubAction = BazaarPriceChange_AddItem;
		QueuePacket(outapp);
		safe_delete(outapp);

		return;
	}

	// This is a safeguard against a Trader increasing the price of an item while a customer is browsing and
	// unwittingly buying it at a higher price than they were expecting to.
	if(!RuleB(AlKabor, AllowPriceIncWhileBrowsing))
	{
		if((OldPrice != 0) && (tpus->NewPrice > OldPrice) && WithCustomer) 
		{
			tsis->SubAction = BazaarPriceChange_Fail;
			QueuePacket(outapp);
			safe_delete(outapp);
			Trader_EndTrader();
			Message(Chat::Red, "You must remove the item from sale before you can increase the price while a customer is browsing.");
			Message(Chat::Red, "Click 'Begin Trader' to restart Trader mode with the increased price for this item.");
			safe_delete(gis);
			return;
		}
	}

	// Send Acknowledgement back to the client.
	if(OldPrice == 0)
		tsis->SubAction = BazaarPriceChange_AddItem;
	else if(tpus->NewPrice != 0)
		tsis->SubAction = BazaarPriceChange_UpdatePrice;
	else
		tsis->SubAction = BazaarPriceChange_RemoveItem;

	QueuePacket(outapp);
	safe_delete(outapp);

	if(OldPrice == tpus->NewPrice) {
		Log(Logs::Detail, Logs::Bazaar, "The new price is the same as the old one.");
		safe_delete(gis);
		return;
	}
	// Update the price for all items we have for sale that have this ItemID and number of charges, or remove
	// them from the trader table if the new price is zero.
	//
	database.UpdateTraderItemPrice(CharacterID(), IDOfItemToUpdate, ChargesOnItemToUpdate, tpus->NewPrice);

	// If a customer is browsing our goods, send them the updated prices / remove the items from the Merchant window
	if(WithCustomer)
	{
		UpdateTraderCustomerPriceChanged(gis, IDOfItemToUpdate, ChargesOnItemToUpdate, tpus->NewPrice);
	}

	safe_delete(gis);

}
