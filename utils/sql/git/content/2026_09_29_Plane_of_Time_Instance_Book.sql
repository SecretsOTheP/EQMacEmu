-- Match the Plane of Time instance book to the other PoP books.
-- These rows and the lower book position were present in the September test realm
-- but were omitted from the book migrations committed to Git.
INSERT INTO `npc_types` (`id`, `name`, `level`, `race`, `class`, `bodytype`, `hp`, `size`, `attack_count`)
VALUES (7151207, 'Plane of Time', 1, 127, 1, 60, 1, 5, 0)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `level` = VALUES(`level`),
    `race` = VALUES(`race`),
    `class` = VALUES(`class`),
    `bodytype` = VALUES(`bodytype`),
    `hp` = VALUES(`hp`),
    `size` = VALUES(`size`),
    `attack_count` = VALUES(`attack_count`);

INSERT INTO `spawngroup` (`id`, `name`, `spawn_limit`)
VALUES (7151207, 'sg_potime_book_potranquility', 1)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `spawn_limit` = VALUES(`spawn_limit`);

INSERT INTO `spawnentry` (`spawngroupID`, `npcID`, `chance`)
VALUES (7151207, 7151207, 100)
ON DUPLICATE KEY UPDATE `chance` = VALUES(`chance`);

UPDATE `spawn2`
SET `x` = 1126.11, `y` = -2194.97, `z` = -917.5, `heading` = 176
WHERE `zone` = 'potranquility' AND `spawngroupID` = 7151207;

INSERT INTO `spawn2` (`spawngroupID`, `zone`, `x`, `y`, `z`, `heading`)
SELECT 7151207, 'potranquility', 1126.11, -2194.97, -917.5, 176
WHERE NOT EXISTS (
    SELECT 1 FROM `spawn2`
    WHERE `zone` = 'potranquility' AND `spawngroupID` = 7151207
);

UPDATE `doors`
SET `pos_z` = -917.5
WHERE `zone` = 'potranquility'
  AND `doorid` = 109
  AND `dest_zone` = 'potimea'
  AND `guild_zone_door` = 1;
