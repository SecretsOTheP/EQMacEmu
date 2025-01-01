/**
 * EQEmulator: Everquest Server Emulator
 * Copyright (C) 2001-2019 EQEmulator Development Team (https://github.com/EQEmu/Server)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY except by those people which sell it, which
 * are required to give you total support for your newly bought product;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
*/

#ifndef EQEMU_EQEMU_LOGSYS_LOG_ALIASES_H
#define EQEMU_EQEMU_LOGSYS_LOG_ALIASES_H

#define LogAA(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::AA))\
        OutF(LogSys, Logs::General, Logs::AA, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAADetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::AA))\
        OutF(LogSys, Logs::Detail, Logs::AA, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAI(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::AI))\
        OutF(LogSys, Logs::General, Logs::AI, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAIDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::AI))\
        OutF(LogSys, Logs::Detail, Logs::AI, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAggro(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Aggro))\
        OutF(LogSys, Logs::General, Logs::Aggro, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAggroDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Aggro))\
        OutF(LogSys, Logs::Detail, Logs::Aggro, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAttack(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Attack))\
        OutF(LogSys, Logs::General, Logs::Attack, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAttackDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Attack))\
        OutF(LogSys, Logs::Detail, Logs::Attack, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketClientServer(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PacketClientServer))\
        OutF(LogSys, Logs::General, Logs::PacketClientServer, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketClientServerDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PacketClientServer))\
        OutF(LogSys, Logs::Detail, Logs::PacketClientServer, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCombat(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Combat))\
        OutF(LogSys, Logs::General, Logs::Combat, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCombatDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Combat))\
        OutF(LogSys, Logs::Detail, Logs::Combat, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCommands(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Commands))\
        OutF(LogSys, Logs::General, Logs::Commands, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCommandsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Commands))\
        OutF(LogSys, Logs::Detail, Logs::Commands, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCrash(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Crash))\
        OutF(LogSys, Logs::General, Logs::Crash, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCrashDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Crash))\
        OutF(LogSys, Logs::Detail, Logs::Crash, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDebug(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Debug))\
        OutF(LogSys, Logs::General, Logs::Debug, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDebugDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Debug))\
        OutF(LogSys, Logs::Detail, Logs::Debug, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDoors(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Doors))\
        OutF(LogSys, Logs::General, Logs::Doors, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDoorsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Doors))\
        OutF(LogSys, Logs::Detail, Logs::Doors, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogError(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Error))\
        OutF(LogSys, Logs::General, Logs::Error, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogErrorDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Error))\
        OutF(LogSys, Logs::Detail, Logs::Error, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogGuilds(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Guilds))\
        OutF(LogSys, Logs::General, Logs::Guilds, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogGuildsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Guilds))\
        OutF(LogSys, Logs::Detail, Logs::Guilds, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogInventory(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Inventory))\
        OutF(LogSys, Logs::General, Logs::Inventory, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogInventoryDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Inventory))\
        OutF(LogSys, Logs::Detail, Logs::Inventory, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogLauncher(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Launcher))\
        OutF(LogSys, Logs::General, Logs::Launcher, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogLauncherDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Launcher))\
        OutF(LogSys, Logs::Detail, Logs::Launcher, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNetcode(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Netcode))\
        OutF(LogSys, Logs::General, Logs::Netcode, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNetcodeDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Netcode))\
        OutF(LogSys, Logs::Detail, Logs::Netcode, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNormal(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Normal))\
        OutF(LogSys, Logs::General, Logs::Normal, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNormalDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Normal))\
        OutF(LogSys, Logs::Detail, Logs::Normal, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogObject(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Object))\
        OutF(LogSys, Logs::General, Logs::Object, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogObjectDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Object))\
        OutF(LogSys, Logs::Detail, Logs::Object, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPathing(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Pathing))\
        OutF(LogSys, Logs::General, Logs::Pathing, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPathingDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Pathing))\
        OutF(LogSys, Logs::Detail, Logs::Pathing, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogQuests(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Quests))\
        OutF(LogSys, Logs::General, Logs::Quests, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogQuestsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Quests))\
        OutF(LogSys, Logs::Detail, Logs::Quests, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogRules(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Rules))\
        OutF(LogSys, Logs::General, Logs::Rules, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogRulesDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Rules))\
        OutF(LogSys, Logs::Detail, Logs::Rules, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSkills(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Skills))\
        OutF(LogSys, Logs::General, Logs::Skills, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSkillsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Skills))\
        OutF(LogSys, Logs::Detail, Logs::Skills, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSpawns(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Spawns))\
        OutF(LogSys, Logs::General, Logs::Spawns, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSpawnsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Spawns))\
        OutF(LogSys, Logs::Detail, Logs::Spawns, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSpells(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Spells))\
        OutF(LogSys, Logs::General, Logs::Spells, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSpellsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Spells))\
        OutF(LogSys, Logs::Detail, Logs::Spells, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogStatus(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Status))\
        OutF(LogSys, Logs::General, Logs::Status, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogStatusDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Status))\
        OutF(LogSys, Logs::Detail, Logs::Status, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTCPConnection(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::TCPConnection))\
        OutF(LogSys, Logs::General, Logs::TCPConnection, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTCPConnectionDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::TCPConnection))\
        OutF(LogSys, Logs::Detail, Logs::TCPConnection, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTasks(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Tasks))\
        OutF(LogSys, Logs::General, Logs::Tasks, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTasksDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Tasks))\
        OutF(LogSys, Logs::Detail, Logs::Tasks, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTradeskills(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Tradeskills))\
        OutF(LogSys, Logs::General, Logs::Tradeskills, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTradeskillsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Tradeskills))\
        OutF(LogSys, Logs::Detail, Logs::Tradeskills, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTrading(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Trading))\
        OutF(LogSys, Logs::General, Logs::Trading, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTradingDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Trading))\
        OutF(LogSys, Logs::Detail, Logs::Trading, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogMySQLError(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::MySQLError))\
        OutF(LogSys, Logs::General, Logs::MySQLError, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogMySQLErrorDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::MySQLError))\
        OutF(LogSys, Logs::Detail, Logs::MySQLError, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogMySQLQuery(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::MySQLQuery))\
        OutF(LogSys, Logs::General, Logs::MySQLQuery, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogMySQLQueryDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::MySQLQuery))\
        OutF(LogSys, Logs::Detail, Logs::MySQLQuery, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogEQMac(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::EQMac))\
        OutF(LogSys, Logs::General, Logs::EQMac, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogEQMacDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::EQMac))\
        OutF(LogSys, Logs::Detail, Logs::EQMac, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogQuestDebug(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::QuestDebug))\
        OutF(LogSys, Logs::General, Logs::QuestDebug, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogQuestDebugDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::QuestDebug))\
        OutF(LogSys, Logs::Detail, Logs::QuestDebug, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketServerClient(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PacketServerClient))\
        OutF(LogSys, Logs::General, Logs::PacketServerClient, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketServerClientDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PacketServerClient))\
        OutF(LogSys, Logs::Detail, Logs::PacketServerClient, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketClientServerUnhandled(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PacketClientServerUnhandled))\
        OutF(LogSys, Logs::General, Logs::PacketClientServerUnhandled, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketClientServerUnhandledDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PacketClientServerUnhandled))\
        OutF(LogSys, Logs::Detail, Logs::PacketClientServerUnhandled, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketServerClientWithDump(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PacketServerClientWithDump))\
        OutF(LogSys, Logs::General, Logs::PacketServerClientWithDump, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketServerClientWithDumpDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PacketServerClientWithDump))\
        OutF(LogSys, Logs::Detail, Logs::PacketServerClientWithDump, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketClientServerWithDump(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PacketClientServerWithDump))\
        OutF(LogSys, Logs::General, Logs::PacketClientServerWithDump, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPacketClientServerWithDumpDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PacketClientServerWithDump))\
        OutF(LogSys, Logs::Detail, Logs::PacketClientServerWithDump, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogMaps(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Maps))\
        OutF(LogSys, Logs::General, Logs::Maps, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogMapsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Maps))\
        OutF(LogSys, Logs::Detail, Logs::Maps, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCharacter(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Character))\
        OutF(LogSys, Logs::General, Logs::Character, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCharacterDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Character))\
        OutF(LogSys, Logs::Detail, Logs::Character, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogFaction(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Faction))\
        OutF(LogSys, Logs::General, Logs::Faction, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogFactionDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Faction))\
        OutF(LogSys, Logs::Detail, Logs::Faction, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogGroup(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Group))\
        OutF(LogSys, Logs::General, Logs::Group, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogGroupDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Group))\
        OutF(LogSys, Logs::Detail, Logs::Group, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCorpse(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Corpse))\
        OutF(LogSys, Logs::General, Logs::Corpse, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCorpseDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Corpse))\
        OutF(LogSys, Logs::Detail, Logs::Corpse, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogBazaar(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Bazaar))\
        OutF(LogSys, Logs::General, Logs::Bazaar, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogBazaarDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Bazaar))\
        OutF(LogSys, Logs::Detail, Logs::Bazaar, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDiscs(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Discs))\
        OutF(LogSys, Logs::General, Logs::Discs, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDiscsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Discs))\
        OutF(LogSys, Logs::Detail, Logs::Discs, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogBoats(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Boats))\
        OutF(LogSys, Logs::General, Logs::Boats, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogBoatsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Boats))\
        OutF(LogSys, Logs::Detail, Logs::Boats, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTraps(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Traps))\
        OutF(LogSys, Logs::General, Logs::Traps, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogTrapsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Traps))\
        OutF(LogSys, Logs::Detail, Logs::Traps, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPTimers(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PTimers))\
        OutF(LogSys, Logs::General, Logs::PTimers, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPTimersDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PTimers))\
        OutF(LogSys, Logs::Detail, Logs::PTimers, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNexus(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Nexus))\
        OutF(LogSys, Logs::General, Logs::Nexus, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNexusDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Nexus))\
        OutF(LogSys, Logs::Detail, Logs::Nexus, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPets(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Pets))\
        OutF(LogSys, Logs::General, Logs::Pets, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPetsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Pets))\
        OutF(LogSys, Logs::Detail, Logs::Pets, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogRegen(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Regen))\
        OutF(LogSys, Logs::General, Logs::Regen, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogRegenDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Regen))\
        OutF(LogSys, Logs::Detail, Logs::Regen, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogFocus(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Focus))\
        OutF(LogSys, Logs::General, Logs::Focus, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogFocusDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Focus))\
        OutF(LogSys, Logs::Detail, Logs::Focus, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDeath(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Death))\
        OutF(LogSys, Logs::General, Logs::Death, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDeathDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Death))\
        OutF(LogSys, Logs::Detail, Logs::Death, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogInfo(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Info))\
        OutF(LogSys, Logs::General, Logs::Info, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogInfoDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Info))\
        OutF(LogSys, Logs::Detail, Logs::Info, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogWarning(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Warning))\
        OutF(LogSys, Logs::General, Logs::Warning, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogWarningDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Warning))\
        OutF(LogSys, Logs::Detail, Logs::Warning, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCritical(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Critical))\
        OutF(LogSys, Logs::General, Logs::Critical, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCriticalDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Critical))\
        OutF(LogSys, Logs::Detail, Logs::Critical, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogEmergency(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Emergency))\
        OutF(LogSys, Logs::General, Logs::Emergency, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogEmergencyDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Emergency))\
        OutF(LogSys, Logs::Detail, Logs::Emergency, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAlert(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Alert))\
        OutF(LogSys, Logs::General, Logs::Alert, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogAlertDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Alert))\
        OutF(LogSys, Logs::Detail, Logs::Alert, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNotice(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Notice))\
        OutF(LogSys, Logs::General, Logs::Notice, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogNoticeDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Notice))\
        OutF(LogSys, Logs::Detail, Logs::Notice, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogQuestErrors(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::QuestErrors))\
        OutF(LogSys, Logs::General, Logs::QuestErrors, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogQuestErrorsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::QuestErrors))\
        OutF(LogSys, Logs::Detail, Logs::QuestErrors, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogScheduler(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Scheduler))\
        OutF(LogSys, Logs::General, Logs::Scheduler, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogSchedulerDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Scheduler))\
        OutF(LogSys, Logs::Detail, Logs::Scheduler, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogLoot(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Loot))\
        OutF(LogSys, Logs::General, Logs::Loot, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogLootDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Loot))\
        OutF(LogSys, Logs::Detail, Logs::Loot, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogFixZ(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::FixZ))\
        OutF(LogSys, Logs::General, Logs::FixZ, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogFixZDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::FixZ))\
        OutF(LogSys, Logs::Detail, Logs::FixZ, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogHotReload(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::HotReload))\
        OutF(LogSys, Logs::General, Logs::HotReload, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogHotReloadDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::HotReload))\
        OutF(LogSys, Logs::Detail, Logs::HotReload, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogZonePoints(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::ZonePoints))\
        OutF(LogSys, Logs::General, Logs::ZonePoints, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogZonePointsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::ZonePoints))\
        OutF(LogSys, Logs::Detail, Logs::ZonePoints, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogEqTime(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::EqTime))\
        OutF(LogSys, Logs::General, Logs::EqTime, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogEqTimeDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::EqTime))\
        OutF(LogSys, Logs::Detail, Logs::EqTime, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogClientLogin(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::ClientLogin))\
        OutF(LogSys, Logs::General, Logs::ClientLogin, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogClientLoginDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::ClientLogin))\
        OutF(LogSys, Logs::Detail, Logs::ClientLogin, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDiscord(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Discord))\
        OutF(LogSys, Logs::General, Logs::Discord, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogDiscordDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Discord))\
        OutF(LogSys, Logs::Detail, Logs::Discord, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCombatRecord(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::CombatRecord))\
        OutF(LogSys, Logs::General, Logs::CombatRecord, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCombatRecordDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::CombatRecord))\
        OutF(LogSys, Logs::Detail, Logs::CombatRecord, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCheat(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::Cheat))\
        OutF(LogSys, Logs::General, Logs::Cheat, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogCheatDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::Cheat))\
        OutF(LogSys, Logs::Detail, Logs::Cheat, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPlayerEvents(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::General, Logs::PlayerEvents))\
        OutF(LogSys, Logs::General, Logs::PlayerEvents, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogPlayerEventsDetail(message, ...) do {\
    if (LogSys.IsLogEnabled(Logs::Detail, Logs::PlayerEvents))\
        OutF(LogSys, Logs::Detail, Logs::PlayerEvents, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define Log(debug_level, log_category, message, ...) do {\
    if (LogSys.IsLogEnabled(debug_level, log_category))\
        LogSys.Out(debug_level, log_category, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)

#define LogF(debug_level, log_category, message, ...) do {\
    if (LogSys.IsLogEnabled(debug_level, log_category))\
        OutF(LogSys, debug_level, log_category, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)
#else
#define LogEmergency(message, ...) do {\
} while (0)

#define LogAlert(message, ...) do {\
} while (0)

#define LogCritical(message, ...) do {\
} while (0)

#define LogError(message, ...) do {\
} while (0)

