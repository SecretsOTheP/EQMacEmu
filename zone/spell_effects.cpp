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

#include "../common/eqemu_logsys.h"
#include "../common/bodytypes.h"
#include "../common/classes.h"
#include "../common/global_define.h"
#include "../common/item_instance.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"

#include <math.h>

#ifndef WIN32
#include <stdlib.h>
#include "../common/unix.h"
#endif


extern Zone* zone;
extern volatile bool is_zone_loaded;
extern WorldServer worldserver;


// the spell can still fail here, if the buff can't stack
// in this case false will be returned, true otherwise
bool Mob::SpellEffect(Mob* caster, uint16 spell_id, int buffslot, int caster_level, float partial)
{
	int effect, effect_value, i;
	EQ::ItemInstance *SummonedItem = nullptr;

	if (!IsValidSpell(spell_id))
		return false;

	const SPDat_Spell_Struct &spell = spells[spell_id];

	Log(Logs::Detail, Logs::Spells, "%s is affected by spell '%s' (id %d)", GetName(), spell.name, spell_id);

	if(buffslot >= 0 && !IsCorpse())
	{
		Log(Logs::Detail, Logs::Spells, "Buff slot: %d  Duration: %d tics", buffslot, buffs[buffslot].ticsremaining);
		buffs[buffslot].melee_rune = 0;
		buffs[buffslot].magic_rune = 0;
	}

	if(IsNPC())
	{
		std::vector<std::any> args;
		args.push_back(&buffslot);
		int i = parse->EventSpell(EVENT_SPELL_EFFECT_NPC, CastToNPC(), nullptr, spell_id, caster ? caster->GetID() : 0, &args);
		if(i != 0){
			CalcBonuses();
			return true;
		}
	}
	else if(IsClient())
	{
		std::vector<std::any> args;
		args.push_back(&buffslot);
		int i = parse->EventSpell(EVENT_SPELL_EFFECT_CLIENT, nullptr, CastToClient(), spell_id, caster ? caster->GetID() : 0, &args);
		if(i != 0){
			CalcBonuses();
			return true;
		}
	}
		
	CalcSpellBonuses();

	if (spell_id == SPELL_FRENZIED_BURNOUT && caster && caster->IsClient())
	{
		const AA_DBAction* caa = &AA_Actions[aaFrenziedBurnout][0];
		caster->CastToClient()->EnableAAEffect(aaEffectFrenziedBurnout, caa->duration);
	}

	// reversed tap spell
	bool is_tap_recourse = (spells[spell_id].targettype == ST_TargetAETap || spells[spell_id].targettype == ST_Tap) && caster == this;

	// iterate through the effects in the spell
	for (i = 0; i < EFFECT_COUNT; i++)
	{
		if(!IsValidSpell(spell_id))
			return false;

		if(IsBlankSpellEffect(spell_id, i))
			continue;

		effect = spell.effectid[i];
		effect_value = CalcSpellEffectValue(spell_id, i, caster_level, 0, caster ? caster->GetInstrumentMod(spell_id) : 10);

		if(spell_id == SPELL_LAY_ON_HANDS && caster && caster->GetAA(aaImprovedLayOnHands))
			effect_value = GetMaxHP();
		if (spell_id == SPELL_LEECH_TOUCH && caster && caster->GetAA(aaConsumptionoftheSoul))
			effect_value -= 200 * caster->GetAA(aaConsumptionoftheSoul);

#ifdef SPELL_EFFECT_SPAM
		effect_desc[0] = 0;
#endif

		switch(effect)
		{
			case SE_CurrentHP:	// nukes, heals; also regen/dot if a buff
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Current Hitpoints: %+i", effect_value);
#endif
				// SE_CurrentHP is calculated at first tick if its a dot/buff
				if (buffslot >= 0)
					break;

				// for offensive spells check if we have a spell rune on
				int32 dmg = effect_value;
				if(dmg < 0)
				{
					if (!PassCastRestriction(false, spells[spell_id].base2[i], true))
						break;

					// take partial damage into account
					dmg = (int32) (dmg * partial / 100);
					if (dmg == 0)
					{
						dmg = -1;		// there are no zero damage non-full resists
					}

					//handles AAs and what not...
					if(caster) {
						dmg = caster->GetActSpellDamage(spell_id, dmg, this);

						if (spell_id == SPELL_LIFE_DRAIN)	// Maestro AoE.  Spell data uses type 8, but it's supposed to heal him.  Sony may have hardcoded
							caster->HealDamage(-dmg);		// this because targettype 20 didn't exist back then and there's seemingly no other way to make it tap
					}

					dmg = -dmg;
					Damage(caster, dmg, spell_id, spell.skill, false, buffslot, false);
				}
				else if(dmg > 0) {
					//healing spell...

					if (!PassCastRestriction(false, spells[spell_id].base2[i], false))
						break;

					if(caster)
						dmg = caster->GetActSpellHealing(spell_id, dmg, this);

					HealDamage(dmg, caster);
				}

#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Current Hitpoints: %+i  actual: %+i", effect_value, dmg);
#endif
				break;
			}

			case SE_CurrentHPOnce:	// another type of heal/dmg; used in buffs and DoTs with an instant damage component.
			{						// crit AAs do not work on this
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Current Hitpoints Once: %+i", effect_value);
#endif

				int32 dmg = effect_value;
				if (spell_id == SPELL_MANABURN && caster)
				{
					dmg = -zone->random.Int(caster->GetMana() * 15 / 10, caster->GetMana() * 2);		// 150%-200%
					if (dmg < -9492)
						dmg = -9492;

					if (caster->IsClient())
						dmg = caster->CastToClient()->TryWizardInnateCrit(spell_id, dmg, 0, -9492);		// manaburn max hit is 9492, both crits and non-crits

					caster->SetMana(0);
				}
				else if (spell_id == 2755 && caster) //Lifeburn
				{
					dmg = -caster->GetHP();
					caster->SetHP(caster->GetMaxHP() * 2 / 10 + 1);
					caster->SendHPUpdate();
				}

				//do any AAs apply to these spells?
				if(dmg < 0) {
					dmg = (int32)(dmg * partial / 100);		// this can also partial hit
					Damage(caster, -dmg, spell_id, spell.skill, false, buffslot, false);
				} else {
					HealDamage(dmg, caster);
				}
				break;
			}

			case SE_PercentalHeal:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Percental Heal: %+i (%d%% max)", spell.max[i], effect_value);
#endif
				int32 val = GetMaxHP() * spell.base[i] / 100;

				//This effect can also do damage by percent.
				if (val < 0) {

					if (spell.max[i] && -val > spell.max[i])
						val = -spell.max[i];

					if (caster)
						val = caster->GetActSpellDamage(spell_id, val, this);

				}

				else
				{
					if (spell.max[i] && val > spell.max[i])
						val = spell.max[i];

					if(caster)
						val = caster->GetActSpellHealing(spell_id, val, this);
				}

				if (val < 0)
					Damage(caster, -val, spell_id, spell.skill, false, buffslot, false);
				else
					HealDamage(val, caster);

				break;
			}

			case SE_CompleteHeal:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Complete Heal");
#endif
				//make sure they are not allready affected by this...
				//I think that is the point of making this a buff.
				//this is in the wrong spot, it should be in the immune
				//section so the buff timer does not get refreshed!

				int i;
				bool inuse = false;
				int buff_count = GetMaxTotalSlots();
				for(i = 0; i < buff_count; i++) {
					if(buffs[i].spellid == spell_id && i != buffslot) {
						Message(Chat::White, "You must wait before you can be affected by this spell again.");
						inuse = true;
						break;
					}
				}
				if(inuse)
					break;

				int32 val = 0;
				val = 7500*effect_value;
				val = caster->GetActSpellHealing(spell_id, val, this);

				if (val > 0)
					HealDamage(val, caster);

				break;
			}

			case SE_CurrentMana:
			{
				// Bards don't get mana from effects, good or bad.
				if(GetClass() == BARD)
					break;

				// from client decompile, these effects are less effective on NPCs above level 52
				if (spells[spell_id].buffdurationformula == 0 && IsNPC())
				{
					if (GetLevel() > 52)
					{
						if (GetLevel() > 54)
						{
							effect_value /= 3;
							if (effect_value < -105)
							{
								effect_value = -105;
							}
						}
						else
						{
							effect_value /= 2;
						}
					}
				}

				if(spells[spell_id].manatapspell) {
					if(GetCasterClass() != 'N') {
#ifdef SPELL_EFFECT_SPAM
						snprintf(effect_desc, _EDLEN, "Current Mana: %+i", effect_value);
#endif
						SetMana(GetMana() + effect_value);
						caster->SetMana(caster->GetMana() + std::abs(effect_value));

#ifdef SPELL_EFFECT_SPAM
						caster->Message(Chat::White, "You have gained %+i mana!", effect_value);
#endif
					}
				}
				else {
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Current Mana: %+i", effect_value);
#endif
				if (buffslot >= 0)
					break;

				SetMana(GetMana() + effect_value);

				}

				break;
			}

			case SE_Translocate:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Translocate: %s %d %d %d heading %d",
					spell.teleport_zone, spell.base[1], spell.base[0],
					spell.base[2], spell.base[3]
				);
#endif
				if(IsClient())
				{
					if (caster == this)
					{
						Message_StringID(Chat::SpellFailure, CANNOT_TRANSLOCATE_SELF);
						break;
					}

					if(caster)
						CastToClient()->SendOPTranslocateConfirm(caster, spell_id);
				}
				break;
			}

			case SE_Succor:
			{				
				if(IsNPC())
					break;

				float x, y, z, heading;
				const char *target_zone = nullptr;
				
				x = static_cast<float>(spell.base[1]);
				y = static_cast<float>(spell.base[0]);
				z = static_cast<float>(spell.base[2]);
				heading = static_cast<float>(spell.base[3]);

				if(!strcmp(spell.teleport_zone, "same"))
					target_zone = 0;
				else
					target_zone = spell.teleport_zone;

				if(IsClient())
				{
					// Below are the spellid's for known evac/succor spells that send player
					// to the current zone's safe points.

					// Succor = 1567
					// Lesser Succor = 2183
					// Evacuate = 1628
					// Lesser Evacuate = 2184
					// Decession = 2558
					// Greater Decession = 3244
					// Egress = 1566

					if(!target_zone) {
#ifdef SPELL_EFFECT_SPAM
						Log(Logs::General, Logs::Spells, "Succor/Evacuation Spell In Same Zone.");
#endif
						if (IsClient())
							CastToClient()->MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), x, y, z, heading, 0, EvacToSafeCoords);
						else
							GMMove(x, y, z, heading);
					}
					else {
#ifdef SPELL_EFFECT_SPAM
						Log(Logs::General, Logs::Spells, "Succor/Evacuation Spell To Another Zone.");
#endif
						if (IsClient())
						{
							uint32 zoneid = database.GetZoneID(target_zone);
							zone->ApplyRandomLoc(zoneid, x, y);
							if (zoneid == zone->GetZoneID()) {
								CastToClient()->MovePCGuildID(zoneid, zone->GetGuildID(), x, y, z, heading);
							}
							else {
								CastToClient()->zone_mode = ZoneSolicited;
								CastToClient()->m_ZoneSummonLocation = glm::vec4(x, y, z, heading);
								CastToClient()->zonesummon_id = zoneid;
								CastToClient()->zonesummon_guildid = GUILD_NONE;
								CastToClient()->zonesummon_ignorerestrictions = 0;
								SetHeading(heading);
								CastToClient()->zoning_timer.Start();
							}
						}
					}
				}

				break;
			}
			case SE_Teleport:	// gates, rings, circles, etc
			case SE_Teleport2:
			{
				if (IsNPC())
					break;

				float x, y, z, heading;
				const char *target_zone = nullptr;

				x = static_cast<float>(spell.base[1]);
				y = static_cast<float>(spell.base[0]);
				z = static_cast<float>(spell.base[2]);
				heading = static_cast<float>(spell.base[3]);

				if(!strcmp(spell.teleport_zone, "same"))
					target_zone = 0;
				else
					target_zone = spell.teleport_zone;

				if (target_zone != 0 && strcmp(zone->GetShortName(), target_zone) != 0)
				{
					Mob* mypet = GetPet();
					if (mypet) {
						if (mypet->IsCharmedPet())
							FadePetCharmBuff();
						SetPet(0);
					}
				}

#ifdef SPELL_EFFECT_SPAM
				const char *efstr = "Teleport";
				if(effect == SE_Teleport)
					efstr = "Teleport v1";
				else if(effect == SE_Teleport2)
					efstr = "Teleport v2";
				else if(effect == SE_Succor)
					efstr = "Succor";

				snprintf(effect_desc, _EDLEN,
					"%s: %0.2f, %0.2f, %0.2f heading %0.2f in %s",
					efstr, x, y, z, heading, target_zone ? target_zone : "same zone"
				);
#endif
				// teleports are not supposed to move NPCs.  Pets were used to kill the Seru earring NPCs, and Dain
				if(IsClient())
				{
					if(!target_zone)
						CastToClient()->MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), x, y, z, heading);
					else
					{
						uint32 zoneid = database.GetZoneID(target_zone);

						// This will only prevent the server side zone. Our client will often initiate the zoning process
						// as an unsoliciated request after the spell is cast. The server will then cancel that using SendZoneCancel().
						if (!CastToClient()->CanBeInZone(zoneid))
						{
							break;
						}
						zone->ApplyRandomLoc(zoneid, x, y);
						if (zoneid == zone->GetZoneID()) {
							CastToClient()->MovePCGuildID(zoneid, zone->GetGuildID(), x, y, z, heading);
						}
						else {
							CastToClient()->zone_mode = ZoneSolicited;
							CastToClient()->m_ZoneSummonLocation = glm::vec4(x, y, z, heading);
							CastToClient()->zonesummon_id = zoneid;
							CastToClient()->zonesummon_guildid = GUILD_NONE;
							CastToClient()->zonesummon_ignorerestrictions = 0;
							SetHeading(heading);
							CastToClient()->zoning_timer.Start();
						}
					}
				}
				break;
			}

			case SE_Invisibility:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Invisibility");
#endif
				SetInvisible(INVIS_NORMAL);
				break;
			}

			case SE_InvisVsAnimals:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Invisibility to Animals");
#endif
				invisible_animals = true;
				SetInvisible(INVIS_VSANIMAL);
				break;
			}

			case SE_InvisVsUndead:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Invisibility to Undead");
#endif
				invisible_undead = true;
				SetInvisible(INVIS_VSUNDEAD);
				break;
			}
			case SE_SeeInvis:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "See Invisible");
#endif
				if(IsClient())
					see_invis = spell.base[i];
				break;
			}

			case SE_AddFaction:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Faction Mod: %+i", effect_value);
#endif
				if(caster && GetPrimaryFaction()>0) 
				{
					caster->ClearFactionBonuses();
					caster->AddFactionBonus(GetPrimaryFaction(),effect_value);
				}
				else if (caster)
				{
					caster->Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT);
				}
				break;
			}

			case SE_Stun:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Stun: %d msec", effect_value);
#endif
				//Typically we check for immunities else where but since stun immunities are different and only
				//Block the stun part and not the whole spell, we do it here, also do the message here so we wont get the message on a resist
				int max_level = spell.max[i];

				//max_level of 0 means we assume a default of 55.
				if (max_level == 0)
					max_level = RuleI(Spells, BaseImmunityLevel);

				// stun level limits apply when landing on NPCs only
				if (IsClient() || (caster && caster->IsNPC()))
					max_level = 999;

				// Ignore if spell is beneficial (ex. Harvest)
				if (IsDetrimentalSpell(spell.id) && (GetSpecialAbility(UNSTUNABLE) || GetLevel() > max_level))
				{
					caster->Message_StringID(Chat::SpellFailure, IMMUNE_STUN);
				}
				else
				{
					if (caster->IsClient())
					{
						// cap player stuns on NPCs at 7.5 seconds
						if (effect_value > 7500 && IsNPC())
							effect_value = 7500;
					}
					Stun(effect_value, caster);
				}
				break;
			}

			case SE_Charm:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Charm: %+i (up to lvl %d)", effect_value, spell.max[i]);
#endif

				if (!caster)	// can't be someone's pet unless we know who that someone is
					break;
				if (buffslot < 0)
					break;

				if (IsNPC())
				{
					if (CastToNPC()->HasEngageNotice())
					{
						if (caster)
							caster->Message(Chat::Red, "That creature is too powerful for your charm.");
						break;
					}
				}

				if(IsDireCharmSpell(spell_id))
				{
					dire_charmed = true;
				}


				Mob *my_pet = GetPet();
				if(IsNPC())
				{
					zone->HasCharmedNPC = true;
					CastToNPC()->SaveGuardSpotCharm();
					CastToNPC()->SetNPCFactionID(0);
					CastToNPC()->SetAppearance(eaStanding);
					SetPetID(0);
					if (my_pet)
					{
						my_pet->SetPetType(petOrphan);
						my_pet->SetOwnerID(0);
					}
				}

				InterruptSpell();
				// only remove damage over time spells, to prevent faction wars
				entity_list.RemoveDotsFromNPCs(this);
				entity_list.RemoveFromNPCTargets(this);
				// charmed players can have hate lists.  So remove them also from their hatelist.
				entity_list.RemoveFromClientHateLists(this);
				WipeHateList(true);

				EndShield();

				caster->SetPet(this);
				SetOwnerID(caster->GetID());
				SetPetOrder(SPO_Follow);

				if (my_pet && IsClient())
				{
					if (my_pet->IsCharmedPet())
						my_pet->BuffFadeByEffect(SE_Charm);
					else
						my_pet->Kill();
				}

				if (IsFleeing() && !IsFearedNoFlee() && !IsBlind())
				{
					curfp = false;
					flee_mode = false;
				}

				if(caster->IsClient())
				{
					auto app = new EQApplicationPacket(OP_Charm, sizeof(Charm_Struct));
					Charm_Struct *ps = (Charm_Struct*)app->pBuffer;
					ps->owner_id = caster->GetID();
					ps->pet_id = this->GetID();
					ps->command = 1;
					entity_list.QueueClients(this, app);
					safe_delete(app);
					SendAppearancePacket(AppearanceType::Pet, caster->GetID(), true, true);
				}

				if (IsClient())
				{
					InterruptSpell();
					//SendAppearancePacket(AT_Anim, 100, true, true);
					if (this->CastToClient()->IsLD())
						CastToClient()->AI_Start();
					else
					{
						bool feared = FindType(SE_Fear);
						if (!feared)
							CastToClient()->AI_Start();
					}
					charmed = true;
					if(buffs[buffslot].ticsremaining > RuleI(Character, MaxCharmDurationForPlayerCharacter))
						buffs[buffslot].ticsremaining = RuleI(Character, MaxCharmDurationForPlayerCharacter);
				}
				if (caster->IsNPC())
				{
					AddToHateList(caster->GetHateRandom(), 20);
					AddToHateList(caster->GetHateTop(), 1);
				}

				break;
			}


			case SE_SenseDead:
			case SE_SenseSummoned:
			case SE_SenseAnimals:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Sense Target: %+i", effect_value);
