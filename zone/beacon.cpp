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

solar: Beacon class, extends Mob. Used for AE rain spells to have a mob
target to center around.

*/

class Zone;

#ifdef _WINDOWS
	#if (!defined(_MSC_VER) || (defined(_MSC_VER) && _MSC_VER < 1900))
		#define snprintf	_snprintf
		#define vsnprintf	_vsnprintf
	#endif
    #define strncasecmp	_strnicmp
    #define strcasecmp	_stricmp
#endif

#include "../common/races.h"
#include "beacon.h"
#include "client.h"
#include "entity.h"
#include "mob.h"
#include "map.h"
#include "string_ids.h"
#include "water_map.h"
#include "../common/fastmath.h"
#include "../common/spdat.h"
#include <chrono>

extern EntityList entity_list;
extern Zone* zone;
extern FastMath g_Math;

// solar: if lifetime is 0 this is a permanent beacon.. not sure if that'll be
// useful for anything
Beacon::Beacon(Mob* at_mob, int lifetime) : Mob(
	nullptr, // in_name
	nullptr, // in_lastname
	0, // in_cur_hp
	0, // in_max_hp
	Gender::Male, // in_gender
	Race::InvisibleMan, // in_race
	Class::None, // in_class
	BodyType::NoTarget, // in_bodytype
	Deity::Unknown, //in_deity
	0, // in_level
	0, // in_npctype_id
	0.0f, // in_size
	0.0f, // in_runspeed
	at_mob->GetPosition(), // position
	0, // in_light
	0, // in_texture
	0, // in_helmtexture
	0, // in_ac
	0, // in_atk
	0, // in_str
	0, // in_sta
	0, // in_dex
	0, // in_agi
	0, // in_int
	0, // in_wis
	0, // in_cha
	0, // in_haircolor
	0, // in_beardcolor
	0, // in_eyecolor1
	0, // in_eyecolor2
	0, // in_hairstyle,
	0, // in_luclinface
	0, // in_beard
	EQ::TintProfile(), // in_armor_tint
	0, // in_aa_title
	0, // in_see_invis
	0, // in_see_invis_undead
	0, // in_see_sneak
	0, // in_see_improved_hide
	0, // in_hp_regen
	0, // in_mana_regen
	0, // in_qglobal
	0, // in_max_level
	0, // in_scalerate
	0, // in_armtexture
	0, // in_bracertexture
	0, // in_handtexture
	0, // in_legtexture
	0, // in_feettexture
	0 // in_chesttexture
),
		remove_timer(lifetime),
		spell_timer(0)
{
	remove_timer.Disable();
	spell_timer.Disable();
	remove_me = false;
	spell_id = 0xFFFF;
	resist_adjust = 0;
	spell_iterations = 0;
	caster_id = 0;
	targets_hit = 0;
	initial_cast = false;
	beaconType = beaconTypeBeacon;
	pitch = 128.0f;
	projectile_hate = 0;
	delay_duration = 0.0;
	delay_start = 0.0;
	time_start = std::chrono::steady_clock::now();
	frame_prev = time_start;
	distance_traveled = 0.0;
	origin = at_mob->GetPosition();
	if(lifetime) {
		remove_timer.Start();
	}
#ifdef SOLAR
	entity_list.Message(Chat::White, 0, "Beacon being created at %0.2f %0.2f %0.2f heading %0.2f lifetime %d", GetX(), GetY(), GetZ(), GetHeading(), lifetime);
#endif
}

Beacon::~Beacon()
{
#ifdef SOLAR
	entity_list.Message(Chat::White, 0, "Beacon %d being removed at %0.2f %0.2f %0.2f heading %0.2f", GetID(), GetX(), GetY(), GetZ(), GetHeading());
#endif
}

