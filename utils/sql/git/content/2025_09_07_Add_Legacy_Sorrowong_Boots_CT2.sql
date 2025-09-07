INSERT INTO lootdrop (name,                          min_expansion, max_expansion)
VALUES               ('a_gyrating_mass_legacy_loot', 3,             99);
SET @a_gyrating_mass_legacy_loot_lootdrop_id = LAST_INSERT_ID();

INSERT INTO lootdrop_entries (lootdrop_id,                              item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer) 
VALUES                       (@a_gyrating_mass_legacy_loot_lootdrop_id, 27846,   1,            1,          50,     0,        255,      1,          0,               3,             99,            50,               2629800);

INSERT INTO loottable_entries (loottable_id, lootdrop_id,                              multiplier, probability, droplimit, mindrop, multiplier_min) 
VALUES                        (96777,        @a_gyrating_mass_legacy_loot_lootdrop_id, 1,          100,         0,         0,      1);
