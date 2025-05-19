/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/eq_constants.h"
#include "../common/eq_packet_structs.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/strings.h"
#include "../common/fastmath.h"
#include "../common/misc_functions.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "water_map.h"
#include "queryserv.h"
#include "worldserver.h"
#include "zone.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern WorldServer worldserver;
extern QueryServ* QServ;
extern FastMath g_Math;

#ifdef _WINDOWS
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#endif

extern EntityList entity_list;
extern Zone* zone;

bool Mob::AttackAnimation(EQ::skills::SkillType &skillinuse, int Hand, const EQ::ItemInstance* weapon)
{
	// Determine animation
	DoAnimation type = DoAnimation::None;
	if (weapon && weapon->IsType(EQ::item::ItemClassCommon)) {
		const EQ::ItemData* item = weapon->GetItem();

		Log(Logs::Detail, Logs::Attack, "Weapon skill : %i", item->ItemType);

		switch (item->ItemType)
		{
			case EQ::item::ItemType1HSlash: // 1H Slashing
			{
				skillinuse = EQ::skills::Skill1HSlashing;
				type = DoAnimation::Weapon1H;
				break;
			}
			case EQ::item::ItemType2HSlash: // 2H Slashing
			{
				skillinuse = EQ::skills::Skill2HSlashing;
				type = DoAnimation::Slashing2H;
				break;
			}
			case EQ::item::ItemType1HPiercing: // Piercing
			{
				skillinuse = EQ::skills::Skill1HPiercing;
				type = DoAnimation::Piercing;
				break;
			}
			case EQ::item::ItemType1HBlunt: // 1H Blunt
			{
				skillinuse = EQ::skills::Skill1HBlunt;
				type = DoAnimation::Weapon1H;
				break;
			}
			case EQ::item::ItemType2HBlunt: // 2H Blunt
			{
				skillinuse = EQ::skills::Skill2HBlunt;
				type = DoAnimation::Weapon2H; //anim2HWeapon
              	break;
			}
			case EQ::item::ItemType2HPiercing: // 2H Piercing
			{
				skillinuse = EQ::skills::Skill1HPiercing; // change to Skill2HPiercing once activated
				type = DoAnimation::Weapon2H;
				break;
			}
			case EQ::item::ItemTypeMartial:
			{
				skillinuse = EQ::skills::SkillHandtoHand;
				type = DoAnimation::Hand2Hand;
				break;
			}
			default:
			{
				skillinuse = EQ::skills::SkillHandtoHand;
				type = DoAnimation::Hand2Hand;
				break;
			}
		}// switch
	}
	else if(IsNPC()) {

		switch (skillinuse)
		{
			case EQ::skills::Skill1HSlashing: // 1H Slashing
			{
				type = DoAnimation::Weapon1H;
				break;
			}
			case EQ::skills::Skill2HSlashing: // 2H Slashing
			{
				type = DoAnimation::Slashing2H;
				break;
			}
			case EQ::skills::Skill1HPiercing: // Piercing
			{
				type = DoAnimation::Piercing;
				break;
			}
			case EQ::skills::Skill1HBlunt: // 1H Blunt
			{
				type = DoAnimation::Weapon1H;
				break;
			}
			case EQ::skills::Skill2HBlunt: // 2H Blunt
			{
				type = DoAnimation::Weapon2H;
				break;
			}
			case EQ::skills::SkillHandtoHand:
			{
				type = DoAnimation::Hand2Hand;
				break;
			}
			default:
			{
				type = DoAnimation::Hand2Hand;
				break;
			}
		}// switch
	}
	else {
		skillinuse = EQ::skills::SkillHandtoHand;
		type = DoAnimation::Hand2Hand;
	}

	// If we're attacking with the secondary hand, play the dual wield anim
	if (Hand == EQ::invslot::slotSecondary)	// DW anim
		type = DoAnimation::DualWield;

	DoAnim(type, 0, false);

	return true;
}

// returns true on hit
bool Mob::AvoidanceCheck(Mob* attacker, EQ::skills::SkillType skillinuse)
{
	Mob *defender = this;

	if (IsClient() && CastToClient()->IsSitting())
	{
		if (IsClient() && attacker->IsNPC())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillDefense, attacker, zone->skill_difficulty[EQ::skills::SkillDefense].difficulty);
		return true;
	}

	int toHit = attacker->GetToHit(skillinuse);
	int avoidance = defender->GetAvoidance();
	int toHitPct = 0;
	int avoidancePct = 0;
	
	// Hit Chance percent modifier
	// Disciplines: Evasive, Precision, Deadeye, Trueshot, Charge
	// Buffs: the Eagle Eye line
	toHitPct = attacker->itembonuses.HitChanceEffect[skillinuse] +
		attacker->spellbonuses.HitChanceEffect[skillinuse] +
		attacker->aabonuses.HitChanceEffect[skillinuse] +
		attacker->itembonuses.HitChanceEffect[EQ::skills::HIGHEST_SKILL + 1] +
		attacker->spellbonuses.HitChanceEffect[EQ::skills::HIGHEST_SKILL + 1] +
		attacker->aabonuses.HitChanceEffect[EQ::skills::HIGHEST_SKILL + 1];

	// Avoidance chance percent modifier
	// Disciplines: Evasive, Precision, Voiddance, Fortitude
	avoidancePct = defender->spellbonuses.AvoidMeleeChanceEffect + defender->itembonuses.AvoidMeleeChanceEffect;

	toHit = toHit * (100 + toHitPct) / 100;
	avoidance = avoidance * (100 + avoidancePct) / 100;

	Log(Logs::Detail, Logs::Attack, "AvoidanceCheck: %s attacked by %s;  Avoidance: %i (pct: %i)  To-Hit: %i (pct: %i)", 
		defender->GetName(), attacker->GetName(), avoidance, avoidancePct, toHit, toHitPct);

	double hitChance;
	toHit += 10;
	avoidance += 10;

	if (toHit * 1.21 > avoidance)
	{
		hitChance = 1.0 - avoidance / (toHit * 1.21 * 2.0);
	}
	else
	{
		hitChance = toHit * 1.21 / (avoidance * 2.0);
	}

	if (IsClient() && attacker->IsNPC())
		CastToClient()->CheckIncreaseSkill(EQ::skills::SkillDefense, attacker, zone->skill_difficulty[EQ::skills::SkillDefense].difficulty);

	if (zone->random.Real(0.0, 1.0) < hitChance)
	{
		Log(Logs::Detail, Logs::Attack, "Hit;  Hit chance was %0.1f%%", hitChance*100);
		return true;
	}

	Log(Logs::Detail, Logs::Attack, "Miss;  Hit chance was %0.1f%%", hitChance * 100);
	return false;
}

bool Mob::AvoidDamage(Mob* attacker, int32 &damage, bool isRangedAttack)
{
	/* solar: called when a mob is attacked, does the checks to see if it's a hit
	* and does other mitigation checks. 'this' is the mob being attacked.
	*
	* special return values:
	* -1 - block
	* -2 - parry
	* -3 - riposte
	* -4 - dodge
	*
	*/
	Mob *defender = this;

	bool InFront = attacker->InFrontMob(this, attacker->GetX(), attacker->GetY());

	// block
	if (GetSkill(EQ::skills::SkillBlock))
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillBlock, attacker, zone->skill_difficulty[EQ::skills::SkillBlock].difficulty);

			// check auto discs ... I guess aa/items too :P
		if (spellbonuses.IncreaseBlockChance == 10000 || aabonuses.IncreaseBlockChance == 10000 ||
			itembonuses.IncreaseBlockChance == 10000) {
			damage = DMG_BLOCK;
			return true;
		}
		int chance = GetSkill(EQ::skills::SkillBlock) + 100;
		chance += (chance * (aabonuses.IncreaseBlockChance + spellbonuses.IncreaseBlockChance + itembonuses.IncreaseBlockChance)) / 100;
		chance /= 25;

		if (zone->random.Roll(chance)) {
			damage = DMG_BLOCK;
			return true;
		}
	}

	// parry
	if (GetSkill(EQ::skills::SkillParry) && InFront && !isRangedAttack)
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillParry, attacker, zone->skill_difficulty[EQ::skills::SkillParry].difficulty);

		// check auto discs ... I guess aa/items too :P
		if (spellbonuses.ParryChance == 10000 || aabonuses.ParryChance == 10000 || itembonuses.ParryChance == 10000) {
			damage = DMG_PARRY;
			return true;
		}
		int chance = GetSkill(EQ::skills::SkillParry) + 100;
		chance += (chance * (aabonuses.ParryChance + spellbonuses.ParryChance + itembonuses.ParryChance)) / 100;
		chance /= 50;		// this is 45 in modern EQ.  Old EQ logs parsed to a lower parry rate, so raising this

		if (zone->random.Roll(chance)) {
			damage = DMG_PARRY;
			return true;
		}
	}

	// riposte
	if (!isRangedAttack && GetSkill(EQ::skills::SkillRiposte) && InFront)
	{
		bool cannotRiposte = false;

		if (IsClient())
		{
			EQ::ItemInstance* weapon = nullptr;
			weapon = CastToClient()->GetInv().GetItem(EQ::invslot::slotPrimary);

			if (weapon != nullptr && !weapon->IsWeapon())
			{
				cannotRiposte = true;
			}
			else
			{
				CastToClient()->CheckIncreaseSkill(EQ::skills::SkillRiposte, attacker, zone->skill_difficulty[EQ::skills::SkillRiposte].difficulty);
			}
		}

		// riposting ripostes is possible, but client attacks become unripable while under a rip disc
		if (attacker->IsEnraged() ||
			(attacker->IsClient() && (attacker->aabonuses.RiposteChance + attacker->spellbonuses.RiposteChance + attacker->itembonuses.RiposteChance) >= 10000)
		)
			cannotRiposte = true;

		if (!cannotRiposte)
		{
			if (IsEnraged() || spellbonuses.RiposteChance == 10000 || aabonuses.RiposteChance == 10000 || itembonuses.RiposteChance == 10000)
			{
				damage = DMG_RIPOSTE;
				return true;
			}

			int chance = GetSkill(EQ::skills::SkillRiposte) + 100;
			chance += (chance * (aabonuses.RiposteChance + spellbonuses.RiposteChance + itembonuses.RiposteChance)) / 100;
			chance /= 55;		// this is 50 in modern EQ.  Old EQ logs parsed to a lower rate, so raising this

			if (chance > 0 && zone->random.Roll(chance)) // could be <0 from offhand stuff
			{
				// March 19 2002 patch made pets not take non-enrage ripostes from NPCs.  it said 'more likely to avoid' but player comments say zero and logs confirm
				if (IsNPC() && attacker->IsPet())
				{
					damage = DMG_MISS;  // converting ripostes to misses.  don't know what Sony did but erring on conservative
					return true;
				}

				damage = DMG_RIPOSTE;
				return true;
			}
		}
	}

	// dodge
	if (GetSkill(EQ::skills::SkillDodge) && InFront)
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillDodge, attacker, zone->skill_difficulty[EQ::skills::SkillDodge].difficulty);

		// check auto discs ... I guess aa/items too :P
		if (spellbonuses.DodgeChance == 10000 || aabonuses.DodgeChance == 10000 || itembonuses.DodgeChance == 10000) {
			damage = DMG_DODGE;
			return true;
		}
		int chance = GetSkill(EQ::skills::SkillDodge) + 100;
		chance += (chance * (aabonuses.DodgeChance + spellbonuses.DodgeChance + itembonuses.DodgeChance)) / 100;
		chance /= 45;

		if (zone->random.Roll(chance)) {
			damage = DMG_DODGE;
			return true;
		}
	}

	return false;
}

int Mob::RollD20(double offense, double mitigation)
{
	int atkRoll = zone->random.Roll0(offense + 5);
	int defRoll = zone->random.Roll0(mitigation + 5);

	int avg = (offense + mitigation + 10) / 2;
	int index = std::max(0, (atkRoll - defRoll) + (avg / 2));
	index = (index * 20) / avg;
	index = std::max(0, index);
	index = std::min(19, index);

	return index + 1;
}

// Our database uses a min hit and max hit system, instead of Sony's DB + baseDmg * 0.1-2.0
// This calcs a DB (which is minHit - DI) from min and max hits
int NPC::GetDamageBonus()
{
	if (min_dmg > max_dmg)
		return min_dmg;

	int di1k = ((max_dmg - min_dmg) * 1000) / 19;		// multiply damage interval by 1000 to avoid using floats
	di1k = (di1k + 50) / 100 * 100;						// round DI to nearest tenth of a point
	int db = max_dmg * 1000 - di1k * 20;

	return db / 1000;
}

// Our database uses a min hit and max hit system, instead of Sony's DB + baseDmg * 0.1-2.0
// This calcs a baseDamage value (which is DI*10) from min and max hits
// baseDamage is the equivalent to weapon damage for clients
int NPC::GetBaseDamage(Mob* defender, int slot)
{
	if (slot != EQ::invslot::slotSecondary && slot != EQ::invslot::slotRange)
		slot = EQ::invslot::slotPrimary;

	int baseDamage = 1;

	if (max_dmg > min_dmg)
	{
		int di1k = (max_dmg - min_dmg) * 1000 / 19;			// multiply damage interval by 1000 to avoid using floats
		di1k = (di1k + 50) / 100 * 100;						// round DI to nearest tenth of a point
		baseDamage = di1k / 100;
	}

	const EQ::ItemData* weapon = nullptr;

	if (equipment[slot] > 0)
		weapon = database.GetItem(equipment[slot]);

	if (weapon)
	{
		int weaponDmg = weapon->Damage;
		
		if (weapon->ElemDmgAmt)
		{
			weaponDmg += CalcEleWeaponResist(weapon->ElemDmgAmt, weapon->ElemDmgType, defender);
		}

		if (weapon->BaneDmgBody == defender->GetBodyType() || weapon->BaneDmgRace == defender->GetRace())
		{
			weaponDmg += weapon->BaneDmgAmt;
		}

		if (slot == EQ::invslot::slotRange)
		{
			weapon = database.GetItem(equipment[EQ::invslot::slotAmmo]);
			if (weapon)
			{
				if (weapon->ElemDmgAmt)
				{
					weaponDmg += CalcEleWeaponResist(weapon->ElemDmgAmt, weapon->ElemDmgType, defender);
				}
			}
		}

		if (weaponDmg > baseDamage)
			baseDamage = weaponDmg;
	}

	return baseDamage;
}

// returns the client's weapon (or hand to hand) damage
// slot parameter should be one of: SlotPrimary, SlotSecondary, SlotRange, SlotAmmo
// does not check for immunities
// calling this with SlotRange will also add the arrow damage
int Client::GetBaseDamage(Mob *defender, int slot)
{
	if (slot != EQ::invslot::slotSecondary && slot != EQ::invslot::slotRange && slot != EQ::invslot::slotAmmo)
		slot = EQ::invslot::slotPrimary;

	int dmg = 0;

	EQ::ItemInstance* weaponInst = GetInv().GetItem(slot);
	const EQ::ItemData* weapon = nullptr;
	if (weaponInst)
		weapon = weaponInst->GetItem();

	if (weapon)
	{
		// cheaters or GMs doing stuff
		if (weapon->ReqLevel > GetLevel())
			return dmg;

		if (!weaponInst->IsEquipable(GetBaseRace(), GetClass()))
			return dmg;

		if (GetLevel() < weapon->RecLevel)
			dmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon->RecLevel, weapon->Damage);
		else
			dmg = weapon->Damage;

		if (weapon->ElemDmgAmt && !defender->GetSpecialAbility(SpecialAbility::MagicImmunity))
		{
			int eledmg = 0;

			if (GetLevel() < weapon->RecLevel)
				eledmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon->RecLevel, weapon->ElemDmgAmt);
			else
				eledmg = weapon->ElemDmgAmt;

			if (eledmg)
			{
				eledmg = CalcEleWeaponResist(eledmg, weapon->ElemDmgType, defender);
				dmg += eledmg;
			}
		}

		if (weapon->BaneDmgBody == defender->GetBodyType() || weapon->BaneDmgRace == defender->GetRace())
		{
			if (GetLevel() < weapon->RecLevel)
				dmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon->RecLevel, weapon->BaneDmgAmt);
			else
				dmg += weapon->BaneDmgAmt;
		}

		if (slot == EQ::invslot::slotRange && GetInv().GetItem(EQ::invslot::slotAmmo))
		{
			dmg += GetBaseDamage(defender, EQ::invslot::slotAmmo);
		}
	}
	else if (slot == EQ::invslot::slotPrimary || slot == EQ::invslot::slotSecondary)
		dmg = GetHandToHandDamage();

	return dmg;
}

// For both Clients and NPCs.  Sony calls client weapon damage and innate NPC damage 'base damage'
// NPC base damage is essentially DI * 10.  Special skills have their own base damage
// All melee, archery and throwing damage should pass through here
// For reference, NPC damage is: minHit = DB + DI*1; maxHit = DB + DI*20.  Clients do the same, but get a multiplier after it
int Mob::CalcMeleeDamage(Mob* defender, int baseDamage, EQ::skills::SkillType skill)
{
	if (!defender || !baseDamage)
		return 0;

	// ranged physical damage does half that of melee
	if ((skill == EQ::skills::SkillArchery || skill == EQ::skills::SkillThrowing) && baseDamage > 1)
		baseDamage /= 2;

	int offense = GetOffense(skill);

	// mitigation roll
	int roll = RollD20(offense, defender->GetMitigation());

	if (defender->IsClient() && defender->CastToClient()->IsSitting())
		roll = 20;

	// SE_MinDamageModifier[186] for disciplines: Fellstrike, Innerflame, Duelist, Bestial Rage
	// min hit becomes 4 x weapon damage + 1 x damage bonus
	int minHit = baseDamage * GetMeleeMinDamageMod_SE(skill) / 100;

	// SE_DamageModifier[185] for disciplines: Aggressive, Ashenhand, Bestial Rage, Defensive, Duelist,
	//                                         Fellstrike, Innerflame, Silentfist, Thunderkick
	baseDamage += baseDamage * GetMeleeDamageMod_SE(skill) / 100;

	// SE_MeleeMitigation[168] for disciplines: Defensive (-50), Stonestance & Protective Spirit (-90)
	//											Aggressive (+50)
	baseDamage += baseDamage * defender->GetSpellBonuses().MeleeMitigationEffect / 100;

	if (defender->IsClient() && IsPet() && GetOwner()->IsClient()) {
		// pets do reduced damage to clients in pvp
		baseDamage /= 2;
	}

	int damage = (roll * baseDamage + 5) / 10;
	if (damage < minHit) damage = minHit;
	if (damage < 1)
		damage = 1;

	if (IsClient())
		CastToClient()->RollDamageMultiplier(offense, damage, skill);

	return damage;
}

// the output of this function is precise and is based on the code from:
// https://forums.daybreakgames.com/eq/index.php?threads/progression-monks-we-have-work-to-do.229581/
uint32 Client::RollDamageMultiplier(uint32 offense, int& damage, EQ::skills::SkillType skill)
{
	int rollChance = 51;
	int maxExtra = 210;
	int minusFactor = 105;

	if (GetClass() == Class::Monk && level >= 65)
	{
		rollChance = 83;
		maxExtra = 300;
		minusFactor = 50;
	}
	else if (level >= 65 || (GetClass() == Class::Monk && level >= 63))
	{
		rollChance = 81;
		maxExtra = 295;
		minusFactor = 55;
	}
	else if (level >= 63 || (GetClass() == Class::Monk && level >= 60))
	{
		rollChance = 79;
		maxExtra = 290;
		minusFactor = 60;
	}
	else if (level >= 60 || (GetClass() == Class::Monk && level >= 56))
	{
		rollChance = 77;
		maxExtra = 285;
		minusFactor = 65;
	}
	else if (level >= 56)
	{
		rollChance = 72;
		maxExtra = 265;
		minusFactor = 70;
	}
	else if (level >= 51 || GetClass() == Class::Monk)
	{
		rollChance = 65;
		maxExtra = 245;
		minusFactor = 80;
	}

	int baseBonus = (static_cast<int>(offense) - minusFactor) / 2;
	if (baseBonus < 10)
		baseBonus = 10;

	if (zone->random.Roll(rollChance))
	{
		uint32 roll;

		roll = zone->random.Int(0, baseBonus) + 100;
		if (roll > maxExtra)
			roll = maxExtra;

		damage = damage * roll / 100;

		if (level >= 55 && damage > 1 && skill != EQ::skills::SkillArchery && IsWarriorClass())
			damage++;

		return roll;
	}
	else
	{
		return 100;
	}
}

// decompile shows that weapon elemental damage resist uses a special function
// credit to demonstar
int Mob::CalcEleWeaponResist(int weaponDamage, int resistType, Mob *target)
{
	int resistValue = 0;

	switch (resistType)
	{
	case RESIST_FIRE:
		resistValue = target->GetFR();
		break;
	case RESIST_COLD:
		resistValue = target->GetCR();
		break;
	case RESIST_MAGIC:
		resistValue = target->GetMR();
		break;
	case RESIST_DISEASE:
		resistValue = target->GetDR();
		break;
	case RESIST_POISON:
		resistValue = target->GetPR();
		break;
	}

	if (resistValue > 200)
		return 0;

	int roll = zone->random.Int(1, 201) - resistValue;
	if (roll < 1)
		return 0;
	if (roll <= 99)
		return weaponDamage * roll / 100;
	else
		return weaponDamage;
}

