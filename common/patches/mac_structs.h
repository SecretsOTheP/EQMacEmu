#ifndef MAC_STRUCTS_H_
#define MAC_STRUCTS_H_

#include <string>

namespace Mac {
	namespace structs {

/*
** Compiler override to ensure
** byte aligned structures
*/
#pragma pack(1)

/*
** Color_Struct
** Size: 4 bytes
** Used for convenience
** Merth: Gave struct a name so gcc 2.96 would compile
**
*/

struct Texture_Struct
{
	uint32 Material;
};

struct TextureProfile
{
	union {
		struct {
			Texture_Struct Head;
			Texture_Struct Chest;
			Texture_Struct Arms;
			Texture_Struct Wrist;
			Texture_Struct Hands;
			Texture_Struct Legs;
			Texture_Struct Feet;
			Texture_Struct Primary;
			Texture_Struct Secondary;
		};
		Texture_Struct Slot[EQ::textures::materialCount];
	};
};

struct Tint_Struct
{
	union
	{
		struct
		{
			uint8	blue;
			uint8	green;
			uint8	red;
			uint8	use_tint;	// if there's a tint this is FF
		};
		uint32 Color;
	};
};

struct TintProfile {
	union {
		struct {
			Tint_Struct Head;
			Tint_Struct Chest;
			Tint_Struct Arms;
			Tint_Struct Wrist;
			Tint_Struct Hands;
			Tint_Struct Legs;
			Tint_Struct Feet;
			Tint_Struct Primary;
			Tint_Struct Secondary;
		};
		Tint_Struct Slot[EQ::textures::materialCount];
	};
};

struct NewZone_Struct 
{
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
	/*0570*/	uint16	unknown570;		// ***Placeholder
	/*0572*/
};

struct Spawn_Struct
{
	/*0000*/	uint32  random_dontuse;
	/*0004*/	int8	accel;
	/*0005*/	uint8	heading;			// Current Heading
	/*0006*/	int8	deltaHeading;		// Delta Heading
	/*0007*/	int16	y_pos;				// Y Position
	/*0009*/	int16	x_pos;				// X Position
	/*0011*/	int16	z_pos;				// Z Position
	/*0013*/	uint32	deltaY:11,			// Velocity Y
						deltaZ:11,			// Velocity Z
						deltaX:10;			// Velocity X
	/*0017*/	uint8	void1;
	/*0018*/	uint16	petOwnerId;		// Id of pet owner (0 if not a pet)
	/*0020*/	uint8	animation;
	/*0021*/    uint8	haircolor; 
	/*0022*/	uint8	beardcolor;	
	/*0023*/	uint8	eyecolor1; 
	/*0024*/	uint8	eyecolor2; 
	/*0025*/	uint8	hairstyle; 
	/*0026*/	uint8	beard;
	/*0027*/    uint8   title; //0xff
	/*0028*/	float	size;
	/*0032*/	float	walkspeed;
	/*0036*/	float	runspeed;
	/*0040*/	TintProfile	equipcolors;
	/*0076*/	uint16	spawn_id;			// Id of new spawn
	/*0078*/	int16	bodytype;			// 65 is disarmable trap, 66 and 67 are invis triggers/traps
	/*0080*/	int16	cur_hp;				// Current hp's of Spawn
	/*0082*/	int16	GuildID;			// GuildID - previously Current hp's of Spawn
	/*0084*/	uint16	race;				// Race
	/*0086*/	uint8	NPC;				// NPC type: 0=Player, 1=NPC, 2=Player Corpse, 3=Monster Corpse, 4=???, 5=Unknown Spawn,10=Self
	/*0087*/	uint8	class_;				// Class
	/*0088*/	uint8	gender;				// Gender Flag, 0 = Male, 1 = Female, 2 = Other
	/*0089*/	uint8	level;				// Level of spawn (might be one int8)
	/*0090*/	uint8	invis;				// 0=visable, 1=invisable
	/*0091*/	uint8	sneaking;
	/*0092*/	uint8	pvp;
	/*0093*/	uint8	anim_type;
	/*0094*/	uint8	light;				// Light emitting
	/*0095*/	int8	anon;				// 0=normal, 1=anon, 2=RP
	/*0096*/	int8	AFK;				// 0=off, 1=on
	/*0097*/	int8	summoned_pc;
	/*0098*/	int8	LD;					// 0=NotLD, 1=LD
	/*0099*/	int8	GM;					// 0=NotGM, 1=GM
	/*0100*/	int8	flymode;				
	/*0101*/	int8	bodytexture;
	/*0102*/	int8	helm; 
	/*0103*/	uint8	face;		
	/*0104*/	uint16	equipment[9];		// Equipment worn: 0=helm, 1=chest, 2=arm, 3=bracer, 4=hand, 5=leg, 6=boot, 7=melee1, 8=melee2
	/*0122*/	int16	guildrank;			// ***Placeholder
	/*0124*/	int16	deity;				// Deity.
	/*0126*/	int8	temporaryPet;			
	/*0127*/	char	name[64];			// Name of spawn (len is 30 or less)
	/*0191*/	char	Surname[32];		// Last Name of player
	/*0223*/	uint8	void_;		
	/*0224*/
};

struct SpawnHPUpdate_Struct
{
	/*000*/ uint32  spawn_id;		// Comment: Id of spawn to update
	/*004*/ int32 cur_hp;		// Comment:  Current hp of spawn
	/*008*/ int32 max_hp;		// Comment: Maximum hp of spawn
	/*012*/	
};

struct SpecialMesg_Struct
{
	/*0000*/ uint32 msg_type;		// Comment: Type of message
	/*0004*/ char  message[0];		// Comment: Message, followed by four bytes?
};

#define ITEM_STRUCT_SIZE 360
#define SHORT_BOOK_ITEM_STRUCT_SIZE	264
#define SHORT_CONTAINER_ITEM_STRUCT_SIZE 276
struct Item_Struct
{
	/*0000*/ char		Name[64];			// Name of item
	/*0064*/ char		Lore[80];			// Lore text
	/*0144*/ char		IDFile[30];			// This is the filename of the item graphic when held/worn.
	/*0174*/ uint8		Weight;				// Weight of item
	/*0175*/ uint8		NoRent;				// Nosave flag 1=normal, 0=nosave, -1=spell?
	/*0176*/ uint8		NoDrop;				// Nodrop flag 1=normal, 0=nodrop, -1=??
	/*0177*/ uint8		Size;				// Size of item
	/*0178*/ int16		ItemClass;
	/*0180*/ int16		ID;					// Record number. Confirmed to be signed.
	/*0182*/ uint16		Icon;				// Icon Number
	/*0184*/ int16		equipSlot;			// Current slot location of item
	/*0186*/ uint8		unknown0186[2];		// Client dump has equipSlot/location as a short so this is still unknown
	/*0188*/ int32		Slots;				// Slots where this item is allowed
	/*0192*/ int32		Price;				// Item cost in copper
	/*0196*/ float		cur_x;				//Here to 227 are named from client struct dump.
	/*0200*/ float		cur_y;
	/*0204*/ float		cur_z;
	/*0208*/ float		heading;
	/*0212*/ uint32		inv_refnum;			// Unique serial. This is required by apply poison.
	/*0216*/ int16		log; 
	/*0218*/ int16		loot_log;
	/*0220*/ int16		avatar_level;		//Usually 01, sometimes seen as FFFF, once as 0.
	/*0222*/ int16		bottom_feed;
	/*0224*/ uint32		poof_item;
	union
	{
		struct
		{
			// 0228- have different meanings depending on flags
			/*0228*/ int8		AStr;				// Strength
			/*0229*/ int8		ASta;				// Stamina
			/*0230*/ int8		ACha;				// Charisma
			/*0231*/ int8		ADex;				// Dexterity
			/*0232*/ int8		AInt;				// Intelligence
			/*0233*/ int8		AAgi;				// Agility
			/*0234*/ int8		AWis;				// Wisdom
			/*0235*/ int8		MR;					// Magic Resistance
			/*0236*/ int8		FR;					// Fire Resistance
			/*0237*/ int8		CR;					// Cold Resistance
			/*0238*/ int8		DR;					// Disease Resistance
			/*0239*/ int8		PR;					// Poison Resistance
			/*0240*/ int16		HP;					// Hitpoints
			/*0242*/ int16		Mana;				// Mana
			/*0244*/ int16		AC;					// Armor Class
			/*0246*/ int8		MaxCharges;			// Maximum number of charges, for rechargable? (Sept 25, 2002)
			/*0247*/ uint8      GMFlag;				// GM flag 0  - normal item, -1 - gm item (Sept 25, 2002)
			/*0248*/ uint8		Light;				// Light effect of this item
			/*0249*/ uint8		Delay;				// Weapon Delay
			/*0250*/ uint8		Damage;				// Weapon Damage
			/*0251*/ int8		EffectType1;		// 0=combat, 1=click anywhere w/o class check, 2=latent/worn, 3=click anywhere EXPENDABLE, 4=click worn, 5=click anywhere w/ class check, -1=no effect
			/*0252*/ uint8		Range;				// Range of weapon
			/*0253*/ uint8		ItemType;			// Skill of this weapon, refer to weaponskill chart
			/*0254*/ uint8      Magic;				// Magic flag
			/*0255*/ uint8      EffectLevel1;		// Casting level
			/*0256*/ uint32		Material;			// Material
			/*0260*/ uint32		Color;				// Amounts of RGB in original color
			/*0264*/ int16		Faction;			// Structs dumped from client has this as Faction
			/*0266*/ int16		Effect1;			// SpellID of special effect
			/*0268*/ int32		Classes;			// Classes that can use this item
			/*0272*/ int32		Races;				// Races that can use this item
			/*0276*/ int8		Stackable;			//  1= stackable, 3 = normal, 0 = ? (not stackable)			
		} common; 
		struct
		{
			/*0228*/ int16		BookType;			// Type of book (scroll, note, etc)
			/*0230*/ int8		Book;				// Are we a book
			/*0231*/ char		Filename[30];		// Filename of book text on server
			/*0261*/ int32		buffer1[4];			// Not used, fills out space in the packet so ShowEQ doesn't complain.
		} book;
		struct
		{
			/*0228*/ int32		buffer2[10];		// Not used, fills out space in the packet so ShowEQ doesn't complain.
			/*0268*/ uint8		BagType;			//Bag type (obviously)
			/*0269*/ uint8		BagSlots;			// number of slots in container
			/*0270*/ uint8		IsBagOpen;			// 1 if bag is open, 0 if not.
			/*0271*/ uint8		BagSize;			// Maximum size item container can hold
			/*0272*/ uint8		BagWR;				// % weight reduction of container
			/*0273*/ uint32		buffer3;			// Not used, fills out space in the packet so ShowEQ doesn't complain.
		} container;
	};
	/*0277*/ uint8		EffectLevel2;				// Casting level
	/*0278*/ int8		Charges;					// Number of charges (-1 = unlimited)
	/*0279*/ int8		EffectType2;				// 0=combat, 1=click anywhere w/o class check, 2=latent/worn, 3=click anywhere EXPENDABLE, 4=click worn, 5=click anywhere w/ class check, -1=no effect
	/*0280*/ uint16		Effect2;					// spellId of special effect
	/*0282*/ int16		Effect2Duration;			// seen in client decompile being set to duration of Effect2 spell but purpose unknown
	/*0284*/ int16		HouseLockID;				// MSG_REQ_HOUSELOCK
	/*0286*/ uint8		unknown0286[2];
	/*0288*/ float		SellRate;
	/*0292*/ int32		CastTime;					// Cast time of clicky item in miliseconds
	/*0296*/ uint8		unknown0296[12];			// ***Placeholder
	/*0308*/ int32		RecastTime;					// Recast time of clicky item in milliseconds
	/*0312*/ uint16		SkillModType;
	/*0314*/ int16		SkillModValue;
	/*0316*/ int16		BaneDmgRace;
	/*0318*/ int16		BaneDmgBody;
	/*0320*/ uint8		BaneDmgAmt;
	/*0321*/ uint8		unknown0321;
	/*0322*/ uint16		title;
	/*0324*/ uint8		RecLevel;					// max should be 65
	/*0325*/ uint8		RecSkill;					// Max should be 252
	/*0326*/ int16		ProcRate; 
	/*0328*/ uint8		ElemDmgType; 
	/*0329*/ uint8		ElemDmgAmt;
	/*0330*/ int16		FactionMod1;
	/*0332*/ int16		FactionMod2;
	/*0334*/ int16		FactionMod3;
	/*0336*/ int16		FactionMod4;
	/*0338*/ int16		FactionAmt1;
	/*0340*/ int16		FactionAmt2;
	/*0342*/ int16		FactionAmt3;
	/*0344*/ int16		FactionAmt4;
	/*0346*/ uint16		Void346;
	/*0348*/ int32		Deity;						// Bitmask of Deities that can equip this item
	/*0352*/ int16		ReqLevel;					// Required level
	/*0354*/ int16		BardType;
	/*0356*/ int16		BardValue;
	/*0358*/ int16		FocusEffect;				//Confirmed
	/*0360*/	
};

struct PlayerItemsPacket_Struct
{
	/*000*/	int16		opcode;		// OP_ItemTradeIn
	/*002*/	struct Item_Struct	item;
};

struct PlayerItems_Struct 
{
	/*000*/	int16		count;
	/*002*/	struct PlayerItemsPacket_Struct	packets[0];
};

struct MerchantItemsPacket_Struct 
{
	/*000*/	uint16 itemtype;
	/*002*/	struct Item_Struct	item;
};

struct TradeItemsPacket_Struct
{
	/*000*/	uint16 fromid;
	/*002*/	uint16 slotid;
	/*004*/	uint8  unknown;
	/*005*/	struct Item_Struct	item;
	/*000*/	uint8 unknown1[5];
	/*000*/	
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

struct PickPocketItemPacket_Struct 
{
    uint16 to;
    uint16 from;
    uint16 myskill;
    uint16 type; // -1 you are being picked, 0 failed , 1 = plat, 2 = gold, 3 = silver, 4 = copper, 5 = item
    uint32 coin;
	struct Item_Struct	item;
};

struct MerchantItems_Struct
{
	/*000*/	int16		count;	
	/*002*/	struct MerchantItemsPacket_Struct packets[0];
};

struct MoveItem_Struct
{
	/*000*/ uint32 from_slot; 
	/*004*/ uint32 to_slot;
	/*008*/ uint32 number_in_stack;
	/*012*/	
};

struct CancelTrade_Struct 
{ 
	/*000*/	uint16 fromid;
	/*002*/	uint16 action;
	/*004*/	
};

struct Merchant_Click_Struct 
{
	/*000*/ uint16	npcid;			// Merchant NPC's entity id
	/*002*/ uint16	playerid;
	/*004*/	uint8  command;
	/*005*/ uint8	unknown[3];
	/*008*/ float   rate;
	/*012*/	
};
	
struct Merchant_Sell_Struct
{
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

struct Merchant_Purchase_Struct
{
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/ uint16  playerid;
	/*004*/	uint16	itemslot;		// Player's entity id
	/*006*/ uint16  price;
	/*008*/	uint8	quantity;
	/*009*/ uint8   unknown_void[7];
	/*016*/	
};

struct Merchant_DelItem_Struct
{
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/	uint16	playerid;		// Player's entity id
	/*004*/	uint8	itemslot;       // Slot of the item you want to remove
	/*005*/	uint8	type;     // 0x40
	/*006*/	
};

struct SetDataRate_Struct 
{
	/*000*/	float newdatarate;	// Comment: 
	/*004*/	
};

// Added this struct for eqemu and started eimplimentation ProcessOP_SendLoginInfo
//TODO: confirm everything in this struct
// PPC Mac version of this is 196 bytes, Intel Mac version is 200 bytes
struct LoginInfo_Struct 
{
	/*000*/	char	AccountName[127];
	/*127*/	char	Password[24];
	/*151*/ uint8	unknown189[41];		
	/*192*/ uint8   zoning;
	/*193*/ uint8   unknown193[3];
	/*196*/
};

struct SpellBuff_Struct
{
	/*000*/uint8  bufftype;        // Comment: 0 = Buff not visible, 1 = Visible and permanent buff, 2 = Visible and timer on, 4 = reverse effect values, used for lifetap type things
	/*001*/uint8  level;			// Comment: Level of person who casted buff
	/*002*/uint8  bard_modifier;	// Comment: this seems to be the bard modifier, it is normally 0x0A because we set in in the CastOn_Struct when its not a bard, else its the instrument mod
	/*003*/uint8  activated;    // Copied from spell data to buff struct.  Only a few spells have this set to 1, the rest are 0
	/*004*/uint16 spellid;        // spell id
	/*006*/uint16 duration;        // Duration in ticks
	/*008*/uint16 counters;        // rune amount, poison/disease/curse counters
	/*010*/
};

// Length: 10
struct ItemProperties_Struct
{

