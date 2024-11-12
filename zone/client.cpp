/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.org)

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
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// for windows compile
#ifndef _WINDOWS
	#include <stdarg.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include "../common/unix.h"
#endif

#include "../common/eqemu_logsys.h"
#include "../common/features.h"
#include "../common/spdat.h"
#include "../common/guilds.h"
#include "../common/languages.h"
#include "../common/rulesys.h"
#include "../common/races.h"
#include "../common/classes.h"
#include "../common/strings.h"
#include "../common/data_verification.h"
#include "data_bucket.h"
#include "position.h"
#include "worldserver.h"
#include "zonedb.h"
#include "petitions.h"
#include "command.h"
#include "string_ids.h"
#include "water_map.h"

#include "guild_mgr.h"
#include "quest_parser_collection.h"
#include "../common/crc32.h"
#include "../common/packet_dump_file.h"
#include "queryserv.h"

#include "../common/repositories/character_spells_repository.h"

extern QueryServ* QServ;
extern EntityList entity_list;
extern Zone* zone;
extern volatile bool is_zone_loaded;
extern WorldServer worldserver;
extern uint32 numclients;
extern PetitionList petition_list;
bool commandlogged;
char entirecommand[255];

void UpdateWindowTitle(char* iNewTitle);

Client::Client(EQStreamInterface* ieqs)
: Mob("No name",	// name
	"",	// lastname
	0,	// cur_hp
	0,	// max_hp
	0,	// gender
	0,	// race
	0,	// class
	BT_Humanoid,	// bodytype
	0,	// deity
	0,	// level
	0,	// npctypeid
	0,	// size
	RuleR(Character, BaseRunSpeed),	// runspeed
	glm::vec4(),
	0,	// light - verified for client innate_light value
	0xFF,	// texture
	0xFF,	// helmtexture
	0,	// ac
	0,	// atk
	0,	// str
	0,	// sta
	0,	// dex
	0,	// agi
	0,	// int
	0,	// wis
	0,	// cha
	0,	// Luclin Hair Colour
	0,	// Luclin Beard Color
	0,	// Luclin Eye1
	0,	// Luclin Eye2
	0,	// Luclin Hair Style
	0,	// Luclin Face
	0,	// Luclin Beard
	EQ::TintProfile(),	// Armor Tint
	0xff,	// AA Title
	0,	// see_invis
	0,	// see_invis_undead
	0,
	0,
	0,
	0,
	0,	// qglobal
	0,	// maxlevel
	0,	// scalerate
	0,
	0,
	0,
	0,
	0,
	0
	),
	//these must be listed in the order they appear in client.h
	position_timer(250),
	get_auth_timer(5000),
	hpupdate_timer(6000),
	camp_timer(35000),
	process_timer(100),
	stamina_timer(46000),
	zoneinpacket_timer(1000),
	accidentalfall_timer(15000),
	linkdead_timer(RuleI(Zone,ClientLinkdeadMS)),
	dead_timer(2000),
	global_channel_timer(1000),
	fishing_timer(8000),
	autosave_timer(RuleI(Character, AutosaveIntervalS) * 1000),
	scanarea_timer(RuleI(Aggro, ClientAggroCheckInterval) * 1000),
	proximity_timer(ClientProximity_interval),
	charm_class_attacks_timer(3000),
	charm_cast_timer(3500),
	qglobal_purge_timer(30000),
	TrackingTimer(2000),
	ItemTickTimer(10000),
	ItemQuestTimer(500),
	anon_toggle_timer(250),
	afk_toggle_timer(250),
	helm_toggle_timer(250),
	trade_timer(3000),
	door_check_timer(1000),
	mend_reset_timer(60000),
	underwater_timer(1000),
	zoning_timer(5000),
	instance_boot_grace_timer(RuleI(Quarm, ClientInstanceBootGraceMS)),
	m_Proximity(FLT_MAX, FLT_MAX, FLT_MAX), //arbitrary large number
	m_ZoneSummonLocation(-2.0f,-2.0f,-2.0f,-2.0f),
	m_AutoAttackPosition(0.0f, 0.0f, 0.0f, 0.0f),
	m_AutoAttackTargetLocation(0.0f, 0.0f, 0.0f)
{
	for(int cf=0; cf < _FilterCount; cf++)
		ClientFilters[cf] = FilterShow;
	
	for (int aa_ix = 0; aa_ix < MAX_PP_AA_ARRAY; aa_ix++) { aa[aa_ix] = nullptr; }
	
	character_id = 0;
	zoneentry = nullptr;
	conn_state = NoPacketsReceived;
	mMovementManager->AddClient(this);
	client_data_loaded = false;
	feigned = false;
	berserk = false;
	dead = false;
	initial_z_position = 0;
	accidentalfall_timer.Disable();
	is_client_moving = false;
	SetDevToolsEnabled(true);
	eqs = ieqs;
	ip = eqs->GetRemoteIP();
	port = ntohs(eqs->GetRemotePort());
	client_state = CLIENT_CONNECTING;
	Trader=false;
	WithCustomer = false;
	TraderSession = 0;
	WID = 0;
	gm_grid = nullptr;
	account_id = 0;
	admin = AccountStatus::Player;
	lsaccountid = 0;
	shield_target = nullptr;
	SQL_log = nullptr;
	guild_id = GUILD_NONE;
	guildrank = 0;
	memset(lskey, 0, sizeof(lskey));
	strcpy(account_name, "");
	prev_last_login_time = 0;
	tellsoff = false;
	last_reported_mana = 0;
	gmhideme = false;
	AFK = false;
	LFG = false;
	gmspeed = 0;
	gminvul = false;
	playeraction = 0;
	SetTarget(0);
	auto_attack = false;
	auto_fire = false;
	runmode = true;
	linkdead_timer.Disable();
	zonesummon_id = 0;
	zonesummon_guildid = GUILD_NONE;
	zonesummon_ignorerestrictions = 0;
	zoning = false;
	m_lock_save_position = false;
	zone_mode = ZoneUnsolicited;
	casting_spell_id = 0;
	npcflag = false;
	npclevel = 0;
	pQueuedSaveWorkID = 0;
	position_timer_counter = 0;
	fishing_timer.Disable();
	shield_timer.Disable();
	dead_timer.Disable();
	camp_timer.Disable();
	autosave_timer.Disable();
	door_check_timer.Disable();
	mend_reset_timer.Disable();
	zoning_timer.Disable();
	instance_boot_grace_timer.Disable();
	instalog = false;
	m_pp.autosplit = false;
	// initialise haste variable
	m_tradeskill_object = nullptr;
	PendingRezzXP = -1;
	PendingRezzZoneID = 0;
	PendingRezzZoneGuildID = 0;
	PendingRezzDBID = 0;
	PendingRezzSpellID = 0;
	numclients++;
	// emuerror;
	UpdateWindowTitle(nullptr);
	horseId = 0;
	tgb = false;
	keyring.clear();
	bind_sight_target = nullptr;
	clickyspellrecovery_burst = 0;
	pending_marriage_character_id = 0;

	//for good measure:
	memset(&m_pp, 0, sizeof(m_pp));
	memset(&m_epp, 0, sizeof(m_epp));
	PendingTranslocate = false;
	PendingSacrifice = false;
	BoatID = 0;

	KarmaUpdateTimer = new Timer(RuleI(Chat, KarmaUpdateIntervalMS));
	GlobalChatLimiterTimer = new Timer(RuleI(Chat, IntervalDurationMS));
	AttemptedMessages = 0;
	TotalKarma = 0;
	HackCount = 0;
	last_position_update_time = std::chrono::high_resolution_clock::now();
	exemptHackCount = false;
	ExpectedRewindPos = glm::vec3();
	m_ClientVersion = EQ::versions::Unknown;
	m_ClientVersionBit = 0;
	AggroCount = 0;

	m_TimeSinceLastPositionCheck = 0;
	m_DistanceSinceLastPositionCheck = 0.0f;
	m_ShadowStepExemption = 0;
	m_KnockBackExemption = 0;
	m_PortExemption = 0;
	m_SenseExemption = 0;
	m_AssistExemption = 0;
	m_CheatDetectMoved = false;
	CanUseReport = true;
	aa_los_them_mob = nullptr;
	los_status = false;
	los_status_facing = false;
	qGlobals = nullptr;
	HideCorpseMode = HideCorpseNone;
	PendingGuildInvitation = false;

	InitializeBuffSlots();

	LoadAccountFlags();

	initial_respawn_selection = 0;

	interrogateinv_flag = false;

	has_zomm = false;
	client_position_update = false;
	ignore_zone_count = false;
	last_target = 0;
	clicky_override = false;
	gm_guild_id = 0;	
	active_disc = 0;
	active_disc_spell = 0;
	trapid = 0;
	last_combine = 0;
	combine_interval[0] = 9999;
	combine_count = 0;
	last_search = 0;
	search_interval[0] = 9999;
	search_count = 0;
	last_forage = 0;
	forage_interval[0] = 9999;
	forage_count = 0;

	for (int i = 0; i < 8; i++) {
		spell_slot[i] = 0;
		spell_interval[i][0] = 999999;
		last_spell[i] = 0;
		spell_count[i] = 0;
	}
	last_who = 0;
	who_interval[0] = 9999;
	who_count = 0;
	last_friends = 0;
	friends_interval[0] = 9999;
	friends_count = 0;
	last_fish = 0;
	fish_interval[0] = 9999;
	fish_count = 0;
	last_say = 0;
	say_interval[0] = 9999;
	say_count = 0;
	last_pet = 0;
	pet_interval[0] = 9999;
	pet_count = 0;
	rested = false;
	camping = false;
	camp_desktop = false;
	food_hp = 0;
	drink_hp = 0;
	poison_spell_id = 0;
	drowning = false;
	pClientSideTarget = 0;

	if (!zone->CanDoCombat())
	{
		scanarea_timer.Disable();
	}

	helmcolor = 0;
	chestcolor = 0;
	armcolor = 0;
	bracercolor = 0;
	handcolor = 0;
	legcolor = 0;
	feetcolor = 0;
	pc_helmtexture = -1;
	pc_chesttexture = -1;
	pc_armtexture = -1;
	pc_bracertexture = -1;
	pc_handtexture = -1;
	pc_legtexture = -1;
	pc_feettexture = -1;

	wake_corpse_id = 0;
	ranged_attack_leeway_timer.Disable();
	last_fatigue = 0;
	mule_initiated = false;
}

Client::~Client() {
	SendAllPackets();
	mMovementManager->RemoveClient(this);

	Mob* horse = entity_list.GetMob(this->CastToClient()->GetHorseId());
	if (horse)
		horse->Depop();

	if(Trader)
		database.DeleteTraderItem(this->CharacterID());

	if(conn_state != ClientConnectFinished) {
		Log(Logs::General, Logs::None, "Client '%s' was destroyed before reaching the connected state:", GetName());
		ReportConnectingState();
	}

	if(m_tradeskill_object != nullptr) {
		m_tradeskill_object->Close();
		m_tradeskill_object = nullptr;
	}

	if(IsDueling() && GetDuelTarget() != 0) {
		Entity* entity = entity_list.GetID(GetDuelTarget());
		if(entity != nullptr && entity->IsClient()) {
			entity->CastToClient()->SetDueling(false);
			entity->CastToClient()->SetDuelTarget(0);
			entity_list.DuelMessage(entity->CastToClient(),this,true);
		}
	}

	if(GetTarget())
		GetTarget()->IsTargeted(-1);

	//if we are in a group and we are not zoning, force leave the group
	if(isgrouped && !zoning && is_zone_loaded)
		LeaveGroup();

	Raid *myraid = entity_list.GetRaidByClient(this);
	if (myraid && !zoning && is_zone_loaded)
		myraid->DisbandRaidMember(GetName());

	UpdateWho(2);

	// we save right now, because the client might be zoning and the world
	// will need this data right away
	Save(2); // This fails when database destructor is called first on shutdown

	safe_delete(KarmaUpdateTimer);
	safe_delete(GlobalChatLimiterTimer);
	safe_delete(qGlobals);

	DepopPet();
	numclients--;
	UpdateWindowTitle(nullptr);
	if (zone) {
		zone->RemoveAuth(GetName(), GetID());
	}

	//let the stream factory know were done with this stream
	eqs->Close();
	eqs->ReleaseFromUse();

	UninitializeBuffSlots();

	if (zoneentry != nullptr)
		safe_delete(zoneentry);

	for (auto &it : corpse_summon_timers)
	{
		Timer *timer = it.second;
		if (timer)
		{
			safe_delete(timer);
		}
	}
	corpse_summon_timers.clear();
}

void Client::SendLogoutPackets() {

	auto outapp = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
	CancelTrade_Struct* ct = (CancelTrade_Struct*) outapp->pBuffer;
	ct->fromid = GetID();
	ct->action = groupActUpdate;
	FastQueuePacket(&outapp);
}

void Client::SendCancelTrade(Mob* with) {

	auto outapp = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
	CancelTrade_Struct* ct = (CancelTrade_Struct*) outapp->pBuffer;
	ct->fromid = with->GetID();
	FastQueuePacket(&outapp);

	outapp = new EQApplicationPacket(OP_TradeReset, 0);
	QueuePacket(outapp);
	safe_delete(outapp);

	FinishTrade(this);
	trade->Reset();
}

void Client::ReportConnectingState() {
	switch(conn_state) {
	case NoPacketsReceived:		//havent gotten anything
		Log(Logs::General, Logs::Status, "Client has not sent us an initial zone entry packet.");
		break;
	case ReceivedZoneEntry:		//got the first packet, loading up PP
		Log(Logs::General, Logs::Status, "Client sent initial zone packet, but we never got their player info from the database.");
		break;
	case PlayerProfileLoaded:	//our DB work is done, sending it
		Log(Logs::General, Logs::Status, "We were sending the player profile, spawns, time and weather, but never finished.");
		break;
	case ZoneInfoSent:		//includes PP, spawns, time and weather
		Log(Logs::General, Logs::Status, "We successfully sent player info and spawns, waiting for client to request new zone.");
		break;
	case NewZoneRequested:	//received and sent new zone request
		Log(Logs::General, Logs::Status, "We received client's new zone request, waiting for client spawn request.");
		break;
	case ClientSpawnRequested:	//client sent ReqClientSpawn
		Log(Logs::General, Logs::Status, "We received the client spawn request, and were sending objects, doors, zone points and some other stuff, but never finished.");
		break;
	case ZoneContentsSent:		//objects, doors, zone points
		Log(Logs::General, Logs::Status, "The rest of the zone contents were successfully sent, waiting for client ready notification.");
		break;
	case ClientReadyReceived:	//client told us its ready, send them a bunch of crap like guild MOTD, etc
		Log(Logs::General, Logs::Status, "We received client ready notification, but never finished Client::CompleteConnect");
		break;
	case ClientConnectFinished:	//client finally moved to finished state, were done here
		Log(Logs::General, Logs::Status, "Client is successfully connected.");
		break;
	};
}

bool Client::SaveAA(){
	int first_entry = 0;
	std::string rquery;
	/* Save Player AA */
	int spentpoints = 0;
	for (int a = 0; a < MAX_PP_AA_ARRAY; a++) {
		uint32 points = aa[a]->value;
		if (points > HIGHEST_AA_VALUE) {
			aa[a]->value = HIGHEST_AA_VALUE;
			points = HIGHEST_AA_VALUE;
		}
		if (points > 0) {
			SendAA_Struct* curAA = zone->FindAA(aa[a]->AA - aa[a]->value + 1, false);
			if (curAA) {
				for (int rank = 0; rank<points; rank++) {
					spentpoints += (curAA->cost + (curAA->cost_inc * rank));
				}
			}
		}
	}
	m_pp.aapoints_spent = spentpoints + m_epp.expended_aa;
	for (int a = 0; a < MAX_PP_AA_ARRAY; a++) {
		if (aa[a]->AA > 0 && aa[a]->value){
			if (first_entry != 1){
				rquery = StringFormat("REPLACE INTO `character_alternate_abilities` (id, slot, aa_id, aa_value)"
					" VALUES (%u, %u, %u, %u)", character_id, a, aa[a]->AA, aa[a]->value);
				first_entry = 1;
			}
			rquery = rquery + StringFormat(", (%u, %u, %u, %u)", character_id, a, aa[a]->AA, aa[a]->value);
		}
	}
	auto results = database.QueryDatabase(rquery);
	return true;
}

bool Client::Save(uint8 iCommitNow) {
	if(!ClientDataLoaded())
		return false;

	/* Wrote current basics to PP for saves */
	if (!m_lock_save_position) {
		m_pp.x = floorf(m_Position.x);
		m_pp.y = floorf(m_Position.y);
		m_pp.z = m_Position.z;
		m_pp.heading = m_Position.w * 2.0f;
	}

	m_pp.guildrank = guildrank;

	/* Mana and HP */

	if (GetHP() <= -100)
	{
		m_pp.cur_hp = GetMaxHP();
	}
	else if (GetHP() <= 0)
	{
		m_pp.cur_hp = 1;
	}
	else 
	{
		m_pp.cur_hp = GetHP();
	}

	m_pp.mana = cur_mana;

	/* Save Character Currency */
	database.SaveCharacterCurrency(CharacterID(), &m_pp);

	// save character binds
	// this may not need to be called in Save() but it's here for now
	// to maintain the current behavior
	database.SaveCharacterBinds(this);

	/* Save Character Buffs */
	database.SaveBuffs(this);

	/* Total Time Played */
	TotalSecondsPlayed += (time(nullptr) - m_pp.lastlogin);
	m_pp.timePlayedMin = (TotalSecondsPlayed / 60);
	m_pp.lastlogin = time(nullptr);

	// we don't reload the pet data for TAKP so don't really need this
	/*
	if (GetPet() && !GetPet()->IsFamiliar() && GetPet()->CastToNPC()->GetPetSpellID() && !dead) {
		NPC *pet = GetPet()->CastToNPC();
		m_petinfo.SpellID = pet->CastToNPC()->GetPetSpellID();
		m_petinfo.HP = pet->GetHP();
		m_petinfo.Mana = pet->GetMana();
		pet->GetPetState(m_petinfo.Buffs, m_petinfo.Items, m_petinfo.Name);
		m_petinfo.petpower = pet->GetPetPower();
		m_petinfo.size = pet->GetSize();
	} else {
		memset(&m_petinfo, 0, sizeof(struct PetInfo));
	}
	database.SavePetInfo(this);
	*/

	p_timers.Store(&database);

	m_pp.hunger_level = EQ::Clamp(m_pp.hunger_level, (int16)0, (int16)32000);
	m_pp.thirst_level = EQ::Clamp(m_pp.thirst_level, (int16)0, (int16)32000);
	database.SaveCharacterData(this->CharacterID(), this->AccountID(), &m_pp, &m_epp); /* Save Character Data */

	return true;
}

void Client::SendSound(uint16 soundID)
{
	//Create the packet
	auto outapp = new EQApplicationPacket (OP_PlaySound, sizeof(Sound_Struct));
	Sound_Struct *ps = (Sound_Struct *) outapp->pBuffer;

	//Define the packet
	ps->entityid = GetID();
	ps->sound_number = soundID;

	//Send the packet
	QueuePacket(outapp);
	safe_delete(outapp);
}

CLIENTPACKET::CLIENTPACKET()
{
	app = nullptr;
	ack_req = false;
}

CLIENTPACKET::~CLIENTPACKET()
{
	safe_delete(app);
}

//this assumes we do not own pApp, and clones it.
bool Client::AddPacket(const EQApplicationPacket *pApp, bool bAckreq) {
	if (!pApp)
		return false;
	if(!zoneinpacket_timer.Enabled()) {
		//drop the packet because it will never get sent.
		return(false);
	}
	auto c = new CLIENTPACKET;

	c->ack_req = bAckreq;
	c->app = pApp->Copy();

	clientpackets.push_back(c);
	return true;
}

//this assumes that it owns the object pointed to by *pApp
bool Client::AddPacket(EQApplicationPacket** pApp, bool bAckreq) {
	if (!pApp || !(*pApp))
		return false;
	if(!zoneinpacket_timer.Enabled()) {
		//drop the packet because it will never get sent.
		if (pApp && (*pApp))
			delete *pApp;
		return(false);
	}
	auto c = new CLIENTPACKET;

	c->ack_req = bAckreq;
	c->app = *pApp;
	*pApp = nullptr;

	clientpackets.push_back(c);
	return true;
}

bool Client::SendAllPackets() {
	std::deque<CLIENTPACKET*>::iterator iterator;
	if (clientpackets.size() == 0)
		return false;
	CLIENTPACKET* cp = nullptr;
	iterator = clientpackets.begin();
	while(iterator != clientpackets.end()) {
		cp = (*iterator);
		if(eqs)
			eqs->FastQueuePacket((EQApplicationPacket **)&cp->app, cp->ack_req);
		iterator = clientpackets.erase(iterator);
		safe_delete(cp);
		Log(Logs::Detail, Logs::PacketServerClient, "Transmitting a packet");
	}
	return true;
}

void Client::QueuePacket(const EQApplicationPacket* app, bool ack_req, CLIENT_CONN_STATUS required_state, eqFilterType filter) {
	if(filter!=FilterNone){
		//this is incomplete... no support for FilterShowGroupOnly or FilterShowSelfOnly
		if(GetFilter(filter) == FilterHide)
			return; //Client has this filter on, no need to send packet
	}

	if(client_state == PREDISCONNECTED)
		return;

	if(client_state != CLIENT_CONNECTED && required_state == CLIENT_CONNECTED){
		// save packets during connection state
		AddPacket(app, ack_req);
		return;
	}

	//Wait for the queue to catch up - THEN send the first available predisconnected (zonechange) packet!
	if (client_state == ZONING && required_state != ZONING)
	{
		// save packets in case this fails
		AddPacket(app, ack_req);
		return;
	}

	// if the program doesnt care about the status or if the status isnt what we requested
	if (required_state != CLIENT_CONNECTINGALL && client_state != required_state)
	{
		// todo: save packets for later use
		AddPacket(app, ack_req);
	}
	else
		if(eqs)
			eqs->QueuePacket(app, ack_req);
}

void Client::FastQueuePacket(EQApplicationPacket** app, bool ack_req, CLIENT_CONN_STATUS required_state) {
	// if the program doesnt care about the status or if the status isnt what we requested


	if(client_state == PREDISCONNECTED)
	{
		if (app && (*app))
			delete *app;
		return;
	}

		//Wait for the queue to catch up - THEN send the first available predisconnected (zonechange) packet!
	if (client_state == ZONING && required_state != ZONING)
	{
		// save packets in case this fails
		AddPacket(app, ack_req);
		return;
	}

	if (required_state != CLIENT_CONNECTINGALL && client_state != required_state) {
		// todo: save packets for later use
		AddPacket(app, ack_req);
		return;
	}
	else if (app != nullptr && *app != nullptr)
	{
		if(eqs)
			eqs->FastQueuePacket((EQApplicationPacket **)app, ack_req);
		else if (app && (*app))
			delete *app;
		*app = nullptr;
	}
	return;
}

