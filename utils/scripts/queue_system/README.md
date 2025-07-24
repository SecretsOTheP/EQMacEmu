# 🔄 Queue System 

## 🎯 **Overview**

The queue system provides server capacity management through a **World Server-centric architecture** where all queue decisions are made centrally by `QueueManager` on the world server. The system includes automatic queue advancement, grace period management for disconnections, and real-time population tracking.

### **🏗️ Key Architectural Components**
- **🌍 QueueManager** (`world/world_queue.cpp`) - Central decision authority for all queue operations
- **🏠 AccountRezMgr** (`world/account_reservation_manager.cpp`) - Grace period & population tracking


### **🔄 Core Flow**
1. **Client** clicks PLAY → **Login Server** receives request
2. **Login Server** queries **World Server** via `ServerOP_UsertoWorldReq`
3. **QueueManager** makes capacity decision (admit/queue/bypass)
4. **World Server** responds with decision via `ServerOP_UsertoWorldResp`
5. **Auto-Connect System** advances queue and triggers connections automatically

### **⚡ Key Features**
- **Account ID-Based** - No IP tracking, pure account reservation system
- **Self-Repairing** - Automatically recovers from crashes and validates state
- **Real-Time Updates** - Queue positions update live via batch packets
- **Grace Periods** - Recently disconnected players bypass queue temporarily
- **GM Bypass** - Configurable admin level queue exemptions

### **🔄 Backward Compatibility**
- **Unknown Opcodes Ignored** - Old servers silently drop `ServerOP_Queue*` packets
- **Standard Login Flow** - Non-queue servers process `ServerOP_UsertoWorldReq/Resp` normally  
- **No Breaking Changes** - All existing login functionality preserved
- **Graceful Degradation** - Queue-enabled login servers work with old world servers


---

## 🏗️ **System Architecture & Flow Diagrams**

### **World Server-Centric Architecture**

```
┌─────────────┐
│   CLIENTS   │
│             │
│ • PLAY btn  │
└──────┬──────┘
       │ OP_PlayEverquestRequest
       ▼
┌─────────────────────────────────────────────┐
│               LOGIN SERVER                  │
│                                             │
│ ┌─────────────────────────────────────────┐ │
│ │ Client Requests:                        │ │
│ │ • Handle_Play                           │ │
│ │ • SendUserToWorldRequest                │ │
│ │ • ProcessUsertoWorldResp                │ │
│ └─────────────────────────────────────────┘ │
│           │                                 │
│           ▼ ServerOP_UsertoWorldReq         │
└───────────┼─────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────────┐
│                WORLD SERVER                 │
│                                             │
│  ┌─────────────────┐    ┌─────────────────┐ │
│  │   QueueManager  │◄──►│ AccountRezMgr   │ │
│  │                 │    │ (Self-Repair)   │ │
│  │ • Queue Logic   │    │ • Population    │ │
│  │ • Bypass Rules  │    │ • Grace Periods │ │
│  │ • Auto-Connect  │    │ • Reservations  │ │
│  └─────────────────┘    └─────────────────┘ │
│           │                                 │
│           ▼ Decision Authority              │
│  ┌─────────────────────────────────────────┐│
│  │ EvaluateConnectionRequest:              ││
│  │ • Capacity check                        ││
│  │ • GM/Grace bypasses                     ││
│  │ • Queue addition                        ││
│  └─────────────────────────────────────────┘│
│           │                                 │
│           ▼ Response Packets                │
└───────────┼─────────────────────────────────┘
            │ • ServerOP_UsertoWorldResp
            │ • ServerOP_QueueAutoConnect  
            │ • ServerOP_QueueBatchUpdate
            │ • ServerOP_WorldListUpdate
            ▼
┌─────────────────────────────────────────────┐
│               LOGIN SERVER                  │
│                                             │
│ ┌─────────────────────────────────────────┐ │
│ │ Response Handling:                      │ │
│ │ • Success (1) → SendClientAuth          │ │
│ │ • Queue (-6) → Stay on server select    │ │
│ │ • Auto-connect → Trigger PLAY           │ │
│ │ • Batch updates → Update queue pos      │ │
│ └─────────────────────────────────────────┘ │
│           │                                 │
│           ▼ Client Updates                  │
└───────────┼─────────────────────────────────┘
            │
            ▼
┌─────────────┐
│   CLIENTS   │
│             │
│ Server List │
│ Queue Pos   │
│ Updates     │
└─────────────┘
```

