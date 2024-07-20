DROP TABLE IF EXISTS `character_reimbursements`;
CREATE TABLE `character_reimbursements`  (
  `charid` int(10) UNSIGNED NOT NULL DEFAULT 0,
  `slot` tinyint(3) UNSIGNED NOT NULL DEFAULT 0,
  `itemid` int(10) UNSIGNED NOT NULL DEFAULT 0,
  `charges` int(10) UNSIGNED NOT NULL DEFAULT 1,
  `quantity` tinyint(4) NOT NULL DEFAULT 0,
  PRIMARY KEY (`charid`, `slot`) USING BTREE,
  INDEX `charid_2`(`charid`, `itemid`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = latin1 COLLATE = latin1_swedish_ci ROW_FORMAT = Dynamic;