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

#include "../common/global_define.h"
#include "../common/strings.h"

#include "client.h"
#include "entity.h"
#include "mob.h"
#include "object.h"

#include "quest_parser_collection.h"
#include "zonedb.h"
#include "string_ids.h"
#include "queryserv.h"
#include "../common/repositories/criteria/content_filter_criteria.h"

#include <iostream>

const char DEFAULT_OBJECT_NAME[] = "IT63_ACTORDEF";
const char DEFAULT_OBJECT_NAME_SUFFIX[] = "_ACTORDEF";


extern Zone* zone;
extern EntityList entity_list;
extern QueryServ* QServ;

// Loading object from database
Object::Object(uint32 id, uint32 type, uint32 icon, const Object_Struct& object, const EQ::ItemInstance* inst)
 : respawn_timer(0), decay_timer(RuleI(Groundspawns, DecayTime)), random_timer(0)
{

	user = 0;
	last_user = 0;
	m_character_id = 0;
	m_ssf_ruleset = false;
   
	// Initialize members
	m_id = id;
	m_type = type;
	m_icon = icon;
	m_inuse = false;
	m_inst = nullptr;
	m_ground_spawn=false;
	m_is_player_drop = false;
	// Copy object data
	memcpy(&m_data, &object, sizeof(Object_Struct));
	if (inst) {
		m_inst = inst->Clone();
		decay_timer.Start();
	} else {
		decay_timer.Disable();
	}
	respawn_timer.Disable();
	random_timer.Disable();

	// Set drop_id to zero - it will be set when added to zone with SetID()
	m_data.drop_id = 0;
}

//creating a re-ocurring ground spawn.
Object::Object(const EQ::ItemInstance* inst, char* name,float max_x,float min_x,float max_y,float min_y,float z,float heading,uint32 respawntimer)
 : respawn_timer(respawntimer), decay_timer(RuleI(Groundspawns, DecayTime)), random_timer(respawntimer)
{
	m_is_player_drop = false;

	user = 0;
	last_user = 0;
	m_character_id = 0;
	m_ssf_ruleset = false;
	m_max_x=max_x;
	m_max_y=max_y;
	m_min_x=min_x;
	m_min_y=min_y;
	m_id	= 0;
	m_inst	= (inst) ? inst->Clone() : nullptr;
	m_type	= OT_DROPPEDITEM;
	m_icon	= 0;
	m_inuse	= false;
	m_ground_spawn = true;
	decay_timer.Disable();
	// Set as much struct data as we can
	memset(&m_data, 0, sizeof(Object_Struct));
	m_data.heading = heading;
	m_data.z = z;
	m_data.zone_id = zone->GetZoneID();
	respawn_timer.Disable();
	if(!RuleB(Groundspawns, RandomSpawn) || m_min_x == m_max_x || m_min_y == m_max_y)
		random_timer.Disable();
	else
		random_timer.Start();
	strcpy(m_data.object_name, name);
	RandomSpawn(false);

	// Hardcoded portion for unknown members
	m_data.unknown208 = 0xFFFFFFFF;
	m_data.unknown216 = 0xFFFF;
}

