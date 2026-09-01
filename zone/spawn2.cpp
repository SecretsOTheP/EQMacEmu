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
#include "../common/strings.h"
#include "../common/zone_store.h"
#include "../common/misc_functions.h"

#include "client.h"
#include "entity.h"
#include "spawn2.h"
#include "spawngroup.h"
#include "worldserver.h"
#include "zone.h"
#include "zonedb.h"
#include "../common/repositories/criteria/content_filter_criteria.h"
#include <limits.h>

extern EntityList entity_list;
extern Zone* zone;

extern WorldServer worldserver;

Spawn2::Spawn2(uint32 in_spawn2_id, uint32 spawngroup_id,
	float in_x, float in_y, float in_z, float in_heading,
	uint32 respawn, uint32 variance, uint32 timeleft, uint32 grid,
	uint16 in_cond_id, int16 in_min_value, bool in_enabled, EmuAppearance anim, 
	bool in_force_z, bool in_rand_spawn, bool in_raid_target_spawnpoint)
: timer(100000), killcount(0)
{
	spawn2_id = in_spawn2_id;
	spawngroup_id_ = spawngroup_id;
	x = in_x;
	y = in_y;
	z = in_z;
	heading = in_heading;
	respawn_ = respawn;
	variance_ = variance;
	grid_ = grid;
	condition_id = in_cond_id;
	condition_min_value = in_min_value;
	npcthis = nullptr;
	enabled = in_enabled;
	this->anim = anim;
	force_z = in_force_z;
	rand_spawn = in_rand_spawn;
	raid_target_spawnpoint = in_raid_target_spawnpoint;
	last_level_attempt = 0;
	last_instance_spawn_timer_override = 0;

	if(timeleft == 0xFFFFFFFF) 
	{
		//special disable timeleft
		timer.Disable();
	}
	else if(timeleft != 0)
	{
		//we have a timeleft from the DB or something
		timer.Start(timeleft);
	} 
	else 
	{
		SpawnGroup* sg = zone->spawn_group_list.GetSpawnGroup(spawngroup_id_);
		uint32 cur = 0;
		if (sg)
		{
			if (sg->despawn == 2 || sg->despawn == 4)
			{
				cur = despawnTimer(sg->despawn_timer);
			}
		}

		//no timeleft at all, reset to
		if (cur == 0)
			cur = resetTimer(true);

		timer.Start(cur);
		timer.Trigger();
	}
}

Spawn2::~Spawn2()
{
}

uint32 Spawn2::resetTimer(bool quake_repop)
{
	// Guild 1 raid targets are repopped by the quake system. Keep defeated
	// targets dormant between quakes while preserving normal guild-instance
	// overrides for Guild 2 and above.
	if (!quake_repop && zone && zone->GetGuildID() == 1 && raid_target_spawnpoint)
	{
		return UINT_MAX;
	}

	uint32 rspawn = respawn_ * 1000;

	if (variance_ != 0) {
		int var_over_2 = (variance_ * 1000) / 2;
		rspawn = zone->random.Int(rspawn - var_over_2, rspawn + var_over_2);

		//put a lower bound on it, not a lot of difference below 100, so set that as the bound.
		if(rspawn < 100)
			rspawn = 100;
	}


	if (RuleB(Quarm, EnableRespawnReductionSystem))
	{
		if (zone->IsReducedSpawnTimersZone())
		{
			if (zone->CanCastDungeon())
			{
				if (rspawn >= RuleI(Quarm, RespawnReductionDungeonHigherBoundMin) && rspawn <= RuleI(Quarm, RespawnReductionDungeonHigherBoundMax))
					rspawn = RuleI(Quarm, RespawnReductionDungeonHigherBound);
				else if (rspawn >= RuleI(Quarm, RespawnReductionDungeonLowerBoundMin) && rspawn <= RuleI(Quarm, RespawnReductionDungeonLowerBoundMax))
					rspawn = RuleI(Quarm, RespawnReductionDungeonLowerBound);
			}
			else
			{
				if (last_level_attempt != 0 && last_level_attempt < 15)
				{
					if (rspawn >= RuleI(Quarm, RespawnReductionHigherBoundMin) && rspawn <= RuleI(Quarm, RespawnReductionHigherBoundMax))
						rspawn = RuleI(Quarm, RespawnReductionHigherBound);
					else if (rspawn >= RuleI(Quarm, RespawnReductionLowerBoundMin) && rspawn <= RuleI(Quarm, RespawnReductionLowerBoundMax))
						rspawn = RuleI(Quarm, RespawnReductionLowerBound);
				}
			}
		}
	}

	if (zone->GetGuildID() != GUILD_NONE && zone->GetGuildID() != 1)
	{
		if (RuleB(Quarm, InstanceAlwaysHasMinimumSpawnTime))
		{
			if (last_instance_spawn_timer_override != 0)
				return last_instance_spawn_timer_override;

			if (rspawn < RuleI(Quarm, InstanceMinimumSpawnTime))
				rspawn = RuleI(Quarm, InstanceMinimumSpawnTime);
		}
	}
	else if(zone && zone->GetGuildID() == 1)
	{
		if (zone->GetZoneID() == Zones::CHARDOK && last_instance_spawn_timer_override == 1592000000)
		{
			return 1800000;
		}

		if (last_instance_spawn_timer_override != 0)
			return (int)((double)last_instance_spawn_timer_override * (double)zone->random.Real(1.0, 1.5));
	}

	return (rspawn);

}

uint32 Spawn2::despawnTimer(uint32 despawn_timer)
{
	uint32 dspawn = despawn_timer * 1000;

	if (variance_ != 0) {
		int var_over_2 = (variance_ * 1000) / 2;
		dspawn = zone->random.Int(dspawn - var_over_2, dspawn + var_over_2);

		//put a lower bound on it, not a lot of difference below 100, so set that as the bound.
		if(dspawn < 100)
			dspawn = 100;
	}

	return (dspawn);

}

