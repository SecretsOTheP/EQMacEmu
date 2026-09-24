-- Increase the Piece of the Crystalline Globe drop probability
-- from an Undead Vassal from 5% to 10%.
UPDATE loottable_entries
SET probability = 10
WHERE loottable_id = 4989
  AND lootdrop_id = 22761
  AND probability = 5;
