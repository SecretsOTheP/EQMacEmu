/* EQEMu: Everquest Server Emulator
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
#ifndef CLIENT_H
#define CLIENT_H

class Client;
class EQApplicationPacket;
class EQStream;
class Group;
class Mob;
class NPC;
class Object;
class Raid;
class Seperator;
class ServerPacket;
struct ItemData;

#include "../common/timer.h"
#include "../common/ptimer.h"
#include "../common/emu_opcodes.h"
#include "../common/eq_packet_structs.h"
#include "../common/emu_constants.h" // inv2 watch
#include "../common/eq_stream_intf.h"
#include "../common/eq_packet.h"
#include "../common/linked_list.h"
#include "../common/extprofile.h"
#include "../common/races.h"
#include "../common/seperator.h"
#include "../common/inventory_profile.h"
#include "../common/guilds.h"
#include "../common/item_data.h"
#include "../common/data_verification.h"
#include "../common/zone_store.h"

#include "aa.h"
#include "common.h"
#include "mob.h"
#include "qglobals.h"
#include "questmgr.h"
#include "zone.h"
#include "zonedb.h"

#ifdef _WINDOWS
	// since windows defines these within windef.h (which windows.h include)
	// we are required to undefine these to use min and max from <algorithm>
	#undef min
	#undef max
#endif

#include <float.h>
#include <set>
#include <algorithm>
#include <chrono>
#include <unordered_map>


#define CLIENT_TIMEOUT		90000
#define CLIENT_LD_TIMEOUT	30000 // length of time client stays in zone after LDing
#define TARGETING_RANGE		200	// range for /target
#define ASSIST_RANGE		250 // range for /assist
#define MAX_SPECIALIZED_SKILL 50

#define BUFFSTACKING_PATCH_V2 2 // Buff Stacking Patch + 6 Buff Song Window (Requires New UI)
#define BUFFSTACKING_PATCH_V1 1 // Buff Stacking Patch only (Old UI)

constexpr int SONG_WINDOW_BUFF_SLOTS = 6; // Buffstacking patch enables song window support
constexpr int CLIENT_MAX_BUFF_SLOTS = (15 + SONG_WINDOW_BUFF_SLOTS);
constexpr int CLIENT_TOTAL_BUFF_SLOTS = (15 + SONG_WINDOW_BUFF_SLOTS + 1); // add 1 for disc slot

extern Zone* zone;

class CLIENTPACKET
{
public:
	CLIENTPACKET();
	~CLIENTPACKET();
	EQApplicationPacket *app;
	bool ack_req;
};

struct BaseStatsStruct {
	uint16 STR;
	uint16 STA;
	uint16 AGI;
	uint16 DEX;
	uint16 WIS;
	uint16 INT;
	uint16 CHA;
	uint16 unspent;
	void Add(const BaseStatsStruct& other) {
		this->STR += other.STR;
		this->STA += other.STA;
		this->AGI += other.AGI;
		this->DEX += other.DEX;
		this->WIS += other.WIS;
		this->INT += other.INT;
		this->CHA += other.CHA;
		this->unspent += other.unspent;
	}
};

// Base Stats for each Race for PermaStats
static const std::unordered_map<uint16, BaseStatsStruct> race_base_stats = {
	//                  str sta agi dex wis  int cha
	{ Race::Human,     { 75, 75, 75, 75, 75,  75, 75}},
	{ Race::Barbarian, {103, 95, 82, 70, 70,  60, 55}},
	{ Race::Erudite,   { 60, 70, 70, 70, 83, 107, 70}},
	{ Race::WoodElf,   { 65, 65, 95, 80, 80,  75, 75}},
	{ Race::HighElf,   { 55, 65, 85, 70, 95,  92, 80}},
	{ Race::DarkElf,   { 60, 65, 90, 75, 83,  99, 60}},
	{ Race::HalfElf,   { 70, 70, 90, 85, 60,  75, 75}},
	{ Race::Dwarf,     { 90, 90, 70, 90, 83,  60, 45}},
	{ Race::Troll,     {108,109, 83, 75, 60,  52, 40}},
	{ Race::Ogre,      {130,122, 70, 70, 67,  60, 37}},
	{ Race::Halfling,  { 70, 75, 95, 90, 80,  67, 50}},
	{ Race::Gnome,     { 60, 70, 85, 85, 67,  98, 60}},
	{ Race::Iksar,     { 70, 70, 90, 85, 80,  75, 55}},
	{ Race::VahShir,   { 90, 75, 90, 70, 70,  65, 65}}
};

// Base stat allocation by class
static const std::unordered_map<uint16, BaseStatsStruct> class_bonus_stats = {
	//                    str sta agi dex wis int cha unspent
	{0,                  {  0,  0,  0,  0,  0,  0,  0, 50}},
	{Class::Warrior,     { 10, 10,  5,  0,  0,  0,  0, 25}},
	{Class::Cleric,      {  5,  5,  0,  0, 10,  0,  0, 30}},
	{Class::Paladin,     { 10,  5,  0,  0,  5,  0, 10, 20}},
	{Class::Ranger,      {  5, 10, 10,  0,  5,  0,  0, 20}},
	{Class::ShadowKnight,{ 10,  5,  0,  0,  0, 10,  5, 20}},
	{Class::Druid,       {  0, 10,  0,  0, 10,  0,  0, 30}},
	{Class::Monk,        {  5,  5, 10, 10,  0,  0,  0, 20}},
	{Class::Bard,        {  5,  0,  0, 10,  0,  0, 10, 25}},
	{Class::Rogue,       {  0,  0, 10, 10,  0,  0,  0, 30}},
	{Class::Shaman,      {  0,  5,  0,  0, 10,  0,  5, 30}},
	{Class::Necromancer, {  0,  0,  0, 10,  0, 10,  0, 30}},
	{Class::Wizard,      {  0, 10,  0,  0,  0, 10,  0, 30}},
	{Class::Magician,    {  0, 10,  0,  0,  0, 10,  0, 30}},
	{Class::Enchanter,   {  0,  0,  0,  0,  0, 10, 10, 30}},
	{Class::Beastlord,   {  0, 10,  5,  0, 10,  0,  5, 20}}
};

// Default startzone we can assign for someone when we don't know their default startzone
static const std::unordered_map<uint16, BindStruct> default_race_bind = {
	//            zone      x          y     z     heading
	{ HUMAN,     {  22,   -1485,      9.2,    0,       0}},
	{ ERUDITE,   {  38,     203,     2295,    0,       0}},
	{ BARBARIAN, {  30,     629,     3139,    0,       0}},
	{ HALFLING,  {  33,       0,        0,    0,       0}},
	{ DWARF,     {  68,    -700,     2550,    0,       0}},
	{ GNOME,     {  56, -272.86,   159.86,    0,       0}},
	{ HALF_ELF,  {  54,      10,      -20,    0,       0}},
	{ WOOD_ELF,  {  54,      10,      -20,    0,       0}},
	{ HIGH_ELF,  {  54,      10,      -20,    0,       0}},
	{ DARK_ELF,  {  25,    -279,    -1201,    0,       0}},
	{ OGRE,      {  47,     905,     1051,    0,       0}},
	{ TROLL,     {  46,    -588,    -2192,    0,       0}},
	{ IKSAR,     {  78,    1617,    -1684,    0,       0}},
	{ VAHSHIR,   { 165,   -3570,    -2122,    0,       0}}
};

enum { //Type arguments to the Message* routines.
	//all not explicitly listed are the same grey color
	clientMessageWhite0 = 0,
	clientMessageLoot = 2, //dark green
	clientMessageTradeskill = 4, //light blue
	clientMessageTell = 5, //magenta
	clientMessageWhite = 7,
	clientMessageWhite2 = 10,
	clientMessageLightGrey = 12,
	clientMessageError = 13, //red
	clientMessageGreen = 14,
	clientMessageYellow = 15,
	clientMessageBlue = 16,
	clientMessageGroup = 18, //cyan
	clientMessageWhite3 = 20,
};

enum { //scribing argument to MemorizeSpell
	memSpellScribing = 0,
	memSpellMemorize = 1,
	memSpellForget = 2,
	memSpellSpellbar = 3
};

//Modes for the zoning state of the client.
typedef enum {
	ZoneToSafeCoords, // Always send ZonePlayerToBind_Struct to client: Succor/Evac
	GMSummon, // Always send ZonePlayerToBind_Struct to client: Only a GM Summon
	ZoneToBindPoint, // Always send ZonePlayerToBind_Struct to client: Death Only
	ZoneSolicited, // Always send ZonePlayerToBind_Struct to client: Portal, Translocate, Evac spells that have a x y z coord in the spell data
	ZoneUnsolicited,
	GateToBindPoint, // Always send RequestClientZoneChange_Struct to client: Gate spell or Translocate To Bind Point spell
	SummonPC, // In-zone GMMove() always: Call of the Hero spell or some other type of in zone only summons
	Rewind, // Summon to /rewind location.
	EvacToSafeCoords,
	ForceZoneToBindPoint,
	ZoneToGuildZone
} ZoneMode;

typedef enum {
	MQWarp,
	MQWarpShadowStep,
	MQWarpKnockBack,
	MQWarpLight,
	MQZone,
	MQZoneUnknownDest,
	MQGate,
	MQGhost
} CheatTypes;

enum {
	HideCorpseNone = 0,
	HideCorpseAll = 1,
	HideCorpseAllButGroup = 2,
	HideCorpseLooted = 3,
	HideCorpseNPC = 5
};

typedef enum {
	Killed_Other,
	Killed_NPC,
	Killed_ENV,
	Killed_DUEL,
	Killed_PVP,
	Killed_Sac,
	Killed_Client, // The client killed us, NOT another player.
	Killed_Self
} KilledByTypes;

struct RespawnOption
{
	std::string name;
	uint32 zone_id;
	float x;
	float y;
	float z;
	float heading;
};


const uint32 POPUPID_UPDATE_SHOWSTATSWINDOW = 1000000;

class Client : public Mob
{
public:
	//pull in opcode mappings:
	#include "client_packet.h"

	Client(EQStreamInterface * ieqs);
	~Client();

	bool is_client_moving;
	
	DBGrid_Struct* gm_grid;
	std::vector<wplist> gm_grid_waypoint_list;
	
	bool IsDevToolsEnabled() const;
	void SetDevToolsEnabled(bool in_dev_tools_enabled);

	void SendChatLineBreak(uint16 color = Chat::White);

	//abstract virtual function implementations required by base abstract class
	virtual bool Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby = 0, bool bufftic = false);
	virtual void Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false);
	virtual bool Attack(Mob* other, int hand = EQ::invslot::slotPrimary, int damagePct = 100);
	virtual bool HasRaid() { return (GetRaid() ? true : false); }
	virtual bool HasGroup() { return (GetGroup() ? true : false); }
	virtual Raid* GetRaid() { return entity_list.GetRaidByClient(this); }
	virtual Group* GetGroup() { return entity_list.GetGroupByClient(this); }
	virtual inline bool IsBerserk() { return berserk; }
	virtual void SetAttackTimer(bool trigger = false);
	virtual bool CombatRange(Mob* other, float dist_squared = 0.0f, bool check_z = false, bool pseudo_pos = false);
	virtual uint8 Disarm(float chance);
	bool ClientMoving() { return (m_Delta.x != 0.0f || m_Delta.y != 0.0f); }
	int GetBaseDamage(Mob* defender = nullptr, int hand = EQ::invslot::slotPrimary);		// weapon damage for clients; DI*10 for NPCs
	int GetDamageBonus();
	int GetHandToHandDamage();
	int GetHandToHandDelay();
	uint16 GetWeaponEffectID(int slot = EQ::invslot::slotPrimary);
	
	void PermaGender(uint32 gender);

	float GetQuiverHaste();
	int	GetHasteCap();

	bool	CheckDeath();

	void	AI_Init();
	void	AI_Start();
	void	AI_Stop();
	void	AI_Process();
	void	AI_SpellCast();
	void	Trader_ShowItems();
	void	Trader_CustomerBrowsing(Client *Customer);
	void	Trader_CustomerBought(Client *Customer, uint32 Price, uint32 ItemID, uint32 Quantity, const char* ItemName, uint8 SlotID);
	void	Trader_EndTrader();
	void	Trader_StartTrader();
	void	KeyRingLoad();
	void	KeyRingAdd(uint32 item_id);
	bool	KeyRingCheck(uint32 item_id);
	void	KeyRingList(Client* notifier);
	bool	CheckKeyRingStage(uint16 item_id);
	std::string	GetMaxKeyRingStage(uint16 item_id, bool use_current_zone = true);
	KeyRing_Data_Struct* GetKeyRing(uint16 keyitem, uint32 zoneid);
	virtual bool IsClient() const { return true; }
	void	CompleteConnect();
	bool	TryStacking(EQ::ItemInstance* item, uint8 type = ItemPacketTrade, bool try_worn = true, bool try_cursor = true);
	int16	GetStackSlot(EQ::ItemInstance* item, bool try_worn = true, bool try_cursor = true);
	void	SendTraderPacket(Client* trader);
	GetItems_Struct* GetTraderItems(bool skipstackable = false);
	GetItem_Struct GrabItem(uint16 item_id);
	GetItem_Struct GrabStackedItem(uint16 item_id);
	int16	GrabStackedSlot(uint16 item_id, uint8 charges);
	uint16	GrabStackedCharges(uint16 item_id);
	void	SendBazaarWelcome();
	void	Message_StringID(uint32 type, uint32 string_id, uint32 distance = 0);
	void	Message_StringID(uint32 type, uint32 string_id, const char* message,const char* message2=0,const char* message3=0,const char* message4=0,const char* message5=0,const char* message6=0,const char* message7=0,const char* message8=0,const char* message9=0, uint32 distance = 0);
	bool	FilteredMessageCheck(Mob *sender, eqFilterType filter);
	void	FilteredMessage_StringID(Mob *sender, uint32 type, eqFilterType filter, uint32 string_id);
	void	FilteredMessage_StringID(Mob *sender, uint32 type, eqFilterType filter,
			uint32 string_id, const char *message1, const char *message2 = nullptr,
			const char *message3 = nullptr, const char *message4 = nullptr,
			const char *message5 = nullptr, const char *message6 = nullptr,
			const char *message7 = nullptr, const char *message8 = nullptr,
			const char *message9 = nullptr);
	void Tell_StringID(uint32 string_id, const char *who, const char *message);
	void	SendBazaarResults(uint32 trader_id,uint32 class_,uint32 race,uint32 stat,uint32 slot,uint32 type,char name[64],uint32 minprice,uint32 maxprice);
	void	SendTraderItem(uint32 item_id,uint16 quantity);
	EQ::ItemInstance* FindTraderItemByIDAndSlot(int32 ItemID, int16 slotid);
	void	FindAndNukeTraderItem(int32 item_id,uint16 quantity,Client* customer,uint16 slot, int8 traderslot);
	void	NukeTraderItem(uint16 slot, int16 charges,uint16 quantity,Client* customer,uint16 traderslot,uint32 sellerid);
	void	ReturnTraderReq(const EQApplicationPacket* app,int16 traderitemcharges, int TraderSlot,uint32 price);
	void	TradeRequestFailed(const EQApplicationPacket* app);
	void	BuyTraderItem(TraderBuy_Struct* tbs,Client* trader,const EQApplicationPacket* app);
	void	FinishTrade(Mob* with, bool finalizer = false, void* event_entry = nullptr);
	void	SendZonePoints();

	void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho);
	virtual bool Process();
	void LogMerchant(Client* player, Mob* merchant, uint32 quantity, uint32 price, const EQ::ItemData* item, bool buying);
	void QueuePacket(const EQApplicationPacket* app, bool ack_req = true, CLIENT_CONN_STATUS = CLIENT_CONNECTINGALL, eqFilterType filter=FilterNone);
	void FastQueuePacket(EQApplicationPacket** app, bool ack_req = true, CLIENT_CONN_STATUS = CLIENT_CONNECTINGALL);
	void ChannelMessageReceived(uint8 chan_num, uint8 language, uint8 lang_skill, const char* orig_message, const char* targetname=nullptr);
	void ChannelMessageSend(const char* from, const char* to, uint8 chan_num, uint8 language, uint8 lang_skill, const char* message, ...);
	void Message(uint32 type, const char* message, ...);
	void SendSound(uint16 soundID);
	bool CanIncreaseTradeskill(EQ::skills::SkillType tradeskill);

	int8			GetRevoked() const { return revoked; }
	void			SetRevoked(int8 rev) { revoked = rev; }
	inline uint32	GetIP()			const { return ip; }
	inline bool		GetHideMe()			const { return gmhideme; }
	void			SetHideMe(bool hm);
	inline uint16	GetPort()		const { return port; }
	bool			IsDead() const { return(dead); }
	bool			IsUnconscious() const { return ((cur_hp <= 0 && cur_hp > -10 && !dead) ? true : false); }
	inline bool		GetRunMode() const { return runmode; }

	virtual bool Save() { return Save(0); }
	bool Save(uint8 iCommitNow); // 0 = delayed, 1=async now, 2=sync now

	/* New PP Save Functions */
	bool SaveCurrency(){ return database.SaveCharacterCurrency(this->CharacterID(), &m_pp); }
	bool SaveAA();

	inline bool ClientDataLoaded() const { return client_data_loaded; }
	inline bool Connected() const { return (client_state == CLIENT_CONNECTED); }
	inline bool InZone() const { return (client_state == CLIENT_CONNECTED || client_state == CLIENT_LINKDEAD); }
	inline void Kick() { client_state = CLIENT_KICKED; }
	inline void Disconnect() { eqs->Close(); client_state = DISCONNECTED; }
	inline void HardDisconnect() { eqs->Close(); client_state = DISCONNECTED; }
	inline void SetZoningState() { client_state = ZONING; }
	inline void	PreDisconnect() { client_state = PREDISCONNECTED; }
	inline void	Reconnect() { client_state = CLIENT_CONNECTED; }
	inline bool IsLD() const { return (bool) (client_state == CLIENT_LINKDEAD); }
	void Kick(const std::string& reason);
	void WorldKick();
	inline uint8 GetAnon() const { return m_pp.anon; }
	inline PlayerProfile_Struct& GetPP() { return m_pp; }
	inline ExtendedProfile_Struct& GetEPP() { return m_epp; }
	inline EQ::InventoryProfile& GetInv() { return m_inv; }
	inline const EQ::InventoryProfile& GetInv() const { return m_inv; }
	inline PetInfo* GetPetInfo(uint16 pet) { return (pet==1)?&m_suspendedminion:&m_petinfo; }

	bool CheckAccess(int16 iDBLevel, int16 iDefaultLevel);

	bool	AutoAttackEnabled() const { return auto_attack; }

	bool ChangeFirstName(const char* in_firstname,const char* gmname);

	void Duck();
	void Stand();

	virtual void SetMaxHP();
	static int32 LevelRegen(int level, bool is_sitting, bool is_resting, bool is_feigned, bool is_famished, bool has_racial_regen_bonus);
	void SetGM(bool toggle);
	void SetPVP(uint8 toggle);
	void	SetAnon(bool toogle);

	inline uint8 GetPVP() const { return m_pp.pvp; }
	inline bool GetGM() const { return m_pp.gm != 0; }

	void SetBaseClass(uint32 i, bool fix_skills = true, bool unscribe_spells = true);
	void SetBaseRace(uint32 i, bool fix_skills = true);
	inline void SetBaseGender(uint32 i) { m_pp.gender=i; }
	inline void SetDeity(uint32 i) {m_pp.deity=i;deity=i;}

	// Calculate the new base stats for a given race/class combination and the specified bonus point allocation.
	// Returns true if valid, and sets the 'out' parameter with the final base stats.
	static bool CalculateBaseStats(
		Client* error_listener,
		uint16 in_class, uint16 in_race,
		uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA,
		BaseStatsStruct& out
	);

	// Lookup a race/class/deity/city combination.
	// Only returns true if the combination is valid and available for the current player and expansion.
	bool GetCharacterCreateCombination(
		Client* error_listener,
		uint16 in_class, uint16 in_race, uint16 in_deity, int in_player_choice_city,
		BindStruct& out_start_zone_bind
	);

	bool HasCharacterCreateCombination(uint16 in_class, uint16 in_race, uint16 in_deity, int in_player_choice_city) {
		BindStruct ignore;
		return GetCharacterCreateCombination((Client*)nullptr, in_class, in_race, in_deity, in_player_choice_city, ignore);
	}

	// Unlocks a new character combination for this player by their character_id.
	void AddCharacterCreateCombinationUnlock(
		uint16 in_class, uint16 in_race, uint16 in_deity, int player_home_choice,
		uint16 home_zone_id, float bind_x, float bind_y, float bind_z, float bind_heading);

	// Changes the starting stats for the character. Returns false if invalid.
	bool PermaStats(
		Client* error_listener,
		uint16 bonusSTR, uint16 bonusSTA, uint16 bonusAGI, uint16 bonusDEX, uint16 bonusWIS, uint16 bonusINT, uint16 bonusCHA,
		bool check_cooldown = true
	);

	// Changes the starting race/deity/city combination for the character, along with the given stats. Returns false if invalid.
	// Setting all bonus values to >=0xFF will default to using the character's current point distribution from their old race.
	bool PermaRace(
		Client* error_listener,
		uint16 in_race, uint16 in_deity, int player_choice_city,
		uint16 bonusSTR = 0xFF, uint16 bonusSTA = 0xFF, uint16 bonusAGI = 0xFF, uint16 bonusDEX = 0xFF, uint16 bonusWIS = 0xFF, uint16 bonusINT = 0xFF, uint16 bonusCHA = 0xFF,
		bool force = false) {
		return PermaRaceClass(error_listener, GetBaseClass(), in_race, in_deity, player_choice_city, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, force);
	}

	bool PermaClass(
		Client* error_listener,
		uint16 in_class, uint16 in_deity, int player_choice_city,
		uint16 bonusSTR = 0xFF, uint16 bonusSTA = 0xFF, uint16 bonusAGI = 0xFF, uint16 bonusDEX = 0xFF, uint16 bonusWIS = 0xFF, uint16 bonusINT = 0xFF, uint16 bonusCHA = 0xFF,
		bool force = false) {
		return PermaRaceClass(error_listener, in_class, GetBaseRace(), in_deity, player_choice_city, bonusSTR, bonusSTA, bonusAGI, bonusDEX, bonusWIS, bonusINT, bonusCHA, force);
	}

	bool PermaRaceClass(
		Client* error_listener,
		uint16 in_class, uint16 in_race, uint16 in_deity, int player_choice_city,
		uint16 bonusSTR = 0xFF, uint16 bonusSTA = 0xFF, uint16 bonusAGI = 0xFF, uint16 bonusDEX = 0xFF, uint16 bonusWIS = 0xFF, uint16 bonusINT = 0xFF, uint16 bonusCHA = 0xFF,
		bool force = false
	);

	inline uint8 GetLevel2() const { return m_pp.level2; }
	inline uint16 GetBaseRace() const { return m_pp.race; }
	inline uint16 GetBaseClass() const { return m_pp.class_; }
	inline uint8 GetBaseGender() const { return m_pp.gender; }
	inline uint8 GetBaseFace() const { return m_pp.face; }
	inline uint8 GetBaseHairColor() const { return m_pp.haircolor; }
	inline uint8 GetBaseBeardColor() const { return m_pp.beardcolor; }
	inline uint8 GetBaseEyeColor() const { return m_pp.eyecolor1; }
	inline uint8 GetBaseHairStyle() const { return m_pp.hairstyle; }
	inline uint8 GetBaseBeard() const { return m_pp.beard; }
	inline const float GetBindX(uint32 index = 0) const { return m_pp.binds[index].x; }
	inline const float GetBindY(uint32 index = 0) const { return m_pp.binds[index].y; }
	inline const float GetBindZ(uint32 index = 0) const { return m_pp.binds[index].z; }
	inline const float GetBindHeading(uint32 index = 0) const { return m_pp.binds[index].heading; }
	inline uint32 GetBindZoneID(uint32 index = 0) const { return m_pp.binds[index].zoneId; }
	inline uint32 GetZoneChangeCount() const { return m_pp.zone_change_count; }
	inline uint32 GetTimesRebirthed() { return m_epp.e_times_rebirthed; }
	
	inline uint8 IsHardcore() const { return m_epp.hardcore; }
	inline uint8 IsSoloOnly() const { return m_epp.solo_only; }
	inline uint8 IsSelfFound() const { return m_epp.self_found; }
	inline uint8 HasBetaBuffGearFlag() const { return m_epp.betabuff_gear_flag; }
	std::string GetSSFLooterName();

	inline void SetHardcore(uint8 in_hardcore) { m_epp.hardcore = in_hardcore; }
	inline void SetSoloOnly(uint8 in_solo_only) { m_epp.solo_only = in_solo_only; }
	inline void SetSelfFound(uint8 in_self_found) { m_epp.self_found = in_self_found; }
	inline void SetHardcoreDeathTimeStamp(uint32 in_death_timestamp) { m_epp.hardcore_death_time = in_death_timestamp; Save(1); }
	inline void SetBetaBuffGearFlag(uint8 gearflag) { m_epp.betabuff_gear_flag = gearflag; }

	int32 CalcMaxMana();
	int32 CalcBaseMana();
	const int32& SetMana(int32 amount);
	int32 CalcManaRegenCap();

	void ServerFilter(SetServerFilter_Struct* filter);
	void BulkSendTraderInventory(uint32 char_id);
	void DisplayTraderInventory(Client* client);
	void SendSingleTraderItem(uint32 char_id, int16 islotid, uint16 slotid);
	void BulkSendMerchantInventory(int merchant_id, int npcid);
	void MerchantWelcome(int merchant_id, int npcid);
	void UpdateTraderCustomerItemsAdded(TraderCharges_Struct* gis, uint32 ItemID);
	void UpdateTraderCustomerPriceChanged(TraderCharges_Struct* gis, uint32 ItemID, int32 Charges, uint32 NewPrice);

	inline uint8 GetLanguageSkill(uint16 n) const { return m_pp.languages[n]; }

	void SendPickPocketResponse(Mob *from, uint32 amt, int type, int16 slotid = 0, EQ::ItemInstance* inst = nullptr, bool skipskill = false);
	bool GetPickPocketSlot(EQ::ItemInstance* inst, int16& slotid);

	inline const char* GetLastName() const { return lastname; } // Gets their current full last name (includes modifications by Title NPC).
	inline const char* GetProfileLastName() const { return m_pp.last_name; } // Gets just their raw /surname value (ignores their modified surname by the Title NPC).

	inline const char* GetTemporaryLastName() const { return m_epp.temp_last_name; }
	bool HasTemporaryLastName();

	uint32 GetTemporaryMarriageCharacterID() { return pending_marriage_character_id; }
	void SetTemporaryMarriageCharacterID(uint32 pending_id) { pending_marriage_character_id = pending_id; }
	void SetMarried(const char* playerName);
	bool IsMarried();

	inline uint8 GetCharExportFlag() { return m_epp.char_export_flag; };

	void SetCharExportFlag(uint8 flag);

	inline float ProximityX() const { return m_Proximity.x; }
	inline float ProximityY() const { return m_Proximity.y; }
	inline float ProximityZ() const { return m_Proximity.z; }
	inline void ClearAllProximities() { entity_list.ProcessMove(this, glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX)); m_Proximity = glm::vec3(FLT_MAX,FLT_MAX,FLT_MAX); }

	void CheckVirtualZoneLines();

	/*
			Begin client modifiers
	*/








	virtual void CalcBonuses();
	inline virtual int32 GetAC() const { return AC; }			// this returns the value displayed in the client and is not used in calcs
	int GetOffense(EQ::skills::SkillType skill);
	uint32 RollDamageMultiplier(uint32 offense, int& damage, EQ::skills::SkillType skill);
	int GetMitigation(bool ignoreCap);
	int GetMitigation() { return GetMitigation(false); }
	static int GetMitigation(bool ignoreCap, int item_ac_sum, int shield_ac, int spell_ac_sum, int classnum, int level, int base_race, int carried_weight, int agi, int defense_skill_value, int combat_stability_percent);
	int GetAvoidance(bool ignoreCombatAgility);
	int GetAvoidance() { return GetAvoidance(false); }
	static int GetAvoidance(int16 defense_skill_value, int16 agi, uint8 level, uint8 intoxication, int combat_agility_percent);
	inline virtual int GetHaste() const { return Haste; }
	int GetRawACNoShield(int &shield_ac, int spell_mod = 1) const;

	inline virtual int32 GetSTR() const { return STR; }
	inline virtual int32 GetSTA() const { return STA; }
	inline virtual int32 GetDEX() const { return DEX; }
	inline virtual int32 GetAGI() const { return AGI; }
	inline virtual int32 GetINT() const { return INT; }
	inline virtual int32 GetWIS() const { return WIS; }
	inline virtual int32 GetCHA() const { return CHA; }
	inline virtual int32 GetMR() const { return MR; }
	inline virtual int32 GetFR() const { return FR; }
	inline virtual int32 GetDR() const { return DR; }
	inline virtual int32 GetPR() const { return PR; }
	inline virtual int32 GetCR() const { return CR; }

	int32 GetMaxStat(int32 aabonusAmount) const;
	int32 GetMaxResist() const;
	int32 GetMaxSTR() const;
	int32 GetMaxSTA() const;
	int32 GetMaxDEX() const;
	int32 GetMaxAGI() const;
	int32 GetMaxINT() const;
	int32 GetMaxWIS() const;
	int32 GetMaxCHA() const;
	int32 GetMaxMR() const;
	int32 GetMaxPR() const;
	int32 GetMaxDR() const;
	int32 GetMaxCR() const;
	int32 GetMaxFR() const;
	inline int16 GetBaseSTR() const { return m_pp.STR; }
	inline int16 GetBaseSTA() const { return m_pp.STA; }
	inline int16 GetBaseCHA() const { return m_pp.CHA; }
	inline int16 GetBaseDEX() const { return m_pp.DEX; }
	inline int16 GetBaseINT() const { return m_pp.INT; }
	inline int16 GetBaseAGI() const { return m_pp.AGI; }
	inline int16 GetBaseWIS() const { return m_pp.WIS; }
	inline uint8 GetBaseCorrup() const { return 15; } // Same for all

	// Mod2
	inline virtual int32 GetShielding() const { return itembonuses.MeleeMitigation; }
	inline virtual int32 GetSpellShield() const { return itembonuses.SpellShield; }
	inline virtual int32 GetDoTShield() const { return itembonuses.DoTShielding; }
	inline virtual int32 GetStunResist() const { return itembonuses.StunResist; }
	inline virtual int32 GetStrikeThrough() const { return itembonuses.StrikeThrough; }
	inline virtual int32 GetAccuracy() const { return itembonuses.HitChance; }
	inline virtual int32 GetDS() const { return itembonuses.DamageShield; }
	// Mod3
	inline virtual int32 GetSingMod() const { return itembonuses.singingMod; }
	inline virtual int32 GetBrassMod() const { return itembonuses.brassMod; }
	inline virtual int32 GetPercMod() const { return itembonuses.percussionMod; }
	inline virtual int32 GetStringMod() const { return itembonuses.stringedMod; }
	inline virtual int32 GetWindMod() const { return itembonuses.windMod; }

	float GetActSpellRange(uint16 spell_id, float range, std::string& item_name);
	float GetSpellRange(uint16 spell_id, float range);
	int32 GetActSpellDamage(uint16 spell_id, int32 dmg, Mob* target = nullptr);
	int32 GetActSpellHealing(uint16 spell_id, int32 value, Mob* target = nullptr, bool hot = false);
	int32 GetActSpellCost(uint16 spell_id, int32);
	int32 GetActSpellDuration(uint16 spell_id, int32);
	int32 GetAACastingTimeModifier(uint16 spell_id, int32 casttime);
	int32 GetActSpellCasttime(uint16 spell_id, int32);
	int32 GetActDoTDamage(uint16 spell_id, int32 value, Mob* target = nullptr);
	int32 TryWizardInnateCrit(uint16 spell_id, int32 damage, int32 focusDmg, int32 maxHit = 0);
	virtual bool CheckFizzle(uint16 spell_id);
	virtual bool CheckSpellLevelRestriction(uint16 spell_id, Mob* caster, EQ::spells::CastingSlot slot);
	virtual int GetCurBuffSlots() const { return 15 + m_song_window_slots; } // Dynamic count based on whether they have song window support detected.
	virtual int GetMaxBuffSlots() const { return CLIENT_MAX_BUFF_SLOTS; } // Total size of the spells + songs window.
	virtual int GetMaxTotalSlots() const { return CLIENT_TOTAL_BUFF_SLOTS; } // Total size of spells + songs + hidden slot for Disciplines.
	int GetDiscBuffSlot() { return CLIENT_MAX_BUFF_SLOTS; } // Always hide the disc buff after the last spell/song slot.
	virtual void InitializeBuffSlots();
	virtual void UninitializeBuffSlots();
	void ApplyDurationFocus(uint16 spell_id, uint16 buffslot, Mob* spelltar = nullptr, int spell_level=-1);
	bool RestictedManastoneClick(int16 zone_id);

	inline const int32 GetBaseHP() const { return base_hp; }

	int32 GetWeight() const { return(weight); }
	inline void RecalcWeight() { weight = CalcCurrentWeight(); }
	uint32 CalcCurrentWeight();
	bool IsEncumbered() { return (weight > (GetSTR() * 10)); }
	inline uint32 GetCopper() const { return m_pp.copper; }
	inline uint32 GetSilver() const { return m_pp.silver; }
	inline uint32 GetGold() const { return m_pp.gold; }
	inline uint32 GetPlatinum() const { return m_pp.platinum; }


	int32 CalcHPRegenCap();
	uint16 CalculateLungCapacity();
	int32 CalculateFatiguePenalty();
	int8 GetFatigue() { return m_pp.fatigue; }
	void SetFatigue(int8 in_fatigue);

	//This gets the skill value of the item type equiped in the Primary Slot
	uint16 GetPrimarySkillValue();

	int HackCount;

	bool exemptHackCount;
	glm::vec3 ExpectedRewindPos;
	std::chrono::high_resolution_clock::time_point last_position_update_time;


	inline uint32 GetEXP() const { return m_pp.exp; }

	void	AddEXP(uint32 in_add_exp, uint8 conlevel = 0xFF, Mob* killed_mob = nullptr, int16 avg_level = 0, bool is_split = false, int16 highest_level = 0);
	void	SetEXP(uint32 set_exp, uint32 set_aaxp, bool resexp=false, bool is_split = false);
	void	AddQuestEXP(uint32 in_add_exp, bool bypass_cap = false);
	void	AddEXPPercent(uint8 percent, uint8 level = 1);
	void	AddLevelBasedExp(uint8 exp_percentage, uint8 max_level=0);
	void	InspectBuffs(Client* Inspector, int Rank);
	virtual void SetLevel(uint8 set_level, bool command = false);
	void	GetExpLoss(Mob* attacker, uint16 spell, int &exploss, uint8 killedby = 0);
	uint32  GetEXPForLevel(uint16 level, bool aa = false);
	bool	IsInExpRange(Mob* defender);
	bool	IsInLevelRange(uint8 maxlevel);

	void GoToBind(uint8 bindnum = 0);
	void BootFromGuildInstance();
	void GoToSafeCoords(uint16 zone_id, uint32 zone_guild_id);
	void Gate();
	void SetBindPoint(int to_zone = -1, const glm::vec3& location = glm::vec3());
	void SetBindPoint2(int to_zone = -1, const glm::vec4& location = glm::vec4());
	uint32 GetStartZone(void);
	void MovePC(const char* zonename, float x, float y, float z, float heading, uint8 ignorerestrictions = 0, ZoneMode zm = ZoneSolicited);
	void MovePC(uint32 zoneID, float x, float y, float z, float heading, uint8 ignorerestrictions = 0, ZoneMode zm = ZoneSolicited);
	void MovePCGuildID(uint32 zoneID, uint32 zone_guild_id, float x, float y, float z, float heading, uint8 ignorerestrictions = 0, ZoneMode zm = ZoneSolicited);
	void MovePCQuest(uint32 zoneID, float x, float y, float z, float heading, uint8 ignorerestrictions = 0, ZoneMode zm = ZoneSolicited);
	void MovePC(float x, float y, float z, float heading, uint8 ignorerestrictions = 0, ZoneMode zm = ZoneSolicited);
	bool CheckLoreConflict(const EQ::ItemData* item);
	void ChangeLastName(const char* in_lastname);
	void SetTemporaryLastName(char* in_lastname);
	void SetTemporaryCustomizedLastName(char* in_lastname);
	void SacrificeConfirm(Mob* caster);
	void Sacrifice(Mob* caster);
	void GoToDeath();
	void ForceGoToDeath();
	void SetZoning(bool in) { zoning = in; }

	FACTION_VALUE GetReverseFactionCon(Mob* iOther);
	FACTION_VALUE GetFactionLevel(uint32 char_id, uint32 p_race, uint32 p_class, uint32 p_deity, int32 pFaction, Mob* tnpc, bool lua = false);
	int16 GetFactionValue(Mob* tnpc);
	int32 GetCharacterFactionLevel(int32 faction_id);
	int32 GetModCharacterFactionLevel(int32 faction_id, bool skip_illusions = false);
	void MerchantRejectMessage(Mob *merchant, int primaryfaction);
	int32 UpdatePersonalFaction(int32 char_id, int32 npc_value, int32 faction_id, int32 temp, bool skip_gm = true, bool show_msg = true);
	void SetFactionLevel(uint32 char_id, uint32 npc_faction_id, bool quest = false);
	void SetFactionLevel2(uint32 char_id, int32 faction_id, int32 value, uint8 temp);
	int32 GetRawItemAC();
	uint8 GetRaceArmorSize();

	inline uint32 LSAccountID() const { return lsaccountid; }
	inline uint32 GetWID() const { return WID; }
	inline void SetWID(uint32 iWID) { WID = iWID; }
	inline uint32 AccountID() const { return account_id; }

	inline const char* AccountName()const { return account_name; }
	inline int16 Admin() const { return admin; }
	inline uint32 CharacterID() const { return character_id; }
	void UpdateAdmin(bool iFromDB = true);
	void UpdateGroupID(uint32 group_id);
	void UpdateWho(uint8 remove = 0);
	bool GMHideMe(Client* client = 0);

	inline bool IsInAGuild() const { return(guild_id != GUILD_NONE && guild_id != 0); }
	inline bool IsInGuild(uint32 in_gid) const { return(in_gid == guild_id && IsInAGuild()); }
	inline bool IsGMInGuild(uint32 in_gid) const { return(in_gid == gm_guild_id && GetGM()); }
	inline uint32	GuildID() const { return guild_id; }
	inline uint8	GuildRank()		const { return guildrank; }
	void	SendGuildMOTD(bool GetGuildMOTDReply = false);
	void	SendGuildSpawnAppearance();
	void	SendGuildList();
	void	SendPlayerGuild();
	void	RefreshGuildInfo();
	std::string GetGuildName();

	uint8 GetClientMaxLevel() const { return client_max_level; }
	void SetClientMaxLevel(uint8 max_level) { client_max_level = max_level; }

	void	SendManaUpdatePacket();
	void	SendManaUpdate();
	void	SendStaminaUpdate();
	uint8	GetFace()		const { return m_pp.face; }
	void	WhoAll(Who_All_Struct* whom);
	void	FriendsWho(char *FriendsString);

	void	Stun(int duration, Mob* attacker);
	void	UnStun();
	void	ReadBook(BookRequest_Struct *book);
	void	QuestReadBook(const char* text, uint8 type);
	void	SendClientMoneyUpdate(uint8 type,uint32 amount);
	void	SendBankAndCursorToInventoryMoneyUpdate(uint8 type, uint32 amount);
	void	SendClientMoney(uint32 copper, uint32 silver, uint32 gold, uint32 platinum);
	bool	TakeMoneyFromPP(uint64 copper, bool updateclient=false);
	void	AddMoneyToPP(uint64 copper,bool updateclient);
	void	AddMoneyToPP(uint32 copper, uint32 silver, uint32 gold,uint32 platinum,bool updateclient);
	bool	HasMoney(uint64 copper);
	void	ClearMoney();
	void	RemoveAllSkills();
	void    ResetAllSkillsByLevel(uint8 level_cap = 0xFF); // Removes or caps skills to the passed in level (default = MaxLevel)
	void    ResetSkillByLevel(EQ::skills::SkillType skill, uint8 level_cap = 0xFF); // Removes or caps skills to the passed in level (default = MaxLevel)
	uint64	GetCarriedMoney();
	uint64	GetAllMoney();

	void ResetStartingSkills();
	void SetRaceStartingSkills();
	void SetRacialLanguages();
	void SetClassLanguages();

	bool IsDiscovered(uint32 itemid);
	void DiscoverItem(uint32 itemid);

	bool TGB() const { return tgb; }

	void OnDisconnect(bool hard_disconnect);

	uint16 GetSkillPoints() { return m_pp.points;}
	void SetSkillPoints(int inp) { m_pp.points = inp;}

	void IncreaseSkill(int skill_id, int value = 1) { if (skill_id <= EQ::skills::HIGHEST_SKILL) { m_pp.skills[skill_id] += value; } }
	void IncreaseLanguageSkill(int skill_id, int value = 1);
	virtual uint16 GetSkill(EQ::skills::SkillType skill_id);
	uint32	GetRawSkill(EQ::skills::SkillType skill_id) const { if (skill_id <= EQ::skills::HIGHEST_SKILL) { return(m_pp.skills[skill_id]); } return 0; }
	bool	HasSkill(EQ::skills::SkillType skill_id);
	bool	CanHaveSkill(EQ::skills::SkillType skill_id, uint8 at_level = 0xFF);
	void	SetSkill(EQ::skills::SkillType skill_num, uint16 value, bool silent = false);
	void	AddSkill(EQ::skills::SkillType skillid, uint16 value);
	void	CheckSpecializeIncrease(uint16 spell_id);
	void	CheckSongSkillIncrease(uint16 spell_id);
	bool	CheckIncreaseSkill(EQ::skills::SkillType skillid, Mob *against_who, float difficulty = 7.0, uint8 success = SKILLUP_SUCCESS, bool skipcon = false); //valid values for difficulty are 1 to 15. Success is a float simply so we don't need to cast
	void	CheckLanguageSkillIncrease(uint8 langid, uint8 TeacherSkill);
	void	SetLanguageSkill(int langid, int value);

	void	SendStats(Client* client);
	void	SendQuickStats(Client* client);

	uint16 MaxSkill(EQ::skills::SkillType skill_id, uint8 class_id, uint8 level) const;
	inline uint16 MaxSkill(EQ::skills::SkillType skill_id) const { return MaxSkill(skill_id, GetClass(), GetLevel()); }
	uint16	GetMaxSkillAfterSpecializationRules(EQ::skills::SkillType skill_id, uint16 maxSkill);
	uint8 GetSkillTrainLevel(EQ::skills::SkillType skill_id, uint16 class_id);
	void MaxSkills();

	bool TradeskillExecute(DBTradeskillRecipe_Struct *spec);
	void CheckIncreaseTradeskill(bool isSuccessfulCombine, EQ::skills::SkillType tradeskill);

	void GMKill();
	inline bool IsMedding() const {return medding;}
	inline uint16 GetDuelTarget() const { return duel_target; }
	inline bool IsDueling() const { return duelaccepted; }
	inline void SetDuelTarget(uint16 set_id) { duel_target=set_id; }
	inline void SetDueling(bool duel) { duelaccepted = duel; }
	// use this one instead
	void MemSpell(uint16 spell_id, int slot, bool update_client = true);
	void UnmemSpell(int slot, bool update_client = true);
	void UnmemSpellAll(bool update_client = true);
	std::vector<int> GetMemmedSpells();
	std::vector<int> GetScribeableSpells(uint8 min_level = 1, uint8 max_level = 0);
	std::vector<int> GetScribedSpells();

	// Bulk Scribe/Learn
	uint16 ScribeSpells(uint8 min_level, uint8 max_level);

	// defer save used when bulk saving
	void ScribeSpell(uint16 spell_id, int slot, bool update_client = true, bool defer_save = false);
	void SaveSpells();

	// defer save used when bulk saving
	void UnscribeSpell(int slot, bool update_client = true, bool defer_save = false);

	void UnscribeSpellAll(bool update_client = true);
	bool SpellGlobalCheck(uint16 spell_id, uint32 char_id);
	bool SpellBucketCheck(uint16 spell_id, uint32 char_id);
	uint8 GetCharMaxLevelFromQGlobal();
	uint8 GetCharMaxLevelFromBucket();

	inline bool IsSitting() const {return (playeraction == eaSitting);}
	inline bool IsBecomeNPC() const { return npcflag; }
	inline uint8 GetBecomeNPCLevel() const { return npclevel; }
	inline void SetBecomeNPC(bool flag) { npcflag = flag; }
	inline void SetBecomeNPCLevel(uint8 level) { npclevel = level; }
	void SetFeigned(bool in_feigned);
	/// this cures timing issues cuz dead animation isn't done but server side feigning is?
	inline bool IsFeigned() const { return(feigned); }
	uint32 GetFeignedTime() { return feigned_time; }
	EQStreamInterface* Connection() { return eqs; }
