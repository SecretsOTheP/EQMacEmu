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

#include "../common/bodytypes.h"
#include "../common/classes.h"
#include "../common/global_define.h"
#include "../common/misc_functions.h"
#include "../common/rulesys.h"
#include "../common/seperator.h"
#include "../common/spdat.h"
#include "../common/strings.h"
#include "../common/emu_versions.h"
#include "../common/features.h"    
#include "../common/item_instance.h"        
#include "../common/item_data.h" 
#include "../common/linked_list.h" 
#include "../common/servertalk.h"
#include "../common/fastmath.h"

#include "aa.h"
#include "client.h"
#include "entity.h"
#include "guild_mgr.h"
#include "npc.h"
#include "string_ids.h"
#include "spawn2.h"
#include "zone.h"
#include "water_map.h"
#include "quest_parser_collection.h"

#include <glm/gtx/projection.hpp>

#include <cctype>
#include <stdio.h>
#include <string>

#ifdef _WINDOWS
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else
#include <stdlib.h>
#include <pthread.h>
#endif

extern Zone* zone;
extern volatile bool is_zone_loaded;
extern EntityList entity_list;
extern FastMath g_Math;

NPC::NPC(const NPCType *npc_type_data, Spawn2* in_respawn, const glm::vec4& position, GravityBehavior iflymode, bool IsCorpse)
	: Mob(
		npc_type_data->name,
		npc_type_data->lastname,
		npc_type_data->max_hp,
		npc_type_data->max_hp,
		npc_type_data->gender,
		npc_type_data->race,
		npc_type_data->class_,
		(bodyType)npc_type_data->bodytype,
		npc_type_data->deity,
		npc_type_data->level,
		npc_type_data->npc_id,
		npc_type_data->size,
		npc_type_data->runspeed,
		position,
		npc_type_data->light, // innate_light
		npc_type_data->texture,
		npc_type_data->helmtexture,
		npc_type_data->AC,
		npc_type_data->ATK,
		npc_type_data->STR,
		npc_type_data->STA,
		npc_type_data->DEX,
		npc_type_data->AGI,
		npc_type_data->INT,
		npc_type_data->WIS,
		npc_type_data->CHA,
		npc_type_data->haircolor,
		npc_type_data->beardcolor,
		npc_type_data->eyecolor1,
		npc_type_data->eyecolor2,
		npc_type_data->hairstyle,
		npc_type_data->luclinface,
		npc_type_data->beard,
		npc_type_data->armor_tint,
		0,
		npc_type_data->see_invis,			// pass see_invis/see_ivu flags to mob constructor
		npc_type_data->see_invis_undead,
		npc_type_data->see_sneak,
		npc_type_data->see_improved_hide,
		npc_type_data->hp_regen,
		npc_type_data->mana_regen,
		npc_type_data->qglobal,
		npc_type_data->maxlevel,
		npc_type_data->scalerate,
		npc_type_data->armtexture,
		npc_type_data->bracertexture,
		npc_type_data->handtexture,
		npc_type_data->legtexture,
		npc_type_data->feettexture,
		npc_type_data->chesttexture
	),
	attacked_timer(CombatEventTimer_expire),
	swarm_timer(100),
	classattack_timer(1000),
	knightattack_timer(1000),
	call_help_timer(AIassistcheck_delay),
	qglobal_purge_timer(30000),
	push_timer(500),
	sendhpupdate_timer(1000),
	enraged_timer(1000),
	taunt_timer(TauntReuseTime * 1000),
	despawn_timer(1000),
	m_SpawnPoint(position),
	m_GuardPoint(-1.0f, -1.0f, -1.0f, 0.0f),
	m_GuardPointSaved(0.0f, 0.0f, 0.0f, 0.0f)
{
	//What is the point of this, since the names get mangled..
	Mob* mob = entity_list.GetMob(name);
	if (mob != nullptr) {
		entity_list.RemoveEntity(mob->GetID());
	}
	loot_lockout_timer = 0;
	instance_spawn_timer_override = 0;
	memset(&NPCTypedata, 0, sizeof(NPCTypedata));
	memcpy(&NPCTypedata, npc_type_data, sizeof(NPCTypedata));
	respawn2 = in_respawn;
	swarm_timer.Disable();

	taunting = false;
	proximity = nullptr;
	m_loot_copper = 0;
	m_loot_silver = 0;
	m_loot_gold = 0;
	m_loot_platinum = 0;
	max_dmg = npc_type_data->max_dmg;
	min_dmg = npc_type_data->min_dmg;
	attack_count = npc_type_data->attack_count;
	grid = 0;
	max_wp = 0;
	save_wp = 0;
	spawn_group = 0;
	swarmInfoPtr = nullptr;
	guild_fte = GUILD_NONE;
	spellscale = npc_type_data->spellscale;
	healscale = npc_type_data->healscale;

	pAggroRange = npc_type_data->aggroradius;
	pAssistRange = npc_type_data->assistradius;

	MR = npc_type_data->MR;
	CR = npc_type_data->CR;
	DR = npc_type_data->DR;
	FR = npc_type_data->FR;
	PR = npc_type_data->PR;

	STR = npc_type_data->STR;
	STA = npc_type_data->STA;
	AGI = npc_type_data->AGI;
	DEX = npc_type_data->DEX;
	INT = npc_type_data->INT;
	WIS = npc_type_data->WIS;
	CHA = npc_type_data->CHA;
	npc_mana = npc_type_data->Mana;



	//quick fix of ordering if they screwed it up in the DB
	if (max_dmg < min_dmg) {
		int tmp = min_dmg;
		min_dmg = max_dmg;
		max_dmg = tmp;
	}

	// Max Level and Stat Scaling if maxlevel is set
	if (maxlevel > level)
	{
		LevelScale();
	}

	// Set Resists if they are 0 in the DB
	CalcNPCResists();

	// Set Mana and HP Regen Rates if they are 0 in the DB
	CalcNPCRegen();

	// Set Min and Max Damage if they are 0 in the DB
	if (max_dmg == 0) {
		CalcNPCDamage();
	}

	accuracy_rating = npc_type_data->accuracy_rating;
	ATK = npc_type_data->ATK;

	loot_lockout_timer = npc_type_data->loot_lockout;

	instance_spawn_timer_override = npc_type_data->instance_spawn_timer_override;

	CalcMaxMana();
	SetMana(GetMaxMana());

	MerchantType = npc_type_data->merchanttype;
	merchant_open = GetClass() == MERCHANT;
	if (MerchantType > 0 && GetClass() == MERCHANT)
	{
		zone->ResetMerchantQuantity(MerchantType);
	}
	flymode = iflymode;
	if (npc_type_data->flymode >= 0) {
		flymode = static_cast<GravityBehavior>(npc_type_data->flymode);
	}
	guard_anim = eaStanding;
	roambox_distance = false;
	roambox_max_x = -2;
	roambox_max_y = -2;
	roambox_min_x = -2;
	roambox_min_y = -2;
	roambox_movingto_x = -2;
	roambox_movingto_y = -2;
	roambox_movingto_z = -2;
	roambox_min_delay = 1000;
	roambox_delay = 1000;
	p_depop = false;
	m_loottable_id = npc_type_data->loottable_id;
	m_skip_global_loot = npc_type_data->skip_global_loot;
	rare_spawn = npc_type_data->rare_spawn;
	primary_faction = 0;
	SetNPCFactionID(npc_type_data->npc_faction_id);
	SetPreCharmNPCFactionID(npc_type_data->npc_faction_id);

	npc_spells_id = 0;
	HasAISpell = false;
	HasAISpellEffects = false;
	innateProcSpellId = 0;
	innateProcChance = 0;
	hasZeroPrioritySpells = false;

	SpellFocusDMG = 0;
	SpellFocusHeal = 0;

	pet_spell_id = 0;

	combat_event = false;
	attack_delay = npc_type_data->attack_delay;
	slow_mitigation = npc_type_data->slow_mitigation;

	entity_list.MakeNameUnique(name);

	npc_aggro = npc_type_data->npc_aggro;

	d_melee_texture1 = npc_type_data->d_melee_texture1;
	d_melee_texture2 = npc_type_data->d_melee_texture2;
	memset(equipment, 0, sizeof(equipment));
	prim_melee_type = npc_type_data->prim_melee_type;
	sec_melee_type = npc_type_data->sec_melee_type;
	ranged_type = npc_type_data->ranged_type;

	// If Melee Textures are not set, set attack type to Hand to Hand as default
	if (!d_melee_texture1)
		prim_melee_type = 28;
	if (!d_melee_texture2)
		sec_melee_type = 28;

	//give NPCs skill values...
	int r;
	for (r = 0; r <= EQ::skills::HIGHEST_SKILL; r++) {
		skills[r] = database.GetSkillCap(GetClass(), (EQ::skills::SkillType)r, level);
	}

	// NPCs get skills at levels earlier than Clients.  NPCs also ignore class skill availabilty restrictions and skill caps
	// melee routines use these; modifying will affect NPC performance significantly.  See http://www.eqemulator.org/forums/showthread.php?t=38708
	// ideally we would softcode/use database for NPC skills, but hardcoding this for now
	uint16 skillLevel = level * 5;
	if (level > 50)
		skillLevel = 250;
	else
		skillLevel = std::min(level * 5, 210);

	uint16 weaponSkill = skillLevel;
	if (zone->GetZoneExpansion() == PlanesEQ)
	{
		if (level > 50)
			weaponSkill = 250 + level;
		else
			weaponSkill = std::min(level * 6, 275);
	}

	skills[EQ::skills::SkillHandtoHand] = weaponSkill;
	skills[EQ::skills::Skill1HBlunt] = weaponSkill;
	skills[EQ::skills::Skill2HBlunt] = weaponSkill;
	skills[EQ::skills::Skill1HSlashing] = weaponSkill;
	skills[EQ::skills::Skill2HSlashing] = weaponSkill;
	skills[EQ::skills::SkillArchery] = weaponSkill;
	skills[EQ::skills::SkillDefense] = weaponSkill;
	skills[EQ::skills::SkillOffense] = weaponSkill;
	skills[EQ::skills::Skill1HPiercing] = weaponSkill;

	skills[EQ::skills::SkillDoubleAttack] = level > 7 ? skillLevel : 0;
	skills[EQ::skills::SkillDualWield] = level > 7 ? skillLevel : 0;
	uint16 bashSkill = level > 5 ? skillLevel : 0;
	uint16 kickSkill = level > 15 ? skillLevel : 0;

	if (level < 51)
		skillLevel = std::min(level * 5, 225);

	uint16 parrySkill   = level >  6 ? std::min(level * 5, 230) : 0;
	uint16 riposteSkill = level > 11 ? std::min(level * 5, 225) : 0;
	skills[EQ::skills::SkillDodge]  = level > 10 ? std::min(level * 5, 190) : 0;
	skills[EQ::skills::SkillRiposte] = 0;
	skills[EQ::skills::SkillParry] = 0;

	if (class_ == WARRIOR || class_ == WARRIORGM || GetSpecialAbility(USE_WARRIOR_SKILLS))
	{
		skills[EQ::skills::SkillBash] = bashSkill;
		skills[EQ::skills::SkillKick] = kickSkill;
		skills[EQ::skills::SkillRiposte] = riposteSkill;
		skills[EQ::skills::SkillParry] = parrySkill;
	}

	switch (class_)
	{
		case PALADIN: case PALADINGM:
		case SHADOWKNIGHT: case SHADOWKNIGHTGM:
		{
			skills[EQ::skills::SkillBash] = bashSkill;
			skills[EQ::skills::SkillRiposte] = riposteSkill;
			skills[EQ::skills::SkillParry] = parrySkill;
			break;
		}
		case RANGER: case RANGERGM:
		{
			skills[EQ::skills::SkillKick] = kickSkill;
			skills[EQ::skills::SkillRiposte] = riposteSkill;
			skills[EQ::skills::SkillParry] = parrySkill;
			break;
		}
		case CLERIC: case CLERICGM:
		{
			skills[EQ::skills::SkillBash] = bashSkill;
			break;
		}
		case ROGUE: case ROGUEGM:
		{
			skills[EQ::skills::SkillBackstab] = level > 9 ? skillLevel : 0;
			skills[EQ::skills::SkillRiposte] = riposteSkill;
			skills[EQ::skills::SkillParry] = parrySkill;
			break;
		}
		case MONK: case MONKGM:
		{
			skills[EQ::skills::SkillBlock] = level > 11 ? skillLevel : 0;
			skills[EQ::skills::SkillDodge] = level > 10 ? std::min(level * 5, 230) : 0;
			skills[EQ::skills::SkillRiposte] = riposteSkill;
			skills[EQ::skills::SkillRoundKick] = level > 4 ? skillLevel : 0;
			skills[EQ::skills::SkillTigerClaw] = level > 9 ? skillLevel : 0;
			skills[EQ::skills::SkillEagleStrike] = level > 19 ? skillLevel : 0;
			skills[EQ::skills::SkillDragonPunch] = level > 24 ? skillLevel : 0;
			skills[EQ::skills::SkillFlyingKick] = level > 29 ? skillLevel : 0;
			break;
		}
		case BARD: case BARDGM:
		{
			skills[EQ::skills::SkillRiposte] = riposteSkill;
			skills[EQ::skills::SkillParry] = parrySkill;
			break;
		}
	}

	reface_timer = new Timer(15000);
	reface_timer->Disable();
	qGlobals = nullptr;
	SetEmoteID(static_cast<uint32>(npc_type_data->emoteid));
	if (npc_type_data->walkspeed > 0.0f)
		SetWalkSpeed(npc_type_data->walkspeed);
	SetCombatHPRegen(npc_type_data->combat_hp_regen);
	SetCombatManaRegen(npc_type_data->combat_mana_regen);

	InitializeBuffSlots();
	CalcBonuses();
	raid_target = npc_type_data->raid_target;
	ignore_distance = npc_type_data->ignore_distance;
	base_texture = npc_type_data->texture;
	base_size = npc_type_data->size;
	ignore_despawn = npc_type_data->ignore_despawn;

	underwater = npc_type_data->underwater;
	private_corpse = npc_type_data->private_corpse;
	aggro_pc = npc_type_data->aggro_pc;
	shop_count = 0;
	raid_fte = 0;
	group_fte = 0;
	fte_charid = 0;
	solo_raid_fte = 0;
	solo_group_fte = 0;
	solo_fte_charid = 0;
	bonusAvoidance = npc_type_data->avoidance;
	exp_pct = npc_type_data->exp_pct;
	push_vector = glm::vec3(0.0f, 0.0f, 0.0f);
	wall_normal1_x = wall_normal1_y = wall_normal2_x = wall_normal2_y = corner_x = corner_y = 0.0f;
	greed = npc_type_data->greed;
	engage_notice = npc_type_data->engage_notice;
	flymode = iflymode;
	stuck_behavior = npc_type_data->stuck_behavior;
	noQuestPause = false;
	assisting = false;
	pbaoe_damage = 0;
	AI_Start();
}

