/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2005 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/classes.h"
#include "../common/global_define.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/strings.h"

#include "entity.h"
#include "event_codes.h"
#include "guild_mgr.h"
#include "qglobals.h"
#include "queryserv.h"
#include "quest_parser_collection.h"
#include "questmgr.h"
#include "spawn2.h"
#include "string_ids.h"
#include "worldserver.h"
#include "zone.h"
#include "zonedb.h"

#include <iostream>
#include <limits.h>
#include <list>

extern QueryServ* QServ;
extern Zone* zone;
extern WorldServer worldserver;
extern EntityList entity_list;

QuestManager quest_manager;

#define QuestManagerCurrentQuestVars() \
	Mob *owner = nullptr; \
	Client *initiator = nullptr; \
	EQ::ItemInstance* questitem = nullptr; \
	bool depop_npc = false; \
	std::string encounter; \
	do { \
		if(!quests_running_.empty()) { \
			running_quest e = quests_running_.top(); \
			owner = e.owner; \
			initiator = e.initiator; \
			questitem = e.questitem; \
			depop_npc = e.depop_npc; \
			encounter = e.encounter; \
		} \
	} while(0)

QuestManager::QuestManager() {
	HaveProximitySays = false;
	item_timers = 0;
}

QuestManager::~QuestManager() {
}

void QuestManager::Process() {
	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end) {
		if (cur->Timer_.Enabled() && cur->Timer_.Check()) {
			if(cur->mob) {
				if(cur->mob->IsNPC()) {
					parse->EventNPC(EVENT_TIMER, cur->mob->CastToNPC(), nullptr, cur->name, 0);

				} 
				else if (cur->mob->IsEncounter()) {
					parse->EventEncounter(EVENT_TIMER, cur->mob->CastToEncounter()->GetEncounterName(), cur->name, 0, nullptr);
				} 
				else if (cur->mob->IsClient()) {
						//this is inheriently unsafe if we ever make it so more than npc/client start timers
						parse->EventPlayer(EVENT_TIMER, cur->mob->CastToClient(), cur->name, 0);
				}

				//we MUST reset our iterator since the quest could have removed/added any
				//number of timers... worst case we have to check a bunch of timers twice
				cur = QTimerList.begin();
				end = QTimerList.end();	//dunno if this is needed, cant hurt...
			} else {
				cur = QTimerList.erase(cur);
			}
		} else
			++cur;
	}

	auto cur_iter = STimerList.begin();
	while(cur_iter != STimerList.end()) {
		if(!cur_iter->Timer_.Enabled()) {
			cur_iter = STimerList.erase(cur_iter);
		} else if(cur_iter->Timer_.Check()) {
			entity_list.SignalMobsByNPCID(cur_iter->npc_id, cur_iter->signal_id, &cur_iter->data[0]);
			cur_iter = STimerList.erase(cur_iter);
		} else {
			++cur_iter;
		}
	}
}

void QuestManager::StartQuest(Mob *_owner, Client *_initiator, EQ::ItemInstance* _questitem, std::string encounter) {
	running_quest run;
	run.owner = _owner;
	run.initiator = _initiator;
	run.questitem = _questitem;
	run.depop_npc = false;
	run.encounter = encounter;
	quests_running_.push(run);
}

void QuestManager::EndQuest() {
	running_quest run = quests_running_.top();
	if(run.depop_npc && run.owner->IsNPC()) {
		//clear out any timers for them...
		std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

		end = QTimerList.end();
		while (cur != end) {
			if (cur->mob == run.owner)
				cur = QTimerList.erase(cur);
			else
				++cur;
		}
		std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend;

		pend = PTimerList.end();
		while (pcur != pend) {
			if (pcur->owner == run.owner)
				pcur = PTimerList.erase(pcur);
			else
				++pcur;
		}
		run.owner->Depop();
	}
	quests_running_.pop();
}

void QuestManager::ClearAllTimers() {
	QTimerList.clear();
	PTimerList.clear();
}

//quest perl functions
void QuestManager::echo(int colour, const char *str) {
	QuestManagerCurrentQuestVars();
	entity_list.MessageClose(initiator, false, 200, colour, str);
}

void QuestManager::say(const char *str) {
	QuestManagerCurrentQuestVars();
	if (!owner) {
		Log(Logs::General, Logs::Quests, "QuestManager::say called with nullptr owner. Probably syntax error in quest file.");
		return;
	}
	else {
		owner->Say(str);
	}
}

void QuestManager::say(const char *str, uint8 language) {
	QuestManagerCurrentQuestVars();
	if (!owner) {
		Log(Logs::General, Logs::Quests, "QuestManager::say called with nullptr owner. Probably syntax error in quest file.");
		return;
	}
	else {
		entity_list.ChannelMessage(owner, 8, language, str);
	}
}

void QuestManager::me(const char *str) {
	QuestManagerCurrentQuestVars();
	if (!initiator)
		return;
	entity_list.MessageClose(initiator, false, 200, 10, str);
}

void QuestManager::summonitem(uint32 itemid, int16 charges) {
	QuestManagerCurrentQuestVars();
	if(!initiator)
		return;
	initiator->SummonItem(itemid, charges);
}

void QuestManager::write(const char *file, const char *str) {
	FILE * pFile;
	pFile = fopen (fmt::format("{}/{}", path.GetServerPath(), file).c_str(), "a");
	if(!pFile)
		return;
	fprintf(pFile, "%s\n", str);
	fclose (pFile);
}

Mob* QuestManager::spawn2(int npc_type, int grid, int unused, const glm::vec4& position, const char* name) {
	NPCType* tmp = (NPCType*)database.LoadNPCTypesData(npc_type);
	char tmp_name[64];
	if (!tmp)
		return nullptr;

	if (name)
	{
		// NPC types are cached; need to save old name so we can revert it after using new name
		strcpy(tmp_name, tmp->name);
		strcpy(tmp->name, name);
	}

	auto npc = new NPC(tmp, nullptr, position, GravityBehavior::Water);
	if (name)
		strcpy(tmp->name, tmp_name);

	npc->AddLootTable();
	if (npc->DropsGlobalLoot()) {
		npc->CheckGlobalLootTables();
	}
	entity_list.AddNPC(npc,true,true);
	if(grid > 0)
	{
		npc->AssignWaypoints(grid);
	}
	npc->SendPosUpdate();
	return npc;
}

Mob* QuestManager::unique_spawn(int npc_type, int grid, int unused, const glm::vec4& position) {
	Mob *other = entity_list.GetMobByNpcTypeID(npc_type);
	if(other != nullptr) {
		return other;
	}

	const NPCType* tmp = 0;
	if (tmp = database.LoadNPCTypesData(npc_type))
	{
		auto npc = new NPC(tmp, nullptr, position, GravityBehavior::Water);
		npc->AddLootTable();
		if (npc->DropsGlobalLoot()) {
			npc->CheckGlobalLootTables();
		}
		entity_list.AddNPC(npc,true,true);
		if(grid > 0)
		{
			npc->AssignWaypoints(grid);
		}
		return npc;
	}
	return nullptr;
}

Mob* QuestManager::spawn_from_spawn2(uint32 spawn2_id)
{
	LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
	iterator.Reset();
	Spawn2 *found_spawn = nullptr;

	while(iterator.MoreElements())
	{
		Spawn2* cur = iterator.GetData();
		iterator.Advance();
		if(cur->GetID() == spawn2_id)
		{
			found_spawn = cur;
			break;
		}
	}

	if(found_spawn)
	{
		SpawnGroup* sg = zone->spawn_group_list.GetSpawnGroup(found_spawn->SpawnGroupID());
		if(!sg)
		{
			database.LoadSpawnGroupsByID(found_spawn->SpawnGroupID(),&zone->spawn_group_list);
			sg = zone->spawn_group_list.GetSpawnGroup(found_spawn->SpawnGroupID());
			if(!sg)
			{
				return nullptr;
			}
		}
		uint32 rtime = 0;
		uint32 npcid = sg->GetNPCType(rtime);
		if(npcid == 0)
		{
			return nullptr;
		}

		const NPCType* tmp = database.LoadNPCTypesData(npcid);
		if(!tmp)
		{
			return nullptr;
		}

		if(tmp->unique_spawn_by_name)
		{
			if(!entity_list.LimitCheckName(tmp->name))
			{
				return nullptr;
			}
		}

		if(tmp->spawn_limit > 0)
		{
			if(!entity_list.LimitCheckType(npcid, tmp->spawn_limit))
			{
				return nullptr;
			}
		}

		database.UpdateRespawnTime(spawn2_id, 0, zone ? zone->GetGuildID() : GUILD_NONE);
		found_spawn->SetCurrentNPCID(npcid);

        auto position = glm::vec4(found_spawn->GetX(), found_spawn->GetY(), found_spawn->GetZ(), found_spawn->GetHeading());

		position.w = FixHeading(position.w);
	auto npc = new NPC(tmp, found_spawn, position, GravityBehavior::Water);

	found_spawn->SetNPCPointer(npc);
	npc->AddLootTable();
	if (npc->DropsGlobalLoot()) {
		npc->CheckGlobalLootTables();
	}
	npc->SetSp2(found_spawn->SpawnGroupID());
	entity_list.AddNPC(npc);
	entity_list.LimitAddNPC(npc);

	if (sg->roamdist && sg->roambox[0] && sg->roambox[1] && sg->roambox[2] && sg->roambox[3] && sg->delay &&
	    sg->min_delay)
		npc->AI_SetRoambox(sg->roambox[0], sg->roambox[1], sg->roambox[2], sg->roambox[3],
				   sg->delay, sg->min_delay);

	found_spawn->LoadGrid();

		return npc;
	}

	return nullptr;
}

