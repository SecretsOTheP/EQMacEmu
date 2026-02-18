-- This is a more targetted fix for the Ssra Emperor faction comapred to the patch on 1/7
-- Friendly Fire is still disabled by keeping 'primaryfaction' the same for all units, but restores everything else.
-- Fixes sideaffects caused by everything using the same npc_faction_id, which broke some pull mechanics.

-- Summary:
-- Changes 'Primary Faction ID' of Emperor & Adds to Brood, which is the one thing that is needed to prevent friendly fire.
-- Restores original npc_faction_ids to all units (emp now has the original emp faction hits)
-- Restores original faction hits to all units (adds have the oringnal adds faction hits)

-- Set's Emp primary faction to Brood (so they can't friendly fire)
UPDATE npc_faction SET primaryfaction = 1535 WHERE id = 1389;

-- Below restores everything else to stock, to be compatible with above --

-- Restores original faction IDs
UPDATE npc_types SET npc_faction_id = 1389 WHERE id = 162491; -- Emperor Ssraeshza(real)
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162189; -- #Blood_of_Ssraeshza
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162493; -- #Ssraeshzian_Blood_Golem
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162126; -- #Nilasz_the_Devourer
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162127; -- #Skzik_the_Tormentor
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162128; -- #Grziz_the_Tormentor
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162124; -- #Yasiz_the_Devourer
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162130; -- #Klazaz_the_Slayer
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162125; -- #Zlakas_the_Slayer
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162123; -- #Heriz_the_Malignant
UPDATE npc_types SET npc_faction_id = 839 WHERE id = 162129; -- #Slakiz_the_Malignant

 -- Restore's Brood faction data/hits
UPDATE npc_faction SET primaryfaction = 1535 WHERE id = 839;
UPDATE npc_faction_entries SET value = -3 WHERE npc_faction_id = 839 and faction_id = 1535;
UPDATE npc_faction_entries SET value = -3 WHERE npc_faction_id = 839 and faction_id = 1536;
UPDATE npc_faction_entries SET value = -1 WHERE npc_faction_id = 839 and faction_id = 1562;

-- Restores Emperor faction hits
UPDATE npc_faction_entries SET value = -1000 WHERE npc_faction_id = 1389 and faction_id = 1535;
UPDATE npc_faction_entries SET value = -1000 WHERE npc_faction_id = 1389 and faction_id = 1536;
UPDATE npc_faction_entries SET value = -500 WHERE npc_faction_id = 1389 and faction_id = 1562;