// Loading object from client dropping item on ground
Object::Object(Client* client, const EQ::ItemInstance* inst)
 : respawn_timer(0), decay_timer(RuleI(Groundspawns, DecayTime)), random_timer(0)
{
	user = 0;
	last_user = 0;
	m_character_id = 0;
	m_ssf_ruleset = false;

	// Initialize members
	m_id	= 0;
	m_inst	= (inst) ? inst->Clone() : nullptr;
	m_type	= OT_DROPPEDITEM;
	m_icon	= 0;
	m_inuse	= false;
	m_ground_spawn = false;
	m_is_player_drop = true;
	// Set as much struct data as we can
	memset(&m_data, 0, sizeof(Object_Struct));
	m_data.heading = client->GetHeading();
	m_data.x = client->GetX();
	m_data.y = client->GetY();
	m_data.z = client->GetZ();
	m_data.zone_id = zone->GetZoneID();

	decay_timer.Start();
	respawn_timer.Disable();
	random_timer.Disable();

	// Hardcoded portion for unknown members
	m_data.unknown208 = 0xFFFFFFFF;
	m_data.unknown216 = 0xFFFF;

	// Set object name
	if (inst) {
		const EQ::ItemData* item = inst->GetItem();
		if (item) {
			if (item->IDFile[0] == '\0') {
				strcpy(m_data.object_name, DEFAULT_OBJECT_NAME);
			}
			else {
				// Object name is idfile + _ACTORDEF
				uint32 len_idfile = strlen(inst->GetItem()->IDFile);
				uint32 len_copy = sizeof(m_data.object_name) - len_idfile - 1;
				if (len_copy > sizeof(DEFAULT_OBJECT_NAME_SUFFIX)) {
					len_copy = sizeof(DEFAULT_OBJECT_NAME_SUFFIX);
				}

				memcpy(&m_data.object_name[0], inst->GetItem()->IDFile, len_idfile);
				memcpy(&m_data.object_name[len_idfile], DEFAULT_OBJECT_NAME_SUFFIX, len_copy);
			}
		}
		else {
			strcpy(m_data.object_name, DEFAULT_OBJECT_NAME);
		}
	}
}

Object::Object(const EQ::ItemInstance *inst, float x, float y, float z, float heading, uint32 decay_time, bool is_player_drop, Client* client)
 : respawn_timer(0), decay_timer(decay_time), random_timer(0)
{
	if (is_player_drop && client) {
		m_character_id = client->CharacterID();
		m_ssf_ruleset = client->IsSoloOnly() || client->IsSelfFound();
	} else {
		m_character_id = 0;
		m_ssf_ruleset = false;
	}
	user = 0;
	last_user = 0;

	// Initialize members
	m_id	= 0;
	m_inst	= (inst) ? inst->Clone() : nullptr;
	m_type	= OT_DROPPEDITEM;
	m_icon	= 0;
	m_inuse	= false;
	m_is_player_drop = is_player_drop;
	m_ground_spawn = false;
	// Set as much struct data as we can
	memset(&m_data, 0, sizeof(Object_Struct));
	m_data.heading = heading;
	m_data.x = x;
	m_data.y = y;
	m_data.z = z;
	m_data.zone_id = zone->GetZoneID();

	if (decay_time)
		decay_timer.Start();

	respawn_timer.Disable();
	random_timer.Disable();

	// Hardcoded portion for unknown members
	m_data.unknown208 = 0xFFFFFFFF;
	m_data.unknown216 = 0xFFFF;

	// Set object name
	if (inst) {
		const EQ::ItemData* item = inst->GetItem();
		if (item) {
			if (item->IDFile[0] == '\0') {
				strcpy(m_data.object_name, DEFAULT_OBJECT_NAME);
			}
			if (strncmp("IT91", item->IDFile, 4) == 0 || strncmp("IT212", item->IDFile, 5) == 0)
			{
				// ulaks and bone shields can't be clicked to be picked up when on the ground.  
				// this appears to be a TAKP client behavior with the IT91 and IT212 models so this is a hack to make them appear with a different model for the sake of being able to pick them up.
				strcpy(m_data.object_name, DEFAULT_OBJECT_NAME);
			}
			else {
				// Object name is idfile + _ACTORDEF
				uint32 len_idfile = strlen(inst->GetItem()->IDFile);
				uint32 len_copy = sizeof(m_data.object_name) - len_idfile - 1;
				if (len_copy > sizeof(DEFAULT_OBJECT_NAME_SUFFIX)) {
					len_copy = sizeof(DEFAULT_OBJECT_NAME_SUFFIX);
				}

				memcpy(&m_data.object_name[0], inst->GetItem()->IDFile, len_idfile);
				memcpy(&m_data.object_name[len_idfile], DEFAULT_OBJECT_NAME_SUFFIX, len_copy);
			}
		}
		else {
			strcpy(m_data.object_name, DEFAULT_OBJECT_NAME);
		}
	}
}



