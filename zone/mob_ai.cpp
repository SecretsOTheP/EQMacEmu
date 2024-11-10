/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2004 EQEMu Development Team (http://eqemu.org)

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
#include "../common/features.h"
#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/data_verification.h"
#include "../common/repositories/npc_spells_repository.h"
#include "../common/repositories/npc_spells_entries_repository.h"
#include "../common/repositories/criteria/content_filter_criteria.h"
#include "client.h"
#include "entity.h"
#include "map.h"
#include "mob.h"
#include "npc.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "water_map.h"
#include "worldserver.h"

#include <algorithm>
#include <iostream>
#include <math.h>

extern EntityList entity_list;
extern WorldServer worldserver;
extern Zone *zone;

#ifdef _EQDEBUG
	#define MobAI_DEBUG_Spells	-1
#else
	#define MobAI_DEBUG_Spells	-1
#endif

//NOTE: do NOT pass in beneficial and detrimental spell types into the same call here!
bool NPC::AICastSpell(Mob* tar, uint8 iChance, uint16 iSpellTypes, bool zeroPriorityOnly)
{
	if (!tar)
		return false;

	if (IsNoCast())
		return false;

	if(AI_HasSpells() == false)
		return false;

	if (iChance < 100) {
		if (zone->random.Int(0, 100) >= iChance)
			return false;
	}

	float dist2;

	if (iSpellTypes & SpellType_Escape) {
		dist2 = 0; //DistNoRoot(*this);	//WTF was up with this...
	}
	else
		dist2 = DistanceSquared(m_Position, tar->GetPosition());

	bool checkedTargetLoS = false;	//we do not check LOS until we are absolutely sure we need to, and we only do it once.

	if (zone->SkipLoS())
		checkedTargetLoS = true;		// ignore LoS checks in zones with LoS disabled

	int roll_mod = 0;	// modify roll chances based on number of spells npc can cast
	int spellArrSize = static_cast<int>(AIspells.size());
	if (spellArrSize < 4)
		roll_mod = 10;
	else if (spellArrSize > 9)
		roll_mod = -10;
	else if (spellArrSize > 6)
		roll_mod = -5;

	for (int i = spellArrSize - 1; i >= 0; i--) {
		if (AIspells[i].spellid <= 0 || AIspells[i].spellid >= SPDAT_RECORDS) {
			// this is both to quit early to save cpu and to avoid casting bad spells
			// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
			//return false;
			continue;
		}
		if (iSpellTypes & AIspells[i].type)
		{
			// manacost has special values, -1 is no mana cost, -2 is instant cast (no mana)
			// recastdelay of 0 is recast time + variance. -1 recast time only. -2 is no recast time or variance (chain casting)
			// All else is specified recast_delay + variance if the rule is enabled.
			int32 mana_cost = AIspells[i].manacost;
			if (mana_cost == -1)
				mana_cost = spells[AIspells[i].spellid].mana;
			else if (mana_cost == -2)
				mana_cost = 0;
			if (
				((
					(spells[AIspells[i].spellid].targettype==ST_AECaster || spells[AIspells[i].spellid].targettype==ST_AEBard)
					&& (dist2 <= spells[AIspells[i].spellid].aoerange*spells[AIspells[i].spellid].aoerange || mana_cost == 0)
				) || dist2 <= spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range)
				&& (!zeroPriorityOnly || AIspells[i].priority == 0)
				&& (mana_cost <= GetMana() || GetMana() == GetMaxMana())
				&& (AIspells[i].time_cancast) <= Timer::GetCurrentTime()
				)
			{

#if MobAI_DEBUG_Spells >= 21
				std::cout << "Mob::AICastSpell: Casting: spellid=" << AIspells[i].spellid
					<< ", tar=" << tar->GetName()
					<< ", dist2[" << dist2 << "]<=" << spells[AIspells[i].spellid].range *spells[AIspells[i].spellid].range
					<< ", mana_cost[" << mana_cost << "]<=" << GetMana()
					<< ", cancast[" << AIspells[i].time_cancast << "]<=" << Timer::GetCurrentTime()
					<< ", type=" << AIspells[i].type << std::endl;
#endif
				switch (AIspells[i].type)
				{
					case SpellType_Heal:
					{
						if (
							(spells[AIspells[i].spellid].targettype == ST_Target || tar == this)
							&& !(tar->IsPet() && tar->GetOwner()->IsClient())	//no buffing PC's pets
						)
						{
							uint8 hpr = (uint8)tar->GetHPRatio();

							if (hpr <= 50 || (tar->IsClient() && hpr <= 99))
							{
								AIDoSpellCast(i, tar, mana_cost);

								if (AIspells[i].spellid == 13)		// Complete Heal
									AIspells[i].time_cancast = Timer::GetCurrentTime() + 8000;	// prevent recast for up to 8 seconds if interrupted

								return true;
							}
						}
						break;
					}
					case SpellType_Root:
					{
						Mob *rootee = GetHateRandom();
						if (rootee && !rootee->IsRooted() && (zone->random.Roll(40 + roll_mod) || AIspells[i].priority == 0)
							&& rootee->DontRootMeBefore() < Timer::GetCurrentTime()
							&& DistanceSquared(m_Position, rootee->GetPosition()) < spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range
							&& rootee->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0
							) {
							if(!CheckLosFN(rootee))
								return(false);	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
							uint32 tempTime = 0;
							AIDoSpellCast(i, rootee, mana_cost, &tempTime);
							rootee->SetDontRootMeBefore(tempTime);
							return true;
						}
						break;
					}
					case SpellType_Buff: {
						if (
							(spells[AIspells[i].spellid].targettype == ST_Target || tar == this)
							&& tar->DontBuffMeBefore() < Timer::GetCurrentTime()
							&& !tar->IsImmuneToSpell(AIspells[i].spellid, this)
							&& tar->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0
							&& !(tar->IsPet() && tar->GetOwner()->IsClient() && this != tar)	//no buffing PC's pets, but they can buff themself
							)
						{
							uint32 tempTime = 0;
							AIDoSpellCast(i, tar, mana_cost, &tempTime);
							tar->SetDontBuffMeBefore(tempTime);
							return true;
						}
						break;
					}

					case SpellType_InCombatBuff: {
						if (zone->random.Roll(50) || AIspells[i].priority == 0)
						{
							AIDoSpellCast(i, tar, mana_cost);
							return true;
						}
						break;
					}

					case SpellType_Escape:
					{
						if (!roambox_distance && !IsPet() && GetHPRatio() <= 10.0f && zone->GetZoneExpansion() != ClassicEQ
							&& zone->random.Roll(50) && DistanceSquared(CastToNPC()->GetSpawnPoint(), GetPosition()) > 40000)
						{
							entity_list.MessageClose_StringID(this, true, 200, Chat::Spells, BEGIN_GATE, this->GetCleanName());
							AIDoSpellCast(i, tar, mana_cost);
							AIspells[i].time_cancast = Timer::GetCurrentTime() + 5000;
							return true;
						}
						break;
					}
					case SpellType_Slow:
					{
						Mob * debuffee = GetHateRandom();
						if (debuffee && (zone->random.Roll(70 + roll_mod) || AIspells[i].priority == 0) && debuffee->IsWarriorClass()
							&& DistanceSquared(m_Position, debuffee->GetPosition()) < spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range
							&& debuffee->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0)
						{
							if (spells[AIspells[i].spellid].targettype == ST_AECaster || spells[AIspells[i].spellid].npc_no_los || CheckLosFN(debuffee))
							{
								AIDoSpellCast(i, debuffee, mana_cost);
								return true;
							}
						}
						break;
					}
					case SpellType_Debuff:
					{
						Mob * debuffee = GetHateRandom();
						if (debuffee && (zone->random.Roll(50 + roll_mod) || AIspells[i].priority == 0)
							&& DistanceSquared(m_Position, debuffee->GetPosition()) < spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range
							&& debuffee->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0)
						{
							if (spells[AIspells[i].spellid].targettype == ST_AECaster || spells[AIspells[i].spellid].npc_no_los || CheckLosFN(debuffee))
							{
								AIDoSpellCast(i, debuffee, mana_cost);
								return true;
							}
						}
						break;
					}
					case SpellType_Nuke:
					{
						if (AIspells[i].spellid == SPELL_CAZIC_TOUCH)
						{
							if (tar->IsPet() && tar->GetOwner())
							{
								AIDoSpellCast(i, tar->GetOwner(), 0);
							}
							else
							{
								AIDoSpellCast(i, tar, 0);
							}
							return true;
						}

						if ((spells[AIspells[i].spellid].targettype == ST_AECaster || spells[AIspells[i].spellid].targettype == ST_AETarget)
							&& AIspells[i].priority > 0 && mana_cost > 0 && GetNumHaters() > 0)
						{
							if (GetNumHaters() < 4)
								break;
							if (spells[AIspells[i].spellid].targettype == ST_AECaster
								&& DistanceNoZ(hate_list.GetClosest()->GetPosition(), this->GetPosition()) > spells[AIspells[i].spellid].aoerange)
								break;
						}

						if ((AIspells[i].priority == 0 || zone->random.Roll(40 + roll_mod))
							&& ((mana_cost == 0 || spells[AIspells[i].spellid].buffduration == 0)		// 0 mana spell probably a raid boss spell; these always cast
							&& (GetClass() != CLERIC || mana_cost == 0 || GetHPRatio() > 50.0f)			// clerics don't nuke if they're < 50%
							|| tar->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0))
						{
							if (spells[AIspells[i].spellid].targettype != ST_AECaster && !spells[AIspells[i].spellid].npc_no_los)
							{
								if (!checkedTargetLoS)
								{
									if (!CheckLosFN(tar))
									{
										return false;
									}
									checkedTargetLoS = true;
								}
							}
							AIDoSpellCast(i, tar, mana_cost);
							return true;
						}
						break;
					}
					case SpellType_Dispel:
					{
						if (zone->random.Roll(10) || AIspells[i].priority == 0)
						{
							if (GetClass() == CLERIC && zone->random.Roll(66))	// cleric NPCs have very short spell lists which is causing the AI to make them cast dispels too frequently without this
								return false;

							if (spells[AIspells[i].spellid].targettype != ST_AECaster && !spells[AIspells[i].spellid].npc_no_los)
							{
								if (!checkedTargetLoS)
								{
									if (!CheckLosFN(tar))
									{
										return false;
									}
									checkedTargetLoS = true;
								}
							}
							if (tar->CountDispellableBuffs() > 0)
							{
								AIDoSpellCast(i, tar, mana_cost);
								return true;
							}
						}
						break;
					}
					case SpellType_Mez:
					{
						if (zone->random.Roll(30 + roll_mod) || AIspells[i].priority == 0)
						{
							Mob * mezTar = nullptr;
							mezTar = entity_list.GetTargetForMez(this);

							if(mezTar && mezTar->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0)
							{
								AIDoSpellCast(i, mezTar, mana_cost);
								return true;
							}
						}
						break;
					}

					case SpellType_Charm:
					{
						if(!IsPet() && (zone->random.Roll(30 + roll_mod) || AIspells[i].priority == 0))
						{
							Mob * chrmTar = GetHateRandom();
							if (chrmTar && DistanceSquared(m_Position, chrmTar->GetPosition()) < spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range)
							{
								if (spells[AIspells[i].spellid].targettype == ST_AECaster || spells[AIspells[i].spellid].npc_no_los || CheckLosFN(chrmTar))
								{
									AIDoSpellCast(i, chrmTar, mana_cost);
									return true;
								}
							}
						}
						break;
					}

					case SpellType_Pet: {
						//keep mobs from recasting pets when they have them.
						if (!IsPet() && !GetPetID() && zone->random.Roll(25)) {
							AIDoSpellCast(i, tar, mana_cost);
							return true;
						}
						break;
					}
					case SpellType_Lifetap:
					{
						if (GetHPRatio() <= 95.0f
							&& (zone->random.Roll(50 + roll_mod) || AIspells[i].priority == 0)
							&& tar->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0
						)
						{
							if (spells[AIspells[i].spellid].targettype != ST_AECaster && !spells[AIspells[i].spellid].npc_no_los)
							{
								if (!checkedTargetLoS)
								{
									if (!CheckLosFN(tar))
									{
										return false;
									}
									checkedTargetLoS = true;
								}
							}
							AIDoSpellCast(i, tar, mana_cost);
							return true;
						}
						break;
					}
					case SpellType_Snare:
					{
						if (
							!tar->IsRooted()
							&& (zone->random.Roll(30 + roll_mod) || AIspells[i].priority == 0)
							&& tar->DontSnareMeBefore() < Timer::GetCurrentTime()
							&& tar->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0
							)
						{
							if (spells[AIspells[i].spellid].targettype != ST_AECaster && !spells[AIspells[i].spellid].npc_no_los)
							{
								if (!checkedTargetLoS)
								{
									if (!CheckLosFN(tar))
									{
										return false;
									}
									checkedTargetLoS = true;
								}
							}
							uint32 tempTime = 0;
							AIDoSpellCast(i, tar, mana_cost, &tempTime);
							tar->SetDontSnareMeBefore(tempTime);
							return true;
						}
						break;
					}
					case SpellType_DOT:
					{
						if (
							(zone->random.Roll(40 + roll_mod) || AIspells[i].priority == 0)
							&& tar->CanBuffStack(AIspells[i].spellid, GetLevel(), true) >= 0
						)
						{
							if (spells[AIspells[i].spellid].targettype != ST_AECaster && !spells[AIspells[i].spellid].npc_no_los)
							{
								if (!checkedTargetLoS)
								{
									if (!CheckLosFN(tar))
									{
										return false;
									}
									checkedTargetLoS = true;
								}
							}
							AIDoSpellCast(i, tar, mana_cost);
							return true;
						}
						break;
					}
					default: {
						std::cout << "Error: Unknown spell type in AICastSpell. caster:" << this->GetName() << " type:" << AIspells[i].type << " slot:" << i << std::endl;
						break;
					}
				}
			}
#if MobAI_DEBUG_Spells >= 21
			else {
				std::cout << "Mob::AICastSpell: NotCasting: spellid=" << AIspells[i].spellid << ", tar=" << tar->GetName() << ", dist2[" << dist2 << "]<=" << spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range << ", mana_cost[" << mana_cost << "]<=" << GetMana() << ", cancast[" << AIspells[i].time_cancast << "]<=" << Timer::GetCurrentTime() << std::endl;
			}
#endif
		}
	}
	return false;
}

bool NPC::AIDoSpellCast(uint8 i, Mob* tar, int32 mana_cost, uint32* oDontDoAgainBefore) {

	Log(Logs::Detail, Logs::AI, "Mob::AIDoSpellCast: spellid=%d, tar=%s, mana=%d, Name: %s", AIspells[i].spellid, tar->GetName(), mana_cost, spells[AIspells[i].spellid].name);
	casting_spell_AIindex = i;
	return CastSpell(AIspells[i].spellid, tar->GetID(), EQ::spells::CastingSlot::Gem2, spells[AIspells[i].spellid].cast_time, mana_cost, oDontDoAgainBefore, -1, -1, 0, 0, &(AIspells[i].resist_adjust));
}

bool EntityList::AICheckCloseBeneficialSpells(NPC* caster, uint8 iChance, float iRange, uint16 iSpellTypes) {
	if((iSpellTypes&SpellTypes_Detrimental) != 0) {
		//according to live, you can buff and heal through walls...
		//now with PCs, this only applies if you can TARGET the target, but
		// according to Rogean, Live NPCs will just cast through walls/floors, no problem..
		//
		// This check was put in to address an idle-mob CPU issue
		Log(Logs::General, Logs::Error, "Error: detrimental spells requested from AICheckCloseBeneficialSpells!!");
		return(false);
	}

	if(!caster)
		return false;

	if(caster->AI_HasSpells() == false)
		return false;

	if(caster->GetSpecialAbility(NPC_NO_BUFFHEAL_FRIENDS))
		return false;

	if (iChance < 100) {
		uint8 tmp = zone->random.Int(0, 99);
		if (tmp >= iChance)
			return false;
	}
	if (caster->GetPrimaryFaction() == 0 )
		return(false); // well, if we dont have a faction set, we're gonna be indiff to everybody

	float iRange2 = iRange*iRange;

	float t1, t2, t3;


	//Only iterate through NPCs
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC* mob = it->second;

		if (mob->IsUnTargetable())
			continue;

		//Since >90% of mobs will always be out of range, try to
		//catch them with simple bounding box checks first. These
		//checks are about 6X faster than DistNoRoot on my athlon 1Ghz
		t1 = mob->GetX() - caster->GetX();
		t2 = mob->GetY() - caster->GetY();
		t3 = mob->GetZ() - caster->GetZ();
		//cheap ABS()
		if(t1 < 0)
			t1 = 0 - t1;
		if(t2 < 0)
			t2 = 0 - t2;
		if(t3 < 0)
			t3 = 0 - t3;
		if (t1 > iRange
			|| t2 > iRange
			|| t3 > iRange * 0.2f
			|| DistanceSquared(mob->GetPosition(), caster->GetPosition()) > iRange2
			|| mob->GetReverseFactionCon(caster) >= FACTION_KINDLY
		) {
			continue;
		}

		//since we assume these are beneficial spells, which do not
		//require LOS, we just go for it.
		// we have a winner!
		if((iSpellTypes & SpellType_Buff) && !RuleB(NPC, BuffFriends)){
			if (mob != caster)
				iSpellTypes = SpellType_Heal;
		}

		if (caster->AICastSpell(mob, 100, iSpellTypes))
			return true;
	}
	return false;
}

