/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemulator.org)

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

/*

	To add a new command 3 things must be done:

	1.	At the bottom of command.h you must add a prototype for it.
	2.	Add the function in this file.
	3.	In the command_init function you must add a call to command_add
		for your function.

	Notes: If you want an alias for your command, add an entry to the
	`command_settings` table in your database. The access level you
	set with command_add is the default setting if the command isn't
	listed in the `command_settings` db table.

*/

#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <thread>
#include <fmt/format.h>
#include "../common/repositories/command_subsettings_repository.h"

#ifdef _WINDOWS
#define strcasecmp _stricmp
#endif

#include "../common/global_define.h"
#include "../common/eq_packet.h"
#include "../common/features.h"
#include "../common/guilds.h"
#include "../common/patches/patches.h"
#include "../common/ptimer.h"
#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/say_link.h"
#include "../common/eqemu_logsys.h"
#include "../common/file.h"
#include "../common/fastmath.h"

#include "data_bucket.h"
#include "command.h"
#include "guild_mgr.h"
#include "map.h"
#include "mob_movement_manager.h"
#include "qglobals.h"
#include "quest_parser_collection.h"
#include <stdlib.h>
#include "string_ids.h"
#include "titles.h"
#include "water_map.h"
#include "worldserver.h"
#include "queryserv.h"
#include "zonedb.h"
#include "../common/events/player_event_logs.h"

extern QueryServ* QServ;
extern WorldServer worldserver;
extern FastMath g_Math;
void CatchSignal(int sig_num);


int command_count; // how many commands we have

// this is the pointer to the dispatch function, updated once
// init has been performed to point at the real function
int (*command_dispatch)(Client *, std::string, bool) = command_notavail;

std::map<std::string, CommandRecord *> commandlist;
std::map<std::string, std::string> commandaliases;
std::vector<CommandRecord *> command_delete_list;
std::map<std::string, uint8> commands_map;
std::vector<CommandSubsettingsRepository::CommandSubsettings> command_subsettings;

/*
* command_notavail
* This is the default dispatch function when commands aren't loaded.
*
* Parameters:
*	not used
*
*/
int command_notavail(Client *c, std::string message, bool ignore_status)
{
	c->Message(Chat::White, "Commands not available.");
	return -1;
}

/*****************************************************************************/
/* the rest below here could be in a dynamically loaded module eventually */
/*****************************************************************************/

/*

Access Levels:

0		Normal
10	* Steward *
20	* Apprentice Guide *
50	* Novice Guide *
80	* Guide *
81	* Senior Guide *
85	* GM-Tester *
90	* EQ Support *
95	* GM-Staff *
100	* GM-Admin *
150	* GM-Lead Admin *
160	* QuestMaster *
170	* GM-Areas *
180	* GM-Coder *
200	* GM-Mgmt *
250	* GM-Impossible *

*/

/*
 * command_init
 * initializes the command list, call at startup
 *
 * Parameters:
 *	none
 *
 * When adding a new command, only hard-code 'real' commands -
 * all command aliases are added later through a database call
 *
 */