void QuestManager::enable_spawn2(uint32 spawn2_id)
{
	database.UpdateSpawn2Status(spawn2_id, 1);
	auto pack = new ServerPacket(ServerOP_SpawnStatusChange, sizeof(ServerSpawnStatusChange_Struct));
	ServerSpawnStatusChange_Struct* ssc = (ServerSpawnStatusChange_Struct*) pack->pBuffer;
	ssc->id = spawn2_id;
	ssc->new_status = 1;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::disable_spawn2(uint32 spawn2_id)
{
	database.UpdateSpawn2Status(spawn2_id, 0);
	auto pack = new ServerPacket(ServerOP_SpawnStatusChange, sizeof(ServerSpawnStatusChange_Struct));
	ServerSpawnStatusChange_Struct* ssc = (ServerSpawnStatusChange_Struct*) pack->pBuffer;
	ssc->id = spawn2_id;
	ssc->new_status = 0;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::castspell(int spell_id, int target_id) {
	QuestManagerCurrentQuestVars();
	if (owner) {
		Mob *tgt = entity_list.GetMob(target_id);
		if(tgt != nullptr)
			owner->SpellFinished(spell_id, tgt, EQ::spells::CastingSlot::Item, 0, -1, spells[spell_id].ResistDiff);
	}
}

void QuestManager::selfcast(int spell_id) {
	QuestManagerCurrentQuestVars();
	if (initiator)
		initiator->SpellFinished(spell_id, initiator, EQ::spells::CastingSlot::Item, 0, -1, spells[spell_id].ResistDiff);
}

void QuestManager::addloot(int item_id, int charges, bool equipitem) {
	QuestManagerCurrentQuestVars();
	if(item_id != 0){
		if(owner->IsNPC())
			owner->CastToNPC()->AddItem(item_id, charges, equipitem);
	}
}

void QuestManager::Zone(const char *zone_name) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
	{
		auto pack = new ServerPacket(ServerOP_ZoneToZoneRequest, sizeof(ZoneToZone_Struct));
		ZoneToZone_Struct* ztz = (ZoneToZone_Struct*) pack->pBuffer;
		ztz->response = 0;
		ztz->current_zone_id = zone->GetZoneID();
		ztz->current_zone_guild_id = zone->GetGuildID();
		ztz->requested_zone_id = database.GetZoneID(zone_name);
		ztz->requested_zone_guild_id = GUILD_NONE;
		ztz->admin = initiator->Admin();
		strcpy(ztz->name, initiator->GetName());
		ztz->guild_id = initiator->GuildID();
		ztz->ignorerestrictions = 3;
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void QuestManager::settimer(const char *timer_name, int seconds) {
	QuestManagerCurrentQuestVars();

	if(questitem) {
		questitem->SetTimer(timer_name, seconds * 1000);
		return;
	}

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end) {
		if(cur->mob && cur->mob == owner && cur->name == timer_name)
		{
			cur->Timer_.Enable();
			cur->Timer_.Start(seconds * 1000, false);
			return;
		}
		++cur;
	}

	QTimerList.push_back(QuestTimer(seconds * 1000, owner, timer_name));
}

void QuestManager::settimerMS(const char *timer_name, int milliseconds) {
	QuestManagerCurrentQuestVars();

	if(questitem) {
		questitem->SetTimer(timer_name, milliseconds);
		return;
	}

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end) {
		if(cur->mob && cur->mob == owner && cur->name == timer_name)
		{
			cur->Timer_.Enable();
			cur->Timer_.Start(milliseconds, true);
			return;
		}
		++cur;
	}

	QTimerList.push_back(QuestTimer(milliseconds, owner, timer_name));
}

void QuestManager::settimerMS(const char *timer_name, int milliseconds, EQ::ItemInstance *inst) {
	if (inst) {
		inst->SetTimer(timer_name, milliseconds);
	}
}

void QuestManager::settimerMS(const char *timer_name, int milliseconds, Mob *mob) {
	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end) {
		if (cur->mob && cur->mob == mob && cur->name == timer_name)
		{
			cur->Timer_.Enable();
			cur->Timer_.Start(milliseconds, false);
			return;
		}
		++cur;
	}

	QTimerList.push_back(QuestTimer(milliseconds, mob, timer_name));
}

void QuestManager::stoptimer(const char *timer_name) {
	QuestManagerCurrentQuestVars();

	if (questitem) {
		questitem->StopTimer(timer_name);
		return;
	}

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end) {
		if (cur->mob && cur->mob == owner && cur->name == timer_name) {
			QTimerList.erase(cur);
			return;
		}
		++cur;
	}

	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend;
	pend = PTimerList.end();
	while (pcur != pend) {
		if (pcur->owner && pcur->owner == owner && pcur->name == timer_name) {
			PTimerList.erase(pcur);
			return;
		}
		++pcur;
	}
}

void QuestManager::stoptimer(const char *timer_name, EQ::ItemInstance *inst) {
	if (inst) {
		inst->StopTimer(timer_name);
	}
}

void QuestManager::stoptimer(const char *timer_name, Mob *mob) {
	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end) {
		if (cur->mob && cur->mob == mob && cur->name == timer_name) {
			QTimerList.erase(cur);
			return;
		}
		++cur;
	}

	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend;
	pend = PTimerList.end();
	while (pcur != pend) {
		if (pcur->owner && pcur->owner == mob && pcur->name == timer_name) {
			PTimerList.erase(pcur);
			return;
		}
		++pcur;
	}
}

void QuestManager::stopalltimers() {
	QuestManagerCurrentQuestVars();

	if(questitem) {
		questitem->ClearTimers();
		return;
	}

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end, tmp;

	end = QTimerList.end();
	while (cur != end) {
		if(cur->mob && cur->mob == owner)
			cur = QTimerList.erase(cur);
		else
			++cur;
	}

	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend, ptmp;

	pend = PTimerList.end();
	while (pcur != pend) {
		if (pcur->owner && pcur->owner == owner)
			pcur = PTimerList.erase(pcur);
		else
			++pcur;
	}
}

void QuestManager::stopalltimers(EQ::ItemInstance *inst) {
	if (inst) {
		inst->ClearTimers();
	}
}

void QuestManager::stopalltimers(Mob *mob) {
	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end, tmp;

	end = QTimerList.end();
	while (cur != end) {
		if (cur->mob && cur->mob == mob)
			cur = QTimerList.erase(cur);
		else
			++cur;
	}

	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend, ptmp;

	pend = PTimerList.end();
	while (pcur != pend) {
		if (pcur->owner && pcur->owner == mob)
			pcur = PTimerList.erase(pcur);
		else
			++pcur;
	}
}

void QuestManager::pausetimer(const char *timer_name) {
	QuestManagerCurrentQuestVars();

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;
	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend;
	PausedTimer pt;
	uint32 milliseconds = 0;
	uint32 duration = 0;

	pend = PTimerList.end();
	while (pcur != pend) 
	{
		if (pcur->owner && pcur->owner == owner && pcur->name == timer_name) 
		{
			Log(Logs::General, Logs::Quests, "Timer %s is already paused for %s. Returning...", timer_name, owner->GetName());
			return;
		}
		++pcur;
	}

	end = QTimerList.end();
	while (cur != end) 
	{
		if (cur->mob && cur->mob == owner && cur->name == timer_name) 
		{
			milliseconds = cur->Timer_.GetRemainingTime();
			duration = cur->Timer_.GetDuration();
			QTimerList.erase(cur);
			break;
		}
		++cur;
	}

	std::string timername = timer_name;
	pt.name = timername;
	pt.owner = owner;
	pt.time = milliseconds;
	pt.duration = duration;
	Log(Logs::General, Logs::Quests, "Pausing timer %s for %s with %d ms remaining.", timer_name, owner->GetName(), milliseconds);
	PTimerList.push_back(pt);
}

void QuestManager::resumetimer(const char *timer_name) {
	QuestManagerCurrentQuestVars();

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;
	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend;
	PausedTimer pt;
	uint32 milliseconds = 0;
	uint32 duration = 0;

	pend = PTimerList.end();
	while (pcur != pend) 
	{
		if (pcur->owner && pcur->owner == owner && pcur->name == timer_name) 
		{
			milliseconds = pcur->time;
			duration = pcur->duration;
			PTimerList.erase(pcur);
			break;
		}
		++pcur;
	}

	if(milliseconds == 0)
	{
		Log(Logs::General, Logs::Quests, "Paused timer %s not found or has expired. Returning...", timer_name);
		return;
	}

	end = QTimerList.end();
	while (cur != end) 
	{
		if(cur->mob && cur->mob == owner && cur->name == timer_name)
		{
			cur->Timer_.Start(milliseconds, false);
			Log(Logs::General, Logs::Quests, "Resuming timer %s for %s with %d ms remaining.", timer_name, owner->GetName(), milliseconds);
			return;
		}
		++cur;
	}

	QTimerList.push_back(QuestTimer(duration, owner, timer_name, milliseconds));
	Log(Logs::General, Logs::Quests, "Creating a new timer and resuming %s for %s with %d ms remaining.", timer_name, owner->GetName(), milliseconds);
	
}

