#ifdef LUA_EQEMU
#include <sstream>

#include "lua.hpp"
#include <luabind/luabind.hpp>
#include <luabind/object.hpp>

#include "quest_parser_collection.h"
#include "quest_interface.h"

#include "masterentity.h"
#include "lua_item.h"
#include "lua_iteminst.h"
#include "lua_entity.h"
#include "lua_mob.h"
#include "lua_client.h"
#include "lua_npc.h"
#include "lua_spell.h"
#include "lua_corpse.h"
#include "lua_door.h"
#include "lua_object.h"
#include "lua_packet.h"
#include "lua_encounter.h"
#include "zone.h"
#include "lua_parser_events.h"

//NPC
void handle_npc_event_say(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	npc->DoQuestPause(init);

	Lua_Client l_client(reinterpret_cast<Client*>(init));
	luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
	l_client_o.push(L);
	lua_setfield(L, -2, "other");

	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "message");

	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "language");
}

void handle_npc_event_trade(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Client l_client(reinterpret_cast<Client*>(init));
	luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
	l_client_o.push(L);
	lua_setfield(L, -2, "other");
	
	lua_createtable(L, 0, 0);
	const auto npc_id = npc->GetNPCTypeID();
	
	if(extra_pointers) {
		size_t sz = extra_pointers->size();
		for(size_t i = 0; i < sz; ++i) {
			auto prefix = fmt::format("item{}", i + 1);
			auto* inst = std::any_cast<EQ::ItemInstance*>(extra_pointers->at(i));

			Lua_ItemInst l_inst = inst;
			luabind::adl::object l_inst_o = luabind::adl::object(L, l_inst);
			l_inst_o.push(L);

			lua_setfield(L, -2, prefix.c_str());
		}
	}

	auto money_string = fmt::format("platinum.{}", npc_id);
	uint32 money_value = !parse->GetVar(money_string).empty() ? Strings::ToUnsignedInt(parse->GetVar(money_string)) : 0;

	lua_pushinteger(L, money_value);
	lua_setfield(L, -2, "platinum");

	money_string = fmt::format("gold.{}", npc_id);
	money_value = !parse->GetVar(money_string).empty() ? Strings::ToUnsignedInt(parse->GetVar(money_string)) : 0;

	lua_pushinteger(L, money_value);
	lua_setfield(L, -2, "gold");

	money_string = fmt::format("silver.{}", npc_id);
	money_value = !parse->GetVar(money_string).empty() ? Strings::ToUnsignedInt(parse->GetVar(money_string)) : 0;

	lua_pushinteger(L, money_value);
	lua_setfield(L, -2, "silver");

	money_string = fmt::format("copper.{}", npc_id);
	money_value = !parse->GetVar(money_string).empty() ? Strings::ToUnsignedInt(parse->GetVar(money_string)) : 0;

	lua_pushinteger(L, money_value);
	lua_setfield(L, -2, "copper");

	if (init && init->IsClient())
	{
		lua_pushinteger(L, init->CastToClient()->IsSelfFound() != 1 && init->CastToClient()->IsSoloOnly() != 1 ? 1 : 0);
		lua_setfield(L, -2, "enable_multiquest");
	}
	// set a reference to the client inside of the trade object as well for plugins to process
	l_client_o.push(L);
	lua_setfield(L, -2, "other");

	lua_setfield(L, -2, "trade");
}

void handle_npc_event_hp(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	if(extra_data == 1) {
		lua_pushinteger(L, -1);
		lua_setfield(L, -2, "hp_event");
		lua_pushinteger(L, Strings::ToInt(data));
		lua_setfield(L, -2, "inc_hp_event");
	}
	else
	{
		lua_pushinteger(L, Strings::ToInt(data));
		lua_setfield(L, -2, "hp_event");
		lua_pushinteger(L, -1);
		lua_setfield(L, -2, "inc_hp_event");
	}
}

void handle_npc_single_mob(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Mob l_mob(init);
	luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
	l_mob_o.push(L);
	lua_setfield(L, -2, "other");
}

void handle_npc_single_client(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Client l_client(reinterpret_cast<Client*>(init));
	luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
	l_client_o.push(L);
	lua_setfield(L, -2, "other");
}

void handle_npc_single_npc(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Mob *init, 
	std::string data, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	Lua_NPC l_npc(reinterpret_cast<NPC*>(init));
	luabind::adl::object l_npc_o = luabind::adl::object(L, l_npc);
	l_npc_o.push(L);
	lua_setfield(L, -2, "other");
}

