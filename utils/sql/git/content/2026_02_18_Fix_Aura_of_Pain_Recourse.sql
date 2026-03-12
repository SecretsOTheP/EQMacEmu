-- Fixes "Aura of Pain Recourse" to not be marked as a detrimental spell effect
-- This recourse provides an AC buff to the caster
-- Was cause NPCs to attack each other because they thought it was detrimental when the caster applied the recourse to itself.

UPDATE spells_new SET goodEffect = 1 WHERE id = 3404; -- Change "Aura of Pain Recourse" to beneficial