Object::Object(const char *model, float x, float y, float z, float heading, uint8 type, uint32 decay_time)
 : respawn_timer(0), decay_timer(decay_time), random_timer(0)
{
	user = 0;
	last_user = 0;
	m_character_id = 0;
   
	m_ssf_ruleset = false;
	EQ::ItemInstance* inst = new EQ::ItemInstance(ItemInstWorldContainer);

	// Initialize members
	m_id	= 0;
	m_inst	= (inst) ? inst->Clone() : nullptr;
	m_type	= type;
	m_icon	= 0;
	m_inuse	= false;
	m_ground_spawn = false;
	m_is_player_drop = false;
	// Set as much struct data as we can
	memset(&m_data, 0, sizeof(Object_Struct));
	m_data.heading = heading;
	m_data.x = x;
	m_data.y = y;
	m_data.z = z;
	m_data.zone_id = zone->GetZoneID();

	if (decay_time)
		decay_timer.Start();

	respawn_timer.Disable();
	random_timer.Disable();

	// Hardcoded portion for unknown members
	m_data.unknown208 = 0xFFFFFFFF;
	m_data.unknown216 = 0xFFFF;

	if(model)
		strcpy(m_data.object_name, model);
	else
		strcpy(m_data.object_name, "IT64_ACTORDEF"); //default object name if model isn't specified for some unknown reason

	safe_delete(inst);
}

Object::~Object()
{
	safe_delete(m_inst);
	if(user != 0) {
		Client* objuser = entity_list.GetClientByCharID(user);
		if(objuser)
			objuser->SetTradeskillObject(nullptr);
	}
}

void Object::SetID(uint16 set_id)
{
	// Invoke base class
	Entity::SetID(set_id);

	// Store new id as drop_id
	m_data.drop_id = (uint32)this->GetID();
}

// Reset state of object back to zero
void Object::ResetState()
{
	safe_delete(m_inst);

	m_id	= 0;
	m_type	= 0;
	m_icon	= 0;
	memset(&m_data, 0, sizeof(Object_Struct));
}

bool Object::Save()
{
	if (m_id) {
		// Update existing
		database.UpdateObject(m_id, m_type, m_icon, m_data, m_inst);
	}
	else {
		// Doesn't yet exist, add now
		m_id = database.AddObject(m_type, m_icon, m_data, m_inst);
	}

	return true;
}

uint16 Object::VarSave()
{
	if (m_id) {
		// Update existing
		database.UpdateObject(m_id, m_type, m_icon, m_data, m_inst);
	}
	else {
		// Doesn't yet exist, add now
		m_id = database.AddObject(m_type, m_icon, m_data, m_inst);
	}
	return m_id;
}

// Remove object from database
void Object::Delete(bool reset_state)
{
	if (m_id != 0) {
		database.DeleteObject(m_id);
	}

	if (reset_state) {
		ResetState();
	}
}

const EQ::ItemInstance* Object::GetItem(uint8 index) {
	if (index < EQ::invtype::WORLD_SIZE) {
		return m_inst->GetItem(index);
	}

	return nullptr;
}

// Add item to object (only logical for world tradeskill containers
void Object::PutItem(uint8 index, const EQ::ItemInstance* inst)
{
	if (index > 9) {
		Log(Logs::General, Logs::Error, "Object::PutItem: Invalid index specified (%i)", index);
		return;
	}

	if (m_inst && m_inst->IsType(EQ::item::ItemClassBag)) {
		if (inst) {
			m_inst->PutItem(index, *inst);
		}
		else {
			m_inst->DeleteItem(index);
		}
		database.SaveWorldContainer(zone->GetZoneID(),m_id,m_inst);
		// This is _highly_ inefficient, but for now it will work: Save entire object to database
		Save();
	}
}

void Object::Close() {
	m_inuse = false;
	if(user != 0)
	{
		last_user = user;
		database.SaveWorldContainer(zone->GetZoneID(),m_id,m_inst);
		Client* objuser = entity_list.GetClientByCharID(user);
		if(objuser)
			objuser->SetTradeskillObject(nullptr);
	}
	user = 0;
}

// Remove item from container
void Object::DeleteItem(uint8 index)
{
	if (m_inst && m_inst->IsType(EQ::item::ItemClassBag)) {
		m_inst->DeleteItem(index);

		// This is _highly_ inefficient, but for now it will work: Save entire object to database
		Save();
	}
}

