-- Remove the faction from Verina Tomb so that she is not KOS to players.
-- Resolves issue with Human clerics of innoruuk being KOS and dying while trying to get spells and train
UPDATE npc_types
SET npc_faction_id = 0
WHERE name = "Verina_Tomb";

-- Helpful query to find the npc_faction_id of a specific NPC:

-- SELECT id, name, npc_faction_id
-- FROM npc_types
-- WHERE id = 42112;
