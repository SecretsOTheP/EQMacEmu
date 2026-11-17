-- Manual content SQL: mark Bo Staff of Transcendence as a magic weapon.
-- Safe to rerun; only the magic field on this item is changed.
UPDATE `items`
SET `magic` = 1
WHERE `id` = 25225
  AND `Name` = 'Bo Staff of Transcendence'
  AND `magic` = 0;
