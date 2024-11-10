/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "../common/global_define.h"
#include "../common/features.h"
#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/races.h"

#include "client.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"

#include "queryserv.h"
#include "worldserver.h"
#include "quest_parser_collection.h"
#include "string_ids.h"

extern QueryServ* QServ;
extern WorldServer worldserver;

float Mob::GetBaseEXP()
{
	if (IsZomm())
		return 1.0f;

	float exp = EXP_FORMULA;

	float zemmod = 75.0f;
	if(zone->newzone_data.zone_exp_multiplier >= 0)
	{
		zemmod = zone->newzone_data.zone_exp_multiplier * 100;
	}
	// very low levels get an artifical ZEM.  It's either 100 or 114, not sure.  Also not sure how many levels this applies.  If you find out, fix this
	if (GetLevel() < 6 && zemmod < 100.0f)
		zemmod = 100.0f;

	float server_bonus = 1.0f;

	// AK had a permanent 20% XP increase.
	if (RuleB(AlKabor, ServerExpBonus))
		server_bonus += 0.20f;
	// Thanksgiving xp bonus
	if (RuleB(Quarm, ThanksgivingExpBonus)) {
		if (IsFlyingCreatureRace(GetRace())) {
			server_bonus += RuleR(Quarm, FlyingRaceExpBonus);
		}
		if (zone->CanCastOutdoor() && !zone->CanCastDungeon()) {
			server_bonus += RuleR(Quarm, ThanksgivingExpBonusOutdoorAmt);
		}
	}
	if (zone && zone->GetGuildID() != GUILD_NONE)
	{
		zemmod = RuleR(Quarm, InstanceZEMOverride);
	}

	float npc_pct = 1.0f;
	if (IsNPC())
		npc_pct = static_cast<float>(CastToNPC()->GetExpPercent()) / 100.0f;

	float basexp = exp * zemmod * server_bonus * npc_pct;
	float logged_xp = basexp;

	Log(Logs::General, Logs::EQMac, "Starting base exp is mob_level(%i)^2 * ZEM(%.0f) * server_bonus(%.2f) * npc_pct(%.2f) = %.0f exp", 
		level, zemmod, server_bonus, npc_pct, basexp);

	if (ds_damage + npc_damage >= total_damage)
	{
		basexp = 0;
		Log(Logs::General, Logs::EQMac, "%s was completely damaged by a damage shield/NPC. No XP for you one year.", GetName());
	}
	else if (player_damage == 0 && RuleB(Quarm, NoPlayerDamagePetPenalty))
	{
		basexp *= 0.25f;
		Log(Logs::General, Logs::EQMac, "%s was not damaged by a player. Exp reduced to 25 percent of normal", GetName());
	}
	else if (dire_pet_damage > 0)
	{
		float pet_dmg_pct = static_cast<float>(dire_pet_damage) / total_damage;
		float reduced_pct = 1.0f;
		if (pet_dmg_pct >= 1.0f)
		{
			reduced_pct = 0.25f;
			basexp *= reduced_pct;
		}
		else if (pet_dmg_pct > 0.5f)
		{
			reduced_pct = 0.75f - (pet_dmg_pct - 0.5f) / 2.0f;
			basexp *= reduced_pct;
		}

		Log(Logs::General, Logs::EQMac, "%s was %0.1f percent damaged by a dire charmed pet (%d/%d). Exp gained is %0.1f percent of normal", 
			GetName(), pet_dmg_pct * 100.0f, dire_pet_damage, total_damage, reduced_pct * 100.0f);
	}

	return basexp;
}

