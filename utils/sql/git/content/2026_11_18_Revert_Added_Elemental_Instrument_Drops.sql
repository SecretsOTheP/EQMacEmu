-- MANUAL CONTENT REVERT: removes the instrument additions from #495.
-- Replaces 2026_11_18_Reduce_Added_Elemental_Instrument_Drops.sql (2.5% version).
-- Works after the original 20% patch, the earlier 2.5% patch, or a prior revert.
-- Removes all added sources AND Rockskull/Grioihin's added bonus rolls.
-- Original instrument drops, other loot and the Bo Staff magic fix are untouched.
-- Only detaches the specifically named patch lootdrops from their target tables;
-- the now-unused lootdrop definitions are retained to avoid deleting shared data.
-- No rebuild required. Run the entire file in one connection with no concurrent
-- loot edits, then refresh NPC loot normally. Existing corpses are not changed.

DROP TEMPORARY TABLE IF EXISTS instrument_revert_targets;
CREATE TEMPORARY TABLE instrument_revert_targets (
 loottable_id INT UNSIGNED PRIMARY KEY,
 item_id INT NOT NULL
);
INSERT INTO instrument_revert_targets VALUES
 (3201,27998),
 (3228,27994),
 (3229,27998),
 (4246,27998),
 (4247,27998),
 (4451,27994),
 (4478,27998),
 (4507,27994),
 (5599,27994),
 (5753,27998),
 (5795,27994),
 (6222,27994),
 (6226,27994),
 (6637,27998),
 (6666,27994),
 (6667,27994),
 (6669,27994),
 (6702,27994),
 (6730,27994),
 (6731,27994),
 (6732,27994),
 (6910,27994),
 (14474,27998),
 (14479,27998),
 (14481,27998),
 (87611,27994),
 (87624,27994),
 (87736,27998),
 (87767,27998),
 (87768,27998),
 (97048,27998);

-- Check ownership by the exact names created by the two versions of this patch.
SET @instrument_revert_ready = NOT EXISTS (
 SELECT name FROM lootdrop
 WHERE name IN ('quarm_20261115_additional_earth_drum',
                'quarm_20261115_additional_water_lute',
                'quarm_20261118_additional_earth_drum_2p5',
                'quarm_20261118_additional_water_lute_2p5')
 GROUP BY name HAVING COUNT(*)>1);
SET @instrument_revert_ready = @instrument_revert_ready AND NOT EXISTS (
 SELECT 1 FROM lootdrop l JOIN lootdrop_entries d ON d.lootdrop_id=l.id
 WHERE (l.name IN ('quarm_20261115_additional_earth_drum',
                   'quarm_20261118_additional_earth_drum_2p5') AND d.item_id<>27998)
    OR (l.name IN ('quarm_20261115_additional_water_lute',
                   'quarm_20261118_additional_water_lute_2p5') AND d.item_id<>27994));

SELECT IF(@instrument_revert_ready,
 'PASS: patch-owned instrument rows can be removed; original loot is preserved',
 'STOP: duplicate names or unexpected items; no changes permitted') AS preflight;

DELETE t FROM loottable_entries t
JOIN instrument_revert_targets x ON x.loottable_id=t.loottable_id
JOIN lootdrop l ON l.id=t.lootdrop_id
WHERE @instrument_revert_ready AND (
 (x.item_id=27998 AND l.name IN ('quarm_20261115_additional_earth_drum',
                                'quarm_20261118_additional_earth_drum_2p5'))
 OR
 (x.item_id=27994 AND l.name IN ('quarm_20261115_additional_water_lute',
                                'quarm_20261118_additional_water_lute_2p5')));
SELECT ROW_COUNT() AS added_instrument_rows_removed;

-- Expected zero after a successful revert. A repeated run removes zero rows.
SELECT COUNT(*) AS remaining_added_instrument_rows
FROM loottable_entries t
JOIN instrument_revert_targets x ON x.loottable_id=t.loottable_id
JOIN lootdrop l ON l.id=t.lootdrop_id
WHERE (x.item_id=27998 AND l.name IN ('quarm_20261115_additional_earth_drum',
                                    'quarm_20261118_additional_earth_drum_2p5'))
   OR (x.item_id=27994 AND l.name IN ('quarm_20261115_additional_water_lute',
                                    'quarm_20261118_additional_water_lute_2p5'));
DROP TEMPORARY TABLE instrument_revert_targets;
