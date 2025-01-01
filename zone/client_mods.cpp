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
#include "../common/eqemu_logsys.h"

#include "../common/rulesys.h"
#include "../common/spdat.h"

#include "client.h"
#include "mob.h"

#include <algorithm>


int32 Client::GetMaxStat(int32 aabonusAmount) const {

	if((RuleI(Character, StatCap)) > 0)
		return (RuleI(Character, StatCap));

	if (GetLevel() <= 60)
		return 255;
	else
		return 255 + (GetLevel() - 60) * 5 + aabonusAmount;
}

int32 Client::GetMaxResist() const
{
	return 500;
}

int32 Client::GetMaxSTR() const {
	return GetMaxStat(aabonuses.STRCapMod);
}
int32 Client::GetMaxSTA() const {
	return GetMaxStat(aabonuses.STACapMod);
}
int32 Client::GetMaxDEX() const {
	return GetMaxStat(aabonuses.DEXCapMod);
}
int32 Client::GetMaxAGI() const {
	return GetMaxStat(aabonuses.AGICapMod);
}
int32 Client::GetMaxINT() const {
	return GetMaxStat(aabonuses.INTCapMod);
}
int32 Client::GetMaxWIS() const {
	return GetMaxStat(aabonuses.WISCapMod);
}
int32 Client::GetMaxCHA() const {
	return GetMaxStat(aabonuses.CHACapMod);
}
int32 Client::GetMaxMR() const {
	return GetMaxResist() + aabonuses.MRCapMod;
}
int32 Client::GetMaxPR() const {
	return GetMaxResist() + aabonuses.PRCapMod;
}
int32 Client::GetMaxDR() const {
	return GetMaxResist() + aabonuses.DRCapMod;
}
int32 Client::GetMaxCR() const {
	return GetMaxResist() + aabonuses.CRCapMod;
}
int32 Client::GetMaxFR() const {
	return GetMaxResist() + aabonuses.FRCapMod;
}

int32 Client::LevelRegen(int level, bool is_sitting, bool is_resting, bool is_feigned, bool is_famished, bool has_racial_regen_bonus)
{
	// base regen is 1
	int hp_regen_amount = 1;

	// sitting adds 1
	if (is_sitting)
	{
		hp_regen_amount += 1;

		if (level >= 20)
		{
			hp_regen_amount += 1;
		}
		if (level >= 50)
		{
			hp_regen_amount += 1;
		}
	}

	// feigning at 51+ adds 1 as if sitting
	if (level > 50 && is_feigned)
	{
		hp_regen_amount += 1;
	}

	// being either hungry or thirsty negates the passive regen from standing/sitting/feigning but can still benefit from level bonuses and resting
	if (is_famished)	// the client checks to see if either food or water is 0 in pp
	{
		hp_regen_amount = 0;
	}

	// additional point of regen is gained at levels 51, 56, 60, 61, 63 and 65
	if (level >= 51)
	{
		hp_regen_amount += 1;
	}
	if (level >= 56)
	{
		hp_regen_amount += 1;
	}
	if (level >= 60)
	{
		hp_regen_amount += 1;
	}
	if (level >= 61)
	{
		hp_regen_amount += 1;
	}
	if (level >= 63)
	{
		hp_regen_amount += 1;
	}
	if (level >= 65)
	{
		hp_regen_amount += 1;
	}

	// racial trait adds to then doubles regen bonuses
	if (has_racial_regen_bonus)
	{
		if (level >= 51)
		{
			hp_regen_amount += 1;
		}
		if (level >= 56)
		{
			hp_regen_amount += 1;
		}

		hp_regen_amount *= 2;
	}

	return hp_regen_amount;
}

// based on client decompile
int32 Client::CalcHPRegen()
{
	bool is_sitting = IsSitting() || (GetHorseId() != 0 && !auto_attack && !ClientMoving());
	bool has_racial_regen_bonus = GetPlayerRaceBit(GetBaseRace()) & RuleI(Character, BaseHPRegenBonusRaces);
	bool is_feigned = GetClass() == Class::Monk && IsFeigned();	// only monks get this bonus

	// naked regen
	int32 hp_regen_amount = LevelRegen(GetLevel(), is_sitting, IsRested(), is_feigned, Famished(), has_racial_regen_bonus);

	// add AA regen - this is here because of the check below needing to negate it so we can bleed out in sync with the client
	hp_regen_amount += aabonuses.HPRegen;

	// we're almost dead, our regeneration won't save us now but a heal could
	if (GetHP() <= 0)
	{
		if (hp_regen_amount <= 0 || GetHP() < -5)
		{
			// bleed to death slowly
			hp_regen_amount = -1;
		}
	}

	// add spell and item regen
	hp_regen_amount += itembonuses.HPRegen + spellbonuses.HPRegen;

	// special case, if we're unconscious and our hp isn't changing, make it -1 so the character doesn't end up stuck in that state
	// this only applies if the character ends up with between -5 and 0 hp, then once they reach -6 they will hit the normal bleeding logic
	if (GetHP() <= 0 && hp_regen_amount == 0)
	{
		hp_regen_amount = -1;
	}

	return hp_regen_amount;
}

int32 Client::CalcHPRegenCap()
{
	int base = 30;
	if (level > 60)
		base = level - 30;

	return base;
}

