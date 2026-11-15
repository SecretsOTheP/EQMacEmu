-- MANUAL CONTENT PATCH. Not automatically applied by merging or rebuilding.
-- Apply to the intended Quarm content database in one connection, with no
-- concurrent loot edits. Then refresh NPC loot using the normal server process.
-- No server rebuild required. Existing spawned NPC loot is not regenerated here.
-- Prepared against the local Quarm snapshot; the preflight guards check live.
-- MyISAM tables do not support transactional rollback: back up before applying.
--
-- Adds one independent instrument roll per target loot table:
-- 12 Earth trap named + 16 Water rare named: 20%.
-- Tantisala + Hydrotha/Krziik/Ofossaa: 20%.
-- Rockskull and Grioihin also gain a separate 20% bonus instrument roll.
-- Their original rolls remain: at least one drum 19% -> 35.2%; lute 28% -> 42.4%.
-- Other loot, cash, timers and lockouts unchanged; multiple instruments are possible.
-- Shared tables receive only ONE roll. Global/PvP loot multipliers remain unchanged.
-- Rerunnable: inserts missing patch rows only; never updates existing loot.

DROP TEMPORARY TABLE IF EXISTS instrument_targets_20261115;
CREATE TEMPORARY TABLE instrument_targets_20261115 (
 npc_id INT UNSIGNED PRIMARY KEY,
 loottable_id INT UNSIGNED NOT NULL,
 item_id INT NOT NULL,
 probability INT NOT NULL
);
INSERT INTO instrument_targets_20261115 VALUES
 (218038, 4246, 27998, 20) /* Tantisala Jaggedtooth */,
 (218351, 3201, 27998, 20) /* A Prismatic Basilisk */,
 (218377, 87767, 27998, 20) /* The Living Earth */,
 (218381, 97048, 27998, 20) /* Calris Bristlebranch */,
 (218384, 5753, 27998, 20) /* A Korascian Warlord */,
 (218378, 3229, 27998, 20) /* A Pristine Gem Golem */,
 (218386, 14474, 27998, 20) /* A Shimmering Gem Sentry */,
 (218385, 6637, 27998, 20) /* A Perfect Rock Formation */,
 (218379, 14479, 27998, 20) /* Glimmerstone */,
 (218380, 87768, 27998, 20) /* Galsinak Earthrumble */,
 (218368, 14481, 27998, 20) /* Tribal Leader Diseranon */,
 (218382, 4247, 27998, 20) /* An Ancient Vekerchiki Champion */,
 (218376, 87736, 27998, 20) /* A Monsterous Earthwalker */,
 (216037, 6730, 27994, 20) /* Gigadon */,
 (216054, 4507, 27994, 20) /* Savage Deepwater Kraken */,
 (216237, 4507, 27994, 20) /* Furious Deepwater Kraken */,
 (216238, 6667, 27994, 20) /* Ferocious Barracuda */,
 (216239, 5599, 27994, 20) /* Frenzied Anglerfish */,
 (216240, 6702, 27994, 20) /* Razorfin */,
 (216241, 6731, 27994, 20) /* Hammertooth */,
 (216242, 87611, 27994, 20) /* Hraquis Arch Magus */,
 (216243, 5795, 27994, 20) /* Hraquis Chieftain */,
 (216244, 6666, 27994, 20) /* Monstrous Sea Turtle */,
 (216246, 6910, 27994, 20) /* Regrua Overlord */,
 (216106, 87624, 27994, 20) /* Regrua Protector */,
 (216248, 6732, 27994, 20) /* Swordfang */,
 (216234, 6669, 27994, 20) /* Triloun Egg Keeper */,
 (216105, 6669, 27994, 20) /* Triloun Seer */,
 (216235, 6669, 27994, 20) /* Triloun Warder */,
 (216043, 3228, 27994, 20) /* Hydrotha */,
 (216041, 6222, 27994, 20) /* Krziik the Mighty */,
 (216040, 6226, 27994, 20) /* Ofossaa the Enlightened */,
 (218374, 4478, 27998, 20) /* Peregrin Rockskull bonus */,
 (216042, 4451, 27994, 20) /* Grioihin the Wise bonus */;

SET @drum_name = 'quarm_20261115_additional_earth_drum';
SET @lute_name = 'quarm_20261115_additional_water_lute';
SET @drum_ld = (SELECT MIN(id) FROM lootdrop WHERE name = @drum_name);
SET @lute_ld = (SELECT MIN(id) FROM lootdrop WHERE name = @lute_name);

