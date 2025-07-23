#include "queue_manager.h"
#include "worlddb.h"           // For WorldDatabase
#include "login_server.h"      // For LoginServer global
#include "world_config.h"      // For WorldConfig
#include "clientlist.h"        // For ClientList
#include "../common/eqemu_logsys.h"
#include "../common/servertalk.h"
#include "../common/queue_packets.h"  // Queue-specific opcodes and structures
#include "../common/rulesys.h"  // For RuleB and RuleI macros
#include "../common/ip_util.h"  // For IpUtil::IsIpInPrivateRfc1918
#include <fmt/format.h>
#include <arpa/inet.h>

// Toggle this to enable/disable verbose queue debugging (0 = off, 1 = on)
// Set to 1 and recompile to enable detailed queue operation logging
// Useful for debugging queue position updates, client connections, and server list updates
#define QUEUE_DEBUG_VERBOSE 1

#if QUEUE_DEBUG_VERBOSE
#define QueueDebugLog(fmt, ...) LogInfo(fmt, ##__VA_ARGS__)
#else
#define QueueDebugLog(fmt, ...) ((void)0)
#endif

extern LoginServer* loginserver; 
extern WorldDatabase database;  
extern ClientList client_list;  


struct QueuedClient {
	uint32 w_accountid;         // Primary key - world server account ID
	uint32 queue_position;
	uint32 estimated_wait;
	uint32 ip_address;
	uint32 queued_timestamp;
	uint32 last_updated;
	uint32 last_seen; // Timestamp for grace period tracking
	
	// Extended connection details for auto-connect
	uint32 ls_account_id;       // Login server account ID (for notifications)
	uint32 from_id;             // Connection type: 0=manual PLAY, 1=auto-connect
	std::string ip_str;         // String representation of IP
	std::string forum_name;     // Forum name or other identifier
	
	// Client authorization tracking for multiple client handling
	std::string authorized_client_key;  // Key of the specific client that was authorized
	
	QueuedClient() : w_accountid(0), queue_position(0), estimated_wait(0), ip_address(0),
		queued_timestamp(0), last_updated(0), last_seen(0), ls_account_id(0), 
		from_id(0) {}
	
	QueuedClient(uint32 world_acct_id, uint32 pos, uint32 wait, uint32 ip = 0, 
			   uint32 ls_acct_id = 0, uint32 f_id = 0, 
			   const std::string& ip_string = "", const std::string& forum = "",
			   const std::string& auth_key = "")
		: w_accountid(world_acct_id), queue_position(pos), estimated_wait(wait), ip_address(ip),
		  queued_timestamp(time(nullptr)), last_updated(time(nullptr)), last_seen(time(nullptr)),
		  ls_account_id(ls_acct_id), from_id(f_id), 
		  ip_str(ip_string), forum_name(forum),
		  authorized_client_key(auth_key) {}
};

QueueManager::QueueManager()
	: m_queue_paused(false), m_cached_test_offset(0)
{
	m_server_name = WorldConfig::get()->LongName;
	
	LogInfo("QueueManager constructed - AccountRezMgr initialization deferred until database ready");
	
	g_effective_population = UINT32_MAX;
}

void QueueManager::Initialize_AccountRezMgr()
{
	// Initialize AccountRezMgr after database is ready
	m_account_rez_mgr = std::make_unique<AccountRezMgr>();
	LogInfo("AccountRezMgr initialized - QueueManager ready for operation");
}

QueueManager::~QueueManager()
{
	LogInfo("QueueManager destroyed.");
}

void QueueManager::AddToQueue(uint32 world_account_id, uint32 position, uint32 estimated_wait, uint32 ip_address, 
							uint32 ls_account_id, uint32 from_id, 
							const char* ip_str, const char* forum_name, const char* client_key)
{
	in_addr addr;
	addr.s_addr = ip_address;
	std::string ip_str_log = inet_ntoa(addr);
	
	position = m_queued_clients.size() + 1;
	
	// TODO: Proper average calcuation here
	uint32 wait_per_player = 60;
	estimated_wait = position * wait_per_player;
	
	QueuedClient new_entry(world_account_id, position, estimated_wait, ip_address, 
								 ls_account_id, from_id,  // removed world_id parameter
								 ip_str ? ip_str : ip_str_log, 
								 forum_name ? forum_name : "",
								 client_key ? client_key : "");
	
	m_queued_clients.push_back(new_entry);
	
	// Mirror to database immediately (event-driven) - use fixed world server ID
	SaveQueueEntry(world_account_id, 1, position, estimated_wait, ip_address);
	LogQueueAction("ADD_TO_QUEUE", world_account_id, 
		fmt::format("pos={} wait={}s ip={} ls_id={} (memory + DB)", position, estimated_wait, ip_str_log, ls_account_id));
	
	 SendQueuedClientsUpdate();
	
}

