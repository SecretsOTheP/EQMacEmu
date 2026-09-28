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
#include "../common/global_define.h"
#include "../common/misc_functions.h"
#include "zonelist.h"
#include "zoneserver.h"
#include "worlddb.h"
#include "ucs.h"
#include "world_config.h"
#include "../common/json/json.h"
#include "../common/event_sub.h"
#include "web_interface.h"
#include "../common/servertalk.h"
#include "../common/strings.h"
#include "../common/random.h"

#include <algorithm>
#include "../common/zone_store.h"
#include "queryserv.h"


extern uint32 numzones;
extern bool holdzones;
extern UCSConnection UCSLink;
extern QueryServConnection QSLink;
extern EQ::Random emu_random;
extern WebInterfaceList web_interface;
volatile bool UCSServerAvailable_ = false;

void CatchSignal(int sig_num);

ZSList::ZSList()
{
	NextID = 1;
	CurGroupID = 1;
	memset(pLockedZones, 0, sizeof(pLockedZones));

	m_tick = std::make_unique<EQ::Timer>(5000, true, std::bind(&ZSList::OnTick, this, std::placeholders::_1));
}

ZSList::~ZSList() {
}

void ZSList::ShowUpTime(WorldTCPConnection* con, const char* adminname) {
	uint32 ms = Timer::GetCurrentTime();
	std::string time_string = Strings::MillisecondsToTime(ms);
	con->SendEmoteMessage(
		adminname,
		0,
		AccountStatus::Player,
		Chat::White,
		fmt::format(
			"Worldserver Uptime | {}",
			time_string
		).c_str()
	);
}

void ZSList::Add(ZoneServer* zoneserver) {
	zone_server_list.emplace_back(std::unique_ptr<ZoneServer>(zoneserver));
	zoneserver->SendGroupIDs();
}

void ZSList::Remove(const std::string& uuid)
{
	auto iter = zone_server_list.begin();
	while (iter != zone_server_list.end()) {
		if ((*iter)->GetUUID().compare(uuid) == 0) {
			auto port = (*iter)->GetCPort();
			zone_server_list.erase(iter);

			// Used to push every departing zone's port, including static ports like 7248, into the
			// dynamic pool. A crashed static zone's port then went to a dynamic zone, which failed to
			// bind once the static zone restarted but stayed registered on it.
			ReleaseZonePort(port);
			return;
		}
		iter++;
	}
}

void ZSList::KillAll() {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		(*iterator)->Disconnect();
		iterator = zone_server_list.erase(iterator);
	}
}

void ZSList::Process() {

	if(shutdowntimer && shutdowntimer->Check()){
		LogInfo("Shutdown timer has expired. Telling all zones to shut down and exiting. (fake sigint)");
		auto pack2 = new ServerPacket;
		pack2->opcode = ServerOP_ShutdownAll;
		pack2->size=0;
		SendPacket(pack2);
		safe_delete(pack2);
		Process();
		CatchSignal(2);
	}
	if(reminder && reminder->Check() && shutdowntimer){
		SendEmoteMessage(0, 0, AccountStatus::Player, Chat::Yellow , fmt::format("[SYSTEM] World coming down in {} minutes.", ((shutdowntimer->GetRemainingTime() / 1000) / 60)).c_str());
	}
}

bool ZSList::SendPacket(ServerPacket* pack) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		(*iterator)->SendPacket(pack);
		iterator++;
	}
	return true;
}

bool ZSList::SendPacket(uint32 ZoneID, uint32 GuildID, ServerPacket* pack) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		//Secrets note: 0 is 'magic number' for all zones. For a specific, non-guild zone, use 0xFFFFFFFF
		if ((*iterator)->GetZoneID() == ZoneID && ((*iterator)->GetZoneGuildID() == GuildID || GuildID == 0)) {
			ZoneServer* tmp = (*iterator).get();
			tmp->SendPacket(pack);
			return true;
		}
		iterator++;
	}
	return(false);
}

