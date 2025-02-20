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
#include "../common/strings.h"
#include "../common/zone_store.h"
#include "../common/misc_functions.h"

#include "data_bucket.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"
#include "mob_movement_manager.h"

#include <limits.h>
#include <math.h>
#include <sstream>
#include "map.h"

extern EntityList entity_list;

extern Zone* zone;
extern WorldServer worldserver;

Mob::Mob(const char* in_name,
		const char* in_lastname,
		int32		in_cur_hp,
		int32		in_max_hp,
		uint8		in_gender,
		uint16		in_race,
		uint8		in_class,
		uint8		in_bodytype,
		uint8		in_deity,
		uint8		in_level,
		uint32		in_npctype_id,
		float		in_size,
		float		in_runspeed,
		const glm::vec4& position,
		uint8		in_light,
		uint8		in_texture,
		uint8		in_helmtexture,
		uint16		in_ac,
		uint16		in_atk,
		uint16		in_str,
		uint16		in_sta,
		uint16		in_dex,
		uint16		in_agi,
		uint16		in_int,
		uint16		in_wis,
		uint16		in_cha,
		uint8		in_haircolor,
		uint8		in_beardcolor,
		uint8		in_eyecolor1, // the eyecolors always seem to be the same, maybe left and right eye?
		uint8		in_eyecolor2,
		uint8		in_hairstyle,
		uint8		in_luclinface,
		uint8		in_beard,
		EQ::TintProfile		in_armor_tint,
		uint8		in_aa_title,
		uint8		in_see_invis, // see through invis/ivu
		uint8		in_see_invis_undead,
		uint8		in_see_sneak,
		uint8		in_see_improved_hide,
		int32		in_hp_regen,
		int32		in_mana_regen,
		uint8		in_qglobal,
		uint8		in_maxlevel,
		uint32		in_scalerate,
		uint8		in_armtexture,
		uint8		in_bracertexture,
		uint8		in_handtexture,
		uint8		in_legtexture,
		uint8		in_feettexture,
		uint8		in_chesttexture
		) :
		attack_timer(2000),
		attack_dw_timer(2000),
		ranged_timer(2000),
		tic_timer(6000),
		mana_timer(2000),
		spellend_timer(0),
		rewind_timer(30000), //Timer used for determining amount of time between actual player position updates for /rewind.
		bindwound_timer(10000),
		stunned_timer(0),
		spun_timer(0),
		anim_timer(250),
		bardsong_timer(6000),
		m_FearWalkTarget(BEST_Z_INVALID,BEST_Z_INVALID,BEST_Z_INVALID),
		m_TargetLocation(glm::vec3()),
		m_TargetV(glm::vec3()),
		flee_timer(FLEE_CHECK_TIMER),
		m_Position(position),
		position_update_melee_push_timer(500)
{
	mMovementManager = &MobMovementManager::Get();
	mMovementManager->AddMob(this);

	targeted = 0;
	tar_ndx=0;
	m_DeltaV = glm::vec3(0.0f, 0.0f, 0.0f);
	AI_Init();
	SetMoving(false);
	moved=false;
	m_RewindLocation = glm::vec3();
	m_LastLocation = glm::vec4();
	name[0]=0;
	orig_name[0]=0;
	clean_name[0]=0;
	clean_name_spaces[0]=0;
	lastname[0]=0;
	if(in_name) {
		strn0cpy(name,in_name,64);
		strn0cpy(orig_name,in_name,64);
	}
	if (in_lastname) {
		strn0cpy(lastname, in_lastname, 64);
	}
	cur_hp		= in_cur_hp;
	max_hp		= in_max_hp;
	base_hp		= in_max_hp;
	gender		= in_gender;
	race		= in_race;
	base_gender	= in_gender;
	base_race	= in_race;
	class_		= in_class;
	bodytype	= in_bodytype;
	orig_bodytype = in_bodytype;
	deity		= in_deity;
	level		= in_level;
	orig_level = in_level;
	npctype_id	= in_npctype_id;
	size		= in_size;
	base_size	= in_size;
	runspeed	= in_runspeed;
	current_speed = 0.0f;

	z_offset = CalcZOffset();
	head_offset = CalcHeadOffset();
	model_size = CalcModelSize();
	model_bounding_radius = CalcBoundingRadius();
	// sanity check
	if (runspeed < 0.0f || runspeed > 20.0f) {
		runspeed = 1.25f; // mob speeds on mac client, only support increments of 0.1
	}

	int_runspeed = (int)((float)runspeed * 40.0f) + 2; // add the +2 to give a round behavior - int speeds are increments of 4
	base_runspeed = (int)((float)runspeed * 40.0f);
	base_walkspeed = base_runspeed * 100 / 265;
	base_fearspeed = base_runspeed * 100 / 127;
	// clients - these look low, compared to mobs.  But player speeds translate different in client, with respect to NPCs.
	// in game a 0.7 speed for client, is same as a 1.4 speed for NPCs.
	if (runspeed == 0.7f) {
		int_runspeed = 28;
		walkspeed = 0.46f;
		int_walkspeed = 12;
		fearspeed = 0.625f;
		int_fearspeed = 24;
	// npcs
	} else {
		int_walkspeed = int_runspeed * 100 / 265;
		walkspeed = (float)int_walkspeed / 40.0f;
		int_fearspeed = int_runspeed * 100 / 127;
		fearspeed = (float)int_fearspeed / 40.0f;
	}
	// adjust speeds to round to 0.100
	int_runspeed = (int_runspeed >> 2) << 2; // equivalent to divide by 4, then multiply by 4
	runspeed = (float)int_runspeed / 40.0f;
	int_walkspeed = (int_walkspeed >> 2) << 2;
	//walkspeed = (float)int_walkspeed / 40.0f;
	int_fearspeed = (int_fearspeed >> 2) << 2;
	fearspeed = (float)int_fearspeed / 40.0f;

	m_Light.Type[EQ::lightsource::LightInnate] = in_light;
	m_Light.Level[EQ::lightsource::LightInnate] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightInnate]);
	m_Light.Type[EQ::lightsource::LightActive] = m_Light.Type[EQ::lightsource::LightInnate];
	m_Light.Level[EQ::lightsource::LightActive] = m_Light.Level[EQ::lightsource::LightInnate];

	
	texture		= in_texture;
	chesttexture = in_chesttexture;
	helmtexture	= in_helmtexture;
	helmtexture_quarm = in_helmtexture;
	armtexture = in_armtexture;
	bracertexture = in_bracertexture;
	handtexture = in_handtexture;
	legtexture = in_legtexture;
	feettexture = in_feettexture;
	multitexture = (chesttexture || armtexture || bracertexture || handtexture || legtexture || feettexture);

	haircolor	= in_haircolor;
	beardcolor	= in_beardcolor;
	eyecolor1	= in_eyecolor1;
	eyecolor2	= in_eyecolor2;
	hairstyle	= in_hairstyle;
	luclinface	= in_luclinface;
	beard		= in_beard;
	if (luclinface == 254) {	// note: a face value of 254 will result in random face/hair/eye characteristics
		SetRandomFeatures();
	}

	attack_delay = 0;
	slow_mitigation = 0;
	has_shieldequiped = false;
	has_bashEnablingWeapon = false;
	has_MGB = false;
	has_ProjectIllusion = false;
	last_los_check = false;
	last_dest = glm::vec3(99999.0f, 99999.0f, 99999.0f);

	if (in_aa_title > 0) {
		aa_title = in_aa_title;
	}
	else {
		aa_title = 0xFF;
	}
	AC		= in_ac;
	ATK		= in_atk;
	STR		= in_str;
	STA		= in_sta;
	DEX		= in_dex;
	AGI		= in_agi;
	INT		= in_int;
	WIS		= in_wis;
	CHA		= in_cha;
	MR = CR = FR = DR = PR = 0;

	ExtraHaste = 0;
	bEnraged = false;

	shield_target = nullptr;
	cur_mana = 0;
	max_mana = 0;
	hp_regen = in_hp_regen;
	mana_regen = in_mana_regen;
	oocregen = RuleI(NPC, OOCRegen); //default Out of Combat Regen
	maxlevel = in_maxlevel;
	scalerate = in_scalerate;
	invisible = false;
	invisible_undead = false;
	invisible_animals = false;
	sneaking = false;
	hidden = false;
	improved_hidden = false;
	invulnerable = false;
	IsFullHP	= (cur_hp == max_hp);
	qglobal=0;
	spawnpacket_sent = false;
	spawned = false;
	rare_spawn = false;

	InitializeBuffSlots();

	// clear the proc arrays
	int i;
	int j;
	for (j = 0; j < MAX_PROCS; j++)	{
		SpellProcs[j].spellID = SPELL_UNKNOWN;
		SpellProcs[j].chance = 0;
		SpellProcs[j].base_spellID = SPELL_UNKNOWN;
	}

	for (i = 0; i < EQ::textures::materialCount; i++) {
		armor_tint.Slot[i].Color = in_armor_tint.Slot[i].Color;
	}

	m_Delta = glm::vec4();
	animation = 0;

	isgrouped = false;
	israidgrouped = false;
	entity_id_being_looted = 0;
	_appearance = eaStanding;
	pRunAnimSpeed = 0;

	ZeroCastingVars();
	_StopSong();
	spellrecovery_timer.Disable();
	target = 0;

	memset(&itembonuses, 0, sizeof(StatBonuses));
	memset(&spellbonuses, 0, sizeof(StatBonuses));
	memset(&aabonuses, 0, sizeof(StatBonuses));
	spellbonuses.AggroRange = -1;
	spellbonuses.AssistRange = -1;
	SetPetID(0);
	SetOwnerID(0);
	typeofpet = petCharmed;		//default to charmed...
	summonerid = 0;
	summonedClientPet = false;
	petpower = 0;
	held = false;
	nocast = false;
	_IsTempPet = false;
	pet_owner_client = false;

	attacked_count = 0;
	mezzed = false;
	stunned = false;
	silenced = false;
	amnesiad = false;
	inWater = false;
	shielder = nullptr;
	shield_target = nullptr;
	shield_cooldown = 0;

	wandertype=0;
	pausetype=0;
	cur_wp = 0;
	m_CurrentWayPoint = glm::vec4();
	cur_wp_pause = 0;
	patrol=0;
	follow=0;
	follow_dist = 100;	// Default Distance for Follow
	flee_mode = false;
	curfp = false;
	flee_timer.Start();

	permarooted = (runspeed > 0.0f) ? false : true;

	roamer = false;
	pause_timer_complete = false;
	rooted = false;
	charmed = false;
	blind = false;

	pStandingPetOrder = SPO_Follow;
	pAggroRange = 0;
	pAssistRange = 0;
	pseudo_rooted = false;

	see_invis = GetSeeInvisible(in_see_invis);
	see_invis_undead = GetSeeInvisible(in_see_invis_undead);
	see_sneak = GetSeeInvisible(in_see_sneak);
	see_improved_hide = GetSeeInvisible(in_see_improved_hide);
	qglobal = in_qglobal != 0;

	// Bind wound
	bindwound_timer.Disable();
	bindwound_target_id = 0;

	trade = new Trade(this);
	// hp event
	nexthpevent = -1;
	nextinchpevent = -1;

	hasTempPet = false;
	count_TempPet = 0;

	//Boats always "run." Ignore launches and player controlled ships.
	if((GetBaseRace() == Race::Ship || GetBaseRace() == Race::GhostShip) && RuleB(NPC, BoatsRunByDefault)) {
		m_is_running = true;
	}
	else {
		m_is_running = false;
	}

	m_targetable = true;

	flymode = GravityBehavior::Water;

	hate_list.SetOwner(this);

	m_AllowBeneficial = false;
	m_DisableMelee = false;

	emoteid = 0;
	combat_hp_regen = 0;
	combat_mana_regen = 0;
	iszomm = false;
	adjustedz = 0;
	engaged = false;
	MerchantSession = 0;
	dire_charmed = false;
	feared = false;
	player_damage = 0;
	ssf_player_damage = 0;
	ssf_ds_damage = 0;
	dire_pet_damage = 0;
	total_damage = 0;
	ds_damage = 0;
	npc_damage = 0;
	gm_damage = 0;
	PacifyImmune = false;
	current_buff_refresh = false;
	temporary_pets_effect = nullptr;
	best_z_fail_count = 0;

	instillDoubtTargetID = 0;
}

Mob::~Mob()
{
	quest_manager.stopalltimers(this);

	mMovementManager->RemoveMob(this);

	AI_Stop();
	if (GetPet()) {
		if (GetPet()->IsCharmedPet()) {
			GetPet()->BuffFadeByEffect(SE_Charm);
		}
		else {
			SetPet(0);
		}
	}

	EQApplicationPacket app;
	CreateDespawnPacket(&app, !IsCorpse());
	Corpse* corpse = entity_list.GetCorpseByID(GetID());
	if(!corpse || (corpse && !corpse->IsPlayerCorpse()))
		entity_list.QueueClients(this, &app, true);

	entity_list.RemoveFromTargets(this);
	EndShield();

	if(trade) {
		Mob *with = trade->With();
		if(with && with->IsClient()) {
			with->CastToClient()->FinishTrade(with);
			with->trade->Reset();
		}
		safe_delete(trade);
	}

	if(HasTempPetsActive()) {
		entity_list.DestroyTempPets(this);
	}
	UninitializeBuffSlots();
}

uint32 Mob::GetAppearanceValue(EmuAppearance iAppearance) {
	switch (iAppearance) {
		// 0 standing, 1 sitting, 2 ducking, 3 lieing down, 4 looting
		case eaStanding: {
			return Animation::Standing;
		}
		case eaSitting: {
			return Animation::Sitting;
		}
		case eaCrouching: {
			return Animation::Crouching;
		}
		case eaDead: {
			return Animation::Lying;
		}
		case eaLooting: {
			return Animation::Looting;
		}
		//to shup up compiler:
		case _eaMaxAppearance:
			break;
	}
	return(Animation::Standing);
}

void Mob::SetInvisible(uint8 state, bool showInvis, bool skipSelf)
{
	if(state == INVIS_OFF || state == INVIS_NORMAL || state == INVIS_HIDDEN)
	{
		invisible = (bool) state;

		if (GetClass() == Class::Rogue)
		{
			// Rogues also get IVU when invisible/hidden.
			invisible_undead = (bool) state;
		}
	}

	if(showInvis) 
	{
		SendAppearancePacket(AppearanceType::Invisibility, state, true, skipSelf);
	}

	// Invis and hide breaks charms
	if (HasPet() && state != INVIS_OFF)
	{
		FadePetCharmBuff();
		DepopPet();
	}
}

//check to see if `this` is invisible to `other`
bool Mob::IsInvisible(Mob* other) const
{
	if(!other)
		return(false);

	if(IsClient() && other->IsClient() && other->SeeInvisible())
		return (false);

	//check regular invisibility
	if (invisible && invisible > (other->SeeInvisible()))
		return true;

	//check invis vs. undead
	if (other->GetBodyType() == BodyType::Undead || other->GetBodyType() == BodyType::SummonedUndead) {
		if(invisible_undead && !other->SeeInvisibleUndead())
			return true;
	}

	//check invis vs. animals...
	if (other->GetBodyType() == BodyType::Animal){
		if(invisible_animals && !other->SeeInvisible())
			return true;
	}

	if(hidden)
	{
		if (RuleB(Quarm, PreNerfSneakHide) && GetClass() == Class::Rogue)
		{
			if (!other->SeeSneak())
				return true;
		}
		else
		{
			if (!other->SeeInvisible()) {
				return true;
			}
		}
	}

	if(improved_hidden){
		if(!other->see_improved_hide){
			return true;
		}
	}

	//handle sneaking
	if(!other->SeeSneak() && sneaking) {
		if(!InFrontMob(other, GetX(), GetY()))
			return true;
	}

	return(false);
}

int Mob::_GetWalkSpeed() const {
 
    if (IsRooted() || IsStunned() || IsMezzed())
		return 0;
	
	int aa_mod = 0;
	int speed_mod = int_walkspeed;
	int base_run = int_runspeed;
	bool has_horse = false;
	if (IsClient())
	{
		Mob* horse = entity_list.GetMob(CastToClient()->GetHorseId());
		if(horse)
		{
			speed_mod = horse->int_walkspeed;
			base_run = horse->int_runspeed;
			has_horse = true;
		} else {
			aa_mod += ((CastToClient()->GetAA(aaInnateRunSpeed))
				+ (CastToClient()->GetAA(aaFleetofFoot))
				+ (CastToClient()->GetAA(aaSwiftJourney))
			);
		}
		//Selo's Enduring Cadence should be +7% per level
	}
	
	int spell_mod = spellbonuses.movementspeed + itembonuses.movementspeed;
	int movemod = 0;

	if(spell_mod < 0)
	{
		movemod += spell_mod;
	}
	else if(spell_mod > aa_mod)
	{
		movemod = spell_mod;
	}
	else
	{
		movemod = aa_mod;
	}
	
	if(movemod < -85) //cap it at moving very very slow
		movemod = -85;
	
	if (!has_horse && movemod != 0)
		speed_mod += (base_run * movemod / 100);

	if(speed_mod < 4)
		return(0);

	//runspeed cap.
	if(IsClient())
	{
		if(GetClass() == Class::Bard) {
			//this extra-high bard cap should really only apply if they have AAs
			if(speed_mod > 72)
				speed_mod = 72;
		} else {
			if(speed_mod > 64)
				speed_mod = 64;
		}
	}
	speed_mod = (speed_mod >> 2) << 2;
	if (speed_mod < 4)
		return 0;
	return speed_mod;
}

int Mob::_GetRunSpeed() const {
	if (IsRooted() || IsStunned() || IsMezzed())
		return 0;
	
	int aa_mod = 0;
	int speed_mod = int_runspeed;
	int base_walk = int_walkspeed;
	bool has_horse = false;
	if (IsClient())
	{
		if(CastToClient()->GetGMSpeed())
		{
			speed_mod = 124; // this is same as speed of 3.1f
		}
		else
		{
			Mob* horse = entity_list.GetMob(CastToClient()->GetHorseId());
			if(horse)
			{
				speed_mod = horse->int_runspeed;
				base_walk = horse->int_walkspeed;
				has_horse = true;
			}
		}
		aa_mod += itembonuses.BaseMovementSpeed + spellbonuses.BaseMovementSpeed + aabonuses.BaseMovementSpeed;

	}
	
	int spell_mod = spellbonuses.movementspeed + itembonuses.movementspeed;
	int movemod = 0;

	if(spell_mod < 0)
	{
		movemod += spell_mod;
	}
	else if(spell_mod > aa_mod)
	{
		movemod = spell_mod;
	}
	else
	{
		movemod = aa_mod;
	}
	
	if(movemod < -85) //cap it at moving very very slow
		movemod = -85;
	
	if (!has_horse && movemod != 0)
	{
		if (IsClient())
		{
			speed_mod += (speed_mod * movemod / 100);
		} else {
			if (movemod < 0) {

				speed_mod += (50 * movemod / 100);

				// cap min snare speed when running to walk speed
				if(speed_mod < int_walkspeed)
					return(int_walkspeed);

			} else {
				speed_mod += int_walkspeed;
				if (movemod > 50)
					speed_mod += 4;
				if (movemod > 40)
					speed_mod += 3;
			}
		}
	}

	if (speed_mod < int_walkspeed)
		return(int_walkspeed);

	//runspeed cap.
	if(IsClient())
	{
		if(GetClass() == Class::Bard) {
			//this extra-high bard cap should really only apply if they have AAs
			if(speed_mod > 72)
				speed_mod = 72;
		} else {
			if(speed_mod > 64)
				speed_mod = 64;
		}
	}
	speed_mod = (speed_mod >> 2) << 2;
	if (speed_mod < int_walkspeed)
		return(int_walkspeed);
	return (speed_mod);
}

int Mob::_GetFearSpeed() const {

	if (IsRooted() || IsStunned() || IsMezzed())
		return 0;

	//float speed_mod = fearspeed;
	int speed_mod = int_fearspeed;

	// use a max of 1.8f in calcs.
	int base_run = std::min(int_runspeed, 72);

	int spell_mod = spellbonuses.movementspeed + itembonuses.movementspeed;

	int movemod = 0;

	if (spell_mod < 0)
	{
		movemod += spell_mod;
	}

	if (movemod < -85) //cap it at moving very very slow
		movemod = -85;

	if (IsClient()) {
		if (CastToClient()->GetRunMode())
			speed_mod = int_fearspeed;
		else
			speed_mod = int_walkspeed;
		if (movemod < 0)
			return int_walkspeed;
		speed_mod += (base_run * movemod / 100);
		speed_mod = (speed_mod >> 2) << 2;
		if (speed_mod < 4)
			return 0;
		return (speed_mod);
	}
	else {
		float hp_ratio = GetHPRatio();
		// very large snares 50% or higher
		if (movemod < -49)
		{
			if (hp_ratio < 25)
				return (0);
			if (hp_ratio < 50)
				return (8);
			else
				return (12);
		}
		if (hp_ratio < 5) {
			speed_mod = int_walkspeed / 3 + 4;
		}
		else if (hp_ratio < 15) {
			speed_mod = int_walkspeed / 2 + 4;
		}
		else if (hp_ratio < 25) {
			speed_mod = int_walkspeed + 4; // add this, so they are + 0.1 so they do the run animation
		}
		else if (hp_ratio < 50) {
			speed_mod *= 82;
			speed_mod /= 100;
		}
		if (movemod > 0) {
			speed_mod += int_walkspeed;
			if (movemod > 50)
				speed_mod += 4;
			if (movemod > 40)
				speed_mod += 3;
			speed_mod = (speed_mod >> 2) << 2;
			if (speed_mod < 4)
				return 0;
			return speed_mod;
		}
		else if (movemod < 0) {
			speed_mod += (base_run * movemod / 100);
		}
	}
	if (speed_mod < 4)
		return (0);
	if (speed_mod < 12)
		return (8); // 0.2f is slow walking - this should be the slowest walker for fleeing, if not snared enough to stop
	speed_mod = (speed_mod >> 2) << 2;
	if (speed_mod < 4)
		return 0;
	return speed_mod;
}

int32 Mob::CalcMaxMana() {
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
}

int32 Mob::CalcMaxHP(bool unbuffed) {
	max_hp = (base_hp + itembonuses.HP + spellbonuses.HP);
	max_hp += max_hp * ((aabonuses.MaxHPChange + spellbonuses.MaxHPChange + itembonuses.MaxHPChange) / 10000.0f);

	if (cur_hp > max_hp)
		cur_hp = max_hp;

	return max_hp;
}

