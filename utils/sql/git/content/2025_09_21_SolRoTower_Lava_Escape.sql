-- Sol Ro Tower lava recovery portal.
-- A large Plane of Knowledge-style portal door sends fallen players to Plane of Tranquility.

INSERT INTO doors (
    doorid,
    zone,
    name,
    pos_y,
    pos_x,
    pos_z,
    heading,
    opentype,
    lockpick,
    keyitem,
    altkeyitem,
    nokeyring,
    triggerdoor,
    triggertype,
    doorisopen,
    door_param,
    dest_zone,
    dest_x,
    dest_y,
    dest_z,
    dest_heading,
    invert_state,
    incline,
    size,
    client_version_mask,
    islift,
    close_time,
    can_open,
    instance_only,
    min_expansion,
    max_expansion,
    guild_zone_door,
    pvp_zone_door,
    content_flags,
    content_flags_disabled,
    pvp_max_level
)
SELECT
    52,
    'solrotower',
    'POKAAPORT500',
    174.62,
    -165.88,
    -572.87,
    0,
    opentype,
    lockpick,
    keyitem,
    altkeyitem,
    nokeyring,
    triggerdoor,
    triggertype,
    doorisopen,
    door_param,
    'PoTranquility',
    -1463,
    774,
    -878,
    131,
    invert_state,
    incline,
    500,
    client_version_mask,
    islift,
    close_time,
    can_open,
    instance_only,
    min_expansion,
    max_expansion,
    guild_zone_door,
    pvp_zone_door,
    content_flags,
    content_flags_disabled,
    pvp_max_level
FROM doors
WHERE id = 2050
  AND NOT EXISTS (
      SELECT 1
      FROM doors
      WHERE zone = 'solrotower'
        AND doorid = 52
  );

UPDATE doors
SET
    name = 'POKAAPORT500',
    pos_y = 174.62,
    pos_x = -165.88,
    pos_z = -572.87,
    heading = 0,
    opentype = 58,
    dest_zone = 'PoTranquility',
    dest_x = -1463,
    dest_y = 774,
    dest_z = -878,
    dest_heading = 131,
    size = 500
WHERE zone = 'solrotower'
  AND doorid = 52;