void Mob::AI_Init() {
	pAIControlled = false;
	AIthink_timer.reset(nullptr);
	AImovement_timer.reset(nullptr);
	AIwalking_timer.reset(nullptr);
	AIscanarea_timer.reset(nullptr);
	AIdoor_timer.reset(nullptr);

	pDontBuffMeBefore = 0;
	pDontRootMeBefore = 0;
	pDontSnareMeBefore = 0;
	pDontCureMeBefore = 0;
}

void NPC::AI_Init()
{
	AIautocastspell_timer.reset(nullptr);
	casting_spell_AIindex = static_cast<uint8>(AIspells.size());

	roambox_max_x = 0;
	roambox_max_y = 0;
	roambox_min_x = 0;
	roambox_min_y = 0;
	roambox_distance = false;
	roambox_movingto_x = 0.0f;
	roambox_movingto_y = 0.0f;
	roambox_movingto_z = 0.0f;
	roambox_min_delay = 2500;
	roambox_delay = 2500;
}

void Client::AI_Init() {
	Mob::AI_Init();
}

void Mob::AI_Start() {
	if (pAIControlled)
		return;
	pAIControlled = true;
	AIthink_timer = std::unique_ptr<Timer>(new Timer(AIthink_duration));
	AIthink_timer->Trigger();
	AIwalking_timer = std::unique_ptr<Timer>(new Timer(0));
	AImovement_timer = std::unique_ptr<Timer>(new Timer(AImovement_duration));
	if (zone->CanDoCombat())
	{
		AIscanarea_timer = std::unique_ptr<Timer>(new Timer(AIscanarea_delay));
	}
	AIhail_timer = std::unique_ptr<Timer>(new Timer(100));
	AIhail_timer->Disable();
	AIpetguard_timer = std::unique_ptr<Timer>(new Timer(500));
	AIdoor_timer = std::unique_ptr<Timer>(new Timer(1250));
	AIloiter_timer = std::unique_ptr<Timer>(new Timer(0));
	AIheading_timer = std::unique_ptr<Timer>(new Timer(2000));
	AIstackedmobs_timer = std::unique_ptr<Timer>(new Timer(1337));

	if (GetAggroRange() == 0)
		pAggroRange = 70;
	if (GetAssistRange() == 0)
		pAssistRange = 70;
	hate_list.Wipe();

	m_Delta = glm::vec4();
	pRunAnimSpeed = 0;
}

void Client::AI_Start() {
	Mob::AI_Start();
	if (!pAIControlled)
		return;

	pClientSideTarget = GetTarget() ? GetTarget()->GetID() : 0;

	// this also interrupts casting client side so the spell gems pop back out
	SendAppearancePacket(AppearanceType::Linkdead, 1); // Sending LD packet so *LD* appears by the player name when charmed/feared -Kasai

	// freeze client
	auto app = new EQApplicationPacket(OP_FreezeClientControl, 65);
	strcpy((char *)app->pBuffer, GetName());
	QueuePacket(app);
	safe_delete(app);

	InterruptSpell(SPELL_UNKNOWN, true);

	SetAttackTimer();
	AI_SetLoiterTimer();
	if(client_state != CLIENT_LINKDEAD)
	{
		SetFeigned(false);
	}
}

void NPC::AI_Start() {
	Mob::AI_Start();
	if (!pAIControlled)
		return;

	if (AIspells.empty()) {
		AIautocastspell_timer = std::unique_ptr<Timer>(new Timer(1000));
		AIautocastspell_timer->Disable();
	} else {
		AIautocastspell_timer = std::unique_ptr<Timer>(new Timer(750));
	}
	AI_AddNPCSpells(NPCTypedata.npc_spells_id);
	ProcessSpecialAbilities(NPCTypedata.special_abilities);
	AI_AddNPCSpellsEffects(NPCTypedata.npc_spells_effects_id);
	SendTo(GetX(), GetY(), GetZ());
	SaveGuardSpot();
	AI_SetLoiterTimer();
	if (roamer || roambox_distance)
		AIloiter_timer->Stop();
}

void Mob::AI_Stop() {
	if (!IsAIControlled())
		return;

	pAIControlled = false;

	AIthink_timer.reset(nullptr);
	AIwalking_timer.reset(nullptr);
	AIscanarea_timer.reset(nullptr);
	AIhail_timer.reset(nullptr);
	AIpetguard_timer.reset(nullptr);
	AIdoor_timer.reset(nullptr);

	hate_list.Wipe();
}

void NPC::AI_Stop() {
	Waypoints.clear();
	AIautocastspell_timer.reset(nullptr);
}

void Client::AI_Stop() {
	Mob::AI_Stop();

	auto app = new EQApplicationPacket(OP_UnfreezeClientControl, 65);
	strcpy((char *)app->pBuffer, GetName());
	QueuePacket(app);
	safe_delete(app);

	//this->Message_StringID(Chat::Red,PLAYER_REGAIN);
	app = new EQApplicationPacket(OP_ReturnClientControl, 0);
	QueuePacket(app);
	safe_delete(app);

	app = new EQApplicationPacket(OP_Charm, sizeof(Charm_Struct));
	Charm_Struct *ps = (Charm_Struct*)app->pBuffer;
	ps->owner_id = 0;
	ps->pet_id = this->GetID();
	ps->command = 0;
	entity_list.QueueClients(this, app);
	safe_delete(app);
	m_Delta = glm::vec4();
	
	SetTarget(entity_list.GetMob(pClientSideTarget));
	SendAppearancePacket(AppearanceType::Animation, GetAppearanceValue(GetAppearance()));
	SendAppearancePacket(AppearanceType::Linkdead, 0); // Removing LD packet so *LD* no longer appears by the player name when charmed/feared -Kasai

	if (!auto_attack) {
		attack_timer.Disable();
		attack_dw_timer.Disable();
	}
	if (IsLD())
	{
		Save();
		OnDisconnect(true);
	}
}

// only call this on a zone shutdown event
void Mob::AI_ShutDown() {	
	attack_timer.Disable();
	attack_dw_timer.Disable();
	ranged_timer.Disable();
	tic_timer.Disable();
	mana_timer.Disable();
	spellend_timer.Disable();
	rewind_timer.Disable();
	bindwound_timer.Disable();
	stunned_timer.Disable();
	spun_timer.Disable();
	bardsong_timer.Disable();
	flee_timer.Disable();
	
	for (int sat = 0; sat < MAX_SPECIAL_ATTACK; ++sat) {
		if (SpecialAbilities[sat].timer)
			 SpecialAbilities[sat].timer->Disable();
		
	}
}

//todo: expand the logic here to cover:
//redundant debuffs
//buffing owner
//certain types of det spells that need special behavior.
void Client::AI_SpellCast()
{
	if(!charm_cast_timer.Check())
		return;

	Mob *targ = GetTarget();
	if(!targ)
		return;

	float dist = DistanceSquaredNoZ(m_Position, targ->GetPosition());

	std::vector<uint32> valid_spells;
	std::vector<uint32> slots;

	for(uint32 x = 0; x < MAX_PP_MEMSPELL; ++x)
	{
		uint32 current_spell = m_pp.mem_spells[x];
		if(!IsValidSpell(current_spell))
			continue;

		if(IsBeneficialSpell(current_spell))
		{
			continue;
		}

		if(dist > spells[current_spell].range*spells[current_spell].range)
		{
			continue;
		}

		if(GetMana() < spells[current_spell].mana)
		{
			continue;
		}

		if(IsEffectInSpell(current_spell, SE_Charm))
		{
			continue;
		}

		if(!GetPTimers().Expired(&database, pTimerSpellStart + current_spell, false))
		{
			continue;
		}

		if(targ->CanBuffStack(current_spell, GetLevel(), true) < 0)
		{
			continue;
		}

		//bard songs cause trouble atm
		if(IsBardSong(current_spell))
			continue;

		valid_spells.push_back(current_spell);
		slots.push_back(x);
	}

	uint32 spell_to_cast = 0xFFFFFFFF;
	EQ::spells::CastingSlot slot_to_use = EQ::spells::CastingSlot::Item;
	if(valid_spells.size() == 1)
	{
		spell_to_cast = valid_spells[0];
		slot_to_use = static_cast<EQ::spells::CastingSlot>(slots[0]);
	}
	else if(valid_spells.empty())
	{
		return;
	}
	else
	{
		uint32 idx = zone->random.Int(0, (valid_spells.size()-1));
		spell_to_cast = valid_spells[idx];
		slot_to_use = static_cast<EQ::spells::CastingSlot>(slots[idx]);
	}

	if(IsMezSpell(spell_to_cast) || IsFearSpell(spell_to_cast))
	{
		Mob *tar = entity_list.GetTargetForMez(this);
		if(!tar)
		{
			tar = GetTarget();
			if(tar && IsFearSpell(spell_to_cast))
			{
				if(!IsBardSong(spell_to_cast))
				{
					StopNavigation();
				}
				CastSpell(spell_to_cast, tar->GetID(), slot_to_use);
				return;
			}
		}
	}
	else
	{
		Mob *tar = GetTarget();
		if(tar)
		{
			if(!IsBardSong(spell_to_cast))
			{
				StopNavigation();
			}
			CastSpell(spell_to_cast, tar->GetID(), slot_to_use);
			return;
		}
	}


}