bool QuestManager::ispausedtimer(const char *timer_name) {
	QuestManagerCurrentQuestVars();

	std::list<PausedTimer>::iterator pcur = PTimerList.begin(), pend;

	pend = PTimerList.end();
	while (pcur != pend) 
	{
		if (pcur->owner && pcur->owner == owner && pcur->name == timer_name) 
		{
			return true;
		}
		++pcur;
	}

	return false;
}

int QuestManager::gettimer(const char *timer_name) {
	QuestManagerCurrentQuestVars();

	std::list<QuestTimer>::iterator cur = QTimerList.begin(), end;

	end = QTimerList.end();
	while (cur != end)
	{
		if (cur->mob && cur->mob == owner && cur->name == timer_name)
		{
			return cur->Timer_.GetRemainingTime();
		}
		++cur;
	}

	return 0;
}

void QuestManager::emote(const char *str) {
	QuestManagerCurrentQuestVars();
	if (!owner) {
		Log(Logs::General, Logs::Quests, "QuestManager::emote called with nullptr owner. Probably syntax error in quest file.");
		return;
	}
	else {
		owner->Emote(str);
	}
}

void QuestManager::shout(const char *str) {
	QuestManagerCurrentQuestVars();
	if (!owner) {
		Log(Logs::General, Logs::Quests, "QuestManager::shout called with nullptr owner. Probably syntax error in quest file.");
		return;
	}
	else {
		owner->Shout(str);
	}
}

void QuestManager::shout2(const char *str) {
	QuestManagerCurrentQuestVars();
	if (!owner) {
		Log(Logs::General, Logs::Quests, "QuestManager::shout2 called with nullptr owner. Probably syntax error in quest file.");
		return;
	}
	else {
		worldserver.SendEmoteMessage(0, 0, AccountStatus::Player, Chat::Red, fmt::format(" {} shouts, '{}'", owner->GetCleanName(), str).c_str());
	}
}

void QuestManager::gmsay(const char *str, uint32 color, bool send_to_world, uint32 to_guilddbid, uint32 to_minstatus) {
	QuestManagerCurrentQuestVars();
	if(send_to_world)
		worldserver.SendEmoteMessage(0, to_guilddbid, to_minstatus, color, "%s", str);
	else
		entity_list.MessageStatus(to_guilddbid, to_minstatus, color, "%s", str);
}

void QuestManager::depop(int npc_type) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC()) {
		Log(Logs::General, Logs::Quests, "QuestManager::depop called with nullptr owner or non-NPC owner. Probably syntax error in quest file.");
		return;
	}
	else {
		if (npc_type != 0) {
			Mob * tmp = entity_list.GetMobByNpcTypeID(npc_type);
			if (tmp) {
				if (tmp != owner) {
					tmp->CastToNPC()->Depop();
				}
				else {
					running_quest e = quests_running_.top();
					e.depop_npc = true;
					quests_running_.pop();
					quests_running_.push(e);
				}
			}
		}
		else {	//depop self
			running_quest e = quests_running_.top();
			e.depop_npc = true;
			quests_running_.pop();
			quests_running_.push(e);
		}
	}
}

void QuestManager::depop_withtimer(int npc_type) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC()) {
		Log(Logs::General, Logs::Quests, "QuestManager::depop_withtimer called with nullptr owner or non-NPC owner. Probably syntax error in quest file.");
		return;
	}
	else {
		if (npc_type != 0) {
			Mob * tmp = entity_list.GetMobByNpcTypeID(npc_type);
			if (tmp) {
				if (tmp != owner) {
					tmp->CastToNPC()->Depop(true);
				}
				else {
					owner->Depop(true);
				}
			}
		}
		else {	//depop self
			owner->Depop(true);
		}
	}
}

void QuestManager::depopall(int npc_type) {
	if(npc_type) {
		entity_list.DepopAll(npc_type);
	}
	else {
		Log(Logs::General, Logs::Quests, "QuestManager::depopall called with nullptr owner, non-NPC owner, or invalid NPC Type ID. Probably syntax error in quest file.");
	}
}

void QuestManager::depopzone(bool StartSpawnTimer) {
	if(zone) {
		zone->Depop(StartSpawnTimer);
	}
	else {
		Log(Logs::General, Logs::Quests, "QuestManager::depopzone called with nullptr zone. Probably syntax error in quest file.");
	}
}

void QuestManager::repopzone() {
	if(zone) {
		zone->Repop();
	}
	else {
		Log(Logs::General, Logs::Quests, "QuestManager::repopzone called with nullptr zone. Probably syntax error in quest file.");
	}
}

void QuestManager::processmobswhilezoneempty(bool on) {
	if (zone) {
		zone->process_mobs_while_empty = on;
	}
	else {
		LogQuests("QuestManager::processmobswhilezoneempty called with nullptr zone. Probably syntax error in quest file");
	}
}

void QuestManager::settarget(const char *type, int target_id) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	Mob* tmp = nullptr;
	if (!strcasecmp(type,"npctype"))
		tmp = entity_list.GetMobByNpcTypeID(target_id);
	else if (!strcasecmp(type, "entity"))
		tmp = entity_list.GetMob(target_id);

	if (tmp != nullptr)
		owner->SetTarget(tmp);
}

void QuestManager::follow(int entity_id, int distance) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	owner->SetFollowID(entity_id);
	owner->SetFollowDistance(distance * distance);
}

void QuestManager::sfollow() {
	QuestManagerCurrentQuestVars();
	if (owner == nullptr || !owner->IsNPC())
		return;
	owner->SetFollowID(0);
}

void QuestManager::changedeity(int deity_id) {
	QuestManagerCurrentQuestVars();
	//Changes the deity.
	if(initiator)
	{
		if(initiator->IsClient())
		{
			initiator->SetDeity(deity_id);
			initiator->Message(Chat::Yellow,"Your Deity has been changed/set to: %i", deity_id);
			initiator->Save(1);
			initiator->Kick();
		}
		else
		{
			initiator->Message(Chat::Yellow,"Error changing Deity");
		}
	}
}

void QuestManager::exp(int amt) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->AddQuestEXP(amt);
}

void QuestManager::level(int newlevel) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->SetLevel(newlevel, true);
}

std::string QuestManager::getskillname(int skill_id) {
	return EQ::skills::GetSkillName(static_cast<EQ::skills::SkillType>(skill_id));
}

std::string QuestManager::getfactionname(int faction_id) {
	return database.GetFactionName(faction_id);
}

std::string QuestManager::getlanguagename(int language_id) {
	return EQ::constants::GetLanguageName(language_id);
}

std::string QuestManager::getbodytypename(uint32 bodytype_id) {
	return EQ::constants::GetBodyTypeName(static_cast<bodyType>(bodytype_id));
}

std::string QuestManager::getconsiderlevelname(uint8 consider_level) {
	return EQ::constants::GetConsiderLevelName(consider_level);
}

void QuestManager::safemove() {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->GoToSafeCoords(zone->GetZoneID(), zone->GetGuildID());
}

void QuestManager::rain(int weather) {
	QuestManagerCurrentQuestVars();
	zone->zone_weather = weather;
	auto outapp = new EQApplicationPacket(OP_Weather, 8);
	*((uint32*) &outapp->pBuffer[4]) = (uint32) weather; // Why not just use 0x01/2/3?
	entity_list.QueueClients(owner, outapp);
	safe_delete(outapp);
}

void QuestManager::snow(int weather) {
	QuestManagerCurrentQuestVars();
	zone->zone_weather = weather + 1;
	auto outapp = new EQApplicationPacket(OP_Weather, 8);
	outapp->pBuffer[0] = 0x01;
	*((uint32*) &outapp->pBuffer[4]) = (uint32)weather;
	entity_list.QueueClients(initiator, outapp);
	safe_delete(outapp);
}

void QuestManager::surname(const char *name) {
	QuestManagerCurrentQuestVars();
	//Changes the last name.
	if(initiator)
	{
		if(initiator->IsClient())
		{
			initiator->ChangeLastName(name);
			initiator->Message(Chat::Yellow,"Your surname has been changed/set to: %s", name);
		}
		else
		{
			initiator->Message(Chat::Yellow,"Error changing/setting surname");
		}
	}
}

void QuestManager::permaclass(int class_id) {
	QuestManagerCurrentQuestVars();
	//Makes the client the class specified
	initiator->SetBaseClass(class_id);
	initiator->Save(2);
	initiator->Kick();
}

void QuestManager::permarace(int race_id) {
	QuestManagerCurrentQuestVars();
	//Makes the client the race specified
	initiator->SetBaseRace(race_id);
	initiator->Save(2);
	initiator->Kick();
}

void QuestManager::permagender(int gender_id) {
	QuestManagerCurrentQuestVars();
	//Makes the client the gender specified
	initiator->SetBaseGender(gender_id);
	initiator->Save(2);
	initiator->Kick();
}

uint16 QuestManager::scribespells(uint8 max_level, uint8 min_level) {
	QuestManagerCurrentQuestVars();
	return initiator->ScribeSpells(min_level, max_level);
}

void QuestManager::unscribespells() {
	QuestManagerCurrentQuestVars();
	initiator->UnscribeSpellAll();
	}