void Client::ChannelMessageReceived(uint8 chan_num, uint8 language, uint8 lang_skill, const char* orig_message, const char* targetname)
{
	char message[4096];
	strn0cpy(message, orig_message, sizeof(message));

	Log(Logs::Detail, Logs::ZoneServer, "Client::ChannelMessageReceived() Channel:%i message:'%s'", chan_num, message);

	if (targetname == nullptr) {
		targetname = (!GetTarget()) ? "" : GetTarget()->GetName();
	}

	if(RuleB(Chat, EnableAntiSpam))
	{
		if(strcmp(targetname, "discard") != 0)
		{
			if(chan_num == ChatChannel_Shout || chan_num == ChatChannel_Auction || chan_num == ChatChannel_OOC || chan_num == ChatChannel_Tell)
			{
				if(GlobalChatLimiterTimer)
				{
					if(GlobalChatLimiterTimer->Check(false))
					{
						GlobalChatLimiterTimer->Start(RuleI(Chat, IntervalDurationMS));
						AttemptedMessages = 0;
					}
				}

				uint32 AllowedMessages = RuleI(Chat, MinimumMessagesPerInterval) + TotalKarma;
				AllowedMessages = AllowedMessages > RuleI(Chat, MaximumMessagesPerInterval) ? RuleI(Chat, MaximumMessagesPerInterval) : AllowedMessages;

				if(RuleI(Chat, MinStatusToBypassAntiSpam) <= Admin())
					AllowedMessages = 10000;

				AttemptedMessages++;
				if(AttemptedMessages > AllowedMessages)
				{
					if(AttemptedMessages > RuleI(Chat, MaxMessagesBeforeKick))
					{
						RevokeSelf();
						return;
					}
					if(GlobalChatLimiterTimer)
					{
						Message(Chat::White, "You have been rate limited, you can send more messages in %i seconds.",
							GlobalChatLimiterTimer->GetRemainingTime() / 1000);
						return;
					}
					else
					{
						Message(Chat::White, "You have been rate limited, you can send more messages in 60 seconds.");
						return;
					}
				}
			}
		}
	}

	/* Logs Player Chat */
	if (RuleB(QueryServ, PlayerLogChat)) {
		auto pack = new ServerPacket(ServerOP_Speech, sizeof(Server_Speech_Struct) + strlen(message) + 1);
		Server_Speech_Struct* sem = (Server_Speech_Struct*) pack->pBuffer;

		if(chan_num == ChatChannel_Guild)
			sem->guilddbid = GuildID();
		else
			sem->guilddbid = 0;

		auto group = GetGroup();
		if (group && chan_num == ChatChannel_Group) {
			sem->groupid = group->GetID();
			std::string groupMembers = group->GetMemberNamesAsCsv({ GetName() });
			strncpy(sem->to, groupMembers.c_str(), 325);
			sem->to[324] = 0;
		}
		else {
			sem->groupid = 0;
		}

		sem->characterid = CharacterID();

		strcpy(sem->message, message);
		sem->minstatus = this->Admin();
		sem->type = chan_num;

		// => to: preserve the targetname if it's not empty
		//     => tells for example will have a targetname already
		// => to: use the guild name if the channel is guild
		// => to: use the zone short name if the channel is auction, ooc, or shout
		// => to: use the target name if the channel is not guild, auction, ooc, shout, broadcast, raid, or petition
		bool targetNameIsEmpty = targetname == nullptr || strlen(targetname) == 0;
		char logTargetName[64] = { 0 };
		if (targetNameIsEmpty) {
			switch (chan_num) {
				case ChatChannel_Guild: {
					std::string guildName = GetGuildName();
					const char* temp = guildName.c_str();
					strncpy(logTargetName, temp, 64);
					logTargetName[63] = 0;
				}
				break;

				case ChatChannel_Auction:
				case ChatChannel_OOC:
				case ChatChannel_Shout: {
					if (zone) {
						const char* temp = zone->GetShortName();
						strncpy(logTargetName, temp, 64);
						logTargetName[63] = 0;
					}
				}
				break;

				case ChatChannel_Broadcast:
				case ChatChannel_Raid:
				case ChatChannel_Petition:
					break;

				default: {
					const char* temp = (!GetTarget()) ? "" : GetTarget()->GetName();
					strncpy(logTargetName, temp, 64);
					logTargetName[63] = 0;
				}
				break;
			}
		}
		else {
			strncpy(logTargetName, targetname, 64);
			logTargetName[63] = 0;
		}

		// => if groupid is not 0, sem->to has already been filled with the group members
		if (sem->groupid == 0 && strlen(logTargetName) != 0)
		{
			strncpy(sem->to, logTargetName, 64);
			sem->to[63] = 0;
		}
		// ----------------------------------------------

		if (GetName() != 0)
		{
			strncpy(sem->from, GetName(), 64);
			sem->from[63] = 0;
		}

		pack->Deflate();
		if(worldserver.Connected())
			worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	// Garble the message based on drunkness, except for OOC and GM
	if (m_pp.intoxication > 0 && !(RuleB(Chat, ServerWideOOC) && chan_num == ChatChannel_OOC) && !GetGM()) {
		GarbleMessage(message, (int)(m_pp.intoxication / 3));
		language = 0; // No need for language when drunk
		lang_skill = 100;
	}

	// some channels don't use languages
	if (chan_num == ChatChannel_OOC || chan_num == ChatChannel_GMSAY || chan_num == ChatChannel_Broadcast || chan_num == ChatChannel_Petition)
	{
		language = 0;
		lang_skill = 100;
	}

	switch(chan_num)
	{
	case ChatChannel_Guild: { /* Guild Chat */
		if (GetRevoked() == 2)
		{
			Message(Chat::Default, "You have been muted. You may not talk on Guild.");
			return;
		}

		if (!IsInAGuild())
			Message_StringID(Chat::DefaultText, GUILD_NOT_MEMBER2);	//You are not a member of any guild.
		else if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_SPEAK))
			Message(Chat::White, "Error: You dont have permission to speak to the guild.");
		else if (!worldserver.SendChannelMessage(this, targetname, chan_num, GuildID(), language, lang_skill, message))
			Message(Chat::White, "Error: World server disconnected");
		break;
	}
	case ChatChannel_Group: { /* Group Chat */

		if (GetRevoked() == 2)
		{
			Message(Chat::Default, "You have been muted. You may not talk on Group.");
			return;
		}
		Raid* raid = entity_list.GetRaidByClient(this);
		if(raid) {
			raid->RaidGroupSay((const char*) message, this, language, lang_skill);
			break;
		}

		Group* group = GetGroup();
		if(group != nullptr) {
			group->GroupMessage(this, language, lang_skill, (const char*) message);
		}
		break;
	}
	case ChatChannel_Raid: { /* Raid Say */
		Raid* raid = entity_list.GetRaidByClient(this);
		if (GetRevoked() == 2)
		{
			Message(Chat::Default, "You have been muted. You may not talk on Raid Say.");
			return;
		}

		if(raid){
			raid->RaidSay((const char*) message, this, language, lang_skill);
		}
		break;
	}
	case ChatChannel_Shout: { /* Shout */
		Mob *sender = this;
		if (GetRevoked())
		{
			Message(Chat::Default, "You have been muted. You may not talk on Shout.");
			return;
		}
		entity_list.ChannelMessage(sender, chan_num, language, lang_skill, message);
		break;
	}
	case ChatChannel_Auction: { /* Auction */
		if(RuleB(Chat, ServerWideAuction))
		{
			if(!global_channel_timer.Check())
			{
				if(strlen(targetname) == 0)
					ChannelMessageReceived(chan_num, language, lang_skill, message, "discard"); //Fast typer or spammer??
				else
					return;
			}

			if(GetRevoked())
			{
				Message(Chat::White, "You have been revoked. You may not talk on Auction.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, KarmaGlobalChatLevelLimit) && !this->IsMule())
				{
					Message(Chat::White, "You do not have permission to talk in Auction at this time.");
					return;
				}
			}

			if (!worldserver.SendChannelMessage(this, 0, chan_num, 0, language, lang_skill, message))
				Message(Chat::White, "Error: World server disconnected");
		}
		else if(!RuleB(Chat, ServerWideAuction)) {
			Mob *sender = this;

			if (GetRevoked())
			{
				Message(Chat::Default, "You have been revoked. You may not send Auction messages.");
				return;
			}

			entity_list.ChannelMessage(sender, chan_num, language, lang_skill, message);
		}
		break;
	}
	case ChatChannel_OOC: { /* OOC */
		if(RuleB(Chat, ServerWideOOC))
		{
			if(!global_channel_timer.Check())
			{
				if(strlen(targetname) == 0)
					ChannelMessageReceived(chan_num, language, lang_skill, message, "discard"); //Fast typer or spammer??
				else
					return;
			}
			if(worldserver.IsOOCMuted() && admin < AccountStatus::GMAdmin)
			{
				Message(Chat::White,"OOC has been muted. Try again later.");
				return;
			}

			if(GetRevoked())
			{
				Message(Chat::White, "You have been revoked. You may not talk on OOC.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, KarmaGlobalChatLevelLimit) && !this->IsMule())
				{
					Message(Chat::White, "You do not have permission to talk in OOC at this time.");
					return;
				}
			}

			if (!worldserver.SendChannelMessage(this, 0, chan_num, 0, language, lang_skill, message))
				Message(Chat::White, "Error: World server disconnected");
		}
		else
		{
			Mob *sender = this;
			if (GetRevoked())
			{
				Message(Chat::Default, "You have been revoked. You may not talk in Out Of Character.");
				return;
			}

			entity_list.ChannelMessage(sender, chan_num, language, lang_skill, message);
		}
		break;
	}
	case ChatChannel_Broadcast: /* Broadcast */
	case ChatChannel_GMSAY: { /* GM Say */
		if (!(admin >= AccountStatus::QuestTroupe))
			Message(Chat::White, "Error: Only GMs can use this channel");
		else if (!worldserver.SendChannelMessage(this, targetname, chan_num, 0, language, lang_skill, message))
			Message(Chat::White, "Error: World server disconnected");
		break;
	}
	case ChatChannel_Tell: { /* Tell */

			if (GetLevel() < RuleI(Chat, GlobalChatLevelLimit) && !this->IsMule())
			{
				Message(Chat::White, "You do not have permission to send tells until level %i.", RuleI(Chat, GlobalChatLevelLimit));
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, KarmaGlobalChatLevelLimit) && !this->IsMule())
				{
					Message(Chat::White, "You do not have permission to send tells.");
					return;
				}
			}

			// allow tells to corpses
			if (targetname) {
				if (GetTarget() && GetTarget()->IsCorpse() && GetTarget()->CastToCorpse()->IsPlayerCorpse()) {
					if (strcasecmp(targetname,GetTarget()->CastToCorpse()->GetName()) == 0) {
						if (strcasecmp(GetTarget()->CastToCorpse()->GetOwnerName(),GetName()) == 0) {
							Message_StringID(Chat::DefaultText, TALKING_TO_SELF);
							return;
						} else {
							targetname = GetTarget()->CastToCorpse()->GetOwnerName();
						}
					}
				}
			}

			char target_name[64] = {};

			if(targetname)
			{
				size_t i = strlen(targetname);
				int x;
				for(x = 0; x < i; ++x)
				{
					if(targetname[x] == '%')
					{
						target_name[x] = '/';
					}
					else
					{
						target_name[x] = targetname[x];
					}
				}
				target_name[x] = '\0';
			}

			if(!worldserver.SendChannelMessage(this, target_name, chan_num, 0, language, lang_skill, message))
				Message(Chat::White, "Error: World server disconnected");
		break;
	}
	case ChatChannel_Say: { /* Say */
		if(message[0] == COMMAND_CHAR) {
			if (command_dispatch(this, message) == -2) {
				if(parse->PlayerHasQuestSub(EVENT_COMMAND)) {
					int i = parse->EventPlayer(EVENT_COMMAND, this, message, 0);
					if(i == 0 && !RuleB(Chat, SuppressCommandErrors)) {
						Message(Chat::Red, "Command '%s' not recognized.", message);
					}
				} else {
					if(!RuleB(Chat, SuppressCommandErrors))
						Message(Chat::Red, "Command '%s' not recognized.", message);
				}
			}
			break;
		}

		Mob* sender = this;
		if (GetPet() && FindType(SE_VoiceGraft))
			sender = GetPet();
		if (GetRevoked())
		{
			Message(Chat::Default, "You have been revoked. You may not talk on say, except to NPCs directly.");
		}
		else
		{
			entity_list.ChannelMessage(sender, chan_num, language, lang_skill, message);
		}
		parse->EventPlayer(EVENT_SAY, this, message, language);

		if (sender != this)
			break;

		if(quest_manager.ProximitySayInUse())
			entity_list.ProcessProximitySay(message, this, language);

		if (GetTarget() != 0 && GetTarget()->IsNPC()) 
		{
			if(!GetTarget()->CastToNPC()->IsEngaged()) 
			{
				NPC *tar = GetTarget()->CastToNPC();
				if(DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition()) <= RuleI(Range, EventSay) && (sneaking || !IsInvisible(tar)))
				{
					CheckEmoteHail(GetTarget(), message);
					parse->EventNPC(EVENT_SAY, tar->CastToNPC(), this, message, language);
				}
			}
			else 
			{
				if (DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition()) <= RuleI(Range, EventAggroSay)) 
				{
					parse->EventNPC(EVENT_AGGRO_SAY, GetTarget()->CastToNPC(), this, message, language);
				}
			}

		}
		break;
	}
	default: {
		Message(Chat::White, "Channel (%i) not implemented", (uint16)chan_num);
	}
	}
}

void Client::ChannelMessageSend(const char* from, const char* to, uint8 chan_num, uint8 language, uint8 lang_skill, const char* message, ...) {
	if ((chan_num==ChatChannel_GMSAY && !(this->GetGM())) || (chan_num==ChatChannel_Petition && this->Admin() < AccountStatus::QuestTroupe)) // dont need to send /pr & /petition to everybody
		return;

	if (!Connected())
		return;

	char message_sender[64];

	EQApplicationPacket app(OP_ChannelMessage, sizeof(ChannelMessage_Struct)+strlen(message)+1);
	ChannelMessage_Struct* cm = (ChannelMessage_Struct*)app.pBuffer;

	if (from == 0 || from[0] == 0)
		strcpy(cm->sender, "ZServer");
	else {
		CleanMobName(from, message_sender);
		strcpy(cm->sender, message_sender);
	}
	if (to != 0)
		strcpy((char *) cm->targetname, to);
	else if (chan_num == ChatChannel_Tell)
		strcpy(cm->targetname, m_pp.name);
	else
		cm->targetname[0] = 0;

	language = language < MAX_PP_LANGUAGE ? language: 0;
	lang_skill = lang_skill <= 100 ? lang_skill : 100;

	cm->language = language;
	cm->skill_in_language = lang_skill;
	cm->chan_num = chan_num;
	strcpy(&cm->message[0], message);
	QueuePacket(&app);

	if ((chan_num == ChatChannel_Group) && (m_pp.languages[language] < 100)) {	// group message in unmastered language, check for skill up
		if ((m_pp.languages[language] <= lang_skill) && (from != this->GetName()))
			CheckLanguageSkillIncrease(language, lang_skill);
	}
}

void Client::Message(uint32 type, const char* message, ...) {
	if (GetFilter(FilterMeleeCrits) == FilterHide && type == Chat::MeleeCrit) //98 is self...
		return;
	if (GetFilter(FilterSpellCrits) == FilterHide && type == Chat::SpellCrit)
		return;
	if (!Connected())
		return;

		va_list argptr;
		auto buffer = new char[4096];
		va_start(argptr, message);
		vsnprintf(buffer, 4096, message, argptr);
		va_end(argptr);

		size_t len = strlen(buffer);

		//client dosent like our packet all the time unless
		//we make it really big, then it seems to not care that
		//our header is malformed.
		//len = 4096 - sizeof(SpecialMesg_Struct);

		uint32 len_packet = sizeof(SpecialMesg_Struct)+len;
		auto app = new EQApplicationPacket(OP_SpecialMesg, len_packet);
		SpecialMesg_Struct* sm=(SpecialMesg_Struct*)app->pBuffer;
		sm->header[0] = 0x00; // Header used for #emote style messages..
		sm->header[1] = 0x00; // Play around with these to see other types
		sm->header[2] = 0x00;
		sm->msg_type = type;
		memcpy(sm->message, buffer, len+1);

		FastQueuePacket(&app);

		safe_delete_array(buffer);

}

void Client::SetMaxHP() {
	if(dead)
		return;
	SetHP(CalcMaxHP());
	SendHPUpdate();
	Save();
}

