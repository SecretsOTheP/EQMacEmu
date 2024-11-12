CREATE TABLE hardcore_ghosts (
    ghost_id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    name VARCHAR(255) NOT NULL,
    last_name VARCHAR(255) NOT NULL,
    title VARCHAR(255) NOT NULL,
    suffix VARCHAR(255) NOT NULL,
    gender TINYINT NOT NULL,
    race SMALLINT NOT NULL,
    class TINYINT NOT NULL,
    level TINYINT UNSIGNED NOT NULL,
    deity INT UNSIGNED NOT NULL,
    size FLOAT NOT NULL,
    face INT NOT NULL,
    hair_color INT NOT NULL,
    hair_style INT NOT NULL,
    beard INT NOT NULL,
    beard_color INT NOT NULL,
    eye_color_1 INT NOT NULL,
    eye_color_2 INT NOT NULL,
    KEY (name),
    KEY (level),
    PRIMARY KEY (ghost_id)
);

-- Helpful query for filling out data in the hardcore_ghosts table

-- INSERT INTO hardcore_ghosts (
--     name,
--     last_name,
--     title,
--     suffix,
--     gender,
--     race,
--     class,
--     level,
--     deity,
--     size,
--     face,
--     hair_color,
--     hair_style,
--     beard,
--     beard_color,
--     eye_color_1,
--     eye_color_2
-- )
-- SELECT
--     name,
--     last_name,
--     title,
--     suffix,
--     gender,
--     race,
--     class,
--     level,
--     deity,
--     6.0 AS size, -- Assuming a default size value, adjust as needed
--     face,
--     hair_color,
--     hair_style,
--     beard,
--     beard_color,
--     eye_color_1,
--     eye_color_2
-- FROM character_data
-- WHERE NAME = "Wizardy"; -- Assuming the character name is "Wizardy", adjust as needed