NPC::~NPC()
{
	AI_Stop();
	quest_manager.stopalltimers(this);

	if(proximity != nullptr) {
		entity_list.RemoveProximity(GetID());
		safe_delete(proximity);
	}

	{
	m_loot_items.clear();
	}

	{
	std::list<struct NPCFaction*>::iterator cur,end;
	cur = faction_list.begin();
	end = faction_list.end();
	for(; cur != end; ++cur) {
		struct NPCFaction* fac = *cur;
		safe_delete(fac);
	}
	faction_list.clear();
	}

	safe_delete(reface_timer);
	safe_delete(swarmInfoPtr);
	safe_delete(qGlobals);
	UninitializeBuffSlots();
}

void NPC::SetTarget(Mob* mob) {
	if(mob == GetTarget())		//dont bother if they are allready our target
		return;

	//This is not the default behavior for swarm pets, must be specified from quest functions or rules value.
	if(GetSwarmInfo() && GetSwarmInfo()->target && GetTarget() && (GetTarget()->GetHP() > 0)) {
		Mob *targ = entity_list.GetMob(GetSwarmInfo()->target);
		if(targ != mob){
			return;
		}
	}

	if (mob) {
		SetAttackTimer();
	} else {
		ranged_timer.Disable();
		//attack_timer.Disable();
		attack_dw_timer.Disable();
	}
	Mob::SetTarget(mob);
}

bool NPC::Process()
{
	if (IsStunned() && stunned_timer.Check())
	{
		this->stunned = false;
		this->stunned_timer.Disable();
		this->spun_timer.Disable();
		this->spun_resist_timer.Disable();
		if (FindType(SE_SpinTarget))
			BuffFadeByEffect(SE_SpinTarget);
	}

	if (!IsEngaged() && GetPetType() != petCharmed && !IsCasting())
	{
		if (GetPetType() == petHatelist && despawn_timer.Check(false))
		{
			if (!IsCorpse() && GetHP() > 0)
			{
				Log(Logs::General, Logs::Pets, "Pet %s is despawning due to its hatelist being empty or its dispawn timer has expired.", GetName());
				int16 resistAdj = -2000;
				CastSpell(892, GetID(), EQ::spells::CastingSlot::Item, 2000, -1, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0u, 0u, &resistAdj);		// Unsummon Self
			}
			else
				Depop();
		}

		if (GetPetType() == petOrphan && hate_list.IsEmpty())
			Depop();
	}

	if (p_depop)
	{
		Mob* owner = entity_list.GetMob(this->ownerid);
		if (owner != 0)
		{
			//if(GetBodyType() != BT_SwarmPet)
			// owner->SetPetID(0);
			this->ownerid = 0;
			this->petid = 0;
		}
		return false;
	}

	SpellProcess();

	ProcessFTE();

	if(tic_timer.Check())
	{
		parse->EventNPC(EVENT_TICK, this, nullptr, "", 0);
		BuffProcess();

		if(flee_mode)
			ProcessFlee();

		int32 old_hp = GetHP();
		if(GetHP() < GetMaxHP())
			SetHP(GetHP() + GetHPRegen());

		if(RuleB(Alkabor, NPCsSendHPUpdatesPerTic) && (IsTargeted() || (IsPet() && GetOwner() && GetOwner()->IsClient()))) 
		{
			if (old_hp != cur_hp || cur_hp<max_hp) 
			{
				SendHPUpdate(false);
			}
		}

		if(GetMana() < GetMaxMana())
			SetMana(GetMana() + GetManaRegen());
	}

	if (!RuleB(Alkabor, NPCsSendHPUpdatesPerTic) && sendhpupdate_timer.Check() && (IsTargeted() || (IsPet() && GetOwner() && GetOwner()->IsClient())))
	{
		if(!IsFullHP || cur_hp<max_hp)
		{
			SendHPUpdate();
		}
	}

	if (IsMezzed())
	{
		AI_Process();
		return true;
	}

	if (enraged_timer.Check())
	{
		ProcessEnrage();

		/* Don't keep running the check every second if we don't have enrage */
		if (!GetSpecialAbility(SPECATK_ENRAGE)) {
			enraged_timer.Disable();
		}
	}

	if (IsStunned())
	{
		if (spun_resist_timer.Check())
			TrySpinStunBreak();

		if(spun_timer.Check())
			Spin();

		AI_Process();
		return true;
	}

	// This will call for help immediately after entering combat in most cases since the engaged check is before
	// the timer check; therefore it is not required to place CallForHelp()s everywhere.
	// If tick splitting is enabled then the timer will not reset early if re-engaged quickly after deaggro however
	// unless ALWAYS_CALL_HELP is true
	if ((IsEngaged() || IsFeared()) && (!IsAssisting() || GetSpecialAbility(ALWAYS_CALL_HELP)) && call_help_timer.Check())
		CallForHelp(GetTarget(), true);

	if(qGlobals)
	{
		if(qglobal_purge_timer.Check())
		{
			qGlobals->PurgeExpiredGlobals();
		}
	}

	if(flee_timer.Check())
	{
		CheckFlee();
	}

	AI_Process();

	return true;
}

uint32 NPC::CountLoot() {
	return(m_loot_items.size());
}

void NPC::UpdateEquipmentLight()
{
	m_Light.Type[EQ::lightsource::LightEquipment] = 0;
	m_Light.Level[EQ::lightsource::LightEquipment] = 0;
	
	for (int index = EQ::invslot::SLOT_BEGIN; index < EQ::invslot::EQUIPMENT_COUNT; ++index)
	{

		auto item = database.GetItem(equipment[index]);
		if (item == nullptr) { continue; }

		if (EQ::lightsource::IsLevelGreater(item->Light, m_Light.Type[EQ::lightsource::LightEquipment])) {
			m_Light.Type[EQ::lightsource::LightEquipment] = item->Light;
			m_Light.Level[EQ::lightsource::LightEquipment] =EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
		}
	}

	uint8 general_light_type = 0;
	for (auto iter = m_loot_items.begin(); iter != m_loot_items.end(); ++iter)
	{
		auto item = database.GetItem((*iter)->item_id);
		if (item == nullptr) { continue; }

		if (item->ItemClass != EQ::item::ItemClassCommon) { continue; }
		if (m_Light.Type[EQ::lightsource::LightEquipment] != 0 && (item->Light < 9 || item->Light > 13)) { continue; }

		if (EQ::lightsource::TypeToLevel(item->Light))
			general_light_type = item->Light;
	}

	if (EQ::lightsource::IsLevelGreater(general_light_type, m_Light.Type[EQ::lightsource::LightEquipment]))
		m_Light.Type[EQ::lightsource::LightEquipment] = general_light_type;

	m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
}

void NPC::Depop(bool StartSpawnTimer) {
	uint32 emoteid = this->GetEmoteID();
	if (emoteid != 0) {
		this->DoNPCEmote(EQ::constants::EmoteEventTypes::OnDespawn, emoteid);
	}
	p_depop = true;
	if (StartSpawnTimer) {
		if (respawn2 != 0) {
			respawn2->DeathReset();
		}
	}

	if(respawn2 != nullptr)
	{
		if(respawn2->NPCPointerValid())
		{
			respawn2->SetNPCPointer(nullptr);
		}
	}
}

void NPC::ForceRepop()
{
	if (respawn2 != nullptr)
	{
		Depop();
		respawn2->Repop();
	}
}

bool NPC::DatabaseCastAccepted(int spell_id) {
	for (int i=0; i < EFFECT_COUNT; i++) {
		switch(spells[spell_id].effectid[i]) {
		case SE_Stamina: {
			if(IsEngaged() && GetHPRatio() < 100.0f)
				return true;
			else
				return false;
			break;
		}
		case SE_CurrentHPOnce:
		case SE_CurrentHP: {
			if(this->GetHPRatio() < 100.0f && spells[spell_id].buffduration == 0)
				return true;
			else
				return false;
			break;
		}

		case SE_HealOverTime: {
			if(this->GetHPRatio() < 100.0f)
				return true;
			else
				return false;
			break;
		}
		case SE_DamageShield: {
			return true;
		}
		case SE_NecPet:
		case SE_SummonPet: {
			if(GetPet()){
#ifdef SPELLQUEUE
				Log(Logs::General, Logs::Pets, "%s: Attempted to make a second pet, denied.\n",GetName());
#endif
				return false;
			}
			break;
		}
		case SE_LocateCorpse:
		case SE_SummonCorpse: {
			return false; //Pfft, npcs don't need to summon corpses/locate corpses!
			break;
		}
		default:
			if(spells[spell_id].goodEffect == 1 && !(spells[spell_id].buffduration == 0 && this->GetHPRatio() == 100) && !IsEngaged())
				return true;
			return false;
		}
	}
	return false;
}

void NPC::SpawnGridNodeNPC(const glm::vec4& position, int32 grid_number, int32 zoffset)
{
	auto npc_type = new NPCType;
	memset(npc_type, 0, sizeof(NPCType));

	std::string str_zoffset = Strings::NumberToWords(zoffset);
	std::string str_number = Strings::NumberToWords(grid_number);

	strcpy(npc_type->name, str_number.c_str());
	if (zoffset != 0) {
		strcat(npc_type->name, "(Zoffset)");
	}

	npc_type->cur_hp = 4000000;
	npc_type->max_hp = 4000000;
	npc_type->race = 151;
	npc_type->gender = 2;
	npc_type->class_ = 9;
	npc_type->deity = 1;
	npc_type->level = 75;
	npc_type->npc_id = 0;
	npc_type->loottable_id = 0;
	npc_type->texture = 1;
	npc_type->light = 1;
	npc_type->size = 3;
	npc_type->runspeed = 0;
	npc_type->d_melee_texture1 = 1;
	npc_type->d_melee_texture2 = 1;
	npc_type->merchanttype = 1;
	npc_type->bodytype = 1;
	npc_type->STR = 150;
	npc_type->STA = 150;
	npc_type->DEX = 150;
	npc_type->AGI = 150;
	npc_type->INT = 150;
	npc_type->WIS = 150;
	npc_type->CHA = 150;

	strcpy(npc_type->special_abilities, "19,1^20,1^24,1^35,1");

	auto node_position = glm::vec4(position.x, position.y, position.z, position.w);
	auto npc = new NPC(npc_type, nullptr, node_position, GravityBehavior::Flying);
	
	npc->name[strlen(npc->name) - 3] = (char)NULL;

	entity_list.AddNPC(npc);
}