int32 Mob::GetItemHPBonuses() {
	int32 item_hp = 0;
	item_hp = itembonuses.HP;
	item_hp += item_hp * itembonuses.MaxHPChange / 10000;
	return item_hp;
}

int32 Mob::GetSpellHPBonuses() {
	int32 spell_hp = 0;
	spell_hp = spellbonuses.HP;
	spell_hp += spell_hp * spellbonuses.MaxHPChange / 10000;
	return spell_hp;
}

char Mob::GetCasterClass() const {
	switch(class_)
	{
	case Class::Cleric:
	case Class::Paladin:
	case Class::Ranger:
	case Class::Druid:
	case Class::Shaman:
	case Class::Beastlord:
	case Class::ClericGM:
	case Class::PaladinGM:
	case Class::RangerGM:
	case Class::DruidGM:
	case Class::ShamanGM:
	case Class::BeastlordGM:
		return 'W';
		break;

	case Class::ShadowKnight:
	case Class::Bard:
	case Class::Necromancer:
	case Class::Wizard:
	case Class::Magician:
	case Class::Enchanter:
	case Class::ShadowKnightGM:
	case Class::BardGM:
	case Class::NecromancerGM:
	case Class::WizardGM:
	case Class::MagicianGM:
	case Class::EnchanterGM:
		return 'I';
		break;

	default:
		return 'N';
		break;
	}
}

uint8 Mob::GetArchetype() const {
	switch(class_)
	{
	case Class::Paladin:
	case Class::Ranger:
	case Class::ShadowKnight:
	case Class::Bard:
	case Class::Beastlord:
	case Class::PaladinGM:
	case Class::RangerGM:
	case Class::ShadowKnightGM:
	case Class::BardGM:
	case Class::BeastlordGM:
		return ARCHETYPE_HYBRID;
		break;
	case Class::Cleric:
	case Class::Druid:
	case Class::Shaman:
	case Class::Necromancer:
	case Class::Wizard:
	case Class::Magician:
	case Class::Enchanter:
	case Class::ClericGM:
	case Class::DruidGM:
	case Class::ShamanGM:
	case Class::NecromancerGM:
	case Class::WizardGM:
	case Class::MagicianGM:
	case Class::EnchanterGM:
		return ARCHETYPE_CASTER;
		break;
	case Class::Warrior:
	case Class::Monk:
	case Class::Rogue:
	case Class::WarriorGM:
	case Class::MonkGM:
	case Class::RogueGM:
		return ARCHETYPE_MELEE;
		break;
	default:
		return ARCHETYPE_HYBRID;
		break;
	}
}

void Mob::CreateSpawnPacket(EQApplicationPacket* app, Mob* ForWho) {
	app->SetOpcode(OP_NewSpawn);
	app->size = sizeof(NewSpawn_Struct);
	safe_delete_array(app->pBuffer);
	app->pBuffer = new uchar[app->size];
	memset(app->pBuffer, 0, app->size);
	NewSpawn_Struct* ns = (NewSpawn_Struct*)app->pBuffer;
	FillSpawnStruct(ns, ForWho);
}

void Mob::CreateSpawnPacket(EQApplicationPacket* app, NewSpawn_Struct* ns) {
	app->SetOpcode(OP_NewSpawn);
	app->size = sizeof(NewSpawn_Struct);

	safe_delete_array(app->pBuffer);
	app->pBuffer = new uchar[app->size];

	// Copy ns directly into packet
	memcpy(app->pBuffer, ns, app->size);

	// Custom packet data
	NewSpawn_Struct* ns2 = (NewSpawn_Struct*)app->pBuffer;
	strcpy(ns2->spawn.name, ns->spawn.name);
	strcpy(ns2->spawn.lastName, ns->spawn.lastName);

	memset(&app->pBuffer[sizeof(Spawn_Struct)-7], 0xFF, 7);
}

void Mob::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho)
{
	int i;

	strcpy(ns->spawn.name, name);
	if(IsClient()) {
		strn0cpy(ns->spawn.lastName, lastname, sizeof(ns->spawn.lastName));
	}

	ns->spawn.heading	= static_cast<uint16>(m_Position.w);
	ns->spawn.x			= m_Position.x;//((int32)x_pos)<<3;
	if (ns->spawn.x > m_Position.x)
		ns->spawn.x--;
	ns->spawn.y			= m_Position.y;//((int32)y_pos)<<3;
	if (ns->spawn.y > m_Position.y)
		ns->spawn.y--;
	ns->spawn.z			= m_Position.z;//((int32)z_pos)<<3;
	ns->spawn.spawnId	= GetID();
	ns->spawn.curHp	= static_cast<uint8>(GetHPRatio());
	ns->spawn.max_hp	= 100;		//this field needs a better name
	ns->spawn.race		= race;
	ns->spawn.runspeed	= runspeed;
	ns->spawn.walkspeed	= walkspeed;
	ns->spawn.class_	= class_;
	ns->spawn.gender	= gender;
	ns->spawn.level		= level;
	ns->spawn.deity		= deity;
	ns->spawn.animation	= animation;

	UpdateActiveLight();
	ns->spawn.light		= m_Light.Type[EQ::lightsource::LightActive];

	if (invisible || hidden || improved_hidden)
		ns->spawn.invis = 1;
	else if (invisible_undead)
		ns->spawn.invis = 3;
	else if (invisible_animals)
		ns->spawn.invis = 2;
	else
		ns->spawn.invis = 0;

	ns->spawn.NPC		= IsNPC() ? 1 : 0;
	ns->spawn.petOwnerId	= ownerid;
	ns->spawn.temporaryPet = IsTempPet();

	ns->spawn.haircolor = haircolor;
	ns->spawn.beardcolor = beardcolor;
	ns->spawn.eyecolor1 = eyecolor1;
	ns->spawn.eyecolor2 = eyecolor2;
	ns->spawn.hairstyle = hairstyle;
	ns->spawn.face = luclinface;
	ns->spawn.beard = beard;
	ns->spawn.StandState = GetAppearanceValue(_appearance);
	ns->spawn.bodytexture = texture;

	if(helmtexture && helmtexture != 0xFF)
	{
		ns->spawn.helm=helmtexture;
	} else {
		ns->spawn.helm = 0;
	}

	ns->spawn.guildrank	= 0xFF;
	ns->spawn.size			= size;
	ns->spawn.bodytype = bodytype;
	// The 'flymode' settings have the following effect:
	// 0 - Mobs in water sink like a stone to the bottom
	// 1 - Same as #flymode 1
	// 2 - Same as #flymode 2
	// 3 - Mobs in water do not sink. A value of 3 in this field appears to be the default setting for all mobs
	if (IsCorpse())
	{
		ns->spawn.flymode = GravityBehavior::Ground;
	}
	else if(IsClient())
	{
		ns->spawn.flymode = FindType(SE_Levitate) ? GravityBehavior::Levitating : GravityBehavior::Ground;
	}
	else
		ns->spawn.flymode = flymode;

	ns->spawn.lastName[0] = '\0';

	strn0cpy(ns->spawn.lastName, lastname, sizeof(ns->spawn.lastName));

	if (!IsClient())
	{
		// Client is handled in Client::FillSpawnStruct()
		for (i = 0; i < EQ::textures::materialCount; i++)
		{
			ns->spawn.equipment[i] = GetEquipmentMaterial(i);
			if (armor_tint.Slot[i].Color)
			{
				ns->spawn.colors.Slot[i].Color = armor_tint.Slot[i].Color;
			}
			else
			{
				ns->spawn.colors.Slot[i].Color = GetEquipmentColor(i);
			}
		}
	}

	memset(ns->spawn.set_to_0xFF, 0xFF, sizeof(ns->spawn.set_to_0xFF));
}

void Mob::CreateDespawnPacket(EQApplicationPacket* app, bool Decay)
{
	app->SetOpcode(OP_DeleteSpawn);
	app->size = sizeof(DeleteSpawn_Struct);
	safe_delete_array(app->pBuffer);
	app->pBuffer = new uchar[app->size];
	memset(app->pBuffer, 0, app->size);
	DeleteSpawn_Struct* ds = (DeleteSpawn_Struct*)app->pBuffer;
	ds->spawn_id = GetID();
}

void Mob::CreateHPPacket(EQApplicationPacket* app)
{
	this->IsFullHP=(cur_hp>=max_hp);
	app->SetOpcode(OP_MobHealth);
	app->size = sizeof(SpawnHPUpdate_Struct2);
	safe_delete_array(app->pBuffer);
	app->pBuffer = new uchar[app->size];
	memset(app->pBuffer, 0, app->size);
	SpawnHPUpdate_Struct2* ds = (SpawnHPUpdate_Struct2*)app->pBuffer;

	ds->spawn_id = GetID();
	// they don't need to know the real hp
	ds->hp = (int)GetHPRatio();

	// hp event
	if (IsNPC() && (GetNextHPEvent() > 0))
	{
		if (ds->hp < GetNextHPEvent())
		{
			std::string export_string = fmt::format("{}", GetNextHPEvent());
			SetNextHPEvent(-1);
			parse->EventNPC(EVENT_HP, CastToNPC(), nullptr, export_string, 0);
		}
	}

	if (IsNPC() && (GetNextIncHPEvent() > 0))
	{
		if (ds->hp > GetNextIncHPEvent())
		{
			std::string export_string = fmt::format("{}", GetNextIncHPEvent());
			SetNextIncHPEvent(-1);
			parse->EventNPC(EVENT_HP, CastToNPC(), nullptr, export_string, 1);
		}
	}
}

// sends hp update of this mob to people who might care
void Mob::SendHPUpdate(bool skipnpc, bool sendtoself)
{
	if (IsNPC() && skipnpc && RuleB(AlKabor, NPCsSendHPUpdatesPerTic))
		return;

	if (IsClient() && !CastToClient()->Connected())
		return;

	if(IsClient())
		Log(Logs::Detail, Logs::Regen, "%s is setting HP to %d Sending out an update.", GetName(), GetHP());

	EQApplicationPacket hp_app;
	Group *group = nullptr;

	// destructor will free the pBuffer
	CreateHPPacket(&hp_app);

	// send to people who have us targeted
	entity_list.QueueClientsByTarget(this, &hp_app, false, 0, true, true, EQ::versions::bit_AllClients);

	// send to group
	if(IsGrouped())
	{
		group = entity_list.GetGroupByMob(this);
		if(group) //not sure why this might be null, but it happens
			group->SendHPPacketsFrom(this);
	}

	if(IsClient()){
		Raid *r = entity_list.GetRaidByClient(CastToClient());
		if(r){
			r->SendHPPacketsFrom(this);
		}
	}

	// send to master
	if(GetOwner() && GetOwner()->IsClient())
	{
		GetOwner()->CastToClient()->QueuePacket(&hp_app, false);
		group = entity_list.GetGroupByClient(GetOwner()->CastToClient());
		if(group)
			group->SendHPPacketsFrom(this);
		Raid *r = entity_list.GetRaidByClient(GetOwner()->CastToClient());
		if(r)
			r->SendHPPacketsFrom(this);
	}

	// send to pet
	if(GetPet() && GetPet()->IsClient())
	{
		GetPet()->CastToClient()->QueuePacket(&hp_app, true);
	}

	// send to self - we need the actual hps here
	if(IsClient() && sendtoself)
	{
		auto hp_app2 = new EQApplicationPacket(OP_HPUpdate, sizeof(SpawnHPUpdate_Struct));
		SpawnHPUpdate_Struct* ds = (SpawnHPUpdate_Struct*)hp_app2->pBuffer;
		ds->cur_hp = CastToClient()->GetHP() - itembonuses.HP;
		ds->spawn_id = GetID();
		ds->max_hp = CastToClient()->GetMaxHP() - itembonuses.HP;
		CastToClient()->QueuePacket(hp_app2, true, Client::CLIENT_CONNECTED);
		safe_delete(hp_app2);
	}
}

// this one just warps the mob to the current location
void Mob::SendPosition(bool everyone, bool ackreq)
{
	auto app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
	SpawnPositionUpdates_Struct* spu = (SpawnPositionUpdates_Struct*)app->pBuffer;
	spu->num_updates = 1; // hack - only one spawn position per update
	MakeSpawnUpdateNoDelta(&spu->spawn_update);
	tar_ndx = 20;
	entity_list.QueueClientsPosUpdate(this, app, true, ackreq);

	//entity_list.QueueCloseClients(this, app, true, 1000, nullptr, false);
	safe_delete(app);
}

// this one just warps the mob to the current location
void Mob::SendRealPosition()
{
	if(IsCorpse())
	{ 
		auto app = new EQApplicationPacket(OP_CorpsePosition, sizeof(CorpsePosition_Struct));
		app->priority = 6;
		CorpsePosition_Struct *p = (CorpsePosition_Struct *)app->pBuffer;
		p->entityid = GetID();
		p->x = GetX();
		p->y = GetY();
		p->z = GetZ();
		entity_list.QueueClients(this, app);
		safe_delete(app);
	}
	else
	{
		auto app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
		SpawnPositionUpdates_Struct *spu = (SpawnPositionUpdates_Struct *)app->pBuffer;
		spu->num_updates = 1; // hack - only one spawn position per update
		MakeSpawnUpdateNoDelta(&spu->spawn_update);
		entity_list.QueueClientsPosUpdate(this, app, true);
		safe_delete(app);
	}
}

// this one is for mobs on the move, and clients.
void Mob::SendPosUpdate(uint8 iSendToSelf)
{
	if (GetRace() == CONTROLLED_BOAT)
	{
		//OP_ClientUpdate handles updates for these boats for us.
		return;
	}

	if (iSendToSelf == 2) {
		if (this->IsClient()) {
			auto app = new EQApplicationPacket(OP_ClientUpdate, sizeof(SpawnPositionUpdate_Struct));
			SpawnPositionUpdate_Struct* spu = (SpawnPositionUpdate_Struct*)app->pBuffer;
			MakeSpawnUpdate(spu);
			this->CastToClient()->FastQueuePacket(&app, false);
			return;
		}
	}
	else
	{
		auto app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
		SpawnPositionUpdates_Struct* spu = (SpawnPositionUpdates_Struct*)app->pBuffer;
		spu->num_updates = 1; // hack - only one spawn position per update
		MakeSpawnUpdate(&spu->spawn_update);

		if (IsClient())
		{
			if (CastToClient()->gmhideme)
				entity_list.QueueClientsStatus(this, app, (iSendToSelf == 0), CastToClient()->Admin(), 255);
			else
				entity_list.QueueClientsPosUpdate(this, app, (iSendToSelf == 0), false);
		}
		else
		{
			entity_list.QueueClientsPosUpdate(this, app, (iSendToSelf == 0), false);
		}
		safe_delete(app);
	}
}

// this is for SendPosition() It shouldn't be used for player updates, only NPCs that haven't moved.
void Mob::MakeSpawnUpdateNoDelta(SpawnPositionUpdate_Struct *spu)
{
	memset(spu,0xff,sizeof(SpawnPositionUpdate_Struct));

	spu->spawn_id	= GetID();
	spu->x_pos = static_cast<int16>(m_Position.x);
	spu->y_pos = static_cast<int16>(m_Position.y);
	spu->z_pos = static_cast<int16>(m_Position.z * 10.0f);
	spu->heading	= static_cast<int8>(m_Position.w);

	spu->delta_yzx.value = 0;
	spu->delta_heading = 0;

	spu->anim_type	= 0;

	/*
	if(IsNPC()) 
	{
		std::vector<std::string> params;
		params.push_back(std::to_string((long)GetID()));
		params.push_back(GetCleanName());
		params.push_back(std::to_string((double)m_Position.x));
		params.push_back(std::to_string((double)m_Position.y));
		params.push_back(std::to_string((double)m_Position.z));
		params.push_back(std::to_string((double)m_Position.w));
		params.push_back(std::to_string((double)GetClass()));
		params.push_back(std::to_string((double)GetRace()));
	}
	*/
}

// this is for SendPosUpdate()
void Mob::MakeSpawnUpdate(SpawnPositionUpdate_Struct* spu) 
{

	spu->spawn_id	= GetID();
	spu->x_pos = static_cast<int16>(m_Position.x);
	spu->y_pos = static_cast<int16>(m_Position.y);
	spu->z_pos = static_cast<int16>(m_Position.z * 10.0f);
	spu->heading	= static_cast<int8>(m_Position.w);

	spu->delta_yzx.SetValue(m_Delta.x, m_Delta.y, IsNPC() ? 0.0f : m_Delta.z);
	spu->delta_heading = static_cast<int8>(m_Delta.w);

	if(this->IsClient() || this->iszomm)
	{
		spu->anim_type = animation;
	}
	else if(this->IsNPC())
	{
		spu->anim_type = pRunAnimSpeed;
	}
}

void Mob::SetSpawnUpdate(SpawnPositionUpdate_Struct* incoming, SpawnPositionUpdate_Struct* outgoing) 
{
	outgoing->spawn_id	= incoming->spawn_id;
	outgoing->x_pos = incoming->x_pos;
	outgoing->y_pos = incoming->y_pos;
	outgoing->z_pos = incoming->z_pos;
	outgoing->heading	= incoming->heading;
	outgoing->delta_yzx.value = incoming->delta_yzx.value;
	outgoing->delta_heading = incoming->delta_heading;
	outgoing->anim_type = incoming->anim_type;
}

void Mob::ShowStats(Client* client)
{
	if (IsClient()) {
		CastToClient()->SendStats(client);
	}
	else if (IsCorpse()) {
		if (IsPlayerCorpse()) {
			client->Message(Chat::White, "  CharID: %i  PlayerCorpse: %i Empty: %i Rezed: %i Exp: %i GMExp: %i KilledBy: %i Rez Time: %d Owner Online: %i", CastToCorpse()->GetCharID(), CastToCorpse()->GetCorpseDBID(), CastToCorpse()->IsEmpty(), CastToCorpse()->IsRezzed(), CastToCorpse()->GetRezExp(), CastToCorpse()->GetGMRezExp(), CastToCorpse()->GetKilledBy(), CastToCorpse()->GetRemainingRezTime(), CastToCorpse()->GetOwnerOnline());
			client->Message(Chat::White, "  Race: %i Texture: %i Gender: %i Size: %0.2f Timestamp: %d", CastToCorpse()->GetRace(), CastToCorpse()->GetTexture(), CastToCorpse()->GetGender(), CastToCorpse()->GetSize(), CastToCorpse()->GetToD());
		}
		else {
			client->Message(Chat::White, "  NPCCorpse %i X: %0.2f Y: %0.2f Z: %0.2f", GetID(), GetX(), GetY(), GetZ());
		}
	}
	else {
		client->Message(Chat::White, "  Level: %i  AC: %i  Class: %i  Size: %1.1f  ModelSize: %1.1f  BaseSize: %1.1f Z-Offset %1.1f Haste: %i (cap %i)", GetLevel(), GetAC(), GetClass(), GetSize(), GetModelSize(), GetBaseSize(), GetZOffset(), GetHaste(), GetHasteCap());
		client->Message(Chat::White, "  HP: %i  Max HP: %i Per: %0.2f",GetHP(), GetMaxHP(), GetHPRatio());
		client->Message(Chat::White, "  Mana: %i  Max Mana: %i", GetMana(), GetMaxMana());
		client->Message(Chat::White, "  X: %0.2f Y: %0.2f Z: %0.2f", GetX(), GetY(), GetZ());
		client->Message(Chat::White, "  Offense: %i  To-Hit: %i  Mitigation: %i  Avoidance: %i", GetOffenseByHand(), GetToHitByHand(), GetMitigation(), GetAvoidance());
		client->Message(Chat::White, "  Displayed ATK: %i  Worn +ATK: %i (Cap %i)  Spell +ATK: %i", GetATK(), GetATKBonusItem(), RuleI(Character, ItemATKCap), GetATKBonusSpell());
		client->Message(Chat::White, "  STR: %i  STA: %i  DEX: %i  AGI: %i  INT: %i  WIS: %i  CHA: %i", GetSTR(), GetSTA(), GetDEX(), GetAGI(), GetINT(), GetWIS(), GetCHA());
		client->Message(Chat::White, "  MR: %i  PR: %i  FR: %i  CR: %i  DR: %i", GetMR(), GetPR(), GetFR(), GetCR(), GetDR());
		client->Message(Chat::White, "  Race: %i  BaseRace: %i Gender: %i  BaseGender: %i BodyType: %i", GetRace(), GetBaseRace(), GetGender(), GetBaseGender(), GetBodyType());
		client->Message(Chat::White, "  Face: % i Beard: %i  BeardColor: %i  Hair: %i  HairColor: %i Light: %i ActiveLight: %i ", GetLuclinFace(), GetBeard(), GetBeardColor(), GetHairStyle(), GetHairColor(), GetInnateLightType(), GetActiveLightType());
		client->Message(Chat::White, "  Texture: %i  HelmTexture: %i  Chest: %i Arms: %i Bracer: %i Hands: %i Legs: %i Feet: %i ", GetTexture(), GetHelmTexture(), GetChestTexture(), GetArmTexture(), GetBracerTexture(), GetHandTexture(), GetLegTexture(), GetFeetTexture());
		client->Message(Chat::White, "  IsPet: %i PetID: %i  OwnerID: %i SummonerID: %i IsFamiliar: %i IsCharmed: %i IsDireCharm: %i IsZomm: %i TypeofPet: %d", IsPet(), GetPetID(), GetOwnerID(), GetSummonerID(), IsFamiliar(), IsCharmedPet(), IsDireCharmed(), iszomm, typeofpet);
		if (client->Admin() >= AccountStatus::GMAdmin)
			client->Message(Chat::White, "  EntityID: %i AIControlled: %i Targetted: %i", GetID(), IsAIControlled(), targeted);

		if (IsNPC()) {
			NPC *n = CastToNPC();
			uint32 spawngroupid = 0;
			if(n->respawn2 != 0)
				spawngroupid = n->respawn2->SpawnGroupID();
			client->Message(Chat::White, "  NPCID: %u  SpawnGroupID: %u Grid: %i FactionID: %i PreCharmFactionID: %i PrimaryFaction: %i", GetNPCTypeID(),spawngroupid, n->GetGrid(), n->GetNPCFactionID(), n->GetPreCharmNPCFactionID(), GetPrimaryFaction());
			client->Message(Chat::White, "  HP Regen: %i Mana Regen: %i Magic Atk: %i Immune to Melee: %i", n->GetHPRegen(), n->GetManaRegen(), GetLevel() >= MAGIC_ATTACK_LEVEL ? 1 : GetSpecialAbility(SpecialAbility::MagicalAttack), GetSpecialAbility(SpecialAbility::MeleeImmunityExceptMagical));
			client->Message(Chat::White, "  Accuracy: %i BonusAvoidance: %i LootTable: %u SpellsID: %u", n->GetAccuracyRating(), bonusAvoidance, n->GetLoottableID(), n->GetNPCSpellsID());
			n->DisplayAttackTimer(client);
			client->Message(Chat::White, "  EmoteID: %i SeeInvis/Hide: %i SeeInvUndead: %i SeeSneak: %i SeeImpHide: %i", n->GetEmoteID(), n->SeeInvisible(), n->SeeInvisibleUndead(), n->SeeSneak(), n->SeeImprovedHide());
			client->Message(Chat::White, "  CanDualWield: %i  IsDualWielding: %i  HasShield: %i", n->CanDualWield(), n->IsDualWielding(), n->HasShieldEquiped());
			client->Message(Chat::White, "  PriSkill: %i SecSkill: %i PriMelee: %i SecMelee: %i Double Attack: %i%% Dual Wield: %i%%", n->GetPrimSkill(), n->GetSecSkill(), n->GetPrimaryMeleeTexture(), n->GetSecondaryMeleeTexture(), n->GetDoubleAttackChance(), n->GetDualWieldChance());
			client->Message(Chat::White, "  Runspeed: %f Walkspeed: %f RunSpeedAnim: %i CurrentSpeed: %f Rooted: %d PermaRooted: %d", GetRunspeed(), GetWalkspeed(), GetRunAnimSpeed(), GetCurrentSpeed(), rooted, permarooted);
			client->Message(Chat::White, "  AssistAggroMode: %d  IgnoreDistance: %0.2f  IgnoreDespawn: %d",  n->IsAssisting(), n->GetIgnoreDistance(), n->IgnoreDespawn());
			client->Message(Chat::White, "  Fleespeed: %f CurFP: %i IsFleeing: %i IsBlind: %i IsFeared: %i", n->GetFearSpeed(), curfp, IsFleeing(), IsBlind(), IsFearedNoFlee());
			client->Message(Chat::White, "  MerchantID: %i Shop Count: %d Greed: %d%% ", n->MerchantType, n->shop_count, n->GetGreedPercent());
			client->Message(Chat::White, "  Primary Weapon: %i Secondary Weapon: %d", n->GetEquipment(EQ::textures::weaponPrimary), n->GetEquipment(EQ::textures::weaponSecondary));
			n->QueryLoot(client);
		}
		if (IsAIControlled()) {
			client->Message(Chat::White, "  AggroRange: %1.0f  AssistRange: %1.0f", GetAggroRange(), GetAssistRange());
		}
	}
}