void Client::SetSkill(EQ::skills::SkillType skillid, uint16 value, bool silent) {
	if (skillid > EQ::skills::HIGHEST_SKILL)
		return;
	m_pp.skills[skillid] = value; // We need to be able to #setskill 254 and 255 to reset skills

	database.SaveCharacterSkill(this->CharacterID(), skillid, value);

	if (silent) 
	{
		// this packet doesn't print a message on the client
		auto outapp = new EQApplicationPacket(OP_SkillUpdate2, sizeof(SkillUpdate2_Struct));
		SkillUpdate2_Struct *pkt = (SkillUpdate2_Struct *)outapp->pBuffer;
		pkt->entity_id = GetID();
		pkt->skillId = skillid;
		pkt->value = value;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	else
	{
		// this packet prints a string: You have become better at %1! (%2)
		auto outapp = new EQApplicationPacket(OP_SkillUpdate, sizeof(SkillUpdate_Struct));
		SkillUpdate_Struct *skill = (SkillUpdate_Struct *)outapp->pBuffer;
		skill->skillId = skillid;
		skill->value = value;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::ResetSkill(EQ::skills::SkillType skillid, bool reset_timer) 
{
	// This only works for a few skills Mend being one of them.

	if (skillid > EQ::skills::HIGHEST_SKILL)
		return;

	if (!HasSkill(skillid))
		return;

	int16 timer = database.GetTimerFromSkill(skillid);

	// The skill is not an activated type.
	if (timer == INVALID_INDEX)
		return;

	if (reset_timer)
	{
		if (!p_timers.Expired(&database, timer))
		{
			p_timers.Clear(&database, timer);
		}
	}

	auto outapp = new EQApplicationPacket(OP_ResetSkill, sizeof(ResetSkill_Struct));
	ResetSkill_Struct* skill = (ResetSkill_Struct*)outapp->pBuffer;
	skill->skillid = skillid;
	skill->timer = 0;
	QueuePacket(outapp);
	safe_delete(outapp);

	Log(Logs::General, Logs::Skills, "Skill %d has been reset.", skillid);
}

void Client::ResetAllSkills()
{
	for (int i = 0; i < EQ::skills::SkillCount; ++i)
	{
		EQ::skills::SkillType skillid = static_cast<EQ::skills::SkillType>(i);
		ResetSkill(skillid, true);
	}
}

void Client::IncreaseLanguageSkill(int skill_id, int value) {

	if (skill_id >= MAX_PP_LANGUAGE)
		return; //Invalid lang id

	m_pp.languages[skill_id] += value;

	if (m_pp.languages[skill_id] > 100) //Lang skill above max
		m_pp.languages[skill_id] = 100;

	database.SaveCharacterLanguage(this->CharacterID(), skill_id, m_pp.languages[skill_id]);

	/* Takp/Mac client does nothing with updates for skills greater than 100 - so no reason to send
	auto outapp = new EQApplicationPacket(OP_SkillUpdate, sizeof(SkillUpdate_Struct));
	SkillUpdate_Struct* skill = (SkillUpdate_Struct*)outapp->pBuffer;
	skill->skillId = 100 + skill_id;
	skill->value = m_pp.languages[skill_id];
	QueuePacket(outapp);
	safe_delete(outapp);*/

	Message_StringID( Chat::Skills, LANG_SKILL_IMPROVED ); //Notify client
}

void Client::AddSkill(EQ::skills::SkillType skillid, uint16 value) {
	if (skillid > EQ::skills::HIGHEST_SKILL)
		return;
	value = GetRawSkill(skillid) + value;
	uint16 max = GetMaxSkillAfterSpecializationRules(skillid, MaxSkill(skillid));
	if (value > max)
		value = max;
	SetSkill(skillid, value);
}

void Client::UpdateWho(uint8 remove) {
	if (account_id == 0)
		return;
	if (!worldserver.Connected())
		return;
	auto pack = new ServerPacket(ServerOP_ClientList, sizeof(ServerClientList_Struct));
	ServerClientList_Struct* scl = (ServerClientList_Struct*) pack->pBuffer;
	scl->remove = remove;
	scl->wid = this->GetWID();
	scl->IP = this->GetIP();
	scl->charid = this->CharacterID();
	strcpy(scl->name, this->GetName());

	scl->gm = GetGM();
	scl->Admin = this->Admin();
	scl->AccountID = this->AccountID();
	strcpy(scl->AccountName, this->AccountName());
	scl->LSAccountID = this->LSAccountID();
	strn0cpy(scl->lskey, lskey, sizeof(scl->lskey));
	scl->zone = zone->GetZoneID();
	scl->zoneguildid = zone->GetGuildID();
	scl->race = this->GetRace();
	scl->class_ = GetClass();
	scl->level = GetLevel();
	if (m_pp.anon == 0)
		scl->anon = 0;
	else if (m_pp.anon == 1)
		scl->anon = 1;
	else if (m_pp.anon >= 2)
		scl->anon = 2;

	scl->ClientVersion = ClientVersion();
	scl->tellsoff = tellsoff;
	scl->guild_id = guild_id;
	scl->LFG = this->LFG;
	scl->LD = this->IsLD();
	scl->baserace = this->GetBaseRace();
	scl->mule = this->IsMule();
	scl->AFK = this->AFK;
	scl->Trader = this->IsTrader();
	scl->Revoked = this->GetRevoked();

	scl->selffound = this->IsSelfFound();
	scl->hardcore = this->IsHardcore();
	scl->solo = this->IsSoloOnly();

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Client::WhoAll(Who_All_Struct* whom) {

	if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {
		auto pack = new ServerPacket(ServerOP_Who, sizeof(ServerWhoAll_Struct));
		ServerWhoAll_Struct* whoall = (ServerWhoAll_Struct*) pack->pBuffer;
		whoall->admin = this->Admin();
		whoall->fromid=this->GetID();
		strcpy(whoall->from, this->GetName());
		strn0cpy(whoall->whom, whom->whom, 64);
		whoall->lvllow = whom->lvllow;
		whoall->lvlhigh = whom->lvlhigh;
		whoall->gmlookup = whom->gmlookup;
		whoall->wclass = whom->wclass;
		whoall->wrace = whom->wrace;
		whoall->guildid = whom->guildid;
		worldserver.SendPacket(pack);
		safe_delete(pack);

		Log(Logs::General, Logs::EQMac,"WhoAll filters: whom %s lvllow %d lvlhigh %d gm %d class %d race %d guild %d", whom->whom, whom->lvllow, whom->lvlhigh, whom->gmlookup, whom->wclass, whom->wrace, whom->guildid);
	}
}

void Client::FriendsWho(char *FriendsString) {

	if (!worldserver.Connected())
		Message(Chat::White, "Error: World server disconnected");
	else {
		auto pack =
		    new ServerPacket(ServerOP_FriendsWho, sizeof(ServerFriendsWho_Struct) + strlen(FriendsString));
		ServerFriendsWho_Struct* FriendsWho = (ServerFriendsWho_Struct*) pack->pBuffer;
		FriendsWho->FromID = this->GetID();
		strcpy(FriendsWho->FromName, GetName());
		strcpy(FriendsWho->FriendsString, FriendsString);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Client::UpdateGroupID(uint32 group_id) {
	// update database
	database.SetGroupID(GetName(), group_id, CharacterID(), AccountID());

	// send update to worldserver, to allow updating ClientListEntry
	auto pack = new ServerPacket(ServerOP_GroupSetID, sizeof(GroupSetID_Struct));
	GroupSetID_Struct* gsid = (GroupSetID_Struct*) pack->pBuffer;
	gsid->char_id = this->CharacterID();
	gsid->group_id = group_id;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Client::UpdateAdmin(bool iFromDB) {
	int16 tmp = admin;
	if (iFromDB)
		admin = database.CheckStatus(account_id);
	if (tmp == admin && iFromDB)
		return;

	if(m_pp.gm)
	{
		Log(Logs::Detail, Logs::ZoneServer, "%s - %s is a GM", __FUNCTION__ , GetName());
// no need for this, having it set in pp you already start as gm
// and it's also set in your spawn packet so other people see it too
//		SendAppearancePacket(AT_GM, 1, false);
		petition_list.UpdateGMQueue();
	}

	UpdateWho();
}

const int32& Client::SetMana(int32 amount) {
	bool update = false;
	if (amount < 0)
		amount = 0;
	if (amount > GetMaxMana())
		amount = GetMaxMana();
	if (amount != cur_mana)
		update = true;
	cur_mana = amount;
	if (update)
		Mob::SetMana(amount);
	SendManaUpdatePacket();
	return cur_mana;
}

void Client::SendManaUpdatePacket() {
	if (!Connected() || IsCasting())
		return;

	if (last_reported_mana != cur_mana) {

		SendManaUpdate();

		last_reported_mana = cur_mana;
	}
}

// sends mana update to self
void Client::SendManaUpdate()
{
	Log(Logs::Detail, Logs::Regen, "%s is setting mana to %d Sending out an update.", GetName(), GetMana());
	auto mana_app = new EQApplicationPacket(OP_ManaUpdate,sizeof(ManaUpdate_Struct));
	ManaUpdate_Struct* mus = (ManaUpdate_Struct*)mana_app->pBuffer;
	mus->spawn_id = GetID();
	mus->cur_mana = GetMana();
	FastQueuePacket(&mana_app);
}

void Client::SendStaminaUpdate()
{
	auto outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	sta->food = m_pp.hunger_level;
	sta->water = m_pp.thirst_level;
	sta->fatigue = m_pp.fatigue;
	//Message(MT_Broadcasts, "OP_Stamina hunger %d thirst %d fatigue %d timer duration %d remaining %d", (int)sta->food, (int)sta->water, (int)sta->fatigue, stamina_timer.GetDuration(), stamina_timer.GetRemainingTime());
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho)
{
	Mob::FillSpawnStruct(ns, ForWho);

	// Populate client-specific spawn information
	ns->spawn.afk		= AFK;
	ns->spawn.anon		= m_pp.anon;
	ns->spawn.gm		= GetGM() ? 1 : 0;
	ns->spawn.guildID	= GuildID();
	ns->spawn.lfg		= LFG;
//	ns->spawn.linkdead	= IsLD() ? 1 : 0;
	ns->spawn.pvp		= GetPVP() ? 1 : 0;
	ns->spawn.aa_title	= GetAARankTitle();

	if (IsBecomeNPC() == true)
		ns->spawn.NPC = 1;
	else if (ForWho == this)
		ns->spawn.NPC = 10;
	else
		ns->spawn.NPC = 0;
	ns->spawn.is_pet = 0;

	if (!IsInAGuild()) {
		ns->spawn.guildrank = 0xFF;
	} else {
		ns->spawn.guildrank = guild_mgr.GetDisplayedRank(GuildID(), GuildRank(), AccountID());
	}
	ns->spawn.size			= size;
	ns->spawn.runspeed		= (gmspeed == 0) ? runspeed : 3.1f;

	for (int i = 0; i < EQ::textures::materialCount; i++)
	{
		if (i < EQ::textures::weaponPrimary)
		{
			int16 texture = 0; uint32 color = 0;
			GetPCEquipMaterial(i, texture, color);
			ns->spawn.equipment[i] = texture;
			ns->spawn.colors.Slot[i].Color = color;
		}
		else
		{
			ns->spawn.equipment[i] = GetEquipmentMaterial(i);
		}
	}

	//these two may be related to ns->spawn.texture
	/*
	ns->spawn.npc_armor_graphic = texture;
	ns->spawn.npc_helm_graphic = helmtexture;
	*/

	//filling in some unknowns to make the client happy
//	ns->spawn.unknown0002[2] = 3;

	UpdateEquipmentLight();
	UpdateActiveLight();
	ns->spawn.light = m_Light.Type[EQ::lightsource::LightActive];
}

bool Client::GMHideMe(Client* client) {
	if (GetHideMe()) {
		if (client == 0)
			return true;
		else if (Admin() > client->Admin())
			return true;
		else
			return false;
	}
	else
		return false;
}

void Client::Duck() {
	playeraction = eaCrouching;
	SetAppearance(eaCrouching, false);
}

void Client::Stand() {
	playeraction = eaStanding;
	SetAppearance(eaStanding, false);
}

void Client::ChangeLastName(const char* in_lastname) {
	memset(m_pp.last_name, 0, sizeof(m_pp.last_name));
	strn0cpy(m_pp.last_name, in_lastname, sizeof(m_pp.last_name));
	auto outapp = new EQApplicationPacket(OP_GMLastName, sizeof(GMLastName_Struct));
	GMLastName_Struct* gmn = (GMLastName_Struct*)outapp->pBuffer;
	strcpy(gmn->name, name);
	strcpy(gmn->gmname, name);
	strcpy(gmn->lastname, in_lastname);
	gmn->unknown[0]=1;
	gmn->unknown[1]=1;
	gmn->unknown[2]=1;
	gmn->unknown[3]=1;
	entity_list.QueueClients(this, outapp, false);
	// Send name update packet here... once know what it is
	safe_delete(outapp);
}

void Client::SetTemporaryLastName(char* in_lastname) {
	if (!in_lastname)
	{
		return;
	}

	char *c = nullptr;
	bool first = true;
	for (c = in_lastname; *c; c++) {
		if (first) {
			*c = toupper(*c);
			first = false;
		}
		else if (*c == '`' || *c == '\'') { // if we find a backtick, don't modify the next character's capitalization
			// If this is the last character, we can break out of the loop
			if (*(c + 1) == '\0')
				break;

			c++; // Move to the next character
		}
		else {
			*c = tolower(*c);
		}
	}

	if (strlen(in_lastname) >= 20) {
		Message_StringID(Chat::Yellow, SURNAME_TOO_LONG);
		return;
	}


	if (in_lastname[0] != 0 && !database.CheckNameFilter(in_lastname, true))
	{
		Message_StringID(Chat::Red, SURNAME_REJECTED);
		return;
	}

	memset(m_epp.temp_last_name, 0, sizeof(m_epp.temp_last_name));
	strn0cpy(m_epp.temp_last_name, in_lastname, sizeof(m_epp.temp_last_name));
	memset(lastname, 0, sizeof(lastname));
	strcpy(lastname, m_epp.temp_last_name);
	auto outapp = new EQApplicationPacket(OP_GMLastName, sizeof(GMLastName_Struct));
	GMLastName_Struct* gmn = (GMLastName_Struct*)outapp->pBuffer;
	strcpy(gmn->name, name);
	strcpy(gmn->gmname, name);
	strcpy(gmn->lastname, in_lastname);
	gmn->unknown[0] = 1;
	gmn->unknown[1] = 1;
	gmn->unknown[2] = 1;
	gmn->unknown[3] = 1;
	entity_list.QueueClients(this, outapp, false);
	// Send name update packet here... once know what it is
	safe_delete(outapp);
}

bool Client::ChangeFirstName(const char* in_firstname, const char* gmname)
{
	// Check duplicate name. The name can be the same as our current name,
	// so we can change case.
	bool usedname = database.CheckUsedName((const char*) in_firstname, CharacterID());
	if (!usedname) {
		return false;
	}

	// update character_
	if(!database.UpdateName(GetName(), in_firstname))
		return false;

	// update pp
	memset(m_pp.name, 0, sizeof(m_pp.name));
	snprintf(m_pp.name, sizeof(m_pp.name), "%s", in_firstname);
	strcpy(name, m_pp.name);
	Save();

	// send name update packet
	auto outapp = new EQApplicationPacket(OP_GMNameChange, sizeof(GMName_Struct));
	GMName_Struct* gmn=(GMName_Struct*)outapp->pBuffer;
	strn0cpy(gmn->gmname,gmname,64);
	strn0cpy(gmn->oldname,GetName(),64);
	strn0cpy(gmn->newname,in_firstname,64);
	gmn->unknown[0] = 1;
	gmn->unknown[1] = 1;
	gmn->unknown[2] = 1;
	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);

	// finally, update the /who list
	UpdateWho();

	// success
	return true;
}

void Client::SetGM(bool toggle) {
	m_pp.gm = toggle ? 1 : 0;
	Message(Chat::Red, "You are %s a GM.", m_pp.gm ? "now" : "no longer");
	SendAppearancePacket(AppearanceType::GM, m_pp.gm);
	Save();
	UpdateWho();
}

void Client::SetAnon(bool toggle) {
	m_pp.anon = toggle ? 1 : 0;
	SendAppearancePacket(AppearanceType::Anonymous, m_pp.anon);
	Save();
	UpdateWho();
}

void Client::ReadBook(BookRequest_Struct *book) {
	char *txtfile = book->txtfile;

	if(txtfile[0] == '0' && txtfile[1] == '\0') {
		//invalid book... coming up on non-book items.
		return;
	}

	std::string booktxt2 = database.GetBook(txtfile);
	int length = booktxt2.length();

	if (booktxt2[0] != '\0') {
#if EQDEBUG >= 6
		Log(Logs::General, Logs::Normal, "Client::ReadBook() textfile:%s Text:%s", txtfile, booktxt2.c_str());
#endif
		auto outapp = new EQApplicationPacket(OP_ReadBook, length + sizeof(BookText_Struct));

		BookText_Struct *out = (BookText_Struct *) outapp->pBuffer;
		out->type = book->type;
		memcpy(out->booktext, booktxt2.c_str(), length);

		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::QuestReadBook(const char* text, uint8 type) {
	std::string booktxt2 = text;
	int length = booktxt2.length();
	if (booktxt2[0] != '\0') {
		auto outapp = new EQApplicationPacket(OP_ReadBook, length + sizeof(BookText_Struct));
		BookText_Struct *out = (BookText_Struct *) outapp->pBuffer;
		out->type = type;
		memcpy(out->booktext, booktxt2.c_str(), length);
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::SendClientMoneyUpdate(uint8 type,uint32 amount){
	auto outapp = new EQApplicationPacket(OP_TradeMoneyUpdate, sizeof(TradeMoneyUpdate_Struct));
	TradeMoneyUpdate_Struct* mus= (TradeMoneyUpdate_Struct*)outapp->pBuffer;
	mus->amount=amount;
	mus->trader=0;
	mus->type=type;
	Log(Logs::Detail, Logs::Debug, "Client::SendClientMoneyUpdate() %s added %i coin of type: %i.",
			GetName(), amount, type);
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendClientMoney(uint32 copper, uint32 silver, uint32 gold, uint32 platinum)
{
	// This method is used to update the client's coin when /split is used, and it cannot
	// complete on the server side (not in a group, not enough coin, rounding error, etc.) 
	// It is not meant to be used as a general coin update.

	int32 updated_plat = GetPlatinum() - platinum;
	int32 updated_gold = GetGold() - gold;
	int32 updated_silver = GetSilver() - silver;
	int32 updated_copper = GetCopper() - copper;

	if (updated_plat >= 0)
		SendClientMoneyUpdate(3, GetPlatinum() - updated_plat);
	else if(updated_plat < 0)
		SendClientMoneyUpdate(3, GetPlatinum());

	if (updated_gold >= 0)
		SendClientMoneyUpdate(2, GetGold() - updated_gold);
	else if (updated_gold < 0)
		SendClientMoneyUpdate(2, GetGold());

	if (updated_silver >= 0)
		SendClientMoneyUpdate(1, GetSilver() - updated_silver);
	else if (updated_silver < 0)
		SendClientMoneyUpdate(1, GetSilver());

	if (updated_copper >= 0)
		SendClientMoneyUpdate(0, GetCopper() - updated_copper);
	else if (updated_copper < 0)
		SendClientMoneyUpdate(0, GetCopper());
}

bool Client::TakeMoneyFromPP(uint64 copper, bool updateclient) {
	int64 copperpp,silver,gold,platinum;
	copperpp = m_pp.copper;
	silver = static_cast<int64>(m_pp.silver) * 10;
	gold = static_cast<int64>(m_pp.gold) * 100;
	platinum = static_cast<int64>(m_pp.platinum) * 1000;

	int64 clienttotal = copperpp + silver + gold + platinum;

	clienttotal -= copper;
	if(clienttotal < 0)
	{
		return false; // Not enough money!
	}
	else
	{
		copperpp -= copper;
		if(copperpp <= 0)
		{
			copper = std::abs(copperpp);
			m_pp.copper = 0;
		}
		else
		{
			m_pp.copper = copperpp;
			SaveCurrency();
			return true;
		}

		silver -= copper;
		if(silver <= 0)
		{
			copper = std::abs(silver);
			m_pp.silver = 0;
		}
		else
		{
			m_pp.silver = silver/10;
			m_pp.copper += (silver-((int64)m_pp.silver*10));
			SaveCurrency();
			return true;
		}

		gold -=copper;

		if(gold <= 0)
		{
			copper = std::abs(gold);
			m_pp.gold = 0;
		}
		else
		{
			m_pp.gold = gold/100;
			uint64 silvertest = (gold-(static_cast<uint64>(m_pp.gold)*100))/10;
			m_pp.silver += silvertest;
			uint64 coppertest = (gold-(static_cast<uint64>(m_pp.gold)*100+silvertest*10));
			m_pp.copper += coppertest;
			SaveCurrency();
			return true;
		}

		platinum -= copper;

		//Impossible for plat to be negative, already checked above

		m_pp.platinum = platinum/1000;
		uint64 goldtest = (platinum-(static_cast<uint64>(m_pp.platinum)*1000))/100;
		m_pp.gold += goldtest;
		uint64 silvertest = (platinum-(static_cast<uint64>(m_pp.platinum)*1000+goldtest*100))/10;
		m_pp.silver += silvertest;
		uint64 coppertest = (platinum-(static_cast<uint64>(m_pp.platinum)*1000+goldtest*100+silvertest*10));
		m_pp.copper = coppertest;
		RecalcWeight();
		SaveCurrency();
		return true;
	}
}

void Client::AddMoneyToPP(uint64 copper, bool updateclient){
	uint64 tmp;
	uint64 tmp2;
	tmp = copper;

	/* Add Amount of Platinum */
	tmp2 = tmp/1000;
	int32 new_val = m_pp.platinum + tmp2;
	if(new_val < 0) { m_pp.platinum = 0; }
	else { m_pp.platinum = m_pp.platinum + tmp2; }
	tmp-=tmp2*1000;
	if(updateclient)
		SendClientMoneyUpdate(3,tmp2);

	/* Add Amount of Gold */
	tmp2 = tmp/100;
	new_val = m_pp.gold + tmp2;
	if(new_val < 0) { m_pp.gold = 0; }
	else { m_pp.gold = m_pp.gold + tmp2; }

	tmp-=tmp2*100;
	if(updateclient)
		SendClientMoneyUpdate(2,tmp2);

	/* Add Amount of Silver */
	tmp2 = tmp/10;
	new_val = m_pp.silver + tmp2;
	if(new_val < 0) {
		m_pp.silver = 0;
	} else {
		m_pp.silver = m_pp.silver + tmp2;
	}
	tmp-=tmp2*10;
	if(updateclient)
		SendClientMoneyUpdate(1,tmp2);

	// Add Amount of Copper
	tmp2 = tmp;
	new_val = m_pp.copper + tmp2;
	if(new_val < 0) {
		m_pp.copper = 0;
	} else {
		m_pp.copper = m_pp.copper + tmp2;
	}
	if(updateclient)
		SendClientMoneyUpdate(0,tmp2);

	RecalcWeight();

	SaveCurrency();

	if (copper != 0)
	{
		Log(Logs::General, Logs::Inventory, "Client::AddMoneyToPP() Added %d copper", copper);
		Log(Logs::General, Logs::Inventory, "%s should have: plat:%i gold:%i silver:%i copper:%i", GetName(), m_pp.platinum, m_pp.gold, m_pp.silver, m_pp.copper);
	}
}

void Client::EVENT_ITEM_ScriptStopReturn(){
	/* Set a timestamp in an entity variable for plugin check_handin.pl in return_items
		This will stopgap players from items being returned if global_npc.pl has a catch all return_items
	*/
	struct timeval read_time;
	char buffer[50];
	gettimeofday(&read_time, 0);
	sprintf(buffer, "%li.%li \n", read_time.tv_sec, read_time.tv_usec);
	this->SetEntityVariable("Stop_Return", buffer);
}

void Client::AddMoneyToPP(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, bool updateclient){
	this->EVENT_ITEM_ScriptStopReturn();

	int32 new_value = m_pp.platinum + platinum;
	if(new_value >= 0 && new_value > m_pp.platinum)
		m_pp.platinum += platinum;
	if(updateclient)
		SendClientMoneyUpdate(3,platinum);

	new_value = m_pp.gold + gold;
	if(new_value >= 0 && new_value > m_pp.gold)
		m_pp.gold += gold;
	if(updateclient)
		SendClientMoneyUpdate(2,gold);

	new_value = m_pp.silver + silver;
	if(new_value >= 0 && new_value > m_pp.silver)
		m_pp.silver += silver;
	if(updateclient)
		SendClientMoneyUpdate(1,silver);

	new_value = m_pp.copper + copper;
	if(new_value >= 0 && new_value > m_pp.copper)
		m_pp.copper += copper;
	if(updateclient)
		SendClientMoneyUpdate(0,copper);

	RecalcWeight();
	SaveCurrency();

	if (copper != 0 || silver != 0 || gold != 0 || platinum != 0)
	{
		Log(Logs::General, Logs::Inventory, "Client::AddMoneyToPP() Added %d copper %d silver %d gold %d platinum", copper, silver, gold, platinum);
		Log(Logs::General, Logs::Inventory, "%s should have: plat:%i gold:%i silver:%i copper:%i", GetName(), m_pp.platinum, m_pp.gold, m_pp.silver, m_pp.copper);
	}
}

bool Client::HasMoney(uint64 Copper) {

	if ((static_cast<uint64>(m_pp.copper) +
		(static_cast<uint64>(m_pp.silver) * 10) +
		(static_cast<uint64>(m_pp.gold) * 100) +
		(static_cast<uint64>(m_pp.platinum) * 1000)) >= Copper)
		return true;

	return false;
}

uint64 Client::GetCarriedMoney() {

	return ((static_cast<uint64>(m_pp.copper) +
		(static_cast<uint64>(m_pp.silver) * 10) +
		(static_cast<uint64>(m_pp.gold) * 100) +
		(static_cast<uint64>(m_pp.platinum) * 1000)));
}

uint64 Client::GetAllMoney() {

	return (
		(static_cast<uint64>(m_pp.copper) +
		(static_cast<uint64>(m_pp.silver) * 10) +
		(static_cast<uint64>(m_pp.gold) * 100) +
		(static_cast<uint64>(m_pp.platinum) * 1000) +
		(static_cast<uint64>(m_pp.copper_bank) +
		(static_cast<uint64>(m_pp.silver_bank) * 10) +
		(static_cast<uint64>(m_pp.gold_bank) * 100) +
		(static_cast<uint64>(m_pp.platinum_bank) * 1000) +
		(static_cast<uint64>(m_pp.copper_cursor) +
		(static_cast<uint64>(m_pp.silver_cursor) * 10) +
		(static_cast<uint64>(m_pp.gold_cursor) * 100) +
		(static_cast<uint64>(m_pp.platinum_cursor) * 1000)))));
}

bool Client::CheckIncreaseSkill(EQ::skills::SkillType skillid, Mob *against_who, float difficulty, uint8 in_success, bool skipcon) {
	if (IsDead() || IsUnconscious())
		return false;
	if (IsAIControlled() && !has_zomm) // no skillups while charmed
		return false;
	if (skillid > EQ::skills::HIGHEST_SKILL)
		return false;
	int skillval = GetRawSkill(skillid);
	int maxskill = GetMaxSkillAfterSpecializationRules(skillid, MaxSkill(skillid));

	if(against_who)
	{
		uint16 who_level = against_who->GetLevel();
		if(against_who->GetSpecialAbility(IMMUNE_AGGRO) || against_who->IsClient() ||
			(!skipcon && GetLevelCon(who_level) == CON_GREEN) ||
			(skipcon && GetLevelCon(who_level + 2) == CON_GREEN)) // Green cons two levels away from light blue.
		{
			Log(Logs::Detail, Logs::Skills, "Skill %d at value %d failed to gain due to invalid target.", skillid, skillval);
			return false; 
		}
	}

	float success = static_cast<float>(in_success);

	// Make sure we're not already at skill cap
	if (skillval < maxskill)
	{
		Log(Logs::General, Logs::Skills, "Skill %d at value %d %s. difficulty: %0.2f", skillid, skillval, in_success == SKILLUP_SUCCESS ? "succeeded" : "failed", difficulty);
		float stat = GetSkillStat(skillid);
		float skillup_modifier = RuleR(Skills, SkillUpModifier);

		if (RuleB(Quarm, EnableGlobalSkillupDifficultyAdjustments))
		{
			float global_skillup_mod = RuleR(Quarm, GlobalSkillupDifficultyAdjustment);
			difficulty *= global_skillup_mod;
		}

		if(difficulty < 1.0f)
			difficulty = 1.0f;
		if(difficulty > 28.0f)
			difficulty = 28.0f;

		float chance1 = (stat / (difficulty * success)) * skillup_modifier;
		if(chance1 > 95)
			chance1 = 95.0;

		if(zone->random.Real(0, 99) < chance1)
		{

			Log(Logs::Detail, Logs::Skills, "Skill %d at value %d passed first roll with %0.2f percent chance (diff %0.2f)", skillid, skillval, chance1, difficulty);

			float skillvalue = skillval / 2.0f;
			if(skillvalue > 95)
				skillvalue = 95.0f;

			float chance2 = 100.0f - skillvalue;

			if(zone->random.Real(0, 99) < chance2)
			{
				SetSkill(skillid, GetRawSkill(skillid) + 1);
				Log(Logs::General, Logs::Skills, "Skill %d at value %d using stat %0.2f successfully gained a point with %0.2f percent chance (diff %0.2f) first roll chance was: %0.2f", skillid, skillval, stat, chance2, difficulty, chance1);
				return true;
			}
			else
			{
				Log(Logs::General, Logs::Skills, "Skill %d at value %d failed second roll with %0.2f percent chance (diff %0.2f)", skillid, skillval, chance2, difficulty);
			}
		} 
		else 
		{
			Log(Logs::Detail, Logs::Skills, "Skill %d at value %d failed first roll with %0.2f percent chance (diff %0.2f)", skillid, skillval, chance1, difficulty);
		}
	} 
	else 
	{
		Log(Logs::Detail, Logs::Skills, "Skill %d at value %d cannot increase due to maximum %d", skillid, skillval, maxskill);
	}
	return false;
}

void Client::CheckLanguageSkillIncrease(uint8 langid, uint8 TeacherSkill) {
	if (IsDead() || IsUnconscious())
		return;
	if (IsAIControlled())
		return;
	if (langid >= MAX_PP_LANGUAGE)
		return;		// do nothing if langid is an invalid language

	int LangSkill = m_pp.languages[langid];		// get current language skill

	if (LangSkill < 100) {	// if the language isn't already maxed
		int32 Chance = 5 + ((TeacherSkill - LangSkill)/10);	// greater chance to learn if teacher's skill is much higher than yours
		Chance = (Chance * RuleI(Skills, LangSkillUpModifier)/100);

		if(zone->random.Real(0,100) < Chance) {	// if they make the roll
			IncreaseLanguageSkill(langid);	// increase the language skill by 1
			Log(Logs::Detail, Logs::Skills, "Language %d at value %d successfully gain with %d%chance", langid, LangSkill, Chance);
		}
		else
			Log(Logs::Detail, Logs::Skills, "Language %d at value %d failed to gain with %d%chance", langid, LangSkill, Chance);
	}
}

bool Client::HasSkill(EQ::skills::SkillType skill_id) const {
	/*if(skill_id == SkillMeditate)
	{
		if(SkillTrainLvl(skill_id, GetClass()) >= GetLevel())
			return true;
	}
	else*/
		return((GetSkill(skill_id) > 0) && CanHaveSkill(skill_id));
}

bool Client::CanHaveSkill(EQ::skills::SkillType skill_id) const 
{
	bool value = database.GetSkillCap(GetClass(), skill_id, RuleI(Character, MaxLevel)) > 0;

	// Racial skills.
	if (!value)
	{
		if (skill_id == EQ::skills::SkillHide)
		{
			if(GetBaseRace() == DARK_ELF || GetBaseRace() == HALFLING || GetBaseRace() == WOOD_ELF)
				return true;
		}
		else if (skill_id == EQ::skills::SkillSneak)
		{
			if (GetBaseRace() == VAHSHIR || GetBaseRace() == HALFLING)
				return true;
		}
		else if (skill_id == EQ::skills::SkillForage)
		{
			if (GetBaseRace() == WOOD_ELF || GetBaseRace() == IKSAR)
				return true;
		}
		else if (skill_id == EQ::skills::SkillSwimming)
		{
			if (GetBaseRace() == IKSAR)
				return true;
		}
		else if (skill_id == EQ::skills::SkillSenseHeading)
		{
			if (GetBaseRace() == DWARF)
				return true;
		}
		else if (skill_id == EQ::skills::SkillTinkering)
		{
			if (GetBaseRace() == GNOME)
				return true;
		}
		else if (skill_id == EQ::skills::SkillSafeFall)
		{
			if (GetBaseRace() == VAHSHIR)
				return true;
		}
	}

	//if you don't have it by max level, then odds are you never will?
	return value;
}

uint16 Client::MaxSkill(EQ::skills::SkillType skillid, uint16 class_, uint16 level) const 
{
	uint16 value = database.GetSkillCap(class_, skillid, level);

	// Racial skills/Minimum values.
	if (value < 50)
	{
		if (skillid == EQ::skills::SkillHide)
		{
			if (GetBaseRace() == DARK_ELF || GetBaseRace() == HALFLING || GetBaseRace() == WOOD_ELF)
				return 50;
		}
		else if (skillid == EQ::skills::SkillSneak)
		{
			if (GetBaseRace() == VAHSHIR || GetBaseRace() == HALFLING)
				return 50;
		}
		else if (skillid == EQ::skills::SkillForage)
		{
			if (GetBaseRace() == WOOD_ELF || GetBaseRace() == IKSAR)
				return 50;
		}
		else if (skillid == EQ::skills::SkillSenseHeading)
		{
			if (GetBaseRace() == DWARF)
				return 50;
		}
		else if (skillid == EQ::skills::SkillTinkering)
		{
			if (GetBaseRace() == GNOME)
				return 50;
		}
		else if (skillid == EQ::skills::SkillSafeFall)
		{
			if (GetBaseRace() == VAHSHIR)
				return 50;
		}
	}
	else if(value < 100)
	{
		if (skillid == EQ::skills::SkillSwimming)
		{
			if (GetBaseRace() == IKSAR)
				return 100;
		}
	}

	return value;
}

uint8 Client::SkillTrainLevel(EQ::skills::SkillType skillid, uint16 class_) {
	return(database.GetTrainLevel(class_, skillid, RuleI(Character, MaxLevel)));
}

uint16 Client::GetMaxSkillAfterSpecializationRules(EQ::skills::SkillType skillid, uint16 maxSkill)
{
	uint16 Result = maxSkill;

	uint16 PrimarySpecialization = 0;

	uint16 PrimarySkillValue = 0;

	uint16 MaxSpecializations = 1;

	if(skillid >= EQ::skills::SkillSpecializeAbjure && skillid <= EQ::skills::SkillSpecializeEvocation)
	{
		bool HasPrimarySpecSkill = false;

		int NumberOfPrimarySpecSkills = 0;

		for(int i = EQ::skills::SkillSpecializeAbjure; i <= EQ::skills::SkillSpecializeEvocation; ++i)
		{
			if(m_pp.skills[i] > 50)
			{
				HasPrimarySpecSkill = true;
				NumberOfPrimarySpecSkills++;
			}
			if(m_pp.skills[i] > PrimarySkillValue)
			{
				PrimarySpecialization = i;
				PrimarySkillValue = m_pp.skills[i];
			}
		}

		if(HasPrimarySpecSkill)
		{
			if(NumberOfPrimarySpecSkills <= MaxSpecializations)
			{
				if(skillid != PrimarySpecialization)
				{
					Result = 50;
				}
			}
			else
			{
				Result = m_pp.skills[skillid]; // don't allow further increase, this is believed to be AKurate behavior https://www.takproject.net/forums/index.php?threads/10-11-2023.26773/#post-123497
				/*
				Message(Chat::Red, "Your spell casting specializations skills have been reset. "
						"Only %i primary specialization skill is allowed.", MaxSpecializations);

				for(int i = EQ::skills::SkillSpecializeAbjure; i <= EQ::skills::SkillSpecializeEvocation; ++i)
					SetSkill((EQ::skills::SkillType)i, 1);

				Save();

				Log(Logs::General, Logs::Normal, "Reset %s's caster specialization skills to 1. "
								"Too many specializations skills were above 50.", GetCleanName());
				*/
			}

		}
	}
	// This should possibly be handled by bonuses rather than here.
	switch(skillid)
	{
		case EQ::skills::SkillTracking:
		{
			Result += ((GetAA(aaAdvancedTracking) * 10) + (GetAA(aaTuneofPursuance) * 10));
			break;
		}

		default:
			break;
	}

	return Result;
}

uint16 Client::GetSkill(EQ::skills::SkillType skill_id) const
{
	uint16 tmp_skill = 0;
	if (skill_id <= EQ::skills::HIGHEST_SKILL)
	{
		if (itembonuses.skillmod[skill_id] > 0)
		{
			tmp_skill = m_pp.skills[skill_id] * (100 + itembonuses.skillmod[skill_id]) / 100;

			// Hard skill cap for our era is 252.  
			// Link Reference: https://mboards.eqtraders.com/eq/forum/tradeskills/general-trade-skill-discussion/17185-get-the-gm-skill-to-mean-something
			if (tmp_skill > HARD_SKILL_CAP)
			{
				tmp_skill = HARD_SKILL_CAP;
			}
		}
		else
		{
			tmp_skill = m_pp.skills[skill_id];
		}
	} 
	
	return tmp_skill;
}

void Client::SetPVP(uint8 toggle) {
	m_pp.pvp = toggle;

	if(GetPVP())
		this->Message_StringID(Chat::Red,PVP_ON);
	else
		Message(Chat::Red, "You no longer follow the ways of discord.");

	SendAppearancePacket(AppearanceType::PVP, GetPVP());
	Save();
}

void Client::WorldKick() {
	auto outapp = new EQApplicationPacket(OP_GMKick, sizeof(GMKick_Struct));
	GMKick_Struct* gmk = (GMKick_Struct *)outapp->pBuffer;
	strcpy(gmk->name,GetName());
	QueuePacket(outapp);
	safe_delete(outapp);
	Kick();
}

void Client::GMKill() {
	auto outapp = new EQApplicationPacket(OP_GMKill, sizeof(GMKill_Struct));
	GMKill_Struct* gmk = (GMKill_Struct *)outapp->pBuffer;
	strcpy(gmk->name,GetName());
	QueuePacket(outapp);
	safe_delete(outapp);
}

bool Client::CheckAccess(int16 iDBLevel, int16 iDefaultLevel) {
	if ((admin >= iDBLevel) || (iDBLevel == AccountStatus::Max && admin >= iDefaultLevel))
		return true;
	else
		return false;
}

void Client::MemorizeSpell(uint32 slot,uint32 spellid,uint32 scribing){
	auto outapp = new EQApplicationPacket(OP_MemorizeSpell, sizeof(MemorizeSpell_Struct));
	MemorizeSpell_Struct* mss=(MemorizeSpell_Struct*)outapp->pBuffer;
	mss->scribing=scribing;
	mss->slot=slot;
	mss->spell_id=spellid;
	outapp->priority = 5;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SetFeigned(bool in_feigned) {
	if (in_feigned)
	{
		if(RuleB(Character, FeignKillsPet))
		{
			SetPet(0);
		}
		// feign breaks charmed pets
		if (GetPet() && GetPet()->IsCharmedPet()) {
			FadePetCharmBuff();
		}
		SetHorseId(0);
		feigned_time = Timer::GetCurrentTime();
	}
	else
	{
		if (feigned)
			entity_list.ClearFeignAggro(this);
	}
	feigned=in_feigned;
 }

void Client::LogMerchant(Client* player, Mob* merchant, uint32 quantity, uint32 price, const EQ::ItemData* item, bool buying)
{
	if(!player || !merchant || !item)
		return;

	std::string LogText = "Qty: ";

	char Buffer[255];
	memset(Buffer, 0, sizeof(Buffer));

	snprintf(Buffer, sizeof(Buffer)-1, "%3i", quantity);
	LogText += Buffer;
	snprintf(Buffer, sizeof(Buffer)-1, "%10i", price);
	LogText += " TotalValue: ";
	LogText += Buffer;
	snprintf(Buffer, sizeof(Buffer)-1, " ItemID: %7i", item->ID);
	LogText += Buffer;
	LogText += " ";
	snprintf(Buffer, sizeof(Buffer)-1, " %s", item->Name);
	LogText += Buffer;

	if (buying==true) {
		database.logevents(player->AccountName(),player->AccountID(),player->admin,player->GetName(),merchant->GetName(),"Buying from Merchant",LogText.c_str(),2);
	}
	else {
		database.logevents(player->AccountName(),player->AccountID(),player->admin,player->GetName(),merchant->GetName(),"Selling to Merchant",LogText.c_str(),3);
	}
}

bool Client::BindWound(uint16 bindmob_id, bool start, bool fail)
{
	EQApplicationPacket* outapp = nullptr;
	bool returned = false;
	Mob *bindmob = entity_list.GetMob(bindmob_id);
	
	if (!bindmob)
	{
		// send "bindmob gone" to client
		outapp = new EQApplicationPacket(OP_Bind_Wound, sizeof(BindWound_Struct));
		BindWound_Struct *bind_out = (BindWound_Struct *)outapp->pBuffer;
		bind_out->type = 5; // not in zone
		QueuePacket(outapp);
		safe_delete(outapp);
		return false;
	}
  
	if (!fail)
	{
		outapp = new EQApplicationPacket(OP_Bind_Wound, sizeof(BindWound_Struct));
		BindWound_Struct *bind_out = (BindWound_Struct *) outapp->pBuffer;
		bind_out->to = bindmob->GetID();

		// Handle solo ruleset
		if (bindmob->IsClient()) {
			Client* bind_client = bindmob->CastToClient();
			std::string msg;
			if (bind_client->IsSoloOnly() && this != bind_client) {
				msg = "This player is running the Solo Only ruleset. You cannot bind wound.";
			}
			else if (IsSelfFound() != bind_client->CastToClient()->IsSelfFound()) {
				msg = "The player's Self Found flag does not match yours. You cannot bind wound.";
			}
			if (!msg.empty()) {
				this->Message(Chat::Red, msg.c_str());
				// DO NOT CHANGE - any other packet order will cause client bugs / crashes.
				bind_out->type = 3;
				QueuePacket(outapp);
				bind_out->type = 1;
				QueuePacket(outapp);
				safe_delete(outapp);
				return false;
			}
		}

		// Start bind
		if(!bindwound_timer.Enabled()) 
		{
			//make sure we actually have a bandage... and consume it.
			int16 bslot = m_inv.HasItemByUse(EQ::item::ItemTypeBandage, 1, invWhereWorn|invWherePersonal|invWhereCursor);
			if (bslot == INVALID_INDEX) 
			{
				bind_out->type = 3;
				QueuePacket(outapp);
				bind_out->type = 0;	//this is the wrong message, dont know the right one.
				QueuePacket(outapp);
				returned = false;
			}
			if (bslot == EQ::invslot::slotCursor) {
				if (GetInv().GetItem(bslot)->GetCharges() > 1) {
					GetInv().DeleteItem(EQ::invslot::slotCursor, 1);
					EQ::ItemInstance* cursoritem = GetInv().GetItem(EQ::invslot::slotCursor);
					// delete item on cursor in client
					auto app = new EQApplicationPacket(OP_DeleteCharge, sizeof(MoveItem_Struct));
					MoveItem_Struct* delitem = (MoveItem_Struct*)app->pBuffer;
					delitem->from_slot = EQ::invslot::slotCursor;
					delitem->to_slot = 0xFFFFFFFF;
					delitem->number_in_stack = 0xFFFFFFFF;
					QueuePacket(app);
					safe_delete(app);
					// send to client new cursor item with updated charges.
					// this does not update visual quantity on cursor, but will reflect
					// correct quantity when put in inventory.
					PushItemOnCursor(*cursoritem, true);
				}
				else {
					DeleteItemInInventory(bslot, 1, true);	//do we need client update?
				}
			} 
			else {
				DeleteItemInInventory(bslot, 1, true);	//do we need client update?
			}
			// start complete timer
			bindwound_timer.Start(10000);
			bindwound_target_id = bindmob->GetID();

			// Send client unlock
			bind_out->type = 3;
			QueuePacket(outapp);
			bind_out->type = 0;
			// Client Unlocked
			/*
			if(!bindmob) 
			{
				// send "bindmob dead" to client
				bind_out->type = 4;
				QueuePacket(outapp);
				bind_out->type = 0;
				bindwound_timer.Disable();
				bindwound_target_id = 0;
				returned = false;
			}
			else 
			*/
			{
				if (bindmob != this )
				{
					bindmob->CastToClient()->Message_StringID(Chat::Skills, STAY_STILL); // Tell IPC to stand still?
				}
			}
		} 
		else 
		{
		// finish bind
			// disable complete timer
			bindwound_timer.Disable();
			bindwound_target_id = 0;
			/*
			if(!bindmob)
			{
				// send "bindmob gone" to client
				bind_out->type = 5; // not in zone
				QueuePacket(outapp);
				bind_out->type = 0;
				returned = false;
			}
			else 
			*/
			{
				if (!IsFeigned() && (DistanceSquared(bindmob->GetPosition(), m_Position)  <= 400))
				{
					bool max_exceeded = false;
					int MaxBindWound = spellbonuses.MaxBindWound + itembonuses.MaxBindWound + aabonuses.MaxBindWound;
					int max_percent = 50 + MaxBindWound;

					if(GetSkill(EQ::skills::SkillBindWound) > 200)
					{
						max_percent = 70 + MaxBindWound;
					}

					if (max_percent > 100)
						max_percent = 100;

					int max_hp = bindmob->GetMaxHP()*max_percent/100;

					// send bindmob new hp's
					if (bindmob->GetHP() < bindmob->GetMaxHP() && bindmob->GetHP() <= (max_hp)-1)
					{
						int bindhps = 3;

						if (GetSkill(EQ::skills::SkillBindWound) > 200)
						{
							// Max 84hp at 210 skill.
							bindhps += GetSkill(EQ::skills::SkillBindWound)*4/10;
						} 
						else if (GetSkill(EQ::skills::SkillBindWound) >= 10)
						{
							// Max 50hp at 200 skill.
							bindhps += GetSkill(EQ::skills::SkillBindWound)/4;
						}

						//Implementation of aaMithanielsBinding is a guess (the multiplier)
						int bindBonus = spellbonuses.BindWound + itembonuses.BindWound + aabonuses.BindWound;

						bindhps += bindhps*bindBonus / 100;

						int chp = bindmob->GetHP() + bindhps;

						bindmob->SetHP(chp);
						bindmob->SendHPUpdate();
					}
					else 
					{
						// Messages taken from Live, could not find a matching stringid in the client.
						if(bindmob->IsClient())
							bindmob->CastToClient()->Message(Chat::Skills, "You cannot be bandaged past %d percent of your max hit points.", max_percent);
						
						Message(Chat::Skills, "You cannot bandage your target past %d percent of their hit points.", max_percent);

						max_exceeded = true;
					}

					// send bindmob bind done
					if (bindmob->IsClient() && bindmob != this)
					{
						bindmob->CastToClient()->Message_StringID(Chat::Yellow, BIND_WOUND_COMPLETE);
					}

					// Send client bind done
					bind_out->type = 1;
					QueuePacket(outapp);
					bind_out->type = 0;
					returned = max_exceeded ? false : true;
				}
				else 
				{
					// Send client bind failed
					if(bindmob != this)
						bind_out->type = 6; // They moved
					else
						bind_out->type = 7; // Bandager moved

					QueuePacket(outapp);
					bind_out->type = 0;
					returned = false;
				}
			}
		}
	}
	else if (bindwound_timer.Enabled()) 
	{
		// You moved
		outapp = new EQApplicationPacket(OP_Bind_Wound, sizeof(BindWound_Struct));
		BindWound_Struct *bind_out = (BindWound_Struct *) outapp->pBuffer;
		bindwound_timer.Disable();
		bindwound_target_id = 0;
		bind_out->type = 7;
		QueuePacket(outapp);
		bind_out->type = 3;
		QueuePacket(outapp);
		return false;
	}
	safe_delete(outapp);
	return returned;
}

void Client::SetMaterial(int16 in_slot, uint32 item_id) {
	const EQ::ItemData* item = database.GetItem(item_id);
	if (item && (item->ItemClass == EQ::item::ItemClassCommon)) {
		if (in_slot== EQ::invslot::slotHead)
			m_pp.item_material.Head.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotChest)
			m_pp.item_material.Chest.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotArms)
			m_pp.item_material.Arms.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotWrist1)
			m_pp.item_material.Wrist.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotWrist2)
			m_pp.item_material.Wrist.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotHands)
			m_pp.item_material.Hands.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotLegs)
			m_pp.item_material.Legs.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotFeet)
			m_pp.item_material.Feet.Material		= item->Material;
		else if (in_slot == EQ::invslot::slotPrimary)
			m_pp.item_material.Primary.Material		= atoi(item->IDFile + 2);
		else if (in_slot == EQ::invslot::slotSecondary)
			m_pp.item_material.Secondary.Material	= atoi(item->IDFile + 2);
	}
}

void Client::ServerFilter(SetServerFilter_Struct* filter){

/*	this code helps figure out the filter IDs in the packet if needed
	static SetServerFilter_Struct ssss;
	int r;
	uint32 *o = (uint32 *) &ssss;
	uint32 *n = (uint32 *) filter;
	for(r = 0; r < (sizeof(SetServerFilter_Struct)/4); r++) {
		if(*o != *n)
			Log(Logs::Detail, Logs::Debug, "Filter %d changed from %d to %d", r, *o, *n);
		o++; n++;
	}
	memcpy(&ssss, filter, sizeof(SetServerFilter_Struct));
*/
#define Filter0(type) \
	if(filter->filters[type] == 1) \
		ClientFilters[type] = FilterShow; \
	else \
		ClientFilters[type] = FilterHide;
#define Filter1(type) \
	if(filter->filters[type] == 0) \
		ClientFilters[type] = FilterShow; \
	else \
		ClientFilters[type] = FilterHide;

	Filter1(FilterNone);
	Filter0(FilterGuildChat);
	Filter0(FilterSocials);
	Filter0(FilterGroupChat);
	Filter0(FilterShouts);
	Filter0(FilterAuctions);
	Filter0(FilterOOC);

	if(filter->filters[FilterPCSpells] == 0)
		ClientFilters[FilterPCSpells] = FilterShow;
	else if(filter->filters[FilterPCSpells] == 1)
		ClientFilters[FilterPCSpells] = FilterHide;
	else
		ClientFilters[FilterPCSpells] = FilterShowGroupOnly;

	//This filter is bugged client side (the client won't toggle the value when the button is pressed.)
	Filter1(FilterNPCSpells);

	if(filter->filters[FilterBardSongs] == 0)
		ClientFilters[FilterBardSongs] = FilterShow;
	else if(filter->filters[FilterBardSongs] == 1)
		ClientFilters[FilterBardSongs] = FilterShowSelfOnly;
	else if(filter->filters[FilterBardSongs] == 2)
		ClientFilters[FilterBardSongs] = FilterShowGroupOnly;
	else
		ClientFilters[FilterBardSongs] = FilterHide;

	if(filter->filters[FilterSpellCrits] == 0)
		ClientFilters[FilterSpellCrits] = FilterShow;
	else if(filter->filters[FilterSpellCrits] == 1)
		ClientFilters[FilterSpellCrits] = FilterShowSelfOnly;
	else
		ClientFilters[FilterSpellCrits] = FilterHide;

	if (filter->filters[FilterMeleeCrits] == 0)
		ClientFilters[FilterMeleeCrits] = FilterShow;
	else if (filter->filters[FilterMeleeCrits] == 1)
		ClientFilters[FilterMeleeCrits] = FilterShowSelfOnly;
	else
		ClientFilters[FilterMeleeCrits] = FilterHide;

	Filter0(FilterMyMisses);
	Filter0(FilterOthersMiss);
	Filter0(FilterOthersHit);
	Filter0(FilterMissedMe);
	Filter1(FilterDamageShields);
}

// this version is for messages with no parameters
void Client::Message_StringID(uint32 type, uint32 string_id, uint32 distance)
{
	if (GetFilter(FilterMeleeCrits) == FilterHide && type == Chat::MeleeCrit) //98 is self...
		return;
	if (GetFilter(FilterSpellCrits) == FilterHide && type == Chat::SpellCrit)
		return;
	if (!Connected())
		return;

	auto outapp = new EQApplicationPacket(OP_FormattedMessage, 12);
	FormattedMessage_Struct *fm = (FormattedMessage_Struct *)outapp->pBuffer;
	fm->string_id = string_id;
	fm->type = type;

	if(distance>0)
		entity_list.QueueCloseClients(this,outapp,false,distance);
	else
		QueuePacket(outapp);
	safe_delete(outapp);

}

//
// this list of 9 args isn't how I want to do it, but to use va_arg
// you have to know how many args you're expecting, and to do that we have
// to load the eqstr file and count them in the string.
// This hack sucks but it's gonna work for now.
//
void Client::Message_StringID(uint32 type, uint32 string_id, const char* message1,
	const char* message2,const char* message3,const char* message4,
	const char* message5,const char* message6,const char* message7,
	const char* message8,const char* message9, uint32 distance)
{
	if (GetFilter(FilterMeleeCrits) == FilterHide && type == Chat::MeleeCrit) //98 is self...
		return;
	if (GetFilter(FilterSpellCrits) == FilterHide && type == Chat::SpellCrit)
		return;

	if (!Connected())
		return;

	int i, argcount, length;
	char *bufptr;
	const char *message_arg[9] = {0};

	if (type == Chat::Emote) {
		type = 4;
	}

	if(!message1) {
		Message_StringID(type, string_id);	// use the simple message instead
		return;
	}

	i = 0;
	message_arg[i++] = message1;
	message_arg[i++] = message2;
	message_arg[i++] = message3;
	message_arg[i++] = message4;
	message_arg[i++] = message5;
	message_arg[i++] = message6;
	message_arg[i++] = message7;
	message_arg[i++] = message8;
	message_arg[i++] = message9;

	for(argcount = length = 0; message_arg[argcount]; argcount++)
    {
		length += strlen(message_arg[argcount]) + 1;
    }

	auto outapp = new EQApplicationPacket(OP_FormattedMessage, length+13);
	FormattedMessage_Struct *fm = (FormattedMessage_Struct *)outapp->pBuffer;
	fm->string_id = string_id;
	fm->type = type;
	bufptr = fm->message;

	// since we're moving the pointer the 0 offset is correct
	bufptr[0] = '\0';

    for(i = 0; i < argcount; i++)
    {
        strcpy(bufptr, message_arg[i]);
        bufptr += strlen(message_arg[i]) + 1;
    }


    if(distance>0)
        entity_list.QueueCloseClients(this,outapp,false,distance);
    else
        QueuePacket(outapp);
    safe_delete(outapp);

}

// helper function, returns true if we should see the message
bool Client::FilteredMessageCheck(Mob *sender, eqFilterType filter)
{
	eqFilterMode mode = GetFilter(filter);
	// easy ones first
	if (mode == FilterShow)
		return true;
	else if (mode == FilterHide)
		return false;

	if (!sender && mode == FilterHide) {
		return false;
	} else if (sender) {
		if (this == sender) {
			if (mode == FilterHide) // don't need to check others
				return false;
		} else if (mode == FilterShowSelfOnly) { // we know sender isn't us
			return false;
		} else if (mode == FilterShowGroupOnly) {
			Group *g = GetGroup();
			Raid *r = GetRaid();
			if (g) {
				if (g->IsGroupMember(sender))
					return true;
			} else if (r && sender->IsClient()) {
				uint32 rgid1 = r->GetGroup(this);
				uint32 rgid2 = r->GetGroup(sender->CastToClient());
				if (rgid1 != 0xFFFFFFFF && rgid1 == rgid2)
					return true;
			} else {
				return false;
			}
		}
	}

	// we passed our checks
	return true;
}

void Client::FilteredMessage_StringID(Mob *sender, uint32 type,
		eqFilterType filter, uint32 string_id)
{
	if (!FilteredMessageCheck(sender, filter))
		return;

	auto outapp = new EQApplicationPacket(OP_Animation, 12);
	SimpleMessage_Struct *sms = (SimpleMessage_Struct *)outapp->pBuffer;
	sms->color = type;
	sms->string_id = string_id;

	sms->unknown8 = 0;

	QueuePacket(outapp);
	safe_delete(outapp);

	return;
}

void Client::FilteredMessage_StringID(Mob *sender, uint32 type, eqFilterType filter, uint32 string_id,
		const char *message1, const char *message2, const char *message3,
		const char *message4, const char *message5, const char *message6,
		const char *message7, const char *message8, const char *message9)
{
	if (!FilteredMessageCheck(sender, filter))
		return;

	if (!Connected())
		return;

	int i, argcount, length;
	char *bufptr;
	const char *message_arg[9] = {0};

	if (type == Chat::Emote) {
		type = 4;
	}

	if (!message1) {
		FilteredMessage_StringID(sender, type, filter, string_id);	// use the simple message instead
		return;
	}

	i = 0;
	message_arg[i++] = message1;
	message_arg[i++] = message2;
	message_arg[i++] = message3;
	message_arg[i++] = message4;
	message_arg[i++] = message5;
	message_arg[i++] = message6;
	message_arg[i++] = message7;
	message_arg[i++] = message8;
	message_arg[i++] = message9;

	for (argcount = length = 0; message_arg[argcount]; argcount++)
    {
		length += strlen(message_arg[argcount]) + 1;
    }

    auto outapp = new EQApplicationPacket(OP_FormattedMessage, length+13);
	FormattedMessage_Struct *fm = (FormattedMessage_Struct *)outapp->pBuffer;
	fm->string_id = string_id;
	fm->type = type;
	bufptr = fm->message;


	for (i = 0; i < argcount; i++) {
		strcpy(bufptr, message_arg[i]);
		bufptr += strlen(message_arg[i]) + 1;
	}

	// since we're moving the pointer the 0 offset is correct
	bufptr[0] = '\0';

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::Tell_StringID(uint32 string_id, const char *who, const char *message)
{

	if (!Connected())
		return;
	char string_id_str[10];
	snprintf(string_id_str, 10, "%d", string_id);

	Message_StringID(Chat::EchoTell, TELL_QUEUED_MESSAGE, who, string_id_str, message);
}

void Client::SetHideMe(bool flag)
{
	EQApplicationPacket app;

	gmhideme = flag;

	if(gmhideme)
	{
		if(!GetGM())
			SetGM(true);
		if(GetAnon() != 1) // 1 is anon, 2 is role
			SetAnon(true);
		database.SetHideMe(AccountID(),true);
		CreateDespawnPacket(&app, false);
		entity_list.RemoveFromTargets(this);
		tellsoff = true;
	}
	else
	{
		SetAnon(false);
		database.SetHideMe(AccountID(),false);
		CreateSpawnPacket(&app);
		tellsoff = false;
	}

	entity_list.QueueClientsStatus(this, &app, true, 0, Admin()-1);
	safe_delete_array(app.pBuffer);
	UpdateWho();
}

void Client::SetLanguageSkill(int langid, int value)
{
	if (langid >= MAX_PP_LANGUAGE)
		return; //Invalid Language

	if (value > 100)
		value = 100; //Max lang value

	m_pp.languages[langid] = value;
	database.SaveCharacterLanguage(this->CharacterID(), langid, value);

	auto outapp = new EQApplicationPacket(OP_SkillUpdate, sizeof(SkillUpdate_Struct));
	SkillUpdate_Struct* skill = (SkillUpdate_Struct*)outapp->pBuffer;
	skill->skillId = 100 + langid;
	skill->value = m_pp.languages[langid];
	QueuePacket(outapp);
	safe_delete(outapp);

	Message_StringID( Chat::Skills, LANG_SKILL_IMPROVED ); //Notify the client
}

void Client::LinkDead()
{
	if (GetGroup())
	{
		entity_list.MessageGroup(this,true,Chat::Yellow,"%s has gone Linkdead.",GetName());
		LeaveGroup();
	}
	Raid *raid = entity_list.GetRaidByClient(this);
	if(raid){
		raid->DisbandRaidMember(GetName());
	}
//	save_timer.Start(2500);
	linkdead_timer.Start(RuleI(Zone,ClientLinkdeadMS));
	SendAppearancePacket(AppearanceType::Linkdead, 1);
	client_state = CLIENT_LINKDEAD;
	if (IsSitting())
	{
		Stand();
	}
	AI_Start();
	UpdateWho();
	if(Trader)
		Trader_EndTrader();
}

void Client::Escape()
{
	entity_list.RemoveFromNPCTargets(this);
	if (GetClass() == ROGUE)
	{
		sneaking = true;
		SendAppearancePacket(AppearanceType::Sneak, sneaking);
		if (GetAA(aaShroudofStealth))
		{
			improved_hidden = true;
		}
		hidden = true;
		SetInvisible(INVIS_HIDDEN);
	}
	else
	{
		SetInvisible(INVIS_NORMAL);
	}
	Message_StringID(Chat::Skills, ESCAPE);
}

// Based on http://www.eqtraders.com/articles/article_page.php?article=g190&menustr=070000000000
float Client::CalcPriceMod(Mob* other, bool reverse)
{
	float priceMult = 0.8f;

	if (other && other->IsNPC())
	{
		int factionlvl = GetFactionLevel(CharacterID(), GetRace(), GetClass(), GetDeity(), other->CastToNPC()->GetPrimaryFaction(), other);
		int32 cha = GetCHA();
		if (factionlvl <= FACTION_AMIABLY)
			cha += 11;		// amiable faction grants a defacto 11 charisma bonus

		uint8 greed = other->CastToNPC()->GetGreedPercent();

		// Sony's precise algorithm is unknown, but this produces output that is virtually identical
		if (factionlvl <= FACTION_INDIFFERENTLY)
		{
			if (cha > 75)
			{
				if (greed)
				{
					// this is derived from curve fitting to a lot of price data
					priceMult = -0.2487768 + (1.599635 - -0.2487768) / (1 + pow((cha / 135.1495), 1.001983));
					priceMult += (greed + 25u) / 100.0f;  // default vendor markup is 25%; anything above that is 'greedy'
					priceMult = 1.0f / priceMult;
				}
				else
				{
					// non-greedy merchants use a linear scale
					priceMult = 1.0f - ((115.0f - cha) * 0.004f);
				}
			}
			else if (cha > 60)
			{
				priceMult = 1.0f / (1.25f + (greed / 100.0f));
			}
			else
			{
				priceMult = 1.0f / ((1.0f - (cha - 120.0f) / 220.0f) + (greed / 100.0f));
			}
		}
		else // apprehensive
		{
			if (cha > 75)
			{
				if (greed)
				{
					// this is derived from curve fitting to a lot of price data
					priceMult = -0.25f + (1.823662 - -0.25f) / (1 + (cha / 135.0f));
					priceMult += (greed + 25u) / 100.0f;  // default vendor markup is 25%; anything above that is 'greedy'
					priceMult = 1.0f / priceMult;
				}
				else
				{
					priceMult = (100.0f - (145.0f - cha) / 2.8f) / 100.0f;
				}
			}
			else if (cha > 60)
			{
				priceMult = 1.0f / (1.4f + greed / 100.0f);
			}
			else
			{
				priceMult = 1.0f / ((1.0f + (143.574 - cha) / 196.434) + (greed / 100.0f));
			}
		}

		float maxResult = 1.0f / 1.05;		// price reduction caps at this amount
		if (priceMult > maxResult)
			priceMult = maxResult;

		if (!reverse)
			priceMult = 1.0f / priceMult;
	}

	std::string type = "sells";
	std::string type2 = "to";
	if (reverse)
	{
		type = "buys";
		type2 = "from";
	}

	Log(Logs::General, Logs::Trading, "%s %s items at %0.2f the cost %s %s", other->GetName(), type.c_str(), priceMult, type2.c_str(), GetName());
	return priceMult;
}

void Client::SacrificeConfirm(Client *caster) {

	auto outapp = new EQApplicationPacket(OP_Sacrifice, sizeof(Sacrifice_Struct));
	Sacrifice_Struct *ss = (Sacrifice_Struct*)outapp->pBuffer;

	if (!caster || PendingSacrifice)
	{
		safe_delete(outapp);
		return;
	}

	if(GetLevel() < RuleI(Spells, SacrificeMinLevel))
	{
		caster->Message_StringID(Chat::Red, SAC_TOO_LOW);	//This being is not a worthy sacrifice.
		safe_delete(outapp);
		return;
	}

	if (GetLevel() > RuleI(Spells, SacrificeMaxLevel)) 
	{
		caster->Message_StringID(Chat::Red, SAC_TOO_HIGH); //This being is too powerful to be a sacrifice.
		safe_delete(outapp);
		return;
	}

	ss->CasterID = caster->GetID();
	ss->TargetID = GetID();
	ss->Confirm = 0;
	QueuePacket(outapp);
	safe_delete(outapp);
	// We store the Caster's name, because when the packet comes back, it only has the victim's entityID in it,
	// not the caster.
	SacrificeCaster += caster->GetName();
	PendingSacrifice = true;
}

//Essentially a special case death function
void Client::Sacrifice(Client *caster)
{
	if(GetLevel() >= RuleI(Spells, SacrificeMinLevel) && GetLevel() <= RuleI(Spells, SacrificeMaxLevel)){
		
		float loss;
		uint8 level = GetLevel();
		if (level >= 1 && level <= 30)
			loss = 0.08f;
		if (level >= 31 && level <= 35)
			loss = 0.075f;
		if (level >= 36 && level <= 40)
			loss = 0.07f;
		if (level >= 41 && level <= 45)
			loss = 0.065f;
		if (level >= 46 && level <= 58)
			loss = 0.06f;
		if (level == 59)
			loss = 0.05f;
		if (level == 60)
			loss = 0.16f;
		if (level >= 61)
			loss = 0.07f;

		int requiredxp = GetEXPForLevel(level + 1) - GetEXPForLevel(level);
		int exploss = (int)((float)requiredxp * (loss * RuleR(Character, EXPLossMultiplier)));

		if(exploss < GetEXP()){
			SetEXP(GetEXP()-exploss, GetAAXP());
			SendLogoutPackets();

			Client* killer = caster ? caster : nullptr;
			GenerateDeathPackets(killer, 0, 1768, EQ::skills::SkillAlteration, false, Killed_Sac);

			BuffFadeAll();
			UnmemSpellAll();
			Group *g = GetGroup();
			if(g){
				g->MemberZoned(this);
			}
			Raid *r = entity_list.GetRaidByClient(this);
			if(r){
				r->MemberZoned(this);
			}
			ClearAllProximities();
			if(RuleB(Character, LeaveCorpses)){
				auto new_corpse = new Corpse(this, 0, Killed_Sac);
				entity_list.AddCorpse(new_corpse, GetID());
				SetID(0);
			}

			SetHP(-100);
			SetMana(GetMaxMana());

			Save();
			GoToDeath();
			caster->SummonItem(RuleI(Spells, SacrificeItemID));
		}
	}
	else{
		caster->Message_StringID(Chat::Red, SAC_TOO_LOW);	//This being is not a worthy sacrifice.
	}
}

void Client::SendOPTranslocateConfirm(Mob *Caster, uint16 SpellID) {

	if (!Caster || PendingTranslocate)
		return;

	const SPDat_Spell_Struct &Spell = spells[SpellID];

	auto outapp = new EQApplicationPacket(OP_Translocate, sizeof(Translocate_Struct));
	Translocate_Struct *ts = (Translocate_Struct*)outapp->pBuffer;

	strcpy(ts->Caster, Caster->GetName());
	PendingTranslocateData.spell_id = ts->SpellID = SpellID;
	uint32 zoneid = database.GetZoneID(Spell.teleport_zone);

	if (!CanBeInZone(zoneid))
	{
		safe_delete(outapp);
		return;
	}

	if ((SpellID == 1422) || (SpellID == 1334) || (SpellID == 3243)) {
		PendingTranslocateData.zone_id = ts->ZoneID = m_pp.binds[0].zoneId;
		PendingTranslocateData.x = ts->x = m_pp.binds[0].x;
		PendingTranslocateData.y = ts->y = m_pp.binds[0].y;
		PendingTranslocateData.z = ts->z = m_pp.binds[0].z;
		PendingTranslocateData.heading = m_pp.binds[0].heading;
	}
	else {
		PendingTranslocateData.zone_id = ts->ZoneID = zoneid;
		PendingTranslocateData.y = ts->y = Spell.base[0];
		PendingTranslocateData.x = ts->x = Spell.base[1];
		PendingTranslocateData.z = ts->z = Spell.base[2];
		PendingTranslocateData.heading = 0.0;
	}

	ts->unknown008 = 0;
	ts->Complete = 0;

	PendingTranslocate = true;
	TranslocateTime = time(nullptr);

	QueuePacket(outapp);
	safe_delete(outapp);

	return;
}

void Client::SendPickPocketResponse(Mob *from, uint32 amt, int type, int16 slotid, EQ::ItemInstance* inst, bool skipskill)
{
	if(!skipskill)
	{
		uint8 success = SKILLUP_FAILURE;
		if(type > PickPocketFailed)
			success = SKILLUP_SUCCESS;

		CheckIncreaseSkill(EQ::skills::SkillPickPockets, nullptr, zone->skill_difficulty[EQ::skills::SkillPickPockets].difficulty, success);
	}

	if(type == PickPocketItem && inst)
	{
		SendItemPacket(slotid, inst, ItemPacketStolenItem, GetID(), from->GetID(), GetSkill(EQ::skills::SkillPickPockets));
		PutItemInInventory(slotid, *inst);
		return;
	}
	else
	{

		auto outapp = new EQApplicationPacket(OP_PickPocket, sizeof(PickPocket_Struct));
		PickPocket_Struct* pick_out = (PickPocket_Struct*) outapp->pBuffer;
		pick_out->coin = amt;
		pick_out->from = GetID();
		pick_out->to = from->GetID();
		pick_out->myskill = GetSkill(EQ::skills::SkillPickPockets);

		if(amt == 0)
			type = PickPocketFailed;

		pick_out->type = type;

		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

bool Client::GetPickPocketSlot(EQ::ItemInstance* inst, int16& freeslotid)
{

	if(CheckLoreConflict(inst->GetItem()))
	{
		return false;
	}

	if(database.ItemQuantityType(inst->GetItem()->ID) == EQ::item::Quantity_Stacked)
	{
		freeslotid = GetStackSlot(inst);
		if(freeslotid >= 0)
		{
			EQ::ItemInstance* oldinst = GetInv().GetItem(freeslotid);
			if(oldinst)
			{
				uint8 newcharges = oldinst->GetCharges() + inst->GetCharges();
				inst->SetCharges(newcharges);
				DeleteItemInInventory(freeslotid, 0, true);
			}
			return true;
		}
	}
	
	if(freeslotid < 0)
	{
		bool is_arrow = (inst->GetItem()->ItemType == EQ::item::ItemTypeArrow) ? true : false;
		freeslotid = m_inv.FindFreeSlot(false, true, inst->GetItem()->Size, is_arrow);

		//make sure we are not completely full...
		if ((freeslotid == EQ::invslot::slotCursor && m_inv.GetItem(EQ::invslot::slotCursor) != nullptr) || freeslotid == INVALID_INDEX)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	return false;
}

bool Client::IsDiscovered(uint32 itemid) {

	std::string query = StringFormat("SELECT count(*) FROM discovered_items WHERE item_id = '%lu'", itemid);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

	auto row = results.begin();
	if (!atoi(row[0]))
		return false;

	return true;
}

void Client::DiscoverItem(uint32 itemid) {

	std::string query = StringFormat("INSERT INTO discovered_items "
									"SET item_id = %lu, char_name = '%s', "
									"discovered_date = UNIX_TIMESTAMP(), account_status = %i",
									itemid, GetName(), Admin());
	auto results = database.QueryDatabase(query);

	parse->EventPlayer(EVENT_DISCOVER_ITEM, this, "", itemid);
}

uint16 Client::GetPrimarySkillValue()
{
	EQ::skills::SkillType skill = EQ::skills::HIGHEST_SKILL; //because nullptr == 0, which is 1H Slashing, & we want it to return 0 from GetSkill
	bool equiped = m_inv.GetItem(EQ::invslot::slotPrimary);

	if (!equiped)
		skill = EQ::skills::SkillHandtoHand;

	else {

		uint8 type = m_inv.GetItem(EQ::invslot::slotPrimary)->GetItem()->ItemType; //is this the best way to do this?

		switch (type)
		{
			case EQ::item::ItemType1HSlash: // 1H Slashing
			{
				skill = EQ::skills::Skill1HSlashing;
				break;
			}
			case EQ::item::ItemType2HSlash: // 2H Slashing
			{
				skill = EQ::skills::Skill2HSlashing;
				break;
			}
			case EQ::item::ItemType1HPiercing: // Piercing
			{
				skill = EQ::skills::Skill1HPiercing;
				break;
			}
			case EQ::item::ItemType1HBlunt: // 1H Blunt
			{
				skill = EQ::skills::Skill1HBlunt;
				break;
			}
			case EQ::item::ItemType2HBlunt: // 2H Blunt
			{
				skill = EQ::skills::Skill2HBlunt;
				break;
			}
			case EQ::item::ItemType2HPiercing: // 2H Piercing
			{
				skill = EQ::skills::Skill1HPiercing; // change to Skill2HPiercing once activated
				break;
			}
			case EQ::item::ItemTypeMartial: // Hand to Hand
			{
				skill = EQ::skills::SkillHandtoHand;
				break;
			}
			default: // All other types default to Hand to Hand
			{
				skill = EQ::skills::SkillHandtoHand;
				break;
			}
		}
	}

	return GetSkill(skill);
}

int Client::GetAggroCount() {
	return AggroCount;
}

void Client::SummonAndRezzAllCorpses()
{
	PendingRezzXP = -1;
	PendingRezzZoneID = 0;
	PendingRezzZoneGuildID = 0;

	auto Pack = new ServerPacket(ServerOP_DepopAllPlayersCorpses, sizeof(ServerDepopAllPlayersCorpses_Struct));

	ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct*)Pack->pBuffer;

	sdapcs->CharacterID = CharacterID();
	sdapcs->ZoneID = zone->GetZoneID();
	sdapcs->GuildID = zone->GetGuildID();

	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveAllCorpsesByCharID(CharacterID());

	int CorpseCount = database.SummonAllCharacterCorpses(CharacterID(), zone->GetZoneID(), zone->GetGuildID(), GetPosition());
	if(CorpseCount <= 0)
	{
		Message(clientMessageYellow, "You have no corpses to summnon.");
		return;
	}

	int RezzExp = entity_list.RezzAllCorpsesByCharID(CharacterID());

	if(RezzExp > 0)
		SetEXP(GetEXP() + RezzExp, GetAAXP(), true);

	Message(clientMessageYellow, "All your corpses have been summoned to your feet and have received a 100% resurrection.");
}

void Client::SummonAllCorpses(const glm::vec4& position)
{
	auto summonLocation = position;
	if(IsOrigin(position) && position.w == 0.0f)
		summonLocation = GetPosition();

	auto Pack = new ServerPacket(ServerOP_DepopAllPlayersCorpses, sizeof(ServerDepopAllPlayersCorpses_Struct));

	ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct*)Pack->pBuffer;

	sdapcs->CharacterID = CharacterID();
	sdapcs->ZoneID = zone->GetZoneID();
	sdapcs->GuildID = zone->GetGuildID();

	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveAllCorpsesByCharID(CharacterID());

	database.SummonAllCharacterCorpses(CharacterID(), zone->GetZoneID(), zone->GetGuildID(), summonLocation);
}

void Client::DepopAllCorpses()
{
	auto Pack = new ServerPacket(ServerOP_DepopAllPlayersCorpses, sizeof(ServerDepopAllPlayersCorpses_Struct));

	ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct*)Pack->pBuffer;

	sdapcs->CharacterID = CharacterID();
	sdapcs->ZoneID = zone->GetZoneID();
	sdapcs->GuildID = zone->GetGuildID();
	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveAllCorpsesByCharID(CharacterID());
}

void Client::DepopPlayerCorpse(uint32 dbid)
{
	auto Pack = new ServerPacket(ServerOP_DepopPlayerCorpse, sizeof(ServerDepopPlayerCorpse_Struct));

	ServerDepopPlayerCorpse_Struct *sdpcs = (ServerDepopPlayerCorpse_Struct*)Pack->pBuffer;

	sdpcs->DBID = dbid;
	sdpcs->ZoneID = zone->GetZoneID();
	sdpcs->GuildID = zone->GetGuildID();
	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveCorpseByDBID(dbid);
}

void Client::BuryPlayerCorpses()
{
	database.BuryAllCharacterCorpses(CharacterID());
}

void Client::NotifyNewTitlesAvailable()
{
	auto outapp = new EQApplicationPacket(OP_SetTitle, 0);
	QueuePacket(outapp);
	safe_delete(outapp);
}

uint32 Client::GetStartZone()
{
	return m_pp.binds[4].zoneId;
}

void Client::SetShadowStepExemption(bool v)
{
	if(v == true)
	{
		uint32 cur_time = Timer::GetCurrentTime();
		if((cur_time - m_TimeSinceLastPositionCheck) > 1000)
		{
			float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
			float runs = GetRunspeed();
			if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
			{
				Log(Logs::General, Logs::Error, "%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
					m_DistanceSinceLastPositionCheck, (cur_time - m_TimeSinceLastPositionCheck), speed);
				if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
				{
					if(IsShadowStepExempted())
					{
						if(m_DistanceSinceLastPositionCheck > 800)
						{
							CheatDetected(MQWarpShadowStep, GetX(), GetY(), GetZ());
						}
					}
					else if(IsKnockBackExempted())
					{
						//still potential to trigger this if you're knocked back off a
						//HUGE fall that takes > 2.5 seconds
						if(speed > 30.0f)
						{
							CheatDetected(MQWarpKnockBack, GetX(), GetY(), GetZ());
						}
					}
					else if(!IsPortExempted())
					{
						if(!IsMQExemptedArea(zone->GetZoneID(), GetX(), GetY(), GetZ()))
						{
							if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
							{
								CheatDetected(MQWarp, GetX(), GetY(), GetZ());
								m_TimeSinceLastPositionCheck = cur_time;
								m_DistanceSinceLastPositionCheck = 0.0f;
								//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
							}
							else
							{
								CheatDetected(MQWarpLight, GetX(), GetY(), GetZ());
							}
						}
					}
				}
			}
		}
		m_TimeSinceLastPositionCheck = cur_time;
		m_DistanceSinceLastPositionCheck = 0.0f;
	}
	m_ShadowStepExemption = v;
}

void Client::SetKnockBackExemption(bool v)
{
	if(v == true)
	{
		uint32 cur_time = Timer::GetCurrentTime();
		if((cur_time - m_TimeSinceLastPositionCheck) > 1000)
		{
			float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
			float runs = GetRunspeed();
			if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
			{
				if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
				{
					Log(Logs::General, Logs::Error, "%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
					m_DistanceSinceLastPositionCheck, (cur_time - m_TimeSinceLastPositionCheck), speed);
					if(IsShadowStepExempted())
					{
						if(m_DistanceSinceLastPositionCheck > 800)
						{
							CheatDetected(MQWarpShadowStep, GetX(), GetY(), GetZ());
						}
					}
					else if(IsKnockBackExempted())
					{
						//still potential to trigger this if you're knocked back off a
						//HUGE fall that takes > 2.5 seconds
						if(speed > 30.0f)
						{
							CheatDetected(MQWarpKnockBack, GetX(), GetY(), GetZ());
						}
					}
					else if(!IsPortExempted())
					{
						if(!IsMQExemptedArea(zone->GetZoneID(), GetX(), GetY(), GetZ()))
						{
							if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
							{
								m_TimeSinceLastPositionCheck = cur_time;
								m_DistanceSinceLastPositionCheck = 0.0f;
								CheatDetected(MQWarp, GetX(), GetY(), GetZ());
								//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
							}
							else
							{
								CheatDetected(MQWarpLight, GetX(), GetY(), GetZ());
							}
						}
					}
				}
			}
		}
		m_TimeSinceLastPositionCheck = cur_time;
		m_DistanceSinceLastPositionCheck = 0.0f;
	}
	m_KnockBackExemption = v;
}

void Client::SetPortExemption(bool v)
{
	if(v == true)
	{
		uint32 cur_time = Timer::GetCurrentTime();
		if((cur_time - m_TimeSinceLastPositionCheck) > 1000)
		{
			float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
			float runs = GetRunspeed();
			if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
			{
				if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
				{
					Log(Logs::General, Logs::Error, "%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
					m_DistanceSinceLastPositionCheck, (cur_time - m_TimeSinceLastPositionCheck), speed);
					if(IsShadowStepExempted())
					{
						if(m_DistanceSinceLastPositionCheck > 800)
						{
								CheatDetected(MQWarpShadowStep, GetX(), GetY(), GetZ());
						}
					}
					else if(IsKnockBackExempted())
					{
						//still potential to trigger this if you're knocked back off a
						//HUGE fall that takes > 2.5 seconds
						if(speed > 30.0f)
						{
							CheatDetected(MQWarpKnockBack, GetX(), GetY(), GetZ());
						}
					}
					else if(!IsPortExempted())
					{
						if(!IsMQExemptedArea(zone->GetZoneID(), GetX(), GetY(), GetZ()))
						{
							if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
							{
								m_TimeSinceLastPositionCheck = cur_time;
								m_DistanceSinceLastPositionCheck = 0.0f;
								CheatDetected(MQWarp, GetX(), GetY(), GetZ());
								//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
							}
							else
							{
								CheatDetected(MQWarpLight, GetX(), GetY(), GetZ());
							}
						}
					}
				}
			}
		}
		m_TimeSinceLastPositionCheck = cur_time;
		m_DistanceSinceLastPositionCheck = 0.0f;
	}
	m_PortExemption = v;
}

void Client::Signal(uint32 data)
{
	std::string export_string = fmt::format("{}", data);
	parse->EventPlayer(EVENT_SIGNAL, this, export_string, 0);
}

const bool Client::IsMQExemptedArea(uint32 zoneID, float x, float y, float z) const
{
	float max_dist = 90000;
	switch(zoneID)
	{
	case 2:
		{
			float delta = (x-(-713.6));
			delta *= delta;
			float distance = delta;
			delta = (y-(-160.2));
			delta *= delta;
			distance += delta;
			delta = (z-(-12.8));
			delta *= delta;
			distance += delta;

			if(distance < max_dist)
				return true;

			delta = (x-(-153.8));
			delta *= delta;
			distance = delta;
			delta = (y-(-30.3));
			delta *= delta;
			distance += delta;
			delta = (z-(8.2));
			delta *= delta;
			distance += delta;

			if(distance < max_dist)
				return true;

			break;
		}
	case 9:
	{
		float delta = (x-(-682.5));
		delta *= delta;
		float distance = delta;
		delta = (y-(147.0));
		delta *= delta;
		distance += delta;
		delta = (z-(-9.9));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-655.4));
		delta *= delta;
		distance = delta;
		delta = (y-(10.5));
		delta *= delta;
		distance += delta;
		delta = (z-(-51.8));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		break;
	}
	case 62:
	case 75:
	case 114:
	case 209:
	{
		//The portals are so common in paineel/felwitheb that checking
		//distances wouldn't be worth it cause unless you're porting to the
		//start field you're going to be triggering this and that's a level of
		//accuracy I'm willing to sacrifice
		return true;
		break;
	}

	case 24:
	{
		float delta = (x-(-183.0));
		delta *= delta;
		float distance = delta;
		delta = (y-(-773.3));
		delta *= delta;
		distance += delta;
		delta = (z-(54.1));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-8.8));
		delta *= delta;
		distance = delta;
		delta = (y-(-394.1));
		delta *= delta;
		distance += delta;
		delta = (z-(41.1));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-310.3));
		delta *= delta;
		distance = delta;
		delta = (y-(-1411.6));
		delta *= delta;
		distance += delta;
		delta = (z-(-42.8));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-183.1));
		delta *= delta;
		distance = delta;
		delta = (y-(-1409.8));
		delta *= delta;
		distance += delta;
		delta = (z-(37.1));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		break;
	}

	case 110:
	case 34:
	case 96:
	case 93:
	case 68:
	case 84:
		{
			if(GetBoatID() != 0)
				return true;
			break;
		}
	default:
		break;
	}
	return false;
}

