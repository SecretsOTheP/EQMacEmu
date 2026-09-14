-- Align the remaining named Plane of Air minibosses to the 66-hour raid cycle.
UPDATE npc_types
SET loot_lockout = 237600
WHERE id IN (
    215003, -- Queen Silandria
    215011, -- Arch Mage Alchtonion
    215024, -- Gakamenial Fir`Disralsi
    215034, -- Rinturion Windblade
    215054, -- Baltaldor the Cursed
    215375  -- Sigismond Windwalker
);

UPDATE spawn2
SET
    respawntime = 237600,
    variance = 0
WHERE id IN (
    365638, -- Baltaldor the Cursed
    365750, -- Arch Mage Alchtonion
    366074, -- Queen Silandria
    366131, -- Sigismond Windwalker
    366212, -- Rinturion Windblade
    367322  -- Gakamenial Fir`Disralsi
);

-- The final loot-bearing NPCs for these scripted or staged encounters use
-- normal character loot lockouts. Their controllers only govern event flow.
UPDATE npc_types
SET loot_lockout = 237600
WHERE id IN (
    201074, -- The Seventh Hammer
    208074, -- Aerin`Dar
    214312, -- Rallos Zek the Warlord
    214316  -- Vallon Zek
);

UPDATE spawn2
SET
    respawntime = 237600,
    variance = 0
WHERE id IN (
    360642, -- Carprin event controller
    237600, -- The Seventh Hammer
    347257, -- Aerin`Dar
    361379, -- Rallos Zek event controller
    369265  -- Vallon Zek
);

-- Keeper of Sorrows is the Tylis event starter, not a raid target. Keep its
-- event respawn aligned with Tylis and remove its raid-window classification.
UPDATE spawn2
SET
    respawntime = 7200,
    variance = 0,
    raid_target_spawnpoint = 0
WHERE id = 346764;

-- Guardian of Coirnav starts the event but is not its loot boss. Keep it
-- loot-lockout free and match Guardian of Doomfire's post-success respawn.
-- The Coirnav event script still overrides the Coirnav retry to ten minutes
-- after a failed attempt.
UPDATE npc_types
SET loot_lockout = 0
WHERE id = 216053;

UPDATE spawn2
SET respawntime = 496800, variance = 0
WHERE id = 366321;