ZoneServer* ZSList::FindByName(const char* zonename) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		//Secrets note: 0 is 'magic number' for all zones. For a specific, non-guild zone, use 0xFFFFFFFF
		//Unbooted zones can still end up with 0. So keep that in mind. Though, in this context they shouldn't have a zonename either.
		if (strcasecmp((*iterator)->GetZoneName(), zonename) == 0 && ((*iterator)->GetZoneGuildID() == 0 || (*iterator)->GetZoneGuildID() == 0xFFFFFFFF)) {
			ZoneServer* tmp = (*iterator).get();
			return tmp;
		}
		iterator++;
	}
	return 0;
}

ZoneServer* ZSList::FindByID(uint32 ZoneID) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		if ((*iterator)->GetID() == ZoneID) {
			ZoneServer* tmp = (*iterator).get();
			return tmp;
		}
		iterator++;
	}
	return 0;
}

ZoneServer* ZSList::FindByZoneID(uint32 ZoneID, uint32 GuildID) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		ZoneServer* tmp = (*iterator).get();
		if (tmp->GetZoneID() == ZoneID && tmp->GetZoneGuildID() == GuildID ) {
			return tmp;
		}
		iterator++;
	}
	return 0;
}

ZoneServer* ZSList::FindByPort(uint16 port) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		if ((*iterator)->GetCPort() == port) {
			ZoneServer* tmp = (*iterator).get();
			return tmp;
		}
		iterator++;
	}
	return 0;
}

bool ZSList::SetLockedZone(uint16 iZoneID, bool iLock) {
	for (auto &zone : pLockedZones) {
		if (iLock) {
			if (zone == 0) {
				zone = iZoneID;
				return true;
			}
		}
		else {
			if (zone == iZoneID) {
				zone = 0;
				return true;
			}
		}
	}
	return false;
}

void ZSList::Init()
{
	const WorldConfig* Config = WorldConfig::get();
	for (uint16 i = Config->ZonePortLow; i <= Config->ZonePortHigh; ++i) {
		m_ports_free.push_back(i);
	}
}

bool ZSList::IsZoneLocked(uint16 iZoneID) {
	for (auto &zone : pLockedZones) {
		if (zone == iZoneID)
			return true;
	}
	return false;
}

void ZSList::ListLockedZones(const char* to, WorldTCPConnection* connection) {
	int x = 0;
	for (auto &zone : pLockedZones) {
		if (zone) {
			connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::White, ZoneName(zone, true));
			x++;
		}
	}
	connection->SendEmoteMessage(to, 0, AccountStatus::Player, Chat::White, "%i zones locked.", x);
}

void ZSList::SendZoneStatus(const char* to, int16 admin, WorldTCPConnection* connection) 
{
	char locked[4];
	if (WorldConfig::get()->Locked == true)	{
		strcpy(locked, "Yes");
	}
	else  {
		strcpy(locked, "No");
	}

	auto out = fmt::memory_buffer();

	if (connection->IsConsole()) {
		fmt::format_to(std::back_inserter(out), "World Locked: {}\r\n", locked);
	}
	else {
		fmt::format_to(std::back_inserter(out), "World Locked: {}^", locked);
	}

	if (connection->IsConsole()) {
		fmt::format_to(std::back_inserter(out), "Zoneservers online:\r\n");
	}
	else {
		fmt::format_to(std::back_inserter(out), "Zoneservers online:^");
	}

	int avail_zones = 0, booted_zones = 0, servers_online = 0, static_zones = 0;
	std::string buffer = " ";
	std::string is_static_string = "", zone_data_string = "";

	ZoneServer* zone_server_data = 0;

	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		zone_server_data = (*iterator).get();
		auto addr = zone_server_data->GetIP();

		if (!zone_server_data->IsStaticZone()) {
			is_static_string = "D";
			if (zone_server_data->GetZoneID() != 0) {
				++booted_zones;
			}
			else if (zone_server_data->GetZoneID() == 0 && !zone_server_data->IsBootingUp()) {
				++avail_zones;
			}
		}
		else {
			is_static_string = "S";
			++static_zones;
		}

		if (zone_server_data->GetZoneID()) {
			std::ostringstream stream;
			stream << "" << zone_server_data->GetZoneName() << " (" << zone_server_data->GetZoneID() << ")";
			zone_data_string = stream.str();
		}
		else if (zone_server_data->IsBootingUp()) {
			zone_data_string = "BOOTING";
		}
		else {
			zone_data_string = "AVAILABLE";
		}

		fmt::format_to(std::back_inserter(out),
			"{} {} :: {}  :: {}:{} :: {} :: {}:{} :: {} :: ({})", 
			buffer.c_str(),
			zone_server_data->GetID(), 
			is_static_string, 
			addr.c_str(),
			zone_server_data->GetPort(), 
			zone_server_data->NumPlayers(), 
			zone_server_data->GetCAddress(), 
			zone_server_data->GetCPort(), 
			zone_data_string,
			zone_server_data->GetZoneOSProcessID()
		);
			
		if (out.size() >= 3584) {
			out.push_back(0);
			connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
			out.clear();
		}
		else  {
			if (connection->IsConsole()) {
				fmt::format_to(std::back_inserter(out), "\r\n");
			}
			else {
				fmt::format_to(std::back_inserter(out), "^");
			}
		}

		++servers_online;
		iterator++;
	}

	if (connection->IsConsole()) {
		fmt::format_to(std::back_inserter(out), "{} {} servers online.\r\n", buffer.c_str(), servers_online);
	}
	else {
		fmt::format_to(std::back_inserter(out), "{} {} servers online.^", buffer.c_str(), servers_online);
	}

	fmt::format_to(std::back_inserter(out), "{} {} zones are static zones, {} zones are booted zones, {} zones available.", buffer.c_str(), static_zones, booted_zones, avail_zones);

	out.push_back(0);
	connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
}