int command_init(void)
{
	if (!commandaliases.empty()) {
		command_deinit();
	}

	if (
		command_add("advnpcspawn", "[maketype|makegroup|addgroupentry|addgroupspawn][removegroupspawn|movespawn|editgroupbox|cleargroupbox].", AccountStatus::GMImpossible, command_advnpcspawn) ||
		command_add("aggrozone", "[aggro] [0/1: Enforce ignore distance. If 0 or not set, all will come] - Aggro every mob in the zone with X aggro. Default is 0. Not recommend if you're not invulnerable.", AccountStatus::GMImpossible, command_aggrozone) ||
		command_add("ai", "[factionid/spellslist/con/guard/roambox/stop/start] - Modify AI on NPC target.", AccountStatus::GMImpossible, command_ai) || 
		command_add("allowexport", "[off, worn, inventory, bank] - Authorize export of this character to be included in nightly, open sourced database dumps.", AccountStatus::Player, command_allowexport) ||
		command_add("altactivate", "[argument] - activates alternate advancement abilities, use altactivate help for more information.", AccountStatus::GMAreas, command_altactivate) ||
		command_add("appearance", "[type] [value] - Send an appearance packet for you or your target.", AccountStatus::GMImpossible, command_appearance) ||
		command_add("apply_shared_memory", "[shared_memory_name] - Tells every zone and world to apply a specific shared memory segment by name.", AccountStatus::GMImpossible, command_apply_shared_memory) ||
		command_add("attack", "[targetname] - Make your NPC target attack targetname.", AccountStatus::QuestMaster, command_attack) ||
		command_add("attackentity", "[entityid] - Make your NPC target attack target entity.", AccountStatus::QuestMaster, command_attackentity) ||

		command_add("ban", "[name][reason] - Ban by character name.", AccountStatus::GMAdmin, command_ban) ||
		command_add("beard", "Change the beard of your target.", AccountStatus::GMImpossible, command_beard) ||
		command_add("beardcolor", "Change the beard color of your target.", AccountStatus::GMImpossible, command_beardcolor) ||
		command_add("bestz", "Ask map for a good Z coord for your x,y coords.", AccountStatus::ApprenticeGuide, command_bestz) ||
		command_add("betabuff", "[level] - Buffs user's player to provided level, giving level * 100 platinum, along with providing a (potentially non era-specific) set of reagents, spells and skills.", AccountStatus::GMImpossible, command_betabuff) ||
		command_add("boatinfo", "Gets infomation about the boats currently spawned in the zone.", AccountStatus::SeniorGuide, command_boatinfo) ||
		command_add("bug", "Bug report system. Encase your bug in quotes. Type: #bug <quote>I have a bug</quote>.", AccountStatus::EQSupport, command_bug) ||

		command_add("castspell", "[spellid] [gm_override] [entityid] - Cast a spell. GM override bypasses resist and stacking checks. If entityid is specified, that NPC will cast a spell on the target mob.", AccountStatus::QuestMaster, command_castspell) ||
		command_add("charactertransfer", "[accountname] [underscoretospaces] - Transfers current character to an account you own. If your account name contains a space, use 1 for underscore to spaces.", AccountStatus::Player, command_charactertransfer) ||
		command_add("chat", "[channel num] [message] - Send a channel message to all zones.", AccountStatus::EQSupport, command_chat) ||
		command_add("chattest", "[color] [loops] - Sends a test message with the specified color to yourself.", AccountStatus::GMCoder, command_chattest) ||
		command_add("cleartimers", "[timer] Clears one or all persistant timers on target.", AccountStatus::GMMgmt, command_cleartimers) ||
		command_add("clearsaylink", "Clear the saylink table.", AccountStatus::GMStaff, command_clearsaylink) ||
		command_add("connectworldserver", "Make zone attempt to connect to worldserver.", AccountStatus::GMTester, command_connectworldserver) ||
#ifndef WIN32
		command_add("coredump", "Dumps a core log of any existing cores to view on web page.", AccountStatus::GMCoder, command_coredump) ||
#endif
		command_add("corpse", "Manipulate corpses, use with no arguments for help.", AccountStatus::EQSupport, command_corpse) ||
		command_add("crashtest", "Crash the zoneserver.", AccountStatus::GMImpossible, command_crashtest) ||

		command_add("damage", "[amount] - Damage your target.", AccountStatus::QuestMaster, command_damage) ||
		command_add("damagetotals", "Displays a list of what has damaged your NPC target.", AccountStatus::GMAdmin, command_damagetotals) ||
		command_add("dbspawn2", "[spawngroup] [respawn] [variance] - Spawn an NPC from a predefined row in the spawn2 table.", AccountStatus::QuestMaster, command_dbspawn2) ||
		command_add("delacct", "[accountname] - Delete an account.", AccountStatus::GMImpossible, command_delacct) ||
		command_add("deletegraveyard", "[zone name] - Deletes the graveyard for the specified zone.", AccountStatus::GMImpossible, command_deletegraveyard) ||
		command_add("depop", "Depop your NPC target.", AccountStatus::GMLeadAdmin, command_depop) ||
		command_add("depopzone", "Depop the zone.", AccountStatus::GMAreas, command_depopzone) ||
		command_add("devtools", "Manages devtools", AccountStatus::QuestTroupe, command_devtools) ||
		command_add("disablerecipe", "[recipe_id] - Disables a recipe using the recipe id.", AccountStatus::GMImpossible, command_disablerecipe) ||
		command_add("d1", "[type] [spell] [damage] - Send an OP_Action packet with the specified values.", AccountStatus::GMImpossible, command_d1) ||
		command_add("doanim", "[animnum] [type] - Send an EmoteAnim for you or your target.", AccountStatus::GMStaff, command_doanim) ||

		command_add("emote", "['name'/'world'/'zone'] [type] [message] - Send an emote message.", AccountStatus::GMStaff, command_emote) ||
		command_add("enablerecipe", "[recipe_id] - Enables a recipe using the recipe id.", AccountStatus::GMImpossible, command_enablerecipe) ||
		command_add("equipitem", "[slotid(0-21)] - Equip the item on your cursor into the specified slot.", AccountStatus::GMLeadAdmin, command_equipitem) ||
		command_add("expansion", "[accountname][expansion] - Sets the expansion value for the specified account.", AccountStatus::GMLeadAdmin, command_expansion) ||

		command_add("face", "Change the face of your target.", AccountStatus::GMLeadAdmin, command_face) || 
		command_add("falltest", "[+Z] sends you to your current loc plus the Z specified.", AccountStatus::GMImpossible, command_falltest) ||
		command_add("fillbuffs", "Casts 15 buffs on the target for testing.", AccountStatus::QuestTroupe, command_fillbuffs) ||
		command_add("find", "Search command used to find various things", AccountStatus::Guide, command_find) ||
		command_add("fish", "Fish for an item", AccountStatus::QuestTroupe, command_fish) ||
		command_add("fixmob", "[race|gender|texture|helm|face|hair|haircolor|beard|beardcolor|heritage|tattoo|detail] [next|prev] - Manipulate appearance of your target.", AccountStatus::GMImpossible, command_fixmob) ||
		command_add("flagedit", "Edit zone flags on your target.", AccountStatus::GMMgmt, command_flagedit) ||
		command_add("fleeinfo", "Gives info about whether a NPC will flee or not, using the command issuer as top hate.", AccountStatus::QuestTroupe, command_fleeinfo) ||
		command_add("forage", "Forage an item", AccountStatus::QuestTroupe, command_forage) ||

		command_add("giveitem", "[itemid] [charges] - Summon an item onto your target's cursor. Charges are optional.", AccountStatus::GMLeadAdmin, command_giveitem) ||
		command_add("givemoney", "[pp] [gp] [sp] [cp] - Gives specified amount of money to the target player.", AccountStatus::GMLeadAdmin, command_givemoney) ||
		command_add("giveplayerfaction", "[factionid] [factionvalue] - Gives the target player faction with the given faction. (Acts as a hit).", AccountStatus::GMMgmt, command_giveplayerfaction) ||
		command_add("gmdamage", "[amount] [skipaggro] - Damage your target. Skips most combat checks, including invul.", AccountStatus::QuestMaster, command_gmdamage) ||
		command_add("goto", "[x] [y] [z] - Teleport to the provided coordinates or to your target.", AccountStatus::ApprenticeGuide, command_goto) ||
		command_add("grid", "[add/delete] [grid_num] [wandertype] [pausetype] - Create/delete a wandering grid.", AccountStatus::GMImpossible, command_grid) ||
		command_add("gridrecord", "[start|stop|addwp] - Record a grid using start, stop and use AddWp to add a waypoint to the grid in question.", 170, command_gridrecord) ||
		command_add("guild", "Guild manipulation commands. Use argument help for more info.", AccountStatus::EQSupport, command_guild) ||
		command_add("guildapprove", "[guildapproveid] - Approve a guild with specified ID (guild creator receives the id).", AccountStatus::EQSupport, command_guildapprove) ||
		command_add("guildcreate", "[guildname] - Creates an approval setup for guild name specified.", AccountStatus::EQSupport, command_guildcreate) ||
		command_add("guildlist", "[guildapproveid] - Lists character names who have approved the guild specified by the approve id.", AccountStatus::EQSupport, command_guildlist) ||

		command_add("hair", "Change the hair style of your target.", AccountStatus::GMImpossible, command_hair) ||
		command_add("haircolor", "Change the hair color of your target.", AccountStatus::GMImpossible, command_haircolor) ||
		command_add("helm", "Change the helm of your target.", AccountStatus::GMImpossible, command_helm) ||
		command_add("help", "[search term] - List available commands and their description, specify partial command as argument to search.", AccountStatus::Player, command_help) ||
		command_add("hotfix", "[hotfix_name] - Reloads shared memory into a hotfix, equiv to load_shared_memory followed by apply_shared_memory", AccountStatus::GMImpossible, command_hotfix) ||

		command_add("interrogateinv", "use [help] argument for available options.", AccountStatus::GMLeadAdmin, command_interrogateinv) ||
		command_add("interrogatelegacy", "Interrogates legacy items of your current target", AccountStatus::GMAdmin, command_interrogatelegacy) ||		
		command_add("interrupt", "[message id] [color] - Interrupt your casting. Arguments are optional.", AccountStatus::EQSupport, command_interrupt) ||
		command_add("ipban", "[IP address] - Ban IP by character name.", AccountStatus::GMMgmt, command_ipban) ||
		command_add("ipexemption", "[accountname] [exemption] - Set IP exemption amount for accountname by amount. Accounts default to 1.", AccountStatus::GMAdmin, command_ipexemption) ||
	
		command_add("iteminfo", "Get information about the item on your cursor.", AccountStatus::Guide, command_iteminfo) ||

		command_add("keyring", "Displays target's keyring items.", AccountStatus::EQSupport, command_keyring) ||
		command_add("kick", "[charname] - Disconnect charname.", AccountStatus::EQSupport, command_kick) ||
		command_add("kill", "Kill your target.", AccountStatus::GMLeadAdmin, command_kill) ||

		command_add("leaderboard", "[SFHC|SSFHC|SFHCOnly|HC] - List hardcore leaderboard.", AccountStatus::Player, command_leaderboard) ||
		command_add("listnpcs", "[name/range] - Search NPCs.", AccountStatus::EQSupport, command_listnpcs) ||
		command_add("list", "[npc] [name|all] - Search entities", AccountStatus::SeniorGuide, command_list) ||
		command_add("load_shared_memory", "[shared_memory_name] - Reloads shared memory and uses the input as output", AccountStatus::GMImpossible, command_load_shared_memory) ||
		command_add("loc", "Print out your or your target's current location and heading.", AccountStatus::Player, command_loc) ||
		command_add("logs", "Manage anything to do with logs.", AccountStatus::GMCoder, command_logs) ||
		command_add("logtest", "Performs log performance testing.", AccountStatus::GMImpossible, command_logtest) ||
		command_add("lootsim", "[npc_type_id] [loottable_id] [iterations] - Runs benchmark simulations using real loot logic to report numbers and data", AccountStatus::GMImpossible, command_lootsim) ||

		command_add("makemule", "Flags the account of the player who runs the command as a mule.", AccountStatus::Player, command_makemule) ||
		command_add("makepet", "[level] [class] [race] [texture] - Make a pet.", AccountStatus::QuestMaster, command_makepet) ||
		command_add("manaburn", "Use AA Wizard class skill manaburn on target.", AccountStatus::GMAreas, command_manaburn) ||
		command_add("manastat", "Report your or your target's cur/max mana.", AccountStatus::Guide, command_manastat) ||
		command_add("memspell", "[slotid] [spellid] - Memorize spellid in the specified slot.", AccountStatus::GMAreas, command_memspell) ||
		command_add("merchant_close_shop", "Closes a merchant shop.", AccountStatus::GMImpossible, command_merchantcloseshop) ||
		command_add("merchant_open_shop", "Opens a merchants shop.", AccountStatus::GMImpossible, command_merchantopenshop) ||
		command_add("modifynpcstat", "[Stat] [Value] - Modifies an NPC's stats temporarily.", AccountStatus::GMLeadAdmin, command_modifynpcstat) ||
		command_add("movechar", "[charname] [zonename] - Move charname to zonename.", AccountStatus::GMStaff, command_movechar) ||
		command_add("mule", "[account name] [0/1] - Toggles the mule status of the specified account ", AccountStatus::GMImpossible, command_mule) ||
		command_add("mysql", "Mysql CLI, see 'help' for options.", AccountStatus::GMImpossible, command_mysql) ||
		command_add("mysqltest", "Akkadius MySQL Bench Test.", AccountStatus::GMImpossible, command_mysqltest) ||
		command_add("mystats", "Show details about you or your pet.", AccountStatus::Guide, command_mystats) ||

		command_add("ngpermaclass", "<class> [deity] [city] [stats...] [force] - Change your or your target's class (disconnects client). Optionally change deity, home city, and stats.", AccountStatus::GMMgmt, command_ngperma_class) ||
		command_add("ngpermarace", "<race> [deity] [city] [stats...] [force] - Change your or your target's race (zone to take effect). Optionally change deity, home city, and stats.", AccountStatus::GMMgmt, command_ngperma_race) ||
		command_add("ngpermaraceclass", "<race> <class> [deity] [city] [stats...] [force] - Change your or your target's race/class (disconnects client). Optionally change deity, city, and stats.", AccountStatus::GMMgmt, command_ngperma_race_class) ||
		command_add("ngpermastats", "<stats...> - Change your or your target's starting stat point allocation (zone to take effect).", AccountStatus::GMMgmt, command_ngperma_stats) ||
		command_add("npccast", "[targetname/entityid] [spellid] - Causes NPC target to cast spellid on targetname/entityid.", AccountStatus::QuestMaster, command_npccast) ||
		command_add("npcedit", "[column] [value] - Mega NPC editing command.", AccountStatus::GMImpossible, command_npcedit) ||
		command_add("npcemote", "[message] - Make your NPC target emote a message.", AccountStatus::QuestMaster, command_npcemote) ||
		command_add("npcloot", "[show/money/add/remove] [itemid/all/money: pp gp sp cp] - Manipulate the loot an NPC is carrying.", AccountStatus::QuestMaster, command_npcloot) ||
		command_add("npcsay", "[message] - Make your NPC target say a message.", AccountStatus::QuestMaster, command_npcsay) ||
		command_add("npcshout", "[message] - Make your NPC target shout a message.", AccountStatus::QuestMaster, command_npcshout) ||
		command_add("npcspawn", "[create/add/update/remove/delete] - Manipulate spawn DB.", AccountStatus::GMImpossible, command_npcspawn) ||
		command_add("npctypecache", "[id] or all - Clears the npc type cache for either the id or all npcs.", AccountStatus::GMImpossible, command_npctypecache) ||
		command_add("npctypespawn", "[npctypeid] [factionid] - Spawn an NPC from the db.", AccountStatus::QuestMaster, command_npctypespawn) ||
		command_add("nukebuffs", "Strip all buffs on you or your target.", AccountStatus::GMLeadAdmin, command_nukebuffs) ||
		command_add("nukeitem", "[itemid] - Remove itemid from your player target's inventory.", AccountStatus::GMLeadAdmin, command_nukeitem) ||
		command_add("numauths", "TODO: describe this command.", AccountStatus::Max, command_numauths) ||

		command_add("opcode", "Reloads all opcodes from server patch files", AccountStatus::GMMgmt, command_reload) ||
		command_add("optest", "solar's private test command.", AccountStatus::GMCoder, command_optest) ||

		command_add("path", "view and edit pathing.", AccountStatus::GMImpossible, command_path) ||
		command_add("petition", "Handles everything petition related. Use with no args or with 'help' for how to use.", AccountStatus::ApprenticeGuide, command_petition) ||
		command_add("pf", "Display additional mob coordinate and wandering data.", AccountStatus::GMStaff, command_pf) ||
		command_add("playsound", "[number] - Plays a sound in the client.  Valid range 0-3999", AccountStatus::ApprenticeGuide, command_playsound) ||
		command_add("profanity", "Manage censored language.", AccountStatus::GMLeadAdmin, command_profanity) ||
		command_add("push", "[pushback] [pushup] - Pushes the target the specified amount.", AccountStatus::GMImpossible, command_push) ||

		command_add("qtest", "QueryServ testing command.", AccountStatus::GMTester, command_qtest) ||
		command_add("quaketrigger", "- [type_num (1 = Normal, 2 = PVP)] Triggers an earthquake manually", AccountStatus::GMImpossible, command_quaketrigger) ||

		command_add("raidloot", "LEADER|GROUPLEADER|SELECTED|ALL - Sets your raid loot settings if you have permission to do so.", 1, command_raidloot) ||
		command_add("randtest", "Perform a sampling of random number generation", AccountStatus::GMImpossible, command_randtest) ||
		command_add("randomfeatures", "Temporarily randomizes the Facial Features of your target.", AccountStatus::GMCoder, command_randomfeatures) ||
		command_add("refreshgroup", "Refreshes Group.", AccountStatus::EQSupport, command_refreshgroup) ||
		command_add("reload", "Reloads different types of server data globally, use no argument for help menu.", AccountStatus::GMMgmt, command_reload) || 
		command_add("rq", "Reloads quests (alias of #reload quests).", AccountStatus::GMMgmt, command_reload) ||
		command_add("rl", "Reloads logs (alias of #reload logs).", AccountStatus::GMMgmt, command_reload) ||
		command_add("repop", "[Force] - Repop the zone with optional force repop.", AccountStatus::GMLeadAdmin, command_repop) ||
		command_add("reloadspmod", "Reload spell modifiers from database", AccountStatus::GMLeadAdmin, command_reloadspmod) ||
		command_add("removelegacyitem", "Remove a legacy item from your target [itemid], or specify a [charid] [itemid] to remove the flag of.", AccountStatus::GMAdmin, command_removelegacyitem) ||
		command_add("repopclose", "[distance in units] Repops only NPC's nearby for fast development purposes", AccountStatus::GMAdmin, command_repopclose) ||
		command_add("resetaa", "Resets a Player's AA in their profile and refunds spent AA's to unspent, disconnects player.", AccountStatus::GMImpossible, command_resetaa) ||
		command_add("resetboat", "Sets player's boat to 0 in their profile.", AccountStatus::GMStaff, command_resetboat) ||
		command_add("revoke", "[charname] [1/0] - Makes charname unable to talk on OOC.", AccountStatus::GMStaff, command_revoke) ||
		command_add("rewind", "Rewind to the previous location", AccountStatus::Player, command_rewind) ||
		command_add("rules", "(subcommand) - Manage server rules.", AccountStatus::GMImpossible, command_rules) ||

		command_add("save", "Force your player or player corpse target to be saved to the database.", AccountStatus::GMLeadAdmin, command_save) ||
		command_add("scribespell", "[spellid] - Scribe specified spell in your target's spell book.", AccountStatus::GMAreas, command_scribespell) ||
		command_add("scribespells", "[max level] [min level] - Scribe all spells for you or your player target that are usable by them, up to level specified. (may freeze client for a few seconds).", AccountStatus::GMAreas, command_scribespells) ||
		command_add("sendop", "[opcode] - LE's Private test command, leave it alone.", AccountStatus::GMCoder, command_sendop) ||
		command_add("sendzonespawns", "Refresh spawn list for all clients in zone.", AccountStatus::GMAdmin, command_sendzonespawns) ||
		command_add("serversidename", "Prints target's server side name.", AccountStatus::GMAdmin, command_serversidename) ||
		command_add("setgraveyard", "[zone name] - Creates a graveyard for the specified zone based on your target's LOC.", AccountStatus::GMImpossible, command_setgraveyard) ||
		command_add("setgreed", "[greed] - Sets a merchant greed value.", AccountStatus::GMAdmin, command_setgreed) ||
		command_add("setnpcexpansion", "[min_expansion] [max_expansion] - Restrict an NPC's spawn2 (spawn location) by min, max expansion. These are float values.", AccountStatus::GMAdmin, command_setnpcexpansion) ||
		command_add("showbonusstats", "[item|spell|all] Shows bonus stats for target from items or spells. Shows both by default.", AccountStatus::Guide, command_showbonusstats) ||
		command_add("set", "Set command used to set various things", AccountStatus::Guide, command_set) || 
		command_add("show", "Show command used to show various things", AccountStatus::Guide, command_show) ||
		command_add("showfilters", "list client serverfilter settings.", AccountStatus::GMCoder, command_showfilters) ||
		command_add("showhelm", "on/off [all] Toggles displaying of player helms (including your own.) Specifying 'all' toggles every character currently on your account", AccountStatus::Player, command_showhelm) ||
		command_add("showpetspell", "[spellid/searchstring] - search pet summoning spells.", AccountStatus::Guide, command_showpetspell) ||
		command_add("showregen", "Shows information about your target's regen.", AccountStatus::GMAdmin, command_showregen) ||
		command_add("showtraderitems", "Displays the list of items a trader has up for sale.", AccountStatus::QuestTroupe, command_showtraderitems) ||
		command_add("shutdown", "Shut this zone process down.", AccountStatus::GMImpossible, command_shutdown) ||
		command_add("size", "[size] - Change size of you or your target.", AccountStatus::GMAdmin, command_size) ||
		command_add("skills", "List skill difficulty.", AccountStatus::GMAdmin, command_skilldifficulty) ||
		command_add("spawn", "[name] [race] [level] [material] [hp] [gender] [class] [priweapon] [secweapon] [merchantid] - Spawn an NPC.", AccountStatus::GMImpossible, command_spawn) ||
		command_add("spawnfix", "Find targeted NPC in database based on its X/Y/heading and update the database to make it spawn at your current location/heading.", AccountStatus::GMImpossible, command_spawnfix) ||
		command_add("spellinfo", "[spellid] - Get detailed info about a spell.", AccountStatus::Guide, command_spellinfo) ||
		command_add("starve", "Sets hunger and thirst to 0.", AccountStatus::GMCoder, command_starve) ||
		command_add("stun", "[duration] - Stuns you or your target for duration.", AccountStatus::QuestMaster, command_stun) ||
		command_add("summon", "[charname] - Summons your player/npc/corpse target, or charname if specified.", AccountStatus::EQSupport, command_summon) ||
		command_add("summonitem", "[itemid] [charges] - Summon an item onto your cursor. Charges are optional.", AccountStatus::QuestMaster, command_summonitem) ||
		command_add("suspend", "[name][days][reason] - Suspend by character name and for specificed number of days.", AccountStatus::EQSupport, command_suspend) ||
		command_add("synctod", "Send a time of day update to every client in zone.", AccountStatus::GMAdmin, command_synctod) ||
		command_add("showlootlockouts", "Shows your currently active loot lockouts. These do not apply to earthquake creatures.", AccountStatus::Player, command_showlootlockouts) ||
		command_add("showquake", "Shows current earthquake timer. Requires you to be a guild officer or leader.", AccountStatus::Player, command_showquake) ||
		command_add("takemoney", "[pp] [gp] [sp] [cp] [reason] - Takes specified amount of money from the target player.", AccountStatus::GMLeadAdmin, command_takemoney) ||
		command_add("testcommand", "Template for temporary commands as needed. Don't delete.", AccountStatus::GMImpossible, command_testcommand) ||
		command_add("testspawn", "[memloc] [value] - spawns a NPC for you only, with the specified values set in the spawn struct.", AccountStatus::GMCoder, command_testspawn) ||
		command_add("togglepvp", "Toggles PVP for a client.", AccountStatus::Player, command_togglepvp) ||
		command_add("undeletechar", "Undelete a character that was previously deleted.", AccountStatus::Max, command_undeletechar) ||
		command_add("underworld", "[z] - Reports NPCs that are below the given Z or if not given, below the lowest spawn2/grid coord. If red, the NPC is below the underworld coord.", AccountStatus::QuestTroupe, command_underworld) ||
		command_add("unmemspell", "[spellid] - Unmem specified spell from your target's spell bar.", AccountStatus::GMAreas, command_unmemspell) ||
		command_add("unmemspells", "Clear out your or your player target's spell gems.", AccountStatus::GMAreas, command_unmemspells) ||
		command_add("unscribespell", "[spellid] - Unscribe specified spell from your target's spell book.", AccountStatus::GMAreas, command_unscribespell) ||
		command_add("unscribespells", "Clear out your or your player target's spell book.", AccountStatus::GMAreas, command_unscribespells) ||

		command_add("viewplayerfaction", "[factionid] - Shows current personal and modified faction with the given ID.", AccountStatus::GMAdmin, command_viewplayerfaction) ||
	
		command_add("wc", "[wear slot] [material] - Sends an OP_WearChange for your target.", AccountStatus::GMImpossible, command_wc) ||
		command_add("worldshutdown", "Shut down world and all zones.", AccountStatus::GMImpossible, command_worldshutdown) ||
		command_add("wp", "[add/delete] [grid_num] [pause] [wp_num] [-h] - Add/delete a waypoint to/from a wandering grid.", AccountStatus::GMImpossible, command_wp) ||
		command_add("wpadd", "[pause] [-h] - Add your current location as a waypoint to your NPC target's AI path.", AccountStatus::GMImpossible, command_wpadd) ||

		command_add("xpinfo", "Show XP info about your current target.", AccountStatus::GMStaff, command_xpinfo) ||

		command_add("zone", "[Zone ID|Zone Short Name] [X] [Y] [Z] - Teleport to specified Zone by ID or Short Name (coordinates are optional).", AccountStatus::QuestTroupe, command_zone) ||
		command_add("zoneguild", "[Zone ID|Zone Short Name] [GuildID] [X] [Y] [Z] - Teleport to specified Zone by ID or Short Name (coordinates are optional).", AccountStatus::QuestTroupe, command_zoneguild) ||
		command_add("zonebootup", "(shortname) (ZoneServerID) - Make a zone server boot a specific zone. If no arguments are given, it will find and boot any crashed zones.", AccountStatus::GMImpossible, command_zonebootup) ||
		command_add("zoneshutdown", "[shortname] - Shut down a zone server.", AccountStatus::GMImpossible, command_zoneshutdown) ||
		command_add("zonespawn", "Not implemented.", AccountStatus::Max, command_zonespawn) ||
		command_add("zopp", "Troubleshooting command - Sends a fake item packet to you. No server reference is created.", AccountStatus::GMCoder, command_zopp) ||
		command_add("zsave", "Saves zheader to the database.", AccountStatus::GMImpossible, command_zsave) ||
		command_add("zuwcoords", "[z coord] - Set underworld coord.", AccountStatus::GMImpossible, command_zuwcoords)
			) {
		command_deinit();
		return -1;
	}

	std::map<std::string, std::pair<uint8, std::vector<std::string>>> command_settings;
	database.GetCommandSettings(command_settings);
	database.GetCommandSubSettings(command_subsettings);

	std::vector<std::pair<std::string, uint8>> injected_command_settings;
	std::vector<std::string> orphaned_command_settings;


	// inject static aliases
	for (auto &cs : command_settings) {
		for (const auto& e : command_subsettings) {
			if (cs.first == e.parent_command) {
				for (const auto& alias : Strings::Split(e.top_level_aliases, "|")) {
					cs.second.second.emplace_back(alias);
				}
			}
		}
	}
	for (const auto &cs : command_settings) {
		auto cl = commandlist.find(cs.first);
		if (cl == commandlist.end()) {
			orphaned_command_settings.push_back(cs.first);
			LogInfo(
				"Command [{}] no longer exists. Deleting orphaned entry from `command_settings` table.",
				cs.first
			);
		}
	}

	if (!orphaned_command_settings.empty()) {
		if (!database.UpdateOrphanedCommandSettings(orphaned_command_settings)) {
			LogInfo("Failed to process 'Orphaned Commands' update operation.");
		}
	}

	auto working_cl = commandlist;
	for (const auto &w : working_cl) {
		auto cs = command_settings.find(w.first);
		if (cs == command_settings.end()) {
			injected_command_settings.emplace_back(w.first, w.second->admin);
			LogInfo(
				"New Command [{}] found. Adding to `command_settings` table with admin status [{}]",
				w.first,
				w.second->admin
			);

			if (w.second->admin == AccountStatus::Player) {
				LogCommands(
					"Warning: Command [{}] defaulting to admin level 0!",
					w.first
				);
			}

			continue;
		}

		w.second->admin = cs->second.first;
		LogCommands(
			"Command [{}] set to admin level [{}]",
			w.first,
			cs->second.first
		);

		if (cs->second.second.empty()) {
			continue;
		}

		for (const auto &a : cs->second.second) {
			if (a.empty()) {
				continue;
			}

			if (commandlist.find(a) != commandlist.end()) {
				LogCommands(
					"command_init(): Warning: Alias [{}] already exists as a command - skipping!",
					a
				);

				continue;
			}

			commandlist[a] = w.second;
			commandaliases[a] = w.first;

			LogCommands(
				"command_init(): - Alias [{}] added to command [{}]",
				a,
				commandaliases[a]
			);
		}
	}

	if (!injected_command_settings.empty()) {
		if (!database.UpdateInjectedCommandSettings(injected_command_settings)) {
			LogInfo("Failed to process 'Injected Commands' update operation.");
		}
	}

	command_dispatch = command_realdispatch;

	return command_count;
}

	/*
	* command_deinit
	* clears the command list, freeing resources
	*
	* Parameters:
	*	none
	*
	*/
