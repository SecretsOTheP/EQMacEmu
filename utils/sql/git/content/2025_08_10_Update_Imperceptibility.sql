-- Update Imperceptibility spell to match Shrink's zonetype, allowing usage in more zones
UPDATE spells_new SET zonetype = -1 WHERE id = 2037