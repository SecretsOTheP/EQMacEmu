#ifndef COMMON_MAC_LIMITS_H
#define COMMON_MAC_LIMITS_H

#include "../types.h"
#include "../emu_versions.h"
#include "../skills.h"

namespace Mac 
{
	const int16 IINVALID = -1;
	const int16 INULL = 0;

	namespace inventory {
		inline EQ::versions::ClientVersion GetInventoryRef() { return EQ::versions::ClientVersion::Mac; }

		const bool ConcatenateInvTypeLimbo = true;

		const bool AllowOverLevelEquipment = false;

		const bool AllowEmptyBagInBag = false;
		const bool AllowClickCastFromBag = false;
	} /*inventory*/

	namespace invtype {
		inline EQ::versions::ClientVersion GetInvTypeRef() { return EQ::versions::ClientVersion::Mac; }

		namespace enum_ {
			enum InventoryType : int16 {
				typePossessions = INULL,
				typeBank,
				typeTrade,
				typeWorld,
				typeLimbo,
				typeMerchant,
				typeCorpse,
				typeBazaar,
				typeInspect,
			};
		} // namespace enum_
		using namespace enum_;

		const int16 POSSESSIONS_SIZE = 30;
		const int16 BANK_SIZE = 30;
		const int16 BANK_8_SIZE = 8;
		const int16 TRADE_SIZE = 8;
		const int16 WORLD_SIZE = 10;
		const int16 LIMBO_SIZE = 36;
		const int16 MERCHANT_SIZE = 80;
		const int16 CORPSE_SIZE = POSSESSIONS_SIZE;
		const int16 BAZAAR_SIZE = 80;
		const int16 INSPECT_SIZE = 21;

		const int16 TRADE_NPC_SIZE = 4; // defined by implication

		const int16 TYPE_INVALID = IINVALID;
		const int16 TYPE_BEGIN = typePossessions;
		const int16 TYPE_END = typeInspect;
		const int16 TYPE_COUNT = (TYPE_END - TYPE_BEGIN) + 1;

		int16 GetInvTypeSize(int16 inv_type);
		const char* GetInvTypeName(int16 inv_type);

		bool IsInvTypePersistent(int16 inv_type);

	} /*invtype*/
	
	namespace invslot {
		inline EQ::versions::ClientVersion GetInvSlotRef() { return EQ::versions::ClientVersion::Mac; }
			
		namespace enum_ {
			enum InventorySlots : int16 {
				slotCursor = INULL,
				slotEar1,
				slotHead,
				slotFace,
				slotEar2,
				slotNeck,
				slotShoulders,
				slotArms,
				slotBack,
				slotWrist1,
				slotWrist2,
				slotRange,
				slotHands,
				slotPrimary,
				slotSecondary,
				slotFinger1,
				slotFinger2,
				slotChest,
				slotLegs,
				slotFeet,
				slotWaist,
				slotAmmo,
				slotGeneral1,
				slotGeneral2,
				slotGeneral3,
				slotGeneral4,
				slotGeneral5,
				slotGeneral6,
				slotGeneral7,
				slotGeneral8
			};

			constexpr int16 format_as(InventorySlots slot) { return static_cast<int16>(slot); }
		} // namespace enum_
		using namespace enum_;

		const int16 SLOT_INVALID = IINVALID;
		const int16 SLOT_BEGIN = INULL;

		const int16 POSSESSIONS_BEGIN = slotCursor;
		const int16	POSSESSIONS_END = slotGeneral8;
		const int16 POSSESSIONS_COUNT = (POSSESSIONS_END - POSSESSIONS_BEGIN +1);

		const int16 EQUIPMENT_BEGIN = slotEar1;
		const int16 EQUIPMENT_END = slotAmmo;
		const int16 EQUIPMENT_COUNT = (EQUIPMENT_END - EQUIPMENT_BEGIN + 1);

		const int16 GENERAL_BEGIN = slotGeneral1;
		const int16 GENERAL_END = slotGeneral8;
		const int16 GENERAL_COUNT = (GENERAL_END - GENERAL_BEGIN + 1);

		const int16 BONUS_BEGIN = invslot::slotEar1;
		const int16 BONUS_STAT_END = invslot::slotWaist;
		const int16 BONUS_SKILL_END = invslot::slotAmmo;

		const int16 BANK_BEGIN = 2000;
		const int16 BANK_END = (BANK_BEGIN + invtype::BANK_SIZE) - 1;
		const int16 BANK_8_END = (BANK_BEGIN + invtype::BANK_8_SIZE) - 1;

		const int16 TRADE_BEGIN = 3000;
		const int16 TRADE_END = (TRADE_BEGIN + invtype::TRADE_SIZE) - 1;

