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

#include "../common/rulesys.h"
#include "../common/strings.h"

#include "beacon.h"
#include "client.h"
#include "entity.h"
#include "mob.h"
#include "string_ids.h"
#include "worldserver.h"

#include <string.h>

extern WorldServer worldserver;

// set minDamage to DMG_INVUL if target is immune
int Mob::DoSpecialAttackDamage(Mob *defender, EQ::skills::SkillType skill, int base, int minDamage, int hate, DoAnimation animation_type)
{
	if (!defender)
		return 0;

	int damage = 1;
	int damageBonus = 0;
	if (IsNPC())
	{
		damageBonus = CastToNPC()->GetDamageBonus();
		hate = base / 2;
	}
	else if (hate == 0)
	{
		hate = base;
	}

	bool rangedAttack = false;
	if (skill == EQ::skills::SkillThrowing || skill == EQ::skills::SkillArchery)
		rangedAttack = true;

	if (minDamage == DMG_INVUL || defender->GetSpecialAbility(SpecialAbility::MeleeImmunity))
		damage = DMG_INVUL;

	if (damage > 0)
		defender->AvoidDamage(this, damage, rangedAttack);

	if (damage > 0 && !defender->AvoidanceCheck(this, skill))
		damage = DMG_MISS;

	if (minDamage >= 32000)		// so finishing blow doesn't do all this other stuff under this
	{
		damage = minDamage;
	}
	else if (damage > 0)
	{
		damage = damageBonus + CalcMeleeDamage(defender, base, skill);

		if (damage < minDamage)
			damage = minDamage;
		
		defender->TryShielderDamage(this, damage, skill);	// warrior /shield

		if (skill == EQ::skills::SkillBash || skill == EQ::skills::SkillKick || skill == EQ::skills::SkillDragonPunch)
			TryBashKickStun(defender, skill);			// put this here so the stun occurs before the damage

		if (skill == EQ::skills::SkillBash && base == 1)
			damage = 1;	// 0 skill bashes always do 1 damage (10 if it crits)

		if (IsClient())
			TryCriticalHit(defender, skill, damage);
	}

	if (animation_type != DoAnimation::None) {
		DoAnim(animation_type, 0, false);
	}

	defender->AddToHateList(this, hate, 0);
	defender->Damage(this, damage, SPELL_UNKNOWN, skill, false);

	if (!HasDied() && GetTarget() && damage == DMG_RIPOSTE && !defender->HasDied())
		DoRiposte(defender);

	return damage;
}

void Mob::TryBashKickStun(Mob* defender, uint8 skill)
{
	// bash and kick stuns. (and silentfisted dragonpunch)  Stuns hit even through runes
	if (!defender || defender->GetSpecialAbility(SpecialAbility::StunImmunity) || defender->DivineAura() || defender->GetInvul()
		|| (skill != EQ::skills::SkillBash && skill != EQ::skills::SkillKick && skill != EQ::skills::SkillDragonPunch))
	{
		return;
	}
	
	// both PC and NPC warrior kicks stun starting at 55
	if (skill == EQ::skills::SkillKick && ((GetClass() != Class::Warrior && GetClass() != Class::WarriorGM) || GetLevel() < 55))
		return;

	if (skill == EQ::skills::SkillDragonPunch && (!IsClient() || !CastToClient()->HasInstantDisc(skill)))
		return;

	// this is precise for the vast majority of NPCs
	// at some point around level 61, base stun chance goes from 45 to 40.  not sure why
	int stun_chance = 45;
	int defenderLevel = defender->GetLevel();
	int levelDiff = GetLevel() - defenderLevel;

	if (GetLevel() > 60)
		stun_chance = 40;

	if (levelDiff < 0)
	{
		stun_chance -= levelDiff * levelDiff / 2;
	}
	else
	{
		stun_chance += levelDiff * levelDiff / 2;
	}
	if (stun_chance < 2)
		stun_chance = 2;

	if (defender->IsNPC() && defenderLevel > RuleI(Spells, BaseImmunityLevel))
		stun_chance = 0;

	if (stun_chance && zone->random.Roll(stun_chance))
	{
		Log(Logs::Detail, Logs::Combat, "Stun passed, checking resists. Was %d chance.", stun_chance);

		int stun_resist = 0;

		if (defender->IsClient())
		{
			stun_resist = defender->aabonuses.StunResist;						// Stalwart Endurance AA
		}

		if (defender->GetBaseRace() == OGRE && !BehindMob(defender, GetX(), GetY()))		// should this work if the ogre is illusioned?
		{
			Log(Logs::Detail, Logs::Combat, "Frontal stun resisted because, Ogre.");
		}
		else
		{
			if (stun_resist && zone->random.Roll(stun_resist))
			{
				defender->Message_StringID(Chat::DefaultText, StringID::AVOID_STUN);
				Log(Logs::Detail, Logs::Combat, "Stun Resisted. %d chance.", stun_resist);
			}
			else
			{
				Log(Logs::Detail, Logs::Combat, "Stunned. %d resist chance.", stun_resist);
				defender->Stun(2000, this);	// bash/kick stun is always 2 seconds
			}
		}
	}
	else
	{
		Log(Logs::Detail, Logs::Combat, "Stun failed. %d chance.", stun_chance);

		// This is a crude approximation of interrupt chance based on old EQ log parses
		int interruptChance = 100;

		if (IsNPC() && !IsPet())
		{
			if (GetLevel() < defenderLevel)
				interruptChance = 80;		// Daybreak recently confirmed this 80% chance
		}
		else if (defender->IsNPC())
		{
			int levelDiff = GetLevel() - defenderLevel;

			interruptChance += levelDiff * 10;

			if (defenderLevel > 65 && interruptChance > 0)
				interruptChance = 2;
			else if (defenderLevel > 55)
				interruptChance /= 2;
			else if (defenderLevel > 50)
				interruptChance = 3 * interruptChance / 4;
		}

		Log(Logs::Detail, Logs::Combat, "Non-stunning bash/kick interrupt roll: chance = %i", interruptChance);

		if (zone->random.Roll(interruptChance))
		{
			if (IsValidSpell(defender->casting_spell_id) && !spells[defender->casting_spell_id].uninterruptable)
			{
				Log(Logs::Detail, Logs::Combat, "Non-stunning bash/kick successfully interrupted spell");
				defender->InterruptSpell(SPELL_UNKNOWN, true);
			}
		}
	}
}

void Mob::DoBash(Mob* defender)
{
	if (!defender) {
		defender = GetTarget();
	}

	if (defender == this || !defender) {
		return;
	}

	uint16 skill_level = GetSkill(EQ::skills::SkillBash);
	bool is_trained = skill_level > 0 && skill_level < 253;
	int base = is_trained ? EQ::skills::GetSkillBaseDamage(EQ::skills::SkillBash, skill_level) : 1;
	int hate = base;
	bool shieldBash = false;

	if (is_trained && IsClient() && (GetClass() == Class::Warrior || GetClass() == Class::ShadowKnight || GetClass() == Class::Paladin || GetClass() == Class::Cleric)) {
		CastToClient()->CheckIncreaseSkill(EQ::skills::SkillBash, GetTarget(), zone->skill_difficulty[EQ::skills::SkillBash].difficulty[GetClass()]);

		EQ::ItemInstance* item = nullptr;
		item = CastToClient()->GetInv().GetItem(EQ::invslot::slotSecondary);

		if (item) {
			if (item->GetItem()->ItemType == EQ::item::ItemTypeShield) {
				shieldBash = true;
				int cap = base + GetLevel() / 5 + 2;
				base += item->GetItem()->AC;
				if (base > cap) {
					base = cap;
				}
				hate = base;
			}

			const EQ::ItemData *itm = item->GetItem();
			int32 fbash = GetFuriousBash(itm->Focus.Effect);
			if (fbash && content_service.IsTheShadowsOfLuclinEnabled()) { // Disable Focus Effect until Luclin
				fbash = zone->random.Int(1, fbash);
				hate = base * (100 + fbash) / 100;
			}
		}
	}

	int minDmg = 1;
	if (defender->IsImmuneToMelee(this, shieldBash ? EQ::invslot::slotSecondary : EQ::invslot::slotPrimary)) {
		minDmg = DMG_INVUL;
	}

	DoSpecialAttackDamage(defender, EQ::skills::SkillBash, base, minDmg, hate, DoAnimation::Slam);
}