void Client::AI_Process()
{
	if (!IsAIControlled())
		return;

	bool ai_think = AIthink_timer->Check();

	if (!(ai_think || attack_timer.Check(false)))
		return;

	if (IsCasting())
		return;

	Mob *owner = GetOwner();
	if (!owner)
	{
		if (!IsFeared() && !IsLD())
		{
			BuffFadeByEffect(SE_Charm);
			return;
		}
	}

	bool hatelist_empty = hate_list.IsEmpty();

	if (!hatelist_empty)
	{
		if ((IsRooted() && !permarooted) || IsBlind()) {
			Mob* closest_client = hate_list.GetClosestClient(this);
			if (closest_client && IsInCombatRange(closest_client)) {
				SetTarget(closest_client);
			}
			else {
				Mob* closest_npc = hate_list.GetClosestNPC(this);
				if (closest_npc && IsInCombatRange(closest_npc))
					SetTarget(closest_npc);
				else
					SetTarget(hate_list.GetTop());
			}
		}
		else
		{
			SetTarget(hate_list.GetTop());
		}
	}
	else
	{
		if (owner)
		{
			if (owner->IsEngaged())
			{
				Mob *tar = owner->GetTarget();
				if (tar)
				{
					AddToHateList(tar, 1, 0);
					hatelist_empty = false;
					SetTarget(tar);
				}
			}
		}
	}
	Mob *cur_tar = GetTarget();

	if (!hatelist_empty && cur_tar)
		engaged = true;
	else
		engaged = false;

	if (RuleB(Combat, EnableFearPathing)) {
		if (IsFeared()) {

			if (IsRooted()) {
				//make sure everybody knows were not moving, for appearance sake
				if (IsMoving()) {
					FaceTarget();
				}
				//continue on to attack code, ensuring that we execute the engaged code
				engaged = true;
			}
			else {
				if (AImovement_timer->Check()) {
					// Check if we have reached the last fear point
					if (IsPositionEqualWithinCertainZ(glm::vec3(GetX(), GetY(), GetZ()), m_FearWalkTarget, 5.0f)) {
						FixZ(true);
						curfp = false;
						CalculateNewFearpoint();
						if (curfp)
							RunTo(m_FearWalkTarget.x, m_FearWalkTarget.y, m_FearWalkTarget.z);
						else
							StopNavigation();
					}
					else {
						RunTo(m_FearWalkTarget.x, m_FearWalkTarget.y, m_FearWalkTarget.z);
					}
				}
				entity_list.ProcessMove(this, glm::vec3(GetX(), GetY(), GetZ()));

				m_Proximity = glm::vec3(GetX(), GetY(), GetZ());

				return;
			}

		}
	}
	
	if (engaged)
	{
		if (camp_timer.Enabled())
		{
			camp_timer.Disable();
			camping = false;
			camp_desktop = false;
		}

		if (!GetTarget())
			return;

		if (GetTarget()->IsCorpse()) {
			RemoveFromHateList(this);
			SetTarget(nullptr);
			return;
		}
		//Todo: Figure out why 'this' can become invalid here. It is related to Fear.
		if (this)
		{
			if (DivineAura())
				return;
		}
		else
		{
			Log(Logs::Detail, Logs::Error, "Preventing DivineAura() crash due to null this.");
			return;
		}

		bool is_combat_range = IsInCombatRange(GetTarget());
		if (is_combat_range && IsMoving()) {
			// this is for mobs that might still be pathing to get LOS
			glm::vec3 my_loc(m_Position.x, m_Position.y, m_Position.z);
			glm::vec3 tar_pos(cur_tar->GetX(), cur_tar->GetY(), cur_tar->GetZ());
			bool has_los = zone->zonemap->CheckLoS(my_loc, tar_pos);
			if (!has_los && Distance(my_loc, tar_pos) > 14.0f)
				is_combat_range = false;
		}
		else if (!is_combat_range) {
			// this is for when target is high above
			if ((std::abs(m_Position.x - cur_tar->GetX()) < 2.0f) && (std::abs(m_Position.y - cur_tar->GetY()) < 2.0f)) {
				// this is for mobs that might still be pathing to get LOS
				glm::vec3 my_loc(m_Position.x, m_Position.y, m_Position.z);
				glm::vec3 tar_pos(cur_tar->GetX(), cur_tar->GetY(), cur_tar->GetZ());
				if (zone->zonemap->CheckLoS(my_loc, tar_pos))
					is_combat_range = true;
			}
		}
		if (is_combat_range) {
			if (charm_class_attacks_timer.CheckKeepSynchronized()) {
				DoClassAttacks(GetTarget());
			}
			if (IsMoving()) {
				StopNavigation();
			}
			if (ai_think) {
				SetRunAnimation(0.0f);

				if (!IsFacingTarget()) {
					FaceTarget(cur_tar);
				}
			}

			if (GetTarget() && !IsStunned() && !IsMezzed() && !IsFeigned())
			{
				if (attack_timer.CheckKeepSynchronized())
				{
					TryProcs(GetTarget(), EQ::invslot::slotPrimary);
					Attack(GetTarget(), EQ::invslot::slotPrimary);

					if (GetTarget())
					{
						CheckIncreaseSkill(EQ::skills::SkillDoubleAttack, GetTarget(), zone->skill_difficulty[EQ::skills::SkillDoubleAttack].difficulty);
						if (CheckDoubleAttack())
						{
							Attack(GetTarget(), EQ::invslot::slotPrimary);

							// Triple attack: Warriors and Monks level 60+ do this.  13.5% looks weird but multiple 8+ hour logs suggest it's about that
							if ((GetClass() == WARRIOR || GetClass() == MONK) && GetLevel() >= 60 && zone->random.Int(0, 999) < 135)
							{
								Attack(GetTarget(), EQ::invslot::slotPrimary);

								// Flurry AA
								if (GetTarget() && aabonuses.FlurryChance)
								{
									if (zone->random.Int(0, 99) < aabonuses.FlurryChance)
									{
										Message_StringID(Chat::NPCFlurry, YOU_FLURRY);
										Attack(GetTarget(), EQ::invslot::slotPrimary);

										if (zone->random.Roll(10))							// flurry is usually only +1 swings
											Attack(GetTarget(), EQ::invslot::slotPrimary);
									}
								}
							}
						}
					}

					// Punishing Blade and Speed of the Knight AAs
					if (GetTarget() && aabonuses.ExtraAttackChance)
					{
						EQ::ItemInstance* wpn = GetInv().GetItem(EQ::invslot::slotPrimary);
						if (wpn)
						{
							if (wpn->GetItem()->ItemType == EQ::item::ItemType2HSlash ||
								wpn->GetItem()->ItemType == EQ::item::ItemType2HBlunt ||
								wpn->GetItem()->ItemType == EQ::item::ItemType2HPiercing)
							{
								if (zone->random.Int(0, 99) < aabonuses.ExtraAttackChance)
								{
									Attack(GetTarget(), EQ::invslot::slotPrimary);
								}
							}
						}
					}
				}
			}

			if (GetTarget() && !IsStunned() && !IsMezzed() && !IsFeigned() && attack_dw_timer.CheckKeepSynchronized() && IsDualWielding())
			{
				CheckIncreaseSkill(EQ::skills::SkillDualWield, GetTarget(), zone->skill_difficulty[EQ::skills::SkillDualWield].difficulty);
				if (CheckDualWield())
				{
					TryProcs(GetTarget(), EQ::invslot::slotSecondary);
					Attack(GetTarget(), EQ::invslot::slotSecondary);

					if (GetSkill(EQ::skills::SkillDoubleAttack) >= 150 && CheckDoubleAttack())
					{
						Attack(GetTarget(), EQ::invslot::slotSecondary);
					}
				}
			}
		}
		else
		{
			if (!IsRooted())
			{
				if (AImovement_timer->Check())
				{
					//SetRunning(true);
					if (m_Navigation.x != GetTarget()->GetX() || m_Navigation.y != GetTarget()->GetY()) {
						m_Navigation = GetTarget()->GetPosition();
						if (GetTarget()->IsClient()) {
							glm::vec3 dest(m_Navigation.x, m_Navigation.y, m_Navigation.z);
							if (GetTarget()->GetZOffset() < 5.0f)
								m_Navigation.z += (5.0f - GetTarget()->GetZOffset());
							float new_z = zone->zonemap->FindGround(dest, nullptr);
							if (new_z == BEST_Z_INVALID)
								new_z = zone->zonemap->FindBestZ(dest, nullptr, 20.0f, GetTarget()->GetZOffset());
							if (new_z == BEST_Z_INVALID)
								new_z = zone->zonemap->FindBestZ(dest, nullptr);
							if (new_z != BEST_Z_INVALID)
								m_Navigation.z = new_z + GetZOffset();
						}
						else {
							glm::vec3 dest(m_Navigation.x, m_Navigation.y, m_Navigation.z);
							if (GetTarget()->GetZOffset() < 5.0f)
								m_Navigation.z += (5.0f - GetTarget()->GetZOffset());
							float new_z = zone->zonemap->FindBestZ(dest, nullptr, 20.0f, GetTarget()->GetZOffset());
							if (new_z == BEST_Z_INVALID)
								new_z = zone->zonemap->FindBestZ(dest, nullptr);
							if (new_z != BEST_Z_INVALID)
								m_Navigation.z = new_z + GetZOffset();
						}
					}
					RunTo(GetTarget()->GetX(), GetTarget()->GetY(), m_Navigation.z);
				}
			}
			else if (IsMoving())
			{
				FaceTarget();
			}
		}
		AI_SpellCast();
	}
	else
	{
		if (IsPet()) {
			Mob* owner = GetOwner();
			if (owner == nullptr)
				return;

			float dist = DistanceSquared(m_Position, owner->GetPosition());
			if (dist >= 400.0f) { // >=20
				if (AImovement_timer->Check()) {
					if (m_Navigation.x != owner->GetX() || m_Navigation.y != owner->GetY()) {
						m_Navigation = owner->GetPosition();
						glm::vec3 dest(owner->GetX(), owner->GetY(), owner->GetZ());
						if (owner->IsClient()) {
							bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(dest) || zone->IsWaterZone(dest.z);
							if (!in_liquid) {
								// we are not in water
								if (owner->FindType(SE_Levitate) || owner->GetFlyMode() != 0) {
									// we are flying - use ground
									float new_z = zone->zonemap->FindBestZ(dest, nullptr);
									if (new_z != BEST_Z_INVALID) {
										m_Navigation.z = SetBestZ(new_z);
									}
								}
								else {
									float new_z = zone->zonemap->FindBestZ(dest, nullptr);
									if (new_z != BEST_Z_INVALID)
										m_Navigation.z = SetBestZ(new_z);
								}
							}
							else {
								float new_z = zone->zonemap->FindBestZ(dest, nullptr);
								if (new_z != BEST_Z_INVALID)
									m_Navigation.z = SetBestZ(new_z);
							}

						}
						else {
							float new_z = zone->zonemap->FindBestZ(dest, nullptr, 20.0f, owner->GetZOffset());
							if (new_z == BEST_Z_INVALID)
								new_z = zone->zonemap->FindBestZ(dest, nullptr);
							if (new_z != BEST_Z_INVALID)
								m_Navigation.z = SetBestZ(new_z);
						}
					}
					if (dist >= 5625.0f) {
						RunTo(owner->GetX(), owner->GetY(), m_Navigation.z);
					}
					else {
						WalkTo(owner->GetX(), owner->GetY(), m_Navigation.z);
					}
				}
			}
			else {
				StopNavigation();
			}
		}
		if (IsLD() && !camping && !client_ld_timer.Enabled()) {
			client_ld_timer.Start(CLIENT_LD_TIMEOUT, true);
		}
	}
}

void Mob::DoMainHandRound(Mob* victim, int damagePct)
{
	if (!victim && !target)
		return;
	if (!victim)
		victim = target;

	if (IsNPC())
	{
		int16 n_atk = CastToNPC()->GetNumberOfAttacks();

		if (n_atk <= 1)
		{
			Attack(victim, EQ::invslot::slotPrimary, damagePct);
		}
		else
		{
			for (int i = 0; i < n_atk; ++i)
			{
				Attack(victim, EQ::invslot::slotPrimary, damagePct);
			}
		}
	}
	else
	{
		Attack(victim, EQ::invslot::slotPrimary, damagePct);
	}

	TryProcs(victim, EQ::invslot::slotPrimary);

	if (CheckDoubleAttack())
	{
		Attack(victim, EQ::invslot::slotPrimary, damagePct);

		// Triple attack: Warriors and Monks level 60+ do this.  13.5% looks weird but multiple 8+ hour logs suggest it's about that
		if ((GetClass() == WARRIOR || GetClass() == MONK) && GetLevel() >= 60 && zone->random.Int(0, 999) < 135)
		{
			Attack(victim, EQ::invslot::slotPrimary, damagePct);
		}
	}
}

void Mob::DoOffHandRound(Mob* victim, int damagePct)
{
	if (!victim && !target)
		return;
	if (!victim)
		victim = target;

	TryProcs(victim, EQ::invslot::slotSecondary);

	Attack(victim, EQ::invslot::slotSecondary, damagePct);

	if (GetSkill(EQ::skills::SkillDoubleAttack) >= 150 && CheckDoubleAttack())
	{
		Attack(victim, EQ::invslot::slotSecondary, damagePct);
	}
}

void Mob::DoFearMovement()
{
	if (IsRooted()) {
		//make sure everybody knows were not moving, for appearance sake
		if (IsMoving()) {
			FaceTarget();
		}
		//continue on to attack code, ensuring that we execute the engaged code
		engaged = true;
	}
	else {
		float f_speed;
		if (IsBlind() && !IsFeared())
			f_speed = GetRunspeed();
		else
			f_speed = GetFearSpeed();
		//if (f_speed > 0.1f) {
			if (AImovement_timer->Check()) {
				// Check if we have reached the last fear point
				if (IsPositionEqualWithinCertainZ(glm::vec3(GetX(), GetY(), GetZ()), m_FearWalkTarget, 5.0f)) {
					FixZ(true);
					curfp = false;
					CalculateNewFearpoint();
					if (curfp)
						RunTo(m_FearWalkTarget.x, m_FearWalkTarget.y, m_FearWalkTarget.z);
					else
						StopNavigation();
				}
				else {
					RunTo(m_FearWalkTarget.x, m_FearWalkTarget.y, m_FearWalkTarget.z);
				}
			}
		//}
	}
}

