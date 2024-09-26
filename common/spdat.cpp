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


/*

	solar:	General outline of spell casting process

	1.
		a)	Client clicks a spell bar gem, ability, or item. client_process.cpp
		gets the op and calls CastSpell() with all the relevant info including
		cast time.

		b) NPC does CastSpell() from AI

	2.
		a)	CastSpell() determines there is a cast time and sets some state keeping
		flags to be used to check the progress of casting and finish it later.

		b)	CastSpell() sees there's no cast time, and calls CastedSpellFinished()
		Go to step 4.

	3.
		SpellProcess() notices that the spell casting timer which was set by
		CastSpell() is expired, and calls CastedSpellFinished()

	4.
		CastedSpellFinished() checks some timed spell specific things, like
		wether to interrupt or not, due to movement or melee. If successful
		SpellFinished() is called.

	5.
		SpellFinished() checks some things like LoS, reagents, target and
		figures out what's going to get hit by this spell based on its type.

	6.
		a)	Single target spell, SpellOnTarget() is called.

		b)	AE spell, Entity::AESpell() is called.

		c)	Group spell, Group::CastGroupSpell()/SpellOnTarget() is called as
		needed.

	7.
		SpellOnTarget() may or may not call SpellEffect() to cause effects to
		the target

	8.
		If this was timed, CastedSpellFinished() will restore the client's
		spell bar gems.


	Most user code should call CastSpell(), with a 0 casting time if needed,
	and not SpellFinished().

*/
#include "../common/eqemu_logsys.h" 

#include "classes.h"
#include "races.h"
#include "spdat.h"
#include "rulesys.h"
#include "eq_constants.h"

#ifndef WIN32
#include <stdlib.h>
#include "unix.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// spell property testing functions

bool IsTargetableAESpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && spells[spell_id].targettype == ST_AETarget)
		return true;

	return false;
}

bool IsSacrificeSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Sacrifice);
}

bool IsLichSpell(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return false;

	// Lich line valid through PoP. This should probably be a hash lookup or dynamic if scaled bigger than quarm PoP.
	if (spell_id == 641 || spell_id == 642 || spell_id == 643 || spell_id == 644 || spell_id == 1611 || spell_id == 1416 || spell_id == 2114 || spell_id == 3311)
		return true;

	return false;
}

bool IsLifetapSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && (spells[spell_id].targettype == ST_Tap || spells[spell_id].targettype == ST_TargetAETap))
		return true;

	return false;
}

bool IsMezSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Mez);
}

int16 GetBaseValue(uint16 spell_id, uint16 effect)
{
	for (int o = 0; o < EFFECT_COUNT; o++) 
	{
		uint32 tid = spells[spell_id].effectid[o];
		if (tid == effect)
		{
			return spells[spell_id].base[o];
		}
	}

	return 0;
}

bool IsStunSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Stun);
}

bool IsSummonSpell(uint16 spellid)
{
	for (int o = 0; o < EFFECT_COUNT; o++) {
		uint32 tid = spells[spellid].effectid[o];
		if (tid == SE_SummonPet || tid == SE_SummonItem || tid == SE_SummonPC)
			return true;
	}

	return false;
}

bool IsEvacSpell(uint16 spellid)
{
	return IsEffectInSpell(spellid, SE_Succor);
}

// All spells that do any form of HP reduction
bool IsDamageSpell(uint16 spellid)
{
	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		uint32 tid = spells[spellid].effectid[o];
		if ((tid == SE_CurrentHPOnce || tid == SE_CurrentHP) && spells[spellid].base[o] < 0)
			return true;
	}

	return false;
}

// Spells with 0 duration that apply instant damage.  This includes stuns.  Use IsPureNukeSpell() to exclude stuns
bool IsDirectDamageSpell(uint16 spellid)
{
	if (spells[spellid].buffduration > 0)
		return false;

	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		uint32 tid = spells[spellid].effectid[o];
		if ((tid == SE_CurrentHPOnce || tid == SE_CurrentHP) &&	spells[spellid].base[o] < 0)
			return true;
	}

	return false;
}

// Any spell with a direct/instant damage component/effect.  This includes many DoTs, many stuns, some debuffs with a damage component like Storm Comet
bool HasDirectDamageEffect(uint16 spellid)
{
	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		uint32 tid = spells[spellid].effectid[o];
		if (tid == SE_CurrentHPOnce && spells[spellid].base[o] < 0)
			return true;
		if (tid == SE_CurrentHP && spells[spellid].base[o] < 0 && spells[spellid].buffduration < 1)
			return true;
	}

	return false;
}

// Spells with a duration > 0 and apply damage every tick.  These spells may also have a direct damage component
bool IsDOTSpell(uint16 spellid)
{
	if (spells[spellid].buffduration < 1)
		return false;

	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		uint32 tid = spells[spellid].effectid[o];
		if (tid == SE_CurrentHP && spells[spellid].base[o] < 0)
			return true;
	}
	
	return false;
}

