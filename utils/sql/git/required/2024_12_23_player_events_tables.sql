CREATE TABLE `player_event_log_settings`
(
    `id`                 bigint(20) NOT NULL,
    `event_name`         varchar(100) DEFAULT NULL,
    `event_enabled`      tinyint(1) DEFAULT NULL,
    `retention_days`     int(11) DEFAULT 0,
    `discord_webhook_id` int(11) DEFAULT 0,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `player_event_logs`
(
    `id`              bigint(20) NOT NULL AUTO_INCREMENT,
    `account_id`      bigint(20) DEFAULT NULL,
    `character_id`    bigint(20) DEFAULT NULL,
    `zone_id`         int(11) DEFAULT NULL,
    `instance_id`     int(11) DEFAULT NULL,
    `x`               float        DEFAULT NULL,
    `y`               float        DEFAULT NULL,
    `z`               float        DEFAULT NULL,
    `heading`         float        DEFAULT NULL,
    `event_type_id`   int(11) DEFAULT NULL,
    `event_type_name` varchar(255) DEFAULT NULL,
    `event_data`      longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`event_data`)),
    `created_at`      datetime     DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY               `event_created_at` (`event_type_id`,`created_at`),
    KEY               `zone_id` (`zone_id`),
    KEY               `character_id` (`character_id`,`zone_id`) USING BTREE,
    KEY               `created_at` (`created_at`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8mb4;

DROP TABLE `hackers`;
DROP TABLE `eventlog`;

DROP TABLE IF EXISTS `qs_player_trade_log`;
CREATE TABLE `qs_player_trade_log` (
	`char1_id` INT(11) NULL DEFAULT '0',
	`char1_pp` INT(11) NULL DEFAULT '0',
	`char1_gp` INT(11) NULL DEFAULT '0',
	`char1_sp` INT(11) NULL DEFAULT '0',
	`char1_cp` INT(11) NULL DEFAULT '0',
	`char1_items` MEDIUMINT(7) NULL DEFAULT '0',
	`char2_id` INT(11) NULL DEFAULT '0',
	`char2_pp` INT(11) NULL DEFAULT '0',
	`char2_gp` INT(11) NULL DEFAULT '0',
	`char2_sp` INT(11) NULL DEFAULT '0',
	`char2_cp` INT(11) NULL DEFAULT '0',
	`char2_items` MEDIUMINT(7) NULL DEFAULT '0',
	`time` TIMESTAMP NULL DEFAULT NULL ON UPDATE current_timestamp()
)
COLLATE='utf8mb3_general_ci'
ENGINE=InnoDB
;

DROP TABLE IF EXISTS `qs_player_trade_record`;
CREATE TABLE `qs_player_trade_record` (
	`trade_id` INT(11) NOT NULL AUTO_INCREMENT,
	`time` TIMESTAMP NULL DEFAULT NULL ON UPDATE current_timestamp(),
	`char1_id` INT(11) NULL DEFAULT '0',
	`char1_pp` INT(11) NULL DEFAULT '0',
	`char1_gp` INT(11) NULL DEFAULT '0',
	`char1_sp` INT(11) NULL DEFAULT '0',
	`char1_cp` INT(11) NULL DEFAULT '0',
	`char1_items` MEDIUMINT(7) NULL DEFAULT '0',
	`char2_id` INT(11) NULL DEFAULT '0',
	`char2_pp` INT(11) NULL DEFAULT '0',
	`char2_gp` INT(11) NULL DEFAULT '0',
	`char2_sp` INT(11) NULL DEFAULT '0',
	`char2_cp` INT(11) NULL DEFAULT '0',
	`char2_items` MEDIUMINT(7) NULL DEFAULT '0',
	PRIMARY KEY (`trade_id`) USING BTREE
)
COLLATE='utf8mb3_general_ci'
ENGINE=InnoDB
;

DROP TABLE IF EXISTS `qs_player_trade_record_entries`;
CREATE TABLE `qs_player_trade_record_entries` (
	`event_id` INT(11) NULL DEFAULT '0',
	`from_id` INT(11) NULL DEFAULT '0',
	`from_slot` MEDIUMINT(7) NULL DEFAULT '0',
	`to_id` INT(11) NULL DEFAULT '0',
	`to_slot` MEDIUMINT(7) NULL DEFAULT '0',
	`item_id` INT(11) NULL DEFAULT '0',
	`charges` MEDIUMINT(7) NULL DEFAULT '0'
)
COLLATE='utf8mb3_general_ci'
ENGINE=InnoDB
;

