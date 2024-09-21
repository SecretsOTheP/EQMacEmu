/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemu.org)

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

#ifndef CORPSE_H
#define CORPSE_H

#include <unordered_set>
#include "mob.h"
#include "client.h"
#include "../common/loot.h"

class Client;
class EQApplicationPacket;
class Group;
class NPC;
class Raid;

struct NPCType;

namespace
{
	class ItemInstance;
}

#define MAX_LOOTERS 72

class Corpse : public Mob {
	public:

	static void SendEndLootErrorPacket(Client* client);
	static void SendLootReqErrorPacket(Client* client, uint8 response = 2);
	Corpse(NPC* in_npc, LootItems *in_itemlist, uint32 in_npctypeid, uint32 in_decaytime = 600000, bool is_client_pet = false);
	Corpse(Client* client, int32 in_rezexp, uint8 killedby = 0);
	Corpse(uint32 in_corpseid, uint32 in_charid, const char* in_charname, LootItems* in_itemlist, uint32 in_copper, uint32 in_silver, uint32 in_gold, uint32 in_plat, const glm::vec4& position, float in_size, uint8 in_gender, uint16 in_race, uint8 in_class, uint8 in_deity, uint8 in_level, uint8 in_texture, uint8 in_helmtexture, uint32 in_rezexp, uint32 in_gmrezexp, uint8 in_killedby, bool in_rezzable, uint32 in_rez_time, bool wasAtGraveyard = false);
	~Corpse();
	static Corpse* LoadCharacterCorpseEntity(uint32 in_dbid, uint32 in_charid, std::string in_charname, const glm::vec4& position, uint32 time_of_death, bool rezzed, bool was_at_graveyard);

	/* Corpse: General */
	virtual bool	Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby = 0, bool bufftic = false) { return true; }
	virtual void	Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false) { return; }
	virtual bool	Attack(Mob* other, int hand = EQ::invslot::slotPrimary, int damagePct = 100) {
		return false;
	}
	virtual bool	CombatRange(Mob* other, float dist_squared = 0.0f, bool check_z = false, bool pseudo_pause = false) { return false; }
	virtual bool	HasRaid()			{ return false; }
	virtual bool	HasGroup()			{ return false; }
	virtual Raid*	GetRaid()			{ return 0; }
	virtual Group*	GetGroup()			{ return 0; }
	inline uint32	GetCorpseDBID()		{ return corpse_db_id; }
	inline char*	GetOwnerName()		{ return corpse_name; }
	bool			IsEmpty() const;
	bool			IsCorpse()			const { return true; }
	bool			IsPlayerCorpse()	const { return is_player_corpse; }
	bool			IsNPCCorpse()		const { return !is_player_corpse; }
	bool			IsBecomeNPCCorpse() const { return become_npc; }
	virtual void	DepopNPCCorpse();
	virtual void	DepopPlayerCorpse();
	bool			Process();
	bool			DepopProcess();
	bool			Save();
	uint32			GetCharID()					{ return char_id; }
	uint32			SetCharID(uint32 iCharID)	{ if (IsPlayerCorpse()) { return (char_id = iCharID); } return 0xFFFFFFFF; };
	uint32			GetDecayTime()				{ if (!corpse_decay_timer.Enabled()) return 0xFFFFFFFF; else return corpse_decay_timer.GetRemainingTime(); }
	uint32			GetRezTime()				{ if (!corpse_rez_timer.Enabled()) return 0; else return corpse_rez_timer.GetRemainingTime(); }
	void			SetDecayTimer(uint32 decay_time);
	
	std::string GetCleanNPCName() {
		return npc_clean_name;
	}

	void			Delete();
	void			Bury();
	void			MoveToGraveyard() { if (IsPlayerCorpse()) corpse_graveyard_timer.Trigger(); }
	void			CalcCorpseName();
	void			LoadPlayerCorpseDecayTime(uint32 dbid, bool empty);

	/* Corpse: Items */
	uint32 GetWornItem(int16 equipSlot) const;
	LootItem* GetItem(uint16 lootslot, LootItem** bag_item_data = 0); 
	void SetPlayerKillItemID(int32 pk_item_id) { player_kill_item = pk_item_id; }
	int32 GetPlayerKillItem() { return player_kill_item; } 
	void RemoveItem(uint16 lootslot);
	void RemoveItem(LootItem* item_data);
	void AddItem(uint32 itemnum, int8 charges, int16 slot = 0);
	
	/* Corpse: Coin */
	void SetCash(uint32 in_copper, uint32 in_silver, uint32 in_gold, uint32 in_platinum);
	void RemoveCash();
	uint32 GetCopper() { return copper; }
	uint32 GetSilver() { return silver; }
	uint32 GetGold() { return gold; }
	uint32 GetPlatinum() { return platinum; }

	/* Corpse: Resurrection */
	bool	IsRezzed() { return rez; }
	void	IsRezzed(bool in_rez) { rez = in_rez; }
	void	CastRezz(uint16 spellid, Mob* Caster);
	void	CompleteResurrection(bool timer_expired = false);
	bool	IsRezzable() { return rezzable; }
	void	SetRezTimer(bool initial_timer = false);

	/* Corpse: Loot */
	void	QueryLoot(Client* to);
	void	LootCorpseItem(Client* client, const EQApplicationPacket* app);
	void	EndLoot(Client* client, const EQApplicationPacket* app);
	void	MakeLootRequestPackets(Client* client, const EQApplicationPacket* app);
	void	AllowPlayerLoot(Mob *them);
	void	DenyPlayerLoot(std::string character_name);
	void	AllowPlayerLoot(std::string character_name);
	void	AddLooter(Mob *who);
	uint32	CountItems();
	bool	CanPlayerLoot(std::string playername);
	bool    ContainsLegacyItem();
	void	ProcessLootLockouts(Client* give_exp_client, NPC* in_npc);
	void	AddPlayerLockout(Client* c);
	void	SetInitialAllowedLooters(const std::vector<std::string>& in) { initial_allowed_looters = std::unordered_set<std::string>(in.begin(), in.end()); }


	inline void	Lock()				{ is_locked = true; }
	inline void	UnLock()			{ is_locked = false; }
	inline bool	IsLocked()			{ return is_locked; }
	inline void	ResetLooter()		{ being_looted_by = 0xFFFFFFFF; }
	inline bool	IsBeingLooted()		{ return (being_looted_by != 0xFFFFFFFF); }

	/* Legacy item looting methods */
	std::unordered_set<std::string> GetLegacyItemLooters() { return legacy_item_looter_client_name_set; }
	inline bool IsLegacyItemLooter(std::string client_name) { return legacy_item_looter_client_name_set.count(client_name); }
	inline void ResetLegacyItemLooterSet() { legacy_item_looter_client_name_set.clear(); }
	void AddLegacyItemLooter(std::string client_name);
	void RemoveLegacyItemLooter(std::string client_name);

	/* Mob */
	void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho);
	bool Summon(Client* client, bool spell, bool CheckDistance);
	void Spawn();

	char		corpse_name[64]; 
	uint32		GetEquipment(uint8 material_slot) const;
	uint32		GetEquipmentColor(uint8 material_slot) const;
	inline int	GetRezExp() { return rez_experience; } 
	inline int	GetGMRezExp() { return gm_rez_experience; } 
	uint8		GetKilledBy() { return killedby; }
	uint32		GetRemainingRezTime() { return rez_time; }

	virtual void UpdateEquipmentLight();

	void		IsOwnerOnline();
	void		SetOwnerOnline(bool value) { is_owner_online = value; }
	bool		GetOwnerOnline() { return is_owner_online; }
	uint32		GetToD() { return time_of_death;  }
	void		RevokeConsent();

