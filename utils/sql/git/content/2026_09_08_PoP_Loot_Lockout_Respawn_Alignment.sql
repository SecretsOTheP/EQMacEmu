-- Keep Emmerik Skyfury and Evynd Firestorm on six-hour
-- respawn and character loot-lockout cycles.
UPDATE npc_types
SET loot_lockout = 21600
WHERE id IN (
    209053,
    209054
);

-- Standardize the remaining selected raid encounters to
-- 66-hour character loot lockouts.
UPDATE npc_types
SET loot_lockout = 237600
WHERE id IN (
    209026,
    220020,
    214026,
    212023,
    212025,
    212026,
    212014,
    212055,
    212046,
    212407,
    212408
);

-- Match their database-controlled respawns to 66 hours.
UPDATE spawn2
SET
    respawntime = 237600,
    variance = 0
WHERE id IN (
    360265,
    365500,
    361403,
    367544,
    367545,
    367546,
    367547,
    367548,
    367793,
    367794,
    367795,
    367796
);


-- Set the four elemental gods to five-day, 18-hour loot lockouts.
UPDATE npc_types
SET loot_lockout = 496800
WHERE id IN (
    215056, -- Xegony
    216048, -- Coirnav
    217440, -- Fennin Ro
    222040  -- Avatar of Earth
);

-- Match the database-controlled elemental encounter respawns.
-- Fennin is started by the Guardian of Doomfire.
UPDATE spawn2
SET
    respawntime = 496800,
    variance = 0
WHERE id IN (
    365346, -- Xegony
    365647, -- Coirnav
    367088  -- Guardian of Doomfire
);

-- Set additional PoP raid encounters to 66-hour loot lockouts.
UPDATE npc_types
SET loot_lockout = 237600
WHERE id IN (
    221041, -- Terris Thule
    207001, -- Saryrn
    205091, -- Grummus
    200226, -- Bertoxxulous
    214317  -- Vallon Zek
);

-- Match the database-controlled encounters and scripted triggers to 66 hours.
UPDATE spawn2
SET
    respawntime = 237600,
    variance = 0
WHERE id IN (
    365941, -- Terris Thule
    346762, -- Saryrn
    344762, -- Grummus
    360643, -- Spectre of Corruption, Bertoxxulous trigger
    369265  -- Vallon Zek trigger
);
