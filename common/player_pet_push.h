#ifndef EQEMU_PLAYER_PET_PUSH_H
#define EQEMU_PLAYER_PET_PUSH_H

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace PlayerPetPush {
constexpr const char *RuleName = "Pets:PlayerMeleePushMultiplier";
constexpr float DefaultMultiplier = 0.25f;

inline bool ParseMultiplier(const char *text, float &value)
{
    if (!text || !*text) return false;
    char *end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text, &end);
    if (end == text || *end || errno == ERANGE || !std::isfinite(parsed) || parsed < 0.0f || parsed > 1.0f) return false;
    value = parsed;
    return true;
}

inline float EffectiveMultiplier(float value)
{
    if (!std::isfinite(value)) return DefaultMultiplier;
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

// Resolve ownership without GetOwner(), which may modify an orphaned pet.
// Charmed NPCs use owner_id; temporary/swarm pets can use swarm owner instead.
template <typename MobType, typename Lookup>
float Scale(float force, MobType *attacker, bool npc_target, bool melee, float multiplier, Lookup lookup)
{
    if (force <= 0.0f || !npc_target || !melee || !attacker || !attacker->IsNPC()) return force;
    auto owner = attacker->GetOwnerID() ? lookup(attacker->GetOwnerID()) : nullptr;
    if (!owner) {
        const auto swarm_owner = attacker->CastToNPC()->GetSwarmOwner();
        if (swarm_owner) owner = lookup(swarm_owner);
    }
    return owner && owner->IsClient() ? force * EffectiveMultiplier(multiplier) : force;
}
}
#endif
