/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

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
/*
New class for handling corpses and everything associated with them.
Child of the Mob class.
-Quagmire
*/

#ifdef _WINDOWS
	#if (!defined(_MSC_VER) || (defined(_MSC_VER) && _MSC_VER < 1900))
		#define snprintf	_snprintf
		#define vsnprintf	_vsnprintf
	#endif
    #define strncasecmp	_strnicmp
    #define strcasecmp	_stricmp
#endif

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "../common/strings.h"

#include "client.h"
#include "corpse.h"
#include "entity.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"
#include "queryserv.h"
#include <iostream>

extern EntityList entity_list;
extern Zone* zone;
extern WorldServer worldserver;
extern QueryServ* QServ;

void Corpse::SendEndLootErrorPacket(Client* client) {
	if (client)
	{
		auto outapp = new EQApplicationPacket(OP_LootComplete, 0);
		client->QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Corpse::SendLootReqErrorPacket(Client* client, uint8 response) {
	auto outapp = new EQApplicationPacket(OP_MoneyOnCorpse, sizeof(moneyOnCorpseStruct));
	moneyOnCorpseStruct* d = (moneyOnCorpseStruct*)outapp->pBuffer;
	d->response = response;
	d->unknown1 = 0x5a;
	d->unknown2 = 0x40;
	client->QueuePacket(outapp);
	safe_delete(outapp);
}

Corpse* Corpse::LoadCharacterCorpseEntity(uint32 in_dbid, uint32 in_charid, std::string in_charname, const glm::vec4& position, uint32 timestamp, bool rezzed, bool was_at_graveyard){
	uint32 item_count = database.GetCharacterCorpseItemCount(in_dbid);
	auto buffer = new char[sizeof(CharacterCorpseEntry) + (item_count * sizeof(LootItem))];
	CharacterCorpseEntry *ce = (CharacterCorpseEntry*)buffer;
	database.LoadCharacterCorpseData(in_dbid, ce);

	LootItems itemlist;
	LootItem* tmp = nullptr;

	for (unsigned int i = 0; i < ce->itemcount; i++) {
		tmp = new LootItem;
		memcpy(tmp, &ce->items[i], sizeof(LootItem));

		itemlist.push_back(tmp);
	}

	/* Create Corpse Entity */
	auto pc = new Corpse(
		in_dbid,			   // uint32 in_dbid
		in_charid,			   // uint32 in_charid
		in_charname.c_str(),   // char* in_charname
		&itemlist,			   // ItemList* in_itemlist
		ce->copper,		   // uint32 in_copper
		ce->silver,		   // uint32 in_silver
		ce->gold,			   // uint32 in_gold
		ce->plat,			   // uint32 in_plat
		position,
		ce->size,			   // float in_size
		ce->gender,		   // uint8 in_gender
		ce->race,			   // uint16 in_race
		ce->class_,		   // uint8 in_class
		ce->deity,			   // uint8 in_deity
		ce->level,			   // uint8 in_level
		ce->texture,		   // uint8 in_texture
		ce->helmtexture,	   // uint8 in_helmtexture
		ce->exp,			   // uint32 in_rezexp
		ce->gmexp,			   // uint32 in_gmrezexp
		ce->killedby,		   // uint8 in_killedby
		ce->rezzable,		   // bool rezzable
		ce->rez_time,		   // uint32 rez_time
		was_at_graveyard	   // bool wasAtGraveyard
	);

	if (ce->locked){
		pc->Lock();
	}

	/* Load Item Tints */
	pc->item_tint.Head.Color = ce->item_tint.Head.Color;
	pc->item_tint.Chest.Color = ce->item_tint.Chest.Color;
	pc->item_tint.Arms.Color = ce->item_tint.Arms.Color;
	pc->item_tint.Wrist.Color = ce->item_tint.Wrist.Color;
	pc->item_tint.Hands.Color = ce->item_tint.Hands.Color;
	pc->item_tint.Legs.Color = ce->item_tint.Legs.Color;
	pc->item_tint.Feet.Color = ce->item_tint.Feet.Color;
	pc->item_tint.Primary.Color = ce->item_tint.Primary.Color;
	pc->item_tint.Secondary.Color = ce->item_tint.Secondary.Color;

	/* Load Physical Appearance */
	pc->haircolor = ce->haircolor;
	pc->beardcolor = ce->beardcolor;
	pc->eyecolor1 = ce->eyecolor1;
	pc->eyecolor2 = ce->eyecolor2;
	pc->hairstyle = ce->hairstyle;
	pc->luclinface = ce->face;
	pc->beard = ce->beard;
	pc->IsRezzed(rezzed);
	pc->become_npc = false;
	pc->time_of_death = timestamp;

	pc->UpdateEquipmentLight(); // itemlist populated above..need to determine actual values

	safe_delete_array(ce);

	return pc;
}

Corpse::Corpse(NPC* in_npc, LootItems* in_itemlist, uint32 in_npctypeid, uint32 in_decaytime, bool is_client_pet)
	: Mob("Unnamed_Corpse",		// const char* in_name,
	"",							// const char* in_lastname,
	0,							// int32		in_cur_hp,
	0,							// int32		in_max_hp,
	in_npc->GetGender(),		// uint8		in_gender,
	in_npc->GetRace(),			// uint16		in_race,
	in_npc->GetClass(),			// uint8		in_class,
	BT_Humanoid,				// bodyType	in_bodytype,
	in_npc->GetDeity(),			// uint8		in_deity,
	in_npc->GetLevel(),			// uint8		in_level,
	in_npc->GetNPCTypeID(),		// uint32		in_npctype_id,
	in_npc->GetSize(),			// float		in_size,
	0,							// float		in_runspeed,
	in_npc->GetPosition(),		// float		in_position
	in_npc->GetInnateLightType(),	// uint8		in_light,
	in_npc->GetTexture(),		// uint8		in_texture,
	in_npc->GetHelmTexture(),	// uint8		in_helmtexture,
	0,							// uint16		in_ac,
	0,							// uint16		in_atk,
	0,							// uint16		in_str,
	0,							// uint16		in_sta,
	0,							// uint16		in_dex,
	0,							// uint16		in_agi,
	0,							// uint16		in_int,
	0,							// uint16		in_wis,
	0,							// uint16		in_cha,
	0,							// uint8		in_haircolor,
	0,							// uint8		in_beardcolor,
	0,							// uint8		in_eyecolor1, // the eyecolors always seem to be the same, maybe left and right eye?
	0,							// uint8		in_eyecolor2,
	0,							// uint8		in_hairstyle,
	0,							// uint8		in_luclinface,
	0,							// uint8		in_beard,
	EQ::TintProfile(),			// uint32		in_armor_tint[EQ::textures::TextureCount], 
	0xff,						// uint8		in_aa_title,
	0,							// uint8		in_see_invis, // see through invis/ivu
	0,							// uint8		in_see_invis_undead,
	0,							// uint8		in_see_sneak,
	0,							// uint8		in_see_improved_hide,
	0,							// int32		in_hp_regen,
	0,							// int32		in_mana_regen,
	0,							// uint8		in_qglobal,
	0,							// uint8		in_maxlevel,
	0,							// uint32		in_scalerate
	0,							// uint8		in_armtexture,
	0,							// uint8		in_bracertexture,
	0,							// uint8		in_handtexture,
	0,							// uint8		in_legtexture,
	0,							// uint8		in_feettexture,
	0							// uint8		in_chesttexture,
),									  
	corpse_decay_timer(in_decaytime),
	corpse_rez_timer(0),
	corpse_delay_timer(RuleI(NPC, CorpseUnlockTimer)),
	corpse_graveyard_timer(0),
	loot_cooldown_timer(10)
{
	corpse_graveyard_timer.Disable();
	corpse_graveyard_moved_timer.Disable();

	is_corpse_changed = false;
	is_player_corpse = false;
	rezzable = false;
	is_owner_online = false;
	is_locked = false;
	flymode = GravityBehavior::Ground;
	being_looted_by = 0xFFFFFFFF;

	if (in_npc && in_npc->loot_lockout_timer != 0)
		loot_lockout_timer = in_npc->loot_lockout_timer;
	else
		loot_lockout_timer = 0;

	if (zone->GetGuildID() == GUILD_NONE)
		loot_lockout_timer = 0;

	if (in_itemlist) {
		itemlist = *in_itemlist;
		in_itemlist->clear();
	}

	SetCash(in_npc->GetCopper(), in_npc->GetSilver(), in_npc->GetGold(), in_npc->GetPlatinum());

	npctype_id = in_npctypeid;
	SetPlayerKillItemID(0);
	char_id = 0;
	corpse_db_id = 0;
	player_corpse_depop = false;
	strcpy(corpse_name, in_npc->GetName());
	strcpy(name, in_npc->GetName());

	if (IsEmpty() && in_decaytime > RuleI(NPC, EmptyNPCCorpseDecayTimeMS))
	{
		corpse_decay_timer.Start(RuleI(NPC, EmptyNPCCorpseDecayTimeMS) + 1000);
	}
	
	if(in_npc->HasPrivateCorpse()) {
		corpse_delay_timer.Start(GetDecayTime() + 1000);
	}

	if(RuleB(Quarm, CorpseUnlockIsHalvedDecayTime))
		corpse_delay_timer.Start(GetDecayTime() / 2);

	initial_allowed_looters.clear();
	allowed_looters.clear();
	if (is_client_pet)
	{
		allowed_looters.emplace("000");		// corpses without looters are apparently lootable by anybody, so doing this to make it unlootable
		initial_allowed_looters.emplace("000");
		corpse_decay_timer.Start(3000);
	}

	npc_clean_name = in_npc->GetCleanName();

	this->rez_experience = 0;
	this->gm_rez_experience = 0;
	UpdateEquipmentLight();
	UpdateActiveLight();
}

Corpse::Corpse(Client* client, int32 in_rezexp, uint8 in_killedby) : Mob (
	"Unnamed_Corpse",				  // const char*	in_name,
	"",								  // const char*	in_lastname,
	0,								  // int32		in_cur_hp,
	0,								  // int32		in_max_hp,
	client->GetGender(),			  // uint8		in_gender,
	client->GetRace(),				  // uint16		in_race,
	client->GetClass(),				  // uint8		in_class,
	BT_Humanoid,					  // bodyType	in_bodytype,
	client->GetDeity(),				  // uint8		in_deity,
	client->GetLevel(),				  // uint8		in_level,
	0,								  // uint32		in_npctype_id,
	client->GetSize(),				  // float		in_size,
	0,								  // float		in_runspeed,
	client->GetPosition(),
	client->GetInnateLightType(),	  // uint8		in_light, - verified for client innate_light value
	client->GetTexture(),			  // uint8		in_texture,
	client->GetHelmTexture(),		  // uint8		in_helmtexture,
	0,								  // uint16		in_ac,
	0,								  // uint16		in_atk,
	0,								  // uint16		in_str,
	0,								  // uint16		in_sta,
	0,								  // uint16		in_dex,
	0,								  // uint16		in_agi,
	0,								  // uint16		in_int,
	0,								  // uint16		in_wis,
	0,								  // uint16		in_cha,
	client->GetPP().haircolor,		  // uint8		in_haircolor,
	client->GetPP().beardcolor,		  // uint8		in_beardcolor,
	client->GetPP().eyecolor1,		  // uint8		in_eyecolor1, // the eye colors always seem to be the same, maybe left and right eye?
	client->GetPP().eyecolor2,		  // uint8		in_eyecolor2,
	client->GetPP().hairstyle,		  // uint8		in_hairstyle,
	client->GetPP().face,			  // uint8		in_luclinface,
	client->GetPP().beard,			  // uint8		in_beard,
	EQ::TintProfile(),					// uint32		in_armor_tint[EQ::textures::TextureCount],
	0xff,							  // uint8		in_aa_title,
	0,								  // uint8		in_see_invis, // see through invis
	0,								  // uint8		in_see_invis_undead, // see through invis vs. un dead
	0,								  // uint8		in_see_sneak,
	0,								  // uint8		in_see_improved_hide,
	0,								  // int32		in_hp_regen,
	0,								  // int32		in_mana_regen,
	0,								  // uint8		in_qglobal,
	0,								  // uint8		in_maxlevel,
	0,								  // uint32		in_scalerate
	0,								  // uint8		in_armtexture,
	0,								  // uint8		in_bracertexture,
	0,								  // uint8		in_handtexture,
	0,								  // uint8		in_legtexture,
	0,								  // uint8		in_feettexture,
	0								  // uint8		in_chesttexture,
	),
	corpse_decay_timer(RuleI(Character, EmptyCorpseDecayTimeMS)),
	corpse_rez_timer(RuleI(Character, CorpseResTimeMS)),
	corpse_delay_timer(RuleI(NPC, CorpseUnlockTimer)),
	corpse_graveyard_timer(zone->graveyard_timer() * 60000),
	loot_cooldown_timer(10)
{
	int i;

	PlayerProfile_Struct *pp = &client->GetPP();
	EQ::ItemInstance *item = nullptr;

	/* Check if Zone has Graveyard First */
	if(!zone->HasGraveyard() || zone->GetGuildID() == GUILD_NONE && zone->GetZoneID() < 1000) {
		corpse_graveyard_timer.Disable();
	}
	corpse_graveyard_moved_timer.Disable();

	initial_allowed_looters.clear();
	allowed_looters.clear();

	is_corpse_changed		= true;
	rez_experience			= in_rezexp;
	gm_rez_experience		= in_rezexp;
	is_player_corpse	= true;
	is_locked			= false;
	being_looted_by	= 0xFFFFFFFF;
	loot_lockout_timer = 0;

	char_id			= client->CharacterID();
	corpse_db_id	= 0;
	player_corpse_depop			= false;
	copper			= 0;
	silver			= 0;
	gold			= 0;
	platinum		= 0;
	killedby		= in_killedby;
	rezzable		= true;
	rez_time		= 0;
	is_owner_online = false; // We can't assume they are online just because they just died. Perhaps they rage smashed their router.
	flymode			= GravityBehavior::Ground;
	time_of_death	= static_cast<uint32>(time(nullptr));

	owner_online_timer.Start(RuleI(Character, CorpseOwnerOnlineTimeMS));

	corpse_rez_timer.Disable();
	SetRezTimer(true);

	strcpy(corpse_name, pp->name);
	strcpy(name, pp->name);

	

	/* become_npc was not being initialized which led to some pretty funky things with newly created corpses */
	become_npc = false;

	SetPlayerKillItemID(0);

	/* Check Rule to see if we can leave corpses */
	if(!RuleB(Character, LeaveNakedCorpses) ||
		RuleB(Character, LeaveCorpses) &&
		GetLevel() >= RuleI(Character, DeathItemLossLevel) || RuleB(Character, LeaveCorpses) && client->IsHardcore() && GetLevel() >= RuleI(Quarm, HardcoreDeathLevel)){
		// cash
		SetCash(pp->copper, pp->silver, pp->gold, pp->platinum);
		pp->copper = 0;
		pp->silver = 0;
		pp->gold = 0;
		pp->platinum = 0;

		// get their tints
		memcpy(&item_tint.Slot, &client->GetPP().item_material, sizeof(item_tint));

		// worn + inventory + cursor
		std::list<uint32> removed_list;
		bool cursor = false;

		for (i = EQ::invslot::SLOT_BEGIN; i < EQ::invslot::POSSESSIONS_COUNT; i++)
		{
			item = client->GetInv().GetItem(i);
			if (item && !item->GetItem()->Soulbound)
			{
				if (!client->IsBecomeNPC() || (client->IsBecomeNPC() && !item->GetItem()->NoRent))
				{
					std::list<uint32> slot_list = MoveItemToCorpse(client, item, i);
					removed_list.merge(slot_list);
				}
			}
			else if (item && item->GetItem()->Soulbound)
				Log(Logs::Detail, Logs::Inventory, "Skipping Soulbound item %s in slot %d", item->GetItem()->Name, i);
		}

		for (i = EQ::invslot::TRADE_BEGIN; i < EQ::invslot::TRADE_END; i++)
		{
			item = client->GetInv().GetItem(i);
			if (item )
			{
				if (!client->IsBecomeNPC())
				{
					std::list<uint32> slot_list = MoveItemToCorpse(client, item, i);
					removed_list.merge(slot_list);
				}
			}
		}

		database.TransactionBegin();
		if (removed_list.size() != 0) {
			std::stringstream ss("");
			ss << "DELETE FROM character_inventory WHERE id=" << client->CharacterID();
			ss << " AND (";
			std::list<uint32>::const_iterator iter = removed_list.begin();
			bool first = true;
			while (iter != removed_list.end()) {
				if (first) {
					first = false;
				}
				else {
					ss << " OR ";
				}
				ss << "slotid=" << (*iter);
				++iter;
			}
			ss << ")";
			database.QueryDatabase(ss.str().c_str());
		}

		auto start = client->GetInv().cursor_cbegin();
		auto finish = client->GetInv().cursor_cend();
		// If soulbound items were moved to the cursor, they need to be moved to a primary inventory slot.
		database.SaveSoulboundItems(client, start, finish);

		// handle any left over items on cursor queue
		if (client->GetInv().CursorSize() > 0) {
			i = EQ::invslot::CURSOR_QUEUE_BEGIN + 1;
			// see if we have any non-soulboud items left in cursor queue
			while ( (item = client->GetInv().PopItem(EQ::invslot::slotCursor)) ) {
				if (item && !item->GetItem()->Soulbound) // soulbound were moved earlier
					AddItem(item->GetItem()->ID, item->GetCharges(), i++);
			}
			if (i > (EQ::invslot::CURSOR_QUEUE_BEGIN + 1)) {
				// now wipe out the cursor items in the db
				std::string query = StringFormat("DELETE FROM character_inventory WHERE id = %i "
					"AND ((slotid >= %i AND slotid <= %i) "
					"OR slotid = %i OR (slotid >= %i AND slotid <= %i) )",
					client->CharacterID(), EQ::invslot::CURSOR_QUEUE_BEGIN, EQ::invslot::CURSOR_QUEUE_END, 
					EQ::invslot::slotCursor, EQ::invbag::CURSOR_BAG_BEGIN, EQ::invbag::CURSOR_BAG_END);
				auto results = database.QueryDatabase(query);
			}
		}
			
		client->CalcBonuses(); // will only affect offline profile viewing of dead characters..unneeded overhead
		client->SetHP(-100);
		client->Save();

		IsRezzed(false);
		Save();
		database.TransactionCommit();
		if (client->IsHardcore())
			corpse_decay_timer.Start(1000);
		else if (!IsEmpty()) {
			corpse_decay_timer.Start(RuleI(Character, CorpseDecayTimeMS));
		}
		else if (IsEmpty() && RuleB(Character, SacrificeCorpseDepop) && killedby == Killed_Sac &&
			(GetZoneID() == Zones::POKNOWLEDGE || GetZoneID() == Zones::NEXUS || GetZoneID() == Zones::BAZAAR))
		{
			corpse_decay_timer.Start(180000);
		}

		return;
	} //end "not leaving naked corpses"

	UpdateEquipmentLight();
	UpdateActiveLight();


	npc_clean_name = GetCleanName();

	IsRezzed(false);
	Save();
}

std::list<uint32> Corpse::MoveItemToCorpse(Client *client, EQ::ItemInstance *item, int16 equipslot)
{
	int bagindex;
	int16 interior_slot;
	EQ::ItemInstance *interior_item;
	std::list<uint32> returnlist;

	AddItem(item->GetItem()->ID, item->GetCharges(), equipslot);
	returnlist.push_back(equipslot);

	// Qualified bag slot iterations. processing bag slots that don't exist is probably not a good idea.
	// Limit the bag check to inventory, trade, and cursor slots.
	if (item->IsClassBag() &&
		(equipslot == EQ::invslot::SLOT_BEGIN ||
		(equipslot >= EQ::invslot::GENERAL_BEGIN && equipslot <= EQ::invslot::GENERAL_END) || 
		(equipslot >= EQ::invslot::TRADE_BEGIN && equipslot <= EQ::invslot::TRADE_END)))
	{ 
		for (bagindex = EQ::invbag::SLOT_BEGIN; bagindex <= EQ::invbag::SLOT_END; bagindex++) 
		{
			// For empty bags in cursor queue, slot was previously being resolved as SLOT_INVALID (-1)
			interior_slot = EQ::InventoryProfile::CalcSlotId(equipslot, bagindex);
			interior_item = client->GetInv().GetItem(interior_slot);

			if (interior_item && !interior_item->GetItem()->Soulbound) 
			{
				AddItem(interior_item->GetItem()->ID, interior_item->GetCharges(), interior_slot);
				returnlist.push_back(EQ::InventoryProfile::CalcSlotId(equipslot, bagindex));
				client->DeleteItemInInventory(interior_slot);
			}
			else if(interior_item && interior_item->GetItem()->Soulbound)
			{
				client->PushItemOnCursor(*interior_item, true); // Push to cursor for now, since parent bag is about to be deleted.
				client->DeleteItemInInventory(interior_slot);
				Log(Logs::Detail, Logs::Inventory, "Skipping Soulbound item %s in slot %d", interior_item->GetItem()->Name, interior_slot);
			}
		}
	}
	client->DeleteItemInInventory(equipslot);
	return returnlist;
}

/* Called from Database Load */

Corpse::Corpse(uint32 in_dbid, uint32 in_charid, const char* in_charname, LootItems* in_itemlist, uint32 in_copper, uint32 in_silver, uint32 in_gold, uint32 in_plat, const glm::vec4& position, float in_size, uint8 in_gender, uint16 in_race, uint8 in_class, uint8 in_deity, uint8 in_level, uint8 in_texture, uint8 in_helmtexture, uint32 in_rezexp, uint32 in_gmrezexp, uint8 in_killedby, bool in_rezzable, uint32 in_rez_time, bool wasAtGraveyard)
	: Mob("Unnamed_Corpse", // const char* in_name,
	"",						// const char* in_lastname,
	0,						// int32		in_cur_hp,
	0,						// int32		in_max_hp,
	in_gender,				// uint8		in_gender,
	in_race,				// uint16		in_race,
	in_class,				// uint8		in_class,
	BT_Humanoid,			// bodyType	in_bodytype,
	in_deity,				// uint8		in_deity,
	in_level,				// uint8		in_level,
	0,						// uint32		in_npctype_id,
	in_size,				// float		in_size,
	0,						// float		in_runspeed,
	position,
	0,						// uint8		in_light,
	in_texture,				// uint8		in_texture,
	in_helmtexture,			// uint8		in_helmtexture,
	0,						// uint16		in_ac,
	0,						// uint16		in_atk,
	0,						// uint16		in_str,
	0,						// uint16		in_sta,
	0,						// uint16		in_dex,
	0,						// uint16		in_agi,
	0,						// uint16		in_int,
	0,						// uint16		in_wis,
	0,						// uint16		in_cha,
	0,						// uint8		in_haircolor,
	0,						// uint8		in_beardcolor,
	0,						// uint8		in_eyecolor1, // the eyecolors always seem to be the same, maybe left and right eye?
	0,						// uint8		in_eyecolor2,
	0,						// uint8		in_hairstyle,
	0,						// uint8		in_luclinface,
	0,						// uint8		in_beard,
	EQ::TintProfile(),		// uint32		in_armor_tint[EQ::textures::TextureCount], 
	0xff,					// uint8		in_aa_title,
	0,						// uint8		in_see_invis, // see through invis/ivu
	0,						// uint8		in_see_invis_undead,
	0,						// uint8		in_see_sneak,
	0,						// uint8		in_see_improved_hide,
	0,						// int32		in_hp_regen,
	0,						// int32		in_mana_regen,
	0,						// uint8		in_qglobal,
	0,						// uint8		in_maxlevel,
	0,						// uint32		in_scalerate
	0,						// uint8		in_armtexture,
	0,						// uint8		in_bracertexture,
	0,						// uint8		in_handtexture,
	0,						// uint8		in_legtexture,
	0,						// uint8		in_feettexture,
	0						// uint8		in_chesttexture,
	),						
	corpse_decay_timer(RuleI(Character, EmptyCorpseDecayTimeMS)),
	corpse_rez_timer(RuleI(Character, CorpseResTimeMS)),
	corpse_delay_timer(RuleI(NPC, CorpseUnlockTimer)),
	corpse_graveyard_timer(zone->graveyard_timer() * 60000),
	loot_cooldown_timer(10)
{
	// The timer needs to know if the corpse has items or not before we actually apply the items
	// to the corpse. The corpse seems to poof shortly after the timer is applied if it is done so
	// after items are loaded.
	bool empty = true;
	if (!in_itemlist->empty() || in_copper != 0 || in_silver != 0 || in_gold != 0 || in_plat != 0)
		empty = false;

	LoadPlayerCorpseDecayTime(in_dbid, empty);

	if (!zone->HasGraveyard() || wasAtGraveyard || zone->GetGuildID() == GUILD_NONE && zone->GetZoneID() < 1000){
		corpse_graveyard_timer.Disable();
	}
	corpse_graveyard_moved_timer.Disable();

	is_corpse_changed = false;
	is_player_corpse = true;
	is_locked = false;
	being_looted_by = 0xFFFFFFFF;
	loot_lockout_timer = 0;
	corpse_db_id = in_dbid;
	player_corpse_depop = false;
	char_id = in_charid;
	itemlist = *in_itemlist;
	in_itemlist->clear();

	strcpy(corpse_name, in_charname);
	strcpy(name, in_charname);

	this->copper = in_copper;
	this->silver = in_silver;
	this->gold = in_gold;
	this->platinum = in_plat;

	rez_experience = in_rezexp;
	gm_rez_experience = in_gmrezexp;
	killedby = in_killedby;
	rezzable = in_rezzable;
	rez_time = in_rez_time;
	is_owner_online = false;

	owner_online_timer.Start(RuleI(Character, CorpseOwnerOnlineTimeMS));

	corpse_rez_timer.Disable();
	SetRezTimer();

	npc_clean_name = GetCleanName();

	allowed_looters.clear();

	SetPlayerKillItemID(0);

	UpdateEquipmentLight();

	UpdateActiveLight();
}

Corpse::~Corpse() {
	if (is_player_corpse && !(player_corpse_depop && corpse_db_id == 0)) {
		Save();
	}
	LootItems::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		LootItem* item = *cur;
		safe_delete(item);
	}
	itemlist.clear();
	ResetLegacyItemLooterSet();
}

/*
this needs to be called AFTER the entity_id is set
the client does this too, so it's unchangeable
*/
void Corpse::CalcCorpseName() {
	EntityList::RemoveNumbers(name);
	char tmp[64];
	if (is_player_corpse){
		snprintf(tmp, sizeof(tmp), "'s corpse%d", GetID());
	}
	else{
		snprintf(tmp, sizeof(tmp), "`s_corpse%d", GetID());
	}
	name[(sizeof(name) - 1) - strlen(tmp)] = 0;
	strcat(name, tmp);
}

bool Corpse::Save() {
	if (!is_player_corpse) {
		return true;
	}
	if (!is_corpse_changed) {
		return true;
	}

	uint32 tmp = this->CountItems();
	uint32 tmpsize = sizeof(CharacterCorpseEntry) + (tmp * sizeof(LootItem));

	CharacterCorpseEntry *ce = (CharacterCorpseEntry*) new uchar[tmpsize];
	memset(ce, 0, tmpsize);
	ce->itemcount = tmp;
	ce->size = this->size;
	ce->locked = is_locked;
	ce->copper = this->copper;
	ce->silver = this->silver;
	ce->gold = this->gold;
	ce->plat = this->platinum;
	ce->race = this->race;
	ce->class_ = class_;
	ce->gender = gender;
	ce->deity = deity;
	ce->level = level;
	ce->texture = this->texture;
	ce->helmtexture = this->helmtexture;
	ce->exp = rez_experience;
	ce->gmexp = gm_rez_experience;
	ce->killedby = killedby;
	ce->rezzable = rezzable;
	ce->rez_time = rez_time;

	memcpy(&ce->item_tint.Slot, &item_tint.Slot, sizeof(ce->item_tint));
	ce->item_tint = item_tint;
	ce->haircolor = haircolor;
	ce->beardcolor = beardcolor;
	ce->eyecolor2 = eyecolor1;
	ce->hairstyle = hairstyle;
	ce->face = luclinface;
	ce->beard = beard;
	ce->time_of_death = time_of_death;

	uint32 x = 0;
	LootItems::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		LootItem* item = *cur;
		memcpy((char*)&ce->items[x++], (char*)item, sizeof(LootItem));
	}

	/* Create New Corpse*/
	if (corpse_db_id == 0) {
		corpse_db_id = database.SaveCharacterCorpse(char_id, corpse_name, zone->GetZoneID(), zone->GetGuildID(), ce, m_Position);
		if(!IsEmpty() && RuleB(Character, UsePlayerCorpseBackups))
		{
			database.SaveCharacterCorpseBackup(corpse_db_id, char_id, corpse_name, zone->GetZoneID(), zone->GetGuildID(), ce, m_Position);
		}
	}
	/* Update Corpse Data */
	else {
		corpse_db_id = database.UpdateCharacterCorpse(corpse_db_id, char_id, corpse_name, zone->GetZoneID(), zone->GetGuildID(), ce, m_Position, IsRezzed());
		if(!IsEmpty() && RuleB(Character, UsePlayerCorpseBackups))
		{
			database.UpdateCharacterCorpseBackup(corpse_db_id, char_id, corpse_name, zone->GetZoneID(), zone->GetGuildID(), ce, m_Position, IsRezzed());
		}
	}

	safe_delete_array(ce);

	return true;
}