### **Connection Flow Integration**

```
═══════════════════════════════════════════════════════════════
🎮 **COMPLETE LOGIN FLOW** (Client PLAY button to world entry)
═══════════════════════════════════════════════════════════════

🎮 *Client clicks PLAY on server*
↓
└─📁📨 `loginserver/client.cpp`
    └─ OP_PlayEverquestRequest received
        └─ [Client::Handle_Play]
            └─ ❔ m_client_status == cs_logged_in?
                └─ ✅ Continue
                └─ ❌ LogError + return
↓
📁📨 `loginserver/server_manager.cpp`
└─ [SendUserToWorldRequest]
    ├─ Find target world server by IP
    ├─ Create UsertoWorldRequest packet
    ├─ utwr->lsaccountid = client_account_id
    ├─ utwr->ip = client_ip
    ├─ utwr->FromID = is_auto_connect ? 1 : 0  // Manual PLAY = 0
    ├─ utwr->forum_name = client_key (for queue targeting)
    └─ Send: ServerOP_UsertoWorldReq → World server
        ↓
        📁🌍 `world/login_server.cpp`
        └─ *OnMessage(ServerOP_UsertoWorldReq)*
            └─ [ProcessUsertoWorldReq]
                ├─ 🔍 **Standard validation checks** (bans, suspensions, IP limits, etc.)
                │   └─ ❌ If failed → Various error responses (-1 to -5)
                │   └─ ✅ If passed → Continue to capacity check
                ├─ 📊 **CAPACITY CHECK:**
                │   ├─ effective_population = GetWorldPop() // queue_manager.EffectivePopulation()
                │   ├─ queue_cap = RuleI(Quarm, PlayerPopulationCap)
                │   └─ ❔ effective_population >= queue_cap?
                │       ├─ ❌ Under capacity → response = 1 ✅ (SUCCESS)
                │       └─ ✅ At capacity → **QUEUE MANAGER EVALUATION:**
                │           └─ 🎯 [queue_manager.EvaluateConnectionRequest]
                │               ├─ ❔ Auto-connect (FromID == 1)?
                │               │   └─ ✅ AutoConnect → Override capacity (return true)
                │               ├─ ❔ Already queued?
                │               │   └─ ✅ QueueToggle → Remove from queue, response = -7
                │               ├─ ❔ GM bypass (status >= 80)?
                │               │   └─ ✅ GMBypass → Override capacity (return true)
                │               ├─ ❔ Grace period whitelist?
                │               │   └─ ✅ GraceBypass → Extend grace, override capacity
                │               └─ ❌ No bypass → QueuePlayer:
                │                   ├─ [AddToQueue] → Add to queue
                │                   ├─ response = -6 (QUEUE)
                │                   └─ return false (respect capacity)
                └─ 📡 Send: ServerOP_UsertoWorldResp → Login server
                    ↓
                    📁📨 `loginserver/world_server.cpp`
                    └─ *OnMessage(ServerOP_UsertoWorldResp)*
                        └─ [ProcessUsertoWorldResp]
                            ├─ Find client by lsaccountid
                            └─ ❔ response code?
                                ├─ ✅ **1 (SUCCESS):**
                                │   ├─ [SendClientAuth] → ServerOP_LSClientAuth → World server
                                │   │   ↓
                                │   │   📁🌍 `world/login_server.cpp`
                                │   │   └─ *OnMessage(ServerOP_LSClientAuth)*
                                │   │       └─ [ProcessLSClientAuth]
                                │   │           ├─ [client_list.CLEAdd] → Player enters world
                                │   │           └─ [m_account_rez_mgr.AddReservation] → Track connection
                                │   └─ 🎮 **CLIENT ENTERS WORLD**
                                ├─ ❌ **-6 (QUEUE):**
                                │   ├─ LogInfo("Player should be queued")
                                │   ├─ return early (no play response)
                                │   └─ 🎮 **CLIENT STAYS ON SERVER SELECT**
                                │       └─ Sees queue position via server list updates
                                ├─ ❌ **-7 (QUEUE TOGGLE):**
                                │   ├─ LogInfo("Player removed from queue")
                                │   ├─ return early (no play response)
                                │   └─ 🎮 **CLIENT STAYS ON SERVER SELECT**
                                └─ ❌ **Other errors:**
                                    ├─ Standard validation failures (-1 to -5)
                                    └─ 🎮 **CLIENT GETS ERROR MESSAGE**
```

