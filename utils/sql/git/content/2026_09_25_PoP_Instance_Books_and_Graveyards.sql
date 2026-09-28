-- Add the imported PoP guild-instance books and their invisible NPC spawns.
-- Door 109 (Plane of Time) is already added by 2026_09_04_Plane_of_Time_Instance_Tome.sql.

INSERT INTO `npc_types` (`id`, `name`, `level`, `race`, `class`, `bodytype`, `hp`, `size`, `attack_count`) VALUES
    (7151002, 'Plane of Valor', 1, 127, 1, 60, 1, 5, 0),
    (7151003, 'Plane of Torment', 1, 127, 1, 60, 1, 5, 0),
    (7151004, 'Crypt of Decay', 1, 127, 1, 60, 1, 5, 0),
    (7151005, 'Plane of Water', 1, 127, 1, 60, 1, 5, 0),
    (7151006, 'Plane of Fire', 1, 127, 1, 60, 1, 5, 0),
    (7151007, 'Plane of Air', 1, 127, 1, 60, 1, 5, 0),
    (7151008, 'Plane of Earth', 1, 127, 1, 60, 1, 5, 0),
    (7151009, 'EarthB', 1, 127, 1, 60, 1, 5, 0),
    (7151010, 'Plane of Justice', 1, 127, 1, 60, 1, 5, 0),
    (7151011, 'Plane of Disease', 1, 127, 1, 60, 1, 5, 0),
    (7151012, 'Plane of Nightmare', 1, 127, 1, 60, 1, 5, 0),
    (7151013, 'NightmareB', 1, 127, 1, 60, 1, 5, 0),
    (7151014, 'Plane of Innovation', 1, 127, 1, 60, 1, 5, 0),
    (7151201, 'Soluseks Tower', 1, 127, 1, 60, 1, 5, 0),
    (7151202, 'Temple of Marr', 1, 127, 1, 60, 1, 5, 0),
    (7151203, 'Halls of Honor', 1, 127, 1, 60, 1, 5, 0),
    (7151204, 'Plane of Storms', 1, 127, 1, 60, 1, 5, 0),
    (7151205, 'Bastion of Thunder', 1, 127, 1, 60, 1, 5, 0),
    (7151206, 'Plane of Tactics', 1, 127, 1, 60, 1, 5, 0)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `level` = VALUES(`level`),
    `race` = VALUES(`race`),
    `class` = VALUES(`class`),
    `bodytype` = VALUES(`bodytype`),
    `hp` = VALUES(`hp`),
    `size` = VALUES(`size`),
    `attack_count` = VALUES(`attack_count`);

INSERT INTO `spawngroup` (`id`, `name`, `spawn_limit`) VALUES
    (6151030, 'sg_torment_book_potranquility', 1),
    (6151031, 'sg_decay_book_potranquility', 1),
    (6151032, 'sg_water_book_potranquility', 1),
    (6151033, 'sg_fire_book_potranquility', 1),
    (6151034, 'sg_air_book_potranquility', 1),
    (6151035, 'sg_earth_book_potranquility', 1),
    (6151036, 'sg_earthb_book_poeartha', 1),
    (6151037, 'sg_justice_book_potranquility', 1),
    (6151038, 'sg_disease_book_potranquility', 1),
    (6151039, 'sg_nightmare_book_potranquility', 1),
    (6151040, 'sg_nightmareb_book_ponightmare', 1),
    (6151041, 'sg_innovation_book_potranquility', 1),
    (6151142, 'sg_valor_book_potranquility', 1),
    (7151201, 'sg_potranquility_book_solrotower', 1),
    (7151202, 'sg_potranquility_book_hohonorb', 1),
    (7151203, 'sg_potranquility_book_hohonora', 1),
    (7151204, 'sg_potranquility_book_postorms', 1),
    (7151205, 'sg_potranquility_book_bothunder', 1),
    (7151206, 'sg_potranquility_book_potactics', 1)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `spawn_limit` = VALUES(`spawn_limit`);

INSERT INTO `spawnentry` (`spawngroupID`, `npcID`, `chance`) VALUES
    (6151030, 7151003, 100),
    (6151031, 7151004, 100),
    (6151032, 7151005, 100),
    (6151033, 7151006, 100),
    (6151034, 7151007, 100),
    (6151035, 7151008, 100),
    (6151036, 7151009, 100),
    (6151037, 7151010, 100),
    (6151038, 7151011, 100),
    (6151039, 7151012, 100),
    (6151040, 7151013, 100),
    (6151041, 7151014, 100),
    (6151142, 7151002, 100),
    (7151201, 7151201, 100),
    (7151202, 7151202, 100),
    (7151203, 7151203, 100),
    (7151204, 7151204, 100),
    (7151205, 7151205, 100),
    (7151206, 7151206, 100)
