-- Apply the standard 60-hour progression lockout to Lord Mithaniel Marr.
UPDATE `npc_types`
SET `loot_lockout` = 216000
WHERE `id` = 220020;