void Client::SuspendMinion()
{
	NPC *CurrentPet = GetPet()->CastToNPC();

	int AALevel = GetAA(aaSuspendedMinion);

	if(AALevel == 0)
		return;

	if(GetLevel() < 62)
		return;

	if(!CurrentPet)
	{
		if(m_suspendedminion.SpellID > 0)
		{
			if (m_suspendedminion.SpellID >= SPDAT_RECORDS) 
			{
				Message(Chat::Red, "Invalid suspended minion spell id (%u).", m_suspendedminion.SpellID);
				memset(&m_suspendedminion, 0, sizeof(PetInfo));
				return;
			}

			MakePoweredPet(m_suspendedminion.SpellID, spells[m_suspendedminion.SpellID].teleport_zone,
				m_suspendedminion.petpower, m_suspendedminion.Name, m_suspendedminion.size, m_suspendedminion.focusItemId);

			CurrentPet = GetPet()->CastToNPC();

			if(!CurrentPet)
			{
				Message(Chat::Red, "Failed to recall suspended minion.");
				return;
			}

			if(AALevel >= 2)
			{
				CurrentPet->SetPetState(m_suspendedminion.Buffs, m_suspendedminion.Items);

			}
			CurrentPet->CalcBonuses();

			CurrentPet->SetHP(m_suspendedminion.HP);

			CurrentPet->SetMana(m_suspendedminion.Mana);

			Message_StringID(clientMessageTell, SUSPEND_MINION_UNSUSPEND, CurrentPet->GetCleanName());

			memset(&m_suspendedminion, 0, sizeof(struct PetInfo));
		}
		else
			return;

	}
	else
	{
		uint16 SpellID = CurrentPet->GetPetSpellID();

		if(SpellID)
		{
			if(m_suspendedminion.SpellID > 0)
			{
				Message_StringID(clientMessageError,ONLY_ONE_PET);

				return;
			}
			else if(CurrentPet->IsEngaged())
			{
				Message_StringID(clientMessageError,SUSPEND_MINION_FIGHTING);

				return;
			}
			else if(entity_list.Fighting(CurrentPet))
			{
				Message_StringID(clientMessageBlue,SUSPEND_MINION_HAS_AGGRO);
			}
			else
			{
				m_suspendedminion.SpellID = SpellID;

				m_suspendedminion.HP = CurrentPet->GetHP();

				m_suspendedminion.Mana = CurrentPet->GetMana();
				m_suspendedminion.petpower = CurrentPet->GetPetPower();
				m_suspendedminion.focusItemId = CurrentPet->GetPetFocusItemID();
				m_suspendedminion.size = CurrentPet->GetSize();

				if(AALevel >= 2)
					CurrentPet->GetPetState(m_suspendedminion.Buffs, m_suspendedminion.Items, m_suspendedminion.Name);
				else
					strn0cpy(m_suspendedminion.Name, CurrentPet->GetName(), 64); // Name stays even at rank 1
				EntityList::RemoveNumbers(m_suspendedminion.Name);

				Message_StringID(clientMessageTell, SUSPEND_MINION_SUSPEND, CurrentPet->GetCleanName());

				CurrentPet->Depop(false);

				SetPetID(0);
			}
		}
		else
		{
			Message_StringID(clientMessageError, ONLY_SUMMONED_PETS);

			return;
		}
	}
}

