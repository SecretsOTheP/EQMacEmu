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
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 04111-1307 USA
*/

#ifndef COMMON_ITEM_DATA_H
#define COMMON_ITEM_DATA_H

/*
 * Note: (Doodman)
 *	This structure has field names that match the DB name exactly.
 *	Please take care as to not mess this up as it should make
 *	everyones life (i.e. mine) much easier. And the DB names
 *	match the field name from the 13th floor (SEQ) item collectors,
 *	so please maintain that as well.
 *
 * Note #2: (Doodman)
 *	UnkXXX fields are left in here for completeness but commented
 *	out since they are really unknown and since the items are now
 *	preserialized they should not be needed. Conversly if they
 *	-are- needed, then they shouldn't be unkown.
 *
 * Note #3: (Doodman)
 *	Please take care when adding new found data fields to add them
 *	to the appropriate structure. Item_Struct has elements that are
 *	global to all types of items only.
 *
 * Note #4: (Doodman)
 *	Made ya look! Ha!
 */

#include "emu_constants.h"


namespace EQ
{
	namespace item {

		enum ItemClass
		{
			ItemClassCommon = 0,
			ItemClassBag,
			ItemClassBook,
			ItemClassCount
		};

		enum ItemTypes : uint8
		{
			/*9138*/	ItemType1HSlash = 0,
			/*9141*/	ItemType2HSlash,
			/*9140*/	ItemType1HPiercing,
			/*9139*/	ItemType1HBlunt,
			/*9142*/	ItemType2HBlunt,
			/*5504*/	ItemTypeBow,
			/*----*/	ItemTypeUnknown1,
			/*----*/	ItemTypeLargeThrowing,
			/*5505*/	ItemTypeShield,
			/*5506*/	ItemTypeScroll,
			/*5507*/	ItemTypeArmor,
			/*5508*/	ItemTypeMisc,			// a lot of random crap has this item use.
			/*7564*/	ItemTypeLockPick,
			/*----*/	ItemTypeUnknown2,
			/*5509*/	ItemTypeFood,
			/*5510*/	ItemTypeDrink,
			/*5511*/	ItemTypeLight,
			/*5512*/	ItemTypeCombinable,		// not all stackable items are this use...
			/*5513*/	ItemTypeBandage,
			/*----*/	ItemTypeSmallThrowing,
			/*----*/	ItemTypeSpell,			// spells and tomes
			/*5514*/	ItemTypePotion,
			/*----*/	ItemTypeFletchedArrows,
			/*0406*/	ItemTypeWindInstrument,
			/*0407*/	ItemTypeStringedInstrument,
			/*0408*/	ItemTypeBrassInstrument,
			/*0405*/	ItemTypePercussionInstrument,
			/*5515*/	ItemTypeArrow,
			/*----*/	ItemTypeUnknown4,
			/*5521*/	ItemTypeJewelry,
			/*----*/	ItemTypeSkull,
			/*5516*/	ItemTypeBook,			// skill-up tomes/books? (would probably need a pp flag if true...)
			/*5517*/	ItemTypeNote,
			/*5518*/	ItemTypeKey,
			/*----*/	ItemTypeCoin,
			/*5520*/	ItemType2HPiercing,
			/*----*/	ItemTypeFishingPole,
			/*----*/	ItemTypeFishingBait,
			/*5519*/	ItemTypeAlcohol,
			/*----*/	ItemTypeKey2,			// keys and satchels?? (questable keys?)
			/*----*/	ItemTypeCompass,
			/*----*/	ItemTypeUnknown5,
			/*----*/	ItemTypePoison,			// might be wrong, but includes poisons
			/*----*/	ItemTypeUnknown6,
			/*----*/	ItemTypeUnknown7,
			/*5522*/	ItemTypeMartial,
			/*----*/	ItemTypeCount

			/*
				Unknowns:

				Mounts?
				Ornamentations?
				GuildBanners?
				Collectible?
				Placeable?
				(others?)
			*/
		};

