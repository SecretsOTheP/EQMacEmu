-- Add Spined Chain Mail (26527) to loot drops currently attached to Servitor of Luclin (163273)
INSERT INTO `lootdrop_entries` (`lootdrop_id`, `item_id`, `item_charges`, `equip_item`, `chance`, `minlevel`, `maxlevel`, `multiplier`, `disabled_chance`, `min_expansion`, `max_expansion`, `min_looter_level`, `item_loot_lockout_timer`, `content_flags_disabled`, `content_flags`)
VALUES (95133, 26527, 1, 1, 20, 0, 127, 1, 0, -1, -1, 0, 0, NULL, NULL);
-- Update the other 4 drops on Servitor of Luclin's loot table to be 20%
-- Lurid Dagger
UPDATE lootdrop_entries SET chance = 20 WHERE lootdrop_id = 95133 AND item_id = 26507;
-- Mask of Pulchritude
UPDATE lootdrop_entries SET chance = 20 WHERE lootdrop_id = 95133 AND item_id = 26513;
-- Sombrous Claidmore
UPDATE lootdrop_entries SET chance = 20 WHERE lootdrop_id = 95133 AND item_id = 26520;
-- Wand of Musing
UPDATE lootdrop_entries SET chance = 20 WHERE lootdrop_id = 95133 AND item_id = 26541;
