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
#ifndef MOB_H
#define MOB_H

#include "common.h"
#include "entity.h"
#include "hate_list.h"
#include "mob_movement_manager.h"
#include "pathfinder_interface.h"
#include "position.h"
#include "water_map.h"
#include "aa.h"
#include "../common/light_source.h"
#include "../common/emu_constants.h"

#include <any>
#include <set>
#include <vector>
#include <memory>

#define INVIS_OFF 0
#define INVIS_NORMAL 1
#define INVIS_VSANIMAL 2
#define INVIS_VSUNDEAD 3
#define INVIS_HIDDEN 1
#define INVIS_IMPHIDDEN 1

char* strn0cpy(char* dest, const char* source, uint32 size);

class Client;
class EQApplicationPacket;
class Group;
class NPC;
class Raid;

struct NewSpawn_Struct;
struct SpawnPositionUpdate_Struct;
const int COLLISION_BOX_SIZE = 8;

namespace EQ
{
	class ItemInstance;
	struct ItemData;
}

class Mob : public Entity {
public:
	enum CLIENT_CONN_STATUS { CLIENT_CONNECTING, CLIENT_WAITING_FOR_AUTH, CLIENT_AUTH_RECEIVED, CLIENT_CONNECTED, CLIENT_LINKDEAD,
						CLIENT_KICKED, PREDISCONNECTED, ZONING, DISCONNECTED, CLIENT_ERROR, CLIENT_CONNECTINGALL };
	enum eStandingPetOrder { SPO_Follow, SPO_Sit, SPO_Guard };

	struct MobSpecialAbility {
		MobSpecialAbility() {
			level = 0;
			timer = nullptr;
			for(int i = 0; i < SpecialAbility::MaxParameters; ++i) {
				params[i] = 0;
			}
		}

		~MobSpecialAbility() {
			safe_delete(timer);
		}

		int level;
		Timer *timer;
		int params[SpecialAbility::MaxParameters];
	};

	Mob(const char*	in_name,
		const char*	in_lastname,
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
		uint8		in_see_invis, // see through invis
		uint8		in_see_invis_undead, // see through invis vs. undead
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
	);
	virtual ~Mob();

	inline virtual bool IsMob() const { return true; }
	inline virtual bool InZone() const { return true; }

	//Somewhat sorted: needs documenting!