void QuestManager::givecash(uint32 copper, uint32 silver, uint32 gold, uint32 platinum) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient() && (copper || silver || gold || platinum)) {
		initiator->AddMoneyToPP(copper, silver, gold, platinum, true);

		auto money_string = Strings::Money(platinum, gold, silver, copper);

		if (money_string == "Unknown") {
			return;
		}

		if (initiator) {
			initiator->Message_StringID(Chat::Green, YOU_RECEIVE_AS_SPLIT, Strings::Money(platinum, gold, silver, copper).c_str());
		}
	}
}

void QuestManager::pvp(const char *mode) {
	QuestManagerCurrentQuestVars();
	if (!strcasecmp(mode,"on"))
	{
		if (initiator)
			initiator->SetPVP(true);
	}
	else
		if (initiator)
			initiator->SetPVP(false);
}

void QuestManager::movepc(int zone_id, float x, float y, float z, float heading) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->MovePC(zone_id, x, y, z, heading);
}

void QuestManager::gmmove(float x, float y, float z) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->GMMove(x, y, z);
}

void QuestManager::movegrp(int zoneid, float x, float y, float z) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
	{
		Group *g = entity_list.GetGroupByClient(initiator);
		if (g != nullptr) {
			g->TeleportGroup(owner, zoneid, x, y, z, 0.0f);
		}
		else {
			Raid *r = entity_list.GetRaidByClient(initiator);
			if (r != nullptr) {
				uint32 gid = r->GetGroup(initiator);
				if (gid >= 0 && gid < MAX_RAID_GROUPS) {
					r->TeleportGroup(owner, zoneid, x, y, z, 0.0f, gid);
				}
				else {
					initiator->MovePC(zoneid, x, y, z, 0.0f);
				}
			}
			else {
				initiator->MovePC(zoneid, x, y, z, 0.0f);
			}
		}
	}
}

void QuestManager::doanim(int anim_id) {
	QuestManagerCurrentQuestVars();
	owner->DoAnim(static_cast<DoAnimation>(anim_id));
}

void QuestManager::addskill(int skill_id, int value) {
	QuestManagerCurrentQuestVars();
	if(skill_id < 0 || skill_id > EQ::skills::HIGHEST_SKILL)
		return;
	if (initiator && initiator->IsClient())
		initiator->AddSkill((EQ::skills::SkillType) skill_id, value);
}

void QuestManager::setlanguage(int skill_id, int value) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->SetLanguageSkill(skill_id, value);
}

void QuestManager::setskill(int skill_id, int value) {
	QuestManagerCurrentQuestVars();
	if(skill_id < 0 || skill_id > EQ::skills::HIGHEST_SKILL)
		return;
	if (initiator && initiator->IsClient())
		initiator->SetSkill((EQ::skills::SkillType) skill_id, value);
}

void QuestManager::setallskill(int value) {
	QuestManagerCurrentQuestVars();
	if (!initiator) {
		return;
	}

	if (initiator && initiator->IsClient()) {
		for (const auto& s : EQ::skills::GetSkillTypeMap()) {
			initiator->SetSkill(s.first, value);
		}
	}
}

void QuestManager::attack(const char *client_name) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	Client* getclient = entity_list.GetClientByName(client_name);
	if (getclient && owner->IsAttackAllowed(getclient))
		owner->AddToHateList(getclient,1);
	else
		owner->Say("I am unable to attack %s.", client_name);
}

void QuestManager::attacknpc(int npc_entity_id) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	Mob *it = entity_list.GetMob(npc_entity_id);
	if (it && owner->IsAttackAllowed(it)) {
		owner->AddToHateList(it,1);
	} else {
		if (it)
			owner->Say("I am unable to attack %s.", it->GetName());
		else
			owner->Say("I am unable to locate NPC entity %i", npc_entity_id);
	}
}

void QuestManager::attacknpctype(int npc_type_id) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	Mob *it = entity_list.GetMobByNpcTypeID(npc_type_id);
	if (it && owner->IsAttackAllowed(it)) {
		owner->AddToHateList(it,1);
	} else {
		if (it)
			owner->Say("I am unable to attack %s.", it->GetName());
		else
			owner->Say("I am unable to locate NPC type %i", npc_type_id);
	}
}

void QuestManager::save() {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient())
		initiator->Save();
}

void QuestManager::faction(int faction_id, int faction_value, int temp) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient()) {
		if(faction_id != 0 && faction_value != 0) {
			initiator->SetFactionLevel2(
				initiator->CharacterID(),
				faction_id,
				faction_value,
				temp);
		}
	}
}

void QuestManager::setsky(uint8 new_sky) {
	QuestManagerCurrentQuestVars();
	if (zone)
		zone->newzone_data.sky = new_sky;
	auto outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
	memcpy(outapp->pBuffer, &zone->newzone_data, outapp->size);
	entity_list.QueueClients(initiator, outapp);
	safe_delete(outapp);
}

void QuestManager::setguild(uint32 new_guild_id, uint8 new_rank) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient()) {
		guild_mgr.SetGuild(initiator->CharacterID(), new_guild_id, new_rank);
	}
}

void QuestManager::CreateGuild(const char *guild_name, const char *leader) {
	QuestManagerCurrentQuestVars();
	uint32 cid = database.GetCharacterID(leader);
	char hString[250];
			if (cid == 0) {
				worldserver.SendEmoteMessage(0, 0, 80, 15, "%s", "Guild Creation: Guild leader not found.");
				return;
			}

			uint32 tmp = guild_mgr.FindGuildByLeader(cid);
			if (tmp != GUILD_NONE) {
				sprintf(hString, "Guild Creation: Error: %s already is the leader of DB# %u '%s'.", leader, tmp, guild_mgr.GetGuildName(tmp));
				worldserver.SendEmoteMessage(0, 0, 80, 15, "%s", hString);
			}
			else {
				uint32 gid = guild_mgr.CreateGuild(guild_name, cid);
				if (gid == GUILD_NONE)
					worldserver.SendEmoteMessage(0, 0, 80, 15, "%s", "Guild Creation: Guild creation failed");
				else {
					sprintf(hString, "Guild Creation: Guild created: Leader: %u, number %u: %s", cid, gid, leader);
					worldserver.SendEmoteMessage(0, 0, 80, 15, "%s", hString);
					if(!guild_mgr.SetGuild(cid, gid, GUILD_LEADER))
						worldserver.SendEmoteMessage(0, 0, 80, 15, "%s", "Unable to set guild leader's guild in the database. Your going to have to run #guild set");
				}

			}
}

void QuestManager::settime(uint8 new_hour, uint8 new_min) {
	if (zone)
		zone->SetTime(new_hour, new_min);
}

void QuestManager::itemlink(int item_id) {
	QuestManagerCurrentQuestVars();
	if (initiator) {
		const EQ::ItemData *item = database.GetItem(item_id);
		if (item == nullptr)
			return;

		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemData);
		linker.SetItemData(item);

		initiator->Message(Chat::White, "%s tells you, %s", owner->GetCleanName(), linker.GenerateLink().c_str());
	}
}

void QuestManager::signalwith(int npc_id, int signal_id, int wait_ms, const char* data)
{
	if (npc_id < 1000 || npc_id / 1000 == zone->GetZoneID() || npc_id / 1000 == database.GetClientZoneID(zone->GetZoneID()))
		STimerList.push_back(SignalTimer(wait_ms < 0 ? 0 : wait_ms, npc_id, signal_id, data));
	else
		CrossZoneSignalNPCByNPCTypeID(npc_id, signal_id, data);
}

void QuestManager::signal(int npc_id, int wait_ms)
{
	if (npc_id < 1000 || npc_id / 1000 == zone->GetZoneID() || npc_id / 1000 == database.GetClientZoneID(zone->GetZoneID()) || npc_id > 999999)
		signalwith(npc_id, 0, wait_ms);
	else
		CrossZoneSignalNPCByNPCTypeID(npc_id, 0);
}

void QuestManager::setglobal(const char *varname, const char *newvalue, int options, const char *duration) {
	QuestManagerCurrentQuestVars();
	int qgZoneid = zone->GetZoneID();
	int qgCharid = 0;
	int qgNpcid = owner ? owner->GetNPCTypeID() : 0; // encounter scripts don't have an owner

	/*	options value determines the availability of global variables to NPCs when a quest begins
		------------------------------------------------------------------
			value		npcid		player		zone
		------------------------------------------------------------------
			0			this		this		this
			1			all			this		this
			2			this		all			this
			3			all			all			this
			4			this		this		all
			5			all			this		all
			6			this		all			all
			7			all			all			all
	*/

	if (initiator && initiator->IsClient()){ // some events like waypoint and spawn don't have a player involved
		qgCharid=initiator->CharacterID();
	}
	else {
		qgCharid=-qgNpcid;		// make char id negative npc id as a fudge
	}
	if (options < 0 || options > 7) {
		std::cerr << "Invalid options for global var " << varname << " using defaults" << std::endl;
	}	// default = 0 (only this npcid,player and zone)
	else {
		if (options & 1)
			qgNpcid=0;
		if (options & 2)
			qgCharid=0;
		if (options & 4)
			qgZoneid=0;
	}

	InsertQuestGlobal(qgCharid, qgNpcid, qgZoneid, varname, newvalue, QGVarDuration(duration));

	/* QS: PlayerLogQGlobalUpdate */
	if (RuleB(QueryServ, PlayerLogQGlobalUpdate) && qgCharid && qgCharid > 0 && initiator && initiator->IsClient())
	{
		QServ->QSQGlobalUpdate(qgCharid, initiator->GetZoneID(), varname, newvalue);
	}
}

