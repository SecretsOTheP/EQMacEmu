ALTER TABLE `account` 
ADD COLUMN `forum_name` varchar(32) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT '' AFTER `force_guild_id`;