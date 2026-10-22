-- Ground spawns for the two foraged Aid Grimel / Party Platter components,
-- so characters without a useful Forage skill have a self-found source.
-- One spawn each, at the zone-in (zone safe point), 1 hour respawn.
INSERT INTO ground_spawns (zoneid, max_x, max_y, max_z, min_x, min_y, name, item, max_allowed, `comment`, respawn_timer)
VALUES
(201, 58, -61, 5, 58, -61, 'IT403', 29755, 1, 'Justice Fruit', 3600000), /* pojustice */
(217, -1387, 1210, -182, -1387, 1210, 'IT403', 29748, 1, 'Habanero Pepper', 3600000); /* pofire */
