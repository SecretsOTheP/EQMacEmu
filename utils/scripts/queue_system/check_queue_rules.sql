-- Quick Quarm Queue Rules Check
-- Run with: mysql -u username -p quarm < check_queue_rules.sql

SELECT '=== QUEUE SYSTEM RULES ===' as '';

SELECT
	'Queue Rules' as 'Category',
	rule_name as 'Rule Name',
	rule_value as 'Current Value',
	defaults as 'Default Value'
FROM rule_values 
WHERE rule_name IN (
	'Quarm:MaxPlayersOnline',
	'Quarm:EnableQueue',
	'Quarm:TestPopulationOffset',
	'Quarm:QueueBypassGMLevel',
	'Quarm:QueueEstimatedWaitPerPlayer',
	'Quarm:EnableQueueLogging',
	'Quarm:FreezeQueue',
	'Quarm:EnableQueuePersistence'
)
ORDER BY 
CASE rule_name
	WHEN 'Quarm:MaxPlayersOnline' THEN 1
	WHEN 'Quarm:EnableQueue' THEN 2
	WHEN 'Quarm:TestPopulationOffset' THEN 3
	WHEN 'Quarm:QueueBypassGMLevel' THEN 4
	WHEN 'Quarm:QueueEstimatedWaitPerPlayer' THEN 5
	WHEN 'Quarm:EnableQueueLogging' THEN 6
	WHEN 'Quarm:FreezeQueue' THEN 7
	WHEN 'Quarm:EnableQueuePersistence' THEN 8
END;

SELECT '' as '';
SELECT '=== CURRENT SERVER STATUS ===' as '';

SELECT 
    server_id as 'Server ID',
    current_population as 'Current Population',
    last_updated as 'Last Updated'
FROM server_population;

SELECT '' as '';
SELECT '=== ACTIVE ACCOUNT RESERVATIONS ===' as '';

SELECT COUNT(*) as 'Active Reservations' FROM active_account_connections;

SELECT '' as '';
SELECT '=== QUEUE ENTRIES ===' as '';

SELECT COUNT(*) as 'Players in Queue' FROM tblLoginQueue; 