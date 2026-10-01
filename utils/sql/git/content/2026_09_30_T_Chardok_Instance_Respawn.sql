-- Replace the old 1,592,000,000-ms Chardok marker with a real 18-hour
-- instance respawn override. Deploy with the matching zone/spawn2.cpp change.
UPDATE npc_types
SET instance_spawn_timer_override = 64800000
WHERE id BETWEEN 103000 AND 103250
  AND instance_spawn_timer_override = 1592000000;
