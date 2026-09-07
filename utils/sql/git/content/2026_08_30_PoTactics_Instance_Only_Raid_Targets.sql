-- Keep Plane of Tactics raid encounters out of the open world while
-- preserving their spawnpoints for guild-instance encounter scripts.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
    361379, -- Rallos Zek encounter trigger
    361403, -- Tallon Zek
    369265  -- Vallon Zek
);