bool IsFearSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Fear);
}

bool IsCureSpell(uint16 spell_id)
{
	const SPDat_Spell_Struct &sp = spells[spell_id];

	bool CureEffect = false;

	for(int i = 0; i < EFFECT_COUNT; i++){
		if (sp.effectid[i] == SE_DiseaseCounter || sp.effectid[i] == SE_PoisonCounter 
			|| sp.effectid[i] == SE_CurseCounter)
			CureEffect = true;
	}

	if (CureEffect && IsBeneficialSpell(spell_id))
		return true;

	return false;
}

bool IsSlowSpell(uint16 spell_id)
{
	const SPDat_Spell_Struct &sp = spells[spell_id];

	for(int i = 0; i < EFFECT_COUNT; i++)
		if ((sp.effectid[i] == SE_AttackSpeed && sp.base[i] < 100))
			return true;

	return false;
}

bool IsHasteSpell(uint16 spell_id)
{
	const SPDat_Spell_Struct &sp = spells[spell_id];

	for(int i = 0; i < EFFECT_COUNT; i++)
		if(sp.effectid[i] == SE_AttackSpeed)
			return (sp.base[i] < 100);

	return false;
}

bool IsHarmonySpell(uint16 spell_id)
{
	//This will also return true for Pacify
	return (IsEffectInSpell(spell_id, SE_Harmony) || IsEffectInSpell(spell_id, SE_ChangeFrenzyRad));
}

bool IsPacifySpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_ChangeFrenzyRad);
}

bool IsLullSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Lull);
}

bool IsMemBlurSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_WipeHateList);
}

bool IsBardAOEDamageSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && GetSpellLevel(spell_id, BARD) < 100 && IsDamageSpell(spell_id) && spells[spell_id].aoerange > 0)
	{
		return true;
	}
	return false;
}

bool IsAEMemBlurSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && IsMemBlurSpell(spell_id) &&
		spells[spell_id].aoerange > 0)
		return true;

	return false;
}

bool IsCrowdControlSpell(uint16 spell_id)
{
	return (IsEffectInSpell(spell_id, SE_Harmony) || IsEffectInSpell(spell_id, SE_ChangeFrenzyRad) || IsEffectInSpell(spell_id, SE_Lull));
}

bool IsPercentalHealSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_PercentalHeal);
}

bool IsGroupOnlySpell(uint16 spell_id)
{
	return IsValidSpell(spell_id) && 
	(
		spells[spell_id].goodEffect == 2 || 
		spell_id == SPELL_TORPOR ||
		spell_id == SPELL_CALL_OF_THE_HERO ||
		IsEffectInSpell(spell_id, SE_InvisVsUndead) ||
		IsEffectInSpell(spell_id, SE_Invisibility) ||
		IsEffectInSpell(spell_id, SE_InvisVsAnimals) ||
		IsEffectInSpell(spell_id, SE_CancelMagic)
	);
}

bool IsBeneficialSpell(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return false;

	// You'd think just checking goodEffect flag would be enough?
	if (spells[spell_id].goodEffect == 1) {
		// If the target type is not ST_Self or ST_Pet or ST_GroupTeleport and is a SE_CancelMagic spell
		// it is not Beneficial
		SpellTargetType tt = spells[spell_id].targettype;
		if (tt != ST_Self && tt != ST_Pet && tt != ST_GroupTeleport &&
				IsEffectInSpell(spell_id, SE_CancelMagic))
			return false;

		// When our targettype is ST_Target, ST_AETarget, ST_Aniaml, ST_Undead, or ST_Pet
		// We need to check more things!
		if (tt == ST_Target || tt == ST_AETarget || tt == ST_Animal ||
				tt == ST_Undead || tt == ST_Pet) {
			// TODO: SpellAffectIndex is data for the older particle cloud system in the client, and not for spell logic.
			// all of this beneficial/detrimental stuff is not right, it's just full of hacks like this for specific spells
			// which masks some of the problems
			uint16 sai = spells[spell_id].SpellAffectIndex;

			// If the resisttype is magic and SpellAffectIndex is Calm/dispell sight
			// it's not beneficial.
			if (spells[spell_id].resisttype == RESIST_MAGIC) {
				if (sai == SAI_Calm || sai == SAI_Dispell_Sight || sai == SAI_Calm_Song)
					return false;
			} else {
				// If the resisttype is not magic and spell is Harmony
				// It's not beneficial
				if (sai == SAI_Calm && IsEffectInSpell(spell_id, SE_Harmony))
					return false;
			}
		}
	}

	// And finally, if goodEffect is not 0 or if it's a group spell it's beneficial
	return spells[spell_id].goodEffect != 0 || IsGroupSpell(spell_id);
}