void Corpse::Delete() 
{
	if (IsPlayerCorpse() && corpse_db_id != 0)
	{
		RevokeConsent();
		database.DeleteCharacterCorpse(corpse_db_id);
	}

	corpse_db_id = 0;
	player_corpse_depop = true;
}

void Corpse::Bury() 
{

	if (IsPlayerCorpse() && corpse_db_id != 0)
	{
		RevokeConsent();
		database.BuryCharacterCorpse(corpse_db_id);
	}

	corpse_db_id = 0;
	player_corpse_depop = true;
}

void Corpse::DepopNPCCorpse() {
	if (IsNPCCorpse())
		player_corpse_depop = true;
}

void Corpse::DepopPlayerCorpse() {
	player_corpse_depop = true;
}

uint32 Corpse::CountItems() {
	return itemlist.size();
}

void Corpse::AddItem(uint32 itemnum, int8 charges, int16 slot) {
	if (!database.GetItem(itemnum))
		return;

	Log(Logs::Detail, Logs::Inventory, "(Corpse) AddItem(%i, %i, %i)", slot, charges, itemnum);

	is_corpse_changed = true;

	auto item = new LootItem;
	
	memset(item, 0, sizeof(LootItem));
	item->item_id = itemnum;
	item->charges = charges;
	item->equip_slot = slot;
	item->min_looter_level = 0;
	item->item_loot_lockout_timer = 0;
	itemlist.push_back(item);

	UpdateEquipmentLight();
}

