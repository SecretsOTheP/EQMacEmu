/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

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

	client_process.cpp:
	Handles client login sequence and packets sent from client to zone
*/

#include "../common/eqemu_logsys.h"
#include "../common/global_define.h"
#include <iostream>
#include <stdio.h>
#include <zlib.h>
#include <limits.h>

#ifdef _WINDOWS
	#include <winsock2.h>
	#include <windows.h>
	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#else
	#include <pthread.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <unistd.h>
#endif

#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/strings.h"
#include "event_codes.h"
#include "guild_mgr.h"
#include "map.h"
#include "petitions.h"
#include "queryserv.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"
#include "zone.h"
#include "zonedb.h"

#include <limits.h>

extern QueryServ* QServ;
extern Zone* zone;
extern volatile bool is_zone_loaded;
extern WorldServer worldserver;
extern PetitionList petition_list;
extern EntityList entity_list;

bool Client::Process() {
	bool ret = true;

	if (linkdead_timer.Check())
	{
		if (ClientDataLoaded())
		{
			Raid *myraid = entity_list.GetRaidByClient(this);
			if (myraid)
			{
				myraid->DisbandRaidMember(GetName());
			}
			if (IsGrouped())
				LeaveGroup();
			Save();
		}
		return false; //delete client
	}

	if(ClientDataLoaded() && (Connected() || IsLD()))
	{
		// try to send all packets that weren't sent before
		if(!IsLD() && zoneinpacket_timer.Check())
		{
			SendAllPackets();
		}

		//Accidental falling timer.
		if (accidentalfall_timer.Enabled())
		{
			if (accidentalfall_timer.Check())
			{
				accidentalfall_timer.Disable();
			}
		}

		if(dead)
		{
			SetHP(-100);
		}

		if (IsSitting())
		{
			if (!rested)
			{
				if (!rest_timer.Enabled())
				{
					rest_timer.Start(60000);
				}
				else if (rest_timer.Check())
				{
					rested = true;
					Log(Logs::General, Logs::Regen, "%s is now rested.", GetName());
					rest_timer.Disable();
				}
			}
		}
		else
		{
			if (rest_timer.Enabled() || rested)
			{
				rested = false;
				Log(Logs::General, Logs::Regen, "%s is no longer rested.", GetName());
				rest_timer.Disable();
			}
		}

		if(hpupdate_timer.Check())
		{
			CalcMaxHP();
			DoHPRegen();
		}

		if(mana_timer.Check())
			SendManaUpdatePacket();

		if(dead && dead_timer.Check()) 
		{
			m_pp.zone_id = m_pp.binds[0].zoneId;
			m_epp.zone_guild_id = GUILD_NONE;
			database.MoveCharacterToZone(GetName(), database.GetZoneName(m_pp.zone_id));

			glm::vec4 bindpts(m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, m_pp.binds[0].heading);
			m_Position = bindpts;
			Save();

			Group *mygroup = GetGroup();
			if (mygroup)
			{
				entity_list.MessageGroup(this,true,15,"%s died.", GetName());
				mygroup->MemberZoned(this);
			}
			Raid *myraid = entity_list.GetRaidByClient(this);
			if (myraid)
			{
				myraid->MemberZoned(this);
			}
			return(false);
		}

		if (zoning_timer.Check())
		{
			zone_mode = ZoneUnsolicited;
			zoning_timer.Disable();
		}

		if (camp_timer.Check())
		{
			// If a player starts to camp and then cancels it by typing /camp the server gets no packet telling us to set camping to false.
			// So, we let the timer continue to tick and set it false when it is checked. 
			camp_timer.Disable();
			camping = false;
			camp_desktop = false;
		}

		if (client_ld_timer.Check())
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
		}

		if (IsStunned() && stunned_timer.Check()) {
			this->stunned = false;
			this->stunned_timer.Disable();
			if (FindType(SE_SpinTarget))
				BuffFadeByEffect(SE_SpinTarget);
		}

		if (underwater_timer.Check())
		{
			if ((IsUnderWater() || GetZoneID() == thegrey) && 
				!spellbonuses.WaterBreathing && !aabonuses.WaterBreathing && !itembonuses.WaterBreathing)
			{
				if (m_pp.air_remaining > 0)
				{
					--m_pp.air_remaining;
				}
				else
				{
					++drowning;
					if (!GetGM() && drowning == 12)
					{
						database.SetHackerFlag(account_name, name, "Possible underwater breathing hack detected.");
						//Death(nullptr, 0, SPELL_UNKNOWN, static_cast<EQ::skills::SkillType>(251), Killed_ENV);
					}
				}
			}
			else
			{
				m_pp.air_remaining = CalculateLungCapacity();
				drowning = 0;
			}
		}

		if(!m_CheatDetectMoved)
		{
			m_TimeSinceLastPositionCheck = Timer::GetCurrentTime();
		}

		if (bardsong_timer.Check() && bardsong != 0) {
			//NOTE: this is kinda a heavy-handed check to make sure the mob still exists before
			//doing the next pulse on them...
			Mob *song_target = nullptr;
			if(bardsong_target_id == GetID()) {
				song_target = this;
			} else {
				song_target = entity_list.GetMob(bardsong_target_id);
			}

			if (!song_target || !ApplyNextBardPulse(bardsong, song_target, bardsong_slot))
			{
				if (interrupt_message > 0)
					InterruptSpell(interrupt_message, CC_User_SpellFailure, bardsong);
				else
					InterruptSpell();
			}
		}

		if(IsAIControlled())
			AI_Process();

		if (bindwound_timer.Check() && bindwound_target_id != 0) {
			if(BindWound(bindwound_target_id, false))
			{
				CheckIncreaseSkill(EQ::skills::SkillBindWound, nullptr, zone->skill_difficulty[EQ::skills::SkillBindWound].difficulty);
			}
			else
			{
				Log(Logs::General, Logs::Skills, "Bind wound failed, skillup check skipped.");
			}
		}

		if(KarmaUpdateTimer)
		{
			if(KarmaUpdateTimer->Check())
			{
				database.UpdateKarma(AccountID(), ++TotalKarma);
			}
		}

		if(qGlobals)
		{
			if(qglobal_purge_timer.Check())
			{
				qGlobals->PurgeExpiredGlobals();
			}
		}

		bool may_use_attacks = false;
		/*
			Things which prevent us from attacking:
				- being under AI control, the AI does attacks
				- being dead
				- casting a spell and bard check
				- not having a target
				- being stunned or mezzed
				- having used a ranged weapon recently
		*/
		if(auto_attack) {
			if(!IsAIControlled() && !dead && !IsSitting()
				&& !(spellend_timer.Enabled() && casting_spell_id && GetClass() != BARD)
				&& !IsStunned() && !IsFeared() && !IsMezzed() && GetAppearance() != eaDead && !IsMeleeDisabled()
				)
				may_use_attacks = true;

			if(may_use_attacks && ranged_timer.Enabled()) {
				//if the range timer is enabled, we need to consider it
				if(!ranged_timer.Check(false)) {
					//the ranged timer has not elapsed, cannot attack.
					may_use_attacks = false;
				}
			}
		}

		Mob *auto_attack_target = GetTarget();
		if (auto_attack && auto_attack_target != nullptr && may_use_attacks && attack_timer.CheckKeepSynchronized())
		{
			//check if change
			//only check on primary attack.. sorry offhand you gotta wait!
			if(aa_los_them_mob)
			{
				if(auto_attack_target != aa_los_them_mob ||
					m_AutoAttackPosition.x != GetX() ||
					m_AutoAttackPosition.y != GetY() ||
					m_AutoAttackPosition.z != GetZ() ||
					m_AutoAttackTargetLocation.x != aa_los_them_mob->GetX() ||
					m_AutoAttackTargetLocation.y != aa_los_them_mob->GetY() ||
					m_AutoAttackTargetLocation.z != aa_los_them_mob->GetZ())
				{
					aa_los_them_mob = auto_attack_target;
					m_AutoAttackPosition = GetPosition();
					m_AutoAttackTargetLocation = glm::vec3(aa_los_them_mob->GetPosition());
					los_status = CheckLosFN(auto_attack_target);
					los_status_facing = IsFacingMob(aa_los_them_mob);
				}
				// If only our heading changes, we can skip the CheckLosFN call
				// but above we still need to update los_status_facing
				if (m_AutoAttackPosition.w != GetHeading()) {
					m_AutoAttackPosition.w = GetHeading();
					los_status_facing = IsFacingMob(aa_los_them_mob);
				}
			}
			else
			{
				aa_los_them_mob = auto_attack_target;
				m_AutoAttackPosition = GetPosition();
				m_AutoAttackTargetLocation = glm::vec3(aa_los_them_mob->GetPosition());
				los_status = CheckLosFN(auto_attack_target);
				los_status_facing = IsFacingMob(aa_los_them_mob);
			}

			if (!CombatRange(auto_attack_target))
			{
				Message_StringID(MT_TooFarAway,TARGET_TOO_FAR);
			}
			else if (auto_attack_target == this)
			{
				Message_StringID(MT_TooFarAway,TRY_ATTACKING_SOMEONE);
			}
			else if (los_status && los_status_facing)
			{
				TryProcs(auto_attack_target, EQ::invslot::slotPrimary);
				Attack(auto_attack_target, EQ::invslot::slotPrimary);

				CheckIncreaseSkill(EQ::skills::SkillDoubleAttack, auto_attack_target, zone->skill_difficulty[EQ::skills::SkillDoubleAttack].difficulty);
				if (CheckDoubleAttack())
				{
					Attack(auto_attack_target, EQ::invslot::slotPrimary);

					// Triple attack: Warriors and Monks level 60+ do this.  13.5% looks weird but multiple 8+ hour logs suggest it's about that
					if ((GetClass() == WARRIOR || GetClass() == MONK) && GetLevel() >= 60 && zone->random.Int(0, 999) < 135)
					{
						Attack(auto_attack_target, EQ::invslot::slotPrimary);

						// Flurry AA
						if (auto_attack_target && aabonuses.FlurryChance)
						{
							if (zone->random.Int(0, 99) < aabonuses.FlurryChance)
							{
								Message_StringID(CC_Yellow, YOU_FLURRY);
								Attack(auto_attack_target, EQ::invslot::slotPrimary);

								if (zone->random.Roll(10))							// flurry is usually only +1 swings
									Attack(auto_attack_target, EQ::invslot::slotPrimary);
							}
						}
					}

					// Punishing Blade and Speed of the Knight AAs
					if (aabonuses.ExtraAttackChance)
					{
						EQ::ItemInstance *wpn = GetInv().GetItem(EQ::invslot::slotPrimary);
						if (wpn)
						{
							if (wpn->GetItem()->ItemType == EQ::item::ItemType2HSlash ||
								wpn->GetItem()->ItemType == EQ::item::ItemType2HBlunt ||
								wpn->GetItem()->ItemType == EQ::item::ItemType2HPiercing)
							{
								if (zone->random.Int(0, 99) < aabonuses.ExtraAttackChance)
								{
									Attack(auto_attack_target, EQ::invslot::slotPrimary);
								}
							}
						}
					}
				}
			}
		}

		if (auto_attack && may_use_attacks && auto_attack_target != nullptr
			&& attack_dw_timer.CheckKeepSynchronized() && IsDualWielding())
		{
			// Range check
			if(!CombatRange(auto_attack_target)) {
				// this is a duplicate message don't use it.
				//Message_StringID(MT_TooFarAway,TARGET_TOO_FAR);
			}
			// Don't attack yourself
			else if(auto_attack_target == this) {
				//Message_StringID(MT_TooFarAway,TRY_ATTACKING_SOMEONE);
			}
			else if (los_status && los_status_facing)
			{
				CheckIncreaseSkill(EQ::skills::SkillDualWield, auto_attack_target, zone->skill_difficulty[EQ::skills::SkillDualWield].difficulty);
				if (CheckDualWield())
				{
					TryProcs(auto_attack_target, EQ::invslot::slotSecondary);
					Attack(auto_attack_target, EQ::invslot::slotSecondary);

					// Bards and Beastlords get an AA that gives them double attack chance.  Sony's skill values are > 252 if the player doesn't have the skill.
					if ((GetSkill(EQ::skills::SkillDoubleAttack) >= 150 || aabonuses.GiveDoubleAttack) && CheckDoubleAttack())
					{
						Attack(auto_attack_target, EQ::invslot::slotSecondary);
					}
				}
			}
		}

		if (GetClass() == WARRIOR)
		{
			if (!HasDied() && !IsBerserk() && GetHPRatio() < RuleI(Combat, BerserkerFrenzyStart))
			{
				entity_list.MessageClose_StringID(this, false, 200, 0, BERSERK_START, GetName());
				berserk = true;
				CalcBonuses();
				SendBerserkState(true);
			}
			if (IsBerserk() && GetHPRatio() > RuleI(Combat, BerserkerFrenzyEnd))
			{
				entity_list.MessageClose_StringID(this, false, 200, 0, BERSERK_END, GetName());
				berserk = false;
				CalcBonuses();
				SendBerserkState(false);
			}
		}

		if (position_timer.Check()) {
			if (IsAIControlled())
			{
				if (!IsMoving())
				{
					animation = 0;
					m_Delta = glm::vec4(0.0f, 0.0f, 0.0f, m_Delta.w);
					SendPosUpdate(2);
				}
			}

			// Send a position packet every 9 seconds - if not done, other clients
			// see this char disappear after 10-12 seconds of inactivity
			if (position_timer_counter >= 36) { // Approx. 4 ticks per second
				entity_list.SendPositionUpdates(this);
				position_timer_counter = 0;
			}
			else {
				position_timer_counter++;
			}
		}

		if (GetClass() == WARRIOR && GetShieldTarget())
		{
			if (GetShieldTarget()->IsCorpse() || GetShieldTarget()->GetHP() < 1 || GetShieldTarget()->CastToClient()->IsDead()
				|| shield_timer.Check() || DistanceSquared(GetPosition(), GetShieldTarget()->GetPosition()) > (16.0f*16.0f)
			)
				EndShield();
		}

		SpellProcess();

		ProcessHungerThirst();

		if(disc_ability_timer.Check())
		{
			disc_ability_timer.Disable();
			if (active_disc_spell && IsValidSpell(active_disc_spell))
			{
				this->Message(MT_Disciplines, "%s", spells[active_disc_spell].spell_fades);
			}
			FadeDisc();

			auto outapp = new EQApplicationPacket(OP_DisciplineChange, sizeof(ClientDiscipline_Struct));
			ClientDiscipline_Struct *d = (ClientDiscipline_Struct*)outapp->pBuffer;
			d->disc_id = 0;
			QueuePacket(outapp);
			safe_delete(outapp);
		}

		if (tic_timer.Check() && !dead) 
		{
			ProcessFatigue();
			CalcMaxMana();
			DoManaRegen();
			BuffProcess();

			if (fishing_timer.Check()) 
			{
				GoFish();
			}

			if (autosave_timer.Check()) 
			{
				Save(0);
			}

			if(m_pp.intoxication > 0)
			{
				--m_pp.intoxication;
				CalcBonuses();
			}

			if(ItemTickTimer.Check())
			{
				TickItemCheck();
			}

			if(ItemQuestTimer.Check())
			{
				ItemTimerCheck();
			}
		}

		if (apperance_timer.Check())
		{
			apperance_timer.Disable();
		}

		// Right now, only veeshan has floor teleports. If more are discovered, create a method to determine which zones need the timer.
		if (GetZoneID() == veeshan && !door_check_timer.Enabled())
		{
			door_check_timer.Start();
		}

		if (mend_reset_timer.Check())
		{
			ResetSkill(EQ::skills::SkillMend);
			mend_reset_timer.Disable();
		}
	}

	if (client_state == CLIENT_KICKED) {
		Save();
		OnDisconnect(true);
		Log(Logs::General, Logs::Status, "Client disconnected (cs=k): %s", GetName());
		return false;
	}

	if (client_state == DISCONNECTED) {
		OnDisconnect(true);
		Log(Logs::General, Logs::Error, "Client disconnected (cs=d): %s", GetName());
		database.SetMQDetectionFlag(this->AccountName(), GetName(), "/MQInstantCamp: Possible instant camp disconnect.", zone->GetShortName());
		return false;
	}

	if (client_state == CLIENT_WAITING_FOR_AUTH || client_state == CLIENT_AUTH_RECEIVED) {
		if (get_auth_timer.Check()) {
			Log(Logs::General, Logs::Error, "GetAuth() timed out waiting, kicking client");
			client_state = CLIENT_KICKED;
			return true;
		}
		if (zone->CheckAuth(GetName())) {
			// we have an auth
			get_auth_timer.Stop();
			Log(Logs::General, Logs::Error, "GetAuth() arrived, processing");
			if (zoneentry != nullptr) {
				client_state = CLIENT_AUTH_RECEIVED;
				ret = HandlePacket(zoneentry);
				safe_delete(zoneentry);
				zoneentry = nullptr;
			}
			else {
				client_state = CLIENT_KICKED;
				return true;
			}
			client_state = CLIENT_CONNECTING;
		}
		else {
			return true;
		}
	}

	/************ Get all packets from packet manager out queue and process them ************/
	EQApplicationPacket *app = nullptr;

	//Predisconnecting is a state where we expect a zone change packet, and the next packet HAS to be a zone change packet once you request to zone. Otherwise, bad things happen!
	if(!eqs->CheckState(CLOSING) && client_state != PREDISCONNECTED && client_state != ZONING)
	{
		while(ret && (app = (EQApplicationPacket *)eqs->PopPacket())) {
			if(app)
				ret = HandlePacket(app);
			safe_delete(app);
		}
	}

	//At this point, we are still connected, everything important has taken
	//place, now check to see if anybody wants to aggro us.
	// only if client is not feigned
	if(ClientDataLoaded() && ret && scanarea_timer.Check()) {
		entity_list.CheckClientAggro(this);
	}

	if (client_state != CLIENT_LINKDEAD && (client_state == PREDISCONNECTED || client_state == CLIENT_ERROR || client_state == DISCONNECTED || client_state == CLIENT_KICKED || !eqs->CheckState(ESTABLISHED)))
	{
		//client logged out or errored out
		//ResetTrade();
		if (client_state != CLIENT_KICKED) {
			Save();
		}

		client_state = CLIENT_LINKDEAD;

		if (IsSitting())
		{
			Stand();
		}

		if (zoning || instalog || GetGM())
		{
			Group *mygroup = GetGroup();
			if (mygroup)
			{
				if (!zoning)
				{
					entity_list.MessageGroup(this,true,CC_Yellow,"%s logged out.",GetName());
					mygroup->DelMember(this);
				}
				else
				{
					mygroup->MemberZoned(this);
				}

			}
			Raid *myraid = entity_list.GetRaidByClient(this);
			if (myraid)
			{
				if (!zoning)
				{
					myraid->DisbandRaidMember(GetName());
				}
				else
				{
					myraid->MemberZoned(this);
				}
			}
			OnDisconnect(false);
			return false;
		}
		else
		{
			if (camping && camp_timer.Enabled() && camp_timer.GetRemainingTime() < 10000) {
				auto outapp = new EQApplicationPacket(OP_LogoutReply, 2);
				FastQueuePacket(&outapp);
				OnDisconnect(true);
			}
			else {
				Log(Logs::General, Logs::ZoneServer, "Client linkdead: %s", name);
				LinkDead();
			}
		}
	}

	return ret;
}

