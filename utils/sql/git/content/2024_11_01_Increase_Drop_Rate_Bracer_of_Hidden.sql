-- Update drop chance for bracer of the hidden to 1% up from 0.5%

SELECT lde.lootdrop_id, lde.item_id 
INTO @lootdrop_id, @item_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "a_sarnak_legionnaire" AND items.name = "Bracer of the Hidden"
LIMIT 1;

UPDATE lootdrop_entries
SET chance = 1
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;

-- Change 2 sarnak recruit spawns near the Chancellor into the spawngroup that has legionnaire in it
UPDATE spawn2
SET spawngroupID = 85087
WHERE id = 339621;

UPDATE spawn2
SET spawnGroupID = 85087
WHERE id = 339620;

