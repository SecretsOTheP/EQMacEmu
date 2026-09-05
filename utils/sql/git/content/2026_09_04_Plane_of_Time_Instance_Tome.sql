-- Add a guild-instance tome near the Plane of Time portal in Plane of Tranquility.
INSERT INTO `doors` (
    `id`,
    `doorid`,
    `zone`,
    `name`,
    `pos_y`,
    `pos_x`,
    `pos_z`,
    `heading`,
    `opentype`,
    `dest_zone`,
    `guild_zone_door`
) VALUES (
    6108354,
    109,
    'potranquility',
    'POKTELE500',
    -2194.97,
    1126.11,
    -914.22,
    176,
    58,
    'potimea',
    1
) ON DUPLICATE KEY UPDATE
    `pos_y` = VALUES(`pos_y`),
    `pos_x` = VALUES(`pos_x`),
    `pos_z` = VALUES(`pos_z`),
    `heading` = VALUES(`heading`),
    `dest_zone` = VALUES(`dest_zone`),
    `guild_zone_door` = VALUES(`guild_zone_door`);
