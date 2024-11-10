DELETE FROM lootdrop
WHERE NAME = "droga_eoe";

INSERT INTO `lootdrop` (`name`, `min_expansion`, `max_expansion`, `content_flags`, `content_flags_disabled`) 
VALUES 
('droga_eoe', -1, -1, NULL, NULL);

-- Retrieve the last inserted ID
SELECT id
INTO @lootdrop_id
FROM lootdrop
WHERE NAME = "droga_eoe";

SELECT id
INTO @earring_item_id
FROM items
WHERE Name = "Earring of Essence";

INSERT INTO `lootdrop_entries` (`lootdrop_id`, `item_id`, `item_charges`, `equip_item`, `chance`, `minlevel`, `maxlevel`, `multiplier`, `disabled_chance`, `min_expansion`, `max_expansion`, `min_looter_level`, `item_loot_lockout_timer`, `content_flags_disabled`, `content_flags`) 
VALUES 
(@lootdrop_id, @earring_item_id, 1, 1, 0.01, 0, 255, 1, 0, -1, -1, 0, 0, NULL, NULL);

-- Update all NPCs loot tables to have this 

-- Get list of all NPCs in zone except for Soothsayer Dregzak

SELECT spawnentry.npcID
FROM spawn2
JOIN spawngroup ON spawngroup.id = spawn2.spawngroupID
JOIN spawnentry ON spawnentry.spawngroupID = spawngroup.id
WHERE spawn2.zone = "droga"
GROUP BY npcID;

-- Loot table IDs for all mobs in droga that aren't named and have loot tables
CREATE TEMPORARY TABLE temp_loottable_ids 
SELECT loottable_id FROM
(
SELECT spawnentry.npcID AS npcID
FROM spawn2
JOIN spawngroup ON spawngroup.id = spawn2.spawngroupID
JOIN spawnentry ON spawnentry.spawngroupID = spawngroup.id
WHERE spawn2.zone = "droga"
GROUP BY npcID
) AS npcsInDroga
JOIN npc_types ON npc_types.id = npcsInDroga.npcID
WHERE (npc_types.name LIKE "a_%" OR npc_types.name LIKE "an_%") AND npc_types.loottable_id <> 0
GROUP BY npc_types.loottable_id;

INSERT INTO `loottable_entries` (`loottable_id`, `lootdrop_id`, `multiplier`, `probability`, `droplimit`, `mindrop`, `multiplier_min`) 
SELECT loottable_id, @lootdrop_id, 1, 100, 1, 0, 0
FROM temp_loottable_ids;

-- Helpful query for testing
 
-- SELECT npc_types.name, items.Name, lte.*, lde.*
-- FROM npc_types 
-- JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id
-- WHERE npc_types.loottable_id IN (SELECT * FROM temp_loottable_ids);

DROP TEMPORARY TABLE temp_loottable_ids;