void ZSList::SendZoneCountConsole(const char* to, int16 admin, WorldTCPConnection* connection, bool zonepop)
{
	std::vector<char> out;
	int avail_zones = 0, booted_zones = 0, servers_online = 0, static_zones = 0, num_players = 0;
	ZoneServer* zone_server_data = 0;
	bool booted = false;

	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		zone_server_data = (*iterator).get();

		if (!zone_server_data->IsStaticZone())
		{
			if (zone_server_data->GetZoneID() != 0)
			{
				++booted_zones;
				booted = true;
			}
			else if (zone_server_data->GetZoneID() == 0 && !zone_server_data->IsBootingUp())
			{
				++avail_zones;
			}
		}
		else
		{
			++static_zones;
			booted = true;
		}

		if (zonepop && booted)
		{
			fmt::format_to(std::back_inserter(out), "{}:{}\r\n", zone_server_data->GetZoneName(), zone_server_data->NumPlayers());
		}

		num_players += zone_server_data->NumPlayers();

		++servers_online;
		booted = false;
		iterator++;
	}

	if (!zonepop)
	{
		fmt::format_to(std::back_inserter(out), "static:{} total:{} dyn:{} pop:{}\r\n", static_zones, servers_online, avail_zones, num_players);
	}

	out.push_back(0);
	connection->SendEmoteMessageRaw(to, 0, AccountStatus::Player, Chat::NPCQuestSay, out.data());
}

void ZSList::SendChannelMessage(const char* from, const char* to, uint8 chan_num, uint8 language, uint8 lang_skill, const char* message, ...) {
	if (!message)
		return;
	va_list argptr;
	char buffer[1024];

	va_start(argptr, message);
	vsnprintf(buffer, sizeof(buffer), message, argptr);
	va_end(argptr);

	SendChannelMessageRaw(from, to, chan_num, language, lang_skill, buffer);
}

void ZSList::SendChannelMessageRaw(const char* from, const char* to, uint8 chan_num, uint8 language, uint8 lang_skill, const char* message) {
	if (!message)
		return;
	auto pack = new ServerPacket;

	pack->opcode = ServerOP_ChannelMessage;
	pack->size = sizeof(ServerChannelMessage_Struct)+strlen(message)+1;
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerChannelMessage_Struct* scm = (ServerChannelMessage_Struct*) pack->pBuffer;
	if (from == 0) {
		strcpy(scm->from, "WServer");
	}
	else if (from[0] == 0) {
		strcpy(scm->from, "WServer");
	}
	else
		strcpy(scm->from, from);
	if (to != 0) {
		strcpy((char *) scm->to, to);
		strcpy((char *) scm->deliverto, to);
	}
	else {
		scm->to[0] = 0;
		scm->deliverto[0] = 0;
	}

	scm->language = language;
	scm->lang_skill = lang_skill;
	scm->chan_num = chan_num;
	strcpy(&scm->message[0], message);
	//if (scm->chan_num == ChatChannel_OOC || scm->chan_num == ChatChannel_Broadcast || scm->chan_num == ChatChannel_GMSAY) {
	//	console_list.SendChannelMessage(scm);
	//}
	SendPacket(pack);
	delete pack;
}

