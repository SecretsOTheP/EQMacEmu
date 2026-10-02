-- Apply with zones stopped. Restart zones afterward so active NPC and spawn
-- timers do not retain cached values.
-- Xanamech is spawned by Nitram's quest, not by its own spawn2 row.
START TRANSACTION;

UPDATE `npc_types`
SET `loot_lockout` = 0,
    `raid_target` = 1
WHERE `id` = 206208;

-- Nitram's spawnpoint controls when another Xanamech attempt can begin.
-- The quest applies a one-second scripted timer after the flagging window.
UPDATE `spawn2`
SET `respawntime` = 1
WHERE `id` = 345275
  AND `zone` = 'poinnovation';

-- Clear existing Xanamech lockouts for every character and saved Nitram
-- countdowns in every guild instance and the open world.
DELETE FROM `character_loot_lockouts` WHERE `npctype_id` = 206208;
DELETE FROM `respawn_times` WHERE `id` = 345275;

COMMIT;
