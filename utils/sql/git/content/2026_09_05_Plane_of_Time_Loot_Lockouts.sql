-- Plane of Time bosses share the 2 day 18 hour progression lockout.
-- Controllers, event adds, the fake Vallon Zek, Zebuxoruk, and projections
-- are intentionally excluded.
UPDATE `npc_types`
SET `loot_lockout` = 237600
WHERE `id` IN (
    223018, 223029, 223032, 223037, 223044,
    223072, 223073, 223074, 223075, 223076,
    223083, 223084, 223090, 223091, 223096, 223097,
    223101, 223105, 223108, 223109, 223115, 223116,
    223123, 223124, 223131, 223132, 223133, 223134,
    223000, 223001, 223002, 223003,
    223004, 223005, 223006, 223007,
    223008
);
