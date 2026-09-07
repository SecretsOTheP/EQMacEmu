-- Apply a 2-day, 18-hour raid lockout to Plane of Tactics progression bosses.
UPDATE `npc_types`
SET `loot_lockout` = 237600
WHERE `id` IN (
  214026, -- Tallon Zek
  214317, -- Vallon Zek final real form
  214312  -- Rallos Zek the Warlord
);