### **QueueAdvancementTimer (3000ms) - Main Queue Processing**

```
═══════════════════════════════════════════════════════════════
🔄 **QUEUE ADVANCEMENT FLOW** (Every 3 seconds in world server)
═══════════════════════════════════════════════════════════════

📁🌍 `world/main.cpp`
├─ ⏰ Timer QueueManagerTimer(3000)  // 3 second interval
├─ QueueManagerTimer.Start() in main()
└─ ⏰ [QueueManagerTimer.Check] (3s interval)
    └─ 🎯 [queue_manager.ProcessAdvancementTimer]
        ├─ 📊 Update server_population table (effective_population)
        ├─ 📡 SendWorldListUpdate() → Login server (if population changed)
        │   └─ Updates g_server_populations[server_id] cache
        │   └─ Pushes to ALL connected login clients immediately
        ├─ 🎯 [UpdateQueuePositions] → MAIN QUEUE LOGIC
        │   ├─ 🧹 Check m_queue_paused and RuleB(Quarm, FreezeQueue)
        │   ├─ 📊 current_population = EffectivePopulation()
        │   ├─ 🎯 available_slots = max_capacity - current_population
        │   ├─ 🔍 Queued player at position 1:
        │   │   └─ ❔ available_slots > 0?
        │   │       └─ ✅ [AutoConnectQueuedPlayer]
        │   │           ├─ 🏠 [m_account_rez_mgr.AddRez] → Create reservation FIRST
        │   │           │   └─ 30-second grace period for capacity bypass
        │   │           └─ 📡 [SendQueueAutoConnect] → ServerOP_QueueAutoConnect → Login server
        │   │               ↓
        │   │               📁📨 `loginserver/world_server.cpp`
        │   │               └─ [ProcessQueueAutoConnect]
        │   │                   └─ Contains: ls_account_id, ip, client_key
        │   │       └─ 🗑️ Remove from queue after auto-connect
        │   └─ 📢 [SendQueuedClientsUpdate] → ServerOP_QueueBatchUpdate → Login server
        │       ├─ Recalculates positions: position = index + 1
        │       ├─ Recalculates wait times: wait = position * 60
        │       └─ Single packet with all queue updates
        │           ↓
        │           📁📨 `loginserver/world_server.cpp`
        │           └─ *OnMessage(ServerOP_QueueBatchUpdate)*
        │               └─ [ProcessQueueBatchUpdate]
        │                   ├─ Parse batch header + update array  
        │                   ├─ For each update in batch:
        │                   │   ├─ 🔍 Find client by ls_account_id
        │                   │   ├─ Update client queue position
        │                   │   └─ 📱 [target_client->SendServerListPacket]
        │                   │       └─ 🔄 Rebuilds server list with NEW position
        │                   │       └─ 📤 **🎯 USER SEES UPDATE INSTANTLY**
        │                   └─ Log: "Processed [X] successful, [Y] failed"
        ├─ 🧹 [m_account_rez_mgr.PeriodicMaintenance]
        │   ├─ Scan all account reservations for expiry
        │   ├─ Remove expired grace periods (30s default)
        │   ├─ Sync to database every 5 minutes
        │   └─ 🔧 **SELF-REPAIR:** Auto-correct inconsistent states
        └─ 🔄 [CheckForExternalChanges]
            ├─ Check for RuleI(Quarm, TestPopulationOffset) changes
            ├─ Check RefreshQueue flag in tblloginserversettings
            ├─ If test offset changed → SendWorldListUpdate()
            └─ If RefreshQueue=1 → RestoreQueueFromDatabase()
```
### **Real-time Population Synchronization**
```
═══════════════════════════════════════════════════════════════
📊 **POPULATION TRACKING & SYNC** (Every 3 seconds + event-driven)
═══════════════════════════════════════════════════════════════

📁🌍 world_queue.cpp [QueueManager::EffectivePopulation]
├─ 🏠 Account Reservations: m_account_rez_mgr.Total()
│   ├─ 👥 Active connections (in-world players)
│   └─ ⏰ Grace period connections (recently disconnected, 30s)
├─ 🧪 Test Offset: m_cached_test_offset
│   ├─ 🔬 RuleI(Quarm, TestPopulationOffset) for simulation
│   └─ 📊 Cached value updated via CheckForExternalChanges()
└─ 📊 effective_population = reservations + test_offset

📁🌍 ProcessAdvancementTimer [Every 3 seconds]
├─ 💾 DB Update: server_population table
│   └─ INSERT/UPDATE effective_population, last_updated = NOW()
├─ 📡 Real-time Login Server Sync:
│   ├─ ❔ effective_population != last_sent_population?
│   │   └─ ✅ [SendWorldListUpdate] → ServerOP_WorldListUpdate → Login server
│   │           ↓
│   │           📁📨 `loginserver/world_server.cpp`
│   │           └─ [ProcessWorldListUpdate]
│   │               ├─ 🎯 g_server_populations[server_id] = new_population
│   │               └─ 📡 server.client_manager->UpdateServerList()
│   │                   └─ Pushes to ALL connected login clients
│   │                   └─ Users see updated server population instantly
│   └─ 📊 last_sent_population = effective_population (cache)
└─ 🔄 Backup: active_ip_connections table (5min snapshots)
    └─ 💾 AccountRezMgr crash recovery data
```




