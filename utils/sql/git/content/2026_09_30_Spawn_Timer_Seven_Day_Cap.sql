-- Run after the historical pre-PoP respawn repair. No content spawnpoint
-- should retain a base or boot timer above seven days. Plane of Justice's
-- ancient crawler spawn uses the 66-hour value already tested on oct_test.
UPDATE spawn2
SET respawntime = CASE
        WHEN id = 345473 AND respawntime > 604800 THEN 237600
        ELSE LEAST(respawntime, 604800)
    END,
    boot_respawntime = LEAST(boot_respawntime, 604800)
WHERE respawntime > 604800 OR boot_respawntime > 604800;

-- Existing non-quake countdowns can outlive an edited spawn2 row. Leave
-- Guild 1 raid-target sentinels alone: the quake system owns those timers.
UPDATE respawn_times AS rt
LEFT JOIN spawn2 AS s ON s.id = rt.id
SET rt.duration = 604800
WHERE rt.duration > 604800
  AND NOT (rt.guild_id = 1 AND COALESCE(s.raid_target_spawnpoint, 0) = 1);