void Mob::DoKick(Mob* defender)
{
	if (!defender)
		defender = GetTarget();
	if (defender == this || !defender)
		return;

	if (IsClient())
		CastToClient()->CheckIncreaseSkill(EQ::skills::SkillKick, GetTarget(), zone->skill_difficulty[EQ::skills::SkillKick].difficulty[GetClass()]);

	int base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillKick, GetSkill(EQ::skills::SkillKick));
	int minDmg = 1;
	if (defender->IsImmuneToMelee(this, EQ::invslot::slotFeet))
		minDmg = DMG_INVUL;
	DoSpecialAttackDamage(defender, EQ::skills::SkillKick, base, minDmg, 0, DoAnimation::Kick);
}

void NPC::DoBackstab(Mob* defender)
{
	if (!defender)
		defender = GetTarget();
	if (defender == this || !defender)
		return;

	if (!BehindMob(defender, GetX(), GetY()))
	{
		Attack(defender);
		return;
	}

	int base = 1;
	int minHit = 1;
	
	// this is not accurate, but provides a reasonable approximation
	if (IsPet() && GetPetType() != petCharmed)
	{
		minHit = CastToNPC()->GetMinDMG() + GetLevel();
		base = CastToNPC()->GetMaxDMG() * 15 / 10;
	}
	else
	{
		// Sony NPCs had unpredictable max backstab damages, in many cases higher level NPCs had oddly low BS caps
		// we don't have database fields for skills, so have to write some weird stuff here to make something kinda fit
		// the accuracy on this isn't great but it's reasonable
		base = (GetSkill(EQ::skills::SkillBackstab) * 2 + 200);
		base = base * (GetLevel() * 1000 / 145) / 1000;
		if (base < 5)
			base = 5;
		if (base > 220)
			base = 220;
		if (zone->GetZoneExpansion() == PlanesEQ)	// PoP NPCs had unusually low backstab damage
			base = base * 2 / 3;
	}

	// this part is accurate
	int db = GetDamageBonus();
	if (level >= 60)
		minHit = GetLevel() * 2 + db;
	else if (level > 50)
		minHit = GetLevel() * 3 / 2 + db;
	else
		minHit = GetLevel() + db;

	DoSpecialAttackDamage(defender, EQ::skills::SkillBackstab, base, minHit, DoAnimation::Piercing);
}

void Client::DoBackstab(Mob* defender)
{
	if (!defender)
		defender = GetTarget();
	if (defender == this || !defender)
		return;

	//make sure we have a proper weapon if we are a client.
	EQ::ItemInstance *wpn = GetInv().GetItem(EQ::invslot::slotPrimary);
	if (!wpn || (wpn->GetItem()->ItemType != EQ::item::ItemType1HPiercing))
	{
		Message_StringID(Chat::Red, StringID::BACKSTAB_WEAPON);
		return;
	}

	int stabs = 1;
	bool doMinHit = false;

	if (!BehindMob(defender, GetX(), GetY()))
	{
		if (!aabonuses.FrontalBackstabMinDmg)		// Luclin AA - Chaotic Stab
		{
			Attack(defender, EQ::invslot::slotPrimary);
			return;
		}

		doMinHit = true;
	}
	else
	{
		if (level > 54 && CastToClient()->CheckDoubleAttack())		// Chaotic Stab doesn't double, but it does skill-up
			stabs = 2;
	}

	CastToClient()->CheckIncreaseSkill(EQ::skills::SkillBackstab, defender, zone->skill_difficulty[EQ::skills::SkillBackstab].difficulty[GetClass()]);

	int minHit = GetLevel();
	int baseDamage = GetBaseDamage(defender, EQ::invslot::slotPrimary);
	// this formula was verified in the client code
	baseDamage = ((GetSkill(EQ::skills::SkillBackstab) * 0.02f) + 2.0f) * (float)baseDamage;
	int hate = baseDamage;

	if (defender->IsImmuneToMelee(this, EQ::invslot::slotPrimary))
	{
		minHit = DMG_INVUL;
	}
	else
	{
		if (doMinHit)
			baseDamage = 1;

		if (level >= 60)
			minHit = level * 2;
		else if (level > 50)
			minHit = level * 3 / 2;
		else
			minHit = level;
	}

	int assassinateDmg = TryAssassinate(defender, EQ::skills::SkillBackstab);
	if (assassinateDmg)
	{
		minHit = assassinateDmg;
		entity_list.MessageClose_StringID(this, false, 200, Chat::MeleeCrit, StringID::ASSASSINATES, GetName());
	}

	for (int i = 0; i < stabs; i++)
	{
		if (defender->GetHP() > 0 && GetTarget() && !HasDied())
			DoSpecialAttackDamage(defender, EQ::skills::SkillBackstab, baseDamage, minHit, hate, DoAnimation::Piercing);
	}

	if (defender->GetHP() > 0 && GetTarget() && !HasDied())
		TrySpellProc(nullptr, nullptr, defender);		// can client backstabs proc weapons?
}


bool Client::HasRacialAbility(const CombatAbility_Struct* ca_atk)
{
	if (ca_atk->m_atk == 100 && ca_atk->m_skill == EQ::skills::SkillBash){// SLAM - Bash without a shield equipped

		switch (GetRace())
		{
		case OGRE:
		case TROLL:
		case BARBARIAN:
			return  true;
		default:
			break;
		}

	}
	return false;
}

