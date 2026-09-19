-- Timekeeper of Druzzil Ro in Plane of Tranquility.
-- Uses a dedicated NPC and spawngroup ID so it cannot collide with other content.

CREATE TEMPORARY TABLE timekeeper_template AS
SELECT *
FROM npc_types
WHERE name = 'Agent_of_Druzzil_Ro'
LIMIT 1;

UPDATE timekeeper_template
SET
    id = 7151200,
    name = 'Timekeeper_of_Druzzil_Ro';

INSERT INTO npc_types
SELECT *
FROM timekeeper_template
WHERE NOT EXISTS (
    SELECT 1
    FROM npc_types
    WHERE id = 7151200
);

DROP TEMPORARY TABLE timekeeper_template;

INSERT INTO spawngroup (
    id, name, spawn_limit, max_x, min_x, max_y, min_y,
    delay, mindelay, despawn, despawn_timer, rand_spawns,
    rand_respawntime, rand_variance, rand_condition_, wp_spawns
)
SELECT
    7151200, 'sg_timekeeper_druzzil_ro', 1, 0, 0, 0, 0,
    45000, 15000, 0, 100, 0,
    1200, 0, 0, 0
WHERE NOT EXISTS (
    SELECT 1
    FROM spawngroup
    WHERE id = 7151200
);

INSERT INTO spawnentry (
    spawngroupID, npcID, chance, mintime, maxtime,
    min_expansion, max_expansion, content_flags, content_flags_disabled
)
SELECT 7151200, 7151200, 100, 0, 0, -1, -1, NULL, NULL
WHERE NOT EXISTS (
    SELECT 1
    FROM spawnentry
    WHERE spawngroupID = 7151200
      AND npcID = 7151200
);

UPDATE spawn2
SET
    x = 1185, y = -2223, z = -901, heading = 0,
    respawntime = 3600, variance = 0, enabled = 1
WHERE zone = 'potranquility'
  AND spawngroupID = 7151200;

INSERT INTO spawn2 (
    id, spawngroupID, zone, x, y, z, heading,
    respawntime, variance, pathgrid, _condition, cond_value,
    enabled, animation, boot_respawntime, clear_timer_onboot,
    boot_variance, force_z, min_expansion, max_expansion,
    content_flags, content_flags_disabled
)
SELECT
    (SELECT COALESCE(MAX(id), 0) + 1 FROM spawn2),
    7151200, 'potranquility', 1185, -2223, -901, 0,
    3600, 0, 0, 0, 1,
    1, 0, 0, 0,
    0, 0, -1, -1,
    NULL, NULL
WHERE NOT EXISTS (
    SELECT 1
    FROM spawn2
    WHERE zone = 'potranquility'
      AND spawngroupID = 7151200
);
