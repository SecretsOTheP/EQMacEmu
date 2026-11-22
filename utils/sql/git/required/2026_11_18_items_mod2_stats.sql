-- PoP-style "mod2" item stats. Values ride in spare bytes of the client item struct
-- (0x128-0x12F) and are applied server-side through the normal item bonus path.
ALTER TABLE `items`
  ADD COLUMN IF NOT EXISTS `strikethrough` tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `stunresist`    tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `combateffects` tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `accuracy`      tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `avoidance`     tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `spellshield`   tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `shielding`     tinyint(4) NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `damageshield`  tinyint(4) NOT NULL DEFAULT 0;

UPDATE `items` SET `strikethrough` = 5 WHERE `id` = 27990; -- Gauntlets of Disruption (P2 - Minis)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 21994; -- Protective Sleeves (P3 - Minis)
UPDATE `items` SET `combateffects` = 5 WHERE `id` = 5760; -- Earring of Xaoth Kor (P3 - Rockmen)
UPDATE `items` SET `accuracy` = 10 WHERE `id` = 29174; -- Mask of Strategic Insight (P3 - Rockmen)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 20898; -- Symbol of the Planemasters (P4 - Saryrn)
UPDATE `items` SET `avoidance` = 10 WHERE `id` = 26994; -- Ring of Evasion (P4 - Saryrn)
UPDATE `items` SET `spellshield` = 4 WHERE `id` = 26996; -- Gloves of the Unseen (P4 - Saryrn)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 28667; -- Bracer of Precision (P4 - Tallon Zek)
UPDATE `items` SET `avoidance` = 10 WHERE `id` = 15815; -- Cloak of the Falling Skies (P4 - Tallon Zek)
UPDATE `items` SET `strikethrough` = 5 WHERE `id` = 23498; -- Hopebringer (P4 - Tallon Zek)
UPDATE `items` SET `combateffects` = 10 WHERE `id` = 26991; -- Band of Prismatic Focus (P4 - Tallon Zek)
UPDATE `items` SET `accuracy` = 15 WHERE `id` = 28658; -- Mantle of Deadly Precision (P4 - Tallon Zek)
UPDATE `items` SET `spellshield` = 4 WHERE `id` = 26988; -- Winged Storm Boots (P4 - Tallon Zek)
UPDATE `items` SET `spellshield` = 5 WHERE `id` = 28656; -- Tactician's Shield (P4 - Tallon Zek)
UPDATE `items` SET `strikethrough` = 5 WHERE `id` = 22890; -- Cudgel of Venomous Hatred (P4 - Terris Thule)
UPDATE `items` SET `spellshield` = 4 WHERE `id` = 21995; -- Armguards of the Brute (P4 - Terris Thule)
UPDATE `items` SET `avoidance` = 5 WHERE `id` = 26740; -- Shinai of the Ancients (P4 - Vallon Zek)
UPDATE `items` SET `avoidance` = 10 WHERE `id` = 26799; -- Globe of Mystical Protection (P4 - Vallon Zek)
UPDATE `items` SET `spellshield` = 4 WHERE `id` = 26986; -- Shoes of Fleeting Fury (P4 - Vallon Zek)
UPDATE `items` SET `shielding` = 2 WHERE `id` = 8977; -- Collar of Catastrophe (P5 - Bertoxxulous)
UPDATE `items` SET `shielding` = 2, `avoidance` = 10 WHERE `id` = 28651; -- Leggings of Furious Might (P5 - Bertoxxulous)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 25995; -- Boots of Despair (P5 - Bertoxxulous)
UPDATE `items` SET `accuracy` = 5 WHERE `id` = 24573; -- Timespinner, Blade of the Hunter (P5 - Bertoxxulous)
UPDATE `items` SET `spellshield` = 5 WHERE `id` = 15927; -- Belt of Temporal Bindings (P5 - Bertoxxulous)
UPDATE `items` SET `stunresist` = 4 WHERE `id` = 26980; -- Earring of Unseen Horrors (P5 - Cazic Thule)
UPDATE `items` SET `shielding` = 4 WHERE `id` = 28650; -- Greaves of Furious Might (P5 - Cazic Thule)
UPDATE `items` SET `strikethrough` = 5 WHERE `id` = 10948; -- Timestone Adorned Ring (P5 - Cazic Thule)
UPDATE `items` SET `strikethrough` = 10 WHERE `id` = 27000; -- Padded Tigerskin Gloves (P5 - Cazic Thule)
UPDATE `items` SET `shielding` = 2 WHERE `id` = 9289; -- Necklace of Eternal Visions (P5 - Innoruuk)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 31603; -- Girdle of Stability (P5 - Innoruuk)
UPDATE `items` SET `strikethrough` = 10 WHERE `id` = 28246; -- Gloves of Airy Mists (P5 - Innoruuk)
UPDATE `items` SET `accuracy` = 5 WHERE `id` = 24692; -- Jagged Timeforged Blade (P5 - Innoruuk)
UPDATE `items` SET `accuracy` = 8 WHERE `id` = 13674; -- Shroud of Survival (P5 - Innoruuk)
UPDATE `items` SET `shielding` = 3 WHERE `id` = 27298; -- Shield of Strife (P5 - Rallos Zek)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 32610; -- Sandals of Empowerment (P5 - Rallos Zek)
UPDATE `items` SET `avoidance` = 10 WHERE `id` = 15824; -- Platinum Cloak of War (P5 - Rallos Zek)
UPDATE `items` SET `strikethrough` = 5 WHERE `id` = 22999; -- Darkblade of the Warlord (P5 - Rallos Zek)
UPDATE `items` SET `avoidance` = 25 WHERE `id` = 28652; -- Pants of Furious Might (P5 - Rallos Zek)
UPDATE `items` SET `shielding` = 4, `damageshield` = 5 WHERE `id` = 15805; -- Shroud of Eternity (P6 - Quarm)
UPDATE `items` SET `stunresist` = 4, `accuracy` = 15 WHERE `id` = 21993; -- Bracer of the Inferno (P6 - Quarm)
UPDATE `items` SET `shielding` = 2, `stunresist` = 4 WHERE `id` = 32100; -- Earthen Bracer of Fortitude (P6 - Quarm)
UPDATE `items` SET `stunresist` = 5, `avoidance` = 20 WHERE `id` = 28649; -- Ethereal Silk Leggings (P6 - Quarm)
UPDATE `items` SET `stunresist` = 5 WHERE `id` = 27280; -- Whorl of Unnatural Forces (P6 - Quarm)
UPDATE `items` SET `avoidance` = 15, `accuracy` = 15 WHERE `id` = 32110; -- Wristband of Icy Vengeance (P6 - Quarm)
UPDATE `items` SET `strikethrough` = 10 WHERE `id` = 16156; -- Cord of Temporal Weavings (P6 - Quarm)
UPDATE `items` SET `combateffects` = 15 WHERE `id` = 28661; -- Shawl of Eternal Forces (P6 - Quarm)
UPDATE `items` SET `combateffects` = 15 WHERE `id` = 7123; -- Silver Hoop of Speed (P6 - Quarm)
UPDATE `items` SET `spellshield` = 6 WHERE `id` = 26989; -- Prismatic Ring of Resistance (P6 - Quarm)
UPDATE `items` SET `shielding` = 2 WHERE `id` = 32602; -- Grimror's Guard of the Plagues (Time Class BP)
UPDATE `items` SET `shielding` = 2 WHERE `id` = 32132; -- Trydan's Chestplate of Nobility (Time Class BP)
UPDATE `items` SET `shielding` = 3 WHERE `id` = 32129; -- Raex's Chestplate of Destruction (Time Class BP)
UPDATE `items` SET `stunresist` = 7 WHERE `id` = 32134; -- Rizlona's Fiery Chestplate (Time Class BP)
UPDATE `items` SET `stunresist` = 7 WHERE `id` = 32603; -- Ultor's Chestguard of Faith (Time Class BP)
UPDATE `items` SET `stunresist` = 7 WHERE `id` = 32139; -- Kerasha's Sylvan Tunic (Time Class BP)
UPDATE `items` SET `stunresist` = 7 WHERE `id` = 32138; -- Rosrak's Hauberk of the Primal (Time Class BP)
UPDATE `items` SET `avoidance` = 15 WHERE `id` = 32131; -- Ton Po's Chestwraps of Composure (Time Class BP)
UPDATE `items` SET `avoidance` = 15 WHERE `id` = 32136; -- Dumul's Chestwraps of the Brute (Time Class BP)
UPDATE `items` SET `accuracy` = 15 WHERE `id` = 32130; -- Bidilis' Hauberk of the Elusive (Time Class BP)
UPDATE `items` SET `accuracy` = 15 WHERE `id` = 32604; -- Askr's Thunderous Chainmail (Time Class BP)
UPDATE `items` SET `spellshield` = 6 WHERE `id` = 32142; -- Miragul's Shroud of Risen Souls (Time Class BP)
UPDATE `items` SET `spellshield` = 6 WHERE `id` = 32143; -- Maelin's Robe of Lore (Time Class BP)
UPDATE `items` SET `spellshield` = 6 WHERE `id` = 32141; -- Magi`Kot's Robe of Convergence (Time Class BP)
UPDATE `items` SET `spellshield` = 6 WHERE `id` = 32140; -- Romar's Robe of Visions (Time Class BP)

-- Shroud of Eternity: its Chromatic Chaos worn effect (DS 5 + HP regen 5) stood in for the damage shield,
-- which is now the mod2 `damageshield` above. Swap it for Regeneration (144, HP regen 5 only) to keep the regen.
-- Other worn effects (Vengeance ATK, Auras, Corporeal Fury) are ATK/regen, not mod2, and are left alone.
UPDATE `items` SET `worneffect` = 144, `worntype` = 2, `wornlevel` = 0, `wornlevel2` = 0
WHERE `id` = 15805 AND `worneffect` = 771;