#endif
				if(IsClient())
				{
					CastToClient()->SetSenseExemption(true);
				}
				break;
			}

			case SE_Fear:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Fear: %+i", effect_value);
#endif
				if (buffslot < 0)
					break;
				if(IsClient())
				{
					if(buffs[buffslot].ticsremaining > RuleI(Character, MaxFearDurationForPlayerCharacter))
						buffs[buffslot].ticsremaining = RuleI(Character, MaxFearDurationForPlayerCharacter);
				}

				if (IsNPC())
					InterruptSpell();
				

				if(RuleB(Combat, EnableFearPathing)){
					if(IsClient())
					{
						CastToClient()->AI_Start();
						feared = true;
					}
					if (!curfp)
						CalculateNewFearpoint();
				}
				else
				{
					Stun(buffs[buffslot].ticsremaining * 6000 - (6000 - tic_timer.GetRemainingTime()), caster);
				}
				break;
			}

			case SE_BindAffinity: //TO DO: Add support for secondary and tertiary gate abilities
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Bind Affinity");
#endif
				if (IsClient())
				{
					if(CastToClient()->GetGM() || RuleB(Character, BindAnywhere))
					{
						auto action_packet =
						    new EQApplicationPacket(OP_Action, sizeof(Action_Struct));
						Action_Struct* action = (Action_Struct*) action_packet->pBuffer;
						auto message_packet =
						    new EQApplicationPacket(OP_Damage, sizeof(Damage_Struct));
						Damage_Struct *cd = (Damage_Struct *)message_packet->pBuffer;

						action->target = GetID();
						action->source = caster ? caster->GetID() : GetID();
						action->level = 65;
						action->instrument_mod = 10;
						action->sequence = ((GetHeading() * 2.0f));
						action->type = 231;
						action->spell = spell_id;
						action->buff_unknown = 4;

						cd->target = action->target;
						cd->source = action->source;
						cd->type = action->type;
						cd->spellid = action->spell;
						cd->sequence = action->sequence;

						Log(Logs::Detail, Logs::Spells, "Sending Action #2/Message packet for spell %d (SE_BindAffinity)", spell_id);
						CastToClient()->QueuePacket(action_packet);
						if(caster->IsClient() && caster != this)
							caster->CastToClient()->QueuePacket(action_packet);

						CastToClient()->QueuePacket(message_packet);
						if(caster->IsClient() && caster != this)
							caster->CastToClient()->QueuePacket(message_packet);

						CastToClient()->SetBindPoint();
						Save();
						safe_delete(action_packet);
						safe_delete(message_packet);
					}
					else
					{
						if(!zone->CanBind() 
							|| (GetZoneID() == Zones::KAEL && !zone->IsBindArea(GetX(), GetY(), GetZ())) 
							|| (GetZoneID() == Zones::SKYSHRINE && !zone->IsBindArea(GetX(), GetY(), GetZ())))
						{
							//Nobody can bind here.
							Message_StringID(Chat::SpellFailure, CANNOT_BIND);
							break;
						}

						if(!zone->IsCity() && !zone->IsBindArea(GetX(),GetY(),GetZ()))
						{
							if(caster != this)
							{
								//Only the caster can bind here
								Message_StringID(Chat::SpellFailure, CANNOT_BIND);
								break;
							}
							else
							{
								auto action_packet = new EQApplicationPacket(
								    OP_Action, sizeof(Action_Struct));
								Action_Struct* action = (Action_Struct*) action_packet->pBuffer;
								auto message_packet = new EQApplicationPacket(
								    OP_Damage, sizeof(Damage_Struct));
								Damage_Struct *cd = (Damage_Struct *)message_packet->pBuffer;

								action->target = GetID();
								action->source = caster ? caster->GetID() : GetID();
								action->level = 65;
								action->instrument_mod = 10;
								action->sequence = (GetHeading() * 2.0f);
								action->type = 231;
								action->spell = spell_id;
								action->buff_unknown = 4;

								cd->target = action->target;
								cd->source = action->source;
								cd->type = action->type;
								cd->spellid = action->spell;
								cd->sequence = action->sequence;
								Log(Logs::Detail, Logs::Spells, "Sending Action #2/Message packet for spell %d (SE_BindAffinity)", spell_id);
								CastToClient()->QueuePacket(action_packet);
								if(caster->IsClient() && caster != this)
									caster->CastToClient()->QueuePacket(action_packet);

								CastToClient()->QueuePacket(message_packet);
								if(caster->IsClient() && caster != this)
									caster->CastToClient()->QueuePacket(message_packet);

								CastToClient()->SetBindPoint();
								Save();
								safe_delete(action_packet);
								safe_delete(message_packet);
							}
						}
						else
						{
							auto action_packet =
							    new EQApplicationPacket(OP_Action, sizeof(Action_Struct));
							Action_Struct* action = (Action_Struct*) action_packet->pBuffer;
							auto message_packet = new EQApplicationPacket(
							    OP_Damage, sizeof(Damage_Struct));
							Damage_Struct *cd = (Damage_Struct *)message_packet->pBuffer;

							action->target = GetID();
							action->source = caster ? caster->GetID() : GetID();
							action->level = 65;
							action->instrument_mod = 10;
							action->sequence = (GetHeading() * 2.0f);
							action->type = 231;
							action->spell = spell_id;
							action->buff_unknown = 4;

							cd->target = action->target;
							cd->source = action->source;
							cd->type = action->type;
							cd->spellid = action->spell;
							cd->sequence = action->sequence;


							Log(Logs::Detail, Logs::Spells, "Sending Action #2/Message packet for spell %d (SE_BindAffinity)", spell_id);
							CastToClient()->QueuePacket(action_packet);
							if(caster->IsClient() && caster != this)
								caster->CastToClient()->QueuePacket(action_packet);

							CastToClient()->QueuePacket(message_packet);
							if(caster->IsClient() && caster != this)
								caster->CastToClient()->QueuePacket(message_packet);

							CastToClient()->SetBindPoint();
							Save();
							safe_delete(action_packet);
							safe_delete(message_packet);
						}
					}
				}
				break;
			}

			case SE_Gate:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Gate");
#endif
				if (!spellbonuses.AntiGate) {
					if (!IsClient()) {
						Gate();
					}
				}
				break;
			}

			case SE_CancelMagic:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Cancel Magic: %d", effect_value);
#endif
				if(GetSpecialAbility(UNDISPELLABLE)){
					caster->Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT, spells[spell_id].name);
					break;
				}

				int buff_count = GetMaxBuffSlots();
				for(int slot = 0; slot < buff_count; slot++) {
					if(	buffs[slot].spellid != SPELL_UNKNOWN &&
						IsDispellableSpell(buffs[slot].spellid))
					{
						if (TryDispel(caster->GetLevel(),buffs[slot].casterlevel, effect_value)){
							if (effect_value == 0) {
								if (buffs[slot].ticsremaining > 2) {
									// taper magic, reduces buff time remaining
									buffs[slot].ticsremaining /= 2;
									buffs[slot].ticsremaining += 1;
									buffs[slot].UpdateClient = true;
								}
							}
							else {
								BuffFadeBySlot(slot);
							}
							break;
						}
					}
				}
				break;
			}

			case SE_DispelDetrimental:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Dispel Detrimental: %d", effect_value);
#endif
				if(GetSpecialAbility(UNDISPELLABLE)){
					caster->Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT, spells[spell_id].name);
					break;
				}

				int buff_count = GetMaxBuffSlots();
				for(int slot = 0; slot < buff_count; slot++) {
					if (buffs[slot].spellid != SPELL_UNKNOWN &&
						IsDetrimentalSpell(buffs[slot].spellid) &&
						IsDispellableSpell(buffs[slot].spellid))
					{
						if (TryDispel(caster->GetLevel(),buffs[slot].casterlevel, effect_value)){
							BuffFadeBySlot(slot);
							slot = buff_count;
						}
					}
				}
				break;
			}

			case SE_DispelBeneficial:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Dispel Beneficial: %d", effect_value);
#endif
				if(GetSpecialAbility(UNDISPELLABLE)){
					caster->Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT, spells[spell_id].name);
					break;
				}

				int buff_count = GetMaxBuffSlots();
				for(int slot = 0; slot < buff_count; slot++) {
					if (buffs[slot].spellid != SPELL_UNKNOWN &&
						IsBeneficialSpell(buffs[slot].spellid) &&
						IsDispellableSpell(buffs[slot].spellid))
					{
						if (TryDispel(caster->GetLevel(),buffs[slot].casterlevel, effect_value)){
							BuffFadeBySlot(slot);
							slot = buff_count;
						}
					}
				}
				break;
			}

			case SE_Purify: // this effect doesn't exist in TAKP era
			{
				//Attempt to remove all Deterimental buffs.
				int buff_count = GetMaxBuffSlots();
				for(int slot = 0; slot < buff_count; slot++) {
					if (buffs[slot].spellid != SPELL_UNKNOWN &&
						IsDetrimentalSpell(buffs[slot].spellid))
					{
						if (TryDispel(caster->GetLevel(),buffs[slot].casterlevel, effect_value)){
							BuffFadeBySlot(slot);
						}
					}
				}
				break;
			}

			case SE_Mez:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Mesmerize");
#endif
				if (IsNPC())
				{
					if (CastToNPC()->HasEngageNotice())
					{
						if (caster)
							caster->Message(Chat::Red, "That creature is too powerful for your magic.");
						break;
					}
				}
				Mesmerize();
				break;
			}

			case SE_SummonItem:
			{
				if (IsClient() && spell.base[i] > 0) {
					if (!RuleB(Quarm, CursorAllowDuplicateItems)) {
						// we already have this item on the cursor - so stop sending it here.  Client will just delete it.
						int16 inv_slot_id = CastToClient()->GetInv().HasItem(spell.base[i], 1, invWhereCursor);
						if (inv_slot_id != INVALID_INDEX) {
							break;
						}
					}
				}
				const EQ::ItemData *item = database.GetItem(spell.base[i]);
#ifdef SPELL_EFFECT_SPAM
				const char *itemname = item ? item->Name : "*Unknown Item*";
				snprintf(effect_desc, _EDLEN, "Summon Item: %s (id %d)", itemname, spell.base[i]);
#endif
				if (!item) {
					Message(Chat::Red, "Unable to summon item %d. Item not found.", spell.base[i]);
				} else if (IsClient()) {
					Client *c = CastToClient();
					if (c->CheckLoreConflict(item)) {
						c->DuplicateLoreMessage(spell.base[i]);
					} else {

						int max = spell.max[i];
						if(max == 0)
							max = 20;

						int quantity = 0;
						if(database.ItemQuantityType(spell.base[i]) == EQ::item::Quantity_Charges) {
							quantity = item->MaxCharges;
						} else if(database.ItemQuantityType(spell.base[i]) == EQ::item::Quantity_Normal) {
							quantity = 1;
						} else if(spell.formula[i] > 0 && spell.formula[i] <= 20) {
							quantity = spell.formula[i];
						} else {
							quantity = CalcSpellEffectValue_formula(spell.formula[i],0,item->Stackable ? item->StackSize : item->MaxCharges,GetLevel(),spell_id);
							if (quantity < max)
								quantity = max;
						}

						if (quantity < 1)
							quantity = 1;

						if (SummonedItem) {
							c->SummonItem(SummonedItem->GetID(), SummonedItem->GetCharges());
							safe_delete(SummonedItem);
						}
						SummonedItem = database.CreateItem(spell.base[i], quantity);
					}
				} else if (IsPet()) {
					int max = spell.max[i];
					if(max == 0)
						max = 20;

					int quantity = 0;
					if(database.ItemQuantityType(spell.base[i]) == EQ::item::Quantity_Charges) {
						quantity = item->MaxCharges;
					} else if(database.ItemQuantityType(spell.base[i]) == EQ::item::Quantity_Normal) {
						quantity = 1;
					} else if(spell.formula[i] > 0 && spell.formula[i] <= 20) {
						quantity = spell.formula[i];
					} else {
						quantity = CalcSpellEffectValue_formula(spell.formula[i],0,item->Stackable ? item->StackSize : item->MaxCharges,GetLevel(),spell_id);
						if (quantity < max)
							quantity = max;
					}

					if (quantity < 1)
						quantity = 1;

					// Destroy duplicate items on charmed pets and nodrop items.
					if (item->NoDrop != 0 && (!IsCharmedPet() || (IsCharmedPet() && CastToNPC()->CountQuestItem(item->ID) == 0)))
					{
						// For pets, all items are added to inventory, even if the
						// item is summoned into a bag so that the pet can equip
						// the item, if possible.
						CastToNPC()->AddPetLoot(item->ID, quantity);
					}
				} else if (caster) {
					caster->Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT);
				}

				break;
			}
			case SE_SummonItemIntoBag:
			{
				const EQ::ItemData *item = database.GetItem(spell.base[i]);
#ifdef SPELL_EFFECT_SPAM
				const char *itemname = item ? item->Name : "*Unknown Item*";
				snprintf(effect_desc, _EDLEN, "Summon Item In Bag: %s (id %d)", itemname, spell.base[i]);
#endif
				uint8 slot;

				if (IsPet() && item) {
					int charges;

					if (item->Stackable)
						charges = (spell.formula[i] > item->StackSize) ? item->StackSize : spell.formula[i];
					else if (item->MaxCharges) // mod rods, not sure if there are actual examples of this for IntoBag
						charges = item->MaxCharges;
					else
						charges = 1;

					if (charges < 1)
						charges = 1;

					// Destroy duplicate items on charmed pets and nodrop items.
					if (item->NoDrop != 0 && (!IsCharmedPet() || (IsCharmedPet() && CastToNPC()->CountQuestItem(item->ID) == 0)))
					{
						CastToNPC()->AddPetLoot(item->ID, charges);
					}
				} else
				if (!SummonedItem || !SummonedItem->IsClassBag()) {
					if (caster)
						caster->Message(Chat::Red, "SE_SummonItemIntoBag but no bag has been summoned!");
				} else if ((slot = SummonedItem->FirstOpenSlot()) == 0xff) {
					if (caster)
						caster->Message(Chat::Red, "SE_SummonItemIntoBag but no room in summoned bag!");
				} else if (IsClient()) {
					if (CastToClient()->CheckLoreConflict(item)) {
						CastToClient()->DuplicateLoreMessage(spell.base[i]);
					} else {
						int charges;

						if (item->Stackable)
							charges = (spell.formula[i] > item->StackSize) ? item->StackSize : spell.formula[i];
						else if (item->MaxCharges) // mod rods, not sure if there are actual examples of this for IntoBag
							charges = item->MaxCharges;
						else
							charges = 1;

						if (charges < 1)
							charges = 1;

						EQ::ItemInstance *SubItem = database.CreateItem(spell.base[i], charges);
						if (SubItem != nullptr) {
							SummonedItem->PutItem(slot, *SubItem);
							safe_delete(SubItem);
						}
					}
				} else if (caster) {
					caster->Message_StringID(Chat::SpellFailure, SPELL_NO_EFFECT);
				}

				break;
			}

			case SE_SummonBSTPet:
			case SE_NecPet:
			case SE_SummonPet:
			case SE_Familiar:
			{
				if (!caster)
					break;

				if (caster->GetHP() < 0)
				{
					break;
				}

#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Summon %s: %s", (effect==SE_Familiar)?"Familiar":"Pet", spell.teleport_zone);
#endif
				if(GetPet() || entity_list.GetSummonedPetID(this))
				{
					Message_StringID(Chat::SpellFailure, ONLY_ONE_PET);
				}
				else
				{
                    //Message(Chat::Red, "MakePet");
					MakePet(spell_id, spell.teleport_zone);
				}

				entity_list.AddHealAggro(this, this, 10);		// making pets adds a small amount of hate, like casting a buff
				break;
			}
			case SE_DivineAura:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Invulnerability");
#endif
				if (buffslot < 0)
					break;
				// out of era for AK
				//if(spell_id==4789) // Touch of the Divine - Divine Save
				//	buffs[buffslot].ticsremaining = spells[spell_id].buffduration; // Prevent focus/aa buff extension

				// DA spells seemed to have reduced hate on AK by an unknown amount, but likely not all of it.  Half is a wild guess
				if (RuleB(AlKabor, InvulnHateReduction))
					entity_list.HalveAggro(this);

				SetInvul(true);
				break;
			}

			case SE_ShadowStep:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Shadow Step: %d", effect_value);
#endif
				if(IsNPC() && (!IsPet() || (IsCharmedPet() && GetPetOrder() != SPO_Guard)))	// see Song of Highsun - sends mob home
				{
					if (IsNPC())
					{
						if (CastToNPC()->HasEngageNotice())
						{
							if (caster)
								caster->Message(Chat::Red, "You can't seem to send this target home... they're already there.");
							break;
						}
					}
					Gate();
				}
				// shadow step is handled by client already, nothing required
				break;
			}

			case SE_Blind:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Blind: %+i", effect_value);
#endif
				// this should catch the cures
				if (IsBeneficialSpell(spell_id) && spells[spell_id].buffduration == 0)
					BuffFadeByEffect(SE_Blind);
				else if (!IsClient() && !curfp)
				{
					if (!GetTarget())
						SetTarget(caster); // have to set a target so blind can set a fear point properly
					CalculateNewFearpoint();
				}
				break;
			}

			case SE_Rune:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Melee Absorb Rune: %+i", effect_value);
#endif
				if (buffslot < 0)
					break;
				buffs[buffslot].melee_rune = effect_value;
				break;
			}

			case SE_AbsorbMagicAtt:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Spell Absorb Rune: %+i", effect_value);
#endif
				if (buffslot < 0)
					break;
				if(effect_value > 0) 
					buffs[buffslot].magic_rune = effect_value;

				break;
			}
			
			case SE_Levitate:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Levitate");
#endif
				//this sends the levitate packet to everybody else
				//who does not otherwise receive the buff packet.
				if(IsClient())
					SendAppearancePacket(AppearanceType::FlyMode, 2, true, false);
				break;
			}

			case SE_DeathSave: 
			{

				if (buffslot < 0)
					break;;
				int16 mod = 0;
				
				if(caster) {
					mod =	caster->aabonuses.UnfailingDivinity +
							caster->itembonuses.UnfailingDivinity +
							caster->spellbonuses.UnfailingDivinity;
				}
 				
				buffs[buffslot].ExtraDIChance = mod;
  				break;
 			}

			case SE_Illusion:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Illusion: race %d", effect_value);
