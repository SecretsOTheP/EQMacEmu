/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2009 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/opcodemgr.h"
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <math.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <limits.h>

#ifdef _WINDOWS
	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#else
	#include <pthread.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <unistd.h>
#endif

#include "../common/crc32.h"
#include "../common/data_verification.h"
#include "../common/faction.h"
#include "../common/guilds.h"
#include "../common/packet_dump_file.h"
#include "../common/rdtsc.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/strings.h"
#include "../common/zone_numbers.h"
#include "../common/skill_caps.h"
#include "data_bucket.h"
#include "event_codes.h"
#include "guild_mgr.h"
#include "mob.h"
#include "petitions.h"
#include "pets.h"
#include "queryserv.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "titles.h"
#include "water_map.h"
#include "worldserver.h"
#include "zone.h"
#include "../common/misc.h"
#include "position.h"
#include "mob_movement_manager.h"
#include "../common/repositories/account_repository.h"

#include <limits.h>

extern QueryServ* QServ;
extern Zone* zone;
extern volatile bool is_zone_loaded;
extern WorldServer worldserver;
extern PetitionList petition_list;
extern EntityList entity_list;

typedef void (Client::*ClientPacketProc)(const EQApplicationPacket *app);

//Use a map for connecting opcodes since it doesn't get used a lot and is sparse
std::map<uint32, ClientPacketProc> ConnectingOpcodes;
//Use a static array for connected, for speed
ClientPacketProc ConnectedOpcodes[_maxEmuOpcode];

void MapOpcodes()
{
	ConnectingOpcodes.clear();
	memset(ConnectedOpcodes, 0, sizeof(ConnectedOpcodes));

	// Now put all the opcodes into their home...
	// connecting opcode handler assignments:
	ConnectingOpcodes[OP_BazaarSearch] = &Client::Handle_OP_BazaarSearchCon;
	ConnectingOpcodes[OP_ClientError] = &Client::Handle_Connect_OP_ClientError;
	ConnectingOpcodes[OP_ClientUpdate] = &Client::Handle_Connect_OP_ClientUpdate;
	ConnectingOpcodes[OP_DataRate] = &Client::Handle_Connect_OP_SetDataRate;
	ConnectingOpcodes[OP_PetitionRefresh] = &Client::Handle_OP_PetitionRefresh;
	ConnectingOpcodes[OP_ReqClientSpawn] = &Client::Handle_Connect_OP_ReqClientSpawn;
	ConnectingOpcodes[OP_ReqNewZone] = &Client::Handle_Connect_OP_ReqNewZone;
	ConnectingOpcodes[OP_SendExpZonein] = &Client::Handle_Connect_OP_SendExpZonein;
	ConnectingOpcodes[OP_SetGuildMOTD] = &Client::Handle_OP_SetGuildMOTDCon;
	ConnectingOpcodes[OP_SetServerFilter] = &Client::Handle_Connect_OP_SetServerFilter;
	ConnectingOpcodes[OP_SpawnAppearance] = &Client::Handle_Connect_OP_SpawnAppearance;
	ConnectingOpcodes[OP_TGB] = &Client::Handle_Connect_OP_TGB;
	ConnectingOpcodes[OP_WearChange] = &Client::Handle_Connect_OP_WearChange;
	ConnectingOpcodes[OP_ZoneEntry] = &Client::Handle_Connect_OP_ZoneEntry;
	ConnectingOpcodes[OP_LFGCommand] = &Client::Handle_OP_LFGCommand;
	ConnectingOpcodes[OP_TargetMouse] = &Client::Handle_Connect_OP_TargetMouse;

	//temporary hack
	ConnectingOpcodes[OP_GetGuildsList] = &Client::Handle_OP_GetGuildsList;
	ConnectingOpcodes[OP_MoveItem] = &Client::Handle_OP_MoveItem;

	// connected opcode handler assignments:
	ConnectedOpcodes[OP_AAAction] = &Client::Handle_OP_AAAction;
	ConnectedOpcodes[OP_Animation] = &Client::Handle_OP_Animation;
	ConnectedOpcodes[OP_ApplyPoison] = &Client::Handle_OP_ApplyPoison;
	ConnectedOpcodes[OP_Assist] = &Client::Handle_OP_Assist;
	ConnectedOpcodes[OP_AutoAttack] = &Client::Handle_OP_AutoAttack;
	ConnectedOpcodes[OP_AutoAttack2] = &Client::Handle_OP_AutoAttack2;
	ConnectedOpcodes[OP_BazaarSearch] = &Client::Handle_OP_BazaarSearch;
	ConnectedOpcodes[OP_Begging] = &Client::Handle_OP_Begging;
	ConnectedOpcodes[OP_Bind_Wound] = &Client::Handle_OP_Bind_Wound;
	ConnectedOpcodes[OP_BoardBoat] = &Client::Handle_OP_BoardBoat;
	ConnectedOpcodes[OP_Buff] = &Client::Handle_OP_Buff;
	ConnectedOpcodes[OP_Bug] = &Client::Handle_OP_Bug;
	ConnectedOpcodes[OP_Camp] = &Client::Handle_OP_Camp;
	ConnectedOpcodes[OP_CancelTrade] = &Client::Handle_OP_CancelTrade;
	ConnectedOpcodes[OP_CastSpell] = &Client::Handle_OP_CastSpell;
	ConnectedOpcodes[OP_ChannelMessage] = &Client::Handle_OP_ChannelMessage;
	ConnectedOpcodes[OP_ClickDoor] = &Client::Handle_OP_ClickDoor;
	ConnectedOpcodes[OP_ClickObject] = &Client::Handle_OP_ClickObject;
	ConnectedOpcodes[OP_ClickObjectAction] = &Client::Handle_OP_ClickObjectAction;
	ConnectedOpcodes[OP_ClientError] = &Client::Handle_OP_ClientError;
	ConnectedOpcodes[OP_ClientUpdate] = &Client::Handle_OP_ClientUpdate;
	ConnectedOpcodes[OP_CombatAbility] = &Client::Handle_OP_CombatAbility;
	ConnectedOpcodes[OP_Consent] = &Client::Handle_OP_Consent;
	ConnectedOpcodes[OP_Consider] = &Client::Handle_OP_Consider;
	ConnectedOpcodes[OP_ConsiderCorpse] = &Client::Handle_OP_ConsiderCorpse;
	ConnectedOpcodes[OP_Consume] = &Client::Handle_OP_Consume;
	ConnectedOpcodes[OP_ControlBoat] = &Client::Handle_OP_ControlBoat;
	ConnectedOpcodes[OP_CorpseDrag] = &Client::Handle_OP_CorpseDrag;
	ConnectedOpcodes[OP_CorpseDrop] = &Client::Handle_OP_CorpseDrop;
	ConnectedOpcodes[OP_Damage] = &Client::Handle_OP_Damage;
	ConnectedOpcodes[OP_Death] = &Client::Handle_OP_Death;
	ConnectedOpcodes[OP_DeleteCharge] = &Client::Handle_OP_DeleteCharge;
	ConnectedOpcodes[OP_DeleteSpawn] = &Client::Handle_OP_DeleteSpawn;
	ConnectedOpcodes[OP_DeleteSpell] = &Client::Handle_OP_DeleteSpell;
	ConnectedOpcodes[OP_DisarmTraps] = &Client::Handle_OP_DisarmTraps;
	ConnectedOpcodes[OP_Discipline] = &Client::Handle_OP_Discipline;
	ConnectedOpcodes[OP_DuelResponse] = &Client::Handle_OP_DuelResponse;
	ConnectedOpcodes[OP_DuelResponse2] = &Client::Handle_OP_DuelResponse2;
	ConnectedOpcodes[OP_Emote] = &Client::Handle_OP_Emote;
	ConnectedOpcodes[OP_EndLootRequest] = &Client::Handle_OP_EndLootRequest;
	ConnectedOpcodes[OP_EnvDamage] = &Client::Handle_OP_EnvDamage;
	ConnectedOpcodes[OP_FaceChange] = &Client::Handle_OP_FaceChange;
	ConnectedOpcodes[OP_FeignDeath] = &Client::Handle_OP_FeignDeath;
	ConnectedOpcodes[OP_Fishing] = &Client::Handle_OP_Fishing;
	ConnectedOpcodes[OP_Forage] = &Client::Handle_OP_Forage;
	ConnectedOpcodes[OP_FriendsWho] = &Client::Handle_OP_FriendsWho;
	ConnectedOpcodes[OP_GetGuildsList] = &Client::Handle_OP_GetGuildsList;
	ConnectedOpcodes[OP_GMBecomeNPC] = &Client::Handle_OP_GMBecomeNPC;
	ConnectedOpcodes[OP_GMDelCorpse] = &Client::Handle_OP_GMDelCorpse;
	ConnectedOpcodes[OP_GMEmoteZone] = &Client::Handle_OP_GMEmoteZone;
	ConnectedOpcodes[OP_GMEndTraining] = &Client::Handle_OP_GMEndTraining;
	ConnectedOpcodes[OP_GMFind] = &Client::Handle_OP_GMFind;
	ConnectedOpcodes[OP_GMGoto] = &Client::Handle_OP_GMGoto;
	ConnectedOpcodes[OP_GMHideMe] = &Client::Handle_OP_GMHideMe;
	ConnectedOpcodes[OP_GMKick] = &Client::Handle_OP_GMKick;
	ConnectedOpcodes[OP_GMKill] = &Client::Handle_OP_GMKill;
	ConnectedOpcodes[OP_GMLastName] = &Client::Handle_OP_GMLastName;
	ConnectedOpcodes[OP_GMNameChange] = &Client::Handle_OP_GMNameChange;
	ConnectedOpcodes[OP_GMSearchCorpse] = &Client::Handle_OP_GMSearchCorpse;
	ConnectedOpcodes[OP_GMServers] = &Client::Handle_OP_GMServers;
	ConnectedOpcodes[OP_GMSummon] = &Client::Handle_OP_GMSummon;
	ConnectedOpcodes[OP_GMToggle] = &Client::Handle_OP_GMToggle;
	ConnectedOpcodes[OP_GMTraining] = &Client::Handle_OP_GMTraining;
	ConnectedOpcodes[OP_GMTrainSkill] = &Client::Handle_OP_GMTrainSkill;
	ConnectedOpcodes[OP_GMZoneRequest] = &Client::Handle_OP_GMZoneRequest;
	ConnectedOpcodes[OP_GMZoneRequest2] = &Client::Handle_OP_GMZoneRequest2;
	ConnectedOpcodes[OP_GroundSpawn] = &Client::Handle_OP_CreateObject;
	ConnectedOpcodes[OP_GroupCancelInvite] = &Client::Handle_OP_GroupCancelInvite;
	ConnectedOpcodes[OP_GroupDisband] = &Client::Handle_OP_GroupDisband;
	ConnectedOpcodes[OP_GroupFollow] = &Client::Handle_OP_GroupFollow;
	ConnectedOpcodes[OP_GroupInvite] = &Client::Handle_OP_GroupInvite;
	ConnectedOpcodes[OP_GroupInvite2] = &Client::Handle_OP_GroupInvite2;
	ConnectedOpcodes[OP_GroupUpdate] = &Client::Handle_OP_GroupUpdate;
	ConnectedOpcodes[OP_GuildDelete] = &Client::Handle_OP_GuildDelete;
	ConnectedOpcodes[OP_GuildInvite] = &Client::Handle_OP_GuildInvite;
	ConnectedOpcodes[OP_GuildInviteAccept] = &Client::Handle_OP_GuildInviteAccept;
	ConnectedOpcodes[OP_GuildLeader] = &Client::Handle_OP_GuildLeader;
	ConnectedOpcodes[OP_GuildPeace] = &Client::Handle_OP_GuildPeace;
	ConnectedOpcodes[OP_GuildRemove] = &Client::Handle_OP_GuildRemove;
	ConnectedOpcodes[OP_GuildWar] = &Client::Handle_OP_GuildWar;
	ConnectedOpcodes[OP_Hide] = &Client::Handle_OP_Hide;
	ConnectedOpcodes[OP_Illusion] = &Client::Handle_OP_Illusion;
	ConnectedOpcodes[OP_InspectAnswer] = &Client::Handle_OP_InspectAnswer;
	ConnectedOpcodes[OP_InspectRequest] = &Client::Handle_OP_InspectRequest;
	ConnectedOpcodes[OP_InstillDoubt] = &Client::Handle_OP_InstillDoubt;
	ConnectedOpcodes[OP_ItemLinkResponse] = &Client::Handle_OP_ItemLinkResponse;
	ConnectedOpcodes[OP_Jump] = &Client::Handle_OP_Jump;
	ConnectedOpcodes[OP_LeaveBoat] = &Client::Handle_OP_LeaveBoat;
	ConnectedOpcodes[OP_Logout] = &Client::Handle_OP_Logout;
	ConnectedOpcodes[OP_LootItem] = &Client::Handle_OP_LootItem;
	ConnectedOpcodes[OP_LootRequest] = &Client::Handle_OP_LootRequest;
	ConnectedOpcodes[OP_ManaChange] = &Client::Handle_OP_ManaChange;
	ConnectedOpcodes[OP_MemorizeSpell] = &Client::Handle_OP_MemorizeSpell;
	ConnectedOpcodes[OP_Mend] = &Client::Handle_OP_Mend;
	ConnectedOpcodes[OP_MoveCoin] = &Client::Handle_OP_MoveCoin;
	ConnectedOpcodes[OP_MoveItem] = &Client::Handle_OP_MoveItem;
	ConnectedOpcodes[OP_PetCommands] = &Client::Handle_OP_PetCommands;
	ConnectedOpcodes[OP_Petition] = &Client::Handle_OP_Petition;
	ConnectedOpcodes[OP_PetitionCheckIn] = &Client::Handle_OP_PetitionCheckIn;
	ConnectedOpcodes[OP_PetitionCheckout] = &Client::Handle_OP_PetitionCheckout;
	ConnectedOpcodes[OP_PetitionDelete] = &Client::Handle_OP_PetitionDelete;
	ConnectedOpcodes[OP_PetitionRefresh] = &Client::Handle_OP_PetitionRefresh;
	ConnectedOpcodes[OP_PickPocket] = &Client::Handle_OP_PickPocket;
	ConnectedOpcodes[OP_RaidInvite] = &Client::Handle_OP_RaidCommand;
	ConnectedOpcodes[OP_RandomReq] = &Client::Handle_OP_RandomReq;
	ConnectedOpcodes[OP_ReadBook] = &Client::Handle_OP_ReadBook;
	ConnectedOpcodes[OP_Report] = &Client::Handle_OP_Report;
	ConnectedOpcodes[OP_RequestDuel] = &Client::Handle_OP_RequestDuel;
	ConnectedOpcodes[OP_RezzAnswer] = &Client::Handle_OP_RezzAnswer;
	ConnectedOpcodes[OP_Sacrifice] = &Client::Handle_OP_Sacrifice;
	ConnectedOpcodes[OP_SafeFallSuccess] = &Client::Handle_OP_SafeFallSuccess;
	ConnectedOpcodes[OP_SafePoint] = &Client::Handle_OP_SafePoint;
	ConnectedOpcodes[OP_Save] = &Client::Handle_OP_Save;
	ConnectedOpcodes[OP_SaveOnZoneReq] = &Client::Handle_OP_SaveOnZoneReq;
	ConnectedOpcodes[OP_SenseHeading] = &Client::Handle_OP_SenseHeading;
	ConnectedOpcodes[OP_SenseTraps] = &Client::Handle_OP_SenseTraps;
	ConnectedOpcodes[OP_SetGuildMOTD] = &Client::Handle_OP_SetGuildMOTD;
	ConnectedOpcodes[OP_SetRunMode] = &Client::Handle_OP_SetRunMode;
	ConnectedOpcodes[OP_SetServerFilter] = &Client::Handle_OP_SetServerFilter;
	ConnectedOpcodes[OP_SetTitle] = &Client::Handle_OP_SetTitle;
	ConnectedOpcodes[OP_Shielding] = &Client::Handle_OP_Shielding;
	ConnectedOpcodes[OP_ShopEnd] = &Client::Handle_OP_ShopEnd;
	ConnectedOpcodes[OP_ShopPlayerBuy] = &Client::Handle_OP_ShopPlayerBuy;
	ConnectedOpcodes[OP_ShopPlayerSell] = &Client::Handle_OP_ShopPlayerSell;
	ConnectedOpcodes[OP_ShopRequest] = &Client::Handle_OP_ShopRequest;
	ConnectedOpcodes[OP_Sneak] = &Client::Handle_OP_Sneak;
	ConnectedOpcodes[OP_SpawnAppearance] = &Client::Handle_OP_SpawnAppearance;
	ConnectedOpcodes[OP_Split] = &Client::Handle_OP_Split;
	ConnectedOpcodes[OP_Surname] = &Client::Handle_OP_Surname;
	ConnectedOpcodes[OP_SwapSpell] = &Client::Handle_OP_SwapSpell;
	ConnectedOpcodes[OP_TargetCommand] = &Client::Handle_OP_TargetCommand;
	ConnectedOpcodes[OP_TargetMouse] = &Client::Handle_OP_TargetMouse;
	ConnectedOpcodes[OP_Taunt] = &Client::Handle_OP_Taunt;
	ConnectedOpcodes[OP_TGB] = &Client::Handle_OP_TGB;
	ConnectedOpcodes[OP_Track] = &Client::Handle_OP_Track;
	ConnectedOpcodes[OP_TradeAcceptClick] = &Client::Handle_OP_TradeAcceptClick;
	ConnectedOpcodes[OP_Trader] = &Client::Handle_OP_Trader;
	ConnectedOpcodes[OP_TraderBuy] = &Client::Handle_OP_TraderBuy;
	ConnectedOpcodes[OP_TradeRequest] = &Client::Handle_OP_TradeRequest;
	ConnectedOpcodes[OP_TradeRequestAck] = &Client::Handle_OP_TradeRequestAck;
	ConnectedOpcodes[OP_TraderShop] = &Client::Handle_OP_TraderShop;
	ConnectedOpcodes[OP_TradeSkillCombine] = &Client::Handle_OP_TradeSkillCombine;
	ConnectedOpcodes[OP_Translocate] = &Client::Handle_OP_Translocate;
	ConnectedOpcodes[OP_WearChange] = &Client::Handle_OP_WearChange;
	ConnectedOpcodes[OP_WhoAllRequest] = &Client::Handle_OP_WhoAllRequest;
	ConnectedOpcodes[OP_YellForHelp] = &Client::Handle_OP_YellForHelp;
	ConnectedOpcodes[OP_ZoneChange] = &Client::Handle_OP_ZoneChange;
	ConnectedOpcodes[OP_ZoneEntryResend] = &Client::Handle_OP_ZoneEntryResend;
	ConnectedOpcodes[OP_LFGCommand] = &Client::Handle_OP_LFGCommand;
	ConnectedOpcodes[OP_Disarm] = &Client::Handle_OP_Disarm;
	ConnectedOpcodes[OP_Feedback] = &Client::Handle_OP_Feedback;
	ConnectedOpcodes[OP_SoulMarkUpdate] = &Client::Handle_OP_SoulMarkUpdate;
	ConnectedOpcodes[OP_SoulMarkList] = &Client::Handle_OP_SoulMarkList;
	ConnectedOpcodes[OP_SoulMarkAdd] = &Client::Handle_OP_SoulMarkAdd;
	ConnectedOpcodes[OP_MBRetrievalRequest] = &Client::Handle_OP_MBRetrievalRequest;
	ConnectedOpcodes[OP_MBRetrievalDetailRequest] = &Client::Handle_OP_MBRetrievalDetailRequest;
	ConnectedOpcodes[OP_MBRetrievalPostRequest] = &Client::Handle_OP_MBRetrievalPostRequest;
	ConnectedOpcodes[OP_MBRetrievalEraseRequest] = &Client::Handle_OP_MBRetrievalEraseRequest;
	ConnectedOpcodes[OP_Key] = &Client::Handle_OP_Key;
	ConnectedOpcodes[OP_TradeRefused] = &Client::Handle_OP_TradeRefused;
	ConnectedOpcodes[OP_SpellTextMessage] = &Client::Handle_OP_SpellTextMessage;
}

void ClearMappedOpcode(EmuOpcode op)
{
	if (op >= _maxEmuOpcode)
		return;

	ConnectedOpcodes[op] = nullptr;
	auto iter = ConnectingOpcodes.find(op);
	if (iter != ConnectingOpcodes.end()) {
		ConnectingOpcodes.erase(iter);
	}
}

// client methods
int Client::HandlePacket(const EQApplicationPacket *app)
{
	LogPacketClientServer(
		"[{}] [{:#06x}] Size [{}] {}",
		OpcodeManager::EmuToName(app->GetOpcode()),
		eqs->GetOpcodeManager()->EmuToEQ(app->GetOpcode()),
		app->Size(),
		(LogSys.IsLogEnabled(Logs::Detail, Logs::PacketClientServer) ? DumpPacketToString(app) : "")
	);
	
	EmuOpcode opcode = app->GetOpcode();

	#if EQDEBUG >= 11
		std::cout << "Received 0x" << std::hex << std::setw(4) << std::setfill('0') << opcode << ", size=" << std::dec << app->size << std::endl;
	#endif

	#ifdef SOLAR
		if(0 && opcode != OP_ClientUpdate)
		{
			Log.LogDebug(Logs::General,"HandlePacket() OPCODE debug enabled client %s", GetName());
			std::cerr << "OPCODE: " << std::hex << std::setw(4) << std::setfill('0') << opcode << std::dec << ", size: " << app->size << std::endl;
			DumpPacket(app);
		}
	#endif

	switch(client_state) {
	case CLIENT_AUTH_RECEIVED:
	case CLIENT_CONNECTING: {
		if(ConnectingOpcodes.count(opcode) != 1) {
			//Hate const cast but everything in lua needs to be non-const even if i make it non-mutable
			std::vector<std::any> args;
			args.push_back(const_cast<EQApplicationPacket*>(app));
			parse->EventPlayer(EVENT_UNHANDLED_OPCODE, this, "", 1, &args);

			break;
		}

		ClientPacketProc p;
		p = ConnectingOpcodes[opcode];

		//call the processing routine
		(this->*p)(app);

		//special case where connecting code needs to boot client...
		if(client_state == CLIENT_KICKED) {
			return(false);
		}

		break;
	}
	case CLIENT_CONNECTED: {
		ClientPacketProc p;
		p = ConnectedOpcodes[opcode];
		if(p == nullptr) { 
			std::vector<std::any> args;
			args.push_back(const_cast<EQApplicationPacket*>(app));
			parse->EventPlayer(EVENT_UNHANDLED_OPCODE, this, "", 0, &args);

			break;
		}

		//call the processing routine
		(this->*p)(app);
		break;
	}
	case CLIENT_KICKED:
	case DISCONNECTED:
	case CLIENT_LINKDEAD:
	case PREDISCONNECTED:
	case ZONING:
	case CLIENT_WAITING_FOR_AUTH:
		break;
	default:
		LogDebug("Unknown client_state: [{}]\n", static_cast<int>(client_state));
		break;
	}

	return(true);
}

// Finish client connecting state
void Client::CompleteConnect()
{
	UpdateWho();
	client_state = CLIENT_CONNECTED;

	hpupdate_timer.Start();
	position_timer.Start();
	autosave_timer.Start();
	SetDuelTarget(0);
	SetDueling(false);

	ZoneFlags.Clear();
	LoadZoneFlags(&ZoneFlags);
	consent_list.clear();
	LoadCharacterConsent();

	/* Sets GM Flag if needed & Sends Petition Queue */
	UpdateAdmin(false);
	SendAllPackets();
	for (uint32 spellInt = 0; spellInt < MAX_PP_SPELLBOOK; spellInt++)
	{
		if (m_pp.spell_book[spellInt] < 3 || m_pp.spell_book[spellInt] > 50000) {
			m_pp.spell_book[spellInt] = 0xFFFF;
		}
	}

	if (GetGM())
	{
		bool levitating = flymode == GravityBehavior::Levitating || FindType(SE_Levitate);
		if (GetGM() || GetHideMe() || GetGMSpeed() || GetGMInvul() || (levitating || flymode != GravityBehavior::Ground) || tellsoff)
		{
			std::vector<std::string> state;

			if (GetHideMe()) {
				state.emplace_back("hidden to all clients");
			}

			if (GetGMSpeed()) {
				state.emplace_back("running at GM speed");
			}

			if (GetGMInvul()) {
				state.emplace_back("invulnerable to all damage");
			}

			if (flymode == GravityBehavior::Flying) {
				state.emplace_back("flying");
			}
			else if (flymode == GravityBehavior::Levitating) {
				state.emplace_back("levitating");
			}

			if (tellsoff) {
				state.emplace_back("ignoring tells");
			}

			if (!state.empty()) {
				Message(Chat::Red, "[GM] You are currently %s.", Strings::Join(state, ", ").c_str());
			}
		}
	}

	/* Send Raid Members via Packet*/
	uint32 raidid = database.GetRaidID(GetName());
	Raid* raid = nullptr;
	if (raidid > 0) {
		raid = entity_list.GetRaidByID(raidid);
		bool loaded_raid = false;
		if (!raid) {
			raid = new Raid(raidid);
			if (raid->GetID() != 0) {
				entity_list.AddRaid(raid, raidid);
			}
			else {
				raid = nullptr;
			}

			if (raid) {
				loaded_raid = true;
				raid->LearnMembers();
				raid->VerifyRaid();
				raid->GetRaidDetails();
			}
		}

		if (raid) {
			if (!loaded_raid) {
				raid->LearnMembers();
				raid->VerifyRaid();
				raid->GetRaidDetails();
			}

			int gid = raid->GetGroup(GetName());
			if (gid != 0xFFFFFFFF)
			{
				raid->SendGroupLeader(gid, this);
			}
			raid->SendRaidMembers(this);
		}
	}
	if (!raid) {
		auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
		RaidGeneral_Struct* rg = (RaidGeneral_Struct*)outapp->pBuffer;
		rg->action = RaidCommandSendDisband;
		strn0cpy(rg->leader_name, GetName(), 64);
		strn0cpy(rg->player_name, GetName(), 64);
		rg->parameter = 0;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	//reapply some buffs - pass 2
	uint32 buff_count = GetMaxTotalSlots();
	for (uint32 j1 = 0; j1 < buff_count; j1++) {
		if (!IsValidSpell(buffs[j1].spellid)) {
			continue;
		}

		uint16 spell_id = buffs[j1].spellid;
		const SPDat_Spell_Struct& spell = spells[spell_id];

		for (int x1 = 0; x1 < EFFECT_COUNT; x1++)
		{
			switch (spell.effectid[x1])
			{
			case SE_IllusionCopy:
			case SE_Illusion:
			{
				ApplyIllusion(spell, x1, this);
				break;
			}
			case SE_SummonHorse:
			{
				SummonHorse(buffs[j1].spellid);
				//hasmount = true;	//this was false, is that the correct thing?
				break;
			}
			case SE_Levitate:
			{
				if (!zone->CanLevitate())
				{
					if (!GetGM())
					{
						SendAppearancePacket(AppearanceType::FlyMode, 0);
						BuffFadeByEffect(SE_Levitate);
						Message(Chat::Red, "You can't levitate in this zone.");
					}
				}
				else {
					SendAppearancePacket(AppearanceType::FlyMode, 2);
				}
				break;
			}
			case SE_InvisVsUndead:
			{
				if (!hidden && !improved_hidden) {
					SendAppearancePacket(AppearanceType::Invisibility, 3);
				}
				invisible_undead = true;
				break;
			}
			case SE_InvisVsAnimals:
			{
				if (!hidden && !improved_hidden) {
					SendAppearancePacket(AppearanceType::Invisibility, 2);
				}
				invisible_animals = true;
				break;
			}
			case SE_WeaponProc:
			{
				AddProcToWeapon(GetProcID(buffs[j1].spellid, x1), 100 + spells[buffs[j1].spellid].base2[x1], buffs[j1].spellid);
				break;
			}
			case SE_VoiceGraft:
			{
				if (!GetPet())
				{
					FadeVoiceGraft();
				}
				break;
			}
			case SE_Familiar:
			{
				if (!GetPet())
				{
					MakePet(spell_id, spell.teleport_zone);
				}
				break;
			}
			}
		}
	}
	if (RuleB(Quarm, PetZoneWithOwner))
	{
		if (m_petinfo.SpellID > 1 && !GetPet() && m_petinfo.SpellID <= SPDAT_RECORDS) {
			MakePoweredPet(m_petinfo.SpellID, spells[m_petinfo.SpellID].teleport_zone, m_petinfo.petpower,
				m_petinfo.Name, m_petinfo.size);
			if (GetPet() && GetPet()->IsNPC()) {
				NPC* pet = GetPet()->CastToNPC();
				pet->SetPetState(m_petinfo.Buffs, m_petinfo.Items);
				pet->CalcBonuses();
				pet->SetHP(m_petinfo.HP);
				pet->SetMana(m_petinfo.Mana);
			}
			m_petinfo.SpellID = 0;
		}
	}

	// Disciplines don't survive zoning.
	if (GetActiveDisc() != 0) {
		FadeDisc();
	}

	// AA effects don't survive zoning.
	FadeAllAAEffects();

	/* Sends appearances for all mobs not doing anim_stand aka sitting, looting, playing dead */
	// entity_list.SendZoneAppearance(this);

	client_data_loaded = true;

	UpdateActiveLight();
	SendAppearancePacket(AppearanceType::Light, GetActiveLightType());

	Mob *pet = GetPet();
	if (pet != nullptr) {
		for (int cur_slot = EQ::textures::textureBegin; cur_slot <= EQ::textures::LastTexture; cur_slot++) {
			pet->SendWearChange(cur_slot, nullptr, true);
		}
		// added due to wear change above
		pet->UpdateActiveLight();
		pet->SendAppearancePacket(AppearanceType::Light, pet->GetActiveLightType());
	}

	if (ShowHelm())
	{
		entity_list.SendHelms(this);
	}
	else
	{
		entity_list.HideHelms(this);
		WearChange(EQ::textures::armorHead, 0, 0, this);
	}

	if (GetEquipmentMaterial(EQ::textures::armorHead) != 0)
	{
		entity_list.HideMyHelm(this);
	}
	entity_list.SendTraders(this);
	entity_list.SendIllusionedPlayers(this);

	conn_state = ClientConnectFinished;
	initial_z_position = m_Position.z;
	accidentalfall_timer.Enable();
	accidentalfall_timer.Start(RuleI(Quarm, AccidentalFallTimerMS));
	database.SetAccountActive(AccountID());

	if (GetGroup())
	{
		database.RefreshGroupLeaderFromDB(this);
	}
	else if (!IsRaidGrouped()) {
		auto outapp = new EQApplicationPacket(OP_GroupFollow, 0);
		QueuePacket(outapp);
		safe_delete(outapp);
	}

	SendGuildSpawnAppearance();

	if (zone) {
		zone->weatherSend();
	}

	TotalKarma = database.GetKarma(AccountID());

	parse->EventPlayer(EVENT_ENTER_ZONE, this, "", 0);

	/* This sub event is for if a player logs in for the first time since entering world. */
	if (firstlogon == 1){
		parse->EventPlayer(EVENT_CONNECT, this, "", 0);
		/* QS: PlayerLogConnectDisconnect */
		if (RuleB(QueryServ, PlayerLogConnectDisconnect)){
			std::string event_desc = StringFormat("Connect :: Logged into zoneid:%i", this->GetZoneID());
			QServ->PlayerLogEvent(Player_Log_Connect_State, this->CharacterID(), event_desc);
		}

		/**
		* Update last login since this doesn't get updated until a late save later so we can update online status
		 */
		database.QueryDatabase(
			StringFormat(
				"UPDATE `character_data` SET `last_login` = UNIX_TIMESTAMP() WHERE id = %u",
				this->CharacterID()
			)
		);
	}

	if (IsInAGuild()) {
		guild_mgr.RequestOnlineGuildMembers(this->CharacterID(), this->GuildID());
	}

	stamina_timer.Reset();
	tic_timer.Reset();

	SendClientVersion();
	FixClientXP();
	SendToBoat(true);
	worldserver.RequestTellQueue(GetName());

	if (GetBaseRace() == IKSAR && IsMule() && RuleB(Quarm, RestrictIksarsToKunark) && zone)
	{
		Save();
		WorldKick();
		Disconnect();
		return;
	}

	if (m_dll_version < RuleI(Quarm, WarnDllVersionBelow) || m_old_feature_detected)
	{
		Message(Chat::Red, "You are running on an older version of the client. Please run the patcher or manually download the server files.");
	}

	//enforce some rules..
	if (!CanBeInZone()) {
		Log(Logs::Detail, Logs::Status, "[CLIENT] Kicking char from zone, not allowed here");
		if (RuleB(Quarm, RestrictIksarsToKunark) && zone)
		{
			// Mules by their very nature require access to at least Luclin. Set that here.
			if (IsMule() && GetBaseRace() != IKSAR)
			{
				if (RuleB(Quarm, EastCommonMules)) {
					DoZoneMove(ZoneID("ecommons"), GUILD_NONE, -164.0f, -1651.0f, 4.0f, 0.0f);
				}
				else {
					DoZoneMove(ZoneID("bazaar"), GUILD_NONE, 140.0f, -821.0f, 5.0f, 0.0f);
				}
			}
			else if (IsMule() && GetBaseRace() == IKSAR)
			{
				//Do nothing, just dc.
			}
			else if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() != KunarkEQ)
			{
				DoZoneMove(ZoneID("fieldofbone"), GUILD_NONE, 1617.0f, -1684.0f, -50.0f, 0.0f);
			}
			else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
			{
				DoZoneMove(ZoneID("ecommons"), GUILD_NONE, -164.0f, -1651.0f, 4.0f, 0.0f);
			}
		}
		else
		{

			if (m_pp.expansions & LuclinEQ)
			{
				if (RuleB(Quarm, EastCommonMules)) {
					DoZoneMove(ZoneID("ecommons"), GUILD_NONE, -164.0f, -1651.0f, 4.0f, 0.0f);

				}
				else {
					DoZoneMove(ZoneID("bazaar"), GUILD_NONE, 140.0f, -821.0f, 5.0f, 0.0f);
				}
			}
			else
			{
				// Mules by their very nature require access to at least Luclin. Set that here.
				if (IsMule())
				{
					if (RuleB(Quarm, EastCommonMules)) {
						DoZoneMove(ZoneID("ecommons"), GUILD_NONE, -164.0f, -1651.0f, 4.0f, 0.0f);
					}
					else {
						DoZoneMove(ZoneID("bazaar"), GUILD_NONE, 140.0f, -821.0f, 5.0f, 0.0f);
					}
				}
				else {
					DoZoneMove(ZoneID("ecommons"), GUILD_NONE, -164.0f, -1651.0f, 4.0f, 0.0f);
				}
			}
		}
		Disconnect();
		return;
	}
	else if (m_epp.hardcore_death_time > 0)
	{
		SetHardcoreDeathTimeStamp(0);
		ClearPlayerInfoAndGrantStartingItems();
		ForceGoToDeath();
	}

	// Begins the shared bank negotiation with the client. Server sends how many slots are possible, waits to hear what the client supports.
	// Upon client response, server will (optionally) enable the bank and stream the contents.
	int shared_bank_bags = RuleI(Quarm, SharedBankBags);
	if (shared_bank_bags >= 0)
	{
		auto* sb_init_app = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
		SpawnAppearance_Struct* sb_init = (SpawnAppearance_Struct*)sb_init_app->pBuffer;
		sb_init->spawn_id = 0;
		sb_init->type = AppearanceType::ClientDllMessage;
		sb_init->parameter = ((uint32)ClientFeature::SharedBankBagsSupported << 16) | (uint32)shared_bank_bags;
		sb_init_app->priority = 6;
		QueuePacket(sb_init_app);
		safe_delete(sb_init_app);
	}

	// Evac player to zonein during PVP quakes if they log in on first login in this zone.
	//if (firstlogon && zone && zone->GetGuildID() == GUILD_NONE && zone->last_quake_struct.quake_type != QuakeType::QuakeDisabled && zone->last_quake_struct.start_timestamp > 0 && prev_last_login_time > 0 && prev_last_login_time < zone->last_quake_struct.start_timestamp)
	//{
	//	MovePCGuildID(zone->GetZoneID(), GUILD_NONE, 0, 0, 0, 0, 0, EvacToSafeCoords);
	//}
	
	if (GetGM() && IsDevToolsEnabled()) {
		ShowDevToolsMenu();
	}

	// this was put in to appease concerns about the RNG being affected by the time of day or day of week the server was started on, resulting in bad loot
	// this adds some external entropy to the process of generating random numbers by discarding some numbers in the sequence when a client connects
	zone->random.Discard(Timer::GetCurrentTime() % 300);
}


void Client::CheatDetected(CheatTypes CheatType, float x, float y, float z)
{ 
	//ToDo: Break warp down for special zones. Some zones have special teleportation pads or bad .map files which can trigger the detector without a legit zone request.

	switch (CheatType)
	{
	case MQWarp: //Some zones may still have issues. Database updates will eliminate most if not all problems.
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			if (GetBoatNPCID() == 0)
			{
				Message(Chat::Red, "Large warp detected.");
				auto hString = fmt::format("/MQWarp with location {:.2f}, {:.2f}, {:.2f} ", GetX(), GetY(), GetZ());
				database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			}
		}
		break;
	case MQWarpShadowStep:
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			auto hString = fmt::format("/MQWarp(SS) with location {:.2f}, {:.2f}, {:.2f}, the target was shadow step exempt but we still found this suspicious.", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		}
		break;
	case MQWarpKnockBack:
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			auto hString = fmt::format("/MQWarp(KB) with location {:.2f}, {:.2f}, {:.2f}, the target was Knock Back exempt but we still found this suspicious.", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		}
		break;

	case MQWarpLight:
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			if (RuleB(Zone, MarkMQWarpLT))
			{
				auto hString = fmt::format("/MQWarp(LT) with location {:.2f}, {:.2f}, {:.2f}, running fast but not fast enough to get killed, possibly: small warp, speed hack, excessive lag, marked as suspicious.", GetX(), GetY(), GetZ());
				database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			}
		}
		break;

	case MQZone:
		if (RuleB(Zone, EnableMQZoneDetector) && ((this->Admin() < RuleI(Zone, MQZoneExemptStatus) || (RuleI(Zone, MQZoneExemptStatus)) == -1)))
		{
			char hString[250];
			sprintf(hString, "/MQZone used at %.2f, %.2f, %.2f to %.2f %.2f %.2f", GetX(), GetY(), GetZ(), x, y, z);
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		}
		break;
	case MQZoneUnknownDest:
		if (RuleB(Zone, EnableMQZoneDetector) && ((this->Admin() < RuleI(Zone, MQZoneExemptStatus) || (RuleI(Zone, MQZoneExemptStatus)) == -1)))
		{
			char hString[250];
			sprintf(hString, "/MQZone used at %.2f, %.2f, %.2f", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		}
		break;
	case MQGate:
		if (RuleB(Zone, EnableMQGateDetector) && ((this->Admin() < RuleI(Zone, MQGateExemptStatus) || (RuleI(Zone, MQGateExemptStatus)) == -1))) {
			Message(Chat::Red, "Illegal gate request.");
			char hString[250];
			sprintf(hString, "/MQGate style hack, zone: %s:%d, loc: %.2f, %.2f, %.2f", ZoneName(GetZoneID()), GetZoneID(), GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			this->SetZone(this->GetZoneID(), zone ? zone->GetGuildID() : GUILD_NONE); //Prevent the player from zoning, place him back in the zone where he tried to originally /gate.
		}
		break;
	case MQGhost: //Not currently implemented, but the framework is in place - just needs detection scenarios identified
		if (RuleB(Zone, EnableMQGhostDetector) && ((this->Admin() < RuleI(Zone, MQGhostExemptStatus) || (RuleI(Zone, MQGhostExemptStatus)) == -1))) {
			database.SetMQDetectionFlag(this->account_name, this->name, "/MQGhost", zone->GetShortName());
		}
		break;
	default:
		auto hString = fmt::format("Unhandled HackerDetection flag with location {:.2f}, {:.2f}, {:.2f}.", GetX(), GetY(), GetZ());
		database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		break;
	}
}

void Client::Handle_Connect_OP_ClientError(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClientError_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size on OP_ClientError: Expected %i, Got %i",
			sizeof(ClientError_Struct), app->size);
		return;
	}

	return;
}

void Client::Handle_Connect_OP_ClientUpdate(const EQApplicationPacket *app)
{
	conn_state = ClientReadyReceived;

	CompleteConnect();
	SendHPUpdate();
}

void Client::Handle_Connect_OP_ReqClientSpawn(const EQApplicationPacket *app)
{
	conn_state = ClientSpawnRequested;

	auto outapp = new EQApplicationPacket;
	if (entity_list.SendZoneDoorsBulk(outapp, this))
	{
		QueuePacket(outapp);
	}
	safe_delete(outapp);

	entity_list.SendZoneObjects(this);
	SendZonePoints();

	outapp = new EQApplicationPacket(OP_SendExpZonein, 0);
	FastQueuePacket(&outapp);

	conn_state = ZoneContentsSent;

	return;
}

void Client::Handle_Connect_OP_ReqNewZone(const EQApplicationPacket *app)
{
	conn_state = NewZoneRequested;

	/////////////////////////////////////
	// New Zone Packet
	auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
	NewZone_Struct* nz = (NewZone_Struct*)outapp->pBuffer;
	memcpy(outapp->pBuffer, &zone->newzone_data, sizeof(NewZone_Struct));
	strcpy(nz->char_name, m_pp.name);
	memset(nz->zone_short_name, 0, 32);
	memset(nz->zone_short_name2, 0, 68);
	strcpy(nz->zone_short_name, database.GetClientZoneName(zone->newzone_data.zone_short_name));
	strcpy(nz->zone_short_name2, database.GetClientZoneName(zone->newzone_data.zone_short_name2));
	nz->zone_id = database.GetClientZoneID(nz->zone_id);
	Log(Logs::Detail, Logs::ZoneServer, "NewZone data for %s (%i) successfully sent.", zone->newzone_data.zone_short_name, zone->newzone_data.zone_id);

	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_Connect_OP_SendExpZonein(const EQApplicationPacket *app)
{
	//////////////////////////////////////////////////////
	// Spawn Appearance Packet
	auto outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)outapp->pBuffer;
	sa->type = AppearanceType::SpawnID;			// Is 0x10 used to set the player id?
	sa->parameter = GetID();	// Four bytes for this parameter...
	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	// Inform the world about the client
	EQApplicationPacket out_app;

	CreateSpawnPacket(&out_app);
	out_app.priority = 6;
	if(GetHideMe())
		entity_list.QueueClientsStatus(this, &out_app, true, Admin(), 255);
	else
		entity_list.QueueClients(this, &out_app, true);
	safe_delete_array(out_app.pBuffer);

	SetSpawned();

	if (GetPVP() == 2 && zone && (zone->last_quake_struct.quake_type != QuakeType::QuakePVP && !zone->IsPVPZone()))
	{
		if (GetPVP() != 1)
			SetPVP(0);
	}
	if (GetPVP() == 0 && zone && zone->last_quake_struct.quake_type == QuakeType::QuakePVP || zone && zone->IsPVPZone())
	{
		if(GetPVP() != 1)
			SetPVP(2);
	}
	if (GetPVP())	//force a PVP update until we fix the spawn struct
		SendAppearancePacket(AppearanceType::PVP, GetPVP() > 0 ? 1 : 0, true, false);

	//Send AA Exp packet:
	if (GetLevel() >= 51)
	{
		SendAAStats();
	}

	// Send exp packets
	outapp = new EQApplicationPacket(OP_ExpUpdate, sizeof(ExpUpdate_Struct));
	ExpUpdate_Struct* eu = (ExpUpdate_Struct*)outapp->pBuffer;
	uint32 tmpxp1 = GetEXPForLevel(GetLevel() + 1);
	uint32 tmpxp2 = GetEXPForLevel(GetLevel());

	// Crash bug fix... Divide by zero when tmpxp1 and 2 equalled each other, most likely the error case from GetEXPForLevel() (invalid class, etc)
	if (tmpxp1 != tmpxp2 && tmpxp1 != 0xFFFFFFFF && tmpxp2 != 0xFFFFFFFF) {
		float tmpxp = (float)((float)m_pp.exp - tmpxp2) / ((float)tmpxp1 - tmpxp2);
		eu->exp = (uint32)(330.0f * tmpxp);
		outapp->priority = 6;
		QueuePacket(outapp);
	}
	safe_delete(outapp);

	if (GetLevel() >= 51)
	{
		SendAATimers();
	}

	outapp = new EQApplicationPacket(OP_SendExpZonein, 0);
	QueuePacket(outapp);
	safe_delete(outapp);

	outapp = new EQApplicationPacket(OP_ZoneInAvatarSet, 1);
	QueuePacket(outapp);
	safe_delete(outapp);

	SendCursorItems();

	return;
}

void Client::Handle_Connect_OP_SetDataRate(const EQApplicationPacket *app)
{
	// Normal packet sequence sends ZoneEntry next.  Sometimes client gets stuck and sends another Data Rate.
	// Go ahead and send OP_Weather back, if we have already sent over zone info.  This sometimes helps client continue.
	if (conn_state == ZoneInfoSent) {
		// resend weather packet
		Log(Logs::General, Logs::Error, "Received a second OP_DataRate after Zone Info Sent.  Resending OP_Weather to see if client recovers.");
		auto outapp = new EQApplicationPacket(OP_Weather, sizeof(Weather_Struct));
		Weather_Struct* ws = (Weather_Struct*)outapp->pBuffer;
		ws->type = 0;
		ws->intensity = 0;

		outapp->priority = 6;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	return;
}

void Client::Handle_Connect_OP_SetServerFilter(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SetServerFilter_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized OP_SetServerFilter");
		DumpPacket(app);
		return;
	}
	SetServerFilter_Struct* filter = (SetServerFilter_Struct*)app->pBuffer;
	ServerFilter(filter);
	return;
}

void Client::Handle_Connect_OP_SpawnAppearance(const EQApplicationPacket *app)
{
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)app->pBuffer;

	// We use these SpawnAppearance message to support basic-messaging for Zeal/EqGameDll features (using a custom SpawnAppearanceType)
	// This allows for doing 2-way handshake between the client<->server to negotiate feature flags that might need coordination on both sides.
	// Messages/Handshakes are supported in either direction (either side can initiate).
	if (sa && (sa->type == AppearanceType::ClientDllMessage || sa->type == AppearanceType::ClientZealMessage)) {

		bool is_request = (sa->parameter >> 31) == 0; // highest bit of SpawnAppearance->parameter marks if the message is a request (0) or response (1).
		bool send_response = is_request;
		uint32 feature_id = (sa->parameter >> 16) & 0x7FFFu; // feature ID is in the hi-word of SpawnAppearance->paramter.
		uint32 feature_value = sa->parameter & 0xFFFFu; // feature value is encoded in the lo-word of SpawnAppearance->parameter.

		switch (feature_id) {
		default: // Unknown message
		{
			return; // no response
		}
		case ClientFeature::CodeVersion: // Client is telling us its version
		{
			if (sa->type == AppearanceType::ClientDllMessage)
				m_dll_version = feature_value;
			return; // no response
		}
		case ClientFeature::BuffStackingPatchHandshakeV1: // Deprecated handshake of older eqgame.dll's buffstacking patch, no longer supported.
		{
			m_old_feature_detected = true; // Tells the user their eqgame.dll is out of date
			return; // no response
		}
		case ClientFeature::BuffStackingPatchWithSongWindowHandshake: // Enables BuffStacking + Song Window (NewUI Uesrs)
		{
			if (!RuleB(Spells, AllowBuffstackingPatch))
			{
				feature_value = 0;
				SetBuffStackingPatch(false);
				SetSongWindowSlots(0);
			}
			else if (feature_value >= 1)
			{
				feature_value = 1;
				SetBuffStackingPatch(true);
				SetSongWindowSlots(SONG_WINDOW_BUFF_SLOTS);
			}
			else
			{
				feature_value = 0;
				SetBuffStackingPatch(false);
				SetSongWindowSlots(0);
			}
			break;
		}
		case ClientFeature::BuffStackingPatchWithoutSongWindowHandshake: // Enables BuffStacking, but no Song Window (OldUI Users)
		{
			if (!RuleB(Spells, AllowBuffstackingPatch))
			{
				feature_value = 0;
				SetBuffStackingPatch(false);
				SetSongWindowSlots(0);
			}
			else if (feature_value >= 1)
			{
				feature_value = 1;
				SetBuffStackingPatch(true);
				SetSongWindowSlots(0);
			}
			else
			{
				feature_value = 0;
				SetBuffStackingPatch(false);
				SetSongWindowSlots(0);
			}
			break;
		}
		case ClientFeature::SharedBankBagsSupported:
		{
			if (GetSharedBankMode() != 0 || GetSharedBankBagsCount() != 0)
				return; // Already initialized
			
			int max_shared_bank = RuleI(Quarm, SharedBankBags);
			uint8 bags = 0;
			uint8 mode = 0;

			if (IsSelfFoundAny())
			{
				bags = 0;
				mode = 2; // disabled (with self-found error message)
			}
			else if (max_shared_bank >= 0)
			{
				bags = feature_value > max_shared_bank ? max_shared_bank : feature_value;
				mode = 1; // enabled
			}
			feature_value = bags; // response value

			SetSharedBankBagsCount(bags);
			SetSharedBankMode(mode);

			// Also send Mode Packet to Client
			auto outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
			SpawnAppearance_Struct* sa_mode = (SpawnAppearance_Struct*)outapp->pBuffer;
			sa_mode->spawn_id = 0;
			sa_mode->type = sa->type;
			sa_mode->parameter = (1 << 31) | ((uint32)ClientFeature::SharedBankMode << 16) | (uint32)mode;
			outapp->priority = 6;
			QueuePacket(outapp);
			safe_delete(outapp);

			if (mode == 1)
			{
				BulkSendSharedBankItems();
			}
		}
		// -----------------------------------------------------------------------------------------------------------------------------------
		// End of Client/Server known feature messages
		// -----------------------------------------------------------------------------------------------------------------------------------
		}

		// Respond to the client to complete the handshake.
		if (send_response) {
			feature_id = feature_id | 0x8000u; // Set the response bit
			sa->parameter = (feature_id << 16) | feature_value;
			sa->spawn_id = 0; // We don't need to set spawn_id on these special messages, they are always to the client's self.
			QueuePacket(app, false);
		}
	}
}

void Client::Handle_Connect_OP_TGB(const EQApplicationPacket *app)
{
	if (app->size != sizeof(uint32)) {
		Log(Logs::General, Logs::Error, "Invalid size on OP_TGB: Expected %i, Got %i",
			sizeof(uint32), app->size);
		return;
	}
	OPTGB(app);
	return;
}

void Client::Handle_Connect_OP_WearChange(const EQApplicationPacket *app)
{
	Handle_OP_WearChange(app);
	return;
}

void Client::Handle_Connect_OP_ZoneEntry(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClientZoneEntry_Struct))
		return;
	ClientZoneEntry_Struct *cze = (ClientZoneEntry_Struct *)app->pBuffer;

	if (strlen(cze->char_name) > 63)
		return;

	if (conn_state != NoPacketsReceived && client_state != CLIENT_AUTH_RECEIVED) {
		// have a ghost maybe?
		/* Check for Client Spoofing */
		Client* client = entity_list.GetClientByName(cze->char_name);
		if (client != 0) {
			uint16 remote_port = ntohs(eqs->GetRemotePort());
			if (client->GetIP() != eqs->GetRemoteIP() || client->GetPort() != remote_port) {
				struct in_addr ghost_addr;
				ghost_addr.s_addr = eqs->GetRemoteIP();
				struct in_addr local_addr;
				local_addr.s_addr = client->GetIP();
				Log(Logs::General, Logs::Error, "Ghosting client: Account ID:%i Name:%s Character:%s IP:%s PORT:%d Incoming IP:%s PORT:%d",
					client->AccountID(), client->AccountName(), client->GetName(), inet_ntoa(ghost_addr), ntohs(eqs->GetRemotePort()), inet_ntoa(local_addr), client->GetPort());
				client->Save();
				client->Kick();
				client->Disconnect();
				return;
			}
		}
		if (conn_state == ZoneInfoSent) {
			// resend weather packet
			Log(Logs::General, Logs::Error, "Received a second OP_ZoneEntry.  Resending OP_Weather to see if client recovers.");
			auto outapp = new EQApplicationPacket(OP_Weather, sizeof(Weather_Struct));
			Weather_Struct* ws = (Weather_Struct*)outapp->pBuffer;
			ws->type = 0;
			ws->intensity = 0;

			outapp->priority = 6;
			QueuePacket(outapp);
			safe_delete(outapp);
		}
		return;
	}

	conn_state = ReceivedZoneEntry;
	/* Antighost code
	tmp var is so the search doesnt find this object
	*/
	Client* client = entity_list.GetClientByName(cze->char_name);
	if (!zone->GetAuth(ip, cze->char_name, &WID, &account_id, &character_id, &admin, lskey, &tellsoff, &versionbit, GetID())) {
		LogClientLogin("[{}] failed zone auth check", cze->char_name);
		if (nullptr != client && client_state != CLIENT_AUTH_RECEIVED) {
			client->Save();
			client->Kick("Failed auth check");
			return;
		}
		else {
			// we did not find client in entity list
			// copy name over, so when it is made to disconnect
			// it will be removed from raid/groups
			// this is a client that likely disconnected while zoning
			if (strlen(cze->char_name) > 2)
				strcpy(name, cze->char_name);
		}
		if (client_state == CLIENT_CONNECTING) {
			Log(Logs::General, Logs::Error, "GetAuth() returned false - saving packet for retry");
			// remove from any groups/raids here
			auto new_app = new EQApplicationPacket(OP_ZoneEntry, sizeof(ClientZoneEntry_Struct));
			memcpy(new_app->pBuffer, app->pBuffer, app->size);
			if (zoneentry != nullptr)
				safe_delete(zoneentry);
			zoneentry = new_app;

			//ret = false; // TODO: Can we tell the client to get lost in a good way
			client_state = CLIENT_WAITING_FOR_AUTH;
			get_auth_timer.Start();
		}
		return;
	}

	/**
	 * DevTools Load Settings
	 */
	if (Admin() > EQ::DevTools::GM_ACCOUNT_STATUS_LEVEL) {
		std::string dev_tools_window_key = StringFormat("%i-dev-tools-disabled", AccountID());
		if (DataBucket::GetData(dev_tools_window_key) == "true") {
			dev_tools_enabled = false;
		}
	}

	SetClientVersion(Connection()->ClientVersion());
	m_ClientVersionBit = EQ::versions::ConvertClientVersionToClientVersionBit(Connection()->ClientVersion());

	if (ClientVersion() == EQ::versions::ClientVersion::Mac)
	{
		m_ClientVersionBit = versionbit;
	}
	else
	{
		m_ClientVersionBit = EQ::versions::Unused;
	}
	std::string clientname = EQ::versions::ClientVersionName(EQ::versions::ConvertClientVersionBitToClientVersion(m_ClientVersionBit));
	Log(Logs::Detail, Logs::ZoneServer, "ClientVersionBit is: %i : %s ", m_ClientVersionBit, clientname.c_str());

	strcpy(name, cze->char_name);
	/* Check for Client Spoofing */
	if (client != 0) {
		uint16 remote_port = ntohs(eqs->GetRemotePort());
		if (client->GetIP() != eqs->GetRemoteIP() || client->GetPort() != remote_port) {
			struct in_addr ghost_addr;
			ghost_addr.s_addr = eqs->GetRemoteIP();
			struct in_addr local_addr;
			local_addr.s_addr = client->GetIP();
			Log(Logs::General, Logs::Error, "Ghosting client: Account ID:%i Name:%s Character:%s IP:%s PORT:%d Incoming IP:%s PORT:%d",
				client->AccountID(), client->AccountName(), client->GetName(), inet_ntoa(ghost_addr), ntohs(eqs->GetRemotePort()), inet_ntoa(local_addr), client->GetPort());
			client->Save();
			client->HardDisconnect();
		}
	}

	uint32 pplen = 0;
	EQApplicationPacket* outapp = nullptr;
	MYSQL_RES* result = nullptr;
	bool loaditems = 0;

	exp_sessionStart = Timer::GetCurrentTime();
	exp_sessionGained = 0;
	exp_nextCheck = RuleI(AA, ExpPerPoint);
	exp_nextCheck *= 2;
	uint32 i;
	std::string query;

	uint32 acct_id = AccountID();
	uint32 cid = CharacterID();
	character_id = cid; /* Global character_id reference */

	/* Flush and reload factions */
	database.RemoveTempFactions(this);
	database.LoadCharacterFactionValues(cid, factionvalues);

	/* Load Character Account Data */
	auto a = AccountRepository::FindOne(database, AccountID());
	if (a.id > 0) {
		strn0cpy(account_name, a.name.c_str(), sizeof(account_name));

		admin = a.status;
		lsaccountid = a.lsaccount_id;
		gmspeed = a.gmspeed;
		revoked = a.revoked;
		gmhideme = a.hideme;
		account_creation = a.time_creation;
		gminvul = a.gminvul;
		flymode = static_cast<GravityBehavior>(a.flymode);
		tellsoff = gmhideme;
	}

	revoked = database.CheckRevoked(AccountID());

	/* Load Character Data */
	query = StringFormat("SELECT `firstlogon`, `guild_id`, `rank` FROM `character_data` LEFT JOIN `guild_members` ON `id` = `char_id` WHERE `id` = %i", cid);
	
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		if (row[1] && atoi(row[1]) > 0) {
			guild_id = atoi(row[1]);
			if (row[2] != nullptr) { guildrank = atoi(row[2]); }
			else { guildrank = GUILD_RANK_NONE; }
		}

		firstlogon = atoi(row[0]);
	}

	/* Do not write to the PP prior to this otherwise it will just be overwritten when it's loaded from the DB */
	loaditems = database.GetInventory(acct_id, cid, &m_inv); /* Load Character Inventory */
	database.LoadCharacterBindPoint(cid, &m_pp); /* Load Character Bind */
	database.LoadCharacterCurrency(cid, &m_pp); /* Load Character Currency into PP */
	database.LoadCharacterData(cid, &m_pp, &m_epp); /* Load Character Data from DB into PP as well as E_PP */
	database.LoadCharacterSkills(cid, &m_pp); /* Load Character Skills */
	database.LoadCharacterSpellBook(cid, &m_pp); /* Load Character Spell Book */
	database.LoadCharacterMemmedSpells(cid, &m_pp);  /* Load Character Memorized Spells */
	database.LoadCharacterLanguages(cid, &m_pp); /* Load Character Languages */
	database.LoadCharacterLootLockouts(loot_lockouts, cid); /* Load Loot Lockouts */
	database.LoadCharacterReimbursements(item_reimbursement_list, cid); /* Load Items for Reimbursement */
	bool deletenorent = database.NoRentExpired(GetName());
	if (loaditems && deletenorent) {
		// client was offline for more than 30 minutes, delete no rent items
		// we do this here to avoid NO RENT weapons / armor from displaying
		// from the ServerZoneEntry_Struct sent further down in this method.
		RemoveNoRent(false);
	}

	if (m_epp.solo_only || m_epp.self_found) {

		// Ensure 'solo' players also have the 'self-found' tag
		if (m_epp.solo_only == 1)
			m_epp.self_found = 1;

		// Any items in their possesion that are missing self-found tag should get a self-found tag on them.
		// This is for players that haven't logged in since this self-found tagging system was implemented
		if (loaditems) {
			m_inv.MarkItemsSelfFound(cid, m_pp.name);
		}
	}

	if (m_pp.platinum_cursor > 0 || m_pp.silver_cursor > 0 || m_pp.gold_cursor > 0 || m_pp.copper_cursor > 0) {
		bool changed = false;
		uint64 new_coin = 0;
		if (m_pp.platinum_cursor > 0) {
			new_coin = (uint64)m_pp.platinum + m_pp.platinum_cursor;
			if (new_coin < INT_MAX) {
				m_pp.platinum += m_pp.platinum_cursor;
				m_pp.platinum_cursor = 0;
				changed = true;
			}
			else {
				new_coin = (uint64)m_pp.platinum_bank + m_pp.platinum_cursor;
				if (new_coin < INT_MAX) {
					m_pp.platinum_bank += m_pp.platinum_cursor;
					m_pp.platinum_cursor = 0;
					changed = true;
				}
			}
		}
		if (m_pp.gold_cursor > 0) {
			new_coin = (uint64)m_pp.gold + m_pp.gold_cursor;
			if (new_coin < INT_MAX) {
				m_pp.gold += m_pp.gold_cursor;
				m_pp.gold_cursor = 0;
				changed = true;
			}
			else {
				new_coin = (uint64)m_pp.gold_bank + m_pp.gold_cursor;
				if (new_coin < INT_MAX) {
					m_pp.gold_bank += m_pp.gold_cursor;
					m_pp.gold_cursor = 0;
					changed = true;
				}
			}
		}
		if (m_pp.silver_cursor > 0) {
			new_coin = (uint64)m_pp.silver + m_pp.silver_cursor;
			if (new_coin < INT_MAX) {
				m_pp.silver += m_pp.silver_cursor;
				m_pp.silver_cursor = 0;
				changed = true;
			}
			else {
				new_coin = (uint64)m_pp.silver_bank + m_pp.silver_cursor;
				if (new_coin < INT_MAX) {
					m_pp.silver_bank += m_pp.silver_cursor;
					m_pp.silver_cursor = 0;
					changed = true;
				}
			}
		}
		if (m_pp.copper_cursor > 0) {
			new_coin = (uint64)m_pp.copper + m_pp.copper_cursor;
			if (new_coin < INT_MAX) {
				m_pp.copper += m_pp.copper_cursor;
				m_pp.copper_cursor = 0;
				changed = true;
			}
			else {
				new_coin = (uint64)m_pp.copper_bank + m_pp.copper_cursor;
				if (new_coin < INT_MAX) {
					m_pp.copper_bank += m_pp.copper_cursor;
					m_pp.copper_cursor = 0;
					changed = true;
				}
			}
		}
		if (changed)
			SaveCurrency();
	}

	if (level) { level = m_pp.level; }

	if (gminvul) { invulnerable = true; }
	/* Set Con State for Reporting */
	conn_state = PlayerProfileLoaded;

	m_pp.zone_id = zone->GetZoneID();
	m_epp.zone_guild_id = zone->GetGuildID();
	ignore_zone_count = false;


	SendToBoat();

	/* Load Character Key Ring */
	KeyRingLoad();

	/*Load Looted Legacy Items*/
	LoadLootedLegacyItems();

	prev_last_login_time = m_pp.lastlogin;

	/* Set Total Seconds Played */
	m_pp.lastlogin = time(nullptr);
	TotalSecondsPlayed = m_pp.timePlayedMin * 60;
	/* Set Max AA XP */
	max_AAXP = GetEXPForLevel(0, true);
	/* If we can maintain intoxication across zones, check for it */
	if (!RuleB(Character, MaintainIntoxicationAcrossZones))
		m_pp.intoxication = 0;

	strcpy(name, m_pp.name);
	strcpy(lastname, m_pp.last_name);

	// Customized lastname is stored in temp_last_name. This can be changed at the Title NPC (PoD).
	if (RuleB(Quarm, ShowTemporaryLastNames)) {
		char* tmp_lastname = m_epp.temp_last_name;
		if (tmp_lastname[0] && strlen(tmp_lastname) < sizeof(lastname))
		{
			memset(lastname, 0, sizeof(lastname));
			// OCD: Strip leading '_' for the rest of the values below
			if (tmp_lastname[0] == '_') {
				tmp_lastname = &tmp_lastname[1];
			}
			strcpy(lastname, tmp_lastname);
		}
	}

	/* If PP is set to weird coordinates */
	if ((m_pp.x == -1 && m_pp.y == -1 && m_pp.z == -1) || (m_pp.x == -2 && m_pp.y == -2 && m_pp.z == -2)) {
        auto safePoint = zone->GetSafePoint();
		m_pp.x = safePoint.x;
		m_pp.y = safePoint.y;
		m_pp.z = safePoint.z;
	}
	/* If too far below ground, then fix */
	// float ground_z = GetGroundZ(m_pp.x, m_pp.y, m_pp.z);
	// if (m_pp.z < (ground_z - 500))
	// 	m_pp.z = ground_z;

	/* Set Mob variables for spawn */
	class_ = m_pp.class_;
	level = m_pp.level;
	last_position_update_time = std::chrono::high_resolution_clock::now();
	m_RewindLocation = glm::vec3();
	m_LastLocation = glm::vec4();
	m_Position.x = m_pp.x;
	m_Position.y = m_pp.y;
	m_Position.z = m_pp.z;
	m_Position.w = m_pp.heading * 0.5f;
	race = m_pp.race;
	base_race = m_pp.race;
	gender = m_pp.gender;
	base_gender = m_pp.gender;
	deity = m_pp.deity;
	haircolor = m_pp.haircolor;
	beardcolor = m_pp.beardcolor;
	eyecolor1 = m_pp.eyecolor1;
	eyecolor2 = m_pp.eyecolor2;
	hairstyle = m_pp.hairstyle;
	luclinface = m_pp.face;
	beard = m_pp.beard;

	// Max Level for Character:PerCharacterQglobalMaxLevel and Character:PerCharacterBucketMaxLevel
	uint8 client_max_level = 0;
	if (RuleB(Character, PerCharacterQglobalMaxLevel)) {
		client_max_level = GetCharMaxLevelFromQGlobal();
	}
	else if (RuleB(Character, PerCharacterBucketMaxLevel)) {
		client_max_level = GetCharMaxLevelFromBucket();
	}
	SetClientMaxLevel(client_max_level);

	/* If GM not set in DB, and does not meet min status to be GM, reset */
	if (m_pp.gm && admin < minStatusToBeGM)
		m_pp.gm = 0;

	/* Load Guild */
	if (!IsInAGuild()) { m_pp.guild_id = GUILD_NONE; }
	else {
		m_pp.guild_id = GuildID();
		m_pp.guildrank = GuildRank();
	}

	size = GetPlayerHeight(race);
	base_size = size;
	m_pp.height = size;
	// PP collects suggest these are all 0.
	m_pp.width = 0;
	m_pp.length = 0;
	m_pp.view_height = 0;

	z_offset = CalcZOffset();
	head_offset = CalcHeadOffset();
	model_size = CalcModelSize();
	model_bounding_radius = CalcBoundingRadius();
	/* Initialize AA's : Move to function eventually */
	for (uint32 a = 0; a < MAX_PP_AA_ARRAY; a++){ aa[a] = &m_pp.aa_array[a]; }
	query = StringFormat(
		"SELECT								"
		"slot,							    "
		"aa_id,								"
		"aa_value							"
		"FROM								"
		"`character_alternate_abilities`    "
		"WHERE `id` = %u ORDER BY `slot`", this->CharacterID());
	results = database.QueryDatabase(query); i = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		i = atoi(row[0]);
		m_pp.aa_array[i].AA = atoi(row[1]);
		m_pp.aa_array[i].value = atoi(row[2]);
		aa[i]->AA = atoi(row[1]);
		aa[i]->value = atoi(row[2]);
	}
	for (uint32 a = 0; a < MAX_PP_AA_ARRAY; a++){
		uint32 id = aa[a]->AA;
		//watch for invalid AA IDs
		if (id == aaNone)
			continue;
		if (id >= aaHighestID) {
			aa[a]->AA = aaNone;
			aa[a]->value = 0;
			continue;
		}
		if (aa[a]->value == 0) {
			aa[a]->AA = aaNone;
			continue;
		}
		if (aa[a]->value > HIGHEST_AA_VALUE) {
			aa[a]->AA = aaNone;
			aa[a]->value = 0;
			continue;
		}

		if (aa[a]->value > 1)	/* hack in some stuff for sony's new AA method (where each level of each aa.has a seperate ID) */
			aa_points[(id - aa[a]->value + 1)] = aa[a]->value;
		else
			aa_points[id] = aa[a]->value;
	}

	/* Send Group Members via PP */
	uint32 groupid = database.GetGroupID(GetName());
	Group* group = nullptr;
	if (groupid > 0) {
		group = entity_list.GetGroupByID(groupid);
		
		if (group)
		{
			// if everyone is out of zone in this group, then our group has been here but every member left the zone.
			// while the group was gone its members could have changed, so now it's stale and needs to be reloaded
			int members_in_zone = 0;
			for (int i = 0; i < MAX_GROUP_MEMBERS; i++)
			{
				if (group->members[i])
				{
					members_in_zone++;
					break;
				}
			}
			if (!members_in_zone)
			{
				entity_list.RemoveGroup(groupid);
				group = nullptr;
			}
		}

		if (!group) {	//nobody from our is here... start a new group
			group = new Group(groupid);
			if (group->GetID() != 0)
				entity_list.AddGroup(group, groupid);
			else	//error loading group members...
			{
				delete group;
				group = nullptr;
			}
		}	//else, somebody from our group is already here...

		if (group)
			group->UpdatePlayer(this);
		else
			UpdateGroupID(0);

	}
	else {	//no group id
		//clear out the group junk in our PP
		uint32 xy = 0;
		for (xy = 0; xy < MAX_GROUP_MEMBERS; xy++)
			memset(m_pp.groupMembers[xy], 0, 64);
	}

	if (group)
	{
		// If the group leader is not set, pull the group leader information from the database.
		char ln[64];
		memset(ln, 0, 64);
		strcpy(ln, database.GetGroupLeadershipInfo(group->GetID(), ln));
		Client *c = entity_list.GetClientByName(ln);
		if (!group->GetLeader())
		{
			Log(Logs::General, Logs::Group, "Player %s in group %d is setting leader to %s due to leader being null.", GetName(), group->GetID(), ln);

			if (c)
			{
				group->SetLeader(c);
			}
			else
			{
				// If we can't set the leader, we should set their name.
				group->SetLeaderName(ln);
			}

			std::string oldleader = database.GetGroupOldLeaderName(group->GetID());
			group->SetOldLeaderName(oldleader.c_str());
		}
	}

	/* Send Raid Group Members via PP */
	uint32 raidid = database.GetRaidID(GetName());
	Raid *raid = nullptr;
	if (raidid > 0) {
		raid = entity_list.GetRaidByID(raidid);
		if (!raid) {
			raid = new Raid(raidid);
			if (raid->GetID() != 0) {
				entity_list.AddRaid(raid, raidid);
			}
			else
				raid = nullptr;
		}
		if (raid) {
			raid->LearnMembers();
			raid->VerifyRaid();
			raid->GetRaidDetails();
			int gid = raid->GetGroup(GetName());
			if (gid != 0xFFFFFFFF)
			{
				raid->UpdatePlayer(this);
				SetRaidGrouped(true);
			}
			else
			{
				uint32 xy = 0;
				for (xy = 0; xy < MAX_GROUP_MEMBERS; xy++)
					memset(m_pp.groupMembers[xy], 0, 64);
			}
		}
	}

	if (SPDAT_RECORDS > 0)
	{
		database.LoadBuffs(this);
		for (uint32 z = 0; z<MAX_PP_MEMSPELL; z++)
		{
			if (m_pp.mem_spells[z] >= (uint32)SPDAT_RECORDS)
				UnmemSpell(z, false);
		}
	}

	CalcBonuses();

	if (m_pp.cur_hp <= -200)
	{
		m_pp.cur_hp = GetMaxHP();
	}

	SetHP(m_pp.cur_hp);

	Mob::SetMana(m_pp.mana); // mob function doesn't send the packet

	uint32 max_slots = GetMaxBuffSlots();
	bool stripbuffs = false;

	if (firstlogon || !zone->CanCastOutdoor() || zone->GetZoneID() == Zones::SSERU)
	{
		BuffFadeByEffect(SE_SummonHorse);
	}

	if(RuleB(AlKabor, StripBuffsOnLowHP) && GetHP() < itembonuses.HP)
		stripbuffs = true;

	for (int i = 0; i < max_slots; i++)
	{
		SpellBuff_Struct& buff = i < 15 ? m_pp.buffs[i] : m_pp.buffs_ext[i - 15];
		if ((buffs[i].spellid != SPELL_UNKNOWN && !stripbuffs) ||
			IsResurrectionEffects(buffs[i].spellid) ||
			SpellPersistsThroughDeath(buffs[i].spellid))
		{
			buff.spellid = buffs[i].spellid;
			buff.bard_modifier = buffs[i].instrumentmod;
			buff.bufftype = buffs[i].bufftype ? buffs[i].bufftype : 2;
			buff.player_id = buffs[i].casterid;
			buff.level = buffs[i].casterlevel;
			buff.activated = spells[buffs[i].spellid].Activated;
			buff.duration = buffs[i].ticsremaining;
			buff.counters = buffs[i].counters;
		}
		else
		{
			buff.spellid = SPELL_UNKNOWN;
			buff.bard_modifier = 10;
			buff.bufftype = 0;
			buff.player_id = 0;
			buff.level = 0;
			buff.activated = 0;
			buff.duration = 0;
			buff.counters = 0;
		}
	}

	if(stripbuffs)
	{
		Log(Logs::General, Logs::EQMac, "Removing buffs from %s. HP is: %i MaxHP is: %i BaseHP is: %i HP from items is: %i HP from spells is: %i", GetName(), GetHP(), GetMaxHP(), GetBaseHP(), itembonuses.HP, spellbonuses.HP);
		BuffFadeNonPersistDeath(true);
		SetHP(itembonuses.HP);
	}

	if (m_pp.z <= zone->newzone_data.underworld) 
	{
		m_pp.x = zone->newzone_data.safe_x;
		m_pp.y = zone->newzone_data.safe_y;
		m_pp.z = zone->newzone_data.safe_z;
	}

	uint16 expansion = 0;
	bool mule = false;
	uint32 force_guild;
	database.GetAccountRestriction(AccountID(), expansion, mule, force_guild);
	m_pp.force_guild_id = force_guild;
	if (m_pp.force_guild_id != 0)
	{
		if (force_guild != guild_id)
		{
			guild_mgr.SetGuild(CharacterID(), force_guild, GUILD_MEMBER);
		}
		guild_id = m_pp.force_guild_id;
		guildrank = GUILD_MEMBER;
		m_pp.guild_id = m_pp.force_guild_id;
		m_pp.guildrank = GUILD_MEMBER;
	}
	m_pp.expansions = expansion;
	m_pp.mule = mule;

	p_timers.SetCharID(CharacterID());
	if (!p_timers.Load(&database)) {
		Log(Logs::General, Logs::Error, "Unable to load ability timers from the database for %s (%i)!", GetCleanName(), CharacterID());
	}

	/* Load Spell Slot Refresh from Currently Memorized Spells */
	for (unsigned int i = 0; i < MAX_PP_MEMSPELL; ++i)
		if (IsValidSpell(m_pp.mem_spells[i]))
			m_pp.spellSlotRefresh[i] = p_timers.GetRemainingTime(pTimerSpellStart + m_pp.mem_spells[i]) * 1000;

	/* Ability slot refresh send SK/PAL */
	if (m_pp.class_ == Class::ShadowKnight || m_pp.class_ == Class::Paladin) {
		uint32 abilitynum = 0;
		if (m_pp.class_ == Class::ShadowKnight)
		{ 
			abilitynum = pTimerHarmTouch; 
		}
		else
		{ 
			abilitynum = pTimerLayHands;
		}

		uint32 remaining = p_timers.GetRemainingTime(abilitynum);
		if (remaining > 0)
			m_pp.abilitySlotRefresh = remaining * 1000;
		else
			m_pp.abilitySlotRefresh = 0;
	}

	if (!p_timers.Expired(&database, pTimerModulation))
	{
		m_pp.LastModulated = p_timers.GetStartTime(pTimerModulation);
	}

#ifdef _EQDEBUG
	Log(Logs::Detail, Logs::Inventory, "Dumping inventory on load:\n");
	m_inv.dumpEntireInventory();
#endif

	/* Reset to max so they dont drown on zone in if its underwater */
	if (!RuleB(AlKabor, RememberAir))
	{
		m_pp.air_remaining = CalculateLungCapacity();
	}

	/* Time entitled on Account: Move to account */
	m_pp.timeentitledonaccount = database.GetTotalTimeEntitledOnAccount(AccountID()) / 1440;

	FillPPItems();
	uint32 zoneid_previous = m_pp.zone_id;
	m_pp.zone_id = database.GetClientZoneID(m_pp.zone_id);
	/* This checksum should disappear once dynamic structs are in... each struct strategy will do it */
	CRC32::SetEQChecksum((unsigned char*)&m_pp, sizeof(PlayerProfile_Struct) - 4);
	outapp = new EQApplicationPacket(OP_PlayerProfile, sizeof(PlayerProfile_Struct));

	PlayerProfile_Struct* pps = (PlayerProfile_Struct*) new uchar[sizeof(PlayerProfile_Struct) - 4];
	memcpy(pps, &m_pp, sizeof(PlayerProfile_Struct) - 4);
	pps->eqbackground = 0;
	pps->heading = m_pp.heading;
	pps->perAA = m_epp.perAA;
	int r = 0;
	for (r = 0; r < MAX_PP_AA_ARRAY; r++)
	{
		if (pps->aa_array[r].AA > 0)
		{
			// skip over Fleet of Foot in case anyone has it from before it was blocked, we'll force it in below
			if (zone->EmuToEQMacAA(pps->aa_array[r].AA) == 211)
			{
				pps->aa_array[r].AA = 0;
				pps->aa_array[r].value = 0;
				continue;
			}
			pps->aa_array[r].AA = zone->EmuToEQMacAA(pps->aa_array[r].AA);
			pps->aa_array[r].value = pps->aa_array[r].value * 16;
		}
	}
	// solar: there is a bug in the macintosh client such that every character always has the equivalent of Fleet of Foot level 3 - non bards too.
	// By granting this ability to every character all the time, it makes them all run at the same speed when grouped with a bard, even on the windows client, which doesn't have this bug.
	// Our AA data only allows the ability to be improved to level 2 but the level 3 case exists in the code in both clients and is the default case in the macintosh client, so level 0 also
	// hits it and no value that we can send disables it.  
	// This solution allows /follow to work consistently across clients and prevents bards from purchasing this ability, which would slow them on the macintosh client.
	for (int a = 0; a < MAX_PP_AA_ARRAY; a++)
	{
		if (pps->aa_array[a].AA == 0)
		{
			pps->aa_array[a].AA = 211;
			pps->aa_array[a].value = 3 * 16;
			break;
		}
	}
	for (int s = 0; s <= EQ::skills::HIGHEST_SKILL; s++)
	{
		EQ::skills::SkillType currentskill = (EQ::skills::SkillType)s;
		if (pps->skills[s] > 0)
			continue;
		else
		{
			int haveskill = GetMaxSkillAfterSpecializationRules(currentskill, MaxSkill(currentskill, GetClass(), RuleI(Character, MaxLevel)));
			if (haveskill > 0)
			{
				pps->skills[s] = 254;
				//If we never get the skill, value is 255. If we qualify for it AND do not need to train it it's 0, 
				//if we get it but don't yet qualify or it needs to be trained it's 254.
				uint16 t_level = GetSkillTrainLevel(currentskill, GetClass());
				if (t_level <= GetLevel())
				{
					if (t_level == 1)
						pps->skills[s] = 0;
				}
			}
			else
				pps->skills[s] = 255;
		}
	}
	
	if(m_pp.boatid > 0 && (zone->GetZoneID() == Zones::TIMOROUS || zone->GetZoneID() == Zones::FIRIONA))
		pps->boat[0] = 0;

	// reapply some buffs - pass 1
	// this pass is not sending packets or changing anything, it's just loading the current state from the buffs like invuln/invis
	uint32 buff_count = GetMaxTotalSlots();
	for (uint32 j1 = 0; j1 < buff_count; j1++) 
	{
		if (!IsValidSpell(buffs[j1].spellid))
			continue;

		uint16 spell_id = buffs[j1].spellid;
		const SPDat_Spell_Struct &spell = spells[spell_id];

		for (int x1 = 0; x1 < EFFECT_COUNT; x1++)
		{
			switch (spell.effectid[x1])
			{
				case SE_Silence:
				{
					Silence(true);
					break;
				}
				case SE_Amnesia:
				{
					Amnesia(true);
					break;
				}
				case SE_DivineAura:
				{
					invulnerable = true;
					break;
				}
				case SE_Invisibility:
				{
					SetInvisible(INVIS_NORMAL, false);
					break;
				}
				case SE_InvisVsUndead:
				{
					invisible_undead = true;
					break;
				}
				case SE_InvisVsAnimals:
				{
					invisible_animals = true;
					break;
				}
				case SE_SeeInvis:
				{
					see_invis = 1;
					break;
				}
			}
		}
	}

	memcpy(outapp->pBuffer, pps, sizeof(PlayerProfile_Struct) - 4);
	safe_delete_array(pps);
	m_pp.zone_id = zoneid_previous;
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	memset(&m_petinfo, 0, sizeof(PetInfo)); // not used for TAKP but leaving in case someone wants to fix it up for custom servers
	if (RuleB(Quarm, PetZoneWithOwner)) {
		/* Load Pet */
		database.LoadPetInfo(this);
	}

	/* Moved here so it's after where we load the pet data. */
	memset(&m_suspendedminion, 0, sizeof(PetInfo));

	/* Server Zone Entry Packet */
	outapp = new EQApplicationPacket(OP_ZoneEntry, sizeof(ServerZoneEntry_Struct));
	ServerZoneEntry_Struct* sze = (ServerZoneEntry_Struct*)outapp->pBuffer;
	memset(sze, 0, sizeof(ServerZoneEntry_Struct));
	
	strcpy(sze->name, name);
	strn0cpy(sze->Surname, lastname, 32);
	sze->zoneID = database.GetClientZoneID(zone->GetZoneID());
	sze->x_pos = m_pp.x;
	sze->y_pos = m_pp.y;
	sze->z_pos = m_pp.z;
	sze->heading = m_pp.heading;
	sze->race = m_pp.race;
	sze->deity = m_pp.deity;
	sze->curHP = static_cast<uint32>(GetHPRatio());
	sze->max_hp = 100;

	sze->size = base_size;
	sze->width = 0;
	sze->length = 0;
	sze->view_height = 6.2;
	sze->sprite_oheight = 3.125;
	
	sze->NPC = IsBecomeNPC() ? 1 : 10;
	sze->invis = invisible ? 1 : 0;
	sze->sneaking = sneaking;
	sze->animation = animation;
	
	sze->haircolor = m_pp.haircolor;
	sze->beardcolor = m_pp.beardcolor;
	sze->eyecolor1 = m_pp.eyecolor1;
	sze->eyecolor2 = m_pp.eyecolor2;
	sze->hairstyle = m_pp.hairstyle;
	sze->beard = m_pp.beard;
	sze->face = m_pp.face;
	sze->level = m_pp.level;
	for(int k = EQ::textures::textureBegin; k <= EQ::textures::LastTexture; k++)
	{
		sze->equipment[k] = GetEquipmentMaterial(k);
		sze->equipcolors.Slot[k].Color = GetEquipmentColor(k);
	}
	sze->AFK = AFK;
	sze->anon = m_pp.anon;
	sze->title = GetAARankTitle();
	sze->anim_type = 0x64;
	sze->bodytexture = texture;
	sze->bodytype = bodytype;

	if(helmtexture && helmtexture != 0xFF)
	{
		sze->helm=helmtexture;
	} else {
		sze->helm = 0;
	}

	sze->GM = GetGM() ? 1 : 0;
	sze->GuildID = GuildID();
	if(sze->GuildID == 0)
		sze->GuildID = 0xFFFF;
	if (!IsInAGuild()) {
		sze->guildrank = 0xFFFF;
	} else {
		sze->guildrank = guild_mgr.GetDisplayedRank(GuildID(), GuildRank(), AccountID());
	}
	if(sze->guildrank == 0)
		sze->guildrank = 0xFFFF;
			
	sze->walkspeed = walkspeed;
	sze->runspeed = (gmspeed == 0) ? runspeed : 3.1f;
	sze->light = m_Light.Type[EQ::lightsource::LightActive];
	sze->class_ = GetClass();

	sze->gender = m_pp.gender;
	sze->flymode = flymode ? flymode : (FindType(SE_Levitate) ? GravityBehavior::Levitating : GravityBehavior::Ground);
	sze->prev = 0xa0ae0e00;
	sze->next = 0xa0ae0e00;
	sze->extra[10] = 0xFF;
	sze->extra[11] = 0xFF;
	sze->extra[12] = 0xFF;
	sze->extra[13] = 0xFF;
	sze->type = 0;
	sze->petOwnerId = ownerid;

	sze->curHP = 1;
	sze->NPC = 0;
	if(zone->zonemap)
	{
	// This prevents hopping on logging in.
	glm::vec3 loc(m_Position.x, m_Position.y, m_Position.z);
	if (!IsEncumbered() && m_pp.boatid == 0 && (!zone->HasWaterMap() || !zone->watermap->InLiquid(loc)) && 
		zone->GetZoneID() != Zones::HOLE && zone->GetZoneID() != Zones::FREPORTE)
	{
		float bestz = zone->zonemap->FindBestZ(loc, nullptr);
		if (bestz != BEST_Z_INVALID && size > 0)
			m_Position.z = bestz + CalcZOffset();
	}
	sze->z_pos = m_Position.z;
	}

	CRC32::SetEQChecksum((unsigned char*)sze, sizeof(ServerZoneEntry_Struct));
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	/* Zone Spawns Packet */
	entity_list.SendZoneSpawnsBulk(this);
	entity_list.SendZoneCorpsesBulk(this);
	entity_list.SendClientAppearances(this);
	UpdateLFG(LFG, true);

	/* Time of Day packet */
	outapp = new EQApplicationPacket(OP_TimeOfDay, sizeof(TimeOfDay_Struct));
	TimeOfDay_Struct* tod = (TimeOfDay_Struct*)outapp->pBuffer;
	zone->zone_time.GetCurrentEQTimeOfDay(time(0), tod);
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	/*
	Character Inventory Packet
	AK sent both individual item packets and a single bulk inventory packet on zonein
	The client requires cursor items to be sent in Handle_Connect_OP_SendExpZonein, should all items be moved there as well?
	*/
	if (loaditems) { /* Dont load if a length error occurs */
		// LINKDEAD TRADE ITEMS
		// Move trade slot items back into normal inventory..need them there now for the proceeding validity checks -U
		for(int slot_id = EQ::invslot::TRADE_BEGIN; slot_id <= EQ::invslot::TRADE_END; slot_id++) {
			EQ::ItemInstance* inst = m_inv.PopItem(slot_id);
			if(inst) {
				bool is_arrow = (inst->GetItem()->ItemType == EQ::item::ItemTypeArrow) ? true : false;
				int16 free_slot_id = m_inv.FindFreeSlot(inst->IsType(EQ::item::ItemClassBag), true, inst->GetItem()->Size, is_arrow);
				Log(Logs::Detail, Logs::Inventory, "Incomplete Trade Transaction: Moving %s from slot %i to %i", inst->GetItem()->Name, slot_id, free_slot_id);
				PutItemInInventory(free_slot_id, *inst, false);
				database.SaveInventory(account_id, character_id, nullptr, slot_id);
				safe_delete(inst);
			}
		}

		RemoveDuplicateLore(false);
		MoveSlotNotAllowed(false);

		BulkSendInventoryItems();
	}

	/*
	Weather Packet
	This shouldent be moved, this seems to be what the client
	uses to advance to the next state (sending ReqNewZone)
	*/
	outapp = new EQApplicationPacket(OP_Weather, sizeof(Weather_Struct));
	Weather_Struct* ws = (Weather_Struct*)outapp->pBuffer;
	ws->type = 0;
	ws->intensity = 0;

	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	instance_boot_grace_timer.Start();
	SetAttackTimer();
	conn_state = ZoneInfoSent;
	zoneinpacket_timer.Start();
	return;
}

void Client::Handle_Connect_OP_TargetMouse(const EQApplicationPacket *app)
{
	// This can happen if you tab while zoning. Just handle the opcode and return.
	return;
}

// connected opcode handlers

void Client::Handle_OP_AAAction(const EQApplicationPacket *app)
{
	//Action packet is always 256 bytes, but fields have varying positions.
	if (app->size != 256)
	{
		Log(Logs::Detail, Logs::Error, "Caught an invalid AAAction packet. Size is: %d", app->size);
		return;
	}

	if(Admin() < 95 && RuleB(Character, DisableAAs))
	{
		Message(Chat::Yellow, "Alternate Abilities are currently disabled. You will continue to use traditional experience.");
		return;
	}

	if (strncmp((char *)app->pBuffer, "on ", 3) == 0)
	{
		if (m_epp.perAA == 0)
			Message_StringID(Chat::White, AA_ON); //ON
		m_epp.perAA = atoi((char *)&app->pBuffer[3]);
		if (m_epp.perAA > 100u)
			m_epp.perAA = 100u;
		SendAAStats();
		SendAATable();
	}
	else if (strcmp((char *)app->pBuffer, "off") == 0)
	{
		if (m_epp.perAA > 0)
			Message_StringID(Chat::White, AA_OFF); //OFF
		m_epp.perAA = 0;
		SendAAStats();
		SendAATable();
	}
	else if (strncmp((char *)app->pBuffer, "buy ", 4) == 0)
	{
		AA_Action *action = (AA_Action *)app->pBuffer;
		int aa = atoi((char *)&app->pBuffer[4]);

		int emuaa = database.GetMacToEmuAA(aa);
		SendAA_Struct* aa2 = zone->FindAA(emuaa, false);

		if (aa2 == nullptr)
		{
			Log(Logs::Detail, Logs::Error, "Handle_OP_AAAction dun goofed. EQMacAAID is: %i, but no valid EmuAAID could be found.", aa);
			SendAATable(); // So client doesn't bug.
			SendAAStats();
			return;
		}

		uint32 cur_level = 0;
		if (aa2->id > 0)
			cur_level = GetAA(aa2->id);

		action->ability = emuaa + cur_level;
		action->action = 3;
		action->exp_value = m_epp.perAA;
		action->unknown08 = 0;

		Log(Logs::Detail, Logs::AA, "Buying: EmuaaID: %i, MacaaID: %i, action: %i, exp: %i", action->ability, aa, action->action, action->exp_value);
		BuyAA(action);
	}
	else if (strncmp((char *)app->pBuffer, "activate ", 9) == 0)
	{
		int aa = atoi((char *)&app->pBuffer[9]);

		aaID activate = (aaID)database.GetMacToEmuAA(aa);

		if (GetBoatNPCID() > 0)
		{
			ResetAATimer(activate, TOO_DISTRACTED);
			return;
		}

		Log(Logs::Detail, Logs::AA, "Activating AA %d", activate);
		ActivateAA(activate);
	}

	return;
}

void Client::Handle_OP_Animation(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Animation_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized "
			"OP_Animation: got %d, expected %d", app->size,
			sizeof(Animation_Struct));
		DumpPacket(app);
		return;
	}

	Animation_Struct *s = (Animation_Struct *)app->pBuffer;

	//might verify spawn ID, but it wouldent affect anything
	DoAnimation action = static_cast<DoAnimation>(s->action);
	DoAnim(action, s->value);

	return;
}

void Client::Handle_OP_ApplyPoison(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(ApplyPoison_Struct))
	{
		Log(Logs::General, Logs::Error, "Wrong size: OP_ApplyPoison, size=%i, expected %i", app->size, sizeof(ApplyPoison_Struct));
		return;
	}

	uint32 ApplyPoisonSuccessResult = 0;
	ApplyPoison_Struct* ApplyPoisonData = (ApplyPoison_Struct*)app->pBuffer;
	const EQ::ItemInstance* PrimaryWeapon = GetInv().GetItem(EQ::invslot::slotPrimary);
	const EQ::ItemInstance* PoisonItemInstance = GetInv()[ApplyPoisonData->inventorySlot];

	bool IsPoison = PoisonItemInstance != nullptr && (PoisonItemInstance->GetItem()->ItemType == EQ::item::ItemTypePoison && PoisonItemInstance->GetCharges() > 0);

	if (!IsPoison)
	{
		Log(Logs::General, Logs::Skills, "Item %s used to cast spell effect from a poison item was missing from inventory slot %d after casting, or is not a poison! Item type is %d", PoisonItemInstance->GetItem()->Name, ApplyPoisonData->inventorySlot, PoisonItemInstance->GetItem()->ItemType);
		Message_StringID(Chat::White, ITEM_OUT_OF_CHARGES);
	}
	else if (GetClass() == Class::Rogue)
	{
		if (PrimaryWeapon && PrimaryWeapon->GetItem()->ItemType == EQ::item::ItemType1HPiercing)
		{
			// this is a guess.  The Safehouse poison FAQ says "once you get over 100, you will very rarely fail to apply a poison"
			float SuccessChance = std::min((GetSkill(EQ::skills::SkillApplyPoison) + GetLevel()) / 200.0f, 0.95f);
			if (GetAA(aaPoisonMastery))
				SuccessChance = 1.0f;
			double ChanceRoll = zone->random.Real(0.0, 1.0);

			if (ChanceRoll < SuccessChance) {
				ApplyPoisonSuccessResult = 1;
				poison_spell_id = PoisonItemInstance->GetItem()->Proc.Effect;
			}

			uint8 success = ApplyPoisonSuccessResult ? SKILLUP_SUCCESS : SKILLUP_FAILURE;
			CheckIncreaseSkill(EQ::skills::SkillApplyPoison, nullptr, zone->skill_difficulty[EQ::skills::SkillApplyPoison].difficulty, success);

			DeleteItemInInventory(ApplyPoisonData->inventorySlot, 1, true);

			Log(Logs::General, Logs::Skills, "Chance to Apply Poison was %f. Roll was %f. Result is %u.", SuccessChance, ChanceRoll, ApplyPoisonSuccessResult);
		}
		else
		{
			Log(Logs::General, Logs::Skills, "No piercing weapon found in primary slot to apply poison to.");
		}
	}

	auto outapp = new EQApplicationPacket(OP_ApplyPoison, nullptr, sizeof(ApplyPoison_Struct));
	ApplyPoison_Struct* ApplyPoisonResult = (ApplyPoison_Struct*)outapp->pBuffer;
	ApplyPoisonResult->success = ApplyPoisonSuccessResult;
	ApplyPoisonResult->inventorySlot = ApplyPoisonData->inventorySlot;

	FastQueuePacket(&outapp);
}

void Client::Handle_OP_Assist(const EQApplicationPacket *app)
{
	if (app->size != sizeof(EntityId_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_Assist expected %i got %i", sizeof(EntityId_Struct), app->size);
		return;
	}

	EntityId_Struct* eid = (EntityId_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetID(eid->entity_id);

	EQApplicationPacket* outapp = app->Copy();
	eid = (EntityId_Struct*)outapp->pBuffer;
	if (RuleB(Combat, AssistNoTargetSelf))
		eid->entity_id = GetID();
	else
		eid->entity_id = -1;
	if (entity && entity->IsMob()) {
		Mob *assistee = entity->CastToMob();
		if (assistee->GetTarget()) {
			Mob *new_target = assistee->GetTarget();
			if (new_target && (GetGM() ||
				Distance(m_Position, assistee->GetPosition()) <= ASSIST_RANGE)) {
				SetAssistExemption(true);
				eid->entity_id = new_target->GetID();
			}
		}
	}

	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_AutoAttack(const EQApplicationPacket *app)
{
	if (app->size != 4) {
		Log(Logs::General, Logs::Error, "OP size error: OP_AutoAttack expected:4 got:%i", app->size);
		return;
	}



	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "You cannot autoattack as a GM.");
		return;
	}

	if (app->pBuffer[0] == 0)
	{
		auto_attack = false;
		if (IsAIControlled())
			return;
		attack_timer.Disable();
		ranged_timer.Disable();
		attack_dw_timer.Disable();

		m_AutoAttackPosition = glm::vec4();
		m_AutoAttackTargetLocation = glm::vec3();
		aa_los_them_mob = nullptr;
	}
	else if (app->pBuffer[0] == 1)
	{
		auto_attack = true;
		auto_fire = false;
		if (IsAIControlled())
			return;
		SetAttackTimer();

		if (GetTarget())
		{
			aa_los_them_mob = GetTarget();
			m_AutoAttackPosition = GetPosition();
			m_AutoAttackTargetLocation = glm::vec3(aa_los_them_mob->GetPosition());
			los_status = CheckLosFN(aa_los_them_mob);
			los_status_facing = IsFacingMob(aa_los_them_mob);
		}
		else
		{
			m_AutoAttackPosition = GetPosition();
			m_AutoAttackTargetLocation = glm::vec3();
			aa_los_them_mob = nullptr;
			los_status = false;
			los_status_facing = false;
		}

		// Fading Memories sets the player invisible but without a buff.  Normally the client removes invis buffs when attacking so this is only for the edge case.
		if (invisible && !FindType(SE_Invisibility))
			CommonBreakInvisible();
	}
}

void Client::Handle_OP_BazaarSearch(const EQApplicationPacket *app)
{

	if (app->size == sizeof(BazaarSearch_Struct)) {

		if (zone->GetZoneID() != Zones::BAZAAR)
		{
			// Doing a bazaar search from outside the bazaar
			char hString[250];
			sprintf(hString, "Bazaar Search hack, search performed from outside bazaar.\nzone: %s:%d, loc: %.2f, %.2f, %.2f", ZoneName(GetZoneID()), GetZoneID(), GetX(), GetY(), GetZ());
		}

		BazaarSearch_Struct* bss = (BazaarSearch_Struct*)app->pBuffer;

		this->SendBazaarResults(bss->TraderID, bss->Class_, bss->Race, bss->ItemStat, bss->Slot, bss->Type,
			bss->Name, bss->MinPrice * 1000, bss->MaxPrice * 1000);
	}
	else if (app->size == sizeof(BazaarWelcome_Struct)) {

		BazaarWelcome_Struct* bws = (BazaarWelcome_Struct*)app->pBuffer;

		if (bws->Beginning.Action == BazaarWelcome)
			SendBazaarWelcome();
	}
	else {
		Log(Logs::General, Logs::Error, "Malformed BazaarSearch_Struct packet recieved ignoring...");
	}

	return;
}

void Client::Handle_OP_BazaarSearchCon(const EQApplicationPacket *app)
{
	SendBazaarWelcome();
	return;
}

void Client::Handle_OP_Begging(const EQApplicationPacket *app) 
{
	auto outapp = new EQApplicationPacket(OP_Begging, sizeof(BeggingResponse_Struct));
	BeggingResponse_Struct *brs = (BeggingResponse_Struct*)outapp->pBuffer;

	brs->target = GetTarget() ? GetTarget()->GetID() : 0;
	brs->begger = GetID();
	brs->skill = GetSkill(EQ::skills::SkillBegging);
	brs->Result = 0; // Default, Fail.
	brs->Amount = 0;

	if (!GetTarget() || GetTarget() == this || !GetTarget()->IsNPC() || GetTarget()->IsBoat())
	{
		Log(Logs::General, Logs::Skills, "Tried to beg from an invalid entity. No skillup check will occur.");
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (!p_timers.Expired(&database, pTimerBeggingPickPocket, false))
	{
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	p_timers.Start(pTimerBeggingPickPocket, 8);

	NPC* npc = nullptr;
	if (GetTarget() && GetTarget()->IsNPC())
		npc = GetTarget()->CastToNPC();

	if (npc && npc->GetSpecialAbility(SpecialAbility::HarmFromClientImmunity))
	{
		return;
	}

	if (npc && DistanceSquaredNoZ(m_Position, npc->GetPosition()) >= 2200)
		return;

	uint16 beg_skill = GetSkill(EQ::skills::SkillBegging);
	if (npc && !npc->IsPet() && zone->random.Int(1, 199) > beg_skill && zone->random.Roll(9))
	{
		if (npc->CanTalk())
		{
			uint8 stringchance = zone->random.Int(0, 1);
			int stringid = BEG_FAIL1;
			if (stringchance == 1)
				stringid = BEG_FAIL2;
			npc->Say_StringID(Chat::White, stringid);
		}

		npc->AddToHateList(this, 50);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	float ChanceToBeg = ((float)(beg_skill / 700.0f) + 0.15f) * 100;
	int RandomChance = zone->random.Int(0, 100);
	uint8 success = SKILLUP_FAILURE;
	if (RandomChance < ChanceToBeg)
	{
		success = SKILLUP_SUCCESS;
		if(npc && !npc->IsPet())
		{
			uint8 money[4];
			money[0] = npc->GetPlatinum();
			money[1] = npc->GetGold();
			money[2] = npc->GetSilver();
			money[3] = npc->GetCopper();
			if (beg_skill < 199)
				money[0] = 0;
			if (beg_skill < 150)
				money[1] = 0;
			if (beg_skill < 100)
				money[2] = 0;

			uint8 maxamt = (beg_skill / 70) + 1;
			if (maxamt < 1)
				maxamt = 1;

			uint8 mincoin = 0;
			if (beg_skill < 199)
				mincoin = 1;
			if (beg_skill < 150)
				mincoin = 2;
			if (beg_skill < 100)
				mincoin = 3;

			uint32 amt = zone->random.Int(1, maxamt);
			uint8 random_coin = zone->random.Int(mincoin, 3);

			uint8 beg_type = 3;
			if (money[random_coin] > 0 && amt > 0)
			{
				beg_type = random_coin;
			}
			else
			{
				beg_type = 4; //NPC has no coin.
				Log(Logs::General, Logs::Skills, "Beg was successful but the NPC has no coin.");
			}

			if (beg_type < 4 && beg_skill > zone->random.Int(1, 210))
			{
				Log(Logs::General, Logs::Skills, "Beg was successful and the NPC has coin. Type is: %d", beg_type);
				switch (beg_type)
				{
				case 0:
				{
					uint32 plat = npc->GetPlatinum();
					if (amt > plat)
						amt = plat;
					npc->SetPlatinum(plat - amt);
					AddMoneyToPP(0, 0, 0, amt, false);
					brs->Amount = amt;
					brs->Result = 1;
					break;
				}
				case 1:
				{
					uint32 gold = npc->GetGold();
					if (amt > gold)
						amt = gold;
					npc->SetGold(gold - amt);
					AddMoneyToPP(0, 0, amt, 0, false);
					brs->Amount = amt;
					brs->Result = 2;
					break;
				}
				case 2:
				{
					uint32 silver = npc->GetSilver();
					if (amt > silver)
						amt = silver;
					npc->SetSilver(silver - amt);
					AddMoneyToPP(0, amt, 0, 0, false);
					brs->Amount = amt;
					brs->Result = 3;
					break;
				}
				case 3:
				{
					uint32 copper = npc->GetCopper();
					if (amt > copper)
						amt = copper;
					npc->SetCopper(copper - amt);
					AddMoneyToPP(amt, 0, 0, 0, false);
					brs->Amount = amt;
					brs->Result = 4;
					break;
				}
				}
			}

			if (brs->Amount != 0 && brs->Result != 0)
				Message_StringID(Chat::White, BEG_SUCCESS, GetName());
		}
		else
		{

			Log(Logs::General, Logs::Skills, "Beg was successful but the NPC is a pet or other invalid type.");
			brs->Result = 0;
		}
	}
	else
	{
		Log(Logs::General, Logs::Skills, "Beg was unsuccessful. Skillup check will occur.");
	}

	QueuePacket(outapp);
	safe_delete(outapp);
	CheckIncreaseSkill(EQ::skills::SkillBegging, nullptr, zone->skill_difficulty[EQ::skills::SkillBegging].difficulty, success);
}

void Client::Handle_OP_Bind_Wound(const EQApplicationPacket *app) 
{
	if(!app)
		return;

	if (app->size != sizeof(BindWound_Struct)){
		Log(Logs::General, Logs::Error, "Size mismatch for Bind wound packet");
		DumpPacket(app);
	}

	BindWound_Struct* bind_in = (BindWound_Struct*)app->pBuffer;

	if(!bind_in)
		return;

	Mob* bindmob = entity_list.GetMob(bind_in->to);
	if (!bindmob){
		Log(Logs::General, Logs::Error, "Bindwound on non-exsistant mob from %s", this->GetName());
	}
	else {
		Log(Logs::General, Logs::Skills, "BindWound in: to:\'%s\' from=\'%s\'", bindmob->GetName(), GetName());
		BindWound(bindmob->GetID(), true);
	}
	return;
}

void Client::Handle_OP_BoardBoat(const EQApplicationPacket *app)
{

	if (app->size <= 7 || app->size > 32) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_BoardBoad. Expected greater than 7 no greater than 32, got %i", app->size);
		DumpPacket(app);
		return;
	}

	char boatname[32];
	memcpy(boatname, app->pBuffer, app->size);
	boatname[31] = '\0';

	Log(Logs::Detail, Logs::Boats, "%s is attempting to board boat %s", GetName(), boatname);

	Mob* boat = entity_list.GetMob(boatname);

	if (!boat || !boat->IsBoat())
		return;

	if (boat)
	{
		BuffFadeByEffect(SE_Levitate);
		BoatID = boat->GetID();	// set the client's BoatID to show that it's on this boat
		m_pp.boatid = boat->GetNPCTypeID(); //For EQMac's boat system.
		strcpy(m_pp.boat, boatname);

		std::string export_string = fmt::format("{}", boat->GetNPCTypeID());
		parse->EventPlayer(EVENT_BOARD_BOAT, this, export_string, 0);
	}

	return;
}

void Client::Handle_OP_Buff(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SpellBuffFade_Struct))
	{
		Log(Logs::General, Logs::Error, "Size mismatch in OP_Buff. expected %i got %i", sizeof(SpellBuffFade_Struct), app->size);
		DumpPacket(app);
		return;
	}
	SpellBuffFade_Struct* sbf = (SpellBuffFade_Struct*)app->pBuffer;
	uint32 spid = sbf->spellid;
	Log(Logs::General, Logs::Spells, "Client requested that buff with spell id %d be canceled. bardmodifier: %d activated: %d slot_number: %d bufftype: %d duration: %d level: %d", spid, sbf->bard_modifier, sbf->activated, sbf->slot_number, sbf->bufftype, sbf->duration, sbf->level);

	if (!IsValidSpell(spid))
		return;

	if (spid == 0xFFFF)
		QueuePacket(app);
	else if(IsValidSpell(spid) && spells[spid].goodEffect != 0 && spells[spid].persist_through_death == 0)
		BuffFadeBySpellID(spid);
	else if(IsValidSpell(spid) && !IsDead())
	{

		int buff_count = GetMaxTotalSlots();
		for (int j = 0; j < buff_count; j++)
		{
			if (buffs[j].spellid == spid && buffs[j].ticsremaining > 1)
			{

				// we need to remove the buff and reapply it in the first empty slot to stay in sync with client
				Buffs_Struct savedbuff = Buffs_Struct(buffs[j]); // save a copy before removing it

				// Don't use BuffFadeBySlot() as that clears client statuses.
				// We are reapplying the buff, so we want to keep their status the same.
				buffs[j].spellid = SPELL_UNKNOWN;

				// reapply buff server-side
				int emptyslot = -1;
				FindAffectSlot(this, spid, &emptyslot, 1);
				buffs[emptyslot] = savedbuff;
				CalcBonuses();

				// reapply buff client-side
				auto action_packet = new EQApplicationPacket(OP_Action, sizeof(Action_Struct));
				Action_Struct* action = (Action_Struct*)action_packet->pBuffer;
				action->target = GetID();
				action->source = GetID();
				action->level = buffs[emptyslot].casterlevel;	// effective level, used for potions
				action->type = 231;	// 231 means a spell
				action->spell = spid;
				action->sequence = (GetHeading() * 2.0f);	// heading
				action->instrument_mod = buffs[emptyslot].instrumentmod;
				action->buff_unknown = 0x04;	// this is a success flag
				
				QueuePacket(action_packet);
				safe_delete(action_packet);
				SendBuffDurationPacket(spid, buffs[emptyslot].ticsremaining, buffs[emptyslot].casterlevel, emptyslot, buffs[emptyslot].instrumentmod);
			}
		}
	}
	return;
}

void Client::Handle_OP_Bug(const EQApplicationPacket *app)
{
	if (!RuleB(Bugs, ReportingSystemActive)) {
		Message(Chat::White, "Bug reporting is disabled on this server.");
		return;
	}

	if (app->size != sizeof(BugReport_Struct) && app->size != sizeof(IntelBugStruct))
	{
		Log(Logs::General, Logs::Error, "Wrong size of BugStruct got %d expected %zu!\n", app->size, sizeof(BugReport_Struct));
		DumpPacket(app);
	}
	else
	{
		BugReport_Struct* bug_report;
		if (app->size == sizeof(IntelBugStruct))
		{
			bug_report = new struct BugReport_Struct;
			IntelBugStruct* intelbug = (IntelBugStruct*)app->pBuffer;
			strcpy(bug_report->reporter_name, intelbug->reporter_name);
			strcpy(bug_report->bug_report, intelbug->bug_report);
			bug_report->pos_x = GetX();
			bug_report->pos_y = GetY();
			bug_report->pos_z = GetZ();
			bug_report->heading = GetHeading();
			memset(bug_report->target_name,0,64);
			if(GetTarget())
				strncpy(bug_report->target_name,GetTarget()->GetName(),64);
		}
		else if(app->size == sizeof(BugReport_Struct))
		{
			bug_report = (BugReport_Struct*)app->pBuffer;
		}
		uint32 clienttype = ClientVersionBit();
		database.UpdateBug(bug_report, clienttype);
		Message(Chat::Yellow, "Thank you for the bug submission, %s.", bug_report->reporter_name);
	}
	return;
}

void Client::Handle_OP_Camp(const EQApplicationPacket *app) 
{
	uint32 option = 0;
	if (app->size == 4) {
		option = (uint32)app->pBuffer[0];
	}

	if (GetBoatNPCID() > 0)
	{
		Stand();
		return;
	}

	if (GetGM())
	{
		OnDisconnect(true);
		return;
	}

	camping = true;

	if (option == 1)
		camp_desktop = true;
	else
		camp_desktop = false;
	camp_timer.Start(35000, true);
	return;
}

void Client::Handle_OP_CancelTrade(const EQApplicationPacket *app)
{
	if (app->size != sizeof(CancelTrade_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_CancelTrade, size=%i, expected %i", app->size, sizeof(CancelTrade_Struct));
		return;
	}

	if (!trade)
		return;

	Mob* with = trade->With();
	if (with && with->IsClient()) {
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;

		// Forward cancel packet to other client
		msg->fromid = GetID();
		//msg->action = 1;

		with->CastToClient()->FinishTrade(with);
		with->CastToClient()->trade->Reset();
		with->CastToClient()->QueuePacket(app);

		// Put trade items/cash back into inventory
		QueuePacket(app);
		FinishTrade(this);
		trade->Reset();
	}
	else if (with){
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;
		msg->fromid = with->GetID();
		QueuePacket(app);

		auto outapp = new EQApplicationPacket(OP_TradeReset, 0);
		QueuePacket(outapp);
		safe_delete(outapp);

		FinishTrade(this);
		trade->Reset();
	}
	else
	{
		//EQMac sends a second CancelTrade packet. Since "with" and the trade became invalid the first time around, this handles the second which prevents the client from bugging.
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;
		msg->fromid = GetID();
		QueuePacket(app);
		Log(Logs::Detail, Logs::Debug, "Cancelled second trade. from is: %i.", msg->fromid);
	}

	return;
}

void Client::Handle_OP_CastSpell(const EQApplicationPacket *app) 
{
	using EQ::spells::CastingSlot;
	if (app->size != sizeof(CastSpell_Struct)) {
		std::cout << "Wrong size: OP_CastSpell, size=" << app->size << ", expected " << sizeof(CastSpell_Struct) << std::endl;
		return;
	}

	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}
	
	CastSpell_Struct *castspell = (CastSpell_Struct *)app->pBuffer;
	if (!IsValidSpell(castspell->spell_id) || castspell->slot > 10)
	{
		Log(Logs::General, Logs::Spells, "OP CastSpell error: Invalid cast packet from player %s! slot=%d, spell=%d, target=%d, inv=%lx", GetName(), castspell->slot, castspell->spell_id, castspell->target_id, (unsigned long)castspell->inventoryslot);
		return;
	}

	if (IsAIControlled() && !has_zomm) {
		this->Message_StringID(Chat::Red, NOT_IN_CONTROL);
		return;
	}

	if (castspell->spell_id == SPELL_MANA_CONVERT && RestictedManastoneClick(zone->GetZoneID())) {
		Message_StringID(Chat::Red, SPELL_DOES_NOT_WORK_HERE);
		InterruptSpell(castspell->spell_id);
		return;
	}

	// Client sends a packet to fade invisible spells before it sends this packet to the server.  
	CommonBreakSneakHide();
	if (IsFeigned())
	{
		SetFeigned(false);
		Message_StringID(Chat::SpellFailure, FEIGN_BROKEN_CAST);
	}

	clicky_override = false;

	Log(Logs::General, Logs::Spells, "OP CastSpell: slot=%d, spell=%d, target=%d, inv=%lx", castspell->slot, castspell->spell_id, castspell->target_id, (unsigned long)castspell->inventoryslot);
	CastingSlot slot = static_cast<CastingSlot>(castspell->slot);

	if (m_pp.boatid != 0)
	{
		InterruptSpell(castspell->spell_id);
		return;
	}

	if (spells[castspell->spell_id].disabled && !GetGM())
	{
		InterruptSpell(castspell->spell_id);
		return;
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "You cannot cast spells as a GM.");
		InterruptSpell(castspell->spell_id);
		return;
	}

	if (Admin() > 0 && IsValidSpell(castspell->spell_id)) {
		Mob* SpellTarget = entity_list.GetMob(castspell->target_id);
		char szArguments[512];
		sprintf(szArguments, "ID %i (%s), Slot %i, InvSlot %i", castspell->spell_id, spells[castspell->spell_id].name, castspell->slot, castspell->inventoryslot);
		QServ->QSLogCommands(this, "spell", szArguments, SpellTarget);
	}


	/* Memorized Spell */
	if (m_pp.mem_spells[castspell->slot] && m_pp.mem_spells[castspell->slot] == castspell->spell_id)
	{
		uint16 spell_to_cast = 0;
		if (castspell->slot < MAX_PP_MEMSPELL) {
			spell_to_cast = m_pp.mem_spells[castspell->slot];
			if (spell_to_cast != castspell->spell_id) {
				InterruptSpell(castspell->spell_id); //CHEATER!!!
				return;
			}
		}
		else if (castspell->slot >= MAX_PP_MEMSPELL) {
			InterruptSpell();
			return;
		}

		CastSpell(spell_to_cast, castspell->target_id, slot);
	}
	/* Item Spell Slot */
	else if ((slot == CastingSlot::Item))	// ITEM cast
	{
		// limit the recast rate of instant clickies slightly
		if (clickyspellrecovery_timer.Check(false))
		{
			clickyspellrecovery_timer.Disable();
			clickyspellrecovery_burst = 0;
		}
		if (++clickyspellrecovery_burst > 4)
		{
			SendSpellBarEnable(castspell->spell_id);
			return;
		}
		if(!clickyspellrecovery_timer.Enabled())
		{
			clickyspellrecovery_timer.Start(1000, true);
		}

		if (m_inv.SupportsClickCasting(castspell->inventoryslot))	// sanity check
		{
			// Check for Mod Rod recast time.
			if (castspell->spell_id == SPELL_MODULATION && !p_timers.Expired(&database, pTimerModulation))
			{
				InterruptSpell(SPELL_RECAST, Chat::SpellFailure, castspell->spell_id);
				return;
			}

			if (castspell->spell_id == SPELL_MANA_CONVERT && !zone->AllowManastoneClick())
			{
				Message_StringID(Chat::Red, SPELL_DOES_NOT_WORK_HERE);
				InterruptSpell(SPELL_RECAST, Chat::SpellFailure, castspell->spell_id);
				return;
			}

			// packet field types will be reviewed as packet transistions occur -U
			const EQ::ItemInstance* inst = m_inv[castspell->inventoryslot]; //slot values are int16, need to check packet on this field
			//bool cancast = true;
			if (inst && inst->IsType(EQ::item::ItemClassCommon))
			{
				const EQ::ItemData* item = inst->GetItem();
				if (item->Click.Effect != (uint32)castspell->spell_id)
				{
					//database.SetMQDetectionFlag(account_name, name, "OP_CastSpell with item, tried to cast a different spell.", zone->GetShortName());
					InterruptSpell(castspell->spell_id);	//CHEATER!!
					return;
				}

				if ((item->Click.Type == EQ::item::ItemEffectClick) || (item->Click.Type == EQ::item::ItemEffectExpendable) || (item->Click.Type == EQ::item::ItemEffectEquipClick) || (item->Click.Type == EQ::item::ItemEffectClick2))
				{
					int32 casttime_ = item->CastTime_ != 0 && zone->GetGuildID() != 0 && zone->GetZoneExpansion() == content_service.GetCurrentExpansion()
						? item->CastTime_ // cast time override for current expansion instances
						: item->CastTime; // normal cast time
					// Clickies with 0 casttime and expendable items had no level or regeant requirement on AK. Also, -1 casttime was instant cast.
					if(casttime_ <= 0 || inst->IsExpendable())
					{
						clicky_override = true;
						if(casttime_ < 0)
							casttime_ = 0;
					}

					if (item->Click.Level2 > 0)
					{
						if (GetLevel() >= item->Click.Level2 || item->Click.Type == EQ::item::ItemEffectClick || clicky_override)
						{
							EQ::ItemInstance* p_inst = (EQ::ItemInstance*)inst;
							int i = parse->EventItem(EVENT_ITEM_CLICK_CAST, this, p_inst, nullptr, "", castspell->inventoryslot);

							if (i == 0) {
								CastSpell(item->Click.Effect, castspell->target_id, slot, casttime_, 0, 0, castspell->inventoryslot);
							}
							else {
								InterruptSpell(castspell->spell_id);
								return;
							}
						}
						else
						{
							database.SetMQDetectionFlag(account_name, name, "OP_CastSpell with item, did not meet req level.", zone->GetShortName());
							Message(Chat::White, "Error: level not high enough.", castspell->inventoryslot);
							InterruptSpell(castspell->spell_id);
						}
					}
					else
					{
						EQ::ItemInstance* p_inst = (EQ::ItemInstance*)inst;
						int i = parse->EventItem(EVENT_ITEM_CLICK_CAST, this, p_inst, nullptr, "", castspell->inventoryslot);

						if (i == 0) {
							CastSpell(item->Click.Effect, castspell->target_id, slot, casttime_, 0, 0, castspell->inventoryslot);
						}
						else {
							InterruptSpell(castspell->spell_id);
							return;
						}
					}
				}
				else
				{
					Message(Chat::White, "Error: unknown item->Click.Type (0x%02x)", item->Click.Type);
				}
			}
			else
			{
				Message(Chat::White, "Error: item not found in inventory slot #%i", castspell->inventoryslot);
				InterruptSpell(castspell->spell_id);
			}
		}
		else
		{
			Message(Chat::White, "Error: castspell->inventoryslot >= %i (0x%04x)", EQ::invslot::slotCursor, castspell->inventoryslot);
			InterruptSpell(castspell->spell_id);
		}
	}
	else if (slot == CastingSlot::Ability) {	// ABILITY cast (LoH and Harm Touch)
		uint16 spell_to_cast = 0;

		//Reuse timers are handled by SpellFinished()
		if (castspell->spell_id == SPELL_LAY_ON_HANDS && GetClass() == Class::Paladin)
		{
			spell_to_cast = SPELL_LAY_ON_HANDS;
		}
		else if (castspell->spell_id == SPELL_HARM_TOUCH && GetClass() == Class::ShadowKnight) 
		{
			if (HasInstantDisc(castspell->spell_id))
				spell_to_cast = SPELL_HARM_TOUCH2;		// unholy aura uses disease resist version, unless it's the AA skill, which is unresistable
			else
				spell_to_cast = castspell->spell_id;
		}

		// if we've matched LoH or HT, cast now
		if (spell_to_cast > 0)	
		{
			CastSpell(spell_to_cast, castspell->target_id, slot);

			if(HasInstantDisc(spell_to_cast))
			{
				FadeDisc();
			}
			SendAATimers();
		}
	}
	else	// MEMORIZED SPELL (first confirm that it's a valid memmed spell slot, then validate that the spell is currently memorized)
	{
		uint16 spell_to_cast = 0;

		if (castspell->slot < MAX_PP_MEMSPELL)
		{
			spell_to_cast = m_pp.mem_spells[castspell->slot];
			if (spell_to_cast != castspell->spell_id)
			{
				InterruptSpell(castspell->spell_id); //CHEATER!!!
				return;
			}
		}
		else if (castspell->slot >= MAX_PP_MEMSPELL) {
			InterruptSpell();
			return;
		}

		CastSpell(spell_to_cast, castspell->target_id, slot);
	}
	return;
}

void Client::Handle_OP_ChannelMessage(const EQApplicationPacket *app)
{
	auto* cm = (ChannelMessage_Struct*) app->pBuffer;

	if (app->size < sizeof(ChannelMessage_Struct)) {
		LogDebug("Size mismatch in OP_ChannelMessage expected [{}] got [{}]", sizeof(ChannelMessage_Struct), app->size);
		return;
	}

	// the client does not process these strings cleanly and if it doesn't crash from it first, it will send the garbage to the server and cause a crash here.
	cm->targetname[63] = 0;
	cm->sender[63] = 0;
	if (app->size > 2184) // size is 136 + strlen(message) + 1 but the client adds an extra 4 bytes to the length when it sends the packet.
		cm->message[2047] = 0; // the client also does this but only for this message field

	uint8 language_skill = Language::MaxValue;
	if (EQ::ValueWithin(cm->language, Language::CommonTongue, Language::Unknown26)) {
		language_skill = m_pp.languages[cm->language];
	}
	ChannelMessageReceived(cm->chan_num, cm->language, language_skill, cm->message, cm->targetname);
	return;
}

void Client::Handle_OP_ClickDoor(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClickDoor_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_ClickDoor, size=%i, expected %i", app->size, sizeof(ClickDoor_Struct));
		return;
	}
	ClickDoor_Struct* cd = (ClickDoor_Struct*)app->pBuffer;
	Doors* currentdoor = entity_list.FindDoor(cd->doorid);
	if (!currentdoor)
	{
		Message(Chat::White, "Unable to find door, please notify a GM (DoorID: %i).", cd->doorid);
		return;
	}

	std::string  export_string = fmt::format("{}", cd->doorid);
	std::vector<std::any> args;
	args.push_back(currentdoor);
	parse->EventPlayer(EVENT_CLICK_DOOR, this, export_string, 0, &args);

	if (!currentdoor->IsMoveable() || (currentdoor->IsTeleport() && currentdoor->GetOpenType() == 57))
	{
		Log(Logs::General, Logs::Doors, "%s clicked a door that does not open. Returning.", GetName());
	}
	else
	{
		currentdoor->HandleClick(this, 0);
	}
	return;
}

void Client::Handle_OP_ClickObject(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(ClickObject_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size on ClickObject_Struct: Expected %i, Got %i",
			sizeof(ClickObject_Struct), app->size);
		return;
	}

	auto* click_object = (ClickObject_Struct*)app->pBuffer;
	auto* entity = entity_list.GetID(click_object->drop_id);
	if (entity && entity->IsObject()) {
		Object* object = entity->CastToObject();
		
		std::string msg;
		if (RuleB(Quarm, RestrictIksarsToKunark))
		{
			if(GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
				msg = "You're not allowed to pick up dropped items if you're an Iksar in classic zones right now.";
			else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
			{
				msg = "You're not allowed to pick up any items if you're a non-Iksar in Kunark zones right now.";
			}
		}
		
		if (object->IsPlayerDrop())
		{
			if (Admin() > 0)
			{
				msg = "You cannot pick up dropped player items because you're a GM and that would make the players around you a sad panda.";
			}
			else if (IsSelfFoundAny())
			{
				// If the client is self found or solo, don't allow them to pick up the item, unless they are the one that dropped it
				// Also make sure they dropped it while SSF
				if(object->GetCharacterDropperID() != this->CharacterID())
				{
					msg = "You cannot pick up dropped items because you are performing a self found or solo challenge.";
				}
				else if(!object->IsSSFRuleSet())
				{
					msg = "You cannot pick up dropped items from yourself before you accepted the challenge.";
				}
				// The else case does not set a msg because they are allowed to pick it up if they dropped it
			}
			if (!msg.empty())
			{
				Message(Chat::Red, msg.c_str());
				auto outapp = new EQApplicationPacket(OP_ClickObject, sizeof(ClickObject_Struct));
				ClickObject_Struct* loreitem = (ClickObject_Struct*)outapp->pBuffer;
				loreitem->player_id = click_object->player_id;
				loreitem->drop_id = 0xFFFFFFFF;
				QueuePacket(outapp);
				safe_delete(outapp);
				return;
			}
		}
		
		object->HandleClick(this, click_object);

		std::vector<std::any> args;
		args.push_back(object);

		std::string export_string = fmt::format("{}", click_object->drop_id);
		parse->EventPlayer(EVENT_CLICK_OBJECT, this, export_string, 0, &args);
	}

	return;
}

void Client::Handle_OP_ClickObjectAction(const EQApplicationPacket *app)
{

	if (app->size != sizeof(ClickObjectAction_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size on OP_ClickObjectAction: Expected %i, Got %i",
			sizeof(ClickObjectAction_Struct), app->size);
		return;
	}

	ClickObjectAction_Struct* oos = (ClickObjectAction_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetEntityObject(oos->drop_id);
	if (entity && entity->IsObject()) {
		Object* object = entity->CastToObject();
		if (oos->open == 0) {
			object->Close();
		}
		else {
			Log(Logs::General, Logs::Error, "Unsupported action %d in OP_ClickObjectAction", oos->open);
		}
	}
	else {
		Log(Logs::Detail, Logs::Error, "Invalid object %d in OP_ClickObjectAction", oos->drop_id);
	}


	SetTradeskillObject(nullptr);

	EQApplicationPacket end_trade1(OP_ClickObjectAction, 0);
	QueuePacket(&end_trade1);

	return;
}

void Client::Handle_OP_ClientError(const EQApplicationPacket *app)
{
	return;
}

void Client::Handle_OP_ClientUpdate(const EQApplicationPacket *app)
{
	if (IsAIControlled() && !has_zomm)
		return;

	if(dead)
		return;

	//currently accepting two sizes, one has an extra byte on the end
	if (app->size != sizeof(SpawnPositionUpdate_Struct)
	&& app->size != (sizeof(SpawnPositionUpdate_Struct)+1)
	) {
		Log(Logs::General, Logs::Error, "OP size error: OP_ClientUpdate expected:%i got:%i", sizeof(SpawnPositionUpdate_Struct), app->size);
		return;
	}
	SpawnPositionUpdate_Struct* ppu = (SpawnPositionUpdate_Struct*)app->pBuffer;

	if(ppu->spawn_id != GetID()) 
	{
		if(has_zomm)
		{
			NPC* zommnpc = nullptr;
			if(entity_list.GetZommPet(this, zommnpc))
			{
				if(zommnpc && zommnpc->GetHP() > 0)
				{
					auto outapp = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
					SpawnPositionUpdates_Struct* ppus = (SpawnPositionUpdates_Struct*)outapp->pBuffer;
					zommnpc->SetSpawnUpdate(ppu, &ppus->spawn_update);
					ppus->num_updates = 1;
					entity_list.QueueCloseClients(zommnpc, outapp, true, 500, this, false);
					safe_delete(outapp);

					auto zommPos = glm::vec4(ppu->x_pos, ppu->y_pos, (float)ppu->z_pos / 10.0f, ppu->heading);
					entity_list.OpenDoorsNearCoords(zommnpc, zommPos);
					zommnpc->GMMove(ppu->x_pos, ppu->y_pos, (float)ppu->z_pos / 10.0f, EQ19toFloat(ppu->heading), false);
					return;
				}
			}
		}
		// check if the id is for a boat the player is controlling
		else if (ppu->spawn_id == BoatID) 
		{
			Mob* boat = entity_list.GetMob(BoatID);
			if (!boat || !boat->IsBoat()) 
			{	// if the boat ID is invalid, reset the id and abort
				BoatID = 0;
				return;
			}

			auto outapp = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
			SpawnPositionUpdates_Struct* ppus = (SpawnPositionUpdates_Struct*)outapp->pBuffer;
			boat->SetSpawnUpdate(ppu, &ppus->spawn_update);
			ppus->num_updates = 1;
			entity_list.QueueCloseClients(boat, outapp, true, 1000, this, false);
			safe_delete(outapp);

			boat->GMMove(ppu->x_pos, ppu->y_pos, ppu->z_pos / 10.f, boat->GetHeading(), false);
			return;
		}
		else return;	// if not a boat, do nothing
	}

	// Treeform spells require trees nearby to cast. Unfortunately, if there are no trees the client will quietly
	// block the spell and not tell the server. Because the server has no knowledge of where trees are on the map,
	// it allows the buff through. The player gets the server side benefit of the spell without being rooted.
	// This will quietly strip treeform spells as soon as the player moves, to get around this issue until we can
	// get tree data into our maps and implement treeform properly. This probably will also fade the spell on
	// intrazone ports and spells, but it will do for now.
	// 
	// this is a workaround for minor illusion and tree form and should be removed once we can emulate CDisplay::GetNearestActorTag()
	// note that this isn't perfect and can leave the client buffs desynced but it attempts to prevent exploiting minor illusion to move around while avoiding NPC aggro.
	// if a client casts minor illusion then gets a buff on them before moving, they will be in a bad state and need to zone to reload buffs but we aren't forcing that here
	// TODO: the proper fix is to know about zone objects and predict when the client will fail to illusion, to keep buffs in sync
	if(GetRace() == Race::Tree || GetRace() == Race::MinorIllusion)
	{ 
		if ((ppu->x_pos != m_Position.x && (ppu->x_pos > m_Position.x + 1 || ppu->x_pos < m_Position.x - 1)) || 
			(ppu->y_pos != m_Position.y && (ppu->y_pos > m_Position.y + 1 || ppu->y_pos < m_Position.y - 1)))
		{
			Log(Logs::General, Logs::Spells, "Fading Treeform/Minor Illusion due to position change. Pos Update: %d, %d. Current Pos: %0.2f, %0.2f", ppu->x_pos, ppu->y_pos, m_Position.x, m_Position.y);
			BuffFadeByEffect(SE_Illusion);
		}
	}

	// Send our original position to others so we don't disappear.
	if(has_zomm)
	{
		SendPosUpdate();
		return;
	}

	auto current_update_time = std::chrono::high_resolution_clock::now();
	auto timeDiff = std::chrono::duration<double>(current_update_time - last_position_update_time);
	
	// client does crappy floor function below 0
	if (ppu->x_pos < 0)
		ppu->x_pos--;
	if (ppu->y_pos < 0)
		ppu->y_pos--;
	if (ppu->z_pos < 0) // this will stop clients from bouncing
		ppu->z_pos --;

	m_EQPosition = glm::vec4(ppu->x_pos, ppu->y_pos, (float)ppu->z_pos/10.0f, ppu->heading);

	if (GetZoneID() == Zones::AIRPLANE && m_EQPosition.z < -1200.0f) {
		MovePCGuildID(Zones::FREPORTE, GUILD_NONE, -1570.0f, -25.0f, 20.0f, 231.0f);
		return;
	}

	if (IsDraggingCorpse())
		DragCorpses();

	//Check to see if PPU should trigger an update to the rewind position.
	float rewind_x_diff = 0;
	float rewind_y_diff = 0;

	rewind_x_diff = ppu->x_pos - m_RewindLocation.x;
	rewind_x_diff *= rewind_x_diff;
	rewind_y_diff = ppu->y_pos - m_RewindLocation.y;
	rewind_y_diff *= rewind_y_diff;

	//We only need to store updated values if the player has moved.
	//If the player has moved more than units for x or y, then we'll store
	//his pre-PPU x and y for /rewind, in case he gets stuck.
	if ((rewind_x_diff > 750) || (rewind_y_diff > 750))
		m_RewindLocation = glm::vec3(m_Position);

	//If the PPU was a large jump, such as a cross zone gate or Call of Hero,
	//just update rewind coords to the new ppu coords. This will prevent exploitation.

	if ((rewind_x_diff > 5000) || (rewind_y_diff > 5000))
		m_RewindLocation = glm::vec3(ppu->x_pos, ppu->y_pos, (float)ppu->z_pos / 10.0f);

	glm::vec4 newPosition(ppu->x_pos, ppu->y_pos, ppu->z_pos / 10.0f, ppu->heading);
	bool bSkip = false;
	if (m_LastLocation == glm::vec4())
	{
		m_LastLocation = newPosition;
		m_Position.x = newPosition.x;
		m_Position.y = newPosition.y;
		m_Position.z = newPosition.z;
		m_Position.w = newPosition.w;

		bSkip = true;
	}		

	if (RuleB(Quarm, EnableProjectSpeedie))
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		double dist = DistanceNoZ(m_LastLocation, newPosition);
		double distFromExpected = DistanceNoZ(ExpectedRewindPos, newPosition);

		bool shouldTryHackCheck = !exemptHackCount;
		auto seconds = std::chrono::duration<double>(currentTime - last_position_update_time);
		auto seccount = seconds.count();
		auto distDivTime = 0.;


		float distFromZonePointThreshold = RuleR(Quarm, SpeedieDistFromZonePointThreshold);
		float distFromBoatThreshold = RuleR(Quarm, SpeedieDistFromBoatThreshold);

		float distThreshold = RuleR(Quarm, SpeedieDistThreshold);
		float secondElapsedThreshold = RuleR(Quarm, SpeedieSecondElapsedThreshold);
		float speedieDistFromExpectedThreshold = RuleR(Quarm, SpeedieDistFromExpectedThreshold);
		if (dist > distThreshold && seccount >= secondElapsedThreshold)
		{
			distDivTime = dist / seccount;
		}

		/*
			If the PPU was a large jump, such as a cross zone gate or Call of Hero,
				just update rewind coordinates to the new ppu coordinates. This will prevent exploitation.
		*/

		bool is_exempt_correct = false;

		if (distFromExpected < speedieDistFromExpectedThreshold && ExpectedRewindPos != glm::vec3(0, 0, 0) && !bSkip)
		{
			is_exempt_correct = true;
			ExpectedRewindPos = glm::vec3(0, 0, 0);
		}
		auto speed = GetRunspeed();
		float distDivTimeThreshold = RuleR(Quarm, SpeedieSlowerDistDivTime);
		float distDivTimeHighSpeedThreshold = RuleR(Quarm, SpeedieHigherDistDivTime);
		float distDivTimeBardSpeedThreshold = RuleR(Quarm, SpeedieBardDistDivTime);

		float highSpeedValueThreshold = RuleR(Quarm, SpeedieHighSpeedThreshold);
		float bardSpeedValueThreshold = RuleR(Quarm, SpeedieBardSpeedThreshold);

		if (speed >= highSpeedValueThreshold)
			distDivTimeThreshold = distDivTimeHighSpeedThreshold;

		if (speed >= bardSpeedValueThreshold)
			distDivTimeThreshold = distDivTimeBardSpeedThreshold;

		bool within_previous_zone_point = false;

		bool has_boat = false;

		if (BoatID || m_pp.boat[0])
			has_boat = true;

		if (distDivTime >= distDivTimeThreshold && !is_exempt_correct && !GetGM() && !dead && client_state == CLIENT_CONNECTED && !has_boat)
		{

			ZonePoint* previous_zone_point = zone->GetClosestZonePointSameZone(m_LastLocation.x, m_LastLocation.y, m_LastLocation.z, this, distFromZonePointThreshold);
			ZonePoint* current_zone_point = zone->GetClosestTargetZonePointSameZone(newPosition.x, newPosition.y, newPosition.z, this, distFromZonePointThreshold);

			if (!previous_zone_point && !current_zone_point)
			{

				std::string warped = std::string(GetCleanName()) + " - entity moving too fast: dist: " + std::to_string(dist) + ", distDivTime: " + std::to_string(distDivTime) + "playerSpeed: " + std::to_string(speed);
				worldserver.SendEmoteMessage(0, 0, 100, Chat::Default, "%s - entity moving too fast: %lf %lf - is_exempt_correct %s, playerSpeed %lf", GetCleanName(), dist, distDivTime, std::to_string(is_exempt_correct).c_str(), speed);
				database.SetHackerFlag(this->account_name, this->name, warped.c_str());
			}
		}
	}

	if(proximity_timer.Check()) {
		entity_list.ProcessMove(this, glm::vec3(ppu->x_pos, ppu->y_pos, (float)ppu->z_pos/10.0f));

		m_Proximity = glm::vec3(ppu->x_pos, ppu->y_pos, (float)ppu->z_pos/10.0f);
	}
	bool update_all = position_timer_counter > 24;

	// Update internal state
	m_Delta = glm::vec4(ppu->delta_yzx.GetX(), ppu->delta_yzx.GetY(), ppu->delta_yzx.GetZ(), (float)ppu->delta_heading);

	uint8 sendtoself = 0;
	// solar: heading range is 512 but divided by 2 to fit into a byte for transferring in the spawn and position update packets.
	// it should be multiplied by 2 here on receipt and divided by 2 on the way out when sending position updates and newspawn packets.
	// because this wasn't being done, there are 256 based headings being stored and used in many places.  it will take some work but this
	// should be cleaned up eventually so that all of these workarounds can be removed
	//m_Position.w = ppu->heading * 2.0f;
	m_Position.w = ppu->heading;

	//Adjust the heading for the Paineel portal near the SK guild
	if(zone->GetZoneID() == Zones::PAINEEL && 
		((GetX() > 519 && GetX() < 530) || (GetY() > 952 && GetY() < 965)))
	{
		float tmph = GetPortHeading(ppu->x_pos, ppu->y_pos);
		if(tmph > 0)
		{
			m_Position.w = tmph;
			sendtoself = 1;
		}

	}
	
	// Break Hide, Trader, feign, and fishing mode if moving and set rewind timer
	if(ppu->y_pos != m_Position.y || ppu->x_pos != m_Position.x)
	{
		if((hidden || improved_hidden) && !sneaking)
		{
			CommonBreakSneakHide();
		}
		if(Trader)
			Trader_EndTrader();

		if(fishing_timer.Enabled())
		{
			if(GetBoatNPCID() == 0 || (GetBoatNPCID() != 0 && ppu->anim_type != 0))
			{
				fishing_timer.Disable();
			}
		}

		/*
		if (IsFeigned() && GetBoatNPCID() == 0 || (GetBoatNPCID() != 0 && ppu->anim_type != 0))
		{
			SetFeigned(false);
			Message_StringID(Chat::SpellFailure, FEIGN_BROKEN_MOVE);
		}
		*/
		rewind_timer.Start(30000, true);
	}

	if (door_check_timer.Check())
	{
		entity_list.OpenFloorTeleportNear(this);
	}

	CheckClientToNpcAggroTimer();

	//last_update = Timer::GetCurrentTime();
	m_Position.x = ppu->x_pos;
	m_Position.y = ppu->y_pos;
	m_Position.z = (float)ppu->z_pos/10.0f;
	m_RewindLocation = m_Position;
	if (RuleB(Quarm, EnableProjectSpeedie))
	{
		auto timeDiff = std::chrono::duration<double>(current_update_time - last_position_update_time);
		if (timeDiff.count() >= 1.0)
		{
			last_position_update_time = current_update_time;
			m_LastLocation = m_Position;
		}
	}
	//auto old_anim = animation;
	animation = ppu->anim_type;
	// No need to check for loc change, our client only sends this packet if it has actually moved in some way.
	if (update_all) {
		entity_list.SendPositionUpdates(this);
		position_timer_counter = 0;
		if (sendtoself)
			SendPosUpdate(2);
	}
	else {
		SendPosUpdate(sendtoself);
	}
	// they can be swimming on the surface and not be underwater but still should gain skill
	bool swimming = IsInWater();
	if (swimming && (m_Delta.x != 0.0f || m_Delta.y != 0.0f) && GetRawSkill(EQ::skills::SkillSwimming) < MaxSkill(EQ::skills::SkillSwimming)) {
		CheckIncreaseSkill(EQ::skills::SkillSwimming, nullptr, zone->skill_difficulty[EQ::skills::SkillSwimming].difficulty, SKILLUP_FAILURE);
	}
	// The TAKP client has a bug where a levitated horse rider's Z position is always on the floor.  this makes the server think that
	// the client is running on the ocean floor even though they are levitated above it, and other players see them on the floor too.
	// The client does remove the horse buff when they become submerged.  With the horse bug (AllLuclinPcModelsOff=TRUE) the position 
	// works correctly but the client does not remove the horse buff when submerged.
	// AK did not remove the horse buff server side and it was possible to ride with lev along the top of the water.
	/*
	if (GetHorseId() != 0 && IsUnderWater()) {
		BuffFadeByEffect(SE_SummonHorse);
	}
	*/

	CorpseSummonOnPositionUpdate();

	CheckVirtualZoneLines();

#if false // just for debugging
	static auto lastpos = glm::vec4();
	float dist_moved = Distance(m_Position, lastpos);
	lastpos = m_Position;
	Message(MT_Shout, "PositionUpdate for %s(%d): loc: %d,%d,%d,%d delta: %1.3f,%1.3f,%1.3f,%d anim: %d, dist %0.2f", GetName(), ppu->spawn_id, ppu->x_pos, ppu->y_pos, ppu->z_pos, ppu->heading, ppu->delta_yzx.GetX(), ppu->delta_yzx.GetY(), ppu->delta_yzx.GetZ(), ppu->delta_heading, ppu->anim_type, dist_moved);
#endif

	return;
}

void Client::Handle_OP_CombatAbility(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(CombatAbility_Struct)) {
		std::cout << "Wrong size on OP_CombatAbility. Got: " << app->size << ", Expected: " << sizeof(CombatAbility_Struct) << std::endl;
		return;
	}


	if (Admin() > 0)
	{
		Message(Chat::Red, "You cannot use abilities or thrown items as a GM.");
		return;
	}

	OPCombatAbility(app);
	return;
}

void Client::Handle_OP_AutoAttack2(const EQApplicationPacket *app)
{
	return;
}

void Client::Handle_OP_Consent(const EQApplicationPacket *app)
{
	if(app->size<64)
	{
		Consent_Struct* c = (Consent_Struct*)app->pBuffer;

		char cname[64];
		memcpy(cname, c->name, app->size);
		cname[63] = '\0';

		std::string gname = cname;
		std::transform(gname.begin(), gname.end(), gname.begin(), ::tolower);
		std::string oname = GetName();
		std::transform(oname.begin(), oname.end(), oname.begin(), ::tolower);

		if (GetCorpseCount() < 1)
		{
			Message_StringID(Chat::White, NO_CORPSES);
			return;
		}
		else if (IsConsented(gname))
		{
			Message_StringID(Chat::White, CONSENT_DENIED, c->name);
			char ownername[64];
			strcpy(ownername, GetName());
			Consent(0, ownername, c->name, true);

			auto pack = new ServerPacket(ServerOP_ConsentDeny, sizeof(ServerOP_ConsentDeny_Struct));
			ServerOP_ConsentDeny_Struct* scs = (ServerOP_ConsentDeny_Struct*)pack->pBuffer;
			strcpy(scs->grantname, c->name);
			strcpy(scs->ownername, GetName());
			worldserver.SendPacket(pack);
			safe_delete(pack);
			return;
		}
		else if(gname != oname)
		{
			auto pack = new ServerPacket(ServerOP_Consent, sizeof(ServerOP_Consent_Struct));
			ServerOP_Consent_Struct* scs = (ServerOP_Consent_Struct*)pack->pBuffer;
			strcpy(scs->grantname, c->name);
			strcpy(scs->ownername, GetName());
			scs->message_string_id = 0;
			scs->permission = 1;
			scs->zone_id = zone->GetZoneID();
			scs->corpse_id = 0;
			worldserver.SendPacket(pack);
			safe_delete(pack);
			return;
		}
		else 
		{
			Message_StringID(Chat::White, CONSENT_YOURSELF);
			return;
		}
	}
	return;
}

void Client::Handle_OP_Consider(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Consider_Struct))
	{
		Log(Logs::General, Logs::Error, "Size mismatch in Consider expected %i got %i", sizeof(Consider_Struct), app->size);
		return;
	}
	Consider_Struct* conin = (Consider_Struct*)app->pBuffer;
	Mob* tmob = entity_list.GetMob(conin->targetid);
	if (tmob == 0)
		return;

	auto outapp = new EQApplicationPacket(OP_Consider, sizeof(Consider_Struct));
	Consider_Struct* con = (Consider_Struct*)outapp->pBuffer;
	con->playerid = GetID();
	con->targetid = conin->targetid;
	if (tmob->IsNPC())
		con->faction = GetFactionLevel(character_id, race, class_, deity, (tmob->IsNPC()) ? tmob->CastToNPC()->GetPrimaryFaction() : 0, tmob); // Dec. 20, 2001; TODO: Send the players proper deity
	else
		con->faction = 1;
	con->level = GetLevelCon(tmob->GetLevel());
	if (zone->IsPVPZone()) {
		if (!tmob->IsNPC())
			con->pvpcon = (bool)tmob->CastToClient()->GetPVP();
	}

	// If we're feigned show NPC as indifferent
	if (tmob->IsNPC())
	{
		if (IsFeigned() && !tmob->GetSpecialAbility(SpecialAbility::FeignDeathImmunity))
			con->faction = FACTION_INDIFFERENTLY;
	}

	if (!(con->faction == FACTION_SCOWLS))
	{
		if (tmob->IsNPC())
		{
			if (tmob->CastToNPC()->IsOnHatelist(this) && (!IsFeigned() || tmob->GetSpecialAbility(SpecialAbility::FeignDeathImmunity)))
				con->faction = FACTION_THREATENINGLY;
		}
	}

	if (con->faction == FACTION_APPREHENSIVELY) {
		con->faction = FACTION_SCOWLS;
	}
	else if (con->faction == FACTION_DUBIOUSLY) {
		con->faction = FACTION_THREATENINGLY;
	}
	else if (con->faction == FACTION_SCOWLS) {
		con->faction = FACTION_APPREHENSIVELY;
	}
	else if (con->faction == FACTION_THREATENINGLY) {
		con->faction = FACTION_DUBIOUSLY;
	}

	QueuePacket(outapp);
	safe_delete(outapp);

	if (tmob->IsClient())
	{
		Client* tmobClient = tmob->CastToClient();

		bool self_found_any = tmobClient->IsSelfFoundAny();
		bool solo_only = tmobClient->IsSoloOnly();
		bool hardcore = tmobClient->IsHardcore();
		bool player_null_class = tmobClient->GetBaseClass() == 0;
		if (self_found_any || solo_only || hardcore || player_null_class)
		{
			std::string ruleset_string = "This player is running the";
			if (solo_only)
				ruleset_string += " solo self found ";
			else if (tmobClient->IsSelfFoundClassic())
				ruleset_string += " classic self found ";
			else if (tmobClient->IsSelfFoundFlex())
				ruleset_string += " self found ";
			if (hardcore)
				ruleset_string += " hardcore ";
			if (player_null_class)
				ruleset_string += " null class ";
			ruleset_string += "ruleset.";

			Message(0, ruleset_string.c_str());

		}
	}
	if (tmob->IsNPC())
	{
		if (tmob->CastToNPC()->HasEngageNotice() && tmob->CastToNPC()->fte_charid != 0)
		{
			bool is_same_group = false;
			bool is_same_raid = false;
			bool is_same_player = false;

			//Check if the raid, group or player matches.
			Raid *kr = entity_list.GetRaidByID(tmob->CastToNPC()->group_fte);
			Group *kg = entity_list.GetGroupByID(tmob->CastToNPC()->raid_fte);
			if (kr && kr == GetRaid())
			{
				is_same_raid = true;
			}
			else if (kg && kg == GetGroup())
			{
				is_same_group = true;
			}
			else if (tmob->CastToNPC()->fte_charid == CharacterID())
			{
				is_same_player = true;
			}
			//If we don't match the raid, group or same player, we see a message.
			if (!is_same_raid && !is_same_group && !is_same_player)
			{
				Message(13, "This mob is currently engaged with another group, raid, or solo player.");
			}
		}
	}
	return;
}

void Client::Handle_OP_ConsiderCorpse(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Consider_Struct))
	{
		Log(Logs::General, Logs::Error, "Size mismatch in Consider corpse expected %i got %i", sizeof(Consider_Struct), app->size);
		return;
	}
	Consider_Struct* conin = (Consider_Struct*)app->pBuffer;
	Corpse* tcorpse = entity_list.GetCorpseByID(conin->targetid);
	if (tcorpse && tcorpse->IsNPCCorpse()) {
		uint32 min; uint32 sec; uint32 ttime;
		if ((ttime = tcorpse->GetDecayTime()) != 0) {
			sec = (ttime / 1000) % 60; // Total seconds
			min = (ttime / 60000) % 60; // Total seconds / 60 drop .00
			char val1[20] = { 0 };
			char val2[20] = { 0 };
			Message_StringID(Chat::White, CORPSE_DECAY1, ConvertArray(min, val1), ConvertArray(sec, val2));
		}
		else {
			Message_StringID(Chat::White, CORPSE_DECAY_NOW);
		}
	}
	else if (tcorpse && tcorpse->IsPlayerCorpse()) {
		uint32 day, hour, min, sec, ttime;
		if (tcorpse->IsRezzable() && (ttime = tcorpse->GetRemainingRezTime()) > 0)
		{
			sec = (ttime / 1000) % 60; // Total seconds
			min = (ttime / 60000) % 60; // Total seconds
			hour = (ttime / 3600000) % 24; // Total hours
			if (hour)
				Message(Chat::White, "This corpse's resurrection time will expire in %i hour(s) %i minute(s) and %i seconds.", hour, min, sec);
			else
				Message(Chat::White, "This corpse's resurrection time will expire in %i minute(s) and %i seconds.", min, sec);

			hour = 0;
		}
		else
			Message(Chat::White, "This corpse is too old to be resurrected.");

		if ((ttime = tcorpse->GetDecayTime()) != 0) {
			sec = (ttime / 1000) % 60; // Total seconds
			min = (ttime / 60000) % 60; // Total seconds
			hour = (ttime / 3600000) % 24; // Total hours
			day = ttime / 86400000; // Total Days
			if (day)
				Message(Chat::White, "This corpse will decay in %i day(s) %i hour(s) %i minute(s) and %i seconds.", day, hour, min, sec);
			else if (hour)
				Message(Chat::White, "This corpse will decay in %i hour(s) %i minute(s) and %i seconds.", hour, min, sec);
			else
				Message(Chat::White, "This corpse will decay in %i minute(s) and %i seconds.", min, sec);

			hour = 0;
		}
		else {
			Message_StringID(Chat::White, CORPSE_DECAY_NOW);
		}
	}
}

void Client::Handle_OP_Consume(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Consume_Struct))
	{
		Log(Logs::General, Logs::Error, "OP size error: OP_Consume expected:%i got:%i", sizeof(Consume_Struct), app->size);
		return;
	}
	Consume_Struct* pcs = (Consume_Struct*)app->pBuffer;
	Log(Logs::Detail, Logs::Debug, "Hit Consume! How consumed: %i. Slot: %i. Type: %i", pcs->auto_consumed, pcs->slot, pcs->type);

	if (pcs->type != 1 && pcs->type != 2)
	{
		Log(Logs::General, Logs::Error, "OP_Consume: unknown type, type:%i", (int)pcs->type);
		return;
	}

	EQ::ItemInstance *myitem = GetInv().GetItem(pcs->slot);
	if (myitem == nullptr)
	{
		Log(Logs::General, Logs::Error, "Consuming from empty slot %d", pcs->slot);
		return;
	}
	const EQ::ItemData* consume_item = myitem->GetItem();
	if (!consume_item)
		return;

	uint8 expectItemType = pcs->type == 1 ? EQ::item::ItemTypeFood : EQ::item::ItemTypeDrink;
	if (consume_item->ItemType != expectItemType)
	{
		// I think this can happen when quickly swapping items in inventory, like after a forage
		Log(Logs::General, Logs::Error, "%s tried to consume something that isn't food or drink from slot %d : %s", name, pcs->slot, consume_item->Name);
		return;
	}
	
	bool auto_consume = pcs->auto_consumed == -1;
	int16 amountMod = auto_consume ? 100 : 50; // force feeding gives half as much fullness
	int16 amount = amountMod * consume_item->CastTime;
	int16 oldValue = consume_item->ItemType == EQ::item::ItemTypeFood ? GetHunger() : GetThirst();
	int16 newValue = oldValue + amount;
	if (!auto_consume && newValue > 6000)
	{
		newValue = 6000; // force feeding limits you to 6000 but auto eating can get you up to 32000
	}

	if (consume_item->ItemType == EQ::item::ItemTypeFood)
	{
		SetHunger(newValue);
		Log(Logs::Detail, Logs::Inventory, "Eating from slot:%i", (int)pcs->slot);
		if (!auto_consume) //no message if the client consumed for us
			entity_list.MessageClose_StringID(this, true, 50, 0, EATING_MESSAGE, GetName(), consume_item->Name);
	}
	else
	{
		SetThirst(newValue);
		Log(Logs::Detail, Logs::Inventory, "Drinking from slot:%i", (int)pcs->slot);
		if (!auto_consume) //no message if the client consumed for us
			entity_list.MessageClose_StringID(this, true, 50, 0, DRINKING_MESSAGE, GetName(), consume_item->Name);
	}

	//Message(MT_Broadcasts, "Consumed %s from slot %d auto_consume %d amount %d newValue %d", consume_item->Name, pcs->slot, auto_consume, amount, newValue);

	DeleteItemInInventory(pcs->slot, 1, false);
	SendStaminaUpdate();
}

void Client::Handle_OP_ControlBoat(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ControlBoat_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_ControlBoat, size=%i, expected %i", app->size, sizeof(ControlBoat_Struct));
		return;
	}

	bool TakeControl = false;
	Mob* boat = 0;
	int unknown5 = 0;
	int16 boatid = 0;
	ControlBoat_Struct* cbs = (ControlBoat_Struct*)app->pBuffer;
	boat = entity_list.GetMob(cbs->boatId);
	TakeControl = cbs->TakeControl;
	unknown5 = cbs->unknown5;
	boatid = cbs->boatId;

	if (boat == 0)
		return;	// do nothing if the boat isn't valid

	if (!boat->IsNPC() || (boat->GetRace() != Race::Boat))
	{
		auto hacked_string = fmt::format("OP_Control Boat was sent against {} which is of race {} ", boat->GetName(), boat->GetRace());
		database.SetMQDetectionFlag(this->AccountName(), this->GetName(), hacked_string, zone->GetShortName());
		return;
	}

	if (TakeControl) {
		// this uses the boat's target to indicate who has control of it. It has to check hate to make sure the boat isn't actually attacking anyone.
		if ((boat->GetTarget() == 0) || (boat->GetTarget() == this && boat->GetHateAmount(this) == 0)) {
			boat->SetTarget(this);
		}
		else {
			this->Message_StringID(Chat::Red, IN_USE);
			return;
		}
	}
	else
		boat->SetTarget(0);

	auto outapp = new EQApplicationPacket(OP_ControlBoat, sizeof(ControlBoat_Struct));
	ControlBoat_Struct* cbs2 = (ControlBoat_Struct*)outapp->pBuffer;
	cbs2->boatId = boatid;
	cbs2->TakeControl = TakeControl;
	cbs2->unknown5 = unknown5;

	FastQueuePacket(&outapp);
	// have the boat signal itself, so quests can be triggered by boat use
	boat->CastToNPC()->SignalNPC(0);
}

void Client::Handle_OP_CorpseDrag(const EQApplicationPacket *app)
{
	if (DraggedCorpses.size() >= (unsigned int)RuleI(Character, MaxDraggedCorpses))
	{
		Message_StringID(Chat::Red, CORPSEDRAG_LIMIT);
		return;
	}

	VERIFY_PACKET_LENGTH(OP_CorpseDrag, app, CorpseDrag_Struct);

	CorpseDrag_Struct *cds = (CorpseDrag_Struct*)app->pBuffer;


	cds->CorpseName[63] = '\0';

	cds->DraggerName[63] = '\0';

	Mob* corpse = entity_list.GetMob(cds->CorpseName);

	if (!corpse || !corpse->IsPlayerCorpse() || corpse->CastToCorpse()->IsBeingLooted())
		return;

	Client *c = entity_list.FindCorpseDragger(corpse->GetID());

	if (c)
	{
		if (c == this)
			Message_StringID(Chat::DefaultText, CORPSEDRAG_ALREADY, corpse->GetCleanName());
		else
			Message_StringID(Chat::DefaultText, CORPSEDRAG_SOMEONE_ELSE, corpse->GetCleanName());

		return;
	}

	if (!corpse->CastToCorpse()->Summon(this, false, true))
		return;

	DraggedCorpses.emplace_back(std::pair<std::string, uint16>(cds->CorpseName, corpse->GetID()));

	Message_StringID(Chat::DefaultText, CORPSEDRAG_BEGIN, cds->CorpseName);
}

void Client::Handle_OP_CorpseDrop(const EQApplicationPacket *app)
{
	if (app->size == 1 || app->size == 0)
	{
		Message_StringID(Chat::Default, CORPSEDRAG_STOPALL);
		ClearDraggedCorpses();
		return;
	}

	VERIFY_PACKET_LENGTH(OP_CorpseDrop, app, CorpseDrag_Struct);

	CorpseDrag_Struct *cds = (CorpseDrag_Struct*)app->pBuffer;

	cds->CorpseName[63] = '\0';

	cds->DraggerName[63] = '\0';

	Mob* corpse = entity_list.GetMob(cds->CorpseName);

	if (!corpse || !corpse->IsPlayerCorpse() || corpse->CastToCorpse()->IsBeingLooted())
		return;

	Client *c = entity_list.FindCorpseDragger(corpse->GetID());

	if (c)
	{
		if (c == this)
		{
			for (auto It = DraggedCorpses.begin(); It != DraggedCorpses.end(); ++It) {
				if (!strcasecmp(It->first.c_str(), cds->CorpseName))
				{
					It = DraggedCorpses.erase(It);
					Message_StringID(Chat::DefaultText, CORPSEDRAG_STOP, corpse->GetCleanName());
					return;
				}
			}

		}
		else
			Message_StringID(Chat::DefaultText, CORPSEDRAG_SOMEONE_ELSE, corpse->GetCleanName());
		return;
	}
}

void Client::Handle_OP_CreateObject(const EQApplicationPacket *app)
{


	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	if (Admin() > 0)
	{
		EQ::ItemInstance *inst = m_inv.GetItem(EQ::invslot::slotCursor);
		if (inst)
		{
			std::string msg = "You cannot drop items as a GM. The emulator has had enough issues with that.";
			Message(Chat::Red, msg.c_str());
			SendItemPacket(EQ::invslot::slotCursor, inst, ItemPacketSummonItem);
		}
		return;
	}

	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		EQ::ItemInstance *inst = m_inv.GetItem(EQ::invslot::slotCursor);
		if (inst)
		{
			std::string msg = "You cannot drop items on the ground in guild instances.";
			Message(Chat::Red, msg.c_str());
			SendItemPacket(EQ::invslot::slotCursor, inst, ItemPacketSummonItem);
		}
		return;
	}

	DropItem(EQ::invslot::slotCursor);
	return;
}

void Client::Handle_OP_Damage(const EQApplicationPacket *app) 
{
	if (!ClientFinishedLoading())
	{
		SendHPUpdate();
		return;
	}

	if (app->size != sizeof(Damage_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized OP_Damage: got %d, expected %d", app->size,
			sizeof(Damage_Struct));
		DumpPacket(app);
		return;
	}
	Damage_Struct* ed = (Damage_Struct*)app->pBuffer;

	int damage = ed->damage;
	if (ed->type == 252) {

		switch (GetAA(aaAcrobatics)) { //Don't know what acrobatics effect is yet but it should be done client side via aa effect.. till then
		case 1:
			damage = damage * 95 / 100;
			break;
		case 2:
			damage = damage * 90 / 100;
			break;
		case 3:
			damage = damage * 80 / 100;
			break;
		}
	}

	if (damage < 0)
		damage = 31337;

	if ((GetGM() && admin >= minStatusToAvoidFalling) || GetGMInvul())
	{
				std::string damagetype = "Unknown";
				switch (ed->type)
				{
				case 246:
					damagetype = "Freezing";
					break;
				case 250:
					damagetype = "Burning";
					break;
				case 251:
					damagetype = "Drowning/Suffocating";
					break;
				case 252:
					damagetype = "Falling";
					break;
				}
				Message(Chat::Red, "Your GM status protects you from %i points of %i environmental damage.", ed->damage, ed->type);
		SendHPUpdate();
		return;
	}
	else if (GetInvul())
	{
		// This client handles Divine Aura for us, but just in case send the HP packet.
		SendHPUpdate();
		return;
	}
	else if (zone->GetZoneID() == Zones::TUTORIAL || zone->GetZoneID() == Zones::LOAD)
	{
		return;
	}
	else
	{
		if (accidentalfall_timer.Enabled())
		{
			if (accidentalfall_timer.Check())
			{
				float z_prev = initial_z_position;
				float z_cur = m_Position.z;
				if (z_cur <= z_prev + RuleR(Quarm, AccidentalFallUnitDist) || z_cur >= z_prev + -RuleR(Quarm, AccidentalFallUnitDist))
				{
					SendHPUpdate();
					return;
				}
			}
		}
		SetHP(GetHP() - (damage * RuleR(Character, EnvironmentDamageMulipliter)));

		/* EVENT_ENVIRONMENTAL_DAMAGE */
		int final_damage = (damage * RuleR(Character, EnvironmentDamageMulipliter));
		std::string export_string = fmt::format(
			"{} {} {}",
			ed->damage,
			ed->type,
			final_damage
		);
		parse->EventPlayer(EVENT_ENVIRONMENTAL_DAMAGE, this, export_string, 0);
	}

	SendHPUpdate();

	if (HasDied())
	{
		EQ::skills::SkillType env_skill = static_cast<EQ::skills::SkillType>(ed->type);
		Death(nullptr, 0, SPELL_UNKNOWN, env_skill, Killed_ENV);
	}

	return;
}

void Client::Handle_OP_Death(const EQApplicationPacket *app)
{
	bool EnvDeath = false;
	if (app->size != sizeof(Death_Struct))
	{
		Log(Logs::Detail, Logs::Debug, "Handle_OP_Death: Struct is incorrect, expected %i got %i", sizeof(Death_Struct), app->size);
		return;
	}
	Death_Struct* ds = (Death_Struct*)app->pBuffer;

	//Burning, Drowning, Falling, Freezing
	if (ds->attack_skill >= 246 && ds->attack_skill <= 255)
	{
		EnvDeath = true;
	}
	//I think this attack_skill value is really a value from SkillDamageTypes...
	else if (ds->attack_skill > 255)
	{
		return;
	}

	Mob* killer = entity_list.GetMob(ds->killer_id);
	Log(Logs::Detail, Logs::Death, "Client has sent a death request for %s. Killer is: %s", GetName(), killer ? killer->GetName() : "null");
	if (EnvDeath == true)
	{
		EQ::skills::SkillType env_skill = static_cast<EQ::skills::SkillType>(ds->attack_skill);
		Death(nullptr, 0, SPELL_UNKNOWN, env_skill, Killed_ENV);
		return;
	}
	else
	{
		Death(killer, ds->damage, ds->spell_id, (EQ::skills::SkillType)ds->attack_skill, Killed_Client);
		return;
	}
}

void Client::Handle_OP_DeleteCharge(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MoveItem_Struct)) {
		std::cout << "Wrong size on OP_DeleteCharge. Got: " << app->size << ", Expected: " << sizeof(MoveItem_Struct) << std::endl;
		return;
	}

	MoveItem_Struct* alc = (MoveItem_Struct*)app->pBuffer;

	const EQ::ItemInstance *inst = GetInv().GetItem(alc->from_slot);
	if (inst && inst->GetItem()->ItemType == EQ::item::ItemTypeAlcohol) {
		entity_list.MessageClose_StringID(this, true, 50, 0, DRINKING_MESSAGE, GetName(), inst->GetItem()->Name);
		CheckIncreaseSkill(EQ::skills::SkillAlcoholTolerance, nullptr, zone->skill_difficulty[EQ::skills::SkillAlcoholTolerance].difficulty);

		int16 AlcoholTolerance = GetSkill(EQ::skills::SkillAlcoholTolerance);
		int16 IntoxicationIncrease;

		IntoxicationIncrease = (200 - AlcoholTolerance) * 30 / 200 + 10;

		if (IntoxicationIncrease < 0)
			IntoxicationIncrease = 1;

		m_pp.intoxication += IntoxicationIncrease;

		if (m_pp.intoxication > 200)
			m_pp.intoxication = 200;

		CalcBonuses();
	}

	if (inst)
		DeleteItemInInventory(alc->from_slot, 1);

	return;
}

void Client::Handle_OP_DeleteSpawn(const EQApplicationPacket *app)
{
	// The client will send this with his id when he zones, maybe when he disconnects too?
	//eqs->RemoveData(); // Flushing the queue of packet data to allow for proper zoning
	//hate_list.RemoveEnt(this->CastToMob());
	//Disconnect();
	HardDisconnect();
	return;
}

void Client::Handle_OP_DeleteSpell(const EQApplicationPacket *app)
{
	if (app->size != sizeof(DeleteSpell_Struct))
		return;

	EQApplicationPacket* outapp = app->Copy();
	DeleteSpell_Struct* dss = (DeleteSpell_Struct*)outapp->pBuffer;

	if (dss->spell_slot < 0 || dss->spell_slot > int(MAX_PP_SPELLBOOK))
		return;

	if (m_pp.spell_book[dss->spell_slot] != SPELL_UNKNOWN) 
	{
		database.DeleteCharacterSpell(this->CharacterID(), m_pp.spell_book[dss->spell_slot], dss->spell_slot);
		dss->success = 1;
	}
	else
		dss->success = 0;

	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_DisarmTraps(const EQApplicationPacket *app) 
{
	if (!HasSkill(EQ::skills::SkillDisarmTraps))
		return;

	if (!p_timers.Expired(&database, pTimerDisarmTraps, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}
	int reuse = DisarmTrapsReuseTime;
	switch (GetAA(aaAdvTrapNegotiation)) {
	case 1:
		reuse -= 1;
		break;
	case 2:
		reuse -= 3;
		break;
	case 3:
		reuse -= 5;
		break;
	}
	p_timers.Start(pTimerDisarmTraps, reuse - 1);

	uint8 success = SKILLUP_FAILURE;
	float curdist = 0;
	Trap* trap = entity_list.FindNearbyTrap(this, 250, curdist, true);
	if (trap && trap->detected)
	{
		float max_radius = (trap->radius*2) * (trap->radius*2); // radius is used to trigger trap, so disarm radius should be a bit bigger.
		Log(Logs::General, Logs::Traps, "%s is attempting to disarm trap %d. Curdist is %0.2f maxdist is %0.2f", GetName(), trap->trap_id, curdist, max_radius);
		if (curdist <= max_radius)
		{
			int uskill = GetSkill(EQ::skills::SkillDisarmTraps);
			if ((zone->random.Int(0, 49) + uskill) >= (zone->random.Int(0, 49) + trap->skill))
			{
				success = SKILLUP_SUCCESS;
				Message_StringID(Chat::Skills, DISARMED_TRAP);
				trap->disarmed = true;
				Log(Logs::General, Logs::Traps, "Trap %d is disarmed.", trap->trap_id);
				trap->UpdateTrap();
			}
			else
			{
				Message_StringID(Chat::Skills, FAIL_DISARM_DETECTED_TRAP);
				if (zone->random.Int(0, 99) < 25) {
					trap->Trigger(this);
				}
			}
			CheckIncreaseSkill(EQ::skills::SkillDisarmTraps, nullptr, zone->skill_difficulty[EQ::skills::SkillDisarmTraps].difficulty, success);
			return;
		}
		else
		{
			Message_StringID(Chat::Skills, TRAP_TOO_FAR);
		}
	}
	else
	{
		Message_StringID(Chat::Skills, TRAP_NOT_DETECTED);
	}

	return;
}

void Client::Handle_OP_Discipline(const EQApplicationPacket *app) 
{
	ClientDiscipline_Struct* cds = (ClientDiscipline_Struct*)app->pBuffer;
	if(cds->disc_id > 0)
	{
		uint32 remain = CheckDiscTimer(pTimerDisciplineReuseStart + GetDiscTimerID(cds->disc_id));
		if(!GetGM() && remain > 0)
		{
			char val1[20]={0};
			char val2[20]={0};
			Log(Logs::General, Logs::Discs, "Disc %d reuse time not yet met. %d", cds->disc_id, remain);
			Message_StringID(Chat::Disciplines, DISCIPLINE_CANUSEIN, ConvertArray((remain)/60,val1), ConvertArray(remain%60,val2));
			return;
		}
		else
		{
			Log(Logs::General, Logs::Discs, "Attempting to cast Disc %d.", cds->disc_id);
			UseDiscipline(cds->disc_id);
		}
	}
	else 
	{
		uint32 remain = 0;
		if (RuleB(Combat, UseDiscTimerGroups))
		{
			//We're not casting a disc, check all timers and tell player if we're ready to cast or not.
			for (int i = 0; i <= 2; ++i)
			{
				// Find the first available timer.
				uint32 tremain = CheckDiscTimer(pTimerDisciplineReuseStart + i);
				if ((tremain < remain && tremain != 0) || remain == 0)
					remain = tremain;
			}
		}
		else
		{
			remain = CheckDiscTimer(pTimerDisciplineReuseStart);
		}

		if(remain > 0)
		{
			char val1[20]={0};
			char val2[20]={0};
			Log(Logs::General, Logs::Discs, "Disc reuse time not yet met. %d", remain);
			Message_StringID(Chat::Disciplines, DISCIPLINE_CANUSEIN, ConvertArray((remain)/60,val1), ConvertArray(remain%60,val2));
		}
		else
		{
			Log(Logs::General, Logs::Discs, "No disc used and reuse time is met.");
			auto outapp = new EQApplicationPacket(OP_InterruptCast, sizeof(InterruptCast_Struct));
			InterruptCast_Struct* ic = (InterruptCast_Struct*)outapp->pBuffer;
			ic->messageid = DISCIPLINE_RDY;
			ic->color = Chat::Disciplines;
			QueuePacket(outapp);
			safe_delete(outapp);
		}
	}
}

void Client::Handle_OP_DuelResponse(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(DuelResponse_Struct))
		return;
	DuelResponse_Struct* ds = (DuelResponse_Struct*)app->pBuffer;
	Entity *entity = entity_list.GetID(ds->duel_target);
	Entity *initiator = entity_list.GetID(ds->duel_initiator);
	if(!entity || !initiator)
		return;
	
	if (!entity->IsClient() || !initiator->IsClient())
		return;

	entity->CastToClient()->SetDuelTarget(0);
	entity->CastToClient()->SetDueling(false);
	initiator->CastToClient()->SetDuelTarget(0);
	initiator->CastToClient()->SetDueling(false);
	// inform initiator the duel was declined (the client handles informing the decliner)
	initiator->CastToClient()->Message_StringID(Chat::White, DUEL_DECLINE, GetName());
	return;
}

void Client::Handle_OP_DuelResponse2(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Duel_Struct))
		return;

	Duel_Struct* ds = (Duel_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetID(ds->duel_target);
	Entity* initiator = entity_list.GetID(ds->duel_initiator);

	if (entity && initiator && entity == this && initiator->IsClient()) {
		auto outapp = new EQApplicationPacket(OP_RequestDuel, sizeof(Duel_Struct));
		Duel_Struct* ds2 = (Duel_Struct*)outapp->pBuffer;

		ds2->duel_initiator = entity->GetID();
		ds2->duel_target = entity->GetID();
		initiator->CastToClient()->QueuePacket(outapp);

		outapp->SetOpcode(OP_DuelResponse2);
		ds2->duel_initiator = initiator->GetID();

		initiator->CastToClient()->QueuePacket(outapp);

		QueuePacket(outapp);
		SetDueling(true);
		initiator->CastToClient()->SetDueling(true);
		SetDuelTarget(ds->duel_initiator);
		safe_delete(outapp);
	}
	return;
}

void Client::Handle_OP_Emote(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Emote_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized ""OP_Emote: got %d, expected %d", app->size, sizeof(Emote_Struct));
		DumpPacket(app);
		return;
	}

	if (GetRevoked())
	{
		Message(Chat::Default, "You have been revoked. You may not send Emote messages.");
		return;
	}

	// Calculate new packet dimensions
	Emote_Struct* in = (Emote_Struct*)app->pBuffer;
	in->message[1023] = '\0';

	const char* name = GetName();
	uint32 len_name = strlen(name);
	uint32 len_msg = strlen(in->message);
	// crash protection -- cheater
	if (len_msg > 512) {
		in->message[512] = '\0';
		len_msg = 512;
	}
	uint32 len_packet = sizeof(in->unknown01) + len_name
		+ len_msg + 1;

	// Construct outgoing packet
	auto outapp = new EQApplicationPacket(OP_Emote, len_packet);
	Emote_Struct* out = (Emote_Struct*)outapp->pBuffer;
	out->unknown01 = in->unknown01;
	memcpy(out->message, name, len_name);
	memcpy(&out->message[len_name], in->message, len_msg);

	entity_list.QueueCloseClients(this, outapp, true, RuleI(Range, Emote), 0, true, FilterSocials);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_EndLootRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(uint16)) {
		std::cout << "Wrong size: OP_EndLootRequest, size=" << app->size << ", expected " << sizeof(uint16) << std::endl;
		return;
	}

	SetLooting(0);

	Entity* entity = entity_list.GetID(*((uint16*)app->pBuffer));
	if (entity == 0) {
		Corpse::SendLootReqErrorPacket(this);
		return;
	}
	else if (!entity->IsCorpse()) {
		Corpse::SendLootReqErrorPacket(this);
		return;
	}
	else {
		entity->CastToCorpse()->EndLoot(this, app);
	}
	return;
}

void Client::Handle_OP_EnvDamage(const EQApplicationPacket *app)
{
	//EQmac sends this when drowning.
}

void Client::Handle_OP_FaceChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(FaceChange_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for OP_FaceChange: Expected: %i, Got: %i",
			sizeof(FaceChange_Struct), app->size);
		return;
	}

	// Notify other clients in zone
	entity_list.QueueClients(this, app, false);

	FaceChange_Struct* fc = (FaceChange_Struct*)app->pBuffer;
	m_pp.haircolor = fc->haircolor;
	m_pp.beardcolor = fc->beardcolor;
	m_pp.eyecolor1 = fc->eyecolor1;
	m_pp.eyecolor2 = fc->eyecolor2;
	m_pp.hairstyle = fc->hairstyle;
	m_pp.face = fc->face;
	m_pp.beard = fc->beard;
	Save();

	return;
}

void Client::Handle_OP_FeignDeath(const EQApplicationPacket *app)
{
	if (GetClass() != Class::Monk)
		return;
	if (!p_timers.Expired(&database, pTimerFeignDeath, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}

	int reuse = FeignDeathReuseTime;
	switch (GetAA(aaRapidFeign))
	{
	case 1:
		reuse -= 1;
		break;
	case 2:
		reuse -= 2;
		break;
	case 3:
		reuse -= 5;
		break;
	}
	p_timers.Start(pTimerFeignDeath, reuse - 1);

	float totalfeign = GetSkill(EQ::skills::SkillFeignDeath);

	// Feign chance caps at 95% at 100 skill. 
	if (totalfeign > 100)
		totalfeign = 100;

	uint8 success = SKILLUP_FAILURE;

	Log(Logs::General, Logs::Skills, "Rolling Feign Death with a %0.1f chance", totalfeign/105*100);
	if (auto_attack)
	{
		SetFeigned(false);
	}
	else if (zone->random.Roll0(105) > totalfeign) {
		SetFeigned(false);
		entity_list.MessageClose_StringID(this, false, 200, 10, STRING_FEIGNFAILED, GetName());
	}
	else 
	{
		if (CastToClient()->auto_attack) {
			CastToClient()->SetFeigned(false);
		}
		else {
			success = SKILLUP_SUCCESS;
			SetFeigned(true);
		}
	}

	CheckIncreaseSkill(EQ::skills::SkillFeignDeath, nullptr, zone->skill_difficulty[EQ::skills::SkillFeignDeath].difficulty, success);
	return;
}

void Client::Handle_OP_Fishing(const EQApplicationPacket *app) 
{
	if (!p_timers.Expired(&database, pTimerFishing, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}

	if (CanFish()) {
		parse->EventPlayer(EVENT_FISH_START, this, "", 0);

		//these will trigger GoFish() after a delay if we're able to actually fish, and if not, we won't stop the client from trying again immediately (although we may need to tell it to repop the button)
		p_timers.Start(pTimerFishing, FishingReuseTime - 1);
		fishing_timer.Start();

	}
	return;
	// Changes made based on Bobs work on foraging. Now can set items in the forage database table to
	// forage for.
}

void Client::Handle_OP_Forage(const EQApplicationPacket *app) 
{
	if (!HasSkill(EQ::skills::SkillForage))
	{
		return;
	}

	if (!p_timers.Expired(&database, pTimerForaging, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}

	if (IsSitting())
	{
		Message_StringID(Chat::Skills, FORAGE_STANDING);
		return;
	}

	if (IsStunned() || IsMezzed() || AutoAttackEnabled())
	{
		Message_StringID(Chat::Skills, FORAGE_COMBAT);
		return;
	}

	p_timers.Start(pTimerForaging, ForagingReuseTime - 1);

	ForageItem();

	return;
}

void Client::Handle_OP_FriendsWho(const EQApplicationPacket *app)
{
	char *FriendsString = (char*)app->pBuffer;
	FriendsWho(FriendsString);
	return;
}

void Client::Handle_OP_GetGuildsList(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_GetGuildsList");
	SendPlayerGuild();
}

void Client::Handle_OP_GMBecomeNPC(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/becomenpc");
		return;
	}
	if (app->size != sizeof(BecomeNPC_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_GMBecomeNPC, size=%i, expected %i", app->size, sizeof(BecomeNPC_Struct));
		return;
	}
	//entity_list.QueueClients(this, app, false);
	BecomeNPC_Struct* bnpc = (BecomeNPC_Struct*)app->pBuffer;

	Mob* cli = (Mob*)entity_list.GetMob(bnpc->id);
	if (cli == nullptr)
		return;

	if (cli->IsClient())
	{
		Client* target = cli->CastToClient();
		target->QueuePacket(app);
		if(target->GetGM())
		{
			target->SetInvul(false);
			target->SetHideMe(false);
			target->SetGM(false);
		}

		cli->SendAppearancePacket(AppearanceType::NPCName, 1, true);
		target->SetBecomeNPC(true);
		target->SetBecomeNPCLevel(bnpc->maxlevel);
		cli->Message_StringID(Chat::White, TOGGLE_OFF);
		target->tellsoff = true;
		target->UpdateWho();
	}

	return;
}

void Client::Handle_OP_GMDelCorpse(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMDelCorpse_Struct))
		return;
	if (this->Admin() < commandEditPlayerCorpses) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/delcorpse");
		return;
	}
	GMDelCorpse_Struct* dc = (GMDelCorpse_Struct *)app->pBuffer;
	Mob* corpse = entity_list.GetMob(dc->corpsename);
	if (corpse == 0) {
		return;
	}
	if (corpse->IsCorpse() != true) {
		return;
	}
	corpse->CastToCorpse()->Delete();
	std::cout << name << " deleted corpse " << dc->corpsename << std::endl;
	Message(Chat::Red, "Corpse %s deleted.", dc->corpsename);
	return;
}

void Client::Handle_OP_GMEmoteZone(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/emote");
		return;
	}
	if (app->size != sizeof(GMEmoteZone_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_GMEmoteZone, size=%i, expected %i", app->size, sizeof(GMEmoteZone_Struct));
		return;
	}
	GMEmoteZone_Struct* gmez = (GMEmoteZone_Struct*)app->pBuffer;
	char* newmessage = 0;
	if (strstr(gmez->text, "^") == 0)
		entity_list.Message(Chat::White, 15, gmez->text);
	else{
		for (newmessage = strtok((char*)gmez->text, "^"); newmessage != nullptr; newmessage = strtok(nullptr, "^"))
			entity_list.Message(Chat::White, 15, newmessage);
	}
	return;
}

void Client::Handle_OP_GMEndTraining(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMTrainEnd_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_GMEndTraining expected %i got %i", sizeof(GMTrainEnd_Struct), app->size);
		DumpPacket(app);
		return;
	}
	OPGMEndTraining(app);
	return;
}

void Client::Handle_OP_GMFind(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/find");
		return;
	}
	if (app->size != sizeof(GMSummon_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_GMFind, size=%i, expected %i", app->size, sizeof(GMSummon_Struct));
		return;
	}
	//Break down incoming
	GMSummon_Struct* request = (GMSummon_Struct*)app->pBuffer;
	QServ->QSLogCommands(this, "/find", request->charname);
	//Create a new outgoing
	auto outapp = new EQApplicationPacket(OP_GMFind, sizeof(GMSummon_Struct));
	GMSummon_Struct* foundplayer = (GMSummon_Struct*)outapp->pBuffer;
	//Copy the constants
	strcpy(foundplayer->charname, request->charname);
	strcpy(foundplayer->gmname, request->gmname);
	//Check if the NPC exits intrazone...
	Mob* gt = entity_list.GetMob(request->charname);
	if (gt != 0) {
		foundplayer->success = 1;
		foundplayer->x = (int32)gt->GetX();
		foundplayer->y = (int32)gt->GetY();

		foundplayer->z = (int32)gt->GetZ();
		foundplayer->zoneID = zone->GetZoneID();
	}
	//Send the packet...
	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_GMGoto(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMSummon_Struct)) {
		std::cout << "Wrong size on OP_GMGoto. Got: " << app->size << ", Expected: " << sizeof(GMSummon_Struct) << std::endl;
		return;
	}
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/goto");
		return;
	}
	GMSummon_Struct* gmg = (GMSummon_Struct*)app->pBuffer;

	QServ->QSLogCommands(this, "/goto", gmg->charname);

	Mob* gt = entity_list.GetMob(gmg->charname);
	if (gt != nullptr) {
		this->MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), gt->GetX(), gt->GetY(), gt->GetZ(), gt->GetHeading());
	}
	else if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected.");
	else {
		auto pack = new ServerPacket(ServerOP_GMGoto, sizeof(ServerGMGoto_Struct));
		memset(pack->pBuffer, 0, pack->size);
		ServerGMGoto_Struct* wsgmg = (ServerGMGoto_Struct*)pack->pBuffer;
		strcpy(wsgmg->myname, this->GetName());
		strcpy(wsgmg->gotoname, gmg->charname);
		wsgmg->admin = admin;
		wsgmg->guildinstanceid = zone->GetGuildID();
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
	return;
}

void Client::Handle_OP_GMHideMe(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/hideme");
		return;
	}
	if (app->size != sizeof(SpawnAppearance_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_GMHideMe, size=%i, expected %i", app->size, sizeof(SpawnAppearance_Struct));
		return;
	}
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)app->pBuffer;
	Message(Chat::Red, "#: %i, %i", sa->type, sa->parameter);
	SetHideMe(!sa->parameter);

	char szArg[5];
	sprintf(szArg, "%i", sa->parameter);
	QServ->QSLogCommands(this, "/hideme", szArg);
	return;

}

void Client::Handle_OP_GMKick(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMKick_Struct))
		return;
	if (this->Admin() < minStatusToKick) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/kick");
		return;
	}
	GMKick_Struct* gmk = (GMKick_Struct *)app->pBuffer;

	Client* client = entity_list.GetClientByName(gmk->name);
	if (client == 0) {
		if (!worldserver.Connected())
			Message(Chat::White, "Error: World server disconnected");
		else {
			auto pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
			ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)pack->pBuffer;
			strcpy(skp->adminname, gmk->gmname);
			strcpy(skp->name, gmk->name);
			skp->adminrank = this->Admin();
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
	}
	else {
		entity_list.QueueClients(this, app);
		//client->Kick();
	}
	return;
}

void Client::Handle_OP_GMKill(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/kill");
		return;
	}
	if (app->size != sizeof(GMKill_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_GMKill, size=%i, expected %i", app->size, sizeof(GMKill_Struct));
		return;
	}
	GMKill_Struct* gmk = (GMKill_Struct *)app->pBuffer;
	Mob* obj = entity_list.GetMob(gmk->name);
	Client* client = entity_list.GetClientByName(gmk->name);
	if (obj != 0) {
		QServ->QSLogCommands(this, "/kill", gmk->name);
		if (client != 0) {
			entity_list.QueueClients(this, app);
		}
		else {
			obj->Kill();
		}
	}
	else {
		if (!worldserver.Connected())
			Message(Chat::White, "Error: World server disconnected");
		else {
			auto pack = new ServerPacket(ServerOP_KillPlayer, sizeof(ServerKillPlayer_Struct));
			ServerKillPlayer_Struct* skp = (ServerKillPlayer_Struct*)pack->pBuffer;
			strcpy(skp->gmname, gmk->gmname);
			strcpy(skp->target, gmk->name);
			skp->admin = this->Admin();
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
	}
	return;
}

void Client::Handle_OP_GMLastName(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMLastName_Struct)) {
		std::cout << "Wrong size on OP_GMLastName. Got: " << app->size << ", Expected: " << sizeof(GMLastName_Struct) << std::endl;
		return;
	}
	GMLastName_Struct* gmln = (GMLastName_Struct*)app->pBuffer;
	if (strlen(gmln->lastname) >= 64) {
		Message(Chat::Red, "/LastName: New last name too long. (max=63)");
	}
	else {
		Client* client = entity_list.GetClientByName(gmln->name);
		if (client == 0) {
			Message(Chat::Red, "/LastName: %s not found", gmln->name);
		}
		else {
			if (this->Admin() < minStatusToUseGMCommands) {
				Message(Chat::Red, "Your account has been reported for hacking.");
				database.SetHackerFlag(client->account_name, client->name, "/lastname");
				return;
			}
			else

				client->ChangeLastName(gmln->lastname);
		}
		gmln->unknown[0] = 1;
		gmln->unknown[1] = 1;
		gmln->unknown[2] = 1;
		gmln->unknown[3] = 1;
		entity_list.QueueClients(this, app, false);
	}
	return;
}

void Client::Handle_OP_GMNameChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMName_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_GMNameChange, size=%i, expected %i", app->size, sizeof(GMName_Struct));
		return;
	}
	const GMName_Struct* gmn = (const GMName_Struct *)app->pBuffer;
	if (this->Admin() < minStatusToUseGMCommands){
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/name");
		return;
	}
	Client* client = entity_list.GetClientByName(gmn->oldname);
	Log(Logs::General, Logs::Status, "GM(%s) changeing players name. Old:%s New:%s", GetName(), gmn->oldname, gmn->newname);
	bool usedname = database.CheckUsedName((const char*)gmn->newname);
	if (client == 0) {
		Message(Chat::Red, "%s not found for name change. Operation failed!", gmn->oldname);
		return;
	}
	if ((strlen(gmn->newname) > 63) || (strlen(gmn->newname) == 0)) {
		Message(Chat::Red, "Invalid number of characters in new name (%s).", gmn->newname);
		return;
	}
	if (!usedname) {
		Message(Chat::Red, "%s is already in use. Operation failed!", gmn->newname);
		return;

	}
	database.UpdateName(gmn->oldname, gmn->newname);
	strcpy(client->name, gmn->newname);
	client->Save();

	if (gmn->badname == 1) {
		database.AddToNameFilter(gmn->oldname);
	}
	EQApplicationPacket* outapp = app->Copy();
	GMName_Struct* gmn2 = (GMName_Struct*)outapp->pBuffer;
	gmn2->unknown[0] = 1;
	gmn2->unknown[1] = 1;
	gmn2->unknown[2] = 1;
	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);
	UpdateWho();
	return;
}

void Client::Handle_OP_GMSearchCorpse(const EQApplicationPacket *app)
{
	// Could make this into a rule, although there is a hard limit since we are using a popup, of 4096 bytes that can
	// be displayed in the window, including all the HTML formatting tags.
	//
	const int maxResults = 10;

	if (app->size < sizeof(GMSearchCorpse_Struct))
	{
		Log(Logs::General, Logs::Error, "OP_GMSearchCorpse size lower than expected: got %u expected at least %u",
			app->size, sizeof(GMSearchCorpse_Struct));
		DumpPacket(app);
		return;
	}

	GMSearchCorpse_Struct *gmscs = (GMSearchCorpse_Struct *)app->pBuffer;
	gmscs->Name[63] = '\0';

	auto escSearchString = new char[129];
	database.DoEscapeString(escSearchString, gmscs->Name, strlen(gmscs->Name));

	std::string query = StringFormat("SELECT charname, zoneid, x, y, z, time_of_death, rezzed, IsBurried "
		"FROM character_corpses WheRE charname LIKE '%%%s%%' ORDER BY charname LIMIT %i",
		escSearchString, maxResults);
	safe_delete_array(escSearchString);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	if (results.RowCount() == 0)
		return;

	if (results.RowCount() == maxResults)
		Message(clientMessageError, "Your search found too many results; some are not displayed.");
	else
		Message(clientMessageYellow, "There are %i corpse(s) that match the search string '%s'.", results.RowCount(), gmscs->Name);

	char charName[64], time_of_death[20];

	std::string popupText = "<table><tr><td>Name</td><td>Zone</td><td>X</td><td>Y</td><td>Z</td><td>Date</td><td>"
		"Rezzed</td><td>Buried</td></tr><tr><td>&nbsp</td><td></td><td></td><td></td><td></td><td>"
		"</td><td></td><td></td></tr>";

	for (auto row = results.begin(); row != results.end(); ++row) {

		strn0cpy(charName, row[0], sizeof(charName));

		uint32 ZoneID = atoi(row[1]);
		float CorpseX = atof(row[2]);
		float CorpseY = atof(row[3]);
		float CorpseZ = atof(row[4]);

		strn0cpy(time_of_death, row[5], sizeof(time_of_death));

		bool corpseRezzed = atoi(row[6]);
		bool corpseBuried = atoi(row[7]);

		popupText += StringFormat("<tr><td>%s</td><td>%s</td><td>%8.0f</td><td>%8.0f</td><td>%8.0f</td><td>%s</td><td>%s</td><td>%s</td></tr>",
			charName, StaticGetZoneName(ZoneID), CorpseX, CorpseY, CorpseZ, time_of_death,
			corpseRezzed ? "Yes" : "No", corpseBuried ? "Yes" : "No");

		if (popupText.size() > 4000) {
			Message(clientMessageError, "Unable to display all the results.");
			break;
		}

	}

	popupText += "</table>";


}

void Client::Handle_OP_GMServers(const EQApplicationPacket *app)
{
	if (Admin() < 20) {
		return;
	}

	QServ->QSLogCommands(this, "/server", 0);

	auto pack = new ServerPacket(ServerOP_ZoneStatus, sizeof(ServerZoneStatus_Struct));

	auto z = (ServerZoneStatus_Struct*)pack->pBuffer;
	z->admin = Admin();
	strn0cpy(z->name, GetName(), sizeof(z->name));

	worldserver.SendPacket(pack);
	delete pack;
}

void Client::Handle_OP_GMSummon(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMSummon_Struct)) {
		std::cout << "Wrong size on OP_GMSummon. Got: " << app->size << ", Expected: " << sizeof(GMSummon_Struct) << std::endl;
		return;
	}
	OPGMSummon(app);
	return;
}

void Client::Handle_OP_GMToggle(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMToggle_Struct)) {
		std::cout << "Wrong size on OP_GMToggle. Got: " << app->size << ", Expected: " << sizeof(GMToggle_Struct) << std::endl;
		return;
	}
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/toggle");
		return;
	}
	GMToggle_Struct *ts = (GMToggle_Struct *)app->pBuffer;
	if (ts->toggle == 0) {
		database.SetGMIgnoreTells(AccountID(), 1);
		tellsoff = true;
	}
	else if (ts->toggle == 1) {
		database.SetGMIgnoreTells(AccountID(), 0);
		tellsoff = false;
	}
	else {
		Message(Chat::White, "Unkown value in /toggle packet");
	}
	UpdateWho();

	char szArg[5];
	sprintf(szArg, "%i", ts->toggle);
	QServ->QSLogCommands(this, "/toggle", szArg);
	return;
}

void Client::Handle_OP_GMTraining(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMTrainee_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_GMTraining expected %i got %i", sizeof(GMTrainee_Struct), app->size);
		DumpPacket(app);
		return;
	}
	OPGMTraining(app);
	return;
}

void Client::Handle_OP_GMTrainSkill(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMSkillChange_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_GMTrainSkill expected %i got %i", sizeof(GMSkillChange_Struct), app->size);
		DumpPacket(app);
		return;
	}
	OPGMTrainSkill(app);
	return;
}

void Client::Handle_OP_GMZoneRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMZoneRequest_Struct)) {
		std::cout << "Wrong size on OP_GMZoneRequest. Got: " << app->size << ", Expected: " << sizeof(GMZoneRequest_Struct) << std::endl;
		return;
	}
	if (this->Admin() < minStatusToBeGM) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/zone");
		return;
	}

	auto *gmzr = (GMZoneRequest_Struct*)app->pBuffer;
	float target_x = -1, target_y = -1, target_z = -1, target_heading;

	int16 minstatus = 0;
	uint8 minlevel = 0;
	char target_zone[32];
	uint16 zid = gmzr->zone_id;
	if (gmzr->zone_id == 0)
		zid = zonesummon_id;
	const char * zname = ZoneName(zid);
	if (zname == nullptr)
		target_zone[0] = 0;
	else
		strcpy(target_zone, zname);

	// this both loads the safe points and does a sanity check on zone name
	if (!database.GetSafePoints(target_zone, &target_x, &target_y, &target_z, &target_heading, &minstatus, &minlevel)) {
		target_zone[0] = 0;
	}

	auto outapp = new EQApplicationPacket(OP_GMZoneRequest, sizeof(GMZoneRequest_Struct));
	auto *gmzr2 = (GMZoneRequest_Struct*)outapp->pBuffer;
	strcpy(gmzr2->charname, this->GetName());
	gmzr2->zone_id = gmzr->zone_id;
	gmzr2->x = target_x;
	gmzr2->y = target_y;
	gmzr2->z = target_z;
	gmzr2->heading = target_heading;
	// Next line stolen from ZoneChange as well... - This gives us a nicer message than the normal "zone is down" message...
	if (target_zone[0] != 0 && admin >= minstatus && GetLevel() >= minlevel)
		gmzr2->success = 1;
	else {
		std::cout << "GetZoneSafeCoords failed. zoneid = " << gmzr->zone_id << "; czone = " << zone->GetZoneID() << std::endl;
		gmzr2->success = 0;
	}

	QueuePacket(outapp);
	safe_delete(outapp);

	char szArg[5];
	sprintf(szArg, "%i", gmzr->zone_id);
	QServ->QSLogCommands(this, "/zone", szArg);
	return;
}

void Client::Handle_OP_GMZoneRequest2(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToBeGM) {
		Message(Chat::Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/zone");
		return;
	}
	if (app->size < sizeof(uint32)) {
		Log(Logs::General, Logs::Error, "OP size error: OP_GMZoneRequest2 expected:%i got:%i", sizeof(uint32), app->size);
		return;
	}

	uint32 zonereq = *((uint32 *)app->pBuffer);
	GoToSafeCoords(zonereq, GUILD_NONE);

	char szArg[5];
	sprintf(szArg, "%i", zonereq);
	QServ->QSLogCommands(this, "/zone2", szArg);

	return;
}

void Client::Handle_OP_GroupCancelInvite(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupCancel_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for OP_GroupCancelInvite: Expected: %i, Got: %i",
			sizeof(GroupCancel_Struct), app->size);
		return;
	}

	GroupCancel_Struct* gf = (GroupCancel_Struct*)app->pBuffer;
	Mob* inviter = entity_list.GetClientByName(gf->name1);

	if (inviter != nullptr)
	{
		if (inviter->IsClient())
			inviter->CastToClient()->QueuePacket(app);
	}
	else
	{
		auto pack = new ServerPacket(ServerOP_GroupCancelInvite, sizeof(GroupCancel_Struct));
		memcpy(pack->pBuffer, gf, sizeof(GroupCancel_Struct));
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	return;
}

void Client::Handle_OP_GroupDisband(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupGeneric_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for GroupGeneric_Struct: Expected: %i, Got: %i",
			sizeof(GroupGeneric_Struct), app->size);
		return;
	}

	Log(Logs::General, Logs::Group, "Member Disband Request from %s\n", GetName());

	GroupGeneric_Struct* gd = (GroupGeneric_Struct*)app->pBuffer;

	Raid *raid = entity_list.GetRaidByClient(this);
	if (raid)
	{
		Mob* memberToDisband = nullptr;

		if (!raid->IsGroupLeader(GetName()))
			memberToDisband = this;
		else
			memberToDisband = GetTarget();

		if (!memberToDisband)
			memberToDisband = entity_list.GetMob(gd->name2);

		if (!memberToDisband)
			memberToDisband = this;

		if (!memberToDisband->IsClient())
			return;
		bool wasGrpLdr = false;
		//we have a raid.. see if we're in a raid group
		uint32 grp = raid->GetGroup(memberToDisband->GetName());
		if (memberToDisband != this && grp >= 0 && grp < MAX_RAID_GROUPS) {
			// check if we are actually in the same raid group.
			uint32 mygrp = raid->GetGroup(GetName());
			if (mygrp != grp)
				return;
		}
		if (grp >= 0 && grp < MAX_RAID_GROUPS) {
			uint32 index = raid->GetPlayerIndex(memberToDisband->GetName());
			if (index >= 0 && index < MAX_RAID_MEMBERS) {
				wasGrpLdr = raid->members[index].IsGroupLeader;
			}
		}
		if (grp >= 0 && grp < MAX_RAID_GROUPS){
			
			if (wasGrpLdr){
				bool newleader = false;
				for (int x = 0; x < MAX_RAID_MEMBERS; x++)
				{
					if (raid->members[x].GroupNumber == grp)
					{
						if (strlen(raid->members[x].membername) > 0 && strcmp(raid->members[x].membername, memberToDisband->GetName()) != 0)
						{
							raid->SetGroupLeader(raid->members[x].membername, grp);
							newleader = true;
							break;
						}
					}
				}
				if (!newleader)
				{
					// to unset group leader flag, we send the name over of a different group leader
					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (raid->members[x].GroupNumber != grp)
						{
							if (strlen(raid->members[x].membername) > 0 && raid->members[x].IsGroupLeader)
							{
								raid->UnSetGroupLeader(memberToDisband->GetName(), raid->members[x].membername, grp);
								break;
							}
						}
					}
				}
			}
			raid->MoveMember(memberToDisband->GetName(), 0xFFFFFFFF);
			raid->LearnMembers();
			raid->VerifyRaid();
			
			raid->SendRaidGroupRemove(memberToDisband->GetName(), grp, true); //MoveMember removes the player from the group so skip them.
		}
		//we're done
		return;
	}

	Group* group = GetGroup();

	if (!group)
		return;

	if (group->GroupCount() <= 2)
	{
		group->DisbandGroup();
	}
	else if (!group->IsLeader(this)) {
		LeaveGroup();
	}
	else if (group->IsLeader(this) && GetTarget() == this)
	{
		group->DelMember(this);
	}
	else
	{
		Mob* memberToDisband = GetTarget();

		if (!memberToDisband)
			memberToDisband = entity_list.GetMob(gd->name2);

		if (!memberToDisband)
			return;

		// check if person to disband is in same group
		if (memberToDisband != this) {
			Group* group2 = memberToDisband->GetGroup();
			if (!group2)
				return;

			if (group != group2)
				return;
		}

		if (memberToDisband)
		{
			if (group->IsLeader(this))
			{
				// the group leader can kick other members out of the group...
				if (memberToDisband->IsClient())
				{
					group->DelMember(memberToDisband);
				}
			}
			else
			{
				// ...but other members can only remove themselves
				group->DelMember(this);
			}
		}
		else
		{
			Log(Logs::General, Logs::Error, "Failed to remove player from group. Unable to find player named %s in player group", gd->name2);
		}
	}
	return;
}

void Client::Handle_OP_GroupFollow(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupGeneric_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for OP_GroupFollow: Expected: %i, Got: %i",
			sizeof(GroupGeneric_Struct), app->size);
		return;
	}

	if (IsSoloOnly())
	{
		Message(Chat::Red, "You are solo and thus cannot join a group.");
		return;
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "You are a GM. Do not join raids or groups.");
		database.SetHackerFlag(account_name, GetCleanName(), "GM attempted to join a group or raid.");
		return;
	}

	// If we've received the packet and it's valid, then we're either going to join the group or fail in some way. 
	// In either case, the invite should be cleared so just do it now.
	Log(Logs::General, Logs::Group, "%s is clearing the group invite.", GetName());
	ClearGroupInvite();

	GroupGeneric_Struct* gf = (GroupGeneric_Struct*)app->pBuffer;
	Mob* inviter = entity_list.GetClientByName(gf->name1);

	if (inviter != nullptr && inviter->IsClient()) 
	{
		if (!CanGroupWith(inviter->CastToClient()))
		{
			Message(Chat::Red, "The player's challenge modes does not match. You cannot join the group.");
			return;
		}

		strn0cpy(gf->name1, inviter->GetName(), 64);
		strn0cpy(gf->name2, this->GetName(), 64);

		Raid* raid = entity_list.GetRaidByClient(inviter->CastToClient());
		Raid* iraid = entity_list.GetRaidByClient(this);

		//inviter has a raid don't do group stuff instead do raid stuff!
		if (raid)
		{
			if (!CanGroupWith(raid->GetRuleSet()))
			{
				Message(Chat::Red, "The raid's challenge modes is not compatible with yours. You cannot join the raid.");
				return;
			}

			if (raid->RaidCount() >= MAX_RAID_MEMBERS && raid != iraid) {
				Message_StringID(Chat::White, RAID_IS_FULL);
				return;
			}
			if (iraid && raid != iraid) {
				Message(Chat::White, "You are already in a different raid.");
				return;
			}

			uint32 groupToUse = raid->GetGroup(gf->name1);

			if (raid->GroupCount(groupToUse) >= MAX_GROUP_MEMBERS)
			{
				Message_StringID(Chat::White, GROUP_IS_FULL);
				return;
			}
			else
			{
				if (raid->GroupCount(groupToUse) == 1)
				{
					//Invite the inviter into the group first.....dont ask
					auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
					GroupJoin_Struct* outgj = (GroupJoin_Struct*)outapp->pBuffer;
					strcpy(outgj->membername, inviter->GetName());
					strcpy(outgj->yourname, inviter->GetName());
					outgj->action = groupActInviteInitial; // 'You have formed the group'.
					inviter->CastToClient()->QueuePacket(outapp);
					safe_delete(outapp);
				}
			}

			//both in same raid
			if (iraid == raid)
			{ 
				SetRaidGrouped(true);
				raid->MoveMember(GetName(), groupToUse);
				raid->SendGroupUpdate(this);
				raid->GroupJoin(GetName(), groupToUse, this, true);
				raid->SendHPPacketsFrom(this);
				raid->SendHPPacketsTo(this);
				return;
			}
			else if (raid->RaidCount() < MAX_RAID_MEMBERS)
			{
				SetRaidGrouped(true);
				raid->AddMember(this, groupToUse, false, false, false, true);
				raid->SendGroupUpdate(this);
				raid->GroupJoin(GetName(), groupToUse, this, true);
				raid->SendRaidMembers(this);
				raid->SendHPPacketsFrom(this);
				raid->SendHPPacketsTo(this);
				return;
			}
			return;
		}

		Group* group = entity_list.GetGroupByClient(inviter->CastToClient());
		if (group && !CanGroupWith(group->GetRuleSet()))
		{
			Message(Chat::Red, "The group's challenge modes does not match with yours. You cannot join the group.");
			return;
		}

		if (!group)
		{
			//Make new group
			group = new Group(inviter);
			if (!group)
				return;
			entity_list.AddGroup(group);

			if (group->GetID() == 0) 
			{
				Message(Chat::Red, "Unable to get new group id. Cannot create group.");
				inviter->Message(Chat::Red, "Unable to get new group id. Cannot create group.");
				return;
			}

			//now we have a group id, can set inviter's id
			inviter->CastToClient()->UpdateGroupID(group->GetID());
			database.SetGroupLeaderName(group->GetID(), inviter->GetName());
			database.SetGroupOldLeaderName(group->GetID(), inviter->GetName());

			//Invite the inviter into the group first.....dont ask
			auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
			GroupJoin_Struct* outgj = (GroupJoin_Struct*)outapp->pBuffer;
			strcpy(outgj->membername, inviter->GetName());
			strcpy(outgj->yourname, inviter->GetName());
			outgj->action = groupActInviteInitial; // 'You have formed the group'.
			inviter->CastToClient()->QueuePacket(outapp);
			safe_delete(outapp);

			Log(Logs::General, Logs::Group, "Initial group formed by %s", inviter->GetName());

		}

		if (!group)
		{
			return;
		}

		if (!group->AddMember(this))
		{
			return;
		}

		isgrouped = true;
		inviter->CastToClient()->QueuePacket(app);//notify inviter the client accepted

		database.RefreshGroupFromDB(this);
		group->SendHPPacketsTo(this);
		group->SendHPPacketsFrom(this);

		//send updates to clients out of zone...
		auto pack = new ServerPacket(ServerOP_GroupJoin, sizeof(ServerGroupJoin_Struct));
		ServerGroupJoin_Struct* gj = (ServerGroupJoin_Struct*)pack->pBuffer;
		gj->gid = group->GetID();
		gj->zoneid = zone->GetZoneID();
		gj->zoneguildid = zone->GetGuildID();
		strcpy(gj->member_name, GetName());
		worldserver.SendPacket(pack);
		safe_delete(pack);

		Log(Logs::General, Logs::Group, "%s has joined the group.", GetName());
	}
	else
	{
		//Message and invite clear are handled by the client, but just in case.
		Message_StringID(Chat::Red, CANNOT_JOIN_GROUP, gf->name1);
		ClearGroupInvite();
		return;
	}
}

void Client::Handle_OP_GroupInvite(const EQApplicationPacket *app)
{
	//this seems to be the initial invite to form a group
	Handle_OP_GroupInvite2(app);
}

void Client::Handle_OP_GroupInvite2(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupInvite_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for OP_GroupInvite: Expected: %i, Got: %i",
			sizeof(GroupInvite_Struct), app->size);
		return;
	}

	GroupInvite_Struct* gis = (GroupInvite_Struct*)app->pBuffer;

	Mob *Invitee = entity_list.GetMob(gis->invitee_name);

	if (IsSoloOnly())
	{
		Message(Chat::Red, "You have solo mode enabled, and cannot group with anyone.");
		return;
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "You are a GM. Do not join raids or groups.");
		database.SetHackerFlag(account_name, GetCleanName(), "GM attempted to join a group or raid.");
		return;
	}

	if (Invitee == this)
	{
		Message_StringID(Chat::White, GROUP_INVITEE_SELF);
		return;
	}

	Raid* InviterRaid = GetRaid();
	Group* InviterGroup = GetGroup();
	ChallengeRules::RuleSet group_ruleset = InviterRaid ? InviterRaid->GetRuleSet() : InviterGroup ? InviterGroup->GetRuleSet() : GetRuleSet();

	if (Invitee) {
		if (Invitee->IsClient()) {

			if (!Invitee->CastToClient()->CanGroupWith(group_ruleset))
			{
				Message(Chat::Red, "This player has a challenge mode that isn't compatible. They cannot join the group.");
				return;
			}

			if (Invitee->CastToClient()->Admin() > 0)
			{
				Message(Chat::Red, "You are inviting a GM. This will never work.");
				return;
			}

			if (!Invitee->IsGrouped() && !Invitee->IsRaidGrouped())
			{

				if (app->GetOpcode() == OP_GroupInvite2)
				{
					//Make a new packet using all the same information but make sure it's a fixed GroupInvite opcode so we
					//Don't have to deal with GroupFollow2 crap.
					auto outapp =
					    new EQApplicationPacket(OP_GroupInvite, sizeof(GroupInvite_Struct));
					memcpy(outapp->pBuffer, app->pBuffer, outapp->size);
					Invitee->CastToClient()->QueuePacket(outapp);
					safe_delete(outapp);
				}
				else
				{
					//The correct opcode, no reason to bother wasting time reconstructing the packet
					Invitee->CastToClient()->QueuePacket(app);
				}

				Group* group = GetGroup();
				if (group != nullptr)
				{
					uint8 language = 0;
					uint8 lang_skill = 100;
					std::string message = StringFormat("Invited %s to the group.", Invitee->CastToClient()->GetCleanName());
					group->GroupMessage(this, language, lang_skill, message.c_str());
					message = StringFormat("You tell your party, '%s'", message.c_str());
					Message(Chat::Group, message.c_str());
				}
			}
			else if (Invitee->IsRaidGrouped())
			{
				Raid* InviterRaid = GetRaid();
				Raid* InviteeRaid = Invitee->CastToClient()->GetRaid();

				bool leader = false;
				if (InviteeRaid)
				{
					leader = InviteeRaid->IsGroupLeader(Invitee->GetName());
				}
				
				if (InviterRaid != InviteeRaid || leader)
				{
					Message_StringID(Chat::White, ALREADY_IN_GRP_RAID, Invitee->GetName());
				}
				else
				{
					Message_StringID(Chat::White, ALREADY_IN_GROUP, Invitee->GetName());
				}

				return;
			}
			else if (Invitee->IsGrouped())
			{
				auto outapp = new EQApplicationPacket(OP_GroupCancelInvite, sizeof(GroupCancel_Struct));
				GroupCancel_Struct* GroupUnInvite = (GroupCancel_Struct*)outapp->pBuffer;
				strcpy(GroupUnInvite->name1, this->GetName());
				strcpy(GroupUnInvite->name2, Invitee->GetName());
				GroupUnInvite->toggle = 2;
				QueuePacket(outapp);
				safe_delete(outapp);

				Invitee->Message_StringID(Chat::White, GROUP_INVITED, GetName());
			}
		}
	}
	else
	{
		auto pack = new ServerPacket(ServerOP_GroupInvite, sizeof(ServerGroupInvite_Struct));
		ServerGroupInvite_Struct* sgis = (ServerGroupInvite_Struct*)pack->pBuffer;

		memcpy(pack->pBuffer, gis, sizeof(ServerGroupInvite_Struct));
		sgis->group_ruleset = group_ruleset;
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	return;
}

void Client::Handle_OP_GroupUpdate(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupJoin_Struct)-252)
	{
		Log(Logs::General, Logs::Error, "Size mismatch on OP_GroupUpdate: got %u expected %u",
			app->size, sizeof(GroupJoin_Struct)-252);
		DumpPacket(app);
		return;
	}

	GroupJoin_Struct* gu = (GroupJoin_Struct*)app->pBuffer;

	switch (gu->action) 
	{
		case groupActMakeLeader:
		{
			Client* newleader = entity_list.GetClientByName(gu->membername);
			Group* group = this->GetGroup();
			std::string membername = gu->membername;
			std::string yourname = gu->yourname;

			// New leader is in the same zone as the old, just use the client object.
			if (newleader && group && newleader != this) 
			{
				// the client only sends this if it's the group leader, but check anyway
				if (group->IsLeader(this))
					group->ChangeLeader(newleader);
				else 
				{
					Log(Logs::General, Logs::Group, "Group /makeleader request originated from non-leader member: %s", GetName());
					DumpPacket(app);
				}
			}
			// New leader and old leader are in different zones, we have to use the name.
			else if (group && membername != yourname && !membername.empty())
			{
				// the client only sends this if it's the group leader, but check anyway
				if (group->IsLeader(this))
				{
					group->ChangeLeaderByName(membername);
				}
				else
				{
					Log(Logs::General, Logs::Group, "Group /makeleader request originated from non-leader member: %s", GetName());
					DumpPacket(app);
				}
			}

			Raid* raid = entity_list.GetRaidByClient(this);
			std::string raidname;
			if (newleader && newleader != this)
				raidname = newleader->GetName();
			else if(membername != yourname && !membername.empty())
				raidname = membername;

			if (raid && !raidname.empty())
			{
				uint32 me = raid->GetPlayerIndex(this->GetName());
				uint32 grp = raid->GetGroup(raidname.c_str());
				if (me != -1 && raid->members[me].IsGroupLeader && raid->members[me].GroupNumber != 0xFFFFFFFF &&
					raid->members[me].GroupNumber == grp) {
					raid->SetGroupLeader(this->GetName(), grp, false);
					raid->SetGroupLeader(raidname.c_str(), grp, true);
				}
			}
			break;
		}

		default:
		{
			Log(Logs::General, Logs::Group, "Received unhandled OP_GroupUpdate requesting action %u", gu->action);
			DumpPacket(app);
			return;
		}
	}
}

void Client::Handle_OP_GuildDelete(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_GuildDelete");

	if (!IsInAGuild() || !guild_mgr.IsGuildLeader(GuildID(), CharacterID()))
		Message(Chat::White, "You are not a guild leader or not in a guild.");
	else {
		Log(Logs::Detail, Logs::Guilds, "Deleting guild %s (%d)", guild_mgr.GetGuildName(GuildID()), GuildID());
		if (!guild_mgr.DeleteGuild(GuildID()))
			Message(Chat::White, "Guild delete failed.");
		else {
			Message(Chat::White, "Guild successfully deleted.");
		}
	}
}

void Client::Handle_OP_GuildInvite(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_GuildInvite");

	if (app->size != sizeof(GuildCommand_Struct)) {
		std::cout << "Wrong size: OP_GuildInvite, size=" << app->size << ", expected " << sizeof(GuildCommand_Struct) << std::endl;
		return;
	}

	GuildCommand_Struct* gc = (GuildCommand_Struct*)app->pBuffer;

	if (!IsInAGuild())
		Message(Chat::White, "Error: You are not in a guild!");
	else if (gc->officer > GUILD_MAX_RANK)
		Message(Chat::Red, "Invalid rank.");
	else if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {

		//ok, the invite is also used for changing rank as well.
		Mob* invitee = entity_list.GetMob(gc->othername);

		if (!invitee) {
			Message(Chat::Red, "Prospective guild member %s must be in zone to preform guild operations on them.", gc->othername);
			return;
		}

		if (invitee->IsClient()) {
			Client* client = invitee->CastToClient();

			//ok, figure out what they are trying to do.
			if (client->GuildID() == GuildID()) {
				//they are already in this guild, must be a promotion or demotion
				if (gc->officer < client->GuildRank()) {
					//demotion
					if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_DEMOTE)) {
						Message(Chat::Red, "You dont have permission to demote.");
						return;
					}

					//we could send this to the member and prompt them to see if they want to
					//be demoted (I guess), but I dont see a point in that.

					Log(Logs::Detail, Logs::Guilds, "%s (%d) is demoting %s (%d) to rank %d in guild %s (%d)",
						GetName(), CharacterID(),
						client->GetName(), client->CharacterID(),
						gc->officer,
						guild_mgr.GetGuildName(GuildID()), GuildID());

					if (!guild_mgr.SetGuildRank(client->CharacterID(), gc->officer)) {
						Message(Chat::Red, "There was an error during the demotion, DB may now be inconsistent.");
						return;
					}

				}
				else if (gc->officer > client->GuildRank()) {
					//promotion
					if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_PROMOTE)) {
						Message(Chat::Red, "You dont have permission to demote.");
						return;
					}

					Log(Logs::Detail, Logs::Guilds, "%s (%d) is asking to promote %s (%d) to rank %d in guild %s (%d)",
						GetName(), CharacterID(),
						client->GetName(), client->CharacterID(),
						gc->officer,
						guild_mgr.GetGuildName(GuildID()), GuildID());

					//record the promotion with guild manager so we know its valid when we get the reply
					guild_mgr.RecordInvite(client->CharacterID(), GuildID(), gc->officer);

					if (gc->guildeqid == 0)
						gc->guildeqid = GuildID();

					Log(Logs::Detail, Logs::Guilds, "Sending OP_GuildInvite for promotion to %s, length %d", client->GetName(), app->size);
					client->QueuePacket(app);

				}
				else {
					Message(Chat::Red, "That member is already that rank.");
					return;
				}
			}
			else if (!client->IsInAGuild()) {
				//they are not in this or any other guild, this is an invite
				//
				if (client->GetPendingGuildInvitation())
				{
					Message(Chat::Red, "That person is already considering a guild invitation.");
					return;
				}

				if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_INVITE)) {
					Message(Chat::Red, "You dont have permission to invite.");
					return;
				}

				Log(Logs::Detail, Logs::Guilds, "Inviting %s (%d) into guild %s (%d)",
					client->GetName(), client->CharacterID(),
					guild_mgr.GetGuildName(GuildID()), GuildID());

				//record the invite with guild manager so we know its valid when we get the reply
				guild_mgr.RecordInvite(client->CharacterID(), GuildID(), gc->officer);

				if (gc->guildeqid == 0)
					gc->guildeqid = GuildID();

				Log(Logs::Detail, Logs::Guilds, "Sending OP_GuildInvite for invite to %s, length %d", client->GetName(), app->size);
				client->SetPendingGuildInvitation(true);
				client->QueuePacket(app);

			}
			else {
				//they are in some other guild
				Message(Chat::Red, "Player is in a guild.");
				return;
			}
		}
	}
}

void Client::Handle_OP_GuildInviteAccept(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_GuildInviteAccept");

	SetPendingGuildInvitation(false);

	if (app->size != sizeof(GuildInviteAccept_Struct)) {
		std::cout << "Wrong size: OP_GuildInviteAccept, size=" << app->size << ", expected " << sizeof(GuildInviteAccept_Struct) << std::endl;
		return;
	}

	GuildInviteAccept_Struct* gj = (GuildInviteAccept_Struct*)app->pBuffer;


	if (gj->response == 5 || gj->response == 4) {
		//dont care if the check fails (since we dont know the rank), just want to clear the entry.
		guild_mgr.VerifyAndClearInvite(CharacterID(), gj->guildeqid, gj->response);

		worldserver.SendEmoteMessage(gj->inviter, 0, 0, "%s has declined to join the guild.", this->GetName());

		return;
	}

	//uint32 tmpeq = gj->guildeqid;
	if (IsInAGuild() && gj->response == GuildRank())
		Message(Chat::White, "Error: You're already in a guild!");
	else if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {
		Log(Logs::Detail, Logs::Guilds, "Guild Invite Accept: guild %d, response %d, inviter %s, person %s",
			gj->guildeqid, gj->response, gj->inviter, gj->newmember);

		//we dont really care a lot about what this packet means, as long as
		//it has been authorized with the guild manager
		if (!guild_mgr.VerifyAndClearInvite(CharacterID(), gj->guildeqid, gj->response)) {
			worldserver.SendEmoteMessage(gj->inviter, 0, 0, "%s has sent an invalid response to your invite!", GetName());
			Message(Chat::Red, "Invalid invite response packet!");
			return;
		}

		if (gj->guildeqid == GuildID()) {
			//only need to change rank.

			Log(Logs::Detail, Logs::Guilds, "Changing guild rank of %s (%d) to rank %d in guild %s (%d)",
				GetName(), CharacterID(),
				gj->response,
				guild_mgr.GetGuildName(GuildID()), GuildID());

			if (!guild_mgr.SetGuildRank(CharacterID(), gj->response)) {
				Message(Chat::Red, "There was an error during the rank change, DB may now be inconsistent.");
				return;
			}
		}
		else {

			Log(Logs::Detail, Logs::Guilds, "Adding %s (%d) to guild %s (%d) at rank %d",
				GetName(), CharacterID(),
				guild_mgr.GetGuildName(gj->guildeqid), gj->guildeqid,
				gj->response);

			//change guild and rank

			uint32 guildrank = gj->response;

			if (!guild_mgr.SetGuild(CharacterID(), gj->guildeqid, guildrank)) {
				Message(Chat::Red, "There was an error during the invite, DB may now be inconsistent.");
				return;
			}
		}
	}
}

void Client::Handle_OP_GuildLeader(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_GuildLeader");

	if (app->size < 2) {
		Log(Logs::Detail, Logs::Guilds, "Invalid length %d on OP_GuildLeader", app->size);
		return;
	}

	app->pBuffer[app->size - 1] = 0;
	GuildMakeLeader* gml = (GuildMakeLeader*)app->pBuffer;
	if (!IsInAGuild())
		Message(Chat::White, "Error: You arent in a guild!");
	else if (GuildRank() != GUILD_LEADER)
		Message(Chat::White, "Error: You arent the guild leader!");
	else if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {

		//NOTE: we could do cross-zone lookups here...
		char target[64];
		strcpy(target, gml->name);

		Client* newleader = entity_list.GetClientByName(target);
		if (newleader) {

			Log(Logs::Detail, Logs::Guilds, "Transfering leadership of %s (%d) to %s (%d)",
				guild_mgr.GetGuildName(GuildID()), GuildID(),
				newleader->GetName(), newleader->CharacterID());

			if (guild_mgr.SetGuildLeader(GuildID(), newleader->CharacterID())){
				Message(Chat::White, "Successfully Transfered Leadership to %s.", target);
				newleader->Message(Chat::Yellow, "%s has transfered the guild leadership into your hands.", GetName());
			}
			else
				Message(Chat::White, "Could not change leadership at this time.");
		}
		else
			Message(Chat::White, "Failed to change leader, could not find target.");
	}
	return;
}

void Client::Handle_OP_GuildPeace(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Got OP_GuildPeace of len %d", app->size);
	return;
}

void Client::Handle_OP_GuildRemove(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_GuildRemove");

	if (app->size != sizeof(GuildCommand_Struct)) {
		std::cout << "Wrong size: OP_GuildRemove, size=" << app->size << ", expected " << sizeof(GuildCommand_Struct) << std::endl;
		return;
	}
	GuildCommand_Struct* gc = (GuildCommand_Struct*)app->pBuffer;
	if (!IsInAGuild())
		Message(Chat::White, "Error: You are not in a guild!");

	if (GuildID() == 1)
	{
		Message(Chat::White, "You can't leave this guild. File a petition for more information.");
		return;
	}
	
	// we can always remove ourself, otherwise, our rank needs remove permissions
	else if (strcasecmp(gc->othername, GetName()) != 0 &&
		!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_REMOVE))
		Message(Chat::White, "You dont have permission to remove guild members.");
	else if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {
		uint32 char_id;
		Client* client = entity_list.GetClientByName(gc->othername);
		Client* remover = entity_list.GetClientByName(gc->myname);

		if (client) {
			if (!client->IsInGuild(GuildID())) {
				Message(Chat::White, "You aren't in the same guild, what do you think you are doing?");
				return;
			}
			char_id = client->CharacterID();

			if (client->GuildRank() >= remover->GuildRank() && strcmp(client->GetName(), remover->GetName()) != 0){
				Message(Chat::White, "You can't remove a player from the guild with an equal or higher rank to you!");
				return;
			}

			Log(Logs::Detail, Logs::Guilds, "Removing %s (%d) from guild %s (%d)",
				client->GetName(), client->CharacterID(),
				guild_mgr.GetGuildName(GuildID()), GuildID());
		}
		else {
			CharGuildInfo gci;
			CharGuildInfo gci_;
			if (!guild_mgr.GetCharInfo(gc->myname, gci_)) {
				Message(Chat::White, "Unable to find '%s'", gc->myname);
				return;
			}
			if (!guild_mgr.GetCharInfo(gc->othername, gci)) {
				Message(Chat::White, "Unable to find '%s'", gc->othername);
				return;
			}
			if (gci.guild_id != GuildID()) {
				Message(Chat::White, "You aren't in the same guild, what do you think you are doing?");
				return;
			}
			if (gci.rank >= gci_.rank) {
				Message(Chat::White, "You can't remove a player from the guild with an equal or higher rank to you!");
				return;
			}

			char_id = gci.char_id;

			Log(Logs::Detail, Logs::Guilds, "Removing remote/offline %s (%d) into guild %s (%d)",
				gci.char_name.c_str(), gci.char_id, guild_mgr.GetGuildName(GuildID()), GuildID());
		}

		uint32 guid = remover->GuildID();
		if (!guild_mgr.SetGuild(char_id, GUILD_NONE, 0)) {
			auto outapp = new EQApplicationPacket(OP_GuildRemove, sizeof(GuildRemove_Struct));
			GuildRemove_Struct* gm = (GuildRemove_Struct*)outapp->pBuffer;
			gm->guildeqid = guid;
			strcpy(gm->Removee, gc->othername);
			Message(Chat::White, "%s successfully removed from your guild.", gc->othername);
			entity_list.QueueClientsGuild(this, outapp, false, GuildID());
			safe_delete(outapp);
		}
	}
	return;
}

void Client::Handle_OP_GuildWar(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Got OP_GuildWar of len %d", app->size);
	return;
}

void Client::Handle_OP_Hide(const EQApplicationPacket *app)
{
	// TODO: this is not correct and causes the skill to never level up until you manually train it to be above 0.
	// Need to differentiate between 0, 254 (untrained) and 255 (can't learn) values but 0 is treated as not having the skill in many places.
	if (!HasSkill(EQ::skills::SkillHide))
	{
		return;
	}

	if (!p_timers.Expired(&database, pTimerHide, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}
	int reuse = HideReuseTime - GetAA(209);
	p_timers.Start(pTimerHide, reuse - 1);

	int hidechance = std::max(10, (int)GetSkill(EQ::skills::SkillHide));
	int random = zone->random.Int(0, 99);
	uint8 success = SKILLUP_FAILURE;
	if (random < hidechance) 
	{
		if (GetAA(aaShroudofStealth))
		{
			improved_hidden = true;
		}
		hidden = true;
		SetInvisible(INVIS_HIDDEN);
		success = SKILLUP_SUCCESS;
	}
	else
	{
		hidden = false;
		improved_hidden = false;
		bool skipself = GetClass() != Class::Rogue;

		// This will also "bug" invisible spells and is intentional.
		SetInvisible(INVIS_OFF, true, skipself);
		if (skipself)
		{
			// Non-Rogues will see themselves as invisible even if they fail. 
			SendAppearancePacket(AppearanceType::Invisibility , 1, false, false, this);
		}
	}

	if (GetClass() == Class::Rogue)
	{
		Mob *evadetar = GetTarget();
		if (!auto_attack && (evadetar && evadetar->CheckAggro(this)
			&& evadetar->IsNPC()))
		{
			if (zone->random.Int(0, 260) < (int)GetSkill(EQ::skills::SkillHide))
			{
				Message_StringID(Chat::Skills, EVADE_SUCCESS);
				RogueEvade(evadetar);
				success = SKILLUP_SUCCESS;
			}
			else
			{
				Message_StringID(Chat::Skills, EVADE_FAIL);
			}
		}
		else
		{
			if (hidden)
			{
				Message_StringID(Chat::Skills, HIDE_SUCCESS);
				success = SKILLUP_SUCCESS;
			}
			else
				Message_StringID(Chat::Skills, HIDE_FAIL);
		}
	}

	CheckIncreaseSkill(EQ::skills::SkillHide, nullptr, zone->skill_difficulty[EQ::skills::SkillHide].difficulty, success);

	Log(Logs::General, Logs::Skills, "Hide setting hide to %d. %s", hidden, !hidden && GetClass() != Class::Rogue ? "Sending to self only..." : "");

	return;
}

void Client::Handle_OP_Ignore(const EQApplicationPacket *app)
{
}

void Client::Handle_OP_Illusion(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Illusion_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized OP_Illusion: got %d, expected %d", app->size,
			sizeof(Illusion_Struct));
		DumpPacket(app);
		return;
	}

	if (!GetGM())
	{
		database.SetMQDetectionFlag(this->AccountName(), this->GetName(), "OP_Illusion sent by non Game Master.", zone->GetShortName());
		return;
	}

	Illusion_Struct* bnpc = (Illusion_Struct*)app->pBuffer;
	//these need to be implemented
	/*
	texture		= bnpc->texture;
	helmtexture	= bnpc->helmtexture;
	luclinface	= bnpc->luclinface;
	*/
	race = bnpc->race;
	size = 0;

	entity_list.QueueClients(this, app);
	return;
}

void Client::Handle_OP_InspectAnswer(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(InspectResponse_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_InspectAnswer, size=%i, expected %i", app->size, sizeof(InspectResponse_Struct));
		return;
	}

	EQApplicationPacket* outapp = app->Copy();
	InspectResponse_Struct* insr = (InspectResponse_Struct*)outapp->pBuffer;
	Mob* tmp = entity_list.GetMob(insr->TargetID);

	if (tmp != 0 && tmp->IsClient())
	{
		tmp->CastToClient()->QueuePacket(outapp);
	}
	safe_delete(outapp);

	return;
}

void Client::Handle_OP_InspectRequest(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(Inspect_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_InspectRequest, size=%i, expected %i", app->size, sizeof(Inspect_Struct));
		return;
	}

	Inspect_Struct* ins = (Inspect_Struct*)app->pBuffer;
	Mob* tmp = entity_list.GetMob(ins->TargetID);

	if (tmp != 0 && tmp->IsClient()) {
		tmp->CastToClient()->QueuePacket(app);
	} // Send request to target

	return;
}

void Client::Handle_OP_InstillDoubt(const EQApplicationPacket *app) 
{
	//packet is empty as of 12/14/04

	if (!p_timers.Expired(&database, pTimerInstillDoubt, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerInstillDoubt, InstillDoubtReuseTime - 1);

	InstillDoubt(GetTarget());
	return;
}

void Client::Handle_OP_ItemLinkResponse(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(ItemViewRequest_Struct)) {
		Log(Logs::General, Logs::Error, "OP size error: OP_ItemLinkResponse expected:%i got:%i", sizeof(ItemViewRequest_Struct), app->size);
		return;
	}

	ItemViewRequest_Struct *ivrs = (ItemViewRequest_Struct *)app->pBuffer;

	if (ivrs->item_id > 0x8000) // saylink uses item ids > 0x8000 (32768)
	{
		std::string response = "";
		int sayid = ivrs->item_id - 0x8000;
		bool silentsaylink = false;

		if (sayid > 0x4000)	//Silent Saylink
		{
			sayid = sayid - 0x4000;
			silentsaylink = true;
		}

		if (sayid > 0)
		{
			std::string query = StringFormat("SELECT `phrase` FROM saylink WHERE id%%0x4000 = %i ORDER BY id DESC LIMIT 1", sayid);
			auto results = database.QueryDatabase(query);
			if (!results.Success())
			{
				Message(Chat::Red, "Error: The saylink (%s) was not found in the database.", response.c_str());
				return;
			}

			if (results.RowCount() != 1)
			{
				Message(Chat::Red, "Error: The saylink (%s) was not found in the database.", response.c_str());
				return;
			}

			auto row = results.begin();
			response = row[0];

		}

		if (!response.empty())
		{
			if (!silentsaylink)
			{
				Message(Chat::LightGray, "You say, '%s'", response.c_str());
			}

			ChannelMessageReceived(ChatChannel_Say, Language::CommonTongue, Language::MaxValue, response.c_str());

			return;
		}
		else
		{
			Message(Chat::Red, "Error: The item for the link you have clicked on does not exist!");
			return;
		}
	}
	else // item link
	{
		const EQ::ItemData *item = database.GetItem(ivrs->item_id);
		if (!item) 
		{
			Message(Chat::Red, "Error: The item for the link you have clicked on does not exist!");
			return;
		}
	}

	EQ::ItemInstance* inst = database.CreateItem(ivrs->item_id);
	if (inst)
	{
		SendItemPacket(0, inst, ItemPacketViewLink);
		safe_delete(inst);
	}
}

void Client::Handle_OP_Jump(const EQApplicationPacket *app)
{
	SetFatigue(GetFatigue() + 10);
	CalcBonuses();
}

void Client::Handle_OP_LeaveBoat(const EQApplicationPacket *app)
{

	if(m_pp.boatid > 0)
	{
		Log(Logs::Detail, Logs::Boats, "%s is attempting to leave boat %s at %0.2f,%0.2f,%0.2f ", GetName(), m_pp.boat, GetX(), GetY(), GetZ());
	}
	else
	{
		Log(Logs::Detail, Logs::Boats, "%s recieved OP_LeaveBoat", GetName());
	}

	Mob* boat = entity_list.GetMob(this->BoatID);	// find the mob corresponding to the boat id
	if (boat) 
	{
		if ((boat->GetTarget() == this) && boat->GetHateAmount(this) == 0)	// if the client somehow left while still controlling the boat (and the boat isn't attacking them)
			boat->SetTarget(0);			// fix it to stop later problems

		std::string export_string = fmt::format("{}", boat->GetNPCTypeID());
		parse->EventPlayer(EVENT_LEAVE_BOAT, this, export_string, 0);
	}

	this->BoatID = 0;
	m_pp.boatid = 0;
	m_pp.boat[0] = 0;
	return;
}

void Client::Handle_OP_Logout(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Character, "%s sent a logout packet.", GetName());

	if (camping)
	{
		if (IsGrouped())
			LeaveGroup();

		Raid *myraid = entity_list.GetRaidByClient(this);
		if (myraid)
		{
			myraid->DisbandRaidMember(GetName());
		}

		Save();
		instalog = true;
		database.ClearAccountActive(this->AccountID());
		camp_timer.Disable();
		camping = false;
		camp_desktop = false;
	}

	SendLogoutPackets();

	if (camp_desktop) {
		if (worldserver.Connected()) {
			ServerPacket kickPlayerPack(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
			ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)kickPlayerPack.pBuffer;
			strcpy(skp->adminname, "CampDesktop");
			strcpy(skp->name, GetName());
			skp->adminrank = 255;
			worldserver.SendPacket(&kickPlayerPack);
		}
		HardDisconnect();
		return;
	}
	else {
		auto outapp = new EQApplicationPacket(OP_LogoutReply, 2);
		FastQueuePacket(&outapp);
	}

	Disconnect();
	return;
}

void Client::Handle_OP_LootItem(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(LootingItem_Struct)) {
		LogError("Wrong size: OP_LootItem, size=[{}], expected [{}]", app->size, sizeof(LootingItem_Struct));
		return;
	}


	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	auto* l = (LootingItem_Struct*)app->pBuffer;
	auto entity = entity_list.GetID(*((uint16*)app->pBuffer));
	if (!entity) {
		auto outapp = new EQApplicationPacket(OP_LootComplete, 0);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (!entity->IsCorpse()) {
		Corpse::SendEndLootErrorPacket(this);
		return;
	}

	entity->CastToCorpse()->LootCorpseItem(this, app);
}

void Client::Handle_OP_LootRequest(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(uint16)) {
		std::cout << "Wrong size: OP_EndLootRequest, size=" << app->size << ", expected " << sizeof(uint16) << std::endl;
		return;
	}

	Entity* ent = entity_list.GetID(*((uint32*)app->pBuffer));
	if (ent == 0) {
		//	Message(Chat::Red, "Error: OP_LootRequest: Corpse not found (ent = 0)");
		Corpse::SendLootReqErrorPacket(this);
		return;
	}
	if (ent->IsCorpse())
	{
		SetLooting(ent->GetID()); //store the entity we are looting
		Corpse *ent_corpse = ent->CastToCorpse();
		if (DistanceSquaredNoZ(m_Position, ent_corpse->GetPosition()) > 1100) // About 33.2 coords away.
		{
			Message(Chat::Red, "[DEBUG] Too far away from corpse.");
			Corpse::SendLootReqErrorPacket(this);
			return;
		}

		CommonBreakInvisNoSneak();
		ent->CastToCorpse()->MakeLootRequestPackets(this, app);
		return;
	}
	else {
		Corpse::SendLootReqErrorPacket(this);
	}
	return;
}

void Client::Handle_OP_ManaChange(const EQApplicationPacket *app)
{
	if (app->size == 0) {
		// i think thats the sign to stop the songs
		if (IsBardSong(casting_spell_id) || bardsong != 0)
			InterruptSpell(SONG_ENDS, Chat::SpellFailure, SPELL_UNKNOWN);
		else
			InterruptSpell(INTERRUPT_SPELL, Chat::SpellFailure, SPELL_UNKNOWN);

		return;
	}
	else	// I don't think the client sends proper manachanges
	{			// with a length, just the 0 len ones for stopping songs
		//ManaChange_Struct* p = (ManaChange_Struct*)app->pBuffer;
		Log(Logs::General, Logs::Status, "OP_ManaChange from client:\n");
		//DumpPacket(app);
	}
	return;
}

#if 0	// I dont think there's an op for this now, and we check this
// when the client is sitting
void Client::Handle_OP_Medding(const EQApplicationPacket *app)
{
	if (app->pBuffer[0])
		medding = true;
	else
		medding = false;
	return;
}
#endif

void Client::Handle_OP_MemorizeSpell(const EQApplicationPacket *app) 
{
	OPMemorizeSpell(app);
	return;
}

void Client::Handle_OP_Mend(const EQApplicationPacket *app)
{
	if (GetClass() != Class::Monk)
		return;

	if (!p_timers.Expired(&database, pTimerMend, false)) 
	{
		// The client clears the mend skill button whenever a player zones/camps. Pressing the button again causes the client to set mend's 
		// timer to the full duration. Meaning, the server side timer will expire but the player will be unable to use the skill because the
		// button is still disabled. This sets a new server timer with mend's remaining time that will be checked in Client::Process() and
		// calls ResetSkill() to reset the button for the player. 
		mend_reset_timer.Start(p_timers.GetRemainingTime(pTimerMend)*1000);
		Message(Chat::White, "Ability recovery time not yet met.");
		Log(Logs::General, Logs::Error, "%d seconds remain of the Mend recovery timer. ", p_timers.GetRemainingTime(pTimerMend));
		return;
	}

	p_timers.Start(pTimerMend, MendReuseTime - 1);

	int mendhp = GetMaxHP() / 4;
	int currenthp = GetHP();
	uint8 success = SKILLUP_FAILURE;
	if (zone->random.Int(0, 99) < (int)GetSkill(EQ::skills::SkillMend)) {

		int criticalchance = spellbonuses.CriticalMend + itembonuses.CriticalMend + aabonuses.CriticalMend;

		if (zone->random.Int(0, 99) < criticalchance){
			mendhp *= 2;
			Message_StringID(Chat::LightBlue, MEND_CRITICAL);
		}
		SetHP(GetHP() + mendhp);
		SendHPUpdate();
		Message_StringID(Chat::LightBlue, MEND_SUCCESS);
		success = SKILLUP_SUCCESS;
	}
	else {
		/* the purpose of the following is to make the chance to worsen wounds much less common,
		which is more consistent with the way eq live works.
		according to my math, this should result in the following probability:
		0 skill - 25% chance to worsen
		20 skill - 23% chance to worsen
		50 skill - 16% chance to worsen */
		if ((GetSkill(EQ::skills::SkillMend) <= 50) && (zone->random.Int(GetSkill(EQ::skills::SkillMend), 100) < 50) && (zone->random.Int(1, 3) == 1))
		{
			SetHP(currenthp > mendhp ? (GetHP() - mendhp) : 1);
			SendHPUpdate();
			Message_StringID(Chat::LightBlue, MEND_WORSEN);
		}
		else
			Message_StringID(Chat::LightBlue, MEND_FAIL);
	}

	CheckIncreaseSkill(EQ::skills::SkillMend, nullptr, zone->skill_difficulty[EQ::skills::SkillMend].difficulty, success);
	return;
}

void Client::Handle_OP_MoveCoin(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MoveCoin_Struct)){
		Log(Logs::General, Logs::Error, "Wrong size on OP_MoveCoin. Got: %i, Expected: %i", app->size, sizeof(MoveCoin_Struct));
		DumpPacket(app);
		return;
	}

	OPMoveCoin(app);

	return;
}

void Client::Handle_OP_MoveItem(const EQApplicationPacket *app)
{

	if (!CharacterID())
	{
		return;
	}

	if (app->size != sizeof(MoveItem_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_MoveItem, size=%i, expected %i", app->size, sizeof(MoveItem_Struct));
		return;
	}

	MoveItem_Struct* mi = (MoveItem_Struct*)app->pBuffer;
	Log(Logs::Detail, Logs::Inventory, "Moveitem from_slot: %i, to_slot: %i, number_in_stack: %i", mi->from_slot, mi->to_slot, mi->number_in_stack);

	if (spellend_timer.Enabled() && casting_spell_id && !IsBardSong(casting_spell_id) && mi->from_slot != EQ::invslot::slotCursor)
	{
		if (mi->from_slot != mi->to_slot && (mi->from_slot <= EQ::invslot::GENERAL_END || mi->from_slot > 39) && IsValidSlot(mi->from_slot) && IsValidSlot(mi->to_slot))
		{
			const EQ::ItemInstance *itm_from = GetInv().GetItem(mi->from_slot);
			const EQ::ItemInstance *itm_to = GetInv().GetItem(mi->to_slot);
			auto detect = fmt::format("Player issued a move item from {} (item id {} ) to {} (item id {} ) while casting {} .",
				mi->from_slot,
				itm_from ? itm_from->GetID() : 0,
				mi->to_slot,
				itm_to ? itm_to->GetID() : 0,
				casting_spell_id);
			database.SetMQDetectionFlag(AccountName(), GetName(), detect, zone->GetShortName());
			Kick(); // Kick client to prevent client and server from getting out-of-sync inventory slots
			return;
		}
	}

	// Illegal bagslot useage checks. Currently, user only receives a message if this check is triggered.
	bool mi_hack = false;

	if (mi->from_slot >= EQ::invbag::GENERAL_BAGS_BEGIN && mi->from_slot <= EQ::invbag::CURSOR_BAG_END) {
		if (mi->from_slot >= EQ::invbag::CURSOR_BAG_BEGIN) { mi_hack = true; }
		else {
			int16 from_parent = m_inv.CalcSlotId(mi->from_slot);
			if (!m_inv[from_parent]) { mi_hack = true; }
			else if (!m_inv[from_parent]->IsType(EQ::item::ItemClassBag)) { mi_hack = true; }
			else if (m_inv.CalcBagIdx(mi->from_slot) >= m_inv[from_parent]->GetItem()->BagSlots) { mi_hack = true; }
		}
	}

	if (mi->to_slot >= EQ::invbag::GENERAL_BAGS_BEGIN && mi->to_slot <= EQ::invbag::CURSOR_BAG_END) {
		if (mi->to_slot >= EQ::invbag::CURSOR_BAG_BEGIN) { mi_hack = true; }
		else {
			int16 to_parent = m_inv.CalcSlotId(mi->to_slot);
			if (!m_inv[to_parent]) { mi_hack = true; }
			else if (!m_inv[to_parent]->IsType(EQ::item::ItemClassBag)) { mi_hack = true; }
			else if (m_inv.CalcBagIdx(mi->to_slot) >= m_inv[to_parent]->GetItem()->BagSlots) { mi_hack = true; }
		}
	}
	
	if (mi_hack) { 
		Message(Chat::Yellow, "Caution: Illegal use of inaccessable bag slots! Resyncing item.");
		SwapItemResync(mi);
		return;
	}

	if (IsValidSlot(mi->from_slot) && IsValidSlot(mi->to_slot)) {
		bool si = SwapItem(mi);
		if (!si)
		{
			Log(Logs::Detail, Logs::Inventory, "WTF Some shit failed. SwapItem: %i, IsValidSlot (from): %i, IsValidSlot (to): %i", si, IsValidSlot(mi->from_slot), IsValidSlot(mi->to_slot));
			SwapItemResync(mi);

			bool error = false;
			InterrogateInventory(this, false, true, false, error, false);
			if (error)
				InterrogateInventory(this, true, false, true, error);
		}
	}

	return;
}

void Client::Handle_OP_PetCommands(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(PetCommand_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_PetCommands, size=%i, expected %i", app->size, sizeof(PetCommand_Struct));
		return;
	}
	char val1[20] = { 0 };
	PetCommand_Struct* pet = (PetCommand_Struct*)app->pBuffer;
	Mob* mypet = this->GetPet();
	Mob *target = entity_list.GetMob(pet->target);

	if (pet->command == PET_LEADER)
	{
		if (mypet && (!GetTarget() || GetTarget() == mypet || (GetTarget() && !GetTarget()->IsPet())))
		{
			// Client's pet.
			mypet->Say_StringID(PET_LEADERIS, GetName());
		}
		else if (GetTarget() && GetTarget() != mypet && (GetTarget()->IsPet() || GetTarget()->GetPetType() == petOrphan))
		{
			Mob* theirpet = GetTarget();
			if (theirpet != nullptr)
			{
				Mob *Owner = GetTarget()->GetOwner();
				if (Owner != nullptr)
				{
					// Somebody else's pet.
					theirpet->Say_StringID(PET_LEADERIS, Owner->GetCleanName());
				}
				else
				{
					// Pet's owner is dead.
					theirpet->Say_StringID(I_FOLLOW_NOONE);
				}
			}
		}

		return;
	}

	if (mypet == nullptr)
		return;

	if (mypet->GetPetType() == petOrphan)
		return;

	if (mypet->GetPetType() == petAnimation && (pet->command != PET_HEALTHREPORT && pet->command != PET_GETLOST) && !GetAA(aaAnimationEmpathy))
		return;

	if (mypet->GetPetType() == petHatelist && pet->command != PET_HEALTHREPORT)
		return;

	// just let the command "/pet get lost" work for familiars
	if (mypet->GetPetType() == petFamiliar && pet->command != PET_GETLOST)
		return;

	uint32 PetCommand = pet->command;

	switch (PetCommand)
	{
	case PET_ATTACK: {
		if (!target)
			break;
		if (target->IsMezzed()) {
			Message_StringID(Chat::White, CANNOT_WAKE, mypet->GetCleanName(), target->GetCleanName());
			break;
		}
		if (mypet->IsFeared())
			break; //prevent pet from attacking stuff while feared

		if (!mypet->IsAttackAllowed(target)) {
			mypet->Say_StringID(NOT_LEGAL_TARGET);
			break;
		}

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 2) || mypet->GetPetType() != petAnimation)
		{
			mypet->SetHeld(false);
			
			if (DistanceSquared(m_Position, target->GetPosition()) <= 350.0f*350.0f || GetGM())
			{
				if (GetZoneID() == Zones::VEXTHAL && std::abs(target->GetPosition().z - m_Position.z) > 50.0f)		// prevent pets from being used to pull through floors
					return;

				if (!mypet->CheckAggro(target))
					mypet->AddToHateList(target, 1);
				else
					mypet->AddToHateList(target);
			}
			else {
				break;
			}
			Message_StringID(Chat::PetResponse, PET_ATTACKING, mypet->GetCleanName(), target->GetCleanName());
		}
		break;
	}
	case PET_BACKOFF: {
		if (mypet->IsFeared()) break; //keeps pet running while feared

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(Chat::PetResponse, PET_CALMING);
			mypet->WipeHateList();
			mypet->SetTarget(nullptr);
			tar_ndx = 20;
		}
		break;
	}
	case PET_HEALTHREPORT: 
	{
		uint8 HP_ratio = static_cast<uint8>(mypet->GetHPRatio());
		Message_StringID(Chat::PetResponse, PET_REPORT_HP, ConvertArray(HP_ratio, val1));
		mypet->ShowBuffList(this);
		break;
	}
	case PET_GETLOST: {
		// Cant tell a charmed pet to get lost
		if (mypet->GetPetType() == PetType::petCharmed) {
			break;
		}

		mypet->Say_StringID(Chat::PetResponse, PET_GETLOST_STRING);
		mypet->CastToNPC()->Depop();
		//mypet->Death(nullptr, 0, SPELL_UNKNOWN, SkillTigerClaw, Killed_ENV);

		FadeVoiceGraft();

		break;
	}
	case PET_GUARDHERE: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 1) || mypet->GetPetType() != petAnimation) {
			if (mypet->IsNPC()) {
				mypet->Say_StringID(Chat::PetResponse, PET_GUARDINGLIFE);
				mypet->SetPetOrder(SPO_Guard);
				if (!mypet->GetTarget()) // want them to not twitch if they're chasing something down
					mypet->StopNavigation();
				glm::vec3 petloc = glm::vec3(mypet->GetPosition().x, mypet->GetPosition().y, mypet->GetPosition().z);
				mypet->Teleport(petloc);
				mypet->FixZ();
				mypet->CastToNPC()->SaveGuardSpot();
			}
		}
		break;
	}
	case PET_FOLLOWME: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 1) || mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(Chat::PetResponse, PET_FOLLOWING);
			mypet->SetPetOrder(SPO_Follow);
			mypet->SendAppearancePacket(AppearanceType::Animation, Animation::Standing);
		}
		break;
	}
	case PET_TAUNT: {
		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			if(mypet->CastToNPC()->IsTaunting())
			{
				Message_StringID(Chat::PetResponse, PET_NO_TAUNT);
				mypet->CastToNPC()->SetTaunting(false);
			}
			else
			{
				Message_StringID(Chat::PetResponse, PET_DO_TAUNT);
				mypet->CastToNPC()->SetTaunting(true);
			}
		}
		break;
	}
	case PET_NOTAUNT: {
		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			Message_StringID(Chat::PetResponse, PET_NO_TAUNT);
			mypet->CastToNPC()->SetTaunting(false);
		}
		break;
	}
	case PET_GUARDME: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 1) || mypet->GetPetType() != petAnimation) {
			mypet->SetHeld(false);
			mypet->Say_StringID(Chat::PetResponse, PET_GUARDME_STRING);
			mypet->SetPetOrder(SPO_Follow);
			mypet->SendAppearancePacket(AppearanceType::Animation, Animation::Standing);
		}
		break;
	}
	case PET_SITDOWN: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
				mypet->Say_StringID(Chat::PetResponse, PET_SIT_STRING);
				mypet->SetPetOrder(SPO_Sit);
				if (!mypet->UseBardSpellLogic())	//maybe we can have a bard pet
					mypet->InterruptSpell(); //No cast 4 u. //i guess the pet should start casting
				mypet->SendAppearancePacket(AppearanceType::Animation, Animation::Sitting);
				mypet->StopNavigation();
				glm::vec3 petloc = glm::vec3(mypet->GetPosition().x, mypet->GetPosition().y, mypet->GetPosition().z);
				mypet->Teleport(petloc);
				mypet->FixZ();
		}
		break;
	}
	case PET_STANDUP: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(Chat::PetResponse, PET_SIT_STRING);
			mypet->SetPetOrder(SPO_Follow);
			mypet->SendAppearancePacket(AppearanceType::Animation, Animation::Standing);
		}
		break;
	}
	case PET_SLUMBER: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if (!mypet->IsCharmedPet() && GetAA(aaFeignedMinion)) 
		{
			// using one of these otherwise never used timers so we don't have to make a new member just for this.  hopefully this is not confusing
			Timer* timer = GetSpecialAbilityTimer(SpecialAbility::FeignDeathImmunity);
			if (timer && timer->Enabled() && !timer->Check())
			{
				Message(Chat::LightBlue, "You must wait longer before your pet can feign death.");
				return;
			}

			uint8 chance = 0;
			switch (GetAA(aaFeignedMinion))
			{
			case 1:
				chance = 25;
				break;
			case 2:
				chance = 50;
				break;
			case 3:
				chance = 75;
				break;
			default:
				chance = 0;
				break;
			}

			mypet->InterruptSpell();
			mypet->WipeHateList();
			mypet->SetTarget(nullptr);
			mypet->Say_StringID(Chat::PetResponse, PET_CALMING);
			mypet->SetPetOrder(SPO_Sit);
			mypet->SendAppearancePacket(AppearanceType::Animation, Animation::Lying);

			StartSpecialAbilityTimer(SpecialAbility::FeignDeathImmunity, FeignDeathReuseTime*1000);

			if (zone->random.Roll(chance))
				entity_list.RemoveFromNPCTargets(mypet);
			else
				entity_list.MessageClose_StringID(this, false, 200, 10, STRING_FEIGNFAILED, mypet->GetCleanName());
		}
		break;
	}
	case PET_HOLD: {
		if (GetAA(aaPetDiscipline) && mypet->IsNPC()) {
			if (mypet->IsFeared())
				break; //could be exploited like PET_BACKOFF

			mypet->Say_StringID(Chat::PetResponse, PET_ON_HOLD);
			mypet->SetHeld(true);
			mypet->WipeHateList();
			mypet->SetTarget(nullptr);
		}
		break;
	}
	default:
		Log(Logs::General, Logs::Pets,"Client attempted to use a unknown pet command:\n");
		break;
	}
}

void Client::Handle_OP_Petition(const EQApplicationPacket *app)
{
	if (!RuleB(Petitions, PetitionSystemActive)) {
		Message(Chat::Default, "Petitionining ingame is disabled. Please report petitions on the server's Discord using the ticketing system there.");
		return;
	}

	if (app->size <= 1)
		return;
	if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	/*else if(petition_list.FindPetitionByAccountName(this->AccountName()))
	{
	Message(Chat::White,"You already have a petition in queue, you cannot petition again until this one has been responded to or you have deleted the petition.");
	return;
	}*/
	else
	{
		if (petition_list.FindPetitionByAccountName(AccountName()))
		{
			Message(Chat::White, "You already have a petition in the queue, you must wait for it to be answered or use /deletepetition to delete it.");
			return;
		}
		auto pet = new Petition(CharacterID());
		pet->SetAName(this->AccountName());
		pet->SetClass(this->GetClass());
		pet->SetLevel(this->GetLevel());
		pet->SetCName(this->GetName());
		pet->SetRace(this->GetRace());
		pet->SetLastGM("");
		pet->SetCName(this->GetName());
		pet->SetPetitionText((char*)app->pBuffer);
		pet->SetZone(zone->GetZoneID());
		pet->SetUrgency(0);
		petition_list.AddPetition(pet);
		database.InsertPetitionToDB(pet);
		petition_list.UpdateGMQueue();
		petition_list.UpdateZoneListQueue();
		worldserver.SendEmoteMessage(0, 0, 80, 15, "%s has made a petition. #%i", GetName(), pet->GetID());
	}
	return;
}

void Client::Handle_OP_PetitionCheckIn(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Petition_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_PetitionCheckIn, size=%i, expected %i", app->size, sizeof(Petition_Struct));
		return;
	}
	if (!RuleB(Petitions, PetitionSystemActive)) {
		Message(Chat::Default, "Petitionining ingame is disabled. Please report petitions on the server's Discord using the ticketing system there.");
		return;
	}

	Petition_Struct* inpet = (Petition_Struct*)app->pBuffer;

	Petition* pet = petition_list.GetPetitionByID(inpet->petnumber);
	//if (inpet->urgency != pet->GetUrgency())
	pet->SetUrgency(inpet->urgency);
	pet->SetLastGM(this->GetName());
	pet->SetGMText(inpet->gmtext);

	pet->SetCheckedOut(false);
	petition_list.UpdatePetition(pet);
	petition_list.UpdateGMQueue();
	petition_list.UpdateZoneListQueue();
	return;
}

void Client::Handle_OP_PetitionCheckout(const EQApplicationPacket *app)
{
	if (app->size != sizeof(uint32)) {
		std::cout << "Wrong size: OP_PetitionCheckout, size=" << app->size << ", expected " << sizeof(uint32) << std::endl;
		return;
	}
	if (!RuleB(Petitions, PetitionSystemActive)) {
		Message(Chat::Default, "Petitionining ingame is disabled. Please report petitions on the server's Discord using the ticketing system there.");
		return;
	}

	if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {
		uint32 getpetnum = *((uint32*)app->pBuffer);
		Petition* getpet = petition_list.GetPetitionByID(getpetnum);
		if (getpet != 0) {
			getpet->AddCheckout();
			getpet->SetCheckedOut(true);
			getpet->SendPetitionToPlayer(this->CastToClient());
			petition_list.UpdatePetition(getpet);
			petition_list.UpdateGMQueue();
			petition_list.UpdateZoneListQueue();
		}
	}
	return;
}

void Client::Handle_OP_PetitionDelete(const EQApplicationPacket *app)
{
	if (app->size != sizeof(PetitionUpdate_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_PetitionDelete, size=%i, expected %i", app->size, sizeof(PetitionUpdate_Struct));
		return;
	}

	if (!RuleB(Petitions, PetitionSystemActive)) {
		Message(Chat::Default, "Petitionining ingame is disabled. Please report petitions on the server's Discord using the ticketing system there.");
		return;
	}

	auto outapp = new EQApplicationPacket(OP_PetitionRefresh, sizeof(PetitionUpdate_Struct));
	PetitionUpdate_Struct* pet = (PetitionUpdate_Struct*)outapp->pBuffer;
	pet->petnumber = *((int*)app->pBuffer);
	pet->color = 0x00;
	pet->status = 0xFFFFFFFF;
	pet->senttime = 0;
	strcpy(pet->accountid, "");
	strcpy(pet->gmsenttoo, "");
	pet->quetotal = petition_list.GetTotalPetitions();
	strcpy(pet->charname, "");
	FastQueuePacket(&outapp);

	if (petition_list.DeletePetition(pet->petnumber) == -1)
		std::cout << "Something is borked with: " << pet->petnumber << std::endl;
	petition_list.ClearPetitions();
	petition_list.UpdateGMQueue();
	petition_list.ReadDatabase();
	petition_list.UpdateZoneListQueue();
	return;
}

void Client::Handle_OP_PetitionRefresh(const EQApplicationPacket *app)
{
	// This is When Client Asks for Petition Again and Again...
	// break is here because it floods the zones and causes lag if it
	// Were to actually do something:P We update on our own schedule now.
	return;
}

void Client::Handle_OP_PickPocket(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(PickPocket_Struct))
	{
		Log(Logs::General, Logs::Error, "Size mismatch for Pick Pocket packet");
		DumpPacket(app);
	}

	if (!HasSkill(EQ::skills::SkillPickPockets))
	{
		return;
	}

	if (!p_timers.Expired(&database, pTimerBeggingPickPocket, false))
	{
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		database.SetMQDetectionFlag(this->AccountName(), this->GetName(), "OP_PickPocket was sent again too quickly.", zone->GetShortName());
		return;
	}

	p_timers.Start(pTimerBeggingPickPocket, 8);

	PickPocket_Struct* pick_in = (PickPocket_Struct*)app->pBuffer;
	auto outapp = new EQApplicationPacket(OP_PickPocket, sizeof(PickPocket_Struct));
	PickPocket_Struct* pick_out = (PickPocket_Struct*)outapp->pBuffer;

	Mob* victim = entity_list.GetMob(pick_in->to);
	if (!victim || m_inv.GetItem(EQ::invslot::slotCursor) != nullptr)
	{
		pick_out->coin = 0;
		pick_out->from = 0;
		pick_out->to = GetID();
		pick_out->myskill = GetSkill(EQ::skills::SkillPickPockets);
		pick_out->type = 0;
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (victim->IsNPC())
	{
		victim->CastToNPC()->PickPocket(this);
		safe_delete(outapp);
		return;
	}

	pick_out->coin = 0;
	pick_out->from = victim->GetID();
	pick_out->to = GetID();
	pick_out->myskill = GetSkill(EQ::skills::SkillPickPockets);
	pick_out->type = 0;

	if (victim == this){
		Message_StringID(Chat::Skills, STEAL_FROM_SELF);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	else if(victim->IsClient() || (victim->GetOwner() && victim->GetOwner()->IsClient()))
	{
		Message_StringID(Chat::Skills, STEAL_PLAYERS);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	else if(victim->IsCorpse())
	{
		Message_StringID(Chat::Skills, STEAL_CORPSES);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	else
	{
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
}

void Client::Handle_OP_RaidCommand(const EQApplicationPacket *app)
{
	if (app->size < sizeof(RaidGeneral_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_RaidCommand, size=%i, expected at least %i", app->size, sizeof(RaidGeneral_Struct));
		DumpPacket(app);
		return;
	}

	if (IsSoloOnly())
	{
		Message(Chat::Red, "You are solo only and cannot be invited to the raid.");
		return;
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "You are a GM. Do not join raids or groups.");
		database.SetHackerFlag(account_name, GetCleanName(), "GM attempted to join a group or raid.");
		return;
	}

	RaidGeneral_Struct *ri = (RaidGeneral_Struct*)app->pBuffer;
	//Say("RaidCommand(action) %d leader_name(68): %s, player_name(04) %s param(132) %d", ri->action, ri->leader_name, ri->player_name, ri->parameter);

	switch (ri->action)
	{
	case RaidCommandInviteIntoExisting:
	case RaidCommandInvite: {
		Client *i = entity_list.GetClientByName(ri->player_name);
		if (i)
		{
			Raid* raid = GetRaid();
			ChallengeRules::RuleSet raidGroupType = raid ? raid->GetRuleSet() : GetRuleSet();
			
			if (!i->CanGroupWith(raidGroupType))
			{
				Message(Chat::Red, "This player's challenge mode does not match your raid's. They cannot join the raid.");
				return;
			}

			if (i->Admin() > 0)
			{
				Message(Chat::Red, "This player is a GM and cannot join your raid.");
				return;
			}
			
			//This sends an "invite" to the client in question.
			auto outapp = new EQApplicationPacket(OP_RaidInvite, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			strn0cpy(rg->leader_name, ri->leader_name, 64);
			strn0cpy(rg->player_name, ri->player_name, 64);

			rg->parameter = 0;
			rg->action = 3;
			i->QueuePacket(outapp);
			safe_delete(outapp);
		}
		break;
	}
	case RaidCommandDeclineInvite: {
		Client *i = entity_list.GetClientByName(ri->player_name);
		if (i)
			i->QueuePacket(app);
		// this should make the client show a message of reason for decline
		// parameter value = string id of the message.
		break;
	}
	case RaidCommandAcceptInvite: {
		// invite was accepted
		Client *i = entity_list.GetClientByName(ri->leader_name);
		Client *leader = entity_list.GetClientByName(ri->player_name);

		if (!leader || !i || i != this) {
			// send back a failure message back to the invited
			i->Message_StringID(Chat::DefaultText, CANNOT_JOIN_RAID, ri->leader_name);

			// this will reset the raid for the invited, but will not give them a message their raid was disbanded.
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandSendDisband;
			strcpy(rg->leader_name, ri->player_name);
			strcpy(rg->player_name, ri->player_name);
			this->QueuePacket(outapp);
			safe_delete(outapp);
			return;
		}
		Raid *invited_raid = entity_list.GetRaidByClient(i);
		Raid *r = entity_list.GetRaidByClient(leader);
		if (r && invited_raid && r != invited_raid) {
			// fail
			return;
		}

		ChallengeRules::RuleSet groupType = r ? r->GetRuleSet() : leader->GetRuleSet();

		if (!CanGroupWith(groupType))
		{
			// this will reset the raid for the invited, but will not give them a message their raid was disbanded.
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandSendDisband;
			strcpy(rg->leader_name, ri->player_name);
			strcpy(rg->player_name, ri->player_name);
			this->QueuePacket(outapp);
			safe_delete(outapp);
			Message(Chat::Red, "Your challenge mode does not match the raid's. You cannot be invited to the raid.");
			return;
		}

		if (i)
		{
			if (IsRaidGrouped())
			{
				i->Message_StringID(Chat::DefaultText, ALREADY_IN_RAID, GetName()); //group failed, must invite members not in raid...
				return;
			}
			
			if (r)
			{
				// we currently have a raid
				r->VerifyRaid();
				Group *g = GetGroup();
				if (g)
				{
					if (g->GroupCount() + r->RaidCount() > MAX_RAID_MEMBERS)
					{
						i->Message(Chat::Red, "Invite failed, group invite would create a raid larger than the maximum number of members allowed.");
						return;
					}
				}
				else
				{
					if (1 + r->RaidCount() > MAX_RAID_MEMBERS)
					{
						i->Message(Chat::Red, "Invite failed, member invite would create a raid larger than the maximum number of members allowed.");
						return;
					}
				}

				//get a free group for invitee or their group
				uint32 freeGroup = r->GetFreeGroup();
				if (g)
				{
					if (freeGroup == 0xFFFFFFFF) 
					{
						i->Message(Chat::Red, "Invite failed, number of groups would exceed the maximum number of groups allowed.");
						return;
					}

					// Existing raid, add invitee and their group.
					r->AddGroupToRaid(leader, this, g, freeGroup);
				}
				else
				{
					// Existing raid, add solo invitee.
					r->AddMember(this, freeGroup, false, freeGroup != 0xFFFFFFFF,  (freeGroup != 0xFFFFFFFF && r->GetLootType() == 2));
					r->SendRaidMembers(this);
				}

				UpdateLFG();
			}
			else
			{
				// need to create a raid
				Group *lg = leader->GetGroup();
				Group *g = GetGroup();

				if (g)
				{
					// Verify group before performing actions on it.
					g->UpdatePlayer(this);
				}

				if (g && g->GroupCount() < 2)
				{
					i->Message(Chat::Red, "Invite failed, group does not have enough members to be invited.");
					return;
				}

				r = new Raid(leader);
				entity_list.AddRaid(r);
				r->SetRaidDetails();
				r->SendRaidCreate(leader);
				
				//get a free group for leader or their group
				uint32 groupFree = r->GetFreeGroup();
				if (lg) 
				{
					// New raid, add inviter (raid leader) and their group.
					r->AddMember(leader, groupFree, true, lg->GetLeader() && lg->GetLeader() == leader, true);
					r->SendRaidMembers(leader);

					for (int x = 0; x < MAX_GROUP_MEMBERS; x++)
					{
						if (lg->members[x] && lg->members[x] != leader)
						{
							if (lg->IsLeader(lg->members[x])){
								Client *c = nullptr;
								if (lg->members[x]->IsClient())
									c = lg->members[x]->CastToClient();
								else
									continue;

								r->AddMember(c, groupFree, false, true, r->GetLootType() == 2);
								r->SendRaidMembers(c);
							}
							else
							{
								Client *c = nullptr;
								if (lg->members[x]->IsClient())
									c = lg->members[x]->CastToClient();
								else
									continue;

								r->AddMember(c, groupFree);
								r->SendRaidMembers(c);
							}
						}
					}

					lg->DisbandGroup(true);
					r->GroupUpdate(groupFree);

				} 
				else 
				{
					// New raid, add solo inviter (raid leader.)
					r->AddMember(leader, groupFree, true, true, true);
					r->SendRaidMembers(leader);
				}

				//get a free group for invitee or their group
				groupFree = r->GetFreeGroup();
				if (g) 
				{
					r->AddGroupToRaid(leader, this, g, groupFree);
				} 
				else 
				{ 
					// New raid, add solo invitee.
					r->AddMember(this, groupFree, false, true, r->GetLootType() == 2);
					r->SendRaidMembers(this);
				}

				UpdateLFG();
				leader->UpdateLFG();
			}
		}
		break;
	}
	case RaidCommandSendDisband:
	case RaidCommandRaidDisband: {
		Raid *r = entity_list.GetRaidByClient(this);
		if (r) {
			r->DisbandRaidMember(ri->leader_name, this);
		} else {
			// we could have a bugged raid.  This will reset it on the client
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandRaidDisband;
			strcpy(rg->leader_name, this->GetName());
			strcpy(rg->player_name, this->GetName());
			this->QueuePacket(outapp);
			safe_delete(outapp);
		}
		break;
	}
	case RaidCommandSetLootType:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			r->ChangeLootType(ri->parameter);
			
			// this sends the message only for loot type being set.
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandLootTypeResponse;
			rg->parameter = ri->parameter;
			
			r->QueuePacket(outapp);

			// now send out to update loot setting on other clients
			rg->action = RaidCommandSetLootType;
			r->QueuePacket(outapp);
			safe_delete(outapp);
		}
		break;
	}

	case RaidCommandAddLooter:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			if (r->GetLootType() < 3 )
				return;
			// dont add raid leader to selected list, they are in it by default
			if (strlen(ri->leader_name) > 0 && strncasecmp(ri->leader_name, r->leadername, 64) == 0)
				return;

			int looters = 0;
			for (int x = 0; x < MAX_RAID_MEMBERS; x++){
				if (strlen(r->members[x].membername) > 0 && r->members[x].IsLooter)
					looters++;
			}
			if (looters > 9) {
				// looter list is full
				auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
				RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
				rg->action = RaidCommandRaidMessage;
				rg->parameter = 5106; // The raid loot list is full.  You cannot add anymore players to the list.
				r->QueuePacket(outapp);
				safe_delete(outapp);
				return;
			}
			r->AddRaidLooter(ri->leader_name);
			
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandAddLooter;
			strn0cpy(rg->leader_name, ri->leader_name, 64);
			strn0cpy(rg->player_name, ri->player_name, 64);
			r->QueuePacket(outapp);

			// this sends message out that the looter was added
			rg->action = RaidCommandRaidMessage;
			rg->parameter = 5104; // 5104 %1 was added to raid loot list.
			r->QueuePacket(outapp);

			// send out a set loot type, to force an update the options window
			rg->action = RaidCommandSetLootType;
			rg->parameter = 3;
			r->QueuePacket(outapp);
			safe_delete(outapp);
		}
		break;
	}
	case RaidCommandRemoveLooter:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			r->RemoveRaidLooter(ri->leader_name);
			
			auto outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
			RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
			rg->action = RaidCommandRemoveLooter;
			strn0cpy(rg->leader_name, ri->leader_name, 64);
			strn0cpy(rg->player_name, ri->player_name, 64);
			r->QueuePacket(outapp);

			// this sends message out that the looter was added
			rg->action = RaidCommandRaidMessage;
			rg->parameter = 5105; // 5105 %1 was removed from the loot list
			r->QueuePacket(outapp);
			safe_delete(outapp);
		}
		break;
	}

	case RaidCommandChangeRaidLeader:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			if (strcmp(r->leadername, GetName()) == 0){
				r->SetRaidLeader(GetName(), ri->leader_name);
			}
		}
		break;
	}

	default: {
		Message(Chat::Red, "Raid command (%d) NYI", ri->action);
		break;
	}
	}
}

void Client::Handle_OP_RandomReq(const EQApplicationPacket *app)
{
	if (app->size != sizeof(RandomReq_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_RandomReq, size=%i, expected %i", app->size, sizeof(RandomReq_Struct));
		return;
	}
	const RandomReq_Struct* rndq = (const RandomReq_Struct*)app->pBuffer;
	uint32 randLow = rndq->low > rndq->high ? rndq->high : rndq->low;
	uint32 randHigh = rndq->low > rndq->high ? rndq->low : rndq->high;
	uint32 randResult;

	if (randLow == 0 && randHigh == 0)
	{	// defaults
		randLow = 0;
		randHigh = 100;
	}
	randResult = zone->random.Int(randLow, randHigh);

	auto outapp = new EQApplicationPacket(OP_RandomReply, sizeof(RandomReply_Struct));
	RandomReply_Struct* rr = (RandomReply_Struct*)outapp->pBuffer;
	rr->low = randLow;
	rr->high = randHigh;
	rr->result = randResult;
	strcpy(rr->name, GetName());
	entity_list.QueueCloseClients(this, outapp, false, 400);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_ReadBook(const EQApplicationPacket *app)
{
	if (app->size > sizeof(BookRequest_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_ReadBook, size=%i, expected %i", app->size, sizeof(BookRequest_Struct));
		return;
	}
	BookRequest_Struct* book = (BookRequest_Struct*)app->pBuffer;
	ReadBook(book);
	return;
}

void Client::Handle_OP_Report(const EQApplicationPacket *app)
{
	if (!CanUseReport)
	{
		Message_StringID(Chat::System, REPORT_ONCE);
		return;
	}

	uint32 size = app->size;
	uint32 current_point = 0;
	std::string reported, reporter;
	std::string current_string;
	int mode = 0;

	while (current_point < size)
	{
		if (mode < 2)
		{
			if (app->pBuffer[current_point] == '|')
			{
				mode++;
			}
			else
			{
				if (mode == 0)
				{
					reported += app->pBuffer[current_point];
				}
				else
				{
					reporter += app->pBuffer[current_point];
				}
			}
			current_point++;
		}
		else
		{
			if (app->pBuffer[current_point] == 0x0a)
			{
				current_string += '\n';
			}
			else if (app->pBuffer[current_point] == 0x00)
			{
				CanUseReport = false;
				database.AddReport(reporter, reported, current_string);
				return;
			}
			else
			{
				current_string += app->pBuffer[current_point];
			}
			current_point++;
		}
	}

	CanUseReport = false;
	database.AddReport(reporter, reported, current_string);
}

void Client::Handle_OP_RequestDuel(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Duel_Struct))
		return;

	EQApplicationPacket* outapp = app->Copy();
	Duel_Struct* ds = (Duel_Struct*)outapp->pBuffer;
	uint32 duel = ds->duel_initiator;
	ds->duel_initiator = ds->duel_target;
	ds->duel_target = duel;
	Entity* entity = entity_list.GetID(ds->duel_target);
	
	if (!entity) {
		safe_delete(outapp);
		return;
	}
	
	if (GetID() != ds->duel_target && entity->IsClient() && (entity->CastToClient()->IsDueling() && entity->CastToClient()->GetDuelTarget() != 0)) {
		Message_StringID(Chat::White, DUEL_CONSIDERING, entity->GetName());
		safe_delete(outapp);
		return;
	}
	if (IsDueling()) {
		Message_StringID(Chat::White, DUEL_INPROGRESS);
		safe_delete(outapp);
		return;
	}

	if (GetID() != ds->duel_target && entity->IsClient() && GetDuelTarget() == 0 && !IsDueling() && !entity->CastToClient()->IsDueling() && entity->CastToClient()->GetDuelTarget() == 0) {
		SetDuelTarget(ds->duel_target);
		entity->CastToClient()->SetDuelTarget(GetID());
		ds->duel_target = ds->duel_initiator;
		entity->CastToClient()->FastQueuePacket(&outapp);
		entity->CastToClient()->SetDueling(false);
		SetDueling(false);
		safe_delete(outapp);
		return;
	}
	
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_RezzAnswer(const EQApplicationPacket *app)
{
	VERIFY_PACKET_LENGTH(OP_RezzAnswer, app, Resurrect_Struct);

	const Resurrect_Struct* ra = (const Resurrect_Struct*)app->pBuffer;

	Log(Logs::Detail, Logs::Spells, "Received OP_RezzAnswer from client. Pendingrezzexp is %i, action is %s",
		PendingRezzXP, ra->action ? "ACCEPT" : "DECLINE");


	OPRezzAnswer(ra->action, ra->spellid, ra->zone_id, 0, ra->x, ra->y, ra->z);

	if (ra->action == 1)
	{
		Mob* mypet = GetPet();
		if (mypet) 
		{
			if (mypet->IsCharmedPet())
				FadePetCharmBuff();
			else
				DepopPet();
		}

		entity_list.ClearAggro(this);

		EQApplicationPacket* outapp = app->Copy();
		// Send the OP_RezzComplete to the world server. This finds it's way to the zone that
		// the rezzed corpse is in to mark the corpse as rezzed.
		outapp->SetOpcode(OP_RezzComplete);
		worldserver.RezzPlayer(outapp, 0, 0, 0, OP_RezzComplete);
		safe_delete(outapp);
	}
	return;
}

void Client::Handle_OP_Sacrifice(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Sacrifice_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_Sacrifice expected %i got %i", sizeof(Sacrifice_Struct), app->size);
		DumpPacket(app);
		return;
	}
	Sacrifice_Struct *ss = (Sacrifice_Struct*)app->pBuffer;

	if (!PendingSacrifice) {
		Log(Logs::General, Logs::Error, "Unexpected OP_Sacrifice reply");
		DumpPacket(app);
		return;
	}

	if (ss->Confirm) {
		Mob *Caster = entity_list.GetMob(sacrifice_caster_id);
		if (Caster) Sacrifice(Caster);
	}
	PendingSacrifice = false;
	sacrifice_caster_id = 0;
}

void Client::Handle_OP_SafeFallSuccess(const EQApplicationPacket *app)	// bit of a misnomer, sent whenever safe fall is used (success of fail)
{
	if (HasSkill(EQ::skills::SkillSafeFall)) //this should only get called if the client has safe fall, but just in case...
		CheckIncreaseSkill(EQ::skills::SkillSafeFall, nullptr, zone->skill_difficulty[EQ::skills::SkillSafeFall].difficulty); //check for skill up
}

void Client::Handle_OP_SafePoint(const EQApplicationPacket *app)
{
}

void Client::Handle_OP_Save(const EQApplicationPacket *app)
{
	// The payload is 192 bytes - Not sure what is contained in payload
	Save();
	return;
}

void Client::Handle_OP_SaveOnZoneReq(const EQApplicationPacket *app)
{
	Handle_OP_Save(app);
}

void Client::Handle_OP_SenseHeading(const EQApplicationPacket *app)
{
	CheckIncreaseSkill(EQ::skills::SkillSenseHeading, nullptr, zone->skill_difficulty[EQ::skills::SkillSenseHeading].difficulty);
	return;
}

void Client::Handle_OP_SenseTraps(const EQApplicationPacket *app) 
{
	if (!HasSkill(EQ::skills::SkillSenseTraps))
		return;

	if (!p_timers.Expired(&database, pTimerSenseTraps, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}
	int reuse = SenseTrapsReuseTime;
	switch (GetAA(aaAdvTrapNegotiation)) {
	case 1:
		reuse -= 1;
		break;
	case 2:
		reuse -= 3;
		break;
	case 3:
		reuse -= 5;
		break;
	}
	p_timers.Start(pTimerSenseTraps, reuse - 1);

	float trap_curdist = 0;
	float door_curdist = 0;
	Trap* trap = entity_list.FindNearbyTrap(this, 800, trap_curdist);	
	Doors* door_trap = entity_list.FindNearbyDoorTrap(this, 800, door_curdist);

	uint8 skill = 1;
	float angle = 0;
	if (door_trap && (door_curdist < trap_curdist || trap_curdist == INVALID_INDEX))
	{
		glm::vec4 door_pos(door_trap->GetPosition());
		skill = 1;
		angle = CalculateHeadingToTarget(door_pos.x, door_pos.y);
		Log(Logs::General, Logs::Traps, "Door trap detected!");
	}
	else if(trap && (trap_curdist < door_curdist || door_curdist == INVALID_INDEX))
	{
		skill = trap->skill > 0 ? trap->skill : 1;
		angle = CalculateHeadingToTarget(trap->m_Position.x, trap->m_Position.y);
		Log(Logs::General, Logs::Traps, "Regular trap detected!");
	}

	uint8 success = SKILLUP_FAILURE;
	if ((trap || door_trap) && skill > 0) 
	{
		int uskill = GetSkill(EQ::skills::SkillSenseTraps);
		if ((zone->random.Int(0, 99) + uskill) >= (zone->random.Int(0, 99) + skill*0.75))
		{
			Message_StringID(Chat::Skills, SENSE_TRAP);

			if(trap)
				trap->detected = true;

			if (angle < 0)
				angle = (256 + angle);

			angle *= 2;
			MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), GetX(), GetY(), GetZ(), angle);
			success = SKILLUP_SUCCESS;
		}
	}
	
	if(success == SKILLUP_FAILURE)
		Message_StringID(Chat::Skills, DO_NOT_SENSE_TRAP);

	CheckIncreaseSkill(EQ::skills::SkillSenseTraps, nullptr, zone->skill_difficulty[EQ::skills::SkillSenseTraps].difficulty, success);
}

void Client::Handle_OP_SetGuildMOTD(const EQApplicationPacket *app)
{
	Log(Logs::Detail, Logs::Guilds, "Received OP_SetGuildMOTD");

	if (app->size != sizeof(GuildMOTD_Struct)) {
		// client calls for a motd on login even if they arent in a guild
		Log(Logs::Detail, Logs::Guilds, "Error: app size of %i != size of GuildMOTD_Struct of %zu\n", app->size, sizeof(GuildMOTD_Struct));
		return;
	}
	if (!IsInAGuild()) {
		Message(Chat::Red, "You are not in a guild!");
		return;
	}

	GuildMOTD_Struct* gmotd = (GuildMOTD_Struct*)app->pBuffer;
	if (gmotd->motd[0] == 0)
	{
		// if nothing is to be set, then send the guild motd
		SendGuildMOTD();
		return;
	}

	if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_MOTD)) {
		return;
	}

	Log(Logs::Detail, Logs::Guilds, "Setting MOTD for %s (%d) to: %s - %s",
		guild_mgr.GetGuildName(GuildID()), GuildID(), GetName(), gmotd->motd);

	if (!guild_mgr.SetGuildMOTD(GuildID(), gmotd->motd, GetName())) {
		Message(Chat::White, "Motd update failed.");
	}

	return;
}

void Client::Handle_OP_SetGuildMOTDCon(const EQApplicationPacket *app)
{
	//This prompts sending guild motd to client.
	SendGuildMOTD();
	return;
}

void Client::Handle_OP_SetRunMode(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SetRunMode_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized "
		"OP_SetRunMode: got %d, expected %d", app->size,
		sizeof(SetRunMode_Struct));
		DumpPacket(app);
		return;
	}
	SetRunMode_Struct* rms = (SetRunMode_Struct*)app->pBuffer;
	if (rms->mode)
		runmode = true;
	else
		runmode = false;
}

void Client::Handle_OP_SetServerFilter(const EQApplicationPacket *app)
{

	if (app->size != sizeof(SetServerFilter_Struct)) {
		Log(Logs::General, Logs::Error, "Received invalid sized "
			"OP_SetServerFilter: got %d, expected %d", app->size,
			sizeof(SetServerFilter_Struct));
		DumpPacket(app);
		return;
	}
	SetServerFilter_Struct* filter = (SetServerFilter_Struct*)app->pBuffer;
	ServerFilter(filter);
	return;
}

void Client::Handle_OP_SetTitle(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SetTitle_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_SetTitle expected %i got %i", sizeof(SetTitle_Struct), app->size);
		DumpPacket(app);
		return;
	}

	SetTitle_Struct *sts = (SetTitle_Struct *)app->pBuffer;

	std::string Title;

	if (!sts->is_suffix)
	{
		Title = title_manager.GetPrefix(sts->title_id);
		SetAATitle(Title.c_str());
	}
	else
	{
		Title = title_manager.GetSuffix(sts->title_id);
		SetTitleSuffix(Title.c_str());
	}
}

void Client::Handle_OP_Shielding(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Shielding_Struct)) {
		Log(Logs::General, Logs::Error, "OP size error: OP_Shielding expected:%i got:%i", sizeof(Shielding_Struct), app->size);
		return;
	}

	if (GetClass() != Class::Warrior || GetLevel() < 30 || IsMezzed() || IsFeared() || IsDead())
	{
		return;
	}
	
	if (!p_timers.Expired(&database, pTimerShield, false))
	{
		uint32 remainingTime = p_timers.GetRemainingTime(pTimerShield);
		//Message_StringID(Chat::White, AA_REUSE_MSG2, itoa(14212), itoa(remainingTime / 60), itoa(remainingTime % 60));		// this method can't handle this input for whatever reason
		Message(Chat::White, "You can use the ability /shield again in %u minute(s) %u seconds.", remainingTime / 60, remainingTime % 60);
		return;
	}

	Shielding_Struct* shield = (Shielding_Struct*)app->pBuffer;
	Mob* target = entity_list.GetMob(shield->target_id);
	
	if (!target || !target->IsClient() || target->IsCorpse() || target->CastToClient()->IsDead())
	{
		Message_StringID(Chat::White, SHIELD_TARGET_LIVING);
		return;
	}

	if (DistanceSquared(GetPosition(), target->GetPosition()) > (15.0f*15.0f))
	{
		Message_StringID(Chat::White, TARGET_TOO_FAR);
		return;
	}

	if (GetActiveDisc() || target->CastToClient()->GetActiveDisc())
	{
		Message_StringID(Chat::White, SHIELD_NO_DISC);
		return;
	}

	if (GetShieldTarget() || target->GetShieldTarget())
	{
		Message_StringID(Chat::White, ALREADY_SHIELDING_ANOTHER);
		return;
	}

	if (GetShielder() || target->GetShielder())
	{
		Message_StringID(Chat::White, ALREADY_SHIELDED);
		return;
	}
	
	StartShield(target);
}

void Client::Handle_OP_ShopEnd(const EQApplicationPacket *app)
{
	SendMerchantEnd();
}

void Client::Handle_OP_ShopPlayerBuy(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Merchant_Sell_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size on OP_ShopPlayerBuy: Expected %i, Got %i",
			sizeof(Merchant_Sell_Struct), app->size);
		return;
	}

	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	RDTSC_Timer t1;
	t1.start();
	Merchant_Sell_Struct* mp = (Merchant_Sell_Struct*)app->pBuffer;
#if EQDEBUG >= 11
	Log(Logs::General, Logs:Trading, "%s, purchase item..", GetName());
	DumpPacket(app);
#endif

	int merchantid;
	bool tmpmer_used = false;
	bool sf_tmpmer_used = false;
	bool reimbursement_used = false;

	Mob* tmp = entity_list.GetMob(mp->npcid);
	if (!tmp)
	{
		// This prevents the client from bugging if we have to return but also sends a bogus message.
		auto returnapp = new EQApplicationPacket(OP_ShopPlayerBuy, sizeof(Merchant_Sell_Struct));
		Merchant_Sell_Struct* mss = (Merchant_Sell_Struct*)returnapp->pBuffer;
		mss->itemslot = 0;
		mss->npcid = mp->npcid;
		mss->playerid = GetID();
		mss->price = 0;
		mss->IsSold = 1;
		mss->quantity = 0;
		QueuePacket(returnapp);
		safe_delete(returnapp);
		SendMerchantEnd();
		return;
	}

	// This prevents the client from bugging if we have to return but also sends a bogus message.
	auto returnapp = new EQApplicationPacket(OP_ShopPlayerBuy, sizeof(Merchant_Sell_Struct));
	Merchant_Sell_Struct* mss = (Merchant_Sell_Struct*)returnapp->pBuffer;
	mss->itemslot=0;
	mss->npcid=tmp->GetID();
	mss->playerid=GetID();
	mss->price=0;
	mss->IsSold=1;
	mss->quantity=0;

	if (tmp == nullptr || !tmp->IsNPC() || tmp->GetClass() != Class::Merchant)
	{
		QueuePacket(returnapp);
		safe_delete(returnapp);
		SendMerchantEnd();
		return;
	}

	++tmp->CastToNPC()->shop_count;

	if (mp->quantity < 1)
		return;

	//you have to be somewhat close to them to be properly using them
	if (DistanceSquared(m_Position, tmp->GetPosition()) > USE_NPC_RANGE2)
	{
		QueuePacket(returnapp);
		safe_delete(returnapp);
		SendMerchantEnd();
		return;
	}

	merchantid = tmp->CastToNPC()->MerchantType;
	uint32 item_id = 0;
	uint32 prevcharges = 0;
	uint8 quantity_left = 0;
	const EQ::ItemData* item = nullptr;

	if (merchantid != 1)
	{
		std::list<MerchantList> merlist = zone->merchanttable[merchantid];
		std::list<MerchantList>::const_iterator itr;
		for (itr = merlist.begin(); itr != merlist.end(); ++itr) {
			MerchantList ml = *itr;
			if (GetLevel() < ml.level_required) {
				continue;
			}

			const EQ::ItemData* item = database.GetItem(ml.item);
			if (!item)
				continue;

			int32 fac = tmp ? tmp->GetPrimaryFaction() : 0;
			int32 facmod = GetModCharacterFactionLevel(fac);
			if (IsInvisible(tmp))
				facmod = 0;
			if (fac != 0 && facmod < ml.faction_required && zone->CanDoCombat()) {
				continue;
			}

			if (ml.quantity > 0 && ml.qty_left <= 0)
			{
				continue;
			}

			if (mp->itemslot == ml.slot) {
				item_id = ml.item;
				if (ml.quantity > 0 && ml.qty_left > 0)
				{
					quantity_left = ml.qty_left;
				}
				break;
			}
		}

		if (!IsSelfFoundAny() || RuleB(SelfFound, TempMerchantSupport))
		{
			if (item_id == 0)
			{
				//check to see if its on the temporary table
				std::list<TempMerchantList> tmp_merlist = zone->tmpmerchanttable[tmp->GetNPCTypeID()];
				std::list<TempMerchantList>::const_iterator tmp_itr;
				TempMerchantList ml;
				for (tmp_itr = tmp_merlist.begin(); tmp_itr != tmp_merlist.end(); ++tmp_itr) {
					ml = *tmp_itr;
					if (mp->itemslot == ml.slot) {
						item_id = ml.item;
						tmpmer_used = true;
						prevcharges = ml.charges;
						if (IsSelfFoundAny())
						{
							sf_tmpmer_used = true;
							uint32 purchase_limit = zone->GetSelfFoundPurchaseLimit(tmp->GetNPCTypeID(), item_id, CharacterID());
							if (purchase_limit <= 0)
							{
								Log(Logs::Detail, Logs::Trading, "[S/SF] TEMP: Blocked %s (%i) attempt to purchase item=%i slot=%i that had charges/qty %i/%i", name, CharacterID(), ml.item, ml.slot, ml.charges, ml.quantity);
								Message(Chat::Red, "You may only repurchase items that belonged to you from merchants. Try selling a copy of this item first.");
								QueuePacket(returnapp);
								safe_delete(returnapp);
								return;
							}
						}
						break;
					}
				}
			}
		}
	}
	else if (merchantid == 1 && item_id == 0)
	{
		for (auto item : item_reimbursement_list)
		{
			if (item.slot == mp->itemslot)
			{
				if (mp->itemslot == item.slot) {
					item_id = item.item;
					tmpmer_used = true;
					reimbursement_used = true;
					prevcharges = item.charges;
					break;
				}
			}
		}
	}

	item = database.GetItem(item_id);
	if (!item){
		//error finding item (most likely the item failed the level or faction check.)
		Message_StringID(Chat::White, ALREADY_SOLD);
		entity_list.SendMerchantInventory(tmp, mp->itemslot, true);
		QueuePacket(returnapp);
		safe_delete(returnapp);
		SendMerchantEnd();
		return;
	}
	if (CheckLoreConflict(item))
	{
		Message_StringID(Chat::White, DUPE_LORE_MERCHANT, tmp->GetCleanName());
		QueuePacket(returnapp);
		safe_delete(returnapp);
		return;
	}

	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		Message(Chat::Red, "You cannot use merchants in guild instances.");
		QueuePacket(returnapp);
		safe_delete(returnapp);
		return;
	}

	if (Admin() > 0 && tmpmer_used)
	{
		Message(Chat::Red, "That item isn't normally sold here. You are a GM. You'd be griefing players. The gods weep today.");
		QueuePacket(returnapp);
		safe_delete(returnapp);
		return;
	}

	// This makes sure the vendor deletes charged items from their lists properly.
	uint8 tmp_qty = 0;
	// Temp / reimbursement merchantlist
	if (reimbursement_used)
		tmp_qty = prevcharges > 240 ? 240 : prevcharges;
	else if (sf_tmpmer_used)
		tmp_qty = prevcharges > 0 ? 1 : 0; // We only support non-stackable S/SF items, so buying 1 at-a-time at most
	else if(tmpmer_used)
		tmp_qty = prevcharges > 240 ? 240 : prevcharges;
	// Regular merchantlist with limited supplies
	else if(quantity_left > 0)
		tmp_qty = quantity_left;

	if ((reimbursement_used) && (mp->quantity > tmp_qty || database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges))
	{
		mp->quantity = tmp_qty;
	}
	else if ((tmpmer_used || quantity_left > 0) && (mp->quantity > tmp_qty || database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges))
	{
		if (database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges && tmpmer_used && !reimbursement_used) 
		{
			int32 temp_val = zone->GetTempMerchantQtyNoSlot(tmp->GetNPCTypeID(), item_id);
			if (temp_val > 240 && temp_val != -1)
				mp->quantity = 240;
			else
				mp->quantity = temp_val;
		} 
		else 
		{
			mp->quantity = tmp_qty;
		}
	}
	else if(database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges && !tmpmer_used)
		mp->quantity = item->MaxCharges;

	uint8 quantity = mp->quantity;
	//This makes sure we don't overflow the quantity in our packet.
	if(database.ItemQuantityType(item_id) == EQ::item::Quantity_Stacked && quantity > 20)
		quantity = 20; 

	if (database.ItemQuantityType(item_id) == EQ::item::Quantity_Stacked && quantity <= 0)
	{
		Log(Logs::General, Logs::Trading, "Handle_OP_ShopPlayerBuy(): Quantity on stacked item is 0. Returning.");
		Message(Chat::Red, "The client sent the server an invalid item quantity number. Cancelling the transaction to prevent a desync...");
		QueuePacket(returnapp);
		safe_delete(returnapp);
		return;
	}

	auto outapp = new EQApplicationPacket(OP_ShopPlayerBuy, sizeof(Merchant_Sell_Struct));
	Merchant_Sell_Struct* mpo = (Merchant_Sell_Struct*)outapp->pBuffer;
	mpo->quantity = (item->MaxCharges > 1 ? 1 : quantity);
	mpo->playerid = mp->playerid;
	mpo->npcid = mp->npcid;
	mpo->itemslot = mp->itemslot;

	int16 freeslotid = INVALID_INDEX;

	EQ::ItemInstance* inst = database.CreateItem(item, quantity);
	if (inst && IsSelfFoundAny())
		inst->SetSelfFoundCharacter(CharacterID(), GetName()); // Purchasing the item, they will own it

	int SinglePrice = 0;
	float price_mod = CalcPriceMod(tmp);
	SinglePrice = item->Price * item->SellRate * price_mod;

	if (item->MaxCharges > 1)
		mpo->price = SinglePrice;
	else
		mpo->price = SinglePrice * mp->quantity + 0.5f;
	if (mpo->price < 0)
	{
		Message_StringID(Chat::White, ALREADY_SOLD);
		safe_delete(outapp);
		safe_delete(inst);
		QueuePacket(returnapp);
		safe_delete(returnapp);
		SendMerchantEnd();
		return;
	}

	// this area needs some work..two inventory insertion check failure points
	// below do not return player's money..is this the intended behavior?

	if (!TakeMoneyFromPP(mpo->price))
	{
		auto hacker_str = fmt::format("Vendor Cheat: attempted to buy {} of {} : {} that cost {} cp but only has {} pp {} gp {} sp {} cp\n",
			mpo->quantity, item->ID, item->Name,
			mpo->price, m_pp.platinum, m_pp.gold, m_pp.silver, m_pp.copper);
		database.SetMQDetectionFlag(AccountName(), GetName(), hacker_str, zone->GetShortName());
		safe_delete(outapp);
		safe_delete(inst);
		QueuePacket(returnapp);
		safe_delete(returnapp);
		SendMerchantEnd();
		return;
	}

	bool stacked = TryStacking(inst);
	bool bag = false;
	bool cursor = true;
	if (inst->IsType(EQ::item::ItemClassBag))
	{
		bag = true;
		cursor = false;
	}

	//If stacked returns true, the items have already been placed in the inventory. No need to check for available slot
	//or if the player inventory is full.
	bool dropped = false;
	if (!stacked)
	{
		freeslotid = m_inv.FindFreeSlot(bag, cursor, item->Size);

		//make sure we are not completely full...
		if (freeslotid == EQ::invslot::slotCursor || freeslotid == INVALID_INDEX)
		{
			if (m_inv.GetItem(EQ::invslot::slotCursor) != nullptr || freeslotid == INVALID_INDEX)
			{
				CreateGroundObject(inst, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime), true);
				dropped = true;
			}
		}
	}

	if (!dropped)
	{
		if (!stacked && inst) 
		{
			if (PutItemInInventory(freeslotid, *inst))
			{
				if (freeslotid == EQ::invslot::slotCursor)
					SendItemPacket(freeslotid, inst, ItemPacketSummonItem);
				else
					SendItemPacket(freeslotid, inst, ItemPacketTrade);
			}
		}
		else if (!stacked) 
		{
			Log(Logs::General, Logs::Error, "OP_ShopPlayerBuy: item->ItemClass Unknown! Type: %i", item->ItemClass);
		}
	}

	QueuePacket(outapp);

	if (inst && (tmpmer_used || quantity_left > 0))
	{
		int32 new_charges = 0;

		if (reimbursement_used)
		{
			new_charges = prevcharges - mp->quantity;
			zone->SaveReimbursementItem(item_reimbursement_list, character_id, item_id, new_charges);
		}
		else if (sf_tmpmer_used)
		{
			new_charges = prevcharges - mp->quantity;
			zone->SaveTempItem(merchantid, tmp->GetNPCTypeID(), item_id, new_charges, false, CharacterID());
		}
		else if(tmpmer_used)
		{
			new_charges = prevcharges - mp->quantity;
			zone->SaveTempItem(merchantid, tmp->GetNPCTypeID(), item_id, new_charges);
		}
		else if(quantity_left > 0)
		{
			new_charges = quantity_left - mp->quantity;
			zone->SaveMerchantItem(merchantid,item_id, new_charges, mp->itemslot);
		}

		int32 new_quantity = zone->GetTempMerchantQtyNoSlot(tmp->GetNPCTypeID(), item_id);

		if (reimbursement_used)
			new_quantity = 0;

		if ((database.ItemQuantityType(item_id) == EQ::item::Quantity_Charges && new_quantity < 0) ||
			(database.ItemQuantityType(item_id) != EQ::item::Quantity_Charges && new_charges <= 0))
		{
			entity_list.SendMerchantInventory(tmp, mp->itemslot, true);
		}
		else 
		{
			if (RuleB(SelfFound, TempMerchantSupport) && RuleB(SelfFound, TempMerchantFiltering))
			{
				// The item isn't fully out of stock, but it could be for SF characters
				// Hide it from those who cannot buy any quantity
				if (tmpmer_used && !reimbursement_used && !inst->IsStackable())
				{
					entity_list.SendDeletedSelfFoundMerchantInventory(tmp, mp->itemslot, item_id);
				}
			}
			inst->SetCharges(new_charges);
			inst->SetPrice(SinglePrice);
			inst->SetMerchantSlot(mp->itemslot);
			inst->SetMerchantCount(new_charges);
			entity_list.SendMerchantInventory(tmp);
		}
	}
	safe_delete(inst);
	safe_delete(outapp);
	safe_delete(returnapp);

	// start QS code
	// stacking purchases not supported at this time - entire process will need some work to catch them properly
	if (RuleB(QueryServ, PlayerLogMerchantTransactions))
	{
		QServ->QSMerchantTransactions(character_id, zone->GetZoneID(), freeslotid == INVALID_INDEX ? 0 : freeslotid,
			item->ID, mpo->quantity, tmp->CastToNPC()->MerchantType, 0, 0, 0, 0, 1,
			mpo->price / 1000, (mpo->price / 100) % 10, (mpo->price / 10) % 10, mpo->price % 10, 0);
	}
	// end QS code

	if (RuleB(EventLog, RecordBuyFromMerchant))
		LogMerchant(this, tmp, mpo->quantity, mpo->price, item, true);

	if ((RuleB(Character, EnableDiscoveredItems)))
	{
		if (!GetGM() && !IsDiscovered(item_id))
			DiscoverItem(item_id);
	}

	t1.stop();
	return;
}

void Client::Handle_OP_ShopPlayerSell(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Merchant_Purchase_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size on OP_ShopPlayerSell: Expected %i, Got %i",
			sizeof(Merchant_Purchase_Struct), app->size);
		return;
	}


	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}


	RDTSC_Timer t1(true);
	Merchant_Purchase_Struct* mp = (Merchant_Purchase_Struct*)app->pBuffer;

	Mob* vendor = entity_list.GetMob(mp->npcid);

	if (vendor == 0 || !vendor->IsNPC() || vendor->GetClass() != Class::Merchant)
		return;

	++vendor->CastToNPC()->shop_count;

	//you have to be somewhat close to them to be properly using them
	if (DistanceSquared(m_Position, vendor->GetPosition()) > USE_NPC_RANGE2)
		return;

	uint32 price = 0;
	uint32 itemid = GetItemIDAt(mp->itemslot);
	if (itemid == 0)
		return;

	if (vendor->CastToNPC()->MerchantType == 1)
	{
		Message(Chat::Red, "This merchant can only be bought from.");
		auto outapp = new EQApplicationPacket(OP_ShopPlayerSell, sizeof(OldMerchant_Purchase_Struct));
		OldMerchant_Purchase_Struct* mco = (OldMerchant_Purchase_Struct*)outapp->pBuffer;

		mco->itemslot = 0;
		mco->npcid = vendor->GetID();
		mco->quantity = 0;
		mco->price = 0;
		mco->playerid = this->GetID();
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "Just use commands. You're literally a GM, silly goose.");
		auto outapp = new EQApplicationPacket(OP_ShopPlayerSell, sizeof(OldMerchant_Purchase_Struct));
		OldMerchant_Purchase_Struct* mco = (OldMerchant_Purchase_Struct*)outapp->pBuffer;

		mco->itemslot = 0;
		mco->npcid = vendor->GetID();
		mco->quantity = 0;
		mco->price = 0;
		mco->playerid = this->GetID();
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		Message(Chat::Red, "You cannot use merchants within guild instances.");
		auto outapp = new EQApplicationPacket(OP_ShopPlayerSell, sizeof(OldMerchant_Purchase_Struct));
		OldMerchant_Purchase_Struct* mco = (OldMerchant_Purchase_Struct*)outapp->pBuffer;

		mco->itemslot = 0;
		mco->npcid = vendor->GetID();
		mco->quantity = 0;
		mco->price = 0;
		mco->playerid = this->GetID();
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	const EQ::ItemData* item = database.GetItem(itemid);
	EQ::ItemInstance* inst = GetInv().GetItem(mp->itemslot);
	if (!item || !inst){
		Message(Chat::Red, "You seemed to have misplaced that item..");
		return;
	}
	if (mp->quantity > 1)
	{
		if ((inst->GetCharges() < 0) || (mp->quantity > (uint32)inst->GetCharges()))
			return;
	}

	if (!item->NoDrop) {
		return;
	}

	// the stone UI sends quantity 1 for normal non stackable items which messes up the price calc below
	if (mp->quantity == 0 && database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Normal)
	{
		mp->quantity = 1;
	}

	int cost_quantity = mp->quantity;
	if (database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Charges)
		cost_quantity = 1; //Always sell items to merchants for base cost

	float price_mod = CalcPriceMod(vendor); 
	price = static_cast<uint32>(item->Price / price_mod + 0.5f) * cost_quantity;
	AddMoneyToPP(price, false);

	if (inst->IsStackable())
	{
		unsigned int i_quan = inst->GetCharges();
		if (mp->quantity > i_quan)
			mp->quantity = i_quan;
	}
	else if(database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Charges)
	{
		int32 final_charges = zone->GetTempMerchantQtyNoSlot(vendor->GetNPCTypeID(), item->ID);
		mp->quantity = (final_charges < 0 ? inst->GetCharges() : final_charges);
	}
	else
	{
		mp->quantity = 1;
	}

	if (RuleB(EventLog, RecordSellToMerchant))
		LogMerchant(this, vendor, mp->quantity, price, item, false);

	int charges = mp->quantity;
	int freeslot = 0;
	uint32 self_found_character_id = inst->GetSelfFoundCharacterID();
	if (charges >= 0 && (freeslot = zone->SaveTempItem(vendor->CastToNPC()->MerchantType, vendor->GetNPCTypeID(), itemid, charges, true, self_found_character_id)) > 0)
	{
		EQ::ItemInstance* inst2 = inst->Clone();
		float merchant_mod = CalcPriceMod(vendor);
		inst2->SetPrice(item->Price * item->SellRate * merchant_mod + 0.5f);
		inst2->SetMerchantSlot(freeslot);

		uint32 MerchantQuantity = zone->GetTempMerchantQuantity(vendor->GetNPCTypeID(), freeslot);

		if (inst2->IsStackable()) {
			inst2->SetCharges(MerchantQuantity);
		}
		inst2->SetMerchantCount(MerchantQuantity);

		entity_list.SendMerchantInventory(vendor);

		safe_delete(inst2);
	}

	// start QS code
	if (RuleB(QueryServ, PlayerLogMerchantTransactions))
	{
		QServ->QSMerchantTransactions(character_id, zone->GetZoneID(), mp->itemslot,
			itemid, charges, vendor->CastToNPC()->MerchantType, price / 1000, (price / 100) % 10,
			(price / 10) % 10, price % 10, 0, 0, 0, 0, 0, 1);
	}
	// end QS code

	// Now remove the item from the player, this happens regardless of outcome
	if (!inst->IsStackable())
		this->DeleteItemInInventory(mp->itemslot, 0, false);
	else
		this->DeleteItemInInventory(mp->itemslot, mp->quantity, false);

	//This forces the price to show up correctly for charged items.
	if (database.ItemQuantityType(inst->GetID()) == EQ::item::Quantity_Charges)
		mp->quantity = 1;

	int8 matslot = EQ::InventoryProfile::CalcMaterialFromSlot(mp->itemslot);
	if (matslot != INVALID_INDEX)
	{
		if (matslot == EQ::textures::armorWrist)
		{
			const EQ::ItemInstance* wrist1 = m_inv.GetItem(EQ::invslot::slotWrist1);
			if (mp->itemslot == EQ::invslot::slotWrist1 ||
				(mp->itemslot == EQ::invslot::slotWrist2 && (!wrist1 || (wrist1 && item && wrist1->GetItem()->Material == item->Material))))
			{
				WearChange(EQ::textures::armorWrist, 0, 0);
			}

		}
		else
		{
			WearChange(matslot, 0, 0);
		}
	}

	auto outapp = new EQApplicationPacket(OP_ShopPlayerSell, sizeof(OldMerchant_Purchase_Struct));
	OldMerchant_Purchase_Struct* mco = (OldMerchant_Purchase_Struct*)outapp->pBuffer;

	mco->itemslot = mp->itemslot;
	mco->npcid = vendor->GetID();
	mco->quantity = mp->quantity;
	mco->price = price;
	mco->playerid = this->GetID();
	QueuePacket(outapp);
	safe_delete(outapp);
	Save(1);

	return;
}

void Client::Handle_OP_ShopRequest(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Merchant_Click_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_ShopRequest, size=%i, expected %i", app->size, sizeof(Merchant_Click_Struct));
		return;
	}

	if (apperance_timer.Enabled())
	{
		if (sneaking)
		{
			sneaking = false;
			SendAppearancePacket(AppearanceType::Sneak, 0);
			Log(Logs::General, Logs::Skills, "Fading sneak due to also being hidden while accessing a merchant.");
		}
		apperance_timer.Disable();
	}

	Merchant_Click_Struct* mc = (Merchant_Click_Struct*)app->pBuffer;

	// Send back opcode OP_ShopRequest - tells client to open merchant window.

	int merchantid = 0;
	Mob* tmp = entity_list.GetMob(mc->npcid);

	if (tmp == 0 || !tmp->IsNPC() || tmp->GetClass() != Class::Merchant)
		return;

	//you have to be somewhat close to them to be properly using them
	if (DistanceSquared(m_Position, tmp->GetPosition()) > USE_NPC_RANGE2)
		return;

	merchantid = tmp->CastToNPC()->MerchantType;

	int action = 1;
	if (merchantid == 0 || IsFeigned() || (IsInvisible(tmp) && !sneaking)) 
	{
		action = 0;
	}

	if (tmp->IsEngaged() || tmp->IsCharmedPet()){
		this->Message_StringID(Chat::White, MERCHANT_BUSY);
		action = 0;
	}

	int primaryfaction = tmp->CastToNPC()->GetPrimaryFaction();
	int factionlvl = GetFactionLevel(CharacterID(), GetRace(), GetClass(), GetDeity(), primaryfaction, tmp);
	if (factionlvl >= FACTION_DUBIOUSLY) {
		MerchantRejectMessage(tmp, primaryfaction);
		action = 0;
	}

	// 1199 I don't have time for that now. etc
	if (!tmp->CastToNPC()->IsMerchantOpen()) {
		tmp->Say_StringID(zone->random.Int(1199, 1202));
		action = 0;
	}

	auto outapp = new EQApplicationPacket(OP_ShopRequest, sizeof(Merchant_Click_Struct));
	Merchant_Click_Struct* mco = (Merchant_Click_Struct*)outapp->pBuffer;

	std::string export_string = fmt::format("{}", tmp->GetNPCTypeID());
	parse->EventPlayer(EVENT_CLICK_MERCHANT, this, export_string, 0);

	mco->npcid = mc->npcid;
	mco->playerid = 0;
	mco->command = action; // Merchant command 0x01 = open
	mco->rate = CalcPriceMod(tmp); // This sets the rate merchants will buy player items at.

	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	if (action == 1)
	{
		MerchantWelcome(merchantid, tmp->GetNPCTypeID());
		BulkSendMerchantInventory(merchantid, tmp->GetNPCTypeID());
		MerchantSession = tmp->GetID();
	}

	return;
}

void Client::Handle_OP_Sneak(const EQApplicationPacket *app)
{
	// TODO: this is not correct and causes the skill to never level up until you manually train it to be above 0.
	// Need to differentiate between 0, 254 (untrained) and 255 (can't learn) values but 0 is treated as not having the skill in many places.
	if (!HasSkill(EQ::skills::SkillSneak)) {
		return;
	}

	if (!p_timers.Expired(&database, pTimerSneak, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerSneak, SneakReuseTime - 1);

	uint8 success = SKILLUP_FAILURE;
	int sneakchance = std::max(10, (int)GetSkill(EQ::skills::SkillSneak));
	int random = zone->random.Int(0, 99);
	if (random < sneakchance) 
	{
		sneaking = true;
		success = SKILLUP_SUCCESS;
	}
	else
	{
		sneaking = false;
	}

	if (GetClass() == Class::Rogue)
	{
		if (sneaking)
		{
			Message_StringID(Chat::Skills, SNEAK_SUCCESS);
		}
		else 
		{
			Message_StringID(Chat::Skills, SNEAK_FAIL);
		}
	}

	CheckIncreaseSkill(EQ::skills::SkillSneak, nullptr, zone->skill_difficulty[EQ::skills::SkillSneak].difficulty, success);

	SendAppearancePacket(AppearanceType::Sneak, sneaking);
	Log(Logs::General, Logs::Skills, "Sneak setting sneak to %d.", sneaking);

	return;
}

void Client::Handle_OP_SpawnAppearance(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SpawnAppearance_Struct)) {
		std::cout << "Wrong size on OP_SpawnAppearance. Got: " << app->size << ", Expected: " << sizeof(SpawnAppearance_Struct) << std::endl;
		return;
	}
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)app->pBuffer;

	if (sa->type == AppearanceType::ClientDllMessage || sa->type == AppearanceType::ClientZealMessage) {
		Handle_Connect_OP_SpawnAppearance(app);
		return;
	}

	if (sa->spawn_id != GetID())
		return;

	if (sa->type == AppearanceType::Invisibility) {
		if (sa->parameter != 0)
		{
			return;
		}
		CommonBreakInvisNoSneak();
		entity_list.QueueClients(this, app, true);
		apperance_timer.Start(500);
		Log(Logs::General, Logs::Skills, "Apperance Packet setting hide to 0. Skipping self update...");
		return;
	}
	else if (sa->type == AppearanceType::Animation) {
		if (IsAIControlled() && !has_zomm)
			return;

		if (sa->parameter == Animation::Standing) 
		{
			SetAppearance(eaStanding);
			playeraction = eaStanding;
			SetFeigned(false);
			BindWound(GetID(), false, true);
			camp_timer.Disable();
			camping = false;
			camp_desktop = false;
		}
		else if (sa->parameter == Animation::Sitting) 
		{
			SetAppearance(eaSitting);
			playeraction = eaSitting;

			if (!UseBardSpellLogic())
				InterruptSpell(SPELL_UNKNOWN, true);

			BuffFadeBySitModifier();

			SetFeigned(false);
			BindWound(GetID(), false, true);
		}
		else if (sa->parameter == Animation::Crouching) 
		{
			if (!UseBardSpellLogic())
				InterruptSpell(SPELL_UNKNOWN, true);
			SetAppearance(eaCrouching);
			playeraction = eaCrouching;
			SetFeigned(false);
		}
		else if (sa->parameter == Animation::Lying) 
		{ // feign death too
			SetAppearance(eaDead);
			playeraction = eaDead;
			InterruptSpell();
		}
		else if (sa->parameter == Animation::Looting) 
		{
			SetAppearance(eaLooting);
			playeraction = eaLooting;
			SetFeigned(false);
		}
		else if (sa->parameter == Animation::Freeze) 
		{
			Log(Logs::General, Logs::Error, "Recieved a client initiated freeze request.");
			SendAppearancePacket(AppearanceType::Animation, Animation::Freeze, false);
			return;
		}

		else {
			Log(Logs::Detail, Logs::Error, "Client %s :: unknown appearance %i", name, (int)sa->parameter);
			return;
		}

		entity_list.QueueClients(this, app, true);
	}
	else if (sa->type == AppearanceType::Anonymous) {
		if(!anon_toggle_timer.Check()) {
			return;
		}

		// For Anon/Roleplay
		if (sa->parameter == 1) { // Anon
			m_pp.anon = 1;
		}
		else if ((sa->parameter == 2) || (sa->parameter == 3)) { // This is Roleplay, or anon+rp
			m_pp.anon = 2;
		}
		else if (sa->parameter == 0) { // This is Non-Anon
			m_pp.anon = 0;
		}
		else {
			Log(Logs::Detail, Logs::Error, "Client %s :: unknown Anon/Roleplay Switch %i", name, (int)sa->parameter);
			return;
		}
		entity_list.QueueClients(this, app, true);
		UpdateWho();
	}
	else if ((sa->type == AppearanceType::Health) && (dead == 0)) {
		return;
	}
	else if (sa->type == AppearanceType::AFK) {
		if(afk_toggle_timer.Check()) {
			AFK = (sa->parameter == 1);
			entity_list.QueueClients(this, app, true);
			UpdateWho();
		}
	}
	else if (sa->type == AppearanceType::Split) {
		m_pp.autosplit = (sa->parameter == 1);
	}
	else if (sa->type == AppearanceType::Sneak) {
		if(sneaking == 0)
			return;

		if (sa->parameter != 0)
		{
			if (!HasSkill(EQ::skills::SkillSneak))
			{
				auto hack_str = fmt::format("Player sent OP_SpawnAppearance with AT_Sneak: {} ", sa->parameter);
				database.SetMQDetectionFlag(this->account_name, this->name, hack_str, zone->GetShortName());
			}
			return;
		}
		sneaking = 0;
		entity_list.QueueClients(this, app, true);
		Log(Logs::General, Logs::Skills, "Apperance Packet setting sneak to 0. Skipping self update...");
	}
	else if (sa->type == AppearanceType::Size)
	{
		auto hack_str = fmt::format("Player sent OP_SpawnAppearance with AT_Size: {} ", sa->parameter);
		database.SetMQDetectionFlag(this->account_name, this->name, hack_str, zone->GetShortName());
	}
	else if (sa->type == AppearanceType::Light)	// client emitting light (lightstone, shiny shield)
	{
		//don't do anything with this
	}
	else if (sa->type == AppearanceType::FlyMode)
	{
		// don't do anything with this, we tell the client when it's
		// levitating, not the other way around
	}
	else {
		std::cout << "Unknown SpawnAppearance type: 0x" << std::hex << std::setw(4) << std::setfill('0') << sa->type << std::dec
			<< " value: 0x" << std::hex << std::setw(8) << std::setfill('0') << sa->parameter << std::dec << std::endl;
	}
	return;
}

void Client::Handle_OP_Split(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Split_Struct)) 
	{
		LogError("Wrong size: OP_Split, size=[{}], expected [{}]", app->size, sizeof(Split_Struct));
		return;
	}

	Split_Struct *split = (Split_Struct *)app->pBuffer;

	Group *group = GetGroup();
	if (group == nullptr)
	{
		Message_StringID(Chat::White, SPLIT_NO_GROUP);
		SendClientMoney(split->copper, split->silver, split->gold, split->platinum);
		return;
	}

	uint64 copper = static_cast<uint64>(split->copper) + 10 * static_cast<uint64>(split->silver) + 
		100 * static_cast<uint64>(split->gold) + 1000 * static_cast<uint64>(split->platinum);
	if (!TakeMoneyFromPP(copper)) 
	{
		Message_StringID(Chat::White, SPLIT_FAIL);
		SendClientMoney(split->copper, split->silver, split->gold, split->platinum);
		return;
	}

	group->SplitMoney(split->copper, split->silver, split->gold, split->platinum, this, true);

	return;

}

void Client::Handle_OP_Surname(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Surname_Struct))
	{
		Log(Logs::General, Logs::Error, "Size mismatch in Surname expected %i got %i", sizeof(Surname_Struct), app->size);
		return;
	}

	if (!p_timers.Expired(&database, pTimerSurnameChange, false) && !GetGM())
	{
		Message(Chat::Yellow, "You may only change surnames once every 7 days, your /surname is currently on cooldown.");
		return;
	}

	if (GetLevel() < 20)
	{
		Message_StringID(Chat::Yellow, SURNAME_LEVEL);
		return;
	}

	Surname_Struct* surname = (Surname_Struct*)app->pBuffer;

	char *c = nullptr;
	bool first = true;
	for (c = surname->lastname; *c; c++) {
		if (first) {
			*c = toupper(*c);
			first = false;
		}
		// Valid captialization will be checked by CheckNameFilter
	}

	if (strlen(surname->lastname) >= 20) {
		Message_StringID(Chat::Yellow, SURNAME_TOO_LONG);
		return;
	}

	if (strlen(surname->lastname) > 0 && !database.CheckNameFilter(surname->lastname, true))
	{
		Message_StringID(Chat::Yellow, SURNAME_REJECTED);
		return;
	}

	ChangeLastName(surname->lastname);
	p_timers.Start(pTimerSurnameChange, 604800);

	EQApplicationPacket* outapp = app->Copy();
	surname = (Surname_Struct*)outapp->pBuffer;
	surname->unknown0064 = 1;
	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_SwapSpell(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SwapSpell_Struct)) {
		std::cout << "Wrong size on OP_SwapSpell. Got: " << app->size << ", Expected: " << sizeof(SwapSpell_Struct) << std::endl;
		return;
	}
	const SwapSpell_Struct* swapspell = (const SwapSpell_Struct*)app->pBuffer;
	int swapspelltemp;

	if (swapspell->from_slot < 0 || swapspell->from_slot > MAX_PP_SPELLBOOK || swapspell->to_slot < 0 || swapspell->to_slot > MAX_PP_SPELLBOOK)
		return;

	swapspelltemp = m_pp.spell_book[swapspell->from_slot];
	m_pp.spell_book[swapspell->from_slot] = m_pp.spell_book[swapspell->to_slot];
	m_pp.spell_book[swapspell->to_slot] = swapspelltemp;

	/* Save Spell Swaps */
	if (!database.SaveCharacterSpell(this->CharacterID(), m_pp.spell_book[swapspell->from_slot], swapspell->from_slot)){
		database.DeleteCharacterSpell(this->CharacterID(), m_pp.spell_book[swapspell->from_slot], swapspell->from_slot);
	}
	if (!database.SaveCharacterSpell(this->CharacterID(), swapspelltemp, swapspell->to_slot)){
		database.DeleteCharacterSpell(this->CharacterID(), swapspelltemp, swapspell->to_slot);
	}

	QueuePacket(app);
	return;
}

void Client::Handle_OP_TargetCommand(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClientTarget_Struct)) {
		Log(Logs::General, Logs::Error, "OP size error: OP_TargetMouse expected:%i got:%i", sizeof(ClientTarget_Struct), app->size);
		return;
	}

	// even if feared/charmed we need to save the client side target change so we can restore in AI_Stop()
	ClientTarget_Struct* ct = (ClientTarget_Struct*)app->pBuffer;
	pClientSideTarget = ct->new_target;

	if (IsAIControlled() && !has_zomm)
		return;

	bool tar_cmd = (app->GetOpcode() == OP_TargetCommand);

	// cache current target
	Mob *cur_tar = GetTarget();

	//Message(0, "Target: %d OP_TargetCommand: %s", ct->new_target, tar_cmd ? "true" : "false");
	if (ct->new_target == 0)
	{
		if (tar_cmd)
		{
			// searched for a target with /target (name), and not found.  Client sends over a new_target == 0
			// dont do anything else, client keeps track of own current target.
			Message_StringID(Chat::White, TARGET_NOT_FOUND2);
		} 
		else
		{
			// cleared target
			if (cur_tar) {
				cur_tar->IsTargeted(-1);
				last_target = cur_tar->GetID();
			}
			SetTarget(nullptr);
			QueuePacket(app);
		}
		return;
	}

	bool send_tar = false;

	// Locate and cache new target
	Mob *new_tar = entity_list.GetMob(ct->new_target);

	// For /target, send reject or success packet
	if (tar_cmd) {
		if (new_tar) {
			if (!new_tar->CastToMob()->IsInvisible(this) && (DistanceSquared(m_Position, new_tar->GetPosition())  <= TARGETING_RANGE*TARGETING_RANGE || GetGM())) {
				if (!GetGM() && new_tar->IsUnTargetable())
				{
					//Targeting something we shouldn't with /target
					//but the client allows this without MQ so you don't flag it
					if (!cur_tar) 
					{
						Message_StringID(Chat::White, TARGET_NOT_FOUND2);
						SendTargetCommand(0);
						SetTarget(nullptr);
						pClientSideTarget = 0;
						last_target = 0;
					}
					return;
				}
				// have a valid target
				// send target packet later if pass validation checks
				send_tar = true;
			}
			else
			{
				Message_StringID(Chat::White, TARGET_NOT_FOUND2);
				SendTargetCommand(cur_tar ? cur_tar->GetID() : 0);
				if (!cur_tar) {
					pClientSideTarget = 0;
					SetTarget(nullptr);
				}
				return;
			}
		} else {
			// no target found
			Message_StringID(Chat::White, TARGET_NOT_FOUND2);
			SendTargetCommand(cur_tar ? cur_tar->GetID() : 0);
			if (!cur_tar) {
				SetTarget(nullptr);
				pClientSideTarget = 0;
			}
			return;
		}
	}

	// validate target
	if (new_tar)
	{
		if (GetGM())
		{
			// always allow GM's to target
		} 
		else if (new_tar->IsUnTargetable())
		{
			auto hacker_str = fmt::format(" {} attempting to target something untargetable, {} bodytype: {} \n",
				GetName(), new_tar->GetName(), (int)new_tar->GetBodyType());
			database.SetMQDetectionFlag(AccountName(), GetName(), hacker_str, zone->GetShortName());
			if (cur_tar)
			{
				SendTargetCommand(cur_tar->GetID());
				SetTarget(cur_tar);
			} else {
				SendTargetCommand(0);
				SetTarget(nullptr);
			}
			return;
		}
		else if (IsAssistExempted())
		{
			SetAssistExemption(false);
		}
		else if (new_tar->IsClient())
		{

		}
		else if (IsPortExempted())
		{

		}
		else if (IsSenseExempted())
		{
			SetSenseExemption(false);
		}

		if (new_tar != cur_tar && new_tar != this)
		{
			if (new_tar)
			{
				EQApplicationPacket hp_app;
				new_tar->CreateHPPacket(&hp_app);
				QueuePacket(&hp_app);
			}
		}

		SetTarget(new_tar);

		if (cur_tar) 
		{
			if(cur_tar != new_tar) 
			{
				cur_tar->IsTargeted(-1);
				new_tar->IsTargeted(1);
			}
		} 
		else 
		{
			new_tar->IsTargeted(1);
		}
		if (send_tar)
			QueuePacket(app);

	} 
	else 
	{
		SendTargetCommand(0);
		SetTarget(nullptr);
	}
	return;
}

void Client::Handle_OP_TargetMouse(const EQApplicationPacket *app)
{
	Handle_OP_TargetCommand(app);
}

void Client::Handle_OP_Taunt(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(ClientTarget_Struct)) {
		std::cout << "Wrong size on OP_Taunt. Got: " << app->size << ", Expected: " << sizeof(ClientTarget_Struct) << std::endl;
		return;
	}

	if (!p_timers.Expired(&database, pTimerTaunt, false)) {
		Log(Logs::General, Logs::Error, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerTaunt, TauntReuseTime - 1);

	if (GetTarget() == nullptr || !GetTarget()->IsNPC())
		return;

	if (!CombatRange(GetTarget()))
		return;

	Taunt(GetTarget()->CastToNPC(), false);
	return;
}

void Client::Handle_OP_TGB(const EQApplicationPacket *app) 
{
	OPTGB(app);
	return;
}

void Client::Handle_OP_Track(const EQApplicationPacket *app)
{
	if (GetClass() != Class::Ranger && GetClass() != Class::Druid && GetClass() != Class::Bard)
	{
		Kick(); //The client handles tracking for us, simply returning is not enough if they are cheating.
		return;
	}

	if (GetSkill(EQ::skills::SkillTracking) == 0)
		SetSkill(EQ::skills::SkillTracking, 1);
	else
		CheckIncreaseSkill(EQ::skills::SkillTracking, nullptr, zone->skill_difficulty[EQ::skills::SkillTracking].difficulty);

	return;
}

void Client::Handle_OP_TradeAcceptClick(const EQApplicationPacket *app)
{
	Mob* with = trade->With();
	trade->state = TradeAccepted;

	if (with && with->IsClient()) {
		//finish trade...
		// Have both accepted?
		Client* other = with->CastToClient();
		other->QueuePacket(app);

		if (other->trade->state == trade->state) {
			other->trade->state = TradeCompleting;
			trade->state = TradeCompleting;

			if (CheckTradeLoreConflict(other) || other->CheckTradeLoreConflict(this)) {
				Message_StringID(Chat::Red, TRADE_CANCEL_LORE);
				other->Message_StringID(Chat::Red, TRADE_CANCEL_LORE);
				this->FinishTrade(this);
				other->FinishTrade(other);
				other->trade->Reset();
				trade->Reset();
			}
			else 
			{
				// start QS code
				if (RuleB(QueryServ, PlayerLogTrades)) 
				{
					QSPlayerLogTrade_Struct event_entry;
					memset(&event_entry, 0, sizeof(QSPlayerLogTrade_Struct));

					// Perform actual trade
					this->FinishTrade(other, true, &event_entry);
					other->FinishTrade(this, false, &event_entry);

					QSPlayerLogTrade_Struct* QS = new struct QSPlayerLogTrade_Struct;
					memcpy(QS, &event_entry, sizeof(QSPlayerLogTrade_Struct));

					QServ->QSPlayerTrade(QS);
					safe_delete(QS);
					// end QS code
				}
				else
				{
					this->FinishTrade(other);
					other->FinishTrade(this);
				}

				other->trade->Reset();
				trade->Reset();
			}
			// All done
			auto outapp = new EQApplicationPacket(OP_FinishTrade, 0);
			other->QueuePacket(outapp);
			this->FastQueuePacket(&outapp);
		}
	}
	// Trading with a Mob object that is not a Client.
	else if (with) {

		if(with->IsNPC() && with->IsEngaged())
		{
			SendCancelTrade(with);
			Log(Logs::General, Logs::Trading, "Cancelled in-progress trade due to %s being in combat.", with->GetCleanName());
			return;
		}

		auto outapp = new EQApplicationPacket(OP_FinishTrade, 0);
		QueuePacket(outapp);
		safe_delete(outapp);

		outapp = new EQApplicationPacket(OP_TradeReset, 0);
		QueuePacket(outapp);
		safe_delete(outapp);

		if (with->IsNPC()) 
		{
			FinishTrade(with->CastToNPC());
		}
		trade->Reset();
	}


	return;
}

void Client::Handle_OP_Trader(const EQApplicationPacket *app) 
{
	if(zone->GetZoneID() != Zones::BAZAAR)
		return;

	if (Admin() > 0)
	{
		Message(Chat::Red, "You are a GM. You cannot use the bazaar. Use the dev server for that.");
		return;
	}

	uint16 code = app->pBuffer[0];
	uint32 max_items = 80;

	if(app->size == 20)
	{
		switch (code)
		{
			case BazaarTrader_EndTraderMode:
			{
				Trader_EndTrader();
				break;
			}
			case BazaarTrader_EndTransaction:
			{
				TraderStatus_Struct* sis = (TraderStatus_Struct*)app->pBuffer;
				Client* c = entity_list.GetClientByID(sis->TraderID);
				if (c)
				{
					TraderSession = 0;
					if(!entity_list.TraderHasCustomer(c))
					{
						c->WithCustomer = false;
					}
				}
				else
					Log(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_Trader: Null Client Pointer");

				break;
			}
			case BazaarTrader_ShowItems:
			{
				Trader_ShowItems();
				break;
			}
			default:
			{
				Log(Logs::General, Logs::Bazaar, "Unhandled Trader action %d!", code);
				break;
			}
		}
	}
	else if(app->size == sizeof(TraderPriceUpdate_Struct))
	{
		HandleTraderPriceUpdate(app);
	}
	//Show Items
	else if (app->size == sizeof(Trader_Struct))
	{
		Trader_Struct* ints = (Trader_Struct*)app->pBuffer;

		if (ints->Code == BazaarTrader_StartTraderMode && app->size == sizeof(Trader_Struct))
		{
			if (IsSelfFoundAny())
			{
				Message(Chat::Red, "You are solo or self found only, and cannot list or sell items in The Bazaar.");
				return;
			}
			if (Admin() > 0)
			{
				Message(Chat::Red, "You are a GM. You cannot list items for sale. Use the dev server for that.");
				database.SetHackerFlag(account_name, GetCleanName(), "GM attempted to sell an item on the Bazaar.");
				return;
			}
			GetItems_Struct* gis = GetTraderItems();
		//	bool TradeItemsValid = true;

			uint16 slot = 0;
			// In this loop, we will compare the items sent by the client in OP_Trader with the items the server has in the player's trader satchels.
			for (uint32 i = 0; i < max_items; ++i) 
			{
				// We've reached the end of the server's item list.
				if (gis->Items[i] == 0) break;

				//This is informative, our checks below will decide if we need to return or not. The most often case of this happening is the client skips items with no
				//price set, and the server does not (or can not rather, it has no knowledge of the cost until the client tells it what the trader set it to.)
				if(ints->Items[i] != gis->Items[i])
				{
					Log(Logs::Detail, Logs::Bazaar, "Server/Client mismatch for slot %d. Server item: %d Client item: %d", i, gis->Items[i], ints->Items[i]);
				}

				const EQ::ItemData *Item = database.GetItem(ints->Items[i]);

				// This usually means the client's item list has ended, and SOE did not memset. :)
				if (!Item) 
				{
					Log(Logs::Detail, Logs::Bazaar, "Invalid item %d sent by client. (Possible end of the client's list.)", ints->Items[i]);
					continue;
				}

				// Ignore no drop items.
				if (Item->NoDrop == 0) 
				{
					Log(Logs::Detail, Logs::Bazaar, "Item %d is no drop and will be ignored.");
					continue;
				}

				// Our client will not send an item if the cost has never been set by the player. If the item was previously set with a cost
				// but the player has since cleared it, the item will be sent with cost 1. Check for cost to be sure.
				if (ints->ItemCost[i] == 0) 
				{
					Log(Logs::Detail, Logs::Bazaar, "0 cost item %d sent by client.", Item->ID);
					continue;
				}

				bool stacked = false;
				if (database.ItemQuantityType(Item->ID) == EQ::item::Quantity_Stacked)
				{
					stacked = true;
					Log(Logs::Detail, Logs::Bazaar, "(%d) Item %d is stackable, and will be combined.", i, Item->ID);
				}

				// The client does not update the trader window if items are moved around in the satchel while the windows is up. So, it will send out of date items the next time 
				// the player hits the Start Trader button. This will confirm each item exists in the player's inventory, and if not we ask the player to close their window and begin
				//again.
				if (!stacked)
				{
					GetItem_Struct gi = GrabItem(Item->ID);
					const EQ::ItemData *checkitem = database.GetItem(gi.Items);
					if (checkitem)
					{
						gis->Items[i] = gi.Items;
						gis->Charges[i] = gi.Charges;
						gis->SlotID[i] = gi.SlotID;
					}
					else
					{
						Log(Logs::Detail, Logs::Bazaar, "(%d) Client sent item %d that the server no longer has as being valid (%d). It may have been moved or deleted while the trader window was up.", i, Item->ID, gi.Items);
						//Message(Chat::Red, "Client sent invalid item %s.", Item->Name);
						//TradeItemsValid = false;
						//break;
						continue;
					}
				}
				else
				{
					GetItem_Struct gi = GrabStackedItem(Item->ID);
					const EQ::ItemData *checkitem = database.GetItem(gi.Items);
					if (checkitem)
					{
						gis->Items[i] = gi.Items;
						gis->Charges[i] = gi.Charges;
						gis->SlotID[i] = gi.SlotID;
					}
					else
					{
						Log(Logs::Detail, Logs::Bazaar, "(%d) Stacked item %d likely already was saved to the DB.", i, Item->ID);
						continue;
					}
				}

				database.SaveTraderItem(this->CharacterID(), Item->ID, gis->SlotID[i], gis->Charges[i], ints->ItemCost[i], slot);
				Log(Logs::Detail, Logs:: Bazaar, "(%d) Item: %s (%d) from slot %d added to trader list with cost: %d and charges: %d to trader slot %d", i, Item->Name, Item->ID, gis->SlotID[i], ints->ItemCost[i], gis->Charges[i], slot);
				++slot;
			}

			Log(Logs::Detail, Logs::Bazaar, "%d total items are up for sale.", slot);

			/*if (!TradeItemsValid) 
			{
				Trader_EndTrader();
				return;
			}*/

			safe_delete(gis);

			this->Trader_StartTrader();
		}
		else 
		{
			Log(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_Trader: Unknown TraderStruct code of: %i\n",
				ints->Code);

			Log(Logs::General, Logs::Error, "Unknown TraderStruct code of: %i\n", ints->Code);
		}
	}
	else 
	{
		Log(Logs::General, Logs::Error, "Unknown size for OP_Trader: %i Code: %d", app->size, code);
		return;
	}

	return;
}

void Client::Handle_OP_TraderBuy(const EQApplicationPacket *app) 
{


	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	if (IsSelfFoundAny())
	{
		TradeRequestFailed(app);
		Message(Chat::Red, "You are solo or self found only, and cannot purchase items from The Bazaar.");
		return;
	}

	if (Admin() > 0)
	{
		Message(Chat::Red, "You are a GM. You cannot list items for sale. Use the dev server for that.");
		database.SetHackerFlag(account_name, GetCleanName(), "GM attempted to sell an item on the Bazaar.");
		return;
	}
	// Bazaar Trader:
	//
	// Client has elected to buy an item from a Trader
	//

	if (app->size == sizeof(TraderBuy_Struct)){

		TraderBuy_Struct* tbs = (TraderBuy_Struct*)app->pBuffer;

		if (Client* Trader = entity_list.GetClientByID(tbs->TraderID)){

			BuyTraderItem(tbs, Trader, app);
		}
		else {
			Log(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_TraderBuy: Null Client Pointer");
		}
	}
	else {
		Log(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_TraderBuy: Struct size mismatch");

	}
	return;
}

void Client::Handle_OP_TradeRequest(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(TradeRequest_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_TradeRequest, size=%i, expected %i", app->size, sizeof(TradeRequest_Struct));
		return;
	}
	// Client requesting a trade session from an npc/client
	// Trade session not started until OP_TradeRequestAck is sent
	if (!trade_timer.Check())
		return;

	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	CommonBreakInvisible(true);

	// Pass trade request on to recipient
	TradeRequest_Struct* msg = (TradeRequest_Struct*)app->pBuffer;

	if (msg->from_mob_id != GetID()) {
		// Client sent a trade request with an originator ID not matching their own ID.
		auto hack_str = fmt::format("Player {} ( {} ) sent OP_TradeRequest with from_mob_id of: {} ", GetCleanName(), GetID(), msg->from_mob_id);
		database.SetMQDetectionFlag(this->account_name, this->name, hack_str, zone->GetShortName());
		return;
	}

	Mob* tradee = entity_list.GetMob(msg->to_mob_id);

	if (tradee && tradee->IsClient()) 
	{
		if (IsFeigned())
		{
			FinishTrade(this);
			trade->Reset();
			return;
		}

		if (IsSelfFoundAny())
		{
			Message(Chat::Red, "You are doing a self-found run. You cannot trade with other players.");
			FinishTrade(this);
			trade->Reset();
			return;
		}

		if (tradee->CastToClient()->IsSelfFoundAny())
		{
			Message(Chat::Red, "This player is doing a self found run. You cannot trade with them.");
			FinishTrade(this);
			trade->Reset();
			return;
		}

		if (Admin() > 0)
		{
			Message(Chat::Red, "You are a GM. You cannot trade with other players. Use the dev server for that.");
			database.SetHackerFlag(account_name, GetCleanName(), "GM attempted to trade with another player instead of using GM commands.");
			return;
		}

		if (tradee->CastToClient()->Admin() > 0)
		{
			Message(Chat::YouMissOther, "You attempt to trade with a GM, but miss!");
			return;
		}

		if (trade->state != TradeNone && trade->state != Requesting) {
			// Put any trade items/cash back into inventory
			CancelTrade_Struct* msg = (CancelTrade_Struct*) app->pBuffer;
			msg->fromid = GetID();
			msg->action = 1;
			Mob* other = trade->With();
			if (other && other->IsClient() && other->trade->GetWithID() == GetID()) {
				// send a cancel to who we were already trading with.
				other->CastToClient()->QueuePacket(app);
				other->CastToClient()->FinishTrade(other);
				other->trade->Reset();
			}
			QueuePacket(app);
			FinishTrade(this);
			trade->Reset();
			return;
		}
		// Check if the other client is busy in a trade
		if (tradee && ((tradee->CastToClient()->trade->state != TradeNone && tradee->CastToClient()->trade->GetWithID() != GetID()) 
			|| tradee->CastToClient()->IsFeigned())) {
			// Other client is in a trade, and its not with us.  So don't send a trade request.
			// Let the other client know we are interested in a trade.
			tradee->CastToClient()->Message_StringID(0,TRADE_INTERESTED,GetCleanName());
			// Send reply to client that other client is busy
			Message_StringID(0,TRADE_BUSY,tradee->CastToClient()->GetCleanName());
			FinishTrade(this);
			trade->Reset();
			return;
		}
		trade->Request(msg->to_mob_id);
		tradee->CastToClient()->QueuePacket(app);
	}
	else if (tradee && tradee->IsNPC()) 
	{
		
		if(tradee->IsEngaged())
		{
			auto outapp = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
			CancelTrade_Struct* ct = (CancelTrade_Struct*) outapp->pBuffer;
			ct->fromid = tradee->GetID();
			FastQueuePacket(&outapp);
			Log(Logs::General, Logs::Trading, "Cancelled trade request due to %s being in combat.", tradee->GetCleanName());
			return;
		}

		//npcs always accept
		trade->Start(msg->to_mob_id);

		auto outapp = new EQApplicationPacket(OP_TradeReset, 0);
		FastQueuePacket(&outapp);

		outapp = new EQApplicationPacket(OP_TradeRequestAck, sizeof(TradeRequest_Struct));
		TradeRequest_Struct* acc = (TradeRequest_Struct*)outapp->pBuffer;
		acc->from_mob_id = msg->to_mob_id;
		acc->to_mob_id = msg->from_mob_id;
		FastQueuePacket(&outapp);
	}
	// cannot find person to trade with
	return;
}

void Client::Handle_OP_TradeRequestAck(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(TradeRequest_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_TradeRequestAck, size=%i, expected %i", app->size, sizeof(TradeRequest_Struct));
		return;
	}

	if (RuleB(Quarm, RestrictIksarsToKunark))
	{
		if (GetBaseRace() == IKSAR && zone->GetZoneExpansion() == ClassicEQ)
			return;
		else if (GetBaseRace() != IKSAR && zone->GetZoneExpansion() == KunarkEQ)
		{
			return;
		}
	}

	// Trade request recipient is acknowledging they are able to trade
	// After this, the trade session has officially started
	// Send ack on to trade initiator if client
	TradeRequest_Struct* msg = (TradeRequest_Struct*)app->pBuffer;
	Mob* tradee = entity_list.GetMob(msg->to_mob_id);

	if (tradee && tradee->IsClient() && tradee->trade->state == Requesting && tradee->trade->GetWithID() == GetID()) {
		trade->Start(msg->to_mob_id);
		tradee->CastToClient()->QueuePacket(app);
	}
	return;
}

void Client::Handle_OP_TraderShop(const EQApplicationPacket *app) 
{
	// Bazaar Trader:
	//
	// This is when a potential purchaser right clicks on this client who is in Trader mode to
	// browse their goods.

	if (app->size != sizeof(TraderClick_Struct)) 
	{
		Log(Logs::Detail, Logs::Error, "Client::Handle_OP_TraderShop: Returning due to struct size mismatch");
		return;
	}

	TraderClick_Struct* tcs = (TraderClick_Struct*)app->pBuffer;
	auto outapp = new EQApplicationPacket(OP_TraderShop, sizeof(TraderClick_Struct));
	TraderClick_Struct* outtcs = (TraderClick_Struct*)outapp->pBuffer;
	Client* Trader = entity_list.GetClientByID(tcs->TraderID);

	if (Trader && !Trader->IsLD())
	{
		outtcs->Approval = 1;
		outtcs->TraderID = tcs->TraderID;
	}
	else 
	{
		Log(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_TraderShop: entity_list.GetClientByID(tcs->traderid)"
			" returned a nullptr pointer");
		outtcs->Approval = 0;
		outtcs->TraderID = 0;
	}
	
	outtcs->Unknown008 = 0x3f800000;
	QueuePacket(outapp);

	if (outtcs->Approval) 
	{
		this->BulkSendTraderInventory(Trader->CharacterID());
		Trader->Trader_CustomerBrowsing(this);
		TraderSession = Trader->GetID();
		Trader->WithCustomer = true;
	}
	else
		Message_StringID(clientMessageYellow, TRADER_BUSY);

	safe_delete(outapp);
	return;
}

void Client::Handle_OP_TradeSkillCombine(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Combine_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for NewCombine_Struct: Expected: %i, Got: %i",
			sizeof(Combine_Struct), app->size);
		return;
	}
	/*if (m_tradeskill_object == nullptr) {
	Message(Chat::Red, "Error: Server is not aware of the tradeskill container you are attempting to use");
	return;
	}*/

	//fixed this to work for non-world objects

	// Delegate to tradeskill object to perform combine
	Combine_Struct* in_combine = (Combine_Struct*)app->pBuffer;
	Object::HandleCombine(this, in_combine, m_tradeskill_object);

	return;
}

void Client::Handle_OP_Translocate(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Translocate_Struct)) {
		Log(Logs::General, Logs::Error, "Size mismatch in OP_Translocate expected %i got %i", sizeof(Translocate_Struct), app->size);
		DumpPacket(app);
		return;
	}
	Translocate_Struct *its = (Translocate_Struct*)app->pBuffer;

	if (!PendingTranslocate)
		return;

	if ((RuleI(Spells, TranslocateTimeLimit) > 0) && (time(nullptr) > (TranslocateTime + RuleI(Spells, TranslocateTimeLimit)))) {
		Message(Chat::Red, "You did not accept the Translocate within the required time limit.");
		PendingTranslocate = false;
		return;
	}

	if (zone->GetGuildID() == 1) {
		Message(Chat::Red, "[PVP] You cannot translocate here.");
		PendingTranslocate = false;
		return;
	}

	if (its->Complete == 1) {

		int SpellID = PendingTranslocateData.spell_id;
		int i = parse->EventSpell(EVENT_SPELL_EFFECT_TRANSLOCATE_COMPLETE, nullptr, this, SpellID, 0);

		if (i == 0)
		{
			// If the spell has a translocate to bind effect, AND we are already in the zone the client
			// is bound in, use the GoToBind method. If we send OP_Translocate in this case, the client moves itself
			// to the bind coords it has from the PlayerProfile, but with the X and Y reversed. I suspect they are
			// reversed in the pp, and since spells like Gate are handled serverside, this has not mattered before.
			if (((SpellID == 1422) || (SpellID == 1334) || (SpellID == 3243)) &&
				(zone->GetZoneID() == PendingTranslocateData.zone_id))
			{
				PendingTranslocate = false;
				GoToBind();
				return;
			}

			////Was sending the packet back to initiate client zone...
			////but that could be abusable, so lets go through proper channels
			MovePCGuildID(PendingTranslocateData.zone_id, GUILD_NONE,
				PendingTranslocateData.x, PendingTranslocateData.y,
				PendingTranslocateData.z, PendingTranslocateData.heading, 0, ZoneSolicited);
		}
	}

	PendingTranslocate = false;
}

void Client::Handle_OP_WearChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(WearChange_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size: OP_WearChange, size %i expected %i", app->size, sizeof(WearChange_Struct));
		return;
	}

	WearChange_Struct* wc = (WearChange_Struct*)app->pBuffer;

	if (wc->spawn_id != GetID() || wc->wear_slot_id > EQ::textures::materialCount)
		return;

	if (wc->wear_slot_id < EQ::textures::weaponPrimary)
	{
		if (!IsPlayableRace(GetRace()))
		{
			Log(Logs::Detail, Logs::Inventory, "Recieved a wearchange request from %s while they are illusioned as race %d. Returning.", GetName(), GetRace());
			return;
		}
		else if (wc->wear_slot_id == EQ::textures::armorWrist && pc_bracertexture != INVALID_INDEX)
		{
			// The client sends a wearchange whenever either wrist changes, and since both share a single texture slot, we have no way to know which slot is being
			// changed here. (We always want to prefer wrist1.) So, we want to let SwapItem() handle the wearchange since it will know which inventory slot is being worked with.
			// We will only handle a wrist change if pc_bracertexture has never been set, and this is the intial wearchange sent by the client during zone-in (or the player had no
			// wrist item at zone-in and acquired one later.)
			Log(Logs::Detail, Logs::Inventory, "Recieved a wrist wearchange request from %s. Let SwapItem() handle it.", GetName());
			return;
		}
		else
		{
			SetPCTexture(wc->wear_slot_id, wc->material, wc->color.Color, false);
		}
	}

	bool force_helm = wc->wear_slot_id == EQ::textures::armorHead && wc->material == 0 && wc->color.Color == 0;

	Log(Logs::Detail, Logs::Inventory, "Received a wear change request from %s on slot %d for texture %d and color %d.", GetName(), wc->wear_slot_id, wc->material, wc->color.Color);
	entity_list.QueueWearChange(this, app, true, wc->wear_slot_id, force_helm);
	return;
}

void Client::Handle_OP_WhoAllRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Who_All_Struct)) {
		Log(Logs::General, Logs::Error, "Wrong size on OP_WhoAll. Got: %i, Expected: %i", app->size, sizeof(Who_All_Struct));
		return;
	}
	Who_All_Struct* whoall = (Who_All_Struct*)app->pBuffer;

	WhoAll(whoall);

	return;
}

void Client::Handle_OP_YellForHelp(const EQApplicationPacket *app)
{
	auto outapp = new EQApplicationPacket(OP_YellForHelp, 4);
	*(uint32 *)outapp->pBuffer = GetID();
	entity_list.QueueCloseClients(this, outapp, true, 100.0);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_ZoneEntryResend(const EQApplicationPacket *app)
{
	//EQMac doesn't reply to this according to ShowEQ captures.
	return;
}

void Client::Handle_OP_LFGCommand(const EQApplicationPacket *app)
{
	if (app->size != sizeof(LFG_Struct)) {
		Log(Logs::General, Logs::Error, "Invalid size for LFG_Struct: Expected: %i, Got: %i", sizeof(LFG_Struct), app->size);
		return;
	}

	LFG_Struct* lfg = (LFG_Struct*) app->pBuffer;
	UpdateLFG(static_cast<bool>(lfg->value), true);

	return;
}

void Client::Handle_OP_Disarm(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Disarm_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for Disarm_Struct: Expected: %i, Got: %i", sizeof(Disarm_Struct), app->size);
		return;
	}

	Disarm_Struct* disin = (Disarm_Struct*)app->pBuffer;
	Mob* target = entity_list.GetMob(disin->target);

	if (!target) {
		return;
	}

	if (target != GetTarget()) {
		return;
	}

	// It looks like the client does its own check except FD, but let's double check.
	if(target->IsCorpse() || !IsAttackAllowed(target) || IsFeigned()) {
		LogDebug("Client does not allow to disarm on corpse, non-combat npc, pvp, and while feign death");
		return;
	}

	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		Message(Chat::Red, "You cannot disarm NPCs in this zone.");
		return;
	}

	// this is completely made up and based on nothing other than disarm rates being seemingly around 20-25% in logs
	float disarmchance = static_cast<float>(GetSkill(EQ::skills::SkillDisarm)) / static_cast<float>(std::max(static_cast<int>(target->GetSkill(EQ::skills::SkillOffense)), 5) * 3);

	if (disarmchance > 0.95f) {
		disarmchance = 0.95f;
	}

	if (disarmchance < 0.05f) {
		disarmchance = 0.05f;
	}

	bool success = 0;
	int8 disarm_result = 0;
	
	disarm_result = target->Disarm(disarmchance);
	if (disarm_result < 2) {
		success = false;
	} else {
		success = true;
	}

	if(target->IsNPC()) {
		if(!GetGM())
			target->AddToHateList(this, 1);
	}

	if(disarm_result > 0) {
		uint8 skillsuccess = disarm_result == 2 ? SKILLUP_SUCCESS : SKILLUP_FAILURE;
		CheckIncreaseSkill(EQ::skills::SkillDisarm, target, zone->skill_difficulty[EQ::skills::SkillDisarm].difficulty, skillsuccess);
	}

	auto outapp = new EQApplicationPacket(OP_Disarm, sizeof(Disarm_Struct));
	Disarm_Struct* dis = (Disarm_Struct*)outapp->pBuffer;
	dis->entityid = disin->target;// These are reversed on the out packet.
	dis->target = disin->entityid;
	dis->skill = disin->skill;
	dis->status = success;

	QueuePacket(outapp);
	safe_delete(outapp);

	return;
}

void Client::Handle_OP_Feedback(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Feedback_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for Feedback_Struct: Expected: %i, Got: %i", sizeof(Feedback_Struct), app->size);
		return;
	}

	Feedback_Struct* in = (Feedback_Struct*)app->pBuffer;
	database.UpdateFeedback(in);

	Message(Chat::Yellow, "Thank you, %s. Your feedback has been recieved.", in->name);
	return;
}


void Client::Handle_OP_SoulMarkList(const EQApplicationPacket *app)
{
	if(Admin() < 80)
		return;

	if (app->size != sizeof(SoulMarkList_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for SoulMarkList_Struct: Expected: %i, Got: %i", sizeof(SoulMarkList_Struct), app->size);
		return;
	}
	SoulMarkList_Struct* in = (SoulMarkList_Struct*)app->pBuffer;

	uint32 charid = 0;
	if(strlen(in->interrogatename) > 0)
	{
		charid = database.GetCharacterID(in->interrogatename);
	}

	if(charid != 0) 
	{
		int max = database.RemoveSoulMark(charid);
		uint32 i = 0;
		for(i = 0; i < max; i++)
		{
			if(in->entries[i].type != 0)
			database.AddSoulMark(charid, in->entries[i].name, in->entries[i].accountname,  in->entries[i].gmname, in->entries[i].gmaccountname, in->entries[i].unix_timestamp, in->entries[i].type, in->entries[i].description);
		}
	}

	return;
}


void Client::Handle_OP_SoulMarkAdd(const EQApplicationPacket *app)
{
	if(Admin() < 80)
		return;

	if (app->size != sizeof(SoulMarkEntry_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for SoulMarkEntry_Struct: Expected: %i, Got: %i", sizeof(SoulMarkEntry_Struct), app->size);
		return;
	}
	SoulMarkEntry_Struct* in = (SoulMarkEntry_Struct*)app->pBuffer;

	uint32 charid = 0;
	if(strlen(in->name) > 0)
	{
		charid = database.GetCharacterID(in->name);
	}
	if(charid != 0)
	{
		database.AddSoulMark(charid, in->name, in->accountname, in->gmname, in->gmaccountname, std::time(nullptr), in->type, in->description);
	}

	return;
}

void Client::Handle_OP_SoulMarkUpdate(const EQApplicationPacket *app)
{
	if(Admin() < 80)
		return;

	if (app->size != sizeof(SoulMarkUpdate_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for SoulMarkUpdate_Struct: Expected: %i, Got: %i", sizeof(SoulMarkList_Struct), app->size);
		return;
	}
	SoulMarkUpdate_Struct* in = (SoulMarkUpdate_Struct*)app->pBuffer;

	auto pack = new ServerPacket(ServerOP_Soulmark, sizeof(ServerRequestSoulMark_Struct));
	ServerRequestSoulMark_Struct* scs = (ServerRequestSoulMark_Struct*)pack->pBuffer;
	strcpy(scs->name, GetCleanName());
	strcpy(scs->entry.interrogatename, in->charname);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	return;
}

void Client::Handle_OP_MBRetrievalRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MBRetrieveMessages_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for MBRetrieveMessages_Struct: Expected: %i, Got: %i", sizeof(MBRetrieveMessages_Struct), app->size);
		return;
	}
	MBRetrieveMessages_Struct* in = (MBRetrieveMessages_Struct*)app->pBuffer;	
	std::vector<MBMessageRetrievalGen_Struct> messageList;

	database.RetrieveMBMessages(in->category, messageList);

	for (int i=0; i<messageList.size();i++) {   

		auto outapp = new EQApplicationPacket(OP_MBRetrievalResponse, sizeof(MBMessageRetrievalGen_Struct));             
		memcpy(outapp->pBuffer, &messageList[i], sizeof(MBMessageRetrievalGen_Struct));
		QueuePacket(outapp);
		safe_delete(outapp);
	}

	
	auto outapp = new EQApplicationPacket(OP_MBRetrievalFin, 0);
	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_OP_MBRetrievalDetailRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MBModifyRequest_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for MBModifyRequest_Struct: Expected: %i, Got: %i", sizeof(MBModifyRequest_Struct), app->size);
		return;
	}
	MBModifyRequest_Struct* in = (MBModifyRequest_Struct*)app->pBuffer;	
	char messageText[2048] = "";	

	
	if(database.ViewMBMessage(in->id, messageText))
	{
		auto outapp = new EQApplicationPacket(OP_MBRetrievalDetailResponse, strlen(messageText) + 1);             
		memcpy(outapp->pBuffer, messageText, strlen(messageText));
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	else
	{
		auto outapp = new EQApplicationPacket(OP_MBRetrievalDetailResponse, 0);
		QueuePacket(outapp);
		safe_delete(outapp);
		
	}

	//No fin sent for this packet. Just data.

	return;
}



void Client::Handle_OP_MBRetrievalPostRequest(const EQApplicationPacket *app)
{
	if (app->size > sizeof(MBMessageRetrievalGen_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for MBRetrieveMessages_Struct: Expected: at most %i, Got: %i", sizeof(MBMessageRetrievalGen_Struct), app->size);
		return;
	}
	MBMessageRetrievalGen_Struct* in = (MBMessageRetrievalGen_Struct*)app->pBuffer;
	database.PostMBMessage(CharacterID(), GetCleanName(), in);

	auto outapp = new EQApplicationPacket(OP_MBRetrievalFin, 0);
	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_OP_MBRetrievalEraseRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MBEraseRequest_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for MBRetrieveMessages_Struct: Expected: %i, Got: %i", sizeof(MBEraseRequest_Struct), app->size);
		return;
	}
	MBEraseRequest_Struct* in = (MBEraseRequest_Struct*)app->pBuffer;	
	database.EraseMBMessage(in->id, CharacterID());
	
	auto outapp = new EQApplicationPacket(OP_MBRetrievalFin, 0);
	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_OP_Key(const EQApplicationPacket *app)
{
	if (app->size != 4) {
		Log(Logs::Detail, Logs::Error, "Invalid size for OP_Key: Expected: %i, Got: %i", 4, app->size);
		return;
	}

	KeyRingList(this);

	return;
}

void Client::Handle_OP_TradeRefused(const EQApplicationPacket *app)
{
	if (app->size != sizeof(RefuseTrade_Struct)) {
		Log(Logs::Detail, Logs::Error, "Invalid size for OP_TradeRefused: Expected: %i, Got: %i", sizeof(RefuseTrade_Struct), app->size);
		return;
	}

	RefuseTrade_Struct* in = (RefuseTrade_Struct*)app->pBuffer;	
	Client* client = entity_list.GetClientByID(in->fromid);

	if(client)
	{
		if ((trade->state == TradeNone && client->trade->state == Requesting) || in->type == 98 || in->type == 99) {
			client->FinishTrade(client);
			client->trade->Reset();
		}
		if (in->type == 98) {
			client->Message_StringID(Chat::White, TRADE_NOBODY, GetCleanName());
		} else if (in->type == 99) {
			client->Message_StringID(Chat::White, TRADE_GROUP_ONLY, GetCleanName());
		} else {
			client->QueuePacket(app);
		}
	}

	return;
}

void Client::Handle_OP_SpellTextMessage(const EQApplicationPacket *app)
{
	// this message is sent in two places in the client
	// 1. lifetap spells 'groaning'
	// 2. spell cast interrupt from stunning warrior kick

	// this is the layout but no need to do any processing on this, we just forward it on
	//int16 entity_id = *(int16 *)app->pBuffer;
	//char *spell_emote_msg = (char *)(app->pBuffer + 2);

	entity_list.QueueCloseClients(this, app, true, 200.0);
}