uint32 QueueManager::CalculateQueuePosition(uint32 account_id) 
{
	// Calculate next queue position based on current queue size
	uint32 current_queue_size = GetTotalQueueSize();
	uint32 next_position = current_queue_size + 1;
	
	LogInfo("QueueManager [{}] - CALCULATE_POSITION: Account [{}] assigned position [{}] (current queue size: {})", 
		m_server_name, 
		account_id, next_position, current_queue_size);
	
	return next_position;
}
void QueueManager::RemoveFromQueue(const std::vector<uint32>& account_ids)
{
	if (account_ids.empty()) {
		return;
	}
	
	std::vector<uint32> removed_accounts;
	std::vector<std::pair<uint32, uint32>> notification_data; // {ls_account_id, ip_address}
	
	// Process each account for removal
	for (uint32 account_id : account_ids) {
		auto it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
			[account_id](const QueuedClient& qclient) {
				return qclient.w_accountid == account_id;
			});
			
		if (it == m_queued_clients.end()) {
			// Not found by direct lookup, try LS account ID mapping using encapsulated method
			uint32 world_account_id = GetWorldAccountFromLS(account_id);
			if (world_account_id != account_id) {  // Only search if we got a different mapping
				it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
					[world_account_id](const QueuedClient& qclient) {
						return qclient.w_accountid == world_account_id;
					});
			}
		}
		
		if (it != m_queued_clients.end()) {
			in_addr addr;
			addr.s_addr = it->ip_address;
			std::string ip_str = inet_ntoa(addr);
			
			// Store for notifications and removal
			uint32 account_id_to_remove = it->w_accountid;
			uint32 ls_account_id_to_notify = it->ls_account_id;
			uint32 ip_address_to_notify = it->ip_address;
			
			removed_accounts.push_back(account_id_to_remove);
			notification_data.emplace_back(ls_account_id_to_notify, ip_address_to_notify);
			
			// Remove from memory - vector automatically shifts remaining elements up
			m_queued_clients.erase(it);
			
			LogQueueAction("REMOVE", account_id_to_remove, 
				fmt::format("IP: {} (batch removal)", ip_str));
		}
	}
	
	if (removed_accounts.empty()) {
		LogDebug("RemoveFromQueue: No valid accounts found to remove from queue");
		return;
	}
	
	// Batch database removals for efficiency
	for (uint32 account_id : removed_accounts) {
		RemoveQueueEntry(account_id, 1); // Fixed server ID for world server
	}
	
	// Send individual removal notifications to disconnected players
	if (loginserver && loginserver->Connected()) {
		uint32 notifications_sent = 0;
		for (const auto& notification : notification_data) {
			uint32 ls_account_id = notification.first;
			uint32 ip_address = notification.second;
			
			if (ls_account_id > 0) {
				auto removal_pack = new ServerPacket(ServerOP_QueueDirectUpdate, sizeof(ServerQueueDirectUpdate_Struct));
				ServerQueueDirectUpdate_Struct* removal_update = (ServerQueueDirectUpdate_Struct*)removal_pack->pBuffer;
				
				removal_update->ls_account_id = ls_account_id;
				removal_update->ip_address = ip_address;
				removal_update->queue_position = 0; // Position 0 = removed from queue
				removal_update->estimated_wait = 0;
				
				loginserver->SendPacket(removal_pack);
				delete removal_pack;
				notifications_sent++;
			}
		}
		
		if (notifications_sent > 0) {
			LogInfo("Sent [{}] queue removal notifications to disconnected players", notifications_sent);
		}
	}
	
	// Send updated positions to remaining queued clients (batch update)
	if (!m_queued_clients.empty()) {
		 SendQueuedClientsUpdate();
		LogInfo("Removed [{}] players from queue, sent position updates to [{}] remaining clients", 
			removed_accounts.size(), m_queued_clients.size());
	} else {
		LogInfo("Removed [{}] players from queue - queue is now empty", removed_accounts.size());
	}
}
void QueueManager::UpdateQueuePositions()
{
	if (m_queued_clients.empty()) {
		return;
	}
	
	// Don't update queue positions if server is down/locked
	if (m_queue_paused) {
		LogInfo("Queue updates paused due to server status - [{}] players remain queued", m_queued_clients.size());
		return;
	}
	
	// Check if queue is manually frozen via rule
	if (RuleB(Quarm, FreezeQueue)) {
		LogInfo("Queue updates frozen by rule - [{}] players remain queued with frozen positions", m_queued_clients.size());
		return;
	}
	
	// Get server capacity for capacity decisions
	uint32 max_capacity = RuleI(Quarm, PlayerPopulationCap);
	uint32 current_population = GetEffectivePopulation();
	
	// Calculate available slots for auto-connects
	uint32 available_slots = (current_population < max_capacity) ? (max_capacity - current_population) : 0;
	uint32 auto_connects_initiated = 0;
	
	// Store accounts to remove after auto-connect (to avoid modifying vector during iteration)
	std::vector<uint32> accounts_to_remove;
	
	// Process players at front of queue for auto-connect
	// Use normal iteration since we'll remove accounts after the loop
	for (int i = 0; i < static_cast<int>(m_queued_clients.size()); ++i) {
		QueuedClient& qclient = m_queued_clients[i];
		uint32 current_position = i + 1; // Position is index + 1
		
		// Only process players at position 1 (front of queue)
		if (current_position == 1 && auto_connects_initiated < available_slots) {
			in_addr addr;
			addr.s_addr = qclient.ip_address;
			std::string ip_str = inet_ntoa(addr);
			
			LogQueueAction("FRONT_OF_QUEUE", qclient.w_accountid, 
				fmt::format("IP: {} reached position [{}] - auto-connecting with grace whitelist (total: {}/{}, slot {}/{})", 
					ip_str, current_position, current_population, max_capacity, auto_connects_initiated + 1, available_slots));
			
			// Send auto-connect request to login server
			if (qclient.ls_account_id > 0) {
				AutoConnectQueuedPlayer(qclient);
				// Mark for removal from queue after auto-connect
				accounts_to_remove.push_back(qclient.w_accountid);
			}
			
			auto_connects_initiated++;
		} else if (current_position == 1) {
			// Server at capacity - log but don't auto-connect
			in_addr addr;
			addr.s_addr = qclient.ip_address;
			std::string ip_str = inet_ntoa(addr);
			
			LogQueueAction("WAITING_CAPACITY", qclient.w_accountid, 
				fmt::format("IP: {} at position [{}] waiting for capacity (total: {}/{}, used slots: {}/{})", 
					ip_str, current_position, current_population, max_capacity, auto_connects_initiated, available_slots));
		}
	}
	
	// Remove auto-connected players from queue
	for (uint32 account_id : accounts_to_remove) {
		RemoveFromQueue(account_id);
		LogInfo("Removed account [{}] from queue after auto-connect", account_id);
	}
	
	if (auto_connects_initiated > 0) {
		LogInfo("Auto-connected [{}] players from queue to grace whitelist - [{}] players remain queued", 
			auto_connects_initiated, m_queued_clients.size());
		
		// Send position updates to remaining queued clients
		SendQueuedClientsUpdate();
	}
}

