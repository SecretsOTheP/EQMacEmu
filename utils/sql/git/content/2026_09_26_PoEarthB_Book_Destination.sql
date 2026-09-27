-- Point the Plane of Earth A book to the normal Plane of Earth B entry.
-- The neighboring PoEarthA doors and original portal book use these coordinates.
UPDATE doors
SET dest_x = -764.0,
    dest_y = 333.0,
    dest_z = -59.0,
    dest_heading = 132.0
WHERE zone = 'poeartha'
  AND doorid = 116
  AND name = 'POKTELE500'
  AND dest_zone = 'poearthb';