void Client::OPCombatAbility(const EQApplicationPacket *app)
{
	if(!GetTarget() || GetTarget() == this)
		return;

	//make sure were actually able to use such an attack.
	if(spellend_timer.Enabled() || IsFeared() || IsStunned() || IsMezzed() || DivineAura() || dead)
		return;

	CombatAbility_Struct* ca_atk = (CombatAbility_Struct*) app->pBuffer;

	/* Check to see if actually have skill or innate racial ability (like Ogres have Slam) */
	if (ca_atk->m_skill != EQ::skills::SkillThrowing && MaxSkill(static_cast<EQ::skills::SkillType>(ca_atk->m_skill)) <= 0 && !HasRacialAbility(ca_atk))
		return;

	if(GetTarget()->GetID() != ca_atk->m_target)
		return;	//invalid packet.

	if(!IsAttackAllowed(GetTarget()))
		return;

	//These two are not subject to the combat ability timer, as they
	//allready do their checking in conjunction with the attack timer
	//throwing weapons
	if(ca_atk->m_atk == EQ::invslot::slotRange) {
		if (ca_atk->m_skill == EQ::skills::SkillThrowing) {
			SetAttackTimer();
			ThrowingAttack(GetTarget());
			return;
		}
		//ranged attack (archery)
		if (ca_atk->m_skill == EQ::skills::SkillArchery) {
			SetAttackTimer();
			RangedAttack(GetTarget());
			return;
		}
		//could we return here? Im not sure is m_atk 11 is used for real specials
	}

	//check range for all these abilities, they are all close combat stuff
	if(!CombatRange(GetTarget()))
		return;

	if(!p_timers.Expired(&database, pTimerCombatAbility, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}

	// not sure what the '100' indicates..if ->m_atk is not used as 'slot' reference, then change SlotRange above back to '11'
	if (ca_atk->m_atk != 100 && GetClass() != Class::Monk)
		return;

	int reuseTime = 10;

	// Slam or possibly shield bash
	if (ca_atk->m_skill == EQ::skills::SkillBash && (GetRace() == Race::Ogre || GetRace() == Race::Troll || GetRace() == Race::Barbarian))
	{
		DoBash();
		reuseTime = BashReuseTime;
	}
	else
	{
		switch (GetClass())
		{
			case Class::Warrior:
			case Class::Paladin:
			case Class::ShadowKnight:
			case Class::Cleric:
			{
				if (ca_atk->m_skill == EQ::skills::SkillBash)
				{
					if (HasShieldEquiped() || HasBashEnablingWeapon())		// knight epics
					{
						DoBash();
						reuseTime = BashReuseTime;
					}
				}
				if (GetClass() != Class::Warrior) break;
			}
			case Class::Ranger:
			case Class::Beastlord:
				if (ca_atk->m_skill == EQ::skills::SkillKick)
				{
					DoKick();
					reuseTime = KickReuseTime;
				}
				break;
			case Class::Monk:
			{
				reuseTime = DoMonkSpecialAttack(GetTarget(), ca_atk->m_skill);
				break;
			}
			case Class::Rogue:
			{
				if (ca_atk->m_skill == EQ::skills::SkillBackstab)
				{
					DoBackstab();
					reuseTime = BackstabReuseTime;
				}
				break;
			}
		}
	}

	reuseTime = reuseTime * 100 / GetHaste() - 1;
	if (reuseTime > 0)
		p_timers.Start(pTimerCombatAbility, reuseTime);
}

//returns the reuse time in sec for the special attack used.
int Mob::DoMonkSpecialAttack(Mob* other, uint8 unchecked_type, bool fromWus)
{
	if(!other)
		return 0;

	int reuse = 10;
	int base = 1;
	int min_dmg = 1;
	EQ::skills::SkillType skill_type;	//to avoid casting... even though it "would work"
	int itemslot = EQ::invslot::slotFeet;
	DoAnimation anim_type = DoAnimation::None;
	switch(unchecked_type)
	{
		case EQ::skills::SkillFlyingKick:
		{
			skill_type = EQ::skills::SkillFlyingKick;
			base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillFlyingKick, GetSkill(EQ::skills::SkillFlyingKick));
			anim_type = DoAnimation::FlyingKick;
			reuse = FlyingKickReuseTime;
			min_dmg = GetLevel() * 4 / 5;
			break;
		}
		case EQ::skills::SkillDragonPunch:
		{
			skill_type = EQ::skills::SkillDragonPunch;
			base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillDragonPunch, GetSkill(EQ::skills::SkillDragonPunch));
			itemslot = EQ::invslot::slotHands;
			anim_type = DoAnimation::Slam;
			reuse = TailRakeReuseTime;
			break;
		}

		case EQ::skills::SkillEagleStrike:{
			skill_type = EQ::skills::SkillEagleStrike;
			base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillEagleStrike, GetSkill(EQ::skills::SkillEagleStrike));
			itemslot = EQ::invslot::slotHands;
			anim_type = DoAnimation::EagleStrike;
			reuse = EagleStrikeReuseTime;
			break;
		}

		case EQ::skills::SkillTigerClaw:
		{
			skill_type = EQ::skills::SkillTigerClaw;
			base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillTigerClaw, GetSkill(EQ::skills::SkillTigerClaw));
			itemslot = EQ::invslot::slotHands;
			anim_type = DoAnimation::TigerClaw;
			reuse = TigerClawReuseTime;
			break;
		}

		case EQ::skills::SkillRoundKick:
		{
			skill_type = EQ::skills::SkillRoundKick;
			base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillRoundKick, GetSkill(EQ::skills::SkillRoundKick));
			anim_type = DoAnimation::RoundKick;
			reuse = RoundKickReuseTime;
			break;
		}

		case EQ::skills::SkillKick:
		{
			skill_type = EQ::skills::SkillKick;
			base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillKick, GetSkill(EQ::skills::SkillKick));
			anim_type = DoAnimation::Kick;
			reuse = KickReuseTime;
			break;
		}
		default:
			Log(Logs::Detail, Logs::Attack, "Invalid special attack type %d attempted", unchecked_type);
			return(1000);
	}

	if (other->IsImmuneToMelee(this, itemslot))
		min_dmg = DMG_INVUL;
	
	int damage = DoSpecialAttackDamage(other, skill_type, base, min_dmg, 0, anim_type);

	if (IsClient())
	{
		CastToClient()->CheckIncreaseSkill(skill_type, other, zone->skill_difficulty[skill_type].difficulty[GetClass()]);

		if (damage > 0 && skill_type == EQ::skills::SkillDragonPunch && GetAA(aaDragonPunch) && !fromWus)
		{
			SpellFinished(SPELL_DRAGON_FORCE, GetTarget(), EQ::spells::CastingSlot::Item, 0, -1, spells[SPELL_DRAGON_FORCE].ResistDiff, true);		// 'Dragon Force I'
		}

		// Technique of Master Wu AA
		int wuchance = aabonuses.DoubleSpecialAttack;
		if (!fromWus && wuchance)
		{
			if (wuchance >= 100 || zone->random.Roll(wuchance))
			{
				int MonkSPA[4] = { EQ::skills::SkillFlyingKick, EQ::skills::SkillEagleStrike, EQ::skills::SkillTigerClaw, EQ::skills::SkillRoundKick };
				int extra = 1;
				// always 1/4 of the double attack chance, 25% at rank 5 (100/4)
				if (zone->random.Roll(wuchance / 4))
					extra++;

				// The "The spirit of Master Wu fills you!" string was added after PoP

				while (extra)
				{
					DoMonkSpecialAttack(GetTarget(), MonkSPA[zone->random.Int(0, 3)], true);
					extra--;
				}
			}
		}
	}

	return(reuse);
}

