-- Allow the PoP spell turn-in materials to be traded.
UPDATE `items`
SET `nodrop` = 1
WHERE `id` IN (29112, 29131, 29132)
  AND `nodrop` = 0;