/* Inserts global variable into quest_globals table */
int QuestManager::InsertQuestGlobal(int charid, int npcid, int zoneid, const char *varname, const char *varvalue, int duration) {

	// Make duration string either "unix_timestamp(now()) + xxx" or "NULL"
	std::string durationText = (duration == INT_MAX)? "NULL": StringFormat("unix_timestamp(now()) + %i", duration);

	/*
		NOTE: this should be escaping the contents of arglist
		npcwise a malicious script can arbitrarily alter the DB
	*/

	std::string query = StringFormat("REPLACE INTO quest_globals "
                                    "(charid, npcid, zoneid, name, value, expdate)"
                                    "VALUES (%i, %i, %i, '%s', '%s', %s)",
                                    charid, npcid, zoneid, varname, varvalue, durationText.c_str());
    auto results = database.QueryDatabase(query);
	if (!results.Success())
		std::cerr << "setglobal error inserting " << varname << " : " << results.ErrorMessage() << std::endl;

	if(!zone)
        return 0;

    /* Delete existing qglobal data and update zone processes */
	auto pack = new ServerPacket(ServerOP_QGlobalDelete, sizeof(ServerQGlobalDelete_Struct));
	ServerQGlobalDelete_Struct *qgd = (ServerQGlobalDelete_Struct *)pack->pBuffer;
	qgd->npc_id = npcid;
	qgd->char_id = charid;
	qgd->zone_id = zoneid;
	qgd->from_zone_id = zone->GetZoneID();
	strcpy(qgd->name, varname);

	entity_list.DeleteQGlobal(std::string((char *)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);
	zone->DeleteQGlobal(std::string((char *)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);

	worldserver.SendPacket(pack);
	safe_delete(pack);

	/* Create new qglobal data and update zone processes */
	pack = new ServerPacket(ServerOP_QGlobalUpdate, sizeof(ServerQGlobalUpdate_Struct));
	ServerQGlobalUpdate_Struct *qgu = (ServerQGlobalUpdate_Struct*)pack->pBuffer;
	qgu->npc_id = npcid;
	qgu->char_id = charid;
	qgu->zone_id = zoneid;

	qgu->expdate = (duration == INT_MAX)? 0xFFFFFFFF: Timer::GetTimeSeconds() + duration;

    strcpy((char*)qgu->name, varname);
    strn0cpy((char*)qgu->value, varvalue, 128);
	qgu->id = results.LastInsertedID();
	qgu->from_zone_id = zone->GetZoneID();

	QGlobal temp;
	temp.npc_id = npcid;
	temp.char_id = charid;
	temp.zone_id = zoneid;
	temp.expdate = qgu->expdate;
	temp.name.assign(qgu->name);
	temp.value.assign(qgu->value);
	entity_list.UpdateQGlobal(qgu->id, temp);
	zone->UpdateQGlobal(qgu->id, temp);

	worldserver.SendPacket(pack);
	safe_delete(pack);

	return 0;
}

void QuestManager::targlobal(const char *varname, const char *value, const char *duration, int qgNpcid, int qgCharid, int qgZoneid) {
	InsertQuestGlobal(qgCharid, qgNpcid, qgZoneid, varname, value, QGVarDuration(duration));
}

void QuestManager::delglobal(const char *varname) {
	QuestManagerCurrentQuestVars();
	int qgZoneid = zone->GetZoneID();
	int qgCharid = 0;
	int qgNpcid = owner ? owner->GetNPCTypeID() : 0; // encounter scripts don't have an owner

	if (initiator && initiator->IsClient()) // some events like way point and spawn don't have a player involved
		qgCharid=initiator->CharacterID();
	else
		qgCharid=-qgNpcid;		// make char id negative npc id as a fudge

	/* QS: PlayerLogQGlobalUpdate */
	if (RuleB(QueryServ, PlayerLogQGlobalUpdate) && qgCharid && qgCharid > 0 && initiator && initiator->IsClient())
	{
		QServ->QSQGlobalUpdate(qgCharid, initiator->GetZoneID(), varname, "deleted");
	}

    std::string query = StringFormat("DELETE FROM quest_globals "
                                    "WHERE name = '%s' "
                                    "&& (npcid=0 || npcid=%i) "
                                    "&& (charid=0 || charid=%i) "
                                    "&& (zoneid=%i || zoneid=0)",
                                    varname, qgNpcid, qgCharid, qgZoneid);
    auto results = database.QueryDatabase(query);
	if (!results.Success())
		std::cerr << "delglobal error deleting " << varname << " : " << results.ErrorMessage() << std::endl;

	if(!zone)
        return;

	auto pack = new ServerPacket(ServerOP_QGlobalDelete, sizeof(ServerQGlobalDelete_Struct));
	ServerQGlobalDelete_Struct *qgu = (ServerQGlobalDelete_Struct *)pack->pBuffer;

	qgu->npc_id = qgNpcid;
	qgu->char_id = qgCharid;
	qgu->zone_id = qgZoneid;
	strcpy(qgu->name, varname);

	entity_list.DeleteQGlobal(std::string((char *)qgu->name), qgu->npc_id, qgu->char_id, qgu->zone_id);
	zone->DeleteQGlobal(std::string((char *)qgu->name), qgu->npc_id, qgu->char_id, qgu->zone_id);

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

// Converts duration string to duration value (in seconds)
// Return of INT_MAX indicates infinite duration
int QuestManager::QGVarDuration(const char *fmt)
{
	int duration = 0;

	// format:	Y#### or D## or H## or M## or S## or T###### or C#######

	int len = strlen(fmt);

	// Default to no duration
	if (len < 1)
		return 0;

	// Set val to value after type character
	// e.g., for "M3924", set to 3924
	int val = atoi(&fmt[0] + 1);

	switch (fmt[0])
	{
		// Forever
		case 'F':
		case 'f':
			duration = INT_MAX;
			break;
		// Years
		case 'Y':
		case 'y':
			duration = val * 31556926;
			break;
		case 'D':
		case 'd':
			duration = val * 86400;
			break;
		// Hours
		case 'H':
		case 'h':
			duration = val * 3600;
			break;
		// Minutes
		case 'M':
		case 'm':
			duration = val * 60;
			break;
		// Seconds
		case 'S':
		case 's':
			duration = val;
			break;
		// Invalid
		default:
			duration = 0;
			break;
	}

	return duration;
}

void QuestManager::rebind(int zoneid, const glm::vec3& location) {
	QuestManagerCurrentQuestVars();
	if(initiator && initiator->IsClient()) {
		initiator->SetBindPoint(zoneid, location);
	}
}

void QuestManager::rebind(int zoneid, const glm::vec4& location) {
	QuestManagerCurrentQuestVars();
	if (initiator && initiator->IsClient()) {
		initiator->SetBindPoint2(zoneid, location);
	}
}

void QuestManager::start(int32 wp) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	owner->CastToNPC()->AssignWaypoints(wp);
	owner->CastToMob()->AI_TriggerHailTimer();
}

void QuestManager::stop() {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	owner->CastToNPC()->StopWandering();
}

void QuestManager::pause(int duration) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	owner->CastToNPC()->PauseWandering(duration);
}

void QuestManager::moveto(const glm::vec4& position, bool saveguardspot, uint32 delay) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	owner->CastToNPC()->MoveTo(position, saveguardspot, delay);
}

void QuestManager::resume() {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	owner->CastToNPC()->ResumeWandering();
}

void QuestManager::setnexthpevent(int at) {
	QuestManagerCurrentQuestVars();
	if (owner)
		owner->SetNextHPEvent(at);
}

void QuestManager::setnextinchpevent(int at) {
	QuestManagerCurrentQuestVars();
	if (owner)
		owner->SetNextIncHPEvent(at);
}

void QuestManager::respawn(int npcTypeID, int grid) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	running_quest e = quests_running_.top();
	e.depop_npc = true;
	quests_running_.pop();
	quests_running_.push(e);

	const NPCType* npcType = nullptr;
	if ((npcType = database.LoadNPCTypesData(npcTypeID)))
	{
		owner = new NPC(npcType, nullptr, owner->GetPosition(), GravityBehavior::Water);
		owner->CastToNPC()->AddLootTable();
		if (owner->CastToNPC()->DropsGlobalLoot()) {
			owner->CastToNPC()->CheckGlobalLootTables();
		}
		entity_list.AddNPC(owner->CastToNPC(),true,true);
		if(grid > 0)
			owner->CastToNPC()->AssignWaypoints(grid);
	}
}

void QuestManager::set_proximity(float minx, float maxx, float miny, float maxy, float minz, float maxz) {
	QuestManagerCurrentQuestVars();
	if (!owner || !owner->IsNPC())
		return;

	entity_list.AddProximity(owner->CastToNPC());

	owner->CastToNPC()->proximity->min_x = minx;
	owner->CastToNPC()->proximity->max_x = maxx;
	owner->CastToNPC()->proximity->min_y = miny;
	owner->CastToNPC()->proximity->max_y = maxy;
	owner->CastToNPC()->proximity->min_z = minz;
	owner->CastToNPC()->proximity->max_z = maxz;
	owner->CastToNPC()->proximity->say = false;
}