LootItem* Corpse::GetItem(uint16 lootslot, LootItem** bag_item_data) {
	LootItem *sitem = nullptr, *sitem2 = nullptr;

	LootItems::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		if((*cur)->lootslot == lootslot) {
			sitem = *cur;
			break;
		}
	}

	if (sitem && bag_item_data && EQ::InventoryProfile::SupportsContainers(sitem->equip_slot)) {
		int16 bagstart = EQ::InventoryProfile::CalcSlotId(sitem->equip_slot, EQ::invbag::SLOT_BEGIN);

		cur = itemlist.begin();
		end = itemlist.end();
		for (; cur != end; ++cur) {
			sitem2 = *cur;
			if (sitem2->equip_slot >= bagstart && sitem2->equip_slot < bagstart + 10) {
				bag_item_data[sitem2->equip_slot - bagstart] = sitem2;
			}
		}
	}

	return sitem;
}

uint32 Corpse::GetWornItem(int16 equipSlot) const {
	LootItems::const_iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		LootItem* item = *cur;
		if (item->equip_slot == equipSlot) {
			return item->item_id;
		}
	}

	return 0;
}

void Corpse::RemoveItem(uint16 lootslot) {
	if (lootslot == 0xFFFF)
		return;

	LootItems::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		LootItem* sitem = *cur;
		if (sitem->lootslot == lootslot) {
			RemoveItem(sitem);
			return;
		}
	}
}

void Corpse::RemoveItem(LootItem* item_data)
{
	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter) {
		auto sitem = *iter;
		if (sitem != item_data) { continue; }

		is_corpse_changed = true;
		itemlist.erase(iter);

		uint8 material = EQ::InventoryProfile::CalcMaterialFromSlot(sitem->equip_slot); // autos to unsigned char

		if (IsNPCCorpse() && sitem->equip_slot == EQ::invslot::slotRange && material == EQ::textures::materialInvalid)
		{
			material = EQ::textures::weaponPrimary;
		}

		if (material != EQ::textures::materialInvalid)
			SendWearChange(material);

		UpdateEquipmentLight();
		if (UpdateActiveLight())
			SendAppearancePacket(AppearanceType::Light, GetActiveLightType());

		safe_delete(sitem);
		return;
	}
}

void Corpse::SetCash(uint32 in_copper, uint32 in_silver, uint32 in_gold, uint32 in_platinum) {
	this->copper = in_copper;
	this->silver = in_silver;
	this->gold = in_gold;
	this->platinum = in_platinum;
	is_corpse_changed = true;
}

void Corpse::RemoveCash() {
	this->copper = 0;
	this->silver = 0;
	this->gold = 0;
	this->platinum = 0;
	is_corpse_changed = true;
}

bool Corpse::IsEmpty() const {
	if (copper != 0 || silver != 0 || gold != 0 || platinum != 0)
		return false;

	return itemlist.empty();
}

bool Corpse::DepopProcess() {
	if (player_corpse_depop) {
		return false;
	}
	return true;
}

bool Corpse::Process() {
	if (player_corpse_depop){
		return false;
	}

	if(owner_online_timer.Check() && rezzable) 
	{
		IsOwnerOnline();
	}

	if (corpse_delay_timer.Check()) {
		allowed_looters.clear();
		corpse_delay_timer.Disable();
		return true;
	}

	if (worldserver.Connected() && corpse_graveyard_timer.Check() && zone->GetGuildID() != GUILD_NONE) {
		if (zone->HasGraveyard()) {
			Save();

			if (being_looted_by != 0xFFFFFFFF)
			{
				Client *looter = entity_list.GetClientByID(being_looted_by);
				if (looter)
				{
					EndLoot(looter, nullptr);
				}
			}

			player_corpse_depop = true;
			database.SendCharacterCorpseToGraveyard(corpse_db_id, zone->graveyard_zoneid(), GUILD_NONE, zone->GetGraveyardPoint());
			corpse_graveyard_timer.Disable();
			auto pack = new ServerPacket(ServerOP_SpawnPlayerCorpse, sizeof(SpawnPlayerCorpse_Struct));
			SpawnPlayerCorpse_Struct* spc = (SpawnPlayerCorpse_Struct*)pack->pBuffer;
			spc->player_corpse_id = corpse_db_id;
			spc->zone_id = zone->graveyard_zoneid();
			spc->GuildID = GUILD_NONE;
			worldserver.SendPacket(pack);
			safe_delete(pack);
			Log(Logs::General, Logs::Corpse, "Moved %s player corpse to the designated graveyard in zone %s.", this->GetName(), database.GetZoneName(zone->graveyard_zoneid()));
			corpse_db_id = 0;
		}

		corpse_graveyard_timer.Disable();
		return false;
	}

	if (IsPlayerCorpse() && zone->HasGraveyard() && !corpse_graveyard_timer.Enabled() && zone->GetGuildID() != GUILD_NONE && zone->GetZoneID() >= 1000)
	{
		// it already went to GY, prevent dragging it back out
		if (!corpse_graveyard_moved_timer.Enabled())
			corpse_graveyard_moved_timer.Start(2000);
		
		if (corpse_graveyard_moved_timer.Check() && corpse_graveyard_moved_lastpos != GetPosition())
		{
			if (DistanceSquaredNoZ(zone->GetGraveyardPoint(), GetPosition()) > 75.0f * 75.0f)
			{
				Log(Logs::General, Logs::Corpse, "Graveyard corpse %s was moved too far from the graveyard, returning it.", GetName());
				double xcorpse = (zone->GetGraveyardPoint().x + zone->random.Real(-20, 20));
				double ycorpse = (zone->GetGraveyardPoint().y + zone->random.Real(-20, 20));
				GMMove(xcorpse, ycorpse, zone->GetGraveyardPoint().z);
				is_corpse_changed = true;
				Save();
			}
			corpse_graveyard_moved_lastpos = GetPosition();
		}
	}

	//Player is offline. If rez timer is enabled, disable it and save corpse.
	if(!is_owner_online && rezzable)
	{
		if(corpse_rez_timer.Enabled())
		{
			rez_time = corpse_rez_timer.GetRemainingTime();
			corpse_rez_timer.Disable();
			is_corpse_changed = true;
			Save();
		}
	}
	//Player is online. If rez timer is disabled, enable it.
	else if(is_owner_online && rezzable)
	{
		if(corpse_rez_timer.Enabled())
		{
			rez_time = corpse_rez_timer.GetRemainingTime();
		}
		else
		{
			SetRezTimer();
		}
	}

	if(corpse_rez_timer.Check()) 
	{
		CompleteResurrection(true);
	}	

	/* This is when a corpse hits decay timer and does checks*/
	if (corpse_decay_timer.Check()) 
	{
		/* NPC */
		if (IsNPCCorpse())
		{
			corpse_decay_timer.Disable();
			return false;
		}
		else
		{
			/* Client */
			if (!RuleB(Zone, EnableShadowrest)) 
			{
				Delete();
			}
			else 
			{
				Bury();
			}
			corpse_decay_timer.Disable();
			return false;
		}
	}

	return true;
}

