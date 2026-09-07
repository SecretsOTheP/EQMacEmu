-- Match quake-end announcements to the hard eight-hour raid window.
-- Leave quake frequency, minimum variance and maximum variance unchanged.
UPDATE `rule_values`
SET `rule_value` = '28800'
WHERE `rule_name` = 'Quarm:QuakeEndTimeDuration';

-- Disable default Guild 1 double loot. The #pvpzone command may explicitly
-- enable normal/raid double loot again for an administrator-run event. Also
-- begin with automatic quakes and timed raid respawns off, even if an earlier
-- test left either control enabled. Do not change PvP zone access, the
-- open-world PvE double-loot rule, or PvP XP settings.
DELETE FROM `data_buckets`
WHERE `key` IN (
    'pvpzone_normal_loot_shortnames', 'pvpzone_raid_loot_shortnames',
    'pvpzone_quake_next', 'pvpzone_raid_spawn_tier',
    'pvpzone_timed_raid_shortnames'
);
