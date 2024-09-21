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
	General outline of spell casting process

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
#include "../common/fastmath.h"

#include "../common/bodytypes.h"
#include "../common/classes.h"
#include "../common/content/world_content_service.h"
#include "../common/data_verification.h"
#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/item_instance.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/strings.h"

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"

#include <assert.h>
#include <math.h>

#ifndef WIN32
	#include <stdlib.h>
	#include "../common/unix.h"
#endif

#ifdef _GOTFRAGS
	#include "../common/packet_dump_file.h"
#endif



extern Zone* zone;
extern volatile bool is_zone_loaded;
extern WorldServer worldserver;
extern FastMath g_Math;

using EQ::spells::CastingSlot;

// this is run constantly for every mob
void Mob::SpellProcess()
{
	// check the rapid recast prevention timer
	if(spellrecovery_timer.Check())
	{
		spellrecovery_timer.Disable();
		Log(Logs::Detail, Logs::Spells, "Spell recovery timer disabled.");
		return;
	}

	// a timed spell is finished casting
	if (casting_spell_id != 0 && spellend_timer.Check())
	{
		CastedSpellFinished(casting_spell_id, casting_spell_targetid, casting_spell_slot,
			casting_spell_mana, casting_spell_inventory_slot, casting_spell_resist_adjust);
	}

	if (temporary_pets_effect && temporary_pets_effect->next_spawn_timer.Check())
	{
		if (temporary_pets_effect->pet_count_remaining > 0)
		{
			// spawn the next pet
			CreateTemporaryPet(&temporary_pets_effect->npc_type, temporary_pets_effect->pet_duration_seconds, temporary_pets_effect->pet_target_id, temporary_pets_effect->followme, temporary_pets_effect->sticktarg, GetPosition());
			temporary_pets_effect->pet_count_remaining--;
		}
		if (temporary_pets_effect->pet_count_remaining <= 0)
		{
			// done with swarm pets
			safe_delete(temporary_pets_effect);
		}
		else
		{
			temporary_pets_effect->next_spawn_timer.Start(500);
		}
	}

	if (IsClient())
	{
		Client *c = CastToClient();
		if (c->m_epp.aa_effects)
		{
			for (int i = 1; i < 32; i++) // aaEffectType uses bits in a uint32 to track active abilities
			{
				if (c->m_epp.aa_effects & (1 << (i - 1)))
				{
					// the ability can remain enabled even after the timer is expired and it would
					// just get disabled the next time it's checked but this will expire
					// the effect on time and generate worn off messages
					c->CheckAAEffect(static_cast<aaEffectType>(i)); 
				}
			}
		}
	}

	// instill doubt
	if (instillDoubtStageTimer.Check())
	{
		instillDoubtStageTimer.Disable();
		InstillDoubt(nullptr, 1); // the target ID is saved in instillDoubtTargetID field
	}
}

void NPC::SpellProcess()
{
	Mob::SpellProcess();
	if (swarm_timer.Check()) {
		DepopSwarmPets();
	}
}