void Mob::DoAnim(DoAnimation animnum, int type, bool ackreq, eqFilterType filter) {

	auto outapp = new EQApplicationPacket(OP_Animation, sizeof(Animation_Struct));
	Animation_Struct* anim = (Animation_Struct*)outapp->pBuffer;
	anim->spawnid = GetID();
	if(GetTarget())
		anim->target = GetTarget()->GetID();
	anim->action = animnum;
	anim->value=type;
	anim->unknown10=16256;
	
	entity_list.QueueCloseClients(this, outapp, false, RuleI(Range, Anims), 0, ackreq, filter);

	safe_delete(outapp);
}

void Mob::ShowBuffs(Client* client) {
	if(SPDAT_RECORDS <= 0)
		return;
	client->Message(Chat::White, "Buffs on: %s", this->GetName());
	uint32 i;
	uint32 buff_count = GetMaxTotalSlots();
	bool beneficial = true;

	for (i=0; i < buff_count; i++)
	{
		beneficial = IsBeneficialSpell(buffs[i].spellid);

		if (buffs[i].spellid != SPELL_UNKNOWN) {
			if (spells[buffs[i].spellid].buffdurationformula == DF_Permanent)
				client->Message(beneficial ? Chat::Lime : Chat::Red, "  %i: %s: Permanent", i, spells[buffs[i].spellid].name);
			else if(zone->BuffTimersSuspended() && beneficial)
				client->Message(Chat::Lime, "  %i: %s: Timer Suspended", i, spells[buffs[i].spellid].name);
			else
				client->Message(beneficial ? Chat::Lime : Chat::Red, "  %i: %s: %i tics left", i, spells[buffs[i].spellid].name, buffs[i].ticsremaining);
			client->Message(Chat::White, "  %i:   spellid: %i instrumentmod: %i casterlevel: %i casterid: %i caster_name: %s",
				i, buffs[i].spellid, buffs[i].instrumentmod, buffs[i].casterlevel, buffs[i].casterid, buffs[i].caster_name != 0 ? buffs[i].caster_name : "[null]");
			client->Message(Chat::White, "  %i:   counters: %i melee_rune: %i magic_rune: %i ExtraDIChance: %i RootBreakChance: %i",
				i, buffs[i].counters, buffs[i].melee_rune, buffs[i].magic_rune, buffs[i].ExtraDIChance, buffs[i].RootBreakChance);

		}
	}
	if (IsClient()){
		client->Message(Chat::White, "itembonuses:");
		client->Message(Chat::White, "Atk:%i Ac:%i HP(%i):%i Mana:%i", itembonuses.ATK, itembonuses.AC, itembonuses.HPRegen, itembonuses.HP, itembonuses.Mana);
		client->Message(Chat::White, "Str:%i Sta:%i Dex:%i Agi:%i Int:%i Wis:%i Cha:%i",
			itembonuses.STR,itembonuses.STA,itembonuses.DEX,itembonuses.AGI,itembonuses.INT,itembonuses.WIS,itembonuses.CHA);
		client->Message(Chat::White, "SvMagic:%i SvFire:%i SvCold:%i SvPoison:%i SvDisease:%i",
				itembonuses.MR,itembonuses.FR,itembonuses.CR,itembonuses.PR,itembonuses.DR);
		client->Message(Chat::White, "DmgShield:%i Haste:%i", itembonuses.DamageShield, itembonuses.haste );
		client->Message(Chat::White, "spellbonuses:");
		client->Message(Chat::White, "Atk:%i Ac:%i HP(%i):%i Mana:%i", spellbonuses.ATK, spellbonuses.AC, spellbonuses.HPRegen, spellbonuses.HP, spellbonuses.Mana);
		client->Message(Chat::White, "Str:%i Sta:%i Dex:%i Agi:%i Int:%i Wis:%i Cha:%i",
			spellbonuses.STR,spellbonuses.STA,spellbonuses.DEX,spellbonuses.AGI,spellbonuses.INT,spellbonuses.WIS,spellbonuses.CHA);
		client->Message(Chat::White, "SvMagic:%i SvFire:%i SvCold:%i SvPoison:%i SvDisease:%i",
				spellbonuses.MR,spellbonuses.FR,spellbonuses.CR,spellbonuses.PR,spellbonuses.DR);
		client->Message(Chat::White, "DmgShield: %i Haste:%i", spellbonuses.DamageShield, spellbonuses.haste );
		for (int i = 0; i < MAX_PROCS; i++) {
			if (SpellProcs[i].spellID != SPELL_UNKNOWN) {
				client->Message(Chat::White, "Spell Buff Proc [%i]: %s", i, spells[SpellProcs[i].spellID].name);
			}
		}
	}
}

void Mob::ShowBuffList(Client* client) {
	if(SPDAT_RECORDS <= 0)
		return;

	uint32 i;
	uint32 buff_count = GetMaxTotalSlots();
	for (i = 0; i < buff_count; i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			client->Message(Chat::White, "%s", spells[buffs[i].spellid].name);
		}
	}
}

void Mob::GMMove(float x, float y, float z, float heading, bool SendUpdate, bool GuardReset) {

	if (IsNPC()) {
		entity_list.ProcessMove(CastToNPC(), x, y, z);
		glm::vec4 new_loc(x, y, z, m_Position.w != 0.01 ? heading : m_Position.w);
		Teleport(new_loc);
	}
	m_Position.x = x;
	m_Position.y = y;
	m_Position.z = z;
	if (m_Position.w != 0.01f)
		this->m_Position.w = heading;
	if (SendUpdate)
		SendRealPosition();

	if (IsNPC() && GuardReset) {
		CastToNPC()->SaveGuardSpot();
	}
}

void Mob::SetCurrentSpeed(float speed) {
	if (current_speed != speed)
	{ 
		current_speed = speed; 
	} 
}

void Mob::SendIllusionPacket(uint16 in_race, uint8 in_gender, uint8 in_texture, uint8 in_helmtexture, uint8 in_haircolor, uint8 in_beardcolor, uint8 in_eyecolor1, uint8 in_eyecolor2, uint8 in_hairstyle, uint8 in_luclinface, uint8 in_beard, uint8 in_aa_title, float in_size, Client* sendto) {

	uint16 BaseRace = GetBaseRace();

	if (in_race == 0) {
		this->race = BaseRace;
		if (in_gender == 0xFF)
			this->gender = GetBaseGender();
		else
			this->gender = in_gender;
	}
	else {
		this->race = in_race;
		if (in_gender == 0xFF) {
			uint8 tmp = Mob::GetDefaultGender(this->race, gender);
			if (tmp == 2)
				gender = 2;
			else if (gender == 2 && GetBaseGender() == 2)
				gender = tmp;
			else if (gender == 2)
				gender = GetBaseGender();
		}
		else
			gender = in_gender;
	}
	if (in_texture == 0xFF) {
		if (IsPlayableRace(in_race))
			this->texture = 0xFF;
		else
			this->texture = GetTexture();
	}
	else
		this->texture = in_texture;

	if (in_helmtexture == 0xFF) {
		if (GetBaseRace() == Race::Quarm && this->race == Race::Quarm)
		{
			// this restores quarm's head appearance after he's illusioned into a skeleton and it wears off
			this->texture = 254;
			this->gender = 2;
			this->helmtexture = this->helmtexture_quarm;
		}
		else if (IsPlayableRace(in_race))
			this->helmtexture = 0xFF;
		else if (in_texture != 0xFF)
			this->helmtexture = in_texture;
		else
			this->helmtexture = GetHelmTexture();
	}
	else
	{
		// quarm head explosion calls this function to set the head appearance
		// this saves quarm's head appearance in case he gets illusioned into something else
		if (GetBaseRace() == Race::Quarm && this->race == Race::Quarm)
			this->helmtexture_quarm = in_helmtexture;
		this->helmtexture = in_helmtexture;
	}

	if (in_haircolor == 0xFF)
		this->haircolor = GetHairColor();
	else
		this->haircolor = in_haircolor;

	if (in_beardcolor == 0xFF)
		this->beardcolor = GetBeardColor();
	else
		this->beardcolor = in_beardcolor;

	if (in_eyecolor1 == 0xFF)
		this->eyecolor1 = GetEyeColor1();
	else
		this->eyecolor1 = in_eyecolor1;

	if (in_eyecolor2 == 0xFF)
		this->eyecolor2 = GetEyeColor2();
	else
		this->eyecolor2 = in_eyecolor2;

	if (in_hairstyle == 0xFF)
		this->hairstyle = GetHairStyle();
	else
		this->hairstyle = in_hairstyle;

	if (in_luclinface == 0xFF)
		this->luclinface = GetLuclinFace();
	else
		this->luclinface = in_luclinface;

	if (in_beard == 0xFF)
		this->beard	= GetBeard();
	else
		this->beard = in_beard;

	this->aa_title = 0xFF;

	if (in_size <= 0.0f)
		// set default size for race, race already set above if in_race was 0
		this->size = GetPlayerHeight(this->race);
	else
		this->size = in_size;

	// Forces the feature information to be pulled from the Player Profile
	if (this->IsClient() && in_race == 0) {
		this->race = CastToClient()->GetBaseRace();
		this->gender = CastToClient()->GetBaseGender();
		this->texture = 0xFF;
		this->helmtexture = 0xFF;
		this->haircolor = CastToClient()->GetBaseHairColor();
		this->beardcolor = CastToClient()->GetBaseBeardColor();
		this->hairstyle = CastToClient()->GetBaseHairStyle();
		this->luclinface = CastToClient()->GetBaseFace();
		this->beard	= CastToClient()->GetBaseBeard();
		this->size = GetBaseSize();
	}

	auto outapp = new EQApplicationPacket(OP_Illusion, sizeof(Illusion_Struct));
	Illusion_Struct* is = (Illusion_Struct*) outapp->pBuffer;
	is->spawnid = this->GetID();
	is->race = this->race;
	is->gender = this->gender;
	is->texture = this->texture;
	is->helmtexture = this->helmtexture;
	is->haircolor = this->haircolor;
	is->beardcolor = this->beardcolor;
	is->beard = this->beard;
	is->hairstyle = this->hairstyle;
	is->face = this->luclinface;
	// this illusion packet can't carry a fractional part, only integers, and some player races have fractional sizes
	// we want to avoid sending a size if it's the default
	if (in_size <= 0.0f || in_size == GetPlayerHeight(this->race))
		is->size = -1; // client will use default size
	else
		is->size = (int32)(this->size + 0.5f); // round to nearest integer
	is->unknown007 = 0xFF;

	if (sendto)
	{
		sendto->QueuePacket(outapp);
	}
	else
	{
		entity_list.QueueClients(this, outapp);
	}
	safe_delete(outapp);
	Log(Logs::Detail, Logs::Spells, "Illusion: Race = %i, Gender = %i, Texture = %i, HelmTexture = %i, HairColor = %i, BeardColor = %i, EyeColor1 = %i, EyeColor2 = %i, HairStyle = %i, Face = %i, Size = %f",
		this->race, this->gender, this->texture, this->helmtexture, this->haircolor, this->beardcolor, this->eyecolor1, this->eyecolor2, this->hairstyle, this->luclinface, this->size);
}

uint8 Mob::GetDefaultGender(uint16 in_race, uint8 in_gender) {
	if (IsPlayableRace(in_race) ||
		in_race == BROWNIE || in_race == LION || in_race == DRACNID || in_race == ZOMBIE || in_race == ELF_VAMPIRE || in_race == ERUDITE_GHOST) {
		if (in_gender >= Gender::Neuter) {
			// Female default for PC Races
			return Gender::Female;
		}
		else
			return in_gender;
	}
	else if (in_race == FREEPORT_GUARD || in_race == MIMIC || in_race == HUMAN_BEGGER || in_race == VAMPIRE || in_race == HIGHPASS_CITIZEN ||
		in_race == CLOCKWORK_GNOME || in_race == DWARF_GHOST || in_race == INVISIBLE_MAN || in_race == NERIAK_CITIZEN || in_race == ERUDITE_CITIZEN ||
		in_race == RIVERVALE_CITIZEN || in_race == HALAS_CITIZEN || in_race == GROBB_CITIZEN || in_race == OGGOK_CITIZEN || in_race == KALADIM_CITIZEN || 
		in_race == FELGUARD || in_race == FAYGUARD) {
		// Male only races
		return Gender::Male;

	}
	else if (in_race == FAIRY || in_race == PIXIE) {
		// Female only races
		return Gender::Female;
	}
	else {
		// Neutral default for NPC Races
		return Gender::Neuter;
	}
}

bool Mob::IsPlayerClass(uint16 in_class) {
	if (
		in_class >= Class::Warrior &&
		in_class <= Class::Beastlord
		) {
		return true;
	}

	return false;
}

bool Mob::IsPlayerRace(uint16 in_race) {

	if (
		(in_race >= HUMAN && in_race <= GNOME) ||
		in_race == IKSAR ||
		in_race == VAHSHIR
		) {
		return true;
	}

	return false;
}


void Mob::SendAppearancePacket(uint32 type, uint32 value, bool WholeZone, bool iIgnoreSelf, Client *specific_target) {
	if (!GetID())
		return;

	auto outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* appearance = (SpawnAppearance_Struct*)outapp->pBuffer;
	appearance->spawn_id = this->GetID();
	appearance->type = type;
	appearance->parameter = value;
	if (WholeZone)
		entity_list.QueueClients(this, outapp, iIgnoreSelf);
	else if(specific_target != nullptr)
		specific_target->QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
	else if (this->IsClient())
		this->CastToClient()->QueuePacket(outapp, true, Client::CLIENT_CONNECTED);
	safe_delete(outapp);
}

const int32& Mob::SetMana(int32 amount)
{
	CalcMaxMana();
	int32 mmana = GetMaxMana();
	cur_mana = amount < 0 ? 0 : (amount > mmana ? mmana : amount);
/*
	if(IsClient())
		Log(Logs::Detail, Logs::Debug, "Setting mana for %s to %d (%4.1f%%)", GetName(), amount, GetManaRatio());
*/

	return cur_mana;
}


void Mob::SetAppearance(EmuAppearance app, bool iIgnoreSelf) 
{
	if (_appearance != app) 
	{
		_appearance = app;
		SendAppearancePacket(AppearanceType::Animation, GetAppearanceValue(app), true, iIgnoreSelf);
		if (this->IsClient() && this->IsAIControlled())
		{
			if(!CastToClient()->has_zomm)
				SendAppearancePacket(AppearanceType::Animation, Animation::Freeze, false, false);
		}
	}
}

bool Mob::UpdateActiveLight()
{
	uint8 old_light_level = m_Light.Level[EQ::lightsource::LightActive];

	m_Light.Type[EQ::lightsource::LightActive] = 0;
	m_Light.Level[EQ::lightsource::LightActive] = 0;

	if (EQ::lightsource::IsLevelGreater((m_Light.Type[EQ::lightsource::LightInnate] & 0x0F), m_Light.Type[EQ::lightsource::LightActive])) { m_Light.Type[EQ::lightsource::LightActive] = m_Light.Type[EQ::lightsource::LightInnate]; }
	if (m_Light.Level[EQ::lightsource::LightEquipment] > m_Light.Level[EQ::lightsource::LightActive]) { m_Light.Type[EQ::lightsource::LightActive] = m_Light.Type[EQ::lightsource::LightEquipment]; } // limiter in property handler
	if (m_Light.Level[EQ::lightsource::LightSpell] > m_Light.Level[EQ::lightsource::LightActive]) { m_Light.Type[EQ::lightsource::LightActive] = m_Light.Type[EQ::lightsource::LightSpell]; } // limiter in property handler

	m_Light.Level[EQ::lightsource::LightActive] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightActive]);

	return (m_Light.Level[EQ::lightsource::LightActive] != old_light_level);
}

void Mob::ChangeSize(float in_size = 0, bool bNoRestriction) {
	// Size Code
	if (!bNoRestriction)
	{
		if (this->IsClient() || this->petid != 0)
		{
			if (in_size < 1.0)
				in_size = 1.0;

			if (in_size > 15.0)
				in_size = 15.0;
		}
	}

	if (in_size < 1.0)
		in_size = 1.0;

	/*if (in_size > 255.0)
		in_size = 255.0;*/
	//End of Size Code
	float newsize = floorf(in_size + 0.5);
	this->size = newsize;
	this->z_offset = CalcZOffset();
	this->head_offset = CalcHeadOffset();
	this->model_size = CalcModelSize();
	this->model_bounding_radius = CalcBoundingRadius();
	if (IsNPC() && IsPet() && pStandingPetOrder == SPO_Guard) {
		glm::vec4 guard_spot (CastToNPC()->GetGuardPoint());
		glm::vec3 new_loc(guard_spot.x, guard_spot.y, guard_spot.z);
		float newz = zone->zonemap->FindBestZ(new_loc, nullptr);
		if (newz != BEST_Z_INVALID) {
			guard_spot.z = newz + z_offset;
			CastToNPC()->SetGuardSpot(guard_spot.x,guard_spot.y,guard_spot.z,guard_spot.w);
		}
	}
	SendAppearancePacket(AppearanceType::Size, (uint32)newsize);
}

Mob* Mob::GetOwnerOrSelf() {
	if (!GetOwnerID())
		return this;
	Mob* owner = entity_list.GetMob(this->GetOwnerID());
	if (!owner) {
		SetOwnerID(0);
		return(this);
	}
	if (owner->GetPetID() == this->GetID()) {
		return owner;
	}
	if(IsNPC() && CastToNPC()->GetSwarmInfo()){
		Mob *mob = CastToNPC()->GetSwarmInfo()->GetOwner();
		return (mob ? mob : this);
	}
	if (this->IsClient() && this->IsCharmed() && owner->IsNPC()) {
		if (owner->GetPetID() == 0)
			owner->SetPetID(this->GetID());
		return owner;
	}
	SetOwnerID(0);
	return this;
}

Mob* Mob::GetOwner() {
	Mob* owner = entity_list.GetMob(this->GetOwnerID());
	if (owner)
	{
		return owner;
	}
	if(IsNPC() && CastToNPC()->GetSwarmInfo()){
		return (CastToNPC()->GetSwarmInfo()->GetOwner());
	}
	SetOwnerID(0);
	return 0;
}

Mob* Mob::GetUltimateOwner()
{
	Mob* Owner = GetOwner();

	if(!Owner)
		return this;

	while(Owner && Owner->HasOwner())
		Owner = Owner->GetOwner();

	return Owner ? Owner : this;
}

void Mob::SetOwnerID(uint16 NewOwnerID) {
	if (NewOwnerID == GetID() && NewOwnerID != 0) // ok, no charming yourself now =p
		return;
	ownerid = NewOwnerID;
	if (ownerid == 0 && this->IsNPC() && this->GetPetType() != petCharmed && this->GetPetType() != petOrphan)
		this->Depop();
}

// used in checking for behind (backstab) and checking in front (melee LoS)
float Mob::MobAngle(Mob *other, float ourx, float oury) const {
	if (!other || other == this || (other->GetX() == ourx && other->GetY() == GetY())) {
		return 0.0f;
	}

	float angle, lengthb, vectorx, vectory, dotp;
	float mobx = -(other->GetX());	// mob xloc (inverse because eq)
	float moby = other->GetY();		// mob yloc
	float heading = other->GetHeadingRadians();	// mob heading

	vectorx = mobx + (10.0f * cosf(heading));	// create a vector based on heading
	vectory = moby + (10.0f * sinf(heading));	// of mob length 10

	// length of mob to player vector
	lengthb = (float) sqrtf(((-ourx - mobx) * (-ourx - mobx)) + ((oury - moby) * (oury - moby)));

	// calculate dot product to get angle
	// Handle acos domain errors due to floating point rounding errors
	dotp = ((vectorx - mobx) * (-ourx - mobx) +
			(vectory - moby) * (oury - moby)) / (10 * lengthb);
	// I haven't seen any errors that  cause problems that weren't slightly
	// larger/smaller than 1/-1, so only handle these cases for now
	if (dotp > 1)
		return 0.0f;
	else if (dotp < -1)
		return 180.0f;

	angle = acosf(dotp);
	angle = angle * 180.0f / 3.141592654f;

	return angle;
}

