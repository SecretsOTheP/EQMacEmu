-- Hand of Glory (only the door behind Drusella, at the exit pad)
UPDATE doors SET nokeyring = 0 WHERE keyitem = 17274 and altkeyitem = 0;

INSERT INTO keyring_data (key_item, key_name, zoneid, stage)
SELECT 17274, 'Hand of Glory', 105, 0
WHERE NOT EXISTS (SELECT 1 FROM keyring_data WHERE key_item = 17274);

-- Hole Key
UPDATE doors SET nokeyring = 0 WHERE keyitem = 6379 AND altkeyitem = 0;

INSERT INTO keyring_data (key_item, key_name, zoneid, stage)
SELECT 6379, 'Hole Key', 75, 0
WHERE NOT EXISTS (SELECT 1 FROM keyring_data WHERE key_item = 6379);

-- Paineel Key
UPDATE doors SET nokeyring = 0 WHERE keyitem = 6378 AND altkeyitem = 0;

INSERT INTO keyring_data (key_item, key_name, zoneid, stage)
SELECT 6378, 'Bone Crafted Key', 75, 0
WHERE NOT EXISTS (SELECT 1 FROM keyring_data WHERE key_item = 6378);
