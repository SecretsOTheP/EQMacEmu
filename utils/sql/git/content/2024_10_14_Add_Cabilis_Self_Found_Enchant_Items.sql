INSERT INTO `npc_types` (
    `name`, `lastname`, `level`, `race`, `class`, `bodytype`, 
    `hp`, `mana`, `gender`, `texture`, `helmtexture`, `size`, 
    `hp_regen_rate`, `mana_regen_rate`, `loottable_id`, `merchant_id`, 
    `npc_spells_id`, `npc_spells_effects_id`, `npc_faction_id`, 
    `mindmg`, `maxdmg`, `attack_count`, `special_abilities`, 
    `aggroradius`, `assistradius`, `face`, `luclin_hairstyle`, 
    `luclin_haircolor`, `luclin_eyecolor`, `luclin_eyecolor2`, 
    `luclin_beardcolor`, `luclin_beard`, `armortint_id`, 
    `armortint_red`, `armortint_green`, `armortint_blue`, 
    `d_melee_texture1`, `d_melee_texture2`, 
    `prim_melee_type`, `sec_melee_type`, `ranged_type`, 
    `runspeed`, `MR`, `CR`, `DR`, `FR`, `PR`, 
    `see_invis`, `see_invis_undead`, `qglobal`, `AC`, 
    `npc_aggro`, `spawn_limit`, `attack_delay`, 
    `STR`, `STA`, `DEX`, `AGI`, `_INT`, `WIS`, `CHA`, 
    `see_sneak`, `see_improved_hide`, `ATK`, `Accuracy`, 
    `slow_mitigation`, `maxlevel`, `scalerate`, 
    `private_corpse`, `unique_spawn_by_name`, `underwater`, 
    `isquest`, `emoteid`, `spellscale`, `healscale`, 
    `raid_target`, `chesttexture`, `armtexture`, 
    `bracertexture`, `handtexture`, `legtexture`, 
    `feettexture`, `light`, `walkspeed`, 
    `combat_hp_regen`, `combat_mana_regen`, `aggro_pc`, 
    `ignore_distance`, `encounter`, `ignore_despawn`, 
    `avoidance`, `exp_pct`, `greed`, `engage_notice`, 
    `stuck_behavior`, `flymode`, `loot_lockout`, 
    `instance_spawn_timer_override`, `skip_global_loot`, 
    `rare_spawn`
) VALUES (
    'Belner_Snuckery',  -- name
    '',                  -- lastname
    1,                   -- level
    11,                  -- race - Halfling
    1,                   -- class 
    1,                   -- bodytype
    10,                  -- hp
    0,                   -- mana
    0,                   -- gender
    0,                   -- texture
    0,                   -- helmtexture
    3.5,                 -- size
    7,                   -- hp_regen_rate
    0,                   -- mana_regen_rate
    0,                   -- loottable_id
    0,                   -- merchant_id
    0,                   -- npc_spells_id
    0,                   -- npc_spells_effects_id
    795,                 -- npc_faction_id - Cabilis Residents
    1,                   -- mindmg
    12,                  -- maxdmg
    -1,                  -- attack_count
    '10,1',              -- special_abilities
    55,                  -- aggroradius
    0,                   -- assistradius
    0,                   -- face
    255,                 -- luclin_hairstyle
    255,                 -- luclin_haircolor
    255,                 -- luclin_eyecolor
    255,                 -- luclin_eyecolor2
    255,                 -- luclin_beardcolor
    255,                 -- luclin_beard
    0,                   -- armortint_id
    0,                   -- armortint_red
    0,                   -- armortint_green
    0,                   -- armortint_blue
    0,                   -- d_melee_texture1
    0,                   -- d_melee_texture2
    28,                  -- prim_melee_type
    28,                  -- sec_melee_type
    7,                   -- ranged_type
    1.25,                -- runspeed
    25,                  -- MR
    25,                  -- CR
    15,                  -- DR
    25,                  -- FR
    15,                  -- PR
    0,                   -- see_invis
    1,                   -- see_invis_undead
    0,                   -- qglobal
    27,                  -- AC
    0,                   -- npc_aggro
    0,                   -- spawn_limit
    30,                  -- attack_delay
    95,                  -- STR
    95,                  -- STA
    95,                  -- DEX
    95,                  -- AGI
    95,                  -- _INT
    95,                  -- WIS
    95,                  -- CHA
    0,                   -- see_sneak
    0,                   -- see_improved_hide
    100,                 -- ATK
    100,                 -- Accuracy
    0,                   -- slow_mitigation
    0,                   -- maxlevel
    0,                   -- scalerate
    0,                   -- private_corpse
    0,                   -- unique_spawn_by_name
    0,                   -- underwater
    0,                   -- isquest
    0,                   -- emoteid
    1000,                -- spellscale
    0,                   -- healscale
    0,                   -- raid_target
    100,                 -- chesttexture
    0,                   -- armtexture
    0,                   -- bracertexture
    0,                   -- handtexture
    0,                   -- legtexture
    0,                   -- feettexture
    0,                   -- light
    0,                   -- walkspeed
    0,                   -- combat_hp_regen
    0,                   -- combat_mana_regen
    0,                   -- aggro_pc
    0,                   -- ignore_distance
    0,                   -- encounter
    0,                   -- ignore_despawn
    0,                   -- avoidance
    0,                   -- exp_pct
    0,                   -- greed
    0,                   -- engage_notice
    0,                   -- stuck_behavior
    0,                   -- flymode
    0,                   -- loot_lockout
    0,                   -- instance_spawn_timer_override
    0,                   -- skip_global_loot
    0                    -- rare_spawn
);