void Client::RangedAttack(Mob* other) {
	//conditions to use an attack checked before we are called
	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if (attack_timer.Enabled() && !attack_timer.Check(false))
	{
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Attack timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return;
	}
	if (ranged_timer.Enabled() && !ranged_timer.CheckKeepSynchronized())
	{
		// during high load, increased data rate or latency spikes the client and server timers can fall significantly out of phase and the client's ranged attacks will look 'early' to the server every second shot, resulting in repeated dry fires.
		// this leeway allows one extra attack to succeed and resynchronize the server timer but still prevents unchecked cheating from modified clients.
		if (ranged_attack_leeway_timer.Enabled() && !ranged_attack_leeway_timer.Check(false))
		{
			Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ranged timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
			return;
		}
		ranged_attack_leeway_timer.Start(5000);
	}
	else
	{
		if (ranged_attack_leeway_timer.Check(false))
		{
			ranged_attack_leeway_timer.Disable();
		}
	}
	const EQ::ItemInstance* RangeWeapon = m_inv[EQ::invslot::slotRange];

	//locate ammo
	int ammo_slot = EQ::invslot::slotAmmo;
	const EQ::ItemInstance* Ammo = m_inv[EQ::invslot::slotAmmo];

	if (!RangeWeapon || !RangeWeapon->IsClassCommon()) {
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Missing or invalid ranged weapon (%d) in slot %d", GetItemIDAt(EQ::invslot::slotRange), EQ::invslot::slotRange);
		Message(Chat::White, "Error: Rangeweapon: GetItem(%i)==0, you have no bow!", GetItemIDAt(EQ::invslot::slotRange));
		return;
	}
	if (!Ammo || !Ammo->IsClassCommon()) {
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Missing or invalid ammo item (%d) in slot %d", GetItemIDAt(EQ::invslot::slotAmmo), EQ::invslot::slotAmmo);
		Message(Chat::White, "Error: Ammo: GetItem(%i)==0, you have no ammo!", GetItemIDAt(EQ::invslot::slotAmmo));
		return;
	}

	AddWeaponAttackFatigue(RangeWeapon);

	const EQ::ItemData* RangeItem = RangeWeapon->GetItem();
	const EQ::ItemData* AmmoItem = Ammo->GetItem();

	if(RangeItem->ItemType != EQ::item::ItemTypeBow) {
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ranged item is not a bow. type %d.", RangeItem->ItemType);
		Message(Chat::White, "Error: Rangeweapon: Item %d is not a bow.", RangeWeapon->GetID());
		return;
	}
	if(AmmoItem->ItemType != EQ::item::ItemTypeArrow) {
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ammo item is not an arrow. type %d.", AmmoItem->ItemType);
		Message(Chat::White, "Error: Ammo: type %d != %d, you have the wrong type of ammo!", AmmoItem->ItemType, EQ::item::ItemTypeArrow);
		return;
	}

	Log(Logs::Detail, Logs::Combat, "Shooting %s with bow %s (%d) and arrow %s (%d)", GetTarget()->GetName(), RangeItem->Name, RangeItem->ID, AmmoItem->Name, AmmoItem->ID);

	//look for ammo in inventory if we only have 1 left...
	if(Ammo->GetCharges() == 1) {
		//first look for quivers
		int r;
		bool found = false;
		for(r = EQ::invslot::GENERAL_BEGIN; r <= EQ::invslot::GENERAL_END; r++) {
			const EQ::ItemInstance *pi = m_inv[r];
			if(pi == nullptr || !pi->IsType(EQ::item::ItemClassBag))
				continue;
			const EQ::ItemData* bagitem = pi->GetItem();
			if(!bagitem || bagitem->BagType != EQ::item::BagTypeQuiver)
				continue;

			//we found a quiver, look for the ammo in it
			int i;
			for (i = 0; i < bagitem->BagSlots; i++) {
				EQ::ItemInstance* baginst = pi->GetItem(i);
				if(!baginst)
					continue;	//empty
				if(baginst->GetID() == Ammo->GetID()) {
					//we found it... use this stack
					//the item wont change, but the instance does
					Ammo = baginst;
					ammo_slot = m_inv.CalcSlotId(r, i);
					found = true;
					Log(Logs::Detail, Logs::Combat, "Using ammo from quiver stack at slot %d. %d in stack.", ammo_slot, Ammo->GetCharges());
					break;
				}
			}
			if(found)
				break;
		}

		if(!found) {
			//if we dont find a quiver, look through our inventory again
			//not caring if the thing is a quiver.
			int32 aslot = m_inv.HasItem(AmmoItem->ID, 1, invWherePersonal);
			if (aslot != INVALID_INDEX) {
				ammo_slot = aslot;
				Ammo = m_inv[aslot];
				Log(Logs::Detail, Logs::Combat, "Using ammo from inventory stack at slot %d. %d in stack.", ammo_slot, Ammo->GetCharges());
			}
		}
	}
	float size_adj = 16.0f;
	if (other)
	{
		float delta_height = 0.6f * std::abs(GetSize() - other->GetSize());
		float other_adj = GetSize() + other->GetSize();
		if (other_adj < 16.0)
			other_adj = 16.0f;
		size_adj = delta_height + 2.0f + other_adj;
		if (size_adj > 75.0f)
			size_adj = 75.0f;
	}

	float range = RangeItem->Range + AmmoItem->Range + size_adj;
	Log(Logs::Detail, Logs::Combat, "Calculated bow range to be %.1f", range);
	
	// Archery range check is a client responsibility but a hacked client can potentially cheat and shoot from across the zone.
	// When the client is moving, the server's notion of the client's position is behind.  This range check is only for cheat/hack protection
	// but because of how the success/failure for range is decided by the client and not the server, refusing to fire the projectile here causes
	// the player to get 'dry fires' which still consumes their ammo (OP_DeleteCharge).  This leeway here still catches blatant cheating but should
	// cause less false positives.
	// Returning here from any of these checks will still eat the ammo because the client assumes that it will succeed and sends the delete together with the attack.
	float range_leeway = 80.0f; // leeway value found experimentally, 77ish units is the most distance I saw between client position updates while running with selo's
	range += range_leeway;

	range *= range;
	float dist = DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition());

	// allow some OOR shots through to account for moving players but if someone is cheating with a modified client or is unlucky enough to keep
	// triggering this check, it will still stop them
	static int oor_count = 0;
	const int oor_threshold = 2;
	if (dist > range || dist < (RuleI(Combat, MinRangedAttackDist) * RuleI(Combat, MinRangedAttackDist)))
	{
		oor_count++;
		if (dist > range) // too far
		{
			Log(Logs::Detail, Logs::Combat, "Ranged attack out of range... client should catch this. (%f > %f).\n", dist, range);
			if (oor_count > oor_threshold)
			{
				Message_StringID(Chat::Red, StringID::TARGET_OUT_OF_RANGE);//Client enforces range and sends the message, this is a backup just incase.
				return;
			}
		}
		else // too close
		{
			if (oor_count > oor_threshold)
				return;
		}
	}
	else if (oor_count > 0)
	{
		oor_count = 0; // this leeway allowance is only reset if they make a successful in-range attack to guard against potential exploiting
	}
	
	// don't shoot things that won't fight back - will still eat arrows
	if (other->IsNPC() && other->CastToNPC()->GetIgnoreDistance() > 349.0f
		&& dist > (other->CastToNPC()->GetIgnoreDistance() * other->CastToNPC()->GetIgnoreDistance()))
	{
		Log(Logs::Detail, Logs::Combat, "Ranged attack distance exceeds NPC ignore range\n");
		return;
	}

	if(!IsAttackAllowed(GetTarget()) ||
		IsCasting() ||
		IsSitting() ||
		(DivineAura() && !GetGM()) ||
		IsStunned() ||
		IsFeared() ||
		IsMezzed() ||
		(GetAppearance() == eaDead)){
		return; // will still eat arrows
	}

	//SendItemAnimation(GetTarget(), AmmoItem, SkillArchery);
	ProjectileAnimation(GetTarget(), AmmoItem->ID,true,-1,-1,-1,-1, EQ::skills::SkillArchery);

	DoArcheryAttackDmg(GetTarget());

	//EndlessQuiver AA base1 = 100% Chance to avoid consumption arrow.
	int ChanceAvoidConsume = aabonuses.ConsumeProjectile + itembonuses.ConsumeProjectile + spellbonuses.ConsumeProjectile;

	if (!ChanceAvoidConsume || (ChanceAvoidConsume < 100 && zone->random.Int(0,99) > ChanceAvoidConsume)){ 
		//Let Handle_OP_DeleteCharge handle the delete, to avoid item desyncs.
		//DeleteItemInInventory(ammo_slot, 1, false);
		Log(Logs::Detail, Logs::Combat, "Consumed one arrow from slot %d", ammo_slot);
	} else {
		Log(Logs::Detail, Logs::Combat, "Endless Quiver prevented ammo consumption.");
	}

	CheckIncreaseSkill(EQ::skills::SkillArchery, GetTarget(), zone->skill_difficulty[EQ::skills::SkillArchery].difficulty[GetClass()]);
	CommonBreakInvisNoSneak();
}

