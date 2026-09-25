-- Codecay was temporarily marked as a holiday zone. Restore its Plane of Power
-- expansion classification so access and guild-instance rules recognize it as PoP.
UPDATE zone
SET expansion = 4
WHERE short_name = 'codecay';