	//Attack
	float MobAngle(Mob *other = 0, float ourx = 0.0f, float oury = 0.0f) const;
	// greater than 90 is behind
	inline bool BehindMob(Mob *other = 0, float ourx = 0.0f, float oury = 0.0f) const
		{ return (!other || other == this) ? true : MobAngle(other, ourx, oury) > 90.0f; }
	// less than 56 is in front, greater than 56 is usually where the client generates the messages
	inline bool InFrontMob(Mob *other = 0, float ourx = 0.0f, float oury = 0.0f) const
		{ return (!other || other == this) ? true : MobAngle(other, ourx, oury) < 56.0f; }
	bool IsFacingMob(Mob *other); // kind of does the same as InFrontMob, but derived from client
	float HeadingAngleToMob(Mob* other) { return HeadingAngleToMob(other->GetX(), other->GetY()); }
	float HeadingAngleToMob(float in_x, float in_y); // to keep consistent with client generated messages
	virtual void RangedAttack(Mob* other) { }
	virtual void ThrowingAttack(Mob* other) { }
	// 13 = Primary (default), 14 = secondary
	virtual bool Attack(Mob* other, int hand = EQ::invslot::slotPrimary, int damagePct = 100) = 0;
	void DoMainHandRound(Mob* victim = nullptr, int damagePct = 100);
	void DoOffHandRound(Mob* victim = nullptr, int damagePct = 100);
	virtual void DoBash(Mob* defender = nullptr);
	virtual void TryBashKickStun(Mob* defender, uint8 skill);
	virtual void DoKick(Mob* defender = nullptr);
	virtual void DoBackstab(Mob* defender = nullptr) {}
	int DoMonkSpecialAttack(Mob* other, uint8 skill_used, bool fromWus = false);
	int DoSpecialAttackDamage(Mob *defender, EQ::skills::SkillType skill, int base, int minDamage = 0, int hate = 0, DoAnimation animation_type = DoAnimation::None);
	virtual bool AvoidDamage(Mob* attacker, int32 &damage, bool isRangedAttack = false);
	virtual bool AvoidanceCheck(Mob* attacker, EQ::skills::SkillType skillinuse);
	virtual void TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, int32 minBase = 0, int32 damageBonus = 0);
	virtual bool TryFinishingBlow(Mob *defender, EQ::skills::SkillType skillinuse, uint32 dmgBonus = 0);
	virtual void DoRiposte(Mob* defender);
	static int RollD20(double offense, double mitigation);
	virtual int GetBaseDamage(Mob* defender = nullptr, int slot = EQ::invslot::slotPrimary) { return 1; }		// weapon damage for clients; DI*10 for NPCs
	virtual int GetDamageBonus() { return 0; }
	static int CalcEleWeaponResist(int weaponDamage, int resistType, Mob *target);
	bool IsImmuneToMelee(Mob* attacker, int slot = EQ::invslot::slotPrimary);
	virtual bool CombatRange(Mob* other, float dist_squared = 0.0f, bool check_z = false, bool pseudo_pos = false) { return false; }
	bool IsInCombatRange(Mob* other, float dist_squared = 0.0f); // This is a set of rules used by AI to determine if `other` is in range to determine closest hate and target. 
	virtual inline bool IsBerserk() { return false; } // only clients
	void RogueEvade(Mob *other);
	void CommonBreakInvisible(bool skip_sneakhide = false);
	void CommonBreakSneakHide(bool bug_sneak = false, bool skip_sneak = false);
	void CommonBreakInvisNoSneak() { CommonBreakInvisible(true); CommonBreakSneakHide(false, true); } // This breaks any form of Invis and hide.
	bool HasDied();
	void GenerateDamagePackets(Mob* attacker, bool FromDamageShield, int32 damage, uint16 spell_id, uint8 skill_id, bool command = false);
	void GenerateDeathPackets(Mob* killerMob, int32 damage, uint16 spell, uint8 attack_skill, bool bufftic = false, uint8 killedby = 0);

	//Appearance
	virtual void SendWearChange(uint8 material_slot, Client* sendto = nullptr, bool skip_if_zero = false, bool update_textures = false, bool illusioned = false);
	virtual void WearChange(uint8 material_slot, uint16 texture, uint32 color, Client* sendto = nullptr);
	void DoAnim(DoAnimation animnum, int type = 0, bool ackreq = false, eqFilterType filter = FilterNone);
	void ProjectileAnimation(Mob* to, int item_id, bool IsItem = false, float speed = 0,
		float angle = 0, float tilt = 0, float arc = 0, EQ::skills::SkillType skillInUse = EQ::skills::Skill1HBlunt);
	void ChangeSize(float in_size, bool bNoRestriction = false);
	inline uint8 SeeInvisible() const 
	{
		uint8 value = see_invis;
		if (IsClient()) 
		{
			value += aabonuses.SeeInvis; 
			value += itembonuses.SeeInvis; 
		} 
		return value; 
	}
	inline bool SeeInvisibleUndead() const { return see_invis_undead; }
	inline bool SeeSneak() const { return see_sneak; }
	inline bool SeeImprovedHide() const { return see_improved_hide; }
	inline bool GetSeeInvisible(uint8 see_invis);
	bool IsInvisible(Mob* other = 0) const;
	void SetInvisible(uint8 state, bool showInvis = true, bool skipSelf = false);
	bool AttackAnimation(EQ::skills::SkillType &skillinuse, int Hand, const EQ::ItemInstance* weapon);

	//Song
	bool UseBardSpellLogic(uint16 spell_id = 0xffff, int slot = -1);
	bool ApplyNextBardPulse(uint16 spell_id, Mob *spell_target, EQ::spells::CastingSlot slot);

	//Spell
	bool IsBeneficialAllowed(Mob *target);
	virtual int GetCasterLevel(uint16 spell_id);
	void ApplySpellsBonuses(uint16 spell_id, uint8 casterlevel, StatBonuses* newbon, uint16 casterID = 0,
		bool item_bonus = false, int16 instrumentmod = 10, uint32 ticsremaining = 0, int buffslot = -1,
		bool IsAISpellEffect = false, uint16 effect_id = 0, int32 se_base = 0, int32 se_limit = 0, int32 se_max = 0,
		bool is_tap_recourse = false);
	virtual float GetActSpellRange(uint16 spell_id, float range, std::string& item_name) { return range;}
	virtual float GetSpellRange(uint16 spell_id, float range) { return range; }
	virtual int32 GetActSpellDamage(uint16 spell_id, int32 value, Mob* target = nullptr) { return value; }
	virtual int32 GetActSpellHealing(uint16 spell_id, int32 value, Mob* target = nullptr, bool hot = false) { return value; }
	virtual int32 GetActSpellCost(uint16 spell_id, int32 cost){ return cost;}
	virtual int32 GetActSpellDuration(uint16 spell_id, int32 duration){ return duration;}
	virtual int32 GetActSpellCasttime(uint16 spell_id, int32 casttime);
	float CheckResistSpell(uint8 resist_type, uint16 spell_id, Mob *caster, Mob *target = nullptr, bool use_resist_override = false,
		int resist_override = 0, bool tick_save = false);
	int ResistPhysical(int level_diff, uint8 caster_level);
	uint16 GetSpecializeSkillValue(uint16 spell_id);
	void SendSpellBarEnable(uint16 spellid);
	void ZeroCastingVars();
	virtual void SpellProcess();
	virtual bool CastSpell(uint16 spell_id, uint16 target_id, EQ::spells::CastingSlot slot = EQ::spells::CastingSlot::Item, int32 casttime = -1,
		int32 mana_cost = -1, uint32* oSpellWillFinish = 0, uint32 item_slot = 0xFFFFFFFF,
		uint32 timer = 0xFFFFFFFF, uint32 timer_duration = 0, uint32 type = 0, int16 *resist_adjust = nullptr);
	virtual bool DoCastSpell(uint16 spell_id, uint16 target_id, EQ::spells::CastingSlot slot = EQ::spells::CastingSlot::Item, int32 casttime = -1,
		int32 mana_cost = -1, uint32* oSpellWillFinish = 0, uint32 item_slot = 0xFFFFFFFF,
		uint32 timer = 0xFFFFFFFF, uint32 timer_duration = 0, uint32 type = 0, int16 resist_adjust = 0);
	void CastedSpellFinished(uint16 spell_id, uint32 target_id, EQ::spells::CastingSlot slot, uint16 mana_used,
		uint32 inventory_slot = 0xFFFFFFFF, int16 resist_adjust = 0);
	bool SpellFinished(uint16 spell_id, Mob *target, EQ::spells::CastingSlot slot = EQ::spells::CastingSlot::Item, uint16 mana_used = 0,
		uint32 inventory_slot = 0xFFFFFFFF, int16 resist_adjust = 0, bool isproc = false, bool isrecourse=false, int recourse_level=-1);
	virtual bool SpellOnTarget(uint16 spell_id, Mob* spelltar, bool reflect = false,
		bool use_resist_adjust = false, int16 resist_adjust = 0, bool isproc = false, uint16 ae_caster_id = 0, bool isrecourse=false, int spell_level=-1);
	virtual bool SpellEffect(Mob* caster, uint16 spell_id, int buffslot, int caster_level, float partial = 100);
	virtual bool DetermineSpellTargets(uint16 spell_id, Mob *&spell_target, Mob *&ae_center,
		CastAction_type &CastAction, bool isproc = false, EQ::spells::CastingSlot slot = EQ::spells::CastingSlot::Item);
	virtual bool CheckFizzle(uint16 spell_id);
	virtual bool CheckSpellLevelRestriction(uint16 spell_id, Mob* caster, EQ::spells::CastingSlot slot);
	virtual bool IsImmuneToSpell(uint16 spell_id, Mob *caster, bool isProc = false);
	virtual float GetAOERange(uint16 spell_id);
	bool DoCastingRangeCheck(uint16 spell_id, EQ::spells::CastingSlot slot, Mob* target = nullptr);
	void InterruptSpell(uint16 spellid = SPELL_UNKNOWN, bool fizzle = false);
	void InterruptSpell(uint16 message, uint16 color, uint16 spellid = SPELL_UNKNOWN, bool fizzle = false, bool message_others = true);
	inline bool IsCasting() const { return((casting_spell_id != 0)); }
	uint16 CastingSpellID() const { return casting_spell_id; }
	bool DoPreCastingChecks(uint16 spell_id, EQ::spells::CastingSlot slot, uint16 spell_targetid);
	bool TryDispel(uint8 caster_level, uint8 buff_level, int level_modifier);
	bool CancelMagicIsAllowedOnTarget(Mob* spelltar);
	bool CancelMagicShouldAggro(uint16 spell_id, Mob* spelltar);
	bool HasSongInstrument(uint16 spell_id);
	bool HasSpellReagent(uint16 spell_id);
	bool HasReagent(uint16 spell_id, int component, int component_count, bool missingreags);
	void ResistSpell(Mob* caster, uint16 spell_id, bool isproc);
	void TrySpinStunBreak();

	//Buff
	void BuffProcess();
	virtual void DoBuffTic(uint16 spell_id, int slot, uint32 ticsremaining, uint8 caster_level, Mob* caster = 0, int instrumentmod = 10);
	void BuffFadeBySpellID(uint16 spell_id, bool message = true);
	void BuffFadeByEffect(int effectid, int skipslot = -1);
	void BuffFadeAll(bool skiprez = false, bool message = false);
	void BuffFadeNonPersistDeath(bool skiprez = false);
	void BuffFadeDetrimental();
	void BuffFadeBySlot(int slot, bool iRecalcBonuses = true, bool message = true, bool update = true);
	void BuffFadeDetrimentalByCaster(Mob *caster);
	void BuffFadeDotsByCaster(Mob *caster);
	void BuffFadeDetrimentalByNPCs(int16 slot_skipped);
	void BuffFadeBySitModifier();
	void BuffModifyDurationBySpellID(uint16 spell_id, int32 newDuration, bool update);
	int CanBuffStack(uint16 spellid, uint8 caster_level, bool iFailIfOverwrite = false);
	int CalcBuffDuration(Mob *caster, Mob *target, uint16 spell_id, int32 caster_level_override = -1);
	virtual int GetCurBuffSlots() const { return 0; }
	virtual int GetMaxBuffSlots() const { return 0; }
	virtual int GetMaxTotalSlots() const { return 0; }
	virtual void InitializeBuffSlots() { buffs = nullptr; current_buff_count = 0; }
	virtual void UninitializeBuffSlots() { }
	inline Buffs_Struct* GetBuffs() { return buffs; }
	void DamageShield(Mob* other, bool spell_ds = false);
	int32 RuneAbsorb(int32 damage, uint16 type);
	bool FindBuff(uint16 spellid);
	bool FindType(uint16 type, bool bOffensive = false, uint16 threshold = 100);
	int16 GetBuffSlotFromType(uint16 type);
	uint16 GetSpellIDFromSlot(uint8 slot);
	int CountDispellableBuffs();
	bool HasMGB() const { return has_MGB; }
	inline void SetMGB(bool val) { has_MGB = val; }
	bool HasProjectIllusion() const { return has_ProjectIllusion ; }
	inline void SetProjectIllusion(bool val) { has_ProjectIllusion  = val; }
	bool IsBuffed();
	bool IsDebuffed();
	bool IsPacified();
	bool HasDoT();
	bool CanCastBindAffinity();
	bool CanClassCastSpell(uint16 spell_id) { if (!spells[spell_id].not_player_spell && spells[spell_id].classes[GetClass() - 1] < 255) return true; else return false; };
	int GetDamageShieldAmount() { return spellbonuses.DamageShield < 0 ? -(spellbonuses.DamageShield + itembonuses.DamageShield) : -spellbonuses.DamageShield; };

	bool IsStackBlocked(uint16 new_buff_spell_id);
	bool IsStackBlocked_PatchV1(uint16 new_buff_spell_id);
	void ProcessBuffOverwriteEffects(uint16 spell_id);
	int FindAffectSlot(Mob *caster, uint16 spell_id, int *result_slotnum, int remove_replaced);
	int FindAffectSlot_PatchV1(Mob* caster, uint16 spell_id, int* result_slotnum, int remove_replaced);
	bool AssignBuffSlot(Mob *caster, uint16 spell_id, int &buffslot, int &caster_level, EQApplicationPacket *action_packet);

	//Basic Stats/Inventory
	virtual void SetLevel(uint8 in_level, bool command = false) { level = in_level; }
	bool IsTargetable() const { return m_targetable; }
	inline void ShieldEquiped(bool val) { has_shieldequiped = val; }
	bool HasShieldEquiped() const { return has_shieldequiped; }
	bool HasBowEquipped() const { return has_bowequipped; }
	void SetBowEquipped(bool val) { has_bowequipped = val; }
	bool HasArrowEquipped() const { return has_arrowequipped; }
	void SetArrowEquipped(bool val) { has_arrowequipped = val; }
	bool HasBowAndArrowEquipped() const { return HasBowEquipped() && HasArrowEquipped(); }
	inline void SetBashEnablingWeapon(bool val) { has_bashEnablingWeapon = val; } //Used for SK/Pal epics
	bool HasBashEnablingWeapon() const { return has_bashEnablingWeapon; }
	virtual uint16 GetSkill(EQ::skills::SkillType skill_num) { return 0; }
	virtual void SetSkill(EQ::skills::SkillType skill_num, uint16 value) {};
	virtual uint32 GetEquipment(uint8 material_slot) const { return(0); }
	virtual int32 GetEquipmentMaterial(uint8 material_slot) const;
	virtual uint32 GetEquipmentColor(uint8 material_slot) const;
	bool AffectedBySpellExcludingSlot(int slot, int effect);
	virtual bool Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby = 0, bool bufftic = false) = 0;
	virtual void Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill,
		bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false) = 0;
	virtual void SetHP(int32 hp);
	void AddAllClientsToEngagementRecords();
	inline void SetOOCRegen(int32 newoocregen) {oocregen = newoocregen;}
	virtual void Heal();
	virtual void HealDamage(uint32 amount, Mob* caster = nullptr, uint16 spell_id = SPELL_UNKNOWN, bool hot = false);
	virtual void SetMaxHP() { cur_hp = max_hp; }
	virtual inline uint16 GetBaseRace() const { return base_race; }
	virtual inline uint8 GetBaseGender() const { return base_gender; }
	virtual inline uint16 GetDeity() const { return deity; }
	virtual uint32 GetDeityBit() { return Deity::GetBitmask(deity); }
	inline uint16 GetRace() const { return race; }
	virtual uint32 GetRaceStringID();
	virtual uint32 GetClassStringID();
	inline uint8 GetGender() const { return gender; }
	inline uint8 GetTexture() const { return texture; }
	inline uint8 GetHelmTexture() const { return helmtexture; }
	inline uint8 GetChestTexture() const { return chesttexture; }
	inline uint8 GetArmTexture() const { return armtexture; }
	inline uint8 GetBracerTexture() const { return bracertexture; }
	inline uint8 GetHandTexture() const { return handtexture; }
	inline uint8 GetLegTexture() const { return legtexture; }
	inline uint8 GetFeetTexture() const { return feettexture; }
	inline uint8 GetHairColor() const { return haircolor; }
	inline uint8 GetBeardColor() const { return beardcolor; }
	inline uint8 GetEyeColor1() const { return eyecolor1; }
	inline uint8 GetEyeColor2() const { return eyecolor2; }
	inline uint8 GetHairStyle() const { return hairstyle; }
	inline uint8 GetLuclinFace() const { return luclinface; }
	inline uint8 GetBeard() const { return beard; }
	void SetRandomFeatures();
	inline uint32 GetArmorTint(uint8 i) const { return armor_tint.Slot[(i < EQ::textures::materialCount) ? i : 0].Color; }
	inline uint8 GetClass() const { return class_; }
	inline uint8 GetLevel() const { return level; }
	inline uint8 GetOrigLevel() const { return orig_level; }
	inline const char* GetName() const { return name; }
	inline const char* GetOrigName() const { return orig_name; }
	inline const char* GetLastName() const { return lastname; }
	const char *GetCleanName();
	const char *GetCleanOwnerName();
	virtual void SetName(const char *new_name = nullptr) { new_name ? strn0cpy(name, new_name, 64) :
		strn0cpy(name, GetName(), 64); return; };
	inline Mob* GetTarget() const { return target; }
	std::string GetTargetDescription(Mob *target, uint8 description_type = TargetDescriptionType::LCSelf, uint16 entity_id_override = 0);
	virtual void SetTarget(Mob* mob);
	virtual inline float GetHPRatio() const { return max_hp == 0 ? 0.0f : ((float)cur_hp/max_hp*100.0f); }
	inline virtual int32 GetAC() const { return AC; }											// returns database AC value
	inline int32 GetATK() { return (GetToHitByHand() + GetOffenseByHand()) * 1000 / 744; }		// this is the value displayed in the client; not used in server calcs
	inline int32 GetATKBonusSpell() const { return spellbonuses.ATK; }
	inline int32 GetATKBonusItem() const { return itembonuses.ATK; }
	inline virtual int32 GetSTR() const { return STR + itembonuses.STR + spellbonuses.STR; }
	inline virtual int32 GetSTA() const { return STA + itembonuses.STA + spellbonuses.STA; }
	inline virtual int32 GetDEX() const { return DEX + itembonuses.DEX + spellbonuses.DEX; }
	inline virtual int32 GetAGI() const { return AGI + itembonuses.AGI + spellbonuses.AGI; }
	inline virtual int32 GetINT() const { return INT + itembonuses.INT + spellbonuses.INT; }
	inline virtual int32 GetWIS() const { return WIS + itembonuses.WIS + spellbonuses.WIS; }
	inline virtual int32 GetCHA() const { return CHA + itembonuses.CHA + spellbonuses.CHA; }
	inline virtual int32 GetMR() const { return MR + itembonuses.MR + spellbonuses.MR; }
	inline virtual int32 GetFR() const { return FR + itembonuses.FR + spellbonuses.FR; }
	inline virtual int32 GetDR() const { return DR + itembonuses.DR + spellbonuses.DR; }
	inline virtual int32 GetPR() const { return PR + itembonuses.PR + spellbonuses.PR; }
	inline virtual int32 GetCR() const { return CR + itembonuses.CR + spellbonuses.CR; }
	inline StatBonuses GetItemBonuses() const { return itembonuses; }
	inline StatBonuses GetSpellBonuses() const { return spellbonuses; }
	inline StatBonuses GetAABonuses() const { return aabonuses; }
	inline virtual int32 GetMaxSTR() const { return GetSTR(); }
	inline virtual int32 GetMaxSTA() const { return GetSTA(); }
	inline virtual int32 GetMaxDEX() const { return GetDEX(); }
	inline virtual int32 GetMaxAGI() const { return GetAGI(); }
	inline virtual int32 GetMaxINT() const { return GetINT(); }
	inline virtual int32 GetMaxWIS() const { return GetWIS(); }
	inline virtual int32 GetMaxCHA() const { return GetCHA(); }
	inline virtual int32 GetMaxMR() const { return 255; }
	inline virtual int32 GetMaxPR() const { return 255; }
	inline virtual int32 GetMaxDR() const { return 255; }
	inline virtual int32 GetMaxCR() const { return 255; }
	inline virtual int32 GetMaxFR() const { return 255; }
	inline int32 GetHP() const { return cur_hp; }
	inline int32 GetMaxHP() const { return max_hp; }
	virtual int32 CalcMaxHP(bool unbuffed = false);
	inline int32 GetMaxMana() const { return max_mana; }
	inline int32 GetMana() const { return cur_mana; }
	int32 GetItemHPBonuses();
	int32 GetSpellHPBonuses();
	virtual const int32& SetMana(int32 amount);
	inline float GetManaRatio() const { return max_mana == 0 ? 100 :
		((static_cast<float>(cur_mana) / max_mana) * 100); }
	virtual int32 CalcMaxMana();
	uint32 GetNPCTypeID() const { return npctype_id; }
	void SetNPCTypeID(uint32 npctypeid) { npctype_id = npctypeid; }
	inline const glm::vec4& GetPosition() const { return m_Position; }
	inline const float GetX() const { return m_Position.x; }
	inline const float GetY() const { return m_Position.y; }
	inline const float GetZ() const { return m_Position.z; }
	inline const float GetHeading() const { return m_Position.w; }
	inline const float GetDeltaX() const { return m_Delta.x; }
	inline const float GetDeltaY() const { return m_Delta.y; }
	inline const float GetDeltaHeading() const { return m_Delta.w; }
	inline const uint32 GetLastUpdate() const { return last_update; }
	float GetHeadingRadians();
	inline const float GetEQX() const { return m_EQPosition.x; }
	inline const float GetEQY() const { return m_EQPosition.y; }
	inline const float GetEQZ() const { return m_EQPosition.z; }
	inline const float GetEQHeading() const { return m_EQPosition.w; }
	inline const float GetSize() const { return size; }
	inline const float GetBaseSize() const { return base_size; }
	inline const float GetHeadOffset() const { return head_offset; }
	inline const float GetModelSize() const { return model_size; }
	inline const float GetBoundingRadius() const { return model_bounding_radius; }
	inline const float SetBestZ(float z_coord) const { return z_coord + z_offset; }
	inline const float SetProjectileZ(float z_coord) const { return z_coord + (size ? size * 0.8f : 4.8f); }
	inline const float GetTarX() const { return m_TargetLocation.x; }
	inline const float GetTarY() const { return m_TargetLocation.y; }
	inline const float GetTarZ() const { return m_TargetLocation.z; }
	inline const float GetTarVX() const { return m_TargetV.x; }
	inline const float GetTarVY() const { return m_TargetV.y; }
	inline const float GetTarVZ() const { return m_TargetV.z; }
	inline const float GetTarVector() const { return tar_vector; }
	inline const uint8 GetTarNDX() const { return tar_ndx; }
	inline const GravityBehavior GetFlyMode() const { return flymode; }
	bool IsBoat() const;

	//Group
	virtual bool HasRaid() = 0;
	virtual bool HasGroup() = 0;
	virtual Raid* GetRaid() = 0;
	virtual Group* GetGroup() = 0;

	//Faction
	virtual inline int32 GetPrimaryFaction() const { return 0; }

	//Movement
	void Warp(const glm::vec3& location);
	inline bool IsMoving() const { return moving; }
	virtual void SetMoving(bool move) { moving = move; if (!move) m_Delta = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); }
	virtual void GoToBind(uint8 bindnum = 0) { }
	virtual void Gate();
	float GetFearSpeed() const { return(0.025f * (float)_GetFearSpeed()); }
	float GetWalkspeed() const { return(0.025f * (float)_GetWalkSpeed()); }
	float GetRunspeed() const { return(0.025f * (float)_GetRunSpeed()); }
	int GetIntFearSpeed() const { return(_GetFearSpeed()); }
	int GetIntWalkspeed() const { return(_GetWalkSpeed()); }
	int GetIntRunspeed() const { return(_GetRunSpeed()); }
	float GetBaseRunspeed() const { return runspeed; }
	void  SetWalkSpeed(float speed) { int_walkspeed = (int)((float)speed * 40.0f); int_walkspeed = (int_walkspeed >> 2) << 2; walkspeed = (float)int_walkspeed / 40.0f; }
	void  SetRunSpeed(float speed) { int_runspeed = (int)((float)speed * 40.0f);  int_runspeed = (int_runspeed >> 2) << 2; runspeed = (float)int_runspeed / 40.0f; permarooted = (runspeed > 0.0f) ? false : true; }
	float GetMovespeed() const { return IsRunning() ? GetRunspeed() : GetWalkspeed(); } // Used by grids roamboxes, and roamers to determine how fast the NPC *should* move.
	bool IsRunning() const { return m_is_running; } 
	void SetRunning(bool val) { m_is_running = val; } // Toggle to force the NPC to run or walk on their next update.
	void SetCurrentSpeed(float speed);
	float GetCurrentSpeed() { return current_speed; }
	virtual void GMMove(float x, float y, float z, float heading = 0.01, bool SendUpdate = true, bool GuardReset = false);
	void SetDelta(const glm::vec4& delta) { m_Delta = delta; }
	void SetPosition(const glm::vec4& pos) { m_Position = pos; }
	void SetTargetDestSteps(uint8 target_steps) { tar_ndx = target_steps; }
	void SendPosUpdate(uint8 iSendToSelf = 0);
	void MakeSpawnUpdateNoDelta(SpawnPositionUpdate_Struct* spu);
	void MakeSpawnUpdate(SpawnPositionUpdate_Struct* spu);
	void SetSpawnUpdate(SpawnPositionUpdate_Struct* incoming, SpawnPositionUpdate_Struct* outgoing);
	void SetSpawned() { spawned = true; };
	bool Spawned() { return spawned; };
	void SendPosition(bool everyone = false, bool ackreq = false);
	void SendRealPosition();
	void SetFlyMode(GravityBehavior flymode);
	void Teleport(const glm::vec3& pos);
	void Teleport(const glm::vec4& pos);
	void SetAnimation(int8 anim) { animation = anim; } // For Eye of Zomm. It's a NPC, but uses PC position updates.
	bool IsFacingTarget();
	float adjustedz; // Fixes NPCs z coord.

	//AI
	static uint32 GetLevelCon(uint8 mylevel, uint8 iOtherLevel);
	inline uint32 GetLevelCon(uint8 iOtherLevel) const {
		return this ? GetLevelCon(GetLevel(), iOtherLevel) : CON_GREEN; }
	virtual void AddToHateList(Mob* other, int32 hate = 0, int32 damage = 0, bool bFrenzy = false, bool iBuffTic = false, bool addpet = true);
	bool RemoveFromHateList(Mob* mob);
	void RemoveFeignedFromHateList() { hate_list.RemoveFeigned(); }
	void SetHate(Mob* other, int32 hate = 0, int32 damage = -1) { hate_list.Set(other,hate,damage);}
	virtual void AddHate(Mob* other, int32 hate = 0, int32 damage = 0, bool bFrenzy = false, bool iAddIfNotExist = true) { hate_list.Add(other, hate, damage, bFrenzy, iAddIfNotExist); }
	void HalveAggro(Mob *other) { uint32 in_hate = GetHateAmount(other, false); SetHate(other, (in_hate > 1 ? in_hate / 2 : 1)); }
	void DoubleAggro(Mob *other) { uint32 in_hate = GetHateAmount(other, false); SetHate(other, (in_hate ? in_hate * 2 : 1)); }
	uint32 GetHateAmount(Mob* tmob, bool include_bonus = true) { return hate_list.GetEntHate(tmob, include_bonus);}
	uint32 GetDamageAmount(Mob* tmob, bool combine_pet_dmg = false) { return hate_list.GetEntDamage(tmob, combine_pet_dmg);}
	Mob* GetHateTop() { return hate_list.GetTop();}
	void HandleFTEEngage(Client* c) { return hate_list.HandleFTEEngage(c); }
	void HandleFTEDisengage() { return hate_list.HandleFTEDisengage(); }
	bool IsCharacterOnHateList(uint32 character_id) { return hate_list.IsCharacterOnHateList(character_id); }
	bool IsGroupOnHateList(uint32 group_id) { return hate_list.IsGroupOnHateList(group_id); }
	bool IsRaidOnHateList(uint32 raid_id) { return hate_list.IsRaidOnHateList(raid_id); }
	Mob* GetDamageTopSingleMob(int32& return_dmg) { return hate_list.GetDamageTopSingleMob(return_dmg); }
	Mob* GetDamageTop(int32& return_dmg, bool combine_pet_dmg = false, bool clients_only = false) { return hate_list.GetDamageTop(return_dmg, combine_pet_dmg, clients_only); }
	Mob* GetHateRandom() { return hate_list.GetRandom();}
	Client* GetHateRandomClient(int32 max_dist = 0) { return hate_list.GetRandomClient(max_dist); }
	Mob* GetHateMost(bool includeBonus = true) { return hate_list.GetMostHate(includeBonus);}
	int GetNumHaters() { return hate_list.GetNumHaters(); }
	int GetHateN(int n) { return hate_list.GetHateN(n); }
	void SetAggroTime(uint32 in_time) { hate_list.SetAggroTime(in_time); }
	uint32 GetAggroDeaggroTime() { return hate_list.GetAggroDeaggroTime(); }	// time since aggro started (if engaged) or time since aggro ended (if not engaged)
	uint32 GetAggroTime() { return hate_list.GetAggroTime(); }	// time since aggro started (if engaged) or time since aggro ended (if not engaged)
	bool IsEngaged() { return engaged; }	// returns true only if actively chasing/fighting a target; returns false if ignoring all haters on hate list or list is empty
	uint32 GetIgnoreStuckCount() { return hate_list.GetIgnoreStuckCount(); }	// return how many times the NPC ignored all haters so we can unstick them if they get stuck
	bool HateSummon(Mob* summoned = nullptr);
	bool CheckHateSummon(Mob* summoned);
	bool BardHateSummon(Mob* summoned);
	void FaceTarget(Mob* MobToFace = 0);
	void SetHeading(float iHeading) { if(m_Position.w != iHeading) { m_Position.w = iHeading;} }
	void WipeHateList(bool from_memblur = false);
	void PrintHateListToClient(Client *who) { hate_list.PrintToClient(who); }
	std::list<tHateEntry*>& GetHateList() { return hate_list.GetHateList(); }
	bool CheckLosFN(Mob* other, bool spell_casting = false);
	bool CheckLosFN(float posX, float posY, float posZ, float mobSize, Mob* other = nullptr, bool spell_casting = false);
	bool CheckRegion(Mob* other, bool skipwater = true);
	inline void SetLastLosState(bool value) { last_los_check = value; }
	inline bool CheckLastLosState() const { return last_los_check; }

	//Quest
	inline bool GetQglobal() const { return qglobal; }
	void DeleteBucket(std::string bucket_name);
	std::string GetBucket(std::string bucket_name);
	std::string GetBucketExpires(std::string bucket_name);
	std::string GetBucketKey();
	std::string GetBucketRemaining(std::string bucket_name);
	void SetBucket(std::string bucket_name, std::string bucket_value, std::string expiration = "");

	//Other Packet
	void CreateDespawnPacket(EQApplicationPacket* app, bool Decay);
	void CreateHorseSpawnPacket(EQApplicationPacket* app, const char* ownername, uint16 ownerid, Mob* ForWho = 0);
	void CreateSpawnPacket(EQApplicationPacket* app, Mob* ForWho = 0);
	static void CreateSpawnPacket(EQApplicationPacket* app, NewSpawn_Struct* ns);
	virtual void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho);
	void CreateHPPacket(EQApplicationPacket* app);
	void SendHPUpdate(bool skipnpc = true, bool sendtoself = true); //skipnpc is only used when rule Alkabor:NPCsSendHPUpdatesPerTic is true.

	//Util
	static uint32 RandomTimer(int min, int max);
	static uint8 GetDefaultGender(uint16 in_race, uint8 in_gender = 0xFF);
	static bool IsPlayerClass(uint16 in_class);
	static bool IsPlayerRace(uint16 in_race);
	uint16 GetSkillByItemType(int ItemType);
	uint8 GetItemTypeBySkill(EQ::skills::SkillType skill);
	virtual void MakePet(uint16 spell_id, const char* pettype, const char *petname = nullptr);
	virtual void MakePoweredPet(uint16 spell_id, const char* pettype, int16 petpower, const char *petname = nullptr, float in_size = 0.0f, int16 focusItemId = 0);
	bool IsWarriorClass() const;
	char GetCasterClass() const;
	uint8 GetArchetype() const;
	void SetZone(uint32 zone_id, uint32 zone_guild_id = 0xFFFFFFFF);
	void ShowStats(Client* client);
	void ShowBuffs(Client* client);
	void ShowBuffList(Client* client);
	bool PlotPositionAroundTarget(Mob* target, float &x_dest, float &y_dest, float &z_dest,
		bool lookForAftArc = true);
	bool CheckBardHateSummon(Mob* summoned);
	int32  GetSkillStat(EQ::skills::SkillType skillid);	
	bool IsPlayerOwned();

	//Procs
	bool AddProcToWeapon(uint16 spell_id, uint16 iChance = 3, uint16 base_spell_id = SPELL_UNKNOWN, bool poison = false);
	bool RemoveProcFromWeapon(uint16 spell_id, bool bAll = false);
	bool RemovePoisonFromWeapon(uint16 spell_id, bool bAll = false);
	bool HasProcs() const;
	bool IsCombatProc(uint16 spell_id);

	//More stuff to sort:
	virtual bool IsRaidTarget() const { return false; };
	virtual bool IsAttackAllowed(Mob *target, bool isSpellAttack = false, int16 spellid = 0);
	bool IsTargeted() const { return (targeted > 0); }
	inline void IsTargeted(int in_tar) { targeted += in_tar; if(targeted < 0) targeted = 0;}
	void SetFollowID(uint32 id) { follow = id; }
	void SetFollowDistance(uint32 dist) { follow_dist = dist; }
	uint32 GetFollowID() const { return follow; }
	uint32 GetFollowDistance() const { return follow_dist; }
	inline bool IsRareSpawn() const { return rare_spawn; }
	inline void SetRareSpawn(bool in) { rare_spawn = in; }

	virtual void Message(uint32 type, const char* message, ...) { }
	virtual void Message_StringID(uint32 type, uint32 string_id, uint32 distance = 0) { }
	virtual void Message_StringID(uint32 type, uint32 string_id, const char* message, const char* message2 = 0,
		const char* message3 = 0, const char* message4 = 0, const char* message5 = 0, const char* message6 = 0,
		const char* message7 = 0, const char* message8 = 0, const char* message9 = 0, uint32 distance = 0) { }
	virtual void FilteredMessage_StringID(Mob *sender, uint32 type, eqFilterType filter, uint32 string_id) { }
	virtual void FilteredMessage_StringID(Mob *sender, uint32 type, eqFilterType filter,
			uint32 string_id, const char *message1, const char *message2 = nullptr,
			const char *message3 = nullptr, const char *message4 = nullptr,
			const char *message5 = nullptr, const char *message6 = nullptr,
			const char *message7 = nullptr, const char *message8 = nullptr,
			const char *message9 = nullptr) { }
	void Say(const char *format, ...);
	void Say_StringID(uint32 string_id, const char *message3 = 0, const char *message4 = 0, const char *message5 = 0,
		const char *message6 = 0, const char *message7 = 0, const char *message8 = 0, const char *message9 = 0);
	void Say_StringID(uint32 type, uint32 string_id, const char *message3 = 0, const char *message4 = 0, const char *message5 = 0,
		const char *message6 = 0, const char *message7 = 0, const char *message8 = 0, const char *message9 = 0);
	void Shout(const char *format, ...);
	void Emote(const char *format, ...);

	int16 CalcFocusEffect(focusType type, uint16 focus_id, uint16 spell_id, bool best_focus=false, bool dot_tick=false, int spell_level=-1);
	uint8 IsFocusEffect(uint16 spellid, int effect_index, bool AA=false,uint32 aa_effect=0);
	void SendIllusionPacket(uint16 in_race, uint8 in_gender = 0xFF, uint8 in_texture = 0xFF, uint8 in_helmtexture = 0xFF, 
		uint8 in_haircolor = 0xFF, uint8 in_beardcolor = 0xFF, uint8 in_eyecolor1 = 0xFF, uint8 in_eyecolor2 = 0xFF, 
		uint8 in_hairstyle = 0xFF, uint8 in_luclinface = 0xFF, uint8 in_beard = 0xFF, uint8 in_aa_title = 0xFF, float in_size = -1.0f, Client* sendto = nullptr);
	virtual void Stun(int duration, Mob* attacker);
	virtual void UnStun();
	inline void Silence(bool newval) { silenced = newval; }
	inline void Amnesia(bool newval) { amnesiad = newval; }
	NPC *CreateTemporaryPet(const NPCType* npc_type, uint32 pet_duration_seconds, uint32 target_id, bool followme, bool sticktarg, glm::vec4 position);
	void TemporaryPets(uint16 spell_id, Mob *target, const char *name_override = nullptr, uint32 duration_override = 0, bool followme=true, bool sticktarg=false);
	void CopyWakeCorpse(NPCType *make_npc, Corpse *CorpseToUse);
	void Spin();
	void Kill();
	bool TryDeathSave();
	bool TryDivineSave();
	int16 GetHealRate(uint16 spell_id, Mob* caster = nullptr);
	bool DoKnockback(Mob *caster, float pushback, float pushup, bool send_packet = false);
	bool CombatPush(Mob* attacker, float pushback);
	void GetPushHeadingMod(Mob* attacker, float pushback, float &x_coord, float &y_coord);
	int16 CalcResistChanceBonus();
	int16 CalcFearResistChance();
	void SlowMitigation(Mob* caster);
	virtual int GetOffense(EQ::skills::SkillType skill);
	virtual int GetOffenseByHand(int hand = EQ::invslot::slotPrimary);
	virtual int GetToHit(EQ::skills::SkillType skill);
	virtual int GetToHitByHand(int hand = EQ::invslot::slotPrimary);
	virtual int GetMitigation();
	virtual int GetAvoidance();
	int16 GetMeleeDamageMod_SE(uint16 skill);
	int16 GetMeleeMinDamageMod_SE(uint16 skill);
	int16 GetSkillReuseTime(uint16 skill);
	bool TryReflectSpell(uint32 spell_id);
	int8 GetDecayEffectValue(uint16 spell_id, uint16 spelleffect);
	int32 GetExtraSpellAmt(uint16 spell_id, int32 extra_spell_amt, int32 base_spell_dmg);
	void MeleeLifeTap(int32 damage);
	bool PassCastRestriction(bool UseCastRestriction = true, int16 value = 0, bool IsDamage = true);
	bool TryRootFadeByDamage(int buffslot, Mob* attacker);
	int16 GetSlowMitigation() const {return slow_mitigation;}
	int32 GetSpellStat(uint32 spell_id, const char *identifier, uint8 slot = 0);

	void SetAllowBeneficial(bool value) { m_AllowBeneficial = value; }
	bool GetAllowBeneficial() { if (m_AllowBeneficial || GetSpecialAbility(SpecialAbility::AllowBeneficial)){return true;} return false; }
	void SetDisableMelee(bool value) { m_DisableMelee = value; }
	bool IsMeleeDisabled() { if (m_DisableMelee || GetSpecialAbility(SpecialAbility::DisableMelee)){return true;} return false; }

	void SetFlurryChance(uint8 value) { SetSpecialAbilityParam(SpecialAbility::Flurry, 0, value); }
	uint8 GetFlurryChance() { return GetSpecialAbilityParam(SpecialAbility::Flurry, 0); }

	static uint32 GetAppearanceValue(EmuAppearance iAppearance);
	void SendAppearancePacket(uint32 type, uint32 value, bool WholeZone = true, bool iIgnoreSelf = false, Client *specific_target=nullptr);
	void SetAppearance(EmuAppearance app, bool iIgnoreSelf = true);
	inline EmuAppearance GetAppearance() const { return _appearance; }
	inline const uint8 GetRunAnimSpeed() const { return pRunAnimSpeed; }
	inline void SetRunAnimSpeed(int8 in) { if (pRunAnimSpeed != in) { pRunAnimSpeed = in; } }
	float SetRunAnimation(float speed);

	inline uint8 GetInnateLightType() { return m_Light.Type[EQ::lightsource::LightInnate]; }
	inline uint8 GetEquipmentLightType() { return m_Light.Type[EQ::lightsource::LightEquipment]; }
	inline uint8 GetSpellLightType() { return m_Light.Type[EQ::lightsource::LightSpell]; }

	virtual void UpdateEquipmentLight() { m_Light.Type[EQ::lightsource::LightEquipment] = 0; m_Light.Level[EQ::lightsource::LightEquipment] = 0; }
	inline void SetSpellLightType(uint8 lightType) { m_Light.Type[EQ::lightsource::LightSpell] = (lightType & 0x0F); m_Light.Level[EQ::lightsource::LightSpell] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightSpell]); }

	inline uint8 GetActiveLightType() { return m_Light.Type[EQ::lightsource::LightActive]; }
	bool UpdateActiveLight(); // returns true if change, false if no change

	EQ::LightSourceProfile* GetLightProfile() { return &m_Light; }

	Mob* GetPet();
	void SetPet(Mob* newpet);
	virtual Mob* GetOwner();
	virtual Mob* GetOwnerOrSelf();
	Mob* GetUltimateOwner();
	void SetPetID(uint16 NewPetID);
	inline uint16 GetPetID() const { return petid; }
	inline uint16 GetSummonerID() const { return summonerid; }
	void SetSummonerID(uint16 entity) { summonerid = entity; }
	inline PetType GetPetType() const { return typeofpet; }
	void SetPetType(PetType p) { typeofpet = p; }
	inline int16 GetPetPower() const { return petpower; }
	inline int16 GetPetFocusItemID() const { return petfocusItemId; }
	bool IsFamiliar() const { return(typeofpet == petFamiliar); }
	bool IsAnimation() const { return(typeofpet == petAnimation); }
	bool IsCharmed() const { return(typeofpet == petCharmed); } // All mobs by default have typeofpet set to 3. Use IsCharmedPet() so you don't also check for IsPet()
	bool IsSummonedClientPet() const { return summonedClientPet; }
	void SetSummonedClientPet(bool value) { summonedClientPet = value; }
	bool IsDireCharmed() { return dire_charmed; }
	void SetOwnerID(uint16 NewOwnerID);
	inline uint16 GetOwnerID() const { return ownerid; }
	inline virtual bool HasOwner() { if(GetOwnerID()==0){return false;} return( entity_list.GetMob(GetOwnerID()) != 0); }
	inline virtual bool IsPet() { return(HasOwner()); }
	inline bool HasPet() const { if(GetPetID()==0){return false;} return (entity_list.GetMob(GetPetID()) != 0);}
	inline bool IsTempPet() const { return(_IsTempPet); }
	inline void SetIsTempPet(bool i) { _IsTempPet = i; }
	inline bool HasTempPetsActive() const { return(hasTempPet); }
	inline void SetTempPetsActive(bool i) { hasTempPet = i; }
	inline int16 GetTempPetCount() const { return count_TempPet; }
	inline void SetTempPetCount(int16 i) { count_TempPet = i; }
	bool HasPetAffinity() { if (aabonuses.GivePetGroupTarget || itembonuses.GivePetGroupTarget || spellbonuses.GivePetGroupTarget) return true; return false; }
	void DepopPet(bool depopsummoned = false);
	void FadePetCharmBuff();
	virtual bool IsCharmedPet() { return IsPet() && IsCharmed(); }
	void SetCorpseID(uint16 in_corpseid) { corpseid = in_corpseid; };
	inline uint16 GetCorpseID() const { return corpseid; }

	inline const uint8 GetBodyType() const { return bodytype; }
	inline const uint8 GetOrigBodyType() const { return orig_bodytype; }
	void SetBodyType(uint8 new_body, bool overwrite_orig);

	uint8 invisible, see_invis;
	bool invulnerable, invisible_undead, invisible_animals, sneaking, hidden, improved_hidden;
	bool see_invis_undead, see_sneak, see_improved_hide;
	bool qglobal;

	virtual void SetAttackTimer(bool trigger = false);
	inline void SetInvul(bool invul) { invulnerable=invul; }
	inline bool GetInvul(void) { return invulnerable; }
	inline void SetExtraHaste(int Haste) { ExtraHaste = Haste; }
	virtual int GetHaste();
	virtual int GetHasteCap() { return 0; }

	uint32 GetClassLevelFactor();
	void Mesmerize();
	bool CanDualWield();
	bool IsDualWielding();
	inline bool IsMezzed() const { return mezzed; }
	inline bool IsStunned() const { return stunned; }
	inline bool IsSilenced() const { return silenced; }
	inline bool IsAmnesiad() const { return amnesiad; }

	int32 ReduceDamage(int32 damage);
	int32 AffectMagicalDamage(int32 damage, uint16 spell_id, const bool iBuffTic, Mob* attacker);

	virtual void DoThrowingAttackDmg(Mob* other);
	virtual void DoArcheryAttackDmg(Mob* other);
	bool CanDoSpecialAttack(Mob *other);
	bool Flurry();
	bool Rampage(int range = 75, int damagePct = 100);
	bool AddRampage(Mob*);
	void ClearRampage();
	void RemoveFromRampageList(Mob* mob, bool force = false);
	int  GetRampageListSize() const { return RampageArray.size(); }
	int  GetRampageEntityID(int slot) const { if (slot < RampageArray.size() && slot >= 0) return RampageArray[slot]; else return -1; }
	void AreaRampage(int numTargets = -1, int damagePct = 100);

	void CheckEnrage();
	void StartEnrage();
	void ProcessEnrage();
	bool IsEnraged();
	void Taunt(NPC* who, bool always_succeed = false, int32 overhate = 0);

	virtual void AI_Init();
	virtual void AI_Start();
	virtual void AI_Stop();
	virtual void AI_ShutDown();
	virtual void AI_Process();

	bool ClearEntityVariables();
	bool DeleteEntityVariable(std::string variable_name);
	std::string GetEntityVariable(std::string variable_name);
	std::vector<std::string> GetEntityVariables();
	void SetEntityVariable(std::string variable_name, std::string variable_value);
	bool EntityVariableExists(std::string variable_name);

	void AI_Event_Engaged(Mob* attacker);
	void AI_SetLoiterTimer();
	void AI_TriggerHailTimer() { AIhail_timer->Trigger(); }
	void AI_Event_NoLongerEngaged();

	std::map<std::string, PlayerEngagementRecord>& GetEngagementRecords() { return m_EngagedClientNames; };

	FACTION_VALUE GetSpecialFactionCon(Mob* iOther);
	inline const bool IsAIControlled() const { return pAIControlled; }
	inline const float GetAggroRange() const { return (spellbonuses.AggroRange == -1) ? pAggroRange : spellbonuses.AggroRange; }
	inline void SetAggroRange(float range) { pAggroRange = range; }
	inline const float GetAssistRange() const { return (spellbonuses.AssistRange == -1) ? pAssistRange : spellbonuses.AssistRange; }
	inline void SetAssistRange(float range) { pAssistRange = range; }

	inline void SetPetOrder(eStandingPetOrder i) { pStandingPetOrder = i; }
	inline const eStandingPetOrder GetPetOrder() const { return pStandingPetOrder; }
	inline void SetHeld(bool nState) { held = nState; }
	inline const bool IsHeld() const { return held; }
	inline void SetNoCast(bool nState) { nocast = nState; }
	inline const bool IsNoCast() const { return nocast; }
	inline const bool IsRoamer() const { return roamer; }
	inline const int GetWanderType() const { return wandertype; }
	inline const bool IsRooted() const { return rooted || permarooted; }
	int GetSnaredAmount();
	inline const bool IsPseudoRooted() const { return pseudo_rooted; }
	inline void SetPseudoRoot(bool prState) { pseudo_rooted = prState; }

	int GetCurWp() { return cur_wp; }

	// This returns true if the mob is feared or fleeing due to low HP
	bool IsFeared() { return (spellbonuses.IsFeared || flee_mode); } 
	bool IsFearedNoFlee() { return spellbonuses.IsFeared; }
	inline void StartFleeing() { flee_mode = true; CalculateNewFearpoint(); }
	void StopFleeing();
	inline bool IsFleeing() { return flee_mode; }
	void ProcessFlee();
	void CheckFlee();
	void FleeInfo(Mob* client);
	int GetFleeRatio(Mob* other = nullptr);
	inline bool IsBlind() { return spellbonuses.IsBlind; }
	void SetMoved(bool val) { moved = val; }

	inline bool			CheckAggro(Mob* other) { return hate_list.IsOnHateList(other); }
	float				CalculateHeadingToTarget(float in_x, float in_y) { return HeadingAngleToMob(in_x, in_y); }
	float				CalculatePitchToTarget(glm::vec3 loc);
	virtual void		WalkTo(float x, float y, float z);
	virtual void		RunTo(float x, float y, float z);
	void				NavigateTo(float x, float y, float z);
	void				RotateTo(float new_heading, bool at_guardpoint = false);
	void				RotateToWalking(float new_heading, bool at_guardpoint = false);
	void				RotateToRunning(float new_heading, bool at_guardpoint = false);
	void				StopNavigation(float new_heading = -1.0f);
	float				CalculateDistance(float x, float y, float z);
	float				GetGroundZ(float new_x, float new_y, float z_find_offset = 0.0);
	void				SendTo(float new_x, float new_y, float new_z);
	void				SendToFixZ(float new_x, float new_y, float new_z);
	inline float		GetZOffset() { return z_offset; }
	float               GetDefaultRaceSize() const;
	void 				FixZ(bool force = false);
	void				FixZInWater();
	float				GetFixedZ(const glm::vec3& destination, float z_find_offset = 5.0f);
	virtual int			GetStuckBehavior() const { return 0; }
	virtual				EQ::InventoryProfile &GetInv() { return m_inv; }
	float				FindGroundZ(float new_x, float new_y, float z_offset = 0.0);
	float				FindDestGroundZ(glm::vec3 dest, float z_find_offset = 0.0);

	inline uint32		DontBuffMeBefore() const { return pDontBuffMeBefore; }
	inline uint32		DontRootMeBefore() const { return pDontRootMeBefore; }
	inline uint32		DontSnareMeBefore() const { return pDontSnareMeBefore; }
	inline uint32		DontCureMeBefore() const { return pDontCureMeBefore; }
	void				SetDontRootMeBefore(uint32 time) { pDontRootMeBefore = time; }
	void				SetDontBuffMeBefore(uint32 time) { pDontBuffMeBefore = time; }
	void				SetDontSnareMeBefore(uint32 time) { pDontSnareMeBefore = time; }
	void				SetDontCureMeBefore(uint32 time) { pDontCureMeBefore = time; }

	// calculate interruption of spell via movement of mob
	void SaveSpellLoc() { m_SpellLocation = glm::vec3(m_Position); }
	inline float GetSpellX() const {return m_SpellLocation.x;}
	inline float GetSpellY() const {return m_SpellLocation.y;}
	inline float GetSpellZ() const {return m_SpellLocation.z;}
	inline bool IsGrouped() const { return isgrouped; }
	void SetGrouped(bool v);
	inline bool IsRaidGrouped() const { return israidgrouped; }
	void SetRaidGrouped(bool v);
	bool InSameGroup(Mob *other);
	bool InSameRaid(Mob *other);
	inline uint16 IsLooting() const { return entity_id_being_looted; }
	void SetLooting(uint16 val) { entity_id_being_looted = val; }

	bool CheckWillAggro(Mob *mob, bool turn_mobs = false);
	bool CheckDragAggro(Mob *mob);

	void InstillDoubt(Mob *who, int stage = 0);
	int16 GetResist(uint8 type) const;
	Mob* GetShieldTarget() const { return shield_target; }		// the entity I am shielding (warrior /shield)
	void SetShieldTarget(Mob* mob) { shield_target = mob;}
	Mob* GetShielder() const { return shielder; }				// the warrior shielding me
	void SetShielder(Mob* mob) { shielder = mob; }
	void StartShield(Mob* mob);
	void EndShield();
	void TryShielderDamage(Mob* attacker, int& damage, EQ::skills::SkillType skill);
	bool HasActiveSong() const { return(bardsong != 0); }
	static uint32 GetLevelHP(uint8 tlevel);
	uint32 GetZoneID() const; //for perl
	virtual int32 CheckAggroAmount(uint16 spell_id, Mob* target);
	virtual int32 CheckHealAggroAmount(uint16 spell_id, Mob* target, uint32 heal_possible = 0, bool from_clickable = false);
	virtual uint32 GetAA(uint32 aa_id) const { return(0); }

	uint32 GetInstrumentMod(uint16 spell_id);
	int CalcSpellEffectValue(uint16 spell_id, int effect_index, int caster_level = 1, int ticsremaining = 0, int instrumentmod = 10);
	int CalcSpellEffectValue_formula(int formula, int base, int max, int caster_level, uint16 spell_id, int ticsremaining = 0);
	uint32 GetCastedSpellInvSlot() const { return casting_spell_inventory_slot; }
	uint8 GetClickLevel(Mob* caster, uint16 spell_id);

	// HP Event
	inline int GetNextHPEvent() const { return nexthpevent; }
	void SetNextHPEvent( int hpevent );
	void SendItemAnimation(Mob *to, const EQ::ItemData *item, EQ::skills::SkillType skillInUse);
	inline int& GetNextIncHPEvent() { return nextinchpevent; }
	void SetNextIncHPEvent( int inchpevent );

	inline bool DivineAura() const { return spellbonuses.DivineAura; }

	int GetSpecialAbility(int ability);
	int GetSpecialAbilityParam(int ability, int param);
	void SetSpecialAbility(int ability, int level);
	void SetSpecialAbilityParam(int ability, int param, int value);
	void StartSpecialAbilityTimer(int ability, uint32 time);
	void StopSpecialAbilityTimer(int ability);
	Timer *GetSpecialAbilityTimer(int ability);
	void ClearSpecialAbilities();
	void ProcessSpecialAbilities(const std::string &str);
	void ModifySpecialAbility(const std::string &abil_str);
	void ClearBestZCount() { best_z_fail_count = 0; };

	Trade* trade;

	inline glm::vec4 GetCurrentWayPoint() const { return m_CurrentWayPoint; }
	inline float GetCWPP() const { return(static_cast<float>(cur_wp_pause)); }
	inline int GetCWP() const { return(cur_wp); }
	void SetCurrentWP(int waypoint) { cur_wp = waypoint; }
	virtual FACTION_VALUE GetReverseFactionCon(Mob* iOther) { return FACTION_INDIFFERENTLY; }

	Timer* GetAIThinkTimer() { return AIthink_timer.get(); }
	Timer* GetAIMovementTimer() { return AImovement_timer.get(); }
	Timer& GetAttackTimer() { return attack_timer; }
	Timer& GetAttackDWTimer() { return attack_dw_timer; }
	inline uint8 GetManaPercent() { return (uint8)((float)cur_mana / (float)max_mana * 100.0f); }
	inline void SpawnPacketSent(bool val) { spawnpacket_sent = val; };

	inline virtual bool IsBlockedBuff(int16 SpellID) { return false; }
	inline virtual bool IsBlockedPetBuff(int16 SpellID) { return false; }

	std::string GetGlobal(const char *varname);
	void SetGlobal(const char *varname, const char *newvalue, int options, const char *duration, Mob *other = nullptr);
	void TarGlobal(const char *varname, const char *value, const char *duration, int npcid, int charid, int zoneid);
	void DelGlobal(const char *varname);

	inline void SetEmoteID(uint32 emote) { emoteid = emote; }
	inline uint32 GetEmoteID() { return emoteid; }
	inline void SetCombatHPRegen(uint32 regen) { combat_hp_regen = regen; }
	inline uint16 GetCombatHPRegen() { return combat_hp_regen; }
	inline void SetCombatManaRegen(uint32 regen) { combat_mana_regen = regen; }
	inline uint16 GetCombatManaRegen() { return combat_mana_regen; }

	bool 	HasSpellEffect(int effectid);

	int		GetDoubleAttackChance(bool returnEffectiveSkill = false);
	bool	CheckDoubleAttack();
	int		GetDualWieldChance(bool returnEffectiveSkill = false);
	bool	CheckDualWield();
	virtual uint8 Disarm(float chance) { return 0; };

	float CalcZOffset();
	float CalcHeadOffset();
	float CalcModelSize();
	float CalcBoundingRadius();

	bool IsZomm() { return iszomm; }
	void SetMerchantSession(uint16 value) { MerchantSession = value; }
	uint16 GetMerchantSession() { return MerchantSession; }
	uint32 player_damage;
	uint32 ssf_player_damage;
	uint32 ssf_ds_damage;
	uint32 dire_pet_damage;
	uint32 total_damage;
	uint32 ds_damage;
	uint32 npc_damage;
	uint32 gm_damage;
	uint32 pbaoe_damage;
	void DamageTotalsWipe(bool from_memblur = false);
	void ReportDmgTotals(Client* client, bool corpse = false, bool xp = false, bool faction = false, int32 dmg_amt = 0);
	float  GetBaseEXP();
	static bool IsPlayableRace(uint16 race);
	float GetPlayerHeight(uint16 race);
	void FadeVoiceGraft();
	bool IsUnTargetable();
	void DamageCommand(Mob* from, int32 damage, bool skipaggro = false, uint16 spell_id = SPELL_UNKNOWN, EQ::skills::SkillType attack_skill = EQ::skills::SkillHandtoHand);
	uint32 GetMobDamageShieldType() { return spellbonuses.DamageShieldType; };
	void PurgePoison(Client* caster); //AA Purge Poison
	void ApplyIllusion(const SPDat_Spell_Struct &spell, int index, Mob* caster);
	void SetGMSpellException(uint8 value) { casting_gm_override = value; };
	bool PermaRooted() { return permarooted; }
	bool PacifyImmune;