void Client::CheckEmoteHail(Mob *target, const char* message)
{
	if(
		(message[0] != 'H'	&&
		message[0] != 'h')	||
		message[1] != 'a'	||
		message[2] != 'i'	||
		message[3] != 'l'){
		return;
	}

	if(!target || !target->IsNPC()) {
		return;
	}

	if(target->GetOwnerID() != 0) {
		return;
	}
	uint32 emoteid = target->GetEmoteID();
	if (emoteid != 0) {
		target->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::Hailed, emoteid, this);
	}
}

void Client::SendZonePoints()
{
	int count = 0;
	LinkedListIterator<ZonePoint*> iterator(zone->zone_point_list);
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* data = iterator.GetData();
		if(ClientVersionBit() & data->client_version_mask)
		{
			count++;
		}
		iterator.Advance();
	}

	uint32 zpsize = sizeof(ZonePoints) + ((count + 1) * sizeof(ZonePoint_Entry));
	auto outapp = new EQApplicationPacket(OP_SendZonepoints, zpsize);
	ZonePoints* zp = (ZonePoints*)outapp->pBuffer;
	zp->count = count;

	int i = 0;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* data = iterator.GetData();
		if(ClientVersionBit() & data->client_version_mask)
		{
			zp->zpe[i].iterator = data->number;
			zp->zpe[i].x = data->target_x;
			zp->zpe[i].y = data->target_y;
			zp->zpe[i].z = data->target_z;
			zp->zpe[i].heading = data->target_heading;
			zp->zpe[i].zoneid = data->target_zone_id;
			i++;
		}
		iterator.Advance();
	}
	FastQueuePacket(&outapp);
}

void Client::SendTargetCommand(uint32 EntityID)
{
	auto outapp = new EQApplicationPacket(OP_TargetCommand, sizeof(ClientTarget_Struct));
	ClientTarget_Struct *cts = (ClientTarget_Struct*)outapp->pBuffer;
	cts->new_target = EntityID;
	FastQueuePacket(&outapp);
}

void Client::LocateCorpse()
{
	Corpse *ClosestCorpse = nullptr;
	if(!GetTarget())
		ClosestCorpse = entity_list.GetClosestCorpse(this, nullptr);
	else if(GetTarget()->IsCorpse())
		ClosestCorpse = entity_list.GetClosestCorpse(this, GetTarget()->CastToCorpse()->GetOwnerName());
	else
		ClosestCorpse = entity_list.GetClosestCorpse(this, GetTarget()->GetCleanName());

	if(ClosestCorpse)
	{
		Message_StringID(Chat::Spells, SENSE_CORPSE_DIRECTION);
		SetHeading(CalculateHeadingToTarget(ClosestCorpse->GetX(), ClosestCorpse->GetY()));
		SetTarget(ClosestCorpse);
		SendTargetCommand(ClosestCorpse->GetID());
		SendPosUpdate(2);
	}
	else if(!GetTarget())
		Message_StringID(clientMessageError, SENSE_CORPSE_NONE);
	else
		Message_StringID(clientMessageError, SENSE_CORPSE_NOT_NAME);
}

void Client::NPCSpawn(NPC *target_npc, const char *identifier, uint32 extra)
{
	if (!target_npc || !identifier)
		return;

	std::string spawn_type = Strings::ToLower(identifier);
	bool is_add = spawn_type.find("add") != std::string::npos;
	bool is_create = spawn_type.find("create") != std::string::npos;
	bool is_delete = spawn_type.find("delete") != std::string::npos;
	bool is_remove = spawn_type.find("remove") != std::string::npos;
	bool is_update = spawn_type.find("update") != std::string::npos;
	if (is_add || is_create) {
		// Add: extra tries to create the NPC ID within the range for the current Zone (Zone ID * 1000)
		// Create: extra sets the Respawn Timer for add
		database.NPCSpawnDB(is_add ? NPCSpawnTypes::AddNewSpawngroup : NPCSpawnTypes::CreateNewSpawn, zone->GetShortName(), this, target_npc->CastToNPC(), extra);
	}
	else if (is_delete || is_remove || is_update) {
		uint8 spawn_update_type = (is_delete ? NPCSpawnTypes::DeleteSpawn : (is_remove ? NPCSpawnTypes::RemoveSpawn : NPCSpawnTypes::UpdateAppearance));
		database.NPCSpawnDB(spawn_update_type, zone->GetShortName(), this, target_npc->CastToNPC());
	}
}

bool Client::IsDraggingCorpse(uint16 CorpseID)
{
	for (auto It = DraggedCorpses.begin(); It != DraggedCorpses.end(); ++It) {
		if (It->second == CorpseID)
			return true;
	}

	return false;
}

void Client::DragCorpses()
{
	for (auto It = DraggedCorpses.begin(); It != DraggedCorpses.end(); ++It) {
		Mob *corpse = entity_list.GetMob(It->second);

		if (corpse && corpse->IsPlayerCorpse() &&
				(DistanceSquaredNoZ(m_Position, corpse->GetPosition()) <= RuleR(Character, DragCorpseDistance)))
			continue;

		if (!corpse || !corpse->IsPlayerCorpse() ||
				corpse->CastToCorpse()->IsBeingLooted() ||
				!corpse->CastToCorpse()->Summon(this, false, false)) {
			Message_StringID(Chat::DefaultText, CORPSEDRAG_STOP);
			It = DraggedCorpses.erase(It);
		}
	}
}

// this is a TAKP enhancement that tries to give some leeway for /corpse drag range when the player is moving while dragging
void Client::CorpseSummoned(Corpse *corpse)
{
	uint16 corpse_summon_id = 0;
	Timer *corpse_summon_timer = nullptr;

	// it's possible to use hotkeys to rapidly target and drag multiple corpses
	for(auto &pair : corpse_summon_timers)
	{
		if (pair.first == corpse->GetID())
		{
			corpse_summon_id = pair.first;
			corpse_summon_timer = pair.second;
			break;
		}
	}

	if (corpse_summon_id != 0)
	{
		// there's a timer for this corpse and it hasn't expired yet, check if it was summoned to the same spot again.
		// when the client is only sending position updates 1 second apart and /corpse is being spammed, the corpse will get
		// summoned to the player's last known location repeatedly - this is detected here.  the next time a position update is
		// received all the corpses in the corpse_summon_on_next_update vector get summoned to the player.
		// it is not expected that more then 1 or 2 corpses are being summoned within the timeout window, but with macros
		// it's possible to drag multiple corpses this way and this allows for it
		if (corpse_summon_timer->Enabled() && !corpse_summon_timer->Check(false))
		{
			// this has to be called before the corpse is actually moved in Corpse::Summoned()
			if (glm::vec3(corpse->GetX(), corpse->GetY(), corpse->GetZ()) == glm::vec3(GetX(), GetY(), GetZ()))
			{
				// double summon to same location, enable summon on update for this corpse, if not already added
				bool exists = false;
				for (auto id : corpse_summon_on_next_update)
				{
					if (id == corpse_summon_id)
					{
						exists = true;
						break;
					}
				}
				if (!exists)
				{
					corpse_summon_on_next_update.push_back(corpse_summon_id);
				}
				//Message(MT_Broadcasts, "corpse_summon_on_next_update armed corpse_id %hu corpse_summon_on_next_update size %d", corpse_summon_id, corpse_summon_on_next_update.size());
			}
		}
	}
	else
	{
		// create a timer for this corpse
		corpse_summon_id = corpse->GetID();
		corpse_summon_timer = new Timer();
		corpse_summon_timers.push_back(std::make_pair(corpse_summon_id, corpse_summon_timer));
	}

	// this is the timeout for tracking if /corpse is being spammed between position updates
	corpse_summon_timer->Start(1500);
}

void Client::CorpseSummonOnPositionUpdate()
{
	// process any delayed corpse summons on client position update
	if (corpse_summon_on_next_update.size() > 0)
	{
		for (auto corpse_summon_id : corpse_summon_on_next_update)
		{
			Timer *corpse_summon_timer = nullptr;
			
			// find timer for this corpse
			for(auto &pair : corpse_summon_timers)
			{
				if (corpse_summon_id == pair.first)
				{
					corpse_summon_timer = pair.second;
					break;
				}
			}

			// check that the timer hasn't expired
			if (corpse_summon_timer != nullptr && corpse_summon_timer->Enabled() && !corpse_summon_timer->Check(false))
			{
				Corpse *corpse = entity_list.GetCorpseByID(corpse_summon_id);

				if (corpse)
				{
					// limit total distance from corpse, just to be conservative in case this gets used with teleports or something
					float dist = Distance(GetPosition(), corpse->GetPosition());
					//Message(MT_Broadcasts, "CorpseSummonOnPositionUpdate dist %0.2f %hu time left %d", dist, corpse_summon_id, corpse_summon_timer->GetRemainingTime());
					if (dist < 100.0f)
					{
						// this ends up calling CorpseSummoned again so the timer is reset too
						corpse->Summon(this, false, false);
					}
				}
			}
		}

		corpse_summon_on_next_update.clear();
	}

	// check for expired timers and delete them
	if (corpse_summon_timers.size() > 0)
	{
		for (auto it = std::begin(corpse_summon_timers); it != std::end(corpse_summon_timers);)
		{
			Timer *timer = it->second;
			if (timer && (!timer->Enabled() || timer->Check(false)))
			{
				it = corpse_summon_timers.erase(it);
				safe_delete(timer);
			}
			else
			{
				it++;
			}
		}
	}
}


void Client::Doppelganger(uint16 spell_id, Mob *target, const char *name_override, int pet_count, int pet_duration)
{
	if(!target || !IsValidSpell(spell_id) || this->GetID() == target->GetID())
		return;

	PetRecord record;
	if(!database.GetPetEntry(spells[spell_id].teleport_zone, &record))
	{
		Log(Logs::General, Logs::Error, "Unknown doppelganger spell id: %d, check pets table", spell_id);
		Message(Chat::Red, "Unable to find data for pet %s", spells[spell_id].teleport_zone);
		return;
	}

	SwarmPet_Struct pet = {};
	pet.count = pet_count;
	pet.duration = pet_duration;
	pet.npc_id = record.npc_type;

	NPCType *made_npc = nullptr;

	const NPCType *npc_type = database.LoadNPCTypesData(pet.npc_id);
	if(npc_type == nullptr) {
		Log(Logs::General, Logs::Error, "Unknown npc type for doppelganger spell id: %d", spell_id);
		Message(Chat::White,"Unable to find pet!");
		return;
	}
	// make a custom NPC type for this
	made_npc = new NPCType;
	memcpy(made_npc, npc_type, sizeof(NPCType));

	strcpy(made_npc->name, name_override);
	made_npc->level = GetLevel();
	made_npc->race = GetRace();
	made_npc->gender = GetGender();
	made_npc->size = GetSize();
	made_npc->AC = GetAC();
	made_npc->STR = GetSTR();
	made_npc->STA = GetSTA();
	made_npc->DEX = GetDEX();
	made_npc->AGI = GetAGI();
	made_npc->MR = GetMR();
	made_npc->FR = GetFR();
	made_npc->CR = GetCR();
	made_npc->DR = GetDR();
	made_npc->PR = GetPR();
	// looks
	made_npc->texture = GetEquipmentMaterial(EQ::textures::armorChest);
	made_npc->helmtexture = GetEquipmentMaterial(EQ::textures::armorHead);
	made_npc->haircolor = GetHairColor();
	made_npc->beardcolor = GetBeardColor();
	made_npc->eyecolor1 = GetEyeColor1();
	made_npc->eyecolor2 = GetEyeColor2();
	made_npc->hairstyle = GetHairStyle();
	made_npc->luclinface = GetLuclinFace();
	made_npc->beard = GetBeard();
	made_npc->d_melee_texture1 = GetEquipmentMaterial(EQ::textures::weaponPrimary);
	made_npc->d_melee_texture2 = GetEquipmentMaterial(EQ::textures::weaponSecondary);
	for (int i = EQ::textures::textureBegin; i <= EQ::textures::LastTexture; i++)	{
		made_npc->armor_tint.Slot[i].Color = GetEquipmentColor(i);
	}
	made_npc->loottable_id = 0;

	int summon_count = pet.count;

	if(summon_count > MAX_SWARM_PETS)
		summon_count = MAX_SWARM_PETS;

	static const glm::vec2 swarmPetLocations[MAX_SWARM_PETS] = {
		glm::vec2(5, 5), glm::vec2(-5, 5), glm::vec2(5, -5), glm::vec2(-5, -5),
		glm::vec2(10, 10), glm::vec2(-10, 10), glm::vec2(10, -10), glm::vec2(-10, -10),
		glm::vec2(8, 8), glm::vec2(-8, 8), glm::vec2(8, -8), glm::vec2(-8, -8)
	};

	while(summon_count > 0) {
		auto npc_type_copy = new NPCType;
		memcpy(npc_type_copy, made_npc, sizeof(NPCType));

		NPC* swarm_pet_npc = new NPC(
				npc_type_copy,
				0,
				GetPosition() + glm::vec4(swarmPetLocations[summon_count - 1], 0.0f, 0.0f),
				GravityBehavior::Water);

		if(!swarm_pet_npc->GetSwarmInfo()){
			auto nSI = new SwarmPet;
			swarm_pet_npc->SetSwarmInfo(nSI);
			swarm_pet_npc->GetSwarmInfo()->duration = new Timer(pet_duration*1000);
		}
		else{
			swarm_pet_npc->GetSwarmInfo()->duration->Start(pet_duration*1000);
		}

		swarm_pet_npc->StartSwarmTimer(pet_duration * 1000);

		swarm_pet_npc->GetSwarmInfo()->owner_id = GetID();

		// Give the pets alittle more agro than the caster and then agro them on the target
		target->AddToHateList(swarm_pet_npc, (target->GetHateAmount(this) + 100), (target->GetDamageAmount(this) + 100));
		swarm_pet_npc->AddToHateList(target, 1000, 1000);
		swarm_pet_npc->GetSwarmInfo()->target = target->GetID();

		entity_list.AddNPC(swarm_pet_npc);
		summon_count--;
	}

	safe_delete(made_npc);
}