NPC* NPC::SpawnNPC(const char* spawncommand, const glm::vec4& position, Client* client) {
	if(spawncommand == 0 || spawncommand[0] == 0) {
		return 0;
	}
	else {
		Seperator sep(spawncommand);
		//Lets see if someone didn't fill out the whole #spawn function properly
		if (!sep.IsNumber(1))
			sprintf(sep.arg[1],"1");
		if (!sep.IsNumber(2))
			sprintf(sep.arg[2],"1");
		if (!sep.IsNumber(3))
			sprintf(sep.arg[3],"0");
		if (atoi(sep.arg[4]) > 2100000000 || atoi(sep.arg[4]) <= 0)
			sprintf(sep.arg[4]," ");
		if (!strcmp(sep.arg[5],"-"))
			sprintf(sep.arg[5]," ");
		if (!sep.IsNumber(5))
			sprintf(sep.arg[5]," ");
		if (!sep.IsNumber(6))
			sprintf(sep.arg[6],"1");
		if (!sep.IsNumber(8))
			sprintf(sep.arg[8],"0");
		if (!sep.IsNumber(9))
			sprintf(sep.arg[9], "0");
		if (!sep.IsNumber(7))
			sprintf(sep.arg[7],"0");
		if (!strcmp(sep.arg[4],"-"))
			sprintf(sep.arg[4]," ");
		if (!sep.IsNumber(10))	// bodytype
			sprintf(sep.arg[10], "0");
		//Calc MaxHP if client neglected to enter it...
		if (!sep.IsNumber(4)) {
			//Stolen from Client::GetMaxHP...
			uint8 multiplier = 0;
			int tmplevel = atoi(sep.arg[2]);
			switch(atoi(sep.arg[5]))
			{
			case WARRIOR:
				if (tmplevel < 20)
					multiplier = 22;
				else if (tmplevel < 30)
					multiplier = 23;
				else if (tmplevel < 40)
					multiplier = 25;
				else if (tmplevel < 53)
					multiplier = 27;
				else if (tmplevel < 57)
					multiplier = 28;
				else
					multiplier = 30;
				break;

			case DRUID:
			case CLERIC:
			case SHAMAN:
				multiplier = 15;
				break;

			case PALADIN:
			case SHADOWKNIGHT:
				if (tmplevel < 35)
					multiplier = 21;
				else if (tmplevel < 45)
					multiplier = 22;
				else if (tmplevel < 51)
					multiplier = 23;
				else if (tmplevel < 56)
					multiplier = 24;
				else if (tmplevel < 60)
					multiplier = 25;
				else
					multiplier = 26;
				break;

			case MONK:
			case BARD:
			case ROGUE:
			//case BEASTLORD:
				if (tmplevel < 51)
					multiplier = 18;
				else if (tmplevel < 58)
					multiplier = 19;
				else
					multiplier = 20;
				break;

			case RANGER:
				if (tmplevel < 58)
					multiplier = 20;
				else
					multiplier = 21;
				break;

			case MAGICIAN:
			case WIZARD:
			case NECROMANCER:
			case ENCHANTER:
				multiplier = 12;
				break;

			default:
				if (tmplevel < 35)
					multiplier = 21;
				else if (tmplevel < 45)
					multiplier = 22;
				else if (tmplevel < 51)
					multiplier = 23;
				else if (tmplevel < 56)
					multiplier = 24;
				else if (tmplevel < 60)
					multiplier = 25;
				else
					multiplier = 26;
				break;
			}
			sprintf(sep.arg[4],"%i",5+multiplier*atoi(sep.arg[2])+multiplier*atoi(sep.arg[2])*75/300);
		}

		// Autoselect NPC Gender
		if (sep.arg[5][0] == 0) {
			sprintf(sep.arg[5], "%i", (int) Mob::GetDefaultGender(atoi(sep.arg[1])));
		}

		//Time to create the NPC!!
		auto npc_type = new NPCType;
		memset(npc_type, 0, sizeof(NPCType));

		strncpy(npc_type->name, sep.arg[0], 60);
		npc_type->cur_hp = atoi(sep.arg[4]);
		npc_type->max_hp = atoi(sep.arg[4]);
		npc_type->race = atoi(sep.arg[1]);
		npc_type->gender = atoi(sep.arg[5]);
		npc_type->class_ = atoi(sep.arg[6]);
		npc_type->deity = 1;
		npc_type->level = atoi(sep.arg[2]);
		npc_type->npc_id = 0;
		npc_type->loottable_id = 0;
		npc_type->texture = atoi(sep.arg[3]);
		npc_type->light = 0; // spawncommand needs update
		npc_type->runspeed = 1.3f;
		npc_type->d_melee_texture1 = atoi(sep.arg[7]);
		npc_type->d_melee_texture2 = atoi(sep.arg[8]);
		npc_type->merchanttype = atoi(sep.arg[9]);
		npc_type->bodytype = atoi(sep.arg[10]);

		npc_type->STR = 150;
		npc_type->STA = 150;
		npc_type->DEX = 150;
		npc_type->AGI = 150;
		npc_type->INT = 150;
		npc_type->WIS = 150;
		npc_type->CHA = 150;

		npc_type->attack_delay = 30;

		npc_type->prim_melee_type = 28;
		npc_type->sec_melee_type = 28;

		if (npc_type->size == 0.0f)
			npc_type->size = 6.0f;

		auto npc = new NPC(npc_type, nullptr, position, GravityBehavior::Water);

		entity_list.AddNPC(npc);

		if (client) {
			// Notify client of spawn data
			client->Message(Chat::White, "New spawn:");
			client->Message(Chat::White, "Name: %s", npc->name);
			client->Message(Chat::White, "Race: %u", npc->race);
			client->Message(Chat::White, "Level: %u", npc->level);
			client->Message(Chat::White, "Material: %u", npc->texture);
			client->Message(Chat::White, "Current/Max HP: %i", npc->max_hp);
			client->Message(Chat::White, "Gender: %u", npc->gender);
			client->Message(Chat::White, "Class: %u", npc->class_);
			client->Message(Chat::White, "Weapon Item Number: %u/%u", npc->d_melee_texture1, npc->d_melee_texture2);
			client->Message(Chat::White, "MerchantID: %u", npc->MerchantType);
			client->Message(Chat::White, "Bodytype: %u", npc->bodytype);
			client->Message(Chat::White, "EntityID: %u", npc->GetID());
		}

		return npc;
	}
}

uint32 ZoneDatabase::CreateNewNPCCommand(const char* zone, Client *client, NPC* spawn, uint32 extra) {

    uint32 npc_type_id = 0;

	if (extra && client && client->GetZoneID())
	{
		// Set an npc_type ID within the standard range for the current zone if possible (zone_id * 1000)
		int starting_npc_id = client->GetZoneID() * 1000;

		std::string query = StringFormat("SELECT MAX(id) FROM npc_types WHERE id >= %i AND id < %i",
                                        starting_npc_id, starting_npc_id + 1000);
        auto results = QueryDatabase(query);
		if (results.Success()) {
            if (results.RowCount() != 0)
			{
                auto row = results.begin();
                npc_type_id = atoi(row[0]) + 1;
                // Prevent the npc_type id from exceeding the range for this zone
                if (npc_type_id >= (starting_npc_id + 1000))
                    npc_type_id = 0;
            }
            else // No npc_type IDs set in this range yet
                npc_type_id = starting_npc_id;
        }
    }

	char tmpstr[64];
	EntityList::RemoveNumbers(strn0cpy(tmpstr, spawn->GetName(), sizeof(tmpstr)));
	std::string query;
	if (npc_type_id)
	{
        query = StringFormat("INSERT INTO npc_types (id, name, level, race, class, hp, gender, "
                                        "texture, helmtexture, size, loottable_id, merchant_id, face, "
                                        "runspeed, prim_melee_type, sec_melee_type) "
                                        "VALUES(%i, \"%s\" , %i, %i, %i, %i, %i, %i, %i, %f, %i, %i, %i, %f, %i, %i)",
                                        npc_type_id, tmpstr, spawn->GetLevel(), spawn->GetRace(), spawn->GetClass(),
                                        spawn->GetMaxHP(), spawn->GetGender(), spawn->GetTexture(),
                                        spawn->GetHelmTexture(), spawn->GetSize(), spawn->GetLoottableID(),
                                        spawn->MerchantType, 0, spawn->GetRunspeed(), 28, 28);
        auto results = QueryDatabase(query);
		if (!results.Success()) {
			return false;
		}
		npc_type_id = results.LastInsertedID();
	}
	else
	{
        query = StringFormat("INSERT INTO npc_types (name, level, race, class, hp, gender, "
                                        "texture, helmtexture, size, loottable_id, merchant_id, face, "
                                        "runspeed, prim_melee_type, sec_melee_type) "
                                        "VALUES(\"%s\", %i, %i, %i, %i, %i, %i, %i, %f, %i, %i, %i, %f, %i, %i)",
                                        tmpstr, spawn->GetLevel(), spawn->GetRace(), spawn->GetClass(),
                                        spawn->GetMaxHP(), spawn->GetGender(), spawn->GetTexture(),
                                        spawn->GetHelmTexture(), spawn->GetSize(), spawn->GetLoottableID(),
                                        spawn->MerchantType, 0, spawn->GetRunspeed(), 28, 28);
        auto results = QueryDatabase(query);
		if (!results.Success()) {
			return false;
		}
		npc_type_id = results.LastInsertedID();
	}

	query = StringFormat("INSERT INTO spawngroup (id, name) VALUES(%i, '%s-%s')", 0, zone, spawn->GetName());
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}
    uint32 spawngroupid = results.LastInsertedID();

    query = StringFormat("INSERT INTO spawn2 (zone, x, y, z, respawntime, heading, spawngroupID) "
                        "VALUES('%s',%f, %f, %f, %i, %f, %i)",
                        zone, spawn->GetX(), spawn->GetY(), spawn->GetZ(), 1200,
                        spawn->GetHeading(), spawngroupid);
    results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

    query = StringFormat("INSERT INTO spawnentry (spawngroupID, npcID, chance) VALUES(%i, %i, %i)",
                        spawngroupid, npc_type_id, 100);
    results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

uint32 ZoneDatabase::AddNewNPCSpawnGroupCommand(const char* zone, Client *client, NPC* spawn, uint32 respawnTime) {
    uint32 last_insert_id = 0;

	std::string query = StringFormat("INSERT INTO spawngroup (name) VALUES('%s%s%i')",
                                    zone, spawn->GetName(), Timer::GetCurrentTime());
    auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}
    last_insert_id = results.LastInsertedID();

    uint32 respawntime = 0;
    uint32 spawnid = 0;
    if (respawnTime)
        respawntime = respawnTime;
    else if(spawn->respawn2 && spawn->respawn2->RespawnTimer() != 0)
        respawntime = spawn->respawn2->RespawnTimer();
    else
        respawntime = 1200;

    query = StringFormat("INSERT INTO spawn2 (zone, x, y, z, respawntime, heading, spawngroupID) "
                        "VALUES('%s',%f, %f, %f, %i, %f, %i)",
                        zone, spawn->GetX(), spawn->GetY(), spawn->GetZ(), respawntime,
                        spawn->GetHeading(), last_insert_id);
    results = QueryDatabase(query);
    if (!results.Success()) {
        return 0;
    }
    spawnid = results.LastInsertedID();

    query = StringFormat("INSERT INTO spawnentry (spawngroupID, npcID, chance) VALUES(%i, %i, %i)",
                        last_insert_id, spawn->GetNPCTypeID(), 100);
    results = QueryDatabase(query);
    if (!results.Success()) {
        return 0;
    }

    return spawnid;
}

uint32 ZoneDatabase::UpdateNPCTypeAppearance(Client *client, NPC* spawn) {

	std::string query = StringFormat("UPDATE npc_types SET name = \"%s\", level = %i, race = %i, class = %i, "
                                    "hp = %i, gender = %i, texture = %i, helmtexture = %i, size = %i, "
                                    "loottable_id = %i, merchant_id = %i, face = %i, WHERE id = %i",
                                    spawn->GetName(), spawn->GetLevel(), spawn->GetRace(), spawn->GetClass(),
                                    spawn->GetMaxHP(), spawn->GetGender(), spawn->GetTexture(),
                                    spawn->GetHelmTexture(), spawn->GetSize(), spawn->GetLoottableID(),
                                    spawn->MerchantType, spawn->GetNPCTypeID());
    auto results = QueryDatabase(query);

    return results.Success() == true? 1: 0;
}

uint32 ZoneDatabase::DeleteSpawnLeaveInNPCTypeTable(const char* zone, Client *client, NPC* spawn) {
	uint32 id = 0;
	uint32 spawngroupID = 0;

	std::string query = StringFormat("SELECT id, spawngroupID FROM spawn2 WHERE "
                                    "zone='%s' AND spawngroupID=%i", zone, spawn->GetSp2());
    auto results = QueryDatabase(query);
    if (!results.Success())
		return 0;

    if (results.RowCount() == 0)
        return 0;

	auto row = results.begin();
	if (row[0])
        id = atoi(row[0]);

	if (row[1])
        spawngroupID = atoi(row[1]);

    query = StringFormat("DELETE FROM spawn2 WHERE id = '%i'", id);
    results = QueryDatabase(query);
	if (!results.Success())
		return 0;

    query = StringFormat("DELETE FROM spawngroup WHERE id = '%i'", spawngroupID);
    results = QueryDatabase(query);
	if (!results.Success())
		return 0;

    query = StringFormat("DELETE FROM spawnentry WHERE spawngroupID = '%i'", spawngroupID);
    results = QueryDatabase(query);
	if (!results.Success())
		return 0;

	return 1;
}

