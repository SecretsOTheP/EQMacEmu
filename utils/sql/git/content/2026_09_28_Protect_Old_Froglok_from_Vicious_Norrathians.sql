/* The Priests of Marr ask the Vicious Norrathians to be kind to the elderly. */

/* Make an_old_froglok the guaranteed spawn in this group. */
UPDATE spawnentry
SET chance = 100
WHERE spawngroupID = 224096
  AND npcID = 65002;

/* Remove the alternate NPC from this spawn group. */
UPDATE spawnentry
SET chance = 0
WHERE spawngroupID = 224096
  AND npcID = 65019;

/* Give an_old_froglok the same protection abilities as Giwin_Mirakon. */
UPDATE npc_types
SET special_abilities = '1,1^2,1^8,1^10,1^14,1^19,1^20,1^24,1^25,1^35,1'
WHERE id = 65002;