namespace {
	bool IsWhitelistedBeneficialSpellForSelfFound(uint16 spell_id, Client *caster, Client *target)
	{
		if (caster == nullptr || !caster->IsSelfFound())
			return false;

		if (target == nullptr || !target->IsSelfFound() || target->IsSoloOnly())
			return false;

		if (IsEffectInSpell(spell_id, SE_Teleport))
			return true;

		if (IsEffectInSpell(spell_id, SE_BindAffinity))
			return true;

		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////
// functions related to begin/finish casting, fizzling etc
//
// only CastSpell and DoCastSpell should be setting casting_spell_id.
// basically casting_spell_id is only set when casting a triggered spell from
// the spell bar gems, an ability, or an item. note that it's actually set
// even if it's a 0 cast time, but then the spell is finished right after and
// it's unset. this is ok, since the 0 cast time spell is still a triggered
// one.
// the rule is you can cast one triggered (usually timed) spell at a time
// but things like SpellFinished() can run concurrent with a triggered cast
// to allow procs to work
bool Mob::CastSpell(uint16 spell_id, uint16 target_id, CastingSlot slot,
	int32 cast_time, int32 mana_cost, uint32* oSpellWillFinish, uint32 item_slot,
	uint32 timer, uint32 timer_duration, uint32 type, int16 *resist_adjust)
{
	Log(Logs::Detail, Logs::Spells, "CastSpell called for spell %s (%d) on entity %d, slot %d, time %d, mana %d adj %d, from item slot %d",
		spells[spell_id].name, spell_id, target_id, static_cast<int>(slot), cast_time, mana_cost, resist_adjust? *resist_adjust : spells[spell_id].ResistDiff, (item_slot==0xFFFFFFFF)?999:item_slot);

	if(casting_spell_id == spell_id)
		ZeroCastingVars();

	if
	(
		!IsValidSpell(spell_id) ||
		casting_spell_id ||
		spellend_timer.Enabled() ||
		IsStunned() ||
		IsFeared() ||
		IsMezzed() ||
		(IsSilenced()) ||
		(IsAmnesiad())
	)
	{
		Log(Logs::Detail, Logs::Spells, "Spell casting canceled: not able to cast now. Valid? %d, casting %d, spellend? %d, stunned? %d, feared? %d, mezed? %d, silenced? %d, amnesiad? %d",
			IsValidSpell(spell_id), casting_spell_id, spellend_timer.Enabled(), IsStunned(), IsFeared(), IsMezzed(), IsSilenced(), IsAmnesiad() );
		if(IsSilenced())
			Message_StringID(Chat::SpellFailure, SILENCED_STRING);
		if (IsAmnesiad())
		{
			// String is not in our eqstr_en.txt file:
			//You *CANNOT* use this melee ability, you are suffering from amnesia!
		}
		if (IsClient())
		{
			CastToClient()->SendSpellBarEnable(spell_id);
			if (casting_aa != 0)
			{
				aaID activeaa = static_cast<aaID>(casting_aa);
				CastToClient()->ResetAATimer(activeaa, ABILITY_FAILED);
			}
		}
		if(casting_spell_id && IsNPC())
			CastToNPC()->AI_Event_SpellCastFinished(false, static_cast<uint16>(casting_spell_slot));
		return(false);
	}

	Mob *spell_target = entity_list.GetMob(target_id);
	//cannot cast under divine aura
	if (spell_id != SPELL_AA_BOASTFUL_BELLOW) // special case - this spell could be cast while invulnerable on AK
	{
		if (DivineAura()) {
			Log(Logs::Detail, Logs::Spells, "Spell casting canceled: cannot cast while Divine Aura is in effect.");
			InterruptSpell(SPELL_FIZZLE, Chat::SpellFailure, spell_id, true, false);
			return(false);
		}
	}

	if (spellbonuses.NegateIfCombat)
		BuffFadeByEffect(SE_NegateIfCombat);

	// check to see if target is a caster mob before performing a mana tap or one of the recourse based mana-tap-over-time spells
	if (IsClient() && spell_target &&
		(spells[spell_id].manatapspell || /* Theft of Thought (player castable), Mind Tap, Essence Drain, Black Claw */
		(spells[spell_id].contains_se_currentmana && spells[spell_id].hasrecourse)) /* Wandering Mind, Mind Wrack, Scryer's Trespass */)
	{
		if (spell_target->GetCasterClass() == 'N') {
			InterruptSpell(TARGET_NO_MANA, Chat::SpellFailure, spell_id, true, false);
			return false;
		}
	}

	//prevent immune from aggro and spells npcs from being casted on.
	if (IsClient() && spell_target && spell_target->IsNPC())
	{
		NPC* spell_target_npc = spell_target->CastToNPC();
		if (spell_target_npc)
		{
			if (spell_target_npc->GetSpecialAbility(IMMUNE_MAGIC) && spell_target_npc->GetSpecialAbility(IMMUNE_AGGRO))
			{
				InterruptSpell(CANNOT_AFFECT_NPC, Chat::SpellFailure, spell_id, true, false);
				return false;
			}
		}
	}

	// check line of sight to target if it's a detrimental spell
	if (IsClient() && spell_target && target_id != this->GetID()
		&& spells[spell_id].targettype != ST_TargetOptional && spells[spell_id].targettype != ST_Self && spells[spell_id].targettype != ST_AECaster)
	{
		if (!zone->SkipLoS() && IsDetrimentalSpell(spell_id) && !spells[spell_id].npc_no_los && !CheckLosFN(spell_target, true)
			&& (!IsHarmonySpell(spell_id) || spells[spell_id].targettype == ST_AETarget)	// harmony and wake of tranq require LoS
			&& !IsBindSightSpell(spell_id) && !IsAllianceSpellLine(spell_id))
		{
			Log(Logs::Detail, Logs::Spells, "Spell %d: cannot see target %s", spell_id, spell_target->GetName());
			InterruptSpell(CANT_SEE_TARGET, Chat::SpellFailure, spell_id, true, false);
			return (false);
		}
	}

	if (!DoPreCastingChecks(spell_id, slot, target_id))
	{
		// the specific message and interrupt is handled inside DoPreCastingChecks
		return false;
	}

	// check for fizzle
	// note that CheckFizzle itself doesn't let NPCs fizzle,
	// but this code allows for it.
	if(slot < CastingSlot::MaxGems && !CheckFizzle(spell_id))
	{
		int fizzle_msg = spells[spell_id].bardsong ? MISS_NOTE : SPELL_FIZZLE;
		InterruptSpell(fizzle_msg, Chat::SpellFailure, spell_id, true);

		uint32 use_mana = spells[spell_id].mana * 4 / 10;
		// the amount fizzled in one cast is capped to 1/8th of the caster's mana pool size
		uint32 use_mana_cap = GetMaxMana() / 8;
		use_mana = use_mana > use_mana_cap ? use_mana_cap : use_mana;
		Log(Logs::Detail, Logs::Spells, "Spell casting canceled: fizzled. %d mana has been consumed", use_mana);

		// fizzle 40% of the mana away
		SetMana(GetMana() - use_mana);
		return(false);
	}

	if (HasActiveSong() && spells[spell_id].bardsong) {
		Log(Logs::Detail, Logs::Spells, "Casting a new song while singing a song. Killing old song %d.", bardsong);
		//Note: this does NOT tell the client
		_StopSong();
	}

	//Added to prevent MQ2 exploitation of equipping normally-unequippable/clickable items with effects and clicking them for benefits.
	if(item_slot && IsClient() && ((slot == CastingSlot::Item)))
	{
		EQ::ItemInstance *itm = CastToClient()->GetInv().GetItem(item_slot);
		int bitmask = 1;
		bitmask = bitmask << (CastToClient()->GetClass() - 1);
		if( itm && itm->GetItem()->Classes != 65535 ) {
			if ((itm->GetItem()->Click.Type == EQ::item::ItemEffectEquipClick) && !(itm->GetItem()->Classes & bitmask)) {
					// They are casting a spell from an item that requires equipping but shouldn't let them equip it
					Log(Logs::General, Logs::Error, "HACKER: %s (account: %s) attempted to click an equip-only effect on item %s (id: %d) which they shouldn't be able to equip!",
						CastToClient()->GetCleanName(), CastToClient()->AccountName(), itm->GetItem()->Name, itm->GetItem()->ID);
					database.SetHackerFlag(CastToClient()->AccountName(), CastToClient()->GetCleanName(), "Clicking equip-only item with an invalid class");
				return(false);
			}
			if ((itm->GetItem()->Click.Type == EQ::item::ItemEffectClick2) && !(itm->GetItem()->Classes & bitmask)) {
					// They are casting a spell from an item that they don't meet the race/class requirements to cast
					Log(Logs::General, Logs::Error, "HACKER: %s (account: %s) attempted to click a race/class restricted effect on item %s (id: %d) which they shouldn't be able to click!",
						CastToClient()->GetCleanName(), CastToClient()->AccountName(), itm->GetItem()->Name, itm->GetItem()->ID);
					database.SetHackerFlag(CastToClient()->AccountName(), CastToClient()->GetCleanName(), "Clicking race/class restricted item with an invalid class");
				return(false);
			}
		}
		if( itm && (itm->GetItem()->Click.Type == EQ::item::ItemEffectEquipClick) && !(item_slot <= EQ::invslot::slotAmmo) ){
				// They are attempting to cast a must equip clicky without having it equipped
				Log(Logs::General, Logs::Error, "HACKER: %s (account: %s) attempted to click an equip-only effect on item %s (id: %d) without equiping it!", CastToClient()->GetCleanName(), CastToClient()->AccountName(), itm->GetItem()->Name, itm->GetItem()->ID);
				database.SetHackerFlag(CastToClient()->AccountName(), CastToClient()->GetCleanName(), "Clicking equip-only item without equiping it");
			return(false);
		}
	}

	std::string export_string = fmt::format("{}", spell_id);
	if(IsClient()) {
		if (parse->EventPlayer(EVENT_CAST_BEGIN, CastToClient(), export_string, 0) != 0) {
			return false;
		}
	} else if(IsNPC()) {
		parse->EventNPC(EVENT_CAST_BEGIN, CastToNPC(), nullptr, export_string, 0);
	}

	//To prevent NPC ghosting when spells are cast from scripts
	if (IsNPC() && IsMoving() && cast_time > 0) {
		StopNavigation();
	}

	if(resist_adjust)
	{
		return(DoCastSpell(spell_id, target_id, slot, cast_time, mana_cost, oSpellWillFinish, item_slot, timer, timer_duration, type, *resist_adjust));
	}
	else
	{
		return(DoCastSpell(spell_id, target_id, slot, cast_time, mana_cost, oSpellWillFinish, item_slot, timer, timer_duration, type, spells[spell_id].ResistDiff));
	}
}

//
// the order of things here is intentional and important. make sure you
// understand the whole spell casting process and the flags that are passed
// around if you're gonna modify this
//
// this is the 2nd phase of CastSpell, broken up like this to make it easier
// to repeat a spell for bard songs
//
bool Mob::DoCastSpell(uint16 spell_id, uint16 target_id, CastingSlot slot,
					int32 cast_time, int32 mana_cost, uint32* oSpellWillFinish,
					uint32 item_slot, uint32 timer, uint32 timer_duration, uint32 type,
					int16 resist_adjust)
{
	Mob* pMob = nullptr;
	int32 orgcasttime;
	EQApplicationPacket *outapp = nullptr;

	if(!IsValidSpell(spell_id)) {
		InterruptSpell();
		return(false);
	}

	const SPDat_Spell_Struct &spell = spells[spell_id];

	Log(Logs::Detail, Logs::Spells, "DoCastSpell called for spell %s (%d) on entity %d, slot %d, time %d, mana %d, adj %d, from item %d",
		spell.name, spell_id, target_id, static_cast<int>(slot), cast_time, mana_cost, resist_adjust, item_slot==0xFFFFFFFF?999:item_slot);

	casting_spell_id = spell_id;
	casting_spell_slot = slot;
	casting_spell_inventory_slot = item_slot;
	if(casting_spell_timer != 0xFFFFFFFF)
	{
		casting_spell_timer = timer;
		casting_spell_timer_duration = timer_duration;
	}
	casting_spell_type = type;

	SaveSpellLoc();
	Log(Logs::Detail, Logs::Spells, "Casting %d Started at (%.3f,%.3f,%.3f)", spell_id, m_SpellLocation.x, m_SpellLocation.y, m_SpellLocation.z);

	// if this spell doesn't require a target, or if it's an optional target
	// and a target wasn't provided, then it's us; unless TGB is on and this
	// is a TGB compatible spell.
	if((IsGroupSpell(spell_id) && target_id == 0) ||
		(spell.targettype == ST_Self && !(IsEffectInSpell(spell_id, SE_Illusion) && IsClient() && CastToClient()->CheckAAEffect(aaEffectProjectIllusion))) ||
		spell.targettype == ST_UndeadAE ||
		spell.targettype == ST_SummonedAE ||
		spell.targettype == ST_AEClientV1 ||
		spell.targettype == ST_AEBard ||
		spell.targettype == ST_AECaster)
	{
		Log(Logs::Detail, Logs::Spells, "Spell %d auto-targeted the caster. Group? %d, target type %d", spell_id, IsGroupSpell(spell_id), spell.targettype);
		target_id = GetID();
	}

	if(target_id == 0 && spell.targettype == ST_Target && IsEffectInSpell(spell_id, SE_SummonItem)) {
		Log(Logs::Detail, Logs::Spells, "SE_SummonItem spell %d auto-targeted the caster.", spell_id);
		target_id = GetID();
	}

	if(cast_time <= -1) {
		// save the non-reduced cast time to use in the packet
		cast_time = orgcasttime = spell.cast_time;
		// if there's a cast time, check if they have a modifier for it
		if(cast_time) {
			cast_time = GetActSpellCasttime(spell_id, cast_time);
		}

		// Innate hybrid detrimental spell haste - 3% cast time reduction per level over 50
		// This was added to the game in the February 21, 2001 patch.  Logs confirm it does apply to NPCs as well.
		if (
			GetLevel() > 50 &&
			(GetClass() == BEASTLORD || GetClass() == PALADIN || GetClass() == RANGER || GetClass() == SHADOWKNIGHT) &&
			orgcasttime > 2999 &&
			spells[spell_id].goodEffect == 0
		)
		{
			float percent_mod = 100.0 - (GetLevel() - 50) * 3.0;
			cast_time = (int32)((float)cast_time * percent_mod / 100.0);
			cast_time = cast_time < orgcasttime / 2 ? orgcasttime / 2 : cast_time;
		}
	}
	else
		orgcasttime = cast_time;

	if (IsNPC() && cast_time == 0)		// NPC instant cast spells show a "begins to cast" message
		orgcasttime = cast_time = 1;

	// we checked for spells not requiring targets above
	if(target_id == 0) {
		Log(Logs::Detail, Logs::Spells, "Spell Error: no target. spell=%d\n", GetName(), spell_id);
		if(IsClient()) {
			//clients produce messages... npcs should not for this case
			InterruptSpell(SPELL_NEED_TAR,Chat::Red, SPELL_UNKNOWN);
		} else {
			InterruptSpell(0, 0, 0);	//the 0 args should cause no messages
		}
		return(false);
	}

	// ok now we know the target
	casting_spell_targetid = target_id;
	Mob *spell_target = entity_list.GetMob(target_id);

	if (mana_cost == -1) {
		mana_cost = spell.mana;
		mana_cost = GetActSpellCost(spell_id, mana_cost);
	}

	if(IsClient() && CastToClient()->CheckAAEffect(aaEffectMassGroupBuff) && IsMGBCompatibleSpell(spell_id))
		mana_cost *= 2;

	// mana is checked for clients on the frontend. we need to recheck it for NPCs though
	// fix: items dont need mana :-/

	// we calculated this above, now enforce it
	if(mana_cost > 0 && slot != CastingSlot::Item)
	{
		int my_curmana = GetMana();
		int my_maxmana = GetMaxMana();
		if(my_curmana < spell.mana)	// not enough mana - note that this checks the base mana cost of the spell not the reduced or magnified MGB cost
		{
			//this is a special case for NPCs with no mana...
			if(IsNPC() && my_curmana == my_maxmana)
			{
				mana_cost = 0;
			} else {
				Log(Logs::Detail, Logs::Spells, "Spell Error not enough mana spell=%d mymana=%d cost=%d\n", GetName(), spell_id, my_curmana, mana_cost);
				if(IsClient()) {
					//clients produce messages... npcs should not for this case
					InterruptSpell(INSUFFICIENT_MANA,Chat::SpellFailure, SPELL_UNKNOWN);
				} else {
					InterruptSpell(0, 0, 0);	//the 0 args should cause no messages
				}
				return(false);
			}
		}
	}

	// we know our mana cost now
	casting_spell_mana = mana_cost;

	casting_spell_resist_adjust = resist_adjust;

	DoCastingRangeCheck(spell_id, slot, spell_target);

	Log(Logs::Detail, Logs::Spells, "Spell %d: Casting time %d (orig %d); mana cost %d; resist adjust %d",
			spell_id, cast_time, orgcasttime, mana_cost, resist_adjust);

	if (cast_time <= 0)
	{
		CastedSpellFinished(spell_id, target_id, slot, mana_cost, item_slot, resist_adjust);
		if (IsBardSong(spell_id) && GetClass() == BARD && slot == CastingSlot::Item)
			_StopSong(); // don't repeat Breath of Harmony and Lute of the Flowing Waters instant cast clickies
		return(true);
	}
	spellend_timer.Start(cast_time); // ok we know it has a cast time so we can start the timer now
	
	if (IsAIControlled())
	{
		SetRunAnimSpeed(0);
		pMob = entity_list.GetMob(target_id);
		if (pMob && this != pMob)
			FaceTarget(pMob);
	}

	// if we got here we didn't fizzle, and are starting our cast
	if (oSpellWillFinish)
		*oSpellWillFinish = Timer::GetCurrentTime() + cast_time + 100;

	uint16 cast_spell = spell_id;
	if(IsLuclinPortSpell(spell_id))
		cast_spell = 2935;

	// now tell the people in the area
	outapp = new EQApplicationPacket(OP_BeginCast,sizeof(BeginCast_Struct));
	BeginCast_Struct* begincast = (BeginCast_Struct*)outapp->pBuffer;
	begincast->caster_id = GetID();
	begincast->spell_id = cast_spell;
	begincast->cast_time = orgcasttime; // client calculates reduced time by itself
	outapp->priority = 3;
	entity_list.QueueCloseClients(this, outapp, false, RuleI(Range, BeginCast), 0, true); //IsClient() ? FILTER_PCSPELLS : FILTER_NPCSPELLS);
	safe_delete(outapp);
	outapp = nullptr;

	if (IsClient() && slot == CastingSlot::Item && item_slot != 0xFFFFFFFF) 
	{
		auto item = CastToClient()->GetInv().GetItem(item_slot);
		if (item && item->GetItem())
			 Message_StringID(Chat::Spells, BEGINS_TO_GLOW, item->GetItem()->Name);
	}

	return(true);
}

bool Mob::DoPreCastingChecks(uint16 spell_id, CastingSlot slot, uint16 spell_targetid)
{
	if (!IsClient())
	{
		return true;
	}

	Client *caster = CastToClient();
	Mob *spell_target = entity_list.GetMob(spell_targetid);

	if (!(IsClient() && CastToClient()->GetGM()))
	{
		if (RuleB(Spells, BuffLevelRestrictions))
		{
			// casting_spell_targetid is guaranteed to be what we went, check for ST_Self for now should work though
			if (spell_target && spells[spell_id].targettype != ST_Self && !spell_target->CheckSpellLevelRestriction(spell_id, this, slot))
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d failed: recipient did not meet the level restrictions", spell_id);
				InterruptSpell(SPELL_TOO_POWERFUL, Chat::SpellFailure, spell_id, false, false);
				return false;
			}
		}

		if (spells[spell_id].TimeOfDay == 1 && zone->zone_time.IsNightTime())
		{
			InterruptSpell(CAST_DAYTIME, Chat::SpellFailure, spell_id, false, false);
			return false;
		}

		if (spells[spell_id].TimeOfDay == 2 && !zone->zone_time.IsNightTime())
		{
			InterruptSpell(CAST_NIGHTTIME, Chat::SpellFailure, spell_id, false, false);
			return false;
		}

		if (spells[spell_id].zonetype == 1 && !zone->CanCastOutdoor()) {
			InterruptSpell(CAST_OUTDOORS, Chat::SpellFailure, spell_id, false, false);
			return false;
		}

		if (spells[spell_id].zonetype == 2 && !zone->CanCastDungeon()) {
			InterruptSpell(CAST_DUNGEONS, Chat::SpellFailure, spell_id, false, false);
			return false;
		}

		if (IsEffectInSpell(spell_id, SE_Levitate) && !zone->CanLevitate()) {
			Message(Chat::SpellFailure, "You can't levitate in this zone.");
			InterruptSpell(INTERRUPT_SPELL, Chat::Spells, spell_id, false, false);
			return false;
		}

		// horse is blocked in sseru with a specific message
		if (IsEffectInSpell(spell_id, SE_SummonHorse) && GetZoneID() == Zones::SSERU)
		{
			InterruptSpell(CANNOT_SUMMON_MOUNT_HERE, Chat::SpellFailure, spell_id, false, false);
			return false;
		}

		// spell blocking can be defined in the database
		if (zone->IsSpellBlocked(spell_id, glm::vec3(GetPosition()))) {
			const char* msg = zone->GetSpellBlockedMessage(spell_id, glm::vec3(GetPosition()));
			if (msg)
			{
				Message(Chat::SpellFailure, msg);
				InterruptSpell(0, Chat::Spells, spell_id, false, false);
			}
			else
			{
				InterruptSpell(SPELL_DOES_NOT_WORK_HERE, Chat::SpellFailure, spell_id, false, false);
			}
			return false;
		}

		// Interrupt summon item spells cast onto non-pet npcs
		if(spells[spell_id].targettype == ST_Target && IsEffectInSpell(spell_id, SE_SummonItem) &&
				spell_target && (!spell_target->IsPet() && !spell_target->IsClient()))
		{
			InterruptSpell(CANNOT_AFFECT_NPC, Chat::SpellFailure, spell_id, false, false);
			return false;
		}

		// Interrupt spell casts that are targetting self found or solo if they're not allowed
		// Already know caster is a client from the first check in this function
		if(spell_target && spell_target->IsClient())					
		{
			// Only fail if it's beneficial - don't want to fail on detrimental for pvp purposes
			if(IsBeneficialSpell(spell_id))
			{
				const char* SOLO_PLAYER_ERROR = "Unable to cast spells on solo players.";
				const char* SELF_FOUND_ERROR = "Unable to cast spells on self found players.";
				const char* LEVEL_ERROR = "This self found player is not close enough to your level.";

				bool is_failed_cast = false;
				const char* fail_message = nullptr;

				if (spell_target->CastToClient()->IsSoloOnly() && spell_target != this)
				{
					// if the target is solo, don't allow anyone to buff it
					// if the caster is solo, it's fine if they try to buff someone
					is_failed_cast = true;
					fail_message = SOLO_PLAYER_ERROR;
				} 
				if (spell_target->CastToClient()->IsSelfFound() && spell_target != this)
				{
					bool can_get_experience = spell_target->CastToClient()->IsInLevelRange(caster->GetLevel2()) && caster->IsInLevelRange(spell_target->CastToClient()->GetLevel2());
					bool compatible = caster->IsSelfFound() == spell_target->CastToClient()->IsSelfFound();
					if (!compatible)
					{
						// if the spell target is self found, but the caster is not, don't allow the caster to buff
						is_failed_cast = true;
						fail_message = SELF_FOUND_ERROR;
					}
					else if(compatible && !can_get_experience && !IsWhitelistedBeneficialSpellForSelfFound(spell_id, caster, spell_target->CastToClient()))
					{
						// if the spell_target can not get EXP while grouped with the caster, don't allow the caster to buff
						is_failed_cast = true;
						fail_message = LEVEL_ERROR;
					}
				}
				if (is_failed_cast)
				{
					Message(Chat::SpellFailure, fail_message);
					InterruptSpell(CANNOT_AFFECT_PC, Chat::SpellFailure, spell_id, false, false);
					return false;
				}
			}
		}

		
	}

	// if already buffed with a horse buff, cancel the casting of a new horse buff with a fizzle style packet so that the bridle acts as a gem refreshing clicky
	if (IsEffectInSpell(spell_id, SE_SummonHorse) && FindType(SE_SummonHorse))
	{
		// the 4th argument being true here makes this reset the spell gems as if the spell fizzled
		InterruptSpell(ALREADY_ON_A_MOUNT, Chat::SpellFailure, spell_id, true, false);
		return false;
	}

	return true;
}

uint16 Mob::GetSpecializeSkillValue(uint16 spell_id) const {
	switch(spells[spell_id].skill) {
	case EQ::skills::SkillAbjuration:
		return(GetSkill(EQ::skills::SkillSpecializeAbjure));
	case EQ::skills::SkillAlteration:
		return(GetSkill(EQ::skills::SkillSpecializeAlteration));
	case EQ::skills::SkillConjuration:
		return(GetSkill(EQ::skills::SkillSpecializeConjuration));
	case EQ::skills::SkillDivination:
		return(GetSkill(EQ::skills::SkillSpecializeDivination));
	case EQ::skills::SkillEvocation:
		return(GetSkill(EQ::skills::SkillSpecializeEvocation));
	default:
		//wtf...
		break;
	}
	return(0);
}

void Client::CheckSpecializeIncrease(uint16 spell_id) {
	// These are not active because CheckIncreaseSkill() already does so.
	// It's such a rare occurance that adding them here is wasted..(ref only)
	/*
	if (IsDead() || IsUnconscious())
		return;
	if (IsAIControlled())
		return;
	*/

	switch(spells[spell_id].skill) {
	case EQ::skills::SkillAbjuration:
		if(GetRawSkill(EQ::skills::SkillSpecializeAbjure) > 0)
			CheckIncreaseSkill(EQ::skills::SkillSpecializeAbjure, nullptr, zone->skill_difficulty[EQ::skills::SkillSpecializeAbjure].difficulty);
		break;
	case EQ::skills::SkillAlteration:
		if(GetRawSkill(EQ::skills::SkillSpecializeAlteration) > 0)
			CheckIncreaseSkill(EQ::skills::SkillSpecializeAlteration, nullptr, zone->skill_difficulty[EQ::skills::SkillSpecializeAlteration].difficulty);
		break;
	case EQ::skills::SkillConjuration:
		if(GetRawSkill(EQ::skills::SkillSpecializeConjuration) > 0)
			CheckIncreaseSkill(EQ::skills::SkillSpecializeConjuration, nullptr, zone->skill_difficulty[EQ::skills::SkillSpecializeConjuration].difficulty);
		break;
	case EQ::skills::SkillDivination:
		if(GetRawSkill(EQ::skills::SkillSpecializeDivination) > 0)
			CheckIncreaseSkill(EQ::skills::SkillSpecializeDivination, nullptr, zone->skill_difficulty[EQ::skills::SkillSpecializeDivination].difficulty);
		break;
	case EQ::skills::SkillEvocation:
		if(GetRawSkill(EQ::skills::SkillSpecializeEvocation) > 0)
			CheckIncreaseSkill(EQ::skills::SkillSpecializeEvocation, nullptr, zone->skill_difficulty[EQ::skills::SkillSpecializeEvocation].difficulty);
		break;
	default:
		//wtf...
		break;
	}
}

void Client::CheckSongSkillIncrease(uint16 spell_id){
	// These are not active because CheckIncreaseSkill() already does so.
	// It's such a rare occurance that adding them here is wasted..(ref only)
	/*
	if (IsDead() || IsUnconscious())
		return;
	if (IsAIControlled())
		return;
	*/

	uint8 success = SKILLUP_FAILURE;
	switch(spells[spell_id].skill)
	{
	case EQ::skills::SkillSinging:
		CheckIncreaseSkill(EQ::skills::SkillSinging, nullptr, zone->skill_difficulty[EQ::skills::SkillSinging].difficulty);
		break;
	case EQ::skills::SkillPercussionInstruments:
		if(this->itembonuses.percussionMod > 0) {
			if(GetRawSkill(EQ::skills::SkillPercussionInstruments) > 0)	// no skill increases if not trained in the instrument
				CheckIncreaseSkill(EQ::skills::SkillPercussionInstruments, nullptr, zone->skill_difficulty[EQ::skills::SkillPercussionInstruments].difficulty);
			else
				Message_StringID(Chat::Red,NO_INSTRUMENT_SKILL);	// tell the client that they need instrument training
		}
		else
			CheckIncreaseSkill(EQ::skills::SkillSinging, nullptr, zone->skill_difficulty[EQ::skills::SkillSinging].difficulty, success);
		break;
	case EQ::skills::SkillStringedInstruments:
		if(this->itembonuses.stringedMod > 0) {
			if(GetRawSkill(EQ::skills::SkillStringedInstruments) > 0)
				CheckIncreaseSkill(EQ::skills::SkillStringedInstruments, nullptr, zone->skill_difficulty[EQ::skills::SkillStringedInstruments].difficulty);
			else
				Message_StringID(Chat::Red,NO_INSTRUMENT_SKILL);
		}
		else
			CheckIncreaseSkill(EQ::skills::SkillSinging, nullptr, zone->skill_difficulty[EQ::skills::SkillSinging].difficulty, success);
		break;
	case EQ::skills::SkillWindInstruments:
		if(this->itembonuses.windMod > 0) {
			if(GetRawSkill(EQ::skills::SkillWindInstruments) > 0)
				CheckIncreaseSkill(EQ::skills::SkillWindInstruments, nullptr, zone->skill_difficulty[EQ::skills::SkillWindInstruments].difficulty);
			else
				Message_StringID(Chat::Red,NO_INSTRUMENT_SKILL);
		}
		else
			CheckIncreaseSkill(EQ::skills::SkillSinging, nullptr, zone->skill_difficulty[EQ::skills::SkillSinging].difficulty, success);
		break;
	case EQ::skills::SkillBrassInstruments:
		if(this->itembonuses.brassMod > 0) {
			if(GetRawSkill(EQ::skills::SkillBrassInstruments) > 0)
				CheckIncreaseSkill(EQ::skills::SkillBrassInstruments, nullptr, zone->skill_difficulty[EQ::skills::SkillBrassInstruments].difficulty);
			else
				Message_StringID(Chat::Red,NO_INSTRUMENT_SKILL);
		}
		else
			CheckIncreaseSkill(EQ::skills::SkillSinging, nullptr, zone->skill_difficulty[EQ::skills::SkillSinging].difficulty, success);
		break;
	default:
		break;
	}
}

/*
returns true if spell is successful, false if it fizzled.
only works for clients, npcs shouldn't be fizzling..
new algorithm thats closer to live eq (i hope)
TODO: Add aa skills, item mods, reduced the chance to fizzle
*/
bool Mob::CheckFizzle(uint16 spell_id)
{
	return(true);
}

bool Client::CheckFizzle(uint16 spell_id)
{
	// GMs don't fizzle
	if (GetGM()) return(true);

	// spellLevel is the level at which this caster class can cast this spell
	// the same spell has different levels for different classes, for example Divine Aura is 1 for cleric and 51 for paladin
	int spellLevel = spells[spell_id].classes[GetClass() - 1];

	//Live AA - Spell Casting Expertise, Mastery of the Past
	int no_fizzle_level = std::max(std::max(aabonuses.MasteryofPast, itembonuses.MasteryofPast), spellbonuses.MasteryofPast);
	if (spellLevel < no_fizzle_level)
	{
		return true;
	}

	// spellFizzleAdjustment is a difficulty value stored with the spell data
	int spellFizzleAdjustment = spells[spell_id].basediff;

	// SE_CastingLevel adjusts the effective casting skill level
	// This effect is provided by some item focus effects, by wizard familiars, and is also used to make players fizzle away all their mana in some raid boss debuffs
	int spellCastingSkillTotalEffects = 
		+ itembonuses.effective_casting_level_for_fizzle_check
		+ spellbonuses.effective_casting_level_for_fizzle_check
		+ aabonuses.effective_casting_level_for_fizzle_check;

	int cappedChance = 95; // base chance
	int randomPenalty = 0;
	int effectiveSpellCastingSkill = 0;
	int spellLevelAdjustment = 0;
	int primeStatBonus = 0;

	if (spellFizzleAdjustment != 0 || spellCastingSkillTotalEffects < 0) // something is hindering our casting, either a debuff or fizzle adjustment in the spell
	{
		// this is a special case for Superior Healing
		// if (a4 == *(_DWORD*)(dword_805CB0 + 36) && *(_BYTE*)(v5 + 140) != 2)
		// PAL/DRU/SHM can cast this but at a higher level which makes it more likely to fizzle
		// the spell also has 25 adjustment in it, but that adjustment is only applied to clerics
		// this special case only has an effect for DRU/SHM because PAL would have 0 adjustment anyway due to another special case below
		// the end result is that at max level CLR/DRU/SHM are equally skilled at casting this spell
		if (spell_id == SPELL_SUPERIOR_HEALING && GetClass() != CLERIC)
		{
			spellFizzleAdjustment = 0;
		}
		// the fizzle adjustment in the spell data is overridden for high level spells (56+)
		if (spellLevel > 55)
		{
			spellFizzleAdjustment = 0;
		}
		// for PAL/RNG/SHD class the fizzle adjustment is overriden for spells 41+ instead
		if ((GetClass() == PALADIN || GetClass() == RANGER || GetClass() == SHADOWKNIGHT) && spellLevel > 40)
		{
			spellFizzleAdjustment = 0;
		}
		// bard fizzle adjustment capped to 15 max.
		if (GetClass() == BARD && spellFizzleAdjustment > 15)
		{
			// this is how it's coded in the client, however there are no bard songs that have an adjustment larger than 15 anyway so this has no effect
			spellFizzleAdjustment = 15;
		}

		// primeStat is just your WIS/INT or DEX+CHA combined for bard
		int primeStat = 0;
		if (GetClass() == BARD)
		{
			primeStat = (GetCHA() + GetDEX()) / 2;
		}
		else if (GetCasterClass() == 'W')
		{
			primeStat = GetWIS();
		}
		else if (GetCasterClass() == 'I')
		{
			primeStat = GetINT();
		}
		primeStatBonus = primeStat / 10; // integer math: 255 -> 25

		// effective spell level is capped at 50 for this formula
		int effectiveSpellLevel = spellLevel - 1;
		effectiveSpellLevel = effectiveSpellLevel > 50 ? 50 : effectiveSpellLevel;

		// spellCastingSkill is going to be 235 for everyone except while leveling up
		int spellCastingSkill = GetSkill(spells[spell_id].skill);

		effectiveSpellCastingSkill = spellCastingSkill + spellCastingSkillTotalEffects;
		effectiveSpellCastingSkill = effectiveSpellCastingSkill < 0 ? 0 : effectiveSpellCastingSkill;

		// up to a 10% penalty is added randomly to the chance calculation
		randomPenalty = zone->random.Int(0, 10);

		// chance
		spellLevelAdjustment = 5 * (18 - effectiveSpellLevel);
		int chance = 0
			+ effectiveSpellCastingSkill				// + 235
			+ (spellLevelAdjustment + primeStatBonus)	// + -135 (-160 + 25)
			- randomPenalty								// - 10
			- spellFizzleAdjustment;					// - 0
		// example chance 90 - 100 with 0 fizzleAdjustment and level 60 stats

		// cap chance
		cappedChance = chance;
		if (GetClass() == BARD)
		{
			// min 1 max 95 for bard
			cappedChance = cappedChance < 1 ? 1 : cappedChance > 95 ? 95 : cappedChance;
		}
		else if(GetClass() <= PLAYER_CLASS_COUNT)
		{
			// min 5 max 95 for non bard
			cappedChance = cappedChance < 5 ? 5 : cappedChance > 95 ? 95 : cappedChance;
		}
		else
		{
			// unknown class
			cappedChance = 0;
		}
	}

	// specializeSkill is the corresponding skill specialization like Specialize Abjuration
	// this is going to be 200 for the primary or 50 for secondary skills on maximum level characters
	int specializeSkill = GetSpecializeSkillValue(spell_id);
	int specializeAdjustment = 0;
	int spellCastingMasteryAdjustment = 0;
	if (specializeSkill > 0)
	{
		specializeAdjustment = specializeSkill / 10 + 1; // 200 skill = 21, 50 skill = 6 - these are directly added to the chance and help succeed

		int spellCastingMasteryLevel = GetAA(aaSpellCastingMastery);
		switch (spellCastingMasteryLevel)
		{
		case 1: spellCastingMasteryAdjustment = 2; break;
		case 2: spellCastingMasteryAdjustment = 5; break;
		case 3: spellCastingMasteryAdjustment = 10; break;
		}

		// with specialization you can get 98 chance instead of 95
		cappedChance = cappedChance + specializeAdjustment + spellCastingMasteryAdjustment;
		cappedChance = cappedChance > 98 ? 98 : cappedChance;
	}

	// roll 1-100
	int roll100 = zone->random.Int(1, 100);
	
	// if silenced, it is not possible to succeed
	if (IsSilenced())
	{
		roll100 = cappedChance + 1;
	}

	Log(Logs::Detail, Logs::Spells, "CheckFizzle: spell_id = %d, roll100 = %02d, cappedChance (%02d) = effectiveSpellCastingSkill (%d) + spellLevelAdjustment (%d) + primeStatBonus (%d) - randomPenalty (%d) - spellFizzleAdjustment (%d) + specializeAdjustment (%d) + spellCastingMasteryAdjustment (%d)", 
		spell_id, roll100, cappedChance, effectiveSpellCastingSkill, spellLevelAdjustment, primeStatBonus, randomPenalty, spellFizzleAdjustment, specializeAdjustment, spellCastingMasteryAdjustment);

	if (cappedChance >= roll100)
	{
		return true; // successful cast
	}

	return false; // fizzled
}

void Mob::ZeroCastingVars()
{
	// zero out the state keeping vars
	attacked_count = 0;
	spellend_timer.Disable();
	casting_spell_id = 0;
	casting_spell_targetid = 0;
	casting_spell_slot = CastingSlot::Invalid;
	casting_spell_mana = 0;
	casting_spell_inventory_slot = 0;
	casting_spell_timer = 0;
	casting_spell_timer_duration = 0;
	casting_spell_type = 0;
	casting_spell_resist_adjust = 0;
	casting_spell_focus_duration = 0;
	casting_spell_focus_range = 0;
	casting_aa = 0;
	casting_gm_override = 0;
	interrupt_message = 0;
}

void Mob::InterruptSpell(uint16 spellid, bool fizzle)
{
	if (spellid == SPELL_UNKNOWN && casting_spell_id)
		spellid = casting_spell_id;
	if (spellid == SPELL_UNKNOWN && bardsong)
		spellid = bardsong;

	int16 message = IsBardSong(spellid) ? SONG_ENDS_ABRUPTLY : INTERRUPT_SPELL;
	InterruptSpell(message, Chat::SpellFailure, spellid, fizzle);
}

void Mob::InterruptSpell(uint16 message, uint16 color, uint16 spellid, bool fizzle, bool message_others)
{
	uint16 message_other;
	if (spellid == SPELL_UNKNOWN) {
		if(bardsong) {
			spellid = bardsong;
		} else {
			spellid = casting_spell_id; // can still be 0 or 0xFFFF after this
		}
	}

	//Rest AA Timer on failed cast
	if(casting_spell_type == 1 && IsClient() && casting_aa != 0) 
	{
		CastToClient()->ResetAATimer(static_cast<aaID>(casting_aa), ABILITY_FAILED);
	}
	if (IsValidSpell(spellid) && message != SPELL_FIZZLE && IsClient() && CastToClient()->CheckAAEffect(aaEffectMassGroupBuff) && IsMGBCompatibleSpell(spellid)) {
		//CastToClient()->SendAATimer(static_cast<aaID>(aaEffectMassGroupBuff), static_cast<uint32>(time(nullptr)), static_cast<uint32>(time(nullptr)));
		CastToClient()->DisableAAEffect(aaEffectMassGroupBuff);
		CastToClient()->ResetSingleAATimer(static_cast<aaID>(aaMassGroupBuff), ABILITY_FAILED);
	}

	if (casting_spell_id && IsNPC())
		CastToNPC()->AI_Event_SpellCastFinished(false, static_cast<uint16>(casting_spell_slot));

	ZeroCastingVars();	// resets all the state keeping stuff
	_StopSong();

	if (IsValidSpell(spellid))
	{
		Log(Logs::Detail, Logs::Spells, "Spell %d has been interrupted.", spellid);

		// clients need some packets
		if (IsClient())
		{
			// when the client stops their own song, this function is called with SONG_ENDS and we shouldn't send a duplicate message
			// when we use InterruptSpell()'s overload above, this functions is called with SONG_ENDS_ABRUPTLY but this also creates an extra message
			if (message && message != SONG_ENDS && message != SONG_ENDS_ABRUPTLY)
			{
				// the interrupt message
				auto outapp = new EQApplicationPacket(OP_InterruptCast, sizeof(InterruptCast_Struct));
				InterruptCast_Struct *ic = (InterruptCast_Struct *)outapp->pBuffer;
				ic->messageid = message;
				ic->color = color;
				outapp->priority = 5;
				CastToClient()->QueuePacket(outapp);
				safe_delete(outapp);
			}

			if (message != SONG_ENDS)
			{
				SendSpellBarEnable(spellid);
			}
		}

		// notify people in the area
		if (message_others)
		{
			// first figure out what message others should get
			switch (message)
			{
			case SONG_ENDS:
				message_other = 0;
				color = Chat::Spells;
				break;
			case SONG_ENDS_ABRUPTLY:
				message_other = SONG_ENDS_ABRUPTLY_OTHER;
				color = Chat::Spells;
				break;
			case MISS_NOTE:
				message_other = MISSED_NOTE_OTHER;
				break;
			case SPELL_FIZZLE:
				message_other = SPELL_FIZZLE_OTHER;
				break;
			case TARGET_OUT_OF_RANGE:
			case TARGET_TOO_CLOSE:
			case CAST_OUTDOORS:
			case CANT_SEE_TARGET:
			case SPELL_RECOVERY:
			case SPELL_RECAST:
				message_other = 0;
				if (IsValidSpell(spellid) && GetClass() == BARD && IsBardSong(spellid))
				{
					message_other = SONG_ENDS_ABRUPTLY_OTHER;
					color = Chat::Spells;
				}
				break;
			default:
				message_other = INTERRUPT_SPELL_OTHER;
				color = Chat::Spells;
			}

			if (message_other > 0)
			{
				entity_list.MessageClose_StringID(this, true, 200, color, message_other, this->GetCleanName());
			}
		}
	}
}

// this is called after the timer is up and the spell is finished
// casting. everything goes through here, including items with zero cast time
// only to be used from SpellProcess
// NOTE: do not put range checking, etc into this function. this should
// just check timed spell specific things before passing off to SpellFinished
// which figures out proper targets etc
void Mob::CastedSpellFinished(uint16 spell_id, uint32 target_id, CastingSlot slot,
							uint16 mana_used, uint32 inventory_slot, int16 resist_adjust)
{
	if (!IsValidSpell(spell_id))
	{
		Log(Logs::Detail, Logs::Spells, "Casting of %d canceled: invalid spell id", spell_id);
		InterruptSpell();
		return;
	}

	bool IsFromItem = false;
	if (IsClient() && ((slot == CastingSlot::Item)))
	{
		IsFromItem = true;
	}

	if(IsClient() && slot != CastingSlot::Item) { // 10 is item
		if(!CastToClient()->GetPTimers().Expired(&database, pTimerSpellStart + spell_id, false)) {
			//should we issue a message or send them a spell gem packet?
			Log(Logs::Detail, Logs::Spells, "Casting of %d canceled: spell reuse timer not expired", spell_id);
			InterruptSpell(SPELL_NOT_RECOVERED, Chat::SpellFailure, spell_id);
			return;
		}
	}

	// Prevent rapid recast of normal spells. Clickies are exempt. 
	if(IsClient() && slot != CastingSlot::Item && slot != CastingSlot::Ability && !IsDisc(spell_id))
	{
		if(spellrecovery_timer.Enabled())
		{
			Log(Logs::General, Logs::Spells, "Casting of %d cancelled: recast too quickly", spell_id);
			InterruptSpell(SPELL_RECOVERY, Chat::SpellFailure, spell_id);
			return;
		}
	}

	// make sure they aren't somehow casting 2 timed spells at once
	if (casting_spell_id != spell_id)
	{
		Log(Logs::Detail, Logs::Spells, "Casting of %d canceled: already casting", spell_id);
		InterruptSpell(ALREADY_CASTING, Chat::SpellFailure, spell_id);
		return;
	}

	bool regain_conc = false;
	Mob *spell_target = entity_list.GetMob(target_id);
	// here we do different things if this is a bard casting a bard song
	if(GetClass() == BARD) // bard's can move when casting any spell...
	{
		if (spells[spell_id].bardsong)
		{
			if(spells[spell_id].recast_time != 0 || spells[spell_id].mana != 0)
			{
				Log(Logs::Detail, Logs::Spells, "Bard song %d not applying bard logic because duration or recast is wrong: dur=%d, recast=%d, mana=%d", spell_id, spells[spell_id].buffduration, spells[spell_id].recast_time, spells[spell_id].mana);
			} 
			else 
			{
				bardsong = spell_id;
				bardsong_slot = slot;
				//NOTE: theres a lot more target types than this to think about...
				if (spell_target == nullptr || (spells[spell_id].targettype != ST_Target && spells[spell_id].targettype != ST_AETarget))
					bardsong_target_id = GetID();
				else
					bardsong_target_id = spell_target->GetID();
				bardsong_timer.Start(6000);
				Log(Logs::Detail, Logs::Spells, "Bard song %d started: slot %d, target id %d", bardsong, bardsong_slot, bardsong_target_id);
			}
		}
	}
	else if (!spells[spell_id].uninterruptable) // not bard, check movement
	{
		// special case - this item can be cast while moving, by any player, not just bards
		if ((IsClient() && ((slot == CastingSlot::Item)) && inventory_slot != 0xFFFFFFFF) &&
			CastToClient()->GetItemIDAt(inventory_slot) == 28906 /* Bracelet of the Shadow Hive */)
		{
			Log(Logs::Detail, Logs::Spells, "Casting from clicky item %s.  Allowing cast while moving.", CastToClient()->GetInv()[inventory_slot]->GetItem()->Name);
		}
		// if has been attacked, or moved while casting, try to channel
		else if (attacked_count > 0 || GetX() != GetSpellX() || GetY() != GetSpellY())
		{
			uint16 channel_chance = 0;

			if (IsClient())
			{
				// this is fairly accurate (but not perfect) and based on decompiles

				// client is holding down forward movement key or no skill
				if (animation > 0 || GetSkill(EQ::skills::SkillChanneling) == 0)
				{
					InterruptSpell();
					return;
				}

				float d_x, d_y;
				if (GetX() != GetSpellX() || GetY() != GetSpellY())
				{
					d_x = std::abs(std::abs(GetX()) - std::abs(GetSpellX()));
					d_y = std::abs(std::abs(GetY()) - std::abs(GetSpellY()));
					if (d_x > 1.00001f || d_y > 1.00001f)
					{
						InterruptSpell();
						return;
					}
				}
				if (attacked_count == 0)
					attacked_count = 1;

				uint16 bonus_chance = 0;
				uint16 spell_level = spells[spell_id].classes[GetClass() - 1];
				uint16 roll = 0;
				uint16 loops = 0;

				do {
					roll = zone->random.Int(1, 390);
					if (!IsFromItem && aabonuses.ChannelChanceSpells)
					{
						roll = (100 - aabonuses.ChannelChanceSpells) * roll / 100;
					}

					if (GetLevel() > (spell_level + 5))
						bonus_chance = 3 * (GetLevel() - spell_level) + 35;

					channel_chance = GetSkill(EQ::skills::SkillChanneling) + GetLevel() + bonus_chance;
					if (channel_chance > 370)
						channel_chance = 370;

					Log(Logs::Detail, Logs::Spells, "Checking Interruption: spell x: %f  spell y: %f  cur x: %f  cur y: %f channelchance %f channeling skill %d\n", GetSpellX(), GetSpellY(), GetX(), GetY(), channel_chance, GetSkill(EQ::skills::SkillChanneling));

					if (roll > channel_chance && roll >= 39)
					{
						Log(Logs::Detail, Logs::Spells, "Casting of %d canceled: interrupted.", spell_id);
						InterruptSpell(SPELL_UNKNOWN, true);
						return;
					}

					loops++;
				} while (loops < attacked_count);

				regain_conc = true;
			}
			else
			{
				// NPCs don't use channeling skill (says Rashere circa 2006) or fail spells from push
				// this is an extremely crude approximation but based on AK log greps of a handful of NPCs

				// hit NPCs typically don't even regain concentration and just finish casts
				int roll = 30;
				if (GetLevel() > 70)
					roll = 10;
				else if (GetLevel() > 30)
					roll = 20;

				// this spell id range is hardcoded to not be interruptable.
				// this was determined from a client decompile.  credit: Mackal
				if (IsNPC() && spell_id >= 859 && spell_id <= 1023)
					roll = 0;

				if (zone->random.Roll(roll))
				{
					// caster NPCs have an extremely high regains rate; almost 100% at high levels
					roll = zone->random.Int(1, 390);

					// AK logs show some warrior NPCs (e.g. Tallon Zek) not regaining concentration; maybe NPCs used channeling skill in 2002 but not 2006?  shrug
					if (GetClass() == WARRIOR || GetClass() == ROGUE || GetClass() == MONK)
						roll = 0;

					if (roll > ((GetLevel() < 51 ? 325 : 225) - GetLevel() * 3))		// not how Sony did it; replace this if you find data/evidence/leaks
						regain_conc = true;
					else
					{
						Log(Logs::Detail, Logs::Spells, "Casting of %d canceled: interrupted.", spell_id);
						InterruptSpell(SPELL_UNKNOWN, true);
						return;
					}
				}
			}

			if (regain_conc)
			{
				Message_StringID(Chat::Spells, REGAIN_AND_CONTINUE);
				uint16 textcolor = IsNPC() ? Chat::DefaultText : Chat::Spells;
				entity_list.MessageClose_StringID(this, true, 200, textcolor, OTHER_REGAIN_CAST, this->GetCleanName());
			}
		}		// mob was hit or moved from start of cast loc
	}		// class != bard

	if(IsCorpseSummon(spell_id))
	{
		if(!GetTarget() || !GetTarget()->IsClient() || !IsClient())
		{
			InterruptSpell();
			return;
		}

		if (!GetTarget()->InSameGroup(this))
		{
			InterruptSpell(CORPSE_SUMMON_TAR, Chat::SpellFailure, spell_id);
			return;
		}
	}

	// Check for consumables and Reagent focus items
	// first check for component reduction
	if(IsClient() && RequiresComponents(spell_id) && 
		(slot != CastingSlot::Item || 
		(slot == CastingSlot::Item && !CastToClient()->ClickyOverride()))) 
	{
		Log(Logs::Detail, Logs::Spells, "Spell %d: requires a component.", spell_id);

		// bard instrument is checked in SpellFinished()
		if (GetClass() != BARD && !spells[spell_id].bardsong && !HasSpellReagent(spell_id))
		{
			InterruptSpell();
			return;
		}
	} 

	// if this was cast from an inventory slot, check out the item that's there
	int16 DeleteChargeFromSlot = -1;

	if(IsClient() && ((slot == CastingSlot::Item))
		&& inventory_slot != 0xFFFFFFFF)	// 10 is an item
	{
		const EQ::ItemInstance* inst = CastToClient()->GetInv()[inventory_slot];

		if (inst && inst->IsType(EQ::item::ItemClassCommon) && (inst->GetItem()->Click.Effect == spell_id) && inst->GetCharges())
		{
			//const EQ::ItemData* item = inst->GetItem();
			int16 charges = inst->GetItem()->MaxCharges;

			if(charges > -1) {	// charged item, expend a charge
				Log(Logs::Detail, Logs::Spells, "Spell %d: Consuming a charge from item %s (%d) which had %d/%d charges.", spell_id, inst->GetItem()->Name, inst->GetItem()->ID, inst->GetCharges(), inst->GetItem()->MaxCharges);
				DeleteChargeFromSlot = inventory_slot;
			} else {
				Log(Logs::Detail, Logs::Spells, "Spell %d: Cast from unlimited charge item %s (%d) (%d charges)", spell_id, inst->GetItem()->Name, inst->GetItem()->ID, inst->GetItem()->MaxCharges);
			}
		}
		else
		{
			Log(Logs::Detail, Logs::Spells, "Item used to cast spell %d was missing from inventory slot %d after casting!", spell_id, inventory_slot);
			InterruptSpell();
			return;
		}
	}

	// we're done casting, now try to apply the spell
	if (!SpellFinished(spell_id, spell_target, slot, mana_used, inventory_slot, resist_adjust))
	{
		Log(Logs::Detail, Logs::Spells, "Casting of %d canceled: SpellFinished returned false.", spell_id);
		if (interrupt_message > 0)
			InterruptSpell(interrupt_message, Chat::SpellFailure, spell_id);
		else
			InterruptSpell();
		return;
	}

	if(DeleteChargeFromSlot >= 0)
		CastToClient()->DeleteItemInInventory(DeleteChargeFromSlot, 1, true);

	//
	// at this point the spell has successfully been cast
	//

	std::string export_string = fmt::format("{}", spell_id);
	if(IsClient()) {
		parse->EventPlayer(EVENT_CAST, CastToClient(), export_string, 0);
	} else if(IsNPC()) {
		parse->EventNPC(EVENT_CAST, CastToNPC(), nullptr, export_string, 0);
	}

	if(IsClient())
	{
		Client *c = CastToClient();

		// bard songs have to be stopped manually, the gems don't pop out on their own, even on nonrepeating songs (those which have a recast delay or mana cost)
		bool skipSpellRefresh = false;
		if (GetClass() == BARD && (IsBardSong(spell_id) || HasActiveSong()))
			skipSpellRefresh = true;

		if (slot < CastingSlot::MaxGems || slot == CastingSlot::Ability)
		{
			// this causes the delayed refresh of the spell bar gems and sets the recast delay timer for this slot on the client for non bards
			c->MemorizeSpell(static_cast<uint32>(slot), spell_id, memSpellSpellbar);
		}

		// this tells the client that casting may happen again
		if (!skipSpellRefresh)
		{
			SendSpellBarEnable(spell_id);
		}

		// skills
		if(slot < CastingSlot::MaxGems)
		{
			// increased chance of gaining channel skill if you regained concentration
			float chan_skill = zone->skill_difficulty[EQ::skills::SkillChanneling].difficulty;
			float final_diff = regain_conc ? chan_skill : chan_skill+2;
			uint8 success = regain_conc ? SKILLUP_SUCCESS : SKILLUP_FAILURE;
			c->CheckIncreaseSkill(EQ::skills::SkillChanneling, nullptr, final_diff, success);
		}
	}

	// there should be no casting going on now
	ZeroCastingVars();

	// Clickies did not have a delay timer on AK. There are no items in the DB with a clickeffect and recastdelay > 0.
	if (IsClient() && slot != CastingSlot::Item && slot != CastingSlot::Ability && !IsDisc(spell_id))
	{
		// set the rapid recast timer for next time around
		spellrecovery_timer.Start(RuleI(Spells,SpellRecoveryTimer), true);
		Log(Logs::Detail, Logs::Spells, "Spell recovery timer set.");
	}

	Log(Logs::Detail, Logs::Spells, "Spell casting of %d is finished.", spell_id);

}

bool Mob::HasSongInstrument(uint16 spell_id){
	bool HasInstrument = true;
	Client *c = this->CastToClient();
	int InstComponent = spells[spell_id].NoexpendReagent[0];

	switch (InstComponent) {
	case -1:
		return true;		// no instrument required

		// percussion songs (13000 = hand drum)
	case 13000:
		if (itembonuses.percussionMod == 0) {			// check for the appropriate instrument type
			HasInstrument = false;
			c->Message_StringID(Chat::Red, SONG_NEEDS_DRUM);	// send an error message if missing
		}
		break;

		// wind songs (13001 = wooden flute)
	case 13001:
		if (itembonuses.windMod == 0) {
			HasInstrument = false;
			c->Message_StringID(Chat::Red, SONG_NEEDS_WIND);
		}
		break;

		// string songs (13011 = lute)
	case 13011:
		if (itembonuses.stringedMod == 0) {
			HasInstrument = false;
			c->Message_StringID(Chat::Red, SONG_NEEDS_STRINGS);
		}
		break;

		// brass songs (13012 = horn)
	case 13012:
		if (itembonuses.brassMod == 0) {
			HasInstrument = false;
			c->Message_StringID(Chat::Red, SONG_NEEDS_BRASS);
		}
		break;

	default:	// some non-instrument component. Let it go, but record it in the log
		Log(Logs::Detail, Logs::Spells, "Something odd happened: Song %d required instrument %d", spell_id, InstComponent);
	}

	if (!HasInstrument) {	// if the instrument is missing, log it and interrupt the song
		Log(Logs::Detail, Logs::Spells, "Song %d: Canceled. Missing required instrument %d", spell_id, InstComponent);
		if (c->GetGM())
			c->Message(Chat::White, "Your GM status allows you to finish casting even though you're missing a required instrument.");
		else {
			return false;
		}
	}
	return true;
}

// this actually consumes the reagents, it doesn't just check for them
bool Mob::HasSpellReagent(uint16 spell_id)
{
	static const int16 petfocusItems[] = { 20508, 28144, 11571, 11569, 11567, 11566, 11568, 6360, 6361, 6362, 6363 };
	Client *c = this->CastToClient();

	bool missingreags = false;

	for (int t_count = 0; t_count < 4; t_count++) { 	//check normal components first, like Malachite for mage pets
		int32 component = spells[spell_id].components[t_count];
		int component_count = spells[spell_id].component_counts[t_count];

		if (component == -1){
			continue;
		}

		if (!HasReagent(spell_id, component, component_count, missingreags))
		{
			missingreags = true;
		}
	}

	for (int t_count = 0; t_count < 4; t_count++) { //check focus components like Fire Beetle Eye for Flame Lick
		int focuscomponent = spells[spell_id].NoexpendReagent[t_count];

		if (focuscomponent == -1)
			continue;

		bool petfocuscomponent = false;
		int8 petfocusItemsize = sizeof(petfocusItems) / sizeof(petfocusItems[0]);
		for (int i = 0; i < petfocusItemsize; i++) {
			if (focuscomponent == petfocusItems[i]) 
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d uses a pet focus %d, additonal checks will be skipped.", spell_id, petfocusItems[i]);
				petfocuscomponent = true;
				break;
			}
		}

		if (petfocuscomponent){//Ignore pet focus items, they are not required to cast pets.
			continue;
		}

		const EQ::ItemData *item = database.GetItem(focuscomponent);
		if(!item && focuscomponent != -1)
		{
			Log(Logs::Detail, Logs::Spells, "UNKNOWN item found in spell data. Please make sure your database is correct.");
		}
		else if (item && !HasReagent(spell_id, focuscomponent, 1, missingreags))
		{
			missingreags = true;
		}
	}