uint32 ZoneDatabase::DeleteSpawnRemoveFromNPCTypeTable(const char* zone, Client *client, NPC* spawn) {

	uint32 id = 0;
	uint32 spawngroupID = 0;

	std::string query = StringFormat("SELECT id, spawngroupID FROM spawn2 WHERE zone = '%s' "
                                    "AND spawngroupID = %i",
                                    zone, spawn->GetSp2());
    auto results = QueryDatabase(query);
    if (!results.Success())
		return 0;

    if (results.RowCount() == 0)
        return 0;

	auto row = results.begin();

	if (row[0])
        id = atoi(row[0]);

	if (row[1])
        spawngroupID = atoi(row[1]);

    query = StringFormat("DELETE FROM spawn2 WHERE id = '%i'", id);
    results = QueryDatabase(query);
	if (!results.Success())
		return 0;

    query = StringFormat("DELETE FROM spawngroup WHERE id = '%i'", spawngroupID);
	results = QueryDatabase(query);
	if (!results.Success())
		return 0;

    query = StringFormat("DELETE FROM spawnentry WHERE spawngroupID = '%i'", spawngroupID);
	results = QueryDatabase(query);
	if (!results.Success())
		return 0;

    query = StringFormat("DELETE FROM npc_types WHERE id = '%i'", spawn->GetNPCTypeID());
	results = QueryDatabase(query);
	if (!results.Success())
		return 0;

	return 1;
}

uint32  ZoneDatabase::AddSpawnFromSpawnGroup(const char* zone, Client *client, NPC* spawn, uint32 spawnGroupID) {

	uint32 last_insert_id = 0;
	std::string query = StringFormat("INSERT INTO spawn2 (zone, x, y, z, respawntime, heading, spawngroupID) "
                                    "VALUES('%s', %f, %f, %f, %i, %f, %i)",
                                    zone, client->GetX(), client->GetY(), client->GetZ(),
                                    120, client->GetHeading(), spawnGroupID);
    auto results = QueryDatabase(query);
    if (!results.Success())
		return 0;

    return 1;
}

uint32 ZoneDatabase::AddNPCTypes(const char* zone, Client *client, NPC* spawn, uint32 spawnGroupID) {

    uint32 npc_type_id;
	char numberlessName[64];

	EntityList::RemoveNumbers(strn0cpy(numberlessName, spawn->GetName(), sizeof(numberlessName)));
	std::string query = StringFormat("INSERT INTO npc_types (name, level, race, class, hp, gender, "
                                    "texture, helmtexture, size, loottable_id, merchant_id, face, "
                                    "runspeed, prim_melee_type, sec_melee_type) "
                                    "VALUES(\"%s\", %i, %i, %i, %i, %i, %i, %i, %f, %i, %i, %i, %f, %i, %i)",
                                    numberlessName, spawn->GetLevel(), spawn->GetRace(), spawn->GetClass(),
                                    spawn->GetMaxHP(), spawn->GetGender(), spawn->GetTexture(),
                                    spawn->GetHelmTexture(), spawn->GetSize(), spawn->GetLoottableID(),
                                    spawn->MerchantType, 0, spawn->GetRunspeed(), 28, 28);
    auto results = QueryDatabase(query);
	if (!results.Success())
		return 0;
    npc_type_id = results.LastInsertedID();

	if(client)
        client->Message(Chat::White, "%s npc_type ID %i created successfully!", numberlessName, npc_type_id);

	return 1;
}

uint32 ZoneDatabase::NPCSpawnDB(uint8 command, const char* zone, Client *c, NPC* spawn, uint32 extra) {

	switch (command) {
		case NPCSpawnTypes::CreateNewSpawn: { // Create a new NPC and add all spawn related data
			return CreateNewNPCCommand(zone, c, spawn, extra);
		}
		case NPCSpawnTypes::AddNewSpawngroup:{ // Add new spawn group and spawn point for an existing NPC Type ID
			return AddNewNPCSpawnGroupCommand(zone, c, spawn, extra);
		}
		case NPCSpawnTypes::UpdateAppearance: { // Update npc_type appearance and other data on targeted spawn
			return UpdateNPCTypeAppearance(c, spawn);
		}
		case NPCSpawnTypes::RemoveSpawn: { // delete spawn from spawning, but leave in npc_types table
			return DeleteSpawnLeaveInNPCTypeTable(zone, c, spawn);
		}
		case NPCSpawnTypes::DeleteSpawn: { //delete spawn from DB (including npc_type)
			return DeleteSpawnRemoveFromNPCTypeTable(zone, c, spawn);
		}
		case NPCSpawnTypes::AddSpawnFromSpawngroup: { // add a spawn from spawngroup
			return AddSpawnFromSpawnGroup(zone, c, spawn, extra);
        }
		case NPCSpawnTypes::CreateNewNPC: { // add npc_type
			return AddNPCTypes(zone, c, spawn, extra);
		}
	}
	return false;
}

uint32 NPC::GetMaxDamage(uint8 tlevel)
{
	uint32 dmg = 0;
	if (tlevel < 40)
		dmg = tlevel*2+2;
	else if (tlevel < 50)
		dmg = level*25/10+2;
	else if (tlevel < 60)
		dmg = (tlevel*3+2)+((tlevel-50)*30);
	else
		dmg = (tlevel*3+2)+((tlevel-50)*35);
	return dmg;
}

void NPC::PickPocket(Client* thief) 
{
	if(bodytype != BT_Humanoid || IsPet() || (thief->hidden || thief->invisible))
	{
		thief->SendPickPocketResponse(this, 0, PickPocketFailed, 0, nullptr, true);
		return;
	}

	int olevel = GetLevel();
	if(thief->GetLevel() < 50) 
	{
		if(olevel > 45) 
		{
			thief->Message_StringID(Chat::Skills, STEAL_OUTSIDE_LEVEL);
			thief->SendPickPocketResponse(this, 0, PickPocketFailed, 0, nullptr, true);
			return;
		}
	} 
	else if(olevel > (thief->GetLevel() - THIEF_PICKPOCKET_UNDER))
	{
		thief->SendPickPocketResponse(this, 0, PickPocketFailed, 0, nullptr, true);
		return;
	}

	uint16 steal_skill = thief->GetSkill(EQ::skills::SkillPickPockets);

	if(zone->random.Int(1,200) > steal_skill && zone->random.Roll(9)) 
	{
		AddToHateList(thief, 50);
		if(CanTalk())
			Say_StringID(PP_FAIL, thief->GetName());
		thief->SendPickPocketResponse(this, 0, PickPocketFailed);
		return;
	}

	float success = 2.0;
	EQ::ItemInstance* inst = nullptr;
	uint16 steal_items[50];
	uint8 charges[50];
	memset(steal_items,0,50);
	memset(charges,0,50);

	uint8 money[4];
	money[0] = GetPlatinum();
	money[1] = GetGold();
	money[2] = GetSilver();
	money[3] = GetCopper();
	if (steal_skill < 125)
		money[0] = 0;
	if (steal_skill < 60)
		money[1] = 0;

	//Determine whether to steal money or an item.
	bool no_coin = ((money[0] + money[1] + money[2] + money[3]) == 0);
	bool steal_item = (steal_skill > zone->random.Int(1,250) || no_coin);
	if (steal_item)
	{
		int x = 0;
		LootItems::iterator cur,end;
		cur = m_loot_items.begin();
		end = m_loot_items.end();
		for(; cur != end && x < 49; ++cur) {
			LootItem* citem = cur->get();
			const EQ::ItemData* item = database.GetItem(citem->item_id);
			if (item)
			{
				inst = database.CreateItem(item, citem->charges);
				if (citem->equip_slot == EQ::invslot::slotGeneral1 && !item->Magic && item->NoDrop != 0 && !inst->IsType(EQ::item::ItemClassBag) && !thief->CheckLoreConflict(item) && citem->min_looter_level == 0)
				{
					steal_items[x] = item->ID;
					if (inst->IsStackable())
						charges[x] = 1;
					else
						charges[x] = citem->charges;

					x++;
				}
			}
		}

		if (x > 0)
		{
			int random = zone->random.Int(0, x-1);
			inst = database.CreateItem(steal_items[random], charges[random]);
			if (inst)
			{
				if (thief->IsSoloOnly() || thief->IsSelfFound())
				{
					inst->SetSelfFoundCharacter(thief->CharacterID(), thief->GetName());
				}
				const EQ::ItemData* item = inst->GetItem();
				if (item)
				{
					if (steal_skill > zone->random.Int(1,210))
					{
						int16 slotid = -1;
						if(!thief->GetPickPocketSlot(inst, slotid))
						{
							thief->SendPickPocketResponse(this, 0, PickPocketFailed);
						}
						else
						{
							if(slotid >= 0)
							{
								thief->SendPickPocketResponse(this, 0, PickPocketItem, slotid, inst);
								LootItem* sitem = GetItem(EQ::invslot::slotGeneral1, steal_items[random]);
								RemoveItem(sitem);
							}
							else
							{
								thief->SendPickPocketResponse(this, 0, PickPocketFailed);
							}
						}
						safe_delete(inst);
						return;
					}
					else
						steal_item = false;
				}
				else
					steal_item = false;
			}
			else
				steal_item = false;
		}
		else if (!no_coin)
		{
			steal_item = false;
		}
		else
		{
			thief->Message(Chat::White, "This target's pockets are empty.");
			thief->SendPickPocketResponse(this, 0, PickPocketFailed);
		}
	}
	if (!steal_item) //Steal money
	{
		uint8 maxamt = (steal_skill/25)+1;
		if(maxamt < 1)
			maxamt = 1;
	
		uint8 mincoin = 0;
		if(steal_skill < 125)
			mincoin = 1;
		if(steal_skill < 60)
			mincoin = 2;

		uint32 amt = zone->random.Int(1, maxamt);
		uint8 random_coin = zone->random.Int(mincoin,3);

		uint8 steal_type = 3;
		if(money[random_coin] > 0 && amt > 0)
		{
			steal_type = random_coin;
		}
		else
		{
			thief->SendPickPocketResponse(this, 0, PickPocketFailed);
			safe_delete(inst);
			return;
		}

		if (steal_skill > zone->random.Int(1,210))
		{
			switch (steal_type)
			{
				case 0:
				{
					if (amt > GetPlatinum())
						amt = GetPlatinum();
					SetPlatinum(GetPlatinum()-amt);
					thief->AddMoneyToPP(0,0,0,amt,false);
					thief->SendPickPocketResponse(this, amt, PickPocketPlatinum);
					break;
				}
				case 1:
				{
					if (amt > GetGold())
						amt = GetGold();
					SetGold(GetGold()-amt);
					thief->AddMoneyToPP(0,0,amt,0,false);
					thief->SendPickPocketResponse(this, amt, PickPocketGold);
					break;
				}
				case 2:
				{
					if (amt > GetSilver())
						amt = GetSilver();
					SetSilver(GetSilver()-amt);
					thief->AddMoneyToPP(0,amt,0,0,false);
					thief->SendPickPocketResponse(this, amt, PickPocketSilver);
					break;
				}
				case 3:
				{
					if (amt > GetCopper())
						amt = GetCopper();
					SetCopper(GetCopper()-amt);
					thief->AddMoneyToPP(amt,0,0,0,false);
					thief->SendPickPocketResponse(this, amt, PickPocketCopper);
					break;
				}
			}
		}
		else
		{
			thief->SendPickPocketResponse(this, 0, PickPocketFailed);
		}
	}
	safe_delete(inst);
}

