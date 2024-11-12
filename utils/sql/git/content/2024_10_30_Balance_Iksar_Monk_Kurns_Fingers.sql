-- Update drop chance to 50% for all fingers because right now you can complete the quest in 24-30 minutes, increases it to ~1 hour which is more in line with the new update for the shaman skulls

SELECT lde.lootdrop_id
INTO @lootdrop_id
FROM npc_types
JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
JOIN items ON items.id = lde.item_id 
WHERE npc_types.name = "fingered_skeleton" AND items.Name LIKE "%Finger"
LIMIT 1;

UPDATE lootdrop_entries
SET chance = 50
WHERE lootdrop_id = @lootdrop_id;

-- Helpful query for verifying results

-- SELECT npc_types.name, items.Name, lte.*, lde.*
-- FROM npc_types
-- JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id 
-- WHERE npc_types.name = "fingered_skeleton";
