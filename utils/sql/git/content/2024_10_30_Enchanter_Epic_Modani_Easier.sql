SELECT spawnentry.spawngroupID, spawnentry.npcID 
INTO @spawngroupID, @npcID
FROM spawnentry
JOIN npc_types ON npc_types.id = spawnentry.npcID
WHERE npc_types.name = "Modani_Qu`Loni";

-- Update non-Modani to 0 chance
-- Do this instead of deleting just in case we want to revert at some point
UPDATE spawnentry
SET chance = 0
WHERE spawngroupID = @spawngroupID AND npcID <> @npcID;

-- Update Modani to 100 chance
UPDATE spawnentry
SET chance = 100
WHERE spawngroupID = @spawngroupID AND npcID = @npcID;

SELECT spawn2.id
INTO @spawn2id 
FROM spawn2
WHERE spawn2.spawngroupID = @spawngroupID;

-- Update respawn time to 15 minutes
UPDATE spawn2
SET respawntime = 900
WHERE id = @spawn2id;

-- Helpful query to double check the results

-- SELECT npc_types.name, spawnentry.*, spawngroup.*, spawn2.* 
-- FROM spawnentry
-- JOIN npc_types ON npc_types.id = spawnentry.npcID
-- JOIN spawngroup ON spawnentry.spawngroupID = spawngroup.id
-- JOIN spawn2 ON spawn2.spawngroupID = spawngroup.id
-- WHERE spawnentry.spawngroupID = @spawngroupID;


-- Update the epic drop from 88% to 100%
SELECT lde.lootdrop_id, lde.item_id 
INTO @lootdrop_id, @item_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "Modani_Qu`Loni" AND items.Name = "Flowing Black Robe"
LIMIT 1;

UPDATE lootdrop_entries
SET chance = 100
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;

-- Helpful query to double check the results
-- SELECT npc_types.name, lte.*, lde.*, items.Name 
-- FROM npc_types
-- JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id 
-- WHERE npc_types.name = "Modani_Qu`Loni";
