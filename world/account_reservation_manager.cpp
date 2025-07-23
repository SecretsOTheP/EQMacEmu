#include "worlddb.h"
#include "account_reservation_manager.h"
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "clientlist.h" 
#include "world_queue.h" 

#include <algorithm>
#include <iostream>
#include <fmt/format.h>
#include <set>

extern ClientList client_list;
extern class WorldDatabase database;


AccountRezMgr::AccountRezMgr() : m_last_cleanup(0), m_last_database_sync(0) {
}

void AccountRezMgr::AddRez(uint32 account_id, uint32 ip_address, uint32 grace_period_seconds) {
	// Default grace period if none specified or database not ready
	extern bool database_ready;
	if (grace_period_seconds == 0) {
		if (database_ready) {
			grace_period_seconds = RuleI(Quarm, DefaultGracePeriod);
		} else {
			QueueDebugLog(1, "Database not ready - using default grace period for account [{}]", account_id);
			grace_period_seconds = 60;
		}
	}
	
	QueueDebugLog(1, "AccountRezMgr: Adding reservation - account_id: {}, ip_address: {}, grace_period: {}s", 
		account_id, ip_address, grace_period_seconds);
	// Set last_seen to current time for new reservations
	m_account_reservations[account_id] = PlayerInfo(account_id, time(nullptr), grace_period_seconds, false, ip_address);
	LogConnectionChange(account_id, "registered");
	
	UpdateGraceWhitelistStatus(account_id);
}

void AccountRezMgr::RemoveRez(uint32 account_id) {
	auto it = m_account_reservations.find(account_id);
	if (it != m_account_reservations.end()) {
		m_account_reservations.erase(it);
		LogConnectionChange(account_id, "removed");
	}
}

void AccountRezMgr::UpdateLastSeen(uint32 account_id)
{
	auto it = m_account_reservations.find(account_id);
	if (it != m_account_reservations.end()) {
		it->second.last_seen = time(nullptr);
		
		UpdateGraceWhitelistStatus(account_id);
	}
}

bool AccountRezMgr::CheckGracePeriod(uint32 account_id, uint32 current_time) {
	if (current_time == 0) {
		current_time = time(nullptr);
	}
	
	auto it = m_account_reservations.find(account_id);
	if (it == m_account_reservations.end()) {
		return true; 
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
		return 0; 
	}
	
	uint32 time_since_last_seen = current_time - it->second.last_seen;
	if (time_since_last_seen >= it->second.grace_period) {
		return 0; 
	}
	
	return it->second.grace_period - time_since_last_seen;
}
void AccountRezMgr::CleanupStaleConnections() {
	const uint32 CONNECTION_GRACE_SECONDS = 15; // TODO: Make a rule for this?
	uint32 current_time = time(nullptr);
	std::vector<uint32> accounts_to_remove;
	
	// First pass: identify what needs to be updated or removed
	for (auto& pair : m_account_reservations) {
		uint32 account_id = pair.first;
		
		bool has_world_connection = client_list.ActiveConnection(account_id);
		
		if (has_world_connection) {
			UpdateLastSeen(account_id);
			QueueDebugLog(2, "AccountRezMgr: kept_active_char connection for account [{}] - total active accounts: {}", 
				account_id, m_account_reservations.size());
		} else {
			// Check if this is a new reservation that hasn't had time to connect
			uint32 time_since_creation = current_time - pair.second.last_seen;
			
			if (pair.second.last_seen == 0 || time_since_creation < CONNECTION_GRACE_SECONDS) {
				QueueDebugLog(2, "AccountRezMgr: Account [{}] not connected to World, waiting for connection (age: {}s)", 
					account_id, time_since_creation);
				continue; // Skip grace period logic for new reservations
			}
			
			QueueDebugLog(2, "AccountRezMgr: Account [{}] doesn't have world connection - adding to grace whitelist", account_id);
			
			UpdateGraceWhitelistStatus(account_id);
			
			if (CheckGracePeriod(account_id, current_time)) {
				LogConnectionChange(account_id, "cleanup_grace_expired");
				accounts_to_remove.push_back(account_id);
			} else {
				uint32 time_since_last_seen = current_time - pair.second.last_seen;
				uint32 time_remaining = pair.second.grace_period - time_since_last_seen;
				QueueDebugLog(2, "AccountRezMgr: Account [{}] in grace period, keeping reservation. Time remaining: [{}] seconds", account_id, time_remaining);
			}
		}
	}
	for (uint32 account_id : accounts_to_remove) {
		RemoveRez(account_id);
	}
}

void AccountRezMgr::PeriodicMaintenance() {
	uint32 current_time = time(nullptr);
	
	CleanupStaleConnections();
	if (ShouldPerformDatabaseSync()) {
		m_last_database_sync = current_time;
		QueueDebugLog(2, "AccountRezMgr: Full reservation list synced to database for crash recovery");
	}
}


void AccountRezMgr::LogConnectionChange(uint32 account_id, const std::string& action) const {
	QueueDebugLog(1, "AccountRezMgr: {} connection for account [{}] - total active accounts: {}", 
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
		QueueDebugLog(2, "AccountRezMgr: Account [{}] found in grace whitelist (expires in {}s)", 
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
	
	QueueDebugLog(2, "AccountRezMgr: Account [{}] found in whitelist (expires: {})", 
		account_id, expires_at);
}

void AccountRezMgr::RemoveFromGraceWhitelist(uint32 account_id) {
	auto it = m_grace_whitelist.find(account_id);
	if (it != m_grace_whitelist.end()) {
		m_grace_whitelist.erase(it);
		QueueDebugLog(1, "AccountRezMgr: Account [{}] removed from grace whitelist", account_id);
	}
}

void AccountRezMgr::IncreaseGraceDuration(uint32 account_id, uint32 grace_duration_seconds) {
	// Only extend grace for accounts that already have reservations
	auto it = m_account_reservations.find(account_id);
	if (it == m_account_reservations.end()) {
		QueueDebugLog(1, "AccountRezMgr: Cannot extend grace for account [{}] - no existing reservation", account_id);
		return; // Fail silently - account needs a reservation first
	}
	
	uint32 expires_at = time(nullptr) + grace_duration_seconds;
	m_grace_whitelist[account_id] = expires_at;
	QueueDebugLog(1, "AccountRezMgr: Account [{}] grace period extended for [{}] seconds (has existing reservation)", account_id, grace_duration_seconds);
}

void AccountRezMgr::CleanupExpiredGraceWhitelist() {
	uint32 current_time = time(nullptr);
	
	for (auto it = m_grace_whitelist.begin(); it != m_grace_whitelist.end(); ) {
		if (it->second <= current_time) {
			QueueDebugLog(2, "AccountRezMgr: Account [{}] grace whitelist expired, removed", it->first);
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