bool Beacon::Process()
{
	if(remove_me)
	{
		return false;
	}

	if
	(
		spell_timer.Enabled() &&
		spell_timer.Check() &&
		(IsValidSpell(spell_id) || IsProjectile())
	)
	{
		Mob *caster = entity_list.GetMob(caster_id);
		// Bolt spells and arrows
		if (IsBolt() || this->IsProjectile()) { // Bolt Spells and arrows
			Entity *targetEntity = entity_list.GetID(target_id); // for a targetless bolt the target it's traveling toward is a beacon
			Mob *target = targetEntity != nullptr && targetEntity->IsMob() ? targetEntity->CastToMob() : nullptr;
			if (!target || !caster) {
				Depop();
				return false;
			}
			if (caster == target && IsBolt()) {
				caster->SpellOnTarget(spell_id, caster, false, true, resist_adjust, true);
				Depop();
				return true;
			}
			std::chrono::time_point<std::chrono::steady_clock> frame_now = std::chrono::steady_clock::now();
			double time_elapsed = std::chrono::duration<double>(frame_now - frame_prev).count();
			frame_prev = frame_now;
			if (delay_duration > 0.0) {
				// this handles animation delays
				double time_passed = std::chrono::duration<double>(frame_now - time_start).count();
				//target->Shout("Time Elapsed %lf", time_passed);
				if (time_passed > delay_duration) {
					time_elapsed = time_passed - delay_duration;
					delay_duration = 0.0;
					spell_iterations = 0;
				}
				else {
					return true;
				}
			}
			spell_iterations++;
			bool finished = false;
			if (spell_iterations > 0)
			{
				tarpos = glm::vec3(target->GetX(), target->GetY(), target->GetZ());
				if (!target->IsClient() && (!zone->watermap || !zone->watermap->InLiquid(tarpos)))
				{
					float newz = zone->zonemap->FindBestZ(tarpos, nullptr);
					if (newz != BEST_Z_INVALID)
					{
						tarpos.z = target->SetProjectileZ(newz);
					}
				}
				float cur_dist = DistanceNoZ(GetPosition(), target->GetPosition());
				float total_dist = DistanceNoZ(origin, target->GetPosition());

				//float move = this->velocity * this->timer_interval / 10;
				double move = this->velocity * time_elapsed * 100;
				distance_traveled += move;
				//target->Shout("Time Elapsed %lf new real %lf", time_elapsed * 1000.0, move);
				if (cur_dist < move || distance_traveled > total_dist) {
					move = cur_dist;
					finished = true;
				}
				
				glm::vec3 last_pos(GetX(), GetY(), GetZ());
				float desired_pitch = CalculatePitchToTarget(tarpos);
				float delta_z = tarpos.z - GetZ();

				const float delta_x = g_Math.FastSin(GetHeading() * 2.0f);
				const float delta_y = g_Math.FastCos(GetHeading() * 2.0f);

				if (!finished && move > 0.0f) {
					int steps = cur_dist / move; // should always have steps > 0
					if (!IsProjectile()) {
						if (steps > 0 && steps < 6) // Bolts curve up at the end.
							delta_z /= (float)steps;
						else
							delta_z = 0.0f;
					}
					if (IsProjectile() && steps > 0) {
						if (pitch > desired_pitch) {
							pitch -= (pitch - desired_pitch) / (float)steps * 2.0f;
						}
						else {
							pitch = desired_pitch;
						}
						if (pitch > 128.0f) {
							delta_z = move * sinf(fabsf(128.0f - pitch) / 512.0f*2.0f*M_PI);
						}
						else {
							delta_z = -move * sinf(fabsf(128.0f - pitch) / 512.0f*2.0f*M_PI);
						}
					}

					const float mid_x = GetX() + (move / 2.0f * delta_x);
					const float mid_y = GetY() + (move / 2.0f * delta_y);

					bool mobs_nearby = entity_list.CheckMobCloseForCollision(caster, target, mid_x, mid_y, move / 2.0f + 5.0f, beaconType);
					int32 collision_mob = 0;
					glm::vec3 newpos = last_pos;

					if (mobs_nearby)
					{
						float step = 1.0f;
						int increment = move / step;
						float inc = move / step;

						for (int i = 0; i < increment; i++)
						{
							newpos.x += (delta_x * move / inc);
							newpos.y += (delta_y * move / inc);
							newpos.z += (delta_z / inc);
							collision_mob = entity_list.CheckMobCollision(caster, nullptr, newpos.x, newpos.y, newpos.z, beaconType);
							if (collision_mob)
								break;
						}
						// we have mobs nearby - see if they are in the way.
						// step through the travel, to see if we collide with an NPC/PC

					}
					if (collision_mob) {
						//caster->Message(13, "Collision mob found Id:(%d)", collision_mob);
						Mob* collision = entity_list.GetMob(collision_mob);
						if (collision)
						{
							// we have a mob to collide with
							tarpos.x = collision->GetX();
							tarpos.y = collision->GetY();
							tarpos.z = newpos.z;
							target = collision;
							finished = true;
						}
					}
					if (!finished && (IsProjectile() || steps < 8))
						m_Position.z += delta_z;
				}
				if (!finished)
				{
					m_Position.x += (move * g_Math.FastSin(GetHeading() * 2.0f));
					m_Position.y += (move * g_Math.FastCos(GetHeading() * 2.0f));
					m_Position.w = CalculateHeadingToTarget(target->GetX(), target->GetY());
					//this->Tracer(caster_id, target_id, GetPosition());
					//caster->CastToClient()->Message(13, "Spawning tracer at (%.2f, %.2f, %.2f pitch %.3f desired %.3f)", m_Position.x, m_Position.y, m_Position.z, pitch, desired_pitch);
				}

				// this is total distance moved
				if (!finished && !this->CheckProjectileCollision(last_pos)) {
					Depop();
					return true;
				}

			}
			if (finished) {
				if (target && this->CheckProjectileCollision(tarpos)) {
					if (IsBolt()) {
						if (caster->IsAttackAllowed(target, true)) {
							bool fail_cast = false;
							if (target->IsNPC()) {
								// don't let aimed bolts affect NPCs that are too far to give chase
								float ignoreDistance = target->CastToNPC()->GetIgnoreDistance();
								fail_cast = ignoreDistance > 0 && DistanceSquared(target->GetPosition(), caster->GetPosition()) > ignoreDistance * ignoreDistance;

								if (!fail_cast && RuleB(AlKabor, BlockProjectileCorners) && target->CastToNPC()->IsCornered())
								{
									Log(Logs::Detail, Logs::Combat, "Poofing bolt; %s is cornered", target->GetName());
									fail_cast = true;
								}

								if (!fail_cast && RuleB(AlKabor, BlockProjectileWalls) && target->CastToNPC()->IsWalled())
								{
									float height_diff = fabs(origin.x - target->GetZ());
									float vector_x, vector_y, magnitude;
									float xy_angle = 0.0f;

									vector_x = target->GetX() - GetX();
									vector_y = target->GetY() - GetY();
									magnitude = sqrtf(vector_x * vector_x + vector_y * vector_y);
									vector_x /= magnitude;
									vector_y /= magnitude;
									xy_angle = fabs(-target->CastToNPC()->GetWallAngle1(vector_x, vector_y));
									// allow the bolt if fired parallel to the wall; deny if fired perpendicular
									if (xy_angle > 0.4f)
									{
										Log(Logs::Detail, Logs::Combat, "Poofing bolt; %s is against a wall(1)  xy_angle: %0.4f", target->GetName(), xy_angle);
										fail_cast = true;
									}

									xy_angle = fabs(-target->CastToNPC()->GetWallAngle2(vector_x, vector_y));
									if (xy_angle > 0.4f)
									{
										Log(Logs::Detail, Logs::Combat, "Poofing bolt; %s is against a wall(2)  xy_angle: %0.4f", target->GetName(), xy_angle);
										fail_cast = true;
									}
								}

							}
							if (!fail_cast)
								caster->SpellOnTarget(spell_id, target, false, true, resist_adjust, true);
						}
					}
					if (IsProjectile() && caster->IsAttackAllowed(target))
					{
						if (weapon_dmg > 0 && caster->IsClient() && skillinuse == EQ::skills::SkillThrowing)
						{
							// move this section to delay based.
							uint32 Assassinate_Dmg = 0;
							if (caster->GetClass() == Class::Rogue && (caster->BehindMob(target, GetX(), GetY())))
								Assassinate_Dmg = caster->CastToClient()->TryAssassinate(target, EQ::skills::SkillThrowing);
							if (Assassinate_Dmg)
							{
								weapon_dmg = Assassinate_Dmg;
								entity_list.MessageClose_StringID(caster, false, 200, Chat::MeleeCrit, StringID::ASSASSINATES, caster->GetName());
							}
							else
							{
								caster->TryCriticalHit(target, EQ::skills::SkillThrowing, weapon_dmg);
							}
						}

						if (weapon_dmg > 0 && skillinuse == EQ::skills::SkillArchery)
						{
							if (caster->GetClass() == Class::Ranger)
							{
								if (target->IsNPC() && target->GetBodyType() == BodyType::Humanoid && caster->GetAABonuses().HeadShot[1]
									&& target->GetLevel() <= caster->GetAABonuses().HSLevel
									&& zone->random.Roll(static_cast<double>(caster->GetDEX()) / 3500.0)
									)
								{
									entity_list.MessageClose_StringID(caster, false, 200, Chat::MeleeCrit, StringID::FATAL_BOW_SHOT, caster->GetName());
									weapon_dmg = 32000;
								}
								else
								{
									if (caster->GetLevel() > 50)
									{
										bool dobonus = false;
										int bonuschance = RuleI(Combat, ArcheryBonusChance);

										if (!RuleB(Combat, UseArcheryBonusRoll) || (zone->random.Int(1, 100) < bonuschance))
										{
											if (RuleB(Combat, ArcheryBonusRequiresStationary))
											{
												if (target->IsNPC() && !target->IsMoving() && (!target->IsRooted() || target->PermaRooted()))
												{
													dobonus = true;
												}
											}
											else
											{
												dobonus = true;
											}
										}

										if (dobonus)
										{
											weapon_dmg *= 2;
											caster->Message_StringID(Chat::MeleeCrit, StringID::BOW_DOUBLE_DAMAGE);
										}
									}

									if (weapon_dmg < 1)
										weapon_dmg = 1;
								}
							}

							caster->TryCriticalHit(target, EQ::skills::SkillArchery, weapon_dmg);
						}

						target->AddToHateList(caster, projectile_hate, 0);
						target->Damage(caster, weapon_dmg, SPELL_UNKNOWN, skillinuse);
					}
				}
				Depop();
			}
			return true;
		}
		else if (caster && spell_iterations--) {
			// AE Spells
			bool affect_caster = spells[spell_id].targettype == ST_Group || (!caster->IsNPC() && !caster->IsAIControlled());	// most NPC AE spells do not affect the NPC caster
			entity_list.AESpell(caster, this, spell_id, affect_caster, resist_adjust, nullptr, initial_cast);
			initial_cast = false;
		}
		else {
			// spell is done casting, or caster disappeared
			spell_id = 0xFFFF;
			spell_iterations = 0;
			spell_timer.Disable();
			caster_id = 0;
			targets_hit = 0;
			initial_cast = false;
		}
	}

	if(remove_timer.Enabled() && remove_timer.Check())
	{
		return false;
	}

	return true;
}