bool QueueManager::EvaluateConnectionRequest(const ConnectionRequest& request, uint32 max_capacity,
                                            UsertoWorldResponse* response, Client* client)
{
	QueueDecisionOutcome decision = QueueDecisionOutcome::QueuePlayer; // Default to queueing
	
	// 1. Auto-connects always bypass (shouldn't get -6, but just in case)
	if (request.is_auto_connect) {
		LogInfo("QueueManager [{}] - AUTO_CONNECT: Account [{}] bypassing queue evaluation", 
			m_server_name, request.account_id);
		decision = QueueDecisionOutcome::AutoConnect;
	}
	// 2. Check if player is already queued (queue toggle behavior)
	else if (IsAccountQueued(request.account_id)) {
		uint32 queue_position = GetQueuePosition(request.account_id);
		LogInfo("QueueManager [{}] - QUEUE_TOGGLE: Account [{}] already queued at position [{}] - toggling off", 
			m_server_name, request.account_id, queue_position);
		decision = QueueDecisionOutcome::QueueToggle;
	}
	// 3. Check GM bypass rules - this is where we override world server's decision
	else if (RuleB(Quarm, QueueBypassGMLevel) && request.status >= 80) {
		LogInfo("QueueManager [{}] - GM_BYPASS: Account [{}] (status: {}) overriding world server capacity decision", 
			m_server_name, request.account_id, request.status);
		decision = QueueDecisionOutcome::GMBypass;
	}
	// 4. Check grace period whitelist (disconnected players still within grace period)
	else if (IsAccountInGraceWhitelist(request.account_id)) {
		LogInfo("QueueManager [{}] - GRACE_BYPASS: Account [{}] in grace period whitelist - overriding capacity check", 
			m_server_name, request.account_id);
		decision = QueueDecisionOutcome::GraceBypass;
	}
	// 5. Default case - no bypass conditions met, queue the player
	else {
		LogInfo("QueueManager [{}] - QUEUE_PLAYER: Account [{}] at capacity with no bypass conditions - adding to queue", 
			m_server_name, request.account_id);
		decision = QueueDecisionOutcome::QueuePlayer;
	}
	
	// Handle the decision outcome
	switch (decision) {
		case QueueDecisionOutcome::AutoConnect:
		case QueueDecisionOutcome::GMBypass:
		case QueueDecisionOutcome::GraceBypass:
			// These cases override the -6 and allow connection
			// For grace bypass, also create authorization token to prevent auth errors
			// if (decision == QueueDecisionOutcome::GraceBypass) {
			// 	// Create minimal entry for authorization - reuse existing AutoConnectQueuedPlayer logic
			// 	QueueManagerEntry grace_entry;
			// 	grace_entry.ls_account_id = request.ls_account_id;
			// 	grace_entry.ip_address = request.ip_address;
			// 	grace_entry.from_id = 0; // Manual PLAY, not auto-connect
			// 	grace_entry.forum_name = request.forum_name ? request.forum_name : "";
			// 	grace_entry.authorized_client_key = ""; // No specific client key needed for grace bypass
			// 	AutoConnectQueuedPlayer(grace_entry); // Create auth entry for grace bypass
			// }
			return true;
			
		case QueueDecisionOutcome::QueueToggle:
			RemoveFromQueue(request.account_id);
			LogInfo("QUEUE TOGGLE: Player clicked PLAY while queued - removed account [{}] from server [{}]", request.account_id, m_server_name);
			if (response) {
				response->response = -7; // Queue toggle response code for login server
			}
			return false;
			
		case QueueDecisionOutcome::QueuePlayer:
			// Add to queue for this server
			{
				uint32 queue_position = CalculateQueuePosition(request.world_account_id);
				uint32 estimated_wait = queue_position * 60; // 60 seconds per position default
				
				// Use the client key from the login server request (passed via forum_name field)
				std::string client_key = request.forum_name ? request.forum_name : "";
				
				AddToQueue(
					request.world_account_id,        // world_account_id (primary key)
					queue_position,                  // position
					estimated_wait,                  // estimated_wait  
					request.ip_address,              // ip_address
					request.ls_account_id,          // ls_account_id
					response ? response->FromID : 0, // from_id
					request.ip_str,                  // ip_str
					request.forum_name,              // forum_name
					client_key.c_str()               // authorized_client_key (use forum_name as client_key)
				);
				
				LogInfo("Added account [{}] to queue at position [{}] with estimated wait [{}] seconds (client key: {})", 
					request.world_account_id, queue_position, estimated_wait, 
					client_key.empty() ? "NONE" : "present");
				
				if (response) {
					response->response = -6; // Queue response code for login server
				}
				return false; // Don't override -6, player should remain queued
			}
	}
	
	// Should never reach here, but default to not overriding
	return false;
}

