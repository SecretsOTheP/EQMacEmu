/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

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
#ifndef EQ_PACKET_STRUCTS_H
#define EQ_PACKET_STRUCTS_H

#include "types.h"
#include <string.h>
#include <string>
#include <list>
#include <time.h>
#include "../common/version.h"
#include "textures.h"

static const uint32 BUFF_COUNT = 15;
static const uint32 QUESTREWARD_COUNT = 8;

#include "emu_constants.h"

/*
** Compiler override to ensure
** byte aligned structures
*/
#pragma pack(1)

struct LoginInfo_Struct {
/*000*/	char	login_info[64];
/*064*/	uint8	unknown064[124];
/*188*/	uint8	zoning;			// 01 if zoning, 00 if not
/*189*/	uint8	unknown189[275];
/*488*/	uint8	macversion;	// in mac packet decode, intel and ppc clients send different lengths of this so we can remember it here and pass it down
/*489*/
};

struct EnterWorld_Struct {
/*000*/	char	name[64];
};

struct ExpansionInfo_Struct {
/*0000*/	uint32	Expansions;
};

/*
** Entity identification struct
** Size: 2 bytes
** OPCodes: OP_DeleteSpawn, OP_Assist
*/
struct EntityId_Struct
{
	/*000*/	int16 entity_id;
	/*002*/
};

struct Duel_Struct
{
	uint16 duel_initiator;
	uint16 duel_target;
};

struct DuelResponse_Struct
{
	uint16 duel_initiator;
	uint16 duel_target;
	uint16 unknown;
};

///////////////////////////////////////////////////////////////////////////////



/*
** Character Selection Struct
** Length: 1620 Bytes
**
*/
struct CharacterSelect_Struct {
	/*0000*/	char	name[10][64];			// Characters Names
	/*0640*/	uint8	level[10];				// Characters Levels
	/*0650*/	uint8	class_[10];				// Characters Classes
	/*0660*/	uint16	race[10];				// Characters Race
	/*0680*/	uint32	zone[10];				// Characters Current Zone
	/*0720*/	uint8	gender[10];				// Characters Gender
	/*0730*/	uint8	face[10];				// Characters Face Type
	/*0740*/	EQ::TextureProfile	equip[10];	// Characters texture array
	/*1100*/	EQ::TintProfile cs_colors[10];	// Characters Equipment Colors (RR GG BB 00)
	/*1460*/	uint16	deity[10];				// Characters Deity
	/*1480*/	uint32	primary[10];			// Characters primary and secondary IDFile number
	/*1520*/	uint32	secondary[10];			// Characters primary and secondary IDFile number
	/*1560*/	uint8	haircolor[10];
	/*1570*/	uint8	beardcolor[10];
	/*1580*/	uint8	eyecolor1[10];
	/*1590*/	uint8	eyecolor2[10];
	/*1600*/	uint8	hairstyle[10];
	/*1610*/	uint8	beard[10];
	/*1620*/
};

/*
** Generic Spawn Struct
** Length: 383 Octets
** Used in:
** spawnZoneStruct
** dbSpawnStruct
** petStruct
** newSpawnStruct
*/
/*
showeq -> eqemu
sed -e 's/_t//g' -e 's/seto_0xFF/set_to_0xFF/g'
*/
struct Spawn_Struct {
	/*0000*/	uint32  random_dontuse;
	/*0004*/	int8	accel;
	/*0005*/	uint8	heading;			// Current Heading
	/*0006*/	int8	deltaHeading;		// Delta Heading
	/*0007*/	int16	y;				// Y Position
	/*0009*/	int16	x;				// X Position
	/*0011*/	int16	z;				// Z Position
	/*0013*/	uint32	deltaY : 11,			// Velocity Y
						deltaZ : 11,			// Velocity Z
						deltaX : 10;			// Velocity X
	/*0017*/	uint8	void1;
	/*0018*/	uint16	petOwnerId;		// Id of pet owner (0 if not a pet)
	/*0020*/	uint8	animation;
	/*0021*/    uint8	haircolor;
	/*0022*/	uint8	beardcolor;
	/*0023*/	uint8	eyecolor1;
	/*0024*/	uint8	eyecolor2;
	/*0025*/	uint8	hairstyle;
	/*0026*/	uint8	beard;
	/*0027*/    uint8   aa_title;
	/*0028*/	float	size;
	/*0032*/	float	walkspeed;
	/*0036*/	float	runspeed;
	/*0040*/	EQ::TintProfile	colors;
	/*0076*/	uint16	spawnId;			// Id of new spawn
	/*0078*/	int16	bodytype;			// 65 is disarmable trap, 66 and 67 are invis triggers/traps
	/*0080*/	int16	curHp;				// Current hp's of Spawn
	/*0082*/	int16	guildID;			// GuildID - previously Current hp's of Spawn
	/*0084*/	uint16	race;				// Race
	/*0086*/	uint8	NPC;				// NPC type: 0=Player, 1=NPC, 2=Player Corpse, 3=Monster Corpse, 4=???, 5=Unknown Spawn,10=Self
	/*0087*/	uint8	class_;				// Class
	/*0088*/	uint8	gender;				// Gender Flag, 0 = Male, 1 = Female, 2 = Other
	/*0089*/	uint8	level;				// Level of spawn (might be one int8)
	/*0090*/	uint8	invis;				// 0=visable, 1=invisable
	/*0091*/	uint8	sneaking;
	/*0092*/	uint8	pvp;
	/*0093*/	uint8	StandState;
	/*0094*/	uint8	light;				// Light emitting
	/*0095*/	int8	anon;				// 0=normal, 1=anon, 2=RP
	/*0096*/	int8	afk;				// 0=off, 1=on
	/*0097*/	int8	summoned_pc;
	/*0098*/	int8	LD;					// 0=NotLD, 1=LD
	/*0099*/	int8	gm;					// 0=NotGM, 1=GM
	/*0100*/	int8	flymode;
	/*0101*/	int8	bodytexture;
	/*0102*/	int8	helm;
	/*0103*/	uint8	face;
	/*0104*/	uint16	equipment[9];		// Equipment worn: 0=helm, 1=chest, 2=arm, 3=bracer, 4=hand, 5=leg, 6=boot, 7=melee1, 8=melee2
	/*0122*/	int16	guildrank;			// ***Placeholder
	/*0124*/	int16	deity;				// Deity.
	/*0126*/	int8	temporaryPet;
	/*0127*/	char	name[64];			// Name of spawn (len is 30 or less)
	/*0191*/	char	lastName[32];		// Last Name of player
	/*0223*/	uint8	void_;  
	/*0224*/								 // end of mac spawn_struct

				uint8	max_hp;				// (name prolly wrong)takes on the value 100 for players, 100 or 110 for NPCs and 120 for PC corpses...
				uint8	is_npc;				// 0=no, 1=yes
				char	suffix[32];			// Player's suffix (of Veeshan, etc.)
				char	title[32];		// Title
				uint8	set_to_0xFF[8];	// ***Placeholder (all ff)
				uint8	is_pet;			// 0=no, 1=yes
				uint8	mount_color;		// drogmor: 0=white, 1=black, 2=green, 3=red
				uint8	lfg;			// 0=off, 1=lfg on
				uint32	zoneID; // for mac.
};

struct LFG_Struct {
	char	name[64];
	int32	value;
};

struct LFG_Appearance_Struct {
	int16	entityid;
	int16	unknown;
	int32	value;
};

/*
** New Spawn
** Length: 176 Bytes
** OpCode: 4921
*/
struct NewSpawn_Struct
{
	struct Spawn_Struct spawn;	// Spawn Information
};

struct ClientZoneEntry_Struct {
/*0000*/	uint32	unknown00;
/*0004*/	char	char_name[64];			// Character Name
};


/*
** Server Zone Entry Struct
** OPCodes: OP_ServerZoneEntry
**
*/
struct ServerZoneEntry_Struct 
{
	/*0000*/	uint8	checksum[4];		// Checksum
	/*0004*/	uint8	type;		// ***Placeholder
	/*0005*/	char	name[64];			// Name
	/*0069*/	uint8	sze_unknown0069;	// ***Placeholder
	/*0070*/	uint16	unknown0070;		// ***Placeholder
	/*0072*/	uint32	zoneID;				// Current Zone
	/*0076*/	float	y_pos;				// Y Position
	/*0080*/	float	x_pos;				// X Position
	/*0084*/	float	z_pos;				// Z Position
	/*0088*/	float	heading;
	/*0092*/	float	physicsinfo[8];
	/*0124*/	int32	prev;
	/*0128*/	int32	next;
	/*0132*/	int32	corpse;
	/*0136*/	int32	LocalInfo;
	/*0140*/	int32	My_Char;
	/*0144*/	float	view_height;
	/*0148*/	float	sprite_oheight;
	/*0152*/	uint16	sprite_oheights;
	/*0154*/	uint16	petOwnerId;
	/*0156*/	uint32	max_hp;
	/*0160*/	uint32	curHP;
	/*0164*/	uint16	GuildID;			// Guild ID Number
	/*0166*/	uint8	socket[6];		// ***Placeholder
	/*0172*/	uint8	NPC;
	/*0173*/	uint8	class_;				// Class
	/*0174*/	uint16	race;				// Race
	/*0176*/	uint8	gender;				// Gender
	/*0177*/	uint8	level;				// Level
	/*0178*/	uint8	invis;
	/*0179*/	uint8	sneaking;
	/*0180*/	uint8	pvp;				// PVP Flag
	/*0181*/	uint8	anim_type;
	/*0182*/	uint8	light;
	/*0183*/	int8	face;				// Face Type
	/*0184*/    uint16  equipment[9]; // Array elements correspond to struct equipment above
	/*0202*/	uint16	unknown; //Probably part of equipment
	/*0204*/	EQ::TintProfile equipcolors; // Array elements correspond to struct equipment_colors above
	/*0240*/	uint32	bodytexture;	// Texture (0xFF=Player - See list of textures for more)
	/*0244*/	float	size;
	/*0248*/	float	width;
	/*0252*/	float	length;
	/*0256*/	uint32	helm;
	/*0260*/	float	walkspeed;			// Speed when you walk
	/*0264*/	float	runspeed;			// Speed when you run
	/*0268*/	int8	LD;
	/*0269*/	int8	GM;
	/*0270*/	int16	flymode;
	/*0272*/	int32	bodytype;
	/*0276*/	int32	view_player;
	/*0280*/	uint8	anon;				// Anon. Flag
	/*0281*/	uint16	avatar;
	/*0283*/	uint8	AFK;
	/*0284*/	uint8	summoned_pc;
	/*0285*/	uint8	title;
	/*0286*/	uint8	extra[18];	// ***Placeholder (At least one flag in here disables a zone point or all)
	/*0304*/	char	Surname[32];		// Lastname (This has to be wrong.. but 70 is to big =/..)
	/*0336*/	uint16  guildrank;
	/*0338*/	uint16	deity;				// Deity (Who you worship for those less literate)
	/*0340*/	int8	animation;		// ***Placeholder
	/*0341*/	uint8	haircolor;			// Hair Color
	/*0342*/	uint8	beardcolor;			// Beard Color
	/*0343*/	uint8	eyecolor1;			// Left Eye Color
	/*0344*/	uint8	eyecolor2;			// Right Eye Color
	/*0345*/	uint8	hairstyle;			// Hair Style
	/*0346*/	uint8	beard;				// AA Title
	/*0347*/	uint32	SerialNumber;
	/*0351*/	char	m_bTemporaryPet[4];
	/*0355*/	uint8	void_;
	/*0356*/
};

