/* Note to SECRETS - 

Did my best to make this super easy to tweak and also show work on research.  At the bottom of the script, I put an analysis of all currently rechargable sky items and what batteries are used.

If you want to remove something from the script entirely, you can flip the "item_rechargable" from 1 to 0 and the insert will be skipped by if statement. 
If you want to change the battery used on an item, change the SET to use a diff variable  SET @rodofmalisementbattery = @manabattery2

In MySQL, IF statements cannot exist outside of stored procedures. Therefore, to create an idempotent migration for MySQL it's recommended to wrap the migration in a stored procedure and execute that stored procedure against the database to perform the migration. 
Procedure gets dropped at the end of the script */

DELIMITER $$


-- Create the stored procedure to perform the migration
CREATE PROCEDURE sp_makeitemsrechargable()

BEGIN

SET @boxofthevoid = '17100'; /* 1p */
SET @manabattery1 = '14800';  /* 10p */
SET @manabattery2 = '14801'; /* 20p */
SET @manabattery3 = '14802'; /* 30p */
SET @manabattery4 = '14803'; /* 40p */
SET @manabattery5 = '14804'; /* 50p */

/* I think I got all the no drop planar items; skipped raiment of thunder because of dps race nuke abuse, skipped planar wands (conflag, swords, allure)  you made no drop because I'm not sure what you want done with them -- original recharge on those was high (500p) and some are VERY powerful like allure and conflag */

/* Wiz 5 charge clarity from innoruuk - Mentioned as one of the items you wanted to make rechargable a few months back. Strong, so made it mana battery 5 */
SET @rodOfUnboundThought = '11572';
SET @rodOfUnboundThoughtRecipeID = '100000';
SET @rodOfUnboundThought_rechargable = 1;
SET @rodOfUnboundThoughtBattery = @manabattery5;

/* Clr 3 charge rez clicky from sky - Original version was rechargable in era using a class 2 battery, mostly invalidated by cleric epic in kunark */
SET @batonOfTheSky = '27719';
SET @batonOfTheSkyRecipeID = '100001';
SET @batonOfTheSky_rechargable = 1;
SET @batonOfTheSkyBattery = @manabattery2;

/* Mnk 2 charge haste clicky from sky - completely invalidated by monk epic in kunark, only 2 charges. Same spell/charges rechargable by Battery1 from Shm Wrist */
SET @sandalsOfAlacrity = '1280';
SET @sandalsOfAlacrityRecipeID = '100002';
SET @sandalsOfAlacrity_rechargable = 1;
SET @sandalsOfAlacrityBattery = @manabattery1;

/* Enc 7 charge mana sieve from Phinny - remains strong through velious easily. Mana Battery 5 */
SET @wandOfManaTapping = '10375';
SET @wandOfManaTappingRecipeID = '100003';
SET @wandOfManaTapping_rechargable = 1;
SET @wandOfManaTappingBattery = @manabattery5;

/* Shm 7 charge malaisement from Phinny - weak, mostly for tagging, lower level version of malosi. Mana Battery 1 */
SET @rodOfMalisement = '10374';
SET @rodOfMalisementRecipeID = '100004';
SET @rodOfMalisement_rechargable = 1;
SET @rodOfMalisementBattery = @manabattery1;

/* Brd 9 charge fury from sky - seems good for regen? Havent seen it used much. 9 charges is a lot. Set to battery 4 */
SET @mantleOfTheSongweaver = '27721';
SET @mantleOfTheSongweaverRecipeID = '100005';
SET @mantleOfTheSongweaver_rechargable = 1;
SET @mantleOfTheSongweaverBattery = @manabattery4;

/* Mag 10 charge malaise from sky - very weak, mostly for tagging, lowest level version of malosi. Mana Battery 1 */
SET @drakehideAmice = '2708';
SET @drakehideAmiceRecipeID = '100006';
SET @drakehideAmice_rechargable = 1;
SET @drakehideAmiceBattery = @manabattery1;

