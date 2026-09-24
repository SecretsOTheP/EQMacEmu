-- Plane of Time manages defeated bosses and lootless repeats in its instance
-- controller. Generic NPC loot lockouts can eject players who must repeat a
-- completed encounter while recovering a partially finished phase.
-- Keep generic lockouts disabled and let the controller own Time progression.
UPDATE `npc_types`
SET `loot_lockout` = 0
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