//This should be treated as an internal struct
struct NewZone_Struct {
	/*0000*/	char	char_name[64];			// Character Name
	/*0064*/	char	zone_short_name[32];	// Zone Short Name
	/*0096*/	char	zone_long_name[278];	// Zone Long Name
	/*0374*/	uint8	ztype;
	/*0375*/	uint8	fog_red[4];				// Red Fog 0-255 repeated over 4 bytes (confirmed)
	/*0379*/	uint8	fog_green[4];			// Green Fog 0-255 repeated over 4 bytes (confirmed)
	/*0383*/	uint8	fog_blue[4];			// Blue Fog 0-255 repeated over 4 bytes (confirmed)
	/*0387*/	uint8	unknown387;
	/*0388*/	float	fog_minclip[4];			// Where the fog begins (lowest clip setting). Repeated over 4 floats. (confirmed)
	/*0404*/	float	fog_maxclip[4];			// Where the fog ends (highest clip setting). Repeated over 4 floats. (confirmed)	
	/*0420*/	float	gravity;
	/*0424*/	uint8	time_type;
	/*0425*/    uint8   rain_chance[4];
	/*0429*/    uint8   rain_duration[4];
	/*0433*/    uint8   snow_chance[4];
	/*0437*/    uint8   snow_duration[4];
	/*0441*/	uint8	specialdates[16];
	/*0457*/	uint8	specialcodes[16];
	/*0473*/	int8	timezone;
	/*0474*/	uint8	sky;					// Sky Type
	/*0475*/	uint8   unknown0475;
	/*0476*/	int16  water_music;
	/*0478*/	int16  normal_music_day;
	/*0480*/	int16  normal_music_night;
	/*0482*/	uint8	unknown0482[2];
	/*0484*/	float	zone_exp_multiplier;	// Experience Multiplier
	/*0488*/	float	safe_y;					// Zone Safe Y
	/*0492*/	float	safe_x;					// Zone Safe X
	/*0496*/	float	safe_z;					// Zone Safe Z
	/*0500*/	float	max_z;					// Guessed
	/*0504*/	float	underworld;				// Underworld, min z (Not Sure?)
	/*0508*/	float	minclip;				// Minimum View Distance
	/*0512*/	float	maxclip;				// Maximum View DIstance
	/*0516*/	uint32	forage_novice;
	/*0520*/	uint32	forage_medium;
	/*0524*/	uint32	forage_advanced;
	/*0528*/	uint32	fishing_novice;
	/*0532*/	uint32	fishing_medium;
	/*0536*/	uint32	fishing_advanced;
	/*0540*/	uint32	skylock;
	/*0544*/	uint16	graveyard_time;
	/*0546*/	uint32	scriptPeriodicHour;
	/*0550*/	uint32	scriptPeriodicMinute;
	/*0554*/	uint32	scriptPeriodicFast;
	/*0558*/	uint32	scriptPlayerDead;
	/*0562*/	uint32	scriptNpcDead;
	/*0566*/	uint32  scriptPlayerEntering;
	/*0570*/	uint16	unknown570;		// ** EQMac newzone_structs stops here
	/*0572*/	uint8	unknown_end[32];		// ***Placeholder
	/*0604*/	char	zone_short_name2[68];
	/*0672*/	char	unknown672[12];
	/*0684*/	uint16	zone_id;
	/*0688*/	uint32	unknown688;
	/*0692*/	uint8	unknown692[8];
	/*0700*/	float	fog_density;
	/*0704*/	uint32	SuspendBuffs;
	/*0708*/	uint8	expansion;
	/*0709*/	bool	never_idle;
	/*0710*/
};

/*
** Memorize Spell Struct
** Length: 12 Bytes
**
*/
struct MemorizeSpell_Struct {
	/*000*/		uint32 slot;			// Comment:  Spot in the spell book/memorized slot 
	/*004*/		uint32 spell_id;		// Comment:  Spell id (200 or c8 is minor healing, etc) 
	/*008*/		uint32 scribing;		// Comment:  1 if memorizing a spell, set to 0 if scribing to book 
	/*012*/
};

struct Charm_Struct
{
	/*000*/	uint16	owner_id;
	/*002*/	uint16	pet_id;
	/*004*/	uint16	command; // 1: make pet, 0: release pet
	/*006*/
};

struct InterruptCast_Struct
{
	uint16 messageid;
	uint16 color;
	char	message[0];
};

struct DeleteSpell_Struct
{
/*000*/int16	spell_slot;
/*002*/uint8	unknowndss002[2];
/*004*/uint8	success;
/*005*/uint8	unknowndss006[3];
/*008*/
};

struct ManaChange_Struct
{
	/*00*/	uint16 new_mana;	// Comment:  New Mana AMount
	/*02*/	uint16 spell_id;	// Comment:  Last Spell Cast
	/*04*/
};

struct SwapSpell_Struct
{
	uint32 from_slot;
	uint32 to_slot;


};

struct BeginCast_Struct
{
	/*000*/	uint16	caster_id;		// Comment: Unknown -> needs confirming -> ID of Spell Caster? 
	/*002*/	uint16	spell_id;		// Comment: Unknown -> needs confirming -> ID of Spell being Cast?
	/*004*/	uint16	cast_time;		// Comment: Unknown -> needs confirming -> in miliseconds?
	/*006*/ uint16  unknown;
	/*008*/
};

struct CastSpell_Struct
{
	/*000*/	uint16	slot;
	/*002*/	uint16	spell_id;
	/*004*/	uint16	inventoryslot;  // slot for clicky item, 0xFFFF = normal cast
	/*006*/	uint16	target_id;
	/*008*/ uint32  spell_crc;
	/*012*/
};

/*
** SpawnAppearance_Struct
** Changes client appearance for all other clients in zone
** Size: 8 bytes
** Used in: OP_SpawnAppearance
**
*/
struct SpawnAppearance_Struct
{
/*0000*/ uint16 spawn_id;		// ID of the spawn
/*0002*/ uint16 type;			// Values associated with the type
/*0004*/ uint32 parameter;		// Type of data sent
/*0008*/
};


// solar: this is used inside profile
struct SpellBuff_Struct
{
	/*000*/ uint8	bufftype;        // Comment: 0 = Buff not visible, 1 = Visible and permanent buff, 2 = Visible and timer on, 4 = reverse effect values, used for lifetap type things
	/*001*/ uint8	level;
	/*002*/	uint8	bard_modifier;
	/*003*/	uint8	activated;	// copied from spell data to buff, only a few spells have this as 1
	/*004*/	uint16	spellid;
	/*008*/ uint16	duration;
	/*012*/	uint16	counters;
	/*016*/	uint32	player_id;	//'global' ID of the caster, for wearoff messages, not part of client struct, just in this internal emu struct
	/*020*/
};

struct SpellBuffFade_Struct {
	/*000*/	uint16	entityid;

	// next 10 bytes is SpellBuff_Struct - we can set bufffade to 3 to replace buffs by slot_number
	/*002*/	uint8   bufftype;
	/*003*/ uint8   level;
	/*004*/ uint8	bard_modifier;
	/*005*/ uint8	activated;    // Copied from spell data to buff struct.  Only a few spells have this set to 1, the rest are 0
	/*006*/	uint16  spellid;
	/*008*/ uint16	duration;        // Duration in ticks
	/*010*/ uint16	counters;        // rune amount, poison/disease/curse counters
	
	/*012*/ uint16	slot_number;
	/*014*/ uint16	unk14;

	// this is an update type field
	// 1 = remove buff.  matches by spell id and bufftype
	// 3 = replace/update buff.  client replaces the buff in 'slot_number' field
	// 0 or anything else = replace/update buff by spell id and bufftype match
	/*016*/	uint32	bufffade;
	/*020*/ 
};

struct GMTrainee_Struct{
	/*000*/ uint16 npcid;
	/*002*/	uint16 playerid;
	/*004*/ uint16 skills[100];
	/*204*/	float  greed;
	/*208*/ uint8  success;
	/*209*/	uint8  language[32];
	/*241*/	uint8  ending[3]; //Copied from client packet (probably void)
	/*244*/
};

struct GMTrainEnd_Struct
{
	/*000*/ int16 npcid;
	/*002*/ int16 playerid;
	/*004*/
};

struct GMSkillChange_Struct {
/*000*/	uint16		npcid;
/*002*/ uint16		playerid;
/*004*/ uint16		skillbank;		// 0 if normal skills, 1 if languages
/*006*/ uint16		unknown2;
/*008*/ uint16		skill_id;
/*010*/ uint16		unknown3;		//probably void
};

struct ConsentResponse_Struct {
	char grantname[64];
	char ownername[64];
	uint8 permission;
	char zonename[32];
};

/*
** Name Generator Struct
** Length: 72 bytes
** OpCode: 0x0290
*/
struct NameGeneration_Struct
{
/*0000*/	uint32	race;
/*0004*/	uint32	gender;
/*0008*/	char	name[64];
/*0072*/
};

/*
** CharCreate
** Length: 8452 Bytes
*/
struct CharCreate_Struct
{
	/*0000*/	uint8	unknown0004[136];
	/*0136*/	uint8	gender;				// Player Gender
	/*0137*/	char	unknown137[1];
	/*0138*/	uint16	race;				// Player Race
	/*0140*/	uint16	class_;				// Player Class
	/*0142*/	uint8	unknown0142[18];
	/*0160*/	uint16	STR;				// Player Strength
	/*0162*/	uint16	STA;				// Player Stamina
	/*0164*/	uint16	CHA;				// Player Charisma
	/*0166*/	uint16	DEX;				// Player Dexterity
	/*0168*/	uint16	INT;				// Player Intelligence
	/*0170*/	uint16	AGI;				// Player Agility
	/*0172*/	uint16	WIS;				// Player Wisdom
	/*0174*/	uint8	oldface;
	/*0175*/	uint8	unknown0175[3265];
	/*3440*/	uint32	start_zone;
	// 0 = odus
	// 1 = qeynos
	// 2 = halas
	// 3 = rivervale
	// 4 = freeport
	// 5 = neriak
	// 6 = gukta/grobb
	// 7 = ogguk
	// 8 = kaladim
	// 9 = gfay
	// 10 = felwithe
	// 11 = akanon
	// 12 = cabalis
	// 13 = shar vahl
	/*3444*/	uint8	unknown3444[1496];
	/*4940*/	uint16	deity;
	/*4942*/	uint8	unknown4946[480];
	/*5422*/	uint8	haircolor;			// Player Hair Color
	/*5423*/	uint8	beardcolor;			// Player Beard Color
	/*5424*/	uint8	eyecolor1;			// Player Left Eye Color
	/*5425*/	uint8	eyecolor2;			// Player Right Eye Color
	/*5426*/	uint8	hairstyle;			// Player Hair Style
	/*5427*/	uint8	beard;				// Player Beard Type
	/*5428*/	uint8	face;				// Player Face Type
	/*5429*/	uint8	unknown5429[3023];
	/*8452*/
};

/*
 *Used in PlayerProfile
 */
struct AA_Array
{
	uint32 AA;
	uint32 value;
};

struct ClientDiscipline_Struct {
    uint8	disc_id;	// There are only a few discs < 60
    uint8	unknown3[3];	// Which leaves room for ??
};

 /**
* A bind point.
* Size: 20 Octets
*/
struct BindStruct {
	/*000*/ uint32 zoneId;
	/*004*/ float x;
	/*008*/ float y;
	/*012*/ float z;
	/*016*/ float heading;
	/*020*/ uint32 unused;
	/*024*/
};

struct SuspendedMinion_Struct
{
	/*000*/	uint16 SpellID;
	/*002*/	uint32 HP;
	/*006*/	uint32 Mana;
	/*010*/	SpellBuff_Struct Buffs[BUFF_COUNT];
	/*510*/	EQ::TextureProfile Items;
	/*546*/	char Name[64];
	/*610*/
};

struct ItemProperties_Struct
{

