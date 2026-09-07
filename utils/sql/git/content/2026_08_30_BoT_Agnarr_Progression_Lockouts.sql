-- Apply a 2-day, 18-hour raid lockout to the Bastion of Thunder progression bosses.
UPDATE `npc_types`
SET `loot_lockout` = 237600
WHERE `id` IN (209054, 209053, 209026);
