-- Increase Hope Stone drops from the existing elemental-plane lootdrop.
UPDATE `lootdrop_entries`
SET `chance` = 25
WHERE `lootdrop_id` = 90402
  AND `item_id` = 16258;
