/* Ungate Diseased Bone Marrow(10264) drop from a_brittle_skeleton(56073) in steamfont */
UPDATE lootdrop_entries SET min_expansion = 1, max_expansion = 99 WHERE item_id = 10264;

/* Make the following weapons in Veeshan's Peak MAGIC items */
UPDATE items SET magic = 1 WHERE id = 32615; -- Fatebringer
UPDATE items SET magic = 1 WHERE id = 10844; -- Staff of Cancellation
UPDATE items SET magic = 1 WHERE id = 6601; -- Skullcrusher
UPDATE items SET magic = 1 WHERE id = 5604; -- Ykeshan Battle Axe
