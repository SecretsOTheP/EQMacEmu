-- Loot Drop Edits
-- Set drop chance on Medic's Pouch (7472) from A Fearful Vision (163024) to 50%
UPDATE lootdrop_entries SET chance = 50 WHERE lootdrop_id = 115147 AND item_id = 7472;

-- Set drop chance on Expeditious Boots (7477) from An Umbral Somnium (163011) to 50%
UPDATE lootdrop_entries SET chance = 50 WHERE lootdrop_id = 115145 AND item_id = 7477;

-- Set drop chance on Beer Goggles (7466) from A Lucid Vision (163025) and A Fearsome Vision (163026) to 50%
UPDATE lootdrop_entries SET chance = 50 WHERE lootdrop_id = 115146 AND item_id = 7466;

-- Spawn Chance Edits
-- Lower spawn chance of A Coelesced Sending (163003) to 75%
UPDATE spawnentry SET chance = 75 WHERE spawngroupID = 225096 AND npcID = 163003;
-- Raise spawn shance of An Umbral Somnium (163011) to 10%
UPDATE spawnentry SET chance = 10 WHERE spawngroupID = 225096 AND npcID = 163011;