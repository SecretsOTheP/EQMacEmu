-- The original Timekeeper migration selected a template NPC name that is absent
-- from the database. Its spawn records exist, but npc_types.id 7151200 does not.
-- Restore the level-65 noncombat NPC used in the September test realm.
INSERT INTO `npc_types` (
    `id`, `name`, `level`, `race`, `class`, `hp`, `mana`,
    `gender`, `texture`, `size`, `face`, `isquest`
)
SELECT
    7151200, 'Timekeeper_of_Druzzil_Ro', 65, 1, 1, 100000, 100000,
    1, 15, 10, 0, 1
WHERE NOT EXISTS (
    SELECT 1 FROM `npc_types` WHERE `id` = 7151200
);