void NPC::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho)
{
	Mob::FillSpawnStruct(ns, ForWho);
	UpdateActiveLight();
	ns->spawn.light = GetActiveLightType();

	//Not recommended if using above (However, this will work better on older clients).
	if (RuleB(Pets, UnTargetableSwarmPet)) {
		if(GetOwnerID() || GetSwarmOwner()) {
			ns->spawn.is_pet = 1;
			if (!IsCharmedPet() && GetOwnerID()) {
				Client *c = entity_list.GetClientByID(GetOwnerID());
				if (c) {
					const auto &tmp_lastname = fmt::format("{}'s Pet", c->GetName());
					if (tmp_lastname.size() < sizeof(ns->spawn.lastName)) {
						strn0cpy(ns->spawn.lastName, tmp_lastname.c_str(), sizeof(ns->spawn.lastName));
					}
				}
			}
			else if (GetSwarmOwner()) {
				ns->spawn.bodytype = 11;
				if(!IsCharmedPet())
				{
					Client *c = entity_list.GetClientByID(GetSwarmOwner());
					if (c) {
						const auto &tmp_lastname = fmt::format("{}'s Pet", c->GetName());
						if (tmp_lastname.size() < sizeof(ns->spawn.lastName)) {
							strn0cpy(ns->spawn.lastName, tmp_lastname.c_str(), sizeof(ns->spawn.lastName));
						}
					}
				}
			}
		}
	} else {
		if(GetOwnerID()) {
			ns->spawn.is_pet = 1;
			if (!IsCharmedPet() && GetOwnerID()) {
				Client *c = entity_list.GetClientByID(GetOwnerID());
				if (c) {
					const auto &tmp_lastname = fmt::format("{}'s Pet", c->GetName());
					if (tmp_lastname.size() < sizeof(ns->spawn.lastName)) {
						strn0cpy(ns->spawn.lastName, tmp_lastname.c_str(), sizeof(ns->spawn.lastName));
					}
				}
			}
		} else
			ns->spawn.is_pet = 0;
	}

	ns->spawn.is_npc = 1;
}

void NPC::SetLevel(uint8 in_level, bool command)
{
	level = in_level;
	SendAppearancePacket(AppearanceType::WhoLevel, in_level);
}

void NPC::ModifyNPCStat(const char *identifier, const char *newValue)
{
	std::string id = Strings::ToLower(identifier);
	std::string val = newValue;

	if(id == "ac") { AC = atoi(val.c_str()); return; }
	else if(id == "str") { STR = atoi(val.c_str()); return; }
	else if(id == "sta") { STA = atoi(val.c_str()); return; }
	else if(id == "agi") { AGI = atoi(val.c_str()); return; }
	else if(id == "dex") { DEX = atoi(val.c_str()); return; }
	else if(id == "wis") { WIS = atoi(val.c_str()); CalcMaxMana(); return; }
	else if(id == "int" || id == "_int") { INT = atoi(val.c_str()); CalcMaxMana(); return; }
	else if(id == "cha") { CHA = atoi(val.c_str()); return; }
	else if(id == "max_hp") { base_hp = atoi(val.c_str()); CalcMaxHP(); if (cur_hp > max_hp) { cur_hp = max_hp; } return; }
	else if(id == "max_mana") { npc_mana = atoi(val.c_str()); CalcMaxMana(); if (cur_mana > max_mana){ cur_mana = max_mana; } return; }
	else if(id == "mr") { MR = atoi(val.c_str()); return; }
	else if(id == "fr") { FR = atoi(val.c_str()); return; }
	else if(id == "cr") { CR = atoi(val.c_str()); return; }
	else if(id == "pr") { PR = atoi(val.c_str()); return; }
	else if(id == "dr") { DR = atoi(val.c_str()); return; }
	else if(id == "runspeed") { runspeed = (float)atof(val.c_str()); CalcBonuses(); return; }
	else if(id == "special_abilities") { ModifySpecialAbility(val.c_str()); return; }
	else if(id == "atk") { ATK = atoi(val.c_str()); return; }
	else if(id == "accuracy") { accuracy_rating = atoi(val.c_str()); return; }
	else if(id == "min_hit") { min_dmg = atoi(val.c_str()); return; }
	else if(id == "max_hit") { max_dmg = atoi(val.c_str()); return; }
	else if(id == "attack_count") { attack_count = atoi(val.c_str()); return; }
	else if(id == "attack_delay") { attack_delay = atoi(val.c_str()); SetAttackTimer(); return; }
	else if(id == "see_invis") { see_invis = atoi(val.c_str()); return; }
	else if(id == "see_invis_undead") { see_invis_undead = atoi(val.c_str()); return; }
	else if(id == "see_sneak") { see_sneak = atoi(val.c_str()); return; }
	else if(id == "see_improved_hide") { see_improved_hide = atoi(val.c_str()); return; }
	else if(id == "hp_regen") { hp_regen = atoi(val.c_str()); return; }
	else if(id == "combat_hp_regen") { combat_hp_regen = atoi(val.c_str()); return; }
	else if(id == "mana_regen") { mana_regen = atoi(val.c_str()); return; }
	else if(id == "combat_mana_regen") { combat_mana_regen = atoi(val.c_str()); return; }
	else if(id == "level") { SetLevel(atoi(val.c_str())); return; }
	else if(id == "aggro") { pAggroRange = atof(val.c_str()); return; }
	else if(id == "assist") { pAssistRange = atof(val.c_str()); return; }
	else if(id == "slow_mitigation") { slow_mitigation = atoi(val.c_str()); return; }
	else if(id == "loottable_id") { m_loottable_id = atoi(val.c_str()); return; }
	else if(id == "healscale") { healscale = atof(val.c_str()); return; }
	else if(id == "spellscale") { spellscale = atof(val.c_str()); return; }
}

void NPC::LevelScale() {

	uint8 random_level = (zone->random.Int(level, maxlevel));
	float scaling = (((random_level / (float)level) - 1) * (scalerate / 100.0f));

	if(scalerate == 0 || maxlevel <= 25)
	{
		//todo: add expansion column to npc_types
		if (npctype_id < 200000)
		{
			max_hp += (random_level - level) * 20;
			base_hp += (random_level - level) * 20;
		}
		else
		{
			max_hp += (random_level - level) * 100;
			base_hp += (random_level - level) * 100;
		}

		cur_hp = max_hp;

		max_dmg += (random_level - level) * 2;
		if (level < 51)
			AC += (random_level - level) * 3;
	}
	else
	{
		uint8 scale_adjust = 1;

		base_hp += (int)(base_hp * scaling);
		max_hp += (int)(max_hp * scaling);
		cur_hp = max_hp;

		if (max_dmg)
		{
			max_dmg += (int)(max_dmg * scaling / scale_adjust);
			min_dmg += (int)(min_dmg * scaling / scale_adjust);
		}

		STR += (int)(STR * scaling / scale_adjust);
		STA += (int)(STA * scaling / scale_adjust);
		AGI += (int)(AGI * scaling / scale_adjust);
		DEX += (int)(DEX * scaling / scale_adjust);
		INT += (int)(INT * scaling / scale_adjust);
		WIS += (int)(WIS * scaling / scale_adjust);
		CHA += (int)(CHA * scaling / scale_adjust);
		if (MR)
			MR += (int)(MR * scaling / scale_adjust);
		if (CR)
			CR += (int)(CR * scaling / scale_adjust);
		if (DR)
			DR += (int)(DR * scaling / scale_adjust);
		if (FR)
			FR += (int)(FR * scaling / scale_adjust);
		if (PR)
			PR += (int)(PR * scaling / scale_adjust);
	}

	level = random_level;

	return;
}

void NPC::CalcNPCResists() {

	if (!MR)
		MR = (GetLevel() * 11)/10;
	if (!CR)
		CR = (GetLevel() * 11)/10;
	if (!DR)
		DR = (GetLevel() * 11)/10;
	if (!FR)
		FR = (GetLevel() * 11)/10;
	if (!PR)
		PR = (GetLevel() * 11)/10;
	return;
}

void NPC::CalcNPCRegen() {

	// Fix for lazy db-updaters (regen values left at 0)
	if (GetCasterClass() != 'N' && mana_regen == 0)
		mana_regen = (GetLevel() / 10) + 4;
	else if(mana_regen < 0)
		mana_regen = 0;
	else
		mana_regen = mana_regen;

	// Gives low end monsters no regen if set to 0 in database. Should make low end monsters killable
	// Might want to lower this to /5 rather than 10.
	if(hp_regen == 0)
	{
		if (GetLevel() <= 15)
			hp_regen = 1;
		else if(GetLevel() > 15 && GetLevel() <= 30)
			hp_regen = 2;
		else if (GetLevel() > 30 && GetLevel() <= 35)
			hp_regen = 4;
		else if(GetLevel() > 35 && GetLevel() <= 40)
			hp_regen = 8;
		else if(GetLevel() > 40 && GetLevel() <= 45)
			hp_regen = 12;
		else if(GetLevel() > 45 && GetLevel() <= 50)
			hp_regen = 18;
		else
			hp_regen = 24;
	}
	else if (hp_regen < 0)
		hp_regen = 0;
	else
		hp_regen = hp_regen;

	return;
}

void NPC::CalcNPCDamage() {

	int AC_adjust=12;

	if (GetLevel() >= 66) {
		if (min_dmg==0)
			min_dmg = 220;
		if (max_dmg==0)
			max_dmg = ((((99000)*(GetLevel()-64))/400)*AC_adjust/10);
	}
	else if (GetLevel() >= 60 && GetLevel() <= 65){
		if(min_dmg==0)
			min_dmg = (GetLevel()+(GetLevel()/3));
		if(max_dmg==0)
			max_dmg = (GetLevel()*3)*AC_adjust/10;
	}
	else if (GetLevel() >= 51 && GetLevel() <= 59){
		if(min_dmg==0)
			min_dmg = (GetLevel()+(GetLevel()/3));
		if(max_dmg==0)
			max_dmg = (GetLevel()*3)*AC_adjust/10;
	}
	else if (GetLevel() >= 40 && GetLevel() <= 50) {
		if (min_dmg==0)
			min_dmg = GetLevel();
		if(max_dmg==0)
			max_dmg = (GetLevel()*3)*AC_adjust/10;
	}
	else if (GetLevel() >= 28 && GetLevel() <= 39) {
		if (min_dmg==0)
			min_dmg = GetLevel() / 2;
		if (max_dmg==0)
			max_dmg = ((GetLevel()*2)+2)*AC_adjust/10;
	}
	else if (GetLevel() <= 27) {
		if (min_dmg==0)
			min_dmg=1;
		if (max_dmg==0)
			max_dmg = (GetLevel()*2)*AC_adjust/10;
	}

	int32 clfact = GetClassLevelFactor();
	min_dmg = (min_dmg * clfact) / 220;
	max_dmg = (max_dmg * clfact) / 220;

	return;
}


uint32 NPC::GetSpawnPointID() const
{
	if(respawn2)
	{
		return respawn2->GetID();
	}
	return 0;
}

void NPC::NPCSlotTexture(uint8 slot, uint16 texture)
{
	if (slot == 7) {
		d_melee_texture1 = texture;
	}
	else if (slot == 8) {
		d_melee_texture2 = texture;
	}
	else if (slot < 6) {
		// Reserved for texturing individual armor slots
	}
	return;
}

uint32 NPC::GetSwarmOwner()
{
	if(GetSwarmInfo() != nullptr)
	{
		return GetSwarmInfo()->owner_id;
	}
	return 0;
}

uint32 NPC::GetSwarmTarget()
{
	if(GetSwarmInfo() != nullptr)
	{
		return GetSwarmInfo()->target;
	}
	return 0;
}

void NPC::SetSwarmTarget(int target_id)
{
	if(GetSwarmInfo() != nullptr)
	{
		GetSwarmInfo()->target = target_id;
	}
	return;
}

int32 NPC::CalcMaxMana() {
	if(npc_mana == 0) {
		switch (GetCasterClass()) {
			case 'I':
				max_mana = (((GetINT()/2)+1) * GetLevel()) + spellbonuses.Mana + itembonuses.Mana;
				break;
			case 'W':
				max_mana = (((GetWIS()/2)+1) * GetLevel()) + spellbonuses.Mana + itembonuses.Mana;
				break;
			case 'N':
			default:
				max_mana = 0;
				break;
		}
		if (max_mana < 0) {
			max_mana = 0;
		}

		return max_mana;
	} else {
		max_mana = npc_mana + spellbonuses.Mana + itembonuses.Mana;
		if (max_mana < 0)
			max_mana = 0;

		return max_mana;
	}
}

void NPC::SignalNPC(int _signal_id, const char* data)
{
	signal_q.push_back(_signal_id);
	signal_strq.push_back(data ? data : "");
}

NPC_Emote_Struct* NPC::GetNPCEmote(uint32 emoteid, uint8 event_)
{
	std::vector<NPC_Emote_Struct*> Emotes;
	for (auto &i : zone->npc_emote_list) {
		NPC_Emote_Struct* nes = i;
		if (emoteid == nes->emoteid && event_ == nes->event_) {
			Emotes.push_back(i);
		}
	}

	if (Emotes.size() == 0)	{
		return nullptr;
	}
	else if (Emotes.size() == 1)	{
		return Emotes[0];
	}
	else {
		int index = zone->random.Roll0(Emotes.size());
		return Emotes[index];
	}
}

