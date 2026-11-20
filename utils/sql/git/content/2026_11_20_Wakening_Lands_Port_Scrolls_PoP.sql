-- Manual content SQL: keep the Wakening Lands port scrolls available in Planes of Power.
--
-- These five scrolls are the only items with the window min_expansion 2 / max_expansion 4.
-- max_expansion is exclusive (SharedDatabase::GetItem: current >= min AND current < max), so at
-- World:CurrentExpansion 4.0 the items stop existing: vendors no longer list them and they no
-- longer drop. Every other Velious-era item uses 2 / 5.
--
-- Safe to rerun; only rows still on the 2 / 4 window are changed.
UPDATE `items`
SET `max_expansion` = 5
WHERE `id` IN (19511, 19514, 19517, 30398, 30399)
  AND `min_expansion` = 2
  AND `max_expansion` = 4;

-- 19511  Spell: Translocate: Wakening Lands   (wizard)
-- 19514  Spell: Wakening Lands Gate           (wizard)
-- 30399  Spell: Wakening Lands Portal         (wizard)
-- 19517  Spell: Ring of Wakening Lands        (druid)
-- 30398  Spell: Circle of Wakening Lands      (druid)
