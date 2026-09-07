-- Make secondary medallion bosses available every 90 minutes.
UPDATE `spawn2` AS `s2`
JOIN `spawnentry` AS `se`
  ON `se`.`spawngroupID` = `s2`.`spawngroupID`
SET
  `s2`.`respawntime` = 5400,
  `s2`.`variance` = 0
WHERE `se`.`npcID` IN (
  210026, -- Laruken the Rigid
  210027, -- Zertuken the Unyielding
  210028, -- Paruek the Strong
  210029, -- Faruek the Bold
  210032, -- Pendubk the Turbulent
  210033  -- Solnebk the Unruly
);