		enum BagTypes : uint8
		{
			/*3400*/	BagTypeSmallBag = 0,
			/*3401*/	BagTypeLargeBag,
			/*3402*/	BagTypeQuiver,
			/*3403*/	BagTypeBeltPouch,
			/*3404*/	BagTypeWristPouch,
			/*3405*/	BagTypeBackPack,
			/*3406*/	BagTypeSmallChest,
			/*3407*/	BagTypeLargeChest,
			/*----*/	BagTypeBandolier,				// <*Database Reference Only>
			/*3408*/	BagTypeMedicineBag,
			/*3409*/	BagTypeToolBox,
			/*3410*/	BagTypeLexicon,
			/*3411*/	BagTypeMortar,
			/*3412*/	BagTypeSelfDusting,				// Quest container (Auto-clear contents?)
			/*3413*/	BagTypeMixingBowl,
			/*3414*/	BagTypeOven,
			/*3415*/	BagTypeSewingKit,
			/*3416*/	BagTypeForge,
			/*3417*/	BagTypeFletchingKit,
			/*3418*/	BagTypeBrewBarrel,
			/*3419*/	BagTypeJewelersKit,
			/*3420*/	BagTypePotteryWheel,
			/*3421*/	BagTypeKiln,
			/*3422*/	BagTypeKeymaker,				// (no database entries as of peq rev 69)
			/*3423*/	BagTypeWizardsLexicon,
			/*3424*/	BagTypeMagesLexicon,
			/*3425*/	BagTypeNecromancersLexicon,
			/*3426*/	BagTypeEnchantersLexicon,
			/*----*/	BagTypeUnknown1,				// (a coin pouch/purse?) (no database entries as of peq rev 69)
			/*----*/	BagTypeConcordanceofResearch,	// <*Database Reference Only>
			/*3427*/	BagTypeAlwaysWorks,				// Quest container (Never-fail combines?)
			/*3428*/	BagTypeKoadaDalForge,			// High Elf
			/*3429*/	BagTypeTeirDalForge,			// Dark Elf
			/*3430*/	BagTypeOggokForge,				// Ogre
			/*3431*/	BagTypeStormguardForge,			// Dwarf
			/*3432*/	BagTypeAkanonForge,				// Gnome
			/*3433*/	BagTypeNorthmanForge,			// Barbarian
			/*----*/	BagTypeUnknown2,				// (no database entries as of peq rev 69)
			/*3434*/	BagTypeCabilisForge,			// Iksar
			/*3435*/	BagTypeFreeportForge,			// Human 1
			/*3436*/	BagTypeRoyalQeynosForge,		// Human 2
			/*3439*/	BagTypeHalflingTailoringKit,
			/*3438*/	BagTypeErudTailoringKit,
			/*3440*/	BagTypeFierDalTailoringKit,		// Wood Elf
			/*3441*/	BagTypeFierDalFletchingKit,		// Wood Elf
			/*3437*/	BagTypeIksarPotteryWheel,
			/*3442*/	BagTypeTackleBox,
			/*3443*/	BagTypeTrollForge,
			/*3445*/	BagTypeFierDalForge,			// Wood Elf
			/*3444*/	BagTypeValeForge,				// Halfling
			/*3446*/	BagTypeErudForge,
			/*----*/	BagTypeTradersSatchel,			// <*Database Reference Only> (db: Yellow Trader's Satchel Token?)
			/*----*/	BagTypeCount
		};

		enum ItemEffect {
			ItemEffectCombatProc = 0,
			ItemEffectClick,
			ItemEffectWorn,
			ItemEffectExpendable,
			ItemEffectEquipClick,
			ItemEffectClick2, //5		//name unknown
			ItemEffectFocus,
			ItemEffectScroll,
			ItemEffectCount
		};

		enum ItemSize : uint8 {
			ItemSizeTiny = 0,
			ItemSizeSmall,
			ItemSizeMedium,
			ItemSizeLarge,
			ItemSizeGiant,
			ItemSizeCount
		};


		enum ItemQuantity
		{
			Quantity_Unknown = 0,
			Quantity_Normal = 1,
			Quantity_Charges = 2,
			Quantity_Stacked = 3
		};

		struct ItemEffect_Struct {
			int16	Effect;
			int8	Type;
			uint8	Level;
			uint8	Level2;
		};
	}

	struct InternalSerializedItem_Struct {
		int16 slot_id;
		const void* inst;
	};

	// use EmuConstants::ITEM_COMMON_SIZE
	//#define MAX_AUGMENT_SLOTS 5

	struct ItemData {
		// Non packet based fields
		uint8	MinStatus;

		// Packet based fields
		char	Name[64];		// Name
		char	Lore[80];		// Lore Name: *=lore, &=summoned, #=artifact, ~=pending lore
		char	IDFile[30];		// Visible model
		uint8	Weight;			// Item weight * 10
		uint8	NoRent;			// No Rent: 0=norent, 255=not norent
		uint8	NoDrop;			// No Drop: 0=nodrop, 255=not nodrop
		uint8	Size;			// Size: 0=tiny, 1=small, 2=medium, 3=large, 4=giant
		int16	ItemClass;		// Item Type: 0=common, 1=container, 2=book
		uint32	ID;				// Unique ID (also PK for DB)
		uint16	Icon;			// Icon Number
		uint16  Unused;
		int32	Slots;			// Bitfield for which slots this item can be used in
		int32	Price;			// Item cost (?)