bool Spawn2::Process() {
	IsDespawned = false;

	if(!Enabled())
		return true;

	if (raid_target_spawnpoint && zone->GetGuildID() == GUILD_NONE) {
		return true;
	}

	if (!RuleB(Quarm, EnableQuakes) && raid_target_spawnpoint && zone->GetGuildID() == 1) {
		return true;
	}

	//grab our spawn group
	SpawnGroup* spawn_group = zone->spawn_group_list.GetSpawnGroup(spawngroup_id_);

	if(NPCPointerValid() && (spawn_group && spawn_group->despawn == 0 || condition_id != 0))
		return true;

	if (timer.Check()) {
		timer.Disable();

		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Timer has triggered", spawn2_id);

		//first check our spawn condition, if this isnt active
		//then we reset the timer and try again next time.
		if (condition_id != SC_AlwaysEnabled
			&& !zone->spawn_conditions.Check(condition_id, condition_min_value)) {
			Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: spawning prevented by spawn condition %d", spawn2_id, condition_id);
			Reset();
			return(true);
		}

		/**
		* Wait for init grids timer because we bulk load this data before trying to fetch it individually
		 */
		if (spawn_group == nullptr && zone->GetInitgridsTimer().Check()) {
			database.LoadSpawnGroupsByID(spawngroup_id_, &zone->spawn_group_list);
			spawn_group = zone->spawn_group_list.GetSpawnGroup(spawngroup_id_);
		}

		if (spawn_group == nullptr) {
			Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Unable to locate spawn group %d. Disabling.", spawn2_id, spawngroup_id_);
			return false;
		}

		//have the spawn group pick an NPC for us
		uint32 rtime = 0;
		uint32 npcid = spawn_group->GetNPCType(rtime, spawn2_id);
		if (npcid == 0)
		{
			Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Spawn group %d did not yield an NPC! not spawning.", spawn2_id, spawngroup_id_);
			if (rtime > 0)
			{
				// rtime will only have a value if there is a spawngroup limit. In that case, we want every NPC to use the same spawn timer.
				Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Spawn group %d did not yield an NPC due a group limit!", spawn2_id, spawngroup_id_);
				zone->UpdateGroupTimers(spawn_group, rtime);
			}
			else
			{
				Reset();
			}
			return(true);
		}

		//try to find our NPC type.
		const NPCType* tmp = database.LoadNPCTypesData(npcid);
		if (tmp == nullptr) {
			LogSpawns("Spawn2 [{}]: Spawn group [{}] yeilded an invalid NPC type [{}]", spawn2_id, spawngroup_id_, npcid);
			Reset();	//try again later
			return(true);
		}

		if (tmp->npc_id == 0) {
			LogError("NPC type did not load for npc_id [{}]", npcid);
			return true;
		}

		if (raid_target_spawnpoint && zone->GetGuildID() == GUILD_NONE) {
			return true;
		}

		if (!RuleB(Quarm, EnableQuakes) && raid_target_spawnpoint && zone->GetGuildID() == 1) {
			timer.Start(60000);	//don't yield quake mobs when they're disabled.
			return true;
		}

		if (tmp->unique_spawn_by_name) {
			if (!entity_list.LimitCheckName(tmp->name)) {
				LogSpawns("Spawn2 [{}]: Spawn group [{}] yeilded NPC type [{}], which is unique and one already exists", spawn2_id, spawngroup_id_, npcid);
				timer.Start(5000);	//try again in five seconds.
				return(true);
			}
		}

		if (tmp->spawn_limit > 0) {
			if (!entity_list.LimitCheckType(npcid, tmp->spawn_limit)) {
				LogSpawns("Spawn2 [{}]: Spawn group [{}] yeilded NPC type [{}], which is over its spawn limit ([{}])", spawn2_id, spawngroup_id_, npcid, tmp->spawn_limit);
				timer.Start(5000);	//try again in five seconds.
				return(true);
			}
		}

		bool ignore_despawn = false;
		if (npcthis) {
			ignore_despawn = npcthis->IgnoreDespawn();
		}

		if (ignore_despawn) {
			return true;
		}

		if (spawn_group->despawn != 0 && condition_id == 0 && !ignore_despawn) {
			zone->Despawn(spawn2_id);
		}

		if (IsDespawned) {
			return true;
		}

		currentnpcid = npcid;

		// ignoring spawnpoint loc and using random loc for large outdoor zones; roambox NPCs are not supposed to be campable
		// a bit hackish but works
		bool open_outdoor_zone = (!zone->CanCastDungeon() && !zone->IsCity()) || zone->GetZoneID() == Zones::QEYNOS2 || zone->GetZoneID() == Zones::FREPORTE || zone->GetZoneID() == Zones::FREPORTW || zone->GetZoneID() == Zones::GFAYDARK;

		if (open_outdoor_zone == zone->GetZoneID() == Zones::KAEL || open_outdoor_zone == zone->GetZoneID() == Zones::MISCHIEFPLANE) {
			open_outdoor_zone = false;
		}

		if (rand_spawn || (spawn_group->roamdist && open_outdoor_zone))	{
			uint8 count = 0;
			z = BEST_Z_INVALID;
			while (z == BEST_Z_INVALID && count < 3) {
				x = zone->random.Real(spawn_group->roambox[1], spawn_group->roambox[0]);
				y = zone->random.Real(spawn_group->roambox[3], spawn_group->roambox[2]);
				glm::vec3 loc(x, y, 0);
				z = zone->zonemap->FindBestZ(loc, nullptr);
				++count;
			}

			if (z == BEST_Z_INVALID) {
				Log(Logs::General, Logs::Error, "Invalid BestZ for random spawn %d", spawn2_id);
				timer.Start(60000);	// try again later
				return false;
			}

			Log(Logs::General, Logs::Status, "Final coords for random spawn %d is %0.2f, %0.2f, %0.2f with timer %d and var %d", spawn2_id, x, y, z, respawn_, variance_);
		}

		glm::vec4 loc(x, y, z, FixHeading(heading));
		int starting_wp = 0;
		if (spawn_group->wp_spawns && grid_ > 0)
		{
			glm::vec4 wploc;
			starting_wp = database.GetRandomWaypointLocFromGrid(wploc, zone->GetZoneID(), grid_);
			if (wploc.x != 0.0f || wploc.y != 0.0f || wploc.z != 0.0f)
			{
				loc = wploc;
				loc.w = FixHeading(loc.w);
				Log(Logs::General, Logs::Spawns, "spawning at random waypoint #%i loc: (%.3f, %.3f, %.3f).", starting_wp, loc.x, loc.y, loc.z);
			}
		}

		NPC* npc = new NPC(tmp, this, loc, GravityBehavior::Water);

		npcthis = npc;
		npc->AddLootTable(false);
		if (npc->DropsGlobalLoot()) {
			npc->CheckGlobalLootTables();
		}
		npc->SetSpawnGroupId(spawngroup_id_);
		npc->SaveGuardPointAnim(anim);
		npc->SetAppearance((EmuAppearance)anim);
		last_level_attempt = tmp->level;
		last_instance_spawn_timer_override = tmp->instance_spawn_timer_override;
		entity_list.AddNPC(npc);
		//this limit add must be done after the AddNPC since we need the entity ID.
		entity_list.LimitAddNPC(npc);
		if (spawn_group->roamdist && spawn_group->roambox[0] && spawn_group->roambox[1] && spawn_group->roambox[2] && spawn_group->roambox[3] && spawn_group->delay && spawn_group->min_delay)
			npc->AI_SetRoambox(spawn_group->roambox[0], spawn_group->roambox[1], spawn_group->roambox[2], spawn_group->roambox[3], spawn_group->delay, spawn_group->min_delay);
		Log(Logs::General, Logs::Spawns, "Spawn2 %d: Group %d spawned %s (%d) at (%.3f, %.3f, %.3f).", spawn2_id, spawngroup_id_, npc->GetName(), npcid, loc.x, loc.y, loc.z);
		
		if(zone->GetGuildID() != 1 || zone->GetGuildID() == 1 && (last_instance_spawn_timer_override == 0 || zone->GetZoneID() == Zones::CHARDOK && last_instance_spawn_timer_override == 1592000000))
			LoadGrid(starting_wp);
	}
	return true;
}

void Spawn2::Disable(bool depop)
{
	if(depop && npcthis)
	{
		npcthis->Depop();
	}
	enabled = false;
}

void Spawn2::LoadGrid(int start_wp)
{
	if(!npcthis)
		return;
	if(grid_ < 1)
		return;
	if(!entity_list.IsMobInZone(npcthis))
		return;
	//dont set an NPC's grid until its loaded for them.
	npcthis->SetGrid(grid_);
	npcthis->AssignWaypoints(grid_, start_wp);
	Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Loading grid %d for %s; starting wp is %d", spawn2_id, grid_, npcthis->GetName(), start_wp);
}

uint16 Spawn2::GetGrid() {
	if(!npcthis)
		return 0;
	if(grid_ < 1)
		return 0;
	if(!entity_list.IsMobInZone(npcthis))
		return 0;

	return grid_;
}

/*
	All three of these actions basically say that the mob which was
	associated with this spawn point is no longer relavent.
*/
void Spawn2::Reset(uint32 rtime) {

	if (rtime > 0)
	{
		timer.Start(rtime);
	}
	else
	{
		timer.Start(resetTimer());
	}
	npcthis = nullptr;
	Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Spawn reset, repop in %d ms", spawn2_id, timer.GetRemainingTime());
}

void Spawn2::Depop() {
	timer.Disable();
	Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Spawn reset, repop disabled", spawn2_id);
	npcthis = nullptr;
}

void Spawn2::Repop(uint32 delay) {
	if (npcthis)
		npcthis->Depop();

	if (delay == 0) {
		timer.Trigger();
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Spawn reset, repop immediately.", spawn2_id);
	} else {
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Spawn reset for repop, repop in %d ms", spawn2_id, delay);
		timer.Start(delay);
	}
}

