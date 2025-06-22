-- Create ST 2.0 lootdrop tables
INSERT INTO lootdrop
(id, name, min_expansion, max_expansion, content_flags, content_flags_disabled)
VALUES(6150522, 'sleeper_2.0_golem_loot', -1.0, -1.0, NULL, NULL);
INSERT INTO lootdrop
(id, name, min_expansion, max_expansion, content_flags, content_flags_disabled)
VALUES(6150523, 'sleeper_2.0_warder_loot', -1.0, -1.0, NULL, NULL);

-- Add 2.0 golem lootdrop_entries to sleeper_2.0_golem_loot
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150522, 29476, 1, 0, 20.0, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150522, 29477, 1, 0, 20.0, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150522, 29478, 1, 0, 20.0, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150522, 29480, 1, 0, 20.0, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150522, 29482, 1, 0, 20.0, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);

-- Add 2.0 warder lootdrop_entries to sleeper_2.0_warder_loot
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29445, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29446, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29447, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29448, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29449, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29450, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29451, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29452, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29453, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29454, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29455, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29456, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29457, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29458, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29459, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29460, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29481, 1, 0, 5.55, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);

-- These last 4 have a lower drop rate because each "eye" was supposed to only drop from the respective ancient warder. 
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29464, 1, 0, 1.41, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29463, 1, 0, 1.41, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29462, 1, 0, 1.41, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);
INSERT INTO lootdrop_entries
(lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer, content_flags_disabled, content_flags)
VALUES(6150523, 29461, 1, 0, 1.41, 0, 255, 1, 0.0, -1.0, -1.0, 0, 0, NULL, NULL);

-- Add 2.0 golem loot to golem bosses. 1 drop at 100% chance
INSERT INTO loottable_entries
(loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
VALUES(91097, 6150522, 1, 100, 1, 1, 0);

-- Add 2.0 warder loot to golem bosses. 0-1 drops, 20% chance
INSERT INTO loottable_entries
(loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
VALUES(91097, 6150523, 1, 20, 1, 1, 0);

-- Add 2.0 warder loot to warder bosses. 2 drops, 100% chance
INSERT INTO loottable_entries
(loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
VALUES(129, 6150523, 1, 100, 2, 2, 0);
INSERT INTO loottable_entries
(loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
VALUES(130, 6150523, 1, 100, 2, 2, 0);
INSERT INTO loottable_entries
(loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
VALUES(131, 6150523, 1, 100, 2, 2, 0);
INSERT INTO loottable_entries
(loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
VALUES(154, 6150523, 1, 100, 2, 2, 0);