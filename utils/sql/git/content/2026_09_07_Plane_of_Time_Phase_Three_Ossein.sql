INSERT INTO lootdrop_entries (
    lootdrop_id,
    item_id,
    item_charges,
    equip_item,
    chance,
    minlevel,
    maxlevel,
    multiplier,
    disabled_chance,
    min_expansion,
    max_expansion,
    min_looter_level,
    item_loot_lockout_timer,
    content_flags_disabled,
    content_flags
)
SELECT
    source.lootdrop_id,
    32605,
    source.item_charges,
    source.equip_item,
    source.chance,
    source.minlevel,
    source.maxlevel,
    source.multiplier,
    source.disabled_chance,
    source.min_expansion,
    source.max_expansion,
    source.min_looter_level,
    source.item_loot_lockout_timer,
    source.content_flags_disabled,
    source.content_flags
FROM lootdrop_entries AS source
WHERE source.lootdrop_id = 22799
  AND source.item_id = 26789
  AND NOT EXISTS (
      SELECT 1
      FROM lootdrop_entries AS existing
      WHERE existing.lootdrop_id = source.lootdrop_id
        AND existing.item_id = 32605
  );