### **AccountRezMgr Grace Period Management (Self-Repairing)**
```
═══════════════════════════════════════════════════════════════
🔧 **ACCOUNT RESERVATION SELF-REPAIR** (Every 3 seconds via PeriodicMaintenance)
═══════════════════════════════════════════════════════════════

📁🌍 account_reservation_manager.cpp
└─ 🎯 [m_account_rez_mgr.PeriodicMaintenance] (called by ProcessAdvancementTimer)
    ├─ 🔍 Scan all account reservations in m_reservations map
    │   └─ For each reservation:
    │       ├─ ❔ Connection still active?
    │       │   └─ ✅ Refresh last_seen timestamp → Keep alive
    │       ├─ ❔ Grace period (30s default) expired?
    │       │   └─ 🗑️ Remove reservation → Update population count
    │       ├─ ❔ Raid detected (extended protection)?
    │       │   └─ ⏰ Extend grace period (longer timeout)
    │       └─ 🔧 **SELF-REPAIR CHECKS:**
    │           ├─ Validate reservation timestamps
    │           ├─ Check for orphaned reservations
    │           ├─ Correct inconsistent IP mappings
    │           └─ Verify account ID validity
    ├─ 💾 Database Sync (every 5 minutes):
    │   ├─ Batch write to active_ip_connections table
    │   ├─ Remove expired entries from database
    │   └─ 🔧 Cross-validate memory vs database state
    ├─ 📊 Population Impact:
    │   ├─ Immediate EffectivePopulation() updates
    │   ├─ Real-time capacity calculations
    │   └─ Auto-trigger SendWorldListUpdate() if changed
```

## 💾 **Event-Driven Database Mirroring**

