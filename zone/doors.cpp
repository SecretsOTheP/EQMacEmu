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
#include "../common/strings.h"
#include "../common/zone_store.h"

#include "client.h"
#include "doors.h"
#include "raids.h"
#include "entity.h"
#include "guild_mgr.h"
#include "mob.h"
#include "string_ids.h"
#include "worldserver.h"
#include "zonedb.h"
#include "../common/repositories/criteria/content_filter_criteria.h"

#include <iostream>
#include <string.h>

#define DOOR_STOP_TRAP 0x00
#define DOOR_RESET_TRAP 0x01
#define OPEN_DOOR 0x02
#define CLOSE_DOOR 0x03
#define OPEN_INVDOOR 0x03
#define CLOSE_INVDOOR 0x02

extern EntityList entity_list;
extern WorldServer worldserver;

Doors::Doors(const DoorsRepository::Doors &door) :
        close_timer(5000),
	    lift_timer(20000),
        m_position(door.pos_x, door.pos_y, door.pos_z, door.heading),
        m_destination(door.dest_x, door.dest_y, door.dest_z, door.dest_heading)
{

	strn0cpy(zone_name, door.zone.c_str(), sizeof(zone_name));
	strn0cpy(door_name, door.name.c_str(), sizeof(door_name));
	strn0cpy(destination_zone_name, door.dest_zone.c_str(), sizeof(destination_zone_name));

	// destination helpers
	if (!door.dest_zone.empty() && Strings::ToLower(door.dest_zone) != "none" && !door.dest_zone.empty()) {
		m_has_destination_zone = true;
	}

	if (!door.dest_zone.empty() && !door.zone.empty() && Strings::EqualFold(door.dest_zone, door.zone)) {
		m_same_destination_zone = true;
	}

	database_id			= door.id;
	door_id             = door.doorid;
	incline             = door.incline;
	open_type           = door.opentype;
	lockpick            = door.lockpick;
	key_item_id         = door.keyitem;
	no_key_ring         = door.nokeyring;
	alt_key_item_id     = door.altkeyitem;
	trigger_door        = door.triggerdoor;
	trigger_type        = door.triggertype;
	triggered           = false;
	door_param          = door.door_param;
	size                = door.size;
	invert_state        = door.invert_state;
	is_lift             = door.islift;
	close_time          = door.close_time;
	can_open            = door.can_open;
	instance_only       = door.instance_only;
	client_version_mask = door.client_version_mask;
	guild_zone_door = door.guild_zone_door;

	SetOpenState(false);

	close_timer.Disable();
	lift_timer.Disable();

	if (HasDestinationZone()) {
		teleport = true;
	} 
	else {
		teleport = false;
	}

}

Doors::Doors(const char *model, const glm::vec4& position, uint8 opentype, uint16 size) :
    close_timer(5000),
	lift_timer(20000),
    m_position(position),
    m_destination(glm::vec4())
{
	strn0cpy(zone_name, zone->GetShortName(), 32);
	strn0cpy(door_name, model, 32);
	strn0cpy(destination_zone_name, "NONE", 32);

	database_id		= database.GetDoorsCountPlusOne(zone->GetShortName());
	door_id	= database.GetDoorsDBCountPlusOne(zone->GetShortName());

	incline		= 0;
	open_type		= opentype;
	lockpick		= 0;
	key_item_id	= 0;
	no_key_ring	= 0;
	alt_key_item_id	= 0;
	trigger_door	= 0;
	trigger_type	= 0;
	triggered		= false;
	door_param	= 0;
	size			= size;
	invert_state	= 0;
	is_lift		= 0;
	close_time	= 0;
	can_open		= 0;
	guild_zone_door = 0;
	client_version_mask = 4294967295u;

	SetOpenState(false);
	teleport = false;
	close_timer.Disable();
	lift_timer.Disable();
}


Doors::~Doors()
{
}