-- All guards must pass before any persistent INSERT is allowed.
-- Literal CTE avoids MariaDB temporary-table reopening restrictions.
WITH instrument_guard_targets (npc_id,loottable_id,item_id,probability) AS (
 SELECT 218038,4246,27998,20
 UNION ALL
 SELECT 218351,3201,27998,20
 UNION ALL
 SELECT 218377,87767,27998,20
 UNION ALL
 SELECT 218381,97048,27998,20
 UNION ALL
 SELECT 218384,5753,27998,20
 UNION ALL
 SELECT 218378,3229,27998,20
 UNION ALL
 SELECT 218386,14474,27998,20
 UNION ALL
 SELECT 218385,6637,27998,20
 UNION ALL
 SELECT 218379,14479,27998,20
 UNION ALL
 SELECT 218380,87768,27998,20
 UNION ALL
 SELECT 218368,14481,27998,20
 UNION ALL
 SELECT 218382,4247,27998,20
 UNION ALL
 SELECT 218376,87736,27998,20
 UNION ALL
 SELECT 216037,6730,27994,20
 UNION ALL
 SELECT 216054,4507,27994,20
 UNION ALL
 SELECT 216237,4507,27994,20
 UNION ALL
 SELECT 216238,6667,27994,20
 UNION ALL
 SELECT 216239,5599,27994,20
 UNION ALL
 SELECT 216240,6702,27994,20
 UNION ALL
 SELECT 216241,6731,27994,20
 UNION ALL
 SELECT 216242,87611,27994,20
 UNION ALL
 SELECT 216243,5795,27994,20
 UNION ALL
 SELECT 216244,6666,27994,20
 UNION ALL
 SELECT 216246,6910,27994,20
 UNION ALL
 SELECT 216106,87624,27994,20
 UNION ALL
 SELECT 216248,6732,27994,20
 UNION ALL
 SELECT 216234,6669,27994,20
 UNION ALL
 SELECT 216105,6669,27994,20
 UNION ALL
 SELECT 216235,6669,27994,20
 UNION ALL
 SELECT 216043,3228,27994,20
 UNION ALL
 SELECT 216041,6222,27994,20
 UNION ALL
 SELECT 216040,6226,27994,20
 UNION ALL
 SELECT 218374,4478,27998,20
 UNION ALL
 SELECT 216042,4451,27994,20
)
SELECT
 (SELECT COUNT(*) = 34 FROM instrument_guard_targets x
  JOIN npc_types n ON n.id=x.npc_id AND n.loottable_id=x.loottable_id
  JOIN loottable t ON t.id=x.loottable_id)
 AND (SELECT COUNT(*) = 2 FROM items WHERE id IN (27994,27998))
 AND NOT EXISTS (
  SELECT 1 FROM npc_types n
  JOIN instrument_guard_targets x ON x.loottable_id=n.loottable_id
  LEFT JOIN instrument_guard_targets allowed ON allowed.npc_id=n.id
  WHERE allowed.npc_id IS NULL)
 AND (SELECT COUNT(*) <= 1 FROM lootdrop WHERE name=@drum_name)
 AND (SELECT COUNT(*) <= 1 FROM lootdrop WHERE name=@lute_name)
 AND NOT EXISTS (
  SELECT 1 FROM lootdrop WHERE id IN (@drum_ld,@lute_ld)
   AND (min_expansion<>-1 OR max_expansion<>-1
    OR COALESCE(content_flags,'')<>'' OR COALESCE(content_flags_disabled,'')<>''))
 AND NOT EXISTS (
  SELECT 1 FROM lootdrop_entries d WHERE d.lootdrop_id IN (@drum_ld,@lute_ld)
   AND (d.item_id<>IF(d.lootdrop_id=@drum_ld,27998,27994)
    OR d.item_charges<>1 OR d.equip_item<>0 OR d.chance<>100
    OR d.multiplier<>1 OR d.disabled_chance<>0 OR d.minlevel<>0 OR d.maxlevel<>255
    OR d.min_expansion<>-1 OR d.max_expansion<>-1 OR d.min_looter_level<>0
    OR d.item_loot_lockout_timer<>0 OR COALESCE(d.content_flags,'')<>''
    OR COALESCE(d.content_flags_disabled,'')<>''))
 AND NOT EXISTS (
  SELECT 1 FROM loottable_entries t
  WHERE t.lootdrop_id IN (@drum_ld,@lute_ld)
   AND (t.multiplier<>1 OR t.multiplier_min<>0 OR t.droplimit<>1 OR t.mindrop<>1
    OR NOT EXISTS (SELECT 1 FROM instrument_guard_targets x
     WHERE x.loottable_id=t.loottable_id AND x.probability=t.probability
      AND t.lootdrop_id=IF(x.item_id=27998,@drum_ld,@lute_ld))))
 AND NOT EXISTS (
  SELECT 1 FROM instrument_guard_targets x
  JOIN loottable_entries t ON t.loottable_id=x.loottable_id
  JOIN lootdrop_entries d ON d.lootdrop_id=t.lootdrop_id AND d.item_id=x.item_id
  WHERE NOT (t.lootdrop_id <=> IF(x.item_id=27998,@drum_ld,@lute_ld))
   AND NOT (x.npc_id=218374 AND t.lootdrop_id=218374)
   AND NOT (x.npc_id=216042 AND t.lootdrop_id=8503))
 -- Confirm the original instrument rolls used for the stated combined chances.
 AND EXISTS (SELECT 1 FROM loottable_entries t JOIN lootdrop_entries d
  ON d.lootdrop_id=t.lootdrop_id
  WHERE t.loottable_id=4478 AND t.lootdrop_id=218374 AND d.item_id=27998
   AND t.probability=10 AND t.multiplier=2 AND t.multiplier_min=0
   AND t.droplimit=0 AND t.mindrop=0 AND d.chance=100 AND d.multiplier=1
   AND d.disabled_chance=0)
 AND EXISTS (SELECT 1 FROM loottable_entries t
  WHERE t.loottable_id=4451 AND t.lootdrop_id=8503 AND t.probability=50
   AND t.multiplier=2 AND t.multiplier_min=1 AND t.droplimit=1 AND t.mindrop=1)
 AND (SELECT COUNT(*)=3 FROM lootdrop_entries WHERE lootdrop_id=8503)
 AND (SELECT COUNT(*)=3 FROM lootdrop_entries WHERE lootdrop_id=8503
  AND multiplier=1 AND disabled_chance=0
  AND ((item_id=27994 AND chance=20) OR (item_id IN (10945,16605) AND chance=40)))
 INTO @instrument_patch_ready;

