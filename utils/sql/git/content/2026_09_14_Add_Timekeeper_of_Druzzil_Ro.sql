-- Timekeeper of Druzzil Ro in Plane of Tranquility.
-- In-game location: X -2232.19, Y 1170.21, Z -899.16.

UPDATE npc_types AS timekeeper
JOIN npc_types AS agent ON agent.name = 'Agent_of_Druzzil_Ro'
SET timekeeper.name = 'Timekeeper_of_Druzzil_Ro',
    timekeeper.race = agent.race,
    timekeeper.gender = agent.gender,
    timekeeper.texture = agent.texture,
    timekeeper.helmtexture = agent.helmtexture,
    timekeeper.size = agent.size,
    timekeeper.face = agent.face,
    timekeeper.luclin_haircolor = agent.luclin_haircolor,
    timekeeper.luclin_hairstyle = agent.luclin_hairstyle,
    timekeeper.luclin_eyecolor = agent.luclin_eyecolor,
    timekeeper.luclin_eyecolor2 = agent.luclin_eyecolor2,
    timekeeper.luclin_beardcolor = agent.luclin_beardcolor,
    timekeeper.luclin_beard = agent.luclin_beard
WHERE timekeeper.id = 7151002;

INSERT INTO spawnentry (spawngroupID, npcID, chance, mintime, maxtime, min_expansion, max_expansion, content_flags, content_flags_disabled)
SELECT 7151002, 7151002, 100, 0, 0, -1, -1, NULL, NULL
WHERE NOT EXISTS (
    SELECT 1 FROM spawnentry WHERE spawngroupID = 7151002 AND npcID = 7151002
);

UPDATE spawn2
SET x = -2232.19, y = 1170.21, z = -899.16, heading = 0,
    respawntime = 3600, variance = 0, enabled = 1
WHERE zone = 'potranquility' AND spawngroupID = 7151002;

INSERT INTO spawn2 (id, spawngroupID, zone, x, y, z, heading, respawntime, variance, pathgrid, _condition, cond_value, enabled, animation, boot_respawntime, clear_timer_onboot, boot_variance, force_z, min_expansion, max_expansion, content_flags, content_flags_disabled)
SELECT (SELECT COALESCE(MAX(id), 0) + 1 FROM spawn2), 7151002, 'potranquility', -2232.19, 1170.21, -899.16, 0, 3600, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, -1, -1, NULL, NULL
WHERE NOT EXISTS (
    SELECT 1 FROM spawn2 WHERE zone = 'potranquility' AND spawngroupID = 7151002
);