void command_deinit(void)
{
	for (auto &c : command_delete_list) {
		delete c;
	}

	command_delete_list.clear();
	commandlist.clear();
	commandaliases.clear();

	command_dispatch = command_notavail;
	command_count = 0;
}

	/*
	* command_add
	* adds a command to the command list; used by command_init
	*
	* Parameters:
	*	command_name	- the command ex: "spawn"
	*	desc		- text description of command for #help
	*	access		- default access level required to use command
	*	function		- pointer to function that handles command
	*
	*/
int command_add(std::string command_name, std::string description, uint8 admin, CmdFuncPtr function)
{
	if (command_name.empty()) {
		LogError("command_add() - Command added with empty name string - check command.cpp");
		return -1;
	}

	if (!function) {
		LogError("command_add() - Command [{}] added without a valid function pointer - check command.cpp", command_name);
		return -1;
	}

	if (commandlist.count(command_name)) {
		LogError("command_add() - Command [{}] is a duplicate command name - check command.cpp", command_name);
		return -1;
	}

	auto c = new CommandRecord;
	c->admin = admin;
	c->description = description;
	c->function = function;

	commands_map[command_name] = admin;

	commandlist[command_name] = c;
	commandaliases[command_name] = command_name;
	command_delete_list.push_back(c);
	command_count++;

	return 0;
}