bool QueueManager::IsAccountQueued(uint32 account_id) const
{
	// First check if it's a direct world account ID lookup
	auto it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
		[account_id](const QueuedClient& qclient) {
			return qclient.w_accountid == account_id;
		});
	if (it != m_queued_clients.end()) {
		return true;
	}
	
	// If not found, check if it's an LS account ID that needs mapping using encapsulated method
	uint32 world_account_id = GetWorldAccountFromLS(account_id);
	if (world_account_id != account_id) {
		auto world_it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
			[world_account_id](const QueuedClient& qclient) {
				return qclient.w_accountid == world_account_id;
			});
		return world_it != m_queued_clients.end();
	}
	
	return false;
}


uint32 QueueManager::GetQueuePosition(uint32 account_id) const
{
	// First check if it's a direct world account ID lookup
	auto it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
		[account_id](const QueuedClient& qclient) {
			return qclient.w_accountid == account_id;
		});
	if (it != m_queued_clients.end()) {
		return std::distance(m_queued_clients.begin(), it) + 1; // Position = index + 1
	}
	
	// If not found, check if it's an LS account ID that needs mapping using encapsulated method
	uint32 world_account_id = GetWorldAccountFromLS(account_id);
	if (world_account_id != account_id) {  // Only search if we got a different mapping
		auto world_it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
			[world_account_id](const QueuedClient& qclient) {
				return qclient.w_accountid == world_account_id;
			});
		if (world_it != m_queued_clients.end()) {
			return std::distance(m_queued_clients.begin(), world_it) + 1; // Position = index + 1
		}
	}
	
	return 0;
}

uint32 QueueManager::GetTotalQueueSize() const
{
	return static_cast<uint32>(m_queued_clients.size());
}
void QueueManager::CheckForExternalChanges() // Handles test offset changes and queue refresh flags
{
	static bool first_run = true;
	static const std::string test_offset_query = "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset' LIMIT 1";
	uint32 current_test_offset = QuerySingleUint32(test_offset_query, 0);
	
	QueueDebugLog("Current test_offset: {}, cached_test_offset: {}", current_test_offset, m_cached_test_offset);
	
	if (first_run || m_cached_test_offset != current_test_offset) {
		if (!first_run) {
			LogInfo("Test offset changed from [{}] to [{}] - pushing server list updates to all login clients", 
				m_cached_test_offset, current_test_offset);
		}
		
		// Update cached value
		m_cached_test_offset = current_test_offset;
		
		if (!first_run) {
			// Get current effective population using the standard method
			uint32 effective_population = GetEffectivePopulation();
			
			// Send server list update to all connected login server clients
			if (loginserver && loginserver->Connected()) {
				QueueDebugLog("Login server is connected - sending ServerOP_WorldListUpdate packet");
				
				// Send the current effective population with the update so login server can update cache immediately
				auto update_pack = new ServerPacket(ServerOP_WorldListUpdate, sizeof(uint32));
				*((uint32*)update_pack->pBuffer) = effective_population;
				
				QueueDebugLog("Created packet - opcode: 0x{:X}, size: {}, population: {}", 
					update_pack->opcode, update_pack->size, effective_population);
				loginserver->SendPacket(update_pack);
				QueueDebugLog("SendPacket() called successfully");
				delete update_pack;
				LogInfo("Sent ServerOP_WorldListUpdate to login server for test offset change with population: {}", effective_population);
			} else {
				QueueDebugLog("Login server NOT connected - cannot send update packet");
			}
		}
		
		first_run = false;
	}
	
	static const std::string refresh_queue_query = 
		"SELECT value FROM tblloginserversettings WHERE type = 'RefreshQueue' ORDER BY value DESC LIMIT 1";
	static const std::string reset_queue_flag_query = 
		"UPDATE tblloginserversettings SET value = '0' WHERE type = 'RefreshQueue'";
		
	auto results = database.QueryDatabase(refresh_queue_query);
	if (results.Success() && results.RowCount() > 0) {
		auto row = results.begin();
		std::string flag_value = row[0] ? row[0] : "0";
		
		// If flag is anything other than "0", process it and reset to "0"
		if (flag_value != "0") {
			bool should_refresh = (flag_value == "1" || flag_value == "true");
			
			if (should_refresh) {
				LogInfo("Queue refresh flag detected - refreshing queue from database");
				RestoreQueueFromDatabase();
				LogInfo("Queue refreshed from database - clients updated");
			} else {
				LogInfo("RefreshQueue flag value = '{}' - resetting to 0", flag_value);
			}
			
			// Always reset the flag to 0, regardless of the value
			auto reset_result = database.QueryDatabase(reset_queue_flag_query);
			
			if (reset_result.Success()) {
				LogInfo("RefreshQueue flag reset to 0");
			} else {
				LogError("Failed to reset RefreshQueue flag: {}", reset_result.ErrorMessage());
			}
		}
	} else {
		if (!results.Success()) {
			LogError("CheckForExternalChanges: Query failed - {}", results.ErrorMessage());
		} else {
			LogDebug("CheckForExternalChanges: No RefreshQueue flag found in database");
		}
	}
}

void QueueManager::PeriodicMaintenance()
{
	// Maintain account reservations
	if (m_account_rez_mgr) {
		m_account_rez_mgr->PeriodicMaintenance();
	}
	CheckForExternalChanges(); // DB changes via admin script	
}

void QueueManager::ClearAllQueues()
{
	if (!m_queued_clients.empty()) {
		uint32 count = GetTotalQueueSize();
		LogInfo("Clearing all queue entries for world server [{}] - removing [{}] players", 
			m_server_name, count);
		
		// 1. Clear memory
		m_queued_clients.clear();
		
		// 2. Clear database immediately (event-driven)
		uint32 server_id = 1; // Fixed server ID for world server
		auto clear_query = fmt::format("DELETE FROM tblLoginQueue WHERE world_server_id = {}", server_id);
		auto result = database.QueryDatabase(clear_query);
		
		if (result.Success()) {
			LogInfo("Queue cleared - [{}] players removed (memory + DB)", count);
		} else {
			LogError("Queue cleared from memory but failed to clear database: {}", result.ErrorMessage());
		}
	}
}