void Mob::AI_Process() {

	if (!IsAIControlled())
		return;

	if (IsNPC() && !spawnpacket_sent)
		return;

	bool ai_think = AIthink_timer->Check();

	if (!(ai_think || attack_timer.Check(false)))
		return;

	bool is_engaged = !zone->CanDoCombat(this, target, true) ? false : !hate_list.IsEmpty();

	if (is_engaged && IsNPC() && CastToNPC()->CheckPushTimer())
	{
		if (!IsMoving())
			CastToNPC()->ApplyPushVector();
		else
			CastToNPC()->ResetPushVector();

	}

	if (IsCasting() && GetClass() != BARD)
		return;

	bool facing_set = false;
	bool doranged = false;

	if (IsHeld() && IsNPC() && GetOwner() && GetOwner()->IsClient() && GetOwner()->CastToClient()->GetAA(aaPetDiscipline)) {
		is_engaged = false;
		SetTarget(nullptr);
	}

	if (is_engaged)
	{
		if (IsRooted() && !permarooted && !IsMezzed() && !IsStunned())
		{
			Mob* closest_client = hate_list.GetClosestClient(this);
			if (closest_client && IsInCombatRange(closest_client)) {
				SetTarget(closest_client);
			}
			else {
				Mob* closest_npc = hate_list.GetClosestNPC(this);
				if (closest_npc && IsInCombatRange(closest_npc))
					SetTarget(closest_npc);
				else
					SetTarget(hate_list.GetTop());
			}

			if (IsMoving())
			{
				FaceTarget();
				facing_set = true;
			}
		}
		else
		{
			SetTarget(hate_list.GetTop());
		}
	}

	if (is_engaged && GetTarget() && !IsEngaged())
	{
		this->engaged = true;
		if (IsNPC() && GetTarget()->IsClient())
			parse->EventNPC(EVENT_AGGRO, this->CastToNPC(), GetTarget(), "", 0);
		AI_Event_Engaged(GetTarget());

		if (IsNPC() && CastToNPC()->GetDepop()) {
			// scripted mobs that work like traps are set to depop right away
			// so clear their target, otherwise they engage and attack
			SetTarget(nullptr);
		}
	}
	else if ((!is_engaged || !GetTarget()) && IsEngaged())
	{
		this->engaged = false;
		AI_Event_NoLongerEngaged();
	}

	if (GetIgnoreStuckCount() > 50)
	{
		// NPC is stuck in place trying to reach a player because geometry runs them out of ignore radius
		// Wipe hate list of mob and any mobs nearby who are also stuck
		std::list<Mob*> npcList;
		entity_list.GetNearestNPCs(this, npcList, GetAssistRange()+50.f, true);
		Mob* listMob;

		auto iter = npcList.begin();
		while (iter != npcList.end())
		{
			listMob = *iter;
			if (listMob->GetIgnoreStuckCount() > 0)
			{
				listMob->WipeHateList(true);
			}
			++iter;
		}
		WipeHateList(true);
		return;
	}

	if (IsMezzed() || IsStunned())
		return; // need to be able to gain or drop aggro and push while mezzed and stunned, so we run AI_Process() up to here

	if (RuleB(Combat, EnableFearPathing))
	{
		if ((IsFearedNoFlee() || (IsFleeing() && is_engaged)) && curfp && (!IsRooted() || (permarooted && GetSpecialAbility(PERMAROOT_FLEE))))
		{
			DoFearMovement();
			return;
		}
	}

	// trigger EVENT_SIGNAL if required
	if(ai_think && IsNPC()) {
		CastToNPC()->CheckSignal();
	}

	if (GetShieldTarget())
	{
		if (GetShieldTarget()->IsCorpse() || GetShieldTarget()->GetHP() < 1	|| shield_timer.Check()
			|| DistanceSquared(GetPosition(), GetShieldTarget()->GetPosition()) > (16.0f*16.0f)
		)
			EndShield();
	}

	if (GetTarget())
	{
		if (GetTarget()->IsCorpse() || IsBoat())
		{
			RemoveFromHateList(GetTarget());
			return;
		}

		if (AIloiter_timer->Enabled())
			AIloiter_timer->Pause();

        auto npcSpawnPoint = CastToNPC()->GetSpawnPoint();
		if(GetSpecialAbility(TETHER)) {
			float tether_range = static_cast<float>(GetSpecialAbilityParam(TETHER, 0));
			tether_range = tether_range > 0.0f ? tether_range * tether_range : pAggroRange * pAggroRange;

			if(DistanceSquaredNoZ(m_Position, npcSpawnPoint) > tether_range) {
				GMMove(npcSpawnPoint.x, npcSpawnPoint.y, npcSpawnPoint.z, npcSpawnPoint.w);
			}
		} else if(GetSpecialAbility(LEASH)) {
			float leash_range = static_cast<float>(GetSpecialAbilityParam(LEASH, 0));
			leash_range = leash_range > 0.0f ? leash_range * leash_range : pAggroRange * pAggroRange;

			if(DistanceSquaredNoZ(m_Position, npcSpawnPoint) > leash_range) {
				GMMove(npcSpawnPoint.x, npcSpawnPoint.y, npcSpawnPoint.z, npcSpawnPoint.w);
				SetHP(GetMaxHP());
				BuffFadeAll();
				WipeHateList(true);
				AIloiter_timer->Trigger();
				return;
			}
		}

		// if NPC warrior, look for injured allies to /shield
		// note that if you change the number of times that the AI loop runs per second, you'll need to change this roll number with it
		if (IsNPC() && GetClass() == WARRIOR && GetLevel() > 29 && !GetShieldTarget() && !IsStunned() && !IsMezzed() && !IsFeared()
			&& shield_cooldown < Timer::GetCurrentTime() && IsEngaged() && zone->random.Roll(0.0001))
		{
			std::list<Mob*> npcList;
			entity_list.GetNearestNPCs(this, npcList, 15.0f, true, true);
			Mob* listMob;

			auto iter = npcList.begin();
			while (iter != npcList.end())
			{
				listMob = *iter;
				if (listMob->GetHPRatio() < 50.0f && !listMob->GetShielder())
				{
					StartShield(listMob);
					break;
				}
				++iter;
			}
		}

		bool is_combat_range = IsInCombatRange(GetTarget());
		if (is_combat_range && IsMoving()) {
			// this is for mobs that might still be pathing to get LOS
			glm::vec3 my_loc(m_Position.x, m_Position.y, m_Position.z);
			glm::vec3 tar_pos(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());
			bool has_los = zone->zonemap->CheckLoS(my_loc, tar_pos);
			if (!has_los && Distance(my_loc, tar_pos) > 14.0f)
				is_combat_range = false;
		} else if (!is_combat_range) {
			// this is for when target is high above
			if ((std::abs(m_Position.x - GetTarget()->GetX()) < 2.0f) && (std::abs(m_Position.y - GetTarget()->GetY()) < 2.0f)) {
				// this is for mobs that might still be pathing to get LOS
				glm::vec3 my_loc(m_Position.x, m_Position.y, m_Position.z);
				glm::vec3 tar_pos(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());
				if (zone->zonemap->CheckLoS(my_loc, tar_pos))
					is_combat_range = true;
			}
		}
		if (is_combat_range)
		{
			if (IsMoving()) {
				FaceTarget(GetTarget());
				facing_set = true;
				
				glm::vec3 my_loc(m_Position.x, m_Position.y, m_Position.z);
				glm::vec3 tar_pos(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());
				bool has_los = zone->zonemap->CheckLoS(my_loc, tar_pos);
				if (!has_los && !GetTarget()->IsMoving()) {
					// warp on top of target?
					if (zone->HasWaterMap() && zone->watermap->InLiquid(tar_pos) || zone->IsWaterZone(tar_pos.z)) {
						// find our position above and below.
						float ceiling = zone->zonemap->FindCeiling(tar_pos, nullptr);
						float ground = zone->zonemap->FindGround(tar_pos, nullptr);

						if (tar_pos.z < (ground + GetZOffset()))
							tar_pos.z = ground + GetZOffset();
						if (tar_pos.z > ceiling) {
							tar_pos.z = ceiling - 1.0f;
						}
					}
					else {
						// has water and target not in water
						if (GetTarget()->IsClient()) {
							float new_z = zone->zonemap->FindBestZ(tar_pos, nullptr);
							if (new_z != BEST_Z_INVALID)
								tar_pos.z = SetBestZ(new_z);
						}
						else {
							float new_z = zone->zonemap->FindBestZ(tar_pos, nullptr, 20.0f);
							if (new_z == BEST_Z_INVALID)
								new_z = zone->zonemap->FindBestZ(tar_pos, nullptr);
							if (new_z != BEST_Z_INVALID)
								tar_pos.z = SetBestZ(new_z);
						}

					}
					Teleport(tar_pos);
				}
			}
	
			if (ai_think && !facing_set && !IsFacingTarget())
			{
				FaceTarget(GetTarget());
			}

			//casting checked above...
			if(GetTarget() && !IsStunned() && !IsMezzed() && GetAppearance() != eaDead && !IsMeleeDisabled()) {
				Mob *victim = GetTarget(); // the victim can die during this block and the NPC will retarget to the next mob on its hate list so need to hold onto this to check if it HasDied() before performing more attacks

				if (IsPet() && GetPetOrder() == SPO_Sit && GetOwner()->IsClient()) {
					SendAppearancePacket(AppearanceType::Animation, Animation::Standing);
					SetPetOrder(SPO_Follow);
				}

				//try main hand first
				if(attack_timer.CheckKeepSynchronized())
				{
					bool specialed = false;				// NPCs may only do one special attack per round

					DoMainHandRound(victim);

					if (victim && !victim->HasDied() && GetSpecialAbility(SPECATK_FLURRY))
					{
						int flurry_chance = GetSpecialAbilityParam(SPECATK_FLURRY, 0);
						flurry_chance = flurry_chance > 0 ? flurry_chance : RuleI(Combat, NPCFlurryChance);

						if (zone->random.Roll(flurry_chance))
						{
							Flurry();
							specialed = true;
						}
					}
					if (IsPet() && HasOwner() && GetOwner()->IsClient() && GetOwner()->CastToClient()->CheckAAEffect(aaEffectFrenziedBurnout)) {
						if (zone->random.Roll(5))
						{
							Flurry();
							specialed = true;
						}
						else if (zone->random.Roll(10))
						{
							Rampage(75, 100);
							specialed = true;
						}
					}

					if (IsPet() || (IsNPC() && CastToNPC()->GetSwarmOwner()))
					{
						Mob *owner = nullptr;

						if (IsPet())
							owner = GetOwner();
						else
							owner = entity_list.GetMobID(CastToNPC()->GetSwarmOwner());

						if (owner && victim && !victim->HasDied())
						{
							int16 flurry_chance = owner->aabonuses.PetFlurry +
								owner->spellbonuses.PetFlurry + owner->itembonuses.PetFlurry;

							if (flurry_chance && zone->random.Roll(flurry_chance))
								Flurry();
						}
					}

					if (victim && !victim->HasDied() && GetSpecialAbility(SPECATK_RAMPAGE) && !specialed)
					{
						int rampage_chance = GetSpecialAbilityParam(SPECATK_RAMPAGE, 0);
						rampage_chance = rampage_chance > 0 ? rampage_chance : 20;

						if(zone->random.Roll(rampage_chance))
						{
							int range = GetSpecialAbilityParam(SPECATK_RAMPAGE, 1);
							int damage_percent = GetSpecialAbilityParam(SPECATK_RAMPAGE, 2);

							if (range <= 0)
								range = 75;
							if (damage_percent <= 0)
								damage_percent = 100;

							Rampage(range, damage_percent);
							specialed = true;
						}
					}

					if (GetSpecialAbility(SPECATK_AREA_RAMPAGE) && !specialed)
					{
						int rampage_chance = GetSpecialAbilityParam(SPECATK_AREA_RAMPAGE, 0);
						rampage_chance = rampage_chance > 0 ? rampage_chance : 20;

						int rampage_targets = GetSpecialAbilityParam(SPECATK_AREA_RAMPAGE, 1);
						if (rampage_targets <= 0)
							rampage_targets = 999;

						if(zone->random.Roll(rampage_chance))
						{
							int damage_percent = GetSpecialAbilityParam(SPECATK_AREA_RAMPAGE, 2);
							if (damage_percent <= 0)
								damage_percent = 100;

							AreaRampage(rampage_targets, damage_percent);
							specialed = true;
						}
					}
				}

				//now off hand
				if (attack_dw_timer.CheckKeepSynchronized() && victim && !victim->HasDied() && IsDualWielding())
				{
					DoOffHandRound(victim);
				}

				//now special attacks (kick, etc)
				if(victim && !victim->HasDied() && IsNPC())
					CastToNPC()->DoClassAttacks(victim);
			}
			AI_EngagedCastCheck();
		}	//end is within combat range
		else
		{
			// Target is outside of melee range
			if (IsRooted() && !permarooted && ai_think && GetTarget() && !IsFacingTarget())
			{
				FaceTarget(GetTarget());
				facing_set = true;
			}

			// max mobs in kite code
			if (ai_think && GetTarget() && GetTarget()->IsClient() && (zone->GetNumAggroedNPCs() > zone->GetPullLimit()) && entity_list.GetTopHateCount(GetTarget()) >= zone->GetPullLimit() && zone->GetGuildID() == GUILD_NONE)
			{
				int limit = zone->GetPullLimit();
				int limit2 = limit * 15 / 10;

				// reduce limit if there are multiple kiters
				if (limit > 10 && zone->GetNumAggroedNPCs() > limit2)
				{
					limit = std::max(limit - (zone->GetNumAggroedNPCs() - limit2) / 2, 10);
				}
				
				if (entity_list.GetTopHateCount(GetTarget()) > limit)
				{
					if (GetAggroDeaggroTime() > 30000)
					{
						if (zone->random.Roll(5) && DistanceSquared(this->GetPosition(), GetTarget()->GetPosition()) < (500.0f*500.0f))
						{
							GMMove(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());	// warping like this was actually very common on AK
						}
						else
							//WipeHateList();

						return;
					}
				}
			}

			// See if we can summon the mob to us
			if (GetTarget()) {
				if (CheckHateSummon(GetTarget()))
				{
					HateSummon(GetTarget());
				}
				else if (RuleB(Quarm, EnableBardInstagibLimit) && GetTarget()->IsClient() && GetTarget()->GetClass() == BARD && CheckBardHateSummon(GetTarget()))
				{
					BardHateSummon(GetTarget());
				}
				else if (!IsBlind())
				{
					//could not summon them, check ranged...
					if (GetSpecialAbility(SPECATK_RANGED_ATK) || HasBowAndArrowEquipped()) {
						doranged = true;
					}

					// Now pursue
					if (AI_EngagedCastCheck()) {
						if (IsCasting() && GetClass() != BARD) {
							FaceTarget(GetTarget());
						}
					}
					else if (ai_think && GetTarget())
					{
						if (AIstackedmobs_timer->Check() && zone->random.Roll(33) && entity_list.StackedMobsCount(this) > 4) {
							if (GetHPRatio() < 95.0f && zone->random.Roll(5) && (GetZoneID() == Zones::POSTORMS || GetZoneID() == Zones::POVALOR || GetZoneID() == Zones::HOHONORA || GetZoneID() == Zones::POFIRE || GetZoneID() == Zones::POTACTICS)) {
								glm::vec3 tar_pos(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());

								// warp on top of target
								if (zone->HasWaterMap() && zone->watermap->InLiquid(tar_pos) || zone->IsWaterZone(tar_pos.z)) {
									// find our position above and below.
									float ceiling = zone->zonemap->FindCeiling(tar_pos, nullptr);
									float ground = zone->zonemap->FindGround(tar_pos, nullptr);

									if (tar_pos.z < (ground + GetZOffset()))
										tar_pos.z = ground + GetZOffset();
									if (tar_pos.z > ceiling) {
										tar_pos.z = ceiling - 1.0f;
									}
								}
								else {
									// has water and target not in water
									if (GetTarget()->IsClient()) {
										float new_z = zone->zonemap->FindBestZ(tar_pos, nullptr);
										if (new_z != BEST_Z_INVALID)
											tar_pos.z = SetBestZ(new_z);
									}
									else {
										float new_z = zone->zonemap->FindBestZ(tar_pos, nullptr, 20.0f);
										if (new_z == BEST_Z_INVALID)
											new_z = zone->zonemap->FindBestZ(tar_pos, nullptr);
										if (new_z != BEST_Z_INVALID)
											tar_pos.z = SetBestZ(new_z);
									}
								}
								Teleport(tar_pos);
							}
							else {
								StopNavigation();
								AIthink_timer->Start(150 + rand() % 200, false);
							}
							return;
						}
						if(!IsRooted()) {
							Log(Logs::Detail, Logs::AI, "Pursuing %s while engaged.", GetTarget()->GetName());
							if (m_Navigation.x != GetTarget()->GetX() || m_Navigation.y != GetTarget()->GetY()) {
								m_Navigation = GetTarget()->GetPosition();

								glm::vec3 dest(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());
								if (GetTarget()->IsClient()) {
									if (zone->HasWaterMap() && zone->watermap->InLiquid(dest) || zone->IsWaterZone(dest.z)) {
										// we are in water, so target is client
										// find our position above and below.
										float ceiling = zone->zonemap->FindCeiling(dest, nullptr);
										float ground = zone->zonemap->FindGround(dest, nullptr);

										if (ground != BEST_Z_INVALID && dest.z < (ground + GetZOffset()))
											dest.z = ground + GetZOffset();
										if (ceiling != BEST_Z_INVALID && dest.z > ceiling) {
											dest.z = ceiling - 1.0f;
										}
										m_Navigation.z = dest.z;
									}
									else if (GetTarget()->FindType(SE_Levitate) || (GetTarget()->GetFlyMode() == 1 || GetTarget()->GetFlyMode() == 2)) {
										// we are flying - use ground
										float new_z = zone->zonemap->FindBestZ(dest, nullptr);
										if (new_z != BEST_Z_INVALID) {
											m_Navigation.z = SetBestZ(new_z);
										}
									}
								}
								else {
									bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(dest) || zone->IsWaterZone(dest.z);
									if (!in_liquid) {
										float newz = zone->zonemap->FindBestZ(dest, nullptr, 20.0f, GetTarget()->GetZOffset());
										if (newz == BEST_Z_INVALID)
											newz = zone->zonemap->FindBestZ(dest, nullptr);

										if (newz != BEST_Z_INVALID)
											newz = SetBestZ(newz);
										if (newz != BEST_Z_INVALID && std::abs(newz - dest.z) < 50.0f)
										{
											glm::vec3 calc_loc(dest.x, dest.y, newz);
											bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(calc_loc) || zone->IsWaterZone(newz);
											if (!in_liquid)
												m_Navigation.z = newz;
										}
									}
									else {
										// target in water
										float ceiling = zone->zonemap->FindCeiling(dest, nullptr);
										float ground = zone->zonemap->FindGround(dest, nullptr);

										if (dest.z < (ground + GetZOffset()))
											dest.z = ground + GetZOffset();
										if (dest.z > ceiling) {
											dest.z = ceiling - 1.0f;
										}
										m_Navigation.z = dest.z;
									}
								}
							}
							RunTo(GetTarget()->GetX(), GetTarget()->GetY(), m_Navigation.z);

							if (IsNPC())
								CastToNPC()->SetNotCornered();
						}
						else if(IsMoving() && !IsFeared()) {
							FaceTarget();
						}
					}
				}
			} // summon and pursuit
			if (ai_think && IsBlind() && !AI_EngagedCastCheck() && !IsRooted() && curfp)
			{
				// target is not in melee range and NPC is blind
				DoFearMovement();
				return;
			}
		} // target not in combat range
	} // if (target)
	else
	{
		//underwater stuff only works with water maps in the zone!
		if (ai_think && IsNPC() && CastToNPC()->IsUnderwaterOnly() && hate_list.HasLandHaters() && hate_list.GetTop() == nullptr)
		{
			// all haters are outside of water
			if (cur_hp < max_hp)
				Heal();
		}
		if (AI_IdleCastCheck())
		{
			if (IsCasting() && GetClass() != BARD) {
				StopNavigation();
			}
		}
		else if (AIscanarea_timer != nullptr && AIscanarea_timer->Check())
		{
			/*
			* This is where NPCs look around to see if they want to attack other NPCs.
			* It it only used if the NPC has npc_aggro enabled.
			*/
			if (IsNPC() && !zone->IsIdling())
			{
				if (CastToNPC()->WillAggroNPCs())
				{
					// if attackable NPC found then also check for clients so both NPCs and clients end up on hate list
					if (entity_list.AICheckNPCAggro(this->CastToNPC()))
						entity_list.AICheckClientAggro(this->CastToNPC());
				}

				if(!IsPet() && zone->HasCharmedNPC)
					entity_list.AICheckPetAggro(this->CastToNPC());
			}
		}
		else if (ai_think && (!IsRooted() || permarooted)) // process permarooted NPCs so they return to spawn heading
		{
			if (IsPet()) {
				// we're a pet, do as we're told
				switch (pStandingPetOrder) {
				case SPO_Follow: {

					Mob* owner = GetOwner();
					if (owner == nullptr) {
						break;
					}

					float dist = DistanceSquared(m_Position, owner->GetPosition());
					if (dist >= 400.0f) { // >=20
						if (AImovement_timer->Check()) {
							if (m_Navigation.x != owner->GetX() || m_Navigation.y != owner->GetY()) {
								m_Navigation = owner->GetPosition();
								glm::vec3 dest(owner->GetX(), owner->GetY(), owner->GetZ());
								if (owner->IsClient()) {
									bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(dest) || zone->IsWaterZone(dest.z);
									if (!in_liquid) {
										// we are not in water
										if (owner->FindType(SE_Levitate) || owner->GetFlyMode() != 0) {
											// we are flying - use ground
											float new_z = zone->zonemap->FindBestZ(dest, nullptr);
											if (new_z != BEST_Z_INVALID) {
												m_Navigation.z = SetBestZ(new_z);
											}
										}
										else {
											float new_z = zone->zonemap->FindBestZ(dest, nullptr);
											if (new_z != BEST_Z_INVALID)
												m_Navigation.z = SetBestZ(new_z);
										}
									}
									else {
										float new_z = zone->zonemap->FindBestZ(dest, nullptr);
										if (new_z != BEST_Z_INVALID)
											m_Navigation.z = SetBestZ(new_z);
									}
								}
								else {
									float new_z = zone->zonemap->FindBestZ(dest, nullptr, 20.0f, owner->GetZOffset());
									if (new_z == BEST_Z_INVALID)
										new_z = zone->zonemap->FindBestZ(dest, nullptr);
									if (new_z != BEST_Z_INVALID)
										m_Navigation.z = SetBestZ(new_z);
								}
							}
							if (dist >= 5625.0f) {
								RunTo(owner->GetX(), owner->GetY(), m_Navigation.z);
							}
							else {
								if (GetWalkspeed() < 0.1f) {
									RunTo(owner->GetX(), owner->GetY(), m_Navigation.z);
								}
								else {
									WalkTo(owner->GetX(), owner->GetY(), m_Navigation.z);
								}
							}
						}
					}
					else {
						StopNavigation();
					}

					break;
				}
				case SPO_Sit: {
					SetAppearance(eaSitting, false);
					break;
				}
				case SPO_Guard: {
					//only NPCs can guard stuff. (forced by where the guard movement code is in the AI)
					if (IsNPC()) {
						CastToNPC()->NextGuardPosition();
					}
					break;
				}
				}
			}
			else if (GetFollowID() && !IsZomm())
			{
				Mob* follow = entity_list.GetMob(GetFollowID());
				if (!follow) SetFollowID(0);
				else {

					float distance = DistanceSquared(m_Position, follow->GetPosition());
					int   follow_distance = GetFollowDistance();

					/**
					 * Default follow distance is 100
					 */
					if (distance >= follow_distance) {
						bool running = false;
						// maybe we want the NPC to only walk doing follow logic
						if (distance >= follow_distance + 150) {
							running = true;
						}

						if (m_Navigation.x != follow->GetX() || m_Navigation.y != follow->GetY()) {
							m_Navigation = follow->GetPosition();
							m_Navigation.z = GetFixedZ(m_Navigation);
						}

						if (running) {
							RunTo(m_Navigation.x, m_Navigation.y, m_Navigation.z);
						}
						else {
							// prevent snared mobs from stopping if they are following
							if (GetWalkspeed() < 0.1f)
								RunTo(m_Navigation.x, m_Navigation.y, m_Navigation.z);
							else
								WalkTo(m_Navigation.x, m_Navigation.y, m_Navigation.z);
						}
					}
					else {
						moved = false;
						StopNavigation();
					}
				}
			}
			else //not a pet, and not following somebody...
			{
				if (!AIloiter_timer->Enabled() || AIloiter_timer->Check(false))
				{
					if (this->IsClient())
					{
						// clients use loiter timer as a LD timer; drop client out of world
						if (this->CastToClient()->IsLD())
							this->CastToClient()->OnDisconnect(true);
						return;
					}

					if (AIloiter_timer->Enabled())
						AIloiter_timer->Stop();

					if(IsNPC())
						CastToNPC()->AI_DoMovement();
				}
			}
		} // else if (ai_think)
	} // if (!target)

	//Do Ranged attack here
	if(doranged && GetTarget() && !GetTarget()->HasDied())
	{
		RangedAttack(GetTarget());
	}
}