	/*000*/	uint8	unknown01[2];
	/*002*/	int8	charges;				// Comment: signed int because unlimited charges are -1
	/*003*/	uint8	unknown02[7];
	/*010*/
};

/*
** Player Profile
**
** Length: 4308 bytes
** OpCode: 0x006a
 */
static const uint32 MAX_PP_LANGUAGE = 26;
static const uint32 MAX_PP_SPELLBOOK = 256;	// Set for all functions
static const uint32 MAX_PP_MEMSPELL = static_cast<uint32>(EQ::spells::CastingSlot::MaxGems);
static const uint32 MAX_PP_REF_SPELLBOOK = 256;	// Set for Player Profile size retain
static const uint32 MAX_PP_REF_MEMSPELL = 8; // Set for Player Profile size retain

static const uint32 MAX_PP_SKILL		= PACKET_SKILL_ARRAY_SIZE;	// 100 - actual skills buffer size
static const uint32 MAX_PP_AA_ARRAY		= 240;

// This should be treated as an internal struct
struct PlayerProfile_Struct
{
	/*0000*/	uint32  checksum;		    // Checksum
	/*0004*/	uint8	unknown0004[2];		// ***Placeholder
	/*0006*/	char	name[64];			// Player First Name
	/*0070*/	char	last_name[66];		// Surname OR title.
	/*0136*/	uint32	uniqueGuildID;
	/*0140*/	uint8	gender;				// Player Gender
	/*0141*/	char	genderchar[1];		// ***Placeholder
	/*0142*/	uint16	race;				// Player Race (Lyenu: Changed to an int16, since races can be over 255)
	/*0144*/	uint16	class_;				// Player Class
	/*0146*/	uint16	bodytype;
	/*0148*/	uint8	level;				// Player Level
	/*0149*/	char	levelchar[3];		// ***Placeholder
	/*0152*/	uint32	exp;				// Current Experience
	/*0156*/	int16	points;				// Players Points
	/*0158*/	int16	mana;				// Player Mana
	/*0160*/	int16	cur_hp;				// Player Health
	/*0162*/	uint16	status;
	/*0164*/	int16	STR;				// Player Strength
	/*0166*/	int16	STA;				// Player Stamina
	/*0168*/	int16	CHA;				// Player Charisma
	/*0170*/	int16	DEX;				// Player Dexterity
	/*0172*/	int16	INT;				// Player Intelligence
	/*0174*/	int16	AGI;				// Player Agility
	/*0176*/	int16	WIS;				// Player Wisdom
	/*0178*/	uint8	face;               //
	/*0179*/    uint8    EquipType[9];       // i think its the visible parts of the body armor
	/*0188*/    uint32   EquipColor[9];      //
	/*0224*/	int16	inventory[30];		// Player Inventory Item Numbers
	/*0284*/	uint8	languages[32];		// Player Languages - space for 32 but only the first 25 are used
	/*0316*/	struct	ItemProperties_Struct	invItemProperties[30];	// These correlate with inventory[30]
	/*0616*/	struct	SpellBuff_Struct	buffs[BUFF_COUNT];	// Player Buffs Currently On
	/*0766*/	int16	containerinv[80];
	/*0926*/	int16   cursorbaginventory[10]; // If a bag is in slot 0, this is where the bag's items are
	/*0946*/	struct	ItemProperties_Struct	bagItemProperties[80];	// Just like InvItemProperties
	/*1746*/    struct  ItemProperties_Struct	cursorItemProperties[10];	  //just like invitemprops[]
	/*1846*/	int16	spell_book[MAX_PP_REF_SPELLBOOK];	// Player spells scribed in their book
	/*2358*/	uint8	unknown2374[512];	// 0xFF
	/*2870*/	int16	mem_spells[MAX_PP_MEMSPELL];	// Player spells memorized
	/*2886*/	uint8	unknown2886[16];	// 0xFF
	/*2902*/	uint16	available_slots;
	/*2904*/	float	y;					// Player Y
	/*2908*/	float	x;					// Player X
	/*2912*/	float	z;					// Player Z
	/*2916*/	float	heading;			// Player Heading
	/*2920*/	uint32	position;		// ***Placeholder
	/*2924*/	int32	platinum;			// Player Platinum (Character)
	/*2928*/	int32	gold;				// Player Gold (Character)
	/*2932*/	int32	silver;				// Player Silver (Character)
	/*2936*/	int32	copper;				// Player Copper (Character)
	/*2940*/	int32	platinum_bank;		// Player Platinum (Bank)
	/*2944*/	int32	gold_bank;			// Player Gold (Bank)
	/*2948*/	int32	silver_bank;		// Player Silver (Bank)
	/*2952*/	int32	copper_bank;		// Player Copper (Bank)
	/*2956*/	int32	platinum_cursor;
	/*2960*/	int32	gold_cursor;
	/*2964*/	int32	silver_cursor;
	/*2968*/	int32	copper_cursor;
	/*2972*/	int32	currency[4];	    //Unused currency?
	/*2988*/	int16	skills[100];		// Player Skills - 100 skills but only the first 74 are used, followed by 25 innates but only the first 14 are used
	/*3188*/	int16	innate_skills[25];	// Like regular skills, these are 255 to indicate that the player doesn't have it and 0 means they do have it
	/*3238*/    uint8   air_supply;
	/*3239*/    uint8   texture;
	/*3240*/	float   height;
	/*3244*/	float	width;
	/*3248*/	float   length;
	/*3252*/	float   view_height;
	/*3256*/    char    boat[32];
	/*3280*/    uint8   unknown[60];
	/*3348*/	uint8	autosplit;
	/*3349*/	uint8	unknown3449[43];
	/*3392*/	uint8	expansions;			//Effects features such as /disc, AA, raid
	/*3393*/	uint8	unknown3393[51];
	/*3444*/	uint32	zone_id;
	/*3448*/	uint8	unknown3448[336];	// On AK, there was a lot of data here in the packet the client sent for OP_Save, but none in the encoded packet the server sent at zone in.
	/*3784*/	BindStruct			binds[5];			// Bind points (primary is first, home city is fifth)
	/*3884*/	ItemProperties_Struct	bankinvitemproperties[8];
	/*3964*/	ItemProperties_Struct	bankbagitemproperties[80];
	/*4764*/	uint32	login_time;
	/*4768*/	int16	bank_inv[8];		// Player Bank Inventory Item Numbers
	/*4784*/	int16	bank_cont_inv[80];	// Player Bank Inventory Item Numbers (Bags)
	/*4944*/	int16	deity;		// ***Placeholder
	/*4946*/	uint32	guild_id;			// Player Guild ID Number
	/*4948*/	uint32  birthday;
	/*4952*/	uint32  lastlogin;
	/*4956*/	uint32  timePlayedMin;
	/*4960*/	int16    thirst_level;
	/*4961*/    int16    hunger_level;
	/*4962*/	int8   fatigue;
	/*4963*/	uint8	pvp;				// Player PVP Flag
	/*4964*/	uint8	level2;
	/*4965*/	uint8	anon;				// Player Anon. Flag
	/*4966*/	uint8	gm;					// Player GM Flag
	/*4967*/	uint8	guildrank;			// Player Guild Rank (0=member, 1=officer, 2=leader)
	/*4968*/    uint8   intoxication;
	/*4969*/	uint8	eqbackground;
	/*4970*/	uint8	unknown4760[2];
	/*4972*/	uint32	spellSlotRefresh[MAX_PP_MEMSPELL];
	/*5004*/	uint32	unknown5003;
	/*5008*/	uint32	abilitySlotRefresh;
	/*5012*/	char	groupMembers[6][64];	// Group Members
	/*5396*/	uint8	unknown5396[20];
	/*5416*/	uint32	groupdat;
	/*5420*/	uint32	expAA;				// Post60Exp
//	/*5424*/    uint8	title;
	/*5425*/	uint8	perAA;			    // Player AA Percent
	/*5426*/	uint8	haircolor;			// Player Hair Color
	/*5427*/	uint8	beardcolor;			// Player Beard Color
	/*5428*/	uint8	eyecolor1;			// Player Left Eye Color
	/*5429*/	uint8	eyecolor2;			// Player Right Eye Color
	/*5430*/	uint8	hairstyle;			// Player Hair Style
	/*5431*/	uint8	beard;				// Player Beard Type
	/*5432*/	uint8	luclinface;			// Player Face Type
	/*5433*/	EQ::TextureProfile	item_material;
	/*5469*/	uint8	unknown5469[143];
	/*5612*/	AA_Array aa_array[MAX_PP_AA_ARRAY];
	/*5852*/	uint32	ATR_DIVINE_RES_timer;
	/*5856*/    uint32  ATR_FREE_HOT_timer;
	/*5860*/	uint32	ATR_TARGET_DA_timer;
	/*5864*/	uint32	SptWoodTimer;
	/*5868*/	uint32	DireCharmTimer;
	/*5872*/	uint32	ATR_STRONG_ROOT_timer;
	/*5876*/	uint32	ATR_MASOCHISM_timer;
	/*5880*/	uint32	ATR_MANA_BURN_timer;
	/*5884*/	uint32	ATR_GATHER_MANA_timer;
	/*5888*/	uint32	ATR_PET_LOH_timer;
	/*5892*/	uint32	ExodusTimer;
	/*5896*/	uint32	ATR_MASS_FEAR_timer;
	/*5900*/    uint16  air_remaining;
	/*5902*/    uint16  aapoints;
	/*5904*/	uint32	MGBTimer;
	/*5908*/	uint8   unknown5908[91];
	/*5999*/	int8	mBitFlags[6];
	/*6005*/	uint8	Unknown6004[707];
	/*6712*/	uint32	PoPSpellTimer;
	/*6716*/	uint32	LastShield;
	/*6720*/	uint32	LastModulated;
	/*6724*/	uint8	Unknown6724[1736]; // According to the client, this is all unused/unknown space.
	/*8460*/								// End of mac PlayerProfile_struct
				char				servername[32];		// length probably not right
				char				title[32];			// length might be wrong
				char				suffix[32];			// length might be wrong
				uint32				guildid2;			//
				uint32				boatid;
				uint32				zone_change_count;	// Number of times user has zoned in their career (guessing)
				uint32				aapoints_spent;
				SuspendedMinion_Struct	SuspendedMinion; // No longer in use
				uint32				timeentitledonaccount;
				bool				mule;
				bool				showhelm;
				uint32	force_guild_id;
				struct	SpellBuff_Struct	buffs_ext[BUFF_COUNT];
};




/*
** client changes target struct
** Length: 2 Bytes
** OpCode: 6241
*/
struct ClientTarget_Struct
{
	/*000*/	uint16	new_target;			// Target ID
	/*002*/
};

struct PetCommand_Struct {
/*000*/ uint32	command;
/*004*/ uint32	target;
};

struct DeleteSpawn_Struct
{
	/*00*/ uint16 spawn_id;				// Comment: Spawn ID to delete
	/*02*/
};

/*
** Channel Message received or sent
** Length: 136 Bytes + Variable Length up to 2048 bytes for the message
** OpCode: OP_ChannelMessage
**
*/
struct ChannelMessage_Struct
{
	/*000*/	char	targetname[64];		// Tell recipient
	/*064*/	char	sender[64];			// The senders name
	/*128*/	uint16	language;			// Language
	/*130*/	uint16	chan_num;			// Channel
	/*132*/	uint16	unused_align132;	// struct alignment padding
	/*134*/	uint16	skill_in_language;	// The players skill in this language? might be wrong
	/*136*/	char	message[0];			// Variable length message
};