// Pop item out of container
EQ::ItemInstance* Object::PopItem(uint8 index)
{
	EQ::ItemInstance* inst = nullptr;

	if (m_inst && m_inst->IsType(EQ::item::ItemClassBag)) {
		inst = m_inst->PopItem(index);

		// This is _highly_ inefficient, but for now it will work: Save entire object to database
		Save();
	}

	return inst;
}

void Object::CreateSpawnPacket(EQApplicationPacket* app)
{
	app->SetOpcode(OP_GroundSpawn);
	safe_delete_array(app->pBuffer);
	app->pBuffer = new uchar[sizeof(Object_Struct)];
	app->size = sizeof(Object_Struct);
	Object_Struct* objData = (Object_Struct*)app->pBuffer;
	memcpy(app->pBuffer, &m_data, app->size);
	objData->zone_id = database.GetClientZoneID(objData->zone_id);
}

void Object::CreateDeSpawnPacket(EQApplicationPacket* app)
{
	app->SetOpcode(OP_ClickObject);
	safe_delete_array(app->pBuffer);
	app->pBuffer = new uchar[sizeof(ClickObject_Struct)];
	app->size = sizeof(ClickObject_Struct);
	memset(app->pBuffer, 0, app->size);
	ClickObject_Struct* co = (ClickObject_Struct*) app->pBuffer;
	co->drop_id = m_data.drop_id;
	co->player_id = 0;
}

bool Object::Process(){
	if(m_type == OT_DROPPEDITEM && decay_timer.Enabled() && decay_timer.Check()) {
		// Send click to all clients (removes entity on client)
		auto outapp = new EQApplicationPacket(OP_ClickObject, sizeof(ClickObject_Struct));
		ClickObject_Struct* click_object = (ClickObject_Struct*)outapp->pBuffer;
		click_object->drop_id = GetID();
		entity_list.QueueClients(nullptr, outapp, false);
		safe_delete(outapp);

		// Remove object
		database.DeleteObject(m_id);
		return false;
	}

	if(m_ground_spawn) {
		if (respawn_timer.Enabled()) {
			// respawn_timer is enabled if item was picked up, and waiting to respawn
			if (respawn_timer.Check()){
				// when random_timer is enabled, RandomSpawn() will send a despawn packet
				bool rand_respawn = random_timer.Enabled();
				random_timer.Disable();
				RandomSpawn(true);
				respawn_timer.Disable();
				// re-start random_timer if it was running
				if (rand_respawn)
					random_timer.Start();
			}
		} else if (random_timer.Check()) {
			RandomSpawn(true);
		}
	}

	Client* objuser = entity_list.GetClientByCharID(user);
	if (user != 0 && !objuser) {
		m_inuse = false;
		last_user = user;
		if (objuser)
			objuser->SetTradeskillObject(nullptr);
		user = 0;
	}

	return true;
}

void Object::RandomSpawn(bool send_packet) {
	if(!m_ground_spawn)
		return;

	m_data.x = zone->random.Real(m_min_x, m_max_x);
	m_data.y = zone->random.Real(m_min_y, m_max_y);

	if(send_packet) {
		EQApplicationPacket app;
		if (random_timer.Enabled()) {
			CreateDeSpawnPacket(&app);
			entity_list.QueueClients(nullptr, &app, true);
			safe_delete_array(app.pBuffer);
		}
		CreateSpawnPacket(&app);
		entity_list.QueueClients(nullptr, &app, true);
		safe_delete_array(app.pBuffer);
	}
}

