#ifndef HARDCOREGHOST_H
#define HARDCOREGHOST_H

#include <unordered_map>
#include <queue>

#include "../common/types.h"
#include "../common/linked_list.h"
#include "../common/servertalk.h"
#include "../common/bodytypes.h"
#include "../common/eq_constants.h"

// Include necessary headers
#include "mob.h"
#include "client.h"
#include "position.h"

// Declare the HardcoreGhost class
class HardcoreGhost : public Mob
{
public:
    HardcoreGhost(const NPCType *npc_type_data, const glm::vec4& position);

    // Static method to spawn a hardcore ghost
    static void Spawn(Client* client);
	bool Process() override;
	bool Save() override;
	bool IsNPC() const override { return false; }
	bool IsHardcoreGhost()	const override { return true; }
	void ShowQuickStats(Client* client);

	// TODO for adding items
	// void AddBotItem(
	// 	uint16 slot_id,
	// 	uint32 item_id,
	// 	int16 charges = -1
	// );

    //abstract virtual function implementations required by base abstract class
	virtual bool Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby = 0, bool bufftic = false);
	virtual void Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false);
	virtual bool Attack(Mob* other, int hand = EQ::invslot::slotPrimary, int damagePct = 100);
	virtual bool HasRaid() { return false; }
	virtual bool HasGroup() { return false; }
	virtual Raid* GetRaid() { return nullptr; }
	virtual Group* GetGroup() { return nullptr; }
	virtual inline bool IsBerserk() { return false; }
	virtual void SetAttackTimer(bool trigger = false);
	virtual bool CombatRange(Mob* other, float dist_squared = 0.0f, bool check_z = false, bool pseudo_pos = false);
	virtual uint8 Disarm(float chance);
	virtual bool AvoidDamage(Mob* attacker, int32 &damage, bool noRiposte = false, bool isRangedAttack = false);
	virtual bool AvoidanceCheck(Mob* attacker, EQ::skills::SkillType skillinuse);
	virtual void TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, int32 minBase = 0, int32 damageBonus = 0);
	virtual bool TryFinishingBlow(Mob *defender, EQ::skills::SkillType skillinuse, uint32 dmgBonus = 0);
	virtual void DoRiposte(Mob* defender);
};

#endif // HARDCOREGHOST_H
