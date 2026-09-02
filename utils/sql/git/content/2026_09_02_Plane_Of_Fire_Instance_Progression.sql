-- Keep Plane of Fire raid encounters in guild instances.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
    366356, -- Arch Mage Yozanni
    367351, -- Blazzax the Omnifiend
    366505, -- General Reparm
    366735, -- Quavonis Firetail
    367063, -- Babnoxis the Spider Queen
    366652, -- Criare Sunmane
    367462, -- General Druav Flamesinger
    367325, -- Jaxoliz Dawneyes
    366567, -- Magmaton
    367421, -- Pyronis
    367088  -- Guardian of Doomfire
);

-- Use a uniform 60-hour respawn for the static raid encounters.
UPDATE `spawn2`
SET `respawntime` = 216000
WHERE `id` IN (
    366356, 367351, 366505, 366735, 367063, 366652,
    367462, 367325, 366567, 367421, 367088
);

-- Match Guild 2+ respawns to the 60-hour schedule.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 216000000
WHERE `id` IN (
    217019, 217003, 217032, 217056, 217005, 217051,
    217036, 217049, 217059, 217063, 217050
);

-- Use 60-hour loot lockouts for the named bosses and Fennin.
-- Fennin is spawned by the Guardian encounter rather than a static spawnpoint.
UPDATE `npc_types`
SET `loot_lockout` = 216000
WHERE `id` IN (
    217019, 217003, 217032, 217056, 217005, 217051,
    217036, 217049, 217059, 217063, 217440
);

-- Lock out completion of the Fennin encounter, not its Guardian trigger.
UPDATE `npc_types`
SET `loot_lockout` = 0
WHERE `id` = 217050;

-- Give Blaring Horn of Fire two independent 10% rolls on the four shared
-- Chaoslord bosses without increasing their other rare drops.
DELETE FROM `lootdrop_entries`
WHERE `lootdrop_id` = 23544
  AND `item_id` = 27996;

INSERT INTO `lootdrop_entries`
    (`lootdrop_id`, `item_id`, `item_charges`, `equip_item`, `chance`, `minlevel`, `maxlevel`, `multiplier`, `disabled_chance`, `expansion`, `min_expansion`, `max_expansion`, `min_looter_level`, `item_loot_lockout_timer`, `content_flags_disabled`, `content_flags`)
VALUES
    (217425, 27996, 1, 0, 100, 0, 255, 1, 0, 0, -1, -1, 0, 0, NULL, NULL)
ON DUPLICATE KEY UPDATE
    `chance` = VALUES(`chance`),
    `multiplier` = VALUES(`multiplier`);

INSERT INTO `loottable_entries`
    (`loottable_id`, `lootdrop_id`, `multiplier`, `probability`, `droplimit`, `mindrop`, `multiplier_min`)
VALUES
    (87723, 217425, 2, 10, 0, 0, 0)
ON DUPLICATE KEY UPDATE
    `multiplier` = VALUES(`multiplier`),
    `probability` = VALUES(`probability`),
    `droplimit` = VALUES(`droplimit`),
    `mindrop` = VALUES(`mindrop`),
    `multiplier_min` = VALUES(`multiplier_min`);
