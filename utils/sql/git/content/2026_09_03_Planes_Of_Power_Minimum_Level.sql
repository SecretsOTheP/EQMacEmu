-- Require level 46 for Planes of Power gameplay zones.
-- Plane of Knowledge remains available as the public hub, while existing
-- requirements above level 46 (Plane of Time) are preserved.
UPDATE `zone`
SET `min_level` = 46
WHERE `zoneidnumber` BETWEEN 200 AND 223
  AND `zoneidnumber` <> 202
  AND `min_level` < 46;