void handle_npc_waypoint(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Mob l_mob(init);
	luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
	l_mob_o.push(L);
	lua_setfield(L, -2, "other");
	Seperator sep(data.c_str());

	lua_pushinteger(L, Strings::ToInt(sep.arg[0]));
	lua_setfield(L, -2, "wp");

	lua_pushinteger(L, Strings::ToInt(sep.arg[1]));
	lua_setfield(L, -2, "gridid");
}

void handle_npc_hate(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Mob l_mob(init);
	luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
	l_mob_o.push(L);
	lua_setfield(L, -2, "other");

	lua_pushboolean(L, Strings::ToBool(data) == 0 ? false : true);
	lua_setfield(L, -2, "joined");
}


void handle_npc_signal(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, Strings::ToInt(data));
	lua_setfield(L, -2, "signal");

	if (extra_pointers) {
		std::string *str = std::any_cast<std::string*>(extra_pointers->at(0));
		lua_pushstring(L, str->c_str());
		lua_setfield(L, -2, "data");
	}
}

void handle_npc_timer(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "timer");
}

void handle_npc_death(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Mob l_mob(init);
	luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
	l_mob_o.push(L);
	lua_setfield(L, -2, "other");

	Seperator sep(data.c_str());

	Mob* killer = entity_list.GetMob(Strings::ToInt(sep.arg[0]));
	Lua_Mob l_mob2(killer);
	luabind::adl::object l_mob_o2 = luabind::adl::object(L, l_mob2);
	l_mob_o2.push(L);
	lua_setfield(L, -2, "killer");

	lua_pushinteger(L, Strings::ToInt(sep.arg[1]));
	lua_setfield(L, -2, "damage");

	int spell_id = Strings::ToUnsignedInt(sep.arg[2]);
	if(IsValidSpell(spell_id)) {
		Lua_Spell l_spell(&spells[spell_id]);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	} else {
		Lua_Spell l_spell(nullptr);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	}

	lua_pushinteger(L, Strings::ToInt(sep.arg[3]));
	lua_setfield(L, -2, "skill_id");
}

void handle_npc_cast(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	int spell_id = Strings::ToUnsignedInt(data);
	if(IsValidSpell(spell_id)) {
		Lua_Spell l_spell(&spells[spell_id]);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	} else {
		Lua_Spell l_spell(nullptr);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	}
}

void handle_npc_area(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(0)));
	lua_setfield(L, -2, "area_id");

	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(1)));
	lua_setfield(L, -2, "area_type");
}

void handle_npc_death_zone(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Mob *init, 
	std::string data, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	Seperator sep(data.c_str());
	lua_pushinteger(L, std::stoi(sep.arg[0]));
	lua_setfield(L, -2, "killer_id");

	lua_pushinteger(L, std::stoi(sep.arg[1]));
	lua_setfield(L, -2, "killer_damage");

	lua_pushinteger(L, std::stoi(sep.arg[2]));
	lua_setfield(L, -2, "killer_spell");

	lua_pushinteger(L, std::stoi(sep.arg[3]));
	lua_setfield(L, -2, "killer_skill");

	lua_pushinteger(L, std::stoi(sep.arg[4]));
	lua_setfield(L, -2, "killed_npc_id");
}

void handle_npc_spawn_zone(QuestInterface *parse, lua_State* L, NPC* npc, Mob *init, std::string data, uint32 extra_data,
	std::vector<std::any> *extra_pointers) {

	Seperator sep(data.c_str());
	lua_pushinteger(L, std::stoi(sep.arg[0]));
	lua_setfield(L, -2, "spawned_entity_id");

	lua_pushinteger(L, std::stoi(sep.arg[1]));
	lua_setfield(L, -2, "spawned_npc_id");
}

void handle_npc_null(
	QuestInterface* parse,
	lua_State* L,
	NPC* npc,
	Mob* init,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
}

//Player
void handle_player_say(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "message");

	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "language");
}

void handle_player_environmental_damage(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Seperator sep(data.c_str());
	lua_pushinteger(L, Strings::ToInt(sep.arg[0]));
	lua_setfield(L, -2, "env_damage");

	lua_pushinteger(L, Strings::ToInt(sep.arg[1]));
	lua_setfield(L, -2, "env_damage_type");

	lua_pushinteger(L, Strings::ToInt(sep.arg[2]));
	lua_setfield(L, -2, "env_final_damage");
}

