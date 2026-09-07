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
#ifndef SPAWN2_H
#define SPAWN2_H

#include "../common/timer.h"
#include "npc.h"

#define SC_AlwaysEnabled 0

class SpawnCondition;
class NPC;

class Spawn2
{
public:
	Spawn2(uint32 spawn2_id, uint32 spawngroup_id,
		float x, float y, float z, float heading,
		uint32 respawn, uint32 variance,
		uint32 timeleft = 0, uint32 grid = 0,
		uint16 cond_id = SC_AlwaysEnabled, int16 min_value = 0, bool in_enabled = true, EmuAppearance anim = eaStanding, 
		bool force_z = false, bool rand_spawn = false, bool raid_target_spawnpoint = false);
	~Spawn2();

	void	LoadGrid(int start_wp = 0);
	uint16	GetGrid();
	void	Enable() { enabled = true; if (!timer.Enabled()) timer.Trigger(); }
	void	Disable(bool depop = true);
	bool	Enabled() { return enabled; }
	bool	Process();
	void	Reset(uint32 rtime = 0);
	void	Depop();
	void	Repop(uint32 delay = 0);
	void	ForceDespawn();
	void	ChangeDespawn(uint8 newval, uint32 despawn_timer = 0);

	void	DeathReset(bool realdeath = 0); //resets the spawn in the case the npc dies, also updates db if needed
	void	QuakeReset(); // despawns & resets an npc after a quake ends

	void	SpawnConditionChanged(const SpawnCondition &c, int16 old_value);
	uint32	GetID()		{ return spawn2_id; }
	float	GetX()		{ return x; }
	float	GetY()		{ return y; }
	float	GetZ()		{ return z; }
	float	GetHeading() { return heading; }
	void	SetRespawnTimer(uint32 newrespawntime) { respawn_ = newrespawntime; };
	void	SetVariance(uint32 newvariance) { variance_ = newvariance; }
	const uint32 GetVariance() const { return variance_; }
	uint32	RespawnTimer() { return respawn_; }
	uint32	SpawnGroupID() { return spawngroup_id_; }
	uint32	CurrentNPCID() { return currentnpcid; }
	void	SetCurrentNPCID(uint32 nid) { currentnpcid = nid; }
	uint32	GetSpawnCondition() { return condition_id; }

	bool	NPCPointerValid() { return (npcthis!=nullptr); }
	void	SetNPCPointer(NPC* n) { npcthis = n; }
	NPC*	GetNPCPointer() { return npcthis; }
	void	SetTimer(uint32 duration) { timer.Start(duration, false); }
	uint32  GetKillCount() { return killcount; }
	bool	GetForceZ() { return force_z; }
	bool	IsRaidTargetSpawnpoint() { return raid_target_spawnpoint; }
protected:
	friend class Zone;
	friend class SpawnGroup;
	Timer	timer;
private:
	uint32	spawn2_id;
	uint32	respawn_;
	uint32	resetTimer(bool quake_repop = false);
	uint32	despawnTimer(uint32 despawn_timer);

	uint32	spawngroup_id_;
	uint32	currentnpcid;
	NPC*	npcthis;
	uint32	last_level_attempt;
	uint32  last_instance_spawn_timer_override;
	float	x;
	float	y;
	float	z;
	float	heading;
	uint32	variance_;
	uint32	grid_;
	uint16	condition_id;
	int16	condition_min_value;
	bool enabled;
	EmuAppearance anim;
	bool IsDespawned;
	uint32  killcount;
	bool force_z;
	bool rand_spawn;
	bool raid_target_spawnpoint;
};

class SpawnCondition {
public:
	typedef enum {
		DoNothing = 0,
		DoDepop = 1,
		DoRepop = 2,
		DoRepopIfReady = 3,
		//... 4...9 reserved for future use
		DoSignalMin = 10	//any number above this value is used as
							//a base for the signal ID sent. e.g.
							// value 12 sends signal id 2
	} OnChange;

	SpawnCondition();

	uint16		condition_id;
	int16		value;
	OnChange	on_change;
};

class SpawnEvent {
public:
	typedef enum {
		ActionSet = 0,
		ActionAdd = 1,
		ActionSubtract = 2,
		ActionMultiply = 3,
		ActionDivide = 4
	} Action;

	SpawnEvent();

	uint32	id;
	uint16	condition_id;
	std::string	zone_name;
	uint32 instance_id;

	bool	enabled;
	Action	action;
	int16	argument;
	bool	strict;

	uint32	period;	//eq minutes (3 seconds) between events
	TimeOfDay_Struct next;	//next time this event triggers
};

class SpawnConditionManager {
public:
	SpawnConditionManager();

	void Process();
	bool LoadSpawnConditions(const char* zone_name, uint32 instance_id);

	int16 GetCondition(const char *zone_short, uint32 instance_id, uint16 condition_id);
	void SetCondition(const char *zone_short, uint32 instance_id, uint16 condition_id, int16 new_value, bool world_update = false);
	void ToggleEvent(uint32 event_id, bool enabled, bool strict, bool reset_base);
	bool Check(uint16 condition, int16 min_value);
	void ReloadEvent(uint32 event_id);

protected:
	std::map<uint16, SpawnCondition> spawn_conditions;
	std::vector<SpawnEvent> spawn_events;

	void ExecEvent(SpawnEvent &e, bool send_update);
	void UpdateDBEvent(SpawnEvent &e);
	bool LoadDBEvent(uint32 event_id, SpawnEvent &e, std::string &zone_name);
	void UpdateDBCondition(const char* zone_name, uint32 instance_id, uint16 cond_id, int16 value);
	void FindNearestEvent();

	Timer minute_timer;
	TimeOfDay_Struct next_event;
};

constexpr int format_as(SpawnCondition::OnChange val) { return static_cast<int>(val); }
constexpr int format_as(SpawnEvent::Action val) { return static_cast<int>(val); }

#endif