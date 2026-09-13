-- Each Plane of Air Avatar now awards six of its matching key essence.

UPDATE loottable_entries
SET
    probability = 100,
    droplimit = 6,
    mindrop = 6,
    multiplier = 1,
    multiplier_min = 0
WHERE lootdrop_id IN (
    23333,  -- Mystical Essence of Wind
    23330,  -- Mystical Essence of Smoke
    115543, -- Mystical Essence of Mist
    23327   -- Mystical Essence of Dust
);