bool Object::HandleClick(Client* sender, const ClickObject_Struct* click_object)
{
	if(m_ground_spawn)
	{
		respawn_timer.Start();
	}
	if (m_type == OT_DROPPEDITEM) 
	{
		if (m_inst && sender) 
		{

			// if there is a lore conflict - don't allow the item to be picked up
			if(sender->CheckLoreConflict(m_inst->GetItem())) 
			{
				sender->Message_StringID(Chat::Red, PICK_LORE);
				auto outapp = new EQApplicationPacket(OP_ClickObject, sizeof(ClickObject_Struct));
				ClickObject_Struct* loreitem = (ClickObject_Struct*)outapp->pBuffer;
				loreitem->player_id = click_object->player_id;
				loreitem->drop_id = 0xFFFFFFFF;
				sender->CastToClient()->QueuePacket(outapp);
				safe_delete(outapp);
				return false;	
			} else if (m_inst->IsType(EQ::item::ItemClassBag)) {
				// We picked up a container, so go through it and check for lore items.
				const EQ::ItemInstance* c_inst = NULL;
				for (int i = 0; i < 10; i++) {
					c_inst = m_inst->GetItem(i);
					if (c_inst && sender->CheckLoreConflict(c_inst->GetItem())) {
						// we found a lore item in container - so do not allow picking up item.
						sender->Message_StringID(Chat::Red, PICK_LORE);
						auto outapp = new EQApplicationPacket(OP_ClickObject, sizeof(ClickObject_Struct));
						ClickObject_Struct* loreitem = (ClickObject_Struct*)outapp->pBuffer;
						loreitem->player_id = click_object->player_id;
						loreitem->drop_id = 0xFFFFFFFF;
						sender->CastToClient()->QueuePacket(outapp);
						safe_delete(outapp);
						return false;
					}
				}
			}

			std::string export_string = fmt::format("{}", m_inst->GetItem()->ID);
			std::vector<std::any> args;
			args.push_back(m_inst);
			parse->EventPlayer(EVENT_PLAYER_PICKUP, sender, export_string, 0, &args);

			int charges = m_inst->GetCharges();
			uint32 item_id = m_inst->GetItem()->ID;
			if(database.ItemQuantityType(item_id) != EQ::item::Quantity_Charges && charges < 1)
				charges = 1;

			if (sender->SummonItem(item_id, charges, EQ::invslot::slotCursor, true, m_inst->GetCustomData()))
			{
				EQ::ItemInstance* curitem = sender->GetInv().GetItem(EQ::invslot::slotCursor);
				if (curitem && curitem->IsType(EQ::item::ItemClassBag))
				{
					database.LoadWorldContainer(m_id, curitem);
				}

				if (RuleB(QueryServ, PlayerLogGroundSpawn) && m_inst)
				{
					QServ->QSGroundSpawn(sender->CharacterID(), m_inst->GetID(), m_inst->GetCharges(), 0, sender->GetZoneID(), false);
					if (m_inst->IsType(EQ::item::ItemClassBag))
					{
						for (uint8 sub_slot = EQ::invbag::SLOT_BEGIN; (sub_slot <= EQ::invbag::SLOT_END); ++sub_slot)
						{
							const EQ::ItemInstance* bag_inst = m_inst->GetItem(sub_slot);
							if (bag_inst)
							{
								sender->PutItemInInventory(EQ::invbag::CURSOR_BAG_BEGIN + sub_slot, *bag_inst, false);
								QServ->QSGroundSpawn(sender->CharacterID(), bag_inst->GetID(), bag_inst->GetCharges(), m_inst->GetID(), sender->GetZoneID(), false);
							}
						}
					}
				}
			}

			if(!m_ground_spawn)
				safe_delete(m_inst);

			// No longer using a tradeskill object
			sender->SetTradeskillObject(nullptr);
			user = 0;
		}

		// Send click to all clients (removes entity on client)
		auto outapp = new EQApplicationPacket(OP_ClickObject, sizeof(ClickObject_Struct));
		memcpy(outapp->pBuffer, click_object, sizeof(ClickObject_Struct));
		entity_list.QueueClients(nullptr, outapp, false);
		safe_delete(outapp);

		// Remove object
		database.DeleteObject(m_id);
		if(!m_ground_spawn)
			entity_list.RemoveEntity(this->GetID());
	} else {
		// Tradeskill item
		auto outapp = new EQApplicationPacket(OP_ClickObjectAction, sizeof(ClickObjectAction_Struct));
		ClickObjectAction_Struct* coa = (ClickObjectAction_Struct*)outapp->pBuffer;

		//TODO: there is prolly a better way to do this.
		coa->type		= m_type;
		coa->slot	= 0x0a;

		coa->drop_id	= click_object->drop_id;
		coa->player_id	= click_object->player_id;
		coa->icon		= m_icon;

		//if this is not the main user, send them a close and a message
		if (user == 0 || user == sender->CharacterID())
		{
			coa->open = 0x01;
		}
		else
		{
			coa->open = 0x00;
		}

		sender->QueuePacket(outapp);
		safe_delete(outapp);

		if (sender->Admin() > 0)
		{
			std::string msg = "You can't interact with a tradeskill container as a GM. Yes. We thought of this one, too.";
			sender->Message(Chat::Red, msg.c_str());
			return false;
		}

		//if the object already had a user, we are done
		if (user != 0)
		{
			return(false);
		}

		// Starting to use this object
		m_inuse = true;
		sender->SetTradeskillObject(this);

		user = sender->CharacterID();

		// Send items inside of container

		if (m_inst && m_inst->IsType(EQ::item::ItemClassBag))
		{
			if (sender->IsSoloOnly())
			{
				m_inst->Clear();
			}
			else if (sender->IsSelfFound())
			{
				m_inst->Clear();
			}
			if (RuleB(AlKabor, NoDropRemoveTradeskill))
			{
				//Clear out no-drop and no-rent items first
				m_inst->ClearByFlags(byFlagSet, byFlagSet);
			}
			else if(user != last_user)
			{
				// if rule is false then Clear out no-drop and no-rent items first if different player opens it
				m_inst->ClearByFlags(byFlagSet, byFlagSet);
			}
			else
			{
				m_inst->ClearByFlags(byFlagIgnore, byFlagSet);
			}

			uint8 i = 0;
			for (i=0; i<10; i++) 
			{
				const EQ::ItemInstance* inst = m_inst->GetItem(i);
				if (inst) 
				{
					sender->SendItemPacket(i, inst, ItemPacketWorldContainer);
				}
			}
		}
	}

	return true;
}

