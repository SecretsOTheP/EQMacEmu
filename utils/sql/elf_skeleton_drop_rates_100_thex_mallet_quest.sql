
/* confirm an_elf_skeleton npc id */
select * from quarm.npc_types
where name = 'an_elf_skeleton'
;

/* confirm spawnentry for an elf skeleton */
select * from spawnentry
where npcid = '36062'
;

/* confirm spawn group id from prior spawnentry only contains 2 mobs */
select * from spawnentry where spawngroupid = '222349'
;

/* remove chance for a_dread_bone to spawn while legacy item exists. Prior chance was 50 */
update spawnentry
set chance = 0
where spawngroupID = '222349' and npcID = '36048'
;

/* increase chance of an_elf_skeleton to 100 while legacy item exists.  Prior chance was 50  */
update spawnentry
set chance = 100
where spawngroupID = '222349' and npcID = '36062'
;


/* confirm item id's for mallet hilt and head  */
select * from quarm.items where id in ('13391', '13392')
;

/* confirm these items exist only in their own lootdrop entries  */
select * from lootdrop_entries 
where item_id in 
('13391', 
'13392')
;

/* confirmed separate loottable entries, no current drop limit  */
select * from loottable_entries
where lootdrop_id in
(
105908,
105909
)
;

/* confirmed to only affect an_elf_skeleton  */
select * from npc_types where loottable_id = '93672'
;

/* set chance for hilt = 100    previous hilt chance was 14.815 */
update lootdrop_entries
set chance = 100
where item_id = '13391' 
and lootdrop_id = '105908'
;

/* set chance for head = 100    previous head chance was 35.185 */
update lootdrop_entries
set chance = 100
where item_id = '13392'
and lootdrop_id = '105909'
;
