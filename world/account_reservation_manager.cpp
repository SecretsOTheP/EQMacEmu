#include "worlddb.h"
#include "account_reservation_manager.h"
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "clientlist.h"  // For GetServerPopulation wrapper

#include <algorithm>
#include <iostream>
#include <fmt/format.h>
#include <set>

extern ClientList client_list;
extern class WorldDatabase database;


// ===== AccountRezMgr Class Implementation =====

AccountRezMgr::AccountRezMgr() : m_last_cleanup(0), m_last_database_sync(0) {
	// Cache queue enablement rule to avoid repeated database queries
	m_queue_enabled = RuleB(Quarm, EnableQueue);
	LogInfo("AccountRezMgr initialized - Queue system: {}", m_queue_enabled ? "ENABLED" : "DISABLED");
}

void AccountRezMgr::AddRez(uint32 account_id, uint32 ip_address, uint32 grace_period_seconds) {
	// Use rule for default grace period if 0 is passed
	if (grace_period_seconds == 0) {
		grace_period_seconds = RuleI(Quarm, DefaultGracePeriod);
	}
	LogInfo("AccountRezMgr: Adding reservation - account_id: {}, ip_address: {}, grace_period: {}s", 
		account_id, ip_address, grace_period_seconds);
	// Set last_seen to current time for new reservations
	m_account_reservations[account_id] = PlayerInfo(account_id, time(nullptr), grace_period_seconds, false, ip_address);
	LogConnectionChange(account_id, "registered");
	
	// Update grace whitelist status immediately
	UpdateGraceWhitelistStatus(account_id);
}

void AccountRezMgr::RemoveRez(uint32 account_id) {
	auto it = m_account_reservations.find(account_id);
	if (it != m_account_reservations.end()) {
		m_account_reservations.erase(it);
		// RemoveConnectionFromDatabase(account_id); // REMOVED DATABASE SYNC
		LogConnectionChange(account_id, "removed");
	}
}

void AccountRezMgr::UpdateLastSeen(uint32 account_id)
{
	auto it = m_account_reservations.find(account_id);
	if (it != m_account_reservations.end()) {
		it->second.last_seen = time(nullptr);
		
		// Update grace whitelist status since last_seen affects expiration
		UpdateGraceWhitelistStatus(account_id);
	}
}

bool AccountRezMgr::CheckGracePeriod(uint32 account_id, uint32 current_time) {
	if (current_time == 0) {
		current_time = time(nullptr);
	}
	
	auto it = m_account_reservations.find(account_id);
	if (it == m_account_reservations.end()) {
		return true; // Connection doesn't exist, should be removed
	}
	
	uint32 time_since_last_seen = current_time - it->second.last_seen;
	bool exceeded_grace_period = time_since_last_seen > it->second.grace_period;
	
	return exceeded_grace_period;
}

uint32 AccountRezMgr::GetRemainingGracePeriod(uint32 account_id, uint32 current_time) const {
	if (current_time == 0) {
		current_time = time(nullptr);
	}
	
	auto it = m_account_reservations.find(account_id);
	if (it == m_account_reservations.end()) {
		return 0; // Connection doesn't exist
	}
	
	uint32 time_since_last_seen = current_time - it->second.last_seen;
	if (time_since_last_seen >= it->second.grace_period) {
		return 0; // Grace period expired
	}
	
	return it->second.grace_period - time_since_last_seen;
}
void AccountRezMgr::CleanupStaleConnections() {
	uint32 current_time = time(nullptr);
	std::vector<uint32> accounts_to_remove;
	
	// First pass: identify what needs to be updated or removed
	for (auto& pair : m_account_reservations) {
		uint32 account_id = pair.first;
		
		// Check if account has active connection to world server (including character select)
		bool has_world_connection = client_list.ActiveConnection(account_id);
		
		if (has_world_connection) {
			// Has world connection - keep connection and update last_seen
			UpdateLastSeen(account_id);
			LogConnectionChange(account_id, "kept_active_char");
		} else {
			// No world connection - but check if this is a new reservation that hasn't had time to connect
			uint32 time_since_creation = current_time - pair.second.last_seen;
			const uint32 CONNECTION_GRACE_SECONDS = 15; // Allow 30 seconds for auto-connect to complete
			
			if (pair.second.last_seen == 0 || time_since_creation < CONNECTION_GRACE_SECONDS) {
				// New reservation or very recent - don't start grace period yet, just wait
				LogInfo("AccountRezMgr: Account [{}] is new reservation, waiting for connection (age: {}s)", 
					account_id, time_since_creation);
				continue; // Skip grace period logic for new reservations
			}
			
			// Existing reservation without connection - start/continue grace period
			LogInfo("AccountRezMgr: Account [{}] doesn't have world connection - adding to grace whitelist", account_id);
			
			// Add to grace whitelist for login server queue bypass
			UpdateGraceWhitelistStatus(account_id);
			
			if (CheckGracePeriod(account_id, current_time)) {
				// Grace period expired - mark for removal
				LogConnectionChange(account_id, "cleanup_grace_expired");
				accounts_to_remove.push_back(account_id);
			} else {
				// Still within grace period - keep reservation, now in grace whitelist
				uint32 time_since_last_seen = current_time - pair.second.last_seen;
				uint32 time_remaining = pair.second.grace_period - time_since_last_seen;
				LogInfo("AccountRezMgr: Account [{}] in grace period, keeping reservation. Time remaining: [{}] seconds", account_id, time_remaining);
			}
		}
	}
	// Second pass: remove all identified connections using proper method
	for (uint32 account_id : accounts_to_remove) {
		RemoveRez(account_id);
	}
}

