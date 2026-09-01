-- Keep the Tower of Solusek Ro progression encounters in guild instances.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
  367544, -- Arlyxir
  367545, -- Solusek Ro
  367546, -- Rizlona encounter trigger
  367547, -- Jiva
  367548, -- Xuzl
  367793, -- Guardian of Dresolik
  367794, -- Guardian of Dresolik
  367795, -- Guardian of Dresolik
  367796  -- Guardian of Dresolik
);

-- Respawn the five tower progression encounters after 1 day and 18 hours in Guild 2+
-- instances. Guild 1 raid targets are repopped by the quake system.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 151200000
WHERE `id` IN (
  212014, -- Jiva
  212023, -- Arlyxir
  212026, -- Rizlona encounter trigger
  212046, -- Guardian of Dresolik encounter trigger
  212055  -- Xuzl
);

-- Respawn Solusek Ro after 2 days and 18 hours in Guild 2+ instances.
UPDATE `npc_types`
SET `instance_spawn_timer_override` = 237600000
WHERE `id` = 212025;

-- Apply 1-day, 18-hour loot lockouts to every loot-bearing stage of the five tower
-- encounters, including the scripted Rizlona and Dresolik follow-up bosses.
UPDATE `npc_types`
SET `loot_lockout` = 151200
WHERE `id` IN (
  212014, -- Jiva
  212023, -- Arlyxir
  212026, -- Rizlona
  212046, -- Guardian of Dresolik
  212055, -- Xuzl
  212407, -- #Rizlona
  212408  -- The Protector of Dresolik
);

-- Apply a 2-day, 18-hour loot lockout to Solusek Ro.
UPDATE `npc_types`
SET `loot_lockout` = 237600
WHERE `id` = 212025;