void Beacon::AELocationSpell(Mob *caster, uint16 cast_spell_id, int16 resist_adjust)
{
	if(!IsValidSpell(cast_spell_id) || !caster)
		return;

	caster_id = caster->GetID();
	spell_id = cast_spell_id;
	beaconType = beaconTypeBeacon;
	this->resist_adjust = resist_adjust;
	spell_iterations = spells[spell_id].AEDuration / 2500;
	spell_iterations = spell_iterations < 1 ? 1 : spell_iterations;	// at least 1
	initial_cast = true;
	spell_timer.Start(2500);
	timer_interval = 2500;
	spell_timer.Trigger();
}

void Beacon::BoltSpell(Mob *caster, Mob *target, int16 cast_spell_id)
{
	if (!IsValidSpell(cast_spell_id) || !caster || !target)
		return;
	beaconType = beaconTypeBolt;
	caster_id = caster->GetID();
	target_id = target->GetID();
	spell_id = cast_spell_id;
	spell_iterations = 0;
	timer_interval = 200;
	spell_timer.Start(200, true);
	velocity = 1.0f;
	SetHeading(caster->CalculateHeadingToTarget(target->GetX(), target->GetY()));
	SetPosition(caster->GetPosition());
	tarpos = glm::vec3(target->GetX(), target->GetY(), target->GetZ() + target->GetSize() * 0.2f);
	bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(tarpos) || zone->IsWaterZone(tarpos.z);
	if (!target->IsClient() && !in_liquid)
	{
		float newz = zone->zonemap->FindBestZ(tarpos, nullptr);
		if (newz != BEST_Z_INVALID)
		{
			tarpos.z = target->SetProjectileZ(newz);
		}
	}
}