bool Doors::Process()
{
	if(close_timer.Enabled() && close_timer.Check() && IsDoorOpen()) {
		if (open_type == 40 || GetTriggerType() == 1 || close_time != 5) {
			auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
			MoveDoor_Struct* md = (MoveDoor_Struct*)outapp->pBuffer;
			md->doorid = door_id;
			md->action = invert_state == 0 ? CLOSE_DOOR : CLOSE_INVDOOR;
			entity_list.QueueClients(0, outapp);

			if (GetTriggerDoorID() != 0) {
				Doors* triggerdoor = entity_list.FindDoor(GetTriggerDoorID());
				if (triggerdoor && triggerdoor->IsDoorOpen()) {
					md->doorid = triggerdoor->door_id;
					md->action = triggerdoor->invert_state == 0 ? CLOSE_DOOR : CLOSE_INVDOOR;
					entity_list.QueueClients(0, outapp);

					triggerdoor->triggered = false;
					triggerdoor->close_timer.Disable();
					triggerdoor->SetOpenState(false);
				}
			}

			safe_delete(outapp);
		}

		triggered=false;
		close_timer.Disable();
		SetOpenState(false);

	} else if(lift_timer.Check() && IsDoorOpen()) {
		auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
		MoveDoor_Struct* md = (MoveDoor_Struct*)outapp->pBuffer;
		md->doorid = door_id;
		md->action = invert_state == 1 ? CLOSE_INVDOOR : CLOSE_DOOR;
		entity_list.QueueClients(0, outapp);
		safe_delete(outapp);

		triggered = false;
		lift_timer.Disable();
		SetOpenState(false);
		if (door_param > 1) {
			Log(Logs::General, Logs::Doors, "Lift %d is being closed by process.", database_id);
		}
	}

	return true;
}

