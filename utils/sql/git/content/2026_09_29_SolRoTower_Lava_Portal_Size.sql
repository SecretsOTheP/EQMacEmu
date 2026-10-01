-- Widen the lava recovery portal in Sol Ro Tower.
UPDATE `doors`
SET `size` = 5000
WHERE `zone` = 'solrotower'
  AND `doorid` = 52
  AND `id` = 6108355;
