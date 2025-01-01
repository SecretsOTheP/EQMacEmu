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

// @merth notes:
// These classes could be optimized with database reads/writes by storing
// a status flag indicating how object needs to interact with database

#ifndef COMMON_ITEM_INSTANCE_H
#define COMMON_ITEM_INSTANCE_H

class ItemParse;			// Parses item packets

#include "../common/eq_constants.h"
#include "../common/item_data.h"
#include "../common/timer.h"
#include "../common/deity.h"
#include "../common/memory_buffer.h"

#include <list>
#include <map>


// Specifies usage type for item inside EQ::ItemInstance
enum ItemInstTypes
{
	ItemInstNormal = 0,
	ItemInstWorldContainer
};

typedef enum {
	byFlagIgnore,	//do not consider this flag
	byFlagSet,		//apply action if the flag is set
	byFlagNotSet	//apply action if the flag is NOT set
} byFlagSetting;


class SharedDatabase;

// ########################################
// Class: EQ::ItemInstance
//	Base class for an instance of an item
//	An item instance encapsulates item data + data specific
//	to an item instance (includes dye, augments, charges, etc)
namespace EQ
{
	class InventoryProfile;

	class ItemInstance
	{
	public:
		/////////////////////////
		// Methods
		/////////////////////////

		// Constructors/Destructor
		ItemInstance(const EQ::ItemData* item = nullptr, int8 charges = 0);

		ItemInstance(SharedDatabase* db, int16 item_id, int8 charges = 0);

		ItemInstance(ItemInstTypes use_type);

		ItemInstance(const ItemInstance& copy);

		~ItemInstance();

		// Query item type
		bool IsType(item::ItemClass item_class) const;

		bool IsClassCommon();
		bool IsClassBag();
		bool IsClassBook();

		bool IsClassCommon() const { return const_cast<ItemInstance*>(this)->IsClassCommon(); }
		bool IsClassBag() const { return const_cast<ItemInstance*>(this)->IsClassBag(); }
		bool IsClassBook() const { return const_cast<ItemInstance*>(this)->IsClassBook(); }

		// Can item be stacked?
		bool IsStackable() const;

		// Can item be equipped by/at?
		bool IsEquipable(uint16 race, uint16 class_) const;
		bool IsEquipable(int16 slot_id) const;


		inline bool IsExpendable() const { return ((m_item->Click.Type == item::ItemEffectExpendable) || (m_item->ItemType == item::ItemTypePotion) || (m_item->ItemType == item::ItemTypePoison)); }

		//
		// Contents
		//
		ItemInstance* GetItem(uint8 slot) const;
		int16 GetItemID(uint8 slot) const;
		inline const ItemInstance* operator[](uint8 slot) const { return GetItem(slot); }
		void PutItem(uint8 slot, const ItemInstance& inst);
		void PutItem(SharedDatabase* db, uint8 slot, int16 item_id) { return; } // not defined anywhere...
		void DeleteItem(uint8 slot);
		ItemInstance* PopItem(uint8 index);
		void Clear();
		void ClearByFlags(byFlagSetting is_nodrop, byFlagSetting is_norent);
		uint8 FirstOpenSlot() const;
		uint8 GetTotalItemCount() const;
		bool IsNoneEmptyContainer();
		std::map<uint8, ItemInstance*>* GetContents() { return &m_contents; }

		// Has attack/delay?
		bool IsWeapon() const;
		bool IsAmmo() const;

		// Accessors
		const uint16 GetID() const { return ((m_item) ? m_item->ID : 0); }
		const EQ::ItemData* GetItem() const;

		int8 GetCharges() const { return m_charges; }
		void SetCharges(int8 charges) { m_charges = charges; }

		uint32 GetPrice() const { return m_price; }
		void SetPrice(uint32 price) { m_price = price; }

		uint32 GetMerchantSlot() const { return m_merchantslot; }
		void SetMerchantSlot(uint32 slot) { m_merchantslot = slot; }

		int32 GetMerchantCount() const { return m_merchantcount; }
		void SetMerchantCount(int32 count) { m_merchantcount = count; }

		int16 GetCurrentSlot() const { return m_currentslot; }
		void SetCurrentSlot(int16 curr_slot) { m_currentslot = curr_slot; }

		bool IsOnCursorQueue() const { return m_cursorqueue; }
		void SetCursorQueue(bool val) { m_cursorqueue = val; }

		std::string GetCustomDataString() const;
		std::string GetCustomData(std::string identifier);
		void SetCustomData(std::string identifier, std::string value);
		void SetCustomData(std::string identifier, int value);
		void SetCustomData(std::string identifier, float value);
		void SetCustomData(std::string identifier, bool value);
		void DeleteCustomData(std::string identifier);

		// Allows treatment of this object as though it were a pointer to m_item
		operator bool() const { return (m_item != nullptr); }

		// Compare inner Item_Struct of two ItemInstance objects
		bool operator==(const ItemInstance& right) const { return (this->m_item == right.m_item); }
		bool operator!=(const ItemInstance& right) const { return (this->m_item != right.m_item); }

		// Clone current item
		ItemInstance* Clone() const;

		bool IsSlotAllowed(int16 slot_id, EQ::InventoryProfile &inv) const;

		void Initialize(SharedDatabase* db = nullptr);

		std::string Serialize(int16 slot_id) const { InternalSerializedItem_Struct s; s.slot_id = slot_id; s.inst = (const void*)this; std::string ser; ser.assign((char*)&s, sizeof(InternalSerializedItem_Struct)); return ser; }
		void Serialize(EQ::OutBuffer& ob, int16 slot_id) const { InternalSerializedItem_Struct isi; isi.slot_id = slot_id; isi.inst = (const void*)this; ob.write((const char*)&isi, sizeof(isi)); }

		inline int32 GetSerialNumber() const { return m_SerialNumber; }
		inline void SetSerialNumber(int32 id) { m_SerialNumber = id; }

		std::map<std::string, ::Timer>& GetTimers() { return m_timers; }
		void SetTimer(std::string name, uint32 time);
		void StopTimer(std::string name);
		void ClearTimers();
		bool IsFlesh() const;
		bool IsTieredPotion() const;

	protected:
		//////////////////////////
		// Protected Members
		//////////////////////////
		friend class InventoryProfile;;

		std::map<uint8, ItemInstance*>::const_iterator _cbegin() { return m_contents.cbegin(); }
		std::map<uint8, ItemInstance*>::const_iterator _cend() { return m_contents.cend(); }

		void _PutItem(uint8 index, ItemInstance* inst) { m_contents[index] = inst; }

		ItemInstTypes			m_use_type {ItemInstNormal};	// Usage type for item
		const EQ::ItemData*		m_item {nullptr};		// Ptr to item data
		int8					m_charges {0};	// # of charges for chargeable items
		uint32					m_price {0};	// Bazaar /trader price
		uint32					m_color{0};
		uint32					m_merchantslot {0};
		int16					m_currentslot {0};
		int32					m_merchantcount {1};		//number avaliable on the merchant, -1=unlimited
		int32					m_SerialNumber {0};	// Unique identifier for this instance of an item. Needed for apply posion
		bool					m_cursorqueue {true};

		//
		// Items inside of this item (augs or contents);
		std::map<uint8, ItemInstance*>			m_contents; // Zero-based index: min=0, max=9
		std::map<std::string, std::string>	m_custom_data;
		std::map<std::string, ::Timer>		m_timers;
	};
}

#endif /*COMMON_ITEM_H*/