int32 Client::CalcMaxHP(bool unbuffed) {
	int32 val = (CalcBaseHP(unbuffed) + itembonuses.HP);

	//The AA desc clearly says it only applies to base hp..
	//but the actual effect sent on live causes the client
	//to apply it to (basehp + itemhp).. I will oblige to the client's whims over
	//the aa description

	// Natural Durability and Physical Enhancement
	double nd = 0.0;
	int nd_level = GetAA(aaNaturalDurability);
	if (nd_level != 0)
	{
		switch (nd_level)
		{
			case 1: nd = 2.0; break;
			case 2: nd = 5.0; break;
			case 3: nd = 10.0; break;
		}
		// Physical Enhancement only functions if you have at least 1 point in Natural Durability
		int pe_level = GetAA(aaPhysicalEnhancement);
		if (pe_level != 0)
		{
			nd += 2.0;
		}
	}
	
	// Planar Durability - WAR PAL SHD ability
	// Note that de-leveling below 60 after getting this ability causes you to lose hitpoints
	int pd_level = GetAA(aaPlanarDurability);
	if (pd_level != 0)
	{
		int levels_over_60 = GetLevel() - 60;
		pd_level = pd_level > levels_over_60 ? levels_over_60 : pd_level;
		if (GetClass() == Class::Warrior)
		{
			nd += 1.5 * pd_level;
		}
		else if (GetClass() == Class::Paladin || GetClass() == Class::ShadowKnight)
		{
			nd += 1.25 * pd_level;
		}
	}

	// the results can end up being slightly off between the server, the mac client and the windows client due to floating point precision
	// there are cases where the clients end up with different hp totals for the same character, but have only seen it off by 1 hp
	val += (int32)(val * nd / 100.0);
	val += aabonuses.HP + 5;
	if (!unbuffed)
		val += spellbonuses.HP;

	// this MaxHPChange bonus is only used in AAs on TAKP, not in items or spells, so this line doesn't do anything
	//val += val * ((spellbonuses.MaxHPChange + itembonuses.MaxHPChange) / 10000.0f);

	if(val > 32767)
		val = 32767;

	if (!unbuffed)
	{
		if (cur_hp > val)
			cur_hp = val;
		max_hp = val;
	}

	return val;
}

uint32 Mob::GetClassLevelFactor(){
	uint32 multiplier = 0;
	uint8 mlevel=GetLevel();
	switch(GetClass())
	{
		case Class::Warrior:{
			if (mlevel < 20)
				multiplier = 220;
			else if (mlevel < 30)
				multiplier = 230;
			else if (mlevel < 40)
				multiplier = 250;
			else if (mlevel < 53)
				multiplier = 270;
			else if (mlevel < 57)
				multiplier = 280;
			else if (mlevel < 60)
				multiplier = 290;
			else if (mlevel < 70)
				multiplier = 300;
			else
				multiplier = 311;
			break;
		}
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:{
			if (mlevel < 70)
				multiplier = 150;
			else
				multiplier = 157;
			break;
		}
		case Class::Paladin:
		case Class::ShadowKnight:{
			if (mlevel < 35)
				multiplier = 210;
			else if (mlevel < 45)
				multiplier = 220;
			else if (mlevel < 51)
				multiplier = 230;
			else if (mlevel < 56)
				multiplier = 240;
			else if (mlevel < 60)
				multiplier = 250;
			else if (mlevel < 68)
				multiplier = 260;
			else
				multiplier = 270;
			break;
		}
		case Class::Monk:
		case Class::Bard:
		case Class::Rogue:
		case Class::Beastlord:{
			if (mlevel < 51)
				multiplier = 180;
			else if (mlevel < 58)
				multiplier = 190;
			else if (mlevel < 70)
				multiplier = 200;
			else
				multiplier = 210;
			break;
		}
		case Class::Ranger:{
			if (mlevel < 58)
				multiplier = 200;
			else if (mlevel < 70)
				multiplier = 210;
			else
				multiplier = 220;
			break;
		}
		case Class::Magician:
		case Class::Wizard:
		case Class::Necromancer:
		case Class::Enchanter:{
			if (mlevel < 70)
				multiplier = 120;
			else
				multiplier = 127;
			break;
		}
		default:{
			if (mlevel < 35)
				multiplier = 210;
			else if (mlevel < 45)
				multiplier = 220;
			else if (mlevel < 51)
				multiplier = 230;
			else if (mlevel < 56)
				multiplier = 240;
			else if (mlevel < 60)
				multiplier = 250;
			else
				multiplier = 260;
			break;
		}
	}
	return multiplier;
}

int32 Client::CalcBaseHP(bool unbuffed)
{
	int32 lm = GetClassLevelFactor() / 10;
	int32 level_hp = GetLevel() * lm;
	int32 sta_factor = GetSTA();
	if (unbuffed) // web stats export uses this
	{
		sta_factor = CalcSTA(unbuffed);
	}
	if (sta_factor > 255)
	{
		sta_factor = (sta_factor - 255) / 2 + 255;
	}
	int32 val = level_hp * sta_factor / 300 + level_hp;
	if (!unbuffed)
		base_hp = val;

	return val;
}

// This should return the combined AC of all the items the player is wearing.
int32 Client::GetRawItemAC() {
	int32 Total = 0;

	// this skips SlotAmmo..add an '=' conditional if that slot is required (original behavior)
	for (int16 slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id < EQ::invslot::EQUIPMENT_END; slot_id++) {
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst && inst->IsType(EQ::item::ItemClassCommon)) {
			Total += inst->GetItem()->AC;
		}
	}

	return Total;
}

