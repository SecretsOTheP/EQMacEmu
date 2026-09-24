-- Keep Plane of Water raid targets out of the open world and Guild 1 outside
-- active quakes while preserving Fishlord quests, traps, and ordinary spawns.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1,
    `respawntime` = 237600,
    `variance` = 0
WHERE `id` IN (
    365647, -- Coirnav the Avatar of Water
    366321, -- Guardian of Coirnav
    366616, -- Ofossaa the Enlightened
    367443, -- Krziik the Mighty
    366444, -- Grioihin the Wise
    365909  -- Hydrotha
);

-- Two days, eighteen hours for Water raid loot and guild-instance respawns.
UPDATE `npc_types`
SET `loot_lockout` = 237600,
    `instance_spawn_timer_override` = 237600000
WHERE `id` IN (
    216048, -- Coirnav the Avatar of Water
    216053, -- Guardian of Coirnav
    216040, -- Ofossaa the Enlightened
    216041, -- Krziik the Mighty
    216042, -- Grioihin the Wise
    216043  -- Hydrotha
);

-- Grioihin is a guaranteed spawn and returns every eighteen hours.
UPDATE `spawn2`
SET `respawntime` = 64800
WHERE `id` = 366444;

UPDATE `npc_types`
SET `loot_lockout` = 64800,
    `instance_spawn_timer_override` = 64800000
WHERE `id` = 216042;

-- Raise Lute of the Flowing Waters from 5% to 10% per loot-table roll while
-- retaining two rolls and keeping the lootdrop weights at a total of 100.
UPDATE `lootdrop_entries`
SET `chance` = CASE `item_id`
    WHEN 10945 THEN 40 -- Ring of Algae
    WHEN 16605 THEN 40 -- Seaweed Woven Leggings
    WHEN 27994 THEN 20 -- Lute of the Flowing Waters
END
WHERE `lootdrop_id` = 8503
  AND `item_id` IN (10945, 16605, 27994);