void QuestManager::clear_proximity() {
	QuestManagerCurrentQuestVars();
	if(!owner || !owner->IsNPC())
		return;

	entity_list.RemoveProximity(owner->GetID());
	safe_delete(owner->CastToNPC()->proximity);
}

void QuestManager::enable_proximity_say() {
	QuestManagerCurrentQuestVars();
	HaveProximitySays = true;
	if (!owner || !owner->IsNPC() || !owner->CastToNPC()->proximity)
		return;
	owner->CastToNPC()->proximity->say = true;
}

void QuestManager::disable_proximity_say() {
	QuestManagerCurrentQuestVars();
	HaveProximitySays = false;
	if (!owner || !owner->IsNPC() || !owner->CastToNPC()->proximity)
		return;
	owner->CastToNPC()->proximity->say = false;
}

void QuestManager::setanim(int npc_type, int animnum) {
	//adds appearance changes
	Mob* thenpc = entity_list.GetMobByNpcTypeID(npc_type);
	if(!thenpc || animnum < 0 || animnum >= _eaMaxAppearance)
		return;
	thenpc->SetAppearance(EmuAppearance(animnum));
}

//displays an in game path based on a waypoint grid
void QuestManager::showgrid(int grid) {
	QuestManagerCurrentQuestVars();
	if(initiator == nullptr)
		return;

	FindPerson_Point pt;
	std::vector<FindPerson_Point> pts;

	pt.x = initiator->GetX();
	pt.y = initiator->GetY();
	pt.z = initiator->GetZ();
	pts.push_back(pt);

	// Retrieve all waypoints for this grid
	std::string query = StringFormat("SELECT `x`,`y`,`z` FROM grid_entries "
                                    "WHERE `gridid` = %i AND `zoneid` = %i "
                                    "ORDER BY `number`", grid, zone->GetZoneID());
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        Log(Logs::General, Logs::Quests, "Error loading grid %d for showgrid(): %s", grid, results.ErrorMessage().c_str());
		return;
    }

    for(auto row = results.begin(); row != results.end(); ++row) {
        pt.x = atof(row[0]);
        pt.y = atof(row[1]);
        pt.z = atof(row[2]);

        pts.push_back(pt);
    }

}

//change the value of a spawn condition
void QuestManager::spawn_condition(const char *zone_short, uint32 instance_id, uint16 condition_id, short new_value) {
	zone->spawn_conditions.SetCondition(zone_short, instance_id, condition_id, new_value);
}

//get the value of a spawn condition
short QuestManager::get_spawn_condition(const char *zone_short, uint32 instance_id, uint16 condition_id) {
	return(zone->spawn_conditions.GetCondition(zone_short, instance_id, condition_id));
}

//toggle a spawn event
void QuestManager::toggle_spawn_event(int event_id, bool enable, bool strict, bool reset_base) {
	zone->spawn_conditions.ToggleEvent(event_id, enable, strict, reset_base);
}

bool QuestManager::has_zone_flag(int zone_id) {
	QuestManagerCurrentQuestVars();
	return initiator ? initiator->HasZoneFlag(zone_id) : false;
}

void QuestManager::set_zone_flag(int zone_id) {
	QuestManagerCurrentQuestVars();
	initiator->SetZoneFlag(zone_id);
}

void QuestManager::clear_zone_flag(int zone_id) {
	QuestManagerCurrentQuestVars();
	initiator->ClearZoneFlag(zone_id);
}

void QuestManager::sethp(int hpperc) {
	QuestManagerCurrentQuestVars();
	int newhp = (owner->GetMaxHP() * (100 - hpperc)) / 100;
	owner->Damage(owner, newhp, SPELL_UNKNOWN, EQ::skills::SkillHandtoHand, false, 0, false);
}

bool QuestManager::summonburriedplayercorpse(uint32 char_id, const glm::vec4& position) {
	bool Result = false;

	if(char_id <= 0)
        return false;

	Corpse* PlayerCorpse = database.SummonBuriedCharacterCorpses(char_id, zone->GetZoneID(), zone->GetGuildID(), position);
	if(!PlayerCorpse)
		return false;

	return true;
}

bool QuestManager::summonallplayercorpses(uint32 char_id, const glm::vec4& position) {

	if(char_id <= 0)
        return false;

	Client* c = entity_list.GetClientByCharID(char_id);
	c->SummonAllCorpses(position);

	return true;
}

uint32 QuestManager::getplayerburriedcorpsecount(uint32 char_id) {
	uint32 Result = 0;

	if(char_id > 0) {
		Result = database.GetCharacterBuriedCorpseCount(char_id);
	}
	return Result;
}

bool QuestManager::buryplayercorpse(uint32 char_id)
{
	bool Result = false;

	if(char_id > 0)
	{
		uint32 PlayerCorpse = database.GetFirstCorpseID(char_id);
		if(PlayerCorpse > 0)
		{
			database.BuryCharacterCorpse(PlayerCorpse);
			Corpse* corpse = entity_list.GetCorpseByDBID(PlayerCorpse);
			if(corpse)
			{
				corpse->Save();
				corpse->DepopPlayerCorpse();
			}
			else
			{
				Client *c = entity_list.GetClientByCharID(char_id);
				c->DepopPlayerCorpse(PlayerCorpse);
			}
			Result = true;
		}
	}
	return Result;
}

void QuestManager::forcedooropen(uint32 doorid, bool altmode) {
	Doors* d = entity_list.FindDoor(doorid);
	if(d){
		if(GetInitiator())
			d->ForceOpen(GetInitiator(), altmode);
		else if(GetOwner())
			d->ForceOpen(GetOwner(), altmode);
	}
}

void QuestManager::forcedoorclose(uint32 doorid, bool altmode) {
	Doors* d = entity_list.FindDoor(doorid);
	if(d){
		if(GetInitiator())
			d->ForceClose(GetInitiator(), altmode);
		else if(GetOwner())
			d->ForceClose(GetOwner(), altmode);
	}
}

void QuestManager::toggledoorstate(uint32 doorid) {
	Doors* d = entity_list.FindDoor(doorid);
	if(d){
		if(GetInitiator())
			d->ToggleState(GetInitiator());
		else if(GetOwner())
			d->ToggleState(GetOwner());
	}
}

bool QuestManager::isdooropen(uint32 doorid) {
	Doors* d = entity_list.FindDoor(doorid);
	if(d){
		return d->IsDoorOpen();
	}
	return false;
}

void QuestManager::npcrace(int race_id)
{
	QuestManagerCurrentQuestVars();
	owner->SendIllusionPacket(race_id);
}

void QuestManager::npcgender(int gender_id)
{
	QuestManagerCurrentQuestVars();
	owner->SendIllusionPacket(owner->GetRace(), gender_id);
}

void QuestManager::npcsize(int newsize)
{
	QuestManagerCurrentQuestVars();
	owner->ChangeSize(newsize, true);
}

void QuestManager::npctexture(int newtexture)
{
	QuestManagerCurrentQuestVars();
	owner->SendIllusionPacket(owner->GetRace(), 0xFF, newtexture);
}

void QuestManager::playerrace(int race_id)
{
	QuestManagerCurrentQuestVars();
	initiator->SendIllusionPacket(race_id);
}

void QuestManager::playergender(int gender_id)
{
	QuestManagerCurrentQuestVars();
	initiator->SendIllusionPacket(initiator->GetRace(), gender_id);
}

void QuestManager::playersize(int newsize)
{
	QuestManagerCurrentQuestVars();
	initiator->ChangeSize(newsize, true);
}

void QuestManager::playertexture(int newtexture)
{
	QuestManagerCurrentQuestVars();
	initiator->SendIllusionPacket(initiator->GetRace(), 0xFF, newtexture);
}

void QuestManager::playerfeature(char *feature, int setting)
{
	QuestManagerCurrentQuestVars();
	uint16 Race = initiator->GetRace();
	uint8 Gender = initiator->GetGender();
	uint8 Texture = 0xFF;
	uint8 HelmTexture = 0xFF;
	uint8 HairColor = initiator->GetHairColor();
	uint8 BeardColor = initiator->GetBeardColor();
	uint8 EyeColor1 = initiator->GetEyeColor1();
	uint8 EyeColor2 = initiator->GetEyeColor2();
	uint8 HairStyle = initiator->GetHairStyle();
	uint8 LuclinFace = initiator->GetLuclinFace();
	uint8 Beard = initiator->GetBeard();
	float Size = initiator->GetSize();

	if (!strcasecmp(feature,"race"))
		Race = setting;
	else if (!strcasecmp(feature,"gender"))
		Gender = setting;
	else if (!strcasecmp(feature,"texture"))
		Texture = setting;
	else if (!strcasecmp(feature,"helm"))
		HelmTexture = setting;
	else if (!strcasecmp(feature,"haircolor"))
		HairColor = setting;
	else if (!strcasecmp(feature,"beardcolor"))
		BeardColor = setting;
	else if (!strcasecmp(feature,"eyecolor1"))
		EyeColor1 = setting;
	else if (!strcasecmp(feature,"eyecolor2"))
		EyeColor2 = setting;
	else if (!strcasecmp(feature,"hair"))
		HairStyle = setting;
	else if (!strcasecmp(feature,"face"))
		LuclinFace = setting;
	else if (!strcasecmp(feature,"beard"))
		Beard = setting;
	else if (!strcasecmp(feature,"size"))
		Size = (float)setting / 10;	//dividing by 10 to allow 1 decimal place for adjusting size
	else
		return;

	initiator->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
										EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF, Size);
}

