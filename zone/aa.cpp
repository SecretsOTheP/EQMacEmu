/*	EQEMu: Everquest Server Emulator
Copyright (C) 2001-2004 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/classes.h"
#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/eq_packet_structs.h"
#include "../common/races.h"
#include "../common/spdat.h"
#include "../common/strings.h"
#include "../common/zone_store.h"

#include "aa.h"
#include "client.h"
#include "corpse.h"
#include "groups.h"
#include "mob.h"
#include "queryserv.h"
#include "raids.h"
#include "string_ids.h"
#include "titles.h"
#include "zonedb.h"

extern QueryServ* QServ;

AA_DBAction AA_Actions[aaHighestID][MAX_AA_ACTION_RANKS];	//[aaid][rank]
std::map<uint32, SendAA_Struct*>aas_send;
std::map<uint32, std::map<uint32, AA_Ability> > aa_effects;	//stores the effects from the aa_effects table in memory

int Client::GetAATimerID(aaID activate)
{
	SendAA_Struct* aa2 = zone->FindAA(activate, true);

	if (aa2)
		return aa2->spell_type;

	return 0;
}

int Client::CalcAAReuseTimer(const AA_DBAction *caa) {

	if (!caa)
		return 0;

	int ReuseTime = caa->reuse_time;

	if (ReuseTime > 0)
	{
		int ReductionPercentage;

		if (caa->redux_aa > 0 && caa->redux_aa < aaHighestID)
		{
			if (caa->redux_aa == aaRushtoJudgement)
			{
				int redux_ability_level = GetAA(caa->redux_aa);
				ReuseTime = caa->reuse_time - redux_ability_level * 7;
			}
			else if (caa->redux_aa == aaTouchoftheWicked)
			{
				int redux_ability_level = GetAA(caa->redux_aa);
				ReuseTime = caa->reuse_time - redux_ability_level * 720;
			}
			else
			{
				ReductionPercentage = GetAA(caa->redux_aa) * caa->redux_rate;

				if (caa->redux_aa2 > 0 && caa->redux_aa2 < aaHighestID)
					ReductionPercentage += (GetAA(caa->redux_aa2) * caa->redux_rate2);

				ReuseTime = caa->reuse_time * (100 - ReductionPercentage) / 100;
			}
		}

	}
	return ReuseTime;
}

void Client::ActivateAA(aaID aaid)
{
	if (aaid < 0 || aaid >= aaHighestID)
		return;
	if (IsStunned() || IsFeared() || IsMezzed() || IsSilenced() || IsPet())
		return;

	if (playeraction != eaStanding || IsFeigned())
	{
		Message_StringID(Chat::SpellFailure, StringID::MUST_BE_STANDING_TO_CAST);
		return;
	}

	SendAA_Struct* aa2 = zone->FindAA(aaid, true);
	uint8 activate_val = GetAA(aaid);

	if (activate_val == 0)
	{
		return;
	}

	if (aa2)
	{
		if (aa2->account_time_required)
		{
			if ((Timer::GetTimeSeconds() + account_creation) < aa2->account_time_required)
			{
				return;
			}
		}
	}

	if (activate_val > MAX_AA_ACTION_RANKS)
		activate_val = MAX_AA_ACTION_RANKS;
	activate_val--;		//to get array index.
						//get our current node, now that the indices are well bounded
	const AA_DBAction *caa = &AA_Actions[aaid][activate_val];

	int ptimerID = GetAATimerID(aaid) + pTimerAAStart;

	if (!p_timers.Expired(&database, ptimerID))
	{
		uint32 aaremain = p_timers.GetRemainingTime(ptimerID);
		uint32 aaremain_hr = aaremain / (60 * 60);
		uint32 aaremain_min = (aaremain / 60) % 60;
		uint32 aaremain_sec = aaremain % 60;

		if (aa2) 
		{
			// These are strings AA_REUSE_MSG and AA_REUSE_MSG2, but it is easier to create a custom message than 
			// to grab the client's string IDs for the AA name.
			if (aaremain_hr >= 1)
			{
				// 1 hour or more
				Message(Chat::White, "You can use the ability %s again in %u hour(s) %u minute(s) %u seconds.",
					aa2->name, aaremain_hr, aaremain_min, aaremain_sec);
			}
			else
			{
				// less than an hour
				Message(Chat::White, "You can use the ability %s again in %u minute(s) %u seconds.",
					aa2->name, aaremain_min, aaremain_sec);
			}
		}
		return;
	}

	// resolve spell_id (if any)
	uint16 spell_id = IsValidSpell(caa->spell_id) ? caa->spell_id : SPELL_UNKNOWN;

	if (aaid == aaDireCharm)
	{
		//special because spell_id depends on class
		switch (GetClass())
		{
		case Class::Druid:
			spell_id = 2760;
			break;
		case Class::Necromancer:
			spell_id = 2759;
			break;
		case Class::Enchanter:
			spell_id = 2761;
			break;
		}
	}

	// PoP ability Allegiant Familiar replaces the effect of Luclin ability Improved Familiar by altering the spell it casts
	if (aaid == aaImprovedFamiliar && GetAA(aaAllegiantFamiliar))
	{
		spell_id = AA_Actions[aaAllegiantFamiliar][0].spell_id; // this ability has only one level to purchase
	}

	// PoP ability Celestial Renewal replaces the effect of Luclin ability Celestial Regeneration by altering the spell it casts
	if (aaid == aaCelestialRegeneration)
	{
		int upgradeLevel = GetAA(aaCelestialRenewal);
		if (upgradeLevel > 0)
		{
			spell_id = AA_Actions[aaCelestialRenewal][upgradeLevel - 1].spell_id;
		}
	}

	if (IsValidSpell(spell_id) && (IsValidSpell(casting_spell_id) || casting_aa != 0) && aaid != aaBoastfulBellow)
	{
		// already casting something
		return;
	}

	aaTargetType target = caa->target; // resolve aaTargetType to a specific target id
	uint16 target_id = 0;

	//figure out our target
	switch (target)
	{
	case aaTargetUser:
	case aaTargetGroup:
		target_id = GetID();
		break;
	case aaTargetCurrentGroup:
		if (caa->spell_id > 0 && IsValidSpell(caa->spell_id))
		{
			if (GetTarget() != nullptr)
			{
				target_id = GetTarget()->GetID();
			}
			// if this spell doesn't require a target, or if it's an optional target
			// and a target wasn't provided, then it's us
			else if ((IsGroupSpell(caa->spell_id) ||
				spells[caa->spell_id].targettype == ST_Self ||
				spells[caa->spell_id].targettype == ST_AECaster ||
				spells[caa->spell_id].targettype == ST_TargetOptional))
			{
				Log(Logs::Detail, Logs::AA, "AA Spell %d auto-targeted the caster. Group? %d, target type %d", caa->spell_id, IsGroupSpell(caa->spell_id), spells[caa->spell_id].targettype);
				target_id = GetID();
			}
			break;
		}
		// fall through to aaTargetCurrent if this is not a spell casting AA
	case aaTargetCurrent:
		if (GetTarget() == nullptr)
		{
			Message(Chat::DefaultText, "You must first select a target for this ability!");
			return;
		}
		target_id = GetTarget()->GetID();
		break;
	case aaTargetPet:
		if (GetPet() == nullptr)
		{
			Message(Chat::White, "A pet is required for this skill.");
			return;
		}
		target_id = GetPetID();
		SetTarget(GetPet());
		SendTargetCommand(target_id);
		break;
	}

	if (aaid == aaWaketheDead)
	{
		Corpse *corpse = entity_list.GetClosestCorpse(this, nullptr);
		if (!corpse || DistanceSquaredNoZ(GetPosition(), corpse->GetPosition()) > 10000 || !CheckLosFN(corpse, true))
		{
			Message_StringID(Chat::White, StringID::NO_SUITABLE_CORPSE);
			return;
		}
		if (IsClient())
			CastToClient()->wake_corpse_id = corpse->GetID();	// save the corpse to make sure we raise the right one; it might move or a new one appears closer before spell finishes

		char corpse_name[64];
		if (corpse->IsPlayerCorpse())
		{
			strcpy(corpse_name, corpse->GetName());
			EntityList::RemoveNumbers(corpse_name);
		}
		else
		{
			strcpy(corpse_name, corpse->GetCleanName());
		}
		Message_StringID(Chat::White, StringID::YOU_BEGIN_TO_CONCENTRATE, corpse_name);
	}

	// start the reuse timer
	int reuse_timer = CalcAAReuseTimer(caa);
	if (reuse_timer > 0)
	{
		SendAATimer(aaid, static_cast<uint32>(time(nullptr)), static_cast<uint32>(time(nullptr)));
		p_timers.Start(ptimerID, reuse_timer);
	}

	//handle non-spell action
	// note that an aa can have a non-spell action AND a spell_id handled below (only Frenzied Burnout)
	if (caa->action != aaActionNone)
	{
		Log(Logs::General, Logs::AA, "Handling non-spell AA action %d for aa %d", caa->action, aaid);

		// Fading Memories has 900 nonspell_mana in the database, that is the only ability that uses this logic
		if (caa->mana_cost > 0)
		{
			if (GetMana() < caa->mana_cost)
			{
				Message_StringID(Chat::Red, StringID::INSUFFICIENT_MANA);
				return;
			}
			SetMana(GetMana() - caa->mana_cost);
		}

		switch (caa->action)
		{
		case aaActionAETaunt:
			entity_list.AETaunt(this);
			break;

		case aaActionMassBuff:
			EnableAAEffect(aaEffectMassGroupBuff, caa->duration);
			Message_StringID(Chat::Disciplines, StringID::MGB_STRING); // The next group buff you cast will hit all targets in range.
			break;

		case aaActionWarcry:
			WarCry(GetAA(aaWarcry));
			break;

		case aaActionRampage:
			entity_list.AEAttack(this);
			break;

		case aaActionActOfValor:
			if (GetTarget() != nullptr)
			{
				int curhp = GetHP(); // TODO - range check and maybe cast spell 2775 instead of doing this
				GetTarget()->HealDamage(curhp, this);
				Death(this, 0, SPELL_UNKNOWN, EQ::skills::SkillHandtoHand);
			}
			break;

		case aaActionProjectIllusion:
			EnableAAEffect(aaEffectProjectIllusion, caa->duration);
			Message_StringID(Chat::Disciplines, StringID::PROJECT_ILLUSION); // The next illusion spell you cast that changes a player into another character model and not an object will work on the group member you have targeted.
			break;

		case aaActionFrenziedBurnout:
			// Moved to Mob::SpellEffect
			//EnableAAEffect(aaEffectFrenziedBurnout, caa->duration);
			break;

		case aaActionFadingMemories:
		case aaActionEscape:
			Escape();
			break;

		case aaActionPurgePoison:
			if (GetTarget() != nullptr)
			{
				GetTarget()->PurgePoison(this);
			}
			break;

		default:
			Log(Logs::General, Logs::Error, "Unknown AA nonspell action type %d", caa->action);
			break;
		}
	}

	//cast the spell, if we have one
	if (IsValidSpell(spell_id))
	{
		Log(Logs::General, Logs::AA, "Casting spell %d for AA %d", spell_id, aaid);
		if (casting_aa != 0)
		{
			LogAA("casting_aa is not 0. Either we are currently casting another ability, or we didn't clear the value somewhere!");
		}
		casting_aa = aaid;

		if (spell_id != SPELL_AA_BOASTFUL_BELLOW) // special case - this AA ability did not break invis on AK
		{
			CommonBreakInvisible();
		}

		bool cast_success = false;

		bool in_range = true;
		if (caa->target == aaTargetCurrent || caa->target == aaTargetPet)
		{
			Mob* spell_target = entity_list.GetMob(target_id);
			in_range = DoCastingRangeCheck(spell_id, EQ::spells::CastingSlot::Item, spell_target);
		}

		if (in_range == false)
		{
			Message_StringID(Chat::Red, StringID::TARGET_OUT_OF_RANGE);
			cast_success = false;
		}
		else
		{
			// Bards can cast instant cast AAs while they are casting another song - this really only applies to one ability - Boastful Bellow
			if (spells[spell_id].cast_time == 0 && GetClass() == Class::Bard && IsBardSong(casting_spell_id))
			{
				cast_success = SpellFinished(spell_id, entity_list.GetMob(target_id), EQ::spells::CastingSlot::Item, -1, -1, spells[spell_id].ResistDiff, false);
			}
			else
			{
				cast_success = CastSpell(spell_id, target_id, EQ::spells::CastingSlot::Item, -1, -1, 0, -1, ptimerID, reuse_timer, 1);
			}
		}

		if (!cast_success)
		{
			//Reset on failed cast
			ResetAATimer(aaid, StringID::ABILITY_FAILED);
			return;
		}

		if (aaid == aaImprovedHarmTouch || aaid == aaLeechTouch)
		{
			GetPTimers().Start(pTimerHarmTouch, reuse_timer);
			if (HasInstantDisc(SPELL_HARM_TOUCH))
				FadeDisc();
		}
	}
}

NPC *Mob::CreateTemporaryPet(const NPCType *npc_type, uint32 pet_duration_seconds, uint32 target_id, bool followme, bool sticktarg, glm::vec4 position)
{
	// normally npc_types are shared read only and not owned by the NPC 
	// but for swarm pets we're making a copy and will need to delete it in the NPC destructor
	NPCType* npc_type_copy = new NPCType;
	memcpy(npc_type_copy, npc_type, sizeof(NPCType));
	NPC* swarm_pet_npc = new NPC(npc_type_copy, 0, position, GravityBehavior::Water);
	if (swarm_pet_npc->GetRace() != Race::EyeOfZomm)
	{
		swarm_pet_npc->SetOwnerID(GetID());
		//swarm_pet_npc->SetSummonedClientPet(IsClient());
	}
	swarm_pet_npc->SetIsTempPet(true);
	swarm_pet_npc->SetSummonerID(GetID());
	

	// have to hardcode this because NPC skills are hardcoded.  Xuzl pets don't bash/kick
	if (GetClass() == Class::Wizard) {
		swarm_pet_npc->SetSkill(EQ::skills::SkillBash, 0);
		swarm_pet_npc->SetSkill(EQ::skills::SkillKick, 0);
	}

	if (followme)
		swarm_pet_npc->SetFollowID(GetID());

	if (!swarm_pet_npc->GetSwarmInfo()) {
		auto nSI = new SwarmPet;
		swarm_pet_npc->SetSwarmInfo(nSI);
		swarm_pet_npc->GetSwarmInfo()->duration = new Timer(pet_duration_seconds * 1000);
	}
	else {
		swarm_pet_npc->GetSwarmInfo()->duration->Start(pet_duration_seconds * 1000);
	}

	swarm_pet_npc->StartSwarmTimer(pet_duration_seconds * 1000);

	//removing this prevents the pet from attacking
	swarm_pet_npc->GetSwarmInfo()->owner_id = GetID();

	//give the pets somebody to "love"
	Mob* target = entity_list.GetMob(target_id);
	if (target != nullptr) {
		swarm_pet_npc->AddToHateList(target, 1000, 1000);
		if (RuleB(Spells, SwarmPetTargetLock) || sticktarg)
			swarm_pet_npc->GetSwarmInfo()->target = target->GetID();
		else
			swarm_pet_npc->GetSwarmInfo()->target = 0;
	}

	if (swarm_pet_npc->GetRace() == Race::EyeOfZomm)
	{
		swarm_pet_npc->iszomm = true;
		swarm_pet_npc->SetNPCFactionID(0);
		swarm_pet_npc->flymode = GravityBehavior::Ground;
		swarm_pet_npc->pAIControlled = false;

		// Zomm always spawns in front of the caster.
		float zomm_x = GetX(), zomm_y = GetY();
		GetPushHeadingMod(this, 7, zomm_x, zomm_y);
		auto zommPos = glm::vec4(zomm_x, zomm_y, GetZ(), GetHeading());
		swarm_pet_npc->SetPosition(zommPos);
	}

	swarm_pet_npc->AddLootTable();
	swarm_pet_npc->UpdateEquipmentLight();

	entity_list.AddNPC(swarm_pet_npc, true, true);

	return swarm_pet_npc;
}

void Mob::TemporaryPets(uint16 spell_id, Mob *targ, const char *name_override, uint32 duration_override, bool followme, bool sticktarg)
{
	//It might not be a bad idea to put these into the database, eventually..

	//Dook- swarms and wards

	if (temporary_pets_effect)
	{
		// there is already a swarm going, can't start another
		return;
	}

	PetRecord record;
	if (!database.GetPetEntry(spells[spell_id].teleport_zone, &record))
	{
		LogError("Unknown swarm pet spell id: {}, check pets table", spell_id);
		Message(Chat::Red, "Unable to find data for pet %s", spells[spell_id].teleport_zone);
		return;
	}

	TemporaryPetsEffect *pet = new TemporaryPetsEffect;

	// create new npc type
	const NPCType* npc_type_template = database.LoadNPCTypesData(record.npc_type);
	if (npc_type_template == nullptr)
	{
		//log write
		LogError("Unknown npc type for swarm pet spell id: [{}]", spell_id);
		Message(Chat::White, "Unable to find pet!");
		return;
	}
	memcpy(&pet->npc_type, npc_type_template, sizeof(NPCType));
	if (name_override)
	{
		strcpy(pet->npc_type.name, name_override);
	}

	// pet count and duration
	pet->pet_count = 1;
	pet->pet_duration_seconds = 1;
	if (spells[spell_id].effectid[0] == SE_TemporaryPets)
	{
		pet->pet_count = spells[spell_id].base[0]; // swarm pet spells all have this in slot 0
		pet->pet_duration_seconds = spells[spell_id].max[0];
	}
	if (duration_override)
	{
		pet->pet_duration_seconds = duration_override;
	}

	pet->pet_target_id = targ ? targ->GetID() : 0;
	pet->followme = followme;
	pet->sticktarg = sticktarg;

	// Wake The Dead
	if (!strcmp(spells[spell_id].teleport_zone, "animateDead"))
	{
		Corpse* corpse;
		
		if (IsClient())
			corpse = entity_list.GetCorpseByID(CastToClient()->wake_corpse_id);
		else
			corpse = entity_list.GetClosestCorpse(this, nullptr);

		if (!corpse || DistanceSquaredNoZ(GetPosition(), corpse->GetPosition()) > 10000 || !CheckLosFN(corpse, true))
		{
			Message_StringID(Chat::White, StringID::NO_SUITABLE_CORPSE);
			if (IsClient())
			{
				CastToClient()->ResetAATimer(aaWaketheDead, StringID::ABILITY_FAILED);
			}
		}
		else
		{
			CopyWakeCorpse(&pet->npc_type, corpse);
			NPC *wakePet = CreateTemporaryPet(&pet->npc_type, pet->pet_duration_seconds, pet->pet_target_id, pet->followme, pet->sticktarg, corpse->GetPosition());

			//gear stuff, need to make sure there's
			//no situation where this stuff can be duped
			for (int x = EQ::invslot::EQUIPMENT_BEGIN; x <= EQ::invslot::EQUIPMENT_END; x++) // (< 21) added MainAmmo
			{
				uint32 sitem = 0;
				sitem = corpse->GetWornItem(x);
				if (sitem) {
					const EQ::ItemData *itm = database.GetItem(sitem);
					auto loot_drop_entry = LootdropEntriesRepository::NewNpcEntity();
					loot_drop_entry.item_charges = 1;
					wakePet->AddLootDrop(itm, loot_drop_entry, true, true);
				}
			}

			char corpse_name[64];
			if (corpse->IsPlayerCorpse())
			{
				strcpy(corpse_name, corpse->GetName());
				EntityList::RemoveNumbers(corpse_name);
			}
			else
			{
				strcpy(corpse_name, corpse->GetCleanName());
			}
			entity_list.MessageClose_StringID(this, false, 100.0f, Chat::Disciplines, StringID::RISES_TO_SERVE, corpse_name, GetCleanName());
		}

		if (IsClient())
			CastToClient()->wake_corpse_id = 0;
		delete pet;
	}
	else if (pet->pet_count > 0)
	{
		CreateTemporaryPet(&pet->npc_type, pet->pet_duration_seconds, pet->pet_target_id, pet->followme, pet->sticktarg, GetPosition());
		if (pet->pet_count == 1)
		{
			delete pet;
		}
		else
		{
			pet->pet_count_remaining = pet->pet_count - 1;
			this->temporary_pets_effect = pet;
			this->temporary_pets_effect->next_spawn_timer.Start(500); // gets handled in Mob::SpellProcess()
		}
	}
}

void Mob::CopyWakeCorpse(NPCType *make_npc, Corpse *CorpseToUse)
{
	//combat stats
	make_npc->AC = 200;
	make_npc->ATK = 0;
	make_npc->max_dmg = 79;
	make_npc->min_dmg = 18;
	make_npc->attack_delay = 28;

	//base stats
	make_npc->cur_hp = (GetLevel() * 25);
	make_npc->max_hp = (GetLevel() * 25);
	make_npc->STR = 85 + GetLevel();
	make_npc->STA = 85 + GetLevel();
	make_npc->DEX = 85 + GetLevel();
	make_npc->AGI = 85 + GetLevel();
	make_npc->INT = 85 + GetLevel();
	make_npc->WIS = 85 + GetLevel();
	make_npc->CHA = 85 + GetLevel();
	make_npc->MR = 25;
	make_npc->FR = 25;
	make_npc->CR = 25;
	make_npc->DR = 25;
	make_npc->PR = 25;

	//level class and gender
	make_npc->level = GetLevel();
	make_npc->class_ = CorpseToUse->class_;
	make_npc->race = CorpseToUse->race;
	make_npc->gender = CorpseToUse->gender;
	make_npc->loottable_id = 0;

	//appearance
	make_npc->beard = CorpseToUse->beard;
	make_npc->beardcolor = CorpseToUse->beardcolor;
	make_npc->eyecolor1 = CorpseToUse->eyecolor1;
	make_npc->eyecolor2 = CorpseToUse->eyecolor2;
	make_npc->haircolor = CorpseToUse->haircolor;
	make_npc->hairstyle = CorpseToUse->hairstyle;
	make_npc->helmtexture = CorpseToUse->helmtexture;
	make_npc->luclinface = CorpseToUse->luclinface;
	make_npc->size = CorpseToUse->size;
	make_npc->texture = CorpseToUse->texture;

	//cast stuff.. based off of PEQ's if you want to change
	//it you'll have to mod this code, but most likely
	//most people will be using PEQ style for the first
	//part of their spell list; can't think of any smooth
	//way to do this
	//some basic combat mods here too since it's convienent
	strcpy(make_npc->special_abilities, "14,1^7,1^10,1^17,1^21,1"); // UNCHARMABLE, INNATE_DUAL_WIELD, SPECATK_MAGICAL, UNFEARABLE, IMMUNE_FLEEING
	switch (CorpseToUse->class_)
	{
	case Class::Cleric:
		make_npc->npc_spells_id = 1;
		break;
	case Class::Wizard:
		make_npc->npc_spells_id = 2;
		break;
	case Class::Necromancer:
		make_npc->npc_spells_id = 3;
		break;
	case Class::Magician:
		make_npc->npc_spells_id = 4;
		break;
	case Class::Enchanter:
		make_npc->npc_spells_id = 5;
		break;
	case Class::Shaman:
		make_npc->npc_spells_id = 6;
		break;
	case Class::Druid:
		make_npc->npc_spells_id = 7;
		break;
	case Class::Paladin:
		make_npc->cur_hp = make_npc->cur_hp * 150 / 100;
		make_npc->max_hp = make_npc->max_hp * 150 / 100;
		make_npc->npc_spells_id = 8;
		break;
	case Class::ShadowKnight:
		make_npc->cur_hp = make_npc->cur_hp * 150 / 100;
		make_npc->max_hp = make_npc->max_hp * 150 / 100;
		make_npc->npc_spells_id = 9;
		break;
	case Class::Ranger:
		make_npc->cur_hp = make_npc->cur_hp * 135 / 100;
		make_npc->max_hp = make_npc->max_hp * 135 / 100;
		make_npc->npc_spells_id = 10;
		break;
	case Class::Bard:
		make_npc->cur_hp = make_npc->cur_hp * 110 / 100;
		make_npc->max_hp = make_npc->max_hp * 110 / 100;
		make_npc->npc_spells_id = 11;
		break;
	case Class::Beastlord:
		make_npc->cur_hp = make_npc->cur_hp * 110 / 100;
		make_npc->max_hp = make_npc->max_hp * 110 / 100;
		make_npc->npc_spells_id = 12;
		break;
	case Class::Rogue:
		strcat(make_npc->special_abilities, "^2,1"); // SPECATK_ENRAGE
		make_npc->max_dmg = make_npc->max_dmg * 150 / 100;
		make_npc->cur_hp = make_npc->cur_hp * 110 / 100;
		make_npc->max_hp = make_npc->max_hp * 110 / 100;
		break;
	case Class::Monk:
		strcat(make_npc->special_abilities, "^2,1"); // SPECATK_ENRAGE
		make_npc->max_dmg = make_npc->max_dmg * 150 / 100;
		make_npc->cur_hp = make_npc->cur_hp * 135 / 100;
		make_npc->max_hp = make_npc->max_hp * 135 / 100;
		break;
	case Class::Warrior:
		strcat(make_npc->special_abilities, "^2,1"); // SPECATK_ENRAGE
		make_npc->max_dmg = make_npc->max_dmg * 150 / 100;
		make_npc->cur_hp = make_npc->cur_hp * 175 / 100;
		make_npc->max_hp = make_npc->max_hp * 175 / 100;
		break;
	default:
		make_npc->npc_spells_id = 0;
		break;
	}

	make_npc->loottable_id = 0;
	make_npc->merchanttype = 0;
	make_npc->d_melee_texture1 = 0;
	make_npc->d_melee_texture2 = 0;
}

//turn on an AA effect
//duration == 0 means no time limit, used for one-shot deals, etc..
void Client::EnableAAEffect(aaEffectType type, uint32 duration) {
	if (type > 32)
		return;	//for now, special logic needed.
	m_epp.aa_effects |= 1 << (type - 1);

	if (duration > 0) {
		p_timers.Start(pTimerAAEffectStart + type, duration);
	}
	else {
		p_timers.Clear(&database, pTimerAAEffectStart + type);
	}
}

void Client::DisableAAEffect(aaEffectType type) {
	if (type > 32)
		return;	//for now, special logic needed.
	uint32 bit = 1 << (type - 1);
	if (m_epp.aa_effects & bit) {
		m_epp.aa_effects ^= bit;
	}
	p_timers.Clear(&database, pTimerAAEffectStart + type);
	
	// some abilities have worn off messages
	switch (type)
	{
	case aaEffectWarcry:
		Message_StringID(Chat::Spells, StringID::WARCRY_FADES);
		break;
	}
}

/*
By default an AA effect is a one shot deal, unless
a duration timer is set.
*/
bool Client::CheckAAEffect(aaEffectType type) {
	if (type > 32)
		return(false);	//for now, special logic needed.
	if (m_epp.aa_effects & (1 << (type - 1))) {	//is effect enabled?
		//has our timer expired?
		if (p_timers.Expired(&database, pTimerAAEffectStart + type)) {
			DisableAAEffect(type);
			return(false);
		}
		return(true);
	}
	return(false);
}