/* Wiz 4 charge bond of force from sky - reasonably strong even though lvl 51 snare is superior in every way. Set to Mana Battery 3 */
SET @alKaborCap = '1271';
SET @alKaborCapRecipeID = '100007';
SET @alKaborCap_rechargable = 1;
SET @alKaborCapBattery = @manabattery3;

/* Shm 2 charge pet from sky - very strong for mana regen via reclaim energy (common use on P99). Set to Mana Battery 5 */
SET @fairyhideMantle = '27728';
SET @fairyhideMantleRecipeID = '100008';
SET @fairyhideMantle_rechargable = 1;
SET @fairyhideMantleBattery = @manabattery5;



IF @rodOfUnboundThought_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@rodOfUnboundThoughtRecipeID, 'Rod of Unbound Thought', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@rodOfUnboundThoughtRecipeID * 10, @rodOfUnboundThoughtRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - Rod Of Unbound Thought from Inny
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@rodOfUnboundThoughtRecipeID * 10) + 1, @rodOfUnboundThoughtRecipeID, @rodOfUnboundThought, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@rodOfUnboundThoughtRecipeID * 10) + 2, @rodOfUnboundThoughtRecipeID, @rodOfUnboundThoughtBattery, '0', '0', '1', '0');


/*
Result 1 - 5 charge Rod of Unbound Thought
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@rodOfUnboundThoughtRecipeID * 10) + 3, @rodOfUnboundThoughtRecipeID, @rodOfUnboundThought, '5', '0', '0', '0');

END IF;

IF @batonOfTheSky_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@batonOfTheSkyRecipeID, 'Baton of the Sky', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@batonOfTheSkyRecipeID * 10, @batonOfTheSkyRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - Baton of Sky
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@batonOfTheSkyRecipeID * 10) + 1, @batonOfTheSkyRecipeID, @batonOfTheSky, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@batonOfTheSkyRecipeID * 10) + 2, @batonOfTheSkyRecipeID, @batonOfTheSkyBattery, '0', '0', '1', '0');


/*
Result 1 - 3 charge Baton of the Sky
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@batonOfTheSkyRecipeID * 10) + 3, @batonOfTheSkyRecipeID, @batonOfTheSky, '3', '0', '0', '0');

END IF;

IF @sandalsOfAlacrity_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@sandalsOfAlacrityRecipeID, 'Sandals of Alacrity', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@sandalsOfAlacrityRecipeID * 10, @sandalsOfAlacrityRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - Sandals of Alacrity
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@sandalsOfAlacrityRecipeID * 10) + 1, @sandalsOfAlacrityRecipeID, @sandalsOfAlacrity, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@sandalsOfAlacrityRecipeID * 10) + 2, @sandalsOfAlacrityRecipeID, @sandalsOfAlacrityBattery, '0', '0', '1', '0');


/*
Result 1 - 2 charge Sandals of Alacrity
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@sandalsOfAlacrityRecipeID * 10) + 3, @sandalsOfAlacrityRecipeID, @sandalsOfAlacrity, '2', '0', '0', '0');

END IF;

IF @wandOfManaTapping_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@wandOfManaTappingRecipeID, 'Wand of Mana Tapping', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@wandOfManaTappingRecipeID * 10, @wandOfManaTappingRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - mana tapping
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@wandOfManaTappingRecipeID * 10) + 1, @wandOfManaTappingRecipeID, @wandOfManaTapping, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@wandOfManaTappingRecipeID * 10) + 2, @wandOfManaTappingRecipeID, @wandOfManaTappingBattery, '0', '0', '1', '0');


/*
Result 1 - 7 charge wand of mana tapping
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@wandOfManaTappingRecipeID * 10) + 3, @wandOfManaTappingRecipeID, @wandOfManaTapping, '7', '0', '0', '0');

END IF;

IF @rodOfMalisement_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@rodOfMalisementRecipeID, 'Rod of Malisement', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@rodOfMalisementRecipeID * 10, @rodOfMalisementRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - rod of malisement
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@rodOfMalisementRecipeID * 10) + 1, @rodOfMalisementRecipeID, @rodOfMalisement, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@rodOfMalisementRecipeID * 10) + 2, @rodOfMalisementRecipeID, @rodOfMalisementBattery, '0', '0', '1', '0');


/*
Result 1 - 7 charge rod of malisement
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@rodOfMalisementRecipeID * 10) + 3, @rodOfMalisementRecipeID, @rodOfMalisement, '7', '0', '0', '0');

END IF;

IF @mantleOfTheSongweaver_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@mantleOfTheSongweaverRecipeID, 'Mantle of the Songweaver', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@mantleOfTheSongweaverRecipeID * 10, @mantleOfTheSongweaverRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - mantle of the songweaver
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@mantleOfTheSongweaverRecipeID * 10) + 1, @mantleOfTheSongweaverRecipeID, @mantleOfTheSongweaver, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@mantleOfTheSongweaverRecipeID * 10) + 2, @mantleOfTheSongweaverRecipeID, @mantleOfTheSongweaverBattery, '0', '0', '1', '0');


/*
Result 1 - 9 charge mantle of the songweaver
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@mantleOfTheSongweaverRecipeID * 10) + 3, @mantleOfTheSongweaverRecipeID, @mantleOfTheSongweaver, '9', '0', '0', '0');

END IF;

IF @drakehideAmice_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@drakehideAmiceRecipeID, 'Drake-hide Amice', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@drakehideAmiceRecipeID * 10, @drakehideAmiceRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - drake hide amice
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@drakehideAmiceRecipeID * 10) + 1, @drakehideAmiceRecipeID, @drakehideAmice, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@drakehideAmiceRecipeID * 10) + 2, @drakehideAmiceRecipeID, @drakehideAmiceBattery, '0', '0', '1', '0');


/*
Result 1 - 10 charge drake hide amice
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@drakehideAmiceRecipeID * 10) + 3, @drakehideAmiceRecipeID, @drakehideAmice, '10', '0', '0', '0');

END IF;

IF @alKaborCap_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@alKaborCapRecipeID, 'Al Kabor Cap of Binding', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@alKaborCapRecipeID * 10, @alKaborCapRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - Al Kabor Cap
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@alKaborCapRecipeID * 10) + 1, @alKaborCapRecipeID, @alKaborCap, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@alKaborCapRecipeID * 10) + 2, @alKaborCapRecipeID, @alKaborCapBattery, '0', '0', '1', '0');


/*
Result 1 - 4 Charge Al Kabor Cap of Binding
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@alKaborCapRecipeID * 10) + 3, @alKaborCapRecipeID, @alKaborCap, '4', '0', '0', '0');

END IF;

IF @fairyhideMantle_rechargable = 1 
THEN 
insert into tradeskill_recipe
(id, name, tradeskill, skillneeded, trivial, nofail, replace_container, notes, quest, enabled)
values
(@fairyhideMantleRecipeID, 'Fairy-Hide Mantle', '75', '0', '0', '1', '1', NULL, '0', '1');


/*
Box Of the Void - isContainer 1
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
(@fairyhideMantleRecipeID * 10, @fairyhideMantleRecipeID, @boxofthevoid, '0', '0', '0', '1');

/*
Component 1 - Fairy-Hide Mantle
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@fairyhideMantleRecipeID * 10) + 1, @fairyhideMantleRecipeID, @fairyhideMantle, '0', '0', '1', '0');

/*
Component 2 - Mana Battery
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@fairyhideMantleRecipeID * 10) + 2, @fairyhideMantleRecipeID, @fairyhideMantleBattery, '0', '0', '1', '0');


/*
Result 1 - 2 Charge fairyhideMantle
*/
insert into tradeskill_recipe_entries
(id, recipe_id, item_id, successcount, failcount, componentcount, iscontainer)
values
((@fairyhideMantleRecipeID * 10) + 3, @fairyhideMantleRecipeID, @fairyhideMantle, '2', '0', '0', '0');

