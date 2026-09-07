-- Keep The Seventh Hammer out of the open-world Plane of Justice while
-- preserving the encounter in guild instances, with a 2.5-day loot lockout.
UPDATE npc_types SET loot_lockout = 216000 WHERE id = 201074;
UPDATE spawn2 SET raid_target_spawnpoint = 1 WHERE id = 345320;