/* Just a set of actions preformed all over in Client::Process */
void Client::OnDisconnect(bool hard_disconnect) {
	database.CharacterQuit(this->CharacterID());
	if(hard_disconnect)
	{
		LeaveGroup();

		Raid *MyRaid = entity_list.GetRaidByClient(this);

		if (MyRaid)
			MyRaid->DisbandRaidMember(GetName());

		parse->EventPlayer(EVENT_DISCONNECT, this, "", 0);

		/* QS: PlayerLogConnectDisconnect */
		if (RuleB(QueryServ, PlayerLogConnectDisconnect)){
			std::string event_desc = StringFormat("Disconnect :: in zoneid:%i", this->GetZoneID());
			QServ->PlayerLogEvent(Player_Log_Connect_State, this->CharacterID(), event_desc);
		}
	}

	Mob *Other = trade->With();
	if(Other)
	{
		Log(Logs::Detail, Logs::Trading, "Client disconnected during a trade. Returning their items."); 
		FinishTrade(this);

		if(Other->IsClient())
			Other->CastToClient()->FinishTrade(Other);

		/* Reset both sides of the trade */
		trade->Reset();
		Other->trade->Reset();
	}

	database.SetFirstLogon(CharacterID(), 0); //We change firstlogon status regardless of if a player logs out to zone or not, because we only want to trigger it on their first login from world.

	/* Remove ourself from all proximities */
	ClearAllProximities();

	//Prevent GMs from being kicked all the way when camping.
	if(GetGM())
	{
		auto outapp = new EQApplicationPacket(OP_LogoutReply, 2);
		FastQueuePacket(&outapp);
		
		Disconnect();
	}
	else
	{
		if (camp_desktop) {
			HardDisconnect();
		}
		else {
			Disconnect();
		}
	}

}

