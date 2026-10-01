-- One-time cleanup of active timers with more than eight days remaining.
-- Does not change NPC, item, or spawnpoint timer definitions.
-- Run while zone processes are stopped; they cache active timers in memory.
START TRANSACTION;

SET @eight_day_cutoff = UNIX_TIMESTAMP() + 691200;

DELETE FROM character_loot_lockouts
WHERE expiry > @eight_day_cutoff;

-- expire_time = 0 is a permanent legacy-item flag and is left alone.
DELETE FROM character_legacy_items
WHERE expire_time > @eight_day_cutoff;

-- Guild 1 raid-target rows can use a long sentinel to wait for the next quake.
DELETE rt FROM respawn_times AS rt
LEFT JOIN spawn2 AS s ON s.id = rt.id
WHERE CAST(rt.start AS UNSIGNED) + CAST(rt.duration AS UNSIGNED) > @eight_day_cutoff
  AND NOT (rt.guild_id = 1 AND COALESCE(s.raid_target_spawnpoint, 0) = 1);

COMMIT;
