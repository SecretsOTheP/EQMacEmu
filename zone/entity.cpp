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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>

#ifdef _WINDOWS
#include <process.h>
#else
#include <pthread.h>
#include "../common/unix.h"
#endif

#include "../common/misc_functions.h"
#include "../common/features.h"
#include "../common/guilds.h"

#include "guild_mgr.h"
#include "petitions.h"
#include "quest_parser_collection.h"
#include "raids.h"
#include "string_ids.h"
#include "worldserver.h"

#ifdef _WINDOWS
	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#endif

extern Zone *zone;
extern volatile bool is_zone_loaded;
extern WorldServer worldserver;
extern uint32 numclients;
extern PetitionList petition_list;

extern char errorname[32];

Entity::Entity()
{
	id = 0;
}

Entity::~Entity()
{

}

Client *Entity::CastToClient()
{
	if (this == 0x00) {
		Log(Logs::General, Logs::Error, "CastToClient error (nullptr)");
		return 0;
	}
#ifdef _EQDEBUG
	if (!IsClient()) {
		Log(Logs::General, Logs::Error, "CastToClient error (not client)"); 
		return 0;
	}
#endif


	return static_cast<Client *>(this);
}

NPC *Entity::CastToNPC()
{
#ifdef _EQDEBUG
	if (!IsNPC()) {
		Log(Logs::General, Logs::Error, "CastToNPC error (Not NPC)");
		return 0;
	}
#endif
	return static_cast<NPC *>(this);
}

Mob *Entity::CastToMob()
{
#ifdef _EQDEBUG
	if (!IsMob()) {
		std::cout << "CastToMob error" << std::endl;
		return 0;
	}
#endif
	return static_cast<Mob *>(this);
}


Trap *Entity::CastToTrap()
{
#ifdef DEBUG
	if (!IsTrap()) {
		return 0;
	}
#endif
	return static_cast<Trap *>(this);
}

Corpse *Entity::CastToCorpse()
{
#ifdef _EQDEBUG
	if (!IsCorpse()) {
		std::cout << "CastToCorpse error" << std::endl;
		return 0;
	}
#endif
	return static_cast<Corpse *>(this);
}

Object *Entity::CastToObject()
{
#ifdef _EQDEBUG
	if (!IsObject()) {
		std::cout << "CastToObject error" << std::endl;
		return 0;
	}
#endif
	return static_cast<Object *>(this);
}

/*Group* Entity::CastToGroup() {
#ifdef _EQDEBUG
	if(!IsGroup()) {
		std::cout << "CastToGroup error" << std::endl;
		return 0;
	}
#endif
	return static_cast<Group*>(this);
}*/

Doors *Entity::CastToDoors()
{
	return static_cast<Doors *>(this);
}

Beacon *Entity::CastToBeacon()
{
	return static_cast<Beacon *>(this);
}

Encounter *Entity::CastToEncounter()
{
	return static_cast<Encounter *>(this);
}

const Client *Entity::CastToClient() const
{
	if (this == 0x00) {
		std::cout << "CastToClient error (nullptr)" << std::endl;
		return 0;
	}
#ifdef _EQDEBUG
	if (!IsClient()) {
		std::cout << "CastToClient error (not client?)" << std::endl;
		return 0;
	}
#endif
	return static_cast<const Client *>(this);
}

const NPC *Entity::CastToNPC() const
{
#ifdef _EQDEBUG
	if (!IsNPC()) {
		std::cout << "CastToNPC error" << std::endl;
		return 0;
	}
#endif
	return static_cast<const NPC *>(this);
}

const Mob *Entity::CastToMob() const
{
#ifdef _EQDEBUG
	if (!IsMob()) {
		std::cout << "CastToMob error" << std::endl;
		return 0;
	}
#endif
	return static_cast<const Mob *>(this);
}

const Trap *Entity::CastToTrap() const
{
#ifdef DEBUG
	if (!IsTrap()) {
		return 0;
	}
#endif
	return static_cast<const Trap *>(this);
}

const Corpse *Entity::CastToCorpse() const
{
#ifdef _EQDEBUG
	if (!IsCorpse()) {
		std::cout << "CastToCorpse error" << std::endl;
		return 0;
	}
#endif
	return static_cast<const Corpse *>(this);
}

const Object *Entity::CastToObject() const
{
#ifdef _EQDEBUG
	if (!IsObject()) {
		std::cout << "CastToObject error" << std::endl;
		return 0;
	}
#endif
	return static_cast<const Object *>(this);
}

const Doors *Entity::CastToDoors() const
{
	return static_cast<const Doors *>(this);
}

const Beacon* Entity::CastToBeacon() const
{
	return static_cast<const Beacon *>(this);
}

const Encounter* Entity::CastToEncounter() const 
{
	return static_cast<const Encounter *>(this);
}

EntityList::EntityList()
	:
	object_timer(5000),
	door_timer(5000),
	corpse_timer(2000),
	corpse_depop_timer(250),
	group_timer(1000),
	raid_timer(1000),
	trap_timer(1000)
{


	// set up ids between 1 and 1500
	// neither client or server performs well if you have
	// enough entities to exhaust this list
	for (uint16 i = 1; i <= 4999; i++)
		free_ids.push(i);
}

EntityList::~EntityList()
{
	//must call this before the list is destroyed, or else it will try to
	//delete the NPCs in the list, which it cannot do.
	RemoveAllLocalities();
}

bool EntityList::CanAddHateForMob(Mob *p)
{
	int count = 0;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *npc = it->second;
		if (npc->IsOnHatelist(p))
			count++;
		// no need to continue if we already hit the limit
		if (count > 3)
			return false;
		++it;
	}

	if (count <= 2)
		return true;
	return false;
}

void EntityList::AddClient(Client *client)
{
	client->SetID(GetFreeID());
	client_list.insert(std::pair<uint16, Client *>(client->GetID(), client));
	mob_list.insert(std::pair<uint16, Mob *>(client->GetID(), client));
}


void EntityList::TrapProcess()
{

	// MobProcess is the master and sets/disables the timers.
	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->ZoneWillNotIdle())
	{
		if(numclients < 1 && zone->idle)
		{
			return;
		}
	}

	if (trap_list.empty()) {
		trap_timer.Disable();
		return;
	}

	auto it = trap_list.begin();
	while (it != trap_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = trap_list.erase(it);
		} else {
			++it;
		}
	}
}


// Debug function -- checks to see if group_list has any nullptr entries.
// Meant to be called after each group-related function, in order
// to track down bugs.
void EntityList::CheckGroupList (const char *fname, const int fline)
{
	std::list<Group *>::iterator it;

	for (it = group_list.begin(); it != group_list.end(); ++it)
	{
		if (*it == nullptr)
		{
			Log(Logs::General, Logs::Error, "nullptr group, %s:%i", fname, fline);
		}
	}
}

void EntityList::GroupProcess()
{
	if (numclients < 1)
		return;

	if (group_list.empty()) {
		group_timer.Disable();
		return;
	}

	for (auto &group : group_list)
		group->Process();

#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
}


void EntityList::RaidProcess()
{
	if (numclients < 1)
		return;

	if (raid_list.empty()) {
		raid_timer.Disable();
		return;
	}

	for (auto &raid : raid_list)
		raid->Process();
}

void EntityList::DoorProcess()
{

	// MobProcess is the master and sets/disables the timers.
	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->ZoneWillNotIdle())
	{
		if(numclients < 1 && zone->idle)
		{
			return;
		}
	}

	if (door_list.empty()) {
		door_timer.Disable();
		return;
	}

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = door_list.erase(it);
		}
		++it;
	}
}

void EntityList::ObjectProcess()
{

	// MobProcess is the master and sets/disables the timers.
	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->ZoneWillNotIdle())
	{
		if(numclients < 1 && zone->idle)
		{
			return;
		}
	}

	if (object_list.empty()) {
		object_timer.Disable();
		return;
	}

	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = object_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::CorpseProcess()
{
	if (corpse_list.empty()) {
		corpse_timer.Disable(); // No corpses in list
		corpse_depop_timer.Disable();
		return;
	}

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::CorpseDepopProcess()
{
	if (corpse_list.empty()) {
		corpse_timer.Disable(); // No corpses in list
		corpse_depop_timer.Disable();
		return;
	}

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (!it->second->DepopProcess()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		}
		else {
			++it;
		}
	}
}

void EntityList::MobProcess()
{
	bool mob_dead;
	uint16 aggroed_npcs = 0;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		uint16 id = it->first;
		Mob *mob = it->second;
		size_t sz = mob_list.size();

		if (zone && RuleB(Zone, IdleWhenEmpty) && !zone->ZoneWillNotIdle() && !zone->IsBoatZone())
		{
			static Timer* mob_settle_timer = new Timer();

			if (numclients < 1 && !mob_settle_timer->Enabled() && !zone->idle)
			{
				mob_settle_timer->Start(RuleI(Zone, IdleTimer)); // idle timer from when the last player left the zone.
				Log(Logs::General, Logs::Status, "Entity Process: Number of clients has dropped to 0. Setting idle timer.");
				LogSys.log_settings[Logs::PacketServerClient].log_to_gmsay = 0;
				LogSys.log_settings[Logs::PacketClientServer].log_to_gmsay = 0;
			}
			else if (numclients >= 1 && zone->idle)
			{
				if (mob_settle_timer->Enabled())
					mob_settle_timer->Disable();
				zone->idle = false;
				Log(Logs::General, Logs::Status, "Entity Process: A player has entered the zone, leaving idle state.");
			}

			if (mob_settle_timer->Check())
			{
				mob_settle_timer->Disable();
				if (numclients < 1)
				{
					zone->idle = true;
					Log(Logs::General, Logs::Status, "Entity Process: Idle timer has expired, zone will now idle.");
				}
				else
				{
					zone->idle = false;
					Log(Logs::General, Logs::Status, "Entity Process: Idle timer has expired, but there are players in the zone. Zone will not idle.");
				}
			}


			if (zone->process_mobs_while_empty || !zone->idle || numclients > 0 || mob_settle_timer->Enabled() || mob->GetWanderType() == GridOneWayRepop || mob->GetWanderType() == GridOneWayDepop)
			{
				// Normal processing, or assuring that spawns that should
				// path and depop do that.  Otherwise all of these type mobs
				// will be up and at starting positions when idle zone wakes up.
				mob_dead = !mob->Process();
			}
			else
			{
				// spawn_events can cause spawns and deaths while zone idled.
				// At the very least, process that.
				mob_dead = mob->CastToNPC()->GetDepop();
			}

		}
		else
			mob_dead = !mob->Process();

		size_t a_sz = mob_list.size();

		if(a_sz > sz) {
			//increased size can potentially screw with iterators so reset it to current value
			//if buckets are re-orderered we may skip a process here and there but since
			//process happens so often it shouldn't matter much
			it = mob_list.find(id);
			++it;
		} else {
			++it;
		}

		if (mob_dead) {
			if(mob->IsNPC()) {
				entity_list.RemoveNPC(id);
			}
			else {
#ifdef _WINDOWS
				struct in_addr in;
				in.s_addr = mob->CastToClient()->GetIP();
				Log(Logs::General, Logs::ZoneServer, "Dropping client: Process=false, ip=%s port=%u", inet_ntoa(in), mob->CastToClient()->GetPort());
#endif
				Group *g = GetGroupByMob(mob);
				if(g) {
					Log(Logs::General, Logs::Error, "About to delete a client still in a group.");
					g->DelMember(mob);
				}
				Raid *r = entity_list.GetRaidByClient(mob->CastToClient());
				if(r) {
					Log(Logs::General, Logs::Error, "About to delete a client still in a raid.");
					r->MemberZoned(mob->CastToClient());
				}
				entity_list.RemoveClient(id);
			}

			entity_list.RemoveMob(id);
		}
		else if (mob->IsNPC() && mob->IsEngaged())
		{
			aggroed_npcs++;
		}
	}
	zone->SetNumAggroedNPCs(aggroed_npcs);
}