struct SpecialMesg_Struct
{
/*00*/	char	header[3];				// 04 04 00 <-- for #emote style msg
/*03*/	uint32	msg_type;				// Color of text (see MT_*** below)
/*07*/	uint32	target_spawn_id;		// Who is it being said to?
/*11*/	char	sayer[1];				// Who is the source of the info
/*12*/	uint8	unknown12[12];
/*24*/	char	message[1];				// What is being said?
};

/*
** When somebody changes what they're wearing or give a pet a weapon (model changes)
*/
struct WearChange_Struct
{
	/*000*/ uint16 spawn_id;
	/*002*/ uint8  wear_slot_id;
	/*003*/ uint8  align03; // struct field alignment only, data here is not meaningful
	/*004*/ uint16 material;
	/*006*/ uint16 align06; // struct field alignment only, data here is not meaningful
	/*008*/ EQ::textures::Tint_Struct color;
	/*012*/
};

struct BindWound_Struct {
/*000*/    uint16  to; // entity id
/*002*/    uint8   type; // 0 or 1 complete, 2 Unknown, 3 ACK, 4 Died, 5 Left, 6 they moved, 7 you moved
/*003*/    uint8   void_;
};

/*
** Type: Zone Change Request (before hand)
** Length: 88 bytes
** OpCode: a320
*/

struct ZoneChange_Struct {
	/*000*/	char	char_name[64];     // Character Name
	/*064*/	uint16	zoneID;
	/*066*/ uint16  zone_reason;
	/*068*/ uint16  unknown[2];
	/*072*/	int8	success;		// =0 client->server, =1 server->client, -X=specific error
	/*073*/	uint8	error[3]; // =0 ok, =ffffff error
	/*076*/
};

// Whatever you send to the client in RequestClientZoneChange_Struct.type, the client will send back
// to the server in ZoneChange_Struct.zone_reason. My guess is this is a memo field of sorts.

struct RequestClientZoneChange_Struct {
	/*000*/	uint32	zone_id;
	/*004*/	float	y;
	/*008*/	float	x;
	/*012*/	float	z;
	/*016*/	float	heading;
	/*020*/	uint32	type;	//unknown... values
	/*024*/
};

struct Animation_Struct {
/*00*/	uint16 spawnid;
/*02*/	uint16 target;
/*04*/	uint8  action;
/*05*/  uint8  value;
/*06*/	uint32 unknown06;
/*10*/	uint16 unknown10; // 80 3F
};

// solar: this is what causes the caster to animate and the target to
// get the particle effects around them when a spell is cast
// also causes a buff icon
// verified used/unused portions in eqmac clients
struct Action_Struct
{
	/*00*/	uint16	target;				// Target entity ID
	/*02*/	uint16	source;				// Caster entity ID
	/*04*/	uint16	level;				// this only does something for spell ids 1252-1266 (potions) and only accepts values 1-60, otherwise the action uses source entity level
	/*06*/	uint16	target_level;		// unused by client
	/*08*/	int32	instrument_mod;		// normally 10, used for bard songs
	/*12*/	float	force;				// push force
	/*16*/	float	sequence;			// push heading
	/*20*/	float	pushup_angle;		// push pitch
	/*24*/	uint8	type;				// 231 for spells
	/*25*/	uint8	unknown25;			// unused by client
	/*26*/	uint16	spell_id_unused;	// extra spell_id, not used by client
	/*28*/	int16	tap_amount;			// used in client for instant, targettype 13 (tap) spells to set the amount that was tapped
	/*30*/	uint16	spell;				// spell_id
	/*32*/	uint8	unknown32;			// 0x00
	/*33*/  uint8	buff_unknown;		// 1 to start then 4 for success
	/*34*/	uint16	unknown34;			// unused by client
};

// this is what prints the You have been struck. and the regular
// melee messages like You try to pierce, etc. It's basically the melee
// and spell damage message.  It is also an enviromental damage struct
struct Damage_Struct
{
	/*000*/	uint16	target;
	/*002*/	uint16	source;
	/*004*/	uint16	type;
	/*006*/	uint16	spellid;
	/*008*/	int32	damage;
	/*012*/	float	force;
	/*016*/	float	sequence; // see above notes in Action_Struct
	/*020*/	float	pushup_angle; // associated with force.  Sine of this angle, multiplied by force, will be z push.
	/*024*/
};

/*
** Consider Struct
*/
struct Consider_Struct
{
	/*000*/ uint16	playerid;               // PlayerID
	/*002*/ uint16	targetid;               // TargetID
	/*004*/ uint32	faction;                // Faction
	/*008*/ uint32	level;                  // Level
	/*012*/ int32	cur_hp;			// Current Hitpoints
	/*016*/ int32	max_hp;			// Maximum Hitpoints
	/*020*/ uint8	pvpcon;			// Pvp con flag 0/1
	/*021*/ uint8	unknown3[3];
	/*024*/
};

  struct Death_Struct
{
	  /*000*/	uint16	spawn_id;
	  /*002*/	uint16	killer_id; 
	  /*004*/	uint16	corpseid;
	  /*006*/	uint8	spawn_level;
	  /*007*/	uint8   unknown007;
	  /*008*/	int16	spell_id;
	  /*010*/	uint8	attack_skill;
	  /*011*/	uint8   unknonw011;
	  /*012*/	int32	damage;
	  /*016*/	uint8   is_PC;
	  /*017*/	uint8   unknown015[3];
	  /*020*/
};

/*
**  Emu Spawn position update
**	Struct sent from server->client to update position of
**	another spawn's position update in zone (whether NPC or PC)
**   
**  This is modified to support multiple client versions in encodes.
**
*/

  struct SpawnPositionUpdate_Struct
  {
	  /*0000*/ uint16	spawn_id;               // Id of spawn to update
	  /*0002*/ int8	    anim_type; // ??
	  /*0003*/ uint8	heading;                // Heading
	  /*0004*/ int8		delta_heading;          // Heading Change
	  /*0005*/ int16	y_pos;                  // New X position of spawn
	  /*0007*/ int16	x_pos;                  // New Y position of spawn
	  /*0009*/ int16	z_pos;                  // New Z position of spawn
	  /*0011*/
	  struct
	  {
		  uint32 value;

		  // This is our X coordinate, but the client's Y coordinate
		  float GetX()
		  { 
			  uint32 vx1 = value >> 22; // 10 bits long, bits 22 through 31
			  int32 vx = static_cast<int32>(vx1 & 0x200 ? vx1 | 0xFFFFFC00 : vx1); // extend sign

			  // these are multiplied by 16 in client pack_physics()
			  return vx * 0.0625f;
		  }

		  // Our Y coordinate, client's X coordinate
		  float GetY() 
		  { 
			  uint32 vy1 = value & 0x7FF; // 11 bits long, bits 0 through 10
			  int32 vy = static_cast<int32>(vy1 & 0x400 ? vy1 | 0xFFFFF800 : vy1); // extend sign

			  return vy * 0.0625f;
		  }
		  
		  float GetZ() 
		  { 
			  uint32 vz1 = (value >> 11) & 0x7FF; // 11 bits long, bits 10 through 21
			  int32 vz = static_cast<int32>(vz1 & 0x400 ? vz1 | 0xFFFFF800 : vz1); // extend sign

			  return vz * 0.0625f;
		  }

		  // X and Y are reversed in this function to match the above
		  uint32 SetValue(float vx, float vy, float vz)
		  {
			  value = ((int)(float)(vx * 16.0f) << 22) | (((int)(float)(vz * 16.0f) & 0x7FF) << 11) | ((int)(float)(vy * 16.0f) & 0x7FF);

			  return value;
		  }
	  } delta_yzx;
	  /*015*/
  };

  struct SpawnPositionUpdates_Struct
{
	/*0000*/ uint32  num_updates;               // Number of SpawnUpdates
	/*0004*/ struct SpawnPositionUpdate_Struct // Spawn Position Update
						spawn_update;
};


// Used for bulk packet, not yet implemented.
struct PlayerPositionUpdates_Struct
{
	/*0000*/ uint32  num_updates;               // Number of SpawnUpdates
	/*0004*/ struct SpawnPositionUpdate_Struct // Spawn Position Update
						spawn_update[0];
};

/*
** Spawn HP Update
** Length: 10 Bytes
** OpCode: OP_HPUpdate
*/
struct SpawnHPUpdate_Struct
{
	/*000*/ uint32  spawn_id;		// Comment: Id of spawn to update
	/*004*/ int32 cur_hp;		// Comment:  Current hp of spawn
	/*008*/ int32 max_hp;		// Comment: Maximum hp of spawn
	/*012*/
};

struct ManaUpdate_Struct
{
/*00*/ uint16	spawn_id;
/*02*/ uint16	cur_mana;
/*04*/
};

struct SpawnHPUpdate_Struct2
{
/*00*/ int16	spawn_id;
/*02*/ int8		hp;			//HP Percentage
/*03*/
};

/*
** Stamina
** Length: 8 Bytes
** OpCode: 5741
*/
struct Stamina_Struct {
/*00*/ int16 food;		// clamped to 0 - 32000
/*02*/ int16 water;		// clamped to 0 - 32000
/*04*/ int8 fatigue;	// clamped to 0 - 100
/*05*/ //uint8 pad1[3];	// just alignment padding
};

/*
** Level Update
** Length: 12 Bytes
*/
struct LevelUpdate_Struct
{
/*00*/ uint32 level;		// New level
/*04*/ uint32 level_old;	// Old level
/*08*/ uint32 exp;			// Current Experience
};

/*
** Experience Update
** Length: 14 Bytes
** OpCode: 9921
*/
struct ExpUpdate_Struct
{
	/*000*/ uint32 exp;			// Comment: Current experience value
	/*004*/
};

/*
** Item Packet Struct - Works on a variety of opcodes
** Packet Types: See ItemPacketType enum
**
*/
enum ItemPacketType
{
	ItemPacketViewLink			= 0x00,
	ItemPacketMerchant			= 0x64,
	ItemPacketTradeView			= 0x65,
	ItemPacketLoot				= 0x66,
	ItemPacketTrade				= 0x67,
	ItemPacketCharInventory		= 0x69,
	ItemPacketSummonItem		= 0x6A,
	ItemPacketWorldContainer	= 0x6B,
	ItemPacketStolenItem		= 0x6C
};

struct ItemPacket_Struct
{
/*00*/	ItemPacketType	PacketType;
/*04*/	uint16			fromid;
		uint16			toid;
		uint16			skill;
/*06*/	char			SerializedItem[1];
/*xx*/
};

struct BulkItemPacket_Struct
{
/*00*/	char			SerializedItem[0];
/*xx*/
};

struct Consume_Struct
{
	/*000*/ uint32 slot;
	/*004*/ int32 auto_consumed; // 0xffffffff (-1) when auto eating e7030000 (999) when right click
	/*008*/ int32 c_unknown1; // -1
	/*012*/ uint32 type; // 0x01=Food 0x02=Water
	/*016*/
};

struct MoveItem_Struct
{
/*0000*/ uint32 from_slot;
/*0004*/ uint32 to_slot;
/*0008*/ uint32 number_in_stack;
/*0012*/
};

//
// cointype
// 0 - copper
// 1 - silver
// 2 - gold
// 3 - platinum
//
static const uint32 COINTYPE_PP = 3;
static const uint32 COINTYPE_GP = 2;
static const uint32 COINTYPE_SP = 1;
static const uint32 COINTYPE_CP = 0;

struct MoveCoin_Struct
{
	int32 from_slot;
	int32 to_slot;
	uint32 cointype1;
	uint32 cointype2;
	int32	amount;
};

struct TradeCoin_Struct
{
	/*000*/	uint16	trader;
	/*002*/	uint16	slot;
	/*004*/	int32	amount;
	/*008*/
};