#define LogWarning(message, ...) do {\
} while (0)

#define LogNotice(message, ...) do {\
} while (0)

#define LogInfo(message, ...) do {\
} while (0)

#define LogDebug(message, ...) do {\
} while (0)

#define LogAA(message, ...) do {\
} while (0)

#define LogAADetail(message, ...) do {\
} while (0)

#define LogAI(message, ...) do {\
} while (0)

#define LogAIDetail(message, ...) do {\
} while (0)

#define LogAggro(message, ...) do {\
} while (0)

#define LogAggroDetail(message, ...) do {\
} while (0)

#define LogAttack(message, ...) do {\
} while (0)

#define LogAttackDetail(message, ...) do {\
} while (0)

#define LogPacketClientServer(message, ...) do {\
} while (0)

#define LogPacketClientServerDetail(message, ...) do {\
} while (0)

#define LogCombat(message, ...) do {\
} while (0)

#define LogCombatDetail(message, ...) do {\
} while (0)

#define LogCommands(message, ...) do {\
} while (0)

#define LogCommandsDetail(message, ...) do {\
} while (0)

#define LogCrash(message, ...) do {\
    if (LogSys.log_settings[Logs::Crash].is_category_enabled == 1)\
        OutF(LogSys, Logs::General, Logs::Crash, __FILE__, __func__, __LINE__, message, ##__VA_ARGS__);\
} while (0)
	
