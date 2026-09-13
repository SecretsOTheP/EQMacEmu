-- Elemental Plane essence drops are guaranteed from their respective bosses.
-- Each listed lootdrop contains only its matching Essence entry.
UPDATE lootdrop_entries
SET chance = 100
WHERE lootdrop_id IN (
    90126,  -- Essence of Wind (Xegony)
    23569,  -- Essence of Fire (Fennin Ro)
    8176,   -- Essence of Water (Coirnav)
    115539  -- Essence of Earth (Avatar of Earth)
);