// this is an intermediate step to adding exp from a NPC kill and will multiply it based on levels involved and client race.  this is called AFTER group/raid splits.
// quest rewards do not use this
// is_split is so we can send the correct exp gain message later on.  it's true when the kill's exp is shared.  group members sometimes get solo exp
void Client::AddEXP(uint32 in_add_exp, uint8 conlevel, Mob* killed_mob, int16 avg_level, bool is_split, int16 highest_level) {

	if(IsMule() || !killed_mob)
		return;

	this->EVENT_ITEM_ScriptStopReturn();

	if(killed_mob && !killed_mob->IsZomm() && conlevel == CON_GREEN)
		return;

	uint32 add_exp = in_add_exp;

	float lb_mult = 1.0f;	// light blue con multiplier
	float hbm = 1.0f;		// hell level balance modifier
	float mlm = 1.0f;		// mob level modifier
	float con_mult;			// custom rules con reduction
	float race_mult = 1.0f; // race modifier for AA exp
	float totalmod = RuleR(Character, ExpMultiplier);	// should be 1.0 for non-custom
	float aa_mult = RuleR(Character, AAExpMultiplier);	// should be 1.0 for non-custom
	float aa_lvl_mod = 1.0f;	// level_exp_mods table
	float class_mult = 1.0f;	// since we don't factor class into exp required for level like Sony did, we have to add exp on kills.  does not apply to AAs
	if (GetClass() == WARRIOR) class_mult = 10.0f / 9.0f;
	if (GetClass() == ROGUE) class_mult = 10.0f / 9.05f;

	if (RuleB(Quarm, ErollsiDayEvent) && m_epp.married_character_id != 0 && IsGrouped())
	{
		Group* our_group = GetGroup();
		for (int x = 0; x < MAX_GROUP_MEMBERS; ++x)
		{
			if (our_group->members[x] != nullptr && our_group->members[x]->IsClient()) // If Group Member is Client
			{
				if (m_epp.married_character_id == our_group->members[x]->CastToClient()->CharacterID())
				{
					class_mult += 0.20f;
					Message(Chat::Yellow, "You receive a bonus! (Partner)");
					break;
				}
			}
		}
	}

	// This logic replicates the September 4 & 6 2002 patch exp modifications that granted a large
	// experience bonus to kills within +/-5 levels of the player for level 51+ players
	uint8 moblevel = killed_mob->GetLevel();
	int lvl_diff = moblevel - GetLevel();

	if (moblevel > 45 && GetLevel() > 50
		&& lvl_diff > -6 && lvl_diff < 6 && (GetLevel() + 5) >= avg_level) // Sony checked group level avg and deep reds to mitigate powerleveling)
	{
		mlm = 2.6f;		// old showeq threads called this the 'mob level modifier'

		if (lvl_diff && GetLevel() > 59)
		{
			mlm = (260.0f - 13.0f * static_cast<float>(GetLevel() - moblevel)) / 100.0f;
			if (mlm > 3.0f)
				mlm = 3.0f;	// Zamiel thread says 'no marginal benefit to killing NPCs more than 3 levels higher' which suggests mlm capped at 3.0.
		}
		else if (GetLevel() < 60)
		{
			float x = static_cast<float>(60u - GetLevel());
			float mlm_cap = 2.6f - x * 0.16;
			float adj = 0.13f - x * 0.013;
			if (moblevel > GetLevel())
				adj /= 2.0f;	// still not quite sure how to handle yellows/reds when player level < 60 but this should be close
			mlm = mlm_cap - ((GetLevel() - moblevel) * adj);
		}
	}
	else if (conlevel == CON_LIGHTBLUE)
	{
		// ShowEQ data from late 2002 showed high light blues granting 80% and 60% then dropping off to 20%
		// Older comments said that high light blues were 50% and low dark light blues were 25%, so this is contradictory
		// Sony may have changed light blue exp mid-Luclin

		lb_mult = 0.2f;
		if (!is_split)
			highest_level = GetLevel();

		if (highest_level)
		{
			if (highest_level > 40)
			{
				// not the most elegant way to do this but it works and doesn't require new methods
				if (GetLevelCon(highest_level, moblevel + 1) == CON_BLUE)
					lb_mult = 0.8f;
				else if (GetLevelCon(highest_level, moblevel + 2) == CON_BLUE)
					lb_mult = 0.6f;
			}
			else if (highest_level > 30)
			{
				// how it should scale at lower levels is unknown
				if (GetLevelCon(highest_level, moblevel + 1) == CON_BLUE)
					lb_mult = 0.6f;
			}
		}
	}

	//Disables the September 4 & 6 2002 patch exp modifications that granted a large experience bonus to kills within +/-5 levels of the player for level 51+ players
	if (!RuleB(AlKabor, EnableMobLevelModifier))
		mlm = 1.0f;

	// These rules are no longer used, but keeping them in for custom control.  They should all be 100.0 for non-custom
	con_mult = 100.0f;
	switch (conlevel)
	{
		case CON_LIGHTBLUE:
		{
			con_mult = RuleR(AlKabor, LightBlueExpMod);
			break;
		}
		case CON_BLUE:
		{
			con_mult = RuleR(AlKabor, BlueExpMod);
			break;
		}
		case CON_WHITE:
		{
			con_mult = RuleR(AlKabor, WhiteExpMod);
			break;
		}
		case CON_YELLOW:
		{
			con_mult = RuleR(AlKabor, YellowExpMod);
			break;
		}
		case CON_RED:
		{
			con_mult = RuleR(AlKabor, RedExpMod);
			break;
		}
	}
	con_mult /= 100.0f;

	add_exp = static_cast<uint32>(add_exp * lb_mult * mlm * con_mult * totalmod); // multipliers that apply to level and aa exp

	// if NPC is killed by PBAoE damage, then reduce experience gained if NPC is in a certain level range. (42-55)  this is AK behavior although specifics are still not known
	if (killed_mob->IsNPC() && RuleB(AlKabor, ReduceAEExp) && killed_mob->pbaoe_damage > (killed_mob->GetMaxHP() / 2))
	{
		float reduction_mult = killed_mob->CastToNPC()->GetPBAoEReduction(GetLevel());
		if (reduction_mult < 1.0)
		{
			Log(Logs::Detail, Logs::EQMac, "Experience reduced to %0.2f percent due to PBAoE reduction.", reduction_mult*100.0);
			add_exp *= reduction_mult;
		}
	}
	if (RuleB(Quarm, EnablePetExperienceSplit))
	{
		if (killed_mob && !HasGroup() && !is_split)
		{
			int32 damage_amount = 0;
			Mob* top_damager = killed_mob->GetDamageTop(damage_amount, false, false);
			if (top_damager)
			{
				if (top_damager->IsPet())
				{
					float pet_dmg_pct = static_cast<float>(damage_amount) / killed_mob->total_damage;
					if (pet_dmg_pct > 0.5f)
					{
						Log(Logs::General, Logs::EQMac, "%s was damaged more than 50 percent by a single pet. Pet takes 50 percent of experience value.", killed_mob->GetCleanName());

						add_exp = (float)add_exp * 0.5f;
					}
				}
			}
		}
	}

	// Used for Luclin era Hell Level Balance Multipliers.  Can also be used for custom behavior.  AA exp should always be 1.0 to be non-custom.
	if (RuleB(Zone, LevelBasedEXPMods))
	{
		hbm = zone->level_exp_mod[GetLevel()].ExpMod;
		if (!hbm)
			hbm = 1.0f;
		aa_lvl_mod = zone->level_exp_mod[GetLevel()].AAExpMod;
		if (!aa_lvl_mod)
			aa_lvl_mod = 1.0f;

		if (GetLevel() == 50 && hbm < 1.0f && RuleR(World, CurrentExpansion) < (float)ExpansionEras::KunarkEQEra)
			hbm = 1.0f;
		if (GetLevel() == 60 && hbm < 1.0f && RuleR(World, CurrentExpansion) < (float)ExpansionEras::PlanesEQEra)
			hbm = 1.0f;
	}

	if (m_epp.perAA<0 || m_epp.perAA>100)
		m_epp.perAA=0;	// stop exploit with sanity check

	uint32 add_aaxp = 0;
	if (GetAAPoints() < 30)
	{
		//figure out how much of this goes to AAs
		add_aaxp = add_exp * m_epp.perAA / 100;
		//take that amount away from regular exp
		add_exp -= add_aaxp;

		// Race modifiers apply to AA exp if AA exp is split
		if (RuleB(AlKabor, RaceEffectsAASplit) && m_epp.perAA > 0 && m_epp.perAA < 100)
		{
			if (GetRace() == HALFLING)
				race_mult = 1.05f;
			else if (GetRace() == BARBARIAN)
				race_mult = 0.95f;
			else if (GetRace() == OGRE)
				race_mult = 0.85f;
			else if (GetRace() == TROLL || GetRace() == IKSAR)
				race_mult = 0.8f;
		}
		
		add_aaxp = static_cast<uint32>(add_aaxp * race_mult * aa_lvl_mod * aa_mult);
	}

	add_exp = static_cast<uint32>(add_exp * hbm * class_mult);  // applies to level exp only

	uint32 requiredxp = GetEXPForLevel(GetLevel() + 1) - GetEXPForLevel(GetLevel());
	uint32 xp_cap = requiredxp / 8u;	// kill exp cap is 12.5%

	if (add_exp > xp_cap)
	{
		add_exp = xp_cap;
		Log(Logs::Detail, Logs::EQMac, "Exp capped to 12.5 percent of level exp");
	}

	if (killed_mob->IsZomm())
	{
		// Zomm always results in 1 exp
		add_exp = 1;
		add_aaxp = 0;
	}

	if (add_aaxp)
		Log(Logs::Detail, Logs::EQMac, "[Exp Multipliers] Light Blue: %0.2f  HBM: %0.3f  MLM: %0.3f  ConRule: %0.2f  ExpRule: %0.2f  AARule: %0.2f  Race: %0.2f", lb_mult, hbm, mlm, con_mult, totalmod, aa_mult, race_mult);
	else
		Log(Logs::Detail, Logs::EQMac, "[Exp Multipliers] Light Blue: %0.2f  HBM: %0.3f  MLM: %0.3f  ConRule: %0.2f  ExpRule: %0.2f  Class: %0.4f", lb_mult, hbm, mlm, con_mult, totalmod, class_mult);

	uint32 new_exp = GetEXP() + add_exp;
	uint32 old_aaexp = GetAAXP();
	uint32 new_aaexp = old_aaexp + add_aaxp;
	
	if (new_aaexp < old_aaexp)
		new_aaexp = old_aaexp;	//watch for wrap

	if (admin >= 100 && GetGM())
	{
		uint32 neededxp = GetEXPForLevel(GetLevel() + 1) - (GetEXP() + add_exp);
		float pct_level_gain = static_cast<float>(add_exp) / static_cast<float>(requiredxp) * 100.0f;
		float pct_aa_gain = static_cast<float>(add_aaxp) / static_cast<float>(RuleI(AA, ExpPerPoint)) * 100.0f;
		Message(Chat::Yellow, "[GM Debug] Final EXP awarded is %d (%0.2f%% of lvl) and %d AXP (%0.2f%% of AA). %d more EXP is needed for Level %d", 
			add_exp, pct_level_gain, add_aaxp, pct_aa_gain, neededxp, GetLevel()+1);
	}

	SetEXP(new_exp, new_aaexp, false, is_split);
}

