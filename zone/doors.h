#ifndef DOORS_H
#define DOORS_H

#include "mob.h"
#include "../common/repositories/doors_repository.h"

class Client;
class Mob;
class NPC;
struct Door;

class Doors : public Entity
{
public:
	~Doors();

	Doors(const char *model, const glm::vec4& position, uint8 open_type = 58, uint16 size = 100);
	Doors(const DoorsRepository::Doors &door);

	bool DoorKeyCheck(Client *sender, uint32 &playerkey);
	bool IsDoor() const { return true; }
	bool IsDoorOpen() { return is_open; }
	bool IsMoveable() { return can_open; }
	bool IsInstanceOnly() { return instance_only; }
	bool IsTeleport() { return teleport; }
	bool Process();
	bool EnterPendingGuildInstance(Client* sender);
	bool triggered;
	char *GetDestinationZone() { return destination_zone_name; }
	char *GetDoorName() { return door_name; }
	const glm::vec4 GetDestination() const { return m_destination; }
	const glm::vec4 &GetPosition() const { return m_position; }
	int GetIncline() { return incline; }
	int GetInvertState() { return invert_state; }
	uint8 GetDoorID() { return door_id; }
	uint8 GetNoKeyring() { return no_key_ring; }
	uint8 GetOpenType() { return open_type; }
	uint8 GetTriggerDoorID() { return trigger_door; }
	uint8 GetTriggerType() { return trigger_type; }
	uint16 GetLockpick() { return lockpick; }
	uint16 GetSize() { return size; }
	uint32 GetAltKeyItem() { return alt_key_item_id; }
	uint32 GetClientVersionMask() { return client_version_mask; }
	uint32 GetDoorDBID() { return database_id; }
	uint32 GetDoorParam() { return door_param; }
	uint32 GetEntityID() { return entity_id; }
	uint32 GetKeyItem() { return key_item_id; }
	void DumpDoor();
	void ForceClose(Mob *sender, bool alt_mode = false);
	void ForceOpen(Mob *sender, bool alt_mode = false);
	void HandleClick(Client* sender, uint8 trigger, bool floor_port = false);
	void HandleLift(Client* sender);
	void NPCOpen(NPC* sender, bool alt_mode=false);
	void OpenDoor(Mob *sender, bool force = false);
	void ToggleState(Mob *sender);
	void SetDoorName(const char *name);
	void SetEntityID(uint32 entity) { entity_id = entity; }
	void SetIncline(int in);
	void SetKeyItem(uint32 in) { key_item_id = in; }
	void SetInvertState(int in);
	void SetLocation(float x, float y, float z);
	void SetLockpick(uint16 in) { lockpick = in; }
	void SetNoKeyring(uint8 in) { no_key_ring = in; }
	void SetOpenState(bool st) { is_open = st; }
	void SetOpenType(uint8 in);
	void SetPosition(const glm::vec4& position);
	void SetSize(uint16 size);

	bool HasDestinationZone() const;
	bool IsDestinationZoneSame() const;

	float GetX();
	float GetY();
	float GetZ();
			
private:

	bool      m_has_destination_zone = false;
	bool      m_same_destination_zone = false;
	uint32     database_id;
	uint8      door_id;
	char       zone_name[32];
	char       door_name[32];
	glm::vec4  m_position;
	int	       incline;
	uint8      open_type;
	uint16     lockpick;
	uint32     key_item_id;
	uint8      no_key_ring;
	uint32     alt_key_item_id;
	uint8      trigger_door;
	uint8      trigger_type;
	uint32     door_param;
	uint16     size;
	int        invert_state;
	uint32     entity_id;
	bool       is_open;
	bool       is_lift;
	Timer      close_timer;
	Timer      lift_timer;
	uint8      close_time;
	bool       can_open;
	bool	   instance_only;
	char       destination_zone_name[16];
	glm::vec4  m_destination;
	uint32	   client_version_mask;
	bool	   teleport;
	bool	guild_zone_door;
};
#endif
