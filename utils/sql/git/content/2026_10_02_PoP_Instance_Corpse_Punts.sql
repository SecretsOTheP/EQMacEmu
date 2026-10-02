-- Timed PoP guild-instance corpses move directly to Plane of Tranquility.
-- Keep each source zone's graveyard assignment for open-world deaths and
-- scripted Justice/Hedge washouts. Apply before rebuilding the zone service.
INSERT INTO `graveyard` (`zone_id`, `x`, `y`, `z`, `heading`)
VALUES (203, -1794, 1264, -898.03, 0)
ON DUPLICATE KEY UPDATE
    `x` = VALUES(`x`), `y` = VALUES(`y`),
    `z` = VALUES(`z`), `heading` = VALUES(`heading`);

-- PoP instance corpse timers are one hour (Guild 1 retains its 30-minute rule).
UPDATE `zone`
SET `graveyard_time` = 60
WHERE `expansion` = 4 AND `graveyard_id` > 0;