// Add new Zone Object (theoretically only called for items dropped to ground)
uint32 ZoneDatabase::AddObject(uint32 type, uint32 icon, const Object_Struct& object, const EQ::ItemInstance* inst)
{
	uint32 database_id = 0;
	uint32 item_id = 0;
	int16 charges = 0;
	std::string custom_data_str;

	if (inst && inst->GetItem()) {
		item_id = inst->GetItem()->ID;
		charges = inst->GetCharges();
		custom_data_str = Strings::Escape(inst->GetCustomDataString());
	}

	// SQL Escape object_name
	uint32 len = strlen(object.object_name) * 2 + 1;
	auto object_name = new char[len];
	DoEscapeString(object_name, object.object_name, strlen(object.object_name));

	std::string query = StringFormat("SELECT MAX(id) FROM object");
	auto results = QueryDatabase(query);
	if (results.Success()) {
		if (results.RowCount() != 0)
		{
			auto row = results.begin();
			database_id = atoi(row[0]) + 1;
		}
        else
		{
			Log(Logs::Detail, Logs::Error, "Unable to get new object id: %s", results.ErrorMessage().c_str());   
			return 0;
        }
	}

    // Save new record for object
	query = StringFormat("INSERT INTO object "
		"(id, zoneid, xpos, ypos, zpos, heading, "
		"itemid, charges, custom_data, objectname, type, icon) "
		"values (%i, %i, %f, %f, %f, %f, %i, %i, '%s', '%s', %i, %i)",
		database_id, object.zone_id, object.x, object.y, object.z, object.heading,
		item_id, charges, custom_data_str.c_str(), object_name, type, icon);

    safe_delete_array(object_name);
	results = QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::General, Logs::Error, "Unable to insert object: %s", results.ErrorMessage().c_str());
		return 0;
	}

    // Save container contents, if container
	if (inst && inst->IsType(EQ::item::ItemClassBag)) {
		SaveWorldContainer(object.zone_id, database_id, inst);
	}

	return database_id;
}

