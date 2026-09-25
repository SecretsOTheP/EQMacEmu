-- Restore the in-zone destination for the Codecay throne portal.
-- Without a destination, CDTHRONE501 behaves as a regular door and cannot
-- teleport players to the throne platform.
UPDATE doors
SET dest_zone = 'codecay',
    dest_x = 82.59,
    dest_y = 59.18,
    dest_z = -290.87,
    dest_heading = 256
WHERE zone = 'codecay'
  AND doorid = 7
  AND name = 'CDTHRONE501';
