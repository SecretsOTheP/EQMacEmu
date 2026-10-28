-- Plane of Earth trap NPCs (TrapMan1-13, invisible trigger NPCs that spawn mobs when a
-- player enters aggro range) could not see improved hide, so a rogue under Shroud of
-- Stealth walked past them without triggering. Let them see improved hide.
UPDATE `npc_types`
SET `see_improved_hide` = 1
WHERE `id` IN (218341, 218399, 218400, 218401, 218402, 218403, 218404, 218405, 218406, 218407, 218408, 218409, 218410)
AND `name` LIKE 'TrapMan%';
