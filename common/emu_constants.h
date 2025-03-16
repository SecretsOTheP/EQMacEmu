/*	EQEMu:  Everquest Server Emulator
	
	Copyright (C) 2001-2022 EQEMu Development Team (http://eqemulator.net)
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef COMMON_EMU_CONSTANTS_H
#define COMMON_EMU_CONSTANTS_H

#include "eq_limits.h"
#include "emu_versions.h"
#include "bodytypes.h"

#include <string.h>

namespace EQ 
{
	using Mac::IINVALID;
	using Mac::INULL;

	namespace inventory {

	} /*inventory*/

	namespace invtype {
		using namespace Mac::invtype::enum_;

		using Mac::invtype::POSSESSIONS_SIZE;
		using Mac::invtype::BANK_SIZE;
		using Mac::invtype::BANK_8_SIZE;
		using Mac::invtype::TRADE_SIZE;
		using Mac::invtype::WORLD_SIZE;
		using Mac::invtype::LIMBO_SIZE;
		using Mac::invtype::MERCHANT_SIZE;
		using Mac::invtype::CORPSE_SIZE;
		using Mac::invtype::BAZAAR_SIZE;
		using Mac::invtype::INSPECT_SIZE;

		using Mac::invtype::TRADE_NPC_SIZE;

		using Mac::invtype::TYPE_INVALID;
		using Mac::invtype::TYPE_BEGIN;
		using Mac::invtype::TYPE_END;
		using Mac::invtype::TYPE_COUNT;

		int16 GetInvTypeSize(int16 inv_type);
		using Mac::invtype::GetInvTypeName;

	} // namespace invtype

	namespace invslot {
		using namespace Mac::invslot::enum_;

		using Mac::invslot::SLOT_INVALID;
		using Mac::invslot::SLOT_BEGIN;

		using Mac::invslot::POSSESSIONS_BEGIN;
		using Mac::invslot::POSSESSIONS_END;
		using Mac::invslot::POSSESSIONS_COUNT;

		using Mac::invslot::EQUIPMENT_BEGIN;
		using Mac::invslot::EQUIPMENT_END;
		using Mac::invslot::EQUIPMENT_COUNT;

		using Mac::invslot::GENERAL_BEGIN;
		using Mac::invslot::GENERAL_END;
		using Mac::invslot::GENERAL_COUNT;

		using Mac::invslot::BONUS_BEGIN;
		using Mac::invslot::BONUS_STAT_END;
		using Mac::invslot::BONUS_SKILL_END;

		using Mac::invslot::BANK_BEGIN;
		using Mac::invslot::BANK_END;
		using Mac::invslot::BANK_8_END;

		using Mac::invslot::TRADE_BEGIN;
		using Mac::invslot::TRADE_END;

		using Mac::invslot::TRADE_NPC_END;

		using Mac::invslot::WORLD_BEGIN;
		using Mac::invslot::WORLD_END;

		const int16 CORPSE_BEGIN = invslot::slotGeneral1;
		const int16 CORPSE_END = CORPSE_BEGIN + invslot::slotGeneral8;

		const int16 CURSOR_QUEUE_BEGIN = 8000;
		const int16 CURSOR_QUEUE_END = 8999;

		using Mac::invslot::POSSESSIONS_BITMASK;
		using Mac::invslot::CORPSE_BITMASK;

		using Mac::invslot::GetInvPossessionsSlotName;
		using Mac::invslot::GetInvSlotName;

	} // namespace invslot

	namespace DevTools {
		const int32 GM_ACCOUNT_STATUS_LEVEL = 150;
	}

	namespace invbag {
		using Mac::invbag::SLOT_INVALID;
		using Mac::invbag::SLOT_BEGIN;
		using Mac::invbag::SLOT_END;
		using Mac::invbag::SLOT_COUNT;

		using Mac::invbag::GENERAL_BAGS_BEGIN;
		const int16 GENERAL_BAGS_COUNT = invslot::GENERAL_COUNT * SLOT_COUNT;
		const int16 GENERAL_BAGS_END = (GENERAL_BAGS_BEGIN + GENERAL_BAGS_COUNT) - 1;

		const int16 GENERAL_BAGS_8_COUNT = 8 * SLOT_COUNT;
		const int16 GENERAL_BAGS_8_END = (GENERAL_BAGS_BEGIN + GENERAL_BAGS_8_COUNT) - 1;

		const int16 CURSOR_BAG_BEGIN = 330;
		const int16 CURSOR_BAG_COUNT = SLOT_COUNT;
		const int16 CURSOR_BAG_END = (CURSOR_BAG_BEGIN + CURSOR_BAG_COUNT) - 1;

		using Mac::invbag::BANK_BAGS_BEGIN;
		const int16 BANK_BAGS_COUNT = (invtype::BANK_SIZE * SLOT_COUNT);
		const int16 BANK_BAGS_END = (BANK_BAGS_BEGIN + BANK_BAGS_COUNT) - 1;

		const int16 BANK_BAGS_8_COUNT = (invtype::BANK_8_SIZE * SLOT_COUNT);
		const int16 BANK_BAGS_8_END = (BANK_BAGS_BEGIN + BANK_BAGS_8_COUNT) - 1;

		using Mac::invbag::TRADE_BAGS_BEGIN;
		const int16 TRADE_BAGS_COUNT = invtype::TRADE_SIZE * SLOT_COUNT;
		const int16 TRADE_BAGS_END = (TRADE_BAGS_BEGIN + TRADE_BAGS_COUNT) - 1;

		using Mac::invbag::GetInvBagIndexName;

	} // namespace invbag

	namespace constants {
		// database
		static const EQ::versions::ClientVersion CharacterCreationClient = EQ::versions::ClientVersion::Mac; // adjust according to starting item placement and target client
		using Mac::constants::CHARACTER_CREATION_LIMIT;

		const size_t SAY_LINK_OPENER_SIZE = 1;
		using Mac::constants::SAY_LINK_BODY_SIZE;
		const size_t SAY_LINK_TEXT_SIZE = 400;
		const size_t SAY_LINK_CLOSER_SIZE = 1;
		const size_t SAY_LINK_MAXIMUM_SIZE = (SAY_LINK_OPENER_SIZE + SAY_LINK_BODY_SIZE + SAY_LINK_TEXT_SIZE + SAY_LINK_CLOSER_SIZE);

		enum GravityBehavior : int8 {
			Ground,
			Flying,
			Levitating,
			Water
		};

		enum SpawnAnimations : uint8 {
			Standing,
			Sitting,
			Crouching,
			Laying,
			Looting
		};

		enum EmoteEventTypes : uint8 {
			LeaveCombat,
			EnterCombat,
			OnDeath,
			AfterDeath,
			Hailed,
			KilledPC,
			KilledNPC,
			OnSpawn,
			OnDespawn,
			Killed
		};

		enum EmoteTypes : uint8 {
			Say,
			Emote,
			Shout,
			Proximity
		};

		extern const std::map<uint8, std::string>& GetLanguageMap();
		std::string GetLanguageName(uint8 language_id);

		extern const std::map<int8, std::string>& GetFlyModeMap();
		std::string GetFlyModeName(int8 flymode_id);

		extern const std::map<uint8, std::string>& GetAccountStatusMap();
		std::string GetAccountStatusName(uint8 account_status);

		extern const std::map<uint8, std::string>& GetConsiderLevelMap();
		std::string GetConsiderLevelName(uint8 consider_level);


		extern const std::map<uint8, std::string>& GetSpawnAnimationMap();
		std::string GetSpawnAnimationName(uint8 animation_id);

		extern const std::map<uint32, std::string> &GetAppearanceTypeMap();
		std::string GetAppearanceTypeName(uint32 animation_type);

		extern const std::map<uint8, std::string> &GetEmoteEventTypeMap();
		std::string GetEmoteEventTypeName(uint8 emote_event_type);

		extern const std::map<uint8, std::string> &GetEmoteTypeMap();
		std::string GetEmoteTypeName(uint8 emote_type);
	}


	namespace spells {
		enum class CastingSlot : int32 {
			Invalid = -1,
			Gem1 = 0,
			Gem2 = 1,
			Gem3 = 2,
			Gem4 = 3,
			Gem5 = 4,
			Gem6 = 5,
			Gem7 = 6,
			Gem8 = 7,
			MaxGems = 8,
			Ability = 9,
			Item = 10,
			AltAbility = 0xFF
		};

		using Mac::spells::SPELL_ID_MAX;
		using Mac::spells::SPELLBOOK_SIZE;
		using Mac::spells::SPELL_GEM_COUNT;

	}

	namespace profile {

		using Mac::profile::SKILL_ARRAY_SIZE;

	} // namespace profile

	namespace behavior {
		using Mac::behavior::CoinHasWeight;

	} // namespace behavior

	namespace bug {
		enum CategoryID : uint32 {
			catOther = 0,
			catVideo,
			catAudio,
			catPathing,
			catQuest,
			catTradeskills,
			catSpellStacking,
			catDoorsPortals,
			catItems,
			catNPC,
			catDialogs,
		};

		enum OptionalInfoFlag : uint32 {
			infoNoOptionalInfo = 0x0,
			infoCanDuplicate = 0x1,
			infoCrashBug = 0x2,
			infoTargetInfo = 0x4,
			infoCharacterFlags = 0x8,
			infoUnknownValue = 0xFFFFFFF0
		};

		const char* CategoryIDToCategoryName(CategoryID category_id);
		CategoryID CategoryNameToCategoryID(const char* category_name);

	} // namespace bug

	enum WaypointStatus : int {
		QuestControlNoGrid = -2,
		QuestControlGrid = -1
	};
}