void Client::AddEXPPercent(uint8 percent, uint8 level) {

	if(percent < 0)
		percent = 1;
	if(percent > 100)
		percent = 100;

	uint32 requiredxp = GetEXPForLevel(level+1) - GetEXPForLevel(level);
	float tmpxp = requiredxp * (percent/100.0);
	uint32 newxp = (uint32)tmpxp;
	AddQuestEXP(newxp, true);
}

void Client::AddQuestEXP(uint32 in_add_exp, bool bypass_cap) {

	// Quest handle method. This divides up AA XP, but does not apply bonuses/modifiers. The quest writer will do that.

	if(IsMule())
		return;

	if (RuleB(Quarm, EnableQuestBasedXPLimit))
	{
		if (GetLevel() >= RuleI(Quarm, QuestBasedXPLimitLevel))
		{
			return;
		}
	}

	this->EVENT_ITEM_ScriptStopReturn();

	uint32 add_exp = in_add_exp;

	if (RuleB(Quarm, EnableQuestExpMultiplier))
	{
		if (in_add_exp > 0)
		{
			add_exp = (uint32)((float)in_add_exp * (float)RuleR(Quarm, QuestExpMultiplier));
		}
	}

	if (m_epp.perAA<0 || m_epp.perAA>100)
		m_epp.perAA=0;	// stop exploit with sanity check

	uint32 add_aaxp;

	if (GetAAPoints() >= 30)
	{
		add_aaxp = 0;
	}
	else
	{
		//figure out how much of this goes to AAs
		add_aaxp = add_exp * m_epp.perAA / 100;
		//take that amount away from regular exp
		add_exp -= add_aaxp;
	}

	if (!bypass_cap)
	{
		// Quest exp is capped to 25% of the player's level
		uint32 cap_exp = (GetEXPForLevel(GetLevel() + 1) - GetEXPForLevel(GetLevel())) / 4u;
		if (add_exp > cap_exp)
			add_exp = cap_exp;
	}

	uint32 exp = GetEXP() + add_exp;
	uint32 aaexp = add_aaxp;
	uint32 had_aaexp = GetAAXP();
	aaexp += had_aaexp;
	if(aaexp < had_aaexp)
		aaexp = had_aaexp;	//watch for wrap

	SetEXP(exp, aaexp, false);
}


