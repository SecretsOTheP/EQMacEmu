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


#include "../common/spdat.h"
#include "string_ids.h"

#include "mob.h"
#include "client.h"

#include <assert.h>

#ifndef WIN32
#include <stdlib.h>
#include "../common/unix.h"
#endif

/*
* solar: This buff stacking locking is based on client decompile and studying the binary code.  The way the server
* calculates the buff slot a buff lands in and what can't stack has to match the client exactly.
* In general we can't make things stack if they aren't supposed to stack. An example of this is the ranger
* spell Warder's Protection blocking the bard song Psalm of Veeshan.  This is due to a bug in IsStackBlocked()
* and if we try to make it stack by altering the logic here, the client will just ignore us and the buff will
* not show up there, and any further buffs will be out of sync.
*
* If the server computes the wrong buff slot then a desync occurs and actions which act on slot numbers
* will be acting on the wrong effects.  This isn't a situation where the server can push down authoritative
* information, it's a matter of the server's logic following what the client logic is, unless the client is
* also modified I guess.
*
* TAKP spells that have been modified in the database may cause stacking logic desync and this code should be using
* unmodified spell data to match the client better.
*/

class BuffStacking
{
public:
	static bool IsValidSpellIndex(int spell_id)
	{
		if (spell_id < 1 || spell_id >= EQ::spells::SPELL_ID_MAX)
			return false;

		return true;
	}

	static bool IsSPAIgnoredByStacking(uint8 effectid)
	{
		switch (effectid)
		{
			case SE_SeeInvis:
			case SE_Stamina:
			case SE_DiseaseCounter:
			case SE_PoisonCounter:
			case SE_Levitate:
			case SE_InfraVision:
			case SE_UltraVision:
			case SE_CurrentHPOnce:
			case SE_CurseCounter:

			case SE_ImprovedDamage:
			case SE_ImprovedHeal:
			case SE_SpellResistReduction:
			case SE_IncreaseSpellHaste:
			case SE_IncreaseSpellDuration:
			case SE_IncreaseRange:
			case SE_SpellHateMod:
			case SE_ReduceReagentCost:
			case SE_ReduceManaCost:
			case SE_FcStunTimeMod:
			case SE_LimitMaxLevel:
			case SE_LimitResist:
			case SE_LimitTarget:
			case SE_LimitEffect:
			case SE_LimitSpellType:
			case SE_LimitSpell:
			case SE_LimitMinDur:
			case SE_LimitInstant:
			case SE_LimitMinLevel:
			case SE_LimitCastTimeMin:
			case SE_LimitCastTimeMax:

			case SE_StackingCommand_Block:
			case SE_StackingCommand_Overwrite:
			case SE_Blank:
				return true;
		}

		return false;
	}

	static bool IsStackWithSelfAllowed(SPDat_Spell_Struct *eqs)
	{
		if (eqs->dot_stacking_exempt || eqs->goodEffect || !eqs->buffdurationformula)
			return false;

		for (int effect_slot_num = 0; effect_slot_num < EFFECT_COUNT; effect_slot_num++)
		{
			if (eqs->effectid[effect_slot_num] == SE_CurrentHP)
				return true;
		}

		return false;
	}
};

