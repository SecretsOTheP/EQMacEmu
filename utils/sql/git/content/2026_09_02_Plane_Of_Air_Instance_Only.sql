-- Keep the confirmed Plane of Air raid targets out of the open world.
-- Preserve enabled spawnpoints, existing timers, and loot lockouts.
-- Requires the companion poair/script_init.lua change to block scripted rings.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `zone` = 'poair'
  AND `id` IN (
    365638, -- Baltaldor the Cursed
    367322, -- Gakamenial Fir`Disralsi
    366074, -- Queen Silandria
    366212, -- Rinturion Windblade
    365346, -- Xegony the Queen of Air
    365750, -- Arch Mage Alchtonion
    366131  -- Sigismond Windwalker
  );