```
🔄 **On Queue Add:**
└─ 📝 Memory: m_queued_players[account_id] = entry
└─ 💾 DB: Update server_population SET effective_population = X (immediate)
└─ 📋 Log: "ADD_TO_QUEUE pos=1 wait=60s account=[12345] (memory + DB)"

🔄 **On Queue Remove:**
└─ 🗑️ Memory: m_queued_players.erase(account_id)
└─ 💾 DB: Update server_population SET effective_population = X (immediate)  
└─ 📋 Log: "REMOVE_FROM_QUEUE account=[12345] (memory + DB)"

🔄 **On Auto-Connect Success:**
└─ ⏳ Memory: Move to m_pending_connections
└─ 💾 DB: Update server_population (pending now counts as population)
└─ 📡 ServerOP_QueueAutoConnect → Login server
└─ 📋 Log: "AUTO_CONNECT account=[12345] client_key=[abc123]"

⚡ **Result:** Database always synchronized, zero queue data loss! 🎯
```

## 📋 **Implementation Details & Code Examples**


### **Startup Lifecycle & Global Object Safety**
```cpp
// world/main.cpp
#include "world_queue.h"

// world/world_queue.h  
extern QueueManager queue_manager;
class QueueManager {
    mutable AccountRezMgr m_account_rez_mgr;
    // Global object - always valid, no null checks needed
};

// world/main.cpp - Timer declaration
Timer QueueManagerTimer(3000);

// world/world_queue.cpp - Direct usage anywhere
queue_manager.ProcessAdvancementTimer();  // Always safe

// ✅ Global Safety Guarantees:
// - extern declaration makes queue_manager available everywhere
// - Global object lifetime = entire program execution  
// - No initialization dependencies or startup order issues
// - Mutable AccountRezMgr handles internal state management
```
## 🔧 **Core Components**
### **class QueueManager** (`world/world_queue.cpp/.h`)
**Primary queue coordinator - World server queue management and population control**
```cpp
class QueueManager {
public:
    // Primary queue operations
    void AddToQueue(uint32 ls_account_id, const std::string& account_name, 
                   uint32 server_id, const std::string& client_key);
    void RemoveFromQueue(uint32 ls_account_id, uint32 server_id = 0);
    uint32 GetQueuePosition(uint32 ls_account_id, uint32 server_id) const;
    
    // Population decision making
    uint32 GetEffectivePopulation();
    uint32 GetWorldPop() const;
    bool ShouldQueuePlayer() const;
    
    // Queue advancement & maintenance
    void UpdateQueuePositions();
    void PeriodicMaintenance();
    void CheckForExternalChanges();
    
private:
    std::queue<QueueEntry> m_queue;
    std::map<uint32, uint32> m_pending_connections;  // account_id -> timestamp
    Timer m_advancement_timer;
    Timer m_maintenance_timer;
};
```

### **class AccountRezMgr** (`world/account_reservation_manager.cpp/.h`)
**Supporting component - Account connection tracking for QueueManager (Self-Repairing)**

```cpp
class AccountRezMgr {
public:
    // Population data for QueueManager
    uint32 Total() const;  // All reservations (active + grace period)
    uint32 ActiveCount() const;  // Active connections only
    
    // Reservation management (supports queue bypass)
    void AddReservation(uint32 account_id, uint32 ip_address);
    void RemoveReservation(uint32 account_id);
    bool HasReservation(uint32 account_id) const;
    
    // Grace period support
    void StartGracePeriod(uint32 account_id);
    void RefreshReservation(uint32 account_id);
    
    // Maintenance (Self-Repairing)
    void CleanupStaleConnections();
    void SyncAllConnectionsToDatabase();
    
private:
    std::unordered_map<uint32, AccountReservation> m_reservations;
    Timer m_cleanup_timer;
    Timer m_sync_timer;
};
```