uint8 GetCommandStatus(std::string command_name)
{
	return commands_map[command_name];
}

	/*
	*
	* command_realdispatch
	* Calls the correct function to process the client's command string.
	* Called from Client::ChannelMessageReceived if message starts with
	* command character (#).
	*
	* Parameters:
	*	c			- pointer to the calling client object
	*	message		- what the client typed
	*
	*/
int command_realdispatch(Client *c, std::string message, bool ignore_status)
{
	Seperator sep(message.c_str(), ' ', 10, 100, true); // "three word argument" should be considered 1 arg

	std::string cstr(sep.arg[0] + 1);

	if (commandlist.count(cstr) != 1) {
		return -2;
	}

	auto cur = commandlist[cstr];

	bool is_subcommand            = false;
	bool can_use_subcommand       = false;
	bool found_subcommand_setting = false;

	const auto arguments = sep.argnum;

	if (arguments >= 2) {
		const std::string& sub_command = sep.arg[1];

		for (const auto& e : command_subsettings) {
			if (e.sub_command == sub_command) {
				can_use_subcommand = c->Admin() >= static_cast<int16>(e.access_level);
				is_subcommand = true;
				found_subcommand_setting = true;
				break;
			}
		}

		if (!found_subcommand_setting) {
			for (const auto& e : command_subsettings) {
				if (e.sub_command == sub_command) {
					can_use_subcommand = c->Admin() >= static_cast<int16>(e.access_level);
					is_subcommand = true;
					break;
				}
			}
		}
	}

	if (!ignore_status) {
		if (!is_subcommand && c->Admin() < cur->admin) {
		c->Message(Chat::White, "Your status is not high enough to use this command.");
		return -1;
	}

		if (is_subcommand && !can_use_subcommand) {
			c->Message(Chat::White, "Your status is not high enough to use this subcommand.");
			return -1;
		}
	}

	if (cur->admin >= COMMANDS_LOGGING_MIN_STATUS) {
		QServ->QSLogCommands(c, sep.arg[0], (char *)sep.argplus[1]);
		LogCommands(
			"[{}] ([{}]) used command: [{}] (target=[{}])",
			c->GetName(),
			c->AccountName(),
			message,
			c->GetTarget() ? c->GetTarget()->GetName() : "NONE"
		);
	}

	if (!cur->function) {
		LogError("Command [{}] has a null function", cstr);
		return -1;
	}

	if (player_event_logs.IsEventEnabled(PlayerEvent::GM_COMMAND) && message != "#help") {
		auto e = PlayerEvent::GMCommandEvent{
			.message = message,
			.target = c->GetTarget() ? c->GetTarget()->GetName() : "NONE"
		};

		RecordPlayerEventLogWithClient(c, PlayerEvent::GM_COMMAND, e);
	}

	cur->function(c, &sep);	// Dispatch C++ Command

	return 0;
}

