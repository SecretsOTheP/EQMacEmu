#ifndef __EQEMU_ZONE_COMMON_H
#define __EQEMU_ZONE_COMMON_H

#include "../common/types.h"
#include "../common/spdat.h"

#define	HIGHEST_RESIST 9 //Max resist type value

/* solar: macros for IsAttackAllowed, IsBeneficialAllowed */
#define _CLIENT(x) (x && x->IsClient() && !x->CastToClient()->IsBecomeNPC())
#define _NPC(x) (x && x->IsNPC() && !x->CastToMob()->GetOwnerID())
#define _BECOMENPC(x) (x && x->IsClient() && x->CastToClient()->IsBecomeNPC())
#define _CLIENTCORPSE(x) (x && x->IsCorpse() && x->CastToCorpse()->IsPlayerCorpse() && !x->CastToCorpse()->IsBecomeNPCCorpse())
#define _NPCCORPSE(x) (x && x->IsCorpse() && (x->CastToCorpse()->IsNPCCorpse() || x->CastToCorpse()->IsBecomeNPCCorpse()))
#define _CLIENTPET(x) (x && x->CastToMob()->GetOwner() && x->CastToMob()->GetOwner()->IsClient())
#define _NPCPET(x) (x && x->IsNPC() && x->CastToMob()->GetOwner() && x->CastToMob()->GetOwner()->IsNPC())
#define _BECOMENPCPET(x) (x && x->CastToMob()->GetOwner() && x->CastToMob()->GetOwner()->IsClient() && x->CastToMob()->GetOwner()->CastToClient()->IsBecomeNPC())

//LOS Parameters:
#define HEAD_POSITION 0.9f	//ratio of GetSize() where NPCs see from
#define SEE_POSITION 0.5f	//ratio of GetSize() where NPCs try to see for LOS
#define CHECK_LOS_STEP 1.0f
#define LOS_MAX_HEIGHT 12.0f
#define LOS_DEFAULT_HEIGHT 6.0f

#define ARCHETYPE_HYBRID	1
#define ARCHETYPE_CASTER	2
#define ARCHETYPE_MELEE		3

#define CON_GREEN		2
#define CON_LIGHTBLUE	18
#define CON_BLUE		4
#define CON_WHITE		20
#define CON_YELLOW		15
#define CON_RED			13

//these are large right now because the x,y,z coords of the zone
//lines do not make a lot of sense
//Maximum distance from a zone point given that the request didnt
//know what zone that the line was for
#define ZONEPOINT_NOZONE_RANGE 40000.0f
//Maximum distance from a zone point if zone was specified
#define ZONEPOINT_ZONE_RANGE 40000.0f

typedef enum {	//focus types
	focusSpellHaste = 1,
	focusSpellDuration,
	focusRange,
	focusReagentCost,
	focusManaCost,
	focusImprovedHeal,
	focusImprovedDamage,
	focusSpellDamageMult,			// 'Increase Spell Damage by X% (Before Crit)';  only used for SK Soul Abrasion AA
	focusSpellHateMod				// Spell and Bash only hate multiplier focus; mostly from Furious Bash shield effects, also in a few raid boss debuffs
} focusType; //Any new FocusType needs to be added to the Mob::IsFocus function
#define HIGHEST_FOCUS	focusSpellHateMod //Should always be last focusType in enum

// This is actually FlyMode, from MQ2
enum GravityBehavior : int8 {
	Ground,
	Flying,
	Levitating,
	Water
};

typedef enum {	//fear states
	fearStateNotFeared = 0,
	fearStateRunning,		//I am running, hoping to find a grid at my WP
	fearStateRunningForever,	//can run straight until spell ends
	fearStateGrid,			//I am allready on a fear grid
	fearStateStuck			//I cannot move somehow...
} FearState;

struct TradeEntity;
class Trade;
enum TradeState {
	TradeNone,
	Requesting,
	Trading,
	TradeAccepted,
	TradeCompleting
};

//this is our internal representation of the BUFF struct, can put whatever we want in it
struct Buffs_Struct {
	uint16	spellid;
	uint8	casterlevel;
	uint8	realcasterlevel; // This is used when saving buffs for zoning.  
							 // When sending buffs back out, this fixes refreshing of buffs, which may have higher effective levels.
	uint16	casterid;		// Maybe change this to a pointer sometime, but gotta make sure it's 0'd when it no longer points to anything
	char	caster_name[64];
	int32	ticsremaining;
	int32	counters;
	uint32	melee_rune;
	uint32	magic_rune;
	int32	ExtraDIChance;
	int16	RootBreakChance; //Not saved to dbase
	int16	instrumentmod;
	bool	persistant_buff;
	bool	client; //True if the caster is a client
	bool	UpdateClient;
	bool	isdisc;
	bool	remove_me;
	bool	first_tic;
	int32	bufftype;
};

