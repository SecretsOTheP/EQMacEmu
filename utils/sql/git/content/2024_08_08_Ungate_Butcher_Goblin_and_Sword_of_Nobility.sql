-- Ungate Blade of Nobility(2412) for Ghoulbane quest
UPDATE lootdrop_entries SET min_expansion = 1 WHERE lootdrop_id = 105704 AND item_id = 2412;

-- Ungate a_goblin in butcher for Sword of Nobility quest leading up to Ghoulbane spawngroup(222155)
UPDATE spawn2 SET min_expansion = 1 WHERE id = 367974;