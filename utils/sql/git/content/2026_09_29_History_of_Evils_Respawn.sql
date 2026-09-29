-- History of Evils: The Age of Scale is a Plane of Knowledge ground pickup.
-- Reduce its respawn from 30 minutes to five minutes.
UPDATE `ground_spawns`
SET `respawn_timer` = 300000
WHERE `zoneid` = 202
  AND `item` = 28188;