void Client::SetEXP(uint32 set_exp, uint32 set_aaxp, bool isrezzexp, bool is_split)
{
	if(IsMule())
		return;

	Log(Logs::Detail, Logs::None, "Attempting to Set Exp for %s (XP: %u, AAXP: %u, Rez: %s, Split: %s)", 
		this->GetCleanName(), set_exp, set_aaxp, isrezzexp ? "true" : "false", is_split ? "true" : "false");
	//max_AAXP = GetEXPForLevel(52) - GetEXPForLevel(51);	//GetEXPForLevel() doesn't depend on class/race, just level, so it shouldn't change between Clients
	max_AAXP = GetEXPForLevel(0, true);	//this may be redundant since we're doing this in Client::FinishConnState2()
	if (max_AAXP == 0 || GetEXPForLevel(GetLevel()) == 0xFFFFFFFF) {
		Message(Chat::Red, "Error in Client::SetEXP. EXP not set.");
		return; // Must be invalid class/race
	}

	if ((set_exp + set_aaxp) > (m_pp.exp+m_pp.expAA)) 
	{
		if (isrezzexp)
		{
			this->Message_StringID(Chat::Yellow, REZ_REGAIN);
		}
		else
		{
			if(this->IsGrouped() && is_split)
				this->Message_StringID(Chat::Yellow, GAIN_GROUPXP);
			else if(IsRaidGrouped() && is_split)
				Message_StringID(Chat::Yellow, GAIN_RAIDEXP);
			else
				this->Message_StringID(Chat::Yellow, GAIN_XP);

			if (m_epp.perAA > 0 && GetAAPoints() >= 30)
			{
				Message_StringID(Chat::Yellow, AA_POINTS_CAP);
			}
		}
	}
	else if((set_exp + set_aaxp) < (m_pp.exp+m_pp.expAA)){ //only loss message if you lose exp, no message if you gained/lost nothing.
		Message(Chat::Yellow, "You have lost experience.");
	}

	//check_level represents the level we should be when we have
	//this amount of exp (once these loops complete)
	uint16 check_level = GetLevel()+1;
	//see if we gained any levels
	bool level_increase = true;
	int8 level_count = 0;

	while (set_exp >= GetEXPForLevel(check_level)) 
	{
		check_level++;
		if (check_level > 127)
		{	//hard level cap
			check_level = 127;
			break;
		}
		level_count++;
	}
	//see if we lost any levels
	while (set_exp < GetEXPForLevel(check_level-1))
	{
		check_level--;
		if (check_level < 2)
		{	//hard level minimum
			check_level = 2;
			break;
		}
		level_increase = false;
	}
	check_level--;


	//see if we gained any AAs
	if (set_aaxp >= max_AAXP) {
		/*
			Note: AA exp is stored differently than normal exp.
			Exp points are only stored in m_pp.expAA until you
			gain a full AA point, once you gain it, a point is
			added to m_pp.aapoints and the amount needed to gain
			that point is subtracted from m_pp.expAA

			then, once they spend an AA point, it is subtracted from
			m_pp.aapoints. In theory it then goes into m_pp.aapoints_spent,
			but I'm not sure if we have that in the right spot.
		*/
		//record how many points we have
		uint32 last_unspentAA = m_pp.aapoints;

		//figure out how many AA points we get from the exp were setting
		m_pp.aapoints = set_aaxp / max_AAXP;
		Log(Logs::Detail, Logs::None, "Calculating additional AA Points from AAXP for %s: %u / %u = %.1f points", this->GetCleanName(), set_aaxp, max_AAXP, (float)set_aaxp / (float)max_AAXP);

		//get remainder exp points, set in PP below
		set_aaxp = set_aaxp - (max_AAXP * m_pp.aapoints);

		//add in how many points we had
		m_pp.aapoints += last_unspentAA;
		//set_aaxp = m_pp.expAA % max_AAXP;

		//figure out how many points were actually gained
		/*uint32 gained = m_pp.aapoints - last_unspentAA;*/	//unused

		//Message(Chat::Yellow, "You have gained %d skill points!!", m_pp.aapoints - last_unspentAA);
		char val1[20]={0};
		Message_StringID(Chat::Yellow, GAIN_ABILITY_POINT,ConvertArray(m_pp.aapoints, val1),m_pp.aapoints == 1 ? "" : "(s)");	//You have gained an ability point! You now have %1 ability point%2.
		if (m_pp.aapoints >= 30)
		{
			Message_StringID(Chat::Yellow, AA_CAP_REACHED);
		}
		
		/* QS: PlayerLogAARate */
		if (RuleB(QueryServ, PlayerLogAARate))
		{
			QServ->QSAARate(this->CharacterID(), m_pp.aapoints, last_unspentAA);
		}
		//Message(Chat::Yellow, "You now have %d skill points available to spend.", m_pp.aapoints);
	}

	uint8 maxlevel = RuleI(Character, MaxExpLevel) + 1;

	if(maxlevel <= 1)
		maxlevel = RuleI(Character, MaxLevel) + 1;

	if(check_level > maxlevel) {
		check_level = maxlevel;

		if(RuleB(Character, KeepLevelOverMax)) {
			set_exp = GetEXPForLevel(GetLevel()+1);
		}
		else {
			set_exp = GetEXPForLevel(maxlevel);
		}
	}

	if(RuleB(Character, PerCharacterQglobalMaxLevel)){
		uint32 MaxLevel = GetCharMaxLevelFromQGlobal();
		if(MaxLevel){
			if(GetLevel() >= MaxLevel){
				uint32 expneeded = GetEXPForLevel(MaxLevel);
				if(set_exp > expneeded) {
					set_exp = expneeded;
				}
			}
		}
	}

	if (RuleB(Character, PerCharacterBucketMaxLevel)) {
		uint32 MaxLevel = GetCharMaxLevelFromBucket();
		if (MaxLevel) {
			if (GetLevel() >= MaxLevel) {
				uint32 expneeded = GetEXPForLevel(MaxLevel);
				if (set_exp > expneeded) {
					set_exp = expneeded;
				}
			}
		}
	}

	//If were at max level then stop gaining experience if we make it to the cap
	if (GetLevel() == maxlevel - 1) {
		uint32 expneeded = GetEXPForLevel(maxlevel);
		if (set_exp > expneeded) {
			set_exp = expneeded;
		}
	}

	//set the client's EXP and AAEXP
	m_pp.exp = set_exp;
	m_pp.expAA = set_aaxp;

	if ((GetLevel() != check_level) && !(check_level >= maxlevel))
	{
		char val1[20] = { 0 };
		if (level_increase)
		{
			if (level_count == 1)
			{
				Message_StringID(Chat::Yellow, GAIN_LEVEL, ConvertArray(check_level, val1));
				/* Message(Chat::Yellow, "You have gained a level! Welcome to level %i!", check_level); */
			}
			else
				Message(Chat::Yellow, "Welcome to level %i!", check_level);

			if (check_level == RuleI(Character, DeathItemLossLevel))
				Message_StringID(Chat::Yellow, CORPSE_ITEM_LOST);

			if (check_level == RuleI(Character, DeathExpLossLevel))
				Message_StringID(Chat::Yellow, CORPSE_EXP_LOST);

			if (check_level == 30)
			{
				if (GetClass() == MONK || GetClass() == BEASTLORD)
					Message_StringID(Chat::Yellow, HANDS_MAGIC);
				else if (GetClass() == WARRIOR)
					Message_StringID(Chat::Yellow, GAINED_SHIELD_LEVEL);
			}
		}
		else 
		{
			Message_StringID(Chat::Yellow, LOSE_LEVEL, ConvertArray(check_level, val1));
			/* Message(Chat::Yellow, "You lost a level! You are now level %i!", check_level); */
		}
		SetLevel(check_level);
	}

	if (GetLevel() < 51) {
		m_epp.perAA = 0;	// turn off aa exp if they drop below 51
	} else
		SendAAStats();	//otherwise, send them an AA update

	//send the expdata in any case so the xp bar isn't stuck after leveling
	uint32 tmpxp1 = GetEXPForLevel(GetLevel()+1);
	uint32 tmpxp2 = GetEXPForLevel(GetLevel());
	// Quag: crash bug fix... Divide by zero when tmpxp1 and 2 equalled each other, most likely the error case from GetEXPForLevel() (invalid class, etc)
	if (tmpxp1 != tmpxp2 && tmpxp1 != 0xFFFFFFFF && tmpxp2 != 0xFFFFFFFF) {
		auto outapp = new EQApplicationPacket(OP_ExpUpdate, sizeof(ExpUpdate_Struct));
		ExpUpdate_Struct* eu = (ExpUpdate_Struct*)outapp->pBuffer;
		float tmpxp = (float) ( (float) set_exp-tmpxp2 ) / ( (float) tmpxp1-tmpxp2 );
		eu->exp = (uint32)(330.0f * tmpxp);
		FastQueuePacket(&outapp);
	}
}