// this processes effect 148 (SE_StackingCommand_Block) in the target's current buffs to see if new_buff_spell_id is blocked by one of them
bool Mob::IsStackBlocked(uint16 new_buff_spell_id)
{
	SPDat_Spell_Struct *new_buff_spelldata = (SPDat_Spell_Struct *)&spells[new_buff_spell_id];

	if (new_buff_spelldata == nullptr)
		return 0;

	// intentional bug reproduced here, it's checking the class of the target of the buff, not the caster of it
	// bard songs were supposed to stack with everything that's not a bard song, but this was botched
	if (new_buff_spelldata->classes[BARD - 1] != 255 && this->GetClass() == BARD)
		return 0;

	for (int buffslot = 0; buffslot < this->GetMaxBuffSlots(); buffslot++)
	{
		if (BuffStacking::IsValidSpellIndex(this->buffs[buffslot].spellid))
		{
			uint16 old_buff_spell_id = this->buffs[buffslot].spellid;

			if ((old_buff_spell_id != new_buff_spell_id || new_buff_spell_id == 2751))// Manaburn
			{
				for (int old_buff_effect_slot_number = 0; old_buff_effect_slot_number < EFFECT_COUNT; old_buff_effect_slot_number++)
				{
					SPDat_Spell_Struct *old_buff_spelldata = (SPDat_Spell_Struct *)&spells[old_buff_spell_id];
					uint8 old_buff_effectid = old_buff_spelldata->effectid[old_buff_effect_slot_number];

					if (old_buff_effectid == SE_Blank) // blank, end of spell, skip rest of slots
						break;

					if (old_buff_effectid == SE_StackingCommand_Block)
					{
						uint8 blocked_effect_id = old_buff_spelldata->base[old_buff_effect_slot_number];
						int blocked_effect_slot_num = old_buff_spelldata->formula[old_buff_effect_slot_number];
						if (blocked_effect_slot_num > 200)
							blocked_effect_slot_num -= 201;
						int blocked_below_value = abs(old_buff_spelldata->max[old_buff_effect_slot_number]);

						if (new_buff_spelldata->effectid[blocked_effect_slot_num] == blocked_effect_id)
						{
							int new_effect_max = new_buff_spelldata->max[blocked_effect_slot_num];
							int new_effect_value = new_buff_spelldata->base[blocked_effect_slot_num];
							if (new_effect_max)
								new_effect_value = new_effect_max;

							if (new_effect_value <= blocked_below_value)
							{
								// what this does is check if a spell of the same line is overwriting.  this is what allows Protection of the Wild to overwrite Warder's Protection
								for (int new_buff_effect_slot_number = 0; new_buff_effect_slot_number < EFFECT_COUNT; new_buff_effect_slot_number++)
								{
									if (new_buff_spelldata->effectid[new_buff_effect_slot_number] == SE_StackingCommand_Block)
									{
										if (new_buff_spelldata->base[new_buff_effect_slot_number] == old_buff_spelldata->base[old_buff_effect_slot_number])
										{
											if (new_buff_spelldata->formula[new_buff_effect_slot_number] == old_buff_spelldata->formula[old_buff_effect_slot_number])
											{
												new_effect_value = new_buff_spelldata->max[new_buff_effect_slot_number];
											}
										}
									}
								}

								if (new_effect_value <= blocked_below_value)
									return 1; // new buff is blocked
							}
						}
					}
				}
			}
		}
	}

	return 0;
}

// this processes effect 149 (SE_StackingCommand_Overwrite) in the newly applied buff to remove any existing buffs that match the filter
// this is what causes Aegolism to 'overwrite' the Symbol line of spells, as they don't otherwise conflict on effect slots.
void Mob::ProcessBuffOverwriteEffects(uint16 spell_id)
{
	for (int effect_slot_num = 0; effect_slot_num < EFFECT_COUNT; effect_slot_num++)
	{
		if (spells[spell_id].effectid[effect_slot_num] == SE_Blank)
			return; // end of spell effects

		if (spells[spell_id].effectid[effect_slot_num] == SE_StackingCommand_Overwrite)
		{
			int overwrite_effectid = spells[spell_id].base[effect_slot_num];
			int overwrite_slot = spells[spell_id].formula[effect_slot_num] - 201;
			int overwrite_below_value = spells[spell_id].max[effect_slot_num];

			for (int buffslot = 0; buffslot < GetMaxBuffSlots(); buffslot++)
			{
				struct Buffs_Struct *old_buff = &this->buffs[buffslot];
				if (!IsValidSpell(old_buff->spellid))
					continue;
				if (old_buff->spellid == spell_id)
					continue;

				if (spells[old_buff->spellid].effectid[overwrite_slot] == overwrite_effectid)
				{
					if (!spells[old_buff->spellid].goodEffect)
						continue;
					if (spells[old_buff->spellid].classes[BARD - 1] != 255)
						continue;
					if (GetSpellEffectIndex(old_buff->spellid, SE_Illusion) != -1)
						continue;
					if (GetSpellEffectIndex(old_buff->spellid, SE_CompleteHeal) != -1)
						continue;

					// i'm not sure if the value should be calculated like this using the formula and caster level, or if it should simply be compared to the base/max values
					// since this is a server driven action, even if it's wrong in what it removes, it won't desync the client buffs and this comparison can be tweaked
					int old_effect_value = CalcSpellEffectValue(old_buff->spellid, overwrite_slot, old_buff->casterlevel, 0, 10);
					if (old_effect_value <= overwrite_below_value)
					{
						// new spell removes this buff
						BuffFadeBySlot(buffslot, true, true, true);
					}
				}
			}
		}
	}
}