void Client::FadeAllAAEffects()
{
	m_epp.aa_effects = 0;
	for (int i = 1; i < 32; i++) // aaEffectType uses bits in a uint32 to track active abilities
	{
		p_timers.Clear(&database, pTimerAAEffectStart + i);
	}
}

void Client::SendAAStats() {
	auto outapp = new EQApplicationPacket(OP_AAExpUpdate, sizeof(AltAdvStats_Struct));
	AltAdvStats_Struct *aps = (AltAdvStats_Struct *)outapp->pBuffer;
	aps->experience = m_pp.expAA;
	aps->experience = (uint32)(((float)330.0f * (float)m_pp.expAA) / (float)max_AAXP);
	aps->unspent = m_pp.aapoints;
	aps->percentage = m_epp.perAA;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::BuyAA(AA_Action* action)
{
	LogAADetail("Starting to buy AA [{}]", action->ability);

	//find the AA information from the database
	SendAA_Struct* aa2 = zone->FindAA(action->ability, true);
	if (aa2 == nullptr)
		return;	//invalid ability...

	if (aa2->special_category == 1 || aa2->special_category == 2)
		return; // Not purchasable progression style AAs

	if (aa2->special_category == 8 && aa2->cost == 0)
		return; // Not purchasable racial AAs(set a cost to make them purchasable)

	uint32 cur_level = GetAA(aa2->id);
	if ((aa2->id + cur_level) != action->ability) { //got invalid AA
		LogAADetail("Unable to find or match AA [{}] (found [{}] + lvl [{}])", action->ability, aa2->id, cur_level);
		return;
	}

	if (aa2->account_time_required)
	{
		if ((Timer::GetTimeSeconds() - account_creation) < aa2->account_time_required)
		{
			return;
		}
	}

	uint32 real_cost;
	real_cost = aa2->cost + (aa2->cost_inc * cur_level);

	if (m_pp.aapoints >= real_cost && cur_level < aa2->max_level) {
		SetAA(aa2->id, cur_level + 1);

		LogAADetail("Set AA [{}] to level [{}]", aa2->id, cur_level + 1);

		m_pp.aapoints -= real_cost;

		/* Do Player Profile rank calculations and set player profile */
		SaveAA();
		/* Save to Database to avoid having to write the whole AA array to the profile, only write changes*/
		// database.SaveCharacterAA(this->CharacterID(), aa2->id, (cur_level + 1));


		SendAATable();

		/*
		We are building these messages ourself instead of using the stringID to work around patch discrepancies
		these are AA_GAIN_ABILITY (410) & AA_IMPROVE (411), respectively.
		*/
		char aa_type[8];
		/* Initial purchase of an AA ability */
		if (cur_level < 1){
			Message(Chat::Yellow, "You have gained the ability \"%s\" at a cost of %d ability %s.", aa2->name, real_cost, (real_cost>1) ? "points" : "point");
			strncpy(aa_type, "Initial", 8);
		}
		/* Ranked purchase of an AA ability */
		else{
			Message(Chat::Yellow, "You have improved %s %d at a cost of %d ability %s.", aa2->name, cur_level + 1, real_cost, (real_cost > 1) ? "points" : "point");
			strncpy(aa_type, "Ranked", 8);
		}

		/* QS: Player_Log_AA_Purchases */
		if (RuleB(QueryServ, PlayerLogAAPurchases))
		{
			QServ->QSAAPurchases(this->CharacterID(), this->GetZoneID(), aa_type, aa2->name, aa2->id, real_cost);
		}

		SendAAStats();

		CalcBonuses();

		SendAATimers();

		//Bugs client, comment out for now until titles can be worked out.
		//if(title_manager.IsNewAATitleAvailable(m_pp.aapoints_spent, GetBaseClass()))
		//	NotifyNewTitlesAvailable();
	}
}

SwarmPet::SwarmPet()
{
	target = 0;
	owner_id = 0;
	duration = nullptr;
}

SwarmPet::~SwarmPet()
{
	target = 0;
	owner_id = 0;
	safe_delete(duration);
}

Mob *SwarmPet::GetOwner()
{
	return entity_list.GetMobID(owner_id);
}

void Client::SendAATimer(uint32 ability, uint32 begin, uint32 end) {
	auto outapp = new EQApplicationPacket(OP_AAAction, sizeof(UseAA_Struct));
	UseAA_Struct* uaaout = (UseAA_Struct*)outapp->pBuffer;
	uaaout->ability = zone->EmuToEQMacAA(ability);
	uaaout->begin = begin;
	uaaout->end = end;
	QueuePacket(outapp);
	safe_delete(outapp);
}

//sends all AA timers.
void Client::SendAATimers() {
	//we dont use SendAATimer because theres no reason to allocate the EQApplicationPacket every time
	auto outapp = new EQApplicationPacket(OP_AAAction, sizeof(UseAA_Struct));
	UseAA_Struct* uaaout = (UseAA_Struct*)outapp->pBuffer;

	//Al'Kabor sent timers for all the abilities you have, even if they have never been used.
	uint8 macaaid = 0;
	for (uint32 i = 0; i < MAX_PP_AA_ARRAY; i++)
	{
		if (aa[i] && aa[i]->AA > 0)
		{
			SendAA_Struct* aa2 = nullptr;
			aa2 = zone->FindAA(aa[i]->AA, true);

			if (aa2 && aa2->spell_refresh > 0)
			{
				int32 starttime = 0;
				PTimerList::iterator c, e;
				c = p_timers.begin();
				e = p_timers.end();
				for (; c != e; ++c) 
				{
					PersistentTimer *cur = c->second;
					if (cur->GetType() < pTimerAAStart || cur->GetType() > pTimerAAEnd)
					{
						continue;	//not an AA timer
					}
					else if (cur->GetType() == pTimerAAStart + aa2->spell_type)
					{
						starttime = cur->GetStartTime();
						break;
					}
				}
				uaaout->begin = starttime;
				uaaout->end = static_cast<uint32>(time(nullptr));
				uaaout->ability = zone->EmuToEQMacAA(aa2->id);
				QueuePacket(outapp);
				LogAADetail("Sending out timer TYPE [{}] for AA: [{}] ([{}]). Timer start: [{}] Timer end: [{}] Recast Time: [{}]", aa2->spell_type, uaaout->ability, aa2->name, uaaout->begin, uaaout->end, aa2->spell_refresh);
			}
		}
	}

	safe_delete(outapp);
}

void Client::ResetSingleAATimer(aaID activate, uint32 messageid)
{
	LogAA("Activated ability [{}] failed to cast. Resetting timer.", (int)activate);

	ZeroCastingVars();

	uint16 color = messageid == StringID::TOO_DISTRACTED ? Chat::SpellFailure : Chat::Yellow;
	Message_StringID(color, messageid);

	int ptimerID = GetAATimerID(activate) + pTimerAAStart;
	p_timers.Clear(&database, ptimerID);

	auto outapp = new EQApplicationPacket(OP_AAAction, sizeof(UseAA_Struct));
	UseAA_Struct* uaaout = (UseAA_Struct*)outapp->pBuffer;
	uaaout->ability = zone->EmuToEQMacAA(activate);
	uaaout->begin = 0;
	uaaout->end = time(nullptr);
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::ResetAATimer(aaID activate, uint32 messageid)
{
	LogAA("Activated ability [{}] failed to cast. Resetting timer.", (int)activate);

	ZeroCastingVars();

	uint16 color = messageid == StringID::TOO_DISTRACTED ? Chat::SpellFailure : Chat::Yellow;
	Message_StringID(color, messageid);

	int ptimerID = GetAATimerID(activate) + pTimerAAStart;
	p_timers.Clear(&database, ptimerID);

	SendAATimers();
}

void Client::SendAATable() {
	auto outapp = new EQApplicationPacket(OP_RespondAA, sizeof(AATable_Struct));

	AATable_Struct* aa2 = (AATable_Struct *)outapp->pBuffer;
	aa2->unknown = GetAAPointsSpent();

	//EQMac's AAs have to be in order based on its IDs, not EQEmu's
	uint8 macaaid = 0;
	for (uint32 i = 0; i < 226; i++, macaaid = 0){
		if (aa[i]->AA > 0)
			macaaid = zone->EmuToEQMacAA(aa[i]->AA);
		if (macaaid > 0)
		{
			for (int r = 0; r < 226; r++){
				if (macaaid == r + 1)
				{
					aa2->aa_list[r].aa_value = aa[i]->value;
					break;
				}
			}
		}
	}
	aa2->aa_list[211 - 1].aa_value = 3; // Fleet of Foot 3 is granted to all characters all the time, bard or not, to work around a bug in the macintosh client
	QueuePacket(outapp);
	safe_delete(outapp);
}

uint32 Client::GetAA(uint32 aa_id) const {
	//std::map<uint32, uint8>::const_iterator res;
	auto res = aa_points.find(aa_id);
	if (res != aa_points.end()) {
		return(res->second);
	}
	return(0);
}

bool Client::SetAA(uint32 aa_id, uint32 new_value) {
	aa_points[aa_id] = new_value;
	uint32 cur;
	for (cur = 0; cur < MAX_PP_AA_ARRAY; cur++){
		if ((aa[cur]->value > 1) && ((aa[cur]->AA - aa[cur]->value + 1) == aa_id)){
			aa[cur]->value = new_value;
			if (new_value > 0)
				aa[cur]->AA++;
			else
				aa[cur]->AA = 0;
			return true;
		}
		else if ((aa[cur]->value == 1) && (aa[cur]->AA == aa_id)){
			aa[cur]->value = new_value;
			if (new_value > 0)
				aa[cur]->AA++;
			else
				aa[cur]->AA = 0;
			return true;
		}
		else if (aa[cur]->AA == 0){ //end of list
			aa[cur]->AA = aa_id;
			aa[cur]->value = new_value;
			return true;
		}
	}
	return false;
}

SendAA_Struct* Zone::FindAA(uint32 id, bool searchParent) {
	SendAA_Struct *ret = aas_send[id];

	if (!ret && searchParent) 
	{
		// look for a lower level of the ability
		for (uint32 i = 1; !ret && i < MAX_AA_ACTION_RANKS; i++) 
		{
			uint32 try_id = id - i;
			if (try_id <= 0)
				break;

			LogAADetail("Could not find AA [{}], trying potential parent [{}]", id, try_id);
			ret = aas_send[try_id];
		}
	}

	return ret;
}

uint8 Zone::EmuToEQMacAA(uint32 id)
{
	if (id > 0)
	{
		SendAA_Struct* saa = zone->FindAA(id, true);
		if (saa != nullptr)
		{
			return saa->eqmacid;
		}
	}
	
	return 0;
}

void Zone::LoadAlternateAdvancement() {
	LogInfo("Loading AA information...");
	totalAAs = database.CountAAs();
	if (totalAAs == 0) {
		LogError("Failed to load AAs!");
		aas = nullptr;
		return;
	}
	aas = new SendAA_Struct *[totalAAs];

	database.LoadAlternateAdvancement(aas);

	int i;
	for (i = 0; i < totalAAs; i++){
		SendAA_Struct* aa = aas[i];
		aas_send[aa->id] = aa;
	}

	//load AA Effects into aa_effects
	LogInfo("Loading AA Effects...");
	if (database.LoadAlternateAdvancementEffects()) {
		LogInfo("Loaded {} AA Effects.", aa_effects.size());
	}
	else {
		LogError("Failed to load AA Effects!");
	}
}

bool ZoneDatabase::LoadAlternateAdvancementEffects() {
	aa_effects.clear();	//start fresh

	const std::string query = "SELECT aaid, slot, effectid, base1, base2 FROM aa_effects ORDER BY aaid ASC, slot ASC";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	if (!results.RowCount()) { //no results
        LogMySQLError("Error loading AA Effects, none found in the database.");
        return false;
	}

	for (auto row = results.begin(); row != results.end(); ++row) {
		int aaid = atoi(row[0]);
		int slot = atoi(row[1]);
		int effectid = atoi(row[2]);
		int base1 = atoi(row[3]);
		int base2 = atoi(row[4]);
		aa_effects[aaid][slot].skill_id = effectid;
		aa_effects[aaid][slot].base1 = base1;
		aa_effects[aaid][slot].base2 = base2;
		aa_effects[aaid][slot].slot = slot;	//not really needed, but we'll populate it just in case
	}

	return true;
}

uint32 ZoneDatabase::GetMacToEmuAA(uint8 eqmacid) {

	std::string query = StringFormat("SELECT skill_id from altadv_vars where eqmacid=%i", eqmacid);
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0)
	{
		LogErrorDetail("Error in GetMacToEmuAA");
		return 0;
	}

	auto row = results.begin();

	LogDebugDetail("GetMacToEmuAA is returning: [{}]", atoi(row[0]));
	return atoi(row[0]);
}

void Client::ResetAA(){
	RefundAA();
	uint32 i;
	for (i = 0; i<MAX_PP_AA_ARRAY; i++){
		aa[i]->AA = 0;
		aa[i]->value = 0;
		m_pp.aa_array[i].AA = 0;
		m_pp.aa_array[i].value = 0;
	}

	std::map<uint32, uint8>::iterator itr;
	for (itr = aa_points.begin(); itr != aa_points.end(); ++itr)
		aa_points[itr->first] = 0;

	database.DeleteCharacterAAs(this->CharacterID());
	SaveAA();
	SendAATable();
	Kick();
}

void Client::InspectBuffs(Client* Inspector, int Rank)
{
	if (!Inspector || (Rank == 0)) return;

	Inspector->Message_StringID(Chat::White, StringID::CURRENT_SPELL_EFFECTS, GetName());
	uint32 buff_count = GetMaxTotalSlots();
	for (uint32 i = 0; i < buff_count; ++i)
	{
		if (buffs[i].spellid != SPELL_UNKNOWN)
		{
			if (Rank == 1)
				Inspector->Message(Chat::White, "%s", spells[buffs[i].spellid].name);
			else
			{
				if (spells[buffs[i].spellid].buffdurationformula == DF_Permanent)
					Inspector->Message(Chat::White, "%s (Permanent)", spells[buffs[i].spellid].name);
				else {
					auto TempString = fmt::format(" {:.1f} ", static_cast<float>(buffs[i].ticsremaining) / 10.0f);
					Inspector->Message_StringID(Chat::White, StringID::BUFF_MINUTES_REMAINING, spells[buffs[i].spellid].name, TempString.c_str());
				}
			}
		}
	}
}

bool ZoneDatabase::LoadAlternateAdvancementActions() {
	memset(AA_Actions, 0, sizeof(AA_Actions));	//I hope the compiler is smart about this size...

	const std::string query = "SELECT aaid, rank, reuse_time, spell_id, target, "
                            "nonspell_action, nonspell_mana, nonspell_duration, "
                            "redux_aa, redux_rate, redux_aa2, redux_rate2 FROM aa_actions";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return false;
	}

	for (auto row = results.begin(); row != results.end(); ++row) {

		int aaid = atoi(row[0]);
		int rank = atoi(row[1]);
		if (aaid < 0 || aaid >= aaHighestID || rank < 0 || rank >= MAX_AA_ACTION_RANKS)
			continue;
		AA_DBAction *caction = &AA_Actions[aaid][rank];

		caction->reuse_time = atoi(row[2]);
		caction->spell_id = atoi(row[3]);
		caction->target = (aaTargetType)atoi(row[4]);
		caction->action = (aaNonspellAction)atoi(row[5]);
		caction->mana_cost = atoi(row[6]);
		caction->duration = atoi(row[7]);
		caction->redux_aa = (aaID)atoi(row[8]);
		caction->redux_rate = atoi(row[9]);
		caction->redux_aa2 = (aaID)atoi(row[10]);
		caction->redux_rate2 = atoi(row[11]);

	}

	return true;
}