struct TradeMoneyUpdate_Struct{
	uint16	trader;
	uint16	type;
	int32	amount;
};
/*
** Surname struct
** Size: 100 bytes
*/
struct Surname_Struct
{
/*0000*/	char name[64];
/*0064*/	uint32 success;
/*0068*/	char lastname[32];
/*0100*/
};

struct GuildsListEntry_Struct 
{
/*0000*/	uint32 guildID;				// Comment: empty = 0xFFFFFFFF
/*0004*/	char name[64];				// Comment: 
/*0068*/	uint32 unknown1;			// Comment: = 0xFF
/*0072*/	uint16 exists;				// Comment: = 1 if exists, 0 on empty
/*0074*/	uint8 unknown2[6];			// Comment: = 0x00
/*0080*/	uint32 unknown3;			// Comment: = 0xFF
/*0084*/	uint8 unknown4[8];			// Comment: = 0x00
/*0092*/	uint32 unknown5;
/*0096*/
};

struct GuildsList_Struct 
{
	uint8 head[4];							// Comment: 
	GuildsListEntry_Struct Guilds[512];		// Comment: 
};

struct GuildUpdate_Struct {
	uint32	guildID;
	GuildsListEntry_Struct entry;
};

/*
** Money Loot
** Length: 22 Bytes
** OpCode: 5020
*/
struct moneyOnCorpseStruct {
/*0000*/ uint8	response;		// 0 = someone else is, 1 = OK, 2 = not at this time
/*0001*/ uint8	unknown1;		// = 0x5a
/*0002*/ uint8	unknown2;		// = 0x40
/*0003*/ uint8	unknown3;		// = 0
/*0004*/ uint32	platinum;		// Platinum Pieces
/*0008*/ uint32	gold;			// Gold Pieces

/*0012*/ uint32	silver;			// Silver Pieces
/*0016*/ uint32	copper;			// Copper Pieces
};

//opcode = 0x5220
// size 292


struct LootingItem_Struct {
	/*000*/	uint16	lootee;
	/*002*/	uint16	looter;
	/*004*/	uint16	slot_id;
	/*006*/	uint8	unknown3[2];
	/*008*/	uint32	auto_loot;
	/*012*/
};

struct GuildInviteAccept_Struct
{
	/*000*/	char inviter[64];
	/*064*/	char newmember[64];
	/*128*/	uint32 response;
	/*132*/	int16 guildeqid;
	/*134*/	uint16 unknown;
	/*136*/
};

struct GuildRemove_Struct
{
	/*000*/	char Remover[64];
	/*064*/	char Removee[64];
	/*128*/	uint16 guildeqid;
	/*130*/	uint8 unknown[2];
	/*132*/	uint32 rank;
	/*136*/
};

struct GuildCommand_Struct {
	char othername[64];
	char myname[64];
	uint16 guildeqid;
	uint8 unknown[2]; // for guildinvite all 0's, for remove 0=0x56, 2=0x02
	uint32 officer;
};

// Opcode OP_GMZoneRequest
// Size = 88 bytes
struct GMZoneRequest_Struct {
/*0000*/	char	charname[64];
/*0064*/	uint32	zone_id;
/*0068*/	float	x;
/*0072*/	float	y;
/*0076*/	float	z;
/*0080*/	float	heading;
/*0084*/	uint32	success;		// 0 if command failed, 1 if succeeded?
/*0088*/
//	/*072*/	int8	success;		// =0 client->server, =1 server->client, -X=specific error
//	/*073*/	uint8	unknown0073[3]; // =0 ok, =ffffff error
};

struct GMSummon_Struct {
/*   0*/	char	charname[64];
/*  64*/	char	gmname[64];
/* 128*/	uint32	success; // not set by client
/* 132*/	uint32	zoneID;
/* 136*/	int	y; // these values are truncated to integer by the client before being sent
/* 140*/	int	x;
/* 144*/	int	z;
/* 148*/	uint32	unknown2; // not set by client
/* 152*/
};

struct GMGoto_Struct { // x,y is swapped as compared to summon and makes sense as own packet
/*  0*/ char	charname[64];

/* 64*/ char	gmname[64];
/* 128*/uint32	success;
/* 132*/ uint32	zoneID;

/*136*/ float	y;
/*140*/ float	x;
/*144*/ float	z;
/*148*/ uint32	unknown2; // E0 E0 56 00
};

struct GMLastName_Struct {
	char name[64];
	char gmname[64];
	char lastname[64];
	char padding196[2];
	int16 response; // request = 0, response = 1
};

//Combat Abilities
struct CombatAbility_Struct {
	/*000*/	uint16 m_target;		//the ID of the target mob
	/*002*/	uint16 unknown;
	/*004*/	uint32 m_atk;
	/*008*/	uint32 m_skill;
	/*012*/
};

struct RandomReq_Struct {
	uint32 low;
	uint32 high;
};

/* solar: 9/23/03 reply to /random command; struct from Zaphod */
struct RandomReply_Struct {
/* 00 */	uint32 low;
/* 04 */	uint32 high;
/* 08 */	uint32 result;
/* 12 */	char name[64];
/* 76 */
};

// EverQuest Time Information:
// 72 minutes per EQ Day
// 3 minutes per EQ Hour
// 6 seconds per EQ Tick (2 minutes EQ Time)
// 3 seconds per EQ Minute

struct TimeOfDay_Struct {
	/*000*/	uint8	hour;			// Comment: 
	/*001*/	uint8	minute;			// Comment: 
	/*002*/	uint8	day;			// Comment: 
	/*003*/	uint8	month;			// Comment: 
	/*004*/	uint16	year;			// Comment: Kibanu - Changed from long to uint16 on 7/30/2009
	/*006*/
};

// Darvik: shopkeeper structs
struct Merchant_Click_Struct {
	/*000*/ uint16	npcid;			// Merchant NPC's entity id
	/*002*/ uint16	playerid;
	/*004*/	uint8  command;
	/*005*/ uint8	unknown[3];
	/*008*/ float   rate;
	/*012*/
};

struct Merchant_Sell_Struct {
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/	uint16	playerid;		// Player's entity id
	/*004*/	uint16	itemslot;
	/*006*/	uint8	IsSold;		// Already sold
	/*007*/	uint8	unknown001;
	/*008*/	uint8	quantity;	// Qty - when used in Merchant_Purchase_Struct
	/*009*/	uint8	unknown004[3];
	/*012*/	uint32	price;
	/*016*/
};

struct Merchant_Purchase_Struct {
/*000*/	uint32	npcid;			// Merchant NPC's entity id
/*004*/	uint32	itemslot;		// Player's entity id
/*008*/	uint32	quantity;
/*012*/	uint32	price;
};

struct OldMerchant_Purchase_Struct {
/*000*/	uint16	npcid;			// Merchant NPC's entity id
/*002*/ uint16  playerid;
/*004*/	uint16	itemslot;		// Player's entity id
/*006*/ uint16  price;
/*008*/	uint8	quantity;
/*009*/ uint8   unknown_void[7];
};

struct Merchant_DelItem_Struct{
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/	uint16	playerid;		// Player's entity id
	/*004*/	uint8	itemslot;       // Slot of the item you want to remove
	/*005*/	uint8	type;     // 0x40
	/*006*/
};

struct Illusion_Struct {
	/*000*/	uint16	spawnid;
	/*002*/	int16	race;
	/*004*/	uint8	gender;
	/*005*/ int8	texture;
	/*006*/ int8	helmtexture;
	/*007*/	int8	unknown007; //Always seems to be 0xFF
	/*008*/ int16	face;
	/*010*/ uint8	hairstyle;
	/*011*/	uint8	haircolor;
	/*012*/	uint8	beard;
	/*013*/	uint8	beardcolor;
	/*014*/	int16	unknown_void;
	/*016*/ int32	size;
	/*020*/
};

/* _MAC_NET_MSG_reward_MacMsg, OP_Reward, Size: 48 */
struct QuestReward_Struct
{
	/*000*/	uint16	mob_id;
	/*002*/	uint16	target_id;
	/*006*/	int32	exp_reward;
	/*010*/	int32	faction;
	/*014*/	int32	faction_mod;
	/*018*/	int32	copper;
	/*022*/	int32	silver;
	/*024*/	int32	gold;
	/*028*/	int32	platinum;
	/*032*/	int16	item_id[QUESTREWARD_COUNT];
	/*048*/
};

struct ZonePoint_Entry {
/*0000*/	uint32	iterator;
/*0004*/	float	y;
/*0008*/	float	x;
/*0012*/	float	z;
/*0016*/	float	heading;
/*0020*/	uint16	zoneid;
/*0022*/	uint16	unused;
};

struct ZonePoints {
/*0000*/	uint32	count;
/*0004*/	struct	ZonePoint_Entry zpe[0]; // Always add one extra to the end after all zonepoints
};

struct SkillUpdate_Struct {
/*00*/	uint32 skillId;
/*04*/	uint32 value;
/*08*/
};

struct SkillUpdate2_Struct {
	/*00*/	uint16 entity_id;
	/*02*/	uint16 align1; // just alignment padding
	/*04*/	int32 skillId;
	/*08*/	int32 value;
	/*12*/
};

struct ZoneUnavail_Struct {
	//This actually varies, but...
	char zonename[16];
	int16 unknown[4];
};

enum {	//Group action fields
	groupActJoin = 0,
	groupActLeave = 1,
	groupActDisband2 = 5,
	groupActDisband = 6,
	groupActUpdate = 7,
	groupActMakeLeader = 8,
	groupActInviteInitial = 9,
	groupActAAUpdate = 10
};

struct GroupGeneric_Struct {
	char name1[64];
	char name2[64];
};

struct GroupSetID_Struct {
	uint32 group_id;
	uint32 char_id;
};

struct GroupCancel_Struct {
	char	name1[64];
	char	name2[64];
	uint8	toggle;
};

struct GroupUpdate_Struct {
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[5][64];
/*0388*/	char	leadersname[64];
/*0580*/	uint8	unknown580[256];
/*0836*/
};

//Client sends a packet with size 136, but expects size 388 back.
struct GroupJoin_Struct {
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[64];
/*0132*/	uint8	unknown196[256];
/*0388*/
};

//Client sends a packet with size 136
struct GroupGeneric_Struct2 {
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[64];
/*0132*/	uint32	param;
/*0136*/
};

struct FaceChange_Struct {
	/*000*/	uint8	haircolor;	// Comment: 
	/*001*/	uint8	beardcolor;	// Comment: 
	/*002*/	uint8	eyecolor1;	// Comment: the eyecolors always seem to be the same, maybe left and right eye?
	/*003*/	uint8	eyecolor2;	// Comment: 
	/*004*/	uint8	hairstyle;	// Comment: 
	/*005*/	uint8	beard;		// Comment: Face Overlay? (barbarian only)
	/*006*/	uint8	face;		// Comment: and beard
	/*007*/
//there are only 10 faces for barbs changing woad just
//increase the face value by ten so if there were 8 woad
//designs then there would be 80 barb faces
};

/*
** Trade request from one client to another
** Used to initiate a trade
** Size: 8 bytes
** Used in: OP_TradeRequest
*/
struct TradeRequest_Struct {
	/*000*/	uint16 to_mob_id;
	/*002*/	uint16 from_mob_id;
	/*004*/
};

/*
** Cancel Trade struct
** Sent when a player cancels a trade
** Size: 8 bytes
** Used In: OP_CancelTrade
**
*/
struct CancelTrade_Struct {
	/*000*/	uint16 fromid;
	/*002*/	uint16 action;
	/*004*/
};

struct RefuseTrade_Struct {
/*00*/	uint16 fromid;
/*02*/	uint16 toid;
/*04*/  uint8  type;
/*04*/  uint8 unknown;
/*06*/
};