void Mob::SetZone(uint32 zone_id, uint32 zone_guild_id)
{
	if(IsClient())
	{
		CastToClient()->GetPP().zone_id = zone_id;
		CastToClient()->GetEPP().zone_guild_id = zone_guild_id;
		CastToClient()->Save();
	}
	Save();
}

void Mob::Kill() {
	Death(this, 0, SPELL_UNKNOWN, EQ::skills::SkillHandtoHand);
}

bool Mob::CanDualWield()
{
	if (GetSkill(EQ::skills::SkillDualWield) || (IsClient() && GetClass() == Class::Monk))
		return true;

	return false;
}

bool Mob::IsDualWielding()
{
	if (IsClient())
	{
		if (GetSkill(EQ::skills::SkillDualWield) > 0 || GetClass() == Class::Monk)
		{
			const EQ::ItemInstance* pinst = CastToClient()->GetInv().GetItem(EQ::invslot::slotPrimary);
			const EQ::ItemInstance* sinst = CastToClient()->GetInv().GetItem(EQ::invslot::slotSecondary);

			if (pinst && pinst->IsWeapon())
			{
				const EQ::ItemData* item = pinst->GetItem();

				if ((item->ItemType == EQ::item::ItemType2HBlunt) || (item->ItemType == EQ::item::ItemType2HSlash) || (item->ItemType == EQ::item::ItemType2HPiercing))
					return false;
			}

			// OffHand Weapon
			if (sinst && !sinst->IsWeapon())
				return false;

			// Dual-Wielding Empty Fists
			if (!pinst && !sinst)
				if (class_ != Class::Monk && class_ != Class::Beastlord)
					return false;

			return true;
		}
	}
	else if (IsNPC())
	{
		if (!CastToNPC()->GetEquipment(EQ::textures::weaponSecondary) && !GetSpecialAbility(SpecialAbility::DualWield))
			return false;

		if (GetSpecialAbility(SpecialAbility::DualWield) && !IsSummonedClientPet())
			return true;

		const EQ::ItemData* mh = database.GetItem(CastToNPC()->GetEquipment(EQ::textures::weaponPrimary));
		const EQ::ItemData* oh = database.GetItem(CastToNPC()->GetEquipment(EQ::textures::weaponSecondary));

		if (mh && (mh->ItemType == EQ::item::ItemType2HBlunt || mh->ItemType == EQ::item::ItemType2HSlash || mh->ItemType == EQ::item::ItemType2HPiercing))
			return false;

		if (oh && GetSkill(EQ::skills::SkillDualWield) > 0)
		{
			if (oh->ItemType == EQ::item::ItemType1HBlunt || oh->ItemType == EQ::item::ItemType1HSlash || oh->ItemType == EQ::item::ItemType1HPiercing || oh->ItemType == EQ::item::ItemTypeMartial)
				return true;
		}
		else if (GetSpecialAbility(SpecialAbility::DualWield))
			return true;
	}
	return false;
}

bool Mob::IsWarriorClass(void) const
{
	switch(GetClass())
	{
	case Class::Warrior:
	case Class::WarriorGM:
	case Class::Rogue:
	case Class::RogueGM:
	case Class::Monk:
	case Class::MonkGM:
	case Class::Paladin:
	case Class::PaladinGM:
	case Class::ShadowKnight:
	case Class::ShadowKnightGM:
	case Class::Ranger:
	case Class::RangerGM:
	case Class::Beastlord:
	case Class::BeastlordGM:
	case Class::Bard:
	case Class::BardGM:
		{
			return true;
		}
	default:
		{
			return false;
		}
	}

}

/*
float Mob::GetReciprocalHeading(Mob* target) {
	float Result = 0;

	if(target) {
		// Convert to radians
		float h = (target->GetHeading() / 256.0f) * 6.283184f;

		// Calculate the reciprocal heading in radians
		Result = h + 3.141592f;

		// Convert back to eq heading from radians
		Result = (Result / 6.283184f) * 256.0f;
	}

	return Result;
}
*/
bool Mob::PlotPositionAroundTarget(Mob* target, float &x_dest, float &y_dest, float &z_dest, bool lookForAftArc) {
	bool Result = false;

	if(target) {
		float look_heading = 0;

		if(lookForAftArc)
			look_heading = GetReciprocalHeading(target->GetPosition());
		else
			look_heading = target->GetHeading();

		// Convert to sony heading to radians
		look_heading = (look_heading / 256.0f) * 6.283185308f;

		float tempX = 0;
		float tempY = 0;
		float tempZ = 0;
		float tempSize = 0;
		const float rangeCreepMod = 0.25;
		const uint8 maxIterationsAllowed = 4;
		uint8 counter = 0;
		float rangeReduction= 0;

		tempSize = target->GetSize();
		rangeReduction = (tempSize * rangeCreepMod);

		while(tempSize > 0 && counter != maxIterationsAllowed) {
			tempX = GetX() + (tempSize * static_cast<float>(sin(double(look_heading))));
			tempY = GetY() + (tempSize * static_cast<float>(cos(double(look_heading))));
			tempZ = target->GetZ();

			if(!CheckLosFN(tempX, tempY, tempZ, tempSize)) {
				tempSize -= rangeReduction;
			}
			else {
				Result = true;
				break;
			}

			counter++;
		}

		if(!Result) {
			// Try to find an attack arc to position at from the opposite direction.
			look_heading += (3.141592654f / 2.0f);

			tempSize = target->GetSize();
			counter = 0;

			while(tempSize > 0 && counter != maxIterationsAllowed) {
				tempX = GetX() + (tempSize * static_cast<float>(sin(double(look_heading))));
				tempY = GetY() + (tempSize * static_cast<float>(cos(double(look_heading))));
				tempZ = target->GetZ();

				if(!CheckLosFN(tempX, tempY, tempZ, tempSize)) {
					tempSize -= rangeReduction;
				}
				else {
					Result = true;
					break;
				}

				counter++;
			}
		}

		if(Result) {
			x_dest = tempX;
			y_dest = tempY;
			z_dest = tempZ;
		}
	}

	return Result;
}

bool Mob::CheckBardHateSummon(Mob* summoned) {

	// check if mob has ability to summon
	// 97% is the offical % that summoning starts on live, not 94
	if (!summoned) {
		return false;
	}

	if (zone->GetGuildID() != GUILD_NONE)
		return false;

	if (summoned->GetClass() != Class::Bard || IsCharmedPet() || !summoned->IsClient() || summoned->PermaRooted() || (summoned->IsNPC() && summoned->GetMaxHP() > 300000)) { // raid bosses may not have been summonable
		return false;
	}

	if(GetHPRatio() >= RuleR(Quarm, BardInstagibHPRatio))
		return false;

	if (entity_list.GetTopHateCount(summoned) < RuleI(Quarm, BardInstagibPullLimit))
		return false;

	// now validate the timer
	Timer* timer = GetSpecialAbilityTimer(SpecialAbility::Summon);
	if (!timer) {
		// dont currently have a timer going, so we are going to summon
		return true;
	}
	else {
		// we have a timer going, so see if its ready to use.
		if (timer->Check(false))
			return true;
	}


	// no go, aren't going to summon someone this time.
	return false;

}

bool Mob::CheckHateSummon(Mob* summoned) {
	// check if mob has ability to summon
	// 97% is the offical % that summoning starts on live, not 94
	if (!summoned) {
		return false;
	}

	if (IsCharmedPet() || summoned->PermaRooted() || (summoned->IsNPC() && summoned->GetMaxHP() > 300000)) { // raid bosses may not have been summonable
		return false;
	}

	int summon_level = GetSpecialAbility(SpecialAbility::Summon);
	if(summon_level != 1 && summon_level != 2) {
		//unsupported summon level or OFF
		return false;
	} 

	// validate hp
	int hp_ratio = GetSpecialAbilityParam(SpecialAbility::Summon, 1);
	hp_ratio = hp_ratio > 0 ? hp_ratio : 97;
	if(GetHPRatio() > static_cast<float>(hp_ratio)) {
		return false;
	}

	// this is so we don't have to make duplicate types; some mob types are 48-52 and only the 51-52s should summon
	if (IsNPC() && GetLevel() < 51 && GetLevel() > 47 && zone->GetZoneExpansion() < LuclinEQ) {
		return false;
	}

	// now validate the timer
	Timer *timer = GetSpecialAbilityTimer(SpecialAbility::Summon);
	if (!timer) {
		// dont currently have a timer going, so we are going to summon
		return true;
	} else {
		// we have a timer going, so see if its ready to use.
		if(timer->Check(false))
			return true;
	}

	// no go, aren't going to summon someone this time.
	return false;
}

bool Mob::BardHateSummon(Mob* summoned) {
	// check if mob has ability to summon
	// 97% is the offical % that summoning starts on live, not 94
	if (!summoned)
		summoned = GetTarget();

	if (!summoned)
		return false;

	if (IsCharmedPet())
		return false;

	if (zone->GetGuildID() != GUILD_NONE)
		return false;

	// now validate the timer
	int summon_timer_duration = RuleI(Quarm, BardInstagibResummonTimerMS);
	Timer* timer = GetSpecialAbilityTimer(SpecialAbility::Summon);
	if (!timer)
	{
		StartSpecialAbilityTimer(SpecialAbility::Summon, summon_timer_duration);
	}
	else {
		if (!timer->Check())
			return false;

		timer->Start(summon_timer_duration);
	}

	// get summon target
	SetTarget(summoned);
	if (target)
	{
			Mob* summoner = this;
			Mob* summon_target = target;
			if (RuleB(Quarm, BardInstagibReverseSummon)) {
				summoner = target;
				summon_target = this;
			}

			entity_list.MessageClose(this, true, 500, Chat::Say, "%s says,'You will not evade me, %s!' ", GetCleanName(), target->GetCleanName());
			glm::vec3 dest(summoner->m_Position.x, summoner->m_Position.y, summoner->m_Position.z);
			// this will ensure that the summon target is summoned very slightly in front of the summoner
			if (summoner->GetHeading() < 43.0f || summoner->GetHeading() > 212.0f)
				dest.y += 1.0f;
			else if (summoner->GetHeading() > 85.0f && summoner->GetHeading() < 171.0f)
				dest.y -= 1.0f;
			if (summoner->GetHeading() > 21.0f && summoner->GetHeading() < 107.0f)
				dest.x += 1.0f;
			else if (summoner->GetHeading() > 149.0f && summoner->GetHeading() < 234.0f)
				dest.x -= 1.0f;

			float newz = zone->zonemap->FindBestZ(dest, nullptr);
			if (newz != BEST_Z_INVALID)
				newz = summon_target->SetBestZ(newz);
			bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(glm::vec3(m_Position.x, m_Position.y, newz)) || zone->IsWaterZone(newz);
			if (newz != BEST_Z_INVALID && !in_liquid)
				dest.z = newz;
			if (summon_target->IsClient()) {
				summon_target->CastToClient()->MovePC(zone->GetZoneID(), dest.x, dest.y, dest.z, summon_target->GetHeading(), 0, SummonPC);
			}
			else {
				summon_target->GMMove(dest.x, dest.y, dest.z, summon_target->GetHeading());
			}

			return true;
	}
	return false;
}

bool Mob::HateSummon(Mob* summoned) {
	// check if mob has ability to summon
	// 97% is the offical % that summoning starts on live, not 94
	if (!summoned)
		summoned = GetTarget();

	if (!summoned)
		return false;

	if (IsCharmedPet())
		return false;

	int summon_level = GetSpecialAbility(SpecialAbility::Summon);
	if (summon_level != 1 && summon_level != 2)
	{
		//unsupported summon level or OFF
		return false;
	}

	// validate hp
	int hp_ratio = GetSpecialAbilityParam(SpecialAbility::Summon, 1);
	hp_ratio = hp_ratio > 0 ? hp_ratio : 97;
	if(GetHPRatio() > static_cast<float>(hp_ratio)) {
		return false;
	}

	// now validate the timer
	int summon_timer_duration = GetSpecialAbilityParam(SpecialAbility::Summon, 0);
	int defaultTime = 11000;
	if (GetLevel() > 65)
		defaultTime = 6000;
	summon_timer_duration = summon_timer_duration > 0 ? summon_timer_duration : defaultTime;
	Timer *timer = GetSpecialAbilityTimer(SpecialAbility::Summon);
	if (!timer)
	{
		StartSpecialAbilityTimer(SpecialAbility::Summon, summon_timer_duration);
	} else {
		if(!timer->Check())
			return false;

		timer->Start(summon_timer_duration);
	}

	// get summon target
	SetTarget(summoned);
	if(target)
	{
		if(summon_level == 1) {
			entity_list.MessageClose(this, true, 500, Chat::Say, "%s says,'You will not evade me, %s!' ", GetCleanName(), target->GetCleanName() );
			glm::vec3 dest(m_Position.x, m_Position.y, m_Position.z);
			// this will ensure that the player is summoned very slightly in front of the NPC
			if (GetHeading() < 43.0f || GetHeading() > 212.0f)
				dest.y += 1.0f;
			else if (GetHeading() > 85.0f && GetHeading() < 171.0f)
				dest.y -= 1.0f;
			if (GetHeading() > 21.0f && GetHeading() < 107.0f)
				dest.x += 1.0f;
			else if (GetHeading() > 149.0f && GetHeading() < 234.0f)
				dest.x -= 1.0f;

			float newz = zone->zonemap->FindBestZ(dest, nullptr);
			if (newz != BEST_Z_INVALID)
				newz = target->SetBestZ(newz);
			bool in_liquid = zone->HasWaterMap() && zone->watermap->InLiquid(glm::vec3(m_Position.x, m_Position.y, newz)) || zone->IsWaterZone(newz);
			if (newz != BEST_Z_INVALID && !in_liquid)
				dest.z = newz;
			if (target->IsClient()) {
				target->CastToClient()->MovePC(zone->GetZoneID(), dest.x, dest.y, dest.z, target->GetHeading(), 0, SummonPC);
			}
			else {
				target->GMMove(dest.x, dest.y, dest.z, target->GetHeading());
			}

			return true;
		} else if(summon_level == 2) {
			entity_list.MessageClose(this, true, 500, Chat::Say, "%s says,'You will not evade me, %s!'", GetCleanName(), target->GetCleanName());
			GMMove(target->GetX(), target->GetY(), target->GetZ());
		}
	}
	return false;
}

void Mob::FaceTarget(Mob* mob_to_face /*= 0*/) {
	Mob* faced_mob = mob_to_face;
	if (!faced_mob) {
		if (!GetTarget()) {
			StopNavigation();
			return;
		}
		else {
			faced_mob = GetTarget();
		}
	}

	float current_heading = GetHeading();
	current_heading = FixHeading(current_heading);
	float new_heading = CalculateHeadingToTarget(faced_mob->GetX(), faced_mob->GetY());
	new_heading = FixHeading(new_heading);
	if (static_cast<int16>(current_heading) != static_cast<int16>(new_heading)) {
		StopNavigation(new_heading);
	}
	else {
		StopNavigation();
	}

	if (IsNPC() && !IsEngaged()) {
		CastToNPC()->GetRefaceTimer()->Start(15000);
		CastToNPC()->GetRefaceTimer()->Enable();
	}
}

bool Mob::RemoveFromHateList(Mob* mob)
{
	bool bFound = false;
	if (!hate_list.IsEmpty())
	{
		bFound = hate_list.RemoveEnt(mob);
	}
	if(GetTarget() == mob)
	{
		SetTarget(hate_list.GetTop());
	}

	return bFound;
}

void Mob::WipeHateList(bool from_memblur)
{
	if (!hate_list.IsEmpty())
	{
		SetTarget(nullptr);
		hate_list.Wipe(from_memblur);
		DamageTotalsWipe(from_memblur);
	}

	if (!from_memblur)
	{
		m_EngagedClientNames.clear();
	}
}

uint32 Mob::RandomTimer(int min,int max) {
	int r = 14000;
	if(min != 0 && max != 0 && min < max)
	{
		r = zone->random.Int(min, max);
	}
	return r;
}

// This sends a WearChange while determining the mob's current apperance.
void Mob::SendWearChange(uint8 material_slot, Client* sendto, bool skip_if_zero, bool update_textures, bool illusioned)
{
	if (!RuleB(Quarm, UseFixedShowHelmBehavior)) {
		if (material_slot == EQ::textures::armorHead && sendto && !sendto->ShowHelm())
		{
			return;
		}
	}

	if (IsClient() && !IsPlayableRace(GetRace()) && material_slot < EQ::textures::weaponPrimary)
	{
		Log(Logs::Detail, Logs::Inventory, "%s tried to send a wearchange while they are illusioned as race %d. Returning.", GetName(), GetRace());
		return;
	}

	auto outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	int16 texture = 0; uint32 color = 0;
	if (illusioned && IsClient())
	{
		CastToClient()->GetPCEquipMaterial(material_slot, texture, color);
	}
	else
	{
		texture = GetEquipmentMaterial(material_slot);
		color = GetEquipmentColor(material_slot);
	}

	wc->spawn_id = GetID();
	wc->material = texture;
	wc->color.Color = color;
	wc->wear_slot_id = material_slot;

	if (update_textures && IsClient() && material_slot < EQ::textures::weaponPrimary)
	{
		CastToClient()->SetPCTexture(wc->wear_slot_id, wc->material, wc->color.Color);
	}

	if (skip_if_zero && wc->material == 0 && wc->color.Color == 0)
	{
		safe_delete(outapp);
		return;
	}

	if (sendto)
	{
		sendto->QueuePacket(outapp);
	}
	else
	{
		Log(Logs::Detail, Logs::Inventory, "SendWearChange(): %s is sending a wear change to the zone. material %d color %d on slot %d", GetName(), wc->material, wc->color, material_slot);
		entity_list.QueueWearChange(this, outapp, false, material_slot);
	}
	safe_delete(outapp);

}

// This sends a WearChange based on the apperance variables passed to it.
void Mob::WearChange(uint8 material_slot, uint16 texture, uint32 color, Client* sendto)
{
	if (IsClient() && !IsPlayableRace(GetRace()) && material_slot < EQ::textures::weaponPrimary)
	{
		Log(Logs::Detail, Logs::Inventory, "%s tried to send a wearchange while they are illusioned as race %d. Returning.", GetName(), GetRace());
		return;
	}

	if (IsNPC())
	{
		armor_tint.Slot[material_slot].Color = color;
	}

	auto outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	wc->spawn_id = this->GetID();
	wc->material = texture;
	wc->color.Color = color;
	wc->wear_slot_id = material_slot;

	if (sendto)
	{
		sendto->QueuePacket(outapp);
	}
	else
	{
		Log(Logs::Detail, Logs::Inventory, "WearChange(): %s is sending a wear change to the zone. material %d color %d on slot %d", GetName(), wc->material, wc->color.Color, material_slot);
		bool force_helm = wc->wear_slot_id == EQ::textures::armorHead && wc->material == 0 && wc->color.Color == 0;
		entity_list.QueueWearChange(this, outapp, false, material_slot, force_helm);
	}
	safe_delete(outapp);
}

int32 Mob::GetEquipmentMaterial(uint8 material_slot) const
{
	const EQ::ItemData *item = nullptr;
	item = database.GetItem(GetEquipment(material_slot));
	if(item != 0 && item->ItemClass == EQ::item::ItemClassCommon)
	{
		// Hack to force custom crowns to show correctly.
		if (material_slot == EQ::textures::armorHead)
		{
			if (strlen(item->IDFile) > 2)
			{
				uint16 material = atoi(&item->IDFile[2]);
				if (material == 240 && item->Material == TextureCloth)
				{
					return material;
				}
			}

			return item->Material;
		}

		if	// for primary and secondary we need the model, not the material
		(
			material_slot == EQ::textures::weaponPrimary ||
			material_slot == EQ::textures::weaponSecondary
		)
		{
			if (strlen(item->IDFile) > 2)
				return atoi(&item->IDFile[2]);
			else	//may as well try this, since were going to 0 anyways
				return item->Material;
		}
		else
		{
			return item->Material;
		}
	}
	else if(IsNPC())
	{
		if(material_slot == EQ::textures::armorHead)
			return helmtexture;
		else if(material_slot == EQ::textures::armorChest)
			return chesttexture;
		else if(material_slot == EQ::textures::armorArms)
			return armtexture;
		else if(material_slot == EQ::textures::armorWrist)
			return bracertexture;
		else if(material_slot == EQ::textures::armorHands)
			return handtexture;
		else if(material_slot == EQ::textures::armorLegs)
			return legtexture;
		else if(material_slot == EQ::textures::armorFeet)
			return feettexture;
	}

	return 0;
}

uint32 Mob::GetEquipmentColor(uint8 material_slot) const
{
	const EQ::ItemData *item = nullptr;

	item = database.GetItem(GetEquipment(material_slot));
	if(item != 0 && item->ItemClass == EQ::item::ItemClassCommon)
	{
		return item->Color;
	}

	return 0;
}

// works just like a printf
void Mob::Say(const char *format, ...)
{
	char buf[1000];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, 1000, format, ap);
	va_end(ap);

	Mob* talker = this;
	if(spellbonuses.VoiceGraft != 0) {
		if(spellbonuses.VoiceGraft == GetPetID())
			talker = entity_list.GetMob(spellbonuses.VoiceGraft);
		else
			spellbonuses.VoiceGraft = 0;
	}

	if(!talker)
		talker = this;

	entity_list.MessageClose_StringID(talker, false, RuleI(Range, Say), 10,
		GENERIC_SAY, GetCleanName(), buf);
}

//
// solar: this is like the above, but the first parameter is a string id
//
void Mob::Say_StringID(uint32 string_id, const char *message3, const char *message4, const char *message5, const char *message6, const char *message7, const char *message8, const char *message9)
{
	char string_id_str[10];

	snprintf(string_id_str, 10, "%d", string_id);

	entity_list.MessageClose_StringID(this, false, RuleI(Range, Say), 10,
		GENERIC_STRINGID_SAY, GetCleanName(), string_id_str, message3, message4, message5,
		message6, message7, message8, message9
	);
}

