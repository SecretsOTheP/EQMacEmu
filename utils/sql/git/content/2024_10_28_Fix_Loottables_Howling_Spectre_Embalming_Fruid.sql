-- https://discord.com/channels/1133452007412334643/1300231705491148931
-- Elroz: fixed also, had no min/max drop set
-- changed to 1 and 1
-- howling spectre and embalming fluid main loot table needs mindrop 1 and droplimit 1 set 
update loottable_entries set droplimit = 1, mindrop = 1 where lootdrop_id in (91740,108306);