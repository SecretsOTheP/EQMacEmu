/* Create new recipe row */
INSERT INTO tradeskill_recipe (name, tradeskill, nofail, replace_container) VALUES ('Larrikan\'s Mask', 75, 1, 1);

SET @recipe_id = LAST_INSERT_ID();

/* Box of the Void */
INSERT INTO tradeskill_recipe_entries (recipe_id, item_id, successcount, failcount, componentcount, iscontainer) VALUES (@recipe_id, 17100, 0, 0, 0, 1);
/* Larrikans Mask component */
INSERT INTO tradeskill_recipe_entries (recipe_id, item_id, successcount, failcount, componentcount, iscontainer) VALUES (@recipe_id, 2736, 0, 0, 1, 0);
/* Class 3 Mana Battery */
INSERT INTO tradeskill_recipe_entries (recipe_id, item_id, successcount, failcount, componentcount, iscontainer) VALUES (@recipe_id, 14802, 0, 0, 1, 0);
/* Larrikans Mask recharged */
INSERT INTO tradeskill_recipe_entries (recipe_id, item_id, successcount, failcount, componentcount, iscontainer) VALUES (@recipe_id, 2736, 10, 0, 0, 0);