void handle_player_death(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Seperator sep(data.c_str());

	Mob *o = entity_list.GetMobID(std::stoi(sep.arg[0]));
	Lua_Mob l_mob(o);
	luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
	l_mob_o.push(L);
	lua_setfield(L, -2, "other");

	lua_pushinteger(L, Strings::ToInt(sep.arg[1]));
	lua_setfield(L, -2, "damage");

	int spell_id = Strings::ToUnsignedInt(sep.arg[2]);
	if(IsValidSpell(spell_id)) {
		Lua_Spell l_spell(&spells[spell_id]);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	} else {
		Lua_Spell l_spell(nullptr);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	}

	lua_pushinteger(L, Strings::ToInt(sep.arg[3]));
	lua_setfield(L, -2, "skill");
}

void handle_player_timer(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "timer");
}

void handle_player_discover_item(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	const EQ::ItemData *item = database.GetItem(extra_data);
	if(item) {
		Lua_Item l_item(item);
		luabind::adl::object l_item_o = luabind::adl::object(L, l_item);
		l_item_o.push(L);
		lua_setfield(L, -2, "item");
	} else {
		Lua_Item l_item(nullptr);
		luabind::adl::object l_item_o = luabind::adl::object(L, l_item);
		l_item_o.push(L);
		lua_setfield(L, -2, "item");
	}
}

void handle_player_fish_forage_success(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_ItemInst l_item(std::any_cast<EQ::ItemInstance*>(extra_pointers->at(0)));
	luabind::adl::object l_item_o = luabind::adl::object(L, l_item);
	l_item_o.push(L);
	lua_setfield(L, -2, "item");
}

void handle_player_click_object(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Object l_object(std::any_cast<Object*>(extra_pointers->at(0)));
	luabind::adl::object l_object_o = luabind::adl::object(L, l_object);
	l_object_o.push(L);
	lua_setfield(L, -2, "object");
}

void handle_player_click_door(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Door l_door(std::any_cast<Doors*>(extra_pointers->at(0)));
	luabind::adl::object l_door_o = luabind::adl::object(L, l_door);
	l_door_o.push(L);
	lua_setfield(L, -2, "door");
}

void handle_player_signal(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, Strings::ToInt(data));
	lua_setfield(L, -2, "signal");
}

void handle_player_pick_up(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_ItemInst l_item(std::any_cast<EQ::ItemInstance*>(extra_pointers->at(0)));
	luabind::adl::object l_item_o = luabind::adl::object(L, l_item);
	l_item_o.push(L);
	lua_setfield(L, -2, "item");
}

void handle_player_cast(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	int spell_id = Strings::ToInt(data);
	if(IsValidSpell(spell_id)) {
		Lua_Spell l_spell(&spells[spell_id]);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
	} else {
		Lua_Spell l_spell(nullptr);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
	}

	lua_setfield(L, -2, "spell");
}

void handle_player_zone(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Seperator sep(data.c_str());

	lua_pushinteger(L, Strings::ToInt(sep.arg[0]));
	lua_setfield(L, -2, "from_zone_id");

	lua_pushinteger(L, Strings::ToInt(sep.arg[1]));
	lua_setfield(L, -2, "zone_id");
}

void handle_player_duel_win(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Client l_client(std::any_cast<Client*>(extra_pointers->at(1)));
	luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
	l_client_o.push(L);
	lua_setfield(L, -2, "other");
}

void handle_player_duel_loss(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Client l_client(std::any_cast<Client*>(extra_pointers->at(0)));
	luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
	l_client_o.push(L);
	lua_setfield(L, -2, "other");
}

void handle_player_loot(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_ItemInst l_item(std::any_cast<EQ::ItemInstance*>(extra_pointers->at(0)));
	luabind::adl::object l_item_o = luabind::adl::object(L, l_item);
	l_item_o.push(L);
	lua_setfield(L, -2, "item");

	Lua_Corpse l_corpse(std::any_cast<Corpse*>(extra_pointers->at(1)));
	luabind::adl::object l_corpse_o = luabind::adl::object(L, l_corpse);
	l_corpse_o.push(L);
	lua_setfield(L, -2, "corpse");
}

void handle_player_command(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Seperator sep(data.c_str(), ' ', 10, 100, true);
	std::string command(sep.arg[0] + 1);
	lua_pushstring(L, command.c_str());
	lua_setfield(L, -2, "command");

	luabind::adl::object args = luabind::newtable(L);
	int max_args = sep.GetMaxArgNum();
	for(int i = 1; i < max_args; ++i) {
		if(strlen(sep.arg[i]) > 0) {
			args[i] = std::string(sep.arg[i]);
		}
	}

	args.push(L);
	lua_setfield(L, -2, "args");
}