void Mob::DoArcheryAttackDmg(Mob* other)
{
	if (!CanDoSpecialAttack(other))
		return;

	// In our era, archery arrows would very often not hit NPCs that were in corners or against walls when fired from certain angles
	float z_angle = 1.0f;
	float height_diff = fabs(GetZ() - other->GetZ());
	float vector_x, vector_y, magnitude;

	// if player is somehow shooting from well above or below the NPC and near it X and Y wise then allow archery even if cornered
	if (height_diff > 15.0f)
	{
		vector_x = Distance(GetPosition(), other->GetPosition());
		vector_y = fabs(GetZ() - other->GetZ());
		magnitude = sqrtf(vector_x * vector_x + vector_y * vector_y);
		vector_x /= magnitude;
		z_angle = vector_x;
	}

	if (other->IsNPC() && RuleB(AlKabor, BlockProjectileWalls) && other->CastToNPC()->IsWalled())
	{
		float xy_angle = 0.0f;

		// There were a few known edge cases of archery hitting cornered mobs.  Hardcoding these here
		// Our NPC Z offsets are way off from Sony's, so can't do this purely algorithmically
		// There seems to have been something other than Z offsets involved because the blob case makes no sense as the angle was rather shallow
		if (other->GetRace() == Race::Goo && height_diff > 8.0f)	// blobs (for Vex Thal bosses)
			z_angle = 0.0f;
		else if (other->GetRace() == Race::MithanielMarr && Distance(GetPosition(), other->GetPosition()) < 45.0f)	// Mith Marr on pedastal (he doesn't end up on top of it server-side)
			z_angle = 0.0f;

		if (z_angle > 0.85f || z_angle < -0.85f)
		{
			vector_x = other->GetX() - GetX();
			vector_y = other->GetY() - GetY();
			magnitude = sqrtf(vector_x * vector_x + vector_y * vector_y);
			vector_x /= magnitude;
			vector_y /= magnitude;
			xy_angle = fabs(-other->CastToNPC()->GetWallAngle1(vector_x, vector_y));
			// allow the shot if fired parallel to the wall; deny if fired perpendicular
			if (xy_angle > 0.4f)
			{
				Log(Logs::Detail, Logs::Combat, "Poofing arrow; %s is against a wall(1)  xy_angle: %0.4f", other->GetName(), xy_angle);
				return;
			}

			xy_angle = fabs(-other->CastToNPC()->GetWallAngle2(vector_x, vector_y));
			if (xy_angle > 0.4f)
			{
				Log(Logs::Detail, Logs::Combat, "Poofing arrow; %s is against a wall(2)  xy_angle: %0.4f", other->GetName(), xy_angle);
				return;
			}
		}
	}

	if (other->IsNPC() && RuleB(AlKabor, BlockProjectileCorners) && other->CastToNPC()->IsCornered() && (z_angle > 0.85f || z_angle < -0.85f))
	{
		Log(Logs::Detail, Logs::Combat, "Poofing arrow; %s is cornered.  z_angle: %0.2f", other->GetName(), z_angle);
		return;
	}
	
	int baseDamage = GetBaseDamage(other, EQ::invslot::slotRange);		// this includes arrow damage
	int damage = 1;
	int32 hate = baseDamage;

	if (other->IsImmuneToMelee(this, EQ::invslot::slotRange))
	{
		damage = DMG_INVUL;		// immune
	}
	else
	{
		other->AvoidDamage(this, damage, true);

		if (damage > 0 && !other->AvoidanceCheck(this, EQ::skills::SkillArchery)) {
			Log(Logs::Detail, Logs::Combat, "Ranged attack missed %s.", other->GetName());
			damage = DMG_MISS;
		}
	}

	if (damage > 0)
	{
		Log(Logs::Detail, Logs::Combat, "Ranged attack hit %s.", other->GetName());

		baseDamage = static_cast<int>(RuleR(Combat, ArcheryBaseDamageBonus) * 100.0)*baseDamage / 100;

		// Archery Mastery AA
		baseDamage += baseDamage * aabonuses.ArcheryDamageModifier / 100;

		// Trueshot discipline
		baseDamage += baseDamage * spellbonuses.ArcheryDamageModifier / 100;

		damage = CalcMeleeDamage(other, baseDamage, EQ::skills::SkillArchery);

		Log(Logs::Detail, Logs::Combat, "Base Damage: %d, Damage: %d.", baseDamage, damage);
	}
	Beacon *beacon = new Beacon(this, 3000);

	entity_list.AddBeacon(beacon);
	if (beacon)
		beacon->Projectile(this, other, EQ::skills::SkillArchery, damage, hate, 0, 150.0f);

	//try proc on hits and misses
	if(GetTarget() && other && !other->HasDied())
		TryProcs(other, EQ::invslot::slotRange);
}