void command_help(Client *c, const Seperator *sep)
{
	int found_count = 0;
	std::string command_link;
	std::string search_criteria = Strings::ToLower(sep->argplus[1]);

	for (const auto &cur : commandlist) {
		if (!search_criteria.empty()) {
			if (
				!Strings::Contains(cur.first, search_criteria) &&
				!Strings::Contains(cur.second->description, search_criteria)
				) {
				continue;
			}
		}

		if (c->Admin() < cur.second->admin) {
			continue;
}

		command_link = Saylink::Silent(
			fmt::format(
				"{}{}",
				COMMAND_CHAR,
				cur.first
			)
		);

		c->Message(
			Chat::White,
			fmt::format(
				"{} | {}",
				command_link,
				cur.second->description
			).c_str()
		);

		found_count++;
}

	if (parse->PlayerHasQuestSub(EVENT_COMMAND)) {
		auto event_parse = parse->EventPlayer(EVENT_COMMAND, c, sep->msg, 0);
		if (event_parse >= 1) {
			found_count += event_parse;
	}
}

	c->Message(
		Chat::White,
		fmt::format(
			"{} Command{} listed{}.",
			found_count,
			found_count != 1 ? "s" : "",
			(
				!search_criteria.empty() ?
				fmt::format(
					" matching '{}'",
					search_criteria
				) :
				""
				)
		).c_str()
	);
}