void Mob::Say_StringID(uint32 type, uint32 string_id, const char *message3, const char *message4, const char *message5, const char *message6, const char *message7, const char *message8, const char *message9)
{
	char string_id_str[10];

	snprintf(string_id_str, 10, "%d", string_id);

	entity_list.MessageClose_StringID(this, false, RuleI(Range, Say), type,
		GENERIC_STRINGID_SAY, GetCleanName(), string_id_str, message3, message4, message5,
		message6, message7, message8, message9
	);
}

void Mob::Shout(const char *format, ...)
{
	char buf[1000];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, 1000, format, ap);
	va_end(ap);

	uint32 message_type = Chat::Shout;
	if (IsNPC() || (IsCorpse() && !IsPlayerCorpse()))
		message_type = Chat::White;

	entity_list.Message_StringID(this, false, message_type,
		GENERIC_SHOUT, GetCleanName(), buf);
}

void Mob::Emote(const char *format, ...)
{
	char buf[1000];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, 1000, format, ap);
	va_end(ap);

	entity_list.MessageClose_StringID(this, false, RuleI(Range, Emote), 10,
		GENERIC_EMOTE, GetCleanName(), buf);
}

const char *Mob::GetCleanName()
{
	if(!strlen(clean_name))
	{
		CleanMobName(GetName(), clean_name);
	}

	return clean_name;
}

const char *Mob::GetCleanOwnerName()
{
	if (!strlen(clean_name_spaces))
	{
		CleanMobNameWithSpaces(GetName(), clean_name_spaces);
	}

	return clean_name_spaces;
}

std::string Mob::GetTargetDescription(Mob *target, uint8 description_type, uint16 entity_id_override)
{
	std::string self_return = "yourself";

	switch (description_type)
	{
		case TargetDescriptionType::LCSelf:
		{
			self_return = "yourself";
			break;
		}
		case TargetDescriptionType::UCSelf:
		{
			self_return = "Yourself";
			break;
		}
		case TargetDescriptionType::LCYou:
		{
			self_return = "you";
			break;
		}
		case TargetDescriptionType::UCYou:
		{
			self_return = "You";
			break;
		}
		case TargetDescriptionType::LCYour:
		{
			self_return = "your";
			break;
		}
		case TargetDescriptionType::UCYour:
		{
			self_return = "Your";
			break;
		}
		default:
		{
			break;
		}
	}


	auto d = fmt::format(
		"{}",
		(
			target && this == target ?
			self_return :
			fmt::format(
				"{} ({})",
				target->GetCleanName(),
				entity_id_override ? entity_id_override : target->GetID()
			)
		)
	);

	return d;
}

// hp event
void Mob::SetNextHPEvent( int hpevent )
{
	nexthpevent = hpevent;
}

void Mob::SetNextIncHPEvent( int inchpevent )
{
	nextinchpevent = inchpevent;
}
//warp for quest function,from sandy
void Mob::Warp(const glm::vec3& location)
{
	if(IsNPC())
		entity_list.ProcessMove(CastToNPC(), location.x, location.y, location.z);

	m_Position = glm::vec4(location, FixHeading(m_Position.w));

	Mob* target = GetTarget();
	if (target)
		FaceTarget( target );

	mMovementManager->Teleport(this, m_Position.x, m_Position.y, m_Position.z, m_Position.w);
}

int16 Mob::GetResist(uint8 type) const
{
	if (IsNPC())
	{
		if (type == 1)
			return MR + spellbonuses.MR + itembonuses.MR;
		else if (type == 2)
			return FR + spellbonuses.FR + itembonuses.FR;
		else if (type == 3)
			return CR + spellbonuses.CR + itembonuses.CR;
		else if (type == 4)
			return PR + spellbonuses.PR + itembonuses.PR;
		else if (type == 5)
			return DR + spellbonuses.DR + itembonuses.DR;
	}
	else if (IsClient())
	{
		if (type == 1)
			return CastToClient()->GetMR();
		else if (type == 2)
			return CastToClient()->GetFR();
		else if (type == 3)
			return CastToClient()->GetCR();
		else if (type == 4)
			return CastToClient()->GetPR();
		else if (type == 5)
			return CastToClient()->GetDR();
	}
	return 25;
}

uint32 Mob::GetLevelHP(uint8 tlevel)
{
	int multiplier = 0;
	if (tlevel < 10)
	{
		multiplier = tlevel*20;
	}
	else if (tlevel < 20)
	{
		multiplier = tlevel*25;
	}
	else if (tlevel < 40)
	{
		multiplier = tlevel*tlevel*12*((tlevel*2+60)/100)/10;
	}
	else if (tlevel < 45)
	{
		multiplier = tlevel*tlevel*15*((tlevel*2+60)/100)/10;
	}
	else if (tlevel < 50)
	{
		multiplier = tlevel*tlevel*175*((tlevel*2+60)/100)/100;
	}
	else
	{
		multiplier = tlevel*tlevel*2*((tlevel*2+60)/100)*(1+((tlevel-50)*20/10));
	}
	return multiplier;
}

int32 Mob::GetActSpellCasttime(uint16 spell_id, int32 casttime) 
{
	return casttime;
}

bool Mob::ExecWeaponProc(const EQ::ItemInstance *inst, uint16 spell_id, Mob *on)
{
	if (spell_id == SPELL_UNKNOWN || IsNoCast() || spell_id == 0) {
		return false;
	}

	if (IsSilenced())
	{
		Message_StringID(Chat::SpellFailure, SILENCED_STRING);
		return false;
	}

	if (IsClient() && on->GetSpecialAbility(SpecialAbility::HarmFromClientImmunity))
		return false;

	if(!IsValidSpell(spell_id)) { // Check for a valid spell otherwise it will crash through the function
		if(IsClient()){
			// Battle Fists will trigger this. The item was collected from AK, and has a spell proc 4113 which is correct for later
			// eras. However, it is beyond what is in our spells_en file. 
			Log(Logs::Detail, Logs::Spells, "Player %s, Weapon Procced invalid spell %u", this->GetName(), spell_id);
		}
		return false;
	}

	if(inst && IsClient()) {
		//const cast is dirty but it would require redoing a ton of interfaces at this point
		//It should be safe as we don't have any truly const EQ::ItemInstance floating around anywhere.
		//So we'll live with it for now
		int i = parse->EventItem(EVENT_WEAPON_PROC, CastToClient(), const_cast<EQ::ItemInstance*>(inst), on, "", spell_id);
		if(i != 0) {
			return false;
		}
	}

	if ((inst && inst->GetID() == 14811) ||	// Iron Bound Tome was bugged during this era and would proc on self
		IsBeneficialSpell(spell_id))
	{
		SpellFinished(spell_id, this, EQ::spells::CastingSlot::Item, 0, -1, spells[spell_id].ResistDiff, true);
		return true;
	}
	else if(!(on->IsClient() && on->CastToClient()->dead)) //dont proc on dead clients
	{ 
		SpellFinished(spell_id, on, EQ::spells::CastingSlot::Item, 0, -1, spells[spell_id].ResistDiff, true);
		return true;
	}
	return false;
}

uint32 Mob::GetZoneID() const {
	return(zone->GetZoneID());
}

// returns 100 for no haste/slow
int Mob::GetHaste()
{
	if (spellbonuses.haste < 0)
		return 100 + spellbonuses.haste;		// we're slowed

	int haste = 100;
	int cap = GetHasteCap();
	int level = GetLevel();

	if (spellbonuses.haste)
		haste += spellbonuses.haste;
	if (spellbonuses.hastetype2 && level > 49)
		haste += spellbonuses.hastetype2 > 10 ? 10 : spellbonuses.hastetype2;

	// only clients and pets (including charmed pets) use worn item haste
	if (IsClient() || IsPet())
	{
		// 26+ no cap, 1-25 10
		if (level > 25) // 26+
			haste += itembonuses.haste;
		else // 1-25
			haste += itembonuses.haste > 10 ? 10 : itembonuses.haste;
	}

	if (haste > cap)
		haste = cap;

	// 51+ 25 (despite there being higher spells...), 1-50 10
	if (level > 50) // 51+
		haste += spellbonuses.hastetype3 > 25 ? 25 : spellbonuses.hastetype3;
	else // 1-50
		haste += spellbonuses.hastetype3 > 10 ? 10 : spellbonuses.hastetype3;

	haste += ExtraHaste;	//GM granted haste.

	return haste;
}

void Mob::SetTarget(Mob* mob)
{
	if (target == mob) return;
	target = mob;
	if(IsNPC())
		parse->EventNPC(EVENT_TARGET_CHANGE, CastToNPC(), mob, "", 0);
	else if (IsClient())
		parse->EventPlayer(EVENT_TARGET_CHANGE, CastToClient(), "", 0);
	
	return;
}

float Mob::FindGroundZ(float new_x, float new_y, float z_offset)
{
	float ret = BEST_Z_INVALID;
	if (zone->zonemap != nullptr)
	{
		glm::vec3 me;
		me.x = new_x;
		me.y = new_y;
		me.z = m_Position.z + z_offset;
		glm::vec3 hit;
		float best_z = zone->zonemap->FindBestZ(me, &hit);
		if (best_z != BEST_Z_INVALID)
		{
			ret = best_z;
		}
	}
	return ret;
}

// Copy of above function that isn't protected to be exported to Lua
float Mob::GetGroundZ(float new_x, float new_y, float z_find_offset)
{
	float ret = BEST_Z_INVALID;
	if (zone->zonemap != 0)
	{
		glm::vec3 me;
		me.x = new_x;
		me.y = new_y;
		if (z_offset < z_find_offset)
			me.z = m_Position.z + z_find_offset;
		glm::vec3 hit;
		float best_z = zone->zonemap->FindBestZ(me, &hit);
		if (best_z != BEST_Z_INVALID)
		{
			ret = best_z;
		}
	}
	return ret;
}

//helper function for npc AI; needs to be mob:: cause we need to be able to count buffs on other clients and npcs
int Mob::CountDispellableBuffs()
{
	int val = 0;
	int buff_count = GetMaxTotalSlots();
	for(int x = 0; x < buff_count; x++)
	{
		if(!IsValidSpell(buffs[x].spellid))
			continue;

		if(buffs[x].counters)
			continue;

		if(spells[buffs[x].spellid].goodEffect == 0)
			continue;

		if(buffs[x].spellid != SPELL_UNKNOWN &&	spells[buffs[x].spellid].buffdurationformula != DF_Permanent)
			val++;
	}
	return val;
}

// Returns the % that a mob is snared (as a positive value). -1 means not snared
int Mob::GetSnaredAmount()
{
	int worst_snare = -1;

	int buff_count = GetMaxTotalSlots();
	for (int i = 0; i < buff_count; i++)
	{
		if (!IsValidSpell(buffs[i].spellid))
			continue;

		for(int j = 0; j < EFFECT_COUNT; j++)
		{
			if (spells[buffs[i].spellid].effectid[j] == SE_MovementSpeed)
			{
				int val = CalcSpellEffectValue_formula(spells[buffs[i].spellid].formula[j], spells[buffs[i].spellid].base[j], spells[buffs[i].spellid].max[j], buffs[i].casterlevel, buffs[i].spellid);
				//int effect = CalcSpellEffectValue(buffs[i].spellid, spells[buffs[i].spellid].effectid[j], buffs[i].casterlevel);
				if (val < 0 && std::abs(val) > worst_snare)
					worst_snare = std::abs(val);
			}
		}
	}

	return worst_snare;
}

bool Mob::ClearEntityVariables()
{
	if (m_EntityVariables.empty()) {
		return false;
	}

	m_EntityVariables.clear();
	return true;
}

bool Mob::DeleteEntityVariable(std::string variable_name)
{
	if (m_EntityVariables.empty() || variable_name.empty()) {
		return false;
	}

	auto v = m_EntityVariables.find(variable_name);
	if (v == m_EntityVariables.end()) {
		return false;
	}

	m_EntityVariables.erase(v);
	return true;
}

std::string Mob::GetEntityVariable(std::string variable_name)
{
	if (m_EntityVariables.empty() || variable_name.empty()) {
		return std::string();
	}

	const auto& v = m_EntityVariables.find(variable_name);
	if (v != m_EntityVariables.end()) {
		return v->second;
	}

	return std::string();
}

std::vector<std::string> Mob::GetEntityVariables()
{
	std::vector<std::string> l;
	if (m_EntityVariables.empty()) {
		return l;
	}

	for (const auto& v : m_EntityVariables) {
		l.push_back(v.first);
	}

	return l;
}

bool Mob::EntityVariableExists(std::string variable_name)
{
	if (m_EntityVariables.empty() || variable_name.empty()) {
		return false;
	}

	const auto& v = m_EntityVariables.find(variable_name);
	if (v != m_EntityVariables.end()) {
		return true;
	}

	return false;
}

void Mob::SetEntityVariable(std::string variable_name, std::string variable_value)
{
	if (variable_name.empty()) {
		return;
	}

	m_EntityVariables[variable_name] = variable_value;
}

void Mob::SetFlyMode(GravityBehavior flymode)
{
	flymode = flymode;
}

void Mob::Teleport(const glm::vec3& pos)
{
	mMovementManager->Teleport(this, pos.x, pos.y, pos.z, m_Position.w);
}

void Mob::Teleport(const glm::vec4& pos)
{
	mMovementManager->Teleport(this, pos.x, pos.y, pos.z, pos.w);
}



float Mob::GetDefaultRaceSize() const {
	return GetRaceGenderDefaultHeight(race, gender);
}


// For when we want a Ground Z at a location we are not at yet
// Like MoveTo.
float Mob::FindDestGroundZ(glm::vec3 dest, float z_find_offset)
{
	float best_z = BEST_Z_INVALID;
	if (zone->zonemap != nullptr)
	{
		if (z_offset < z_find_offset)
			dest.z += (z_find_offset - z_offset);
		best_z = zone->zonemap->FindBestZ(dest, nullptr);
	}
	return best_z;
}

int16 Mob::GetHealRate(uint16 spell_id, Mob* caster) {

	int16 heal_rate = 0;

	heal_rate += itembonuses.HealRate + spellbonuses.HealRate + aabonuses.HealRate;

	if(heal_rate < -99)
		heal_rate = -99;

	return heal_rate;
}

std::string Mob::GetGlobal(const char *varname) {
	int qgCharid = 0;
	int qgNpcid = 0;
	
	if (this->IsNPC())
		qgNpcid = this->GetNPCTypeID();
	
	if (this->IsClient())
		qgCharid = this->CastToClient()->CharacterID();
	
	QGlobalCache *qglobals = nullptr;
	std::list<QGlobal> globalMap;
	
	if (this->IsClient())
		qglobals = this->CastToClient()->GetQGlobals();
	
	if (this->IsNPC())
		qglobals = this->CastToNPC()->GetQGlobals();

	if(qglobals)
		QGlobalCache::Combine(globalMap, qglobals->GetBucket(), qgNpcid, qgCharid, zone->GetZoneID());
	
	auto iter = globalMap.begin();
	while(iter != globalMap.end()) {
		if ((*iter).name.compare(varname) == 0)
			return (*iter).value;

		++iter;
	}
	
	return "Undefined";
}

void Mob::SetGlobal(const char *varname, const char *newvalue, int options, const char *duration, Mob *other) {

	int qgZoneid = zone->GetZoneID();
	int qgCharid = 0;
	int qgNpcid = 0;

	if (this->IsNPC())
	{
		qgNpcid = this->GetNPCTypeID();
	}
	else if (other && other->IsNPC())
	{
		qgNpcid = other->GetNPCTypeID();
	}

	if (this->IsClient())
	{
		qgCharid = this->CastToClient()->CharacterID();
	}
	else if (other && other->IsClient())
	{
		qgCharid = other->CastToClient()->CharacterID();
	}
	else
	{
		qgCharid = -qgNpcid;		// make char id negative npc id as a fudge
	}

	if (options < 0 || options > 7)
	{
		//cerr << "Invalid options for global var " << varname << " using defaults" << endl;
		options = 0;	// default = 0 (only this npcid,player and zone)
	}
	else
	{
		if (options & 1)
			qgNpcid=0;
		if (options & 2)
			qgCharid=0;
		if (options & 4)
			qgZoneid=0;
	}

	InsertQuestGlobal(qgCharid, qgNpcid, qgZoneid, varname, newvalue, QGVarDuration(duration));
}

void Mob::TarGlobal(const char *varname, const char *value, const char *duration, int qgNpcid, int qgCharid, int qgZoneid)
{
	InsertQuestGlobal(qgCharid, qgNpcid, qgZoneid, varname, value, QGVarDuration(duration));
}

