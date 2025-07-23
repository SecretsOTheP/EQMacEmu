#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "../common/types.h"
#include "account_reservation_manager.h"  // Add AccountRezMgr
#include <map>
#include <string>
#include <vector>
#include <tuple>
#include <set> // Added for std::set
#include <memory> // Added for std::unique_ptr

// Forward declarations
struct QueuedClient;  // Defined in queue_manager.cpp
class WorldDatabase;  // From worlddb.h
class AccountRezMgr;
class Client;
struct UsertoWorldResponse;

namespace EQ {
	namespace Net {
		class Packet;
	}
}

enum class QueueDecisionOutcome {
	AutoConnect,        // Auto-connect bypass
	QueueToggle,        // Player was queued, remove them (toggle off)
	GMBypass,          // GM bypass - override capacity
	GraceBypass,       // Grace period bypass - override capacity  
	QueuePlayer        // Normal queueing - respect capacity decision
};

struct ConnectionRequest {
	uint32 account_id;
	uint32 ls_account_id;
	uint32 ip_address;
	int16 status;           // Account status (GM level, etc.)
	bool is_auto_connect;   // Is this an auto-connect vs manual PLAY?
	bool is_mule;          // Is this a mule account?
	const char* ip_str;
	const char* forum_name;
	uint32 world_account_id;
};

class QueueManager {
public:
	QueueManager();  // No longer needs WorldServer pointer
	~QueueManager();

	/**
	 * Core queue operations
	 */
	void AddToQueue(uint32 world_account_id, uint32 position, uint32 estimated_wait, uint32 ip_address, 
					uint32 ls_account_id, uint32 from_id, 
					const char* ip_str, const char* forum_name, const char* client_key = nullptr);
	void RemoveFromQueue(const std::vector<uint32>& account_ids);
	void RemoveFromQueue(uint32 account_id) { RemoveFromQueue(std::vector<uint32>{account_id}); } // Single account overload
	void UpdateQueuePositions();
	uint32 CalculateQueuePosition(uint32 account_id);
	
	/**
	 * Connection decision logic - handles -6 queue responses from world server
	 * Returns true if player should bypass queue (override -6), false if should be queued
	 */
	bool EvaluateConnectionRequest(const ConnectionRequest& request, uint32 max_capacity, 
	                               UsertoWorldResponse* response = nullptr, Client* client = nullptr);
	
	/**
	 * Queue queries
	 */
	bool IsAccountQueued(uint32 account_id) const;
	uint32 GetQueuePosition(uint32 account_id) const;
	uint32 GetTotalQueueSize() const;
	bool IsAccountInGraceWhitelist(uint32 account_id) const; // Check if account can bypass population cap
	
	/**
	 * Population management
	 */
	uint32 GetEffectivePopulation(); // World population + test offset
	uint32 GetWorldPopulation() const; // Just the world server population for logging
	
	/**
	 * Queue state management
	 */
	void SetPaused(bool paused) { m_queue_paused = paused; }
	bool IsPaused() const { return m_queue_paused; }
	
	/**
	 * Persistence operations
	 */
	// void SyncQueueToDatabase();
	void RestoreQueueFromDatabase();
	void CheckForExternalChanges(); // NEW: Check if database changed externally
	void PeriodicMaintenance();
	
	/**
	 * Enhanced maintenance operations - replaces login server iteration
	 */
	void RemovePlayerOnDisconnect(uint32 account_id); // Remove player when they disconnect from login server
	void ProcessAdvancementTimer(); // Enhanced queue management - handles population updates, DB sync, and advancement
	
	/**
	 * Database queue operations - moved from loginserver
	 */
	void SaveQueueEntry(uint32 account_id, uint32 world_server_id, uint32 queue_position, uint32 estimated_wait, uint32 ip_address);
	void RemoveQueueEntry(uint32 account_id, uint32 world_server_id);
	bool LoadQueueEntries(uint32 world_server_id, std::vector<std::tuple<uint32, uint32, uint32, uint32>>& queue_entries);
	
	/**
	 * Debugging and management
	 */
	void ClearAllQueues();
	
	/**
	 * Auto-connect functionality
	 */
	void AutoConnectQueuedPlayer(const QueuedClient& qclient);
	
	/**
	 * Targeted broadcast system - send updates only to queued clients
	 */
	void SendQueuedClientsUpdate() const;
	
	// Account reservation manager - initialized after database is ready
	std::unique_ptr<AccountRezMgr> m_account_rez_mgr;
	
	// Global effective population cache (updated by ProcessAdvancementTimer)
	uint32 g_effective_population;
	
	// Initialization method to be called after database is ready
	void Initialize_AccountRezMgr();
	
	// Constants
	static const uint32 queue_grace_period = 30; // 30 sec grace period for disconnected players
	
private:
	// Queue data
	std::vector<QueuedClient> m_queued_clients;  // Ordered queue - position = index + 1
	std::map<uint32, uint32> m_last_seen;             // account_id -> timestamp of last login server connection
	bool m_queue_paused;                              // Queue updates paused
	// std::string m_freeze_reason;
	
	// Cached server name for efficient logging
	std::string m_server_name;
	
	// Cached test offset for efficient population calculation
	uint32 m_cached_test_offset;
	
	// Helper functions
	void LogQueueAction(const std::string& action, uint32 account_id, const std::string& details = "") const;
	
	// Encapsulated database operations
	uint32 GetWorldAccountFromLS(uint32 ls_account_id) const;  // Wrapper for GetAccountIDFromLSID with fallback logic
	uint32 GetLSAccountFromWorld(uint32 world_account_id) const;  // Reverse mapping for dialogs and notifications
	
	// Database query helpers
	bool ExecuteQuery(const std::string& query, const std::string& operation_desc) const;
	uint32 QuerySingleUint32(const std::string& query, uint32 default_value = 0) const;
};

#endif // QUEUE_MANAGER_H 