		int8	AStr;			// Strength
		int8	ASta;			// Stamina
		int8	AAgi;			// Agility
		int8	ADex;			// Dexterity
		int8	ACha;			// Charisma
		int8	AInt;			// Intelligence
		int8	AWis;			// Wisdom
		int8	MR;				// Save vs Magic
		int8	FR;				// Save vs Fire
		int8	CR;				// Save vs Cold
		int8	DR;				// Save vs Disease
		int8	PR;				// Save vs Poison
		int16	HP;				// HP
		int16	Mana;			// Mana
		int16	AC;				// AC
		int8	MaxCharges;		// Maximum charges items can hold: -1 if not a chargeable item
		int8	GMFlag;
		int8	Light;			// Light
		uint8	Delay;			// Delay * 10
		uint8	Damage;			// Delay between item usage (in 0.1 sec increments)
		item::ItemEffect_Struct Click, Proc, Worn, Focus, Scroll, Bard;
		uint8	Range;			// Range of item
		uint8	ItemType;		// Item Type/Skill (itemClass* from above)
		bool	Magic;			// True=Magic Item, False=not
		uint8	Material;		// Item material type
		uint32	Color;			// RR GG BB 00 <-- as it appears in pc
		int32	Classes;		// Bitfield of classes that can equip item (1 << class#)
		int32	Races;			// Bitfield of races that can equip item (1 << race#)
		bool	Stackable;
		int8	Stackable_; // EQMac uses 0 for non-stackable, 1 for stackable, and 3 indicates the item has a spell effect. 2 seems to be unused.
		int16	StackSize;

		int8	Book;			// 0=Not book, 1=Book
		int16	BookType;
		char	Filename[33];	// Filename for book data

		uint8	BagType;		// 0:Small Bag, 1:Large Bag, 2:Quiver, 3:Belt Pouch ... there are 50 types
		uint8	BagSlots;		// Number of slots: can only be 2, 4, 6, 8, or 10
		uint8	BagSize;		// 0:TINY, 1:SMALL, 2:MEDIUM, 3:LARGE, 4:GIANT
		uint8	BagWR;			// 0->100

		float	SellRate;		// Sell rate
		int32	CastTime;		// Cast Time for clicky effects, in milliseconds. Also determines how long food/drink lasts.
		int32	CastTime_;
		uint32	RecastType;
		uint32	RecastDelay;
		uint32	SkillModType;	// Type of skill for SkillModValue to apply to
		int32	SkillModValue;	// % Mod to skill specified in SkillModType
		int16	BaneDmgRace;	// Bane Damage Race
		int16	BaneDmgBody;	// Bane Damage Body
		uint8	BaneDmgAmt;		// Bane Damage Body Amount
		uint8	RecLevel;		// Recommended level to use item
		uint8	RecSkill;		// Recommended skill to use item (refers to primary skill of item)
		int32	ProcRate;
		uint8	ElemDmgType;		// Elemental Damage Type (1=magic, 2=fire)
		uint8	ElemDmgAmt;		// Elemental Damage
		int32	FactionMod1;	// Faction Mod 1
		int32	FactionMod2;	// Faction Mod 2
		int32	FactionMod3;	// Faction Mod 3
		int32	FactionMod4;	// Faction Mod 4
		int32	FactionAmt1;	// Faction Amt 1
		int32	FactionAmt2;	// Faction Amt 2
		int32	FactionAmt3;	// Faction Amt 3
		int32	FactionAmt4;	// Faction Amt 4
		uint32	Deity;			// Bitmask of Deities that can equip this item
		uint8	ReqLevel;		// Required Level to use item
		uint32	BardType;		// Bard Skill Type
		int32	BardValue;		// Bard Skill Amount
		bool	Tradeskills;	// Is this a tradeskill item?
		bool	QuestItemFlag;
		int8	Soulbound;
		uint8	FVNoDrop;		// Firiona Vie nodrop flag
		float	min_expansion;	//What expansion this item becomes unlocked in.
		float	max_expansion;	//What expansion this item becomes unavailable in.
		uint8	legacy_item;

		// PoP-style "mod2" stats. Sent to the client in spare Item_Struct bytes (see mac_structs.h).
		int8	StrikeThrough;	// % chance to strike through dodge/parry/riposte/block
		int8	StunResist;		// % chance to resist melee stuns
		int8	CombatEffects;	// weapon proc chance bonus, /10 = %
		int8	Accuracy;		// flat to-hit bonus
		int8	Avoidance;		// flat avoidance bonus
		int8	SpellShield;	// % reduction of direct damage spells
		int8	Shielding;		// % reduction of incoming melee damage
		int8	DamageShield;	// adds to an existing spell damage shield

		bool IsEquipable(uint16 Race, uint16 Class) const;
		bool IsClassCommon() const;
		bool IsClassBag() const;
		bool IsClassBook() const;
		bool IsType1HWeapon() const;
		bool IsType2HWeapon() const;
		bool IsTypeShield() const;
	};
}
#endif /*COMMON_ITEM_DATA_H*/