void Client::SendStats(Client* client)
{

	int shield_ac = 0;
	GetRawACNoShield(shield_ac);

	std::string state = "Alive";
	if (IsUnconscious())
		state = "Unconscious";
	else if (IsDead())
		state = "Dead";

	client->Message(Chat::Yellow, "~~~~~ %s %s ~~~~~", GetCleanName(), GetLastName());
	client->Message(Chat::White, " Level: %i Class: %i Race: %i RaceBit: %i Size: %1.1f ModelSize: %1.1f BaseSize: %1.1f Weight: %d/%d  ", GetLevel(), GetClass(), GetRace(), GetPlayerRaceBit(GetRace()), GetSize(), GetModelSize(), GetBaseSize(), uint16(CalcCurrentWeight() / 10.0), GetSTR());
	client->Message(Chat::White, " AAs: %i  HP: %i/%i  Per: %0.2f HP Regen: %i/%i State: %s", GetAAPoints() + GetSpentAA(), GetHP(), GetMaxHP(), GetHPRatio(), CalcHPRegen(), CalcHPRegenCap(), state.c_str());
	client->Message(Chat::White, " AC: %i (Mit.: %i/%i + Avoid.: %i/%i) | Item AC: %i  Buff AC: %i  Shield AC: %i  DS: %i", CalcAC(), GetMitigation(), GetMitigation(true), GetAvoidance(true), GetAvoidance(), itembonuses.AC, spellbonuses.AC, shield_ac, GetDS());
	client->Message(Chat::White, " Offense: %i  To-Hit: %i  Displayed ATK: %i  Worn +ATK: %i (cap: %i)  Spell +ATK: %i  Dmg Bonus: %i", GetOffenseByHand(), GetToHitByHand(), GetATK(), GetATKBonusItem(), RuleI(Character, ItemATKCap), GetATKBonusSpell(), GetDamageBonus());
	client->Message(Chat::White, " Haste: %i (cap %i) (Item: %i + Spell: %i + Over: %i)  Double Attack: %i%%  Dual Wield: %i%%", GetHaste(), GetHasteCap(), itembonuses.haste, spellbonuses.haste + spellbonuses.hastetype2, spellbonuses.hastetype3 + ExtraHaste, GetDoubleAttackChance(), GetDualWieldChance());
	client->Message(Chat::White, " AFK: %i LFG: %i Anon: %i PVP: %i LD: %i Client: %s Mule: %i ", AFK, LFG, GetAnon(), GetPVP(), IsLD(), EQ::versions::ClientVersionName(EQ::versions::ConvertClientVersionBitToClientVersion(ClientVersionBit())), IsMule());
	client->Message(Chat::White, " GM: %i Flymode: %i GMSpeed: %i Hideme: %i GMInvul: %d TellsOff: %i ", GetGM(), flymode, GetGMSpeed(), GetHideMe(), GetGMInvul(), tellsoff);
	if(CalcMaxMana() > 0)
		client->Message(Chat::White, " Mana: %i/%i  Mana Regen: %i/%i", GetMana(), GetMaxMana(), CalcManaRegen(), CalcManaRegenCap());
	client->Message(Chat::White, "  X: %0.2f Y: %0.2f Z: %0.2f", GetX(), GetY(), GetZ());
	client->Message(Chat::White, "  InWater: %d UnderWater: %d Air: %d WaterBreathing: %d Lung Capacity: %d", IsInWater(), IsUnderWater(), m_pp.air_remaining, spellbonuses.WaterBreathing || aabonuses.WaterBreathing || itembonuses.WaterBreathing, aabonuses.BreathLevel > 0 ? aabonuses.BreathLevel : 100);
	client->Message(Chat::White, " STR: %i  STA: %i  AGI: %i DEX: %i  WIS: %i INT: %i  CHA: %i", GetSTR(), GetSTA(), GetAGI(), GetDEX(), GetWIS(), GetINT(), GetCHA());
	client->Message(Chat::White, " PR: %i MR: %i  DR: %i FR: %i  CR: %i  ", GetPR(), GetMR(), GetDR(), GetFR(), GetCR());
	client->Message(Chat::White, " Shielding: %i  Spell Shield: %i  DoT Shielding: %i Stun Resist: %i  Strikethrough: %i  Accuracy: %i", GetShielding(), GetSpellShield(), GetDoTShield(), GetStunResist(), GetStrikeThrough(), GetAccuracy());
	client->Message(Chat::White, " Hunger: %i Thirst: %i IsFamished: %i Rested: %i Drunk: %i", GetHunger(), GetThirst(), Famished(), IsRested(), m_pp.intoxication);
	client->Message(Chat::White, " Runspeed: %0.1f  Walkspeed: %0.1f Encumbered: %i", GetRunspeed(), GetWalkspeed(), IsEncumbered());
	client->Message(Chat::White, " Boat: %s (EntityID %i : NPCID %i)", GetBoatName(), GetBoatID(), GetBoatNPCID());
	if(GetClass() == PALADIN || GetClass() == SHADOWKNIGHT)
		client->Message(Chat::White, "HasShield: %i HasBashEnablingWeapon: %i", HasShieldEquiped(), HasBashEnablingWeapon());
	else
		client->Message(Chat::White, "HasShield: %i", HasShieldEquiped());
	if(GetClass() == BARD)
		client->Message(Chat::White, " Singing: %i  Brass: %i  String: %i Percussion: %i Wind: %i", GetSingMod(), GetBrassMod(), GetStringMod(), GetPercMod(), GetWindMod());
	if(HasGroup())
	{
		Group* g = GetGroup();
		client->Message(Chat::White, " GroupID: %i Count: %i GroupLeader: %s GroupLeaderCached: %s", g->GetID(), g->GroupCount(), g->GetLeaderName(), g->GetOldLeaderName());
	}
	client->Message(Chat::White, " Hidden: %i ImpHide: %i Sneaking: %i Invisible: %i InvisVsUndead: %i InvisVsAnimals: %i", hidden, improved_hidden, sneaking, invisible, invisible_undead, invisible_animals);
	client->Message(Chat::White, " Feigned: %i Invulnerable: %i SeeInvis: %i HasZomm: %i Disc: %i/%i", feigned, invulnerable, SeeInvisible(), has_zomm, GetActiveDisc(), GetActiveDiscSpell());

	client->Message(Chat::White, "Extra_Info:");

	client->Message(Chat::White, " BaseRace: %i  Gender: %i  BaseGender: %i Texture: %i  HelmTexture: %i", GetBaseRace(), GetGender(), GetBaseGender(), GetTexture(), GetHelmTexture());
	if (client->Admin() >= AccountStatus::GMAdmin) {
		client->Message(Chat::White, "  CharID: %i  EntityID: %i  AccountID: %i PetID: %i  OwnerID: %i  AIControlled: %i  Targetted: %i Zone Count: %i", CharacterID(), GetID(), AccountID(), GetPetID(), GetOwnerID(), IsAIControlled(), targeted, GetZoneChangeCount());
		uint32 xpneeded = GetEXPForLevel(GetLevel()+1) - GetEXP();
		int exploss;
		GetExpLoss(nullptr,0,exploss);
		if(GetLevel() < 10)
			exploss = 0;
		client->Message(Chat::White, "  CurrentXP: %i XP Needed: %i ExpLoss: %i CurrentAA: %i", GetEXP(), xpneeded, exploss, GetAAXP());
		client->Message(Chat::White, "  MerchantSession: %i TraderSession: %i Trader: %i WithCustomer: %i", MerchantSession, TraderSession, Trader, WithCustomer);
	}
}

void Client::SendQuickStats(Client* client)
{
	client->Message(Chat::Yellow, "~~~~~ %s %s ~~~~~", GetCleanName(), GetLastName());
	client->Message(Chat::White, " Level: %i Class: %i Race: %i Size: %1.1f ModelSize: %1.1f BaseSize: %1.1f Weight: %d/%d  ", GetLevel(), GetClass(), GetRace(), GetSize(), GetModelSize(), GetBaseSize(), uint16(CalcCurrentWeight() / 10.0), GetSTR());
	client->Message(Chat::White, " HP: %i/%i Per %0.2f HP Regen: %i/%i ",GetHP(), GetMaxHP(), GetHPRatio(), CalcHPRegen(), CalcHPRegenCap());
	if(CalcMaxMana() > 0)
		client->Message(Chat::White, " Mana: %i/%i  Mana Regen: %i/%i", GetMana(), GetMaxMana(), CalcManaRegen(), CalcManaRegenCap());
	client->Message(Chat::White, " AC: %i ATK: %i Haste: %i / %i", CalcAC(), GetATK(), GetHaste(), RuleI(Character, HasteCap));
	client->Message(Chat::White, " STR: %i  STA: %i  AGI: %i DEX: %i  WIS: %i INT: %i  CHA: %i", GetSTR(), GetSTA(), GetAGI(), GetDEX(), GetWIS(), GetINT(), GetCHA());
	client->Message(Chat::White, " PR: %i MR: %i  DR: %i FR: %i  CR: %i  ", GetPR(), GetMR(), GetDR(), GetFR(), GetCR());
	client->Message(Chat::White, " CharID: %i  EntityID: %i AccountID: %i", CharacterID(), GetID(), AccountID());
}

void Client::DuplicateLoreMessage(uint32 ItemID)
{
	Message_StringID(Chat::White, PICK_LORE);
	return;

	const EQ::ItemData *item = database.GetItem(ItemID);

	if(!item)
		return;

	Message_StringID(Chat::White, PICK_LORE, item->Name);
}