	if (missingreags) 
	{
		if (c->GetGM())
			c->Message(Chat::White, "Your GM status allows you to finish casting even though you're missing required components.");
		else {
			return false;
		}
	}
	else
	{
		// mage AA still requires reagents but does not consume them
		if (GetClass() == MAGICIAN && GetAA(aaElementalPact) > 0 && GetSpellEffectIndex(spell_id, SE_SummonPet) != -1)
			return true;

		std::string item_name;
		int reg_focus = CastToClient()->GetFocusEffect(focusReagentCost, spell_id, item_name);

		for (int t_count = 0; t_count < 4; t_count++) // loop through each of the 4 possible component slots
		{
			int32 component = spells[spell_id].components[t_count];
			if (component == -1)
				continue;
			int component_count = spells[spell_id].component_counts[t_count];

			for (int t_component_count = 0; t_component_count < component_count; t_component_count++) // loop for each quantity of each reagent, it's not all or nothing, each one rolls
			{
				// these items can't benefit from reagent conservation even if the focus filters would apply (from client decompile)
				bool exempt_reagent = component == 9963 /* Essence Emerald */ || component == 10092 /* Fuligan Soulstone of Innoruuk */ || component == 10094 /* Cloudy Stone of Veeshan */;

				// roll for reagent conservation
				if (!exempt_reagent && zone->random.Roll(reg_focus))
				{
					// roll success, conserve reagent
					if (item_name.length() > 0)
					{
						Message_StringID(Chat::Spells, BEGINS_TO_SHINE, item_name.c_str());
					}
					Log(Logs::General, Logs::Focus, "focusReagentCost prevented reagent consumption (%d chance) spell_id %d item_id %d", reg_focus, spell_id, component);
				}
				else
				{
					// roll failed or not a conservable reagent
					if (reg_focus > 0)
						Log(Logs::General, Logs::Focus, "focusReagentCost failed to prevent reagent consumption (%d chance, exempt_reagent = %d) spell_id %d item_id %d", reg_focus, exempt_reagent, spell_id, component);
					Log(Logs::Detail, Logs::Spells, "Spell %d: Consuming 1 of spell component item id %d", spell_id, component);
					// Components found, Deleting
					// now we go looking for and deleting the items one by one
					int16 inv_slot_id = c->GetInv().HasItem(component, 1, invWhereWorn | invWherePersonal);
					if (inv_slot_id != -1)
					{
						c->DeleteItemInInventory(inv_slot_id, 1, true);
					}
					else
					{	// some kind of error in the code if this happens
						Log(Logs::Detail, Logs::Spells, "ERROR: reagent item disappeared while processing?");
					}
				}
			}
		}
	}
	return true;
}

bool Mob::HasReagent(uint16 spell_id, int component, int component_count, bool missingreags){
	Client *c = this->CastToClient();
	if (c->GetInv().HasItem(component, component_count, invWhereWorn | invWherePersonal) == -1){
		if (!missingreags)
		{
			c->Message_StringID(Chat::SpellFailure, MISSING_SPELL_COMP);
		}

		const EQ::ItemData *item = nullptr;
		if (component != -1)
		{
			item = database.GetItem(component);
		}

		if (item) {
			c->Message_StringID(Chat::SpellFailure, MISSING_SPELL_COMP_ITEM, item->Name);
			Log(Logs::Detail, Logs::Spells, "Spell %d: Canceled. Missing required reagent %s (%d)", spell_id, item->Name, component);
		}
		else {
			char TempItemName[64];
			strcpy((char*)&TempItemName, "UNKNOWN");
			Log(Logs::Detail, Logs::Spells, "Spell %d: Canceled. Missing required reagent %s (%d)", spell_id, TempItemName, component);
		}
		return false;
	}
	return true;
}

bool Mob::DetermineSpellTargets(uint16 spell_id, Mob *&spell_target, Mob *&ae_center, CastAction_type &CastAction, bool isproc, CastingSlot slot) {

/*
	The basic types of spells:

	Single target - some might be undead only, self only, etc, but these
	all affect the target of the caster.

	AE around caster - these affect entities close to the caster, and have
	no target.

	AE around target - these have a target, and affect the target as well as
	entities close to the target.

	AE on location - this is a tricky one that is cast on a mob target but
	has a special AE duration that keeps it recasting every 2.5 sec on the
	same location. These work the same as AE around target spells, except
	the target is a special beacon that's created when the spell is cast

	Group - the caster is always affected, but there's more
		targetgroupbuffs on - these affect the target and the target's group.
		targetgroupbuffs off - no target, affects the caster's group.

	Group Teleport - the caster plus his group are affected. these cannot
	be targeted.

	I think the string ID SPELL_NEED_TAR is wrong, it dosent seem to show up.
*/

	// during this switch, this variable gets set to one of these things
	// and that causes the spell to be executed differently

	bodyType target_bt = BT_Humanoid;
	SpellTargetType targetType = spells[spell_id].targettype;
	bodyType mob_body = spell_target ? spell_target->GetBodyType() : BT_Humanoid;

	if(IsClient()
		&& spell_target != nullptr // null ptr crash safeguard
		&& spell_target->IsClient() // still self only if NPC targetted
		&& CastToClient()->CheckAAEffect(aaEffectProjectIllusion)
		&& IsPlayerIllusionSpell(spell_id)
		&& InSameGroup(spell_target) // still self only if not grouped
	)
	{
			Log(Logs::Detail, Logs::AA, "Project Illusion overwrote target caster: %s spell id: %d was ON", GetName(), spell_id);
			targetType = ST_ProjectIllusion;
	}

	switch (targetType)
	{
		// single target spells
		case ST_Self:
		{
			// innate NPC procs always hit the target, even self-only spells (like scareling step)
			if (!isproc || !IsNPC() || (IsNPC() && CastToNPC()->GetInnateProcSpellId() != spell_id))
				spell_target = this;

			CastAction = SingleTarget;
			break;
		}

		// bolt spells and flare
		case ST_TargetOptional:
		{
			CastAction = Projectile;
			break;
		}

		// target required for these
		case ST_Undead: {
			if(!spell_target || (
				mob_body != BT_SummonedUndead
				&& mob_body != BT_Undead
				&& mob_body != BT_Vampire
				)
			)
			{
				//invalid target
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target of body type %d (undead)", spell_id, mob_body);
				if (!spell_target)
					Message_StringID(Chat::Red, SPELL_NEED_TAR);
				else
					Message_StringID(Chat::Red, CANNOT_AFFECT_NPC);
				return false;
			}
			CastAction = SingleTarget;
			break;
		}

		case ST_Summoned: {
			if(!spell_target || (mob_body != BT_Summoned && mob_body != BT_Summoned2 && mob_body != BT_Summoned3))
			{
				//invalid target
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target of body type %d (summoned)", spell_id, mob_body);
				Message_StringID(Chat::Red,SPELL_NEED_TAR);
				return false;
			}
			CastAction = SingleTarget;
			break;
		}

		//single body type target spells...
		//this is a little hackish, but better than duplicating code IMO
		case ST_Plant: if(target_bt == BT_Humanoid) target_bt = BT_Plant;
		case ST_UberDragon: if(target_bt == BT_Humanoid) target_bt = BT_VeliousDragon;
		case ST_UberGiant: if(target_bt == BT_Humanoid) target_bt = BT_BaneGiant;
		case ST_Animal: if(target_bt == BT_Humanoid) target_bt = BT_Animal;
		{
			if(!spell_target || mob_body != target_bt)
			{
				//invalid target
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target of body type %d (want body Type %d)", spell_id, mob_body, target_bt);
				if(!spell_target)
					Message_StringID(Chat::Red,SPELL_NEED_TAR);
				else
					Message_StringID(Chat::Red,CANNOT_AFFECT_NPC);
				// leaving this here for reference how to reset AA timer easy
				// this breaks casting, because it zero's casting vars, so when
				// this returns false, it is unable to perform spell interrupt correctly.
				//if (IsClient() && casting_aa == aaDireCharm)
				///{
				//	InterruptSpell();
					//CastToClient()->ResetAATimer(aaDireCharm, ABILITY_FAILED);
				//}
				return false;
			}
			CastAction = SingleTarget;
			break;
		}

		case ST_Tap:
		case ST_Target: 
		{
			if(!spell_target)
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target (normal)", spell_id);
				Message_StringID(Chat::Red,SPELL_NEED_TAR);
				return false;	// can't cast these unless we have a target
			}
			CastAction = SingleTarget;
			break;
		}

		case ST_Corpse:
		{
			if(!spell_target || !spell_target->IsPlayerCorpse())
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target (corpse)", spell_id);
				uint32 message = ONLY_ON_CORPSES;
				if(!spell_target) message = SPELL_NEED_TAR;
				else if(!spell_target->IsCorpse()) message = ONLY_ON_CORPSES;
				else if(!spell_target->IsPlayerCorpse()) message = CORPSE_NOT_VALID;
				Message_StringID(Chat::Red, message);
				return false;
			}
			CastAction = SingleTarget;
			break;
		}
		case ST_Pet:
		{
			if(GetPet())
			{
				if(IsEffectInSpell(spell_id, SE_VoiceGraft))
					spell_target = this;
				else
					spell_target = GetPet();
			}
			else
			{
				spell_target = nullptr;
			}

			if(!spell_target)
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target (no pet)", spell_id);
				Message_StringID(Chat::Red,NO_PET);
				return false;	// can't cast these unless we have a target
			}
			CastAction = SingleTarget;
			break;
		}

		case ST_AEClientV1:
		case ST_AEBard:
		case ST_AECaster:
		case ST_UndeadAE:
		case ST_SummonedAE:
		{
			ae_center = this;
			CastAction = AECaster;
			break;
		}

		case ST_TargetAETap:
		case ST_AETarget:
		{
			if(!spell_target)
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target (AOE)", spell_id);
				Message_StringID(Chat::Red,SPELL_NEED_TAR);
				return false;
			}
			ae_center = spell_target;
			if (spells[spell_id].targettype == ST_AETarget && spells[spell_id].aoerange == 0 && spells[spell_id].goodEffect == 0 && // Torment's Beckon is beneficial, only single target since it has 0 aoerange
				(spells[spell_id].effectid[0] == SE_Teleport || spells[spell_id].effectid[0] == SE_Teleport2 )) // some PoP scripted teleport spells cast by NPCs
			{
				CastAction = GroupSpell;
			}
			else
			{
				CastAction = AETarget;
			}
			break;
		}

		// Group spells
		case ST_GroupTeleport:
		case ST_Group:
		{
			if(IsClient() && CastToClient()->TGB() && IsTGBCompatibleSpell(spell_id) && (casting_aa != 0 || slot != CastingSlot::Item))
			{
				Mob* original_target = spell_target;
				if (!spell_target || (spell_target && !spell_target->IsClient()))
				{
					spell_target = this;
				}
				else if (spell_target && spell_target != this)
				{
					float range = spells[spell_id].aoerange;
					
					range = GetSpellRange(spell_id, range);
					float dist2 = DistanceSquaredNoZ(m_Position, spell_target->GetPosition());
					float dist3 = std::abs(GetZ() - spell_target->GetZ());
					float range2 = range * range;
					if (dist2 > range2 || dist3 > range) 
					{
						//target is out of range.
						spell_target = this;
					}
				}

				if (spell_target && spell_target != original_target)
					Log(Logs::Detail, Logs::Spells, "Spell has TGB active. Original target was %s Current target is %s", original_target ? original_target->GetName() : "null", spell_target->GetName());
			} 
			else 
			{
				spell_target = this;
			}
			CastAction = GroupSpell;
			break;
		}
		case ST_ProjectIllusion: // this is only used for signaling project illusion to DetermineSpellTargets
		{
			if(!spell_target)
			{
				Log(Logs::Detail, Logs::Spells, "Spell %d canceled: invalid target (Group Required: Single Target)", spell_id);
				Message_StringID(Chat::Red,SPELL_NEED_TAR);
				return false;
			}

			CastAction = SingleTarget;
			break;
		}

		default:
		{
			Log(Logs::Detail, Logs::Spells, "I dont know Target Type: %d   Spell: (%d) %s", spells[spell_id].targettype, spell_id, spells[spell_id].name);
			Message(Chat::White, "I dont know Target Type: %d   Spell: (%d) %s", spells[spell_id].targettype, spell_id, spells[spell_id].name);
			CastAction = CastActUnknown;
			break;
		}
	}
	return(true);
}