// Checks for weapon (including ranged) immunity, and magic flagged armor (kicks, punches, bashes)
// slot argument should be one of the following: SlotPrimary, SlotSecondary, SlotRange, SlotAmmo, SlotFeet, SlotHands
bool Mob::IsImmuneToMelee(Mob* attacker, int slot)
{
	if (!attacker || GetInvul() || GetSpecialAbility(SpecialAbility::MeleeImmunity))
		return true;

	if (slot != EQ::invslot::slotSecondary && slot != EQ::invslot::slotRange && slot != EQ::invslot::slotAmmo && slot != EQ::invslot::slotFeet && slot != EQ::invslot::slotHands)
		slot = EQ::invslot::slotPrimary;

	if (attacker->IsNPC() && slot != EQ::invslot::slotPrimary && slot != EQ::invslot::slotSecondary)
		slot = EQ::invslot::slotPrimary;

	EQ::ItemInstance* weaponInst = nullptr;
	const EQ::ItemData* weapon = nullptr;

	if (attacker->IsClient())
	{
		weaponInst = attacker->CastToClient()->GetInv().GetItem(slot);
		if (weaponInst)
			weapon = weaponInst->GetItem();
	}
	else if (attacker->IsNPC())
	{
		uint32 weaponID = attacker->CastToNPC()->GetEquipment(slot == EQ::invslot::slotSecondary ? EQ::textures::weaponSecondary : EQ::textures::weaponPrimary);

		if (weaponID > 0)
			weapon = database.GetItem(weaponID);
	}

	bool magicWeapon = false;
	if (weapon)
	{
		if (weapon->Magic || attacker->spellbonuses.MagicWeapon || attacker->itembonuses.MagicWeapon)
			magicWeapon = true;
	}
	else if ((attacker->GetClass() == Class::Monk || attacker->GetClass() == Class::Beastlord) && attacker->GetLevel() > 29
		&& (slot == EQ::invslot::slotPrimary || slot == EQ::invslot::slotSecondary || slot == EQ::invslot::slotHands)
	)
		magicWeapon = true;
		
	if (!magicWeapon && GetSpecialAbility(SpecialAbility::MeleeImmunityExceptMagical))
	{
		if (attacker->IsNPC() && !attacker->GetSpecialAbility(SpecialAbility::MagicalAttack) && attacker->GetLevel() < MAGIC_ATTACK_LEVEL)
		{
			return true;
		}
		else if (attacker->IsClient())
		{
			// grant magic attack to fists but not to held weapons if gloves are magic to these three classes
			if ( (attacker->GetClass() == Class::Monk || attacker->GetClass() == Class::Beastlord || attacker->GetClass() == Class::Bard)
				&& ((!weapon && (slot == EQ::invslot::slotPrimary || slot == EQ::invslot::slotSecondary)) || slot == EQ::invslot::slotHands)
			)
			{
				EQ::ItemInstance *gloves = attacker->CastToClient()->GetInv().GetItem(EQ::invslot::slotHands);

				if (!gloves || !gloves->GetItem()->Magic)
					return true;
			}
			else
				return true;
		}
	}

	// don't think there are weapons that are pure elemental damage, but handling them anyway
	if (weapon && !weapon->Damage && weapon->ElemDmgAmt && GetSpecialAbility(SpecialAbility::MagicImmunity))
		return true;

	if (GetSpecialAbility(SpecialAbility::MeleeImmunityExceptBane))
	{
		if (attacker->IsClient())
		{
			// Use primary hand weapon to check for bane immunity on special attacks
			if (slot != EQ::invslot::slotPrimary && slot != EQ::invslot::slotSecondary && slot != EQ::invslot::slotRange)
			{
				weapon = nullptr;
				weaponInst = attacker->CastToClient()->GetInv().GetItem(EQ::invslot::slotPrimary);
				if (weaponInst)
					weapon = weaponInst->GetItem();
			}

			if (!weapon || (weapon->BaneDmgBody != GetBodyType() && weapon->BaneDmgRace != GetRace()))
				return true;
		}
		else
		{
			if (!attacker->GetSpecialAbility(SpecialAbility::BaneAttack)
				&& (!weapon || (weapon->BaneDmgBody != GetBodyType() && weapon->BaneDmgRace != GetRace()))
			)
				return true;
		}
	}

	return false;
}

//note: throughout this method, setting `damage` to a negative is a way to
//stop the attack calculations
bool Client::Attack(Mob* other, int hand, int damagePct)
{
	if (!other) {
		SetTarget(nullptr);
		Log(Logs::General, Logs::Error, "A null Mob object was passed to Client::Attack() for evaluation!");
		return false;
	}

	if (hand != EQ::invslot::slotSecondary)
		hand = EQ::invslot::slotPrimary;

	Log(Logs::Detail, Logs::Combat, "Attacking %s with hand %d", other?other->GetName():"(nullptr)", hand);

	//SetAttackTimer();
	if (
		(IsCasting() && GetClass() != Class::Bard)
		|| other == nullptr
		|| ((IsClient() && CastToClient()->dead) || (other->IsClient() && other->CastToClient()->dead))
		|| (GetHP() < 0)
		|| (!IsAttackAllowed(other))
		) {
		Log(Logs::Detail, Logs::Combat, "Attack canceled, invalid circumstances.");
		return false; // Only bards can attack while casting
	}

	if(DivineAura() && !GetGM()) {//cant attack while invulnerable unless your a gm
		Log(Logs::Detail, Logs::Combat, "Attack canceled, Divine Aura is in effect.");
		Message_StringID(Chat::DefaultText, DIVINE_AURA_NO_ATK);	//You can't attack while invulnerable!
		return false;
	}

	if (IsFeigned() || other->HasDied())
		return false;

	if (!GetTarget())
		SetTarget(other);

	EQ::ItemInstance* weapon = GetInv().GetItem(hand);

	if(weapon != nullptr) {
		if (!weapon->IsWeapon()) {
			Log(Logs::Detail, Logs::Combat, "Attack canceled, Item %s (%d) is not a weapon.", weapon->GetItem()->Name, weapon->GetID());
			return(false);
		}
		Log(Logs::Detail, Logs::Combat, "Attacking with weapon: %s (%d)", weapon->GetItem()->Name, weapon->GetID());
	} else {
		Log(Logs::Detail, Logs::Combat, "Attacking without a weapon.");
	}

	// calculate attack_skill and skillinuse depending on hand and weapon
	// also send Packet to near clients
	EQ::skills::SkillType skillinuse;

	AttackAnimation(skillinuse, hand, weapon);
	Log(Logs::Detail, Logs::Combat, "Attacking with %s in slot %d using skill %d", weapon?weapon->GetItem()->Name:"Fist", hand, skillinuse);

	AddWeaponAttackFatigue(weapon);

	// Now figure out damage
	int damage = 1;
	uint8 mylevel = GetLevel();
	int baseDamage = GetBaseDamage(other, hand);

	// anti-twink damage caps.  Taken from decompiles
	if (mylevel < 10)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 9)
				baseDamage = 9;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 6)
				baseDamage = 6;
			break;
		default:
			if (baseDamage > 10)
				baseDamage = 10;
		}
	}
	else if (mylevel < 20)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 12)
				baseDamage = 12;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 10)
				baseDamage = 10;
			break;
		default:
			if (baseDamage > 14)
				baseDamage = 14;
		}
	}
	else if (mylevel < 30)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 20)
				baseDamage = 20;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 12)
				baseDamage = 12;
			break;
		default:
			if (baseDamage > 30)
				baseDamage = 30;
		}
	}
	else if (mylevel < 40)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 26)
				baseDamage = 26;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 18)
				baseDamage = 18;
			break;
		default:
			if (baseDamage > 60)
				baseDamage = 60;
		}
	}
	/*
	// these are in the decompile but so unrealistic, commenting them out for cycles
	// also caps GM weapons
	else
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 80)
				baseDamage = 80;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 40)
				baseDamage = 40;
			break;
		default:
			if (baseDamage > 200)
				baseDamage = 200;
		}
	}*/
	int damageBonus = 0;
	if (hand == EQ::invslot::slotPrimary)
		damageBonus = GetDamageBonus();
	int hate = baseDamage + damageBonus;

	if (other->IsImmuneToMelee(this, hand))
	{
		damage = DMG_INVUL;
	}
	else
	{
		// check avoidance skills
		other->AvoidDamage(this, damage);

		if (damage < 0 && (damage == DMG_DODGE || damage == DMG_PARRY || damage == DMG_RIPOSTE || damage == DMG_BLOCK)
			&& aabonuses.StrikeThrough && zone->random.Roll(aabonuses.StrikeThrough))
		{
			damage = 1;		// Warrior Tactical Mastery AA
		}

		//riposte
		if (damage == DMG_RIPOSTE)
		{
			DoRiposte(other);
			if (IsDead()) return false;
		}

		if (damage > 0)
		{
			// swing not avoided by skills; do avoidance AC check
			if (!other->AvoidanceCheck(this, skillinuse))
			{
				Log(Logs::Detail, Logs::Combat, "Attack missed. Damage set to 0.");
				damage = DMG_MISS;
			}
		}

		CheckIncreaseSkill(skillinuse, other, zone->skill_difficulty[skillinuse].difficulty);
		CheckIncreaseSkill(EQ::skills::SkillOffense, other, zone->skill_difficulty[EQ::skills::SkillOffense].difficulty);

		if (damage > 0)
		{
			//try a finishing blow.. if successful end the attack
			if(TryFinishingBlow(other, skillinuse, damageBonus))
				return (true);

			damage = damageBonus + CalcMeleeDamage(other, baseDamage, skillinuse);

			if (damagePct <= 0)
				damagePct = 100;
			damage = damage * damagePct / 100;

			other->TryShielderDamage(this, damage, skillinuse);		// warrior /shield
			TryCriticalHit(other, skillinuse, damage, baseDamage, damageBonus);

			Log(Logs::Detail, Logs::Combat, "Damage calculated to %d (str %d, skill %d, DMG %d, lv %d)",
				damage, GetSTR(), GetSkill(skillinuse), baseDamage, mylevel);
		}
	}

	// Hate Generation is on a per swing basis, regardless of a hit, miss, or block, its always the same.
	// If we are this far, this means we are atleast making a swing.
	other->AddToHateList(this, hate);

	///////////////////////////////////////////////////////////
	////// Send Attack Damage
	///////////////////////////////////////////////////////////
	other->Damage(this, damage, SPELL_UNKNOWN, skillinuse);

	if (IsDead()) return false;

	MeleeLifeTap(damage);

	// old rogue poison from apply poison skill.  guaranteed procs first hit then fades
	if (poison_spell_id && damage > 0 && hand == EQ::invslot::slotPrimary && skillinuse == EQ::skills::Skill1HPiercing)
	{
		ExecWeaponProc(weapon, poison_spell_id, other);
		poison_spell_id = 0;
	}

	CommonBreakInvisNoSneak();

	if (damage > 0)
		return true;

	else
		return false;
}

//used by complete heal and #heal
void Mob::Heal()
{
	SetMaxHP();
	SendHPUpdate();
}

void Client::Damage(Mob* other, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable, int8 buffslot, bool iBuffTic)
{
	if(dead || IsCorpse())
		return;

	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	if(spell_id!=0 && spell_id != SPELL_UNKNOWN && other && damage > 0)
	{
		if(other->IsNPC() && !other->IsPet())
		{
			float npcspellscale = other->CastToNPC()->GetSpellScale();
			damage = ((float)damage * npcspellscale) / (float)100;
		}
	}

	// Reduce PVP damage. Don't do PvP mitigation if the caster is damaging themself or it's from a DS.
	bool FromDamageShield = (attack_skill == EQ::skills::SkillAbjuration);
	if (!FromDamageShield && other && other->IsClient() && damage > 0)
	{
		if (spell_id != SPELL_UNKNOWN)
		{
			/*
			int ruleDmg = RuleI(Combat, PvPSpellDmgPct);
			if (ruleDmg < 1)
				ruleDmg = 62;

			// lower level spells are less reduced than higher level spells
			// this scales PvP damage from 91% at level 1 to 62% at level 50
			PvPMitigation = 91 - spells[spell_id].classes[other->GetClass()-1] * 58 / 100;

			if (PvPMitigation < ruleDmg)
				PvPMitigation = ruleDmg;
			*/

			// this spell mitigation part is from a client decompile
			if (IsValidSpell(spell_id) && (spells[spell_id].goodEffect == 0 || IsLichSpell(spell_id)))
			{
				int caster_class = other->GetClass();
				float mitigation;

				if (caster_class == Class::Bard || caster_class == Class::Paladin || caster_class == Class::Ranger || caster_class == Class::ShadowKnight)
				{
					if (spell_id != SPELL_IMP_HARM_TOUCH && spell_id != SPELL_HARM_TOUCH)
					{
						mitigation = 0.80000001f;
					}
					else
					{
						mitigation = 0.68000001f;
					}
				}
				else
				{
					int spell_level = 39;
					if (caster_class <= Class::PLAYER_CLASS_COUNT)
					{
						spell_level = spells[spell_id].classes[caster_class - 1];
					}
					if (spell_level < 14)
					{
						mitigation = 0.88f;
					}
					else if (spell_level < 24)
					{
						mitigation = 0.77999997f;
					}
					else if (spell_level < 39)
					{
						mitigation = 0.68000001f;
					}
					else
					{
						mitigation = 0.63f;
					}
				}
				
				// Reintroduce spell dampening for self, but only for lich spells at the correct value.
				if (other == this)
				{
					mitigation = 1.0f;
					if (RuleB(Quarm, LichDamageMitigation) && IsLichSpell(spell_id))
					{
						mitigation = 0.68000001f;
						Log(Logs::Detail, Logs::Spells, "%s is getting %.2f lich mitigation for %s in slot %d. Was %d damage", GetName(), mitigation, GetSpellName(spell_id), buffslot, damage);
					}
				}
				
				damage = (int32)((double)damage * mitigation);
				if (damage < 1)
				{
					damage = 1;
				}
			}
		}
		else if (attack_skill == EQ::skills::SkillArchery)
		{
			if (RuleI(Combat, PvPArcheryDmgPct) > 0)
			{
				int PvPMitigation = RuleI(Combat, PvPArcheryDmgPct);
				damage = (damage * PvPMitigation) / 100;
			}
		}
		else  // melee PvP mitigation
		{
			if (RuleI(Combat, PvPMeleeDmgPct) > 0)
			{
				int PvPMitigation = RuleI(Combat, PvPMeleeDmgPct);
				damage = (damage * PvPMitigation) / 100;
			}
		}
	}

	// 3 second flee stun check.  NPCs can stun players who are running away from them.  10% chance on hit
	if (spell_id == SPELL_UNKNOWN && damage > 0 && other && other->IsNPC() && !other->IsPet() && other->GetLevel() > 4 && EQ::skills::IsMeleeWeaponSkill(attack_skill))
	{
		if (animation > 0 && zone->random.Roll(10) && other->BehindMob(this, other->GetX(), other->GetY()))
		{
			int stun_resist = aabonuses.StunResist;

			if (!stun_resist || zone->random.Roll(100 - stun_resist))
			{
				if (CombatRange(other, 0.0f, false, true)) {
					Log(Logs::Detail, Logs::Combat, "3 second flee stun sucessful");
					Stun(3000, other);
				}
			}
			else
			{
				Log(Logs::Detail, Logs::Combat, "Stun Resisted. %d chance.", stun_resist);
				Message_StringID(Chat::DefaultText, AVOID_STUN);
			}
		}
	}


	if(!ClientFinishedLoading())
		damage = DMG_INVUL;

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
}

void Mob::DamageCommand(Mob* other, int32 damage, bool skipaggro, uint16 spell_id, EQ::skills::SkillType attack_skill)
{
	if(!skipaggro)
		AddToHateList(other, 0, damage, false, false);

	if (damage > 0)
	{
		SetHP(GetHP() - damage);
		if (IsClient())
			CastToClient()->CalcAGI(); // AGI depends on current hp (near death)
	}

	if (HasDied()) 
	{
		SetHP(-100);
	}

	if (IsNPC())
	{
		total_damage += damage;
		gm_damage += damage;
	}

	if (GetHP() < GetMaxHP())
	{
		bool hideme = other && other->IsClient() && other->CastToClient()->GMHideMe();
		bool sendtoself = true;
		if (spell_id == SPELL_UNKNOWN && other != this && !hideme)
		{
			sendtoself = false;
		}

		SendHPUpdate(true, sendtoself);
	}

	if (!HasDied())
	{
		GenerateDamagePackets(other, false, damage, spell_id, attack_skill, true);
	}
	else
	{
		Death(other, damage, spell_id, attack_skill);
	}
}

void Mob::AggroPet(Mob* attacker)
{
	/**
	 * Pets should always assist if someone is trying to attack the master
	 * Uneless Pet hold is activated
	 */
	if(attacker) {
		Mob *pet = GetPet();
		if (pet && !pet->IsFamiliar() && !pet->GetSpecialAbility(SpecialAbility::AggroImmunity) && attacker && attacker != this && !attacker->IsCorpse() && !attacker->IsUnTargetable())
		{
			if (pet->hate_list.IsOnHateList(attacker))
				return;

			if (attacker->IsClient() && attacker->CastToClient()->IsFeigned())
				return;

			
			Log(Logs::Detail, Logs::Combat, "Sending pet %s into battle due to attack.", pet->GetName());
			pet->AddToHateList(attacker, 1);
			if (!pet->IsHeld()) {
				pet->SetTarget(attacker);
				Message_StringID(Chat::White, PET_ATTACKING, pet->GetCleanName(), attacker->GetCleanName());
			}
		}
	}
}