void NPC::DoNPCEmote(uint8 event_, uint32 emoteid, Mob* target)
{
	if(this == nullptr || emoteid == 0) {
		return;
	}

	NPC_Emote_Struct* nes = GetNPCEmote(emoteid,event_);
	if(nes == nullptr) {
		return;
	}

	std::string processed = nes->text;
	Strings::FindReplace(processed, "$mname", GetCleanName());
	Strings::FindReplace(processed, "$mracep", GetRaceIDNamePlural(GetRace()));
	Strings::FindReplace(processed, "$mrace", GetRaceIDName(GetRace()));
	Strings::FindReplace(processed, "$mclass", GetClassIDName(GetClass()));
	if (target) {
		Strings::FindReplace(processed, "$name", target->GetCleanName());
		Strings::FindReplace(processed, "$racep", GetRaceIDNamePlural(target->GetRace()));
		Strings::FindReplace(processed, "$race", GetRaceIDName(target->GetRace()));
		Strings::FindReplace(processed, "$class", GetClassIDName(target->GetClass()));
	}
	else {
		Strings::FindReplace(processed, "$name", "foe");
		Strings::FindReplace(processed, "$racep", "races");
		Strings::FindReplace(processed, "$race", "race");
		Strings::FindReplace(processed, "$class", "class");
	}

	if(emoteid == nes->emoteid) {
		if (event_ == EQ::constants::EmoteEventTypes::Hailed && target) {
			DoQuestPause(target);
		}

		if (nes->type == 1) {
			this->Emote("%s", processed.c_str());
		}
		else if (nes->type == 2) {
			this->Shout("%s", processed.c_str());
		}
		else if (nes->type == 3) {
			entity_list.MessageClose_StringID(this, true, 200, 10, GENERIC_STRING, processed.c_str());
		}
		else {
			this->Say("%s", processed.c_str());
		}
	}

	// emoteid 1 ENTERCOMBAT event is special and we do the faction emote if the NPC proximity aggros or assist aggros on a client. (i.e. not 
	// directly aggroed with an attack from outside aggro range)  After mid-Luclin, the only NPCs to do the secdonary faction emotes were 
	// NPCs that did the "Time to die $name." + "my comrades will avenge my death." emotes.  Prior to that a lot of NPCs did them including
	// lizardmen, gnolls, goblins, kobolds etc.
	if (emoteid == 1 && event_ == 1 && target && target->IsClient() && GetHateAmount(target, false) == 20 && GetDamageAmount(target) == 0) {
		DoFactionEmote();
	}
}

void NPC::DoFactionEmote()
{
	Mob* target = GetTarget();
	if (!target || !target->IsClient() || !GetPrimaryFaction() || IsPet() || GetClass() == MERCHANT || GetClass() == BANKER)
		return;

	FactionMods fmods;
	int32 reputation = target->CastToClient()->GetCharacterFactionLevel(GetPrimaryFaction()) + GetFactionBonus(GetPrimaryFaction());

	database.GetFactionData(&fmods, target->GetClass(), target->GetRace(), target->GetDeity(), GetPrimaryFaction(),
		target->GetTexture(), target->GetGender(), target->GetBaseRace());

	// note: Sony probably doesn't do it exactly this way.  this is a guess
	if (reputation <= fmods.race_mod && reputation <= fmods.class_mod && reputation <= fmods.deity_mod)
		Say_StringID(zone->random.Int(1180, 1183));
	else if (fmods.class_mod <= fmods.race_mod && fmods.class_mod <= fmods.deity_mod)
		Say_StringID(0, zone->random.Int(1172, 1175), itoa(target->GetClassStringID()));
	else if (fmods.race_mod <= fmods.deity_mod)
	{
		if (IsPlayableRace(target->GetRace()))
			Say_StringID(0, zone->random.Int(1176, 1179), itoa(target->GetRaceStringID()));
		else
			Say_StringID(0, zone->random.Int(1176, 1179), itoa(SCUMSUCKERS));
	}
	else
		Say_StringID(zone->random.Int(1184, 1187));
}

bool NPC::CanTalk()
{
	//Races that should be able to talk.

	uint16 TalkRace[329] =
	{1,2,3,4,5,6,7,8,9,10,11,12,0,0,15,16,0,18,19,20,0,0,23,0,25,0,0,0,0,0,0,
	32,0,0,0,0,0,0,39,40,0,0,0,44,0,0,0,0,49,0,51,0,53,54,55,56,57,58,0,0,0,
	62,0,64,65,66,67,0,0,70,71,0,0,0,0,0,77,78,79,0,81,82,0,0,0,86,0,0,0,90,
	0,92,93,94,95,0,0,98,99,0,101,0,103,0,0,0,0,0,0,110,111,112,0,0,0,0,0,0,
	0,0,0,0,123,0,0,126,0,128,0,130,131,0,0,0,0,136,137,0,139,140,0,0,0,144,
	0,0,0,0,0,150,151,152,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,183,184,0,0,187,188,189,0,0,0,0,0,195,196,0,198,0,0,0,202,0,
	0,205,0,0,208,0,0,0,0,0,0,0,0,217,0,219,0,0,0,0,0,0,226,0,0,229,230,0,0,
	0,0,235,236,0,238,239,240,241,242,243,244,0,246,247,0,0,0,251,0,0,254,255,
	256,257,0,0,0,0,0,0,0,0,266,267,0,0,270,271,0,0,0,0,0,277,278,0,0,0,0,283,
	284,0,286,0,288,289,290,0,0,0,0,295,296,297,298,299,300,0,0,0,304,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,320,0,322,323,324,325,0,0,0,0};

	int talk_check = TalkRace[GetRace() - 1];

	if (TalkRace[GetRace() - 1] > 0)
		return true;

	return false;
}

//this is called with 'this' as the mob being looked at, and
//iOther the mob who is doing the looking. It should figure out
//what iOther thinks about 'this'
FACTION_VALUE NPC::GetReverseFactionCon(Mob* iOther) {
	iOther = iOther->GetOwnerOrSelf();
	int primaryFaction= iOther->GetPrimaryFaction();

	//I am pretty sure that this special faction call is backwards
	//and should be iOther->GetSpecialFactionCon(this)
	if (primaryFaction < 0)
		return GetSpecialFactionCon(iOther);

	if (primaryFaction == 0)
		return FACTION_INDIFFERENTLY;

	//if we are a pet, use our owner's faction stuff
	Mob *own = GetOwner();
	if (own != nullptr)
		return own->GetReverseFactionCon(iOther);

	//make sure iOther is an npc
	//also, if we dont have a faction, then they arnt gunna think anything of us either
	if(!iOther->IsNPC() || GetPrimaryFaction() == 0)
		return(FACTION_INDIFFERENTLY);

	//if we get here, iOther is an NPC too

	//otherwise, employ the npc faction stuff
	//so we need to look at iOther's faction table to see
	//what iOther thinks about our primary faction
	return(iOther->CastToNPC()->CheckNPCFactionAlly(GetPrimaryFaction()));
}

//Look through our faction list and return a faction con based
//on the npc_value for the other person's primary faction in our list.
FACTION_VALUE NPC::CheckNPCFactionAlly(int32 other_faction) {
	std::list<struct NPCFaction*>::iterator cur,end;
	cur = faction_list.begin();
	end = faction_list.end();
	for(; cur != end; ++cur) {
		struct NPCFaction* fac = *cur;
		if ((int32)fac->factionID == other_faction) {
			if (fac->npc_value > 0)
				return FACTION_ALLY;
			else if (fac->npc_value < 0)
				return FACTION_SCOWLS;
			else
				return FACTION_INDIFFERENTLY;
		}
	}
	return FACTION_INDIFFERENTLY;
}

bool NPC::IsFactionListAlly(uint32 other_faction) {
	return(CheckNPCFactionAlly(other_faction) == FACTION_ALLY);
}

uint32 NPC::GetSpawnKillCount()
{
	uint32 sid = GetSpawnPointID();

	if(sid > 0)
	{
		return(zone->GetSpawnKillCount(sid));
	}

	return(0);
}

void NPC::DoQuestPause(Mob *other)
{
	if (noQuestPause)
		return;
	if(IsMoving() && !IsOnHatelist(other)) {
		PauseWandering(RuleI(NPC, SayPauseTimeInSec));
		if (other && !other->sneaking)
			FaceTarget(other);
	} else if(!IsMoving()) {
		if(other && !other->sneaking && GetAppearance() != eaSitting && GetAppearance() != eaDead)
			FaceTarget(other);
		// this stops them from auto changing direction, in AI_DoMovement()
		AIhail_timer->Start((RuleI(NPC, SayPauseTimeInSec)-1)*1000);
	}
}

void NPC::DepopSwarmPets()
{
	if (GetSwarmInfo()) {
		if (GetSwarmInfo()->duration->Check(false)){
			Mob* owner = entity_list.GetMobID(GetSwarmInfo()->owner_id);
			if (owner)
				owner->SetTempPetCount(owner->GetTempPetCount() - 1);

			Depop();
			return;
		}

		//This is only used for optional quest or rule derived behavior now if you force a temp pet on a specific target.
		if (GetSwarmInfo()->target) {
			Mob *targMob = entity_list.GetMob(GetSwarmInfo()->target);
			if(!targMob || (targMob && targMob->IsCorpse())){
				Mob* owner = entity_list.GetMobID(GetSwarmInfo()->owner_id);
				if (owner)
					owner->SetTempPetCount(owner->GetTempPetCount() - 1);

				Depop();
				return;
			}
		}
	}
}

int32 NPC::GetHPRegen() 
{
	uint32 bonus = 0;
	if(GetAppearance() == eaSitting)
		bonus = 1;

	if (GetHP() < GetMaxHP())
	{
		// OOC
		if(!IsEngaged() && (!IsPet() || (!IsCharmedPet() && !IsDireCharmed())))
		{
			return(GetNPCHPRegen() + bonus); // hp_regen + spell/item regen + sitting bonus
		}
		else
			return(GetCombatHPRegen() + (GetNPCHPRegen() - hp_regen)); // combat_regen + spell/item regen
	} 
	else
		return 0;
}

int32 NPC::GetManaRegen()
{
	uint32 bonus = 0;
	if(GetAppearance() == eaSitting)
		bonus += 3;	// made-up, prob should not exist

	if (GetMana() < GetMaxMana())
	{
		if (!IsEngaged())
		{
			return(GetNPCManaRegen() + bonus); // mana_regen + spell/item regen + sitting bonus
		}
		else
			return(GetCombatManaRegen() + (GetNPCManaRegen() - mana_regen)); // combat_regen + spell/item regen
	} 
	else
		return 0;
}
void NPC::AddPush(float heading, float magnitude)
{
	float heading_512 = heading;
	if (heading_512 < 384.0f)
		heading_512 += 128.0f;
	else
		heading_512 -= 384.0f;

	push_vector.x += g_Math.FastCos(heading_512) * magnitude;
	push_vector.y += g_Math.FastSin(heading_512) * magnitude;
}