protected:
	std::list<uint32> MoveItemToCorpse(Client *client, EQ::ItemInstance *item, int16 equipslot);

private:
	bool		is_player_corpse;
	bool		is_corpse_changed;
	bool		is_locked;
	int32		player_kill_item;
	uint32		corpse_db_id;
	uint32		char_id;
	LootItems	itemlist;
	uint32		copper;
	uint32		silver;
	uint32		gold;
	uint32		platinum;
	bool		player_corpse_depop; /* Sets up Corpse::Process to depop the player corpse */
	uint32		being_looted_by; /* Determines what the corpse is being looted by internally for logic */
	uint32		loot_lockout_timer; /* Length of loot lockout timer, if specified. */
	std::unordered_set<std::string> legacy_item_looter_client_name_set; /* Determines what the corpse is being looted by internally for logic */
	uint32		rez_experience; /* Amount of experience that the corpse would rez for */
	uint32		gm_rez_experience; /* Amount of experience that the corpse would rez for from a GM*/
	bool		rez; /*Sets if a corpse has been rezzed or not to determine if XP should be given*/
	bool		become_npc;
	std::unordered_set<std::string>	allowed_looters; // People allowed to loot the corpse, character name
	std::unordered_set<std::string>	initial_allowed_looters; // People initially allowed to loot the corpse, character name
	std::unordered_set<std::string>	denied_looters; // People not allowed to loot the corpse, character name
	std::unordered_set<std::string>	temporarily_allowed_looters; // People allowed to loot the corpse, character name
	Timer		corpse_decay_timer; /* The amount of time in millseconds in which a corpse will take to decay (Depop/Poof) */
	Timer		corpse_rez_timer; /* The amount of time in millseconds in which a corpse can be rezzed */
	Timer		corpse_delay_timer;
	Timer		corpse_graveyard_timer;
	Timer		corpse_graveyard_moved_timer;
	glm::vec4	corpse_graveyard_moved_lastpos;
	Timer		loot_cooldown_timer; /* Delay between loot actions on the corpse entity */
	Timer		owner_online_timer; /* How often in milliseconds in which a corpse will check if its owner is online */
	uint8		killedby;
	bool		rezzable; /* Determines if the corpse is still rezzable */
	EQ::TintProfile item_tint;
	uint32		rez_time; /* How much of the rez timer remains */
	bool		is_owner_online;
	uint32		time_of_death;
	std::string npc_clean_name;
};

#endif