void EntityList::BeaconProcess()
{
	auto it = beacon_list.begin();
	while (it != beacon_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = beacon_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::EncounterProcess()
{
	auto it = encounter_list.begin();
	while (it != encounter_list.end()) {
		if (!it->second->Process()) {
			// if Process is returning false here, we probably just got called from ReloadQuests .. oh well
			parse->RemoveEncounter(it->second->GetEncounterName());
			safe_delete(it->second);
			free_ids.push(it->first);
			it = encounter_list.erase(it);
		}
		else {
			++it;
		}
	}
}

void EntityList::AddGroup(Group *group)
{
	if (group == nullptr)	//this seems to be happening somehow...
		return;

	uint32 gid = worldserver.NextGroupID();
	if (gid == 0) {
		Log(Logs::General, Logs::Error, 
				"Unable to get new group ID from world server. group is going to be broken.");
		return;
	}

	AddGroup(group, gid);
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
}

void EntityList::AddGroup(Group *group, uint32 gid)
{
	group->SetID(gid);
	group_list.push_back(group);
	if (!group_timer.Enabled())
		group_timer.Start();
#if EQDEBUG >= 5
	CheckGroupList(__FILE__, __LINE__);
#endif
}

void EntityList::AddRaid(Raid *raid)
{
	if (raid == nullptr)
		return;

	uint32 gid = worldserver.NextGroupID();
	if (gid == 0) {
		Log(Logs::General, Logs::Error, 
				"Unable to get new group ID from world server. group is going to be broken.");
		return;
	}

	AddRaid(raid, gid);
}

void EntityList::AddRaid(Raid *raid, uint32 gid)
{
	raid->SetID(gid);
	raid_list.push_back(raid);
	if (!raid_timer.Enabled())
		raid_timer.Start();
}


void EntityList::AddCorpse(Corpse *corpse, uint32 in_id)
{
	if (corpse == 0)
		return;

	if (in_id == 0xFFFFFFFF)
		corpse->SetID(GetFreeID());
	else
		corpse->SetID(in_id);

	corpse->CalcCorpseName();
	corpse_list.emplace(std::pair<uint16, Corpse *>(corpse->GetID(), corpse));

	// we are a new corpse, so update distances to us for clients.
	float mydist = 0;

	if (!corpse_timer.Enabled()) {
		corpse_timer.Start();
		corpse_depop_timer.Start();
	}
}

void EntityList::AddNPC(NPC *npc, bool SendSpawnPacket, bool dontqueue)
{
	npc->SetID(GetFreeID());
	//npc->SetMerchantProbability((uint8) zone->random.Int(0, 99));

	npc->SetSpawned();
	if (SendSpawnPacket) {
		if (dontqueue) { // aka, SEND IT NOW BITCH!
			EQApplicationPacket app;
			npc->CreateSpawnPacket(&app, npc);
			QueueClients(npc, &app);
			safe_delete_array(app.pBuffer);
			npc->SpawnPacketSent(true);
			parse->EventNPC(EVENT_SPAWN, npc, nullptr, "", 0);

			if (!npc->GetDepop()) {
				uint32 emoteid = npc->GetEmoteID();
				if (emoteid != 0) {
					npc->DoNPCEmote(EQ::constants::EmoteEventTypes::OnSpawn, emoteid);
				}
			}
		} else {
			auto ns = new NewSpawn_Struct;
			memset(ns, 0, sizeof(NewSpawn_Struct));
			npc->FillSpawnStruct(ns, nullptr);	// Not working on player newspawns, so it's safe to use a ForWho of 0
			AddToSpawnQueue(npc->GetID(), &ns);
			safe_delete(ns);
			if (client_list.size() == 0) {
				npc->SpawnPacketSent(true);
				parse->EventNPC(EVENT_SPAWN, npc, nullptr, "", 0);
			}
		}
	}
	else {
		npc->SpawnPacketSent(true);
		parse->EventNPC(EVENT_SPAWN, npc, nullptr, "", 0);
	}

	npc_list.insert(std::pair<uint16, NPC *>(npc->GetID(), npc));
	mob_list.insert(std::pair<uint16, Mob *>(npc->GetID(), npc));

	npc->SetAttackTimer(true); // set attacker timers to be ready immediately on spawn

	/* Zone controller process EVENT_SPAWN_ZONE */
	if (entity_list.GetNPCByNPCTypeID(ZONE_CONTROLLER_NPC_ID) && npc->GetNPCTypeID() != ZONE_CONTROLLER_NPC_ID) {
		char data_pass[100] = { 0 };
		snprintf(data_pass, 99, "%d %d", npc->GetID(), npc->GetNPCTypeID());
		parse->EventNPC(EVENT_SPAWN_ZONE, entity_list.GetNPCByNPCTypeID(ZONE_CONTROLLER_NPC_ID)->CastToNPC(), nullptr, data_pass, 0);
	}

}

void EntityList::AddObject(Object *obj, bool SendSpawnPacket)
{
	obj->SetID(GetFreeID());

	if (SendSpawnPacket) {
		EQApplicationPacket app;
		obj->CreateSpawnPacket(&app);
		#if (EQDEBUG >= 6)
			DumpPacket(&app);
		#endif
		QueueClients(0, &app,false);
		safe_delete_array(app.pBuffer);
	}

	object_list.emplace(std::pair<uint16, Object *>(obj->GetID(), obj));

	if (!object_timer.Enabled())
		object_timer.Start();
}

void EntityList::AddDoor(Doors *door)
{
	door->SetEntityID(GetFreeID());
	door_list.emplace(std::pair<uint16, Doors *>(door->GetEntityID(), door));

	if (!door_timer.Enabled())
		door_timer.Start();
}

void EntityList::AddTrap(Trap *trap)
{
	trap->SetID(GetFreeID());
	trap_list.emplace(std::pair<uint16, Trap *>(trap->GetID(), trap));
	if (!trap_timer.Enabled())
		trap_timer.Start();
}

void EntityList::AddBeacon(Beacon *beacon)
{
	beacon->SetID(GetFreeID());
	beacon_list.emplace(std::pair<uint16, Beacon *>(beacon->GetID(), beacon));
}

void EntityList::AddEncounter(Encounter *encounter)
{
	encounter->SetID(GetFreeID());
	encounter_list.emplace(std::pair<uint16, Encounter*>(encounter->GetID(), encounter));
}

void EntityList::AddToSpawnQueue(uint16 entityid, NewSpawn_Struct **ns)
{
	uint32 count;
	if ((count = (client_list.size())) == 0)
		return;
	SpawnQueue.Append(*ns);
	NumSpawnsOnQueue++;
	if (tsFirstSpawnOnQueue == 0xFFFFFFFF)
		tsFirstSpawnOnQueue = Timer::GetCurrentTime();
	*ns = nullptr;
}

void EntityList::CheckSpawnQueue()
{
	// Send the stuff if the oldest packet on the queue is older than 50ms -Quagmire
	if (tsFirstSpawnOnQueue != 0xFFFFFFFF && (Timer::GetCurrentTime() - tsFirstSpawnOnQueue) > 50) {
		LinkedListIterator<NewSpawn_Struct *> iterator(SpawnQueue);
		EQApplicationPacket outapp;

		iterator.Reset();
		while(iterator.MoreElements()) {
			Mob::CreateSpawnPacket(&outapp, iterator.GetData());
			QueueClients(0, &outapp);
			Mob* mob = GetMob(iterator.GetData()->spawn.spawnId);
			if (mob && mob->IsNPC()) {
				mob->SpawnPacketSent(true);
				parse->EventNPC(EVENT_SPAWN, mob->CastToNPC(), nullptr, "", 0);
				if (!mob->CastToNPC()->GetDepop()) {
					uint16 emoteid = mob->CastToNPC()->GetEmoteID();
					if (emoteid != 0) {
						mob->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::OnSpawn, emoteid);
					}
				}
			}
			
			safe_delete_array(outapp.pBuffer);
			iterator.RemoveCurrent();
		}
		tsFirstSpawnOnQueue = 0xFFFFFFFF;
		NumSpawnsOnQueue = 0;
	}
}

void EntityList::OpenFloorTeleportNear(Client* c)
{
	if (!c || door_list.empty())
		return;

	auto client_pos = c->GetPosition();
	for (auto it = door_list.begin(); it != door_list.end(); ++it) 
	{
		Doors *cdoor = it->second;

		if (!cdoor || !cdoor->IsMoveable() || cdoor->GetOpenType() != 57 || !cdoor->IsTeleport())
			continue;

		auto diff = c->GetPosition() - cdoor->GetPosition();
		float curdist = diff.x * diff.x + diff.y * diff.y;
		Log(Logs::Detail, Logs::Doors, "A floor teleport with id %d was found at %0.2f away at %0.2f Z.", cdoor->GetDoorDBID(), curdist, diff.z * diff.z);
		if (diff.z * diff.z < 105 && curdist <= 600)
		{
			cdoor->HandleClick(c, 0, true);
			return;
		}
	}
}

Doors *EntityList::FindNearestDoor(Client* c)
{
	if (!c || door_list.empty())
		return nullptr;

	Doors *nearest = nullptr;
	float closest = 999999.0f;

	auto it = door_list.begin();
	while (it != door_list.end()) {

		if (!it->second)
			continue;

		auto diff = c->GetPosition() - it->second->GetPosition();

		float curdist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

		if (curdist < closest)
		{
			closest = curdist;
			nearest = it->second;
		}
		++it;
	}
	return nearest;

}

glm::vec3 EntityList::GetDoorLoc(Client *c, int doorid)
{
	if (!c || door_list.empty())
		return glm::vec3(0.0f, 0.0f, 0.0f);

	auto it = door_list.begin();
	while (it != door_list.end()) {

		if (!it->second)
			continue;
		if (it->second->GetDoorID() == doorid) {
			return glm::vec3(it->second->GetPosition().x, it->second->GetPosition().y, it->second->GetPosition().z);
		}	
		++it;
	}

	return glm::vec3(0.0f, 0.0f, 0.0f);
	
}

void EntityList::ListDoors(Client* c)
{
	if (!c || door_list.empty())
		return;

	auto it = door_list.begin();
	while (it != door_list.end()) {

		if (!it->second)
			continue;
		c->Message(Chat::White, "DoorID %3d at %.2f, %.2f, %.2f. (%s)", it->second->GetDoorID(), it->second->GetPosition().x,it->second->GetPosition().y,it->second->GetPosition().z, it->second->GetDoorName());
				
		++it;
	}
	return;
}

Doors *EntityList::FindDoor(uint8 door_id)
{
	if (door_id < 0 || door_list.empty())
		return nullptr;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (it->second->GetDoorID() == door_id)
			return it->second;
		++it;
	}

	return nullptr;
}

Object *EntityList::FindObject(uint32 object_id)
{
	if (object_id == 0 || object_list.empty())
		return nullptr;

	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (it->second->GetDBID() == object_id)
			return it->second;
		++it;
	}

	return nullptr;
}

Object *EntityList::FindNearbyObject(float x, float y, float z, float radius)
{
	if (object_list.empty())
		return nullptr;

	float ox;
	float oy;
	float oz;

	auto it = object_list.begin();
	while (it != object_list.end()) {
		Object *object = it->second;

		object->GetLocation(&ox, &oy, &oz);

		ox = (x < ox) ? (ox - x) : (x - ox);
		oy = (y < oy) ? (oy - y) : (y - oy);
		oz = (z < oz) ? (oz - z) : (z - oz);

		if ((ox <= radius) && (oy <= radius) && (oz <= radius))
			return object;

		++it;
	}

	return nullptr;
}

bool EntityList::SendZoneDoorsBulk(EQApplicationPacket* app, Client *client)
{
	if (door_list.empty())
		return false;

	uint32 mask_test = client->ClientVersionBit();
	uchar doorstruct[sizeof(Door_Struct)];
	uchar packet[sizeof(Door_Struct)*255];
	int16 length = 0;
	uint8 count = 0;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if ((it->second->GetClientVersionMask() & mask_test) &&
			strlen(it->second->GetDoorName()) > 3)
			count++;
		++it;
	}

	if (count == 0 || count > 255) //doorid is uint8
		return false;

	Doors *door;
	Door_Struct* nd = (Door_Struct*)doorstruct;
	memset(nd,0,sizeof(Door_Struct));

	it = door_list.begin();
	while (it != door_list.end()) {
		door = it->second;
		if (door && (door->GetClientVersionMask() & mask_test) &&
				strlen(door->GetDoorName()) > 3) {
			memcpy(nd->name, door->GetDoorName(), 16);
			auto position = door->GetPosition();
			nd->xPos = position.x;
			nd->yPos = position.y;
			nd->zPos = position.z;
			nd->heading = position.w;			
			nd->incline = door->GetIncline();
			nd->size = door->GetSize();
			nd->doorid = door->GetDoorID();
			nd->opentype = door->GetOpenType();
			nd->doorIsOpen = door->GetInvertState() ? !door->IsDoorOpen() : door->IsDoorOpen();
			nd->inverted = door->GetInvertState();
			nd->parameter = door->GetDoorParam();
			
			memcpy(packet+length,doorstruct,sizeof(Door_Struct));
			length += sizeof(Door_Struct);
		}
		++it;
	}

	int32 deflength = sizeof(Door_Struct) * count;
	int buffer = 2; //Length of count that preceeds the packet.

	app->SetOpcode(OP_SpawnDoor);
	uint32 deflatedSize = EstimateDeflateBuffer(length);
	app->pBuffer = new uchar[deflatedSize];
	app->size = buffer + DeflatePacket(packet, length, app->pBuffer + buffer, deflatedSize);
	DoorSpawns_Struct* ds = (DoorSpawns_Struct*)app->pBuffer;

	ds->count = count;

	return true;
}

Entity *EntityList::GetEntityMob(uint16 id)
{
	auto it = mob_list.find(id);
	if (it != mob_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetEntityMob(const char *name)
{
	if (name == 0 || mob_list.empty())
		return 0;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (strcasecmp(it->second->GetName(), name) == 0)
			return it->second;
		++it;
	}

	return nullptr;
}

Entity *EntityList::GetEntityDoor(uint16 id)
{
	auto it = door_list.find(id);
	if (it != door_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetEntityCorpse(uint16 id)
{
	auto it = corpse_list.find(id);
	if (it != corpse_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetEntityCorpse(const char *name)
{
	if (name == 0 || corpse_list.empty())
		return nullptr;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (strcasecmp(it->second->GetName(), name) == 0)
			return it->second;
		++it;
	}

	return nullptr;
}

Entity *EntityList::GetEntityTrap(uint16 id)
{
	auto it = trap_list.find(id);
	if (it != trap_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetEntityObject(uint16 id)
{
	auto it = object_list.find(id);
	if (it != object_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetEntityBeacon(uint16 id)
{
	auto it = beacon_list.find(id);
	if (it != beacon_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetEntityEncounter(uint16 id)
{
	auto it = encounter_list.find(id);
	if (it != encounter_list.end())
		return it->second;
	return nullptr;
}

Entity *EntityList::GetID(uint16 get_id)
{
	Entity *ent = 0;
	if ((ent = entity_list.GetEntityMob(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityDoor(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityCorpse(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityObject(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityTrap(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityBeacon(get_id)) != 0)
		return ent;
	else if ((ent = entity_list.GetEntityEncounter(get_id)) != 0)
		return ent;
	else
		return 0;
}

NPC *EntityList::GetNPCByNPCTypeID(uint32 npc_id)
{
	if (npc_id == 0 || npc_list.empty())
		return nullptr;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->GetNPCTypeID() == npc_id)
			return it->second;
		++it;
	}

	return nullptr;
}

Mob *EntityList::GetMob(uint16 get_id)
{
	Entity *ent = nullptr;

	if (get_id == 0)
		return nullptr;

	if ((ent = entity_list.GetEntityMob(get_id)))
		return ent->CastToMob();
	else if ((ent = entity_list.GetEntityCorpse(get_id)))
		return ent->CastToMob();

	return nullptr;
}

Mob *EntityList::GetMob(const char *name)
{
	Entity* ent = nullptr;

	if (name == 0)
		return nullptr;

	if ((ent = entity_list.GetEntityMob(name)))
		return ent->CastToMob();
	else if ((ent = entity_list.GetEntityCorpse(name)))
		return ent->CastToMob();

	return nullptr;
}

Mob *EntityList::GetMobByNpcTypeID(uint32 get_id)
{
	if (get_id == 0 || mob_list.empty())
		return 0;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->GetNPCTypeID() == get_id)
			return it->second;
		++it;
	}
	return nullptr;
}

bool EntityList::IsMobSpawnedByNpcTypeID(uint32 get_id)
{
	if (get_id == 0 || npc_list.empty())
		return false;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		// Mobs will have a 0 as their GetID() if they're dead
		if (it->second->GetNPCTypeID() == get_id && it->second->GetID() != 0)
			return true;
		++it;
	}

	return false;
}

bool EntityList::IsMobSpawnedByEntityID(uint32 get_id)
{
	if (get_id == 0 || mob_list.empty())
		return false;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		// Mobs will have a 0 as their GetID() if they're dead
		if (it->second->GetID() == get_id && it->second->GetID() != 0)
			return true;
		++it;
	}

	return false;
}

Object *EntityList::GetObjectByDBID(uint32 id)
{
	if (id == 0 || object_list.empty())
		return nullptr;

	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (it->second->GetDBID() == id)
			return it->second;
		++it;
	}
	return nullptr;
}

Doors *EntityList::GetDoorsByDBID(uint32 id)
{
	if (id == 0 || door_list.empty())
		return nullptr;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (it->second->GetDoorDBID() == id)
			return it->second;
		++it;
	}

	return nullptr;
}

Doors *EntityList::GetDoorsByDoorID(uint32 id)
{
	if (id == 0 || door_list.empty())
		return nullptr;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (it->second->CastToDoors()->GetDoorID() == id)
			return it->second;
		++it;
	}

	return nullptr;
}

uint16 EntityList::GetFreeID()
{
	if (free_ids.empty()) { // hopefully this will never be true
		// The client has a hard cap on entity count some where
		// Neither the client or server performs well with a lot entities either
		uint16 newid = 4997; // Client's g_pEQP_IDArray is memset by 20000. However, there are many cases where IDs checked to be <= 4998u. We adhere to this value minus 1, just to be sure.
		while (true) {
			newid++;
			if (GetID(newid) == nullptr)
				return newid;
		}
	}

	uint16 newid = free_ids.front();
	free_ids.pop();
	return newid;
}

// if no language skill is specified, sent with 100 skill
void EntityList::ChannelMessage(Mob *from, uint8 chan_num, uint8 language, const char *message, ...)
{
	ChannelMessage(from, chan_num, language, Language::MaxValue, message);
}

void EntityList::ChannelMessage(Mob *from, uint8 chan_num, uint8 language,
		uint8 lang_skill, const char *message, ...)
{
	char buffer[4096];

	memcpy(buffer, message, 4096);

	auto it = client_list.begin();
	while(it != client_list.end()) {
		Client *client = it->second;
		eqFilterType filter = FilterNone;
		if (chan_num == ChatChannel_Shout) //shout
			filter = FilterShouts;
		else if (chan_num == ChatChannel_Auction) //auction
			filter = FilterAuctions;
		//
		// Only say is limited in range
		if (chan_num != ChatChannel_Say || Distance(client->GetPosition(), from->GetPosition()) < 200)
			if (filter == FilterNone || client->GetFilter(filter) != FilterHide)
				client->ChannelMessageSend(from->GetName(), 0, chan_num, language, lang_skill, buffer);
		++it;
	}
}

void EntityList::SendZoneSpawns(Client *client)
{
	EQApplicationPacket app;
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *ent = it->second;
		if (!(ent->InZone()) || (ent->IsClient())) {
			if (ent->CastToClient()->GMHideMe(client)) {
				++it;
				continue;
			}
		}
		it->second->CastToMob()->CreateSpawnPacket(&app); // TODO: Use zonespawns opcode instead
		client->QueuePacket(&app, true, Client::CLIENT_CONNECTED);
		safe_delete_array(app.pBuffer);
		++it;
	}
}

void EntityList::SendZoneSpawnsBulk(Client *client)
{
	NewSpawn_Struct ns;
	Mob *spawn;
	uint32 maxspawns = 100;

	if (maxspawns > mob_list.size())
		maxspawns = mob_list.size();
	auto bzsp = new BulkZoneSpawnPacket(client, maxspawns);
	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		spawn = it->second;
		if (spawn && spawn->Spawned()) {
			if (spawn->IsClient() && (spawn == client || spawn->CastToClient()->GMHideMe(client)))
				continue;
			memset(&ns, 0, sizeof(NewSpawn_Struct));
			spawn->FillSpawnStruct(&ns, client);
			bzsp->AddSpawn(&ns);
		}
	}
	safe_delete(bzsp);
}

void EntityList::SendZoneCorpses(Client *client)
{
	EQApplicationPacket app;
	for (auto it = corpse_list.begin(); it != corpse_list.end(); ++it) {
		Corpse *ent = it->second;
		ent->CreateSpawnPacket(&app);
		client->QueuePacket(&app, true, Client::CLIENT_CONNECTED);
		safe_delete_array(app.pBuffer);
	}
}

void EntityList::SendZoneCorpsesBulk(Client *client)
{
	NewSpawn_Struct ns;
	Corpse *spawn;
	uint32 maxspawns = 100;

	auto bzsp = new BulkZoneSpawnPacket(client, maxspawns);

	for (auto it = corpse_list.begin(); it != corpse_list.end(); ++it) {
		spawn = it->second;
		if (spawn && spawn->InZone()) {
			memset(&ns, 0, sizeof(NewSpawn_Struct));
			spawn->FillSpawnStruct(&ns, client);
			bzsp->AddSpawn(&ns);
		}
	}
	safe_delete(bzsp);
}

void EntityList::SendZoneObjects(Client *client)
{
	EQApplicationPacket app;
	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (!it->second->IsGroundSpawn() || !it->second->RespawnTimerEnabled()) {
			it->second->CreateSpawnPacket(&app);
			client->QueuePacket(&app);
			safe_delete_array(app.pBuffer);
		}
		++it;
	}
}

void EntityList::Save()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		it->second->Save();
		++it;
	}
}

void EntityList::InterruptTargeted(Mob* mob)
{
	if (!mob)
		return;
	Mob* Cur = nullptr;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		Cur = it->second;
		if (Cur->GetTarget() == mob && (!Cur->GetOwner() || !Cur->GetOwner()->IsClient()) && Cur->IsCasting()) 
			Cur->InterruptSpell();
		++it;
	}	
}