#define LogCrashDetail(message, ...) do {\
} while (0)

#define LogDoors(message, ...) do {\
} while (0)

#define LogDoorsDetail(message, ...) do {\
} while (0)

#define LogGuilds(message, ...) do {\
} while (0)

#define LogGuildsDetail(message, ...) do {\
 } while (0)

#define LogInventory(message, ...) do {\
} while (0)

#define LogInventoryDetail(message, ...) do {\
} while (0)

#define LogLauncher(message, ...) do {\
} while (0)

#define LogLauncherDetail(message, ...) do {\
} while (0)

#define LogNetcode(message, ...) do {\
} while (0)

#define LogNetcodeDetail(message, ...) do {\
} while (0)

#define LogNormal(message, ...) do {\
} while (0)

#define LogNormalDetail(message, ...) do {\
} while (0)

#define LogObject(message, ...) do {\
} while (0)

#define LogObjectDetail(message, ...) do {\
} while (0)

#define LogPathing(message, ...) do {\
} while (0)

#define LogPathingDetail(message, ...) do {\
} while (0)

#define LogQSServer(message, ...) do {\
} while (0)

#define LogQSServerDetail(message, ...) do {\
} while (0)

#define LogQuests(message, ...) do {\
} while (0)

#define LogQuestsDetail(message, ...) do {\
} while (0)