//Returns the number effects an aa.has when we send them to the client
//For the purposes of sizing a packet because every skill does not
//have the same number effects, they can range from none to a few depending on AA.
//counts the # of effects by counting the different slots of an AAID in the DB.

//AndMetal: this may now be obsolete since we have Zone::GetTotalAALevels()
uint8 ZoneDatabase::GetTotalAALevels(uint32 skill_id) {

	std::string query = StringFormat("SELECT count(slot) FROM aa_effects WHERE aaid = %i", skill_id);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return 0;
    }

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();

	return atoi(row[0]);
}

//this will allow us to count the number of effects for an AA by pulling the info from memory instead of the database. hopefully this will same some CPU cycles
uint8 Zone::GetTotalAALevels(uint32 skill_id) {
	size_t sz = aa_effects[skill_id].size();
	return sz >= 255 ? 255 : static_cast<uint8>(sz);
}

uint32 ZoneDatabase::CountAAs(){

	const std::string query = "SELECT count(*) FROM altadv_vars";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();

	return atoi(row[0]);
}

uint32 ZoneDatabase::CountAAEffects() {

	const std::string query = "SELECT count(id) FROM aa_effects";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return 0;
	}

	if (results.RowCount() != 1)
		return 0;

	auto row = results.begin();

	return atoi(row[0]);
}