// Private helper functions

// Encapsulated database operations
uint32 QueueManager::GetWorldAccountFromLS(uint32 ls_account_id) const
{
	uint32 world_account_id = database.GetAccountIDFromLSID(ls_account_id);
	if (world_account_id == 0) {
		// For new accounts that don't have world accounts yet, use LS account ID as fallback
		LogDebug("No world account mapping for LS account [{}] - using LS account ID as fallback", ls_account_id);
		return ls_account_id;
	}
	return world_account_id;
}

uint32 QueueManager::GetLSAccountFromWorld(uint32 world_account_id) const
{
	auto query = fmt::format("SELECT lsaccount_id FROM account WHERE id = {} LIMIT 1", world_account_id);
	uint32 ls_account_id = QuerySingleUint32(query, 0);
	
	if (ls_account_id > 0) {
		return ls_account_id;
	}
	
	// Fallback: assume world_account_id is actually the LS account ID for new accounts
	LogDebug("No LS account mapping for world account [{}] - using world account ID as fallback", world_account_id);
	return world_account_id;
}

// Database query helpers
bool QueueManager::ExecuteQuery(const std::string& query, const std::string& operation_desc) const
{
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		LogError("Failed to {}: {}", operation_desc, results.ErrorMessage());
		return false;
	}
	return true;
}

uint32 QueueManager::QuerySingleUint32(const std::string& query, uint32 default_value) const
{
	auto results = database.QueryDatabase(query);
	if (results.Success() && results.RowCount() > 0) {
		auto row = results.begin();
		if (row[0]) {
			try {
				return static_cast<uint32>(std::stoul(row[0]));
			} catch (const std::exception&) {
				LogError("Failed to parse uint32 from query result: {}", row[0]);
			}
		}
	}
	return default_value;
}

void QueueManager::LogQueueAction(const std::string& action, uint32 account_id, const std::string& details) const
{
	std::string server_name = m_server_name; // Use cached member variable
	LogInfo("QueueManager [{}] - {}: Account [{}] {}", server_name, action, account_id, details);
}

void QueueManager::SendQueuedClientsUpdate() const {
	QueueDebugLog("SendQueuedClientsUpdate called - checking queue state...");
	
	if (m_queued_clients.empty()) {
		QueueDebugLog("No queued players to update");
		return;
	}
	
	if (!loginserver || !loginserver->Connected()) {
		LogError("Login server not available - cannot send queue updates");
		return;
	}
	
	QueueDebugLog("Found [{}] queued players, login server available - sending batch update", m_queued_clients.size());
	
	std::vector<ServerQueueDirectUpdate_Struct> updates;
	updates.reserve(m_queued_clients.size());
	
	uint32 valid_updates = 0;
	uint32 skipped_invalid = 0;
	
	// Collect all valid queue updates with dynamic position calculation
	for (size_t i = 0; i < m_queued_clients.size(); ++i) {
		const QueuedClient& qclient = m_queued_clients[i];
		uint32 dynamic_position = i + 1; // Position = index + 1
		
		// Create update entry
		ServerQueueDirectUpdate_Struct update = {};
		update.ls_account_id = qclient.ls_account_id;
		update.queue_position = dynamic_position;  // Use calculated position
		update.estimated_wait = dynamic_position * 60;  // Use calculated wait time
		
		updates.push_back(update);
		valid_updates++;
		
		QueueDebugLog("Added to batch: account [{}] (LS: {}) position [{}] wait [{}]s", 
			qclient.w_accountid, qclient.ls_account_id, dynamic_position, update.estimated_wait);
	}
	
	if (valid_updates == 0) {
		QueueDebugLog("No valid queue updates to send");
		return;
	}
	
	// Create batch packet: header + array of updates
	size_t packet_size = sizeof(ServerQueueBatchUpdate_Struct) + (valid_updates * sizeof(ServerQueueDirectUpdate_Struct));
	auto batch_pack = new ServerPacket(ServerOP_QueueBatchUpdate, packet_size);
	
	// Set update count
	ServerQueueBatchUpdate_Struct* batch_header = (ServerQueueBatchUpdate_Struct*)batch_pack->pBuffer;
	batch_header->update_count = valid_updates;
	
	// Copy update array after header
	ServerQueueDirectUpdate_Struct* update_array = (ServerQueueDirectUpdate_Struct*)(batch_pack->pBuffer + sizeof(ServerQueueBatchUpdate_Struct));
	memcpy(update_array, updates.data(), valid_updates * sizeof(ServerQueueDirectUpdate_Struct));
	
	QueueDebugLog("Created batch packet - opcode: 0x{:X}, size: {}, update count: {}", 
		ServerOP_QueueBatchUpdate, packet_size, valid_updates);
	
	// Send single batch packet to login server
	loginserver->SendPacket(batch_pack);
	delete batch_pack;
	
	LogInfo("Sent batch queue update with [{}] player updates to login server (skipped: {})", 
		valid_updates, skipped_invalid);
	
	QueueDebugLog("SendQueuedClientsUpdate completed - batch sent: {}, skipped: {}, total queued: {}", 
		valid_updates, skipped_invalid, m_queued_clients.size());
}

bool QueueManager::IsAccountInGraceWhitelist(uint32 account_id) const
{
	if (!m_account_rez_mgr) {
		LogDebug("QueueManager: AccountRezMgr not initialized yet - cannot check grace whitelist");
		return false;
	}
	
	bool is_whitelisted = m_account_rez_mgr->IsAccountInGraceWhitelist(account_id);
	
	if (is_whitelisted) {
		LogInfo("QueueManager [{}] - GRACE_WHITELIST: Account [{}] found in grace period whitelist", 
			m_server_name, account_id);
	}
	
	return is_whitelisted;
}

