-- MANUAL CONTENT PATCH: apply AFTER 2026_11_15_Elemental_Bard_Instrument_Sources.sql.
-- Lowers only the newly added Earth/Water sources from 20% to exactly 2.5%.
-- Rockskull/Grioihin keep their original drops and their separate 20% bonus rolls.
-- Parent probability is an integer. Use a guaranteed parent and one independent
-- item roll at 2.5%, with no forced minimum/limited selection to normalize it.
-- No server rebuild. Refresh NPC loot through the normal server process afterward.
-- Run in one connection with no concurrent loot edits. MyISAM cannot roll back.
-- Rerunnable. Every persistent write is guarded; a failed preflight writes nothing.

DROP TEMPORARY TABLE IF EXISTS instrument_reduction_targets;
CREATE TEMPORARY TABLE instrument_reduction_targets (
 loottable_id INT UNSIGNED PRIMARY KEY,
 item_id INT NOT NULL
);
INSERT INTO instrument_reduction_targets VALUES
 (3201,27998),
 (3228,27994),
 (3229,27998),
 (4246,27998),
 (4247,27998),
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

SET @old_drum = (SELECT MIN(id) FROM lootdrop WHERE name='quarm_20261115_additional_earth_drum');
SET @old_lute = (SELECT MIN(id) FROM lootdrop WHERE name='quarm_20261115_additional_water_lute');
SET @new_drum_name = 'quarm_20261118_additional_earth_drum_2p5';
SET @new_lute_name = 'quarm_20261118_additional_water_lute_2p5';
SET @new_drum = (SELECT MIN(id) FROM lootdrop WHERE name=@new_drum_name);
SET @new_lute = (SELECT MIN(id) FROM lootdrop WHERE name=@new_lute_name);

SET @instrument_reduction_ready =
 (SELECT COUNT(*)=1 FROM lootdrop WHERE name='quarm_20261115_additional_earth_drum')
 AND (SELECT COUNT(*)=1 FROM lootdrop WHERE name='quarm_20261115_additional_water_lute')
 AND (SELECT COUNT(*)<=1 FROM lootdrop WHERE name=@new_drum_name)
 AND (SELECT COUNT(*)<=1 FROM lootdrop WHERE name=@new_lute_name);

-- Only accept the known original or already-reduced drop entry shapes.
SET @instrument_reduction_ready = @instrument_reduction_ready AND
 (SELECT COUNT(*)=2 FROM lootdrop_entries WHERE lootdrop_id IN (@old_drum,@old_lute))
 AND NOT EXISTS (
  SELECT 1 FROM lootdrop_entries d
  WHERE d.lootdrop_id IN (@old_drum,@old_lute,@new_drum,@new_lute)
   AND (d.item_id<>IF(d.lootdrop_id IN (@old_drum,@new_drum),27998,27994)
    OR d.chance<>IF(d.lootdrop_id IN (@old_drum,@old_lute),100,2.5)
    OR d.item_charges<>1 OR d.equip_item<>0 OR d.multiplier<>1
    OR d.disabled_chance<>0 OR d.minlevel<>0 OR d.maxlevel<>255
    OR d.min_expansion<>-1 OR d.max_expansion<>-1 OR d.min_looter_level<>0
    OR d.item_loot_lockout_timer<>0 OR COALESCE(d.content_flags,'')<>''
    OR COALESCE(d.content_flags_disabled,'')<>''))
 AND NOT EXISTS (
  SELECT 1 FROM lootdrop WHERE id IN (@old_drum,@old_lute,@new_drum,@new_lute)
   AND (min_expansion<>-1 OR max_expansion<>-1
    OR COALESCE(content_flags,'')<>'' OR COALESCE(content_flags_disabled,'')<>''));

-- Exactly one owned parent per target; duplicates or mixed old/new rows stop us.
SET @instrument_reduction_ready = @instrument_reduction_ready AND
 (SELECT COUNT(*)=29 FROM instrument_reduction_targets x
  JOIN loottable_entries t ON t.loottable_id=x.loottable_id
   AND t.lootdrop_id IN (@old_drum,@old_lute,@new_drum,@new_lute));