#endif
				if (buffslot < 0)
					break;
				ApplyIllusion(spell, i, caster);

				for (int x = EQ::textures::textureBegin; x <= EQ::textures::LastTintableTexture; x++)
					SendWearChange(x, nullptr, false, false, true);
				
				if (caster == this &&
				    (spellbonuses.IllusionPersistence || aabonuses.IllusionPersistence ||
				     itembonuses.IllusionPersistence) && (spell_id != SPELL_MINOR_ILLUSION && spell_id != SPELL_ILLUSION_TREE)) // exclude these for permanent illusion AA
					buffs[buffslot].persistant_buff = 1;
				else
					buffs[buffslot].persistant_buff = 0;
				break;
			}

			case SE_IllusionCopy:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Illusion Copy");
#endif
				if(caster && caster->GetTarget()){
						SendIllusionPacket
						(
							caster->GetTarget()->GetRace(),
							caster->GetTarget()->GetGender(),
							caster->GetTarget()->GetTexture(),
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
							caster->GetTarget()->GetSize()
						);

						if(!IsPlayableRace(caster->GetTarget()->GetRace()))
						{
							uint32 newsize = floor(caster->GetTarget()->GetSize() + 0.5);
							caster->SendAppearancePacket(AppearanceType::Size, newsize);
						}

						for(int x = EQ::textures::textureBegin; x <= EQ::textures::LastTintableTexture; x++)
							caster->SendWearChange(x, nullptr, false, false, true);
				}
				break;
			}

			case SE_WipeHateList:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Memory Blur: %d", effect_value);
#endif

				if (IsMezSpell(spell_id))
				{
					if (current_buff_refresh)
					{
						Log(Logs::General, Logs::Spells, "Spell %d cast on %s is a mez the entity is already debuffed with. Skipping Mem Blur component.", spell_id, GetName());
						break;
					}
				}

				if (IsNPC())
				{
					if (CastToNPC()->HasEngageNotice())
					{
						if(caster)
						caster->Message(Chat::Red, "You can't seem to wipe the mind of this target...");
						break;
					}
				}

				int wipechance = spells[spell_id].base[i];
				int bonus = 0;

				if (GetLevel() < 17)
				{
					wipechance = 100;
				}
				else
				{
					if (caster) {
						bonus = caster->spellbonuses.IncreaseChanceMemwipe +
							caster->itembonuses.IncreaseChanceMemwipe +
							caster->aabonuses.IncreaseChanceMemwipe;
					}

					wipechance += wipechance*bonus / 100;
					if (GetLevel() >= 17 && GetLevel() < 53)
					{
						wipechance += -2.08333 * GetLevel() + 135.4167;
					}
					else
					{
						wipechance += 25;
					}

					uint8 cha_bonus = 0;
					if(caster && caster->GetCHA() > 150)
						cha_bonus = (caster->GetCHA() - 150) / 10;

					wipechance += cha_bonus > 15 ? 15 : cha_bonus;

					if (wipechance > 100)
						wipechance = 100;

				}

				if(zone->random.Roll(wipechance))
				{
					if(IsAIControlled())
					{
						WipeHateList(true);
					}
					Message(Chat::Red, "Your mind fogs. Who are my friends? Who are my enemies?... it was all so clear a moment ago...");
				}
				break;
			}

			case SE_SpinTarget:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Spin: %d", effect_value);
#endif
				// the spinning is handled by the client
				int max_level = spells[spell_id].max[i];
				if(max_level == 0)
					max_level = RuleI(Spells, BaseImmunityLevel); // Default max is 55 level limit

				// NPCs ignore level limits in their spells
				if(GetSpecialAbility(UNSTUNABLE) ||
					(GetLevel() > max_level && caster && caster->IsClient() && IsNPC()))
				{
					caster->Message_StringID(Chat::SpellFailure, IMMUNE_STUN);
				}
				else
				{
					// the spinning is handled by the client
					// Stun duration is based on the effect_value, not the buff duration(alot don't have buffs)
					// AK had 7500 milisecond spinstuns.  Confirmed in logs.  The effect values should be 7500
					Stun(effect_value, caster);
					if(!IsClient()) {
						Spin();
						spun_timer.Start(100); // spins alittle every 100 ms
						spun_resist_timer.Start(1000);
					}
				}
				break;
			}

			case SE_EyeOfZomm:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Eye of Zomm");
#endif
				if(caster && caster->IsClient()) {
					char eye_name[64];
					snprintf(eye_name, sizeof(eye_name), "Eye_of_%s", caster->GetCleanName());
					int duration = CalcBuffDuration(caster, this, spell_id) * 6;
					caster->TemporaryPets(spell_id, nullptr, eye_name, duration, false);
					caster->CastToClient()->has_zomm = true;
				}
				break;
			}

			case SE_ReclaimPet:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Reclaim Pet");
#endif
				if
				(
					IsNPC() &&
					GetOwnerID() &&		// I'm a pet
					caster &&					// there's a caster
					caster->GetID() == GetOwnerID()	&& // and it's my master
					caster->GetID() == GetSummonerID() && // I made it
					GetPetType() != petCharmed
				)
				{
				uint16 pet_spellid =  CastToNPC()->GetPetSpellID();
				uint16 pet_ActSpellCost = caster->GetActSpellCost(pet_spellid, spells[pet_spellid].mana);

				// Manifest of Elements
				if (pet_spellid == 1936)
					pet_ActSpellCost = 500;

				int16 ImprovedReclaimMod =	caster->spellbonuses.ImprovedReclaimEnergy + 
											caster->itembonuses.ImprovedReclaimEnergy + 
											caster->aabonuses.ImprovedReclaimEnergy;

				if (ImprovedReclaimMod == 0)
					ImprovedReclaimMod = 75;

				int16 pet_HPMod = GetHPRatio() > ImprovedReclaimMod ? ImprovedReclaimMod : GetHPRatio();
				uint16 pet_ActSpellCostMod = pet_ActSpellCost*pet_HPMod /100;
				if (GetPetType() == petHatelist || GetPetType() == petFamiliar)
					pet_ActSpellCostMod = 0;

				Log(Logs::General, Logs::Spells, "Reclaimed %d mana from pet %s. ReclaimMod: %d", pet_ActSpellCostMod, GetName(), ImprovedReclaimMod);
				caster->SetMana(caster->GetMana() + pet_ActSpellCostMod);

				if(caster->IsClient())
					caster->CastToClient()->SetPet(0);

				SetOwnerID(0);	// this will kill the pet

				}
				break;
			}

			case SE_BindSight:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Bind Sight");
#endif
				if(caster && caster->IsClient())
				{
					caster->CastToClient()->SetBindSightTarget(this);
				}
				break;
			}

			case SE_FeignDeath:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Feign Death");
#endif
				//todo, look up spell ID in DB
				if(spell_id == 2488) //Dook- Lifeburn fix
					break;

				uint8 chance = spells[spell_id].base[i];
				if (chance == 1)
					chance = 87;

				// Death Peace
				if (spell_id == 1460)
					chance = 98;

				if(IsClient()) {
					if (zone->random.Int(1, 100) > chance) {
						CastToClient()->SetFeigned(false);
						entity_list.MessageClose_StringID(this, false, 200, 10, STRING_FEIGNFAILED, GetName());
					}
					else
					{
						// NPC casted spells with FD and stun effects in them should not break the FD when auto attacking (e.g. Tactical Strike)
						// NPC casted spells with no stun compoent should work sometimes (e.g. Wind Strike)
						if (CastToClient()->auto_attack && (!caster->IsNPC() || (!IsStunSpell(spell_id) && zone->random.Roll(50))))
						{
							CastToClient()->SetFeigned(false);
						}
						else
						{
							CastToClient()->SetFeigned(true);
						}
					}
				}
				break;
			}

			case SE_Sentinel:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Sentinel");
#endif

				break;
			}

			case SE_LocateCorpse:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Locate Corpse");
#endif

				break;
			}

			case SE_Revive:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Revive");	// heh the corpse won't see this
#endif
				if (IsCorpse() && CastToCorpse()->IsPlayerCorpse()) {

					if(caster)
						Log(Logs::Detail, Logs::Spells, " corpse being rezzed using spell %i by %s",
							spell_id, caster->GetName());

					CastToCorpse()->CastRezz(spell_id, caster);
				}
				break;
			}

			case SE_ModelSize:
			case SE_ChangeHeight:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Model Size: %d%%", effect_value);
#endif


				float modifyAmount = (static_cast<float>(effect_value) / 100.0f);
				if (zone && zone->GetGuildID() == GUILD_NONE)
				{
					if (zone->last_quake_struct.quake_type != QuakeType::QuakeDisabled)
					{
						if (modifyAmount > 1.0f)
						{
							break;
						}
					}
				}

				float gnome_height = GetPlayerHeight(GNOME);
				float ogre_height = GetPlayerHeight(OGRE);
				float size = GetSize();

				// Do not allow Shrink if Gnome size or smaller, or if size 20 or higher. 
				if(modifyAmount < 1.0f && size > gnome_height && (size < 20.0f || IsSummonedClientPet()))
				{
					if (size == ogre_height)
						size = 8.0f;
					else if (size == 6.0f || size == 5.5f)
						size = 5.0f;

					float newsize = floor((size * modifyAmount) - 0.5f);
					if(newsize >= gnome_height)
					{
						Log(Logs::General, Logs::Spells, "Shrink successful from %0.2f to %0.2f. modifyAmount %0.2f", size, newsize, modifyAmount);
						ChangeSize(newsize, true);
					}
					else if (newsize < gnome_height)
					{
						Log(Logs::General, Logs::Spells, "Shrink successful from %0.2f to %0.2f. modifyAmount %0.2f", size, gnome_height, modifyAmount);
						ChangeSize(gnome_height, true);
					}

				}
				else if(modifyAmount > 1.0f && size < ogre_height)
				{
					float newsize = floor((size * modifyAmount) + 0.5f);
					if(newsize <= ogre_height)
					{
						Log(Logs::General, Logs::Spells, "Growth successful from %0.2f to %0.2f. modifyAmount %0.2f", size, newsize, modifyAmount);
						ChangeSize(newsize, true);
					}
					else if (newsize > ogre_height)
					{
						Log(Logs::General, Logs::Spells, "Growth successful from %0.2f to %0.2f. modifyAmount %0.2f", size, ogre_height, modifyAmount);
						ChangeSize(ogre_height, true);
					}
				}

				break;
			}

			case SE_Root:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Root: %+i", effect_value);
#endif
				if (buffslot < 0)
					break;
				rooted = true;

				if (caster){
					buffs[buffslot].RootBreakChance = caster->aabonuses.RootBreakChance + 
													caster->itembonuses.RootBreakChance +
													caster->spellbonuses.RootBreakChance;
				}

				if (IsNPC() && flee_mode)
				{
					CheckEnrage();
				}

				break;
			}

			case SE_SummonHorse:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Summon Mount: %s", spell.teleport_zone);
#endif
				if(IsClient())	// NPCs can't ride
				{
					CastToClient()->SummonHorse(spell_id);
				}


				break;
			}

			case SE_SummonCorpse:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Summon Corpse: %d", effect_value);
#endif
				// can only summon corpses of clients
				if(!IsNPC()) {
					Client* TargetClient = nullptr;
					if(this->GetTarget())
						TargetClient = this->GetTarget()->CastToClient();
					else
						TargetClient = this->CastToClient();

					// Now we should either be casting this on self or its being cast on a valid group member
					if(TargetClient) {

						if (TargetClient->GetLevel() <= effect_value){

							Corpse *corpse = entity_list.GetCorpseByOwner(TargetClient);
							if(corpse) {
								if(TargetClient == this->CastToClient())
									caster->Message_StringID(Chat::Spells, SUMMONING_CORPSE, TargetClient->CastToMob()->GetCleanName());
								else
									caster->Message_StringID(Chat::Spells, SUMMONING_CORPSE_OTHER, TargetClient->CastToMob()->GetCleanName());

								corpse->Summon(CastToClient(), true, false);
							}
							else {
								// No corpse found in the zone
								caster->Message_StringID(Chat::Spells, CORPSE_CANT_SENSE);
								this->CastToClient()->SummonItem(spells[spell_id].components[0]);
							}
						}
						else
						{
							caster->Message_StringID(Chat::SpellFailure, SPELL_LEVEL_REQ);
							this->CastToClient()->SummonItem(spells[spell_id].components[0]);
						}
					}
					else {
						caster->Message_StringID(Chat::SpellFailure, TARGET_NOT_FOUND);
						this->CastToClient()->SummonItem(spells[spell_id].components[0]);
						Log(Logs::General, Logs::Error, "%s attempted to cast spell id %u with spell effect SE_SummonCorpse, but could not cast target into a Client object.", GetCleanName(), spell_id);
					}
				}

				break;
			}
			case SE_WeaponProc:
			{
				uint16 procid = GetProcID(spell_id, i);
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Weapon Proc: %s (id %d)", spells[effect_value].name, procid);
#endif
				int procmod = 0;
				switch (spell_id) {
					// these are proc mods for bst pets
					case 2635:
					case 2636:
					case 2637:
					case 2638:
					case 2639:
					case 2640:
					case 2641:
					case 2888:
					case 2890:
					case 3459:
						procmod = 275;
						break;
					default:
						procmod = spells[spell_id].base2[i];
				}

				AddProcToWeapon(procid, 100+procmod, spell_id);
				break;
			}

			case SE_Lull:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Lull");
#endif
				// TODO: check vs. CHA when harmony effect failed, if caster is to be added to hatelist
				break;
			}

			case SE_PoisonCounter:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Poison Counter: %+i", effect_value);
#endif
				if (effect_value < 0)
				{
					int buff_count = GetMaxTotalSlots();
					for (int j=0; j < buff_count; j++) {
						if (!IsValidSpell(buffs[j].spellid))
							continue;
						if (CalculatePoisonCounters(buffs[j].spellid) == 0)
							continue;
						// solar: there is an intentional weirdness here - the variable 'i' here is the slot number of the counter effect in the cure spell, but it's also used to index the buff spell.
						// it should probably use the index of the counter effect in the buff spell but this is how sony did it.
						if (effect_value + CalcSpellEffectValue(buffs[j].spellid, i, buffs[j].casterlevel, 0, buffs[j].instrumentmod) <= 6)
						{
							buffs[j].counters += effect_value;
							if (buffs[j].counters <= 0)
							{
								if (caster)
									caster->Message_StringID(Chat::Spells, TARGET_CURED);
								BuffFadeBySlot(j);
							}
						}
					}
				}
				break;
			}

			case SE_DiseaseCounter:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Disease Counter: %+i", effect_value);
#endif
				if (effect_value < 0)
				{
					int buff_count = GetMaxTotalSlots();
					for (int j=0; j < buff_count; j++) {
						if (!IsValidSpell(buffs[j].spellid))
							continue;
						if (CalculateDiseaseCounters(buffs[j].spellid) == 0)
							continue;
						// solar: there is an intentional weirdness here - the variable 'i' here is the slot number of the counter effect in the cure spell, but it's also used to index the buff spell.
						// it should probably use the index of the counter effect in the buff spell but this is how sony did it.
						if (effect_value + CalcSpellEffectValue(buffs[j].spellid, i, buffs[j].casterlevel, 0, buffs[j].instrumentmod) <= 6)
						{
							buffs[j].counters += effect_value;
							if (buffs[j].counters <= 0)
							{
								if (caster)
									caster->Message_StringID(Chat::Spells, TARGET_CURED);
								BuffFadeBySlot(j);
							}
						}
					}
				}
				break;
			}

			case SE_CurseCounter:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Curse Counter: %+i", effect_value);
#endif
				if (effect_value < 0)
				{
					int buff_count = GetMaxTotalSlots();
					for (int j=0; j < buff_count; j++) {
						if (!IsValidSpell(buffs[j].spellid))
							continue;
						if (CalculateCurseCounters(buffs[j].spellid) == 0)
							continue;
						// solar: there is an intentional weirdness here - the variable 'i' here is the slot number of the counter effect in the cure spell, but it's also used to index the buff spell.
						// it should probably use the index of the counter effect in the buff spell but this is how sony did it.
						if (effect_value + CalcSpellEffectValue(buffs[j].spellid, i, buffs[j].casterlevel, 0, buffs[j].instrumentmod) <= 6 
							|| (buffs[j].spellid == SPELL_EPOCH_CONVICTION && spell_id == SPELL_REMOVE_GREATER_CURSE)) // TAKP customization for Quarm spell Epoch Conviction
						{
							buffs[j].counters += effect_value;
							if (buffs[j].counters <= 0)
							{
								if (caster)
									caster->Message_StringID(Chat::Spells, TARGET_CURED);
								BuffFadeBySlot(j);
							}
						}
					}
				}
				break;
			}

			case SE_Destroy:
			{
				if(IsNPC()) {
					if(GetLevel() <= 52) {
						CastToNPC()->Depop(true);

						std::string export_string = fmt::format(
							"{} {} {} {}",
							caster ? caster->GetID() : 0,
							0,
							spell_id,
							static_cast<int>(EQ::skills::SkillTigerClaw)
						);
						parse->EventNPC(EVENT_DEATH, this->CastToNPC(), caster, export_string, 0);
					}
					else
					{
						caster->Message(Chat::SpellFailure, "Your target is too high level to be affected by this spell.");
					}
				}
				break;
			}

			case SE_TossUp:
			{
				// This is only used in 4 spells and those spells also have push components which are taken care of by the spell's OP_Action already.

				break;
			}

			case SE_StopRain:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Stop Rain");
#endif
				zone->zone_weather = 0;
				zone->weather_intensity = 0;
				zone->weatherSend();
				break;
			}

			case SE_Sacrifice:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Sacrifice");
#endif
				if(!IsClient() || !caster->IsClient()){
					break;
				}
				CastToClient()->SacrificeConfirm(caster->CastToClient());
				break;
			}

			case SE_SummonPC:
			{
				if (IsClient())
				{
					if (GetPet())
					{
						if (!GetPet()->IsCharmedPet())
						{
							DepopPet();
						}
						else
						{
							FadePetCharmBuff();
						}
					}

					glm::vec3 caster_pos(caster->GetX(), caster->GetY(), caster->GetZ());
					glm::vec3 pos(GetX(), GetY(), GetZ());
					auto diff = pos - caster_pos;
					float curdist = diff.x * diff.x + diff.y * diff.y;

					if (curdist > 10000)
					{
						entity_list.ClearAggro(this);
						if (IsClient())
							CastToClient()->scanarea_timer.Reset(); // prevent mobs from immediately reaggroing before player is actually moved
					}
					else if (caster != this)
					{
						entity_list.AddHealAggro(this, caster, CheckHealAggroAmount(spell_id, this, (GetMaxHP() - GetHP())));
					}

					CastToClient()->MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), caster->GetX(), caster->GetY(), caster->GetZ(), caster->GetHeading(), 2, SummonPC);
				}
				else
					caster->Message(Chat::Red, "This spell can only be cast on players.");

				break;
			}

			case SE_Silence:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Silence");