ON DUPLICATE KEY UPDATE `chance` = VALUES(`chance`);

INSERT INTO `spawn2` (`id`, `spawngroupID`, `zone`, `x`, `y`, `z`, `heading`) VALUES
    (21450287, 6151142, 'potranquility', -1616.0, 892.0, -894.549988, 8.0),
    (21450288, 6151030, 'potranquility', -1646.0, 848.0, -894.549988, 8.0),
    (21450289, 6151031, 'potranquility', -1663.550049, 892.119995, -894.549988, 8.0),
    (21450290, 6151032, 'potranquility', -2114.0, 148.0, -930.530029, 120.0),
    (21450291, 6151033, 'potranquility', -2069.0, 193.0, -930.530029, 120.0),
    (21450292, 6151034, 'potranquility', -2112.0, 363.0, -930.530029, 120.0),
    (21450293, 6151035, 'potranquility', -2063.0, 315.0, -930.530029, 120.0),
    (21450294, 6151036, 'poeartha', 1509.0, -2753.0, 0.6, 120.0),
    (21450295, 6151037, 'potranquility', -1676.0, 530.0, -914.859985, 256.0),
    (21450296, 6151038, 'potranquility', -1672.0, 456.0, -914.859985, 256.0),
    (21450297, 6151039, 'potranquility', -1626.0, 530.0, -914.859985, 256.0),
    (21450298, 6151040, 'ponightmare', -1851.0, 197.0, 117.110001, 124.0),
    (21450299, 6151041, 'potranquility', -1626.0, 456.0, -914.859985, 256.0),
    (21450600, 7151201, 'potranquility', -1825.0, 680.0, -911.52002, 10.0),
    (21450601, 7151202, 'potranquility', -1785.0, 680.0, -911.52002, 10.0),
    (21450602, 7151203, 'potranquility', -1765.0, 680.0, -911.52002, 10.0),
    (21450603, 7151204, 'potranquility', -1805.0, 680.0, -911.52002, 10.0),
    (21450604, 7151205, 'potranquility', -1845.0, 680.0, -911.52002, 10.0),
    (21450605, 7151206, 'potranquility', -1865.0, 680.0, -911.52002, 10.0)
ON DUPLICATE KEY UPDATE
    `spawngroupID` = VALUES(`spawngroupID`),
    `zone` = VALUES(`zone`),
    `x` = VALUES(`x`),
    `y` = VALUES(`y`),
    `z` = VALUES(`z`),
    `heading` = VALUES(`heading`);