int32 Client::acmod() {
	int agility = GetAGI();
	int level = GetLevel();
	if(agility < 1 || level < 1)
		return(0);

	if (agility <=74){
		if (agility == 1)
			return -24;
		else if (agility <=3)
			return -23;
		else if (agility == 4)
			return -22;
		else if (agility <=6)
			return -21;
		else if (agility <=8)
			return -20;
		else if (agility == 9)
			return -19;
		else if (agility <=11)
			return -18;
		else if (agility == 12)
			return -17;
		else if (agility <=14)
			return -16;
		else if (agility <=16)
			return -15;
		else if (agility == 17)
			return -14;
		else if (agility <=19)
			return -13;
		else if (agility == 20)
			return -12;
		else if (agility <=22)
			return -11;
		else if (agility <=24)
			return -10;
		else if (agility == 25)
			return -9;
		else if (agility <=27)
			return -8;
		else if (agility == 28)
			return -7;
		else if (agility <=30)
			return -6;
		else if (agility <=32)
			return -5;
		else if (agility == 33)
			return -4;
		else if (agility <=35)
			return -3;
		else if (agility == 36)
			return -2;
		else if (agility <=38)
			return -1;
		else if (agility <=65)
			return 0;
		else if (agility <=70)
			return 1;
		else if (agility <=74)
			return 5;
	}
	else if(agility <= 137) {
		if (agility == 75){
			if (level <= 6)
				return 9;
			else if (level <= 19)
				return 23;
			else if (level <= 39)
				return 33;
			else
				return 39;
		}
		else if (agility >= 76 && agility <= 79){
			if (level <= 6)
				return 10;
			else if (level <= 19)
				return 23;
			else if (level <= 39)
				return 33;
			else
				return 40;
		}
		else if (agility == 80){
			if (level <= 6)
				return 11;
			else if (level <= 19)
				return 24;
			else if (level <= 39)
				return 34;
			else
				return 41;
		}
		else if (agility >= 81 && agility <= 85){
			if (level <= 6)
				return 12;
			else if (level <= 19)
				return 25;
			else if (level <= 39)
				return 35;
			else
				return 42;
		}
		else if (agility >= 86 && agility <= 90){
			if (level <= 6)
				return 12;
			else if (level <= 19)
				return 26;
			else if (level <= 39)
				return 36;
			else
				return 42;
		}
		else if (agility >= 91 && agility <= 95){
			if (level <= 6)
				return 13;
			else if (level <= 19)
				return 26;
			else if (level <= 39)
				return 36;
			else
				return 43;
		}
		else if (agility >= 96 && agility <= 99){
			if (level <= 6)
				return 14;
			else if (level <= 19)
				return 27;
			else if (level <= 39)
				return 37;
			else
				return 44;
		}
		else if (agility == 100 && level >= 7){
			if (level <= 19)
				return 28;
			else if (level <= 39)
				return 38;
			else
				return 45;
		}
		else if (level <= 6) {
			return 15;
		}
		//level is >6
		else if (agility >= 101 && agility <= 105){
			if (level <= 19)
				return 29;
			else if (level <= 39)
				return 39;// not verified
			else
				return 45;
		}
		else if (agility >= 106 && agility <= 110){
			if (level <= 19)
				return 29;
			else if (level <= 39)
				return 39;// not verified
			else
				return 46;
		}
		else if (agility >= 111 && agility <= 115){
			if (level <= 19)
				return 30;
			else if (level <= 39)
				return 40;// not verified
			else
				return 47;
		}
		else if (agility >= 116 && agility <= 119){
			if (level <= 19)
				return 31;
			else if (level <= 39)
				return 41;
			else
				return 47;
		}
		else if (level <= 19) {
				return 32;
		}
		//level is > 19
		else if (agility == 120){
			if (level <= 39)
				return 42;
			else
				return 48;
		}
		else if (agility <= 125){
			if (level <= 39)
				return 42;
			else
				return 49;
		}
		else if (agility <= 135){
			if (level <= 39)
				return 42;
			else
				return 50;
		}
		else {
			if (level <= 39)
				return 42;
			else
				return 51;
		}
	} else if(agility <= 300) {
		if(level <= 6) {
			if(agility <= 139)
				return(21);
			else if(agility == 140)
				return(22);
			else if(agility <= 145)
				return(23);
			else if(agility <= 150)
				return(23);
			else if(agility <= 155)
				return(24);
			else if(agility <= 159)
				return(25);
			else if(agility == 160)
				return(26);
			else if(agility <= 165)
				return(26);
			else if(agility <= 170)
				return(27);
			else if(agility <= 175)
				return(28);
			else if(agility <= 179)
				return(28);
			else if(agility == 180)
				return(29);
			else if(agility <= 185)
				return(30);
			else if(agility <= 190)
				return(31);
			else if(agility <= 195)
				return(31);
			else if(agility <= 199)
				return(32);
			else if(agility <= 219)
				return(33);
			else if(agility <= 239)
				return(34);
			else
				return(35);
		} else if(level <= 19) {
			if(agility <= 139)
				return(34);
			else if(agility == 140)
				return(35);
			else if(agility <= 145)
				return(36);
			else if(agility <= 150)
				return(37);
			else if(agility <= 155)
				return(37);
			else if(agility <= 159)
				return(38);
			else if(agility == 160)
				return(39);
			else if(agility <= 165)
				return(40);
			else if(agility <= 170)
				return(40);
			else if(agility <= 175)
				return(41);
			else if(agility <= 179)
				return(42);
			else if(agility == 180)
				return(43);
			else if(agility <= 185)
				return(43);
			else if(agility <= 190)
				return(44);
			else if(agility <= 195)
				return(45);
			else if(agility <= 199)
				return(45);
			else if(agility <= 219)
				return(46);
			else if(agility <= 239)
				return(47);
			else
				return(48);
		} else if(level <= 39) {
			if(agility <= 139)
				return(44);
			else if(agility == 140)
				return(45);
			else if(agility <= 145)
				return(46);
			else if(agility <= 150)
				return(47);
			else if(agility <= 155)
				return(47);
			else if(agility <= 159)
				return(48);
			else if(agility == 160)
				return(49);
			else if(agility <= 165)
				return(50);
			else if(agility <= 170)
				return(50);
			else if(agility <= 175)
				return(51);
			else if(agility <= 179)
				return(52);
			else if(agility == 180)
				return(53);
			else if(agility <= 185)
				return(53);
			else if(agility <= 190)
				return(54);
			else if(agility <= 195)
				return(55);
			else if(agility <= 199)
				return(55);
			else if(agility <= 219)
				return(56);
			else if(agility <= 239)
				return(57);
			else
				return(58);
		} else {	//lvl >= 40
			if(agility <= 139)
				return(51);
			else if(agility == 140)
				return(52);
			else if(agility <= 145)
				return(53);
			else if(agility <= 150)
				return(53);
			else if(agility <= 155)
				return(54);
			else if(agility <= 159)
				return(55);
			else if(agility == 160)
				return(56);
			else if(agility <= 165)
				return(56);
			else if(agility <= 170)
				return(57);
			else if(agility <= 175)
				return(58);
			else if(agility <= 179)
				return(58);
			else if(agility == 180)
				return(59);
			else if(agility <= 185)
				return(60);
			else if(agility <= 190)
				return(61);
			else if(agility <= 195)
				return(61);
			else if(agility <= 199)
				return(62);
			else if(agility <= 219)
				return(63);
			else if(agility <= 239)
				return(64);
			else
				return(65);
		}
	}
	else{
		//seems about 21 agil per extra AC pt over 300...
	return (65 + ((agility-300) / 21));
	}

	Log(Logs::Detail, Logs::Error, "Error in Client::acmod(): Agility: %i, Level: %i", agility, level);

	return 0;
};