protected:
	void CommonDamage(Mob* other, int32 &damage, const uint16 spell_id, const  EQ::skills::SkillType  attack_skill, bool &avoidable, const int8 buffslot, const bool iBuffTic);
	void AggroPet(Mob* attacker);
	static uint16 GetProcID(uint16 spell_id, uint8 effect_index);
	//float _GetMovementSpeed(int mod, bool iswalking = false) const;
	int _GetRunSpeed() const;
	int _GetWalkSpeed() const;
	int _GetFearSpeed() const;
	void DoFearMovement();

	virtual bool AI_EngagedCastCheck() { return(false); }
	virtual bool AI_IdleCastCheck() { return(false); }


	bool IsFullHP;
	bool moved;
	float current_speed;

	MobMovementManager* mMovementManager;

	std::vector<uint16> RampageArray;
	std::map<std::string, std::string> m_EntityVariables;

	std::map<std::string, PlayerEngagementRecord> m_EngagedClientNames;
	
	bool m_AllowBeneficial;
	bool m_DisableMelee;

	bool isgrouped;
	bool israidgrouped;
	bool pendinggroup;
	uint16 entity_id_being_looted; //the id of the entity being looted, 0 if not looting.
	uint8 texture;
	uint8 chesttexture;
	uint8 helmtexture;
	uint8 armtexture;
	uint8 bracertexture;
	uint8 handtexture;
	uint8 legtexture;
	uint8 feettexture;
	bool multitexture;
	uint8 helmtexture_quarm; // used for saving Quarm's head state when illusioned into skeleton

	int AC;
	int32 ATK;
	int32 STR;
	int32 STA;
	int32 DEX;
	int32 AGI;
	int32 INT;
	int32 WIS;
	int32 CHA;
	int32 MR;
	int32 CR;
	int32 FR;
	int32 DR;
	int32 PR;
	bool moving;
	int targeted;
	int32 cur_hp;
	int32 max_hp;
	int32 base_hp;
	int32 cur_mana;
	int32 max_mana;
	int32 hp_regen;
	int32 mana_regen;
	int32 oocregen;
	uint8 maxlevel;
	uint32 scalerate;
	Buffs_Struct *buffs;
	uint32 current_buff_count;
	bool current_buff_refresh;
	StatBonuses itembonuses;
	StatBonuses spellbonuses;
	StatBonuses aabonuses;
	uint16 petid;
	uint16 ownerid;
	uint16 summonerid;		// if pet is summoned pet, store caster's ID so we can return pet to owner after charm
	PetType typeofpet;
	int16 petpower;
	int16 petfocusItemId;
	uint32 follow;
	uint32 follow_dist;
	bool rare_spawn;
	bool dire_charmed;
	bool feared;
	int16 bonusAvoidance;	// needed for NPCs but putting this in Mob class in case we put back in Ykesha era stats
	uint16 corpseid;	// When a mob dies and creates a corpse, before it depops the mob's ID is zeroed and the corpse receives it

	uint8 gender;
	uint16 race;
	uint8 base_gender;
	uint16 base_race;
	uint8 class_;
	uint8 bodytype;
	uint8 orig_bodytype;
	uint16 deity;
	uint8 level;
	uint8 orig_level;
	uint32 npctype_id;
	glm::vec4 m_Position;
	glm::vec4 m_Navigation;
	glm::vec4 m_EQPosition; // This acts as a home/backup set of coords. It is currently used to set a home point for Eye of Zomm.
	int8 animation;
	uint32 last_update;
	float base_size;
	float size;
	float z_offset;
	float head_offset;
	float model_size;
	float model_bounding_radius;
	float runspeed;
	float walkspeed;
	float fearspeed;
	bool held;
	bool nocast;
	bool spawnpacket_sent;
	bool spawned;
	bool summonedClientPet;
	void CalcSpellBonuses(StatBonuses* newbon);
	virtual void CalcBonuses();
	void CalcSpellBonuses();
	bool PassLimitClass(uint32 Classes_, uint16 Class_);
	bool TryWeaponProc(const EQ::ItemInstance* inst, const EQ::ItemData* weapon, Mob *on, uint16 hand = EQ::invslot::slotPrimary);
	bool TrySpellProc(const EQ::ItemInstance* inst, const EQ::ItemData* weapon, Mob *on, uint16 hand = EQ::invslot::slotPrimary);
	bool TryProcs(Mob *target, uint16 hand = EQ::invslot::slotPrimary);
	bool ExecWeaponProc(const EQ::ItemInstance* weapon, uint16 spell_id, Mob *on);
	virtual float GetProcChance(uint16 hand = EQ::invslot::slotPrimary);
	uint16 GetWeaponSpeedbyHand(uint16 hand);
	int CalcMeleeDamage(Mob* defender, int baseDamage, EQ::skills::SkillType skill);
	void CalculateNewFearpoint();
	glm::vec3 UpdatePath(float ToX, float ToY, float ToZ, float Speed, bool &WaypointChange, bool &NodeReached, float to_size = 6.0f);
	void PrintRoute();

	enum {MAX_PROCS = 4};
	tProc SpellProcs[MAX_PROCS];

	char name[64];
	char orig_name[64];
	char clean_name[64];
	char clean_name_spaces[64];
	char lastname[32];

	glm::vec4 m_Delta;

	// just locs around them to double check, if we do expand collision this should be cached on movement
	// ideally we should use real models, but this should be quick and work mostly
	glm::vec4 m_CollisionBox[COLLISION_BOX_SIZE];

	EQ::LightSourceProfile m_Light;

	float fixedZ;
	EmuAppearance _appearance;
	int8 pRunAnimSpeed;
	bool m_is_running; // This bool tells us if the NPC *should* be running or walking, to calculate speed.

	Timer attack_timer;
	Timer attack_dw_timer;
	Timer ranged_timer;
	int16 attack_delay; //delay between attacks in milliseconds or hundreds of milliseconds
	int16 slow_mitigation; // Allows for a slow mitigation (100 = 100%, 50% = 50%)
	Timer tic_timer;
	Timer mana_timer;

	//spell casting vars
	Timer spellend_timer;
	uint16 casting_spell_id;
	glm::vec3 m_SpellLocation;
	int attacked_count;
	uint16 casting_spell_targetid;
	EQ::spells::CastingSlot casting_spell_slot;
	uint16 casting_spell_mana;
	uint32 casting_spell_inventory_slot;
	uint32 casting_spell_timer;
	uint32 casting_spell_timer_duration;
	uint32 casting_spell_type;
	int16 casting_spell_resist_adjust;
	int32 casting_spell_focus_duration;
	uint8 casting_spell_focus_range;
	uint32 casting_aa;
	uint16 bardsong;
	EQ::spells::CastingSlot bardsong_slot;
	uint32 bardsong_target_id;
	uint32 interrupt_message; // SpellFinished() uses this to pass the failure message back to CastedSpellFinished()
	Timer spellrecovery_timer;
	Timer clickyspellrecovery_timer;
	int clickyspellrecovery_burst;
	uint8 casting_gm_override;
	TemporaryPetsEffect* temporary_pets_effect;

	glm::vec3 m_RewindLocation;
	glm::vec4 m_LastLocation;
	Timer rewind_timer;

	uint8 haircolor;
	uint8 beardcolor;
	uint8 eyecolor1; // the eyecolors always seem to be the same, maybe left and right eye?
	uint8 eyecolor2;
	uint8 hairstyle;
	uint8 luclinface; //
	uint8 beard;
	EQ::TintProfile armor_tint;

	uint8 aa_title;

	Mob* shield_target;		// (for warriors) target being /shielded
	Mob* shielder;			// warrior /shielding me
	uint32 shield_cooldown;
	Timer shield_timer;

	int ExtraHaste; // for the #haste command
	bool mezzed;
	bool stunned;
	bool charmed; //this isnt fully implemented yet
	bool rooted;
	bool silenced;
	bool blind;
	bool amnesiad;
	bool inWater; // Set to true or false by Water Detection code if enabled by rules
	bool offhand;
	bool has_shieldequiped;
	bool has_bowequipped = false;
	bool has_arrowequipped = false;
	bool has_bashEnablingWeapon; //Used for Pal/SK epic
	bool has_MGB;
	bool has_ProjectIllusion;
	bool last_los_check;
	bool pseudo_rooted;
	glm::vec3 last_dest;

	// Bind wound
	Timer bindwound_timer;
	uint16 bindwound_target_id;

	Timer stunned_timer;
	Timer spun_timer;
	Timer bardsong_timer;
	Timer position_update_melee_push_timer;
	Timer spun_resist_timer;
	Timer anim_timer;

	// MobAI stuff
	eStandingPetOrder pStandingPetOrder;
	float pAggroRange;
	float pAssistRange;
	std::unique_ptr<Timer> AIthink_timer;
	std::unique_ptr<Timer> AImovement_timer;
	bool permarooted;
	std::unique_ptr<Timer> AI_scan_area_timer;
	std::unique_ptr<Timer> AIwalking_timer;
	std::unique_ptr<Timer> AIhail_timer;
	std::unique_ptr<Timer> AIpetguard_timer;
	std::unique_ptr<Timer> AIdoor_timer;
	std::unique_ptr<Timer> AIloiter_timer;
	std::unique_ptr<Timer> AIheading_timer;
	std::unique_ptr<Timer> AIstackedmobs_timer;
	HateList hate_list;
	// This is to keep track of mobs we cast faction mod spells on
	std::map<uint32,int32> faction_bonuses; // Primary FactionID, Bonus
	void AddFactionBonus(uint32 pFactionID,int32 bonus);
	int32 GetFactionBonus(uint32 pFactionID);
	// This is to keep track of item faction modifiers
	std::map<uint32,int32> item_faction_bonuses; // Primary FactionID, Bonus
	void AddItemFactionBonus(uint32 pFactionID,int32 bonus);
	int32 GetItemFactionBonus(uint32 pFactionID);
	void ClearItemFactionBonuses();
	void ClearFactionBonuses();

	bool flee_mode;
	Timer flee_timer;

	bool pAIControlled;
	bool roamer;

	int wandertype;
	int pausetype;

	int cur_wp;
	glm::vec4 m_CurrentWayPoint;
	int cur_wp_pause;


	int patrol;
	glm::vec3 m_FearWalkTarget;
	bool curfp;

	int best_z_fail_count;

	bool pause_timer_complete;
	uint32 pDontBuffMeBefore;
	uint32 pDontRootMeBefore;
	uint32 pDontSnareMeBefore;
	uint32 pDontCureMeBefore;

	// hp event
	int nexthpevent;
	int nextinchpevent;

	//temppet
	bool hasTempPet;
	bool _IsTempPet;
	int16 count_TempPet;
	bool pet_owner_client; //Flags regular and pets as belonging to a client

	glm::vec3 m_TargetLocation;
	uint8 tar_ndx;
	glm::vec3 m_DeltaV;
	glm::vec3 m_TargetV;
	float tar_vector;
	float test_vector;

	GravityBehavior flymode;
	bool m_targetable;
	int QGVarDuration(const char *fmt);
	void InsertQuestGlobal(int charid, int npcid, int zoneid, const char *name, const char *value, int expdate);
	uint32 emoteid;
	uint32 combat_hp_regen;
	uint32 combat_mana_regen;

	MobSpecialAbility SpecialAbilities[SpecialAbility::Max];
	bool bEnraged;
	bool iszomm;
	uint16 MerchantSession;
	bool engaged;

	uint16 instillDoubtTargetID;
	Timer instillDoubtStageTimer;

private:

	EQ::InventoryProfile m_inv;

	void _StopSong(); //this is not what you think it is
	int int_runspeed;
	int int_walkspeed;
	int int_fearspeed;

	int base_runspeed;
	int base_walkspeed;
	int base_fearspeed;
	Mob* target;
};

#endif