void ZSList::SendEmoteMessage(const char* to, uint32 to_guilddbid, int16 to_minstatus, uint32 type, const char* message, ...) {
	if (!message)
		return;
	va_list argptr;
	char buffer[1024];

	va_start(argptr, message);
	vsnprintf(buffer, sizeof(buffer), message, argptr);
	va_end(argptr);

	SendEmoteMessageRaw(to, to_guilddbid, to_minstatus, type, buffer);
}

void ZSList::SendEmoteMessageRaw(const char* to, uint32 to_guilddbid, int16 to_minstatus, uint32 type, const char* message) {
	if (!message)
		return;
	auto pack = new ServerPacket;

	pack->opcode = ServerOP_EmoteMessage;
	pack->size = sizeof(ServerEmoteMessage_Struct)+strlen(message)+1;
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerEmoteMessage_Struct* sem = (ServerEmoteMessage_Struct*) pack->pBuffer;

	if (to) {
		strcpy((char *) sem->to, to);
	}
	else {
		sem->to[0] = 0;
	}

	sem->guilddbid = to_guilddbid;
	sem->minstatus = to_minstatus;
	sem->type = type;
	strcpy(&sem->message[0], message);
	char tempto[64]={0};
	if(to)
		strn0cpy(tempto,to,64);
	if (tempto[0] == 0) {
		SendPacket(pack);
	}
	else {
		ZoneServer* zs = FindByName(to);

		if (zs != 0)
			zs->SendPacket(pack);
		else
			SendPacket(pack);
	}
	delete pack;
}

void ZSList::SendTimeSync() {
	auto pack = new ServerPacket(ServerOP_SyncWorldTime, sizeof(eqTimeOfDay));
	eqTimeOfDay* tod = (eqTimeOfDay*) pack->pBuffer;
	tod->start_eqtime=worldclock.getStartEQTime();
	tod->start_realtime=worldclock.getStartRealTime();
	SendPacket(pack);
	delete pack;
}

void ZSList::NextGroupIDs(uint32 &start, uint32 &end) {
	start = CurGroupID;
	CurGroupID += 1000;	//hand them out 1000 at a time...
	if(CurGroupID < start) {	//handle overflow
		start = 1;
		CurGroupID = 1001;
	}
	end = CurGroupID - 1;
}

void ZSList::SOPZoneBootup(const char* adminname, uint32 ZoneServerID, uint32 ZoneServerGuildID, const char* zonename, bool iMakeStatic) {
	ZoneServer* zs = 0;
	ZoneServer* zs2 = 0;
	uint32 zoneid;
	if (!(zoneid = ZoneID(zonename)))
		SendEmoteMessage(adminname, 0, AccountStatus::Player, Chat::White, fmt::format("Error: SOP_ZoneBootup: zone '{}' not found in 'zone' table.", zonename).c_str());
	else {
		if (ZoneServerID != 0)
			zs = FindByID(ZoneServerID);
		else
			SendEmoteMessage(adminname, 0, AccountStatus::Player, Chat::White, "Error: SOP_ZoneBootup: ServerID must be specified");

		if (!zs)
			SendEmoteMessage(adminname, 0, AccountStatus::Player, Chat::White, "Error: SOP_ZoneBootup: zoneserver not found");
		else {
			zs2 = FindByZoneID(zoneid, ZoneServerGuildID);
			if (zs2 != 0)
				SendEmoteMessage(adminname, 0, AccountStatus::Player, Chat::White, fmt::format("Error: SOP_ZoneBootup: zone '{}' already being hosted by ZoneServer ID {} ", zonename, zs2->GetID()).c_str());
			else {
				zs->TriggerBootup(zoneid, adminname, iMakeStatic, ZoneServerGuildID);
				SendEmoteMessage(adminname, 0, AccountStatus::Player, Chat::White, fmt::format("Booting {} on zoneserverid {} ", zonename, ZoneServerID).c_str());
			}
		}
	}
}