int32 Client::CalcAC()
{
	// this is the value displayed in clients (it ignores the softcap) and is not used in combat calculations
	AC = (GetAvoidance(true) + GetMitigation(true)) * 1000 / 847;
	return AC;
}

int32 Client::CalcMaxMana()
{
	switch(GetCasterClass())
	{
		case 'I':
		case 'W': {
			if ((GetClass() == Class::Ranger || GetClass() == Class::Paladin || GetClass() == Class::Beastlord) && GetLevel() < 9)
				max_mana = 0;
			else
				max_mana = (CalcBaseMana() + itembonuses.Mana + spellbonuses.Mana);
			break;
		}
		case 'N': {
			max_mana = 0;
			break;
		}
		default: {
			Log(Logs::Detail, Logs::Spells, "Invalid Class '%c' in CalcMaxMana", GetCasterClass());
			max_mana = 0;
			break;
		}
	}
	if (max_mana < 0) {
		max_mana = 0;
	}

	if(max_mana > 32767)
		max_mana = 32767;

	if (cur_mana > max_mana) {
		cur_mana = max_mana;
	}
	#if EQDEBUG >= 11
		Log(Logs::Detail, Logs::Spells, "Client::CalcMaxMana() called for %s - returning %d", GetName(), max_mana);
	#endif
	return max_mana;
}

int32 Client::CalcBaseMana()
{
	int WisInt = 0;
	int MindLesserFactor, MindFactor;
	int32 max_m = 0;
	int wisint_mana = 0;
	int base_mana = 0;
	int ConvertedWisInt = 0;
	switch(GetCasterClass())
	{
		case 'I':
			WisInt = GetINT();

				if((( WisInt - 199 ) / 2) > 0)
					MindLesserFactor = ( WisInt - 199 ) / 2;
				else
					MindLesserFactor = 0;

				MindFactor = WisInt - MindLesserFactor;
				if(WisInt > 100)
					max_m = (((5 * (MindFactor + 20)) / 2) * 3 * GetLevel() / 40);
				else
					max_m = (((5 * (MindFactor + 200)) / 2) * 3 * GetLevel() / 100);
			break;

		case 'W':
			WisInt = GetWIS();


				if((( WisInt - 199 ) / 2) > 0)
					MindLesserFactor = ( WisInt - 199 ) / 2;
				else
					MindLesserFactor = 0;

				MindFactor = WisInt - MindLesserFactor;
				if(WisInt > 100)
					max_m = (((5 * (MindFactor + 20)) / 2) * 3 * GetLevel() / 40);
				else
					max_m = (((5 * (MindFactor + 200)) / 2) * 3 * GetLevel() / 100);
			break;

		case 'N': {
			max_m = 0;
			break;
		}
		default: {
			Log(Logs::General, Logs::Spells, "Invalid Class '%c' in CalcMaxMana", GetCasterClass());
			max_m = 0;
			break;
		}
	}

#if EQDEBUG >= 11
	Log(Logs::General, Logs::Spells, "Client::CalcBaseMana() called for %s - returning %d", GetName(), max_m);
#endif
	return max_m;
}