void Doors::HandleClick(Client* sender, uint8 trigger, bool floor_port)
{
	//door debugging info dump
	Log(Logs::General, Logs::Doors, "%s clicked door %s (dbid %d, eqid %d) at %s", sender->GetName(), this->door_name, this->database_id, this->door_id, to_string(m_position).c_str());
	Log(Logs::Detail, Logs::Doors, "  incline %d, opentype %d, lockpick %d, keys %d %d, nokeyring %d, trigger %d type %d, param %d", this->incline, this->open_type, this->lockpick, this->key_item_id, this->alt_key_item_id, this->no_key_ring, this->trigger_door, this->trigger_type, this->door_param);
	Log(Logs::Detail, Logs::Doors, "  size %d, invert %d, dest: %s %s open %d lift: %d closetime: %d", this->size, this->invert_state, this->destination_zone_name, to_string(m_destination).c_str(), this->is_open, this->is_lift, this->close_time);

	if (!IsMoveable()) {
		Log(Logs::General, Logs::Doors, "%s clicked door %d that doesn't open.", sender->GetName(), door_id);
		return;
	}

	if (is_lift) {
		HandleLift(sender);
		return;
	}

	auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
	auto *move_door_packet = (MoveDoor_Struct*)outapp->pBuffer;
	move_door_packet->doorid = door_id;

	// Traps. 120 is ceiling spears. 125 is wall spears. 130 is swinging axe. 140 is falling block trap. 
	if (open_type == 120 || open_type == 125 || open_type == 130 || open_type == 140) {
		Log(Logs::General, Logs::Doors, "Clicking a door that is a trap!");
		if(sender->HasSkill(EQ::skills::SkillDisarmTraps)) {
			uint8 success = SKILLUP_FAILURE;
			int uskill = sender->GetSkill(EQ::skills::SkillDisarmTraps);
			if ((zone->random.Int(0, 49) + uskill) >= (zone->random.Int(0, 49) + 1)) {
				Log(Logs::General, Logs::Traps, "Door Trap %d is disarmed.", door_id);
				success = SKILLUP_SUCCESS;
				sender->Message_StringID(Chat::Skills, StringID::DISARMED_TRAP);
				move_door_packet->action = DOOR_RESET_TRAP;
			} else {
				sender->Message_StringID(Chat::Skills, StringID::FAILED_DISARM_TRAP);
				sender->CommonBreakInvisible();
			}

			sender->CheckIncreaseSkill(EQ::skills::SkillDisarmTraps, nullptr, zone->skill_difficulty[EQ::skills::SkillDisarmTraps].difficulty[sender->GetClass()], success);
		} else {
			sender->CommonBreakInvisible();
		}

		entity_list.QueueClients(sender, outapp, false);
		safe_delete(outapp);
		return;
	}

	if(GetTriggerType() == 255) { // this object isnt triggered
		if(trigger == 1) { // this door is only triggered by an object
			if(!IsDoorOpen() || (open_type == 58)) {
				move_door_packet->action = invert_state == 0 ? OPEN_DOOR : OPEN_INVDOOR;
			} else {
				move_door_packet->action = invert_state == 0 ? CLOSE_DOOR : CLOSE_INVDOOR;
			}
		} else {
			safe_delete(outapp);
			return;
		}
	}

	uint32 key = 0;
	if (!DoorKeyCheck(sender, key)) {
		safe_delete(outapp);
		return;
	} else {
		if (!IsDoorOpen() || (open_type == 58)) {
			move_door_packet->action = invert_state == 0 ? OPEN_DOOR : OPEN_INVDOOR;
		} else {
			move_door_packet->action = invert_state == 0 ? CLOSE_DOOR : CLOSE_INVDOOR;
		}
	}

	if ((!floor_port && open_type != 58) || !HasDestinationZone()) {
		entity_list.QueueClients(sender, outapp, false);
		if (!IsDoorOpen()) {
			if (close_time > 0)
				close_timer.Start(close_time * 1000);
			SetOpenState(true);
		} else {
			if (close_time > 0)
				close_timer.Disable();
			SetOpenState(false);
		}
	}

	if(GetTriggerDoorID() != 0) {
		Doors* triggerdoor = entity_list.FindDoor(GetTriggerDoorID());
		if (triggerdoor) {
			if (!triggerdoor->triggered) {
				triggered = true;
				uint8 trigger = GetTriggerType() == 1 ? 1 : 0;
				triggerdoor->HandleClick(sender, trigger);
			} else {
				triggered = false;
				triggerdoor->triggered = false;
			}
		}
	}

	//everything past this point assumes we opened the door
	//and met all the reqs for opening
	//everything to do with closed doors has already been taken care of
	//we return because we don't want people using teleports on an unlocked door (exploit!)
	if((move_door_packet->action == CLOSE_DOOR && invert_state == 0) || (move_door_packet->action == CLOSE_INVDOOR && invert_state == 1)) {
		safe_delete(outapp);
		return;
	}

	safe_delete(outapp);

	// Teleport door!
	if((floor_port || open_type == 58) && IsTeleport() && sender && !sender->HasDied()) {

		uint32 keyneeded = GetKeyItem();
		uint32 playerkey = key;
		uint8 keepoffkeyring = GetNoKeyring();
		uint32 zoneid = ZoneID(destination_zone_name);
		float temp_x = m_destination.x;
		float temp_y = m_destination.y;
		uint32 zoneguildid = GUILD_NONE;

		if (zoneid != zone->GetZoneID() && !sender->CanBeInZone(zoneid)) {
			return;
		}

		if (guild_zone_door)
		{

			if (!sender)
				return;

			Raid* player_raid = sender->GetRaid();

			if (!player_raid)
			{
				sender->Message(Chat::Red, "You are unable to enter a guild instance because you are not a part of a raid containing at least a guild officer as its leader with %i guild members present, and %i players at or above level %i present total.",
					RuleI(Quarm, AutomatedRaidRotationRaidGuildMemberCountRequirement),
					RuleI(Quarm, AutomatedRaidRotationRaidNonMemberCountRequirement),
					RuleI(Quarm, AutomatedRaidRotationRaidGuildLevelRequirement));
				return;
			}

			if (!player_raid->CanRaidEngageRaidTarget(player_raid->GetLeaderGuildID()))
			{
				sender->Message(Chat::Red, "You are unable to enter a guild instance because you are not a part of a raid containing at least a guild officer as its leader with %i guild members present, and %i players at or above level %i present total.",
					RuleI(Quarm, AutomatedRaidRotationRaidGuildMemberCountRequirement),
					RuleI(Quarm, AutomatedRaidRotationRaidNonMemberCountRequirement),
					RuleI(Quarm, AutomatedRaidRotationRaidGuildLevelRequirement));
				return;
			}
			zoneguildid = player_raid->GetLeaderGuildID();
		}

		if ((floor_port || strncmp(destination_zone_name,zone_name,strlen(zone_name)) == 0) && !keyneeded)
		{
			if(!keepoffkeyring)
			{
				sender->KeyRingAdd(playerkey);
			}
			sender->MovePCGuildID(zone->GetZoneID(), zoneguildid, m_destination.x, m_destination.y, m_destination.z, m_destination.w);
		}
		else if ((!IsDoorOpen() || open_type == 58 || floor_port) && (keyneeded && ((keyneeded == playerkey) || sender->GetGM())))
		{
			if(!keepoffkeyring)
			{
				sender->KeyRingAdd(playerkey);
			}
			if(zoneid == zone->GetZoneID())
			{
				sender->MovePCGuildID(zone->GetZoneID(), zoneguildid, m_destination.x, m_destination.y, m_destination.z, m_destination.w);
			}
			else
			{
				zone->ApplyRandomLoc(zoneid, temp_x, temp_y);
				sender->MovePCGuildID(zoneid, zoneguildid, temp_x, temp_y, m_destination.z, m_destination.w);
			}
		}

		if ((!IsDoorOpen() || open_type == 58) && !keyneeded)
		{
			if(zoneid == zone->GetZoneID())
			{
				sender->MovePCGuildID(zone->GetZoneID(), zoneguildid, m_destination.x, m_destination.y, m_destination.z, m_destination.w);
			}
			else
			{
				zone->ApplyRandomLoc(zoneid, temp_x, temp_y);
				sender->MovePCGuildID(zoneid, zoneguildid, temp_x, temp_y, m_destination.z, m_destination.w);
			}
		}
	}
}