void Corpse::SetDecayTimer(uint32 decaytime) {
	if (decaytime == 0)
		corpse_decay_timer.Trigger();
	else
		corpse_decay_timer.Start(decaytime);
}

bool Corpse::CanPlayerLoot(std::string playername) {
	uint8 looters = 0;

	Client* c = entity_list.GetClientByName(playername.c_str());
	if (c && c->IsSelfFound() || c && c->IsSoloOnly())
	{
		std::string appendedCharName = c->GetSSFLooterName();
		auto temporarily_allowed_itr = temporarily_allowed_looters.find(appendedCharName);

		if (temporarily_allowed_itr == temporarily_allowed_looters.end() && c->IsLootLockedOutOfNPC(npctype_id) && npctype_id != 0)
		{
			return false;
		}

		if (denied_looters.find(appendedCharName) != denied_looters.end()) {
			return false;
		}

		if (!c->HasRaid()) {
			if (initial_allowed_looters.find(appendedCharName) != initial_allowed_looters.end()) {
				return true;
			}
		}
		else
		{
			if (allowed_looters.empty() && initial_allowed_looters.find(appendedCharName) != initial_allowed_looters.end())
			{
				// Clearing allowed_looters is the mechanism by which a corpse is opened
				// to FFA looting. For SF players, the corpse is only unlocked for those
				// players who were present at the moment of FTE in an eligible raid,
				// which is indicated by being in initial_allowed_looters.
				c->Message(Chat::Cyan, "You were in an eligible raid that was first to engage, so you are allowed to loot.");
				return true;
			}
			else if (allowed_looters.find(appendedCharName) == allowed_looters.end() && initial_allowed_looters.find(appendedCharName) != initial_allowed_looters.end())
			{
				Raid* raid = c->GetRaid();
				if (raid->GetLootType() == 3) // Looter / Raid Leader loot
				{
					if (raid->IsRaidLooter(c->GetCleanName()))
					{
						for (int x = 0; x < MAX_RAID_MEMBERS; x++)
						{
							if (raid->members[x].member)
							{
								if (allowed_looters.find(raid->members[x].member->GetSSFLooterName()) != allowed_looters.end())
								{
									c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid with another eligible member of the raid.");
									AllowPlayerLoot(appendedCharName);
									break;
								}
							}
						}
					}
				}
				else if (raid->GetLootType() == 2) // Group Leader / Raid Leader loot
				{
					if (raid->IsRaidLeader(c->GetCleanName()) || raid->IsGroupLeader(c->GetCleanName()))
					{
						for (int x = 0; x < MAX_RAID_MEMBERS; x++)
						{
							if (raid->members[x].member)
							{
								if (allowed_looters.find(raid->members[x].member->GetSSFLooterName()) != allowed_looters.end())
								{
									c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid and you're a group or raid leader.");
									AllowPlayerLoot(appendedCharName);
									break;
								}
							}
						}
					}
				}
				else if (raid->GetLootType() == 1 && raid->IsRaidLeader(c->GetCleanName())) // Raid Leader loot
				{
					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (raid->members[x].member)
						{
							if (allowed_looters.find(raid->members[x].member->GetSSFLooterName()) != allowed_looters.end())
							{
								c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid and you're the new raid leader.");
								AllowPlayerLoot(appendedCharName);
								break;
							}
						}
					}
				}
				else if (raid->GetLootType() == 4) // Raid Leader loot
				{
					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (raid->members[x].member)
						{
							if (allowed_looters.find(raid->members[x].member->GetSSFLooterName()) != allowed_looters.end())
							{
								c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid and the loot is set to free-for-all.");
								AllowPlayerLoot(appendedCharName);
								break;
							}
						}
					}
				}
			}
		}

		if (allowed_looters.find(appendedCharName) != allowed_looters.end()) {
				return true;
		}
	}
	else if(c)
	{

		if (npctype_id != 0 && loot_lockout_timer > 0)
		{
			auto temporarily_allowed_itr = temporarily_allowed_looters.find(playername);

			if (temporarily_allowed_itr == temporarily_allowed_looters.end() && c->IsLootLockedOutOfNPC(npctype_id))
			{
				c->Message(Chat::Red, "You were locked out of this creature on its death, and are not eligible to loot.");
				return false;
			}
		}

		if (denied_looters.find(playername) != denied_looters.end()) {
			c->Message(Chat::Red, "You are not allowed to loot this NPC as you are locked out of the creature in question.");
			return false;
		}

		/*
		Check if client is in a raid + is a looter and retroactively
		allow looting of the corpse if the raid looters can loot the corpse
		*/
		if (c->HasRaid()) {
			if (allowed_looters.find(c->GetCleanName()) == allowed_looters.end())
			{
				Raid* raid = c->GetRaid();
				if (raid->GetLootType() == 3) // Looter / Raid Leader loot
				{
					if (raid->IsRaidLooter(c->GetCleanName()))
					{
						for (int x = 0; x < MAX_RAID_MEMBERS; x++)
						{
							if (raid->members[x].membername[0])
							{
								if (allowed_looters.find(raid->members[x].membername) != allowed_looters.end())
								{
									c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid with another eligible member of the raid.");
									AddLooter(c);
									break;
								}
							}
						}
					}
				}
				else if (raid->GetLootType() == 2) // Group Leader / Raid Leader loot
				{
					if (raid->IsRaidLeader(c->GetCleanName()) || raid->IsGroupLeader(c->GetCleanName()))
					{
						for (int x = 0; x < MAX_RAID_MEMBERS; x++)
						{
							if (raid->members[x].membername[0])
							{
								if (allowed_looters.find(raid->members[x].membername) != allowed_looters.end())
								{
									c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid and you're a group or raid leader.");
									AddLooter(c);
									break;
								}
							}
						}
					}
				}
				else if (raid->GetLootType() == 1 && raid->IsRaidLeader(c->GetCleanName())) // Raid Leader loot
				{
					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (raid->members[x].membername[0])
						{
							if (allowed_looters.find(raid->members[x].membername) != allowed_looters.end())
							{
								c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid and you're the new raid leader.");
								AddLooter(c);
								break;
							}
						}
					}
				}
				else if (raid->GetLootType() == 4) // Raid Leader loot
				{
					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (raid->members[x].membername[0])
						{
							if (allowed_looters.find(raid->members[x].membername) != allowed_looters.end())
							{
								c->Message(Chat::Cyan, "Adding you to the looter list of this corpse. You are in a raid and the loot is set to free-for-all.");
								AddLooter(c);
								break;
							}
						}
					}
				}
			}
		}

		if (c->HasGroup()) {
			for (int x = 0; x < MAX_GROUP_MEMBERS; x++) {
				if (c->GetGroup()->membername[0]) {
					if (allowed_looters.find(c->GetGroup()->membername[0]) != allowed_looters.end()) {
						{
							AddLooter(c);
							break;
						}
					}
				}
			}
		}

		if (allowed_looters.find(playername) != allowed_looters.end()) {
				return true;
		}

		/* If we have no looters, obviously client can loot */
		if (allowed_looters.size() == 0) {
				return true;
		}
	}
	if (c && c->HasRaid()) {
		c->Message(Chat::Red, "[DEBUG] You are in a raid, but cannot loot this corpse.");
	}
	return false;
}

void Corpse::AllowPlayerLoot(Mob *them) 
{

	if (them == nullptr || !them->IsClient())
		return;

	std::string playername = them->CastToClient()->GetCleanName();
	if (them->CastToClient()->IsSelfFound())
		playername += "-SF";

	if (them->CastToClient()->IsSoloOnly())
		playername += "-Solo";

	if (allowed_looters.find(playername) == allowed_looters.end())
		allowed_looters.emplace(playername);

	if (initial_allowed_looters.find(playername) == initial_allowed_looters.end())
		initial_allowed_looters.emplace(playername);

}



void Corpse::DenyPlayerLoot(std::string character_name)
{
	if (character_name.size() == 0)
		return;

	std::string playername = character_name;
	std::string playernameSelfFound = character_name;
	std::string playernameSolo = character_name;
	std::string playernameSoloSelfFound = character_name;
	playernameSelfFound += "-SF";
	playernameSolo += "-Solo";

	playernameSoloSelfFound += "-SF";
	playernameSoloSelfFound += "-Solo";

	std::unordered_set<std::string>::iterator nameItr = allowed_looters.find(playername);
	if (nameItr != allowed_looters.end())
	{
		allowed_looters.erase(nameItr);
	}

	nameItr = allowed_looters.find(playernameSelfFound);
	if (nameItr != allowed_looters.end())
	{
		allowed_looters.erase(nameItr);
	}

	nameItr = allowed_looters.find(playernameSolo);
	if (nameItr != allowed_looters.end())
	{
		allowed_looters.erase(nameItr);
	}

	nameItr = allowed_looters.find(playernameSoloSelfFound);
	if (nameItr != allowed_looters.end())
	{
		allowed_looters.erase(nameItr);
	}

	std::unordered_set<std::string>::iterator initialNameItr = initial_allowed_looters.find(playername);
	if (initialNameItr != initial_allowed_looters.end())
	{
		initial_allowed_looters.erase(initialNameItr);
	}

	initialNameItr = initial_allowed_looters.find(playernameSelfFound);
	if (initialNameItr != initial_allowed_looters.end())
	{
		initial_allowed_looters.erase(initialNameItr);
	}

	initialNameItr = initial_allowed_looters.find(playernameSolo);
	if (initialNameItr != initial_allowed_looters.end())
	{
		initial_allowed_looters.erase(initialNameItr);
	}

	initialNameItr = initial_allowed_looters.find(playernameSoloSelfFound);
	if (initialNameItr != initial_allowed_looters.end())
	{
		initial_allowed_looters.erase(initialNameItr);
	}

	std::unordered_set<std::string>::iterator deniedNameItr = denied_looters.find(playername);
	if (deniedNameItr == denied_looters.end())
	{
		denied_looters.emplace(playername);
	}

	deniedNameItr = denied_looters.find(playernameSelfFound);
	if (deniedNameItr == denied_looters.end())
	{
		denied_looters.emplace(playernameSelfFound);
	}

	deniedNameItr = denied_looters.find(playernameSolo);
	if (deniedNameItr == denied_looters.end())
	{
		denied_looters.emplace(playernameSolo);
	}

	deniedNameItr = denied_looters.find(playernameSoloSelfFound);
	if (deniedNameItr == denied_looters.end())
	{
		denied_looters.emplace(playernameSoloSelfFound);
	}

}

