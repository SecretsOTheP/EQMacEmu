-- Fixed patch, includes itemtype 17
-- Faction turn-in items are now stackable: Giant Warrior Helmet, Coldain Head, Storm Giant Toes, Mercenary Assignments, Frost Giant Toes
UPDATE items SET itemtype = 17, stacksize = 20, stackable = 1, maxcharges = 1, races = 0 WHERE id = 29062; -- Giant Warrior Helmet
UPDATE items SET itemtype = 17, stacksize = 20, stackable = 1, maxcharges = 1 WHERE id = 30081; -- Coldain Head
UPDATE items SET itemtype = 17, stacksize = 20, stackable = 1, maxcharges = 1 WHERE id = 29124; -- Storm Giant Toes
UPDATE items SET itemtype = 17, stacksize = 20, stackable = 1, maxcharges = 1 WHERE id = 29624; -- Mercenary Assignments
UPDATE items SET itemtype = 17, stacksize = 20, stackable = 1, maxcharges = 1 WHERE id = 29125; -- Frost Giant Toes