void Doors::HandleLift(Client* sender)
{
	if (door_param > 1) {
		Log(Logs::General, Logs::Doors, "%s activated lift %s (dbid %d, eqid %d) at %s which moves %d coords.", sender->GetName(), door_name, database_id, door_id, to_string(m_position).c_str(), door_param);
	}

	auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
	auto *move_door_packet = (MoveDoor_Struct*)outapp->pBuffer;
	move_door_packet->doorid = door_id;

	uint32 key = 0;
	if (!DoorKeyCheck(sender, key)) {
		safe_delete(outapp);
		return;
	}

	if (!IsDoorOpen()) {
		move_door_packet->action = invert_state == 1 ? OPEN_INVDOOR : OPEN_DOOR;
		if (door_param > 1) {
			Log(Logs::General, Logs::Doors, "Lift %d is opening (%d) %s", database_id, move_door_packet->action, door_param > 100 ? "." : "starting lift timer.");
		}

		if (close_time > 0) {
			lift_timer.Start(close_time * 1000);
		}

		SetOpenState(true);
	} else {
		move_door_packet->action = invert_state == 1 ? CLOSE_INVDOOR : CLOSE_DOOR;
		if (door_param > 1) {
			Log(Logs::General, Logs::Doors, "Lift %d is closing (%d) %s", database_id, move_door_packet->action, door_param > 100 ? "." : "disabling lift timer.");
		}

		if (close_time > 0) {
			lift_timer.Disable();
		}

		SetOpenState(false);
	}

	entity_list.QueueClients(sender, outapp);
	safe_delete(outapp);

	Doors* triggerdoor = entity_list.FindDoor(GetTriggerDoorID());
	if (triggerdoor) {
		if (!triggerdoor->triggered) {
			triggered = true;
			triggerdoor->HandleLift(sender);
		} else {
			triggered = false;
			triggerdoor->triggered = false;
		}
	}
}