void Spawn2::ForceDespawn()
{
	SpawnGroup* sg = zone->spawn_group_list.GetSpawnGroup(spawngroup_id_);

	if (sg == nullptr)
		return;

	if(npcthis != nullptr)
	{
		if (npcthis->IgnoreDespawn())
			return;

		if(!npcthis->IsEngaged())
		{
			if(sg->despawn == 3 || sg->despawn == 4)
			{
				npcthis->Depop(true);
				IsDespawned = true;
				npcthis = nullptr;
			}
			else
			{
				npcthis->Depop(false);
				npcthis = nullptr;
			}
		}
	}

	uint32 cur = 100000;
	uint32 dtimer = sg->despawn_timer;

	if(sg->despawn == 1 || sg->despawn == 3)
	{
		cur = resetTimer();
	}

	if(sg->despawn == 2 || sg->despawn == 4)
	{
		cur = despawnTimer(dtimer);
	}

	Log(Logs::Detail, Logs::Spawns, "ForceDespawn: Spawn2 %d: Spawn group %d set despawn timer to %d ms.", spawn2_id, spawngroup_id_, cur);
	timer.Start(cur);
}

void Spawn2::ChangeDespawn(uint8 new_despawn, uint32 new_despawn_timer)
{
	if (!NPCPointerValid())
		return;

	SpawnGroup* sg = zone->spawn_group_list.GetSpawnGroup(spawngroup_id_);

	if (sg == nullptr)
		return;

	if (sg->despawn == new_despawn)
		return;

	timer.Disable();
	sg->despawn = new_despawn;

	if (sg->despawn != 0)
	{
		if(new_despawn_timer != 0)
			sg->despawn_timer = new_despawn_timer;
		uint32 cur = 100000;
		uint32 dtimer = sg->despawn_timer;

		if (sg->despawn == 1 || sg->despawn == 3)
		{
			cur = resetTimer();
		}

		if (sg->despawn == 2 || sg->despawn == 4)
		{
			cur = despawnTimer(dtimer);
		}

		Log(Logs::Detail, Logs::Spawns, "ChangeDespawn: Spawn2 %d: Spawn group %d set despawn timer to %d ms.", spawn2_id, spawngroup_id_, cur);
		timer.Start(cur);
	}
}

//resets our spawn as if we just died
void Spawn2::DeathReset(bool realdeath)
{
	//get our reset based on variance etc and store it locally
	uint32 cur = resetTimer();
	//set our timer to our reset local
	timer.Start(cur);

	npcthis = nullptr;

	if(realdeath) { killcount++; }

	//if we have a valid spawn id
	if(spawn2_id)
	{
		database.UpdateRespawnTime(spawn2_id, (cur/1000), zone ? zone->GetGuildID() : GUILD_NONE);
		Log(Logs::General, Logs::Spawns, "Spawn2 %d: Spawn reset by death, repop in %d ms", spawn2_id, timer.GetRemainingTime());
		//store it to database too
	}
}


//resets our spawn as if we just died
void Spawn2::QuakeReset()
{
	//get our reset based on variance etc and store it locally
	uint32 cur = resetTimer(true);
	//set our timer to our reset local
	timer.Start(cur);

	if (npcthis)
	{
		npcthis->Depop();
	}
	npcthis = nullptr;
	//if we have a valid spawn id
	if (spawn2_id)
	{
		database.UpdateRespawnTime(spawn2_id, (cur / 1000), zone ? zone->GetGuildID() : GUILD_NONE);
		Log(Logs::General, Logs::Spawns, "Spawn2 %d: Spawn reset by death, repop in %d ms", spawn2_id, timer.GetRemainingTime());
		//store it to database too
	}
}


bool ZoneDatabase::PopulateZoneSpawnListClose(uint32 zoneid, LinkedList<Spawn2*> &spawn2_list, const glm::vec4& client_position, uint32 repop_distance, uint32 guildid)
{
	std::unordered_map<uint32, uint32> spawn_times;

	float mob_distance = 0;

	timeval tv;
	gettimeofday(&tv, nullptr);

	std::string spawn_query = StringFormat(
		"SELECT "
		"respawn_times.id, "
		"respawn_times.`start`, "
		"respawn_times.duration "
		"FROM "
		"respawn_times WHERE respawn_times.guild_id = %lu", (unsigned long)guildid
		);
	auto results = QueryDatabase(spawn_query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		uint32 start_duration = atoi(row[1]) > 0 ? atoi(row[1]) : 0;
		uint32 end_duration = atoi(row[2]) > 0 ? atoi(row[2]) : 0;

		/* Our current time was expired */
		if ((start_duration + end_duration) <= tv.tv_sec) {
			spawn_times[atoi(row[0])] = 0;
		}
		/* We still have time left on this timer */
		else {
			spawn_times[atoi(row[0])] = ((start_duration + end_duration) - tv.tv_sec) * 1000;
		}
	}

	const char *zone_name = ZoneName(zoneid);
	std::string query = StringFormat(
		"SELECT "
		"id, "
		"spawngroupID, "
		"x, "
		"y, "
		"z, "
		"heading, "
		"respawntime, "
		"variance, "
		"pathgrid, "
		"_condition, "
		"cond_value, "
		"enabled, "
		"animation, "
		"force_z, "
		"raid_target_spawnpoint "
		"FROM "
		"spawn2 "
		"AND ((%.2f >= min_expansion AND %.2f < max_expansion) OR (min_expansion = -1.0 AND max_expansion = -1.0))",
		zone_name,
		RuleR(World, CurrentExpansion),
		RuleR(World, CurrentExpansion)
		);
	results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	for (auto row = results.begin(); row != results.end(); ++row) {

		uint32 spawn_time_left = 0;
		Spawn2* new_spawn = 0;
		bool perl_enabled = atoi(row[11]) == 1 ? true : false;

		if (spawn_times.count(atoi(row[0])) != 0)
			spawn_time_left = spawn_times[atoi(row[0])];

		glm::vec4 point;
		point.x = atof(row[2]);
		point.y = atof(row[3]);

		mob_distance = DistanceNoZ(client_position, point);

		if (mob_distance > repop_distance)
			continue;

		new_spawn = new Spawn2(							   // 
			atoi(row[0]), 								   // uint32 in_spawn2_id
			atoi(row[1]), 								   // uint32 spawngroup_id
			atof(row[2]), 								   // float in_x
			atof(row[3]), 								   // float in_y
			atof(row[4]),								   // float in_z
			atof(row[5]), 								   // float in_heading
			atoi(row[6]), 								   // uint32 respawn
			atoi(row[7]), 								   // uint32 variance
			spawn_time_left,							   // uint32 timeleft
			atoi(row[8]),								   // uint32 grid
			atoi(row[9]), 								   // uint16 in_cond_id
			atoi(row[10]), 								   // int16 in_min_value
			perl_enabled, 								   // bool in_enabled
			(EmuAppearance)atoi(row[12]),				   // EmuAppearance anim
			atobool(row[13]),							   // bool force_z
			false,
			atobool(row[14])							   // bool raid_target_spawnpoint
			);

		spawn2_list.Insert(new_spawn);
	}

	return true;
}

