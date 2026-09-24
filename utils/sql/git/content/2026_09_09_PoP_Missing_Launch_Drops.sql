-- Add missing Plane of Power launch-era drops as independent 10% drops.

INSERT INTO lootdrop (
    id,
    name,
    min_expansion,
    max_expansion,
    content_flags,
    content_flags_disabled
)
VALUES
    (6150535, 'PoTime_Cazic_Thule_Bo_Staff_of_Transcendence', -1, -1, NULL, NULL),
    (6150536, 'Mujaki_Recurved_Wormwood_Bow', -1, -1, NULL, NULL),
    (6150537, 'Avatar_of_Smoke_Alabaster_Hilted_Wind_Bow', -1, -1, NULL, NULL),
    (6150538, 'Krziik_Ornate_Abalone_Recurve_Bow', -1, -1, NULL, NULL);

INSERT INTO lootdrop_entries (
    lootdrop_id,
    item_id,
    item_charges,
    equip_item,
    chance,
    minlevel,
    maxlevel,
    multiplier,
    disabled_chance,
    min_expansion,
    max_expansion,
    min_looter_level,
    item_loot_lockout_timer,
    content_flags_disabled,
    content_flags
)
VALUES
    (6150535, 25225, 1, 0, 100, 0, 255, 1, 0, -1, -1, 0, 0, NULL, NULL),
    (6150536, 27700, 1, 0, 100, 0, 255, 1, 0, -1, -1, 0, 0, NULL, NULL),
    (6150537, 27986, 1, 0, 100, 0, 255, 1, 0, -1, -1, 0, 0, NULL, NULL),
    (6150538, 27646, 1, 0, 100, 0, 255, 1, 0, -1, -1, 0, 0, NULL, NULL);

INSERT INTO loottable_entries (
    loottable_id,
    lootdrop_id,
    multiplier,
    probability,
    droplimit,
    mindrop,
    multiplier_min
)
VALUES
    (4453,  6150535, 1, 10, 0, 0, 0), -- Cazic Thule
    (2064,  6150536, 1, 10, 0, 0, 0), -- Mujaki the Devourer
    (87587, 6150537, 1, 10, 0, 0, 0), -- Avatar of Smoke
    (6222,  6150538, 1, 10, 0, 0, 0); -- Krziik the Mighty
