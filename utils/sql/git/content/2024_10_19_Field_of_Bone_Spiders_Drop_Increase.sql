-- Get the keys for the lootdrop entries from bonecrawler spider silk drop
SELECT lde.lootdrop_id, lde.item_id 
INTO @lootdrop_id, @item_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "bonecrawler" AND items.Name = "spider silk"
LIMIT 1;

SELECT @lootdrop_id, @item_id;

UPDATE lootdrop_entries
SET multiplier = 4
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;

-- Get the keys for the lootdrop entries from bonebinder spider silk drop
SELECT lde.lootdrop_id, lde.item_id 
INTO @lootdrop_id, @item_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "a_bonebinder" AND items.Name = "spider silk"
LIMIT 1;

SELECT @lootdrop_id, @item_id;

UPDATE lootdrop_entries
SET multiplier = 4
WHERE lootdrop_id = @lootdrop_id AND item_id = @item_id;