void EntityList::ReplaceWithTarget(Mob *pOldMob, Mob *pNewTarget)
{
	if (!pNewTarget)
		return;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->IsAIControlled()) {
			// replace the old mob with the new one
			if (it->second->RemoveFromHateList(pOldMob))
					it->second->AddToHateList(pNewTarget, 1, 0);
		}
		++it;
	}
}

void EntityList::RemoveFromTargets(Mob *mob)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *m = it->second;
		++it;

		if (!m)
			continue;

		m->RemoveFromHateList(mob);
		m->RemoveFromRampageList(mob);
	}
}

void EntityList::RemoveFromNPCTargets(Mob *mob)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *npc = it->second;
		++it;

		if (!npc)
			continue;

		npc->RemoveFromHateList(mob);
		npc->RemoveFromRampageList(mob);
	}
}

void EntityList::QueueClientsByTarget(Mob *sender, const EQApplicationPacket *app,
		bool iSendToSender, Mob *SkipThisMob, bool ackreq, bool HoTT, uint32 ClientVersionBits)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		++it;

		Mob *Target = c->GetTarget();

		if (!Target)
			continue;

		Mob *TargetsTarget = nullptr;

		if (Target)
			TargetsTarget = Target->GetTarget();

		bool Send = false;

		if (c == SkipThisMob)
			continue;

		if (iSendToSender)
			if (c == sender)
				Send = true;

		if (c != sender) {
			if (Target == sender)
				Send = true;
			else if (HoTT)
				if (TargetsTarget == sender)
					Send = true;
		}

		if (Send && (c->ClientVersionBit() & ClientVersionBits))
			c->QueuePacket(app, ackreq);
	}
}

void EntityList::QueueCloseClients(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, float dist, Mob *SkipThisMob, bool ackreq, eqFilterType filter)
{
	if (sender == nullptr) {
		QueueClients(sender, app, ignore_sender);
		return;
	}

	if (dist <= 0)
		dist = 600;
	float dist2 = dist * dist; //pow(dist, 2);

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if (ent != nullptr && (!ignore_sender || ent != sender) && (ent != SkipThisMob)) {
			eqFilterMode filter2 = ent->GetFilter(filter);
			if(ent->Connected() &&
				(filter == FilterNone
				|| filter2 == FilterShow
				|| (filter2 == FilterShowGroupOnly && (sender == ent ||
					(ent->GetGroup() && ent->GetGroup()->IsGroupMember(sender))))
				|| (filter2 == FilterShowSelfOnly && ent == sender))
			&& (DistanceSquared(ent->GetPosition(), sender->GetPosition()) <= dist2)) {
				ent->QueuePacket(app, ackreq, Client::CLIENT_CONNECTED);
			}
		}
		++it;
	}
}

//sender can be null
void EntityList::QueueClientsPosUpdate(Mob *sender, const EQApplicationPacket *app,
	bool ignore_sender, bool ackreq)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if ((!ignore_sender || ent != sender)) {
			ent->QueuePacket(app, ackreq, Client::CLIENT_CONNECTED);
		}
		++it;
	}
}

//sender can be null
void EntityList::QueueClients(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, bool ackreq)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if ((!ignore_sender || ent != sender))
			ent->QueuePacket(app, ackreq, Client::CLIENT_CONNECTED);

		++it;
	}
}

void EntityList::QueueWearChange(Mob *sender, const EQApplicationPacket *app, bool ignore_sender, uint16 slot, bool force_helm_update)
{
	/*
		custom helms - IT240 but clients using old models don't send 240, they send the specific model instead

		665 and 660 work with luclin models for vah shir, they don't show up on non cats.
		these send 240 with both new and old models so shouldn't be seen in wearchanges normally

		vah shir 665/660
		human 627/620
		barbarian 537/530
		erudite 575/570
		wood elf 565/561
		high elf 605/600
		dark elf 545/540
		half elf 595/590
		dwarf 557/550
		troll 655/650
		ogre 645/640
		halfling 615/610
		gnome 585/580
		iksar 635/630

        The following race/gender combinations have a hair portion to the head which is shown/hidden for custom helms.
        This is implemented poorly in the client and all instances of that head share the same state, so if a spawn has a custom
        helm equipped all other spawns/players with that head lose the back of their head.  This also happens in the other direction
		so that you get both the custom helm and the hair model showing at the same time.  There seems to be no way to work around this
		short of disabling custom helms for less glitchy models.

        Human Female
        Barbarian Female
        Erudite Male
        Erudite Female
        Wood Elf Female
        Dark Elf Female
	*/
	if (app)
	{
		WearChange_Struct *wc = (WearChange_Struct *)app->pBuffer;
		if (wc->wear_slot_id == EQ::textures::armorHead)
		{
			switch (wc->material)
			{
			case 665: // vah
			case 660: // vah
			case 627: // hum
			case 620: // hum
			case 537: // bar
			case 530: // bar
			case 570: // eru
			case 575: // eru
			case 565: // elf
			case 561: // elf
			case 605: // hie
			case 600: // hie
			case 545: // def
			case 540: // def
			case 595: // hef
			case 590: // hef
			case 557: // dwf
			case 550: // dwf
			case 655: // trl
			case 650: // trl
			case 645: // ogr
			case 640: // ogr
			case 615: // hlf
			case 610: // hlf
			case 585: // gnm
			case 580: // gnm
			case 635: // iks
			case 630: // iks
				wc->material = 240;
				break;
			case 646: // ogr (alt)
			case 641: // ogr (alt)
				// Client has built-in support for material 241 on ogres, giving an alternate helm model. It looks the same as barbarian helm with that RZ-style look.
				if (sender && sender->GetRace() == Race::Ogre)
					wc->material = 241;
				else
					wc->material = 240;
				break;

				// unfortunately the tint is wrong when a tinted custom helm is worn by a client using old models and there seems to be no good way to work around this.
				// when the same helm is worn by a luclin model client it just treats it as a normal tinted item so they will appear inconsistent to luclin model using viewers
				// we can't just look at their inventory because the wear change packets come before the item swap packets.  it would work for the special case where we're initially,
				// zoning in and sending the first appearance but it would just get overwritten again when they swap the helm and get the wrong tint if we tried to peek at the inventory

				/* this is just here for a comment and is wrong, don't use this code.
				   if we're relaying a wear change where the client sent the wrong tint, we can't know the right tint since it may not be in the inventory slot yet. 
				   the wearchange is sent before the item swaps.
				Client *c = GetClientByID(wc->spawn_id);
				if (c)
				{
					wc->color.Color = c->GetEquipmentColor(wc->wear_slot_id);
				}
				*/
			}

			if (RuleB(Quarm, UseFixedShowHelmBehavior)) {
				force_helm_update = true;
				if (sender && sender->IsClient() && !sender->CastToClient()->ShowHelm()) {
					wc->material = 0;
				}
			}
		}
	}

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if ((!ignore_sender || ent != sender) && (slot != EQ::textures::armorHead || ent->ShowHelm() || force_helm_update))
			ent->QueuePacket(app, true, Client::CLIENT_CONNECTED);

		++it;
	}
}

void EntityList::QueueClientsStatus(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, uint8 minstatus, uint8 maxstatus)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if ((!ignore_sender || it->second != sender) &&
				(it->second->Admin() >= minstatus && it->second->Admin() <= maxstatus))
			it->second->QueuePacket(app);

		++it;
	}
}

void EntityList::DuelMessage(Mob *winner, Mob *loser, bool flee)
{
	if (winner->GetLevelCon(winner->GetLevel(), loser->GetLevel()) > 2) {
		std::vector<std::any> args;
		args.push_back(winner);
		args.push_back(loser);

		parse->EventPlayer(EVENT_DUEL_WIN, winner->CastToClient(), loser->GetName(), loser->CastToClient()->CharacterID(), &args);
		parse->EventPlayer(EVENT_DUEL_LOSE, loser->CastToClient(), winner->GetName(), winner->CastToClient()->CharacterID(), &args);
	}

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *cur = it->second;
		//might want some sort of distance check in here?
		if (cur != winner && cur != loser) {
			if (flee)
				cur->Message_StringID(Chat::Yellow, DUEL_FLED, winner->GetName(),loser->GetName(),loser->GetName());
			else
				cur->Message_StringID(Chat::Yellow, DUEL_FINISHED, winner->GetName(),loser->GetName());
		}
		++it;
	}
}

Client *EntityList::GetClientByName(const char *checkname)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (strcasecmp(it->second->GetName(), checkname) == 0)
			return it->second;
		++it;
	}
	return nullptr;
}

Client *EntityList::GetClientByCharID(uint32 iCharID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->CharacterID() == iCharID)
			return it->second;
		++it;
	}
	return nullptr;
}

Client *EntityList::GetClientByWID(uint32 iWID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->GetWID() == iWID) {
			return it->second;
		}
		++it;
	}
	return nullptr;
}

Client* EntityList::GetClientByLSID(uint32 iLSID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->LSAccountID() == iLSID) {
			return it->second;
		}
		++it;
	}
	return nullptr;
}

Client *EntityList::GetRandomClient(const glm::vec3& location, float Distance, Client *ExcludeClient)
{
	std::vector<Client *> ClientsInRange;


	for (auto it = client_list.begin();it != client_list.end(); ++it)
		if ((it->second != ExcludeClient) && (DistanceSquared(static_cast<glm::vec3>(it->second->GetPosition()), location) <= Distance))
			ClientsInRange.push_back(it->second);

	if (ClientsInRange.empty())
		return nullptr;

	return ClientsInRange[zone->random.Int(0, ClientsInRange.size() - 1)];
}

Corpse *EntityList::GetCorpseByOwner(Client *client)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->IsPlayerCorpse())
			if (strcasecmp(it->second->GetOwnerName(), client->GetName()) == 0)
				return it->second;
		++it;
	}
	return nullptr;
}

Corpse *EntityList::GetCorpseByOwnerWithinRange(Client *client, Mob *center, int range)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->IsPlayerCorpse())
			if (DistanceSquaredNoZ(center->GetPosition(), it->second->GetPosition()) < range &&
					strcasecmp(it->second->GetOwnerName(), client->GetName()) == 0)
				return it->second;
		++it;
	}
	return nullptr;
}

Corpse *EntityList::GetCorpseByDBID(uint32 dbid)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCorpseDBID() == dbid)
			return it->second;
		++it;
	}
	return nullptr;
}

Corpse *EntityList::GetCorpseByName(const char *name)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (strcmp(it->second->GetName(), name) == 0)
			return it->second;
		++it;
	}
	return nullptr;
}

Spawn2 *EntityList::GetSpawnByID(uint32 id)
{
	if (!zone || !zone->IsLoaded())
		return nullptr;

	LinkedListIterator<Spawn2 *> iterator(zone->spawn2_list);
	iterator.Reset();
	while(iterator.MoreElements())
	{
		if(iterator.GetData()->GetID() == id) {
			return iterator.GetData();
		}
		iterator.Advance();
	}

	return nullptr;
}

bool EntityList::IsMobSpawnBySpawnID(uint32 id)
{
	if (id == 0 || npc_list.empty())
		return false;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		// Mobs will have a 0 as their GetID() if they're dead
		if (it->second->GetNPCTypeID() && it->second->GetID() != 0 && it->second->GetSpawnPointID() == id)
			return true;
		++it;
	}

	return false;
}

void EntityList::RemoveAllCorpsesByCharID(uint32 charid)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCharID() == charid) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::RemoveCorpseByDBID(uint32 dbid)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCorpseDBID() == dbid) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		} else {
			++it;
		}
	}
}

int EntityList::RezzAllCorpsesByCharID(uint32 charid)
{
	int RezzExp = 0;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCharID() == charid) {
			RezzExp += it->second->GetRezExp();
			it->second->CompleteResurrection();
		}
		++it;
	}
	return RezzExp;
}

Group *EntityList::GetGroupByMob(Mob *mob)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if ((*iterator)->IsGroupMember(mob))
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Group *EntityList::GetGroupByLeaderName(const char *leader)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if (!strcmp((*iterator)->GetLeaderName(), leader))
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Group *EntityList::GetGroupByID(uint32 group_id)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if ((*iterator)->GetID() == group_id)
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Group *EntityList::GetGroupByClient(Client *client)
{
	std::list <Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if ((*iterator)->IsGroupMember(client->CastToMob()))
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Raid *EntityList::GetRaidByLeaderName(const char *leader)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		if ((*iterator)->GetLeader())
			if(strcmp((*iterator)->GetLeader()->GetName(), leader) == 0)
				return *iterator;
		++iterator;
	}
	return nullptr;
}

