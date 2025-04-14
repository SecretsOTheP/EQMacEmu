-- Makes ToV mini items stackable. 
-- (Fixed patch, includes itemtype 17)
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31250; -- White Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31251; -- Black Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31252; -- Runed Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31253; -- Silver Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31254; -- serrated Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31255; -- Poison Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31256; -- Flame Kissed Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31257; -- Emerald Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31258; -- Platinum Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1 WHERE id = 31259; -- Ruby Symbol
UPDATE items SET itemtype = 17, stackable = 1, stacksize = 20, nodrop = 1, maxcharges = 1, lore = 'Glowing Orb' WHERE id = 31260; -- Glowing Drake Orb (removing Lore as well)