bool Doors::DoorKeyCheck(Client* sender, uint32& key)
{
	uint32 required_key_item        = GetKeyItem();
	uint32 alternate_key_item       = GetAltKeyItem();
	uint8  disable_add_to_key_ring  = GetNoKeyring();
	uint32 player_has_key           = 0;
	uint32 player_has_alternate_key = 0;
	uint32 player_key               = 0;

	const EQ::ItemInstance *lock_pick_item = sender->GetInv().GetItem(EQ::invslot::slotCursor);

	if (lock_pick_item != nullptr) {
		if (lock_pick_item->GetItem()->ItemType != EQ::item::ItemTypeLockPick) {
			lock_pick_item = nullptr;
		}
	}

	player_has_key           = sender->GetInv().HasItem(required_key_item, 1, invWhereCursor);
	player_has_alternate_key = sender->GetInv().HasItem(alternate_key_item, 1, invWhereCursor);

	if (player_has_key == EQ::invslot::slotCursor) {
		player_key = required_key_item;
	} else if (player_has_alternate_key == EQ::invslot::slotCursor) {
		player_key = alternate_key_item;
	}

	key = player_key;

	if ((required_key_item == 0 && alternate_key_item == 0 && GetLockpick() == 0) || (IsDoorOpen() && open_type == 58)) {
		//door not locked
		return true;
	} else {
		// a key is required or the door is locked but can be picked or both
		if (sender->GetGM()) {
			// GM can always open locks
			sender->Message_StringID(Chat::LightBlue, StringID::DOORS_GM);
			return true;
		} else if (player_key) {	// they have something they are trying to open it with
			if (required_key_item && (required_key_item == player_key) || alternate_key_item && (alternate_key_item == player_key)) {
				// key required and client is using the right key
				if (!disable_add_to_key_ring) {
					sender->KeyRingAdd(player_key);
				}

				return true;
			}
		} else if (lock_pick_item != nullptr) {
			if (sender->GetSkill(EQ::skills::SkillPickLock)) {
				if (lock_pick_item->GetItem()->ItemType == EQ::item::ItemTypeLockPick) {
					float modskill = sender->GetSkill(EQ::skills::SkillPickLock);

					// Lockpicks will be on the cursor and not equipped, so we need to manually get any skillmod they may have.
					if (lock_pick_item->GetItem()->SkillModType == EQ::skills::SkillPickLock) {
						modskill += modskill * (static_cast<float>(lock_pick_item->GetItem()->SkillModValue) / 100.0);
						if (modskill > HARD_SKILL_CAP) {
							modskill = HARD_SKILL_CAP;
						}
					}

					Log(Logs::General, Logs::Skills, "Client has lockpicks: skill=%f", modskill);

					if (GetLockpick() <= modskill) {
						if (!IsDoorOpen()) {
							sender->CheckIncreaseSkill(EQ::skills::SkillPickLock, nullptr, zone->skill_difficulty[EQ::skills::SkillPickLock].difficulty[sender->GetClass()]);
						}
						sender->Message_StringID(Chat::LightBlue, StringID::DOORS_SUCCESSFUL_PICK);
						return true;
					} else {
						sender->Message_StringID(Chat::LightBlue, StringID::DOORS_INSUFFICIENT_SKILL);
						return false;
					}
				} else {
					sender->Message_StringID(Chat::LightBlue, StringID::DOORS_NO_PICK);
					return false;
				}
			} else {
				sender->Message_StringID(Chat::LightBlue, StringID::DOORS_CANT_PICK);
				return false;
			}
		} else {	// locked door and nothing to open it with
			// search for key on keyring
			uint32 key_ring = 0;
			if (sender->KeyRingCheck(required_key_item)) {
				key_ring = required_key_item;
			} else if (sender->KeyRingCheck(alternate_key_item)) {
				key_ring = alternate_key_item;
			}

			if (key_ring > 0) {
				key = key_ring;
				return true;
			} else {
				sender->Message_StringID(Chat::LightBlue, StringID::DOORS_LOCKED);
				return false;
			}
		}
	}

	return true;
}

void Doors::OpenDoor(Mob* sender, bool force)
{
	auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
	auto *move_door_packet = (MoveDoor_Struct*)outapp->pBuffer;
	move_door_packet->doorid = door_id;
	move_door_packet->action = invert_state == 0 ? OPEN_DOOR : OPEN_INVDOOR;
	entity_list.QueueClients(sender, outapp, false);

	if (GetTriggerDoorID() != 0) {
		Doors* triggerdoor = entity_list.FindDoor(GetTriggerDoorID());
		if (triggerdoor && (!triggerdoor->IsDoorOpen() || force)) {
			move_door_packet->doorid = triggerdoor->door_id;
			move_door_packet->action = triggerdoor->invert_state == 0 ? OPEN_DOOR : OPEN_INVDOOR;
			entity_list.QueueClients(sender, outapp, false);

			triggerdoor->close_timer.Start(close_time * 1000);
			triggerdoor->SetOpenState(true);
		}
	}

	safe_delete(outapp);
}