void Mob::DelGlobal(const char *varname) {

	if (!zone) {
		return;
	}

	int qgZoneid=zone->GetZoneID();
	int qgCharid=0;
	int qgNpcid=0;

	if (this->IsNPC())
		qgNpcid = this->GetNPCTypeID();

	if (this->IsClient())
		qgCharid = this->CastToClient()->CharacterID();
	else
		qgCharid = -qgNpcid;		// make char id negative npc id as a fudge

    std::string query = StringFormat("DELETE FROM quest_globals "
                                    "WHERE name='%s' && (npcid=0 || npcid=%i) "
                                    "&& (charid=0 || charid=%i) "
                                    "&& (zoneid=%i || zoneid=0)",
                                    varname, qgNpcid, qgCharid, qgZoneid);

	database.QueryDatabase(query);

	auto pack = new ServerPacket(ServerOP_QGlobalDelete, sizeof(ServerQGlobalDelete_Struct));
	ServerQGlobalDelete_Struct *qgu = (ServerQGlobalDelete_Struct*)pack->pBuffer;

	qgu->npc_id = qgNpcid;
	qgu->char_id = qgCharid;
	qgu->zone_id = qgZoneid;
	strcpy(qgu->name, varname);

	entity_list.DeleteQGlobal(std::string((char*)qgu->name), qgu->npc_id, qgu->char_id, qgu->zone_id);
	zone->DeleteQGlobal(std::string((char*)qgu->name), qgu->npc_id, qgu->char_id, qgu->zone_id);

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

// Inserts global variable into quest_globals table
void Mob::InsertQuestGlobal(int charid, int npcid, int zoneid, const char *varname, const char *varvalue, int duration) {

	// Make duration string either "unix_timestamp(now()) + xxx" or "NULL"
	std::stringstream duration_ss;

	if (duration == INT_MAX)
		duration_ss << "NULL";
	else
		duration_ss << "unix_timestamp(now()) + " << duration;

	//NOTE: this should be escaping the contents of arglist
	//npcwise a malicious script can arbitrarily alter the DB
	uint32 last_id = 0;
	std::string query = StringFormat("REPLACE INTO quest_globals "
                                    "(charid, npcid, zoneid, name, value, expdate)"
                                    "VALUES (%i, %i, %i, '%s', '%s', %s)",
                                    charid, npcid, zoneid, varname, varvalue, duration_ss.str().c_str());
	database.QueryDatabase(query);

	if(zone)
	{
		//first delete our global
		auto pack = new ServerPacket(ServerOP_QGlobalDelete, sizeof(ServerQGlobalDelete_Struct));
		ServerQGlobalDelete_Struct *qgd = (ServerQGlobalDelete_Struct*)pack->pBuffer;
		qgd->npc_id = npcid;
		qgd->char_id = charid;
		qgd->zone_id = zoneid;
		qgd->from_zone_id = zone->GetZoneID();
		strcpy(qgd->name, varname);

		entity_list.DeleteQGlobal(std::string((char*)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);
		zone->DeleteQGlobal(std::string((char*)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);

		worldserver.SendPacket(pack);
		safe_delete(pack);

		//then create a new one with the new id
		pack = new ServerPacket(ServerOP_QGlobalUpdate, sizeof(ServerQGlobalUpdate_Struct));
		ServerQGlobalUpdate_Struct *qgu = (ServerQGlobalUpdate_Struct*)pack->pBuffer;
		qgu->npc_id = npcid;
		qgu->char_id = charid;
		qgu->zone_id = zoneid;

		if(duration == INT_MAX)
			qgu->expdate = 0xFFFFFFFF;
		else
			qgu->expdate = Timer::GetTimeSeconds() + duration;

		strcpy((char*)qgu->name, varname);
		strcpy((char*)qgu->value, varvalue);
		qgu->id = last_id;
		qgu->from_zone_id = zone->GetZoneID();

		QGlobal temp;
		temp.npc_id = npcid;
		temp.char_id = charid;
		temp.zone_id = zoneid;
		temp.expdate = qgu->expdate;
		temp.name.assign(qgu->name);
		temp.value.assign(qgu->value);
		entity_list.UpdateQGlobal(qgu->id, temp);
		zone->UpdateQGlobal(qgu->id, temp);

		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

}

// Converts duration string to duration value (in seconds)
// Return of INT_MAX indicates infinite duration
int Mob::QGVarDuration(const char *fmt)
{
	int duration = 0;

	// format:	Y#### or D## or H## or M## or S## or T###### or C#######

	int len = static_cast<int>(strlen(fmt));

	// Default to no duration
	if (len < 1)
		return 0;

	// Set val to value after type character
	// e.g., for "M3924", set to 3924
	int val = atoi(&fmt[0] + 1);

	switch (fmt[0])
	{
		// Forever
		case 'F':
		case 'f':
			duration = INT_MAX;
			break;
		// Years
		case 'Y':
		case 'y':
			duration = val * 31556926;
			break;
		case 'D':
		case 'd':
			duration = val * 86400;
			break;
		// Hours
		case 'H':
		case 'h':
			duration = val * 3600;
			break;
		// Minutes
		case 'M':
		case 'm':
			duration = val * 60;
			break;
		// Seconds
		case 'S':
		case 's':
			duration = val;
			break;
		// Invalid
		default:
			duration = 0;
			break;
	}

	return duration;
}

bool Mob::DoKnockback(Mob *caster, float pushback, float pushup, bool send_packet)
{
	// This method should only be used for spell effects.
	if (PermaRooted())
	{
		return false;
	}

	// this should not be sent if the client is being tossed from a spell OP_Action already but is useful for gfluxed NPCs and #push
	// NPCs will need to move or send another position update or they look like they keep falling from sky after a fling like this
	if (send_packet)
	{
		auto app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
		SpawnPositionUpdates_Struct *spu = (SpawnPositionUpdates_Struct *)app->pBuffer;
		spu->num_updates = 1;
		MakeSpawnUpdate(&spu->spawn_update);
		float heading = caster->GetHeading() / 128.0f * glm::pi<float>();
		float dx = sinf(heading) * std::min(std::max(pushback, -32.0f), 32.0f);
		float dy = cosf(heading) * std::min(std::max(pushback, -32.0f), 32.0f);
		// the limit for this is -64 to 63.  a negative Z delta around -10 to -15 can kill the client for 20k falling damage
		float dz = std::min(std::max(pushup, -64.0f), 63.0f);
		//Message(MT_Broadcasts, "dx %0.2f dy %0.2f dz %0.2f", dx, dy, dz);
		spu->spawn_update.delta_yzx.SetValue(dx, dy, dz);
		entity_list.QueueCloseClients(this, app, false, 350, nullptr, true, FilterPCSpells);
		safe_delete(app);
	}

	glm::vec3 newloc(GetX(), GetY(), GetZ() + pushup);
	float newz = GetZ();

	GetPushHeadingMod(caster, pushback, newloc.x, newloc.y);
	if (pushup == 0 && zone->zonemap)
	{
		newz = zone->zonemap->FindBestZ(newloc, nullptr);
		if (newz != BEST_Z_INVALID)
			newloc.z = SetBestZ(newz);
	}

	if (CheckCoordLosNoZLeaps(GetX(), GetY(), GetZ(), newloc.x, newloc.y, newloc.z))
	{
		if (IsClient())
		{
			CastToClient()->SetKnockBackExemption(true);
		}
		else
		{
			m_Position.x = newloc.x;
			m_Position.y = newloc.y;
			m_Position.z = newloc.z;
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool Mob::CombatPush(Mob* attacker, float pushback)
{
	// Use this method for stun/combat pushback.
    if (GetRunspeed() <= 0.0 && GetWalkspeed() <= 0.0) {
        return false;
    }	

	glm::vec3 newloc(GetX(), GetY(), GetZ());
	float newz = GetZ();

	GetPushHeadingMod(attacker, pushback, newloc.x, newloc.y);
	if(zone->zonemap)
	{
		newz = zone->zonemap->FindBestZ(newloc, nullptr);
		if (newz != BEST_Z_INVALID)
			newloc.z = SetBestZ(newz);

		Log(Logs::Detail, Logs::Combat, "Push: BestZ returned %0.2f for %0.2f,%0.2f,%0.2f", newloc.z, newloc.x, newloc.y, m_Position.z);
	}

	if(CheckCoordLosNoZLeaps(m_Position.x, m_Position.y, m_Position.z, newloc.x, newloc.y, newloc.z))
	{
		Log(Logs::Detail, Logs::Combat, "Push: X: %0.2f -> %0.2f Y: %0.2f -> %0.2f Z: %0.2f -> %0.2f", m_Position.x, newloc.x, m_Position.y, newloc.y, m_Position.z, newloc.z);
		m_Position.x = newloc.x;
		m_Position.y = newloc.y;
		m_Position.z = newloc.z;

		uint8 self_update = 0;
		if(IsClient())
		{
			self_update = 1;
			CastToClient()->SetKnockBackExemption(true);
		}

		GMMove(m_Position.x, m_Position.y, m_Position.z, m_Position.w);

		return true;
	}
	return false;
}

void Mob::GetPushHeadingMod(Mob* attacker, float pushback, float &x_coord, float &y_coord)
{
	float headingRadians = attacker->GetHeading();
	headingRadians = (headingRadians * 360.0f) / 256.0f;
	if (headingRadians < 270)
		headingRadians += 90;
	else
		headingRadians -= 270;
	headingRadians = headingRadians * 3.141592654f / 180.0f;

	float tmpx = -cosf(headingRadians) * pushback;
	float tmpy = sinf(headingRadians) * pushback;

	x_coord += tmpx;
	y_coord += tmpy;
}

void Mob::SetGrouped(bool v)
{
	if(v)
	{
		israidgrouped = false;
	}
	isgrouped = v;

	if(IsClient())
	{
			parse->EventPlayer(EVENT_GROUP_CHANGE, CastToClient(), "", 0);
	}
}

void Mob::SetRaidGrouped(bool v)
{
	if(v)
	{
		isgrouped = false;
	}
	israidgrouped = v;

	if(IsClient())
	{
		parse->EventPlayer(EVENT_GROUP_CHANGE, CastToClient(), "", 0);
	}
}

int16 Mob::GetMeleeDamageMod_SE(uint16 skill)
{
	int dmg_mod = 0;

	// All skill dmg mod + Skill specific
	dmg_mod += itembonuses.DamageModifier[EQ::skills::HIGHEST_SKILL+1] + spellbonuses.DamageModifier[EQ::skills::HIGHEST_SKILL+1] + aabonuses.DamageModifier[EQ::skills::HIGHEST_SKILL+1] +
				itembonuses.DamageModifier[skill] + spellbonuses.DamageModifier[skill] + aabonuses.DamageModifier[skill];

	if(dmg_mod < -100)
		dmg_mod = -100;

	return dmg_mod;
}

int16 Mob::GetMeleeMinDamageMod_SE(uint16 skill)
{
	int dmg_mod = 0;

	dmg_mod = itembonuses.MinDamageModifier[skill] + spellbonuses.MinDamageModifier[skill] +
				itembonuses.MinDamageModifier[EQ::skills::HIGHEST_SKILL+1] + spellbonuses.MinDamageModifier[EQ::skills::HIGHEST_SKILL+1];

	if(dmg_mod < -100)
		dmg_mod = -100;

	return dmg_mod;
}

int16 Mob::GetSkillReuseTime(uint16 skill)
{
	int skill_reduction = this->itembonuses.SkillReuseTime[skill] + this->spellbonuses.SkillReuseTime[skill] + this->aabonuses.SkillReuseTime[skill];

	return skill_reduction;
}

void Mob::MeleeLifeTap(int32 damage) {

	int32 lifetap_amt = 0;
	lifetap_amt = spellbonuses.MeleeLifetap + itembonuses.MeleeLifetap + aabonuses.MeleeLifetap
				+ spellbonuses.Vampirism + itembonuses.Vampirism + aabonuses.Vampirism;

	if(lifetap_amt && damage > 0){

		lifetap_amt = damage * lifetap_amt / 100;
		Log(Logs::Detail, Logs::Combat, "Melee lifetap healing for %d damage.", damage);

		if (lifetap_amt > 0)
			HealDamage(lifetap_amt); //Heal self for modified damage amount.
		else
			Damage(this, -lifetap_amt,0, EQ::skills::SkillEvocation,false); //Dmg self for modified damage amount.
	}
}

bool Mob::TryReflectSpell(uint32 spell_id)
{
	if (!spells[spell_id].reflectable)
 		return false;

	int chance = itembonuses.reflect_chance + spellbonuses.reflect_chance + aabonuses.reflect_chance;

	if(chance && zone->random.Roll(chance))
		return true;

	return false;
}

bool Mob::IsBoat() const 
{
	return (GetBaseRace() == SHIP || GetBaseRace() == LAUNCH || GetBaseRace() == CONTROLLED_BOAT || GetBaseRace() == GHOST_SHIP);
}

void Mob::SetBodyType(uint8 new_body, bool overwrite_orig) {
	bool needs_spawn_packet = false;
	if(bodytype == 11 || bodytype >= 65 || new_body == 11 || new_body >= 65) {
		needs_spawn_packet = true;
	}

	if(overwrite_orig) {
		orig_bodytype = new_body;
	}
	bodytype = new_body;

	if(needs_spawn_packet) {
		EQApplicationPacket app;
		CreateDespawnPacket(&app, true);
		entity_list.QueueClients(this, &app);
		safe_delete_array(app.pBuffer);
		CreateSpawnPacket(&app, this);
		entity_list.QueueClients(this, &app);
		safe_delete_array(app.pBuffer);
	}
}

void Mob::SlowMitigation(Mob* caster)
{
	/*if (GetSlowMitigation() && caster && caster->IsClient())
	{
		if ((GetSlowMitigation() > 0) && (GetSlowMitigation() < 26))
			caster->Message_StringID(Chat::SpellFailure, SLOW_MOSTLY_SUCCESSFUL);

		else if ((GetSlowMitigation() >= 26) && (GetSlowMitigation() < 74))
			caster->Message_StringID(Chat::SpellFailure, SLOW_PARTIALLY_SUCCESSFUL);

		else if ((GetSlowMitigation() >= 74) && (GetSlowMitigation() < 101))
			caster->Message_StringID(Chat::SpellFailure, SLOW_SLIGHTLY_SUCCESSFUL);

		else if (GetSlowMitigation() > 100)
			caster->Message_StringID(Chat::SpellFailure, SPELL_OPPOSITE_EFFECT);
	}*/
}

uint16 Mob::GetSkillByItemType(int ItemType)
{
	switch (ItemType)
	{
		case EQ::item::ItemType1HSlash:
			return EQ::skills::Skill1HSlashing;
		case EQ::item::ItemType2HSlash:
			return EQ::skills::Skill2HSlashing;
		case EQ::item::ItemType1HPiercing:
			return EQ::skills::Skill1HPiercing;
		case EQ::item::ItemType1HBlunt:
			return EQ::skills::Skill1HBlunt;
		case EQ::item::ItemType2HBlunt:
			return EQ::skills::Skill2HBlunt;
		case EQ::item::ItemType2HPiercing:
			return EQ::skills::Skill1HPiercing; // change to 2HPiercing once activated
		case EQ::item::ItemTypeBow:
			return EQ::skills::SkillArchery;
		case EQ::item::ItemTypeLargeThrowing:
		case EQ::item::ItemTypeSmallThrowing:
			return EQ::skills::SkillThrowing;
		case EQ::item::ItemTypeMartial:
			return EQ::skills::SkillHandtoHand;
		default:
			return EQ::skills::SkillHandtoHand;
	}
	return EQ::skills::SkillHandtoHand;
 }

uint8 Mob::GetItemTypeBySkill(EQ::skills::SkillType skill)
{
	switch (skill)
	{
		case EQ::skills::SkillThrowing:
			return EQ::item::ItemTypeSmallThrowing;
		case EQ::skills::SkillArchery:
			return EQ::item::ItemTypeArrow;
		case EQ::skills::Skill1HSlashing:
			return EQ::item::ItemType1HSlash;
		case EQ::skills::Skill2HSlashing:
			return EQ::item::ItemType2HSlash;
		case EQ::skills::Skill1HPiercing:
			return EQ::item::ItemType1HPiercing;
		case EQ::skills::Skill1HBlunt:
			return EQ::item::ItemType1HBlunt;
		case EQ::skills::Skill2HBlunt:
			return EQ::item::ItemType2HBlunt;
		case EQ::skills::SkillHandtoHand:
			return EQ::item::ItemTypeMartial;
		default:
			return EQ::item::ItemTypeMartial;
	}
	return EQ::item::ItemTypeMartial;
 }

uint16 Mob::GetWeaponSpeedbyHand(uint16 hand) {

	uint16 weapon_speed = 0;
	switch (hand) {
		case 0:
		case 13:
			weapon_speed = attack_timer.GetDuration();
			break;
		case 14:
			weapon_speed = attack_dw_timer.GetDuration();
			break;
		case 11:
			weapon_speed = ranged_timer.GetDuration();
			break;
	}

	if (weapon_speed < RuleI(Combat, MinHastedDelay))
		weapon_speed = RuleI(Combat, MinHastedDelay);

	return weapon_speed;
}

int8 Mob::GetDecayEffectValue(uint16 spell_id, uint16 spelleffect) {

	if (!IsValidSpell(spell_id))
		return false;

	int spell_level = spells[spell_id].classes[(GetClass()%16) - 1];
	int effect_value = 0;
	int lvlModifier = 100;

	int buff_count = GetMaxTotalSlots();
	for (int slot = 0; slot < buff_count; slot++){
		if (IsValidSpell(buffs[slot].spellid)){
			for (int i = 0; i < EFFECT_COUNT; i++){
				if(spells[buffs[slot].spellid].effectid[i] == spelleffect) {

					int critchance = spells[buffs[slot].spellid].base[i];
					int decay = spells[buffs[slot].spellid].base2[i];
					int lvldiff = spell_level - spells[buffs[slot].spellid].max[i];

					if(lvldiff > 0 && decay > 0)
					{
						lvlModifier -= decay*lvldiff;
						if (lvlModifier > 0){
							critchance = (critchance*lvlModifier)/100;
							effect_value += critchance;
						}
					}

					else
						effect_value += critchance;
				}
			}
		}
	}

	return effect_value;
}

// Faction Mods for Alliance type spells
void Mob::AddFactionBonus(uint32 pFactionID,int32 bonus) 
{
	//Normal Alliance spells cap at 300, Guide Alliance adds 2200.
	if (bonus > 300 && bonus != 2200)
		bonus = 300;

	std::map <uint32, int32> :: const_iterator faction_bonus;
	typedef std::pair <uint32, int32> NewFactionBonus;

	faction_bonus = faction_bonuses.find(pFactionID);
	if(faction_bonus == faction_bonuses.end())
	{
		faction_bonuses.emplace(NewFactionBonus(pFactionID,bonus));
	}
	else
	{
		if(faction_bonus->second<bonus)
		{
			faction_bonuses.erase(pFactionID);
			faction_bonuses.emplace(NewFactionBonus(pFactionID,bonus));
		}
	}
}

// Faction Mods from items
void Mob::AddItemFactionBonus(uint32 pFactionID,int32 bonus) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	typedef std::pair <uint32, int32> NewFactionBonus;

	faction_bonus = item_faction_bonuses.find(pFactionID);
	if(faction_bonus == item_faction_bonuses.end())
	{
		item_faction_bonuses.emplace(NewFactionBonus(pFactionID,bonus));
	}
	else
	{
		if((bonus > 0 && faction_bonus->second < bonus) || (bonus < 0 && faction_bonus->second > bonus))
		{
			item_faction_bonuses.erase(pFactionID);
			item_faction_bonuses.emplace(NewFactionBonus(pFactionID, bonus));
		}
	}
}

int32 Mob::GetFactionBonus(uint32 pFactionID) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	faction_bonus = faction_bonuses.find(pFactionID);
	if(faction_bonus != faction_bonuses.end())
	{
		return (*faction_bonus).second;
	}
	return 0;
}

int32 Mob::GetItemFactionBonus(uint32 pFactionID) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	faction_bonus = item_faction_bonuses.find(pFactionID);
	if(faction_bonus != item_faction_bonuses.end())
	{
		return (*faction_bonus).second;
	}
	return 0;
}

void Mob::ClearItemFactionBonuses() {
	item_faction_bonuses.clear();
}

void Mob::ClearFactionBonuses() {
	faction_bonuses.clear();
}

FACTION_VALUE Mob::GetSpecialFactionCon(Mob* iOther) {
	if (!iOther)
		return FACTION_INDIFFERENTLY;

	iOther = iOther->GetOwnerOrSelf();
	Mob* self = this->GetOwnerOrSelf();

	bool selfAIcontrolled = self->IsAIControlled();
	bool iOtherAIControlled = iOther->IsAIControlled();
	int selfPrimaryFaction = self->GetPrimaryFaction();
	int iOtherPrimaryFaction = iOther->GetPrimaryFaction();

	if (selfPrimaryFaction >= 0 && selfAIcontrolled)
		return FACTION_INDIFFERENTLY;
	if (iOther->GetPrimaryFaction() >= 0)
		return FACTION_INDIFFERENTLY;
/* special values:
	-2 = indiff to player, ally to AI on special values, indiff to AI
	-3 = dub to player, ally to AI on special values, indiff to AI
	-4 = atk to player, ally to AI on special values, indiff to AI
	-5 = indiff to player, indiff to AI
	-6 = dub to player, indiff to AI
	-7 = atk to player, indiff to AI
	-8 = indiff to players, ally to AI on same value, indiff to AI
	-9 = dub to players, ally to AI on same value, indiff to AI
	-10 = atk to players, ally to AI on same value, indiff to AI
	-11 = indiff to players, ally to AI on same value, atk to AI
	-12 = dub to players, ally to AI on same value, atk to AI
	-13 = atk to players, ally to AI on same value, atk to AI
*/
	switch (iOtherPrimaryFaction) {
		case -2: // -2 = indiff to player, ally to AI on special values, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_ALLY;
			else
				return FACTION_INDIFFERENTLY;
		case -3: // -3 = dub to player, ally to AI on special values, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_ALLY;
			else
				return FACTION_DUBIOUSLY;
		case -4: // -4 = atk to player, ally to AI on special values, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_ALLY;
			else
				return FACTION_SCOWLS;
		case -5: // -5 = indiff to player, indiff to AI
			return FACTION_INDIFFERENTLY;
		case -6: // -6 = dub to player, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_INDIFFERENTLY;
			else
				return FACTION_DUBIOUSLY;
		case -7: // -7 = atk to player, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_INDIFFERENTLY;
			else
				return FACTION_SCOWLS;
		case -8: // -8 = indiff to players, ally to AI on same value, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_INDIFFERENTLY;
			}
			else
				return FACTION_INDIFFERENTLY;
		case -9: // -9 = dub to players, ally to AI on same value, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_INDIFFERENTLY;
			}
			else
				return FACTION_DUBIOUSLY;
		case -10: // -10 = atk to players, ally to AI on same value, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_INDIFFERENTLY;
			}
			else
				return FACTION_SCOWLS;
		case -11: // -11 = indiff to players, ally to AI on same value, atk to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_SCOWLS;
			}
			else
				return FACTION_INDIFFERENTLY;
		case -12: // -12 = dub to players, ally to AI on same value, atk to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_SCOWLS;


			}
			else
				return FACTION_DUBIOUSLY;
		case -13: // -13 = atk to players, ally to AI on same value, atk to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_SCOWLS;
			}
			else
				return FACTION_SCOWLS;
		default:
			return FACTION_INDIFFERENTLY;
	}
}

bool Mob::HasSpellEffect(int effectid)
{
    int i;

    int buff_count = GetMaxTotalSlots();
    for(i = 0; i < buff_count; i++)
    {
        if(buffs[i].spellid == SPELL_UNKNOWN) { continue; }

        if(IsEffectInSpell(buffs[i].spellid, effectid))
        {
            return(1);
        }
    }
    return(0);
}

int Mob::GetSpecialAbility(int ability) {
	if(ability >= SpecialAbility::Max || ability < 0) {
		return 0;
	}

	return SpecialAbilities[ability].level;
}

int Mob::GetSpecialAbilityParam(int ability, int param) {
	if(param >= SpecialAbility::MaxParameters || param < 0 || ability >= SpecialAbility::Max || ability < 0) {
		return 0;
	}

	return SpecialAbilities[ability].params[param];
}

void Mob::SetSpecialAbility(int ability, int level) {
	if(ability >= SpecialAbility::Max || ability < 0) {
		return;
	}

	SpecialAbilities[ability].level = level;
	if (ability == SpecialAbility::CorpseCamper)
		AI_SetLoiterTimer();
}

void Mob::SetSpecialAbilityParam(int ability, int param, int value) {
	if(param >= SpecialAbility::MaxParameters || param < 0 || ability >= SpecialAbility::Max || ability < 0) {
		return;
	}

	SpecialAbilities[ability].params[param] = value;
}

void Mob::StartSpecialAbilityTimer(int ability, uint32 time) {
	if (ability >= SpecialAbility::Max || ability < 0) {
		return;
	}

	if(SpecialAbilities[ability].timer) {
		SpecialAbilities[ability].timer->Start(time);
	} else {
		SpecialAbilities[ability].timer = new Timer(time);
		SpecialAbilities[ability].timer->Start();
	}
}

void Mob::StopSpecialAbilityTimer(int ability) {
	if (ability >= SpecialAbility::Max || ability < 0) {
		return;
	}

	safe_delete(SpecialAbilities[ability].timer);
}

Timer *Mob::GetSpecialAbilityTimer(int ability) {
	if (ability >= SpecialAbility::Max || ability < 0) {
		return nullptr;
	}

	return SpecialAbilities[ability].timer;
}

void Mob::ClearSpecialAbilities() {
	for(int a = 0; a < SpecialAbility::Max; ++a) {
		SpecialAbilities[a].level = 0;
		safe_delete(SpecialAbilities[a].timer);
		for(int p = 0; p < SpecialAbility::MaxParameters; ++p) {
			SpecialAbilities[a].params[p] = 0;
		}
	}
}

void Mob::ProcessSpecialAbilities(const std::string &str) {
	ClearSpecialAbilities();

	std::vector<std::string> sp = Strings::Split(str, '^');
	for(auto iter = sp.begin(); iter != sp.end(); ++iter)
		ModifySpecialAbility((*iter));
}

void Mob::ModifySpecialAbility(const std::string &abil_str)
{
	std::vector<std::string> sub_sp = Strings::Split(abil_str, ',');
	if (sub_sp.size() >= 2) {
		int ability = std::stoi(sub_sp[0]);
		int value = std::stoi(sub_sp[1]);

		SetSpecialAbility(ability, value);

		for (size_t i = 2, p = 0; i < sub_sp.size(); ++i, ++p) {
			if (p >= SpecialAbility::MaxParameters) {
				break;
			}

			SetSpecialAbilityParam(ability, p, std::stoi(sub_sp[i]));
		}
	}
}

// derived from client to keep these functions more consistent
// if anything seems weird, blame SoE
bool Mob::IsFacingMob(Mob *other)
{
	if (!other)
		return false;
	float angle = HeadingAngleToMob(other);
	// what the client uses appears to be 2x our internal heading
	float heading = GetHeading();

	if (angle > 236.0 && heading < 20.0)
		angle = heading;
	if (angle < 20.0 && heading > 236.0)
		angle = heading;

	if (std::abs(angle - heading) <= 40.0)
		return true;

	return false;
}

// All numbers derived from the client
float Mob::HeadingAngleToMob(float in_x, float in_y)
{
	float this_x = GetX();
	float this_y = GetY();

	return CalculateHeadingAngleBetweenPositions(this_x, this_y, in_x, in_y);
}

float Mob::CalculatePitchToTarget(glm::vec3 loc) {
	float angle;

	float distance = DistanceNoZ(GetPosition(), loc);

	if (loc.z < m_Position.z)
		angle = 90.0f - atan((m_Position.z - loc.z) / distance) * 180.0f / 3.141592654f;
	else if (loc.z > m_Position.z)
		angle = 90.0f + atan((loc.z - m_Position.z) / distance) * 180.0f / 3.141592654f;
	else // Added?
	{
		angle = 180.0f;
	}
	return (512.0f * angle / 360.f);
}

bool Mob::GetSeeInvisible(uint8 see_invis)
{ 
	if(see_invis > 0)
	{
		if(see_invis == 1)
			return true;
		else
		{
			if (zone->random.Int(0, 99) < see_invis)
				return true;
		}
	}
	return false;
}