Raid *EntityList::GetRaidByID(uint32 id)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		if ((*iterator)->GetID() == id)
			return *iterator;
		++iterator;
	}
	return nullptr;
}

Raid *EntityList::GetRaidByClient(Client* client)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		for (int x = 0; x < MAX_RAID_MEMBERS; x++)
			if ((*iterator)->members[x].member)
				if((*iterator)->members[x].member == client)
					return *iterator;
		++iterator;
	}
	return nullptr;
}

Raid *EntityList::GetRaidByMob(Mob *mob)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
			// TODO: Implement support for Mob objects in Raid class
			/*if((*iterator)->members[x].member){
				if((*iterator)->members[x].member == mob)
					return *iterator;
			}*/
		}
		++iterator;
	}
	return nullptr;
}

Client *EntityList::GetClientByAccID(uint32 accid)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->AccountID() == accid)
			return it->second;
		++it;
	}
	return nullptr;
}

void EntityList::ChannelMessageFromWorld(const char *from, const char *to,
		uint8 chan_num, uint32 guild_id, uint8 language, uint8 lang_skill, const char *message)
{
	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		Client *client = it->second;

		if (!client->Connected())
			continue;

		if (chan_num == ChatChannel_Guild) {
			if (!client->IsInGuild(guild_id) && !client->IsGMInGuild(guild_id))
				continue;
			if (!guild_mgr.CheckPermission(guild_id, client->GuildRank(), GUILD_HEAR) && !client->IsGMInGuild(guild_id))
				continue;
			if (client->GetFilter(FilterGuildChat) == FilterHide)
				continue;
		} else if (chan_num == ChatChannel_OOC) {
			if (client->GetFilter(FilterOOC) == FilterHide)
				continue;
		}
		if(chan_num == ChatChannel_Guild && guild_id > 0 && client->GetGM() && client->IsGMInGuild(guild_id) && !client->IsInGuild(guild_id))
			client->Message(Chat::Yellow,"[GM Monitor] %s tells the guild, '%s'", from, message);
		else
			client->ChannelMessageSend(from, to, chan_num, language, lang_skill, message);
	}
}

void EntityList::Message(uint32 to_guilddbid, uint32 type, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4096, message, argptr);
	va_end(argptr);

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (to_guilddbid == 0 || client->IsInGuild(to_guilddbid))
			client->Message(type, buffer);
		++it;
	}
}

void EntityList::QueueClientsGuild(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, uint32 guild_id)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (client->IsInGuild(guild_id))
			client->QueuePacket(app);
		++it;
	}
}

void EntityList::MessageStatus(uint32 to_guild_id, int to_minstatus, uint32 type, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4096, message, argptr);
	va_end(argptr);

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if ((to_guild_id == 0 || client->IsInGuild(to_guild_id)) && client->Admin() >= to_minstatus)
			client->Message(type, buffer);
		++it;
	}
}

// works much like MessageClose, but with formatted strings
void EntityList::MessageClose_StringID(Mob *sender, bool skipsender, float dist, uint32 type, uint32 string_id, const char* message1,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9)
{
	Client *c;
	float dist2 = dist * dist;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if(c && DistanceSquared(c->GetPosition(), sender->GetPosition()) <= dist2 && (!skipsender || c != sender))
			c->Message_StringID(type, string_id, message1, message2, message3, message4, message5, message6, message7, message8, message9);
	}
}

void EntityList::FilteredMessageClose_StringID(Mob *sender, bool skipsender,
		float dist, uint32 type, eqFilterType filter, uint32 string_id,
		const char *message1, const char *message2, const char *message3,
		const char *message4, const char *message5, const char *message6,
		const char *message7, const char *message8, const char *message9)
{
	Client *c;
	float dist2 = dist * dist;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if (c && DistanceSquared(c->GetPosition(), sender->GetPosition()) <= dist2 && (!skipsender || c != sender))
			c->FilteredMessage_StringID(sender, type, filter, string_id,
					message1, message2, message3, message4, message5,
					message6, message7, message8, message9);
	}
}

void EntityList::Message_StringID(Mob *sender, bool skipsender, uint32 type, uint32 string_id, const char* message1,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9)
{
	Client *c;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if(c && (!skipsender || c != sender))
			c->Message_StringID(type, string_id, message1, message2, message3, message4, message5, message6, message7, message8, message9);
	}
}

void EntityList::FilteredMessage_StringID(Mob *sender, bool skipsender,
		uint32 type, eqFilterType filter, uint32 string_id,
		const char *message1, const char *message2, const char *message3,
		const char *message4, const char *message5, const char *message6,
		const char *message7, const char *message8, const char *message9)
{
	Client *c;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if (c && (!skipsender || c != sender))
			c->FilteredMessage_StringID(sender, type, filter, string_id,
					message1, message2, message3, message4, message5, message6,
					message7, message8, message9);
	}
}

void EntityList::MessageClose(Mob* sender, bool skipsender, float dist, uint32 type, const char* message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4095, message, argptr);
	va_end(argptr);

	float dist2 = dist * dist;

	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (DistanceSquared(it->second->GetPosition(), sender->GetPosition()) <= dist2 && (!skipsender || it->second != sender))
			it->second->Message(type, buffer);
		++it;
	}
}

void EntityList::RemoveAllMobs()
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (!it->second) {
			++it;
			continue;
		}
		safe_delete(it->second);
		free_ids.push(it->first);
		it = mob_list.erase(it);
	}
}

void EntityList::RemoveAllClients()
{
	// doesn't clear the data
	client_list.clear();
}

void EntityList::RemoveAllNPCs()
{
	// doesn't clear the data
	npc_list.clear();
	npc_limit_list.clear();
}

void EntityList::RemoveAllGroups()
{
	while (!group_list.empty()) {
		auto group = group_list.front();
		group_list.pop_front();
		safe_delete(group);
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
}

void EntityList::RemoveAllRaids()
{
	while (!raid_list.empty()) {
		auto raid = raid_list.front();
		raid_list.pop_front();
		safe_delete(raid);
	}
}

void EntityList::RemoveAllDoors()
{
	auto it = door_list.begin();
	while (it != door_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = door_list.erase(it);
	}
	DespawnAllDoors();
}

void EntityList::DespawnAllDoors()
{
	auto outapp = new EQApplicationPacket(OP_DespawnDoor, 0);
	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		if (it->second) {
			it->second->QueuePacket(outapp);
		}
	}
	safe_delete(outapp);
}

void EntityList::RespawnAllDoors()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second) {
			auto outapp = new EQApplicationPacket();
			SendZoneDoorsBulk(outapp, it->second);
			it->second->FastQueuePacket(&outapp);
		}
		++it;
	}
}

void EntityList::RemoveAllCorpses()
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = corpse_list.erase(it);
	}
}

void EntityList::RemoveAllObjects()
{
	auto it = object_list.begin();
	while (it != object_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = object_list.erase(it);
	}
}

void EntityList::RemoveAllTraps()
{
	auto it = trap_list.begin();
	while (it != trap_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = trap_list.erase(it);
	}
}

void EntityList::RemoveAllEncounters()
{
	auto it = encounter_list.begin();
	while (it != encounter_list.end()) {
		parse->RemoveEncounter(it->second->GetEncounterName());
		safe_delete(it->second);
		free_ids.push(it->first);
		it = encounter_list.erase(it);
	}
}

bool EntityList::RemoveMob(uint16 delete_id)
{
	if (delete_id == 0) {
		return true;
	}

	auto it = mob_list.find(delete_id);
	if (it != mob_list.end()) {
		if (!it->second) {
			return false;
		}
		if (npc_list.count(delete_id)) {
			entity_list.RemoveNPC(delete_id);
		}
		else if (client_list.count(delete_id)) {
			entity_list.RemoveClient(delete_id);
		}
		safe_delete(it->second);
		if (!corpse_list.count(delete_id)) {
			free_ids.push(it->first);
		}
		mob_list.erase(it);
		return true;
	}
	return false;
}

// This is for if the ID is deleted for some reason
bool EntityList::RemoveMob(Mob *delete_mob)
{
	if (delete_mob == 0)
		return true;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second == delete_mob) {
			safe_delete(it->second);
			if (!corpse_list.count(it->first))
				free_ids.push(it->first);
			mob_list.erase(it);
			return true;
		}
		++it;
	}
	return false;
}

bool EntityList::RemoveNPC(uint16 delete_id)
{
	auto it = npc_list.find(delete_id);
	if (it != npc_list.end()) {
		NPC* npc = it->second;
		RemoveProximity(delete_id);
		npc_list.erase(it);
		
		if (npc_limit_list.count(delete_id)) {
			npc_limit_list.erase(delete_id);
		}

		return true;
	}
	return false;
}

bool EntityList::RemoveClient(uint16 delete_id)
{
	auto it = client_list.find(delete_id);
	if (it != client_list.end()) {
		client_list.erase(it); // Already deleted
		return true;
	}
	return false;
}

// If our ID was deleted already
bool EntityList::RemoveClient(Client *delete_client)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second == delete_client) {
			client_list.erase(it);
			return true;
		}
		++it;
	}
	return false;
}

bool EntityList::RemoveObject(uint16 delete_id)
{
	auto it = object_list.find(delete_id);
	if (it != object_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		object_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveTrap(uint16 delete_id)
{
	auto it = trap_list.find(delete_id);
	if (it != trap_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		trap_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveDoor(uint16 delete_id)
{
	auto it = door_list.find(delete_id);
	if (it != door_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		door_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveCorpse(uint16 delete_id)
{
	auto it = corpse_list.find(delete_id);
	if (it != corpse_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		corpse_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveGroup(uint32 delete_id)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while(iterator != group_list.end())
	{
		if((*iterator)->GetID() == delete_id) {
			safe_delete(*iterator);
			group_list.remove(*iterator);
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
			return true;
		}
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return false;
}

bool EntityList::RemoveRaid(uint32 delete_id)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while(iterator != raid_list.end())
	{
		if((*iterator)->GetID() == delete_id) {
			safe_delete(*iterator);
			raid_list.remove(*iterator);
			return true;
		}
		++iterator;
	}
	return false;
}

void EntityList::Clear()
{
	RemoveAllClients();
	entity_list.RemoveAllTraps(); //we can have child npcs so we go first
	entity_list.RemoveAllNPCs();
	entity_list.RemoveAllMobs();
	entity_list.RemoveAllCorpses();
	entity_list.RemoveAllGroups();
	entity_list.RemoveAllDoors();
	entity_list.RemoveAllObjects();
	entity_list.RemoveAllRaids();
	entity_list.RemoveAllLocalities();
}

void EntityList::UpdateWho(bool iSendFullUpdate)
{
	if ((!worldserver.Connected()) || !is_zone_loaded)
		return;
	uint32 tmpNumUpdates = numclients + 5;
	ServerPacket* pack = 0;
	ServerClientListKeepAlive_Struct* sclka = 0;
	if (!iSendFullUpdate) {
		pack = new ServerPacket(ServerOP_ClientListKA, sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4));
		sclka = (ServerClientListKeepAlive_Struct*) pack->pBuffer;
	}

	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->InZone()) {
			if (iSendFullUpdate) {
				it->second->UpdateWho();
			} else {
				if (sclka->numupdates >= tmpNumUpdates) {
					tmpNumUpdates += 10;
					uint8* tmp = pack->pBuffer;
					pack->pBuffer = new uint8[sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4)];
					memset(pack->pBuffer, 0, sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4));
					memcpy(pack->pBuffer, tmp, pack->size);
					pack->size = sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4);
					safe_delete_array(tmp);
					sclka = (ServerClientListKeepAlive_Struct*) pack->pBuffer;
				}
				sclka->wid[sclka->numupdates] = it->second->GetWID();
				sclka->numupdates++;
			}
		}
		++it;
	}
	if (!iSendFullUpdate) {
		if (sclka->numupdates > 0)
		{
			pack->size = sizeof(ServerClientListKeepAlive_Struct) + (sclka->numupdates * 4);
			worldserver.SendPacket(pack);
		}
		safe_delete(pack);
	}
}

void EntityList::RemoveEntity(uint16 id)
{
	if (id == 0)
		return;
	if (entity_list.RemoveMob(id))
		return;
	else if (entity_list.RemoveCorpse(id))
		return;
	else if (entity_list.RemoveDoor(id))
		return;
	else if (entity_list.RemoveGroup(id))
		return;
	else if (entity_list.RemoveTrap(id))
		return;
	else
		entity_list.RemoveObject(id);
}

void EntityList::Process()
{
	CheckSpawnQueue();
}

void EntityList::Depop(bool StartSpawnTimer)
{
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *pnpc = it->second;
		if (pnpc) {
			Mob *own = pnpc->GetOwner();
			//do not depop player's pets...
			if (own && own->IsClient())
				continue;

			if (pnpc->IsHorse())
				continue;

			pnpc->Depop(StartSpawnTimer);
		}
	}
}

void EntityList::DepopAll(int NPCTypeID, bool StartSpawnTimer)
{
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *pnpc = it->second;
		if (pnpc && (pnpc->GetNPCTypeID() == (uint32)NPCTypeID)){
			pnpc->Depop(StartSpawnTimer); 

			/* Web Interface Depop Entities */
			/*std::vector<std::string> params;
			params.push_back(std::to_string((long)pnpc->GetID()));
			RemoteCallSubscriptionHandler::Instance()->OnEvent("NPC.Depop", params);*/
		}
	}
}

void EntityList::SendTraders(Client *client)
{
	Client *trader = nullptr;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		trader = it->second;
		if (trader->IsTrader())
			client->SendTraderPacket(trader);

		++it;
	}
}

void EntityList::SendIllusionedPlayers(Client *client)
{
	Client *illusion = nullptr;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		illusion = it->second;
		if (!illusion->GMHideMe(client) && (illusion->GetRace() == Race::MinorIllusion || illusion->GetRace() == Race::Tree))
		{
			illusion->SendIllusionPacket(
				illusion->GetRace(),
				2,
				0,
				0,
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				6,
				client
			);
		}
		++it;
	}
}

void EntityList::SendHelms(Client *client)
{
	if (RuleB(Quarm, UseFixedShowHelmBehavior)) {
		// This is no-op when this rule is enabled, because '/showhelm' only toggles your own WearChange events.
		// You don't need other clients to send you WearChanges when you change '/showhelm'.
		return;
	}
	Client *helm = nullptr;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		helm = it->second;
		if(client != helm && helm && helm->GetEquipmentMaterial(EQ::textures::armorHead) != 0)
			helm->SendWearChange(EQ::textures::armorHead, client);

		++it;
	}
}

void EntityList::HideHelms(Client *client)
{
	if (RuleB(Quarm, UseFixedShowHelmBehavior)) {
		// This is no-op when this rule is enabled, because '/showhelm' only toggles your own WearChange events.
		// You don't need other clients to send you WearChanges when you change '/showhelm'.
		return;
	}
	Client *helm = nullptr;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		helm = it->second;
		if (client != helm && helm && helm->GetEquipmentMaterial(EQ::textures::armorHead) != 0)
			helm->WearChange(EQ::textures::armorHead, 0, 0, client);

		++it;
	}
}

void EntityList::HideMyHelm(Client *client)
{
	bool force_helm_update = false;
	if (RuleB(Quarm, UseFixedShowHelmBehavior)) {
		if (client->ShowHelm()) {
			// We have '/showhelm on', so don't hide our helmet from anyone.
			return;
		}
		// We have '/showhelm off', so hide it from everyone in the zone.
		force_helm_update = true;
	}

	Client *other = nullptr;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		other = it->second;

		if (client != other && other && (force_helm_update || !other->ShowHelm()))
			client->WearChange(EQ::textures::armorHead, 0, 0, other);

		++it;
	}
}