void Doors::NPCOpen(NPC* sender, bool alt_mode)
{
	if(sender) {
		if(GetTriggerType() == 255 || (GetTriggerDoorID() > 0 && IsDoorOpen()) || GetLockpick() != 0 || GetKeyItem() != 0 || open_type == 59 || open_type == 58 || !sender->IsNPC()) { // this object isnt triggered or door is locked - NPCs should not open locked doors!
			return;
		}

		OpenDoor(sender);

		if(!alt_mode) { // original function
			if(!is_open) {
				close_timer.Start(close_time * 1000);
				is_open=true;
			} else {
				close_timer.Disable();
				is_open=false;
			}
		} else { // alternative function
			close_timer.Start();
			is_open=true;
		}
	}
}

void Doors::ForceOpen(Mob *sender, bool alt_mode)
{
	OpenDoor(sender, true);

	if(!alt_mode) { // original function
		if(!is_open) {
			close_timer.Start(close_time * 1000);
			is_open=true;
		} else {
			close_timer.Disable();
			is_open=false;
		}
	} else { // alternative function
		close_timer.Start();
		is_open=true;
	}
}

void Doors::ForceClose(Mob *sender, bool alt_mode)
{
	auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
	auto *move_door_packet = (MoveDoor_Struct*)outapp->pBuffer;
	move_door_packet->doorid = door_id;
	move_door_packet->action = invert_state == 0 ? CLOSE_DOOR : CLOSE_INVDOOR; // change from original (open to close)
	entity_list.QueueClients(sender,outapp,false);
	safe_delete(outapp);

	if(!alt_mode) { // original function
		if(!is_open) {
			close_timer.Start(close_time * 1000);
			is_open=true;
		} else {
			close_timer.Disable();
			is_open=false;
		}
	} else { // alternative function
		if(is_open)
			close_timer.Trigger();
	}
}

void Doors::ToggleState(Mob *sender)
{
	if(GetTriggerDoorID() > 0 || GetLockpick() != 0 || GetKeyItem() != 0 || open_type == 58 || open_type == 40) { // borrowed some NPCOpen criteria
		return;
	}

	auto outapp = new EQApplicationPacket(OP_MoveDoor, sizeof(MoveDoor_Struct));
	auto *move_door_packet = (MoveDoor_Struct*)outapp->pBuffer;
	move_door_packet->doorid = door_id;

	if(!is_open) {
		move_door_packet->action = invert_state == 0 ? OPEN_DOOR : OPEN_INVDOOR;
		is_open=true;
	} else {
		move_door_packet->action = invert_state == 0 ? CLOSE_DOOR : CLOSE_INVDOOR;
		is_open=false;
	}

	entity_list.QueueClients(sender,outapp,false);
	safe_delete(outapp);
}

void Doors::DumpDoor(){
	Log(Logs::General, Logs::Doors,
		"db_id:%i door_id:%i zone_name:%s door_name:%s %s",
		database_id, door_id, zone_name, door_name, to_string(m_position).c_str());
	Log(Logs::General, Logs::Doors,
		"opentype:%i lockpick:%i keyitem:%i altkeyitem:%i nokeyring:%i trigger_door:%i trigger_type:%i door_param:%i open:%s lift:%i",
		open_type, lockpick, key_item_id, alt_key_item_id, no_key_ring, trigger_door, trigger_type, door_param, (is_open) ? "open":"closed", is_lift);
	Log(Logs::General, Logs::Doors,
		"dest_zone:%s destination:%s ",
		destination_zone_name, to_string(m_destination).c_str());
}

int32 ZoneDatabase::GetDoorsCount(uint32* oMaxID, const char *zone_name) {

	std::string query = StringFormat("SELECT MAX(id), count(*) FROM doors "
                                    "WHERE zone = '%s'",
                                    zone_name);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return -1;
    }

	if (results.RowCount() != 1) {
		return -1;
	}

    auto row = results.begin();
	 
	if (!oMaxID) {
		return atoi(row[1]);
	}

	if (row[0]) {
		*oMaxID = atoi(row[0]);
	} else {
		*oMaxID = 0;
	}

    return atoi(row[1]);

}

