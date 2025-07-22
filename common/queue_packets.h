#ifndef EQEMU_QUEUE_PACKETS_H
#define EQEMU_QUEUE_PACKETS_H

#include "types.h"

#pragma pack(1)

// Queue-related opcodes (separate from main servertalk.h to avoid full rebuilds)
#define ServerOP_QueueAuthorization		0x4012
#define ServerOP_QueueAutoConnect		0x4013
#define ServerOP_QueuePositionQuery		0x4014	// Login->World: Request queue position for account
#define ServerOP_QueuePositionResponse	0x4015	// World->Login: Reply with queue position
#define ServerOP_QueueDirectUpdate		0x4016	// World->Login: Send pre-built server list packet to specific client
#define ServerOP_QueueBatchRemoval		0x4017	// Login->World: Batch removal of disconnected clients from queue
#define ServerOP_QueueBatchUpdate		0x4018	// World->Login: Batch queue position updates for multiple clients
#define ServerOP_RemoveFromQueue		0x4019	// Login->World: Remove single disconnected client from queue

// Queue-related packet structures
struct ServerQueueAuthorization_Struct {
	uint32 account_id;
	uint32 authorization_timestamp;  // When this authorization was granted
	uint32 timeout_seconds;          // How long this authorization is valid (default: 300 = 5 minutes)
};

struct ServerQueueAutoConnect_Struct {
	uint32 loginserver_account_id;
	uint32 world_id;
	uint32 from_id;
	uint32 to_id;
	uint32 ip_address;
	char ip_addr_str[64];
	char forum_name[31];
	char client_key[11];  // Unique key of the client that was authorized (10 chars + null terminator)
};

struct ServerQueuePositionQuery_Struct {
	uint32 loginserver_account_id;  // Which account to query position for
};

struct ServerQueuePositionResponse_Struct {
	uint32 loginserver_account_id;  // Account this response is for
	uint32 queue_position;          // 0 = not queued, >0 = position in queue
};

struct ServerQueueDirectUpdate_Struct {
	uint32 ls_account_id;       // Account identifier for lookup
	uint32 ip_address;           
	uint32 queue_position;      // New queue position (0 = not queued)
	uint32 estimated_wait;      // Estimated wait time in seconds
};

struct ServerQueueBatchUpdate_Struct {
	uint32 update_count;        // Number of queue updates in this batch
	// Followed by update_count instances of ServerQueueDirectUpdate_Struct
	// Use: ServerQueueDirectUpdate_Struct* updates = (ServerQueueDirectUpdate_Struct*)((char*)packet_data + sizeof(ServerQueueBatchUpdate_Struct));
};

struct ServerQueueRemoval_Struct {
	uint32 ls_account_id;       // Account to remove from queue when client disconnects
};

#pragma pack()

#endif // EQEMU_QUEUE_PACKETS_H 