void Corpse::AllowPlayerLoot(std::string character_name) {
	if (character_name.size() == 0)
		return;

	if (allowed_looters.find(character_name) == allowed_looters.end())
		allowed_looters.emplace(character_name);

	// Quarm:
	// Solo / SF are intentionally excluded from this function as there are exploits related to SSF players being able to loot the body here.

}
void Corpse::MakeLootRequestPackets(Client* client, const EQApplicationPacket* app) {

	if (!client)
		return;

	// Added 12/08. Started compressing loot struct on live.
	if(player_corpse_depop) {
		client->Message(Chat::Red, "[DEBUG] This corpse is being depooped!");
		SendEndLootErrorPacket(client);
		return;
	}

	if(IsPlayerCorpse() && corpse_db_id == 0) {
		// SendLootReqErrorPacket(client, 0);
		client->Message(Chat::Red, "Warning: Corpse's dbid = 0! Corpse will not survive zone shutdown!");
		std::cout << "Error: PlayerCorpse::MakeLootRequestPackets: dbid = 0!" << std::endl;
		// return;
	}

	if(is_locked && client->Admin() < AccountStatus::GMAdmin) {
		SendLootReqErrorPacket(client, 0);
		client->Message(Chat::Red, "Error: Corpse locked by GM.");
		return;
	}

	if (zone && zone->IsTrivialLootCodeEnabled() && !IsPlayerCorpse() && client && client->GetLevelCon(GetLevel()) == CON_GREEN)
	{
		SendLootReqErrorPacket(client, 0);
		client->Message(Chat::Red, "Trivial Loot Code is enabled in this zone. You are unable to loot any NPC that is considered green to you.");
		return;
	}

	if(being_looted_by == 0) { 
		being_looted_by = 0xFFFFFFFF; 
	}

	if(this->being_looted_by != 0xFFFFFFFF) {
		// lets double check....
		Client* looter = entity_list.GetClientByID(this->being_looted_by);
		if(looter == 0) { 
			ResetLooter();
		}
	}

	bool contains_legacy_item = ContainsLegacyItem();
	uint8 Loot_Request_Type = 1;
	bool loot_coin = false;
	std::string tmp;
	if(database.GetVariable("LootCoin", tmp))
		loot_coin = tmp[0] == 1 && tmp[1] == '\0';

	if (this->being_looted_by != 0xFFFFFFFF && this->being_looted_by != client->GetID() && !contains_legacy_item) {
		SendLootReqErrorPacket(client, 0);
		Loot_Request_Type = 0;
		client->Message(Chat::Red, "[DEBUG] This corpse is being looted already by %i.", being_looted_by);
	}
	else if (IsPlayerCorpse() && char_id == client->CharacterID()) {
		Loot_Request_Type = 2;
	}
	else if ((IsNPCCorpse() || become_npc) && CanPlayerLoot(client->GetCleanName())) {
		Loot_Request_Type = 2;
	}
	else if (GetPlayerKillItem() == -1 && CanPlayerLoot(client->GetCleanName())) { /* PVP loot all items, variable cash */
		Loot_Request_Type = 3;
	}
	else if (GetPlayerKillItem() == 1 && CanPlayerLoot(client->GetCleanName())) { /* PVP loot 1 item, variable cash */
		Loot_Request_Type = 4;
	}
	else if (GetPlayerKillItem() > 1 && CanPlayerLoot(client->GetCleanName())) { /* PVP loot 1 set item, variable cash */
		Loot_Request_Type = 5;
	}

	if (Loot_Request_Type == 1) {
		if (client->Admin() < AccountStatus::GMAdmin || !client->GetGM()) {
			SendLootReqErrorPacket(client, 2);
		}
	}

	if(Loot_Request_Type >= 2 || (Loot_Request_Type == 1 && client->Admin() >= AccountStatus::GMAdmin && client->GetGM())) {
		if (contains_legacy_item) {
			AddLegacyItemLooter(client->GetCleanName());
		} else {
			this->being_looted_by = client->GetID();
		}
		auto outapp = new EQApplicationPacket(OP_MoneyOnCorpse, sizeof(moneyOnCorpseStruct));
		moneyOnCorpseStruct* d = (moneyOnCorpseStruct*)outapp->pBuffer;

		d->response		= 1;
		d->unknown1		= 0x42;
		d->unknown2		= 0xef;

		/* Don't take the coin off if it's a gm peeking at the corpse */
		if(Loot_Request_Type == 2 || (Loot_Request_Type >= 3 && loot_coin)) { 
			if(!IsPlayerCorpse() && client->IsGrouped() && client->AutoSplitEnabled() && client->GetGroup()) {
				d->copper		= 0;
				d->silver		= 0;
				d->gold			= 0;
				d->platinum		= 0;
				Group *cgroup = client->GetGroup();
				cgroup->SplitMoney(GetCopper(), GetSilver(), GetGold(), GetPlatinum(), client);
				/* QS: Player_Log_Looting */
				if (RuleB(QueryServ, PlayerLogLoot))
				{
					QServ->QSLootRecords(client->CharacterID(), corpse_name, "CASH-SPLIT", client->GetZoneID(), 0, "null", 0, GetPlatinum(), GetGold(), GetSilver(), GetCopper());
				}
			}
			else {
				d->copper = this->GetCopper();
				d->silver = this->GetSilver();
				d->gold = this->GetGold();
				d->platinum = this->GetPlatinum();
				client->AddMoneyToPP(GetCopper(), GetSilver(), GetGold(), GetPlatinum(), false);
				/* QS: Player_Log_Looting */
				if (RuleB(QueryServ, PlayerLogLoot))
				{
					QServ->QSLootRecords(client->CharacterID(), corpse_name, "CASH", client->GetZoneID(), 0, "null", 0, GetPlatinum(), GetGold(), GetSilver(), GetCopper());
				}
			}

			RemoveCash();
			Save();
		}

		outapp->priority = 6;
		client->QueuePacket(outapp);
		safe_delete(outapp);
		if(Loot_Request_Type == 5) {
			int pkitem = GetPlayerKillItem();
			const EQ::ItemData* item = database.GetItem(pkitem);
			EQ::ItemInstance* inst = database.CreateItem(item, item->MaxCharges);
			if (inst) {
				client->SendItemPacket(EQ::invslot::CORPSE_BEGIN, inst, ItemPacketLoot);
				safe_delete(inst);
			}
			else { client->Message(Chat::Red, "Could not find item number %i to send!!", GetPlayerKillItem()); }

			client->QueuePacket(app);
			return;
		}

		int i = 0;
		const EQ::ItemData* item = nullptr;
		LootItems::iterator cur, end;
		cur = itemlist.begin();
		end = itemlist.end();

		int corpselootlimit = EQ::inventory::Lookup(EQ::versions::ConvertClientVersionToMobVersion(client->ClientVersion()))->InventoryTypeSize[EQ::invtype::typeCorpse];

		for (; cur != end; ++cur) {
			LootItem* item_data = *cur;
			item_data->lootslot = 0xFFFF;

			// Don't display the item if it's in a bag
			// Added cursor queue slots to corpse item visibility list. Nothing else should be making it to corpse.
			if (!IsPlayerCorpse() || item_data->equip_slot <= EQ::invslot::GENERAL_END || Loot_Request_Type >= 3 ||
				(item_data->equip_slot >= EQ::invslot::TRADE_BEGIN && item_data->equip_slot <= EQ::invslot::TRADE_END) || 
				(item_data->equip_slot >= EQ::invslot::CURSOR_QUEUE_BEGIN && item_data->equip_slot <= EQ::invslot::CURSOR_QUEUE_END)) {
				if (i < corpselootlimit) 
				{
					if(!RuleB(NPC, IgnoreQuestLoot) || (RuleB(NPC, IgnoreQuestLoot) && item_data->quest == 0))
					{
						item = database.GetItem(item_data->item_id);
						if(client && item && (item_data->quest == 0 || (item_data->quest == 1 && item->NoDrop != 0))) 
						{
							EQ::ItemInstance* inst = database.CreateItem(item, item_data->charges);
							if(inst) {
								// SlotGeneral1 is the corpse inventory start offset for Ti(EMu) - CORPSE_END = SlotGeneral1 + SlotCursor
								client->SendItemPacket(i, inst, ItemPacketLoot);
								safe_delete(inst);
							}
						}

						item_data->lootslot = i;
					}
				}

				i++;
			}
		}

		if(IsPlayerCorpse() && (char_id == client->CharacterID() || client->GetGM())) {
			if(i > corpselootlimit) {
				client->Message(Chat::Yellow, "*** This corpse contains more items than can be displayed! ***");
				client->Message(Chat::White, "Remove items and re-loot corpse to access remaining inventory.");
				client->Message(Chat::White, "(%s contains %i additional %s.)", GetName(), (i - corpselootlimit), (i - corpselootlimit) == 1 ? "item" : "items");
			}
			// this will happen if an item in a bag slot is on the corpse without its parent bag.
			if (IsPlayerCorpse() && i == 0 && itemlist.size() > 0) 
			{
				cur = itemlist.begin();
				end = itemlist.end();
				int8 slot = EQ::invslot::GENERAL_BEGIN;
				for (; cur != end; ++cur) 
				{
					LootItem* item_data = *cur;
					item = database.GetItem(item_data->item_id);
					if(item)
					{
						Log(Logs::General, Logs::Corpse, "Corpse Looting: %s was not sent to client loot window. Resetting to slot %d...", item->Name, slot);
						client->Message(Chat::White, "Inaccessible Corpse Item: %s. Please close the window and try to loot again.", item->Name);
						item_data->equip_slot = slot;
						Save();
					}
					++slot;
					if(slot > EQ::invslot::GENERAL_END)
						break;
				}
			}
		}
	}

	// Disgrace: Client seems to require that we send the packet back...
	client->QueuePacket(app);
}

bool Corpse::ContainsLegacyItem() {
	if (!IsPlayerCorpse()) {
		for (const auto& item_data : itemlist) {
			if (item_data->min_looter_level != 0) {
				return true;
			}
		}
	}
	return false;
}

