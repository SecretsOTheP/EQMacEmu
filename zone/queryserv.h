#ifndef QUERYSERV_ZONE_H
#define QUERYSERV_ZONE_H

#include <list>
#include "masterentity.h"

/*
	enum PlayerGenericLogEventTypes
	These Enums are for the generic logging table that are not complex and require more advanced logic
*/

enum PlayerGenericLogEventTypes {
	Player_Log_Quest = 1,
	Player_Log_Zoning,
	Player_Log_Connect_State,
	Player_Log_Levels,
	Player_Log_Keyring_Addition,
	//Player_Log_QGlobal_Update,
	//Player_Log_Task_Updates,
	//Player_Log_AA_Purchases,
	//Player_Log_Trade_Skill_Events,
	//Player_Log_Issued_Commands,
	//Player_Log_Money_Transactions,
	//Player_Log_Alternate_Currency_Transactions,
};

class QueryServ {
	public:
		QueryServ();
		~QueryServ();
		void SendQuery(std::string Query);
		void PlayerLogEvent(int Event_Type, int Character_ID, std::string Event_Desc);

		void QSFirstToEngageEvent(uint32 char_id, std::string guild_name, std::string mob_name, bool engaged);

		void QSQGlobalUpdate(uint32 char_id, uint32 zone_id, const char* varname, const char* newvalue);
		void QSAAPurchases(uint32 char_id, uint32 zone_id, char aa_type[8], char aa_name[128], uint32 aa_id, uint32 cost);
		void QSAARate(uint32 char_id, uint32 aapoints, uint32 last_unspentAA);
		void QSDeathBy(uint32 char_id, uint32 zone_id, std::string killer_name, uint16 spell, int32 damage, uint8 killedby);
		void QSTSEvents(uint32 char_id, uint32 zone_id, const char results[8], uint32 recipe, uint32 tradeskill, uint16 trivial, float chance);
		void QSMerchantTransactions(uint32 char_id, uint32 zone_id, int16 slot_id, uint32 item_id, uint8 charges,
			uint32 merchant_id, int32 merchant_plat, int32 merchant_gold, int32 merchant_silver, int32 merchant_copper, uint16 merchant_count,
			int32 char_plat, int32 char_gold, int32 char_silver, int32 char_copper, uint16 char_count);
		void QSLootRecords(uint32 char_id, const char* corpsename, const char* type, uint32 zone_id, uint32 item_id, const char* item_name, int16 charges, int32 platinum, int32 gold, int32 silver, int32 copper);
		void QSNPCKills(uint32 npcid, uint32 zoneid, uint8 type, std::list<uint32> &charids, uint32 kill_steal);
		void QSTradeItems(uint32 from_id, uint32 to_id, uint32 from_slot, uint32 to_slot, int16 item_id, uint8 charges, bool bagged, bool pc_trade = true);
		void QSPlayerTrade(struct QSPlayerLogTrade_Struct * QS);
		void QSHandinItems(struct QSPlayerLogHandin_Struct* QS);
		void QSLogCommands(Client* c, const char* Command, const char* Arguments, Mob* Target = 0);
		void QSDeleteItems();
		void QSMoveItems();
		void QSItemDesyncs(uint32 char_id, const char error[512], uint32 zoneid);
		void QSBazaarAudit(const char *seller, const char *buyer, const char *itemName, int itemid, int quantity, int totalCost);
		void QSCoinMove(uint32 from_id, uint32 to_id, uint32 npcid, int32 to_slot, uint32 amount, uint32 cointype = 0);
		void QSGroundSpawn(uint32 characterid, int16 itemid, uint8 quantity, int16 in_bag, uint16 zoneid, bool dropped, bool forced = false);
		void QSLogKillSteal(NPC* const npc, uint32 zoneid, Client* const client, const SInitialEngageEntry& engageids);
};

#endif /* QUERYSERV_ZONE_H */
