

-- -----------------------------
-- Lady Vox: White Dragon Scales
-- -----------------------------
INSERT INTO lootdrop (name,             min_expansion, max_expansion)
VALUES               ('Lady_Vox_Scale', 1,             99);
SET @white_scale_drop_id = LAST_INSERT_ID();

INSERT INTO lootdrop_entries (lootdrop_id,          item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer) 
VALUES                       (@white_scale_drop_id, 11602,   1,            0,          100,    0,        255,      1,          0,               1,             99,            0,                0);

INSERT INTO loottable_entries (loottable_id, lootdrop_id,          multiplier, probability, droplimit, mindrop, multiplier_min) 
VALUES                        (93174,        @white_scale_drop_id, 1,          100,         1,         1,       1);

-- ----------------------------------
-- Lady Vox: Torn, frost covered book
-- ----------------------------------

INSERT INTO lootdrop (name,            min_expansion, max_expansion)
VALUES               ('Lady_Vox_Book', 1,             99);
SET @frost_book_drop_id = LAST_INSERT_ID();

INSERT INTO lootdrop_entries (lootdrop_id,         item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer) 
VALUES                       (@frost_book_drop_id, 19070,   1,            0,          100,    0,        255,      1,          0,               1,             99,            0,                0);

INSERT INTO loottable_entries (loottable_id, lootdrop_id,         multiplier, probability, droplimit, mindrop, multiplier_min) 
VALUES                        (93174,        @frost_book_drop_id, 1,          100,         1,         1,       1);

-- -------------------------------
-- Lord Nagafen: Red Dragon Scales
-- ------------------------------- 

INSERT INTO lootdrop (name,                 min_expansion, max_expansion)
VALUES               ('Lord_Nagafen_Scale', 1,             99);
SET @red_scale_drop_id = LAST_INSERT_ID();

INSERT INTO lootdrop_entries (lootdrop_id,        item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer) 
VALUES                       (@red_scale_drop_id, 11622,   1,            0,          100,    0,        255,      1,          0,               1,             99,            0,                0);

INSERT INTO loottable_entries (loottable_id, lootdrop_id,        multiplier, probability, droplimit, mindrop, multiplier_min) 
VALUES                        (93173,        @red_scale_drop_id, 1,          100,         1,         1,       1);

-- ------------------------------
-- Lord Nagafen: Torn, burnt book
-- ------------------------------- 

INSERT INTO lootdrop (name,                min_expansion, max_expansion)
VALUES               ('Lord_Nagafen_Book', 1,             99);
SET @burnt_book_drop_id = LAST_INSERT_ID();

INSERT INTO lootdrop_entries (lootdrop_id,         item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer) 
VALUES                       (@burnt_book_drop_id, 19071,   1,            0,          100,    0,        255,      1,          0,               1,             99,            0,                0);

INSERT INTO loottable_entries (loottable_id, lootdrop_id,         multiplier, probability, droplimit, mindrop, multiplier_min) 
VALUES                        (93173,        @burnt_book_drop_id, 1,          100,         1,         1,       1);

-- -----------------------------------
-- Remove Scales/Book from common pool
-- -----------------------------------
UPDATE lootdrop_entries SET min_expansion = 0, max_expansion = 0.9 WHERE lootdrop_id = 104461 AND item_id = 11602; -- Lady Vox: Scale
UPDATE lootdrop_entries SET min_expansion = 99, max_expansion = 99 WHERE lootdrop_id = 104461 AND item_id = 19070; -- Lady Vox: Book
UPDATE lootdrop_entries SET min_expansion = 0, max_expansion = 0.9 WHERE lootdrop_id = 104458 AND item_id = 11622; -- Nagafen: Scale
UPDATE lootdrop_entries SET min_expansion = 99, max_expansion = 99 WHERE lootdrop_id = 104458 AND item_id = 19071; -- Nagafen: Book