void Client::SetLevel(uint8 set_level, bool command)
{
	if (GetEXPForLevel(set_level) == 0xFFFFFFFF) {
		Log(Logs::General, Logs::Error, "Client::SetLevel() GetEXPForLevel(%i) = 0xFFFFFFFF", set_level);
		return;
	}

	if(IsMule())
		return;

	int original_level = level;
	int original_highest_level = m_pp.level2 > 0 ? m_pp.level2 : 1;

	auto outapp = new EQApplicationPacket(OP_LevelUpdate, sizeof(LevelUpdate_Struct));
	LevelUpdate_Struct* lu = (LevelUpdate_Struct*)outapp->pBuffer;

	level = set_level;

	if(IsRaidGrouped()) {
		Raid *r = this->GetRaid();
		if(r){
			r->UpdateLevel(GetName(), set_level);
		}
	}

	lu->level_old = original_highest_level;
	if(set_level > original_highest_level) {
		// the client only gives 5 skill points regardless of how many levels it gained, so we need to gain them one at a time to make that work
		int levels_gained = set_level - original_highest_level;
		if (levels_gained > 1)
		{
			for (int i = original_highest_level + 1; i < set_level; i++)
			{
				lu->level = i;
				lu->level_old = lu->level - 1; // this causes the client to grant skill points when it's less than the new level
				lu->exp = 330;
				QueuePacket(outapp);
			}
			lu->level_old = lu->level;
		}

		m_pp.points += 5 * levels_gained;
		m_pp.level2 = set_level;
	}

	if(set_level > m_pp.level) {
		parse->EventPlayer(EVENT_LEVEL_UP, this, "", 0);
		/* QS: PlayerLogLevels */
		if (RuleB(QueryServ, PlayerLogLevels)){
			std::string event_desc = StringFormat("Leveled UP :: to Level:%i from Level:%i in zoneid:%i", set_level, m_pp.level, this->GetZoneID());
			QServ->PlayerLogEvent(Player_Log_Levels, this->CharacterID(), event_desc); 
		}
	}
	else if (set_level < m_pp.level){
		/* QS: PlayerLogLevels */
		if (RuleB(QueryServ, PlayerLogLevels)){
			std::string event_desc = StringFormat("Leveled DOWN :: to Level:%i from Level:%i in zoneid:%i", set_level, m_pp.level, this->GetZoneID());
			QServ->PlayerLogEvent(Player_Log_Levels, this->CharacterID(), event_desc);
		}
	}

	m_pp.level = set_level;
	if (command){
		m_pp.exp = GetEXPForLevel(set_level);
		Message(Chat::Yellow, "Welcome to level %i!", set_level);
		lu->exp = 0;
	}
	else {
		float tmpxp = (float) ( (float) m_pp.exp - GetEXPForLevel( GetLevel() )) / ( (float) GetEXPForLevel(GetLevel()+1) - GetEXPForLevel(GetLevel()));
		lu->exp = (uint32)(330.0f * tmpxp);
	}
	lu->level = set_level;
	QueuePacket(outapp);
	safe_delete(outapp);
	this->SendAppearancePacket(AppearanceType::WhoLevel, set_level); // who level change

	Log(Logs::General, Logs::Normal, "Setting Level for %s to %i", GetName(), set_level);

	CalcBonuses();

	if(!RuleB(Character, HealOnLevel)) {
		int mhp = CalcMaxHP();
		if(GetHP() > mhp)
			SetHP(mhp);
	}
	else {
		SetHP(CalcMaxHP()); // Why not, lets give them a free heal
	}
	if(!RuleB(Character, ManaOnLevel))
	{
		int mp = CalcMaxMana();
		if(GetMana() > mp)
			SetMana(mp);
	}
	else
	{
		SetMana(CalcMaxMana()); // Why not, lets give them a free heal
	}

	SendHPUpdate();
	SendManaUpdate();
	UpdateWho();
	Save();
}

uint32 Client::GetEXPForLevel(uint16 check_level, bool aa)
{
	// Warning: Changing anything in this method WILL cause levels to change in-game the first time a player
	// gains or loses XP. 

	// Note: Sony's AA exp was 15,000,000 but they also reduced AA exp by 20%, so the effective AAExp was 18,750,000
	// This rule should be 18,750,000 to be non-custom since we do not reduce by 20%
	if (aa)
		return (RuleI(AA, ExpPerPoint));

	check_level -= 1;
	float base = (check_level)*(check_level)*(check_level);

	// Classes: In the XP formula AK used, they WERE calculated in. This was due to Sony not being able to change their XP
	// formula drastically (see above comment.) Instead, they gave the penalized classes a bonus on gain. We've decided to go
	// the easy route, and simply not use a class mod at all.

	float playermod = 10;
	uint8 race = GetBaseRace();
	if(race == HALFLING)
		playermod *= 95.0;
	else if(race == DARK_ELF || race == DWARF || race == ERUDITE || race == GNOME || 
		race == HALF_ELF || race == HIGH_ELF || race == HUMAN || race == WOOD_ELF ||
		race == VAHSHIR)
		playermod *= 100.0;
	else if(race == BARBARIAN)
		playermod *= 105.0;
	else if(race == OGRE)
		playermod *= 115.0;
	else if(race == IKSAR || race == TROLL)
		playermod *= 120.0;

	float mod;
	if (check_level <= 29)
		mod = 1.0;
	else if (check_level <= 34)
		mod = 1.1;
	else if (check_level <= 39)
		mod = 1.2;
	else if (check_level <= 44)
		mod = 1.3;
	else if (check_level <= 50)
		mod = 1.4;
	else if (check_level == 51)
		mod = 1.5;
	else if (check_level == 52)
		mod = 1.6;
	else if (check_level == 53)
		mod = 1.7;
	else if (check_level == 54)
		mod = 1.9;
	else if (check_level == 55)
		mod = 2.1;
	else if (check_level == 56)
		mod = 2.3;
	else if (check_level == 57)
		mod = 2.5;
	else if (check_level == 58)
		mod = 2.7;
	else if (check_level == 59)
		mod = 3.0;
	else if (check_level == 60)
		mod = 3.0;
	else if (check_level == 61)
		mod = 3.225;
	else if (check_level == 62)
		mod = 3.45;
	else if (check_level == 63)
		mod = 3.675;
	else if (check_level == 64)
		mod = 3.9;
	else
		mod = 4.125;

	uint32 finalxp = uint32(base * playermod * mod);

	return finalxp;
}

