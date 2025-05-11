-- Adds SK ability to research necro spells, same level/skill caps at Necros to train.
-- Note: Also requires dll patch to allow clicking the Combine button.

-- Copys all Necro (11) Skill Caps for research to SK (5)
INSERT INTO skill_caps (skill_id, class_id, `level`, cap, class_)
SELECT skill_id, 5, `level`, cap, class_ FROM skill_caps sc WHERE sc.class_id = 11 and sc.skill_id = 58; 
