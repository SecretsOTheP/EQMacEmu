-- Guarantee crystalline globe piece 25797 from Sergeant Terrick Burns.
-- Lootdrop 22761 contains only this item, already at 100% internally.
-- Raise its loot-table roll from 15% to 100% for Terrick only.
-- Leave the Undead Vassal loot table (4989) at its existing 5% chance.
UPDATE `loottable_entries`
SET `probability` = 100
WHERE `loottable_id` = 96945
  AND `lootdrop_id` = 22761;