bool Client::Death(Mob* killerMob, int32 damage, uint16 spell, EQ::skills::SkillType attack_skill, uint8 killedby, bool bufftic)
{
	if(!ClientFinishedLoading())
		return false;

	if(dead)
		return false;	//cant die more than once...

	zone_mode = ZoneUnsolicited;

	if(!spell)
		spell = SPELL_UNKNOWN;

	std::string export_string = fmt::format(
		"{} {} {} {}",
		killerMob ? killerMob->GetID() : 0,
		damage,
		spell,
		static_cast<int>(attack_skill)
	);	if(parse->EventPlayer(EVENT_DEATH, this, export_string, 0) != 0) {
		if(GetHP() < 0) {
			SetHP(0);
		}
		return false;
	}

	if(killerMob && killerMob->IsClient() && (spell != SPELL_UNKNOWN) && damage > 0) {
		char val1[20]={0};
	}

	// We're in the middle of a trade and are not leaving a corpse.
	if (trade && (GetGM() || !RuleB(Character, LeaveCorpses) || IsHardcore() && GetLevel() < RuleI(Quarm, HardcoreDeathLevel) || GetLevel() < RuleI(Character, DeathItemLossLevel)))
	{
		Mob *with = trade->With();
		if (with && with->IsClient()) 
		{
			Log(Logs::General, Logs::Trading, "Canceling trade with %s due to death.", with->GetName());
			FinishTrade(this);
			trade->Reset();

			with->CastToClient()->FinishTrade(with);
			with->trade->Reset();
		}
	}

	int exploss = 0;
	Log(Logs::General, Logs::Death, "Fatal blow dealt by %s with %d damage, spell %d, skill %d", killerMob ? killerMob->GetName() : "Unknown", damage, spell, attack_skill);

	/*
		#1: Send death packet to everyone. Final damage packet is combined with death packet.
	*/

	SendLogoutPackets();
	SendRealPosition();

	if (killerMob && killerMob->IsClient() && killerMob == this)
	{
		killedby = Killed_Self;
	}

	GenerateDeathPackets(killerMob, damage, spell, attack_skill, bufftic, killedby);

	/*
		#2: figure out things that affect the player dying and mark them dead
	*/

	if(GetPet() && GetPet()->IsCharmedPet())
	{
		Log(Logs::General, Logs::Death, "%s has died. Fading charm on pet.", GetName());
		GetPet()->BuffFadeByEffect(SE_Charm);
	}

	InterruptSpell();
	SetPet(0);
	SetHorseId(0);
	dead = true;

	ClearTimersOnDeath();
	if (IsHardcore())
	{
		if (GetLevel() >= RuleI(Quarm, HardcoreDeathBroadcastLevel))
		{
			std::string rulesets = "Hardcore";

			if (IsSoloOnly())
			{
				rulesets.append(" Solo Self Found");
			} 
			else if (IsSelfFoundClassic())
			{
				rulesets.append(" Classic Self Found");
			}
			else if (IsSelfFoundFlex())
			{
				rulesets.append(" Self Found");
			}

			if (killerMob)
			{
				worldserver.SendEmoteMessage(0, 0, 15, "[%s] %s has died to %s! They were level %i.", rulesets.c_str(), GetCleanName(), killerMob->GetCleanName(), GetLevel());
			}
			else
			{
				worldserver.SendEmoteMessage(0, 0, 15, "[%s] %s has died! They were level %i.", rulesets.c_str(), GetCleanName(), GetLevel());
			}
		}
	}
	bool dueling = IsDueling();
	if (killerMob != nullptr)
	{
		if (killerMob->IsNPC()) 
		{
			parse->EventNPC(EVENT_SLAY, killerMob->CastToNPC(), this, "", 0);

			if (killerMob && zone->GetGuildID() == 1)
			{
				std::string pvpKilledGuildName = GetGuildName();
				entity_list.Message(0, 15, "[PVP] %s of <%s> has died to %s in combat!", GetCleanName(), pvpKilledGuildName.empty() ? " " : pvpKilledGuildName.c_str(), killerMob->GetCleanName());
			}

			killedby = Killed_NPC;

			uint32 emoteid = killerMob->GetEmoteID();
			if(emoteid != 0)
				killerMob->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::KilledPC,emoteid,this);
		}

		else if (killerMob->IsClient() && zone->GetGuildID() == 1)
		{
			if (killerMob != this)
			{
				killedby = Killed_PVP;
				std::string pvpKilledGuildName = GetGuildName();
				std::string pvpKillerGuildName = killerMob->CastToClient()->GetGuildName();
				entity_list.Message(0, 15, "[PVP] %s of <%s> has been killed in combat by %s of <%s>!", GetCleanName(), pvpKilledGuildName.empty() ? " " : pvpKilledGuildName.c_str(), killerMob->GetCleanName(), pvpKillerGuildName.empty() ? " " : pvpKillerGuildName.c_str());
			}
			else
			{
				killedby = Killed_Self;
				std::string pvpKilledGuildName = GetGuildName();
				entity_list.Message(0, 15, "[PVP] %s of <%s> has unalived themselves!", GetCleanName(), pvpKilledGuildName.empty() ? " " : pvpKilledGuildName.c_str());
			}
		}
		else if (killerMob->IsClient() && (dueling || killerMob->CastToClient()->IsDueling())) 
		{
			SetDueling(false);
			SetDuelTarget(0);
			if (killerMob->IsClient() && killerMob->CastToClient()->IsDueling() && killerMob->CastToClient()->GetDuelTarget() == GetID())
			{
				//if duel opponent killed us...
				killerMob->CastToClient()->SetDueling(false);
				killerMob->CastToClient()->SetDuelTarget(0);
				entity_list.DuelMessage(killerMob, this, false);

				killedby = Killed_DUEL;

			}
			else 
			{
				//otherwise, we just died, end the duel.
				Mob* who = entity_list.GetMob(GetDuelTarget());
				if (who && who->IsClient()) {
					who->CastToClient()->SetDueling(false);
					who->CastToClient()->SetDuelTarget(0);
				}
			}
		}
		else if (killerMob->IsClient())
		{
			if(killerMob != this)
				killedby = Killed_PVP;
		}
	}

	// Sometimes during a duel, we can die due to Pain and Suffering or sometimes we tell the client we've killed ourself.
	// In both cases, this incorrectly affects XP loss and the corpse. This is a workaround, until the cause of the unknown
	// damage is found. Killedby has already been set for all cases by this point, so if it is still 0 something is wrong.
	if (killedby == Killed_Other)
	{
		bool pvparea = false;
		glm::vec3 mypos(GetX(), GetY(), GetZ());
		if (zone->watermap)
		{
			pvparea = zone->watermap->InPVP(mypos);
		}

		if (dueling)
		{
			SetDueling(false);
			SetDuelTarget(0);
			killedby = Killed_DUEL;
			Log(Logs::General, Logs::Death, "%s is in a duel and killedby is 0. This is likely an error due to pain and suffering, setting killedby to 3.", GetName());
		}
		else if(pvparea || zone->GetGuildID() == 1)
		{
			killedby = Killed_PVP;
			if (zone->GetGuildID() == 1)
			{
				std::string pvpKilledGuildName = GetGuildName();
				entity_list.Message(0, 15, "[PVP] %s of <%s> has unalived themselves!", GetCleanName(), pvpKilledGuildName.empty() ? " " : pvpKilledGuildName.c_str());
			}
			Log(Logs::General, Logs::Death, "%s is in a PVP situation and killedby is 0. This is likely an error due to pain and suffering, setting killedby to 4.", GetName());
		}
		else
		{
			Log(Logs::General, Logs::Death, "%s was killed by an unknown entity. This is possibly due to the pain and suffering bug.", GetName());
		}
	}

	entity_list.RemoveFromTargets(this);
	hate_list.RemoveEnt(this);
	EndShield();

	//remove ourself from all proximities
	ClearAllProximities();

	/*
		#3: exp loss and corpse generation
	*/
	if(IsClient())
		CastToClient()->GetExpLoss(killerMob, spell, exploss, killedby);

	if (killerMob && !killerMob->IsClient() && !killerMob->IsPlayerOwned())
		SetMana(GetMaxMana());

	uint32 previous_level = GetLevel();

	bool LeftCorpse = false;

	// now we apply the exp loss, unmem their spells, and make a corpse
	// unless they're a GM (or less than lvl 10
	if(!GetGM())
	{
		if(exploss > 0) {
			int32 newexp = GetEXP();
			if(exploss > newexp) {
				//lost more than we have... wtf..
				newexp = 1;
			} else {
				newexp -= exploss;
			}
			SetEXP(newexp, GetAAXP());
			//m_epp.perAA = 0;	//reset to no AA exp on death.
		}
		
		UnmemSpellAll(false);
		if((RuleB(Character, LeaveCorpses) && GetLevel() >= RuleI(Character, DeathItemLossLevel)) || RuleB(Character, LeaveNakedCorpses) || IsHardcore() && previous_level >= RuleI(Quarm, HardcoreDeathLevel))
		{

			// If we've died on a boat, make sure corpse falls overboard.
			if(GetBoatNPCID() != 0)
			{
				if(zone->zonemap != nullptr)
				{
					glm::vec3 dest(GetX(), GetY(), GetZ());
					m_Position.z = zone->zonemap->FindBestZ(dest, nullptr);
					Log(Logs::General, Logs::Corpse, "Corpse was on a boat. Moving to %0.2f, %0.2f, %0.2f", GetX(), GetY(), m_Position.z);
				}
			}

			// creating the corpse takes the cash/items off the player too
			auto new_corpse = new Corpse(this, exploss, killedby);

			std::string tmp;
			database.GetVariable("ServerType", tmp);
			if(tmp[0] == '1' && tmp[1] == '\0' && killerMob != nullptr && killerMob->IsClient()){
				database.GetVariable("PvPreward", tmp);
				int reward = atoi(tmp.c_str());
				if(reward==3){
					database.GetVariable("PvPitem", tmp);
					int pvpitem = atoi(tmp.c_str());
					if(pvpitem>0 && pvpitem<200000)
						new_corpse->SetPlayerKillItemID(pvpitem);
				}
				else if(reward==2)
					new_corpse->SetPlayerKillItemID(-1);
				else if(reward==1)
					new_corpse->SetPlayerKillItemID(1);
				else
					new_corpse->SetPlayerKillItemID(0);
				if(killerMob->CastToClient()->isgrouped) {
					Group* group = entity_list.GetGroupByClient(killerMob->CastToClient());
					if(group != 0)
					{
						for(int i=0;i<6;i++)
						{
							if(group->members[i] != nullptr)
							{
									new_corpse->AllowPlayerLoot(group->members[i]);
							}
							else if (group->membername[i][0])
							{
									if (m_EngagedClientNames.find(group->membername[i]) != m_EngagedClientNames.end())
										new_corpse->AllowPlayerLoot(group->members[i]);
							}
						}
					}
				}
			}

			entity_list.AddCorpse(new_corpse, GetID());
			SetID(0);
			SetCorpseID(new_corpse->GetID()); // save id for sending damage packets

			LeftCorpse = true;
		}

		BuffFadeNonPersistDeath();

	} else {
		BuffFadeDetrimental();
	}

	if (IsHardcore() && previous_level >= RuleI(Quarm, HardcoreDeathLevel))
	{

		// Delete the character on next character select retrieval, so it can be hidden from owned characters or wiped, depending on rules. Purge these periodically.
		uint64 death_timestamp = std::time(nullptr);
		SetHardcoreDeathTimeStamp(death_timestamp);
		ClearPlayerInfoAndGrantStartingItems(false);
	}

	/*
		Finally, send em home

		We change the mob variables, not pp directly, because Save() will copy
		from these and overwrite what we set in pp anyway
	*/
	if(isgrouped)
	{
		Group *g = GetGroup();
		if(g)
			g->MemberZoned(this);
	}

	Raid* r = entity_list.GetRaidByClient(this);

	if(r)
		r->MemberZoned(this);

	dead_timer.Start(5000, true);

	if (!IsLD())
	{
		m_pp.zone_id = m_pp.binds[0].zoneId;
		m_epp.zone_guild_id = GUILD_NONE;
		database.MoveCharacterToZone(this->CharacterID(), m_pp.zone_id);
	}
	else
	{
		m_pp.zone_id = database.MoveCharacterToBind(CharacterID());
		m_epp.zone_guild_id = GUILD_NONE;
		glm::vec4 bindpts (m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, m_pp.binds[0].heading);
		m_Position = bindpts;
	}

	m_pp.intoxication = 0;
	m_pp.air_remaining = CalculateLungCapacity();
	SetHP(-100);
	m_pp.cur_hp = -100;

	Save();

	if (!IsLD() && zonesummon_id == 0)
	{
		GoToDeath();
	}

	/* QS: PlayerLogDeaths */
	if (RuleB(QueryServ, PlayerLogDeaths))
	{
		std::string killer_name = "NONE";
		if (killerMob)
		{
			killer_name = killerMob->GetCleanName();
		}
		QServ->QSDeathBy(this->CharacterID(), this->GetZoneID(), killer_name, spell, damage, killedby);
	}

	parse->EventPlayer(EVENT_DEATH_COMPLETE, this, export_string, 0);

	return true;
}

bool Client::CheckDeath()
{
	if (!ClientFinishedLoading())
		return false;

	if (dead)
		return false;	//cant die more than once...

	return true;
}

bool NPC::Attack(Mob* other, int hand, int damagePct)
{   
	if (!other) {
		SetTarget(nullptr);
		Log(Logs::General, Logs::Error, "A null Mob object was passed to NPC::Attack() for evaluation!");
		return false;
	}

	if (hand != EQ::invslot::slotSecondary)
		hand = EQ::invslot::slotPrimary;

	if (other->HasDied())
		return false;

	if(IsPet() && GetOwner()->IsClient() && other->IsMezzed()) {
		RemoveFromHateList(other);
		GetOwner()->Message_StringID(Chat::Yellow, CANNOT_WAKE, GetCleanName(), other->GetCleanName());
		return false;
	}
	int damage = 1;

	if(!GetTarget())
		SetTarget(other);

	//Check that we can attack before we calc heading and face our target
	if (!IsAttackAllowed(other)) {
		if (this->GetOwnerID())
			this->Say_StringID(NOT_LEGAL_TARGET);
		if(other) {
			RemoveFromHateList(other);
			Log(Logs::Detail, Logs::Combat, "I am not allowed to attack %s", other->GetName());
		}
		return false;
	}
	if (!IsFeared() && !IsRooted())
		FaceTarget(GetTarget());

	EQ::skills::SkillType skillinuse = EQ::skills::SkillHandtoHand;
	if (hand == EQ::invslot::slotSecondary)
		skillinuse = static_cast<EQ::skills::SkillType>(GetSecSkill());
	else
		skillinuse = static_cast<EQ::skills::SkillType>(GetPrimSkill());

	//figure out what weapon they are using, if any
	const EQ::ItemData* weapon = nullptr;
	if (equipment[hand] > 0)
		weapon = database.GetItem(equipment[hand]);

	//We don't factor much from the weapon into the attack.
	//Just the skill type so it doesn't look silly using punching animations and stuff while wielding weapons
	if(weapon) {
		Log(Logs::Detail, Logs::Combat, "Attacking with weapon: %s (%d) (too bad im not using it for much)", weapon->Name, weapon->ID);

		if(hand == EQ::invslot::slotSecondary && weapon->ItemType == EQ::item::ItemTypeShield){
			Log(Logs::Detail, Logs::Combat, "Attack with shield canceled.");
			return false;
		}

		switch(weapon->ItemType){
			case EQ::item::ItemType1HSlash:
				skillinuse = EQ::skills::Skill1HSlashing;
				break;
			case EQ::item::ItemType2HSlash:
				skillinuse = EQ::skills::Skill2HSlashing;
				break;
			case EQ::item::ItemType1HPiercing:
				//skillinuse = Skill1HPiercing;
				//break;
			case EQ::item::ItemType2HPiercing:
				skillinuse = EQ::skills::Skill1HPiercing; // change to Skill2HPiercing once activated
				break;
			case EQ::item::ItemType1HBlunt:
				skillinuse = EQ::skills::Skill1HBlunt;
				break;
			case EQ::item::ItemType2HBlunt:
				skillinuse = EQ::skills::Skill2HBlunt;
				break;
			case EQ::item::ItemTypeBow:
				skillinuse = EQ::skills::SkillArchery;
				break;
			case EQ::item::ItemTypeLargeThrowing:
			case EQ::item::ItemTypeSmallThrowing:
				skillinuse = EQ::skills::SkillThrowing;
				break;
			default:
				skillinuse = EQ::skills::SkillHandtoHand;
				break;
		}
	}

	int baseDamage = GetBaseDamage(other, hand);
	int damageBonus = GetDamageBonus();
	int hate = baseDamage / 2;

	//do attack animation regardless of whether or not we can hit below
	int8 charges = 0;
	EQ::ItemInstance weapon_inst(weapon, charges);
	
	AttackAnimation(skillinuse, hand, &weapon_inst);

	// Remove this once Skill2HPiercing is activated
	//Work-around for there being no 2HP skill - We use 99 for the 2HB animation and 36 for pierce messages
	if(skillinuse == 99)
		skillinuse = static_cast<EQ::skills::SkillType>(36);

	if (other->IsImmuneToMelee(this, hand))
	{
		if (other->IsClient())
		{
			other->AvoidDamage(this, damage);				// Even if we are immune, we still want to check for ripsote, dodge, parry, and block skillups.
			other->AvoidanceCheck(this, skillinuse);		// this is just for the Defense skill raise check.  Only misses should raise the skill
															// I'm assuming Sony still did avoidance checks on invuln swings, else DA might raise
		}													// the Defense skill too much

		damage = DMG_INVUL;
	}
	else
	{
		// check avoidance skills
		other->AvoidDamage(this, damage);

		if (damage > 0)
		{
			// check avoidance AC
			if (!other->AvoidanceCheck(this, skillinuse))
			{
				damage = DMG_MISS;
			}
		}
	}

	if (damage > 0)
	{
		damage = damageBonus + CalcMeleeDamage(other, baseDamage, skillinuse);

		if (damagePct <= 0)
			damagePct = 100;
		damage = damage * damagePct / 100;

		other->TryShielderDamage(this, damage, skillinuse);	// warrior /shield

		Log(Logs::Detail, Logs::Combat, "Final damage against %s: %d", other->GetName(), damage);
	}

	Log(Logs::Detail, Logs::Combat, "Generating hate %d towards %s", hate, GetName());

	other->AddToHateList(this, hate);

	if(GetHP() > 0 && !other->HasDied())
	{
		other->Damage(this, damage, SPELL_UNKNOWN, skillinuse, false); // Not avoidable client already had thier chance to Avoid
	} else
		return false;

	if (HasDied()) //killed by damage shield ect
		return false;

	MeleeLifeTap(damage);

	CommonBreakInvisible();

	//I doubt this works...
	if (!GetTarget())
		return true; //We killed them

	if(!other->HasDied())
	{
		if (damage > 0)			// NPCs only proc innate procs on a hit
			TryInnateProc(other);
	}

	// now check ripostes
	if (damage == DMG_RIPOSTE) { // riposting
		DoRiposte(other);
	}

	if (damage > 0)
		return true;

	else
		return false;
}

void NPC::Damage(Mob* other, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable, int8 buffslot, bool iBuffTic) {
	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	//handle EVENT_ATTACK. Resets after we have not been attacked for 12 seconds
	if(attacked_timer.Check())
	{
		Log(Logs::Detail, Logs::Combat, "Triggering EVENT_ATTACK due to attack by %s", other->GetName());
		parse->EventNPC(EVENT_ATTACK, this, other, "", 0);
	}

	attacked_timer.Start(CombatEventTimer_expire);

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
}

// killerMob is the actual Mob who dealt the deathblow.
bool NPC::Death(Mob* killerMob, int32 damage, uint16 spell, EQ::skills::SkillType attack_skill, uint8 killedby, bool bufftic) 
{
	if (zone->IsIdling())
	{
		IdleDeath(killerMob);
		return true;
	}

	uint16 OrigEntID = this->GetID();
	// oos is the non-pet Mob who dealt the deathblow. 
	Mob *oos = nullptr;
	bool skip_corpse_checks = false;
	bool ismerchant = class_ == Class::Merchant || MerchantType > 1;
	bool player_damaged = ds_damage + npc_damage < total_damage;
	bool corpse = false;
	bool xp = false;
	bool faction = false;

	if (MerchantType == 1)
	{
		ismerchant = false;
	}

	if(killerMob) 
	{
		Log(Logs::General, Logs::Death, "Fatal blow dealt by %s with %d damage, spell %d, skill %d", killerMob->GetName(), damage, spell, attack_skill);

		oos = killerMob->GetOwnerOrSelf();

		std::string export_string = fmt::format(
			"{} {} {} {}",
			killerMob ? killerMob->GetID() : 0,
			damage,
			spell,
			static_cast<int>(attack_skill)
		);		
		if(parse->EventNPC(EVENT_DEATH, this, oos, export_string, 0) != 0) {
			if(GetHP() < 0) {
				SetHP(0);
			}
			return false;
		}

		if (oos)
		{
			if (oos->IsClient() && GetSpecialAbility(SpecialAbility::PCDeathblowCorpse) && !ismerchant)
			{
				skip_corpse_checks = true;
				Log(Logs::Detail, Logs::Death, "Deathblow dealt by %s, skipping all corpse checks for %s...", oos->GetName(), GetName());
			}

			if (oos->IsNPC())
			{
				parse->EventNPC(EVENT_NPC_SLAY, oos->CastToNPC(), this, "", 0);

				uint32 emoteid = oos->GetEmoteID();
				if (emoteid != 0)
					oos->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::KilledNPC, emoteid, this);
			}
		}
	} 
	else 
	{
		std::string export_string = fmt::format(
			"{} {} {} {}",
			killerMob ? killerMob->GetID() : 0,
			damage,
			spell,
			static_cast<int>(attack_skill)
		);
		if(parse->EventNPC(EVENT_DEATH, this, nullptr, export_string, 0) != 0)
		{
			if(GetHP() < 0) 
			{
				SetHP(0);
			}
			return false;
		}
	}

	uint32 emoteid = this->GetEmoteID();
	if (emoteid != 0)
		this->DoNPCEmote(EQ::constants::EmoteEventTypes::OnDeath, emoteid, killerMob);

	/* Zone controller process EVENT_DEATH_ZONE (Death events) */
	if (entity_list.GetNPCByNPCTypeID(ZONE_CONTROLLER_NPC_ID) && this->GetNPCTypeID() != ZONE_CONTROLLER_NPC_ID)
	{
		std::string export_string = fmt::format(
			"{} {} {} {} {}",
			killerMob ? killerMob->GetID() : 0,
			damage,
			spell,
			static_cast<int>(attack_skill),
			this->GetNPCTypeID()
		);
		parse->EventNPC(EVENT_DEATH_ZONE, entity_list.GetNPCByNPCTypeID(ZONE_CONTROLLER_NPC_ID)->CastToNPC(), nullptr, export_string, 0);
	}

	SetHP(0);

	if (GetPet() && !GetPet()->IsCharmedPet())
	{
		GetPet()->SetPetType(petOrphan);
		GetPet()->SetOwnerID(0);
	}

	if(GetOwner())
	{
		GetOwner()->FadeVoiceGraft();
		if (GetOwner()->IsClient() && !IsCharmedPet())
			GetOwner()->CastToClient()->SavePetInfo(true);
	}

	if (GetSwarmOwner())
	{
		Mob* owner = entity_list.GetMobID(GetSwarmOwner());
		if (owner)
			owner->SetTempPetCount(owner->GetTempPetCount() - 1);
	}

	entity_list.RemoveFromTargets(this);
	EndShield();

	if(p_depop == true)
		return false;

	SendRealPosition();

	HasAISpellEffects = false;

	GenerateDeathPackets(killerMob, damage, spell, attack_skill, bufftic);

	if(respawn2) 
	{
		respawn2->DeathReset(true);
	}

	// Do faction hits to any player on the hatelist, so long as a player damaged us.
	if (GetNPCFactionID() > 0 && player_damaged && oos && !oos->IsNPC())
	{
		hate_list.DoFactionHits(GetNPCFactionID(), faction);
	}

	// Killer is whoever gets corpse rights. Corpse will be left if killer is a player.
	int32 dmg_amt = 0;
	Mob* killer = nullptr;
	// give_exp is whoever gets XP credit.
	uint8 force_level = RuleI(AlKabor, LevelCorpsesAlwaysSpawn);
	bool force_corpse = GetLevel() >= force_level;

	if (oos && oos->IsNPC() && !force_corpse)
	{
		// If the final blow was from a NPC that was not a client pet and the dead mob was below force_level, give the NPC credit so the corpse doesn't spawn.
		killer = oos;
		Log(Logs::Detail, Logs::Death, "A NPC got the deathblow. Giving credit to %s.", oos->GetName());
	}
	else if (IsZomm())
		killer = killerMob;
	else
	{
		killer = GetDamageTop(dmg_amt, true, force_corpse);	// returns the top damage dealer or a member of the top group/raid

		if (killer == nullptr)
		{
			Log(Logs::Detail, Logs::Death, "Killer of mob could not be determined.  This could indicate a problem");
		}
		else
		{
			Log(Logs::Detail, Logs::Death, "%s%s was chosen as the top damage killer with %d damage done to %s",
				killer->GetName(), killer->GetGroup() || killer->GetRaid() ? "'s group/raid " : "", dmg_amt, GetName());
		}
	}

	bool is_majority_ds_damage = (float)ds_damage - (float)ssf_ds_damage > (float)GetMaxHP() * 0.45f;
	bool is_majority_killer_dmg = (float)ssf_player_damage + (float)npc_damage > (float)GetMaxHP() * 0.45f;

	if(killer)
		LogDeathDetail("{} Before credit. solo_fte_credit = {}, ds damage: {}, is_majority_ds_damage {}, solo damage {}, is_majority_killer_dmg {}", killer->GetName(), solo_fte_charid, ds_damage, is_majority_ds_damage == true ? 1 : 0, ssf_player_damage, is_majority_killer_dmg == true ? 1 : 0);

	if (killer && killer->IsClient())
	{
		Log(Logs::Detail, Logs::Death, "In credit.");
		//give_exp_client is the player who gets XP credit.
		Client *give_exp_client = killer->CastToClient();
		// Make sure the dead NPC should give XP and the player is able to receive it (not a mule)
		if (IsNPC() && !killer->CastToClient()->IsMule() && !IsPlayerOwned() && (!GetSwarmInfo() || IsZomm()) && !ismerchant && player_damaged)
		{

			if (give_exp_client)
			{
				Raid* raid = give_exp_client->GetRaid();
				Group* group = give_exp_client->GetGroup();
				ChallengeRules::RuleParams ruleset = raid ? raid->GetRuleSetParams() : group ? group->GetRuleSetParams() : give_exp_client->GetRuleSetParams();

				if (!ruleset.IsFteRequired())
				{
					LogDeathDetail("{} will receive XP credit.", give_exp_client->GetName());

					// We hand out XP here.
					GiveExp(give_exp_client, xp);
				}
				else
				{
					bool is_raid_solo_fte_credit = raid ? raid->GetID() == solo_raid_fte : false;
					bool is_group_solo_fte_credit = group ? group->GetID() == solo_group_fte : false;
					bool is_solo_fte_credit = give_exp_client->CharacterID() == solo_fte_charid;

					if (is_raid_solo_fte_credit || is_group_solo_fte_credit || is_solo_fte_credit)
					{
						// If DS damage is from other players, don't give xp - Could be powerleveling if there's a bug in the buffing code
						// If Ds damage is from the group, it's okay to give xp - 
						if (!is_majority_ds_damage && is_majority_killer_dmg)
						{
							LogDeathDetail("{} will receive XP credit. solo_fte_credit = {}, ds damage: {}, is_majority_ds_damage {}, solo damage {}, is_majority_killer_dmg {}", give_exp_client->GetName(), solo_fte_charid, ds_damage, is_majority_ds_damage == true ? 1 : 0, ssf_player_damage, is_majority_killer_dmg == true ? 1 : 0);
							// We hand out XP here.
							GiveExp(give_exp_client, xp);
						}
						else
						{
							LogDeathDetail("{} will not receive XP credit (failed killer damage or ds damage check). solo_fte_credit = {}, ds damage: {}, is_majority_ds_damage {}, solo damage {}, is_majority_killer_dmg {}", give_exp_client->GetName(), solo_fte_charid, ds_damage, is_majority_ds_damage == true ? 1 : 0, ssf_player_damage, is_majority_killer_dmg == true ? 1 : 0);
						}
					}
					else
					{
						LogDeathDetail("{} will not receive XP credit. failed characterid/groupid/raidid check. solo_fte_credit = {}, ds damage: {}, is_majority_ds_damage {}, solo damage {}, is_majority_killer_dmg {}", give_exp_client->GetName(), solo_fte_charid, ds_damage, is_majority_ds_damage == true ? 1 : 0, ssf_player_damage, is_majority_killer_dmg == true ? 1 : 0);
					}
				}
			}
			else
			{
				LogDeathDetail("NPC checks failed. No XP for you.");
			}
		}
		else
		{
			LogDeathDetail("Give exp client checks failed. No XP for you.");
		}

		if (IsNPC() && (ismerchant || MerchantType == 1) && RuleB(Merchant, ClearTempList)) {
			database.DeleteMerchantTempList(GetNPCTypeID());
			zone->tmpmerchanttable[GetNPCTypeID()].clear();
			zone->tmpmerchanttable_ssf_purchase_limits[GetNPCTypeID()].clear();
		}
	}
	else
	{
		Log(Logs::Detail, Logs::Death, "killer is %s. No XP will be given.", killer ? "a NPC" : "null");
	}

	if (IsNPC() && zone && zone->GetGuildID() != GUILD_NONE)
	{
		AddAllClientsToEngagementRecords();
	}

	if (IsNPC() && HasEngageNotice() && killer && killer->IsClient() && zone && zone->GetGuildID() != GUILD_NONE)
	{
		Client* killerClient = killer->CastToClient();
		std::string guild_string = killerClient->GetGuildName();
		if (!guild_string.empty())
		{
			std::string kill_message = killerClient->GetCleanName();
			kill_message += " of <";
			kill_message += guild_string.c_str();
			kill_message += "> has killed ";
			kill_message += GetCleanName();
			kill_message += " in ";
			kill_message += zone->GetLongName();
			kill_message += "!";
			worldserver.SendChannelMessage("Druzzil_Ro", ChatChannel_Guild, zone->GetGuildID(), 0, 100, kill_message.c_str());
		}
	}

	if (skip_corpse_checks || GetSummonerID() || (killer && (killer->IsClient() || killer->IsPlayerOwned())))
	{
		// Make sure the dead NPC should leave a corpse.
		if (IsNPC() && (!GetSwarmInfo() || IsZomm()) && !ismerchant && GetPetType() != petHatelist && (player_damaged || skip_corpse_checks || GetSummonerID()))
		{
			// Here we create the corpse.
			DeleteInvalidQuestLoot();

			if (skip_corpse_checks)
			{
				killer = oos;
			}

			Log(Logs::Detail, Logs::Death, "Creating a corpse for %s", killer->GetName());

			CreateCorpse(killer, dmg_amt, corpse);

			if (IsZomm())
			{
				// Fade the Zomm buff here, or else we'll be attached to the corpse until it fades.
				uint32 zomm_owner_id = GetSwarmOwner();
				Mob* zomm_owner = entity_list.GetMob(zomm_owner_id);
				if (zomm_owner)
					zomm_owner->BuffFadeByEffect(SE_EyeOfZomm);
			}
		}
		else
		{
			Log(Logs::General, Logs::Death, "%s should not leave a corpse...", GetName());
		}
	}
	else
	{
		Log(Logs::Detail, Logs::Death, "Killer is NULL or is a NPC. No corpse will be left.");
	}

	
	hate_list.ReportDmgTotals(this, corpse, xp, faction, dmg_amt);

	BuffFadeAll();

	WipeHateList();

	p_depop = true;
	if(killerMob && killerMob->GetTarget() == this) //we can kill things without having them targeted
		killerMob->SetTarget(nullptr); //via AE effects and such..

	std::string export_string = fmt::format(
		"{} {} {} {}",
		killer ? killer->GetID() : 0,
		dmg_amt,
		spell,
		static_cast<int>(attack_skill)
	);
	parse->EventNPC(EVENT_DEATH_COMPLETE, this, oos, export_string, 0);

	return true;
}

