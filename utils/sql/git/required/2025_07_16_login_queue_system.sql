-- Login Queue System Database Update
-- Add queue persistence table for maintaining queue positions across restarts
-- This enables queue persistence during emergency maintenance with 100+ queued players

CREATE TABLE IF NOT EXISTS tblLoginQueue (
    account_id INT UNSIGNED NOT NULL,
    world_server_id INT UNSIGNED NOT NULL,
    queue_position INT UNSIGNED NOT NULL,
    estimated_wait INT UNSIGNED NOT NULL,
    ip_address INT UNSIGNED NOT NULL,
    queued_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (account_id, world_server_id),
    INDEX idx_world_position (world_server_id, queue_position),
    INDEX idx_timestamp (queued_timestamp)
) ENGINE=InnoDB;

-- Server Population Tracking Table
-- Real-time population data updated by LoginServer::SendStatus()
-- Used for monitoring and debugging queue decisions
CREATE TABLE IF NOT EXISTS server_population (
    server_id INT NOT NULL DEFAULT 1,
    effective_population INT NOT NULL DEFAULT 0,
    last_updated TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (server_id)
) ENGINE=InnoDB;

-- Initialize server_population table with default row
INSERT IGNORE INTO server_population (server_id, effective_population) VALUES (1, 0);

-- Initialize RefreshQueue setting with default value
INSERT INTO tblloginserversettings (type, value, category, description, defaults)
VALUES ('RefreshQueue', '0', 'options', 'Trigger queue refresh - auto-reset by system', '0')
ON DUPLICATE KEY UPDATE value = '0', description = 'Trigger queue refresh - auto-reset by system'; 

-- Initialize pop_count setting to enable population display in server list
INSERT INTO tblloginserversettings (type, value, category, description, defaults)
VALUES ('pop_count', '1', 'display', 'Show population counts in server list', '1')
ON DUPLICATE KEY UPDATE value = '1', description = 'Show population counts in server list'; 