**Supporting Role for QueueManager:**
- **Population Reporting** - Provides base player count for queue decisions
- **Reservation Tracking** - Maintains connection state that QueueManager uses
- **Queue Bypass Implementation** - Handles grace periods for recently disconnected players
- **Database Persistence** - Ensures QueueManager has reliable population data
- **Connection Lifecycle** - Manages the full player connection experience
- **Self-Repairing** - Auto-corrects inconsistent states, recovers from crashes, validates DB integrity

---

### **Data Flow Code Examples:**

**1. Receives Capacity Check Request**
```cpp
// world/login_server.cpp - ProcessUsertoWorldReq
if (effective_population >= queue_cap) {
    ConnectionRequest request = {};
    request.account_id = id;
    request.is_auto_connect = (utwr->FromID == 1);
    
    bool should_override = queue_manager.EvaluateConnectionRequest(request, queue_cap, utwrs, nullptr);
}
```

**2. Queries AccountRezMgr for Population**
```cpp
// world/world_queue.cpp - EffectivePopulation  
uint32 QueueManager::EffectivePopulation() {
    uint32 base_population = m_account_rez_mgr.Total();
    uint32 test_offset = m_cached_test_offset;
    uint32 effective_population = base_population + test_offset;
    return effective_population;
}
```

**3. Makes Queue/Admit Decision**
```cpp
// world/world_queue.cpp - EvaluateConnectionRequest
if (request.is_auto_connect) {
    decision = QueueDecisionOutcome::AutoConnect;
} else if (IsAccountQueued(request.account_id)) {
    decision = QueueDecisionOutcome::QueueToggle;
} else if (RuleB(Quarm, QueueBypassGMLevel) && request.status >= 80) {
    decision = QueueDecisionOutcome::GMBypass;
} else if (m_account_rez_mgr.IsAccountInGraceWhitelist(request.account_id)) {
    decision = QueueDecisionOutcome::GraceBypass;
} else {
    decision = QueueDecisionOutcome::QueuePlayer;
}
```

**4. Sends Response to Login Server**
```cpp
// world/world_queue.cpp - EvaluateConnectionRequest  
switch (decision) {
    case QueueDecisionOutcome::AutoConnect:
    case QueueDecisionOutcome::GMBypass:
        return true; // Override capacity - allow connection
        
    case QueueDecisionOutcome::GraceBypass:
        m_account_rez_mgr.IncreaseGraceDuration(request.account_id, 30);
        return true; // Allow connection with extended grace
        
    case QueueDecisionOutcome::QueuePlayer:
        response->response = -6; // Queue response
        return false;
        
    case QueueDecisionOutcome::QueueToggle:
        response->response = -7; // Toggle response  
        return false;
}
```

**5. Processes Queue Advancement via Timer**
```cpp
// world/main.cpp + world/world_queue.cpp
if (QueueAdvancementTimer.Check()) {
    queue_manager.UpdateQueuePositions();
}

void QueueManager::UpdateQueuePositions() {
    for (auto& client : m_queued_clients) {
        if (current_position == 1 && available_slots > 0) {
            // Create reservation FIRST, then auto-connect
            AutoConnectQueuedPlayer(client);
            accounts_to_remove.push_back(client.w_accountid);
        }
    }
}
```

**6. Sends Auto-Connect Requests to Login Server**
```cpp
// world/world_queue.cpp - AutoConnectQueuedPlayer + SendQueueAutoConnect
void QueueManager::AutoConnectQueuedPlayer(const QueuedClient& qclient) {
    // Add reservation FIRST - grants capacity bypass
    m_account_rez_mgr.AddRez(qclient.w_accountid, qclient.ip_address, 30);
    
    // Then send auto-connect packet to login server
    SendQueueAutoConnect(qclient);
}

void QueueManager::SendQueueAutoConnect(const QueuedClient& client) {
    auto pack = new ServerPacket(ServerOP_QueueAutoConnect, sizeof(ServerQueueAutoConnect_Struct));
    ServerQueueAutoConnect_Struct* sqac = (ServerQueueAutoConnect_Struct*)pack->pBuffer;
    
    sqac->loginserver_account_id = client.ls_account_id;
    sqac->ip_address = client.ip_address;
    strncpy(sqac->client_key, client.authorized_client_key.c_str(), sizeof(sqac->client_key) - 1);
    
    loginserver->SendPacket(pack);  // → ServerOP_QueueAutoConnect → ProcessQueueAutoConnect
}
```

