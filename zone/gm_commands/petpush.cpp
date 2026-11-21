#include "../client.h"
#include "../command.h"
#include "../worldserver.h"
#include "../../common/player_pet_push.h"
#include "../../common/rulesys.h"

extern WorldServer worldserver;

void command_petpush(Client *c, const Seperator *sep)
{
    auto *rules = RuleManager::Instance();
    const auto ruleset = rules->GetActiveRulesetID();
    if (sep->argnum == 0) {
        c->Message(Chat::White, "Player-pet melee push: %.3f (%.1f%% of normal), ruleset %u.",
            PlayerPetPush::EffectiveMultiplier(RuleR(Pets, PlayerMeleePushMultiplier)),
            PlayerPetPush::EffectiveMultiplier(RuleR(Pets, PlayerMeleePushMultiplier)) * 100.0f,
            static_cast<unsigned>(ruleset));
        c->Message(Chat::White, "Usage: #petpush 0 to disable, #petpush 0.25 for reduced push, #petpush 1 for normal.");
        return;
    }
    float multiplier;
    if (sep->argnum != 1 || !PlayerPetPush::ParseMultiplier(sep->arg[1], multiplier)) {
        c->Message(Chat::Red, "Use #petpush with a number from 0 to 1. Example: #petpush 0.25");
        return;
    }
    if (!worldserver.Connected()) {
        c->Message(Chat::Red, "World is disconnected; pet push was not changed.");
        return;
    }
    if (ruleset < 0) {
        c->Message(Chat::Red, "No active ruleset is available; pet push was not changed.");
        return;
    }
    const auto value = fmt::format("{:.9g}", multiplier);
    auto result = database.QueryDatabase(fmt::format(
        "INSERT INTO rule_values (ruleset_id, rule_name, rule_value, notes) VALUES ({}, '{}', '{}', '{}') "
        "ON DUPLICATE KEY UPDATE rule_value = VALUES(rule_value)",
        ruleset, PlayerPetPush::RuleName, value,
        "Player-owned pet melee push against NPCs. 0 disables; 1 is normal."));
    if (!result.Success()) {
        c->Message(Chat::Red, "Could not save pet push; no live settings were changed.");
        return;
    }
    rules->SetRule(PlayerPetPush::RuleName, value, nullptr, false, true);
    auto *packet = new ServerPacket(ServerOP_PlayerPetPushUpdate, sizeof(ServerPlayerPetPushUpdate_Struct));
    auto *update = reinterpret_cast<ServerPlayerPetPushUpdate_Struct *>(packet->pBuffer);
    update->ruleset_id = ruleset;
    const bool sent = worldserver.SendPacket(packet);
    safe_delete(packet);
    c->Message(Chat::White, "Player-pet melee push saved at %.1f%% of normal for ruleset %u.",
        multiplier * 100.0f, static_cast<unsigned>(ruleset));
    c->Message(sent ? Chat::White : Chat::Red, sent
        ? "Update sent to running zones using this ruleset. New zones will load the saved setting."
        : "Saved and applied here, but the world update failed. Repeat the command when connected to update other zones.");
}