void Corpse::LootCorpseItem(Client* client, const EQApplicationPacket* app) {
	/* This gets sent no matter what as a sort of ACK */
	if (!client)
	{
		return;
	}
	client->QueuePacket(app);

	bool contains_legacy_item = ContainsLegacyItem();
	if (!loot_cooldown_timer.Check()) {
		SendEndLootErrorPacket(client);
		if (contains_legacy_item) {
			RemoveLegacyItemLooter(client->GetCleanName());
		}
		//unlock corpse for others
		if (this->being_looted_by == client->GetID()) {
			ResetLooter();
		}
		return;
	}

	/* To prevent item loss for a player using 'Loot All' who doesn't have inventory space for all their items. */
	if (RuleB(Character, CheckCursorEmptyWhenLooting) && !client->GetInv().CursorEmpty()) {
		client->Message(Chat::Red, "You may not loot an item while you have an item on your cursor.");
		SendEndLootErrorPacket(client);
		if (contains_legacy_item) {
			RemoveLegacyItemLooter(client->GetCleanName());
		}
		/* Unlock corpse for others */
		if (this->being_looted_by == client->GetID()) {
			ResetLooter();
		}
		return;
	}

	LootingItem_Struct* lootitem = (LootingItem_Struct*)app->pBuffer;

	if (this->being_looted_by != client->GetID() && !contains_legacy_item) {
		client->Message(Chat::Red, "Error: Corpse::LootItem: BeingLootedBy != client");
		SendEndLootErrorPacket(client);
		return;
	}
	if (contains_legacy_item && !IsLegacyItemLooter(client->GetCleanName())) {
		client->Message(Chat::Red, "Error: Corpse::LootItem: client NOT in legacy looters set");
		SendEndLootErrorPacket(client);
		return;
	}

	if (IsPlayerCorpse() && !CanPlayerLoot(client->GetCleanName()) && !become_npc && (char_id != client->CharacterID() && client->Admin() < AccountStatus::GMLeadAdmin)) {
		client->Message(Chat::Red, "Error: This is a player corpse and you don't own it.");
		SendEndLootErrorPacket(client);
		return;
	}
	if (is_locked && client->Admin() < AccountStatus::GMAdmin) {
		SendLootReqErrorPacket(client, 0);
		client->Message(Chat::Red, "Error: Corpse locked by GM.");
		return;
	}
	
	if (IsPlayerCorpse() && (char_id != client->CharacterID()) && CanPlayerLoot(client->GetCleanName()) && GetPlayerKillItem() == 0){
		client->Message(Chat::Red, "Error: You cannot loot any more items from this corpse.");
		SendEndLootErrorPacket(client);
		ResetLooter();
		if (contains_legacy_item) { RemoveLegacyItemLooter(client->GetCleanName()); }
		return;
	}
	const EQ::ItemData* item = nullptr;
	EQ::ItemInstance *inst = nullptr;
	LootItem* item_data = nullptr, * bag_item_data[10] = {};

	memset(bag_item_data, 0, sizeof(bag_item_data));
	if (GetPlayerKillItem() > 1){
		item = database.GetItem(GetPlayerKillItem());
	}
	else if (GetPlayerKillItem() == -1 || GetPlayerKillItem() == 1){
		item_data = GetItem(lootitem->slot_id); //don't allow them to loot entire bags of items as pvp reward
	}
	else{
		item_data = GetItem(lootitem->slot_id, bag_item_data);
	}

	if (GetPlayerKillItem()<=1 && item_data != 0) {
		item = database.GetItem(item_data->item_id);
	}

	if (item != 0) {
		if (item_data){
			inst = database.CreateItem(item, item_data ? item_data->charges : 0);
		}
		else {
			inst = database.CreateItem(item);
		}
	}

		if (client->CheckLoreConflict(item)) {
			client->Message_StringID(0, LOOT_LORE_ERROR);
			SendEndLootErrorPacket(client);
			ResetLooter();
			if (contains_legacy_item) { RemoveLegacyItemLooter(client->GetCleanName()); }
			delete inst;
			return;
		}

		// search through bags for lore items
		if (item && item->IsClassBag()) {
			for (int i = 0; i < 10; i++) {
				if (bag_item_data[i])
				{
					const EQ::ItemData* bag_item = 0;
					bag_item = database.GetItem(bag_item_data[i]->item_id);
					if (bag_item && client->CheckLoreConflict(bag_item))
					{
						client->Message(Chat::White, "You cannot loot this container. The %s inside is a Lore Item and you already have one.", bag_item->Name);
						SendEndLootErrorPacket(client);
						ResetLooter();
						if (contains_legacy_item) { RemoveLegacyItemLooter(client->GetCleanName()); }
						delete inst;
						return;
					}
				}
			}
		}
		
		if (client && inst && item_data) {
			if (item_data->pet || item_data->quest)
			{
				if (client->IsSoloOnly() || client->IsSelfFound())
				{
					client->Message(Chat::Red, "This item is from a charmed pet, which is not allowed during a solo or self found run.");
					SendEndLootErrorPacket(client);
					ResetLooter();
					if (contains_legacy_item) { RemoveLegacyItemLooter(GetCleanName()); }
					delete inst;
					return;
				}
			}

		if (!IsPlayerCorpse() && item_data->min_looter_level != 0)
		{
			if (client->GetLevel() < item_data->min_looter_level)
			{
				client->Message(Chat::Red, "You cannot loot this type of legacy item. Required character level: %i", item_data->min_looter_level);
				SendEndLootErrorPacket(client);
				ResetLooter();
				if (contains_legacy_item) { RemoveLegacyItemLooter(client->GetCleanName()); }
				delete inst;
				return;
			}

			if (client->CheckLegacyItemLooted(item_data->item_id))
			{
				std::string time_message = client->GetLegacyItemLockoutFailureMessage(item_data->item_id);

				client->Message(Chat::Red, time_message.c_str());
				SendEndLootErrorPacket(client);
				ResetLooter();
				if (contains_legacy_item) { RemoveLegacyItemLooter(client->GetCleanName()); }
				delete inst;
				return;
			}

			int64_t expiration_timestamp = (int64_t)((int64_t)(Timer::GetTimeSeconds()) + (int64_t)item_data->item_loot_lockout_timer);


			if (item_data->item_loot_lockout_timer == 0)
			{
				expiration_timestamp = 0;
				client->Message(ChatChannel_Group, "You have looted a legacy item. You can no longer loot this legacy item from any NPC that is legacy item flagged, even if you destroy or trade it.");
			}
			else
			{
				client->Message(ChatChannel_Group, "You have looted a legacy item. You can no longer loot this legacy item from any NPC that is legacy item flagged until its timer expires in %s ", Strings::SecondsToTime(item_data->item_loot_lockout_timer).c_str());
			}
			client->AddLootedLegacyItem(item_data->item_id, expiration_timestamp);
		}

		char buf[88];
		char corpse_name[64];
		strcpy(corpse_name, GetName());
		snprintf(buf, 87, "%d %d %s", inst->GetItem()->ID, inst->GetCharges(), EntityList::RemoveNumbers(corpse_name));
		buf[87] = '\0';
		
		std::string export_string = fmt::format(
			"{} {} {}",
			inst->GetItem()->ID,
			inst->GetCharges(),
			EntityList::RemoveNumbers(corpse_name)
		);
		
		std::vector<std::any> args;
		args.push_back(inst);
		args.push_back(this);
		parse->EventPlayer(EVENT_LOOT, client, export_string, 0, &args);
		parse->EventItem(EVENT_LOOT, client, inst, this, export_string, 0);

		if ((RuleB(Character, EnableDiscoveredItems))) {
			if (client && !client->GetGM() && !client->IsDiscovered(inst->GetItem()->ID))
				client->DiscoverItem(inst->GetItem()->ID);
		}

		/* First add it to the looter - this will do the bag contents too */
		if (lootitem && lootitem->auto_loot) {
			if (!client->AutoPutLootInInventory(*inst, true, true, bag_item_data)) {
				client->PutLootInInventory(EQ::invslot::slotCursor, *inst, bag_item_data);
			}
		}
		else {
			client->PutLootInInventory(EQ::invslot::slotCursor, *inst, bag_item_data);
		}

		/* Remove it from Corpse */
		if (item_data){
			/* Delete needs to be before RemoveItem because its deletes the pointer for item_data/bag_item_data */
			database.DeleteItemOffCharacterCorpse(this->corpse_db_id, item_data->equip_slot, item_data->item_id);
			/* Delete Item Instance */
			RemoveItem(item_data->lootslot);
		}

		/* Remove Bag Contents */
		if (item->IsClassBag() && (GetPlayerKillItem() != -1 || GetPlayerKillItem() != 1)) {
			for (int i = EQ::invbag::SLOT_BEGIN; i <= EQ::invbag::SLOT_END; i++) {
				if (bag_item_data[i]) {
					/* Delete needs to be before RemoveItem because its deletes the pointer for item_data/bag_item_data */
					database.DeleteItemOffCharacterCorpse(this->corpse_db_id, bag_item_data[i]->equip_slot, bag_item_data[i]->item_id);
					/* Delete Item Instance */
					RemoveItem(bag_item_data[i]);
				}
			}
		}

		if (contains_legacy_item) {
			// send packets to other clients trying to loot the legacy item to cancel their loot
			for (std::string client_name : GetLegacyItemLooters()) {
				Client* other_client = entity_list.GetClientByName(client_name.c_str());
				if (other_client)
				{
					SendEndLootErrorPacket(other_client);
				}
			}
			ResetLegacyItemLooterSet();
		}

		if (GetPlayerKillItem() != -1){
			SetPlayerKillItemID(0);
		}

		//std::string item_link;
		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemInst);
		linker.SetItemInst(inst);

		linker.GenerateLink();

		if (!IsPlayerCorpse()) 
		{
			client->Message_StringID(Chat::Loot, LOOTED_MESSAGE, linker.Link().c_str());

			Group *g = client->GetGroup();
			if (g != nullptr) 
			{
				g->GroupMessage_StringID(client, Chat::Loot, OTHER_LOOTED_MESSAGE, client->GetName(), linker.Link().c_str());
			}
			else
			{
				Raid *r = client->GetRaid();
				if (r != nullptr) 
				{
					r->RaidMessage_StringID(client, Chat::Loot, OTHER_LOOTED_MESSAGE, client->GetName(), linker.Link().c_str());
				}
			}
		}
	}
	else {
		SendEndLootErrorPacket(client);
		if (contains_legacy_item) { RemoveLegacyItemLooter(client->GetCleanName()); }
		safe_delete(inst);
		return;
	}

	if(loot_lockout_timer > 0)
		AddPlayerLockout(client);

	/* QS: Player_Log_Looting */
	if (RuleB(QueryServ, PlayerLogLoot))
	{
		QServ->QSLootRecords(client->CharacterID(), corpse_name, "ITEM", client->GetZoneID(), item->ID, item->Name, inst->GetCharges(), GetPlatinum(), GetGold(), GetSilver(), GetCopper());
	}

	safe_delete(inst);
}

void Corpse::EndLoot(Client* client, const EQApplicationPacket* app)
{
	if (!client)
	{
		Save();
		return;
	}

	auto outapp = new EQApplicationPacket;
	outapp->SetOpcode(OP_LootComplete);
	outapp->size = 0;
	client->QueuePacket(outapp);
	safe_delete(outapp);

	ResetLooter();
	if (this->ContainsLegacyItem()) {
		RemoveLegacyItemLooter(client->GetName());
	}
	if (this->IsEmpty())
	{
		Delete();
	}
	else
	{
		Save();
	}
}

void Corpse::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho) {
	Mob::FillSpawnStruct(ns, ForWho);

	ns->spawn.max_hp = 120;

	if (IsPlayerCorpse())
		ns->spawn.NPC = 3;
	else
		ns->spawn.NPC = 2;

	UpdateActiveLight();
	ns->spawn.light = m_Light.Type[EQ::lightsource::LightActive];
}

void Corpse::QueryLoot(Client* to) {
	if (itemlist.size() > 0) {
		int player_corpse_limit = EQ::inventory::Lookup(EQ::versions::ConvertClientVersionToMobVersion(to->ClientVersion()))->InventoryTypeSize[EQ::invtype::typeCorpse];
		to->Message(
			Chat::White,
			fmt::format(
				"Loot | Name: {} ID: {}",
				GetName(),
				GetNPCTypeID()
			).c_str()
		);

		int item_count = 0;
		for (auto current_item : itemlist) {
			int item_number = (item_count + 1);
			if (!current_item) {
				LogError("Corpse::QueryLoot() - ItemList error, null item.");
				continue;
			}

			if (!current_item->item_id || !database.GetItem(current_item->item_id)) {
				LogError("Corpse::QueryLoot() - Database error, invalid item.");
				continue;
			}

			EQ::SayLinkEngine linker;
			linker.SetLinkType(EQ::saylink::SayLinkLootItem);
			linker.SetLootData(current_item);

			to->Message(
				Chat::White,
				fmt::format(
					"Item {} | Name: {} ({}){}",
					item_number,
					linker.GenerateLink().c_str(),
					current_item->item_id,
					(
						current_item->charges > 1 ?
						fmt::format(
							" Amount: {}",
							current_item->charges
						) :
						""
						)
				).c_str()
			);
			item_count++;
		}
	}

	bool has_money = (
		platinum > 0 ||
		gold > 0 ||
		silver > 0 ||
		copper > 0
		);
	if (has_money) {
		to->Message(
			Chat::White,
			fmt::format(
				"Money | {}",
				Strings::Money(
					platinum,
					gold,
					silver,
					copper
				)
			).c_str()
		);
	}
}

