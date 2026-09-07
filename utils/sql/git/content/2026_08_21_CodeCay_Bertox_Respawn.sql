-- Bertoxxulous event trash must bypass the guild-instance minimum respawn
-- so the encounter's scripted waves can spawn on the 3-minute-50-second cadence.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 230000
WHERE `id` IN (200236, 200237, 200238, 200247, 200251, 200268);