// Update information about existing object in database
void ZoneDatabase::UpdateObject(uint32 id, uint32 type, uint32 icon, const Object_Struct& object, const EQ::ItemInstance* inst)
{
	uint32 item_id = 0;
	int16 charges = 0;
	std::string custom_data_str;

	if (inst && inst->GetItem()) {
		item_id = inst->GetItem()->ID;
		charges = inst->GetCharges();
		custom_data_str = Strings::Escape(inst->GetCustomDataString());
	}

	// SQL Escape object_name
	uint32 len = strlen(object.object_name) * 2 + 1;
	auto object_name = new char[len];
	DoEscapeString(object_name, object.object_name, strlen(object.object_name));

	// Save new record for object
	std::string query = StringFormat("UPDATE object SET "
                                    "zoneid = %i, xpos = %f, ypos = %f, zpos = %f, heading = %f, "
                                    "itemid = %i, charges = %i, custom_data = '%s', objectname = '%s', "
                                    "type = % i, icon = % i "
                                    "WHERE id = %i",
                                    object.zone_id, object.x, object.y, object.z, object.heading,
                                    item_id, charges, custom_data_str.c_str(), object_name,
                                    type, icon, id);
    safe_delete_array(object_name);
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::General, Logs::Error, "Unable to update object: %s", results.ErrorMessage().c_str());
		return;
	}

    // Save container contents, if container
    if (inst && inst->IsType(EQ::item::ItemClassBag))
        SaveWorldContainer(object.zone_id, id, inst);
}

Ground_Spawns* ZoneDatabase::LoadGroundSpawns(uint32 zone_id, Ground_Spawns* gs) {

	std::string query = StringFormat(
		"SELECT max_x, max_y, max_z, "
        "min_x, min_y, heading, name, "
        "item, max_allowed, respawn_timer "
        "FROM ground_spawns "
        "WHERE zoneid = %i %s"
        "LIMIT 50", 
		zone_id,
		ContentFilterCriteria::apply().c_str()
		);
		
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return gs;
	}

	int spawnIndex=0;
    for (auto row = results.begin(); row != results.end(); ++row, ++spawnIndex) {
        gs->spawn[spawnIndex].max_x=atof(row[0]);
        gs->spawn[spawnIndex].max_y=atof(row[1]);
        gs->spawn[spawnIndex].max_z=atof(row[2]);
        gs->spawn[spawnIndex].min_x=atof(row[3]);
        gs->spawn[spawnIndex].min_y=atof(row[4]);
        gs->spawn[spawnIndex].heading=atof(row[5]);
        strcpy(gs->spawn[spawnIndex].name,row[6]);
        gs->spawn[spawnIndex].item=atoi(row[7]);
        gs->spawn[spawnIndex].max_allowed=atoi(row[8]);
        gs->spawn[spawnIndex].respawntimer=atoi(row[9]);
    }
	return gs;
}

void ZoneDatabase::DeleteObject(uint32 id)
{
	// delete record of object
	std::string query = StringFormat("DELETE FROM object WHERE id = %i", id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::General, Logs::Error, "Unable to delete object: %s", results.ErrorMessage().c_str());
	}
	else
	{
		DeleteWorldContainer(id,zone->GetZoneID());
	}
}

uint32 Object::GetDBID()
{
	return this->m_id;
}

uint32 Object::GetType()
{
	return this->m_type;
}

void Object::SetType(uint32 type)
{
	this->m_type = type;
	this->m_data.object_type = type;
}

uint32 Object::GetIcon()
{
	return this->m_icon;
}

float Object::GetX()
{
	return this->m_data.x;
}

float Object::GetY()
{
	return this->m_data.y;
}


float Object::GetZ()
{
	return this->m_data.z;
}

float Object::GetHeadingData()
{
	return this->m_data.heading;
}

void Object::SetX(float pos)
{
	this->m_data.x = pos;

	EQApplicationPacket app;
	this->CreateDeSpawnPacket(&app);
	entity_list.QueueClients(0, &app);
	safe_delete_array(app.pBuffer);

	this->CreateSpawnPacket(&app);
	entity_list.QueueClients(0, &app);
	safe_delete_array(app.pBuffer);
}

void Object::SetY(float pos)
{
	this->m_data.y = pos;

	EQApplicationPacket app;

	this->CreateDeSpawnPacket(&app);
	entity_list.QueueClients(0, &app);
	safe_delete_array(app.pBuffer);
	
	this->CreateSpawnPacket(&app);
	entity_list.QueueClients(0, &app);
	safe_delete_array(app.pBuffer);
}

void Object::Depop()
{
	auto app = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	entity_list.QueueClients(0, app);
	safe_delete(app);
	entity_list.RemoveObject(this->GetID());
}