void EntityList::RemoveFromClientHateLists(Mob *mob)
{
	auto it = client_list.begin();

	while (it != client_list.end())
	{
		if (it->second->CheckAggro(mob))
		{
			it->second->RemoveFromHateList(mob);
		}
		++it;
	}
}

void EntityList::RemoveFromHateLists(Mob *mob, bool settoone)
{
	auto it = npc_list.begin();

	while (it != npc_list.end())
	{
		if (it->second->CheckAggro(mob))
		{
			if (!settoone)
			{
				it->second->RemoveFromHateList(mob);
				it->second->RemoveFromRampageList(mob);
			}
			else
			{
				it->second->SetHate(mob, 1);
			}
		}
		++it;
	}
}

void EntityList::RemoveDebuffs(Mob *caster)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		it->second->BuffFadeDetrimentalByCaster(caster);
		++it;
	}
}

void EntityList::RemoveDebuffsFromNPCs(Mob *caster)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		it->second->BuffFadeDetrimentalByCaster(caster);
		++it;
	}
}

void EntityList::RemoveDotsFromNPCs(Mob *caster)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		it->second->BuffFadeDotsByCaster(caster);
		++it;
	}
}

// This is for sending client updates out to other clients.
void EntityList::SendPositionUpdates(Client *client)
{
	float range = zone->update_range;

	auto outapp = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
	SpawnPositionUpdates_Struct *ppu = (SpawnPositionUpdates_Struct*)outapp->pBuffer;
	client->MakeSpawnUpdate(&ppu->spawn_update);
	ppu->num_updates = 1;

	Client *c = 0;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		c = it->second;
		if (c && !c->IsCorpse() && (c != client) && !client->GMHideMe(c)) {
			c->QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
		}
		++it;
	}
	safe_delete(outapp);
}


char* EntityList::MakeNameUnique(char* name)
{
	name[60] = 0;  name[61] = 0; name[62] = 0; name[63] = 0;

	for (int number = 0; number < 1000; number++)
	{
		char temp_name[64];
		snprintf(temp_name, 64, "%s%03d", name, number);

		bool used = false;
		for (auto& it : mob_list)
		{
			Mob* m = it.second;
			if (m)
			{
				if (strncasecmp(m->GetName(), temp_name, 63) == 0)
				{
					used = true;
					break;
				}
			}
		}

		if (!used)
		{
			strcpy(name, temp_name);
			return name;
		}
	}

	Log(Logs::General, Logs::Error, "Fatal error in EntityList::MakeNameUnique: Unable to find unique name for '%s'", name);
	char tmp[64] = "!";
	strn0cpy(&tmp[1], name, sizeof(tmp) - 1);
	strcpy(name, tmp);
	return MakeNameUnique(name);
}

char *EntityList::RemoveNumbers(char *name)
{
	char tmp[64];
	memset(tmp, 0, sizeof(tmp));
	int k = 0;
	for (unsigned int i=0; i<strlen(name) && i<sizeof(tmp); i++) {
		if (name[i] < '0' || name[i] > '9')
			tmp[k++] = name[i];
	}
	strn0cpy(name, tmp, sizeof(tmp));
	return name;
}

void EntityList::ListNPCs(Client* client, const char *arg1, const char *arg2, uint8 searchtype)
{
	if (arg1 == 0)
		searchtype = 0;
	else if (arg2 == 0 && searchtype >= 2)
		searchtype = 0;
	uint32 x = 0;
	uint32 z = 0;
	std::string sName;

	auto it = npc_list.begin();
	client->Message(Chat::White, "NPCs in the zone:");
	if (searchtype == 0) {
		while (it != npc_list.end()) {
			NPC *n = it->second;
			std::string spawn2 = n->GetSpawnedString();
			client->Message(Chat::White, "  %5d: %s (%.0f, %0.f, %.0f) Spawned: %s", n->GetID(), n->GetName(), n->GetX(), n->GetY(), n->GetZ(), spawn2.c_str());
			x++;
			z++;
			++it;
		}
	} 
	else if (searchtype == 1) {
		client->Message(Chat::White, "Searching by name method. (%s)",arg1);
		std::string tmp;
		tmp = arg1;
		for (auto & c : tmp) c = toupper(c);
		while (it != npc_list.end()) {
			z++;
			sName = it->second->GetName();
			for (auto & c : sName) c = toupper(c);
			if (sName.find(tmp) != std::string::npos) 
			{
				NPC *n = it->second;
				std::string spawn2 = n->GetSpawnedString();
				client->Message(Chat::White, "  %5d: %s (%.0f, %.0f, %.0f) Spawned: %s", n->GetID(), n->GetName(), n->GetX(), n->GetY(), n->GetZ(), spawn2.c_str());
				x++;
			}
			++it;
		}
	} 
	else if (searchtype == 2) {
		client->Message(Chat::White, "Searching by number method. (%s %s)",arg1,arg2);
		while (it != npc_list.end()) {
			z++;
			if ((it->second->GetID() >= atoi(arg1)) && (it->second->GetID() <= atoi(arg2)) && (atoi(arg1) <= atoi(arg2))) {
				client->Message(Chat::White, "  %5d: %s", it->second->GetID(), it->second->GetName());
				x++;
			}
			++it;
		}
	}
	else if (searchtype == 3) {
		while (it != npc_list.end()) {
			z++;
			NPC *n = it->second;
			if(!n->IsAssisting())
			{
				std::string spawn2 = n->GetSpawnedString();
				client->Message(Chat::White, "  %5d: %s (%.0f, %0.f, %.0f) Spawned: %s", n->GetID(), n->GetName(), n->GetX(), n->GetY(), n->GetZ(), spawn2.c_str());
				x++;
			}
			++it;
		}
	}
	client->Message(Chat::White, "%d npcs listed. There is a total of %d npcs in this zone.", x, z);
}

void EntityList::ListNPCCorpses(Client *client)
{
	uint32 corpse_count = 0;
	for (const auto& corpse : corpse_list) {
		uint32 corpse_number = (corpse_count + 1);
		if (corpse.second->IsNPCCorpse()) {
			client->Message(
				Chat::White,
				fmt::format(
					"Corpse {} | Name: {} ({})",
					corpse_number,
					corpse.second->GetName(),
					corpse.second->GetID()
				).c_str()
			);
			corpse_count++;
		}
	}

	if (corpse_count > 0) {
		client->Message(
			Chat::White,
			fmt::format(
				" {} NPC corpses listed.",
				corpse_count
			).c_str()
		);
	}
}

void EntityList::ListPlayerCorpses(Client *client)
{
	uint32 corpse_count = 0;
	for (const auto& corpse : corpse_list) {
		uint32 corpse_number = (corpse_count + 1);
		if (corpse.second->IsPlayerCorpse()) {
			client->Message(
				Chat::White,
				fmt::format(
					"Corpse {} | Name: {} ({})",
					corpse_number,
					corpse.second->GetName(),
					corpse.second->GetID()
				).c_str()
			);
			corpse_count++;
		}
	}

	if (corpse_count > 0) {
		client->Message(
			Chat::White,
			fmt::format(
				" {} Player corpses listed.",
				corpse_count
			).c_str()
		);
	}
}

void EntityList::DespawnGridNodes(int32 grid_id) {
	for (auto m : mob_list) {
		Mob* mob = m.second;
		if (
			mob->IsNPC() &&
			mob->GetRace() == Race::Tribunal &&
			mob->EntityVariableExists("grid_id") &&
			Strings::ToInt(mob->GetEntityVariable("grid_id")) == grid_id)
		{
			mob->Depop();
		}
	}
}

// returns the number of corpses deleted. A negative number indicates an error code.
uint32 EntityList::DeleteNPCCorpses()
{
	uint32 corpse_count = 0;
	for (const auto& corpse : corpse_list) {
		if (corpse.second->IsNPCCorpse()) {
			corpse.second->DepopNPCCorpse();
			corpse_count++;
		}
	}
	return corpse_count;
}

// returns the number of corpses deleted. A negative number indicates an error code.
uint32 EntityList::DeletePlayerCorpses()
{
	uint32 corpse_count = 0;
	for (const auto& corpse : corpse_list) {
		if (corpse.second->IsPlayerCorpse()) {
			corpse.second->Delete();
			corpse_count++;
		}
	}
	return corpse_count;
}

void EntityList::SendPetitionToAdmins()
{
	auto outapp = new EQApplicationPacket(OP_PetitionRefresh,sizeof(PetitionUpdate_Struct));
	PetitionUpdate_Struct *pcus = (PetitionUpdate_Struct*) outapp->pBuffer;
	pcus->petnumber = 0;		// Petition Number
	pcus->color = 0;
	pcus->status = 0xFFFFFFFF;
	pcus->senttime = 0;
	strcpy(pcus->accountid, "");
	strcpy(pcus->gmsenttoo, "");
	pcus->quetotal=0;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->CastToClient()->Admin() >= 80)
			it->second->CastToClient()->QueuePacket(outapp);
		++it;
	}
	safe_delete(outapp);
}

void EntityList::SendPetitionToAdmins(Petition *pet)
{
	auto outapp = new EQApplicationPacket(OP_PetitionRefresh,sizeof(PetitionUpdate_Struct));
	PetitionUpdate_Struct *pcus = (PetitionUpdate_Struct*) outapp->pBuffer;
	pcus->petnumber = pet->GetID();		// Petition Number
	if (pet->CheckedOut()) {
		pcus->color = 0x00;
		pcus->status = 0xFFFFFFFF;
		pcus->senttime = pet->GetSentTime();
		strcpy(pcus->accountid, "");
		strcpy(pcus->gmsenttoo, "");
	} else {
		pcus->color = pet->GetUrgency();	// 0x00 = green, 0x01 = yellow, 0x02 = red
		pcus->status = pet->GetSentTime();
		pcus->senttime = pet->GetSentTime();			// 4 has to be 0x1F
		strcpy(pcus->accountid, pet->GetAccountName());
		strcpy(pcus->charname, pet->GetCharName());
	}
	pcus->quetotal = petition_list.GetTotalPetitions();
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->CastToClient()->Admin() >= 80) {
			if (pet->CheckedOut())
				strcpy(pcus->gmsenttoo, "");
			else
				strcpy(pcus->gmsenttoo, it->second->CastToClient()->GetName());
			it->second->CastToClient()->QueuePacket(outapp);
		}
		++it;
	}
	safe_delete(outapp);
}

void EntityList::WriteEntityIDs()
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		std::cout << "ID: " << it->first << "  Name: " << it->second->GetName() << std::endl;
		++it;
	}
}

BulkZoneSpawnPacket::BulkZoneSpawnPacket(Client *iSendTo, uint32 iMaxSpawnsPerPacket)
{
	data = nullptr;
	pSendTo = iSendTo;
	pMaxSpawnsPerPacket = iMaxSpawnsPerPacket;
}

BulkZoneSpawnPacket::~BulkZoneSpawnPacket()
{
	SendBuffer();
	safe_delete_array(data);
}

bool BulkZoneSpawnPacket::AddSpawn(NewSpawn_Struct *ns)
{
	if (!data) {
		data = new NewSpawn_Struct[pMaxSpawnsPerPacket];
		memset(data, 0, sizeof(NewSpawn_Struct) * pMaxSpawnsPerPacket);
		index = 0;
	}
	memcpy(&data[index], ns, sizeof(NewSpawn_Struct));
	index++;
	if (index >= pMaxSpawnsPerPacket) {
		SendBuffer();
		return true;
	}
	return false;
}

void BulkZoneSpawnPacket::SendBuffer()
{
	if (!data)
		return;

	uint32 tmpBufSize = (index * sizeof(NewSpawn_Struct));
	auto outapp = new EQApplicationPacket(OP_ZoneSpawns, (unsigned char *)data, tmpBufSize);

	if (pSendTo) {
		pSendTo->FastQueuePacket(&outapp);
	} else {
		entity_list.QueueClients(0, outapp);
		safe_delete(outapp);
	}
	memset(data, 0, sizeof(NewSpawn_Struct) * pMaxSpawnsPerPacket);
	safe_delete_array(data);
	index = 0;
}

void EntityList::DoubleAggro(Mob *who)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(who))
			it->second->SetHate(who, it->second->CastToNPC()->GetHateAmount(who),
					it->second->CastToNPC()->GetHateAmount(who) * 2);
		++it;
	}
}

void EntityList::HalveAggro(Mob *who)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CastToNPC()->CheckAggro(who))
			it->second->CastToNPC()->SetHate(who, it->second->CastToNPC()->GetHateAmount(who, false) / 2);
		++it;
	}
}

void EntityList::ReduceAggro(Mob *who)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CastToNPC()->CheckAggro(who))
			it->second->CastToNPC()->SetHate(who, 1);
		++it;
	}
}

//removes "targ" from all hate lists, including feigned, in the zone
void EntityList::ClearAggro(Mob* targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end())
	{
		if (it->second->CheckAggro(targ))
		{
			it->second->RemoveFromHateList(targ);
			it->second->RemoveFromRampageList(targ, true);
		}

		++it;
	}
}

// this is called when the player stands up
// stun_elapsed is the number of miliseconds elapsed since stun timer has started; 0 if client not stunned
void EntityList::ClearFeignAggro(Mob *targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(targ)) {
			if (it->second->GetSpecialAbility(SpecialAbility::FeignDeathImmunity)) {
				++it;
				continue;
			}

			if (targ->IsClient())
			{
				std::vector<std::any> args;
				args.push_back(it->second);
				int i = parse->EventPlayer(EVENT_FEIGN_DEATH, targ->CastToClient(), "", 0, &args);
				if (i != 0) {
					++it;
					continue;
				}
				if (it->second->IsNPC()) {
					int i = parse->EventNPC(EVENT_FEIGN_DEATH, it->second->CastToNPC(), targ, "", 0);
					if (i != 0) {
						++it;
						continue;
					}
				}
				
				// Sony's FD has a strange quirk in where if the client is stunned while feigning (i.e. by a raid boss spell) and also
				// if the client stands up immediately after the FD+stun spells lands on them, then the client will reliably avoid a blur
				if (targ->IsStunned() && Timer::GetCurrentTime() - targ->CastToClient()->GetFeignedTime() < 500)
				{
					++it;
					continue;
				}

				if (it->second->GetLevel() < 35 || zone->random.Roll(35))
				{
					it->second->RemoveFromHateList(targ);
					if (it->second->GetSpecialAbility(SpecialAbility::Rampage))
						it->second->RemoveFromRampageList(targ, true);
				}
				else 
				{
					it->second->SetHate(targ, 64);
				}
			}
		}
		++it;
	}
}

void EntityList::AggroZone(Mob *who, int hate, bool use_ignore_dist)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (!it->second->IsUnTargetable())
			it->second->AddToHateList(who, hate);
		if(!use_ignore_dist)
			it->second->SetRememberDistantMobs(true);
		++it;
	}
}

// Signal Quest command function
void EntityList::SignalMobsByNPCID(uint32 snpc, int signal_id, const char* data)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *pit = it->second;
		if (pit->GetNPCTypeID() == snpc)
			pit->SignalNPC(signal_id, data);
		++it;
	}
}

void EntityList::MessageGroup(Mob *sender, bool skipclose, uint32 type, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4095, message, argptr);
	va_end(argptr);

	float dist2 = 100;

	if (skipclose)
		dist2 = 0;

	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second != sender &&
				(Distance(it->second->GetPosition(), sender->GetPosition()) <= dist2 || it->second->GetGroup() == sender->CastToClient()->GetGroup())) {
			it->second->Message(type, buffer);
		}
		++it;
	}
}