void ZoneDatabase::LoadAlternateAdvancement(SendAA_Struct **load)
{
	if (!load)
		return;

	std::string query = "SELECT skill_id FROM altadv_vars ORDER BY eqmacid";
	auto results = QueryDatabase(query);
	if (results.Success()) {
		int skill = 0, index = 0;
		for (auto row = results.begin(); row != results.end(); ++row, ++index) {
			skill = atoi(row[0]);
			load[index] = GetAASkillVars(skill);
			load[index]->seq = index + 1;
		}
	}
	else {
	}
}

SendAA_Struct* ZoneDatabase::GetAASkillVars(uint32 skill_id)
{
	std::string query = "SET @row = 0"; //initialize "row" variable in database for next query
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return nullptr;
    }

    query = StringFormat("SELECT a.cost, a.max_level, a.type, "
                        "COALESCE("	//So we can return 0 if it's null.
                        "("	// this is our derived table that has the row #
                            // that we can SELECT from, because the client is stupid.
                        "SELECT p.prereq_index_num "
                        "FROM (SELECT a2.skill_id, @row := @row + 1 AS prereq_index_num "
						"FROM altadv_vars a2) AS p "
                        "WHERE p.skill_id = a.prereq_skill), 0) "
                        "AS prereq_skill_index, a.prereq_minpoints, a.spell_type, a.spell_refresh, a.classes, "
                        "a.spellid, a.class_type, a.name, a.cost_inc, a.aa_expansion, a.special_category, "
                        "a.account_time_required, a.level_inc, a.eqmacid "
                        "FROM altadv_vars a WHERE skill_id=%i", skill_id);
    results = QueryDatabase(query);
    if (!results.Success()) {
        return nullptr;
    }

	if (results.RowCount() != 1)
		return nullptr;

	int total_abilities = GetTotalAALevels(skill_id);	//eventually we'll want to use zone->GetTotalAALevels(skill_id) since it should save queries to the DB
	int totalsize = total_abilities * sizeof(AA_Ability) + sizeof(SendAA_Struct);

	SendAA_Struct* sendaa = nullptr;
	uchar* buffer;

	buffer = new uchar[totalsize];
	memset(buffer, 0, totalsize);
	sendaa = (SendAA_Struct*)buffer;

	auto row = results.begin();

	//ATOI IS NOT UNSIGNED LONG-SAFE!!!

	sendaa->cost = atoul(row[0]);
	sendaa->cost2 = sendaa->cost;
	sendaa->max_level = atoul(row[1]);
	sendaa->id = skill_id;
	sendaa->type = atoul(row[2]);
	sendaa->prereq_skill = atoul(row[3]);
	sendaa->prereq_minpoints = atoul(row[4]); // reference only
	sendaa->spell_type = atoul(row[5]);
	sendaa->spell_refresh = atoul(row[6]);
	sendaa->classes = static_cast<uint16>(atoul(row[7]));
	sendaa->last_id = 0xFFFFFFFF;
	sendaa->current_level = 1;
	sendaa->spellid = atoul(row[8]);
	sendaa->class_type = atoul(row[9]);
	strcpy(sendaa->name, row[10]);

	sendaa->total_abilities = total_abilities;
	if (sendaa->max_level > 1)
		sendaa->next_id = skill_id + 1;
	else
		sendaa->next_id = 0xFFFFFFFF;

	sendaa->cost_inc = atoi(row[11]);
	sendaa->aa_expansion = atoul(row[12]); // reference only
	sendaa->special_category = atoul(row[13]);
	sendaa->account_time_required = atoul(row[14]);
	sendaa->level_inc = static_cast<uint8>(atoul(row[15])); // reference only
	sendaa->eqmacid = static_cast<uint8>(atoul(row[16]));

	return sendaa;
}