struct StatBonuses {
	int32	AC;
	int32	SpellAC; // for item bonus, AC provided by worn effects is treated differently than AC directly on an item
	int32	HP;
	int32	HPRegen;
	int32	HPRegenUncapped; // for item bonus
	int32	MaxHP;
	int32	ManaRegen;
	int32	ManaRegenUncapped; // for item bonus
	int32	Mana;
	int32	ATK;
	int32	ATKUncapped; // for item bonus
	//would it be worth it to create a Stat_Struct?
	int32	STR;
	int32	STRCapMod;
	int32	STA;
	int32	STACapMod;
	int32	DEX;
	int32	DEXCapMod;
	int32	AGI;
	int32	AGICapMod;
	int32	INT;
	int32	INTCapMod;
	int32	WIS;
	int32	WISCapMod;
	int32	CHA;
	int32	CHACapMod;
	int32	MR;
	int32	MRCapMod;
	int32	FR;
	int32	FRCapMod;
	int32	CR;
	int32	CRCapMod;
	int32	PR;
	int32	PRCapMod;
	int32	DR;
	int32	DRCapMod;
	uint16	DamageShieldSpellID;
	int		DamageShield;						// this is damage done to mobs that attack this
	DmgShieldType	DamageShieldType;
	int		SpellDamageShield;
	int		SpellShield;
	int		ReverseDamageShield;				// this is damage done to the mob when it attacks
	uint16	ReverseDamageShieldSpellID;
	DmgShieldType	ReverseDamageShieldType;
	int		movementspeed;
	int32	haste;
	int32	hastetype2;
	int32	hastetype3;
	float	AggroRange;							// when calculate just replace original value with this
	float	AssistRange;
	int32	skillmod[EQ::skills::HIGHEST_SKILL+1];
	int		effective_casting_level;			// Jam Fest
	int		effective_casting_level_for_fizzle_check;			// Brilliance of Ro, flappies
	int		reflect_chance;						// chance to reflect incoming spell
	uint32	singingMod;
	uint32	Amplification;						// stacks with singingMod
	uint32	brassMod;
	uint32	percussionMod;
	uint32	windMod;
	uint32	stringedMod;
	uint32	songModCap;
	int8	hatemod;

	int32	StrikeThrough;						// PoP: Strike Through %
	int32	MeleeMitigation;					//i = Shielding
	int32	MeleeMitigationEffect;				//i = Spell Effect Melee Mitigation
	int32	CriticalHitChance;					//i
	int32	CriticalSpellChance;				//i
	int32	CriticalHealChance;					//i
	int32	CriticalDoTChance;					//i
	int32	CrippBlowChance;					//
	int32	CombatAgility;						// Melee Avoidance AAs (Combat Agility, Lightning Reflexes, Physical Enhancement)
	int32	AvoidMeleeChanceEffect;				// AvoidMeleeChance Spell Effect (disciplines)
	int32	RiposteChance;						//i
	int32	DodgeChance;						//i
	int32	ParryChance;						//i
	int32	DualWieldChance;					//i
	int32	DoubleAttackChance;					//i
	int32	ResistSpellChance;					//i
	int32	ResistFearChance;					//i
	bool	Fearless;							//i
	bool	IsFeared;							//i
	bool	IsBlind;							//i
	int32	StunResist;							//i
	int32	MeleeSkillCheck;					//i
	uint8	MeleeSkillCheckSkill;
	int32	HitChance;							//HitChance/15 == % increase i = Accuracy (Item: Accuracy)
	int32	HitChanceEffect[EQ::skills::HIGHEST_SKILL+2];	//Spell effect Chance to Hit, straight percent increase
	int32	DamageModifier[EQ::skills::HIGHEST_SKILL+2];	//i
	int32	MinDamageModifier[EQ::skills::HIGHEST_SKILL+2]; //i
	int32	ExtraAttackChance;
	int32	DoTShielding;
	int32	DivineSaveChance[2];				// Second Chance (base1 = chance, base2 = spell on trigger)
	uint32	DeathSave[2];						// Death Pact [0](value = 1 partial 2 = full) [1]=slot
	int32	FlurryChance;
	int32	Accuracy[EQ::skills::HIGHEST_SKILL+2];			//Accuracy/15 == % increase	[Spell Effect: Accuracy)
	int32	HundredHands;						//extra haste, stacks with all other haste	i
	int32	MeleeLifetap;						//i
	int32	Vampirism;							//i
	int32	HealRate;							// Spell effect that influences effectiveness of heals
	int32	MaxHPChange;						// Spell Effect
	int32	DSMitigationOffHand;				// Lowers damage shield from off hand attacks.
	uint32	SpellTriggers[MAX_SPELL_TRIGGER];	// Innate/Spell/Item Spells that trigger when you cast
	uint32	SpellOnKill[MAX_SPELL_TRIGGER*3];	// Chance to proc after killing a mob
	int32	CritDmgMob[EQ::skills::HIGHEST_SKILL+2];		// All Skills + -1
	int32	SkillReuseTime[EQ::skills::HIGHEST_SKILL+1];	// Reduces skill timers
	bool	AntiGate;							// spell effect that prevents gating
	bool	MagicWeapon;						// spell effect that makes weapon magical
	int32	IncreaseBlockChance;				// overall block chance modifier
	//uint16	BlockSpellEffect[EFFECT_COUNT];		// Prevents spells with certain effects from landing on you *no longer used
	uint32	VoiceGraft;							// Stores the ID of the mob with which to talk through
	int32	CharmBreakChance;					// chance to break charm
	int32	SongRange;							// increases range of beneficial bard songs
	uint32	FocusEffects[HIGHEST_FOCUS+1];		// Stores the focus effectid for each focustype you have.
	bool	NegateEffects;						// Check if you contain a buff with negate effect. (only spellbonuses)
	uint32	MitigateMeleeRune[4];				// 0 = Mitigation value 1 = Buff Slot 2 = Max mitigation per hit 3 = Rune Amt
	uint32	MitigateSpellRune[4];				// 0 = Mitigation value 1 = Buff Slot 2 = Max mitigation per spell 3 = Rune Amt
	bool	DivineAura;							// invulnerability
	int8	Root[2];							// The lowest buff slot a root can be found. [0] = Bool if has root [1] = buff slot
	uint32	AbsorbMagicAtt[3];					// 0 = magic rune value 1 = buff slot 2 = effect slot
	uint32	MeleeRune[3];						// 0 = rune value 1 = buff slot 2 = effect slot
	bool	NegateIfCombat;						// Bool Drop buff if cast or melee
	int8	Screech;							// -1 = Will be blocked if another Screech is +(1)
	int32	AlterNPCLevel;						// amount of lvls +/-
	bool	BerserkSPA;							// berserk effect
	int32	Metabolism;							// Food/drink consumption rates.

