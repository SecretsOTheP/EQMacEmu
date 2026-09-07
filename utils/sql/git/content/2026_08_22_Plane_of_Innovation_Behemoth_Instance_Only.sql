-- Keep the scripted Manaetic Behemoth/Nitram event out of open-world
-- Plane of Innovation. These static controllers remain available in raids.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
    345224, -- Weapon_Event_Master
    345273, -- Manaetic Behemoth
    345275  -- Nitram Anizok
);
