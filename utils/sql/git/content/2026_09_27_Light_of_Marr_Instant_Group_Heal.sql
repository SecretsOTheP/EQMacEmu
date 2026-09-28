-- Light of Marr: replace the old two-tick heal with one instant 150 HP group heal.
-- Keep spells_new and spells_en in sync; the client spells_en.txt row is distributed separately.
UPDATE `spells_new`
SET `goodEffect` = 1,
    `targettype` = 41,
    `resisttype` = 0,
    `cast_on_you` = 'The Light of Marr surrounds you.',
    `cast_on_other` = ' is surrounded by the Light of Marr.',
    `spell_fades` = '',
    `buffdurationformula` = 0,
    `buffduration` = 0,
    `effectid1` = 0,
    `effect_base_value1` = 150,
    `formula1` = 100,
    `effectid2` = 254,
    `effect_base_value2` = 0,
    `effectid3` = 254,
    `effect_base_value3` = 0,
    `RecourseLink` = 0
WHERE `id` = 3606
  AND `name` = 'Light of Marr';

UPDATE `spells_en`
SET `goodEffect` = 1,
    `targettype` = 41,
    `resisttype` = 0,
    `cast_on_you` = 'The Light of Marr surrounds you.',
    `cast_on_other` = ' is surrounded by the Light of Marr.',
    `spell_fades` = '',
    `buffdurationformula` = 0,
    `buffduration` = 0,
    `effectid1` = 0,
    `effect_base_value1` = 150,
    `formula1` = 100,
    `effectid2` = 254,
    `effect_base_value2` = 0,
    `effectid3` = 254,
    `effect_base_value3` = 0,
    `RecourseLink` = 0
WHERE `id` = 3606
  AND `name` = 'Light of Marr';
