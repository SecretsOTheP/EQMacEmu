-- Make only the Plane of Power spell-conversion turn-ins stackable.
-- The companion quest guard accepts exactly one per turn-in.
UPDATE items
SET itemtype = 17, stackable = 1, stacksize = 20, maxcharges = 1
WHERE id IN (
    29112, -- Ethereal Parchment
    29131, -- Spectral Parchment
    29132  -- Glyphed Rune Word
);
