

/*
This Script increases the number of ST keys to 3 for Tormax and Statue. Optionally, it also does this for Vindi and Zlandi through a new lootdrop for each.

You can flip these variables as needed for comfort.

@key_count_per_mob - sets the number of Hsagra's Talismans (No-stat, No Drop ST Keys) that will drop from Ken Tormax, Statue, and Vindi
@ignore_vindi_updates - if this is set to TRUE script will NOT update vindis loot table to always drop a ST Key
@ignore_zlandi_updates - if this is set to TRUE script will NOT update zlandis loot table to drop extra Hsagra's Talismans (No-stat, No Drop ST Keys)

*/



SET @key_count_per_mob = 3;
SET @ignore_vindi_updates = FALSE;
SET @ignore_zlandi_updates = FALSE;

START TRANSACTION;

UPDATE loottable_entries
SET multiplier = @key_count_per_mob,
droplimit = 0,
mindrop = 0
WHERE lootdrop_id IN
(
111114,
111155
);



INSERT INTO lootdrop (id, NAME, min_expansion, max_expansion) 
SELECT '111201', 'kael Derakor_HsagraTalisman', -1, -1 FROM DUAL WHERE @ignore_vindi_updates = FALSE;

INSERT INTO lootdrop_entries (lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, 
	min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer)
SELECT '111201', '9296', 1, 0, 100, 0, 127, 1, 0, -1, -1, 0, 0 FROM DUAL WHERE @ignore_vindi_updates = FALSE;
	
INSERT INTO loottable_entries (loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
SELECT '95315', '111201', @key_count_per_mob, 100, 0, 0, 0 FROM DUAL WHERE @ignore_vindi_updates = FALSE;




INSERT INTO lootdrop (id, NAME, min_expansion, max_expansion) 
SELECT '112551', 'DN Zlandi_HsagraTalisman', -1, -1 FROM DUAL WHERE @ignore_zlandi_updates = FALSE;

INSERT INTO lootdrop_entries (lootdrop_id, item_id, item_charges, equip_item, chance, minlevel, maxlevel, multiplier, disabled_chance, 
	min_expansion, max_expansion, min_looter_level, item_loot_lockout_timer)
SELECT '112551', '9296', 1, 0, 100, 0, 127, 1, 0, -1, -1, 0, 0 FROM DUAL WHERE @ignore_zlandi_updates = FALSE;
	
INSERT INTO loottable_entries (loottable_id, lootdrop_id, multiplier, probability, droplimit, mindrop, multiplier_min)
SELECT '95315', '112551', @key_count_per_mob, 100, 0, 0, 0 FROM DUAL WHERE @ignore_zlandi_updates = FALSE;




COMMIT;