### **Implementation Examples:**

**1. Master Queue Controller - Queue Decision Logic**
```cpp
// loginserver/client.cpp - Handle_Play (entry point)
void Client::Handle_Play(const char* data) {
    if(m_client_status != cs_logged_in) {
        LogError("Client sent a play request when they either were not logged in, discarding.");
        return;
    }
    
    if (data) {
        server.server_manager->SendUserToWorldRequest(data, m_account_id, 
                                                     m_connection->GetRemoteIP(), false, m_key);
    }
}

// loginserver/server_manager.cpp - SendUserToWorldRequest
void ServerManager::SendUserToWorldRequest(const char* server_id, unsigned int client_account_id, 
                                          uint32 ip, bool is_auto_connect, const std::string& client_key) {
    // Find target world server and send ServerOP_UsertoWorldReq packet
    UsertoWorldRequest* utwr = (UsertoWorldRequest*)outapp.Data();
    utwr->lsaccountid = client_account_id;
    utwr->ip = ip;
    utwr->FromID = is_auto_connect ? 1 : 0; // 0 = manual PLAY, 1 = auto-connect
    strncpy(utwr->forum_name, client_key.c_str(), sizeof(utwr->forum_name) - 1);
    
    (*iter)->GetConnection()->Send(ServerOP_UsertoWorldReq, outapp);
}

// world/login_server.cpp - ProcessUsertoWorldReq (main decision point)
void LoginServer::ProcessUsertoWorldReq(uint16_t opcode, EQ::Net::Packet& p) {
    // ... status checks, bans, IP limits ...
    
    uint32 effective_population = GetWorldPop();
    uint32 queue_cap = RuleI(Quarm, PlayerPopulationCap);
    
    if (effective_population >= queue_cap) {
        // Build connection request for centralized evaluation
        ConnectionRequest request = {};
        request.account_id = id;
        request.ls_account_id = utwr->lsaccountid;
        request.ip_address = utwr->ip;
        request.status = status;
        request.is_auto_connect = (utwr->FromID == 1);
        request.forum_name = utwr->forum_name;
        
        // CENTRALIZED DECISION: Let queue manager handle ALL queue logic
        bool should_override_capacity = queue_manager.EvaluateConnectionRequest(request, queue_cap, utwrs, nullptr);
        
        if (should_override_capacity) {
            LogInfo("APPROVED bypass for account [{}] - allowing connection", id);
        } else {
            LogInfo("QueueManager decision for account [{}] - response code [{}]", id, utwrs->response);
        }
    }
}
```

**2. Capacity Authority - Population Calculation**
```cpp
// world/world_queue.cpp - GetWorldPop calls EffectivePopulation
uint32 GetWorldPop() {
    return queue_manager.EffectivePopulation();
}

uint32 QueueManager::EffectivePopulation() {
    // Get base population from account reservation manager
    uint32 base_population = m_account_rez_mgr.Total();
    
    // Add test population offset for simulation  
    uint32 test_offset = m_cached_test_offset;
    
    // Calculate final effective population
    uint32 effective_population = base_population + test_offset;
    
    QueueDebugLog(2, "Account reservations: {}, test offset: {}, effective total: {}", 
        base_population, test_offset, effective_population);
    
    return effective_population;
}
```