#ifdef PACKET_PROFILER
	void DumpPacketProfile() { if(eqs) eqs->DumpPacketProfile(); }
#endif
	uint32 GetEquipment(uint8 material_slot) const; // returns item id
	uint32 GetEquipmentColor(uint8 material_slot) const;
	virtual void UpdateEquipmentLight() { m_Light.Type[EQ::lightsource::LightEquipment] = m_inv.FindBrightestLightType(); m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]); }

	inline bool AutoSplitEnabled() { return m_pp.autosplit != 0; }

	void SummonHorse(uint16 spell_id);
	void SetHorseId(uint16 horseid_in);
	uint16 GetHorseId() const { return horseId; }

	void NPCSpawn(NPC *target_npc, const char *identifier, uint32 extra = 0);

	bool BindWound(uint16 bindmob_id, bool start, bool fail = false);
	void SetTradeskillObject(Object* object) { m_tradeskill_object = object; }
	Object* GetTradeskillObject() { return m_tradeskill_object; }
	inline PTimerList &GetPTimers() { return(p_timers); }

	bool SendGMCommand(std::string message, bool ignore_status = false);

	//AA Methods
	void ResetAA();
	void BuyAA(AA_Action* action);
	//this function is used by some AA stuff
	void MemorizeSpell(uint32 slot,uint32 spellid,uint32 scribing);
	void SetAATitle(const char *Title);
	void SetTitleSuffix(const char *txt);
	inline uint32 GetMaxAAXP(void) const { return max_AAXP; }
	inline uint32 GetAAXP() const { return m_pp.expAA; }
	void SendAAStats();
	void SendAATable();
	void SendAATimers();
	void ResetAATimer(aaID activate, uint32 messageid);
	void ResetSingleAATimer(aaID activate, uint32 messageid);
	int GetAATimerID(aaID activate);
	int CalcAAReuseTimer(const AA_DBAction *caa);
	void ActivateAA(aaID activate);
	void SendAATimer(uint32 ability, uint32 begin, uint32 end);
	void EnableAAEffect(aaEffectType type, uint32 duration = 0);
	void DisableAAEffect(aaEffectType type);
	bool CheckAAEffect(aaEffectType type);
	void FadeAllAAEffects();
	uint32 GetAA(uint32 aa_id) const;
	bool SetAA(uint32 aa_id, uint32 new_value);
	inline uint32 GetAAPointsSpent() { return m_pp.aapoints_spent; }
	int16 CalcAAFocus(focusType type, uint32 aa_ID, uint16 spell_id);
	void SetAAPoints(uint32 points) { m_pp.aapoints = points; SendAAStats(); }
	void AddAAPoints(uint32 points) { m_pp.aapoints += points; SendAAStats(); }
	int GetAAPoints() { return m_pp.aapoints; }
	int GetSpentAA() { return m_pp.aapoints_spent; }
	void RefundAA();
	void IncrementAA(int aa_id);
	int32 GetAAEffectDataBySlot(uint32 aa_ID, uint32 slot_id, bool GetEffect, bool GetBase1, bool GetBase2);
	int32 GetAAEffectid(uint32 aa_ID, uint32 slot_id) { return GetAAEffectDataBySlot(aa_ID, slot_id, true, false,false); }
	int32 GetAABase1(uint32 aa_ID, uint32 slot_id) { return GetAAEffectDataBySlot(aa_ID, slot_id, false, true,false); }
	int32 GetAABase2(uint32 aa_ID, uint32 slot_id) { return GetAAEffectDataBySlot(aa_ID, slot_id, false, false,true); }
	int32 acmod();
	void WarCry(uint8 rank); // AA Warcry
	bool FleshToBone(); // AA Flesh To Bone
	uint8 GetAARankTitle();
	void ExpendAATimer(int aaid_int);

	// Item methods
	void EVENT_ITEM_ScriptStopReturn();
	uint32	NukeItem(uint32 itemnum, uint8 where_to_check =
		(invWhereWorn | invWherePersonal | invWhereBank | invWhereTrading | invWhereCursor));
	void	SetMaterial(int16 slot_id, uint32 item_id);
	int32	GetItemIDAt(int16 slot_id);
	bool	FindOnCursor(uint32 item_id);
	void	ClearPlayerInfoAndGrantStartingItems(bool goto_death = true);

	// Reset player back to spawn for NewGame+ type restarts
	// - Faction is reset
	// - Spells are unmemmed
	// - Level-down to 'new_level'
	// - For more drastic changes (like class change), the 'new_level2' and 'reset_skill_points' can be used to futher reset a character to their base skills and points.
	void	ResetPlayerForNewGamePlus(uint8 new_level, uint8 new_level2, bool reset_skill_points);
	bool	PutItemInInventory(int16 slot_id, const EQ::ItemInstance& inst, bool client_update = false);
	bool	PushItemOnCursor(const EQ::ItemInstance& inst, bool client_update = false);
	bool	PushItemOnCursorWithoutQueue(EQ::ItemInstance* inst, bool drop = false);
	void	DeleteItemInInventory(int16 slot_id, int8 quantity = 0, bool client_update = false, bool update_db = true);
	bool	SwapItem(MoveItem_Struct* move_in);
	void	SwapItemResync(MoveItem_Struct* move_slots);
	void	QSSwapItemAuditor(MoveItem_Struct* move_in, bool postaction_call = false);
	void	PutLootInInventory(int16 slot_id, const EQ::ItemInstance &inst, LootItem** bag_item_data = 0);
	bool	AutoPutLootInInventory(EQ::ItemInstance& inst, bool try_worn = false, bool try_cursor = true, LootItem** bag_item_data = 0);
	bool	SummonItem(uint32 item_id, int8 quantity = -1, uint16 to_slot = EQ::invslot::slotCursor, bool force_charges = false, const QuarmItemData& quarm_item_data = EmptyQuarmItemData);
	void	DropItem(int16 slot_id);
	void	SendLootItemInPacket(const EQ::ItemInstance* inst, int16 slot_id);
	void	SendItemPacket(int16 slot_id, const EQ::ItemInstance* inst, ItemPacketType packet_type, int16 fromid = 0, int16 toid = 0, int16 skill = 0);
	bool	IsValidSlot(uint32 slot);
	bool	IsBankSlot(uint32 slot);


	std::map<uint32, LootLockout> loot_lockouts;
	std::map<uint16, LootItemLockout> looted_legacy_items;
	bool IsLootLockedOutOfNPC(uint32 npctype_id);


	inline bool IsTrader() const { return(Trader); }
	eqFilterMode GetFilter(eqFilterType filter_id) const { return ClientFilters[filter_id]; }
	void SetFilter(eqFilterType filter_id, eqFilterMode value) { ClientFilters[filter_id]=value; }

	void LeaveGroup();

	bool Hungry() const { return m_pp.hunger_level < 3000; } //Stomach is below the auto-consume threeshold of 3000. 
	bool Thirsty() const { return m_pp.thirst_level < 3000; }
	bool Famished() const { return m_pp.thirst_level <= 0 || m_pp.hunger_level <= 0; } //Stomach is empty
	bool FoodFamished() const { return m_pp.hunger_level <= 0; }
	bool WaterFamished() const { return m_pp.thirst_level <= 0; }
	int16 GetHunger() const { return m_pp.hunger_level; }
	int16 GetThirst() const { return m_pp.thirst_level; }
	void SetHunger(int16 in_hunger);
	void SetThirst(int16 in_thirst);
	void SetConsumption(int16 in_hunger, int16 in_thirst);
	void ProcessHungerThirst();
	void ProcessFatigue();
	void AddWeaponAttackFatigue(const EQ::ItemInstance *weapon);

	bool	CheckTradeLoreConflict(Client* other);
	void	LinkDead();

	//remove charges/multiple objects from inventory:
	//bool DecreaseByType(uint32 type, uint8 amt);
	bool DecreaseByID(uint32 type, uint8 amt);
	void Escape(); //AA Escape
	void RemoveNoRent(bool client_update = true);
	void RemoveDuplicateLore(bool client_update = true);
	void MoveSlotNotAllowed(bool client_update = true);
	virtual void RangedAttack(Mob* other);
	virtual void ThrowingAttack(Mob* other, bool CanDoubleAttack = false);
	void DoClassAttacks(Mob *ca_target, uint16 skill = -1, bool IsRiposte=false);
	void DoBackstab(Mob* defender = nullptr);
	int TryAssassinate(Mob* defender, EQ::skills::SkillType skillInUse);

	void SetZoneFlag(uint32 zone_id);
	void ClearZoneFlag(uint32 zone_id);
	bool HasZoneFlag(uint32 zone_id);
	void SendZoneFlagInfo(Client *to);
	void LoadZoneFlags(LinkedList<ZoneFlags_Struct*>* ZoneFlags);

	bool CanFish();
	void GoFish();
	void ForageItem(bool guarantee = false);
	//Calculate vendor price modifier based on CHA: (reverse==merchant buying)
	float CalcPriceMod(Mob* other = 0, bool reverse = false);
	void ResetTrade();
	void CreateGroundObject(const EQ::ItemInstance* item, glm::vec4 coords, uint32 decay_time = 300000, bool message = false);
	bool UseDiscipline(uint8 disc_id);
	uint8 DisciplineUseLevel(uint8 disc_id);
	bool CastDiscipline(uint8 disc_id, uint8 level_to_use);
	uint8 GetDiscTimerID(uint8 disc_id);
	uint32 CheckDiscTimer(uint8 type);

	bool CheckTitle(int titleset);
	void EnableTitle(int titleset);
	void RemoveTitle(int titleset);

	std::list<CharacterConsent> consent_list;
	void Consent(uint8 permission, char ownername[64], char grantname[64], bool do_not_update_list = false, uint32 corpse_id = 0);
	bool IsConsented(std::string grantname);
	bool LoadCharacterConsent();
	void RemoveFromConsentList(char ownername[64], uint32 corpse_id = 0);

	//Anti-Cheat Stuff
	uint32 m_TimeSinceLastPositionCheck;
	float m_DistanceSinceLastPositionCheck;
	bool m_CheatDetectMoved;
	void SetShadowStepExemption(bool v);
	void SetKnockBackExemption(bool v);
	void SetPortExemption(bool v);
	void SetSenseExemption(bool v) { m_SenseExemption = v; }
	void SetAssistExemption(bool v) { m_AssistExemption = v; }
	const bool IsShadowStepExempted() const { return m_ShadowStepExemption; }
	const bool IsKnockBackExempted() const { return m_KnockBackExemption; }
	const bool IsPortExempted() const { return m_PortExemption; }
	const bool IsSenseExempted() const { return m_SenseExemption; }
	const bool IsAssistExempted() const { return m_AssistExemption; }
	const bool GetGMSpeed() const { return (gmspeed > 0); }
	const bool GetGMInvul() const { return gminvul; }
	void SetGmInvul(bool state) { gminvul = state; invulnerable = state; }
	void CheatDetected(CheatTypes CheatType, float x, float y, float z);
	const bool IsMQExemptedArea(uint32 zoneID, float x, float y, float z) const;
	bool CanUseReport;

	//This is used to later set the buff duration of the spell, in slot to duration.
	//Doesn't appear to work directly after the client recieves an action packet.
	void SendBuffDurationPacket(uint16 spell_id, int duration, int inlevel, int slotid, int bardmodifier);

	bool ClientFinishedLoading() { return (conn_state == ClientConnectFinished); }
	int FindSpellBookSlotBySpellID(uint16 spellid);
	int FindSpellMemSlotBySpellID(uint16 spellid);
	int GetNextAvailableSpellBookSlot(int starting_slot = 0);
	inline uint32 GetSpellByBookSlot(int book_slot) { return m_pp.spell_book[book_slot]; }
	inline bool HasSpellScribed(int spellid) { return (FindSpellBookSlotBySpellID(spellid) != -1 ? true : false); }
	bool	PendingTranslocate;
	time_t	TranslocateTime;
	bool	PendingSacrifice;
	uint16 sacrifice_caster_id;
	PendingTranslocate_Struct PendingTranslocateData;
	void	SendOPTranslocateConfirm(Mob *Caster, uint16 SpellID);

	inline const EQ::versions::ClientVersion ClientVersion() const { return m_ClientVersion; }
	inline const uint32 ClientVersionBit() const { return m_ClientVersionBit; }
	inline void SetClientVersion(EQ::versions::ClientVersion client_version) { m_ClientVersion = client_version; }

	int GetAggroCount();

	void CheckEmoteHail(NPC* n, const char* message);

	void SummonAndRezzAllCorpses();
	void SummonAllCorpses(const glm::vec4& position);
	void DepopAllCorpses();
	void DepopPlayerCorpse(uint32 dbid);
	void BuryPlayerCorpses();
	uint32 GetCorpseCount() { return database.GetCharacterCorpseCount(CharacterID()); }
	uint32 GetCorpseID(int corpse) { return database.GetCharacterCorpseID(CharacterID(), corpse); }
	uint32 GetCorpseItemAt(int corpse_id, int slot_id) { return database.GetCharacterCorpseItemAt(corpse_id, slot_id); }
	void SuspendMinion();
	void Doppelganger(uint16 spell_id, Mob *target, const char *name_override, int pet_count, int pet_duration);
	void NotifyNewTitlesAvailable();
	void Signal(uint32 data);
	Mob *GetBindSightTarget() { return bind_sight_target; }
	void SetBindSightTarget(Mob *n) { bind_sight_target = n; }
	uint16 GetBoatID() const { return BoatID; }
	uint32 GetBoatNPCID() { return m_pp.boatid; }
	char* GetBoatName() { return m_pp.boat; }
	void SetBoatID(uint32 boatid);
	void SetBoatName(const char* boatname);
	QGlobalCache *GetQGlobals() { return qGlobals; }
	QGlobalCache *CreateQGlobals() { qGlobals = new QGlobalCache(); return qGlobals; }
	inline void SetPendingGuildInvitation(bool inPendingGuildInvitation) { PendingGuildInvitation = inPendingGuildInvitation; }
	inline bool GetPendingGuildInvitation() { return PendingGuildInvitation; }
	void LocateCorpse();
	void SendTargetCommand(uint32 EntityID);
	bool MoveItemToInventory(EQ::ItemInstance *BInst, bool UpdateClient = false);
	std::list<RespawnOption> respawn_options;
	void SetPendingRezzData(uint32 zoneID, uint32 zoneGuildID, int XP, uint32 DBID, uint16 SpellID, const char *CorpseName) { PendingRezzZoneID = zoneID, PendingRezzZoneGuildID = zoneGuildID, PendingRezzXP = XP; PendingRezzDBID = DBID; PendingRezzSpellID = SpellID; PendingRezzCorpseName = CorpseName; }
	bool IsRezzPending() { return PendingRezzSpellID > 0; }
	bool IsDraggingCorpse(uint16 CorpseID);
	inline bool IsDraggingCorpse() { return (DraggedCorpses.size() > 0); }
	void DragCorpses();
	inline void ClearDraggedCorpses() { DraggedCorpses.clear(); }

	void SendSpellAnim(uint16 targetid, uint16 spell_id);

	void DuplicateLoreMessage(uint32 ItemID);
	void GarbleMessage(char *, uint8);

	void TickItemCheck();
	void TryItemTick(int slot);
	void ItemTimerCheck();
	void TryItemTimer(int slot);

	int32 GetActSTR() { return( std::min(GetMaxSTR(), GetSTR()) ); }
	int32 GetActSTA() { return( std::min(GetMaxSTA(), GetSTA()) ); }
	int32 GetActDEX() { return( std::min(GetMaxDEX(), GetDEX()) ); }
	int32 GetActAGI() { return( std::min(GetMaxAGI(), GetAGI()) ); }
	int32 GetActINT() { return( std::min(GetMaxINT(), GetINT()) ); }
	int32 GetActWIS() { return( std::min(GetMaxWIS(), GetWIS()) ); }
	int32 GetActCHA() { return( std::min(GetMaxCHA(), GetCHA()) ); }
	void LoadAccountFlags();
	void SetAccountFlag(std::string flag, std::string val);
	std::string GetAccountFlag(std::string flag);
	void QuestReward(Mob* target, int32 copper = 0, int32 silver = 0, int32 gold = 0, int32 platinum = 0, int16 itemid = 0, int32 exp = 0, bool faction = false);
	void QuestReward(Mob* target, const QuestReward_Struct& reward); // TODO: Fix faction processing
	void RewindCommand();
	virtual bool IsCharmedPet() { return charmed; }

	void TripInterrogateInvState() { interrogateinv_flag = true; }
	bool GetInterrogateInvState() { return interrogateinv_flag; }

	bool InterrogateInventory(Client* requester, bool log, bool silent, bool allowtrip, bool& error, bool autolog = true);

	uint8 client_max_level;

	bool IsLFG() { return LFG; }

	void SendSoulMarks(SoulMarkList_Struct* SMS);

	void SetGMGuildID(uint32 in_gid) { gm_guild_id = in_gid; }
	bool has_zomm;
	bool client_position_update;
	bool ignore_zone_count; 
	uint32 prev_last_login_time;
	uint16 last_target;

	inline virtual int32 GetLastLogin() const { return m_pp.lastlogin; }
	inline virtual int32 GetTimePlayedMin() const { return m_pp.timePlayedMin; }

	bool ClickyOverride() { return clicky_override; }
	void SetActiveDisc(uint8 value, int16 spellid) { active_disc = value; active_disc_spell = spellid; }
	void FadeDisc() { BuffFadeBySpellID(active_disc_spell); p_timers.Clear(&database, pTimerSpellStart + active_disc_spell); active_disc = 0; active_disc_spell = 0; Log(Logs::General, Logs::Discs, "Fading currently enabled disc."); }
	uint8 GetActiveDisc() { return active_disc; }
	uint16 GetActiveDiscSpell() { return active_disc_spell; }
	bool HasInstantDisc(uint16 skill_type = 0);

	void SendClientVersion();
	void FixClientXP();
	void SendToBoat(bool messageonly = false);

	uint32 trapid; //ID of trap player has triggered. This is cleared when the player leaves the trap's radius, or it despawns.

	void SendMerchantEnd();
	float GetPortHeading(uint16 newx, uint16 newy);
	bool IsMule() { return (Admin() < 80 && m_pp.mule); }
	void SendCancelTrade(Mob* with);
	void ClearPTimers(uint16 type);
	void UpdateItemHP(EQ::ItemInstance* item, bool equip = true);
	void UpdateHP(int32 hp, bool equip = true);
	uint32 GetTraderSession() { return TraderSession; }
	void SetTraderSession(uint32 val) { TraderSession = val; }
	void ShowRegenInfo(Client* message);
	bool IsRested() { return rested && IsSitting() && !IsFeigned(); }
	void SendBerserkState(bool state);

	int combine_skill;
	int combine_recipe;
	uint32 last_combine;
	int32 combine_interval[25];
	int combine_count;

	uint32 last_forage;
	int32 forage_interval[20];
	int forage_count;

	uint32 last_search;
	int32 search_interval[10];
	std::string search_string[10];
	int search_count;

	uint32 last_fish;
	int32 fish_interval[10];
	int fish_count;

	uint32 last_who;
	int32 who_interval[10];
	int who_count;

	uint32 last_friends;
	int32 friends_interval[10];
	int friends_count;
	
	uint32 spell_slot[8];
	int32 spell_interval[8][20];
	uint32 last_spell[8];
	int spell_count[8];

	uint32 last_say;
	int32 say_interval[10];
	int say_count;

	int pet_count;
	uint32 last_pet;
	int32 pet_interval[10];

	bool camping;
	bool camp_desktop;
	void ClearGroupInvite();
	void ClearTimersOnDeath();
	void UpdateLFG(bool value = false, bool ignoresender = false);
	uint16 poison_spell_id; // rogue apply poison
	bool ShowHelm() { return m_pp.showhelm; }
	void SetShowHelm(bool value) { m_pp.showhelm = value; }
	bool SpillBeer();
	void AddLootedLegacyItem(uint16 item_id, uint32 legacy_item_expiry);
	bool RemoveLootedLegacyItem(uint16 item_id);
	void RevokeSelf();
	void ShowLegacyItemsLooted(Client* to);
	bool CheckLegacyItemLooted(uint16 item_id);
	std::string GetLegacyItemLockoutFailureMessage(uint16 item_id);
	void LoadLootedLegacyItems();
	void ResetSkill(EQ::skills::SkillType skillid, bool reset_timer = false);
	void ResetAllSkills();

	//Illusion textures 
	uint32 helmcolor;
	uint32 chestcolor;
	uint32 armcolor;
	uint32 bracercolor;
	uint32 handcolor;
	uint32 legcolor;
	uint32 feetcolor;
	int16 pc_helmtexture;
	int16 pc_chesttexture;
	int16 pc_armtexture;
	int16 pc_bracertexture;
	int16 pc_handtexture;
	int16 pc_legtexture;
	int16 pc_feettexture;
	void SetPCTexture(uint8 slot, uint16 texture, uint32 color, bool set_wrist = true);
	void GetPCEquipMaterial(uint8 slot, int16& texture, uint32& color);
	void OPRezzAnswer(uint32 Action, uint32 SpellID, uint16 ZoneID, uint32 GuildZoneID, float x, float y, float z);
	bool IsUnderWater();
	bool IsInWater();
	bool CanBeInZone(uint32 zoneid = 0, uint32 guild_id = 0);

	// this is a TAKP enhancement that tries to give some leeway for /corpse drag range when the player is moving while dragging
	// https://www.takproject.net/forums/index.php?threads/11-9-2022.23725/
	// the client only sends position updates in 1 second intervals when running straight
	// this allows the range to be exceeded when spamming /corpse while moving
	std::vector<uint16> corpse_summon_on_next_update;
	std::vector<std::pair<uint16, Timer*>> corpse_summon_timers;

	void CorpseSummoned(Corpse *corpse);
	void CorpseSummonOnPositionUpdate();

	inline bool InstanceBootGraceTimerExpired() { return instance_boot_grace_timer.Check(); }
	void ShowDevToolsMenu();
	void SendReloadCommandMessages();

