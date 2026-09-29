-- Restore the three guild-instance books outside Plane of Tranquility.
-- The September book migration added their marker NPCs but omitted these doors.
-- The positions and destinations match the archived September test database.
INSERT INTO `doors` (
    `doorid`, `zone`, `name`, `pos_x`, `pos_y`, `pos_z`, `heading`,
    `opentype`, `door_param`, `dest_zone`, `dest_x`, `dest_y`,
    `dest_z`, `dest_heading`, `guild_zone_door`
) VALUES
    (13, 'poeartha', 'POKTELE500', 1509, -2753, 0.6, 120, 58, 1,
     'poearthb', -764, 333, -59, 132, 1),
    (116, 'poeartha', 'POKTELE500', -1134, 145, 68, 488, 58, 0,
     'poearthb', -764, 333, -59, 132, 1),
    (61, 'ponightmare', 'POKTELE500', -1851, 197, 117.11, 124, 58, 1,
     'nightmareb', 1594, 23, -327.06, 378, 1)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `pos_x` = VALUES(`pos_x`),
    `pos_y` = VALUES(`pos_y`),
    `pos_z` = VALUES(`pos_z`),
    `heading` = VALUES(`heading`),
    `opentype` = VALUES(`opentype`),
    `door_param` = VALUES(`door_param`),
    `dest_zone` = VALUES(`dest_zone`),
    `dest_x` = VALUES(`dest_x`),
    `dest_y` = VALUES(`dest_y`),
    `dest_z` = VALUES(`dest_z`),
    `dest_heading` = VALUES(`dest_heading`),
    `guild_zone_door` = VALUES(`guild_zone_door`);