int32 Client::CalcManaRegen(bool meditate)
{
	/*
		- base regen
			famished = 0
			not famished, standing = 1
			not famished, sitting/horse = 2

		- meditate
			non bard, sitting/horse, has meditate skill
				skill <=1 = 3 (doesn't happen during normal play due to skills starting higher when trained)
				skill >1 = 4 + skill / 15

		- AA
			mental clarity = add 0-3
			body and mind rejuvenation = add 0-1

		- level
			level >61 = add 1
			level >63 = add 1
		
		- worn items
			items = add 0-15
			donal's buff = add 0-1
	*/

	if (CalcMaxMana() == 0)
		return 0;

	uint8 clevel = GetLevel();

	int32 regen = 0;
	bool new_medding_state = false;

	// standing/sitting passive regen stops when famished
	if (!Famished())
	{
		regen = 1; // passive standing regen when not famished

		if (IsSitting() || (GetHorseId() != 0 && !auto_attack && !ClientMoving()))
		{
			regen = 2; // passive sitting regen when not famished

			// bards cannot meditate even though they have the skill
			if (GetClass() != Class::Bard && HasSkill(EQ::skills::SkillMeditate))
			{
				// matches client logic for an edge case
				// happens with skill level 0 or 1 but normally training the skill at level 4/8/12 will put it higher anyway
				// except bards, they get only 1 point, but they can't meditate so they don't hit this condition either
				regen = 3;
				new_medding_state = true;

				if (GetSkill(EQ::skills::SkillMeditate) > 1)
				{
					regen = 4 + GetSkill(EQ::skills::SkillMeditate) / 15;	// meditate regen is 20 for characters with maximum skill
				}
			}
		}
	}

	this->medding = new_medding_state; // turn off medding flag if it didn't get set above (famished or not sitting/mounted)
	if (this->medding && meditate)
	{
		CheckIncreaseSkill(EQ::skills::SkillMeditate, nullptr, zone->skill_difficulty[EQ::skills::SkillMeditate].difficulty[GetClass()]);
	}
	
	// level regen bonus applies even when famished
	if (clevel > 61)
	{
		regen += 1;
	}
	if (clevel > 63)
	{
		regen += 1;
	}

	//AAs
	regen += aabonuses.ManaRegen + spellbonuses.ManaRegen + itembonuses.ManaRegen;

	// Donal's BP leaves a 7.5 min residual effect (extended by buff duration extension AAs) which provides 1 point of mana regen that is separate from spell bonuses and flowing thought
	if (GetBuffSlotFromType(SE_CompleteHeal) >= 0)
	{
		regen += 1;
	}

	return regen;
}

int32 Client::CalcManaRegenCap()
{
	if(GetCasterClass() == 'N')
		return 0;

	int32 cap = RuleI(Character, ItemManaRegenCap);

	return cap;
}

uint32 Client::CalcCurrentWeight() {

	const EQ::ItemInstance* ins = nullptr;
	uint32 Total = 0;
	int x;
	for (x = EQ::invslot::slotCursor; x <= EQ::invslot::GENERAL_END; x++)
	{
		ins = GetInv().GetItem(x);
		if (ins)
		{
			Total += ins->GetItem()->Weight;
		}

		const EQ::ItemInstance* baginst = ins;
		const EQ::ItemInstance* inst = nullptr;
		if (baginst && baginst->GetItem() && baginst->IsClassBag())
		{
			uint32 bag_weight = 0;
			int i;
			for (i = 0; i < baginst->GetItem()->BagSlots; i++) 
			{
				inst = GetInv().GetItem(m_inv.CalcSlotId(x, i));
				if (inst)
				{
					bag_weight += inst->GetItem()->Weight;
				}
			}

			int reduction = baginst->GetItem()->BagWR;
			if (reduction > 0 && bag_weight > 0)
				bag_weight -= bag_weight*reduction/100;

			Total += bag_weight;
		}
	}

	uint32 coin_weight = (m_pp.platinum + m_pp.gold + m_pp.silver + m_pp.copper) / 4;

	// Coin purses only work when in a main inventory slot
	const EQ::ItemInstance* cins = nullptr;
	uint32 coin_reduction = 0;
	for (x = EQ::invslot::GENERAL_BEGIN; x <= EQ::invslot::GENERAL_END; x++)
	{
		cins = GetInv().GetItem(x);
		if (cins && cins->GetID() >= 17201 && cins->GetID() <= 17230)
		{
			int reduction = cins->GetItem()->BagWR;
			coin_reduction = reduction > coin_reduction ? reduction : coin_reduction;
		}
	}

	coin_weight -= coin_weight*coin_reduction/100;
	Total += coin_weight;
	return Total;
}

int32 Client::CalcAlcoholPhysicalEffect()
{
	if(m_pp.intoxication <= 55)
		return 0;

	return (m_pp.intoxication - 40) / 16;
}

int32 Client::CalcSTR() {
	int32 val = m_pp.STR + itembonuses.STR + spellbonuses.STR + CalcAlcoholPhysicalEffect() - CalculateFatiguePenalty();

	if (CalcAlcoholPhysicalEffect() == 0 && GetClass() == Class::Warrior && IsBerserk())
	{
		val += GetLevel() / 10 + 9;
	}

	int32 mod = aabonuses.STR;

	STR = val + mod;

	if(STR < 1)
		STR = 1;

	int m = GetMaxSTR();
	if(STR > m)
		STR = m;

	return(STR);
}

int32 Client::CalcSTA(bool unbuffed) {
	int32 val = m_pp.STA + itembonuses.STA; 
	if(!unbuffed)
		val += spellbonuses.STA + CalcAlcoholPhysicalEffect();

	int32 mod = aabonuses.STA;

	int32 newSTA = val + mod;

	if(newSTA < 1)
		newSTA = 1;

	int m = GetMaxSTA();
	if(newSTA > m)
		newSTA = m;

	if (!unbuffed)
		STA = newSTA;

	return(newSTA);
}