void command_coredump(Client *c, const Seperator *sep)
{
#ifdef _WINDOWS
	// TODO: Add same functionality for windows from the following command.
	// Maybe have a batch file spit logs to a web folder?
	c->Message(Chat::White, "Not yet implemented for windows.");
#else
	int system_var = system("./dump");
	c->Message(Chat::White, "core dumped.");
#endif
}

void command_hotfix(Client *c, const Seperator *sep)
{
	auto items_count = database.GetItemsCount();
	auto shared_items_count = database.GetSharedItemsCount();
	if (items_count != shared_items_count) {
		c->Message(Chat::Yellow, "Your database does not have the same item count as your shared memory.");

		c->Message(
			Chat::Yellow,
			fmt::format(
				"Database Count: {} Shared Memory Count: {}",
				items_count,
				shared_items_count
			).c_str()
		);

		c->Message(Chat::Yellow, "If you want to be able to add new items to your server while it is online, you need to create placeholder entries in the database ahead of time and do not add or remove rows/entries. Only modify the existing placeholder rows/entries to safely use #hotfix.");

		return;
	}

	auto spells_count = database.GetSpellsCount();
	auto shared_spells_count = database.GetSharedSpellsCount();
	if (spells_count != shared_spells_count) {
		c->Message(Chat::Yellow, "Your database does not have the same spell count as your shared memory.");

		c->Message(
			Chat::Yellow,
			fmt::format(
				"Database Count: {} Shared Memory Count: {}",
				spells_count,
				shared_spells_count
			).c_str()
		);

		c->Message(Chat::Yellow, "If you want to be able to add new spells to your server while it is online, you need to create placeholder entries in the database ahead of time and do not add or remove rows/entries. Only modify the existing placeholder rows/entries to safely use #hotfix.");

		c->Message(Chat::Yellow, "Note: You may still have to distribute a spell file, even with dynamic changes.");

		return;
	}

	std::string hotfix;
	database.GetVariable("hotfix_name", hotfix);

	std::string hotfix_name;
	if(!strcasecmp(hotfix.c_str(), "hotfix_")) {
		hotfix_name = "";
	}
	else
	{
		hotfix_name = "hotfix_";
	}

	c->Message(Chat::White, "Creating and applying hotfix");
	std::thread t1([c, hotfix_name]() {
		int sysRet = -1;
#ifdef WIN32
		if (hotfix_name.length() > 0)
		{
			sysRet = system(StringFormat("shared_memory -hotfix=%s", hotfix_name.c_str()).c_str());
		}
		else
		{
			sysRet = system(StringFormat("shared_memory").c_str());
		}
#else
		if (hotfix_name.length() > 0)
		{
			sysRet = system(StringFormat("./shared_memory -hotfix=%s", hotfix_name.c_str()).c_str());
		}
		else
		{
			sysRet = system(StringFormat("./shared_memory").c_str());
		}
#endif
		if (sysRet == -1)
		{
			c->Message(Chat::White, "Hotfix failed.");
			return;
		}
		database.SetVariable("hotfix_name", hotfix_name);

		ServerPacket pack(ServerOP_ChangeSharedMem, hotfix_name.length() + 1);
		if (hotfix_name.length() > 0)
		{
			strcpy((char*)pack.pBuffer, hotfix_name.c_str());
		}
		worldserver.SendPacket(&pack);

		c->Message(Chat::White, "Hotfix applied");
	});

	t1.detach();
}

