-- Courier currently drops a few named items including the ring everyone wants
-- Increase all of those from 1-3% -> 4%.  Ring goes from 2% -> 4%
-- Keep sarnak blood the same (10%)

UPDATE lootdrop_entries AS lde
JOIN items ON lde.item_id = items.id
SET chance = 4
WHERE lde.lootdrop_id = 105798 AND items.Name <> "Sarnak Blood";

-- Helpful query

-- SELECT npc_types.name, items.Name, lte.*, lde.*
-- FROM npc_types
-- JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id 
-- WHERE npc_types.name = "a_sarnak_courier";
