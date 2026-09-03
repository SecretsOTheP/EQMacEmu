-- Keep Plane of Earth raid encounters out of the open world while preserving
-- the ordinary Earth A ring mobs. Ring scripts remain instance-only.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
    366256, -- Tantisala Jaggedtooth
    369376, -- Rathe Councilman
    369377, -- Rathe Councilman
    369378, -- Rathe Councilman
    369379, -- Rathe Councilman
    369380, -- Rathe Councilman
    369381, -- Rathe Councilman
    369382, -- Rathe Councilman
    369383, -- Rathe Councilman
    369384, -- Rathe Councilman
    369385, -- Rathe Councilman
    369386, -- Rathe Councilman
    369387, -- Rathe Councilman
    369493, -- War Chieftan Birak
    369494, -- War Chieftan Galronar
    369492, -- War Chieftan Awisano
    369491  -- Warlord Gintolaken
);

-- Two days, eighteen hours for the static bosses, four Earth A ring bosses,
-- and the Rathe Council's Avatar of Earth.
UPDATE `npc_types`
SET `loot_lockout` = 237600,
    `instance_spawn_timer_override` = 237600000
WHERE `id` IN (
    218038, -- Tantisala Jaggedtooth
    218374, -- Peregrin Rockskull (Stone)
    218360, -- A Monsterous Mudwalker (Mud)
    218363, -- Derugoak Bloodwalker (Vine)
    218413, -- A Perfected Warder of Earth (Dust)
    222035, -- War Chieftan Birak
    222036, -- War Chieftan Galronar
    222037, -- War Chieftan Awisano
    222038, -- Warlord Gintolaken
    222040  -- Avatar of Earth / Rathe Council
);

UPDATE `spawn2`
SET `respawntime` = 237600,
    `variance` = 0
WHERE `id` IN (366256, 369493, 369494, 369492, 369491);

-- Give War Drums of the Rathe two independent 10% rolls, matching the
-- Lute of the Flowing Waters model without affecting Peregrin's other loot.
DELETE FROM `lootdrop_entries`
WHERE `lootdrop_id` = 8559
  AND `item_id` = 27998;

INSERT INTO `lootdrop_entries`
    (`lootdrop_id`, `item_id`, `item_charges`, `equip_item`, `chance`, `minlevel`, `maxlevel`, `multiplier`, `disabled_chance`, `expansion`, `min_expansion`, `max_expansion`, `min_looter_level`, `item_loot_lockout_timer`, `content_flags_disabled`, `content_flags`)
VALUES
    (218374, 27998, 1, 0, 100, 0, 255, 1, 0, 0, -1, -1, 0, 0, NULL, NULL)
ON DUPLICATE KEY UPDATE
    `chance` = VALUES(`chance`),
    `multiplier` = VALUES(`multiplier`);

INSERT INTO `loottable_entries`
    (`loottable_id`, `lootdrop_id`, `multiplier`, `probability`, `droplimit`, `mindrop`, `multiplier_min`)
VALUES
    (4478, 218374, 2, 10, 0, 0, 0)
ON DUPLICATE KEY UPDATE
    `multiplier` = VALUES(`multiplier`),
    `probability` = VALUES(`probability`),
    `droplimit` = VALUES(`droplimit`),
    `mindrop` = VALUES(`mindrop`),
    `multiplier_min` = VALUES(`multiplier_min`);