bool ZoneDatabase::PopulateZoneSpawnList(uint32 zoneid, LinkedList<Spawn2*> &spawn2_list, uint32 guildid) {

	std::unordered_map<uint32, uint32> spawn_times;

	timeval tv;
	gettimeofday(&tv, nullptr);

	/* Bulk Load NPC Types Data into the cache*/
	database.LoadNPCTypesData(0, true);

	std::string spawn_query = StringFormat(
		"SELECT "
		"respawn_times.id, "
		"respawn_times.`start`, "
		"respawn_times.duration "
		"FROM "
		"respawn_times WHERE respawn_times.guild_id = %lu", (unsigned long)guildid
	);
	auto results = QueryDatabase(spawn_query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		uint32 start_duration = atoi(row[1]) > 0 ? atoi(row[1]) : 0;
		uint32 end_duration = atoi(row[2]) > 0 ? atoi(row[2]) : 0;

		/* Our current time was expired */
		if ((start_duration + end_duration) <= tv.tv_sec) {
			spawn_times[atoi(row[0])] = 0;
		}
		/* We still have time left on this timer */
		else {
			spawn_times[atoi(row[0])] = ((start_duration + end_duration) - tv.tv_sec) * 1000;
		}
	}

	const char *zone_name = ZoneName(zoneid);
	std::string query = fmt::format(
		"SELECT "
		"id, "
		"spawngroupID, "
		"x, "
		"y, "
		"z, "
		"heading, "
		"respawntime, "
		"variance, "
		"pathgrid, "
		"_condition, "
		"cond_value, "
		"enabled, "
		"animation, "
		"force_z, "
		"raid_target_spawnpoint "
		"FROM "
		"spawn2 "
		"WHERE TRUE {} AND zone = '{}'",
		ContentFilterCriteria::apply(),
		zone_name
	);
	results = QueryDatabase(query);

	if (!results.Success()) {
		return false;
	}

	for (auto row = results.begin(); row != results.end(); ++row) {

		uint32 spawn_time_left = 0; 
		Spawn2* new_spawn = 0; 
		bool perl_enabled = atoi(row[11]) == 1 ? true : false;

		if (spawn_times.count(atoi(row[0])) != 0)
			spawn_time_left = spawn_times[atoi(row[0])];

		new_spawn = new Spawn2(							   // 
			atoi(row[0]), 								   // uint32 in_spawn2_id
			atoi(row[1]), 								   // uint32 spawngroup_id
			atof(row[2]), 								   // float in_x
			atof(row[3]), 								   // float in_y
			atof(row[4]),								   // float in_z
			atof(row[5]), 								   // float in_heading
			atoi(row[6]), 								   // uint32 respawn
			atoi(row[7]), 								   // uint32 variance
			spawn_time_left,							   // uint32 timeleft
			atoi(row[8]),								   // uint32 grid
			atoi(row[9]), 								   // uint16 in_cond_id
			atoi(row[10]), 								   // int16 in_min_value
			perl_enabled, 								   // bool in_enabled
			(EmuAppearance)atoi(row[12]),				   // EmuAppearance anim
			atobool(row[13]),							   // bool force_z
			false,
			atobool(row[14])							   // bool raid_target_spawnpoint
		);

		spawn2_list.Insert(new_spawn);
	}

	return true;
}

bool ZoneDatabase::PopulateRandomZoneSpawnList(uint32 zoneid, LinkedList<Spawn2*> &spawn2_list, uint32 guild_id) 
{
	const char *zone_name = ZoneName(zoneid);
	uint32 next_id = RANDOM_SPAWNID + (zoneid * 1000);

	std::string query2 = fmt::format(
		"SELECT DISTINCT(spawngroupID), "
		"spawngroup.rand_spawns, "
		"spawngroup.rand_respawntime, "
		"spawngroup.rand_variance, "
		"spawngroup.rand_condition_ "
		"FROM spawn2, spawngroup "
		"WHERE spawn2.spawngroupID = spawngroup.ID "
		"AND TRUE {} AND zone = '{}' AND spawngroup.rand_spawns > 0 AND "
		"(spawngroup.max_x != 0 OR spawngroup.min_x != 0 OR spawngroup.max_y != 0 OR spawngroup.min_y != 0)",
		ContentFilterCriteria::apply(),
		zone_name
	);
	auto results = QueryDatabase(query2);

	if (!results.Success()) {
		return false;
	}

	uint16 count = 0;
	for (auto row = results.begin(); row != results.end(); ++row) 
	{
		uint8 rand_count = atoi(row[1]);
		if (rand_count > 0)
		{
			for (int i = 0; i < rand_count; ++i)
			{
				uint32 spawn2_id = next_id + count;
				if (spawn2_id > next_id + 999)
				{
					Log(Logs::General, Logs::Error, "Spawn2 ID %d is over the allocated limit for random box spawns in %s!", spawn2_id, zone_name);
					return false;
				}

				Spawn2* new_spawn = 0;
				new_spawn = new Spawn2(							   // 
					spawn2_id, 									   // uint32 in_spawn2_id
					atoi(row[0]), 								   // uint32 spawngroup_id
					0.0f, 										   // float in_x
					0.0f, 								           // float in_y
					0.0f,								           // float in_z
					0.0f, 										   // float in_heading
					atoi(row[2]), 								   // uint32 respawn
					atoi(row[3]), 								   // uint32 variance
					0,											   // uint32 timeleft
					0,											   // uint32 grid
					atoi(row[4]), 								   // uint16 in_cond_id
					1, 											   // int16 in_min_value
					true, 										   // bool in_enabled
					eaStanding,									   // EmuAppearance anim
					true,										   // bool force_z
					true,										   // bool rand_spawn		
					false										   // bool raid_target_spawnpoint
				);

				spawn2_list.Insert(new_spawn);
				++count;
			}
		}
	}

	if (count > 0)
	{
		// Since we have no way of knowing what the spawn2 id is at this point, we need to
		// clear any respawn timers whenever this method is called (zone bootup, #repop, #reloadworld)
		std::string query = StringFormat(
			"DELETE FROM respawn_times WHERE id >= %d && id <= %d and guild_id = %u", next_id, next_id + 999, guild_id);
		auto results1 = database.QueryDatabase(query);

		Log(Logs::General, Logs::Status, "%d random box spawns found and loaded!", count);
	}

	return true;
}

bool ZoneDatabase::CreateSpawn2(Client *client, uint32 spawngroup, const char* zone, const glm::vec4& position, uint32 respawn, uint32 variance, uint16 condition, int16 cond_value)
{

	std::string query = StringFormat("INSERT INTO spawn2 (spawngroupID, zone, x, y, z, heading, "
                                    "respawntime, variance, _condition, cond_value) "
                                    "VALUES (%i, '%s', %f, %f, %f, %f, %i, %i, %u, %i)",
                                    spawngroup, zone, position.x, position.y, position.z, position.w,
                                    respawn, variance, condition, cond_value);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return false;
    }

    if (results.RowsAffected() != 1)
        return false;

    return true;
}

uint32 Zone::CountSpawn2() {
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	uint32 count = 0;

	iterator.Reset();
	while(iterator.MoreElements())
	{
		count++;
		iterator.Advance();
	}
	return count;
}

void Zone::Despawn(uint32 spawn2ID) {
	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while(iterator.MoreElements()) {
		Spawn2 *cur = iterator.GetData();
		if(spawn2ID == cur->spawn2_id)
			cur->ForceDespawn();
		iterator.Advance();
	}
}

void Spawn2::SpawnConditionChanged(const SpawnCondition &c, int16 old_value) {
	if(GetSpawnCondition() != c.condition_id)
		return;

	Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Notified that our spawn condition %d has changed from %d to %d. Our min value is %d.", spawn2_id, c.condition_id, old_value, c.value, condition_min_value);

	bool old_state = (old_value >= condition_min_value);
	bool new_state = (c.value >= condition_min_value);
	if(old_state == new_state) {
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our threshold for this condition was not crossed. Doing nothing.", spawn2_id);
		return;	//no change
	}

	uint32 timer_remaining = 0;
	switch(c.on_change) {
	case SpawnCondition::DoNothing:
		//that was easy.
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our condition is now %s. Taking no action on existing spawn.", spawn2_id, new_state?"enabled":"disabled");
		break;
	case SpawnCondition::DoDepop:
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our condition is now %s. Depoping our mob.", spawn2_id, new_state?"enabled":"disabled");
		if(npcthis != nullptr)
			npcthis->Depop(false);	//remove the current mob
		Depop();
		break;
	case SpawnCondition::DoRepop:
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our condition is now %s. Forcing a repop.", spawn2_id, new_state?"enabled":"disabled");
		if(npcthis != nullptr)
			npcthis->Depop(false);	//remove the current mob
		Repop();	//repop
		break;
	case SpawnCondition::DoRepopIfReady:
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our condition is now %s. Forcing a repop if repsawn timer is expired.", spawn2_id, new_state?"enabled":"disabled");
		if(npcthis != nullptr) {
			Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our npcthis is currently not null. The zone thinks it is %s. Forcing a depop.", spawn2_id, npcthis->GetName());
			npcthis->Depop(false);	//remove the current mob
			npcthis = nullptr;
		}
		if(new_state) { // only get repawn timer remaining when the SpawnCondition is enabled.
			timer_remaining = database.GetSpawnTimeLeft(spawn2_id, zone ? zone->GetGuildID() : GUILD_NONE);
			Log(Logs::Detail, Logs::Spawns,"Spawn2 %d: Our condition is now %s. The respawn timer_remaining is %d. Forcing a repop if it is <= 0.", spawn2_id, new_state?"enabled":"disabled", timer_remaining);
			if(timer_remaining <= 0)
				Repop();
		} else {
			Log(Logs::Detail, Logs::Spawns,"Spawn2 %d: Our condition is now %s. Not checking respawn timer.", spawn2_id, new_state?"enabled":"disabled");
		}
		break;
	default:
		if(c.on_change < SpawnCondition::DoSignalMin) {
			Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our condition is now %s. Invalid on-change action %d.", spawn2_id, new_state?"enabled":"disabled", c.on_change);
			return;	//unknown onchange action
		}
		int signal_id = c.on_change - SpawnCondition::DoSignalMin;
		Log(Logs::Detail, Logs::Spawns, "Spawn2 %d: Our condition is now %s. Signaling our mob with %d.", spawn2_id, new_state?"enabled":"disabled", signal_id);
		if(npcthis != nullptr)
			npcthis->SignalNPC(signal_id);
	}
}

