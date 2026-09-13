-- Plane of Fire named raid targets use the standard 66-hour respawn and loot lockout.
UPDATE npc_types
SET loot_lockout = 237600
WHERE id IN (
    217019, -- Arch Mage Yozanni
    217005, -- Babnoxis the Spider Queen
    217003, -- Blazzax the Omnifiend
    217051, -- Criare Sunmane
    217036, -- General Druav Flamesinger
    217032, -- General Reparm
    217049, -- Jaxoliz Dawneyes
    217059, -- Magmaton
    217063, -- Pyronis
    217056  -- Quavonis Firetail
);

UPDATE spawn2
SET
    respawntime = 237600,
    variance = 0
WHERE id IN (
    366356, -- Arch Mage Yozanni
    367063, -- Babnoxis the Spider Queen
    367351, -- Blazzax the Omnifiend
    366652, -- Criare Sunmane
    367462, -- General Druav Flamesinger
    366505, -- General Reparm
    367325, -- Jaxoliz Dawneyes
    366567, -- Magmaton
    367421, -- Pyronis
    366735  -- Quavonis Firetail
);

-- Keeper of Sorrows and Tylis are paired event starters; reset both after two hours.
UPDATE spawn2
SET
    respawntime = 7200,
    variance = 0
WHERE id IN (
    346764, -- The Keeper of Sorrows
    346874  -- Tylis Newleaf
);