bool IsDetrimentalSpell(uint16 spell_id)
{
	return !IsBeneficialSpell(spell_id);
}

bool IsNeutralSpell(uint16 spell_id)
{
	//These are spells that can be cast on any target.
	return (IsBindSightSpell(spell_id) || (IsMemBlurSpell(spell_id) && !IsMezSpell(spell_id)) || IsLuclinPortSpell(spell_id) || IsHarmonySpell(spell_id)
		|| spells[spell_id].effectid[0] == SE_Teleport2); // some PoP NPC teleports are flagged beneficial
}

bool IsInvulnerabilitySpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_DivineAura);
}

bool IsCHDurationSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_CompleteHeal);
}

bool IsPoisonCounterSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_PoisonCounter);
}

bool IsDiseaseCounterSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_DiseaseCounter);
}

bool IsSummonItemSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_SummonItem);
}

bool IsSummonSkeletonSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_NecPet);
}

bool IsSummonPetSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_SummonPet) ||
			IsEffectInSpell(spell_id, SE_SummonBSTPet))
		return true;

	return false;
}

bool IsSummonPCSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_SummonPC);
}

bool IsCharmSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Charm);
}

bool IsDireCharmSpell(uint16 spell_id)
{
	if(IsEffectInSpell(spell_id, SE_Charm) && spells[spell_id].buffdurationformula == 50)
		return true;
	else
		return false;
}

bool IsRootSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Root);
}

bool IsBlindSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Blind);
}

bool IsEffectHitpointsSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_CurrentHP);
}

bool IsReduceCastTimeSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_IncreaseSpellHaste);
}

bool IsIncreaseDurationSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_IncreaseSpellDuration);
}

bool IsReduceManaSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_ReduceManaCost);
}

bool IsExtRangeSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_IncreaseRange);
}

bool IsImprovedHealingSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_ImprovedHeal);
}

bool IsHealingSpell(uint16 spellid)
{
	if (IsEffectInSpell(spellid, SE_HealOverTime) || IsEffectInSpell(spellid, SE_PercentalHeal) || IsEffectInSpell(spellid, SE_CompleteHeal))
		return true;

	for (int o = 0; o < EFFECT_COUNT; o++) {
		uint32 tid = spells[spellid].effectid[o];
		if ((tid == SE_CurrentHPOnce || tid == SE_CurrentHP) &&
			spells[spellid].base[o] > 0)
		{
			return true;
		}
	}

	return false;
}

bool IsImprovedDamageSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_ImprovedDamage);
}

bool IsBindSightSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_BindSight);
}

bool IsAEDurationSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) &&
			(spells[spell_id].targettype == ST_AETarget || spells[spell_id].targettype == ST_UndeadAE) &&
			spells[spell_id].AEDuration != 0)
		return true;

	return false;
}

// Direct Damage spells without any other component. i.e. stuns are excluded
bool IsPureNukeSpell(uint16 spell_id)
{
	int i, effect_count = 0;

	if (!IsValidSpell(spell_id))
		return false;

	for (i = 0; i < EFFECT_COUNT; i++)
		if (!IsBlankSpellEffect(spell_id, i))
			effect_count++;

	if (effect_count == 1 && IsEffectInSpell(spell_id, SE_CurrentHP) &&
			spells[spell_id].buffduration == 0 && IsDirectDamageSpell(spell_id))
		return true;

	return false;
}

bool IsAENukeSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && IsPureNukeSpell(spell_id) &&
			spells[spell_id].aoerange > 0)
		return true;

	return false;
}

bool IsPBAENukeSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && IsPureNukeSpell(spell_id) &&
			spells[spell_id].aoerange > 0 && spells[spell_id].targettype == ST_AECaster)
		return true;

	return false;
}

bool IsAERainNukeSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && IsPureNukeSpell(spell_id) &&
			spells[spell_id].aoerange > 0 && spells[spell_id].AEDuration > 1000)
		return true;

	return false;
}

bool IsPureDispelSpell(uint16 spell_id)
{
	int i;

	if (!IsValidSpell(spell_id))
		return false;

	for (i = 0; i < EFFECT_COUNT; i++)
		if (!IsBlankSpellEffect(spell_id, i) && spells[spell_id].effectid[i] != SE_CancelMagic)
			return false;

	return true;
}

bool IsDispelSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && IsEffectInSpell(spell_id, SE_CancelMagic))
		return true;

	return false;
}

