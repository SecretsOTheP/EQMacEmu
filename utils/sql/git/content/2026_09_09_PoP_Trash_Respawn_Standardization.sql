-- Standardize selected Plane of Power trash respawns to 19.5 minutes.

UPDATE spawn2
SET
    respawntime = 1170,
    variance = 0
WHERE
       (zone = 'bothunder'    AND respawntime = 1715)
    OR (zone = 'codecay'      AND respawntime IN (1755, 2280, 2338))
    OR (zone = 'poair'        AND respawntime = 2640
                               AND spawngroupID <> 225681)
    OR (zone = 'pofire'       AND respawntime = 2460)
    OR (zone = 'ponightmare'  AND respawntime IN (1500, 3200, 4680))
    OR (zone = 'postorms'     AND respawntime IN (6300, 6650))
    OR (zone = 'potorment'    AND respawntime IN (1800, 2631, 3600))
    OR (zone = 'powater'      AND respawntime = 2340)
    OR (zone = 'solrotower'   AND respawntime = 3508);
