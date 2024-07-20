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
#include "../common/eqemu_logsys.h"
#include "../common/languages.h"

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

extern WorldServer worldserver;
extern QueryServ* QServ;

void CatchSignal(int sig_num);


int commandcount;					// how many commands we have

// this is the pointer to the dispatch function, updated once
// init has been performed to point at the real function
int(*command_dispatch)(Client *, char const *) = command_notavail;


void command_bestz(Client *c, const Seperator *message);
void command_pf(Client *c, const Seperator *message);

std::map<std::string, CommandRecord *> commandlist;
std::map<std::string, std::string> commandaliases;

//All allocated CommandRecords get put in here so they get deleted on shutdown
LinkedList<CommandRecord *> cleanup_commandlist;

/*
* command_notavail
* This is the default dispatch function when commands aren't loaded.
*
* Parameters:
*	not used
*
*/
int command_notavail(Client *c, const char *message)
{
	c->Message(CC_Red, "Commands not available.");
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
	commandaliases.clear();

	if
		(
		command_add("advnpcspawn", "[maketype|makegroup|addgroupentry|addgroupspawn][removegroupspawn|movespawn|editgroupbox|cleargroupbox].", AccountStatus::GMImpossible, command_advnpcspawn) ||
		command_add("aggro", "(range) [-v] - Display aggro information for all mobs 'range' distance from your target. -v is verbose faction info.", AccountStatus::GMStaff, command_aggro) ||
		command_add("aggrozone", "[aggro] [0/1: Enforce ignore distance. If 0 or not set, all will come] - Aggro every mob in the zone with X aggro. Default is 0. Not recommend if you're not invulnerable.", AccountStatus::GMImpossible, command_aggrozone) ||
		command_add("ai", "[factionid/spellslist/con/guard/roambox/stop/start] - Modify AI on NPC target.", AccountStatus::GMImpossible, command_ai) || 
		command_add("allowexport", "[off, worn, inventory, bank] - Authorize export of this character to be included in nightly, open sourced database dumps.", AccountStatus::Player, command_allowexport) ||
		command_add("altactivate", "[argument] - activates alternate advancement abilities, use altactivate help for more information.", AccountStatus::GMAreas, command_altactivate) ||
		command_add("appearance", "[type] [value] - Send an appearance packet for you or your target.", AccountStatus::GMImpossible, command_appearance) ||
		command_add("apply_shared_memory", "[shared_memory_name] - Tells every zone and world to apply a specific shared memory segment by name.", AccountStatus::GMImpossible, command_apply_shared_memory) ||
		command_add("attack", "[targetname] - Make your NPC target attack targetname.", AccountStatus::QuestMaster, command_attack) ||
		command_add("attackentity", "[entityid] - Make your NPC target attack target entity.", AccountStatus::QuestMaster, command_attackentity) ||

		command_add("ban", "[name][reason] - Ban by character name.", AccountStatus::GMAdmin, command_ban) ||
		command_add("beard", "- Change the beard of your target.", AccountStatus::GMImpossible, command_beard) ||
		command_add("beardcolor", "- Change the beard color of your target.", AccountStatus::GMImpossible, command_beardcolor) ||
		command_add("bestz", "- Ask map for a good Z coord for your x,y coords.", AccountStatus::ApprenticeGuide, command_bestz) ||
		command_add("betabuff", "[level] - Buffs user's player to provided level, giving level * 100 platinum, along with providing a (potentially non era-specific) set of reagents, spells and skills.", AccountStatus::GMImpossible, command_betabuff) ||
		command_add("bind", "- Sets your targets bind spot to their current location.", AccountStatus::SeniorGuide, command_bind) ||
		command_add("boatinfo", "- Gets infomation about the boats currently spawned in the zone.", AccountStatus::SeniorGuide, command_boatinfo) ||
		command_add("bug", "- Bug report system. Encase your bug in quotes. Type: #bug <quote>I have a bug</quote>.", AccountStatus::EQSupport, command_bug) ||

		command_add("castspell", "[spellid] [gm_override] [entityid] - Cast a spell. GM override bypasses resist and stacking checks. If entityid is specified, that NPC will cast a spell on the target mob.", AccountStatus::QuestMaster, command_castspell) ||
		command_add("chat", "[channel num] [message] - Send a channel message to all zones.", AccountStatus::EQSupport, command_chat) ||
		command_add("chattest", "[color] [loops] - Sends a test message with the specified color to yourself.", AccountStatus::GMCoder, command_chattest) ||
		command_add("checklos", "- Check for line of sight to your target.", AccountStatus::GMStaff, command_checklos) ||
		command_add("cleartimers", "- [timer] Clears one or all persistant timers on target.", AccountStatus::GMMgmt, command_cleartimers) ||
		command_add("connectworldserver", "- Make zone attempt to connect to worldserver.", AccountStatus::GMTester, command_connectworldserver) ||
#ifdef WIN32
#else
		command_add("coredump", "Dumps a core log of any existing cores to view on web page.", AccountStatus::GMCoder, command_coredump) ||
#endif
		command_add("corpse", "- Manipulate corpses, use with no arguments for help.", AccountStatus::EQSupport, command_corpse) ||
		command_add("crashtest", "- Crash the zoneserver.", AccountStatus::GMImpossible, command_crashtest) ||
		command_add("cvs", "- Summary of client versions currently online.", AccountStatus::GMCoder, command_cvs) ||

		command_add("damage", "[amount] - Damage your target.", AccountStatus::QuestMaster, command_damage) ||
		command_add("damagetotals", "Displays a list of what has damaged your NPC target.", AccountStatus::GMAdmin, command_damagetotals) ||
		command_add("date", "[yyyy] [mm] [dd] [HH] [MM] - Set EQ time.", AccountStatus::GMImpossible, command_date) ||
		command_add("dbspawn2", "[spawngroup] [respawn] [variance] - Spawn an NPC from a predefined row in the spawn2 table.", AccountStatus::QuestMaster, command_dbspawn2) ||
		command_add("delacct", "[accountname] - Delete an account.", AccountStatus::GMImpossible, command_delacct) ||
		command_add("deletegraveyard", "[zone name] - Deletes the graveyard for the specified zone.", AccountStatus::GMImpossible, command_deletegraveyard) ||
		command_add("depop", "- Depop your NPC target.", AccountStatus::GMLeadAdmin, command_depop) ||
		command_add("depopzone", "- Depop the zone.", AccountStatus::GMAreas, command_depopzone) ||
		command_add("disablerecipe", "[recipe_id] - Disables a recipe using the recipe id.", AccountStatus::GMImpossible, command_disablerecipe) ||
		command_add("distance", "- Reports the distance between you and your target.", AccountStatus::Steward, command_distance) ||
		command_add("d1", "[type] [spell] [damage] - Send an OP_Action packet with the specified values.", AccountStatus::GMImpossible, command_d1) ||
		command_add("doanim", "[animnum] [type] - Send an EmoteAnim for you or your target.", AccountStatus::GMStaff, command_doanim) ||

		command_add("emote", "['name'/'world'/'zone'] [type] [message] - Send an emote message.", AccountStatus::GMStaff, command_emote) ||
		command_add("emotesearch", "Searches NPC Emotes.", AccountStatus::GMStaff, command_emotesearch) ||
		command_add("emoteview", "Lists all NPC Emotes.", AccountStatus::GMStaff, command_emoteview) ||
		command_add("enablerecipe", "[recipe_id] - Enables a recipe using the recipe id.", AccountStatus::GMImpossible, command_enablerecipe) ||
		command_add("equipitem", "[slotid(0-21)] - Equip the item on your cursor into the specified slot.", AccountStatus::GMLeadAdmin, command_equipitem) ||
		command_add("expansion", "[accountname][expansion] - Sets the expansion value for the specified account.", AccountStatus::GMLeadAdmin, command_expansion) ||

		command_add("face", "- Change the face of your target.", AccountStatus::GMLeadAdmin, command_face) || 
		command_add("falltest", "[+Z] sends you to your current loc plus the Z specified.", AccountStatus::GMImpossible, command_falltest) ||
		command_add("fillbuffs", "Casts 15 buffs on the target for testing.", AccountStatus::QuestTroupe, command_fillbuffs) ||
		command_add("findaliases", "[search term]- Searches for available command aliases, by alias or command", AccountStatus::ApprenticeGuide, command_findaliases) ||
		command_add("findnpctype", "[search criteria] - Search database NPC types.", AccountStatus::GMStaff, command_findnpctype) ||
		command_add("findspell", "[searchstring] - Search for a spell.", AccountStatus::Player, command_findspell) ||
		command_add("findzone", "[search criteria] - Search database zones.", AccountStatus::QuestTroupe, command_findzone) ||
		command_add("fixmob", "[race|gender|texture|helm|face|hair|haircolor|beard|beardcolor|heritage|tattoo|detail] [next|prev] - Manipulate appearance of your target.", AccountStatus::GMImpossible, command_fixmob) ||
		command_add("flag", "[status] [acctname] - Refresh your admin status, or set an account's admin status if arguments provided.", AccountStatus::GMImpossible, command_flag) ||
		command_add("flagedit", "- Edit zone flags on your target.", AccountStatus::GMMgmt, command_flagedit) ||
		command_add("fleeinfo", "- Gives info about whether a NPC will flee or not, using the command issuer as top hate.", AccountStatus::QuestTroupe, command_fleeinfo) ||
		command_add("flags", "- displays the flags of you or your target.", AccountStatus::EQSupport, command_flags) ||
		command_add("flymode", "[0/1/2] - Set your or your player target's flymode to off/on/levitate.", AccountStatus::QuestTroupe, command_flymode) ||
		command_add("fov", "- Check wether you're behind or in your target's field of view.", AccountStatus::QuestTroupe, command_fov) ||
		command_add("freeze", "- Freeze your target.", AccountStatus::QuestMaster, command_freeze) ||

		command_add("gassign", "[id] - Assign targetted NPC to predefined wandering grid id.", AccountStatus::GMImpossible, command_gassign) ||
		command_add("gender", "[0/1/2] - Change your or your target's gender to male/female/neuter.", AccountStatus::QuestMaster, command_gender) ||
		command_add("getvariable", "[varname] - Get the value of a variable from the database.", AccountStatus::GMCoder, command_getvariable) ||
		command_add("ginfo", "- get group info on target.", AccountStatus::QuestTroupe, command_ginfo) ||
		command_add("giveitem", "[itemid] [charges] [reason] - Summon an item onto your target's cursor. Charges are optional.", AccountStatus::GMLeadAdmin, command_giveitem) ||
		command_add("givemoney", "[pp] [gp] [sp] [cp] [reason]  - Gives specified amount of money to the target player.", AccountStatus::GMLeadAdmin, command_givemoney) ||
		command_add("giveplayerfaction", "[factionid] [factionvalue] - Gives the target player faction with the given faction. (Acts as a hit).", AccountStatus::GMMgmt, command_giveplayerfaction) ||
		command_add("globalview", "Lists all qglobals in cache if you were to do a quest with this target.", AccountStatus::GMStaff, command_globalview) ||
		command_add("gm", "- Turn player target's or your GM flag on or off.", AccountStatus::GMStaff, command_gm) ||
		command_add("gmdamage", "[amount] [skipaggro] - Damage your target. Skips most combat checks, including invul.", AccountStatus::QuestMaster, command_gmdamage) ||
		command_add("gmspeed", "[on/off] - Turn GM speed hack on/off for you or your player target.", AccountStatus::GMLeadAdmin, command_gmspeed) ||
		command_add("godmode", "[on/off] - Turns on/off hideme, gmspeed, invul, and flymode.", AccountStatus::GMMgmt, command_godmode) ||
		command_add("goto", "[x] [y] [z] - Teleport to the provided coordinates or to your target.", AccountStatus::ApprenticeGuide, command_goto) ||
		command_add("grid", "[add/delete] [grid_num] [wandertype] [pausetype] - Create/delete a wandering grid.", AccountStatus::GMImpossible, command_grid) ||
		command_add("gridrecord", "[start|stop|addwp] - Record a grid using start, stop and use AddWp to add a waypoint to the grid in question.", 170, command_gridrecord) ||
		command_add("guild", "- Guild manipulation commands. Use argument help for more info.", AccountStatus::EQSupport, command_guild) ||
		command_add("guildapprove", "[guildapproveid] - Approve a guild with specified ID (guild creator receives the id).", AccountStatus::EQSupport, command_guildapprove) ||
		command_add("guildcreate", "[guildname] - Creates an approval setup for guild name specified.", AccountStatus::EQSupport, command_guildcreate) ||
		command_add("guildlist", "[guildapproveid] - Lists character names who have approved the guild specified by the approve id.", AccountStatus::EQSupport, command_guildlist) ||

		command_add("hair", "- Change the hair style of your target.", AccountStatus::GMImpossible, command_hair) ||
		command_add("haircolor", "- Change the hair color of your target.", AccountStatus::GMImpossible, command_haircolor) ||
		command_add("haste", "[percentage] - Set your haste percentage.", AccountStatus::GMLeadAdmin, command_haste) ||
		command_add("hatelist", " - Display hate list for target.", AccountStatus::QuestTroupe, command_hatelist) ||
		command_add("heal", "- Completely heal your target.", AccountStatus::GMMgmt, command_heal) ||
		command_add("helm", "- Change the helm of your target.", AccountStatus::GMImpossible, command_helm) ||
		command_add("help", "[search term] - List available commands and their description, specify partial command as argument to search.", AccountStatus::Player, command_help) ||
		command_add("hideme", "[on/off] - Hide yourself from spawn lists.", AccountStatus::SeniorGuide, command_hideme) ||
		command_add("hotfix", "[hotfix_name] - Reloads shared memory into a hotfix, equiv to load_shared_memory followed by apply_shared_memory", AccountStatus::GMImpossible, command_hotfix) ||
		command_add("hp", "- Refresh your HP bar from the server.", AccountStatus::Max, command_hp) ||

		command_add("interrogateinv", "- use [help] argument for available options.", AccountStatus::GMLeadAdmin, command_interrogateinv) ||
		command_add("interrogatelegacy", "- Interrogates legacy items of your current target", AccountStatus::GMAdmin, command_interrogatelegacy) ||

		command_add("interrupt", "[message id] [color] - Interrupt your casting. Arguments are optional.", AccountStatus::EQSupport, command_interrupt) ||
		command_add("invul", "[on/off] - Turn player target's or your invulnerable flag on or off", AccountStatus::QuestTroupe, command_invul) ||
		command_add("ipban", "[IP address] - Ban IP by character name.", AccountStatus::GMMgmt, command_ipban) ||
#ifdef IPC
		command_add("ipc", "- Toggle an NPC's interactive flag.", AccountStatus::GMImpossible, command_ipc) ||
#endif
		command_add("ipexemption", "[accountname] [exemption] - Set IP exemption amount for accountname by amount. Accounts default to 1.", AccountStatus::GMAdmin, command_ipexemption) ||
		command_add("iplookup", "[charname] - Look up IP address of charname.", AccountStatus::GMStaff, command_iplookup) ||
		command_add("iteminfo", "- Get information about the item on your cursor.", AccountStatus::Guide, command_iteminfo) ||
		command_add("itemsearch", "[search criteria] - Search for an item.", AccountStatus::GMAdmin, command_itemsearch) ||

		command_add("keyring", "Displays target's keyring items.", AccountStatus::EQSupport, command_keyring) ||
		command_add("kick", "[charname] - Disconnect charname.", AccountStatus::EQSupport, command_kick) ||
		command_add("kill", "- Kill your target.", AccountStatus::GMLeadAdmin, command_kill) ||

		command_add("lastname", "[new lastname] - Set your or your player target's lastname.", AccountStatus::EQSupport, command_lastname) ||
		command_add("leaderboard", "[SFHC|SSFHC|SFHCOnly|HC] - List hardcore leaderboard.", AccountStatus::Player, command_leaderboard) ||
		command_add("level", "[level] - Set your or your target's level.", AccountStatus::QuestTroupe, command_level) ||
		command_add("listnpcs", "[name/range] - Search NPCs.", AccountStatus::EQSupport, command_listnpcs) ||
		command_add("load_shared_memory", "[shared_memory_name] - Reloads shared memory and uses the input as output", AccountStatus::GMImpossible, command_load_shared_memory) ||
		command_add("loc", "- Print out your or your target's current location and heading.", AccountStatus::Player, command_loc) ||
		command_add("logs", "Manage anything to do with logs.", AccountStatus::GMCoder, command_logs) ||
		command_add("logtest", "Performs log performance testing.", AccountStatus::GMImpossible, command_logtest) ||

		command_add("makemule", "Flags the account of the player who runs the command as a mule.", AccountStatus::Player, command_makemule) ||
		command_add("makepet", "[level] [class] [race] [texture] - Make a pet.", AccountStatus::QuestMaster, command_makepet) ||
		command_add("mana", "- Fill your or your target's mana.", AccountStatus::GMMgmt, command_mana) ||
		command_add("manaburn", "- Use AA Wizard class skill manaburn on target.", AccountStatus::GMAreas, command_manaburn) ||
		command_add("manastat", "- Report your or your target's cur/max mana.", AccountStatus::Guide, command_manastat) ||
		command_add("maxskills", "Maxes skills for you.", AccountStatus::Guide, command_maxallskills) ||
		command_add("memspell", "[slotid] [spellid] - Memorize spellid in the specified slot.", AccountStatus::GMAreas, command_memspell) ||
		command_add("merchant_close_shop", "Closes a merchant shop.", AccountStatus::GMImpossible, command_merchantcloseshop) ||
		command_add("merchant_open_shop", "Opens a merchants shop.", AccountStatus::GMImpossible, command_merchantopenshop) ||
		command_add("modifynpcstat", "- Modifys a NPC's stats.", AccountStatus::GMImpossible, command_modifynpcstat) ||
		command_add("motd", "[Message of the Day] - Set Message of the Day (leave empty to have no Message of the Day)", AccountStatus::GMAreas, command_motd) ||
		command_add("movechar", "[charname] [zonename] - Move charname to zonename.", AccountStatus::GMStaff, command_movechar) ||
		command_add("mule", "[account name] [0/1] - Toggles the mule status of the specified account ", AccountStatus::GMImpossible, command_mule) ||
		command_add("mysql", "Mysql CLI, see 'help' for options.", AccountStatus::GMImpossible, command_mysql) ||
		command_add("mysqltest", "Akkadius MySQL Bench Test.", AccountStatus::GMImpossible, command_mysqltest) ||
		command_add("mystats", "- Show details about you or your pet.", AccountStatus::Guide, command_mystats) ||

		command_add("name", "[newname] - Rename your player target.", AccountStatus::GMStaff, command_name) ||
		command_add("netstats", "- Gets the network stats for a stream.", AccountStatus::GMCoder, command_netstats) ||
		command_add("npccast", "[targetname/entityid] [spellid] - Causes NPC target to cast spellid on targetname/entityid.", AccountStatus::QuestMaster, command_npccast) ||
		command_add("npcedit", "[column] [value] - Mega NPC editing command.", AccountStatus::GMImpossible, command_npcedit) ||
		command_add("npcemote", "[message] - Make your NPC target emote a message.", AccountStatus::QuestMaster, command_npcemote) ||
		command_add("npcloot", "[show/money/add/remove] [itemid/all/money: pp gp sp cp] - Manipulate the loot an NPC is carrying.", AccountStatus::QuestMaster, command_npcloot) ||
		command_add("npcsay", "[message] - Make your NPC target say a message.", AccountStatus::QuestMaster, command_npcsay) ||
		command_add("npcshout", "[message] - Make your NPC target shout a message.", AccountStatus::QuestMaster, command_npcshout) ||
		command_add("npcspawn", "[create/add/update/remove/delete] - Manipulate spawn DB.", AccountStatus::GMImpossible, command_npcspawn) ||
		command_add("npcstats", "- Show stats about target NPC.", AccountStatus::SeniorGuide, command_npcstats) ||
		command_add("npctypecache", "[id] or all - Clears the npc type cache for either the id or all npcs.", AccountStatus::GMImpossible, command_npctypecache) ||
		command_add("npctypespawn", "[npctypeid] [factionid] - Spawn an NPC from the db.", AccountStatus::QuestMaster, command_npctypespawn) ||
		command_add("nukebuffs", "- Strip all buffs on you or your target.", AccountStatus::GMLeadAdmin, command_nukebuffs) ||
		command_add("nukeitem", "[itemid] - Remove itemid from your player target's inventory.", AccountStatus::GMLeadAdmin, command_nukeitem) ||
		command_add("numauths", "- TODO: describe this command.", AccountStatus::Max, command_numauths) ||

		command_add("oocmute", "[1/0] - Mutes OOC chat.", AccountStatus::GMStaff, command_oocmute) ||
		command_add("opcode", "- opcode management.", AccountStatus::GMCoder, command_opcode) ||
		command_add("optest", "- solar's private test command.", AccountStatus::GMCoder, command_optest) ||

#ifdef PACKET_PROFILER
		command_add("packetprofile", "- Dump packet profile for target or self.", AccountStatus::GMCoder, command_packetprofile) ||
#endif
		command_add("path", "- view and edit pathing.", AccountStatus::GMImpossible, command_path) ||
		command_add("peekinv", "[worn/inv/cursor/bank/trade/world/all] - Print out contents of your player target's inventory.", AccountStatus::EQSupport, command_peekinv) ||
		command_add("permaclass", "[classnum] - Change your or your player target's class (target is disconnected).", AccountStatus::GMMgmt, command_permaclass) ||
		command_add("permagender", "[gendernum] - Change your or your player target's gender (zone to take effect).", AccountStatus::GMMgmt, command_permagender) ||
		command_add("permarace", "[racenum] - Change your or your player target's race (zone to take effect).", AccountStatus::GMMgmt, command_permarace) ||
		command_add("petition", "Handles everything petition related. Use with no args or with 'help' for how to use.", AccountStatus::ApprenticeGuide, command_petition) ||
		command_add("peqzone", "[zonename] - Go to specified zone, if you have > 75% health.", AccountStatus::Max, command_peqzone) ||
		command_add("pf", "- Display additional mob coordinate and wandering data.", AccountStatus::GMStaff, command_pf) ||
#ifdef EQPROFILE
		command_add("profiledump", "- Dump profiling info to logs.", AccountStatus::GMImpossible, command_profiledump) ||
		command_add("profilereset", "- Reset profiling info.", AccountStatus::GMImpossible, command_profilereset) ||
#endif
		command_add("playsound", "[number] - Plays a sound in the client.  Valid range 0-3999", AccountStatus::ApprenticeGuide, command_playsound) ||
		command_add("push", "[pushback] [pushup] - Pushes the target the specified amount.", AccountStatus::GMImpossible, command_push) ||
		command_add("pvp", "[on/off] - Set your or your player target's PVP status.", AccountStatus::GMAdmin, command_pvp) ||

		command_add("qglobal", "[on/off/view] - Toggles qglobal functionality on an NPC.", AccountStatus::GMImpossible, command_qglobal) ||
		command_add("qtest", "- QueryServ testing command.", AccountStatus::GMTester, command_qtest) ||
		command_add("quaketrigger", "- [type_num (1 = Normal, 2 = PVP)] Triggers an earthquake manually", AccountStatus::GMImpossible, command_quaketrigger) ||
		command_add("questerrors", "Shows quest errors.",AccountStatus::Player, command_questerrors) ||

		command_add("race", "[racenum] - Change your or your target's race. Use racenum 0 to return to normal.", AccountStatus::QuestMaster, command_race) ||
		command_add("raidloot", "LEADER|GROUPLEADER|SELECTED|ALL - Sets your raid loot settings if you have permission to do so.", 1, command_raidloot) ||
		command_add("randtest", "Perform a sampling of random number generation", AccountStatus::GMImpossible, command_randtest) ||
		command_add("randomfeatures", "- Temporarily randomizes the Facial Features of your target.", AccountStatus::GMCoder, command_randomfeatures) ||
		command_add("refreshgroup", "- Refreshes Group.", AccountStatus::EQSupport, command_refreshgroup) ||
		command_add("reloadallrules", "Executes a reload of all rules.", AccountStatus::GMCoder, command_reloadallrules) ||
		command_add("reloademote", "Reloads NPC Emotes.", AccountStatus::GMCoder, command_reloademote) ||
		command_add("reloadlevelmods", nullptr, AccountStatus::Max, command_reloadlevelmods) ||
		command_add("reloadmerchants", "Reloads NPC merchant list.", AccountStatus::Max, command_reloadmerchants) ||
		command_add("reloadqst", " - Clear quest cache (any argument causes it to also stop all timers).", AccountStatus::QuestMaster, command_reloadqst) ||
		command_add("reloadrulesworld", "Executes a reload of all rules in world specifically.", AccountStatus::GMCoder, command_reloadworldrules) ||
		command_add("reloadstatic", "- Reload Static Zone Data.", AccountStatus::GMCoder, command_reloadstatic) ||
		command_add("reloadtitles", "- Reload player titles from the database.", AccountStatus::GMCoder, command_reloadtitles) ||
		command_add("reloadtraps", "- Repops all traps in the current zone.", AccountStatus::QuestTroupe, command_reloadtraps) ||
		command_add("reloadworld", "[0|1] - Clear quest cache and reload all rules (0 - no repop, 1 - repop).", AccountStatus::GMImpossible, command_reloadworld) ||
		command_add("reloadzps", "- Reload zone points from database", AccountStatus::GMLeadAdmin, command_reloadzps) ||
		command_add("removelegacyitem", "- Remove a legacy item from your target [itemid], or specify a [charid] [itemid] to remove the flag of.", AccountStatus::GMAdmin, command_removelegacyitem) ||
		command_add("repop", "[delay] - Repop the zone with optional delay.", AccountStatus::GMLeadAdmin, command_repop) ||
		command_add("repopclose", "[distance in units] Repops only NPC's nearby for fast development purposes", AccountStatus::GMAdmin, command_repopclose) ||
		command_add("resetaa", "- Resets a Player's AA in their profile and refunds spent AA's to unspent, disconnects player.", AccountStatus::GMImpossible, command_resetaa) ||
		command_add("resetboat", "- Sets player's boat to 0 in their profile.", AccountStatus::GMStaff, command_resetboat) ||
		command_add("revoke", "[charname] [1/0] - Makes charname unable to talk on OOC.", AccountStatus::GMStaff, command_revoke) ||
		command_add("rewind", nullptr,AccountStatus::Player, command_rewind) ||
		command_add("rules", "(subcommand) - Manage server rules.", AccountStatus::GMImpossible, command_rules) ||

		command_add("save", "- Force your player or player corpse target to be saved to the database.", AccountStatus::GMLeadAdmin, command_save) ||
		command_add("scribespell", "[spellid] - Scribe specified spell in your target's spell book.", AccountStatus::GMAreas, command_scribespell) ||
		command_add("scribespells", "[max level] [min level] - Scribe all spells for you or your player target that are usable by them, up to level specified. (may freeze client for a few seconds).", AccountStatus::GMAreas, command_scribespells) ||
		command_add("sendop", "[opcode] - LE's Private test command, leave it alone.", AccountStatus::GMCoder, command_sendop) ||
		command_add("sendzonespawns", "- Refresh spawn list for all clients in zone.", AccountStatus::GMAdmin, command_sendzonespawns) ||
		command_add("serverlock", "[0|1] - Lock or Unlock the World Server (0 = Unlocked, 1 = Locked)", AccountStatus::GMLeadAdmin, command_serverlock) ||
		command_add("serversidename", "- Prints target's server side name.", AccountStatus::GMAdmin, command_serversidename) ||
		command_add("setaapts", "[value] - Set your or your player target's available AA points.", AccountStatus::GMImpossible, command_setaapts) ||
		command_add("setaaxp", "[value] - Set your or your player target's AA experience.", AccountStatus::GMImpossible, command_setaaxp) ||
		command_add("setanim", "[animnum] - Set target's appearance to animnum.", AccountStatus::GMImpossible, command_setanim) ||
		command_add("setfaction", "[faction number] - Sets targeted NPC's faction in the database.", AccountStatus::GMImpossible, command_setfaction) ||
		command_add("setgraveyard", "[zone name] - Creates a graveyard for the specified zone based on your target's LOC.", AccountStatus::GMImpossible, command_setgraveyard) ||
		command_add("setgreed", "[greed] - Sets a merchant greed value.", AccountStatus::GMAdmin, command_setgreed) ||
		command_add("setlanguage", "[language ID] [value] - Set your target's language skillnum to value.", AccountStatus::GMAreas, command_setlanguage) ||
		command_add("setlsinfo", "[email] [password] - Set login server email address and password (if supported by login server).", AccountStatus::Max, command_setlsinfo) ||
		command_add("setnpcexpansion", "[min_expansion] [max_expansion] - Restrict an NPC's spawn2 (spawn location) by min, max expansion. These are float values.", AccountStatus::GMAdmin, command_setnpcexpansion) ||
		command_add("setpass", "[accountname] [password] - Set local password for accountname.", AccountStatus::Max, command_setpass) ||
		command_add("setskill", "[skillnum] [value] - Set your target's skill skillnum to value.", AccountStatus::GMAreas, command_setskill) ||
		command_add("setskillall", "[value] - Set all of your target's skills to value.", AccountStatus::GMAreas, command_setskillall) ||
		command_add("setxp", "[value] - Set your or your player target's experience.", AccountStatus::GMAreas, command_setxp) ||
		command_add("showbonusstats", "[item|spell|all] Shows bonus stats for target from items or spells. Shows both by default.", AccountStatus::Guide, command_showbonusstats) ||
		command_add("showbuffs", "- List buffs active on your target or you if no target.", AccountStatus::Guide, command_showbuffs) ||
		command_add("showfilters", "- list client serverfilter settings.", AccountStatus::GMCoder, command_showfilters) ||
		command_add("showhelm", "on/off [all] Toggles displaying of player helms (including your own.) Specifying 'all' toggles every character currently on your account", AccountStatus::Player, command_showhelm) ||
		command_add("showlootlockouts", "- Shows your currently active loot lockouts. These do not apply to earthquake creatures.", AccountStatus::Player, command_showlootlockouts) ||
		command_add("showpetspell", "[spellid/searchstring] - search pet summoning spells.", AccountStatus::Guide, command_showpetspell) ||
		command_add("showquake", "- Shows current earthquake timer. Requires you to be a guild officer or leader.", AccountStatus::Player, command_showquake) ||
		command_add("showregen", "- Shows information about your target's regen.", AccountStatus::GMAdmin, command_showregen) ||
		command_add("showskills", "- Show the values of your skills if no target, or your target's skills.", AccountStatus::Guide, command_showskills) ||
		command_add("showspellslist", "Shows spell list of targeted NPC.", AccountStatus::GMStaff, command_showspellslist) ||
		command_add("showstats", "[quick stats]- Show details about you or your target. Quick stats shows only key stats.", AccountStatus::Guide, command_showstats) ||
		command_add("showtraderitems", "Displays the list of items a trader has up for sale.", AccountStatus::QuestTroupe, command_showtraderitems) ||
		command_add("shutdown", "- Shut this zone process down.", AccountStatus::GMImpossible, command_shutdown) ||
		command_add("size", "[size] - Change size of you or your target.", AccountStatus::GMAdmin, command_size) ||
		command_add("skills", "List skill difficulty.", AccountStatus::GMAdmin, command_skilldifficulty) ||
		command_add("spawn", "[name] [race] [level] [material] [hp] [gender] [class] [priweapon] [secweapon] [merchantid] - Spawn an NPC.", AccountStatus::GMImpossible, command_spawn) ||
		command_add("spawnfix", "- Find targeted NPC in database based on its X/Y/heading and update the database to make it spawn at your current location/heading.", AccountStatus::GMImpossible, command_spawnfix) ||
		command_add("spawnstatus", "[a|u|s|d|e|spawnid|help] - Show respawn timer status.", AccountStatus::GMStaff, command_spawnstatus) ||
		command_add("spellinfo", "[spellid] - Get detailed info about a spell.", AccountStatus::Guide, command_spellinfo) ||
		command_add("starve", "Sets hunger and thirst to 0.", AccountStatus::GMCoder, command_starve) ||
		command_add("stun", "[duration] - Stuns you or your target for duration.", AccountStatus::QuestMaster, command_stun) ||
		command_add("summon", "[charname] - Summons your player/npc/corpse target, or charname if specified.", AccountStatus::EQSupport, command_summon) ||
		command_add("summonitem", "[itemid] [charges] - Summon an item onto your cursor. Charges are optional.", AccountStatus::QuestMaster, command_summonitem) ||
		command_add("suspend", "[name][days][reason] - Suspend by character name and for specificed number of days.", AccountStatus::EQSupport, command_suspend) ||
		command_add("synctod", "- Send a time of day update to every client in zone.", AccountStatus::GMAdmin, command_synctod) ||

		command_add("testcopy", "Sends a copy of the targets loginserver/game account/characters to a backup file.", AccountStatus::GMImpossible, command_testcopy) ||
		command_add("testcommand", "Template for temporary commands as needed. Don't delete.", AccountStatus::GMImpossible, command_testcommand) ||
		command_add("testspawn", "[memloc] [value] - spawns a NPC for you only, with the specified values set in the spawn struct.", AccountStatus::GMCoder, command_testspawn) ||
		command_add("testspawnkill", "- Sends an OP_Death packet for spawn made with #testspawn.", AccountStatus::GMCoder, command_testspawnkill) ||
		command_add("texture", "[texture] [helmtexture] - Change your or your target's appearance, use 255 to show equipment.", AccountStatus::GMImpossible, command_texture) ||
		command_add("time", "[HH] [MM] - Set EQ time", AccountStatus::GMImpossible, command_time) ||
		command_add("timers", "- Display persistent timers for target.", AccountStatus::GMAdmin, command_timers) ||
		command_add("timezone", "[HH] [MM] - Set timezone. Minutes are optional.", AccountStatus::GMImpossible, command_timezone) ||
		command_add("title", "[text] [1 = create title table row] - Set your or your player target's title.", AccountStatus::GMStaff, command_title) ||
		command_add("titlesuffix", "[text] [1 = create title table row] - Set your or your player target's title suffix.", AccountStatus::Max, command_titlesuffix) ||
		command_add("trapinfo", "- Gets infomation about the traps currently spawned in the zone.", AccountStatus::SeniorGuide, command_trapinfo) ||

		command_add("undeletechar", "- Undelete a character that was previously deleted.", AccountStatus::Max, command_undeletechar) ||
		command_add("underworld", "[z] - Reports NPCs that are below the given Z or if not given, below the lowest spawn2/grid coord. If red, the NPC is below the underworld coord.", AccountStatus::QuestTroupe, command_underworld) ||
		command_add("unfreeze", "- Unfreeze your target.", AccountStatus::QuestMaster, command_unfreeze) ||
		command_add("unmemspell", "[spellid] - Unmem specified spell from your target's spell bar.", AccountStatus::GMAreas, command_unmemspell) ||
		command_add("unmemspells", "- Clear out your or your player target's spell gems.", AccountStatus::GMAreas, command_unmemspells) ||
		command_add("unscribespell", "[spellid] - Unscribe specified spell from your target's spell book.", AccountStatus::GMAreas, command_unscribespell) ||
		command_add("unscribespells", "- Clear out your or your player target's spell book.", AccountStatus::GMAreas, command_unscribespells) ||
		command_add("update", "Handles all server updates/reboots. Use with no args or 'help' for how to use.", AccountStatus::GMMgmt, command_update) ||
		command_add("uptime", "[zone server id] - Get uptime of worldserver, or zone server if argument provided.", AccountStatus::GMStaff, command_uptime) ||

		command_add("version", "- Display current version of EQEmu server.", AccountStatus::GMCoder, command_version) ||
		command_add("viewnpctype", "[npctype id] - Show info about an npctype.", AccountStatus::GMStaff, command_viewnpctype) ||
		command_add("viewplayerfaction", "[factionid] - Shows current personal and modified faction with the given ID.", AccountStatus::GMAdmin, command_viewplayerfaction) ||
		command_add("viewzoneloot", "[item id] - Allows you to search a zone's loot for a specific item ID. (0 shows all loot in the zone)", AccountStatus::QuestTroupe, command_viewzoneloot) ||
	
		command_add("wc", "[wear slot] [material] - Sends an OP_WearChange for your target.", AccountStatus::GMImpossible, command_wc) ||
		command_add("weather", "[0/1/2] (Off/Rain/Snow) [0/1] Serverwide [minutes] Duration - Change the weather.", AccountStatus::QuestMaster, command_weather) ||
		command_add("worldshutdown", "- Shut down world and all zones.", AccountStatus::GMImpossible, command_worldshutdown) ||
		command_add("wp", "[add/delete] [grid_num] [pause] [wp_num] [-h] - Add/delete a waypoint to/from a wandering grid.", AccountStatus::GMImpossible, command_wp) ||
		command_add("wpadd", "[pause] [-h] - Add your current location as a waypoint to your NPC target's AI path.", AccountStatus::GMImpossible, command_wpadd) ||
		command_add("wpinfo", "- Show waypoint info about your NPC target.", AccountStatus::GMImpossible, command_wpinfo) ||

		command_add("xpinfo", "- Show XP info about your current target.", AccountStatus::GMStaff, command_xpinfo) ||

		command_add("zclip", "[min] [max] - modifies and resends zhdr packet.", AccountStatus::GMImpossible, command_zclip) ||
		command_add("zcolor", "[red] [green] [blue] - Change sky color.", AccountStatus::GMImpossible, command_zcolor) ||
		command_add("zheader", "[zonename] - Load zheader for zonename from the database.", AccountStatus::GMImpossible, command_zheader) ||
		command_add("zone", "[Zone ID|Zone Short Name] [X] [Y] [Z] - Teleport to specified Zone by ID or Short Name (coordinates are optional).", AccountStatus::QuestTroupe, command_zone) ||
		command_add("zonebootup", "(shortname) (ZoneServerID) - Make a zone server boot a specific zone. If no arguments are given, it will find and boot any crashed zones.", AccountStatus::GMImpossible, command_zonebootup) ||
			command_add("zoneguild", "[Zone ID|Zone Short Name] [Guild ID] [X] [Y] [Z] - Teleport to specified Zone by ID or Short Name (coordinates are optional).", AccountStatus::QuestTroupe, command_zoneguild) ||
		command_add("zonelock", "[list/lock/unlock] - Set/query lock flag for zoneservers.", AccountStatus::GMAreas, command_zonelock) ||
		command_add("zoneshutdown", "[shortname] - Shut down a zone server.", AccountStatus::GMImpossible, command_zoneshutdown) ||
		command_add("zonespawn", "- Not implemented.", AccountStatus::Max, command_zonespawn) ||
		command_add("zonestatus", "- Show connected zoneservers, synonymous with /servers.", AccountStatus::GMStaff, command_zonestatus) ||
		command_add("zopp", "Troubleshooting command - Sends a fake item packet to you. No server reference is created.", AccountStatus::GMCoder, command_zopp) ||
		command_add("zsafecoords", "[x] [y] [z] - Set safe coords.", AccountStatus::GMImpossible, command_zsafecoords) ||
		command_add("zsave", " - Saves zheader to the database.", AccountStatus::GMImpossible, command_zsave) ||
		command_add("zsky", "[skytype] - Change zone sky type.", AccountStatus::GMImpossible, command_zsky) ||
		command_add("zstats", "- Show info about zone header.", AccountStatus::QuestTroupe, command_zstats) ||
		command_add("zunderworld", "[zcoord] - Sets the underworld using zcoord.", AccountStatus::GMImpossible, command_zunderworld) ||
		command_add("zuwcoords", "[z coord] - Set underworld coord.", AccountStatus::GMImpossible, command_zuwcoords)
		)
	{
		command_deinit();
		return -1;
	}

	std::map<std::string, std::pair<uint8, std::vector<std::string>>> command_settings;
	database.GetCommandSettings(command_settings);

	std::vector<std::pair<std::string, uint8>> injected_command_settings;
	std::vector<std::string> orphaned_command_settings;

	for (auto cs_iter : command_settings) {

		auto cl_iter = commandlist.find(cs_iter.first);
		if (cl_iter == commandlist.end()) {

			orphaned_command_settings.push_back(cs_iter.first);
			LogInfo(
				"Command [{}] no longer exists... Deleting orphaned entry from `command_settings` table...",
				cs_iter.first.c_str()
			);
		}
	}

	if (orphaned_command_settings.size()) {
		if (!database.UpdateOrphanedCommandSettings(orphaned_command_settings)) {
			LogInfo("Failed to process 'Orphaned Commands' update operation.");
		}
	}

	auto working_cl = commandlist;
	for (auto working_cl_iter : working_cl) {

		auto cs_iter = command_settings.find(working_cl_iter.first);
		if (cs_iter == command_settings.end()) {

			injected_command_settings.push_back(std::pair<std::string, uint8>(working_cl_iter.first, working_cl_iter.second->access));
			LogInfo(
				"New Command [{}] found... Adding to `command_settings` table with access [{}]...",
				working_cl_iter.first.c_str(),
				working_cl_iter.second->access
			);

			if (working_cl_iter.second->access == 0) {
				LogCommands(
					"command_init(): Warning: Command [{}] defaulting to access level 0!",
					working_cl_iter.first.c_str()
				);
			}

			continue;
		}

		working_cl_iter.second->access = cs_iter->second.first;
		LogCommands(
			"command_init(): - Command [{}] set to access level [{}]",
			working_cl_iter.first.c_str(),
			cs_iter->second.first
		);

		if (cs_iter->second.second.empty()) {
			continue;
		}

		for (auto alias_iter : cs_iter->second.second) {
			if (alias_iter.empty()) {
				continue;
			}

			if (commandlist.find(alias_iter) != commandlist.end()) {
				LogCommands(
					"command_init(): Warning: Alias [{}] already exists as a command - skipping!",
					alias_iter.c_str()
				);

				continue;
			}

			commandlist[alias_iter] = working_cl_iter.second;
			commandaliases[alias_iter] = working_cl_iter.first;

			LogCommands(
				"command_init(): - Alias [{}] added to command [{}]",
				alias_iter.c_str(),
				commandaliases[alias_iter].c_str()
			);
		}
	}

	if (injected_command_settings.size()) {
		if (!database.UpdateInjectedCommandSettings(injected_command_settings)) {
			LogInfo("Failed to process 'Injected Commands' update operation.");
		}
	}

	command_dispatch = command_realdispatch;

	return commandcount;
}

void command_deinit(void){
	/*
	* command_deinit
	* clears the command list, freeing resources
	*
	* Parameters:
	*	none
	*
	*/
	commandlist.clear();
	commandaliases.clear();

	command_dispatch = command_notavail;
	commandcount = 0;
}

int command_add(std::string command_name, const char *desc, int access, CmdFuncPtr function){
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
	if (command_name.empty()) {
		Log(Logs::General, Logs::Error, "command_add() - Command added with empty name string - check command.cpp.");
		return -1;
	}
	if (function == nullptr) {
		Log(Logs::General, Logs::Error, "command_add() - Command '%s' added without a valid function pointer - check command.cpp.", command_name.c_str());
		return -1;
	}
	if (commandlist.count(command_name) != 0) {
		Log(Logs::General, Logs::Error, "command_add() - Command '%s' is a duplicate command name - check command.cpp.", command_name.c_str());
		return -1;
	}
	for (auto iter = commandlist.begin(); iter != commandlist.end(); ++iter) {
		if (iter->second->function != function)
			continue;
		Log(Logs::General, Logs::Error, "command_add() - Command '%s' equates to an alias of '%s' - check command.cpp.", command_name.c_str(), iter->first.c_str());
		return -1;
	}

	auto c = new CommandRecord;
	c->access = access;
	c->desc = desc;
	c->function = function;

	commandlist[command_name] = c;
	commandaliases[command_name] = command_name;
	cleanup_commandlist.Append(c);
	commandcount++;

	return 0;
}

int command_realdispatch(Client *c, const char *message){
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
	Seperator sep(message, ' ', 10, 100, true); // "three word argument" should be considered 1 arg

	command_logcommand(c, message);

	std::string cstr(sep.arg[0] + 1);

	if (commandlist.count(cstr) != 1)
	{
		return(-2);
	}

	CommandRecord *cur = commandlist[cstr];
	if (c->Admin() < cur->access)
	{
		c->Message(CC_Red, "Your access level is not high enough to use this command.");
		return(-1);
	}

	if(cur->access >= COMMANDS_LOGGING_MIN_STATUS)
	{
		QServ->QSLogCommands(c, sep.arg[0], (char *)sep.argplus[1]);
	}

	if (cur->function == nullptr)
	{
		Log(Logs::General, Logs::Error, "Command '%s' has a null function\n", cstr.c_str());
		return(-1);
	}
	else
	{
		//dispatch C++ command
		cur->function(c, &sep);	// dispatch command
	}
	return 0;
}

void command_logcommand(Client *c, const char *message){
	int admin = c->Admin();

	bool continueevents = false;
	switch (zone->loglevelvar){ //catch failsafe
	case 9: { // log only LeadGM
		if ((admin >= AccountStatus::GMLeadAdmin) && (admin < AccountStatus::GMMgmt))
			continueevents = true;
		break;
	}
	case 8: { // log only GM
		if ((admin >= AccountStatus::GMAdmin) && (admin < AccountStatus::GMLeadAdmin))
			continueevents = true;
		break;
	}
	case 1: {
		if ((admin >= AccountStatus::GMMgmt))
			continueevents = true;
		break;
	}
	case 2: {
		if ((admin >= AccountStatus::GMLeadAdmin))
			continueevents = true;
		break;
	}
	case 3: {
		if ((admin >= AccountStatus::GMAdmin))
			continueevents = true;
		break;
	}
	case 4: {
		if ((admin >= AccountStatus::QuestTroupe))
			continueevents = true;
		break;
	}
	case 5: {
		if ((admin >= AccountStatus::ApprenticeGuide))
			continueevents = true;
		break;
	}
	case 6: {
		if ((admin >= AccountStatus::Steward))
			continueevents = true;
		break;
	}
	case 7: {
		continueevents = true;
		break;
	}
	}

	if (continueevents)
		database.logevents(
		c->AccountName(),
		c->AccountID(),
		admin, c->GetName(),
		c->GetTarget() ? c->GetTarget()->GetName() : "None",
		"Command",
		message,
		1
		);
}

// commands go below here

void command_resetaa(Client* c, const Seperator *sep){
	if (c->GetTarget() != 0 && c->GetTarget()->IsClient()){
		c->GetTarget()->CastToClient()->ResetAA();
		c->Message(CC_Red, "Successfully reset %s's AAs", c->GetTarget()->GetName());
	}
	else
		c->Message(CC_Default, "Usage: Target a client and use #resetaa to reset the AA data in their Profile.");
}

void command_resetboat(Client* c, const Seperator *sep){
	if (c->GetTarget() != 0 && c->GetTarget()->IsClient()){
		c->GetTarget()->CastToClient()->SetBoatID(0);
		c->GetTarget()->CastToClient()->SetBoatName("");
		c->Message(CC_Red, "Successfully removed %s from a boat in their PP.", c->GetTarget()->GetName());
	}
	else
		c->Message(CC_Default, "Usage: Target a client and use #resetboat to remove any boat in their Profile.");
}

void command_sendop(Client *c, const Seperator *sep){

	int RezSpell = 0;

	if (sep->arg[1][0]) {
		RezSpell = atoi(sep->arg[1]);
	}

	auto outapp = new EQApplicationPacket(OP_RezzRequest, sizeof(Resurrect_Struct));
	Resurrect_Struct *rs = (Resurrect_Struct*)outapp->pBuffer;

	strcpy(rs->your_name, c->GetName());
	strcpy(rs->rezzer_name, "A Cleric01");
	rs->spellid = RezSpell;
	DumpPacket(outapp);
	c->QueuePacket(outapp);
	safe_delete(outapp);
	return;
}

void command_optest(Client *c, const Seperator *sep){

	if(c)
	{
		int arg = atoi(sep->arg[1]);
		if (arg == 1)
		{
			auto app = new EQApplicationPacket(OP_FreezeClientControl, 65);
			strcpy((char *)app->pBuffer, c->GetName());
			c->QueuePacket(app);
			safe_delete(app);
		}
		else
		{
			auto app = new EQApplicationPacket(OP_UnfreezeClientControl, 65);
			strcpy((char *)app->pBuffer, c->GetName());
			c->QueuePacket(app);
			safe_delete(app);
		}
	}

}
void command_showfilters(Client *c, const Seperator *sep) {

	if (c)
	{
		for (int i = 0; i < 17; i++)
		{
			c->Message(CC_Yellow, "ServerFilter (%i) = %i", i, (int)c->GetFilter(static_cast<eqFilterType>(i)));
		}
	}
}

void command_help(Client *c, const Seperator *sep){
	int commands_shown = 0;

	c->Message(CC_Default, "Available EQEMu commands:");

	std::map<std::string, CommandRecord *>::iterator cur, end;
	cur = commandlist.begin();
	end = commandlist.end();

	for (; cur != end; ++cur) {
		if (sep->arg[1][0]) {
			if (cur->first.find(sep->arg[1]) == std::string::npos) {
				continue;
			}
		}

		if (c->Admin() < cur->second->access)
			continue;
		commands_shown++;
		c->Message(CC_Default, "	%c%s %s", COMMAND_CHAR, cur->first.c_str(), cur->second->desc == nullptr ? "" : cur->second->desc);
	}
	c->Message(CC_Default, "%d command%s listed.", commands_shown, commands_shown != 1 ? "s" : "");

}

void command_version(Client *c, const Seperator *sep){
	c->Message(CC_Default, "Current version information.");
	c->Message(CC_Default, "	%s", CURRENT_VERSION);
	c->Message(CC_Default, "	Compiled on: %s at %s", COMPILE_DATE, COMPILE_TIME);
	c->Message(CC_Default, "	Last modified on: %s", LAST_MODIFIED);
}

void command_setfaction(Client *c, const Seperator *sep){
	if ((sep->arg[1][0] == 0 || strcasecmp(sep->arg[1], "*") == 0) || ((c->GetTarget() == 0) || (c->GetTarget()->IsClient()))) {
		c->Message(CC_Default, "Usage: #setfaction [faction number]");
		return;
	}
	
	uint32 faction_id = atoi(sep->argplus[1]);
	auto npcTypeID = c->GetTarget()->CastToNPC()->GetNPCTypeID();
	c->Message(CC_Yellow, "Setting NPC %u to faction %i", npcTypeID, faction_id);
	
	std::string query = StringFormat("UPDATE npc_types SET npc_faction_id = %i WHERE id = %i",
		faction_id, npcTypeID);
	database.QueryDatabase(query);
}

void command_giveplayerfaction(Client *c, const Seperator *sep)
{
	if (!sep->IsNumber(1) || atoi(sep->arg[2]) == 0)
	{
		c->Message(CC_Default, "Usage: #giveplayerfaction [factionid] [value]");
		return;
	}

	uint32 factionid = atoi(sep->arg[1]);
	int32 value = atoi(sep->arg[2]);
	Client* t;
	if (c->GetTarget() && c->GetTarget()->IsClient())
	{
		t = c->GetTarget()->CastToClient();
	}
	else
	{
		t = c;
	}

	char name[50];
	if (database.GetFactionName(factionid, name, sizeof(name)) == false)
		snprintf(name, sizeof(name), "Faction%i", factionid);

	t->SetFactionLevel2(t->CharacterID(), factionid, value, 0);
	c->Message(CC_Default, "%s was given %i points with %s.", t->GetName(), value, name);
}

void command_viewplayerfaction(Client *c, const Seperator *sep)
{
	NPC* npc = nullptr;
	uint32 factionid = 0;
	if (c->GetTarget() && c->GetTarget()->IsNPC())
	{
		npc = c->GetTarget()->CastToNPC();

		if (npc)
		{
			factionid = npc->GetPrimaryFaction();
		}
		else
		{
			if (!sep->IsNumber(1))
			{
				c->Message(CC_Red, "Invalid target.");
				return;
			}
		}
	}
	else if (!sep->IsNumber(1))
	{
		c->Message(CC_Default, "Usage: #viewplayerfaction [factionid 1-5999]");
		return;
	}

	if (sep->IsNumber(1) && npc == nullptr)
	{
		if (atoi(sep->arg[1]) > 0 && atoi(sep->arg[1]) < 6000)
			factionid = atoi(sep->arg[1]);
		else
		{
			c->Message(CC_Default, "Usage: #viewplayerfaction [factionid 1-5999]");
			return;
		}
	}

	Client* t;
	if (c->GetTarget() && c->GetTarget()->IsClient())
	{
		t = c->GetTarget()->CastToClient();
	}
	else
	{
		t = c;
	}

	char name[50];
	if (database.GetFactionName(factionid, name, sizeof(name)) == false)
		snprintf(name, sizeof(name), "Faction%i", factionid);

	int32 personal = t->GetCharacterFactionLevel(factionid);
	int32 modified = t->GetModCharacterFactionLevel(factionid, true);
	int32 illusioned = t->GetModCharacterFactionLevel(factionid, false);
	int16 min_cap = database.MinFactionCap(factionid);
	int16 max_cap = database.MaxFactionCap(factionid);
	int16 modified_min_cap = min_cap + (modified - personal);
	int16 modified_max_cap = max_cap + (modified - personal);
	
	c->Message(CC_Default, "%s has %d personal and %d modified faction with '%s' (%d)  personal cap: %d to %d; modified cap: %d to %d",
		t->GetName(), personal, modified, name, factionid, min_cap, max_cap, modified_min_cap, modified_max_cap);
	if (illusioned != modified)
		c->Message(CC_Default, "Illusion is active and fooling this faction.  Illusioned faction value is %d", illusioned);

	if (npc != nullptr)
	{
		modified = t->GetFactionValue(npc);
		c->Message(CC_Default, "Effective faction for '%s' is %d and includes extra modifiers such as invis and aggro", npc->GetName(), modified);
	}
}

void command_serversidename(Client *c, const Seperator *sep){
	if (c->GetTarget())
		c->Message(CC_Default, c->GetTarget()->GetName());
	else
		c->Message(CC_Default, "Error: no target");
}

void command_testcopy(Client *c, const Seperator *sep)
{
	if (c->GetTarget() == nullptr)
	{
		c->Message(CC_Red, "You must target a player.");
	}
	else if (!c->GetTarget()->IsClient())
	{
		c->Message(CC_Red, "You can only save backups of players.");
	}
	else if (c->GetTarget() && c->GetTarget()->IsClient())
	{
		Client* client = c->GetTarget()->CastToClient();
		int targetLSID = client->LSAccountID();
		int targetID = client->CharacterID();
		auto targetAcctID = database.GetAccountIDByChar(targetID);
		const char* targetName = client->GetName();
		c->Message(CC_Default, "Backing up targetName: %s, targetLSID: %i, targetID: %i, targetAcctID: %i", targetName, targetLSID, targetAcctID, targetID);

		char String[255];

#ifdef _WINDOWS
		sprintf(String, "tak_testcopy.bat %s %i %i %i", targetName, targetLSID, targetAcctID, targetID);
#else
		sprintf(String, "./tak_testcopy %s %i %i %i", targetName, targetLSID, targetAcctID, targetID);
#endif
		int system_var = system(String);
	}
}

void command_testspawnkill(Client *c, const Seperator *sep){
	/*	auto outapp = new EQApplicationPacket(OP_Death, sizeof(Death_Struct));
	Death_Struct* d = (Death_Struct*)outapp->pBuffer;
	d->corpseid = 1000;
	//	d->unknown011 = 0x05;
	d->spawn_id = 1000;
	d->killer_id = c->GetID();
	d->damage = 1;
	d->spell_id = 0;
	d->type = BASH;
	d->bindzoneid = 0;
	c->FastQueuePacket(&outapp);*/
}

void command_testspawn(Client *c, const Seperator *sep){
	if (sep->IsNumber(1)) {
		auto outapp = new EQApplicationPacket(OP_NewSpawn, sizeof(NewSpawn_Struct));
		NewSpawn_Struct* ns = (NewSpawn_Struct*)outapp->pBuffer;
		c->FillSpawnStruct(ns, c);
		strcpy(ns->spawn.name, "Test");
		ns->spawn.spawnId = 1000;
		ns->spawn.NPC = 1;
		if (sep->IsHexNumber(2)) {
			if (strlen(sep->arg[2]) >= 3) // 0x00, 1 byte
				*(&((uint8*)&ns->spawn)[atoi(sep->arg[1])]) = hextoi(sep->arg[2]);
			else if (strlen(sep->arg[2]) >= 5) // 0x0000, 2 bytes
				*((uint16*)&(((uint8*)&ns->spawn)[atoi(sep->arg[1])])) = hextoi(sep->arg[2]);
			else if (strlen(sep->arg[2]) >= 9) // 0x0000, 2 bytes
				*((uint32*)&(((uint8*)&ns->spawn)[atoi(sep->arg[1])])) = hextoi(sep->arg[2]);
			else
				c->Message(CC_Default, "Error: unexpected hex string length");
		}
		else {
			strcpy((char*)(&((uint8*)&ns->spawn)[atoi(sep->arg[1])]), sep->argplus[2]);
		}
		EncryptSpawnPacket(outapp);
		c->FastQueuePacket(&outapp);
	}
	else
		c->Message(CC_Default, "Usage: #testspawn [memloc] [value] - spawns a NPC for you only, with the specified values set in the spawn struct");
}

void command_wc(Client *c, const Seperator *sep){
	if (sep->argnum < 2)
	{
		c->Message(CC_Default, "Usage: #wc slot material [color] [unknown06]");
	}
	else if (c->GetTarget() == nullptr) {
		c->Message(CC_Red, "You must have a target to do a wear change.");
	}
	else
	{
		uint8 wearslot = atoi(sep->arg[1]);
		uint16 texture = atoi(sep->arg[2]);
		uint32 color = 0;
		
		if (sep->argnum > 2)
		{
			color = atoi(sep->arg[3]);
		}

		c->GetTarget()->WearChange(wearslot, texture, color);
	}
}

void command_numauths(Client *c, const Seperator *sep){
	c->Message(CC_Default, "NumAuths: %i", zone->CountAuth());
	c->Message(CC_Default, "Your WID: %i", c->GetWID());
}

void command_setanim(Client *c, const Seperator *sep){
	if (c->GetTarget() && sep->IsNumber(1)) {
		int num = atoi(sep->arg[1]);
		if (num < 0 || num >= _eaMaxAppearance) {
			c->Message(CC_Default, "Invalid animation number, between 0 and %d", _eaMaxAppearance - 1);
		}
		c->GetTarget()->SetAppearance(EmuAppearance(num));
	}
	else
		c->Message(CC_Default, "Usage: #setanim [animnum]");
}

void command_connectworldserver(Client *c, const Seperator *sep){
	if (worldserver.Connected())
		c->Message(CC_Default, "Error: Already connected to world server");
	else
	{
		c->Message(CC_Default, "Attempting to connect to world server...");
		worldserver.AsyncConnect();
	}
}

void command_getvariable(Client *c, const Seperator *sep){
	std::string tmp;
	if (database.GetVariable(sep->argplus[1], tmp))
		c->Message(CC_Default, "%s = %s",  sep->argplus[1], tmp.c_str());
	else
		c->Message(CC_Default, "GetVariable(%s) returned false", sep->argplus[1]);
}

void command_chat(Client *c, const Seperator *sep){
	if (sep->arg[2][0] == 0)
		c->Message(CC_Default, "Usage: #chat [channum] [message]");
	else
		if (!worldserver.SendChannelMessage(0, 0, (uint8)atoi(sep->arg[1]), 0, 0, 100, sep->argplus[2]))
			c->Message(CC_Default, "Error: World server disconnected");
}

void command_showpetspell(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #ShowPetSpells [spellid | searchstring]");
	else if (SPDAT_RECORDS <= 0)
		c->Message(CC_Default, "Spells not loaded");
	else if (Seperator::IsNumber(sep->argplus[1]))
	{
		int spellid = atoi(sep->argplus[1]);
		if (spellid <= 0 || spellid >= SPDAT_RECORDS)
			c->Message(CC_Default, "Error: Number out of range");
		else
			c->Message(CC_Default, "	%i: %s, %s", spellid, spells[spellid].teleport_zone, spells[spellid].name);
	}
	else
	{
		int count = 0;
		std::string sName;
		std::string sCriteria;
		sCriteria = sep->argplus[1];
		for (auto & c : sCriteria) c = toupper(c);
		for (int i = 0; i < SPDAT_RECORDS; i++)
		{
			if (spells[i].name[0] != 0 && (spells[i].effectid[0] == SE_SummonPet || spells[i].effectid[0] == SE_NecPet))
			{
				sName = spells[i].teleport_zone;
				for (auto & c : sName) c = toupper(c);
				if (sName.find(sCriteria) != std::string::npos && (count <= 20))
				{
					c->Message(CC_Default, "	%i: %s, %s", i, spells[i].teleport_zone, spells[i].name);
					count++;
				}
				else if (count > 20)
					break;
			}
		}
		if (count > 20)
			c->Message(CC_Default, "20 spells found... max reached.");
		else
			c->Message(CC_Default, "%i spells found.", count);
	}
}

#ifdef IPC
void command_ipc(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetTarget()->IsNPC())
	{
		if (c->GetTarget()->CastToNPC()->IsInteractive())
		{
			c->GetTarget()->CastToNPC()->interactive = false;
			c->Message(CC_Default, "Disabling IPC");
		}
		else
		{
			c->GetTarget()->CastToNPC()->interactive = true;
			c->Message(CC_Default, "Enabling IPC");
		}
	}
	else
		c->Message(CC_Default, "Error: You must target an NPC");
}
#endif /* IPC */

void command_npcloot(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0)
		c->Message(CC_Default, "Error: No target");
	// #npcloot show
	else if (strcasecmp(sep->arg[1], "show") == 0)
	{
		if (c->GetTarget()->IsNPC())
			c->GetTarget()->CastToNPC()->QueryLoot(c);
		else if (c->GetTarget()->IsCorpse())
			c->GetTarget()->CastToCorpse()->QueryLoot(c);
	}
	// These 2 types are *BAD* for the next few commands
	else if (c->GetTarget()->IsClient() || c->GetTarget()->IsCorpse())
		c->Message(CC_Default, "Error: Invalid target type, try a NPC =).");
	// #npcloot add
	else if (strcasecmp(sep->arg[1], "add") == 0)
	{
		// #npcloot add item
		if (c->GetTarget()->IsNPC() && sep->IsNumber(2))
		{
			uint32 item = atoi(sep->arg[2]);
			const EQ::ItemData* dbitem = database.GetItem(item);
			if (dbitem)
			{
				bool quest = false;
				if (sep->arg[4][0] != 0 && sep->IsNumber(4))
					quest = atoi(sep->arg[4]);

				if (sep->arg[3][0] != 0 && sep->IsNumber(3))
					c->GetTarget()->CastToNPC()->AddItem(item, atoi(sep->arg[3]), !quest, quest);
				else
				{
					if(database.ItemQuantityType(item) == EQ::item::Quantity_Charges)
					{
						int8 charges = dbitem->MaxCharges;
						c->GetTarget()->CastToNPC()->AddItem(item, charges, !quest, quest);
					}
					else
						c->GetTarget()->CastToNPC()->AddItem(item, 1, !quest, quest);
				}
				c->Message(CC_Default, "Added item(%i) to %s's loot.", item, c->GetTarget()->GetCleanName());
			}
			else
				c->Message(CC_Default, "Error: #npcloot add: Item(%i) does not exist!", item);
		}
		else if (!sep->IsNumber(2))
			c->Message(CC_Default, "Error: #npcloot add: Itemid must be a number.");
		else
			c->Message(CC_Default, "Error: #npcloot add: This is not a valid target.");
	}
	// #npcloot remove
	else if (strcasecmp(sep->arg[1], "remove") == 0)
	{
		//#npcloot remove all
		if (strcasecmp(sep->arg[2], "all") == 0)
		{
			c->GetTarget()->CastToNPC()->ClearItemList();
			c->GetTarget()->CastToNPC()->RemoveCash();
			c->Message(CC_Default, "Removed all loot from %s.", c->GetTarget()->GetCleanName());
		}
		//#npcloot remove itemid
		else
		{
			if (c->GetTarget()->IsNPC() && sep->IsNumber(2))
			{
				NPC* npc = c->GetTarget()->CastToNPC();
				uint32 item = atoi(sep->arg[2]);

				if (npc)
				{
					ServerLootItem_Struct* sitem = npc->GetItemByID(item);
					npc->RemoveItem(sitem);
					c->Message(CC_Default, "Removed item(%i) from %s's loot.", item, npc->GetCleanName());
				}
			}
			else if (!sep->IsNumber(2))
				c->Message(CC_Default, "Error: #npcloot remove: Item must be a number.");
			else
				c->Message(CC_Default, "Error: #npcloot remove: This is not a valid target.");
		}
	}
	// #npcloot money
	else if (strcasecmp(sep->arg[1], "money") == 0)
	{
		if (c->GetTarget()->IsNPC() && sep->IsNumber(2) && sep->IsNumber(3) && sep->IsNumber(4) && sep->IsNumber(5))
		{
			if ((atoi(sep->arg[2]) < 34465 && atoi(sep->arg[2]) >= 0) && (atoi(sep->arg[3]) < 34465 && atoi(sep->arg[3]) >= 0) && (atoi(sep->arg[4]) < 34465 && atoi(sep->arg[4]) >= 0) && (atoi(sep->arg[5]) < 34465 && atoi(sep->arg[5]) >= 0))
			{
				c->GetTarget()->CastToNPC()->AddCash(atoi(sep->arg[5]), atoi(sep->arg[4]), atoi(sep->arg[3]), atoi(sep->arg[2]));
				c->Message(CC_Default, "Set %i Platinum, %i Gold, %i Silver, and %i Copper as %s's money.", atoi(sep->arg[2]), atoi(sep->arg[3]), atoi(sep->arg[4]), atoi(sep->arg[5]), c->GetTarget()->GetName());
			}
			else
				c->Message(CC_Default, "Error: #npcloot money: Values must be between 0-34465.");
		}
		else
			c->Message(CC_Default, "Usage: #npcloot money platinum gold silver copper");
	}
	else
		c->Message(CC_Default, "Usage: #npcloot [show/money/add/remove] [itemid/all/money: pp gp sp cp] [quantity] [quest]");
}

void command_gm(Client *c, const Seperator *sep){
	bool state = atobool(sep->arg[1]);
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->arg[1][0] != 0) {
		t->SetGM(state);
		c->Message(CC_Default, "%s is %s a GM.", t->GetName(), state ? "now" : "no longer");
	}
	else
		c->Message(CC_Default, "Usage: #gm [on/off]");
}

void command_summon(Client *c, const Seperator *sep){
	Mob *t;

	if (sep->arg[1][0] != 0)		// arg specified
	{
		Client* client = entity_list.GetClientByName(sep->arg[1]);
		if (client != 0)	// found player in zone
			t = client->CastToMob();
		else
		{
			if (!worldserver.Connected())
				c->Message(CC_Default, "Error: World server disconnected.");
			else
			{ // player is in another zone
				//Taking this command out until we test the factor of 8 in ServerOP_ZonePlayer
				//c->Message(CC_Default, "Summoning player from another zone not yet implemented.");
				//return;

				auto pack = new ServerPacket(ServerOP_ZonePlayer, sizeof(ServerZonePlayer_Struct));
				ServerZonePlayer_Struct* szp = (ServerZonePlayer_Struct*)pack->pBuffer;
				strcpy(szp->adminname, c->GetName());
				szp->adminrank = c->Admin();
				szp->ignorerestrictions = 2;
				strcpy(szp->name, sep->arg[1]);
				strcpy(szp->zone, zone->GetShortName());
				szp->x_pos = c->GetX(); // May need to add a factor of 8 in here..
				szp->y_pos = c->GetY();
				szp->z_pos = c->GetZ();
				szp->zoneguildid = zone->GetGuildID();
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
			return;
		}
	}
	else if (c->GetTarget())		// have target
		t = c->GetTarget();
	else
	{
		/*if(c->Admin() < 150)
		c->Message(CC_Default, "You need a NPC/corpse target for this command");
		else*/
		c->Message(CC_Default, "Usage: #summon [charname] Either target or charname is required");
		return;
	}

	if (!t)
		return;

	if (t->IsNPC())
	{ // npc target
		c->Message(CC_Default, "Summoning NPC %s to %1.1f, %1.1f, %1.1f", t->GetName(), c->GetX(), c->GetY(), c->GetZ());
		t->CastToNPC()->GMMove(c->GetX(), c->GetY(), c->GetZ(), c->GetHeading());
		t->CastToNPC()->SaveGuardSpot();
	}
	else if (t->IsCorpse())
	{ // corpse target
		c->Message(CC_Default, "Summoning corpse %s to %1.1f, %1.1f, %1.1f", t->GetName(), c->GetX(), c->GetY(), c->GetZ());
		t->CastToCorpse()->GMMove(c->GetX(), c->GetY(), c->GetZ(), c->GetHeading());
	}
	else if (t->IsClient())
	{
		//If #hideme is on, prevent being summoned by a lower GM.
		if(t->CastToClient()->GetAnon() == 1 && t->CastToClient()->Admin() > c->Admin())
		{
			c->Message(CC_Red, "You cannot summon a GM with a higher status than you.");
			return;
		}

		c->Message(CC_Default, "Summoning player %s to %1.1f, %1.1f, %1.1f", t->GetName(), c->GetX(), c->GetY(), c->GetZ());
		t->CastToClient()->MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), c->GetX(), c->GetY(), c->GetZ(), c->GetHeading(), 2, GMSummon);
	}
}

void command_zone(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #zone [Zone ID|Zone Short Name] [X] [Y] [Z]");
		return;
	}

	const char* zone_identifier = sep->arg[1];

	if (Strings::IsNumber(zone_identifier) && !strcmp(zone_identifier, "0")) {
		c->Message(CC_Default, "Sending you to the safe coordinates of this zone.");

		c->MovePC(
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			0,
			ZoneToSafeCoords
		);
		return;
	}

	auto zone_id = (
		sep->IsNumber(1) ?
		std::stoul(zone_identifier) :
		database.GetZoneID(zone_identifier)
		);
	auto zone_short_name = database.GetZoneName(zone_id);
	if (!zone_id || !zone_short_name) {
		c->Message(
			CC_Default,
			fmt::format(
				"No zones were found matching '{}'.",
				zone_identifier
			).c_str()
		);
		return;
	}

	auto min_status = database.GetMinStatus(zone_id);
	if (c->Admin() < min_status) {
		c->Message(CC_Default, "Your status is not high enough to go to this zone.");
		return;
	}

	auto x = sep->IsNumber(2) ? std::stof(sep->arg[2]) : 0.0f;
	auto y = sep->IsNumber(3) ? std::stof(sep->arg[3]) : 0.0f;
	auto z = sep->IsNumber(4) ? std::stof(sep->arg[4]) : 0.0f;
	auto zone_mode = sep->IsNumber(2) ? ZoneSolicited : ZoneToSafeCoords;

	c->MovePC(
		zone_id,
		x,
		y,
		z,
		0.0f,
		0,
		zone_mode
	);
}

void command_zoneguild(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #zoneguild [Zone ID|Zone Short Name] [Guild Id] [X] [Y] [Z]");
		return;
	}

	const char* zone_identifier = sep->arg[1];

	if (Strings::IsNumber(zone_identifier) && !strcmp(zone_identifier, "0")) {
		c->Message(CC_Default, "Sending you to the safe coordinates of this zone.");

		c->MovePC(
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			0,
			ZoneToSafeCoords
		);
		return;
	}

	auto zone_id = (
		sep->IsNumber(1) ?
		std::stoul(zone_identifier) :
		database.GetZoneID(zone_identifier)
		);
	auto zone_short_name = database.GetZoneName(zone_id);
	if (!zone_id || !zone_short_name) {
		c->Message(
			CC_Default,
			fmt::format(
				"No zones were found matching '{}'.",
				zone_identifier
			).c_str()
		);
		return;
	}

	auto min_status = database.GetMinStatus(zone_id);
	if (c->Admin() < min_status) {
		c->Message(CC_Default, "Your status is not high enough to go to this zone.");
		return;
	}


	const char* guild_identifier = sep->arg[2];

	uint32 guild_id = (uint32)(
		sep->IsNumber(2) ?
		atol(guild_identifier) :
		GUILD_NONE
		);

	if (guild_id == 0)
		guild_id = GUILD_NONE;

	auto x = sep->IsNumber(3) ? std::stof(sep->arg[3]) : 0.0f;
	auto y = sep->IsNumber(4) ? std::stof(sep->arg[4]) : 0.0f;
	auto z = sep->IsNumber(5) ? std::stof(sep->arg[5]) : 0.0f;
	auto zone_mode = sep->IsNumber(3) ? ZoneSolicited : ZoneToSafeCoords;

	c->MovePCGuildID(
		zone_id,
		guild_id,
		x,
		y,
		z,
		0.0f,
		0,
		zone_mode
	);
}

void command_showbuffs(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0)
		c->CastToMob()->ShowBuffs(c);
	else
		c->GetTarget()->CastToMob()->ShowBuffs(c);
}

void command_peqzone(Client *c, const Seperator *sep){
	uint32 timeleft = c->GetPTimers().GetRemainingTime(pTimerPeqzoneReuse) / 60;

	if (!c->GetPTimers().Expired(&database, pTimerPeqzoneReuse, false)) {
		c->Message(CC_Red, "You must wait %i minute(s) before using this ability again.", timeleft);
		return;
	}

	if (c->GetHPRatio() < 75.0f) {
		c->Message(CC_Default, "You cannot use this command with less than 75 percent health.");
		return;
	}

	//this isnt perfect, but its better...
	if (
		c->IsInvisible(c)
		|| c->IsRooted()
		|| c->IsStunned()
		|| c->IsMezzed()
		|| c->AutoAttackEnabled()
		|| c->GetInvul()
		) {
		c->Message(CC_Default, "You cannot use this command in your current state. Settle down and wait.");
		return;
	}

	uint16 zoneid = 0;
	uint8 destzone = 0;
	if (sep->IsNumber(1))
	{
		zoneid = atoi(sep->arg[1]);
		destzone = database.GetPEQZone(zoneid);
		if (destzone == 0){
			c->Message(CC_Red, "You cannot use this command to enter that zone!");
			return;
		}
		if (zoneid == zone->GetZoneID()) {
			c->Message(CC_Red, "You cannot use this command on the zone you are in!");
			return;
		}
	}
	else if (sep->arg[1][0] == 0 || sep->IsNumber(2) || sep->IsNumber(3) || sep->IsNumber(4) || sep->IsNumber(5))
	{
		c->Message(CC_Default, "Usage: #peqzone [zonename]");
		c->Message(CC_Default, "Optional Usage: #peqzone [zoneid]");
		return;
	}
	else {
		zoneid = database.GetZoneID(sep->arg[1]);
		destzone = database.GetPEQZone(zoneid);
		if (zoneid == 0) {
			c->Message(CC_Default, "Unable to locate zone '%s'", sep->arg[1]);
			return;
		}
		if (destzone == 0){
			c->Message(CC_Red, "You cannot use this command to enter that zone!");
			return;
		}
		if (zoneid == zone->GetZoneID()) {
			c->Message(CC_Red, "You cannot use this command on the zone you are in!");
			return;
		}
	}

	if (RuleB(Zone, UsePEQZoneDebuffs)){
		c->SpellOnTarget(RuleI(Zone, PEQZoneDebuff1), c);
		c->SpellOnTarget(RuleI(Zone, PEQZoneDebuff2), c);
	}

	//zone to safe coords
	c->GetPTimers().Start(pTimerPeqzoneReuse, RuleI(Zone, PEQZoneReuseTime));
	c->MovePC(zoneid, 0.0f, 0.0f, 0.0f, 0.0f, 0, ZoneToSafeCoords);
}

void command_movechar(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0 || sep->arg[2][0] == 0)
		c->Message(CC_Default, "Usage: #movechar [charactername] [zonename]/bind");
	else if (c->Admin() < commandMovecharToSpecials && strcasecmp(sep->arg[2], "cshome") == 0 || strcasecmp(sep->arg[2], "load") == 0 || strcasecmp(sep->arg[2], "load2") == 0)
		c->Message(CC_Default, "Invalid zone name");
	else
	{
		uint32 tmp = database.GetAccountIDByChar(sep->arg[1]);
		if (tmp)
		{
			if (c->Admin() >= commandMovecharSelfOnly || tmp == c->AccountID())
			{
				if (strcasecmp(sep->arg[2], "bind") == 0)
				{
					uint32 charid = database.GetCharacterID((char*)sep->arg[1]);
					uint16 zone_id = database.MoveCharacterToBind(charid);
					if(zone_id == 0)
						c->Message(CC_Default, "Character Move Failed!");
					else
						c->Message(CC_Default, "%s has been moved to %s.", (char*)sep->arg[1], database.GetZoneName(zone_id));
				}
				else if (!database.MoveCharacterToZone((char*)sep->arg[1], (char*)sep->arg[2]))
					c->Message(CC_Default, "Character Move Failed!");
				else
					c->Message(CC_Default, "%s has been moved to %s.", (char*)sep->arg[1], (char*)sep->arg[2]);
			}
			else
				c->Message(CC_Red, "You cannot move characters that are not on your account.");
		}
		else
			c->Message(CC_Default, "Character Does Not Exist");
	}
}

void command_bug(Client *c, const Seperator *sep)
{
	int admin = c->Admin();
	std::string help0 = "Bug commands usage:";
	std::string help1 = "  #bug list - Lists all bugs.";
	std::string help2 = "  #bug view - #bug view (bug number).";
	std::string help3 = "  #bug delete - #bug delete. (bug number).";

	std::string help[] = { help0, help1, help2, help3 };

	if (strcasecmp(sep->arg[1], "help") == 0)
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
	else if (strcasecmp(sep->arg[1], "list") == 0)
	{
		std::string query = "SELECT id, name, zone, date FROM bugs ORDER BY id";
		auto results = database.QueryDatabase(query);

		if (!results.Success() || results.RowCount() == 0)
		{
			c->Message(CC_Red, "There was an error in your request: No petitions found!");
			return;
		}

		c->Message(CC_Red, "ID : Name , Zone , Time Sent");
		for (auto row = results.begin(); row != results.end(); ++row)
		{

			c->Message(CC_Yellow, " %s:	%s , %s , %s", row[0], row[1], row[2], row[3]);
		}
	}
	else if (strcasecmp(sep->arg[1], "view") == 0)
	{
		if (sep->arg[2][0] == 0)
		{
			c->Message(CC_Default, "Usage: #bug view (bug number) Type #bug list for a list");
			return;
		}
		Log(Logs::Detail, Logs::Normal, "Bug viewed by %s, bug number:", c->GetName(), atoi(sep->arg[2]));
		c->Message(CC_Red, "ID : Name , Zone , x , y , z , Type , Flag , Target , Status , Client, Time Sent , Bug");
		std::string query = StringFormat("SELECT id, name, zone, x, y, z, type, flag, target, status, ui, date, bug FROM bugs WHERE id = %i", atoi(sep->arg[2]));
		auto results = database.QueryDatabase(query);

		if (!results.Success() || results.RowCount() == 0)
		{
			c->Message(CC_Red, "There was an error in your request: ID not found! Please check the Id and try again.");
			return;
		}

		auto row = results.begin();

		c->Message(CC_Yellow, " %s: %s , %s , %s , %s , %s , %s , %s , %s , %s , %s , %s", row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10], row[11]);
		c->Message(CC_Yellow, " %s ", row[12]);
	}
	else if (strcasecmp(sep->arg[1], "delete") == 0)
	{
		if (admin >= 90)
		{
			if (sep->arg[1][0] == 0 || sep->arg[2][0] == 0 || strcasecmp(sep->arg[2], "*") == 0)
			{
				c->Message(CC_Default, "Usage: #bug delete (bug number) Type #bug list for a list");
				return;
			}

			c->Message(CC_Red, "Attempting to delete bug number: %i", atoi(sep->arg[2]));
			std::string query = StringFormat("DELETE FROM bugs WHERE id = %i", atoi(sep->arg[2]));
			auto results = database.QueryDatabase(query);
			if (!results.Success())
				return;

			petition_list.ReadDatabase();
			Log(Logs::Detail, Logs::Normal, "Delete bug request from %s, bug number:", c->GetName(), atoi(sep->arg[2]));
		}
		else
			c->Message(CC_Default, "Your access level is not high enough to use this command.");
	}
	else
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
}

void command_petition(Client *c, const Seperator *sep)
{
	int admin = c->Admin();
	std::string help0 = "Petition commands usage:";
	std::string help1 = "  #petition list - Lists all petitions.";
	std::string help2 = "  #petition view - #petition view (petition number).";
	std::string help3 = "  #petition info - #petition info (petition number).";
	std::string help4 = "  #petition update - #petition update (petition number) (Text).";
	std::string help5 = "      Adds GM comment Make sure you contain the comments in quotes.";
	std::string help6 = "  #petition delete - #petition delete. (petition number).";

	std::string help[] = { help0, help1, help2, help3, help4, help5, help6 };

	if (strcasecmp(sep->arg[1], "help") == 0)
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
	else if (strcasecmp(sep->arg[1], "list") == 0)
	{
		std::string query = "SELECT petid, charname, accountname, senttime FROM petitions ORDER BY petid";
		auto results = database.QueryDatabase(query);

		if (!results.Success() || results.RowCount() == 0)
		{
			c->Message(CC_Red, "There was an error in your request: No petitions found!");
			return;
		}

		c->Message(CC_Red, "ID : Character , Account , Time Sent");
		for (auto row = results.begin(); row != results.end(); ++row)
		{
			char *pet_time = row[3];
			time_t t = atoi(pet_time);
			struct tm *tm = localtime(&t);
			char date[80];
			strftime(date, 80, "%m/%d %I:%M%p", tm);

			c->Message(CC_Yellow, " %s:	%s , %s , %s", row[0], row[1], row[2], date);
		}
	}
	else if (strcasecmp(sep->arg[1], "view") == 0)
	{
		if (sep->arg[2][0] == 0)
		{
			c->Message(CC_Default, "Usage: #petition view (petition number) Type #petition list for a list");
			return;
		}
		Log(Logs::Detail, Logs::Normal, "Petition viewed by %s, petition number:", c->GetName(), atoi(sep->arg[2]));
		c->Message(CC_Red, "ID : Character , Account , Petition Text");
		std::string query = StringFormat("SELECT petid, charname, accountname, petitiontext FROM petitions WHERE petid = %i", atoi(sep->arg[2]));
		auto results = database.QueryDatabase(query);

		if (!results.Success() || results.RowCount() == 0)
		{
			c->Message(CC_Red, "There was an error in your request: ID not found! Please check the Id and try again.");
			return;
		}

		auto row = results.begin();
		c->Message(CC_Yellow, " %s: %s , %s , %s", row[0], row[1], row[2], row[3]);
	}
	else if (strcasecmp(sep->arg[1], "info") == 0)
	{
		if (sep->arg[2][0] == 0)
		{
			c->Message(CC_Default, "Usage: #petition info (petition number) Type #petition list for a list");
			return;
		}

		Log(Logs::General, Logs::Normal, "Petition information request from %s, petition number:", c->GetName(), atoi(sep->arg[2]));
		c->Message(CC_Red, "ID : Character , Account , Zone , Class , Race , Level , Last GM , GM Comment");
		std::string query = StringFormat("SELECT petid, charname, accountname, zone, charclass, charrace, charlevel, lastgm, gmtext FROM petitions WHERE petid = %i", atoi(sep->arg[2]));
		auto results = database.QueryDatabase(query);

		if (!results.Success() || results.RowCount() == 0)
		{
			c->Message(CC_Red, "There was an error in your request: ID not found! Please check the Id and try again.");
			return;
		}

		auto row = results.begin();
		c->Message(CC_Yellow, "%s: %s %s %s %s %s %s %s %s", row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
	}
	else if (strcasecmp(sep->arg[1], "update") == 0)
	{
		if (admin >= 90)
		{
			if (sep->arg[2][0] == 0 || sep->arg[3][0] == 0)
			{
				c->Message(CC_Default, "Usage: #petition update (petition number) (Text) Make sure you contain the comments in quotes. Type #petition list for a list");
				return;
			}

			Log(Logs::Detail, Logs::Normal, "Petition update request from %s, petition number:", c->GetName(), atoi(sep->arg[2]));
			std::string query = StringFormat("UPDATE `petitions` SET `lastgm` = '%s', `gmtext` = '%s' WHERE `petid` = %i;", c->GetName(), sep->arg[3], atoi(sep->arg[2]));
			auto results = database.QueryDatabase(query);

			if (!results.Success())
			{
				c->Message(CC_Red, "There was an error in your request: ID not found! Please check the Id and try again.");
				return;
			}

			c->Message(CC_Yellow, "%s, Updated petition comment to ( %s ) for petition: %i", c->GetName(), sep->arg[3], atoi(sep->arg[2]));
		}
		else
			c->Message(CC_Default, "Your access level is not high enough to use this command.");
	}
	else if (strcasecmp(sep->arg[1], "delete") == 0)
	{
		if (admin >= 90)
		{
			if (sep->arg[1][0] == 0 || sep->arg[2][0] == 0 || strcasecmp(sep->arg[2], "*") == 0)
			{
				c->Message(CC_Default, "Usage: #petition delete (petition number) Type #petition list for a list");
				return;
			}

			c->Message(CC_Red, "Attempting to delete petition number: %i", atoi(sep->arg[2]));
			std::string query = StringFormat("DELETE FROM petitions WHERE petid = %i", atoi(sep->arg[2]));
			auto results = database.QueryDatabase(query);
			if (!results.Success())
				return;

			petition_list.ReadDatabase();
			Log(Logs::Detail, Logs::Normal, "Delete petition request from %s, petition number:", c->GetName(), atoi(sep->arg[2]));
		}
		else
			c->Message(CC_Default, "Your access level is not high enough to use this command.");
	}
	else
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
}

void command_listnpcs(Client *c, const Seperator *sep){
	if (strcasecmp(sep->arg[1], "attacked") == 0)
	{
		entity_list.ListNPCs(c, "null", "null", 3);
	}
	else if (strcasecmp(sep->arg[1], "all") == 0)
		entity_list.ListNPCs(c, sep->arg[1], sep->arg[2], 0);
	else if (sep->IsNumber(1) && sep->IsNumber(2))
		entity_list.ListNPCs(c, sep->arg[1], sep->arg[2], 2);
	else if (sep->arg[1][0] != 0)
		entity_list.ListNPCs(c, sep->arg[1], sep->arg[2], 1);
	else {
		c->Message(CC_Default, "Usage of #listnpcs: (all/attacked/npc_name/#) (#)");
		c->Message(CC_Default, "#listnpcs [#] [#] (Each number would search by ID, ex. #listnpcs 1 30, searches 1-30)");
		c->Message(CC_Default, "#listnpcs [name] (Would search for a npc with [name])");
	}
}

void command_date(Client *c, const Seperator *sep)
{
	const auto arguments = sep->argnum;
	if (
		arguments < 1 ||
		!sep->IsNumber(1) ||
		!sep->IsNumber(2) ||
		!sep->IsNumber(3)
		) {
		c->Message(CC_Default, "Usage: #date [Year] [Month] [Day] [Hour] [Minute]");
		c->Message(CC_Default, "Hour and Minute are optional");
		return;
	}

	TimeOfDay_Struct eqTime;
	zone->zone_time.getEQTimeOfDay(time(0), &eqTime);

	const uint16 year = Strings::ToUnsignedInt(sep->arg[1]);
	const uint8  month = Strings::ToUnsignedInt(sep->arg[2]);
	const uint8  day = Strings::ToUnsignedInt(sep->arg[3]);
	const uint8  hour = !sep->IsNumber(4) ? eqTime.hour : Strings::ToUnsignedInt(sep->arg[4]);
	const uint8  minute = !sep->IsNumber(5) ? eqTime.minute : Strings::ToUnsignedInt(sep->arg[5]);
	
	c->Message(CC_Default, fmt::format("Setting world time to {}-{}-{} {}:{}...", year, month, day, hour, minute).c_str());
	zone->SetDate(year, month, day, hour, minute);
}

void command_timezone(Client *c, const Seperator *sep)
{
	const auto arguments = sep->argnum;
	if (arguments < 1 || sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #timezone HH [MM]");
		c->Message(CC_Default, fmt::format("Current timezone is: {}h {}m", zone->zone_time.getEQTimeZoneHr(), zone->zone_time.getEQTimeZoneMin()).c_str());
		return;
	}

	uint8 minutes = 0;
	uint8 hours = Strings::ToUnsignedInt(sep->arg[1]);

	if (hours > 24) {
		hours = 24;
	}

	if (!sep->IsNumber(2)) {
		minutes = Strings::ToUnsignedInt(sep->arg[2]);

		if (minutes > 59) {
			minutes = 59;
		}
	}

	c->Message(CC_Default, fmt::format("Setting timezone to {} h {} m", hours, minutes).c_str());
	const int new_timezone = ((hours * 60) + minutes);
	zone->zone_time.setEQTimeZone(new_timezone);
	database.SetZoneTZ(zone->GetZoneID(), new_timezone);

	// Update all clients with new TZ.
	auto outapp = new EQApplicationPacket(OP_TimeOfDay, sizeof(TimeOfDay_Struct));

	auto tod = (TimeOfDay_Struct*)outapp->pBuffer;
	zone->zone_time.getEQTimeOfDay(time(0), tod);

	entity_list.QueueClients(c, outapp);
	safe_delete(outapp);
}

void command_synctod(Client *c, const Seperator *sep)
{
	c->Message(CC_Default, "Updating Time/Date for all clients in zone...");
	auto outapp = new EQApplicationPacket(OP_TimeOfDay, sizeof(TimeOfDay_Struct));
	TimeOfDay_Struct* tod = (TimeOfDay_Struct*)outapp->pBuffer;
	zone->zone_time.getEQTimeOfDay(time(0), tod);
	entity_list.QueueClients(c, outapp);
	safe_delete(outapp);
}

void command_invul(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #invul [On|Off]");
		return;
	}

	bool invul_flag = atobool(sep->arg[1]);
	Client* target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		target = c->GetTarget()->CastToClient();
	}

	target->SetInvul(invul_flag);
	uint32 account = target->AccountID();
	database.SetGMInvul(account, invul_flag);
	c->Message(
		CC_Default,
		fmt::format(
			"{} {} now {}.",
			c == target ? "You" : target->GetCleanName(),
			c == target ? "are" : "is",
			invul_flag ? "invulnerable" : "vulnerable"
		).c_str()
	);
}

void command_hideme(Client *c, const Seperator *sep){
	bool state = atobool(sep->arg[1]);

	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #hideme [on/off]");
	else
	{
		c->SetHideMe(state);
		c->Message_StringID(MT_Broadcasts, c->GetHideMe() ? NOW_INVISIBLE : NOW_VISIBLE, c->GetName());
	}
}

void command_emote(Client *c, const Seperator *sep){
	if (sep->arg[3][0] == 0)
		c->Message(CC_Default, "Usage: #emote [name | world | zone] type# message");
	else {
		if (strcasecmp(sep->arg[1], "zone") == 0){
			char* newmessage = 0;
			if (strstr(sep->arg[3], "^") == 0)
				entity_list.Message(CC_Default, atoi(sep->arg[2]), sep->argplus[3]);
			else{
				for (newmessage = strtok((char*)sep->arg[3], "^"); newmessage != nullptr; newmessage = strtok(nullptr, "^"))
					entity_list.Message(CC_Default, atoi(sep->arg[2]), newmessage);
			}
		}
		else if (!worldserver.Connected())
			c->Message(CC_Default, "Error: World server disconnected");
		else if (strcasecmp(sep->arg[1], "world") == 0)
			worldserver.SendEmoteMessage(0, 0, atoi(sep->arg[2]), sep->argplus[3]);
		else
			worldserver.SendEmoteMessage(sep->arg[1], 0, atoi(sep->arg[2]), sep->argplus[3]);
	}
}

void command_fov(Client *c, const Seperator *sep){
	if (c->GetTarget())
		if (c->BehindMob(c->GetTarget(), c->GetX(), c->GetY()))
			c->Message(CC_Default, "You are behind mob %s, it is looking to %d", c->GetTarget()->GetName(), c->GetTarget()->GetHeading());
		else
			c->Message(CC_Default, "You are NOT behind mob %s, it is looking to %d", c->GetTarget()->GetName(), c->GetTarget()->GetHeading());
	else
		c->Message(CC_Default, "I Need a target!");
}

void command_manastat(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget() ? c->GetTarget() : c;

	c->Message(CC_Default, "Mana for %s:", target->GetName());
	c->Message(CC_Default, "  Current Mana: %d", target->GetMana());
	c->Message(CC_Default, "  Max Mana: %d", target->GetMaxMana());
}

void command_npcstats(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0)
		c->Message(CC_Default, "ERROR: No target!");
	else if (!c->GetTarget()->IsNPC())
		c->Message(CC_Default, "ERROR: Target is not a NPC!");
	else {
		c->GetTarget()->CastToNPC()->ShowQuickStats(c);
	}
}

void command_zclip(Client *c, const Seperator *sep){
	// modifys and resends zhdr packet
	if (sep->arg[2][0] == 0)
		c->Message(CC_Default, "Usage: #zclip <min clip> <max clip>");
	else if (atoi(sep->arg[1]) <= 0)
		c->Message(CC_Default, "ERROR: Min clip can not be zero or less!");
	else if (atoi(sep->arg[2]) <= 0)
		c->Message(CC_Default, "ERROR: Max clip can not be zero or less!");
	else if (atoi(sep->arg[1])>atoi(sep->arg[2]))
		c->Message(CC_Default, "ERROR: Min clip is greater than max clip!");
	else {
		zone->newzone_data.minclip = atof(sep->arg[1]);
		zone->newzone_data.maxclip = atof(sep->arg[2]);
		if (sep->arg[3][0] != 0)
			zone->newzone_data.fog_minclip[0] = atof(sep->arg[3]);
		if (sep->arg[4][0] != 0)
			zone->newzone_data.fog_minclip[1] = atof(sep->arg[4]);
		if (sep->arg[5][0] != 0)
			zone->newzone_data.fog_maxclip[0] = atof(sep->arg[5]);
		if (sep->arg[6][0] != 0)
			zone->newzone_data.fog_maxclip[1] = atof(sep->arg[6]);
		auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
		memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
		entity_list.QueueClients(c, outapp);
		safe_delete(outapp);
	}
}

void command_npccast(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetTarget()->IsNPC() && !sep->IsNumber(1) && sep->arg[1] != 0 && sep->IsNumber(2)) {
		Mob* spelltar = entity_list.GetMob(sep->arg[1]);
		if (spelltar)
			c->GetTarget()->CastSpell(atoi(sep->arg[2]), spelltar->GetID());
		else
			c->Message(CC_Default, "Error: %s not found", sep->arg[1]);
	}
	else if (c->GetTarget() && c->GetTarget()->IsNPC() && sep->IsNumber(1) && sep->IsNumber(2)) {
		Mob* spelltar = entity_list.GetMob(atoi(sep->arg[1]));
		if (spelltar)
			c->GetTarget()->CastSpell(atoi(sep->arg[2]), spelltar->GetID());
		else
			c->Message(CC_Default, "Error: target ID %i not found", atoi(sep->arg[1]));
	}
	else
		c->Message(CC_Default, "Usage: (needs NPC targeted) #npccast targetname/entityid spellid");
}

void command_zstats(Client *c, const Seperator *sep){
	c->Message(CC_Default, "Zone Header Data:");
	c->Message(CC_Default, "Sky Type: %i", zone->newzone_data.sky);
	c->Message(CC_Default, "Fog Colour: Red: %i; Blue: %i; Green %i", zone->newzone_data.fog_red[0], zone->newzone_data.fog_green[0], zone->newzone_data.fog_blue[0]);
	c->Message(CC_Default, "Safe Coords: %f, %f, %f", zone->newzone_data.safe_x, zone->newzone_data.safe_y, zone->newzone_data.safe_z);
	c->Message(CC_Default, "Underworld Coords: %f", zone->newzone_data.underworld);
	c->Message(CC_Default, "Clip Plane: %f - %f", zone->newzone_data.minclip, zone->newzone_data.maxclip);
}

void command_permaclass(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #permaclass <classnum>");
	}
	else if (!t->IsClient())
		c->Message(CC_Default, "Target is not a client.");
	else {
		c->Message(CC_Default, "Setting %s's class...Sending to char select.", t->GetName());
		Log(Logs::General, Logs::Normal, "Class change request from %s for %s, requested class:%i", c->GetName(), t->GetName(), atoi(sep->arg[1]) );
		t->SetBaseClass(atoi(sep->arg[1]));
		t->Save();
		t->Kick();
	}
}

void command_permarace(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #permarace <racenum>");
		c->Message(CC_Default, "NOTE: Not all models are global. If a model is not global, it will appear as a human on character select and in zones without the model.");
	}
	else if (!t->IsClient())
		c->Message(CC_Default, "Target is not a client.");
	else {
		c->Message(CC_Default, "Setting %s's race - zone to take effect",t->GetName());
		Log(Logs::General, Logs::Normal, "Permanant race change request from %s for %s, requested race:%i", c->GetName(), t->GetName(), atoi(sep->arg[1]) );
		uint32 tmp = Mob::GetDefaultGender(atoi(sep->arg[1]), t->GetBaseGender());
		t->SetBaseRace(atoi(sep->arg[1]));
		t->SetBaseGender(tmp);
		t->Save();
		t->SendIllusionPacket(atoi(sep->arg[1]));
	}
}

void command_permagender(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #permagender <gendernum>");
		c->Message(CC_Default, "Gender Numbers: 0=Male, 1=Female, 2=Neuter");
	}
	else if (!t->IsClient())
		c->Message(CC_Default, "Target is not a client.");
	else {
		c->Message(CC_Default, "Setting %s's gender - zone to take effect",t->GetName());
		Log(Logs::General, Logs::Normal, "Permanant gender change request from %s for %s, requested gender:%i", c->GetName(), t->GetName(), atoi(sep->arg[1]) );
		t->SetBaseGender(atoi(sep->arg[1]));
		t->Save();
		t->SendIllusionPacket(atoi(sep->arg[1]));
	}
}

void command_weather(Client *c, const Seperator *sep)
{
	if (!(sep->arg[1][0] == '0' || sep->arg[1][0] == '1' || sep->arg[1][0] == '2')) 
	{
		c->Message(CC_Default, "Usage: #weather <0/1/2> - Off/Rain/Snow <0/1> - Serverwide <minutes> - Duration");
		return;
	}

	uint8 weather_type = 0;
	if (sep->IsNumber(1) && sep->arg[1][0] != 0)
		weather_type = atoi(sep->arg[1]);

	uint8 serverwide = 0;
	if (sep->IsNumber(2) && sep->arg[2][0] != 0)
		serverwide = atoi(sep->arg[2]);

	//Turn off weather
	if(weather_type == 0)
	{
		c->Message(CC_Yellow, "Turning off weather.");
		zone->zone_weather = 0;
		zone->weather_intensity = 0;

		if (serverwide == 1)
		{
			auto pack = new ServerPacket(ServerOP_Weather, sizeof(ServerWeather_Struct));
			ServerWeather_Struct* ws = (ServerWeather_Struct*)pack->pBuffer;
			ws->type = 0;
			ws->intensity = 0;
			ws->timer = 0;
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
		else
		{
			zone->weatherSend();
		}
	}
	if(zone->zone_weather == 0)
	{
		uint8 intensity = 3;
		uint16 timer = 0;
		if (sep->IsNumber(3) && sep->arg[3][0] != 0)
			timer = atoi(sep->arg[3]) * 60;

		// Snow
		if (weather_type == 2)
		{
			if(timer > 0)
				c->Message(CC_Yellow, "Changing weather to snow for %d seconds.", timer);
			else
				c->Message(CC_Yellow, "Changing weather to snow until the next cycle.");
			zone->zone_weather = 2;
			zone->weather_intensity = intensity;

			if (serverwide == 1)
			{
				auto pack = new ServerPacket(ServerOP_Weather, sizeof(ServerWeather_Struct));
				ServerWeather_Struct* ws = (ServerWeather_Struct*)pack->pBuffer;
				ws->type = zone->zone_weather;
				ws->intensity = intensity;
				ws->timer = timer;
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
			else
			{

				zone->weatherSend(timer*1000);
			}

			return;
		}
		// Rain
		else if (weather_type == 1)
		{
			if(timer > 0)
				c->Message(CC_Yellow, "Changing weather to rain for %d seconds.", timer);
			else
				c->Message(CC_Yellow, "Changing weather to rain until the next cycle.");
			zone->zone_weather = 1;
			zone->weather_intensity = intensity;

			if (serverwide == 1)
			{
				auto pack = new ServerPacket(ServerOP_Weather, sizeof(ServerWeather_Struct));
				ServerWeather_Struct* ws = (ServerWeather_Struct*)pack->pBuffer;
				ws->type = zone->zone_weather;
				ws->intensity = intensity;
				ws->timer = timer;
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
			else
			{
				zone->weatherSend(timer*1000);
			}

			return;
		}
	}
	else
	{
		if(weather_type != 0)
		{
			c->Message(CC_Yellow, "You cannot change from one type of weather to another without first stopping it.");
			return;
		}
	}
}

void command_zheader(Client *c, const Seperator *sep){
	// sends zhdr packet
	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #zheader <zone name>");
	}
	else if (database.GetZoneID(sep->argplus[1]) == 0)
		c->Message(CC_Default, "Invalid Zone Name: %s", sep->argplus[1]);
	else {

		if (zone->LoadZoneCFG(sep->argplus[1], true))
			c->Message(CC_Default, "Successfully loaded zone header for %s from database.", sep->argplus[1]);
		else
			c->Message(CC_Default, "Failed to load zone header %s from database", sep->argplus[1]);
		auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
		memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
		entity_list.QueueClients(c, outapp);
		safe_delete(outapp);
	}
}

void command_zsky(Client *c, const Seperator *sep){
	// modifys and resends zhdr packet
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #zsky <sky type>");
	else if (atoi(sep->arg[1])<0 || atoi(sep->arg[1])>255)
		c->Message(CC_Default, "ERROR: Sky type can not be less than 0 or greater than 255!");
	else {
		zone->newzone_data.sky = atoi(sep->arg[1]);
		auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
		memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
		entity_list.QueueClients(c, outapp);
		safe_delete(outapp);
	}
}

void command_zcolor(Client *c, const Seperator *sep){
	// modifys and resends zhdr packet
	if (sep->arg[3][0] == 0)
		c->Message(CC_Default, "Usage: #zcolor <red> <green> <blue>");
	else if (atoi(sep->arg[1])<0 || atoi(sep->arg[1])>255)
		c->Message(CC_Default, "ERROR: Red can not be less than 0 or greater than 255!");
	else if (atoi(sep->arg[2])<0 || atoi(sep->arg[2])>255)
		c->Message(CC_Default, "ERROR: Green can not be less than 0 or greater than 255!");
	else if (atoi(sep->arg[3])<0 || atoi(sep->arg[3])>255)
		c->Message(CC_Default, "ERROR: Blue can not be less than 0 or greater than 255!");
	else {
		for (int z = 0; z<4; z++) {
			zone->newzone_data.fog_red[z] = atoi(sep->arg[1]);
			zone->newzone_data.fog_green[z] = atoi(sep->arg[2]);
			zone->newzone_data.fog_blue[z] = atoi(sep->arg[3]);
		}
		auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
		memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
		entity_list.QueueClients(c, outapp);
		safe_delete(outapp);
	}
}

void command_gassign(Client *c, const Seperator *sep){
	if (sep->IsNumber(1) && c->GetTarget() && c->GetTarget()->IsNPC() && c->GetTarget()->CastToNPC()->GetSpawnPointID() > 0) {
		int spawn2id = c->GetTarget()->CastToNPC()->GetSpawnPointID();
		database.AssignGrid(c, atoi(sep->arg[1]), spawn2id);
	}
	else
		c->Message(CC_Default, "Usage: #gassign [num] - must have an npc target!");
}

void command_ai(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #ai consider [Mob Name] - Show how an NPC considers to a mob");
		c->Message(CC_Default, "Usage: #ai faction [Faction ID] - Set an NPC's Faction ID");
		c->Message(CC_Default, "Usage: #ai guard - Save an NPC's guard spot to their current location");
		c->Message(CC_Default, "Usage: #ai roambox [Min X] [Max X] [Min Y] [Max Y] [Delay] [Minimum Delay] - Set an NPC's roambox using X and Y coordinates");
		c->Message(CC_Default, "Usage: #ai spells [Spell List ID] - Set an NPC's Spell List ID");
		return;
	}

	if (!c->GetTarget() || !c->GetTarget()->IsNPC()) {
		c->Message(CC_Default, "You must target an NPC to use this command.");
		return;
	}

	auto target = c->GetTarget()->CastToNPC();

	bool is_consider = !strcasecmp(sep->arg[1], "consider");
	bool is_faction = !strcasecmp(sep->arg[1], "faction");
	bool is_guard = !strcasecmp(sep->arg[1], "guard");
	bool is_roambox = !strcasecmp(sep->arg[1], "roambox");
	bool is_spells = !strcasecmp(sep->arg[1], "spells");

	if (
		!is_consider &&
		!is_faction &&
		!is_guard &&
		!is_roambox &&
		!is_spells
		) {
		c->Message(CC_Default, "Usage: #ai consider [Mob Name] - Show how an NPC considers to a mob");
		c->Message(CC_Default, "Usage: #ai faction [Faction ID] - Set an NPC's Faction ID");
		c->Message(CC_Default, "Usage: #ai guard - Save an NPC's guard spot to their current location");
		c->Message(CC_Default, "Usage: #ai roambox [Min X] [Max X] [Min Y] [Max Y] [Delay] [Minimum Delay] - Set an NPC's roambox using X and Y coordinates");
		c->Message(CC_Default, "Usage: #ai spells [Spell List ID] - Set an NPC's Spell List ID");
		return;
	}

	if (is_consider) {
		if (arguments == 2) {
			auto mob_name = sep->arg[2];
			auto mob_to_consider = entity_list.GetMob(mob_name);
			if (mob_to_consider) {
				auto consider_level = static_cast<uint8>(mob_to_consider->GetReverseFactionCon(target));
				c->Message(
					CC_Default,
					fmt::format(
						"{} ({}) considers {} ({}) as {} ({}).",
						target->GetCleanName(),
						target->GetID(),
						mob_to_consider->GetCleanName(),
						mob_to_consider->GetID(),
						EQ::constants::GetConsiderLevelName(consider_level),
						consider_level
					).c_str()
				);
			}
			else
				c->Message(CC_Default, fmt::format("Error: {} not found.", mob_name).c_str());
		}
		else {
			c->Message(CC_Default, "Usage: #ai consider [Mob Name] - Show how an NPC considers a mob");
		}
	}
	else if (is_faction) {
		if (sep->IsNumber(2)) {
			auto faction_id = std::stoi(sep->arg[2]);
			auto faction_name = database.GetFactionName(faction_id);
			target->SetNPCFactionID(faction_id);
			c->Message(
				CC_Default,
				fmt::format(
					"{} ({}) is now on Faction {}.",
					target->GetCleanName(),
					target->GetID(),
					(
						faction_name.empty() ?
						std::to_string(faction_id) :
						fmt::format(
							"{} ({})",
							faction_name,
							faction_id
						)
						)
				).c_str()
			);
		}
		else {
			c->Message(CC_Default, "Usage: #ai faction [Faction ID] - Set an NPC's Faction ID");
		}
	}
	else if (is_guard) {
		auto target_position = target->GetPosition();

		target->SaveGuardSpot();

		c->Message(
			CC_Default,
			fmt::format(
				"{} ({}) now has a guard spot of {:.2f}, {:.2f}, {:.2f} with a heading of {:.2f}.",
				target->GetCleanName(),
				target->GetID(),
				target_position.x,
				target_position.y,
				target_position.z,
				target_position.w
			).c_str()
		);
	}
	else if (is_roambox) {
		if (target->IsAIControlled()) {
			if (
				arguments >= 5 &&
				arguments <= 7 &&
				sep->IsNumber(2) &&
				sep->IsNumber(3) &&
				sep->IsNumber(4) &&
				sep->IsNumber(5)
				) {
				auto min_x = std::stof(sep->arg[2]);
				auto max_x = std::stof(sep->arg[3]);
				auto min_y = std::stof(sep->arg[4]);
				auto max_y = std::stof(sep->arg[5]);

				uint32 delay = 2500;
				uint32 minimum_delay = 2500;

				if (sep->IsNumber(6)) {
					delay = std::stoul(sep->arg[6]);
				}

				if (sep->IsNumber(7)) {
					minimum_delay = std::stoul(sep->arg[7]);
				}

				target->CastToNPC()->AI_SetRoambox(
					max_x,
					min_x,
					max_y,
					min_y,
					delay,
					minimum_delay
				);

				c->Message(
					CC_Default,
					fmt::format(
						"{} ({}) now has a roambox from {}, {} to {}, {} with {} and {}.",
						target->GetCleanName(),
						target->GetID(),
						min_x,
						min_y,
						max_x,
						max_y,
						(
							delay ?
							fmt::format(
								"a delay of {} ({})",
								Strings::MillisecondsToTime(delay),
								delay
							) :
							"no delay"
							),
						(
							minimum_delay ?
							fmt::format(
								"a minimum delay of {} ({})",
								Strings::MillisecondsToTime(minimum_delay),
								minimum_delay
							) :
							"no minimum delay"
							)
					).c_str()
				);
			}
			else {
				c->Message(CC_Default, "Usage: #ai roambox [Min X] [Max X] [Min Y] [Max Y] [Delay] [Minimum Delay] - Set an NPC's roambox using X and Y coordinates");
			}
		}
		else {
			c->Message(CC_Default, "You must target an NPC with AI.");
		}
	}
	else if (is_spells) {
		if (sep->IsNumber(2)) {
			auto spell_list_id = std::stoul(sep->arg[2]);
			if (spell_list_id >= 0) {
				target->CastToNPC()->AI_AddNPCSpells(spell_list_id);

				c->Message(
					CC_Default,
					fmt::format(
						"{} ({}) is now using Spell List {}.",
						target->GetCleanName(),
						target->GetID(),
						spell_list_id
					).c_str()
				);
			}
			else {
				c->Message(CC_Default, "Spell List ID must be greater than or equal to 0.");
			}
		}
	}
}

void command_worldshutdown(Client *c, const Seperator *sep){
	// GM command to shutdown world server and all zone servers
	uint32 time = 0;
	uint32 interval = 0;
	if (worldserver.Connected()) {
		if (
			sep->IsNumber(1) &&
			sep->IsNumber(2) &&
			((time = std::stoi(sep->arg[1])) > 0) &&
			((interval = std::stoi(sep->arg[2])) > 0)
			) {
			int time_minutes = (time / 60);
			worldserver.SendEmoteMessage(0, AccountStatus::Player,
				CC_Default,
				fmt::format(
					"[SYSTEM] World will be shutting down in {} minutes.",
					time_minutes
				).c_str()
			);
			c->Message(
				CC_Default,
				fmt::format(
					"World will be shutting down in {} minutes, notifying every {} seconds",
					time_minutes,
					interval
				).c_str()
			);
			auto pack = new ServerPacket(ServerOP_ShutdownAll, sizeof(WorldShutDown_Struct));
			WorldShutDown_Struct* wsd = (WorldShutDown_Struct*)pack->pBuffer;
			wsd->time = time * 1000;
			wsd->interval = (interval * 1000);
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
		else if (!strcasecmp(sep->arg[1], "now")){
			worldserver.SendEmoteMessage(0, AccountStatus::Player, CC_Yellow, "[SYSTEM] World is shutting down now.");
			c->Message(CC_Default, "World is shutting down now.");
			auto pack = new ServerPacket;
			pack->opcode = ServerOP_ShutdownAll;
			pack->size = 0;
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
		else if (!strcasecmp(sep->arg[1], "disable")){
			c->Message(CC_Default, "World shutdown has been aborted.");
			auto pack = new ServerPacket(ServerOP_ShutdownAll, sizeof(WorldShutDown_Struct));
			WorldShutDown_Struct* wsd = (WorldShutDown_Struct*)pack->pBuffer;
			wsd->time = 0;
			wsd->interval = 0;
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
		else{
			c->Message(CC_Default, "#worldshutdown - Shuts down the server and all zones.");
			c->Message(CC_Default, "Usage: #worldshutdown now - Shuts down the server and all zones immediately.");
			c->Message(CC_Default, "Usage: #worldshutdown disable - Stops the server from a previously scheduled shut down.");
			c->Message(CC_Default, "Usage: #worldshutdown [timer] [interval] - Shuts down the server and all zones after [timer] seconds and notifies players every [interval] seconds.");
		}
	}
	else
		c->Message(CC_Default, "Error: World server disconnected");
}

void command_sendzonespawns(Client *c, const Seperator *sep){
	entity_list.SendZoneSpawns(c);
}

void command_zsave(Client *c, const Seperator *sep){
	if (zone->SaveZoneCFG())
		c->Message(CC_Red, "Zone header saved successfully.");
	else
		c->Message(CC_Red, "ERROR: Zone header data was NOT saved.");
}

void command_dbspawn2(Client *c, const Seperator *sep){

	if (sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3)) {
		Log(Logs::General, Logs::Normal, "Spawning database spawn");
		uint16 cond = 0;
		int16 cond_min = 0;
		if (sep->IsNumber(4)) {
			cond = atoi(sep->arg[4]);
			if (sep->IsNumber(5))
				cond_min = atoi(sep->arg[5]);
		}
		database.CreateSpawn2(c, atoi(sep->arg[1]), zone->GetShortName(), c->GetPosition(), atoi(sep->arg[2]), atoi(sep->arg[3]), cond, cond_min);
	}
	else {
		c->Message(CC_Default, "Usage: #dbspawn2 spawngroup respawn variance [condition_id] [condition_min]");
	}
}

void command_shutdown(Client *c, const Seperator *sep){
	CatchSignal(2);
}

void command_delacct(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Format: #delacct accountname");
	else
		if (database.DeleteAccount(sep->arg[1]))
			c->Message(CC_Default, "The account was deleted.");
		else
			c->Message(CC_Default, "Unable to delete account.");
}

void command_setnpcexpansion(Client* c, const Seperator* sep)
{
	if (sep->arg[1][0] && sep->arg[2][0]  && c->GetTarget() && c->GetTarget()->IsNPC() && c->GetTarget()->CastToNPC()->GetSpawnPointID() != 0)
	{
		std::string query = fmt::format(
			"UPDATE spawn2 SET min_expansion = {}, max_expansion = {} WHERE id = {}",
			std::stof(sep->arg[1]),
			std::stof(sep->arg[2]),
			c->GetTarget()->CastToNPC()->GetSpawnPointID()

		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Red, "Failed to set spawn expansion bitmask.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"Name: {} Min Expansion {} Max Expansion {} Successfully Set",
				c->GetTarget()->GetCleanName(),
				std::stof(sep->arg[1]),
				std::stof(sep->arg[2])
			).c_str()
		);
	}
	else
	{
		c->Message(CC_Default, "Usage: #setnpcexpansion [min_expansion] [max_expansion]");
	}
}

void command_setpass(Client *c, const Seperator *sep){
	int arguments = sep->argnum;
	if (arguments < 2) {
		c->Message(CC_Default, "Usage: #setpass [Account Name] [Password]");
		return;
	}

	std::string account_name;
	std::string loginserver;
	ParseAccountString(sep->arg[1], account_name, loginserver);
	int16 status = 0;

	auto account_id = database.GetAccountIDByName(account_name, &status);
	if (!account_id) {
		c->Message(
			CC_Default,
			fmt::format(
				"Account {} not found.",
				account_name
			).c_str()
		);
		return;
	}

	if (status > c->Admin()) {
		c->Message(
			CC_Default,
			fmt::format(
				"You cannot change the password for Account {} as its status is higher than yours.",
				account_name
			).c_str()
		);
		return;
	}

	c->Message(
		CC_Default,
		fmt::format(
			"Password {} changed for Account {}.",
			(
				database.SetLocalPassword(account_id, sep->arg[2]) ?
				"successfully" :
				"failed"
				),
			account_name
		).c_str()
	);
}

void command_setlsinfo(Client* c, const Seperator* sep) {
	int arguments = sep->argnum;
	if (arguments < 2) {
		c->Message(CC_Default, "Usage: #setlsinfo [Email] [Password]");
		return;
	}

	auto pack = new ServerPacket(ServerOP_LSAccountUpdate, sizeof(ServerLSAccountUpdate_Struct));
	auto s = (ServerLSAccountUpdate_Struct*)pack->pBuffer;
	s->useraccountid = c->LSAccountID();
	strn0cpy(s->useraccount, c->AccountName(), 16);
	strn0cpy(s->useremail, sep->arg[1], 100);
	strn0cpy(s->userpassword, sep->arg[2], 16);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	c->Message(CC_Default, "Your email and local loginserver password have been set.");
}

void command_grid(Client* c, const Seperator* sep) {
	if (strcasecmp("max", sep->arg[1]) == 0)
	{
		c->Message(CC_Default, "Highest grid ID in this zone: %d", database.GetHighestGrid(zone->GetZoneID()));
	}
	else if (strcasecmp("add", sep->arg[1]) == 0)
	{
		database.ModifyGrid(c, false, atoi(sep->arg[2]), atoi(sep->arg[3]), atoi(sep->arg[4]), zone->GetZoneID());
	}
	else if (strcasecmp("show", sep->arg[1]) == 0) 
	{

		Mob* target = c->GetTarget();

		if (!target || !target->IsNPC()) 
		{
			c->Message(0, "You need a NPC target!");
			return;
		}

		std::string query = StringFormat (
			"SELECT `x`, `y`, `z`, `heading`, `number` "
			"FROM `grid_entries` "
			"WHERE `zoneid` = %u and `gridid` = %i "
			"ORDER BY `number`",
			zone->GetZoneID(),
			target->CastToNPC()->GetGrid()
		);

		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(0, "Error querying database.");
			c->Message(0, query.c_str());
		}

		if (results.RowCount() == 0) {
			c->Message(0, "No grid found");
			return;
		}

		/**
		 * Spawn grid nodes
		 */
		std::map<std::vector<float>, int32> zoffset;

		for (auto row = results.begin(); row != results.end(); ++row) {
			glm::vec4 node_position = glm::vec4(atof(row[0]), atof(row[1]), atof(row[2]), atof(row[3]));
			std::vector<float> node_loc{
					node_position.x, node_position.y, node_position.z
			};

			// If we already have a node at this location, set the z offset
			// higher from the existing one so we can see it.  Adjust so if
			// there is another at the same spot we adjust again.
			auto search = zoffset.find(node_loc);
			if (search != zoffset.end()) {
				search->second = search->second + 3;
			}
			else {
				zoffset[node_loc] = 0.0;
			}

			node_position.z += zoffset[node_loc];

			NPC::SpawnGridNodeNPC(node_position, atoi(row[4]), zoffset[node_loc]);
		}
	}
	else if (strcasecmp("delete", sep->arg[1]) == 0)
	{
		database.ModifyGrid(c, true, atoi(sep->arg[2]), 0, 0, zone->GetZoneID());
	}
	else
	{
		c->Message(CC_Default, "Usage: #grid add/delete grid_num wandertype pausetype");
		c->Message(CC_Default, "Usage: #grid max - displays the highest grid ID used in this zone (for add)");
	}
}

std::vector<std::string> grid_wander_types = { "circle","rand10","random","patrol","onewayrepop","random5los","onewaydepop","wp0center","rcenter","randpath" };

std::vector<std::string> grid_pause_types = { "rph", "full","pr" };



void command_gridrecord(Client* c, const Seperator* sep)
{
	int32 highest_grid_plus_one = database.GetHighestGrid(zone->GetZoneID()) + 1;

	//#gridrecord, no params

	if (sep->arg[1][0] == 0 || strcasecmp(sep->arg[1], "help") == 0)
	{
		goto HelpLabel;
	}

	if (strcasecmp(sep->arg[1], "start") == 0)
	{
		GridWanderType wander_type = GridWanderType::eGridPatrol;
		GridPauseType pause_type = GridPauseType::eGridPauseFull;
		if (sep->arg[2][0] != 0)
		{
			auto wander_it = std::find(grid_wander_types.begin(), grid_wander_types.end(), (const char*)(sep->arg[2]));
			auto pause_it = std::find(grid_pause_types.begin(), grid_pause_types.end(), (const char*)(sep->arg[3]));
			if (wander_it != grid_wander_types.end())
				wander_type = (GridWanderType)std::distance(grid_wander_types.begin(), wander_it);
			if (pause_it != grid_pause_types.end())
				pause_type = (GridPauseType)std::distance(grid_wander_types.begin(), pause_it);
			if (c->gm_grid)
				safe_delete(c->gm_grid);
			if (!c->gm_grid)
			{
				c->gm_grid = new DBGrid_Struct();
				c->gm_grid->id = highest_grid_plus_one;
				c->gm_grid->wander_type = wander_type;
				c->gm_grid->pause_type = pause_type;
				c->gm_grid_waypoint_list.clear();
				c->Message(CC_Default, "Started recording grid %i, wandertype %i pausetype %i", c->gm_grid->id, wander_type, pause_type);
			}
		}
		else
		{
			goto HelpLabel;
		}
	}
	else if (strcasecmp(sep->arg[1], "stop") == 0)
	{
		if (c->gm_grid && c->gm_grid->id != 0)
		{
			database.ModifyGrid(c, false, c->gm_grid->id, (uint8)c->gm_grid->wander_type, (uint8)c->gm_grid->pause_type, zone->GetZoneID());
			int i = 1;
			for (auto wp : c->gm_grid_waypoint_list)
			{
				glm::vec4 wpposition;
				wpposition.x = wp.x;
				wpposition.y = wp.y;
				wpposition.z = wp.z;
				wpposition.w = wp.heading;
				database.AddWP(c, c->gm_grid->id, wp.index, wpposition, wp.pause, zone->GetZoneID());
				i++;
			}
			c->gm_grid_waypoint_list.clear();
			c->Message(CC_Default, "Wrote grid %i to DB, wandertype %i pausetype %i, Wrote %i waypoints", c->gm_grid->id, (uint8)c->gm_grid->wander_type, (uint8)c->gm_grid->pause_type, i);
		}
		if (c->gm_grid)
			safe_delete(c->gm_grid);
	}
	else if (strcasecmp(sep->arg[1], "addwp") == 0)
	{
		if (c->gm_grid && c->gm_grid->id != 0)
		{
			int pause = 0;
			if (sep->arg[2][0] != 0)
				pause = atoi(sep->arg[2]);

			auto index = c->gm_grid_waypoint_list.size() + 1;
			wplist wp;
			wp.centerpoint = 0;
			wp.heading = c->GetHeading();
			wp.x = c->GetX();
			wp.y = c->GetY();
			//start bestz
			glm::vec3 me;
			me.x = c->GetX();
			me.y = c->GetY();
			me.z = c->GetZ();
			float best_z = zone->zonemap ? zone->zonemap->FindBestZ(me, nullptr) : c->GetZ();
			wp.z = best_z;
			// end best_z


			wp.pause = pause;
			wp.index = index;
			c->gm_grid_waypoint_list.push_back(wp);
			c->Message(CC_Default, "Added WP %i to Temporary Grid %i", wp.index, c->gm_grid->id);
		}
		else
		{
			goto HelpLabel;
		}
	}
	else
	{
	HelpLabel:
		c->Message(CC_Default, "Usage: #gridrecord [start|stop|addwp]");
		c->Message(CC_Default, "[start] [type = ] [pausetype = full] ");
		c->Message(CC_Default, "[stop] [type] [pausetype = full] ");
		c->Message(CC_Default, "Valid WanderTypes (use abbreviation): [circle|rand10|random|patrol|onewayrepop|random5los|onewaydepop|wp0centerpoint (wp0center)|randcenterpoint (rcenter)|randpath ]");
		c->Message(CC_Default, "Valid FullTypes (use abbreviation): [randomplushalf (rph)|pausefull (full) |pauserandom (pr)]");
		c->Message(CC_Default, "Max Grid in Zone +1: %i", highest_grid_plus_one);
	}

}

void command_wp(Client *c, const Seperator *sep){
	int wp = atoi(sep->arg[4]);

	if (strcasecmp("add", sep->arg[1]) == 0) {
		if (wp == 0) //default to highest if it's left blank, or we enter 0
			wp = database.GetHighestWaypoint(zone->GetZoneID(), atoi(sep->arg[2])) + 1;
		if (strcasecmp("-h",sep->arg[5]) == 0) {
			database.AddWP(c, atoi(sep->arg[2]),wp, c->GetPosition(), atoi(sep->arg[3]),zone->GetZoneID());
		}
		else {
            auto position = c->GetPosition();
            position.w = -1;
			database.AddWP(c, atoi(sep->arg[2]),wp, position, atoi(sep->arg[3]),zone->GetZoneID());
		}
	}
	else if (strcasecmp("delete", sep->arg[1]) == 0)
		database.DeleteWaypoint(c, atoi(sep->arg[2]), wp, zone->GetZoneID());
	else
		c->Message(CC_Default, "Usage: #wp add/delete grid_num pause wp_num [-h]");
}

void command_iplookup(Client *c, const Seperator *sep){
	auto pack =
	    new ServerPacket(ServerOP_IPLookup, sizeof(ServerGenericWorldQuery_Struct) + strlen(sep->argplus[1]) + 1);
	ServerGenericWorldQuery_Struct* s = (ServerGenericWorldQuery_Struct *)pack->pBuffer;
	strcpy(s->from, c->GetName());
	s->admin = c->Admin();
	if (sep->argplus[1][0] != 0)
		strcpy(s->query, sep->argplus[1]);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void command_size(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
	{
		// ChangeSize just sends the AT_Size apperance packet, which doesn't support float.
		// All attempts to workaround this using the illusion packet have failed. 
		c->Message(CC_Default, "Usage: #size [0 - 255] This command does not support decimals.");
	}
	else 
	{
		float newsize = atof(sep->arg[1]);
		//if (newsize > 255)
		//{
		//	c->Message(CC_Default, "Error: #size: Size can not be greater than 255.");
		//	return;
		//}
		if (newsize < 0)
		{
			c->Message(CC_Default, "Error: #size: Size can not be less than 0.");
			return;
		}
		else if (!target)
		{
			target = c;
		}
		else 
		{
			target->ChangeSize(newsize, true);
			c->Message(CC_Default, "%s is now size %0.1f", target->GetName(), atof(sep->arg[1]));
		}
	}
}

void command_mana(Client *c, const Seperator *sep)
{
	auto target = c->GetTarget() ? c->GetTarget() : c;
	if (target->IsClient()) {
		target->CastToClient()->SetMana(target->CastToClient()->CalcMaxMana());
	}
	else {
		target->SetMana(target->CalcMaxMana());
	}

	if (c != target) {
		c->Message(
			CC_Default,
			fmt::format(
				"Set {} ({}) to full Mana.",
				target->GetCleanName(),
				target->GetID()
			).c_str()
		);
	}
	else {
		c->Message(CC_Default, "Restored your Mana to full.");
	}
}

void command_flymode(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (!arguments || !sep->IsNumber(1)) {
		return;
	}

	Mob* target = c;

	if (c->GetTarget()) {
		target = c->GetTarget();
	}

	auto flymode_id = std::stoul(sep->arg[1]);
	uint32 account = c->AccountID();
	if (
		flymode_id < EQ::constants::GravityBehavior::Ground &&
		flymode_id > EQ::constants::GravityBehavior::Water
		) {
		c->Message(CC_Default, "Usage:: #flymode [Flymode ID]");
		c->Message(CC_Default, "0 = Ground, 1 = Flying, 2 = Levitating, 3 = Water");
		return;
	}

	target->SendAppearancePacket(AT_Levitate, flymode_id);
	database.SetGMFlymode(account, static_cast<EQ::constants::GravityBehavior>(flymode_id));
	c->Message(
		CC_Default,
		fmt::format(
			"Fly Mode for {} is now {} ({}).",
			(
				c == target ?
				"yourself" :
				fmt::format(
					"{} ({})",
					target->GetCleanName(),
					target->GetID()
				)
				),
			EQ::constants::GetFlyModeName(flymode_id),
			flymode_id
		).c_str()
	);
}

void command_showskills(Client *c, const Seperator *sep){
	Mob *t = c;

	if (c->GetTarget())
		t = c->GetTarget();

	c->Message(CC_Default, "Skills for %s", t->GetName());
	for (EQ::skills::SkillType i = EQ::skills::Skill1HBlunt; i <= EQ::skills::HIGHEST_SKILL; i = (EQ::skills::SkillType)(i + 1))
		c->Message(CC_Default, "Skill [%d] is at [%d]", i, t->GetSkill(i));
}

void command_findspell(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #FindSpell [spellname]");
	else if (SPDAT_RECORDS <= 0)
		c->Message(CC_Default, "Spells not loaded");
	else if (Seperator::IsNumber(sep->argplus[1])) {
		int spellid = atoi(sep->argplus[1]);
		if (spellid <= 0 || spellid >= SPDAT_RECORDS) {
			c->Message(CC_Default, "Error: Number out of range");
		}
		else {
			c->Message(CC_Default, "  %i: %s", spellid, spells[spellid].name);
		}
	}
	else {
		int count = 0;
		//int iSearchLen = strlen(sep->argplus[1])+1;
		std::string sName;
		std::string sCriteria;
		sCriteria = sep->argplus[1];
		for (auto & c : sCriteria) c = toupper(c);
		for (int i = 0; i<SPDAT_RECORDS; i++) {
			if (spells[i].name[0] != 0) {
				sName = spells[i].name;

				for (auto & c : sName) c = toupper(c);
				if (sName.find(sCriteria) != std::string::npos && (count <= 20)) {
					c->Message(CC_Default, "  %i: %s", i, spells[i].name);
					count++;
				}
				else if (count > 20)
					break;
			}
		}
		if (count > 20)
			c->Message(CC_Default, "20 spells found... max reached.");
		else
			c->Message(CC_Default, "%i spells found.", count);
	}
}

void command_castspell(Client *c, const Seperator *sep){
	if (!sep->IsNumber(1))
	{
		c->Message(CC_Default, "Usage: #CastSpell spellid gm_override entityid");
		c->Message(CC_Default, "gm_override 0: Cast Normally. 1: Skip stacking and resist checks. 2: Normal, but don't send buff fade when a spell overwrites.");
	}
	else 
	{
		uint16 spellid = atoi(sep->arg[1]);
		uint8 gm_override = atoi(sep->arg[2]);
		uint16 entityid = atoi(sep->arg[3]);

		if(entityid > 0)
		{ 
			Mob* caster = entity_list.GetMob(entityid);
			if (caster && caster->IsNPC())
			{
				if (c->GetTarget() == 0)
				{
					caster->CastSpell(spellid, c->GetID(), EQ::spells::CastingSlot::Item);
				}
				else
				{
					caster->CastSpell(spellid, c->GetTarget()->GetID(), EQ::spells::CastingSlot::Item);
				}
			}
			else
			{
				c->Message(CC_Default, "Caster specified is not a NPC or is not valid.");
				return;
			}
		}
		/*
		Spell restrictions.
		*/
		else if (((spellid == 2859) || (spellid == 841) || (spellid == 300) || (spellid == 2314) ||
			(spellid == 3716) || (spellid == 911) || (spellid == 3014) || (spellid == 982) ||
			(spellid == 905) || (spellid == 2079) || (spellid == 1218) || (spellid == 819) ||
			((spellid >= 780) && (spellid <= 785)) || ((spellid >= 1200) && (spellid <= 1205)) ||
			((spellid >= 1342) && (spellid <= 1348)) || (spellid == 1923) || (spellid == 1924) ||
			(spellid == 3355)) &&
			c->Admin() < commandCastSpecials)
			c->Message(CC_Red, "Unable to cast spell.");
		else if (spellid >= SPDAT_RECORDS)
			c->Message(CC_Default, "Error: #CastSpell: Argument out of range");
		else
		{
			if (c->GetTarget() == 0)
			{
				if (gm_override != 0)
				{
					c->SetGMSpellException(gm_override);
				}

				if (c->Admin() >= commandInstacast)
					c->SpellFinished(spellid, 0, EQ::spells::CastingSlot::Item, 0, -1, spells[spellid].ResistDiff);
				else
					c->CastSpell(spellid, 0, EQ::spells::CastingSlot::Item, 0);

				c->SetGMSpellException(0);
			}
			else
			{
				if (gm_override != 0)
				{
					c->GetTarget()->SetGMSpellException(gm_override);
				}

				if (c->Admin() >= commandInstacast)
					c->SpellFinished(spellid, c->GetTarget(), EQ::spells::CastingSlot::Item, 0, -1, spells[spellid].ResistDiff);
				else
					c->CastSpell(spellid, c->GetTarget()->GetID(), EQ::spells::CastingSlot::Item, 0);

				if(c->GetTarget())
					c->GetTarget()->SetGMSpellException(0);
			}
		}
	}
}

void command_setlanguage(Client *c, const Seperator *sep){
	Client* target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		target = c->GetTarget()->CastToClient();
	}

	auto language_id = sep->IsNumber(1) ? std::stoi(sep->arg[1]) : -1;
	auto language_value = sep->IsNumber(2) ? std::stoi(sep->arg[2]) : -1;
	if (!strcasecmp(sep->arg[1], "list")) {
		for (int language = LANG_COMMON_TONGUE; language <= LANG_UNKNOWN2; language++) {
			c->Message(
				CC_Default,
				fmt::format(
					"Language {}: {}",
					language,
					EQ::constants::GetLanguageName(language)
				).c_str()
			);
		}
	}
	else if (
		language_id < LANG_COMMON_TONGUE ||
		language_id > LANG_UNKNOWN2 ||
		language_value < 0 ||
		language_value > 100
		) {
		c->Message(CC_Default, "Usage: #setlanguage [Language ID] [Language Value]");
		c->Message(CC_Default, "Usage: #setlanguage [List]");
		c->Message(CC_Default, "Language ID = 0 to 26", LANG_UNKNOWN2);
		c->Message(CC_Default, "Language Value = 0 to 100", HARD_SKILL_CAP);
	}
	else {
		LogInfo(
			"Set language request from [{}], Target: [{}] Language ID: [{}] Language Value: [{}]",
			c->GetCleanName(),
			target->GetCleanName(),
			language_id,
			language_value
		);

		target->SetLanguageSkill(language_id, language_value);

		if (c != target) {
			c->Message(
				CC_Default,
				fmt::format(
					"Set {} ({}) to {} for {}.",
					EQ::constants::GetLanguageName(language_id),
					language_id,
					language_value,
					target->GetCleanName()
				).c_str()
			);
		}
	}
}

void command_setskill(Client *c, const Seperator *sep){
	if (c->GetTarget() == nullptr) {
		c->Message(CC_Default, "Error: #setskill: No target.");
	}
	else if (!c->GetTarget()->IsClient()) {
		c->Message(CC_Default, "Error: #setskill: Target must be a client.");
	}
	else if (
		!sep->IsNumber(1) || atoi(sep->arg[1]) < 0 || atoi(sep->arg[1]) > EQ::skills::HIGHEST_SKILL ||
		!sep->IsNumber(2) || atoi(sep->arg[2]) < 0
		)
	{
		c->Message(CC_Default, "Usage: #setskill skill x ");
		c->Message(CC_Default, "       skill = 0 to %d", EQ::skills::HIGHEST_SKILL);
		c->Message(CC_Default, "       x = 0 to %d", HARD_SKILL_CAP);
	}
	else {
		Log(Logs::General, Logs::Normal, "Set skill request from %s, target:%s skill_id:%i value:%i", c->GetName(), c->GetTarget()->GetName(), atoi(sep->arg[1]), atoi(sep->arg[2]) );
		int skill_num = atoi(sep->arg[1]);
		uint16 skill_value = atoi(sep->arg[2]) > HARD_SKILL_CAP ? HARD_SKILL_CAP : atoi(sep->arg[2]);
		if (skill_num < EQ::skills::HIGHEST_SKILL)
		{
			Client* t = c->GetTarget()->CastToClient();
			if (t)
			{
				t->SetSkill((EQ::skills::SkillType)skill_num, skill_value);
			}
			else
			{
				c->Message(CC_Default, "Error: #setskill: No target.");
			}
		}
	}
}

void command_setskillall(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0)
		c->Message(CC_Default, "Error: #setallskill: No target.");
	else if (!c->GetTarget()->IsClient())
		c->Message(CC_Default, "Error: #setskill: Target must be a client.");
	else if (!sep->IsNumber(1) || atoi(sep->arg[1]) < 0) {
		c->Message(CC_Default, "Usage: #setskillall value ");
		c->Message(CC_Default, "       value = 0 to %d", HARD_SKILL_CAP);
	}
	else {
		if (c->Admin() >= commandSetSkillsOther || c->GetTarget() == c || c->GetTarget() == 0) {
			Log(Logs::General, Logs::Normal, "Set ALL skill request from %s, target:%s", c->GetName(), c->GetTarget()->GetName());
			uint16 level = atoi(sep->arg[1]) > HARD_SKILL_CAP ? HARD_SKILL_CAP : atoi(sep->arg[1]);

			for (EQ::skills::SkillType skill_num = EQ::skills::Skill1HBlunt; skill_num <= EQ::skills::HIGHEST_SKILL; skill_num = (EQ::skills::SkillType)(skill_num + 1))
			{
				Client* t = c->GetTarget()->CastToClient();
				if (t)
				{
					uint16 max_level = t->GetMaxSkillAfterSpecializationRules(skill_num, t->MaxSkill(skill_num));
					uint16 cap_level = level > max_level ? max_level : level;

					t->SetSkill(skill_num, cap_level);
				}
				else
				{
					c->Message(CC_Default, "Error: #setallskill: No target.");
					return;
				}
			}
		}
		else
			c->Message(CC_Default, "Error: Your status is not high enough to set anothers skills");
	}
}

void command_race(Client *c, const Seperator *sep){
	Mob *t = c->CastToMob();

	// Need to figure out max race for LoY/LDoN: going with upper bound of 500 now for testing
	if (sep->IsNumber(1) && atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 724) {
		if ((c->GetTarget()) && c->Admin() >= commandRaceOthers)
			t = c->GetTarget();
		t->SendIllusionPacket(atoi(sep->arg[1]));
	}
	else
		c->Message(CC_Default, "Usage: #race [0-724] (0 for back to normal)");
}

void command_gender(Client *c, const Seperator *sep){
	Mob *t = c->CastToMob();

	if (sep->IsNumber(1) && atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 500) {
		if ((c->GetTarget()) && c->Admin() >= commandGenderOthers)
			t = c->GetTarget();
		t->SendIllusionPacket(t->GetRace(), atoi(sep->arg[1]));
	}
	else
		c->Message(CC_Default, "Usage: #gender [0/1/2]");
}

void command_makepet(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == '\0')
		c->Message(CC_Default, "Usage: #makepet pet_type_name (will not survive across zones)");
	else
		c->MakePet(0, sep->arg[1]);
}

void command_level(Client *c, const Seperator *sep)
{
	uint16 level = atoi(sep->arg[1]);
	uint16 current_level = c->GetLevel();
	bool target = false;

	if (c->GetTarget() && ((level <= 0) ||
		((level > RuleI(Character, MaxLevel)) && (c->Admin() < commandLevelAboveCap)) ||
		(!c->GetTarget()->IsNPC() && ((c->Admin() < commandLevelNPCAboveCap) && (level > RuleI(Character, MaxLevel))))) ||
		(c->Admin() < RuleI(GM, MinStatusToLevelTarget) && level > RuleI(Character, MaxLevel)))
	{
		c->Message(CC_Default, "Error: #Level: Invalid Level");
		return;
	}
	else if (c->GetTarget())
	{
		current_level = c->GetTarget()->GetLevel();
		c->GetTarget()->SetLevel(level, true);
		target = true;
	}
	else if (c->GetTarget() && c->Admin() < RuleI(GM, MinStatusToLevelTarget) && level <= RuleI(Character, MaxLevel) && level > 0) {
		c->Message(CC_Default, "Your status level only supports self use of this command.");
		c->SetLevel(level, true);
	}
	else
	{
		c->Message(CC_Default, "No valid target selected, using command on self.");
		c->SetLevel(level, true);
	}

	if (level < current_level)
	{
		Mob* tar = !target ? c : c->GetTarget();
		if (tar->IsClient())
			c->Message(CC_Yellow, "%s must zone before their client will see the lowered level.", tar->GetName());
	}
}

void command_spawn(Client *c, const Seperator *sep){
	if (sep->arg[1][0] != 0){
		Client* client = entity_list.GetClientByName(sep->arg[1]);
		if (client){
			c->Message(CC_Default, "You cannot spawn a mob with the same name as a character!");
			return;
		}
	}

	NPC* npc = NPC::SpawnNPC(sep->argplus[1], c->GetPosition(), c);
	if (!npc) {
		c->Message(CC_Default, "Format: #spawn name race level material hp gender class priweapon secweapon merchantid bodytype - spawns a npc those parameters.");
		c->Message(CC_Default, "Name Format: NPCFirstname_NPCLastname - All numbers in a name are stripped and \"_\" characters become a space.");
		c->Message(CC_Default, "Note: Using \"-\" for gender will autoselect the gender for the race. Using \"-\" for HP will use the calculated maximum HP.");
	}
}

void command_texture(Client *c, const Seperator *sep){

	uint16 texture;
	if (sep->IsNumber(1) && atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 255) {
		texture = atoi(sep->arg[1]);
		uint8 helm = 0xFF;

		// Player Races Wear Armor, so Wearchange is sent instead
		int i;
		if (!c->GetTarget())
			for (i = EQ::textures::textureBegin; i <= EQ::textures::LastTintableTexture; i++)
			{
			c->WearChange(i, texture, 0);
			}
		else if (c->GetTarget()->IsPlayableRace(c->GetTarget()->GetRace())) {
			for (i = EQ::textures::textureBegin; i <= EQ::textures::LastTintableTexture; i++)
			{
				c->GetTarget()->WearChange(i, texture, 0);
			}
		}
		else	// Non-Player Races only need Illusion Packets to be sent for texture
		{
			if (sep->IsNumber(2) && atoi(sep->arg[2]) >= 0 && atoi(sep->arg[2]) <= 255)
				helm = atoi(sep->arg[2]);
			else
				helm = texture;

			if (texture == 255) {
				texture = 0xFFFF;	// Should be pulling these from the database instead
				helm = 0xFF;
			}

			if ((c->GetTarget()) && (c->Admin() >= commandTextureOthers))
				c->GetTarget()->SendIllusionPacket(c->GetTarget()->GetRace(), 0xFF, texture, helm);
			else
				c->SendIllusionPacket(c->GetRace(), 0xFF, texture, helm);
		}
	}
	else
		c->Message(CC_Default, "Usage: #texture [texture] [helmtexture] (0-255, 255 for show equipment)");
}

void command_npctypespawn(Client *c, const Seperator *sep){
	if (sep->IsNumber(1)) {
		const NPCType* tmp = 0;
		if ((tmp = database.GetNPCType(atoi(sep->arg[1])))) {
			//tmp->fixedZ = 1;
			auto npc = new NPC(tmp, 0, c->GetPosition(), EQ::constants::GravityBehavior::Water);
			if (npc && sep->IsNumber(2))
				npc->SetNPCFactionID(atoi(sep->arg[2]));

			npc->AddLootTable();
			entity_list.AddNPC(npc);
		}
		else
			c->Message(CC_Default, "NPC Type %i not found", atoi(sep->arg[1]));
	}
	else
		c->Message(CC_Default, "Usage: #npctypespawn npctypeid factionid");

}

void command_heal(Client *c, const Seperator *sep)
{
	auto target = c->GetTarget() ? c->GetTarget() : c;
	target->Heal();
	if (c != target) {
		c->Message(
			CC_Default,
			fmt::format(
				"Healed {} ({}) to full.",
				target->GetCleanName(),
				target->GetID()
			).c_str()
		);
	}
	else {
		c->Message(CC_Default, "Healed yourself to full.");
	}
}

void command_appearance(Client *c, const Seperator *sep){
	Mob *t = c->CastToMob();

	// sends any appearance packet
	// Dev debug command, for appearance types
	if (sep->arg[2][0] == 0)
		c->Message(CC_Default, "Usage: #appearance type value");
	else {
		if ((c->GetTarget()))
			t = c->GetTarget();
		t->SendAppearancePacket(atoi(sep->arg[1]), atoi(sep->arg[2]));
		c->Message(CC_Default, "Sending appearance packet: target=%s, type=%s, value=%s", t->GetName(), sep->arg[1], sep->arg[2]);
	}
}

void command_nukeitem(Client *c, const Seperator *sep){
	int numitems, itemid;

	if (c->GetTarget() && c->GetTarget()->IsClient() && (sep->IsNumber(1) || sep->IsHexNumber(1))) {
		itemid = sep->IsNumber(1) ? atoi(sep->arg[1]) : hextoi(sep->arg[1]);
		numitems = c->GetTarget()->CastToClient()->NukeItem(itemid);
		c->Message(CC_Default, " %u items deleted", numitems);
	}
	else
		c->Message(CC_Default, "Usage: (targted) #nukeitem itemnum - removes the item from the player's inventory");
}

void command_peekinv(Client *c, const Seperator *sep)
{
	enum {
		peekNone = 0x00,
		peekWorn = 0x01,
		peekInv = 0x02,
		peekCursor = 0x04,
		peekBank = 0x08,
		peekTrade = 0x10,
		peekWorld = 0x20,
		peekOutOfScope = (peekWorld * 2)
	};

	if (!c->GetTarget() || !c->GetTarget()->IsClient()) {
		c->Message(CC_Default, "You must have a PC target selected for this command");
		return;
	}

	int scopeWhere = peekNone;

	if (strcasecmp(sep->arg[1], "all") == 0) { scopeWhere = (peekOutOfScope); }
	else if (strcasecmp(sep->arg[1], "worn") == 0) { scopeWhere |= peekWorn; }
	else if (strcasecmp(sep->arg[1], "inv") == 0) { scopeWhere |= peekInv; }
	else if (strcasecmp(sep->arg[1], "cursor") == 0) { scopeWhere |= peekCursor; }
	else if (strcasecmp(sep->arg[1], "bank") == 0) { scopeWhere |= peekBank; }
	else if (strcasecmp(sep->arg[1], "trade") == 0) { scopeWhere |= peekTrade; }
	else if (strcasecmp(sep->arg[1], "world") == 0) { scopeWhere |= peekWorld; }

	if (scopeWhere == 0) {
		c->Message(CC_Default, "Usage: #peekinv [worn|inv|cursor|trib|bank|trade|world|all]");
		c->Message(CC_Default, "  Displays a portion of the targeted user's inventory");
		c->Message(CC_Default, "  Caution: 'all' is a lot of information!");
		return;
	}

	Client* targetClient = c->GetTarget()->CastToClient();
	const EQ::ItemInstance* inst_main = nullptr;
	const EQ::ItemInstance* inst_sub = nullptr;
	const EQ::ItemData* item_data = nullptr;
	
	EQ::SayLinkEngine linker;
	linker.SetLinkType(EQ::saylink::SayLinkItemInst);

	c->Message(CC_Default, fmt::format("Displaying inventory for {}...", targetClient->GetName()).c_str());
	
	// worn
	for (int16 indexMain = EQ::invslot::EQUIPMENT_BEGIN; (scopeWhere & peekWorn) && (indexMain <= EQ::invslot::EQUIPMENT_END); ++indexMain) {
		inst_main = targetClient->GetInv().GetItem(indexMain);
		item_data = (inst_main == nullptr) ? nullptr : inst_main->GetItem();
		linker.SetItemInst(inst_main);

				c->Message((item_data == nullptr), "WornSlot: %i, Item: %i (%s), Charges: %i",
			indexMain, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_main->GetCharges()));
	}

	// inv
	for (int16 indexMain = EQ::invslot::GENERAL_BEGIN; (scopeWhere & peekInv) && (indexMain <= EQ::invslot::GENERAL_END); ++indexMain) {
		inst_main = targetClient->GetInv().GetItem(indexMain);
		item_data = (inst_main == nullptr) ? nullptr : inst_main->GetItem();
		linker.SetItemInst(inst_main);

				c->Message((item_data == 0), fmt::format("InvSlot: {}, Item: {} ({}), Charges: {}",
			indexMain, ((item_data == 0) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == 0) ? 0 : inst_main->GetCharges())).c_str());

		for (uint8 indexSub = EQ::invbag::SLOT_BEGIN; inst_main && inst_main->IsClassBag() && (indexSub <= EQ::invbag::SLOT_END); ++indexSub) {
			inst_sub = inst_main->GetItem(indexSub);
			item_data = (inst_sub == nullptr) ? nullptr : inst_sub->GetItem();
			linker.SetItemInst(inst_sub);

			c->Message((item_data == nullptr), fmt::format("  InvBagSlot: {} (Slot #{}, Bag #{}), Item: {} ({}), Charges: {}",
				EQ::InventoryProfile::CalcSlotId(indexMain, indexSub), indexMain, indexSub, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_sub->GetCharges())).c_str());

		}
	}
	// money
	if (scopeWhere & peekInv)
		c->Message(CC_Default, fmt::format("Carried Money: {} pp {} gp {} sp {} cp", targetClient->GetPP().platinum, targetClient->GetPP().gold, targetClient->GetPP().silver, targetClient->GetPP().copper).c_str());

	// cursor
	if (scopeWhere & peekCursor) {
		if (targetClient->GetInv().CursorEmpty()) {
			linker.SetItemInst(nullptr);

			c->Message(CC_Default, fmt::format("CursorSlot: {}, Item: {} ({}), Charges: {}",
				EQ::invslot::slotCursor, 0, linker.GenerateLink().c_str(), 0).c_str());
		}
		else {
			int cursorDepth = 0;
			for (auto it = targetClient->GetInv().cursor_cbegin(); (it != targetClient->GetInv().cursor_cend()); ++it, ++cursorDepth) {
				inst_main = *it;
				item_data = (inst_main == nullptr) ? nullptr : inst_main->GetItem();
				linker.SetItemInst(inst_main);

				int16 SlotCursor = cursorDepth;
				if(SlotCursor > 0)
					SlotCursor += EQ::invslot::CURSOR_QUEUE_BEGIN;

				c->Message((item_data == 0), fmt::format("CursorSlot: {}, Depth: {}, Item: {} ({}), Charges: {}",
					SlotCursor, cursorDepth, ((item_data == 0) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == 0) ? 0 : inst_main->GetCharges())).c_str());

				for (uint8 indexSub = EQ::invbag::SLOT_BEGIN; (cursorDepth == 0) && inst_main && inst_main->IsClassBag() && (indexSub <= EQ::invbag::SLOT_END); ++indexSub) {
					inst_sub = inst_main->GetItem(indexSub);
					item_data = (inst_sub == nullptr) ? nullptr : inst_sub->GetItem();
					linker.SetItemInst(inst_sub);

					c->Message((item_data == nullptr), fmt::format("  CursorBagSlot: {} (Slot #{}, Bag #{}), Item: {} ({}), Charges: {}",
						EQ::InventoryProfile::CalcSlotId(SlotCursor, indexSub), SlotCursor, indexSub, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_sub->GetCharges())).c_str());

				}
			}
		}
		// money
		c->Message(CC_Default, fmt::format("Cursor Money: {} pp {} gp {} sp {} cp", targetClient->GetPP().platinum_cursor, targetClient->GetPP().gold_cursor, targetClient->GetPP().silver_cursor, targetClient->GetPP().copper_cursor).c_str());
	}

	// bank
	for (int16 indexMain = EQ::invslot::BANK_BEGIN; (scopeWhere & peekBank) && (indexMain <= EQ::invslot::BANK_END); ++indexMain) {
		inst_main = targetClient->GetInv().GetItem(indexMain);
		item_data = (inst_main == nullptr) ? nullptr : inst_main->GetItem();
		linker.SetItemInst(inst_main);

		c->Message((item_data == nullptr), fmt::format("BankSlot: {}, Item: {} ({}), Charges: {}",
			indexMain, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_main->GetCharges())).c_str());

		for (uint8 indexSub = EQ::invbag::SLOT_BEGIN; inst_main && inst_main->IsClassBag() && (indexSub <= EQ::invbag::SLOT_END); ++indexSub) {
			inst_sub = inst_main->GetItem(indexSub);
			item_data = (inst_sub == nullptr) ? nullptr : inst_sub->GetItem();
			linker.SetItemInst(inst_sub);

			c->Message((item_data == nullptr), fmt::format("  BankBagSlot: {} (Slot #{}, Bag #{}), Item: {} ({}), Charges: {}",
				EQ::InventoryProfile::CalcSlotId(indexMain, indexSub), indexMain, indexSub, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_sub->GetCharges())).c_str());
		}
	}
	// money
	if (scopeWhere & peekBank) 
		c->Message(CC_Default, fmt::format("Bank Money: {} pp {} gp {} sp {} cp", targetClient->GetPP().platinum_bank, targetClient->GetPP().gold_bank, targetClient->GetPP().silver_bank, targetClient->GetPP().copper_bank).c_str());


	// trade
	for (int16 indexMain = EQ::invslot::TRADE_BEGIN; (scopeWhere & peekTrade) && (indexMain <= EQ::invslot::TRADE_END); ++indexMain) {
		inst_main = targetClient->GetInv().GetItem(indexMain);
		item_data = (inst_main == nullptr) ? nullptr : inst_main->GetItem();
		linker.SetItemInst(inst_main);

		c->Message((item_data == nullptr), fmt::format("TradeSlot: {}, Item: {} ({}), Charges: {}",
			indexMain, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_main->GetCharges())).c_str());

		for (uint8 indexSub = EQ::invbag::SLOT_BEGIN; inst_main && inst_main->IsClassBag() && (indexSub <= EQ::invbag::SLOT_END); ++indexSub) {
			inst_sub = inst_main->GetItem(indexSub);
			item_data = (inst_sub == nullptr) ? nullptr : inst_sub->GetItem();
			linker.SetItemInst(inst_sub);

			c->Message((item_data == nullptr), fmt::format("  TradeBagSlot: {} (Slot #{}, Bag #{}), Item: {} ({}), Charges: {}",
				EQ::InventoryProfile::CalcSlotId(indexMain, indexSub), indexMain, indexSub, ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_sub->GetCharges())).c_str());
		}
	}

	// world
	if (scopeWhere & peekWorld) {
		Object* objectTradeskill = targetClient->GetTradeskillObject();

		if (objectTradeskill == nullptr) {
			c->Message(CC_Grey, "No world tradeskill object selected...");
		}
		else {
			c->Message(CC_Default, fmt::format("[WorldObject DBID: {} (entityid: {})]", objectTradeskill->GetDBID(), objectTradeskill->GetID()).c_str());

			for (int16 indexMain = EQ::invslot::SLOT_BEGIN; indexMain < EQ::invtype::WORLD_SIZE; ++indexMain) {
				inst_main = objectTradeskill->GetItem(indexMain);
				item_data = (inst_main == nullptr) ? nullptr : inst_main->GetItem();
				linker.SetItemInst(inst_main);

				c->Message((item_data == nullptr), fmt::format("WorldSlot: {}, Item: {} ({}), Charges: {}",
					(EQ::invslot::WORLD_BEGIN + indexMain), ((item_data == nullptr) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == 0) ? 0 : inst_main->GetCharges())).c_str());

				for (uint8 indexSub = EQ::invbag::SLOT_BEGIN; inst_main && inst_main->IsClassBag() && (indexSub <= EQ::invbag::SLOT_END); ++indexSub) {
					inst_sub = inst_main->GetItem(indexSub);
					item_data = (inst_sub == nullptr) ? nullptr : inst_sub->GetItem();
					linker.SetItemInst(inst_sub);

					c->Message((item_data == nullptr), fmt::format("  WorldBagSlot: {} (Slot #{}, Bag #{}), Item: {} ({}), Charges: {}",
						INVALID_INDEX, indexMain, indexSub, ((item_data == 0) ? 0 : item_data->ID), linker.GenerateLink().c_str(), ((item_data == nullptr) ? 0 : inst_sub->GetCharges())).c_str());

				}
			}
		}
	}
}

void command_findnpctype(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #findnpctype [search criteria]");
		return;
	}

	std::string query;

	int id = atoi((const char *)sep->arg[1]);
	if (id == 0) // If id evaluates to 0, then search as if user entered a string.
		query = StringFormat("SELECT id, name FROM npc_types WHERE name LIKE '%%%s%%' ORDER by id", sep->arg[1]);
	else // Otherwise, look for just that npc id.
		query = StringFormat("SELECT id, name FROM npc_types WHERE id = %i", id);

	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		c->Message(CC_Default, "Error querying database.");
		c->Message(CC_Default, query.c_str());
	}

	if (results.RowCount() == 0) // No matches found.
		c->Message(CC_Default, "No matches found for %s.", sep->arg[1]);

	// If query runs successfully.
	int count = 0;
	const int maxrows = 20;

	// Process each row returned.
	for (auto row = results.begin(); row != results.end(); ++row) {
		// Limit to returning maxrows rows.
		if (++count > maxrows) {
			c->Message(CC_Default, "%i npc types shown. Too many results.", maxrows);
			break;
		}

		c->Message(CC_Default, "  %s: %s", row[0], row[1]);
	}

	// If we did not hit the maxrows limit.
	if (count <= maxrows)
		c->Message(CC_Default, "Query complete. %i rows shown.", count);

}

void command_leaderboard(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #leaderboard [SFHC|SSFHC|SFHCOnly|HC]");
		return;
	}

	std::string query = "SELECT level, class, name, race, e_solo_only, e_self_found FROM character_data WHERE ";

	if(strncasecmp(sep->arg[1], "SFHCOnly", 8) == 0)
	{
		query += "e_solo_only = 0 AND e_self_found = 1 AND e_hardcore = 1";
	}
	else if(strncasecmp(sep->arg[1], "SFHC", 4) == 0)
	{
		query += "e_self_found = 1 AND e_hardcore = 1";
	}
	else if(strncasecmp(sep->arg[1], "SSFHC", 5) == 0)
	{
		query += "e_solo_only = 1 AND e_self_found = 1 AND e_hardcore = 1";
	}
	else if(strncasecmp(sep->arg[1], "HC", 2) == 0)
	{
		query += "e_hardcore = 1";
	}
	else
	{
		c->Message(CC_Default, "Usage: #leaderboard [SFHC|SSFHC|SFHCOnly|HC]");
		return;
	}

	query += " ORDER BY level DESC LIMIT 25;";

	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		c->Message(CC_Default, "Error querying database.");
		c->Message(CC_Default, query.c_str());
		return;
	}

	int count = 0;
	const int maxrows = 25;

	c->Message(CC_Default, "---------------------------");

	for (auto row = results.begin(); row != results.end(); ++row) {
		if (++count > maxrows) {
			c->Message(CC_Default, "%i players shown.", maxrows);
			break;
		}

		bool IsSolo = strcmp(row[4], "1") == 0; 
		bool IsSelfFound = strcmp(row[5], "1") == 0; 

		std::string SSFTag = "";
		if(IsSolo)
			SSFTag += "Solo ";
		if(IsSelfFound)
			SSFTag += "Self Found";

		// Convert race and class to strings
		c->Message(CC_Default, "[%s %s] %s (%s) - %s", row[0], GetClassIDName(std::stoi(row[1]), std::stoi(row[0])), row[2], GetRaceIDName(std::stoi(row[3])), SSFTag.c_str());
	}

	if (count <= maxrows)
		c->Message(CC_Default, "Query complete. %i rows shown.", count);
	else if (count == 0)
		c->Message(CC_Default, "No matches found for %s.", sep->arg[1]);
}

void command_findzone(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Usage: #findzone [search criteria]");
		return;
	}

	std::string query;
	int id = atoi((const char *)sep->arg[1]);
	if (id == 0) { // If id evaluates to 0, then search as if user entered a string.
		auto escName = new char[strlen(sep->arg[1]) * 2 + 1];
		database.DoEscapeString(escName, sep->arg[1], strlen(sep->arg[1]));

		query = StringFormat("SELECT zoneidnumber, short_name, long_name FROM zone "
			"WHERE long_name RLIKE '%s'", escName);
		safe_delete_array(escName);
	}
	else // Otherwise, look for just that zoneidnumber.
		query = StringFormat("SELECT zoneidnumber, short_name, long_name FROM zone "
		"WHERE zoneidnumber = %i", id);

	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		c->Message(CC_Default, "Error querying database.");
		c->Message(CC_Default, query.c_str());
		return;
	}

	int count = 0;
	const int maxrows = 20;

	for (auto row = results.begin(); row != results.end(); ++row) {
		if (++count > maxrows) {
			c->Message(CC_Default, "%i zones shown. Too many results.", maxrows);
			break;
		}

		c->Message(CC_Default, "  %s: %s, %s", row[0], row[1], row[2]);
	}

	if (count <= maxrows)
		c->Message(CC_Default, "Query complete. %i rows shown.", count);
	else if (count == 0)
		c->Message(CC_Default, "No matches found for %s.", sep->arg[1]);
}

void command_viewnpctype(Client *c, const Seperator *sep){
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #viewnpctype [npctype id]");
	else
	{
		uint32 npctypeid = atoi(sep->arg[1]);
		const NPCType* npct = database.GetNPCType(npctypeid);
		if (npct) {
			c->Message(CC_Default, " NPCType Info, ");
			c->Message(CC_Default, "  NPCTypeID: %u", npct->npc_id);
			c->Message(CC_Default, "  Name: %s", npct->name);
			c->Message(CC_Default, "  Level: %i", npct->level);
			c->Message(CC_Default, "  Race: %i", npct->race);
			c->Message(CC_Default, "  Class: %i", npct->class_);
			c->Message(CC_Default, "  MinDmg: %i", npct->min_dmg);
			c->Message(CC_Default, "  MaxDmg: %i", npct->max_dmg);
			c->Message(CC_Default, "  Special Abilities: %s", npct->special_abilities);
			c->Message(CC_Default, "  Spells: %i", npct->npc_spells_id);
			c->Message(CC_Default, "  Loot Table: %i", npct->loottable_id);
			c->Message(CC_Default, "  NPCFactionID: %i", npct->npc_faction_id);
		}
		else
			c->Message(CC_Default, "NPC #%d not found", npctypeid);
	}
}

void command_reloadqst(Client *c, const Seperator *sep)
{
	bool stop_timers = false;

	if (sep->IsNumber(1)) {
		stop_timers = std::stoi(sep->arg[1]) != 0 ? true : false;
	}

	std::string stop_timers_message = stop_timers ? " and stopping timers" : "";
	c->Message(
		CC_Default,
		fmt::format(
			"Clearing quest memory cache{}.",
			stop_timers_message
		).c_str()
	);
	entity_list.ClearAreas();
	parse->ReloadQuests(stop_timers);
}

void command_reloadworld(Client *c, const Seperator *sep){
	//c->Message(CC_Default, "Reloading quest cache, reloading rules, and repopping zones worldwide.");
	auto pack = new ServerPacket(ServerOP_ReloadWorld, sizeof(ReloadWorld_Struct));
	ReloadWorld_Struct* RW = (ReloadWorld_Struct*) pack->pBuffer;
	RW->Option = 0; //Keep it, maybe we'll use it in the future.
	worldserver.SendPacket(pack);
	safe_delete(pack);
	if (!worldserver.SendChannelMessage(c, 0, ChatChannel_Broadcast, 0, 0, 100, "Reloading quest cache, reloading rules, merchants, emotes, and repopping zones worldwide."))
		c->Message(CC_Default, "Error: World server disconnected");

	c->Message(CC_Yellow, "You broadcast, Reloading quest cache, reloading rules, merchants, emotes, and repopping zones worldwide.");
}

void command_reloadlevelmods(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
	{
		if (RuleB(Zone, LevelBasedEXPMods)){
			zone->LoadLevelEXPMods();
			c->Message(CC_Yellow, "Level based EXP Mods have been reloaded zonewide");
		}
		else{
			c->Message(CC_Yellow, "Level based EXP Mods are disabled in rules!");
		}
	}
}

void command_reloadzps(Client *c, const Seperator *sep){
	database.LoadStaticZonePoints(&zone->zone_point_list, zone->GetShortName());
	c->Message(CC_Default, "Reloading server zone_points.");
}

void command_zoneshutdown(Client *c, const Seperator *sep){
	if (!worldserver.Connected())
		c->Message(CC_Default, "Error: World server disconnected");
	else if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #zoneshutdown zoneshortname");
	else {
		auto pack = new ServerPacket(ServerOP_ZoneShutdown, sizeof(ServerZoneStateChange_struct));
		ServerZoneStateChange_struct* s = (ServerZoneStateChange_struct *)pack->pBuffer;
		strcpy(s->adminname, c->GetName());
		if (sep->arg[1][0] >= '0' && sep->arg[1][0] <= '9')
			s->ZoneServerID = atoi(sep->arg[1]);
		else
			s->zoneid = database.GetZoneID(sep->arg[1]);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void command_zonebootup(Client *c, const Seperator *sep)
{
	if (!worldserver.Connected())
	{
		c->Message(CC_Default, "Error: World server disconnected");
	}
	else if (sep->arg[1][0] == 0)
	{
		auto pack = new ServerPacket(ServerOP_BootDownZones, sizeof(ServerDownZoneBoot_struct));
		ServerDownZoneBoot_struct* s = (ServerDownZoneBoot_struct *)pack->pBuffer;
		strcpy(s->adminname, c->GetName());
		worldserver.SendPacket(pack);
		safe_delete(pack);
		c->Message(CC_Default, "Zonebootup completed.");
	}
	else 
	{
		auto pack = new ServerPacket(ServerOP_ZoneBootup, sizeof(ServerZoneStateChange_struct));
		ServerZoneStateChange_struct* s = (ServerZoneStateChange_struct *)pack->pBuffer;
		s->ZoneServerID = atoi(sep->arg[2]);
		strcpy(s->adminname, c->GetName());
		s->zoneid = database.GetZoneID(sep->arg[1]);
		s->makestatic = true;
		worldserver.SendPacket(pack);
		safe_delete(pack);
		c->Message(CC_Default, "Zonebootup completed.");
	}
}

void command_kick(Client *c, const Seperator *sep){
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #kick [Character Name]");
		return;
	}
	else {
		std::string character_name = sep->arg[1];
		auto client = entity_list.GetClientByName(character_name.c_str());

		if (!worldserver.Connected())
			c->Message(CC_Default, "The world server is currently disconnected.");
		else {
			auto pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
			ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)pack->pBuffer;
			strcpy(skp->adminname, c->GetName());
			strcpy(skp->name, character_name.c_str());
			skp->adminrank = c->Admin();
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
		if (client) {
			if (client->Admin() <= c->Admin()) {
				client->Message(CC_Default, 
					fmt::format(
						"You have been kicked by {} ", 
						c->GetName()
					).c_str()
				);
				client->WorldKick();
				c->Message(CC_Default, 
					fmt::format(
					"{} has been kicked from the server.",
					character_name
					).c_str()
				);
			}
		}
	}
}

void command_attack(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetTarget()->IsNPC() && sep->arg[1] != 0) {
		Mob* sictar = entity_list.GetMob(sep->argplus[1]);
		if (sictar)
			c->GetTarget()->CastToNPC()->AddToHateList(sictar, 1, 0);
		else
			c->Message(CC_Default, "Error: %s not found", sep->arg[1]);
	}
	else
		c->Message(CC_Default, "Usage: (needs NPC targeted) #attack targetname");
}

void command_attackentity(Client *c, const Seperator *sep) {
	if (c->GetTarget() && c->GetTarget()->IsNPC() && atoi(sep->arg[1]) > 0) {
		Mob* sictar = entity_list.GetMob(atoi(sep->arg[1]));
		if (sictar)
			c->GetTarget()->CastToNPC()->AddToHateList(sictar, 1, 0);
		else
			c->Message(CC_Default, "Error: Entity %d not found", atoi(sep->arg[1]));
	}
	else
		c->Message(CC_Default, "Usage: (needs NPC targeted) #attackentity entityid");
}

void command_serverlock(Client* c, const Seperator* sep)
{
	if (!sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #serverlock [0|1] - Lock or Unlock the World Server (0 = Unlocked, 1 = Locked)");
		return;
	}

	auto is_locked = std::stoi(sep->arg[1]) ? true : false;

	auto pack = new ServerPacket(ServerOP_Lock, sizeof(ServerLock_Struct));
	auto l = (ServerLock_Struct*)pack->pBuffer;
	strn0cpy(l->character_name, c->GetCleanName(), sizeof(l->character_name));
	l->is_locked = is_locked;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void command_motd(Client *c, const Seperator *sep)
{
	auto pack = new ServerPacket(ServerOP_Motd, sizeof(ServerMotd_Struct));
	auto mss = (ServerMotd_Struct*)pack->pBuffer;
	strn0cpy(mss->myname, c->GetName(), sizeof(mss->myname));
	strn0cpy(mss->motd, sep->argplus[1], sizeof(mss->motd));
	c->Message(CC_Default, "MoTD is set.");
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void command_equipitem(Client *c, const Seperator *sep){
	uint32 slot_id = atoi(sep->arg[1]);
	if (sep->IsNumber(1) && ((slot_id >= EQ::invslot::EQUIPMENT_BEGIN) && (slot_id <= EQ::invslot::EQUIPMENT_END))) {
		const EQ::ItemInstance* from_inst = c->GetInv().GetItem(EQ::invslot::slotCursor);
		const EQ::ItemInstance* to_inst = c->GetInv().GetItem(slot_id); // added (desync issue when forcing stack to stack)
		bool partialmove = false;
		int16 movecount;

		if (from_inst && from_inst->IsClassCommon()) {
			auto outapp = new EQApplicationPacket(OP_MoveItem, sizeof(MoveItem_Struct));
			MoveItem_Struct* mi = (MoveItem_Struct*)outapp->pBuffer;
			mi->from_slot = EQ::invslot::slotCursor;
			mi->to_slot = slot_id;
			// mi->number_in_stack	= from_inst->GetCharges(); // replaced with con check for stacking

			// crude stackable check to only 'move' the difference count on client instead of entire stack when applicable
			if (to_inst && to_inst->IsStackable() &&
				(to_inst->GetItem()->ID == from_inst->GetItem()->ID) &&
				(to_inst->GetCharges() < to_inst->GetItem()->StackSize) &&
				(from_inst->GetCharges() > to_inst->GetItem()->StackSize - to_inst->GetCharges())) {
				movecount = to_inst->GetItem()->StackSize - to_inst->GetCharges();
				mi->number_in_stack = (uint32)movecount;
				partialmove = true;
			}
			else
				mi->number_in_stack = from_inst->GetCharges();

			// Save move changes
			// Added conditional check to packet send..would have sent change even on a swap failure..whoops!

			if (partialmove) { // remove this con check if someone can figure out removing charges from cursor stack issue below
				// mi->number_in_stack is always from_inst->GetCharges() when partialmove is false
				c->Message(CC_Red, "Error: Partial stack added to existing stack exceeds allowable stacksize");
				safe_delete(outapp);
				return;
			}
			else if (c->SwapItem(mi) == 1) {
				c->FastQueuePacket(&outapp);
				return;
			}
			else {
				c->Message(CC_Red, "Error: Unable to equip current item");
			}
			safe_delete(outapp);

			// also send out a wear change packet?
		}
		else if (from_inst == nullptr)
			c->Message(CC_Red, "Error: There is no item on your cursor");
		else
			c->Message(CC_Red, "Error: Item on your cursor cannot be equipped");
	}
	else
		c->Message(CC_Default, "Usage: #equipitem slotid[0-21] - equips the item on your cursor to the position");
}

void command_zonelock(Client *c, const Seperator *sep){
	auto pack = new ServerPacket(ServerOP_LockZone, sizeof(ServerLockZone_Struct));
	ServerLockZone_Struct* lock_zone = (ServerLockZone_Struct*)pack->pBuffer;
	strn0cpy(lock_zone->adminname, c->GetName(), sizeof(lock_zone->adminname));
	if (strcasecmp(sep->arg[1], "list") == 0) {
		lock_zone->op = ServerLockType::List;
		worldserver.SendPacket(pack);
	}
	else if (strcasecmp(sep->arg[1], "lock") == 0 && c->Admin() >= commandLockZones) {
		uint16 tmp = database.GetZoneID(sep->arg[2]);
		if (tmp) {
			lock_zone->op = ServerLockType::Lock;
			lock_zone->zoneID = tmp;
			worldserver.SendPacket(pack);
		}
		else
			c->Message(CC_Default, "Usage: #zonelock lock [zonename]");
	}
	else if (strcasecmp(sep->arg[1], "unlock") == 0 && c->Admin() >= commandLockZones) {
		uint16 tmp = database.GetZoneID(sep->arg[2]);
		if (tmp) {
			lock_zone->op = ServerLockType::Unlock;
			lock_zone->zoneID = tmp;
			worldserver.SendPacket(pack);
		}
		else
			c->Message(CC_Default, "Usage: #zonelock unlock [zonename]");
	}
	else {
		c->Message(CC_Default, "#zonelock sub-commands");
		c->Message(CC_Default, "  list");
		if (c->Admin() >= commandLockZones)
		{
			c->Message(CC_Default, "  lock [zonename]");
			c->Message(CC_Default, "  unlock [zonename]");
		}
	}
	safe_delete(pack);
}

void command_showquake(Client *c, const Seperator *sep)
{
	if (!c)
		return;
	
	if (c->GuildID() == GUILD_NONE)
	{
		c->Message(CC_Default, "You must be part of a guild to use this command.");
		return;
	}

	if (c->GuildRank() == 0)
	{
		c->Message(CC_Default, "You must be an officer rank or higher to use this command.");
		return;
	}

	if (zone)
	{
		ServerEarthquakeImminent_Struct quake_struct;
		memset(&quake_struct, 0, sizeof(ServerEarthquakeImminent_Struct));
		database.LoadQuakeData(quake_struct);
		int64 nextQuakeTime = quake_struct.next_start_timestamp;
		int64 curTime = Timer::GetTimeSeconds();

		if (nextQuakeTime - curTime > 0)
		{
			std::string time_str = "The next earthquake will begin in ";
			time_str += Strings::SecondsToTime(nextQuakeTime - curTime);
			time_str += "";
			c->Message(CC_Yellow, time_str.c_str());
		}
	}
}

void command_showlootlockouts(Client *c, const Seperator *sep)
{
	if (!c)
		return;

	int64_t curTime = Timer::GetTimeSeconds();


	c->Message(CC_LightGreen, "=== Current Loot Lockouts ===");

	for (auto lockout : c->loot_lockouts)
	{
		int64_t time_remaining = lockout.second.expirydate - curTime;
		if (time_remaining >= 1)
		{
			c->Message(CC_Red, "== %s: Expires in %s", lockout.second.npc_name, Strings::SecondsToTime((int)time_remaining).c_str());
		}
		else
		{
			if (time_remaining <= 1)
			{
				c->Message(CC_LightGreen, "== %s: Available", lockout.second.npc_name);
			}
		}
	}


	c->Message(CC_LightGreen, "=== Current Legacy Item Lockouts ===");

	for (auto legacyitem : c->looted_legacy_items)
	{
		const EQ::ItemData* item = database.GetItem(legacyitem.first);

		if (!item)
			continue;

		if (!item->Name[0])
			continue;

		int64_t time_remaining = 0xFFFFFFFFFFFFFFFF;
		if (legacyitem.second.expirydate != 0)
		{
			time_remaining = legacyitem.second.expirydate - curTime;
		}

		//Magic number
		if (time_remaining == 0xFFFFFFFFFFFFFFFF) // Never unlocks
		{
			c->Message(CC_Red, "== %s: Never Expires", item->Name);
		}
		else if (time_remaining >= 1) // Lockout present
		{
			c->Message(CC_Red, "== %s: Expires in %s", item->Name, Strings::SecondsToTime((int)time_remaining).c_str());
		}
		else //Available
		{
			c->Message(CC_LightGreen, "== %s: Available", item->Name);
		}
	}
}

void command_corpse(Client *c, const Seperator *sep)
{
	std::string help0 = "#Corpse commands usage:";
	std::string help1 = "  #corpse buriedcount - Get the target's total number of buried player corpses.";
	std::string help2 = "  #corpse buriedsummon - Summons the target's oldest buried corpse, if any exist.";
	std::string help3 = "  #corpse charid [charid] - Change player corpse's owner.";
	std::string help4 = "  #corpse delete - Delete targetted corpse.";
	std::string help5 = "  #corpse deletenpccorpses - Delete all NPC corpses.";
	std::string help6 = "  #corpse deleteplayercorpses - Delete all player corpses.";
	std::string help7 = "  #corpse depop - Depops single target corpse. Optional arg [bury].";
	std::string help8 = "  #corpse depopall - Depops all target player's corpses. Optional arg [bury].";
	std::string help9 = "  - Set bury to 0 to skip burying the corpses for the above.";
	std::string help10 = "  #corpse inspect - Inspect contents of target corpse.";
	std::string help11 = "  #corpse list - List corpses for target.";
	std::string help12 = "  #corpse locate - Locates targetted player corpses. zone and loc.";
	std::string help13 = "  #corpse lock - Locks targetted corpse. Only GMs can loot locked corpses.";
	std::string help14 = "  #corpse unlock - Unlocks targetted corpse. Only GMs can loot locked corpses.";
	std::string help15 = "  #corpse removecash - Removes cash from targetted corpse.";
	std::string help16 = "  - To remove items from corpses, lock and loot them.";
	std::string help17 = "  #corpse reset - Resets looter status on targetted corpse for debugging.";
	std::string help18 = "  #corpse backups - List of current target's corpse backups.";
	std::string help19 = "  #corpse restore [corpse_id] - Summons the specified corpse from a player's backups.";
	std::string help20 = "  #corpse summonall [name] - Summons all of [name]'s oldest buried corpse, if any exist.";
	std::string help[] = { help0, help1, help2, help3, help4, help5, help6, help7, help8, help9, help10, help11, help12, help13, help14, help15, help16, help17, help18, help19, help20 };

	Mob *target = c->GetTarget();

	if (strcasecmp(sep->arg[1], "help") == 0)
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
	else if (strcasecmp(sep->arg[1], "buriedcount") == 0)
	{
		Client *t = c;

		if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
			t = c->GetTarget()->CastToClient();
		else
		{
			c->Message(CC_Default, "You must first select a target!");
			return;
		}

		uint32 CorpseCount = database.GetCharacterBuriedCorpseCount(t->CharacterID());

		if (CorpseCount > 0)
			c->Message(CC_Default, "Your target has a total of %u buried corpses.", CorpseCount);
		else
			c->Message(CC_Default, "Your target doesn't have any buried corpses.");

		return;
	}
	else if (strcasecmp(sep->arg[1], "buriedsummon") == 0)
	{
		if (c->Admin() >= commandEditPlayerCorpses)
		{
			Client *t = c;

			if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
				t = c->GetTarget()->CastToClient();
			else
			{
				c->Message(CC_Default, "You must first turn your GM flag on and select a target!");
				return;
			}

			Corpse* PlayerCorpse = database.SummonBuriedCharacterCorpses(t->CharacterID(), t->GetZoneID(), zone->GetGuildID(), t->GetPosition());

			if (!PlayerCorpse)
				c->Message(CC_Default, "Your target doesn't have any buried corpses.");

			return;
		}
		else
			c->Message(CC_Default, "Insufficient status to summon buried corpses.");
	}
	else if (strcasecmp(sep->arg[1], "charid") == 0)
	{
		if (c->Admin() >= commandEditPlayerCorpses)
		{
			if (target == 0 || !target->IsPlayerCorpse())
				c->Message(CC_Default, "Error: Target must be a player corpse to set ID.");
			else if (!sep->IsNumber(2))
				c->Message(CC_Default, "Error: charid must be a number.");
			else
				c->Message(CC_Default, "Setting CharID=%u on PlayerCorpse '%s'", target->CastToCorpse()->SetCharID(atoi(sep->arg[2])), target->GetName());
		}
		else
			c->Message(CC_Default, "Insufficient status to change corpse owner.");
	}
	else if (strcasecmp(sep->arg[1], "delete") == 0)
	{
		if (target == 0 || !target->IsCorpse())
			c->Message(CC_Default, "Error: Target the corpse you wish to delete");
		else if (target->IsNPCCorpse())
		{
			c->Message(CC_Default, "Depoping %s.", target->GetName());
			target->CastToCorpse()->DepopNPCCorpse();
		}
		else if (c->Admin() >= commandEditPlayerCorpses)
		{
			c->Message(CC_Default, "Deleting %s.", target->GetName());
			target->CastToCorpse()->Delete();
		}
		else
			c->Message(CC_Default, "Insufficient status to delete player corpse.");
	}
	else if (strcasecmp(sep->arg[1], "deletenpccorpses") == 0)
	{
		int32 tmp = entity_list.DeleteNPCCorpses();
		if (tmp >= 0)
			c->Message(CC_Default, "%d corpses deleted.", tmp);
		else
			c->Message(CC_Default, "DeletePlayerCorpses Error #%d", tmp);
	}
	else if (strcasecmp(sep->arg[1], "deleteplayercorpses") == 0)
	{
		if (c->Admin() >= commandEditPlayerCorpses)
		{
			int32 tmp = entity_list.DeletePlayerCorpses();
			if (tmp >= 0)
				c->Message(CC_Default, "%i corpses deleted.", tmp);
			else
				c->Message(CC_Default, "DeletePlayerCorpses Error #%i", tmp);
		}
		else
			c->Message(CC_Default, "Insufficient status to delete player corpse.");
	}
	else if (strcasecmp(sep->arg[1], "depop") == 0)
	{
		if (target == 0 || !target->IsPlayerCorpse())
			c->Message(CC_Default, "Error: Target must be a player corpse to depop.");
		else if (c->Admin() >= commandEditPlayerCorpses && target->IsPlayerCorpse())
		{
			c->Message(CC_Default, "Depoping %s.", target->GetName());
			target->CastToCorpse()->DepopPlayerCorpse();
			if (!sep->arg[2][0] || atoi(sep->arg[2]) != 0)
				target->CastToCorpse()->Bury();
		}
		else
			c->Message(CC_Default, "Insufficient status to depop player corpse.");
	}
	else if (strcasecmp(sep->arg[1], "depopall") == 0)
	{
		if (target == 0 || !target->IsClient())
			c->Message(CC_Default, "Error: Target must be a player to depop their corpses.");
		else if (c->Admin() >= commandEditPlayerCorpses && target->IsClient())
		{
			c->Message(CC_Default, "Depoping %s\'s corpses.", target->GetName());
			target->CastToClient()->DepopAllCorpses();
			if (!sep->arg[2][0] || atoi(sep->arg[2]) != 0)
				target->CastToClient()->BuryPlayerCorpses();
		}
		else
			c->Message(CC_Default, "Insufficient status to depop player corpses.");
	}
	else if (strcasecmp(sep->arg[1], "inspect") == 0)
	{
		if (target == 0 || !target->IsCorpse())
			c->Message(CC_Default, "Error: Target must be a corpse to inspect.");
		else
			target->CastToCorpse()->QueryLoot(c);
	}
	else if (strcasecmp(sep->arg[1], "list") == 0)
	{
		if(!target || (target && target->IsNPC()))
		{
			entity_list.ListNPCCorpses(c);
		}
		else if(target && target->IsClient())
		{
			entity_list.ListPlayerCorpses(c);
		}
		else
			c->Message(CC_Yellow, "Please select a NPC or Client to list corpses of that type.");
			
	}
	else if (strcasecmp(sep->arg[1], "locate") == 0)
	{
		if (target == 0 || !target->IsClient())
			c->Message(CC_Default, "Error: Target must be a player to locate their corpses.");
		else
		{
			c->Message(CC_Red, "CorpseID : Zone , x , y , z , Buried");
			std::string query = StringFormat("SELECT id, zone_id, x, y, z, is_buried, zone_guild_id FROM character_corpses WHERE charid = %d", target->CastToClient()->CharacterID());
			auto results = database.QueryDatabase(query);

			if (!results.Success() || results.RowCount() == 0)
			{
				c->Message(CC_Red, "No corpses exist for %s with ID: %i.", target->GetName(), target->CastToClient()->CharacterID());
				return;
			}

			for (auto row = results.begin(); row != results.end(); ++row)
			{

				c->Message(CC_Yellow, " %s:	%s (%s), %s, %s, %s, (%s)", row[0], database.GetZoneName(atoi(row[1])), row[6], row[2], row[3], row[4], row[5]);
			}
		}
	}
	else if (strcasecmp(sep->arg[1], "lock") == 0)
	{
		if (target == 0 || !target->IsCorpse())
			c->Message(CC_Default, "Error: Target must be a corpse in order to lock.");
		else {
			target->CastToCorpse()->Lock();
			c->Message(CC_Default, "Locking %s...", target->GetName());
		}
	}
	else if (strcasecmp(sep->arg[1], "unlock") == 0)
	{
		if (target == 0 || !target->IsCorpse())
			c->Message(CC_Default, "Error: Target must be a corpse in order to unlock.");
		else {
			target->CastToCorpse()->UnLock();
			c->Message(CC_Default, "Unlocking %s...", target->GetName());
		}
	}
	else if (strcasecmp(sep->arg[1], "removecash") == 0)
	{
		if (target == 0 || !target->IsCorpse())
			c->Message(CC_Default, "Error: Target the corpse you wish to remove the cash from");
		else if (!target->IsPlayerCorpse() || c->Admin() >= commandEditPlayerCorpses)
		{
			c->Message(CC_Default, "Removing Cash from %s.", target->GetName());
			target->CastToCorpse()->RemoveCash();
		}
		else
			c->Message(CC_Default, "Insufficient status to modify cash on player corpse.");
	}
	else if (strcasecmp(sep->arg[1], "reset") == 0)
	{
		if (target == 0 || !target->IsCorpse())
			c->Message(CC_Default, "Error: Target the corpse you wish to reset");
		else
			target->CastToCorpse()->ResetLooter();
	}
	else if (strcasecmp(sep->arg[1], "backups") == 0)
	{

		if (target == 0 || !target->IsClient())
			c->Message(CC_Default, "Error: Target must be a player to list their backups.");
		else
		{
			uint32 charid = 0;
			if (!sep->arg[2][0])
			{
				charid = target->CastToClient()->CharacterID();
			}
			else
				charid = database.GetCharacterID(sep->arg[2]);


			c->Message(CC_Red, "CorpseID : Zone , Guild, x , y , z , Items");
			std::string query = StringFormat("SELECT id, zone_id, x, y, z, zone_guild_id FROM character_corpses_backup WHERE charid = %d", charid);
			auto results = database.QueryDatabase(query);

			if (!results.Success() || results.RowCount() == 0)
			{
				c->Message(CC_Red, "No corpse backups exist for %s with ID: %i.", sep->arg[3], charid);
				return;
			}

			for (auto row = results.begin(); row != results.end(); ++row)
			{
				std::string ic_query = StringFormat("SELECT COUNT(*) FROM character_corpse_items_backup WHERE corpse_id = %d", atoi(row[0]));
				auto ic_results = database.QueryDatabase(ic_query);
				auto ic_row = ic_results.begin();

				c->Message(CC_Yellow, " %s:	%s (%s), %s, %s, %s, (%s)", row[0], database.GetZoneName(atoi(row[1])), row[5], row[2], row[3], row[4], ic_row[0]);
			}
		}
	}
	else if (strcasecmp(sep->arg[1], "restore") == 0)
	{
		if (c->Admin() >= commandEditPlayerCorpses)
		{
			uint32 corpseid;
			Client *t = c;

			if (!sep->IsNumber(2))
			{
				c->Message(CC_Default, "Usage: #corpse restore [corpse_id].");
				return;
			}
			else
				corpseid = atoi(sep->arg[2]);

			if(!database.IsValidCorpseBackup(corpseid))
			{
				c->Message(CC_Red, "Backup corpse %i not found.", corpseid);
				return;
			}
			else if(database.IsValidCorpse(corpseid))
			{
				c->Message(CC_Red, "Corpse %i has been found! Please summon or delete it before attempting to restore from a backup.", atoi(sep->arg[2]));
				return;
			}
			else
			{
				if(database.CopyBackupCorpse(corpseid))
				{
					Corpse* PlayerCorpse = database.SummonCharacterCorpse(corpseid, 0, zone->GetZoneID(), zone->GetGuildID(), c->GetPosition());

					if (!PlayerCorpse)
						c->Message(CC_Default, "Summoning of backup corpse failed. Please escalate this issue.");

					return;
				}
				else
				{
					c->Message(CC_Red, "There was an error copying corpse %i. Please contact a DB admin.", corpseid);
					return;
				}
			}
		}
		else
		{
			c->Message(CC_Default, "Insufficient status to summon backup corpses.");
		}
	}
	else if (strcasecmp(sep->arg[1], "summonall") == 0)
	{
		if (sep->arg[2][0] != 0 && !sep->IsNumber(2))
		{
			std::string summon_corpse_char_name = sep->arg[2];
			auto corpse_list_copy = entity_list.GetCorpseList();
			int nCorpseCount = 0;
			for (auto corpse : corpse_list_copy)
			{
				Corpse* pCorpse = corpse.second;
				if (pCorpse && pCorpse->IsPlayerCorpse())
				{
					if (strcmp(pCorpse->GetOwnerName(), summon_corpse_char_name.c_str()) == 0)
					{
						pCorpse->GMMove(c->GetX(), c->GetY(), c->GetZ(), c->GetHeading());
						nCorpseCount++;
					}
				}
			}
			if (nCorpseCount > 0)
			{
				c->Message(CC_Default, "Summoned %d corpses to your location from player %s", nCorpseCount, summon_corpse_char_name.c_str());
			}
			else
			{
				c->Message(CC_Red, "No corpses with name %s exist in this zone.", summon_corpse_char_name.c_str());
			}
		}
		else
		{
			c->Message(CC_Red, "Invalid character ID or parameter count for #corpse summonall");
		}
	}
	else
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
}

void command_fixmob(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	const char* Usage = "Usage: #fixmob [race|gender|texture|helm|face|hair|haircolor|beard|beardcolor|heritage|tattoo|detail] [next|prev]";

	if (!sep->arg[1])
		c->Message(CC_Default, Usage);
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else
	{

		uint32 Adjustment = 1;	// Previous or Next
		char codeMove = 0;

		if (sep->arg[2])
		{
			char* command2 = sep->arg[2];
			codeMove = (command2[0] | 0x20); // First character, lower-cased
			if (codeMove == 'n')
				Adjustment = 1;
			else if (codeMove == 'p')
				Adjustment = -1;
		}

		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = 0xFF;
		uint8 HairColor = target->GetHairColor();
		uint8 BeardColor = target->GetBeardColor();
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = target->GetHairStyle();
		uint8 LuclinFace = target->GetLuclinFace();
		uint8 Beard = target->GetBeard();

		const char* ChangeType = nullptr; // If it's still nullptr after processing, they didn't send a valid command
		uint32 ChangeSetting;
		char* command = sep->arg[1];

		if (strcasecmp(command, "race") == 0)
		{
			if (Race == 1 && codeMove == 'p')
				Race = 724;
			else if (Race >= 724 && codeMove != 'p')
				Race = 1;
			else
				Race += Adjustment;
			ChangeType = "Race";
			ChangeSetting = Race;
		}
		else if (strcasecmp(command, "gender") == 0)
		{
			if (Gender == 0 && codeMove == 'p')
				Gender = 2;
			else if (Gender >= 2 && codeMove != 'p')
				Gender = 0;
			else
				Gender += Adjustment;
			ChangeType = "Gender";
			ChangeSetting = Gender;
		}
		else if (strcasecmp(command, "texture") == 0)
		{
			Texture = target->GetTexture();

			if (Texture == 0 && codeMove == 'p')
				Texture = 25;
			else if (Texture >= 25 && codeMove != 'p')
				Texture = 0;
			else
				Texture += Adjustment;
			ChangeType = "Texture";
			ChangeSetting = Texture;
		}
		else if (strcasecmp(command, "helm") == 0)
		{
			HelmTexture = target->GetHelmTexture();
			if (HelmTexture == 0 && codeMove == 'p')
				HelmTexture = 25;
			else if (HelmTexture >= 25 && codeMove != 'p')
				HelmTexture = 0;
			else
				HelmTexture += Adjustment;
			ChangeType = "HelmTexture";
			ChangeSetting = HelmTexture;
		}
		else if (strcasecmp(command, "face") == 0)
		{
			if (LuclinFace == 0 && codeMove == 'p')
				LuclinFace = 87;
			else if (LuclinFace >= 87 && codeMove != 'p')
				LuclinFace = 0;
			else
				LuclinFace += Adjustment;
			ChangeType = "LuclinFace";
			ChangeSetting = LuclinFace;
		}
		else if (strcasecmp(command, "hair") == 0)
		{
			if (HairStyle == 0 && codeMove == 'p')
				HairStyle = 8;
			else if (HairStyle >= 8 && codeMove != 'p')
				HairStyle = 0;
			else
				HairStyle += Adjustment;
			ChangeType = "HairStyle";
			ChangeSetting = HairStyle;
		}
		else if (strcasecmp(command, "haircolor") == 0)
		{
			if (HairColor == 0 && codeMove == 'p')
				HairColor = 24;
			else if (HairColor >= 24 && codeMove != 'p')
				HairColor = 0;
			else
				HairColor += Adjustment;
			ChangeType = "HairColor";
			ChangeSetting = HairColor;
		}
		else if (strcasecmp(command, "beard") == 0)
		{
			if (Beard == 0 && codeMove == 'p')
				Beard = 11;
			else if (Beard >= 11 && codeMove != 'p')
				Beard = 0;
			else
				Beard += Adjustment;
			ChangeType = "Beard";
			ChangeSetting = Beard;
		}
		else if (strcasecmp(command, "beardcolor") == 0)
		{
			if (BeardColor == 0 && codeMove == 'p')
				BeardColor = 24;
			else if (BeardColor >= 24 && codeMove != 'p')
				BeardColor = 0;
			else
				BeardColor += Adjustment;
			ChangeType = "BeardColor";
			ChangeSetting = BeardColor;
		}

		// Hack to fix some races that base features from face
		switch (Race)
		{
		case 2:	// Barbarian
		case 3: // Erudite
			if (LuclinFace > 10) {
				LuclinFace -= ((HairStyle - 1) * 10);
			}
			LuclinFace += (HairStyle * 10);
			break;
		case 5: // HighElf
		case 6: // DarkElf
		case 7: // HalfElf
			if (LuclinFace > 10) {
				LuclinFace -= ((Beard - 1) * 10);
			}
			LuclinFace += (Beard * 10);
			break;
		default:
			break;
		}


		if (ChangeType == nullptr)
		{
			c->Message(CC_Default, Usage);
		}
		else
		{
			target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
				EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

			c->Message(CC_Default, "%s=%i", ChangeType, ChangeSetting);
		}
	}
}

void command_gmspeed(Client *c, const Seperator *sep)
{
	bool   state = atobool(sep->arg[1]);
	Client* t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		t = c->GetTarget()->CastToClient();
	}

	if (sep->arg[1][0] != 0) {
		database.SetGMSpeed(t->AccountID(), state ? 1 : 0);
		c->Message(CC_Default, fmt::format("Turning GMSpeed {} for {} (zone to take effect)", state ? "On" : "Off", t->GetName()).c_str());
	}
	else {
		c->Message(CC_Default, "Usage: #gmspeed [on/off]");
	}
}

void command_title(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #title [remove|text] [1 = Create row in title table] - remove or set title to 'text'");
	else {
		bool Save = (atoi(sep->arg[2]) == 1);

		Mob *target_mob = c->GetTarget();
		if (!target_mob)
			target_mob = c;
		if (!target_mob->IsClient()) {
			c->Message(CC_Red, "#title only works on players.");
			return;
		}
		Client *t = target_mob->CastToClient();

		if (strlen(sep->arg[1]) > 31) {
			c->Message(CC_Red, "Title must be 31 characters or less.");
			return;
		}

		bool removed = false;
		if (!strcasecmp(sep->arg[1], "remove")) {
			t->SetAATitle("");
			removed = true;
		}
		else {
			for (unsigned int i = 0; i<strlen(sep->arg[1]); i++)
				if (sep->arg[1][i] == '_')
					sep->arg[1][i] = ' ';
			if (!Save)
				t->SetAATitle(sep->arg[1]);
			else
				title_manager.CreateNewPlayerTitle(t, sep->arg[1]);
		}

		t->Save();

		if (removed) {
			c->Message(CC_Red, "%s's title has been removed.", t->GetName(), sep->arg[1]);
			if (t != c)
				t->Message(CC_Red, "Your title has been removed.", sep->arg[1]);
		}
		else {
			c->Message(CC_Red, "%s's title has been changed to '%s'.", t->GetName(), sep->arg[1]);
			if (t != c)
				t->Message(CC_Red, "Your title has been changed to '%s'.", sep->arg[1]);
		}
	}
}

void command_titlesuffix(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #titlesuffix [remove|text] [1 = create row in title table] - remove or set title suffix to 'text'");
	else {
		bool Save = (atoi(sep->arg[2]) == 1);

		Mob *target_mob = c->GetTarget();
		if (!target_mob)
			target_mob = c;
		if (!target_mob->IsClient()) {
			c->Message(CC_Red, "#titlesuffix only works on players.");
			return;
		}
		Client *t = target_mob->CastToClient();

		if (strlen(sep->arg[1]) > 31) {
			c->Message(CC_Red, "Title suffix must be 31 characters or less.");
			return;
		}

		bool removed = false;
		if (!strcasecmp(sep->arg[1], "remove")) {
			t->SetTitleSuffix("");
			removed = true;
		}
		else {
			for (unsigned int i = 0; i<strlen(sep->arg[1]); i++)
				if (sep->arg[1][i] == '_')
					sep->arg[1][i] = ' ';

			if (!Save)
				t->SetTitleSuffix(sep->arg[1]);
			else
				title_manager.CreateNewPlayerSuffix(t, sep->arg[1]);
		}

		t->Save();

		if (removed) {
			c->Message(CC_Red, "%s's title suffix has been removed.", t->GetName(), sep->arg[1]);
			if (t != c)
				t->Message(CC_Red, "Your title suffix has been removed.", sep->arg[1]);
		}
		else {
			c->Message(CC_Red, "%s's title suffix has been changed to '%s'.", t->GetName(), sep->arg[1]);
			if (t != c)
				t->Message(CC_Red, "Your title suffix has been changed to '%s'.", sep->arg[1]);
		}
	}
}

void command_spellinfo(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #spellinfo [spell_id]");
	else {
		short int spell_id = atoi(sep->arg[1]);
		const struct SPDat_Spell_Struct *s = &spells[spell_id];
		c->Message(CC_Default, "Spell info for spell #%d:", spell_id);
		c->Message(CC_Default, "  name: %s", s->name);
		c->Message(CC_Default, "  player_1: %s", s->player_1);
		c->Message(CC_Default, "  teleport_zone: %s", s->teleport_zone);
		c->Message(CC_Default, "  you_cast: %s", s->you_cast);
		c->Message(CC_Default, "  other_casts: %s", s->other_casts);
		c->Message(CC_Default, "  cast_on_you: %s", s->cast_on_you);
		c->Message(CC_Default, "  spell_fades: %s", s->spell_fades);
		c->Message(CC_Default, "  range: %f", s->range);
		c->Message(CC_Default, "  aoerange: %f", s->aoerange);
		c->Message(CC_Default, "  pushback: %f", s->pushback);
		c->Message(CC_Default, "  pushup: %f", s->pushup);
		c->Message(CC_Default, "  cast_time: %d", s->cast_time);
		c->Message(CC_Default, "  recovery_time: %d", s->recovery_time);
		c->Message(CC_Default, "  recast_time: %d", s->recast_time);
		c->Message(CC_Default, "  buffdurationformula: %d", s->buffdurationformula);
		c->Message(CC_Default, "  buffduration: %d", s->buffduration);
		c->Message(CC_Default, "  AEDuration: %d", s->AEDuration);
		c->Message(CC_Default, "  mana: %d", s->mana);
		c->Message(CC_Default, "  base[12]: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", s->base[0], s->base[1], s->base[2], s->base[3], s->base[4], s->base[5], s->base[6], s->base[7], s->base[8], s->base[9], s->base[10], s->base[11]);
		c->Message(CC_Default, "  base22[12]: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", s->base2[0], s->base2[1], s->base2[2], s->base2[3], s->base2[4], s->base2[5], s->base2[6], s->base2[7], s->base2[8], s->base2[9], s->base2[10], s->base2[11]);
		c->Message(CC_Default, "  max[12]: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", s->max[0], s->max[1], s->max[2], s->max[3], s->max[4], s->max[5], s->max[6], s->max[7], s->max[8], s->max[9], s->max[10], s->max[11]);
		c->Message(CC_Default, "  components[4]: %d, %d, %d, %d", s->components[0], s->components[1], s->components[2], s->components[3]);
		c->Message(CC_Default, "  component_counts[4]: %d, %d, %d, %d", s->component_counts[0], s->component_counts[1], s->component_counts[2], s->component_counts[3]);
		c->Message(CC_Default, "  NoexpendReagent[4]: %d, %d, %d, %d", s->NoexpendReagent[0], s->NoexpendReagent[1], s->NoexpendReagent[2], s->NoexpendReagent[3]);
		c->Message(CC_Default, "  formula[12]: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x", s->formula[0], s->formula[1], s->formula[2], s->formula[3], s->formula[4], s->formula[5], s->formula[6], s->formula[7], s->formula[8], s->formula[9], s->formula[10], s->formula[11]);
		c->Message(CC_Default, "  goodEffect: %d", s->goodEffect);
		c->Message(CC_Default, "  Activated: %d", s->Activated);
		c->Message(CC_Default, "  resisttype: %d", s->resisttype);
		c->Message(CC_Default, "  effectid[12]: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x", s->effectid[0], s->effectid[1], s->effectid[2], s->effectid[3], s->effectid[4], s->effectid[5], s->effectid[6], s->effectid[7], s->effectid[8], s->effectid[9], s->effectid[10], s->effectid[11]);
		c->Message(CC_Default, "  targettype: %d", s->targettype);
		c->Message(CC_Default, "  basediff: %d", s->basediff);
		c->Message(CC_Default, "  skill: %d", s->skill);
		c->Message(CC_Default, "  zonetype: %d", s->zonetype);
		c->Message(CC_Default, "  EnvironmentType: %d", s->EnvironmentType);
		c->Message(CC_Default, "  TimeOfDay: %d", s->TimeOfDay);
		c->Message(CC_Default, "  classes[15]: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
			s->classes[0], s->classes[1], s->classes[2], s->classes[3], s->classes[4],
			s->classes[5], s->classes[6], s->classes[7], s->classes[8], s->classes[9],
			s->classes[10], s->classes[11], s->classes[12], s->classes[13], s->classes[14]);
		c->Message(CC_Default, "  CastingAnim: %d", s->CastingAnim);
		c->Message(CC_Default, "  SpellAffectIndex: %d", s->SpellAffectIndex);
		c->Message(CC_Default, " RecourseLink: %d", s->RecourseLink);
	}
}

void command_lastname(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t=c->GetTarget()->CastToClient();
	Log(Logs::General, Logs::Normal, "#lastname request from %s for %s", c->GetName(), t->GetName());

	if (strlen(sep->arg[1]) <= 70)
		t->ChangeLastName(sep->arg[1]);
	else
		c->Message(CC_Default, "Usage: #lastname <lastname> where <lastname> is less than 70 chars long");
}

void command_memspell(Client *c, const Seperator *sep){
	uint32 slot;
	uint16 spell_id;

	if (!(sep->IsNumber(1) && sep->IsNumber(2)))
	{
		c->Message(CC_Default, "Usage: #MemSpell slotid spellid");
	}
	else
	{
		slot = atoi(sep->arg[1]) - 1;
		spell_id = atoi(sep->arg[2]);
		if (slot > MAX_PP_MEMSPELL || spell_id >= SPDAT_RECORDS)
		{
			c->Message(CC_Default, "Error: #MemSpell: Arguement out of range");
		}
		else
		{
			c->MemSpell(spell_id, slot);
			c->Message(CC_Default, "Spell slot changed, have fun!");
		}
	}
}

void command_save(Client *c, const Seperator *sep)
{
	if (
		!c->GetTarget() ||
		(
			c->GetTarget() &&
			!c->GetTarget()->IsClient() &&
			!c->GetTarget()->IsPlayerCorpse()
			)
		) {
		c->Message(CC_Default, "You must target a player or player corpse to use this command.");
		return;
	}

	auto target = c->GetTarget();

	if (target->IsClient()) 
	{
		if (target->CastToClient()->Save(2))
		{
			c->Message_StringID(CC_Default, PC_SAVED, target->GetName());
		}
		else
			c->Message(CC_Default, "Manual save for %s failed.", target->GetName());
	}
	else if (target->IsPlayerCorpse())
	{
		if (target->CastToMob()->Save())
			c->Message(CC_Default, "%s saved. (dbid=%u)", target->GetName(), target->CastToCorpse()->GetCorpseDBID());
		else
			c->Message(CC_Default, "Manual save for %s failed.", target->GetName());
	}
	else
		c->Message(CC_Default, "Error: target not a Client/PlayerCorpse");
}

void command_showstats(Client *c, const Seperator *sep)
{
	if (sep->IsNumber(1) && atoi(sep->arg[1]) == 1) 
	{
		if (c->GetTarget() != 0 && c->GetTarget()->IsClient())
		{
			c->GetTarget()->CastToClient()->SendQuickStats(c);
		}
		else if(c->GetTarget() != 0 && c->GetTarget()->IsNPC())
		{
			c->GetTarget()->CastToNPC()->ShowQuickStats(c);
		}
		else
		{
			c->SendQuickStats(c);
		}
	}
	else if (c->GetTarget() != 0)
	{
		c->GetTarget()->ShowStats(c);
	}
	else
	{
		c->ShowStats(c);
	}
}

void command_mystats(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetPet()) {
		if (c->GetTarget()->IsPet() && c->GetTarget() == c->GetPet())
			c->GetTarget()->ShowStats(c);
		else
			c->ShowStats(c);
	}
	else
		c->ShowStats(c);
}

void command_bind(Client *c, const Seperator *sep){
	if (c->GetTarget() != 0) {
		if (c->GetTarget()->IsClient())
			c->GetTarget()->CastToClient()->SetBindPoint();
		else
			c->Message(CC_Default, "Error: target not a Player");
	}
	else
		c->SetBindPoint();
}

void command_depop(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0 || !(c->GetTarget()->IsNPC() || c->GetTarget()->IsNPCCorpse()))
		c->Message(CC_Default, "You must have a NPC target for this command. (maybe you meant #depopzone?)");
	else {
		c->Message(CC_Default, "Depoping '%s'.", c->GetTarget()->GetName());
		c->GetTarget()->Depop();
	}
}

void command_depopzone(Client *c, const Seperator *sep){
	zone->Depop();
	c->Message(CC_Default, "Zone depoped.");
}

void command_repop(Client *c, const Seperator *sep)
{
	bool force = false;
	if (sep->arg[1] && strcasecmp(sep->arg[1], "force") == 0) {
		force = true;
	}

	if (!force && c->GetTarget() && c->GetTarget()->IsNPC())
	{
		c->GetTarget()->CastToNPC()->ForceRepop();
		c->Message(CC_Default, "Repopping %s", c->GetTarget()->GetName());
	}
	else
	{
		int timearg = 1;
		if (force) {
			timearg++;

			LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
			iterator.Reset();
			while (iterator.MoreElements()) {
				std::string query = StringFormat(
					"DELETE FROM respawn_times WHERE id = %lu and guild_id = %lu",
					(unsigned long)iterator.GetData()->GetID(),
					(unsigned long)zone->GetGuildID()
				);
				auto results = database.QueryDatabase(query);
				iterator.Advance();
			}
			c->Message(CC_Default, "Zone depop: Force resetting spawn timers.");
		}

		c->Message(CC_Default, "Zone depoped. Repoping now.");
		zone->Repop();
		return;
	}
}

void command_repopclose(Client *c, const Seperator *sep)
{
	int repop_distance = 500;

	if (sep->arg[1] && strcasecmp(sep->arg[1], "force") == 0) {

		LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
		iterator.Reset();
		while (iterator.MoreElements()) {
			std::string query = StringFormat(
				"DELETE FROM respawn_times WHERE id = %lu and guild_id = %lu",
				(unsigned long)iterator.GetData()->GetID(), zone->GetGuildID()
			);
			auto results = database.QueryDatabase(query);
			iterator.Advance();
		}
		c->Message(0, "Zone depop: Force resetting spawn timers.");
	}
	if (sep->IsNumber(1)) {
		repop_distance = atoi(sep->arg[1]);
	}

	c->Message(0, "Zone depoped. Repopping NPC's within %i distance units", repop_distance);
	zone->RepopClose(c->GetPosition(), repop_distance);
}

void command_rewind(Client *c, const Seperator *sep){
	c->RewindCommand();
	return;
}

void command_spawnstatus(Client *c, const Seperator *sep)
{
	if (sep->IsNumber(1))
	{
		// show spawn status by spawn2 id
		zone->SpawnStatus(c, 'a', atoi(sep->arg[1]));
	}
	else if (strcmp(sep->arg[1], "help") == 0)
	{
		c->Message(CC_Default, "Usage: #spawnstatus <[a]ll (default) | [u]nspawned | [s]pawned | [d]isabled | [e]nabled | {Spawn2 ID}>");
	}
	else {
		zone->SpawnStatus(c, sep->arg[1][0]);
	}
}

void command_nukebuffs(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0)
		c->BuffFadeAll(false, true);
	else
		c->GetTarget()->BuffFadeAll(false, true);
}

void command_zuwcoords(Client *c, const Seperator *sep){
	// modifys and resends zhdr packet
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #zuwcoords <under world coords>");
	else {
		zone->newzone_data.underworld = atof(sep->arg[1]);
		//float newdata = atof(sep->arg[1]);
		//memcpy(&zone->zone_header_data[130], &newdata, sizeof(float));
		auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
		memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
		entity_list.QueueClients(c, outapp);
		safe_delete(outapp);
	}
}

void command_zunderworld(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #zunderworld <zcoord>");
	else {
		zone->newzone_data.underworld = atof(sep->arg[1]);
	}
}

void command_zsafecoords(Client *c, const Seperator *sep){
	// modifys and resends zhdr packet
	if (sep->arg[3][0] == 0)
		c->Message(CC_Default, "Usage: #zsafecoords <safe x> <safe y> <safe z>");
	else {
		zone->newzone_data.safe_x = atof(sep->arg[1]);
		zone->newzone_data.safe_y = atof(sep->arg[2]);
		zone->newzone_data.safe_z = atof(sep->arg[3]);
		//float newdatax = atof(sep->arg[1]);
		//float newdatay = atof(sep->arg[2]);
		//float newdataz = atof(sep->arg[3]);
		//memcpy(&zone->zone_header_data[114], &newdatax, sizeof(float));
		//memcpy(&zone->zone_header_data[118], &newdatay, sizeof(float));
		//memcpy(&zone->zone_header_data[122], &newdataz, sizeof(float));
		//zone->SetSafeCoords();
		auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
		memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
		entity_list.QueueClients(c, outapp);
		safe_delete(outapp);
	}
}

void command_freeze(Client *c, const Seperator *sep){
	if (c->GetTarget() != 0)
		c->GetTarget()->SendAppearancePacket(AT_Anim, ANIM_FREEZE);
	else
		c->Message(CC_Default, "ERROR: Freeze requires a target.");
}

void command_unfreeze(Client *c, const Seperator *sep){
	if (c->GetTarget() != 0)
		c->GetTarget()->SendAppearancePacket(AT_Anim, ANIM_STAND);
	else
		c->Message(CC_Default, "ERROR: Unfreeze requires a target.");
}

void command_pvp(Client *c, const Seperator *sep){
	bool state = atobool(sep->arg[1]);
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->arg[1][0] != 0) {
		t->SetPVP(state);
		c->Message(CC_Default, "%s now follows the ways of %s.", t->GetName(), state ? "discord" : "order");
	}
	else
		c->Message(CC_Default, "Usage: #pvp [on/off]");
}

void command_setxp(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->IsNumber(1)) {
		int exploss;
		t->GetExpLoss(nullptr,0,exploss);
		uint32 currentXP = t->GetEXP();
		uint32 currentaaXP = t->GetAAXP();
		int input = atoi(sep->arg[1]);

		if (input > 9999999)
			c->Message(CC_Default, "Error: Value too high.");
		else if(input == -1)
		{
			uint32 newxp = currentXP - exploss;
			if(newxp < 1000)
				newxp = 1000;
			t->SetEXP(newxp, currentaaXP);
		}
		else if(input == 0)
		{
			uint32 newxp = currentXP + exploss;
			t->SetEXP(newxp, currentaaXP);
		}
		else if(input <= 100)
		{
			float percent = input/100.0f;
			uint32 requiredxp = t->GetEXPForLevel(t->GetLevel()+1) - t->GetEXPForLevel(t->GetLevel());
			float final_ = requiredxp*percent;
			uint32 newxp = (uint32)final_ + currentXP;
			t->SetEXP(newxp, currentaaXP);
		}
		else
		{
			uint32 newxp = currentXP + input;
			t->SetEXP(newxp, currentaaXP);
		}
	}
	else
		c->Message(CC_Default, "Usage: #setxp number or percentage. If 0, will 'rez' a single death. If -1 will subtract a single death.");
}

void command_name(Client *c, const Seperator *sep){
	Client *target;

	if ((strlen(sep->arg[1]) == 0) || (!(c->GetTarget() && c->GetTarget()->IsClient())))
		c->Message(CC_Default, "Usage: #name newname (requires player target)");
	else
	{
		target = c->GetTarget()->CastToClient();
		std::string oldname = target->GetName();
		oldname[0] = toupper(oldname[0]);

		if (target->ChangeFirstName(sep->arg[1], c->GetName()))
		{
			c->Message(CC_Default, "Successfully renamed %s to %s", oldname.c_str(), sep->arg[1]);
			// until we get the name packet working right this will work
			c->Message(CC_Default, "Sending player to char select.");
			target->Kick();
		}
		else
			c->Message(CC_Red, "ERROR: Unable to rename %s. Check that the new name '%s' isn't already taken.", oldname.c_str(), sep->arg[2]);
	}
}

void command_kill(Client *c, const Seperator *sep)
{
	auto target = c->GetTarget();
	if (!target) 
	{
		c->Message(CC_Default, "You must have a target to use this command.");
		return;
	}

	if (!target->IsClient() || target->CastToClient()->Admin() <= c->Admin())
	{
		if (c != target)
		{
			c->Message(CC_Default, fmt::format("Killing {} .", target->GetCleanName()).c_str());

		}
		target->Kill();
	}
}

void command_haste(Client *c, const Seperator *sep){
	// #haste command to set client attack speed. Takes a percentage (100 = twice normal attack speed)
	if (sep->arg[1][0] != 0) {
		uint16 Haste = atoi(sep->arg[1]);
		if (Haste > 85)
			Haste = 85;
		c->SetExtraHaste(Haste);
		// SetAttackTimer must be called to make this take effect, so player needs to change
		// the primary weapon.
		c->Message(CC_Default, "Haste set to %d%% - Need to re-equip primary weapon before it takes effect", Haste);
	}
	else
		c->Message(CC_Default, "Usage: #haste [percentage]");
}

void command_damage(Client *c, const Seperator *sep)
{
	if (c->GetTarget() == 0)
		c->Message(CC_Default, "Error: #Damage: No Target.");
	else if (!sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #damage [dmg]");
	}
	else {			
		int32 nkdmg = atoi(sep->arg[1]);
		if (nkdmg > 2100000000)
			c->Message(CC_Default, "Enter a value less then 2,100,000,000.");
		else if (c->GetTarget() != c)
			c->GetTarget()->Damage(c, nkdmg, SPELL_UNKNOWN, EQ::skills::SkillHandtoHand);
		else
			c->GetTarget()->DamageCommand(c, nkdmg);
	}
}

void command_gmdamage(Client *c, const Seperator *sep)
{
	if (c->GetTarget() == 0)
		c->Message(CC_Default, "Error: #gmamage: No Target.");
	else if (!sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #gmdamage [dmg] [skipaggro] [spell]");
	}
	else {
		int32 nkdmg = atoi(sep->arg[1]);
		bool skipaggro = false;
		if (sep->IsNumber(2))
			skipaggro = atoi(sep->arg[2]) > 0 ? true : false;

		bool spell = false;
		if (sep->IsNumber(3))
			spell = atoi(sep->arg[3]) > 0 ? true : false;

		uint16 spell_id = SPELL_UNKNOWN;
		EQ::skills::SkillType attack_skill = EQ::skills::SkillHandtoHand;
		if (spell)
		{
			spell_id = 383;
			attack_skill = EQ::skills::SkillEvocation;
		}

		if (nkdmg > 2100000000)
			c->Message(CC_Default, "Enter a value less then 2,100,000,000.");
		else
			c->GetTarget()->DamageCommand(c, nkdmg, skipaggro, spell_id, attack_skill);
	}
}

void command_zonespawn(Client *c, const Seperator *sep){
	c->Message(CC_Default, "This command is not yet implemented.");
	return;

	/* this was kept from client.cpp verbatim (it was commented out) */
	//	if (target && target->IsNPC()) {
	//		Message(CC_Default, "Inside main if.");
	//		if (strcasecmp(sep->arg[1], "add")==0) {
	//			Message(CC_Default, "Inside add if.");
	//			database.DBSpawn(1, StaticGetZoneName(this->GetPP().current_zone), target->CastToNPC());
	//		}
	//		else if (strcasecmp(sep->arg[1], "update")==0) {
	//			database.DBSpawn(2, StaticGetZoneName(this->GetPP().current_zone), target->CastToNPC());
	//		}
	//		else if (strcasecmp(sep->arg[1], "remove")==0) {
	//			if (strcasecmp(sep->arg[2], "all")==0) {
	//				database.DBSpawn(4, StaticGetZoneName(this->GetPP().current_zone));
	//			}
	//			else {
	//				if (database.DBSpawn(3, StaticGetZoneName(this->GetPP().current_zone), target->CastToNPC())) {
	//					Message(CC_Default, "#zonespawn: %s removed successfully!", target->GetName());
	//					target->CastToNPC()->Death(target, target->GetHP());
	//				}
	//			}
	//		}
	//		else
	//			Message(CC_Default, "Error: #dbspawn: Invalid command. (Note: EDIT and REMOVE are NOT in yet.)");
	//		if (target->CastToNPC()->GetNPCTypeID() > 0) {
	//			Message(CC_Default, "Spawn is type %i", target->CastToNPC()->GetNPCTypeID());
	//		}
	//	}
	//	else if(!target || !target->IsNPC())
	//		Message(CC_Default, "Error: #zonespawn: You must have a NPC targeted!");
	//	else
	//		Message(CC_Default, "Usage: #zonespawn [add|edit|remove|remove all]");
}

void command_npcspawn(Client *c, const Seperator *sep){
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Command Syntax: #npcspawn [Add|Create|Delete|Remove|Update]");
		return;
	}

	if (!(c->GetTarget() && c->GetTarget()->IsNPC())) {
		c->Message(CC_Default, "You must target an NPC to use this command.");
		return;
	}

	NPC* target = c->GetTarget()->CastToNPC();
	std::string spawn_type = Strings::ToLower(sep->arg[1]);
	uint32 extra = 0;
	bool is_add = spawn_type.find("add") != std::string::npos;
	bool is_create = spawn_type.find("create") != std::string::npos;
	bool is_delete = spawn_type.find("delete") != std::string::npos;
	bool is_remove = spawn_type.find("remove") != std::string::npos;
	bool is_update = spawn_type.find("update") != std::string::npos;
	if (!is_add && !is_create && !is_delete && !is_remove && !is_update) {
		c->Message(CC_Default, "Command Syntax: #npcspawn [Add|Create|Delete|Remove|Update]");
		return;
	}

	if (is_add || is_create) {
		extra = (sep->IsNumber(2) ? (is_add ? std::stoi(sep->arg[2]) : 1) : (is_add ? 1200 : 0)); // Default to 1200 for Add, 0 for Create if not set
		database.NPCSpawnDB(is_add ? NPCSpawnTypes::AddNewSpawngroup : NPCSpawnTypes::CreateNewSpawn, zone->GetShortName(), c, target, extra);
		c->Message(CC_Default, fmt::format("Spawn {} | Name: {} ({})", is_add ? "Added" : "Created", target->GetCleanName(), target->GetID()).c_str());
	}
	else if (is_delete || is_remove || is_update) {
		uint8 spawn_update_type = (is_delete ? NPCSpawnTypes::DeleteSpawn : (is_remove ? NPCSpawnTypes::RemoveSpawn : NPCSpawnTypes::UpdateAppearance));
		std::string spawn_message = (is_delete ? "Deleted" : (is_remove ? "Removed" : "Updated"));
		database.NPCSpawnDB(spawn_update_type, zone->GetShortName(), c, target);
		c->Message(CC_Default, fmt::format("Spawn {} | Name: {} ({})", spawn_message, target->GetCleanName(), target->GetID()).c_str());
		if (is_delete || is_remove)
			target->Depop(false);
	}
}

void command_spawnfix(Client *c, const Seperator *sep)
{
	Mob *targetMob = c->GetTarget();
	if (!targetMob || !targetMob->IsNPC()) {
		c->Message(CC_Default, "Error: #spawnfix: Need an NPC target.");
		return;
	}

	Spawn2* s2 = targetMob->CastToNPC()->respawn2;

	if (!s2) {
		c->Message(CC_Default, "#spawnfix FAILED -- cannot determine which spawn entry in the database this mob came from.");
		return;
	}

	std::string query = StringFormat("UPDATE spawn2 SET x = '%f', y = '%f', z = '%f', heading = '%f' WHERE id = '%i'",
		c->GetX(), c->GetY(), c->GetZ(), c->GetHeading(), s2->GetID());
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		c->Message(CC_Red, "Update failed! MySQL gave the following error:");
		c->Message(CC_Red, results.ErrorMessage().c_str());
		return;
	}

    c->Message(CC_Default, "Updating coordinates successful.");
    targetMob->Depop(false);
}

void command_loc(Client *c, const Seperator *sep)
{
	Mob* target = c;
	if (c->GetTarget()) {
		target = c->GetTarget();
	}

	auto target_position = target->GetPosition();

	// client heading uses 0 - 511 if target is client.
	c->Message(CC_Default, fmt::format(" {} Location | XYZ: {:.2f}, {:.2f}, {:.2f} Heading: {:.2f} ", (c == target ? "Your" : fmt::format(" {} ({}) ", target->GetCleanName(), target->GetID())), target_position.x, target_position.y, target_position.z, (c == target || target->IsClient()) ? target_position.w * 2 : target_position.w).c_str());
		
	float newz = 0;
	if (!zone->zonemap)
	{
		c->Message(CC_Default, "Map not loaded for this zone.");
	}
	else
	{
		auto z = c->GetZ() + (c->GetSize() == 0.0 ? 6 : c->GetSize()) * HEAD_POSITION;
		auto me = glm::vec3(c->GetX(), c->GetY(), z);
		glm::vec3 hit;
		glm::vec3 bme(me);
		bme.z -= 500;

		auto newz = zone->zonemap->FindBestZ(me, &hit);
		if (newz != BEST_Z_INVALID)
		{
			me.z = target->SetBestZ(newz);
			c->Message(CC_Default, fmt::format("Best Z is {:.2f} ", newz).c_str());
		}
		else
		{
			c->Message(CC_Default, "Could not find Best Z.");
		}
	}

	if (!zone->watermap)
	{
		c->Message(CC_Default, "Water Map not loaded for this zone.");
	}
	else
	{
		auto position = glm::vec3(target->GetX(), target->GetY(), target->GetZ());
		auto region_type = zone->watermap->ReturnRegionType(position);
		auto position_string = fmt::format(" {} is", target->GetCleanName());
		
		switch (region_type) {
			case RegionTypeIce: {
				c->Message(CC_Default, fmt::format("{} in Ice.", position_string).c_str());
				break;
			}
			case RegionTypeLava: {
				c->Message(CC_Default, fmt::format("{} in Lava.", position_string).c_str());
				break;
			}
			case RegionTypeNormal: {
				c->Message(CC_Default, fmt::format("{} in a Normal Region.", position_string).c_str());
				break;
			}
			case RegionTypePVP: {
				c->Message(CC_Default, fmt::format("{} in a PvP Area.", position_string).c_str());
				break;
			}
			case RegionTypeSlime: {
				c->Message(CC_Default, fmt::format("{} in Slime.", position_string).c_str());
				break;
			}
			case RegionTypeVWater: {
				c->Message(CC_Default, fmt::format("{} in VWater (Icy Water?).", position_string).c_str());
				break;
			}
			case RegionTypeWater: {
				c->Message(CC_Default, fmt::format("{} in Water.", position_string).c_str());
				break;
			}
			default: {
				c->Message(CC_Default, fmt::format("{} in an Unknown Region.", position_string).c_str());
				break;
			}
		}

	}

	if(target->CanCastBindAffinity() && (zone->CanBind() || zone->IsCity() || zone->CanBindOthers()))
	{
		c->Message(CC_Default, fmt::format(" {} can bind here.", target->GetCleanName()).c_str());
	}
	else if(!target->CanCastBindAffinity() && (zone->IsCity() || zone->IsBindArea(target->GetX(), target->GetY(), target->GetZ())))
	{
		c->Message(CC_Default, fmt::format(" {} can be bound here.", target->GetCleanName()).c_str());
	}
	else
	{
		c->Message(CC_Default, fmt::format(" {} cannot bind here.", target->GetCleanName()).c_str());
	}
}

void command_goto(Client *c, const Seperator *sep)
{
	// goto function
	if (sep->arg[1][0] == '\0' && c->GetTarget())
		c->MovePC(zone->GetZoneID(), c->GetTarget()->GetX(), c->GetTarget()->GetY(), c->GetTarget()->GetZ(), c->GetTarget()->GetHeading(), 0, SummonPC);
	else if (!(sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3)))
		c->Message(CC_Default, "Usage: #goto [x y z]");
	else
		c->MovePC(zone->GetZoneID(), atof(sep->arg[1]), atof(sep->arg[2]), atof(sep->arg[3]), 0.0f, 0, SummonPC);
}

void command_iteminfo(Client *c, const Seperator *sep)
{
	auto inst = c->GetInv()[EQ::invslot::slotCursor];
	if (!inst) { c->Message(13, "Error: You need an item on your cursor for this command"); }
	auto item = inst->GetItem();
	if (!item)
		c->Message(CC_Red, "Error: You need an item on your cursor for this command");
	else {

		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemInst);
		linker.SetItemInst(inst);

		c->Message(0, "*** Item Info for [%s] ***", linker.GenerateLink().c_str());
		c->Message(CC_Default, "ID: %i Name: %s", item->ID, item->Name);
		c->Message(CC_Default, "  Lore: %s  ND: %i  NS: %i  Type: %i", item->Lore, item->NoDrop, item->NoRent, item->ItemClass);
		c->Message(CC_Default, "  IDF: %s  Size: %i  Weight: %i  icon_id: %i  Price: %i", item->IDFile, item->Size, item->Weight, item->Icon, item->Price);
		c->Message(CC_Default, "  QuantityType: %d (1 Normal, 2 Charges, 3 Stacked)", database.ItemQuantityType(inst->GetItem()->ID));
		if (c->Admin() >= 200)
			c->Message(CC_Default, "MinStatus: %i", item->MinStatus);
		if (item->ItemClass == EQ::item::ItemClassBook)
			c->Message(CC_Default, "  This item is a Book: %s", item->Filename);
		else if (item->ItemClass == EQ::item::ItemClassBag)
			c->Message(CC_Default, "  This item is a container with %i slots", item->BagSlots);
		else {
			c->Message(CC_Default, "  equipableSlots: %u equipable Classes: %u", item->Slots, item->Classes);
			c->Message(CC_Default, "  Magic: %i  SpellID: %i  Proc Level: %i DBCharges: %i  CurCharges: %i", item->Magic, item->Click.Effect, item->Click.Level, item->MaxCharges, inst->GetCharges());
			c->Message(CC_Default, "  EffectType: 0x%02x  CastTime: %.2f", (uint8)item->Click.Type, (double)item->CastTime / 1000);
			c->Message(CC_Default, "  Material: 0x%02x  Color: 0x%08x  Skill: %i", item->Material, item->Color, item->ItemType);
			c->Message(CC_Default, " Required level: %i Required skill: %i Recommended level:%i", item->ReqLevel, item->RecSkill, item->RecLevel);
			c->Message(CC_Default, " Skill mod: %i percent: %i", item->SkillModType, item->SkillModValue);
			c->Message(CC_Default, " BaneRace: %i BaneBody: %i BaneDMG: %i", item->BaneDmgRace, item->BaneDmgBody, item->BaneDmgAmt);
		}
	}
}

void command_uptime(Client *c, const Seperator *sep){
	if (!worldserver.Connected())
		c->Message(CC_Default, "Error: World server disconnected");
	else
	{
		auto pack = new ServerPacket(ServerOP_Uptime, sizeof(ServerUptime_Struct));
		ServerUptime_Struct* sus = (ServerUptime_Struct*)pack->pBuffer;
		strcpy(sus->adminname, c->GetName());
		if (sep->IsNumber(1) && atoi(sep->arg[1]) > 0)
			sus->zoneserverid = atoi(sep->arg[1]);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void command_flag(Client *c, const Seperator *sep){
	int arguments = sep->argnum;
	if (!arguments) {
		auto target = c->GetTarget() && c->GetTarget()->IsClient() ? c->GetTarget()->CastToClient() : c;
		if (target != c) {
			c->Message(
				CC_Default,
				fmt::format(
					"Status level has been refreshed for {}.",
					target->GetCleanName()
				).c_str()
			);

			target->Message(
				CC_Default,
				fmt::format(
					"Your status level has been refreshed by {}.",
					c->GetCleanName()
				).c_str()
			);
		}
		else {
			c->Message(CC_Default, "Your status level has been refreshed.");
		}
		target->UpdateAdmin();
		return;
	}


	if (
		!sep->IsNumber(1) ||
		strlen(sep->arg[2]) == 0
		) {
		c->Message(CC_Default, "Usage: #flag [Status] [Account Name]");
		return;
	}

	auto status = std::stoi(sep->arg[1]);
	if (status < -2 || status > 255) {
		c->Message(CC_Default, "The lowest a status level can go is -2 and the highest a status level can go is 255.");
		return;
	}

	std::string account_name = sep->argplus[2];
	auto account_id = database.GetAccountIDByChar(account_name.c_str());

	if (c->Admin() < commandChangeFlags) { //this check makes banning players by less than this level impossible, but i'll leave it in anyways
		c->Message(CC_Default, "You may only refresh your own flag, doing so now.");
		c->UpdateAdmin();
	}
	else {
		if (status > c->Admin()) {
			c->Message(
				CC_Default,
				fmt::format(
					"You cannot set someone's status level to {} because your status level is only {}.",
					status,
					c->Admin()
				).c_str()
			);
		}
		else if (status < 0 && c->Admin() < commandBanPlayers) {
			c->Message(CC_Default, "Your status level is not high enough to ban or suspend.");
		}
		else if (!database.SetAccountStatus(account_name, status)) {
			c->Message(CC_Default, "Failed to set status level.");
		}
		else {
			c->Message(CC_Default, "Set GM Flag on account.");

			std::string user;
			std::string loginserver;
			ParseAccountString(account_name, user, loginserver);

			account_id = database.GetAccountIDByName(account_name);

			ServerPacket pack(ServerOP_FlagUpdate, sizeof(ServerFlagUpdate_Struct));
			ServerFlagUpdate_Struct* sfus = (ServerFlagUpdate_Struct*)pack.pBuffer;
			sfus->account_id = account_id;
			sfus->admin = status;
			worldserver.SendPacket(&pack);
		}
	}
}

void command_time(Client *c, const Seperator *sep)
{
	const auto arguments = sep->argnum;
	if (arguments < 1 || !sep->IsNumber(1)) {
		c->Message(CC_Default, "To set the Time: #time HH [MM]");

		TimeOfDay_Struct eqTime;
		zone->zone_time.getEQTimeOfDay(time(0), &eqTime);

		auto time_string = fmt::format("{}:{}{} {}",
			((eqTime.hour) % 12) == 0 ? 12 : ((eqTime.hour) % 12),
			(eqTime.minute < 10) ? "0" : "",
			eqTime.minute,
			(eqTime.hour >= 12 && eqTime.hour < 24) ? "PM" : "AM"
		);

		c->Message(CC_Default, fmt::format("It is now {}.", time_string).c_str());

		return;
	}

	uint8 minutes = 0;
	uint8 hours = Strings::ToUnsignedInt(sep->arg[1]);

	if (hours > 24) {
		hours = 24;
	}


	if (sep->IsNumber(2)) {
		minutes = Strings::ToUnsignedInt(sep->arg[2]);

		if (minutes > 59) {
			minutes = 59;
		}
	}
		
	c->Message(CC_Default, fmt::format("Setting world time to {}:{} ...", hours, minutes).c_str());
		
	zone->SetTime(hours, minutes);
	
	LogInfo("{} :: Setting world time to {}:{} ...", c->GetCleanName(), hours, minutes);
}

void command_guild(Client *c, const Seperator *sep){
	int admin = c->Admin();
	Mob *target = c->GetTarget();

	if (strcasecmp(sep->arg[1], "help") == 0) {
		/*
		c->Message(CC_Default, "Guild commands:");
		c->Message(CC_Default, "  #guild status [name] - shows guild and rank of target");
		c->Message(CC_Default, "  #guild info guildnum - shows info/current structure");
		c->Message(CC_Default, "  #guild invite [charname]");
		c->Message(CC_Default, "  #guild remove [charname]");
		c->Message(CC_Default, "  #guild promote rank [charname]");
		c->Message(CC_Default, "  #guild demote rank [charname]");
		c->Message(CC_Default, "  /guildmotd [newmotd] (use 'none' to clear)");
		c->Message(CC_Default, "  #guild edit rank title newtitle");
		c->Message(CC_Default, "  #guild edit rank permission 0/1");
		c->Message(CC_Default, "  #guild leader newleader (they must be rank0)");
		*/
		c->Message(CC_Default, "GM Guild commands:");
		c->Message(CC_Default, "  #guild list - lists all guilds on the server");
		c->Message(CC_Default, "  #guild create {guildleader charname or CharID} guildname");
		c->Message(CC_Default, "  #guild delete guildID");
		c->Message(CC_Default, "  #guild rename guildID newname");
		c->Message(CC_Default, "  #guild set charname guildID    (0=no guild)");
		c->Message(CC_Default, "  #guild setrank charname rank");
		c->Message(CC_Default, "  #guild setleader guildID {guildleader charname or CharID}");
		//c->Message(CC_Default, "  #guild setdoor guildEQID");
	}
	else if (strcasecmp(sep->arg[1], "status") == 0 || strcasecmp(sep->arg[1], "stat") == 0) {
		Client* client = 0;
		if (sep->arg[2][0] != 0)
			client = entity_list.GetClientByName(sep->argplus[2]);
		else if (target != 0 && target->IsClient())
			client = target->CastToClient();
		if (client == 0)
			c->Message(CC_Default, "You must target someone or specify a character name");
		else if ((client->Admin() >= minStatusToEditOtherGuilds && admin < minStatusToEditOtherGuilds) && client->GuildID() != c->GuildID()) // no peeping for GMs, make sure tell message stays the same
			c->Message(CC_Default, "You must target someone or specify a character name.");
		else {
			if (!client->IsInAGuild())
				c->Message(CC_Default, "%s is not in a guild.", client->GetName());
			else if (guild_mgr.IsGuildLeader(client->GuildID(), client->CharacterID()))
				c->Message(CC_Default, "%s is the leader of <%s> rank: %s", client->GetName(), guild_mgr.GetGuildName(client->GuildID()), guild_mgr.GetRankName(client->GuildID(), client->GuildRank()));
			else
				c->Message(CC_Default, "%s is a member of <%s> rank: %s", client->GetName(), guild_mgr.GetGuildName(client->GuildID()), guild_mgr.GetRankName(client->GuildID(), client->GuildRank()));
		}
	}
	else if (strcasecmp(sep->arg[1], "info") == 0) {
		if (sep->arg[2][0] == 0 && c->IsInAGuild()) {
			if (admin >= minStatusToEditOtherGuilds)
				c->Message(CC_Default, "Usage: #guildinfo guild_id");
			else
				c->Message(CC_Default, "You're not in a guild");
		}
		else {
			uint32 tmp = GUILD_NONE;
			if (sep->arg[2][0] == 0)
				tmp = c->GuildID();
			else if (admin >= minStatusToEditOtherGuilds)
				tmp = atoi(sep->arg[2]);

			if (tmp != GUILD_NONE)
				guild_mgr.DescribeGuild(c, tmp);
		}
	}
	/*
	else if (strcasecmp(sep->arg[1], "edit") == 0) {
	if (c->GuildDBID() == 0)
	c->Message(CC_Default, "You arent in a guild!");
	else if (!sep->IsNumber(2))
	c->Message(CC_Default, "Error: invalid rank #.");
	else if (atoi(sep->arg[2]) < 0 || atoi(sep->arg[2]) > GUILD_MAX_RANK)
	c->Message(CC_Default, "Error: invalid rank #.");
	else if (!c->GuildRank() == 0)
	c->Message(CC_Default, "You must be rank %s to use edit.", guilds[c->GuildEQID()].rank[0].rankname);
	else if (!worldserver.Connected())
	c->Message(CC_Default, "Error: World server dirconnected");
	else {
	if (!helper_guild_edit(c, c->GuildDBID(), c->GuildEQID(), atoi(sep->arg[2]), sep->arg[3], sep->argplus[4])) {
	c->Message(CC_Default, "  #guild edit rank title newtitle");
	c->Message(CC_Default, "  #guild edit rank permission 0/1");
	}
	else {
	auto pack = new ServerPacket(ServerOP_RefreshGuild, 5);
	int32 geqid=c->GuildEQID();
	memcpy(pack->pBuffer, &geqid, 4);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	}
	}
	}
	else if (strcasecmp(sep->arg[1], "gmedit") == 0 && admin >= 100) {
	if (!sep->IsNumber(2))
	c->Message(CC_Default, "Error: invalid guilddbid.");
	else if (!sep->IsNumber(3))
	c->Message(CC_Default, "Error: invalid rank #.");
	else if (atoi(sep->arg[3]) < 0 || atoi(sep->arg[3]) > GUILD_MAX_RANK)
	c->Message(CC_Default, "Error: invalid rank #.");
	else if (!worldserver.Connected())
	c->Message(CC_Default, "Error: World server dirconnected");
	else {
	uint32 eqid = database.GetGuildEQID(atoi(sep->arg[2]));
	if (eqid == GUILD_NONE)
	c->Message(CC_Default, "Error: Guild not found");
	else if (!helper_guild_edit(c, atoi(sep->arg[2]), eqid, atoi(sep->arg[3]), sep->arg[4], sep->argplus[5])) {
	c->Message(CC_Default, "  #guild gmedit guilddbid rank title newtitle");
	c->Message(CC_Default, "  #guild gmedit guilddbid rank permission 0/1");
	}
	else {
	auto pack = new ServerPacket(ServerOP_RefreshGuild, 5);
	memcpy(pack->pBuffer, &eqid, 4);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	}
	}
	}
	*/
	else if (strcasecmp(sep->arg[1], "set") == 0) {
		if (!sep->IsNumber(3))
			c->Message(CC_Default, "Usage: #guild set charname guildgbid (0 = clear guildtag)");
		else {
			uint32 guild_id = atoi(sep->arg[3]);

			if (guild_id == 0)
				guild_id = GUILD_NONE;
			else if (!guild_mgr.GuildExists(guild_id)) {
				c->Message(CC_Red, "Guild %d does not exist.", guild_id);
				return;
			}

			uint32 charid = database.GetCharacterID(sep->arg[2]);
			if (charid == 0) {
				c->Message(CC_Red, "Unable to find character '%s'", charid);
				return;
			}

			//we could do the checking we need for guild_mgr.CheckGMStatus, but im lazy right now
			if (admin < minStatusToEditOtherGuilds) {
				c->Message(CC_Red, "Access denied.");
				return;
			}

			if (guild_id == GUILD_NONE) {
				Log(Logs::Detail, Logs::Guilds, "%s: Removing %s (%d) from guild with GM command.", c->GetName(),
					sep->arg[2], charid);
			}
			else {
				Log(Logs::Detail, Logs::Guilds, "%s: Putting %s (%d) into guild %s (%d) with GM command.", c->GetName(),
					sep->arg[2], charid,
					guild_mgr.GetGuildName(guild_id), guild_id);
			}

			if (!guild_mgr.SetGuild(charid, guild_id, GUILD_MEMBER)) {
				c->Message(CC_Red, "Error putting '%s' into guild %d", sep->arg[2], guild_id);
			}
			else {
				c->Message(CC_Default, "%s has been put into guild %d", sep->arg[2], guild_id);
			}
		}
	}
	/*else if (strcasecmp(sep->arg[1], "setdoor") == 0 && admin >= minStatusToEditOtherGuilds) {

	if (!sep->IsNumber(2))
	c->Message(CC_Default, "Usage: #guild setdoor guildEQid (0 = delete guilddoor)");
	else {
	// guild doors
	if((!guilds[atoi(sep->arg[2])].databaseID) && (atoi(sep->arg[2])!=0) )
	{

	c->Message(CC_Default, "These is no guild with this guildEQid");
	}
	else {
	c->SetIsSettingGuildDoor(true);
	c->Message(CC_Default, "Click on a door you want to become a guilddoor");
	c->SetSetGuildDoorID(atoi(sep->arg[2]));
	}
	}
	}*/
	else if (strcasecmp(sep->arg[1], "setrank") == 0) {
		int rank = atoi(sep->arg[3]);
		if (!sep->IsNumber(3))
			c->Message(CC_Default, "Usage: #guild setrank charname rank");
		else if (rank < 0 || rank > GUILD_MAX_RANK)
			c->Message(CC_Default, "Error: invalid rank #.");
		else {
			uint32 charid = database.GetCharacterID(sep->arg[2]);
			if (charid == 0) {
				c->Message(CC_Red, "Unable to find character '%s'", charid);
				return;
			}

			//we could do the checking we need for guild_mgr.CheckGMStatus, but im lazy right now
			if (admin < minStatusToEditOtherGuilds) {
				c->Message(CC_Red, "Access denied.");
				return;
			}

			Log(Logs::Detail, Logs::Guilds, "%s: Setting %s (%d)'s guild rank to %d with GM command.", c->GetName(),
				sep->arg[2], charid, rank);

			if (!guild_mgr.SetGuildRank(charid, rank))
				c->Message(CC_Red, "Error while setting rank %d on '%s'.", rank, sep->arg[2]);
			else
				c->Message(CC_Default, "%s has been set to rank %d", sep->arg[2], rank);
		}
	}
	else if (strcasecmp(sep->arg[1], "create") == 0) {
		if (sep->arg[3][0] == 0)
			c->Message(CC_Default, "Usage: #guild create {guildleader charname or CharID} guild name");
		else if (!worldserver.Connected())
			c->Message(CC_Default, "Error: World server dirconnected");
		else {
			uint32 leader = 0;
			if (sep->IsNumber(2)) {
				leader = atoi(sep->arg[2]);
			}
			else if ((leader = database.GetCharacterID(sep->arg[2])) != 0) {
				//got it from the db..
			}
			else {
				c->Message(CC_Red, "Unable to find char '%s'", sep->arg[2]);
				return;
			}
			if (leader == 0) {
				c->Message(CC_Default, "Guild leader not found.");
				return;
			}

			uint32 tmp = guild_mgr.FindGuildByLeader(leader);
			if (tmp != GUILD_NONE) {
				c->Message(CC_Default, "Error: %s already is the leader of DB# %i '%s'.", sep->arg[2], tmp, guild_mgr.GetGuildName(tmp));
			}
			else {

				if (admin < minStatusToEditOtherGuilds) {
					c->Message(CC_Red, "Access denied.");
					return;
				}

				uint32 id = guild_mgr.CreateGuild(sep->argplus[3], leader);

				Log(Logs::Detail, Logs::Guilds, "%s: Creating guild %s with leader %d with GM command. It was given id %lu.", c->GetName(),
					sep->argplus[3], leader, (unsigned long)id);

				if (id == GUILD_NONE)
					c->Message(CC_Default, "Guild creation failed.");
				else {
					c->Message(CC_Default, "Guild created: Leader: %i, number %i: %s", leader, id, sep->argplus[3]);

					if (!guild_mgr.SetGuild(leader, id, GUILD_LEADER))
						c->Message(CC_Default, "Unable to set guild leader's guild in the database. Your going to have to run #guild set");
				}

			}
		}
	}
	else if (strcasecmp(sep->arg[1], "delete") == 0) {
		if (!sep->IsNumber(2))
			c->Message(CC_Default, "Usage: #guild delete guildID");
		else if (!worldserver.Connected())
			c->Message(CC_Default, "Error: World server dirconnected");
		else {
			uint32 id = atoi(sep->arg[2]);

			if (!guild_mgr.GuildExists(id)) {
				c->Message(CC_Default, "Guild %d does not exist!", id);
				return;
			}

			if (admin < minStatusToEditOtherGuilds) {
				//this person is not allowed to just edit any guild, check this guild's min status.
				if (c->GuildID() != id) {
					c->Message(CC_Red, "Access denied to edit other people's guilds");
					return;
				}
				else if (!guild_mgr.CheckGMStatus(id, admin)) {
					c->Message(CC_Red, "Access denied to edit your guild with GM commands.");
					return;
				}
			}

			Log(Logs::Detail, Logs::Guilds, "%s: Deleting guild %s (%d) with GM command.", c->GetName(),
				guild_mgr.GetGuildName(id), id);

			if (!guild_mgr.DeleteGuild(id))
				c->Message(CC_Default, "Guild delete failed.");
			else {
				c->Message(CC_Default, "Guild %d deleted.", id);
			}
		}
	}
	else if (strcasecmp(sep->arg[1], "rename") == 0) {
		if ((!sep->IsNumber(2)) || sep->arg[3][0] == 0)
			c->Message(CC_Default, "Usage: #guild rename guildID newname");
		else if (!worldserver.Connected())
			c->Message(CC_Default, "Error: World server dirconnected");
		else {
			uint32 id = atoi(sep->arg[2]);

			if (!guild_mgr.GuildExists(id)) {
				c->Message(CC_Default, "Guild %d does not exist!", id);
				return;
			}

			if (admin < minStatusToEditOtherGuilds) {
				//this person is not allowed to just edit any guild, check this guild's min status.
				if (c->GuildID() != id) {
					c->Message(CC_Red, "Access denied to edit other people's guilds");
					return;
				}
				else if (!guild_mgr.CheckGMStatus(id, admin)) {
					c->Message(CC_Red, "Access denied to edit your guild with GM commands.");
					return;
				}
			}

			Log(Logs::Detail, Logs::Guilds, "%s: Renaming guild %s (%d) to '%s' with GM command.", c->GetName(),
				guild_mgr.GetGuildName(id), id, sep->argplus[3]);

			if (!guild_mgr.RenameGuild(id, sep->argplus[3]))
				c->Message(CC_Default, "Guild rename failed.");
			else {
				c->Message(CC_Default, "Guild %d renamed to %s", id, sep->argplus[3]);
			}
		}
	}
	else if (strcasecmp(sep->arg[1], "setleader") == 0) {
		if (sep->arg[3][0] == 0 || !sep->IsNumber(2))
			c->Message(CC_Default, "Usage: #guild setleader guild_id {guildleader charname or CharID}");
		else if (!worldserver.Connected())
			c->Message(CC_Default, "Error: World server dirconnected");
		else {
			uint32 leader = 0;
			if (sep->IsNumber(3)) {
				leader = atoi(sep->arg[3]);
			}
			else if ((leader = database.GetCharacterID(sep->arg[3])) != 0) {
				//got it from the db..
			}
			else {
				c->Message(CC_Red, "Unable to find char '%s'", sep->arg[3]);
				return;
			}

			uint32 tmpdb = guild_mgr.FindGuildByLeader(leader);
			if (leader == 0)
				c->Message(CC_Default, "New leader not found.");
			else if (tmpdb != GUILD_NONE) {
				c->Message(CC_Default, "Error: %s already is the leader of guild # %i", sep->arg[2], tmpdb);
			}
			else {
				uint32 id = atoi(sep->arg[2]);

				if (!guild_mgr.GuildExists(id)) {
					c->Message(CC_Default, "Guild %d does not exist!", id);
					return;
				}

				if (admin < minStatusToEditOtherGuilds) {
					//this person is not allowed to just edit any guild, check this guild's min status.
					if (c->GuildID() != id) {
						c->Message(CC_Red, "Access denied to edit other people's guilds");
						return;
					}
					else if (!guild_mgr.CheckGMStatus(id, admin)) {
						c->Message(CC_Red, "Access denied to edit your guild with GM commands.");
						return;
					}
				}

				Log(Logs::Detail, Logs::Guilds, "%s: Setting leader of guild %s (%d) to %d with GM command.", c->GetName(),
					guild_mgr.GetGuildName(id), id, leader);

				if (!guild_mgr.SetGuildLeader(id, leader))
					c->Message(CC_Default, "Guild leader change failed.");
				else {
					c->Message(CC_Default, "Guild leader changed: guild # %d, Leader: %s", id, sep->argplus[3]);
				}
			}
		}
	}
	else if (strcasecmp(sep->arg[1], "list") == 0) {
		if (admin < minStatusToEditOtherGuilds) {
			c->Message(CC_Red, "Access denied.");
			return;
		}
		guild_mgr.ListGuilds(c);
	}
	else {
		c->Message(CC_Default, "Unknown guild command, try #guild help");
	}
}

/*
bool helper_guild_edit(Client *c, uint32 dbid, uint32 eqid, uint8 rank, const char* what, const char* value) {
struct GuildRankLevel_Struct grl;
strcpy(grl.rankname, guild_mgr.GetRankName(eqid, rank));
grl.demote = guilds[eqid].rank[rank].demote;
grl.heargu = guilds[eqid].rank[rank].heargu;
grl.invite = guilds[eqid].rank[rank].invite;
grl.motd = guilds[eqid].rank[rank].motd;
grl.promote = guilds[eqid].rank[rank].promote;
grl.remove = guilds[eqid].rank[rank].remove;
grl.speakgu = guilds[eqid].rank[rank].speakgu;
grl.warpeace = guilds[eqid].rank[rank].warpeace;

if (strcasecmp(what, "title") == 0) {
if (strlen(value) > 100)
c->Message(CC_Default, "Error: Title has a maxium length of 100 characters.");
else
strcpy(grl.rankname, value);
}
else if (rank == 0)
c->Message(CC_Default, "Error: Rank 0's permissions can not be changed.");
else {
if (!(strlen(value) == 1 && (value[0] == '0' || value[0] == '1')))

return false;
if (strcasecmp(what, "demote") == 0)
grl.demote = (value[0] == '1');
else if (strcasecmp(what, "heargu") == 0)
grl.heargu = (value[0] == '1');
else if (strcasecmp(what, "invite") == 0)
grl.invite = (value[0] == '1');
else if (strcasecmp(what, "motd") == 0)
grl.motd = (value[0] == '1');
else if (strcasecmp(what, "promote") == 0)
grl.promote = (value[0] == '1');
else if (strcasecmp(what, "remove") == 0)

grl.remove = (value[0] == '1');
else if (strcasecmp(what, "speakgu") == 0)
grl.speakgu = (value[0] == '1');
else if (strcasecmp(what, "warpeace") == 0)
grl.warpeace = (value[0] == '1');
else
c->Message(CC_Default, "Error: Permission name not recognized.");
}
if (!database.EditGuild(dbid, rank, &grl))
c->Message(CC_Default, "Error: database.EditGuild() failed");
return true;
}*/

void command_zonestatus(Client *c, const Seperator *sep){
	if (!worldserver.Connected())
		c->Message(CC_Default, "Error: World server disconnected");
	else {
		auto pack = new ServerPacket(ServerOP_ZoneStatus, strlen(c->GetName()) + 2);
		memset(pack->pBuffer, (uint8)c->Admin(), 1);
		strcpy((char *)&pack->pBuffer[1], c->GetName());
		worldserver.SendPacket(pack);
		delete pack;
	}
}

void command_manaburn(Client *c, const Seperator *sep){
	Mob* target = c->GetTarget();

	if (c->GetTarget() == 0)
		c->Message(CC_Default, "#Manaburn needs a target.");
	else {
		int cur_level = c->GetAA(MANA_BURN);//ManaBurn ID
		if (DistanceSquared(c->GetPosition(), target->GetPosition()) > 200)
			c->Message(CC_Default, "You are too far away from your target.");
		else {
			if (cur_level == 1) {
				if (c->IsAttackAllowed(target))
				{
					c->SetMana(0);
					int nukedmg = (c->GetMana()) * 2;
					if (nukedmg>0)
					{
						target->Damage(c, nukedmg, 2751, EQ::skills::SkillAbjuration/*hackish*/);
						c->Message(CC_Blue, "You unleash an enormous blast of magical energies.");
					}
					Log(Logs::General, Logs::Normal, "Manaburn request from %s, damage: %d", c->GetName(), nukedmg);
				}
			}
			else
				c->Message(CC_Default, "You have not learned this skill.");
		}
	}
}

void command_doanim(Client *c, const Seperator *sep){
	Animation animation = static_cast<Animation>(atoi(sep->arg[1]));
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #DoAnim [number]");
	else
		if (c->Admin() >= commandDoAnimOthers)
			if (c->GetTarget() == 0)
				c->Message(CC_Default, "Error: You need a target.");
			else
				c->GetTarget()->DoAnim(animation, atoi(sep->arg[2]));
		else
			c->DoAnim(animation, atoi(sep->arg[2]));
}

void command_randomfeatures(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!target)
		c->Message(CC_Default, "Error: This command requires a target");
	else
	{
		uint16 Race = target->GetRace();
		if (Race <= 12 || Race == 128 || Race == 130 || Race == 330) {

			uint8 Gender = target->GetGender();
			uint8 Texture = 0xFF;
			uint8 HelmTexture = 0xFF;
			uint8 HairColor = 0xFF;
			uint8 BeardColor = 0xFF;
			uint8 EyeColor1 = 0xFF;
			uint8 EyeColor2 = 0xFF;
			uint8 HairStyle = 0xFF;
			uint8 LuclinFace = 0xFF;
			uint8 Beard = 0xFF;

			// Set some common feature settings
			EyeColor1 = zone->random.Int(0, 9);
			EyeColor2 = zone->random.Int(0, 9);
			LuclinFace = zone->random.Int(0, 7);

			// Adjust all settings based on the min and max for each feature of each race and gender
			switch (Race)
			{
			case 1:	// Human
				HairColor = zone->random.Int(0, 19);
				if (Gender == 0) {
					BeardColor = HairColor;
					HairStyle = zone->random.Int(0, 3);
					Beard = zone->random.Int(0, 5);
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 2:	// Barbarian
				HairColor = zone->random.Int(0, 19);
				LuclinFace = zone->random.Int(0, 87);
				if (Gender == 0) {
					BeardColor = HairColor;
					HairStyle = zone->random.Int(0, 3);
					Beard = zone->random.Int(0, 5);
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 3: // Erudite
				if (Gender == 0) {
					BeardColor = zone->random.Int(0, 19);
					Beard = zone->random.Int(0, 5);
					LuclinFace = zone->random.Int(0, 57);
				}
				if (Gender == 1) {
					LuclinFace = zone->random.Int(0, 87);
				}
				break;
			case 4: // WoodElf
				HairColor = zone->random.Int(0, 19);
				if (Gender == 0) {
					HairStyle = zone->random.Int(0, 3);
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 5: // HighElf
				HairColor = zone->random.Int(0, 14);
				if (Gender == 0) {
					HairStyle = zone->random.Int(0, 3);
					LuclinFace = zone->random.Int(0, 37);
					BeardColor = HairColor;
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 6: // DarkElf
				HairColor = zone->random.Int(13, 18);
				if (Gender == 0) {
					HairStyle = zone->random.Int(0, 3);
					LuclinFace = zone->random.Int(0, 37);
					BeardColor = HairColor;
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 7: // HalfElf
				HairColor = zone->random.Int(0, 19);
				if (Gender == 0) {
					HairStyle = zone->random.Int(0, 3);
					LuclinFace = zone->random.Int(0, 37);
					BeardColor = HairColor;
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 8: // Dwarf
				HairColor = zone->random.Int(0, 19);
				BeardColor = HairColor;
				if (Gender == 0) {
					HairStyle = zone->random.Int(0, 3);
					Beard = zone->random.Int(0, 5);
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
					LuclinFace = zone->random.Int(0, 17);
				}
				break;
			case 9: // Troll
				EyeColor1 = zone->random.Int(0, 10);
				EyeColor2 = zone->random.Int(0, 10);
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 3);
					HairColor = zone->random.Int(0, 23);
				}
				break;
			case 10: // Ogre
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 3);
					HairColor = zone->random.Int(0, 23);
				}
				break;
			case 11: // Halfling
				HairColor = zone->random.Int(0, 19);
				if (Gender == 0) {
					BeardColor = HairColor;
					HairStyle = zone->random.Int(0, 3);
					Beard = zone->random.Int(0, 5);
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 12: // Gnome
				HairColor = zone->random.Int(0, 24);
				if (Gender == 0) {
					BeardColor = HairColor;
					HairStyle = zone->random.Int(0, 3);
					Beard = zone->random.Int(0, 5);
				}
				if (Gender == 1) {
					HairStyle = zone->random.Int(0, 2);
				}
				break;
			case 128: // Iksar
			case 130: // VahShir
				break;
			case 330: // Froglok
				LuclinFace = zone->random.Int(0, 9);
				break;
			default:
				break;
			}

			target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
				EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

			c->Message(CC_Default, "NPC Features Randomized");
		}
		else
			c->Message(CC_Default, "This command requires a Playable Race as the Target");
	}
}

void command_randtest(Client *c, const Seperator *sep) {
	
	if (!sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #randtest [iterations]");
		return;
	}
	int total = atoi(sep->arg[1]);
	if (total < 1 || total > 10000000) {
		c->Message(CC_Default, "Usage: #randtest [iterations] min value 1, max 1000000");
		return;
	}
	int lastval = -1;
	int maxlastval = 0;
	int maxtimes = 0;
	int maxcount = 0;
	int results[100];
	for (int i = 0; i < 100; i++) {
		results[i] = 0;
	}

	for (int i = 0; i < total; i++) {
		int value = zone->random.Int(0, 99);
		if (lastval == value) {
			maxlastval++;
		}
		else {
			if (maxlastval > maxtimes) {
				maxcount = 1;
				maxtimes = maxlastval;
			}
			else if (maxlastval == maxtimes) {
				maxcount++;
			}
			maxlastval = 0;
		}
		lastval = value;
		results[value]++;
	}
	for (int i = 0; i < 100; i++) {
		c->Message(CC_Default, "Random Results [%i], %i (%.2f %s)", i, results[i], (float)results[i] / (float)total * 100.0f, "%");
	}
	c->Message(CC_Default, "Same number happened %i times in a row, %i times", maxtimes + 1, maxcount);
}

void command_face(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #face [number of face]");
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else {
		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = 0xFF;
		uint8 HairColor = target->GetHairColor();
		uint8 BeardColor = target->GetBeardColor();
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = target->GetHairStyle();
		uint8 LuclinFace = atoi(sep->arg[1]);
		uint8 Beard = target->GetBeard();

		target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
			EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

		c->Message(CC_Default, "Face = %i", atoi(sep->arg[1]));
	}
}

void command_findaliases(Client* c, const Seperator* sep)
{
	if (!sep->arg[1][0]) {
		c->Message(0, "Usage: #findaliases [alias | command]");
		return;
	}

	std::map<std::string, std::string>::iterator find_iter = commandaliases.find(sep->arg[1]);
	if (find_iter == commandaliases.end()) {
		c->Message(15, "No commands or aliases match '%s'", sep->arg[1]);
		return;
	}

	std::map<std::string, CommandRecord*>::iterator command_iter = commandlist.find(find_iter->second);
	if (find_iter->second.empty() || command_iter == commandlist.end()) {
		c->Message(0, "An unknown condition occurred...");
		return;
	}

	c->Message(0, "Available command aliases for '%s':", command_iter->first.c_str());

	int commandaliasesshown = 0;
	for (std::map<std::string, std::string>::iterator alias_iter = commandaliases.begin(); alias_iter != commandaliases.end(); ++alias_iter) {
		if (strcasecmp(find_iter->second.c_str(), alias_iter->second.c_str()) || c->Admin() < command_iter->second->access)
			continue;

		c->Message(0, "%c%s", COMMAND_CHAR, alias_iter->first.c_str());
		++commandaliasesshown;
	}
	c->Message(0, "%d command alias%s listed.", commandaliasesshown, commandaliasesshown != 1 ? "es" : "");
}

void command_helm(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #helm [number of helm texture]");
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else {
		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = atoi(sep->arg[1]);
		uint8 HairColor = target->GetHairColor();
		uint8 BeardColor = target->GetBeardColor();
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = target->GetHairStyle();
		uint8 LuclinFace = target->GetLuclinFace();
		uint8 Beard = target->GetBeard();

		target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
			EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

		c->Message(CC_Default, "Helm = %i", atoi(sep->arg[1]));
	}
}

void command_hair(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #hair [number of hair style]");
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else {
		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = 0xFF;
		uint8 HairColor = target->GetHairColor();
		uint8 BeardColor = target->GetBeardColor();
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = atoi(sep->arg[1]);
		uint8 LuclinFace = target->GetLuclinFace();
		uint8 Beard = target->GetBeard();

		target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
			EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

		c->Message(CC_Default, "Hair = %i", atoi(sep->arg[1]));
	}
}

void command_haircolor(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #haircolor [number of hair color]");
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else {
		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = 0xFF;
		uint8 HairColor = atoi(sep->arg[1]);
		uint8 BeardColor = target->GetBeardColor();
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = target->GetHairStyle();
		uint8 LuclinFace = target->GetLuclinFace();
		uint8 Beard = target->GetBeard();

		target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
			EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

		c->Message(CC_Default, "Hair Color = %i", atoi(sep->arg[1]));
	}
}

void command_beard(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #beard [number of beard style]");
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else {
		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = 0xFF;
		uint8 HairColor = target->GetHairColor();
		uint8 BeardColor = target->GetBeardColor();
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = target->GetHairStyle();
		uint8 LuclinFace = target->GetLuclinFace();
		uint8 Beard = atoi(sep->arg[1]);

		target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
			EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

		c->Message(CC_Default, "Beard = %i", atoi(sep->arg[1]));
	}
}

void command_beardcolor(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (!sep->IsNumber(1))
		c->Message(CC_Default, "Usage: #beardcolor [number of beard color]");
	else if (!target)
		c->Message(CC_Default, "Error: this command requires a target");
	else {
		uint16 Race = target->GetRace();
		uint8 Gender = target->GetGender();
		uint8 Texture = 0xFF;
		uint8 HelmTexture = 0xFF;
		uint8 HairColor = target->GetHairColor();
		uint8 BeardColor = atoi(sep->arg[1]);
		uint8 EyeColor1 = target->GetEyeColor1();
		uint8 EyeColor2 = target->GetEyeColor2();
		uint8 HairStyle = target->GetHairStyle();
		uint8 LuclinFace = target->GetLuclinFace();
		uint8 Beard = target->GetBeard();

		target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
			EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF);

		c->Message(CC_Default, "Beard Color = %i", atoi(sep->arg[1]));
	}
}

void command_scribespells(Client *c, const Seperator *sep) {
	Client *target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM()) {
		target = c->GetTarget()->CastToClient();
	}

	if (sep->argnum < 1 || !sep->IsNumber(1)) {
		c->Message(CC_Default, "FORMAT: #scribespells <max level> <min level>");
		return;
	}

	uint8 rule_max_level = (uint8)RuleI(Character, MaxLevel);
	uint8 max_level = (uint8)std::stoi(sep->arg[1]);
	uint8 min_level = (
		sep->IsNumber(2) ?
		(uint8)
		std::stoi(sep->arg[2]) :
		1
		); // Default to Level 1 if there isn't a 2nd argument
	
	if (!c->GetGM()) { // Default to Character:MaxLevel if we're not a GM and Level is higher than the max level
		if (max_level > rule_max_level) {
			max_level = rule_max_level;
		}

		if (min_level > rule_max_level) {
			min_level = rule_max_level;
		}
	}

	if (max_level < 1 || min_level < 1) {
		c->Message(CC_Default, "ERROR: Level must be greater than or equal to 1.");
		return;
	}

	if (min_level > max_level) {
		c->Message(CC_Default, "ERROR: Minimum Level must be less than or equal to Maximum Level.");
		return;
	}

	uint16 scribed_spells = target->ScribeSpells(min_level, max_level);
	if (target != c) {
		std::string spell_message = (
			scribed_spells > 0 ?
			(
				scribed_spells == 1 ?
				"A new spell" :
				fmt::format("{} New spells", scribed_spells)
				) :
			"No new spells"
			);
		c->Message(
			CC_Default,
			fmt::format(
				"{} scribed for {}.",
				spell_message,
				target->GetCleanName()
			).c_str()
		);
	}
}

void command_scribespell(Client *c, const Seperator *sep){
	uint16 spell_id = 0;
	uint16 book_slot = -1;
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
		t = c->GetTarget()->CastToClient();

	if (!sep->arg[1][0]) {
		c->Message(CC_Default, "FORMAT: #scribespell <spellid>");
		return;
	}

	spell_id = atoi(sep->arg[1]);

	if (IsValidSpell(spell_id)) {
		t->Message(CC_Default, "Scribing spell: %s (%i) to spellbook.", spells[spell_id].name, spell_id);

		if (t != c)
			c->Message(CC_Default, "Scribing spell: %s (%i) for %s.", spells[spell_id].name, spell_id, t->GetName());

		Log(Logs::General, Logs::Normal, "Scribe spell: %s (%i) request for %s from %s.", spells[spell_id].name, spell_id, t->GetName(), c->GetName());

		if (spells[spell_id].classes[WARRIOR] != 0 && spells[spell_id].skill != 52 && spells[spell_id].classes[t->GetPP().class_ - 1] > 0) {
			book_slot = t->GetNextAvailableSpellBookSlot();

			if (book_slot >= 0 && t->FindSpellBookSlotBySpellID(spell_id) < 0)
				t->ScribeSpell(spell_id, book_slot);
			else {
				t->Message(CC_Red, "Unable to scribe spell: %s (%i) to your spellbook.", spells[spell_id].name, spell_id);

				if (t != c)
					c->Message(CC_Red, "Unable to scribe spell: %s (%i) for %s.", spells[spell_id].name, spell_id, t->GetName());
			}
		}
		else
			c->Message(CC_Red, "Your target can not scribe this spell.");
	}
	else
		c->Message(CC_Red, "Spell ID: %i is an unknown spell and cannot be scribed.", spell_id);
}

void command_unmemspell(Client *c, const Seperator *sep){
	uint16 spell_id = 0;
	uint16 mem_slot = -1;
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
		t = c->GetTarget()->CastToClient();

	if (!sep->arg[1][0]) {
		c->Message(CC_Default, "FORMAT: #unmemspell <spellid>");
		return;
	}

	spell_id = atoi(sep->arg[1]);

	if (IsValidSpell(spell_id)) {
		mem_slot = t->FindSpellMemSlotBySpellID(spell_id);

		if (mem_slot >= 0) {
			t->UnmemSpell(mem_slot);

			t->Message(CC_Default, "Unmemming spell: %s (%i) from gembar.", spells[spell_id].name, spell_id);

			if (t != c)
				c->Message(CC_Default, "Unmemming spell: %s (%i) for %s.", spells[spell_id].name, spell_id, t->GetName());

			Log(Logs::Detail, Logs::Normal, "Unmem spell: %s (%i) request for %s from %s.", spells[spell_id].name, spell_id, t->GetName(), c->GetName());
		}
		else {
			t->Message(CC_Red, "Unable to unmemspell spell: %s (%i) from your gembar. This spell is not memmed.", spells[spell_id].name, spell_id);

			if (t != c)
				c->Message(CC_Red, "Unable to unmemspell spell: %s (%i) for %s due to spell not memmed.", spells[spell_id].name, spell_id, t->GetName());
		}
	}
}

void command_unmemspells(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
		t = c->GetTarget()->CastToClient();

	t->UnmemSpellAll();
}

void command_unscribespell(Client *c, const Seperator *sep){
	uint16 spell_id = 0;
	uint16 book_slot = -1;
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
		t = c->GetTarget()->CastToClient();

	if (!sep->arg[1][0]) {
		c->Message(CC_Default, "FORMAT: #unscribespell <spellid>");
		return;
	}

	spell_id = atoi(sep->arg[1]);

	if (IsValidSpell(spell_id)) {
		book_slot = t->FindSpellBookSlotBySpellID(spell_id);

		if (book_slot >= 0) {
			t->UnscribeSpell(book_slot);

			t->Message(CC_Default, "Unscribing spell: %s (%i) from spellbook.", spells[spell_id].name, spell_id);

			if (t != c)
				c->Message(CC_Default, "Unscribing spell: %s (%i) for %s.", spells[spell_id].name, spell_id, t->GetName());

			Log(Logs::General, Logs::Normal, "Unscribe spell: %s (%i) request for %s from %s.", spells[spell_id].name, spell_id, t->GetName(), c->GetName());
		}
		else {
			t->Message(CC_Red, "Unable to unscribe spell: %s (%i) from your spellbook. This spell is not scribed.", spells[spell_id].name, spell_id);

			if (t != c)
				c->Message(CC_Red, "Unable to unscribe spell: %s (%i) for %s due to spell not scribed.", spells[spell_id].name, spell_id, t->GetName());
		}
	}
}

void command_unscribespells(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
		t = c->GetTarget()->CastToClient();

	t->UnscribeSpellAll();
}

void command_wpinfo(Client *c, const Seperator *sep){
	Mob *t = c->GetTarget();

	if (t == nullptr || !t->IsNPC()) {
		c->Message(CC_Default, "You must target an NPC to use this.");
		return;
	}

	NPC *n = t->CastToNPC();
	n->DisplayWaypointInfo(c);
}

void command_wpadd(Client *c, const Seperator *sep)
{
	int	type1=0,
		type2=0,
		pause=0;	// Defaults for a new grid

	Mob *t = c->GetTarget();
	if (t && t->IsNPC())
	{
		Spawn2* s2info = t->CastToNPC()->respawn2;

		if (s2info == nullptr)	// Can't figure out where this mob's spawn came from... maybe a dynamic mob created by #spawn
		{
			c->Message(CC_Default, "#wpadd FAILED -- Can't determine which spawn record in the database this mob came from!");
			return;
		}

		if (sep->arg[1][0])
		{
			if (atoi(sep->arg[1]) >= 0)
				pause = atoi(sep->arg[1]);
			else
			{
				c->Message(CC_Default, "Usage: #wpadd [pause] [-h]");
				return;
			}
		}
		auto position = c->GetPosition();
		if (strcmp("-h",sep->arg[2]) != 0)
			position.w = -1;

		uint32 tmp_grid = database.AddWPForSpawn(c, s2info->GetID(), position, pause, type1, type2, zone->GetZoneID());
		if (tmp_grid)
			t->CastToNPC()->SetGrid(tmp_grid);

		t->CastToNPC()->AssignWaypoints(t->CastToNPC()->GetGrid());
		c->Message(CC_Default, "Waypoint added. Use #wpinfo to see waypoints for this NPC (may need to #repop first).");
	}
	else
		c->Message(CC_Default, "You must target an NPC to use this.");
}

void command_interrogateinv(Client *c, const Seperator *sep)
{
	// 'command_interrogateinv' is an in-memory inventory interrogation tool only.
	//
	// it does not verify against actual database entries..but, the output can be
	// used to verify that something has been corrupted in a player's inventory.
	// any error condition should be assumed that the item in question will be
	// lost when the player logs out or zones (or incurrs any action that will
	// consume the Client-Inventory object instance in question.)
	//
	// any item instances located at a greater depth than a reported error should
	// be treated as an error themselves regardless of whether they report as the
	// same or not.

	if (strcasecmp(sep->arg[1], "help") == 0) {
		if (c->Admin() < commandInterrogateInv) {
			c->Message(CC_Default, "Usage: #interrogateinv");
			c->Message(CC_Default, "  Displays your inventory's current in-memory nested storage references");
		}
		else {
			c->Message(CC_Default, "Usage: #interrogateinv [log] [silent]");
			c->Message(CC_Default, "  Displays your or your Player target inventory's current in-memory nested storage references");
			c->Message(CC_Default, "  [log] - Logs interrogation to file");
			c->Message(CC_Default, "  [silent] - Omits the in-game message portion of the interrogation");
		}
		return;
	}

	Client* target = nullptr;
	std::map<int16, const EQ::ItemInstance*> instmap;
	bool log = false;
	bool silent = false;
	bool error = false;
	bool allowtrip = false;

	if (c->Admin() < commandInterrogateInv) {
		if (c->GetInterrogateInvState()) {
			c->Message(CC_Red, "The last use of #interrogateinv on this inventory instance discovered an error...");
			c->Message(CC_Red, "Logging out, zoning or re-arranging items at this point will result in item loss!");
			return;
		}
		target = c;
		allowtrip = true;
	}
	else {
		if (c->GetTarget() == nullptr) {
			target = c;
		}
		else if (c->GetTarget()->IsClient()) {
			target = c->GetTarget()->CastToClient();
		}
		else {
			c->Message(1, "Use of this command is limited to Client entities");
			return;
		}

		if (strcasecmp(sep->arg[1], "log") == 0)
			log = true;
		if (strcasecmp(sep->arg[2], "silent") == 0)
			silent = true;
	}

	bool success = target->InterrogateInventory(c, log, silent, allowtrip, error);

	if (!success)
		c->Message(CC_Red, "An unknown error occurred while processing Client::InterrogateInventory()");
}

void command_interrupt(Client *c, const Seperator *sep){
	uint16 ci_message = 0x01b7, ci_color = 0x0121;

	if (sep->arg[1][0])
		ci_message = atoi(sep->arg[1]);
	if (sep->arg[2][0])
		ci_color = atoi(sep->arg[2]);

	c->InterruptSpell(ci_message, ci_color);
}

void command_d1(Client *c, const Seperator *sep){
}

void command_summonitem(Client *c, const Seperator *sep)
{
	uint32 itemid = 0;
	std::string cmd_msg = sep->msg;
	size_t link_open = cmd_msg.find('\x12');
	size_t link_close = cmd_msg.find_last_of('\x12');
	if (link_open != link_close && (cmd_msg.length() - link_open) > EQ::constants::SAY_LINK_BODY_SIZE) {
		EQ::SayLinkBody_Struct link_body;
		EQ::saylink::DegenerateLinkBody(link_body, cmd_msg.substr(link_open + 1, EQ::constants::SAY_LINK_BODY_SIZE));
		itemid = link_body.item_id;
	}
	else if (!sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #summonitem [item id] [charges], charges are optional");
		return;
	}
	else {
		uint32 itemid = atoi(sep->arg[1]);
		int16 item_status = 0;
		const EQ::ItemData* item = database.GetItem(itemid);
		if (item) {
			item_status = static_cast<int16>(item->MinStatus);
		}

		int16 charges = 0;
		if (sep->argnum<2 || !sep->IsNumber(2))
		{
			if(item && database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
			{
				charges = item->MaxCharges;
			}
			else
			{
				charges = 0;
			}
		}
		else
			charges = atoi(sep->arg[2]);

		//No rent GM Uber Sword
		if (item_status > c->Admin() || (c->Admin() < 80 && itemid == 2661))
			c->Message(CC_Red, "Error: Insufficient status to summon this item.");
		else
			c->SummonItem(itemid, charges, 0, true);
	}
}

void command_giveitem(Client *c, const Seperator *sep){
	if (!sep->IsNumber(1)) {
		c->Message(CC_Red, "Usage: #giveitem [item id] [charges] [reason], charges are optional, reason is required.");
	}
	else if (sep->arg[2][0] == 0) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (sep->IsNumber(2) &&sep->arg[3][0] == 0) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (c->GetTarget() == nullptr) {
		c->Message(CC_Red, "You must target a client to give the item to.");
	}
	else if (!c->GetTarget()->IsClient()) {
		c->Message(CC_Red, "You can only give items to players with this command.");
	}
	else {
		Client *t = c->GetTarget()->CastToClient();
		uint32 itemid = atoi(sep->arg[1]);
		int16 item_status = 0;
		const EQ::ItemData* item = database.GetItem(itemid);
		if (item) {
			item_status = static_cast<int16>(item->MinStatus);
		}

		int16 charges = 0;
		if (sep->argnum<2 || !sep->IsNumber(2))
		{
			if(item && database.ItemQuantityType(itemid) == EQ::item::Quantity_Charges)
			{
				charges = item->MaxCharges;
			}
			else
			{
				charges = 0;
			}
		}
		else
			charges = atoi(sep->arg[2]);

		//No rent GM Uber Sword
		if (item_status > c->Admin() || (c->Admin() < 80 && itemid == 2661))
			c->Message(CC_Red, "Error: Insufficient status to summon this item.");
		else
			t->SummonItem(itemid, charges, 0, true);
	}
}

void command_givemoney(Client *c, const Seperator *sep){
	if (!sep->IsNumber(1)) {	//as long as the first one is a number, we'll just let atoi convert the rest to 0 or a number
		c->Message(CC_Red, "Usage: #Usage: #givemoney [pp] [gp] [sp] [cp] [reason] - Reason is required");
	}
	else if (sep->IsNumber(1) && sep->arg[2][0] == 0) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (sep->IsNumber(2) && sep->arg[3][0] == 0) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (sep->IsNumber(3) && sep->arg[4][0] == 0) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (sep->IsNumber(4) && sep->arg[5][0] == 0) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (sep->IsNumber(5)) {
		c->Message(CC_Red, "Reason is required.");
	}
	else if (c->GetTarget() == nullptr) {
		c->Message(CC_Red, "You must target a player to give money to.");
	}
	else if (!c->GetTarget()->IsClient()) {
		c->Message(CC_Red, "You can only give money to players with this command.");
	}
	else {
		//TODO: update this to the client, otherwise the client doesn't show any weight change until you zone, move an item, etc
		c->GetTarget()->CastToClient()->AddMoneyToPP(atoi(sep->arg[4]), atoi(sep->arg[3]), atoi(sep->arg[2]), atoi(sep->arg[1]), true);
		c->Message(CC_Default, "Added %i Platinum, %i Gold, %i Silver, and %i Copper to %s's inventory.", atoi(sep->arg[1]), atoi(sep->arg[2]), atoi(sep->arg[3]), atoi(sep->arg[4]), c->GetTarget()->GetName());
	}
}

void command_itemsearch(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #itemsearch [search string]");
	else
	{
		const char *search_criteria = sep->argplus[1];

		const EQ::ItemData *item = nullptr;
		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemData);

		if (Seperator::IsNumber(search_criteria)) {
			item = database.GetItem(atoi(search_criteria));
			if (item) {
				linker.SetItemData(item);

				c->Message(CC_Default, fmt::format(" {} : {} ", item->ID, linker.GenerateLink().c_str()).c_str());

			}
			else {
				c->Message(CC_Default, fmt::format("Item {} not found", search_criteria).c_str());
			}
			return;
		}

		int count = 0;
		std::string sName;
		std::string sCriteria;
		sCriteria = search_criteria;
		for (auto & c : sCriteria) c = toupper(c);
		uint32 it = 0;
		while ((item = database.IterateItems(&it))) 
		{
			sName = item->Name;
			for (auto & c : sName) c = toupper(c);
			if (sName.find(sCriteria) != std::string::npos) 
			{
				linker.SetItemData(item);
				c->Message(CC_Default, fmt::format(" {} : {} ", (int)item->ID, linker.GenerateLink().c_str()).c_str());

				++count;
			}

			if (count == 50)
				break;
		}

		if (count == 50)
			c->Message(CC_Default, "50 items shown...too many results.");
		else
			c->Message(CC_Default, fmt::format(" {} items found", count).c_str());
	}
}

void command_setaaxp(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->IsNumber(1)) {
		t->SetEXP(t->GetEXP(), atoi(sep->arg[1]), false);
	}
	else
		c->Message(CC_Default, "Usage: #setaaxp <new AA XP value> (<new Group AA XP value> <new Raid XP value>)");
}

void command_setaapts(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();

	if (sep->arg[1][0] == '\0')
		c->Message(CC_Default, "Usage: #setaapts <new AA points value>");
	else if (atoi(sep->arg[1]) <= 0 || atoi(sep->arg[1]) > 170)
		c->Message(CC_Default, "You must have a number greater than 0 for points and no more than 170.");
	else {
		t->SetEXP(t->GetEXP(), t->GetEXPForLevel(t->GetLevel(), true)*atoi(sep->arg[1]), false);
		t->SendAAStats();
		t->SendAATable();
	}
}

void command_stun(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (!arguments || !sep->IsNumber(1)) {
		c->Message(CC_Default, "Usage: #stun [Duration]");
		return;
	}

	Mob* target = c;
	int duration = static_cast<int>(std::min(std::stoll(sep->arg[1]), (long long)2000000000));

	if (duration < 0) {
		duration = 0;
	}

	if (c->GetTarget()) {
		target = c->GetTarget();
		if (target->IsClient()) {
			target->CastToClient()->Stun(duration, c);
		}
		else if (target->IsNPC()) {
			target->CastToNPC()->Stun(duration, c);
		}
	}
	else {
		c->Stun(duration, c);
	}

	std::string stun_message = (
		duration ?
		fmt::format(
			"You stunned {} for {}.",
			(
				c == target ?
				"yourself" :
				fmt::format(
					"{} ({})",
					target->GetCleanName(),
					target->GetID()
				)
				),
			Strings::MillisecondsToTime(duration)
		) :
		fmt::format(
			"You unstunned {}.",
			(
				c == target ?
				"yourself" :
				fmt::format(
					"{} ({})",
					target->GetCleanName(),
					target->GetID()
				)
				)
		)
		);
	c->Message(
		CC_Default,
		stun_message.c_str()
	);
}

void command_ban(Client *c, const Seperator *sep)
{
	int arguments = sep->argnum;
	if (arguments < 2) {
		c->Message(CC_Default, "Usage: #ban [Character Name] [Reason]");
		return;
	}

	std::string character_name = sep->arg[1];
	if (character_name.empty()) {
		c->Message(CC_Default, "Usage: #ban [Character Name] [Reason]");
		return;
	}

	std::string reason = sep->argplus[2];
	if (reason.empty()) {
		c->Message(CC_Default, "Usage: #ban [Character Name] [Reason]");
		return;
	}

	auto account_id = database.GetAccountIDByChar(character_name.c_str());
	if (!account_id) {
		c->Message(
			CC_Default,
			fmt::format(
				"Character {} does not exist.",
				character_name
			).c_str()
		);
		return;
	}

	auto query = fmt::format(
		"UPDATE account SET status = -2, ban_reason = '{}' WHERE id = {}",
		Strings::Escape(reason),
		account_id
	);
	auto results = database.QueryDatabase(query);

	c->Message(CC_Default, 
		fmt::format(
			"Account ID {} with the character {} has been banned for the following reason: \"{}\"",
			account_id,
			character_name,
			reason
		).c_str()
	);

	ServerPacket flagUpdatePack(ServerOP_FlagUpdate, sizeof(ServerFlagUpdate_Struct));
	auto sfus = (ServerFlagUpdate_Struct*)flagUpdatePack.pBuffer;
	sfus->account_id = account_id;
	sfus->admin = -2;
	worldserver.SendPacket(&flagUpdatePack);

	auto client = entity_list.GetClientByName(character_name.c_str());
	if (client) {
		client->WorldKick();
	}

	ServerPacket kickPlayerPack(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
	auto skp = (ServerKickPlayer_Struct*)kickPlayerPack.pBuffer;
	strcpy(skp->adminname, c->GetName());
	strcpy(skp->name, character_name.c_str());
	skp->adminrank = c->Admin();
	worldserver.SendPacket(&kickPlayerPack);
}

void command_suspend(Client *c, const Seperator *sep)
{
	if ((sep->arg[1][0] == 0) || (sep->arg[2][0] == 0)) {
		c->Message(CC_Default, "Usage: #suspend <charname> <days> (Specify 0 days to lift the suspension immediately) <message>");
		return;
	}

	int duration = atoi(sep->arg[2]);

	if (duration < 0)
		duration = 0;

	std::string message;

	if (duration > 0) {
		int i = 3;
		while (1) {
			if (sep->arg[i][0] == 0) {
				break;
			}

			if (message.length() > 0) {
				message.push_back(' ');
			}

			message += sep->arg[i];
			++i;
		}

		if (message.length() == 0) {
			c->Message(CC_Default, "Usage: #suspend <charname> <days>(Specify 0 days to lift the suspension immediately) <message>");
			return;
		}
	}

	auto escName = new char[strlen(sep->arg[1]) * 2 + 1];
	database.DoEscapeString(escName, sep->arg[1], strlen(sep->arg[1]));
	int accountID = database.GetAccountIDByChar(escName);
	safe_delete_array(escName);

	if (accountID <= 0) {
		c->Message(CC_Red, "Character does not exist.");
		return;
	}

	std::string query = StringFormat("UPDATE `account` SET `suspendeduntil` = DATE_ADD(NOW(), INTERVAL %i DAY), "
		"suspend_reason = '%s' WHERE `id` = %i",
		duration, Strings::Escape(message).c_str(), accountID);
	auto results = database.QueryDatabase(query);

	if (duration)
		c->Message(CC_Red, "Account number %i with the character %s has been temporarily suspended for %i day(s).", accountID, sep->arg[1], duration);
	else
		c->Message(CC_Red, "Account number %i with the character %s is no longer suspended.", accountID, sep->arg[1]);

	auto pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
	ServerKickPlayer_Struct* sks = (ServerKickPlayer_Struct*)pack->pBuffer;

    strn0cpy(sks->adminname, c->GetName(), sizeof(sks->adminname));
    strn0cpy(sks->name, sep->arg[1], sizeof(sks->name));
    sks->adminrank = c->Admin();

    worldserver.SendPacket(pack);

    safe_delete(pack);

	Client *bannedClient = entity_list.GetClientByName(sep->arg[1]);

	if (bannedClient) {
		bannedClient->WorldKick();
	}
}

void command_ipban(Client *c, const Seperator *sep){
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #ipban [IP]");
		return;
	}

	std::string ip = sep->arg[1];
	if (ip.empty()) {
		c->Message(CC_Default, "Usage: #ipban [IP]");
		return;
	}

	if (database.AddBannedIP(ip, c->GetName())) {
		c->Message(
			CC_Default,
			fmt::format(
				"IP '{}' has been successfully banned.",
				ip
			).c_str()
		);
	}
	else {
		c->Message(
			CC_Default,
			fmt::format(
				"IP '{}' has failed to be banned, the IP address may already be in the table.",
				ip
			).c_str()
		);
	}
}

void command_revoke(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] == 0 || sep->arg[2][0] == 0 || sep->arg[3][0] == 0) {
		c->Message(CC_Default, "Usage: #revoke [charname] [0(unrevoke) / 1(all but guild/group/raid) / 2(guild/group/raid mute)] [duration in days (0 days is perma)]");
		return;
	}

	uint32 characterID = database.GetAccountIDByChar(sep->arg[1]);
	if (characterID == 0) {
		c->Message(CC_Red, "Character does not exist.");
		return;
	}

	int flag = atoi(sep->arg[2]);
	std::string query = StringFormat("UPDATE account SET revoked = %d WHERE id = %i", flag, characterID);
	auto results = database.QueryDatabase(query);

	int duration_in_days = atoi(sep->arg[3]);
	if (duration_in_days != 0 && flag != 0)
	{
		std::string query2 = StringFormat("UPDATE `account` SET `revokeduntil` = DATE_ADD(NOW(), INTERVAL %i DAY) WHERE `id` = %i", duration_in_days, characterID);
		auto results2 = database.QueryDatabase(query2);
	}
	else
	{
		//perma, or unrevoke
		std::string query3 = StringFormat("UPDATE `account` SET `revokeduntil` = '0000-00-00 00:00:00' WHERE `id` = %i", characterID);
		auto results3 = database.QueryDatabase(query3);
	}

	c->Message(CC_Red, "%s account number %i with the character %s.", flag ? "Revoking" : "Unrevoking", characterID, sep->arg[1]);

	auto outapp = new ServerPacket(ServerOP_Revoke, sizeof(RevokeStruct));
	RevokeStruct* revoke = (RevokeStruct*)outapp->pBuffer;
	strn0cpy(revoke->adminname, c->GetName(), 64);
	strn0cpy(revoke->name, sep->arg[1], 64);
	revoke->toggle = flag;
	worldserver.SendPacket(outapp);
	safe_delete(outapp);
}

void command_oocmute(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0 || !(sep->arg[1][0] == '1' || sep->arg[1][0] == '0'))
		c->Message(CC_Default, "Usage: #oocmute [1/0]");
	else {
		auto outapp = new ServerPacket(ServerOP_OOCMute, 1);
		*(outapp->pBuffer) = atoi(sep->arg[1]);
		worldserver.SendPacket(outapp);
		safe_delete(outapp);
	}
}

void command_checklos(Client *c, const Seperator *sep){
	if (c->GetTarget())
	{
		//		if(c->CheckLos(c->GetTarget()))
		if (c->CheckLosFN(c->GetTarget()))
			c->Message(CC_Default, "You have LOS to %s", c->GetTarget()->GetName());
		else
			c->Message(CC_Default, "You do not have LOS to %s", c->GetTarget()->GetName());
		if (c->CheckRegion(c->GetTarget(), false))
			c->Message(CC_Default, "You are in the same region as %s", c->GetTarget()->GetName());
		else
		{
			c->Message(CC_Default, "You are in a different region than %s", c->GetTarget()->GetName());
			auto position = glm::vec3(c->GetX(), c->GetY(), c->GetZ());
			auto other_position = glm::vec3(c->GetTarget()->GetX(), c->GetTarget()->GetY(), c->GetTarget()->GetZ());
			c->Message(CC_Default,"Your region: %d Target region: %d", zone->watermap->ReturnRegionType(position), zone->watermap->ReturnRegionType(other_position));
		}
		if (c->GetTarget()->CheckLosFN(c))
			c->Message(CC_Default, " %s has LOS to you.", c->GetTarget()->GetName());
		else
			c->Message(CC_Default, " %s does not have LOS to you.", c->GetTarget()->GetName());
		c->Message(CC_Default, " %s is at heading %.3f pitch %.3f", c->GetTarget()->GetName(), c->CalculateHeadingToTarget(c->GetTarget()->GetX(), c->GetTarget()->GetY()), c->CalculatePitchToTarget(c->GetTarget()->GetPosition()));
	}
	else
	{
		c->Message(CC_Default, "ERROR: Target required");
	}
}

void command_npcsay(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetTarget()->IsNPC() && sep->arg[1][0])
	{
		c->GetTarget()->Say(sep->argplus[1]);
	}
	else
	{
		c->Message(CC_Default, "Usage: #npcsay message (requires NPC target");
	}
}

void command_npcshout(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetTarget()->IsNPC() && sep->arg[1][0])
	{
		c->GetTarget()->Shout(sep->argplus[1]);
	}
	else
	{
		c->Message(CC_Default, "Usage: #npcshout message (requires NPC target");
	}
}

void command_timers(Client *c, const Seperator *sep){
	if (!c->GetTarget() || !c->GetTarget()->IsClient()) {
		c->Message(CC_Default, "Need a player target for timers.");
		return;
	}
	Client *them = c->GetTarget()->CastToClient();

	std::vector< std::pair<pTimerType, PersistentTimer *> > res;
	them->GetPTimers().ToVector(res);

	c->Message(CC_Default, "Timers for target:");

	int r;
	int l = res.size();
	for (r = 0; r < l; r++) {
		c->Message(CC_Default, "Timer %d: %d seconds remain.", res[r].first, res[r].second->GetRemainingTime());
	}
}

void command_npcemote(Client *c, const Seperator *sep){
	if (c->GetTarget() && c->GetTarget()->IsNPC() && sep->arg[1][0])
	{
		c->GetTarget()->Emote(sep->argplus[1]);
	}
	else
	{
		c->Message(CC_Default, "Usage: #npcemote message (requires NPC target");
	}
}

void command_npcedit(Client *c, const Seperator *sep){
	if (!c->GetTarget() || !c->GetTarget()->IsNPC())
	{
		c->Message(CC_Default, "Error: Must have NPC targeted");
		return;
	}

	if (strcasecmp(sep->arg[1], "help") == 0) {

		c->Message(CC_Default, "Help File for #npcedit. Syntax for commands are:");
		c->Message(CC_Default, "#npcedit Name - Sets an NPC's name");
		c->Message(CC_Default, "#npcedit Lastname - Sets an NPC's lastname");
		c->Message(CC_Default, "#npcedit Level - Sets an NPC's level");
		c->Message(CC_Default, "#npcedit Maxlevel - Sets an NPC's maximum level");
		c->Message(CC_Default, "#npcedit Race - Sets an NPC's race");
		c->Message(CC_Default, "#npcedit Class - Sets an NPC's class");
		c->Message(CC_Default, "#npcedit Bodytype - Sets an NPC's bodytype");
		c->Message(CC_Default, "#npcedit HP - Sets an NPC's hitpoints");
		c->Message(CC_Default, "#npcedit Gender - Sets an NPC's gender");
		c->Message(CC_Default, "#npcedit Texture - Sets an NPC's texture");
		c->Message(CC_Default, "#npcedit Helmtexture - Sets an NPC's helmtexture");
		c->Message(CC_Default, "#npcedit Armtexture - Sets an NPC's arm texture");
		c->Message(CC_Default, "#npcedit Bracertexture - Sets an NPC's bracer texture");
		c->Message(CC_Default, "#npcedit Handtexture - Sets an NPC's hand texture");
		c->Message(CC_Default, "#npcedit Legtexture - Sets an NPC's leg texture");
		c->Message(CC_Default, "#npcedit Feettexture - Sets an NPC's feettexture");
		c->Message(CC_Default, "#npcedit Size - Sets an NPC's size");
		c->Message(CC_Default, "#npcedit Hpregen - Sets an NPC's hitpoint regen rate per tick");
		c->Message(CC_Default, "#npcedit Manaregen - Sets an NPC's mana regen rate per tick");
		c->Message(CC_Default, "#npcedit Loottable - Sets the loottable ID for an NPC ");
		c->Message(CC_Default, "#npcedit Merchantid - Sets the merchant ID for an NPC");
		c->Message(CC_Default, "#npcedit npc_spells_effects_id - Sets the NPC Spell Effects ID");
		c->Message(CC_Default, "#npcedit special_abilities - Sets the NPC's Special Abilities");
		c->Message(CC_Default, "#npcedit Spell - Sets the npc spells list ID for an NPC");
		c->Message(CC_Default, "#npcedit Faction - Sets the NPC's faction id");
		c->Message(CC_Default, "#npcedit Mindmg - Sets an NPC's minimum damage");
		c->Message(CC_Default, "#npcedit Maxdmg - Sets an NPC's maximum damage");
		c->Message(CC_Default, "#npcedit Aggroradius - Sets an NPC's aggro radius");
		c->Message(CC_Default, "#npcedit Assistradius - Sets an NPC's assist radius");
		c->Message(CC_Default, "#npcedit Social - Set to 1 if an NPC should assist others on its faction");
		c->Message(CC_Default, "#npcedit Runspeed - Sets an NPC's run speed");
		c->Message(CC_Default, "#npcedit AGI - Sets an NPC's Agility");
		c->Message(CC_Default, "#npcedit CHA - Sets an NPC's Charisma");
		c->Message(CC_Default, "#npcedit DEX - Sets an NPC's Dexterity");
		c->Message(CC_Default, "#npcedit INT - Sets an NPC's Intelligence");
		c->Message(CC_Default, "#npcedit STA - Sets an NPC's Stamina");
		c->Message(CC_Default, "#npcedit STR - Sets an NPC's Strength");
		c->Message(CC_Default, "#npcedit WIS - Sets an NPC's Wisdom");
		c->Message(CC_Default, "#npcedit MR - Sets an NPC's Magic Resistance");
		c->Message(CC_Default, "#npcedit PR - Sets an NPC's Poison Resistance");
		c->Message(CC_Default, "#npcedit DR - Sets an NPC's Disease Resistance");
		c->Message(CC_Default, "#npcedit FR - Sets an NPC's Fire Resistance");
		c->Message(CC_Default, "#npcedit CR - Sets an NPC's cold resistance");
		c->Message(CC_Default, "#npcedit Seeinvis - Sets an NPC's ability to see invis");
		c->Message(CC_Default, "#npcedit Seeinvisundead - Sets an NPC's ability to see through invis vs. undead");
		c->Message(CC_Default, "#npcedit Seehide - Sets an NPC's ability to see through hide");
		c->Message(CC_Default, "#npcedit Seeimprovedhide - Sets an NPC's ability to see through improved hide");
		c->Message(CC_Default, "#npcedit AC - Sets an NPC's Armor Class");
		c->Message(CC_Default, "#npcedit ATK - Sets an NPC's Attack");
		c->Message(CC_Default, "#npcedit Accuracy - Sets an NPC's Accuracy");
		c->Message(CC_Default, "#npcedit npcaggro - Sets an NPC's npc_aggro flag");
		c->Message(CC_Default, "#npcedit qglobal - Sets an NPC's quest global flag");
		c->Message(CC_Default, "#npcedit limit - Sets an NPC's spawn limit counter");
		c->Message(CC_Default, "#npcedit Attackspeed - Sets an NPC's attack speed modifier");
		c->Message(CC_Default, "#npcedit Attackdelay - Sets an NPC's attack delay");
		c->Message(CC_Default, "#npcedit wep1 - Sets an NPC's primary weapon model");
		c->Message(CC_Default, "#npcedit wep2 - Sets an NPC's secondary weapon model");
		c->Message(CC_Default, "#npcedit featuresave - Saves all current facial features to the database");
		c->Message(CC_Default, "#npcedit color - Sets an NPC's red, green, and blue armor tint");
		c->Message(CC_Default, "#npcedit armortint_id - Set an NPC's Armor tint ID");
		c->Message(CC_Default, "#npcedit setanimation - Set an NPC's animation on spawn (Stored in spawn2 table)");
		c->Message(CC_Default, "#npcedit scalerate - Set an NPC's scaling rate");
		c->Message(CC_Default, "#npcedit healscale - Set an NPC's heal scaling rate");
		c->Message(CC_Default, "#npcedit spellscale - Set an NPC's spell scaling rate");
		c->Message(CC_Default, "#npcedit version - Set an NPC's version");

	}

	uint32 npcTypeID = c->GetTarget()->CastToNPC()->GetNPCTypeID();

	if (strcasecmp(sep->arg[1], "name") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has the name %s.", npcTypeID, sep->argplus[2]);

		std::string query = StringFormat("UPDATE npc_types SET name = '%s' WHERE id = %i",
			sep->argplus[2], npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "lastname") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has the lastname %s.", npcTypeID, sep->argplus[2]);

		std::string query = StringFormat("UPDATE npc_types SET lastname = '%s' WHERE id = %i",
			sep->argplus[2], npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "race") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has the race %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET race = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "class") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now class %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET class = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "bodytype") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has type %i bodytype.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET bodytype = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "hp") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Hitpoints.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET hp = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "gender") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now gender %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET gender = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "texture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses texture %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET texture = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "helmtexture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses helmtexture %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET helmtexture = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "armtexture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses armtexture %i.", npcTypeID, atoi(sep->argplus[2]));
		std::string query = StringFormat("UPDATE npc_types SET armtexture = %i WHERE id = %i", atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "bracertexture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses bracertexture %i.", npcTypeID, atoi(sep->argplus[2]));
		std::string query = StringFormat("UPDATE npc_types SET bracertexture = %i WHERE id = %i", atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "handtexture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses handtexture %i.", npcTypeID, atoi(sep->argplus[2]));
		std::string query = StringFormat("UPDATE npc_types SET handtexture = %i WHERE id = %i", atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "legtexture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses legtexture %i.", npcTypeID, atoi(sep->argplus[2]));
		std::string query = StringFormat("UPDATE npc_types SET legtexture = %i WHERE id = %i", atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "feettexture") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses feettexture %i.", npcTypeID, atoi(sep->argplus[2]));
		std::string query = StringFormat("UPDATE npc_types SET feettexture = %i WHERE id = %i", atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "size") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now size %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET size = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "hpregen") == 0) {
		c->Message(CC_Yellow, "NPCID %u now regens %i hitpoints per tick.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET hp_regen_rate = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "manaregen") == 0) {
		c->Message(CC_Yellow, "NPCID %u now regens %i mana per tick.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET mana_regen_rate = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "loottable") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now on loottable_id %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET loottable_id = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "merchantid") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now merchant_id %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET merchant_id = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "npc_spells_effects_id") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has field 'npc_spells_effects_id' set to %s.", npcTypeID, sep->argplus[2]);

		std::string query = StringFormat("UPDATE npc_types SET npc_spells_effects_id = '%s' WHERE id = %i",
			sep->argplus[2], npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "special_abilities") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has field 'special_abilities' set to %s.", npcTypeID, sep->argplus[2]);

		std::string query = StringFormat("UPDATE npc_types SET special_abilities = '%s' WHERE id = %i",
			sep->argplus[2], npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "spell") == 0) {
		c->Message(CC_Yellow, "NPCID %u now uses spell list %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET npc_spells_id = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "faction") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now faction %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET npc_faction_id = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "mindmg") == 0) {
		c->Message(CC_Yellow, "NPCID %u now hits for a min of %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET mindmg = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "maxdmg") == 0) {
		c->Message(CC_Yellow, "NPCID %u now hits for a max of %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET maxdmg = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "aggroradius") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has an aggro radius of %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET aggroradius = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "assistradius") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has an assist radius of %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET assistradius = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "social") == 0) {
		c->Message(CC_Yellow, "NPCID %u social status is now %i", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET social = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "runspeed") == 0) {
		c->Message(CC_Yellow, "NPCID %u now runs at %f", npcTypeID, atof(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET runspeed = %f WHERE id = %i",
			atof(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "AGI") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Agility.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET AGI = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "CHA") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Charisma.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET CHA = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "DEX") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Dexterity.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET DEX = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "INT") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Intelligence.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET _INT = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "STA") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Stamina.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET STA = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "STR") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Strength.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET STR = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "WIS") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a Magic Resistance of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET WIS = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "MR") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a Magic Resistance of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET MR = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "DR") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a Disease Resistance of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET DR = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "CR") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a Cold Resistance of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET CR = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "FR") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a Fire Resistance of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET FR = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "PR") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a Poison Resistance of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET PR = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "seeinvis") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has seeinvis set to %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET see_invis = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "seeinvisundead") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has seeinvisundead set to %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET see_invis_undead = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "seesneak") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has seesneak set to %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET see_sneak = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "seeimprovedhide") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has seeimprovedhide set to %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET see_improved_hide = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "AC") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Armor Class.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET ac = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "ATK") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Attack.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET atk = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "Accuracy") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i Accuracy.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET accuracy = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "level") == 0) {
		c->Message(CC_Yellow, "NPCID %u is now level %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET level = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "maxlevel") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a maximum level of %i.", npcTypeID, atoi(sep->argplus[2]));

		std::string query = StringFormat("UPDATE npc_types SET maxlevel = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "qglobal") == 0) {
		c->Message(CC_Yellow, "Quest globals have been %s for NPCID %u",
			atoi(sep->arg[2]) == 0 ? "disabled" : "enabled", npcTypeID);

		std::string query = StringFormat("UPDATE npc_types SET qglobal = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "npcaggro") == 0) {
		c->Message(CC_Yellow, "NPCID %u will now %s other NPCs with negative faction npc_value",
			npcTypeID, atoi(sep->arg[2]) == 0 ? "not aggro" : "aggro");

		std::string query = StringFormat("UPDATE npc_types SET npc_aggro = %i WHERE id = %i",
			atoi(sep->argplus[2]) == 0 ? 0 : 1, npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "limit") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a spawn limit of %i",
			npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET limit = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "Attackdelay") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has attack_delay set to %i", npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET attack_delay = %i WHERE id = %i", atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}


	if (strcasecmp(sep->arg[1], "wep1") == 0) {
		c->Message(CC_Yellow, "NPCID %u will have item graphic %i set to his primary on repop.",
			npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET d_melee_texture1 = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "wep2") == 0) {
		c->Message(CC_Yellow, "NPCID %u will have item graphic %i set to his secondary on repop.",
			npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET d_melee_texture2 = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "featuresave") == 0) {
		c->Message(CC_Yellow, "NPCID %u saved with all current facial feature settings",
			npcTypeID);

		Mob* target = c->GetTarget();

		std::string query = StringFormat("UPDATE npc_types "
			"SET luclin_haircolor = %i, luclin_beardcolor = %i, "
			"luclin_hairstyle = %i, luclin_beard = %i, "
			"face = %i "
			"WHERE id = %i",
			target->GetHairColor(), target->GetBeardColor(),
			target->GetHairStyle(), target->GetBeard(),
			target->GetLuclinFace(), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "color") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has %i red, %i green, and %i blue tinting on their armor.",
			npcTypeID, atoi(sep->arg[2]), atoi(sep->arg[3]), atoi(sep->arg[4]));

		std::string query = StringFormat("UPDATE npc_types "
			"SET armortint_red = %i, armortint_green = %i, armortint_blue = %i "
			"WHERE id = %i",
			atoi(sep->arg[2]), atoi(sep->arg[3]), atoi(sep->arg[4]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "armortint_id") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has field 'armortint_id' set to %s",
			npcTypeID, sep->arg[2]);

		std::string query = StringFormat("UPDATE npc_types SET armortint_id = '%s' WHERE id = %i",
			sep->argplus[2], npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "setanimation") == 0) {
		int animation = 0;
		if (sep->arg[2] && atoi(sep->arg[2]) <= 4) {
			if ((strcasecmp(sep->arg[2], "stand") == 0) || atoi(sep->arg[2]) == 0)
				animation = 0; //Stand
			if ((strcasecmp(sep->arg[2], "sit") == 0) || atoi(sep->arg[2]) == 1)
				animation = 1; //Sit
			if ((strcasecmp(sep->arg[2], "crouch") == 0) || atoi(sep->arg[2]) == 2)
				animation = 2; //Crouch
			if ((strcasecmp(sep->arg[2], "dead") == 0) || atoi(sep->arg[2]) == 3)
				animation = 3; //Dead
			if ((strcasecmp(sep->arg[2], "loot") == 0) || atoi(sep->arg[2]) == 4)
				animation = 4; //Looting Animation
		}
		else {
			c->Message(CC_Default, "You must specifiy an animation stand, sit, crouch, dead, loot (0-4)");
			c->Message(CC_Default, "Example: #npcedit setanimation sit");
			c->Message(CC_Default, "Example: #npcedit setanimation 0");
			return;
		}

		c->Message(CC_Yellow,"NPCID %u now has the animation set to %i on spawn with spawngroup %i", npcTypeID, animation, c->GetTarget()->CastToNPC()->GetSp2() );

		std::string query = StringFormat("UPDATE spawn2 SET animation = %i "
			"WHERE spawngroupID = %i",
			animation, c->GetTarget()->CastToNPC()->GetSp2());
		database.QueryDatabase(query);

		c->GetTarget()->SetAppearance(EmuAppearance(animation));
		return;
		}

	if (strcasecmp(sep->arg[1], "scalerate") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a scaling rate of %i.",
			npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET scalerate = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "healscale") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a heal scaling rate of %i.",
			npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET healscale = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if (strcasecmp(sep->arg[1], "spellscale") == 0) {
		c->Message(CC_Yellow, "NPCID %u now has a spell scaling rate of %i.",
			npcTypeID, atoi(sep->arg[2]));

		std::string query = StringFormat("UPDATE npc_types SET spellscale = %i WHERE id = %i",
			atoi(sep->argplus[2]), npcTypeID);
		database.QueryDatabase(query);
		return;
	}

	if ((sep->arg[1][0] == 0 || strcasecmp(sep->arg[1], "*") == 0) || ((c->GetTarget() == 0) || (c->GetTarget()->IsClient())))
		c->Message(CC_Default, "Type #npcedit help for more info");

}

#ifdef PACKET_PROFILER
void command_packetprofile(Client *c, const Seperator *sep){
	Client *t = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		t = c->GetTarget()->CastToClient();
	}
	c->DumpPacketProfile();
}
#endif

#ifdef EQPROFILE
void command_profiledump(Client *c, const Seperator *sep){
	DumpZoneProfile();
}

void command_profilereset(Client *c, const Seperator *sep){
	ResetZoneProfile();
}
#endif

void command_opcode(Client *c, const Seperator *sep){
	if (!strcasecmp(sep->arg[1], "reload")) {
		ReloadAllPatches();
		c->Message(CC_Default, "Opcodes for all patches have been reloaded");
	}
}

void command_qglobal(Client *c, const Seperator *sep)
{
	//In-game switch for qglobal column
	if (sep->arg[1][0] == 0) {
		c->Message(CC_Default, "Syntax: #qglobal [on/off/view]. Requires NPC target.");
		return;
	}

	Mob *target = c->GetTarget();

	if (!target || !target->IsNPC()) {
		c->Message(CC_Red, "NPC Target Required!");
		return;
	}

	if (!strcasecmp(sep->arg[1], "on")) {
		std::string query = StringFormat("UPDATE npc_types SET qglobal = 1 WHERE id = '%i'",
			target->GetNPCTypeID());
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Yellow, "Could not update database.");
			return;
		}

        c->Message(CC_Yellow, "Success! Changes take effect on zone reboot.");
		return;
	}

	if (!strcasecmp(sep->arg[1], "off")) {
		std::string query = StringFormat("UPDATE npc_types SET qglobal = 0 WHERE id = '%i'",
			target->GetNPCTypeID());
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Yellow, "Could not update database.");
			return;
		}

		c->Message(CC_Yellow, "Success! Changes take effect on zone reboot.");
		return;
	}

	if (!strcasecmp(sep->arg[1], "view")) {
		const NPCType *type = database.GetNPCType(target->GetNPCTypeID());
		if (!type)
			c->Message(CC_Yellow, "Invalid NPC type.");
		else if (type->qglobal)
			c->Message(CC_Yellow, "This NPC has quest globals active.");
		else
			c->Message(CC_Yellow, "This NPC has quest globals disabled.");
		return;
	}

	c->Message(CC_Yellow, "Invalid action specified.");
}

void command_path(Client *c, const Seperator *sep)
{
	if (zone->pathing) {
		zone->pathing->DebugCommand(c, sep);
	}
}

void command_ginfo(Client *c, const Seperator *sep){
	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	Group *g = t->GetGroup();
	if (!g)
	{
		Raid* r = t->GetRaid();
		if (r)
		{
			uint32 t_group_id = r->GetGroup(t);
			c->Message(CC_Default, "Client %s is in a raid.  Raid ID: %u;  Raid leader: %s;  Raid count: %u;  Raid group ID: %u",
				t->GetName(), r->GetID(), r->leadername, r->RaidCount(), t_group_id);

			if (t->IsRaidGrouped())
			{
				for (int i = 0; i < MAX_RAID_MEMBERS; i++)
				{
					Client* rmember = r->members[i].member;
					if (r->members[i].GroupNumber == t_group_id)
					{
						if (rmember)
						{
							c->Message(CC_Default, "- name: %s (%s)%s%s HP: %u / %u MP: %u / %u", rmember->GetName(), GetClassIDName(rmember->GetClass()),
								r->members[i].IsGroupLeader ? " (group leader)" : "", r->members[i].IsLooter ? " (looter)" : "",
								rmember->GetHP(), rmember->GetMaxHP(), rmember->GetMana(), rmember->GetMaxMana());
						}
						else if (r->members[i].membername[0] != '\0')
						{
							c->Message(CC_Default, "- name: %s (%s)%s%s (not in zone)", r->members[i].membername, GetClassIDName(r->members[i]._class),
								r->members[i].IsGroupLeader ? " (group leader)" : "", r->members[i].IsLooter ? " (looter)" : "");
						}
					}
				}
			}
			else
				c->Message(CC_Default, "Client is not in a raid group");
		}
		else
			c->Message(CC_Default, "Client %s is not in a group or raid", t->GetName());

		return;
	}

	c->Message(CC_Default, "Player %s is in group ID %u with %u members.  Group leader is: %s", t->GetName(), g->GetID(), g->GroupCount(), g->GetLeaderName());

	uint32 r;
	for (r = 0; r < MAX_GROUP_MEMBERS; r++) {
		if (g->members[r] == nullptr) {
			if (g->membername[r][0] == '\0')
				continue;
			c->Message(CC_Default, "- Zoned Member: %s", g->membername[r]);
		}
		else {
			c->Message(CC_Default, "- In-Zone Member: %s (%s) level: %u HP: %u / %u MP: %u / %u", g->membername[r], GetClassIDName(g->members[r]->GetClass()), 
				g->members[r]->GetLevel(), g->members[r]->GetHP(), g->members[r]->GetMaxHP(), g->members[r]->GetMana(), g->members[r]->GetMaxMana());
		}
	}
}

void command_hp(Client *c, const Seperator *sep){
	c->SendHPUpdate();
	c->SendManaUpdatePacket();
}

void command_aggro(Client *c, const Seperator *sep){
	if (c->GetTarget() == nullptr || !c->GetTarget()->IsNPC()) {
		c->Message(CC_Default, "Error: you must have an NPC target.");
		return;
	}
	float d = atof(sep->arg[1]);
	if (d == 0.0f) {
		c->Message(CC_Red, "Error: distance argument required.");
		return;
	}
	bool verbose = false;
	if (sep->arg[2][0] == '-' && sep->arg[2][1] == 'v' && sep->arg[2][2] == '\0') {
		verbose = true;
	}

	entity_list.DescribeAggro(c, c->GetTarget()->CastToNPC(), d, verbose);
}

void command_pf(Client *c, const Seperator *sep){
	if (c->GetTarget())
	{
		Mob *who = c->GetTarget();
		c->Message(CC_Default, "POS: (%.2f, %.2f, %.2f)", who->GetX(), who->GetY(), who->GetZ());
		c->Message(CC_Default, "WP: %s (%d/%d)", to_string(who->GetCurrentWayPoint()).c_str(), who->IsNPC()?who->CastToNPC()->GetMaxWp():-1);
		c->Message(CC_Default, "TAR: (%.2f, %.2f, %.2f)", who->GetTarX(), who->GetTarY(), who->GetTarZ());
		c->Message(CC_Default, "TARV: (%.2f, %.2f, %.2f)", who->GetTarVX(), who->GetTarVY(), who->GetTarVZ());
		c->Message(CC_Default, "|TV|=%.2f index=%d", who->GetTarVector(), who->GetTarNDX());
		c->Message(CC_Default, "pause=%d RAspeed=%d", who->GetCWPP(), who->GetRunAnimSpeed());
	}
	else {
		c->Message(CC_Default, "ERROR: target required");
	}
}

void command_bestz(Client *c, const Seperator *sep)
{
	float xcoord = c->GetX();
	float ycoord = c->GetY();
	if (sep->IsNumber(1) && sep->IsNumber(2))
	{
		xcoord = atof(sep->arg[1]);
		ycoord = atof(sep->arg[2]);
	}

	if (zone->zonemap == nullptr) {
		c->Message(CC_Default, "Map not loaded for this zone");
	}
	else {
		glm::vec3 me;
		me.x = xcoord;
		me.y = ycoord;
		me.z = c->GetZ();
		glm::vec3 hit;
		glm::vec3 bme(me);
		bme.z -= 500;

		float best_z = zone->zonemap->FindBestZ(me, &hit);

		if (best_z != BEST_Z_INVALID)
		{
			c->Message(CC_Default, "Z is %.3f at (%.3f, %.3f).", best_z, me.x, me.y);
		}
		else
		{
			c->Message(CC_Default, "Found no Z.");
		}
	}

	if (zone->watermap == nullptr) {
		c->Message(CC_Default, "Water Region Map not loaded for this zone");
	}
	else {
		WaterRegionType RegionType;
		float z;

		if (c->GetTarget()) {
			z = c->GetTarget()->GetZ();
			auto position = glm::vec3(c->GetTarget()->GetX(), c->GetTarget()->GetY(), z);
			RegionType = zone->watermap->ReturnRegionType(position);
			c->Message(CC_Default,"InWater returns %d", zone->watermap->InWater(position));
			c->Message(CC_Default,"InLava returns %d", zone->watermap->InLava(position));

		}
		else {
			z = c->GetZ();
			auto position = glm::vec3(c->GetX(), c->GetY(), z);
			RegionType = zone->watermap->ReturnRegionType(position);
			c->Message(CC_Default,"InWater returns %d", zone->watermap->InWater(position));
			c->Message(CC_Default,"InLava returns %d", zone->watermap->InLava(position));

		}

		switch (RegionType) {
		case RegionTypeNormal:	{ c->Message(CC_Default, "There is nothing special about the region you are in!"); break; }
		case RegionTypeWater:	{ c->Message(CC_Default, "You/your target are in Water."); break; }
		case RegionTypeLava:	{ c->Message(CC_Default, "You/your target are in Lava."); break; }
		case RegionTypeVWater:	{ c->Message(CC_Default, "You/your target are in VWater (Icy Water?)."); break; }
		case RegionTypePVP:	{ c->Message(CC_Default, "You/your target are in a pvp enabled area."); break; }
		case RegionTypeSlime:	{ c->Message(CC_Default, "You/your target are in slime."); break; }
		case RegionTypeIce:	{ c->Message(CC_Default, "You/your target are in ice."); break; }
		default: c->Message(CC_Default, "You/your target are in an unknown region type %d.", (int)RegionType);
		}
	}


}

void command_reloadstatic(Client *c, const Seperator *sep){
	c->Message(CC_Default, "Reloading zone static data...");
	zone->ReloadStaticData();
}

void command_flags(Client *c, const Seperator *sep){
	Client *t = c;

	if (c->Admin() >= minStatusToSeeOthersZoneFlags) {
		Mob *tgt = c->GetTarget();
		if (tgt != nullptr && tgt->IsClient())
			t = tgt->CastToClient();
	}

	t->SendZoneFlagInfo(c);
}

void command_flagedit(Client *c, const Seperator *sep)
{
	//super-command for editing zone flags
	if (sep->arg[1][0] == '\0' || !strcasecmp(sep->arg[1], "help")) {
		c->Message(CC_Default, "Syntax: #flagedit [lockzone|unlockzone|listzones|give|take].");
		c->Message(CC_Default, "...lockzone [zone id/short] [flag name] - Set the specified flag name on the zone, locking the zone");
		c->Message(CC_Default, "...unlockzone [zone id/short] - Removes the flag requirement from the specified zone");
		c->Message(CC_Default, "...listzones - List all zones which require a flag, and their flag's name");
		c->Message(CC_Default, "...give [zone id/short] - Give your target the zone flag for the specified zone.");
		c->Message(CC_Default, "...take [zone id/short] - Take the zone flag for the specified zone away from your target");
		c->Message(CC_Default, "...Note: use #flags to view flags on a person");
		return;
	}

	if (!strcasecmp(sep->arg[1], "lockzone")) {
		uint32 zoneid = 0;
		if (sep->arg[2][0] != '\0') {
			zoneid = atoi(sep->arg[2]);
			if (zoneid < 1) {
				zoneid = database.GetZoneID(sep->arg[2]);
			}
		}
		if (zoneid < 1) {
			c->Message(CC_Red, "zone required. see help.");
			return;
		}

		char flag_name[128];
		if (sep->argplus[3][0] == '\0') {
			c->Message(CC_Red, "flag name required. see help.");
			return;
		}
		database.DoEscapeString(flag_name, sep->argplus[3], 64);
		flag_name[127] = '\0';

		std::string query = StringFormat("UPDATE zone SET flag_needed = '%s' "
			"WHERE zoneidnumber = %d",
			flag_name, zoneid);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Red, "Error updating zone: %s", results.ErrorMessage().c_str());
			return;
		}

        c->Message(CC_Yellow, "Success! Zone %s now requires a flag, named %s", database.GetZoneName(zoneid), flag_name);
        return;
	}

	if (!strcasecmp(sep->arg[1], "unlockzone")) {
		uint32 zoneid = 0;
		if (sep->arg[2][0] != '\0') {
			zoneid = atoi(sep->arg[2]);
			if (zoneid < 1) {
				zoneid = database.GetZoneID(sep->arg[2]);
			}
		}

		if (zoneid < 1) {
			c->Message(CC_Red, "zone required. see help.");
			return;
		}

		std::string query = StringFormat("UPDATE zone SET flag_needed = '' "
			"WHERE zoneidnumber = %d",
			zoneid);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Yellow, "Error updating zone: %s", results.ErrorMessage().c_str());
			return;
		}

		c->Message(CC_Yellow, "Success! Zone %s no longer requires a flag.", database.GetZoneName(zoneid));
		return;
	}

	if (!strcasecmp(sep->arg[1], "listzones")) {
		std::string query = "SELECT zoneidnumber, short_name, long_name, flag_needed FROM zone WHERE flag_needed != ''";
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
            return;
        }

		c->Message(CC_Default, "Zones which require flags:");
		for (auto row = results.begin(); row != results.end(); ++row)
			c->Message(CC_Default, "Zone %s (%s,%s) requires key %s", row[1], row[0], row[2], row[3]);

		return;
	}

	if (!strcasecmp(sep->arg[1], "give")) {
		uint32 zoneid = 0;
		if (sep->arg[2][0] != '\0') {
			zoneid = atoi(sep->arg[2]);
			if (zoneid < 1) {
				zoneid = database.GetZoneID(sep->arg[2]);
			}
		}
		if (zoneid < 1) {
			c->Message(CC_Red, "zone required. see help.");
			return;
		}

		Mob *t = c->GetTarget();
		if (t == nullptr || !t->IsClient()) {
			c->Message(CC_Red, "client target required");
			return;
		}

		t->CastToClient()->SetZoneFlag(zoneid);
		return;
	}

	if (!strcasecmp(sep->arg[1], "give")) {
		uint32 zoneid = 0;
		if (sep->arg[2][0] != '\0') {
			zoneid = atoi(sep->arg[2]);
			if (zoneid < 1) {
				zoneid = database.GetZoneID(sep->arg[2]);
			}
		}
		if (zoneid < 1) {
			c->Message(CC_Red, "zone required. see help.");
			return;
		}

		Mob *t = c->GetTarget();
		if (t == nullptr || !t->IsClient()) {
			c->Message(CC_Red, "client target required");
			return;
		}

		t->CastToClient()->ClearZoneFlag(zoneid);
		return;
	}

	c->Message(CC_Yellow, "Invalid action specified. use '#flagedit help' for help");
}

void command_guildcreate(Client *c, const Seperator *sep){
	if (strlen(sep->argplus[1])>4 && strlen(sep->argplus[1])<16)
	{
		guild_mgr.AddGuildApproval(sep->argplus[1], c);
	}
	else
	{
		c->Message(CC_Default, "Guild name must be more than 4 characters and less than 16.");
	}
}

void command_guildapprove(Client *c, const Seperator *sep){
	guild_mgr.AddMemberApproval(atoi(sep->arg[1]), c);
}

void command_guildlist(Client *c, const Seperator *sep){
	GuildApproval* tmp = guild_mgr.FindGuildByIDApproval(atoi(sep->arg[1]));
	if (tmp)
	{
		tmp->ApprovedMembers(c);
	}
	else
		c->Message(CC_Default, "Could not find reference id.");
}

void command_hatelist(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();
	if (target == nullptr) {
		c->Message(CC_Default, "Error: you must have a target.");
		return;
	}

	c->Message(CC_Default, "Display hate list for %s..", target->GetName());
	target->PrintHateListToClient(c);
}

void command_rules(Client *c, const Seperator *sep){
	//super-command for managing rules settings
	if (sep->arg[1][0] == '\0' || !strcasecmp(sep->arg[1], "help")) {
		c->Message(CC_Default, "Syntax: #rules [subcommand].");
		c->Message(CC_Default, "-- Rule Set Manipulation --");
		c->Message(CC_Default, "...listsets - List avaliable rule sets");
		c->Message(CC_Default, "...current - gives the name of the ruleset currently running in this zone");
		c->Message(CC_Default, "...reload - Reload the selected ruleset in this zone");
		c->Message(CC_Default, "...switch (ruleset name) - Change the selected ruleset and load it");
		c->Message(CC_Default, "...load (ruleset name) - Load a ruleset in just this zone without changing the selected set");
		//too lazy to write this right now:
		//		c->Message(CC_Default, "...wload (ruleset name) - Load a ruleset in all zones without changing the selected set");
		c->Message(CC_Default, "...store [ruleset name] - Store the running ruleset as the specified name");
		c->Message(CC_Default, "---------------------");
		c->Message(CC_Default, "-- Running Rule Manipulation --");
		c->Message(CC_Default, "...reset - Reset all rules to their default values");
		c->Message(CC_Default, "...get [rule] - Get the specified rule's local value");
		c->Message(CC_Default, "...set (rule) (value) - Set the specified rule to the specified value locally only");
		c->Message(CC_Default, "...setdb (rule) (value) - Set the specified rule to the specified value locally and in the DB");
		c->Message(CC_Default, "...list [catname] - List all rules in the specified category (or all categiries if omitted)");
		c->Message(CC_Default, "...values [catname] - List the value of all rules in the specified category");
		return;
	}

	if (!strcasecmp(sep->arg[1], "current")) {
		c->Message(CC_Default, "Currently running ruleset '%s' (%d)", RuleManager::Instance()->GetActiveRuleset(),
			RuleManager::Instance()->GetActiveRulesetID());
	}
	else if (!strcasecmp(sep->arg[1], "listsets")) {
		std::map<int, std::string> sets;
		if (!RuleManager::Instance()->ListRulesets(&database, sets)) {
			c->Message(CC_Red, "Failed to list rule sets!");
			return;
		}

		c->Message(CC_Default, "Avaliable rule sets:");
		std::map<int, std::string>::iterator cur, end;
		cur = sets.begin();
		end = sets.end();
		for (; cur != end; ++cur) {
			c->Message(CC_Default, "(%d) %s", cur->first, cur->second.c_str());
		}
	}
	else if (!strcasecmp(sep->arg[1], "reload")) {
		RuleManager::Instance()->LoadRules(&database, RuleManager::Instance()->GetActiveRuleset());
		c->Message(CC_Default, "The active ruleset (%s (%d)) has been reloaded", RuleManager::Instance()->GetActiveRuleset(),
			RuleManager::Instance()->GetActiveRulesetID());
	}
	else if (!strcasecmp(sep->arg[1], "switch")) {
		//make sure this is a valid rule set..
		int rsid = RuleManager::Instance()->GetRulesetID(&database, sep->arg[2]);
		if (rsid < 0) {
			c->Message(CC_Red, "Unknown rule set '%s'", sep->arg[2]);
			return;
		}
		if (!database.SetVariable("RuleSet", sep->arg[2])) {
			c->Message(CC_Red, "Failed to update variables table to change selected rule set");
			return;
		}

		//TODO: we likely want to reload this ruleset everywhere...
		RuleManager::Instance()->LoadRules(&database, sep->arg[2]);

		c->Message(CC_Default, "The selected ruleset has been changed to (%s (%d)) and reloaded locally", sep->arg[2], rsid);
	}
	else if (!strcasecmp(sep->arg[1], "load")) {
		//make sure this is a valid rule set..
		int rsid = RuleManager::Instance()->GetRulesetID(&database, sep->arg[2]);
		if (rsid < 0) {
			c->Message(CC_Red, "Unknown rule set '%s'", sep->arg[2]);
			return;
		}
		RuleManager::Instance()->LoadRules(&database, sep->arg[2]);
		c->Message(CC_Default, "Loaded ruleset '%s' (%d) locally", sep->arg[2], rsid);
	}
	else if (!strcasecmp(sep->arg[1], "store")) {
		if (sep->argnum == 1) {
			//store current rule set.
			RuleManager::Instance()->SaveRules(&database);
			c->Message(CC_Default, "Rules saved");
		}
		else if (sep->argnum == 2) {
			RuleManager::Instance()->SaveRules(&database, sep->arg[2]);
			int prersid = RuleManager::Instance()->GetActiveRulesetID();
			int rsid = RuleManager::Instance()->GetRulesetID(&database, sep->arg[2]);
			if (rsid < 0) {
				c->Message(CC_Red, "Unable to query ruleset ID after store, it most likely failed.");
			}
			else {
				c->Message(CC_Default, "Stored rules as ruleset '%s' (%d)", sep->arg[2], rsid);
				if (prersid != rsid) {
					c->Message(CC_Default, "Rule set %s (%d) is now active in this zone", sep->arg[2], rsid);
				}
			}
		}
		else {
			c->Message(CC_Red, "Invalid argument count, see help.");
			return;
		}
	}
	else if (!strcasecmp(sep->arg[1], "reset")) {
		RuleManager::Instance()->ResetRules();
		c->Message(CC_Default, "The running ruleset has been set to defaults");

	}
	else if (!strcasecmp(sep->arg[1], "get")) {
		if (sep->argnum != 2) {
			c->Message(CC_Red, "Invalid argument count, see help.");
			return;
		}
		std::string value;
		if (!RuleManager::Instance()->GetRule(sep->arg[2], value))
			c->Message(CC_Red, "Unable to find rule %s", sep->arg[2]);
		else
			c->Message(CC_Default, "%s - %s", sep->arg[2], value.c_str());

	}
	else if (!strcasecmp(sep->arg[1], "set")) {
		if (sep->argnum != 3) {
			c->Message(CC_Red, "Invalid argument count, see help.");
			return;
		}
		if (!RuleManager::Instance()->SetRule(sep->arg[2], sep->arg[3])) {
			c->Message(CC_Red, "Failed to modify rule");
		}
		else {
			c->Message(CC_Default, "Rule modified locally.");
		}
	}
	else if (!strcasecmp(sep->arg[1], "setdb")) {
		if (sep->argnum != 3) {
			c->Message(CC_Red, "Invalid argument count, see help.");
			return;
		}
		if (!RuleManager::Instance()->SetRule(sep->arg[2], sep->arg[3], &database, true)) {
			c->Message(CC_Red, "Failed to modify rule");
		}
		else {
			c->Message(CC_Default, "Rule modified locally and in the database.");
		}
	}
	else if (!strcasecmp(sep->arg[1], "list")) {
		if (sep->argnum == 1) {
			std::vector<const char *> rule_list;
			if (!RuleManager::Instance()->ListCategories(rule_list)) {
				c->Message(CC_Red, "Failed to list categories!");
				return;
			}
			c->Message(CC_Default, "Rule Categories:");
			std::vector<const char *>::iterator cur, end;
			cur = rule_list.begin();
			end = rule_list.end();
			for (; cur != end; ++cur) {
				c->Message(CC_Default, " %s", *cur);
			}
		}
		else if (sep->argnum == 2) {
			const char *catfilt = nullptr;
			if (std::string("all") != sep->arg[2])
				catfilt = sep->arg[2];
			std::vector<const char *> rule_list;
			if (!RuleManager::Instance()->ListRules(catfilt, rule_list)) {
				c->Message(CC_Red, "Failed to list rules!");
				return;
			}
			c->Message(CC_Default, "Rules in category %s:", sep->arg[2]);
			std::vector<const char *>::iterator cur, end;
			cur = rule_list.begin();
			end = rule_list.end();
			for (; cur != end; ++cur) {
				c->Message(CC_Default, " %s", *cur);
			}
		}
		else {
			c->Message(CC_Red, "Invalid argument count, see help.");
		}
	}
	else if (!strcasecmp(sep->arg[1], "values")) {
		if (sep->argnum != 2) {
			c->Message(CC_Red, "Invalid argument count, see help.");
			return;
		}
		else {
			const char *catfilt = nullptr;
			if (std::string("all") != sep->arg[2])
				catfilt = sep->arg[2];
			std::vector<const char *> rule_list;
			if (!RuleManager::Instance()->ListRules(catfilt, rule_list)) {
				c->Message(CC_Red, "Failed to list rules!");
				return;
			}
			c->Message(CC_Default, "Rules & values in category %s:", sep->arg[2]);
			std::vector<const char *>::iterator cur, end;
			cur = rule_list.begin();
			end = rule_list.end();
			for (std::string tmp_value; cur != end; ++cur) {
				if (RuleManager::Instance()->GetRule(*cur, tmp_value))
					c->Message(CC_Default, " %s - %s", *cur, tmp_value.c_str());
			}
		}

	}
	else {
		c->Message(CC_Yellow, "Invalid action specified. use '#rules help' for help");
	}
}

void command_reloadtitles(Client *c, const Seperator *sep){
	auto pack = new ServerPacket(ServerOP_ReloadTitles, 0);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	c->Message(CC_Yellow, "Player Titles Reloaded.");

}

void command_allowexport(Client* c, const Seperator* sep) {
	int arguments = sep->argnum;

	if (!strcasecmp(sep->arg[1], "status")) {
		uint8 flagval = c->GetCharExportFlag();
		if (flagval == 0)
			c->Message(CC_Default, "Export is currently off.");
		else if (flagval == 1)
			c->Message(CC_Default, "Export is currently set to include character info and worn items.");
		else if (flagval == 2)
			c->Message(CC_Default, "Export is currently set to include character info, worn items and inventory.");
		else if (flagval == 3)
			c->Message(CC_Default, "Export is currently set to include character info, worn items, inventory, and bank.");
		return;
	}
	if (!strcasecmp(sep->arg[1], "off")) {
		c->SetCharExportFlag(0);
		return;
	}
	else if (!strcasecmp(sep->arg[1], "worn")) {
		c->SetCharExportFlag(1);
		return;
	}
	else if (!strcasecmp(sep->arg[1], "inventory")) {
		c->SetCharExportFlag(2);
		return;
	}
	else if (!strcasecmp(sep->arg[1], "bank")) {
		c->SetCharExportFlag(3);
		return;
	}

	if (arguments) 
	{ 
		c->Message(CC_Default, "Invalid argument"); 
	}
	else 
	{ 
		c->Message(CC_Default, "All export options will include character name, level, guild, stats, skills, factions, and keyrings.");
	}

	c->Message(CC_Default, "Usage:");
	c->Message(CC_Default, "#allowexport status - shows your current setting.");
	c->Message(CC_Default, "#allowexport worn - include character name, level, guild, stats, skills, factions, and keyrings.");
	c->Message(CC_Default, "#allowexport inventory - Includes worn, and the character's inventory.");
	c->Message(CC_Default, "#allowexport bank - In addition to worn and inventory, include bank.");
	c->Message(CC_Default, "#allowexport off - turns off future exports.");
	return;
}

void command_altactivate(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == '\0'){
		c->Message(CC_Default, "Invalid argument, usage:");
		c->Message(CC_Default, "#altactivate list - lists the AA ID numbers that are available to you");
		c->Message(CC_Default, "#altactivate time [argument] - returns the time left until you can use the AA with the ID that matches the argument.");
		c->Message(CC_Default, "#altactivate [argument] - activates the AA with the ID that matches the argument.");
		return;
	}
	if (!strcasecmp(sep->arg[1], "help")){
		c->Message(CC_Default, "Usage:");
		c->Message(CC_Default, "#altactivate list - lists the AA ID numbers that are available to you");
		c->Message(CC_Default, "#altactivate time [argument] - returns the time left until you can use the AA with the ID that matches the argument.");
		c->Message(CC_Default, "#altactivate [argument] - activates the AA with the ID that matches the argument.");
		return;
	}
	if (!strcasecmp(sep->arg[1], "list")){
		c->Message(CC_Default, "You have access to the following AA Abilities:");
		int x, val;
		SendAA_Struct* saa = nullptr;
		for (x = 0; x < aaHighestID; x++){
			if (AA_Actions[x][0].spell_id || AA_Actions[x][0].action){ //if there's an action or spell associated we assume it's a valid
				val = 0;					//and assume if they don't have a value for the first rank then it isn't valid for any rank
				saa = nullptr;
				val = c->GetAA(x);
				if (val){
					saa = zone->FindAA(x, false);
					c->Message(CC_Default, "%d: %s %d", x, saa->name, val);
				}
			}
		}
	}
	else if (!strcasecmp(sep->arg[1], "time")){
		int ability = atoi(sep->arg[2]);
		if (c->GetAA(ability)){
			int remain = c->GetPTimers().GetRemainingTime(pTimerAAStart + ability);
			if (remain)
				c->Message(CC_Default, "You may use that ability in %d minutes and %d seconds.", (remain / 60), (remain % 60));
			else
				c->Message(CC_Default, "You may use that ability now.");
		}
		else{
			c->Message(CC_Default, "You do not have access to that ability.");
		}
	}
	else
	{
		c->ActivateAA((aaID)atoi(sep->arg[1]));
	}
}

void command_setgraveyard(Client *c, const Seperator *sep){
	uint32 zoneid = 0;
	uint32 graveyard_id = 0;
	Client *t = c;

	if (c->GetTarget() && c->GetTarget()->IsClient() && c->GetGM())
		t = c->GetTarget()->CastToClient();

	if (!sep->arg[1][0]) {
		c->Message(CC_Default, "Usage: #setgraveyard [zonename]");
		return;
	}

	zoneid = database.GetZoneID(sep->arg[1]);

	if (zoneid > 0) {
		graveyard_id = database.CreateGraveyardRecord(zoneid, t->GetPosition());

		if (graveyard_id > 0) {
			c->Message(CC_Default, "Successfuly added a new record for this graveyard!");
			if (database.AddGraveyardIDToZone(zoneid, graveyard_id) > 0) {
				c->Message(CC_Default, "Successfuly added this new graveyard for the zone %s.", sep->arg[1]);
				// TODO: Set graveyard data to the running zone process.
				c->Message(CC_Default, "Done!");
			}
			else
				c->Message(CC_Default, "Unable to add this new graveyard to the zone %s.", sep->arg[1]);
		}
		else {
			c->Message(CC_Default, "Unable to create a new graveyard record in the database.");
		}
	}
	else {
		c->Message(CC_Default, "Unable to retrieve a ZoneID for the zone: %s", sep->arg[1]);
	}

	return;
}

void command_deletegraveyard(Client *c, const Seperator *sep){
	uint32 zoneid = 0;
	uint32 graveyard_id = 0;

	if (!sep->arg[1][0]) {
		c->Message(CC_Default, "Usage: #deletegraveyard [zonename]");
		return;
	}

	zoneid = database.GetZoneID(sep->arg[1]);
	graveyard_id = database.GetZoneGraveyardID(zoneid);

	if (zoneid > 0 && graveyard_id > 0) {
		if (database.DeleteGraveyard(zoneid, graveyard_id))
			c->Message(CC_Default, "Successfuly deleted graveyard %u for zone %s.", graveyard_id, sep->arg[1]);
		else
			c->Message(CC_Default, "Unable to delete graveyard %u for zone %s.", graveyard_id, sep->arg[1]);
	}
	else {
		if (zoneid <= 0)
			c->Message(CC_Default, "Unable to retrieve a ZoneID for the zone: %s", sep->arg[1]);
		else if (graveyard_id <= 0)
			c->Message(CC_Default, "Unable to retrieve a valid GraveyardID for the zone: %s", sep->arg[1]);
	}

	return;
}

void command_refreshgroup(Client *c, const Seperator *sep){
	if (!c)
		return;

	Group *g = c->GetGroup();

	if (!g)
		return;

	database.RefreshGroupFromDB(c);
	//g->SendUpdate(7, c);
}

void command_advnpcspawn(Client* c, const Seperator* sep)
{
	int arguments = sep->argnum;
	if (!arguments) {
		c->Message(CC_Default, "Usage: #advnpcspawn addentry [Spawngroup ID] [NPC ID] [Spawn Chance] - Adds a new Spawngroup Entry");
		c->Message(CC_Default, "Usage: #advnpcspawn addspawn [Spawngroup ID] - Adds a new Spawngroup Entry from an existing Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn clearbox [Spawngroup ID] - Clears the roambox of a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn deletespawn - Deletes a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn editbox [Spawngroup ID] [Minimum X] [Maximum X] [Minimum Y] [Maximum Y] [Delay]  - Edit the roambox of a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn editrespawn [Respawn Timer] [Variance] - Edit the Respawn Timer of a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn makegroup [Spawn Group Name] [Spawn Limit] [Minimum X] [Maximum X] [Minimum Y] [Maximum Y] [Delay] - Makes a new Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn makenpc - Makes a new NPC");
		c->Message(CC_Default, "Usage: #advnpcspawn movespawn - Moves a Spawngroup to your current location");
		return;
	}

	std::string spawn_command = Strings::ToLower(sep->arg[1]);
	bool is_add_entry = spawn_command.find("addentry") != std::string::npos;
	bool is_add_spawn = spawn_command.find("addspawn") != std::string::npos;
	bool is_clear_box = spawn_command.find("clearbox") != std::string::npos;
	bool is_delete_spawn = spawn_command.find("deletespawn") != std::string::npos;
	bool is_edit_box = spawn_command.find("editgroup") != std::string::npos;
	bool is_edit_respawn = spawn_command.find("editrespawn") != std::string::npos;
	bool is_make_group = spawn_command.find("makegroup") != std::string::npos;
	bool is_make_npc = spawn_command.find("makenpc") != std::string::npos;
	bool is_move_spawn = spawn_command.find("movespawn") != std::string::npos;
	if (
		!is_add_entry &&
		!is_add_spawn &&
		!is_clear_box &&
		!is_delete_spawn &&
		!is_edit_box &&
		!is_edit_respawn &&
		!is_make_group &&
		!is_make_npc &&
		!is_move_spawn
		) {
		c->Message(CC_Default, "Usage: #advnpcspawn addentry [Spawngroup ID] [NPC ID] [Spawn Chance] - Adds a new Spawngroup Entry");
		c->Message(CC_Default, "Usage: #advnpcspawn addspawn [Spawngroup ID] - Adds a new Spawngroup Entry from an existing Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn clearbox [Spawngroup ID] - Clears the roambox of a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn deletespawn - Deletes a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn editbox [Spawngroup ID] [Minimum X] [Maximum X] [Minimum Y] [Maximum Y] [Delay]  - Edit the roambox of a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn editrespawn [Respawn Timer] [Variance] - Edit the Respawn Timer of a Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn makegroup [Spawn Group Name] [Spawn Limit] [Minimum X] [Maximum X] [Minimum Y] [Maximum Y] [Delay] - Makes a new Spawngroup");
		c->Message(CC_Default, "Usage: #advnpcspawn makenpc - Makes a new NPC");
		c->Message(CC_Default, "Usage: #advnpcspawn movespawn - Moves a Spawngroup to your current location");
		return;
	}


	if (is_add_entry) {
		if (arguments < 4) {
			c->Message(CC_Default, "Usage: #advnpcspawn addentry [Spawngroup ID] [NPC ID] [Spawn Chance]");
			return;
		}

		auto spawngroup_id = std::stoi(sep->arg[2]);
		auto npc_id = std::stoi(sep->arg[2]);
		auto spawn_chance = std::stoi(sep->arg[2]);

		std::string query = fmt::format(
			SQL(
				INSERT INTO spawnentry(spawngroupID, npcID, chance)
				VALUES({}, {}, {})
			),
			spawngroup_id,
			npc_id,
			spawn_chance
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to add entry to Spawngroup.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"({}) added to Spawngroup {}, its spawn chance is {}%%.",
				npc_id,
				spawngroup_id,
				spawn_chance
			).c_str()
		);
		return;
	}
	else if (is_add_spawn) {
		database.NPCSpawnDB(
			NPCSpawnTypes::AddSpawnFromSpawngroup,
			zone->GetShortName(),
			c,
			0,
			std::stoi(sep->arg[2])
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawn Added | Added spawn from Spawngroup ID {}.",
				std::stoi(sep->arg[2])
			).c_str()
		);
		return;
	}
	else if (is_clear_box) {
		if (arguments != 2 || !sep->IsNumber(2)) {
			c->Message(CC_Default, "Usage: #advnpcspawn clearbox [Spawngroup ID]");
			return;
		}

		std::string query = fmt::format(
			"UPDATE spawngroup SET min_x = 0, max_x = 0, min_y = 0, max_y = 0, delay = 0 WHERE id = {}",
			std::stoi(sep->arg[2])
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to clear Spawngroup box.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Roambox Cleared | Delay: 0 Distance: 0.00",
				std::stoi(sep->arg[2])
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Roambox Cleared | Minimum X: 0.00 Maximum X: 0.00",
				std::stoi(sep->arg[2])
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Roambox Cleared | Minimum Y: 0.00 Maximum Y: 0.00",
				std::stoi(sep->arg[2])
			).c_str()
		);
		return;
	}
	else if (is_delete_spawn) {
		if (!c->GetTarget() || !c->GetTarget()->IsNPC()) {
			c->Message(CC_Default, "You must target an NPC to use this command.");
			return;
		}

		NPC* target = c->GetTarget()->CastToNPC();
		Spawn2* spawn2 = target->respawn2;
		if (!spawn2) {
			c->Message(CC_Default, "Failed to delete spawn because NPC has no Spawn2.");
			return;
		}

		auto spawn2_id = spawn2->GetID();
		std::string query = fmt::format(
			"DELETE FROM spawn2 WHERE id = {}",
			spawn2_id
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to delete spawn.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"Spawn2 {} Deleted | Name: {} ({})",
				spawn2_id,
				target->GetCleanName(),
				target->GetID()
			).c_str()
		);
		target->Depop(false);
		return;
	}
	else if (is_edit_box) {
		if (
			arguments != 7 ||
			!sep->IsNumber(3) ||
			!sep->IsNumber(4) ||
			!sep->IsNumber(5) ||
			!sep->IsNumber(6) ||
			!sep->IsNumber(7)
			) {
			c->Message(CC_Default, "Usage: #advnpcspawn editbox [Spawngroup ID] [Minimum X] [Maximum X] [Minimum Y] [Maximum Y] [Delay]");
			return;
		}
		auto spawngroup_id = std::stoi(sep->arg[2]);
		auto minimum_x = std::stof(sep->arg[3]);
		auto maximum_x = std::stof(sep->arg[4]);
		auto minimum_y = std::stof(sep->arg[5]);
		auto maximum_y = std::stof(sep->arg[6]);
		auto delay = std::stoi(sep->arg[7]);

		std::string query = fmt::format(
			"UPDATE spawngroup SET min_x = {:.2f}, max_x = {:.2f}, max_y = {:.2f}, min_y = {:.2f}, delay = {} WHERE id = {}",
			minimum_x,
			maximum_x,
			minimum_y,
			maximum_y,
			delay,
			spawngroup_id
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to edit Spawngroup box.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Roambox Edited | Delay: {}",
				spawngroup_id,
				delay
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Roambox Edited | Minimum X: {:.2f} Maximum X: {:.2f}",
				spawngroup_id,
				minimum_x,
				maximum_x
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Roambox Edited | Minimum Y: {:.2f} Maximum Y: {:.2f}",
				spawngroup_id,
				minimum_y,
				maximum_y
			).c_str()
		);
		return;
	}
	else if (is_edit_respawn) {
		if (arguments < 2 || !sep->IsNumber(2)) {
			c->Message(CC_Default, "Usage: #advnpcspawn editrespawn [Respawn Timer] [Variance]");
			return;
		}

		if (!c->GetTarget() || !c->GetTarget()->IsNPC()) {
			c->Message(CC_Default, "You must target an NPC to use this command.");
			return;
		}

		NPC* target = c->GetTarget()->CastToNPC();
		Spawn2* spawn2 = target->respawn2;
		if (!spawn2) {
			c->Message(CC_Default, "Failed to edit respawn because NPC has no Spawn2.");
			return;
		}

		auto spawn2_id = spawn2->GetID();
		uint32 respawn_timer = std::stoi(sep->arg[2]);
		uint32 variance = (
			sep->IsNumber(3) ?
			std::stoi(sep->arg[3]) :
			spawn2->GetVariance()
			);
		std::string query = fmt::format(
			"UPDATE spawn2 SET respawntime = {}, variance = {} WHERE id = {}",
			respawn_timer,
			variance,
			spawn2_id
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to edit respawn.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"Spawn2 {} Respawn Modified | Name: {} ({}) Respawn Timer: {} Variance: {}",
				spawn2_id,
				target->GetCleanName(),
				target->GetID(),
				respawn_timer,
				variance
			).c_str()
		);
		spawn2->SetRespawnTimer(respawn_timer);
		spawn2->SetVariance(variance);
		return;
	}
	else if (is_make_group) {
		if (
			arguments != 8 ||
			!sep->IsNumber(3) ||
			!sep->IsNumber(4) ||
			!sep->IsNumber(5) ||
			!sep->IsNumber(6) ||
			!sep->IsNumber(7) ||
			!sep->IsNumber(8)
			) {
			c->Message(CC_Default, "Usage: #advncspawn makegroup [Spawn Group Name] [Spawn Limit] [Minimum X] [Maximum X] [Minimum Y] [Maximum Y] [Delay]");
			return;
		}
		std::string spawngroup_name = sep->arg[2];
		auto spawn_limit = std::stoi(sep->arg[3]);
		auto minimum_x = std::stof(sep->arg[4]);
		auto maximum_x = std::stof(sep->arg[5]);
		auto minimum_y = std::stof(sep->arg[6]);
		auto maximum_y = std::stof(sep->arg[7]);
		auto delay = std::stoi(sep->arg[8]);

		std::string query = fmt::format(
			"INSERT INTO spawngroup"
			"(name, spawn_limit, min_x, max_x, min_y, max_y, delay)"
			"VALUES ('{}', {}, {:.2f}, {:.2f}, {:.2f}, {:.2f}, {})",
			spawngroup_name,
			spawn_limit,
			minimum_x,
			maximum_x,
			minimum_y,
			maximum_y,
			delay
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to make Spawngroup.");
			return;
		}

		auto spawngroup_id = results.LastInsertedID();
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Created | Name: {} Spawn Limit: {}",
				spawngroup_id,
				spawngroup_name,
				spawn_limit
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Created | Delay: {}",
				spawngroup_id,
				delay
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Created | Minimum X: {:.2f} Maximum X: {:.2f}",
				spawngroup_id,
				minimum_x,
				maximum_x
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawngroup {} Created | Minimum Y: {:.2f} Maximum Y: {:.2f}",
				spawngroup_id,
				minimum_y,
				maximum_y
			).c_str()
		);
		return;
	}
	else if (is_make_npc) {
		if (!c->GetTarget() || !c->GetTarget()->IsNPC()) {
			c->Message(CC_Default, "You must target an NPC to use this command.");
			return;
		}

		NPC* target = c->GetTarget()->CastToNPC();
		database.NPCSpawnDB(
			NPCSpawnTypes::CreateNewNPC,
			zone->GetShortName(),
			c,
			target
		);
		return;
	}
	else if (is_move_spawn) {
		if (!c->GetTarget() || !c->GetTarget()->IsNPC()) {
			c->Message(CC_Default, "You must target an NPC to use this command.");
			return;
		}

		NPC* target = c->GetTarget()->CastToNPC();
		Spawn2* spawn2 = target->respawn2;
		if (!spawn2) {
			c->Message(CC_Default, "Failed to move spawn because NPC has no Spawn2.");
			return;
		}

		auto client_position = c->GetPosition();
		auto spawn2_id = spawn2->GetID();
		std::string query = fmt::format(
			"UPDATE spawn2 SET x = {:.2f}, y = {:.2f}, z = {:.2f}, heading = {:.2f} WHERE id = {}",
			client_position.x,
			client_position.y,
			client_position.z,
			client_position.w,
			spawn2_id
		);
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(CC_Default, "Failed to move spawn.");
			return;
		}

		c->Message(
			CC_Default,
			fmt::format(
				"Spawn2 {} Moved | Name: {} ({})",
				spawn2_id,
				target->GetCleanName(),
				target->GetID()
			).c_str()
		);
		c->Message(
			CC_Default,
			fmt::format(
				"Spawn2 {} Moved | XYZ: {}, {}, {} Heading: {}",
				spawn2_id,
				client_position.x,
				client_position.y,
				client_position.z,
				client_position.w
			).c_str()
		);
		target->GMMove(
			client_position.x,
			client_position.y,
			client_position.z,
			client_position.w
		);
		return;
	}
}

void command_aggrozone(Client *c, const Seperator *sep){
	if (!c)
		return;

	Mob *m = c->GetTarget() ? c->GetTarget() : c->CastToMob();

	if (!m)
		return;

	int hate = atoi(sep->arg[1]); //should default to 0 if we don't enter anything
	bool use_ignore_dist = false;
	if(sep->IsNumber(2))
		use_ignore_dist = atoi(sep->arg[2]);
	entity_list.AggroZone(m, hate, use_ignore_dist);
	if (!c->GetTarget())
		c->Message(CC_Default, "Train to you! Last chance to go invulnerable...");
	else
		c->Message(CC_Default, "Train to %s! Watch them die!!!", c->GetTarget()->GetCleanName());
}

void command_modifynpcstat(Client *c, const Seperator *sep){
	if (!c)
		return;

	if (sep->arg[1][0] == '\0')
	{
		c->Message(CC_Default, "usage #modifynpcstat arg value");
		c->Message(CC_Default, "Args: ac, str, sta, agi, dex, wis, _int, cha, max_hp, mr, fr, cr, pr, dr, runspeed, special_attacks, "
			"atk, accuracy, min_hit, max_hit, see_invis_undead, see_sneak, see_improved_hide, "
			"hp_regen, mana_regen, aggro, assist, slow_mitigation, loottable_id, healscale, spellscale");
		return;
	}

	if (!c->GetTarget())
		return;

	if (!c->GetTarget()->IsNPC())
		return;

	c->GetTarget()->CastToNPC()->ModifyNPCStat(sep->arg[1], sep->arg[2]);
}

void command_netstats(Client *c, const Seperator *sep){
	if (c)
	{
		if (c->GetTarget() && c->GetTarget()->IsClient())
		{
			c->Message(CC_Default, "Sent:");
			c->Message(CC_Default, "Total: %u, per second: %u", c->GetTarget()->CastToClient()->Connection()->GetBytesSent(),
				c->GetTarget()->CastToClient()->Connection()->GetBytesSentPerSecond());
			c->Message(CC_Default, "Recieved:");
			c->Message(CC_Default, "Total: %u, per second: %u", c->GetTarget()->CastToClient()->Connection()->GetBytesRecieved(),
				c->GetTarget()->CastToClient()->Connection()->GetBytesRecvPerSecond());

		}
		else
		{
			c->Message(CC_Default, "Sent:");
			c->Message(CC_Default, "Total: %u, per second: %u", c->Connection()->GetBytesSent(), c->Connection()->GetBytesSentPerSecond());
			c->Message(CC_Default, "Recieved:");
			c->Message(CC_Default, "Total: %u, per second: %u", c->Connection()->GetBytesRecieved(), c->Connection()->GetBytesRecvPerSecond());
		}
	}
}

void command_showspellslist(Client *c, const Seperator *sep){
	Mob *target = c->GetTarget();

	if (!target) {
		c->Message(CC_Default, "Must target an NPC.");
		return;
	}

	if (!target->IsNPC()) {
		c->Message(CC_Default, "%s is not an NPC.", target->GetName());
		return;
	}

	target->CastToNPC()->AISpellsList(c);

	return;
}

void command_raidloot(Client *c, const Seperator *sep){
	if (!sep->arg[1][0]) {
		c->Message(CC_Default, "Usage: #raidloot [LEADER/GROUPLEADER/SELECTED/ALL]");
		return;
	}

	Raid *r = c->GetRaid();
	if (r)
	{
		for (int x = 0; x < 72; ++x)
		{
			if (r->members[x].member == c)
			{
				if (r->members[x].IsRaidLeader == 0)
				{
					c->Message(CC_Default, "You must be the raid leader to use this command.");
				}
				else
				{
					break;
				}
			}
		}

		if (strcasecmp(sep->arg[1], "LEADER") == 0)
		{
			c->Message(CC_Yellow, "Loot type changed to: 1");
			r->ChangeLootType(1);
		}
		else if (strcasecmp(sep->arg[1], "GROUPLEADER") == 0)
		{
			c->Message(CC_Yellow, "Loot type changed to: 2");
			r->ChangeLootType(2);
		}
		else if (strcasecmp(sep->arg[1], "SELECTED") == 0)
		{
			c->Message(CC_Yellow, "Loot type changed to: 3");
			r->ChangeLootType(3);
		}
		else if (strcasecmp(sep->arg[1], "ALL") == 0)
		{
			c->Message(CC_Yellow, "Loot type changed to: 4");
			r->ChangeLootType(4);
		}
		else
		{
			c->Message(CC_Default, "Usage: #raidloot [LEADER/GROUPLEADER/SELECTED/ALL]");
		}
	}
	else
	{
		c->Message(CC_Default, "You must be in a raid to use that command.");
	}
}

void command_emoteview(Client *c, const Seperator *sep){
	if (!c->GetTarget() || !c->GetTarget()->IsNPC())
	{
		c->Message(CC_Default, "You must target a NPC to view their emotes.");
		return;
	}

	if (c->GetTarget() && c->GetTarget()->IsNPC())
	{
		int count = 0;
		int emoteid = c->GetTarget()->CastToNPC()->GetEmoteID();

		for (auto &i : zone->NPCEmoteList)
		{
			NPC_Emote_Struct* nes = i;
			if (emoteid == nes->emoteid)
			{
				c->Message(CC_Default, "EmoteID: %i Event: %i Type: %i Text: %s", nes->emoteid, nes->event_, nes->type, nes->text);
				count++;
			}
		}
		if (count == 0)
			c->Message(CC_Default, "No emotes found.");
		else
			c->Message(CC_Default, "%i emote(s) found", count);
	}
}

void command_emotesearch(Client *c, const Seperator *sep){
	if (sep->arg[1][0] == 0)
		c->Message(CC_Default, "Usage: #emotesearch [search string or emoteid]");
	else
	{
		const char *search_criteria = sep->argplus[1];
		int count = 0;

		if (Seperator::IsNumber(search_criteria))
		{
			uint16 emoteid = atoi(search_criteria);
			for (auto &i : zone->NPCEmoteList)
			{
				NPC_Emote_Struct* nes = i;
				if (emoteid == nes->emoteid)
				{
					c->Message(CC_Default, "EmoteID: %i Event: %i Type: %i Text: %s", nes->emoteid, nes->event_, nes->type, nes->text);
					count++;
				}
			}
			if (count == 0)
				c->Message(CC_Default, "No emotes found.");
			else
				c->Message(CC_Default, "%i emote(s) found", count);
		}
		else
		{
			std::string sText;
			std::string sCriteria;
			sCriteria = search_criteria;
			for (auto & c : sCriteria) c = toupper(c);

			for (auto &i : zone->NPCEmoteList)
			{
				NPC_Emote_Struct* nes = i;
				sText = nes->text;
				for (auto & c : sText) c = toupper(c);
				if (sText.find(sCriteria) != std::string::npos)
				{
					c->Message(CC_Default, "EmoteID: %i Event: %i Type: %i Text: %s", nes->emoteid, nes->event_, nes->type, nes->text);
					count++;
				}
				if (count == 50)
					break;
			}
			if (count == 50)
				c->Message(CC_Default, "50 emotes shown...too many results.");
			else
				c->Message(CC_Default, "%i emote(s) found", count);
		}
	}
}

void command_reloademote(Client *c, const Seperator *sep){
	zone->LoadNPCEmotes(&zone->NPCEmoteList);
	c->Message(CC_Default, "NPC emotes reloaded.");
}

void command_globalview(Client *c, const Seperator *sep){
	NPC * npcmob = nullptr;

	if (c->GetTarget() && c->GetTarget()->IsNPC())
	{
		npcmob = c->GetTarget()->CastToNPC();
		QGlobalCache *npc_c = nullptr;
		QGlobalCache *char_c = nullptr;
		QGlobalCache *zone_c = nullptr;

		if (npcmob)
			npc_c = npcmob->GetQGlobals();

		char_c = c->GetQGlobals();
		zone_c = zone->GetQGlobals();

		std::list<QGlobal> globalMap;
		uint32 ntype = 0;

		if (npcmob)
			ntype = npcmob->GetNPCTypeID();

		if (npc_c)
		{
			QGlobalCache::Combine(globalMap, npc_c->GetBucket(), ntype, c->CharacterID(), zone->GetZoneID());
		}

		if (char_c)
		{
			QGlobalCache::Combine(globalMap, char_c->GetBucket(), ntype, c->CharacterID(), zone->GetZoneID());
		}

		if (zone_c)
		{
			QGlobalCache::Combine(globalMap, zone_c->GetBucket(), ntype, c->CharacterID(), zone->GetZoneID());
		}

		auto iter = globalMap.begin();
		uint32 gcount = 0;

		c->Message(CC_Default, "Name, Value");
		while (iter != globalMap.end())
		{
			c->Message(CC_Default, "%s %s", (*iter).name.c_str(), (*iter).value.c_str());
			++iter;
			++gcount;
		}
		c->Message(CC_Default, "%u globals loaded.", gcount);
	}
	else
	{
		QGlobalCache *char_c = nullptr;
		QGlobalCache *zone_c = nullptr;

		char_c = c->GetQGlobals();
		zone_c = zone->GetQGlobals();

		std::list<QGlobal> globalMap;
		uint32 ntype = 0;

		if (char_c)
		{
			QGlobalCache::Combine(globalMap, char_c->GetBucket(), ntype, c->CharacterID(), zone->GetZoneID());
		}

		if (zone_c)
		{
			QGlobalCache::Combine(globalMap, zone_c->GetBucket(), ntype, c->CharacterID(), zone->GetZoneID());
		}

		auto iter = globalMap.begin();
		uint32 gcount = 0;

		c->Message(CC_Default, "Name, Value");
		while (iter != globalMap.end())
		{
			c->Message(CC_Default, "%s %s", (*iter).name.c_str(), (*iter).value.c_str());
			++iter;
			++gcount;
		}
		c->Message(CC_Default, "%u globals loaded.", gcount);
	}
}

void command_distance(Client *c, const Seperator *sep){
	if (c && c->GetTarget()) {
		Mob* target = c->GetTarget();

		c->Message(CC_Default, "Your target, %s, is %1.1f units from you.", c->GetTarget()->GetName(), Distance(c->GetPosition(), target->GetPosition()));
	}
}

void command_cvs(Client *c, const Seperator *sep)
{
	auto pack = new ServerPacket(ServerOP_ClientVersionSummary, sizeof(ServerRequestClientVersionSummary_Struct));
	ServerRequestClientVersionSummary_Struct *srcvss = (ServerRequestClientVersionSummary_Struct*)pack->pBuffer;
	strn0cpy(srcvss->Name, c->GetName(), sizeof(srcvss->Name));
	worldserver.SendPacket(pack);

	safe_delete(pack);
}

void command_maxallskills(Client *c, const Seperator *sep){
	if (c)
	{
		for (int i = 0; i <= EQ::skills::HIGHEST_SKILL; ++i)
		{
			if (i >= EQ::skills::SkillSpecializeAbjure && i <= EQ::skills::SkillSpecializeEvocation)
			{
				c->SetSkill((EQ::skills::SkillType)i, 50);
			}
			else
			{
				int max_skill_level = database.GetSkillCap(c->GetClass(), (EQ::skills::SkillType)i, c->GetLevel());
				c->SetSkill((EQ::skills::SkillType)i, max_skill_level);
			}
		}
	}
}

void command_showbonusstats(Client *c, const Seperator *sep){
	if (c->GetTarget() == 0)
		c->Message(CC_Default, "ERROR: No target!");
	else if (!c->GetTarget()->IsMob() && !c->GetTarget()->IsClient())
		c->Message(CC_Default, "ERROR: Target is not a Mob or Player!");
	else {
		bool bAll = false;
		if (sep->arg[1][0] == '\0' || strcasecmp(sep->arg[1], "all") == 0)
			bAll = true;
		if (bAll || (strcasecmp(sep->arg[1], "item") == 0)) {
			c->Message(CC_Default, "Target Item Bonuses:");
			c->Message(CC_Default, "  Accuracy: %i%%   Divine Save: %i%%", c->GetTarget()->GetItemBonuses().Accuracy, c->GetTarget()->GetItemBonuses().DivineSaveChance);
			c->Message(CC_Default, "  Flurry: %i%%     HitChance: %i%%", c->GetTarget()->GetItemBonuses().FlurryChance, c->GetTarget()->GetItemBonuses().HitChance / 15);
		}
		if (bAll || (strcasecmp(sep->arg[1], "spell") == 0)) {
			c->Message(CC_Default, "  Target Spell Bonuses:");
			c->Message(CC_Default, "  Accuracy: %i%%   Divine Save: %i%%", c->GetTarget()->GetSpellBonuses().Accuracy, c->GetTarget()->GetSpellBonuses().DivineSaveChance);
			c->Message(CC_Default, "  Flurry: %i%%     HitChance: %i%% ", c->GetTarget()->GetSpellBonuses().FlurryChance, c->GetTarget()->GetSpellBonuses().HitChance / 15);
		}
		c->Message(CC_Default, "  Effective Casting Level: %i", c->GetTarget()->GetCasterLevel(0));
	}
}

void command_reloadallrules(Client *c, const Seperator *sep){
	if (c)
	{
		auto pack = new ServerPacket(ServerOP_ReloadRules, 0);
		worldserver.SendPacket(pack);
		c->Message(CC_Red, "Successfully sent the packet to world to reload rules globally. (including world)");
		safe_delete(pack);

	}
}

void command_reloadworldrules(Client *c, const Seperator *sep){
	if (c)
	{
		auto pack = new ServerPacket(ServerOP_ReloadRulesWorld, 0);
		worldserver.SendPacket(pack);
		c->Message(CC_Red, "Successfully sent the packet to world to reload rules. (only world)");
		safe_delete(pack);
	}
}

void command_qtest(Client *c, const Seperator *sep){


	if (c && sep->arg[1][0])
	{
		if (c->GetTarget())
		{
			auto pack = new ServerPacket(ServerOP_Speech, sizeof(Server_Speech_Struct) + strlen(sep->arg[1]) + 1);
			Server_Speech_Struct* sem = (Server_Speech_Struct*)pack->pBuffer;
			strcpy(sem->message, sep->arg[1]);
			sem->minstatus = c->Admin();
			sem->type = 1;
			strncpy(sem->to, c->GetTarget()->GetCleanName(), 64);
			strncpy(sem->from, c->GetCleanName(), 64);
			sem->guilddbid = c->GuildID();
			sem->characterid = c->CharacterID();
			sem->groupid = 0;
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
	}
}

void command_mysql(Client *c, const Seperator *sep)
{
	if (!sep->arg[1][0] || !sep->arg[2][0]) {
		c->Message(CC_Default, "Usage: #mysql query \"Query here\"");
		return;
	}

	if (strcasecmp(sep->arg[1], "help") == 0) {
		c->Message(CC_Default, "MYSQL In-Game CLI Interface:");
		c->Message(CC_Default, "Example: #mysql query \"Query goes here quoted\" -s -h");
		c->Message(CC_Default, "To use 'like \"%%something%%\" replace the %% with #");
		c->Message(CC_Default, "Example: #mysql query \"select * from table where name like \"#something#\"");
		c->Message(CC_Default, "-s - Spaces select entries apart");
		c->Message(CC_Default, "-h - Colors every other select result");
		return;
	}

	if (strcasecmp(sep->arg[1], "query") == 0) {
		///Parse switches here
		int argnum = 3;
		bool optionS = false;
		bool optionH = false;
		while (sep->arg[argnum] && strlen(sep->arg[argnum]) > 1){
			switch (sep->arg[argnum][1]){
			case 's': optionS = true; break;
			case 'h': optionH = true; break;
			default:
				c->Message(CC_Yellow, "%s, there is no option '%c'", c->GetName(), sep->arg[argnum][1]);
				return;
			}
			++argnum;
		}

        int highlightTextIndex = 0;
        std::string query(sep->arg[2]);
        //swap # for % so like queries can work
        std::replace(query.begin(), query.end(), '#', '%');
        auto results = database.QueryDatabase(query);
        if (!results.Success()) {
            return;
        }

		//Using sep->arg[2] again, replace # with %% so it doesn't screw up when sent through vsnprintf in Message
		query = sep->arg[2];
		int pos = query.find('#');
		while (pos != std::string::npos) {
			query.erase(pos, 1);
			query.insert(pos, "%%");
			pos = query.find('#');
		}
		c->Message(CC_Yellow, "---Running query: '%s'", query.c_str());

		for (auto row = results.begin(); row != results.end(); ++row) {
			std::stringstream lineText;
			std::vector<std::string> lineVec;
			for (int i = 0; i < results.RowCount(); i++) {
				//split lines that could overflow the buffer in Client::Message and get cut off
				//This will crash MQ2 @ 4000 since their internal buffer is only 2048.
				//Reducing it to 2000 fixes that but splits more results from tables with a lot of columns.
				if (lineText.str().length() > 2600) {
					lineVec.push_back(lineText.str());
					lineText.str("");
				}
				lineText << results.FieldName(i) << ":" << "[" << (row[i] ? row[i] : "nullptr") << "] ";
			}

			lineVec.push_back(lineText.str());

			if (optionS) //This provides spacing for the space switch
				c->Message(CC_Default, " ");
			if (optionH) //This option will highlight every other row
				highlightTextIndex = 1 - highlightTextIndex;

			for (int lineNum = 0; lineNum < lineVec.size(); ++lineNum)
				c->Message(highlightTextIndex, lineVec[lineNum].c_str());
		}
	}
}

void command_zopp(Client *c, const Seperator *sep){
	// - Owner only command..non-targetable to eliminate malicious or mischievious activities.
	if (!c)
		return;
	else if (sep->argnum < 3 || sep->argnum > 4)
		c->Message(CC_Default, "Usage: #zopp [trade/summon] [slot id] [item id] [*charges]");
	else if (strcasecmp(sep->arg[1], "trade") && strcasecmp(sep->arg[1], "t") && strcasecmp(sep->arg[1], "summon") && strcasecmp(sep->arg[1], "s"))
		c->Message(CC_Default, "Usage: #zopp [trade/summon] [slot id] [item id] [*charges]");
	else if (!sep->IsNumber(2) || !sep->IsNumber(3) || (sep->argnum == 4 && !sep->IsNumber(4)))
		c->Message(CC_Default, "Usage: #zopp [trade/summon] [slot id] [item id] [*charges]");
	else {
		ItemPacketType packettype;

		if (strcasecmp(sep->arg[1], "trade") == 0 || strcasecmp(sep->arg[1], "t") == 0) {
			packettype = ItemPacketTrade;
		}
		else {
			packettype = ItemPacketSummonItem;
		}

		int16 slotid = atoi(sep->arg[2]);
		uint32 itemid = atoi(sep->arg[3]);
		int16 charges = sep->argnum == 4 ? atoi(sep->arg[4]) : 1; // defaults to 1 charge if not specified

		const EQ::ItemData* FakeItem = database.GetItem(itemid);

		if (!FakeItem) {
			c->Message(CC_Red, "Error: Item [%u] is not a valid item id.", itemid);
			return;
		}

		int16 item_status = 0;
		const EQ::ItemData* item = database.GetItem(itemid);
		if (item) {
			item_status = static_cast<int16>(item->MinStatus);
		}
		if (item_status > c->Admin()) {
			c->Message(CC_Red, "Error: Insufficient status to use this command.");
			return;
		}

		if (charges < 0 || charges > FakeItem->StackSize) {
			c->Message(CC_Red, "Warning: The specified charge count does not meet expected criteria!");
			c->Message(CC_Default, "Processing request..results may cause unpredictable behavior.");
		}

		EQ::ItemInstance* FakeItemInst = database.CreateItem(FakeItem, charges);
		c->SendItemPacket(slotid, FakeItemInst, packettype);
		c->Message(CC_Default, "Sending zephyr op packet to client - [%s] %s (%u) with %i %s to slot %i.", packettype == ItemPacketTrade ? "Trade" : "Summon", FakeItem->Name, itemid, charges, abs(charges == 1) ? "charge" : "charges", slotid);
		safe_delete(FakeItemInst);
	}
}

void command_questerrors(Client *c, const Seperator *sep)
{
	std::list<std::string> quest_errors;
	parse->GetErrors(quest_errors);

	if (quest_errors.size()) {
		c->Message(CC_Default, "Quest errors currently are as follows:");

		int error_index = 0;
		for (auto quest_error : quest_errors) {
			if (error_index >= 30) {
				c->Message(CC_Default, "Maximum of 30 errors shown.");
				break;
			}

			c->Message(CC_Default, quest_error.c_str());
			error_index++;
		}
	}
	else {
		c->Message(CC_Default, "There are no Quest errors currently.");
	}
}

void command_update(Client *c, const Seperator *sep)
{
	int admin = c->Admin();
	int system_var = 0;
	std::string help0 = "Update commands usage:";
	std::string help1 = "  #update quests - Updates all zone quests on the server from svn - Does not reload quests.";
	std::string help2 = "  #update source - Fires off a 10 min shutdown warning, takes down server,";
	std::string help3 = "					downloads current git and compiles then restarts the server.";
	std::string help4 = "  #update reboot - Fires off a 10 min shutdown warning and restarts the server without updates.";
	std::string help5 = "  #update rebootNOW - Restarts the server without updates immediately.";

	std::string help[] = { help0, help1, help2, help3, help4, help5 };

	if (strcasecmp(sep->arg[1], "help") == 0)
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
	else if (strcasecmp(sep->arg[1], "quests") == 0)
	{
		FILE *fp;
#ifdef _WINDOWS
		char buf[1024];
		fp = _popen("svn update quests", "r");

		while (fgets(buf, 1024, fp))
		{
			const char * output = (const char *)buf;
			c->Message(CC_Default, "%s", output);
		}
		fclose(fp);
#else
		char* buf = NULL;
		size_t len = 0;
		fflush(NULL);
		fp = popen("svn update quests", "r");

		while (getline(&buf, &len, fp) != -1)
		{
			const char * output = (const char *)buf;
			c->Message(CC_Default, "%s", output);
		}
		free(buf);
		fflush(fp);
#endif
		c->Message(CC_Default, "Quests are updated.");
	}
	else if (strcasecmp(sep->arg[1], "source") == 0)
	{
		if (admin >= 205)
		{
#ifdef _WINDOWS
			// TODO: Add same functionality for windows from the following command.
			c->Message(CC_Default, "Not yet implemented for windows.");
#else
			c->Message(CC_Default, "Server will be going down and building, 10 min warning issued.");
			system_var = system("./EQServer.sh update");
#endif
		}
		else
			c->Message(CC_Default, "Your access level is not high enough to use this command.");
	}
	else if (strcasecmp(sep->arg[1], "reboot") == 0)
	{
		if (admin >= 205)
		{
#ifdef _WINDOWS
			// TODO: Add same functionality for windows from the following command.
			c->Message(CC_Default, "Not yet implemented for windows.");
#else
			c->Message(CC_Default, "Server will be going down for reboot, 10 min warning issued.");
			system_var = system("./EQServer.sh reboot");
#endif
		}
		else
			c->Message(CC_Default, "Your access level is not high enough to use this command.");
	}
	else if (strcasecmp(sep->arg[1], "rebootNOW") == 0)
	{
		if (admin >= 250)
		{
#ifdef _WINDOWS
			// TODO: Add same functionality for windows from the following command.
			c->Message(CC_Default, "Not yet implemented for windows.");
#else
			c->Message(CC_Default, "Server will be going down for reboot.");
			system_var = system("./EQServer.sh rebootNOW");
#endif
		}
		else
			c->Message(CC_Default, "Your access level is not high enough to use this command.");
	}
	else
	{
		int size = sizeof(help) / sizeof(std::string);
		for (int i = 0; i < size; i++)
		{
			c->Message(CC_Default, help[i].c_str());
		}
	}
}

void command_coredump(Client *c, const Seperator *sep)
{
#ifdef _WINDOWS
	// TODO: Add same functionality for windows from the following command.
	// Maybe have a batch file spit logs to a web folder?
	c->Message(CC_Default, "Not yet implemented for windows.");
#else
	int system_var = system("./dump");
	c->Message(CC_Default, "core dumped.");
#endif
}

void command_enablerecipe(Client *c, const Seperator *sep){
	uint32 recipe_id = 0;
	bool success = false;
	if (c) {
		if (sep->argnum == 1) {
			recipe_id = atoi(sep->arg[1]);
		}
		else {
			c->Message(CC_Default, "Invalid number of arguments.\nUsage: #enablerecipe recipe_id");
			return;
		}
		if (recipe_id > 0) {
			success = database.EnableRecipe(recipe_id);
			if (success) {
				c->Message(CC_Default, "Recipe enabled.");
			}
			else {
				c->Message(CC_Default, "Recipe not enabled.");
			}
		}
		else {
			c->Message(CC_Default, "Invalid recipe id.\nUsage: #enablerecipe recipe_id");
		}
	}
}

void command_disablerecipe(Client *c, const Seperator *sep){
	uint32 recipe_id = 0;
	bool success = false;
	if (c) {
		if (sep->argnum == 1) {
			recipe_id = atoi(sep->arg[1]);
		}
		else {
			c->Message(CC_Default, "Invalid number of arguments.\nUsage: #disablerecipe recipe_id");
			return;
		}
		if (recipe_id > 0) {
			success = database.DisableRecipe(recipe_id);
			if (success) {
				c->Message(CC_Default, "Recipe disabled.");
			}
			else {
				c->Message(CC_Default, "Recipe not disabled.");
			}
		}
		else {
			c->Message(CC_Default, "Invalid recipe id.\nUsage: #disablerecipe recipe_id");
		}
	}
}

void command_npctypecache(Client *c, const Seperator *sep){
	if (sep->argnum > 0) {
		for (int i = 0; i < sep->argnum; ++i) {
			if (strcasecmp(sep->arg[i + 1], "all") == 0) {
				c->Message(CC_Default, "Clearing all npc types from the cache.");
				zone->ClearNPCTypeCache(-1);
			}
			else {
				int id = atoi(sep->arg[i + 1]);
				if (id > 0) {
					c->Message(CC_Default, "Clearing npc type %d from the cache.", id);
					zone->ClearNPCTypeCache(id);
					return;
				}
			}
		}
	}
	else {
		c->Message(CC_Default, "Usage:");
		c->Message(CC_Default, "#npctypecache [npctype_id] ...");
		c->Message(CC_Default, "#npctypecache all");
	}
}

void command_starve(Client *c, const Seperator *sep){

	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	t->SetConsumption(0,0);
	t->SendStaminaUpdate();
	c->Message(CC_Default, "Target starved.");

}

void command_merchantopenshop(Client *c, const Seperator *sep){
	Mob *merchant = c->GetTarget();
	if (!merchant || merchant->GetClass() != MERCHANT) {
		c->Message(CC_Default, "You must target a merchant to open their shop.");
		return;
	}

	merchant->CastToNPC()->MerchantOpenShop();
}

void command_merchantcloseshop(Client *c, const Seperator *sep){
	Mob *merchant = c->GetTarget();
	if (!merchant || merchant->GetClass() != MERCHANT) {
		c->Message(CC_Default, "You must target a merchant to close their shop.");
		return;
	}

	merchant->CastToNPC()->MerchantCloseShop();
}

void command_falltest(Client *c, const Seperator *sep)
{
	float zmod;
	if(c)
	{
		if (!sep->IsNumber(1)) {
			c->Message(CC_Default, "Invalid number of arguments.\nUsage: #falltest [+z]");
			return;
		}
		else
		{
			zmod = c->GetZ() + atof(sep->arg[1]);
			c->MovePC(zone->GetZoneID(), c->GetX(), c->GetY(), zmod, c->GetHeading());
			c->Message(CC_Default, "Moving to X: %0.2f Y: %0.2f Z: %0.2f", c->GetX(), c->GetY(), zmod);
		}
	}
}

void command_push(Client *c, const Seperator *sep) {

	Mob* t;
	if (!c->GetTarget())
	{
		c->Message(CC_Default, "You need a target to push.");
		return;
	}
	else if (!c->GetTarget()->IsNPC() && !c->GetTarget()->IsClient())
	{
		c->Message(CC_Default, "That's an invalid target, nerd.");
		return;
	}
	else if (!sep->IsNumber(1))
	{
		c->Message(CC_Default, "Invalid number of arguments.\nUsage: #push [pushback] [pushup]");
		return;
	}
	else
	{
		bool success = false;
		float pushback = atof(sep->arg[1]);
		t = c->GetTarget();
		if (sep->IsNumber(2))
		{
			float pushup = atof(sep->arg[2]);
			if (t->DoKnockback(c, pushback, pushup))
			{
				success = true;
			}
		}
		else
		{
			if (t->IsNPC())
			{
				t->CastToNPC()->AddPush(c->GetHeading() * 2.0f, pushback);
				pushback = t->CastToNPC()->ApplyPushVector();
				if (pushback > 0.0f)
					success = true;
			}
			else
			{
				if (t->CombatPush(c, pushback))
				{
					success = true;
				}
			}
		}

		if (success)
		{
			c->Message(CC_Default, "%s was pushed for %0.1f!", t->GetCleanName(), pushback);
			return;
		}
		else
		{
			c->Message(CC_Default, "Pushed failed on %s. Coord check likely failed.", t->GetCleanName());
			return;
		}
	}
}

void command_xpinfo(Client *c, const Seperator *sep){

	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	uint16 level = t->GetLevel();
	uint32 totalrequiredxp = t->GetEXPForLevel(level + 1);
	uint32 currentxp = t->GetEXP();
	float xpforlevel = totalrequiredxp - currentxp;
	float totalxpforlevel = totalrequiredxp - t->GetEXPForLevel(level);
	float xp_percent = 100.0 - ((xpforlevel/totalxpforlevel) * 100.0);

	int exploss;
	t->GetExpLoss(nullptr, 0, exploss);
	float loss_percent = (exploss/totalxpforlevel) * 100.0;

	float maxaa = t->GetEXPForLevel(0, true);
	uint32 currentaaxp = t->GetAAXP();
	float aa_percent = (currentaaxp/maxaa) * 100.0;

	c->Message(CC_Yellow, "%s has %d of %d required XP.", t->GetName(), currentxp, totalrequiredxp);
	c->Message(CC_Yellow, "They need %0.1f more to get to %d. They are %0.2f percent towards this level.", xpforlevel, level+1, xp_percent);
	c->Message(CC_Yellow, "Their XP loss at this level is %d which is %0.2f percent of their current level.", exploss, loss_percent);
	c->Message(CC_Yellow, "They have %d of %0.1f towards an AA point. They are %0.2f percent towards this point.", currentaaxp, maxaa, aa_percent);
}

void command_chattest(Client *c, const Seperator *sep)
{
	if(!sep->IsNumber(1))
		c->Message(CC_Red, "Please specify a valid number to send as the message color. (This message is red, btw.)");
	else
	{
		int default_ = 10;
		if(sep->IsNumber(2))
			default_ = atoi(sep->arg[2]);

		uint16 base = atoi(sep->arg[1]);
		for(uint8 i = 0; i < default_; ++i)
		{
			uint16 color = base + i;
			c->Message(color, "All work and no play makes Jack a dull boy. (%i)", color);
		}
	}
}

void command_logtest(Client *c, const Seperator *sep){
	clock_t t = std::clock(); /* Function timer start */
	if (sep->IsNumber(1)){
		uint32 i = 0;
		t = std::clock();
		for (i = 0; i < atoi(sep->arg[1]); i++){
			Log(Logs::General, Logs::Debug, "[%u] Test #2... Took %f seconds", i, ((float)(std::clock() - t)) / CLOCKS_PER_SEC);
		}
	}
}

void command_crashtest(Client *c, const Seperator *sep)
{
	c->Message(CC_Default, "Alright, now we get an GPF ;) ");
	char* gpf = 0;
	memcpy(gpf, "Ready to crash", 30);
}

void command_logs(Client *c, const Seperator *sep){
	int logs_set = 0;
	if (sep->argnum > 0) {
		/* #logs reload_all */
		if (strcasecmp(sep->arg[1], "reload_all") == 0){
			auto pack = new ServerPacket(ServerOP_ReloadLogs, 0);
			worldserver.SendPacket(pack);
			c->Message(CC_Red, "Successfully sent the packet to world to reload log settings from the database for all zones");
			safe_delete(pack);
		}
		/* #logs list_settings */
		if (strcasecmp(sep->arg[1], "list_settings") == 0 || (strcasecmp(sep->arg[1], "set") == 0 && strcasecmp(sep->arg[3], "") == 0)){
			c->Message(CC_Default, "[Category ID | console | file | gmsay | Category Description]");
			int redisplay_columns = 0;
			for (int i = 0; i < Logs::LogCategory::MaxCategoryID; i++){
				if (redisplay_columns == 10){
					c->Message(CC_Default, "[Category ID | console | file | gmsay | Category Description]");
					redisplay_columns = 0;
				}
				c->Message(CC_Default, StringFormat("--- %i | %u | %u | %u | %s", i, LogSys.log_settings[i].log_to_console, LogSys.log_settings[i].log_to_file, LogSys.log_settings[i].log_to_gmsay, Logs::LogCategoryName[i]).c_str());
				redisplay_columns++;
			}
		}
		/* #logs set */
		if (strcasecmp(sep->arg[1], "set") == 0){
			if (strcasecmp(sep->arg[2], "console") == 0){
				LogSys.log_settings[atoi(sep->arg[3])].log_to_console = atoi(sep->arg[4]);
				logs_set = 1;
			}
			else if (strcasecmp(sep->arg[2], "file") == 0){
				LogSys.log_settings[atoi(sep->arg[3])].log_to_file = atoi(sep->arg[4]);
				logs_set = 1;
			}
			else if (strcasecmp(sep->arg[2], "gmsay") == 0){
				LogSys.log_settings[atoi(sep->arg[3])].log_to_gmsay = atoi(sep->arg[4]);
				logs_set = 1;
			}
			else{
				c->Message(CC_Default, "--- #logs set [console|file|gmsay] <category_id> <debug_level (1-3)> - Sets log settings during the lifetime of the zone");
				c->Message(CC_Default, "--- #logs set gmsay 20 1 - Would output Quest errors to gmsay");
			}
			if (logs_set == 1){
				c->Message(CC_Yellow, "Your Log Settings have been applied");
				c->Message(CC_Yellow, "Output Method: %s :: Debug Level: %i - Category: %s", sep->arg[2], atoi(sep->arg[4]), Logs::LogCategoryName[atoi(sep->arg[3])]);
			}
			/* We use a general 'is_category_enabled' now, let's update when we update any output settings 
				This is used in hot places of code to check if its enabled in any way before triggering logs
			*/
			if (atoi(sep->arg[4]) > 0){
				LogSys.log_settings[atoi(sep->arg[3])].is_category_enabled = 1;
			}
			else{
				LogSys.log_settings[atoi(sep->arg[3])].is_category_enabled = 0;
			}
		}
		if (strcasecmp(sep->arg[1], "quiet") == 0)
		{
			for (int i = 0; i < Logs::LogCategory::MaxCategoryID; i++)
			{
				LogSys.log_settings[i].log_to_gmsay = 0;
				logs_set = 0;
			}
			c->Message(CC_Yellow, "Shhh. Be vewy vewy quiet, I'm hunting wabbits.");
		}
	}
	else 
	{
		c->Message(CC_Default, "#logs usage:");
		c->Message(CC_Default, "--- #logs quiet - Turns off all gmsay logs in the current zone until the next time the zone resets.");
		c->Message(CC_Default, "--- #logs reload_all - Reload all settings in world and all zone processes with what is defined in the database");
		c->Message(CC_Default, "--- #logs list_settings - Shows current log settings and categories loaded into the current process' memory");
		c->Message(CC_Default, "--- #logs set [console|file|gmsay] <category_id> <debug_level (1-3)> - Sets log settings during the lifetime of the zone");
	}
}

void command_mysqltest(Client *c, const Seperator *sep)
{
	clock_t t = std::clock(); /* Function timer start */
	if (sep->IsNumber(1)){
		uint32 i = 0;
		t = std::clock();
		for (i = 0; i < atoi(sep->arg[1]); i++){
			std::string query = "SELECT * FROM `zone`";
			auto results = database.QueryDatabase(query);
		} 
	}
	Log(Logs::General, Logs::Debug, "MySQL Test... Took %f seconds", ((float)(std::clock() - t)) / CLOCKS_PER_SEC); 
}

void command_boatinfo(Client *c, const Seperator *sep)
{
	if(!zone->IsBoatZone())
	{
		c->Message(CC_Default, "Zone is not a boat zone!");
	}
	else
	{
		entity_list.GetBoatInfo(c);
	}
}

void command_undeletechar(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] != 0)
	{
		if(!database.UnDeleteCharacter(sep->arg[1]))
		{
			c->Message(CC_Red, "%s could not be undeleted. Check the spelling of their name.", sep->arg[1]);
		}
		else
		{
			c->Message(CC_Green, "%s successfully undeleted!", sep->arg[1]);
		}
	}
	else
	{
		c->Message(CC_Default, "Usage: undeletechar [charname]");
	}
}

void command_hotfix(Client *c, const Seperator *sep)
{
	auto items_count = database.GetItemsCount();
	auto shared_items_count = database.GetSharedItemsCount();
	if (items_count != shared_items_count) {
		c->Message(CC_Yellow, "Your database does not have the same item count as your shared memory.");

		c->Message(
			CC_Yellow,
			fmt::format(
				"Database Count: {} Shared Memory Count: {}",
				items_count,
				shared_items_count
			).c_str()
		);

		c->Message(CC_Yellow, "If you want to be able to add new items to your server while it is online, you need to create placeholder entries in the database ahead of time and do not add or remove rows/entries. Only modify the existing placeholder rows/entries to safely use #hotfix.");

		return;
	}

	auto spells_count = database.GetSpellsCount();
	auto shared_spells_count = database.GetSharedSpellsCount();
	if (spells_count != shared_spells_count) {
		c->Message(CC_Yellow, "Your database does not have the same spell count as your shared memory.");

		c->Message(
			CC_Yellow,
			fmt::format(
				"Database Count: {} Shared Memory Count: {}",
				spells_count,
				shared_spells_count
			).c_str()
		);

		c->Message(CC_Yellow, "If you want to be able to add new spells to your server while it is online, you need to create placeholder entries in the database ahead of time and do not add or remove rows/entries. Only modify the existing placeholder rows/entries to safely use #hotfix.");

		c->Message(CC_Yellow, "Note: You may still have to distribute a spell file, even with dynamic changes.");

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

	c->Message(CC_Default, "Creating and applying hotfix");
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
			c->Message(CC_Default, "Hotfix failed.");
			return;
		}
		database.SetVariable("hotfix_name", hotfix_name);

		ServerPacket pack(ServerOP_ChangeSharedMem, hotfix_name.length() + 1);
		if (hotfix_name.length() > 0)
		{
			strcpy((char*)pack.pBuffer, hotfix_name.c_str());
		}
		worldserver.SendPacket(&pack);

		c->Message(CC_Default, "Hotfix applied");
	});

	t1.detach();
}

void command_load_shared_memory(Client *c, const Seperator *sep)
{
	std::string hotfix;
	database.GetVariable("hotfix_name", hotfix);

	std::string hotfix_name;
	if(strcasecmp(hotfix.c_str(), sep->arg[1]) == 0) {
		c->Message(CC_Default, "Cannot attempt to load this shared memory segment as it is already loaded.");
		return;
	}

	hotfix_name = sep->arg[1];
	c->Message(CC_Default, "Loading shared memory segment %s", hotfix_name.c_str());
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
			c->Message(CC_Default, "Shared memory segment failed loading.");
			return;
		}
		c->Message(CC_Default, "Shared memory segment finished loading.");
	});

	t1.detach();
}

void command_apply_shared_memory(Client *c, const Seperator *sep) {
	std::string hotfix;
	database.GetVariable("hotfix_name", hotfix);
	std::string hotfix_name = sep->arg[1];

	c->Message(CC_Default, "Applying shared memory segment %s", hotfix_name.c_str());
	database.SetVariable("hotfix_name", hotfix_name);

	ServerPacket pack(ServerOP_ChangeSharedMem, hotfix_name.length() + 1);
	if(hotfix_name.length() > 0) {
		strcpy((char*)pack.pBuffer, hotfix_name.c_str());
	}
	worldserver.SendPacket(&pack);
}

void command_keyring(Client *c, const Seperator *sep)
{
	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	t->KeyRingList(c);
}

void command_reloadmerchants(Client *c, const Seperator *sep)
{
	zone->ClearMerchantLists();
	zone->GetMerchantDataForZoneLoad();
	zone->LoadTempMerchantData();
	c->Message(CC_Default, "Merchant list reloaded for %s.", zone->GetShortName());
}

void command_setgreed(Client *c, const Seperator *sep)
{
	if (!sep->IsNumber(1))
	{
		c->Message(CC_Default, "Usage: #setgreed [greed]");
		return;
	}

	uint32 value = atoi(sep->arg[1]);
	if (c->GetTarget() && c->GetTarget()->IsNPC())
	{
		c->GetTarget()->CastToNPC()->shop_count = value;
		c->Message(CC_Default, "Set %s greed to %d (%d percent)", c->GetTarget()->GetName(), value, c->GetTarget()->CastToNPC()->GetGreedPercent());
	}
	else
	{
		c->Message(CC_Default, "Please select a NPC target to set greed.");
		return;
	}
		
}

void command_trapinfo(Client *c, const Seperator *sep)
{
	entity_list.GetTrapInfo(c);
}

void command_reloadtraps(Client *c, const Seperator *sep)
{
	entity_list.UpdateAllTraps(true, true);
	c->Message(CC_Default, "Traps reloaded for %s.", zone->GetShortName());
}

void command_godmode(Client *c, const Seperator *sep){
	bool state = atobool(sep->arg[1]);
	uint32 account = c->AccountID();

	if (sep->arg[1][0] != 0)
	{
		c->SetInvul(state);
		database.SetGMInvul(account, state);
		database.SetGMSpeed(account, state ? 1 : 0);
		c->SendAppearancePacket(AT_Levitate, state);
		database.SetGMFlymode(account, state);
		c->SetHideMe(state);
		database.SetGMIgnoreTells(account, state ? 1 : 0);
		c->Message(CC_Default, "Turning GodMode %s for %s (zone for gmspeed to take effect)", state ? "On" : "Off", c->GetName());
	}
	else
		c->Message(CC_Default, "Usage: #godmode [on/off]");
}

void command_skilldifficulty(Client *c, const Seperator *sep)
{
	if (sep->argnum > 0) 
	{
		Client *t;

		if (c->GetTarget() && c->GetTarget()->IsClient())
			t = c->GetTarget()->CastToClient();
		else
			t = c;

		if (strcasecmp(sep->arg[1], "info") == 0)
		{
			for (int i = 0; i < EQ::skills::SkillCount; ++i)
			{
				int rawskillval = t->GetRawSkill(EQ::skills::SkillType(i));
				int skillval = t->GetSkill(EQ::skills::SkillType(i));
				int maxskill = t->GetMaxSkillAfterSpecializationRules(EQ::skills::SkillType(i), t->MaxSkill(EQ::skills::SkillType(i)));
				c->Message(CC_Yellow, "Skill: %s (%d) has difficulty: %0.2f", zone->skill_difficulty[i].name, i, zone->skill_difficulty[i].difficulty);
				if(maxskill > 0)
				{
					c->Message(CC_Green, "%s currently has %d (raw: %d) of %d towards this skill.", t->GetName(), skillval, rawskillval, maxskill);
				}
			}
		}
		else if (strcasecmp(sep->arg[1], "difficulty") == 0)
		{
			if(!sep->IsNumber(2) && !sep->IsNumber(3))
			{
				c->Message(CC_Red, "Please specify a valid skill and difficulty.");
				return;
			}
			else if(atoi(sep->arg[2]) > 74 || atoi(sep->arg[2]) < 0 || atof(sep->arg[3]) > 15 || atof(sep->arg[3]) < 1)
			{
				c->Message(CC_Red, "Please specify a skill between 0 and 74 and a difficulty between 1 and 15.");
				return;
			}
			else
			{
				uint16 skillid = atoi(sep->arg[2]);
				float difficulty = atof(sep->arg[3]);
				database.UpdateSkillDifficulty(skillid, difficulty);
				auto pack = new ServerPacket(ServerOP_ReloadSkills, 0);
				worldserver.SendPacket(pack);
				safe_delete(pack);
				c->Message(CC_Default, "Set skill %d to difficulty %0.2f and reloaded all zones.", skillid, difficulty);
			}

		}
		else if (strcasecmp(sep->arg[1], "reload") == 0)
		{
			auto pack = new ServerPacket(ServerOP_ReloadSkills, 0);
			worldserver.SendPacket(pack);
			safe_delete(pack);
			c->Message(CC_Default, "Reloaded skills in all zones.");
		}
		else if (strcasecmp(sep->arg[1], "values") == 0)
		{
			for (int i = 0; i < EQ::skills::SkillCount; ++i)
			{
				int rawskillval = t->GetRawSkill(EQ::skills::SkillType(i));
				int skillval = t->GetSkill(EQ::skills::SkillType(i));
				int maxskill = t->GetMaxSkillAfterSpecializationRules(EQ::skills::SkillType(i), t->MaxSkill(EQ::skills::SkillType(i)));
				if (skillval > 0)
				{
					uint16 type = CC_Green;
					if (maxskill < 1 || skillval > HARD_SKILL_CAP)
						type = CC_Red;
					else if (skillval > maxskill)
						type = CC_Yellow;

					c->Message(type, "%s currently has %d (raw: %d) of %d towards %s.", t->GetName(), skillval, rawskillval, maxskill, zone->skill_difficulty[i].name);
				}
			}
		}
	}
	else
	{
		c->Message(CC_Default, "Usage: #skills info - Provides information about target.");
		c->Message(CC_Default, "#skills difficulty [skillid] [difficulty] - Sets difficulty for selected skill.");
		c->Message(CC_Default, "#skills reload - Reloads skill difficulty in each zone.");
		c->Message(CC_Default, "#skills values - Displays target's skill values.");
	}
}

void command_makemule(Client* c, const Seperator* sep)
{
	if (!c) {
		return;
	}
	if (c->IsMule()) {
		c->Message(CC_Red, "Account is already flagged as a mule.");
		return;
	}


	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (c->GetBaseRace() == IKSAR)
		{
			c->Message(CC_Red, "Iksars cannot be mules during the pre-launch period.");
			return;
		}
	}

	if (c->GetLevel() < RuleI(Chat, GlobalChatLevelLimit)) {
		c->Message(CC_Red, "To flag an account as a mule you must first level to %i.", RuleI(Chat, GlobalChatLevelLimit));
		return;
	}
	if (c->IsMuleInitiated() && sep->arg[1][0] != 0 && strcasecmp(sep->arg[1], "confirm") == 0) {
		if (!database.SetMule(c->GetName())) {
			c->Message(CC_Red, "Could not set mule status for this account. More than one character already exists on account.");
		}
		else {
			c->Message(CC_Green, "Successfully flagged your account as a mule.");
			c->SetLevel(1);
			if (RuleB(Quarm, EastCommonMules)) {
				c->SetBindPoint(ecommons, glm::vec3(-164.0f, -1651.0f, 4.0f));
			}
			else {
				c->SetBindPoint(bazaar, glm::vec3(140.0f, -821.0f, 5.0f));
			}
			c->WorldKick();
		}
		return;
	}
	if (sep->arg[1][0] != 0 && strcasecmp(sep->arg[1], "confirm") != 0) {
		c->Message(CC_Red, "To confirm your mule account you must type: #makemule confirm");
		return;
	}
	c->SetMuleInitiated(true);
	c->Message(CC_Default, "You have initiated the process to make this account a mule. Mules can not leave designated trader zones or level past 1.");
	c->Message(CC_Default, "Once an account is made a mule it can not be undone. If successful then you will be disconnected and must re-enter the world.");
	c->Message(CC_Default, "If you wish to proceed then type: #makemule confirm");
}

void command_mule(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] != 0)
	{
		uint8 toggle = 0;
		if(sep->IsNumber(2))
			toggle = atoi(sep->arg[2]);

		if(toggle >= 1)
			toggle = 1;
		else
			toggle = 0;

		if(!database.SetMule(sep->arg[1], toggle))
		{
			c->Message(CC_Red, "%s could not be toggled. Check the spelling of their account name.", sep->arg[1]);
		}
		else
		{
			c->Message(CC_Green, "%s is %s a mule!", sep->arg[1], toggle == 0 ? "no longer" : "now");
		}
	}
	else
	{
		c->Message(CC_Default, "Usage: #mule [accountname] [0/1]");
	}
}

void command_ipexemption(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] != 0 && sep->arg[2][0] != 0 && sep->IsNumber(2))
	{
		uint8 amount = atoi(sep->arg[2]);

		if (!database.SetIPExemption(sep->arg[1], amount))
		{
			c->Message(CC_Red, "%s could not be set. Check the spelling of their account name.", sep->arg[1]);
		}
		else
		{
			c->Message(CC_Green, "%s has has their IP exemption amount set to %s!", sep->arg[1], sep->arg[2]);
		}
	}
	else
	{
		c->Message(CC_Default, "Usage: #ipexemption [accountname] [amount]");
	}
}

void command_cleartimers(Client *c, const Seperator *sep)
{
	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	uint8 type = 0;
	if(sep->IsNumber(1))
	{
		type = atoi(sep->arg[1]);
		t->ClearPTimers(type);
		c->Message(CC_Default, "Cleared PTimer %d on %s", type, t->GetName());
	}
	else
	{
		int x = 0;
		if (strcasecmp(sep->arg[1], "help") == 0)
		{
			c->Message(CC_Default, "PTimer list: (1 Unused) (2 Surname) (3 FD) (4 Sneak) (5 Hide) (6 Taunt) (7 InstallDoubt)");
			c->Message(CC_Default, "(8 Fishing) (9 Foraging) (10 Mend) (11 Tracking) (12 SenseTraps) (13 DisarmTraps)");
			c->Message(CC_Default, "(14-16 Discs) (17 Combat Ability) (18 Begging/PP) (19 Sense Heading) (20 Bind Wound)");
			c->Message(CC_Default, "(21 Apply Posion) (22 Disarm) (23 PEQZone)");
			c->Message(CC_Default, "(1000-2999 AAs) (3001-4999 AA Effects) (5000-9678 Spells)");
			c->Message(CC_Default, "(5087 Lay Hands) (5088 Harm Touch)");
		}
		else if(strcasecmp(sep->arg[1], "all") == 0)
		{
			t->ClearPTimers(0);
			t->SendAATimers();
			t->ResetAllSkills();
			c->Message(CC_Default, "Cleared all timers on %s", t->GetName());
		}
		else if(strcasecmp(sep->arg[1], "skills") == 0)
		{
			t->ResetAllSkills();
			c->Message(CC_Default, "Cleared all skill timers on %s", t->GetName());
		}
		else if(strcasecmp(sep->arg[1], "disc") == 0)
		{
			for (x = pTimerDisciplineReuseStart; x <= pTimerDisciplineReuseEnd; ++x)
			{
				t->ClearPTimers(x);
			}
			c->Message(CC_Default, "Cleared all disc timers on %s", t->GetName());
		}
		else if(strcasecmp(sep->arg[1], "aa") == 0)
		{
			for (x = pTimerAAStart; x <= pTimerAAEffectEnd; ++x)
			{
				t->ClearPTimers(x);
			}
			t->SendAATimers();
			c->Message(CC_Default, "Cleared all AA timers on %s", t->GetName());
		}
		else if(strcasecmp(sep->arg[1], "spells") == 0)
		{
			for (x = pTimerSpellStart; x <= pTimerSpellStart + 4678; ++x)
			{
				t->ClearPTimers(x);
			}
			c->Message(CC_Default, "Cleared all spell timers on %s", t->GetName());
		}
		else
		{
			c->Message(CC_Default, "Usage: #cleartimers [type]/help/all/skills/disc/aa/spells");
		}

	}
	t->Save();
}

void command_damagetotals(Client *c, const Seperator *sep)
{
	Mob* target = c->GetTarget();
	if (!target || !target->IsNPC())
	{
		c->Message(CC_Default, "Please target a NPC to use this command on.");
	}
	else
	{
		target->ReportDmgTotals(c);
	}	
}

void command_showregen(Client *c, const Seperator *sep)
{
	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	t->ShowRegenInfo(c);
}

void command_fleeinfo(Client *c, const Seperator *sep)
{
	if (c->GetTarget() && c->GetTarget()->IsNPC())
	{
		Mob* client = entity_list.GetMob(c->GetID());
		if (client)
		{
			c->GetTarget()->FleeInfo(client);
		}
	}
	else
	{
		c->Message(CC_Default, "Please target a NPC to use this command on.");
	}

}

void command_fillbuffs(Client *c, const Seperator *sep)
{
	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	t->BuffFadeAll();
	c->SpellFinished(3295, t);
	c->SpellFinished(278, t);
	c->SpellFinished(3441, t);
	c->SpellFinished(3451, t);
	c->SpellFinished(2519, t);
	c->SpellFinished(226, t);
	c->SpellFinished(227, t);
	c->SpellFinished(228, t);
	c->SpellFinished(3360, t);
	c->SpellFinished(80, t);
	c->SpellFinished(86, t);
	c->SpellFinished(3439, t);
	c->SpellFinished(3450, t);
	c->SpellFinished(3453, t);
	c->SpellFinished(1709, t);

}

void command_showhelm(Client *c, const Seperator *sep)
{
	bool state = atobool(sep->arg[1]);
	bool current_state = c->ShowHelm();
	bool all = strcasecmp(sep->arg[2], "all") == 0 ? true : false;

	if (sep->arg[1][0] != 0) 
	{
		c->SetShowHelm(state);
		if (!state && current_state)
		{
			c->WearChange(EQ::textures::armorHead, 0, 0, c);
			entity_list.HideHelms(c);
			if (all)
			{
				database.SaveAccountShowHelm(c->AccountID(), state);
			}
		}
		else if(state && !current_state)
		{
			c->SendWearChange(EQ::textures::armorHead, c);
			entity_list.SendHelms(c);
			if (all)
			{
				database.SaveAccountShowHelm(c->AccountID(), state);
			}
		}
		else
		{
			c->Message(CC_Default, "There was no change in your showhelm setting.");
			return;
		}
		
		c->Message(CC_Yellow, "You will %s display helms.", state ? "now" : "no longer");
	}
	else
		c->Message(CC_Default, "Usage: #showhelm on/off [all]");

	return;
}


void command_underworld(Client *c, const Seperator *sep)
{
	float z_coord = 0;
	if (sep->IsNumber(1))
	{
		z_coord = atof(sep->arg[1]);
	}
	else
	{
		std::string query = StringFormat("SELECT min(z) from spawn2 where zone = '%s'", zone->GetShortName());
		auto results = database.QueryDatabase(query);
		if (!results.Success() || results.RowCount() == 0)
		{
			c->Message(CC_Red, "This zone has no valid spawn2 entries.");
			return;
		}

		auto row = results.begin();
		z_coord = atof(row[0]);

		std::string query1 = StringFormat("SELECT min(z) from grid_entries where zoneid = %i", zone->GetZoneID());
		auto results1 = database.QueryDatabase(query1);

		if (results1.Success() && results1.RowCount() != 0)
		{
			auto row = results1.begin();
			if (atof(row[0]) < z_coord)
			{
				z_coord = atof(row[0]);
			}
		}
	}

	entity_list.ReportUnderworldNPCs(c, z_coord);
	return;
}

void command_expansion(Client *c, const Seperator *sep)
{
	if (sep->arg[1][0] != 0)
	{
		uint8 toggle = 0;
		if (sep->IsNumber(2))
			toggle = atoi(sep->arg[2]);

		if (!database.SetExpansion(sep->arg[1], toggle))
		{
			c->Message(CC_Red, "%s could not be toggled. Check the spelling of their account name.", sep->arg[1]);
		}
		else
		{
			c->Message(CC_Green, "%s has their expansion set to %d!", sep->arg[1], toggle);
		}
	}
	else
	{
		c->Message(CC_Default, "Usage: expansion [accountname] [expansion]");
		c->Message(CC_Default, "Expansions: 0 - Classic 1 - Kunark 2 - Velious 4 - Luclin 8 - PoP");
		c->Message(CC_Default, "Added them together to set which expansions the account will have.");
		c->Message(CC_Default, "Classic cannot be disabled.");
	}
}

void command_showtraderitems(Client *c, const Seperator *sep)
{
	Client *t;

	if (c->GetTarget() && c->GetTarget()->IsClient())
		t = c->GetTarget()->CastToClient();
	else
		t = c;

	c->DisplayTraderInventory(t);
}

void command_playsound(Client* c, const Seperator* sep)
{
	uint16 soundnum = 50;
	if (sep->IsNumber(1))
	{
		soundnum = atoi(sep->arg[1]);
		if (soundnum > 3999)
		{
			c->Message(CC_Default, "Sound number out of range.");
			return;
		}
	}

	c->SendSound(soundnum);
}

void command_viewzoneloot(Client* c, const Seperator* sep)
{
	std::map<uint32, ItemList> zone_loot_list;
	auto npc_list = entity_list.GetNPCList();
	uint32 loot_amount = 0, loot_id = 1, search_item_id = 0;
	if (sep->argnum == 1 && sep->IsNumber(1)) {
		search_item_id = atoi(sep->arg[1]);
	}
	else if (sep->argnum == 1 && !sep->IsNumber(1)) {
		c->Message(
			CC_Yellow,
			"Usage: #viewzoneloot [item id]"
		);
		return;
	}
	for (auto npc_entity : npc_list) {
		auto current_npc_item_list = npc_entity.second->GetItemList();
		zone_loot_list.insert({ npc_entity.second->GetID(), current_npc_item_list });
	}
	for (auto loot_item : zone_loot_list) {
		uint32 current_entity_id = loot_item.first;
		auto current_item_list = loot_item.second;
		auto current_npc = entity_list.GetNPCByID(current_entity_id);
		std::string npc_link;
		if (current_npc) {
			std::string npc_name = current_npc->GetCleanName();
			uint32 zone_id = zone->GetZoneID();
			// The GenerateQuestSaylink doesn't always produce desired result. it needs a workaround fix
			/*std::string command_link = EQ::SayLinkEngine::GenerateQuestSaylink(
				fmt::format(
					"#{} {} {} {} {}",
					"zone",
					zone_id,
					current_npc->GetX(),
					current_npc->GetY(),
					current_npc->GetZ()
				),
				false,
				"Goto"
			);*/
			npc_link = fmt::format(
				" NPC: {} (ID {}) ", // [{}]",
				npc_name,
				current_entity_id
				//command_link
			);
		}

		for (auto current_item : current_item_list) {
			if (search_item_id == 0 || current_item->item_id == search_item_id) {
				EQ::SayLinkEngine linker;
				linker.SetLinkType(EQ::saylink::SayLinkLootItem);
				linker.SetLootData(current_item);
				c->Message(
					CC_Default,
					fmt::format(
						" {}. {} ({}) {} ",
						loot_id,
						linker.GenerateLink(),
						current_item->item_id,
						npc_link
					).c_str()
				);
				loot_id++;
				loot_amount++;
			}
		}
	}


	if (search_item_id != 0) {
		std::string drop_string = (
			loot_amount > 0 ?
			fmt::format(
				"dropping in {} {} ",
				loot_amount,
				(loot_amount > 1 ? "places" : "place")
			) :
			"not dropping"
			);
		c->Message(
			CC_Default,
			fmt::format(
				" {} ({}) is {}. ",
				database.CreateItemLink(search_item_id),
				search_item_id,
				drop_string
			).c_str()
		);
	}
	else {
		std::string drop_string = (
			loot_amount > 0 ?
			fmt::format(
				" {} {} {} ",
				(loot_amount > 1 ? "items" : "item"),
				(loot_amount > 1 ? "are" : "is"),
				(loot_amount > 1 ? "dropping" : "not dropping")
			) :
			"items are dropping"
			);
		c->Message(
			CC_Default,
			fmt::format(
				" {} {}. ",
				loot_amount,
				drop_string
			).c_str()
		);
	}
}

void command_quaketrigger(Client* c, const Seperator* sep)
{
	//Arguments?
	if (sep->IsNumber(1))
	{
		uint8_t quaketype = atoi(sep->arg[1]);
		auto pack = new ServerPacket(ServerOP_QuakeRequest, sizeof(ServerEarthquakeRequest_Struct));
		ServerEarthquakeRequest_Struct* sqr = (ServerEarthquakeRequest_Struct*)pack->pBuffer;
		sqr->type = (QuakeType)quaketype;
		worldserver.SendPacket(pack);
		safe_delete(pack);
		c->Message(15, "Triggered an earthquake!");
	}
	else
	{
		c->Message(15, "Invalid parameters. Usage: #quaketrigger [type_num]");
	}
}

void command_betabuff(Client* c, const Seperator* sep)
{
	//Arguments?
	if (sep->IsNumber(1))
	{
		//Variables
		uint32 level = atoi(sep->arg[1]);
		int curspell = 0;
		int book_slot = 0;

		uint16 skillLevel = HARD_SKILL_CAP;

		int pClass = c->GetClass();

		bool hasBetaBuffed = false;

		int bandedIDs[12] = { 3053,3054,3055,3056,3061,3057,3058,3059,3060,3062,3063,3064 };
		int smallBandedIDs[12] = { 3065,3066,3067,3068,3069,3070,3071,3072,3073,3074,3075,3076 };
		int largeBandedIDs[12] = { 3080,3084,3087,3089,3088,3083,3085,3077,3082,3081,3078,3079 };
		int rawSilkIDs[12] = { 1160,1161,1162,1163,1164,1165,1166,1167,1168,1169,1170,1171 };
		int reinforcedIDs[12] = { 2237,2238,2239,2240,2241,2242,2243,2244,2245,2246,2247 };
		int smallReinforcedIDs[12] = { 2249,2250,2251,2252,2253,2254,2255,2256,2257,2258,2259,2260 };
		int largeReinforcedIDs[12] = { 2261,2262,2263,2264,2265,2266,2267,2268,2269,2270,2271,2272 };

		//Get Armor Size based on Race(Function)
		uint8 armorSize = c->GetRaceArmorSize();
		//Check if level argument is lower than our level


		if ((uint32)c->GetLevel() >= level)
		{
			c->Message(CC_Red, "This character cannot be buffed to this level.");
			return;
		}

		//Check MaxBetaBuffLevel Rule
		if (level > RuleI(Character, MaxBetaBuffLevel))
		{
			c->Message(CC_Red, "This character cannot be buffed to this level. The current betabuff cap is: %i", RuleI(Character, MaxBetaBuffLevel));
			return;
		}

		//Check if character is above the max level 
		if (c->GetLevel() > RuleI(Character, MaxLevel))
		{
			c->Message(CC_Red, "This character is above the maximum level for test buff.");
			return;
		}

		//Set Level and Exp
		c->SetLevel(1, true);
		c->SetLevel(level, true);
		c->AddEXPPercent(10, level);

		//Scribe Spells
		for (curspell = 0, book_slot = c->GetNextAvailableSpellBookSlot(); curspell < SPDAT_RECORDS && book_slot < MAX_PP_SPELLBOOK; curspell++, book_slot = c->GetNextAvailableSpellBookSlot(book_slot))
		{
			if (spells[curspell].classes[c->GetPP().class_ - 1] >= 1 && spells[curspell].classes[c->GetPP().class_ - 1] <= level) {
				if (!c->HasSpellScribed(curspell)) {
					c->ScribeSpell(curspell, book_slot);
				}
			}
		}
		// Skills
		for (EQ::skills::SkillType skill_num = EQ::skills::Skill1HBlunt; skill_num <= EQ::skills::HIGHEST_SKILL; skill_num = (EQ::skills::SkillType)(skill_num + 1))
		{
			uint16 max_level = c->GetMaxSkillAfterSpecializationRules(skill_num, c->MaxSkill(skill_num));
			uint16 cap_level = skillLevel > max_level ? max_level : skillLevel;
			c->SetSkill(skill_num, cap_level);
		}

		//Summon Bags, Gear and Plat for first time beta buff
		if (!c->HasBetaBuffGearFlag())
		{
			int16 slot = c->GetInv().FindFreeSlot(0, 0);
			if (slot >= 0)
				c->SummonItem(17046, 1, slot);
			slot = c->GetInv().FindFreeSlot(0, 0);
			if (slot >= 0)
				c->SummonItem(17046, 1, slot);
			slot = c->GetInv().FindFreeSlot(0, 0);
			if (slot >= 0)
				c->SummonItem(17046, 1, slot);
			slot = c->GetInv().FindFreeSlot(0, 0);
			if (slot >= 0)
				c->SummonItem(6352, 1, slot);

			//Armor Summoning Skeleton
			switch (pClass)
			{
			case CLERIC:
			case PALADIN:
			case SHAMAN:
			case RANGER:
			case SHADOWKNIGHT:
			case BARD:
			case ROGUE:
			case WARRIOR:
				if (armorSize == 0) // Small
				{
					for (int i : smallBandedIDs)
					{
						const EQ::ItemData* banded_item = database.GetItem(i);
						slot = c->GetInv().FindFreeSlot(0, 0);
						if (banded_item && slot >= 0)
						{
							c->SummonItem(i, 1, slot);
							if (banded_item->Slots == 1536) // SLOT_WRIST IDs
							{
								slot = c->GetInv().FindFreeSlot(0, 0);
								c->SummonItem(i, 1, slot);
							}
						}
					}
				}
				else if (armorSize == 1) // Medium
				{
					for (int i : bandedIDs)
					{
						const EQ::ItemData* banded_item = database.GetItem(i);
						slot = c->GetInv().FindFreeSlot(0, 0);
						if (banded_item && slot >= 0)
						{
							c->SummonItem(i, 1, slot);
							if (banded_item->Slots == 1536) // SLOT_WRIST IDs
							{
								slot = c->GetInv().FindFreeSlot(0, 0);
								if (slot >= 0)
									c->SummonItem(i, 1, slot);
							}
						}
					}
				}
				if (armorSize == 2) // Large
				{
					for (int i : largeBandedIDs)
					{
						const EQ::ItemData* banded_item = database.GetItem(i);
						slot = c->GetInv().FindFreeSlot(0, 0);
						if (banded_item && slot >= 0)
						{
							c->SummonItem(i, 1, slot);
							if (banded_item->Slots == 1536) // SLOT_WRIST IDs
							{
								slot = c->GetInv().FindFreeSlot(0, 0);
								if (slot >= 0)
									c->SummonItem(i, 1, slot);
							}
						}
					}
				}
				break;
			case NECROMANCER:
			case ENCHANTER:
			case MAGICIAN:
			case WIZARD:
				for (int i : rawSilkIDs)
				{
					const EQ::ItemData* banded_item = database.GetItem(i);
					slot = c->GetInv().FindFreeSlot(0, 0);
					if (banded_item && slot >= 0)
					{
						c->SummonItem(i, 1, slot);
						if (banded_item->Slots == 1536) // SLOT_WRIST IDs
						{
							slot = c->GetInv().FindFreeSlot(0, 0);
							c->SummonItem(i, 1, slot);
						}
					}
				}
				break;
			case DRUID:
			case BEASTLORD:
			case MONK:
				if (armorSize == 0)
				{
					for (int i : smallReinforcedIDs)
					{
						const EQ::ItemData* banded_item = database.GetItem(i);
						slot = c->GetInv().FindFreeSlot(0, 0);
						if (banded_item && slot >= 0)
						{
							c->SummonItem(i, 1, slot);
							if (banded_item->Slots == 1536) // SLOT_WRIST IDs
							{
								slot = c->GetInv().FindFreeSlot(0, 0);
								if (slot >= 0)
									c->SummonItem(i, 1, slot);
							}
						}
					}
					break;
				}

				if (armorSize == 1)
				{
					for (int i : reinforcedIDs)
					{
						const EQ::ItemData* banded_item = database.GetItem(i);
						slot = c->GetInv().FindFreeSlot(0, 0);
						if (banded_item && slot >= 0)
						{
							c->SummonItem(i, 1, slot);
							if (banded_item->Slots == 1536) // SLOT_WRIST IDs
							{
								slot = c->GetInv().FindFreeSlot(0, 0);
								if (slot >= 0)
									c->SummonItem(i, 1, slot);
							}
						}
					}
				}

				if (armorSize == 2) // LARGE
				{
					for (int i : largeReinforcedIDs)
					{
						const EQ::ItemData* banded_item = database.GetItem(i);
						slot = c->GetInv().FindFreeSlot(0, 0);
						if (banded_item && slot >= 0)
						{
							c->SummonItem(i, 1, slot);
							if (banded_item->Slots == 1536) // SLOT_WRIST IDs
							{
								slot = c->GetInv().FindFreeSlot(0, 0);
								if (slot >= 0)
									c->SummonItem(i, 1, slot);
							}
						}
					}
				}

				//Weapons:
				//ID:  Weapon:
				//6350 Fine Steel Warhammer
				//6352 Fine Steel Great Staff
				//6359 Stein of Moggok
				//9002 Round Shield
				//7352 Fine Steel Rapier
				//7350 Fine Steel Dagger
				slot = c->GetInv().FindFreeSlot(0, 0);
				if (slot >= 0)
					c->SummonItem(6350, 1, slot);

				slot = c->GetInv().FindFreeSlot(0, 0);
				if (slot >= 0)
					c->SummonItem(6352, 1, slot);

				slot = c->GetInv().FindFreeSlot(0, 0);
				if (slot >= 0)
					c->SummonItem(9002, 1, slot);

				slot = c->GetInv().FindFreeSlot(0, 0);
				if (slot >= 0)
					c->SummonItem(7352, 1, slot);

				slot = c->GetInv().FindFreeSlot(0, 0);
				if (slot >= 0)
					c->SummonItem(7350, 1, slot);
				slot = c->GetInv().FindFreeSlot(0, 0);
				if (slot >= 0)
					c->SummonItem(6359, 1, slot);

				c->AddMoneyToPP(0, 0, 0, 100 * level, true);

				//Give Sow and Strength incase we're encumbered or too low agility.
				c->SetGMSpellException(1);
				c->SpellFinished(278, c); // Spirit of Wolf
				c->SpellFinished(430, c); // Storm Strength
				c->SetGMSpellException(0);


				//Pet Reagents
				switch (pClass)
				{
				case NECROMANCER:
					slot = c->GetInv().FindFreeSlot(0, 0);
					if(slot >= 20)
						c->SummonItem(13073, 20, slot);
					break;
				case MAGICIAN:
					slot = c->GetInv().FindFreeSlot(0, 0);
					if (slot >= 20)
						c->SummonItem(10015, 20, slot);
					break;
				case ENCHANTER:
					slot = c->GetInv().FindFreeSlot(0, 0);
					if (slot >= 20)
						c->SummonItem(13080, 20, slot);
					break;
				}
			}

			c->SetBetaBuffGearFlag(1);
			c->Save(1);

		}
	}
	else
	{
		c->Message(CC_Default, "Usage: #betabuff [level] - level must be a level higher than your current level.");
	}
}

void command_removelegacyitem(Client* c, const Seperator* sep) {
	if (sep->IsNumber(1) && sep->arg[2] == 0)
	{
		Mob* target = c->GetTarget();
		if (target && target->IsClient())
		{
			Client* target_client = target->CastToClient();
			bool res = target_client->RemoveLootedLegacyItem(atoi(sep->arg[1]));
			if(!res)
			{
				c->Message(MT_System, "Unable to remove legacy item %d from %s.", atoi(sep->arg[1]), target->GetCleanName());
				return;
			}
			else
			{
				c->Message(MT_System, "Successfully removed legacy item id flag %d from %s.", atoi(sep->arg[1]), target->GetCleanName());
				return;
			}
		}
		else
		{
			c->Message(MT_Shout, "You need a target for this GM command.");
			return;
		}
	}
	else if (sep->IsNumber(1) && sep->IsNumber(2))
	{
		char target_name[64] = { 0 };
		target_name[0] = '\0';
		database.GetCharName(atoi(sep->arg[1]), target_name);
		if (target_name[0] == '\0')
		{
			c->Message(MT_Shout, "This is not a valid character to perform this interaction on!");
			return;
		}

		std::string query = StringFormat("DELETE FROM character_legacy_items WHERE character_id = %d AND item_id = %d", atoi(sep->arg[1]), atoi(sep->arg[2]));

		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			c->Message(MT_System, "Unable to run query!");
			return;
		}
		c->Message(MT_System, "Successfully attempted to remove legacy item id flag %d from %s.", sep->arg[2], target_name);
		return;
	}
	c->Message(MT_Shout, "Usage: #removelegacyitem [charid] [itemid] or #removelegacyitem [itemid] with a Client targeted.");
}

void command_interrogatelegacy(Client* c, const Seperator* sep) {
	if (c && c->GetTarget() && c->GetTarget()->IsClient())
	{
		c->GetTarget()->CastToClient()->ShowLegacyItemsLooted(c);
	}
	else
	{
		c->Message(MT_Shout, "Error: requires a client target!");
	}
}

//Please keep this at the bottom of command.cpp! Feel free to use this for temporary commands used in testing :)
void command_testcommand(Client *c, const Seperator *sep)
{
	return;
}