protected:
	friend class Mob;
	void CalcItemBonuses(StatBonuses* newbon);
	void AddItemBonuses(const EQ::ItemInstance *inst, StatBonuses* newbon);
	int CalcRecommendedLevelBonus(uint8 level, uint8 reclevel, int basestat);
	void CalcEdibleBonuses(StatBonuses* newbon);
	void CalcAABonuses(StatBonuses* newbon);
	void ApplyAABonuses(uint32 aaid, uint32 slots, StatBonuses* newbon);
	void MakeBuffFadePacket(uint16 spell_id, int slot_id, bool send_message = true);
	bool client_data_loaded;

	int16 GetFocusEffect(focusType type, uint16 spell_id, std::string& item_name, bool dot_tick = false, int spell_level = -1, bool include_items = true, bool include_spells = true, bool include_aa = true);

	Mob* bind_sight_target;

	glm::vec4 m_AutoAttackPosition;
	glm::vec3 m_AutoAttackTargetLocation;
	Mob *aa_los_them_mob;
	bool los_status;
	bool los_status_facing;
	QGlobalCache *qGlobals;

private:
	eqFilterMode ClientFilters[_FilterCount];
	int32 HandlePacket(const EQApplicationPacket *app);
	void OPTGB(const EQApplicationPacket *app);
	void OPMemorizeSpell(const EQApplicationPacket *app);
	void OPMoveCoin(const EQApplicationPacket* app);
	void MoveItemCharges(EQ::ItemInstance &from, int16 to_slot, uint8 type);
	void OPGMTraining(const EQApplicationPacket *app);
	void OPGMEndTraining(const EQApplicationPacket *app);
	void OPGMTrainSkill(const EQApplicationPacket *app);
	void OPGMSummon(const EQApplicationPacket *app);
	void OPCombatAbility(const EQApplicationPacket *app);
	bool    HasRacialAbility(const CombatAbility_Struct* ca_atk);

	void HandleTraderPriceUpdate(const EQApplicationPacket *app);

	int32 CalcAC();
	int32 CalcAlcoholPhysicalEffect();
	int32 CalcSTR();
	int32 CalcSTA(bool unbuffed = false);
	int32 CalcDEX();
	int32 CalcAGI();
	int32 CalcINT();
	int32 CalcWIS();
	int32 CalcCHA();

	int32 CalcMR();
	int32 CalcMR(bool ignoreCap, bool includeSpells);
	int32 CalcFR();
	int32 CalcFR(bool ignoreCap, bool includeSpells);
	int32 CalcDR();
	int32 CalcDR(bool ignoreCap, bool includeSpells);
	int32 CalcPR();
	int32 CalcPR(bool ignoreCap, bool includeSpells);
	int32 CalcCR();
	int32 CalcCR(bool ignoreCap, bool includeSpells);
	int32 CalcMaxHP(bool unbuffed = false);
	int32 CalcBaseHP(bool unbuffed = false);
	int32 CalcHPRegen();
	int32 CalcManaRegen(bool meditate = false);
	void DoHPRegen();
	void DoManaRegen();

	uint8 playeraction;

	EQStreamInterface* eqs;
	EQApplicationPacket* zoneentry;

	uint32				ip;
	uint16				port;
	CLIENT_CONN_STATUS	client_state;
	uint32				character_id;
	uint32				WID;
	uint32				account_id;
	char				account_name[30];
	uint32				lsaccountid;
	char				lskey[30];
	int16				admin;
	uint32				guild_id;
	uint32				gm_guild_id;
	uint8				guildrank; // player's rank in the guild, 0-GUILD_MAX_RANK
	uint16				duel_target;
	bool				duelaccepted;
	std::list<uint32>	keyring;
	std::list<TempMerchantList> item_reimbursement_list;

	bool				tellsoff;	// GM /toggle
	bool				gmhideme;
	bool				AFK;
	bool				LFG;
	bool				auto_attack;
	bool				auto_fire;
	bool				runmode;
	uint8				gmspeed;
	bool				gminvul;
	bool				medding;
	uint16				horseId;
	int8				revoked;
	uint32				pQueuedSaveWorkID;
	uint16				pClientSideTarget;
	int32				weight;
	bool				berserk;
	bool				dead;
	uint16				BoatID;
	uint32				account_creation;
	uint8				firstlogon;
	float initial_z_position;
	bool	Trader;
	std::string	BuyerWelcomeMessage;
	bool	AbilityTimer;
	int Haste; //precalced value
	uint32				versionbit;
	uint32				TraderSession; //Used by bazaar traders to track which players are currently browsing their wares.
	bool				WithCustomer;

	bool m_lock_save_position = false;