	/*000*/	uint8	unknown01[2];
	/*002*/	int8	charges;				// Comment: signed int because unlimited charges are -1
	/*003*/	uint8	unknown02[7];
	/*010*/
};

/*
	*Used in PlayerProfile
	*/
struct AA_Array
{
	uint8 AA;
	uint8 value;
};

static const uint32  MAX_PP_AA_ARRAY		= 120;
static const uint32 MAX_PP_SKILL		= 100; // _SkillPacketArraySize;	// 100 - actual skills buffer size
struct PlayerProfile_Struct
{
	#define pp_inventory_size 30
	#define pp_containerinv_size 80
	#define pp_cursorbaginventory_size 10
	#define pp_bank_inv_size 8
	/* ***************** */
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
	/*0188*/    TintProfile   EquipColor;      //
	/*0224*/	int16	inventory[pp_inventory_size];		// Player Inventory Item Numbers
	/*0284*/	uint8	languages[32];		// Player Languages - space for 32 but only the first 25 are used
	/*0316*/	struct	ItemProperties_Struct	invItemProperties[pp_inventory_size];	// These correlate with inventory[30]
	/*0616*/	struct	SpellBuff_Struct	buffs[15];	// Player Buffs Currently On
	/*0766*/	int16	containerinv[pp_containerinv_size];	
	/*0926*/	int16   cursorbaginventory[pp_cursorbaginventory_size]; // If a bag is in slot 0, this is where the bag's items are
	/*0946*/	struct	ItemProperties_Struct	bagItemProperties[pp_containerinv_size];	// Just like InvItemProperties
	/*1746*/    struct  ItemProperties_Struct	cursorItemProperties[pp_cursorbaginventory_size];	  //just like invitemprops[]
	/*1846*/	int16	spell_book[spells::SPELLBOOK_SIZE];	// Player spells scribed in their book
	/*2358*/	uint8	unknown2374[512];	// 0xFF
	/*2870*/	int16	mem_spells[spells::SPELL_GEM_COUNT];	// Player spells memorized
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
	/*3393*/	uint8	unknown3393[23];
	/*3416*/	int32	hunger_level;
	/*3420*/	int32	thirst_level;
	/*3424*/	SpellBuff_Struct buff_unknown3424[2];
	/*3444*/	uint32	zone_id;
	/*3448*/	uint8	unknown3448[126];	// On AK, there was a lot of data here in the packet the client sent for OP_Save, but none in the encoded packet the server sent at zone in.
	/*3574*/	SpellBuff_Struct buffs_ext[15];
	/*3724*/	uint16  buff_caster_id[30];
	/*3784*/	uint32	bind_point_zone[5];	
	/*3804*/	float	bind_y[5];
	/*3824*/	float	bind_x[5];
	/*3844*/	float	bind_z[5];
	/*3864*/	float	bind_heading[5];
	/*3884*/	ItemProperties_Struct	bankinvitemproperties[pp_bank_inv_size];
	/*3964*/	ItemProperties_Struct	bankbagitemproperties[pp_containerinv_size];
	/*4764*/	uint32	login_time;
	/*4768*/	int16	bank_inv[pp_bank_inv_size];		// Player Bank Inventory Item Numbers
	/*4784*/	int16	bank_cont_inv[pp_containerinv_size];	// Player Bank Inventory Item Numbers (Bags)
	/*4944*/	uint16	deity;		// ***Placeholder
	/*4946*/	uint16	guild_id;			// Player Guild ID Number
	/*4948*/	uint32  birthday;
	/*4952*/	uint32  lastlogin;
	/*4956*/	uint32  timePlayedMin;
	/*4960*/	int8    thirst_level_unused;
	/*4961*/    int8    hunger_level_unused;
	/*4962*/	int8   fatigue;
	/*4963*/	uint8	pvp;				// Player PVP Flag
	/*4964*/	uint8	level2;	
	/*4965*/	uint8	anon;				// Player Anon. Flag
	/*4966*/	uint8	gm;					// Player GM Flag
	/*4967*/	uint8	guildrank;			// Player Guild Rank (0=member, 1=officer, 2=leader)
	/*4968*/    uint8   intoxication;
	/*4969*/	uint8	eqbackground;
	/*4970*/	uint8	unknown4760[2];
	/*4972*/	uint32	spellSlotRefresh[spells::SPELL_GEM_COUNT];
	/*5004*/	uint32	unknown5003;
	/*5008*/	uint32	abilitySlotRefresh;
	/*5012*/	char	groupMembers[6][64];	// Group Members
	/*5396*/	uint8	unknown5396[20];	
	/*5416*/	uint32	groupdat;
	/*5420*/	uint32	expAA;				// Post60Exp
	/*5424*/    uint8	title;
	/*5425*/	uint8	perAA;			    // Player AA Percent
	/*5426*/	uint8	haircolor;			// Player Hair Color
	/*5427*/	uint8	beardcolor;			// Player Beard Color
	/*5428*/	uint8	eyecolor1;			// Player Left Eye Color
	/*5429*/	uint8	eyecolor2;			// Player Right Eye Color
	/*5430*/	uint8	hairstyle;			// Player Hair Style
	/*5431*/	uint8	beard;				// Player Beard Type
	/*5432*/	uint8	luclinface;				// Player Face Type mostly 0 in packet
	/*5433*/	TextureProfile item_material;
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
	/*8460*/
};

//Server sends this packet for reuse timers
//Client sends this packet as 256 bytes, and is our equivlent of AA_Action
struct UseAA_Struct
{
	/*000*/ int32 begin;
	/*004*/ int16 ability; // skill_id of a purchased AA.
	/*006*/ int16  unknown_void; 
	/*008*/ int32 end;
	/*012*/

};

/*Not Implemented*/

//           No idea what this is used for, but it creates a
//           perminent object that no client may interact with.
//			 It also accepts spell sprites (i.e., GENC00), but 
//			 they do not currently display. I guess we could use 
//			 this for GM events?
//
//Opcode: 0xF640 MSG_ADD_OBJECT
struct ObjectDisplayOnly_Struct
{
	/*0000*/ char test1[32];
	/*0032*/ char modelName[16];	
	/*0048*/ char test2[12];
	/*0060*/ float size;			
	/*0064*/ float y;				
	/*0068*/ float x;				
	/*0072*/ float z;				
	/*0076*/ float heading;			
	/*0080*/ float tilt;
	/*0084*/ char test4[40];
};

	};	//end namespace structs
};	//end namespace MAC

#endif /*MAC_STRUCTS_H_*/