bool IsPartialCapableSpell(uint16 spell_id)
{
	if (spells[spell_id].no_partial_resist)
		return false;
	
	// spell uses 600 (partial) scale if first effect is damage, else it uses 200 scale.
	// this includes DoTs.  no_partial_resist excludes spells like necro snares
	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		uint16 tid = spells[spell_id].effectid[o];

		if (IsBlankSpellEffect(spell_id, o))
			continue;

		if ((tid == SE_CurrentHPOnce || tid == SE_CurrentHP ) && spells[spell_id].base[o] < 0)
			return true;

		return false;
	}

	return false;
}

bool IsResistableSpell(uint16 spell_id)
{
	// for now only detrimental spells are resistable. later on i will
	// add specific exceptions for the beneficial spells that are resistable
	// Torven: dispels do not have a MR check; they have a different check that is entirely level based
	if (IsDetrimentalSpell(spell_id) && !IsPureDispelSpell(spell_id) && 
		!IsAllianceSpellLine(spell_id) && spells[spell_id].resisttype != RESIST_NONE)
	{
		return true;
	}

	Log(Logs::Detail, Logs::Spells, "Spell %i is unresistable.", spell_id);
	return false;
}

// checks if this spell affects your group
bool IsGroupSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) &&
			(spells[spell_id].targettype == ST_AEBard ||
			 spells[spell_id].targettype == ST_Group ||
			 spells[spell_id].targettype == ST_GroupTeleport))
		return true;

	return false;
}

// checks if this spell can be targeted
bool IsTGBCompatibleSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) &&
			(!IsDetrimentalSpell(spell_id) && spells[spell_id].buffduration != 0 &&
			 !IsBardSong(spell_id) && !IsEffectInSpell(spell_id, SE_Illusion) && !IsGroupOnlySpell(spell_id)))
		return true;

	return false;
}

bool IsBardSong(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && spells[spell_id].bardsong)
		return true;

	return false;
}

bool IsEffectInSpell(uint16 spellid, int effect)
{
	int j;

	if (!IsValidSpell(spellid))
		return false;

	for (j = 0; j < EFFECT_COUNT; j++)
		if (spells[spellid].effectid[j] == effect)
			return true;

	return false;
}

// arguments are spell id and the index of the effect to check.
// this is used in loops that process effects inside a spell to skip
// the blanks
bool IsBlankSpellEffect(uint16 spellid, int effect_index)
{
	int effect, base, formula;

	effect = spells[spellid].effectid[effect_index];
	base = spells[spellid].base[effect_index];
	formula = spells[spellid].formula[effect_index];

	// SE_CHA is "spacer"
	// SE_Stacking* are also considered blank where this is used
	if (effect == SE_Blank || (effect == SE_CHA && base == 0 && formula == 100) ||
			effect == SE_StackingCommand_Block || effect == SE_StackingCommand_Overwrite)
		return true;

	return false;
}

bool IsSpellEffectBlocked(uint16 sp1, uint16 sp2, uint16 effectid, int value)
{
	for (int j = 0; j < EFFECT_COUNT; j++)
	{
		if (spells[sp1].effectid[j] == SE_StackingCommand_Block && 
			spells[sp1].base[j] == effectid)
		{
			int blocked_effect = spells[sp1].base[j];
			int blocked_slot = spells[sp1].formula[j] - 201;
			int blocked_below_value = spells[sp1].max[j];
			
			if (spells[sp2].effectid[blocked_slot] == blocked_effect)
			{
				if (value < blocked_below_value)
				{
					Log(Logs::General, Logs::Spells, "Spell %d blocks one or more effects found in spell %d.", sp1, sp2);
					return true;
				}
					
			}
		}
	}

	return false;
}

// checks some things about a spell id, to see if we can proceed
bool IsValidSpell(uint16 spellid)
{
	if (SPDAT_RECORDS > 0 && spellid != 0 && spellid != 1 &&
			spellid != SPELL_UNKNOWN && spellid < SPDAT_RECORDS && spells[spellid].player_1[0])
		return true;

	return false;
}

// returns the lowest level of any caster which can use the spell
int GetMinLevel(uint16 spell_id)
{
	return spells[spell_id].min_castinglevel;
}

int GetSpellLevel(uint16 spell_id, int classa)
{
	if (classa >= PLAYER_CLASS_COUNT)
		return 255;

	const SPDat_Spell_Struct &spell = spells[spell_id];
	return spell.classes[classa - 1];
}

// this will find the first occurrence of effect. this is handy
// for spells like mez and charm, but if the effect appears more than once
// in a spell this will just give back the first one.
int GetSpellEffectIndex(uint16 spell_id, int effect)
{
	int i;

	if (!IsValidSpell(spell_id))
		return -1;

	for (i = 0; i < EFFECT_COUNT; i++)
		if (spells[spell_id].effectid[i] == effect)
			return i;

	return -1;
}