	// AAs
	int8	BaseMovementSpeed;					// Adjust base run speed, does not stack with other movement bonuses.
	uint8	IncreaseRunSpeedCap;				// Increase max run speed above cap.
	int32	DoubleSpecialAttack;				// Chance to to perform a double special attack (ie flying kick 2x)
	int32	BindWound;							// Increase amount of HP by percent.
	int32	MaxBindWound;						// Increase max amount of HP you can bind wound.
	int32	ChannelChanceSpells;				// Modify chance to channel a spell.
	uint8	SeeInvis;							// See Invs.
	bool	FrontalBackstabMinDmg;				// Allow frontal backstabs for min damage
	uint8	ConsumeProjectile;					// Chance to not consume arrow.
	uint32	ArcheryDamageModifier;				// Increase Archery Damage by percent
	bool	SecondaryDmgInc;					// Allow off hand weapon to recieve damage bonus.
	uint32	GiveDoubleAttack;					// Allow classes to double attack with a specified chance.
	int32	SlayUndead[2];						// Allow classes to do extra damage verse undead.(base1 = rate, base2 = damage mod)
	int32	CombatStability;					// Melee damage mitigation.
	int32	DoubleRiposte;						// Chance to double riposte
	int32	GiveDoubleRiposte[3];				// 0=Regular Chance, 1=Skill Attack Chance, 2=Skill
	uint32	RaiseSkillCap[2];					// Raise a specific skill cap (1 = value, 2=skill)
	int32	Ambidexterity;						// Increase chance to duel wield by adding bonus 'skill'.
	int32	PetFlurry;							// Chance for pet to flurry.
	uint8	MasteryofPast;						// Can not fizzle spells below this level specified in value.
	bool	GivePetGroupTarget;					// All pets to recieve group buffs. (Pet Affinity)
	int32	RootBreakChance;					// Chance root will break;
	int32	UnfailingDivinity;					// Improves chance that DI will fire + increase partial heal.
	int32	OffhandRiposteFail;					// chance for opponent to fail riposte with offhand attack.
	int32	FinishingBlow[2];					// Chance to do a finishing blow for specified damage amount.
	uint32	FinishingBlowLvl[2];				// Sets max level an NPC can be affected by FB. (base1 = lv, base2= ???)
	int8	IncreaseChanceMemwipe;				// increases chance to memory wipe
	int8	CriticalMend;						// chance critical monk mend
	int32	ImprovedReclaimEnergy;				// Modifies amount of mana returned from reclaim energy
	uint32	HeadShot[2];						// Headshot AA (Massive dmg vs humaniod w/ archery) 0= ? 1= Dmg
	uint8	HSLevel;							// Max Level Headshot will be effective at.
	int32	PetMeleeMitigation;					// Add AC to owner's pet.
	bool	IllusionPersistence;				// Causes illusions not to fade.
	bool	WaterBreathing;
	uint8	BreathLevel;
	int32	FoodWater;							// SE_Hunger - stops food/water consumption
	// Quarm - XP Modifiers used for Potions
	float KillXPBonus;					// Multiply XP received for kills
	float QuestXPBonus;					// Multiply XP received for quests
	float FactionBonus;					// Multiply faction hits
	float SkillUpBonus;					// Increase skillup rate
	float TradeSkillUpBonus;		// Increase skillup rate
};

