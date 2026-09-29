-- Correct the Seventh Hammer's database respawn timer.
-- The previous alignment migration used 237600 (the timer value) as a spawn ID.
UPDATE spawn2
SET
    respawntime = 237600,
    variance = 0
WHERE id = 345320;