// returns the level required to use the spell if that class/level
// can use it, 0 otherwise
// note: this isn't used by anything right now
int CanUseSpell(uint16 spellid, int classa, int level)
{
	int level_to_use;

	if (!IsValidSpell(spellid) || classa >= PLAYER_CLASS_COUNT)
		return 0;

	level_to_use = spells[spellid].classes[classa - 1];

	if (level_to_use && level_to_use != 255 && level >= level_to_use)
		return level_to_use;

	return 0;
}

int32 CalculatePoisonCounters(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return 0;

	return spells[spell_id].poison_counters;
}

int32 CalculateDiseaseCounters(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return 0;

	return spells[spell_id].disease_counters;
}

int32 CalculateCurseCounters(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return 0;

	return spells[spell_id].curse_counters;
}

int32 CalculateCounters(uint16 spell_id)
{
	int32 counter = CalculatePoisonCounters(spell_id);
	if (counter != 0)
		return counter;

	counter = CalculateDiseaseCounters(spell_id);
	if (counter != 0)
		return counter;

	counter = CalculateCurseCounters(spell_id);
	if (counter != 0)
		return counter;

	return counter;
}

bool IsCombatSkill(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return false;
	
	//Check if Discipline
	if ((spells[spell_id].mana == 0 &&	(spells[spell_id].EndurCost || spells[spell_id].EndurUpkeep)))			
		return true;

	return false;
}

bool IsResurrectionEffects(uint16 spell_id)
{
	if(IsValidSpell(spell_id) && spell_id == SPELL_RESURRECTION_EFFECTS)
		return true;

	return false;
}

bool IsRuneSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id))
		for (int i = 0; i < EFFECT_COUNT; i++)
			if (spells[spell_id].effectid[i] == SE_Rune)
				return true;

	return false;
}

bool IsMagicRuneSpell(uint16 spell_id)
{
	if(IsValidSpell(spell_id))
		for(int i = 0; i < EFFECT_COUNT; i++)
			if(spells[spell_id].effectid[i] == SE_AbsorbMagicAtt)
				return true;

	return false;
}

bool IsManaTapSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && spells[spell_id].manatapspell)
		return true;

	return false;
}

bool IsAllianceSpellLine(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_AddFaction))
		return true;

	return false;
}

bool IsDeathSaveSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_DeathSave))
		return true;

	return false;
}

// Deathsave spells with base of 1 are partial
bool IsPartialDeathSaveSpell(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return false;

	for (int i = 0; i < EFFECT_COUNT; i++)
		if (spells[spell_id].effectid[i] == SE_DeathSave &&
				spells[spell_id].base[i] == 1)
			return true;

	return false;
}

// Deathsave spells with base 2 are "full"
bool IsFullDeathSaveSpell(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return false;

	for (int i = 0; i < EFFECT_COUNT; i++)
		if (spells[spell_id].effectid[i] == SE_DeathSave &&
				spells[spell_id].base[i] == 2)
			return true;

	return false;
}

bool IsShadowStepSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_ShadowStep))
		return true;

	return false;
}

bool IsSuccorSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_Succor))
		return true;

	return false;
}

bool IsTeleportSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_Teleport))
		return true;

	return false;
}

bool IsGateSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_Gate))
		return true;

	return false;
}

bool IsPlayerIllusionSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_Illusion) && spells[spell_id].targettype == ST_Self)
	{
		if (spell_id == SPELL_MINOR_ILLUSION || spell_id == SPELL_ILLUSION_TREE)
			return false;
		return true;
	}

	return false;
}

int GetSpellEffectDescNum(uint16 spell_id)
{
	if (IsValidSpell(spell_id))
		return spells[spell_id].effectdescnum;

	return -1;
}

DmgShieldType GetDamageShieldType(uint16 spell_id, int32 DSType)
{
	// If we have a DamageShieldType for this spell from the damageshieldtypes table, return that,
	// else, make a guess, based on the resist type. Default return value is DS_THORNS
	if (IsValidSpell(spell_id)) {
		Log(Logs::Detail, Logs::Spells, "DamageShieldType for spell %i (%s) is %X\n", spell_id,
			spells[spell_id].name, spells[spell_id].DamageShieldType);

		if (spells[spell_id].DamageShieldType)
			return (DmgShieldType) spells[spell_id].DamageShieldType;

		switch (spells[spell_id].resisttype) {
			case RESIST_COLD:
				return DS_TORMENT;
			case RESIST_FIRE:
				return DS_BURN;
			case RESIST_DISEASE:
				return DS_DECAY;
			default:
				return DS_THORNS;
		}
	}

	else if (DSType)
		return (DmgShieldType) DSType;

	return DS_THORNS;
}

int32 GetSpellResistType(uint16 spell_id)
{
	return spells[spell_id].resisttype;
}

int32 GetSpellTargetType(uint16 spell_id)
{
	return (int32)spells[spell_id].targettype;
}