void Object::DepopWithTimer()
{
	if (m_inst && !m_ground_spawn)
		safe_delete(m_inst);
	
	auto app = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	entity_list.QueueClients(0, app);
	safe_delete(app);

	database.DeleteObject(m_id);
	if (!m_ground_spawn)
		entity_list.RemoveEntity(this->GetID());
	else
		respawn_timer.Start();
}

void Object::Repop()
{
	auto app = new EQApplicationPacket();
	auto app2 = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	this->CreateSpawnPacket(app2);
	entity_list.QueueClients(0, app);
	entity_list.QueueClients(0, app2);
	safe_delete(app);
	safe_delete(app2);
}



void Object::SetZ(float pos)
{
	this->m_data.z = pos;

	auto app = new EQApplicationPacket();
	auto app2 = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	this->CreateSpawnPacket(app2);
	entity_list.QueueClients(0, app);
	entity_list.QueueClients(0, app2);
	safe_delete(app);
	safe_delete(app2);
}

void Object::SetModelName(const char* modelname)
{
	strn0cpy(m_data.object_name, modelname, sizeof(m_data.object_name)); // 32 is the max for chars in object_name, this should be safe
	auto app = new EQApplicationPacket();
	auto app2 = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	this->CreateSpawnPacket(app2);
	entity_list.QueueClients(0, app);
	entity_list.QueueClients(0, app2);
	safe_delete(app);
	safe_delete(app2);
}

const char* Object::GetModelName()
{
	return this->m_data.object_name;
}

void Object::SetIcon(uint32 icon)
{
	this->m_icon = icon;
}

int16 Object::GetItemID()
{
	if (this->m_inst == 0)
	{
		return 0;
	}

	const EQ::ItemData* item = this->m_inst->GetItem();

	if (item == 0)
	{
		return 0;
	}

	return item->ID;
}

void Object::SetItemID(uint32 itemid)
{
	safe_delete(this->m_inst);

	if (itemid)
	{
		this->m_inst = database.CreateItem(itemid);
	}
}

void Object::GetObjectData(Object_Struct* Data)
{
	if (Data)
	{
		memcpy(Data, &this->m_data, sizeof(this->m_data));
	}
}

void Object::SetObjectData(Object_Struct* Data)
{
	if (Data)
	{
		memcpy(&this->m_data, Data, sizeof(this->m_data));
	}
}

void Object::GetLocation(float* x, float* y, float* z)
{
	if (x)
	{
		*x = this->m_data.x;
	}

	if (y)
	{
		*y = this->m_data.y;
	}

	if (z)
	{
		*z = this->m_data.z;
	}
}

void Object::SetLocation(float x, float y, float z)
{
	this->m_data.x = x;
	this->m_data.y = y;
	this->m_data.z = z;
	auto app = new EQApplicationPacket();
	auto app2 = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	this->CreateSpawnPacket(app2);
	entity_list.QueueClients(0, app);
	entity_list.QueueClients(0, app2);
	safe_delete(app);
	safe_delete(app2);
}

void Object::GetHeading(float* heading)
{
	if (heading)
	{
		*heading = this->m_data.heading;
	}
}

void Object::SetHeading(float heading)
{
	this->m_data.heading = heading;
	auto app = new EQApplicationPacket();
	auto app2 = new EQApplicationPacket();
	this->CreateDeSpawnPacket(app);
	this->CreateSpawnPacket(app2);
	entity_list.QueueClients(0, app);
	entity_list.QueueClients(0, app2);
	safe_delete(app);
	safe_delete(app2);
}

void Object::SetEntityVariable(const char *id, const char *m_var)
{
	std::string n_m_var = m_var;
	o_EntityVariables[id] = n_m_var;
}

const char* Object::GetEntityVariable(const char *id)
{
	if(!id)
		return nullptr;

	auto iter = o_EntityVariables.find(id);
	if(iter != o_EntityVariables.end())
	{
		return iter->second.c_str();
	}
	return nullptr;
}

bool Object::EntityVariableExists(const char * id)
{
	if(!id)
		return false;

	auto iter = o_EntityVariables.find(id);
	if(iter != o_EntityVariables.end())
	{
		return true;
	}
	return false;
}
