ALTER TABLE `character_corpse_items`
ADD COLUMN `custom_data` TEXT DEFAULT NULL AFTER `charges`;

ALTER TABLE `character_corpse_items_backup`
ADD COLUMN `custom_data` TEXT DEFAULT NULL AFTER `charges`;

ALTER TABLE `object`
ADD COLUMN `custom_data` TEXT DEFAULT NULL AFTER `charges`;

ALTER TABLE `object_contents`
ADD COLUMN `custom_data` TEXT DEFAULT NULL AFTER `charges`;