bool IsHealOverTimeSpell(uint16 spell_id)
{
	if (IsEffectInSpell(spell_id, SE_HealOverTime) && !IsGroupSpell(spell_id))
		return true;

	return false;
}

bool IsCompleteHealSpell(uint16 spell_id)
{
	if (spell_id == 13 || IsEffectInSpell(spell_id, SE_CompleteHeal) ||
			(IsPercentalHealSpell(spell_id) && !IsGroupSpell(spell_id)))
		return true;

	return false;
}

bool IsFastHealSpell(uint16 spell_id)
{
	const int MaxFastHealCastingTime = 2000;

	if (spells[spell_id].cast_time <= MaxFastHealCastingTime &&
			spells[spell_id].effectid[0] == 0 && spells[spell_id].base[0] > 0 &&
			!IsGroupSpell(spell_id))
		return true;

	return false;
}

bool IsVeryFastHealSpell(uint16 spell_id)
{
	const int MaxFastHealCastingTime = 1000;

	if (spells[spell_id].cast_time <= MaxFastHealCastingTime &&
			spells[spell_id].effectid[0] == 0 && spells[spell_id].base[0] > 0 &&
			!IsGroupSpell(spell_id))
		return true;

	return false;
}

bool IsRegularSingleTargetHealSpell(uint16 spell_id)
{
	if(spells[spell_id].effectid[0] == 0 && spells[spell_id].base[0] > 0 &&
			spells[spell_id].targettype == ST_Target && spells[spell_id].buffduration == 0 &&
			!IsFastHealSpell(spell_id) && !IsCompleteHealSpell(spell_id) &&
			!IsHealOverTimeSpell(spell_id) && !IsGroupSpell(spell_id))
		return true;

	return false;
}

bool IsRegularGroupHealSpell(uint16 spell_id)
{
	if (IsGroupSpell(spell_id) && !IsCompleteHealSpell(spell_id) && !IsHealOverTimeSpell(spell_id))
		return true;

	return false;
}

bool IsGroupCompleteHealSpell(uint16 spell_id)
{
	if (IsGroupSpell(spell_id) && IsCompleteHealSpell(spell_id))
		return true;

	return false;
}

bool IsGroupHealOverTimeSpell(uint16 spell_id)
{
	if( IsGroupSpell(spell_id) && IsHealOverTimeSpell(spell_id) && spells[spell_id].buffduration < 10)
		return true;

	return false;
}

bool IsDebuffSpell(uint16 spell_id)
{
	if (IsBeneficialSpell(spell_id) || IsEffectHitpointsSpell(spell_id) || IsStunSpell(spell_id) ||
			IsMezSpell(spell_id) || IsCharmSpell(spell_id) || IsSlowSpell(spell_id) ||
			IsEffectInSpell(spell_id, SE_Root) || IsEffectInSpell(spell_id, SE_CancelMagic) ||
			IsEffectInSpell(spell_id, SE_MovementSpeed) || IsFearSpell(spell_id) || IsEffectInSpell(spell_id, SE_InstantHate))
		return false;
	else
		return true;
}

bool IsResistDebuffSpell(uint16 spell_id)
{
	if ((IsEffectInSpell(spell_id, SE_ResistFire) || IsEffectInSpell(spell_id, SE_ResistCold) ||
				IsEffectInSpell(spell_id, SE_ResistPoison) || IsEffectInSpell(spell_id, SE_ResistDisease) ||
				IsEffectInSpell(spell_id, SE_ResistMagic) || IsEffectInSpell(spell_id, SE_ResistAll)) && !IsBeneficialSpell(spell_id))
		return true;
	else
		return false;
}

bool IsSelfConversionSpell(uint16 spell_id)
{
	if(!IsValidSpell(spell_id))
		return false;

	if (spells[spell_id].targettype == ST_Self && IsEffectInSpell(spell_id, SE_CurrentMana) &&
			IsEffectInSpell(spell_id, SE_CurrentHP) && spells[spell_id].base[GetSpellEffectIndex(spell_id, SE_CurrentMana)] > 0 &&
			spells[spell_id].base[GetSpellEffectIndex(spell_id, SE_CurrentHP)] < 0)
		return true;
	else
		return false;
}

// returns true for both detrimental and beneficial buffs
bool IsBuffSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id)) {
		// duration formula 0 always is no buff
		if (spells[spell_id].buffdurationformula == 0)
			return false;

		if (spells[spell_id].buffduration || spells[spell_id].buffdurationformula)
			return true;
	}

	return false;
}

bool IsSuspendableSpell(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && spells[spell_id].suspendable)
		return true;

	return false;
}

uint32 GetMorphTrigger(uint32 spell_id)
{
	for (int i = 0; i < EFFECT_COUNT; ++i)
		if (spells[spell_id].effectid[i] == SE_CastOnFadeEffect)
			return spells[spell_id].base[i];

	return 0;
}