void QueueManager::AutoConnectQueuedPlayer(const QueuedClient& qclient)
{
	if (!loginserver || !loginserver->Connected()) {
		LogError("Cannot auto-connect queued player - login server not available or not connected");
		return;
	}
	
	LogInfo("AUTO-CONNECT: Sending auto-connect trigger for LS account [{}]", qclient.ls_account_id);
	
	// Convert IP to string if needed
	in_addr addr;
	addr.s_addr = qclient.ip_address;
	char ip_str_buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr, ip_str_buf, INET_ADDRSTRLEN);
	
	// Send auto-connect signal to login server to trigger the client connection
	auto autoconnect_pack = new ServerPacket(ServerOP_QueueAutoConnect, sizeof(ServerQueueAutoConnect_Struct));
	ServerQueueAutoConnect_Struct* sqac = (ServerQueueAutoConnect_Struct*)autoconnect_pack->pBuffer;
	
	sqac->loginserver_account_id = qclient.ls_account_id;
	sqac->world_id = 0;  // Not used
	sqac->from_id = qclient.from_id;
	sqac->to_id = 0;  // Not used  
	sqac->ip_address = qclient.ip_address;
	
	strncpy(sqac->ip_addr_str, ip_str_buf, sizeof(sqac->ip_addr_str) - 1);
	sqac->ip_addr_str[sizeof(sqac->ip_addr_str) - 1] = '\0';
	
	strncpy(sqac->forum_name, qclient.forum_name.c_str(), sizeof(sqac->forum_name) - 1);
	sqac->forum_name[sizeof(sqac->forum_name) - 1] = '\0';
	
	// CRITICAL: Include the authorized client key for specific client targeting
	strncpy(sqac->client_key, qclient.authorized_client_key.c_str(), sizeof(sqac->client_key) - 1);
	sqac->client_key[sizeof(sqac->client_key) - 1] = '\0';
	
	LogInfo("AUTO-CONNECT: Added account [{}] to grace whitelist for population cap bypass", qclient.ls_account_id);
	m_account_rez_mgr->AddRez(qclient.w_accountid, qclient.ip_address, 30);
	
	// Send to login server to trigger active connection
	LogInfo("AUTO-CONNECT: Sending ServerOP_QueueAutoConnect to loginserver for account [{}]", qclient.ls_account_id);
	loginserver->SendPacket(autoconnect_pack);
	delete autoconnect_pack;
}
uint32 QueueManager::GetEffectivePopulation()
{
// TODO: Add bypass logic for trader + GM accounts
	
	if (!m_account_rez_mgr) {
		LogError("AccountRezMgr not initialized yet - using actual client count as fallback");
		// Fallback to actual connected client count + test offset
		uint32 actual_clients = client_list.GetClientCount();
		uint32 test_offset = m_cached_test_offset;
		uint32 fallback_population = actual_clients + test_offset;
		LogInfo("Fallback population: {} actual clients + {} test offset = {}", 
			actual_clients, test_offset, fallback_population);
		return fallback_population;
	}
	
	// Base population from account reservations
	uint32 base_population = m_account_rez_mgr->Total();
	
	// Use cached test offset instead of database query for better performance
	uint32 test_offset = m_cached_test_offset;
	
	// Calculate final effective population
	uint32 effective_population = base_population + test_offset;
	
	LogInfo("Account reservations: {}, test offset: {}, effective total: {}", 
		base_population, test_offset, effective_population);
	
	return effective_population;
}

// DATABASE QUEUE OPERATIONS

void QueueManager::SaveQueueEntry(uint32 account_id, uint32 world_server_id, uint32 queue_position, uint32 estimated_wait, uint32 ip_address)
{
	auto query = fmt::format(
		"REPLACE INTO tblLoginQueue (account_id, world_server_id, queue_position, estimated_wait, ip_address) "
		"VALUES ({}, {}, {}, {}, {})",
		account_id, world_server_id, queue_position, estimated_wait, ip_address
	);

	ExecuteQuery(query, fmt::format("save queue entry for account {} on world server {}", account_id, world_server_id));
}

void QueueManager::RemoveQueueEntry(uint32 account_id, uint32 world_server_id)
{
	auto query = fmt::format(
		"DELETE FROM tblLoginQueue WHERE account_id = {} AND world_server_id = {}",
		account_id, world_server_id
	);

	ExecuteQuery(query, fmt::format("remove queue entry for account {} on world server {}", account_id, world_server_id));
}

bool QueueManager::LoadQueueEntries(uint32 world_server_id, std::vector<std::tuple<uint32, uint32, uint32, uint32>>& queue_entries)
{
	auto query = fmt::format(
		"SELECT account_id, queue_position, estimated_wait, ip_address "
		"FROM tblLoginQueue WHERE world_server_id = {} "
		"ORDER BY queue_position ASC",
		world_server_id
	);

	auto results = database.QueryDatabase(query);  // Use world server global database
	if (!results.Success()) {
		LogError("Failed to load queue entries for world server {}", world_server_id);
		return false;
	}

	queue_entries.clear();
	for (auto row = results.begin(); row != results.end(); ++row) {
		uint32 account_id = atoi(row[0]);
		uint32 queue_position = atoi(row[1]);
		uint32 estimated_wait = atoi(row[2]);
		uint32 ip_address = atoi(row[3]);
		
		queue_entries.emplace_back(account_id, queue_position, estimated_wait, ip_address);
	}

	LogInfo("Loaded {} queue entries for world server {}", queue_entries.size(), world_server_id);
	return true;
}

