static void Run(lua_State *L, const char *script) {
    if (luaL_dostring(L, script)) {
        std::cerr << lua_tostring(L, -1) << '\n';
        std::abort();
    }
}
static Lua_NPC Create(lua_State *L) {
    return lua_create_npc(luabind::globals(L)["spec"], 949, 25, 3.4, 157);
}
int main() {
    auto L = luaL_newstate();
    luaL_openlibs(L);
    luabind::open(L);
    Run(L, "spec = false");
    assert(Create(L).npc == nullptr && entity_list.entries.empty());
    Run(L, "spec = {}");
    auto n = Create(L).npc;
    assert(n && n->owns); // Must transfer ownership before returning to Lua.
    // This is a read after return: ASan detects the original premature delete.
    assert(n->definition->cur_hp == 30 && n->definition->max_hp == 30);
    assert(n->definition->size == 6.0f && n->definition->race == 1);
    assert(n->definition->STR == 75 && n->definition->MR == 25);
    assert(n->definition->loot_lockout == 0 && !n->definition->skip_global_loot);
    assert(std::string(n->definition->name) == "_");
    entity_list.Clear();

    Run(L, "spec = {name='Mork,_Applied_Physicist',npc_id=7151302,size=7.5,"
           "race=10,level=65,cur_hp=10000,max_hp=10000,helmtexture=3,"
           "chesttexture=1,armtexture=1,bracertexture=1,handtexture=1,"
           "legtexture=1,feettexture=1,walkspeed=0.5,skip_global_loot=true}");
    n = Create(L).npc;
    assert(n->owns && n->definition->npc_id == 7151302);
    assert(n->definition->cur_hp == 10000 && n->definition->size == 7.5f);
    assert(n->definition->race == 10 && n->definition->level == 65);
    assert(n->definition->helmtexture == 3 && n->definition->chesttexture == 1);
    assert(n->definition->armtexture == 1 && n->definition->bracertexture == 1);
    assert(n->definition->handtexture == 1 && n->definition->legtexture == 1);
    assert(n->definition->feettexture == 1 && n->definition->walkspeed == 0.5f);
    assert(n->definition->skip_global_loot);
    entity_list.Clear();

    Run(L, "spec = {cur_hp={},race={},size=7.5,name={},lastname={},special_abilities={}}");
    n = Create(L).npc;
    assert(n->definition->cur_hp == 30 && n->definition->race == 1);
    assert(n->definition->size == 7.5f);
    assert(std::string(n->definition->name) == "_");
    assert(n->definition->lastname[0] == '\0' && n->definition->special_abilities[0] == '\0');
    entity_list.Clear();

    Run(L, "spec = {name=string.rep('n',64),lastname=string.rep('l',100),"
           "special_abilities=string.rep('s',512)}");
    n = Create(L).npc;
    assert(std::strlen(n->definition->name) == 63);
    assert(std::strlen(n->definition->lastname) == 63);
    assert(std::strlen(n->definition->special_abilities) == 511);
    entity_list.Clear();
    for (int i = 0; i < 100; ++i) {
        n = Create(L).npc;
        assert(n->owns && n->definition->name[63] == '\0');
        entity_list.Clear();
    }
    lua_close(L);
    std::cout << "PASS: Lua NPC defaults, failed conversions, appearance fields, string bounds and retained ownership (100 create/delete cycles).\n";
}
