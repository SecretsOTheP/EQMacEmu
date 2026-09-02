-- Protect both Plane of Innovation versions of Giwin Mirakon.
-- Preserve existing abilities, including #Giwin's corpse-camper flag.
-- The Plane of Tactics version (214014) is deliberately unchanged.

-- Melee immunity.
UPDATE `npc_types`
SET `special_abilities` = CONCAT_WS('^', NULLIF(`special_abilities`, ''), '19,1')
WHERE `id` IN (206038, 206203)
  AND CONCAT('^', COALESCE(`special_abilities`, '')) NOT LIKE '%^19,%';

-- Magic immunity.
UPDATE `npc_types`
SET `special_abilities` = CONCAT_WS('^', NULLIF(`special_abilities`, ''), '20,1')
WHERE `id` IN (206038, 206203)
  AND CONCAT('^', COALESCE(`special_abilities`, '')) NOT LIKE '%^20,%';

-- Does not initiate aggro.
UPDATE `npc_types`
SET `special_abilities` = CONCAT_WS('^', NULLIF(`special_abilities`, ''), '24,1')
WHERE `id` IN (206038, 206203)
  AND CONCAT('^', COALESCE(`special_abilities`, '')) NOT LIKE '%^24,%';

-- Cannot be aggroed by nearby NPCs.
UPDATE `npc_types`
SET `special_abilities` = CONCAT_WS('^', NULLIF(`special_abilities`, ''), '25,1')
WHERE `id` IN (206038, 206203)
  AND CONCAT('^', COALESCE(`special_abilities`, '')) NOT LIKE '%^25,%';

-- Immune to harm from players.
UPDATE `npc_types`
SET `special_abilities` = CONCAT_WS('^', NULLIF(`special_abilities`, ''), '35,1')
WHERE `id` IN (206038, 206203)
  AND CONCAT('^', COALESCE(`special_abilities`, '')) NOT LIKE '%^35,%';