void Beacon::Projectile(Mob *attacker, Mob *target, EQ::skills::SkillType skill, int32 wep_dmg, int32 hate, uint32 reusetimer, float pitch_angle)
{
	if (!attacker || !target)
		return;
	beaconType = beaconTypeProjectile;
	caster_id = attacker->GetID();
	target_id = target->GetID();
	weapon_dmg = wep_dmg;
	reuse_timer = reusetimer;
	skillinuse = skill;
	spell_id = SPELL_UNKNOWN;
	spell_iterations = 0;
	delay_duration = 0.9;
	timer_interval = 50;
	spell_timer.Start(15, true);
	velocity = 4.0f;
	pitch = pitch_angle;
	projectile_hate = hate;
	SetPosition(attacker->GetPosition());
	m_Position.z += attacker->GetSize() * 0.3f;
	m_Position.w = attacker->CalculateHeadingToTarget(target->GetX(), target->GetY());
	tarpos = glm::vec3(target->GetX(), target->GetY(), target->GetZ() + target->GetSize() * 0.2f);
	bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(tarpos) || zone->IsWaterZone(tarpos.z);
	if (!target->IsClient() && !in_liquid)
	{
		float newz = zone->zonemap->FindBestZ(tarpos, nullptr);
		if (newz != BEST_Z_INVALID)
		{
			tarpos.z = target->SetProjectileZ(newz);
		}
	}
}
void Beacon::Tracer(int16 src_id, int16 tgt_id, glm::vec4 pos)
{
	Mob* attacker = entity_list.GetMob(src_id);
	NPCType* npc_type = new NPCType;
	memset(npc_type, 0, sizeof(NPCType));
	sprintf(npc_type->name, "%s", "Tracer");


	sprintf(npc_type->lastname, "%s", "Gnome");

	npc_type->cur_hp = 50;
	npc_type->max_hp = 50;
	npc_type->race = 12;
	npc_type->gender = 1;
	npc_type->class_ = 1;
	npc_type->deity = 1;
	npc_type->level = 1;
	npc_type->npc_id = 0;
	npc_type->loottable_id = 0;
	npc_type->texture = 1;
	npc_type->light = 0;
	npc_type->runspeed = 0;
	npc_type->d_melee_texture1 = 1;
	npc_type->d_melee_texture2 = 1;
	npc_type->merchanttype = 1;
	npc_type->bodytype = 1;

	npc_type->size = 1;

	npc_type->STR = 150;
	npc_type->STA = 150;
	npc_type->DEX = 150;
	npc_type->AGI = 150;
	npc_type->INT = 150;
	npc_type->WIS = 150;
	npc_type->CHA = 150;

	NPC* npc = new NPC(npc_type, 0, pos, GravityBehavior::Flying);

	entity_list.AddNPC(npc, true, true);
}

