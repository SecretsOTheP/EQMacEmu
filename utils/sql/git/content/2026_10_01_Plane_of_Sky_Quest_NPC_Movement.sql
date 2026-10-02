-- These quest NPC types are shared by airplane and air_instanced. Their zero
-- run and walk speeds leave them stationary even when they enter combat.
-- Key Master (71056) is a merchant and is deliberately excluded.
UPDATE npc_types
SET runspeed = 1.25,
    walkspeed = 0.5
WHERE id IN (
    71037, 71042, 71043, 71044, 71046, 71047, 71048,
    71049, 71050, 71051, 71052, 71053, 71054
)
  AND runspeed = 0
  AND walkspeed = 0;
