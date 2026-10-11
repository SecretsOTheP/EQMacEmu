// Standalone regression harness. The runner appends the production Lua factory.
// Real Lua/luabind conversions and NPCType layout; only zone entities are faked.
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "lua.hpp"
#include <luabind/luabind.hpp>
#include <glm/vec4.hpp>
#include "common/strings.h"
#include "zone/zonedump.h"

namespace Class { constexpr uint8 Warrior = 1; }
enum class GravityBehavior { Water };
struct NPC {
    const NPCType *definition;
    bool owns = false;
    NPC(const NPCType *type, void *, glm::vec4, GravityBehavior) : definition(type) {}
    void GiveNPCTypeData() { owns = true; }
    ~NPC() { if (owns) delete definition; }
};
struct Lua_NPC {
    NPC *npc = nullptr;
    Lua_NPC() = default;
    Lua_NPC(NPC *value) : npc(value) {}
};
struct EntityList {
    std::vector<NPC *> entries;
    void AddNPC(NPC *npc) { entries.push_back(npc); }
    void Clear() { for (auto npc : entries) delete npc; entries.clear(); }
} entity_list;