int32 Client::CalcAGI() {
	int32 val = m_pp.AGI + itembonuses.AGI + spellbonuses.AGI - CalcAlcoholPhysicalEffect() - CalculateFatiguePenalty();
	int32 mod = aabonuses.AGI;

	int32 str = GetSTR();

	float encum_factor = 1.0;

	// Near death - from client decompile
	if (GetMaxHP() > 0 && !dead)	// agi calculation depends on max hp calculation
	{
		if (cur_hp < max_hp && GetLevel() > 3)
		{
			if ((float)cur_hp / max_hp >= 0.2 || cur_hp >= 50.0)
			{
				if ((float)cur_hp / max_hp < 0.33000001 && cur_hp < 100.0)
				{
					encum_factor = 0.75;
				}
			}
			else
			{
				encum_factor = 0.5;
			}
		}
	}

	//Encumbered penalty
	if (IsEncumbered() && GetZoneID() != Zones::BAZAAR)
	{
		//AGI is halved when we double our weight, zeroed (defaults to 1) when we triple it. this includes AGI from AAs
		encum_factor = fmaxf(0.0, encum_factor - (weight / 10.0f - str) / (float)(str + str));
	}
	AGI = (val + mod) * encum_factor;

	if(AGI < 1)
		AGI = 1;

	int m = GetMaxAGI();
	if(AGI > m)
		AGI = m;

	return(AGI);
}

int32 Client::CalcDEX() {
	int32 val = m_pp.DEX + itembonuses.DEX + spellbonuses.DEX - CalcAlcoholPhysicalEffect() - CalculateFatiguePenalty();

	int32 mod = aabonuses.DEX;

	DEX = val + mod;

	if(DEX < 1)
		DEX = 1;

	int m = GetMaxDEX();
	if(DEX > m)
		DEX = m;

	return(DEX);
}

int32 Client::CalcINT() {
	int32 val = m_pp.INT + itembonuses.INT + spellbonuses.INT;

	int32 mod = aabonuses.INT;

	INT = val + mod;

	if(m_pp.intoxication)
	{
		int32 AlcINT = INT - (int32)((float)m_pp.intoxication / 200.0f * (float)INT) - 1;

		if((AlcINT < (int)(0.2 * INT)))
			INT = (int)(0.2f * (float)INT);
		else
			INT = AlcINT;
	}

	if(INT < 1)
		INT = 1;

	int m = GetMaxINT();
	if(INT > m)
		INT = m;

	return(INT);
}

int32 Client::CalcWIS() {
	int32 val = m_pp.WIS + itembonuses.WIS + spellbonuses.WIS;

	int32 mod = aabonuses.WIS;

	WIS = val + mod;

	if(m_pp.intoxication)
	{
		int32 AlcWIS = WIS - (int32)((float)m_pp.intoxication / 200.0f * (float)WIS) - 1;

		if((AlcWIS < (int)(0.2 * WIS)))
			WIS = (int)(0.2f * (float)WIS);
		else
			WIS = AlcWIS;
	}

	if(WIS < 1)
		WIS = 1;

	int m = GetMaxWIS();
	if(WIS > m)
		WIS = m;

	return(WIS);
}

int32 Client::CalcCHA() {
	int32 val = m_pp.CHA + itembonuses.CHA + spellbonuses.CHA;

	int32 mod = aabonuses.CHA;

	CHA = val + mod;

	if(CHA < 1)
		CHA = 1;

	int m = GetMaxCHA();
	if(CHA > m)
		CHA = m;

	return(CHA);
}

//The AA multipliers are set to be 5, but were 2 on WR
//in Mob::CheckResistSpell
int32	Client::CalcMR()
{
	return (MR = CalcMR(false, true));
}
int32	Client::CalcMR(bool ignoreCap, bool includeSpells)
{
	int32 calc;

	//racial bases
	switch(GetBaseRace()) {
		case HUMAN:
			calc = 25;
			break;
		case BARBARIAN:
			calc = 25;
			break;
		case ERUDITE:
			calc = 30;
			break;
		case WOOD_ELF:
			calc = 25;
			break;
		case HIGH_ELF:
			calc = 25;
			break;
		case DARK_ELF:
			calc = 25;
			break;
		case HALF_ELF:
			calc = 25;
			break;
		case DWARF:
			calc = 30;
			break;
		case TROLL:
			calc = 25;
			break;
		case OGRE:
			calc = 25;
			break;
		case HALFLING:
			calc = 25;
			break;
		case GNOME:
			calc = 25;
			break;
		case IKSAR:
			calc = 25;
			break;
		case VAHSHIR:
			calc = 25;
			break;
		default:
			calc = 20;
	}

	calc += itembonuses.MR + aabonuses.MR;
	if (includeSpells)
	{
		calc += spellbonuses.MR;
	}

	if(GetClass() == Class::Warrior)
		calc += GetLevel() / 2;

	if(calc < 1)
		calc = 1;

	if(!ignoreCap)
	{
		if(calc > GetMaxMR())
			calc = GetMaxMR();
	}

	return(calc);
}

int32	Client::CalcFR()
{
	return (FR = CalcFR(false, true));
}
int32	Client::CalcFR(bool ignoreCap, bool includeSpells)
{
	int32 calc;

	//racial bases
	switch(GetBaseRace()) {
		case HUMAN:
			calc = 25;
			break;
		case BARBARIAN:
			calc = 25;
			break;
		case ERUDITE:
			calc = 25;
			break;
		case WOOD_ELF:
			calc = 25;
			break;
		case HIGH_ELF:
			calc = 25;
			break;
		case DARK_ELF:
			calc = 25;
			break;
		case HALF_ELF:
			calc = 25;
			break;
		case DWARF:
			calc = 25;
			break;
		case TROLL:
			calc = 5;
			break;
		case OGRE:
			calc = 25;
			break;
		case HALFLING:
			calc = 25;
			break;
		case GNOME:
			calc = 25;
			break;
		case IKSAR:
			calc = 30;
			break;
		case VAHSHIR:
			calc = 25;
			break;
		default:
			calc = 20;
	}

	int c = GetClass();
	if(c == Class::Ranger) 
	{
		calc += 4;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	}
	else if(c == Class::Monk) 
	{
		calc += 8;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	}

	calc += itembonuses.FR + aabonuses.FR;
	if (includeSpells)
	{
		calc += spellbonuses.FR;
	}

	if(calc < 1)
		calc = 1;

	if(!ignoreCap)
	{
		if(calc > GetMaxFR())
			calc = GetMaxFR();
	}

	return(calc);
}

