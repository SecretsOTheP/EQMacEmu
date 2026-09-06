-- Keep the database command override aligned with the C++ registration.
-- Existing aliases are preserved when the command row is already present.
INSERT INTO `command_settings` (`command`, `access`, `aliases`)
VALUES ('undeletechar', 100, '')
ON DUPLICATE KEY UPDATE
    `access` = VALUES(`access`);