#endif
				Silence(true);
				break;
			}

			case SE_Amnesia:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Amnesia");
#endif
				Amnesia(true);
				break;
			}

			case SE_CallPet:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Call Pet");
#endif
				// this is cast on self, not on the pet
				if(GetPet() && GetPet()->IsNPC() && !GetPet()->IsCharmedPet())
				{
					GetPet()->CastToNPC()->GMMove(GetX(), GetY(), GetZ(), GetHeading());
				}
				break;
			}

			case SE_StackingCommand_Block:
			case SE_StackingCommand_Overwrite:
			{
				// these are special effects used by the buff stuff
				break;
			}


			case SE_TemporaryPets: //Dook- swarms and wards:
			{
				char pet_name[64];
				snprintf(pet_name, sizeof(pet_name), "%s`s_pet", caster->GetCleanName());
				caster->TemporaryPets(spell_id, this, pet_name);
				break;
			}

			case SE_FadingMemories:		//Dook- escape etc
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Fading Memories");
#endif
				if(zone->random.Roll(spells[spell_id].base[i])) {

					if(caster && caster->IsClient())
						caster->CastToClient()->Escape();
					else
					{
						entity_list.RemoveFromNPCTargets(caster);
						SetInvisible(INVIS_NORMAL);
					}
				}
				break;
			}

			case SE_WakeTheDead:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Wake The Dead");
#endif
				//meh dupe issue with npc casting this
				if(caster->IsClient()){
					int dur = spells[spell_id].max[i];
					if (!dur)
						dur = 60;

					//caster->WakeTheDead(spell_id, caster->GetTarget(), dur);
				}
				break;
			}

			case SE_Doppelganger:
			{
				if(caster && caster->IsClient()) {
					char pet_name[64];
					snprintf(pet_name, sizeof(pet_name), "%s`s doppelganger", caster->GetCleanName());
					int pet_count = spells[spell_id].base[i];
					int pet_duration = spells[spell_id].max[i];
					caster->CastToClient()->Doppelganger(spell_id, this, pet_name, pet_count, pet_duration);
				}
				break;
			}

			case SE_BalanceHP: {
				if(!caster)
					break;

				if(!caster->IsClient())
					break;

				Raid *r = entity_list.GetRaidByClient(caster->CastToClient());
				if(r)
				{
					uint32 gid = r->GetGroup(caster->GetName());
					if(gid < 11)
					{
						r->BalanceHP(spell.base[i], gid, spell.range, caster, spell.base2[i]);
						break;
					}
				}

				Group *g = entity_list.GetGroupByClient(caster->CastToClient());

				if(!g)
					break;

				g->BalanceHP(spell.base[i], spell.range, caster, spell.base2[i]);
				break;
			}

			case SE_BalanceMana: {
				if (!caster)
					break;

				if (!caster->IsClient())
					break;

				Raid *r = entity_list.GetRaidByClient(caster->CastToClient());
				if (r)
				{
					uint32 gid = r->GetGroup(caster->GetName());
					if (gid < 11)
					{
						r->BalanceMana(spell.base[i], gid, spell.range, caster, spell.base2[i]);
						break;
					}
				}

				Group *g = entity_list.GetGroupByClient(caster->CastToClient());

				if (!g)
					break;

				g->BalanceMana(spell.base[i], spell.range, caster, spell.base2[i]);
				break;
			}

			case SE_SuspendMinion:
			case SE_SuspendPet:
			{
				if(IsClient())
					CastToClient()->SuspendMinion();

				break;
			}

			case SE_VoiceGraft:
			{
				if(caster && caster->GetPet())
					caster->spellbonuses.VoiceGraft = caster->GetPetID();

				break;
			}

			case SE_AttackSpeed:
				if (spell.base[i] < 100)
					SlowMitigation(caster);
				break;

			case SE_AttackSpeed2:
				if (spell.base[i] < 100)
					SlowMitigation(caster);
				break;

			case SE_AttackSpeed3:
				if (spell.base[i] < 0)
					SlowMitigation(caster);
				break;

			case SE_MassGroupBuff:{

				SetMGB(true);
				Message_StringID(Chat::Disciplines, MGB_STRING);
				break;
			}

			case SE_IllusionOther: {
				SetProjectIllusion(true);
				Message_StringID(Chat::Spells, PROJECT_ILLUSION);
				break;
			}

			case SE_MovementSpeed: {
				if (IsNPC() && IsSpeedBuff(spell_id) && RuleB(NPC, CheckSoWBuff))
					SetRunning(true);

				break;
			}
			
			case SE_ChangeFrenzyRad:
			case SE_Harmony:
			{
				break;
			}

			case SE_Stamina:
			{
				if (buffslot >= 0)
					break;

				if (IsClient())
				{
					CastToClient()->SetFatigue(CastToClient()->GetFatigue() + effect_value);
				}
				break;
			}

			// Handled Elsewhere
			case SE_ReduceReuseTimer:
			case SE_ExtraAttackChance:
			case SE_ProcChance:
			case SE_StunResist:
			case SE_MinDamageModifier:
			case SE_DamageModifier:
			case SE_IncreaseArchery:
			case SE_HitChance:
			case SE_MeleeSkillCheck:
			case SE_HundredHands:
			case SE_ResistFearChance:
			case SE_ResistSpellChance:
			case SE_AllInstrumentMod:
			case SE_MeleeLifetap:
			case SE_DoubleAttackChance:
			case SE_DualWieldChance:
			case SE_ParryChance:
			case SE_DodgeChance:
			case SE_RiposteChance:
			case SE_AvoidMeleeChance:
			case SE_CrippBlowChance:
			case SE_CriticalHitChance:
			case SE_MeleeMitigation:
			case SE_Reflect:
			case SE_Screech:
			case SE_Amplification:
			case SE_MagicWeapon:
			case SE_Hunger:
			case SE_MagnifyVision:
			case SE_Lycanthropy:
			case SE_NegateIfCombat:
			case SE_CastingLevel:
			case SE_CastingLevel2:
			case SE_RaiseStatCap:
			case SE_ResistAll:
			case SE_ResistMagic:
			case SE_ResistDisease:
			case SE_ResistPoison:
			case SE_ResistCold:
			case SE_ResistFire:
			case SE_AllStats:
			case SE_CHA:
			case SE_WIS:
			case SE_INT:
			case SE_STA:
			case SE_AGI:
			case SE_DEX:
			case SE_STR:
			case SE_ATK:
			case SE_ArmorClass:
			case SE_EndurancePool:
			case SE_UltraVision:
			case SE_InfraVision:
			case SE_ManaPool:
			case SE_TotalHP:
			case SE_ChangeAggro:
			case SE_Identify:
			case SE_InstantHate:
			case SE_SpellDamageShield:
			case SE_ReverseDS:
			case SE_DamageShield:
			case SE_TrueNorth:
			case SE_WaterBreathing:
			case SE_HealOverTime:
			case SE_DivineSave:
			case SE_Accuracy:
			case SE_Flurry:
			case SE_ImprovedDamage:
			case SE_ImprovedHeal:
			case SE_IncreaseSpellHaste:
			case SE_IncreaseSpellDuration:
			case SE_IncreaseRange:
			case SE_SpellHateMod:
			case SE_ReduceReagentCost:
			case SE_ReduceManaCost:
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
			case SE_LimitCombatSkills:
			case SE_SpellDurationIncByTic:
			case SE_HealRate:
			case SE_SkillDamageTaken:
			case SE_FcSpellVulnerability:
			case SE_CastOnFadeEffect:
			case SE_MaxHPChange:
			case SE_FcDamageAmt:
			case SE_CriticalSpellChance:
			case SE_SpellCritChance:
			case SE_SpellCritDmgIncrease:
			case SE_CriticalHealChance:
			case SE_CriticalDoTChance:
			case SE_LimitSpellGroup:
			case SE_ReduceSkillTimer:
			case SE_SkillDamageAmount:
			case SE_IncreaseBlockChance:
			case SE_AntiGate:
			case SE_Fearless:
			case SE_FcDamageAmtCrit:
			case SE_SpellProcChance:
			case SE_CharmBreakChance:
			case SE_BardSongRange:
			case SE_FcDamagePctCrit:
			case SE_FcDamageAmtIncoming:
			case SE_MitigateDamageShield:
			case SE_BlockBehind:
			case SE_ShieldBlock:
			case SE_PetCriticalHit:
			case SE_SlayUndead:
			case SE_GiveDoubleAttack:
			case SE_StrikeThrough:
			case SE_StrikeThrough2:
			case SE_SecondaryDmgInc:
			case SE_ArcheryDamageModifier:
			case SE_ConsumeProjectile:
			case SE_FrontalBackstabChance:
			case SE_FrontalBackstabMinDmg:
			case SE_TripleBackstab:
			case SE_DoubleSpecialAttack:
			case SE_IncreaseRunSpeedCap:
			case SE_BaseMovementSpeed:
			case SE_FrontalStunResist:
			case SE_ImprovedBindWound:
			case SE_MaxBindWound:
			case SE_CombatStability:
			case SE_AddSingingMod:
			case SE_SongModCap:
			case SE_HeadShot:
			case SE_HeadShotLevel:
			case SE_PetAvoidance:
			case SE_GiveDoubleRiposte:
			case SE_Ambidexterity:
			case SE_PetMaxHP:
			case SE_PetFlurry:
			case SE_MasteryofPast:
			case SE_GivePetGroupTarget:
			case SE_RootBreakChance:
			case SE_UnfailingDivinity:
			case SE_ChannelChanceSpells:
			case SE_FcStunTimeMod:
			case SE_StunBashChance:
			case SE_IncreaseChanceMemwipe:
			case SE_CriticalMend:
			case SE_LimitCastTimeMax:
			case SE_FrenziedDevastation:
			case SE_DoubleRiposte:
			case SE_Berserk:
			case SE_Vampirism:
			case SE_Metabolism:
			case SE_FinishingBlow:
			case SE_FinishingBlowLvl:
			case SE_AETaunt:
			case SE_SkillAttack:			// Used after PoP
			{
				break;
			}

			default:
			{
#ifdef SPELL_EFFECT_SPAM
				snprintf(effect_desc, _EDLEN, "Unknown Effect ID %d", effect);
#else
				Message(Chat::White, "Unknown spell effect %d in spell %s (id %d)", effect, spell.name, spell_id);
#endif
			}
		}
#ifdef SPELL_EFFECT_SPAM
		Message(Chat::White, ". . . Effect #%i: %s", i + 1, (effect_desc && effect_desc[0]) ? effect_desc : "Unknown");
#endif
	}

	CalcBonuses();

	if (SummonedItem) {
		Client *c=CastToClient();
		c->PushItemOnCursorWithoutQueue(SummonedItem);
		safe_delete(SummonedItem);
	}

	current_buff_refresh = false;

	return true;
}

int Mob::CalcSpellEffectValue(uint16 spell_id, int effect_index, int caster_level, int ticsremaining, int instrumentmod)
{
	int formula, base, max, effect_value;

	if
	(
		!IsValidSpell(spell_id) ||
		effect_index < 0 ||
		effect_index >= EFFECT_COUNT ||
		IsBlankSpellEffect(spell_id, effect_index)
	)
		return 0;

	formula = spells[spell_id].formula[effect_index];
	base = spells[spell_id].base[effect_index];
	max = spells[spell_id].max[effect_index];

	if (RuleB(Spells, JamFestAAOnlyAffectsBard))
	{
		if (GetClass() == BARD && IsBardSong(spell_id))
		{
			// this bonus (effective_casting_level) is only used in the Jam Fest AA
			caster_level += itembonuses.effective_casting_level + spellbonuses.effective_casting_level + aabonuses.effective_casting_level;
		}
	}

	effect_value = CalcSpellEffectValue_formula(formula, base, max, caster_level, spell_id, ticsremaining);

	// apply bard instrument mod to bard songs
	if(spells[spell_id].bardsong && IsInstrumentModdableSpellEffect(spell_id, effect_index))
	{
		int oval = effect_value;
		int mod = instrumentmod;
		effect_value = effect_value * mod / 10;
		Log(Logs::Detail, Logs::Spells, "Effect value %d altered with bard modifier of %d to yield %d", oval, mod, effect_value);
	}

	return(effect_value);
}

// generic formula calculations
int Mob::CalcSpellEffectValue_formula(int formula, int base, int max, int caster_level, uint16 spell_id, int ticsremaining)
{
/*
i need those formulas checked!!!!

0 = base
1 - 99 = base + level * formulaID
100 = base
101 = base + level / 2
102 = base + level
103 = base + level * 2
104 = base + level * 3
105 = base + level * 4
106 ? base + level * 5
107 ? min + level / 2
108 = min + level / 3
109 = min + level / 4
110 = min + level / 5
119 ? min + level / 8
121 ? min + level / 4
122 = splurt
123 ?
203 = stacking issues ? max
205 = stacking issues ? 105


0x77 = min + level / 8
*/

	int result = 0, updownsign = 1, ubase = base;
	if(ubase < 0)
		ubase = 0 - ubase;

	// this updown thing might look messed up but if you look at the
	// spells it actually looks like some have a positive base and max where
	// the max is actually less than the base, hence they grow downward
/*
This seems to mainly catch spells where both base and max are negative.
Strangely, damage spells have a negative base and positive max, but
snare has both of them negative, yet their range should work the same:
(meaning they both start at a negative value and the value gets lower)
*/
	if (max < base && max != 0)
	{
		// values are calculated down
		updownsign = -1;
	}
	else
	{
		// values are calculated up
		updownsign = 1;
	}
	#if EQDEBUG >= 11
		Log(Logs::Detail, Logs::Spells, "CSEV: spell %d, formula %d, base %d, max %d, lvl %d. Up/Down %d",
			spell_id, formula, base, max, caster_level, updownsign);
	#endif

	switch(formula)
	{
		// 60 and 70 are used only in magician air pet stuns
		// This was making the stuns last only 30 miliseconds
		// commenting this out so it uses the default case, which works better.  No idea what it should be
		//case 60:
		//case 70:
		//	result = ubase / 100; break;
		case 0:
		case 100:	// confirmed 2/6/04
			result = ubase; break;
		case 101:	// confirmed 2/6/04
			result = updownsign * (ubase + (caster_level / 2)); break;
		case 102:	// confirmed 2/6/04
			result = updownsign * (ubase + caster_level); break;
		case 103:	// confirmed 2/6/04
			result = updownsign * (ubase + (caster_level * 2)); break;
		case 104:	// confirmed 2/6/04
			result = updownsign * (ubase + (caster_level * 3)); break;
		case 105:	// confirmed 2/6/04
			result = updownsign * (ubase + (caster_level * 4)); break;

		case 107:
		{
			// Client duration extension focus effects are disabled for spells that use this formula
			if (ticsremaining > 0)
			{
				int ticdif = CalcBuffDuration_formula(caster_level, spells[spell_id].buffdurationformula, spells[spell_id].buffduration) - (ticsremaining - 1);
				if (ticdif < 0)
					ticdif = 0;

				result = updownsign * (ubase - ticdif);
			}
			else
			{
				result = updownsign * ubase;
			}
			break;
		}
		case 108:
		{
			// Client duration extension focus effects are disabled for spells that use this formula
			if (ticsremaining > 0)
			{
				int ticdif = CalcBuffDuration_formula(caster_level, spells[spell_id].buffdurationformula, spells[spell_id].buffduration) - (ticsremaining - 1);
				if (ticdif < 0)
					ticdif = 0;

				result = updownsign * (ubase - (2 * ticdif));
			}
			else
			{
				result = updownsign * ubase;
			}
			break;
		}
		case 109:	// confirmed 2/6/04
			result = updownsign * (ubase + (caster_level / 4)); break;
		case 110:
			result = updownsign * (ubase + (caster_level / 6)); break;
		case 111:
			result = updownsign * (ubase + 6 * (caster_level - 16));
			break;
		case 112:
			result = updownsign * (ubase + 8 * (caster_level - 24));
			break;
		case 113:
			result = updownsign * (ubase + 10 * (caster_level - 34));
			break;
		case 114:
			result = updownsign * (ubase + 15 * (caster_level - 44));
			break;

		case 115:	// this is only in symbol of transal
			result = ubase;
			if (caster_level > 15)
				result += 7 * (caster_level - 15);
			break;
		case 116:	// this is only in symbol of ryltan
			result = ubase;
			if (caster_level > 24)
				result += 10 * (caster_level - 24);
			break;
		case 117:	// this is only in symbol of pinzarn
			result = ubase;
			if (caster_level > 34)
				result += 13 * (caster_level - 34);
			break;
		case 118:	// used in naltron and a few others
			result = ubase;
			if (caster_level > 44)
				result += 20 * (caster_level - 44);
			break;

		case 119:	// confirmed 2/6/04
			result = ubase + (caster_level / 8); break;
		case 120:
		{
			// Client duration extension focus effects are disabled for spells that use this formula
			if (ticsremaining > 0)
			{
				int ticdif = CalcBuffDuration_formula(caster_level, spells[spell_id].buffdurationformula, spells[spell_id].buffduration) - (ticsremaining - 1);
				if (ticdif < 0)
					ticdif = 0;

				result = updownsign * (ubase - (5 * ticdif));
			}
			else
			{
				result = updownsign * ubase;
			}
			break;
		}
		case 121:	// corrected 2/6/04
			result = ubase + (caster_level / 3); break;
		case 122:
		{
			// Client duration extension focus effects are disabled for spells that use this formula
			if (ticsremaining > 0)
			{
				int ticdif = CalcBuffDuration_formula(caster_level, spells[spell_id].buffdurationformula, spells[spell_id].buffduration) - (ticsremaining - 1);
				if (ticdif < 0)
					ticdif = 0;

				result = updownsign * (ubase - (12 * ticdif));
			}
			else
			{
				result = updownsign * ubase;
			}
			break;
		}
		case 123:	// added 2/6/04
			result = zone->random.Int(ubase, std::abs(max));
			break;

		case 124:	// check sign
			result = ubase;
			if (caster_level > 50)
				result += updownsign * (caster_level - 50);
			break;

		case 125:	// check sign
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 2 * (caster_level - 50);
			break;

		case 126:	// check sign
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 3 * (caster_level - 50);
			break;

		case 127:	// check sign
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 4 * (caster_level - 50);
			break;

		case 128:	// MentalCorruptionRecourse
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 5 * (caster_level - 50);
			break;

		case 129:	// Used in LoY era spell Frozen Harpoon
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 10 * (caster_level - 50);
			break;

		case 130:	// check sign
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 15 * (caster_level - 50);
			break;

		case 131:	// check sign
			result = ubase;
			if (caster_level > 50)
				result += updownsign * 20 * (caster_level - 50);
			break;

		case 150: //resistant discipline (custom formula)
			result = caster_level > 50 ? 10 : caster_level > 45 ? 5 + caster_level - 45 : caster_level > 40 ? 5 : caster_level > 34 ? 4 : 3;
			break;

		//these are used in stacking effects... formula unknown
		case 201:
		case 202:
		case 203:
		case 204:
		case 205:
			result = max;
			break;
		default:
		{
			if (formula < 100)
			{
				if (spell_id == SPELL_HARM_TOUCH2)		// Unholy Aura disc HT.  This has formula = 14 but it's supposed to multiply all HTs by 1.5, including
					formula = 10;						// Unholy Touch AA's added damage.  SPELL_HARM_TOUCH has formula 10.  Doing the mult elsewhere

				result = ubase + (caster_level * formula);
				
				// Sony hardcoded a HT damage bonus
				if (spell_id == SPELL_HARM_TOUCH || spell_id == SPELL_HARM_TOUCH2 || spell_id == SPELL_IMP_HARM_TOUCH)
				{
					if (caster_level > 40)		// HT damage starts increasing by 30 per level at level 41
					{
						int htBonus = 20 * caster_level - 40;
						if (htBonus > 400)		// scale goes back to +10 per level at level 60
							htBonus = 400;
						result += htBonus;
					}
				}
			}
			else
				Log(Logs::General, Logs::Error, "Unknown spell effect value formula %d", formula);
		}
	}

	int oresult = result;

	// now check result against the allowed maximum
	if (max != 0)
	{
		if (updownsign == 1)
		{
			if (result > max)
				result = max;
		}
		else
		{
			if (result < max)
				result = max;
		}
	}

	// if base is less than zero, then the result need to be negative too
	if (base < 0 && result > 0)
		result *= -1;
	#if EQDEBUG >= 11
		Log(Logs::Detail, Logs::Spells, "Result: %d (orig %d), cap %d %s", result, oresult, max, (base < 0 && result > 0)?"Inverted due to negative base":"");
	#endif

	return result;
}