/*
	This function attempts to move the NPC in the drection of push_vector.  Checks are made to prevent NPCs from
	going into walls.  Multiple raycasts with zone geoemtry are done to prevent NPCs from going into the wall up
	to their centerpoints.  If the NPC hits a wall, it will 'glance' it and change the direction of the vector
	to align with the wall so mobs won't get stuck on it.

	Since this is expensive, this gets called on a timer periodically instead of called for every melee hit.
	Use AddPush() to add to the push vector.
*/
float NPC::ApplyPushVector(bool noglance)
{
	// if mob is perma rooted do not push
	if (!zone->zonemap || GetBaseRunspeed() <= 0.0f) {
		push_vector = glm::vec3(0.0f, 0.0f, 0.0f);
		return 0.0f;
	}

	if (push_vector.x == 0.0f && push_vector.y == 0.0f)
		return 0.0f;

	glm::vec3 currentLoc(GetX(), GetY(), GetZ());
	glm::vec3 newLoc(GetX(), GetY(), GetZ());
	glm::vec3 hitLocation, hitNormal, glanceV, intersection, closestWallNormal, closestHitLocation;
	float hitDistance;
	float magnitude = sqrtf(push_vector.x * push_vector.x + push_vector.y * push_vector.y);
	glm::vec2 pushUnitV(push_vector.x / magnitude, push_vector.y / magnitude);
	float sizeCushion = GetSize() / 2.0f;
	if (GetRace() == 304) // use larger cushion on quarm.  Adjusted to match quarm videos of how far he goes into wall.
		sizeCushion /= 2.0f;
	else if (sizeCushion > 2.5f)
		sizeCushion = 2.5f;
	if (sizeCushion < 1.0f)
		sizeCushion = 1.0f;
	float newz;
	int closestPoint = -1;
	float closestHitDist = sizeCushion;

	// where we want to be
	newLoc.x -= push_vector.x;
	newLoc.y += push_vector.y;

	/*
	if (noglance)
		Say("starting loc: %.2f, %.2f, %.2f  newLoc: %.2f, %.2f, %.2f (%.2f)  push mag: %.2f;  sizeCushion: %.2f",
		currentLoc.x, currentLoc.y, currentLoc.z, newLoc.x, newLoc.y, newLoc.z, GetZ(), magnitude, sizeCushion);
	else
		Shout("starting loc: %.2f, %.2f, %.2f  newLoc: %.2f, %.2f, %.2f (%.2f)  push mag: %.2f;  sizeCushion: %.2f",
		currentLoc.x, currentLoc.y, currentLoc.z, newLoc.x, newLoc.y, newLoc.z, GetZ(), magnitude, sizeCushion);
	glm::vec3 down(currentLoc.x, currentLoc.y, -99999);
	zone->zonemap->LineIntersectsZone(currentLoc, down, 0.0f, &hitLocation, &hitNormal, &hitDistance);
	Say("curentLoc.z to floor == %.2f", hitDistance);

	glm::vec3 towall(currentLoc.x, currentLoc.y, currentLoc.y);
	towall.x -= pushUnitV.x * 1000.0f;
	towall.y += pushUnitV.y * 1000.0f;
	zone->zonemap->LineIntersectsZone(currentLoc, towall, 0.0f, &hitLocation, &hitNormal, &hitDistance);
	Say("curentLoc to wall == %.5f", hitDistance);
	*/

	bool glancing = false;
	float glanceMag = 0.0f;
	int pointsHit = 0;
	bool bounce = false;
	glm::vec3 pointsUnitV[5];
	glm::vec3 point;

	if (!noglance)
	{
		// don't forget corner status/loc until mob has been pushed 3 units out of corner.  Corner state checking is unreliable
		if (IsCornered() && (fabs(corner_x - m_Position.x) > 3.0f || fabs(corner_y - m_Position.y) > 3.0f))
		{
			//if (corner_x || corner_y) Shout(" -----------> out of corner");
			SetNotCornered();
		}
		else
			wall_normal1_x = wall_normal1_y = wall_normal2_x = wall_normal2_y = 0.0f; // delete previous wall detection prior to each push; SetNotCornered() does this too
	}

	if (zone->zonemap->LineIntersectsZone(currentLoc, newLoc, 0.0f, &hitLocation, &hitNormal, &hitDistance))
	{
		//Shout("Destination loc behind wall; dist: %.2f point: %.2f, %.2f; hitLoc: %.2f, %.2f, %.2f  push_vector: %.2f, %.2f; normal: %.2f, %.2f, %.2f",
		//	hitDistance, newLoc.x, newLoc.y, hitLocation.x, hitLocation.y, hitLocation.z, push_vector.x, push_vector.y, hitNormal.x, hitNormal.y, hitNormal.z);

		if (hitNormal.x != 0.0f || hitNormal.y != 0.0f)
		{
			// z > 0 means NPC hit an inclined floor.  z < 0 means NPC hit an inclined ceiling
			// z == 1 is flat floor; z == -1 is flat ceiling; z == 0 is vertical wall
			if (hitNormal.z < 0.25f || hitDistance > 10)		// stop large pushes to inclined floors, otherwise we can see
			{													// weird stuff like pushing through hills or walls
				pointsHit++;

				if (hitDistance < sizeCushion)
				{
					// we are too close to the wall

					// the hit normal frustratingly returns the opposite direction often when striking a double sided wall
					// so we need to flip it if that's the case
					if (hitNormal.z == 0.0f)		// non-vertical walls probably not double sided
					{
						// angle between wall normal and push vector
						float hitNormalDotp = hitNormal.x * pushUnitV.x + hitNormal.y * pushUnitV.y;

						if (hitNormalDotp > 0.0f)
						{
							// if wall normal points away from push vector, then flip it
							hitNormal.x = -hitNormal.x;
							hitNormal.y = -hitNormal.y;
							//Say("flipping wall normal");
						}
					}
					// push NPC back to cushion dist from wall
					newLoc.x = currentLoc.x + pushUnitV.x * (sizeCushion - hitDistance);
					newLoc.y = currentLoc.y - pushUnitV.y * (sizeCushion - hitDistance);
					bounce = true;
					//Say("bouncing off front wall");

					// 'push energy' goes into glance vector
					glanceMag = magnitude;
					magnitude = 0.0f;
				}
				else
				{
					// NPC is not too close to the wall yet, but new position is past wall

					// reduce vector magnitude to end up precisely at wall
					float newMag = magnitude - (magnitude - hitDistance + sizeCushion);
					push_vector.x = pushUnitV.x * newMag;
					push_vector.y = pushUnitV.y * newMag;

					newLoc.x = currentLoc.x - push_vector.x;
					newLoc.y = currentLoc.y + push_vector.y;
					bounce = true;

					// save the remaining 'push energy' for the glance vector
					glanceMag = magnitude - newMag;
					magnitude = newMag;

					//Say("reducing push magnitude to match cushion dist;   newLoc: %.2f, %.2f, %.2f  new push mag: %.2f  newLoc to wall dist: %.2f",
					//	newLoc.x, newLoc.y, newLoc.z, magnitude, Distance(newLoc, hitLocation));
				}

				closestWallNormal.x = hitNormal.x; closestWallNormal.y = hitNormal.y; closestWallNormal.z = hitNormal.z;
				closestHitLocation.x = hitLocation.x; closestHitLocation.y = hitLocation.y; closestHitLocation.z = hitLocation.z;
				closestPoint = 0;
				closestHitDist = hitDistance;

				if (!noglance)
				{
					wall_normal1_x = hitNormal.x;	// save wall normals for archery poof checks
					wall_normal1_y = hitNormal.y;
				}
			}
			else
			{
				//Say("hit inclined floor");
				if (hitNormal.z > 0.0f)
					newLoc.z += magnitude / hitNormal.z;
			}
		}
	}

	// Check for walls in 5 locations: in front of NPC, both sides of NPC, and 45 degree angles, half size dist away.
	// this prevents NPC centerpoints from colliding into the wall, burying half their model in zone geometry
	for (int i = 0; i < 5; i++)
	{
		switch (i)
		{
		case 0:
			pointsUnitV[i].x = pushUnitV.x;
			pointsUnitV[i].y = pushUnitV.y;
			pointsUnitV[i].z = newLoc.z;
			break;
		case 1:
			pointsUnitV[i].x = -pushUnitV.y;
			pointsUnitV[i].y = pushUnitV.x;
			pointsUnitV[i].z = newLoc.z;
			break;

		case 2:
			pointsUnitV[i].x = pushUnitV.y;
			pointsUnitV[i].y = -pushUnitV.x;
			pointsUnitV[i].z = newLoc.z;
			break;

		case 3:
			pointsUnitV[i].x = (pushUnitV.x * cosf(-3.141592654f / 4.0f) - pushUnitV.y * sinf(-3.141592f / 4.0f));
			pointsUnitV[i].y = (pushUnitV.x * sinf(-3.141592654f / 4.0f) + pushUnitV.y * cosf(-3.141592f / 4.0f));
			pointsUnitV[i].z = newLoc.z;
			break;

		case 4:
			pointsUnitV[i].x = (pushUnitV.x * cosf(3.141592654f / 4.0f) - pushUnitV.y * sinf(3.141592f / 4.0f));
			pointsUnitV[i].y = (pushUnitV.x * sinf(3.141592654f / 4.0f) + pushUnitV.y * cosf(3.141592f / 4.0f));
			pointsUnitV[i].z = newLoc.z;
			break;
		}

		point.x = newLoc.x - pointsUnitV[i].x * sizeCushion;
		point.y = newLoc.y + pointsUnitV[i].y * sizeCushion;
		point.z = newLoc.z;

		if (zone->zonemap->LineIntersectsZone(newLoc, point, 0.0f, &hitLocation, &hitNormal, &hitDistance))
		{
			//Shout("Hit Wall %i; dist: %.2f point: %.2f, %.2f; hitLoc: %.2f, %.2f, %.2f  push_vector: %.2f, %.2f; normal: %.2f, %.2f, %.2f",
			//	i, hitDistance, point.x, point.y, hitLocation.x, hitLocation.y, hitLocation.z, push_vector.x, push_vector.y, hitNormal.x, hitNormal.y, hitNormal.z);

			if (hitNormal.x != 0.0f || hitNormal.y != 0.0f)
			{
				// z > 0 means NPC hit an inclined floor.  z < 0 means NPC hit an inclined ceiling
				if (hitNormal.z < 0.25f || hitDistance > 10.0f)		// stop large pushes to inclined floors, otherwise we can see
				{													// weird stuff like pushing through hills or walls
					pointsHit++;

					// save wall normals for archery poof checks
					if (!wall_normal1_x && !wall_normal1_y)
					{
						wall_normal1_x = hitNormal.x;
						wall_normal1_y = hitNormal.y;
					}
					else if (!wall_normal2_x && !wall_normal2_y && wall_normal1_x != hitNormal.x && wall_normal1_y != hitNormal.y)
					{
						wall_normal2_x = hitNormal.x;
						wall_normal2_y = hitNormal.y;
					}

					if (magnitude == 0.0f)
					{
						continue;
					}
					else if (hitDistance < (sizeCushion - 0.0001f))
					{
						// push NPC back to cushion dist from wall

						// the hit normal frustratingly returns the opposite direction often when striking a double sided wall
						// so we need to flip it if that's the case
						if (hitNormal.z == 0.0f)		// non-vertical walls probably not double sided
						{
							// angle between wall normal and unit vector of newLoc to point
							float hitNormalDotp = hitNormal.x * -pointsUnitV[i].x + hitNormal.y * pointsUnitV[i].y;

							if (hitNormalDotp > 0.0f)
							{
								// if wall normal points away from push vector, then flip it
								hitNormal.x = -hitNormal.x;
								hitNormal.y = -hitNormal.y;
								//Say("flipping wall normal");
							}
						}

						float bounceDist = sizeCushion - hitDistance;

						// push away from wall
						newLoc.x = newLoc.x + pointsUnitV[i].x * bounceDist;
						newLoc.y = newLoc.y - pointsUnitV[i].y * bounceDist;
						bounce = true;

						// save push 'energy' for glance vector
						glanceMag += bounceDist;
						magnitude -= bounceDist;

						if (hitDistance < closestHitDist)
						{
							closestWallNormal.x = hitNormal.x;		closestWallNormal.y = hitNormal.y;		closestWallNormal.z = hitNormal.z;
							closestHitLocation.x = hitLocation.x;	closestHitLocation.y = hitLocation.y;	closestHitLocation.z = hitLocation.z;
							closestPoint = i;
							closestHitDist = hitDistance;
						}
						//Say("bouncing off wall; distance from old point == %.2f", bounceDist);
					}

				}  // if (hitNormal.z < 0.25f)
				else
				{
					//Say("hit inclined floor %i", i);
					if (i == 0 && hitNormal.z > 0.0f)
						newLoc.z += magnitude / hitNormal.z;
				}
			}  // if (hitNormal.x != 0.0f || hitNormal.y != 0.0f)
			/*else
			{
			Shout("Error: hitNormal x and y both 0");
			}*/
		}
	}

	if (!glancing && !noglance && closestPoint > -1)
	{
		// create glancing vector

		if (closestWallNormal.z != 0.0f)
		{
			// 'rotate' by extending the x and y to a 2d unit vector
			float normal2dMag = sqrt(closestWallNormal.x * closestWallNormal.x + closestWallNormal.y * closestWallNormal.y);
			closestWallNormal.x /= normal2dMag;
			closestWallNormal.y /= normal2dMag;
			closestWallNormal.z = 0.0f;
		}

		// dot product of wall normal and push vector
		float hitNormalDotp = closestWallNormal.x * -pushUnitV.x + closestWallNormal.y * pushUnitV.y;
		if (hitNormalDotp < 0.0f)
			hitNormalDotp = -hitNormalDotp;

		if (hitNormalDotp > 0.99999f)
			hitNormalDotp = 0.99999f;

		// create perpendicular vector of wall normal
		float perpNormalX = -closestWallNormal.y;
		float perpNormalY = closestWallNormal.x;

		float perpDotp = perpNormalX * -pushUnitV.x + perpNormalY * pushUnitV.y;

		// two possible perpendicular vectors; figure out which one to use
		if (perpDotp < 0.0f)
		{
			perpNormalX = -perpNormalX;
			perpNormalY = -perpNormalY;
		}

		if (glanceMag == 0.0f)
			glanceMag = magnitude;

		// reduce push magnitude the more the push vector is perpendicular to wall
		float angle = acosf(hitNormalDotp) * 57.2957795f;
		if (angle < 5.0f)		// prevent the walls from being 'too sticky' if push force is perpendicular
			angle = 5.0f;
		glanceV.x = -perpNormalX * glanceMag * (angle / 90.0f);
		glanceV.y = perpNormalY * glanceMag * (angle / 90.0f);

		if (glanceV.x != 0.0f || glanceV.y != 0.0f)
			glancing = true;
		//	Say("Glancing the wall; angle: %.3f  glanceMag(before): %.3f, glanceMag(reduced): %.3f  perpNormal: %.3f, %.3f  wallNormal %.3f, %.3f  wallNormalDotp: %.3f  glanceV: %.3f, %.3f",
		//		angle, glanceMag, (glanceMag * (angle / 90.0f)), perpNormalX, perpNormalY, closestWallNormal.x, closestWallNormal.y, hitNormalDotp, glanceV.x, glanceV.y);

	}

	float pushedDist = 0.0f;

	if ((pointsHit == 0 || bounce) && (newLoc.x != currentLoc.x || newLoc.y != currentLoc.y))
	{
		bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(newLoc) || zone->IsWaterZone(newLoc.z);
		if (!IsUnderwaterOnly() && !in_liquid)
		{
			newz = zone->zonemap->FindBestZ(newLoc, nullptr);
			if (newz != BEST_Z_INVALID)
			{
				newLoc.z = SetBestZ(newz);
			}
			else {
				push_vector.x = 0.0f;
				push_vector.y = 0.0f;
				push_vector.z = 0.0f;
				return 0.0f;
			}
		}

		if (!zone->zonemap->LineIntersectsZone(currentLoc, newLoc, 0.0f, &hitLocation, nullptr, &hitDistance))
		{
			pushedDist += magnitude;
			if (!glancing || noglance) {
				glm::vec4 new_pos(newLoc, m_Position.w);
				Teleport(new_pos);
			}
			else {
				m_Position.x = newLoc.x;
				m_Position.y = newLoc.y;
				m_Position.z = newLoc.z;
			}
			//Shout(" -- Setting position to: %.2f, %.2f, %.2f; push magnitude: %.3f", m_Position.x, m_Position.y, m_Position.z, magnitude);
		}
		/*else
		{
			Shout(" --- No LOS to newLoc from currentLoc; hitDist: %.2f  newLoc: %.2f, %.2f, %.2f  hitLoc: %.2f, %.2f, %.2f  mag: %.2f",
				hitDistance, newLoc.x, newLoc.y, newLoc.z, hitLocation.x, hitLocation.y, hitLocation.z, magnitude);
		}*/
	}
	if ((wall_normal1_x || wall_normal1_y) && (wall_normal2_x || wall_normal2_y) && GetZoneID() != Zones::POWATER)
	{
		// NPC is touching two walls
		float angle = wall_normal1_x * wall_normal2_x + wall_normal1_y * wall_normal2_y;
		if (angle < 0.72f)
		{
			corner_x = m_Position.x;
			corner_y = m_Position.y;
		}
	}

	push_vector.x = 0.0f;
	push_vector.y = 0.0f;
	push_vector.z = 0.0f;

	if (glancing)
	{
		push_vector.x = glanceV.x;
		push_vector.y = glanceV.y;
		pushedDist += ApplyPushVector(true);
	}
	//else if (inCorner)
	//	Say(inCorner ? " -- IN CORNER -- points hit == %i" : " not in corner; points hit == %i", pointsHit);

	return pushedDist;
}