void NPC::IdleDeath(Mob* killerMob)
{
	Log(Logs::General, Logs::Death, "%s is empty and does not idle. Skipping most death checks...", zone->GetShortName());

	SetHP(0);

	if (GetPet() && !GetPet()->IsCharmedPet())
	{
		GetPet()->SetPetType(petOrphan);
		GetPet()->SetOwnerID(0);
	}

	if (GetOwner())
	{
		GetOwner()->FadeVoiceGraft();
	}

	if (GetSwarmOwner())
	{
		Mob* owner = entity_list.GetMobID(GetSwarmOwner());
		if (owner)
			owner->SetTempPetCount(owner->GetTempPetCount() - 1);
	}

	entity_list.RemoveFromTargets(this);
	EndShield();

	if (p_depop == true)
		return;

	HasAISpellEffects = false;

	if (respawn2)
	{
		respawn2->DeathReset(true);
	}

	BuffFadeAll();

	WipeHateList();

	p_depop = true;

	if (killerMob && killerMob->GetTarget() == this) //we can kill things without having them targeted
		killerMob->SetTarget(nullptr); //via AE effects and such..

	return;
}

void NPC::CreateCorpse(Mob* killer, int32 dmg_total, bool &corpse_bool)
{
	Client* clientKiller = nullptr;

	if (killer != 0)
	{
		if (killer->IsPlayerOwned())
			killer = killer->GetOwner();

		if (killer->IsClient() && !killer->CastToClient()->GetGM())
		{
			this->CheckMinMaxLevel(killer);
			clientKiller = killer->CastToClient();
		}
	}
	uint32 emoteid = this->GetEmoteID();
	bool is_client_pet = false;
	if (GetOwner() && GetOwner()->IsClient())
		is_client_pet = true;

	auto corpse = new Corpse(this, &m_loot_items, GetNPCTypeID(),
		level > 54 ? RuleI(NPC, MajorNPCCorpseDecayTimeMS) : RuleI(NPC, MinorNPCCorpseDecayTimeMS),
		is_client_pet);
	corpse_bool = true;
	entity_list.LimitRemoveNPC(this);
	entity_list.AddCorpse(corpse, GetID());

	entity_list.RemoveNPC(GetID());
	this->SetID(0);
	this->SetCorpseID(corpse->GetID()); // save id for sending damage packets
	if (killer != 0 && emoteid != 0)
		corpse->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::AfterDeath, emoteid, killer);

	if (killer != 0 && killer->IsClient())
	{
		
		Raid* raid = killer->GetRaid();
		Group* group = killer->GetGroup();
		ChallengeRules::RuleParams ruleset = raid ? raid->GetRuleSetParams() : group ? group->GetRuleSetParams() : killer->CastToClient()->GetRuleSetParams();

		bool is_solo_fte_charid = solo_fte_charid == killer->CastToClient()->CharacterID();
		bool is_raid_solo_fte_credit = raid ? raid->GetID() == CastToNPC()->solo_raid_fte : false;
		bool is_group_solo_fte_credit = group ? group->GetID() == CastToNPC()->solo_group_fte : false;
		bool is_majority_ds_damage = (float)ds_damage - (float)ssf_ds_damage > (float)GetMaxHP() * 0.45f;
		bool is_majority_killer_dmg = (float)ssf_player_damage + (float)npc_damage > (float)GetMaxHP() * 0.45f;

		bool sf_solo_credit = is_solo_fte_charid && !is_raid_solo_fte_credit && !is_group_solo_fte_credit;
		bool sf_group_credit = is_group_solo_fte_credit && !is_majority_ds_damage && is_majority_killer_dmg;
		bool sf_raid_credit = is_raid_solo_fte_credit && !is_majority_ds_damage && is_majority_killer_dmg;

		{
			if (raid == nullptr && group == nullptr) {
				corpse->AllowPlayerLoot(killer);
				if (sf_solo_credit || !killer->CastToClient()->IsFteRequired()) {
					corpse->AddKillCredit(killer->GetCleanName(), killer->CastToClient()->IsSelfFoundAny());
				}
			}
			if (raid == nullptr && group)
			{
				{
					for (int i = 0; i < MAX_GROUP_MEMBERS; i++)
					{
						if (group->members[i] != nullptr)
						{
							corpse->AllowPlayerLoot(group->members[i]);
							bool kill_credit = group->members[i]->CastToClient()->CanGetLootCreditWith(ruleset, sf_group_credit);
							if (kill_credit) {
								corpse->AddKillCredit(group->members[i]->CastToClient()->GetCleanName(), group->members[i]->CastToClient()->IsSelfFoundAny());
							}
						}
						else
						{
							auto record = m_EngagedClientNames.find(group->membername[i]);
							if (record != m_EngagedClientNames.end()) {
								corpse->AllowPlayerLoot(group->membername[i]);
								bool kill_credit = record->second.CanGetLootCreditWith(ruleset, sf_group_credit);
								if (kill_credit) {
									corpse->AddKillCredit(group->membername[i], record->second.IsSelfFoundAny());
								}
							}
						}
					}
				}
			}
			else
			{
				Raid* r = raid;
				if (r) {
					r->GetRaidDetails();
					r->LearnMembers();
					r->VerifyRaid();
					
					for (auto& sf_fte_name : this->sf_fte_list) {
						corpse->AddKillCredit(sf_fte_name, true);
					}

					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (!r->members[x].membername[0])
							continue;

						bool add_looter = false;
						switch (r->GetLootType())
						{
						case 0:
						case 1:
							add_looter = r->members[x].IsRaidLeader;
							break;
						case 2:
							add_looter = (r->members[x].IsRaidLeader || r->members[x].IsGroupLeader);
							break;
						case 3:
							add_looter = (r->members[x].IsRaidLeader || r->members[x].IsLooter);
							break;
						case 4:
							add_looter = true;
							break;
						}
						if (add_looter)
						{
							corpse->AllowPlayerLoot(r->members[x].membername);
						}
						if (r->members[x].member)
						{
							bool kill_credit = r->members[x].member->CanGetLootCreditWith(ruleset, sf_raid_credit);
							if (kill_credit) {
								corpse->AddKillCredit(r->members[x].member->GetCleanName(), r->members[x].member->IsSelfFoundAny());
							}
						}
						else if (r->members[x].membername[0])
						{
							auto record = m_EngagedClientNames.find(r->members[x].membername);
							if (record != m_EngagedClientNames.end()) {
								bool kill_credit = record->second.CanGetLootCreditWith(ruleset, sf_raid_credit);
								if (kill_credit) {
									corpse->AddKillCredit(r->members[x].membername, record->second.IsSelfFoundAny());
								}
							}
						}
					}
				}
			}
		}
	}

	if (corpse && clientKiller)
	{
		corpse->ProcessLootLockouts(clientKiller, this);
	}
}

void NPC::GiveExp(Client* give_exp_client, bool &xp)
{
	if (hate_list.KillerIsNotInitialEngager(give_exp_client)) {
		hate_list.LogInitialEngageIdResult(give_exp_client);
	}

	Group *kg = entity_list.GetGroupByClient(give_exp_client);
	Raid *kr = entity_list.GetRaidByClient(give_exp_client);

	int32 finalxp = static_cast<int32>(GetBaseEXP());

	if (finalxp > 0)
		xp = true;

	bool always_log = GetLevel() >= RuleI(QueryServ, LevelAlwaysLogKills);
	if (kr)
	{
		if (kr->GetID() == raid_fte)
		{
			fte_charid = 0;
		}

		kr->SplitExp(finalxp, this);

		/* Send the EVENT_KILLED_MERIT event for all raid members */
		std::list<uint32>charids;
		for (int i = 0; i < MAX_RAID_MEMBERS; i++)
		{
			if (kr->members[i].member != nullptr && kr->members[i].member->IsClient() && IsOnHatelist(kr->members[i].member))
			{ // If Group Member is Client
				Client *c = kr->members[i].member;
				parse->EventNPC(EVENT_KILLED_MERIT, this, c, "killed", 0);

				charids.push_back(c->CharacterID());

				// In case the player joined the raid after engaging, or they wiped.
				if (fte_charid != 0 && fte_charid == c->CharacterID())
				{
					fte_charid = 0;
				}
			}
		}

		if (fte_charid != 0 || always_log)
		{
			if (fte_charid != 0 && !ValidateFTE())
			{
				fte_charid = 0;
			}

			Log(Logs::Detail, Logs::Death, "Raid kill %s a killsteal.", fte_charid != 0 ? "is" : "is not");
			// QueryServ Logging - Raid Kills
			QServ->QSNPCKills(GetNPCTypeID(), GetZoneID(), 2, charids, fte_charid);
		}
		charids.clear();
	}
	else if (give_exp_client->IsGrouped() && kg != nullptr)
	{
		kg->SplitExp(finalxp, this);

		if (kg->GetID() == group_fte)
		{
			fte_charid = 0;
		}

		/* Send the EVENT_KILLED_MERIT event and update kill tasks
		* for all group members */
		std::list<uint32>charids;
		for (int i = 0; i < MAX_GROUP_MEMBERS; i++)
		{
			if (kg->members[i] != nullptr && kg->members[i]->IsClient() && IsOnHatelist(kg->members[i]))
			{ // If Group Member is Client
				Client *c = kg->members[i]->CastToClient();
				parse->EventNPC(EVENT_KILLED_MERIT, this, c, "killed", 0);

				charids.push_back(c->CharacterID());

				// In case the player joined the group after engaging, or they wiped.
				if (fte_charid != 0 && fte_charid == c->CharacterID())
				{
					fte_charid = 0;
				}
			}
		}

		if (fte_charid != 0 || always_log)
		{
			if (fte_charid != 0 && !ValidateFTE())
			{
				fte_charid = 0;
			}

			Log(Logs::Detail, Logs::Death, "Group kill %s a killsteal.", fte_charid != 0 ? "is" : "is not");
			// QueryServ Logging - Raid Kills
			QServ->QSNPCKills(GetNPCTypeID(), GetZoneID(), 2, charids, fte_charid);
		}
		charids.clear();
	}
	else
	{
		int conlevel = give_exp_client->GetLevelCon(GetLevel());
		if (conlevel != CON_GREEN || IsZomm())
		{
			give_exp_client->AddEXP((finalxp), conlevel, this);
		}
		else
		{
			xp = false;
		}
		/* Send the EVENT_KILLED_MERIT event */
		parse->EventNPC(EVENT_KILLED_MERIT, this, give_exp_client, "killed", 0);

		// QueryServ Logging - Solo
		std::list<uint32>charids;
		Client *c = give_exp_client;
		charids.push_back(c->CharacterID());

		if (fte_charid == c->CharacterID())
		{
			fte_charid = 0;
		}
		
		if (fte_charid != 0 || always_log)
		{
			if (fte_charid != 0 && !ValidateFTE())
			{
				fte_charid = 0;
			}

			Log(Logs::Detail, Logs::Death, "Solo kill %s a killsteal.", fte_charid != 0 ? "is" : "is not");
			QServ->QSNPCKills(GetNPCTypeID(), GetZoneID(), 0, charids, fte_charid);
		}
		charids.clear();
	}
}

bool NPC::ValidateFTE()
{
	// If the player is not in the zone or not on the hatelist, don't mark as a kill steal.
	if (fte_charid != 0)
	{
		Client* fte = entity_list.GetClientByCharID(fte_charid);
		if (!fte)
		{
			return false;
		}
		else if (!IsOnHatelist(fte))
		{
			return false;
		}
	}

	return true;
}

void Mob::AddToHateList(Mob* other, int32 hate, int32 damage, bool bFrenzy, bool iBuffTic, bool addpet)
{
	if (!other || other == this || (other && other->IsTrap()) || !zone->CanDoCombat(this, other))
		return;

	Mob* owner = other->GetOwner();
	Mob* mypet = this->GetPet();
	Mob* myowner = this->GetOwner();
	Mob* targetmob = this->GetTarget();

	if(other){
		if (!iBuffTic)
			AddRampage(other);
	}

	if(IsClient() && !IsAIControlled())
		return;

	if(IsFamiliar() || GetSpecialAbility(SpecialAbility::AggroImmunity))
		return;

	if (other == myowner)
		return;

	if(other->GetSpecialAbility(SpecialAbility::BeingAggroImmunity))
		return;

	if (IsNPC() && other->IsClient() && zone && zone->GetGuildID() != GUILD_NONE && zone->GetGuildID() != 1)
	{
		if (CastToNPC()->loot_lockout_timer > 0)
		{

			auto lootLockoutItr = other->CastToClient()->loot_lockouts.find(npctype_id);
			if (lootLockoutItr != other->CastToClient()->loot_lockouts.end())
			{
				if (lootLockoutItr->second.HasLockout(Timer::GetTimeSeconds()))
				{
					other->CastToClient()->Message(Chat::Red, "You were locked out of %s. Sending you out.", GetCleanName() );
					other->CastToClient()->BootFromGuildInstance();
				}
			}
		}
	}

	if(GetSpecialAbility(SpecialAbility::TunnelVision)) {
		int tv_mod = GetSpecialAbilityParam(SpecialAbility::TunnelVision, 0);

		Mob *top = GetTarget();
		if(top && top != other) {
			if(tv_mod) {
				float tv = tv_mod / 100.0f;
				hate *= tv;
			} else {
				hate *= RuleR(Aggro, TunnelVisionAggroMod);
			}
		}
	}

	if(IsNPC() && CastToNPC()->IsUnderwaterOnly() && zone->HasWaterMap()) {
		bool in_liquid = zone->watermap->InLiquid(glm::vec3(other->GetPosition())) || zone->IsWaterZone(other->GetZ());
		if(!in_liquid) {
			return;
		}
	}
	// first add self

	// The damage on the hate list is used to award XP to the killer. This check is to prevent Killstealing.
	// e.g. Mob has 5000 hit points, Player A melees it down to 500 hp, Player B executes a headshot (10000 damage).
	// If we add 10000 damage, Player B would get the kill credit, so we only award damage credit to player B of the
	// amount of HP the mob had left.
	//
	if(damage > GetHP())
		damage = GetHP();

	if (IsNPC() && other->IsClient())
	{
		if (m_EngagedClientNames.find(other->GetCleanName()) == m_EngagedClientNames.end())
		{
			PlayerEngagementRecord record = PlayerEngagementRecord();
			record.isFlagged = false;
			record.lockout = LootLockout();
			strncpy(record.character_name, other->CastToClient()->GetCleanName(), 64);
			record.character_id = other->CastToClient()->CharacterID();
			record.character_level = other->CastToClient()->GetLevel();
			record.character_level2 = other->CastToClient()->GetLevel2();
			record.character_ruleset = other->CastToClient()->GetRuleSet();

			auto lootLockoutItr = other->CastToClient()->loot_lockouts.find(npctype_id);
			if (lootLockoutItr != other->CastToClient()->loot_lockouts.end())
			{
				memcpy(&record.lockout, &lootLockoutItr->second, sizeof(LootLockout));
			}
			m_EngagedClientNames.emplace(other->GetCleanName(), record);
		}
	}

	if (myowner)
	{
		if (myowner->IsClient())
		{
			Client* petowner = myowner->CastToClient();
			if (petowner && m_EngagedClientNames.find(petowner->GetCleanName()) == m_EngagedClientNames.end())
			{
				PlayerEngagementRecord record = PlayerEngagementRecord();
				record.isFlagged = false;
				record.lockout = LootLockout();
				strncpy(record.character_name, petowner->CastToClient()->GetCleanName(), 64);
				record.character_id = petowner->CastToClient()->CharacterID();
				record.character_level = petowner->CastToClient()->GetLevel();
				record.character_level2 = petowner->CastToClient()->GetLevel2();
				record.character_ruleset = petowner->CastToClient()->GetRuleSet();

				auto lootLockoutItr = petowner->CastToClient()->loot_lockouts.find(npctype_id);
				if (lootLockoutItr != petowner->CastToClient()->loot_lockouts.end())
				{
					memcpy(&record.lockout, &lootLockoutItr->second, sizeof(LootLockout));
					if (zone && zone->GetGuildID() != GUILD_NONE && zone->GetGuildID() != 1 && lootLockoutItr->second.HasLockout(Timer::GetTimeSeconds()))
					{
						petowner->CastToClient()->Message(Chat::Red, "You were locked out of %s. Sending you out.", GetCleanName());
						petowner->CastToClient()->BootFromGuildInstance();
					}
				}
				m_EngagedClientNames.emplace(petowner->GetCleanName(), record);
			}
		}
	}

	hate_list.Add(other, hate, damage, bFrenzy, !iBuffTic);
	if (IsNPC())
		CastToNPC()->SetAssisting(false);

	// then add pet owner if there's one
	if (owner)
	{
		if(!owner->GetSpecialAbility(SpecialAbility::AggroImmunity))
		{
			hate_list.Add(owner, 0, 0, false, !iBuffTic);
			if (!iBuffTic && owner->IsClient())
				AddRampage(owner);
		}
	}

	if (addpet && mypet) 
	{ // I have a pet, add other to it
		if (!mypet->IsFamiliar() && !mypet->GetSpecialAbility(SpecialAbility::AggroImmunity) && damage < GetMaxHP())
		{
			mypet->hate_list.Add(other, 0, 0, bFrenzy);
		}
	} 
	else if (myowner) 
	{ // I am a pet, add other to owner if it's NPC/LD
		if (myowner->IsAIControlled() && !myowner->GetSpecialAbility(SpecialAbility::AggroImmunity))
			myowner->hate_list.Add(other, 0, 0, bFrenzy);
	}

	if (other->GetTempPetCount())
		entity_list.AddTempPetsToHateList(other, this, bFrenzy);
}