void QueueManager::RemovePlayerOnDisconnect(uint32 account_id)
{
	// Remove player from this world server's queue when they disconnect from login server
	if (IsAccountQueued(account_id)) {
		RemoveFromQueue(account_id);
		LogInfo("Removed account [{}] from queue due to disconnection from login server", account_id);
	}
}


void QueueManager::ProcessAdvancementTimer()
{
	// Enhanced queue management when enabled
	if (RuleB(Quarm, EnableQueue)) {
		// Log current queue size
		uint32 queue_size = GetTotalQueueSize();
		LogDebug("Queue Status: {} players currently in queue", queue_size);
		uint32 effective_population = GetEffectivePopulation();
		
		// Store in cache for SendStatus() to read
		g_effective_population = effective_population;
		
		// Update server_population table for monitoring/debugging
		static const std::string server_population_query_template = 
			"INSERT INTO server_population (server_id, effective_population) "
			"VALUES (1, {}) ON DUPLICATE KEY UPDATE "
			"effective_population = {}, last_updated = NOW()";
		auto query = fmt::format(fmt::runtime(server_population_query_template), effective_population, effective_population);
		auto result = database.QueryDatabase(query);
		if (!result.Success()) {
			LogDebug("Failed to update server_population table: {}", result.ErrorMessage());
		}
		
		// Send real-time update to login server cache ONLY if population changed
		if (loginserver && loginserver->Connected()) {
			static uint32 last_sent_population = UINT32_MAX; // Track last sent value
			
			if (effective_population != last_sent_population) {
				auto update_pack = new ServerPacket(ServerOP_WorldListUpdate, sizeof(uint32));
				*((uint32*)update_pack->pBuffer) = effective_population;
				loginserver->SendPacket(update_pack);
				delete update_pack;
				LogDebug("Sent real-time ServerOP_WorldListUpdate with population: {} (changed from {})", 
					effective_population, last_sent_population);
				
				last_sent_population = effective_population; // Update tracking variable
			}
		}
	} else {
		// Queue disabled - invalidate cache
		g_effective_population = UINT32_MAX;
	}
	
	// Queue advancement logic
	UpdateQueuePositions();
	PeriodicMaintenance();
}

// Admin Queue Management Methods

// void QueueManager::FreezeQueue(const std::string& reason)
// {
// 	m_queue_paused = true;
// 	m_freeze_reason = reason.empty() ? "Admin freeze" : reason;
	
// 	LogInfo("Queue frozen by admin - Reason: {}", m_freeze_reason);
	
// 	// Announce to all queued players
// 	std::string message = fmt::format("QUEUE FROZEN\n\nReason: {}\n\nYour position is preserved.", m_freeze_reason);
// 	SendDialogToAllQueuedPlayers(message);
// }

// void QueueManager::UnfreezeQueue()
// {
// 	if (!m_queue_paused) {
// 		LogInfo("Queue unfreeze requested but queue was not frozen");
// 		return;
// 	}
	
// 	std::string old_reason = m_freeze_reason;
// 	m_queue_paused = false;
// 	m_freeze_reason = "";
	
// 	LogInfo("Queue unfrozen by admin - Previous reason: {}", old_reason);
	
// 	// Announce to all queued players  
// 	std::string message = "QUEUE RESUMED\n\nThe queue is now active again.\nThank you for your patience.";
// 	SendDialogToAllQueuedPlayers(message);
	
// 	// Force immediate queue position update
// 	SendQueuedClientsUpdate();
// }


void QueueManager::RestoreQueueFromDatabase()
{
	// Check if queue persistence is enabled
	if (!RuleB(Quarm, EnableQueuePersistence)) {
		LogInfo("Queue persistence disabled - clearing old queue entries for world server [{}]", 1);
		auto clear_query = fmt::format("DELETE FROM tblLoginQueue WHERE world_server_id = {}", 1);
		database.QueryDatabase(clear_query);  // Use global database
		return;
	}
	
	LogInfo("Restoring queue from database for world server [{}]", 1);
	
	// Load queue entries from database
	std::vector<std::tuple<uint32, uint32, uint32, uint32>> queue_entries;
	if (!LoadQueueEntries(1, queue_entries)) { // Fixed server ID for world server
		LogError("Failed to load queue entries from database");
		return;
	}
	
	// Restore queue entries to memory
	m_queued_clients.clear();
	uint32 restored_count = 0;
	
	for (const auto& entry_tuple : queue_entries) {
		uint32 world_account_id = std::get<0>(entry_tuple);  // This should be world account ID
		uint32 queue_position = std::get<1>(entry_tuple);
		uint32 estimated_wait = std::get<2>(entry_tuple);
		uint32 ip_address = std::get<3>(entry_tuple);
		
		// Skip entries with invalid world account IDs
		if (world_account_id == 0) {
			LogInfo("QueueManager [{}] - SKIP: Invalid world account ID [0] during restoration", 
				m_server_name);
			continue;
		}
		
		QueuedClient entry;
		entry.w_accountid = world_account_id;
		entry.queue_position = queue_position;
		entry.estimated_wait = estimated_wait;
		entry.ip_address = ip_address;
		entry.queued_timestamp = time(nullptr); // Current time for restored entries
		entry.last_updated = time(nullptr);
		
		// For restored entries, we don't have LS account ID or extended connection details
		entry.ls_account_id = 0; // Unknown for restored entries
		entry.from_id = 0;
		entry.ip_str = "";
		entry.forum_name = "";
		
		// Use world account ID as map key (consistent with runtime behavior)
		m_queued_clients[world_account_id] = entry;
		restored_count++;
		
		LogInfo("QueueManager [{}] - RESTORE: World account [{}] at position [{}] with wait [{}] - persistent queue entry restored", 
			m_server_name, 
			world_account_id, queue_position, estimated_wait);
	}
	
	if (restored_count > 0) {
		LogInfo("Restored [{}] persistent queue entries from database for world server [{}]", 
			restored_count, 1);
		LogInfo("NOTE: Restored entries use world account IDs only - LS account mapping will be established when players reconnect");
	} else {
		LogInfo("No queue entries to restore for world server [{}]", 1);
	}
}
// TODO: Implement database sync/restore methods
// void QueueManager::SyncQueueToDatabase()
// {
// 	uint32 server_id = 1; // Fixed server ID for world server
// 	Database* db = &database;
// 	if (!db) {
// 		LogError("Cannot sync queue to database - database not available");
// 		return;
// 	}
	