// the name of this function and its contents are based on studying the client binary code
int Mob::FindAffectSlot(Mob *caster, uint16 spell_id, int *result_slotnum, int remove_replaced)
{
	bool spell_id_already_affecting_target;
	int emptyslot;
	int16 buffslotnum;
	struct Buffs_Struct *buff3;
	struct Buffs_Struct *old_buff;
	SPDat_Spell_Struct *old_spelldata;
	bool no_slot_found_yet;
	uint8 old_spell_bard_level;
	int16 old_effect_value;
	int16 new_effect_value;
	bool old_effect_is_negative_or_zero;
	bool new_spell_is_diseased_cloud;
	bool old_effect_value_is_negative_or_zero;
	uint8 new_buff_effectid2;
	int curbuff_slotnum;
	//int curbuf_spell_id;
	SPDat_Spell_Struct *curbuf_spelldata;
	int16 old_buff_spellid;
	uint8 effect_slot_num;
	//int16 cur_slotnum7;
	char is_stack_with_self_allowed;

	*result_slotnum = -1;
	if (!caster || !BuffStacking::IsValidSpellIndex(spell_id))
		return 0;

	if (!this->GetMaxBuffSlots() || !caster->GetMaxBuffSlots()) // only NPC and Client can have buffs
	{
		return 0;
	}

	// TAKP special, to allow Fennin Ro's AoE to overwrite bard Selos.
	if (spell_id == 3055) // Cataclysm of Ro
	{
		for (int i = 0; i < this->GetMaxBuffSlots(); i++)
		{
			if (buffs[i].spellid != SPELL_UNKNOWN)
			{
				SPDat_Spell_Struct *old_buff_spelldata = (SPDat_Spell_Struct *)&spells[buffs[i].spellid];

				if (old_buff_spelldata->classes[BARD - 1] != 255 && old_buff_spelldata->goodEffect && GetSpellEffectIndex(old_buff_spelldata->id, SE_MovementSpeed) != -1)
				{
					// dispel Selo's to make this work
					if (remove_replaced)  // only 0 for Mob AI considering spells to cast
					{
						BuffFadeBySlot(i, true, true, true);
					}
				}
			}
		}
	}

	// TAKP special, to allow Bertoxxulous's AoE to overwrite mana/regen buffs.  This is a deviation from AKurate to make this boss more interesting
	if (spell_id == 3012) // Rot of the Plaguebringer
	{
		for (int i = 0; i < this->GetMaxBuffSlots(); i++)
		{
			if (buffs[i].spellid != SPELL_UNKNOWN)
			{
				SPDat_Spell_Struct *old_buff_spelldata = (SPDat_Spell_Struct *)&spells[buffs[i].spellid];

				if (old_buff_spelldata->goodEffect)
				{
					if (old_buff_spelldata->effectid[1] == SE_CurrentMana || old_buff_spelldata->effectid[3] == SE_CurrentHP)
					{
						// dispel conflicting regen buffs otherwise the 1 base value in spell 3012 will not overwrite them and the AoE ends up doing nothing
						if (remove_replaced)  // only 0 for Mob AI considering spells to cast
						{
							BuffFadeBySlot(i, true, true, true);
						}
					}
				}
			}
		}
	}

	// prevent multiple invis stacking/overwriting
	if (IsInvisSpell(spell_id))
	{
		for (int i = 0; i < this->GetMaxBuffSlots(); i++)
		{
			if (buffs[i].spellid != SPELL_UNKNOWN)
			{
				if (IsInvisSpell(buffs[i].spellid))
				{
					// bard song invis can overwrite each other
					if ((spells[spell_id].classes[BARD - 1] != 255 && spells[buffs[i].spellid].classes[BARD - 1] != 255) || spell_id == buffs[i].spellid)
					{
						continue;
					}

					if (caster != this)
						Message_StringID(CC_User_SpellFailure, ALREADY_INVIS, caster->GetCleanName());
					return 0;
				}
			}
		}
	}

	SPDat_Spell_Struct *new_spelldata = (SPDat_Spell_Struct *)&spells[spell_id]; // TODO - have to use same spells as client, not the customized TAKP spells in the database

	if (!new_spelldata || !caster->IsNPC() && IsStackBlocked(spell_id))
		return 0;

	// Screech Immunity
	if (!new_spelldata->goodEffect)
	{
		for (int effect_slot = 0; effect_slot < EFFECT_COUNT; effect_slot++)
		{
			if (new_spelldata->effectid[effect_slot] == SE_Screech)
			{
				if (spellbonuses.Screech + new_spelldata->base[effect_slot] >= 0)
				{
					Message_StringID(CC_User_SpellFailure, SCREECH_BUFF_BLOCK, new_spelldata->name);
					return 0;
				}
			}
		}
	}

	if (new_spelldata->classes[BARD - 1] != 255 && caster->GetClass() == BARD)  // Bard casting bard song
	{
		for (int cur_slotnum2 = 0; cur_slotnum2 < this->GetMaxBuffSlots(); cur_slotnum2++)
		{
			struct Buffs_Struct *old_buff2 = &this->buffs[cur_slotnum2];

			if (old_buff2->spellid != SPELL_UNKNOWN && BuffStacking::IsValidSpellIndex(old_buff2->spellid))
			{
				SPDat_Spell_Struct *old_buff_spelldata2 = (SPDat_Spell_Struct *)&spells[old_buff2->spellid];
				if (old_buff_spelldata2
					&& old_buff_spelldata2->classes[BARD - 1] == 255 // existing buff is not bard song
					&& !old_buff_spelldata2->goodEffect // existing buff is detrimental
					&& new_spelldata->goodEffect // new buff is beneficial bard song
					&& GetSpellEffectIndex(new_spelldata->id, SE_MovementSpeed) != -1 // Selo`s Accelerando, Selo`s Song of Travel, Selo`s Accelerating Chorus
					&& (GetSpellEffectIndex(old_buff_spelldata2->id, SE_MovementSpeed) != -1 // existing debuff is a root or snare
						|| GetSpellEffectIndex(old_buff_spelldata2->id, SE_Root) != -1)
					)
				{
					// Selos can't take hold if target is rooted or snared
					*result_slotnum = -1;
					return 0;
				}
			}
		}
	}

	// this flag is true for spells that can have multiple copies of the same spell_id buff on the target
	// things that can do this are DoTs from different casters, but some DoTs are specifically excluded
	// because they contain a debuff that shouldn't be doubled up.  Bard chants, druid fire DoT and Lifeburn
	is_stack_with_self_allowed = BuffStacking::IsStackWithSelfAllowed(new_spelldata);
	spell_id_already_affecting_target = 0;


	for (int cur_slotnum = 0; cur_slotnum < this->GetMaxBuffSlots(); cur_slotnum++)
	{
		struct Buffs_Struct *old_buff3 = &this->buffs[cur_slotnum];
		if (old_buff3->spellid != SPELL_UNKNOWN)
		{
			if (old_buff3->spellid == spell_id)
			{
				if (!this || caster->IsNPC() || !this->IsNPC())
					goto OVERWRITE_SAME_SPELL_WITHOUT_REMOVING_FIRST;

				if (old_buff3->spellid == 2755)      // Lifeburn
					is_stack_with_self_allowed = 0;

				if (!is_stack_with_self_allowed || caster->GetID() == old_buff3->casterid)
				{
				OVERWRITE_SAME_SPELL_WITHOUT_REMOVING_FIRST:
					if (caster->GetLevel() >= old_buff3->casterlevel
						&& GetSpellEffectIndex(new_spelldata->id, SE_EyeOfZomm) == -1
						&& GetSpellEffectIndex(new_spelldata->id, SE_CompleteHeal) == -1 // Donal's BP
						&& GetSpellEffectIndex(new_spelldata->id, SE_SummonHorse) == -1)
					{
						// overwrite same spell_id without removing first
						*result_slotnum = cur_slotnum;
						return 1;
					}
					else
					{
						// did not take hold
						*result_slotnum = -1;
						return 0;
					}
				}
				spell_id_already_affecting_target = 1;
			}
		}
	}

	if (is_stack_with_self_allowed)
	{
		if (spell_id_already_affecting_target)
		{
			emptyslot = -1;
			buffslotnum = 0;
			if (this->GetMaxBuffSlots())
			{
				do
				{
					buff3 = &this->buffs[buffslotnum];
					if (buff3->spellid != SPELL_UNKNOWN)
					{
						if (spell_id == buff3->spellid)
						{
							Mob *old_buff_caster = entity_list.GetMob(buff3->casterid);
							if (!old_buff_caster || old_buff_caster == this)
							{
								*result_slotnum = buffslotnum;
								return 1; // overwrite same spell without removing first
							}
						}
					}
					else if (emptyslot == -1)
					{
						emptyslot = buffslotnum;
					}
					++buffslotnum;
				} while (buffslotnum < this->GetMaxBuffSlots());
				if (emptyslot != -1)
				{
					*result_slotnum = emptyslot;
					return emptyslot; // first empty slot, this is a DoT that will stack with itself because it's from another caster
				}
			}
		}
	}


	int16 cur_slotnum7 = 0;

	if (false) // not entered here, jumped into with goto
	{
	STACK_OK_OVERWRITE_BUFF_IF_NEEDED:
		// if we have a result slot already, overwrite the result slot if something is there and it's not this spell_id
		if (*result_slotnum != -1)
		{
			if (this->buffs[*result_slotnum].spellid != SPELL_UNKNOWN && spell_id != this->buffs[*result_slotnum].spellid)
			{
				if (remove_replaced == 2)
				{
					this->BuffFadeBySlot(*result_slotnum, true, true, true); // remove it on the client too, used for overwriting secondary buffs once replacement slot found
				}
				else if (remove_replaced == 1)
				{
					this->BuffFadeBySlot(*result_slotnum, true, true, false); // Overwriting without removing explicitly, client will handle this
				}
			}
			return 1;
		}

		// no result slot found yet, but the new spell is a debuff so overwrite a beneficial buff to make room for it
		if (!new_spelldata->goodEffect)
		{
			if (this)
			{
				if (!(this->IsClient() && this->CastToClient()->GetGM()))
				{
					curbuff_slotnum = 0;
					if (this->GetMaxBuffSlots())
					{
						while (1)
						{
							int curbuf_spell_id = this->buffs[curbuff_slotnum].spellid;
							if (BuffStacking::IsValidSpellIndex(curbuf_spell_id))
							{
								curbuf_spelldata = (SPDat_Spell_Struct *)&spells[curbuf_spell_id];
								if (curbuf_spelldata && curbuf_spelldata->goodEffect) // found a beneficial spell to overwrite
									break;
							}
							if (++curbuff_slotnum >= this->GetMaxBuffSlots())
								return 0;
						}
						if (remove_replaced) // only 0 for Mob AI considering spells to cast
						{
							this->BuffFadeBySlot(curbuff_slotnum, true, true, true); // overwriting a beneficial buff to make room for a detrimental one.  this is server driven and we have to remove the buff from the client
						}
						else
						{
							*result_slotnum = -1;
							return 0;
						}
						*result_slotnum = curbuff_slotnum;
						goto RETURN_RESULT_SLOTNUM;
					}
				}
			}
		}

		return 0;
	}


	// iterate over each buff in turn below

	while (1)
	{
		old_buff = &this->buffs[cur_slotnum7];
		if (old_buff->spellid == SPELL_UNKNOWN) // blank buff slot at cur_slotnum7
			goto STACK_OK4;



		// if a buff is in cur_slotnum7, break out of this block and keep comparing
		old_buff_spellid = old_buff->spellid;
		if (BuffStacking::IsValidSpellIndex(old_buff->spellid))
		{
			old_spelldata = (SPDat_Spell_Struct *)&spells[old_buff->spellid];
			break;
		}

	STACK_OK4:
		no_slot_found_yet = *result_slotnum == -1;
	STACK_OK3:
		if (no_slot_found_yet)
		{
		STACK_OK2:
			*result_slotnum = cur_slotnum7; // save first blank slot found
		}
	STACK_OK: // jump here when current buff and new buff don't interact to increment slot number and check next buff
		if (++cur_slotnum7 >= this->GetMaxBuffSlots())
		{
			goto STACK_OK_OVERWRITE_BUFF_IF_NEEDED;
		}
	}
	if (caster->GetClass() == BARD && old_spelldata->classes[BARD - 1] == 255) // Bard caster and old buff is not a bard song
	{
		if (new_spelldata->goodEffect
			&& GetSpellEffectIndex(new_spelldata->id, SE_MovementSpeed) != -1
			&& GetSpellEffectIndex(old_spelldata->id, SE_MovementSpeed) != -1
			|| new_spelldata->goodEffect
			&& GetSpellEffectIndex(new_spelldata->id, SE_MovementSpeed) != -1
			&& GetSpellEffectIndex(old_spelldata->id, SE_Root) != -1)
		{
			goto BLOCKED_BUFF;                        // Bard Selos can't overwrite regular SoW type spell or rooting illusion
		}

		// generally, bard songs stack with anything that's not a bard song
		if (new_spelldata->classes[BARD - 1] != 255 && caster->GetClass() == BARD) // Bard casting bard song
			goto STACK_OK;                            // Bard song can stack with non bard song
	}


	if (new_spelldata->goodEffect)
	{
		if (old_spelldata->goodEffect)
		{
			if (GetSpellEffectIndex(new_spelldata->id, SE_MovementSpeed) != -1)
			{
				if (GetSpellEffectIndex(old_spelldata->id, SE_MovementSpeed) != -1)
				{
					old_spell_bard_level = old_spelldata->classes[BARD - 1];
					if (old_spell_bard_level)
					{
						if (old_spell_bard_level != 255 && new_spelldata->classes[BARD - 1] == 255)
							goto BLOCKED_BUFF;                // regular SoW type spell can't overwrite bard Selos
					}
				}
			}
		}
	}


	// below is a for loop that's kind of decomposed with gotos, comparing each effect slot
	effect_slot_num = 0;
	while (2)
	{
		uint8 old_buff_effectid = old_spelldata->effectid[effect_slot_num];
		if (old_buff_effectid == SE_Blank) // blank effect slot in old spell, end of spell, don't check rest of slots
			goto STACK_OK;

		uint8 new_buff_effectid = new_spelldata->effectid[effect_slot_num];
		if (new_buff_effectid == SE_Blank) // blank effect slot in new spell, end of spell, don't check rest of slots
			goto STACK_OK;

		if (new_buff_effectid == SE_Lycanthropy || new_buff_effectid == SE_Vampirism)
			goto BLOCKED_BUFF;

		// this is so KEI can stack with mana song
		if ((old_buff_effectid == SE_CurrentMana || old_buff_effectid == SE_CurrentHP || old_buff_effectid == SE_HealOverTime)
			&& old_spelldata->classes[BARD - 1] != 255 && new_spelldata->classes[BARD - 1] == 255 // old buff is a bard song and new buff is not a bard song
			|| old_buff_effectid != new_buff_effectid // or effects don't conflict
			|| BuffStacking::IsSPAIgnoredByStacking(new_buff_effectid))
		{
			goto NEXT_EFFECT_SLOT;                    // ignore if different effect, ignored effect, or if the existing buff is a bard hp/mana regen song
		}

		// at this point the effect ids are the same in this slot

		if (new_buff_effectid == SE_CurrentHP || new_buff_effectid == SE_ArmorClass)
		{
			if (new_spelldata->base[effect_slot_num] >= 0)
				break;
			goto NEXT_EFFECT_SLOT; // if the new spell has a DoT or negative AC debuff in this effect slot, ignore for stacking
		}

		if (new_buff_effectid == SE_CHA)
		{
			if (new_spelldata->base[effect_slot_num] == 0 || old_spelldata->base[effect_slot_num] == 0) // SE_CHA can be used as a spacer with 0 base
			{
			NEXT_EFFECT_SLOT:
				if (++effect_slot_num >= 12u)
					goto STACK_OK;
				continue;
			}
		}
		break;
	}



	// compare same effect id below


	if (new_spelldata->goodEffect && (!old_spelldata->goodEffect || GetSpellEffectIndex(old_spelldata->id, SE_Illusion) != -1)
		|| old_spelldata->effectid[effect_slot_num] == SE_CompleteHeal	// Donal's BP effect
		|| old_buff_spellid >= 775 && old_buff_spellid <= 785
		|| old_buff_spellid >= 1200 && old_buff_spellid <= 1250
		|| old_buff_spellid >= 1900 && old_buff_spellid <= 1924
		|| old_buff_spellid == 2079 // ShapeChange65
		|| old_buff_spellid == 2751 // Manaburn
		|| old_buff_spellid == 756 // Resurrection Effects
		|| old_buff_spellid == 757 // Resurrection Effect
		|| old_buff_spellid == 836) // Diseased Cloud
	{
		goto BLOCKED_BUFF;
	}

	old_effect_value = CalcSpellEffectValue(old_spelldata->id, effect_slot_num, old_buff->casterlevel, 0, 10);
	new_effect_value = CalcSpellEffectValue(new_spelldata->id, effect_slot_num, caster->GetLevel(), 0, 10);

	if (spell_id == 1620 || spell_id == 1816 || spell_id == 833 || old_buff_spellid == 1814)// mistake or is this intentional?
		new_effect_value = -1;
	if (old_buff_spellid == 1620 || old_buff_spellid == 1816 || old_buff_spellid == 833 || old_buff_spellid == 1814)
		old_effect_value = -1;
	old_effect_is_negative_or_zero = old_effect_value <= 0;
	if (old_effect_value >= 0)
	{
	OVERWRITE_INCREASE_WITH_DECREASE2:
		if (!old_effect_is_negative_or_zero && new_effect_value < 0)
			goto OVERWRITE_INCREASE_WITH_DECREASE;
		new_spell_is_diseased_cloud = spell_id == 836 ? true : false;
		if (new_spelldata->effectid[effect_slot_num] == SE_AttackSpeed)
		{
			if (new_effect_value < 100 && new_effect_value <= old_effect_value)
				goto OVERWRITE;
			if (old_effect_value <= 100)
				goto BLOCKED_BUFF3;
			if (new_effect_value >= 100)
			{
			OVERWRITE_IF_GREATER_BLOCK_OTHERWISE:
				if (new_effect_value >= old_effect_value)
					goto OVERWRITE;
			BLOCKED_BUFF3:
				if (!new_spell_is_diseased_cloud)
					goto BLOCKED_BUFF;
				if (!new_spelldata->goodEffect && !old_spelldata->goodEffect)
				{
					*result_slotnum = cur_slotnum7;
					if (spell_id != old_buff->spellid)
						goto OVERWRITE_REMOVE_FIRST;
					goto RETURN_RESULT_SLOTNUM;
				}
				if (*result_slotnum == -1)
					goto STACK_OK2;
				no_slot_found_yet = this->buffs[*result_slotnum].spellid == SPELL_UNKNOWN;
				goto STACK_OK3;
			}
		OVERWRITE:
			new_spell_is_diseased_cloud = true;
			goto BLOCKED_BUFF3;
		}
		old_effect_value_is_negative_or_zero = old_effect_value <= 0;
		if (old_effect_value < 0)
		{
			if (new_effect_value <= old_effect_value)
				goto OVERWRITE;
			old_effect_value_is_negative_or_zero = old_effect_value <= 0;
		}
		if (old_effect_value_is_negative_or_zero)
			goto BLOCKED_BUFF3;
		goto OVERWRITE_IF_GREATER_BLOCK_OTHERWISE;
	}
	if (new_effect_value <= 0)
	{
		old_effect_is_negative_or_zero = old_effect_value <= 0;
		goto OVERWRITE_INCREASE_WITH_DECREASE2;
	}
OVERWRITE_INCREASE_WITH_DECREASE:
	new_buff_effectid2 = new_spelldata->effectid[effect_slot_num];
	if (new_buff_effectid2 != SE_MovementSpeed)
	{
		if (new_buff_effectid2 != SE_CurrentHP || old_effect_value >= 0 || new_effect_value <= 0)
			goto OVERWRITE_REMOVE_FIRST_CUR_SLOTNUM;
	BLOCKED_BUFF:
		*result_slotnum = -1;
		return 0; // this should be reached before overwriting anything if it doesn't stack?
	}
	if (new_effect_value >= 0)
		goto BLOCKED_BUFF;
OVERWRITE_REMOVE_FIRST_CUR_SLOTNUM:
	*result_slotnum = cur_slotnum7;
	if (spell_id != old_buff->spellid)
	{
	OVERWRITE_REMOVE_FIRST:
		if (remove_replaced == 2)
		{
			this->BuffFadeBySlot(*result_slotnum, true, true, true); // remove it on the client too, used for overwriting secondary buffs once replacement slot found
		}
		else if (remove_replaced == 1)
		{
			this->BuffFadeBySlot(*result_slotnum, true, true, false); // Overwriting without removing explicitly, client will handle this
		}
	}

RETURN_RESULT_SLOTNUM:
	return 1;
}