void Client::AddLevelBasedExp(uint8 exp_percentage, uint8 max_level) { 
	if (exp_percentage > 100) { exp_percentage = 100; } 
	if (!max_level || GetLevel() < max_level) { max_level = GetLevel(); } 
	uint32 newexp = GetEXP() + ((GetEXPForLevel(max_level + 1) - GetEXPForLevel(max_level)) * exp_percentage / 100); 
	SetEXP(newexp, GetAAXP());
}

void Group::SplitExp(uint32 exp, Mob* killed_mob) 
{
	if( killed_mob->CastToNPC()->MerchantType != 0 || killed_mob->GetClass() == BANKER) // Ensure NPC isn't a merchant
		return;

	if(killed_mob->GetOwner() && killed_mob->GetOwner()->IsClient() && !killed_mob->IsZomm()) // Ensure owner isn't pc
		return;

	GroupExpSplit_Struct gs;
	memset(&gs, 0, sizeof(GroupExpSplit_Struct));

	SetLevels();
	gs.max_green_level = 0;

	bool isgreen = false;
	int conlevel = Mob::GetLevelCon(maxlevel, killed_mob->GetLevel());
	if (conlevel == CON_GREEN)
	{
		isgreen = true;
	}

	//Give XP out to lower toons from NPCs that are green to the highest player.
	if(isgreen && !RuleB(AlKabor, GreensGiveXPToGroup) && !killed_mob->IsZomm())
		return;

	if (!ProcessGroupSplit(killed_mob, gs, isgreen))
		return;

	float groupmod = 1.0;

	int8 members = gs.close_membercount;
	if (RuleB(AlKabor, OutOfRangeGroupXPBonus))
		members = gs.membercount;

	if (RuleB(Quarm, BonusGroupEXPOverride))
	{
		// group bonus from Launch (Classic) until Jan 2001 (Velious, 1 Month In).
		switch (members)
		{
		case 2:
			groupmod = RuleR(Quarm, BonusGroupEXP2MemberOverride);
			break;
		case 3:
			groupmod = RuleR(Quarm, BonusGroupEXP3MemberOverride);
			break;
		case 4:
			groupmod = RuleR(Quarm, BonusGroupEXP4MemberOverride);
			break;
		case 5:
			groupmod = RuleR(Quarm, BonusGroupEXP5MemberOverride);
			break;
		case 6:
			groupmod = RuleR(Quarm, BonusGroupEXP6MemberOverride);
			break;
		case 7:
			groupmod = RuleR(Quarm, BonusGroupEXP7MemberOverride);
			break;
		}
	}
	else if (RuleB(AlKabor, ClassicGroupEXPBonuses))
	{
		// group bonus from Launch (Classic) until Jan 2001 (Velious, 1 Month In).
		switch (members)
		{
		case 2:
			groupmod = 1.02f;
			break;
		case 3:
			groupmod = 1.04f;
			break;
		case 4:
			groupmod = 1.06f;
			break;
		case 5:
			groupmod = 1.08f;
			break;
		case 6:
			groupmod = 1.1f;
			break;
		case 7:
			groupmod = 1.12f;
			break;
		}
	}
    else if (RuleB(AlKabor, VeliousGroupEXPBonuses))
	{
		// group bonus from Jan 2001 until June 2003. Technically, this is enabled the first month of Velious.
		switch (members)
		{
		case 2:
			groupmod = 1.02f;
			break;
		case 3:
			groupmod = 1.06f;
			break;
		case 4:
			groupmod = 1.10f;
			break;
		case 5:
			groupmod = 1.14f;
			break;
		case 6:
			groupmod = 1.2f;
			break;
		case 7:
			groupmod = 1.24f;
			break;
		}
	}
	else
	{
		// Use mid-Ykesha era or AK's unique bonuses

		if (members == 2)
			groupmod += 0.20;
		else if (members == 3)
			groupmod += 0.40;

		if (RuleB(AlKabor, GroupEXPBonuses))
		{
			// Use the "broken" bonuses on AK.
			if (members == 4)
				groupmod += 1.20;
			else if (members > 4)
				groupmod += 1.60;
		}
		else
		{
			if (members == 4)
				groupmod += 0.60;
			else if (members > 4)
				groupmod += 0.80;
		}
	}

	float groupexp = (static_cast<float>(exp) * groupmod) * RuleR(Character, GroupExpMultiplier);
	Log(Logs::Detail, Logs::Group, "Group Base XP: %d GroupMod: %0.2f Final XP: %0.2f", exp, groupmod, groupexp);

	// 6th member is free in the split under mid-Ykesha+ era rules, but not on AK or under classic rules
	if (!RuleB(AlKabor, Count6thGroupMember) && gs.close_membercount >= 6)
	{
		gs.weighted_levels -= minlevel;
	}

	GiveGroupSplitExp(killed_mob, maxlevel, gs.weighted_levels, conlevel, groupexp, gs.close_membercount);
}

bool Group::ProcessGroupSplit(Mob* killed_mob, struct GroupExpSplit_Struct& gs, bool isgreen)
{
	if (isgreen)
	{
		for (int x = 0; x < MAX_GROUP_MEMBERS; ++x)
		{
			if (members[x] != nullptr && members[x]->IsClient()) // If Group Member is Client
			{
				Client *cmember = members[x]->CastToClient();
				if (cmember->CastToClient()->GetZoneID() == zone->GetZoneID() && 
					cmember->IsInLevelRange(maxlevel) && 
					(cmember->GetLevelCon(killed_mob->GetLevel()) != CON_GREEN || killed_mob->IsZomm()))
				{
					// get highest level of player who gets exp from this mob
					if (cmember->GetLevel() > gs.max_green_level)
					{
						gs.max_green_level = cmember->GetLevel();
					}
				}
			}
		}

		if (gs.max_green_level < 1)
		{
			Log(Logs::Detail, Logs::Group, "Nobody qualifies for XP. Returning...");
			return false;
		}

		Log(Logs::Detail, Logs::Group, "Max green level was set to %d", gs.max_green_level);
	}

	for (int i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if (members[i] != nullptr && members[i]->IsClient()) // If Group Member is Client
		{
			Client *cmember = members[i]->CastToClient();
			if (cmember->CastToClient()->GetZoneID() == zone->GetZoneID())
			{
				++gs.membercount;

				if (cmember->IsInExpRange(killed_mob))
				{
					++gs.close_membercount;

					if (cmember->GetLevelCon(killed_mob->GetLevel()) == CON_GREEN && RuleB(AlKabor, GreenExpBonus))
					{
						// When a green is giving XP to the group, the players who don't get XP are considered equal to the highest player who does.
						// This is according to Placer.  No hard evidence of this is known, so this is iffy.
						// This results in higher level players being able to powerlevel lower level players by simply grouping with them and mass killing greens.
						// It would still be doable without this but this makes it worse.
						uint8 level = gs.max_green_level > 0 && cmember->GetLevel() > gs.max_green_level ? gs.max_green_level : cmember->GetLevel();
						gs.weighted_levels += level;
					}
					else
					{
						gs.weighted_levels += cmember->GetLevel();
					}
					Log(Logs::Detail, Logs::Group, "%s was added to close_membercount", cmember->GetName());
				}
			}
		}
	}

	if (RuleB(Quarm, EnablePetExperienceSplit))
	{
		if (killed_mob)
		{
			int32 damage_amount = 0;
			Mob* top_damager = killed_mob->GetDamageTopSingleMob(damage_amount);
			if (top_damager)
			{
				if (top_damager->IsPet())
				{
					float pet_dmg_pct = static_cast<float>(damage_amount) / killed_mob->total_damage;
					if (pet_dmg_pct > 0.5f)
					{
						++gs.membercount;
						++gs.close_membercount;
						gs.weighted_levels += top_damager->GetLevel();
						Log(Logs::General, Logs::EQMac, "%s was damaged more than 50 percent by a single pet. Pet was added to group experience weights.", killed_mob->GetCleanName());
					}
				}
			}
		}
	}

	if (gs.close_membercount == 0)
		return false;

	return true;
}