SELECT IF(@instrument_patch_ready,
 'PASS: mappings and existing patch rows verified',
 'STOP: no patch inserts permitted; inspect live mappings/loot before proceeding') AS preflight;

INSERT INTO lootdrop (name)
SELECT @drum_name WHERE @instrument_patch_ready AND @drum_ld IS NULL;
INSERT INTO lootdrop (name)
SELECT @lute_name WHERE @instrument_patch_ready AND @lute_ld IS NULL;
SET @drum_ld = (SELECT MIN(id) FROM lootdrop WHERE name=@drum_name);
SET @lute_ld = (SELECT MIN(id) FROM lootdrop WHERE name=@lute_name);

INSERT INTO lootdrop_entries
 (lootdrop_id,item_id,item_charges,equip_item,chance,multiplier,disabled_chance)
SELECT @drum_ld,27998,1,0,100,1,0
WHERE @instrument_patch_ready AND @drum_ld IS NOT NULL
 AND NOT EXISTS (SELECT 1 FROM lootdrop_entries WHERE lootdrop_id=@drum_ld);
INSERT INTO lootdrop_entries
 (lootdrop_id,item_id,item_charges,equip_item,chance,multiplier,disabled_chance)
SELECT @lute_ld,27994,1,0,100,1,0
WHERE @instrument_patch_ready AND @lute_ld IS NOT NULL
 AND NOT EXISTS (SELECT 1 FROM lootdrop_entries WHERE lootdrop_id=@lute_ld);

INSERT INTO loottable_entries
 (loottable_id,lootdrop_id,multiplier,probability,droplimit,mindrop,multiplier_min)
SELECT DISTINCT x.loottable_id,d.lootdrop_id,1,x.probability,1,1,0
FROM instrument_targets_20261115 x
JOIN lootdrop_entries d ON d.item_id=x.item_id
 AND d.lootdrop_id=IF(x.item_id=27998,@drum_ld,@lute_ld)
WHERE @instrument_patch_ready
 AND NOT EXISTS (SELECT 1 FROM loottable_entries t
  WHERE t.loottable_id=x.loottable_id AND t.lootdrop_id=d.lootdrop_id);

-- Expected: 34 NPC rows, backed by 31 distinct new parent entries.
SELECT n.id,n.name,t.loottable_id,d.item_id,t.probability,
       t.multiplier,t.multiplier_min,t.droplimit,t.mindrop,d.chance
FROM instrument_targets_20261115 x
JOIN npc_types n ON n.id=x.npc_id
JOIN loottable_entries t ON t.loottable_id=n.loottable_id
 AND t.lootdrop_id=IF(x.item_id=27998,@drum_ld,@lute_ld)
JOIN lootdrop_entries d ON d.lootdrop_id=t.lootdrop_id AND d.item_id=x.item_id
ORDER BY d.item_id,n.id;

-- Original sources: verify retained original rows plus their new bonus rows.
SELECT n.id,n.name,t.*,d.item_id,d.chance AS item_chance
FROM npc_types n JOIN loottable_entries t ON t.loottable_id=n.loottable_id
JOIN lootdrop_entries d ON d.lootdrop_id=t.lootdrop_id
WHERE (n.id=218374 AND d.item_id=27998) OR (n.id=216042 AND d.item_id=27994);
DROP TEMPORARY TABLE instrument_targets_20261115;