void Client::GarbleMessage(char *message, uint8 variance)
{
	// Garble message by variance%
	const char alpha_list[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; // only change alpha characters for now

	// Don't garble # commands
	if (message[0] == '#')
		return;

	for (size_t i = 0; i < strlen(message); i++) {
		uint8 chance = (uint8)zone->random.Int(0, 115); // variation just over worst possible scrambling
		if (isalpha((unsigned char)message[i]) && (chance <= variance)) {
			uint8 rand_char = (uint8)zone->random.Int(0,51); // choose a random character from the alpha list
			message[i] = alpha_list[rand_char];
		}
	}
}

// returns what Other thinks of this
FACTION_VALUE Client::GetReverseFactionCon(Mob* iOther) {
	if (GetOwnerID()) {
		return GetOwnerOrSelf()->GetReverseFactionCon(iOther);
	}

	iOther = iOther->GetOwnerOrSelf();

	if (iOther->GetPrimaryFaction() < 0)
		return GetSpecialFactionCon(iOther);

	if (iOther->GetPrimaryFaction() == 0)
		return FACTION_INDIFFERENTLY;

	return GetFactionLevel(CharacterID(), GetRace(), GetClass(), GetDeity(), iOther->GetPrimaryFaction(), iOther);
}

//o--------------------------------------------------------------
//| Name: GetFactionLevel; Dec. 16, 2001
//o--------------------------------------------------------------
//| Notes: Gets the characters faction standing with the specified NPC.
//|			Will return Indifferent on failure.
//o--------------------------------------------------------------
FACTION_VALUE Client::GetFactionLevel(uint32 char_id, uint32 p_race, uint32 p_class, uint32 p_deity, int32 pFaction, Mob* tnpc, bool lua)
{
	if (pFaction < 0)
		return GetSpecialFactionCon(tnpc);
	FACTION_VALUE fac = FACTION_INDIFFERENTLY;
	int32 tmpFactionValue;
	FactionMods fmods;

	// few optimizations
	if (IsFeigned() && tnpc && !tnpc->GetSpecialAbility(IMMUNE_FEIGN_DEATH))
		return FACTION_INDIFFERENTLY;
	if (!zone->CanDoCombat())
		return FACTION_INDIFFERENTLY;
	if (invisible_undead && tnpc && !tnpc->SeeInvisibleUndead())
		return FACTION_INDIFFERENTLY;
	if (IsInvisible(tnpc))
		return FACTION_INDIFFERENTLY;
	if (tnpc && tnpc->GetOwnerID() != 0) // pets con amiably to owner and indiff to rest
	{
		if (tnpc->GetOwner() && tnpc->GetOwner()->IsClient() && char_id == tnpc->GetOwner()->CastToClient()->CharacterID())
			return FACTION_AMIABLY;
		else
			return FACTION_INDIFFERENTLY;
	}

	//First get the NPC's Primary faction
	if(pFaction > 0)
	{
		//Get the faction data from the database
		if(database.GetFactionData(&fmods, p_class, p_race, p_deity, pFaction, GetTexture(), GetGender(), GetBaseRace()))
		{
			//Get the players current faction with pFaction
			tmpFactionValue = GetCharacterFactionLevel(pFaction);
			//Tack on any bonuses from Alliance type spell effects
			tmpFactionValue += GetFactionBonus(pFaction);
			tmpFactionValue += GetItemFactionBonus(pFaction);
			//Return the faction to the client
			fac = CalculateFaction(&fmods, tmpFactionValue, lua);
		}
	}
	else
	{
		return(FACTION_INDIFFERENTLY);
	}

	// merchant fix
	if (tnpc && tnpc->IsNPC() && tnpc->CastToNPC()->MerchantType && (fac == FACTION_THREATENINGLY || fac == FACTION_SCOWLS))
		fac = FACTION_DUBIOUSLY;

	// We're engaged with the NPC and their base is dubious or higher, return threatenly
	if (tnpc != 0 && fac != FACTION_SCOWLS && tnpc->CastToNPC()->CheckAggro(this))
		fac = FACTION_THREATENINGLY;

	return fac;
}

int16 Client::GetFactionValue(Mob* tnpc)
{
	int16 tmpFactionValue;
	FactionMods fmods;

	if (IsFeigned() || IsInvisible(tnpc))
		return 0;

	// pets con amiably to owner and indiff to rest
	if (tnpc && tnpc->GetOwnerID() != 0)
	{
		if (tnpc->GetOwner() && tnpc->GetOwner()->IsClient() && CharacterID() == tnpc->GetOwner()->CastToClient()->CharacterID())
			return 100;
		else
			return 0;
	}

	//First get the NPC's Primary faction
	int32 pFaction = tnpc->GetPrimaryFaction();
	if (pFaction > 0)
	{
		//Get the faction data from the database
		if (database.GetFactionData(&fmods, GetClass(), GetRace(), GetDeity(), pFaction, GetTexture(), GetGender(), GetBaseRace()))
		{
			//Get the players current faction with pFaction
			tmpFactionValue = GetCharacterFactionLevel(pFaction);
			//Tack on any bonuses from Alliance type spell effects
			tmpFactionValue += GetFactionBonus(pFaction);
			tmpFactionValue += GetItemFactionBonus(pFaction);
			//Add base mods, GetFactionData() above also accounts for illusions.
			tmpFactionValue += fmods.base + fmods.class_mod + fmods.race_mod + fmods.deity_mod;
		}
	}
	else
	{
		return 0;
	}

	// merchant fix
	if (tnpc && tnpc->IsNPC() && tnpc->CastToNPC()->MerchantType && tmpFactionValue <= -501)
		return -500;

	// We're engaged with the NPC and their base is dubious or higher, return threatenly
	if (tnpc != 0 && tmpFactionValue >= -500 && tnpc->CastToNPC()->CheckAggro(this))
		return -501;

	return tmpFactionValue;
}

//Sets the characters faction standing with the specified NPC.
void Client::SetFactionLevel(uint32 char_id, uint32 npc_id, bool quest)
{
	int32 faction_id[MAX_NPC_FACTIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int32 npc_value[MAX_NPC_FACTIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	uint8 temp[MAX_NPC_FACTIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	// Get the npc faction list
	if (!database.GetNPCFactionList(npc_id, faction_id, npc_value, temp))
		return;
	for (int i = 0; i < MAX_NPC_FACTIONS; i++)
	{
		if (faction_id[i] <= 0)
			continue;

		if (quest)
		{
			//The ole switcheroo
			if (npc_value[i] > 0)
				npc_value[i] = -abs(npc_value[i]);
			else if (npc_value[i] < 0)
				npc_value[i] = abs(npc_value[i]);
		}

		SetFactionLevel2(char_id, faction_id[i], npc_value[i], temp[i]);
	}
}

// This is the primary method used by Lua and #giveplayerfaction. SetFactionLevel() which hands out faction on a NPC death also resolves to this method.
void Client::SetFactionLevel2(uint32 char_id, int32 faction_id, int32 value, uint8 temp)
{
	if(faction_id > 0 && value != 0) 
	{
		UpdatePersonalFaction(char_id, value, faction_id, temp, false);
	}
}

// Gets the client personal faction value
int32 Client::GetCharacterFactionLevel(int32 faction_id)
{
	int32 faction = 0;
	faction_map::iterator res;
	res = factionvalues.find(faction_id);
	if (res == factionvalues.end())
		return 0;
	faction = res->second;

	Log(Logs::Detail, Logs::Faction, "%s has %d personal faction with %d", GetName(), faction, faction_id);
	return faction;
}

// Modifies client personal faction by npc_value, which is the faction hit value that can be positive or negative
// Personal faction is a the raw faction value before any modifiers and starts at 0 for everything and is almost always a value between -2000 and 2000
// optionally sends a faction gain/loss message to the client (default is true)
int32 Client::UpdatePersonalFaction(int32 char_id, int32 npc_value, int32 faction_id, int32 temp, bool skip_gm, bool show_msg)
{
	int32 hit = npc_value;
	uint32 msg = FACTION_BETTER;
	int32 current_value = GetCharacterFactionLevel(faction_id);
	int32 unadjusted_value = current_value;

	if (GetGM() && skip_gm)
		return 0;

	if (hit != 0)
	{	
		if (RuleB(Quarm, ClientFactionOverride))
		{
			const float factionMultiplier = RuleR(Quarm, ClientFactionMultiplier);
			
			// We should not support sign flipping or zeroing out faction as this could have undetermined ramifications
			if (factionMultiplier > 0)
			{
				Log(Logs::Detail, Logs::Faction, "Faction Multiplier %0.2f applied to faction %d for %s.", factionMultiplier, faction_id, GetName());
				hit *= factionMultiplier;
			}
		}
		
		int16 min_personal_faction = database.MinFactionCap(faction_id);
		int16 max_personal_faction = database.MaxFactionCap(faction_id);
		int32 personal_faction = GetCharacterFactionLevel(faction_id);

		if (hit < 0)
		{
			if (personal_faction <= min_personal_faction)
			{
				msg = FACTION_WORST;
				hit = 0;
			}
			else
			{
				msg = FACTION_WORSE;
				if (personal_faction + hit < min_personal_faction)
				{
					hit = min_personal_faction - personal_faction;
				}
			}
		}
		else // hit > 0
		{
			if (personal_faction >= max_personal_faction)
			{
				msg = FACTION_BEST;
				hit = 0;
			}
			else
			{
				msg = FACTION_BETTER;
				if (personal_faction + hit > max_personal_faction)
				{
					hit = max_personal_faction - personal_faction;
				}
			}
		}

		if (hit)
		{
			current_value += hit;
			database.SetCharacterFactionLevel(char_id, faction_id, current_value, temp, factionvalues);
			Log(Logs::General, Logs::Faction, "Adding %d to faction %d for %s. New personal value is %d, old personal value was %d.", hit, faction_id, GetName(), current_value, unadjusted_value);
		}
		else
			Log(Logs::General, Logs::Faction, "Faction %d will not be updated for %s. Personal faction is capped at %d.", faction_id, GetName(), personal_faction);
	}

	if (show_msg && npc_value && temp != 1 && temp != 2)
	{
		char name[50];
		// default to Faction# if we couldn't get the name from the ID
		if (database.GetFactionName(faction_id, name, sizeof(name)) == false)
			snprintf(name, sizeof(name), "Faction%i", faction_id);

		Message_StringID(Chat::White, msg, name);
	}

	return hit;
}

// returns the character's faction level, adjusted for racial, class, and deity modifiers
int32 Client::GetModCharacterFactionLevel(int32 faction_id, bool skip_illusions) 
{
	int32 Modded = GetCharacterFactionLevel(faction_id);
	FactionMods fm;
	if (database.GetFactionData(&fm, GetClass(), GetRace(), GetDeity(), faction_id, GetTexture(), GetGender(), GetBaseRace(), skip_illusions))
		Modded += fm.base + fm.class_mod + fm.race_mod + fm.deity_mod;

	return Modded;
}

void Client::MerchantRejectMessage(Mob *merchant, int primaryfaction)
{
	int messageid = 0;
	int32 tmpFactionValue = 0;
	int32 lowestvalue = 0;
	FactionMods fmod;

	// If a faction is involved, get the data.
	if (primaryfaction > 0) {
		if (database.GetFactionData(&fmod, GetClass(), GetRace(), GetDeity(), primaryfaction, GetTexture(), GetGender(), GetBaseRace())) {
			tmpFactionValue = GetCharacterFactionLevel(primaryfaction);
			lowestvalue = std::min(tmpFactionValue, std::min(fmod.class_mod, fmod.race_mod));
		}
	}
	// If no primary faction or biggest influence is your faction hit
	// Hack to get Shadowhaven messages correct :I
	if (GetZoneID() != Zones::SHADOWHAVEN && (primaryfaction <= 0 || lowestvalue == tmpFactionValue)) 
	{
		merchant->Say_StringID(zone->random.Int(WONT_SELL_DEEDS1, WONT_SELL_DEEDS6));
	} 
	//class biggest
	else if (lowestvalue == fmod.class_mod) 
	{
		merchant->Say_StringID(0, zone->random.Int(WONT_SELL_CLASS1, WONT_SELL_CLASS4), itoa(GetClassStringID()));
	}
	// race biggest/default
	else
	{ 
		// Non-standard race (ex. illusioned to wolf)
		if (GetRace() > GNOME && GetRace() != IKSAR && GetRace() != VAHSHIR)
		{
			messageid = zone->random.Int(1, 3); // these aren't sequential StringIDs :(
			switch (messageid) {
			case 1:
				messageid = WONT_SELL_NONSTDRACE1;
				break;
			case 2:
				messageid = WONT_SELL_NONSTDRACE2;
				break;
			case 3:
				messageid = WONT_SELL_NONSTDRACE3;
				break;
			default: // w/e should never happen
				messageid = WONT_SELL_NONSTDRACE1;
				break;
			}
			merchant->Say_StringID(messageid);
		} 
		else 
		{ // normal player races
			messageid = zone->random.Int(1, 5);
			switch (messageid) {
			case 1:
				messageid = WONT_SELL_RACE1;
				break;
			case 2:
				messageid = WONT_SELL_RACE2;
				break;
			case 3:
				messageid = WONT_SELL_RACE3;
				break;
			case 4:
				messageid = WONT_SELL_RACE4;
				break;
			case 5:
				messageid = WONT_SELL_RACE5;
				break;
			default: // w/e should never happen
				messageid = WONT_SELL_RACE1;
				break;
			}
			merchant->Say_StringID(0, messageid, itoa(GetRaceStringID()));
		}
	}
	return;
}

void Client::LoadAccountFlags()
{

	accountflags.clear();
	std::string query = StringFormat("SELECT p_flag, p_value "
                                    "FROM account_flags WHERE p_accid = '%d'",
                                    account_id);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

	for (auto row = results.begin(); row != results.end(); ++row)
		accountflags[row[0]] = row[1];
}

void Client::SetAccountFlag(std::string flag, std::string val) {

	std::string query = StringFormat("REPLACE INTO account_flags (p_accid, p_flag, p_value) "
									"VALUES( '%d', '%s', '%s')",
									account_id, flag.c_str(), val.c_str());
	auto results = database.QueryDatabase(query);
	if(!results.Success()) {
		return;
	}

	accountflags[flag] = val;
}

std::string Client::GetAccountFlag(std::string flag)
{
	return(accountflags[flag]);
}

void Client::TickItemCheck()
{
	int i;

	if(zone->tick_items.empty()) { return; }

	//Scan equip slots for items  + cursor
	for(i = EQ::invslot::slotCursor; i <= EQ::invslot::EQUIPMENT_END; i++)
	{
		TryItemTick(i);
	}
	//Scan Slot inventory
	for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		TryItemTick(i);
	}
	//Scan bags
	for(i = EQ::invbag::GENERAL_BAGS_BEGIN; i <= EQ::invbag::CURSOR_BAG_END; i++)
	{
		TryItemTick(i);
	}
}

void Client::TryItemTick(int slot)
{
	int iid = 0;
	const EQ::ItemInstance* inst = m_inv[slot];
	if(inst == 0) { return; }

	iid = inst->GetID();

	if(zone->tick_items.count(iid) > 0)
	{
		if( GetLevel() >= zone->tick_items[iid].level && zone->random.Int(0, 100) >= (100 - zone->tick_items[iid].chance) && (zone->tick_items[iid].bagslot || slot <= EQ::invslot::EQUIPMENT_END) )
		{
			EQ::ItemInstance* e_inst = (EQ::ItemInstance*)inst;
			parse->EventItem(EVENT_ITEM_TICK, this, e_inst, nullptr, "", slot);
		}
	}

	if(slot > EQ::invslot::EQUIPMENT_END) { return; }

}

void Client::ItemTimerCheck()
{
	int i;
	for(i = EQ::invslot::slotCursor; i <= EQ::invslot::EQUIPMENT_END; i++)
	{
		TryItemTimer(i);
	}

	for (i = EQ::invslot::GENERAL_BEGIN; i <= EQ::invslot::GENERAL_END; i++)
	{
		TryItemTimer(i);
	}

	for(i = EQ::invbag::GENERAL_BAGS_BEGIN; i <= EQ::invbag::CURSOR_BAG_END; i++)
	{
		TryItemTimer(i);
	}
}

void Client::TryItemTimer(int slot)
{
	EQ::ItemInstance* inst = m_inv.GetItem(slot);
	if(!inst) {
		return;
	}

	auto item_timers = inst->GetTimers();
	auto it_iter = item_timers.begin();
	while(it_iter != item_timers.end()) {
		if(it_iter->second.Check()) {
			parse->EventItem(EVENT_TIMER, this, inst, nullptr, it_iter->first, 0);
		}
		++it_iter;
	}

	if(slot > EQ::invslot::EQUIPMENT_END) {
		return;
	}
}

void Client::RefundAA() {
	int cur = 0;
	bool refunded = false;

	for(int x = 0; x < aaHighestID; x++) {
		cur = GetAA(x);
		if(cur > 0)
		{
			SendAA_Struct* curaa = zone->FindAA(x, false);
			if(cur){
				SetAA(x, 0);
				for (int j = 0; j < cur; j++) {
					if (curaa)
					{
						m_pp.aapoints += curaa->cost + (curaa->cost_inc * j);
						refunded = true;
					}
				}
			}
			else
			{
				m_pp.aapoints += cur;
				SetAA(x, 0);
				refunded = true;
			}
		}
	}

	if(refunded) {
		SaveAA();
		Save();
		Kick();
	}
}

void Client::IncrementAA(int aa_id) {
	SendAA_Struct* aa2 = zone->FindAA(aa_id, false);

	if(aa2 == nullptr)
		return;

	if(GetAA(aa_id) == aa2->max_level)
		return;

	SetAA(aa_id, GetAA(aa_id) + 1);

	SaveAA();

	SendAATable();
	SendAAStats();
	CalcBonuses();
}

void Client::SetHunger(int16 in_hunger)
{
	m_pp.hunger_level = in_hunger > 0 ? (in_hunger < 32000 ? in_hunger : 32000) : 0;
}

void Client::SetThirst(int16 in_thirst)
{
	m_pp.thirst_level = in_thirst > 0 ? (in_thirst < 32000 ? in_thirst : 32000) : 0;
}

void Client::SetConsumption(int16 in_hunger, int16 in_thirst)
{
	m_pp.hunger_level = in_hunger > 0 ? (in_hunger < 32000 ? in_hunger : 32000) : 0;
	m_pp.thirst_level = in_thirst > 0 ? (in_thirst < 32000 ? in_thirst : 32000) : 0;
}

void Client::SetBoatID(uint32 boatid)
{
	m_pp.boatid = boatid;
}

void Client::SetBoatName(const char* boatname)
{
	strncpy(m_pp.boat, boatname, 32);
}

void Client::QuestReward(Mob* target, int32 copper, int32 silver, int32 gold, int32 platinum, int16 itemid, int32 exp, bool faction) 
{
	if (!target)
		return;

	auto outapp = new EQApplicationPacket(OP_Reward, sizeof(QuestReward_Struct));
	memset(outapp->pBuffer, 0, sizeof(QuestReward_Struct));
	QuestReward_Struct* qr = (QuestReward_Struct*)outapp->pBuffer;

	qr->mob_id = target->GetID();		// Entity ID for the from mob name
	qr->target_id = GetID();			// The Client ID (this)
	qr->copper = copper;
	qr->silver = silver;
	qr->gold = gold;
	qr->platinum = platinum;
	qr->item_id[0] = itemid;
	qr->exp_reward = exp;

	if (copper > 0 || silver > 0 || gold > 0 || platinum > 0)
		AddMoneyToPP(copper, silver, gold, platinum, false);

	if (itemid > 0)
		SummonItem(itemid, 0, EQ::legacy::SLOT_QUEST);

	if (faction && !target->IsCharmedPet())
	{
		if (target->IsNPC())
		{
			int32 nfl_id = target->CastToNPC()->GetNPCFactionID();
			SetFactionLevel(CharacterID(), nfl_id, true);
			qr->faction = target->CastToNPC()->GetPrimaryFaction();
			qr->faction_mod = 1; // Too lazy to get real value, not sure if this is even used by client anyhow.
		}
	}

	if (exp > 0)
		AddQuestEXP(exp);

	QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
	safe_delete(outapp);
}

void Client::QuestReward(Mob* target, const QuestReward_Struct& reward)
{
	if (!target)
		return;

	auto outapp = new EQApplicationPacket(OP_Reward, sizeof(QuestReward_Struct));
	memset(outapp->pBuffer, 0, sizeof(QuestReward_Struct));
	QuestReward_Struct* qr = (QuestReward_Struct*)outapp->pBuffer;

	memcpy(qr, &reward, sizeof(QuestReward_Struct));

	// not set in caller because reasons
	qr->mob_id = target->GetID();		// Entity ID for the from mob name

	if (reward.copper > 0 || reward.silver > 0 || reward.gold > 0 || reward.platinum > 0)
		AddMoneyToPP(reward.copper, reward.silver, reward.gold, reward.platinum, false);

	for (int i = 0; i < QUESTREWARD_COUNT; ++i)
		if (reward.item_id[i] > 0)
			SummonItem(reward.item_id[i], 0, EQ::legacy::SLOT_QUEST);

	if (reward.faction > 0 && target->IsNPC() && !target->IsCharmedPet())
	{
		if (reward.faction_mod > 0)
			SetFactionLevel2(CharacterID(), reward.faction, reward.faction_mod, 0);
		else
			SetFactionLevel2(CharacterID(), reward.faction, 0, 0);
	}

	if (reward.exp_reward > 0)
		AddQuestEXP(reward.exp_reward);

	QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
	safe_delete(outapp);
}

void Client::RewindCommand()
{
	if ((rewind_timer.GetRemainingTime() > 1 && rewind_timer.Enabled())) {
		Message(Chat::White, "You must wait before using #rewind again.");
	}
	else {
		MovePCGuildID(zone->GetZoneID(), zone->GetGuildID(), m_RewindLocation.x, m_RewindLocation.y, m_RewindLocation.z, 0, 2, Rewind);
		rewind_timer.Start(30000, true);
	}
}

// this assumes that 100 == no haste/slow; returns the rule value + 100
int Client::GetHasteCap()
{
	int cap = 100;

	if (level > 59) // 60+
		cap += RuleI(Character, HasteCap);
	else if (level > 50) // 51-59
		cap += 85;
	else // 1-50
		cap += level + 25;

	return cap;
}

float Client::GetQuiverHaste()
{
	float quiver_haste = 0;
	for (int r = EQ::invslot::GENERAL_BEGIN; r <= EQ::invslot::GENERAL_END; r++) {
		const EQ::ItemInstance *pi = GetInv().GetItem(r);
		if (!pi)
			continue;
		if (pi->IsType(EQ::item::ItemClassBag) && pi->GetItem()->BagType == EQ::item::BagTypeQuiver && pi->GetItem()->BagWR != 0) {
			quiver_haste = (pi->GetItem()->BagWR / RuleI(Combat, QuiverWRHasteDiv));
			break; // first quiver is used, not the highest WR
		}
	}
	if (quiver_haste > 0)
		quiver_haste /= 100.0f;

	return quiver_haste;
}

uint8 Client::Disarm(float chance)
{
	EQ::ItemInstance* weapon = m_inv.GetItem(EQ::invslot::slotPrimary);
	if(weapon)
	{
		if (zone->random.Roll(chance))
		{
			int8 charges = weapon->GetCharges();
			uint16 freeslotid = m_inv.FindFreeSlot(false, true, weapon->GetItem()->Size);
			if(freeslotid != INVALID_INDEX)
			{
				DeleteItemInInventory(EQ::invslot::slotPrimary,0,true);
				SummonItem(weapon->GetID(),charges,freeslotid);
				WearChange(EQ::textures::weaponPrimary,0,0);

				return 2;
			}
		}
		else
		{
			return 1;
		}
	}

	return 0;
}


void Client::SendSoulMarks(SoulMarkList_Struct* SMS)
{
	if(Admin() <= 80)
		return;

	auto outapp = new EQApplicationPacket(OP_SoulMarkUpdate, sizeof(SoulMarkList_Struct));
	memset(outapp->pBuffer, 0, sizeof(SoulMarkList_Struct));
	SoulMarkList_Struct* soulmarks = (SoulMarkList_Struct*)outapp->pBuffer;
	memcpy(&soulmarks->entries, SMS->entries, 12 * sizeof(SoulMarkEntry_Struct));
	strncpy(soulmarks->interrogatename, SMS->interrogatename, 64);
	QueuePacket(outapp);
	safe_delete(outapp);	
}

void Client::SendClientVersion()
{
	if(ClientVersion() == EQ::versions::Mac)
	{
		std::string string("Mac");
		std::string type;
		if(ClientVersionBit() == EQ::versions::bit_MacIntel)
			type = "Intel";
		else if(ClientVersionBit() == EQ::versions::bit_MacPPC)
			type = "PowerPC";
		else if(ClientVersionBit() == EQ::versions::bit_MacPC)
			type = "PC";
		else
			type = "Invalid";

		if(GetGM())
			Message(Chat::Yellow, "[GM Debug] Your client version is: %s (%i). Your client type is: %s.", string.c_str(), ClientVersion(), type.c_str());
		else
			Log(Logs::Detail, Logs::Debug, "%s: Client version is: %s. The client type is: %s.", GetName(), string.c_str(), type.c_str());

	}
	else
	{
		std::string string;
		if(ClientVersion() == EQ::versions::Unused)
			string = "Unused";
		else
			string = "Unknown";

		if(GetGM())
			Message(Chat::Yellow, "[GM Debug] Your client version is: %s (%i).", string.c_str(), ClientVersion());	
		else
			Log(Logs::Detail, Logs::Debug, "%s: Client version is: %s.", GetName(), string.c_str());
	}
}

void Client::FixClientXP()
{
	//This is only necessary when the XP formula changes. However, it should be left for toons that have not been converted.

	uint16 level = GetLevel();
	uint32 totalrequiredxp = GetEXPForLevel(level);
	float currentxp = GetEXP();
	uint32 currentaa = GetAAXP();

	if(currentxp < totalrequiredxp)
	{
		if(Admin() == 0 && level > 1)
		{
			Message(Chat::Red, "Error: Your current XP (%0.2f) is lower than your current level (%i)! It needs to be at least %i", currentxp, level, totalrequiredxp);
			SetEXP(totalrequiredxp, currentaa);
			Save();
			Kick();
		}
		else if(Admin() > 0 && level > 1)
			Message(Chat::Red, "Error: Your current XP (%0.2f) is lower than your current level (%i)! It needs to be at least %i. Use #level or #addxp to correct it and logout!", currentxp, level, totalrequiredxp);
	}
}

void Client::KeyRingLoad()
{
	std::string query = StringFormat("SELECT item_id FROM character_keyring "
									"WHERE id = '%i' ORDER BY item_id", character_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
		keyring.push_back(atoi(row[0]));

}

void Client::KeyRingAdd(uint32 item_id)
{
	if(0==item_id)
		return;

	bool found = KeyRingCheck(item_id);
	if (found)
		return;

	std::string query = StringFormat("INSERT INTO character_keyring(id, item_id) VALUES(%i, %i)", character_id, item_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	std::string keyName("");
	const EQ::ItemData* item = database.GetItem(item_id);

	switch (item_id) {
		case 20033:
		{
			keyName = "Tower of Frozen Shadow 2nd floor key";
			break;
		}
		case 20034:
		{
			keyName = "Tower of Frozen Shadow 3rd floor key";
			break;
		}
		case 20035:
		{
			keyName = "Tower of Frozen Shadow 4th floor key";
			break;
		}
		case 20036:
		{
			keyName = "Tower of Frozen Shadow 5th floor key";
			break;
		}
		case 20037:
		{
			keyName = "Tower of Frozen Shadow 6th floor key";
			break;
		}
		case 20038:
		{
			keyName = "Tower of Frozen Shadow 7th floor key";
			break;
		}
		case 20039:
		{
			keyName = "Tower of Frozen Shadow master key";
			break;
		}
		default:
		{
			if (item && item->Name)
				keyName = item->Name;
			break;
		}
	}

	if (keyName.length() > 0)
		Message(Chat::Yellow, "%s has been added to your key ring.", keyName.c_str());

	keyring.push_back(item_id);
}

bool Client::KeyRingCheck(uint32 item_id)
{
	if (GetGM())
		return true;

	for(std::list<uint32>::iterator iter = keyring.begin();
		iter != keyring.end();
		++iter)
	{
		uint16 keyitem = *iter;
		if (keyitem == item_id)
		{
			return true;
		}
		else
		{
			if (CheckKeyRingStage(item_id))
			{
				return true;
			}
		}
	}
	return false;
}

void Client::KeyRingList(Client* notifier)
{
	std::vector< int > keygroups;
	for(std::list<uint32>::iterator iter = keyring.begin();
		iter != keyring.end();
		++iter)
	{
		uint16 keyitem = *iter;
		KeyRing_Data_Struct* krd = GetKeyRing(keyitem, 0);
		if (krd != nullptr)
		{
			if (krd->stage == 0)
			{
				notifier->Message(Chat::Yellow, "%s", krd->name.c_str());
			}
			else
			{
				bool found = false;
				uint32 keygroup = krd->zoneid;
				for (unsigned int i = 0; i < keygroups.size(); ++i)
				{
					if (keygroups[i] == keygroup)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					std::string name = GetMaxKeyRingStage(krd->keyitem, false);
					notifier->Message(Chat::Yellow, "%s", name.c_str());
					keygroups.push_back(keygroup);
				}
			}
		}
	}

	if (GetLevel() > 45) // flags not obtainable until 46+
	{
		// Mob::GetGlobal() does not work reliably, so doing it this way
		std::list<QGlobal> global_map;
		QGlobalCache::GetQGlobals(global_map, nullptr, this, zone);
		auto iter = global_map.begin();
		while (iter != global_map.end())
		{
			if ((*iter).name == "bertox_key")
				notifier->Message(Chat::Yellow, "%s", "Key to the lower depths of the Ruins of Lxanvom.");
			else if ((*iter).name == "zebuxoruk" || ((*iter).name == "karana" && ((*iter).value == "3" || (*iter).value == "4")))
				notifier->Message(Chat::Yellow, "%s", "Talisman of Thunderous Foyer");
			else if ((*iter).name == "earthb_key")
				notifier->Message(Chat::Yellow, "%s", "Passkey of the Twelve");
			++iter;
		}
	}

	keygroups.clear();
}

bool Client::CheckKeyRingStage(uint16 keyitem)
{
	KeyRing_Data_Struct* krd = GetKeyRing(keyitem, GetZoneID());
	if (krd != nullptr)
	{
		uint8 required_stage = krd->stage;
		if (required_stage == 0)
		{
			return false;
		}
		else
		{
			for (std::list<uint32>::iterator iter = keyring.begin();
				iter != keyring.end();
				++iter)
			{
				uint16 key = *iter;
				KeyRing_Data_Struct* kr = GetKeyRing(key, GetZoneID());
				if (kr != nullptr)
				{
					if (kr->stage >= required_stage)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

std::string Client::GetMaxKeyRingStage(uint16 keyitem, bool use_current_zone)
{
	uint8 max_stage = 0;
	uint32 zoneid = 0;
	if (use_current_zone)
		zoneid = GetZoneID();

	KeyRing_Data_Struct* krd = GetKeyRing(keyitem, zoneid);
	std::string ret;
	if (krd != nullptr)
	{
		if (krd->stage == 0)
		{
			return ret;
		}
		else
		{
			for (std::list<uint32>::iterator iter = keyring.begin();
				iter != keyring.end();
				++iter)
			{
				uint16 key = *iter;
				KeyRing_Data_Struct* kr = GetKeyRing(key, krd->zoneid);
				if (kr != nullptr)
				{
					if (kr->stage >= max_stage)
					{
						ret = kr->name;
						max_stage = kr->stage;
					}
				}
			}
		}
	}

	return ret;
}

KeyRing_Data_Struct* Client::GetKeyRing(uint16 keyitem, uint32 zoneid) 
{
	LinkedListIterator<KeyRing_Data_Struct*> iterator(zone->KeyRingDataList);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		KeyRing_Data_Struct* krd = iterator.GetData();
		if (keyitem == krd->keyitem && (zoneid == krd->zoneid || zoneid == 0))
		{
			return (krd);
		}
		iterator.Advance();
	}

	return (nullptr);
}

void Client::SendToBoat(bool messageonly)
{
	// Sometimes, the client doesn't send OP_LeaveBoat, so the boat values don't get cleared.
	// This can lead difficulty entering the zone, since some people's client's don't like
	// the boat timeout period.
	if(!zone->IsBoatZone())
	{
		m_pp.boatid = 0;
		m_pp.boat[0] = 0;
		return;
	}
	else
	{
		if(m_pp.boatid > 0)
		{
			Log(Logs::Detail, Logs::Boats, "%s's boatid is %d boatname is %s", GetName(), m_pp.boatid, m_pp.boat);

			if(messageonly)
			{
				Mob* boat = entity_list.GetNPCByNPCTypeID(m_pp.boatid);
				if(boat && boat->IsBoat())
				{
					Log(Logs::Detail, Logs::Boats, "%s's boat %s (%d) location is %0.2f,%0.2f,%0.2f", GetName(), boat->GetCleanName(), m_pp.boatid, boat->GetX(), boat->GetY(), boat->GetZ());
					Log(Logs::Detail, Logs::Boats, "%s's location is: %0.2f,%0.2f,%0.2f", GetName(), m_Position.x, m_Position.y, m_Position.z);
				}
				if(!boat)
				{
					Log(Logs::Detail, Logs::Boats, "%s's boat is not spawned.", GetName());
				}

				return;
			}

			Mob* boat = entity_list.GetNPCByNPCTypeID(m_pp.boatid);
			if(!boat || !boat->IsBoat())
			{
				Log(Logs::Detail, Logs::Boats, "Boat %d is not spawned. Sending %s to safe points.", m_pp.boatid, GetName());
				auto safePoint = zone->GetSafePoint();
				m_pp.boatid = 0;
				m_pp.boat[0] = 0;
				m_pp.x = safePoint.x;
				m_pp.y = safePoint.y;
				m_pp.z = safePoint.z;
			}
			else
			{
				//The Kunark zones force the client to the wrong coords if boat name is set in PP, this is the workaround
				if(zone->GetZoneID() == Zones::TIMOROUS || zone->GetZoneID() == Zones::FIRIONA)
				{
					auto PPPos = glm::vec4(m_pp.x, m_pp.y, m_pp.z, m_pp.heading);
					float distance = DistanceNoZ(PPPos, boat->GetPosition());
					if(distance >= RuleI(Zone,BoatDistance))
					{
						float z_mod = 0.0f;
						if(m_pp.boatid == Maidens_Voyage)
							z_mod = 76.0f;
						else if(m_pp.boatid == Bloated_Belly)
							z_mod = 20.0f;
						m_pp.x = boat->GetX();
						m_pp.y = boat->GetY();
						m_pp.z = boat->GetZ() + z_mod;
						Log(Logs::Detail, Logs::Boats, "Kunark boat %s found at %0.2f,%0.2f,%0.2f! %s's location changed to match.", boat->GetName(), boat->GetX(), boat->GetY(), boat->GetZ(), GetName());
						return;
					}
				}

				Log(Logs::Detail, Logs::Boats, "Boat %s found at %0.2f,%0.2f,%0.2f! %s's location (%0.2f,%0.2f,%0.2f) unchanged.", boat->GetName(), boat->GetX(), boat->GetY(), boat->GetZ(), GetName(), m_pp.x, m_pp.y, m_pp.z);
			}
		}
	}
}

bool Client::HasInstantDisc(uint16 skill_type)
{
	if(GetClass() == MONK)
	{
		if((skill_type == EQ::skills::SkillFlyingKick && GetActiveDisc() == disc_thunderkick) ||
			(skill_type == EQ::skills::SkillEagleStrike && GetActiveDisc() == disc_ashenhand) ||
			(skill_type == EQ::skills::SkillDragonPunch && GetActiveDisc() == disc_silentfist))
			return true;
	}
	else if(GetClass() == SHADOWKNIGHT)
	{
		if(GetActiveDisc() == disc_unholyaura && (skill_type == SPELL_HARM_TOUCH || skill_type == SPELL_HARM_TOUCH2 || skill_type == SPELL_IMP_HARM_TOUCH))
			return true;
	}

	return false;
}

void Client::SendMerchantEnd()
{
	MerchantSession = 0;
	auto outapp = new EQApplicationPacket(OP_ShopEndConfirm, 2);
	outapp->pBuffer[0] = 0x0a;
	outapp->pBuffer[1] = 0x66;
	QueuePacket(outapp);
	safe_delete(outapp);
	Save();
}

void Client::Consent(uint8 permission, char ownername[64], char grantname[64], bool do_not_update_list, uint32 corpse_id)
{
	//do_not_update_list should only be false when the granted player is online.

	if(permission == 1)
	{
		//Add Consent
		if (!do_not_update_list)
		{
			RemoveFromConsentList(ownername);
			database.SaveCharacterConsent(grantname, ownername, consent_list);
		}
		else
		{
			database.SaveCharacterConsent(grantname, ownername);
		}
	}
	else
	{
		//Remove Consent
		if (!do_not_update_list)
		{
			RemoveFromConsentList(ownername, corpse_id);
		}
		database.DeleteCharacterConsent(grantname, ownername, corpse_id);
	}
}

void Client::RemoveFromConsentList(char ownername[64], uint32 corpse_id)
{
	std::list<CharacterConsent> tmp_consent_list;
	std::list<CharacterConsent>::const_iterator itr;
	for (itr = consent_list.begin(); itr != consent_list.end(); ++itr)
	{
		CharacterConsent cc = *itr;
		if (cc.consenter == ownername && (cc.corpse_id == corpse_id || corpse_id == 0))
		{
			Log(Logs::Detail, Logs::Corpse, "Removing entry %s (%d) from %s consent list...", ownername, corpse_id, GetName());
		}
		else
		{
			Log(Logs::Detail, Logs::Corpse, "Adding back entry %s (%d) to %s consent list...", cc.consenter.c_str(), cc.corpse_id, GetName());
			tmp_consent_list.push_back(cc);
		}
	}
	consent_list.clear();
	consent_list = tmp_consent_list;
	tmp_consent_list.clear();
}

bool Client::IsConsented(std::string grantname)
{
	std::string query = StringFormat("SELECT `id` FROM `character_corpses` WHERE `charid` = '%u' AND `is_buried` = 0", CharacterID());
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) 
	{
		uint32 current_corpse = atoi(row[0]);
		std::string query1 = StringFormat("SELECT count(*) FROM `character_consent` WHERE corpse_id = %d AND name = '%s' AND consenter_name = '%s'", current_corpse, Strings::Escape(grantname).c_str(), GetName());
		auto results1 = database.QueryDatabase(query1);
		auto row1 = results1.begin();
		if (atoi(row1[0]) == 0)
		{
			return false;
		}
	}

	return true;
}

bool Client::LoadCharacterConsent()
{
	consent_list.clear();
	std::string query = StringFormat("SELECT consenter_name, corpse_id FROM `character_consent` WHERE `name` = '%s'", GetName());
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) 
	{
		CharacterConsent cc;
		cc.consenter = row[0];
		cc.corpse_id = atoul(row[1]);
		consent_list.push_back(cc);
	}
	return true;
}

float Client::GetPortHeading(uint16 newx, uint16 newy)
{
	if(zone->GetZoneID() == Zones::PAINEEL)
	{
		// To Bank
		if(GetX() > 519 && GetX() < 530 && newx > 540 && newx < 560)
		{
			return 64.0f;
		}
		// To SK guild
		else if(GetY() > 952 && GetY() < 965 && newy >= 900 && newy <= 920)
		{
			return 120.0f;
		}
	}

	return 0.0f;
}

void Client::ClearPTimers(uint16 type)
{
	if(type > 0)
	{
		if(p_timers.GetRemainingTime(type) > 0)
		{
			Log(Logs::General, Logs::PTimers, "Clearing timer %d.", type);
			p_timers.Clear(&database, type);
		}
	}
	else
	{
		Log(Logs::General, Logs::PTimers, "Clearing all timers.");
		int x = 0;
		for (x = pTimerUnUsedTimer; x <= pTimerPeqzoneReuse; ++x)
		{
			if(p_timers.GetRemainingTime(x) > 0)
				p_timers.Clear(&database, x);
		}
		for (x = pTimerAAStart; x <= pTimerSpellStart + 4678; ++x)
		{
			if(p_timers.GetRemainingTime(x) > 0)
				p_timers.Clear(&database, x);
		}
	}
}

uint8 Client::GetDiscTimerID(uint8 disc_id)
{
	if (RuleB(Combat, UseDiscTimerGroups))
	{
		// This is for Discipline timer groups. Bard, Paladin, Ranger, SK, and Beastlord all use a single group.
		// Warrior, Monk, and Rogue can all have up to 3 discs available at a time.

		switch (disc_id)
		{
		case disc_kinesthetics:
			return 1;

			break;
		case disc_aggressive:
		case disc_precision:
		case disc_ashenhand:
		case disc_thunderkick:
		case disc_charge:
		case disc_mightystrike:
		case disc_silentfist:
			return 2;

			break;

		case disc_furious:
			if (GetClass() == WARRIOR)
				return 1;
			else
				return 0;

			break;
		case disc_fortitude:
			if (GetClass() == WARRIOR)
				return 1;
			else if (GetClass() == MONK)
				return 0;

			break;
		case disc_fellstrike:
			if (GetClass() == BEASTLORD)
				return 0;
			else if (GetClass() == WARRIOR)
				return 2;
			else
				return 1;

			break;
		case disc_hundredfist:
			if (GetClass() == MONK)
				return 1;
			else if (GetClass() == ROGUE)
				return 2;

			break;
		default:
			return 0;
		}
	}

	return 0;
}

uint32 Client::CheckDiscTimer(uint8 type)
{
	if(p_timers.GetRemainingTime(type) > 7200)
	{
		p_timers.Clear(&database, type);
		p_timers.Start(type, 3600);
		Save();
		Log(Logs::General, Logs::Discs, "Reset disc timer %d for %s to %d", type, GetName(), p_timers.GetRemainingTime(type));
	}
	else
	{
		Log(Logs::General, Logs::Discs, "Disc using timer %d has %d seconds remaining", type, p_timers.GetRemainingTime(type));
	}

	return(p_timers.GetRemainingTime(type));
}

void Client::ShowRegenInfo(Client* message)
{
	if (GetPlayerRaceBit(GetBaseRace()) & RuleI(Character, BaseHPRegenBonusRaces))
		message->Message(Chat::White, "%s is race %d which has a HP regen bonus.", GetName(), GetRace());
	else
		message->Message(Chat::White, "%s has no regen bonus.", GetName());
	message->Message(Chat::White, "Total HP Regen: %d/%d From Spells: %d From Items: %d From AAs: %d", CalcHPRegen(), CalcHPRegenCap(), spellbonuses.HPRegen, itembonuses.HPRegen, aabonuses.HPRegen);
	message->Message(Chat::White, "Total Mana Regen: %d/%d From Spells: %d From Items: %d From AAs: %d", CalcManaRegen(), CalcManaRegenCap(), spellbonuses.ManaRegen, itembonuses.ManaRegen, aabonuses.ManaRegen);
	message->Message(Chat::White, "%s %s food famished and %s water famished. They %s and %s rested.", GetName(), FoodFamished() ? "is" : "is not", WaterFamished() ? "is" : "is not", IsSitting() ? "sitting" : "standing", IsRested() ? "are" : "are not");
	if (Famished())
		message->Message(Chat::Red, "This character is currently famished and has lowered HP/Mana regen.");
}

void Client::ClearGroupInvite()
{
	auto outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupGeneric_Struct2));
	GroupGeneric_Struct2* gu = (GroupGeneric_Struct2*)outapp->pBuffer;
	gu->action = groupActDisband2;
	gu->param = 0;
	QueuePacket(outapp);
	safe_delete(outapp);
}


void Client::WarCry(uint8 rank)
{
	uint32 time = rank * 10;
	float rangesq = 100.0f * 100.0f;

	if (rank == 0)
		return;

	// group members
	if (IsGrouped())
	{
		Group *g = GetGroup();
		if (g) 
		{
			for (int gi = 0; gi < MAX_GROUP_MEMBERS; gi++) 
			{
				// skip self
				if (g->members[gi] && g->members[gi]->IsClient() && g->members[gi] != this)
				{
					float distance = DistanceSquared(GetPosition(), g->members[gi]->CastToClient()->GetPosition());
					if (distance <= rangesq)
					{
						g->members[gi]->CastToClient()->EnableAAEffect(aaEffectWarcry, time);
						g->members[gi]->Message_StringID(Chat::Spells, WARCRY_ACTIVATE);
					}
				}
			}
		}
	}
	// raid group members
	else if (IsRaidGrouped())
	{
		Raid *r = GetRaid();
		if (r)
		{
			uint32 rgid = r->GetGroup(GetName());
			if (rgid >= 0 && rgid < MAX_RAID_GROUPS)
			{
				for (int z = 0; z < MAX_RAID_MEMBERS; z++)
				{
					// skip self
					if (r->members[z].member != nullptr && r->members[z].member != this && r->members[z].GroupNumber == rgid)
					{
						Client *member = r->members[z].member;
						float distance = DistanceSquared(GetPosition(), member->GetPosition());
						if (distance <= rangesq)
						{
							member->EnableAAEffect(aaEffectWarcry, time);
							member->Message_StringID(Chat::Spells, WARCRY_ACTIVATE);
						}
					}
				}
			}
		}
	}

	// self
	EnableAAEffect(aaEffectWarcry, time);
	Message_StringID(Chat::Spells, WARCRY_ACTIVATE);
}

void Client::ClearTimersOnDeath()
{
	// Skills
	for (int x = pTimerUnUsedTimer; x <= pTimerPeqzoneReuse; ++x)
	{
		if(x < pTimerDisciplineReuseStart || x > pTimerDisciplineReuseEnd)
			ClearPTimers(x);
	}

	// Spell skills
	if (GetClass() == PALADIN && !p_timers.Expired(&database, pTimerLayHands))
	{
		ClearPTimers(pTimerLayHands);
	}
}

void Client::UpdateLFG(bool value, bool ignoresender)
{
	if (LFG == value)
		return;

	LFG = value;

	UpdateWho();

	auto outapp = new EQApplicationPacket(OP_LFGCommand, sizeof(LFG_Appearance_Struct));
	LFG_Appearance_Struct* lfga = (LFG_Appearance_Struct*)outapp->pBuffer;
	lfga->entityid = GetID();
	lfga->value = (int8)LFG;

	entity_list.QueueClients(this, outapp, ignoresender);
	safe_delete(outapp);
}

bool Client::FleshToBone()
{
	EQ::ItemInstance* flesh = GetInv().GetItem(EQ::invslot::slotCursor);
	if (flesh && flesh->IsFlesh())
	{
		uint32 fcharges = flesh->GetCharges();
		DeleteItemInInventory(EQ::invslot::slotCursor, fcharges, true);
		SummonItem(13073, fcharges);
		return true;
	}

	Message_StringID(Chat::SpellFailure, FLESHBONE_FAILURE);
	return false;
}

void Client::SetPCTexture(uint8 slot, uint16 texture, uint32 color, bool set_wrist)
{
	if (set_wrist || (!set_wrist && (texture != 0 || slot != EQ::textures::armorWrist)))
		Log(Logs::General, Logs::Inventory, "%s is setting material slot %d to %d color %d", GetName(), slot, texture, color);

	switch (slot)
	{
	case EQ::textures::armorHead:
	{
		// fix up custom helms worn by old model clients so they appear correctly to luclin model clients too
		// old model clients don't send the item's tint since they are using the custom helm graphic, there seems to be no good way to make this consistent between the 2 model types
		switch (texture)
		{
		case 665:
		case 660:
		case 627:
		case 620:
		case 537:
		case 530:
		case 565:
		case 561:
		case 605:
		case 600:
		case 545:
		case 540:
		case 595:
		case 590:
		case 557:
		case 550:
		case 655:
		case 650:
		case 645:
		case 640:
		case 615:
		case 610:
		case 585:
		case 580:
		case 635:
		case 630:
			pc_helmtexture = 240;
			break;
		default:
			pc_helmtexture = texture;
			helmcolor = color;
		}

		break;
	}
	case EQ::textures::armorChest:
	{
		pc_chesttexture = texture;
		chestcolor = color;
		break;
	}
	case EQ::textures::armorArms:
	{
		pc_armtexture = texture;
		armcolor = color;
		break;
	}
	case EQ::textures::armorWrist:
	{
		// The client sends both wrists at zone-in. Since we always favor wrist1, if it is empty when the player zones then 
		// we will be using texture 0 for pc_bracertexture even if wrist2 has an item.
		// This code causes us to break if the initial wrist1 slot is empty which will give us a chance to set wrist2's texture.
		if (!set_wrist && texture == 0)
		{
			break;
		}

		pc_bracertexture = texture;
		bracercolor = color;
		break;
	}
	case EQ::textures::armorHands:
	{
		pc_handtexture = texture;
		handcolor = color;
		break;
	}
	case EQ::textures::armorLegs:
	{
		pc_legtexture = texture;
		legcolor = color;
		break;
	}
	case EQ::textures::armorFeet:
	{
		pc_feettexture = texture;
		feetcolor = color;
		break;
	}
	}
}

void Client::GetPCEquipMaterial(uint8 slot, int16& texture, uint32& color)
{
	switch (slot)
	{
	case EQ::textures::armorHead:
	{
		if (pc_helmtexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_helmtexture;
			color = helmcolor;
		}
		break;
	}
	case EQ::textures::armorChest:
	{
		if (pc_chesttexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_chesttexture;
			color = chestcolor;
		}
		break;
	}
	case EQ::textures::armorArms:
	{
		if (pc_armtexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_armtexture;
			color = armcolor;
		}
		break;
	}
	case EQ::textures::armorWrist:
	{
		if (pc_bracertexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_bracertexture;
			color = bracercolor;
		}
		break;
	}
	case EQ::textures::armorHands:
	{
		if (pc_handtexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_handtexture;
			color = handcolor;
		}
		break;
	}
	case EQ::textures::armorLegs:
	{
		if (pc_legtexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_legtexture;
			color = legcolor;
		}
		break;
	}
	case EQ::textures::armorFeet:
	{
		if (pc_feettexture == INVALID_INDEX)
		{
			texture = GetEquipmentMaterial(slot);
			color = GetEquipmentColor(slot);
		}
		else
		{
			texture = pc_feettexture;
			color = feetcolor;
		}
		break;
	}
	}

	return;
}

bool Client::IsUnderWater()
{
	if (!zone->watermap)
	{
		return false;
	}
	if (zone->GetZoneID() == Zones::KEDGE)
		return true;

	auto underwater = glm::vec3(GetX(), GetY(), GetZ() + GetZOffset());
	if (zone->IsWaterZone(underwater.z) || zone->watermap->InLiquid(underwater))
	{
		return true;
	}

	return false;
}

bool Client::IsInWater()
{
	if (!zone->watermap)
	{
		return false;
	}
	if (zone->GetZoneID() == Zones::KEDGE)
		return true;
	if (zone->GetZoneID() == Zones::POWATER && GetZ() < 0.0f)
		return true;

	auto inwater = glm::vec3(GetX(), GetY(), GetZ() + 0.1f);
	if (zone->watermap->InLiquid(inwater))
	{
		return true;
	}

	return false;
}

void Client::SendBerserkState(bool state)
{
	auto outapp = new EQApplicationPacket(state ? OP_BerserkStart : OP_BerserkEnd, 0);
	QueuePacket(outapp);
	safe_delete(outapp);
}

bool Client::IsLockSavePosition() const
{
	return m_lock_save_position;
}

void Client::SetLockSavePosition(bool lock_save_position)
{
	Client::m_lock_save_position = lock_save_position;
}

void Client::RemoveAllSkills()
{
	for (uint32 i = 0; i <= EQ::skills::HIGHEST_SKILL; ++i) {
		m_pp.skills[i] = 0;
	}
	database.DeleteCharacterSkills(CharacterID(), &m_pp);
}

void Client::SetRaceStartingSkills()
{
	switch (m_pp.race)
	{
	case BARBARIAN:
	case ERUDITE:
	case HALF_ELF:
	case HIGH_ELF:
	case HUMAN:
	case OGRE:
	case TROLL:
	{
		// No Race Specific Skills
		break;
	}
	case DWARF:
	{
		m_pp.skills[EQ::skills::SkillSenseHeading] = 50; //Even if we set this to 0, Intel client sets this to 50 anyway. Confirmed this is correct for era.
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillSenseHeading, 50);
		break;
	}
	case DARK_ELF:
	{
		m_pp.skills[EQ::skills::SkillHide] = 50;
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillHide, 50);
		break;
	}
	case GNOME:
	{
		m_pp.skills[EQ::skills::SkillTinkering] = 50;
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillTinkering, 50);
		break;
	}
	case HALFLING:
	{
		m_pp.skills[EQ::skills::SkillHide] = 50;
		m_pp.skills[EQ::skills::SkillSneak] = 50;
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillHide, 50);
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillSneak, 50);
		break;
	}
	case IKSAR:
	{
		m_pp.skills[EQ::skills::SkillForage] = 50;
		m_pp.skills[EQ::skills::SkillSwimming] = 100;
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillForage, 50);
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillSwimming, 100);
		break;
	}
	case WOOD_ELF:
	{
		m_pp.skills[EQ::skills::SkillForage] = 50;
		m_pp.skills[EQ::skills::SkillHide] = 50;
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillForage, 50);
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillHide, 50);
		break;
	}
	case VAHSHIR:
	{
		m_pp.skills[EQ::skills::SkillSafeFall] = 50;
		m_pp.skills[EQ::skills::SkillSneak] = 50;
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillSafeFall, 50);
		database.SaveCharacterSkill(CharacterID(), EQ::skills::SkillSneak, 50);
		break;
	}
	}
}

