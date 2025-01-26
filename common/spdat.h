/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2005 EQEMu Development Team (http://eqemulator.net)

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
#ifndef SPDAT_H
#define SPDAT_H

#include "classes.h"
#include "skills.h"

#define SPELL_UNKNOWN 0xFFFF

//some spell IDs
#define SPELL_SUPERIOR_HEALING 9
#define SPELL_GATE 36
#define SPELL_PACIFY 45
#define SPELL_LAY_ON_HANDS 87
#define SPELL_HARM_TOUCH 88				// regular HT (MR resist); Both PCs and NPCs use this
#define SPELL_FEAR 229
#define SPELL_MINOR_ILLUSION 287
#define SPELL_ILLUSION_TREE 601
#define SPELL_CINDAS_CHARISMATIC_CARILLON 708
#define SPELL_CASSINDRAS_CHORUS_OF_CLARITY 723
#define SPELL_RESURRECTION_EFFECTS 756
#define SPELL_MISTWALKER 759
#define SPELL_DRAGON_CHARM 841
#define SPELL_NPC_HARM_TOUCH 929		// this is only used by a single NPC that I can see from logs (Nortlav)
#define SPELL_MANA_CONVERT 940          // manastone click effect
#define SPELL_CAZIC_TOUCH 982
#define SPELL_DIMENSIONAL_RETURN 1133
#define SPELL_CASSINDRAS_CHANT_OF_CLARITY 1287
#define SPELL_GREENMIST 1363
#define SPELL_DEATH_PEACE 1460
#define SPELL_MODULATION 1502
#define SPELL_TORPOR 1576
#define SPELL_WIND_OF_THE_NORTH 1736
#define SPELL_WIND_OF_THE_SOUTH 1737
#define SPELL_TISHANS_RELOCATION 1738
#define SPELL_MARKARS_RELOCATION 1739
#define SPELL_CALL_OF_THE_HERO 1771
#define SPELL_MANIFEST_ELEMENTS 1936
#define SPELL_MANABURN 2751
#define SPELL_FRENZIED_BURNOUT 2754
#define SPELL_LEECH_TOUCH 2766
#define SPELL_DRAGON_FORCE 2767
#define SPELL_TRANSMUTE_FLESH_TO_BONE 2772
#define SPELL_IMP_HARM_TOUCH 2774		// AA HT (unresistable)
#define SPELL_HARM_TOUCH2 2821			// unholy aura HT (disease resist)
#define SPELL_REMOVE_GREATER_CURSE 2880
#define SPELL_BALANCE_OF_THE_NAMELESS 3230
#define SPELL_AA_BOASTFUL_BELLOW 3282
#define SPELL_EPOCH_CONVICTION 3767
#define SPELL_GREENMIST_RECOURSE 3978
#define SPELL_LIFE_DRAIN 931

#define EFFECT_COUNT 12
#define MAX_SPELL_TRIGGER 12	// One for each slot(only 6 for AA since AA use 2)
#define MAX_RESISTABLE_EFFECTS 12	// Number of effects that are typcially checked agianst resists.
#define MaxLimitInclude 16 //Number(x 0.5) of focus Limiters that have inclusive checks used when calcing focus effects
#define MAX_SKILL_PROCS 4 //Number of spells to check skill procs from. (This is arbitrary) [Single spell can have multiple proc checks]


const int Z_AGGRO=10;

const int MobAISpellRange=80; // max range of buffs
const int SpellType_Nuke=1;
const int SpellType_Heal=2;
const int SpellType_Root=4;
const int SpellType_Buff=8;
const int SpellType_Escape=16;
const int SpellType_Pet=32;
const int SpellType_Lifetap=64;
const int SpellType_Snare=128;
const int SpellType_DOT=256;
const int SpellType_Dispel=512;
const int SpellType_InCombatBuff=1024;
const int SpellType_Mez=2048;
const int SpellType_Charm=4096;
const int SpellType_Slow = 8192;
const int SpellType_Debuff = 16384;
const int SpellType_Cure = 32768;
const int SpellType_Resurrect = 65536;

const int SpellTypes_Detrimental = SpellType_Nuke|SpellType_Root|SpellType_Lifetap|SpellType_Snare|SpellType_DOT|SpellType_Dispel|SpellType_Mez|SpellType_Charm|SpellType_Debuff|SpellType_Slow;
const int SpellTypes_Beneficial = SpellType_Heal|SpellType_Buff|SpellType_Escape|SpellType_Pet|SpellType_InCombatBuff|SpellType_Cure;
const int SpellTypes_Innate = SpellType_Nuke | SpellType_Lifetap | SpellType_DOT | SpellType_Dispel | SpellType_Mez | SpellType_Slow | SpellType_Debuff | SpellType_Charm | SpellType_Root;

#define SpellType_Any		0xFFFF

enum SpellAffectIndex {
	SAI_Calm			= 12, // Lull and Alliance Spells
	SAI_Dispell_Sight	= 14, // Dispells and Spells like Bind Sight
	SAI_Memory_Blur		= 27,
	SAI_Calm_Song		= 43 // Lull and Alliance Songs
};
enum RESISTTYPE
{
	RESIST_NONE = 0,
	RESIST_MAGIC = 1,
	RESIST_FIRE = 2,
	RESIST_COLD = 3,
	RESIST_POISON = 4,
	RESIST_DISEASE = 5
};

typedef enum {
	Beneficial = -1,
	Detrimental = -2
} SpellModifierType;

//Target Type IDs
typedef enum {
/* 01 */	ST_TargetOptional = 0x01,
/* 02 */	ST_AEClientV1 = 0x02,
/* 03 */	ST_GroupTeleport = 0x03,
/* 04 */	ST_AECaster = 0x04,
/* 05 */	ST_Target = 0x05,
/* 06 */	ST_Self = 0x06,
/* 07 */	// NOT USED
/* 08 */	ST_AETarget = 0x08,
/* 09 */	ST_Animal = 0x09,
/* 10 */	ST_Undead = 0x0a,
/* 11 */	ST_Summoned = 0x0b,
/* 12 */	// NOT USED
/* 13 */	ST_Tap = 0x0d,
/* 14 */	ST_Pet = 0x0e,
/* 15 */	ST_Corpse = 0x0f,
/* 16 */	ST_Plant = 0x10,
/* 17 */	ST_UberGiant = 0x11, //special giant
/* 18 */	ST_UberDragon = 0x12, //special dragon
/* 19 */	// NOT USED
/* 20 */	ST_TargetAETap = 0x14,
/* 21 */	// NOT USED
/* 22 */	// NOT USED
/* 23 */	// NOT USED
/* 24 */	ST_UndeadAE = 0x18,
/* 25 */	ST_SummonedAE = 0x19,
/* 26 */	// NOT USED
/* 27 */	// NOT USED
/* 28 */	// NOT USED
/* 29 */	// NOT USED
/* 30 */	// NOT USED
/* 31 */	// NOT USED
/* 32 */	// NOT USED
/* 33 */	// NOT USED
/* 34 */	// NOT USED
/* 35 */	// NOT USED
/* 36 */	// NOT USED
/* 37 */	// NOT USED
/* 38 */	// NOT USED
/* 39 */	// NOT USED
/* 40 */	ST_AEBard = 0x28,
/* 41 */	ST_Group = 0x29,
/* 42 */	// NOT USED
/* 43 */	ST_ProjectIllusion = 0x2b, // Not found in spell data, used internally.
} SpellTargetType;

typedef enum {
	DS_DECAY = 244,
	DS_CHILLED = 245,
	DS_FREEZING = 246,
	DS_TORMENT = 247,
	DS_BURN = 248,
	DS_THORNS = 249
} DmgShieldType;

//Spell Effect IDs
// full listing: https://forums.station.sony.com/eq/index.php?threads/enumerated-spa-list.206288/
// mirror: http://pastebin.com/MYeQqGwe
#define SE_CurrentHP					0	// implemented - Heals and nukes, repeates every tic if in a buff
#define SE_ArmorClass					1	// implemented	
#define SE_ATK							2	// implemented
#define SE_MovementSpeed				3	// implemented - SoW, SoC, etc
#define SE_STR							4	// implemented
#define SE_DEX							5	// implemented
#define SE_AGI							6	// implemented
#define SE_STA							7	// implemented
#define SE_INT							8	// implemented
#define SE_WIS							9	// implemented
#define SE_CHA							10	// implemented - used as a spacer
#define SE_AttackSpeed					11	// implemented
#define SE_Invisibility					12	// implemented - TO DO: Implemented Invisiblity Levels
#define SE_SeeInvis						13	// implemented - TO DO: Implemented See Invisiblity Levels
#define SE_WaterBreathing				14	// implemented
#define SE_CurrentMana					15	// implemented
//#define SE_NPCFrenzy					16	// not used
//#define SE_NPCAwareness				17	// not used
#define SE_Lull							18	// implemented - Reaction Radius
#define SE_AddFaction					19	// implemented - Alliance line
#define SE_Blind						20	// implemented
#define SE_Stun							21	// implemented
#define SE_Charm						22	// implemented
#define SE_Fear							23	// implemented
#define SE_Stamina						24	// implemented - Invigor and such
#define SE_BindAffinity					25	// implemented - TO DO: Implement 2nd and 3rd Recall (value 2,3 ect). Sets additional bind points.
#define SE_Gate							26	// implemented - Gate to bind point
#define SE_CancelMagic					27	// implemented
#define SE_InvisVsUndead				28	// implemented
#define SE_InvisVsAnimals				29	// implemented
#define SE_ChangeFrenzyRad				30	// implemented - Pacify
#define SE_Mez							31	// implemented
#define SE_SummonItem					32	// implemented
#define SE_SummonPet					33	// implemented
//#define SE_Confuse					34	// not used (Nimbus of Temporal Rifting) ?
#define SE_DiseaseCounter				35	// implemented
#define SE_PoisonCounter				36	// implemented
//#define SE_DetectHostile				37	// not used
//#define SE_DetectMagic				38	// not used
//#define SE_DetectPoison				39	// not used
#define SE_DivineAura					40	// implemented
#define SE_Destroy						41	// implemented - Disintegrate, Banishment of Shadows
#define SE_ShadowStep					42	// implemented
#define SE_Berserk						43	// implemented (*not used in any known live spell) Makes client 'Berserk' giving crip blow chance.
#define SE_Lycanthropy					44	// implemented 
#define SE_Vampirism					45	// implemented (*not used in any known live spell) Stackable lifetap from melee.
#define SE_ResistFire					46	// implemented
#define SE_ResistCold					47	// implemented
#define SE_ResistPoison					48	// implemented
#define SE_ResistDisease				49	// implemented
#define SE_ResistMagic					50	// implemented
//#define SE_DetectTraps				51	// not used
#define SE_SenseDead					52	// implemented
#define SE_SenseSummoned				53	// implemented
#define SE_SenseAnimals					54	// implemented
#define SE_Rune							55	// implemented
#define SE_TrueNorth					56	// implemented
#define SE_Levitate						57	// implemented
#define SE_Illusion						58	// implemented
#define SE_DamageShield					59	// implemented
//#define SE_TransferItem				60	// not used
#define SE_Identify						61	// implemented
//#define SE_ItemID						62	// not used
#define SE_WipeHateList					63	// implemented
#define SE_SpinTarget					64	// implemented - TO DO: Not sure stun portion is working correctly
#define SE_InfraVision					65	// implemented
#define SE_UltraVision					66	// implemented
#define SE_EyeOfZomm					67	// implemented
#define SE_ReclaimPet					68	// implemented
#define SE_TotalHP						69	// implemented
//#define SE_CorpseBomb					70	// not used
#define SE_NecPet						71	// implemented
//#define SE_PreserveCorpse				72	// not used
#define SE_BindSight					73	// implemented
#define SE_FeignDeath					74	// implemented
#define SE_VoiceGraft					75	// implemented
#define SE_Sentinel						76	// *not implemented?(just seems to send a message)
#define SE_LocateCorpse					77	// implemented
#define SE_AbsorbMagicAtt				78	// implemented - Rune for spells
#define SE_CurrentHPOnce				79	// implemented - Heals and nukes, non-repeating if in a buff
//#define SE_EnchantLight				80	// not used
#define SE_Revive						81	// implemented - Resurrect
#define SE_SummonPC						82	// implemented
#define SE_Teleport						83	// implemented
#define SE_TossUp						84	// implemented - Gravity Flux
#define SE_WeaponProc					85	// implemented - i.e. Call of Fire
#define SE_Harmony						86	// implemented
#define SE_MagnifyVision				87	// implemented - Telescope
#define SE_Succor						88	// implemented - Evacuate/Succor lines
#define SE_ModelSize					89	// implemented - Shrink, Growth
//#define SE_Cloak						90	// *not implemented - Used in only 2 spells
#define SE_SummonCorpse					91	// implemented
#define SE_InstantHate					92	// implemented - add hate
#define SE_StopRain						93	// implemented - Wake of Karana
#define SE_NegateIfCombat				94	// implemented 
#define SE_Sacrifice					95	// implemented
#define SE_Silence						96	// implemented
#define SE_ManaPool						97	// implemented
#define SE_AttackSpeed2					98	// implemented - Melody of Ervaj
#define SE_Root							99	// implemented
#define SE_HealOverTime					100	// implemented
#define SE_CompleteHeal					101	// implemented
#define SE_Fearless						102	// implemented - Valiant Companion
#define SE_CallPet						103	// implemented - Summon Companion
#define SE_Translocate					104	// implemented
#define SE_AntiGate						105	// implemented - Translocational Anchor
#define SE_SummonBSTPet					106	// implemented
#define SE_AlterNPCLevel				107	// implemented - not used on live
#define SE_Familiar						108	// implemented
#define SE_SummonItemIntoBag			109	// implemented - summons stuff into container
#define SE_IncreaseArchery				110	// implemented
#define SE_ResistAll					111	// implemented - Note: Physical Resists are not modified by this effect.
#define SE_CastingLevel					112	// implemented
#define	SE_SummonHorse					113	// implemented
#define SE_ChangeAggro					114	// implemented - All hate sources multiplier; e.g. SK buffs, spell casting subtlety, raid boss debuffs, war time BP
#define SE_Hunger						115	// implemented - Song of Sustenance
#define SE_CurseCounter					116	// implemented
#define SE_MagicWeapon					117	// implemented - makes weapon magical
#define SE_Amplification				118	// implemented - Harmonize/Amplification (stacks with other singing mods)
#define SE_AttackSpeed3					119	// implemented
#define SE_HealRate						120	// implemented - reduces healing by a %
#define SE_ReverseDS					121 // implemented
//#define SE_ReduceSkill				122	// not used
#define SE_Screech						123	// implemented Spell Blocker(If have buff with value +1 will block any effect with -1)
#define SE_ImprovedDamage				124 // implemented
#define SE_ImprovedHeal					125 // implemented
#define SE_SpellResistReduction			126 // implemented
#define SE_IncreaseSpellHaste			127 // implemented
#define SE_IncreaseSpellDuration		128 // implemented
#define SE_IncreaseRange				129 // implemented
#define SE_SpellHateMod					130 // implemented - Spell and Bash only hate multiplier focus; mostly from Furious Bash shield effects, also in a few raid boss debuffs
#define SE_ReduceReagentCost			131 // implemented
#define SE_ReduceManaCost				132 // implemented
#define SE_FcStunTimeMod				133	// implemented - Modify duration of stuns.
#define SE_LimitMaxLevel				134 // implemented
#define SE_LimitResist					135 // implemented
#define SE_LimitTarget					136 // implemented
#define SE_LimitEffect					137 // implemented
#define SE_LimitSpellType				138 // implemented
#define SE_LimitSpell					139 // implemented
#define SE_LimitMinDur					140 // implemented
#define SE_LimitInstant					141 // implemented
#define SE_LimitMinLevel				142 // implemented
#define SE_LimitCastTimeMin				143 // implemented
#define SE_LimitCastTimeMax				144	// implemented (*not used in any known live spell)
#define SE_Teleport2					145	// implemented - Banishment of the Pantheon
//#define SE_ElectricityResist			146	// *not implemented (Lightning Rod: 23233) 
#define SE_PercentalHeal				147 // implemented
#define SE_StackingCommand_Block		148 // implemented?
#define SE_StackingCommand_Overwrite	149 // implemented?
#define SE_DeathSave					150 // implemented
#define SE_SuspendPet					151	// *not implemented as bonus
#define SE_TemporaryPets				152	// implemented
#define SE_BalanceHP					153 // implemented
#define SE_DispelDetrimental			154 // implemented
#define SE_SpellCritDmgIncrease			155 // implemented - no known live spells use this currently
#define SE_IllusionCopy					156	// implemented - Deception
#define SE_SpellDamageShield			157	// implemented - Petrad's Protection
#define SE_Reflect						158 // implemented
#define SE_AllStats						159	// implemented
//#define SE_MakeDrunk					160 // *not implemented - Effect works entirely client side (Should check against tolerance)
#define SE_MitigateSpellDamage			161	// implemented - rune with max value
#define SE_MitigateMeleeDamage			162	// implemented - rune with max value
#define SE_NegateAttacks				163	// implemented
#define SE_PetPowerIncrease				167 // implemented
#define SE_MeleeMitigation				168	// implemented
#define SE_CriticalHitChance			169	// implemented
#define SE_SpellCritChance				170	// implemented
#define SE_CrippBlowChance				171	// implemented
#define SE_AvoidMeleeChance				172	// implemented
#define SE_RiposteChance				173	// implemented
#define SE_DodgeChance					174	// implemented
#define SE_ParryChance					175	// implemented
#define SE_DualWieldChance				176	// implemented
#define SE_DoubleAttackChance			177	// implemented
#define SE_MeleeLifetap					178	// implemented
#define SE_AllInstrumentMod				179	// implemented
#define SE_ResistSpellChance			180	// implemented
#define SE_ResistFearChance				181	// implemented
#define SE_HundredHands					182	// implemented
#define SE_MeleeSkillCheck				183	// implemented
#define SE_HitChance					184	// implemented
#define SE_DamageModifier				185	// implemented
#define SE_MinDamageModifier			186	// implemented
#define SE_BalanceMana					187	// implemented - Balances party mana
#define SE_IncreaseBlockChance			188	// implemented
#define SE_CurrentEndurance				189	// implemented
#define SE_EndurancePool				190	// implemented
#define SE_Amnesia						191	// implemented - Silence vs Melee Effect
#define SE_Hate							192	// implemented - Instant and hate over time.
#define SE_SkillAttack					193	// implemented
#define SE_FadingMemories				194	// implemented
#define SE_StunResist					195	// implemented
#define SE_StrikeThrough				196	// implemented
#define SE_SkillDamageTaken				197	// unused
#define SE_CurrentEnduranceOnce			198	// implemented
#define SE_Taunt						199	// implemented - % chance to taunt the target
#define SE_ProcChance					200	// implemented
#define SE_RangedProc					201	// implemented
#define SE_IllusionOther				202	// implemented - Project Illusion
#define SE_MassGroupBuff				203	// implemented
//#define SE_GroupFearImmunity			204	// implemented as AA Action.
#define SE_Rampage						205	// implemented
#define SE_AETaunt						206	// implemented
//#define SE_FleshToBone				207	// implemented as hardcoded effect in SpellOnTarget()
//#define SE_PurgePoison				208	// implemented as AA Action.
#define SE_DispelBeneficial				209 // implemented
//#define SE_PetShield					210	// *not implemented
#define SE_AEMelee						211	// implemented TO DO: Implement to allow NPC use (client only atm).
#define SE_FrenziedDevastation			212	// implemented - increase spell criticals + all DD spells cast 2x mana.
#define SE_PetMaxHP						213	// implemented[AA] - increases the maximum hit points of your pet
#define SE_MaxHPChange					214	// implemented
#define SE_PetAvoidance					215	// implemented[AA] - increases pet ability to avoid melee damage
#define SE_Accuracy						216	// implemented
#define SE_HeadShot						217	// implemented - ability to head shot (base2 = damage)
#define SE_PetCriticalHit				218 // implemented[AA] - gives pets a baseline critical hit chance
#define SE_SlayUndead					219	// implemented - Allow extra damage against undead (base1 = rate, base2 = damage mod).
#define SE_SkillDamageAmount			220	// after our era
#define SE_Packrat						221 // Not used by our client.
#define SE_BlockBehind					222	// after our era; GoD AA
#define SE_DoubleRiposte				223	// implemented - Chance to double riposte [not used on live]
#define	SE_GiveDoubleRiposte			224 // implemented[AA]
#define SE_GiveDoubleAttack				225	// implemented[AA] - Allow any class to double attack with set chance.
#define SE_TwoHandBash					226 // *not implemented as bonus
#define SE_ReduceSkillTimer				227	// implemented
//#define SE_ReduceFallDamage			228	// not implented as bonus - reduce the damage that you take from falling
#define SE_PersistantCasting			229 // implemented
//#define SE_ExtendedShielding			230	// not used as bonus - increase range of /shield ability
#define SE_StunBashChance				231	// implemented - increase chance to stun from bash.
#define SE_DivineSave					232	// implemented (base1 == % chance on death to insta-res) (base2 == spell cast on save)
#define SE_Metabolism					233	// implemented - Modifies food/drink consumption rates.
//#define SE_ReduceApplyPoisonTime		234	// not implemented as bonus - reduces the time to apply poison
#define	SE_ChannelChanceSpells			235 // implemented[AA] - chance to channel from SPELLS *No longer used on live.
//#define SE_FreePet					236	// not used
#define SE_GivePetGroupTarget			237 // implemented[AA] - (Pet Affinity)
#define SE_IllusionPersistence			238	// implemented - lends persistence to your illusionary disguises, causing them to last until you die or the illusion is forcibly removed.
//#define SE_FeignedCastOnChance		239	// *not implemented as bonus - ability gives you an increasing chance for your feigned deaths to not be revealed by spells cast upon you.
//#define SE_StringUnbreakable			240	// not used [Likely related to above - you become immune to feign breaking on a resisted spell and have a good chance of feigning through a spell that successfully lands upon you.]
#define SE_ImprovedReclaimEnergy		241	// implemented - increase the amount of mana returned to you when reclaiming your pet.
#define SE_IncreaseChanceMemwipe		242	// implemented - increases the chance to wipe hate with memory blurr
#define SE_CharmBreakChance				243	// implemented - Total Domination
#define	SE_RootBreakChance				244	// implemented[AA] reduce the chance that your root will break.
//#define SE_TrapCircumvention			245	// *not implemented[AA] - decreases the chance that you will set off a trap when opening a chest
#define SE_SetBreathLevel				246 // *not implemented as bonus
#define SE_RaiseSkillCap				247	// *not implemented[AA] - adds skill over the skill cap.
//#define SE_SecondaryForte				248 // not implemented as bonus(gives you a 2nd specialize skill that can go past 50 to 100)
#define SE_SecondaryDmgInc				249 // implemented[AA] Allows off hand weapon to recieve a damage bonus (Sinister Strikes)
#define SE_SpellProcChance				250	// implemented - Increase chance to proc from melee proc spells (ie Spirit of Panther)
#define SE_ConsumeProjectile			251	// implemented[AA] - chance to not consume an arrow (ConsumeProjectile = 100)
#define SE_FrontalBackstabChance		252	// implemented[AA] - chance to perform a full damage backstab from front.
#define SE_FrontalBackstabMinDmg		253	// implemented[AA] - allow a frontal backstab for mininum damage.
#define SE_Blank						254 // implemented
//#define SE_ShieldDuration				255	// not implemented as bonus - increases duration of /shield
//#define SE_ShroudofStealth			256	// not implemented as bonus - rogue improved invs
//#define SE_PetDiscipline				257 // not implemented as bonus - /pet hold
#define SE_TripleBackstab				258 // implemented[AA] - chance to perform a triple backstab
#define SE_CombatStability				259 // implemented[AA] - damage mitigation
#define SE_AddSingingMod				260 // implemented[AA] - Instrument/Singing Mastery, base1 is the mod, base2 is the ItemType
#define SE_SongModCap					261	// implemented[AA] - Song Mod cap increase (no longer used on live)
#define SE_RaiseStatCap					262 // implemented
//#define SE_TradeSkillMastery			263	// not implemented - lets you raise more than one tradeskill above master.
//#define SE_HastenedAASkill			264 // not implemented as bonus - Use redux field in aa_actions table for this effect
#define SE_MasteryofPast				265 // implemented[AA] - Spells less than effect values level can not be fizzled
#define SE_ExtraAttackChance			266 // implemented - increase chance to score an extra attack with a 2-Handed Weapon.
#define SE_PetDiscipline2				267 // *not implemented - /pet focus, /pet no cast
//#define SE_ReduceTradeskillFail		268 // *not implemented? - reduces chance to fail with given tradeskill by a percent chance
#define SE_MaxBindWound					269	// implemented[AA] - Increase max HP you can bind wound.
#define SE_BardSongRange				270	// implemented[AA] - increase range of beneficial bard songs (Sionachie's Crescendo)
#define SE_BaseMovementSpeed			271 // implemented[AA] - mods basemove speed, doesn't stack with other move mods
#define SE_CastingLevel2				272 // implemented
#define SE_CriticalDoTChance			273	// implemented
#define SE_CriticalHealChance			274	// implemented
#define SE_CriticalMend					275	// implemented[AA] - chance to critical monk mend
#define SE_Ambidexterity				276 // implemented[AA] - increase chance to duel weild by adding bonus 'skill'
#define SE_UnfailingDivinity			277	// implemented[AA] - ability grants your Death Pact-type spells a second chance to successfully heal their target, also can cause said spells to do a portion of their healing value even on a complete failure.
#define	SE_FinishingBlow				278 // implemented[AA] - chance to do massive damage under 10% HP (base1 = chance, base2 = damage)
#define SE_Flurry						279	// implemented
#define SE_PetFlurry					280 // implemented[AA]
//#define SE_FeignedMinion				281	// *not implemented[AA] ability allows you to instruct your pet to feign death via the '/pet feign' command. value = succeed chance
#define SE_ImprovedBindWound			282	// implemented[AA] - increase bind wound amount by percent.
#define SE_DoubleSpecialAttack			283	// implemented[AA] - Chance to perform second special attack as monk
//#define SE_LoHSetHeal					284	// not used
//#define SE_NimbleEvasion				285	// *not implemented - base1 = 100 for max
#define SE_FcDamageAmt					286	// implemented - adds direct spell damage
#define SE_SpellDurationIncByTic		287 // implemented
#define SE_SpecialAttackKBProc			288	// only dragon punch/tail rake uses this in our era, so it was hardcoded instead of using a bonus
#define SE_CastOnFadeEffect				289 // implemented - Triggers only if fades after natural duration.
#define SE_IncreaseRunSpeedCap			290	// implemented[AA] - increases run speed over the hard cap
#define SE_Purify						291 // implemented - Removes determental effects
#define SE_StrikeThrough2				292	// implemented[AA] - increasing chance of bypassing an opponent's special defenses, such as dodge, block, parry, and riposte.
#define SE_FrontalStunResist			293	// after our era
#define SE_CriticalSpellChance			294 // implemented - increase chance to critical hit and critical damage modifier.
//#define SE_ReduceTimerSpecial			295	// not used
#define SE_FcSpellVulnerability			296	// implemented - increase in incoming spell damage
#define SE_FcDamageAmtIncoming			297 // implemented - debuff that adds points damage to spells cast on target (focus effect).
#define SE_ChangeHeight					298	// implemented
#define SE_WakeTheDead					299	// implemented
#define SE_Doppelganger					300	// implemented
#define SE_ArcheryDamageModifier		301	// implemented[AA] - increase archery damage by percent
#define SE_FcDamagePctCrit				302	// implemented - this seems misnamed?  seems to be a spell dmg multiplier that works on everything, not just crits; only used for SK Soul Abrasion AA
#define SE_FcDamageAmtCrit				303	// implemented - adds direct spell damage
#define SE_OffhandRiposteFail			304 // implemented as bonus - enemy cannot riposte offhand attacks
#define SE_MitigateDamageShield			305 // implemented - off hand attacks only (Shielding Resistance)
//#define SE_ArmyOfTheDead				306 // *not implemented NecroAA - This ability calls up to five shades of nearby corpses back to life to serve the necromancer. The soulless abominations will mindlessly fight the target until called back to the afterlife some time later. The first rank summons up to three shades that serve for 60 seconds, and each additional rank adds one more possible shade and increases their duration by 15 seconds
//#define SE_Appraisal					307 // *not implemented Rogue AA - This ability allows you to estimate the selling price of an item you are holding on your cursor.
#define SE_SuspendMinion				308 // implemented
#define SE_GateCastersBindpoint			309 // implemented - Gate to casters bind point
#define SE_ReduceReuseTimer				310 // implemented
#define SE_LimitCombatSkills			311 // implemented - Excludes focus from procs (except if proc is a memorizable spell)
#define SE_ShieldBlock					320	// unsued; well after PoP
#define SE_HeadShotLevel				346	// implemented[AA] - HeadShot max level to kill
#define SE_LimitSpellGroup				385	// implemented - Limits to spell group(ie type 3 reuse reduction augs that are class specific and thus all share s SG)
#define SE_FinishingBlowLvl				440 // implemented[AA] - Sets the level Finishing blow can be triggered on an NPC
#define SE_KillXPBonus				500 // Quarm - Kill XP Multiplier
#define SE_QuestXPBonus				501 // Quarm - Quest XP Mutiplier
#define SE_FactionBonus				502 // Quarm - Faction Increase Multiplier
#define SE_SkillUpBonus				503 // Quarm - SkillUp Rate Multiplier
#define SE_TradeSkillUpBonus			504 // Quarm - TradeSkillUp Rate Multiplier
// LAST


#define DF_Permanent			50

// Disciplines
#define disc_aggressive					1
#define disc_precision					2
#define disc_defensive					3
#define disc_evasive					4
#define disc_ashenhand					5
#define disc_furious					6  //whirlwind, counterattack
#define disc_stonestance				11 //protectivespirit
#define disc_thunderkick				12
#define disc_fortitude					13 //voiddance
#define disc_fellstrike					14 //bestialrage, innerflame, duelist
#define disc_hundredfist				15 //blindingspeed
#define disc_charge						16 //deadeye
#define disc_mightystrike				17
#define disc_nimble						19
#define disc_silentfist					20
#define disc_kinesthetics				21
#define disc_holyforge					22
#define disc_sanctification				23
#define disc_trueshot					24
#define disc_weaponshield				25
#define disc_unholyaura					26
#define disc_leechcurse					27
#define disc_deftdance					28
#define disc_puretone					29
#define disc_resistant					30
#define disc_fearless					31

// solar: note this struct is historical, we don't actually need it to be
// aligned to anything, but for maintaining it it is kept in the order that
// the fields in the text file are. the numbering is not offset, but field
// number. note that the id field is counted as 0, this way the numbers
// here match the numbers given to sep in the loading function net.cpp
//
#define SPELL_LOAD_FIELD_COUNT 186

struct SPDat_Spell_Struct
{
/* 000 */	int id;	// not used
/* 001 */	char name[64]; // Name of the spell
/* 002 */	char player_1[32]; // "PLAYER_1"
/* 003 */	char teleport_zone[64];	// Teleport zone, pet name summoned, or item summoned
/* 004 */	char you_cast[64]; // Message when you cast
/* 005 */	char other_casts[64]; // Message when other casts
/* 006 */	char cast_on_you[64]; // Message when spell is cast on you
/* 007 */	char cast_on_other[64]; // Message when spell is cast on someone else
/* 008 */	char spell_fades[64]; // Spell fades
/* 009 */	float range;
/* 010 */	float aoerange;
/* 011 */	float pushback;
/* 012 */	float pushup;
/* 013 */	uint32 cast_time; // Cast time
/* 014 */	uint32 recovery_time; // Recovery time
/* 015 */	uint32 recast_time; // Recast same spell time
/* 016 */	uint32 buffdurationformula;
/* 017 */	uint32 buffduration;
/* 018 */	uint32 AEDuration;	// sentinel, rain of something
/* 019 */	uint16 mana; // Mana Used
/* 020 */	int base[EFFECT_COUNT];	//various purposes
/* 032 */	int base2[EFFECT_COUNT]; //various purposes
/* 044 */	int32 max[EFFECT_COUNT];
/* 056 */	uint16 icon; // Spell icon
/* 057 */	uint16 memicon; // Icon on membarthing
/* 058 */	int32 components[4]; // reagents
/* 062 */	int component_counts[4]; // amount of regents used
/* 066 */	int NoexpendReagent[4];	// focus items (Need but not used; Flame Lick has a Fire Beetle Eye focus.)
											// If it is a number between 1-4 it means components[number] is a focus and not to expend it
											// If it is a valid itemid it means this item is a focus as well
/* 070 */	uint16 formula[EFFECT_COUNT]; // Spell's value formula
/* 082 */	int LightType; // probaly another effecttype flag
/* 083 */	int8 goodEffect; //0=detrimental, 1=Beneficial, 2=Beneficial, Group Only
/* 084 */	int Activated; // probably another effecttype flag
/* 085 */	int resisttype;
/* 086 */	int effectid[EFFECT_COUNT];	// Spell's effects
/* 098 */	SpellTargetType targettype;	// Spell's Target
/* 099 */	int basediff; // base difficulty fizzle adjustment
/* 100 */	EQ::skills::SkillType skill;
/* 101 */	int8 zonetype; // 01=Outdoors, 02=dungeons, ff=Any
/* 102 */	int8 EnvironmentType;
/* 103 */	int8 TimeOfDay;
/* 104 */	uint8 classes[Class::PLAYER_CLASS_COUNT]; // Classes, and their min levels
/* 119 */	uint8 CastingAnim;
/* 120 */	uint8 TargetAnim;
/* 121 */	uint32 TravelType;
/* 122 */	uint16 SpellAffectIndex;
/* 123 */	int8 disallow_sit; // 124: high-end Yaulp spells (V, VI, VII, VIII [Rk 1, 2, & 3], & Gallenite's Bark of Fury
/* 124 */	int8 deity_agnostic;// 125: Words of the Skeptic
/* 125 */	int8 deities[16];	// Deity check. 201 - 216 per http://www.eqemulator.net/wiki/wikka.php?wakka=DeityList
										// -1: Restrict to Deity; 1: Restrict to Deity, but only used on non-Live (Test Server "Blessing of ...") spells; 0: Don't restrict
/* 141 */	int8 npc_no_cast;	// 142: between 0 & 100
/* 142 */	int8 ai_pt_bonus;	// 143: always set to 0
/* 143 */	int16 new_icon;	// Spell icon used by the client in uifiles/default/spells??.tga, both for spell gems & buff window. Looks to depreciate icon & memicon
/* 144 */	int16 spellanim; // Doesn't look like it's the same as #doanim, so not sure what this is
/* 145 */	int8 uninterruptable;	// Looks like anything != 0 is uninterruptable. Values are mostly -1, 0, & 1 (Fetid Breath = 90?)
/* 146 */	int16 ResistDiff;
/* 147 */	int8 dot_stacking_exempt; // If 1 doesn't stack with self cast by others. If -1 (not implemented) doesn't stack with same effect (???)
/* 148 */	int deletable;
/* 149 */	uint16 RecourseLink;
/* 150 */	bool no_partial_resist;	// 151: -1, 0, or 1
/* 151 */   int8 small_targets_only; // 152 & 153: all set to 0
/* 152 */   int8 use_persistent_particles;
/* 153 */	int8 short_buff_box;	// != 0, goes to short buff box.
/* 154 */	int descnum; // eqstr of description of spell
/* 155 */	int typedescnum; // eqstr of type description
/* 156 */	int effectdescnum; // eqstr of effect description
/* 157 */   int effectdescnum2;
/* 158 */	bool npc_no_los;

/* Below are custom fields for our client*/
/* 159 */	bool reflectable;
/* 160 */	int resist_per_level;
/* 161 */	int resist_cap;
/* 162 */	int EndurCost;
/* 163 */	int8 EndurTimerIndex;
/* 164 */	bool IsDisciplineBuff; //Is a Discipline spell
/* 165 */	int HateAdded;
/* 166 */	int EndurUpkeep;
/* 167 */	int pvpresistbase;
/* 168 */	int pvpresistcalc;
/* 169 */	int pvpresistcap;
/* 170 */	int spell_category;
/* 171 */	int pvp_duration;
/* 172 */	int pvp_duration_cap;
/* 173 */	bool cast_not_standing;
/* 174 */	int8 can_mgb; // 0=no, -1 or 1 = yes
/* 175 */	int dispel_flag;
/* 176 */	int npc_category;
/* 177 */	int npc_usefulness;
/* 178 */	bool wear_off_message;
/* 179 */	bool suspendable; // buff is suspended in suspended buff zones
/* 180 */	int spellgroup;
/* 181 */	bool allow_spellscribe;
/* 182 */	bool AllowRest;
/* 183 */	int custom_icon; // Used by spell guide
/* 184 */	bool not_player_spell;
/* 185 */	bool disabled;
/* 186 */	bool persist_through_death; // Quarm addition - Used by BuffFadeNonPersistDeath()
			uint8 DamageShieldType; // This field does not exist in spells_us.txt
			int min_castinglevel;
			bool bardsong;
			int32 poison_counters;
			int32 disease_counters;
			int32 curse_counters;
			bool manatapspell;
			bool hasrecourse;
			bool contains_se_currentmana;
};

struct SpellModifier_Struct {
	int     client_cast;     // Client 1 or NPC 0 
	int     spell_match_id;  // Spell ID or -1 for beneficial and -2 for detrimental
	int     zone_id;         // Impact zone ID or 0 for all
	int     tic_duration;    // Absolute tic duration
	float   tic_multiplier;  // Tic multiplier
	int     tic_add;         // Tic count added
};

extern const SPDat_Spell_Struct* spells;
extern std::map<std::tuple<int,int,int>, SpellModifier_Struct> spellModifiers;
extern int32 SPDAT_RECORDS;

bool IsTargetableAESpell(uint16 spell_id);
bool IsSacrificeSpell(uint16 spell_id);
bool IsLichSpell(uint16 spell_id);
bool IsLifetapSpell(uint16 spell_id);
bool IsMezSpell(uint16 spell_id);
int16 GetBaseValue(uint16 spell_id, uint16 effect);
bool IsStunSpell(uint16 spell_id);
bool IsSlowSpell(uint16 spell_id);
bool IsHasteSpell(uint16 spell_id);
bool IsHarmonySpell(uint16 spell_id);
bool IsPacifySpell(uint16 spell_id);
bool IsLullSpell(uint16 spell_id);
bool IsMemBlurSpell(uint16 spell_id);
bool IsBardAOEDamageSpell(uint16 spell_id);
bool IsAEMemBlurSpell(uint16 spell_id);
bool IsCrowdControlSpell(uint16 spell_id);
bool IsPercentalHealSpell(uint16 spell_id);
bool IsGroupOnlySpell(uint16 spell_id);
bool IsBeneficialSpell(uint16 spell_id);
bool IsDetrimentalSpell(uint16 spell_id);
bool IsNeutralSpell(uint16 spell_id);
bool IsInvulnerabilitySpell(uint16 spell_id);
bool IsCHDurationSpell(uint16 spell_id);
bool IsPoisonCounterSpell(uint16 spell_id);
bool IsDiseaseCounterSpell(uint16 spell_id);
bool IsSummonItemSpell(uint16 spell_id);
bool IsSummonSkeletonSpell(uint16 spell_id);
bool IsSummonPetSpell(uint16 spell_id);
bool IsSummonPCSpell(uint16 spell_id);
bool IsCharmSpell(uint16 spell_id);
bool IsDireCharmSpell(uint16 spell_id);
bool IsRootSpell(uint16 spell_id);
bool IsBlindSpell(uint16 spell_id);
bool IsEffectHitpointsSpell(uint16 spell_id);
bool IsReduceCastTimeSpell(uint16 spell_id);
bool IsIncreaseDurationSpell(uint16 spell_id);
bool IsReduceManaSpell(uint16 spell_id);
bool IsExtRangeSpell(uint16 spell_id);
bool IsImprovedHealingSpell(uint16 spell_id);
bool IsHealingSpell(uint16 spell_id);
bool IsImprovedDamageSpell(uint16 spell_id);
bool IsBindSightSpell(uint16 spell_id);
bool IsAEDurationSpell(uint16 spell_id);
bool IsPureNukeSpell(uint16 spell_id);
bool IsAENukeSpell(uint16 spell_id);
bool IsPBAENukeSpell(uint16 spell_id);
bool IsAERainNukeSpell(uint16 spell_id);
bool IsPureDispelSpell(uint16 spell_id);
bool IsDispelSpell(uint16 spell_id);
bool IsPartialCapableSpell(uint16 spell_id);
bool IsResistableSpell(uint16 spell_id);
bool IsGroupSpell(uint16 spell_id);
bool IsTGBCompatibleSpell(uint16 spell_id);
bool IsBardSong(uint16 spell_id);
bool IsEffectInSpell(uint16 spellid, int effect);
bool IsBlankSpellEffect(uint16 spellid, int effect_index);
bool IsSpellEffectBlocked(uint16 spellid1, uint16 spellid2, uint16 effectid, int value);
bool IsValidSpell(uint16 spellid);
bool IsSummonSpell(uint16 spellid);
bool IsEvacSpell(uint16 spellid);
bool IsDamageSpell(uint16 spellid);
bool IsDirectDamageSpell(uint16 spellid);
bool HasDirectDamageEffect(uint16 spellid);
bool IsDOTSpell(uint16 spellid);
bool IsFearSpell(uint16 spellid);
bool IsCureSpell(uint16 spellid);
bool IsRegeantFocus(uint16 spellid);
bool IsBoltSpell(uint16 spellid);
bool RequiresComponents(uint16 spellid);
int GetSpellEffectIndex(uint16 spell_id, int effect);
int GetSpellEffectIndex_PatchV1(uint16 spell_id, int effect); // Used in FindAffectSlot_PatchV1() only
int CanUseSpell(uint16 spellid, int classa, int level);
int GetMinLevel(uint16 spell_id);
int GetSpellLevel(uint16 spell_id, int classa);
int CalcBuffDuration_formula(int level, int formula, int duration);
int CalcBuffDuration_modification(int spell_id, int duration, bool isClient);
bool FindSpellModifier(int isclient, int spell_id, int zone_id, SpellModifier_Struct &spellModifier);
int32 CalculatePoisonCounters(uint16 spell_id);
int32 CalculateDiseaseCounters(uint16 spell_id);
int32 CalculateCurseCounters(uint16 spell_id);
int32 CalculateCounters(uint16 spell_id);
bool IsCombatSkill(uint16 spell_id);
bool IsResurrectionEffects(uint16 spell_id);
bool IsRuneSpell(uint16 spell_id);
bool IsMagicRuneSpell(uint16 spell_id);
bool IsManaTapSpell(uint16 spell_id);
bool IsAllianceSpellLine(uint16 spell_id);
bool IsDeathSaveSpell(uint16 spell_id);
bool IsFullDeathSaveSpell(uint16 spell_id);
bool IsPartialDeathSaveSpell(uint16 spell_id);
bool IsShadowStepSpell(uint16 spell_id);
bool IsSuccorSpell(uint16 spell_id);
bool IsTeleportSpell(uint16 spell_id);
bool IsGateSpell(uint16 spell_id);
bool IsPlayerIllusionSpell(uint16 spell_id); // seveian 2008-09-23
int32 GetSpellResistType(uint16 spell_id);
int32 GetSpellTargetType(uint16 spell_id);
bool IsHealOverTimeSpell(uint16 spell_id);
bool IsCompleteHealSpell(uint16 spell_id);
bool IsFastHealSpell(uint16 spell_id);
bool IsVeryFastHealSpell(uint16 spell_id);
bool IsRegularSingleTargetHealSpell(uint16 spell_id);
bool IsRegularGroupHealSpell(uint16 spell_id);
bool IsGroupCompleteHealSpell(uint16 spell_id);
bool IsGroupHealOverTimeSpell(uint16 spell_id);
bool IsDebuffSpell(uint16 spell_id);
bool IsResistDebuffSpell(uint16 spell_id);
bool IsSelfConversionSpell(uint16 spell_id);
bool IsBuffSpell(uint16 spell_id);
bool IsSuspendableSpell(uint16 spell_id);
uint32 GetMorphTrigger(uint32 spell_id);
bool IsCastonFadeDurationSpell(uint16 spell_id);
bool IsRacialIllusion(uint16 spell_id);
bool IsCorpseSummon(uint16 spell_id);
bool IsSpeedBuff(uint16 spell_id);
bool IsSpeedDeBuff(uint16 spell_id);

void GetRandPetName(char* name);
int GetSpellEffectDescNum(uint16 spell_id);
DmgShieldType GetDamageShieldType(uint16 spell_id, int32 DSType = 0);
bool DetrimentalSpellAllowsRest(uint16 spell_id);
int32 GetFuriousBash(uint16 spell_id);
bool IsShortDurationBuff(uint16 spell_id);
bool IsSpellUsableThisZoneType(uint16 spell_id, uint8 zone_type);
const char *GetSpellName(int16 spell_id);
bool IsRainSpell(uint16 spell_id);
bool IsDisc(uint16 spell_id);
bool IsShrinkSpell(uint16 spell_id);
bool IsLuclinPortSpell(uint16 spell_id);
bool IsInvisSpell(uint16 spell_id);
bool IsFixedDurationInvisSpell(uint16 spell_id);
bool IsInstrumentModdableSpellEffect(uint16 spell_id, int effect_index);
bool IsSplurtFormulaSpell(uint16 spell_id);
bool IsMGBCompatibleSpell(uint16 spell_id);
bool IsDispellableSpell(uint16 spell_id);
bool SpellPersistsThroughDeath(uint16 spell_id);

#endif