#define LogRules(message, ...) do {\
} while (0)

#define LogRulesDetail(message, ...) do {\
} while (0)

#define LogSkills(message, ...) do {\
} while (0)

#define LogSkillsDetail(message, ...) do {\
} while (0)

#define LogSpawns(message, ...) do {\
} while (0)

#define LogSpawnsDetail(message, ...) do {\
} while (0)

#define LogSpells(message, ...) do {\
} while (0)

#define LogSpellsDetail(message, ...) do {\
} while (0)

#define LogTCPConnection(message, ...) do {\
} while (0)

#define LogTCPConnectionDetail(message, ...) do {\
} while (0)

#define LogTasks(message, ...) do {\
} while (0)

#define LogTasksDetail(message, ...) do {\
} while (0)

#define LogTradeskills(message, ...) do {\
} while (0)

#define LogTradeskillsDetail(message, ...) do {\
} while (0)

#define LogTrading(message, ...) do {\
} while (0)

#define LogTradingDetail(message, ...) do {\
} while (0)

#define LogMySQLError(message, ...) do {\
} while (0)

#define LogMySQLErrorDetail(message, ...) do {\
} while (0)

#define LogMySQLQuery(message, ...) do {\
} while (0)

#define LogMySQLQueryDetail(message, ...) do {\
} while (0)

