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