bool NPC::IsBoat()
{
	return (GetBaseRace() == SHIP || GetBaseRace() == LAUNCH || GetBaseRace() == CONTROLLED_BOAT || GetBaseRace() == GHOST_SHIP);
}


void NPC::ShowQuickStats(Client* c)
{
	//This is just #npcstats, but accessible using #showstats 1
	c->Message(Chat::White, "NPC Stats:");
	c->Message(Chat::White, "Name: %s  NpcID: %u EntityID: %i", GetName(), GetNPCTypeID(), GetID());
	c->Message(Chat::White, "Race: %i  Level: %i  Class: %i  Material: %i", GetRace(), GetLevel(), GetClass(), GetTexture());
	c->Message(Chat::White, "Current HP: %i  Max HP: %i Per: %0.2f", GetHP(), GetMaxHP(), GetHPRatio());
	c->Message(Chat::White, "Min Damage: %i Max Damage: %i", GetMinDMG(), GetMaxDMG());
	c->Message(Chat::White, "Gender: %i  Size: %f  Bodytype: %d", GetGender(), GetSize(), GetBodyType());
	c->Message(Chat::White, "Runspeed: %f  Walkspeed: %f FlyMode: %d", GetRunspeed(), GetWalkspeed(), GetFlyMode());
	c->Message(Chat::White, "Spawn Group: %i  Grid: %i", GetSp2(), GetGrid());
	c->Message(Chat::White, "EmoteID: %i", GetEmoteID());
	DisplayAttackTimer(c);
	QueryLoot(c);
	if (IsCasting())
		c->Message(Chat::White, "NPC is currently casting spell: %s (id %u); remaining cast time: %0.3f seconds", 
			spells[casting_spell_id].name, casting_spell_id, static_cast<float>(spellend_timer.GetRemainingTime()) / 1000.0f);
	if (IsWalled())
	{
		c->Message(Chat::White, "NPC is walled.  Wall 1 normal: %0.3f, %0.3f", wall_normal1_x, wall_normal1_y);
		if (wall_normal2_x || wall_normal2_y)
			c->Message(Chat::White, "NPC is walled.  Wall 2 normal: %0.3f, %0.3f", wall_normal2_x, wall_normal2_y);
	}
	if (corner_x || corner_y)
		c->Message(Chat::White, "NPC is cornered.  Corner loc: %0.3f, %0.3f", corner_x, corner_y);
}

void NPC::ClearPathing()
{
	roambox_max_x = 0;
	roambox_max_y = 0;
	roambox_min_x = 0;
	roambox_min_y = 0;
	roambox_distance = false;
	roambox_movingto_x = 0;
	roambox_movingto_y = 0;
	roambox_movingto_z = 0;
	roambox_min_delay = 2500;
	roambox_delay = 2500;
	SetGrid(0);
	roamer = false;
}

uint8 NPC::GetGreedPercent()
{
	if (greed)
		return greed;

	if (RuleB(Merchant, UseGreed) && shop_count >= RuleI(Merchant, GreedThreshold))
	{
		uint8 greedper = 0;
		for (int i = 1; i < 11; ++i)
		{
			if (shop_count >= i * RuleI(Merchant, GreedThreshold))
			{
				greedper = i*3;
			}
			else
			{
				break;
			}
		}
		Log(Logs::General, Logs::Trading, "Greed for %s is set to %d percent.", GetName(), greedper);
		return greedper;
	}

	return 0;
}

uint8 NPC::Disarm(float chance)
{
	if (!IsNPC())
		return 0;

	if (GetSpecialAbility(IMMUNE_DISARM))
	{
		Log(Logs::General, Logs::Skills, "%s is immune to disarm!", GetName());
		return 0;
	}

	LootItem* weapon = GetItem(EQ::invslot::slotPrimary);
	if (weapon)
	{
		if (zone->random.Roll(chance))
		{
			uint16 weaponid = weapon->item_id;
			const EQ::ItemInstance* inst = database.CreateItem(weaponid);

			if (inst)
			{
				// No drop weapons not disarmable
				if (inst->GetItem()->Magic)
					return 0;

				if (inst->GetItem()->NoDrop == 0)
				{
					MoveItemToGeneralInventory(weapon);
				}
				else
				{
					RemoveItem(weapon);
					entity_list.CreateGroundObject(weaponid, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, DisarmDecayTime));
				}

				WearChange(EQ::textures::weaponPrimary, 0, 0);
				CalcBonuses();
				SetPrimSkill(EQ::skills::SkillHandtoHand);

				safe_delete(inst);

				// NPC has a weapon and was disarmed.
				if (CanTalk())
					Say_StringID(DISARM_SUCCESS);

				return 2;
			}
		}
		else
		{
			// NPC has a weapon, but the roll failed.
			return 1;
		}
	}

	return 0;
}

std::string NPC::GetSpawnedString()
{
	uint32 sp2 = GetSpawnPointID();
	std::string spawn2 = "Scripted";
	if (IsPet())
	{
		spawn2 = "Pet";
	}
	else if (sp2 > RANDOM_SPAWNID)
	{
		spawn2 = "Random";
	}
	else if (sp2 > 0)
	{
		spawn2 = std::to_string(sp2);
	}

	return spawn2;
}

// this assumes that 100 == no haste/slow and returns values > 100
int NPC::GetHasteCap()
{
	// from decompiles
	if (IsPet() && !IsCharmedPet() && GetSummonerID())
	{
		int cap = 110 + GetLevel();
		if (GetSummonerID())
		{
			Mob* owner = entity_list.GetMobID(GetSummonerID());

			if (owner)
				cap += std::max(0, owner->GetLevel() - 39) + std::max(0, owner->GetLevel() - 60);
		}

		return cap;
	}
	else
	{
		return 250;
	}
}

void NPC::SetSkill(EQ::skills::SkillType skill_num, uint16 value)
{
	if (skill_num > EQ::skills::HIGHEST_SKILL)
		return;
	
	skills[skill_num] = value;
}

bool NPC::IsGuildInFTELockout(uint32 guild_id)
{
	if (guild_id == GUILD_NONE || guild_id == 0)
		return true;

	auto itr = guild_fte_lockouts.find(guild_id);

	if (itr != guild_fte_lockouts.end())
		return true;

	return false;
}

void NPC::InsertGuildFTELockout(uint32 guild_id)
{

	if (guild_id == GUILD_NONE || guild_id == 0)
		return;

	auto itr = guild_fte_lockouts.find(guild_id);

	if (itr != guild_fte_lockouts.end())
		return;

	guild_fte_lockouts[guild_id] = Timer::GetCurrentTime();

}

void NPC::ProcessFTE()
{
	//engage notice lockout processing
	if (HasEngageNotice())
	{
		for (std::map<uint32, uint32>::iterator it = guild_fte_lockouts.begin(); it != guild_fte_lockouts.end();)
		{
			if (Timer::GetCurrentTime() >= it->second + RuleI(Quarm, GuildFTELockoutTimeMS))
			{
				std::string guild_string = "";
				if (it->first != GUILD_NONE) {
					guild_mgr.GetGuildNameByID(it->first, guild_string);
				}
				if (!guild_string.empty())
				{
					//entity_list.Message(0, 15, "Guild %s is no longer FTE locked out of %s!", guild_string.c_str(), GetCleanName());
				}
				it = guild_fte_lockouts.erase(it);
				continue;
			}
			it++;
		}
	}

	//ssf damage tracking
	if ((float)cur_hp >= ((float)max_hp * (level <= 5 ? 0.995f : 0.997f))) // reset FTE
	{
		solo_group_fte = 0;
		solo_raid_fte = 0;
		solo_fte_charid = 0;
		ssf_player_damage = 0;
		ssf_ds_damage = 0;
	}

	//guild fte
	bool send_engage_notice = HasEngageNotice();
	if (send_engage_notice)
	{
		bool no_guild_fte = guild_fte == GUILD_NONE;
		if (!no_guild_fte)
		{
			uint32 curtime = (Timer::GetCurrentTime());
			uint32 lastAggroTime = GetAggroTime();
			bool is_engaged = lastAggroTime != 0xFFFFFFFF;
			bool engaged_too_long = false;
			if (is_engaged)
				engaged_too_long = curtime >= lastAggroTime + RuleI(Quarm, GuildFTEDisengageTimeMS) && hate_list.GetNumHaters() > 0 && (float)cur_hp >= ((float)max_hp * (0.97f));
			bool no_haters = hate_list.GetNumHaters() == 0;
			bool valid_guild_on_hatelist = guild_fte != GUILD_NONE;
			bool is_guild_on_hate_list = hate_list.IsGuildOnHateList(guild_fte);
			//If we're engaged for 60 seconds and still have the same fte at 97% or above HP, clear fte, and memwipe if engage notice target.
			if (engaged_too_long || no_haters || valid_guild_on_hatelist && !is_guild_on_hate_list)
			{
				HandleFTEDisengage();
				SetAggroTime(0xFFFFFFFF);
			}
		}
	}

	bool no_fte = fte_charid == 0 && raid_fte == 0 && group_fte == 0;
	if (!no_fte)
	{
		uint32 curtime = (Timer::GetCurrentTime());
		uint32 lastAggroTime = GetAggroTime();
		bool is_engaged = lastAggroTime != 0xFFFFFFFF;
		bool engaged_too_long = false;
		if (is_engaged)
			engaged_too_long = curtime >= lastAggroTime + RuleI(Quarm, GuildFTEDisengageTimeMS) && hate_list.GetNumHaters() > 0 && (float)cur_hp >= ((float)max_hp * (0.97f));
		bool no_haters = hate_list.GetNumHaters() == 0;

		//If we're engaged for 60 seconds and still have the same fte at 97% or above HP, clear fte, and memwipe if engage notice target.
		if (engaged_too_long || no_haters)
		{
			fte_charid = 0;
			group_fte = 0;
			raid_fte = 0;
		}
	}
}