void ZSList::RebootZone(const char* ip1,uint16 port,const char* ip2, uint32 skipid, uint32 zoneid){
	// get random zone
	uint32 x = 0;
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		x++;
		iterator++;
	}
	if (x == 0)
		return;
	auto tmp = new ZoneServer *[x];
	uint32 y = 0;

	iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		if (!strcmp((*iterator)->GetCAddress(),ip2) && !(*iterator)->IsBootingUp() && (*iterator)->GetID() != skipid) {
			tmp[y++] = (*iterator).get();
		}
		iterator++;
	}
	if (y == 0) {
		safe_delete_array(tmp);
		return;
	}
	uint32 z = emu_random.Int(0, y-1);

	auto pack = new ServerPacket(ServerOP_ZoneReboot, sizeof(ServerZoneReboot_Struct));
	ServerZoneReboot_Struct* s = (ServerZoneReboot_Struct*) pack->pBuffer;
//	strcpy(s->ip1,ip1);
	strcpy(s->ip2,ip2);
	s->port = port;
	s->zoneid = zoneid;
	if(zoneid != 0)
		LogInfo("Rebooting static zone with the ID of: [{}]",zoneid);
	tmp[z]->SendPacket(pack);
	delete pack;
	safe_delete_array(tmp);
}

uint16 ZSList::GetAvailableZonePort()
{
	// Skip anything a registered zone server already uses; ReleaseZonePort puts it back when that
	// server goes away.
	while (!m_ports_free.empty()) {
		auto port = m_ports_free.front();
		m_ports_free.pop_front();
		if (!FindByPort(port)) {
			return port;
		}
		LogError("Dynamic zone port [{}] is already in use by a registered zone; skipping it", port);
	}

	return 0;
}

bool ZSList::IsDynamicZonePort(uint16 port) const
{
	const WorldConfig* Config = WorldConfig::get();
	return port != 0 && port >= Config->ZonePortLow && port <= Config->ZonePortHigh;
}

void ZSList::ClaimZonePort(uint16 port)
{
	m_ports_free.erase(std::remove(m_ports_free.begin(), m_ports_free.end(), port), m_ports_free.end());
}

void ZSList::ReleaseZonePort(uint16 port)
{
	if (!IsDynamicZonePort(port) || FindByPort(port)) {
		return;
	}

	if (std::find(m_ports_free.begin(), m_ports_free.end(), port) == m_ports_free.end()) {
		m_ports_free.push_back(port);
	}
}

uint32 ZSList::GetAvailableZoneID()
{
	//LinkedListIterator<ZoneServer*> iterator(list, BACKWARD);
	//iterator.Reset();

	ZoneServer* zone_server_data = 0;

	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		zone_server_data = (*iterator).get();

		if (zone_server_data->GetZoneID() || zone_server_data->IsBootingUp())
		{
			iterator++;
		}
		else
		{
			return zone_server_data->GetID();
		}
	}

	return 0;
}

uint32 ZSList::TriggerBootup(uint32 iZoneID, uint32 iGuildID) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
			if((*iterator)->GetZoneID() == iZoneID && (*iterator)->GetZoneGuildID() == iGuildID)
			{
				return (*iterator)->GetID();
			}
			iterator++;
		}

	iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		if ((*iterator)->GetZoneID() == 0 && !(*iterator)->IsBootingUp()) {
			ZoneServer* zone = (*iterator).get();
			zone->TriggerBootup(iZoneID, 0, false, iGuildID);
			return zone->GetID();
		}
		iterator++;
	}
	return 0;
}

void ZSList::SendLSZones(){
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		ZoneServer* zs = (*iterator).get();
		zs->LSBootUpdate(zs->GetZoneID(),true);
		iterator++;
	}
}

int ZSList::GetZoneCount() {
	return(zone_server_list.size());
}

