CREATE TABLE `character_instance_lockouts`  (
  `character_id` int(11) NOT NULL,
  `zone_id` int(11) NOT NULL,
  `zone_instance_id` int(11) NOT NULL,
  `expiry` bigint(24) NOT NULL,
  PRIMARY KEY (`character_id`, `zone_id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = latin1 COLLATE = latin1_swedish_ci ROW_FORMAT = Dynamic;