bool EntityList::Fighting(Mob *targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(targ))
			return true;
		++it;
	}
	return false;
}

void EntityList::AddHealAggro(Mob *target, Mob *caster, uint16 hate)
{
	// This is used for all beneficial aggro, and not just heals.

	if (!target || !caster)
		return;

	NPC *cur = nullptr;
	auto it = npc_list.begin();
	while (it != npc_list.end())
	{
		cur = it->second;

		if (cur->IsPet() || !cur->CheckAggro(target) || cur->IsFeared() 
			|| (target->IsClient() && target->CastToClient()->IsFeigned() && !cur->GetSpecialAbility(SpecialAbility::FeignDeathImmunity))
		)
		{
			++it;
			continue;
		}
		if (zone->random.Roll(50))		// heals and other beneficial spells can fail a 'witness check' and do zero hate
		{
			++it;
			continue;
		}

		float ignoreDistance = cur->GetIgnoreDistance() * cur->GetIgnoreDistance();
		if (cur->GetIgnoreDistance() > 500)
			ignoreDistance = 500.0f * 500.0f;			// 500 max range for beneficial aggro

		if (ignoreDistance > 0 && DistanceSquaredNoZ(cur->GetPosition(), caster->GetPosition()) > ignoreDistance)
		{
			++it;
			continue;
		}

		if ((cur->IsMezzed() || cur->IsStunned()) && hate > 1)		// mezzed & stunned NPCs only add a small amount of witness hate, as per patch note
		{
			cur->AddToHateList(caster, hate / 4);
		}
		else
		{
			cur->AddToHateList(caster, hate);
		}
		++it;
	}
}

void EntityList::OpenDoorsNear(NPC *who)
{

	for (auto it = door_list.begin();it != door_list.end(); ++it) {
		Doors *cdoor = it->second;
		if (!cdoor || cdoor->IsDoorOpen() || !cdoor->IsMoveable())
			continue;

		auto diff = who->GetPosition() - cdoor->GetPosition();
		float curdist = diff.x * diff.x + diff.y * diff.y;
		float max_dist = who->GetSize() >= 20 ? RuleR(Doors, LargeRaceRange) : RuleR(Doors, NormalRaceRange);
		float max_z = who->GetSize() >= 20 ? RuleR(Doors, LargeRaceZ) : RuleR(Doors, NormalRaceZ);

		if (diff.z * diff.z < max_z && curdist <= max_dist)
		{
			Log(Logs::Detail, Logs::Doors, "%s has opened door %s (%d) curdist is %0.2f z diff is %0.2f", who->GetName(), cdoor->GetDoorName(), cdoor->GetDoorID(), curdist, diff.z * diff.z);
			cdoor->NPCOpen(who);
		}
	}
}

void EntityList::OpenDoorsNearCoords(NPC *who, const glm::vec4& position)
{

	for (auto it = door_list.begin();it != door_list.end(); ++it) {
		Doors *cdoor = it->second;
		if (!cdoor || cdoor->IsDoorOpen() || !cdoor->IsMoveable())
			continue;

		auto diff = position - cdoor->GetPosition();
		float curdist = diff.x * diff.x + diff.y * diff.y;
		float max_dist = who->GetSize() >= 20 ? RuleR(Doors, LargeRaceRange) : RuleR(Doors, NormalRaceRange);
		float max_z = who->GetSize() >= 20 ? RuleR(Doors, LargeRaceZ) : RuleR(Doors, NormalRaceZ);

		if (diff.z * diff.z < max_z && curdist <= max_dist)
		{
			cdoor->NPCOpen(who);
		}
	}
}

void EntityList::SendAlarm(Trap *trap, Mob *currenttarget, uint8 kos)
{
	float preSquareDistance = trap->effectvalue * trap->effectvalue;

	for (auto it = npc_list.begin();it != npc_list.end(); ++it) {
		NPC *cur = it->second;

		auto diff = glm::vec3(cur->GetPosition()) - trap->m_Position;
		float curdist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

		if (cur->GetOwner() || cur->IsEngaged() || curdist > preSquareDistance )
			continue;

		if (kos) {
			uint8 factioncon = currenttarget->GetReverseFactionCon(cur);
			if (factioncon == FACTION_THREATENINGLY || factioncon == FACTION_SCOWLS) {
				cur->AddToHateList(currenttarget,1);
			}
		}
		else
			cur->AddToHateList(currenttarget,1);
	}
}

void EntityList::AddProximity(NPC *proximity_for)
{
	RemoveProximity(proximity_for->GetID());

	proximity_list.push_back(proximity_for);

	proximity_for->proximity = new NPCProximity; // deleted in NPC::~NPC
}

bool EntityList::RemoveProximity(uint16 delete_npc_id)
{
	auto it = std::find_if(proximity_list.begin(), proximity_list.end(),
			[delete_npc_id](const NPC *a) { return a->GetID() == delete_npc_id; });
	if (it == proximity_list.end())
		return false;

	proximity_list.erase(it);
	return true;
}

void EntityList::RemoveAllLocalities()
{
	proximity_list.clear();
}

struct quest_proximity_event {
	QuestEventID event_id;
	Client *client;
	NPC *npc;
	int area_id;
	int area_type;
};

void EntityList::ProcessMove(Client *c, const glm::vec3& location)
{
	if (proximity_list.empty() && area_list.empty())
		return;

	float last_x = c->ProximityX();
	float last_y = c->ProximityY();
	float last_z = c->ProximityZ();

	std::list<quest_proximity_event> events;
	for (auto iter = proximity_list.begin(); iter != proximity_list.end(); ++iter) {
		NPC *d = (*iter);
		NPCProximity *l = d->proximity;
		if (l == nullptr)
			continue;

		//check both bounding boxes, if either coords pairs
		//cross a boundary, send the event.
		bool old_in = true;
		bool new_in = true;
		if (last_x < l->min_x || last_x > l->max_x ||
				last_y < l->min_y || last_y > l->max_y ||
				last_z < l->min_z || last_z > l->max_z) {
			old_in = false;
		}
		if (location.x < l->min_x || location.x > l->max_x ||
				location.y < l->min_y || location.y > l->max_y ||
				location.z < l->min_z || location.z > l->max_z) {
			new_in = false;
		}

		if (old_in && !new_in) {
			quest_proximity_event evt;
			evt.event_id = EVENT_EXIT;
			evt.client = c;
			evt.npc = d;
			evt.area_id = 0;
			evt.area_type = 0;
			events.push_back(evt);
		} else if (new_in && !old_in) {
			quest_proximity_event evt;
			evt.event_id = EVENT_ENTER;
			evt.client = c;
			evt.npc = d;
			evt.area_id = 0;
			evt.area_type = 0;
			events.push_back(evt);
		}
	}

	for (auto iter = area_list.begin(); iter != area_list.end(); ++iter) {
		Area& a = (*iter);
		bool old_in = true;
		bool new_in = true;
		if (last_x < a.min_x || last_x > a.max_x ||
				last_y < a.min_y || last_y > a.max_y ||
				last_z < a.min_z || last_z > a.max_z) {
			old_in = false;
		}

		if (location.x < a.min_x || location.x > a.max_x ||
				location.y < a.min_y || location.y > a.max_y ||
				location.z < a.min_z || location.z > a.max_z ) {
			new_in = false;
		}

		if (old_in && !new_in) {
			//were in but are no longer.
			quest_proximity_event evt;
			evt.event_id = EVENT_LEAVE_AREA;
			evt.client = c;
			evt.npc = nullptr;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		} else if (!old_in && new_in) {
			//were not in but now are
			quest_proximity_event evt;
			evt.event_id = EVENT_ENTER_AREA;
			evt.client = c;
			evt.npc = nullptr;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		}
	}

	for (auto iter = events.begin(); iter != events.end(); ++iter) {
		quest_proximity_event& evt = (*iter);
		if (evt.npc) {
			std::vector<std::any> args;
			parse->EventNPC(evt.event_id, evt.npc, evt.client, "", 0, &args);
		} else {
			std::vector<std::any> args;
			args.push_back(&evt.area_id);
			args.push_back(&evt.area_type);
			parse->EventPlayer(evt.event_id, evt.client, "", 0, &args);
		}
	}
}

void EntityList::ProcessMove(NPC *n, float x, float y, float z)
{
	if (area_list.empty())
		return;

	float last_x = n->GetX();
	float last_y = n->GetY();
	float last_z = n->GetZ();

	std::list<quest_proximity_event> events;
	for (auto iter = area_list.begin(); iter != area_list.end(); ++iter) {
		Area& a = (*iter);
		bool old_in = true;
		bool new_in = true;
		if (last_x < a.min_x || last_x > a.max_x ||
				last_y < a.min_y || last_y > a.max_y ||
				last_z < a.min_z || last_z > a.max_z) {
			old_in = false;
		}

		if (x < a.min_x || x > a.max_x ||
				y < a.min_y || y > a.max_y ||
				z < a.min_z || z > a.max_z) {
			new_in = false;
		}

		if (old_in && !new_in) {
			//were in but are no longer.
			quest_proximity_event evt;
			evt.event_id = EVENT_LEAVE_AREA;
			evt.client = nullptr;
			evt.npc = n;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		} else if (!old_in && new_in) {
			//were not in but now are
			quest_proximity_event evt;
			evt.event_id = EVENT_ENTER_AREA;
			evt.client = nullptr;
			evt.npc = n;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		}
	}

	for (auto iter = events.begin(); iter != events.end(); ++iter) {
		quest_proximity_event& evt = (*iter);
		std::vector<std::any> args;
		args.push_back(&evt.area_id);
		args.push_back(&evt.area_type);
		parse->EventNPC(evt.event_id, evt.npc, evt.client, "", 0, &args);
	}
}

void EntityList::AddArea(int id, int type, float min_x, float max_x, float min_y,
		float max_y, float min_z, float max_z)
{
	RemoveArea(id);
	Area a;
	a.id = id;
	a.type = type;
	if (min_x > max_x) {
		a.min_x = max_x;
		a.max_x = min_x;
	} else {
		a.min_x = min_x;
		a.max_x = max_x;
	}

	if (min_y > max_y) {
		a.min_y = max_y;
		a.max_y = min_y;
	} else {
		a.min_y = min_y;
		a.max_y = max_y;
	}

	if (min_z > max_z) {
		a.min_z = max_z;
		a.max_z = min_z;
	} else {
		a.min_z = min_z;
		a.max_z = max_z;
	}

	area_list.push_back(a);
}

void EntityList::RemoveArea(int id)
{
	auto it = std::find_if(area_list.begin(), area_list.end(),
			[id](const Area &a) { return a.id == id; });
	if (it == area_list.end())
		return;

	area_list.erase(it);
}

void EntityList::ClearAreas()
{
	area_list.clear();
}

void EntityList::ProcessProximitySay(const char *Message, Client *c, uint8 language)
{
	if (!Message || !c || proximity_list.empty())
		return;

	auto iter = proximity_list.begin();
	for (; iter != proximity_list.end(); ++iter) {
		NPC *d = (*iter);
		NPCProximity *l = d->proximity;
		if (l == nullptr || !l->say)
			continue;

		if (c->GetX() < l->min_x || c->GetX() > l->max_x
				|| c->GetY() < l->min_y || c->GetY() > l->max_y
				|| c->GetZ() < l->min_z || c->GetZ() > l->max_z)
			continue;

		parse->EventNPC(EVENT_PROXIMITY_SAY, d, c, Message, language);
	}
}

bool EntityList::IsMobInZone(Mob *who)
{
	//We don't use mob_list.find(who) because this code needs to be able to handle dangling pointers for the quest code.
	auto it = mob_list.begin();
	while(it != mob_list.end()) {
		if(it->second == who) {
			return true;
		}
		++it;
	}

	auto enc_it = encounter_list.begin();
	while (enc_it != encounter_list.end()) {
		if (enc_it->second == who) {
			return true;
		}
		++enc_it;
	}

	return false;
}

/*
Code to limit the amount of certain NPCs in a given zone.
Primarily used to make a named mob unique within the zone, but written
to be more generic allowing limits larger than 1.

Maintain this stuff in a seperate list since the number
of limited NPCs will most likely be much smaller than the number
of NPCs in the entire zone.
*/
void EntityList::LimitAddNPC(NPC *npc)
{
	if (!npc)
		return;

	SpawnLimitRecord r;

	uint16 eid = npc->GetID();
	r.spawngroup_id = npc->GetSpawnGroupId();
	r.npc_type = npc->GetNPCTypeID();

	npc_limit_list[eid] = r;
}

void EntityList::LimitRemoveNPC(NPC *npc)
{
	if (!npc)
		return;

	uint16 eid = npc->GetID();
	npc_limit_list.erase(eid);
}

//check a limit over the entire zone.
//returns true if the limit has not been reached
bool EntityList::LimitCheckType(uint32 npc_type, int count)
{
	if (count < 1)
		return true;

	std::map<uint16, SpawnLimitRecord>::iterator cur,end;
	cur = npc_limit_list.begin();
	end = npc_limit_list.end();

	for (; cur != end; ++cur) {
		if (cur->second.npc_type == npc_type) {
			count--;
			if (count == 0) {
				return false;
			}
		}
	}
	return true;
}

//check limits on an npc type in a given spawn group.
//returns true if the limit has not been reached
bool EntityList::LimitCheckGroup(uint32 spawngroup_id, int count, uint16& total_count)
{
	if (count < 1)
		return true;

	std::map<uint16, SpawnLimitRecord>::iterator cur,end;
	cur = npc_limit_list.begin();
	end = npc_limit_list.end();

	for (; cur != end; ++cur) {
		if (cur->second.spawngroup_id == spawngroup_id) {
			++total_count;
			--count;
			if (count == 0) {
				return false;
			}
		}
	}
	return true;
}

//check limits on an npc type in a given spawn group, and
//checks limits on the entire zone in one pass.
//returns true if neither limit has been reached
bool EntityList::LimitCheckBoth(uint32 npc_type, uint32 spawngroup_id, int group_count, int type_count)
{
	if (group_count < 1 && type_count < 1)
		return true;

	std::map<uint16, SpawnLimitRecord>::iterator cur,end;
	cur = npc_limit_list.begin();
	end = npc_limit_list.end();

	for (; cur != end; ++cur) {
		if (cur->second.npc_type == npc_type) {
			type_count--;
			if (type_count == 0) {
				return false;
			}
		}
		if (cur->second.spawngroup_id == spawngroup_id) {
			group_count--;
			if (group_count == 0) {
				return false;
			}
		}
	}
	return true;
}

bool EntityList::LimitCheckName(const char *npc_name)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* npc = it->second;
		if (npc)
			if (strcasecmp(npc_name, npc->GetRawNPCTypeName()) == 0)
				return false;
		++it;
	}
	return true;
}

void EntityList::DestroyTempPets(Mob *owner)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID()) {
				n->Depop();
			}
		}
		++it;
	}
}

int16 EntityList::CountTempPets(Mob *owner)
{
	int16 count = 0;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID()) {
				count++;
			}
		}
		++it;
	}

	owner->SetTempPetCount(count);

	return count;
}

bool EntityList::GetZommPet(Mob *owner, NPC* &pet)
{
	int16 count = 0;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID() && n->GetRace() == Race::EyeOfZomm) {
				pet = it->second;
				return true;
			}
		}
		++it;
	}
	return false;
}

uint16 EntityList::GetSummonedPetID(Mob *summoner)
{
	auto it = npc_list.begin();
	while (it != npc_list.end())
	{
		if (it->second->GetSummonerID() == summoner->GetID())
		{
			return it->second->GetID();
		}
		++it;
	}
	return 0;
}