bool Corpse::Summon(Client* client, bool spell, bool CheckDistance) {
	float dist2 = 2500.0f; // /corpse range was likely 50 units before 2004
	float z_dist = 1000.0f; // a Z check was very likely implemented shortly after Velious launch, at least for some zones
	switch (GetZoneID())
	{
	case Zones::THURGADINB:
	case Zones::VELKETOR:
	case Zones::CRYSTAL:
	case Zones::NECROPOLIS:
	case Zones::VEXTHAL:
	case Zones::SSERU:
	case Zones::SSRATEMPLE:
	case Zones::POTORMENT:
	case Zones::BOTHUNDER:
	case Zones::SOLROTOWER:
	case Zones::POAIR:
		z_dist = 150.0f;
	}

	// check GM lock
	if (IsLocked() && client->Admin() < AccountStatus::GMAdmin) {
		client->Message(Chat::Red, "That corpse is locked by a GM.");
		return false;
	}

	// check consent if not own corpse
	if (this->GetCharID() != client->CharacterID())
	{
		bool consented = spell; // Conjure Corpse does not require consent.  Target restrictions are checked in spell code, not here.
		std::list<CharacterConsent>::iterator itr;
		for (itr = client->consent_list.begin(); itr != client->consent_list.end(); ++itr)
		{
			if (strcmp(this->GetOwnerName(), itr->consenter.c_str()) == 0 && itr->corpse_id == GetCorpseDBID())
			{
				consented = true;
				break;
			}
		}
		if (!consented) {
			client->Message_StringID(Chat::White, CONSENT_NONE);
			return false;
		}
	}

	// check range if not spell
	if (!spell && CheckDistance)
	{
		bool in_range = DistanceSquaredNoZ(m_Position, client->GetPosition()) <= dist2 && fabs(m_Position.z - client->GetPosition().z) < z_dist;
		if (!in_range)
		{
			client->Message_StringID(Chat::White, CORPSE_TOO_FAR);
			return false;
		}
	}

	// move corpse
	client->CorpseSummoned(this);
	GMMove(client->GetX(), client->GetY(), client->GetZ());
	is_corpse_changed = true;
	Save();

	return true;
}

void Corpse::CompleteResurrection(bool timer_expired)
{
	if(timer_expired)
	{
		rez_time = 0;
		rezzable = false; // Players can no longer rez this corpse.
		corpse_rez_timer.Disable();
	}
	else
	{
		rez_time = corpse_rez_timer.Enabled () ? corpse_rez_timer.GetRemainingTime() : 0;
	}

	IsRezzed(true); // Players can rez this corpse for no XP (corpse gate) provided rezzable is true.
	rez_experience = 0;
	is_corpse_changed = true;
	this->Save();
}

void Corpse::Spawn() {
	EQApplicationPacket app;
	this->CreateSpawnPacket(&app, this);
	entity_list.QueueClients(this, &app);
	safe_delete_array(app.pBuffer);
}

uint32 Corpse::GetEquipment(uint8 material_slot) const {
	int invslot;

	if(material_slot > EQ::textures::LastTexture) {
		return 0;
	}

	invslot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);
	if (invslot == INVALID_INDEX) // GetWornItem() should be returning a NO_ITEM for any invalid index...
		return 0;

	return GetWornItem(invslot);
}

uint32 Corpse::GetEquipmentColor(uint8 material_slot) const {
	const EQ::ItemData *item = nullptr;

	if(material_slot > EQ::textures::LastTexture) {
		return 0;
	}

	item = database.GetItem(GetEquipment(material_slot));
	if(item != 0) {
		return item_tint.Slot[material_slot].UseTint ?
			item_tint.Slot[material_slot].Color :
			item->Color;
	}

	return 0;
}

void Corpse::UpdateEquipmentLight()
{
	m_Light.Type[EQ::lightsource::LightEquipment] = 0;
	m_Light.Level[EQ::lightsource::LightEquipment] = 0;

	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter) {
		if (((*iter)->equip_slot < EQ::invslot::EQUIPMENT_BEGIN || (*iter)->equip_slot > EQ::invslot::EQUIPMENT_END)) { continue; }
		
		auto item = database.GetItem((*iter)->item_id);
		if (item == nullptr) { continue; }
		
		if (EQ::lightsource::IsLevelGreater(item->Light, m_Light.Type[EQ::lightsource::LightEquipment]))
			m_Light.Type[EQ::lightsource::LightEquipment] = item->Light;
	}
	
	uint8 general_light_type = 0;
	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter) {
		if ((*iter)->equip_slot < EQ::invslot::GENERAL_BEGIN || (*iter)->equip_slot > EQ::invslot::GENERAL_END) { continue; }
		
		auto item = database.GetItem((*iter)->item_id);
		if (item == nullptr) { continue; }
		
		if (item->ItemClass != EQ::item::ItemClassCommon) { continue; }
		if (m_Light.Type[EQ::lightsource::LightEquipment] != 0 && (item->Light < 9 || item->Light > 13)) { continue; }

		if (EQ::lightsource::TypeToLevel(item->Light))
			general_light_type = item->Light;
	}

	if (EQ::lightsource::IsLevelGreater(general_light_type, m_Light.Type[EQ::lightsource::LightEquipment]))
		m_Light.Type[EQ::lightsource::LightEquipment] = general_light_type;

	m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
}

void Corpse::AddLooter(Mob* who) {
	if (!who)
		return;

	if (allowed_looters.find(who->CastToClient()->GetCleanName()) == allowed_looters.end())
		allowed_looters.emplace(who->CastToClient()->GetCleanName());
}

void Corpse::LoadPlayerCorpseDecayTime(uint32 corpse_db_id, bool empty){
	if(!corpse_db_id)
		return;

	corpse_graveyard_moved_lastpos = glm::vec4();
	uint32 active_corpse_decay_timer = database.GetCharacterCorpseDecayTimer(corpse_db_id);
	int32 corpse_decay;
	if(empty)
	{
		corpse_decay = RuleI(Character, EmptyCorpseDecayTimeMS);
	}
	else
	{
		corpse_decay = RuleI(Character, CorpseDecayTimeMS);
	}

	if (active_corpse_decay_timer > 0 && corpse_decay > (active_corpse_decay_timer * 1000)) {
		corpse_decay_timer.Start(corpse_decay - (active_corpse_decay_timer * 1000));
	}
	else {
		corpse_decay_timer.Start(2000);
	}
	if (active_corpse_decay_timer > 0 && (zone->graveyard_timer() * 60000) > (active_corpse_decay_timer * 1000)) {
		corpse_graveyard_timer.Start((zone->graveyard_timer() * 60000) - (active_corpse_decay_timer * 1000));
	}
	else {
		corpse_graveyard_timer.Start(3000);
	}
}

void Corpse::SetRezTimer(bool initial_timer)
{

	if(!rezzable)
	{
		if(corpse_rez_timer.Enabled())
			corpse_rez_timer.Disable();

		return;
	}

	IsOwnerOnline();
	if(!is_owner_online && !initial_timer)
	{
		if(corpse_rez_timer.Enabled())
			corpse_rez_timer.Disable();

		return;
    }

	if(corpse_rez_timer.Enabled() && !initial_timer)
	{
		return;
	}

	if(initial_timer)
	{
		uint32 timer = RuleI(Character, CorpseResTimeMS);
		if(killedby == Killed_DUEL)
		{
			timer = RuleI(Character, DuelCorpseResTimeMS);
		}
		else if (IsEmpty() && RuleB(Character, SacrificeCorpseDepop) && killedby == Killed_Sac &&
			(GetZoneID() == Zones::POKNOWLEDGE || GetZoneID() == Zones::NEXUS || GetZoneID() == Zones::BAZAAR))
		{
			if (corpse_rez_timer.Enabled())
				corpse_rez_timer.Disable();

			return;
		}
		rez_time = timer;
	}

	if(rez_time < 1)
	{
		// Corpse is no longer rezzable
		CompleteResurrection(true);
		return;
	}

	corpse_rez_timer.Start(rez_time);

}