struct PetitionUpdate_Struct {
	uint32 petnumber;	// Petition Number
	uint32 color;		// 0x00 = green, 0x01 = yellow, 0x02 = red
	uint32 status;
	time_t senttime;	// 4 has to be 0x1F
	char accountid[32];
	char gmsenttoo[64];
	int32 quetotal;
	char charname[64];
};

struct Petition_Struct {
	uint32 petnumber;
	uint32 urgency;
	char accountid[32];
	char lastgm[32];
	uint32	zone;
	//char zone[32];
	char charname[64];
	uint32 charlevel;
	uint32 charclass;
	uint32 charrace;
	uint32 unknown;
	//time_t senttime; // Time?
	uint32 checkouts;
	uint32 unavail;
	//uint8 unknown5[4];
	time_t senttime;
	uint32 unknown2;
	char petitiontext[1024];
	char gmtext[1024];
};

struct Who_All_Struct 
{ 
	/*000*/	char	whom[64];
	/*064*/	int16	wrace;		// FF FF = no race
	/*066*/	int16	wclass;		// FF FF = no class
	/*068*/	int16	lvllow;		// FF FF = no numbers
	/*070*/	int16	lvlhigh;	// FF FF = no numbers
	/*072*/	int16	gmlookup;	// FF FF = not doing /who all gm
	/*074*/	int16	guildid;	// FF FF = no guild FD FF = lfg 
	/*076*/	int8	unknown076[64];
	/*140*/
};

struct Stun_Struct { // 4 bytes total
	uint32 duration; // Duration of stun
};

// OP_Emote
struct Emote_Struct {
/*0000*/	uint16 unknown01;
/*0002*/	char message[1024];
/*1026*/
};

// Inspect
struct Inspect_Struct {
	uint16 TargetID;
	uint16 PlayerID;
};

struct InspectResponse_Struct 
{ 
	int16 TargetID;			// Comment: ? 
	int16 PlayerID;			// Comment: ?
	int8  unknown[1740];	// Comment: ?
}; 

//OP_SetDataRate
struct SetDataRate_Struct {
	float newdatarate;
};

//OP_SetServerFilter
struct SetServerFilter_Struct 
{
	/*000*/	uint32 filters[17];	// Comment: 
	/*068*/	
};

struct GMName_Struct {
	char oldname[64];
	char gmname[64];
	char newname[64];
	uint8 badname;
	uint8 unknown[3];
};

struct GMDelCorpse_Struct {
	char corpsename[64];
	char gmname[64];
	uint8 unknown;
};

struct GMKick_Struct {
	char name[64];
	char gmname[64];
	uint8 unknown;
};


struct GMKill_Struct {
	char name[64];
	char gmname[64];
	uint8 unknown;
};


struct GMEmoteZone_Struct {
	char text[512];
};

// This is where the Text is sent to the client.
// Use ` as a newline character in the text.
// Variable length.
struct BookText_Struct {
	uint8 type;		//type: 0=scroll, 1=book, 2=item info.. prolly others.
	char booktext[1]; // Variable Length
};

// This is the request to read a book.
// This is just a "text file" on the server
// or in our case, the 'name' column in our books table.
struct BookRequest_Struct {
	uint8 type;		//type: 0=scroll, 1=book, 2=item info.. prolly others.
	char txtfile[20];
};

/*
** Object/Ground Spawn struct
	We rely on the encode/decode to send the proper packet, this is just an internal data struct like PP.
	*/
struct Object_Struct {
/*00*/	uint32	linked_list_addr[2];// <Zaphod> They are, get this, prev and next, ala linked list
/*008*/		uint16  itemid;
/*010*/		uint16  unknown010;
/*012*/		int32	drop_id;	//this id will be used, if someone clicks on this object
/*016*/		int16	zone_id;
/*018*/		uint8   unknown014[6];
/*024*/		uint8   charges;
/*025*/		uint8   unknown25;
/*026*/		uint8   maxcharges;
/*027*/		uint8	unknown027[113];		// ***Placeholder
/*140*/		float   heading;
/*144*/		float	z;
/*148*/		float	x;
/*152*/		float	y;					// Z Position
/*156*/		char	object_name[16];				// ACTOR ID
/*172*/		int8	unknown172[14];
/*186*/		int16	itemsinbag[10]; //if you drop a bag, thats where the items are
/*206*/		int16	unknown206;
/*208*/		int32	unknown208; //set to 0xFF
/*212*/		uint32	object_type;
/*216*/		int16	unknown216; //set to 0xFF
/*218*/		int16	unknown218[2];
/*224*/
};

//<Zaphod> 01 = generic drop, 02 = armor, 19 = weapon
//[13:40] <Zaphod> and 0xff seems to be indicative of the tradeskill/openable items that end up returning the old style item type in the OP_OpenObject

/*
** Click Object Struct
** Client clicking on zone object (forge, groundspawn, etc)
** Size: 8 bytes
** Last Updated: Oct-17-2003
**
*/
struct ClickObject_Struct {
/*00*/	uint32 drop_id;
/*04*/	uint32 player_id;
/*08*/
};

struct Shielding_Struct {
	uint16 target_id;
};

/*
** Click Object Action Struct
** Response to client clicking on a World Container (ie, forge)
** also sent by the client when they close the container.
**
*/
struct ClickObjectAction_Struct {
	/*000*/	uint32	player_id;		// Comment: Entity Id of player who clicked object
	/*004*/	uint32	drop_id;		// Comment: Unknown 
	/*008*/	uint8	open;			// Comment: 0=close 1=open
	/*009*/	uint8	unknown9;		// Comment: Unknown 
	/*010*/	uint8	type;			// Comment: number that determines the type of the item (i.e forge) 
	/*011*/	uint8	unknown11;		// Comment: Unknown 
	/*012*/	uint8	slot;			// Comment: number of slots of the container
	/*013*/  uint8	unknown10[3];	// Comment: Unknown 
	/*016*/	uint16	icon;		// Comment: Icon of the item
	/*018*/	uint8	unknown16[2];	// Comment: Unknown
	/*020*/
};

struct Door_Struct
{
/*0000*/ char    name[16];            // Filename of Door
/*0016*/ float   yPos;               // y loc
/*0020*/ float   xPos;               // x loc
/*0024*/ float   zPos;               // z loc
/*0028*/ float	 heading;
/*0032*/ uint16	 incline;
/*0034*/ uint16	 size;
/*0036*/ uint16	 unknown;
/*0038*/ uint8	 doorid;             // door's id #
/*0039*/ uint8	 opentype;
/*0040*/ uint8	 doorIsOpen;
/*0041*/ uint8	 inverted;
/*0042*/ uint16	 parameter; 
};

struct DoorSpawns_Struct	//SEQ
{
	uint16 count;            
	struct Door_Struct doors[];
};
/*
 OP Code: Op_ClickDoor
 Size:		8
*/
struct ClickDoor_Struct {
	/*000*/	uint8	doorid;
	/*001*/	uint8	unknown[3];
	/*004*/	uint16	item_id;
	/*006*/	uint16	player_id;
	/*008*/
};

struct MoveDoor_Struct {
	uint8	doorid;
	uint8	action;
};


struct BecomeNPC_Struct {
	uint32 id;
	int32 maxlevel;
};

struct Resurrect_Struct	{
/*000*/	uint32	unknown000;
/*004*/	uint16	zone_id;
/*006*/	uint16	unused;
/*008*/	float	y;
/*012*/	float	x;
/*016*/	float	z;
/*020*/	uint32	unknown020;
/*024*/	char	your_name[64];
/*088*/	uint32	unknown088;
/*092*/	char	rezzer_name[64];
/*156*/	uint32	spellid;
/*160*/	char	corpse_name[64];
/*224*/	uint32	action;
/* 228 */
};

struct Translocate_Struct {
/*000*/	uint32	ZoneID;
/*004*/	uint32	SpellID;
/*008*/	uint32	unknown008; //Heading ?
/*012*/	char	Caster[64];
/*076*/	float	y;
/*080*/	float	x;
/*084*/	float	z;
/*088*/	uint32	Complete;
};

struct PendingTranslocate_Struct
{
	uint32 zone_id;
	float heading;
	float x;
	float y;
	float z;
	uint32 spell_id;
};

struct Sacrifice_Struct {
/*000*/	uint32	CasterID;
/*004*/	uint32	TargetID;
/*008*/	uint32	Confirm;
};

struct SetRunMode_Struct {
	uint8 mode;
	uint8 unknown[3];
};

//Bazaar Packets
//

enum {
	BazaarTrader_StartTraderMode = 1,
	BazaarTrader_EndTraderMode = 2,
	BazaarTrader_UpdatePrice = 3,
	BazaarTrader_EndTransaction = 4,
	BazaarSearchResults = 7,
	BazaarWelcome = 9,
	BazaarBuyItem = 10,
	BazaarTrader_ShowItems = 11,
	BazaarSearchDone = 12,
	BazaarTrader_CustomerBrowsing = 13,
	BazaarInspectItem = 18,
	BazaarSearchDone2 = 19,
	BazaarTrader_StartTraderMode2 = 22
};

enum {
	BazaarPriceChange_Fail = 0,
	BazaarPriceChange_UpdatePrice = 1,
	BazaarPriceChange_RemoveItem = 2,
	BazaarPriceChange_AddItem = 3
};

struct BazaarWindowStart_Struct 
{
	uint8	Action;
	uint8	Unknown001;
};

struct BazaarWelcome_Struct 
{
	/*000*/	BazaarWindowStart_Struct Beginning;
	/*002*/ uint16	Unknown002;
	/*004*/	uint32	Traders;
	/*008*/	uint32	Items;
	/*012*/	uint8	Unknown012[8];
	/*020*/	
};

struct BazaarSearch_Struct
{
	/*000*/	BazaarWindowStart_Struct Beginning;
	/*002*/	uint16	TraderID;
	/*004*/	uint16	Class_;
	/*006*/	uint16	Race;
	/*008*/	uint16	ItemStat;
	/*010*/	uint16	Slot;
	/*012*/	uint16	Type;
	/*014*/	char	Name[64];
	/*078*/ uint16  unknown;
	/*080*/	uint32	MinPrice;
	/*084*/	uint32	MaxPrice;
	/*088*/
};

struct BazaarReturnDone_Struct
{
	uint16 Type;
	uint16 Unknown002; //00
	uint32 Unknown004; //FFFF
	uint32 Unknown008; //FFFF
	uint16 Unknown012; //FF
	uint32 Unknown014; 
	uint16 void_;
};

struct BazaarSearchResults_Struct 
{
/*000*/	uint16  Action;
/*002*/	uint16  NumItems;
/*004*/	uint16	ItemID;
/*006*/	uint16	SellerID;
/*008*/	uint32	Cost;
/*012*/	char	ItemName[64];
/*076*/

};

struct Trader_Struct 
{
	/*000*/	uint16	Code;
	/*002*/ uint16  TraderID;
	/*004*/	uint32	Items[80];
	/*324*/	uint32	ItemCost[80];
	/*644*/
};

struct BecomeTrader_Struct
{
	/*000*/	uint16 ID;
	/*002*/ uint16 unknown;
	/*004*/	uint32 Code;
	/*008*/	
};

struct TraderStatus_Struct
{
	/*000*/	uint16 Code;
	/*002*/	uint16 Unknown002;
	/*004*/	uint16 TraderID;
	/*006*/ uint16 Unknown006;
	/*008*/ uint16 BuyerID;
	/*010*/ uint16 Unknown010;
	/*012*/	uint32 Unknown012[2];
	/*020*/	
};

struct Trader_ShowItems_Struct
{
	/*000*/	uint16 Code;
	/*002*/	uint16 TraderID;
	/*004*/ uint32 SubAction;
	/*008*/	uint32 Items[3];
	/*020*/	
};