void Mob::BuffProcess()
{
	int buff_count = GetMaxTotalSlots();

	for (int buffs_i = 0; buffs_i < buff_count; ++buffs_i)
	{
		if (buffs[buffs_i].spellid != SPELL_UNKNOWN)
		{
			// this is a flag that removes the buff on the next BuffProcess() run, used to work around a client lockup issue for fear/charm spells
			if (buffs[buffs_i].remove_me)
			{
				buffs[buffs_i].remove_me = false;
				BuffFadeBySlot(buffs_i);
				continue;
			}

			DoBuffTic(buffs[buffs_i].spellid, buffs_i, buffs[buffs_i].ticsremaining, buffs[buffs_i].casterlevel, entity_list.GetMob(buffs[buffs_i].casterid), buffs[buffs_i].instrumentmod);
			// If the Mob died during DoBuffTic, then the buff we are currently processing will have been removed
			if(buffs[buffs_i].spellid == SPELL_UNKNOWN)
				continue;

			if(spells[buffs[buffs_i].spellid].buffdurationformula != DF_Permanent)
			{
				if(!zone->BuffTimersSuspended() || !IsSuspendableSpell(buffs[buffs_i].spellid) || !IsBeneficialSpell(buffs[buffs_i].spellid))
				{
					--buffs[buffs_i].ticsremaining;

					if (buffs[buffs_i].ticsremaining == 0) {
						if (!IsShortDurationBuff(buffs[buffs_i].spellid) ||
							IsFearSpell(buffs[buffs_i].spellid) ||
							IsCharmSpell(buffs[buffs_i].spellid) ||
							IsMezSpell(buffs[buffs_i].spellid) ||
							IsBlindSpell(buffs[buffs_i].spellid))
						{
							Log(Logs::Detail, Logs::Spells, "Buff %d in slot %d has expired. Fading.", buffs[buffs_i].spellid, buffs_i);
							BuffFadeBySlot(buffs_i);
						}
					}
					else if (buffs[buffs_i].ticsremaining < 0)
					{
						Log(Logs::Detail, Logs::Spells, "Buff %d in slot %d has expired. Fading.", buffs[buffs_i].spellid, buffs_i);
						BuffFadeBySlot(buffs_i);
					}
					else
					{
						Log(Logs::Detail, Logs::Spells, "Buff %d in slot %d has %d tics remaining.", buffs[buffs_i].spellid, buffs_i, buffs[buffs_i].ticsremaining);
					}
				}
				else if(IsClient())
				{
					buffs[buffs_i].UpdateClient = true;
				}
			}

			if (buffs[buffs_i].spellid != SPELL_UNKNOWN && IsSplurtFormulaSpell(buffs[buffs_i].spellid))
			{
				CalcBonuses();
			}

			if(buffs[buffs_i].spellid != SPELL_UNKNOWN && IsClient())
			{
				if(buffs[buffs_i].UpdateClient == true)
				{
					CastToClient()->SendBuffDurationPacket(buffs[buffs_i].spellid, buffs[buffs_i].ticsremaining, buffs[buffs_i].casterlevel, buffs_i, buffs[buffs_i].instrumentmod);
					buffs[buffs_i].UpdateClient = false;
				}
			}
		}
	}
	for (int buffs_i = 0; buffs_i < buff_count; ++buffs_i)
	{
		if (buffs[buffs_i].spellid != SPELL_UNKNOWN)
			buffs[buffs_i].first_tic = false;
	}
}

void Mob::DoBuffTic(uint16 spell_id, int slot, uint32 ticsremaining, uint8 caster_level, Mob* caster, int instrumentmod) {
	int effect, effect_value;

	if(!IsValidSpell(spell_id))
		return;

	const SPDat_Spell_Struct &spell = spells[spell_id];

	if (spell_id == SPELL_UNKNOWN)
		return;

	if(IsNPC())
	{
		std::vector<std::any> args;
		args.push_back(&ticsremaining);
		args.push_back(&caster_level);
		args.push_back(&slot);
		int i = parse->EventSpell(EVENT_SPELL_BUFF_TIC_NPC, CastToNPC(), nullptr, spell_id, caster ? caster->GetID() : 0, &args);
		if(i != 0) {
			return;
		}
	}
	else
	{
		std::vector<std::any> args;
		args.push_back(&ticsremaining);
		args.push_back(&caster_level);
		args.push_back(&slot);
		int i = parse->EventSpell(EVENT_SPELL_BUFF_TIC_CLIENT, nullptr, CastToClient(), spell_id, caster ? caster->GetID() : 0, &args);
		if(i != 0) {
			return;
		}
	}

	// Yaulp V, Yaulp VI - these have a mana regen component and are lost when sitting
	if (spell.disallow_sit && IsClient())
	{
		Client *client = this->CastToClient();
		if (client->IsSitting() || client->GetHorseId() != 0)
		{
			BuffFadeBySlot(slot);
			return;
		}
	}

	// reversed tap spell
	bool is_tap_recourse = (spells[spell_id].targettype == ST_TargetAETap || spells[spell_id].targettype == ST_Tap) && caster == this;

	for (int i = 0; i < EFFECT_COUNT; i++)
	{
		if(IsBlankSpellEffect(spell_id, i))
			continue;

		effect = spell.effectid[i];
		//I copied the calculation into each case which needed it instead of
		//doing it every time up here, since most buff effects dont need it

		switch(effect)
		{
			case SE_CurrentHP:
			{
				effect_value = CalcSpellEffectValue(spell_id, i, caster_level, ticsremaining, instrumentmod);
				if (is_tap_recourse) effect_value = -effect_value;
				int hate_amount = effect_value;
				//Handle client cast DOTs here.
				if (caster && caster->IsClient() && IsDetrimentalSpell(spell_id) && effect_value < 0) {

					effect_value = caster->CastToClient()->GetActDoTDamage(spell_id, effect_value, this);

					if (!caster->CastToClient()->IsFeigned()
						&& (!GetOwner() || !GetOwner()->IsClient())
					)
						AddToHateList(caster, -hate_amount);
				}

				if(effect_value < 0)
				{
					if(caster)
					{
						if(!caster->IsClient()){

							if (!IsClient() && !GetOwner()) //Allow NPC's to generate hate if casted on other NPC's.
								AddToHateList(caster, -hate_amount);
						}

						if(caster->IsNPC())
							effect_value = caster->CastToNPC()->GetActSpellDamage(spell_id, effect_value, this);

					}

					effect_value = -effect_value;
					Log(Logs::Detail, Logs::Spells, "%s is being damaged for %d points due to DOT %s in slot %d.", GetName(), effect_value, GetSpellName(spell_id), slot);
					Damage(caster, effect_value, spell_id, spell.skill, false, i, true);
				} else if(effect_value > 0) {
					// Regen spell...
					// handled with bonuses
				}
				break;
			}
			case SE_HealOverTime:
			{
				effect_value = CalcSpellEffectValue(spell_id, i, caster_level, ticsremaining, instrumentmod);
				if(caster)
					effect_value = caster->GetActSpellHealing(spell_id, effect_value, nullptr, true);

				HealDamage(effect_value, caster, spell_id, true);
				//healing aggro would go here; removed for now
				break;
			}

			case SE_Stamina:
			{
				if (IsClient())
				{
					effect_value = CalcSpellEffectValue(spell_id, i, caster_level, ticsremaining, instrumentmod);
					CastToClient()->SetFatigue(CastToClient()->GetFatigue() + effect_value);
				}
				break;
			}

			case SE_WipeHateList:
			{
				if (IsMezSpell(spell_id))
					break;

				int wipechance = spells[spell_id].base[i];
				int bonus = 0;

				if (caster){
					bonus =	caster->spellbonuses.IncreaseChanceMemwipe +
							caster->itembonuses.IncreaseChanceMemwipe +
							caster->aabonuses.IncreaseChanceMemwipe;
				}

				wipechance += wipechance*bonus/100;

				if(zone->random.Roll(wipechance))
				{
					if(IsAIControlled())
					{
						WipeHateList(true);
					}
					Message(Chat::Red, "Your mind fogs. Who are my friends? Who are my enemies?... it was all so clear a moment ago...");
				}
				break;
			}

			case SE_Charm: {

				// See http://www.eqemulator.org/forums/showthread.php?t=43370

				bool breakCharm = false;
				
				if (!caster)
					breakCharm = true;

				if (spells[spell_id].ResistDiff > -600 && zone->random.Int(0, 99) < 50)	// let Dictate hold; Preliminary roll for charm is 50%
				{
					float resist_check = CheckResistSpell(spells[spell_id].resisttype, spell_id, caster, this, false, 0, true);

					if (resist_check != 100.0f)
					{
						if (caster->IsClient())
						{
							if (zone->random.Int(1, 100) > caster->aabonuses.CharmBreakChance)	// Total Domination AA
								breakCharm = true;
							else
								Log(Logs::Detail, Logs::Spells, "Total Domination success; charm will hold");
						}
						else
							breakCharm = true;
					}
				}

				if (breakCharm)
				{
					// this is a workaround for a client lockup that can happen if the saving throw occurs immediately after landing
					if (IsClient() && buffs[slot].first_tic)
					{
						// this is checked in BuffProcess() and schedules the buff to be removed on the next tic.
						buffs[slot].remove_me = true;
					}
					else
					{
						BuffFadeByEffect(SE_Charm);
					}
				}

				break;
			}

			case SE_Lull: {

				/* Lulls have a chance to end early.  Chance is not affected by MR or charisma.
				   On Live, fade chance per tick was about 2% per tick on white cons, 7% on a +5
				  a red con, 0% on a -5 blue, and 1% on a -1 blue.
				*/
				int fadeChance = GetLevel() - caster_level + 2;

				if (zone->random.Roll(fadeChance))
				{
					BuffFadeBySlot(slot);
				}
				break;
			}

			case SE_Root: {

				if (zone->random.Roll(RuleI(Spells, RootBreakCheckChance)))
				{
					if (CheckResistSpell(spells[spell_id].resisttype, spell_id, caster, this, false, 0, true) != 100.0f)
						BuffFadeBySlot(slot);
				}

				break;
			}

			case SE_Blind: {
				if (zone->random.Roll(RuleI(Spells, BlindBreakCheckChance)))
				{
					if (CheckResistSpell(spells[spell_id].resisttype, spell_id, caster, this, false, 0, true) != 100.0f)
					{
						BuffFadeBySlot(slot);
						break;
					}
				}

				// if target is moving, then blind has a high chance to run toward target
				if (IsAIControlled() && GetTarget() && (GetTarget()->GetCurrentSpeed() > 0.01f || (GetTarget()->IsClient() && GetTarget()->animation != 0)) && !IsFeared())
					CalculateNewFearpoint();

				break;
			}

			case SE_Fear:
			{
				if (zone->random.Roll(RuleI(Spells, FearBreakCheckChance)))
				{
					if (CheckResistSpell(spells[spell_id].resisttype, spell_id, caster, this) != 100.0f)
					{
						// this is a workaround for a client lockup that can happen if the saving throw occurs immediately after landing
						if (IsClient() && buffs[slot].first_tic)
						{
							// this is checked in BuffProcess() and schedules the buff to be removed on the next tic.
							buffs[slot].remove_me = true;
						}
						else
						{
							BuffFadeBySlot(slot);
						}
					}
				}

				break;
			}

			case SE_Hunger: {
				break;
			}
			case SE_Invisibility:
			case SE_InvisVsAnimals:
			case SE_InvisVsUndead:
			{
				if(ticsremaining > 10)
				{
					//EQMac has no effect for fixed length invis :(
					if(!spells[spell_id].bardsong && !IsFixedDurationInvisSpell(spell_id))
					{
						double break_chance = 2.0;
						if(caster)
						{
							break_chance -= (2 * (((double)caster->GetSkill(EQ::skills::SkillDivination) + ((double)caster->GetLevel() * 3.0)) / 650.0));
						}
						else
						{
							break_chance -= (2 * (((double)GetSkill(EQ::skills::SkillDivination) + ((double)GetLevel() * 3.0)) / 650.0));
						}

						if(zone->random.Real(0.0, 100.0) < break_chance)
						{
							BuffModifyDurationBySpellID(spell_id, 10, true);
							Log(Logs::General, Logs::Spells, "Invis spell %d fading early. 10 tics remain.", spell_id);
						}
					}
				}
				else if (ticsremaining == 2)
				{
					Message_StringID(Chat::SpellFailure, INVIS_BEGIN_BREAK);
				}
				break;
			}
			// These effects always trigger when they fade.
			case SE_CastOnFadeEffect:
			{
				if (ticsremaining == 1)
				{
					SpellOnTarget(spells[spell_id].base[i], this);
				}
				break;
			}
			case SE_TotalHP:
			{
				if (spell.formula[i] > 1000 && spell.formula[i] < 1999)
				{
					// These formulas can affect Max HP each tick
					// Maybe there is a more efficient way to recalculate this for just Max HP each tic...
					//CalcBonuses();
					CalcSpellBonuses(&spellbonuses);
					CalcMaxHP();
				}
				break;
			}

			default:
			{
				// do we need to do anyting here?
			}
		}
	}
}

