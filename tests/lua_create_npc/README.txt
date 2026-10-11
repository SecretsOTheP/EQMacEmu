Lua NPC creation regression test

Requirements: Python 3, C++20 compiler with AddressSanitizer, Lua 5.1 development
headers/library, and an existing Unix Makefiles server build containing
bin/libluabind.a and zone/CMakeFiles/zone.dir/flags.make.

From the server repository:
  bash tests/lua_create_npc/run.sh /absolute/path/to/server/build

The generator extracts the actual creation function/macros and strn0cpy from the
checkout. It uses real Lua/luabind conversions and the real NPCType structure.
Only the entity list, NPC ownership wrapper, and Lua NPC handle are test doubles;
this does not boot a zone or access a database. AddressSanitizer checks the
factory's retained definition after return and its cleanup across 100 cycles.

Covers omitted and invalid fields, size preservation, HP/race/stat defaults,
clothing and global-loot settings, bounded strings and invalid table arguments.
A complete server build and an in-game zone/reload test remain necessary.

API: eq.create_npc_from_table(definition, x, y, z, heading)
Returns an NPC handle, or an invalid handle for a non-table argument. Its presence
allows quests to require the repaired factory instead of accidentally calling an
older unsafe eq.create_npc. The existing eq.create_npc name uses the same repaired
implementation and now also returns a handle. Callers that ignore it still work.
NPC definitions remain in memory; creation does not insert content database rows.
Use encounter registrations for handlers when there is no npc_types row, since
normal named NPC-script discovery currently requires a database definition.
