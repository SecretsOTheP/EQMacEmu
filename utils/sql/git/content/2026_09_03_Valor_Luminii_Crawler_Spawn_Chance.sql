-- Modestly improve access to crystalline globe piece 25798.
-- Keep the crawler's guaranteed item drop and existing respawn timers unchanged.
-- Spawn group 208004 is shared by eleven Plane of Valor spawnpoints.
UPDATE `spawnentry`
SET `chance` = CASE `npcID`
    WHEN 208006 THEN 10 -- A Luminii Crawler: previously 5
    WHEN 208004 THEN 90 -- Common spawn: previously 95
END
WHERE `spawngroupID` = 208004
  AND `npcID` IN (208006, 208004);