// 	LogInfo("Syncing queue to database for world server [{}] with [{}] entries", 
// 		server_id, m_queued_clients.size());
	
// 	// Clear existing entries for this world server
// 	auto clear_query = fmt::format("DELETE FROM tblLoginQueue WHERE world_server_id = {}", server_id);
// 	database.QueryDatabase(clear_query);
	
// 	// Save all current queue entries
// 	for (const auto& pair : m_queued_clients) {
// 		const QueueManagerEntry& entry = pair.second;
// 		SaveQueueEntry(
// 			entry.account_id,
// 			server_id,
// 			entry.queue_position,
// 			entry.estimated_wait,
// 			entry.ip_address
// 		);
// 	}
	
// 	LogInfo("Queue sync completed for world server [{}]", server_id);
// }
// void QueueManager::SaveQueueSnapshot(const std::string& snapshot_name)
// {
// 	// First clear any existing snapshot with this name
// 	auto clear_query = fmt::format(
// 		"DELETE FROM queue_snapshots WHERE snapshot_name = '{}'", 
// 		snapshot_name
// 	);
// 	ExecuteQuery(clear_query, fmt::format("clear existing snapshot {}", snapshot_name));
	
// 	// Save current queue state
// 	uint32 current_time = time(nullptr);
// 	for (const auto& pair : m_queued_clients) {
// 		const QueueManagerEntry& entry = pair.second;
		
// 		auto save_query = fmt::format(
// 			"INSERT INTO queue_snapshots (snapshot_name, account_id, queue_position, estimated_wait, ip_address, ls_account_id, created_at) "
// 			"VALUES ('{}', {}, {}, {}, {}, {}, {})",
// 			snapshot_name, entry.account_id, entry.queue_position, entry.estimated_wait, 
// 			entry.ip_address, entry.ls_account_id, current_time
// 		);
		
// 		ExecuteQuery(save_query, fmt::format("save queue snapshot {} entry", snapshot_name));
// 	}
	
// 	LogInfo("Saved queue snapshot '{}' with [{}] entries", snapshot_name, m_queued_clients.size());
// }

// bool QueueManager::RestoreQueueSnapshot(const std::string& snapshot_name)
// {
// 	auto query = fmt::format(
// 		"SELECT account_id, queue_position, estimated_wait, ip_address, ls_account_id "
// 		"FROM queue_snapshots WHERE snapshot_name = '{}' ORDER BY queue_position ASC",
// 		snapshot_name
// 	);
	
// 	auto results = database.QueryDatabase(query);
// 	if (!results.Success()) {
// 		LogError("Failed to restore queue snapshot '{}': {}", snapshot_name, results.ErrorMessage());
// 		return false;
// 	}
	
// 	if (results.RowCount() == 0) {
// 		LogInfo("Queue snapshot '{}' not found or empty", snapshot_name);
// 		return false;
// 	}
	
// 	// Clear current queue
// 	ClearAllQueues();
	
// 	// Restore entries
// 	uint32 restored_count = 0;
// 	for (auto row = results.begin(); row != results.end(); ++row) {
// 		uint32 account_id = atoi(row[0]);
// 		uint32 queue_position = atoi(row[1]);
// 		uint32 estimated_wait = atoi(row[2]);
// 		uint32 ip_address = atoi(row[3]);
// 		uint32 ls_account_id = atoi(row[4]);
		
// 		QueueManagerEntry entry;
// 		entry.account_id = account_id;
// 		entry.queue_position = queue_position;
// 		entry.estimated_wait = estimated_wait;
// 		entry.ip_address = ip_address;
// 		entry.ls_account_id = ls_account_id;
// 		entry.queued_timestamp = time(nullptr);
// 		entry.last_updated = time(nullptr);
// 		entry.last_seen = time(nullptr);
		
// 		m_queued_clients[account_id] = entry;
// 		SaveQueueEntry(account_id, 1, queue_position, estimated_wait, ip_address);
// 		restored_count++;
// 	}
	
// 	LogInfo("Restored queue snapshot '{}' with [{}] entries", snapshot_name, restored_count);
	
// 	// Send updates to all restored players
// 	SendQueuedClientsUpdate();
	
// 	return true;
// }

// std::vector<std::string> QueueManager::GetQueueSnapshots() const
// {
// 	std::vector<std::string> snapshots;
	
// 	auto query = "SELECT DISTINCT snapshot_name FROM queue_snapshots ORDER BY snapshot_name";
// 	auto results = database.QueryDatabase(query);
	
// 	if (results.Success()) {
// 		for (auto row = results.begin(); row != results.end(); ++row) {
// 			if (row[0]) {
// 				snapshots.push_back(row[0]);
// 			}
// 		}
// 	}
	
// 	return snapshots;
// }

// DATABASE QUEUE OPERATIONS 