uint8 Client::GetAARankTitle()
{
	/*
	 0 = no title
	 1 = general title (baron/baronness) (6 points in general)
	 2 = archtype title (master/brother/veteran/venerable) (12 points in archtype)
	 3 - class title (muse/marshall/sage/duke) (24 points in class)
	*/

	int general_points = 0;
	int archtype_points = 0;
	int class_points = 0;

	if (this->aa)
	{
		for (int i = 0; i < MAX_PP_AA_ARRAY; i++)
		{
			if (this->aa[i] && this->aa[i]->AA)
			{
				// locate the aa data for this - search for the base parent ability id, for abilities we have more than one level of
				SendAA_Struct *aadata = zone->FindAA(this->aa[i]->AA, true);

				if
				(
					aadata &&
					!(aadata->special_category == 1 || aadata->special_category == 2) && // Not purchasable progression style AAs
					!(aadata->special_category == 8 && aadata->cost == 0) // Not purchasable racial AAs(set a cost to make them purchasable)
				)
				{
					// get the current level of this AA
					uint32 cur_level = GetAA(aadata->id);

					// the way this works, the next level of the ability is the base id + 1, so this just verifies the FindAA() found the correct base ability id
					if ((aadata->id + (cur_level - 1)) != this->aa[i]->AA) 
					{ 
						//got invalid AA
						LogAADetail("Unable to find or match AA [{}] (found [{}] + lvl [{}])", this->aa[i]->AA, aadata->id, cur_level);
						continue;
					}

					int points_spent = 0; // spent on this ability
										  
					// count the points spent on each level
					for (int j = 0; j < cur_level; j++)
					{
						uint32 real_cost;
						real_cost = aadata->cost + (aadata->cost_inc * j);

						points_spent += real_cost;
					}

					switch (aadata->type)
					{
						case 1: // general
							general_points += points_spent;
							break;
						case 2: // archtype
							archtype_points += points_spent;
							break;
						case 3: // class
							class_points += points_spent;
							break;
					}

					//Message(Chat::Yellow, "i = %d, name = %s, aa[i]->AA = %u, base id = %u, aa[i]->value = %u, cost = %u, type = %u - points spent: %u", i, aadata->name, aa[i]->AA, aadata->id, aa[i]->value, aadata->cost, aadata->type, points_spent);
				}
			}
		}
	}

	if (class_points >= 24)
		return 3;
	if (archtype_points >= 12)
		return 2;
	if (general_points >= 6)
		return 1;

	return 0;
}

// copied from ActivateAA().  used for lua scripts that need to disable player AAs
void Client::ExpendAATimer(int aaid_int)
{
	if (aaid_int <= 0 || aaid_int >= aaHighestID)
		return;
	aaID aaid = (aaID)aaid_int;
	uint8 activate_val = GetAA(aaid);

	if (activate_val == 0)
	{
		return;
	}
	if (activate_val > MAX_AA_ACTION_RANKS)
		activate_val = MAX_AA_ACTION_RANKS;
	activate_val--;

	const AA_DBAction* caa = &AA_Actions[aaid][activate_val];

	int ptimerID = GetAATimerID(aaid) + pTimerAAStart;

	int reuse_timer = CalcAAReuseTimer(caa);
	if (reuse_timer > 0)
	{
		SendAATimer(aaid, static_cast<uint32>(time(nullptr)), static_cast<uint32>(time(nullptr)));
		p_timers.Start(ptimerID, reuse_timer);
	}
}