void NPC::AI_DoMovement() {
	if (IsZomm())
		return;

	if (GetBaseRace() == CONTROLLED_BOAT)
	{
		uint8 passengers = entity_list.GetClientCountByBoatID(GetID());
		if (passengers > 0)
			return;
	}

	if (AIwalking_timer->Enabled() && !AIwalking_timer->Check(false)) {
		if (IsMoving())
			StopNavigation();
		return;
	}

	float move_speed;

	if (hate_list.IsIgnoringAllHaters() && !hate_list.HasFeignedHaters())	// NPCs with a hate list and with all haters out of range should run
		move_speed = GetRunspeed();
	else
		move_speed = GetMovespeed();

	bool atguardpoint = IsGuarding() && m_Position.x == m_GuardPoint.x && m_Position.y == m_GuardPoint.y && m_Position.z == m_GuardPoint.z && !roambox_distance && !roamer;
	if (permarooted && m_Position.w == m_GuardPoint.w)
		atguardpoint = true;

	if (atguardpoint || permarooted) {
		move_speed = 0.0f;
	}

	if (reface_timer->Check())
	{
		moved = true;
		reface_timer->Disable();
	}

	if (AIhail_timer->Enabled()) {
		if (!AIhail_timer->Check()) {
			return;
		}
		else {
			AIhail_timer->Disable();
			moved = true;
			move_speed = 0.0f;
		}
	}

	SetRunAnimation(move_speed);

	if (move_speed < 0.1f) {
		// we are stopped for some reason.
		if (roambox_distance) {
			if (IsMoving())
				StopNavigation();
			if (AIloiter_timer->Paused()) {
				AIloiter_timer->Resume();
				StopNavigation();
				return;
			}
			if (AIheading_timer->Check())
				RotateToRunning(CalculateHeadingToTarget(roambox_movingto_x, roambox_movingto_y));
			return;
		}
		else if (roamer) {
			if (AIloiter_timer->Paused()) {
				AIloiter_timer->Resume();
				StopNavigation();
				return;
			}
			if (AIheading_timer->Check() && (m_CurrentWayPoint.x != GetX() || m_CurrentWayPoint.y != GetY())) {
				if (zone->pathing) {
					if (m_Navigation.x == GetX() && m_Navigation.y == GetY()) {
						if (m_Navigation == GetPosition()) {
							if (IsMoving() && m_Delta.w == 0.0f)
								StopNavigation();
							return;
						}
						PathfinderOptions opts;
						opts.smooth_path = true;
						opts.step_size = RuleR(Pathing, NavmeshStepSize);
						opts.offset = GetZOffset();
						opts.flags = PathingNotDisabled ^ PathingZoneLine;
						auto partial = false;
						auto stuck = false;
						auto route = zone->pathing->FindPath(
							glm::vec3(GetX(), GetY(), GetZ()),
							glm::vec3(m_CurrentWayPoint.x, m_CurrentWayPoint.y, m_CurrentWayPoint.z),
							partial,
							stuck,
							opts
						);
						if (route.size() > 0) {
							auto routeNode = route.begin();
							m_Navigation = GetPosition();
							m_Navigation.w = CalculateHeadingToTarget(routeNode->pos.x, routeNode->pos.y);
							if (GetHeading() != m_Navigation.w && m_Delta.w == 0.0f)
								RotateTo(m_Navigation.w);
							return;
						}
					}
					moved = true;
				}
			}
			return;
		}
		else if (!atguardpoint) {
			if (AIloiter_timer->Paused()) {
				AIloiter_timer->Resume();
				StopNavigation();
				return;
			}
			if (AIheading_timer->Check()) {
				glm::vec3 Goal(m_GuardPoint.x, m_GuardPoint.y, m_GuardPoint.z);

				PathfinderOptions opts;
				opts.smooth_path = true;
				opts.step_size = RuleR(Pathing, NavmeshStepSize);
				opts.offset = GetZOffset();
				opts.flags = PathingNotDisabled ^ PathingZoneLine;
				auto partial = false;
				auto stuck = false;
				auto route = zone->pathing->FindPath(
					glm::vec3(GetX(), GetY(), GetZ()),
					glm::vec3(m_GuardPoint.x, m_GuardPoint.y, m_GuardPoint.z),
					partial,
					stuck,
					opts
				);

				auto routeNode = route.begin();
				m_Navigation = GetPosition();
				if (permarooted)
					m_Navigation.w = m_GuardPoint.w;
				else if (route.size() > 0)
					m_Navigation.w = CalculateHeadingToTarget(routeNode->pos.x, routeNode->pos.y);
				else
					m_Navigation.w = CalculateHeadingToTarget(m_GuardPoint.x, m_GuardPoint.y);
				if (GetHeading() != m_Navigation.w && m_Delta.w == 0.0f)
					RotateTo(m_Navigation.w);
				return;

			}
			if (IsMoving() && m_Delta.w == 0.0f) {
				SetHeading(m_Navigation.w);
				StopNavigation();
			}
			return;
		}
	}
	else if (AIdoor_timer->Check() && GetSize() >= 20)
	{
		// Larger NPCs require more help opening doors due to much larger coord differences.
		entity_list.OpenDoorsNear(CastToNPC());
	}

	/**
	 * Roambox logic sets precedence
	 */
	if (roambox_distance) {
		if (AIloiter_timer->Paused()) {
			AIloiter_timer->Resume();
			StopNavigation();
			return;
		}
		if (GetX() == roambox_movingto_x && GetY() == roambox_movingto_y) {
			roambox_movingto_x = roambox_max_x + 1; // force update
			AIloiter_timer->Reset();
			AIloiter_timer->Start(RandomTimer(roambox_min_delay, roambox_delay));
			StopNavigation();
			return;
		}

		if (
			roambox_movingto_x > roambox_max_x
			|| roambox_movingto_x < roambox_min_x
			|| roambox_movingto_y > roambox_max_y
			|| roambox_movingto_y < roambox_min_y
			)
		{
			float x_diff_max = roambox_max_x - GetX();
			float x_diff_min = GetX() - roambox_min_x;
			float x_diff_random = zone->random.Real(x_diff_min + 1, x_diff_max - 1);
			float movedist = x_diff_random * x_diff_random;
			float movex = zone->random.Real(0, movedist);
			float movey = movedist - movex;
			movex = sqrtf(movex);
			movey = sqrtf(movey);
			movex *= zone->random.Int(0, 1) ? 1 : -1;
			movey *= zone->random.Int(0, 1) ? 1 : -1;
			roambox_movingto_x = GetX() + movex;
			roambox_movingto_y = GetY() + movey;
			//Try to calculate new coord using distance.
			if (roambox_movingto_x > roambox_max_x || roambox_movingto_x < roambox_min_x)
				roambox_movingto_x -= movex * 2;
			if (roambox_movingto_y > roambox_max_y || roambox_movingto_y < roambox_min_y)
				roambox_movingto_y -= movey * 2;
			//New coord is still invalid, ignore distance and just pick a new random coord.
			//If we're here we may have a roambox where one side is shorter than the specified distance. Commons, Wkarana, etc.
			if (roambox_movingto_x > roambox_max_x || roambox_movingto_x < roambox_min_x)
				roambox_movingto_x = zone->random.Real(roambox_min_x + 1.0f, roambox_max_x - 1.0f);
			if (roambox_movingto_y > roambox_max_y || roambox_movingto_y < roambox_min_y)
				roambox_movingto_y = zone->random.Real(roambox_min_y + 1.0f, roambox_max_y - 1.0f);

			roambox_movingto_z = GetZ();
			if (zone->HasMap()) {
				bool has_los = false;
				glm::vec3 start(roambox_movingto_x, roambox_movingto_y, roambox_ceil);
				glm::vec3 dest(roambox_movingto_x, roambox_movingto_y, GetZ());

				if (zone->HasMap() && zone->HasWaterMap() && zone->watermap->InLiquid(GetPosition()) || zone->IsWaterZone(GetZ())) {
					has_los = zone->zonemap->CheckLoS(GetPosition(), dest);
				}
				if (zone->random.Roll(5)) {
					// randomly return to spawn point
					// this helps mobs that fall below world and end up stuck in a water area
					roambox_movingto_x = m_SpawnPoint.x;
					roambox_movingto_y = m_SpawnPoint.y;
					roambox_movingto_z = m_SpawnPoint.z;
					has_los = false;
				}
				if (!has_los && zone->HasMap()) {
					float roam_z = zone->zonemap->FindGround(start, &dest);
					// self correcting ceiling cap - for mobs that spawn under objects
					float ceil = zone->zonemap->FindCeiling(start, &dest);
					if (ceil != BEST_Z_INVALID && ((ceil - 10.0f) > roambox_ceil)) {
						roambox_ceil = ceil - 1.0f;
					}

					if (roam_z == BEST_Z_INVALID)
						roambox_movingto_z = GetZ();
					else
						roambox_movingto_z = SetBestZ(roam_z);
				}
			}
			Log(Logs::Detail, Logs::AI, "Roam Box: (%.3f->%.3f,%.3f->%.3f): Go To (%.3f,%.3f)",
				roambox_min_x, roambox_max_x, roambox_min_y, roambox_max_y, roambox_movingto_x, roambox_movingto_y);
		}
		if (!IsMoving()) {
			NavigateTo(roambox_movingto_x, roambox_movingto_y, roambox_movingto_z);
		}
		return;
	}
	else if (roamer) {
		if (AIloiter_timer->Paused()) {
			AIloiter_timer->Resume();
			StopNavigation();
			return;
		}
		if (AIwalking_timer->Check())
		{
			pause_timer_complete = true;
			AIwalking_timer->Disable();
		}

		int32 gridno = CastToNPC()->GetGrid();

		if (gridno > 0 || cur_wp == EQ::WaypointStatus::QuestControlNoGrid) {
			if (pause_timer_complete == true) { // time to pause at wp is over

				AI_SetupNextWaypoint();
				AIwalking_timer->Disable();
				NavigateTo(
					m_CurrentWayPoint.x,
					m_CurrentWayPoint.y,
					m_CurrentWayPoint.z);
			}    // endif (pause_timer_complete==true)
			else if (!AIwalking_timer->Enabled()) {    // currently moving
				bool domove = true;
				if (m_CurrentWayPoint.x == GetX() && m_CurrentWayPoint.y == GetY()) {
					if (wandertype == GridRandomPath)
					{
						if (cur_wp == patrol)
						{
							// reached our randomly selected destination; force a pause
							if (cur_wp_pause == 0)
							{
								if (Waypoints.size() >= cur_wp && Waypoints[cur_wp].pause)
									cur_wp_pause = Waypoints[cur_wp].pause;
								else if (Waypoints.size() > 0 && Waypoints[0].pause)
									cur_wp_pause = Waypoints[0].pause;
								else
									cur_wp_pause = 38;
							}
							Log(Logs::Detail, Logs::AI, "NPC using wander type GridRandomPath on grid %d at waypoint %d has reached its random destination; pause time is %d", GetGrid(), cur_wp, cur_wp_pause);
						}
						else
							cur_wp_pause = 0; // skipping pauses until destination
					}
					if (cur_wp_pause != 0) {
						SetWaypointPause();
						if (GetAppearance() != eaStanding) {
							SetAppearance(eaStanding, false);
						}
						StopNavigation();
						if (m_CurrentWayPoint.w >= 0.0f) {
							// use running to snap to the new heading
							RotateToRunning(m_CurrentWayPoint.w);
						}
					}
					//kick off event_waypoint arrive
					std::string export_string = fmt::format(
						"{} {}", 
						cur_wp,
						gridno
						);
					parse->EventNPC(EVENT_WAYPOINT_ARRIVE, CastToNPC(), nullptr, export_string, 0);
					if (!AIwalking_timer->Enabled()) {
						AI_SetupNextWaypoint();
					}
					else {
						domove = false;
					}
				}
				if (domove) {
					NavigateTo(
						m_CurrentWayPoint.x,
						m_CurrentWayPoint.y,
						m_CurrentWayPoint.z);
				}
			}
		}        // endif (gridno > 0)
			// handle new quest grid command processing
		else if (gridno < 0) {    // this mob is under quest control
			if (pause_timer_complete == true) { // time to pause has ended
				SetGrid(0 - GetGrid()); // revert to AI control
				SetAppearance(eaStanding, false);
			}
		}
	}
	else if (IsGuarding()) {
		if (m_Position != m_GuardPoint) {
			float z_tolerance = 15.0f;
			if (respawn2 && respawn2->GetForceZ() && m_GuardPoint.z > m_Position.z)  // generally used for NPCs that are flying e.g. Bees in airplane
				z_tolerance = 200.0f;
			bool at_gp = IsPositionEqualWithinCertainZ(m_Position, m_GuardPoint, z_tolerance);
			if (at_gp) {
				SetRunAnimation(0.0f);
				if (GetTarget() == nullptr || DistanceSquared(m_Position, GetTarget()->GetPosition()) >= 5 * 5) {
					// add a check to how far off from heading
					// rotate or snap
					if (m_GuardPoint.x != m_Position.x || m_GuardPoint.y != m_Position.y || m_GuardPoint.z != m_Position.z) {
						m_Position.x = m_GuardPoint.x;
						m_Position.y = m_GuardPoint.y;
						m_Position.z = m_GuardPoint.z;
					}
					if (m_GuardPoint.w != m_Position.w && m_Delta.w == 0.0f) {
						RotateTo(m_GuardPoint.w, true);
					}
					return;
				}
				else {
					if (AIheading_timer->Check())
						FaceTarget(GetTarget());
				}
			}
			else {
				if (!IsMoving()) {
					NavigateTo(m_GuardPoint.x, m_GuardPoint.y, m_GuardPoint.z);
				}
			}
		}
	}
}