bool IsCastonFadeDurationSpell(uint16 spell_id)
{
	for (int i = 0; i < EFFECT_COUNT; ++i) {
		if (spells[spell_id].effectid[i] == SE_CastOnFadeEffect){

		return true;
		}
	}
	return false;
}

bool IsRegeantFocus(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_ReduceReagentCost);
}

bool IsBoltSpell(uint16 spell_id)
{
	if(IsValidSpell(spell_id) && spells[spell_id].spell_category == 13)
		return true;

	return false;
}

bool RequiresComponents(uint16 spell_id)
{
	if(IsValidSpell(spell_id))
	{
		for (int t_count = 0; t_count < 4; t_count++) 
		{
			int32 component = IsBardSong(spell_id) ? -1 : spells[spell_id].components[t_count];
			int32 focuscomponent = spells[spell_id].NoexpendReagent[t_count];

			if (component == -1 && focuscomponent == -1)
				continue;

			if(component > 0 || focuscomponent > 0)
				return true;
		}
	}

	return false;
}

bool DetrimentalSpellAllowsRest(uint16 spell_id)
{
	if (IsValidSpell(spell_id))
		return spells[spell_id].AllowRest;

	return false;
}

int32 GetFuriousBash(uint16 spell_id)
{
	if (!IsValidSpell(spell_id))
		return 0;

	if (RuleB(AlKabor, EnableEraItemRules))
	{
		//Disable Focus Effects before we enter Luclin.
		if (RuleR(World, CurrentExpansion) < (float)ExpansionEras::LuclinEQEra)
			return 0;
	}

	for (int i = 0; i < EFFECT_COUNT; ++i)
		if (spells[spell_id].effectid[i] == SE_SpellHateMod)
			return spells[spell_id].base[i];

		return 0;
}

bool IsShortDurationBuff(uint16 spell_id)
{
	if (IsValidSpell(spell_id) && spells[spell_id].short_buff_box != 0)
		return true;

	return false;
}

bool IsSpellUsableThisZoneType(uint16 spell_id, uint8 zone_type)
{
	//check if spell can be cast in any zone (-1 or 255), then if spell zonetype matches zone's zonetype
	// || spells[spell_id].zonetype == 255 comparing signed 8 bit int to 255 is always false
	if (IsValidSpell(spell_id) && (spells[spell_id].zonetype == -1 ||
				spells[spell_id].zonetype == zone_type))
		return true;

	return false;
}

const char* GetSpellName(int16 spell_id)
{
    return spells[spell_id].name;
}

bool IsRacialIllusion(uint16 spell_id)
{
	for (int i = 0; i < EFFECT_COUNT; i++)
	{
		if(spells[spell_id].effectid[i] == SE_Illusion && (spells[spell_id].base[i] == HUMAN || 
			spells[spell_id].base[i] == BARBARIAN || spells[spell_id].base[i] == ERUDITE || 
			spells[spell_id].base[i] == WOOD_ELF || spells[spell_id].base[i] == HIGH_ELF || 
			spells[spell_id].base[i] == DARK_ELF || spells[spell_id].base[i] == HALF_ELF || 
			spells[spell_id].base[i] == DWARF || spells[spell_id].base[i] == TROLL || 
			spells[spell_id].base[i] == OGRE || spells[spell_id].base[i] == HALFLING || 
			spells[spell_id].base[i] == GNOME || spells[spell_id].base[i] == IKSAR || 
			spells[spell_id].base[i] == VAHSHIR))
			return true;
	}
	return false;
}

bool IsCorpseSummon(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_SummonCorpse);
}

bool IsSpeedBuff(uint16 spell_id)
{
	if(IsBeneficialSpell(spell_id))
	{
		for (int i = 0; i < EFFECT_COUNT; i++)
		{
			if(spells[spell_id].effectid[i] == SE_MovementSpeed && spells[spell_id].base[i] != 0) // Stoicism, Torpor are negative
				return true;
		}
	}
	return false;
}

bool IsSpeedDeBuff(uint16 spell_id)
{
	if (!IsBeneficialSpell(spell_id))
	{
		for (int i = 0; i < EFFECT_COUNT; i++)
		{
			if (spells[spell_id].effectid[i] == SE_MovementSpeed && spells[spell_id].base[i] < 0)
				return true;
		}
	}
	return false;
}

bool IsRainSpell(uint16 spell_id)
{
	if (IsDetrimentalSpell(spell_id) && spells[spell_id].targettype == ST_AETarget &&
		spells[spell_id].AEDuration > 2000 && spells[spell_id].AEDuration < 360000)		// Sentinel and Sanctuary have durations of 360,000
	{
		return true;
	}

	return false;
}

