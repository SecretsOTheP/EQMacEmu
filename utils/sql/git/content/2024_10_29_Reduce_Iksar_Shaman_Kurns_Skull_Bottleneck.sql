-- Update the lore skull item to 17% drop rate so the ratio is even with the quest requirements of 1 lore, 5 non-lore

SELECT lde.lootdrop_id, lde.item_id 
INTO @lootdrop_id, @item_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "undead_crusader" AND items.lore = "*Leader of Bone Brethren"
LIMIT 1;

UPDATE lootdrop_entries
SET chance = 17
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;

-- Update the non-lore skull item to 83% drop rate

SELECT lde.lootdrop_id, lde.item_id 
INTO @lootdrop_id, @item_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "undead_crusader" AND items.lore = "Caste of Bone Brethren"
LIMIT 1;

UPDATE lootdrop_entries
SET chance = 83
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;

-- Fingered skeleton is 100% drop rate for all items at 22% spawn rate on one spawn with 6 min spawn.  Should get all items in 24-30 minutes on average
-- Making it 2 fingers at a time would be 48-60 mins to get all 4 fingers
-- This is a  harder area with the burynai than upstairs for the shaman skulls, so makes sense for it to be a little better chance imo

-- Undead crusader is 10% drop rate, 10% spawn rate, 3 spawns with 6 min spawn - so 1 spawn every 2 minutes, so 20 mins per mob, 200 mins per lore skull
-- We increased drop rate is 17%, which should make it 117 minutes
-- Increasing spawn rate to 16%  - 3 spawns with 6 min spawn - so 1 spawn every 2 minutes, so 12.5 minutes per mob, 73 min per lore skull
-- I tested this with #repop and #listnpcs "undead_crusader" and it took a long time even then to get 3 skulls
-- After which I realized that shamans need 50% more drops than monks for the fingers

-- So increasing the spawn rate by ~50% more would make sense, so bump it to 25%, leaving 75 remaining, which puts the others at 25 each

-- Testing this change it took me 12 spawn cycles to get 1 lore skull and 5+ normal skulls. I think I got 9-10 normal skulls.  With fast respawns this is 72 minutes

SELECT spawnentry.spawngroupID, spawnentry.npcID 
INTO @spawngroupID, @crusaderID
FROM spawnentry
JOIN npc_types ON npc_types.id = spawnentry.npcID
WHERE npc_types.name = "undead_crusader";

-- Update non-crusader spawn to 28
UPDATE spawnentry
SET chance = 25
WHERE spawngroupID = @spawngroupID AND npcID <> @crusaderID;

-- Update Crusader spawn rate to 16
UPDATE spawnentry
SET chance = 25
WHERE spawngroupID = @spawngroupID AND npcID = @crusaderID;

-- Helpful query to double check the results

-- SELECT npc_types.name, spawnentry.*, spawngroup.*, spawn2.* FROM spawnentry
-- JOIN npc_types ON npc_types.id = spawnentry.npcID
-- JOIN spawngroup ON spawnentry.spawngroupID = spawngroup.id
-- JOIN spawn2 ON spawn2.spawngroupID = spawngroup.id
-- WHERE spawnentry.spawngroupID = @spawngroupID;


-- Reduce drop rate of fine steel drops to match closer to what it was before this change

SELECT lde.lootdrop_id, lde.item_id, lte.loottable_id
INTO @lootdrop_id, @item_id, @loottable_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "undead_crusader" AND items.lore = "Fine Steel Morning Star"
LIMIT 1;

UPDATE lootdrop_entries
SET chance = 40
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;

UPDATE loottable_entries
SET probability = 100, mindrop = 0
WHERE lootdrop_id = @lootdrop_id AND loottable_id = @loottable_id;

-- Helpful query to double check the results

-- SELECT npc_types.name, lte.*, lde.*, items.Name 
-- FROM npc_types
-- JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id 
-- WHERE npc_types.name = "undead_crusader" AND items.Name = "Fine Steel Morning Star";

-- Another helpful query

-- SELECT npc_types.name, spawnentry.spawngroupID, spawnentry.chance, spawngroup.*, spawn2.* FROM spawnentry
-- JOIN npc_types ON npc_types.id = spawnentry.npcID
-- JOIN spawngroup ON spawnentry.spawngroupID = spawngroup.id
-- JOIN spawn2 ON spawn2.spawngroupID = spawngroup.id
-- WHERE npc_types.name = "undead_crusader";