typedef struct
{
	uint16 spellID;
	uint16 chance;
	uint16 base_spellID;
	bool poison;
} tProc;

typedef enum : uint16 {	//type arguments to DoAnim
	None,											 // 0 jesus arms
	Kick,
	Piercing,	//might be piercing?
	Slashing2H,
	Weapon2H,
	Weapon1H,
	DualWield,
	Slam,		//slam & Dpunch too
	Hand2Hand,
	ShootBow,
	Unknown10,	// 10 nothing
	RoundKick,
	Unknown12,	// 12 nothing
	Unknown13,	// 13 nothing
	Falling,
	Drowning,
	FeignDeath,
	Walk,
	Slippery,
	ForwardJump,
	JumpStright, //20
	Fall,
	DuckWalk,
	LadderUp,
	Duck,
	SwimStill, // or burn
	LookAround,
	RightOn,
	Shy,
	Wave,
	Rude, //30
	Yawn,
	Unknown32,
	Sit,
	LeftTurn,
	RightTurn,
	Kneel,
	SwimMove,
	Unknown38,
	Unknown39,
	Unknown40, //40
	Unknown41,
	BuffCast,
	HealCast,
	DamageCast,
	FlyingKick,
	TigerClaw,
	EagleStrike,
	Agree,
	Amaze,
	Plead, //50
	Clap,
	Bleed,
	Blush,
	Chuckle,
	Cough,
	Cringe,
	HeadSideways,
	Dance,
	Veto,
	Glare, // 60
	Peer,
	Knelt,
	LaughHard,
	Point,
	Shrug,
	Handraise,
	Salute,
	Shiver,
	FootTap,
	WaistBow,
	Unknown71,
	LookAroundIdle,
	Unknown73,
	Mounted,
	Unknown75,
	Unknown76,
	FiddleWeapon,
	MountedFall
} DoAnimation;


typedef enum {
	petFamiliar,		//only listens to /pet get lost
	petAnimation,		//does not listen to any commands
	petOther,
	petCharmed,
	petNPCFollow,
	petHatelist,			//remain active as long something is on the hatelist. Don't listen to any commands
	petOrphan			//Pet's NPC owner died or was charmed. 
} PetType;

typedef enum {
	SingleTarget,	// causes effect to spell_target
	Projectile,		// bolt spells, flare
	AETarget,			// causes effect in aerange of target + target
	AECaster,			// causes effect in aerange of 'this'
	GroupSpell,		// causes effect to caster + target's group
	CastActUnknown
} CastAction_type;

enum {
	DMG_MISS = 0,
	DMG_BLOCK = -1,
	DMG_PARRY = -2,
	DMG_RIPOSTE = -3,
	DMG_DODGE = -4,
	DMG_INVUL = -5,
	DMG_RUNE = -6
};

enum {
	SKILLUP_UNKNOWN = 0,
	SKILLUP_SUCCESS = 1,
	SKILLUP_FAILURE = 2
};

enum {
	GridCircular,
	GridRandom10,
	GridRandom,
	GridPatrol,
	GridOneWayRepop,
	GridRand5LoS,
	GridOneWayDepop,
	GridCenterPoint,
	GridRandomCenterPoint,
	GridRandomPath
};
namespace EQ {
	class ItemInstance;
}
class Mob;
// All data associated with a single trade
class Trade
{
public:
	Trade(Mob* in_owner);
	virtual ~Trade();

	void Reset();
	void Request(uint32 mob_id);

	// Initiate a trade with another mob
	// Also puts other mob into trader mode with this mob
	void Start(uint32 mob_id, bool initiate_with=true);

	// Mob the owner is trading with
	Mob* With();

	// Add item from cursor slot to trade bucket (automatically does bag data too)
	void AddEntity(uint16 trade_slot_id, uint32 stack_size);

	uint32 GetWithID() { return with_id; }

public:
	// Object state
	TradeState state;
	int32 pp;
	int32 gp;
	int32 sp;
	int32 cp;

private:
	// Send item data for trade item to other person involved in trade
	void SendItemData(const EQ::ItemInstance* inst, int16 dest_slot_id);

	uint32 with_id;
	Mob* owner;
public:
	Mob *GetOwner() const;
};

#endif