// This is called from Damage() when 'this' is attacked by 'other.
// 'this' is the one being attacked
// 'other' is the attacker
// a damage shield causes damage (or healing) to whoever attacks the wearer.
// a damage shield on a spell is a negative value but on an item it's a positive value so add the spell value and subtract the item value to get the end ds value.
// a reverse ds causes damage to the wearer whenever it attack someone. 
void Mob::DamageShield(Mob* attacker, bool spell_ds) {
	
	if(!attacker || this == attacker || (attacker && attacker->GetID() == 0))
		return;

	int DS = 0;
	int rev_ds = 0;
	uint16 spellid = 0;

	if(!spell_ds)
	{
		DS = spellbonuses.DamageShield;
		rev_ds = attacker->spellbonuses.ReverseDamageShield;
		
		if(spellbonuses.DamageShieldSpellID != 0 && spellbonuses.DamageShieldSpellID != SPELL_UNKNOWN)
			spellid = spellbonuses.DamageShieldSpellID;
	}
	else 
	{
		DS = spellbonuses.SpellDamageShield;
		rev_ds = 0;
		// This ID returns "you are burned", seemed most appropriate for spell DS
		spellid = 2166;
	}

	if (DS == 0 && rev_ds == 0)
		return;

	//Normal damage shields. The spell is on the attackee, and they either damage or heal the attacker.
	if (DS > 0 && !spell_ds)
	{
		Log(Logs::Detail, Logs::Spells, "Applying Healing Shield of value %d to %s", DS, attacker->GetName());

		//we are healing the attacker...
		attacker->HealDamage(DS);
	}
	else if (DS < 0)
	{
		Log(Logs::Detail, Logs::Spells, "Applying Damage Shield of value %d to %s", DS, attacker->GetName());

		if (!spell_ds)
		{
			DS += aabonuses.DamageShield; //Live AA - coat of thistles. (negative value)
		}
		if (itembonuses.DamageShield < 0)
			DS += itembonuses.DamageShield;

		uint32 targetid = attacker->GetID();
		attacker->Damage(this, -DS, spellid, EQ::skills::SkillAbjuration, false);

		//we can assume there is a spell now
		auto outapp = new EQApplicationPacket(OP_Damage, sizeof(Damage_Struct));
		Damage_Struct* cds = (Damage_Struct*)outapp->pBuffer;
		cds->target = targetid;
		cds->source = GetID();
		cds->type = spellbonuses.DamageShieldType;
		cds->spellid = spellid;
		cds->damage = DS;

		if (attacker && !attacker->HasDied())
		{
			entity_list.QueueCloseClients(this, outapp, false, RuleI(Range, DamageMessages));
		}
		safe_delete(outapp);

		//DS causes offense skillups.
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillOffense, attacker, zone->skill_difficulty[EQ::skills::SkillOffense].difficulty, SKILLUP_SUCCESS, true);
	}

	//Reverse DS. This is basically a DS, but the spell is on the attacker, not the attackee
	if (rev_ds != 0)
	{
		uint16 rev_ds_spell_id = SPELL_UNKNOWN;

		if (spellbonuses.ReverseDamageShieldSpellID != 0 && spellbonuses.ReverseDamageShieldSpellID != SPELL_UNKNOWN)
			rev_ds_spell_id = spellbonuses.ReverseDamageShieldSpellID;


		Log(Logs::General, Logs::Spells, "Applying Reverse Damage Shield of value %d to %s", rev_ds, attacker->GetName());
		attacker->Damage(this, -rev_ds, rev_ds_spell_id, EQ::skills::SkillAbjuration, false);
	}
}

int Client::GetDamageBonus()
{
	if (GetLevel() < 28 || !IsWarriorClass())
		return 0;

	int delay = 1;
	int bonus = 1 + (GetLevel() - 28) / 3;

	EQ::ItemInstance* weaponInst = GetInv().GetItem(EQ::invslot::slotPrimary);
	const EQ::ItemData* weapon = nullptr;
	if (weaponInst)
		weapon = weaponInst->GetItem();

	if (!weapon)
		delay = GetHandToHandDelay();
	else
		delay = weapon->Delay;

	if ( weapon && (weapon->ItemType == EQ::item::ItemType2HSlash || weapon->ItemType == EQ::item::ItemType2HBlunt || weapon->ItemType == EQ::item::ItemType2HPiercing) )
	{
		if (delay <= 27)
			return bonus + 1;

		if (level > 29)
		{
			int level_bonus = (level - 30) / 5 + 1;
			if (level > 50)
			{
				level_bonus++;
				int level_bonus2 = level - 50;
				if (level > 67)
					level_bonus2 += 5;
				else if (level > 59)
					level_bonus2 += 4;
				else if (level > 58)
					level_bonus2 += 3;
				else if (level > 56)
					level_bonus2 += 2;
				else if (level > 54)
					level_bonus2++;
				level_bonus += level_bonus2 * delay / 40;
			}
			bonus += level_bonus;
		}
		if (delay >= 40)
		{
			int delay_bonus = (delay - 40) / 3 + 1;
			if (delay >= 45)
				delay_bonus += 2;
			else if (delay >= 43)
				delay_bonus++;
			bonus += delay_bonus;
		}
		return bonus;
	}
	return bonus;
}

int Client::GetHandToHandDamage()
{
	static uint8 mnk_dmg[] = { 99,
		4, 4, 4, 4, 5, 5, 5, 5, 5, 6,           // 1-10
		6, 6, 6, 6, 7, 7, 7, 7, 7, 8,           // 11-20
		8, 8, 8, 8, 9, 9, 9, 9, 9, 10,          // 21-30
		10, 10, 10, 10, 11, 11, 11, 11, 11, 12, // 31-40
		12, 12, 12, 12, 13, 13, 13, 13, 13, 14, // 41-50
		14, 14, 14, 14, 14, 14, 14, 14, 14, 14, // 51-60
		14, 14 };                                // 61-62
	static uint8 bst_dmg[] = { 99,
		4, 4, 4, 4, 4, 5, 5, 5, 5, 5,        // 1-10
		5, 6, 6, 6, 6, 6, 6, 7, 7, 7,        // 11-20
		7, 7, 7, 8, 8, 8, 8, 8, 8, 9,        // 21-30
		9, 9, 9, 9, 9, 10, 10, 10, 10, 10,   // 31-40
		10, 11, 11, 11, 11, 11, 11, 12, 12 }; // 41-49

	if (GetClass() == Class::Monk)
	{
		if (IsClient() && CastToClient()->GetItemIDAt(12) == 10652 && GetLevel() > 50)		// Celestial Fists, monk epic
			return 9;
		if (level > 62)
			return 15;
		return mnk_dmg[level];
	}
	else if (GetClass() == Class::Beastlord)
	{
		if (level > 49)
			return 13;
		return bst_dmg[level];
	}
	return 2;
}

int Client::GetHandToHandDelay()
{
	int delay = 35;
	static uint8 mnk_hum_delay[] = { 99,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 1-10
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 11-20
		35, 35, 35, 35, 35, 35, 35, 34, 34, 34, // 21-30
		34, 33, 33, 33, 33, 32, 32, 32, 32, 31, // 31-40
		31, 31, 31, 30, 30, 30, 30, 29, 29, 29, // 41-50
		29, 28, 28, 28, 28, 27, 27, 27, 27, 26, // 51-60
		24, 22 };								// 61-62
	static uint8 mnk_iks_delay[] = { 99,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 1-10
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 11-20
		35, 35, 35, 35, 35, 35, 35, 35, 35, 34, // 21-30
		34, 34, 34, 34, 34, 33, 33, 33, 33, 33, // 31-40
		33, 32, 32, 32, 32, 32, 32, 31, 31, 31, // 41-50
		31, 31, 31, 30, 30, 30, 30, 30, 30, 29, // 51-60
		25, 23 };								// 61-62
	static uint8 bst_delay[] = { 99,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 1-10
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 11-20
		35, 35, 35, 35, 35, 35, 35, 35, 34, 34, // 21-30
		34, 34, 34, 33, 33, 33, 33, 33, 32, 32, // 31-40
		32, 32, 32, 31, 31, 31, 31, 31, 30, 30, // 41-50
		30, 30, 30, 29, 29, 29, 29, 29, 28, 28, // 51-60
		28, 28, 28 };							// 61-63

	if (GetClass() == Class::Monk)
	{
		if (GetItemIDAt(12) == 10652 && GetLevel() > 50)		// Celestial Fists, monk epic
			return 16;
		
		if (GetLevel() > 62)
			return GetRace() == Race::Iksar ? 21 : 20;

		return GetRace() == Race::Iksar ? mnk_iks_delay[level] : mnk_hum_delay[level];
	}
	else if (GetClass() == Class::Beastlord)
	{
		if (GetLevel() > 63)
			return 27;
		return bst_delay[level];
	}
	return 35;
}

int32 Mob::ReduceDamage(int32 damage)
{
	if(damage <= 0)
		return damage;

	if(damage < 1)
		return DMG_RUNE;

	if (spellbonuses.MeleeRune[0] && spellbonuses.MeleeRune[1] >= 0)
		damage = RuneAbsorb(damage, SE_Rune);

	if(damage < 1)
		return DMG_RUNE;

	return(damage);
}

int32 Mob::AffectMagicalDamage(int32 damage, uint16 spell_id, const bool iBuffTic, Mob* attacker)
{
	if(damage <= 0)
		return damage;

	bool DisableSpellRune = false;
	int32 slot = -1;

	// If this is a DoT, use DoT Shielding...
	if(iBuffTic) {
		damage -= (damage * itembonuses.DoTShielding / 100);
	}

	// This must be a DD then so lets apply Spell Shielding and runes.
	else
	{
		// Reduce damage by the Spell Shielding first so that the runes don't take the raw damage.
		damage -= (damage * itembonuses.SpellShield / 100);

		// Do runes now.
		if (spellbonuses.MitigateSpellRune[0] && !DisableSpellRune){
			slot = spellbonuses.MitigateSpellRune[1];
			if(slot >= 0)
			{
				int damage_to_reduce = damage * spellbonuses.MitigateSpellRune[0] / 100;

				if (spellbonuses.MitigateSpellRune[2] && (damage_to_reduce > spellbonuses.MitigateSpellRune[2]))
					damage_to_reduce = spellbonuses.MitigateSpellRune[2];

				if(spellbonuses.MitigateSpellRune[3] && (damage_to_reduce >= buffs[slot].magic_rune))
				{
					Log(Logs::Detail, Logs::Spells, "Mob::ReduceDamage SE_MitigateSpellDamage %d damage negated, %d"
						" damage remaining, fading buff.", damage_to_reduce, buffs[slot].magic_rune);
					damage -= buffs[slot].magic_rune;
					BuffFadeBySlot(slot);
				}
				else
				{
					Log(Logs::Detail, Logs::Spells, "Mob::ReduceDamage SE_MitigateMeleeDamage %d damage negated, %d"
						" damage remaining.", damage_to_reduce, buffs[slot].magic_rune);

					if (spellbonuses.MitigateSpellRune[3])
						buffs[slot].magic_rune = (buffs[slot].magic_rune - damage_to_reduce);

					damage -= damage_to_reduce;
				}
			}
		}

		if(damage < 1)
			return 0;

		//Regular runes absorb spell damage (except dots) - Confirmed on live.
		if (spellbonuses.MeleeRune[0] && spellbonuses.MeleeRune[1] >= 0)
			damage = RuneAbsorb(damage, SE_Rune);

		if (spellbonuses.AbsorbMagicAtt[0] && spellbonuses.AbsorbMagicAtt[1] >= 0)
			damage = RuneAbsorb(damage, SE_AbsorbMagicAtt);
	}
	return damage;
}

bool Mob::HasProcs() const
{
	for (int i = 0; i < MAX_PROCS; i++)
		if (SpellProcs[i].spellID != SPELL_UNKNOWN)
			return true;
	return false;
}

// returns either chance in %, or the effective skill level which is essentially chance% * 5
// combat rolls should use the latter for accuracy.  Sony rolls against 500
int Mob::GetDoubleAttackChance(bool returnEffectiveSkill)
{
	int chance = GetSkill(EQ::skills::SkillDoubleAttack);

	if (chance > 0)
	{
		if (IsClient())
			chance += GetLevel();
		else if (GetLevel() > 35)
			chance += GetLevel();
	}

	// Bestial Frenzy/Harmonious Attack AA - grants double attacks for classes that otherwise do not get the skill
	if (aabonuses.GiveDoubleAttack)
		chance += aabonuses.GiveDoubleAttack * 5;

	// Knight's Advantage and Ferocity AAs; Double attack rate is 67% at 245 skill with Ferocity, so this is the only way it can be
	if (aabonuses.DoubleAttackChance)
		chance += chance * aabonuses.DoubleAttackChance / 100;

	if (returnEffectiveSkill)
		return chance;
	else
		return chance / 5;
}

bool Mob::CheckDoubleAttack()
{
	if (GetDoubleAttackChance(true) > zone->random.Int(0, 499))			// 1% per 5 skill
		return true;

	return false;
}

// returns either chance in %, or the effective skill level which is essentially chance% * 3.75
// combat rolls should use the latter for accuracy.  Sony rolls against 375
int Mob::GetDualWieldChance(bool returnEffectiveSkill)
{
	int chance = GetSkill(EQ::skills::SkillDualWield); // 245 or 252
	if (chance > 0)
	{
		if (IsClient())
			chance += GetLevel();
		else if (GetLevel() > 35)
			chance += GetLevel();
	}

	chance += aabonuses.Ambidexterity; // 32

	// SE_DualWieldChance[176] - Deftdance and Kinesthetics Disciplines
	chance += spellbonuses.DualWieldChance;

	if (returnEffectiveSkill)
		return chance;
	else
		return chance * 100 / 375;
}

bool Mob::CheckDualWield()
{
	if (GetDualWieldChance(true) > zone->random.Int(0, 374))			// 1% per 3.75 skill
		return true;

	return false;
}

void Mob::CommonDamage(Mob* attacker, int32 &damage, const uint16 spell_id, const EQ::skills::SkillType skill_used, bool &avoidable, const int8 buffslot, const bool iBuffTic) 
{

	if (!IsSelfConversionSpell(spell_id) && !iBuffTic)
	{
		CommonBreakInvisible(true);

		bool miss = damage == DMG_MISS;
		bool skip_self = false;
		if (!hidden && !improved_hidden && sneaking && miss)
			skip_self = true;

		CommonBreakSneakHide(skip_self);

	}

	// This method is called with skill_used=ABJURE for Damage Shield damage.
	bool FromDamageShield = (skill_used == EQ::skills::SkillAbjuration);

	Log(Logs::Detail, Logs::Combat, "Applying damage %d done by %s with skill %d and spell %d, avoidable? %s, is %sa buff tic in slot %d",
		damage, attacker?attacker->GetName():"NOBODY", skill_used, spell_id, avoidable?"yes":"no", iBuffTic?"":"not ", buffslot);

	if ((GetInvul() || DivineAura()) && spell_id != SPELL_CAZIC_TOUCH) {
		Log(Logs::Detail, Logs::Combat, "Avoiding %d damage due to invulnerability.", damage);
		damage = DMG_INVUL;
	}

	if( spell_id != SPELL_UNKNOWN || attacker == nullptr )
		avoidable = false;

	if (attacker) 
	{
		if (attacker->IsClient())
		{
			// Damage shield damage shouldn't count towards who gets EXP
			if (!attacker->CastToClient()->IsFeigned() && !FromDamageShield)
			{
				AddToHateList(attacker, 0, damage, false, iBuffTic, false);
			}
		}
		else
		{
			AddToHateList(attacker, 0, damage, false, iBuffTic, false);
		}
	}

	bool died = false;
	if(damage > 0) {
		//if there is some damage being done and theres an attacker involved
		if(attacker) {
			if(spell_id == SPELL_LEECH_TOUCH && attacker->IsClient() && attacker->CastToClient()->CheckAAEffect(aaEffectLeechTouch)){
				attacker->CastToClient()->DisableAAEffect(aaEffectLeechTouch);
			}

			// if spell is lifetap add hp to the caster
			// TODO - investigate casting the reversed tap spell on the caster instead of doing this, so we can send proper tap_amount in action packet.  instant spells too not just buffs maybe.
			if (spell_id != SPELL_UNKNOWN && IsLifetapSpell(spell_id) && !IsBuffSpell(spell_id))
			{
				int healed = attacker->GetActSpellHealing(spell_id, damage, this);
				Log(Logs::General, Logs::Spells, "Applying lifetap heal of %d to %s", healed, attacker->GetName());
				attacker->HealDamage(healed);
			}

			if(IsNPC() && !zone->IsIdling())
			{
				int32 adj_damage = GetHP() - damage < 0 ? GetHP() : damage;

				total_damage += adj_damage;


				// NPC DS damage is just added to npc_damage.
				if (FromDamageShield && (attacker->IsClient() || attacker->IsPlayerOwned()))
					ds_damage += adj_damage;

				if (!FromDamageShield && attacker->IsClient())
				{
					player_damage += adj_damage;
				}

				Client* ultimate_owner = attacker->IsClient() ? attacker->CastToClient() : nullptr;

				if (attacker->IsPlayerOwned() && ultimate_owner == nullptr)
				{
					if (attacker->HasOwner() && attacker->GetUltimateOwner()->IsClient())
					{
						ultimate_owner = attacker->GetUltimateOwner()->CastToClient();
					}
					if ((attacker->IsNPC() && attacker->CastToNPC()->GetSwarmInfo() && attacker->CastToNPC()->GetSwarmInfo()->GetOwner() && attacker->CastToNPC()->GetSwarmInfo()->GetOwner()->IsClient()))
					{
						ultimate_owner = attacker->CastToNPC()->GetSwarmInfo()->GetOwner()->CastToClient();
					}
				}

				if (ultimate_owner)
				{
					bool is_raid_solo_fte_credit = ultimate_owner->GetRaid() ? ultimate_owner->GetRaid()->GetID() == CastToNPC()->solo_raid_fte : false;
					bool is_group_solo_fte_credit = ultimate_owner->GetGroup() ? ultimate_owner->GetGroup()->GetID() == CastToNPC()->solo_group_fte : false;
					bool is_solo_fte_credit = ultimate_owner->CharacterID() == CastToNPC()->solo_fte_charid ? true : false;
					if (is_solo_fte_credit || is_raid_solo_fte_credit || is_group_solo_fte_credit)
					{
						ssf_player_damage += adj_damage;
						if (FromDamageShield)
						{
							ssf_ds_damage += adj_damage;
						}
					}
				}

				if (attacker->IsDireCharmed())
					dire_pet_damage += adj_damage;

				if (attacker->IsNPC() && (!attacker->IsPet() || (attacker->GetOwner() && attacker->GetOwner()->IsNPC())))
					npc_damage += adj_damage;

				if (IsValidSpell(spell_id) && spells[spell_id].targettype == ST_AECaster)
					pbaoe_damage += adj_damage;
			}

			// only apply DS if physical damage (no spell damage) damage shield calls this function with spell_id set, so its unavoidable
			// We want to check damage shield after the round's damage packets are sent out, otherwise to the client we will be processing
			// the damage shield before we process the hit that caused it. 
			// solar: 2022-05-24 moved this up before rune contrary to above comment to make WotW work
			if (spell_id == SPELL_UNKNOWN && skill_used != EQ::skills::SkillArchery && skill_used != EQ::skills::SkillThrowing) {
				DamageShield(attacker);
			}
		}	//end `if there is some damage being done and theres an attacker person involved`

		//see if any runes want to reduce this damage. DS should bypass runes.
		if (!FromDamageShield)
		{
			if (spell_id == SPELL_UNKNOWN) {
				damage = ReduceDamage(damage);
				Log(Logs::Detail, Logs::Combat, "Melee Damage reduced to %d", damage);
			}
			else {
				int32 origdmg = damage;
				damage = AffectMagicalDamage(damage, spell_id, iBuffTic, attacker);
				if (origdmg != damage && attacker && attacker->IsClient()) {
					if (attacker->CastToClient()->GetFilter(FilterDamageShields) != FilterHide)
						attacker->Message(Chat::Yellow, "The Spellshield absorbed %d of %d points of damage", origdmg - std::max(damage, 0), origdmg);
				}
			}
		}

		//final damage has been determined.
		int old_hp_ratio = (int)GetHPRatio();
		if (damage > 0)
		{
			SetHP(GetHP() - damage);
			if (IsClient())
				CastToClient()->CalcAGI(); // AGI depends on current hp (near death)
		}

		// Don't let them go unconscious due to a DOT, otherwise the client may not complete its death routine properly.
		if(HasDied() || (iBuffTic && GetHP() <= 0))
		{
			bool IsSaved = false;

			if(TryDivineSave()) 
			{
				IsSaved = true;
            }

			if(!IsSaved) 
			{
				if (IsNPC())
					died = !CastToNPC()->GetDepop();
				else if (IsClient())
					died = CastToClient()->CheckDeath();

				if (died)
				{
					if (IsClient())
						CastToClient()->m_pp.cur_hp = -100;
					SetHP(-100);
				}
			}
		}
		else
		{
			if(GetHPRatio() < 16.0f)
				TryDeathSave();
		}

		if (!died)
		{
			TrySpinStunBreak();
		}

		//fade mez if we are mezzed
		if (IsMezzed() && attacker) {
			Log(Logs::Detail, Logs::Combat, "Breaking mez due to attack.");
			BuffFadeByEffect(SE_Mez);
			if(IsNPC())
				entity_list.MessageClose(this, true, RuleI(Range, SpellMessages), Chat::SpellWornOff, "%s is no longer mezzed. (%s - %s)", GetCleanName(), attacker->GetCleanName(), spell_id != SPELL_UNKNOWN ? GetSpellName(spell_id) : "melee" );
		}

		if(spell_id != SPELL_UNKNOWN && !iBuffTic) {
			//see if root will break
			if (IsRooted() && !FromDamageShield)  // neotoyko: only spells cancel root
				TryRootFadeByDamage(buffslot, attacker);
		}
		else if(spell_id == SPELL_UNKNOWN)
		{
			//increment chances of interrupting
			if(damage > 0 && IsCasting()) { //shouldnt interrupt on regular spell damage
				attacked_count++;
				Log(Logs::Detail, Logs::Combat, "Melee attack while casting. Attack count %d", attacked_count);
			}
		}

		//send an HP update if we are hurt
		if(GetHP() < GetMaxHP())
		{
			// Don't send a HP update for melee damage unless we've damaged ourself.
			bool hideme = attacker && attacker->IsClient() && attacker->CastToClient()->GMHideMe();
			bool sendtoself = true;
			if (spell_id == SPELL_UNKNOWN && !iBuffTic && attacker != this && !hideme)
			{
				sendtoself = false;
			}

			if (IsNPC()) 
			{
				int cur_hp_ratio = (int)GetHPRatio();
				if (cur_hp_ratio != old_hp_ratio)
					SendHPUpdate(true, sendtoself);
			}
			// Let regen handle buff tics unless this tic killed us.
			else if (!iBuffTic || died)
			{
				SendHPUpdate(true, sendtoself);
			}

			if (!died && IsNPC())
			{
				CheckFlee();
				CheckEnrage();
			}
		}
	}	//end `if damage was done`

	// hundreds of spells have the skill id set to tiger claw in the spell data
	// without this, lots of stuff will 'strike' instead of give a proper spell damage message
	uint8 skill_id = skill_used;
	if (skill_used == EQ::skills::SkillTigerClaw && spell_id > 0 && ((attacker && attacker->GetClass() != Class::Monk) || spell_id != SPELL_UNKNOWN))
	{
		skill_id = EQ::skills::SkillEvocation;
	}

	//buff ticks (DOTs) do not send damage, instead they just call SendHPUpdate()
	if (!iBuffTic) 
	{ 
		// Aggro pet. Pet won't add the attacker to their hatelist until after its master checks if it is dead or not. 
		// DOTs should only aggro pets on the inital cast, and that is handled in SpellOnTarget() DS should also not aggro a pet.
		if (!died && !FromDamageShield)
		{
			AggroPet(attacker);
		}

		if(!died)
			GenerateDamagePackets(attacker, FromDamageShield, damage, spell_id, skill_id, false);
	}
	else
	{ /* Else it is a buff tic - DoT DMG messages were implemented on live in LoY era */
		if (RuleB(Spells, ShowDotDmgMessages) && IsValidSpell(spell_id) && damage > 0 && attacker)
		{
			if (damage > 0 && spell_id != SPELL_UNKNOWN)
			{
				if (attacker && attacker->IsClient() && attacker != this)
				{
					attacker->Message_StringID(Chat::SpellWornOff, YOUR_HIT_DOT, GetCleanName(), itoa(damage), spells[spell_id].name);
				}
			}
		}
	}

	if (died)
	{
		EQ::skills::SkillType attack_skill = static_cast<EQ::skills::SkillType>(skill_id);
		Death(attacker, damage, spell_id, attack_skill, 0, iBuffTic);

		/* After this point, "this" is still a valid object, but its entityID is 0.*/

		if (attacker && attacker->IsNPC())
		{
			uint32 emoteid = attacker->GetEmoteID();
			if (emoteid != 0)
				attacker->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::Killed, emoteid, this);
		}
	}
}