-- Add the imported Plane of Tranquility instance-book portals except door 109,
-- which is already covered by 2026_09_04_Plane_of_Time_Instance_Tome.sql.
-- Door IDs are the unique (zone, doorid) key; database row IDs are auto-assigned.
INSERT INTO `doors` (
    `doorid`, `zone`, `name`, `pos_x`, `pos_y`, `pos_z`, `heading`, `opentype`,
    `dest_zone`, `dest_x`, `dest_y`, `dest_z`, `dest_heading`, `guild_zone_door`
) VALUES
    (98, 'potranquility', 'POKTELE500', -1616.07, 892.12, -894.55, 8.0, 58, 'povalor', 190.0, -1668.0, 65.0, 0.0, 1),
    (99, 'potranquility', 'POKTELE500', -1646.0, 848.0, -894.55, 8.0, 58, 'potorment', -341.0, 1706.0, -491.0, 0.0, 1),
    (100, 'potranquility', 'POKTELE500', -1663.55, 892.12, -894.55, 8.0, 58, 'codecay', -170.0, -65.0, -93.0, 0.0, 1),
    (101, 'potranquility', 'POKTELE500', -2114.0, 148.0, -930.53, 120.0, 58, 'powater', -93.0, -1234.0, 11.0, 105.0, 1),
    (102, 'potranquility', 'POKTELE500', -2069.0, 193.0, -930.53, 120.0, 58, 'pofire', -1387.0, 1210.0, -180.0, 251.0, 1),
    (103, 'potranquility', 'POKTELE500', -2112.0, 363.0, -930.53, 120.0, 58, 'poair', 532.0, 884.0, -90.0, 0.0, 1),
    (104, 'potranquility', 'POKTELE500', -2063.0, 315.0, -930.53, 120.0, 58, 'poeartha', -1150.0, 200.0, 71.0, 0.0, 1),
    (105, 'potranquility', 'POKTELE500', -1676.0, 530.0, -914.86, 256.0, 58, 'pojustice', 58.0, -61.0, 5.0, 127.0, 1),
    (106, 'potranquility', 'POKTELE500', -1672.0, 456.0, -914.86, 256.0, 58, 'podisease', -1750.0, -1245.0, -62.0, 6.0, 1),
    (107, 'potranquility', 'POKTELE500', -1626.0, 530.0, -914.86, 256.0, 58, 'ponightmare', 1668.0, 282.0, 212.0, 5.0, 1),
    (108, 'potranquility', 'POKTELE500', -1626.0, 456.0, -914.86, 256.0, 58, 'poinnovation', 263.0, 516.0, -53.0, 187.0, 1),
    (110, 'potranquility', 'POKTELE500', -1825.0, 680.0, -911.52, 10.0, 58, 'solrotower', -3.0, -2957.0, -764.0, 4.0, 1),
    (111, 'potranquility', 'POKTELE500', -1785.0, 680.0, -911.52, 10.0, 58, 'hohonorb', 975.0, 2.0, 396.0, 134.0, 1),
    (112, 'potranquility', 'POKTELE500', -1765.0, 680.0, -911.52, 10.0, 58, 'hohonora', -2760.0, -3.0, 130.0, 134.0, 1),
    (113, 'potranquility', 'POKTELE500', -1805.0, 680.0, -911.52, 10.0, 58, 'postorms', -1795.0, -2059.0, -475.0, 32.0, 1),
    (114, 'potranquility', 'POKTELE500', -1845.0, 680.0, -911.52, 10.0, 58, 'bothunder', 207.0, 178.0, -1626.0, 260.0, 1),
    (115, 'potranquility', 'POKTELE500', -1865.0, 680.0, -911.52, 10.0, 58, 'potactics', -210.0, 10.0, -38.9, 128.0, 1)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `pos_x` = VALUES(`pos_x`),
    `pos_y` = VALUES(`pos_y`),
    `pos_z` = VALUES(`pos_z`),
    `heading` = VALUES(`heading`),
    `opentype` = VALUES(`opentype`),
    `dest_zone` = VALUES(`dest_zone`),
    `dest_x` = VALUES(`dest_x`),
    `dest_y` = VALUES(`dest_y`),
    `dest_z` = VALUES(`dest_z`),
    `dest_heading` = VALUES(`dest_heading`),
    `guild_zone_door` = VALUES(`guild_zone_door`);

