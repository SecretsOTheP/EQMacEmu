#ifndef ACCOUNT_RESERVATION_MANAGER_H
#define ACCOUNT_RESERVATION_MANAGER_H

#include "../common/types.h"
#include <map>
#include <string>
#include <vector>
#include <tuple>
#include <ctime>

// Forward declarations

struct PlayerInfo {
	uint32 account_id;
	uint32 last_seen;
	bool is_in_raid;
	uint32 grace_period;
	uint32 ip_address; // Keep IP for logging purposes
	PlayerInfo(uint32 acct_id = 0, uint32 last_seen_time = 0, uint32 grace_period_seconds = 60, bool raid_status = false, uint32 ip_addr = 0) 
		: account_id(acct_id)
		, last_seen(last_seen_time ? last_seen_time : time(nullptr))
		, is_in_raid(raid_status) // TODO: Flag this to calculate longer grace period window
		, grace_period(grace_period_seconds)
		, ip_address(ip_addr) {}
};

class AccountRezMgr {
public:
	AccountRezMgr();
	
	void AddRez(uint32 account_id, uint32 ip_address = 0, uint32 grace_period_seconds = 0);
	void RemoveRez(uint32 account_id);
	void PeriodicMaintenance();
	void UpdateLastSeen(uint32 account_id);
	bool CheckGracePeriod(uint32 account_id, uint32 current_time = 0);
	uint32 GetRemainingGracePeriod(uint32 account_id, uint32 current_time = 0) const;
	uint32 Total() const { return m_account_reservations.size(); }
	// uint32 EffectivePopulation(); // Single source of truth for world server population (now handles DB sync)
	
	// Constants and static queries
	static const std::string queue_enablement_query;
	// static const std::string population_sync_query_format; // REMOVED DATABASE SYNC
	
	// In-memory grace whitelist management (no database needed - same process)
	bool IsAccountInGraceWhitelist(uint32 account_id); // Checks and auto-cleans expired entries
	void IncreaseGraceDuration(uint32 account_id, uint32 grace_duration_seconds); // Add to in-memory whitelist
	void RemoveFromGraceWhitelist(uint32 account_id); // Remove from in-memory whitelist
	void CleanupExpiredGraceWhitelist(); // Clean up expired entries from in-memory map
	void UpdateGraceWhitelistStatus(uint32 account_id); // Updates whitelist when reservation changes
	
private:
	// Account tracking data
	std::map<uint32, PlayerInfo> m_account_reservations;  // account_id -> PlayerInfo
	uint32 m_last_cleanup;        // Last cleanup timestamp
	uint32 m_last_database_sync;  // Last database sync timestamp
	bool m_queue_enabled;         // Cached queue enablement status
	
	// In-memory grace whitelist - no database needed since both managers are in same process
	std::map<uint32, uint32> m_grace_whitelist;  // account_id -> expires_at timestamp

	// Helper methods
	void CleanupStaleConnections();
	std::string AccountToString(uint32 account_id) const;
	void LogConnectionChange(uint32 account_id, const std::string& action) const;
	bool ShouldPerformCleanup() const;
	bool ShouldPerformDatabaseSync() const;
};

#endif // ACCOUNT_RESERVATION_MANAGER_H 