enum ServerLockType : int {
	List,
	Lock,
	Unlock
};

enum AccountStatus : uint8 {
	Player = 0,
	Steward = 10,
	ApprenticeGuide = 20,
	Guide = 50,
	QuestTroupe = 80,
	SeniorGuide = 81,
	GMTester = 85,
	EQSupport = 90,
	GMStaff = 95,
	GMAdmin = 100,
	GMLeadAdmin = 150,
	QuestMaster = 160,
	GMAreas = 170,
	GMCoder = 180,
	GMMgmt = 200,
	GMImpossible = 250,
	Max = 255
};

enum ConsiderLevel : uint8 {
	Ally = 1,
	Warmly,
	Kindly,
	Amiably,
	Indifferently,
	Apprehensively,
	Dubiously,
	Threateningly,
	Scowls
};

enum TargetDescriptionType : uint8 {
	LCSelf,
	UCSelf,
	LCYou,
	UCYou,
	LCYour,
	UCYour
};

enum QuakeType : uint8_t
{
	QuakeDisabled = 0,
	QuakeNormal = 1,
	QuakePVP = 2,
	QuakeMax
};

std::string QuakeTypeToString(QuakeType in_type);

enum ReloadWorld : uint8 {
	NoRepop = 0,
	Repop,
	ForceRepop
};

