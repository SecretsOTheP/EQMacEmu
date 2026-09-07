-- Keep the complete Agnarr tower event out of open-world Bastion of Thunder.
-- Scripted lieutenants, temporary adds, later Askr incarnations, and Karana
-- have no independent spawn points and can only be created by this event.
UPDATE `spawn2`
SET `raid_target_spawnpoint` = 1
WHERE `id` IN (
    -- Tower entrance
    360600, -- Askr the Lost

    -- First level: Evynd Firestorm
    360330, -- storm guardian
    360331, -- storm guardian
    360332, -- storm guardian
    360333, -- firestorm portal
    360334, -- firestorm portal
    360335, -- firestorm portal
    360414, -- firestorm elemental
    360517, -- firestorm elemental
    360602, -- Evynd Firestorm

    -- Second level: Emmerik Skyfury
    360313, -- celestial portal
    360322, -- storm guardian
    360323, -- storm guardian
    360324, -- storm guardian
    360326, -- celestial portal
    360327, -- celestial portal
    360601, -- Emmerik Skyfury

    -- Third level: Agnarr the Storm Lord
    360317, -- untargetable storm portal
    360318, -- storm portal
    360319, -- storm portal
    360320, -- storm portal
    360321, -- storm portal
    360265, -- Agnarr the Storm Lord

    -- Post-event Karana controllers
    360277, -- event timer
    360611  -- Karana trigger
);

-- Classify the four Lua-spawned lieutenants as raid loot without giving that status
-- to portals, guardians, or temporary trash.
UPDATE `npc_types`
SET `raid_target` = 1
WHERE `id` IN (209142, 209146, 209147, 209148);