void Group::GiveGroupSplitExp(Mob* killed_mob, uint8 maxlevel, int16 weighted_levels, int conlevel, float groupexp, int8 close_count)
{

	// This loop figures out the split, and sends XP for each player that qualifies. (NPC is not green to the player, player is in the 
	// zone where the kill occurred, is in range of the corpse, and is in level range with the rest of the group.)
	for (int i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] != nullptr && members[i]->IsClient()) // If Group Member is Client
		{
			Client *cmember = members[i]->CastToClient();

			if (cmember->CastToClient()->GetZoneID() == zone->GetZoneID())
			{
				if (cmember->IsInExpRange(killed_mob))
				{
					if (cmember->GetLevelCon(killed_mob->GetLevel()) != CON_GREEN || killed_mob->IsZomm())
					{
						if (cmember->IsInLevelRange(maxlevel))
						{
							float split_percent = static_cast<float>(cmember->GetLevel() + 5u) / static_cast<float>(weighted_levels + 5*close_count);
							float splitgroupxp = groupexp * split_percent;
							if (splitgroupxp < 1)
								splitgroupxp = 1;

							int local_conlevel = conlevel;
							if (conlevel == CON_GREEN)
								local_conlevel = Mob::GetLevelCon(cmember->GetLevel(), killed_mob->GetLevel());

							Log(Logs::Detail, Logs::Group, "%s splits %0.2f with the rest of the group. Their share: %0.2f (%0.2f PERCENT)  weighted_levels: %i;  close_count: %i", cmember->GetName(), groupexp, splitgroupxp, split_percent * 100, weighted_levels, close_count);
							cmember->AddEXP(static_cast<uint32>(splitgroupxp), local_conlevel, killed_mob, weighted_levels/close_count, close_count == 1 ? false : true, maxlevel);
						}
						else
						{
							Log(Logs::Detail, Logs::Group, "%s is too low in level to gain XP from this group.", cmember->GetName());
						}
					}
					else
					{
						Log(Logs::Detail, Logs::Group, "%s is green to %s. They won't receive group XP.", killed_mob->GetName(), cmember->GetName());
					}
				}
				else
				{
					Log(Logs::Detail, Logs::Group, "%s is out of physical range. They won't receive group XP.", cmember->GetName());
				}
			}
			else 
			{
				Log(Logs::Detail, Logs::Group, "%s is not in the zone. They won't receive group XP.", cmember->GetName());
			}
		}
	}
}

void Raid::SplitExp(uint32 exp, Mob* killed_mob) 
{
	if (killed_mob->CastToNPC()->MerchantType != 0 || killed_mob->GetClass() == BANKER) // Ensure NPC isn't a merchant
		return;

	if (killed_mob->GetOwner() && killed_mob->GetOwner()->IsClient() && !killed_mob->IsZomm()) // Ensure owner isn't pc
		return;

	uint8 membercount = 0;
	uint16 maxlevel = 0;
	uint16 weighted_levels = 0;

	//Grabs membercount and maxlevel.
	for (int i = 0; i < MAX_RAID_MEMBERS; i++) 
	{
		if (members[i].member != nullptr) 
		{
			Client *cmember = members[i].member;
			if (cmember && cmember->GetZoneID() == zone->GetZoneID())
			{
				if (cmember->GetLevel() > maxlevel)
					maxlevel = cmember->GetLevel();

				if ((cmember->GetLevelCon(killed_mob->GetLevel()) != CON_GREEN || killed_mob->IsZomm()) &&
					cmember->IsInExpRange(killed_mob))
				{
					weighted_levels += cmember->GetLevel();
					++membercount;
				}
			}
		}
	}

	if (membercount == 0)
		return;

	//Check to make sure we're all in level range now that we know our maxlevel.
	for (int i = 0; i < MAX_RAID_MEMBERS; i++) 
	{
		if (members[i].member != nullptr)
		{
			Client *cmember = members[i].member;
			if(cmember && !cmember->IsInLevelRange(maxlevel))
			{
				if(membercount != 0)
				{
					Log(Logs::Detail, Logs::Group, "%s is not within level range, removing from XP gain.", cmember->GetName());
					if(weighted_levels >= cmember->GetLevel())
						weighted_levels -= cmember->GetLevel();
					--membercount;
				}
				else
					return;
			}
		}
	}

	if (membercount == 0)
		return;

	float groupexp = static_cast<float>(exp) * RuleR(Character, RaidExpMultiplier);
	int conlevel = Mob::GetLevelCon(maxlevel, killed_mob->GetLevel());
	Log(Logs::Detail, Logs::Group, "Raid XP: %d Final XP: %0.2f", exp, groupexp);

	//Assigns XP if the qualifications are met.
	for (int i = 0; i < MAX_RAID_MEMBERS; i++) 
	{
		if (members[i].member != nullptr)
		{
			Client *cmember = members[i].member;
			if(cmember && cmember->GetZoneID() == zone->GetZoneID() && 
				(cmember->GetLevelCon(killed_mob->GetLevel()) != CON_GREEN || killed_mob->IsZomm()) && 
				cmember->IsInExpRange(killed_mob))
			{
				if (cmember->IsInLevelRange(maxlevel))
				{
					float split_percent = static_cast<float>(cmember->GetLevel()) / weighted_levels;
					float splitgroupxp = groupexp * split_percent;
					if (splitgroupxp < 1)
						splitgroupxp = 1;

					cmember->AddEXP(static_cast<uint32>(splitgroupxp), conlevel, killed_mob, 0, true, maxlevel);
					Log(Logs::Detail, Logs::Group, "%s splits %0.2f with %d players in the raid. Their share is %0.2f", cmember->GetName(), groupexp, membercount, splitgroupxp);
				}
				else
					Log(Logs::Detail, Logs::Group, "%s is too low in level to gain XP from this raid.", cmember->GetName());
			}
			else
				Log(Logs::Detail, Logs::Group, "%s is not in the kill zone, is out of range, or %s is green to them. They won't receive raid XP.", cmember->GetName(), killed_mob->GetCleanName());
		}
	}
}