// only used from CastedSpellFinished, and procs
// we can't interrupt in this, or anything called from this!
// if you need to abort the casting, return false
bool Mob::SpellFinished(uint16 spell_id, Mob *spell_target, CastingSlot slot, uint16 mana_used,
						uint32 inventory_slot, int16 resist_adjust, bool isproc, bool isrecourse, int recourse_level)
{
	//EQApplicationPacket *outapp = nullptr;
	Mob *ae_center = nullptr;

	if(!IsValidSpell(spell_id))
		return false;

	if( spells[spell_id].zonetype == 1 && !zone->CanCastOutdoor()){
		if(IsClient()){
			if(!CastToClient()->GetGM()){
				interrupt_message = CAST_OUTDOORS;
				return false;
			}
		}
	}

	if(IsEffectInSpell(spell_id, SE_Levitate) && !zone->CanLevitate()){
		if(IsClient()){
			if(!CastToClient()->GetGM()){
				Message(Chat::SpellFailure, "You can't levitate in this zone.");
				return false;
			}
		}
	}

	if (IsBardSong(spell_id) && !HasSongInstrument(spell_id))
	{
		return false;
	}

	if (spell_target && spell_id == SPELL_CAZIC_TOUCH && IsNPC() && zone->GetZoneExpansion() != PlanesEQ)
	{
		Shout("%s!",spell_target->name);
	}

	if(IsClient() && !CastToClient()->GetGM()){

		if(zone->IsSpellBlocked(spell_id, glm::vec3(GetPosition()))){
			const char *msg = zone->GetSpellBlockedMessage(spell_id, glm::vec3(GetPosition()));
			if(msg){
				Message(Chat::SpellFailure, msg);
				return false;
			}
			else{
				Message(Chat::SpellFailure, "You can't cast this spell here.");
				return false;
			}

		}
	}

	if
	(
		this->IsClient() &&	
		(zone->GetZoneID() == Zones::TUTORIAL || zone->GetZoneID() == Zones::LOAD) &&
		CastToClient()->Admin() < 80
	)
	{
		if
		(
			IsEffectInSpell(spell_id, SE_Gate) ||
			IsEffectInSpell(spell_id, SE_Translocate) ||
			IsEffectInSpell(spell_id, SE_Teleport)
		)
		{
			Message(Chat::White, "The Gods brought you here, only they can send you away.");
			return false;
		}
	}

	//determine the type of spell target we have
	CastAction_type CastAction;
	if (!DetermineSpellTargets(spell_id, spell_target, ae_center, CastAction, isproc, slot))
	{
		Log(Logs::Detail, Logs::Spells, "Spell %d: DetermineSpellTargets has failed.", spell_id);
		return(false);
	}

	Log(Logs::Detail, Logs::Spells, "Spell %d: target type %d, target %s, AE center %s", spell_id, CastAction, spell_target?spell_target->GetName():"NONE", ae_center?ae_center->GetName():"NONE");

	if (!isproc && CastAction != AECaster && spell_target != nullptr && spell_target != this) 
	{
		//range check our target, if we have one and it is not us
		float range = spells[spell_id].range;
		if (IsClient() && CastToClient()->TGB() && IsTGBCompatibleSpell(spell_id) && IsGroupSpell(spell_id) && (casting_aa != 0 || slot != CastingSlot::Item))
			range = spells[spell_id].aoerange;

		range = GetSpellRange(spell_id, range);
		if (IsPlayerIllusionSpell(spell_id)
			&& IsClient()
			&& CastToClient()->CheckAAEffect(aaEffectProjectIllusion)) 
		{
			range = 100;
		}

		Log(Logs::Detail, Logs::Spells, "Spell %d: Performing second range check.", spell_id);

		//casting a spell on somebody but ourself, make sure they are in range
		float dist2 = DistanceSquaredNoZ(m_Position, spell_target->GetPosition());
		float dist3 = std::abs(GetZ() - spell_target->GetZ());
		float range2 = range * range;
		if(dist2 > range2 || (dist3 > range && CastAction != SingleTarget))
		{
			//target is out of range.
			Log(Logs::Detail, Logs::Spells, "Spell %d: Spell target is out of range (squared: %f > %f)", spell_id, dist2, range2);
			interrupt_message = TARGET_OUT_OF_RANGE;
			return(false);
		}
	}

	// NPC innate procs that are AoE spells only hit the target they are attacking
	if (IsNPC() && isproc && CastToNPC()->GetInnateProcSpellId() == spell_id && (CastAction == AETarget || CastAction == AECaster))
	{
		CastAction = SingleTarget;
	}
	// if a spell has the AEDuration flag, it becomes an AE on target
	// spell that's recast every 2500 msec for AEDuration msec. There are
	// spells of all kinds of target types that do this, strangely enough
	else if (IsAEDurationSpell(spell_id))
	{
		// the spells are AE target, but we aim them on a beacon
		Mob *beacon_loc = spell_target ? spell_target : this;
		auto beacon = new Beacon(beacon_loc, spells[spell_id].AEDuration);
		entity_list.AddBeacon(beacon);
		Log(Logs::Detail, Logs::Spells, "Spell %d: AE duration beacon created, entity id %d", spell_id, beacon->GetID());
		spell_target = nullptr;
		ae_center = beacon;
		CastAction = AECaster;
	}


	// a hackish way to let CheckHealAggroAmount() called from SpellOnTarget() to know that the spell comes from a clickable
	// adding another parameter to SpellOnTarget() just for this would be rather bloaty
	if (IsClient() && slot == CastingSlot::Item && IsBuffSpell(spell_id))
		isproc = true;

	//
	// Switch #2 - execute the spell
	//
	switch(CastAction)
	{
		default:
		case CastActUnknown:
		case SingleTarget:
		{

			if(spell_target == nullptr) {
				Log(Logs::Detail, Logs::Spells, "Spell %d: Targeted spell, but we have no target", spell_id);
				return(false);
			}
			if (isproc || slot == CastingSlot::Item) {
				if (!SpellOnTarget(spell_id, spell_target, false, true, resist_adjust, isproc, 0, isrecourse, recourse_level)) {
					if (IsDireCharmSpell(spell_id)) {
						if (casting_spell_type == 1)
							InterruptSpell();
					}
				}
			} else {
				if(!SpellOnTarget(spell_id, spell_target, false, true, resist_adjust, false, 0, isrecourse, recourse_level)) {
					if(IsBuffSpell(spell_id) && IsBeneficialSpell(spell_id)) {
						// Prevent mana usage/timers being set for beneficial buffs
						if(casting_spell_type == 1)
							InterruptSpell();
						return false;
					}
				}
			}

			if(IsPlayerIllusionSpell(spell_id) && IsClient())
			{ 
				if(CastToClient()->CheckAAEffect(aaEffectProjectIllusion))
				{
					Log(Logs::Detail, Logs::AA, "Effect Project Illusion for %s on spell id: %d was ON", GetName(), spell_id);
					CastToClient()->DisableAAEffect(aaEffectProjectIllusion);
				}
				else
				{
					Log(Logs::Detail, Logs::AA, "Effect Project Illusion for %s on spell id: %d was OFF", GetName(), spell_id);
				}
			}
			break;
		}

		case Projectile:
		{
			if (spell_target == nullptr || spell_target == this)
			{
				// no target or self target - shoot it straight forward at a beacon ahead of us

				glm::vec4 aimpos = glm::vec4(GetPosition());
				float headingRadians = GetHeading();
				headingRadians = (headingRadians * 360.0f) / 256.0f;	// convert to degrees first; heading range is 0-255
				if (headingRadians < 270.0f)
					headingRadians += 90.0f;
				else
					headingRadians -= 270.0f;
				headingRadians = headingRadians * 3.141592654f / 180.0f;

				aimpos.x -= cosf(headingRadians) * 300; // bolt range is only 300
				aimpos.y += sinf(headingRadians) * 300;
				Beacon *aim = new Beacon(this, 10000);
				aim->SetPosition(aimpos);
				entity_list.AddBeacon(aim);
				spell_target = aim;
			}

			if (isproc) // Dagarns Tail
			{
				SpellOnTarget(spell_id, spell_target, false, true, resist_adjust, isproc);
			}
			else
			{
				// launch bolt at target
				Beacon *beacon = new Beacon(this, 10000);

				entity_list.AddBeacon(beacon);
				if (beacon)
					beacon->BoltSpell(this, spell_target, spell_id);
				if (IsNPC())
					CastToNPC()->AI_Event_SpellCastFinished(true, static_cast<uint16>(slot));
				ProjectileAnimation(spell_target ? spell_target : this, 0, false, 1.0f);
				break;

			}
			break;
		}

		case AECaster:
		case AETarget:
		{
			// we can't cast an AE spell without something to center it on
			assert(ae_center != nullptr);

			if (ae_center->IsBeacon()) {
				// special ae duration spell
				ae_center->CastToBeacon()->AELocationSpell(this, spell_id, resist_adjust);
			}
			else {
				if (ae_center && ae_center == this && IsBeneficialSpell(spell_id) && (!IsLuclinPortSpell(spell_id) || !IsUnTargetable()))
					SpellOnTarget(spell_id, this);

				bool affect_caster = !IsNPC();	//NPC AE spells do not affect the NPC caster
				entity_list.AESpell(this, ae_center, spell_id, affect_caster, resist_adjust, spell_target);
			}
			break;
		}

		case GroupSpell:
		{
			if (IsClient())
			{
				// the casting_spell_id check makes this only work for timed spells, and not autocast recourse spells like Mind Wrack Recourse
				if (casting_spell_id != 0 && spell_id == casting_spell_id && (casting_aa != 0 || slot != CastingSlot::Item) && IsMGBCompatibleSpell(spell_id) && IsClient() && CastToClient()->CheckAAEffect(aaEffectMassGroupBuff))
				{
					entity_list.MassGroupBuff(this, this, spell_id);
					CastToClient()->DisableAAEffect(aaEffectMassGroupBuff);
				}
				else
				{
					// at this point spell_target is a member of the other group, or the
					// caster if they're not using TGB
					// NOTE: this will always hit the caster, plus the target's group so
					// it can affect up to 7 people if the targeted group is not our own
					if (spell_target->IsGrouped())
					{
						Group *target_group = entity_list.GetGroupByMob(spell_target);
						if (target_group)
						{
							target_group->CastGroupSpell(this, spell_id, isrecourse, recourse_level);
						}
					}
					else if (spell_target->IsRaidGrouped() && spell_target->IsClient())
					{
						Raid *target_raid = entity_list.GetRaidByClient(spell_target->CastToClient());
						uint32 gid = 0xFFFFFFFF;
						if (target_raid) {
							gid = target_raid->GetGroup(spell_target->GetName());
							if (gid >= 0 && gid < MAX_RAID_GROUPS)
								target_raid->CastGroupSpell(this, spell_id, gid, isrecourse, recourse_level);
							else
								SpellOnTarget(spell_id, spell_target, false, false, 0, isproc, 0, true, recourse_level);
						}
					}
					else
					{
						// if target is grouped, CastGroupSpell will cast it on the caster
						// too, but if not then we have to do that here.

						if (spell_target != this) {
							SpellOnTarget(spell_id, this, false, false, 0, isproc, 0, isrecourse, recourse_level);
#ifdef GROUP_BUFF_PETS
							//pet too
							if (GetPet() && HasPetAffinity() && !GetPet()->IsCharmedPet())
								SpellOnTarget(spell_id, GetPet(), false, false, 0, isproc, 0, isrecourse, recourse_level);
#endif
						}

						SpellOnTarget(spell_id, spell_target, false, false, 0, isproc, 0, isrecourse, recourse_level);
#ifdef GROUP_BUFF_PETS
						//pet too
						if (spell_target->GetPet() && HasPetAffinity() && !spell_target->GetPet()->IsCharmedPet())
							SpellOnTarget(spell_id, spell_target->GetPet(), false, false, 0, isproc, 0, isrecourse, recourse_level);
#endif
					}
				}
			}
			else
			{
				if (spells[spell_id].targettype == ST_GroupTeleport || spells[spell_id].targettype == ST_Group)
				{
					// group spell cast by NPC has a special case in AESpell
					// Balance of the Nameless, Cazic's Gift, recourse spells
					entity_list.AESpell(this, this, spell_id, true, resist_adjust, spell_target);
				}
				else
				{
					// self only
					SpellOnTarget(spell_id, this, false, false, 0, isproc, 0, isrecourse, recourse_level);
				}
			}
			break;
		}
	}
	// the OP_Action takes care of animating - client does not animate bards unless they have an instrument mod in effect, even if they're casting a clicky not a song
	//if (!spells[spell_id].bardsong && !IsDisc(spell_id) && !IsLuclinPortSpell(spell_id))
	//	DoAnim(static_cast<Animation>(spells[spell_id].CastingAnim), 0, false, IsClient() ? FilterPCSpells : FilterNPCSpells);

	// if this was a spell slot or an ability use up the mana for it
	// CastSpell already reduced the cost for it if we're a client with focus
	if(slot != CastingSlot::Item && (mana_used > 0 || isrecourse))
	{
		if (IsClient())
		{
			std::string item_name;
			int16 focus_val = CastToClient()->GetFocusEffect(focusManaCost, spell_id, item_name);
			if (focus_val != 0)
			{
				float PercentManaReduction = focus_val;
				int16 saved_mana = mana_used * (PercentManaReduction / 100);
				if (saved_mana == 0)
					saved_mana = 1;
				Log(Logs::General, Logs::Focus, "focusManaCost saved %d mana of %d. new cost is %d", saved_mana, mana_used, mana_used-saved_mana);
				mana_used -= saved_mana;
			}
		}

		Log(Logs::Detail, Logs::Spells, "Spell %d: consuming %d mana", spell_id, mana_used);
		SetMana(GetMana() - mana_used);
	}

	if (IsClient() && slot < CastingSlot::MaxGems)
	{
		Client *c = CastToClient();
		if (GetClass() == BARD && IsBardSong(spell_id))
		{
			c->CheckSongSkillIncrease(spell_id);
		}
		else
		{
			c->CheckIncreaseSkill(spells[spell_id].skill, nullptr, zone->skill_difficulty[spells[spell_id].skill].difficulty);
			c->CheckSpecializeIncrease(spell_id);
		}
	}

	//set our reuse timer on long ass reuse_time spells...
	if(IsClient() && slot != CastingSlot::Item)
	{
		if(spell_id == casting_spell_id && casting_spell_timer != 0xFFFFFFFF)
		{
			CastToClient()->GetPTimers().Start(casting_spell_timer, casting_spell_timer_duration);
			Log(Logs::Detail, Logs::Spells, "Spell %d: Setting custom reuse timer %d to %d", spell_id, casting_spell_timer, casting_spell_timer_duration);
		}
		else if(spells[spell_id].recast_time > 1000) {
			int recast = spells[spell_id].recast_time/1000;
			if (spell_id == SPELL_LAY_ON_HANDS)	//lay on hands
			{
				recast -= GetAA(aaFervrentBlessing) * 720;
			}
			else if (spell_id == SPELL_HARM_TOUCH || spell_id == SPELL_HARM_TOUCH2)	//harm touch
			{
				recast -= GetAA(aaTouchoftheWicked) * 720;
				CastToClient()->ExpendAATimer(aaImprovedHarmTouch);
			}
			else
			{
				recast += GetActSpellCasttime(spell_id, spells[spell_id].cast_time) / 1000 - 1;
				if (
					GetLevel() > 50 && spells[spell_id].goodEffect == 0 && spells[spell_id].cast_time > 2999 &&
					(GetClass() == BEASTLORD || GetClass() == PALADIN || GetClass() == RANGER || GetClass() == SHADOWKNIGHT)
				)
					recast -= 1;
			}

			uint16 timer_id = spell_id;
			if(spell_id == SPELL_HARM_TOUCH2)
				timer_id = SPELL_HARM_TOUCH;

			Log(Logs::Detail, Logs::Spells, "Spell %d: Setting long reuse timer to %d s (orig %d)", spell_id, recast, spells[spell_id].recast_time);
			CastToClient()->GetPTimers().Start(pTimerSpellStart + timer_id, recast);
			CastToClient()->GetPTimers().Store(&database);
		}
	}

	// Mod rods should have a 300 second recast delay, but the item and spells are all set to 0 in the database. Hardcode it here.
	if (IsClient() && slot == CastingSlot::Item && spell_id == SPELL_MODULATION)
	{
		CastToClient()->m_pp.LastModulated = static_cast<uint32>(time(nullptr));
		uint32 recast = 300;
		Log(Logs::Detail, Logs::Spells, "Spell %d: Setting long reuse timer to %d s (orig %d)", spell_id, recast, spells[spell_id].recast_time);
		CastToClient()->GetPTimers().Start(pTimerModulation, recast);
		CastToClient()->GetPTimers().Store(&database);
	}

	if(IsNPC())
		CastToNPC()->AI_Event_SpellCastFinished(true, static_cast<uint16>(slot));

	// Reset attack timer on casts for most classes.  Logs confirm this does apply to NPCs as well.
	// Note that the exceptions for hybrid classes (except bard) were added in the January 9, 2001 patch. (BSTs in Luclin, obv)
	if (slot >= CastingSlot::Gem1 && slot < CastingSlot::MaxGems && GetClass() != BARD && GetClass() != RANGER
		&& GetClass() != SHADOWKNIGHT && GetClass() != PALADIN && GetClass() != BEASTLORD)
	{
		GetAttackTimer().Reset();
	}

	return true;
}

/*
 * The first pulse goes through the normal CastedSpellFinished flow, repeat pulses go through here.
 * return false to stop the song
 */
bool Mob::ApplyNextBardPulse(uint16 spell_id, Mob *spell_target, CastingSlot slot)
{
	if (IsClient() && CastToClient()->GetBoatNPCID() > 0)
	{
		Message_StringID(Chat::SpellFailure, TOO_DISTRACTED);
		return(false);
	}

	return SpellFinished(spell_id, spell_target, slot, spells[spell_id].mana, -1, spells[spell_id].ResistDiff);
}

///////////////////////////////////////////////////////////////////////////////
// buff related functions

// returns how many _ticks_ the buff will last.
// a tick is 6 seconds
// both the caster and target mobs are passed in, so different behavior can
// even be created depending on the types of mobs involved
//
// right now this is just an outline, working on this..
int Mob::CalcBuffDuration(Mob *caster, Mob *target, uint16 spell_id, int32 caster_level_override)
{
	int formula, duration;

	if(!IsValidSpell(spell_id) || (!caster && !target))
		return 0;

	if(!caster && !target)
		return 0;

	// if we have at least one, we can make do, we'll just pretend they're the same
	if(!caster)
		caster = target;
	if(!target)
		target = caster;

	formula = spells[spell_id].buffdurationformula;
	duration = spells[spell_id].buffduration;

	int castlevel = caster->GetCasterLevel(spell_id);
	if(caster_level_override > 0)
		castlevel = caster_level_override;

	int res = CalcBuffDuration_formula(castlevel, formula, duration);

	if (caster && caster->IsClient() && IsBeneficialSpell(spell_id) && formula != DF_Permanent)
	{
		int aa_bonus = 0;
		uint8 spell_reinforcement = caster->GetAA(aaSpellCastingReinforcement);
		if (spell_reinforcement > 0)
		{
			switch (spell_reinforcement)
			{
			case 1:
				aa_bonus = 5;
				break;
			case 2:
				aa_bonus = 15;
				break;
			case 3:
				aa_bonus = 30;
				break;
			default:
				aa_bonus = 0;
				break;
				Log(Logs::General, Logs::AA, "Spell Casting Reinforcement added %d% to the duration of buff %d", aa_bonus, spell_id);

			}
			if (caster->GetAA(aaSpellCastingReinforcementMastery))
			{
				aa_bonus += 20;
				Log(Logs::General, Logs::AA, "Spell Casting Reinforcement Mastery added a total %d to the duration of buff %d%", aa_bonus, spell_id);
			}
			int bonus = res * aa_bonus / 100;

			res += bonus;
		}
	}

	if (caster == target && (target->aabonuses.IllusionPersistence || target->spellbonuses.IllusionPersistence ||
				 target->itembonuses.IllusionPersistence) &&
	    IsEffectInSpell(spell_id, SE_Illusion))
		res = 10000; // ~16h override

	Log(Logs::Detail, Logs::Spells, "Spell %d: Casting level %d, formula %d, base_duration %d: result %d",
		spell_id, castlevel, formula, duration, res);

	return(res);
}

