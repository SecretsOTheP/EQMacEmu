#ifndef HARDCOREGHOST_DATABASE_H
#define HARDCOREGHOST_DATABASE_H

#include <cstdint>
#include "../common/inventory_profile.h"

class HardcoreGhost;

class HardcoreGhostDatabase {
public:
    // Loads a HardcoreGhost by its name
    // Returns true if the ghost was successfully loaded, false otherwise
    static bool LoadHardcoreGhost(const std::string& ghost_name, HardcoreGhost*& loaded_ghost);

    // Loads items for a HardcoreGhost by its ID
    // Returns true if the items were successfully loaded, false otherwise
    static bool LoadItems(const uint32 ghost_id, EQ::InventoryProfile& inventory_inst);
};

#endif // HARDCOREGHOST_DATABASE_H
