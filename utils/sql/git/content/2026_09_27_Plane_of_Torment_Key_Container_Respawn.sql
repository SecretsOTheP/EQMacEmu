-- An Unimaginable Horror's death script spawns #Baraguj_Szuul, which drops
-- Mouths of Baraguj Szuul for the Screaming Sphere key quest.
-- Reduce the event gate's normal respawn to 30 minutes. In guild instances,
-- Timekeeper Slow Down may still apply the configured 18-hour minimum.
UPDATE `spawn2` AS s
JOIN `spawngroup` AS sg ON sg.`id` = s.`spawngroupID`
SET s.`respawntime` = 1800
WHERE s.`id` = 346828
	AND s.`zone` = 'potorment'
	AND sg.`name` = 'potorment_An_Unimaginable_Horror75'
	AND s.`respawntime` = 216000;