void QuestManager::npcfeature(char *feature, int setting)
{
	QuestManagerCurrentQuestVars();
	uint16 Race = owner->GetRace();
	uint8 Gender = owner->GetGender();
	uint8 Texture = 0xFF;
	uint8 HelmTexture = 0xFF;
	uint8 HairColor = owner->GetHairColor();
	uint8 BeardColor = owner->GetBeardColor();
	uint8 EyeColor1 = owner->GetEyeColor1();
	uint8 EyeColor2 = owner->GetEyeColor2();
	uint8 HairStyle = owner->GetHairStyle();
	uint8 LuclinFace = owner->GetLuclinFace();
	uint8 Beard = owner->GetBeard();
	float Size = owner->GetSize();

	if (!strcasecmp(feature,"race"))
		Race = setting;
	else if (!strcasecmp(feature,"gender"))
		Gender = setting;
	else if (!strcasecmp(feature,"texture"))
		Texture = setting;
	else if (!strcasecmp(feature,"helm"))
		HelmTexture = setting;
	else if (!strcasecmp(feature,"haircolor"))
		HairColor = setting;
	else if (!strcasecmp(feature,"beardcolor"))
		BeardColor = setting;
	else if (!strcasecmp(feature,"eyecolor1"))
		EyeColor1 = setting;
	else if (!strcasecmp(feature,"eyecolor2"))
		EyeColor2 = setting;
	else if (!strcasecmp(feature,"hair"))
		HairStyle = setting;
	else if (!strcasecmp(feature,"face"))
		LuclinFace = setting;
	else if (!strcasecmp(feature,"beard"))
		Beard = setting;
	else if (!strcasecmp(feature,"size"))
		Size = (float)setting / 10;	//dividing by 10 to allow 1 decimal place for adjusting size
	else
		return;

	owner->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
										EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF, Size);
}

void QuestManager::clearspawntimers() {
	if(!zone)
        return;

	//TODO: Dec 19, 2008, replace with code updated for current spawn timers.
    LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
	iterator.Reset();
	while (iterator.MoreElements()) {
		std::string query = StringFormat("DELETE FROM respawn_times "
                                        "WHERE id = %lu and guild_id = %lu",
                                        (unsigned long)iterator.GetData()->GetID(), (unsigned long)zone->GetGuildID());
        auto results = database.QueryDatabase(query);
		iterator.Advance();
	}
}

void QuestManager::ze(int type, const char *str) {
	entity_list.Message(Chat::White, type, str);
}

void QuestManager::we(int type, const char *str) {
	worldserver.SendEmoteMessage(0, 0, type, str);
}

int QuestManager::getlevel(uint8 type)
{
	QuestManagerCurrentQuestVars();
	if (type == 0)
	{
		return (initiator->GetLevel());
	}
	else if(type == 1)
	{
		Group *g = entity_list.GetGroupByClient(initiator);
		if (g != nullptr)
			return (g->GetAvgLevel());
		else
			return 0;
	}
	else if(type == 2)
	{
		Raid *r = entity_list.GetRaidByClient(initiator);
		if (r != nullptr)
			return (r->GetAvgLevel());
		else
			return 0;
	}
	else if(type == 3)
	{
		Raid *r = entity_list.GetRaidByClient(initiator);
		if(r != nullptr)
		{
			return (r->GetAvgLevel());
		}
		Group *g = entity_list.GetGroupByClient(initiator);
		if(g != nullptr)
		{
			return (g->GetAvgLevel());
		}
		else
			return (initiator->GetLevel());
	}
	else if(type == 4 && initiator->IsClient())
	{
		return (initiator->CastToClient()->GetLevel2());
	}
	else
		return 0;
}

uint16 QuestManager::CreateGroundObject(uint32 itemid, const glm::vec4& position, uint32 decay_time)
{
	uint16 entid = 0; //safety check
	entid = entity_list.CreateGroundObject(itemid, position, decay_time);
	return entid;
}

uint16 QuestManager::CreateGroundObjectFromModel(const char *model, const glm::vec4& position, uint8 type, uint32 decay_time)
{
	uint16 entid = 0; //safety check
	entid = entity_list.CreateGroundObjectFromModel(model, position, type, decay_time);
	return entid;
}

void QuestManager::ModifyNPCStat(const char *identifier, const char *newValue)
{
	QuestManagerCurrentQuestVars();
	if(owner){
		if(owner->IsNPC())
		{
			owner->CastToNPC()->ModifyNPCStat(identifier, newValue);
		}
	}
}

int QuestManager::collectitems_processSlot(int16 slot_id, uint32 item_id,
	bool remove)
{
	QuestManagerCurrentQuestVars();
	EQ::ItemInstance *item = nullptr;
	int quantity = 0;

	item = initiator->GetInv().GetItem(slot_id);

	// If we have found matching item, add quantity
	if (item && item->GetID() == item_id)
	{
		// If item is stackable, add its charges (quantity)
		if (item->IsStackable())
		{
			quantity = item->GetCharges();
		}
		else
		{
			quantity = 1;
		}

		// Remove item from inventory
		if (remove)
		{
			initiator->DeleteItemInInventory(slot_id, 0, true);
		}
	}

	return quantity;
}

// Returns number of item_id that exist in inventory
// If remove is true, items are removed as they are counted.
int QuestManager::collectitems(uint32 item_id, bool remove)
{
	int quantity = 0;
	int slot_id;

	for (slot_id = EQ::invslot::GENERAL_BEGIN; slot_id <= EQ::invslot::GENERAL_END; ++slot_id)
	{
		quantity += collectitems_processSlot(slot_id, item_id, remove);
	}

	for (slot_id = EQ::invbag::GENERAL_BAGS_BEGIN; slot_id <= EQ::invbag::GENERAL_BAGS_END; ++slot_id)
	{
		quantity += collectitems_processSlot(slot_id, item_id, remove);
	}

	return quantity;
}

