## 🔄 EQMacEmu Player Connection & Queue Flow

### **Complete Request/Response Flow (SIMPLIFIED ARCHITECTURE):**
<!-- ``` -->
🎮 *Client clicks PLAY on server*
↓
└─📁 📨 `loginserver/client.cpp`
    └─[Client::Handle_Play]
↓
📁📨 `loginserver/server_manager.cpp`
[ServerManager::SendUserToWorldRequest]
    └─ ❔ Queued on a server?
        └─ Clean up other server queues first
        └─ Check target server queue toggle
            └─ [QueueManager::RemoveFromQueue] (if toggling off)
    └─ target_server->GetConnection()->Send[ServerOP_UsertoWorldReq]
↓
└─📁🌍 `world/login_server.cpp`
    └─ *OnMessage(ServerOP_UsertoWorldReq)*
        └─[LoginServer::ProcessUsertoWorldReq]
            └─ ❔ Check account status, bans, locks
                └─[utwrs->response] = -1,-2,-3
            └─ ❔ < Max Population
                └─[utwrs->response] = 1 ✅
            └─ ❔ >= Max Population
                └─[utwrs->response] = -6 ❌ (SIMPLIFIED: Just return -6)
            └─ *SendPacket(ServerOP_UsertoWorldResp)*
↓
📁📨 `loginserver/world_server.cpp`
    └─ *OnMessage(ServerOP_UsertoWorldResp)*
        └─[WorldServer::ProcessUsertoWorldResp]   
            └─✅ Success (1)       
                └─[WorldServer::SendClientAuth]
                    ↓
                    └─📁🌍 `world/login_server.cpp`
                        └─*OnMessage(ServerOP_LSClientAuth)*
                            └─[LoginServer::ProcessLSClientAuth]
                                └─[client_list.CLEAdd]
            └─ 🔄 Queue (-6) 
                └─ [WorldServer::HandleCapacityQueueLogic]
                    └─ Uses same EvaluateConnectionRequest logic
                    └─ Adds to queue directly in login server
                    └─ Updates server list automatically
			            └─ ❌ Other failures (-1,-2,-3,-4,-5,-7)
                └─ Send appropriate error to client
↓
🎮 *Client receives connection approval and connects to world server*
↓
└─📁🌍 `world/client.cpp`
    └─[EQ::Net::TCPConnection::OnConnect]
        └─ Creates new Client object
        └─ Adds to client_list.list
    ↓
    └─🎮 *Client sends OP_SendLoginInfo packet*
    ↓  
    └─[Client::HandleSendLoginInfoPacket]
        └─ ❔ Validate client exists in CLEAdd list
        └─ ❔ Check grace whitelist (redundant GM check here)
            └─ 🛡️ **Note**: GMs already bypassed upstream
        └─ ❔ Account active, IP limits, etc.
        └─ Account authenticated and logged in ✅
<!-- ``` -->
### **Timers:**

📁 📨 `loginserver/main.cpp`
    └─[main]
        └─[proccess_timer.Start] (32ms)
        └─[maintenance_timer.Start] (3000ms)
            └─server.server_manager->[ServerManager::PeriodicMaintenance]
                └─GetQueueManager().[QueueManager::PeriodicMaintenance]
↓
└─📁🌍 `world/login_server.cpp`
    └─[LoginServer::ProcessQueueAuthorization]
        └─m_statusupdate_timer.reset(new EQ::Timer(LoginServer_StatusUpdateInterval, true, [this](EQ::Timer* t){SendStatus();}
            └─[LoginServer::SendStatus]
            ↓
            📁📨 `loginserver/world_server.cpp`
            └─*OnMessage(ServerOP_LSStatus)*
                    └─[WorldServer::ProcessLSStatus]
                        └─[WorldServer::Handle_LSStatus]
                            └─(m_queue_manager.)[QueueManager::CheckForExternalChanges];
                            └─server.client_manager->[UpdateServerList]
                            ↓
                            └─📁📨 `loginserver/client_manager.cpp`
                                └─ server.client_manager->[ClientManager::UpdateServerList]
                                └─ server.client_manager->[SendServerListPacket()]
                            

Timer [EQTimeTimer(600000)];           // 10 minutes - EQ time updates
Timer [InterserverTimer(10000)];       // 10 seconds - DB ping/reconnect  
Timer [NextQuakeTimer(900000)];        // 15 minutes - earthquake events
Timer [player_event_process_timer(1000)]; // 1 second - player events
Timer [ChannelListProcessTimer(60000)]; // 1 minute - channel cleanup
Timer [ClientConnectionPruneTimer(60000)]; // 1 minute - connection cleanup

### **Packet Types:**
- [ServerOP_UsertoWorldReq] = Login → World (connection request)
- [ServerOP_UsertoWorldResp] = World → Login (allow/deny response) 
- [ServerOP_LSClientAuth] = Login → World (Add client to WS)
- [ServerOP_QueueAuthorization] = Login → World (authorize queued player)
- [ServerOP_QueueAutoConnect] = World → Login (auto-connect request)

### **Key Decision Points:**

1. **📨 Login Server** = Handles queue toggle behavior and cross-server queue cleanup
2. **🌍 World Server** = Authoritative capacity decision maker
3. **🎮 Client** = Sees queue status via server list updates

### **Queue Toggle Architecture:**

**NEW: Simplified Queue Management**
- Cross-server queue cleanup handled in `ServerManager::SendUserToWorldRequest`
- Target server queue toggle handled in `ServerManager::SendUserToWorldRequest` 
- Capacity queueing handled in `WorldServer::HandleCapacityQueueLogic` when -6 response received
- `QueueManager::AddToQueue` now only adds players to queue (no complex packet logic)

### **Response Codes:**
- [1] = Success (allow connection)
- [-6] = Queue player (server at capacity)
- [-3] = World at capacity (standard)
- [-2] = Banned
- [-1] = Suspended

📁📨 `loginserver/queue_manager.cpp`
    class QueueManager - Performance/Capacity Logic:
        uint32 GetEffectivePopulation() {
            return GetWorldPopulation() + pending_connections.size();
        }
    ✅ Authoritative for queue decisions
    ✅ Handles pending connections locally (login server knows about auto-connects)
    ✅ Performance critical - used for capacity check

📁🌍 `/world/account_reservation_manager.cpp`
    class AccountRezMgr - Display/Reporting:
        uint32 effective_population = m_account_rez_mgr.Total() + test_offset;
✅ Display purposes - server list population, status reporting
✅ Includes all reservations (active + grace period players)
✅ Test offset for simulation - useful for testing queue behavior


└📁🌍 `world/main.cpp`
[ServerManager::SendUserToWorldRequest]
    └─ ⏲️ QueueAdvancementTimer(3000ms)
        └─ Clean up other server queues first