// the generic formula calculations
int CalcBuffDuration_formula(int level, int formula, int duration)
{
	int i;	// temp variable

	if (formula >= 200)
	{
		return formula;
	}

	switch(formula)
	{
		case 0:	// not a buff
			return 0;

		case 1:
			i = level / 2;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 2:
			i = level <= 1 ? 6 : level / 2 + 5;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 3:
			i = level * 30;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 4:
			i = 50;
			return duration ? i < duration ? i : duration : i;

		case 5:
			i = 2;
			return duration ? i < duration ? i : duration : i;

		case 6:
			i = level / 2 + 2;
			return duration ? i < duration ? i : duration : i;

		case 7:
			i = level;
			return duration ? i < duration ? i : duration : i;

		case 8:
			i = level + 10;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 9:
			i = level * 2 + 10;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 10:
			i = level * 3 + 10;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 11:
			i = level * 30 + 90;
			return i < duration ? (i < 1 ? 1 : i) : duration;

		case 12:	// not used by any spells
			i = level / 4;
			i = i ? i : 1;
			return duration ? i < duration ? i : duration : i;

		case 50:	// permanent buff
			return 0xFFFE;

		default:
			Log(Logs::General, Logs::Spells, "CalcBuffDuration_formula: unknown formula %d", formula);
			return 0;
	}
}

// Check Spell Level Restrictions
// returns true if they meet the restrictions, false otherwise
// derived from http://samanna.net/eq.general/buffs.shtml
// spells 1-50: no restrictons
// 51-65: SpellLevel/2+15
bool Mob::CheckSpellLevelRestriction(uint16 spell_id, Mob* caster, CastingSlot slot)
{
	return true;
}

bool Client::CheckSpellLevelRestriction(uint16 spell_id, Mob* caster, CastingSlot slot)
{
	if (IsBardSong(spell_id))
		return true;

	if (IsGroupSpell(spell_id) && slot == CastingSlot::Item)
		return true;

	int SpellLevel = GetMinLevel(spell_id);
	bool client_is_targeted = false;

	if(caster->GetTarget() && caster->GetTarget() == this)
		client_is_targeted = true;

	// Only check for beneficial buffs
	if (IsBuffSpell(spell_id) && IsBeneficialSpell(spell_id)) 
	{
		if(SpellLevel <= 50 || GetLevel() >= SpellLevel || !client_is_targeted)
		{
			return true;
		}
		else if (SpellLevel > 50)
		{
			if (GetLevel() < (SpellLevel / 2 + 15))
			{
				return false;
			}
		}
	}

	return true;
}

// used by some MobAI stuff
// NOT USED BY SPELL CODE
// note that this should not be used for determining which slot to place a
// buff into
// returns -1 on stack failure, -2 if all slots full, the slot number if the buff should overwrite another buff, or a free buff slot
int Mob::CanBuffStack(uint16 spellid, uint8 caster_level, bool iFailIfOverwrite)
{
	Log(Logs::Detail, Logs::AI, "Checking if buff %d cast at level %d can stack on me.%s", spellid, caster_level, iFailIfOverwrite ? " failing if we would overwrite something" : "");

	if (FindBuff(spellid))
		return(-1);	//do not recast a buff we already have on, we recast fast enough that we dont need to refresh our buffs

	// TODO - it would be better to pass the caster object into this function instead of the caster_level.
	// FindAffectSlot behaves differently if NPC caster instead of client caster but we only have the target here.
	int slot = -1;
	FindAffectSlot(this, spellid, &slot, 0);

	if (slot == -1)
	{
		Log(Logs::Detail, Logs::AI, "Buff %d would conflict, reporting stack failure", spellid);
		return -1;	// stop the spell, can't stack it
	}
	if (buffs[slot].spellid != SPELL_UNKNOWN)
	{
		// should overwrite current slot
		if (iFailIfOverwrite) {
			Log(Logs::Detail, Logs::AI, "Buff %d would overwrite, reporting stack failure", spellid);
			return(-1);
		}
	}

	Log(Logs::Detail, Logs::AI, "Reporting that buff %d could successfully be placed into slot %d", spellid, slot);

	return slot;
}

bool Mob::CancelMagicIsAllowedOnTarget(Mob* spelltar)
{
	if (this == spelltar)
		return true;

	if (IsClient() && spelltar->IsClient() && !IsAttackAllowed(spelltar))
	{
		return InSameGroup(spelltar);
	}
	return true;
}

bool Mob::CancelMagicShouldAggro(uint16 spell_id, Mob* spelltar)
{
	if (IsEffectInSpell(spell_id, SE_CancelMagic))
	{
		if (this == spelltar)
			return false;

		if (IsClient() && spelltar->IsPet() && !IsAttackAllowed(spelltar->GetOwner(), true, spell_id))
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// spell effect related functions
//
// this is actually applying a spell cast from 'this' on 'spelltar'
// it performs pvp checking and applies resists, etc then it
// passes it to SpellEffect which causes effects to the target
//
// this is called by these functions:
// Mob::SpellFinished
// Entity::AESpell (called by Mob::SpellFinished)
// Group::CastGroupSpell (called by Mob::SpellFinished)
//
// also note you can't interrupt the spell here. at this point it's going
// and if you don't want effects just return false. interrupting here will
// break stuff
//
bool Mob::SpellOnTarget(uint16 spell_id, Mob* spelltar, bool reflect, bool use_resist_adjust, int16 resist_adjust, bool isproc, uint16 ae_caster_id, bool isrecourse, int recourse_spell_level)
{
	// well we can't cast a spell on target without a target
	if(!spelltar) {
		Log(Logs::Detail, Logs::Spells, "Unable to apply spell %d without a target", spell_id);
		Message(Chat::Red, "SOT: You must have a target for this spell.");
		return false;
	}

	// don't allow these mana-tap-over-time spells to apply to things without mana.  this is also checked in DoCastSpell but checking again here stops procs too (Drakkel Wolf Claws)
	if(spelltar && spelltar->GetCasterClass() == 'N' &&
		(IsEffectInSpell(spell_id, SE_CurrentMana) && IsValidSpell(spells[spell_id].RecourseLink)) /* Wandering Mind, Mind Wrack, Scryer's Trespass */)
	{
		Message_StringID(Chat::SpellFailure, TARGET_NO_MANA);
		return false;
	}

	//We have to check for Gate failure before its cast, because the client resolves on its own.
	if(IsGateSpell(spell_id)) {
		if (spellbonuses.AntiGate) {
			InterruptSpell(spell_id);
			return false;
		}
		else if (spelltar->IsClient()) {
			if (spelltar->CastToClient()->GetBindZoneID() != zone->GetZoneID()) {
				CastToClient()->zone_mode = GateToBindPoint;
			}
			else {
				CastToClient()->GoToBind();
				return false;
			}
		}
	}

	EQApplicationPacket *action_packet = nullptr, *message_packet = nullptr;
	float spell_effectiveness;

	if (!IsValidSpell(spell_id)) {
		return false;
	}

	// reversed tap spell
	bool is_tap_recourse = (spells[spell_id].targettype == ST_TargetAETap || spells[spell_id].targettype == ST_Tap) && spelltar == this;

	uint16 caster_level = GetCasterLevel(spell_id);
	uint8 item_level = GetClickLevel(this, spell_id);
	uint8 action_level = item_level > 0 ? item_level : caster_level; // client doesn't use this field unless 1252 <= spell_id <= 1266 (potions)

	Log(Logs::Detail, Logs::Spells, "Casting spell %d on %s with effective caster level %d", spell_id, spelltar->GetName(), caster_level);

	// Actual cast action - this causes the caster animation and the particles
	// around the target
	// we do this first, that way we get the particles even if the spell
	// doesn't land due to pvp protection
	// note: this packet is sent again if the spell is successful, with a flag
	// set
	action_packet = new EQApplicationPacket(OP_Action, sizeof(Action_Struct));
	Action_Struct* action = (Action_Struct*) action_packet->pBuffer;

	// select source
	if(IsClient() && CastToClient()->GMHideMe()) {
		action->source = spelltar->GetID();
	}
	else if (ae_caster_id > 0) {
		action->source = ae_caster_id;
		Log(Logs::General, Logs::Spells, "Rain spell is using entityid %d as caster.", ae_caster_id);
	}
	else {
		action->source = GetID();
	}

	// select target
	if	// Bind Sight line of spells
	(IsBindSightSpell(spell_id)) {
		action->target = GetID();
	}
	else {
		action->target = spelltar->GetID();
	}

	action->level = action_level;	// effective level, used for potions
	action->type = 231;	// 231 means a spell
	action->spell = spell_id;
	action->sequence = (GetHeading() * 2.0f);	// heading
	action->instrument_mod = GetInstrumentMod(spell_id);
	action->buff_unknown = 0;

	if(!IsDisc(spell_id)) {
		Log(Logs::Detail, Logs::Spells, "Sending Action packet #1 for spell %d", spell_id);

		if (spelltar != this && spelltar->IsClient()) {	// send to target
			spelltar->CastToClient()->QueuePacket(action_packet);
		}

		if (IsClient()) {	// send to caster
			CastToClient()->QueuePacket(action_packet);
		}

		// send to people in the area, ignoring caster and target
		entity_list.QueueCloseClients(spelltar, action_packet, true, RuleI(Range, SpellAnims), this, false, FilterNone);
	}
	else {
		Message(Chat::Disciplines, "%s", spells[spell_id].cast_on_you);
	}

	/* Send the EVENT_CAST_ON event */
	std::string export_string = fmt::format("{}", spell_id);
	if(spelltar->IsNPC()) {
		parse->EventNPC(EVENT_CAST_ON, spelltar->CastToNPC(), this, export_string, 0);
	}
	else if (spelltar->IsClient()) {
		parse->EventPlayer(EVENT_CAST_ON, spelltar->CastToClient(), export_string, 0);
	}

	// Casting on an entity in a different region silently fails after letting the spell be cast
	if ((IsClient() || IsPet()) && (!CheckRegion(spelltar) && !IsBindSightSpell(spell_id) && !IsSummonPCSpell(spell_id))) {
		if (IsClient() && spelltar->IsClient()) 	{
			spelltar->Message_StringID(Chat::SpellFailure, YOU_ARE_PROTECTED, GetName());
		}
		safe_delete(action_packet);
		return false;
	}

	if (IsEffectInSpell(spell_id, SE_Levitate)) {
		if (spelltar->IsClient() && spelltar->CastToClient()->Trader) {
			Log(Logs::General, Logs::Spells, "Levitate cannot be cast on a trader.");
			Message_StringID(Chat::SpellFailure, SPELL_NO_HOLD);
			safe_delete(action_packet);
			return true; // We want the spell to finish casting.
		}
	}
	if (IsCharmSpell(spell_id)) {
		if ((IsClient() && (spelltar->IsClient() || (spelltar->GetOwner() && spelltar->GetOwner()->IsClient()) || spelltar->IsCharmedPet())) || spelltar->IsCorpse()) {
			if (spelltar->IsClient()) {
				spelltar->Message_StringID(Chat::SpellFailure, YOU_ARE_PROTECTED, GetName());
				Message_StringID(Chat::SpellFailure, SPELL_NO_HOLD);
			}
			else {
				Message_StringID(Chat::SpellFailure, CANNOT_CHARM);
			}
			safe_delete(action_packet);
			return false;
		}

		if (IsClient() && GetPet() != nullptr) {
			Message_StringID(Chat::SpellFailure, ONLY_ONE_PET);
			safe_delete(action_packet);
			return false;
		}
	}
	if (IsEffectInSpell(spell_id, SE_CallPet)) {
		Mob *pet = spelltar->GetPet();
		if (IsClient() && pet) {
			if (entity_list.Fighting(pet)) {
				Message_StringID(Chat::SpellFailure, SUSPEND_MINION_HAS_AGGRO);
				safe_delete(action_packet);
				return false;
			}
		}
	}
	if (IsHarmonySpell(spell_id))  {
		spelltar->PacifyImmune = false;
		for (int i = 0; i < EFFECT_COUNT; i++) {
			// not important to check limit on SE_Lull as it doesn't have one and if the other components won't land, then SE_Lull wont either
			if (spells[spell_id].effectid[i] == SE_ChangeFrenzyRad || spells[spell_id].effectid[i] == SE_Harmony) {
				// Harmony was nerfed in the October 21, 2002 patch,
				// "Reduced the max NPC level that Harmony can effect." 
				// to L40; this disables this nerf until PoP
				bool is_spell_target_out_of_range = spelltar->GetLevel() > spells[spell_id].max[i];
				if (!content_service.IsThePlanesOfPowerEnabled() && spell_id == 250) {
					is_spell_target_out_of_range = false;
				}
				if ((IsClient() && spelltar->IsNPC() && spells[spell_id].max[i] != 0 && is_spell_target_out_of_range) ||
					spelltar->GetSpecialAbility(IMMUNE_PACIFY)) 
				{
					spelltar->PacifyImmune = true;
					Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT);
				}
			}
		}

		// prevent dirty exploiters from pulling mobs between floors with lull in VT
		if (GetZoneID() == Zones::VEXTHAL && (spelltar->GetHP() > 600000 || (std::abs(spelltar->GetPosition().z - m_Position.z) > 50.0f))) {
			safe_delete(action_packet);
			return false;
		}
	}

	bool targetIsGM = false;
	if (spelltar->IsClient())
		targetIsGM = spelltar->CastToClient()->GetGMInvul();

	// invuln: cazic touch penetrates DA for non-GMs; allow beneficial if GM mode only
	if (spelltar->GetInvul() || spelltar->DivineAura()) {
		bool spellHit = false;
		if (spells[spell_id].effectid[0] == SE_Teleport2) { // Banishment of the Pantheon, Dimensional Rift, Dimensional Return, Insanity of Tylis, Visions of Argan, Torment's Beckon
			spellHit = true;
		}
		else if (IsDisc(spell_id)) {
			spellHit = true;
		}
		else if (IsDetrimentalSpell(spell_id)) {
			if (spell_id == SPELL_CAZIC_TOUCH && !targetIsGM)
				spellHit = true;
		}
		else if (targetIsGM) {
			spellHit = true;		// beneficial on GMs only
		}

		if (!spellHit) {
			Log(Logs::Detail, Logs::Spells, "Casting spell %d on %s aborted: they are invulnerable.", spell_id, spelltar->GetName());
			if (targetIsGM)
				spelltar->Message_StringID(Chat::SpellFailure, YOU_ARE_PROTECTED, GetName());
			safe_delete(action_packet);
			return false;
		}
	}

	//cannot hurt untargetable mobs
	if(spelltar->IsUnTargetable()) {
		if (RuleB(Pets, UnTargetableSwarmPet)) {
			if (spelltar->IsNPC()) {
				if (!spelltar->CastToNPC()->GetSwarmOwner()) {
					Log(Logs::Detail, Logs::Spells, "Casting spell %d on %s aborted: they are untargetable", spell_id, spelltar->GetName());
					safe_delete(action_packet);
					return(false);
				}
			} else {
				Log(Logs::Detail, Logs::Spells, "Casting spell %d on %s aborted: they are untargetable", spell_id, spelltar->GetName());
				safe_delete(action_packet);
				return(false);
			}
		} else {
			Log(Logs::Detail, Logs::Spells, "Casting spell %d on %s aborted: they are untargetable", spell_id, spelltar->GetName());
			safe_delete(action_packet);
			return(false);
		}
	}

	if(!(IsClient() && CastToClient()->GetGM()) && !IsHarmonySpell(spell_id)) {	// GMs can cast on anything
		// Beneficial spells check
		if(IsBeneficialSpell(spell_id)) {
			if(IsClient() &&	//let NPCs do beneficial spells on anybody if they want, should be the job of the AI, not the spell code to prevent this from going wrong
				spelltar != this)
			{

				Client* pClient = nullptr;
				Raid* pRaid = nullptr;
				Group* pBasicGroup = nullptr;
				uint32 nGroup = 0; //raid group

				Client* pClientTarget = nullptr;
				Raid* pRaidTarget = nullptr;
				Group* pBasicGroupTarget = nullptr;
				uint32 nGroupTarget = 0; //raid group

				Client* pClientTargetPet = nullptr;
				Raid* pRaidTargetPet = nullptr;
				Group* pBasicGroupTargetPet = nullptr;
				uint32 nGroupTargetPet = 0; //raid group

				//Caster client pointers
				pClient = this->CastToClient();
				pRaid = entity_list.GetRaidByClient(pClient);
				pBasicGroup = entity_list.GetGroupByMob(this);
				if(pRaid)
					nGroup = pRaid->GetGroup(pClient) + 1;

				//Target client pointers
				if(spelltar->IsClient()) {
					pClientTarget = spelltar->CastToClient();
					pRaidTarget = entity_list.GetRaidByClient(pClientTarget);
					pBasicGroupTarget = entity_list.GetGroupByMob(spelltar);
					if(pRaidTarget)
						nGroupTarget = pRaidTarget->GetGroup(pClientTarget) + 1;
				}

				if(spelltar->IsPet()) {
					Mob *owner = spelltar->GetOwner();
					if(owner->IsClient()) {
						pClientTargetPet = owner->CastToClient();
						pRaidTargetPet = entity_list.GetRaidByClient(pClientTargetPet);
						pBasicGroupTargetPet = entity_list.GetGroupByMob(owner);
						if(pRaidTargetPet)
							nGroupTargetPet = pRaidTargetPet->GetGroup(pClientTargetPet) + 1;
					}

				}

				if(!IsBeneficialAllowed(spelltar) && !IsWhitelistedBeneficialSpellForSelfFound(spell_id, pClient, pClientTarget) ||
					(IsGroupOnlySpell(spell_id) &&
						!(
							(pBasicGroup && ((pBasicGroup == pBasicGroupTarget) || (pBasicGroup == pBasicGroupTargetPet))) || //Basic Group

							((nGroup > 0) && ((nGroup == nGroupTarget) || (nGroup == nGroupTargetPet))) || //Raid group
							(pRaid && ((pRaid == pRaidTarget) || (pRaid == pRaidTargetPet)) && !IsBuffSpell(spell_id)) || //Raid. Buffs don't sync client-side

							(spelltar == GetPet()) //should be able to cast grp spells on self and pet despite grped status.
						)
					)
				)
				{
					if(!IsNeutralSpell(spell_id)) {
						if(spells[spell_id].targettype == ST_AEBard) {
							//if it was a beneficial AE bard song don't spam the window that it would not hold
							Log(Logs::General, Logs::Spells, "Beneficial ae bard song %d can't take hold %s -> %s, IBA? %d", spell_id, GetName(), spelltar->GetName(), IsBeneficialAllowed(spelltar));
						} else {
							Log(Logs::General, Logs::Spells, "Beneficial spell %d can't take hold %s -> %s, IBA? %d", spell_id, GetName(), spelltar->GetName(), IsBeneficialAllowed(spelltar));
							Message_StringID(Chat::SpellFailure, SPELL_NO_HOLD);
						}
						safe_delete(action_packet);
						return true; // We want the spell to finish casting.
					}
				}
			}
		}
		else if (spells[spell_id].goodEffect != 0 && IsEffectInSpell(spell_id, SE_CancelMagic)){
			if (!CancelMagicIsAllowedOnTarget(spelltar)) {
				spelltar->Message_StringID(Chat::SpellFailure, YOU_ARE_PROTECTED, GetCleanName());
				safe_delete(action_packet);
				return false;
			}
		}
		else if	( !IsAttackAllowed(spelltar, true, spell_id) && !IsResurrectionEffects(spell_id) && spell_id != 721) { // Detrimental spells - PVP check. This also is used for Bard AE detrimental songs.
			Log(Logs::Detail, Logs::Spells, "Detrimental spell %d can't take hold %s -> %s", spell_id, GetName(), spelltar->GetName());
			spelltar->Message_StringID(Chat::SpellFailure, YOU_ARE_PROTECTED, GetCleanName());
			if((GetClass() == BARD && IsBardSong(spell_id)) == false) // no spam for bard songs, including single target ones
				Message_StringID(Chat::SpellFailure, SPELL_NO_HOLD);
			safe_delete(action_packet);
			return false;
		}
	}

	// ok at this point the spell is permitted to affect the target,
	// but we need to check special cases and resists

	// check immunities
	if(spelltar->IsImmuneToSpell(spell_id, this, isproc)) {
		//the above call does the message to the client if needed
		Log(Logs::Detail, Logs::Spells, "Spell %d can't take hold due to immunity %s -> %s", spell_id, GetName(), spelltar->GetName());
		safe_delete(action_packet);
		return false;
	}

	//check for AE_Undead
	if(spells[spell_id].targettype == ST_UndeadAE){
		if(spelltar->GetBodyType() != BT_SummonedUndead &&
			spelltar->GetBodyType() != BT_Undead &&
			spelltar->GetBodyType() != BT_Vampire)
		{
			safe_delete(action_packet);
			return false;
		}
	}
	// Reflect
	if(spelltar && spelltar->TryReflectSpell(spell_id) && !reflect && IsDetrimentalSpell(spell_id) && this != spelltar) {
		int reflect_chance = 0;
		switch(RuleI(Spells, ReflectType))
		{
			case 0:
				break;

			case 1:
			{
				if(spells[spell_id].targettype == ST_Target) {
					for(int y = 0; y < PLAYER_CLASS_COUNT; y++) {
						if(spells[spell_id].classes[y] < 255)
							reflect_chance = 1;
					}
				}
				break;
			}
			case 2:
			{
				for(int y = 0; y < PLAYER_CLASS_COUNT; y++) {
					if(spells[spell_id].classes[y] < 255)
						reflect_chance = 1;
				}
				break;
			}
			case 3:
			{
				if(spells[spell_id].targettype == ST_Target)
					reflect_chance = 1;

				break;
			}
			case 4:
				reflect_chance = 1;

			default:
				break;
		}
		if(reflect_chance) {
			SpellOnTarget(spell_id, this, true, use_resist_adjust, resist_adjust);
			safe_delete(action_packet);
			return false;
		}
	}

	Log(Logs::Detail, Logs::Spells, "Checking Resists for spell %d", spell_id);
	if(!is_tap_recourse && IsResistableSpell(spell_id)) {
		spell_effectiveness = spelltar->CheckResistSpell(spells[spell_id].resisttype, spell_id, this, spelltar, use_resist_adjust, resist_adjust);

		if(spell_effectiveness < 100) {
			if(spell_effectiveness == 0 || !IsPartialCapableSpell(spell_id) ) {
				Log(Logs::Detail, Logs::Spells, "Spell %d was completely resisted by %s", spell_id, spelltar->GetName());
				spelltar->ResistSpell(this, spell_id, isproc);

				safe_delete(action_packet);
				return false;
			}
		}
	}
	else
	{
		spell_effectiveness = 100;
	}

	if (spelltar->spellbonuses.SpellDamageShield && IsDetrimentalSpell(spell_id) && !is_tap_recourse) {
		spelltar->DamageShield(this, true);
	}
	else if (IsDetrimentalSpell(spell_id) && !is_tap_recourse && !IsHarmonySpell(spell_id) && !IsAllianceSpellLine(spell_id) &&
		     CancelMagicShouldAggro(spell_id, spelltar))
	{
		Log(Logs::Detail, Logs::Spells, "Applying aggro for spell %d", spell_id);
		// Damage aggro is handled in CommonDamage()
		if (!HasDirectDamageEffect(spell_id)) {
			spelltar->AggroPet(this);
		}

		if (spelltar->IsAIControlled()) {
			int32 aggro_amount = CheckAggroAmount(spell_id, spelltar);
			int32 current_hate = spelltar->GetHateAmount(this, false);

			Log(Logs::Detail, Logs::Spells, "Spell %d cast on %s generated %d hate current hate is: %d", spell_id, spelltar->GetName(), aggro_amount, current_hate);
			if (current_hate == 0) {
				if (!spelltar->IsEngaged() && IsDirectDamageSpell(spell_id)) {
					// don't aggro pet if NPC is not aggro and is damage spell; Damage routine will aggro pet if needed
					// have to do this so we can poof pet if spell one-shots it since we call AddToHateList() twice on damage casts
					spelltar->AddToHateList(this, aggro_amount, 0, false, false, false);
				}
				else {
					spelltar->AddToHateList(this, aggro_amount);
				}
			}
			else {
				spelltar->AddHate(this, aggro_amount);
			}

			if (spelltar->IsNPC()) {
				spelltar->CastToNPC()->CallForHelp(this);	// this will fail if timer is ticking down, so won't spam
			}
		}
	}
	else if ((IsBeneficialSpell(spell_id) || is_tap_recourse) && !IsSummonPCSpell(spell_id) && !IsAEMemBlurSpell(spell_id) && !IsBindSightSpell(spell_id)
		&& (!spelltar->IsPet() || spelltar->IsCharmedPet())											// no beneficial aggro for summoned pets
		&& (!IsNPC() || !isproc || CastToNPC()->GetInnateProcSpellId() != spell_id)					// NPC innate procs always hit the target, even if beneficial. we don't want beneficial procs aggroing nearby NPCs
		&& (!spelltar->IsCharmedPet() || (spelltar->IsCharmedPet() && !IsHealingSpell(spell_id)))	// Healing spells on charmed pets don't cause aggro.
		)
	{
		Log(Logs::Detail, Logs::Spells, "Applying beneficial aggro for spell %d", spell_id);
		entity_list.AddHealAggro(spelltar, this, CheckHealAggroAmount(spell_id, spelltar, (spelltar->GetMaxHP() - spelltar->GetHP()), isproc));
	}

	// Figure out a slot for the buff.
	Mob *buff_target = IsBindSightSpell(spell_id) ? this : spelltar;
	int buffslot = -1, clevel = action_level;
	if (IsBuffSpell(spell_id) && !buff_target->AssignBuffSlot(this, spell_id, buffslot, clevel, action_packet))
	{
		// if AssignBuffSlot returned false there's a problem applying the spell. It's most likely a buff that can't stack.
		Log(Logs::General, Logs::Spells, "Spell %d could not be assigned a slot %s -> %s\n", spell_id, GetName(), spelltar->GetName());
		if ((GetClass() == BARD && IsBardSong(spell_id)) == false) // no spam for bard songs, including single target ones
			Message_StringID(Chat::SpellFailure, SPELL_NO_HOLD);
		safe_delete(action_packet);
		CalcBonuses();
		return true; // We want the spell to finish casting.
	}

	// send the action packet again now that the spell is successful
	// NOTE: this is what causes the buff icon to appear on the client, if
	// this is a buff - but it sortof relies on the first packet.
	// the complete sequence is 2 actions and 1 damage message
	action->buff_unknown = 0x04;	// this is a success flag

	//SE_BindAffinity is sent in effect handles. Discs don't send spell packets. 
	//Buff stacking logic already sent this packet if this is a buff.
	if (!IsEffectInSpell(spell_id, SE_BindAffinity) && !IsDisc(spell_id)  && !IsBuffSpell(spell_id))
	{
		Log(Logs::Detail, Logs::Spells, "Sending Action packet #2 for spell %d", spell_id);
		if (spelltar->IsClient())	// send to target
			spelltar->CastToClient()->QueuePacket(action_packet);

		if (spelltar != this && IsClient())	// send to caster
			CastToClient()->QueuePacket(action_packet);
	}

	// We send this packet in Mob::GenerateDamagePackets for damage spells
	//Buff stacking logic already sent this packet if this is a buff.
	if(!IsDisc(spell_id) && !IsEffectInSpell(spell_id, SE_BindAffinity) && !IsBuffSpell(spell_id) &&
		!HasDirectDamageEffect(spell_id))	// spells with direct damage effect handled in CommonDamage()
	{
		// This is the message for the spell.
		message_packet = new EQApplicationPacket(OP_Damage, sizeof(Damage_Struct));
		Damage_Struct *cd = (Damage_Struct *)message_packet->pBuffer;
		cd->target = action->target;
		cd->source = action->source;
		cd->type = action->type;
		cd->spellid = action->spell;
		cd->sequence = action->sequence;
		cd->damage = 0;

		Log(Logs::Detail, Logs::Spells, "SpellOnTarget: SpellMessage target: %i, source: %i, type: %i, spellid: %i, sequence: %f, damage: %i BardSong: %i", cd->target, cd->source, cd->type, cd->spellid, cd->sequence, cd->damage, IsBardSong(spell_id));
		Log(Logs::Detail, Logs::Spells, "Sending Message packet for spell %d", spell_id);

		// send to target unfiltered.
		if (spelltar->IsClient())
		{
			spelltar->CastToClient()->QueuePacket(message_packet);
		}
		// send to caster unfiltered.
		if (spelltar != this && IsClient())
		{
			CastToClient()->QueuePacket(message_packet);
		}
		// send to people in the area, ignoring caster and target
		if (!IsBardSong(spell_id))
		{
			entity_list.QueueCloseClients(spelltar, message_packet, true, RuleI(Range, SpellMessages), this, true, spelltar->IsClient() ? FilterPCSpells : FilterNPCSpells);
		}
		else
		{
			entity_list.QueueCloseClients(spelltar, message_packet, true, RuleI(Range, SongMessages), this, true, FilterBardSongs);
		}

		safe_delete(message_packet);
	}

	safe_delete(action_packet);

	if ((spells[spell_id].pushback != 0 || spells[spell_id].pushup != 0))
	{
		Log(Logs::Detail, Logs::Spells, "Spell: %d has a pushback (%0.1f) or pushup (%0.1f) component.", spell_id, spells[spell_id].pushback, spells[spell_id].pushup);

		if (spells[spell_id].pushup > 0) {
			spelltar->DoKnockback(this, spells[spell_id].pushback, spells[spell_id].pushup, spelltar->IsNPC());
		}
		else if (spelltar->IsNPC() && spells[spell_id].pushback != 0) {
			spelltar->CastToNPC()->AddPush(this->GetHeading() * 2.0f, spells[spell_id].pushback);
			spelltar->CastToNPC()->TriggerPushTimer();
		}
	}

	if (spelltar->IsClient() && spelltar->CastToClient()->IsFeigned() &&
		IsDetrimentalSpell(spell_id) && !IsNeutralSpell(spell_id) && (!IsDispelSpell(spell_id) || (IsDispelSpell(spell_id) && IsNPC())))
	{
		spelltar->CastToClient()->SetFeigned(false);
		spelltar->Message_StringID(Chat::SpellFailure, FEIGN_BROKEN_SPELL);
	}

	if (spelltar->IsClient() && IsEffectInSpell(spell_id, SE_ShadowStep))
	{
		spelltar->CastToClient()->SetShadowStepExemption(true);
	}

	// Handle spells that don't have effects.
	if (IsClient())
	{
		// Flesh To Bone
		if (spell_id == SPELL_TRANSMUTE_FLESH_TO_BONE)
		{
			if (!CastToClient()->FleshToBone())
			{
				return false;
			}
		}
	}

	// Apply the spell effects and bonuses.
	if (!spelltar->SpellEffect(this, spell_id, buffslot, clevel, spell_effectiveness))
	{
		// if SpellEffect() returned false there's a problem applying the effects (invalid spell.)
		Log(Logs::General, Logs::Spells, "Spell %d could not apply its effects %s -> %s\n", spell_id, GetName(), spelltar->GetName());
	}

	if (IsClient() && IsBuffSpell(spell_id))
	{
		int caster_spell_level = spells[spell_id].classes[GetClass() - 1];
		CastToClient()->ApplyDurationFocus(spell_id, buffslot, spelltar, isrecourse ? recourse_spell_level : caster_spell_level);
	}

	Log(Logs::Detail, Logs::Spells, "Cast of %d by %s on %s complete successfully.", spell_id, GetName(), spelltar->GetName());

	if (IsDetrimentalSpell(spell_id) && !is_tap_recourse)
		spelltar->CommonBreakSneakHide();

	// autocast recourse spells
	if (!isrecourse)
	{
		// normal recourse spell
		if (IsValidSpell(spells[spell_id].RecourseLink))
		{
			int recourse_spell_id = spells[spell_id].RecourseLink;
			if (spell_id == SPELL_GREENMIST)
			{
				// It seems that the recourse simply didn't work on AK.  The reason is unknown (to me).

				if (RuleB(AlKabor, GreenmistHack))
				{
					// 
					// We have the target cast the recourse so Greenmist Recourse doesn't become a reversed tap from being self cast. 
					// This is a TAKP hack for Greenmist specifically.  The recourse spell data seems to have a messed up target type, it should be regular single target.
					// If we cast this like a normal recourse, it will become reversed because of the target type and the caster being our self.
					// The workaround is to have the target cast it on us, which mostly works, but the spell is cast at the level of the target and
					// can't be overwritten to be refreshed by procing on a lower level target after.  Doing this also makes the target do the casting
					// animation of the recourse instead of the player procing it.
					//

					for (int i = 0; i < GetMaxBuffSlots(); i++)
					{
						// the spell won't overwrite to refresh duration when it procs on a lower level target since we're faking it out as the target casting it instead of the player.
						// here we fade the existing recourse buff so we can stick a new full duration one on from the lower level target.
						if (buffs[i].spellid == recourse_spell_id && buffs[i].casterlevel > spelltar->GetLevel())
						{
							BuffFadeBySlot(i, true, true, true); // generates worn off message "The green mist disperses." when the Greenmist Recourse is refreshed this way.
							break;
						}
					}
					spelltar->SpellFinished(recourse_spell_id, this, CastingSlot::Item, 0, -1, spells[recourse_spell_id].ResistDiff, false, true, GetLevel());
				}
			}
			else
			{
				SpellFinished(recourse_spell_id, this, CastingSlot::Item, 0, -1, spells[recourse_spell_id].ResistDiff, false, true, GetLevel());
			}
		}
		// tap buff spells use themselves as the recourse, but with the effects reversed
		// TODO: investigate doing this for all ST_Tap spells not just buffs - cast the spell on the original caster for each affected target with the effects reversed
		else if (buffslot != -1 && spelltar != this && (spells[spell_id].targettype == ST_TargetAETap || spells[spell_id].targettype == ST_Tap))
		{
			SpellFinished(spell_id, this, CastingSlot::Item, 0, -1, spells[spell_id].ResistDiff, false, true, GetLevel());
		}
	}

	return true;
}

void Corpse::CastRezz(uint16 spellid, Mob* Caster)
{
	Log(Logs::Detail, Logs::Spells, "Corpse::CastRezz spellid %i, Rezzed() is %i, rezzexp is %i rez_timer enabled: %i", spellid,IsRezzed(),rez_experience, corpse_rez_timer.Enabled());

	// refresh rezzed state from database
	uint32 db_exp, db_gmexp;
	bool db_rezzable, db_is_rezzed;
	if (!database.LoadCharacterCorpseRezData(corpse_db_id, &db_exp, &db_gmexp, &db_rezzable, &db_is_rezzed))
	{
		// db error, corpse disappeared?
		Caster->Message_StringID(Chat::White, REZZ_ALREADY_PENDING);
		return;
	}
	rezzable = db_rezzable;
	IsRezzed(db_is_rezzed);
	rez_experience = db_exp;
	gm_rez_experience = db_gmexp;

	// Rez timer has expired, only GMs can rez at this point. (uses rezzable)
	if(!IsRezzable())
	{
		if(Caster && Caster->IsClient() && !Caster->CastToClient()->GetGM())
		{
			Caster->Message_StringID(Chat::White, REZZ_ALREADY_PENDING);
			Caster->Message_StringID(Chat::White, CORPSE_TOO_OLD);
			return;
		}
	}

	// Corpse has been rezzed, but timer is still active. Players can corpse gate, GMs can rez for XP. (uses is_rezzed)
	if(IsRezzed())
	{
		if(Caster && Caster->IsClient())
		{
			if(Caster->CastToClient()->GetGM())
			{
				rez_experience = gm_rez_experience;
				gm_rez_experience = 0;
			}
			else
				rez_experience = 0;
		}
	}

	auto outapp = new EQApplicationPacket(OP_RezzRequest, sizeof(Resurrect_Struct));
	Resurrect_Struct* rezz = (Resurrect_Struct*) outapp->pBuffer;
	// Why are we truncating these names to 30 characters ?
	memcpy(rezz->your_name,this->corpse_name,30);
	memcpy(rezz->corpse_name,this->name,30);
	memcpy(rezz->rezzer_name,Caster->GetName(),30);
	rezz->zone_id = zone->GetZoneID();
	rezz->spellid = spellid;
	rezz->x = this->m_Position.x;
	rezz->y = this->m_Position.y;
	rezz->z = this->m_Position.z;
	rezz->unknown000 = 0x00000000;
	rezz->unknown020 = 0x00000000;
	rezz->unknown088 = 0x00000000;
	// We send this to world, because it needs to go to the player who may not be in this zone.
	worldserver.RezzPlayer(outapp, rez_experience, corpse_db_id, char_id, OP_RezzRequest);
	safe_delete(outapp);
}

bool Mob::FindBuff(uint16 spellid)
{
	int i;

	uint32 buff_count = GetMaxTotalSlots();
	for(i = 0; i < buff_count; i++)
		if(buffs[i].spellid == spellid)
			return true;

	return false;
}

// removes all buffs
void Mob::BuffFadeAll(bool skiprez, bool message)
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if (buffs[j].spellid != SPELL_UNKNOWN)
		{
			if (!skiprez || (skiprez && !IsResurrectionEffects(buffs[j].spellid)))
				BuffFadeBySlot(j, false, message);
		}
	}

	//we tell BuffFadeBySlot not to recalc, so we can do it only once when were done
	CalcBonuses();
}

