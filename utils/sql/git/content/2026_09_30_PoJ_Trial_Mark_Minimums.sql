-- Plane of Justice trial Marks: make the single lootdrop roll explicit.
-- Each Mark keeps its existing six-copy lootdrop entry multiplier.
UPDATE loottable_entries
SET droplimit = 1,
    mindrop = 1
WHERE (loottable_id, lootdrop_id) IN (
    (96847, 115276), -- Hanging: Mark of Suffocation
    (96849, 115278), -- Lashing: Mark of Lashing
    (96856, 115285), -- Torture: Mark of Torture
    (96861, 115291)  -- Stoning: Mark of Stone
)
  AND droplimit = 0
  AND mindrop = 0;