public:
	bool IsLockSavePosition() const;
	void SetLockSavePosition(bool lock_save_position);
	inline bool IsMuleInitiated() { return mule_initiated; }
	inline void SetMuleInitiated(bool initiated) { mule_initiated = initiated; }
	bool GetBuffStackingPatch() const { return m_buff_stacking_patch; }
	void SetBuffStackingPatch(bool enabled) { m_buff_stacking_patch = enabled; }
	void SetSongWindowSlots(uint8 song_window_buff_slots) { m_song_window_slots = song_window_buff_slots; }
private:
	bool dev_tools_enabled;

	PlayerProfile_Struct		m_pp;
	ExtendedProfile_Struct		m_epp;
	EQ::InventoryProfile		m_inv;
	Object*						m_tradeskill_object;
	PetInfo						m_petinfo; // current pet data, used while loading from and saving to DB
	PetInfo						m_suspendedminion; // pet data for our suspended minion.

	void SendLogoutPackets();
	bool AddPacket(const EQApplicationPacket *, bool);
	bool AddPacket(EQApplicationPacket**, bool);
	bool SendAllPackets();
	std::deque<CLIENTPACKET *> clientpackets;

	//Zoning related stuff
	void SendZoneCancel(ZoneChange_Struct *zc);
	void SendZoneError(ZoneChange_Struct *zc, int8 err);
	void DoZoneMove(uint16 zone_id, uint32 zone_guild_id, float dest_x, float dest_y, float dest_z, float dest_h);
	void DoZoneSuccess(ZoneChange_Struct *zc, uint16 zone_id, uint32 zone_guild_id, float dest_x, float dest_y, float dest_z, float dest_h, int8 ignore_r);
	void ZonePC(uint32 zoneID, uint32 zoneGuildID, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm);
	void ProcessMovePC(uint32 zoneID, uint32 zoneguildid, float x, float y, float z, float heading, uint8 ignorerestrictions = 0, ZoneMode zm = ZoneSolicited);

	glm::vec4 m_ZoneSummonLocation;
	uint16 zonesummon_id;
	uint32 zonesummon_guildid;
	uint8 zonesummon_ignorerestrictions;
	ZoneMode zone_mode;

	Timer position_timer;
	uint8 position_timer_counter;

	PTimerList p_timers; //persistent timers
	Timer get_auth_timer;
	Timer hpupdate_timer;
	Timer camp_timer;
	Timer process_timer;
	Timer stamina_timer;
	Timer zoneinpacket_timer;
	Timer accidentalfall_timer;
	Timer linkdead_timer;
	Timer dead_timer;
	Timer global_channel_timer;
	Timer fishing_timer;
	Timer autosave_timer;

	Timer	proximity_timer;
	Timer	charm_class_attacks_timer;
	Timer	charm_cast_timer;
	Timer	qglobal_purge_timer;
	Timer	TrackingTimer;

	Timer anon_toggle_timer;
	Timer afk_toggle_timer;
	Timer helm_toggle_timer;
	Timer trade_timer;
	Timer door_check_timer;
	Timer mend_reset_timer;

	Timer disc_ability_timer;
	Timer rest_timer;
	Timer client_ld_timer;
	Timer apperance_timer; //This gets set to 500 milliseconds when we receive an invis packet in, and allows us to also fade sneak if another action happens within the timer's window.
	Timer underwater_timer;

	Timer zoning_timer;
	Timer instance_boot_grace_timer;

    glm::vec3 m_Proximity;

	// client aggro
	Timer m_client_npc_aggro_scan_timer;
	void CheckClientToNpcAggroTimer();

	void BulkSendInventoryItems();
	void SendCursorItems();
	void FillPPItems();

	faction_map factionvalues;

	FILE *SQL_log;
	uint32 max_AAXP;
	uint32 exp_sessionStart;
	uint32 exp_sessionGained;
	uint32 exp_nextCheck;
	uint32 staminacount;
	AA_Array* aa[MAX_PP_AA_ARRAY]; //this list contains pointers into our player profile
	std::map<uint32,uint8> aa_points;
	bool npcflag;
	uint8 npclevel;
	bool feigned;
	bool zoning;
	bool tgb;
	bool instalog;
	int32 last_reported_mana;

	unsigned int AggroCount; // How many mobs are aggro on us.

	LinkedList<ZoneFlags_Struct*> ZoneFlags;

	int TotalSecondsPlayed;

	//Anti Spam Stuff
	Timer *KarmaUpdateTimer;
	uint32 TotalKarma;

	Timer *GlobalChatLimiterTimer; //60 seconds
	uint32 AttemptedMessages;

	EQ::versions::ClientVersion m_ClientVersion;
	uint32 m_ClientVersionBit;

	bool m_ShadowStepExemption;
	bool m_KnockBackExemption;
	bool m_PortExemption;
	bool m_SenseExemption;
	bool m_AssistExemption;

	//Connecting debug code.
	enum { //connecting states, used for debugging only
			NoPacketsReceived, //havent gotten anything
			//this is the point where the client changes to the loading screen
			ReceivedZoneEntry, //got the first packet, loading up PP
			PlayerProfileLoaded, //our DB work is done, sending it
			ZoneInfoSent, //includes PP, spawns, time and weather
			//this is the point where the client shows a status bar zoning in
			NewZoneRequested, //received and sent new zone request
			ClientSpawnRequested, //client sent ReqClientSpawn
			ZoneContentsSent, //objects, doors, zone points
			ClientReadyReceived, //client told us its ready, send them a bunch of crap like guild MOTD, etc
			//this is the point where the client releases the mouse
			ClientConnectFinished //client finally moved to finished state, were done here
	} conn_state;
	void ReportConnectingState();

	uint8 HideCorpseMode;
	bool PendingGuildInvitation;
	int PendingRezzXP;
	uint32 PendingRezzDBID;
	uint32 PendingRezzZoneID;
	uint32 PendingRezzZoneGuildID;
	uint16 PendingRezzSpellID; // Only used for resurrect while hovering.
	std::string PendingRezzCorpseName; // Only used for resurrect while hovering.

	std::list<std::pair<std::string, uint16> > DraggedCorpses;

	Timer ItemTickTimer;
	Timer ItemQuestTimer;
	std::map<std::string,std::string> accountflags;

	uint8 initial_respawn_selection;

	bool interrogateinv_flag; // used to minimize log spamming by players

	void InterrogateInventory_(bool errorcheck, Client* requester, int16 head, int16 index, const EQ::ItemInstance* inst, const EQ::ItemInstance* parent, bool log, bool silent, bool &error, int depth);
	bool InterrogateInventory_error(int16 head, int16 index, const EQ::ItemInstance* inst, const EQ::ItemInstance* parent, int depth);

	void UpdateZoneChangeCount(uint32 zoneid);

	bool clicky_override; // On AK, clickies with 0 casttime did not enforce any restrictions (level, regeant consumption, etc) 
	uint8 active_disc;
	uint16 active_disc_spell;
	bool rested; // Has been sitting for at least 60 seconds.
	int32 food_hp;
	int32 drink_hp;
	uint8 drowning;
	uint16 wake_corpse_id; // Wake The Dead AA
	Timer ranged_attack_leeway_timer;
	uint32 feigned_time; // GetCurrentTime() when feigned
	int8 last_fatigue;
	bool mule_initiated;
	uint32 pending_marriage_character_id;

	bool m_buff_stacking_patch = false; // Flag for whether the client has a buffstacking patch installed. See Handle_OP_SpawnAppearance for how this is negotiated.
	uint8 m_song_window_slots = 0; // If buffstacking patch is on, this is the number of song window slots available
};

#endif