void Zone::SpawnConditionChanged(const SpawnCondition &c, int16 old_value) {
	Log(Logs::Detail, Logs::Spawns, "Zone notified that spawn condition %d has changed from %d to %d. Notifying all spawn points.", c.condition_id, old_value, c.value);

	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while(iterator.MoreElements()) {
		Spawn2 *cur = iterator.GetData();
		if(cur->GetSpawnCondition() == c.condition_id)
			cur->SpawnConditionChanged(c, old_value);
		iterator.Advance();
	}
}

SpawnCondition::SpawnCondition() {
	condition_id = 0;
	value = 0;
	on_change = DoNothing;
}

SpawnEvent::SpawnEvent() {
	id = 0;
	condition_id = 0;
	enabled = false;
	action = ActionSet;
	argument = 0;
	period = 0xFFFFFFFF;
	strict = false;
	memset(&next, 0, sizeof(next));
}

SpawnConditionManager::SpawnConditionManager()
 : minute_timer(3000)	//1 eq minute
{
	memset(&next_event, 0, sizeof(next_event));
}

void SpawnConditionManager::Process() {
	if(spawn_events.empty())
		return;

	if(minute_timer.Check()) {
		//check each spawn event.

		//get our current time
		TimeOfDay_Struct tod;
		zone->zone_time.GetCurrentEQTimeOfDay(&tod);

		//see if time is past our nearest event.
		if(EQTime::IsTimeBefore(&next_event, &tod))
			return;

		//at least one event should get triggered,
		std::vector<SpawnEvent>::iterator cur,end;
		cur = spawn_events.begin();
		end = spawn_events.end();
		for(; cur != end; ++cur) {
			SpawnEvent &cevent = *cur;

			if(cevent.enabled)
			{
				if(EQTime::IsTimeBefore(&tod, &cevent.next)) {
					//this event has been triggered.
					//execute the event
					uint8 min = cevent.next.minute + RuleI(Zone, SpawnEventMin);
					if(!cevent.strict || (cevent.strict && tod.minute < min && cevent.next.hour == tod.hour && cevent.next.day == tod.day && cevent.next.month == tod.month && cevent.next.year == tod.year))
						ExecEvent(cevent, true);
					else
						Log(Logs::Detail, Logs::Spawns, "Event %d: Is strict, ExecEvent is skipped.", cevent.id);

					//add the period of the event to the trigger time
					EQTime::AddMinutes(cevent.period, &cevent.next);
					std::string t;
					EQTime::ToString(&cevent.next, t);
					Log(Logs::Detail, Logs::Spawns, "Event %d: Will trigger again in %d EQ minutes at %s.", cevent.id, cevent.period, t.c_str());
					//save the next event time in the DB
					UpdateDBEvent(cevent);
					//find the next closest event timer.
					FindNearestEvent();
					//minor optimization, if theres no more possible events,
					//then stop trying... I dunno how worth while this is.
					if(EQTime::IsTimeBefore(&next_event, &tod))
						return;
				}
			}
		}
	}
}

void SpawnConditionManager::ExecEvent(SpawnEvent &event, bool send_update) {
	std::map<uint16, SpawnCondition>::iterator condi;
	condi = spawn_conditions.find(event.condition_id);
	if(condi == spawn_conditions.end()) {
		Log(Logs::Detail, Logs::Spawns, "Event %d: Unable to find condition %d to execute on.", event.id, event.condition_id);
		return;	//unable to find the spawn condition to operate on
	}

	TimeOfDay_Struct tod;
	zone->zone_time.GetCurrentEQTimeOfDay(&tod);
	//If we're here, strict has already been checked. Check again in case hour has changed.
	if(event.strict && (event.next.hour != tod.hour || event.next.day != tod.day || event.next.month != tod.month || event.next.year != tod.year))
	{
		Log(Logs::Detail, Logs::Spawns, "Event %d: Unable to execute. Condition is strict, and event time has already passed.", event.id);
		return;
	}

	SpawnCondition &cond = condi->second;

	int16 new_value = cond.value;

	//we have our event and our condition, do our stuff.
	switch(event.action) {
	case SpawnEvent::ActionSet:
		new_value = event.argument;
		Log(Logs::Detail, Logs::Spawns, "Event %d: Executing. Setting condition %d to %d.", event.id, event.condition_id, event.argument);
		break;
	case SpawnEvent::ActionAdd:
		new_value += event.argument;
		Log(Logs::Detail, Logs::Spawns, "Event %d: Executing. Adding %d to condition %d, yeilding %d.", event.id, event.argument, event.condition_id, new_value);
		break;
	case SpawnEvent::ActionSubtract:
		new_value -= event.argument;
		Log(Logs::Detail, Logs::Spawns, "Event %d: Executing. Subtracting %d from condition %d, yeilding %d.", event.id, event.argument, event.condition_id, new_value);
		break;
	case SpawnEvent::ActionMultiply:
		new_value *= event.argument;
		Log(Logs::Detail, Logs::Spawns, "Event %d: Executing. Multiplying condition %d by %d, yeilding %d.", event.id, event.condition_id, event.argument, new_value);
		break;
	case SpawnEvent::ActionDivide:
		new_value /= event.argument;
		Log(Logs::Detail, Logs::Spawns, "Event %d: Executing. Dividing condition %d by %d, yeilding %d.", event.id, event.condition_id, event.argument, new_value);
		break;
	default:
		Log(Logs::Detail, Logs::Spawns, "Event %d: Invalid event action type %d", event.id, event.action);
		return;
	}

	//now set the condition to the new value
	if(send_update)	//full blown update
		SetCondition(zone->GetShortName(), zone->GetGuildID(), cond.condition_id, new_value);
	else	//minor update done while loading
		cond.value = new_value;
}

void SpawnConditionManager::UpdateDBEvent(SpawnEvent &event) {

	std::string query = StringFormat("UPDATE spawn_events SET "
                                    "next_minute = %d, next_hour = %d, "
                                    "next_day = %d, next_month = %d, "
                                    "next_year = %d, enabled = %d, "
                                    "strict = %d WHERE id = %d",
                                    event.next.minute, event.next.hour,
                                    event.next.day, event.next.month,
                                    event.next.year, event.enabled? 1: 0,
                                    event.strict? 1: 0, event.id);
	database.QueryDatabase(query);
}

void SpawnConditionManager::UpdateDBCondition(const char* zone_name, uint32 instance_id, uint16 cond_id, int16 value) {

	std::string query = StringFormat("REPLACE INTO spawn_condition_values "
                                    "(id, value, zone, instance_id) "
                                    "VALUES( %u, %u, '%s', %lu)",
                                    cond_id, value, zone_name, instance_id);
    database.QueryDatabase(query);
}