void NPC::AI_SetupNextWaypoint() {
	int32 spawn_id = this->GetSpawnPointID();
	LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
	iterator.Reset();
	Spawn2 *found_spawn = nullptr;

	while(iterator.MoreElements())
	{
		Spawn2* cur = iterator.GetData();
		iterator.Advance();
		if(cur->GetID() == spawn_id)
		{
			found_spawn = cur;
			break;
		}
	}

	if (wandertype == GridOneWayRepop && cur_wp == CastToNPC()->GetMaxWp()) {
		CastToNPC()->Depop(true); //depop and resart spawn timer
		if(found_spawn)
			found_spawn->SetNPCPointer(nullptr);
	}
	else if (wandertype == GridOneWayDepop && cur_wp == CastToNPC()->GetMaxWp()) {
		CastToNPC()->Depop(false);//depop without spawn timer
		if(found_spawn)
			found_spawn->SetNPCPointer(nullptr);
	}
	else {
		pause_timer_complete = false;

		Log(Logs::Detail, Logs::Pathing, "We are departing waypoint %d.", cur_wp);

		//if we were under quest control (with no grid), we are done now..
		if(cur_wp == EQ::WaypointStatus::QuestControlNoGrid) {
			Log(Logs::Detail, Logs::Pathing, "Non-grid quest mob has reached its quest ordered waypoint. Leaving pathing mode.");
			roamer = false;
			cur_wp = 0;
		}
		if (cur_wp == EQ::WaypointStatus::QuestControlGrid) {
			CalculateNewWaypoint();
		}

		if(GetAppearance() != eaStanding)
			SetAppearance(eaStanding, false);

		entity_list.OpenDoorsNear(CastToNPC());

		//kick off event_waypoint depart
		std::string export_string = fmt::format(
			"{} {}",
			cur_wp,
			GetGrid()
		);
		parse->EventNPC(EVENT_WAYPOINT_DEPART, CastToNPC(), nullptr, export_string, 0);

		//setup our next waypoint, if we are still on our normal grid
		//remember that the quest event above could have done anything it wanted with our grid
		if(GetGrid() > 0) {
			CastToNPC()->CalculateNewWaypoint();
		}
	}
}

void Mob::AI_Event_Engaged(Mob* attacker)
{
	if (!IsAIControlled()) {
		return;
	}

	if(GetAppearance() != eaStanding) {
		SetAppearance(eaStanding);
	}

	if (IsNPC() && !IsPet() && !CastToNPC()->IsAssisting()) {
		if (!RuleB(AlKabor, AllowTickSplit) || GetSpecialAbility(ALWAYS_CALL_HELP)) {
			CastToNPC()->CallForHelp(attacker, true);
		}
	}

	if (IsNPC()) {
		if (AIhail_timer->Enabled()) {
			AIhail_timer->Disable();
		}

		if (CastToNPC()->GetGrid() > 0) {
			if (AIwalking_timer->Enabled()) {
				if (AIwalking_timer->Check(false)) {
					AIwalking_timer->Disable();
				}
				else if (!GetSpecialAbility(NO_LOITERING)) {
					AIwalking_timer->Pause();
				}
			}
		}

		CastToNPC()->TriggerAutoCastTimer();

		if(attacker && !attacker->IsCorpse()) {
			//Because sometimes the AIYellForHelp triggers another engaged and then immediately a not engaged
			//if the target dies before it goes off
			if(attacker->GetHP() > 0) {
				if(!CastToNPC()->GetCombatEvent() && GetHP() > 0) {
					parse->EventNPC(EVENT_COMBAT, CastToNPC(), attacker, "1", 0);
					uint32 emoteid = GetEmoteID();
					if (emoteid != 0 && !IsPet()) {
						CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::EnterCombat, emoteid, attacker);
					}

					CastToNPC()->SetCombatEvent(true);
				}
			}
		}
	}
}

void Mob::AI_SetLoiterTimer()
{
	uint32 min_time = static_cast<uint32>(RuleI(NPC, LastFightingDelayMovingMin));
	uint32 max_time = static_cast<uint32>(RuleI(NPC, LastFightingDelayMovingMax));
	uint32 time = 0;

	if (IsNPC())
	{
		if (GetSpecialAbility(CORPSE_CAMPER))
		{
			max_time = GetSpecialAbility(CORPSE_CAMPER);
			min_time = GetSpecialAbilityParam(CORPSE_CAMPER, 0);

			if (max_time < min_time)
				max_time = min_time;

			if (max_time == 1)
			{
				// a max time parameter of 1 is indicates infinite
				min_time = max_time = 1209600000u;		// two weeks long
			}
			else
			{
				max_time *= 1000;
				min_time *= 1000;
			}
		}
		else if (GetSpecialAbility(NO_LOITERING))
		{
			min_time = max_time = 0;
		}
	}
	else if (IsClient())
	{
		min_time = max_time = CLIENT_LD_TIMEOUT;
	}

	if (min_time == max_time)
		time = min_time;
	else
		time = static_cast<uint32>(zone->random.Int(min_time, max_time));

	AIloiter_timer->Start(time);
	AIloiter_timer->Pause();
}

// Note: Hate list may not be empty when this is called
void Mob::AI_Event_NoLongerEngaged() {
	if (!IsAIControlled()) {
		return;
	}

	if (AIloiter_timer->Paused()) {
		AIloiter_timer->Resume();
	}
	if (!AIloiter_timer->Enabled() || AIloiter_timer->GetRemainingTime() < 1000) {
		AIloiter_timer->Start(1000);
	}

	if (AIwalking_timer->Paused()) {
		AIwalking_timer->Resume();
	}

	// So mobs don't keep running as a ghost until AIwalking_timer fires
	// if they were moving prior to losing all hate
	if (IsMoving() && (!IsFearedNoFlee() || !curfp)) {
		StopNavigation();
	}
	if (GetCurrentSpeed() < 0.1f || IsMezzed() || IsStunned()) {
		StopNavigation();
	}

	ClearRampage();

	if(IsNPC()) {
		CastToNPC()->SetAssisting(false);

		if(CastToNPC()->GetCombatEvent() && GetHP() > 0) {
			if(entity_list.GetNPCByID(this->GetID())) {
				uint32 emoteid = CastToNPC()->GetEmoteID();
				parse->EventNPC(EVENT_COMBAT, CastToNPC(), nullptr, "0", 0);
				if (emoteid != 0) {
					CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::LeaveCombat, emoteid);
				}
				CastToNPC()->SetCombatEvent(false);
			}
		}

		if (zone && zone->GetGuildID() != GUILD_NONE)
		{
			if (GetSpecialAbility(TETHER)) {

				auto npcSpawnPoint = CastToNPC()->GetSpawnPoint();
				GMMove(npcSpawnPoint.x, npcSpawnPoint.y, npcSpawnPoint.z, npcSpawnPoint.w);
			}

			else if (GetSpecialAbility(LEASH)) {
				auto npcSpawnPoint = CastToNPC()->GetSpawnPoint();
				GMMove(npcSpawnPoint.x, npcSpawnPoint.y, npcSpawnPoint.z, npcSpawnPoint.w);
				SetHP(GetMaxHP());
				BuffFadeAll();
				WipeHateList(true);
				AIloiter_timer->Trigger();
				return;
			}
		}
	}
}

//this gets called from InterruptSpell() for failure or SpellFinished() for success
void NPC::AI_Event_SpellCastFinished(bool iCastSucceeded, uint16 slot)
{
	if (slot == 1) {
		uint32 recovery_time = 0;
		if (iCastSucceeded) {
			if (casting_spell_AIindex < AIspells.size()) {
				int32 recast_delay = AIspells[casting_spell_AIindex].recast_delay;
				int32 cast_variance = zone->random.Int(0, 4) * 1000;
        
				if (AIspells[casting_spell_AIindex].spellid == SPELL_CAZIC_TOUCH) {
					cast_variance = 0;
				}

				recovery_time += spells[AIspells[casting_spell_AIindex].spellid].recovery_time;

				if (recast_delay > 0) {
					if (recast_delay < 10000) {
						AIspells[casting_spell_AIindex].time_cancast = Timer::GetCurrentTime() + (recast_delay * 1000) + cast_variance;
					}
				}
				else if (recast_delay == -1) {
					// editor default; add variance
					AIspells[casting_spell_AIindex].time_cancast = Timer::GetCurrentTime() + spells[AIspells[casting_spell_AIindex].spellid].recast_time + cast_variance;
				}
				else if (recast_delay == -2) {
					AIspells[casting_spell_AIindex].time_cancast = Timer::GetCurrentTime();
				}
				else {
					// 0; no variance
					AIspells[casting_spell_AIindex].time_cancast = Timer::GetCurrentTime() + spells[AIspells[casting_spell_AIindex].spellid].recast_time;
				}
			}
			if (recovery_time < AIautocastspell_timer->GetDuration()) {
				recovery_time = AIautocastspell_timer->GetDuration();
			}
			AIautocastspell_timer->Start(recovery_time, false);
		} else {
			AIautocastspell_timer->Start(AISpellVar.fail_recast, false);
		}
		casting_spell_AIindex = AIspells.size();
	}
}


bool NPC::AI_EngagedCastCheck() {
	if (AIautocastspell_timer->Check(false)) {
		AIautocastspell_timer->Disable();	//prevent the timer from going off AGAIN while we are casting.

		Log(Logs::Detail, Logs::AI, "Engaged autocast check triggered. Trying to cast healing spells then maybe offensive spells.");

		// prioritize raid boss spells (spells with priority == 0) first, with no detrimental roll
		if (hasZeroPrioritySpells)
		{
			if (AICastSpell(GetTarget(), 100, SpellType_Nuke | SpellType_Lifetap | SpellType_DOT | SpellType_Dispel | SpellType_Mez | SpellType_Slow | SpellType_Debuff | SpellType_Charm | SpellType_Root | SpellType_Snare, true))
				return(true);
		}

		// try casting a heal or gate
		if (!AICastSpell(this, AISpellVar.engaged_beneficial_self_chance, SpellType_Heal | SpellType_Escape | SpellType_InCombatBuff)) {
			// try casting a heal on nearby
			if (!entity_list.AICheckCloseBeneficialSpells(this, AISpellVar.engaged_beneficial_other_chance, MobAISpellRange, SpellType_Heal)) {
				//nobody to heal, try some detrimental spells.
				if(!AICastSpell(GetTarget(), AISpellVar.engaged_detrimental_chance, SpellType_Nuke | SpellType_Lifetap | SpellType_DOT | SpellType_Dispel | SpellType_Mez | SpellType_Slow | SpellType_Debuff | SpellType_Charm | SpellType_Root | SpellType_Snare)) {
					//no spell to cast, try again soon.
					AIautocastspell_timer->Start(RandomTimer(AISpellVar.engaged_no_sp_recast_min, AISpellVar.engaged_no_sp_recast_max), false);
				}
			}
		}
		return(true);
	}

	return(false);
}

bool NPC::AI_IdleCastCheck() {
	if (AIautocastspell_timer->Check(false)) {
#if MobAI_DEBUG_Spells >= 25
		std::cout << "Non-Engaged autocast check triggered: " << this->GetName() << std::endl;
#endif
		AIautocastspell_timer->Disable();	//prevent the timer from going off AGAIN while we are casting.
		if (IsUnTargetable())
		{
			AIautocastspell_timer->Start(AISpellVar.idle_no_sp_recast_max, false);
			return false;
		}
		if (!AICastSpell(this, AISpellVar.idle_beneficial_chance, SpellType_Heal | SpellType_Buff | SpellType_Pet)) {
			if(!entity_list.AICheckCloseBeneficialSpells(this, 40, MobAISpellRange, SpellType_Heal | SpellType_Buff)) {
				//if we didnt cast any spells, our autocast timer just resets to the
				//last duration it was set to... try to put up a more reasonable timer...
				AIautocastspell_timer->Start(RandomTimer(AISpellVar.idle_no_sp_recast_min, AISpellVar.idle_no_sp_recast_max), false);
			}	//else, spell casting finishing will reset the timer.
		}	//else, spell casting finishing will reset the timer.
		return(true);
	}
	return(false);
}

void Mob::CheckEnrage()
{
	if (!bEnraged && GetSpecialAbility(SPECATK_ENRAGE)) {
		// this is so we don't have to make duplicate NPC types
		if (IsNPC() && GetLevel() < 56 && GetLevel() > 52) {
			return;
		}

		int hp_ratio = GetSpecialAbilityParam(SPECATK_ENRAGE, 0);
		hp_ratio = hp_ratio > 0 ? hp_ratio : RuleI(NPC, StartEnrageValue);
		if (GetHPRatio() <= static_cast<float>(hp_ratio)) {
			StartEnrage();
		}
	}

	return;
}

void Mob::StartEnrage()
{
	// dont continue if already enraged
	if (bEnraged)
		return;

	if(!GetSpecialAbility(SPECATK_ENRAGE))
		return;

	// Do not enrage if we are fleeing or feared, unless we are also rooted.
	if (IsFeared() && !IsRooted())
		return;

	int hp_ratio = GetSpecialAbilityParam(SPECATK_ENRAGE, 0);
	hp_ratio = hp_ratio > 0 ? hp_ratio : RuleI(NPC, StartEnrageValue);
	if(GetHPRatio() > static_cast<float>(hp_ratio)) {
		return;
	}

	if(RuleB(NPC, LiveLikeEnrage) && !((IsPet() && !IsCharmedPet() && GetOwner() && GetOwner()->IsClient()) ||
		(CastToNPC()->GetSwarmOwner() && entity_list.GetMob(CastToNPC()->GetSwarmOwner())->IsClient()))) {
		return;
	}

	Timer *timer = GetSpecialAbilityTimer(SPECATK_ENRAGE);
	if (timer && !timer->Check())
		return;

	int enraged_duration = GetSpecialAbilityParam(SPECATK_ENRAGE, 1);
	enraged_duration = enraged_duration > 0 ? enraged_duration : EnragedDurationTimer;
	StartSpecialAbilityTimer(SPECATK_ENRAGE, enraged_duration);

	// start the timer. need to call IsEnraged frequently since we dont have callback timers :-/
	bEnraged = true;
	entity_list.MessageClose_StringID(this, true, 200, Chat::NPCEnrage, NPC_ENRAGE_START, GetCleanName());
}

void Mob::ProcessEnrage(){
	if(IsEnraged()){
		Timer *timer = GetSpecialAbilityTimer(SPECATK_ENRAGE);
		if(timer && timer->Check()){
			entity_list.MessageClose_StringID(this, true, 200, Chat::NPCEnrage, NPC_ENRAGE_END, GetCleanName());

			int enraged_cooldown = GetSpecialAbilityParam(SPECATK_ENRAGE, 2);
			enraged_cooldown = enraged_cooldown > 0 ? enraged_cooldown : EnragedTimer;
			StartSpecialAbilityTimer(SPECATK_ENRAGE, enraged_cooldown);
			bEnraged = false;
		}
	}
}

