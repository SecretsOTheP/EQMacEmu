-- Keep the actual Plane of Power raid reward NPCs on the standard 66-hour loot lockout.
-- Event starters and non-loot trigger NPCs are deliberately not included.
UPDATE npc_types
SET loot_lockout = 237600
WHERE id IN (
    206046, -- Manaetic Behemoth
    206208, -- Xanamech Nezmirthafen
    218375, -- A Mystical Arbitor of Earth
    200245, -- High Priest Ultor Szanvon
    215060, -- Lossenmachar (Dust event boss)
    215390, -- Pherlondien Clawpike (Wind event boss)
    215396, -- An Elemental Masterpiece (Smoke event boss)
    215399, -- Melernil Faal`Armanna (Mist event boss)
    215391, -- Avatar of Wind
    215392, -- Avatar of Smoke
    215393, -- Avatar of Mist
    215394  -- Avatar of Dust
);
