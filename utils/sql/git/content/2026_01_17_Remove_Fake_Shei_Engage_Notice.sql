-- Fixing broadcast announcement to not trigger on death of Fake Shei
UPDATE npc_types nt SET engage_notice = 0 WHERE nt.id = 179032; -- fake shei
