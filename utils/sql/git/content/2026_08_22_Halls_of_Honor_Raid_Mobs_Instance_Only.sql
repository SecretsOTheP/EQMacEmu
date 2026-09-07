-- Keep Halls of Honor raid-event bosses out of the open world.
-- These spawn points remain available to raid instances.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
    360971, -- Rhaliq Trell
    360859, -- Trydan Faye
    361021, -- Alekson Garn
    -- Temple of Marr raid
    365500, -- Lord Mithaniel Marr
    367163, -- Ralthazor, Champion of Marr
    367227, -- Edium, Guardian of Marr
    366604  -- Halon of Marr
);