void Mob::GenerateDamagePackets(Mob* attacker, bool FromDamageShield, int32 damage, uint16 spell_id, uint8 skill_id, bool command)
{
	auto outapp = new EQApplicationPacket(OP_Damage, sizeof(Damage_Struct));
	Damage_Struct* a = (Damage_Struct*)outapp->pBuffer;
	a->target = GetID();

	uint16 attacker_id = 0;
	if (attacker)
	{
		attacker_id = attacker->GetID();
		if (!attacker_id) // if the attacker died and created a corpse, the id got zeroed and the corpse inherited it
		{
			attacker_id = attacker->GetCorpseID();
		}
	}

	bool hideme = attacker && attacker->IsClient() && attacker->CastToClient()->GMHideMe();
	if (attacker == nullptr || hideme || (attacker && attacker_id == 0))
	{
		if (attacker != this && IsClient() && !hideme)
		{
			// The attacker is not the same as the defender. Due to this, the HP packet has already been skipped. 
			// Now that we are setting attacker = defender in the packet, we need to send the HP update or else the client will not instantly update the HP bar. 
			auto hp_app2 = new EQApplicationPacket(OP_HPUpdate, sizeof(SpawnHPUpdate_Struct));
			SpawnHPUpdate_Struct* ds = (SpawnHPUpdate_Struct*)hp_app2->pBuffer;
			ds->cur_hp = CastToClient()->GetHP() - itembonuses.HP;
			ds->spawn_id = GetID();
			ds->max_hp = CastToClient()->GetMaxHP() - itembonuses.HP;
			CastToClient()->QueuePacket(hp_app2, false);
			safe_delete(hp_app2);
		}

		a->source = GetID();
	}
	else
	{
		a->source = attacker_id;
	}
	a->type = SkillDamageTypes[skill_id]; // was 0x1c
	a->damage = damage;
	a->spellid = spell_id;

	if (damage > 0 && skill_id < 75) {
		// Push magnitudes in unknown11 are from client decompile
		switch (skill_id) {
		case EQ::skills::Skill1HBlunt:
		case EQ::skills::Skill1HSlashing:
		case EQ::skills::SkillHandtoHand:
		case EQ::skills::SkillThrowing:
			a->force = 0.1f;
			break;
		case EQ::skills::Skill2HBlunt:
		case EQ::skills::Skill2HSlashing:
		case EQ::skills::SkillEagleStrike:
		case EQ::skills::SkillKick:
			a->force = 0.2f;
			break;
		case EQ::skills::SkillArchery:
			a->force = 0.15f;
			break;
		case EQ::skills::SkillBackstab:
		case EQ::skills::SkillBash:
			a->force = 0.3f;
			break;
		case EQ::skills::SkillDragonPunch:
			a->force = 0.25f;
			break;
		case EQ::skills::SkillFlyingKick:
			a->force = 0.4f;
			break;
		case EQ::skills::Skill1HPiercing:
			a->force = 0.05f;
			break;
		case EQ::skills::SkillIntimidation:
			a->force = 2.5f;
			break;
		default:
			a->force = 0.0f;
		}
		if (a->force > 0.0f)
			a->sequence = attacker->GetHeading() * 2.0f;

		if (IsNPC())
		{
			CastToNPC()->AddPush(attacker->GetHeading() * 2.0f, a->force);
		}
	}

	if (spell_id != SPELL_UNKNOWN)
		Log(Logs::Detail, Logs::Spells, "Sending Damage packet for spell %d", spell_id);

	//Note: if players can become pets, they will not receive damage messages of their own
	//this was done to simplify the code here (since we can only effectively skip one mob on queue)
	eqFilterType filter;
	Mob *skip = attacker;
	if (attacker && attacker->GetOwnerID())
	{
		//attacker is a pet
		Mob* owner = attacker->GetOwner();
		if (owner && owner->IsClient())
		{
			if (damage > 0)
			{
				if (spell_id != SPELL_UNKNOWN)
					filter = FilterNone;
				else
					filter = FilterOthersHit;
			}
			else if (damage == DMG_INVUL)
				filter = FilterNone;	//cant filter invulnerable
			else
				filter = FilterOthersMiss;

			// Send OP_Damage to pet attacker's owner.
			if (!FromDamageShield)
				owner->CastToClient()->QueuePacket(outapp, true, CLIENT_CONNECTED, filter);
		}
		skip = owner;
	}
	else
	{
		//attacker is not a pet, send to the attacker
		//if the attacker is a client, try them with the correct filter
		if (attacker && attacker->IsClient())
		{
			if ((spell_id != SPELL_UNKNOWN || FromDamageShield) && damage > 0)
			{
				char val1[20] = { 0 };
				if (attacker != this)
				{
					// Send message + OP_Damage (non-melee/spell) to caster. 
					attacker->Message_StringID(Chat::NonMelee, OTHER_HIT_NONMELEE, GetCleanName(), ConvertArray(damage, val1));
					if (spell_id != SPELL_UNKNOWN && !FromDamageShield)
					{
						attacker->CastToClient()->QueuePacket(outapp);
					}
				}
			}
			else
			{
				if (damage > 0 || damage == DMG_INVUL)
				{
					filter = FilterNone;	//cant filter our own hits or invul.
				}
				else
				{
					filter = FilterMyMisses;
				}

				// Send OP_Damage (melee) to attacker.
				attacker->CastToClient()->QueuePacket(outapp, true, CLIENT_CONNECTED, filter);
			}
		}
		skip = attacker;
	}

	// If this is Damage Shield damage, the correct OP_Damage packets will be sent from Mob::DamageShield, so
	// we don't send them here.
	if (!FromDamageShield)
	{
		//send damage to all clients around except the specified skip mob (attacker or the attacker's owner) and ourself
		if (damage > 0)
		{
			if (spell_id != SPELL_UNKNOWN && !FromDamageShield)
				filter = attacker->IsNPC() ? FilterNPCSpells : FilterPCSpells;
			else
				filter = FilterOthersHit;
		}
		else if (damage == DMG_INVUL)
			filter = FilterNone;	//cant filter invulnerable
		else
			filter = FilterOthersMiss;

		// Send OP_Damage to everybody in range except attacker and defender.
		entity_list.QueueCloseClients(this, outapp, true, RuleI(Range, DamageMessages), skip, true, filter);

		if (IsClient() && (attacker != this || !command))
		{
			if (filter == FilterOthersMiss)
				filter = FilterMissedMe;
			else
				filter = FilterNone;

			// Send OP_Damage to defender/target.
			CastToClient()->QueuePacket(outapp, true, CLIENT_CONNECTED, filter);
		}
	}

	safe_delete(outapp);

	// TODO - this really isn't the right place to do this; investigate casting the lifetap spell on the caster like a recourse
	if (spell_id != SPELL_UNKNOWN && IsLifetapSpell(spell_id) && !IsBuffSpell(spell_id))
	{
		// this causes the caster's client to emote things
		// %1 beams a smile at %2
		// Ahhh, I feel much better now...
		// %1 groans and looks a little weaker.
		// You groan and feel a bit weaker.
		//
		// NOTE: when tapping an dealing 0 damage to a rune, the client doing the tapping will send an emote chat message (OP_SpellTextMessage)
		// The message is routed to other clients and instead of the character's name, it has the word 'You' formatted into it, causing the following:
		// You groans and looks a little weaker.
		// This is a client behavior and looks weird but it's correct.
		if (attacker->IsClient())
		{
			auto message_packet = new EQApplicationPacket(OP_Damage, sizeof(Damage_Struct));
			Damage_Struct *cd = (Damage_Struct *)message_packet->pBuffer;
			cd->target = attacker->GetID();
			cd->source = attacker->GetID();
			cd->type = 231;
			cd->spellid = spell_id;
			cd->sequence = attacker->GetHeading() * 2.0f;
			cd->damage = -damage;
			attacker->CastToClient()->QueuePacket(message_packet);
		}
	}
}

void Mob::GenerateDeathPackets(Mob* killerMob, int32 damage, uint16 spell, uint8 attack_skill, bool bufftic, uint8 killedby)
{
	// Packet creation. The final damage packet is combined with the death packet.
	bool FromDamageShield = (attack_skill == EQ::skills::SkillAbjuration);
	if (killerMob && killerMob->IsClient() && !bufftic)
	{
		if ((spell != SPELL_UNKNOWN || FromDamageShield) && damage > 0)
		{
			char val1[20] = { 0 };
			if (killerMob != this)
			{
				killerMob->Message_StringID(Chat::NonMelee, OTHER_HIT_NONMELEE, GetCleanName(), ConvertArray(damage, val1));
			}
		}
	}

	uint32 out_skill = SkillDamageTypes[attack_skill];
	uint16 out_spell = spell == SPELL_UNKNOWN ? 0xffffffff : spell;
	int32 out_damage = damage;
	uint32 out_killer = killerMob ? killerMob->GetID() : 0;
	uint32 out_corpseid = 0;

	bool hideme = killerMob && killerMob->IsClient() && killerMob->CastToClient()->GMHideMe();
	if (hideme)
	{
		out_killer = GetID();
	}

	if (FromDamageShield && killerMob)
	{
		out_skill = killerMob->GetMobDamageShieldType();
	}

	if (IsClient())
	{
		out_corpseid = GetID();
	}

	// This causes the generic "You died." generic message if there is no killerid sent.
	if (bufftic || out_killer == 0)
	{
		out_spell = 0;
		out_damage = 0;
	}

	if (bufftic)
	{
		out_killer = 0;
		out_skill = EQ::skills::SkillHandtoHand;
		Message(Chat::White, "Pain and suffering tries to strike YOU, but misses!");
	}

	auto app = new EQApplicationPacket(OP_Death, sizeof(Death_Struct));
	Death_Struct* d = (Death_Struct*)app->pBuffer;
	d->spawn_id = GetID();
	d->killer_id = out_killer;
	d->corpseid = out_corpseid;
	d->spell_id = out_spell;
	d->attack_skill = out_skill;
	d->damage = out_damage;
	app->priority = 6;

	Mob* packetsender = this;
	bool skipsender = false;
	if (IsNPC())
	{
		packetsender = killerMob ? killerMob : this;
	}
	else if(IsClient())
	{
		// The client generates its own message when it kills you, skip the sender so we don't get a double message.
		if (killedby == Killed_Client || killedby == Killed_Self || bufftic)
			skipsender = true;
	}
	
	entity_list.QueueClients(packetsender, app, skipsender);
	safe_delete(app);
}

void Mob::HealDamage(uint32 amount, Mob *caster, uint16 spell_id, bool hot)
{
	int32 maxhp = GetMaxHP();
	int32 curhp = GetHP();
	uint32 acthealed = 0;

	if (caster && amount > 0) {
		if (caster->IsNPC() && !caster->IsPet()) {
			float npchealscale = caster->CastToNPC()->GetHealScale();
			amount = (static_cast<float>(amount) * npchealscale) / 100.0f;
		}
	}

	if (amount > (maxhp - curhp))
		acthealed = (maxhp - curhp);
	else
		acthealed = amount;
	if (acthealed > 0 && !hot) 
	{
		// message to target	
		Message_StringID(Chat::Spells, YOU_HEALED, itoa(acthealed));
	}

	if (IsClient())
	{
		CastToClient()->CalcAGI(); // AGI depends on current hp
	}

	if (curhp < maxhp) {
		if ((curhp + amount) > maxhp)
			curhp = maxhp;
		else
			curhp += amount;
		SetHP(curhp);

		SendHPUpdate();
	}
}

float Mob::GetProcChance(uint16 hand)
{
	double chance = 0.0;
	double weapon_speed = GetWeaponSpeedbyHand(hand);
	weapon_speed /= 100.0;

	double dex = GetDEX();
	if (dex > 255.0)
		dex = 255.0;		// proc chance caps at 255
	
	/* Kind of ugly, but results are very accurate
	   Proc chance is a linear function based on dexterity
	   0.0004166667 == base proc chance at 1 delay with 0 dex (~0.25 PPM for main hand)
	   1.1437908496732e-5 == chance increase per point of dex at 1 delay
	   Result is 0.25 PPM at 0 dex, 2 PPM at 255 dex
	*/
	chance = static_cast<double>((0.0004166667 + 1.1437908496732e-5 * dex) * weapon_speed);

	if (hand == EQ::invslot::slotSecondary)
	{
		chance *= 50.0 / static_cast<double>(GetDualWieldChance());
	}								
	
	Log(Logs::Detail, Logs::Combat, "Proc base chance percent: %.3f;  weapon delay: %.2f;  Est PPM: %0.2f", 
		static_cast<float>(chance*100.0), static_cast<float>(weapon_speed), static_cast<float>(chance * (600.0/weapon_speed)));
	return static_cast<float>(chance);
}

bool Mob::TryProcs(Mob *target, uint16 hand)
{
	if(!target) {
		SetTarget(nullptr);
		Log(Logs::General, Logs::Error, "A null Mob object was passed to Mob::TryWeaponProc for evaluation!");
		return false;
	}

	if (!IsAttackAllowed(target)) {
		Log(Logs::Detail, Logs::Combat, "Preventing procing off of unattackable things.");
		return false;
	}

	if (DivineAura() || target->HasDied())
	{
		return false;
	}

	EQ::ItemInstance* heldInst = nullptr;
	const EQ::ItemData* heldStruct = nullptr;

	if (IsNPC())
	{
		if (hand == EQ::invslot::slotPrimary)
			heldStruct = database.GetItem(GetEquipment(EQ::textures::weaponPrimary));
		else if (hand == EQ::invslot::slotSecondary)
			heldStruct = database.GetItem(GetEquipment(EQ::textures::weaponSecondary));
	}
	else if (IsClient())
	{
		heldInst = CastToClient()->GetInv().GetItem(hand);
		if (heldInst)
			heldStruct = heldInst->GetItem();

		if (heldInst && !heldInst->IsWeapon())
			return false;							// abort; holding a non-weapon
	}

	if (heldStruct && TryWeaponProc(heldInst, heldStruct, target, hand))
	{
		return true;	// procs from buffs do not fire if weapon proc succeeds
	}
	else if (hand == EQ::invslot::slotPrimary)	// only do buff procs on primary attacks
	{		
		return TrySpellProc(heldInst, heldStruct, target, hand);
	}

	return false;
}

bool Mob::TryWeaponProc(const EQ::ItemInstance *inst, const EQ::ItemData *weapon, Mob *on, uint16 hand)
{
	if (!weapon)
		return false;

	if (DivineAura())
	{
		return false;
	}

	if (weapon->Proc.Type == EQ::item::ItemEffectCombatProc && weapon->Proc.Effect)
	{
		float ProcChance = GetProcChance(hand);
		float WPC = ProcChance * (100.0f + static_cast<float>(weapon->ProcRate)) / 100.0f;

		if (zone->random.Roll(WPC))
		{
			if (weapon->Proc.Level > GetLevel())
			{
				Log(Logs::Detail, Logs::Combat,
						"Tried to proc (%s), but our level (%d) is lower than required (%d)",
						weapon->Name, GetLevel(), weapon->Proc.Level);

				if (IsPet())
				{
					Mob *own = GetOwner();
					if (own)
						own->Message_StringID(Chat::Red, PROC_PETTOOLOW);
				}
				else
				{
					Message_StringID(Chat::Red, PROC_TOOLOW);
				}
			}
			else
			{
				Log(Logs::Detail, Logs::Combat,
					"Attacking weapon (%s) successfully procing spell %s (%.3f percent chance; weapon proc mod is %i percent)",
					weapon->Name, spells[weapon->Proc.Effect].name, WPC * 100.0f, weapon->ProcRate);

				return ExecWeaponProc(inst, weapon->Proc.Effect, on);
			}
		}
	}
	return false;
}

bool Mob::TrySpellProc(const EQ::ItemInstance *inst, const EQ::ItemData *weapon, Mob *on, uint16 hand)
{
	if (DivineAura() || !on || on->HasDied())
	{
		return false;
	}

	for (uint32 i = 0; i < MAX_PROCS; i++)
	{
		// Spell procs (buffs)
		if (SpellProcs[i].spellID != SPELL_UNKNOWN && !SpellProcs[i].poison)
		{
			float ProcChance = GetProcChance(hand);
			float chance = ProcChance * (static_cast<float>(SpellProcs[i].chance) / 100.0f);
			if (zone->random.Roll(chance))
			{
				Log(Logs::Detail, Logs::Combat,
						"Spell buff proc %d procing spell %s (%.3f percent chance)",
						i, spells[SpellProcs[i].spellID].name, chance * 100.0f);
				ExecWeaponProc(nullptr, SpellProcs[i].spellID, on);
				return true;		// only one proc per round
			}
			else
			{
				Log(Logs::Detail, Logs::Combat,
						"Spell buff proc %d failed to proc %s (%.3f percent chance)",
						i, spells[SpellProcs[i].spellID].name, chance*100.0f);
			}
		}
	}

	return false;
}

bool NPC::TryInnateProc(Mob* target)
{
	// can't innate proc on runed targets
	if (innateProcSpellId == SPELL_UNKNOWN || target->GetSpellBonuses().MeleeRune[0])
	{
		return false;
	}

	if (zone->random.Roll(innateProcChance))
	{
		Log(Logs::Detail, Logs::Combat,
			"NPC innate proc success: spell %d (%d percent chance)",
			innateProcSpellId, innateProcChance);

		int16 resist_diff = spells[innateProcSpellId].ResistDiff;
		
		// Sleeper proc.  it seemed to have hit clients only and was unresistable
		if (innateProcSpellId == SPELL_DRAGON_CHARM && GetRace() == Race::PrismaticDragon)
		{
			if (!target->IsClient())
				return false;

			resist_diff = -1000;
		}

		SpellFinished(innateProcSpellId, target, EQ::spells::CastingSlot::Item, 0, -1, resist_diff, true);
		return true;
	}
	return false;
}

