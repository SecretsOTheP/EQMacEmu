#include "world_queue.h"
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
#ifndef _WINDOWS
#include <arpa/inet.h>
#endif
#include <set>
#include <unordered_set> // Added for std::unordered_set
#include <algorithm>     // Added for std::find_if

extern LoginServer* loginserver; 
extern WorldDatabase database;  
extern ClientList client_list;  
extern bool database_ready;  // Add extern declaration for database_ready flag

struct QueuedClient {
	uint32 w_accountid;         
	uint32 queue_position;
	uint32 estimated_wait;
	uint32 ip_address;
	uint32 queued_timestamp;
	uint32 last_updated;
	uint32 last_seen; 
	
	uint32 ls_account_id;       
	uint32 from_id;             
	std::string ip_str;         
	std::string forum_name;     
	
	std::string authorized_client_key;  
	
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
	: m_queue_paused(false), m_cached_test_offset(0), m_world_server_id(1)
{
}

QueueManager::~QueueManager()
{
}
uint32 QueueManager::EffectivePopulation()
{
	uint32 account_reservations = m_account_rez_mgr.Total();
	uint32 test_offset = m_cached_test_offset;
	uint32 effective_population = account_reservations + test_offset;

	QueueDebugLog(2, "Account reservations: {}, test offset: {}, queue size: {}, effective total: {}", 
		account_reservations, test_offset, m_queued_clients.size(), effective_population);
	
	return effective_population;
}

void QueueManager::AddToQueue(uint32 world_account_id, uint32 position, uint32 estimated_wait, uint32 ip_address, 
							uint32 ls_account_id, uint32 from_id, 
							const char* ip_str, const char* forum_name, const char* client_key)
{
	in_addr addr;
	addr.s_addr = ip_address;
	std::string ip_str_log = inet_ntoa(addr);
	
	// Calculate position if not provided (0 = auto-calculate)
	if (position == 0) {
		position = m_queued_clients.size() + 1;
	}
	
	// TODO: Proper average calcuation here
	uint32 wait_per_player = 60;
	estimated_wait = position * wait_per_player;
	
	QueuedClient new_entry(world_account_id, position, estimated_wait, ip_address, 
								 ls_account_id, from_id, 
								 ip_str ? ip_str : ip_str_log, 
								 forum_name ? forum_name : "",
								 client_key ? client_key : "");
	
	m_queued_clients.push_back(new_entry);
	
	SaveQueueDBEntry(world_account_id, position, estimated_wait, ip_address);
	LogQueueAction("ADD_TO_QUEUE", world_account_id, 
		fmt::format("pos={} wait={}s ip={} ls_id={} (memory + DB)", position, estimated_wait, ip_str_log, ls_account_id));
	
	 SendQueuedClientsUpdate();
	
}

void QueueManager::RemoveFromQueue(const std::vector<uint32>& account_ids){RemoveFromQueue(account_ids, false); /* Call with skip_database = false (normal behavior) */}

void QueueManager::RemoveFromQueue(const std::vector<uint32>& account_ids, bool skip_database)
{
	if (account_ids.empty()) {
		return;
	}
	
	// Create lookup set for O(1) account ID checking - maintains queue order
	std::unordered_set<uint32> accounts_to_remove(account_ids.begin(), account_ids.end());
	
	std::vector<uint32> removed_accounts;
	uint32 notifications_sent = 0;
	
	// Single pass through queue in order, using erase-remove idiom
	for (auto it = m_queued_clients.begin(); it != m_queued_clients.end(); ) {
		bool should_remove = false;
		
		// O(1) lookup instead of O(n) search
		if (accounts_to_remove.count(it->w_accountid)) {
			should_remove = true;
		} else {
			// Check LS account mapping fallback
			uint32 world_account_id = GetWorldAccountFromLS(it->w_accountid);
			if (world_account_id != it->w_accountid && accounts_to_remove.count(world_account_id)) {
				should_remove = true;
			}
		}
		
		if (should_remove) {
			in_addr addr;
			addr.s_addr = it->ip_address;
			std::string ip_str = inet_ntoa(addr);
			
			uint32 account_id_to_remove = it->w_accountid;
			uint32 ls_account_id_to_notify = it->ls_account_id;
			
			// Send immediate notification to removed player for responsive UI
			if (loginserver && loginserver->Connected() && ls_account_id_to_notify > 0) {
				SendQueueRemoval(ls_account_id_to_notify);
				notifications_sent++;
			}
			
			// Remove from database immediately (unless skipping database operations)
			if (!skip_database) {
				RemoveQueueDBEntry(account_id_to_remove);
			}
			
			removed_accounts.push_back(account_id_to_remove);
			
			LogQueueAction("REMOVE", account_id_to_remove, 
				fmt::format("IP: {} (efficient removal)", ip_str));
			
			it = m_queued_clients.erase(it); // erase returns next iterator
		} else {
			++it; // Keep this player, advance iterator
		}
	}
	
	if (removed_accounts.empty()) {
		LogDebug("RemoveFromQueue: No valid accounts found to remove from queue");
		return;
	}
	
	if (skip_database) {
		QueueDebugLog(2, "RemoveFromQueue: Skipped database deletion for [{}] accounts (sync from database)", removed_accounts.size());
	}
	
	if (notifications_sent > 0) {
		QueueDebugLog(1, "RemoveFromQueue: Sent [{}] queue removal notifications to disconnected players", notifications_sent);
	}
	
	if (!m_queued_clients.empty()) {
		 SendQueuedClientsUpdate(); // Sends updated queue positions to each player
		QueueDebugLog(1, "RemoveFromQueue: Removed [{}] players from queue, sent position updates to [{}] remaining clients", 
			removed_accounts.size(), m_queued_clients.size());
	} else {
		QueueDebugLog(1, "RemoveFromQueue: Removed [{}] players from queue - queue is now empty", removed_accounts.size());
	}
}

void QueueManager::SyncQueueFromDatabase()
{
	// Load current database queue entries
	std::vector<std::tuple<uint32, uint32, uint32, uint32>> db_queue_entries;
	if (!LoadQueueEntries(db_queue_entries)) {
		QueueDebugLog(2, "SyncQueueFromDatabase: Failed to load queue entries from database");
		return;
	}
	
	// Create a set of account IDs that exist in database for fast lookup
	std::set<uint32> db_account_ids;
	for (const auto& entry : db_queue_entries) {
		uint32 account_id = std::get<0>(entry);
		db_account_ids.insert(account_id);
	}
	
	// Find accounts that are in memory but not in database (removed externally)
	std::vector<uint32> accounts_to_remove;
	for (const auto& qclient : m_queued_clients) {
		if (db_account_ids.find(qclient.w_accountid) == db_account_ids.end()) {
			// Account is in memory but not in database - was removed externally
			accounts_to_remove.push_back(qclient.w_accountid);
		}
	}
	
	if (!accounts_to_remove.empty()) {
		QueueDebugLog(1, "SyncQueueFromDatabase: Found [{}] accounts removed externally from database", accounts_to_remove.size());
		
		// Remove from memory only (skip database since they're already gone)
		RemoveFromQueue(accounts_to_remove, true); // skip_database = true
		
		QueueDebugLog(1, "SyncQueueFromDatabase: Synced memory queue with database - removed [{}] externally deleted accounts", 
			accounts_to_remove.size());
	} else {
		QueueDebugLog(2, "SyncQueueFromDatabase: Memory queue is in sync with database");
	}
}

void QueueManager::UpdateQueuePositions()
{
	if (m_queued_clients.empty()) {
		return;
	}
	
	// Don't update queue positions if server is down/locked
	if (m_queue_paused) {
		QueueDebugLog(2, "Queue updates paused due to server status - [{}] players remain queued", m_queued_clients.size());
		return;
	}
	
	// Check if queue is manually frozen via rule
	if (RuleB(Quarm, FreezeQueue)) {
		QueueDebugLog(2, "Queue updates frozen by rule - [{}] players remain queued with frozen positions", m_queued_clients.size());
		return;
	}
	
	// Get server capacity for capacity decisions
	uint32 max_capacity = RuleI(Quarm, PlayerPopulationCap);
	uint32 current_population = EffectivePopulation();
	
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
	
	RemoveFromQueue(accounts_to_remove);
	
	if (auto_connects_initiated > 0) {
		QueueDebugLog(1, "Auto-connected [{}] players from queue to grace whitelist - [{}] players remain queued", 
			auto_connects_initiated, m_queued_clients.size());
		
		SendQueuedClientsUpdate();
	}
}

bool QueueManager::EvaluateConnectionRequest(const ConnectionRequest& request, uint32 max_capacity,
                                            UsertoWorldResponse* response, Client* client)
{
	QueueDecisionOutcome decision = QueueDecisionOutcome::QueuePlayer;
	
	// 1. Auto-connects always bypass (shouldn't get -6, but just in case)
	if (request.is_auto_connect) {
		QueueDebugLog(1, "QueueManager - AUTO_CONNECT: Account [{}] bypassing queue evaluation", 
			request.account_id);
		decision = QueueDecisionOutcome::AutoConnect;
	}
	// 2. Check if player is already queued (queue toggle behavior)
	else if (IsAccountQueued(request.account_id)) {
		uint32 queue_position = GetQueuePosition(request.account_id);
		QueueDebugLog(1, "QueueManager - QUEUE_TOGGLE: Account [{}] already queued at position [{}] - toggling off", 
		request.account_id, queue_position);
		decision = QueueDecisionOutcome::QueueToggle;
	}
	// 3. Check GM bypass rules - this is where we override world server's decision
	else if (RuleB(Quarm, QueueBypassGMLevel) && (request.status >= 80)) {
		QueueDebugLog(1, "QueueManager - GM_BYPASS: Account [{}] (status: {}) overriding world server capacity decision", 
			request.account_id, request.status);
		decision = QueueDecisionOutcome::GMBypass;
	}
	// 4. Check grace period whitelist (disconnected players still within grace period)
	else if (m_account_rez_mgr.IsAccountInGraceWhitelist(request.account_id)) {
		QueueDebugLog(1, "QueueManager - GRACE_BYPASS: Account [{}] in grace period whitelist - overriding capacity check", 
			request.account_id);
			
		decision = QueueDecisionOutcome::GraceBypass;
	}
	// 5. Default case - no bypass conditions met, queue the player
	else {
		QueueDebugLog(1, "QueueManager - QUEUE_PLAYER: Account [{}] at capacity with no bypass conditions - adding to queue", 
			request.account_id);
		
		decision = QueueDecisionOutcome::QueuePlayer;
	}
	switch (decision) {
		case QueueDecisionOutcome::AutoConnect:
		case QueueDecisionOutcome::GMBypass:
			return true;
			
		case QueueDecisionOutcome::GraceBypass:
			// Extend grace period for players who already have reservations but are looping char select/server select
			m_account_rez_mgr.IncreaseGraceDuration(request.account_id, 30);
			return true;
			
		case QueueDecisionOutcome::QueueToggle:
			RemoveFromQueue(request.account_id);
			QueueDebugLog(1, "QUEUE TOGGLE: Player clicked PLAY while queued - removed account [{}] from server", request.account_id);
			if (response) {
				response->response = -7; // Queue toggle response code for login server
			}
			return false;
			
		case QueueDecisionOutcome::QueuePlayer:
			// Add to queue for this server
			{
				// Use the client key from the login server request (passed via forum_name field)
				std::string client_key = request.client_key ? request.client_key : "";
				
				AddToQueue(
					request.world_account_id,        // world_account_id (primary key)
					0,                               // position (0 = auto-calculate)
					0,                               // estimated_wait (auto-calculated)
					request.ip_address,              // ip_address
					request.ls_account_id,          // ls_account_id
					response ? response->FromID : 0, // from_id
					request.ip_str,                  // ip_str
					request.forum_name,              // forum_name
					client_key.c_str()               // authorized_client_key (use forum_name as client_key)
				);
				
				uint32 queue_position = m_queued_clients.size(); // Position just added
				uint32 estimated_wait = queue_position * 60;    // TODO: Calcualte avg wait
				
				QueueDebugLog(1, "Added account [{}] to queue at position [{}] with estimated wait [{}] seconds (client key: {})", 
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
bool QueueManager::CheckForExternalChanges() // Handles test offset changes and queue refresh flags
{
	bool changes_detected = false;
	
	// Check for test offset changes
	if (CheckTestOffsetChange()) {
		changes_detected = true;
	}
	
	// Check for queue refresh flag
	if (CheckQueueRefreshFlag()) {
		changes_detected = true;
	}
	
	// Check for move player flag  
	if (CheckMovePlayerFlag()) {
		changes_detected = true;
	}
	
	return changes_detected;
}

// Helper function implementations for CheckForExternalChanges

bool QueueManager::CheckTestOffsetChange()
{
	static bool first_run = true;
	static const std::string test_offset_query = "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset' LIMIT 1";
	uint32 current_test_offset = QuerySingleUint32(test_offset_query, 0);
	
	QueueDebugLog(1, "CheckTestOffsetChange: Current test_offset: {}, cached_test_offset: {}", current_test_offset, m_cached_test_offset);
	
	if (first_run || m_cached_test_offset != current_test_offset) {
		if (!first_run) {
			QueueDebugLog(2, "Test offset changed from [{}] to [{}] - pushing server list updates to all login clients", 
				m_cached_test_offset, current_test_offset);
		}
		
		m_cached_test_offset = current_test_offset;
		
		if (!first_run) {
			// Get current effective population using the standard method
			uint32 effective_population = EffectivePopulation();
			
			// Send server list update to all connected login server clients
			if (loginserver && loginserver->Connected()) {
				QueueDebugLog(1, "Login server is connected - sending ServerOP_WorldListUpdate packet");
				
				SendWorldListUpdate(effective_population);
				QueueDebugLog(2, "Sent ServerOP_WorldListUpdate to login server for test offset change with population: {}", effective_population);
			} else {
				QueueDebugLog(1, "Login server NOT connected - cannot send update packet");
			}
			first_run = false;
			return true; // Change detected
		}
		
		first_run = false;
	}
	
	return false; // No change
}

bool QueueManager::CheckQueueRefreshFlag()
{
	static const std::string refresh_queue_query = 
		"SELECT value FROM tblloginserversettings WHERE type = 'RefreshQueue' ORDER BY value DESC LIMIT 1";
	static const std::string reset_queue_flag_query = 
		"UPDATE tblloginserversettings SET value = '0' WHERE type = 'RefreshQueue'";
		
	auto results = database.QueryDatabase(refresh_queue_query);
	if (results.Success() && results.RowCount() > 0) {
		auto row = results.begin();
		std::string flag_value = row[0] ? row[0] : "0";
		
		if (flag_value != "0") {
			bool should_refresh = (flag_value == "1" || flag_value == "true");
			
			if (should_refresh) {
				QueueDebugLog(1, "Queue refresh flag detected - refreshing queue from database");
				SyncQueueFromDatabase();
				
				// Reset the flag
				auto reset_result = database.QueryDatabase(reset_queue_flag_query);
				if (reset_result.Success()) {
					QueueDebugLog(2, "RefreshQueue flag reset to 0");
				} else {
					LogError("Failed to reset RefreshQueue flag: {}", reset_result.ErrorMessage());
				}
				
				return true; // Change detected and processed
			} else {
				QueueDebugLog(2, "RefreshQueue flag value = '{}' - resetting to 0", flag_value);
				// Reset the flag even if we don't process it
				database.QueryDatabase(reset_queue_flag_query);
			}
		}
	} else {
		if (!results.Success()) {
			LogError("CheckQueueRefreshFlag: Query failed - {}", results.ErrorMessage());
		} else {
			QueueDebugLog(2, "CheckQueueRefreshFlag: No RefreshQueue flag found in database");
		}
	}
	
	return false; // No change
}

bool QueueManager::CheckMovePlayerFlag()
{
	static const std::string move_player_query = 
		"SELECT value FROM tblloginserversettings WHERE type = 'MovePlayerInQueue' ORDER BY value DESC LIMIT 1";
	static const std::string reset_move_flag_query = 
		"UPDATE tblloginserversettings SET value = '0' WHERE type = 'MovePlayerInQueue'";
		
	auto results = database.QueryDatabase(move_player_query);
	if (results.Success() && results.RowCount() > 0) {
		auto row = results.begin();
		std::string flag_value = row[0] ? row[0] : "0";
		
		if (flag_value != "0" && flag_value.find(',') != std::string::npos) {
			// Parse the flag value: "lsid,new_position"
			size_t comma_pos = flag_value.find(',');
			std::string lsid_str = flag_value.substr(0, comma_pos);
			std::string position_str = flag_value.substr(comma_pos + 1);
			
			try {
				uint32 ls_account_id = static_cast<uint32>(std::stoul(lsid_str));
				uint32 new_position = static_cast<uint32>(std::stoul(position_str));
				
				QueueDebugLog(1, "Move player flag detected: LSID [{}] to position [{}]", ls_account_id, new_position);
				
				// Convert LS account ID to world account ID
				uint32 world_account_id = GetWorldAccountFromLS(ls_account_id);
				if (world_account_id == 0) {
					LogError("Failed to find world account for LS account [{}] in move player request", ls_account_id);
				} else {
					// Find the player in the queue
					auto it = std::find_if(m_queued_clients.begin(), m_queued_clients.end(),
						[world_account_id](const QueuedClient& qclient) {
							return qclient.w_accountid == world_account_id;
						});
					
					if (it != m_queued_clients.end()) {
						uint32 old_position = std::distance(m_queued_clients.begin(), it) + 1;
						
						if (new_position >= 1 && new_position <= m_queued_clients.size()) {
							// Move the player to the new position
							QueuedClient player_to_move = *it;
							m_queued_clients.erase(it);
							
							// Insert at new position (convert 1-based to 0-based index)
							uint32 insert_index = (new_position > m_queued_clients.size()) ? m_queued_clients.size() : (new_position - 1);
							m_queued_clients.insert(m_queued_clients.begin() + insert_index, player_to_move);
							
							// Update database positions for all affected players
							for (size_t i = 0; i < m_queued_clients.size(); ++i) {
								uint32 db_position = i + 1;
								uint32 wait_time = db_position * 60; // 60 seconds per position
								SaveQueueDBEntry(m_queued_clients[i].w_accountid, db_position, wait_time, m_queued_clients[i].ip_address);
							}
							
							LogQueueAction("MOVE_PLAYER", world_account_id, 
								fmt::format("LS ID [{}] moved from position [{}] to [{}])", ls_account_id, old_position, new_position));
							
							// Send updates to all queued clients
							SendQueuedClientsUpdate();
						} else {
							LogError("Invalid new position [{}] for move player request (queue size: {})", new_position, m_queued_clients.size());
						}
					} else {
						LogError("Player with world account [{}] (LS account [{}]) not found in queue", world_account_id, ls_account_id);
					}
				}
				
				// Reset the flag
				auto reset_result = database.QueryDatabase(reset_move_flag_query);
				if (reset_result.Success()) {
					QueueDebugLog(2, "MovePlayerInQueue flag reset to 0");
				} else {
					LogError("Failed to reset MovePlayerInQueue flag: {}", reset_result.ErrorMessage());
				}
				
				return true; // Change detected and processed
				
			} catch (const std::exception& e) {
				LogError("Failed to parse move player flag value '{}': {}", flag_value, e.what());
				// Reset the flag even if parsing failed
				database.QueryDatabase(reset_move_flag_query);
			}
		} else if (flag_value != "0") {
			QueueDebugLog(2, "MovePlayerInQueue flag value = '{}' - invalid format, resetting to 0", flag_value);
			// Reset invalid flag
			database.QueryDatabase(reset_move_flag_query);
		}
	} else {
		if (!results.Success()) {
			LogError("CheckMovePlayerFlag: Query failed - {}", results.ErrorMessage());
		} else {
			QueueDebugLog(2, "CheckMovePlayerFlag: No MovePlayerInQueue flag found in database");
		}
	}
	
	return false; // No change
}

void QueueManager::ClearAllQueues() // Unused for now
{
	if (!m_queued_clients.empty()) {
		uint32 count = GetTotalQueueSize();
		QueueDebugLog(1, "Clearing all queue entries for world server - removing [{}] players", count);
		
		// 1. Clear memory
		m_queued_clients.clear();
		
		// 2. Clear database immediately (event-driven)
		auto clear_query = fmt::format("DELETE FROM tblLoginQueue WHERE world_server_id = {}", m_world_server_id);
		auto result = database.QueryDatabase(clear_query);
		
		if (result.Success()) {
			QueueDebugLog(1, "Queue cleared - [{}] players removed (memory + DB)", count);
		} else {
			LogError("Queue cleared from memory but failed to clear database: {}", result.ErrorMessage());
		}
	}
}

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

bool QueueManager::QueryDB(const std::string& query, const std::string& operation_desc) const
{
	// Safety check: ensure database is ready before accessing it
	if (!database_ready) {
		QueueDebugLog(2, "Database not ready - skipping operation: {}", operation_desc);
		return false;
	}
	
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		LogError("Failed to {}: {}", operation_desc, results.ErrorMessage());
		return false;
	}
	return true;
}

uint32 QueueManager::QuerySingleUint32(const std::string& query, uint32 default_value) const
{
	if (!ValidateDatabaseReady()) {
		return default_value;
	}
	
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
	if (details.empty()) {
		QueueDebugLog(1, "QueueManager - {}: Account [{}]", action, account_id);
	} else {
		QueueDebugLog(1, "QueueManager - {}: Account [{}] - {}", action, account_id, details);
	}
}

void QueueManager::SendQueuedClientsUpdate() const {
	QueueDebugLog(1, "SendQueuedClientsUpdate called - checking queue state...");
	
	if (m_queued_clients.empty()) {
		QueueDebugLog(1, "No queued players to update");
		return;
	}
	
	if (!loginserver || !loginserver->Connected()) {
		LogError("Login server not available - cannot send queue updates");
		return;
	}
	
	QueueDebugLog(1, "Found [{}] queued players, login server available - sending batch update", m_queued_clients.size());
	
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
		
		QueueDebugLog(1, "Added to batch: account [{}] (LS: {}) position [{}] wait [{}]s", 
			qclient.w_accountid, qclient.ls_account_id, dynamic_position, update.estimated_wait);
	}
	
	if (valid_updates == 0) {
		QueueDebugLog(1, "No valid queue updates to send");
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
	
	QueueDebugLog(1, "Created batch packet - opcode: 0x{:X}, size: {}, update count: {}", 
		ServerOP_QueueBatchUpdate, packet_size, valid_updates);
	
	// Send single batch packet to login server
	loginserver->SendPacket(batch_pack);
	delete batch_pack;
	
	QueueDebugLog(1, "Sent batch queue update with [{}] player updates to login server (skipped: {})", 
		valid_updates, skipped_invalid);
	
	QueueDebugLog(1, "SendQueuedClientsUpdate completed - batch sent: {}, skipped: {}, total queued: {}", 
		valid_updates, skipped_invalid, m_queued_clients.size());
}

void QueueManager::AutoConnectQueuedPlayer(const QueuedClient& qclient)
{
	if (!loginserver || !loginserver->Connected()) {
		LogError("Cannot auto-connect queued player - login server not available or not connected");
		return;
	}
	
	QueueDebugLog(1, "AUTO-CONNECT: Sending auto-connect trigger for LS account [{}]", qclient.ls_account_id);
	
	// Add to grace whitelist before sending auto-connect trigger - direct object access
	QueueDebugLog(1, "AUTO-CONNECT: Added account [{}] to grace whitelist for population cap bypass", qclient.w_accountid);
	m_account_rez_mgr.AddRez(qclient.w_accountid, qclient.ip_address, 45);
	
	// Send auto-connect packet to login server
	SendQueueAutoConnect(qclient);
	QueueDebugLog(1, "AUTO-CONNECT: Sent ServerOP_QueueAutoConnect to loginserver for account [{}]", qclient.ls_account_id);
}

void QueueManager::SendQueueAutoConnect(const QueuedClient& qclient)
{
	if (!ValidateLoginServerConnection(ServerOP_QueueAutoConnect)) {
		return;
	}
	
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
	
	loginserver->SendPacket(autoconnect_pack);
	delete autoconnect_pack;
	
	QueueDebugLog(2, "Sent ServerOP_QueueAutoConnect for LS account {} with client key", qclient.ls_account_id);
}

// DATABASE QUEUE OPERATIONS 
bool QueueManager::ValidateDatabaseReady() const
{
	if (!database_ready) {
		QueueDebugLog(1, "Database not ready - skipping operation");
		return false;
	}
	return true;
}
void QueueManager::SaveQueueDBEntry(uint32 account_id, uint32 queue_position, uint32 estimated_wait, uint32 ip_address)
{
	if (!ValidateDatabaseReady()) {
		return;
	}
	
	auto query = fmt::format(
		"REPLACE INTO tblLoginQueue (account_id, world_server_id, queue_position, estimated_wait, ip_address) "
		"VALUES ({}, {}, {}, {}, {})",
		account_id, m_world_server_id, queue_position, estimated_wait, ip_address
	);

	QueryDB(query, fmt::format("save queue entry for account {} on world server {}", account_id, m_world_server_id));
}

void QueueManager::RemoveQueueDBEntry(uint32 account_id)
{
	if (!ValidateDatabaseReady()) {
		return;
	}
	
	auto query = fmt::format(
		"DELETE FROM tblLoginQueue WHERE account_id = {} AND world_server_id = {}",
		account_id, m_world_server_id
	);

	QueryDB(query, fmt::format("remove queue entry for account {} on world server {}", account_id, m_world_server_id));
}

bool QueueManager::LoadQueueEntries(std::vector<std::tuple<uint32, uint32, uint32, uint32>>& queue_entries)
{
	auto query = fmt::format(
		"SELECT account_id, queue_position, estimated_wait, ip_address "
		"FROM tblLoginQueue WHERE world_server_id = {} "
		"ORDER BY queue_position ASC",
		m_world_server_id
	);

	auto results = database.QueryDatabase(query);  // Use world server global database
	if (!results.Success()) {
		LogError("Failed to load queue entries for world server {}", m_world_server_id);
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

	QueueDebugLog(2, "Loaded {} queue entries for world server {}", queue_entries.size(), m_world_server_id);
	return true;
}

void QueueManager::ProcessAdvancementTimer()
{
	uint32 queue_size = GetTotalQueueSize();
	LogDebug("Queue Status: {} players currently in queue", queue_size);

	// Update server_population table for monitoring/debugging 
	uint32 effective_population = EffectivePopulation();
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
	// 
	if (loginserver && loginserver->Connected()) {
		static uint32 last_sent_population = UINT32_MAX; // Track last sent value
		
		if (effective_population != last_sent_population) {
			SendWorldListUpdate(effective_population);
			LogDebug("Sent real-time ServerOP_WorldListUpdate with population: {} (changed from {})", 
				effective_population, last_sent_population);
			
			last_sent_population = effective_population; // Update tracking variable
		}
	}
	// Auto connect Q player #1. Qplayers are shown their # in the queue 
	UpdateQueuePositions();
	// Check for stale connections
	m_account_rez_mgr.PeriodicMaintenance();
	// Sync queue with database
	if (CheckForExternalChanges()) {
		SendWorldListUpdate(EffectivePopulation());
	}
}

void QueueManager::RestoreQueueFromDatabase()
{
	// Check if queue persistence is enabled
	if (!RuleB(Quarm, EnableQueuePersistence)) {
		QueueDebugLog(2, "Queue persistence disabled - clearing old queue entries for world server [{}]", m_world_server_id);
		auto clear_query = fmt::format("DELETE FROM tblLoginQueue WHERE world_server_id = {}", m_world_server_id);
		database.QueryDatabase(clear_query);  // Use global database
		return;
	}
	
	QueueDebugLog(1, "Restoring queue from database for world server [{}]", m_world_server_id);
	
	// Load queue entries from database
	std::vector<std::tuple<uint32, uint32, uint32, uint32>> queue_entries;
	if (!LoadQueueEntries(queue_entries)) {
		LogError("Failed to load queue entries from database");
		return;
	}
	
	// Restore queue entries to memory
	m_queued_clients.clear();
	uint32 restored_count = 0;
	
	for (const auto& entry_tuple : queue_entries) {
		uint32 world_account_id = std::get<0>(entry_tuple);  
		uint32 queue_position = std::get<1>(entry_tuple);
		uint32 estimated_wait = std::get<2>(entry_tuple);
		uint32 ip_address = std::get<3>(entry_tuple);
		
		// Skip entries with invalid world account IDs
		if (world_account_id == 0) {
			QueueDebugLog(2, "QueueManager - SKIP: Invalid world account ID [0] during restoration");
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
		entry.authorized_client_key = "";
		
		// Use vector push_back instead of map indexing (consistent with vector declaration)
		m_queued_clients.push_back(entry);
		restored_count++;
		
		QueueDebugLog(2, "QueueManager - RESTORE: World account [{}] at position [{}] with wait [{}] - persistent queue entry restored", 
			world_account_id, queue_position, estimated_wait);
	}
	
	if (restored_count > 0) {
		QueueDebugLog(1, "Restored [{}] persistent queue entries from database for world server [{}]", 
			restored_count, m_world_server_id);
		QueueDebugLog(2, "NOTE: Restored entries use world account IDs only - LS account mapping will be established when players reconnect");
	} else {
		QueueDebugLog(2, "No queue entries to restore for world server [{}]", m_world_server_id);
	}
}

// Connection validation helper to reduce code duplication
bool QueueManager::ValidateLoginServerConnection(uint16 opcode) const
{
	if (!loginserver || !loginserver->Connected()) {
		if (opcode != 0) {
			QueueDebugLog(2, "Cannot send opcode 0x{:X} - login server not connected", opcode);
		} else {
			QueueDebugLog(2, "Login server not connected - skipping packet send");
		}
		return false;
	}
	return true;
}

// ServerPacket helper methods to reduce code duplication
void QueueManager::SendWorldListUpdate(uint32 effective_population)
{
	if (!ValidateLoginServerConnection(ServerOP_WorldListUpdate)) {return;}
	
	auto update_pack = new ServerPacket(ServerOP_WorldListUpdate, sizeof(uint32));
	*((uint32*)update_pack->pBuffer) = effective_population;
	loginserver->SendPacket(update_pack);
	delete update_pack;
	
	QueueDebugLog(2, "Sent ServerOP_WorldListUpdate with population: {}", effective_population);
}

void QueueManager::SendQueueRemoval(uint32 ls_account_id)
{
	if (!ValidateLoginServerConnection(ServerOP_QueueDirectUpdate)) {return;}
	
	auto removal_pack = new ServerPacket(ServerOP_QueueDirectUpdate, sizeof(ServerQueueDirectUpdate_Struct));
	ServerQueueDirectUpdate_Struct* removal = (ServerQueueDirectUpdate_Struct*)removal_pack->pBuffer;
	
	removal->ls_account_id = ls_account_id;
	removal->queue_position = 0; // Position 0 = removed from queue
	removal->estimated_wait = 0;
	removal->ip_address = 0;
	
	loginserver->SendPacket(removal_pack);
	delete removal_pack;
	
	QueueDebugLog(2, "Sent queue removal for LS account {}", ls_account_id);
}

// Template method for common packet sending pattern
template<typename T>
void QueueManager::SendLoginServerPacket(uint16 opcode, const T& data)
{
	if (!ValidateLoginServerConnection(opcode)) {return;}
	
	auto packet = new ServerPacket(opcode, sizeof(T));
	*((T*)packet->pBuffer) = data;
	loginserver->SendPacket(packet);
	delete packet;
	
	QueueDebugLog(2, "Sent packet opcode 0x{:X}, size: {}", opcode, sizeof(T));
}

// Overload for single uint32 values
void QueueManager::SendLoginServerPacket(uint16 opcode, uint32 single_value)
{
	if (!ValidateLoginServerConnection(opcode)) {return;}
	
	auto packet = new ServerPacket(opcode, sizeof(uint32));
	*((uint32*)packet->pBuffer) = single_value;
	loginserver->SendPacket(packet);
	delete packet;
	
	QueueDebugLog(2, "Sent packet opcode 0x{:X} with value: {}", opcode, single_value);
}

// Overload for opcodes with no data
void QueueManager::SendLoginServerPacket(uint16 opcode)
{
	if (!ValidateLoginServerConnection(opcode)) {return;}
	
	auto packet = new ServerPacket(opcode, 0);
	loginserver->SendPacket(packet);
	delete packet;
	
	QueueDebugLog(2, "Sent packet opcode 0x{:X} (no data)", opcode);
} 