SET @instrument_reduction_ready = @instrument_reduction_ready AND
 (SELECT COUNT(DISTINCT x.loottable_id)=29 FROM instrument_reduction_targets x
  JOIN loottable_entries t ON t.loottable_id=x.loottable_id
  WHERE t.multiplier=1 AND t.multiplier_min=0 AND (
   (t.lootdrop_id=IF(x.item_id=27998,@old_drum,@old_lute)
    AND t.probability=20 AND t.droplimit=1 AND t.mindrop=1)
   OR (t.lootdrop_id=IF(x.item_id=27998,@new_drum,@new_lute)
    AND t.probability=100 AND t.droplimit=0 AND t.mindrop=0)));
SET @instrument_reduction_ready = @instrument_reduction_ready AND NOT EXISTS (
 SELECT 1 FROM loottable_entries t
 LEFT JOIN instrument_reduction_targets x ON x.loottable_id=t.loottable_id
 WHERE t.lootdrop_id IN (@new_drum,@new_lute)
  AND (x.loottable_id IS NULL OR t.lootdrop_id<>IF(x.item_id=27998,@new_drum,@new_lute)));

SELECT IF(@instrument_reduction_ready,
 'PASS: 29 added-source parent rows verified; original bosses excluded',
 'STOP: no changes permitted; verify the prerequisite patch and existing loot rows') AS preflight;

INSERT INTO lootdrop (name)
SELECT @new_drum_name WHERE @instrument_reduction_ready AND @new_drum IS NULL;
INSERT INTO lootdrop (name)
SELECT @new_lute_name WHERE @instrument_reduction_ready AND @new_lute IS NULL;
SET @new_drum = (SELECT MIN(id) FROM lootdrop WHERE name=@new_drum_name);
SET @new_lute = (SELECT MIN(id) FROM lootdrop WHERE name=@new_lute_name);
INSERT INTO lootdrop_entries
 (lootdrop_id,item_id,item_charges,equip_item,chance,multiplier,disabled_chance)
SELECT @new_drum,27998,1,0,2.5,1,0
WHERE @instrument_reduction_ready
 AND NOT EXISTS (SELECT 1 FROM lootdrop_entries WHERE lootdrop_id=@new_drum);
INSERT INTO lootdrop_entries
 (lootdrop_id,item_id,item_charges,equip_item,chance,multiplier,disabled_chance)
SELECT @new_lute,27994,1,0,2.5,1,0
WHERE @instrument_reduction_ready
 AND NOT EXISTS (SELECT 1 FROM lootdrop_entries WHERE lootdrop_id=@new_lute);

UPDATE loottable_entries t
JOIN instrument_reduction_targets x ON x.loottable_id=t.loottable_id
SET t.lootdrop_id=IF(x.item_id=27998,@new_drum,@new_lute),
    t.probability=100, t.droplimit=0, t.mindrop=0
WHERE @instrument_reduction_ready
 AND t.lootdrop_id IN (IF(x.item_id=27998,@old_drum,@old_lute),
                      IF(x.item_id=27998,@new_drum,@new_lute));

-- Expected: 29 rows, each with parent 100%, item 2.5%, one independent roll.
SELECT x.loottable_id,x.item_id,t.probability AS parent_chance,d.chance AS item_chance,
       t.multiplier,t.multiplier_min,t.droplimit,t.mindrop
FROM instrument_reduction_targets x
JOIN loottable_entries t ON t.loottable_id=x.loottable_id
 AND t.lootdrop_id=IF(x.item_id=27998,@new_drum,@new_lute)
JOIN lootdrop_entries d ON d.lootdrop_id=t.lootdrop_id AND d.item_id=x.item_id
ORDER BY x.item_id,x.loottable_id;
DROP TEMPORARY TABLE instrument_reduction_targets;