void command_load_shared_memory(Client *c, const Seperator *sep)
{
	std::string hotfix;
	database.GetVariable("hotfix_name", hotfix);

	std::string hotfix_name;
	if(strcasecmp(hotfix.c_str(), sep->arg[1]) == 0) {
		c->Message(Chat::White, "Cannot attempt to load this shared memory segment as it is already loaded.");
		return;
	}

	hotfix_name = sep->arg[1];
	c->Message(Chat::White, "Loading shared memory segment %s", hotfix_name.c_str());
	std::thread t1([c, hotfix_name]()
	{
		int sysRet = -1;
#ifdef WIN32
		if(hotfix_name.length() > 0)
		{
			sysRet = system(StringFormat("shared_memory -hotfix=%s", hotfix_name.c_str()).c_str());
		}
		else
		{
			sysRet = system(StringFormat("shared_memory").c_str());
		}
#else
		if(hotfix_name.length() > 0)
		{
			sysRet = system(StringFormat("./shared_memory -hotfix=%s", hotfix_name.c_str()).c_str());
		}
		else
		{
			sysRet = system(StringFormat("./shared_memory").c_str());
		}
#endif
		if (sysRet == -1)
		{
			c->Message(Chat::White, "Shared memory segment failed loading.");
			return;
		}
		c->Message(Chat::White, "Shared memory segment finished loading.");
	});

	t1.detach();
}

void command_apply_shared_memory(Client *c, const Seperator *sep) {
	std::string hotfix;
	database.GetVariable("hotfix_name", hotfix);
	std::string hotfix_name = sep->arg[1];

	c->Message(Chat::White, "Applying shared memory segment %s", hotfix_name.c_str());
	database.SetVariable("hotfix_name", hotfix_name);

	ServerPacket pack(ServerOP_ChangeSharedMem, hotfix_name.length() + 1);
	if(hotfix_name.length() > 0) {
		strcpy((char*)pack.pBuffer, hotfix_name.c_str());
	}
	worldserver.SendPacket(&pack);
}

void command_devtools(Client *c, const Seperator *sep)
{
	std::string dev_tools_key = StringFormat("%i-dev-tools-window-disabled", c->AccountID());

	/**
	 * Handle window toggle
	 */
	if (strcasecmp(sep->arg[1], "disable") == 0) {
		DataBucket::SetData(dev_tools_key, "true");
		c->SetDevToolsEnabled(false);
	}
	if (strcasecmp(sep->arg[1], "enable") == 0) {
		DataBucket::DeleteData(dev_tools_key);
		c->SetDevToolsEnabled(true);
	}

	c->ShowDevToolsMenu();
}

void command_clearsaylink(Client *c, const Seperator *sep) {
	database.ClearSayLink();
	c->Message(0, "Clearing Saylinks");
}