uint32 Client::GetCharMaxLevelFromQGlobal() {
	QGlobalCache *char_c = nullptr;
	char_c = this->GetQGlobals();

	std::list<QGlobal> globalMap;
	uint32 ntype = 0;

	if(char_c) {
		QGlobalCache::Combine(globalMap, char_c->GetBucket(), ntype, this->CharacterID(), zone->GetZoneID());
	}

	auto iter = globalMap.begin();
	uint32 gcount = 0;
	while(iter != globalMap.end()) {
		if((*iter).name.compare("CharMaxLevel") == 0){
			return atoi((*iter).value.c_str());
		} 
		++iter;
		++gcount;
	}

	return false;
}

uint32 Client::GetCharMaxLevelFromBucket() {
	uint32 char_id = this->CharacterID();
	std::string query = StringFormat("SELECT value FROM data_buckets WHERE key = '%i-CharMaxLevel'", char_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::General, Logs::Error, "Data bucket for CharMaxLevel for char ID %i failed.", char_id);
		return false;
	}

	if (results.RowCount() > 0) {
		auto row = results.begin();
		return atoi(row[0]);
	}
	return false;
}

bool Client::IsInExpRange(Mob* defender)
{
	if(IsMule())
		return false;

	float exprange = RuleR(Zone, GroupEXPRange);

	float t1, t2;
	t1 = defender->GetX() - GetX();
	t2 = defender->GetY() - GetY();
	
	if (t1 > exprange || t2 > exprange || t1 < -exprange || t2 < -exprange) {
		//_log(CLIENT__EXP, "%s is out of range. distances (%.3f,%.3f,%.3f), range %.3f No XP will be awarded.", defender->GetName(), t1, t2, t3, exprange);
		return false;
	}
	else
		return true;
}

bool Client::IsInLevelRange(uint8 maxlevel)
{
	if(IsMule())
		return false;
	
	// EQ supposedly had a minimum group range at very low levels.  What this should be is not known exactly.
	// The EQ Official Player's Guide says it was 3 but a PoP era log shows a level 10 grouping with a level 6.
	// I have a hunch they may have enlarged it in Luclin along with the newbie changes.  Setting this to 4 for now
	if (maxlevel < 6u)	// allow level 1s to group with level 5s
		return true;
	else if (maxlevel < 10u && GetLevel() > (maxlevel - 5u))	// allow a minimum of a -4 difference
		return true;
	else if (GetLevel() >= (maxlevel * 10u / 15u))	// this rounding allows a level 40 to group with a 61 which is correct.  some sources online are inaccurate
		return true;

	return false;
}

void Client::GetExpLoss(Mob* killerMob, uint16 spell, int &exploss, uint8 killedby)
{
	if (GetLevel() < 2)
	{
		exploss = 0;
		return;
	}
	int last_level_xp = GetEXPForLevel(GetLevel()) - GetEXPForLevel(GetLevel() - 1);
	if (GetLevel() >= 25)
		exploss = last_level_xp / 4;
	else
		exploss = last_level_xp * GetLevel() / 100;

	exploss /= 2;						// death loss was cut in half on May 24 1999
	exploss = exploss * 9 / 10;			// death loss was based on warrior exp.  since we don't have class bonuses or penalties, everybody is reduced here
	if (exploss > 6000000)
		exploss = 6000000;

	if (GetLevel() == 31 || GetLevel() == 36 || GetLevel() == 41 || GetLevel() == 46)
		exploss /= 2;					// these post-hell levels had double exp loss until March 19 2002

	exploss = static_cast<int>(static_cast<float>(exploss) * RuleR(Character, EXPLossMultiplier));

	// Death exp loss started at level 6 until March 19 2002, then it was 11
	if( (level < RuleI(Character, DeathExpLossLevel)) || (level > RuleI(Character, DeathExpLossMaxLevel)) || IsBecomeNPC() )
	{
		exploss = 0;
	}
	else if( killerMob && spell != 940 ) //ManaConvert
	{

		if( killerMob->IsClient() )
		{
			exploss = 0;
		}
		else if( killerMob->GetOwner() && killerMob->GetOwner()->IsClient() )
		{
			exploss = 0;
		}
	}

	if (spell == 940) // ManaConvert causes EXP loss. Sorry.
		return;

	if (killedby == Killed_DUEL || killedby == Killed_PVP || killedby == Killed_Self)
	{
		exploss = 0;
	}

	if(spell != SPELL_UNKNOWN)
	{
		uint32 buff_count = GetMaxTotalSlots();
		for(uint16 buffIt = 0; buffIt < buff_count; buffIt++)
		{
			if(buffs[buffIt].spellid == spell && buffs[buffIt].client)
			{
				exploss = 0;	// no exp loss for pvp dot
				break;
			}
		}
	}
}

// This will return a multiplier used to reduce experience gains when the NPC meets the PBAoE reduction critera
float NPC::GetPBAoEReduction(uint8 killer_level)
{
	if (killer_level < 42 || GetLevel() > 55)
		return 1.0;

	int level_diff = killer_level - GetLevel();

	// this is a crude approximation of Sony's logic.  it's near impossible to figure out from adding up log kills
	int x = 5;
	int y = 72;

	if (killer_level < 51)
	{
		x = 4;
		y = 70;
	}

	float new_pct = static_cast<float>((1000 - ((level_diff - x) * y))) / 1000.0f;

	if (new_pct < 0.05)
		new_pct = 0.05;

	if (new_pct > 0.75 && GetLevel() > 50)
		new_pct = 0.75;
	else if (new_pct > 1.0)
		new_pct = 1.0; // sanity

	return new_pct;
}