// Sends the client complete inventory used in character login
void Client::BulkSendInventoryItems() {

	EQ::OutBuffer ob;
	EQ::OutBuffer::pos_type last_pos = ob.tellp();

	auto iter = m_inv.cursor_cbegin();
	if (iter != m_inv.cursor_cend()) {
		const EQ::ItemInstance *inst = *iter;
		if (inst) {
			inst->Serialize(ob, EQ::invslot::slotCursor);
			if (ob.tellp() == last_pos)
				Log(Logs::General, Logs::Inventory, "Serialization failed on item slot %d during BulkSendInventoryItems.  Item skipped.", EQ::invslot::slotCursor);

			last_pos = ob.tellp();
		}
	}

	//Inventory items
	for (int16 slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id < EQ::invtype::POSSESSIONS_SIZE; slot_id++) {
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (!inst)
			continue;

		inst->Serialize(ob, slot_id);

		if (ob.tellp() == last_pos)
			Log(Logs::General, Logs::Inventory, "Serialization failed on item slot %d during BulkSendInventoryItems.  Item skipped.", slot_id);

		last_pos = ob.tellp();
	}

	//Items in Character Inventory Bags
	for (int16 slot_id = EQ::invbag::GENERAL_BAGS_BEGIN; slot_id <= EQ::invbag::CURSOR_BAG_END; slot_id++) {
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (!inst)
			continue;

		inst->Serialize(ob, slot_id);

		if (ob.tellp() == last_pos)
			Log(Logs::General, Logs::Inventory, "Serialization failed on item slot %d during BulkSendInventoryItems.  Item skipped.", slot_id);

		last_pos = ob.tellp();
	}	

	// Bank items
	for(int16 slot_id = EQ::invslot::BANK_BEGIN; slot_id <= EQ::invslot::BANK_END; slot_id++) {
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (!inst)
			continue;

		inst->Serialize(ob, slot_id);

		if (ob.tellp() == last_pos)
			Log(Logs::General, Logs::Inventory, "Serialization failed on item slot %d during BulkSendInventoryItems.  Item skipped.", slot_id);

		last_pos = ob.tellp();
	}

	//Items in Bank Bags
	for (int16 slot_id = EQ::invbag::BANK_BAGS_BEGIN; slot_id <= EQ::invbag::BANK_BAGS_END; slot_id++) {
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (!inst)
			continue;

		inst->Serialize(ob, slot_id);

		if (ob.tellp() == last_pos)
			Log(Logs::General, Logs::Inventory, "Serialization failed on item slot %d during BulkSendInventoryItems.  Item skipped.", slot_id);

		last_pos = ob.tellp();
	}	

	std::string serialized = ob.str();

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_CharInventory, serialized.size());
	memcpy(outapp->pBuffer, serialized.c_str(), serialized.size());

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendCursorItems()
{
	/* Send stuff on the cursor which isnt sent in bulk */
	bool first_item = true;
	for (auto iter = m_inv.cursor_cbegin(); iter != m_inv.cursor_cend(); ++iter) {
		const EQ::ItemInstance *inst = *iter;
		
		if (!first_item)
			SendItemPacket(0, inst, ItemPacketSummonItem);
		first_item = false;
	}
}

void Client::FillPPItems()
{
	int16 slot_id = 0;
	int i = 0;
	memset(m_pp.invItemProperties, 0, sizeof(ItemProperties_Struct)*30);
	for (slot_id = EQ::invslot::slotCursor; slot_id <= EQ::invslot::GENERAL_END; slot_id++)
	{
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst){
			m_pp.inventory[i] = inst->GetItem()->ID;
			m_pp.invItemProperties[i].charges = inst->GetCharges();
		}
		else
			m_pp.inventory[i] = 0xFFFF;

		++i;
	}

	i = 0;
	memset(m_pp.bagItemProperties, 0, sizeof(ItemProperties_Struct)*80);
	for (slot_id = EQ::invbag::GENERAL_BAGS_BEGIN; slot_id <= EQ::invbag::GENERAL_BAGS_END; slot_id++) 
	{
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst){
			m_pp.containerinv[i] = inst->GetItem()->ID;
			m_pp.bagItemProperties[i].charges = inst->GetCharges();
		}
		else
			m_pp.containerinv[i] = 0xFFFF;

		++i;
	}

	i = 0;
	memset(m_pp.cursorItemProperties, 0, sizeof(ItemProperties_Struct)*10);
	for (slot_id = EQ::invbag::CURSOR_BAG_BEGIN; slot_id <= EQ::invbag::CURSOR_BAG_END; slot_id++) 
	{
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst){
			m_pp.cursorbaginventory[i] = inst->GetItem()->ID;
			m_pp.cursorItemProperties[i].charges = inst->GetCharges();
		}
		else
			m_pp.cursorbaginventory[i] = 0xFFFF;

		++i;
	}

	i = 0;
	memset(m_pp.bankinvitemproperties, 0, sizeof(ItemProperties_Struct)*8);
	for (slot_id = EQ::invslot::BANK_BEGIN; slot_id <= EQ::invslot::BANK_END; slot_id++) 
	{
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst){
			m_pp.bank_inv[i] = inst->GetItem()->ID;
			m_pp.bankinvitemproperties[i].charges = inst->GetCharges();
		}
		else
			m_pp.bank_inv[i] = 0xFFFF;

		++i;
	}

	i = 0;
	memset(m_pp.bankbagitemproperties, 0, sizeof(ItemProperties_Struct)*80);
	for (slot_id = EQ::invbag::BANK_BAGS_BEGIN; slot_id <= EQ::invbag::BANK_BAGS_END; slot_id++) 
	{
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst){
			m_pp.bank_cont_inv[i] = inst->GetItem()->ID;
			m_pp.bankbagitemproperties[i].charges = inst->GetCharges();
		}
		else
			m_pp.bank_cont_inv[i] = 0xFFFF;

		++i;
	}
}