void NPC::RangedAttack(Mob* other)
{
	if (!other) {
		return;
	}

	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if((attack_timer.Enabled() && !attack_timer.Check(false)) || 
		(ranged_timer.Enabled() && !ranged_timer.Check())){
		Log(Logs::Detail, Logs::Combat, "Archery canceled. Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return;
	}

	if (!HasBowAndArrowEquipped() && !GetSpecialAbility(SpecialAbility::RangedAttack)) {
		return;
	}

	if (!CheckLosFN(other)) {
		return;
	}

	bool require_ammo = GetSpecialAbility(SpecialAbility::RangedAttack) >= 2;
	const EQ::ItemData* weapon = nullptr;
	const EQ::ItemData* ammo = nullptr;
	EQ::skills::SkillType skillInUse = EQ::skills::SkillArchery;

	if (equipment[EQ::invslot::slotPrimary] > 0) { // check primary slot for bow
		weapon = database.GetItem(equipment[EQ::invslot::slotPrimary]);
	}

	if (weapon && weapon->ItemType != EQ::item::ItemTypeBow) {
		weapon = nullptr;
	}

	if (!weapon && equipment[EQ::invslot::slotRange] > 0) { // check range slot for bow
		weapon = database.GetItem(equipment[EQ::invslot::slotRange]);
	}

	if (weapon && weapon->ItemType != EQ::item::ItemTypeBow && weapon->ItemType != EQ::item::ItemTypeSmallThrowing && weapon->ItemType != EQ::item::ItemTypeLargeThrowing) {
		weapon = NULL;
	}

	if (weapon && (weapon->ItemType == EQ::item::ItemTypeSmallThrowing || weapon->ItemType == EQ::item::ItemTypeLargeThrowing)) {
		ammo = weapon;
		skillInUse = EQ::skills::SkillThrowing;
	}

	if (!weapon || weapon && weapon->ItemType == EQ::item::ItemTypeBow)
	{
		ammo = database.GetItem(equipment[EQ::invslot::slotAmmo]);
		if (require_ammo &&
			(!ammo || (ammo && ammo->ItemType != EQ::item::ItemTypeArrow && ammo->ItemType != EQ::item::ItemTypeSmallThrowing))) {
			return;
		}

		if (!ammo || (ammo && ammo->ItemType != EQ::item::ItemTypeArrow)) { 
			ammo = database.GetItem(8005);
		}
	}

	float min_range = static_cast<float>(RuleI(Combat, MinRangedAttackDist));
	float max_range = 250.0f; // needs to be longer than 200(most spells)
	int16 damage_mod = 0;

	if (GetSpecialAbility(SpecialAbility::RangedAttack)) {
		//if we have SPECATK_RANGED_ATK set then we range attack without weapon or ammo
		int sa_min_range = GetSpecialAbilityParam(SpecialAbility::RangedAttack, 2); //Min Range of NPC attack
		int sa_max_range = GetSpecialAbilityParam(SpecialAbility::RangedAttack, 1); //Max Range of NPC attack
		damage_mod = GetSpecialAbilityParam(SpecialAbility::RangedAttack, 3);

		if (sa_max_range) {
			max_range = static_cast<float>(sa_max_range);
		}

		if (sa_min_range) {
			min_range = static_cast<float>(sa_min_range);
		}
	}

	Log(Logs::Detail, Logs::Combat, "Calculated bow range to be %.1f", max_range);
	max_range *= max_range;
	if (DistanceSquaredNoZ(m_Position, other->GetPosition()) > max_range) {
		return;
	}
	else if (DistanceSquaredNoZ(m_Position, other->GetPosition()) < (min_range * min_range)) {
		return;
	}
	

	if (!other || !IsAttackAllowed(other) ||
		IsCasting() ||
		DivineAura() ||
		IsStunned() ||
		IsFeared() ||
		IsMezzed() ||
		(GetAppearance() == eaDead)) {
		return;
	}

	skillInUse = static_cast<EQ::skills::SkillType>(GetRangedSkill());

	if (!ammo) {
		ammo = database.GetItem(8005);
	}

	if (ammo) {
		//SendItemAnimation(GetTarget(), ammo, SkillArchery);
		ProjectileAnimation(GetTarget(), ammo->ID, true, -1, -1, -1, -1, skillInUse);
	}

	FaceTarget(other);

	int damage = 1;
	int baseDamage = GetBaseDamage(other, EQ::invslot::slotRange);
	int damageBonus = GetDamageBonus();
	int hate = baseDamage + damageBonus / 2;

	other->AvoidDamage(this, damage, true);

	if (damage > 0 && !other->AvoidanceCheck(this, skillInUse))
	{
		damage = DMG_MISS;
		Log(Logs::Detail, Logs::Combat, "Ranged attack missed %s.", other->GetName());
	}

	if (damage > 0)
	{
		if (other->IsImmuneToMelee(this)) {
			damage = DMG_INVUL;		// immune
		}
		else {
			Log(Logs::Detail, Logs::Combat, "Ranged attack hit %s.", other->GetName());


			damage = damageBonus + CalcMeleeDamage(other, baseDamage, skillInUse);
			damage = static_cast<int>(static_cast<double>(damage) * RuleR(Combat, ArcheryNPCMultiplier));
			damage += damage * damage_mod / 100; //Damage modifier
		}
	}

	Beacon *beacon = new Beacon(this, 3000);

	entity_list.AddBeacon(beacon);
	if (beacon) {
		beacon->Projectile(this, other, skillInUse, damage, hate);
	}

	CommonBreakInvisible();

	if (ammo && GetSpecialAbility(SpecialAbility::RangedAttack) == 3)	{
		LootItem* sitem = GetItem(EQ::invslot::slotAmmo);
		RemoveItem(sitem, 1);
	}
}

void Client::ThrowingAttack(Mob* other, bool CanDoubleAttack) { //old was 51
	//conditions to use an attack checked before we are called

	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if((!CanDoubleAttack && (attack_timer.Enabled() && !attack_timer.Check(false)) || (ranged_timer.Enabled() && !ranged_timer.Check()))) {
		Log(Logs::Detail, Logs::Combat, "Throwing attack canceled. Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		// The server and client timers are not exact matches currently, so this would spam too often if enabled
		//Message(Chat::White, "Error: Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return;
	}

	int ammo_slot = EQ::invslot::slotRange;
	const EQ::ItemInstance* RangeWeapon = m_inv[EQ::invslot::slotRange];

	if (!RangeWeapon || !RangeWeapon->IsClassCommon()) {
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Missing or invalid ranged weapon (%d) in slot %d", GetItemIDAt(EQ::invslot::slotRange), EQ::invslot::slotRange);
		//Message(Chat::White, "Error: Rangeweapon: GetItem(%i)==0, you have nothing to throw!", GetItemIDAt(SlotRange));
		return;
	}

	AddWeaponAttackFatigue(RangeWeapon);

	const EQ::ItemData* item = RangeWeapon->GetItem();
	if(item->ItemType != EQ::item::ItemTypeLargeThrowing && item->ItemType != EQ::item::ItemTypeSmallThrowing) {
		Log(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ranged item %d is not a throwing weapon. type %d.", item->ItemType);
		//Message(Chat::White, "Error: Rangeweapon: GetItem(%i)==0, you have nothing useful to throw!", GetItemIDAt(SlotRange));
		return;
	}

	Log(Logs::Detail, Logs::Combat, "Throwing %s (%d) at %s", item->Name, item->ID, GetTarget()->GetName());
	float size_adj = 16.0f;
	if (other)
	{
		float delta_height = 0.6f * std::abs(GetSize() - other->GetSize());
		float other_adj = GetSize() + other->GetSize();
		if (other_adj < 16.0)
			other_adj = 16.0f;
		size_adj = delta_height + 2.0f + other_adj;
		if (size_adj > 75.0f)
			size_adj = 75.0f;
	}
	int range = item->Range + size_adj;
	Log(Logs::Detail, Logs::Combat, "Calculated throwing range to be %.1f", range);
	range *= range;
	float dist = DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition());
	if(dist > range) {
		Log(Logs::Detail, Logs::Combat, "Throwing attack out of range... client should catch this. (%f > %f).\n", dist, range);
		Message_StringID(Chat::Red, StringID::TARGET_OUT_OF_RANGE);//Client enforces range and sends the message, this is a backup just incase.
		return;
	}
	else if(dist < (RuleI(Combat, MinRangedAttackDist)*RuleI(Combat, MinRangedAttackDist))){
		return;
	}

	if(!IsAttackAllowed(GetTarget()) ||
		IsCasting() ||
		IsSitting() ||
		(DivineAura() && !GetGM()) ||
		IsStunned() ||
		IsFeared() ||
		IsMezzed() ||
		(GetAppearance() == eaDead)){
		return;
	}
	//send item animation, also does the throw animation
	ProjectileAnimation(GetTarget(), item->ID,true,-1,-1,-1,-1, EQ::skills::SkillThrowing);
	DoThrowingAttackDmg(GetTarget());

	//Let Handle_OP_DeleteCharge handle the delete, to avoid item desyncs. 
	//DeleteItemInInventory(ammo_slot, 1, false);

	CheckIncreaseSkill(EQ::skills::SkillThrowing, GetTarget(), zone->skill_difficulty[EQ::skills::SkillThrowing].difficulty[GetClass()]);
	CommonBreakInvisNoSneak();
}

void Mob::DoThrowingAttackDmg(Mob* other)
{
	if (!CanDoSpecialAttack(other))
		return;

	int damage = 1;

	int baseDamage = GetBaseDamage(other, EQ::invslot::slotRange);
	int hate = baseDamage;

	if (other->IsImmuneToMelee(this, EQ::invslot::slotRange))
	{
		damage = DMG_INVUL;		// immune
	}
	else
	{
		other->AvoidDamage(this, damage, true);
	}

	if (damage > 0 && !other->AvoidanceCheck(this, EQ::skills::SkillThrowing))
	{
		damage = DMG_MISS;
		Log(Logs::Detail, Logs::Combat, "Ranged attack missed %s.", other->GetName());
	}
	
	if (damage > 0)
	{
		Log(Logs::Detail, Logs::Combat, "Throwing attack hit %s.", other->GetName());

		damage = CalcMeleeDamage(other, baseDamage, EQ::skills::SkillThrowing);
	}

	Beacon *beacon = new Beacon(this, 3000);

	entity_list.AddBeacon(beacon);
	if (beacon)
		beacon->Projectile(this, other, EQ::skills::SkillThrowing, damage, hate);

	if(GetTarget() && other && (other->GetHP() > -10))
		TryProcs(other, EQ::invslot::slotRange);
}

void Mob::SendItemAnimation(Mob *to, const EQ::ItemData *item, EQ::skills::SkillType skillInUse) {
	auto outapp = new EQApplicationPacket(OP_Projectile, sizeof(Arrow_Struct));
	Arrow_Struct *as = (Arrow_Struct *) outapp->pBuffer;
	as->type = 1;
	as->src_x = GetX();
	as->src_y = GetY();
	as->src_z = GetZ();
	as->source_id = GetID();
	as->target_id = to->GetID();
	as->object_id = item->ID;

	as->effect_type = item->ItemType;
	as->skill = (uint8)skillInUse;

	strn0cpy(as->model_name, item->IDFile, 16);


	/*
		The angular field affects how the object flies towards the target.
		A low angular (10) makes it circle the target widely, where a high
		angular (20000) makes it go straight at them.

		The tilt field causes the object to be tilted flying through the air
		and also seems to have an effect on how it behaves when circling the
		target based on the angular field.

		Arc causes the object to form an arc in motion. A value too high will
	*/
	as->velocity = 4.0f;

	//these angle and tilt used together seem to make the arrow/knife throw as straight as I can make it

	as->launch_angle = CalculateHeadingToTarget(to->GetX(), to->GetY()) * 2.0f;
	as->tilt = 125.0f;
	as->arc = 50.0f;

	entity_list.QueueCloseClients(this, outapp, false, RuleI(Range, ProjectileAnims));
	safe_delete(outapp);
}

void Mob::ProjectileAnimation(Mob* to, int id, bool IsItem, float speed, float angle, float tilt, float arc, EQ::skills::SkillType skillInUse) {
	if (!to)
		return;

	const EQ::ItemData* item = nullptr;
	uint8 effect_type = 0;
	char name[16];
	uint8 behavior = 0;
	uint8 light = 0;
	uint8 yaw = 0;
	uint16 target_id = to->GetID();

	if (IsItem) {
		if (!id)
			item = database.GetItem(8005); // Arrow will be default
		else
			item = database.GetItem(id);
	}

	if(item)
	{
		effect_type = item->ItemType;
		behavior = 1;
		strn0cpy(name,item->IDFile, 16);
		if (item->ItemType == EQ::item::ItemTypeArrow) {
			tilt = 150;
		}
		else {
			// set angle of throwing items, if target is higher than us, to make it look better
			float destz = to->GetZ() - to->GetZOffset() + (to->GetSize() * 0.9f);
			float delta = destz - GetZ();
			float tilt_angle = 0.0f;
			if (delta > 0.0f)
			{
				float dist = DistanceNoZ(glm::vec3(GetX(), GetY(), GetZ()), glm::vec3(to->GetX(), to->GetY(), to->GetZ()));
				if (dist > 0.0f) {
					float ratio = delta / dist;
					tilt_angle = atanf(ratio) * 256.0f / 3.141592654f;
				}
			}
			tilt = tilt_angle;
		}
	}
	else
	{
		// 9 is also a valid type.
		tilt = 128;
		effect_type = 28;
		light = 8;
		//strn0cpy(name, "GENC00", 16); // this needs fixed, should be GENC00 or GENW00, but there appears to be a size param missing.
		strn0cpy(name, "IT68", 16); // this isn't correct but it shows a projectile flying at least
	}

	if(!speed || speed < 0)
		speed = 4.0;

	if (!angle || angle < 0) {
		angle = CalculateHeadingToTarget(to->GetX(), to->GetY()) * 2.0f;
	}
	if(!arc || arc < 0)
		if(IsItem)
			arc = 50;
		else
			arc = 0;

	if(GetID() == to->GetID())
	{
		yaw = 0.001;
		target_id = 0;
	}

	// See SendItemAnimation() for some notes on this struct
	auto outapp = new EQApplicationPacket(OP_Projectile, sizeof(Arrow_Struct));	
	Arrow_Struct *as = (Arrow_Struct *) outapp->pBuffer;
	as->type = 1;
	as->src_x = GetX();
	as->src_y = GetY();
	as->src_z = GetZ() + GetSize() * 0.4f;
	as->source_id = GetID();
	as->target_id = target_id;
	as->object_id = id;
	as->effect_type = effect_type;
	if (IsItem)
		as->skill = (uint8)skillInUse;
	else
		as->skill = 0;
	strn0cpy(as->model_name, name, 16);	
	as->velocity = speed;
	as->launch_angle = angle;
	as->tilt = tilt;
	as->arc = arc;
	as->light = light;
	as->behavior = behavior;
	as->pitch = 0;
	as->yaw = yaw;

	if(!IsItem)
		entity_list.QueueClients(this, outapp, false);
	else
		entity_list.QueueCloseClients(this, outapp, false, RuleI(Range, ProjectileAnims));

	safe_delete(outapp);

}

void NPC::DoClassAttacks(Mob *target)
{
	if (target == nullptr)
		return;

	bool taunt_time = taunt_timer.Check();
	bool ca_time = classattack_timer.Check(false);
	bool ka_time = knightattack_timer.Check(false);

	//only check attack allowed if we are going to do something
	if((taunt_time || ca_time || ka_time) && !IsAttackAllowed(target))
		return;

	if(ka_time)
	{
		int knightreuse = 1; //lets give it a small cooldown actually.
		switch(GetClass())
		{
			case Class::ShadowKnight: case Class::ShadowKnightGM:
			{
				CastSpell(SPELL_HARM_TOUCH, target->GetID());
				knightreuse = HarmTouchReuseTimeNPC;
				break;
			}
			case Class::Paladin: case Class::PaladinGM:
			{
				if(GetHPRatio() < 20)
				{
					CastSpell(SPELL_LAY_ON_HANDS, GetID());
					knightreuse = LayOnHandsReuseTimeNPC;
				} else {
					knightreuse = 2; //Check again in two seconds.
				}
				break;
			}
		}
		knightattack_timer.Start(knightreuse * 1000);
	}

	// pet taunt
	if (taunting && HasOwner() && !IsCharmedPet() && target->IsNPC() && taunt_time && CombatRange(target))
	{
		// pet taunt is 6 seconds with a chance at not working.  easily seen in logs
		// most times it's 6 seconds between 'taunting attacker master', sometimes 12, somtimes 18, etc
		// mage pets seem to taunt ~66%.  Enchanter pets ~40%
		int tauntChance = 66;
		if (this->GetOwner()->GetClass() == Class::Enchanter || this->GetOwner()->GetClass() == Class::Shaman)
			tauntChance = 40;

		if (zone->random.Roll(tauntChance))
		{
			this->GetOwner()->Message_StringID(Chat::PetResponse, StringID::PET_TAUNTING);
			Taunt(target->CastToNPC(), false);
		}
	}

	if(!ca_time)
		return;

	int reuse = 8;

	if (skills[EQ::skills::SkillBackstab])
	{
		reuse = BackstabReuseTime;
		DoBackstab(target);
	}
	else if (skills[EQ::skills::SkillFlyingKick])
	{
		reuse = DoMonkSpecialAttack(target, EQ::skills::SkillFlyingKick);
	}
	else if (skills[EQ::skills::SkillDragonPunch])
	{
		reuse = DoMonkSpecialAttack(target, EQ::skills::SkillDragonPunch);
	}
	else if (skills[EQ::skills::SkillEagleStrike])
	{
		reuse = DoMonkSpecialAttack(target, EQ::skills::SkillEagleStrike);
	}
	else if (skills[EQ::skills::SkillTigerClaw])
	{
		reuse = DoMonkSpecialAttack(target, EQ::skills::SkillTigerClaw);
	}
	else if (skills[EQ::skills::SkillRoundKick])
	{
		reuse = DoMonkSpecialAttack(target, EQ::skills::SkillRoundKick);
	}
	else if (skills[EQ::skills::SkillKick])
	{
		if (GetSkill(EQ::skills::SkillBash) && zone->random.Roll(66))			// NPCs with both skills parsed at a 2/3rds bash to kick rate
		{
			reuse = BashReuseTime;
			DoBash(target);
		}
		else
		{
			reuse = KickReuseTime;
			DoKick(target);
		}
	}
	else if (skills[EQ::skills::SkillBash])
	{
		reuse = BashReuseTime;
		DoBash(target);
	}

	reuse = reuse * 1000 / GetHaste() * 100;
	classattack_timer.Start(reuse);
}

// for charmed Client AI
void Client::DoClassAttacks(Mob *ca_target, uint16 skill, bool IsRiposte)
{
	if(!ca_target || ca_target == this)
		return;

	if(spellend_timer.Enabled() || IsFeared() || IsStunned() || IsMezzed() || DivineAura() || dead)
		return;

	if(!IsAttackAllowed(ca_target))
		return;

	//check range for all these abilities, they are all close combat stuff
	if(!CombatRange(ca_target)){
		return;
	}

	if(!IsRiposte && (!p_timers.Expired(&database, pTimerCombatAbility, false))) {
		return;
	}

	int ReuseTime = 0;
	int32 dmg = 0;
	uint16 skill_to_use = -1;

	if (skill == -1)
	{
		switch(GetClass()){
		case Class::Warrior:
		case Class::Ranger:
		case Class::Beastlord:
			skill_to_use = EQ::skills::SkillKick;
			break;
		case Class::ShadowKnight:
		case Class::Paladin:
			skill_to_use = EQ::skills::SkillBash;
			break;
		case Class::Monk:
			if(GetLevel() >= 30)
			{
				skill_to_use = EQ::skills::SkillFlyingKick;
			}
			else if(GetLevel() >= 25)
			{
				skill_to_use = EQ::skills::SkillDragonPunch;
			}
			else if(GetLevel() >= 20)
			{
				skill_to_use = EQ::skills::SkillEagleStrike;
			}
			else if(GetLevel() >= 10)
			{
				skill_to_use = EQ::skills::SkillTigerClaw;
			}
			else if(GetLevel() >= 5)
			{
				skill_to_use = EQ::skills::SkillRoundKick;
			}
			else
			{
				skill_to_use = EQ::skills::SkillKick;
			}
			break;
		case Class::Rogue:
			skill_to_use = EQ::skills::SkillBackstab;
			break;
		}
	}
	else
		skill_to_use = skill;

	if(skill_to_use == -1)
		return;

	if (skill_to_use == EQ::skills::SkillBash)
	{
		ReuseTime = BashReuseTime;
		DoBash();
	}
	else if (skill_to_use == EQ::skills::SkillKick)
	{
		ReuseTime = KickReuseTime;
		DoKick();
	}
	else if (skill_to_use == EQ::skills::SkillFlyingKick || skill_to_use == EQ::skills::SkillDragonPunch || skill_to_use == EQ::skills::SkillEagleStrike || skill_to_use == EQ::skills::SkillTigerClaw || skill_to_use == EQ::skills::SkillRoundKick)
	{
		ReuseTime = DoMonkSpecialAttack(ca_target, skill_to_use);
	}
	else if (skill_to_use == EQ::skills::SkillBackstab)
	{
		ReuseTime = BackstabReuseTime;
		DoBackstab(ca_target);
	}

	ReuseTime = ReuseTime * 100 / GetHaste();
	if (ReuseTime > 0 && !IsRiposte)
		p_timers.Start(pTimerCombatAbility, ReuseTime);
}

void Mob::Taunt(NPC* who, bool always_succeed, int32 overhate)
{
	if (who == nullptr)
		return;

	// charmed NPCs don't seem to taunt
	if(DivineAura() || (IsNPC() && IsCharmedPet()))
		return;

	// summoned pets don't seem to taunt level 50+ targets (might be a little lower than that even)
	if (IsPet() && who->GetLevel() >= 50)
		return;

	int levelDifference = GetLevel() - who->GetLevel();

	// Support for how taunt worked pre March 2001 - Can not taunt whites yellows or reds
	if (((RuleB(Combat, TauntOverLevel) == false) && (levelDifference <= 0)) || who->GetSpecialAbility(SpecialAbility::TauntImmunity)){
		//Message_StringID(Chat::SpellFailure,FAILED_TAUNT);
		return;
	}

	int tauntChance = 0;

	if (always_succeed)
	{
		tauntChance = 100;
	}
	else if (GetSkill(EQ::skills::SkillTaunt) > 0)
	{
		/* Taunt worked in two ways: a skill check for targets under level 60 and a non-skill chance
		 * for targets 60 and above if the player was also 60 and above.
		 * Before November 2000, mobs above level 50 were untauntable.  After that patch players could
		 * taunt mobs above 50 but still not equal to and above their level.
		 * After March 2001, Sony allowed level 60+ mobs to be tauntable, but they added this in a way
		 * that did not check the player's skill, which was seemingly an implementation oversight.
		 * 2001 logs show lower level players taunting mobs above their level, so that March patch
		 * seems to have allowed mobs above the player's level to be taunted at all levels, not just at 60.
		 * Old player tests and logs show about a 60% chance to succeed for level 60+ players taunting
		 * on white or lower targets, including greens.
		 * 
		 * See http://www.elitegamerslounge.com/home/soearchive/viewtopic.php?t=81156 
		 * https://web.archive.org/web/20050101085413/http://www.thesteelwarrior.org/forum/archive/index.php/t-8608.html
		 * https://web.archive.org/web/20041217221816/http://www.thesteelwarrior.org/forum/archive/index.php/t-1439.html
		 * https://web.archive.org/web/20060903140908/http://eqforums.station.sony.com/eq/board/message?board.id=warriorbalance&message.id=12360&page=1
		 */
		
		if (GetLevel() >= 60 && levelDifference < 0)
		{
			// This is likely accurate
			tauntChance = 60 + levelDifference * 10;
		}
		else
		{
			// This is not how Sony did it and is basically made-up.  Logs don't show sub level 60 player
			// taunt successes because most NPCs were made not to talk during Luclin and most old logs are PoP era,
			// so there is virtually no data to base this on and we must pull something out of our butts here
			tauntChance = 60 * GetSkill(EQ::skills::SkillTaunt) / (who->GetLevel() * 5 + 5);
			tauntChance += levelDifference * 5;

			if (tauntChance > 60)
				tauntChance = 60;
			else if (tauntChance < 10 && who->GetLevel() < 66)
				tauntChance = 10;
		}

		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillTaunt, who, zone->skill_difficulty[EQ::skills::SkillTaunt].difficulty[GetClass()]);

		Log(Logs::Detail, Logs::Aggro, "Attempting to Taunt with a chance of %i", tauntChance);
	}

	if (!who->IsOnHatelist(this))
	{
		who->CastToNPC()->AddToHateList(this, 20);
	}

	if (tauntChance > 0 && zone->random.Roll(tauntChance))
	{
		Mob *topHaterNoBonus = who->GetHateMost(false);
		Mob *topHater = who->GetHateMost(true);
		int32 myHate = who->GetNPCHate(this, false);
		int32 topHate = who->GetNPCHate(topHaterNoBonus, false);

		if (topHaterNoBonus != this)
		{
			myHate = topHate + 10;
			who->SetHate(this, myHate);
		}
		else if (topHater != this)
		{
			// we're top pre-bonus hate but not post-bonus hate
			who->AddHate(this, 10);
			myHate += 10;
		}

		if ((myHate - topHate) < overhate)
			who->SetHate(this, topHate + overhate);

		if (!always_succeed && who->CanTalk())		// Area Taunt doesn't make them speak
			who->Say_StringID(StringID::SUCCESSFUL_TAUNT,GetCleanName());
	}
//	else{
	//	Message_StringID(Chat::SpellFailure,FAILED_TAUNT);
//	}

	//else
	//	Message_StringID(Chat::SpellFailure,FAILED_TAUNT);
}


void Mob::InstillDoubt(Mob *who, int stage) 
{
	if (stage == 0) // begin instill, when button is first pressed a series of animations is played for 6 seconds
	{
		if (!who || !(who->IsNPC() || who->IsClient()) || who->IsUnTargetable())
			return;

		instillDoubtTargetID = who->GetID(); // save target for next stage
		instillDoubtStageTimer.Start(6000);
	}
	else if (stage == 1) // finish instill 6 seconds later when the animations are done
	{
		instillDoubtStageTimer.Disable();

		if (DivineAura())
			return;

		uint16 skillValue = GetSkill(EQ::skills::SkillIntimidation);
		int random = zone->random.Int(1, 350); // found in decompile credit to kicnlag
		if (random < skillValue)
		{
			// skill check success, now kick the target and fear it if the kick lands

			// make sure the target still exists
			Mob *instillTarget = entity_list.GetMob(instillDoubtTargetID);
			instillDoubtTargetID = 0; // clear saved target
			if (!instillTarget || instillTarget == this || instillTarget->HasDied())
				return;

			// skill up
			if (IsClient())
			{
				CastToClient()->CheckIncreaseSkill(EQ::skills::SkillIntimidation, instillTarget, zone->skill_difficulty[EQ::skills::SkillIntimidation].difficulty[GetClass()]);
			}

			// range check after skill up so can practice on anything if you just want skill points
			if (!CombatRange(instillTarget))
				return;

			if (!IsFacingMob(instillTarget))
			{
				Message_StringID(Chat::TooFarAway, StringID::CANT_HIT_THEM);
				return;
			}

			if (instillTarget->IsNPC() && instillTarget->CastToNPC()->GetSpecialAbility(SpecialAbility::AggroImmunity) || !IsAttackAllowed(instillTarget))
				return;

			// base damage is 2, this function scales it up some based on skill level
			int base = EQ::skills::GetSkillBaseDamage(EQ::skills::SkillIntimidation, GetSkill(EQ::skills::SkillIntimidation));
			int minDmg = 1;
			if (instillTarget->IsImmuneToMelee(this, EQ::invslot::slotFeet))
				minDmg = DMG_INVUL;
			int dmgDone = DoSpecialAttackDamage(instillTarget, EQ::skills::SkillIntimidation, base, minDmg, 0, DoAnimation::None);

			// the fear doesn't happen unless the kick dealt damage
			if(dmgDone > 0)
				SpellOnTarget(SPELL_FEAR, instillTarget);
		}
		else
		{
			// skill check failure

			Message_StringID(Chat::LightBlue, StringID::NOT_SCARING);
			instillDoubtTargetID = 0;
		}
	}
}

int Client::TryAssassinate(Mob* defender, EQ::skills::SkillType skillInUse)
{
	if (defender && (defender->GetBodyType() == BodyType::Humanoid) && !defender->IsClient() &&
		(skillInUse == EQ::skills::SkillBackstab || skillInUse == EQ::skills::SkillThrowing))
	{
		if (GetLevel() >= 60 && defender->GetLevel() <= 46)
		{
			if (zone->random.Roll(static_cast<double>(GetDEX()) / 3500.0))
				return 32000;
		}
	}

	return 0;
}

bool Mob::CanDoSpecialAttack(Mob *other) {
	//Make sure everything is valid before doing any attacks.
	if (!other) {
		SetTarget(nullptr);
		return false;
	}

	if(!GetTarget())
		SetTarget(other);

	if ((other == nullptr || ((IsClient() && CastToClient()->dead) || (other->IsClient() && other->CastToClient()->dead)) || HasDied() || (!IsAttackAllowed(other)))) {
		return false;
	}

	if(other->GetInvul() || other->GetSpecialAbility(SpecialAbility::MeleeImmunity))
		return false;

	return true;
}