// called from SpellOnTarget()
bool Mob::AssignBuffSlot(Mob *caster, uint16 spell_id, int &buffslot, int &caster_level, EQApplicationPacket *action_packet)
{
	if (!IsValidSpell(spell_id))
		return false;

	// the inventory slot number comes from Mob.casting_spell_inventory_slot which gets set in DoCastSpell()
	uint8 item_level = caster ? caster->GetClickLevel(caster, spell_id) : 0;
	int duration = 0;

	if (item_level > 0)
	{
		caster_level = item_level;
		duration = CalcBuffDuration(caster, this, spell_id, caster_level);
	}
	else
	{
		caster_level = caster ? caster->GetCasterLevel(spell_id) : GetCasterLevel(spell_id);
		duration = CalcBuffDuration(caster, this, spell_id);
	}
	if (duration == 0)
	{
		Log(Logs::General, Logs::Spells, "Buff %d failed to add because its duration came back as 0.", spell_id);
		return false;
	}

	Log(Logs::Detail, Logs::Spells, "Trying to add buff %d cast by %s (cast level %d) with duration %d",
		spell_id, caster ? caster->GetName() : "UNKNOWN", caster_level, duration);

	int emptyslot = -1;
	bool isdisc = IsDisc(spell_id);

	if (isdisc && IsClient())
	{
		// cavedude's discipline implementation uses a 16th buff slot to hold discipline spells and these don't interact with buffs
		buffslot = emptyslot = CastToClient()->GetDiscBuffSlot();
		BuffFadeBySlot(emptyslot, false, true, false);
	}
	else
	{
		// see if the buff will land or not
		int slot = -1;
		FindAffectSlot(caster, spell_id, &slot, 1); // remove_replaced = 1 suppresses sending a buff fade packet so the buff is still on the client after this returns, but cleared on server

		// buff is blocked for some reason, your spell did not take hold
		if (slot == -1)
		{
			return false;
		}

		// FindAffectSlot removes the buff it's overwriting in the buffs[] array but doesn't send a packet to the client.
		// The expectation is that our FindAffectSlot behaves identically to the client so that the slot number we just computed
		// is where the client will put the buff when we send the action packet to it.
		buffslot = emptyslot = slot;

		// the buff can stick, we need to get this packet to the client to get it to land in the slot we just determined.
		// we don't control the slot number, we just have to do things in the right order to make sure that our idea of what buff is where matches what the client does
		// this action packet is passed in already filled for us from SpellOnTarget() - to get things to overwrite in the correct order we have to send the action packet here
		{
			Action_Struct *action = (Action_Struct *)action_packet->pBuffer;
			action->buff_unknown = 0x04;	// this is a success flag
			if (IsClient())	// send to target
				CastToClient()->QueuePacket(action_packet);
			if (caster && caster != this && caster->IsClient())	// send to caster
				caster->CastToClient()->QueuePacket(action_packet);

			// This is the message for the spell.  Non buff spells send these in SpellOnTarget()
			if (!HasDirectDamageEffect(spell_id))	// spells with direct damage effect handled in CommonDamage()
			{
				EQApplicationPacket *message_packet = new EQApplicationPacket(OP_Damage, sizeof(Damage_Struct));
				Damage_Struct *cd = (Damage_Struct *)message_packet->pBuffer;
				cd->target = action->target;
				cd->source = action->source;
				cd->type = action->type;
				cd->spellid = action->spell;
				cd->sequence = action->sequence;
				cd->damage = 0;

				Log(Logs::Moderate, Logs::Spells, "AssignBuffSlot: SpellMessage target: %i, source: %i, type: %i, spellid: %i, sequence: %f, damage: %i BardSong: %i", cd->target, cd->source, cd->type, cd->spellid, cd->sequence, cd->damage, IsBardSong(spell_id));
				Log(Logs::Moderate, Logs::Spells, "Sending Message packet for spell %d", spell_id);

				// send to target unfiltered.
				if (IsClient())
				{
					CastToClient()->QueuePacket(message_packet);
				}
				// send to caster unfiltered.
				if (caster && caster != this && caster->IsClient())
				{
					caster->CastToClient()->QueuePacket(message_packet);
				}
				// send to people in the area, ignoring caster and target
				if (!IsBardSong(spell_id))
				{
					entity_list.QueueCloseClients(this, message_packet, true, (float)RuleI(Range, SpellMessages), caster, true, IsClient() ? FilterPCSpells : FilterNPCSpells);
				}
				else
				{
					entity_list.QueueCloseClients(this, message_packet, true, (float)RuleI(Range, SongMessages), caster, true, FilterBardSongs);
				}

				safe_delete(message_packet);
			}
		}

		// if overwriting the same spell id, we don't need to remove the spell and we must not remove it on the client, but still want to generate worn off messages.
		// at this point the new buff has already been sent to client with the packets above
		bool same_caster_refreshing = false;
		if (caster && buffs[emptyslot].spellid == spell_id && buffs[emptyslot].caster_name[0] && !strcmp(buffs[emptyslot].caster_name, caster->GetName()))
			same_caster_refreshing = true; // no fade/message for refreshing our own spell
		if (!same_caster_refreshing)
			BuffFadeBySlot(emptyslot, true, true, false);

		// process effect 149.  this is what overwrites symbol when aego lands but it happens after the buff lands
		ProcessBuffOverwriteEffects(spell_id);

		int i = 0;

		// buff can stack but FindAffectSlot only overwrote the first slot and we may need to remove more buffs in higher slot numbers
		do
		{
			FindAffectSlot(caster, spell_id, &slot, 2); // remove_replaced = 2 sends buff fade packet to remove buff on client too

			if (i > 100)
				break;
			i++;

		} while (slot > emptyslot);
	}

	if (emptyslot == -1)
		return false;

	// now add buff at emptyslot
	assert(buffs[emptyslot].spellid == SPELL_UNKNOWN || buffs[emptyslot].spellid == spell_id);	// sanity check

	int extraTick = (IsSplurtFormulaSpell(spell_id)) ? 0 : 1;  // all buffs get an extra tick, except splurts
	buffs[emptyslot].spellid = spell_id;
	buffs[emptyslot].casterlevel = caster_level;
	buffs[emptyslot].realcasterlevel = caster ? caster->GetLevel() : caster_level;
	if (caster && caster->IsClient())
		strcpy(buffs[emptyslot].caster_name, caster->GetName());
	else
		memset(buffs[emptyslot].caster_name, 0, 64);
	buffs[emptyslot].casterid = caster ? caster->GetID() : 0;
	buffs[emptyslot].ticsremaining = duration + extraTick;
	buffs[emptyslot].counters = CalculateCounters(spell_id);
	buffs[emptyslot].client = caster ? caster->IsClient() : 0;
	buffs[emptyslot].persistant_buff = 0;
	buffs[emptyslot].ExtraDIChance = 0;
	buffs[emptyslot].RootBreakChance = 0;
	buffs[emptyslot].instrumentmod = 10;
	buffs[emptyslot].isdisc = isdisc;
	buffs[emptyslot].remove_me = false;
	buffs[emptyslot].first_tic = true;

	// we don't tell the client the bufftype except when loading buffs from the database - we have to stay in sync by computing the same bufftype here
	buffs[emptyslot].bufftype = 2;
	if ((spells[spell_id].targettype == ST_TargetAETap || spells[spell_id].targettype == ST_Tap) && caster == this)
		buffs[emptyslot].bufftype = 4; // reversed tap effect

	if (item_level > 0)
	{
		buffs[emptyslot].UpdateClient = true;
	}
	else
	{
		if (buffs[emptyslot].ticsremaining > (extraTick + CalcBuffDuration_formula(caster_level, spells[spell_id].buffdurationformula, spells[spell_id].buffduration)))
			buffs[emptyslot].UpdateClient = true;
	}

	if (IsBardSong(spell_id) && caster)
	{
		int mod = caster->GetInstrumentMod(spell_id);
		if (mod > 10)
			buffs[emptyslot].instrumentmod = mod;
	}

	Log(Logs::Detail, Logs::Spells, "Buff %d added to slot %d with caster level %d", spell_id, emptyslot, caster_level);

	CalcBonuses();

	return true;
}

