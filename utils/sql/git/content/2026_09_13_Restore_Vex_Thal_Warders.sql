-- Restore the Vex Thal Akhevan Warder spawn points.
UPDATE spawn2 AS s2
JOIN spawnentry AS se ON se.spawngroupID = s2.spawngroupID
SET s2.enabled = 1
WHERE s2.zone = 'vexthal'
  AND se.npcID IN (158393, 158399, 158405, 158409, 158418);
