-- Make scribeable NO DROP "Spell:" scrolls tradeable.
UPDATE `items`
SET `nodrop` = 1
WHERE `id` IN (
    1118, 1119, 9722, 15678, 15924, 15925,
    19436, 19469, 19470, 19491,
    26948, 26949, 26950, 26951, 26952, 26955, 26956, 26957,
    26958, 26959, 26960, 26961, 26962, 26963, 26964, 26965,
    26966, 26967, 26968, 26969, 26971, 26972, 26973, 26974,
    26975, 26976, 26977, 26978
)
  AND `name` LIKE 'Spell: %'
  AND `itemtype` = 20
  AND `scrolleffect` > 0
  AND `classes` <> 0
  AND `nodrop` = 0;