void AccountRezMgr::PeriodicMaintenance() {
	uint32 current_time = time(nullptr);
	
	CleanupStaleConnections();
	if (ShouldPerformDatabaseSync()) {
		// SyncAllConnectionsToDatabase(); 
		m_last_database_sync = current_time;
		LogInfo("AccountRezMgr: Full reservation list synced to database for crash recovery");
	}
}


void AccountRezMgr::LogConnectionChange(uint32 account_id, const std::string& action) const {
	LogInfo("AccountRezMgr: {} connection for account [{}] - total active accounts: {}", 
		action, account_id, m_account_reservations.size());
}

std::string AccountRezMgr::AccountToString(uint32 account_id) const {
	return fmt::format("Account[{}]", account_id);
}

bool AccountRezMgr::ShouldPerformCleanup() const {
	return (time(nullptr) - m_last_cleanup) >= RuleI(Quarm, IPCleanupInterval);
}

bool AccountRezMgr::ShouldPerformDatabaseSync() const {
	return (time(nullptr) - m_last_database_sync) >= RuleI(Quarm, IPDatabaseSyncInterval);
}

bool AccountRezMgr::IsAccountInGraceWhitelist(uint32 account_id) {
	uint32 current_time = time(nullptr);
	
	CleanupExpiredGraceWhitelist();
	
	auto it = m_grace_whitelist.find(account_id);
	if (it != m_grace_whitelist.end()) {
		LogInfo("AccountRezMgr: Account [{}] found in grace whitelist (expires in {}s)", 
			account_id, it->second - current_time);
		return true;
	}
	
	return false;
}

void AccountRezMgr::UpdateGraceWhitelistStatus(uint32 account_id) {
	auto it = m_account_reservations.find(account_id);
	if (it == m_account_reservations.end()) {
		return;  
	}
	
	const PlayerInfo& info = it->second;
	uint32 current_time = time(nullptr);
	uint32 expires_at = info.last_seen + info.grace_period;
	
	m_grace_whitelist[account_id] = expires_at;
	
	LogInfo("AccountRezMgr: Account [{}] found in whitelist (expires: {})", 
		account_id, expires_at);
}

void AccountRezMgr::RemoveFromGraceWhitelist(uint32 account_id) {
	auto it = m_grace_whitelist.find(account_id);
	if (it != m_grace_whitelist.end()) {
		m_grace_whitelist.erase(it);
		LogInfo("AccountRezMgr: Account [{}] removed from grace whitelist", account_id);
	}
}

void AccountRezMgr::IncreaseGraceDuration(uint32 account_id, uint32 grace_duration_seconds) {
	uint32 expires_at = time(nullptr) + grace_duration_seconds;
	m_grace_whitelist[account_id] = expires_at;
	LogInfo("AccountRezMgr: Account [{}] added to grace whitelist for [{}] seconds", account_id, grace_duration_seconds);
}

void AccountRezMgr::CleanupExpiredGraceWhitelist() {
	uint32 current_time = time(nullptr);
	
	for (auto it = m_grace_whitelist.begin(); it != m_grace_whitelist.end(); ) {
		if (it->second <= current_time) {
			LogInfo("AccountRezMgr: Account [{}] grace whitelist expired, removed", it->first);
			it = m_grace_whitelist.erase(it);
		} else {
			++it;
		}
	}
}
// TODO: Implement database sync/restore methods
/*
void AccountRezMgr::SyncConnectionToDatabase(uint32 account_id, const PlayerInfo& info) {
	std::string query = fmt::format(
		"INSERT INTO active_account_connections (account_id, ip_address, last_seen, grace_period, is_in_raid) "
		"VALUES ({}, {}, FROM_UNIXTIME({}), {}, {}) "
		"ON DUPLICATE KEY UPDATE "
		"ip_address = VALUES(ip_address), "
		"last_seen = VALUES(last_seen), "
		"grace_period = VALUES(grace_period), "
		"is_in_raid = VALUES(is_in_raid)",
		account_id, info.ip_address, info.last_seen, info.grace_period, info.is_in_raid ? 1 : 0
	);
	
	auto result = database.QueryDatabase(query);
	if (!result.Success()) {
		LogError("Failed to sync account connection to database: {}", result.ErrorMessage());
	}
}

void AccountRezMgr::RemoveConnectionFromDatabase(uint32 account_id) {
	std::string query = fmt::format(
		"DELETE FROM active_account_connections WHERE account_id = {}",
		account_id
	);
	
	auto result = database.QueryDatabase(query);
	if (!result.Success()) {
		LogError("Failed to remove account connection from database: {}", result.ErrorMessage());
	}
}

void AccountRezMgr::SyncAllConnectionsToDatabase() {
	// Clear existing entries
	auto clear_result = database.QueryDatabase("DELETE FROM active_account_connections");
	if (!clear_result.Success()) {
		LogError("Failed to clear active_account_connections table: {}", clear_result.ErrorMessage());
		return;
	}
	
	// Sync all current connections
	for (const auto& pair : m_account_reservations) {
		SyncConnectionToDatabase(pair.first, pair.second);
	}
	
	LogInfo("AccountRezMgr: Synced {} connections to database", m_account_reservations.size());
}
*/
