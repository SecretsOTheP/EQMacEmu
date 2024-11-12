#ifndef HARDCOREGHOST_H
#define HARDCOREGHOST_H

#include <unordered_map>
#include <queue>
#include <string>
#include <glm/vec4.hpp>

#include "../common/types.h"
#include "../common/linked_list.h"
#include "../common/servertalk.h"
#include "../common/bodytypes.h"
#include "../common/eq_constants.h"
#include "../common/inventory_profile.h"

// Include necessary headers
#include "mob.h"
#include "client.h"
#include "position.h"

// Declare the HardcoreGhost class
class HardcoreGhost : public Mob
{
public:
    HardcoreGhost(const NPCType *npc_type_data, uint32_t ghost_id);

    // Static method to load a hardcore ghost
    static HardcoreGhost* LoadGhost(const std::string& ghost_name);

    void Spawn(const glm::vec4& in_position, Client* client = nullptr);
    bool Process() override;
    bool Save() override;
    bool IsNPC() const override { return false; }
    bool IsHardcoreGhost() const override { return true; }
    void ShowQuickStats(Client* client);

    // Static methods to create and fill NPCType structures
    static NPCType* CreateDefaultNPCTypeStructForGhost(
        const std::string& ghostName,
        const std::string& ghostLastName,
        uint8 ghostLevel,
        uint16 ghostRace,
        uint8 ghostClass,
        uint8 gender
    );

    static NPCType* FillNPCTypeStruct(
        const std::string& ghostName,
        const std::string& ghostLastName,
        uint8 ghostLevel,
        uint16 ghostRace,
        uint8 ghostClass,
        uint8 gender,
        float size,
        uint32 face,
        uint32 hairStyle,
        uint32 hairColor,
        uint32 eyeColor,
        uint32 eyeColor2,
        uint32 beard,
        uint32 beardColor
    );

    void EquipItems();
    void GetGhostItems(EQ::InventoryProfile &inv);
	EQ::ItemInstance* GetGhostItem(uint16 slot_id);
    void GhostAddEquipItem(uint16 slot_id, uint32 item_id);
	uint32 GetEquipment(uint8 material_slot) const;


    // Abstract virtual function implementations required by base abstract class
    virtual bool Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby = 0, bool bufftic = false) override;
    virtual void Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false) override;
    virtual bool Attack(Mob* other, int hand = EQ::invslot::slotPrimary, int damagePct = 100) override;
    virtual bool HasRaid() override { return false; }
    virtual bool HasGroup() override { return false; }
    virtual Raid* GetRaid() override { return nullptr; }
    virtual Group* GetGroup() override { return nullptr; }
    virtual inline bool IsBerserk() override { return false; }
    virtual void SetAttackTimer(bool trigger = false) override;
    virtual bool CombatRange(Mob* other, float dist_squared = 0.0f, bool check_z = false, bool pseudo_pos = false) override;
    virtual uint8 Disarm(float chance) override;
    virtual bool AvoidDamage(Mob* attacker, int32 &damage, bool noRiposte = false, bool isRangedAttack = false) override;
    virtual bool AvoidanceCheck(Mob* attacker, EQ::skills::SkillType skillinuse) override;
    virtual void TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, int32 minBase = 0, int32 damageBonus = 0) override;
    virtual bool TryFinishingBlow(Mob *defender, EQ::skills::SkillType skillinuse, uint32 dmgBonus = 0) override;
    virtual void DoRiposte(Mob* defender) override;

protected:
    EQ::InventoryProfile m_inv;

private:
    // Class Members
    uint32_t _ghostID;
	uint32_t equipment[EQ::invslot::EQUIPMENT_COUNT];	//this is an array of item IDs
};

#endif // HARDCOREGHOST_H