void Mob::BuffFadeNonPersistDeath()
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if (buffs[j].spellid != SPELL_UNKNOWN)
			BuffFadeBySlot(j, false, false);
	}
	//we tell BuffFadeBySlot not to recalc, so we can do it only once when were done
	CalcBonuses();
}

void Mob::BuffFadeDetrimental() {
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if(buffs[j].spellid != SPELL_UNKNOWN) {
			if(IsDetrimentalSpell(buffs[j].spellid))
				BuffFadeBySlot(j, false);
		}
	}
}

void Mob::BuffFadeDetrimentalByNPCs(int16 slot_skipped) {
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if (buffs[j].spellid != SPELL_UNKNOWN && j != slot_skipped) {
			if (!buffs[j].client && IsDetrimentalSpell(buffs[j].spellid))
			{
				BuffFadeBySlot(j, false);
			}
		}
	}
}

void Mob::BuffFadeDotsByCaster(Mob *caster) {
	if (!caster)
		return;

	int buff_count = GetMaxTotalSlots();

	for (int j = 0; j < buff_count; j++) {
		if (buffs[j].spellid != SPELL_UNKNOWN) {

			if (caster->GetID() == buffs[j].casterid && IsDetrimentalSpell(buffs[j].spellid) && IsDOTSpell(buffs[j].spellid))
			{
				BuffFadeBySlot(j, false);
			}
		}
	}
}

void Mob::BuffFadeDetrimentalByCaster(Mob *caster)
{
	if(!caster)
		return;

	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if(buffs[j].spellid != SPELL_UNKNOWN) {
			if(caster->GetID() == buffs[j].casterid && IsDetrimentalSpell(buffs[j].spellid))
			{
				BuffFadeBySlot(j, false);
			}
		}
	}
}

void Mob::BuffFadeBySitModifier()
{
	bool r_bonus = false;
	uint32 buff_count = GetMaxTotalSlots();
	for(uint32 j = 0; j < buff_count; ++j)
	{
		if(buffs[j].spellid != SPELL_UNKNOWN)
		{
			if(spells[buffs[j].spellid].disallow_sit)
			{
				BuffFadeBySlot(j, false);
				r_bonus = true;
			}
		}
	}

	if(r_bonus)
	{
		CalcBonuses();
	}
}

// removes the buff matching spell_id
void Mob::BuffFadeBySpellID(uint16 spell_id, bool message)
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++)
	{
		if (buffs[j].spellid == spell_id)
			BuffFadeBySlot(j, false, message);
	}

	//we tell BuffFadeBySlot not to recalc, so we can do it only once when were done
	CalcBonuses();
}

// removes buffs containing effectid, skipping skipslot
void Mob::BuffFadeByEffect(int effectid, int skipslot)
{
	int i;

	int buff_count = GetMaxTotalSlots();
	for(i = 0; i < buff_count; i++)
	{
		if(buffs[i].spellid == SPELL_UNKNOWN)
			continue;
		if(IsEffectInSpell(buffs[i].spellid, effectid) && i != skipslot)
			BuffFadeBySlot(i, false);
	}

	//we tell BuffFadeBySlot not to recalc, so we can do it only once when were done
	CalcBonuses();
}

// checks if 'this' can be affected by spell_id from caster
// returns true if the spell should fail, false otherwise
bool Mob::IsImmuneToSpell(uint16 spell_id, Mob *caster, bool isProc)
{
	int effect_index;

	if(caster == nullptr)
		return(false);

	if (casting_gm_override == 1)
		return(false);

	//TODO: this function loops through the effect list for
	//this spell like 10 times, this could easily be consolidated
	//into one loop through with a switch statement.

	Log(Logs::Detail, Logs::Spells, "Checking to see if we are immune to spell %d cast by %s", spell_id, caster->GetName());

	if(!IsValidSpell(spell_id))
		return true;

	if (spell_id == SPELL_RESURRECTION_EFFECTS)
	{
		return false;
	}

	if(IsBeneficialSpell(spell_id) && (caster->GetNPCTypeID())) //then skip the rest, stop NPCs aggroing each other with buff spells. 2013-03-05
		return false;

	if (IsDetrimentalSpell(spell_id) && !zone->CanDoCombat(caster, this)) {
		caster->Message_StringID(Chat::SpellFailure, SPELL_WOULDNT_HOLD);
		return true;
	}

	if(IsMezSpell(spell_id))
	{
		if(GetSpecialAbility(UNMEZABLE)) {
			Log(Logs::Detail, Logs::Spells, "We are immune to Mez spells.");
			caster->Message_StringID(Chat::SpellFailure, CANNOT_MEZ);
			ResistSpell(caster, spell_id, isProc);
			return true;
		}

		// check max level for spell
		effect_index = GetSpellEffectIndex(spell_id, SE_Mez);
		assert(effect_index >= 0);
		// NPCs get to ignore the max level
		if(GetLevel() > spells[spell_id].max[effect_index] && caster->IsClient() && IsNPC())
		{
			Log(Logs::Detail, Logs::Spells, "Our level (%d) is higher than the limit of this Mez spell (%d)", GetLevel(), spells[spell_id].max[effect_index]);
			caster->Message_StringID(Chat::SpellFailure, CANNOT_MEZ_WITH_SPELL);
			ResistSpell(caster, spell_id, isProc);
			return true;
		}
	}

	// slow and haste spells
	if((GetSpecialAbility(UNSLOWABLE) && !GetSpecialAbility(REVERSE_SLOW) && IsSlowSpell(spell_id)) ||
		(GetSpecialAbility(NO_HASTE) && IsHasteSpell(spell_id)))
	{
		Log(Logs::Detail, Logs::Spells, "We are immune to changes to attack speed spells.");
		caster->Message_StringID(Chat::SpellFailure, IMMUNE_ATKSPEED);
		ResistSpell(caster, spell_id, isProc);
		return true;
	}

	if(IsEffectInSpell(spell_id, SE_Fear))
	{
		effect_index = GetSpellEffectIndex(spell_id, SE_Fear);

		// level 52 hardcap immunity for fear spells. Unlike Mez, Charm, Pacify, and Stun this should be enforced on NPC vs NPC action
		if (GetSpecialAbility(UNFEARABLE) || (IsNPC() && GetLevel() > 52))
		{
			Log(Logs::Detail, Logs::Spells, "We are immune to Fear spells.");
			caster->Message_StringID(Chat::SpellFailure, IMMUNE_FEAR);
			ResistSpell(caster, spell_id, isProc);
			return true;

		}
		// client vs client fear
		else if(IsClient() && caster->IsClient() && (caster->CastToClient()->GetGM() == false))
		{
			Log(Logs::Detail, Logs::Spells, "Clients cannot fear each other!");
			caster->Message_StringID(Chat::SpellFailure, IMMUNE_FEAR);
			return true;
		}
		else if(GetLevel() > spells[spell_id].max[effect_index] && spells[spell_id].max[effect_index] != 0)
		{
			Log(Logs::Detail, Logs::Spells, "Level is %d, cannot be feared by this spell.", GetLevel());
			ResistSpell(caster, spell_id, isProc);
			return true;
		}

		else if (IsClient() && CastToClient()->CheckAAEffect(aaEffectWarcry))
		{
			Message(Chat::Red, "Your are immune to fear.");
			Log(Logs::Detail, Logs::Spells, "Clients has WarCry effect, immune to fear!");
			caster->Message_StringID(Chat::SpellFailure, IMMUNE_FEAR);
			return true;
		}
	}

	if(IsCharmSpell(spell_id))
	{
		if (GetClass() == MERCHANT || GetClass() == BANKER || GetClass() >= WARRIORGM && GetClass() <= BEASTLORDGM)
		{
			Log(Logs::Detail, Logs::Spells, "We are immune to Charm spells. (Banker, Merchant, Guildmaster)");
			caster->Message_StringID(Chat::SpellFailure, CANNOT_CHARM);
			ResistSpell(caster, spell_id, isProc);
			return true;
		}

		if(GetSpecialAbility(UNCHARMABLE))
		{
			Log(Logs::Detail, Logs::Spells, "We are immune to Charm spells.");
			caster->Message_StringID(Chat::SpellFailure, CANNOT_CHARM);
			ResistSpell(caster, spell_id, isProc);
			return true;
		}

		if(this == caster)
		{
			Log(Logs::Detail, Logs::Spells, "You are immune to your own charms.");
			caster->Message(Chat::SpellFailure, "You cannot charm yourself.");
			return true;
		}

		//let npcs cast whatever charm on anyone
		if(caster->IsClient())
		{
			// check level limit of charm spell
			effect_index = GetSpellEffectIndex(spell_id, SE_Charm);
			assert(effect_index >= 0);
			if(IsNPC() && GetLevel() > spells[spell_id].max[effect_index] && spells[spell_id].max[effect_index] != 0)
			{
				Log(Logs::Detail, Logs::Spells, "Our level (%d) is higher than the limit of this Charm spell (%d)", GetLevel(), spells[spell_id].max[effect_index]);
				caster->Message_StringID(Chat::SpellFailure, CANNOT_CHARM_YET);
				ResistSpell(caster, spell_id, isProc);
				return true;
			}
		}
	}

	if
	(
		IsEffectInSpell(spell_id, SE_Root) ||
		IsEffectInSpell(spell_id, SE_MovementSpeed)
	)
	{
		if(GetSpecialAbility(UNSNAREABLE)) {
			Log(Logs::Detail, Logs::Spells, "We are immune to Snare spells.");
			caster->Message_StringID(Chat::SpellFailure, IMMUNE_MOVEMENT);
			ResistSpell(caster, spell_id, isProc);
			return true;
		}
	}

	if(IsSacrificeSpell(spell_id))
	{
		if(this == caster)
		{
			Log(Logs::Detail, Logs::Spells, "You cannot sacrifice yourself.");
			caster->Message_StringID(Chat::SpellFailure, CANNOT_SAC_SELF);
			return true;
		}
	}

	Log(Logs::Detail, Logs::Spells, "No immunities to spell %d found.", spell_id);

	return false;
}