END IF;

END $$




/* Research:

Mana Battery 1 existing examples: 
mage mask burnout (5 charges, good in classic, invalidated in kunark) 
necro 10 charge levi ring (pretty useless?)
wiz 3 charge rune ring (very powerful actually for ripping trains due to high rune aggro)
shaman 2 charges alacrity - mostly invalidated in kunark due to celerity

select distinct i.Name from tradeskill_recipe_entries tce_battery
inner join tradeskill_recipe_entries tce_item on tce_battery.recipe_id = tce_item.recipe_id and tce_item.item_id <> tce_battery.item_id and tce_item.iscontainer = 0
inner join items i on tce_item.item_id = i.id
where tce_battery.item_id = '14800'

Mana Battery 2 existing examples:
cleric 3 charge rez stick (strong in classic, niche use after that. mostly invalidated by cleric epic)
ench 2 charge charm neck (niche use, strong on p99 in very very specific scenarios that likely wont be a thing here)
ench 2 charge grav flux earring  (niche use, ultra powerful in p99 in very specific FTE race scenarios that likely wont be a thing here)
bard 4 charge heal neck (might be useful for ripping trains maybe considering heal aggro here? otherwise kinda hot garbage)

select distinct i.Name from tradeskill_recipe_entries tce_battery
inner join tradeskill_recipe_entries tce_item on tce_battery.recipe_id = tce_item.recipe_id and tce_item.item_id <> tce_battery.item_id and tce_item.iscontainer = 0
inner join items i on tce_item.item_id = i.id
where tce_battery.item_id = '14801'

Mana Battery 3 existing examples:
2 charge shaman 44 pet (very useful because shamans can reclaim energy from mage shovel) Seems to be missing from TAKP, but exists on P99
4 charge ench rune staff (maybe useful for ripping trains similar to wiz ring? never seeen it used)
3 charge spirit of cheetah war shoulders (ultra useful on P99 for racing in specific scenarios -- warrior bis racing in plane of fear lol)


select distinct i.Name from tradeskill_recipe_entries tce_battery
inner join tradeskill_recipe_entries tce_item on tce_battery.recipe_id = tce_item.recipe_id and tce_item.item_id <> tce_battery.item_id and tce_item.iscontainer = 0
inner join items i on tce_item.item_id = i.id
where tce_battery.item_id = '14802'

Mana Battery 4 existing examples:
2 charge ranger earthquake shoulders (niche use for farming quillmane by ae'ing down all PH's)
3 charge necro invoke fear ring (strong)
1 charge CH neck cleric (god tier)


select distinct i.Name from tradeskill_recipe_entries tce_battery
inner join tradeskill_recipe_entries tce_item on tce_battery.recipe_id = tce_item.recipe_id and tce_item.item_id <> tce_battery.item_id and tce_item.iscontainer = 0
inner join items i on tce_item.item_id = i.id
where tce_battery.item_id = '14803'

Mana Battery 5 existing examples:
1 charge mage DA ring (god tier)
1 charge wiz insta sky evac (god tier)
2 charge shaman FD ring (god tier)
10 charge rog lev shoulders (crap)

select distinct i.Name from tradeskill_recipe_entries tce_battery
inner join tradeskill_recipe_entries tce_item on tce_battery.recipe_id = tce_item.recipe_id and tce_item.item_id <> tce_battery.item_id and tce_item.iscontainer = 0
inner join items i on tce_item.item_id = i.id
where tce_battery.item_id = '14804'

*/
 
DELIMITER ;




