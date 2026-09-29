-- Remove the timed raid-spawn override left by #pvpzone luclin/pop on.
-- Keep the active PVP zone list and loot settings unchanged.
DELETE FROM data_buckets
WHERE `key` IN ('pvpzone_raid_spawn_tier', 'pvpzone_timed_raid_shortnames');