// removes the buff in the buff slot 'slot'
void Mob::BuffFadeBySlot(int slot, bool iRecalcBonuses, bool message, bool update)
{
	if(slot < 0 || slot > GetMaxTotalSlots())
		return;

	if(!IsValidSpell(buffs[slot].spellid))
		return;

	if (IsClient() && !CastToClient()->IsDead() && update)
		CastToClient()->MakeBuffFadePacket(buffs[slot].spellid, slot);

	bool isbuff = false;
	if (IsBeneficialSpell(buffs[slot].spellid))
		isbuff = true;

	if (buffs[slot].isdisc)
	{
		Log(Logs::General, Logs::Discs, "Fading disc spell %d from slot %d on %s", buffs[slot].spellid, slot, GetName());
	}
	else
	{
		Log(Logs::Detail, Logs::Spells, "Fading %s %d from slot %d on %s", isbuff ? "buff" : "debuff", buffs[slot].spellid, slot, GetName());
	}

	if(IsClient()) {
		std::vector<std::any> args;
		args.push_back(&buffs[slot].casterid);

		parse->EventSpell(EVENT_SPELL_FADE, nullptr, CastToClient(), buffs[slot].spellid, slot, &args);
	} else if(IsNPC()) {
		std::vector<std::any> args;
		args.push_back(&buffs[slot].casterid);

		parse->EventSpell(EVENT_SPELL_FADE, CastToNPC(), nullptr, buffs[slot].spellid, slot, &args);
	}

	bool was_mezzed = IsMezzed();

	for (int i=0; i < EFFECT_COUNT; i++)
	{
		if(IsBlankSpellEffect(buffs[slot].spellid, i))
			continue;

		switch (spells[buffs[slot].spellid].effectid[i])
		{
			case SE_WeaponProc:
			{
				uint16 procid = GetProcID(buffs[slot].spellid, i);
				RemoveProcFromWeapon(procid, false);
				break;
			}

			case SE_SummonHorse:
			{
				if(IsClient())
				{
					/*Mob* horse = entity_list.GetMob(this->CastToClient()->GetHorseId());
					if (horse) horse->Depop();
					CastToClient()->SetHasMount(false);*/
					CastToClient()->SetHorseId(0);
				}
				break;
			}

			case SE_IllusionCopy:
			case SE_Illusion:
			{
				uint8 texture = 0xFF;
				if(IsNPC() && !IsPlayableRace(GetBaseRace()))
				{
					texture = CastToNPC()->GetBaseTexture();
				}

				SendIllusionPacket
				(
					0,
					GetBaseGender(),
					texture, 
					0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
					GetBaseSize()
				);

				if(IsNPC() && !IsPlayableRace(GetBaseRace()))
				{
					uint32 newsize = floor(GetBaseSize() + 0.5);
					SendAppearancePacket(AppearanceType::Size, newsize);
				}
				else if(IsClient())
				{
					// Client doesn't keep track of helm like it does all other textures during illusions. Send it here.
					WearChange(EQ::textures::armorHead, CastToClient()->pc_helmtexture, CastToClient()->helmcolor);
				}

				if (FindType(SE_BindSight))
				{
					int16 fade_slot = GetBuffSlotFromType(SE_BindSight);
					if(slot != fade_slot && fade_slot != INVALID_INDEX)
						BuffFadeByEffect(SE_BindSight);
				}
				size = GetBaseSize();
				race = GetBaseRace();
				this->z_offset = CalcZOffset();
				this->head_offset = CalcHeadOffset();
				this->model_size = CalcModelSize();
				this->model_bounding_radius = CalcBoundingRadius();
				break;
			}

			case SE_Levitate:
			{
				if (!AffectedBySpellExcludingSlot(slot, SE_Levitate) && IsClient())
					SendAppearancePacket(AppearanceType::FlyMode, 0, true, false);
				break;
			}

			case SE_Invisibility:
			{
				SetInvisible(INVIS_OFF, false);
				break;
			}

			case SE_InvisVsUndead:
			{
				invisible_undead = false;	// Mongrel: No longer IVU
				SetInvisible(INVIS_OFF, false);
				break;
			}

			case SE_InvisVsAnimals:
			{
				invisible_animals = false;
				SetInvisible(INVIS_OFF, false);
				break;
			}

			case SE_SeeInvis:
			{
				see_invis = 0;
				break;
			}

			case SE_Silence:
			{
				Silence(false);
				break;
			}

			case SE_Amnesia:
			{
				Amnesia(false);
				break;
			}

			case SE_DivineAura:
			{
				SetInvul(false);
				break;
			}

			case SE_Rune:
			{
				buffs[slot].melee_rune = 0;
				break;
			}

			case SE_AbsorbMagicAtt:
			{
				buffs[slot].magic_rune = 0;
				break;
			}

			case SE_Familiar:
			{
				Mob *mypet = GetPet();
				if (mypet){
					if(mypet->IsNPC())
						mypet->CastToNPC()->Depop();
					SetPetID(0);
				}
				break;
			}

			case SE_Mez:
			{
				SendAppearancePacket(AppearanceType::Animation, Animation::Standing);	// unfreeze
				this->mezzed = false;
				if(IsNPC())
					UnStun();
				break;
			}

			case SE_Charm:
			{
				dire_charmed = false;

				if(IsNPC())
				{
					InterruptSpell();
					CastToNPC()->RestoreGuardSpotCharm();
					SendAppearancePacket(AppearanceType::Pet, 0, true, true);
					CastToNPC()->RestoreNPCFactionID();
				}

				if (IsAIControlled())
					StopNavigation();

				Mob* owner = GetOwner();

				if (GetSummonerID() != 0)
				{
					Mob* summoner = entity_list.GetMobID(GetSummonerID());

					if (summoner && !summoner->GetPet() && !summoner->IsInvisible(this) && (summoner->IsNPC() || (summoner->IsClient() && !summoner->IsAIControlled())))
					{
						SetOwnerID(GetSummonerID());
						summoner->SetPetID(this->GetID());
						SetPetOrder(SPO_Follow);
					}
					else
					{
						// If the summoner hasn't zoned but has become an invalid owner of its old pet, send the client a packet to clear the pet window. In every case,
						// we need to depop the NPC here.
						if (summoner && summoner->IsClient())
						{
							auto app = new EQApplicationPacket(OP_Charm, sizeof(Charm_Struct));
							Charm_Struct *ps = (Charm_Struct*)app->pBuffer;
							ps->owner_id = summoner->GetID();
							ps->pet_id = this->GetID();
							ps->command = 0;
							entity_list.QueueClients(this, app);
							safe_delete(app);
						}

						Depop();
					}
				}
				else
				{
					// The NPC was either never a summoned pet, or it was and the player owner has since zoned. Pets that were once summoned should always depop here.
					// Normal NPCs that were charmed will continue on their way as normal NPCs again.
					SetOwnerID(0);
				}

				if(owner)
				{
					if (!GetSummonerID())
						owner->SetPet(0);
				}
				if (IsAIControlled())
				{
					// clear the hate list of the mobs
					entity_list.InterruptTargeted(this);
					BuffFadeDetrimentalByNPCs(slot);
					entity_list.RemoveDotsFromNPCs(this);
					//entity_list.RemoveDebuffsFromNPCs(this);
					entity_list.RemoveFromNPCTargets(this);
					WipeHateList(true);
					if (owner && owner->IsClient() && !GetSummonerID() && (!owner->GetOwnerID() || !owner->IsCharmed() || (owner->GetOwner() && owner->GetOwner()->IsClient())))
					{
						// PoP ench charms had a rare chance to attack non-enchanter targets on break (might be just CoD?)
						// The behavior is not well understood, so just doing this until more info is discovered
						bool found = false;
						bool pop_charm = buffs[slot].spellid == 3355 || buffs[slot].spellid == 3347;	// Command of Druzzil, Beckon
						if (this->IsNPC() && pop_charm && zone->random.Roll(5))
						{
							Log(Logs::Detail, Logs::Spells, "Attempting to aggro on nearby client instead of enchanter");
							found = entity_list.AICheckClientAggro(this->CastToNPC());	// find a client to aggro on
						}
						if (!pop_charm || !found)
						{
							int32 charmBreakHate = GetMaxHP() / 8;
							if (charmBreakHate > 1200) charmBreakHate = 1200;
							if (charmBreakHate < 50) charmBreakHate = 50;

							AddToHateList(owner, 1, 0);		// doing this twice so animals get correct hate amount
							AddToHateList(owner, charmBreakHate, 0);
						}
					}
					SendAppearancePacket(AppearanceType::Animation, Animation::Standing);
				}
				if (owner && owner->IsClient())
				{
					owner->FadeVoiceGraft();

					auto app = new EQApplicationPacket(OP_Charm, sizeof(Charm_Struct));
					Charm_Struct *ps = (Charm_Struct*)app->pBuffer;
					ps->owner_id = owner->GetID();
					ps->pet_id = this->GetID();
					ps->command = 0;
					entity_list.QueueClients(this, app);
					safe_delete(app);
				}
				if (IsClient())
				{
					InterruptSpell();
					charmed = false;
					CastToClient()->AI_Stop();
					if (this->CastToClient()->IsLD())
					{					
						CastToClient()->AI_Start();
					}
					else
					{
						bool is_feared = FindType(SE_Fear);
						if (is_feared) {
							feared = true;
							CastToClient()->AI_Start();
						}
					}
				}
				if (owner && owner->IsNPC())
				{
					// NPCs can have multiple pets, but can only control one for now (multi pet assist needs to be implemented)
					// set NPC's pet id to summoned pet so it doesn't end up not assisting in case the charmed pet was the pet id
					uint16 summonedPetID = entity_list.GetSummonedPetID(owner);
					if (summonedPetID)
						owner->SetPetID(summonedPetID);
				}

				if (IsNPC())
				{
					zone->HasCharmedNPC = entity_list.HasCharmedNPC();
				}
				break;
			}

			case SE_Root:
			{
				buffs[slot].RootBreakChance = 0;
				rooted = false;
				break;
			}

			case SE_Blind:
				if (curfp && !FindType(SE_Fear))
				{
					curfp = false;
				}
				break;

			case SE_Fear:
			{
				if(RuleB(Combat, EnableFearPathing)){
					if(IsClient())
					{
						feared = false;
						CastToClient()->AI_Stop();
						bool is_charmed = FindType(SE_Charm);
						if (is_charmed) {
							charmed = true;
							CastToClient()->AI_Start();
						}
					}

					if (curfp)
					{
						curfp = false;
						if (IsNPC())
							StopNavigation();
					}
				}
				else
				{
					if (IsClient())
						CastToClient()->UnStun();
					else
						UnStun();
				}
				break;
			}

			case SE_BindSight:
			{
				if(IsClient())
				{
					CastToClient()->SetBindSightTarget(nullptr);
				}
				break;
			}

			case SE_AlterNPCLevel:
			{
				if (IsNPC())
					SetLevel(GetOrigLevel());
				break;
			}

			case SE_MovementSpeed:
			{
				if(IsNPC() && IsRunning() && !IsEngaged())
				{
					if (GetBaseRace() != SHIP)
						SetRunning(false);
				}
				if(IsClient())
				{
					Client *my_c = CastToClient();
					uint32 cur_time = Timer::GetCurrentTime();
					if((cur_time - my_c->m_TimeSinceLastPositionCheck) > 1000)
					{
						float speed = (my_c->m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - my_c->m_TimeSinceLastPositionCheck);
						float runs = my_c->GetRunspeed();
						if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
						{
							if(!my_c->GetGMSpeed() && (runs >= my_c->GetBaseRunspeed() || (speed > (my_c->GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
							{
								Log(Logs::General, Logs::Status, "%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
									my_c->m_DistanceSinceLastPositionCheck, (cur_time - my_c->m_TimeSinceLastPositionCheck), speed);
								if(my_c->IsShadowStepExempted())
								{
									if(my_c->m_DistanceSinceLastPositionCheck > 800)
									{
										my_c->CheatDetected(MQWarpShadowStep, my_c->GetX(), my_c->GetY(), my_c->GetZ());
									}
								}
								else if(my_c->IsKnockBackExempted())
								{
									//still potential to trigger this if you're knocked back off a
									//HUGE fall that takes > 2.5 seconds
									if(speed > 30.0f)
									{
										my_c->CheatDetected(MQWarpKnockBack, my_c->GetX(), my_c->GetY(), my_c->GetZ());
									}
								}
								else if(!my_c->IsPortExempted())
								{
									if(!my_c->IsMQExemptedArea(zone->GetZoneID(), my_c->GetX(), my_c->GetY(), my_c->GetZ()))
									{
										if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
										{
											my_c->m_TimeSinceLastPositionCheck = cur_time;
											my_c->m_DistanceSinceLastPositionCheck = 0.0f;
											my_c->CheatDetected(MQWarp, my_c->GetX(), my_c->GetY(), my_c->GetZ());
											//my_c->Death(my_c, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
										}
										else
										{
											my_c->CheatDetected(MQWarpLight, my_c->GetX(), my_c->GetY(), my_c->GetZ());
										}
									}
								}
							}
						}
					}
					my_c->m_TimeSinceLastPositionCheck = cur_time;
					my_c->m_DistanceSinceLastPositionCheck = 0.0f;
				}

				break;
			}

			case SE_EyeOfZomm:
			{
				if(IsClient())
				{
					NPC* zommnpc = nullptr;
					if (entity_list.GetZommPet(this, zommnpc))
					{
						if (zommnpc != nullptr)
						{
							zommnpc->SetFollowID(0);
							zommnpc->GetSwarmInfo()->owner_id = 0;
						}
					}
					CastToClient()->has_zomm = false;
					
					// The client handles this as well on the first OP_ClientUpdate sent after Zomm fades, but we can't trust the client.
					m_Position = glm::vec4(GetEQX(), GetEQY(), GetEQZ(), GetEQHeading());
				}

				break;
			}
		}
	}

	// Generate worn off messages.
	if (buffs[slot].client) {
		Mob *p = entity_list.GetMob(buffs[slot].caster_name);
		if (p && message && !buffs[slot].isdisc)
		{
			char spellname[32];
			if (IsCharmSpell(buffs[slot].spellid))
				strcpy(spellname, "charm");
			else if (IsFearSpell(buffs[slot].spellid))
				strcpy(spellname, "fear");
			else
				strcpy(spellname, spells[buffs[slot].spellid].name);

			bool detrimental_exception = message && !update && IsDetrimentalSpell(buffs[slot].spellid);
			// A spell has worn off a pet. Send the message to its master.
			if (HasOwner() && GetOwner()->IsClient() && !IsCharmedPet())
			{
				Mob* notify = GetOwner();
				if (notify)
				{
					notify->Message_StringID(Chat::SpellWornOff, PET_SPELL_WORN_OFF, spellname);
				}
			}
			// Our spell has worn off another NPC or client.
			// Lulls/harmonies don't show message
			else if ((p != this && !p->IsPet() && !IsBeneficialSpell(buffs[slot].spellid) && !IsCrowdControlSpell(buffs[slot].spellid))
				|| detrimental_exception)
			{
				p->Message_StringID(Chat::SpellWornOff, SPELL_WORN_OFF, spellname);
			}
		}
	}
	
	uint16 spellid = buffs[slot].spellid;

	buffs[slot].spellid = SPELL_UNKNOWN;

	if (was_mezzed && !IsMezzed())
	{
		// mob was mezzed and this fade removed the mez effect.  this code removes all other mez spells to generate worn off messages and avoid
		// a situation where an NPC can be in melee combat while still having a mez spell active, causing a second UnStun() later when that wears
		// off due to duration
		BuffFadeByEffect(SE_Mez);
	}

	if (iRecalcBonuses)
		CalcBonuses();
}

int32 Client::GetAAEffectDataBySlot(uint32 aa_ID, uint32 slot_id, bool GetEffect, bool GetBase1, bool GetBase2)
{
	int32 aa_effects_data[3] = { 0 };
	uint32 effect = 0;
	int32 base1 = 0;
	int32 base2 = 0;
	uint32 slot = 0;


	std::map<uint32, std::map<uint32, AA_Ability> >::const_iterator find_iter = aa_effects.find(aa_ID);
	if(find_iter == aa_effects.end())
		return 0;

	for (std::map<uint32, AA_Ability>::const_iterator iter = aa_effects[aa_ID].begin(); iter != aa_effects[aa_ID].end(); ++iter)
	{
		effect = iter->second.skill_id;
		base1 = iter->second.base1;
		base2 = iter->second.base2;
		slot = iter->second.slot;
	
		if (slot && slot == slot_id) {

			if (GetEffect)
				return effect;
			
			if (GetBase1)
				return base1;
			
			if (GetBase2)
				return base2;
		}
	}

	return 0;
}


int16 Client::CalcAAFocus(focusType type, uint32 aa_ID, uint16 spell_id)
{
	const SPDat_Spell_Struct &spell = spells[spell_id];

	int16 value = 0;
	int lvlModifier = 100;
	int spell_level = 0;
	int lvldiff = 0;
	uint32 effect = 0;
	int32 base1 = 0;
	int32 base2 = 0;
	uint32 slot = 0;

	bool LimitFailure = false;
	bool LimitInclude[MaxLimitInclude] = { false }; 
	/* Certain limits require only one of several Include conditions to be true. Ie. Add damage to fire OR ice spells.
	0/1   SE_LimitResist
	2/3   SE_LimitSpell
	4/5   SE_LimitEffect
	6/7   SE_LimitTarget
	8/9   SE_LimitSpellGroup:
	10/11 SE_LimitCastingSkill:
	12/13 SE_LimitSpellClass:
	14/15 SE_LimitSpellSubClass:
	Remember: Update MaxLimitInclude in spdat.h if adding new limits that require Includes
	*/ 
	int FocusCount = 0;

	std::map<uint32, std::map<uint32, AA_Ability> >::const_iterator find_iter = aa_effects.find(aa_ID);
	if(find_iter == aa_effects.end())
	{
		return 0;
	}

	for (std::map<uint32, AA_Ability>::const_iterator iter = aa_effects[aa_ID].begin(); iter != aa_effects[aa_ID].end(); ++iter)
	{
		effect = iter->second.skill_id;
		base1 = iter->second.base1;
		base2 = iter->second.base2;
		slot = iter->second.slot;
		
		/*
		AA Foci's can contain multiple focus effects within the same AA.
		To handle this we will not automatically return zero if a limit is found.
		Instead if limit is found and multiple focus effects, we will reset the limit check
		when the next valid focus effect is found.
		*/

		if (IsFocusEffect(0, 0, true,effect)){
			FocusCount++;
			//If limit found on prior check next, else end loop.
			if (FocusCount > 1){

				for(int e = 0; e < MaxLimitInclude; e+=2) {
					if (LimitInclude[e] && !LimitInclude[e+1])
						LimitFailure = true;
				}

				if (LimitFailure){
					value = 0;
					LimitFailure = false;
					
					for(int e = 0; e < MaxLimitInclude; e++) {
						LimitInclude[e] = false; //Reset array
					}
				}

				else{
					break;
				}
			}
		}


		switch (effect)
		{
			case SE_Blank:
				break;

			//Handle Focus Limits

			case SE_LimitResist:
				if(base1 < 0){
					if(spell.resisttype == -base1) //Exclude
						LimitFailure = true;
				}
				else {
				LimitInclude[0] = true;
				if (spell.resisttype == base1) //Include
					LimitInclude[1] = true;
				}
				break;

			case SE_LimitInstant:
				if(base1 == 1 && spell.buffduration) //Fail if not instant
					LimitFailure = true;
				if(base1 == 0 && (spell.buffduration == 0)) //Fail if instant
					LimitFailure = true;

				break;
			
			case SE_LimitMaxLevel:
				spell_level = spell.classes[(GetClass()%16) - 1];
				lvldiff = spell_level - base1;
				//every level over cap reduces the effect by base2 percent unless from a clicky when ItemCastsUseFocus is true
				if(lvldiff > 0 && (spell_level <= RuleI(Character, MaxLevel) || RuleB(Character, ItemCastsUseFocus) == false))	{
					if(base2 > 0){
						lvlModifier -= base2*lvldiff;
						if(lvlModifier < 1)
							LimitFailure = true;
					}
					else 
						LimitFailure = true;
				}
				break;

			case SE_LimitMinLevel:
				if((spell.classes[(GetClass()%16) - 1]) < base1)
					LimitFailure = true;
				break;

			case SE_LimitCastTimeMin:
				if (static_cast<int32>(spell.cast_time) < base1)
					LimitFailure = true;
				break;

			case SE_LimitCastTimeMax:
				if (static_cast<int32>(spell.cast_time) > base1)
					LimitFailure = true;
				break;

			case SE_LimitSpell:
				if(base1 < 0) {	//Exclude
					if (spell_id == -base1)
						LimitFailure = true;
				} 
				else {
					LimitInclude[2] = true;
					if (spell_id == base1) //Include
						LimitInclude[3] = true;
				}
				break;

			case SE_LimitMinDur:
				if (base1 > CalcBuffDuration_formula(GetLevel(), spell.buffdurationformula, spell.buffduration))
					LimitFailure = true;

				break;

			case SE_LimitEffect:
				if(base1 < 0){
					if(IsEffectInSpell(spell_id,-base1)) //Exclude
						LimitFailure = true;
				}
				else{
					LimitInclude[4] = true;
					if(IsEffectInSpell(spell_id,base1)) //Include
						LimitInclude[5] = true;
				}
				break;

			case SE_LimitSpellType:
				switch(base1)
				{
					case 0:
						if (!IsDetrimentalSpell(spell_id) || IsNeutralSpell(spell_id))
							LimitFailure = true;
						break;
					case 1:
						if (!IsBeneficialSpell(spell_id) && !IsNeutralSpell(spell_id))
							LimitFailure = true;
						break;
				}
				break;

			case SE_LimitTarget:
				if (base1 < 0) {
					if (-base1 == spell.targettype) //Exclude
						LimitFailure = true;
				}
				else {
					LimitInclude[6] = true;
					if (base1 == spell.targettype) //Include
						LimitInclude[7] = true;
				}
				break;

			case SE_LimitCombatSkills:
				if (base1 == 0 && (IsCombatSkill(spell_id) || IsCombatProc(spell_id))) //Exclude Discs / Procs
					LimitFailure = true;
				else if (base1 == 1 && (!IsCombatSkill(spell_id) || !IsCombatProc(spell_id))) //Exclude Spells
					LimitFailure = true;

			break;

			case SE_LimitSpellGroup:
				if(base1 < 0) {
					if (-base1 == spell.spellgroup) //Exclude
						LimitFailure = true;
				}
				else {
					LimitInclude[8] = true;
					if (base1 == spell.spellgroup) //Include
						LimitInclude[9] = true;
				}
				break;

			//Handle Focus Effects
			case SE_ImprovedDamage:
				if (type == focusImprovedDamage && base1 > value)
					value = base1;
				break;

			case SE_ImprovedHeal:
				if (type == focusImprovedHeal && base1 > value)
					value = base1;
				break;

			case SE_ReduceManaCost:
				if (type == focusManaCost)
					value = base1;
				break;

			case SE_IncreaseSpellHaste:
				if (type == focusSpellHaste && base1 > value)
					value = base1;
				break;

			case SE_IncreaseSpellDuration:
				if (type == focusSpellDuration && base1 > value)
					value = base1;
				break;

			case SE_IncreaseRange:
				if (type == focusRange && base1 > value)
					value = base1;
				break;

			case SE_ReduceReagentCost:
				if (type == focusReagentCost && base1 > value)
					value = base1;
				break;

			case SE_SpellHateMod:
				if (type == focusSpellHateMod ) {
					if(value != 0) {
						if(value > 0){
							if(base1 > value)
								value = base1;
						}
						else{
							if(base1 < value)
								value = base1;
						}
					}
					else
						value = base1;
				}
				break;

			case SE_FcDamagePctCrit:	// only used for SK Soul Abrasion AA
				if(type == focusSpellDamageMult)
					value = base1;
				break;

		}
	}

	for(int e = 0; e < MaxLimitInclude; e+=2) {
		if (LimitInclude[e] && !LimitInclude[e+1])
			return 0;
	}

	if (LimitFailure)
		return 0;
	
	return(value*lvlModifier/100);
}

//given an item/spell's focus ID and the spell being cast, determine the focus ammount, if any
//assumes that spell_id is not a bard spell and that both ids are valid spell ids
// best_focus disables the randomizing of the focus value for damage/healing/mana preservation
int16 Mob::CalcFocusEffect(focusType type, uint16 focus_id, uint16 spell_id, bool best_focus, bool dot_tick, int spell_level)
{
	// spell_level is used for recourses.  It is the spell level of the parent spell.
	// if this has a level, then the caster can cast this spell, and that is the spells level.
	if(!IsValidSpell(focus_id) || !IsValidSpell(spell_id))
		return 0;
	
	const SPDat_Spell_Struct &focus_spell = spells[focus_id];
	const SPDat_Spell_Struct &spell = spells[spell_id];

	int16 value = 0;
	int lvlModifier = 100;
	int casted_spell_level = 0; 
	int lvldiff = 0;
	uint32 Caston_spell_id = 0;

	if (spell_level != -1)
		casted_spell_level = spell_level;
	else
		casted_spell_level = spell.classes[(GetClass() % 16) - 1];

	bool LimitInclude[MaxLimitInclude] = { false }; 
	/* Certain limits require only one of several Include conditions to be true. Ie. Add damage to fire OR ice spells.
	0/1   SE_LimitResist
	2/3   SE_LimitSpell
	4/5   SE_LimitEffect
	6/7   SE_LimitTarget
	8/9   SE_LimitSpellGroup:
	10/11 SE_LimitCastingSkill:
	12/13 SE_LimitSpellClass:
	14/15 SE_LimitSpellSubClass:
	Remember: Update MaxLimitInclude in spdat.h if adding new limits that require Includes

	The focus effects themselves are always in effect slot 0 and only work there.  The filters are always in the higher slots only.
	*/ 
	
	for (int i = 0; i < EFFECT_COUNT; i++) {

		switch (focus_spell.effectid[i]) {
		
		case SE_Blank:
			break;
			
		case SE_LimitResist:
			if(focus_spell.base[i] < 0){
				if (spell.resisttype == -focus_spell.base[i]) //Exclude
					return 0;
			}
			else {
				LimitInclude[0] = true;
				if (spell.resisttype == focus_spell.base[i]) //Include
					LimitInclude[1] = true;
			}
			break;
		
		case SE_LimitInstant:
			if (focus_spell.base[i] == 1 && spell.buffduration) //Fail if not instant
			{
				Log(Logs::Detail, Logs::Focus, "Focus %d only affects direct spells.", focus_id);
				return 0;
			}
			if (focus_spell.base[i] == 0 && (spell.buffduration == 0)) //Fail if instant
			{
				Log(Logs::Detail, Logs::Focus, "Focus %d only affects buffs.", focus_id);
				return 0;
			}

			break;

		case SE_LimitMaxLevel:
			if (IsNPC())
				break;;
			lvldiff = casted_spell_level - focus_spell.base[i];
			Log(Logs::Detail, Logs::Focus, "Focus %d affects buffs with a max level of %d.", focus_id, focus_spell.base[i]);

			//every level over cap reduces the effect by focus_spell.base2[i] percent unless from a clicky when ItemCastsUseFocus is true
			if(lvldiff > 0 && (casted_spell_level <= RuleI(Character, MaxLevel) || RuleB(Character, ItemCastsUseFocus) == false)){
				if(focus_spell.base2[i] > 0){
					lvlModifier -= focus_spell.base2[i]*lvldiff;
					if(lvlModifier < 1)
						return 0;
				}
				else
					return 0;
			}
			break;
		
		case SE_LimitMinLevel:
			if (IsNPC())
				break;
			Log(Logs::Detail, Logs::Focus, "Focus %d affects buffs with a min level of %d.", focus_id, focus_spell.base[i]);
			if (spell.classes[(GetClass()%16) - 1] < focus_spell.base[i])
				return(0);
			break;

		case SE_LimitCastTimeMin:
			Log(Logs::Detail, Logs::Focus, "Focus %d affects buffs with a min casting time of %d.", focus_id, focus_spell.base[i]);
			if (spells[spell_id].cast_time < (uint16)focus_spell.base[i])
				return(0);
			break;

		case SE_LimitCastTimeMax:
			Log(Logs::Detail, Logs::Focus, "Focus %d affects buffs with a max casting time of %d.", focus_id, focus_spell.base[i]);
			if (spells[spell_id].cast_time > (uint16)focus_spell.base[i])
				return(0);
			break;
			
		case SE_LimitSpell:
			if(focus_spell.base[i] < 0) {	//Exclude
				Log(Logs::Detail, Logs::Focus, "Focus %d excludes spellid of %d.", focus_id, -focus_spell.base[i]);
				if (spell_id == -focus_spell.base[i])
					return(0);
			} 
			else {
				LimitInclude[2] = true;
				if (spell_id == focus_spell.base[i]) //Include
					LimitInclude[3] = true;
			}
			break;

		case SE_LimitMinDur:
			if (focus_spell.base[i] > CalcBuffDuration_formula(GetLevel(), spell.buffdurationformula, spell.buffduration))
			{
				Log(Logs::Detail, Logs::Focus, "Focus %d only affects buffs with a min duration of %d.", focus_id, focus_spell.base[i]);
				return(0);
			}
			break;

		case SE_LimitEffect:
			if(focus_spell.base[i] < 0){
				if(IsEffectInSpell(spell_id,-focus_spell.base[i])) //Exclude
				{
					return (0);
				}
			}
			else {
				LimitInclude[4] = true;
				// Affliction Haste, Affliction Efficiency
				if(focus_spell.base[i] == SE_CurrentHP && (type == focusSpellHaste || type == focusManaCost))
				{
					LimitInclude[5] = true;
				}
				else if(IsEffectInSpell(spell_id,focus_spell.base[i])) //Include
				{
					LimitInclude[5] = true;
				}
			}
			break;


		case SE_LimitSpellType:
			if (spells[spell_id].goodEffect != focus_spell.base[i])
				return 0;
			break;

		case SE_LimitTarget:
			if (focus_spell.base[i] < 0) {
				if (-focus_spell.base[i] == spell.targettype) //Exclude
					return 0;
			}
			else {
				LimitInclude[6] = true;
				if (focus_spell.base[i] == spell.targettype) //Include
					LimitInclude[7] = true;
			}
			break;

		case SE_LimitCombatSkills:
			if (focus_spell.base[i] == 0 && (IsCombatSkill(spell_id) || IsCombatProc(spell_id))) //Exclude Discs / Procs
				return 0;
			else if (focus_spell.base[i] == 1 && (!IsCombatSkill(spell_id) || !IsCombatProc(spell_id))) //Exclude Spells
				return 0;

			break;

		case SE_LimitSpellGroup:
			if(focus_spell.base[i] < 0) {
				if (-focus_spell.base[i] == spell.spellgroup) //Exclude
					return 0;
			}
			else {
				LimitInclude[8] = true;
				if (focus_spell.base[i] == spell.spellgroup) //Include
					LimitInclude[9] = true;
			}
			break;

		case SE_ImprovedDamage:
			if (type == focusImprovedDamage)
			{
				if (!CanClassCastSpell(spell_id) && spell_id != SPELL_HARM_TOUCH && spell_id != SPELL_HARM_TOUCH2 && spell_id != SPELL_IMP_HARM_TOUCH)
					return 0;

				if (best_focus)
				{
					value = focus_spell.base[i];
				}
				else
				{
					value = zone->random.Int(1, focus_spell.base[i]);
				}
				// Effects 140 and 141 is what determines if the focus can affect a DoT or not.
				// Most DD focus spells don't have this effect, so they will always return
				// a value, even for DoT since there is nothing to tell them not to.
				// This loop returns 0 if the spell is a DoT and the effect is not found.
				if (dot_tick)
				{
					bool has_limit_effect = false;
					for (int d = 0; d < EFFECT_COUNT; d++)
					{
						switch (focus_spell.effectid[d]) {
						case SE_LimitInstant:
							has_limit_effect = true;
							break;

						case SE_LimitMinDur:
							has_limit_effect = true;
							break;
						}
					}

					if (!has_limit_effect && spell.buffduration)
					{
						Log(Logs::Detail, Logs::Focus, "Focus %d is not Burning Affliction, dot tick will be ignored.", focus_id);
						return 0;
					}
				}
			}
			break;

		case SE_ReduceManaCost:
			if (type == focusManaCost)
			{
				if (!CanClassCastSpell(spell_id))
					return 0;

				if (best_focus)
				{
					value = focus_spell.base[i];
				}
				else
				{
					value = zone->random.Int(1, focus_spell.base[i]);
				}
			}
			break;
		case SE_ImprovedHeal:
			if (type == focusImprovedHeal)
			{
				if (!CanClassCastSpell(spell_id))
					return 0;

				if (best_focus)
				{
					value = focus_spell.base[i];
				}
				else
				{
					value = zone->random.Int(1, focus_spell.base[i]);
				}
			}
			break;

		case SE_IncreaseSpellHaste:
			// focus effects only work in effect slot 0.  there are 2 spells that have SE_IncreaseSpellHaste in the wrong slot
			if (i == 0 && type == focusSpellHaste && (value == 0 || (value > 0 && (focus_spell.base[i] > value || focus_spell.base[i] < 0)) || (value < 0 && focus_spell.base[i] < value)))
			{
				if (!CanClassCastSpell(spell_id))
					return 0;

				value = focus_spell.base[i];
			}
			break;
		
		case SE_IncreaseSpellDuration:
			if (type == focusSpellDuration && focus_spell.base[i] > value)
			{
				if (!CanClassCastSpell(spell_id) && spell_level == -1)
					return 0;

				value = focus_spell.base[i];
			}
			break;

		case SE_IncreaseRange:
			if (type == focusRange && focus_spell.base[i] > value)
			{
				if (!CanClassCastSpell(spell_id))
					return 0;

				value = focus_spell.base[i];
			}
			break;

		case SE_ReduceReagentCost:
			if (type == focusReagentCost && focus_spell.base[i] > value)
			{
				if (!CanClassCastSpell(spell_id))
					return 0;

				value = focus_spell.base[i];
			}
			break;

		case SE_SpellHateMod:
			if (type == focusSpellHateMod)
			{
				if (!CanClassCastSpell(spell_id))
					return 0;

				if (best_focus)
				{
					value = focus_spell.base[i];
				}
				else
				{
					value = zone->random.Int(1, focus_spell.base[i]);
				}
			}
			break;

		case SE_FcDamagePctCrit:
			if(type == focusSpellDamageMult)		// used for SK Soul Abrasion AA
				value = focus_spell.base[i];
			break;

#if EQDEBUG >= 6
		//this spits up a lot of garbage when calculating spell focuses
		//since they have all kinds of extra effects on them.
		default:
			Log(Logs::General, Logs::Normal, "CalcFocusEffect: unknown effectid %d", focus_spell.effectid[i]);
#endif
		}
		
	}

	for(int e = 0; e < MaxLimitInclude; e+=2) {
		if (LimitInclude[e] && !LimitInclude[e+1])
		{
			return 0;
		}
	}
	
	if (Caston_spell_id){
		if(IsValidSpell(Caston_spell_id) && (Caston_spell_id != spell_id))
			SpellFinished(Caston_spell_id, this, EQ::spells::CastingSlot::Item, 0, -1, spells[Caston_spell_id].ResistDiff);
	}

	return(value*lvlModifier/100);
}

int16 Client::GetFocusEffect(focusType type, uint16 spell_id, std::string& item_name, bool dot_tick, int spell_level, bool include_items, bool include_spells, bool include_aa) {

	if (IsBardSong(spell_id)) {
		return 0;
	}

	if ((type == focusManaCost || type == focusSpellHaste) &&
		(casting_spell_slot == EQ::spells::CastingSlot::Item || casting_aa > 0)) {
		return 0;
	}

	if (!content_service.IsTheShadowsOfLuclinEnabled()) {
		return 0;
	}

	if (RuleB(AlKabor, EnableEraItemRules))
	{
		//Disable Focus Effects before we enter Luclin.
		if (RuleR(World, CurrentExpansion) < (float)ExpansionEras::LuclinEQEra)
			return 0;
	}
	int16 realTotal = 0;
	int16 realTotal2 = 0;
	int16 realTotal3 = 0;
	bool rand_effectiveness = false;

	//Improved Healing, Damage & Mana Reduction are handled differently in that some are random percentages
	//In these cases we need to find the most powerful effect, so that each piece of gear wont get its own chance
	if((type == focusManaCost || type == focusImprovedHeal || type == focusImprovedDamage || type == focusSpellHateMod)
		&& RuleB(Spells, LiveLikeFocusEffects)) {
		rand_effectiveness = true;
	}

	//Check if item focus effect exists for the client.
	if (include_items && itembonuses.FocusEffects[type]){

		const EQ::ItemData* TempItem = nullptr;
		const EQ::ItemData* UsedItem = nullptr;
		uint16 UsedFocusID = 0;
		int16 Total = 0;
		int16 focus_max = 0;
		int16 focus_max_real = 0;

		//item focus
		for(int x = EQ::invslot::EQUIPMENT_BEGIN; x <= EQ::invslot::EQUIPMENT_END; x++) {
			TempItem = nullptr;
			EQ::ItemInstance* ins = GetInv().GetItem(x);
			if (!ins)
				continue;
			TempItem = ins->GetItem();
			if (TempItem && TempItem->Focus.Effect > 0 && TempItem->Focus.Effect != SPELL_UNKNOWN) {
				if(rand_effectiveness) {
					focus_max = CalcFocusEffect(type, TempItem->Focus.Effect, spell_id, true, dot_tick, spell_level);
					if (focus_max > 0 && focus_max_real >= 0 && focus_max > focus_max_real) {
						focus_max_real = focus_max;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					} else if (focus_max < 0 && focus_max < focus_max_real) {
						focus_max_real = focus_max;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					}
				}
				else {
					Total = CalcFocusEffect(type, TempItem->Focus.Effect, spell_id, false, dot_tick, spell_level);
					if (Total > 0 && realTotal >= 0 && Total > realTotal) {
						realTotal = Total;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					} else if (Total < 0 && Total < realTotal) {
						realTotal = Total;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					}
				}
			}
		}

		if (UsedItem) {
			if (rand_effectiveness && focus_max_real != 0) {
				realTotal = CalcFocusEffect(type, UsedFocusID, spell_id, false, dot_tick, spell_level);
			}

			item_name = UsedItem->Name;
		}

		// if ((rand_effectiveness && UsedItem) || (realTotal != 0 && UsedItem && type != focusReagentCost))
		if ((type != focusReagentCost && type != focusSpellHaste && type != focusRange) && ((rand_effectiveness && UsedItem) || (realTotal != 0 && UsedItem))) {
			// focusReagentCost is already handled in spells.cpp HasSpellReagent()
			// focusSpellHaste is already generated by the client, it is not needed here.
			// focusRange is already handled in spells.cpp DoCastingRangeCheck()
			// the RNG effective ones appear to have a different message for failing to focus
			uint32 string_id;
			switch (type) {
			case focusManaCost: // this might be GROWS_DIM for fail
				string_id = FLICKERS_PALE_LIGHT;
				break;
			case focusSpellDuration:
				string_id = SPARKLES;
				break;
			case focusImprovedDamage:
				if (realTotal)
					string_id = ALIVE_WITH_POWER;
				else
					string_id = SEEMS_DRAINED;
				break;
			case focusImprovedHeal:
				if (realTotal)
					string_id = FEEDS_WITH_POWER;
				else
					string_id = POWER_DRAIN_INTO;
				break;
			default:
				string_id = BEGINS_TO_GLOW; 
				break;
			}
			Message_StringID(Chat::Spells, string_id, item_name.c_str());
		}
	}

	//Check if spell focus effect exists for the client.
	if (include_spells && spellbonuses.FocusEffects[type]){

		//Spell Focus
		int16 Total2 = 0;
		int16 focus_max2 = 0;
		int16 focus_max_real2 = 0;

		int buff_tracker = -1;
		int buff_slot = 0;
		uint16 focusspellid = 0;
		uint16 focusspell_tracker = 0;
		int buff_max = GetMaxTotalSlots();
		for (buff_slot = 0; buff_slot < buff_max; buff_slot++) {
			focusspellid = buffs[buff_slot].spellid;
			if (focusspellid == 0 || focusspellid >= SPDAT_RECORDS)
				continue;

			if(rand_effectiveness) {
				focus_max2 = CalcFocusEffect(type, focusspellid, spell_id, true, dot_tick, spell_level);
				if (focus_max2 > 0 && focus_max_real2 >= 0 && focus_max2 > focus_max_real2) {
					focus_max_real2 = focus_max2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				} else if (focus_max2 < 0 && focus_max2 < focus_max_real2) {
					focus_max_real2 = focus_max2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				}
			}
			else {
				Total2 = CalcFocusEffect(type, focusspellid, spell_id, false, dot_tick, spell_level);
				if (Total2 > 0 && realTotal2 >= 0 && Total2 > realTotal2) {
					realTotal2 = Total2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				} else if (Total2 < 0 && Total2 < realTotal2) {
					realTotal2 = Total2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				}
			}
		}

		if (focusspell_tracker && rand_effectiveness && focus_max_real2 != 0) {
			realTotal2 = CalcFocusEffect(type, focusspell_tracker, spell_id, false, dot_tick, spell_level);
		}
	}


	// AA Focus
	if (include_aa && type != focusSpellHaste) { // AA Spell Haste has special handling, not just scanning for the highest effect
		if (aabonuses.FocusEffects[type]) {

			int16 Total3 = 0;
			uint32 aa_AA = 0;
			uint32 aa_value = 0;

			for (int i = 0; i < MAX_PP_AA_ARRAY; i++)
			{
				aa_AA = this->aa[i]->AA;
				aa_value = this->aa[i]->value;
				if (aa_AA < 1 || aa_value < 1)
					continue;

				Total3 = CalcAAFocus(type, aa_AA, spell_id);
				if (Total3 > 0 && realTotal3 >= 0 && Total3 > realTotal3) {
					realTotal3 = Total3;
				}
				else if (Total3 < 0 && Total3 < realTotal3) {
					realTotal3 = Total3;
				}
			}
		}
	}

	return realTotal + realTotal2 + realTotal3;
}

void Client::ApplyDurationFocus(uint16 spell_id, uint16 buffslot, Mob* spelltar, int spell_level)
{
	std::string item_name;
	if (IsBuffSpell(spell_id) && !IsSplurtFormulaSpell(spell_id))
	{
		if (!spelltar)
			return;

		if (casting_spell_focus_duration == 0)
		{
			casting_spell_focus_duration = GetFocusEffect(focusSpellDuration, spell_id, item_name, false, spell_level) + 100;
		}
		if (casting_spell_focus_duration > 100 || (spell_id == SPELL_PACIFY && RuleB(Spells, ReducePacifyDuration)))
		{
			Buffs_Struct *buffs = spelltar->GetBuffs();
			if (buffs)
			{
				// this is for custom behavior and is not AKurate
				if (spell_id == SPELL_PACIFY && RuleB(Spells, ReducePacifyDuration))
				{
					int32 pacify_original_duration = buffs[buffslot].ticsremaining;
					int32 pacify_modified_duration = 8; // 7 plus extra tick
					Log(Logs::General, Logs::Focus, "Pacify spell TAKP special - reducing duration from %d to %d before focus", pacify_original_duration, pacify_modified_duration);
					spelltar->BuffModifyDurationBySpellID(spell_id, pacify_modified_duration, false); // update false because we call the function again below to really update
				}
				int32 tics = buffs[buffslot].ticsremaining;
				int32 newduration = (tics * casting_spell_focus_duration) / 100;

				Log(Logs::General, Logs::Focus, "focusSpellDuration update tics remaining from %d to %d", tics, newduration);
				spelltar->BuffModifyDurationBySpellID(spell_id, newduration, true);
			}
		}
	}
}

//for some stupid reason SK procs return theirs one base off...
uint16 Mob::GetProcID(uint16 spell_id, uint8 effect_index)
{
	if (!RuleB(Spells, SHDProcIDOffByOne)) // UF+ spell files
		return spells[spell_id].base[effect_index];

	// We should actually just be checking if the mob is SHD, but to not force
	// custom servers to create new spells, we will still do this
	bool sk = false;
	bool other = false;
	for (int x = 0; x < 16; x++) {
		if (x == 4) {
			if (spells[spell_id].classes[4] < 255)
				sk = true;
		} else {
			if (spells[spell_id].classes[x] < 255)
				other = true;
		}
	}

	if (sk && !other)
		return spells[spell_id].base[effect_index] + 1;
	else
		return spells[spell_id].base[effect_index];
}

bool Mob::TryDivineSave()
{
	/*
	How Touch of the Divine AA works:
	-Gives chance to avoid death when client is killed.
	-Chance is determined by the sum of AA/item/spell chances.
	-If the chance is met a divine aura like effect 'Touch of the Divine' is applied to the client removing detrimental spell effects.
	-If desired, additional spells can be triggered from the AA/item/spell effect, generally a heal.
	*/

	int32 SuccessChance = aabonuses.DivineSaveChance[0] + itembonuses.DivineSaveChance[0] + spellbonuses.DivineSaveChance[0];
	if (SuccessChance && zone->random.Roll(SuccessChance))
	{
		SetHP(1);

		int32 EffectsToTry[] =
		{
			aabonuses.DivineSaveChance[1],
			itembonuses.DivineSaveChance[1],
			spellbonuses.DivineSaveChance[1]
		};
		//Fade the divine save effect here after saving the old effects off.
		//That way, if desired, the effect could apply SE_DivineSave again.
		BuffFadeByEffect(SE_DivineSave);
		for(size_t i = 0; i < ( sizeof(EffectsToTry) / sizeof(EffectsToTry[0]) ); ++i)
		{
			if( EffectsToTry[i] )
			{
				SpellOnTarget(EffectsToTry[i], this);
			}
		}

		// out of era for AK
		//SpellOnTarget(4789, this); //Touch of the Divine=4789, an Invulnerability/HoT/Purify effect
		SendHPUpdate();
		return true;
	}
	return false;
}

bool Mob::TryDeathSave() {

	/*
	How Death Save works:
	-Chance for Death Save to fire is the same for Death Pact/Divine Intervention
	-Base value of these determines amount healed (1=partial(300HP), 2='full (8000HP)) HARD CODED
	-Charisma of client who has the effect determines fire rate, parses show this clearly with ratio used.
	-Unfailing Divinity AA - Allows for a chance to give a heal for a percentage of the orginal value
	-No evidence of chance rate increasing between UD1-3, numbers indicate it uses same CHA rate as first DI.
	*/

	if (spellbonuses.DeathSave[0])
	{

		int buffSlot = spellbonuses.DeathSave[1];
		float SuccessChance = ((GetCHA() * 3.2) + 1) / 10;

		if (GetCHA() < 97)
			SuccessChance = 30;
		else if (GetCHA() > 245)
			SuccessChance = 80;

		if(buffSlot >= 0)
		{

			int32 UD_HealMod = buffs[buffSlot].ExtraDIChance;
			bool success = false;
			bool first_success = false;
			if (zone->random.Roll(static_cast<uint8>(SuccessChance)))
			{
				success = true;
				first_success = true;
			}

			if (!success && UD_HealMod)
			{
				if (zone->random.Roll(static_cast<uint8>(SuccessChance)))
				{
					success = true;
				}
			}

			if(success)
			{
				int HealAmt = 300; //Death Pact max Heal
				if (spellbonuses.DeathSave[0] == 2) // DI
				{
					if (IsClient())
						HealAmt = 8000;
					else if (IsNPC())
						HealAmt = 100000;
				}

				if(!first_success && UD_HealMod)
					HealAmt = HealAmt*UD_HealMod / 100;

				if ((GetMaxHP() - GetHP()) < HealAmt)
					HealAmt = GetMaxHP() - GetHP();

				SetHP((GetHP()+HealAmt));

				if(spellbonuses.DeathSave[0] == 2)
					entity_list.MessageClose_StringID(this, false, 200, Chat::MeleeCrit, DIVINE_INTERVENTION, GetCleanName());
				else
					entity_list.MessageClose_StringID(this, false, 200, Chat::MeleeCrit, DEATH_PACT, GetCleanName());

				SendHPUpdate();
				BuffFadeBySlot(buffSlot, true, false);
				return true;
			}
		}
	}
	return false;
}

bool Mob::AffectedBySpellExcludingSlot(int slot, int effect)
{
	for (int i = 0; i <= EFFECT_COUNT; i++)
	{
		if (i == slot)
			continue;

		if (IsEffectInSpell(buffs[i].spellid, effect))
			return true;
	}
	return false;
}

bool Mob::PassLimitClass(uint32 Classes_, uint16 Class_)
{
	//The class value for SE_LimitClass is +1 to its equivelent value in item dbase
	//Example Bard on items is '128' while Bard on SE_LimitClass is '256', keep this in mind if making custom spells.
	if (Class_ > 16)
		return false;

	Class_ += 1;

	for (int CurrentClass = 1; CurrentClass <= PLAYER_CLASS_COUNT; ++CurrentClass){
		if (Classes_ % 2 == 1){
			if (CurrentClass == Class_)
				return true;
		}
		Classes_ >>= 1;
	}
	return false;
}

bool Mob::TryDispel(uint8 caster_level, uint8 buff_level, int level_modifier){

	/*Live 5-20-14 Patch Note: Updated all spells which use Remove Detrimental and 
	Cancel Beneficial spell effects to use a new method. The chances for those spells to 
	affect their targets have not changed unless otherwise noted.*/

	/*This should provide a somewhat accurate conversion between pre 5/14 base values and post.
	until more information is avialble - Kayen*/
	if (level_modifier >= 100)
		level_modifier = level_modifier/100;

	//Dispels - Check level of caster agianst buffs level (level of the caster who cast the buff)
	//Effect value of dispels are treated as a level modifier.
	//Values for scaling were obtain from live parses, best estimates.

	caster_level += level_modifier; 
	int dispel_chance = 50; //Baseline chance if no level difference and no modifier
	int level_diff = caster_level - buff_level;

	dispel_chance += level_diff * 5;

	if (dispel_chance > 95)
		dispel_chance = 95;

	else if (dispel_chance < 5)
		dispel_chance = 5;

	if (zone->random.Roll(dispel_chance))
		return true;
	else
		return false;
}

bool Mob::PassCastRestriction(bool UseCastRestriction,  int16 value, bool IsDamage)
{
	/*If return TRUE spell met all restrictions and can continue (this = target).
	This check is used when the spell_new field CastRestriction is defined OR spell effect '0'(DD/Heal) has a defined limit
	Range 1			: UNKNOWN
	Range 100		: *Animal OR Humanoid
	Range 101		: *Dragon
	Range 102		: *Animal OR Insect
	Range 103		: NOT USED
	Range 104		: *Animal
	Range 105		: Plant
	Range 106		: *Giant
	Range 107		: NOT USED
	Range 108		: NOT Animal or Humaniod
	Range 109		: *Bixie
	Range 111		: *Harpy
	Range 112		: *Sporali
	Range 113		: *Kobold
	Range 114		: *Shade Giant
	Range 115		: NOT USED
	Range 116		: NOT USED
	Range 117		: *Animal OR Plant
	Range 118		: *Summoned
	Range 119		: *Firepet
	Range 120		: Undead
	Range 121		: *Living (NOT Undead)
	Range 122		: *Fairy
	Range 123		: Humanoid
	Range 124		: *Undead HP < 10%
	Range 125		: *Clockwork HP < 10%
	Range 126		: *Wisp HP < 10%
	Range 127-130	: UNKNOWN
	Range 150		: UNKNOWN
	Range 190		: No Raid boss flag *not implemented
	Range 191		: This spell will deal less damage to 'exceptionally strong targets' - Raid boss flag *not implemented
	Range 201		: Damage if HP > 75%
	Range 203		: Damage if HP < 20%
	Range 216		: TARGET NOT IN COMBAT
	Range 221 - 249	: Causing damage dependent on how many pets/swarmpets are attacking your target.
	Range 250		: Damage if HP < 35%
	Range 300 - 303	: UNKOWN *not implemented
	Range 304		: Chain + Plate class (buffs)
	Range 399 - 409	: Heal if HP within a specified range (400 = 0-25% 401 = 25 - 35% 402 = 35-45% ect)
	Range 410 - 411 : UNKOWN
	Range 500 - 599	: Heal if HP less than a specified value
	Range 600 - 699	: Limit to Body Type [base2 - 600 = Body]
	Range 700		: UNKNOWN
	Range 701		: NOT PET
	Range 800		: UKNOWN
	Range 818 - 819 : If Undead/If Not Undead
	Range 820 - 822	: UKNOWN
	Range 835 		: Unknown *not implemented
	Range 836 -	837	: Progression Server / Live Server *not implemented
	Range 839 		: Unknown *not implemented
	Range 842 - 844 : Humaniod lv MAX ((842 - 800) * 2)
	Range 845 - 847	: UNKNOWN
	Range 10000 - 11000	: Limit to Race [base2 - 10000 = Race] (*Not on live: Too useful a function to not implement)
	THIS IS A WORK IN PROGRESS
	*/ 

	if (value <= 0)
		return true;

	if (IsDamage || UseCastRestriction) {

		switch(value)
		{
			case 100:	
				if ((GetBodyType() == BT_Animal) || (GetBodyType() == BT_Humanoid))
					return true;
				break;

			case 101:	
				if (GetBodyType() == BT_Dragon || GetBodyType() == BT_VeliousDragon || GetBodyType() == BT_Dragon3)
					return true;
				break;

			case 102:	
				if ((GetBodyType() == BT_Animal) || (GetBodyType() == BT_Insect))
					return true;
				break;

			case 104:	
				if (GetBodyType() == BT_Animal)
					return true;
				break;

			case 105:	
				if (GetBodyType() == BT_Plant)
					return true;
				break;

			case 106:	
				if (GetBodyType() == BT_Giant)
					return true;
				break;

			case 108:	
				if ((GetBodyType() != BT_Animal) || (GetBodyType() != BT_Humanoid))
					return true;
				break;

			case 109:	
				if (GetRace() == BIXIE)
					return true;
				break;

			case 111:	
				if (GetRace() == HALFLING)
					return true;
				break;

			case 112:	
				if (GetRace() == FUNGUSMAN)
					return true;
				break;

			case 113:	
				if (GetRace() == KOBOLD)
					return true;
				break;

			case 114:	
				break;

			case 117:	
				if ((GetBodyType() == BT_Animal) || (GetBodyType() == BT_Plant))
					return true;
				break;

			case 118:	
				if (GetBodyType() == BT_Summoned)
					return true;
				break;

			case 119:	
				if (IsPet() && ((GetRace() == FIRE_ELEMENTAL) || ((GetRace() == ELEMENTAL) && GetTexture() == 1)))
					return true;
				break;

			case 120:	
				if (GetBodyType() == BT_Undead)
					return true;
				break;

			case 121:	
				if (GetBodyType() != BT_Undead)
					return true;
				break;

			case 122:	
				break;

			case 123:	
				if (GetBodyType() == BT_Humanoid)
					return true;
				break;

			case 124:	
				if ((GetBodyType() == BT_Undead) && (GetHPRatio() < 10.0f))
					return true;
				break;

			case 125:	
				if ((GetRace() == CLOCKWORK_GNOME) && (GetHPRatio() < 10.0f))
					return true;
				break;

			case 126:	
				if ((GetRace() == WISP) && (GetHPRatio() < 10.0f))
					return true;
				break;

			case 201:	
				if (GetHPRatio() > 75.0f)
					return true;
				break;

			case 204:	
				if (GetHPRatio() < 20.0f)
					return true;
				break;

			case 216:	
				if (!IsEngaged())
					return true;
				break;

			case 250:	
				if (GetHPRatio() < 35.0f)
					return true;
				break;

			case 304:	
				if (IsClient() && 
					((GetClass() == WARRIOR) || (GetClass() == BARD) || (GetClass() == SHADOWKNIGHT) || (GetClass() == PALADIN) || (GetClass() == CLERIC)
					 || (GetClass() == RANGER) || (GetClass() == SHAMAN) || (GetClass() == ROGUE)))
					return true;
				break;

			case 701:	
				if (!IsPet())
					return true;
				break;

			case 818:	
				if (GetBodyType() == BT_Undead)
					return true;
				break;

			case 819:	
				if (GetBodyType() != BT_Undead)
					return true;
				break;

			case 842:	
				if (GetBodyType() == BT_Humanoid && GetLevel() <= 84)
					return true;
				break;

			case 843:	
				if (GetBodyType() == BT_Humanoid && GetLevel() <= 86)
					return true;
				break;

			case 844:	
				if (GetBodyType() == BT_Humanoid && GetLevel() <= 88)
					return true;
				break;
		}

		//Limit to amount of pets
		if (value >= 221 && value <= 249){
			int count = hate_list.SummonedPetCount(this);
	
			for (int base2_value = 221; base2_value <= 249; ++base2_value){
				if (value == base2_value){
					if (count >= (base2_value - 220)){
						return true;
					}
				}
			}
		}

		//Limit to Body Type
		if (value >= 600 && value <= 699){
			if (GetBodyType() == (value - 600))
				return true;
		}

		//Limit to Race. *Not implemented on live
		if (value >= 10000 && value <= 11000){
			if (GetRace() == (value - 10000))
				return true;
		}
	} //End Damage

	if (!IsDamage || UseCastRestriction) {
	
		//Heal only if HP within specified range. [Doesn't follow a set forumla for all values...]
		if (value >= 400 && value <= 408){
			for (int base2_value = 400; base2_value <= 408; ++base2_value){
				if (value == base2_value){
																		
					if (value == 400 && GetHPRatio() <= 25)
							return true;

					else if (value == base2_value){
						if (GetHPRatio() > 25+((base2_value - 401)*10) && GetHPRatio() <= 35+((base2_value - 401)*10))
							return true;
					}
				}
			}
		}
						
		else if (value >= 500 && value <= 549){
			for (int base2_value = 500; base2_value <= 520; ++base2_value){
				if (value == base2_value){
					if (GetHPRatio() < (base2_value - 500)*5) 
						return true;
				}
			}
		}

		else if (value == 399) {
			if (GetHPRatio() > 15 && GetHPRatio() <= 25)
				return true;
		}
	} // End Heal
			
						
	return false;
}

