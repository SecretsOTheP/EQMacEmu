CREATE TABLE `spell_modifiers` (
  `client_cast` tinyint NOT NULL,
  `spell_match_id` int NOT NULL,
  `zone_id` int DEFAULT 0,
  `tic_duration` int DEFAULT 0,
  `tic_multiplier` decimal(6,2) DEFAULT 0,
  `tic_add` int DEFAULT 0,
  PRIMARY KEY (`client_cast`,`spell_match_id`)
) ENGINE=InnoDB CHARSET=latin1 COLLATE=latin1_swedish_ci;