void handle_player_combine(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "recipe_id");

	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "recipe_name");	
}

void handle_player_feign(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_NPC l_npc(std::any_cast<NPC*>(extra_pointers->at(0)));
	luabind::adl::object l_npc_o = luabind::adl::object(L, l_npc);
	l_npc_o.push(L);
	lua_setfield(L, -2, "other");
}

void handle_player_area(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(0)));
	lua_setfield(L, -2, "area_id");

	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(1)));
	lua_setfield(L, -2, "area_type");
}

void handle_player_respawn(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, Strings::ToInt(data));
	lua_setfield(L, -2, "option");

	lua_pushboolean(L, extra_data == 1 ? true : false);
	lua_setfield(L, -2, "resurrect");
}

void handle_player_packet(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Packet l_packet(std::any_cast<EQApplicationPacket*>(extra_pointers->at(0)));
	luabind::adl::object l_packet_o = luabind::adl::object(L, l_packet);
	l_packet_o.push(L);
	lua_setfield(L, -2, "packet");

	lua_pushboolean(L, Strings::ToBool(std::to_string(extra_data)));
	lua_setfield(L, -2, "connecting");
}

void handle_player_null(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
}

void handle_player_warp(
	QuestInterface *parse, 
	lua_State *L, 
	Client *client, 
	std::string data, 
	uint32 extra_data, 
	std::vector<std::any> *extra_pointers
) {
	Seperator sep(data.c_str());
	lua_pushnumber(L, std::stof(sep.arg[0]));
	lua_setfield(L, -2, "from_x");

	lua_pushnumber(L, std::stof(sep.arg[1]));
	lua_setfield(L, -2, "from_y");

	lua_pushnumber(L, std::stof(sep.arg[2]));
	lua_setfield(L, -2, "from_z");
}

//Item
void handle_item_click(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	EQ::ItemInstance* item,
	Mob* mob,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "slot_id");
}

void handle_item_timer(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	EQ::ItemInstance* item,
	Mob* mob,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "timer");
}

void handle_item_proc(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	EQ::ItemInstance* item,
	Mob* mob,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	Lua_Mob l_mob(mob);
	luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
	l_mob_o.push(L);
	lua_setfield(L, -2, "target");

	if(IsValidSpell(extra_data)) {
		Lua_Spell l_spell(&spells[extra_data]);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	} else {
		Lua_Spell l_spell(nullptr);
		luabind::adl::object l_spell_o = luabind::adl::object(L, l_spell);
		l_spell_o.push(L);
		lua_setfield(L, -2, "spell");
	}
}

void handle_item_loot(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	EQ::ItemInstance* item,
	Mob* mob,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	if(mob && mob->IsCorpse()) {
		Lua_Corpse l_corpse(mob->CastToCorpse());
		luabind::adl::object l_corpse_o = luabind::adl::object(L, l_corpse);
		l_corpse_o.push(L);
		lua_setfield(L, -2, "corpse");
	} else {
		Lua_Corpse l_corpse(nullptr);
		luabind::adl::object l_corpse_o = luabind::adl::object(L, l_corpse);
		l_corpse_o.push(L);
		lua_setfield(L, -2, "corpse");
	}
}

void handle_item_equip(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	EQ::ItemInstance* item,
	Mob* mob,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "slot_id");
}

void handle_item_null(
	QuestInterface* parse,
	lua_State* L,
	Client* client,
	EQ::ItemInstance* item,
	Mob* mob,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
}

//Spell
void handle_spell_effect(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Client* client, 
	uint32 spell_id, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	if(npc) {
		Lua_Mob l_npc(npc);
		luabind::adl::object l_npc_o = luabind::adl::object(L, l_npc);
		l_npc_o.push(L);
	} else if(client) {
		Lua_Mob l_client(client);
		luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
		l_client_o.push(L);
	} else {
		Lua_Mob l_mob(nullptr);
		luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
		l_mob_o.push(L);
	}

	lua_setfield(L, -2, "target");

	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(0)));
	lua_setfield(L, -2, "buff_slot");

	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "caster_id");
}