**5. Login Server Communication - Queue Update Packets**
```cpp
// world/world_queue.cpp - SendQueueAutoConnect
void QueueManager::SendQueueAutoConnect(const QueuedClient& client) {
    auto pack = new ServerPacket(ServerOP_QueueAutoConnect, sizeof(ServerQueueAutoConnect_Struct));
    ServerQueueAutoConnect_Struct* sqac = (ServerQueueAutoConnect_Struct*)pack->pBuffer;
    
    sqac->loginserver_account_id = client.ls_accountid;
    sqac->ip_address = client.ip;
    strncpy(sqac->ip_addr_str, client.ip_str.c_str(), sizeof(sqac->ip_addr_str) - 1);
    strncpy(sqac->client_key, client.authorized_client_key.c_str(), sizeof(sqac->client_key) - 1);
    
    if (loginserver.Connected()) {
        loginserver.SendPacket(pack);
        QueueDebugLog(1, "Sent ServerOP_QueueAutoConnect for LS account [{}]", client.ls_accountid);
    }
    
    safe_delete(pack);
}

// world/world_queue.cpp - SendQueuedClientsUpdate
void QueueManager::SendQueuedClientsUpdate() {
    for (const auto& client : m_queued_clients) {
        auto pack = new ServerPacket(ServerOP_QueueDirectUpdate, sizeof(ServerQueueDirectUpdate_Struct));
        ServerQueueDirectUpdate_Struct* sqdu = (ServerQueueDirectUpdate_Struct*)pack->pBuffer;
        
        sqdu->ls_account_id = client.ls_accountid;
        sqdu->queue_position = client.position;
        sqdu->estimated_wait = client.estimated_wait;
        
        if (loginserver.Connected()) {
            loginserver.SendPacket(pack);
        }
        
        safe_delete(pack);
    }
    
    QueueDebugLog(1, "Sent queue position updates for {} clients", m_queued_clients.size());
}
```

---
### **Database Schema & Event-Driven Synchronization**

#### **Table Structures**
```sql
-- Real-time population tracking (QueueManager writes)
server_population: 
  - server_id, effective_population, last_updated
  - Updated every 15s for login server display

-- Account reservation backup (AccountRezMgr writes)  
active_ip_connections:
  - account_id, ip_address, last_seen, grace_period, is_in_raid
  - Updated every 5min for crash recovery

-- Queue configuration (Runtime toggles)
rule_values:
  - Quarm:EnableQueue, Quarm:PlayerPopulationCap
  - Quarm:TestPopulationOffset, Quarm:QueueBypassGMLevel
```

#### **Event-Driven Synchronization Patterns**

**Queue Operations (Immediate Sync)**
```
🔄 **On Queue Add:**
└─ 📝 Memory: m_queued_players[account_id] = entry
└─ 💾 DB: Update server_population SET effective_population = X (immediate)
└─ 📋 Log: "ADD_TO_QUEUE pos=1 wait=60s account=[12345] (memory + DB)"

🔄 **On Queue Remove:**
└─ 🗑️ Memory: m_queued_players.erase(account_id)
└─ 💾 DB: Update server_population SET effective_population = X (immediate)  
└─ 📋 Log: "REMOVE_FROM_QUEUE account=[12345] (memory + DB)"

🔄 **On Auto-Connect Success:**
└─ ⏳ Memory: Move to m_pending_connections
└─ 💾 DB: Update server_population (pending now counts as population)
└─ 📡 ServerOP_QueueAutoConnect → Login server
└─ 📋 Log: "AUTO_CONNECT account=[12345] client_key=[abc123]"

⚡ **Result:** Database always synchronized, zero queue data loss! 🎯
```

**Account Reservation Sync**
```
🏠 **On Reservation Add:**
└─ 📝 Memory: m_reservations[account_id] = {ip, timestamp, grace_period}
└─ 💾 DB: INSERT/UPDATE active_ip_connections (every 5min batch)
└─ 📊 Immediate population count update

🗑️ **On Reservation Remove:**
└─ 🧹 Memory: m_reservations.erase(account_id)
└─ 💾 DB: Mark for deletion in next sync cycle
└─ 📉 Immediate population count update

⏰ **Grace Period Events:**
└─ 🕐 Memory: Update grace_period timestamps
└─ 💾 DB: Batch sync every 5min (crash recovery data)
└─ 📊 Population changes update immediately
```