void Client::BulkSendMerchantInventory(int merchant_id, int npcid) 
{
	const EQ::ItemData *item = nullptr;
	std::list<MerchantList> merlist = zone->merchanttable[merchant_id];
	std::list<MerchantList>::const_iterator itr;
	Mob* merch = entity_list.GetMobByNpcTypeID(npcid);
	if (merlist.size() == 0) { //Attempt to load the data, it might have been missed if someone spawned the merchant after the zone was loaded
		zone->LoadNewMerchantData(merchant_id);
		merlist = zone->merchanttable[merchant_id];
		if (merchant_id == 0)
			return;
	}

	std::list<TempMerchantList> tmp_merlist = zone->tmpmerchanttable[npcid];
	std::list<TempMerchantList>::iterator tmp_itr;

	uint32 size = 0;
	uint16 m = 0;
	std::map<uint16, std::string> ser_items;
	std::map<uint16, std::string>::iterator mer_itr;

	uint32 i=0;
	std::list<MerchantList> orig_merlist = zone->merchanttable[merchant_id];
	merlist.clear();
	
	if (merchant_id != 1)
	{
		for (itr = orig_merlist.begin(); itr != orig_merlist.end(); ++itr) {
			MerchantList ml = *itr;
			ml.slot = i;

			bool expansion_enabled = RuleR(World, CurrentExpansion) >= ml.min_expansion && RuleR(World, CurrentExpansion) < ml.max_expansion;
			bool expansion_all = ml.min_expansion == 0.0f && ml.max_expansion == 0.0f;

			if (!expansion_enabled && !expansion_all)
			{
				continue;
			}

			if (GetLevel() < ml.level_required)
				continue;

			if (!(ml.classes_required & (1 << (GetClass() - 1))) && GetClass() != 0)
				continue;

			item = database.GetItem(ml.item);
			if (!item)
				continue;

			int32 fac = merch ? merch->GetPrimaryFaction() : 0;
			int32 facmod = GetModCharacterFactionLevel(fac);
			if (IsInvisible(merch))
				facmod = 0;
			if (fac != 0 && facmod < ml.faction_required && zone->CanDoCombat())
			{
				Log(Logs::General, Logs::Trading, "Item %d is being skipped due to bad faction. Faction: %d Required: %d", ml.item, facmod, ml.faction_required);
				continue;
			}

			if (ml.quantity > 0)
			{
				if (ml.qty_left <= 0)
				{
					Log(Logs::General, Logs::Trading, "Merchant is skipping item %d that has %d left.", ml.item, ml.qty_left);
					continue;
				}
				else
				{
					Log(Logs::General, Logs::Trading, "Merchant is sending item %d that has %d left in slot %d.", ml.item, ml.qty_left, ml.slot);
				}
			}

			item = database.GetItem(ml.item);
			if (item) {
				int charges = 1;
				if (item->ItemClass == EQ::item::ItemClassCommon)
					charges = item->MaxCharges;
				EQ::ItemInstance* inst = database.CreateItem(item, charges);
				if (inst) {
					inst->SetPrice(item->Price * item->SellRate);
					inst->SetMerchantSlot(ml.slot);
					inst->SetMerchantCount(-1);		//unlimited
					if (charges > 0)
						inst->SetCharges(charges);
					else
						inst->SetCharges(1);

					if (inst)
					{
						std::string packet(inst->Serialize(ml.slot - 1));
						Log(Logs::Moderate, Logs::Trading, "PERM (%d): %s was added to merchant in slot %d with price %d", i, item->Name, ml.slot, inst->GetPrice());
						ser_items[m] = packet;
						size += packet.length();
						m++;
					}
				}
			}
			merlist.push_back(ml);
			++i;
		}
		if (!IsSoloOnly() && !IsSelfFound())
		{
			std::list<TempMerchantList> origtmp_merlist = zone->tmpmerchanttable[npcid];
			tmp_merlist.clear();
			for (tmp_itr = origtmp_merlist.begin(); tmp_itr != origtmp_merlist.end(); ++tmp_itr) {
				TempMerchantList ml = *tmp_itr;
				item = database.GetItem(ml.item);
				ml.slot = i;
				if (item) {
					int charges = 1;
					if (database.ItemQuantityType(item->ID) == EQ::item::Quantity_Charges)
					{
						charges = zone->GetTempMerchantQtyNoSlot(npcid, item->ID);
					}
					EQ::ItemInstance* inst = database.CreateItem(item, charges);
					if (inst) {
						uint32 capped_charges = ml.charges > MERCHANT_CHARGE_CAP ? MERCHANT_CHARGE_CAP : ml.charges;
						inst->SetPrice(item->Price * item->SellRate);
						inst->SetMerchantSlot(ml.slot);
						inst->SetMerchantCount(capped_charges);
						inst->SetCharges(charges);

						if (inst)
						{
							std::string packet = inst->Serialize(ml.slot - 1);
							ser_items[m] = packet;
							size += packet.length();
							m++;
						}
						Log(Logs::Moderate, Logs::Trading, "TEMP (%d): %s was added to merchant in slot %d with %d count and %d charges and price %d", i, item->Name, ml.slot, capped_charges, charges, inst->GetPrice());
					}
				}
				tmp_merlist.push_back(ml);
				++i;

				// 80 inventory slots + 10 "hidden" items.
				if (i > 89)
				{
					Log(Logs::Moderate, Logs::Trading, "Item at position %d is not being added.", i);
					break;
				}
			}
		}
	}
	else
	{
		std::list<TempMerchantList> origtmp_merlist = item_reimbursement_list;
		tmp_merlist.clear();
		for (tmp_itr = origtmp_merlist.begin(); tmp_itr != origtmp_merlist.end(); ++tmp_itr) {
			TempMerchantList ml = *tmp_itr;
			item = database.GetItem(ml.item);
			ml.slot = i;
			if (item) {
				int charges = ml.charges;
				EQ::ItemInstance* inst = database.CreateItem(item, charges);
				if (inst) {
					uint32 capped_charges = ml.charges > MERCHANT_CHARGE_CAP ? MERCHANT_CHARGE_CAP : ml.charges;
					inst->SetPrice(item->Price * item->SellRate);
					inst->SetMerchantSlot(ml.slot);
					inst->SetMerchantCount(capped_charges);
					inst->SetCharges(charges);

					if (inst)
					{
						std::string packet = inst->Serialize(ml.slot - 1);
						ser_items[m] = packet;
						size += packet.length();
						m++;
					}
					Log(Logs::Moderate, Logs::Trading, "TEMP (%d): %s was added to merchant in slot %d with %d count and %d charges and price %d", i, item->Name, ml.slot, capped_charges, charges, inst->GetPrice());
				}
			}
			tmp_merlist.push_back(ml);
			++i;

			// 80 inventory slots + 10 "hidden" items.
			if (i > 80)
			{
				Log(Logs::Moderate, Logs::Trading, "Item at position %d is not being added.", i);
				break;
			}
		}
	}

	uint8 lastslot = i;

	uint32 entityid = 0;
	if(merch)
		entityid = merch->GetID();

	auto delitempacket = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
	Merchant_DelItem_Struct* delitem = (Merchant_DelItem_Struct*)delitempacket->pBuffer;
	delitem->itemslot = lastslot;
	delitem->npcid = entityid;
	delitem->playerid = GetID();
	delitempacket->priority = 6;
	QueuePacket(delitempacket);
	safe_delete(delitempacket);
	Log(Logs::General, Logs::Trading, "Cleared last merchant slot %d", lastslot);

	//this resets the slot
	zone->merchanttable[merchant_id] = merlist;
	if (merchant_id != 1)
		zone->tmpmerchanttable[npcid] = tmp_merlist;
	else
		item_reimbursement_list = tmp_merlist;
	int8 count = 0;
	auto outapp = new EQApplicationPacket(OP_ShopInventoryPacket, size);
	uchar* ptr = outapp->pBuffer;
	for(mer_itr = ser_items.begin(); mer_itr != ser_items.end(); mer_itr++)
	{
		int length = mer_itr->second.length();
		if(count < 80 && length > 5) {
			memcpy(ptr, mer_itr->second.c_str(), length);
			ptr += length;
			count++;
		}
	}
	ser_items.clear();
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::MerchantWelcome(int merchant_id, int npcid)
{
	const EQ::ItemData* handyitem = nullptr;
	const EQ::ItemData* item;
	std::list<MerchantList> merlist = zone->merchanttable[merchant_id];
	std::list<MerchantList>::const_iterator itr;
	Mob* merch = entity_list.GetMobByNpcTypeID(npcid);
	if (merlist.size() == 0) { //Attempt to load the data, it might have been missed if someone spawned the merchant after the zone was loaded
		zone->LoadNewMerchantData(merchant_id);
		merlist = zone->merchanttable[merchant_id];
		if (merlist.size() == 0)
			return;
	}

	std::list<TempMerchantList> tmp_merlist = zone->tmpmerchanttable[npcid];
	std::list<TempMerchantList>::iterator tmp_itr;

	uint32 i = 0;
	uint8 handychance = 0;
	std::list<MerchantList> orig_merlist = zone->merchanttable[merchant_id];
	merlist.clear();

	for (itr = orig_merlist.begin(); itr != orig_merlist.end(); ++itr) {
		MerchantList ml = *itr;
		ml.slot = i;

		handychance = zone->random.Int(0, merlist.size() + tmp_merlist.size() - 1);

		item = database.GetItem(ml.item);
		if (item) {
			if (handychance == 0)
				handyitem = item;
			else
				handychance--;
		}
		merlist.push_back(ml);
		++i;
	}
	std::list<TempMerchantList> origtmp_merlist = zone->tmpmerchanttable[npcid];
	tmp_merlist.clear();
	for (tmp_itr = origtmp_merlist.begin(); tmp_itr != origtmp_merlist.end(); ++tmp_itr) {
		TempMerchantList ml = *tmp_itr;
		item = database.GetItem(ml.item);
		ml.slot = i;
		if (item) {
			if (handychance == 0)
				handyitem = item;
			else
				handychance--;
		}
		tmp_merlist.push_back(ml);
		++i;
	}

	if (merch != nullptr && handyitem) {
		char handy_id[8] = { 0 };
		int greeting = zone->random.Int(0, 4);
		int greet_id = 0;
		switch (greeting) {
		case 1:
			greet_id = MERCHANT_GREETING;
			break;
		case 2:
			greet_id = MERCHANT_HANDY_ITEM1;
			break;
		case 3:
			greet_id = MERCHANT_HANDY_ITEM2;
			break;
		case 4:
			greet_id = MERCHANT_HANDY_ITEM3;
			break;
		default:
			greet_id = MERCHANT_HANDY_ITEM4;
		}
		sprintf(handy_id, "%i", greet_id);

		if (greet_id != MERCHANT_GREETING)
			Message_StringID(CC_Default, GENERIC_STRINGID_SAY, merch->GetCleanName(), handy_id, this->GetName(), handyitem->Name);
		else
			Message_StringID(CC_Default, GENERIC_STRINGID_SAY, merch->GetCleanName(), handy_id, this->GetName());
	}
}

void Client::OPRezzAnswer(uint32 Action, uint32 SpellID, uint16 ZoneID, uint32 GuildZoneID, float x, float y, float z)
{
	if(PendingRezzXP < 0 || PendingRezzZoneID == 0) {
		// pendingrezexp is set to -1 if we are not expecting an OP_RezzAnswer
		Log(Logs::Detail, Logs::Spells, "Unexpected OP_RezzAnswer. Ignoring it.");
		Message(CC_Red, "You have already been resurrected.\n");
		return;
	}

	if (Action == 1)
	{
		// Mark the corpse as rezzed in the database, just in case the corpse has buried, or the zone the
		// corpse is in has shutdown since the rez spell was cast.
		database.MarkCorpseAsRezzed(PendingRezzDBID);
		Log(Logs::Detail, Logs::Spells, "Player %s got a %i Rezz, spellid %i in zone%i",
				this->name, (uint16)spells[SpellID].base[0],
				SpellID, ZoneID);

		this->BuffFadeNonPersistDeath();
		int SpellEffectDescNum = GetSpellEffectDescNum(SpellID);
		// Rez spells with Rez effects have this DescNum
		if(SpellEffectDescNum == 39067) {
			SetMana(0);
			SetHP(GetMaxHP()/5);
			if(!GetGM())
				SpellOnTarget(SPELL_RESURRECTION_EFFECTS, this); // Rezz effects
		}
		else {
			SetMana(GetMaxMana());
			SetHP(GetMaxHP());
		}
		if(spells[SpellID].base[0] < 100 && spells[SpellID].base[0] > 0 && PendingRezzXP > 0)
		{
				SetEXP(((int)(GetEXP()+((float)((PendingRezzXP / 100) * spells[SpellID].base[0])))),
						GetAAXP(),true);
		}
		else if (spells[SpellID].base[0] == 100 && PendingRezzXP > 0) {
			SetEXP((GetEXP() + PendingRezzXP), GetAAXP(), true);
		}

		entity_list.RemoveFromTargets(this);

		//Was sending the packet back to initiate client zone...
		//but that could be abusable, so lets go through proper channels
		if(PendingRezzZoneID != 0 && PendingRezzZoneGuildID != 0)
			MovePCGuildID(PendingRezzZoneID, PendingRezzZoneGuildID, x, y, z, GetHeading() * 2.0f, 0, ZoneSolicited);
	}
	PendingRezzZoneID = 0;
	PendingRezzZoneGuildID = 0;
	PendingRezzXP = -1;
	PendingRezzSpellID = 0;
}

void Client::OPTGB(const EQApplicationPacket *app)
{
	if(!app) return;
	if(!app->pBuffer) return;

	uint32 tgb_flag = *(uint32 *)app->pBuffer;
	if(tgb_flag == 2)
		Message_StringID(CC_Default, TGB() ? TGB_ON : TGB_OFF);
	else
		tgb = tgb_flag;
}

void Client::OPMemorizeSpell(const EQApplicationPacket* app)
{
	if(app->size != sizeof(MemorizeSpell_Struct))
	{
		Log(Logs::General, Logs::Error, "Wrong size on OP_MemorizeSpell. Got: %i, Expected: %i", app->size, sizeof(MemorizeSpell_Struct));
		DumpPacket(app);
		return;
	}

	const MemorizeSpell_Struct* memspell = (const MemorizeSpell_Struct*) app->pBuffer;

	if(!IsValidSpell(memspell->spell_id))
	{
		Message(CC_Red, "Unexpected error: spell id out of range");
		return;
	}

	if
	(
		GetClass() > 16 ||
		GetLevel() < spells[memspell->spell_id].classes[GetClass()-1]
	)
	{
		char val1[20]={0};
		Message_StringID(CC_Red,SPELL_LEVEL_TO_LOW,ConvertArray(spells[memspell->spell_id].classes[GetClass()-1],val1),spells[memspell->spell_id].name);
		//Message(CC_Red, "Unexpected error: Class cant use this spell at your level!");
		return;
	}

	switch(memspell->scribing)
	{
		case memSpellScribing:	{	// scribing spell to book
			const EQ::ItemInstance* inst = m_inv[EQ::invslot::slotCursor];

			if(inst && inst->IsType(EQ::item::ItemClassCommon))
			{
				const EQ::ItemData* item = inst->GetItem();

				if(item && item->Scroll.Effect == (int32)(memspell->spell_id))
				{
					ScribeSpell(memspell->spell_id, memspell->slot);
					DeleteItemInInventory(EQ::invslot::slotCursor, 0, true);
				}
				else {
					Message_StringID(MT_Spells, ABORTED_SCRIBING_SPELL);
					SendSpellBarEnable(0); // if we don't send this, the client locks up
				}
			}
			else {
				Message_StringID(MT_Spells, ABORTED_SCRIBING_SPELL);
				SendSpellBarEnable(0);
			}
			break;
		}
		case memSpellMemorize:	{	// memming spell
			if(HasSpellScribed(memspell->spell_id))
			{
				MemSpell(memspell->spell_id, memspell->slot);
			}
			else
			{
				database.SetMQDetectionFlag(AccountName(), GetName(), "OP_MemorizeSpell but we don't have this spell scribed...", zone->GetShortName());
			}
			break;
		}
		case memSpellForget:	{	// unmemming spell
			UnmemSpell(memspell->slot);
			break;
		}
	}

	Save();
}

static uint64 CoinTypeCoppers(uint32 type) {
	switch(type) {
	case COINTYPE_PP:
		return(1000);
	case COINTYPE_GP:
		return(100);
	case COINTYPE_SP:
		return(10);
	case COINTYPE_CP:
	default:
		break;
	}
	return(1);
}

void Client::OPMoveCoin(const EQApplicationPacket* app)
{
	MoveCoin_Struct* mc = (MoveCoin_Struct*)app->pBuffer;
	uint64 value = 0, amount_to_take = 0, amount_to_add = 0;
	int32 *from_bucket = 0, *to_bucket = 0;
	Mob* trader = trade->With();

	// if amount < 0, client is sending a malicious packet
	if (mc->amount < 0)
	{
		return;
	}
	
	// could just do a range, but this is clearer and explicit
	if
	(
		(
			mc->cointype1 != COINTYPE_PP &&
			mc->cointype1 != COINTYPE_GP &&
			mc->cointype1 != COINTYPE_SP &&
			mc->cointype1 != COINTYPE_CP
		) ||
		(
			mc->cointype2 != COINTYPE_PP &&
			mc->cointype2 != COINTYPE_GP &&
			mc->cointype2 != COINTYPE_SP &&
			mc->cointype2 != COINTYPE_CP
		)
	)
	{
		return;
	}

	switch(mc->from_slot)
	{
		case -1:	// destroy
		{
			// I don't think you can move coin from the void,
			// but need to check this
			break;
		}
		case 0:	// cursor
		{
			switch(mc->cointype1)
			{
				case COINTYPE_PP:
					from_bucket = (int32 *) &m_pp.platinum_cursor; break;
				case COINTYPE_GP:
					from_bucket = (int32 *) &m_pp.gold_cursor; break;
				case COINTYPE_SP:
					from_bucket = (int32 *) &m_pp.silver_cursor; break;
				case COINTYPE_CP:
					from_bucket = (int32 *) &m_pp.copper_cursor; break;
			}
			break;
		}
		case 1:	// inventory
		{
			switch(mc->cointype1)
			{
				case COINTYPE_PP:
					from_bucket = (int32 *) &m_pp.platinum; break;
				case COINTYPE_GP:
					from_bucket = (int32 *) &m_pp.gold; break;
				case COINTYPE_SP:
					from_bucket = (int32 *) &m_pp.silver; break;
				case COINTYPE_CP:
					from_bucket = (int32 *) &m_pp.copper; break;
			}
			break;
		}
		case 2:	// bank
		{
			uint32 distance = 0;
			NPC *banker = entity_list.GetClosestBanker(this, distance);
			if(!banker || distance > USE_NPC_RANGE2)
			{
				auto hacked_string = fmt::format("Player tried to make use of a banker(coin move) but {} is non-existant or too far away ( {} units).",
					banker ? banker->GetName() : "UNKNOWN NPC", distance);
				database.SetMQDetectionFlag(AccountName(), GetName(), hacked_string, zone->GetShortName());
				return;
			}

			switch(mc->cointype1)
			{
				case COINTYPE_PP:
					from_bucket = (int32 *) &m_pp.platinum_bank; break;
				case COINTYPE_GP:
					from_bucket = (int32 *) &m_pp.gold_bank; break;
				case COINTYPE_SP:
					from_bucket = (int32 *) &m_pp.silver_bank; break;
				case COINTYPE_CP:
					from_bucket = (int32 *) &m_pp.copper_bank; break;
			}
			break;
		}
		case 3:	// trade
		{
			// can't move coin from trade
			break;
		}
	}

	switch(mc->to_slot)
	{
		case -1:	// destroy
		{
			// no action required
			break;
		}
		case 0:	// cursor
		{
			switch(mc->cointype2)
			{
				case COINTYPE_PP:
					to_bucket = (int32 *) &m_pp.platinum_cursor; break;
				case COINTYPE_GP:
					to_bucket = (int32 *) &m_pp.gold_cursor; break;
				case COINTYPE_SP:
					to_bucket = (int32 *) &m_pp.silver_cursor; break;
				case COINTYPE_CP:
					to_bucket = (int32 *) &m_pp.copper_cursor; break;
			}
			break;
		}
		case 1:	// inventory
		{
			switch(mc->cointype2)
			{
				case COINTYPE_PP:
					to_bucket = (int32 *) &m_pp.platinum; break;
				case COINTYPE_GP:
					to_bucket = (int32 *) &m_pp.gold; break;
				case COINTYPE_SP:
					to_bucket = (int32 *) &m_pp.silver; break;
				case COINTYPE_CP:
					to_bucket = (int32 *) &m_pp.copper; break;
			}
			break;
		}
		case 2:	// bank
		{
			uint32 distance = 0;
			NPC *banker = entity_list.GetClosestBanker(this, distance);
			if(!banker || distance > USE_NPC_RANGE2)
			{
				auto hacked_string = fmt::format("Player tried to make use of a banker(coin move) but {} is non-existant or too far away ( {} units).",
					banker ? banker->GetName() : "UNKNOWN NPC", distance);
				database.SetMQDetectionFlag(AccountName(), GetName(), hacked_string, zone->GetShortName());
				return;
			}
			switch(mc->cointype2)
			{
				case COINTYPE_PP:
					to_bucket = (int32 *) &m_pp.platinum_bank; break;
				case COINTYPE_GP:
					to_bucket = (int32 *) &m_pp.gold_bank; break;
				case COINTYPE_SP:
					to_bucket = (int32 *) &m_pp.silver_bank; break;
				case COINTYPE_CP:
					to_bucket = (int32 *) &m_pp.copper_bank; break;
			}
			break;
		}
		case 3:	// trade
		{
			// we have to move the coin, with or without a trader.  Otherwise the coin gets lost
			// check later after coin movements done if there is a trader and cancel trade as necessary.
			switch(mc->cointype2)
				{
					case COINTYPE_PP:
						to_bucket = (int32 *) &trade->pp; break;
					case COINTYPE_GP:
						to_bucket = (int32 *) &trade->gp; break;
					case COINTYPE_SP:
						to_bucket = (int32 *) &trade->sp; break;
					case COINTYPE_CP:
						to_bucket = (int32 *) &trade->cp; break;
				}
			break;
		}
	}

	if(!from_bucket)
	{
		return;
	}

	// don't allow them to go into negatives (from our point of view)
	amount_to_take = *from_bucket < mc->amount ? *from_bucket : mc->amount;

	// if you move 11 gold into a bank platinum location, the packet
	// will say 11, but the client will have 1 left on their cursor, so we have
	// to figure out the conversion ourselves
	amount_to_add = amount_to_take;
	amount_to_add *= CoinTypeCoppers(mc->cointype1);
	amount_to_add /= CoinTypeCoppers(mc->cointype2);

	// the amount we're adding could be different than what was requested, so
	// we have to adjust the amount we take as well
	amount_to_take = amount_to_add;
	amount_to_take *= CoinTypeCoppers(mc->cointype2);
	amount_to_take /= CoinTypeCoppers(mc->cointype1);
	// now we should have a from_bucket, a to_bucket, an amount_to_take
	// and an amount_to_add

	if (amount_to_take > *from_bucket) {
		// we have a chance to go negative
		// something went wrong
		FinishTrade(this);
		trade->Reset();
		auto canceltrade = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
		CancelTrade_Struct* ct = (CancelTrade_Struct*)canceltrade->pBuffer;
		ct->fromid = 0;
		ct->action = 1;
		FastQueuePacket(&canceltrade);
		Kick();
		return;
	}
	if (to_bucket)
	{
		uint64 new_total = *to_bucket + amount_to_add;
		if (new_total > INT_MAX) {
			// overflow - dont want this to happen either
			FinishTrade(this);
			trade->Reset();
			auto canceltrade = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
			CancelTrade_Struct* ct = (CancelTrade_Struct*)canceltrade->pBuffer;
			ct->fromid = 0;
			ct->action = 1;
			FastQueuePacket(&canceltrade);
			Kick();
			return;
		}
	}
	// now we actually take it from the from bucket. if there's an error
	// with the destination slot, they lose their money
	*from_bucket -= amount_to_take;
	// why are intentionally inducing a crash here rather than letting the code attempt to stumble on?
	// assert(*from_bucket >= 0);

	if(to_bucket)
	{
		if(*to_bucket + amount_to_add > *to_bucket)	// overflow check
			*to_bucket += amount_to_add;
	}

	if(mc->to_slot == 3 && !trader) {
		// if we got here, then we have an issue with the trade
		FinishTrade(this);
		trade->Reset();
		auto canceltrade = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
		CancelTrade_Struct* ct = (CancelTrade_Struct*) canceltrade->pBuffer;
		ct->fromid = 0;
		ct->action = 1;
		FastQueuePacket(&canceltrade);
	}

	// if this is a trade move, inform the person being traded with
	if(mc->to_slot == 3 && trader && trader->IsClient())
	{

		// If one party accepted the trade then some coin was added, their state needs to be reset
		trade->state = Trading;
		Mob* with = trade->With();
		if (with)
			with->trade->state = Trading;

		Client* recipient = trader->CastToClient();
		recipient->Message(CC_Yellow, "%s adds some coins to the trade.", GetName());
		recipient->Message(CC_Yellow, "The total trade is: %i PP, %i GP, %i SP, %i CP",
			trade->pp, trade->gp,
			trade->sp, trade->cp
		);

		auto outapp = new EQApplicationPacket(OP_TradeCoins, sizeof(TradeCoin_Struct));
		TradeCoin_Struct* tcs = (TradeCoin_Struct*)outapp->pBuffer;
		tcs->trader = trader->GetID();
		tcs->slot = mc->cointype2;
		tcs->amount = amount_to_add;
		recipient->QueuePacket(outapp);
		safe_delete(outapp);
	}

	if (RuleB(QueryServ, PlayerLogMoneyTransactions))
	{
		if (mc->to_slot == -1 || mc->to_slot == 3 || 
			(RuleB(QueryServ, PlayerLogBankTransactions) && (mc->to_slot == 2 || mc->from_slot == 2)))
		{
			uint32 ToCharID = 0;
			uint32 ToNPC = 0;
			uint32 amount = amount_to_take;
			int32 slot = mc->to_slot;
			if (mc->to_slot == 2 || mc->from_slot == 2)
			{
				amount = mc->amount;
				if (mc->from_slot == 2)
				{
					slot = 99;
				}
			}
			else if (mc->to_slot == 3)
			{
				amount = amount_to_add;
				if (trader)
				{
					if (trader->IsClient())
					{
						ToCharID = trader->CastToClient()->CharacterID();
					}
					else if (trader->IsNPC())
					{
						ToNPC = trader->GetNPCTypeID();
					}
				}
			}
			QServ->QSCoinMove(CharacterID(), ToCharID, ToNPC, slot, amount, mc->cointype2);
		}
	}

	SaveCurrency();
	RecalcWeight();
}

void Client::OPGMTraining(const EQApplicationPacket *app)
{

	EQApplicationPacket* outapp = app->Copy();

	GMTrainee_Struct* gmtrain = (GMTrainee_Struct*) outapp->pBuffer;

	Mob* pTrainer = entity_list.GetMob(gmtrain->npcid);

	if(!pTrainer || !pTrainer->IsNPC() || pTrainer->GetClass() < WARRIORGM) {
		safe_delete(outapp);
		return;
	}

	//you can only use your own trainer, client enforces this, but why trust it
	int trains_class = pTrainer->GetClass() - (WARRIORGM - WARRIOR);
	if(GetClass() != trains_class) {
		safe_delete(outapp);
		return;
	}

	//you have to be somewhat close to a trainer to be properly using them
	if(DistanceSquared(m_Position,pTrainer->GetPosition()) > USE_NPC_RANGE2) {
		safe_delete(outapp);
		return;
	}

	int primaryfaction = pTrainer->CastToNPC()->GetPrimaryFaction();
	int factionlvl = GetFactionLevel(CharacterID(), GetRace(), GetClass(), GetDeity(), primaryfaction, pTrainer);
	if (factionlvl >= FACTION_APPREHENSIVELY)
	{
		gmtrain->success = 0;
		pTrainer->Say_StringID(0, WONT_SELL_RACE5, itoa(GetRaceStringID()));

		FastQueuePacket(&outapp);
		return;
	}

	EQ::skills::SkillType sk;
	for (sk = EQ::skills::Skill1HBlunt; sk <= EQ::skills::HIGHEST_SKILL; sk = (EQ::skills::SkillType)(sk+1))
	{
		//Only Gnomes can tinker.
		if(sk == EQ::skills::SkillTinkering && GetBaseRace() != GNOME)
			gmtrain->skills[sk] = 0;
		else 
			gmtrain->skills[sk] = GetMaxSkillAfterSpecializationRules((EQ::skills::SkillType)sk, MaxSkill((EQ::skills::SkillType)sk, GetClass(), RuleI(Character, MaxLevel)));

	}
	Mob* trainer = entity_list.GetMob(gmtrain->npcid);
	gmtrain->greed = CalcPriceMod(trainer);
	gmtrain->success = 1;
	for(int l = 0; l < 32; l++) {
		gmtrain->language[l] = 201;
	}

	FastQueuePacket(&outapp);
	// welcome message
	if (pTrainer && pTrainer->IsNPC())
	{
		pTrainer->Say_StringID(zone->random.Int(1204, 1207), GetCleanName());
	}
}

void Client::OPGMEndTraining(const EQApplicationPacket *app)
{
	GMTrainEnd_Struct *p = (GMTrainEnd_Struct *)app->pBuffer;
	Mob* pTrainer = entity_list.GetMob(p->npcid);

	if(!pTrainer || !pTrainer->IsNPC() || pTrainer->GetClass() < WARRIORGM)
		return;

	//you can only use your own trainer, client enforces this, but why trust it
	int trains_class = pTrainer->GetClass() - (WARRIORGM - WARRIOR);
	if(GetClass() != trains_class)
		return;

	//you have to be somewhat close to a trainer to be properly using them
	if(DistanceSquared(m_Position, pTrainer->GetPosition()) > USE_NPC_RANGE2)
		return;

	int primaryfaction = pTrainer->CastToNPC()->GetPrimaryFaction();
	int factionlvl = GetFactionLevel(CharacterID(), GetRace(), GetClass(), GetDeity(), primaryfaction, pTrainer);
	if (factionlvl >= FACTION_APPREHENSIVELY)
		return;

	// goodbye message
	if (pTrainer->IsNPC())
	{
		pTrainer->Say_StringID(zone->random.Int(1208, 1211), GetCleanName());
	}
}

void Client::OPGMTrainSkill(const EQApplicationPacket *app)
{
	if(!m_pp.points)
		return;

	int Cost = 0;

	GMSkillChange_Struct* gmskill = (GMSkillChange_Struct*) app->pBuffer;

	Mob* pTrainer = entity_list.GetMob(gmskill->npcid);
	if(!pTrainer || !pTrainer->IsNPC() || pTrainer->GetClass() < WARRIORGM)
		return;

	//you can only use your own trainer, client enforces this, but why trust it
	int trains_class = pTrainer->GetClass() - (WARRIORGM - WARRIOR);
	if(GetClass() != trains_class)
		return;

	//you have to be somewhat close to a trainer to be properly using them
	if(DistanceSquared(m_Position, pTrainer->GetPosition()) > USE_NPC_RANGE2)
		return;

	if (gmskill->skillbank == 0x01)
	{
		// languages go here
		if (gmskill->skill_id > 25)
		{
			Log(Logs::Detail, Logs::Combat, "Wrong Training Skill (languages)");
			DumpPacket(app);
			return;
		}
		int AdjustedSkillLevel = GetLanguageSkill(gmskill->skill_id) - 10;
		if(AdjustedSkillLevel > 0)
			Cost = (int)((double)(AdjustedSkillLevel * AdjustedSkillLevel * AdjustedSkillLevel) * CalcPriceMod(pTrainer) * 0.0099999998);

		IncreaseLanguageSkill(gmskill->skill_id);
	}
	else if (gmskill->skillbank == 0x00)
	{
		// normal skills go here
		if (gmskill->skill_id > EQ::skills::HIGHEST_SKILL)
		{
			Log(Logs::Detail, Logs::Combat, "Wrong Training Skill (abilities)" );
			DumpPacket(app);
			return;
		}

		EQ::skills::SkillType skill = (EQ::skills::SkillType) gmskill->skill_id;

		if(!CanHaveSkill(skill)) {
			Log(Logs::Detail, Logs::Skills, "Tried to train skill %d, which is not allowed.", skill);
			return;
		}

		if(MaxSkill(skill) == 0) {
			Log(Logs::Detail, Logs::Skills, "Tried to train skill %d, but training is not allowed at this level.", skill);
			return;
		}

		uint16 skilllevel = GetRawSkill(skill);
		if(skilllevel == 0 || (skilllevel == 254 && SkillTrainLevel(skill, GetClass()) <= GetLevel())) {
			//this is a new skill..
			uint16 t_level = SkillTrainLevel(skill, GetClass());
			if (t_level == 0)
			{
				Log(Logs::Detail, Logs::Combat, "Tried to train a new skill %d which is invalid for this race/class.", skill);
				return;
			}
			// solar: the client code uses the level required as the initial skill level of a newly acquired skill.  it is believed
			// that the initial level of a skill should be the player's current level instead, but this puts the client window out
			// of sync with the real value.
			{
				// TODO: this check for level 1 is a workaround for not being able to differentiate between a value 0 skill and an untrained (254) skill
				t_level = t_level == 1 ? 1 : std::min((uint16)GetLevel(), MaxSkill(skill));
			}
			SetSkill(skill, t_level, true);
		} else {
			switch(skill) {
			case EQ::skills::SkillBrewing:
			case EQ::skills::SkillTinkering:
			case EQ::skills::SkillAlchemy:
			case EQ::skills::SkillBaking:
			case EQ::skills::SkillTailoring:
			case EQ::skills::SkillBlacksmithing:
			case EQ::skills::SkillFletching:
			case EQ::skills::SkillJewelryMaking:
			case EQ::skills::SkillPottery:
				if(skilllevel >= RuleI(Skills, MaxTrainTradeskills)) {
					Message_StringID(CC_Red, MORE_SKILLED_THAN_I, pTrainer->GetCleanName());
					SetSkill(skill, skilllevel, true);
					return;
				}
				break;
			case EQ::skills::SkillSpecializeAbjure:
			case EQ::skills::SkillSpecializeAlteration:
			case EQ::skills::SkillSpecializeConjuration:
			case EQ::skills::SkillSpecializeDivination:
			case EQ::skills::SkillSpecializeEvocation:
				if(skilllevel >= RuleI(Skills, MaxTrainSpecializations)) {
					Message_StringID(CC_Red, MORE_SKILLED_THAN_I, pTrainer->GetCleanName());
					SetSkill(skill, skilllevel, true);
					return;
				}
			default:
				break;
			}

			int MaxSkillValue = MaxSkill(skill);
			if (skilllevel >= MaxSkillValue)
			{
				// Don't allow training over max skill level
				Message_StringID(CC_Red, MORE_SKILLED_THAN_I, pTrainer->GetCleanName());
				SetSkill(skill, skilllevel, true);
				return;
			}

			if(gmskill->skill_id >= EQ::skills::SkillSpecializeAbjure && gmskill->skill_id <= EQ::skills::SkillSpecializeEvocation)
			{
				int MaxSpecSkill = GetMaxSkillAfterSpecializationRules(skill, MaxSkillValue);
				if (skilllevel >= MaxSpecSkill)
				{
					// Restrict specialization training to follow the rules
					Message_StringID(CC_Red, MORE_SKILLED_THAN_I, pTrainer->GetCleanName());
					SetSkill(skill, skilllevel, true);
					return;
				}
			}

			// Client train a valid skill
			//
			int AdjustedSkillLevel = skilllevel - 10;

			if (AdjustedSkillLevel > 0)
				Cost = (int)((double)(AdjustedSkillLevel * AdjustedSkillLevel * AdjustedSkillLevel) * CalcPriceMod(pTrainer) * 0.0099999998);

			SetSkill(skill, skilllevel + 1, true);


		}
	}

	if(Cost)
		TakeMoneyFromPP(Cost);

	m_pp.points--;
}

// this is used for /summon and /corpse
void Client::OPGMSummon(const EQApplicationPacket *app)
{
	GMSummon_Struct* gms = (GMSummon_Struct*) app->pBuffer;
	Mob* st = entity_list.GetMob(gms->charname);

	if(st && st->IsCorpse())
	{
		st->CastToCorpse()->Summon(this, false, true);
	}
	else
	{
		if(admin < 80)
		{
			return;
		}
		QServ->QSLogCommands(this, "/summon", gms->charname);
		if(st)
		{
			if(st->IsClient())
			{
				//If #hideme is on, prevent being summoned by a lower GM.
				if(st->CastToClient()->GetAnon() == 1 && st->CastToClient()->Admin() > this->Admin())
				{
					Message(CC_Red, "You cannot summon a GM with a higher status than you.");
					return;
				}
			}

			Message(CC_Default, "Local: Summoning %s to %f, %f, %f", gms->charname, (float)gms->x, (float)gms->y, (float)gms->z);
			if (st->IsClient() && (st->CastToClient()->GetAnon() != 1 || this->Admin() >= st->CastToClient()->Admin()))
				st->CastToClient()->MovePC(zone->GetZoneID(), (float)gms->x, (float)gms->y, (float)gms->z, this->GetHeading(), true);
			else
				st->GMMove(this->GetX(), this->GetY(), this->GetZ(),this->GetHeading());
		}
		else
		{
			uint8 tmp = gms->charname[strlen(gms->charname)-1];
			if (!worldserver.Connected())
			{
				Message(CC_Default, "Error: World server disconnected");
			}
			else if (tmp < '0' || tmp > '9') // dont send to world if it's not a player's name
			{
				auto pack = new ServerPacket(ServerOP_ZonePlayer, sizeof(ServerZonePlayer_Struct));
				ServerZonePlayer_Struct* szp = (ServerZonePlayer_Struct*) pack->pBuffer;
				strcpy(szp->adminname, this->GetName());
				szp->adminrank = this->Admin();
				strcpy(szp->name, gms->charname);
				strcpy(szp->zone, zone->GetShortName());
				szp->x_pos = (float)gms->x;
				szp->y_pos = (float)gms->y;
				szp->z_pos = (float)gms->z;
				szp->zoneguildid = zone->GetGuildID();
				szp->ignorerestrictions = 2;
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
			else {
				//all options have been exhausted
				//summon our target...
				if(GetTarget() && GetTarget()->IsCorpse()){
					GetTarget()->CastToCorpse()->Summon(this, false, true);
				}
			}
		}
	}
}

void Client::DoHPRegen() 
{
	if (GetHP() >= max_hp)
		return;

	SetHP(GetHP() + CalcHPRegen());
	CalcAGI();
	SendHPUpdate();
}

void Client::DoManaRegen() 
{
	if (GetMaxMana() == 0)
		return;

	if (GetMana() < GetMaxMana())
	{
		int32 old_mana = GetMana();
		SetMana(GetMana() + CalcManaRegen(true));
		if (GetMana() != old_mana)
			SendManaUpdatePacket();
	}
}

void Client::ProcessHungerThirst()
{
	// GM and BecomeNPC don't consume food/water
	if (GetGM() || IsBecomeNPC())
	{
		SetHunger(32000);
		SetThirst(32000);
		return;
	}

	// SE_Hunger effect from Song of Sustenance stops food/water consumption
	if (spellbonuses.FoodWater)
	{
		if (GetHunger() < 3500)
			SetHunger(3500);
		if (GetThirst() < 3500)
			SetThirst(3500);
	}

	// timer
	// 46000 ms base - 96600/103500/115000ms with AA
	// 92000 ms(double) for monk class - 193200/207000/230000ms with AA
	uint32 timer = GetClass() == MONK ? 92000 : 46000;
	uint32 metabolismLevel = GetAA(aaInnateMetabolism);
	if(metabolismLevel)
	{
		int mod = 100;
		switch (metabolismLevel)
		{
		case 1:
			mod = 110;
			break;
		case 2:
			mod = 125;
			break;
		case 3:
			mod = 150;
			break;
		}
		timer += timer * mod / 100;
	}
	if (stamina_timer.GetDuration() != timer || !stamina_timer.Enabled())
	{
		stamina_timer.SetDuration(timer, true);
	}

	// digest some food/water
	if (stamina_timer.Check())
	{
		// horse triples consumption of food and water
		int32 consumption = GetHorseId() ? 96 : 32;
		SetHunger(GetHunger() > consumption ? GetHunger() - consumption : 0);
		SetThirst(GetThirst() > consumption ? GetThirst() - consumption : 0);
		SendStaminaUpdate();
	}
}

void Client::ProcessFatigue()
{
	// being out of food and/or drink increases fatigue
	if (Famished())
		SetFatigue(GetFatigue() + 1);

	// running increases fatigue, animation is a weird name for velocity from the client position update packet
	if(animation && runmode)
		SetFatigue(GetFatigue() + 1);

	// swimming increases fatigue
	if(IsInWater())
		SetFatigue(GetFatigue() + 1);

	// recovery
	// the way this check is written, it will trigger if you're fully fatigued at 100 since 
	// it can't go any higher, giving some recovery even while continuing to exercise
	if (GetFatigue() > 0 && GetFatigue() == last_fatigue && !Famished())
		SetFatigue(GetFatigue() - 10);
	last_fatigue = GetFatigue();

	CalcBonuses(); // STR/AGI/DEX depend on fatigue
	SendStaminaUpdate();
}

void Client::AddWeaponAttackFatigue(const EQ::ItemInstance *weapon)
{
	/*
	Attacking with a weapon increases fatigue:

	Weight > 100     : +2
	Weight > 51-100  : +1

	In addition, there is 33% chance to gain +1 fatigue, 50% chance if weight > 25
	*/
	uint8 weight = 0;
	if (weapon && weapon->IsWeapon())
	{
		weight = weapon->GetItem()->Weight;
	}
	int fatigue_chance = weight > 25 ? 2 : 3;

	if (weight > 100)
		SetFatigue(GetFatigue() + 2);
	else if (weight > 50)
		SetFatigue(GetFatigue() + 1);

	if (zone->random.Roll(1.0f / fatigue_chance))
		SetFatigue(GetFatigue() + 1);

	CalcBonuses(); // STR/AGI/DEX depend on fatigue
	//SendStaminaUpdate();
}

void Client::SetFatigue(int8 in_fatigue)
{
	//int8 old_fatigue = m_pp.fatigue;
	m_pp.fatigue = in_fatigue > 0 ? (in_fatigue < 100 ? in_fatigue : 100) : 0;
	//Message(MT_Broadcasts, "SetFatigue %d -> %d = %d", old_fatigue, in_fatigue, m_pp.fatigue);
}