int32 Mob::GetSpellStat(uint32 spell_id, const char *identifier, uint8 slot)
{
	if (!IsValidSpell(spell_id))
		return 0;

	if (!identifier)
		return 0;

	int32 stat = 0;

	if (slot > 0)
		slot = slot - 1;

	std::string id = identifier;
	for(uint32 i = 0; i < id.length(); ++i)
	{
		id[i] = tolower(id[i]);
	}

	if (slot < 16){
		if (id == "classes") {stat = spells[spell_id].classes[slot]; }
		else if (id == "deities") {stat = spells[spell_id].deities[slot];}
	}

	if (slot < 12){
		if (id == "base") {stat = spells[spell_id].base[slot];}
		else if (id == "base2") {stat = spells[spell_id].base2[slot];}
		else if (id == "max") {stat = spells[spell_id].max[slot];}
		else if (id == "formula") {spells[spell_id].formula[slot];}
		else if (id == "effectid") {spells[spell_id].effectid[slot];}
	}

	if (slot < 4){
		if (id == "components") { spells[spell_id].components[slot];}
		else if (id == "component_counts") {spells[spell_id].component_counts[slot];}
		else if (id == "NoexpendReagent") {spells[spell_id].NoexpendReagent[slot];}
	}

	if (id == "range") {stat = static_cast<int32>(spells[spell_id].range); }
	else if (id == "aoerange") {stat = static_cast<int32>(spells[spell_id].aoerange);}
	else if (id == "pushback") {stat = static_cast<int32>(spells[spell_id].pushback);}
	else if (id == "pushup") {stat = static_cast<int32>(spells[spell_id].pushup);}
	else if (id == "cast_time") {stat = spells[spell_id].cast_time;}
	else if (id == "recovery_time") {stat = spells[spell_id].recovery_time;}
	else if (id == "recast_time") {stat = spells[spell_id].recast_time;}
	else if (id == "buffdurationformula") {stat = spells[spell_id].buffdurationformula;}
	else if (id == "buffduration") {stat = spells[spell_id].buffduration;}
	else if (id == "AEDuration") {stat = spells[spell_id].AEDuration;}
	else if (id == "mana") {stat = spells[spell_id].mana;}
	//else if (id == "LightType") {stat = spells[spell_id].LightType;} - Not implemented
	else if (id == "goodEffect") {stat = spells[spell_id].goodEffect;}
	else if (id == "Activated") {stat = spells[spell_id].Activated;}
	else if (id == "resisttype") {stat = spells[spell_id].resisttype;}
	else if (id == "targettype") {stat = spells[spell_id].targettype;}
	else if (id == "basedeiff") {stat = spells[spell_id].basediff;}
	else if (id == "skill") {stat = spells[spell_id].skill;}
	else if (id == "zonetype") {stat = spells[spell_id].zonetype;}
	else if (id == "EnvironmentType") {stat = spells[spell_id].EnvironmentType;}
	else if (id == "TimeOfDay") {stat = spells[spell_id].TimeOfDay;}
	else if (id == "CastingAnim") {stat = spells[spell_id].CastingAnim;}
	else if (id == "SpellAffectIndex") {stat = spells[spell_id].SpellAffectIndex; }
	else if (id == "disallow_sit") {stat = spells[spell_id].disallow_sit; }
	//else if (id == "spellanim") {stat = spells[spell_id].spellanim; } - Not implemented
	else if (id == "uninterruptable") {stat = spells[spell_id].uninterruptable; }
	else if (id == "ResistDiff") {stat = spells[spell_id].ResistDiff; }
	else if (id == "dot_stacking_exemp") {stat = spells[spell_id].dot_stacking_exempt; }
	else if (id == "RecourseLink") {stat = spells[spell_id].RecourseLink; }
	else if (id == "no_partial_resist") {stat = spells[spell_id].no_partial_resist; }
	else if (id == "short_buff_box") {stat = spells[spell_id].short_buff_box; }
	else if (id == "descnum") {stat = spells[spell_id].descnum; }
	else if (id == "effectdescnum") {stat = spells[spell_id].effectdescnum; }
	else if (id == "npc_no_los") {stat = spells[spell_id].npc_no_los; }
	else if (id == "reflectable") {stat = spells[spell_id].reflectable; }
	else if (id == "EndurCost") {stat = spells[spell_id].EndurCost; }
	else if (id == "EndurTimerIndex") {stat = spells[spell_id].EndurTimerIndex; }
	else if (id == "HateAdded") {stat = spells[spell_id].HateAdded; }
	else if (id == "EndurUpkeep") {stat = spells[spell_id].EndurUpkeep; }
	else if (id == "pvpresistbase") {stat = spells[spell_id].pvpresistbase; }
	else if (id == "pvpresistcalc") {stat = spells[spell_id].pvpresistcalc; }
	else if (id == "pvpresistcap") {stat = spells[spell_id].pvpresistcap; }
	else if (id == "spell_category") {stat = spells[spell_id].spell_category; }
	else if (id == "can_mgb") {stat = spells[spell_id].can_mgb; }
	else if (id == "dispel_flag") {stat = spells[spell_id].dispel_flag; }
	else if (id == "suspendable") {stat = spells[spell_id].suspendable; }
	else if (id == "spellgroup") {stat = spells[spell_id].spellgroup; }
	else if (id == "AllowRest") {stat = spells[spell_id].AllowRest; }
	else if (id == "DamageShieldType") {stat = spells[spell_id].DamageShieldType; }

	return stat;
}

uint32 Mob::GetRaceStringID() {

	switch (GetRace()) {
		case HUMAN:
			return 1257; break;
		case BARBARIAN:
			return 1258; break;
		case ERUDITE:
			return 1259; break;
		case WOOD_ELF:
			return 1260; break;
		case HIGH_ELF:
			return 1261; break;
		case DARK_ELF:
			return 1262; break;
		case HALF_ELF:
			return 1263; break;
		case DWARF:
			return 1264; break;
		case TROLL:
			return 1265; break;
		case OGRE:
			return 1266; break;
		case HALFLING:
			return 1267; break;
		case GNOME:
			return 1268; break;
		case IKSAR:
			return 1269; break;
		case VAHSHIR:
			return 1270; break;
		default:
			return 1256; break;
	}
}

uint32 Mob::GetClassStringID() {

	switch (GetClass()) {
		case Class::Warrior:
		case Class::WarriorGM:
			return 1240; break;
		case Class::Cleric:
		case Class::ClericGM:
			return 1241; break;
		case Class::Paladin:
		case Class::PaladinGM:
			return 1242; break;
		case Class::Ranger:
		case Class::RangerGM:
			return 1243; break;
		case Class::ShadowKnight:
		case Class::ShadowKnightGM:
			return 1244; break;
		case Class::Druid:
		case Class::DruidGM:
			return 1245; break;
		case Class::Monk:
		case Class::MonkGM:
			return 1246; break;
		case Class::Bard:
		case Class::BardGM:
			return 1247; break;
		case Class::Rogue:
		case Class::RogueGM:
			return 1248; break;
		case Class::Shaman:
		case Class::ShamanGM:
			return 1249; break;
		case Class::Necromancer:
		case Class::NecromancerGM:
			return 1250; break;
		case Class::Wizard:
		case Class::WizardGM:
			return 1251; break;
		case Class::Magician:
		case Class::MagicianGM:
			return 1252; break;
		case Class::Enchanter:
		case Class::EnchanterGM:
			return 1253; break;
		case Class::Beastlord:
		case Class::BeastlordGM:
			return 1254; break;
		case Class::Banker:
			return 1255; break;
		default:
			return 1239; break;
	}
}

float Mob::CalcZOffset()
{
	float mysize = GetSize();
	int myrace = GetRace();

	if (mysize > RuleR(Map, BestZSizeMax))
		mysize = RuleR(Map, BestZSizeMax);

	// Z offset for beastlord pets is calculated different
	if (myrace == Race::Tiger || ((myrace == Race::Wolf || myrace == Race::WolfElemental) && GetGender() == Gender::Neuter)) {
		return (mysize / 5.0f * 3.125f * 0.44999999f);
	}

	// fixed size dragons
	if (myrace == Race::LavaDragon || myrace == Race::Wurm || myrace == Race::GhostDragon) {
		return 20.0f;
	}

	return (mysize / 5.0f * 3.125f);
}
float Mob::CalcHeadOffset()
{
	float mysize = GetSize();
	int myrace = GetRace();

	if (mysize > RuleR(Map, BestZSizeMax))
		mysize = RuleR(Map, BestZSizeMax);

	if (mysize < LOS_DEFAULT_HEIGHT)
		mysize = LOS_DEFAULT_HEIGHT;

	// fixed size dragons
	if (myrace == LAVA_DRAGON || myrace == WURM || myrace == GHOST_DRAGON)
		return (16.8f);

	return (std::min(LOS_MAX_HEIGHT, mysize * HEAD_POSITION));
}
float Mob::CalcModelSize()
{
	float mysize = GetSize();
	int myrace = GetRace();

	if (mysize < LOS_DEFAULT_HEIGHT)
		mysize = LOS_DEFAULT_HEIGHT;

	// fixed size dragons
	if (myrace == LAVA_DRAGON || myrace == WURM || myrace == GHOST_DRAGON)
		return (20.0);

	return (mysize);
}

float Mob::CalcBoundingRadius()
{
	int myrace = GetRace();
	float mysize = GetSize();
	float base_size = 5.0f;
	int mygender = GetGender();
	float myradius = 6.0f;
	switch (myrace)
	{
	case 1: // "Human"
	case 2: // "Barbarian"
	case 3: // "Erudite"
	case 4: // "Wood Elf"
	case 5: // "High Elf"
	case 6: // "Dark Elf"
	case 7: // "Half Elf"
	case 8: // "Dwarf"
	case 9: // "Troll",
	case 10: // "Ogre"
	case 11: // "Halfling"
	case 12: // "Gnome"
		// playable races have fixed mods
		mysize = 5.0f;
		myradius = 5.0f;
		break;
	case 13: // "Aviak"
		break;
	case 14: // "Were Wolf"
		break;
	case 15: // "Brownie",
		break;
	case 16: // "Centaur"
		break;
	case 17: // "Golem"
		break;
	case 18: // "Giant / Cyclops"
		break;
	case 19: // "Trakanon",
		myradius = 10.48f;
		break;
	case 20:
		break;
	case 21: // "Evil Eye"
		break;
	case 22: // "Beetle"
		break;
	case 23: // "Kerra"
		break;
	case 24: // "Fish"
		break;
	case 25: // "Fairy"
		break;
	case 26: // "Old Froglok"
		break;
	case 27: // "Old Froglok Ghoul"
		break;
	case 28: // "Fungusman"
		break;
	case 29: // "Gargoyle"
		break;
	case 30: // "Gasbag"
		break;
	case 31: // "Gelatinous Cube"
		break;
	case 33: // "Ghoul"
		break;
	case 34: // "Giant Bat"
		mysize = 5.0f;
		break;
	case 36: // "Giant Rat"
		break;
	case 37: // "Giant Snake"
		break;
	case 38: // "Giant Spider"
		break;
	case 39: // "Gnoll"
		break;
	case 40: // "Goblin"
		break;
	case 41: // "Gorilla"
		break;
	case 42: // "Wolf"
		break;
	case 43: // "Bear"
		break;
	case 44: // "Freeport Guards"
		break;
	case 45: // "Demi Lich"
		break;
	case 46: // "Imp"
		break;
	case 47: // "Griffin"
		break;
	case 48: // "Kobold"
		break;
	case 49: // "Lava Dragon"
		mysize = 32.5f;
		break;
	case 50: // "Lion"
		break;
	case 51: // "Lizard Man"
		break;
	case 52: // "Mimic"
		break;
	case 53: // "Minotaur"
		break;
	case 54: // "Orc"
		break;
	case 55: // "Human Beggar"
		break;
	case 56: // "Pixie"
		break;
	case 57: // "Dracnid"
		break;
	case 58: // "Solusek Ro"
		break;
	case 59: // "Bloodgills"
		break;
	case 60: // "Skeleton"
		break;
	case 61: // "Shark"
		break;
	case 62: // "Tunare"
		break;
	case 63: // "Tiger"
		break;
	case 64: // "Treant"
		break;
	case 65: // "Vampire"
		break;
	case 66: // "Rallos Zek"
		break;
	case 67: // "Highpass Citizen"
		break;
	case 68: // "Tentacle"
		break;
	case 69: // "Will 'O Wisp"
		break;
	case 70: // "Zombie"
		break;
	case 71: // "Qeynos Citizen"
		break;
	case 74: // "Piranha"	
		break;
	case 75: // "Elemental"
		break;
	case 76: // "Puma"
		break;
	case 77: // "Neriak Citizen"
		break;
	case 78: // "Erudite Citizen"
		break;
	case 79: // "Bixie"
		break;
	case 80: // "Reanimated Hand"
		break;
	case 81: // "Rivervale Citizen"
		break;
	case 82: // "Scarecrow"
		break;
	case 83: // "Skunk"
		break;
	case 85: // "Spectre"
		break;
	case 86: // "Sphinx"
		break;
	case 87: // "Armadillo"
		break;
	case 88: // "Clockwork Gnome"
		break;
	case 89: // "Drake"
		break;
	case 90: // "Halas Citizen"
		break;
	case 91: // "Alligator"
		break;
	case 92: // "Grobb Citizen"
		break;
	case 93: // "Oggok Citizen"
		break;
	case 94: // "Kaladim Citizen"
		break;
	case 95: // "Cazic Thule"
		break;
	case 96: // "Cockatrice"
		break;
	case 98: // "Elf Vampire"
		break;
	case 99: // "Denizen"
		break;
	case 100: // "Dervish"
		break;
	case 101: // "Efreeti"
		break;
	case 102: // "Old Froglok Tadpole"
		break;
	case 103: // "Kedge"
		break;
	case 104: // "Leech"
		break;
	case 105: // "Swordfish"
		break;
	case 106: // "Felguard"
		break;
	case 107: // "Mammoth"
		break;
	case 108: // "Eye of Zomm"
		break;
	case 109: // "Wasp"
		break;
	case 110: // "Mermaid"
		break;
	case 111: // "Harpie"
		break;
	case 112: // "Fayguard"
		break;
	case 113: // "Drixie"
		break;
	case 116: // "Sea Horse"
		break;
	case 117: // "Ghost Dwarf"
		break;
	case 118: // "Erudite Ghost"
		break;
	case 119: // "Sabertooth Cat"
		break;
	case 120: // "Wolf Elemental",
		break;
	case 121: // "Gorgon"
		break;
	case 122: // "Dragon Skeleton"
		break;
	case 123: // "Innoruuk"
		break;
	case 124: // "Unicorn"
		break;
	case 125: // "Pegasus"
		break;
	case 126: // "Djinn"
		break;
	case 127: // "Invisible Man"
		break;
	case 128: // "Iksar"
		// playable races have fixed mods
		mysize = 5.0f;
		myradius = 5.0f;
		break;
	case 129: // "Scorpion"
		break;
	case 130: // "Vah Shir"
		// playable races have fixed mods
		mysize = 5.0f;
		myradius = 5.0f;
		break;
	case 131: // "Sarnak"
		break;
	case 133: // "Lycanthrope"
		break;
	case 134: // "Mosquito"
		break;
	case 135: // "Rhino"
		break;
	case 136: // "Xalgoz"
		break;
	case 137: // "Kunark Goblin"
		break;
	case 138: // "Yeti"
		break;
	case 139: // "Iksar Citizen"
		break;
	case 140: // "Forest Giant"
		break;
	case 144: // "Burynai"
		break;
	case 145: // "Goo"
		break;
	case 146: // "Spectral Sarnak"
		break;
	case 147: // "Spectral Iksar"
		break;
	case 148: // "Kunark Fish"
		break;
	case 149: // "Iksar Scorpion"
		break;
	case 150: // "Erollisi"
		break;
	case 151: // "Tribunal"
		break;
	case 153: // "Bristlebane"
		break;
	case 154: // "Fay Drake"
		break;
	case 155: // "Sarnak Skeleton"
		break;
	case 156: // "Ratman"
		break;
	case 157: // "Wyvern"
		break;
	case 158: // "Wurm"
		mysize = 16.0f;
		break;
	case 159: // "Devourer"
		break;
	case 160: // "Iksar Golem"
		break;
	case 161: // "Iksar Skeleton"
		break;
	case 162: // "Man Eating Plant"
		break;
	case 163: // "Raptor"
		break;
	case 164: // "Sarnak Golem"
		break;
	case 165: // "Water Dragon"
		break;
	case 166: // "Iksar Hand"
		break;
	case 167: // "Succulent"
		break;
	case 168: // "Flying Monkey"
		break;
	case 169: // "Brontotherium"
		break;
	case 170: // "Snow Dervish"
		break;
	case 171: // "Dire Wolf"
		break;
	case 172: // "Manticore"
		break;
	case 173: // "Totem"
		break;
	case 174: // "Cold Spectre"
		break;
	case 175: // "Enchanted Armor"
		break;
	case 176: // "Snow Bunny"
		break;
	case 177: // "Walrus"
		break;
	case 178: // "Rock-gem Men"
		break;
	case 181: // "Yak Man"
		break;
	case 183: // "Coldain"
		break;
	case 184: // "Velious Dragons"
		myradius = 9.48f;
		break;
	case 185: // "Hag"
		break;
	case 187: // "Siren"
		break;
	case 188: // "Frost Giant"
		break;
	case 189: // "Storm Giant"
		break;
	case 190: // "Ottermen"
		break;
	case 191: // "Walrus Man"
		break;
	case 192: // "Clockwork Dragon"
		myradius = 10.48f;
		break;
	case 193: // "Abhorent"
		break;
	case 194: // "Sea Turtle"
		break;
	case 195: // "Black and White Dragons"
		myradius = 9.48f;
		break;
	case 196: // "Ghost Dragon"
		myradius = 9.48f;
		break;
	case 198: // "Prismatic Dragon"
		myradius = 9.48f;
		break;
	case 199: // "ShikNar"
		break;
	case 200: // "Rockhopper"
		break;
	case 201: // "Underbulk"
		break;
	case 202: // "Grimling"
		break;
	case 203: // "Vacuum Worm"
		break;
	case 205: // "Kahli Shah"
		break;
	case 206: // "Owlbear"
		break;
	case 207: // "Rhino Beetle"
		break;
	case 208: // "Vampyre"
		break;
	case 209: // "Earth Elemental"
		break;
	case 210: // "Air Elemental"
		break;
	case 211: // "Water Elemental"
		break;
	case 212: // "Fire Elemental"
		break;
	case 213: // "Wetfang Minnow"
		break;
	case 214: // "Thought Horror"
		break;
	case 215: // "Tegi"
		break;
	case 216: // "Horse"
		break;
	case 217: // "Shissar"
		break;
	case 218: // "Fungal Fiend"
		break;
	case 219: // "Vampire Volatalis"
		break;
	case 220: // "StoneGrabber"
		break;
	case 221: // "Scarlet Cheetah"
		break;
	case 222: // "Zelniak"
		break;
	case 223: // "Lightcrawler"
		break;
	case 224: // "Shade"
		break;
	case 225: // "Sunflower"
		break;
	case 226: // "Sun Revenant"
		break;
	case 227: // "Shrieker"
		break;
	case 228: // "Galorian"
		break;
	case 229: // "Netherbian"
		break;
	case 230: // "Akheva"
		break;
	case 231: // "Spire Spirit"
		break;
	case 232: // "Sonic Wolf"
		break;
	case 234: // "Vah Shir Skeleton"
		break;
	case 235: // "Mutant Humanoid"
		break;
	case 236: // "Seru"
		break;
	case 237: // "Recuso"
		break;
	case 238: // "Vah Shir King"
		break;
	case 239: // "Vah Shir Guard"
		break;
	case 241: // "Lujein",
	case 242: // "Naiad",
	case 243: // "Nymph",
	case 244: // "Ent",
	case 245: // "Fly Man",
	case 246: // "Tarew Marr"
		break;
	case 247: // "Sol Ro"
		break;
	case 248: // "Clockwork Golem"
		break;
	case 249: // "Clockwork Brain",
	case 250: // "Spectral Banshee",
	case 251: // "Guard of Justice",
	case 252: // 'PoM Castle',
	case 253: // "Disease Boss"
	case 254: // "Sol Ro Guard"
	case 255: // "New Bertox",
	case 256: // "New Tribunal",
	case 257: // "Terris Thule",
	case 258: // "Vegerog",
	case 259: // "Crocodile",
	case 260: // "Bat",
	case 261: // "Slarghilug",
	case 262: // "Tranquilion"
	case 263: // "Tin Soldier"
	case 264: // "Nightmare Wraith",
	case 265: // "Malarian",
	case 266: // "Knight of Pestilence",
	case 267: // "Lepertoloth",
	case 268: // "Bubonian Boss",
	case 269: // "Bubonian Underling",
	case 270: // "Pusling",
	case 271: // "Water Mephit",
	case 272: // "Stormrider",
	case 273: // "Junk Beast"
		break;
	case 274: // "Broken Clockwork"
		break;
	case 275: // "Giant Clockwork",
	case 276: // "Clockwork Beetle",
	case 277: // "Nightmare Goblin",
	case 278: // "Karana",
	case 279: // "Blood Raven",
	case 280: // "Nightmare Gargoyle",
	case 281: // "Mouths of Insanity",
	case 282: // "Skeletal Horse",
	case 283: // "Saryn",
	case 284: // "Fennin Ro",
	case 285: // "Tormentor",
	case 286: // "Necro Priest",
	case 287: // "Nightmare",
	case 288: // "New Rallos Zek",
	case 289: // "Vallon Zek",
	case 290: // "Tallon Zek",
	case 291: // "Air Mephit",
	case 292: // "Earth Mephit",
	case 293: // "Fire Mephit",
	case 294: // "Nightmare Mephit",
	case 295: // "Zebuxoruk",
	case 296: // "Mithaniel Marr",
	case 297: // "Undead Knight",
	case 298: // "The Rathe",
	case 299: // "Xegony",
	case 300: // "Fiend",
	case 301: // "Test Object",
	case 302: // "Crab",
	case 303: // "Phoenix",
	case 304: // "PoP Dragon",
	case 305: // "PoP Bear",
	case 306: // "Storm Taarid",
	case 307: // "Storm Satuur",
	case 308: // "Storm Kuraaln",
	case 309: // "Storm Volaas",
	case 310: // "Storm Mana",
	case 311: // "Storm Fire",
	case 312: // "Storm Celestial",
	case 313: // "War Wraith",
	case 314: // "Wrulon",
	case 315: // "Kraken",
	case 316: // "Poison Frog",
	case 317: // "Queztocoatal",
	case 318: // "Valorian",
	case 319: // "War Boar",
	case 320: // "PoP Efreeti",
	case 321: // "War Boar Unarmored",
	case 322: // "Black Knight",
	case 323: // "Animated Armor",
	case 324: // "Undead Footman",
	case 325: // "Rallos Zek Minion"
	case 326: // "Arachnid"
	case 327: // "Crystal Spider",
	case 328: // "Zeb Cage",
	case 329: // "BoT Portal",
	case 330: // "Froglok",
	case 331: // "Troll Buccaneer",
	case 332: // "Troll Freebooter",
	case 333: // "Troll Sea Rover",
	case 334: // "Spectre Pirate Boss",
	case 335: // "Pirate Boss",
	case 336: // "Pirate Dark Shaman",
	case 337: // "Pirate Officer",
	case 338: // "Gnome Pirate",
	case 339: // "Dark Elf Pirate",
	case 340: // "Ogre Pirate",
	case 341: // "Human Pirate",
	case 342: // "Erudite Pirate",
	case 343: // "Poison Dart Frog",
	case 344: // "Troll Zombie",
	case 345: // "Luggald Land",
	case 346: // "Luggald Armored",
	case 347: // "Luggald Robed",
	case 348: // "Froglok Mount",
	case 349: // "Froglok Skeleton",
	case 350: // "Undead Froglok",
	case 351: // "Chosen Warrior",
	case 352: // "Chosen Wizard",
	case 353: // "Veksar",
	case 354: // "Greater Veksar",
	case 355: // "Veksar Boss",
	case 356: // "Chokadai",
	case 357: // "Undead Chokadai",
	case 358: // "Undead Veksar",
	case 359: // "Vampire Lesser",
	case 360: // "Vampire Elite",	
		break;
	default:
		myradius = 6.0;
		break;

	}
	myradius *= mysize;
	myradius /= base_size;
	return myradius;
}