int32	Client::CalcDR()
{
	return (DR = CalcDR(false, true));
}
int32	Client::CalcDR(bool ignoreCap, bool includeSpells)
{
	int32 calc;

	//racial bases
	switch(GetBaseRace()) {
		case HUMAN:
			calc = 15;
			break;
		case BARBARIAN:
			calc = 15;
			break;
		case ERUDITE:
			calc = 10;
			break;
		case WOOD_ELF:
			calc = 15;
			break;
		case HIGH_ELF:
			calc = 15;
			break;
		case DARK_ELF:
			calc = 15;
			break;
		case HALF_ELF:
			calc = 15;
			break;
		case DWARF:
			calc = 15;
			break;
		case TROLL:
			calc = 15;
			break;
		case OGRE:
			calc = 15;
			break;
		case HALFLING:
			calc = 20;
			break;
		case GNOME:
			calc = 15;
			break;
		case IKSAR:
			calc = 15;
			break;
		case VAHSHIR:
			calc = 15;
			break;
		default:
			calc = 15;
	}

	int c = GetClass();
	if(c == Class::Paladin) 
	{
		calc += 8;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;

	} 
	else if(c == Class::ShadowKnight) 
	{
		calc += 4;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	} 
	else if(c == Class::Beastlord) 
	{
		calc += 4;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	} 
	else if(c == Class::Monk) 
	{
		int l = GetLevel();
		if(l > 50)
			calc += l - 50;
	}

	calc += itembonuses.DR + aabonuses.DR;
	if (includeSpells)
	{
		calc += spellbonuses.DR;
	}

	if(calc < 1)
		calc = 1;

	if(!ignoreCap)
	{
		if(calc > GetMaxDR())
			calc = GetMaxDR();
	}

	return(calc);
}

int32	Client::CalcPR()
{
	return (PR = CalcPR(false, true));
}
int32	Client::CalcPR(bool ignoreCap, bool includeSpells)
{
	int32 calc;

	//racial bases
	switch(GetBaseRace()) {
		case HUMAN:
			calc = 15;
			break;
		case BARBARIAN:
			calc = 15;
			break;
		case ERUDITE:
			calc = 15;
			break;
		case WOOD_ELF:
			calc = 15;
			break;
		case HIGH_ELF:
			calc = 15;
			break;
		case DARK_ELF:
			calc = 15;
			break;
		case HALF_ELF:
			calc = 15;
			break;
		case DWARF:
			calc = 20;
			break;
		case TROLL:
			calc = 15;
			break;
		case OGRE:
			calc = 15;
			break;
		case HALFLING:
			calc = 20;
			break;
		case GNOME:
			calc = 15;
			break;
		case IKSAR:
			calc = 15;
			break;
		case VAHSHIR:
			calc = 15;
			break;
		default:
			calc = 15;
	}

	int c = GetClass();
	if(c == Class::Rogue) 
	{
		calc += 8;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;

	} 
	else if(c == Class::ShadowKnight) 
	{
		calc += 4;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	}
	else if(c == Class::Monk) 
	{
		int l = GetLevel();
		if(l > 50)
			calc += l - 50;
	}

	calc += itembonuses.PR + aabonuses.PR;
	if (includeSpells)
	{
		calc += spellbonuses.PR;
	}

	if(calc < 1)
		calc = 1;

	if(!ignoreCap)
	{
		if(calc > GetMaxPR())
			calc = GetMaxPR();
	}

	return(calc);
}

int32	Client::CalcCR()
{
	return (CR = CalcCR(false, true));
}
int32	Client::CalcCR(bool ignoreCap, bool includeSpells)
{
	int32 calc;

	//racial bases
	switch(GetBaseRace()) {
		case HUMAN:
			calc = 25;
			break;
		case BARBARIAN:
			calc = 35;
			break;
		case ERUDITE:
			calc = 25;
			break;
		case WOOD_ELF:
			calc = 25;
			break;
		case HIGH_ELF:
			calc = 25;
			break;
		case DARK_ELF:
			calc = 25;
			break;
		case HALF_ELF:
			calc = 25;
			break;
		case DWARF:
			calc = 25;
			break;
		case TROLL:
			calc = 25;
			break;
		case OGRE:
			calc = 25;
			break;
		case HALFLING:
			calc = 25;
			break;
		case GNOME:
			calc = 25;
			break;
		case IKSAR:
			calc = 15;
			break;
		case VAHSHIR:
			calc = 25;
			break;
		default:
			calc = 25;
	}

	int c = GetClass();
	if(c == Class::Ranger) 
	{
		calc += 4;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	} 
	else if(c == Class::Beastlord) 
	{
		calc += 4;

		int l = GetLevel();
		if(l > 49)
			calc += l - 49;
	}

	calc += itembonuses.CR + aabonuses.CR;
	if (includeSpells)
	{
		calc += spellbonuses.CR;
	}

	if(calc < 1)
		calc = 1;

	if(!ignoreCap)
	{
		if(calc > GetMaxCR())
			calc = GetMaxCR();
	}

	return(calc);
}