namespace SpecialAbility {
	constexpr int Summon = 1;
	constexpr int Enrage = 2;
	constexpr int Rampage = 3;
	constexpr int AreaRampage = 4;
	constexpr int Flurry = 5;
	constexpr int TripleAttack = 6;
	constexpr int DualWield = 7;
	constexpr int DisallowEquip = 8;
	constexpr int BaneAttack = 9;
	constexpr int MagicalAttack = 10;
	constexpr int RangedAttack = 11;
	constexpr int SlowImmunity = 12;
	constexpr int MesmerizeImmunity = 13;
	constexpr int CharmImmunity = 14;
	constexpr int StunImmunity = 15;
	constexpr int SnareImmunity = 16;
	constexpr int FearImmunity = 17;
	constexpr int DispellImmunity = 18;
	constexpr int MeleeImmunity = 19;
	constexpr int MagicImmunity = 20;
	constexpr int FleeingImmunity = 21;
	constexpr int MeleeImmunityExceptBane = 22;
	constexpr int MeleeImmunityExceptMagical = 23;
	constexpr int AggroImmunity = 24;
	constexpr int BeingAggroImmunity = 25;
	constexpr int CastingFromRangeImmunity = 26;
	constexpr int FeignDeathImmunity = 27;
	constexpr int TauntImmunity = 28;
	constexpr int TunnelVision = 29;
	constexpr int NoBuffHealFriends = 30;
	constexpr int PacifyImmunity = 31;
	constexpr int Leash = 32;
	constexpr int Tether = 33;
	constexpr int PermarootFlee = 34;
	constexpr int HarmFromClientImmunity = 35;
	constexpr int AlwaysFlee = 36;
	constexpr int FleePercent = 37;
	constexpr int AllowBeneficial = 38;
	constexpr int DisableMelee = 39;
	constexpr int NPCChaseDistance = 40;
	constexpr int AllowedToTank = 41;
	constexpr int ProximityAggro = 42;
	constexpr int AlwaysCallHelp = 43;
	constexpr int UseWarriorSkills = 44;
	constexpr int AlwaysFleeLowCon = 45;
	constexpr int NoLoitering = 46;
	constexpr int BadFactionBlockHandin = 47;
	constexpr int PCDeathblowCorpse = 48;
	constexpr int CorpseCamper = 49;
	constexpr int ReverseSlow = 50;
	constexpr int HasteImmunity = 51;
	constexpr int DisarmImmunity = 52;
	constexpr int RiposteImmunity = 53;
	constexpr int ProximityAggro2 = 54;
	constexpr int Max = 55;

	constexpr int MaxParameters = 8;

