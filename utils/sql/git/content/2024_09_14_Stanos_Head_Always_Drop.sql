/* Make Stanos Herkanor(5088) in highpass drop head at 100% rate */
UPDATE loottable_entries SET droplimit = 1, mindrop = 1 WHERE loottable_id = 94877 AND lootdrop_id = 109729;
UPDATE lootdrop_entries SET chance = 100 WHERE lootdrop_id = 109729 AND item_id = 28058;