uint32 Mob::GetInstrumentMod(uint16 spell_id)
{
	if (GetClass() != Class::Bard)	// instrument mods don't work for non bards
		return 10;

	if (!IsBardSong(spell_id)) // instrument mods don't work on spells that aren't bard songs.  there are 7 or so spells that have a bard casting skill assigned but they aren't bard songs.
		return 10;

	uint32 effectmod = 10;	// base mod - 100% effectiveness

	/*
	From Ravenwing:

	Puretone should stack with your instrument mod.

	You can stack:

	1. one instrument or worn mod (best of whatever's equipped)
	2. one buff (resonance or harmonize)
	3. one song (amplification)*
	4. puretone
	5. singing or instrument mastery

	*(resonance or harmonize will overwrite amplification when it first lands, but you can sing amp again and it will stick once the buff is already up; 
	  a fair number of bard song/buff stacking interactions work this way)

	As a side note on bard arithmetic, the way mods are described in the spell data (puretone = 2.8) is rather counterintuitive. 
	Puretone alone will put the mod at 2.8, or 280% of the base value, but Puretone + Epic (1.8, or 180%) adds up to 3.6 or 360%, not 4.6. 
	The bard's base value of 1.0 seems to be included in every mod. I usually think of puretone as +1.8, the epic as +0.8, and so forth.
	*/

	if (GetSkill(spells[spell_id].skill) > 0)	// no mods if the skill isn't trained
	{
		switch (spells[spell_id].skill)
		{
			case EQ::skills::SkillPercussionInstruments:
				if (itembonuses.percussionMod > 10)
					effectmod += itembonuses.percussionMod - 10;	// instruments have the 10 already added
				if (spellbonuses.percussionMod > 10)
					effectmod += spellbonuses.percussionMod - 10;	// puretone has the 10 already added
				effectmod += aabonuses.percussionMod;
				break;
			case EQ::skills::SkillStringedInstruments:
				if (itembonuses.stringedMod > 10)
					effectmod += itembonuses.stringedMod - 10;
				if (spellbonuses.stringedMod > 10)
					effectmod += spellbonuses.stringedMod - 10;
				effectmod += aabonuses.stringedMod;
				break;
			case EQ::skills::SkillWindInstruments:
				if (itembonuses.windMod > 10)
					effectmod += itembonuses.windMod - 10;
				if (spellbonuses.windMod > 10)
					effectmod += spellbonuses.windMod - 10;
				effectmod += aabonuses.windMod;
				break;
			case EQ::skills::SkillBrassInstruments:
				if (itembonuses.brassMod > 10)
					effectmod += itembonuses.brassMod - 10;
				if (spellbonuses.brassMod > 10)
					effectmod += spellbonuses.brassMod - 10;
				effectmod += aabonuses.brassMod;
				break;
			case EQ::skills::SkillSinging:
				if (itembonuses.singingMod > 10)
					effectmod += itembonuses.singingMod - 10;
				if (spellbonuses.singingMod > 10)
					effectmod += spellbonuses.singingMod - 10;
				effectmod += aabonuses.singingMod + spellbonuses.Amplification;	// Amplification is only for singing
				break;
			default:
				effectmod = 10;
				break;
		}
	}

	int effectmodcap = RuleI(Character, BaseInstrumentSoftCap);	// base mod cap, usually 36
	effectmodcap += aabonuses.songModCap + spellbonuses.songModCap + itembonuses.songModCap;  // mod cap can be raised to 39 with Ayonae's Tutelage AA

	if (effectmod < 10)
		effectmod = 10;

	if (effectmod > effectmodcap)
		effectmod = effectmodcap;

	Log(Logs::Detail, Logs::Spells, "%s::GetInstrumentMod() song=%d mod=%d modcap=%d\n",
			GetName(), spell_id, effectmod, effectmodcap);

	// it's unclear if the 2 cassindra clarity songs should be modded.  there are conflicting accounts on old forum posts.
	// the TAKP client does mod these spells unless we lie to the client by sending this false mod value in the action packet instead.
	// it is possible that AK worked this way, but this is just a guess.
	if (spell_id == SPELL_CINDAS_CHARISMATIC_CARILLON || spell_id == SPELL_CASSINDRAS_CHANT_OF_CLARITY || spell_id == SPELL_CASSINDRAS_CHORUS_OF_CLARITY)
	{
		effectmod = 10;
		Log(Logs::General, Logs::Spells, "Overriding instrument mod for song %d to %d", spell_id, effectmod);
	}
		
	return effectmod;
}

int Client::GetRawACNoShield(int &shield_ac, int spell_mod) const
{
	int ac = itembonuses.AC + spellbonuses.AC / spell_mod + aabonuses.AC;
	shield_ac = 0;
	const EQ::ItemInstance *inst = m_inv.GetItem(EQ::invslot::slotSecondary);
	if(inst)
	{
		if(inst->GetItem()->ItemType == EQ::item::ItemTypeShield)
		{
			ac -= inst->GetItem()->AC;
			shield_ac = inst->GetItem()->AC;
		}
	}
	return ac;
}

uint16 Client::CalculateLungCapacity()
{
	// Iksar do not benefit from Innate Lung Capacity and do not have STA penalty
	if (GetBaseRace() == IKSAR) return 127;

	// Innate Lung Capacity AA gives 10%/25%/50% more
	int base_lung_capacity = aabonuses.BreathLevel > 0 ? aabonuses.BreathLevel : 100;
	int lung_capacity = base_lung_capacity;

	int STA_penalty = GetSTA() - 30;
	if (STA_penalty < 100)
	{
		lung_capacity = base_lung_capacity * STA_penalty / 100;

		if (lung_capacity < 10)
			lung_capacity = 10;
		if (lung_capacity > base_lung_capacity)
			lung_capacity = base_lung_capacity;
	}

	return lung_capacity;
}

int32 Client::CalculateFatiguePenalty()
{
	if (m_pp.fatigue > GetSTA())
	{
		return std::min(m_pp.fatigue - GetSTA(), 10);
	}

	return 0;
}
