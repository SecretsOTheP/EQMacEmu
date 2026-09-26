CREATE TABLE `character_blocked_buffs` (
  `character_id` int(11) UNSIGNED NOT NULL,
  `spell_id` smallint(5) UNSIGNED NOT NULL,
  `if_spell_id` smallint(5) UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`character_id`, `spell_id`, `if_spell_id`)
);