int32 ZoneDatabase::GetDoorsCountPlusOne(const char *zone_name) {

    std::string query = StringFormat("SELECT MAX(id) FROM doors "
                                    "WHERE zone = '%s'", zone_name);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return -1;
    }

	if (results.RowCount() != 1) {
		return -1;
	}

    auto row = results.begin();

	if (!row[0]) {
		return 0;
	}

    return atoi(row[0]) + 1;
}

int32 ZoneDatabase::GetDoorsDBCountPlusOne(const char *zone_name) {

	uint32 oMaxID = 0;

    std::string query = StringFormat("SELECT MAX(doorid) FROM doors "
                                    "WHERE zone = '%s'",
                                    zone_name);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return -1;
	}

	if (results.RowCount() != 1) {
		return -1;
	}

    auto row = results.begin();

	if (!row[0]) {
		return 0;
	}

    return atoi(row[0]) + 1;
}

std::vector<DoorsRepository::Doors> ZoneDatabase::LoadDoors(const std::string &zone_name)
{
	auto door_entries = DoorsRepository::GetWhere(
		*this, fmt::format(
			"zone = '{}' {} ORDER BY doorid ASC",
			zone_name, ContentFilterCriteria::apply()));

	LogInfo("Loaded doors for [{}]", zone_name);

	return door_entries;
}

void Doors::SetLocation(float x, float y, float z)
{
	entity_list.DespawnAllDoors();
    m_position = glm::vec4(x, y, z, m_position.w);
	entity_list.RespawnAllDoors();
}

void Doors::SetPosition(const glm::vec4& position) {
	entity_list.DespawnAllDoors();
	m_position = position;
	entity_list.RespawnAllDoors();
}

void Doors::SetIncline(int in) {
	entity_list.DespawnAllDoors();
	incline = in;
	entity_list.RespawnAllDoors();
}

void Doors::SetInvertState(int in) {
	entity_list.DespawnAllDoors();
	invert_state = in;
	entity_list.RespawnAllDoors();
}

void Doors::SetOpenType(uint8 in) {
	entity_list.DespawnAllDoors();
	open_type = in;
	entity_list.RespawnAllDoors();
}

void Doors::SetDoorName(const char* name) {
	entity_list.DespawnAllDoors();
	memset(door_name, 0, sizeof(door_name));
	strncpy(door_name, name, sizeof(door_name));
	entity_list.RespawnAllDoors();
}

void Doors::SetSize(uint16 in) {
	entity_list.DespawnAllDoors();
	size = in;
	entity_list.RespawnAllDoors();
}

// Old world traps are doors.
Doors *EntityList::FindNearbyDoorTrap(Mob* searcher, float max_dist, float &door_curdist)
{
	float dist = 999999;
	Doors* current_trap = nullptr;

	float max_dist2 = max_dist*max_dist;

	for (auto it = door_list.begin(); it != door_list.end(); ++it) {
		Doors *cur = it->second;
		if (cur->GetOpenType() != 120 && cur->GetOpenType() != 125 && cur->GetOpenType() != 130 && cur->GetOpenType() != 140) {
			continue;
		}

		auto diff = searcher->GetPosition() - cur->GetPosition();
		float curdist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

		if (curdist < max_dist2 && curdist < dist) {
			Log(Logs::General, Logs::Traps, "Door %d has opentype %d and is curdist %0.1f", cur->GetDoorID(), cur->GetOpenType(), curdist);
			dist = curdist;
			current_trap = cur;
		}
	}

	if (current_trap != nullptr) {
		Log(Logs::General, Logs::Traps, "Door %d is the closest trap.", current_trap->GetDoorID());
		door_curdist = dist;
	} else {
		door_curdist = INVALID_INDEX;
	}

	return current_trap;
}

bool Doors::HasDestinationZone() const
{
	return m_has_destination_zone;
}

bool Doors::IsDestinationZoneSame() const
{
	return m_same_destination_zone;
}

float Doors::GetX()
{
	return m_position.x;
}

float Doors::GetY()
{
	return m_position.y;
}

float Doors::GetZ()
{
	return m_position.z;
}