struct TraderBuy_Struct
{
	/*000*/	uint16 Action;
	/*002*/	uint16 TraderID;
	/*004*/	uint32 ItemID;
	/*008*/	uint32 Price;
	/*012*/	uint16 Quantity;
	/*014*/ uint16 Slot;
	/*016*/	char   ItemName[64];
	/*080*/
};

struct TraderPriceUpdate_Struct
{
	/*000*/	uint16	Action;
	/*002*/	uint16	SubAction;
	/*004*/	int32	ItemID;
	/*008*/	uint32	NewPrice;
	/*012*/	
};

struct BuyerBrowse_Struct
{
	/*000*/ uint16 Code;
	/*002*/ uint16 Unknown002;
	/*004*/ uint16 TraderID;
	/*006*/
};

struct TraderClick_Struct
{
/*000*/	uint16 TraderID;
/*002*/ uint16 Unknown002;
/*004*/	uint32 Unknown004;
/*008*/	uint32 Unknown008;
/*012*/	uint32 Approval;
};

//
// End Bazaar packets

struct ServerSideFilters_Struct {
uint8	clientattackfilters;	// 0) No, 1) All (players) but self, 2) All (players) but group
uint8	npcattackfilters;		// 0) No, 1) Ignore NPC misses (all), 2) Ignore NPC Misses + Attacks (all but self), 3) Ignores NPC Misses + Attacks (all but group)
uint8	clientcastfilters;		// 0) No, 1) Ignore PC Casts (all), 2) Ignore PC Casts (not directed towards self)
uint8	npccastfilters;			// 0) No, 1) Ignore NPC Casts (all), 2) Ignore NPC Casts (not directed towards self)
};

struct	ItemViewRequest_Struct
{
	/*000*/uint16	item_id;
	/*002*/char	item_name[64];
	/*066*/
};

enum {
	PickPocketFailed = 0,
	PickPocketPlatinum = 1,
	PickPocketGold = 2,
	PickPocketSilver = 3,
	PickPocketCopper = 4,
	PickPocketItem = 5
};

struct PickPocket_Struct 
{
// Size 18
    uint16 to;
    uint16 from;
    uint16 myskill;
    uint16 type; // -1 you are being picked, 0 failed , 1 = plat, 2 = gold, 3 = silver, 4 = copper, 5 = item
    uint32 coin;
    uint8 data[6];
};

struct RuleSets_Struct
{
	/* 0x000 */ uint32 enable_FV;
	/* 0x004 */ uint32 enable_pvp;
	/* 0x008 */ uint32 auto_identify;
	/* 0x00C */ uint32 NameGen;
	/* 0x010 */ uint32 Gibberish;
	/* 0x014 */ uint32 test_server;
};

struct LogServer_Struct
{
	/*000*/	uint32	enable_FV; //Is FV ruleset?
	/*004*/	uint32	enable_pvp; //Is a Zek-era server?
	/*008*/	uint32	auto_identify; //Dunno, keep 0
	/*012*/	uint32	NameGen;	// Name generator enabled?
	/*016*/	uint32	Gibberish;
	/*020*/	uint32	test_server;
	/*024*/	uint32	Locale;
	/*028*/	uint32	ProfanityFilter;
	/*032*/	char	worldshortname[32]; //ServerName on disasm
	/*064*/	uint8	loggingServerPassword[32]; //  loggingServerPassword
	/*096*/	char	unknown096[16];	// 'pacman' on live
	/*112*/	char	loggingServerAddress[16];	// '64.37,148,36' on live
	/*126*/	uint8	unknown128[48];
	/*176*/	uint32	loggingServerPort;
	/*180*/	char	localizedEmailAddress[64];	// 'eqdataexceptions@mail.station.sony.com' on live
	/*244*/
};

struct ApproveWorld_Struct 
{
	/*000*/uint8 response;
	/*001*/char name[64];
};

struct ClientError_Struct
{
/*00001*/	char	type;
/*00001*/	char	unknown0001[69];
/*00069*/	char	character_name[64];
/*00134*/	char	unknown134[192];
/*00133*/	char	message[31994];
/*32136*/
};

/*
** ZoneServerInfo_Struct
** Zone server information
** Size: 130 bytes
** Used In: OP_ZoneServerInfo
**
*/
struct ZoneServerInfo_Struct
{
/*0000*/	char	ip[128];
/*0128*/	uint16	port;
};

struct WhoAllPlayer{
	uint16	formatstring;
	uint16	pidstring;
	char*	name;
	uint16	rankstring;
	char*	guild;
	uint16	unknown80[3];
	uint16	zonestring;
	uint32	zone;
	uint16	class_;
	uint16	level;
	uint16	race;
	char*	account;
	uint16	unknown100;
};

struct WhoAllReturnStruct {
/*000*/	uint32	id;
/*004*/	uint16	playerineqstring;
/*006*/	char	line[27];
/*033*/	uint8	unknown35; //0A
/*034*/	uint16	unknown36;//0s
/*036*/	uint16	playersinzonestring;
/*038*/	uint16	unknown44[5]; //0s
/*048*/	uint32	unknown52;//1
/*052*/	uint32	unknown56;//1
/*056*/	uint16	playercount;//1
struct WhoAllPlayer player[0];
};

// The following four structs are the WhoAllPlayer struct above broken down
// for use in World ClientList::SendFriendsWho to accomodate the user of variable
// length strings within the struct above.

struct	WhoAllPlayerPart1 {
	uint16	FormatMSGID;
	uint16	PIDMSGID;
	char	Name[1];
};

struct	WhoAllPlayerPart2 {
	uint16	RankMSGID;
	char	Guild[1];
};

struct	WhoAllPlayerPart3 {
	uint16	Unknown80[3];
	uint16	ZoneMSGID;
	uint32	Zone;
	uint16	Class_;
	uint16	Level;
	uint16	Race;
	char	Account[1];
};

struct	WhoAllPlayerPart4 {
	uint16	Unknown100;
};

struct GetItems_Struct{
	uint32	Items[80];
	int32	SlotID[80];
	int32	Charges[80];
};

struct GetItem_Struct{
	uint32	Items;
	int32	SlotID;
	int32	Charges;
};

struct MoneyUpdate_Struct
{
	int32 platinum;
	int32 gold;
	int32 silver;
	int32 copper;
};

struct FormattedMessage_Struct{
	uint16	unknown0;
	uint16	string_id;
	uint16	type;
	char	message[0];
};
struct SimpleMessage_Struct{
	uint32	string_id;
	uint32	color;
	uint32	unknown8;
};

struct Internal_GuildMemberEntry_Struct {
//	char	name[64];					//variable length
	uint32	level;						//network byte order
	uint32	banker;						//1=yes, 0=no, network byte order
	uint32	class_;						//network byte order
	uint32	rank;						//network byte order
	uint32	time_last_on;				//network byte order
//	char	public_note[1];				//variable length.
	uint16	zone_id;					//network byte order
};

struct Internal_GuildMembers_Struct {	//just for display purposes, this is not actually used in the message encoding.
	char	player_name[64];		//variable length.
	uint32	count;				//network byte order
	uint32	name_length;	//total length of all of the char names, excluding terminators
	uint32	note_length;	//total length of all the public notes, excluding terminators
	Internal_GuildMemberEntry_Struct member[0];
	/*
	* followed by a set of `count` null terminated name strings
	* and then a set of `count` null terminated public note strings
	*/
};

struct GuildMOTD_Struct {
	/*000*/	char	name[64];
	/*064*/	uint32	unknown64;
	/*068*/	char	motd[512];
	/*580*/
};

struct GuildMakeLeader{
	char	name[64];
	char	target[64];
};

struct BugReport_Struct
{
	/*0000*/	char	category_name[64];
	/*0064*/	char	reporter_name[64];
	/*0128*/	char	unused_0128[32];
	/*0160*/	float	pos_x;
	/*0164*/	float	pos_y;
	/*0168*/	float	pos_z;
	/*0172*/	float	heading;
	/*0176*/	char	unknown176[16];
	/*0192*/	char	target_name[64];
	/*0256*/	uint32	optional_info_mask;
	/*0260*/	char	unknown256[2052];
	/*2312*/	char	bug_report[1024];
	/*3336*/	uint32	unknown3336;
	/*3340*/
};

struct IntelBugStruct
{
	/*0000*/	char	reporter_name[64];
	/*0064*/	char	unused_064[32];
	/*0096*/	uint8	unknown96[11];
	/*0107*/	char	bug_report[1024];
	/*1131*/
};

struct Ground_Spawn{
	float max_x;
	float max_y;
	float min_x;
	float min_y;
	float max_z;
	float heading;
	char name[20];
	uint32 item;
	uint32 max_allowed;
	uint32 respawntimer;
};
struct Ground_Spawns {
	struct Ground_Spawn spawn[50]; //Assigned max number to allow
};

struct ZoneInSendName_Struct {
	uint32	unknown0;
	char	name[64];
	char	name2[64];
	uint32	unknown132;
};

struct Split_Struct
{
	uint32	platinum;
	uint32	gold;
	uint32	silver;
	uint32	copper;
};

// Size: 32 Bytes
struct Combine_Struct {
	/*000*/	uint8 worldobjecttype;	// Comment: if its a world object like forge, id will be here
	/*001*/	uint8 unknown001;		// Comment: 
	/*002*/	int16 skill;		// Comment: 
	/*004*/	int16 container_slot;	// Comment: the position of the container, or 1000 if its a world container	
	/*006*/	int16 iteminslot[10];	// Comment: IDs of items in container
	/*026*/	int16 product;		// Comment: 
	/*028*/	int16 difficulty;		// Comment: 
	/*030*/	int16 containerID;		// Comment: ID of container item
	/*032*/
};

struct MerchantList {
	uint32	id;
	uint32	slot;
	uint32	item;
	int16	faction_required;
	int8	level_required;
	uint32	classes_required;
	uint8	probability;
	uint8	quantity;
	float	min_expansion;
	float	max_expansion;
	uint8	qty_left; // Not stored in the DB
};

struct TempMerchantList {
	uint32	npcid;
	uint32	slot;
	uint32	item;
	uint32	charges; //charges/quantity
	uint32	origslot;
	uint32  quantity; //This is used to determine how many charged items we have, since the charges clump together
};


struct NPC_Emote_Struct {
	uint32	emoteid;
	uint8	event_;
	uint8	type;
	char	text[515];
};

struct KeyRing_Data_Struct {
	uint16 keyitem;
	uint32 zoneid;
	uint8 stage;
	std::string name;
};

struct FindPerson_Point {
	float y;
	float x;
	float z;
};

struct FindPersonRequest_Struct {
	uint32	npc_id;
	FindPerson_Point client_pos;
};

//variable length packet of points
struct FindPersonResult_Struct {
	FindPerson_Point dest;
	FindPerson_Point path[0];	//last element must be the same as dest
};

struct SetTitle_Struct {
	uint32	is_suffix;	//guessed: 0 = prefix, 1 = suffix
	uint32	title_id;
};

struct RaidGeneral_Struct {
/*00*/	uint32		action;	//=10
/*04*/	char		player_name[64];	//should both be the player's name
/*68*/	char		leader_name[64];
/*132*/	uint32		parameter;
};

struct RaidAddMember_Struct {
/*000*/ RaidGeneral_Struct raidGen; //param = (group num-1); 0xFFFFFFFF = no group
/*136*/ uint8 _class;
/*137*/	uint8 level;
/*138*/	uint8 isGroupLeader;
/*139*/	uint8 flags[5]; //no idea if these are needed...
};

struct RaidCreate_Struct {
/*00*/	uint32		action;	//=8
/*04*/	char		leader_name[64];
/*68*/	uint32		leader_id;
};