bool SpawnConditionManager::LoadDBEvent(uint32 event_id, SpawnEvent &event, std::string &zone_name) {

    std::string query = StringFormat("SELECT id, cond_id, period, "
                                    "next_minute, next_hour, next_day, "
                                    "next_month, next_year, enabled, "
                                    "action, argument, strict, zone, instance_id "
                                    "FROM spawn_events WHERE id = %d", event_id);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
		return false;
	}

    if (results.RowCount() == 0)
        return false;

	auto row = results.begin();

    event.id = atoi(row[0]);
    event.condition_id = atoi(row[1]);
    event.period = atoi(row[2]);

    event.next.minute = atoi(row[3]);
    event.next.hour = atoi(row[4]);
    event.next.day = atoi(row[5]);
    event.next.month = atoi(row[6]);
    event.next.year = atoi(row[7]);

    event.enabled = atoi(row[8]) != 0;
    event.action = (SpawnEvent::Action) atoi(row[9]);
    event.argument = atoi(row[10]);
    event.strict = atoi(row[11]) != 0;
    zone_name = row[12];
	event.instance_id = atoi(row[13]);

    std::string timeAsString;
    EQTime::ToString(&event.next, timeAsString);

    Log(Logs::Detail, Logs::Spawns, "(LoadDBEvent) Loaded %s spawn event %d on condition %d with period %d, action %d, argument %d, strict %d. Will trigger at %s", event.enabled? "enabled": "disabled", event.id, event.condition_id, event.period, event.action, event.argument, event.strict, timeAsString.c_str());

	return true;
}

bool SpawnConditionManager::LoadSpawnConditions(const char* zone_name, uint32 instance_id)
{
	//clear out old stuff..
	spawn_conditions.clear();

	std::string query = StringFormat("SELECT id, onchange, value "
                                    "FROM spawn_conditions "
                                    "WHERE zone = '%s'", zone_name);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
		return false;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        //load spawn conditions
        SpawnCondition cond;

        cond.condition_id = atoi(row[0]);
        cond.value = atoi(row[2]);
        cond.on_change = (SpawnCondition::OnChange) atoi(row[1]);
        spawn_conditions[cond.condition_id] = cond;

        Log(Logs::Detail, Logs::Spawns, "Loaded spawn condition %d with value %d and on_change %d", cond.condition_id, cond.value, cond.on_change);
    }

	//load values
	query = StringFormat("SELECT id, value FROM spawn_condition_values "
                        "WHERE zone = '%s' AND instance_id = %lu",
                        zone_name, (unsigned long)instance_id);
    results = database.QueryDatabase(query);
    if (!results.Success()) {
		spawn_conditions.clear();
		return false;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        auto iter = spawn_conditions.find(atoi(row[0]));

        if(iter != spawn_conditions.end())
            iter->second.value = atoi(row[1]);
    }

	//load spawn events
    query = StringFormat("SELECT id, cond_id, period, next_minute, next_hour, "
                        "next_day, next_month, next_year, enabled, action, argument, strict "
                        "FROM spawn_events WHERE zone = '%s' AND instance_id = %lu", zone_name, (unsigned long)instance_id);
    results = database.QueryDatabase(query);
    if (!results.Success()) {
		return false;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        SpawnEvent event;

        event.id = atoi(row[0]);
        event.condition_id = atoi(row[1]);
        event.period = atoi(row[2]);

        if(event.period == 0) {
            Log(Logs::General, Logs::Error, "Refusing to load spawn event #%d because it has a period of 0\n", event.id);
            continue;
        }

        event.next.minute = atoi(row[3]);
        event.next.hour = atoi(row[4]);
        event.next.day = atoi(row[5]);
        event.next.month = atoi(row[6]);
        event.next.year = atoi(row[7]);

        event.enabled = atoi(row[8])==0?false:true;
        event.action = (SpawnEvent::Action) atoi(row[9]);
        event.argument = atoi(row[10]);
        event.strict = atoi(row[11])==0?false:true;

        spawn_events.push_back(event);

        Log(Logs::Detail, Logs::Spawns, "(LoadSpawnConditions) Loaded %s spawn event %d on condition %d with period %d, action %d, argument %d, strict %d", event.enabled? "enabled": "disabled", event.id, event.condition_id, event.period, event.action, event.argument, event.strict);
    }

	//now we need to catch up on events that happened while we were away
	//and use them to alter just the condition variables.

	//each spawn2 will then use its correct condition value when
	//it decides what to do. This essentially forces a 'depop' action
	//on spawn points which are turned off, and a 'repop' action on
	//spawn points which get turned on. Im too lazy to figure out a
	//better solution, and I just dont care thats much.
	//get our current time
	TimeOfDay_Struct tod;
	zone->zone_time.GetCurrentEQTimeOfDay(&tod);

	for(auto cur = spawn_events.begin(); cur != spawn_events.end(); ++cur) {
		SpawnEvent &cevent = *cur;

		bool StrictCheck = false;
		if(cevent.strict &&
			cevent.next.hour == tod.hour &&
			cevent.next.day == tod.day &&
			cevent.next.month == tod.month &&
			cevent.next.year == tod.year)
			StrictCheck = true;

		//If event is disabled, or we failed the strict check, set initial spawn_condition to 0.
		if(!cevent.enabled || !StrictCheck)
			SetCondition(zone->GetShortName(), zone->GetGuildID(), cevent.condition_id,0);

		if(!cevent.enabled)
            continue;

        //watch for special case of all 0s, which means to reset next to now
        if(cevent.next.year == 0 && cevent.next.month == 0 && cevent.next.day == 0 && cevent.next.hour == 0 && cevent.next.minute == 0) {
            Log(Logs::Detail, Logs::Spawns, "Initial next trigger time set for spawn event %d", cevent.id);
            memcpy(&cevent.next, &tod, sizeof(cevent.next));
            //add one period
            EQTime::AddMinutes(cevent.period, &cevent.next);
            //save it in the db.
            UpdateDBEvent(cevent);
            continue;	//were done with this event.
        }

        bool ran = false;
        while(EQTime::IsTimeBefore(&tod, &cevent.next)) {
            Log(Logs::Detail, Logs::Spawns, "Catch up triggering on event %d", cevent.id);
            //this event has been triggered.
            //execute the event
            if(!cevent.strict || StrictCheck)
                ExecEvent(cevent, false);

            //add the period of the event to the trigger time
            EQTime::AddMinutes(cevent.period, &cevent.next);
            ran = true;
        }

        //only write it out if the event actually ran
        if(ran)
            UpdateDBEvent(cevent); //save the event in the DB
	}

	//now our event timers are all up to date, find our closest event.
	FindNearestEvent();

	return true;
}

void SpawnConditionManager::FindNearestEvent() {
	//set a huge year which should never get reached normally
	next_event.year = 0xFFFF;

	std::vector<SpawnEvent>::iterator cur,end;
	cur = spawn_events.begin();
	end = spawn_events.end();
	int next_id = -1;
	for(; cur != end; ++cur) {
		SpawnEvent &cevent = *cur;
		if(cevent.enabled)
		{
			//see if this event is before our last nearest
			if(EQTime::IsTimeBefore(&next_event, &cevent.next))
			{
				memcpy(&next_event, &cevent.next, sizeof(next_event));
				next_id = cevent.id;
			}
		}
	}
	if (next_id == -1) {
		Log(Logs::Detail, Logs::Spawns, "No spawn events enabled. Disabling next event.");
	}
	else {
		Log(Logs::Detail, Logs::Spawns, "Next event determined to be event %d", next_id);
	}
}