// minBase == 0 will skip the minimum base damage check
void Mob::TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, int32 minBase, int32 damageBonus)
{
	if (damage < 1)
		return;
	if (damageBonus > damage)		// damage should include the bonus already, but calcs need the non-bonus portion
		damageBonus = damage;

	float critChance = 0.0f;
	bool isBerserk = false;
	bool undeadTarget = false;

	//1: Try Slay Undead
	if (defender && (defender->GetBodyType() == BodyType::Undead ||
		defender->GetBodyType() == BodyType::SummonedUndead || defender->GetBodyType() == BodyType::Vampire))
	{
		undeadTarget = true;

		// these were added together in a december 2004 patch.  before then it was probably this, but not 100% sure
		int32 SlayRateBonus = std::max(aabonuses.SlayUndead[0], spellbonuses.SlayUndead[0]);

		if (SlayRateBonus)
		{
			float slayChance = static_cast<float>(SlayRateBonus) / 10000.0f;
			if (zone->random.Roll(slayChance))
			{
				int32 slayDmgBonus = std::max(aabonuses.SlayUndead[1], spellbonuses.SlayUndead[1]);
				damage = ((damage - damageBonus + 6) * slayDmgBonus) / 100 + damageBonus;

				int minSlay = (minBase + 5) * slayDmgBonus / 100 + damageBonus;
				if (damage < minSlay)
					damage = minSlay;

				if (GetGender() == Gender::Female) // female
					entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, FEMALE_SLAYUNDEAD,
						GetCleanName(), itoa(damage));
				else // males and neuter I guess
					entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, MALE_SLAYUNDEAD,
						GetCleanName(), itoa(damage));
				return;
			}
		}
	}

	//2: Try Melee Critical
	if (IsClient())
	{
		// Combat Fury and Fury of the Ages AAs
		int critChanceMult = aabonuses.CriticalHitChance;

		critChance += RuleI(Combat, ClientBaseCritChance);
		float overCap = 0.0f;
		if (GetDEX() > 255)
			overCap = static_cast<float>(GetDEX() - 255) / 400.0f;

		// not used in anything, but leaving for custom servers I guess
		if (spellbonuses.BerserkSPA || itembonuses.BerserkSPA || aabonuses.BerserkSPA)
			isBerserk = true;

		if (GetClass() == Class::Warrior && GetLevel() >= 12)
		{
			if (IsBerserk())
				isBerserk = true;

			critChance += 0.5f + static_cast<float>(std::min(GetDEX(), 255)) / 90.0f + overCap;
		}
		else if (skill == EQ::skills::SkillArchery && GetClass() == Class::Ranger && GetLevel() > 16)
		{
			critChance += 1.35f + static_cast<float>(std::min(GetDEX(), 255)) / 34.0f + overCap * 2;
		}
		else if (GetClass() != Class::Warrior && critChanceMult)
		{
			critChance += 0.275f + static_cast<float>(std::min(GetDEX(), 255)) / 150.0f + overCap;
		}

		if (critChanceMult)
			critChance += critChance * static_cast<float>(critChanceMult) / 100.0f;

		// this is cleaner hardcoded due to the way bonuses work and holyforge crit rate is a max()
		uint8 activeDisc = CastToClient()->GetActiveDisc();

		if (activeDisc == disc_defensive)
			critChance = 0.0f;
		else if (activeDisc == disc_mightystrike)
			critChance = 100.0f;
		else if (activeDisc == disc_holyforge && undeadTarget && critChance < (spellbonuses.CriticalHitChance / 100.0f))
			critChance = static_cast<float>(spellbonuses.CriticalHitChance) / 100.0f;
	}

	int deadlyChance = 0;
	int deadlyMod = 0;

	if (skill == EQ::skills::SkillThrowing && GetClass() == Class::Rogue && GetSkill(EQ::skills::SkillThrowing) >= 65) {
		critChance += RuleI(Combat, RogueCritThrowingChance);
		deadlyChance = RuleI(Combat, RogueDeadlyStrikeChance);
		deadlyMod = RuleI(Combat, RogueDeadlyStrikeMod);
	}

	if (critChance > 0)
	{
		critChance /= 100.0f;

		if (zone->random.Roll(critChance))
		{
			int32 critMod = 17;
			bool crip_success = false;
			int32 cripplingBlowChance = spellbonuses.CrippBlowChance;		// Holyforge Discipline
			int32 minDamage = 0;

			if (cripplingBlowChance || isBerserk)
			{
				if (isBerserk || (cripplingBlowChance && zone->random.Roll(cripplingBlowChance)))
				{
					critMod = 29;
					crip_success = true;
				}
			}

			if (minBase)
				minDamage = (minBase * critMod + 5) / 10 + 8 + damageBonus;

			damage = ((damage - damageBonus) * critMod + 5) / 10 + 8 + damageBonus;
			if (crip_success)
			{
				damage += 2;
				minDamage += 2;
			}
			if (minBase && minDamage > damage)
				damage = minDamage;

			bool deadlySuccess = false;
			if (deadlyChance && zone->random.Roll(static_cast<float>(deadlyChance) / 100.0f))
			{
				if (BehindMob(defender, GetX(), GetY()))
				{
					damage *= deadlyMod;
					deadlySuccess = true;
				}
			}

			// sanity check; 1 damage crits = an error somewhere
			if (damage > 1000000 || damage < 0)
				damage = 1;

			if (crip_success)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, CRIPPLING_BLOW,
						GetCleanName(), itoa(damage));
				// Crippling blows also have a chance to stun
				//Kayen: Crippling Blow would cause a chance to interrupt for npcs < 55, with a staggers message.
				if (defender != nullptr && defender->GetLevel() <= 55 && !defender->GetSpecialAbility(IMMUNE_STUN) && zone->random.Roll(85))
				{
					defender->Emote("staggers.");
					defender->Stun(0, this);
				}
			}
			else if (deadlySuccess)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, DEADLY_STRIKE,
						GetCleanName(), itoa(damage));
			}
			else
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, CRITICAL_HIT,
						GetCleanName(), itoa(damage));
			}
		}
	}

	// Discs
	if (defender && IsClient() && CastToClient()->HasInstantDisc(skill))
	{
		if (damage > 0)
		{
			if (skill == EQ::skills::SkillFlyingKick)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
					Chat::MeleeCrit, FilterMeleeCrits, THUNDEROUS_KICK,
					GetName(), itoa(damage));
			}
			else if (skill == EQ::skills::SkillEagleStrike)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
					Chat::MeleeCrit, FilterMeleeCrits, ASHEN_CRIT,
					GetName(), defender->GetCleanName());
			}
			else if (skill == EQ::skills::SkillDragonPunch)
			{
				uint32 stringid = SILENT_FIST_CRIT;
				if (GetRace() == Race::Iksar)
				{
					stringid = SILENT_FIST_TAIL;
				}

				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
					Chat::MeleeCrit, FilterMeleeCrits, stringid,
					GetName(), defender->GetCleanName());
			}
		}

		if (damage != DMG_MISS)
		{
			CastToClient()->FadeDisc();
		}
	}
}

bool Mob::TryFinishingBlow(Mob *defender, EQ::skills::SkillType skillinuse, uint32 dmgBonus)
{
	if (defender && !defender->IsClient() && defender->GetHPRatio() < 10 && defender->IsFleeing())
	{
		uint32 FB_Dmg = std::max(32000, aabonuses.FinishingBlow[1]);
		uint32 FB_Level = aabonuses.FinishingBlowLvl[0];

		//Proc Chance value of 500 = 5%
		int32 ProcChance = (aabonuses.FinishingBlow[0] + spellbonuses.FinishingBlow[0] + spellbonuses.FinishingBlow[0])/10;

		if (FB_Level && FB_Dmg && (defender->GetLevel() < FB_Level) && (ProcChance >= zone->random.Int(0, 1000)))
		{
			entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
				Chat::MeleeCrit, FilterMeleeCrits, FINISHING_BLOW, 
				GetName());
			DoSpecialAttackDamage(defender, skillinuse, 1, FB_Dmg + dmgBonus, 0);
			return true;
		}
	}
	return false;
}

void Mob::DoRiposte(Mob* defender) {
	Log(Logs::Detail, Logs::Combat, "Preforming a riposte");

	if (!defender || GetSpecialAbility(SpecialAbility::RiposteImmunity))
		return;

	if (defender->IsClient())
	{
		if (defender->CastToClient()->IsUnconscious() || defender->IsStunned() || defender->CastToClient()->IsSitting()
			|| defender->GetAppearance() == eaDead || defender->GetAppearance() == eaCrouching
		)
			return;
	}

	defender->Attack(this, EQ::invslot::slotPrimary);
	if (HasDied()) return;

	int32 DoubleRipChance = defender->aabonuses.GiveDoubleRiposte[0] +
							defender->spellbonuses.GiveDoubleRiposte[0] +
							defender->itembonuses.GiveDoubleRiposte[0];

	DoubleRipChance		 =  defender->aabonuses.DoubleRiposte +
							defender->spellbonuses.DoubleRiposte +
							defender->itembonuses.DoubleRiposte;

	//Live AA - Double Riposte
	if(DoubleRipChance && zone->random.Roll(DoubleRipChance)) {
		Log(Logs::Detail, Logs::Combat, "Preforming a double riposed (%d percent chance)", DoubleRipChance);
		defender->Attack(this, EQ::invslot::slotPrimary);
		if (HasDied()) return;
	}

	//Double Riposte effect, allows for a chance to do RIPOSTE with a skill specfic special attack (ie Return Kick).
	//Coded narrowly: Limit to one per client. Limit AA only. [1 = Skill Attack Chance, 2 = Skill]

	DoubleRipChance = defender->aabonuses.GiveDoubleRiposte[1];

	if(DoubleRipChance && zone->random.Roll(DoubleRipChance)) {
	Log(Logs::Detail, Logs::Combat, "Preforming a return SPECIAL ATTACK (%d percent chance)", DoubleRipChance);

		if (defender->GetClass() == Class::Monk)
			defender->DoMonkSpecialAttack(this, defender->aabonuses.GiveDoubleRiposte[2], true);
	}
}

bool Mob::HasDied() 
{
	bool Result = false;

	if (IsClient() && (GetHP() < -10 || CastToClient()->IsDead()))
	{
		Result = true;
	}
	else if (!IsClient() && GetHP() <= 0)
	{
		Result = true;
	}

	return Result;
}

bool Mob::TryRootFadeByDamage(int buffslot, Mob* attacker) {

 	/*Dev Quote 2010: http://forums.station.sony.com/eq/posts/list.m?topic_id=161443
 	The Viscid Roots AA does the following: Reduces the chance for root to break by X percent.
 	There is no distinction of any kind between the caster inflicted damage, or anyone
 	else's damage. There is also no distinction between Direct and DOT damage in the root code.

 	/* General Mechanics
 	- Check buffslot to make sure damage from a root does not cancel the root
 	- If multiple roots on target, always and only checks first root slot and if broken only removes that slots root.
 	- Only roots on determental spells can be broken by damage.
	- Root break chance values obtained from live parses.
 	*/

	if (!attacker || !spellbonuses.Root[0] || spellbonuses.Root[1] < 0)
		return false;

	int root_buffslot = spellbonuses.Root[1];
	uint16 root_spell_id = buffs[root_buffslot].spellid;

	if (IsValidSpell(root_spell_id) && spells[root_spell_id].goodEffect == 0 && root_buffslot != buffslot)
	{
		int BreakChance = RuleI(Spells, RootBreakFromSpells);

		BreakChance -= BreakChance*buffs[root_buffslot].RootBreakChance/100;
		int level_diff = attacker->GetLevel() - GetLevel();

		BreakChance -= level_diff;

		if (BreakChance < 10)
			BreakChance = 10;

		if (zone->random.Roll(BreakChance))
		{
			BuffFadeBySlot(spellbonuses.Root[1]);
			Log(Logs::Detail, Logs::Combat, "Spell broke root! BreakChance percent chance");
			return true;
		}
	}

	Log(Logs::Detail, Logs::Combat, "Spell did not break root. BreakChance percent chance");
	return false;
}

int32 Mob::RuneAbsorb(int32 damage, uint16 type)
{
	if (type == SE_Rune)
	{
		for(uint32 effectslot = EFFECT_COUNT; effectslot > 0; --effectslot)
		{	
			if(effectslot == spellbonuses.MeleeRune[2] && spellbonuses.MeleeRune[0])
			{
				uint32 slot = spellbonuses.MeleeRune[1];
				if (buffs[slot].melee_rune && IsValidSpell(buffs[slot].spellid))
				{
					int melee_rune_left = buffs[slot].melee_rune;

					if (melee_rune_left > damage)
					{
						melee_rune_left -= damage;
						buffs[slot].melee_rune = melee_rune_left;
						return DMG_RUNE;
					}

					else
					{
						if (melee_rune_left > 0)
							damage -= melee_rune_left;

						BuffFadeBySlot(slot);
					}
				}
			}
		}
	}
	else
	{
		for(uint32 effectslot = EFFECT_COUNT; effectslot > 0; --effectslot)
		{
			if(effectslot == spellbonuses.AbsorbMagicAtt[2] && spellbonuses.AbsorbMagicAtt[0])
			{
				uint32 slot = spellbonuses.AbsorbMagicAtt[1];
				if (buffs[slot].magic_rune && IsValidSpell(buffs[slot].spellid))
				{
					int magic_rune_left = buffs[slot].magic_rune;
					if (magic_rune_left > damage)
					{
						magic_rune_left -= damage;
						buffs[slot].magic_rune = magic_rune_left;
						return 0;
					}

					else
					{
						if (magic_rune_left > 0)
							damage -= magic_rune_left;

						BuffFadeBySlot(slot);
					}
				}
			}
		}
	}

	return damage;
}

void Mob::CommonBreakInvisible(bool skip_sneakhide)
{
	if (invisible || invisible_undead || invisible_animals)
	{
		// we want to make the client uninvis before we ask them to fade the buff slot, 
		// otherwise the client will ask us to make it uninvis again by sending its own appearance packet to the server
		// not calling SetInvisible so that we don't yet clear the invisible flags that are checked below
		SendAppearancePacket(AppearanceType::Invisibility, 0, true, false);

		//break invis when you attack
		if(invisible) 
		{
			Log(Logs::Detail, Logs::Combat, "Removing invisibility due to attack.");
			BuffFadeByEffect(SE_Invisibility);
			invisible = false;
		}
		if(invisible_undead) 
		{
			Log(Logs::Detail, Logs::Combat, "Removing invisibility vs. undead due to attack.");
			BuffFadeByEffect(SE_InvisVsUndead);
			invisible_undead = false;
		}
		if(invisible_animals)
		{
			Log(Logs::Detail, Logs::Combat, "Removing invisibility vs. animals due to attack.");
			BuffFadeByEffect(SE_InvisVsAnimals);
			invisible_animals = false;
		}
	}

	if (!skip_sneakhide)
	{
		CommonBreakSneakHide();
	}
}

void Mob::CommonBreakSneakHide(bool bug_sneak, bool skip_sneak)
{
	bool was_hidden = hidden || improved_hidden;
	bool was_sneaking = sneaking;

	if (!was_hidden && !was_sneaking)
		return;

	if(was_hidden)
	{
		hidden = false;
		improved_hidden = false;
		invisible = false;
		SetInvisible(INVIS_OFF);
	}

	if (was_sneaking && !skip_sneak)
	{
		sneaking = false;
		// AK sometimes updated sneak server side, without sending the client a packet causing a desync. It never happened while hidden.
		if (bug_sneak && !was_hidden)
		{
			SendAppearancePacket(AppearanceType::Sneak, 0, true, true);
			Log(Logs::General, Logs::Skills, "Common break setting sneak to 0. Skipping self update...");
			return;
		}
		else
		{
			SendAppearancePacket(AppearanceType::Sneak, 0);
		}
	}

	if (IsClient() && (!skip_sneak || (was_hidden && !was_sneaking)))
	{
		auto app = new EQApplicationPacket(OP_CancelSneakHide, 0);
		CastToClient()->FastQueuePacket(&app);
		Log(Logs::General, Logs::Skills, "Sent CancelSneakHide packet.");
		return;
	}
}

/* Dev quotes:
 * Old formula
 *	 Final delay = (Original Delay / (haste mod *.01f)) + ((Hundred Hands / 100) * Original Delay)
 * New formula
 *	 Final delay = (Original Delay / (haste mod *.01f)) + ((Hundred Hands / 1000) * (Original Delay / (haste mod *.01f))
 * Base Delay	  20			  25			  30			  37
 * Haste		   2.25			2.25			2.25			2.25
 * HHE (old)	  -17			 -17			 -17			 -17
 * Final Delay	 5.488888889	 6.861111111	 8.233333333	 10.15444444
 *
 * Base Delay	  20			  25			  30			  37
 * Haste		   2.25			2.25			2.25			2.25
 * HHE (new)	  -383			-383			-383			-383
 * Final Delay	 5.484444444	 6.855555556	 8.226666667	 10.14622222
 *
 * Difference	 -0.004444444   -0.005555556   -0.006666667   -0.008222222
 *
 * These times are in 10th of a second
 */

void Mob::SetAttackTimer(bool trigger)
{
	attack_timer.SetDuration(3000, true);
	if (trigger) attack_timer.Trigger();
}

void Client::SetAttackTimer(bool trigger)
{
	float haste_mod = GetHaste() * 0.01f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetDuration(4000, true);

	Timer *TimerToUse = nullptr;
	const EQ::ItemData *PrimaryWeapon = nullptr;

	for (int i = EQ::invslot::slotRange; i <= EQ::invslot::slotSecondary; i++) {
		//pick a timer
		if (i == EQ::invslot::slotPrimary)
			TimerToUse = &attack_timer;
		else if (i == EQ::invslot::slotRange)
			TimerToUse = &ranged_timer;
		else if (i == EQ::invslot::slotSecondary)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		const EQ::ItemData *ItemToUse = nullptr;

		//find our item
		EQ::ItemInstance *ci = GetInv().GetItem(i);
		if (ci)
			ItemToUse = ci->GetItem();

		//see if we have a valid weapon
		if (ItemToUse != nullptr) {
			//check type and damage/delay
			if (ItemToUse->ItemClass != EQ::item::ItemClassCommon
					|| ItemToUse->Damage == 0
					|| ItemToUse->Delay == 0) {
				//no weapon
				ItemToUse = nullptr;
			}
			// Check to see if skill is valid
			else if ((ItemToUse->ItemType > EQ::item::ItemTypeLargeThrowing) &&
					(ItemToUse->ItemType != EQ::item::ItemTypeMartial) &&
					(ItemToUse->ItemType != EQ::item::ItemType2HPiercing)) {
				//no weapon
				ItemToUse = nullptr;
			}
		}

		// Hundred Fists and Blinding Speed disciplines
		int hhe = itembonuses.HundredHands + spellbonuses.HundredHands;
		int speed = 0;
		int delay = 36;
		float quiver_haste = 0.0f;
		int min_delay = RuleI(Combat, MinHastedDelay);
		// for ranged attack, the average on a client will be min delay
		// but it can be lower on a single interval.  Add an additional delay
		// adjust for clients to prevent repeated skill not ready.
		int timer_corr = 0;

		//if we have no weapon..
		if (ItemToUse == nullptr)
		{
			if (GetClass() == Class::Monk || GetClass() == Class::Beastlord)
				delay = GetHandToHandDelay();
		}
		else
		{
			//we have a weapon, use its delay
			delay = ItemToUse->Delay;
		}

		speed = static_cast<int>(((delay / haste_mod) + ((hhe / 100.0f) * delay)) * 100);

		if (ItemToUse != nullptr && ItemToUse->ItemType == EQ::item::ItemTypeBow)
		{
			timer_corr = 100;
			quiver_haste = GetQuiverHaste(); // 0.15 Fleeting Quiver
			if (quiver_haste)
			{
				int bow_delay_reduction = quiver_haste * speed + 1;
				if (speed - bow_delay_reduction > 1000) // this is how sony did it, if your delay is too it doesn't cap at the limit, it just does nothing
				{
					speed -= bow_delay_reduction;
				}
			}
		}

		TimerToUse->SetDuration(std::max(min_delay, speed) - timer_corr, true);
		if (trigger) TimerToUse->Trigger();

		if (i == EQ::invslot::slotPrimary)
			PrimaryWeapon = ItemToUse;
	}

	if (!IsDualWielding())
		attack_dw_timer.Disable();
}

void NPC::SetAttackTimer(bool trigger)
{
	float haste_mod = GetHaste() * 0.01f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetDuration(3000, true);

	Timer *TimerToUse = nullptr;

	for (int i = EQ::invslot::slotRange; i <= EQ::invslot::slotSecondary; i++)
	{
		int16 delay = attack_delay;

		// NOTE: delay values under 401 are consdiered hundreds of milliseconds, so we multiply by 100 for those
		if (delay < 401)
			delay *= 100;

		//pick a timer
		if (i == EQ::invslot::slotPrimary)
			TimerToUse = &attack_timer;
		else if (i == EQ::invslot::slotRange)
			TimerToUse = &ranged_timer;
		else if (i == EQ::invslot::slotSecondary)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		if (!IsPet())
		{
			//find our item
			EQ::ItemInstance* ItemToUseInst = database.CreateItem(database.GetItem(equipment[i]));
			if (ItemToUseInst)
			{
				uint8 weapon_delay = ItemToUseInst->GetItem()->Delay;
				if (ItemToUseInst->IsWeapon() && weapon_delay < attack_delay)
				{
					delay = weapon_delay * 100;
				}

				safe_delete(ItemToUseInst);
			}
		}

		int speed = static_cast<int>(delay / haste_mod);

		TimerToUse->SetDuration(std::max(RuleI(Combat, MinHastedDelay), speed), true);
		if (trigger) TimerToUse->Trigger();
	}

	if (!IsDualWielding())
		attack_dw_timer.Disable();
}