bool Mob::IsEnraged()
{
	return bEnraged;
}

bool Mob::Flurry()
{
	if (IsCharmedPet())
		return false;

	Mob *target = GetTarget();
	if (target)
	{
		if (!IsPet())
		{
			entity_list.MessageClose_StringID(this, true, 200, Chat::NPCFlurry, NPC_FLURRY, GetCleanName(), target->GetCleanName());
		} else
		{
			entity_list.MessageClose_StringID(this, true, 200, Chat::PetFlurry, NPC_FLURRY, GetCleanName(), target->GetCleanName());
		}

		if (IsNPC())
		{
			// flurries always double class attack the target
			CastToNPC()->TriggerClassAtkTimer();
			CastToNPC()->DoClassAttacks(target);
			CastToNPC()->TriggerClassAtkTimer();
		}

		// NPC flurries are merely an extra attack round
		DoMainHandRound(target);
		if (IsDualWielding())
			DoOffHandRound(target);
	}
	return true;
}

bool Mob::AddRampage(Mob *mob)
{
	if (!mob)
		return false;

	if (!GetSpecialAbility(SPECATK_RAMPAGE))
		return false;

	int firsthole = -1;
	for (int i = 0; i < RampageArray.size(); i++) {
		// in case entity isn't removed from list when it should be
		if (!entity_list.GetMob(RampageArray[i]))
		{
			RampageArray[i] = 0;
		}

		// look for a 'hole' to fill instead of adding to bottom of list
		if (firsthole == -1 && RampageArray[i] == 0)
		{
			firsthole = i;
		}

		// if Entity ID is already on the list don't add it again
		if (mob->GetID() == RampageArray[i])
			return false;
	}

	if (firsthole > -1)
		RampageArray[firsthole] = mob->GetID();
	else
		RampageArray.push_back(mob->GetID());

	return true;
}

void Mob::ClearRampage()
{
	RampageArray.clear();
}

// if force is false, it will not remove feigned clients
void Mob::RemoveFromRampageList(Mob* mob, bool force)
{
	if (!mob)
		return;

	if (IsNPC() && GetSpecialAbility(SPECATK_RAMPAGE)
		&& (force || mob->IsNPC() || (mob->IsClient() && !mob->CastToClient()->IsFeigned())))
	{
		for (int i = 0; i < RampageArray.size(); i++)
		{
			if (mob->GetID() == RampageArray[i])
			{
				RampageArray[i] = 0;
			}
		}
	}
}

bool Mob::Rampage(int range, int damagePct)
{
	if (IsCharmedPet())
		return false;

	if (!IsPet())
		entity_list.MessageClose_StringID(this, true, 200, Chat::NPCRampage, NPC_RAMPAGE, GetCleanName());
	else
		entity_list.MessageClose_StringID(this, true, 200, Chat::PetFlurry, NPC_RAMPAGE, GetCleanName());

	for (int i = 0; i < RampageArray.size(); i++)
	{
		if (!RampageArray[i])
			continue;

		Mob *m_target = entity_list.GetMob(RampageArray[i]);
		if (m_target)
		{
			if (m_target == GetTarget())
				continue;
			if (m_target->IsCorpse())
			{
				Log(Logs::General, Logs::Error, "%s is on %s's rampage list", m_target->GetCleanName(), GetCleanName());
				RemoveFromRampageList(m_target, true);
				continue;
			}
			if (m_target->IsClient() && m_target->CastToClient()->IsFeigned())
				continue;

			if (DistanceSquared(m_Position, m_target->GetPosition()) > (range * range))
				continue;

			DoMainHandRound(m_target, damagePct);
			if (IsDualWielding())
				DoOffHandRound(m_target, damagePct);

			if (IsNPC() && zone->random.Roll(50))
			{
				// On live, rampages will either always class attack from that specific spawn or never
				// unknown why some spawns do that and some don't
				// making this a 50% chance for now as some sort of middle-ground
				CastToNPC()->TriggerClassAtkTimer();
				CastToNPC()->DoClassAttacks(m_target);
			}
			break;
		}
	}

	// rampages always kick/bash/backstab the target
	if (IsNPC())
		CastToNPC()->TriggerClassAtkTimer();

	return true;
}

void Mob::AreaRampage(int numTargets, int damagePct)
{
	if (IsCharmedPet())
		return;

	if (numTargets <= 0)
		numTargets = 999;

	int index_hit = 0;
	if (!IsPet())	// do not know every pet AA so thought it safer to add this
	{
		// older clients did not have 'wild rampage' string
		entity_list.MessageClose_StringID(this, true, 200, Chat::NPCRampage, NPC_RAMPAGE, GetCleanName());
		//entity_list.MessageClose_StringID(this, true, 200, Chat::NPCRampage, AE_RAMPAGE, GetCleanName());
	}
	else
	{
		entity_list.MessageClose_StringID(this, true, 200, Chat::PetFlurry, NPC_RAMPAGE, GetCleanName());
		//entity_list.MessageClose_StringID(this, true, 200, Chat::PetFlurry, AE_RAMPAGE, GetCleanName());
	}

	index_hit = hate_list.AreaRampage(this, GetTarget(), numTargets, damagePct);

	if (IsNPC())
		CastToNPC()->TriggerClassAtkTimer();
}

