CREATE TABLE `hardcore_ghosts_inventory` (
	`ghost_id` INT(11) NOT NULL DEFAULT '0',
	`slotid` MEDIUMINT(7) UNSIGNED NOT NULL DEFAULT '0',
	`itemid` INT(11) UNSIGNED NULL DEFAULT '0',
	`charges` SMALLINT(3) UNSIGNED NULL DEFAULT '0',
	`custom_data` TEXT NULL DEFAULT NULL,
	`serialnumber` INT(10) NOT NULL DEFAULT '0',
	`initialserial` TINYINT(3) NOT NULL DEFAULT '0',
	PRIMARY KEY (`ghost_id`, `slotid`),
	INDEX `charid` (`ghost_id`),
	INDEX `itemid` (`itemid`)
)
;


-- Helpful query for filling out data in the hardcore_ghosts table

-- INSERT INTO hardcore_ghosts_inventory (ghost_id, slotid, itemid, charges, custom_data, serialnumber, initialserial)
-- SELECT 1, ci.slotid, ci.itemid, ci.charges, ci.custom_data, ci.serialnumber, ci.initialserial
-- FROM character_inventory ci
-- JOIN character_data cd ON ci.id = cd.id
-- WHERE cd.name = 'Wizardy';