void ZSList::GetZoneIDList(std::vector<uint32> &zones) {
	auto iterator = zone_server_list.begin();
	while (iterator != zone_server_list.end()) {
		ZoneServer* zs = (*iterator).get();
		zones.push_back(zs->GetID());
		iterator++;
	}
}

void ZSList::WorldShutDown(uint32 time, uint32 interval)
{
	if( time > 0 ) {
		SendEmoteMessage(0, 0, AccountStatus::Player, Chat::System, fmt::format("[SYSTEM] World will be shutting down in {} minutes.", (time / 60)).c_str());

		time *= 1000;
		interval *= 1000;
		if(interval < 5000) { interval = 5000; }

		shutdowntimer->Start(time);
		reminder->Start(interval - 1000);
		reminder->SetDuration(interval);
	}
	else {
		SendEmoteMessage(0, 0, AccountStatus::Player, Chat::Yellow,"[SYSTEM] World is shutting down.");
		auto pack = new ServerPacket;
		pack->opcode = ServerOP_ShutdownAll;
		pack->size=0;
		SendPacket(pack);
		safe_delete(pack);
		Process();
		CatchSignal(2);
	}
}

void ZSList::DropClient(uint32 lsid, ZoneServer* ignore_zoneserver) {
	ServerPacket packet(ServerOP_DropClient, sizeof(ServerZoneDropClient_Struct));
	auto drop = (ServerZoneDropClient_Struct*)packet.pBuffer;
	drop->lsid = lsid;
	for (auto& zs : zone_server_list) {
		if (zs.get() != ignore_zoneserver) {
			zs->SendPacket(&packet);
		}
	}
}

void ZSList::OnTick(EQ::Timer* t)
{
	if (!EventSubscriptionWatcher::Get()->IsSubscribed("EQW::ZoneUpdate")) {
		return;
	}

	Json::Value out;
	out["event"] = "EQW::ZoneUpdate";
	out["data"] = Json::Value();
	for (auto& zone : zone_server_list)
	{
		Json::Value outzone;

		outzone["CAddress"] = zone->GetCAddress();
		outzone["CLocalAddress"] = zone->GetCLocalAddress();
		outzone["CompileTime"] = zone->GetCompileTime();
		outzone["CPort"] = zone->GetCPort();
		outzone["ID"] = zone->GetID();
		outzone["IP"] = zone->GetIP();
		outzone["LaunchedName"] = zone->GetLaunchedName();
		outzone["LaunchName"] = zone->GetLaunchName();
		outzone["Port"] = zone->GetPort();
		outzone["PrevZoneID"] = zone->GetPrevZoneID();
		outzone["UUID"] = zone->GetUUID();
		outzone["ZoneID"] = zone->GetZoneID();
		outzone["ZoneLongName"] = zone->GetZoneLongName();
		outzone["ZoneName"] = zone->GetZoneName();
		outzone["ZoneOSProcessID"] = zone->GetZoneOSProcessID();
		outzone["NumPlayers"] = zone->NumPlayers();
		outzone["BootingUp"] = zone->IsBootingUp();
		outzone["StaticZone"] = zone->IsStaticZone();
		out["data"].append(outzone);
	}
	web_interface.SendEvent(out);
}

const std::list<std::unique_ptr<ZoneServer>> &ZSList::getZoneServerList() const
{
	return zone_server_list;
}

void ZSList::UpdateUCSServerAvailable(bool ucss_available) {
	UCSServerAvailable_ = ucss_available;
	auto outapp = new ServerPacket(ServerOP_UCSServerStatusReply, sizeof(UCSServerStatus_Struct));
	auto ucsss = (UCSServerStatus_Struct*)outapp->pBuffer;
	ucsss->available = (ucss_available ? 1 : 0);
	ucsss->timestamp = Timer::GetCurrentTime();
	SendPacket(outapp);
	safe_delete(outapp);

	if (ucss_available) {
		SendEmoteMessage(0, 0, AccountStatus::Player, Chat::ChatChannel, "The Universal Chat service has been restored.  You must zone to re-join channels.");
	}
	else {
		SendEmoteMessage(0, 0, AccountStatus::Player, Chat::ChatChannel, "The Universal Chat service is temporarily unavailable. You will be notified when it is restored.");
	}
}