uint32 Mob::GetLevelCon(uint8 mylevel, uint8 iOtherLevel) {
	int16 diff = iOtherLevel - mylevel;
	uint32 conlevel=0;

    if (diff == 0)
        return CON_WHITE;
    else if (diff >= 1 && diff <= 2)
        return CON_YELLOW;
    else if (diff >= 3)
        return CON_RED;

    if (mylevel <= 7)
    {
        if (diff <= -4)
            conlevel = CON_GREEN;
        else
            conlevel = CON_BLUE;
    }
	else if (mylevel <= 8)
	{
        if (diff <= -5)
            conlevel = CON_GREEN;
        else if (diff <= -4)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
    else if (mylevel <= 12)
	{
        if (diff <= -6)
            conlevel = CON_GREEN;
        else if (diff <= -4)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
    else if (mylevel <= 16)
	{
        if (diff <= -7)
            conlevel = CON_GREEN;
        else if (diff <= -5)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 20)
	{
        if (diff <= -8)
            conlevel = CON_GREEN;
        else if (diff <= -6)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 24)
	{
        if (diff <= -9)
            conlevel = CON_GREEN;
        else if (diff <= -7)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 28)
	{
        if (diff <= -10)
            conlevel = CON_GREEN;
        else if (diff <= -8)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 30)
	{
        if (diff <= -11)
            conlevel = CON_GREEN;
        else if (diff <= -9)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 32)
	{
        if (diff <= -12)
            conlevel = CON_GREEN;
        else if (diff <= -9)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 36)
	{
        if (diff <= -13)
            conlevel = CON_GREEN;
        else if (diff <= -10)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 40)
	{
        if (diff <= -14)
            conlevel = CON_GREEN;
        else if (diff <= -11)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 44)
	{
        if (diff <= -16)
            conlevel = CON_GREEN;
        else if (diff <= -12)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 48)
	{
        if (diff <= -17)
            conlevel = CON_GREEN;
        else if (diff <= -13)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 52)
	{
        if (diff <= -18)

            conlevel = CON_GREEN;
        else if (diff <= -14)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 54)
	{
        if (diff <= -19)

            conlevel = CON_GREEN;
        else if (diff <= -15)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 56)
	{
        if (diff <= -20)

            conlevel = CON_GREEN;
        else if (diff <= -15)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 60)
	{
        if (diff <= -21)
            conlevel = CON_GREEN;
        else if (diff <= -16)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
	}
	else if (mylevel <= 61)
    {
        if (diff <= -19)
            conlevel = CON_GREEN;
        else if (diff <= -14)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
    }
	else if (mylevel <= 62)
    {
        if (diff <= -17)
            conlevel = CON_GREEN;
        else if (diff <= -12)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
    }
	else
    {
        if (diff <= -16)
            conlevel = CON_GREEN;
        else if (diff <= -11)
            conlevel = CON_LIGHTBLUE;
        else
            conlevel = CON_BLUE;
    }

	return conlevel;
}

void NPC::CheckSignal()
{
	if (!signal_q.empty()) {
		int signal_id = signal_q.front();
		signal_q.pop_front();
		std::string signal_data = signal_strq.front();
		signal_strq.pop_front();
		std::string export_string = fmt::format(
			"{}",
			signal_id
		);
		if (!signal_data.empty()) {
			std::vector<std::any> info_ptrs;
			info_ptrs.push_back(&signal_data);
			parse->EventNPC(EVENT_SIGNAL, this, nullptr, export_string, 0, &info_ptrs);
		}
		else {
			parse->EventNPC(EVENT_SIGNAL, this, nullptr, export_string, 0);
		}
	}
}

bool IsSpellInList(DBnpcspells_Struct* spell_list, int16 iSpellID);
bool IsSpellEffectInList(DBnpcspellseffects_Struct* spelleffect_list, uint16 iSpellEffectID, int32 base, int32 limit, int32 max);

bool NPC::AI_AddNPCSpells(uint32 iDBSpellsID) {
	// ok, this function should load the list, and the parent list then shove them into the struct and sort
	npc_spells_id = iDBSpellsID;
	AIspells.clear();
	if (npc_spells_id == 0) {
		AIautocastspell_timer->Disable();
		return false;
	}
	DBnpcspells_Struct* spell_list = database.GetNPCSpells(npc_spells_id);
	if (!spell_list) {
		AIautocastspell_timer->Disable();
		return false;
	}
	DBnpcspells_Struct* parentlist = database.GetNPCSpells(spell_list->parent_list);
	uint32 i;
	std::string debug_msg = StringFormat("Loading NPCSpells onto %s: dbspellsid=%u, level=%u", GetName(), npc_spells_id, GetLevel());
	if (spell_list) {
		debug_msg.append(StringFormat(" (found, %u), parentlist=%u", spell_list->entries.size(), spell_list->parent_list));
		if (spell_list->parent_list) {
			if (parentlist) {
				debug_msg.append(StringFormat(" (found, %u)", parentlist->entries.size()));
			}
			else {
				debug_msg.append(" (not found)");
			}
		}
	}
	else {
		debug_msg.append(" (not found)");
	}
	LogAI("[{}]", debug_msg.c_str());

	if (parentlist) {
		for (const auto &iter : parentlist->entries) {
			LogAIDetail("([{}]) [{}]", iter.spellid, spells[iter.spellid].name);
		}
	}
	LogAI("fin (parent list)");
	if (spell_list) {
		for (const auto &iter : spell_list->entries) {
			LogAIDetail("([{}]) [{}]", iter.spellid, spells[iter.spellid].name);
		}
	}
	LogAI("fin (spell list)");

	uint16 attack_proc_spell = -1;
	int8 proc_chance = 3;
	uint16 range_proc_spell = -1;
	int16 rproc_chance = 0;
	uint16 defensive_proc_spell = -1;
	int16 dproc_chance = 0;
	uint32 _fail_recast = 0;
	uint32 _engaged_no_sp_recast_min = 0;
	uint32 _engaged_no_sp_recast_max = 0;
	uint8 _engaged_beneficial_self_chance = 0;
	uint8 _engaged_beneficial_other_chance = 0;
	uint8 _engaged_detrimental_chance = 0;
	uint32 _idle_no_sp_recast_min = 0;
	uint32 _idle_no_sp_recast_max = 0;
	uint8 _idle_beneficial_chance = 0;

	if (parentlist) {
		attack_proc_spell = parentlist->attack_proc;
		proc_chance = parentlist->proc_chance;
		range_proc_spell = parentlist->range_proc;
		rproc_chance = parentlist->rproc_chance;
		defensive_proc_spell = parentlist->defensive_proc;
		dproc_chance = parentlist->dproc_chance;
		_fail_recast = parentlist->fail_recast;
		_engaged_no_sp_recast_min = parentlist->engaged_no_sp_recast_min;
		_engaged_no_sp_recast_max = parentlist->engaged_no_sp_recast_max;
		_engaged_beneficial_self_chance = parentlist->engaged_beneficial_self_chance;
		_engaged_beneficial_other_chance = parentlist->engaged_beneficial_other_chance;
		_engaged_detrimental_chance = parentlist->engaged_detrimental_chance;
		_idle_no_sp_recast_min = parentlist->idle_no_sp_recast_min;
		_idle_no_sp_recast_max = parentlist->idle_no_sp_recast_max;
		_idle_beneficial_chance = parentlist->idle_beneficial_chance;
		for (auto &e : parentlist->entries) {
			if (GetLevel() >= e.minlevel && GetLevel() <= e.maxlevel && e.spellid > 0) {
				if (!IsSpellInList(spell_list, e.spellid)) {
					AddSpellToNPCList(e.priority, e.spellid, e.type, e.manacost, e.recast_delay, e.resist_adjust);
				}
			}
		}
	}

	if (spell_list->attack_proc >= 0)
	{
		attack_proc_spell = spell_list->attack_proc;
		proc_chance = spell_list->proc_chance;
	}

	if (spell_list->range_proc >= 0) {
		range_proc_spell = spell_list->range_proc;
		rproc_chance = spell_list->rproc_chance;
	}

	//If any casting variables are defined in the current list, ignore those in the parent list.
	if (spell_list->fail_recast || spell_list->engaged_no_sp_recast_min || spell_list->engaged_no_sp_recast_max
		|| spell_list->engaged_beneficial_self_chance || spell_list->engaged_beneficial_other_chance || spell_list->engaged_detrimental_chance
		|| spell_list->idle_no_sp_recast_min || spell_list->idle_no_sp_recast_max || spell_list->idle_beneficial_chance) {
		_fail_recast = spell_list->fail_recast;
		_engaged_no_sp_recast_min = spell_list->engaged_no_sp_recast_min;
		_engaged_no_sp_recast_max = spell_list->engaged_no_sp_recast_max;
		_engaged_beneficial_self_chance = spell_list->engaged_beneficial_self_chance;
		_engaged_beneficial_other_chance = spell_list->engaged_beneficial_other_chance;
		_engaged_detrimental_chance = spell_list->engaged_detrimental_chance;
		_idle_no_sp_recast_min = spell_list->idle_no_sp_recast_min;
		_idle_no_sp_recast_max = spell_list->idle_no_sp_recast_max;
		_idle_beneficial_chance = spell_list->idle_beneficial_chance;
	}

	for (auto &e : spell_list->entries) {
		if (GetLevel() >= e.minlevel && GetLevel() <= e.maxlevel && e.spellid > 0) {
			AddSpellToNPCList(e.priority, e.spellid, e.type, e.manacost, e.recast_delay, e.resist_adjust);
		}
	}

	std::sort(AIspells.begin(), AIspells.end(), [](const AISpells_Struct& a, const AISpells_Struct& b) {
		return a.priority > b.priority;
	});

	if (IsValidSpell(attack_proc_spell)) {
		innateProcSpellId = attack_proc_spell;
		innateProcChance = proc_chance;
	}

	//Set AI casting variables

	AISpellVar.fail_recast = (_fail_recast) ? _fail_recast : RuleI(Spells, AI_SpellCastFinishedFailRecast);
	AISpellVar.engaged_no_sp_recast_min = (_engaged_no_sp_recast_min) ? _engaged_no_sp_recast_min : RuleI(Spells, AI_EngagedNoSpellMinRecast);
	AISpellVar.engaged_no_sp_recast_max = (_engaged_no_sp_recast_max) ? _engaged_no_sp_recast_max : RuleI(Spells, AI_EngagedNoSpellMaxRecast);
	AISpellVar.engaged_beneficial_self_chance = (_engaged_beneficial_self_chance) ? _engaged_beneficial_self_chance : RuleI(Spells, AI_EngagedBeneficialSelfChance);
	AISpellVar.engaged_beneficial_other_chance = (_engaged_beneficial_other_chance) ? _engaged_beneficial_other_chance : RuleI(Spells, AI_EngagedBeneficialOtherChance);
	if (GetArchetype() == ARCHETYPE_HYBRID)
		AISpellVar.engaged_detrimental_chance = (_engaged_detrimental_chance) ? _engaged_detrimental_chance : RuleI(Spells, AI_EngagedDetrimentalChanceHybrid);
	else if (GetCasterClass() == 'W')
		AISpellVar.engaged_detrimental_chance = (_engaged_detrimental_chance) ? _engaged_detrimental_chance : RuleI(Spells, AI_EngagedDetrimentalChanceHealer);
	else
		AISpellVar.engaged_detrimental_chance = (_engaged_detrimental_chance) ? _engaged_detrimental_chance : RuleI(Spells, AI_EngagedDetrimentalChance);
	AISpellVar.idle_no_sp_recast_min = (_idle_no_sp_recast_min) ? _idle_no_sp_recast_min : RuleI(Spells, AI_IdleNoSpellMinRecast);
	AISpellVar.idle_no_sp_recast_max = (_idle_no_sp_recast_max) ? _idle_no_sp_recast_max : RuleI(Spells, AI_IdleNoSpellMaxRecast);
	AISpellVar.idle_beneficial_chance = (_idle_beneficial_chance) ? _idle_beneficial_chance : RuleI(Spells, AI_IdleBeneficialChance);

	if (AIspells.empty())
		AIautocastspell_timer->Disable();
	else
		AIautocastspell_timer->Trigger();
	return true;
}

bool NPC::AI_AddNPCSpellsEffects(uint32 iDBSpellsEffectsID) {

	npc_spells_effects_id = iDBSpellsEffectsID;
	AIspellsEffects.clear();

	if (iDBSpellsEffectsID == 0)
		return false;

	DBnpcspellseffects_Struct* spell_effects_list = database.GetNPCSpellsEffects(iDBSpellsEffectsID);

	if (!spell_effects_list) {
		return false;
	}

	DBnpcspellseffects_Struct* parentlist = database.GetNPCSpellsEffects(spell_effects_list->parent_list);

	uint32 i;
	std::string debug_msg = StringFormat("Loading NPCSpellsEffects onto %s: dbspellseffectid=%u", GetName(), iDBSpellsEffectsID);
	if (spell_effects_list) {
		debug_msg.append(StringFormat(" (found, %u), parentlist=%u", spell_effects_list->numentries, spell_effects_list->parent_list));
		if (spell_effects_list->parent_list) {
			if (parentlist)
				debug_msg.append(StringFormat(" (found, %u)", parentlist->numentries));
			else
				debug_msg.append(" (not found)");
		}
	}
	else {
		debug_msg.append(" (not found)");
	}
	LogAI("[{}]", debug_msg.c_str());

	if (parentlist) {
		for (i=0; i<parentlist->numentries; i++) {
			if (GetLevel() >= parentlist->entries[i].minlevel && GetLevel() <= parentlist->entries[i].maxlevel && parentlist->entries[i].spelleffectid > 0) {
				if (!IsSpellEffectInList(spell_effects_list, parentlist->entries[i].spelleffectid, parentlist->entries[i].base,
					parentlist->entries[i].limit, parentlist->entries[i].max))
				{
				AddSpellEffectToNPCList(parentlist->entries[i].spelleffectid,
						parentlist->entries[i].base, parentlist->entries[i].limit,
						parentlist->entries[i].max);
				}
			}
		}
	}

	for (i=0; i<spell_effects_list->numentries; i++) {
		if (GetLevel() >= spell_effects_list->entries[i].minlevel && GetLevel() <= spell_effects_list->entries[i].maxlevel && spell_effects_list->entries[i].spelleffectid > 0) {
			AddSpellEffectToNPCList(spell_effects_list->entries[i].spelleffectid,
				spell_effects_list->entries[i].base, spell_effects_list->entries[i].limit,
				spell_effects_list->entries[i].max);
		}
	}

	return true;
}

void NPC::ApplyAISpellEffects(StatBonuses* newbon)
{
	if (!AI_HasSpellsEffects())
		return;

	for(int i=0; i < AIspellsEffects.size(); i++)
	{
		ApplySpellsBonuses(0, 0, newbon, 0, false, 10, 0,-1,
			true, AIspellsEffects[i].spelleffectid,  AIspellsEffects[i].base, AIspellsEffects[i].limit,AIspellsEffects[i].max);
	}

	return;
}

// adds a spell to the list, taking into account priority and resorting list as needed.
void NPC::AddSpellEffectToNPCList(uint16 iSpellEffectID, int32 base, int32 limit, int32 max)
{

	if(!iSpellEffectID)
		return;

	HasAISpellEffects = true;
	AISpellsEffects_Struct t;

	t.spelleffectid = iSpellEffectID;
	t.base = base;
	t.limit = limit;
	t.max = max;
	AIspellsEffects.push_back(t);

}

bool IsSpellEffectInList(DBnpcspellseffects_Struct* spelleffect_list, uint16 iSpellEffectID, int32 base, int32 limit, int32 max) {
	for (uint32 i=0; i < spelleffect_list->numentries; i++) {
		if (spelleffect_list->entries[i].spelleffectid == iSpellEffectID &&  spelleffect_list->entries[i].base == base
			&& spelleffect_list->entries[i].limit == limit && spelleffect_list->entries[i].max == max)
			return true;
	}
	return false;
}

bool IsSpellInList(DBnpcspells_Struct* spell_list, int16 iSpellID) {
	auto it = std::find_if(spell_list->entries.begin(), spell_list->entries.end(),
		[iSpellID](const DBnpcspells_entries_Struct &a) { return a.spellid == iSpellID; });
	return it != spell_list->entries.end();
}

// adds a spell to the list, taking into account priority and resorting list as needed.
void NPC::AddSpellToNPCList(int16 iPriority, int16 iSpellID, uint16 iType,
							int16 iManaCost, int32 iRecastDelay, int16 iResistAdjust)
{

	if(!IsValidSpell(iSpellID))
		return;

	HasAISpell = true;
	AISpells_Struct t;

	t.priority = iPriority;
	t.spellid = iSpellID;
	t.type = iType;
	t.manacost = iManaCost;
	t.recast_delay = iRecastDelay;
	t.time_cancast = 0;
	t.resist_adjust = iResistAdjust;

	AIspells.push_back(t);

	// If we're going from an empty list, we need to start the timer
	if (AIspells.size() == 1)
		AIautocastspell_timer->Start(100, false);
		
	if (iPriority == 0 && iType & (SpellType_Nuke | SpellType_Lifetap | SpellType_DOT | SpellType_Dispel | SpellType_Mez | SpellType_Slow | SpellType_Debuff | SpellType_Charm | SpellType_Root))
        hasZeroPrioritySpells = true;
}

void NPC::RemoveSpellFromNPCList(int16 spell_id)
{
	auto iter = AIspells.begin();
	while(iter != AIspells.end())
	{
		if((*iter).spellid == spell_id)
		{
			iter = AIspells.erase(iter);
			continue;
		}
		++iter;
	}
}

void NPC::AISpellsList(Client *c)
{
	if (!c)
		return;

	if (AIspells.size() > 0) {
		c->Message(
			Chat::White,
			fmt::format(
				"{} has {} AI spells.",
				GetCleanName(),
				AIspells.size()
			).c_str()
		);

		int spell_slot = 1;
		for (const auto &ai_spell : AIspells) {
			c->Message(
				Chat::White,
				fmt::format(
					"Spell {} | Name: {} ({}) Type: {} Mana Cost: {}",
					spell_slot,
					GetSpellName(ai_spell.spellid),
					ai_spell.spellid,
					ai_spell.type,
					ai_spell.manacost
				).c_str()
			);

			c->Message(
				Chat::White,
				fmt::format(
					"Spell {} | Priority: {} | Recast Delay: {}",
					spell_slot,
					ai_spell.priority,
					ai_spell.recast_delay
				).c_str()
			);

			if (ai_spell.time_cancast) {
				c->Message(
					Chat::White,
					fmt::format(
						"Spell {} | Time Can Cast : {}",
						spell_slot,
						ai_spell.time_cancast
					).c_str()
				);
			}

			if (ai_spell.resist_adjust) {
				c->Message(
					Chat::White,
					fmt::format(
						"Spell {} | Resist Adjust: {}",
						spell_slot,
						ai_spell.resist_adjust
					).c_str()
				);
			}

			spell_slot++;
		}
	}
	else {
		c->Message(
			Chat::White,
			fmt::format(
				"{} has no AI spells.",
				GetCleanName()
			).c_str()
		);
	}

	return;
}

DBnpcspells_Struct* ZoneDatabase::GetNPCSpells(uint32 npc_spells_id) 
{
	if (npc_spells_id == 0) {
		return nullptr;
	}

	auto it = npc_spells_cache.find(npc_spells_id);
	if (it != npc_spells_cache.end()) { // it's in the cache, easy =)
		return &it->second;
	}

	if (!npc_spells_loadtried.count(npc_spells_id)) { // no reason to ask the DB again if we have failed once already
		npc_spells_loadtried.insert(npc_spells_id);

		auto ns = NpcSpellsRepository::FindOne(*this, npc_spells_id);
		if (!ns.id) {
			return nullptr;
        }

		DBnpcspells_Struct ss;

		ss.parent_list                       = ns.parent_list;
        ss.attack_proc                       = ns.attack_proc;
        ss.proc_chance                       = ns.proc_chance;
        ss.range_proc                        = ns.range_proc;
        ss.rproc_chance                      = ns.rproc_chance;
        ss.defensive_proc                    = ns.defensive_proc;
        ss.dproc_chance                      = ns.dproc_chance;
        ss.fail_recast                       = ns.fail_recast;
        ss.engaged_no_sp_recast_min          = ns.engaged_no_sp_recast_min;
        ss.engaged_no_sp_recast_max          = ns.engaged_no_sp_recast_max;
        ss.engaged_beneficial_self_chance    = ns.engaged_b_self_chance;
        ss.engaged_beneficial_other_chance   = ns.engaged_b_other_chance;
        ss.engaged_detrimental_chance        = ns.engaged_d_chance;
        ss.idle_no_sp_recast_min             = ns.engaged_no_sp_recast_min;
        ss.idle_no_sp_recast_max             = ns.engaged_no_sp_recast_max;
        ss.idle_beneficial_chance            = ns.idle_b_chance;

		auto entries = NpcSpellsEntriesRepository::GetWhere(
			*this,
			fmt::format(
				"npc_spells_id = {} {} ORDER BY minlevel",
				npc_spells_id,
				ContentFilterCriteria::apply()
			)
		);

		for (auto &e : entries) {
			DBnpcspells_entries_Struct se{};

			se.spellid = e.spellid;
			se.type = e.type;
			se.minlevel = e.minlevel;
			se.maxlevel = e.maxlevel;
			se.manacost = e.manacost;
			se.recast_delay = e.recast_delay;
			se.priority = e.priority;

			// some spell types don't make much since to be priority 0, so fix that
			if (!(se.type & SpellTypes_Innate) && se.priority == 0) {
				se.priority = 1;
			}

			if (e.resist_adjust) {
				se.resist_adjust = e.resist_adjust;
			}
			else if (IsValidSpell(e.spellid)) {
				se.resist_adjust = spells[e.spellid].ResistDiff;
			}

			ss.entries.push_back(se);
		 }

		npc_spells_cache.emplace(std::make_pair(npc_spells_id, ss));

		return &npc_spells_cache[npc_spells_id];
    }

	return nullptr;
}

uint32 ZoneDatabase::GetMaxNPCSpellsID() {

	std::string query = "SELECT max(id) from npc_spells";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

    if (results.RowCount() != 1)
        return 0;

    auto row = results.begin();

    if (!row[0])
        return 0;

    return atoi(row[0]);
}

DBnpcspellseffects_Struct *ZoneDatabase::GetNPCSpellsEffects(uint32 iDBSpellsEffectsID)
{
	if (iDBSpellsEffectsID == 0)
		return nullptr;

	if (!npc_spellseffects_cache) {
		npc_spellseffects_maxid = GetMaxNPCSpellsEffectsID();
		npc_spellseffects_cache = new DBnpcspellseffects_Struct *[npc_spellseffects_maxid + 1];
		npc_spellseffects_loadtried = new bool[npc_spellseffects_maxid + 1];
		for (uint32 i = 0; i <= npc_spellseffects_maxid; i++) {
			npc_spellseffects_cache[i] = 0;
			npc_spellseffects_loadtried[i] = false;
		}
	}

	if (iDBSpellsEffectsID > npc_spellseffects_maxid)
		return nullptr;

	if (npc_spellseffects_cache[iDBSpellsEffectsID]) // it's in the cache, easy =)
		return npc_spellseffects_cache[iDBSpellsEffectsID];

	if (npc_spellseffects_loadtried[iDBSpellsEffectsID])
		return nullptr;

	npc_spellseffects_loadtried[iDBSpellsEffectsID] = true;

	std::string query =
	    StringFormat("SELECT id, parent_list FROM npc_spells_effects WHERE id=%d", iDBSpellsEffectsID);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return nullptr;
	}

	if (results.RowCount() != 1)
		return nullptr;

	auto row = results.begin();
	uint32 tmpparent_list = atoi(row[1]);

	query = StringFormat("SELECT spell_effect_id, minlevel, "
			     "maxlevel,se_base, se_limit, se_max "
			     "FROM npc_spells_effects_entries "
			     "WHERE npc_spells_effects_id = %d ORDER BY minlevel",
			     iDBSpellsEffectsID);
	results = QueryDatabase(query);
	if (!results.Success())
		return nullptr;

	uint32 tmpSize =
	    sizeof(DBnpcspellseffects_Struct) + (sizeof(DBnpcspellseffects_entries_Struct) * results.RowCount());
	npc_spellseffects_cache[iDBSpellsEffectsID] = (DBnpcspellseffects_Struct *)new uchar[tmpSize];
	memset(npc_spellseffects_cache[iDBSpellsEffectsID], 0, tmpSize);
	npc_spellseffects_cache[iDBSpellsEffectsID]->parent_list = tmpparent_list;
	npc_spellseffects_cache[iDBSpellsEffectsID]->numentries = results.RowCount();

	int entryIndex = 0;
	for (row = results.begin(); row != results.end(); ++row, ++entryIndex) {
		int spell_effect_id = atoi(row[0]);
		npc_spellseffects_cache[iDBSpellsEffectsID]->entries[entryIndex].spelleffectid = spell_effect_id;
		npc_spellseffects_cache[iDBSpellsEffectsID]->entries[entryIndex].minlevel = atoi(row[1]);
		npc_spellseffects_cache[iDBSpellsEffectsID]->entries[entryIndex].maxlevel = atoi(row[2]);
		npc_spellseffects_cache[iDBSpellsEffectsID]->entries[entryIndex].base = atoi(row[3]);
		npc_spellseffects_cache[iDBSpellsEffectsID]->entries[entryIndex].limit = atoi(row[4]);
		npc_spellseffects_cache[iDBSpellsEffectsID]->entries[entryIndex].max = atoi(row[5]);
	}

	return npc_spellseffects_cache[iDBSpellsEffectsID];
}

uint32 ZoneDatabase::GetMaxNPCSpellsEffectsID() {

	std::string query = "SELECT max(id) FROM npc_spells_effects";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

    if (results.RowCount() != 1)
        return 0;

    auto row = results.begin();
    if (!row[0])
        return 0;

    return atoi(row[0]);
}

// This will halt scripted MoveTo() commands, not grid movement
void NPC::StopQuestMove(bool setGuardSpot) {
	if (!roamer || cur_wp != EQ::WaypointStatus::QuestControlNoGrid)
		return;

	StopNavigation();
	roamer = false;
	SetGrid(0 - GetGrid());
	SetAppearance(eaStanding, false);
	if (setGuardSpot)
		SaveGuardSpot();
}
