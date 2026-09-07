-- Temporary bard kite limits can be used during server or zone congestion
-- to prevent a single player from monopolizing large numbers of NPCs.
-- Over-limit NPCs should summon the bard rather than teleport to the bard.
UPDATE `rule_values`
SET `rule_value` = 'false'
WHERE `rule_name` = 'Quarm:BardInstagibReverseSummon';
