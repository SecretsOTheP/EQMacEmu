UPDATE lootdrop_entries
SET chance = 30
WHERE item_id = 16838; -- heartsting venom sack

INSERT INTO `loottable_entries` (`loottable_id`, `lootdrop_id`, `multiplier`, `probability`, `droplimit`, `mindrop`, `multiplier_min`) 
VALUES 
(
	5779,  -- large heartsting scorpion's loottable 
	104743, -- lootdrop for heartsting venom sack found on the regular heartsting scorpion, so they'll have same drop rate
	1, 
	100, 
	0, 
	0, 
	0
);

-- Handy queries for inspecting drop tables of relevant NPCs

-- SELECT * FROM lootdrop_entries as lde
-- LEFT JOIN items ON items.id = lde.item_id
-- WHERE lootdrop_id = 104744 OR lootdrop_id = 104743;  -- 104743 is the venom sack

-- SELECT * FROM lootdrop_entries as lde
-- LEFT JOIN items ON items.id = lde.item_id
-- WHERE lootdrop_id = 10844;

-- SELECT * FROM loottable_entries AS lte
-- JOIN lootdrop_entries as lde ON lde.lootdrop_id = lte.lootdrop_id
-- JOIN items ON items.id = lde.item_id 
-- WHERE loottable_id = 5779;
