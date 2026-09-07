-- Remove the obsolete PvP event teleporter/book from Bastion of Thunder.
DELETE FROM `doors`
WHERE `id` = 50964
  AND `zone` = "bothunder"
  AND `dest_zone` = "ecommons";