void EntityList::ClearSummonedPetID(uint16 entityid)
{
	if (entityid == 0)
		return;

	auto it = npc_list.begin();
	while (it != npc_list.end())
	{
		if (it->second->GetSummonerID() == entityid)
		{
			it->second->SetSummonerID(0);
		}
		++it;
	}
}

void EntityList::AddTempPetsToHateList(Mob *owner, Mob* other, bool bFrenzy)
{
	if (!other || !owner)
		return;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID()) {
				n->CastToNPC()->hate_list.Add(other, 0, 0, bFrenzy);
			}
		}
		++it;
	}
}

bool Entity::CheckCoordLosNoZLeaps(float cur_x, float cur_y, float cur_z,
		float trg_x, float trg_y, float trg_z, float perwalk)
{
	if (zone->zonemap == nullptr)
		return true;

	glm::vec3 myloc;
	glm::vec3 oloc;
	glm::vec3 hit;

	myloc.x = cur_x;
	myloc.y = cur_y;
	myloc.z = cur_z+5;

	oloc.x = trg_x;
	oloc.y = trg_y;
	oloc.z = trg_z+5;

	if (myloc.x == oloc.x && myloc.y == oloc.y && myloc.z == oloc.z)
		return true;

	if (!zone->zonemap->LineIntersectsZoneNoZLeaps(myloc,oloc,perwalk,&hit))
		return true;
	return false;
}

Corpse *EntityList::GetClosestCorpse(Mob *sender, const char *Name)
{
	if (!sender)
		return nullptr;

	uint32 CurrentDistance, ClosestDistance = 4294967295u;
	Corpse *CurrentCorpse, *ClosestCorpse = nullptr;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		CurrentCorpse = it->second;

		++it;

		if (Name && strcasecmp(CurrentCorpse->GetOwnerName(), Name))
			continue;

		CurrentDistance = ((CurrentCorpse->GetY() - sender->GetY()) * (CurrentCorpse->GetY() - sender->GetY())) +
					((CurrentCorpse->GetX() - sender->GetX()) * (CurrentCorpse->GetX() - sender->GetX()));

		if (CurrentDistance < ClosestDistance) {
			ClosestDistance = CurrentDistance;

			ClosestCorpse = CurrentCorpse;

		}
	}
	return ClosestCorpse;
}

void EntityList::ForceGroupUpdate(uint32 gid)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second){
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					database.RefreshGroupFromDB(it->second);
				}
			}
		}
		++it;
	}
}

void EntityList::SendGroupLeave(uint32 gid, const char *name, bool checkleader)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		if (c) {
			Group *g = nullptr;
			g = c->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					strcpy(gj->membername, name);
					gj->action = groupActLeave;
					strcpy(gj->yourname, c->GetName());
					c->QueuePacket(outapp);
					safe_delete(outapp);
					g->DelMemberOOZ(name, checkleader);
				}
			}
		}
		++it;
	}
}

void EntityList::SendGroupLeader(uint32 gid, const char *lname, const char *oldlname, bool leaderset)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second){
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					auto outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					gj->action = groupActMakeLeader;
					strcpy(gj->membername, lname);
					strcpy(gj->yourname, oldlname);
					it->second->QueuePacket(outapp);
					Log(Logs::Detail, Logs::Group, "SendGroupLeader(): Entity loop leader update packet sent to: %s .", it->second->GetName());
					g->SetLeaderName(lname);
					g->SetOldLeaderName(lname);
					safe_delete(outapp);
				}
			}
		}
		++it;
	}

	if (!leaderset)
	{
		Client* newleader = entity_list.GetClientByName(lname);
		if (newleader)
		{
			Group *g = newleader->GetGroup();
			if (g)
			{
				if (g->GetID() == gid)
				{
					g->SetLeader(newleader);
					database.SetGroupLeaderName(g->GetID(), newleader->GetName());
					database.SetGroupOldLeaderName(g->GetID(), newleader->GetName());
					Log(Logs::Detail, Logs::Group, "Out of zone leader for group %d set to %s", g->GetID(), newleader->GetName());
				}
			}
		}
	}
}

void EntityList::SendGroupJoin(uint32 gid, const char *name)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second){
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					strcpy(gj->membername, name);
					gj->action = groupActJoin;
					strcpy(gj->yourname, it->second->GetName());
					Mob *Leader = g->GetLeader();
					it->second->QueuePacket(outapp);
					safe_delete(outapp);
				}
			}
		}
		++it;
	}
}

void EntityList::GroupMessage(uint32 gid, const char *from, const char *message, uint8 language, uint8 lang_skill)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second) {
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid)
					it->second->ChannelMessageSend(from, it->second->GetName(), ChatChannel_Group, language, lang_skill, message);
			}
		}
		++it;
	}
}

uint16 EntityList::CreateGroundObject(uint32 itemid, const glm::vec4& position, uint32 decay_time)
{
	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		return 0;
	}

	const EQ::ItemData *is = database.GetItem(itemid);
	if (!is)
		return 0;

	auto i = new EQ::ItemInstance(is, is->MaxCharges);
	if (!i)
		return 0;

	auto object = new Object(i, position.x, position.y, position.z, position.w, decay_time);
	entity_list.AddObject(object, true);

	safe_delete(i);
	if (!object)
		return 0;

	return object->GetID();
}

uint16 EntityList::CreateGroundObjectFromModel(const char *model, const glm::vec4& position, uint8 type, uint32 decay_time)
{
	if (!model)
		return 0;

	auto object = new Object(model, position.x, position.y, position.z, position.w, type);
	entity_list.AddObject(object, true);

	if (!object)
		return 0;

	return object->GetID();
}

uint16 EntityList::CreateDoor(const char *model, const glm::vec4& position, uint8 opentype, uint16 size)
{
	if (!model)
		return 0; // fell through everything, this is bad/incomplete from perl

	auto door = new Doors(model, position, opentype, size);
	RemoveAllDoors();
	zone->LoadZoneDoors();
	entity_list.AddDoor(door);
	entity_list.RespawnAllDoors();

	if (door)
		return door->GetEntityID();

	return 0; // fell through everything, this is bad/incomplete from perl
}


Mob *EntityList::GetTargetForMez(Mob *caster)
{
	if (!caster)
		return nullptr;

	auto it = mob_list.begin();
	//TODO: make this smarter and not mez targets being damaged by dots
	while (it != mob_list.end()) {
		Mob *d = it->second;
		if (d) {
			if (d == caster) { //caster can't pick himself
				++it;
				continue;
			}

			if (caster->GetTarget() == d) { //caster can't pick his target
				++it;
				continue;
			}

			if (!caster->CheckAggro(d)) { //caster can't pick targets that aren't aggroed on himself
				++it;
				continue;
			}

			if (DistanceSquared(caster->GetPosition(), d->GetPosition()) > 22250) { //only pick targets within 150 range
				++it;
				continue;
			}

			if (!caster->CheckLosFN(d)) {	//this is wasteful but can't really think of another way to do it
				++it;						//that wont have us trying to los the same target every time
				continue;					//it's only in combat so it's impact should be minimal.. but stil.
			}
			return d;
		}
		++it;
	}
	return nullptr;
}

void EntityList::SendZoneAppearance(Client *c)
{
	if (!c)
		return;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *cur = it->second;

		if (cur) {
			if (cur == c) {
				++it;
				continue;
			}
			if (cur->GetAppearance() != eaStanding) {
				cur->SendAppearancePacket(AppearanceType::Animation, cur->GetAppearanceValue(cur->GetAppearance()), false, true, c);
			}
			if (cur->GetSize() != cur->GetBaseSize()) {
				uint32 newsize = floor(cur->GetSize() + 0.5);
				cur->SendAppearancePacket(AppearanceType::Size, newsize, false, true, c);
			}
		}
		++it;
	}
}

uint32 EntityList::CheckNPCsClose(Mob *center)
{
	uint32 count = 0;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *cur = it->second;
		if (!cur || cur == center || cur->IsPet() || cur->IsUnTargetable()) {
			++it;
			continue;
		}

		float xDiff = cur->GetX() - center->GetX();
		float yDiff = cur->GetY() - center->GetY();
		float zDiff = cur->GetZ() - center->GetZ();
		float dist = ((xDiff * xDiff) + (yDiff * yDiff) + (zDiff * zDiff));

		++it;
	}
	return count;
}

void EntityList::GateAllClients()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		if (c)
			c->GoToBind();
		++it;
	}
}

void EntityList::SignalAllClients(uint32 data)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;
		if (ent)
			ent->Signal(data);
		++it;
	}
}

uint16 EntityList::GetClientCount()
{
	return client_list.size();
}

void EntityList::GetMobList(std::list<Mob *> &m_list)
{
	m_list.clear();
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		m_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetNPCList(std::list<NPC *> &n_list)
{
	n_list.clear();
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		n_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetClientList(std::list<Client *> &c_list)
{
	c_list.clear();
	auto it = client_list.begin();
	while (it != client_list.end()) {
		c_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetCorpseList(std::list<Corpse *> &c_list)
{
	c_list.clear();
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		c_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetObjectList(std::list<Object *> &o_list)
{
	o_list.clear();
	auto it = object_list.begin();
	while (it != object_list.end()) {
		o_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetDoorsList(std::list<Doors*> &o_list)
{
	o_list.clear();
	auto it = door_list.begin();
	while (it != door_list.end()) {
		o_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetSpawnList(std::list<Spawn2*> &o_list)
{
	o_list.clear();
	if(zone) {
		LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
		iterator.Reset();
		while(iterator.MoreElements())
		{
			Spawn2 *ent = iterator.GetData();
			o_list.push_back(ent);
			iterator.Advance();
		}
	}
}

void EntityList::UpdateQGlobal(uint32 qid, QGlobal newGlobal)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *ent = it->second;

		if (ent->IsClient()) {
			QGlobalCache *qgc = ent->CastToClient()->GetQGlobals();
			if (qgc) {
				uint32 char_id = ent->CastToClient()->CharacterID();
				if (newGlobal.char_id == char_id && newGlobal.npc_id == 0)
					qgc->AddGlobal(qid, newGlobal);
			}
		} else if (ent->IsNPC()) {
			QGlobalCache *qgc = ent->CastToNPC()->GetQGlobals();
			if (qgc) {
				uint32 npc_id = ent->GetNPCTypeID();
				if (newGlobal.npc_id == npc_id)
					qgc->AddGlobal(qid, newGlobal);
			}
		}
		++it;
	}
}

void EntityList::DeleteQGlobal(std::string name, uint32 npcID, uint32 charID, uint32 zoneID)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *ent = it->second;

		if (ent->IsClient()) {
			QGlobalCache *qgc = ent->CastToClient()->GetQGlobals();
			if (qgc)
				qgc->RemoveGlobal(name, npcID, charID, zoneID);
		} else if (ent->IsNPC()) {
			QGlobalCache *qgc = ent->CastToNPC()->GetQGlobals();
			if (qgc)
				qgc->RemoveGlobal(name, npcID, charID, zoneID);
		}
		++it;
	}
}

void EntityList::HideCorpses(Client *c, uint8 CurrentMode, uint8 NewMode)
{
	if (!c)
		return;

	if (NewMode == HideCorpseNone) {
		SendZoneCorpses(c);
		return;
	}

	Group *g = nullptr;

	if (NewMode == HideCorpseAllButGroup) {
		g = c->GetGroup();

		if (!g)
			NewMode = HideCorpseAll;
	}
	EQApplicationPacket outapp;
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		Corpse *b = it->second;

		if (b && (b->GetCharID() != c->CharacterID())) {
			if ((NewMode == HideCorpseAll) || ((NewMode == HideCorpseNPC) && (b->IsNPCCorpse()))) {
				b->CreateDespawnPacket(&outapp, false);
				c->QueuePacket(&outapp);
				safe_delete_array(outapp.pBuffer);
			} else if(NewMode == HideCorpseAllButGroup) {
				if (!g->IsGroupMember(b->GetOwnerName())) {
					b->CreateDespawnPacket(&outapp, false);
					c->QueuePacket(&outapp);
					safe_delete_array(outapp.pBuffer);
				} else if((CurrentMode == HideCorpseAll)) {
					b->CreateSpawnPacket(&outapp);
					c->QueuePacket(&outapp);
					safe_delete_array(outapp.pBuffer);
				}
			}
		}
		++it;
	}
}

void EntityList::AddLootToNPCS(uint32 item_id, uint32 count)
{
	if (count == 0)
		return;

	int npc_count = 0;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (!it->second->IsPet()
				&& !it->second->IsUnTargetable())
			npc_count++;
		++it;
	}

	if (npc_count == 0)
		return;

	auto npcs = new NPC *[npc_count];
	auto counts = new int[npc_count];
	auto marked = new bool[npc_count];
	memset(counts, 0, sizeof(int) * npc_count);
	memset(marked, 0, sizeof(bool) * npc_count);

	int i = 0;
	it = npc_list.begin();
	while (it != npc_list.end()) {
		if (!it->second->IsPet()
				&& !it->second->IsUnTargetable())
			npcs[i++] = it->second;
		++it;
	}

	while (count > 0) {
		std::vector<int> selection;
		selection.reserve(npc_count);
		for (int j = 0; j < npc_count; ++j)
			selection.push_back(j);

		while (!selection.empty() && count > 0) {
			int k = zone->random.Int(0, selection.size() - 1);
			counts[selection[k]]++;
			count--;
			selection.erase(selection.begin() + k);
		}
	}

	for (int j = 0; j < npc_count; ++j)
		if (counts[j] > 0)
			for (int k = 0; k < counts[j]; ++k)
				npcs[j]->AddItem(item_id, 1);

	safe_delete_array(npcs);
	safe_delete_array(counts);
	safe_delete_array(marked);
}

NPC *EntityList::GetClosestBanker(Mob *sender, uint32 &distance)
{
	if (!sender)
		return nullptr;

	distance = 4294967295u;
	NPC *nc = nullptr;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->GetClass() == Class::Banker) {
			uint32 nd = ((it->second->GetY() - sender->GetY()) * (it->second->GetY() - sender->GetY())) +
				((it->second->GetX() - sender->GetX()) * (it->second->GetX() - sender->GetX()));
			if (nd < distance){
				distance = nd;
				nc = it->second;
			}
		}
		++it;
	}
	return nc;
}

Mob *EntityList::GetClosestMobByBodyType(Mob *sender, uint8 BodyType)
{

	if (!sender)
		return nullptr;

	uint32 CurrentDistance, ClosestDistance = 4294967295u;
	Mob *CurrentMob, *ClosestMob = nullptr;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		CurrentMob = it->second;
		++it;

		if (CurrentMob->GetBodyType() != BodyType)
			continue;

		CurrentDistance = ((CurrentMob->GetY() - sender->GetY()) * (CurrentMob->GetY() - sender->GetY())) +
					((CurrentMob->GetX() - sender->GetX()) * (CurrentMob->GetX() - sender->GetX()));

		if (CurrentDistance < ClosestDistance) {
			ClosestDistance = CurrentDistance;
			ClosestMob = CurrentMob;
		}
	}
	return ClosestMob;
}

Mob *EntityList::GetClosestClient(Mob *sender, uint32 &distance)
{
	if (!sender)
		return nullptr;

	distance = 4294967295u;
	Mob *nc = nullptr;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->IsClient() || (it->second->GetOwner() && it->second->GetOwner()->IsClient())) 
		{
			uint32 nd = ((it->second->GetY() - sender->GetY()) * (it->second->GetY() - sender->GetY())) +
				((it->second->GetX() - sender->GetX()) * (it->second->GetX() - sender->GetX()));
			if (nd < distance){
				distance = nd;
				nc = it->second;
			}
		}
		++it;
	}
	return nc;
}