#include "gm_commands/advnpcspawn.cpp"
#include "gm_commands/aggrozone.cpp"
#include "gm_commands/ai.cpp"
#include "gm_commands/allowexport.cpp"
#include "gm_commands/altactivate.cpp"
#include "gm_commands/appearance.cpp"
#include "gm_commands/attack.cpp"
#include "gm_commands/attackentity.cpp"
#include "gm_commands/ban.cpp"
#include "gm_commands/beard.cpp"
#include "gm_commands/beardcolor.cpp"
#include "gm_commands/bestz.cpp"
#include "gm_commands/betabuff.cpp"
#include "gm_commands/boatinfo.cpp"
#include "gm_commands/bug.cpp"
#include "gm_commands/castspell.cpp"
#include "gm_commands/charactertransfer.cpp"
#include "gm_commands/chat.cpp"
#include "gm_commands/chattest.cpp"
#include "gm_commands/cleartimers.cpp"
#include "gm_commands/connectworldserver.cpp"
#include "gm_commands/corpse.cpp"
#include "gm_commands/crashtest.cpp"
#include "gm_commands/d1.cpp"
#include "gm_commands/damage.cpp"
#include "gm_commands/damagetotals.cpp"
#include "gm_commands/dbspawn2.cpp"
#include "gm_commands/delacct.cpp"
#include "gm_commands/deletegraveyard.cpp"
#include "gm_commands/depop.cpp"
#include "gm_commands/depopzone.cpp"
#include "gm_commands/disablerecipe.cpp"
#include "gm_commands/doanim.cpp"
#include "gm_commands/emote.cpp"
#include "gm_commands/enablerecipe.cpp"
#include "gm_commands/equipitem.cpp"
#include "gm_commands/expansion.cpp"
#include "gm_commands/face.cpp"
#include "gm_commands/falltest.cpp"
#include "gm_commands/fillbuffs.cpp"
#include "gm_commands/find.cpp"
#include "gm_commands/fish.cpp"
#include "gm_commands/fixmob.cpp"
#include "gm_commands/flagedit.cpp"
#include "gm_commands/fleeinfo.cpp"
#include "gm_commands/forage.cpp"
#include "gm_commands/giveitem.cpp"
#include "gm_commands/givemoney.cpp"
#include "gm_commands/giveplayerfaction.cpp"
#include "gm_commands/gmdamage.cpp"
#include "gm_commands/goto.cpp"
#include "gm_commands/grid.cpp"
#include "gm_commands/gridrecord.cpp"
#include "gm_commands/guild.cpp"
#include "gm_commands/guildapprove.cpp"
#include "gm_commands/guildcreate.cpp"
#include "gm_commands/guildlist.cpp"
#include "gm_commands/hair.cpp"
#include "gm_commands/haircolor.cpp"
#include "gm_commands/helm.cpp"
#include "gm_commands/interrogateinv.cpp"
#include "gm_commands/interrogatelegacy.cpp"
#include "gm_commands/interrupt.cpp"
#include "gm_commands/ipban.cpp"
#include "gm_commands/ipexemption.cpp"
#include "gm_commands/iteminfo.cpp"
#include "gm_commands/keyring.cpp"
#include "gm_commands/kick.cpp"
#include "gm_commands/kill.cpp"
#include "gm_commands/leaderboard.cpp"
#include "gm_commands/list.cpp"
#include "gm_commands/listnpcs.cpp"
#include "gm_commands/loc.cpp"
#include "gm_commands/logs.cpp"
#include "gm_commands/logtest.cpp"
#include "gm_commands/makemule.cpp"
#include "gm_commands/lootsim.cpp"
#include "gm_commands/makepet.cpp"
#include "gm_commands/manaburn.cpp"
#include "gm_commands/manastat.cpp"
#include "gm_commands/memspell.cpp"
#include "gm_commands/merchantcloseshop.cpp"
#include "gm_commands/merchantopenshop.cpp"
#include "gm_commands/modifynpcstat.cpp"
#include "gm_commands/movechar.cpp"
#include "gm_commands/mule.cpp"
#include "gm_commands/mysql.cpp"
#include "gm_commands/mysqltest.cpp"
#include "gm_commands/mystats.cpp"
#include "gm_commands/ngperma.cpp"
#include "gm_commands/npccast.cpp"
#include "gm_commands/npcedit.cpp"
#include "gm_commands/npcemote.cpp"
#include "gm_commands/npcloot.cpp"
#include "gm_commands/npcsay.cpp"
#include "gm_commands/npcshout.cpp"
#include "gm_commands/npcspawn.cpp"
#include "gm_commands/npctypecache.cpp"
#include "gm_commands/npctypespawn.cpp"
#include "gm_commands/nukebuffs.cpp"
#include "gm_commands/nukeitem.cpp"
#include "gm_commands/numauths.cpp"
#include "gm_commands/optest.cpp"
#include "gm_commands/path.cpp"
#include "gm_commands/petition.cpp"
#include "gm_commands/pf.cpp"
#include "gm_commands/playsound.cpp"
#include "gm_commands/profanity.cpp"
#include "gm_commands/push.cpp"
#include "gm_commands/qtest.cpp"
#include "gm_commands/quaketrigger.cpp"
#include "gm_commands/raidloot.cpp"
#include "gm_commands/randomfeatures.cpp"
#include "gm_commands/randtest.cpp"
#include "gm_commands/refreshgroup.cpp"
#include "gm_commands/reload.cpp"
#include "gm_commands/reloadspmod.cpp"
#include "gm_commands/removelegacyitem.cpp"
#include "gm_commands/repop.cpp"
#include "gm_commands/repopclose.cpp"
#include "gm_commands/resetaa.cpp"
#include "gm_commands/resetboat.cpp"
#include "gm_commands/revoke.cpp"
#include "gm_commands/rewind.cpp"
#include "gm_commands/rules.cpp"
#include "gm_commands/save.cpp"
#include "gm_commands/scribespell.cpp"
#include "gm_commands/scribespells.cpp"
#include "gm_commands/sendop.cpp"
#include "gm_commands/sendzonespawns.cpp"
#include "gm_commands/serversidename.cpp"
#include "gm_commands/set.cpp"
#include "gm_commands/setnpcexpansion.cpp"
#include "gm_commands/setgraveyard.cpp"
#include "gm_commands/setgreed.cpp"
#include "gm_commands/showlootlockouts.cpp"
#include "gm_commands/showbonusstats.cpp"
#include "gm_commands/showfilters.cpp"
#include "gm_commands/show.cpp"
#include "gm_commands/showhelm.cpp"
#include "gm_commands/showpetspell.cpp"
#include "gm_commands/showquake.cpp"
#include "gm_commands/showregen.cpp"
#include "gm_commands/showtraderitems.cpp"
#include "gm_commands/shutdown.cpp"
#include "gm_commands/size.cpp"
#include "gm_commands/skilldifficulty.cpp"
#include "gm_commands/spawn.cpp"
#include "gm_commands/spawnfix.cpp"
#include "gm_commands/spellinfo.cpp"
#include "gm_commands/starve.cpp"
#include "gm_commands/stun.cpp"
#include "gm_commands/summon.cpp"
#include "gm_commands/summonitem.cpp"
#include "gm_commands/suspend.cpp"
#include "gm_commands/synctod.cpp"
#include "gm_commands/takemoney.cpp"
#include "gm_commands/testcommand.cpp"
#include "gm_commands/testspawn.cpp"
#include "gm_commands/togglepvp.cpp"
#include "gm_commands/undeletechar.cpp"
#include "gm_commands/underworld.cpp"
#include "gm_commands/unmemspell.cpp"
#include "gm_commands/unmemspells.cpp"
#include "gm_commands/unscribespell.cpp"
#include "gm_commands/unscribespells.cpp"
#include "gm_commands/viewplayerfaction.cpp"
#include "gm_commands/wc.cpp"
#include "gm_commands/worldshutdown.cpp"
#include "gm_commands/wp.cpp"
#include "gm_commands/wpadd.cpp"
#include "gm_commands/xpinfo.cpp"
#include "gm_commands/zone.cpp"
#include "gm_commands/zonebootup.cpp"
#include "gm_commands/zoneguild.cpp"
#include "gm_commands/zoneshutdown.cpp"
#include "gm_commands/zonespawn.cpp"
#include "gm_commands/zopp.cpp"
#include "gm_commands/zsave.cpp"
#include "gm_commands/zuwcoords.cpp"