bool IsDisc(uint16 spell_id)
{
	if(spells[spell_id].IsDisciplineBuff)
		return true;
	else
		return false;
}

bool IsShrinkSpell(uint16 spell_id)
{
	int j;

	if (!IsValidSpell(spell_id))
		return false;

	for (j = 0; j < EFFECT_COUNT; j++)
	{
		if (spells[spell_id].effectid[j] == SE_ModelSize && spells[spell_id].base[j] < 100)
		{
			return true;
		}
	}

	return false;
}

bool IsLuclinPortSpell (uint16 spell_id)
{
	if(spell_id == 2062 || spell_id == 2706 || spell_id == 2707 || spell_id == 2708 || spell_id == 2709 || spell_id == 2935)
		return true;

	return false;
}

bool IsInvisSpell(uint16 spell_id)
{
	return IsEffectInSpell(spell_id, SE_Invisibility) || IsEffectInSpell(spell_id, SE_InvisVsUndead) || IsEffectInSpell(spell_id, SE_InvisVsAnimals);
}

bool IsFixedDurationInvisSpell(uint16 spell_id)
{
	if 
	(
		spell_id == 2765 /* Camouflage (innate from AA) */ ||
		spell_id == 1435 /* Improved Superior Camouflage */ ||
		spell_id == 1406 /* Improved Invisibility */ ||
		spell_id == 1411 /* Improved Invis to Undead */ ||
		spell_id == 1625 /* Skin of the Shadow */ ||
		spell_id == 2537 /* Veil of Elements */
	)
		return true;

	return false;
}

bool IsInstrumentModdableSpellEffect(uint16 spell_id, int effect_index)
{
	int SPA = spells[spell_id].effectid[effect_index];

	switch (SPA)
	{
		case SE_CurrentHP:
		case SE_ArmorClass:
		case SE_ATK: // Jonthan's Provocation, McVaxius` Rousing Rondo, Jonthan's Inspiration, Warsong of Zek
		case SE_MovementSpeed:	// maybe only positive values should be modded? Selo`s Consonant Chain uses this for snare
		case SE_STR:
		case SE_DEX:
		case SE_AGI:
		case SE_STA:
		case SE_INT:
		case SE_WIS:
		case SE_CHA:
		case SE_Stamina:
		case SE_ResistFire:
		case SE_ResistCold:
		case SE_ResistPoison:
		case SE_ResistDisease:
		case SE_ResistMagic:
		case SE_Rune: // Shield of Songs, Nillipus` March of the Wee
		case SE_DamageShield: // Psalm of Warmth, Psalm of Vitality, Psalm of Cooling, Psalm of Purity, McVaxius` Rousing Rondo, Warsong of Zek, Psalm of Veeshan
		case SE_AbsorbMagicAtt: // Psalm of Mystic Shielding, Niv`s Melody of Preservation, Shield of Songs, Niv`s Harmonic
		case SE_ResistAll: // Psalm of Veeshan
			return true;

		case SE_CurrentMana:
		{
			// Only these mana songs are moddable: Cassindra`s Chorus of Clarity, Denon`s Dissension, Cassindra`s Chant of Clarity, Ervaj's Lost Composition
			// but we override the mod for the mana regen songs in Mob::GetInstrumentMod()
			if (SPA == SE_CurrentMana && spells[spell_id].buffdurationformula == 0 && spells[spell_id].targettype != ST_Tap && spells[spell_id].targettype != ST_TargetAETap)
				return true;
			return false;
		}
	}

	return false;
}

bool IsSplurtFormulaSpell(uint16 spell_id)
{
	for (int i = 0; i < EFFECT_COUNT; i++)
	{
		int formula = spells[spell_id].formula[i];
		if (formula == 107 || formula == 108 || formula == 120 || formula == 122) return true;
	}

	return false;
}

bool IsMGBCompatibleSpell(uint16 spell_id)
{
	auto s = spells[spell_id];

	if (s.can_mgb && s.goodEffect && s.buffdurationformula && (s.targettype == ST_Group || s.targettype == ST_GroupTeleport))
		return true;

	return false;
}

bool IsDispellableSpell(uint16 spell_id)
{
	// not a buff
	if (spells[spell_id].buffdurationformula == 0)
		return false;

	// this is a custom database field and didn't exist in TAKP era
	if (spells[spell_id].dispel_flag)
		return false;

	for (int i = 0; i < EFFECT_COUNT; i++)
	{
		int spa = spells[spell_id].effectid[i];

		// spells with counters must be cured with the same counter effect and can't be dispelled with cancel magic effects
		if (spa == SE_PoisonCounter || spa == SE_DiseaseCounter || spa == SE_CurseCounter)
			return false;

		if (spa == SE_EyeOfZomm || spa == SE_SummonHorse || spa == SE_CompleteHeal)
			return false;
	}

	return true;
}
