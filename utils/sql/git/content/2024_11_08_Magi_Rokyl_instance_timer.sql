-- Instance override timer was not set for Magi Rokyl (Kunark and earlier), but was for Magus Rokyl (Velious and later)
UPDATE npc_types SET instance_spawn_timer_override = 64800000 WHERE id = 32000;