void QuestManager::UpdateSpawnTimer(uint32 id, uint32 newTime)
{
	bool found = false;

	database.UpdateRespawnTime(id, (newTime/1000), zone ? zone->GetGuildID() : GUILD_NONE);
	LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		if(iterator.GetData()->GetID() == id)
		{
			if(!iterator.GetData()->NPCPointerValid())
			{
				iterator.GetData()->SetTimer(newTime);
			}
			found = true;
			break;
		}
		iterator.Advance();
	}

	if(!found)
	{
		//Spawn wasn't in this zone...
		//Tell the other zones to update their spawn time for this spawn point
		auto pack = new ServerPacket(ServerOP_UpdateSpawn, sizeof(UpdateSpawnTimer_Struct));
		UpdateSpawnTimer_Struct *ust = (UpdateSpawnTimer_Struct*)pack->pBuffer;
		ust->id = id;
		ust->duration = newTime;
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

// used to set the number of an item in the selected merchant's temp item list. Defaults to zero if no quantity is specified.
void QuestManager::MerchantSetItem(uint32 NPCid, uint32 itemid, uint32 quantity) {
	Mob* merchant = entity_list.GetMobByNpcTypeID(NPCid);

	if (merchant == 0 || !merchant->IsNPC() || (merchant->GetClass() != MERCHANT))
		return;	// don't do anything if NPCid isn't a merchant

	const EQ::ItemData* item = nullptr;
	item = database.GetItem(itemid);
	if (!item) return;		// if the item id doesn't correspond to a real item, do nothing

	zone->SaveTempItem(merchant->CastToNPC()->MerchantType, NPCid, itemid, quantity);
}

uint32 QuestManager::MerchantCountItem(uint32 NPCid, uint32 itemid) {
	Mob* merchant = entity_list.GetMobByNpcTypeID(NPCid);

	if (merchant == 0 || !merchant->IsNPC() || (merchant->GetClass() != MERCHANT))
		return 0;	// if it isn't a merchant, it doesn't have any items

	const EQ::ItemData* item = nullptr;
	item = database.GetItem(itemid);
	if (!item)
		return 0;	// if it isn't a valid item, the merchant doesn't have any

	// look for the item in the merchant's temporary list
	std::list<TempMerchantList> MerchList = zone->tmpmerchanttable[NPCid];
	std::list<TempMerchantList>::const_iterator itr;
	uint32 Quant = 0;

	for (itr = MerchList.begin(); itr != MerchList.end(); ++itr) {
		if (itr->item == itemid) {	// if this is the item we're looking for
			Quant = itr->charges;
			break;
		}
	}

	return Quant;	// return the quantity of itemid (0 if it was never found)
}

// Item Link for use in Variables - "my $example_link = quest::varlink(item_id);"
const char* QuestManager::varlink(char* perltext, int item_id) {
	QuestManagerCurrentQuestVars();
	const EQ::ItemData *item = database.GetItem(item_id);
	if (!item)
		return "INVALID ITEM ID IN VARLINK";

	EQ::SayLinkEngine linker;
	linker.SetLinkType(EQ::saylink::SayLinkItemData);
	linker.SetItemData(item);

	strcpy(perltext, linker.GenerateLink().c_str()); // link length is currently ranged from 1 to 250 in TextLink::GenerateLink()

	return perltext;
}

std::string QuestManager::saylink(char* saylink_text, bool silent, const char* link_name) 
{
	QuestManagerCurrentQuestVars();

	return Saylink::Create(saylink_text, silent, link_name);

}

const char* QuestManager::getguildnamebyid(int guild_id) {
	if (guild_id > 0)
		return guild_mgr.GetGuildName(guild_id);
	else
		return("");
}

void QuestManager::SetRunning(bool val)
{
	QuestManagerCurrentQuestVars();
	if(!owner)
		return;
	owner->SetRunning(val);
}

bool QuestManager::IsRunning()
{
	QuestManagerCurrentQuestVars();
	if(!owner)
		return false;
	return owner->IsRunning();
}

void QuestManager::FlyMode(GravityBehavior flymode)
{
	QuestManagerCurrentQuestVars();
	if(initiator)
	{

		initiator->SendAppearancePacket(AppearanceType::FlyMode, static_cast<int>(flymode));
		initiator->SetFlyMode(flymode);
	}
	else if(owner)
	{
		owner->SendAppearancePacket(AppearanceType::FlyMode, static_cast<int>(flymode));
		owner->SetFlyMode(flymode);
	}
}

uint8 QuestManager::FactionValue()
{
	QuestManagerCurrentQuestVars();
	FACTION_VALUE oldfac;
	uint8 newfac = 0;
	if(initiator && owner->IsNPC()) {
		oldfac = initiator->GetFactionLevel(initiator->GetID(), initiator->GetRace(), initiator->GetClass(), initiator->GetDeity(), owner->GetPrimaryFaction(), owner);

		// now, reorder the faction to have it make sense (higher values are better)
		switch (oldfac) {
			case FACTION_SCOWLS:
				newfac = 1;
				break;
			case FACTION_THREATENINGLY:
				newfac = 2;
				break;
			case FACTION_DUBIOUSLY:
				newfac = 3;
				break;
			case FACTION_APPREHENSIVELY:
				newfac = 4;
				break;
			case FACTION_INDIFFERENTLY:
				newfac = 5;
				break;
			case FACTION_AMIABLY:
				newfac = 6;
				break;
			case FACTION_KINDLY:
				newfac = 7;
				break;
			case FACTION_WARMLY:
				newfac = 8;
				break;
			case FACTION_ALLY:
				newfac = 9;
				break;
		}
	}

	return newfac;
}

void QuestManager::enabletitle(int titleset) {
	QuestManagerCurrentQuestVars();
	initiator->EnableTitle(titleset);
}

bool QuestManager::checktitle(int titleset) {
	QuestManagerCurrentQuestVars();
	return initiator ? initiator->CheckTitle(titleset) : false;
}

void QuestManager::removetitle(int titleset) {
	QuestManagerCurrentQuestVars();
	initiator->RemoveTitle(titleset);
}

void QuestManager::wearchange(uint8 slot, uint16 texture)
{
	QuestManagerCurrentQuestVars();
	if(owner){
		owner->WearChange(slot, texture, 0);
		if(owner->IsNPC()) {
			owner->CastToNPC()->NPCSlotTexture(slot, texture);
		}
	}
}

void QuestManager::SendMail(const char *to, const char *from, const char *subject, const char *message) {
	if(to == nullptr || from == nullptr || subject == nullptr || message == nullptr) {
		return;
	}

	uint32 message_len = strlen(message) + 1;
	auto pack = new ServerPacket(ServerOP_UCSMailMessage, sizeof(ServerMailMessageHeader_Struct) + message_len);
	ServerMailMessageHeader_Struct* mail = (ServerMailMessageHeader_Struct*) pack->pBuffer;

	strn0cpy(mail->to, to, 64);
	strn0cpy(mail->from, from, 64);
	strn0cpy(mail->subject, subject, 128);
	strcpy(mail->message, message);

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

uint16 QuestManager::CreateDoor(const char* model, float x, float y, float z, float heading, uint8 opentype, uint16 size)
{
	uint16 entid = 0; //safety check
	entid = entity_list.CreateDoor(model, glm::vec4(x, y, z, heading), opentype, size);
	return entid;
}

int32 QuestManager::GetZoneID(const char *zone) {
	return static_cast<int32>(database.GetZoneID(zone));
}

const char* QuestManager::GetZoneLongName(const char *zone) {
	char *long_name;
	database.GetZoneLongName(zone, &long_name);
	std::string ln = long_name;
	safe_delete_array(long_name);

	return ln.c_str();
}

void QuestManager::CrossZoneSignalNPCByNPCTypeID(uint32 npctype_id, int num, const char* data){
	uint32 data_len = data ? strlen(data) + 1 : 1;
	auto pack = new ServerPacket(ServerOP_CZSignalNPC, sizeof(CZNPCSignal_Struct) + data_len);
	CZNPCSignal_Struct* CZSN = (CZNPCSignal_Struct*)pack->pBuffer;
	CZSN->npctype_id = npctype_id;
	CZSN->num = num;
	strcpy(CZSN->data, data ? data : "");
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::CrossZoneSignalPlayerByCharID(int charid, uint32 data){
	auto pack = new ServerPacket(ServerOP_CZSignalClient, sizeof(CZClientSignal_Struct));
	CZClientSignal_Struct* CZSC = (CZClientSignal_Struct*) pack->pBuffer;
	CZSC->charid = charid;
	CZSC->data = data;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::CrossZoneSignalPlayerByName(const char *CharName, uint32 data){
	uint32 message_len = strlen(CharName) + 1;
	auto pack = new ServerPacket(ServerOP_CZSignalClientByName, sizeof(CZClientSignalByName_Struct) + message_len);
	CZClientSignalByName_Struct* CZSC = (CZClientSignalByName_Struct*) pack->pBuffer;
	strn0cpy(CZSC->Name, CharName, 64);
	CZSC->data = data;
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::CrossZoneMessagePlayerByName(uint32 Type, const char *CharName, const char *Message){
	uint32 message_len = strlen(CharName) + 1;
	uint32 message_len2 = strlen(Message) + 1;
	auto pack =
	    new ServerPacket(ServerOP_CZMessagePlayer, sizeof(CZMessagePlayer_Struct) + message_len + message_len2);
	CZMessagePlayer_Struct* CZSC = (CZMessagePlayer_Struct*) pack->pBuffer;
	CZSC->Type = Type;
	strn0cpy(CZSC->CharName, CharName, 64);
	strn0cpy(CZSC->Message, Message, 512);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::CrossZoneSetEntityVariableByNPCTypeID(uint32 npctype_id, const char *id, const char *m_var){
	uint32 message_len = strlen(id) + 1;
	uint32 message_len2 = strlen(m_var) + 1;
	auto pack = new ServerPacket(ServerOP_CZSetEntityVariableByNPCTypeID,
				     sizeof(CZSetEntVarByNPCTypeID_Struct) + message_len + message_len2);
	CZSetEntVarByNPCTypeID_Struct* CZSNBYNID = (CZSetEntVarByNPCTypeID_Struct*)pack->pBuffer;
	CZSNBYNID->npctype_id = npctype_id;
	strn0cpy(CZSNBYNID->id, id, 256);
	strn0cpy(CZSNBYNID->m_var, m_var, 256);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QuestManager::SendDebug(const char* message, int level)
{
	if (level > Logs::Detail)
		return;

	if (level == Logs::General)
	{
		Log(Logs::General, Logs::QuestDebug, message);
	}
	else if (level == Logs::Detail)
	{
		Log(Logs::Detail, Logs::QuestDebug, message);
	}
}

bool QuestManager::EnableRecipe(uint32 recipe_id)
{
	bool success = false;
	if (recipe_id > 0)
		success = database.EnableRecipe(recipe_id);
	return (success);
}

bool QuestManager::DisableRecipe(uint32 recipe_id)
{
	bool success = false;
	if (recipe_id > 0)
		success = database.DisableRecipe(recipe_id);
	return (success);
}

void QuestManager::ClearNPCTypeCache(int npctype_id) {
	if (zone) {
		zone->ClearNPCTypeCache(npctype_id);
	}
}

void QuestManager::ReloadZoneStaticData()
{
	if (zone) {
		zone->ReloadStaticData();
	}
}

Client *QuestManager::GetInitiator() const {
	if(!quests_running_.empty()) {
		running_quest e = quests_running_.top();
		return e.initiator;
	}

	return nullptr;
}

NPC *QuestManager::GetNPC() const {
	if(!quests_running_.empty()) {
		running_quest e = quests_running_.top();
		return (e.owner && e.owner->IsNPC()) ? e.owner->CastToNPC() : nullptr;
	}

	return nullptr;
}

Mob *QuestManager::GetOwner() const {
	if(!quests_running_.empty()) {
		running_quest e = quests_running_.top();
		return e.owner;
	}

	return nullptr;
}

EQ::ItemInstance *QuestManager::GetQuestItem() const {
	if(!quests_running_.empty()) {
		running_quest e = quests_running_.top();
		return e.questitem;
	}

	return nullptr;
}

std::string QuestManager::GetEncounter() const {
	if(!quests_running_.empty()) {
		running_quest e = quests_running_.top();
		return e.encounter;
	}

	return "";
}

float QuestManager::GetCurrentExpansion() const {
	return RuleR(World, CurrentExpansion);
}

std::string QuestManager::getdeityname(uint32 deity_id) {
	return Deity::GetName(deity_id);
}