INSERT INTO `spawngroup` (`name`, `spawn_limit`, `max_x`, `min_x`, `max_y`, `min_y`, `delay`, `mindelay`, `despawn`, `despawn_timer`, `rand_spawns`, `rand_respawntime`, `rand_variance`, `rand_condition_`, `wp_spawns`) 
VALUES 
('cabeast_Belner_Snuckery', 0, 0, 0, 0, 0, 0, 15000, 0, 100, 0, 1200, 0, 0, 0);

-- Get the spawn group ID and assign it to a user-defined variable
SELECT ID 
INTO @snuckerySpawnGroupID
FROM spawngroup 
WHERE name = 'cabeast_Belner_Snuckery' 
LIMIT 1;

-- Get the NPC ID and assign it to a user-defined variable
SELECT ID 
INTO @snuckeryNPCID 
FROM npc_types 
WHERE name = 'Belner_Snuckery' 
LIMIT 1;

INSERT INTO `spawnentry` (`spawngroupID`, `npcID`, `chance`, `mintime`, `maxtime`, `min_expansion`, `max_expansion`, `content_flags`, `content_flags_disabled`) 
VALUES 
(@snuckerySpawnGroupID, @snuckeryNPCID, 100, 0, 0, 1, 99, NULL, NULL);

INSERT INTO `spawn2` (
    `spawngroupID`,     -- ID of the spawn group
    `zone`,             -- Name of the zone where the spawn occurs
    `x`,                -- X coordinate for the spawn location
    `y`,                -- Y coordinate for the spawn location
    `z`,                -- Z coordinate for the spawn location (elevation)
    `heading`,          -- Direction the spawn points to (angle)
    `respawntime`,      -- Time in seconds for the spawn to respawn
    `variance`,         -- Variance in respawn time
    `pathgrid`,         -- Path grid for the spawn (0 if not used)
    `_condition`,       -- Condition under which the spawn is active
    `cond_value`,       -- Value associated with the condition
    `enabled`,          -- Status of the spawn (0 = disabled, 1 = enabled)
    `animation`,        -- Animation ID for the spawn
    `boot_respawntime`, -- Initial respawn time after boot
    `clear_timer_onboot`, -- Should the respawn timer be cleared on boot?
    `boot_variance`,    -- Variance in initial respawn time after boot
    `force_z`,          -- Force Z-coordinate (0 for default)
    `min_expansion`,    -- Minimum expansion value (for dynamic content)
    `max_expansion`,    -- Maximum expansion value (for dynamic content)
    `raid_target_spawnpoint`, -- Raid target spawn point ID
    `content_flags`,    -- Flags indicating special content properties
    `content_flags_disabled` -- Flags indicating disabled content properties
) VALUES (
    @snuckerySpawnGroupID,      -- ID of the spawn group
    'cabeast',                  -- Name of the zone where the spawn occurs
    -512,                    -- X coordinate for the spawn location
    796,                    -- Y coordinate for the spawn location
    -25,                     -- Z coordinate for the spawn location (elevation)
    247.818695,                 -- Direction the spawn points to (angle)
    60,                         -- Time in seconds for the spawn to respawn
    0,                          -- Variance in respawn time
    0,                          -- Path grid for the spawn (0 if not used)
    0,                          -- Condition under which the spawn is active
    1,                          -- Value associated with the condition
    1,                          -- Status of the spawn (0 = disabled, 1 = enabled)
    0,                          -- Animation ID for the spawn
    0,                          -- Initial respawn time after boot
    0,                          -- Should the respawn timer be cleared on boot?
    0,                          -- Variance in initial respawn time after boot
    0,                          -- Force Z-coordinate (0 for default)
    1,                          -- Minimum expansion value (for dynamic content)
    99,                         -- Maximum expansion value (for dynamic content)
    0,                          -- Raid target spawn point ID
    NULL,                       -- Flags indicating special content properties
    NULL                        -- Flags indicating disabled content properties
);