int32 Mob::GetSkillStat(EQ::skills::SkillType skillid)
{

	if(EQ::skills::IsSpellSkill(skillid))
	{
		if(GetCasterClass() == 'I')
		{
			return (GetINT() <= 190) ? GetINT() : 190;
		}
		else
		{
			return (GetWIS() <= 190) ? GetWIS() : 190;
		}
	}

	int32 stat = GetINT();
	bool penalty = true;
	switch (skillid) 
	{
		case EQ::skills::Skill1HBlunt:
		case EQ::skills::Skill1HSlashing:
		case EQ::skills::Skill2HBlunt:
		case EQ::skills::Skill2HSlashing:
		case EQ::skills::SkillArchery:
		case EQ::skills::Skill1HPiercing:
		case EQ::skills::SkillHandtoHand:
		case EQ::skills::SkillThrowing:
		case EQ::skills::SkillApplyPoison:
		case EQ::skills::SkillDisarmTraps:
		case EQ::skills::SkillPickLock:
		case EQ::skills::SkillPickPockets:
		case EQ::skills::SkillSenseTraps:
		case EQ::skills::SkillSafeFall:
		case EQ::skills::SkillHide:
		case EQ::skills::SkillSneak:
			stat = GetDEX();
			penalty = false;
			break;
		case EQ::skills::SkillBackstab:
		case EQ::skills::SkillBash:
		case EQ::skills::SkillDisarm:
		case EQ::skills::SkillDoubleAttack:
		case EQ::skills::SkillDragonPunch:
		case EQ::skills::SkillDualWield:
		case EQ::skills::SkillEagleStrike:
		case EQ::skills::SkillFlyingKick:
		case EQ::skills::SkillKick:
		case EQ::skills::SkillOffense:
		case EQ::skills::SkillRoundKick:
		case EQ::skills::SkillTigerClaw:
		case EQ::skills::SkillTaunt:
		case EQ::skills::SkillIntimidation:
			stat = GetSTR();
			penalty = false;
			break;
		case EQ::skills::SkillBlock:
		case EQ::skills::SkillDefense:
		case EQ::skills::SkillDodge:
		case EQ::skills::SkillParry:
		case EQ::skills::SkillRiposte:
			stat = GetAGI();
			penalty = true;
			break;
		case EQ::skills::SkillBegging:
			stat = GetCHA();
			penalty = false;
			break;
		case EQ::skills::SkillSwimming:
			stat = GetSTA();
			penalty = true;
			break;
		default:
			penalty = true;
			break;
	}

	int16 higher_from_int_wis = (GetINT() > GetWIS()) ? GetINT() : GetWIS();
	stat = (higher_from_int_wis > stat) ? higher_from_int_wis : stat;

	if(penalty)
		stat -= 15;

	return (stat <= 190) ? stat : 190;
}

bool Mob::IsPlayableRace(uint16 race)
{
	if(race > 0 && (race <= GNOME || race == IKSAR || race == VAHSHIR))
	{
		return true;
	}

	return false;
}

float Mob::GetPlayerHeight(uint16 race)
{
	float ret_size = 6.0f;

	switch (race)
	{
	case Race::Barbarian:
	case Race::HalasCitizen:
	case Race::VahShir:
		ret_size = 7.0;
		break;
	case Race::WoodElf:
	case Race::DarkElf:
	case Race::NeriakCitizen:
	case Race::Fayguard:
		ret_size = 5.0;
		break;
	case Race::HalfElf:
		ret_size = 5.5;
		break;
	case Race::Dwarf:
	case Race::KaladimCitizen:
		ret_size = 4.0;
		break;
	case Race::Troll:
	case Race::GrobbCitizen:
		ret_size = 8.0;
		break;
	case Race::Ogre:
	case Race::OggokCitizen:
		ret_size = 9.0;
		break;
	case Race::Halfling:
	case Race::RivervaleCitizen:
		ret_size = 3.5;
		break;
	case Race::Gnome:
	case Race::ClockworkGnome:
		ret_size = 3.0;
		break;
	case Race::Wolf:
	case Race::WolfElemental:
		if (this->gender != 2)
			break;
		ret_size = 3.0;
		break;
	case Race::Bear:
		ret_size = 4.6999998;
		break;
	}

	return ret_size;
}

float Mob::GetHeadingRadians()
{
	float headingRadians = GetHeading();
	headingRadians = (headingRadians * 360.0f) / 256.0f;	// convert to degrees first; heading range is 0-255
	if (headingRadians < 270)
		headingRadians += 90;
	else
		headingRadians -= 270;
	
	return headingRadians * 3.141592654f / 180.0f;
}

bool Mob::IsFacingTarget()
{
	if (!target)
		return false;

	float heading = GetHeadingRadians();
	float headingUnitVx = cosf(heading);
	float headingUnitVy = sinf(heading);

	float toTargetVx = -GetTarget()->GetPosition().x - -m_Position.x;
	float toTargetVy = GetTarget()->GetPosition().y - m_Position.y;

	float distance = sqrtf(toTargetVx * toTargetVx + toTargetVy * toTargetVy);

	float dotp = headingUnitVx * (toTargetVx / distance) +
		headingUnitVy * (toTargetVy / distance);

	if (dotp > 0.95f)
		return true;

	return false;
}

bool Mob::CanCastBindAffinity()
{
	uint8 class_ = GetClass();
	uint8 level = GetLevel();

	if(level >= 12 && (class_ == Class::Necromancer || class_ == Class::Wizard || class_ == Class::Magician || class_ == Class::Enchanter))
	{
		return true;
	}
	else if(level >= 14 && (class_ == Class::Cleric || class_ == Class::Shaman || class_ == Class::Druid))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void Mob::FadeVoiceGraft()
{
	if(FindType(SE_VoiceGraft))
	{
		BuffFadeByEffect(SE_VoiceGraft);
	}
}

bool Mob::IsUnTargetable()
{
	if (GetBodyType() == BodyType::NoTarget || GetBodyType() == BodyType::NoTarget2 || GetBodyType() == BodyType::Special ||
		(GetBaseRace() == INVISIBLE_MAN && GetBodyType() == BodyType::InvisibleMan))
		return true;

	return false;
}

void Mob::ReportDmgTotals(Client* client, bool corpse, bool xp, bool faction, int32 dmg_amt)
{
	if (IsPlayerOwned())
		return;

	uint32 pet_damage = total_damage - (player_damage + npc_damage + gm_damage + ds_damage);
	client->Message(Chat::Yellow, "[GM Debug] %s damage report: TotalDmg: %d KillerGroupDmg: %d PlayerDmg: %d NPCDmg: %d PlayerPetDmg: %d DireCharmDmg: %d DSDmg: %d GMDmg: %d PBAoEDmg: %d SSFDmg: %d SSFDSDmg: %d", GetName(), total_damage, dmg_amt, player_damage, npc_damage, pet_damage, dire_pet_damage, ds_damage, gm_damage, pbaoe_damage, ssf_player_damage, ssf_ds_damage);

	if (corpse || xp || faction)
	{
		client->Message(Chat::Yellow, "[GM Debug] %s %s leave a corpse, %s give XP to players and %s give faction hits.", GetName(), corpse ? "will" : "will not", xp ? "should" : "will not", faction ? "may" : "will not");
	}
}

bool Mob::IsPlayerOwned()
{
	if (IsZomm())
		return false;

	if ((HasOwner() && GetUltimateOwner()->IsClient()) || // Is a client controlled pet.
		(IsNPC() && CastToNPC()->GetSwarmInfo() && CastToNPC()->GetSwarmInfo()->GetOwner() && CastToNPC()->GetSwarmInfo()->GetOwner()->IsClient())) // Is a client controlled swarm pet.
		return true;

	return false;
}

void Mob::PurgePoison(Client* caster)
{
	for (int i = 0; i < EFFECT_COUNT; ++i)
	{
		int effect_value = 99;
		int buff_count = GetMaxTotalSlots();
		for (int j = 0; j < buff_count; ++j)
		{
			if (!IsValidSpell(buffs[j].spellid))
				continue;
			if (CalculatePoisonCounters(buffs[j].spellid) == 0)
				continue;
			if (effect_value >= static_cast<int>(buffs[j].counters))
			{
				if (caster)
					caster->Message_StringID(Chat::Spells, TARGET_CURED);
				effect_value -= buffs[j].counters;
				buffs[j].counters = 0;
				BuffFadeBySlot(j);
			}
			else
			{
				buffs[j].counters -= effect_value;
				effect_value = 0;
				break;
			}
		}
	}
}

void Mob::ApplyIllusion(const SPDat_Spell_Struct &spell, int i, Mob* caster)
{
	uint16 spell_id = spell.id;
	uint16 spell_base = spell.base[i];
	float spell_model_size = -1.0f;

	// Gender Illusions
	if (spell_base == -1)
	{
		int specific_gender = -1;
		// Male
		if (spell_id == 1732)
			specific_gender = Gender::Male;
		// Female
		else if (spell_id == 1731)
			specific_gender = Gender::Female;
		// Switch
		else if (spell_id == 1730)
		{
			if (GetGender() == Gender::Male)
				specific_gender = Gender::Female;
			else
				specific_gender = Gender::Male;
		}

		if (specific_gender > -1)
		{
			SendIllusionPacket
			(
				GetBaseRace(),
				specific_gender,
				GetTexture()
			);
		}
	}
	else // Racial Illusions
	{
		int8 gender = Mob::GetDefaultGender(spell_base, GetGender());
		// Texture doesn't seem to be in our spell data :I
		int8 texture = 0;
		int8 helmtexture = spell.max[i];
		if (IsRacialIllusion(spell_id))
		{
			if (RuleB(Quarm, UseFixedShowHelmBehavior) && IsClient() && CastToClient()->ShowHelm()) {
				helmtexture = 0xFF;
			}
			texture = GetTexture();
		}
		// Great Bear - Ogre is Grizzly texture 0.
		else if (spell_id == 1431)
		{
			if (GetBaseRace() == Race::Troll || GetBaseRace() == Race::Iksar)
				texture = 1;
			else if (GetBaseRace() == Race::Barbarian)
				texture = 2;
		}
		else
		{
			switch (spell_id)
			{
			// Fire Elemental
			case 598:
			case 2795:
			case 2796:
			case 2797:
			case 3856:
			//Hunter/Howler
			case 1562:
			case 1563:
			{
				texture = 1;
				break;
			}

			// Water Elemental
			case 599:
			case 2798:
			case 2799:
			case 2800:
			case 3857:
			// Great Wolf
			case 427:
			{
				texture = 2;
				break;
			}

			// Air Elemental
			case 597:
			case 2789:
			case 2790:
			case 2791:
			case 3854:
			// Greater Wolf
			case 426:
			{
				texture = 3;
				break;
			}

			// Scaled Wolf is a female wolf.
			case 3586:
			{
				gender = Gender::Female;
				break;
			}

			case 1416:
			{
				if (GetBaseRace() == GNOME)
					spell_model_size = 4.0f;
				break;
			}

            case 581:
			case 643:
            case 644:
			case 1376:
            case 1611:
            {
				if (GetBaseRace() == IKSAR && spell_base == SKELETON)
				{
					spell_base = IKSAR_SKELETON;
					helmtexture = GetGender();
				}

				if (GetBaseRace() == GNOME && spell_id != 581)
					spell_model_size = 4.0f;

				break;
            }

			}
		}

		SendIllusionPacket
		(
			spell_base,
			gender,
			texture,
			helmtexture, // seems to be 0 for every illusion
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			spell_model_size // default size
		);

		this->z_offset = CalcZOffset();
		this->head_offset = CalcHeadOffset();
		this->model_size = CalcModelSize();
		this->model_bounding_radius = CalcBoundingRadius();
	}
}

void Mob::EndShield()
{
	if (GetShieldTarget())
	{
		entity_list.MessageClose_StringID(this, false, 100, 0, END_SHIELDING, GetCleanName(), GetShieldTarget()->GetCleanName());
		GetShieldTarget()->SetShielder(nullptr);
		SetShieldTarget(nullptr);
		shield_timer.Disable();
	}
	if (GetShielder())
	{
		GetShielder()->EndShield();
	}
}

void Mob::TryShielderDamage(Mob* attacker, int& damage, EQ::skills::SkillType skill)
{
	if (GetShielder() && !GetShielder()->GetInvul())
	{
		if (!GetShielder()->IsCorpse() && !GetShielder()->HasDied() && GetShielder()->GetHP() > 0)
		{
			damage /= 2;
			if (damage < 0)
				damage = 1;
			int shielderDamage = damage * 15 / 10;

			if (GetShielder()->IsNPC())
				shielderDamage = damage;
			else if (GetShielder()->IsClient())
			{
				EQ::ItemInstance* offhandInst = GetShielder()->CastToClient()->GetInv().GetItem(EQ::invslot::slotSecondary);
				const EQ::ItemData* offhandStruct = offhandInst ? offhandInst->GetItem() : nullptr;

				if (offhandStruct && offhandStruct->ItemType == EQ::item::ItemTypeShield)
				{
					int reduction = offhandStruct->AC;
					if (reduction > 50)
						reduction = 50;

					shielderDamage = damage * (150 - reduction) / 100;
				}
			}

			GetShielder()->Damage(attacker, shielderDamage, SPELL_UNKNOWN, skill, false);
		}
		else if (GetShielder()->GetShieldTarget())
			GetShielder()->EndShield();
	}
}

void Mob::StartShield(Mob* mob)
{
	if (!mob || mob->IsCorpse())
		return;

	entity_list.MessageClose_StringID(this, false, 100, 0, START_SHIELDING, mob->GetCleanName(), GetCleanName());

	SetShieldTarget(mob);
	mob->SetShielder(this);

	uint32 time = 12000;
	if (IsNPC())
		time = 48000;
	else
	{
		switch (GetAA(aaLivingShield))
		{
		case 1:
			time += 12000;
			break;
		case 2:
			time += 24000;
			break;
		case 3:
			time += 36000;
			break;
		}
	}

	shield_timer.Start(time);
	if (IsNPC())
		shield_cooldown = Timer::GetCurrentTime() + 180000;
	else if (IsClient())
		CastToClient()->p_timers.Start(pTimerShield, 180);
}

// this will permanently randomize face, hair, eye color.  this is based on a copy of the #randomfeatures command and may not have the ranges entirely correct
void Mob::SetRandomFeatures()
{
	if (GetRace() <= Race::Gnome || GetRace() == Race::Iksar || GetRace() == Race::VahShir)
	{
		eyecolor1 = zone->random.Int(0, 9);
		eyecolor2 = zone->random.Int(0, 9);
		luclinface = zone->random.Int(0, 7);

		switch (GetRace())
		{
		case Race::Human:	// Human
			haircolor = zone->random.Int(0, 19);
			if (GetGender() == Gender::Male) {
				beardcolor = haircolor;
				hairstyle = zone->random.Int(0, 3);
				beard = zone->random.Int(0, 5);
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::Barbarian:	// Barbarian
			haircolor = zone->random.Int(0, 19);
			if (GetGender() == Gender::Male) {
				beardcolor = haircolor;
				hairstyle = zone->random.Int(0, 3);
				beard = zone->random.Int(0, 5);
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::Erudite: // Erudite
			if (GetGender() == Gender::Male) {
				beardcolor = zone->random.Int(0, 19);
				beard = zone->random.Int(0, 5);
			}
			break;
		case Race::WoodElf: // WoodElf
			haircolor = zone->random.Int(0, 19);
			if (GetGender() == Gender::Male) {
				hairstyle = zone->random.Int(0, 3);
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::HighElf: // HighElf
			haircolor = zone->random.Int(0, 14);
			if (GetGender() == Gender::Male) {
				hairstyle = zone->random.Int(0, 3);
				beardcolor = haircolor;
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::DarkElf: // DarkElf
			haircolor = zone->random.Int(13, 18);
			if (GetGender() == Gender::Male) {
				hairstyle = zone->random.Int(0, 3);
				beardcolor = haircolor;
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::HalfElf: // HalfElf
			haircolor = zone->random.Int(0, 19);
			if (GetGender() == Gender::Male) {
				hairstyle = zone->random.Int(0, 3);
				beardcolor = haircolor;
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::Dwarf: // Dwarf
			haircolor = zone->random.Int(0, 19);
			beardcolor = haircolor;
			if (GetGender() == Gender::Male) {
				hairstyle = zone->random.Int(0, 3);
				beard = zone->random.Int(0, 5);
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case  Race::Troll: // Troll
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 3);
				haircolor = zone->random.Int(0, 23);
			}
			break;
		case Race::Ogre: // Ogre
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 3);
				haircolor = zone->random.Int(0, 23);
			}
			break;
		case Race::Halfling: // Halfling
			haircolor = zone->random.Int(0, 19);
			if (GetGender() == Gender::Male) {
				beardcolor = haircolor;
				hairstyle = zone->random.Int(0, 3);
				beard = zone->random.Int(0, 5);
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::Gnome: // Gnome
			haircolor = zone->random.Int(0, 24);
			if (GetGender() == Gender::Male) {
				beardcolor = haircolor;
				hairstyle = zone->random.Int(0, 3);
				beard = zone->random.Int(0, 5);
			}
			if (GetGender() == Gender::Female) {
				hairstyle = zone->random.Int(0, 2);
			}
			break;
		case Race::Iksar: // Iksar
		case Race::VahShir: // VahShir
			break;
		default:
			break;
		}
	}
}

void Mob::SetHP(int32 hp)
{
	if (hp >= max_hp)
		cur_hp = max_hp; 
	else 
		cur_hp = hp;
}

void Mob::AddAllClientsToEngagementRecords()
{
	if (!IsNPC())
		return;

	if (IsPet())
		return;

	if (npctype_id == 0)
		return;

	auto clientList = entity_list.GetClientList();

	for (auto client : clientList)
	{
		if (client.second)
		{
			if (m_EngagedClientNames.find(client.second->GetCleanName()) == m_EngagedClientNames.end())
			{
				PlayerEngagementRecord record = PlayerEngagementRecord();
				record.isFlagged = false;
				record.lockout = LootLockout();
				record.character_id = client.second->CharacterID();
				strncpy(record.character_name, client.second->CastToClient()->GetCleanName(), 64);
				record.character_level = client.second->CastToClient()->GetLevel();
				record.character_level2 = client.second->CastToClient()->GetLevel2();
				record.character_ruleset = client.second->CastToClient()->GetRuleSet();

				auto lootLockoutItr = client.second->loot_lockouts.find(GetNPCTypeID());
				if (lootLockoutItr != client.second->loot_lockouts.end())
				{
					memcpy(&record.lockout, &lootLockoutItr->second, sizeof(LootLockout));
				}
				m_EngagedClientNames.emplace(client.second->GetCleanName(), record);
			}
		}
	}
}

void Mob::DeleteBucket(std::string bucket_name) {
	std::string full_bucket_name = fmt::format("{}-{}", GetBucketKey(), bucket_name);
	DataBucket::DeleteData(full_bucket_name);
}

std::string Mob::GetBucket(std::string bucket_name) {
	std::string full_bucket_name = fmt::format("{}-{}", GetBucketKey(), bucket_name);
	std::string bucket_value = DataBucket::GetData(full_bucket_name);
	if (!bucket_value.empty()) {
		return bucket_value;
	}
	return std::string();
}

std::string Mob::GetBucketExpires(std::string bucket_name) {
	std::string full_bucket_name = fmt::format("{}-{}", GetBucketKey(), bucket_name);
	std::string bucket_expiration = DataBucket::GetDataExpires(full_bucket_name);
	if (!bucket_expiration.empty()) {
		return bucket_expiration;
	}
	return std::string();
}

std::string Mob::GetBucketKey() {
	if (IsClient()) {
		return fmt::format("character-{}", CastToClient()->CharacterID());
	}
	else if (IsNPC()) {
		return fmt::format("npc-{}", GetNPCTypeID());
	}
	return std::string();
}

std::string Mob::GetBucketRemaining(std::string bucket_name) {
	std::string full_bucket_name = fmt::format("{}-{}", GetBucketKey(), bucket_name);
	std::string bucket_remaining = DataBucket::GetDataRemaining(full_bucket_name);
	if (!bucket_remaining.empty() && atoi(bucket_remaining.c_str()) > 0) {
		return bucket_remaining;
	}
	else if (atoi(bucket_remaining.c_str()) == 0) {
		return "0";
	}
	return std::string();
}

void Mob::SetBucket(std::string bucket_name, std::string bucket_value, std::string expiration) {
	std::string full_bucket_name = fmt::format("{}-{}", GetBucketKey(), bucket_name);
	DataBucket::SetData(full_bucket_name, bucket_value, expiration);
}
