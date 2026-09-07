-- Keep Aerin`Dar out of open-world Plane of Valor while preserving the
-- encounter in raid instances.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` = 347257;