//
// Spell resists:
// returns an effectiveness index from 0 to 100. for most spells, 100 means
// it landed, and anything else means it was resisted; however there are some
// spells that can be partially effective, and this value can be used there.
float Mob::CheckResistSpell(uint8 resist_type, uint16 spell_id, Mob *caster, Mob *target, bool use_resist_override, int resist_override, bool tick_save)
{
	if (!caster) {
		return 100;
	}

	if (casting_gm_override == 1) {
		return 100;
	}

	if (spell_id != 0 && !IsValidSpell(spell_id)) {
		return 0;
	}

	if(GetSpecialAbility(IMMUNE_MAGIC)) {
		Log(Logs::Detail, Logs::Spells, "We are immune to magic, so we fully resist the spell %d", spell_id);
		return(0);
	}

	//Get resist modifier and adjust it based on focus 2 resist about eq to 1% resist chance
	int resist_modifier = (use_resist_override) ? resist_override : spells[spell_id].ResistDiff;

	//Check for fear resist
	bool IsFear = false;
	if(IsFearSpell(spell_id)) {
		IsFear = true;
		int fear_resist_bonuses = CalcFearResistChance();
		if(zone->random.Roll(fear_resist_bonuses)) {
			Log(Logs::Detail, Logs::Spells, "Resisted spell in fear resistance, had %d chance to resist", fear_resist_bonuses);
			return 0;
		}
	}

	if (!tick_save) {
		// Check for Chance to Resist Spell bonuses (ie Sanctification Discipline)
		int resist_bonuses = CalcResistChanceBonus();
		if(resist_bonuses && zone->random.Roll(resist_bonuses)) {
			Log(Logs::Detail, Logs::Spells, "Resisted spell in sanctification, had %d chance to resist", resist_bonuses);
			return 0;
		}
	}

	//Get the resist chance for the target
	if(resist_type == RESIST_NONE) {
		Log(Logs::Detail, Logs::Spells, "Spell was unresistable");
		return 100;
	}

	if (GetSpecialAbility(IMMUNE_CASTING_FROM_RANGE)) {
		if (!caster->CombatRange(this)) {
			return(0);
		}
	}

	// SummonedPet is actually the owner Mob
	Mob* SummonedPet = GetOwner() && !IsCharmedPet() && GetPetType() != petHatelist && caster->IsNPC() ? GetOwner() : nullptr;
	if (!content_service.IsTheShadowsOfLuclinEnabled()) {
		SummonedPet = nullptr;
	}
	int fire = GetFR();
	int cold = GetCR();
	int magic = GetMR();
	int disease = GetDR();
	int poison = GetPR();

	if (SummonedPet) {
		int petfire = FR + itembonuses.FR;
		int petcold = CR + itembonuses.CR;
		int petmagic = MR + itembonuses.MR;
		int petdisease = DR + itembonuses.DR;
		int petpoison = PR + itembonuses.PR;

		if (SummonedPet->IsClient()) {
			Client* owner = SummonedPet->CastToClient();
			fire = std::max(owner->CalcFR(false, false), petfire) + spellbonuses.FR;
			cold = std::max(owner->CalcCR(false, false), petcold) + spellbonuses.CR;
			magic = std::max(owner->CalcMR(false, false), petmagic) + spellbonuses.MR;
			disease = std::max(owner->CalcDR(false, false), petdisease) + spellbonuses.DR;
			poison = std::max(owner->CalcPR(false, false), petpoison) + spellbonuses.PR;
		}
		else {
			Mob* owner = SummonedPet;
			fire = std::max(owner->FR + owner->itembonuses.FR, petfire) + spellbonuses.FR;
			cold = std::max(owner->CR + owner->itembonuses.CR, petcold) + spellbonuses.CR;
			magic = std::max(owner->MR + owner->itembonuses.MR, petmagic) + spellbonuses.MR;
			disease = std::max(owner->DR + owner->itembonuses.DR, petdisease) + spellbonuses.DR;
			poison = std::max(owner->PR + owner->itembonuses.PR, petpoison) + spellbonuses.PR;
		}
	}

	int target_resist;
	switch(resist_type) {
	case RESIST_FIRE:
		target_resist = fire;
		break;
	case RESIST_COLD:
		target_resist = cold;
		break;
	case RESIST_MAGIC:
		target_resist = magic;
		break;
	case RESIST_DISEASE:
		target_resist = disease;
		break;
	case RESIST_POISON:
		target_resist = poison;
		break;
	default:
		target_resist = 0;
		break;
	}

	//Setup our base resist chance.
	int caster_level = caster->GetLevel();
	if (tick_save) {
		caster_level += 4;
	}
	int target_level = SummonedPet ? SummonedPet->GetLevel() : GetLevel();
	int resist_chance = 0;
	int level_mod = 0;

	if (SummonedPet) {
		Log(Logs::Detail, Logs::Spells, "CheckResistSpell(): Pet %s is using level: %d target_resist: %d (MR: %d FR: %d CR: %d DR: %d PR: %d) Against spell %d cast by %s", GetName(), target_level, target_resist, magic, fire, cold, disease, poison, spell_id, caster->GetName());
	}

	//Adjust our resist chance based on level modifiers
	int leveldiff = target_level - caster_level;
	int temp_level_diff = leveldiff;

	if (IsNPC() && target_level >= RuleI(Casting, ResistFalloff)) {
		int a = (RuleI(Casting, ResistFalloff) - 1) - caster_level;
		if (a > 0) {
			temp_level_diff = a;
		}
		else {
			temp_level_diff = 0;
		}
	}

	if(IsClient() && target_level >= 21 && temp_level_diff > 15) {
		temp_level_diff = 15;
	}

	if(IsNPC() && temp_level_diff < -9) {
		temp_level_diff = -9;
	}

	level_mod = temp_level_diff * temp_level_diff / 2;
	if(temp_level_diff < 0) {
		level_mod = -level_mod;
	}

	// set resist modifiers for targets well above caster's level
	// this is a crude approximation of Sony's convoluted function
	// it's far from precise but way better than nothing
	if (IsNPC()) {
		if (caster_level < 50) {
			// after a certain level above the caster, NPCs gain a significant resist bonus
			// how many levels above the caster and how large the bonus is both increase with caster level
			// It's not a flat 1000 resist mod as Sony's pseudocode stated.  Many parses were done to prove this
			int bump_level = caster_level + 4 + caster_level / 6;
			if (target_level >= bump_level) {
				level_mod += 70 + caster_level * 6;
			}
		}
		else {
			if (caster_level < 64) {
				if (leveldiff >= 13)
					level_mod = caster_level * 5;
			}
			else {
				// note: if you use this for expacs beyond PoP, you may need to expand this
				if (leveldiff >= 16)
					level_mod = caster_level * 5;
			}
		}
	}

	//Even more level stuff this time dealing with damage spells
	if (IsNPC() && IsDirectDamageSpell(spell_id)) {
		if (target_level >= RuleI(Casting, ResistFalloff)) {
			temp_level_diff = (RuleI(Casting, ResistFalloff) - 1) - caster_level;
			if (temp_level_diff < 0) {
				temp_level_diff = 0;
			}
		}
		else {
			temp_level_diff = target_level - caster_level;
		}

		if (temp_level_diff > 0 && target_level >= 17) {
			level_mod += (2 * temp_level_diff);
		}
	}
	
	if (!tick_save && caster->GetClass() == ENCHANTER) {
		// See http://www.eqemulator.org/forums/showthread.php?t=43370

		if (IsCharmSpell(spell_id) || IsMezSpell(spell_id)) {
			if (caster->GetCHA() > 75) {
				resist_modifier -= (caster->GetCHA() - 75) / 8;
			}

			Log(Logs::Detail, Logs::Spells, "CheckResistSpell(): Spell: %d  Charisma is modifying resist value. resist_modifier is: %i", spell_id, resist_modifier);
		}
	}

	// Lull spells DO NOT use regular resists on initial cast, instead they use a value of 15.  Prathun pseudocode and live parses confirm.  Late luclin era change
	if (content_service.IsThePlanesOfPowerEnabled()) {
		if (IsHarmonySpell(spell_id)) {
			target_resist = 15;
			Log(Logs::Detail, Logs::Spells, "CheckResistSpell(): Spell: %d  Lull spell is overriding MR. target_resist is: %i resist_modifier is: %i", spell_id, target_resist, resist_modifier);
		}
	}

	if (RuleB(Quarm, EnableSpellSixLevelRule))
	{
		//Prathun's post just says If target is an NPC and caster is far below target's level, set level modifier to 1000.
		if (caster)
		{
			if (IsNPC() && GetLevel() >= std::max(caster->GetLevel() + 7, (int)(caster->GetLevel() * 1.25f)))
			{
				level_mod = 1000;
			}
		}
	}

	bool use_classic_resists = RuleR(World, CurrentExpansion) < (float)ExpansionEras::LuclinEQEra + 0.79;

	//Add our level, resist and -spell resist modifier to our roll chance
	resist_chance += target_resist;
	resist_chance += level_mod;
	if (use_classic_resists && IsClient()) {
		if (resist_chance > 200 && spells[spell_id].targettype == ST_Tap) {
			resist_chance = 200;
		}

		if (caster->IsNPC()) {
			if (spell_id == 837) {
				// Stun Breath
				resist_modifier = -50;
			}

			if (spell_id == 843) {
				// Immolating Breath
				resist_modifier = -100;
			}

			if (resist_modifier == -150) {
				// dragon aoes
				resist_modifier = -100;
			}
		}

		int hardcap = 350;
		if (!content_service.IsTheScarsOfVeliousEnabled()) {
			hardcap = 250;
		}

		if (resist_chance > hardcap) {
			resist_chance = hardcap;
		}

		if (resist_chance > 200) {
			resist_chance = 200 + (resist_chance - 200) / 2;
		}
	}

	resist_chance += resist_modifier;
	
	if (use_classic_resists && caster->IsNPC() && caster->GetLevel() > 24 && zone->GetZoneID() != Zones::SIRENS
		&& (caster->GetClass() == ENCHANTER || caster->GetClass() == ENCHANTERGM))
	{
		if (GetLevel() < resist_chance) {
			resist_chance = GetLevel();
		}

		if (resist_chance > 80) {
			resist_chance = 80;
		}
	}

	if (tick_save) {
		// See http://www.eqemulator.org/forums/showthread.php?t=43370
		if (IsCharmSpell(spell_id)) {
			if (resist_chance < RuleI(Spells, CharmMinResist)) {	// this value is 5 for non-custom servers
				resist_chance = RuleI(Spells, CharmMinResist);
			}
		}
		else if (IsRootSpell(spell_id)) {
			if (resist_chance < RuleI(Spells, RootMinResist)) {		// this value is 5 for non-custom servers
				resist_chance = RuleI(Spells, RootMinResist);
			}
		}
		else {
			if (resist_chance < 5) {
				resist_chance = 5;
			}
		}
	}

	// NPCs use special rules for rain spells in our era.
	if (IsNPC() && IsRainSpell(spell_id)) {
		// 20% innate resist
		if (zone->random.Roll(20)) {
			return 0;
		}

		uint8 hp_percent = (uint8)((float)GetHP() / (float)GetMaxHP() * 100.0f);
		if(target_level > 20 && hp_percent < 10) {
			return 0;
		}
	}

	// PvP
	if (caster->IsClient() && target && target->IsClient()) {
		if (resist_chance > 1 && resist_chance < 200) {
			resist_chance = resist_chance * 400 / (200 + resist_chance);		// this changes the curve from linear to the bow shape seen in parses
		}
		if (resist_chance > 196) {
			resist_chance = 196;		// minimum 2% chance for spells to land
		}
	}

	//Finally our roll
	int roll = zone->random.Int(0, 200);
	Log(Logs::Detail, Logs::Spells, "CheckResistSpell(): Spell: %d roll %i > resist_chance %i", spell_id, roll, resist_chance);
	if(roll > resist_chance) {
		return 100;
	}
	else {
		if(!IsPartialCapableSpell(spell_id) || resist_chance == 0) {
			return 0;
		}
		else {
			if (use_classic_resists && resist_chance > 200 && IsClient()) {
				resist_chance = 200;
			}

			int partial_modifier = ((150 * (resist_chance - roll)) / resist_chance);

			if (IsNPC()) {
				if (target_level > caster_level && target_level >= 17 && (caster_level <= 50 || use_classic_resists)) {
					partial_modifier += 5;
				}

				if (target_level >= 30 && (caster_level <= 50 || use_classic_resists)) {
					partial_modifier += (caster_level - 25);
				}

				if(target_level < 15) {
					partial_modifier -= 5;
				}
			}

			if(caster->IsNPC()) {
				if((target_level - caster_level) >= 20) {
					partial_modifier += (int)((target_level - caster_level) * 1.5);
				}
			}
			
			if(partial_modifier <= 0) {
				return 100;
			}
			else if(partial_modifier >= 100) {
				return 0;
			}

			return (100.0f - partial_modifier);
		}
	}
}

int Mob::ResistPhysical(int level_diff, uint8 caster_level)
{
	/*	Physical resists use the standard level mod calculation in
	conjunction with a resist fall off formula that greatly prevents you
	from landing abilities on mobs that are higher level than you.
	After level 12, every 4 levels gained the max level you can hit
	your target without a sharp resist penalty is raised by 1.
	Extensive parsing confirms this, along with baseline phyiscal resist rates used.
	*/


	if (level_diff == 0)
		return level_diff;

	int level_mod = 0;

	if (level_diff > 0) {

		int ResistFallOff = 0;

		if (caster_level <= 12)
			ResistFallOff = 3;
		else
			ResistFallOff = caster_level/4;

		if (level_diff > ResistFallOff || level_diff >= 15)
			level_mod = ((level_diff * 10) + level_diff)*2;
		else
			level_mod = level_diff * level_diff / 2;
	}

	else
		level_mod = -(level_diff * level_diff / 2);

	return level_mod;
}

int16 Mob::CalcResistChanceBonus()
{
	int resistchance = spellbonuses.ResistSpellChance + itembonuses.ResistSpellChance;

	if(IsClient())
		resistchance += aabonuses.ResistSpellChance;

	return resistchance;
}

int16 Mob::CalcFearResistChance()
{
	int resistchance = spellbonuses.ResistFearChance + itembonuses.ResistFearChance;
	if(this->IsClient()) {
		resistchance += aabonuses.ResistFearChance;
		if(aabonuses.Fearless == true)
			resistchance = 100;
	}
	if(spellbonuses.Fearless == true || itembonuses.Fearless == true)
		resistchance = 100;

	return resistchance;
}

float Mob::GetAOERange(uint16 spell_id) {
	float range;

	range = spells[spell_id].aoerange;
	if(range == 0)	//for TGB spells, they prolly do not have an aoe range
		range = spells[spell_id].range;
	if(range == 0)
		range = 10;	//something....

	if (IsClient()) {

		if(IsBardSong(spell_id) && IsBeneficialSpell(spell_id) && (spells[spell_id].targettype == ST_Group || spells[spell_id].targettype == ST_GroupTeleport)) {
			//Live AA - Extended Notes, SionachiesCrescendo
			float song_bonus = static_cast<float>(aabonuses.SongRange + spellbonuses.SongRange + itembonuses.SongRange);
			range += range*song_bonus /100.0f;
		}

		range = CastToClient()->GetSpellRange(spell_id, range);
	}

	return(range);
}

///////////////////////////////////////////////////////////////////////////////
// 'other' functions

void Mob::Spin() {
	if(IsClient()) {
		auto outapp = new EQApplicationPacket(OP_Action, sizeof(Action_Struct));
		outapp->pBuffer[0] = 0x0B;
		outapp->pBuffer[1] = 0x0A;
		outapp->pBuffer[2] = 0x0B;
		outapp->pBuffer[3] = 0x0A;
		outapp->pBuffer[4] = 0xE7;
		outapp->pBuffer[5] = 0x00;
		outapp->pBuffer[6] = 0x4D;
		outapp->pBuffer[7] = 0x04;
		outapp->pBuffer[8] = 0x00;
		outapp->pBuffer[9] = 0x00;
		outapp->pBuffer[10] = 0x00;
		outapp->pBuffer[11] = 0x00;
		outapp->pBuffer[12] = 0x00;
		outapp->pBuffer[13] = 0x00;
		outapp->pBuffer[14] = 0x00;
		outapp->pBuffer[15] = 0x00;
		outapp->pBuffer[16] = 0x00;
		outapp->pBuffer[17] = 0x00;
		outapp->pBuffer[18] = 0xD4;
		outapp->pBuffer[19] = 0x43;
		outapp->pBuffer[20] = 0x00;
		outapp->pBuffer[21] = 0x00;
		outapp->pBuffer[22] = 0x00;
		outapp->priority = 5;
		CastToClient()->QueuePacket(outapp);
		safe_delete(outapp);
	}
	else {
		GMMove(GetX(), GetY(), GetZ(), GetHeading() + 5);
	}
}

// this puts the spell bar back into a usable state fast
void Mob::SendSpellBarEnable(uint16 spell_id)
{
	if(!IsClient())
		return;

	auto outapp = new EQApplicationPacket(OP_ManaChange, sizeof(ManaChange_Struct));
	ManaChange_Struct* manachange = (ManaChange_Struct*)outapp->pBuffer;
	manachange->new_mana = GetMana();
	manachange->spell_id = spell_id;
	outapp->priority = 6;
	CastToClient()->QueuePacket(outapp);
	safe_delete(outapp);
}

void Mob::Stun(int duration, Mob* attacker)
{
	if (stunned)
	{
		if (IsClient())
		{
			// clients don't chain stun.  Without this the client will unstun itself
			// even while the server thinks otherwise
			return;
		}
		else
		{
			if (stunned_timer.GetRemainingTime() > duration)
				return;
		}
	}

	if (IsNPC() && IsMezzed())
	{
		return;
	}

	uint16 spellid = casting_spell_id;
	if (bardsong > 0) 
	{
		spellid = bardsong;
	}

	if(IsValidSpell(spellid) && !spells[spellid].uninterruptable)
	{
		if (IsNPC() && spellid >= 859 && spellid <= 1023)
			return;

		InterruptSpell(spellid, true);
	}

	if(duration > 0)
	{
		stunned = true;
		stunned_timer.Start(duration);
		if (IsAIControlled())
		{
			StopNavigation();
		}
	}
}

void Mob::UnStun() {
	if(stunned && stunned_timer.Enabled()) {
		stunned = false;
		stunned_timer.Disable();
	}
}