#define LogEQMac(message, ...) do {\
} while (0)

#define LogEQMacDetail(message, ...) do {\
} while (0)

#define LogQuestDebug(message, ...) do {\
} while (0)

#define LogQuestDebugDetail(message, ...) do {\
} while (0)

#define LogLoginserver(message, ...) do {\
} while (0)

#define LogLoginserverDetail(message, ...) do {\
} while (0)

#define LogTraps(message, ...) do {\
} while (0)

#define LogTrapsDetail(message, ...) do {\
} while (0)

#define LogStatus(message, ...) do {\
} while (0)

#define LogStatusDetail(message, ...) do {\
} while (0)

#define LogZoneServer(message, ...) do {\
} while (0)

#define LogZoneServerDetail(message, ...) do {\
} while (0)

#define LogMaps(message, ...) do {\
} while (0)

#define LogMapsDetail(message, ...) do {\
} while (0)

#define LogFaction(message, ...) do {\
} while (0)

#define LogFactionDetail(message, ...) do {\
} while (0)

#define LogCharacter(message, ...) do {\
} while (0)

#define LogCharacterDetail(message, ...) do {\
} while (0)

#define LogGroup(message, ...) do {\
} while (0)

#define LogGroupDetail(message, ...) do {\
} while (0)

#define LogCorpse(message, ...) do {\
} while (0)

#define LogCorpseDetail(message, ...) do {\
} while (0)

#define LogBazaar(message, ...) do {\
} while (0)

#define LogBazaarDetail(message, ...) do {\
} while (0)

#define LogDisc(message, ...) do {\
} while (0)

#define LogDiscDetail(message, ...) do {\
} while (0)

#define LogBoats(message, ...) do {\
} while (0)

#define LogBoatsDetail(message, ...) do {\
} while (0)

#define LogPTimers(message, ...) do {\
} while (0)

#define LogPTimersDetail(message, ...) do {\
} while (0)

#define LogNexus(message, ...) do {\
} while (0)

#define LogNexusDetail(message, ...) do {\
} while (0)

#define LogPets(message, ...) do {\
} while (0)

#define LogPetsDetail(message, ...) do {\
} while (0)

#define LogRegen(message, ...) do {\
} while (0)

#define LogRegenDetail(message, ...) do {\
} while (0)

#define LogFocus(message, ...) do {\
} while (0)

#define LogFoscusDetail(message, ...) do {\
} while (0)

#define LogDeath(message, ...) do {\
} while (0)

#define LogDeathDetail(message, ...) do {\
} while (0)

#define LogQuestErrors(message, ...) do {\
} while (0)

#define LogQuestErrorsDetail(message, ...) do {\
} while (0)
	
#define LogScheduler(message, ...) do {\
} while (0)

#define LogSchedulerDetail(message, ...) do {\
} while (0)

#define LogLoot(message, ...) do {\
} while (0)

#define LogLootDetail(message, ...) do {\
} while (0)

#define LogFixZ(message, ...) do {\
} while (0)

#define LogFixZDetail(message, ...) do {\
} while (0)

#define Log(debug_level, log_category, message, ...) do {\
} while (0)

#define LogF(debug_level, log_category, message, ...) do {\
} while (0)
#endif //EQEMU_EQEMU_LOGSYS_LOG_ALIASES_H