void Corpse::IsOwnerOnline()
{
	Client* client = entity_list.GetClientByCharID(GetCharID());

	if(!client)
	{
		uint32 accountid = database.GetAccountIDByChar(GetCharID());
		client = entity_list.GetClientByAccID(accountid);

		if(!client)
		{
			// Client is not in the corpse's zone, send a packet to world to have it check.
			auto pack = new ServerPacket(ServerOP_IsOwnerOnline, sizeof(ServerIsOwnerOnline_Struct));
			ServerIsOwnerOnline_Struct* online = (ServerIsOwnerOnline_Struct*)pack->pBuffer;
			strncpy(online->name, GetOwnerName(), 64);
			online->corpseid = this->GetID();
			online->zoneid = zone->GetZoneID();
			online->zoneguildid = zone->GetGuildID();
			online->online = 0;
			online->accountid = accountid;
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
		else
		{
			SetOwnerOnline(true);
		}
	}
	else
	{
		SetOwnerOnline(true);
	}
}

void Corpse::RevokeConsent()
{
	if (IsPlayerCorpse())
	{
		Log(Logs::General, Logs::Corpse, "%s is denying consent to all players with corpse_id %u", GetOwnerName(), GetCorpseDBID());

		auto pack = new ServerPacket(ServerOP_ConsentDenyByID, sizeof(ServerOP_ConsentDenyByID_Struct));
		ServerOP_ConsentDenyByID_Struct* scs = (ServerOP_ConsentDenyByID_Struct*)pack->pBuffer;
		strncpy(scs->ownername, GetOwnerName(), 64);
		scs->corpse_id = GetCorpseDBID();
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Corpse::AddLegacyItemLooter(std::string client_name)
{
	auto itr = legacy_item_looter_client_name_set.find(client_name);

	if (itr == legacy_item_looter_client_name_set.end())
	{
		legacy_item_looter_client_name_set.insert(client_name);
	}
}

void Corpse::RemoveLegacyItemLooter(std::string client_name)
{
	auto itr = legacy_item_looter_client_name_set.find(client_name);
	
	if (itr != legacy_item_looter_client_name_set.end())
	{
		legacy_item_looter_client_name_set.erase(client_name);
	}
}

void Corpse::ProcessLootLockouts(Client* give_exp_client, NPC* in_npc)
{
	if (loot_lockout_timer == 0)
		return;

	if (zone->GetGuildID() == GUILD_NONE)
		return;

	auto cur_time = time(nullptr);
	auto records = in_npc->GetEngagementRecords();
	if (give_exp_client)
	{

		if (give_exp_client->IsGrouped())
		{
			Group* kg = give_exp_client->GetGroup();
			for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {

				Mob* mclient = kg->members[i];
				Client* c = mclient && mclient->IsClient() ? mclient->CastToClient() : nullptr;
				if (kg->membername[i][0]) { // If Group Member is Active

					std::list<LootItem*> itemlist;

					auto playerItr = records.find(kg->membername[i]);

					if (playerItr == records.end())
						continue;

					bool noLockouts = !playerItr->second.HasLockout(cur_time);

					if (noLockouts)
					{
						LootLockout lootLockout;
						memset(&lootLockout, 0, sizeof(LootLockout));
						bool bIsAALockout = false;

						lootLockout.character_id = playerItr->second.character_id;
						lootLockout.expirydate = cur_time + loot_lockout_timer;
						lootLockout.npctype_id = in_npc->GetNPCTypeID();
						strncpy(lootLockout.npc_name, in_npc->GetCleanName(), 64);

						if (mclient && mclient->IsClient())
						{
							auto clientLootLockoutItr = mclient->CastToClient()->loot_lockouts.find(in_npc->GetNPCTypeID());
							if (clientLootLockoutItr != mclient->CastToClient()->loot_lockouts.end())
							{
								clientLootLockoutItr->second = lootLockout;
							}
							else
							{
								mclient->CastToClient()->loot_lockouts.emplace(in_npc->GetNPCTypeID(), lootLockout);
							}
						}

						std::string message = "You have incurred a lockout for ";
						message += in_npc->GetCleanName();
						message += " that expires in ";
						message += Strings::SecondsToTime(loot_lockout_timer).c_str();
						message += ".";

						if (mclient && mclient->IsClient())
							mclient->CastToClient()->Message(Chat::Yellow, message.c_str());
						else
						{


							uint32 message_len = strlen(kg->membername[i]) + 1;
							uint32 message_len2 = strlen(message.c_str()) + 1;
							auto pack = new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
							CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*)pack->pBuffer;
							CZSC->Type = Chat::Yellow;
							strn0cpy(CZSC->CharName, kg->membername[i], 64);
							strn0cpy(CZSC->Message, message.c_str(), 512);
							worldserver.SendPacket(pack);
							safe_delete(pack);
						}
						//if they're not in zone, this will be loaded once they are.
						database.SaveCharacterLootLockout(playerItr->second.character_id, lootLockout.expirydate, in_npc->GetNPCTypeID(), in_npc->GetCleanName());

						std::string appendedCharName = kg->membername[i];

						if (playerItr->second.isSelfFound)
							appendedCharName += "-SF";

						if (playerItr->second.isSoloOnly)
							appendedCharName += "-Solo";

						temporarily_allowed_looters.emplace(appendedCharName);
						records.erase(playerItr);
					}
					else
					{
						if (mclient && mclient->IsClient())
							mclient->CastToClient()->Message(Chat::Yellow, "You were locked out of %s and receive no loot.", in_npc->GetCleanName());
						else
						{
							std::string message = "You were locked out of ";
							message += in_npc->GetCleanName();
							message += " and receive no loot.";


							uint32 message_len = strlen(kg->membername[i]) + 1;
							uint32 message_len2 = strlen(message.c_str()) + 1;
							auto pack = new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
							CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*)pack->pBuffer;
							CZSC->Type = Chat::Yellow;
							strn0cpy(CZSC->CharName, kg->membername[i], 64);
							strn0cpy(CZSC->Message, message.c_str(), 512);
							worldserver.SendPacket(pack);
							safe_delete(pack);
						}

						DenyPlayerLoot(kg->membername[i]);
						records.erase(playerItr);
					}
				}
			}
		}
		else if (give_exp_client->IsRaidGrouped())
		{
			Raid* kr = give_exp_client->GetRaid();
			if(kr)
			{
				for (uint32 i = 0; i < MAX_RAID_MEMBERS; i++)
				{
					Client* mclient = kr->members[i].member;

					if (kr->members[i].membername[0])
					{
						auto playerItr = records.find(kr->members[i].membername);

						if (playerItr == records.end())
							continue;

						bool noLockouts = !playerItr->second.HasLockout(cur_time);

						if (noLockouts)
						{
							LootLockout lootLockout;
							memset(&lootLockout, 0, sizeof(LootLockout));

							bool bIsAALockout = false;

							lootLockout.character_id = playerItr->second.character_id;
							lootLockout.expirydate = cur_time + loot_lockout_timer;
							lootLockout.npctype_id = in_npc->GetNPCTypeID();
							strncpy(lootLockout.npc_name, in_npc->GetCleanName(), 64);

							if (mclient && mclient->IsClient())
							{
								auto clientLootLockoutItr = mclient->CastToClient()->loot_lockouts.find(in_npc->GetNPCTypeID());
								if (clientLootLockoutItr != mclient->CastToClient()->loot_lockouts.end())
								{
									clientLootLockoutItr->second = lootLockout;
								}
								else
								{
									mclient->CastToClient()->loot_lockouts.emplace(in_npc->GetNPCTypeID(), lootLockout);
								}
							}


							std::string message = "You have incurred a lockout for ";
							message += in_npc->GetCleanName();
							message += " that expires in ";
							message += Strings::SecondsToTime(loot_lockout_timer).c_str();
							message += ".";

							if (mclient && mclient->IsClient())
								mclient->CastToClient()->Message(Chat::Yellow, message.c_str());
							else
							{


								uint32 message_len = strlen(kr->members[i].membername) + 1;
								uint32 message_len2 = strlen(message.c_str()) + 1;
								auto pack = new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
								CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*)pack->pBuffer;
								CZSC->Type = Chat::Yellow;
								strn0cpy(CZSC->CharName, kr->members[i].membername, 64);
								strn0cpy(CZSC->Message, message.c_str(), 512);
								worldserver.SendPacket(pack);
								safe_delete(pack);
							}

							std::string appendedCharName = kr->members[i].membername;

							if (playerItr->second.isSelfFound)
								appendedCharName += "-SF";

							if (playerItr->second.isSoloOnly)
								appendedCharName += "-Solo";

							temporarily_allowed_looters.emplace(appendedCharName);


							//if they're not in zone, this will be loaded once they are.
							database.SaveCharacterLootLockout(playerItr->second.character_id, lootLockout.expirydate, in_npc->GetNPCTypeID(), in_npc->GetCleanName());
							records.erase(playerItr);
						}
						else
						{
							if (mclient && mclient->IsClient())
								mclient->CastToClient()->Message(Chat::Yellow, "You were locked out of %s and receive no loot.", in_npc->GetCleanName());
							else
							{
								std::string message = "You were locked out of ";
								message += in_npc->GetCleanName();
								message += " and receive no loot.";

								uint32 message_len = strlen(kr->members[i].membername) + 1;
								uint32 message_len2 = strlen(message.c_str()) + 1;
								auto pack = new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
								CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*)pack->pBuffer;
								CZSC->Type = Chat::Yellow;
								strn0cpy(CZSC->CharName, kr->members[i].membername, 64);
								strn0cpy(CZSC->Message, message.c_str(), 512);
								worldserver.SendPacket(pack);
								safe_delete(pack);
							}

							DenyPlayerLoot(kr->members[i].membername);
							records.erase(playerItr);
						}
					}
				}
			}
		}
		else if (give_exp_client)
		{
			std::list<LootItem*> itemlist;
			auto playerItr = records.find(give_exp_client->GetCleanName());

			if (playerItr != records.end())
			{

				bool noLockouts = !playerItr->second.HasLockout(cur_time);

				if (noLockouts)
				{

					LootLockout lootLockout;
					memset(&lootLockout, 0, sizeof(LootLockout));

					lootLockout.character_id = playerItr->second.character_id;
					lootLockout.expirydate = cur_time + loot_lockout_timer;
					lootLockout.npctype_id = in_npc->GetNPCTypeID();
					strncpy(lootLockout.npc_name, in_npc->GetCleanName(), 64);


					std::string message = "You have incurred a lockout for ";
					message += in_npc->GetCleanName();
					message += " that expires in ";
					message += Strings::SecondsToTime(loot_lockout_timer).c_str();
					message += ".";


					if (give_exp_client && give_exp_client->IsClient())
					{
						auto clientLootLockoutItr = give_exp_client->CastToClient()->loot_lockouts.find(in_npc->GetNPCTypeID());
						if (clientLootLockoutItr != give_exp_client->CastToClient()->loot_lockouts.end())
						{
							clientLootLockoutItr->second = lootLockout;
						}
						else
						{
							give_exp_client->CastToClient()->loot_lockouts.emplace(in_npc->GetNPCTypeID(), lootLockout);
						}
					}

					if (give_exp_client)
						give_exp_client->Message(Chat::Yellow, message.c_str());

					//if they're not in zone, this will be loaded once they are.
					database.SaveCharacterLootLockout(playerItr->second.character_id, lootLockout.expirydate, in_npc->GetNPCTypeID(), in_npc->GetCleanName());

					std::string appendedCharName = give_exp_client->GetCleanName();

					if (playerItr->second.isSelfFound)
						appendedCharName += "-SF";

					if (playerItr->second.isSoloOnly)
						appendedCharName += "-Solo";


					temporarily_allowed_looters.emplace(appendedCharName);
					records.erase(playerItr);
				}
				else
				{
					give_exp_client->Message(Chat::Yellow, "You were locked out of %s and receive no standard loot.", in_npc->GetCleanName());
					DenyPlayerLoot(give_exp_client->GetCleanName());
					records.erase(playerItr);

				}
			}
		}
	}

	auto record_remainder = in_npc->GetEngagementRecords();
	for (auto record : record_remainder)
	{
		bool noLockouts = !record.second.HasLockout(cur_time);

		if (noLockouts)
		{
			LootLockout lootLockout;
			memset(&lootLockout, 0, sizeof(LootLockout));

			lootLockout.character_id = record.second.character_id;
			lootLockout.expirydate = cur_time + loot_lockout_timer;
			lootLockout.npctype_id = in_npc->GetNPCTypeID();
			strncpy(lootLockout.npc_name, in_npc->GetCleanName(), 64);

			std::string message = "You have incurred a lockout for ";
			message += in_npc->GetCleanName();
			message += " that expires in ";
			message += Strings::SecondsToTime(loot_lockout_timer).c_str();
			message += ".";
			Client* c = entity_list.GetClientByCharID(record.second.character_id);
			if (c && c->IsClient())
				c->CastToClient()->Message(Chat::Yellow, message.c_str());
			else
			{


				uint32 message_len = strlen(record.second.character_name) + 1;
				uint32 message_len2 = strlen(message.c_str()) + 1;
				auto pack = new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
				CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*)pack->pBuffer;
				CZSC->Type = Chat::Yellow;
				strn0cpy(CZSC->CharName, record.second.character_name, 64);
				strn0cpy(CZSC->Message, message.c_str(), 512);
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
			//if they're not in zone, this will be loaded once they are.
			database.SaveCharacterLootLockout(record.second.character_id, lootLockout.expirydate, in_npc->GetNPCTypeID(), in_npc->GetCleanName());
			
			if (c)
			{
				auto clientLootLockoutItr = c->loot_lockouts.find(in_npc->GetNPCTypeID());
				if (clientLootLockoutItr != c->loot_lockouts.end())
				{
					clientLootLockoutItr->second = lootLockout;
				}
				else
				{
					c->loot_lockouts.emplace(in_npc->GetNPCTypeID(), lootLockout);
				}
			}
			
			std::string appendedCharName = record.second.character_name;

			if (record.second.isSelfFound)
				appendedCharName += "-SF";

			if (record.second.isSoloOnly)
				appendedCharName += "-Solo";

			temporarily_allowed_looters.emplace(appendedCharName);
		}
		else
		{

			std::string appendedCharName = record.second.character_name;
			std::string message = "You were locked out of ";
			message += in_npc->GetCleanName();
			message += " and receive no loot.";

			uint32 message_len = strlen(appendedCharName.c_str()) + 1;
			uint32 message_len2 = strlen(message.c_str()) + 1;
			auto pack = new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
			CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*)pack->pBuffer;
			CZSC->Type = Chat::Yellow;
			strn0cpy(CZSC->CharName, appendedCharName.c_str(), 64);
			strn0cpy(CZSC->Message, message.c_str(), 512);
			worldserver.SendPacket(pack);
			safe_delete(pack);

			DenyPlayerLoot(appendedCharName.c_str());
		}
	}
}

void Corpse::AddPlayerLockout(Client* c)
{
	if (npctype_id == 0)
		return;

	if (loot_lockout_timer == 0)
		return;

	if (!c)
		return;

	bool noLockouts = !c->IsLootLockedOutOfNPC(npctype_id);

	if (noLockouts)
	{
		LootLockout lootLockout;
		memset(&lootLockout, 0, sizeof(LootLockout));

		bool bIsAALockout = false;

		lootLockout.character_id = c->CharacterID();
		lootLockout.expirydate = Timer::GetTimeSeconds() + loot_lockout_timer;
		lootLockout.npctype_id = npctype_id;
		strncpy(lootLockout.npc_name, GetCleanName(), 64);

		auto clientLootLockoutItr = c->loot_lockouts.find(npctype_id);
		if (clientLootLockoutItr != c->loot_lockouts.end())
		{
			clientLootLockoutItr->second = lootLockout;
		}
		else
		{
			c->loot_lockouts.emplace(npctype_id, lootLockout);
		}


		std::string message = "You have incurred a lockout for ";
		message += GetCleanNPCName().c_str();
		message += " that expires in ";
		message += Strings::SecondsToTime(loot_lockout_timer).c_str();
		message += ".";

		c->CastToClient()->Message(Chat::Yellow, message.c_str());

		//if they're not in zone, this will be loaded once they are.
		database.SaveCharacterLootLockout(c->CharacterID(), lootLockout.expirydate, npctype_id, GetCleanNPCName().c_str());

		std::string appendedCharName = c->GetCleanName();

		if (c->IsSelfFound())
			appendedCharName += "-SF";

		if (c->IsSoloOnly())
			appendedCharName += "-Solo";


		temporarily_allowed_looters.emplace(appendedCharName);
	}
}