struct Arrow_Struct {
	/*000*/ uint32 type;			//Always 1
	/*004*/ uint32 unknown004;		//Always 0		
	/*008*/ uint32 unknown008;
	/*012*/ float src_y;
	/*016*/ float src_x;
	/*020*/ float src_z;
	/*024*/ float launch_angle;		//heading		
	/*028*/ float tilt;
	/*032*/ float velocity;
	/*036*/ float burstVelocity;
	/*040*/ float burstHorizontal;
	/*044*/ float burstVertical;
	/*048*/ float yaw;
	/*052*/ float pitch;
	/*056*/ float arc;
	/*060*/ uint8 unknown060[4];
	/*064*/	uint16	source_id;
	/*066*/ uint16	target_id;
	/*068*/ uint16  unknown068;
	/*070*/ uint16  unknown070;
	/*072*/ uint32	object_id; //Spell or ItemID
	/*076*/ uint8  light;
	/*077*/ uint8  unknown077;
	/*078*/ uint8  behavior;
	/*079*/ uint8  effect_type; //9 for spell, uses itemtype for items. 28 is also valid, possibly underwater attack?
	/*080*/ uint8  skill;
	/*081*/ char   model_name[16];
	/*097*/ char   buffer[15];
	/*112*/
};

//made a bunch of trivial structs for stuff for opcode finder to use
struct Consent_Struct {
	char name[1];	//always at least a null
};

struct GMToggle_Struct {
	uint8 unknown0[64];
	uint32 toggle;
};

struct GroupInvite_Struct
{
	/*000*/	char invitee_name[64];		// Comment: 
	/*064*/	char inviter_name[64];		// Comment: 
	/*128*/	char unknown[65];		// Comment: 
	/*193*/
};

struct UseAA_Struct {
	/*000*/ int32 begin;
	/*004*/ int16 ability; // skill_id of a purchased AA.
	/*006*/ int16  unknown_void;
	/*008*/ int32 end;
	/*012*/
};

struct AA_Ability {
/*00*/	uint32 skill_id;
/*04*/	uint32 base1;
/*08*/	uint32 base2;
/*12*/	uint32 slot;
};

struct SendAA_Struct {
/* EMU additions for internal use */
	char name[128];
	int16 cost_inc;
	uint8 level_inc;
	uint8 eqmacid;

/*0000*/	uint32 id;
/*0024*/	uint32 class_type;
/*0028*/	uint32 cost;
/*0032*/	uint32 seq;
/*0036*/	uint32 current_level; //1s, MQ2 calls this AARankRequired
/*0040*/	uint32 prereq_skill;		//is < 0, abs() is category #
/*0044*/	uint32 prereq_minpoints; //min points in the prereq
/*0048*/	uint32 type;
/*0052*/	uint32 spellid;
/*0056*/	uint32 spell_type;
/*0060*/	uint32 spell_refresh;
/*0064*/	uint16 classes;
/*0068*/	uint32 max_level;
/*0072*/	uint32 last_id;
/*0076*/	uint32 next_id;
/*0080*/	uint32 cost2;
/*0088*/	uint32 aa_expansion;
/*0092*/	uint32 special_category;
/*0016*/	uint32 account_time_required;
/*0120*/	uint32 total_abilities;
};

struct AA_Action {
/*00*/	uint32	action;
/*04*/	uint32	ability;
/*08*/	uint32	unknown08;
/*12*/	uint32	exp_value;
};

struct AA_Skills {
	uint8 aa_value;
};

struct AATable_Struct {
	uint8 unknown;
	AA_Skills aa_list[226];
};

struct AltAdvStats_Struct {
	/*000*/	int32 experience;
	/*004*/	int16 unspent;
	/*006*/	int8	percentage;
	/*007*/	int8	unknown_void;
	/*008*/
};

struct Weather_Struct
{
	/*000*/	uint32 type;
	/*004*/	uint32 intensity;
	/*008*/	
};

struct ApplyPoison_Struct {
	uint32 inventorySlot;
	uint32 success;
};

struct ControlBoat_Struct
{
	/*000*/	uint16	boatId;			// entitylist id of the boat
	/*002*/	bool	TakeControl;	// 01 if taking control, 00 if releasing it
	/*003*/ uint8   unknown5;	// no idea what these last byte represent
	/*004*/							
};

struct ClearObject_Struct
{
/*000*/	uint8	Clear;	// If this is not set to non-zero there is a random chance of a client crash.
/*001*/	uint8	Unknown001[7];
};

struct GMSearchCorpse_Struct
{
/*000*/	char Unknown000[64];
/*064*/	char Name[64];
/*128*/	uint32 Unknown128;
};

struct BeggingResponse_Struct
{
/*00*/	uint16	target;
/*02*/	uint16	begger;
/*04*/	uint8	skill;
/*05*/	uint8	unknown1;
/*06*/	int8	Result;	// -1 = Request, 0 = Fail, 1 = Plat, 2 = Gold, 3 = Silver, 4 = Copper
/*07*/	uint8	unknown2;
/*08*/	uint32	Amount;
/*12*/	uint32	unknown3;
/*16*/	uint8	unknown4[2];
};

struct CorpseDrag_Struct
{
/*000*/ char CorpseName[64];
/*064*/ char DraggerName[64];
/*128*/ uint8 Unknown128[24];
/*152*/
};

struct Checksum_Struct {
	uint64 checksum;
	uint8  data[2048];
};

struct Disarm_Struct {
/*000*/ int16 entityid;
/*002*/ int16 target;
/*004*/ int16 skill;
/*006*/ int16 status;
/*008*/
};


//C->S, used by the client for adding Soulmarks to a player. 
//0x41D0
struct SoulMarkUpdate_Struct {
	char gmname[64];
	char gmacctname[32];
	char charname[64];
	char acctname[32];
};

//S->C and C->S ENTRY for informing us about players' soulmarks. Used in struct below this one.
struct SoulMarkEntry_Struct {
	char name[64]; //charname
	char accountname[32]; //station name
	char gmname[64];  //charname
	char gmaccountname[32]; //gm station name
	uint32 unix_timestamp; //time of remark
	uint32 type; //time of remark
	char description[256]; //actual text description
};

//S->C C->S PACKET
//0x41D1
struct SoulMarkList_Struct {
	char interrogatename[64];
	SoulMarkEntry_Struct entries[12]; //I have no idea why SOE hardcodes the amount of soulmarks, but it shall never exceed 12. Delete soulmarks I guess in the future /shrug
};

//C->S PACKET
//0x41D2
struct SoulMarkAdd_Struct{
	SoulMarkEntry_Struct entry;
};


struct Feedback_Struct {
/*0000*/ char	name[64];	
/*0064*/ char	unknown[43];
/*0107*/ char	message[1024];
/*1131*/
};

// Struct for Clients Request to Show specific message board
struct MBRetrieveMessages_Struct {
	uint16 entityID;
	uint16 category; // category is the type of board selected by the client
	/* Categories */ 
	/* 00 - OFFICIAL */
	/* 01 - FOR SALE */
	/* 02 - GENERAL */
	/* 03 - HELP WANTED */
	/* 04 - PERSONALS */
	/* 05 - GUILDS */
};


struct MBMessageRetrieval_Struct {		
	uint32 id; 
	char date[10]; /* char year[2]; char month[2]; char day[2]; */
	char unknown4[4];
	char author[64];
	uint8 language;
	char unknown8;
	char subject[64];
	uint8 category;
	char unknown12;
}; 

struct MBMessageRetrievalGen_Struct {		
	uint32 id; 
	char date[10]; /* char year[2]; char month[2]; char day[2]; */
	char unknown4[4];
	char author[64];
	uint8 language;
	char unknown8;
	char subject[64];
	uint8 category;
	char unknown12;
	char message[2048]; // see eqgame function at .text:0047D4E5
}; 


struct MBModifyRequest_Struct {
	uint16 EntityID;
	uint16 id;	
	uint16 category;
		 
}; 

struct MBEraseRequest_Struct {
	 uint16 EntityID;
	 uint16 id;
	 uint16 category;
	 uint16 unknown;

}; 

struct ZoneFlags_Struct {
	uint32 zoneid;
};

struct ConsentDenied_Struct {
	char oname[64];
	char gname[64];
	uint32 corpse_id;
};

struct NameApproval_Struct 
{
	/*000*/ char name[64];
	/*064*/ uint16 race;
	/*066*/ uint16 unknown066;
	/*068*/ uint16 class_;
	/*070*/ uint32 unknown070[2];
	/*078*/
};

struct NameApprovalReply_Struct 
{
	/*000*/ uint8 approval;
	/*001*/
};

struct CharacterConsent
{
	uint32 corpse_id;
	std::string consenter;
};

struct ResetSkill_Struct
{
	/*000*/ uint16 skillid;
	/*002*/ uint16 padding;
	/*004*/ uint32 timer;
};

struct GroupExpSplit_Struct
{
	// The membercount variables are all used for the group bonus. The split uses weighted_levels.
	int8 membercount; // Total members in the group who are in the zone
	int8 close_membercount; // Total members in the group who are in zone and are in physical range of the corpse.
	uint8 max_green_level; // Highest player level in the group who the NPC is not green to.
	uint16 weighted_levels; // The sum of all player levels in the group in range of corpse.  Used to determine group split.
};

struct Sound_Struct
{
	uint16	entityid;
	int16	sound_number;
};

struct CorpsePosition_Struct
{
	uint16 entityid;
	uint16 unused02;
	float y;
	float x;
	float z;
};


//Quarm Custom:

struct LootLockout
{
	uint32 character_id;
	uint32 npctype_id;
	int64 expirydate;
	char npc_name[64];

	LootLockout()
	{
		character_id = 0;
		npctype_id = 0;
		expirydate = 0;
		memset(npc_name, 0, 64);
	}

	bool HasLockout(time_t curTime)
	{
		if (character_id == 0)
			return false;

		if (curTime >= expirydate || expirydate == 0)
			return false;
		return true;
	}
};

struct LootItemLockout
{
	uint32 item_id;
	int64 expirydate;

	LootItemLockout()
	{
		item_id = 0;
		expirydate = 0;
	}

	bool HasLockout(time_t curTime)
	{
		if (expirydate == 0)
			return true;

		if (curTime >= expirydate)
			return false;
		return true;
	}
};

namespace ChallengeRules {

	struct RuleParams {
		ChallengeRules::RuleSet type;
		uint8 min_level;
		uint8 max_level;
		uint8 max_level2;
		uint32 character_id; // If not a group, the ID of the player

		bool IsFteRequired() {
			return ChallengeRules::IsFteRequired(type);
		}
	};

}

struct PlayerEngagementRecord
{
	bool isFlagged = false;
	uint32 character_id = 0;
	char character_name[64] = { 0 };
	uint8 character_level = 0;
	uint8 character_level2 = 0;
	ChallengeRules::RuleSet character_ruleset = ChallengeRules::RuleSet::NORMAL;
	LootLockout lockout = LootLockout();

	bool HasLockout(time_t curTime)
	{
		if (lockout.character_id == 0)
			return false;

		if (curTime >= lockout.expirydate || lockout.expirydate == 0)
			return false;

		return true;
	}

	bool IsSelfFoundAny() {
		return ChallengeRules::IsSelfFoundAny(character_ruleset);
	}

	bool IsFteRequired() {
		return ChallengeRules::IsFteRequired(character_ruleset);
	}

	bool CanGetLootCreditWith(ChallengeRules::RuleParams& group) {
		return ChallengeRules::CanGetLootCreditWith(character_ruleset, character_level, character_level2, group.type, group.max_level, group.max_level2);
	}

	bool CanGetLootCreditWith(ChallengeRules::RuleParams& group, bool sf_credit_check) {
		return CanGetLootCreditWith(group) && (sf_credit_check || !IsFteRequired());
	}
};

// Restore structure packing to default
#pragma pack()

#endif