void handle_spell_tic(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Client* client, 
	uint32 spell_id, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	if(npc) {
		Lua_Mob l_npc(npc);
		luabind::adl::object l_npc_o = luabind::adl::object(L, l_npc);
		l_npc_o.push(L);
	} else if(client) {
		Lua_Mob l_client(client);
		luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
		l_client_o.push(L);
	} else {
		Lua_Mob l_mob(nullptr);
		luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
		l_mob_o.push(L);
	}

	lua_setfield(L, -2, "target");

	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(0)));
	lua_setfield(L, -2, "tics_remaining");

	lua_pushinteger(L, *std::any_cast<uint8*>(extra_pointers->at(1)));
	lua_setfield(L, -2, "caster_level");

	lua_pushinteger(L, *std::any_cast<int*>(extra_pointers->at(2)));
	lua_setfield(L, -2, "buff_slot");

	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "caster_id");
}

void handle_spell_fade(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Client* client, 
	uint32 spell_id, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	if(npc) {
		Lua_Mob l_npc(npc);
		luabind::adl::object l_npc_o = luabind::adl::object(L, l_npc);
		l_npc_o.push(L);
	} else if(client) {
		Lua_Mob l_client(client);
		luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
		l_client_o.push(L);
	} else {
		Lua_Mob l_mob(nullptr);
		luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
		l_mob_o.push(L);
	}

	lua_setfield(L, -2, "target");

	lua_pushinteger(L, extra_data);
	lua_setfield(L, -2, "buff_slot");

	lua_pushinteger(L, *std::any_cast<uint16*>(extra_pointers->at(0)));
	lua_setfield(L, -2, "caster_id");
}

void handle_translocate_finish(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Client* client, 
	uint32 spell_id, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	if(npc) {
		Lua_Mob l_npc(npc);
		luabind::adl::object l_npc_o = luabind::adl::object(L, l_npc);
		l_npc_o.push(L);
	} else if(client) {
		Lua_Mob l_client(client);
		luabind::adl::object l_client_o = luabind::adl::object(L, l_client);
		l_client_o.push(L);
	} else {
		Lua_Mob l_mob(nullptr);
		luabind::adl::object l_mob_o = luabind::adl::object(L, l_mob);
		l_mob_o.push(L);
	}

	lua_setfield(L, -2, "target");
}

void handle_spell_null(
	QuestInterface *parse, 
	lua_State* L, 
	NPC* npc, 
	Client* client, 
	uint32 spell_id, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
}

void handle_board_boat(
	QuestInterface *parse, 
	lua_State* L, 
	Client* client, 
	std::string data, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	lua_pushinteger(L, Strings::ToInt(data));
	lua_setfield(L, -2, "boat_id");
}

void handle_leave_boat(
	QuestInterface *parse, 
	lua_State* L, 
	Client* client, 
	std::string data, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	lua_pushinteger(L, Strings::ToInt(data));
	lua_setfield(L, -2, "boat_id");
}

void handle_encounter_timer(
	QuestInterface* parse,
	lua_State* L,
	Encounter* encounter,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	lua_pushstring(L, data.c_str());
	lua_setfield(L, -2, "timer");
}

void handle_click_merchant(
	QuestInterface *parse, 
	lua_State* L, 
	Client* client, 
	std::string data, 
	uint32 extra_data,
	std::vector<std::any> *extra_pointers
) {
	lua_pushinteger(L, Strings::ToUnsignedInt(data));
	lua_setfield(L, -2, "merchant_type_id");
}

void handle_encounter_load(
	QuestInterface* parse,
	lua_State* L,
	Encounter* encounter,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	if (encounter) {
		Lua_Encounter l_enc(encounter);
		luabind::adl::object l_enc_o = luabind::adl::object(L, l_enc);
		l_enc_o.push(L);
		lua_setfield(L, -2, "encounter");
	}
	if (extra_pointers) {
		std::string *str = std::any_cast<std::string*>(extra_pointers->at(0));
		lua_pushstring(L, str->c_str());
		lua_setfield(L, -2, "data");
	}
}

void handle_encounter_unload(
	QuestInterface* parse,
	lua_State* L,
	Encounter* encounter,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
	if (extra_pointers) {
		std::string *str = std::any_cast<std::string*>(extra_pointers->at(0));
		lua_pushstring(L, str->c_str());
		lua_setfield(L, -2, "data");
	}
}

void handle_encounter_null(
	QuestInterface* parse,
	lua_State* L,
	Encounter* encounter,
	std::string data,
	uint32 extra_data,
	std::vector<std::any>* extra_pointers
) {
}

#endif
