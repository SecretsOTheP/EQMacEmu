-- 112935 lootdropid is the drop given to felwithe guards that contains 8 finesteel weapons at 4% drop rate each, see queries below to confirm
-- Seems safe to add high elf parts to this drop table

-- Didn't want to add it to the two handed longsword lootdrop table because that's already at 100% and don't want them to not have their featured longsword

-- Reasoning for 32% is the other high elves that drop it are at 20% and these guys are significantly harder to kill, so slightly more is justified imo
SELECT id
INTO @foundid
FROM items
WHERE items.Name = "High Elf Parts";

INSERT INTO `lootdrop_entries` (`lootdrop_id`, `item_id`, `item_charges`, `equip_item`, `chance`, `minlevel`, `maxlevel`, `multiplier`, `disabled_chance`, `min_expansion`, `max_expansion`, `min_looter_level`, `item_loot_lockout_timer`, `content_flags_disabled`, `content_flags`) 
VALUES 
(112935, @foundid, 1, 0, 32, 0, 255, 1, 0, -1, -1, 0, 0, NULL, NULL);


-- Handy queries to help confirm data


-- SELECT npc_types.name, items.Name, lte.*, lde.*
-- FROM npc_types
-- JOIN loottable_entries AS lte ON lte.loottable_id = npc_types.loottable_id
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id 
-- WHERE npc_types.name = "guard_legver";

-- -- Who has 99851 lootdropid

-- SELECT * FROM
-- loottable_entries AS lte
-- JOIN npc_types ON lte.loottable_id = npc_types.loottable_id
-- JOIN spawnentry ON npc_types.id = spawnentry.npcID
-- JOIN spawn2 ON spawn2.spawngroupID = spawnentry.spawngroupID
-- WHERE lte.lootdrop_id = 99851;

-- SELECT * FROM
-- loottable_entries AS lte
-- JOIN npc_types ON lte.loottable_id = npc_types.loottable_id
-- JOIN spawnentry ON npc_types.id = spawnentry.npcID
-- JOIN spawn2 ON spawn2.spawngroupID = spawnentry.spawngroupID
-- WHERE lte.lootdrop_id = 112935;

-- SELECT items.name, lde.* FROM
-- lootdrop_entries AS lde
-- JOIN items ON lde.item_id = items.id
-- WHERE lde.lootdrop_id = 112935;