void SpawnConditionManager::SetCondition(const char *zone_short, uint32 instance_id, uint16 condition_id, int16 new_value, bool world_update)
{
	if(world_update) {
		//this is an update coming from another zone, they
		//have allready updated the DB, just need to update our
		//memory, and check for condition changes
		std::map<uint16, SpawnCondition>::iterator condi;
		condi = spawn_conditions.find(condition_id);
		if(condi == spawn_conditions.end()) {
			Log(Logs::Detail, Logs::Spawns, "Condition update received from world for %d, but we do not have that conditon.", condition_id);
			return;	//unable to find the spawn condition
		}

		SpawnCondition &cond = condi->second;

		if(cond.value == new_value) {
			Log(Logs::Detail, Logs::Spawns, "Condition update received from world for %d with value %d, which is what we already have.", condition_id, new_value);
			return;
		}

		int16 old_value = cond.value;

		//set our local value
		cond.value = new_value;

		Log(Logs::Detail, Logs::Spawns, "Condition update received from world for %d with value %d", condition_id, new_value);

		//now we have to test each spawn point to see if it changed.
		zone->SpawnConditionChanged(cond, old_value);
	} else if(!strcasecmp(zone_short, zone->GetShortName()) && instance_id == zone->GetGuildID())
	{
		//this is a local spawn condition, we need to update the DB,
		//our memory, then notify spawn points of the change.
		std::map<uint16, SpawnCondition>::iterator condi;
		condi = spawn_conditions.find(condition_id);
		if(condi == spawn_conditions.end()) {
			Log(Logs::Detail, Logs::Spawns, "Local Condition update requested for %d, but we do not have that conditon.", condition_id);
			return;	//unable to find the spawn condition
		}

		SpawnCondition &cond = condi->second;

		if(cond.value == new_value) {
			Log(Logs::Detail, Logs::Spawns, "Local Condition update requested for %d with value %d, which is what we already have.", condition_id, new_value);
			return;
		}

		int16 old_value = cond.value;

		//set our local value
		cond.value = new_value;
		//save it in the DB too
		UpdateDBCondition(zone_short, instance_id, condition_id, new_value);

		Log(Logs::Detail, Logs::Spawns, "Local Condition update requested for %d with value %d", condition_id, new_value);

		//now we have to test each spawn point to see if it changed.
		zone->SpawnConditionChanged(cond, old_value);
	}
	else
	{
		//this is a remote spawn condition, update the DB and send
		//an update packet to the zone if its up

		Log(Logs::Detail, Logs::Spawns, "Remote spawn condition %d set to %d. Updating DB and notifying world.", condition_id, new_value);

		UpdateDBCondition(zone_short, instance_id, condition_id, new_value);

		auto pack = new ServerPacket(ServerOP_SpawnCondition, sizeof(ServerSpawnCondition_Struct));
		ServerSpawnCondition_Struct* ssc = (ServerSpawnCondition_Struct*)pack->pBuffer;

		ssc->zoneID = ZoneID(zone_short);
		ssc->zoneGuildID = zone->GetGuildID();
		ssc->condition_id = condition_id;
		ssc->value = new_value;

		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void SpawnConditionManager::ReloadEvent(uint32 event_id) {
	std::string zone_short_name;

	Log(Logs::Detail, Logs::Spawns, "Requested to reload event %d from the database.", event_id);

	//first look for the event in our local event list
	std::vector<SpawnEvent>::iterator cur,end;
	cur = spawn_events.begin();
	end = spawn_events.end();
	for(; cur != end; ++cur) {
		SpawnEvent &cevent = *cur;

		if(cevent.id == event_id) {
			//load the event into the old event slot
			if(!LoadDBEvent(event_id, cevent, zone_short_name)) {
				//unable to find the event in the database...
				Log(Logs::Detail, Logs::Spawns, "Failed to reload event %d from the database.", event_id);
				return;
			}
			//sync up our nearest event
			FindNearestEvent();
			return;
		}
	}

	//if we get here, it is a new event...
	SpawnEvent e;
	if(!LoadDBEvent(event_id, e, zone_short_name)) {
		//unable to find the event in the database...
		Log(Logs::Detail, Logs::Spawns, "Failed to reload event %d from the database.", event_id);
		return;
	}

	//we might want to check the next timer like we do on
	//regular load events, but we are assuming this is a new event
	//and anyways, this will get handled (albeit not optimally)
	//naturally by the event handling code.

	spawn_events.push_back(e);

	//sync up our nearest event
	FindNearestEvent();
}

void SpawnConditionManager::ToggleEvent(uint32 event_id, bool enabled, bool strict, bool reset_base) {

	Log(Logs::Detail, Logs::Spawns, "Request to %s spawn event %d %sresetting trigger time", enabled?"enable":"disable", event_id, reset_base?"":"without ");

	//first look for the event in our local event list
	std::vector<SpawnEvent>::iterator cur,end;
	cur = spawn_events.begin();
	end = spawn_events.end();
	for(; cur != end; ++cur) {
		SpawnEvent &cevent = *cur;

		if(cevent.id == event_id) {
			//make sure were actually changing something
			if(cevent.enabled != enabled || reset_base || cevent.strict != strict) {
				cevent.enabled = enabled;
				cevent.strict = strict;
				if(reset_base) {
					Log(Logs::Detail, Logs::Spawns, "Spawn event %d located in this zone. State set. Trigger time reset (period %d).", event_id, cevent.period);
					//start with the time now
					zone->zone_time.GetCurrentEQTimeOfDay(&cevent.next);
					//advance the next time by our period
					EQTime::AddMinutes(cevent.period, &cevent.next);
				} else {
					Log(Logs::Detail, Logs::Spawns, "Spawn event %d located in this zone. State changed.", event_id);
				}

				//save the event in the DB
				UpdateDBEvent(cevent);

				//sync up our nearest event
				FindNearestEvent();
			} else {
				Log(Logs::Detail, Logs::Spawns, "Spawn event %d located in this zone but no change was needed.", event_id);
			}
			//even if we dont change anything, we still found it
			return;
		}
	}

	//if we get here, the condition must be in another zone.
	//first figure out what zone it applies to.
	//update the DB and send and send an update packet to
	//the zone if its up

	//we need to load the event from the DB and then update
	//the values in the DB, because we do not know if the zone
	//is up or not. The message to the zone will just tell it to
	//update its in-memory event list
	SpawnEvent e;
	std::string zone_short_name;
	if(!LoadDBEvent(event_id, e, zone_short_name)) {
		Log(Logs::Detail, Logs::Spawns, "Unable to find spawn event %d in the database.", event_id);
		//unable to find the event in the database...
		return;
	}
	if(e.enabled == enabled && !reset_base) {
		Log(Logs::Detail, Logs::Spawns, "Spawn event %d is not located in this zone but no change was needed.", event_id);
		return;	//no changes.
	}

	e.enabled = enabled;
	if(reset_base) {
		Log(Logs::Detail, Logs::Spawns, "Spawn event %d is in zone %s. State set. Trigger time reset (period %d). Notifying world.", event_id, zone_short_name.c_str(), e.period);
		//start with the time now
		zone->zone_time.GetCurrentEQTimeOfDay(&e.next);
		//advance the next time by our period
		EQTime::AddMinutes(e.period, &e.next);
	} else {
		Log(Logs::Detail, Logs::Spawns, "Spawn event %d is in zone %s. State changed. Notifying world.", event_id, zone_short_name.c_str(), e.period);
	}
	//save the event in the DB
	UpdateDBEvent(e);


	//now notify the zone
	auto pack = new ServerPacket(ServerOP_SpawnEvent, sizeof(ServerSpawnEvent_Struct));
	ServerSpawnEvent_Struct* sse = (ServerSpawnEvent_Struct*)pack->pBuffer;

	sse->zoneID = ZoneID(zone_short_name.c_str());
	sse->event_id = event_id;

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

int16 SpawnConditionManager::GetCondition(const char *zone_short, uint32 instance_id, uint16 condition_id) {
	if(!strcasecmp(zone_short, zone->GetShortName()))
	{
		//this is a local spawn condition
		std::map<uint16, SpawnCondition>::iterator condi;
		condi = spawn_conditions.find(condition_id);
		if(condi == spawn_conditions.end())
		{
			Log(Logs::Detail, Logs::Spawns, "Unable to find local condition %d in Get request.", condition_id);
			return(0);	//unable to find the spawn condition
		}

		SpawnCondition &cond = condi->second;
		return cond.value;
	}

	//this is a remote spawn condition, grab it from the DB
    //load spawn conditions
    std::string query = StringFormat("SELECT value FROM spawn_condition_values "
                                    "WHERE zone = '%s' AND instance_id = %lu AND id = %d",
                                    zone_short, (unsigned long)instance_id, condition_id);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        Log(Logs::Detail, Logs::Spawns, "Unable to query remote condition %d from zone %s in Get request.", condition_id, zone_short);
		return 0;	//dunno a better thing to do...
    }

    if (results.RowCount() == 0) {
        Log(Logs::Detail, Logs::Spawns, "Unable to load remote condition %d from zone %s in Get request.", condition_id, zone_short);
		return 0;	//dunno a better thing to do...
    }

    auto row = results.begin();

    return atoi(row[0]);
}

bool SpawnConditionManager::Check(uint16 condition, int16 min_value) {
	std::map<uint16, SpawnCondition>::iterator condi;
	condi = spawn_conditions.find(condition);
	if(condi == spawn_conditions.end())
		return(false);	//unable to find the spawn condition

	SpawnCondition &cond = condi->second;

	return(cond.value >= min_value);
}

void ZoneDatabase::UpdateRespawnTime(uint32 spawn2_id, uint32 time_left, uint32 guild_id)
{

	timeval tv;
	gettimeofday(&tv, nullptr);
	uint32 current_time = tv.tv_sec;

	/*	If we pass timeleft as 0 that means we clear from respawn time
	otherwise we update with a REPLACE INTO
	*/

	if (time_left == 0) {
		std::string query = StringFormat("DELETE FROM `respawn_times` WHERE `id` = %u and guild_id = %lu", spawn2_id, guild_id);
		QueryDatabase(query);

		return;
	}

	std::string query = StringFormat(
		"REPLACE INTO `respawn_times` "
		"(id, "
		"guild_id, "
		"start, "
		"duration) "
		"VALUES "
		"(%u, "
		"%u, "
		"%u, "
		"%u)",
		spawn2_id,
		guild_id,
		current_time,
		time_left
	);
	QueryDatabase(query);

	return;
}

//Gets the respawn time left in the database for the current spawn id
uint32 ZoneDatabase::GetSpawnTimeLeft(uint32 id, uint32 guild_id)
{
	std::string query = StringFormat("SELECT start, duration FROM respawn_times "
		"WHERE id = %lu AND guild_id = %lu",
		(unsigned long)id, (unsigned long)guild_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();

	timeval tv;
	gettimeofday(&tv, nullptr);
	uint32 resStart = atoi(row[0]);
	uint32 resDuration = atoi(row[1]);

	//compare our values to current time
	if ((resStart + resDuration) <= tv.tv_sec) {
		//our current time was expired
		return 0;
	}

	//we still have time left on this timer
	return ((resStart + resDuration) - tv.tv_sec);

}

void ZoneDatabase::UpdateSpawn2Status(uint32 id, uint8 new_status)
{
	std::string query = StringFormat("UPDATE spawn2 SET enabled = %i WHERE id = %lu and guild_id = %i", new_status, (unsigned long)id);
	QueryDatabase(query);
}

uint16 Zone::GetAvailPointCount(SpawnGroup* sg)
{
	uint32 id = sg->id;
	uint32 skipped_spawn2 = sg->skipped_spawn;
	uint16 spawncount = 0;
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		Spawn2 *cur = iterator.GetData();
		uint32 sp2id = cur->GetID();

		if (cur->SpawnGroupID() == id && sp2id != skipped_spawn2 && cur->npcthis == nullptr)
		{
			bool reset = sg->group_spawn_limit == 1;
			uint32 timeleft = reset ? 0 : database.GetSpawnTimeLeft(sp2id, zone->GetGuildID());

			// We don't want to count the points waiting to spawn.
			if (timeleft == 0 || reset)
			{
				++spawncount;
			}
		}
		iterator.Advance();
	}

	return spawncount;
}

void Zone::UpdateGroupTimers(SpawnGroup* sg, uint32 newtimer_id)
{

	// This method will only be called if the spawn group limit is 1 (in that case we always want to keep all timers in sync) or 
	// when the spawn limit is greater than 1, and we are forcing the current spawnpoint to pick another random one, so the same
	// point isn't picked over and over. In the second case, newtimer_id will always be set to 1.
	// This method should not be used when there is no spawn limit on the spawngroup.

	uint32 id = sg->id;
	uint32 skipped_spawn2 = sg->skipped_spawn;
	uint8 spawn_limit = sg->group_spawn_limit;
	uint16 spawncount = GetAvailPointCount(sg);
	if (spawncount == 0)
	{
		Log(Logs::Detail, Logs::Spawns, "Spawngroup %d: We have no available spawns (%d), no timers will be updated.", id, spawncount);
		return;
	}

	uint16 index = 0;
	uint8 random = spawncount == 1 ? 1 : zone->random.Int(1, spawncount); // When the spawnpoint with this index value is reached, it will either spawn immediately or have its timer updated to spawn next.
	Spawn2 *sp = newtimer_id > 1 ? entity_list.GetSpawnByID(newtimer_id) : nullptr;
	uint32 remaining_time = sp ? sp->timer.GetRemainingTime() : 0;

	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		Spawn2 *cur = iterator.GetData();
		uint32 sp2id = cur->GetID();
		if (cur->SpawnGroupID() == id && cur->npcthis == nullptr)
		{
			bool reset = spawn_limit == 1;
			uint32 timeleft = reset ? 0 : database.GetSpawnTimeLeft(sp2id, GetGuildID());

			// We don't want to update the timer of points waiting to spawn.
			if (timeleft == 0 || reset)
			{
				if (sp2id != skipped_spawn2)
				{
					++index;
				}

				// Pick a random spawnpoint to process next.
				if (index == random && sp2id != skipped_spawn2)
				{
					if (newtimer_id == 1)
					{
						Log(Logs::General, Logs::Spawns, "Spawngroup %d: %d is triggering! Roll: %d of %d", id, sp2id, random, spawncount);
						cur->timer.Trigger();
					}
					else
					{
						Log(Logs::General, Logs::Spawns, "Spawngroup %d: %d will be triggered next in %d seconds. Roll: %d of %d", id, sp2id, static_cast<uint32>(remaining_time / 1000), random, spawncount);
						cur->Reset(remaining_time);
					}
				}
				else
				{
					if (reset || newtimer_id == 1)
					{
						// If spawn limit is 1, or we are forcing a new point to be picked (newtimer_id == 1) then we can safely reset all the other timers.
						// Spawnpoints with an active timer in the db are ignored when newtimer_id is 1.
						cur->Reset();
					}
					else
					{
						// We have a spawn limit higher than 1 and there may be multiple NPCs waiting to spawn. We'll want another spawnpoint to process after this, to grab the next expiring timer. 
						cur->Reset(remaining_time + 100);
					}
				}
			}
		}
		iterator.Advance();
	}

	if (skipped_spawn2 > 0)
		Log(Logs::Detail, Logs::Spawns, "Spawngroup %d: Skipped spawnpoint was: %d", id, skipped_spawn2);
}

uint16 Zone::GetGroupActiveTimers(uint32 spawngroupid, uint32& remaining_time_id)
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	uint16 count = 0;
	uint32 tmp = UINT_MAX;
	while (iterator.MoreElements())
	{
		Spawn2 *cur = iterator.GetData();
		if (cur->SpawnGroupID() == spawngroupid && cur->timer.Enabled())
		{
			// We hit the db to see if they are waiting to spawn. The timer in memory cannot tell us, because it is reset 
			// whenever we fail to spawn a NPC from that spawnpoint. (In this case due to the spawngroup limit.)
			uint32 timeleft = database.GetSpawnTimeLeft(cur->GetID(), GetGuildID());
			if (timeleft > 0)
			{
				// Now that we know this NPC was recently despawned, we want to use the time left in memory for the pointer value, 
				// because it is in ms and more accurate. 
				uint32 timerleft = cur->timer.GetRemainingTime();
				if (timerleft < tmp)
				{
					tmp = timerleft;
					remaining_time_id = cur->GetID();
				}
				++count;
			}
		}
		iterator.Advance();
	}

	return count;
}