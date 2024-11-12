-- Add bread cakes recipe

INSERT INTO `tradeskill_recipe` (`name`, `tradeskill`, `skillneeded`, `trivial`, `nofail`, `replace_container`, `notes`, `quest`, `enabled`, `min_expansion`, `max_expansion`, `content_flags`, `content_flags_disabled`) 
VALUES 
('Bread Cakes', 60, 0, 21, 0, 0, NULL, 0, 1, -1, -1, NULL, NULL);

SELECT ID 
INTO @breadcakesid
FROM tradeskill_recipe
WHERE name = 'Bread Cakes';

INSERT INTO `tradeskill_recipe_entries` (`recipe_id`, `item_id`, `successcount`, `failcount`, `componentcount`, `iscontainer`) 
VALUES 
-- Containers
( @breadcakesid, 17947, 0, 0, 0, 1),
( @breadcakesid, 17164, 0, 0, 0, 1),
( @breadcakesid, 15, 0, 0, 0, 1),
-- Successful items
-- Muffin Tin
( @breadcakesid, 13437, 1, 0, 0, 0),
-- Bread cakes
( @breadcakesid, 9991, 4, 0, 0, 0),
-- Requirements
-- Clump of dough
( @breadcakesid, 13090, 0, 0, 1, 0),
-- Muffin tin comes back on both success and fail
( @breadcakesid, 13437, 0, 1, 1, 0);

-- Add Skin of Milk recipe

INSERT INTO `tradeskill_recipe` (`name`, `tradeskill`, `skillneeded`, `trivial`, `nofail`, `replace_container`, `notes`, `quest`, `enabled`, `min_expansion`, `max_expansion`, `content_flags`, `content_flags_disabled`) 
VALUES 
('Skin of Milk', 60, 0, 21, 0, 0, NULL, 0, 1, -1, -1, NULL, NULL);

SELECT ID 
INTO @milkid
FROM tradeskill_recipe
WHERE name = 'Skin of Milk';

INSERT INTO `tradeskill_recipe_entries` (`recipe_id`, `item_id`, `successcount`, `failcount`, `componentcount`, `iscontainer`) 
VALUES 
-- Containers
( @milkid, 17906, 0, 0, 0, 1),
( @milkid, 17162, 0, 0, 0, 1),
-- Successful items
-- Bottle
( @milkid, 16598, 1, 0, 0, 0),
-- Skin of Milk
( @milkid, 9990, 2, 0, 0, 0),
-- Requirements
-- Bottle of milk
( @milkid, 13087, 0, 0, 1, 0),
-- Ruined Wolf Pelt
( @milkid, 13782, 0, 1, 1, 0);
