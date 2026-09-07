-- Award Bastion of Thunder key progression loot through Lua so loot modifiers
-- cannot duplicate key components.
DELETE FROM `loottable_entries`
WHERE `lootdrop_id` IN (
    115507, -- Sandstorm Gem
    22963,  -- Lightning Gem
    23008,  -- Blizzard Gem
    22970,  -- Tornado Gem
    22965,  -- Sandstorm Sphere
    22961,  -- Lightning Sphere
    22968,  -- Blizzard Sphere
    22969,  -- Tornado Sphere
    22768,  -- Ring of Torden
    22767   -- Unadorned Symbol of Torden
);

DELETE FROM `lootdrop_entries`
WHERE `lootdrop_id` IN (
    115507, 22963, 23008, 22970,
    22965, 22961, 22968, 22969,
    22768, 22767
);

DELETE FROM `lootdrop`
WHERE `id` IN (
    115507, 22963, 23008, 22970,
    22965, 22961, 22968, 22969,
    22768, 22767
);