void NPC::DisplayAttackTimer(Client* sender)
{
	uint32 primary = 0;
	uint32 ranged = 0;
	uint32 dualwield = 0;
	float haste_mod = GetHaste() * 0.01f;
	int speed = 0;

	for (int i = EQ::invslot::slotRange; i <= EQ::invslot::slotSecondary; i++)
	{
		int16 delay = attack_delay;

		// NOTE: delay values under 401 are consdiered hundreds of milliseconds, so we multiply by 100 for those
		if (delay < 401)
			delay *= 100;

		//special offhand stuff
		if (i == EQ::invslot::slotSecondary)
		{
			if (!IsDualWielding()) 
			{
				continue;
			}
		}

		if (!IsPet())
		{
			//find our item
			EQ::ItemInstance* ItemToUseInst = database.CreateItem(database.GetItem(equipment[i]));
			if (ItemToUseInst)
			{
				uint8 weapon_delay = ItemToUseInst->GetItem()->Delay;
				if (ItemToUseInst->IsWeapon() && weapon_delay < attack_delay)
				{
					delay = weapon_delay * 100;
				}

				safe_delete(ItemToUseInst);
			}
		}

		speed = static_cast<int>(delay / haste_mod);

		if (i == EQ::invslot::slotPrimary)
		{
			primary = speed;
		}
		else if (i == EQ::invslot::slotSecondary)
		{
			dualwield = speed;
		}
		else if (i == EQ::invslot::slotRange)
		{
			ranged = speed;
		}
	}

	sender->Message(Chat::White, "Attack Delays: Main-hand: %d  Off-hand: %d  Ranged: %d", primary, dualwield, ranged);
}

// This will provide reasonable estimates for NPC offense.  based on many parsed logs.
// This ignores NPC strength values in the database for now.  Parsing strength for all
// NPCs is unfeasible
int Mob::GetOffense(EQ::skills::SkillType skill)
{
	int mobLevel = GetLevel();
	int offense = 0;
	bool isSummonedPet = IsSummonedClientPet() || (IsPet() && GetSummonerID());
	if (!isSummonedPet && mobLevel > 45 && mobLevel < 51) {		// NPCs around level 43-50 have a flatter offense value because
		mobLevel = 45;                                          // NPC weapon skills cap at 210 then jump to 250 at level 51
	}

	int baseOffense = mobLevel * 55 / 10 - 4;	// parses indicate that the floor/baseline offense isn't level * 5.  don't know why
	if (baseOffense > 320) {
		baseOffense = 320;
	}

	int strOffense = 0;

	if (mobLevel < 6) {
		baseOffense = mobLevel * 4;
		strOffense = mobLevel;
	}
	else if (mobLevel < 30) {
		strOffense = mobLevel / 2 + 1;
	} else {
		strOffense = mobLevel * 2 - 40;
		if (!isSummonedPet && zone->GetZoneExpansion() == PlanesEQ) {
			strOffense += 20;
		}

	}

	if (isSummonedPet) {
		baseOffense = GetSkill(skill);
		strOffense = 0;
	}

	strOffense += (itembonuses.STR + spellbonuses.STR) * 2 / 3;
	if (strOffense < 0) {
		strOffense = 0;
	}

	offense = baseOffense + strOffense;

	offense += ATK + spellbonuses.ATK;
	if (offense < 1) {
		offense = 1;
	}

	return offense;
}

// This is one half of the atk value displayed in clients
// This is accurate and based on a client decompile done by demonstar
int Client::GetOffense(EQ::skills::SkillType skill)
{
	int statBonus;

	if (skill == EQ::skills::SkillArchery || skill == EQ::skills::SkillThrowing)
	{
		statBonus = GetDEX();
	}
	else
	{
		statBonus = GetSTR();
	}

	int offense = GetSkill(skill) + spellbonuses.ATK + itembonuses.ATK + (statBonus >= 75 ? ((2 * statBonus - 150) / 3) : 0);
	if (offense < 1)
		offense = 1;

	if (GetClass() == Class::Ranger && GetLevel() > 54)
	{
		offense = offense + GetLevel() * 4 - 216;
	}
	
	return offense;
}

int Mob::GetOffenseByHand(int hand)
{
	const EQ::ItemData* weapon = nullptr;

	if (hand != EQ::invslot::slotSecondary)
		hand = EQ::invslot::slotPrimary;

	if (IsNPC())
	{
		uint32 handItem = CastToNPC()->GetEquipment(hand == EQ::invslot::slotSecondary ? EQ::textures::weaponSecondary : EQ::textures::weaponPrimary);
		if (handItem)
			weapon = database.GetItem(handItem);
	}
	else if (IsClient())
	{
		EQ::ItemInstance* weaponInst = CastToClient()->GetInv().GetItem(hand);
		if (weaponInst && weaponInst->IsType(EQ::item::ItemClassCommon))
			weapon = weaponInst->GetItem();
	}

	if (weapon)
	{
		return GetOffense(static_cast<EQ::skills::SkillType>(GetSkillByItemType(weapon->ItemType)));
	}
	else
	{
		return GetOffense(EQ::skills::SkillHandtoHand);
	}
}

int Mob::GetToHit(EQ::skills::SkillType skill)
{
	int accuracy = 0;
	int toHit = 7 + GetSkill(EQ::skills::SkillOffense) + GetSkill(skill);

	if (IsClient())
	{
		accuracy = itembonuses.Accuracy[EQ::skills::HIGHEST_SKILL + 1] +
			spellbonuses.Accuracy[EQ::skills::HIGHEST_SKILL + 1] +
			aabonuses.Accuracy[EQ::skills::HIGHEST_SKILL + 1] +
			aabonuses.Accuracy[skill] +
			itembonuses.HitChance; //Item Mod 'Accuracy'

		// taken from a client decompile (credit: demonstar)
		int drunkValue = CastToClient()->m_pp.intoxication / 2;
		if (drunkValue > 20)
		{
			int drunkReduction = 110 - drunkValue;
			if (drunkReduction > 100)
				drunkReduction = 100;
			toHit = toHit * drunkReduction / 100;
		}
		else if (GetClass() == Class::Warrior && CastToClient()->IsBerserk())
		{
			toHit += 2 * GetLevel() / 5;
		}

	}
	else
	{
		accuracy = CastToNPC()->GetAccuracyRating();	// database value
		if (GetLevel() < 3)
			accuracy += 2;		// level 1 and 2 NPCs parsed a few points higher than expected
	}

	toHit += accuracy;
	return toHit;
}

int Mob::GetToHitByHand(int hand)
{
	if (IsNPC())
		return GetToHit(EQ::skills::SkillHandtoHand);
	
	EQ::ItemInstance* weapon;

	if (hand == EQ::invslot::slotSecondary)
		weapon = CastToClient()->GetInv().GetItem(EQ::invslot::slotSecondary);
	else
		weapon = CastToClient()->GetInv().GetItem(EQ::invslot::slotPrimary);

	if (weapon && weapon->IsType(EQ::item::ItemClassCommon))
	{
		return GetToHit(static_cast<EQ::skills::SkillType>(GetSkillByItemType(weapon->GetItem()->ItemType)));
	}
	else
	{
		return GetToHit(EQ::skills::SkillHandtoHand);
	}
}

/*	This will ignore the database AC value for NPCs under level 52 or so and calculate a value instead.
	Low level NPC mitigation estimates parsed to highly predictable and uniform values, and the AC value
	is very sensitive to erroneous entries, which means entering the wrong value in the database will
	result in super strong or weak NPCs, so it seems wiser to hardcode it.

	Most NPCs level 50+ have ~200 mit AC.  Raid bosses have more. (anywhere from 200-1200)  This uses the
	database AC value if it's higher than 200 and the default calcs to 200.

	Note that the database AC values are the computed estimates from parsed logs, so it factors in AC from
	the defense skill+agility.  If NPC data is ever leaked in the future then Sony's AC values will likely
	be lower than what the AC values in our database are because of this, and this algorithm will need to
	be altered to add in AC from defense skill and agility.
*/
int Mob::GetMitigation()
{
	int mit;

	if (IsSummonedClientPet())
	{		
		mit = GetAC();
	}
	else
	{
		if (GetLevel() < 15)
		{
			mit = GetLevel() * 3;

			if (GetLevel() < 3)
				mit += 2;
		}
		else
		{
			if (zone->GetZoneExpansion() == PlanesEQ)
				mit = 200;
			else
				mit = GetLevel() * 41 / 10 - 15;
		}

		if (mit > 200)
			mit = 200;

		if (mit == 200 && GetAC() > 200)
			mit = GetAC();
	}

	mit += (4 * itembonuses.AC / 3) + (spellbonuses.AC / 4);
	if (mit < 1)
		mit = 1;

	return mit;
}

// This returns the mitigation portion of the client AC value
// this is accurate and based on a Sony developer post
// See https://forums.daybreakgames.com/eq/index.php?threads/ac-vs-acv2.210028/
// ignoreCap set true will ignore the AC softcap and anti-twink cap and is only intended to
// compare output with the client's displayed AC value
int Client::GetMitigation(bool ignoreCap, int item_ac_sum, int shield_ac, int spell_ac_sum, int classnum, int level, int base_race, int carried_weight, int agi, int defense_skill_value, int combat_stability_percent)
{
	int32 acSum = item_ac_sum;
	uint8 playerClass = classnum;

	// add 33% to item AC for all but NEC WIZ MAG ENC
	if (playerClass != Class::Necromancer && playerClass != Class::Wizard && playerClass != Class::Magician && playerClass != Class::Enchanter)
	{
		acSum = 4 * acSum / 3;
	}

	// anti-twink
	if (!ignoreCap && level < 50 && acSum > (level * 6 + 25))
	{
		acSum = level * 6 + 25;
	}

	if (playerClass == Class::Monk)
	{
		int32 hardcap, softcap;

		if (level < 15)
		{ // 1-14
			hardcap = 30;
			softcap = 14;
		}
		else if (level <= 29)
		{ // 15-29
			hardcap = 32;
			softcap = 15;
		}
		else if (level <= 44)
		{ // 30-44
			hardcap = 34;
			softcap = 16;
		}
		else if (level <= 50)
		{ // 45-50
			hardcap = 36;
			softcap = 17;
		}
		else if (level <= 54)
		{ // 51-54
			hardcap = 38;
			softcap = 18;
		}
		else if (level <= 59)
		{ // 55-59
			hardcap = 40;
			softcap = 20;
		}
		else if (level <= 61)
		{ // 60-61
			hardcap = 45;
			softcap = 24;
		}
		else if (level <= 63)
		{ // 62-63
			hardcap = 47;
			softcap = 24;
		}
		else if (level <= 64)
		{ // 64
			hardcap = 50;
			softcap = 24;
		}
		else
		{ // 65
			hardcap = 53;
			softcap = 24;
		}

		int32 weight = carried_weight;
		double acBonus = level + 5.0;

		if (weight <= softcap) // 93 bonus at level 65 when under 24 weight
		{
			acSum += static_cast<int32>(acBonus * 4.0 / 3.0);
		}
		else if (weight > hardcap + 1) // scales the penalty from -11 down to -93 at level 65 with 143 weight
		{
			double penalty = level + 5.0;
			double multiplier = (weight - (hardcap - 10)) / 100.0;
			if (multiplier > 1.0) multiplier = 1.0;
			penalty = 4.0 * penalty / 3.0;
			penalty = multiplier * penalty;

			acSum -= static_cast<int32>(penalty);
		}
		else if (weight > softcap) // scales the bonus from 93 down to 0 at level 65 with 39 weight
		{
			double reduction = (weight - softcap) * 6.66667;
			if (reduction > 100.0) reduction = 100.0;
			reduction = (100.0 - reduction) / 100.0;
			acBonus *= reduction;
			if (acBonus < 0.0) acBonus = 0.0;
			acBonus = 4.0 * acBonus / 3.0;

			acSum += static_cast<int32>(acBonus);
		}

	}
	else if (playerClass == Class::Rogue)
	{
		if (level >= 30 && agi > 75)
		{
			// this bonus is small, it gets maxed out at 12
			//   by level 50 with 80 agi
			//   by level 42 with 85 agi
			//   by level 38 with 90 agi
			//   by level 36 with 100 agi
			int32 levelScaler = level - 26;
			int32 acBonus = 0;

			if (agi < 80)
			{
				acBonus = levelScaler / 4;
			}
			else if (agi < 85)
			{
				acBonus = levelScaler * 2 / 4;
			}
			else if (agi < 90)
			{
				acBonus = levelScaler * 3 / 4;
			}
			else if (agi < 100)
			{
				acBonus = levelScaler * 4 / 4;
			}
			else
			{
				acBonus = levelScaler * 5 / 4;
			}

			if (acBonus > 12) acBonus = 12;

			acSum += acBonus;
		}
	}
	else if (playerClass == Class::Beastlord)
	{
		if (level > 10)
		{
			// this bonus is small, it gets maxed out at 16
			//   by level 46 with 80 agi
			//   by level 33 with 85 agi
			//   by level 26 with 90 agi
			//   by level 22 with 100 agi
			int32 levelScaler = level - 6;
			int32 acBonus = 0;

			if (agi < 80)
			{
				acBonus = levelScaler / 5;
			}
			else if (agi < 85)
			{
				acBonus = levelScaler * 2 / 5;
			}
			else if (agi < 90)
			{
				acBonus = levelScaler * 3 / 5;
			}
			else if (agi < 100)
			{
				acBonus = levelScaler * 4 / 5;
			}
			else
			{
				acBonus = levelScaler * 5 / 5;
			}

			if (acBonus > 16) acBonus = 16;

			acSum += acBonus;
		}
	}
	if (base_race == IKSAR)
	{
		if (level < 10)
		{
			acSum += 10;
		}
		else if (level > 35)
		{
			acSum += 35;
		}
		else
		{
			acSum += level;
		}
	}

	if (acSum < 0)
		acSum = 0;

	int32 defense = defense_skill_value;
	if (defense > 0)
	{
		if (playerClass == Class::Wizard || playerClass == Class::Necromancer || playerClass == Class::Enchanter || playerClass == Class::Magician)
		{
			acSum += defense / 2;
		}
		else
		{
			acSum += defense / 3;
		}
	}

	int spellACDivisor = 4;
	if (playerClass == Class::Wizard || playerClass == Class::Magician || playerClass == Class::Necromancer || playerClass == Class::Enchanter)
	{
		spellACDivisor = 3;
	}
	acSum += (spell_ac_sum / spellACDivisor);

	if (agi > 70)
		acSum += agi / 20;

	if (acSum < 0)
		acSum = 0;

	int32 softcap = 350; // AC cap is 350 for all classes in Classic era and for levels 50 and under

	if (level > 50) {
		if (content_service.IsTheScarsOfVeliousEnabled()) { // earliest known client with these caps is April 4, 2001; defaulting this to Velious Era
			switch (playerClass) {
				case Class::Warrior: {
					softcap = 430;
					break;
				}
				case Class::Paladin:
				case Class::ShadowKnight:
				case Class::Cleric:
				case Class::Bard: {
					softcap = 403;
					break;
				}
				case Class::Ranger:
				case Class::Shaman: {
					softcap = 375;
					break;
				}
				case Class::Monk: {
					softcap = RuleB(AlKabor, ReducedMonkAC) ? 315 : 350;
					break;
				}
				default: {
					softcap = 350;		// dru, rog, wiz, ench, nec, mag, bst
				}
			}
		}

		else {
			if (playerClass == Class::Warrior && content_service.IsTheRuinsOfKunarkEnabled()) {
				softcap = 405;
			}
		}
	}

	// Combat Stability AA - this raises the softcap
	softcap += combat_stability_percent * softcap / 100;

	if (content_service.IsTheShadowsOfLuclinEnabled()) {
		// shield AC is not capped in Luclin
		softcap += shield_ac;
	}

	if (!ignoreCap && acSum > softcap) {
		if (level <= 50) {
			return softcap;		// it's hard <= level 50
		}

		if (!content_service.IsTheShadowsOfLuclinEnabled()) {
			return softcap;
		}

		int32 overcap = acSum - softcap;
		int32 returns = 20;

		if (!content_service.IsThePlanesOfPowerEnabled()) {
			returns = 12;
			
			if (playerClass == Class::Cleric || playerClass == Class::Druid || playerClass == Class::Shaman || playerClass == Class::Wizard || playerClass == Class::Magician || playerClass == Class::Enchanter || playerClass == Class::Necromancer) {
				overcap = 0; // melee only until PoP
			}
		}
		else {
			if (playerClass == Class::Warrior) {
				if (level <= 61) {
					returns = 5;
				}
				else if (level <= 63) {
					returns = 4;
				}
				else {
					returns = 3;
				}
			}
			else if (playerClass == Class::Paladin || playerClass == Class::ShadowKnight) {
				if (level <= 61) {
					returns = 6;
				}
				else if (level <= 63) {
					returns = 5;
				}
				else {
					returns = 4;
				}
			}
			else if (playerClass == Class::Bard) {
				if (level <= 61) {
					returns = 8;
				}
				else if (level <= 63) {
					returns = 7;
				}
				else {
					returns = 6;
				}
			}
			else if (playerClass == Class::Monk || playerClass == Class::Rogue) {
				if (level <= 61) {
					returns = 20;
				}
				else if (level == 62) {
					returns = 18;
				}
				else if (level == 63) {
					returns = 16;
				}
				else if (level == 64) {
					returns = 14;
				}
				else {
					returns = 12;
				}
			}
			else if (playerClass == Class::Ranger || playerClass == Class::Beastlord)	{
				if (level <= 61) {
					returns = 10;
				}
				else if (level == 62) {
					returns = 9;
				}
				else if (level == 63) {
					returns = 8;
				}
				else {
					returns = 7;
				}
			}
		}
		acSum = softcap + overcap / returns;

	}
	return acSum;
}

int Client::GetMitigation(bool ignoreCap)
{
	// shield AC is not capped, so this value is just added to the softcap
	int shield_ac = 0;
	const EQ::ItemInstance* inst = m_inv.GetItem(EQ::invslot::slotSecondary);
	if (inst)
	{
		if (inst->GetItem()->ItemType == EQ::item::ItemTypeShield)
		{
			shield_ac = inst->GetItem()->AC;
		}
	}

	int carried_weight = GetWeight() / 10;

	return GetMitigation(ignoreCap, itembonuses.AC, shield_ac, spellbonuses.AC, GetClass(), GetLevel(), GetBaseRace(), carried_weight, GetAGI(), GetSkill(EQ::skills::SkillDefense), aabonuses.CombatStability);
}

// These are fairly accurate estimates based on many parsed Live logs
// NPC defense skill and agility values are unknowable, so we estimate avoidance AC based on miss rates
int Mob::GetAvoidance()
{
	int level = GetLevel();
	int avoidance = level * 9 + 5;

	if (level <= 50 && avoidance > 400)
		avoidance = 400;
	else if (avoidance > 460)
		avoidance = 460;

	// this is how Live does it for PCs and NPCs.  AK might have (likely) been different.  Can't know how AK did it.
	// but the difference is so small nobody would notice
	avoidance += (spellbonuses.AGI + itembonuses.AGI) * 22 / 100;
	avoidance += bonusAvoidance;
	if (avoidance < 1)
		avoidance = 1;

	return avoidance;
}

// this output is precise and is based on https://forums.daybreakgames.com/eq/index.php?threads/ac-vs-acv2.210028/
int Client::GetAvoidance(int16 defense_skill_value, int16 agi, uint8 level, uint8 intoxication, int combat_agility_percent)
{
	int computedDefense = 1;
	int defenseAvoidance = 0;

	if (defense_skill_value > 0)
	{
		defenseAvoidance = defense_skill_value * 400 / 225;
	}

	// max agility bonus (called agiAvoidance here) is 53 with level > 40 and AGI 200
	// defense 252 (WAR PAL SHD MNK BRD ROG ) = 448 + 53 = 501
	// defense 240 (RNG BST) = 426 + 53 = 479
	// defense 200 (CLR DRU SHM) = 355 + 53 = 408
	// defense 145 (NEC WIZ MAG ENC) = 257 + 53 = 310

	// note: modern EQ does this here: GetAGI() > 40 ? (GetAGI() - 40) * 8000 / 36000 : 0;
	// old clients had a different calculation.  This is the precise output, based on a decompile done by Secrets
	int agiAvoidance = 0;
	if (agi < 40)
	{
		// 0-39 AGI = -25 to 0
		agiAvoidance = (25 * (agi - 40)) / 40;
	}
	else if (agi >= 60 && agi <= 74)
	{
		// 40-60 AGI = 0
		agiAvoidance = (2 * (28 - ((200 - agi) / 5))) / 3;
	}
	else if (agi >= 75)
	{
		// 75-200 AGI = 6 to 53
		// AGI over 200 provides no further benefit for this bonus
		
		// 36 to 53
		int bonusAdj = 80;

		if (level < 7)
		{
			// 6 to 23
			bonusAdj = 35;
		}
		else if (level < 20)
		{
			// 20 to 36
			bonusAdj = 55;
		}
		else if (level < 40)
		{
			// 30 to 46
			bonusAdj = 70;
		}

		if (agi < 200)
		{
			agiAvoidance = (2 * (bonusAdj - ((200 - agi) / 5))) / 3;
		}
		else
		{
			agiAvoidance = 2 * bonusAdj / 3;
		}
	}

	computedDefense = defenseAvoidance + agiAvoidance;

	// combat agility scaling
	computedDefense += computedDefense * combat_agility_percent / 100;

	int drunk_factor = intoxication / 2;
	if (drunk_factor > 20)
	{
		int drunk_multiplier = 110 - drunk_factor;
		if (drunk_multiplier > 100)
		{
			drunk_multiplier = 100;
		}
		computedDefense = computedDefense * drunk_multiplier / 100;
	}

	if (computedDefense < 1)
		computedDefense = 1;

	return computedDefense;
}

// the bool to ignore the Combat Agility AAs is to compare AC with the client's displayed AC.  Combat calcs
// should not ignore it
int Client::GetAvoidance(bool ignoreCombatAgility)
{
	int combat_agility_percent = ignoreCombatAgility ? 0 : aabonuses.CombatAgility;
	int computedDefense = GetAvoidance(GetSkill(EQ::skills::SkillDefense), GetAGI(), GetLevel(), m_pp.intoxication, combat_agility_percent);

	return computedDefense;
}

void Mob::DamageTotalsWipe(bool from_memblur)
{
	player_damage = 0;
	dire_pet_damage = 0;
	total_damage = 0;
	ds_damage = 0;
	npc_damage = 0;
	gm_damage = 0;
	pbaoe_damage = 0;
	if(!from_memblur)
	{
		ssf_player_damage = 0;
		ssf_ds_damage = 0;
	}
}
