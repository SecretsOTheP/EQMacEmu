-- Adds Velious 'custom helmet' graphics to some Velious helms that have colors defined, but were missing a helmet graphic.
-- Adds more fashion variety on chase items and a few new colors to choose from.

-- 'Helmet of Rallos Zek' (Avatar of War). (Color: Default)
-- Note: IT241 is a special ID that is a 'Rallos Zek'-style helmet when ogres equip it (looks like a regular Custom Helm on other races).
UPDATE items set idfile = 'IT241' WHERE id = 25209;

-- 'Imbued Royal Velium Helmet' (No Drop, blacksmithing, part of Shawl 8). This is a full fashion set but helm had no graphic. (Color 10/20/100)
UPDATE items set idfile = 'IT240' WHERE id = 9281;

-- 'Royal Velium Frosted Helmet' (No drop, from Icewell keep guards). This is a full fashion set but helm had no graphic. (Color: 0/100/125)
UPDATE items set idfile = 'IT240' WHERE id = 25850;

-- 'Helm of the Deep Sea' (No drop, Lord Koi`Doken). Missing graphic. (Color: 0/50/0)
UPDATE items set idfile = 'IT240' WHERE id = 31311; 

-- PoM Class Crowns
UPDATE items set idfile = 'IT240' WHERE id = 6845; -- Mischievous Dazzler Crown (Color: 80/0/180)
UPDATE items set idfile = 'IT240' WHERE id = 6852; -- Maleficent Crown (Color: 0/250/250)
UPDATE items set idfile = 'IT240' WHERE id = 6859; -- Wily Warlock Crown (Color: 50/0/150)
UPDATE items set idfile = 'IT240' WHERE id = 6866; -- Sly Summoner's Crown (Color: 0/250/250)
