UPDATE spawn2 SET enabled ='1' WHERE spawngroupID IN (SELECT id FROM spawngroup WHERE NAME = "cazicthule a_Thulian_High_Ritualist");
