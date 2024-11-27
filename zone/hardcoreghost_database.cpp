#include "../common/repositories/base/base_hardcoreghost_repository.h"
#include "../common/repositories/base/base_hardcore_ghost_inventory_repository.h"
#include "../common/emu_constants.h"

#include "hardcoreghost_database.h"
#include "hardcoreghost.h"

bool HardcoreGhostDatabase::LoadHardcoreGhost(const std::string& ghost_name, HardcoreGhost*& loaded_ghost)
{
    if (ghost_name.empty() || loaded_ghost) {
        return false;
    }

    // Find the ghost by name
    const auto& e = BaseHardcoreGhostDataRepository::GetWhere(database, "name = '" + ghost_name + "'");
    if (e.empty()) {
        return false;
    }

    // Assuming the highest level ghost is the first one in the result
    const auto& ghost_data = e.front();

    auto t = HardcoreGhost::FillNPCTypeStruct(
        ghost_data.name,
        ghost_data.last_name,
        ghost_data.level,
        ghost_data.race,
        ghost_data.class_,
        ghost_data.gender,
        ghost_data.size,
        ghost_data.face,
        ghost_data.hair_style,
        ghost_data.hair_color,
        ghost_data.eye_color_1,
        ghost_data.eye_color_2,
        ghost_data.beard,
        ghost_data.beard_color
    );

    loaded_ghost = new HardcoreGhost(t, ghost_data.ghost_id);

    // TODO later if needed
    // if (loaded_ghost) {
    //     loaded_ghost->SetSurname(ghost_data.last_name);
    //     loaded_ghost->SetTitle(ghost_data.title);
    //     loaded_ghost->SetSuffix(ghost_data.suffix);
    // }

    return true;
}

bool HardcoreGhostDatabase::LoadItems(const uint32 ghost_id, EQ::InventoryProfile& inventory_inst)
{
	if (!ghost_id) {
		return false;
	}

	const auto& l = BaseHardcoreGhostInventoryRepository::GetWhere(
		database,
		fmt::format(
			"`ghost_id` = {} ORDER BY `slotid`",
			ghost_id
		)
	);

	if (l.empty()) {
		return true;
	}

	for (const auto& e : l) {
		if (!EQ::ValueWithin(e.slotid, EQ::invslot::EQUIPMENT_BEGIN, EQ::invslot::EQUIPMENT_END)) {
			continue;
		}

        auto inst = database.CreateItem(database.GetItem(e.itemid));

		if (!inst) {
			LogError(
				"Warning: ghost_id [{}] has an invalid itemid [{}] in slotid [{}]",
				ghost_id,
				e.itemid,
				e.slotid
			);

			continue;
		}

		if (inventory_inst.PutItem(e.slotid, *inst) == INVALID_INDEX) {
			LogError(
				"Warning: Invalid slotid for item in inventory: ghost_id [{}] itemid [{}] slotid [{}]",
				ghost_id,
				e.itemid,
				e.slotid
			);
		}
        else
        {
            Log(Logs::General, Logs::Info, "HardcoreGhostDatabase::LoadItems() - Loaded item: %s for slot: %d", inst->GetItem()->Name, e.slotid);
        }

		safe_delete(inst);
	}

	return true;
}