void Client::SetRacialLanguages()
{
	switch (m_pp.race)
	{
	case BARBARIAN:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_BARBARIAN] = 100;
		break;
	}
	case DARK_ELF:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DARK_ELVISH] = 100;
		m_pp.languages[LANG_DARK_SPEECH] = 100;
		m_pp.languages[LANG_ELDER_ELVISH] = 54;
		m_pp.languages[LANG_ELVISH] = 54;
		break;
	}
	case DWARF:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DWARVISH] = 100;
		m_pp.languages[LANG_GNOMISH] = 25;
		break;
	}
	case ERUDITE:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_ERUDIAN] = 100;
		break;
	}
	case GNOME:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DWARVISH] = 25;
		m_pp.languages[LANG_GNOMISH] = 100;
		break;
	}
	case HALF_ELF:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_ELVISH] = 100;
		break;
	}
	case HALFLING:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_HALFLING] = 100;
		break;
	}
	case HIGH_ELF:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DARK_ELVISH] = 51;
		m_pp.languages[LANG_ELDER_ELVISH] = 51;
		m_pp.languages[LANG_ELVISH] = 100;
		break;
	}
	case HUMAN:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		break;
	}
	case IKSAR:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DARK_SPEECH] = 100;
		m_pp.languages[LANG_LIZARDMAN] = 100;
		break;
	}
	case OGRE:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DARK_SPEECH] = 100;
		m_pp.languages[LANG_OGRE] = 100;
		break;
	}
	case TROLL:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_DARK_SPEECH] = 100;
		m_pp.languages[LANG_TROLL] = 100;
		break;
	}
	case WOOD_ELF:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_ELVISH] = 100;
		break;
	}
	case VAHSHIR:
	{
		m_pp.languages[LANG_COMMON_TONGUE] = 100;
		m_pp.languages[LANG_COMBINE_TONGUE] = 100;
		m_pp.languages[LANG_ERUDIAN] = 32;
		m_pp.languages[LANG_VAH_SHIR] = 100;
		break;
	}
	}
}

void Client::SetClassLanguages()
{
	// we only need to handle one class, but custom server might want to do more
	switch (m_pp.class_) {
	case ROGUE:
		m_pp.languages[LANG_THIEVES_CANT] = 100;
		break;
	default:
		break;
	}
}


uint8 Client::GetRaceArmorSize() 
{

	uint8 armorSize = 0;
	int pRace = GetBaseRace();

	switch (pRace)
	{
	case WOOD_ELF: // Small
	case HIGH_ELF:
	case DARK_ELF:
	case DWARF:
	case HALFLING:
	case GNOME:
		armorSize = 0;
		break;
	case BARBARIAN:  // Medium
	case ERUDITE:
	case HALF_ELF:
	case IKSAR:
	case HUMAN:
		armorSize = 1;
		break;
	case TROLL: // Large
	case OGRE:
	case VAHSHIR:
		armorSize = 2;
		break;
	}

	return armorSize;
}

void Client::LoadLootedLegacyItems()
{
	std::string query = StringFormat("SELECT item_id, expire_time FROM character_legacy_items "
		"WHERE character_id = '%i' ORDER BY item_id", character_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		LootItemLockout lockout = LootItemLockout();
		lockout.item_id = atoi(row[0]);
		lockout.expirydate = atoll(row[1]);
		looted_legacy_items.emplace(atoi(row[0]), lockout);
	}

}

bool Client::CheckLegacyItemLooted(uint16 item_id)
{
	auto it = looted_legacy_items.find(item_id);
	if (it != looted_legacy_items.end())
	{
		if(it->second.HasLockout(Timer::GetTimeSeconds()))
			return true;
	}
	return false;
}

std::string Client::GetLegacyItemLockoutFailureMessage(uint16 item_id)
{
	std::string return_string = "Invalid loot lockout timer. Contact a GM.";


	auto it = looted_legacy_items.find(item_id);
	if (it != looted_legacy_items.end())
	{
		const EQ::ItemData* item = database.GetItem(it->first);

		if (!item)
			return return_string;

		if (!item->Name[0])
			return return_string;

		int64_t time_remaining = 0xFFFFFFFFFFFFFFFF;
		if (it->second.expirydate != 0)
		{
			time_remaining = it->second.expirydate - Timer::GetTimeSeconds();
		}

		//Magic number
		if (time_remaining == 0xFFFFFFFFFFFFFFFF) // Never unlocks
		{
			return_string = "This is a legacy item. You have already looted a legacy item of this type already on this character.";
		}
		else if (time_remaining >= 1) // Lockout present
		{
			return_string = "This is a legacy item, and this legacy item has a personal loot lockout that expires in: ";
			return_string += Strings::SecondsToTime(time_remaining).c_str();
		}
	}
	return return_string;
}

void Client::AddLootedLegacyItem(uint16 item_id, uint32 expiry_end_timestamp)
{
	if (CheckLegacyItemLooted(item_id))
		return;

	auto it = looted_legacy_items.find(item_id);
	if (it != looted_legacy_items.end())
		looted_legacy_items.erase(it);

	std::string query = StringFormat("REPLACE INTO character_legacy_items (character_id, item_id, expire_time) VALUES (%i, %i, %u)", character_id, item_id, expiry_end_timestamp);

	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	LootItemLockout lockout = LootItemLockout();
	lockout.item_id = item_id;
	lockout.expirydate = expiry_end_timestamp;

	looted_legacy_items.emplace(item_id, lockout);
	
}

bool Client::RemoveLootedLegacyItem(uint16 item_id)
{
	if (!CheckLegacyItemLooted(item_id))
		return false;

	std::string query = StringFormat("DELETE FROM character_legacy_items WHERE character_id = %i AND item_id = %i", character_id, item_id);

	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	auto it = looted_legacy_items.find(item_id);
	if (it != looted_legacy_items.end())
	{
		looted_legacy_items.erase(it);
	}
	return true;
}

void Client::RevokeSelf()
{
	if (GetRevoked())
		return;

	Message(Chat::Red, "You have been server muted for %i seconds for violating the anti-spam filter.", RuleI(Quarm, AntiSpamMuteInSeconds));

	std::string warped = "GM Alert: [" + std::string(GetCleanName()) + "] has violated the anti-spam filter.";
	worldserver.SendEmoteMessage(0, 0, 100, Chat::Default, "%s", warped.c_str());

	std::string query = StringFormat("UPDATE account SET revoked = 1 WHERE id = %i", AccountID());
	SetRevoked(1);
	database.QueryDatabase(query);

	std::string query2 = StringFormat("UPDATE `account` SET `revokeduntil` = DATE_ADD(NOW(), INTERVAL %i SECOND) WHERE `id` = %i", RuleI(Quarm, AntiSpamMuteInSeconds), AccountID());
	auto results2 = database.QueryDatabase(query2);
}


void Client::ShowLegacyItemsLooted(Client* to)
{
	if (!to)
		return;

	to->Message(Chat::Yellow, "Showing all legacy loot lockouts / items for %s..", GetCleanName());
	to->Message(Chat::Yellow, "======");
	to->Message(Chat::Yellow, "Legacy Item Flags On Character:");
	for(auto looted_legacy_item : looted_legacy_items)
	{
		const EQ::ItemData* itemdata = database.GetItem(looted_legacy_item.first);
		if (itemdata)
		{
			to->Message(Chat::Yellow, "ID %d : Name %s, Expiry: %s", itemdata->ID, itemdata->Name, Strings::SecondsToTime(looted_legacy_item.second.expirydate).c_str());
		}
	}
	to->Message(Chat::Yellow, "======");
	to->Message(Chat::Yellow, "Legacy Items On Character:");
	
	std::string query = StringFormat("SELECT id, name FROM items "
		"WHERE legacy_item = 1 ORDER BY id", character_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {

		to->Message(Chat::Yellow, "======");
		return;
	}

	std::map<uint16, std::string> item_ids_to_names;
	std::map<uint16, uint32> item_ids_to_quantity;

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		auto itr = item_ids_to_names.find(atoi(row[0]));

		if (itr == item_ids_to_names.end())
			item_ids_to_names[atoi(row[0])] = std::string(row[1]);

		auto itr2 = item_ids_to_quantity.find(atoi(row[0]));

		if (itr2 == item_ids_to_quantity.end())
			item_ids_to_quantity[atoi(row[0])] = 0;
	}

	for (int16 i = EQ::invslot::SLOT_BEGIN; i <= EQ::invbag::BANK_BAGS_END;)
	{
		const EQ::ItemInstance* newinv = m_inv.GetItem(i);

		if (i == EQ::invslot::GENERAL_END + 1) {
			i = EQ::invbag::GENERAL_BAGS_BEGIN;
			continue;
		}
		else if (i == EQ::invbag::CURSOR_BAG_END + 1) {
			i = EQ::invslot::BANK_BEGIN;
			continue;
		}
		else if (i == EQ::invslot::BANK_END + 1) {
			i = EQ::invbag::BANK_BAGS_BEGIN;
			continue;
		}

		if (newinv)
		{
			if (newinv->GetItem())
			{
				auto quantityItr = item_ids_to_quantity.find(newinv->GetItem()->ID);
				if (quantityItr != item_ids_to_quantity.end())
				{
					int countQuantity = quantityItr->second + 1;
					item_ids_to_quantity[newinv->GetItem()->ID] = countQuantity;
				}
			}
		}
		i++;
	}

	for (auto itemids : item_ids_to_names)
	{
		auto itr3 = item_ids_to_quantity.find(itemids.first);
		if (itr3 != item_ids_to_quantity.end())
		{
			if (itr3->second > 0)
			{
				const EQ::ItemData* item_data = database.GetItem(itemids.first);
				if (item_data)
				{
					to->Message(Chat::Yellow, "ID %d || Name %s || Quantity %d ", item_data->ID, item_data->Name, itr3->second);
				}
			}
		}
	}	
	to->Message(Chat::Yellow, "======");

}

bool Client::IsLootLockedOutOfNPC(uint32 npctype_id)
{
	if (npctype_id == 0)
		return false;

	auto lootItr = loot_lockouts.find(npctype_id);

	if (lootItr != loot_lockouts.end())
		return lootItr->second.HasLockout(Timer::GetTimeSeconds());

	return false;
};

std::string Client::GetSSFLooterName() {
	std::string appendedCharName = GetCleanName();
	if (IsSelfFound())
		appendedCharName += "-SF";

	if (IsSoloOnly())
		appendedCharName += "-Solo";

	return appendedCharName;
}

std::vector<int> Client::GetMemmedSpells() {
	std::vector<int> memmed_spells;
	for (int index = 0; index < EQ::spells::SPELL_GEM_COUNT; index++) {
		if (IsValidSpell(m_pp.mem_spells[index])) {
			memmed_spells.push_back(m_pp.mem_spells[index]);
		}
	}
	return memmed_spells;
}

std::vector<int> Client::GetScribeableSpells(uint8 min_level, uint8 max_level) {
	bool SpellGlobalRule = RuleB(Spells, EnableSpellGlobals);
	bool SpellBucketRule = RuleB(Spells, EnableSpellBuckets);
	bool SpellGlobalCheckResult = false;
	bool SpellBucketCheckResult = false;
	std::vector<int> scribeable_spells;
	for (int spell_id = 0; spell_id < SPDAT_RECORDS; ++spell_id) {
		bool scribeable = false;
		if (!IsValidSpell(spell_id))
			continue;
		if (spells[spell_id].classes[WARRIOR] == 0)
			continue;
		if (max_level > 0 && spells[spell_id].classes[m_pp.class_ - 1] > max_level)
			continue;
		if (min_level > 1 && spells[spell_id].classes[m_pp.class_ - 1] < min_level)
			continue;
		if (spells[spell_id].skill == 52)
			continue;
		if (spells[spell_id].not_player_spell)
			continue;
		if (HasSpellScribed(spell_id))
			continue;

		if (SpellGlobalRule) {
			SpellGlobalCheckResult = SpellGlobalCheck(spell_id, CharacterID());
			if (SpellGlobalCheckResult) {
				scribeable = true;
			}
		}
		else if (SpellBucketRule) {
			SpellBucketCheckResult = SpellBucketCheck(spell_id, CharacterID());
			if (SpellBucketCheckResult) {
				scribeable = true;
			}
		}
		else {
			scribeable = true;
		}

		if (scribeable) {
			scribeable_spells.push_back(spell_id);
		}
	}
	return scribeable_spells;
}

std::vector<int> Client::GetScribedSpells() {
	std::vector<int> scribed_spells;
	for (int index = 0; index < EQ::spells::SPELLBOOK_SIZE; index++) {
		if (IsValidSpell(m_pp.spell_book[index])) {
			scribed_spells.push_back(m_pp.spell_book[index]);
		}
	}
	return scribed_spells;
}

void Client::SaveSpells()
{
	std::vector<CharacterSpellsRepository::CharacterSpells> character_spells = {};

	for (int index = 0; index < EQ::spells::SPELLBOOK_SIZE; index++) {
		if (IsValidSpell(m_pp.spell_book[index])) {
			auto spell = CharacterSpellsRepository::NewEntity();
			spell.id = CharacterID();
			spell.slot_id = index;
			spell.spell_id = m_pp.spell_book[index];
			character_spells.emplace_back(spell);
		}
	}

	CharacterSpellsRepository::DeleteWhere(database, fmt::format("id = {}", CharacterID()));

	if (!character_spells.empty()) {
		CharacterSpellsRepository::InsertMany(database, character_spells);
	}
}

uint16 Client::ScribeSpells(uint8 min_level, uint8 max_level)
{
	int available_book_slot = GetNextAvailableSpellBookSlot();
	std::vector<int> spell_ids = GetScribeableSpells(min_level, max_level);
	uint16 spell_count = spell_ids.size();
	uint16 scribed_spells = 0;
	if (spell_count > 0) {
		for (auto spell_id : spell_ids) {
			if (available_book_slot == -1) {
				Message(
					Chat::Red,
					fmt::format(
						"Unable to scribe {} ({}) to Spell Book because your Spell Book is full.",
						spells[spell_id].name,
						spell_id
					).c_str()
				);
				break;
			}

			if (HasSpellScribed(spell_id)) {
				continue;
			}

			// defer saving per spell and bulk save at the end
			ScribeSpell(spell_id, available_book_slot, true, true);
			available_book_slot = GetNextAvailableSpellBookSlot(available_book_slot);
			scribed_spells++;
		}
	}

	if (scribed_spells > 0) {
		std::string spell_message = (
			scribed_spells == 1 ?
			"a new spell" :
			fmt::format("{} new spells", scribed_spells)
			);
		Message(Chat::White, fmt::format("You have learned {}!", spell_message).c_str());

		// bulk insert spells
		SaveSpells();
	}
	return scribed_spells;
}

void Client::SetMarried(const char* playerName)
{
	if (!playerName || !playerName[0])
		return;

	if (strlen(playerName) > 20)
		return;

	if (!RuleB(Quarm, ErollsiDayEvent))
	{
		Message(13, "Marriage season is not active.. Marriage failed.");
		return;
	}

	Client* c = entity_list.GetClientByName(playerName);
	if (c)
	{
		if (c->CharacterID() == CharacterID())
		{
			Message(13, "Bristlebane notices your antics and is on to you. Marriage failed.");
			return;
		}
		SetTemporaryMarriageCharacterID(c->CharacterID());

		if (c->m_epp.married_character_id == 0 && m_epp.married_character_id == 0)
		{
			if (c->m_epp.temp_last_name[0] && m_epp.temp_last_name[0])
			{
				if (strcmp(GetTemporaryLastName(), c->GetTemporaryLastName()) == 0)
				{
					if (c->GetTemporaryMarriageCharacterID() == CharacterID() && GetTemporaryMarriageCharacterID() == c->CharacterID())
					{
						m_epp.married_character_id = GetTemporaryMarriageCharacterID();
						c->m_epp.married_character_id = c->GetTemporaryMarriageCharacterID();
						c->Save(1);
						Save(1);
						c->Message(15, "Your marriage was a success! Erollsi has blessed and ordained the %s family. You will now receive a 20 percent experience bonus while traveling with your partner, %s, for the duration of the event.", GetTemporaryLastName(), GetCleanName());
						Message(15, "Your marriage was a success! Erollsi has blessed and ordained the %s family. You will now receive a 20 percent experience bonus while traveling with your partner, %s, for the duration of the event.", c->GetTemporaryLastName(), c->GetCleanName());
						return;
					}
					else
					{
						Message(13, "Your marriage requires the vows of your other partner. Please have them state your name. They must also have the same surname in order to confirm your vows.");
						return;
					}
				}
				else
				{
					Message(13, "Erollsi Marr whispers in your ear, 'You and your partner must share the same surname before you can truly tie the knot together.'");
					return;
				}
			}
			else
			{
				Message(13, "Erollsi Marr whispers in your ear, 'You and your partner must have a surname name before you can truly tie the knot together.'");
				return;
			}
		}
		else
		{
			Message(13, "Erollsi Marr whispers in your ear, 'You or your desired partner is already married. Try again next year when their marriage crumbles.'");
			return;
		}
	}
	else
	{
		Message(13, "Erollsi Marr whispers in your ear, 'Your marriage attempt failed. Your partner isn't here. Try again.'");
		return;
	}
}

bool Client::IsMarried()
{
	return m_epp.married_character_id != 0;
}

void Client::SetCharExportFlag(uint8 flag)
{
	if (flag == 0) {
		m_epp.char_export_flag = 0;
		Save(1);
		Message(Chat::Default, "Character export disabled.");
		return;
	}
	else if (flag == 1) {
		m_epp.char_export_flag = 1;
		Save(1);
		Message(Chat::Default, "Character \"worn\" export enabled.");
		return;
	}
	else if (flag == 2) {
		m_epp.char_export_flag = 2;
		Save(1);
		Message(Chat::Default, "Character \"inventory\" export enabled.");
		return;
	}
	else if (flag == 3) {
		m_epp.char_export_flag = 3;
		Save(1);
		Message(Chat::Default, "Character \"bank\" export enabled.");
		return;
	}
}

bool Client::HasTemporaryLastName()
{
	return m_epp.temp_last_name[0] != 0;
}

bool Client::IsDevToolsEnabled() const
{
	return dev_tools_enabled && RuleB(World, EnableDevTools);
}

void Client::SetDevToolsEnabled(bool in_dev_tools_enabled)
{
	Client::dev_tools_enabled = in_dev_tools_enabled;
}

void Client::ShowDevToolsMenu()
{
	std::string menu_commands_search;
	std::string menu_commands_show;
	std::string reload_commands_show;
	std::string clear_commands_show;
	std::string devtools_toggle;

	/**
	 * Search entity commands
	 */
	menu_commands_search += "[" + Saylink::Silent("#list npcs", "NPC") + "] ";
	menu_commands_search += "[" + Saylink::Silent("#list players", "Players") + "] ";
	menu_commands_search += "[" + Saylink::Silent("#list corpses", "Corpses") + "] ";
	menu_commands_search += "[" + Saylink::Silent("#list doors", "Doors") + "] ";
	menu_commands_search += "[" + Saylink::Silent("#list objects", "Objects") + "] ";
	menu_commands_search += "[" + Saylink::Silent("#fz", "Zones") + "] ";
	menu_commands_search += "[" + Saylink::Silent("#itemsearch", "Items") + "] ";

	/**
	 * Show
	 */
	//menu_commands_show += "[" + EQ::SayLinkEngine::GenerateQuestSaylink("#showzonepoints", false, "Zone Points") + "] ";
	menu_commands_show += "[" + Saylink::Silent("#showzonegloballoot", "Zone Global Loot") + "] ";

	/**
	 * Reload
	 */
	reload_commands_show += "[" + Saylink::Silent("#reloadqst", "Quests") + "] ";
	reload_commands_show += "[" + Saylink::Silent("#reloadmerchants", "Merchants") + "] ";
	reload_commands_show += "[" + Saylink::Silent("#reloadallrules", "Rules Globally") + "] ";
	reload_commands_show += "[" + Saylink::Silent("#reloadstatic", "Zone Static Data") + "] ";
	reload_commands_show += "[" + Saylink::Silent("#reloadtraps", "Traps") + "] ";
	reload_commands_show += "[" + Saylink::Silent("#reloadzps", "Zone Points") + "] ";
	reload_commands_show += "[" + Saylink::Silent("#reloadcontentflags", "Content Flags") + "] ";

	/**
	 * Show window status
	 */
	devtools_toggle = "Disabled [" + Saylink::Silent("#devtools enable", "Enable") + "] ";
	if (IsDevToolsEnabled()) {
		devtools_toggle = "Enabled [" + Saylink::Silent("#devtools disable", "Disable") + "] ";
	}

	/**
	 * Print menu
	 */
	SendChatLineBreak();
	Message(Chat::White, "| [Devtools] %s Show this menu with %s", devtools_toggle.c_str(), Saylink::Silent("#devtools", "#dev").c_str());
	Message(Chat::White, "| [Devtools] Search %s", menu_commands_search.c_str());
	Message(Chat::White, "| [Devtools] Show %s", menu_commands_show.c_str());
	Message(Chat::White, "| [Devtools] Reload %s", reload_commands_show.c_str());
	Message(Chat::White, "| [Devtools] Search commands with #help <search>");
	SendChatLineBreak();
}

void Client::SendChatLineBreak(uint16 color) {
	Message(color, "------------------------------------------------");
}

uint16 Client::GetWeaponEffectID(int slot)
{
	if (slot != EQ::invslot::slotPrimary && slot != EQ::invslot::slotSecondary && slot != EQ::invslot::slotRange && slot != EQ::invslot::slotAmmo)
		return 0;

	EQ::ItemInstance* weaponInst = GetInv().GetItem(slot);
	const EQ::ItemData* weapon = nullptr;
	if (weaponInst)
		weapon = weaponInst->GetItem();

	if (weapon)
		return weapon->Proc.Effect;
	else
		return 0;
}

void Client::PermaGender(uint32 gender)
{
	SetBaseGender(gender);
	Save();
	SendIllusionPacket(gender);
}

bool Client::SendGMCommand(std::string message, bool ignore_status) {
	return command_dispatch(this, message.c_str()) >= 0 ? true : false;
}
