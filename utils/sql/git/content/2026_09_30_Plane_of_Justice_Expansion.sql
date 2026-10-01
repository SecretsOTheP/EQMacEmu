-- Plane of Justice belongs to the Planes of Power expansion.
UPDATE `zone`
SET `expansion` = 4
WHERE `zoneidnumber` = 201
  AND `short_name` = 'pojustice'
  AND `expansion` <> 4;