Mob *EntityList::GetClosestPlayer(Mob *sender)
{
	if (!sender)
		return nullptr;

	uint32 distance = 4294967295u;
	Mob *nc = nullptr;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->IsClient())
		{
			uint32 nd = ((it->second->GetY() - sender->GetY()) * (it->second->GetY() - sender->GetY())) +
				((it->second->GetX() - sender->GetX()) * (it->second->GetX() - sender->GetX()));
			if (nd < distance) {
				distance = nd;
				nc = it->second;
			}
		}
		++it;
	}
	return nc;
}

Client *EntityList::FindCorpseDragger(uint16 CorpseID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->IsDraggingCorpse(CorpseID))
			return it->second;
		++it;
	}
	return nullptr;
}

Mob *EntityList::GetNearestNPC(Mob *mob, bool excludePets, bool friendlyOnly, uint8 npcClass)
{
	if (!mob || (friendlyOnly && !mob->GetPrimaryFaction()))
		return (nullptr);

	float closest = 25000000; // Max range of 5000
	float e_dist = 5000;
	float t1, t2, m_dist;
	NPC* closest_npc = nullptr;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC* npc = it->second;
		if (!npc || npc == mob || (excludePets && npc->GetOwnerID()))
			continue;

		// if x or y distance > e_dist, then skip.
		t1 = npc->GetX() - mob->GetX();
		if(t1 < 0)
			t1 = 0 - t1;
		if (t1 > e_dist)
			continue;

		t2 = npc->GetY() - mob->GetY();
		if(t2 < 0)
			t2 = 0 - t2;
		if (t2 > e_dist)
			continue;

		m_dist = DistanceSquaredNoZ(mob->GetPosition(), npc->GetPosition());

		if (m_dist > closest)
			continue;

		if (npcClass && npcClass != npc->GetClass())
			continue;

		if (friendlyOnly && npc->GetReverseFactionCon(mob) > FACTION_KINDLY)
			continue;

		closest_npc = npc;
		closest = m_dist;

		// quick estimate of sqrt (within approximately 12%)
		if (t1 > t2)
			e_dist = t1 + t2 * 0.5;
		else
			e_dist = t2 + t1 * 0.5;
	}
	if (closest_npc)
		return (closest_npc->CastToMob());
	else
		return (nullptr);
}

// get all NPCs within dist range of mob and push into npcList.  npcClass == 0 means all classes (default)
void EntityList::GetNearestNPCs(Mob *mob, std::list<Mob*> &npcList, float dist, bool excludePets, bool friendlyOnly, uint8 npcClass)
{
	npcList.clear();

	if (!mob || (friendlyOnly && !mob->GetPrimaryFaction()))
		return;

	float t1, t2, m_dist;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC* npc = it->second;
		if (!npc || npc == mob || (excludePets && npc->GetOwnerID()))
			continue;

		t1 = npc->GetX() - mob->GetX();
		if (t1 > dist)
			continue;
		if (t1 < -dist)
			continue;

		t2 = npc->GetY() - mob->GetY();
		if (t2 > dist)
			continue;
		if (t2 < -dist)
			continue;

		if (npcClass && npcClass != npc->GetClass())
			continue;

		if (friendlyOnly && npc->GetReverseFactionCon(mob) > FACTION_KINDLY)
			continue;

		m_dist = DistanceSquaredNoZ(mob->GetPosition(), npc->GetPosition());

		if (m_dist > dist*dist)
			continue;

		npcList.push_back(npc);
	}
}

void EntityList::SendClientAppearances(Client *to_client)
{
	for(auto &it : client_list)
	{
		Client *c = it.second;

		if (c->IsLFG())
		{
			auto outapp = new EQApplicationPacket(OP_LFGCommand, sizeof(LFG_Appearance_Struct));
			LFG_Appearance_Struct *lfga = (LFG_Appearance_Struct *)outapp->pBuffer;
			lfga->entityid = c->GetID();
			lfga->value = c->IsLFG();

			to_client->QueuePacket(outapp);
			safe_delete(outapp);
		}

		int levitate_value = c->GetFlyMode() ? c->GetFlyMode() : (c->FindType(SE_Levitate) ? 2 : 0);
		if (levitate_value)
		{
			c->SendAppearancePacket(AppearanceType::FlyMode, levitate_value, false, true, to_client);
		}
	}
}

void EntityList::StopMobAI()
{
	for (auto &mob : mob_list) {
		mob.second->AI_Stop();
		mob.second->AI_ShutDown();
	}
}

void EntityList::GetBoatInfo(Client* client)
{
	uint8 count = 0;
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		// We don't want to include player controlled boats.
		if (it->second->IsNPC() && (it->second->GetBaseRace() == SHIP || it->second->GetBaseRace() == LAUNCH || it->second->GetBaseRace() == GHOST_SHIP))
		{
			// Have to use NPCID, because the EntityID will change when the player zones.
			uint8 passengers = GetClientCountByBoatNPCID(it->second->GetNPCTypeID());
			client->Message(Chat::White, " Boat: %s (%d) found at %0.2f,%0.2f,%0.2f. Waypoint: %d Passengers: %d", it->second->GetName(), it->second->GetNPCTypeID(), it->second->GetX(), it->second->GetY(), it->second->GetZ(), it->second->GetCurWp()+1, passengers);
			++count;
		}
		++it;
	}

	client->Message(Chat::White, "%d boats found.", count);
}

uint8 EntityList::GetClientCountByBoatNPCID(uint32 boatid)
{
	uint8 count = 0;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->GetBoatNPCID() == boatid)
			++count;
		++it;
	}
	return count;
}

uint8 EntityList::GetClientCountByBoatID(uint32 boatid)
{
	uint8 count = 0;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->GetBoatID() == boatid)
			++count;
		++it;
	}
	return count;
}

void EntityList::SendMerchantEnd(Mob* merchant)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetMerchantSession() == merchant->GetID())
		{
			c->SendMerchantEnd();
		}
		++it;
	}

	return;
}

void EntityList::SendTraderEnd(Client* merchant)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetTraderSession() == merchant->GetID())
		{
			c->Message_StringID(Chat::White, zone->random.Int(1199, 1202));
			c->SendMerchantEnd();
			c->SetTraderSession(0);
		}
		++it;
	}

	return;
}

bool EntityList::TraderHasCustomer(Client* merchant)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetTraderSession() == merchant->GetID())
		{
			return true;
		}
		++it;
	}

	return false;
}

void EntityList::NukeTraderItem(Client* merchant, uint16 slotid)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetTraderSession() == merchant->GetID())
		{
			auto outapp = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
			Merchant_DelItem_Struct* tdis = (Merchant_DelItem_Struct*)outapp->pBuffer;
			tdis->playerid = 0;
			tdis->npcid = c->GetID();
			tdis->type=65;
			tdis->itemslot = slotid;

			Log(Logs::Detail, Logs::Bazaar, "Telling %s to remove item in slot %d.", c->GetName(), slotid);

			c->QueuePacket(outapp);
			safe_delete(outapp);
		}
		++it;
	}

	return;
}

void EntityList::NukeTraderItemByID(Client* merchant, TraderCharges_Struct* gis, uint32 ItemID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetTraderSession() == merchant->GetID())
		{
			auto outapp = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
			Merchant_DelItem_Struct* tdis = (Merchant_DelItem_Struct*)outapp->pBuffer;
			tdis->playerid = 0;
			tdis->npcid = c->GetID();
			tdis->type=65;
			
			for(int i = 0; i < 80; i++) {
				if(gis->ItemID[i] == ItemID) {
					tdis->itemslot = i;
					Log(Logs::Detail, Logs::Bazaar, "Telling %s to remove item %i.", c->GetName(), ItemID);

					c->QueuePacket(outapp);
				}
			}

			safe_delete(outapp);	
		}
		++it;
	}

	return;
}

void EntityList::SendTraderInventory(Client* merchant)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetTraderSession() == merchant->GetID())
		{
			c->BulkSendTraderInventory(merchant->CharacterID());
		}
		++it;
	}

	return;
}

void EntityList::SendTraderUpdateMessage(Client* merchant, const EQ::ItemData* item, uint8 message)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetTraderSession() == merchant->GetID())
		{
			if(message == 0)
				c->Message(Chat::Red, "The Trader has changed the price of %s.", item->Name);
			else if(message == 1)
				c->Message(Chat::Red, "The Trader has put up %s for sale.", item->Name);
			else
				c->Message(Chat::Red, "The Trader has withdrawn the %s from sale.", item->Name);
		}
		++it;
	}

	return;
}


void EntityList::SendDeletedSelfFoundMerchantInventory(Mob* merchant, int32 slotid, uint32 itemid)
{
	if (!merchant || !merchant->IsNPC())
		return;

	for (auto it = client_list.begin(); it != client_list.end(); ++it)
	{
		Client* c = it->second;
		if (c && c->IsSelfFoundAny() && c->GetMerchantSession() == merchant->GetID())
		{
			if (zone->GetSelfFoundPurchaseLimit(merchant->GetNPCTypeID(), itemid, c->CharacterID()) == 0) {
				auto delitempacket = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
				Merchant_DelItem_Struct* delitem = (Merchant_DelItem_Struct*)delitempacket->pBuffer;
				delitem->itemslot = slotid;
				delitem->npcid = merchant->GetID();
				delitem->playerid = c->GetID();
				delitempacket->priority = 6;
				c->QueuePacket(delitempacket);
				safe_delete(delitempacket);
			}
		}
	}
}

void EntityList::SendMerchantInventory(Mob* merchant, int32 slotid, bool isdelete)
{

	if(!merchant || !merchant->IsNPC())
		return;

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;

		if (!c)
			continue;

		if(c->GetMerchantSession() == merchant->GetID())
		{
			if(!isdelete)
			{
				c->BulkSendMerchantInventory(merchant->CastToNPC()->MerchantType, merchant->GetNPCTypeID());
			}
			else
			{
				auto delitempacket = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
				Merchant_DelItem_Struct* delitem = (Merchant_DelItem_Struct*)delitempacket->pBuffer;
				delitem->itemslot = slotid;
				delitem->npcid = merchant->GetID();
				delitem->playerid = c->GetID();
				delitempacket->priority = 6;
				c->QueuePacket(delitempacket);
				safe_delete(delitempacket);
			}
		}
		++it;
	}

	return;
}

void EntityList::AreaCastSpell(float minx, float miny, float maxx, float maxy, uint16 spellid, uint16 regeant_itemid)
{
	auto it = client_list.begin();
	while (it != client_list.end()) 
	{
		Client* client = it->second;
		if(client)
		{
			int16 slotid = INVALID_INDEX;
			if(regeant_itemid > 0)
			{
				slotid = client->GetInv().HasItem(regeant_itemid);
			}

			if (client->GetX() >= minx && client->GetX() <= maxx && client->GetY() >= miny && client->GetY() <= maxy)
			{
				if(regeant_itemid == 0 || slotid != INVALID_INDEX)
				{
					if(slotid != INVALID_INDEX)
					{
						client->DeleteItemInInventory(slotid);
					}
					client->SpellFinished(spellid, client);
				}
				else if(regeant_itemid > 0 && slotid == INVALID_INDEX)
				{
					client->Message_StringID(Chat::SpellFailure, MISSING_SPELL_COMP);
				}
			}
		}
		++it;
	}
	return;
}

void EntityList::RepopNPCsByNPCID(uint32 npcid)
{
	std::vector< int > entityids;
	auto it = npc_list.begin();
	while (it != npc_list.end())
	{
		NPC* current = it->second;
		if (current && current->GetNPCTypeID() == npcid)
		{
			entityids.push_back(current->GetID()); 
		}
		++it;
	}

	if (entityids.size() > 0)
	{
		auto nt = npc_list.begin();
		while (nt != npc_list.end())
		{
			NPC* current = nt->second;
			if (current)
			{
				for (unsigned int i = 0; i < entityids.size(); ++i)
				{
					if (entityids[i] == current->GetID())
					{
						Log(Logs::General, Logs::EQMac, "Repopping %s EntityID %d.", current->GetName(), current->GetID());
						current->ForceRepop();
						break;
					}
				}
			}
			++nt;
		}
	}

	entityids.clear();
}

bool EntityList::HasCharmedNPC()
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) 
	{
		if (it->second->IsCharmedPet())
			return true;
		++it;
	}
	return false;
}


void EntityList::EvacAllPlayers()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second && it->second->GetID() > 0) {

			it->second->MovePCGuildID(zone->GetZoneID(), GUILD_NONE, 0, 0, 0, 0, 0, EvacToSafeCoords);
		}
		++it;
	}
}

void EntityList::TogglePVPForQuake()
{
	if (!zone)
		return;

	if (zone && zone->last_quake_struct.quake_type == QuakeType::QuakePVP || zone && zone->IsPVPZone())
	{
		// update distances to us for clients.
		auto it = client_list.begin();
		// go through the list and update distances
		while (it != client_list.end()) {
			if (it->second && it->second->GetPVP() == 0) {

				it->second->SetPVP(2);
			}
			++it;
		}
		return;
	}
	else if (zone && !zone->IsPVPZone())
	{
		// update distances to us for clients.
		auto it = client_list.begin();
		// go through the list and update distances
		while (it != client_list.end()) {
			if (it->second && it->second->GetPVP() == 2) {

				it->second->SetPVP(0);
			}
			++it;
		}
	}
}

void EntityList::ReportUnderworldNPCs(Client* sendto, float min_z)
{
	float underworld_z = zone->newzone_data.underworld;

	sendto->Message(Chat::White, "Possible underworld NPCs:");
	uint16 count = 0;

	auto it = npc_list.begin();
	while (it != npc_list.end()) 
	{
		NPC* current = it->second;
		if (current)
		{
			glm::vec3 coords(current->GetX(), current->GetY(), current->GetZ());
			if (coords.z < min_z || coords.z < underworld_z)
			{
				uint32 type = Chat::Yellow;
				if (coords.z < underworld_z)
				{
					type = Chat::Red;
				}

				std::string spawn2 = current->GetSpawnedString();
				bool find_best_z = spawn2 == "Quest" || spawn2 == "Pet" || spawn2 == "Random" ? false : true;

				sendto->Message(type, "%s: %0.2f,%0.2f,%0.2f NPCID: %d Spawn2ID: %s (DB Z: %0.2f Underworld Z: %0.2f)", current->GetName(), coords.x, coords.y, coords.z, current->GetNPCTypeID(), spawn2.c_str(), min_z, underworld_z);
				if (type == Chat::Red && find_best_z)
				{
					float best_z = 0;
					if (zone->zonemap)
					{
						best_z = zone->zonemap->FindBestZ(coords, nullptr);
						if (best_z > underworld_z)
						{
							sendto->Message(type, "BestZ for %s is %0.2f", current->GetName(), best_z);
						}
					}
				}
				++count;
			}
		}
		++it;
	}

	sendto->Message(Chat::White, "%d NPCs found.", count);
	return;
}

// returns the number of NPCs that have this mob targeted
// note that NPCs with all top haters outside of ignore range will have a null target
uint16 EntityList::GetTopHateCount(Mob* targ)
{
	uint16 num = 0;
	if (npc_list.empty())
		return num;

	auto it = npc_list.begin();
	while (it != npc_list.end())
	{
		if (it->second->GetTarget() == targ)
			num++;
		++it;
	}
	return num;
}

void EntityList::ReloadMerchants() {
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC* cur = it->second;
		if (cur->MerchantType != 0) {
			zone->ClearMerchantLists();
			zone->LoadNewMerchantData(cur->MerchantType);
		}
	}
}