INSERT INTO `doors` (
    `doorid`, `zone`, `name`, `pos_x`, `pos_y`, `pos_z`, `heading`, `opentype`, `door_param`,
    `dest_zone`, `dest_x`, `dest_y`, `dest_z`, `dest_heading`, `incline`, `client_version_mask`,
    `min_expansion`, `max_expansion`
) VALUES
    (77, 'arena', 'POKTELE500', 147.807, -1022.76, 46.7928, 504.45, 58, 77, 'poknowledge', 140.0, -430.0, -152.0, 260.0, 0, 4294967294, 4, 99),
    (78, 'butcher', 'POKTELE500', -506.765, 1754.95, -3.65425, 258.958, 58, 77, 'poknowledge', 474.0, 829.0, -157.0, 511.0, 511, 4294967294, 4, 99),
    (1, 'everfrost', 'POKTELE500', -78.1207, 2887.25, -64.498, 238.73, 58, 0, 'poknowledge', 131.0, 846.0, -157.0, 511.0, 0, 4294967294, 4, 99),
    (77, 'feerrott', 'POKTELE500', -161.403, 867.158, -9.75448, 504.0, 58, 0, 'poknowledge', 444.0, -844.0, -157.0, 255.0, 509, 4294967294, 4, 99),
    (77, 'fieldofbone', 'POKTELE500', 1843.5, -3010.88, 8.377, 0.0, 58, 0, 'poknowledge', 36.0, -655.0, -156.0, 1.0, 0, 4294967294, 4, 99),
    (77, 'firiona', 'POKTELE500', 4717.31, -455.367, 10.1273, 384.0, 58, 0, 'poknowledge', -279.0, -363.0, -157.0, 383.0, 0, 4294967294, 4, 99),
    (177, 'freportw', 'POKTELE500', 77.3664, -682.064, -34.8048, 0.0, 58, 0, 'poknowledge', -234.0, -406.0, -157.0, 258.0, 0, 4294967294, 4, 99),
    (108, 'gfaydark', 'POKTELE500', -1820.6, -2260.51, -1.68313, 17.0, 58, 0, 'poknowledge', 97.0, 812.0, -157.0, 380.0, 0, 4294967294, 4, 99),
    (109, 'gfaydark', 'POKTELE500', -757.995, -173.6, -4.09175, 163.29, 58, 0, 'poknowledge', 882.0, 838.0, -157.0, 2.0, 0, 4294967294, 4, 99),
    (77, 'greatdivide', 'POKTELE500', -1811.42, 14.5537, 389.439, 272.0, 58, 0, 'poknowledge', -235.0, 393.0, -157.0, 405.0, 0, 4294967294, -1, -1),
    (1, 'innothule', 'POKTELE500', -29.9866, -729.133, -29.2157, 477.21, 58, 0, 'poknowledge', 95.0, -812.0, -157.0, 384.0, 0, 4294967294, 4, 99),
    (44, 'misty', 'POKTELE500', -1262.71, -559.555, 6.89263, 2.0, 58, 0, 'poknowledge', 1227.0, 830.0, -157.0, 511.0, 0, 4294967294, 4, 99),
    (78, 'nektulos', 'POKTELE500', -342.478, 742.973, -8.34902, 267.81, 58, 77, 'poknowledge', 132.0, -841.0, -157.0, 257.0, 0, 4294967294, 4, 99),
    (2, 'nexus', 'POKTELE500', 481.664, 44.9147, -30.8975, 384.0, 58, 0, 'poknowledge', -76.0, 381.0, -157.9, 1.0, 0, 4294967294, 4, 99),
    (78, 'nexus', 'POKTELE500', 481.664, 44.9147, -30.8975, 384.0, 58, 77, 'poknowledge', -76.0, 381.0, -157.9, 1.0, 0, 4294967294, 4, 99),
    (77, 'overthere', 'POKTELE500', 1937.59, 3134.56, -53.3417, 384.0, 58, 0, 'poknowledge', 884.0, -840.0, -157.0, 254.0, 0, 4294967294, 4, 99),
    (78, 'potranquility', 'POKTELE500', -1436.7, 773.064, -880.159, 375.0, 58, 77, 'poknowledge', -285.0, -148.0, -159.0, 383.0, 0, 4294967295, -1, -1),
    (77, 'qeynos2', 'POKTELE500', 484.484, 183.698, 0.002, 3.0, 58, 0, 'poknowledge', -289.0, 147.0, -157.0, 383.0, 0, 4294967294, 4, 99),
    (78, 'shadeweaver', 'POKTELE500', -2425.1, -3011.8, -218.342, 494.0, 58, 77, 'poknowledge', -293.0, 364.0, -157.0, 383.0, 511, 4294967294, 4, 99),
    (2, 'shadowrest', 'POKTELE500', -8.38333, -244.331, 4.0946, 384.0, 58, 0, 'poknowledge', 460.0, -13.0, -90.0, 128.0, 0, 4294967294, -1, -1),
    (46, 'steamfont', 'POKTELE500', 933.79, -1373.04, -111.662, 0.0, 58, 0, 'poknowledge', -76.0, -401.0, -157.0, 255.0, 0, 4294967294, -1, -1),
    (77, 'towerbone', 'POKTELE500', 1843.5, -3010.88, 8.377, 0.0, 58, 0, 'poknowledge', 36.0, -655.0, -156.0, 1.0, 0, 4294967294, -1, -1),
    (7, 'tox', 'POKTELE500', 295.884, -2344.14, -49.748, 371.01, 58, 0, 'poknowledge', 1227.0, -840.0, -157.0, 257.0, 0, 4294967294, 4, 99),
    (8, 'tox', 'POKTELE500', -582.532, 2324.96, -47.8143, 120.0, 58, 0, 'poknowledge', 36.0, 662.0, -157.0, 256.0, 0, 4294967294, 4, 99)
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
    `incline` = VALUES(`incline`),
    `client_version_mask` = VALUES(`client_version_mask`),
    `min_expansion` = VALUES(`min_expansion`),
    `max_expansion` = VALUES(`max_expansion`);

-- PoP graveyard_time is measured in minutes; use 60 minutes (one hour) wherever
-- a Plane of Power zone has an active graveyard configured.
UPDATE `zone`
SET `graveyard_time` = 60
WHERE `expansion` = 4
  AND `graveyard_id` > 0;
