#ifndef WORLD_QUEUE_H
#define WORLD_QUEUE_H

#include "../common/types.h"
#include "account_reservation_manager.h"  // Add AccountRezMgr
#include <map>
#include <string>
#include <vector>
#include <tuple>
#include <set> // Added for std::set
#include <memory> // Added for std::unique_ptr
#include <optional> // Added for std::optional
#include <type_traits> // Added for template metaprogramming

// Queue debug system - shared across all queue-related files
// 0 = off, 1 = important events only, 2 = verbose/noisy operations
#ifndef QUEUE_DEBUG_LEVEL
#define QUEUE_DEBUG_LEVEL 1 // TODO: Need to refine which msgs are @ which level
#endif

#define QueueDebugLog(level, fmt, ...) \
    do { if (QUEUE_DEBUG_LEVEL >= level) LogInfo(fmt, ##__VA_ARGS__); } while(0)

struct QueuedClient;  
class WorldDatabase;  
class AccountRezMgr;
class Client;
struct UsertoWorldResponse;
class QueueManager; 
extern QueueManager queue_manager;  // Global object - always valid, no null checks needed

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
	
	/**
	 * Population management
	 */
	uint32 EffectivePopulation(); // World population + test offset
	
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

	void ProcessAdvancementTimer(); // Enhanced queue management - handles population updates, DB sync, and advancement
	
	/**
	 * Database queue operations - moved from loginserver
	 */
	void SaveQueueDBEntry(uint32 account_id, uint32 queue_position, uint32 estimated_wait, uint32 ip_address);
	void RemoveQueueDBEntry(uint32 account_id);
	bool LoadQueueEntries(std::vector<std::tuple<uint32, uint32, uint32, uint32>>& queue_entries);
	
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
	
	// Account reservation manager - true POD safety (always callable)
	mutable AccountRezMgr m_account_rez_mgr;

private:
	// Database safety helper - checks if database is ready for operations
	bool ValidateDatabaseReady() const;
	
	// Queue data
	std::vector<QueuedClient> m_queued_clients;  // Ordered queue - position = index + 1
	std::map<uint32, uint32> m_last_seen;             // account_id -> timestamp of last login server connection
	bool m_queue_paused;                              // Queue updates paused
	// std::string m_freeze_reason;
	
	// Cached test offset for efficient population calculation
	uint32 m_cached_test_offset;
	
	// World server ID for database operations 
	uint32 m_world_server_id;
	
	// Helper functions
	void LogQueueAction(const std::string& action, uint32 account_id, const std::string& details = "") const;
	
	// Encapsulated database operations
	uint32 GetWorldAccountFromLS(uint32 ls_account_id) const;  // Wrapper for GetAccountIDFromLSID with fallback logic
	uint32 GetLSAccountFromWorld(uint32 world_account_id) const;  // Reverse mapping for dialogs and notifications
	
	// Database query helpers
	bool QueryDB(const std::string& query, const std::string& operation_desc) const;
	uint32 QuerySingleUint32(const std::string& query, uint32 default_value = 0) const;
	
	// ServerPacket helper methods to reduce code duplication
	void SendWorldListUpdate(uint32 effective_population);
	void SendQueuedClientUpdate(uint32 ls_account_id, uint32 queue_position, uint32 estimated_wait, uint32 ip_address);
	void SendQueueRemoval(uint32 ls_account_id);
	void SendQueueAutoConnect(const QueuedClient& qclient);

	// Helper methods for common packet sending patterns - simplified overloads
	template<typename T>
	void SendLoginServerPacket(uint16 opcode, const T& data);
	void SendLoginServerPacket(uint16 opcode, uint32 value);
	void SendLoginServerPacket(uint16 opcode);
	
	// Connection validation helper
	bool ValidateLoginServerConnection(uint16 opcode = 0) const;
};

// Global population accessor function
inline uint32 GetWorldPop() {
	return queue_manager.EffectivePopulation();
}

#endif // WORLD_QUEUE_H 