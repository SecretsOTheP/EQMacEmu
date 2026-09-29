-- When a bard exceeds the kite limit, move the extra NPCs to the bard.
-- The earlier summon-direction migration set this rule to false.
UPDATE `rule_values`
SET `rule_value` = 'true'
WHERE `rule_name` = 'Quarm:BardInstagibReverseSummon';