// Stuns "this"
void Client::Stun(int duration, Mob* attacker)
{
	Mob::Stun(duration, attacker);

	auto outapp = new EQApplicationPacket(OP_Stun, sizeof(Stun_Struct));
	Stun_Struct* stunon = (Stun_Struct*) outapp->pBuffer;
	stunon->duration = duration;
	outapp->priority = 5;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::UnStun() {
	Mob::UnStun();

	auto outapp = new EQApplicationPacket(OP_Stun, sizeof(Stun_Struct));
	Stun_Struct* stunon = (Stun_Struct*) outapp->pBuffer;
	stunon->duration = 0;
	outapp->priority = 5;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void NPC::Stun(int duration, Mob* attacker) {
	Mob::Stun(duration, attacker);
}

void NPC::UnStun() {
	Mob::UnStun();
}

void Mob::Mesmerize()
{
	mezzed = true;

	if (casting_spell_id)
		InterruptSpell(casting_spell_id, true);

	StopNavigation();
	EndShield();

/* this stuns the client for max time, with no way to break it
	if (this->IsClient()){
		auto outapp = new EQApplicationPacket(OP_Stun, sizeof(Stun_Struct));
		Stun_Struct* stunon = (Stun_Struct*) outapp->pBuffer;
		stunon->duration = 0xFFFF;
		this->CastToClient()->QueuePacket(outapp);
		safe_delete(outapp);
	} else {
		SetRunAnimSpeed(0);
	}
*/
}

void Client::MakeBuffFadePacket(uint16 spell_id, int slot_id, bool send_message)
{
	auto outapp = new EQApplicationPacket(OP_Buff, sizeof(SpellBuffFade_Struct));
	SpellBuffFade_Struct* sbf = (SpellBuffFade_Struct*) outapp->pBuffer;

	// when using buffade = 1 only bufftype, spellid and bufffade need to be set in this packet
	//sbf->entityid=GetID();
	sbf->bufftype=buffs[slot_id].bufftype;
	//sbf->level=buffs[slot_id].casterlevel;
	//sbf->bard_modifier=buffs[slot_id].instrumentmod;
	//sbf->activated=spells[spell_id].Activated;
	sbf->spellid=spell_id;
	//sbf->duration = buffs[slot_id].ticsremaining;
	//sbf->counters = buffs[slot_id].counters;
	//sbf->slot_number=slot_id;
	sbf->bufffade = 1; // client matches by spell_id and bufftype to find the target buff and fades it as if clicked off.

	QueuePacket(outapp);
	safe_delete(outapp);
}

int Client::FindSpellMemSlotBySpellID(uint16 spellid) {
	for (int i = 0; i < MAX_PP_MEMSPELL; i++) {
		if (m_pp.mem_spells[i] == spellid)
			return i;
	}

	return -1;	//default
}

void Client::MemSpell(uint16 spell_id, int slot, bool update_client)
{
	if(slot >= MAX_PP_MEMSPELL || slot < 0)
		return;

	if(update_client)
	{
		if(m_pp.mem_spells[slot] != 0xFFFFFFFF)
			UnmemSpell(slot, update_client);
	}

	m_pp.mem_spells[slot] = spell_id;
	uint32 recast = spells[spell_id].recast_time/1000;
	CastToClient()->GetPTimers().Start(pTimerSpellStart + spell_id, recast); // This is for the benefit of spells like Spirit of Cheetah, so the player can't cast the newly memmed spell right after zoning.
	Log(Logs::Detail, Logs::Spells, "Spell %d memorized into slot %d", spell_id, slot);

	database.SaveCharacterMemorizedSpell(this->CharacterID(), m_pp.mem_spells[slot], slot);

	if(update_client)
	{
		MemorizeSpell(slot, spell_id, memSpellMemorize);
	}
}

void Client::UnmemSpell(int slot, bool update_client)
{
	if(slot > MAX_PP_MEMSPELL || slot < 0)
		return;

	Log(Logs::Detail, Logs::Spells, "Spell %d forgotten from slot %d", m_pp.mem_spells[slot], slot);
	m_pp.mem_spells[slot] = 0xFFFF;

	database.DeleteCharacterMemorizedSpell(this->CharacterID(), m_pp.mem_spells[slot], slot);

	if(update_client)
	{
		MemorizeSpell(slot, m_pp.mem_spells[slot], memSpellForget);
	}
}

void Client::UnmemSpellAll(bool update_client)
{
	int i;

	for(i = 0; i < MAX_PP_MEMSPELL; i++)
		if(m_pp.mem_spells[i] != 0xFFFFFFFF)
			UnmemSpell(i, update_client);
}

void Client::ScribeSpell(uint16 spell_id, int slot, bool update_client, bool defer_save)
{
	if (slot >= MAX_PP_SPELLBOOK || slot < 0) {
		return;
	}

	if(update_client) {
		if (m_pp.spell_book[slot] != 0xFFFFFFFF) {
			UnscribeSpell(slot, update_client, defer_save);
		}
	}

	m_pp.spell_book[slot] = spell_id;

	// defer save if we're bulk saving elsewhere
	if (!defer_save) {
		database.SaveCharacterSpell(this->CharacterID(), spell_id, slot);
	}
	Log(Logs::Detail, Logs::Spells, "Spell %d scribed into spell book slot %d", spell_id, slot);

	if(update_client) {
		MemorizeSpell(slot, spell_id, memSpellScribing);
	}
}

void Client::UnscribeSpell(int slot, bool update_client, bool defer_save)
{
	if (slot >= MAX_PP_SPELLBOOK || slot < 0) {
		return;
	}

	Log(Logs::Detail, Logs::Spells, "Spell %d erased from spell book slot %d", m_pp.spell_book[slot], slot);
	m_pp.spell_book[slot] = 0xFFFF;

	if (!defer_save) {
		database.DeleteCharacterSpell(this->CharacterID(), m_pp.spell_book[slot], slot);
	}

	if (update_client) {
		auto outapp = new EQApplicationPacket(OP_DeleteSpell, sizeof(DeleteSpell_Struct));
		DeleteSpell_Struct *del = (DeleteSpell_Struct*)outapp->pBuffer;
		del->spell_slot = slot;
		del->success = 1;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::UnscribeSpellAll(bool update_client)
{
	for(int i = 0; i < MAX_PP_SPELLBOOK; i++) {
		if (m_pp.spell_book[i] != 0xFFFFFFFF) {
			UnscribeSpell(i, update_client, true);
		}
	}

	// bulk save at end (this will only delete)
	SaveSpells();
}

int Client::GetNextAvailableSpellBookSlot(int starting_slot) {
	for (int i = starting_slot; i < MAX_PP_SPELLBOOK; i++) {	//using starting_slot should help speed this up when we're iterating through a bunch of spells
		if (!IsValidSpell(GetSpellByBookSlot(i)))
			return i;
	}

	return -1;	//default
}

int Client::FindSpellBookSlotBySpellID(uint16 spellid) {
	for(int i = 0; i < MAX_PP_SPELLBOOK; i++) {
		if(m_pp.spell_book[i] == spellid)
			return i;
	}

	return -1;	//default
}

bool Client::SpellGlobalCheck(uint16 spell_ID, uint32 char_ID) {

	std::string spell_Global_Name;
	int spell_Global_Value;
	int global_Value;

	std::string query = StringFormat("SELECT qglobal, value FROM spell_globals "
                                    "WHERE spellid = %i", spell_ID);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
		return false; // Query failed, so prevent spell from scribing just in case
    }

    if (results.RowCount() != 1)
        return true; // Spell ID isn't listed in the spells_global table, so it is not restricted from scribing

    auto row = results.begin();
    spell_Global_Name = row[0];
	spell_Global_Value = atoi(row[1]);

	if (spell_Global_Name.empty())
        return true; // If the entry in the spell_globals table has nothing set for the qglobal name

    query = StringFormat("SELECT value FROM quest_globals "
                        "WHERE charid = %i AND name = '%s'",
                        char_ID, spell_Global_Name.c_str());
    results = database.QueryDatabase(query);
    if (!results.Success()) {
        Log(Logs::General, Logs::Error, "Spell ID %i query of spell_globals with Name: '%s' Value: '%i' failed", spell_ID, spell_Global_Name.c_str(), spell_Global_Value);
        return false;
    }

    if (results.RowCount() != 1) {
        Log(Logs::General, Logs::Error, "Char ID: %i does not have the Qglobal Name: '%s' for Spell ID %i", char_ID, spell_Global_Name.c_str(), spell_ID);
        return false;
    }

    row = results.begin();

    global_Value = atoi(row[0]);

    if (global_Value == spell_Global_Value)
        return true; // If the values match from both tables, allow the spell to be scribed
    else if (global_Value > spell_Global_Value)
        return true; // Check if the qglobal value is greater than the require spellglobal value

    // If no matching result found in qglobals, don't scribe this spell
    Log(Logs::General, Logs::Error, "Char ID: %i Spell_globals Name: '%s' Value: '%i' did not match QGlobal Value: '%i' for Spell ID %i", char_ID, spell_Global_Name.c_str(), spell_Global_Value, global_Value, spell_ID);
    return false;
}

bool Client::SpellBucketCheck(uint16 spell_id, uint32 char_id) {
	std::string spell_bucket_name;
	int spell_bucket_value;
	int bucket_value;
	std::string query = StringFormat("SELECT key, value FROM spell_buckets WHERE spellid = %i", spell_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success())
		return false;

	if (results.RowCount() != 1)
		return true;

	auto row = results.begin();
	spell_bucket_name = row[0];
	spell_bucket_value = atoi(row[1]);
	if (spell_bucket_name.empty())
		return true;

	query = StringFormat("SELECT value FROM data_buckets WHERE key = '%i-%s'", char_id, spell_bucket_name.c_str());
	results = database.QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::General, Logs::Error, "Spell bucket %s for spell ID %i for char ID %i failed.", spell_bucket_name.c_str(), spell_id, char_id);
		return false;
	}

	if (results.RowCount() != 1) {
		Log(Logs::General, Logs::Error, "Spell bucket %s does not exist for spell ID %i for char ID %i.", spell_bucket_name.c_str(), spell_id, char_id);
		return false;
	}

	row = results.begin();

	bucket_value = atoi(row[0]);

	if (bucket_value == spell_bucket_value)
		return true; // If the values match from both tables, allow the spell to be scribed
	else if (bucket_value > spell_bucket_value)
		return true; // Check if the data bucket value is greater than the required spell bucket value

	// If no matching result found in spell buckets, don't scribe this spell
	Log(Logs::General, Logs::Error, "Spell bucket %s for spell ID %i for char ID %i did not match value %i.", spell_bucket_name.c_str(), spell_id, char_id, spell_bucket_value);
	return false;
}

int16 Mob::GetBuffSlotFromType(uint16 type) {
	uint32 buff_count = GetMaxTotalSlots();
	for (int i = 0; i < buff_count; i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			for (int j = 0; j < EFFECT_COUNT; j++) {
				if (spells[buffs[i].spellid].effectid[j] == type )
					return i;
			}
		}
	}
	return -1;
}

uint16 Mob::GetSpellIDFromSlot(uint8 slot)
{
	if (buffs[slot].spellid != SPELL_UNKNOWN)
		return buffs[slot].spellid;
	return 0;
}

bool Mob::FindType(uint16 type, bool bOffensive, uint16 threshold) {
	int buff_count = GetMaxTotalSlots();
	for (int i = 0; i < buff_count; i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {

			for (int j = 0; j < EFFECT_COUNT; j++) {
				// adjustments necessary for offensive npc casting behavior
				if (bOffensive) {
					if (spells[buffs[i].spellid].effectid[j] == type) {
						int16 value =
								CalcSpellEffectValue_formula(spells[buffs[i].spellid].buffdurationformula,
											spells[buffs[i].spellid].base[j],
											spells[buffs[i].spellid].max[j],
											buffs[i].casterlevel, buffs[i].spellid);
						Log(Logs::General, Logs::Normal, 
								"FindType: type = %d; value = %d; threshold = %d",
								type, value, threshold);
						if (value < threshold)
							return true;
					}
				} else {
					if (spells[buffs[i].spellid].effectid[j] == type )
						return true;
				}
			}
		}
	}
	return false;
}

bool Mob::IsCombatProc(uint16 spell_id) {

	if (RuleB(Spells, FocusCombatProcs))
		return false;

	if(spell_id == SPELL_UNKNOWN)
		return(false);

	if ((spells[spell_id].cast_time == 0) && (spells[spell_id].recast_time == 0) && (spells[spell_id].recovery_time == 0))
	{

		for (int i = 0; i < MAX_PROCS; i++){
			if (SpellProcs[i].spellID == spell_id)
			{
				return true;
			}
		}
	}

	return false;
}

bool Mob::AddProcToWeapon(uint16 spell_id, uint16 iChance, uint16 base_spell_id, bool poison) {
	if(spell_id == SPELL_UNKNOWN)
		return(false);

	for (int i = 0; i < MAX_PROCS; i++) {
		if (SpellProcs[i].spellID == spell_id) {
			Log(Logs::Detail, Logs::Spells, "Proc is already on weapon");
			return true;
		}
	}
	for (int i = 0; i < MAX_PROCS; i++) {
		if (SpellProcs[i].spellID == SPELL_UNKNOWN) {
			SpellProcs[i].spellID = spell_id;
			SpellProcs[i].chance = iChance;
			SpellProcs[i].base_spellID = base_spell_id;
			SpellProcs[i].poison = poison;
			Log(Logs::General, Logs::Spells, "Added spell-granted proc spell %d with chance %d to slot %d Posion: %d", spell_id, iChance, i, poison);
			return true;
		}
	}
	Log(Logs::Detail, Logs::Spells, "Too many procs for %s", GetName());
	return false;
}

bool Mob::RemoveProcFromWeapon(uint16 spell_id, bool bAll) 
{
	for (int i = 0; i < MAX_PROCS; i++)
	{
		if (SpellProcs[i].spellID != SPELL_UNKNOWN && (bAll || SpellProcs[i].spellID == spell_id)) 
		{
			uint16 deleted_spell = SpellProcs[i].spellID;
			SpellProcs[i].spellID = SPELL_UNKNOWN;
			SpellProcs[i].chance = 0;
			SpellProcs[i].base_spellID = SPELL_UNKNOWN;
			SpellProcs[i].poison = false;
			Log(Logs::General, Logs::Spells, "Removed proc %d from slot %d", deleted_spell, i);
		}
	}
	return true;
}

bool Mob::RemovePoisonFromWeapon(uint16 spell_id, bool bAll) 
{
	for (int i = 0; i < MAX_PROCS; i++)
	{
		if (SpellProcs[i].spellID != SPELL_UNKNOWN && SpellProcs[i].poison && (bAll || SpellProcs[i].spellID == spell_id))
		{
			uint16 deleted_spell = SpellProcs[i].spellID;
			SpellProcs[i].spellID = SPELL_UNKNOWN;
			SpellProcs[i].chance = 0;
			SpellProcs[i].base_spellID = SPELL_UNKNOWN;
			SpellProcs[i].poison = false;
			Log(Logs::General, Logs::Spells, "Removed poison %d from slot %d", deleted_spell, i);
		}
	}

	return true;
}

// this is checked in a few places to decide wether special bard
// behavior should be used.
bool Mob::UseBardSpellLogic(uint16 spell_id, int slot)
{
	if (spell_id == SPELL_UNKNOWN && casting_spell_id)
		spell_id = casting_spell_id;
	if (spell_id == SPELL_UNKNOWN && bardsong)
		spell_id = bardsong;

	if (slot == -1 && casting_spell_slot != CastingSlot::Invalid)
		slot = static_cast<int>(casting_spell_slot);
	if (slot == -1 && bardsong_slot != CastingSlot::Invalid)
		slot = static_cast<int>(bardsong_slot);

	// should we treat this as a bard singing?
	return
	(
		spell_id != 0 &&
		spell_id != SPELL_UNKNOWN &&
		slot != -1 &&
		GetClass() == BARD &&
		slot <= MAX_PP_MEMSPELL &&
		IsBardSong(spell_id)
	);
}

int Mob::GetCasterLevel(uint16 spell_id) {
	int level = GetLevel();
	if (!RuleB(Spells, JamFestAAOnlyAffectsBard))
	{
		// this bonus (effective_casting_level) is only used in the Jam Fest AA
		level += itembonuses.effective_casting_level + spellbonuses.effective_casting_level + aabonuses.effective_casting_level;
		Log(Logs::Detail, Logs::Spells, "Determined effective casting level %d+%d+%d=%d", GetLevel(), spellbonuses.effective_casting_level, itembonuses.effective_casting_level, level);
	}
	return(level);
}

//this method does NOT tell the client to stop singing the song.
//this is NOT the right way to stop a mob from singing, use InterruptSpell
//you should really know what your doing before you call this
void Mob::_StopSong()
{
	bardsong = 0;
	bardsong_target_id = 0;
	bardsong_slot = CastingSlot::Invalid;
	bardsong_timer.Disable();
}

//This member function sets the buff duration on the client
//however it does not work if sent quickly after an action packets, which is what one might perfer to do
//Thus I use this in the buff process to update the correct duration once after casting
//this allows AAs and focus effects that increase buff duration to work correctly, but could probably
//be used for other things as well
void Client::SendBuffDurationPacket(uint16 spell_id, int duration, int inlevel, int slot_id, int bardmodifier)
{
	auto outapp = new EQApplicationPacket(OP_Buff, sizeof(SpellBuffFade_Struct));
	SpellBuffFade_Struct* sbf = (SpellBuffFade_Struct*)outapp->pBuffer;

	sbf->entityid = GetID();
	
	// this is SpellBuff_Struct - client replaces the existing buff fully with the data sent here
	sbf->bufftype = buffs[slot_id].bufftype;
	sbf->level = inlevel > 0 ? inlevel : GetLevel();
	sbf->bard_modifier = bardmodifier;
	sbf->activated = spells[spell_id].Activated;
	sbf->spellid = spell_id;
	sbf->duration = duration;
	sbf->counters = buffs[slot_id].counters;
	sbf->slot_number = slot_id;
	sbf->unk14 = 0;
	// end SpellBuff_Struct

	sbf->bufffade = 0;
	FastQueuePacket(&outapp);
}

void Mob::BuffModifyDurationBySpellID(uint16 spell_id, int32 newDuration, bool update)
{
	int buff_count = GetMaxTotalSlots();
	for(int i = 0; i < buff_count; ++i)
	{
		if (buffs[i].spellid == spell_id)
		{
			buffs[i].ticsremaining = newDuration;
			if(IsClient())
			{
				if(update)
				{
					CastToClient()->SendBuffDurationPacket(buffs[i].spellid, buffs[i].ticsremaining, buffs[i].casterlevel, i, buffs[i].instrumentmod);
				}
				else
				{
					buffs[i].UpdateClient = false;
				}
			}
		}
	}
}

void Client::InitializeBuffSlots()
{
	int max_slots = GetMaxTotalSlots();
	buffs = new Buffs_Struct[max_slots];
	memset(buffs, 0, sizeof(Buffs_Struct) * max_slots);
	for(int x = 0; x < max_slots; ++x)
	{
		buffs[x].spellid = SPELL_UNKNOWN;
	}
	current_buff_count = 0;
}

void Client::UninitializeBuffSlots()
{
	safe_delete_array(buffs);
}

void NPC::InitializeBuffSlots()
{
	int max_slots = GetMaxTotalSlots();
	buffs = new Buffs_Struct[max_slots];
	memset(buffs, 0, sizeof(Buffs_Struct) * max_slots);
	for(int x = 0; x < max_slots; ++x)
	{
		buffs[x].spellid = SPELL_UNKNOWN;
	}
	current_buff_count = 0;
}

void NPC::UninitializeBuffSlots()
{
	safe_delete_array(buffs);
}

void Client::SendSpellAnim(uint16 targetid, uint16 spell_id)
{
	if (!targetid || !IsValidSpell(spell_id))
		return;

	EQApplicationPacket app(OP_Action, sizeof(Action_Struct));
	Action_Struct* a = (Action_Struct*)app.pBuffer;
	a->target = targetid;
	a->source = this->GetID();
	a->type = 231;
	a->spell = spell_id;
	a->sequence = 231;

	app.priority = 1;
	entity_list.QueueCloseClients(this, &app, false, RuleI(Range, SpellAnims), 0, false);
}

bool Mob::IsBuffed()
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if(buffs[j].spellid != SPELL_UNKNOWN)
		{
			if(IsBeneficialSpell(buffs[j].spellid))
				return true;
		}
	}
	
	return false;
}

bool Mob::IsDebuffed()
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if(buffs[j].spellid != SPELL_UNKNOWN)
		{
			if(IsDetrimentalSpell(buffs[j].spellid))
				return true;
		}
	}
	
	return false;
}

bool Mob::HasDoT()
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if (buffs[j].spellid != SPELL_UNKNOWN)
		{
			if (IsDOTSpell(buffs[j].spellid))
				return true;
		}
	}

	return false;
}

//This will return true for the Pacify, Harmony, and Lull line of spells.
bool Mob::IsPacified()
{
	int buff_count = GetMaxTotalSlots();
	for (int j = 0; j < buff_count; j++) {
		if(buffs[j].spellid != SPELL_UNKNOWN)
		{
			if(IsDetrimentalSpell(buffs[j].spellid) && IsCrowdControlSpell(buffs[j].spellid))
				return true;
		}
	}
	
	return false;
}

void Mob::ResistSpell(Mob* caster, uint16 spell_id, bool isProc)
{
	if (caster == nullptr)
		return;

	AggroPet(caster);

	CommonBreakInvisible();
	CommonBreakSneakHide(true);

	if (IsClient())
	{
		if (CastToClient()->IsFeigned() && !IsHarmonySpell(spell_id))
		{
			CastToClient()->SetFeigned(false);
			Message_StringID(Chat::SpellFailure, FEIGN_BROKEN_SPELL);
		}
	}

	// Dire Charm
	if (caster && caster->IsClient() && caster->casting_aa == aaDireCharm)
	{
		aaID activeaa = static_cast<aaID>(caster->casting_aa);
		caster->CastToClient()->ResetAATimer(activeaa, ABILITY_FAILED);
	}

	if(caster != this)
		caster->Message_StringID(Chat::SpellFailure, TARGET_RESISTED, spells[spell_id].name);
	Message_StringID(Chat::SpellFailure, YOU_RESIST, spells[spell_id].name);

	if (IsAIControlled())
	{
		int32 aggro = caster->CheckAggroAmount(spell_id, this);
		if (aggro == 0)
			aggro = 1;

		if (!IsHarmonySpell(spell_id))
		{
			AddToHateList(caster, aggro);
		}
		else
		{
			// Calculate Lull critical fail aggro chance
			// See http://www.eqemulator.org/forums/showthread.php?t=43370
			int aggroChance = 90 - caster->GetCHA() / 4;

			if (!zone->random.Roll(aggroChance))
			{
				AddToHateList(caster, aggro);
				Log(Logs::Detail, Logs::Spells, "ResistSpell(): Lull critical fail; aggro chance was %i.", aggroChance);
			}
			else
				return;
		}

		Log(Logs::Detail, Logs::Spells, "ResistSpell(): %s has generated %d hate against %s due to spell %d being resisted.", GetName(), aggro, caster->GetName(), spell_id);
	}
}

void Mob::TrySpinStunBreak()
{
	if (IsClient())
		return;

	if (!IsStunned())
		return;

	if (!FindType(SE_SpinTarget))
		return;

	uint8 chance = 5; // RoF had this at 10%, but it was causing One Hundred Blows to break instantly often.
	uint32 max_slots = GetMaxBuffSlots();
	for (int i = 0; i < max_slots; i++)
	{
		if (buffs[i].spellid == 303) // Whirl till you hurl. RoF client has a 12% chance and is based on level, but based on research for our era that wasn't high enough.
		{
			chance = 20;
			break;
		}
		else if (buffs[i].spellid == 619) // Dyn`s Dizzying Draught
		{
			chance = 20;
			break;
		}
	}

	if (zone->random.Roll(chance))
	{
		BuffFadeByEffect(SE_SpinTarget);
		UnStun();
		spun_resist_timer.Disable();
		Log(Logs::General, Logs::Spells, "SpinStun spell has been broken with a %d chance.", chance);
	}
}

uint8 Mob::GetClickLevel(Mob* caster, uint16 spell_id)
{
	// This is for clickies that should use click level instead of caster level.

	if (!caster || !caster->IsClient())
	{
		return 0;
	}

	if (caster->GetCastedSpellInvSlot() > 0)
	{
		const EQ::ItemInstance *inst = caster->CastToClient()->GetInv().GetItem(caster->GetCastedSpellInvSlot());
		if (inst)
		{
			if (inst->IsTieredPotion())
			{
				return inst->GetItem()->Click.Level;
			}
		}
	}

	return 0;
}

bool Mob::DoCastingRangeCheck(uint16 spell_id, CastingSlot slot, Mob* spell_target)
{
	// First range check. The client largely handles this, we are mainly here for the focus message and cheat detection.
	if (spell_target != nullptr && spell_target != this)
	{
		//range check our target, if we have one and it is not us
		float range = spells[spell_id].range;
		if (IsClient() && CastToClient()->TGB() && IsTGBCompatibleSpell(spell_id) && IsGroupSpell(spell_id) && slot != CastingSlot::Item)
			range = spells[spell_id].aoerange;

		float dist2 = DistanceSquaredNoZ(m_Position, spell_target->GetPosition());
		float dist3 = std::abs(GetZ() - spell_target->GetZ());
		float range2 = range * range;
		if (dist2 > range2 || (dist3 > range && spells[spell_id].targettype != ST_Target))
		{
			std::string item_name;
			float focus_range = GetActSpellRange(spell_id, range, item_name);
			if (focus_range > range)
			{
				Log(Logs::General, Logs::Focus, "focusRange is checking the out of range mob using range %0.1f", focus_range);

				float focus_range2 = focus_range * focus_range;
				if (dist2 > focus_range2 || (dist3 > focus_range && spells[spell_id].targettype != ST_Target))
				{
					Log(Logs::General, Logs::Spells, "Target is out of range, client will cancel this cast for us.");
					return false;
				}
				else
				{
					Message_StringID(Chat::Spells, PULSES_WITH_LIGHT, item_name.c_str());
					return true;
				}
			}
			else
			{
				Log(Logs::General, Logs::Spells, "Target is out of range, client will cancel this cast for us.");
				return false;
			}
		}
	}

	return true;
}

bool Mob::InSameGroup(Mob *other)
{
	if (!other)
		return false;

	// self
	if (this == other)
		return true;

	// group
	if (this->IsGrouped() && other->IsGrouped())
	{
		Group *g1 = this->GetGroup();
		Group *g2 = other->GetGroup();

		if(g1 && g2)
			if (g1->GetID() == g2->GetID())
				return true;
	}

	// raid group
	if (this->IsRaidGrouped() && other->IsRaidGrouped())
	{
		if (this->IsClient() && other->IsClient())
		{
			// solar 20220729 seems like IsRaidGrouped() can get out of sync so we need to check they're really in a raid to avoid a crash
			Raid *r1 = this->GetRaid();
			Raid *r2 = other->GetRaid();
			Client *c1 = this->CastToClient();
			Client *c2 = other->CastToClient();

			if (r1 && r2)
				if (r1->GetID() == r2->GetID())
					if (r1->GetGroup(c1) != 0xFFFFFFFF)
						if (r1->GetGroup(c1) == r2->GetGroup(c2))
							return true;
		}
	}

	return false;
}

bool Mob::InSameRaid(Mob *other)
{
	if (!other)
		return false;

	// self
	if (this == other)
		return true;

	Raid *r1 = this->GetRaid();
	Raid *r2 = other->GetRaid();

	if (r1 && r2)
		if (r1->GetID() == r2->GetID())
			return true;

	return false;
}

bool Client::RestictedManastoneClick(int16 zone_id)
{
	return {
		EQ::ValueWithin(zone_id, Zones::FIELDOFBONE, Zones::POTIMEB) ||
		zone_id == Zones::AIRPLANE ||
		zone_id == Zones::HATEPLANE ||
		zone_id == Zones::FEARPLANE
	};
}