		const int16 TRADE_NPC_END = (TRADE_BEGIN + invtype::TRADE_NPC_SIZE) - 1; // defined by implication

		const int16 WORLD_BEGIN = 4000;
		const int16 WORLD_END = (WORLD_BEGIN + invtype::WORLD_SIZE) - 1;

		const int16 CORPSE_BEGIN = invslot::slotGeneral1;
		const int16 CORPSE_END = invslot::slotGeneral1 + invslot::slotGeneral8;

		const uint64 POSSESSIONS_BITMASK = 0x000000027FDFFFFF; //
		const uint64 CORPSE_BITMASK = 0x017FFFFE7F800000; //

		const char* GetInvPossessionsSlotName(int16 inv_slot);
		const char* GetInvCorpseSlotName(int16 inv_slot);
		const char* GetInvSlotName(int16 inv_type, int16 inv_slot);

	}

	namespace invbag {
		inline EQ::versions::ClientVersion GetInvBagRef() { return EQ::versions::ClientVersion::Mac; }

		const int16 SLOT_INVALID = IINVALID;
		const int16 SLOT_BEGIN = INULL;
		const int16 SLOT_END = 9;
		const int16 SLOT_COUNT = 10;

		const int16 GENERAL_BAGS_BEGIN = 250;
		const int16 GENERAL_BAGS_COUNT = invslot::GENERAL_COUNT * SLOT_COUNT;
		const int16 GENERAL_BAGS_END = (GENERAL_BAGS_BEGIN + GENERAL_BAGS_COUNT) - 1;

		const int16 CURSOR_BAG_BEGIN = 330;
		const int16 CURSOR_BAG_COUNT = SLOT_COUNT;
		const int16 CURSOR_BAG_END = (CURSOR_BAG_BEGIN + CURSOR_BAG_COUNT) - 1;

		const int16 BANK_BAGS_BEGIN = 2030;
		const int16 BANK_BAGS_COUNT = (invtype::BANK_SIZE * SLOT_COUNT);
		const int16 BANK_BAGS_END = (BANK_BAGS_BEGIN + BANK_BAGS_COUNT) - 1;

		const int16 TRADE_BAGS_BEGIN = 3030;
		const int16 TRADE_BAGS_COUNT = invtype::TRADE_SIZE * SLOT_COUNT;
		const int16 TRADE_BAGS_END = (TRADE_BAGS_BEGIN + TRADE_BAGS_COUNT) - 1;

		const char* GetInvBagIndexName(int16 bag_index);

	} /*invbag*/

	namespace item {
		inline EQ::versions::ClientVersion GetItemRef() { return EQ::versions::ClientVersion::Mac; }

		enum ItemPacketType : int {
			ItemPacketMerchant = 100,
			ItemPacketTradeView = 101,
			ItemPacketLoot = 102,
			ItemPacketTrade = 103,
			ItemPacketCharInventory = 105,
			ItemPacketSummonItem = 106,
			ItemPacketWorldContainer = 107,
			ItemPacketStolenItem = 108
		};

	} /*item*/

	namespace profile {
		inline EQ::versions::ClientVersion GetProfileRef() { return EQ::versions::ClientVersion::Mac; }

		const int16 SKILL_ARRAY_SIZE = 100;

	} /*profile*/

	namespace constants {
		inline EQ::versions::ClientVersion GetConstantsRef() { return EQ::versions::ClientVersion::Mac; }

		const size_t CHARACTER_CREATION_LIMIT = 8; // Hard-coded in client - DO NOT ALTER
		const size_t ITEM_COMMON_SIZE = 5;
		const size_t SAY_LINK_BODY_SIZE = 7;

	} /*constants*/

	namespace behavior {
		inline EQ::versions::ClientVersion GetBehaviorRef() { return EQ::versions::ClientVersion::Mac; }

		const bool CoinHasWeight = true;

	} /*behavior*/

	namespace skills {
		inline EQ::versions::ClientVersion GetSkillsRef() { return EQ::versions::ClientVersion::Mac; }

		const size_t LastUsableSkill = EQ::skills::SkillTaunt;

	} /*skills*/

	namespace spells {
		inline EQ::versions::ClientVersion GetSkillsRef() { return EQ::versions::ClientVersion::Mac; }

		enum class CastingSlot : uint32 {
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

		const int SPELL_ID_MAX = 4000;
		const int SPELLBOOK_SIZE = 256;
		const int SPELL_GEM_COUNT = static_cast<uint32>(CastingSlot::MaxGems);

	} /*spells*/

};	//end namespace TEMPLATE

#endif /*COMMON_MAC_LIMITS_H_*/






