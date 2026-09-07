-- Make Bastion of Thunder access an active Plane of Storms progression path
-- instead of a multi-day open-world respawn bottleneck.

-- Medallions are added once by the Plane of Storms encounter scripts so
-- loot modifiers cannot duplicate these progression items. Clean up the
-- database-loot implementation if an older version of this patch was
-- applied.
DELETE lte FROM `loottable_entries` AS lte
JOIN `lootdrop` AS ld ON ld.`id` = lte.`lootdrop_id`
WHERE ld.`name` IN (
    'Plane of Storms - Srerendi Esoteric Medallion',
    'Plane of Storms - Krendic Esoteric Medallion',
    'Plane of Storms - Kelek`Vor Esoteric Medallion'
);

DELETE lde FROM `lootdrop_entries` AS lde
JOIN `lootdrop` AS ld ON ld.`id` = lde.`lootdrop_id`
WHERE ld.`name` IN (
    'Plane of Storms - Srerendi Esoteric Medallion',
    'Plane of Storms - Krendic Esoteric Medallion',
    'Plane of Storms - Kelek`Vor Esoteric Medallion'
);

DELETE FROM `lootdrop`
WHERE `name` IN (
    'Plane of Storms - Srerendi Esoteric Medallion',
    'Plane of Storms - Krendic Esoteric Medallion',
    'Plane of Storms - Kelek`Vor Esoteric Medallion'
);

-- Remove the original single-medallion rolls from the three leaders. Their
-- scripts supply 5-8 medallions normally, or 3 with a 50% chance of 2 more
-- in Guild 1.
DELETE FROM `loottable_entries`
WHERE (`loottable_id`, `lootdrop_id`) IN (
    (96975, 22888), -- Jeplak
    (96974, 22889), -- Gurebk
    (96976, 22886)  -- Neffiken
);

-- Remove the original fixed-three packages from the six secondary minibosses.
-- MiniBosses.lua supplies 1-3 matching medallions normally and 1-2 in
-- Guild 1.
DELETE FROM `loottable_entries`
WHERE (`loottable_id`, `lootdrop_id`) IN (
    (1080,  22886), -- Laruken and Zertuken: Kelek`Vor
    (96973, 22888), -- Paruek: Srerendi
    (1083,  22888), -- Faruek: Srerendi
    (1085,  22889)  -- Pendubk and Solnebk: Krendic
);

-- Three-hour leader respawns in the open world.
UPDATE `spawn2`
SET `respawntime` = 10800,
    `variance` = 0
WHERE `id` IN (346559, 346640, 346711);

-- Use a six-hour leader respawn inside guild instances rather than the
-- global 18-hour instance minimum. Guild 1 applies its normal PvP variance.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 21600000
WHERE `id` IN (210251, 210332, 210403);

-- Keep the six secondary medallion minibosses from becoming a faster guild 1
-- progression path. PvE instances use 18 hours; guild 1 applies its existing
-- 1.0-to-1.5 multiplier, while their six-hour open-world timers are unchanged.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 64800000
WHERE `id` IN (210026, 210027, 210028, 210029, 210032, 210033);

-- Preserve the faction giants' existing natural respawns inside guild
-- instances so their medallion drops remain a repeatable progression path.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 6650000
WHERE `id` IN (210015, 210022, 210035, 210038, 210056, 210099, 210407, 210411, 210419);

-- Do not alter the unrelated fast bridge giant that shared the old Srerendi
-- component lootdrop.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 0
WHERE `id` = 210490;