	std::string GetName(int ability_id);
	bool IsValid(int ability_id);
}

static std::map<int, std::string> special_ability_names = {
	{ SpecialAbility::Summon,                     "Summon" },
	{ SpecialAbility::Enrage,                     "Enrage" },
	{ SpecialAbility::Rampage,                    "Rampage" },
	{ SpecialAbility::AreaRampage,                "Area Rampage" },
	{ SpecialAbility::Flurry,                     "Flurry" },
	{ SpecialAbility::TripleAttack,               "Triple Attack" },
	{ SpecialAbility::DualWield,                  "Dual Wield" },
	{ SpecialAbility::DisallowEquip,              "Disallow Equip" },
	{ SpecialAbility::BaneAttack,                 "Bane Attack" },
	{ SpecialAbility::MagicalAttack,              "Magical Attack" },
	{ SpecialAbility::RangedAttack,               "Ranged Attack" },
	{ SpecialAbility::SlowImmunity,               "Immune to Slow" },
	{ SpecialAbility::MesmerizeImmunity,          "Immune to Mesmerize" },
	{ SpecialAbility::CharmImmunity,              "Immune to Charm" },
	{ SpecialAbility::StunImmunity,               "Immune to Stun" },
	{ SpecialAbility::SnareImmunity,              "Immune to Snare" },
	{ SpecialAbility::FearImmunity,               "Immune to Fear" },
	{ SpecialAbility::DispellImmunity,            "Immune to Dispell" },
	{ SpecialAbility::MeleeImmunity,              "Immune to Melee" },
	{ SpecialAbility::MagicImmunity,              "Immune to Magic" },
	{ SpecialAbility::FleeingImmunity,            "Immune to Fleeing" },
	{ SpecialAbility::MeleeImmunityExceptBane,    "Immune to Melee except Bane" },
	{ SpecialAbility::MeleeImmunityExceptMagical, "Immune to Non-Magical Melee" },
	{ SpecialAbility::AggroImmunity,              "Immune to Aggro" },
	{ SpecialAbility::BeingAggroImmunity,         "Immune to Being Aggro" },
	{ SpecialAbility::CastingFromRangeImmunity,   "Immune to Ranged Spells" },
	{ SpecialAbility::FeignDeathImmunity,         "Immune to Feign Death" },
	{ SpecialAbility::TauntImmunity,              "Immune to Taunt" },
	{ SpecialAbility::TunnelVision,               "Tunnel Vision" },
	{ SpecialAbility::NoBuffHealFriends,          "Does Not Heal or Buff Allies" },
	{ SpecialAbility::PacifyImmunity,             "Immune to Pacify" },
	{ SpecialAbility::Leash,                      "Leashed" },
	{ SpecialAbility::Tether,                     "Tethered" },
	{ SpecialAbility::PermarootFlee,              "Permaroot Flee" },
	{ SpecialAbility::HarmFromClientImmunity,     "Immune to Harm from Client" },
	{ SpecialAbility::AlwaysFlee,                 "Always Flees" },
	{ SpecialAbility::FleePercent,                "Flee Percentage" },
	{ SpecialAbility::AllowBeneficial,            "Allows Beneficial Spells" },
	{ SpecialAbility::DisableMelee,               "Melee is Disabled" },
	{ SpecialAbility::NPCChaseDistance,           "Chase Distance" },
	{ SpecialAbility::AllowedToTank,              "Allowed to Tank" },
	{ SpecialAbility::ProximityAggro,             "Proximity Aggro" },
	{ SpecialAbility::AlwaysCallHelp,             "Always Call for Help" },
	{ SpecialAbility::UseWarriorSkills,           "Use Warrior Skills" },
	{ SpecialAbility::AlwaysFleeLowCon,           "Always Flee on Low Con" },
	{ SpecialAbility::NoLoitering,                "No Loitering" },
	{ SpecialAbility::BadFactionBlockHandin,      "Block Quest Handin on Bad Faction" },
	{ SpecialAbility::PCDeathblowCorpse,          "PC Deathblow to Corpse" },
	{ SpecialAbility::CorpseCamper,               "Corpse Camper" },
	{ SpecialAbility::ReverseSlow,                "Reverse Slow" },
	{ SpecialAbility::HasteImmunity,              "Immune to Haste" },
	{ SpecialAbility::DisarmImmunity,             "Immune to Disarm" },
	{ SpecialAbility::RiposteImmunity,            "Immune to Riposte" },
	{ SpecialAbility::ProximityAggro2,            "Proximity Aggro2" },
};

#endif /*COMMON_EMU_CONSTANTS_H*/