bool Beacon::CheckProjectileCollision(glm::vec3 oloc) {
	// returns false if no map, or has a collision
	if (zone->zonemap == NULL) {
		return(false);
	}

	glm::vec3 mypos = glm::vec3(GetPosition());
	//see if anything is in the way
	if (zone->zonemap->LineIntersectsZone(mypos, oloc, 1.0f, nullptr))
		return false;

	return true;
}

bool EntityList::CheckMobCloseForCollision(Mob *attacker, Mob *exclude, float x, float y, float dist, uint8 beaconType) {
	if (!attacker || (attacker->GetUltimateOwner()->IsNPC() && zone->GetZoneID() != Zones::MISCHIEFPLANE))
		return false;

	const float xmax = x + dist;
	const float xmin = x - dist;
	const float ymax = y + dist;
	const float ymin = y - dist;

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		Mob *current = it->second;
		if (!current)
		{
			continue;
		}
		if (!current->IsNPC() && !current->IsClient()) {
			continue;
		}
		if (current == exclude || current == attacker) {
			continue;
		}
		if (!attacker->IsAttackAllowed(current) && beaconType != beaconTypeBolt) {
			continue;
		}
		float targetSize = current->GetSize() * 0.9f;;
		if (current->GetX() - targetSize / 2 > xmax || current->GetX() + targetSize / 2 < xmin)
		{
			continue;
		}
		if (current->GetY() - targetSize / 2 > ymax || current->GetY() + targetSize / 2 < ymin)
		{
			continue;
		}
		// if we got here, then we have one within x, y +/- dist
		return true;
	}

	return false;
}

uint32 EntityList::CheckMobCollision(Mob *attacker, Mob *exclude, float x, float y, float z, uint8 beaconType) {
	if (!attacker)
		return 0;

	const float projectileSize = beaconType == beaconTypeBolt ? 3.0f : 0.5f;
	const float xmax = x + projectileSize / 2;
	const float xmin = x - projectileSize / 2;
	const float ymax = y + projectileSize / 2;
	const float ymin = y - projectileSize / 2;

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		Mob *current = it->second;
		if (!current)
		{
			continue;
		}
		if (!current->IsNPC() && !current->IsClient()) {
			continue;
		}
		if (current == exclude || current == attacker) {
			continue;
		}
		if (!attacker->IsAttackAllowed(current) && beaconType != beaconTypeBolt) {
			continue;
		}
		float targetSize = current->GetSize() * 0.9f;;
		if (current->GetX() - targetSize / 2 > xmax || current->GetX() + targetSize / 2 < xmin)
		{
			continue;
		}
		if (current->GetY() - targetSize / 2 > ymax || current->GetY() + targetSize / 2 < ymin)
		{
			continue;
		}
		float minz = current->GetZ() - (current->GetZOffset());
		if (z > minz)
		{
			//attacker->Message(0, "Projectile z (%.2f) greater than minz (%.2f)", z, minz);
			float maxz = minz + targetSize;
			if (z < maxz)
				return current->GetID();
		}
	}
	return 0;
}

