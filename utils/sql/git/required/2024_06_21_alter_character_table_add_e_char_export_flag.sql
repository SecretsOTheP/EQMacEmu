ALTER TABLE `character_data` 
ADD COLUMN `e_char_export_flag` tinyint unsigned NOT NULL DEFAULT 0 AFTER `e_married_character_id`;