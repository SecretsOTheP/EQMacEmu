/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2004 EQEMu Development Team (http://eqemu.org)

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
#include "../common/spdat.h"
#include "../common/strings.h"
#include "../common/types.h"

#include "entity.h"
#include "client.h"
#include "mob.h"

#include "pets.h"
#include "zonedb.h"

#ifndef WIN32
#include <stdlib.h>
#include "../common/unix.h"
#endif


void GetRandPetName(char* name)
{
	// (G,J,K,L,V,X,Z) + ("",ab,ar,as,eb,en,ib,ob,on) + ("",an,ar,ek,ob) + (ab,er,n,tik)
	static const char *prefix[] = { "G", "J", "K", "L", "V", "X", "Z" };
	static const char *affix1[] = { "", "ab", "ar", "as", "eb", "en", "ib", "ob", "on" };
	static const char *affix2[] = { "", "an", "ar", "ek", "ob" };
	static const char *suffix[] = { "ab", "er", "n", "tik" };
	
	int p = zone->random.Int(0, (sizeof(prefix) / sizeof(const char *)) - 1);
	int a1 = zone->random.Int(0, (sizeof(affix1) / sizeof(const char *)) - 1);
	int a2 = zone->random.Int(0, (sizeof(affix2) / sizeof(const char *)) - 1);
	int s = zone->random.Int(0, (sizeof(suffix) / sizeof(const char *)) - 1);
	
	if (a1 == 0 && a2 == 0)
		s = 3;
	if (a2 == 3)
		s = zone->random.Int(0, (sizeof(suffix) / sizeof(const char *)) - 2);

	strcpy(name, prefix[p]);
	strcat(name, affix1[a1]);
	strcat(name, affix2[a2]);
	strcat(name, suffix[s]);

	Log(Logs::General, Logs::Pets, "Pet being created: %s", name);
}

// If you add more to this (for custom servers or whatever) make sure this is ordered from strongest to weakest
const FocusPetItem Pet::focusItems[FocusPetItemSize] = {
	// Symbol of Ancient Summoning
	{ PET_FOCUS_TIME, 25, 60, 40, FocusPetType::ALL },
	// Gloves of Dark Summoning
	{ PET_FOCUS_VT, 20, 60, 40, FocusPetType::ALL },
	// Encyclopedia Necrotheurgia
	{ PET_FOCUS_HATE_NECRO, 10, 48, 40, FocusPetType::NECRO },
	// Staff of Elemental Mastery: Water
	{ PET_FOCUS_STAFF_WATER, 10, 48, 40, FocusPetType::WATER },
	// Staff of Elemental Mastery: Earth
	{ PET_FOCUS_STAFF_EARTH, 10, 48, 40, FocusPetType::EARTH },
	// Staff of Elemental Mastery: Fire
	{ PET_FOCUS_STAFF_FIRE, 10, 48, 40, FocusPetType::FIRE },
	// Staff of Elemental Mastery: Air
	{ PET_FOCUS_STAFF_AIR, 10, 48, 40, FocusPetType::AIR },
	// Broom of Trilon
	{ PET_FOCUS_SOLTEMPLE_AIR, 5, 41, 4, FocusPetType::AIR },
	// Shovel of Ponz
	{ PET_FOCUS_SOLTEMPLE_EARTH, 5, 41, 4, FocusPetType::EARTH },
	// Torch of Alna
	{ PET_FOCUS_SOLTEMPLE_FIRE, 5, 41, 4, FocusPetType::FIRE },
	// Stein of Ulissa
	{ PET_FOCUS_SOLTEMPLE_WATER, 5, 41, 4, FocusPetType::WATER },
};

FocusPetType Pet::GetPetItemPetTypeFromSpellId(uint16 spell_id) {
	static const int firePets[]  = {626, 630, 634, 316, 399, 403, 395, 498, 571, 575, 622, 1673, 1677, 3322};
	static const int airPets[]   = {627, 631, 635, 317, 396, 400, 404, 499, 572, 576, 623, 1674, 1678, 3371};
	static const int earthPets[] = {624, 628, 632, 58, 397, 401, 335, 496, 569, 573, 620, 1675, 1671, 3324};
	static const int waterPets[] = {625, 629, 633, 315, 398, 403, 336, 497, 570, 574, 621, 1676, 1672, 3320};

	for(int i=0; i < sizeof(firePets)/ sizeof(firePets[0]); i++) {
		if((int)spell_id == firePets[i]) {
			return FocusPetType::FIRE;
		}
	}
	for(int i=0; i < sizeof(airPets)/ sizeof(airPets[0]); i++) {
		if((int)spell_id == airPets[i]) {
			return FocusPetType::AIR;
		}
	}
	for(int i=0; i < sizeof(earthPets)/ sizeof(earthPets[0]); i++) {
		if((int)spell_id == earthPets[i]) {
			return FocusPetType::EARTH;
		}
	}
	for(int i=0; i < sizeof(waterPets)/ sizeof(waterPets[0]); i++) {
		if((int)spell_id == waterPets[i]) {
			return FocusPetType::WATER;
		}
	}
	return FocusPetType::NONE;
}

void Mob::MakePet(uint16 spell_id, const char* pettype, const char *petname) {
	// petpower of -1 is used to get the petpower based on whichever focus is currently
	// equipped. This should replicate the old functionality for the most part.
	MakePoweredPet(spell_id, pettype, -1, petname);
}

// Split from the basic MakePet to allow backward compatiblity with existing code while also
// making it possible for petpower to be retained without the focus item having to
// stay equipped when the character zones. petpower of -1 means that the currently equipped petfocus
// of a client is searched for and used instead.
void Mob::MakePoweredPet(uint16 spell_id, const char* pettype, int16 petpower,
		const char *petname, float in_size, int16 focusItemId) {
	// Sanity and early out checking first.
	bool scale_pet = false;
	if(HasPet() || pettype == nullptr)
		return;

	//lookup our pets table record for this type
	PetRecord record;
	if(!database.GetPoweredPetEntry(pettype, petpower, &record)) {
		Log(Logs::General, Logs::Error, "Unable to find data for pet %s, check pets table.", pettype);
		return;
	}

	//find the NPC data for the specified NPC type
	const NPCType *base = database.GetNPCType(record.npc_type);
	if(base == nullptr) {
		Log(Logs::General, Logs::Error, "Unable to load NPC data for pet %s (NPC ID %d), check pets and npc_types tables.", pettype, record.npc_type);
		return;
	}

	FocusPetType focusType = Pet::GetPetItemPetTypeFromSpellId(spell_id);

	if (focusItemId == 0)
	{
		if (this->IsClient())
		{
			//Log(Logs::General, Logs::Pets, "We are a client time to check for focus items");
			FocusPetItem petItem;
			// Loop over all the focus items and figure out which on is the best to use
			// It will step down from PoP - Classic looking for the best focus item to use based on pet level
			int16 slot = 0;
			for(int i=0; i < FocusPetItemSize; i++) {
				petItem = Pet::focusItems[i];
				// Look in out inventory
				int16 slot_id = this->CastToClient()->GetInv().HasItem(petItem.item_id, 1, invWhereWorn);
				if(slot_id != INVALID_INDEX) {
					//skip this focus item if its effect is out of rage for the pet we are casting
					if(base->level >= petItem.min_level && base->level <= petItem.max_level) {
						Log(Logs::General, Logs::Pets, "Found Focus Item: %d in Inventory: %d", petItem.item_id, slot_id);
						Log(Logs::General, Logs::Pets, "Npc spell levels: %d (%d - %d)", base->level, petItem.min_level, petItem.max_level);
						slot = slot_id;
						focusItemId = petItem.item_id;
						break;
					} else {
						Log(Logs::General, Logs::Pets, "Moving on Pet base level is out of range: %d (%d - %d)", base->level, petItem.min_level, petItem.max_level);
					}
				}
			}
			// we have a focus item
			if(focusItemId != 0)
			{
				// Symbol or Gloves can be used by all NEC, MAG, BST
				if(petItem.pet_type == FocusPetType::ALL)
				{
					//Log(Logs::General, Logs::Pets, "Type is ALL");
					focusType = FocusPetType::ALL;
				} else {
					// make sure we can use the focus item as the class .. client should never let us fail this but for sanity!
					if (GetClass() == MAGICIAN) {
						Log(Logs::General, Logs::Pets, "Looking up mage");
						Log(Logs::General, Logs::Pets, "Looking up if spell: %d is allowed ot be focused", spell_id);
						focusType = Pet::GetPetItemPetTypeFromSpellId(spell_id);
						Log(Logs::General, Logs::Pets, "FocusType fround %i", focusType);
					} else if (GetClass() == NECROMANCER) {
						Log(Logs::General, Logs::Pets, "We are a necro");
						focusType = FocusPetType::NECRO;
					}
				}

				Log(Logs::General, Logs::Pets, "Pet Item Type  %i", petItem.pet_type);
				if (focusType != petItem.pet_type)
					focusItemId = 0;
			}
		}
	}

	//we copy the npc_type data because we need to edit it a bit
	auto npc_type = new NPCType;
	memcpy(npc_type, base, sizeof(NPCType));

	npc_type->loot_lockout = 0;
	// If pet power is set to -1 in the DB, use stat scaling
	// Torven: Al'Kabor pre-dates pet power, and focii do different stats per class in our era, so needs to be hardcoded anyway
	if (focusItemId && this->IsClient() && record.petpower == -1)
	{
		uint32 minDmg = 0;
		uint32 maxDmg = 0;
		int32 hpPct = 0;
		uint32 ac = 0;
		uint32 atk = 0;
		uint8 lvl = 0;
		float size = 1.0f;

		switch (focusItemId)
		{
		case PET_FOCUS_SOLTEMPLE_AIR:
			atk = 7;
			hpPct = 20;
			break;
		case PET_FOCUS_SOLTEMPLE_EARTH:
			atk = 10;
			break;
		case PET_FOCUS_SOLTEMPLE_FIRE:
			atk = 5;
			hpPct = 25;
			break;
		case PET_FOCUS_SOLTEMPLE_WATER:
			atk = 8;
			hpPct = 10;
			break;
		case PET_FOCUS_HATE_NECRO:
			if (spell_id == 1623)		// Emissary of Thule; Live has +4 for this pet with book
				maxDmg += 2;
		case PET_FOCUS_STAFF_WATER:
			lvl = 1;
			maxDmg = 2;
			hpPct = 3;
			ac = 5;
			atk = 8;
			break;
		case PET_FOCUS_STAFF_EARTH:
			lvl = 1;
			maxDmg = 3;
			ac = 10;
			atk = 10;
			break;
		case PET_FOCUS_STAFF_FIRE:
			lvl = 1;
			maxDmg = 1;
			hpPct = 10;
			ac = 5;
			atk = 4;
			break;			
		case PET_FOCUS_STAFF_AIR:
			lvl = 1;
			maxDmg = 2;
			hpPct = 6;
			ac = 5;
			atk = 5;
			break;
		case PET_FOCUS_VT:
			lvl = 1;
			minDmg = 1;
			maxDmg = 3;
			hpPct = 15;
			ac = 20;
			atk = 20;
			if (spell_id == 1936)		// magician epic pet
				maxDmg += 1;			// logs show GoDS doing +3 on PoP pets, but some web evidence suggests it was +4 for epic.  unverified
			break;
		case PET_FOCUS_TIME:
			lvl = 1;
			hpPct = 30;
			ac = 30;
			atk = 25;
			switch (GetClass())
			{
			case MAGICIAN:
				minDmg = 4;
				maxDmg = 11;
				break;
			case NECROMANCER:
				minDmg = 2;
				maxDmg = 8;
				break;
			case BEASTLORD:
				minDmg = 1;
				maxDmg = 9;
				break;
			}
			break;
		}
		npc_type->level += lvl;
		npc_type->size += size;
		npc_type->max_hp = npc_type->max_hp * (100 + hpPct) / 100;
		npc_type->cur_hp = npc_type->max_hp;
		npc_type->AC += ac;
		npc_type->min_dmg += minDmg;
		npc_type->max_dmg += maxDmg;
		npc_type->ATK += atk;
	}

	// Pet naming:
	// 0 - `s pet
	// 1 - `s familiar
	// 2 - `s Warder
	// 3 - Random name if client, `s pet for others
	// 4 - Keep DB name

	if (petname != nullptr) {
		// Name was provided, use it.
		strn0cpy(npc_type->name, petname, 64);
	} else if (record.petnaming == 0) {
		strcpy(npc_type->name, this->GetCleanOwnerName());
		npc_type->name[25] = '\0';
		strcat(npc_type->name, "`s_pet");
	} else if (record.petnaming == 1) {
		strcpy(npc_type->name, this->GetName());
		npc_type->name[19] = '\0';
		strcat(npc_type->name, "`s_familiar");
	} else if (record.petnaming == 2) {
		strcpy(npc_type->name, this->GetName());
		npc_type->name[21] = 0;
		strcat(npc_type->name, "`s_warder");
	} else if (record.petnaming == 4) {
		// Keep the DB name
	} else if (record.petnaming == 3 && IsClient()) {
		GetRandPetName(npc_type->name);
	} else {
		strcpy(npc_type->name, this->GetCleanOwnerName());
		npc_type->name[25] = '\0';
		strcat(npc_type->name, "`s_pet");
	}

	//handle beastlord pet appearance
	if(record.petnaming == 2)
	{
		switch(GetBaseRace())
		{
		case VAHSHIR:
			npc_type->race = TIGER;
			npc_type->size *= 0.8f;
			break;
		case TROLL:
			npc_type->race = ALLIGATOR;
			npc_type->size *= 2.5f;
			break;
		case OGRE:
			npc_type->race = BEAR;
			npc_type->texture = 3;
			npc_type->gender = 2;
			break;
		case BARBARIAN:
			npc_type->race = WOLF;
			npc_type->texture = 2;
			break;
		case IKSAR:
			npc_type->race = WOLF;
			npc_type->texture = 0;
			npc_type->gender = 1;
			npc_type->luclinface = 0;
			break;
		default:
			npc_type->race = WOLF;
			npc_type->texture = 0;
		}
	}

	// handle monster summoning pet appearance
	if(record.monsterflag) {

		uint32 monsterid = 0;

		// get a random npc id from the spawngroups assigned to this zone
		auto query = StringFormat("SELECT npcID "
									"FROM (spawnentry INNER JOIN spawn2 ON spawn2.spawngroupID = spawnentry.spawngroupID) "
									"INNER JOIN npc_types ON npc_types.id = spawnentry.npcID "
									"WHERE spawn2.zone = '%s' AND npc_types.bodytype NOT IN (11, 33, 66, 67) "
									"AND npc_types.race NOT IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 44, "
									"55, 67, 71, 72, 73, 77, 78, 81, 90, 92, 93, 94, 106, 112, 114, 127, 128, "
									"130, 139, 141, 183, 236, 237, 238, 239, 254, 266, 329) AND npc_types.underwater = 0 "
									"ORDER BY RAND() LIMIT 1", zone->GetShortName());
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			// if the database query failed
		}

		if (results.RowCount() != 0) {
			auto row = results.begin();
			monsterid = atoi(row[0]);
		}

		// since we don't have any monsters, just make it look like an earth pet for now
		if (monsterid == 0)
			monsterid = 579;

		Log(Logs::General, Logs::Pets, "Monster Summon appearance NPCID is %d", monsterid);

		// give the summoned pet the attributes of the monster we found
		const NPCType* monster = database.GetNPCType(monsterid);
		if(monster) {
			npc_type->race = monster->race;
			if (monster->size < 1)
			{
				npc_type->size = 6;
			}
			else
			{
				npc_type->size = monster->size;
			}
			npc_type->texture = monster->texture;
			npc_type->gender = monster->gender;
			npc_type->luclinface = monster->luclinface;
			npc_type->helmtexture = monster->helmtexture;
		} else
			Log(Logs::General, Logs::Error, "Error loading NPC data for monster summoning pet (NPC ID %d)", monsterid);

	}

	npc_type->loot_lockout = 0;

	//this takes ownership of the npc_type data
	auto npc = new Pet(npc_type, this, (PetType)record.petcontrol, spell_id, record.petpower, focusItemId);

	if (npc != nullptr)
	{
		npc->AddLootTable();
		npc->UpdateEquipmentLight();

		// finally, override size if one was provided
		if (in_size > 0.0f)
			npc->size = in_size;

		npc->z_offset = npc->CalcZOffset();
		npc->head_offset = npc->CalcHeadOffset();
		npc->model_size = npc->CalcModelSize();
		npc->model_bounding_radius = npc->CalcBoundingRadius();
		entity_list.AddNPC(npc, true, true);
		SetPetID(npc->GetID());
	}

	safe_delete(npc_type);
}
/* This is why the pets ghost - pets were being spawned too far away from its npc owner and some
into walls or objects (+10), this sometimes creates the "ghost" effect. I changed to +2 (as close as I
could get while it still looked good). I also noticed this can happen if an NPC is spawned on the same spot of another or in a related bad spot.*/
Pet::Pet(NPCType *type_data, Mob *owner, PetType type, uint16 spell_id, int16 power, int16 focusItemId)
: NPC(type_data, 0, owner->GetPosition() + glm::vec4(2.0f, 2.0f, 0.0f, 0.0f), EQ::constants::GravityBehavior::Water)
{
	typeofpet = type;
	petpower = power;
	petfocusItemId = focusItemId;
	SetOwnerID(owner ? owner->GetID() : 0);
	SetPetSpellID(spell_id);
	summonerid = owner->GetID();
	loot_lockout_timer = 0;
	taunting = true;
	if (owner && owner->IsClient())
		summonedClientPet = true;

	// melee routines use these; modifying will affect NPC performance significantly.  See http://www.eqemulator.org/forums/showthread.php?t=38708
	// ideally we would softcode/use database for NPC skills, but hardcoding this for now
	uint16 skillLevel = std::min(level * 5, 225);
	if (level >= 60)		// PoP pets
		skillLevel += 10;

	skills[EQ::skills::SkillHandtoHand] = skillLevel;
	skills[EQ::skills::Skill1HBlunt] = skillLevel;
	skills[EQ::skills::Skill2HBlunt] = skillLevel;
	skills[EQ::skills::Skill1HSlashing] = skillLevel;
	skills[EQ::skills::Skill2HSlashing] = skillLevel;
	skills[EQ::skills::SkillDefense] = skillLevel;
	skills[EQ::skills::SkillOffense] = skillLevel;
	skills[EQ::skills::Skill1HPiercing] = skillLevel;
	skills[EQ::skills::SkillDoubleAttack] = 0;
	skills[EQ::skills::SkillDualWield] = 0;

	if (class_ == WARRIOR || class_ == PALADIN || class_ == SHADOWKNIGHT)
		skills[EQ::skills::SkillBash] = level > 5 ? skillLevel : 0;

	if (class_ == WARRIOR || class_ == RANGER)
		skills[EQ::skills::SkillKick] = level > 15 ? skillLevel : 0;

	if (class_ == ROGUE)
		skills[EQ::skills::SkillBackstab] = level > 9 ? skillLevel : 0;

	if (class_ == MONK)
	{
		skills[EQ::skills::SkillRoundKick] = level > 4 ? skillLevel : 0;
		skills[EQ::skills::SkillTigerClaw] = level > 9 ? skillLevel : 0;
		skills[EQ::skills::SkillEagleStrike] = level > 19 ? skillLevel : 0;
		skills[EQ::skills::SkillDragonPunch] = level > 24 ? skillLevel : 0;
		skills[EQ::skills::SkillFlyingKick] = level > 29 ? skillLevel : 0;
	}

	// avoidance skills
	skillLevel = std::min(level * 5, 100);
	if (level >= 60)
		skillLevel += 25;

	if (class_ == WARRIOR || class_ == PALADIN || class_ == SHADOWKNIGHT || class_ == ROGUE || class_ == RANGER || class_ == BARD)
	{
		skills[EQ::skills::SkillParry] = level > 6 ? skillLevel : 0;
		skills[EQ::skills::SkillRiposte] = level > 11 ? skillLevel : 0;
	}
	else if (class_ == MONK)
	{
		skills[EQ::skills::SkillParry] = 0;
		skills[EQ::skills::SkillRiposte] = level > 11 ? skillLevel : 0;
		skills[EQ::skills::SkillBlock] = skillLevel;
	}
	else
	{
		skills[EQ::skills::SkillParry] = 0;
		skills[EQ::skills::SkillRiposte] = 0;
		skills[EQ::skills::SkillBlock] = 0;
	}

	skills[EQ::skills::SkillDodge] = level > 10 ? skillLevel : 0;

	// double attack & dual wield
	skillLevel = std::min(level * 5, 200);

	skills[EQ::skills::SkillDoubleAttack] = level > 9 ? skillLevel : 0;
	skills[EQ::skills::SkillDualWield] = level > 18 ? skillLevel : 0;

	// these class pets are weaker at lower levels (shaman, druid, beastlord)
	if (race == WOLF_ELEMENTAL || race == TIGER || race == ALLIGATOR || race == WOLF || race == BEAR)
	{
		if (level < 35)
		{
			skills[EQ::skills::SkillDualWield] = std::max((level - 16) * 5, 0);

			if (race != WOLF_ELEMENTAL && skills[EQ::skills::SkillDoubleAttack])
				skills[EQ::skills::SkillDoubleAttack] -= 35 - level;
		}
	}

	// beastlord pets block instead of parry
	if ((race == TIGER || race == ALLIGATOR || race == BEAR || race == WOLF) && type_data->npc_id != 638)	// 638 is druid bear pet
	{
		skills[EQ::skills::SkillBlock] = std::min(level *2, 50);
		skills[EQ::skills::SkillParry] = 0;
	}

	// temporary pets, Unswervering Hammer of Faith and Flaming Sword of Xuzl
	if (typeofpet == petHatelist && owner)
	{
		taunting = false;
		Mob *tar = owner ? owner->GetTarget() : nullptr;
		if (tar && DistanceSquaredNoZ(owner->GetPosition(), tar->GetPosition()) < (200 * 200) && IsAttackAllowed(tar)
			&& (tar->GetZ() - owner->GetZ() < 75) && (tar->GetZ() - owner->GetZ() > -75)
		)
			AddToHateList(tar, 1);

		despawn_timer.Start(1000);

		skills[EQ::skills::SkillBash] = 0;
		skills[EQ::skills::SkillKick] = 0;
		skills[EQ::skills::SkillDoubleAttack] = 150;
		skills[EQ::skills::SkillOffense] = 150;
	}
}

bool ZoneDatabase::GetPetEntry(const char *pet_type, PetRecord *into) {
	return GetPoweredPetEntry(pet_type, 0, into);
}

bool ZoneDatabase::GetPoweredPetEntry(const char *pet_type, int16 petpower, PetRecord *into) {
	std::string query;

	if (petpower <= 0)
		query = StringFormat("SELECT npcID, temp, petpower, petcontrol, petnaming, monsterflag, equipmentset "
							"FROM pets WHERE type='%s' AND petpower<=0", pet_type);
	else
		query = StringFormat("SELECT npcID, temp, petpower, petcontrol, petnaming, monsterflag, equipmentset "
                            "FROM pets WHERE type='%s' AND petpower<=%d ORDER BY petpower DESC LIMIT 1",
                            pet_type, petpower);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();

	into->npc_type = atoi(row[0]);
	into->temporary = atoi(row[1]);
	into->petpower = atoi(row[2]);
	into->petcontrol = atoi(row[3]);
	into->petnaming = atoi(row[4]);
	into->monsterflag = atoi(row[5]);
	into->equipmentset = atoi(row[6]);

	return true;
}

Mob* Mob::GetPet() {
	if(GetPetID() == 0)
		return(nullptr);

	Mob* tmp = entity_list.GetMob(GetPetID());
	if(tmp == nullptr) {
		SetPetID(0);
		return(nullptr);
	}

	if(tmp->GetOwnerID() != GetID()) {
		SetPetID(0);
		return(nullptr);
	}

	return(tmp);
}

void Mob::SetPet(Mob* newpet) {
	Mob* oldpet = GetPet();
	if (oldpet && IsClient()) {
		oldpet->SetOwnerID(0);
	}
	if (newpet == nullptr) {
		SetPetID(0);
	} else {
		SetPetID(newpet->GetID());
		Mob* oldowner = entity_list.GetMob(newpet->GetOwnerID());
		if (oldowner && oldowner->IsClient())
			oldowner->SetPetID(0);
		newpet->SetOwnerID(this->GetID());
	}
}

void Mob::DepopPet(bool depopsummoned)
{
	if (HasPet())
	{
		Mob* mypet = GetPet();
		SetPet(nullptr);
		if (mypet)
		{
			// IsCharmedPet() is not safe to use here, because we may be at a point where the pet's ownerid has
			// already been set to 0 and thus will fail the IsPet() check.
			if (mypet->IsNPC() && mypet->GetPetType() != petCharmed)
			{
				mypet->CastToNPC()->Depop();
			}
		}
	}

	// kill summoned pet even if charmed
	if (depopsummoned)
	{
		uint16 petID = entity_list.GetSummonedPetID(this);
		if (petID)
		{
			Mob* pet = entity_list.GetMobID(petID);

			if (pet)
				pet->SetOwnerID(0);
		}
	}
	// just clear the variable so the pet will despawn on its own once charm wear off.
	else
	{
		entity_list.ClearSummonedPetID(GetID());
	}

	FadeVoiceGraft();
}

void Mob::FadePetCharmBuff()
{
	if ((this->GetPetType() == petCharmed))
	{
		Mob* formerpet = this->GetPet();

		if (formerpet) {
			formerpet->BuffFadeByEffect(SE_Charm);
		}
	}
}

void Mob::SetPetID(uint16 NewPetID) {
	if (NewPetID == GetID() && NewPetID != 0)
		return;
	petid = NewPetID;
}

void NPC::GetPetState(SpellBuff_Struct *pet_buffs, uint32 *items, char *name) {
	//save the pet name
	strn0cpy(name, GetName(), 64);

	//save their items, we only care about what they are actually wearing
	memcpy(items, equipment, sizeof(uint32)* EQ::invslot::EQUIPMENT_COUNT);

	//save their buffs.
	for (int i=0; i < GetPetMaxTotalSlots(); i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			pet_buffs[i].bufftype = 2; // TODO - don't hardcode this, it can be 4 for reversed effects
			pet_buffs[i].level = buffs[i].casterlevel;
			pet_buffs[i].bard_modifier = buffs[i].instrumentmod;
			pet_buffs[i].activated = spells[buffs[i].spellid].Activated;
			pet_buffs[i].spellid = buffs[i].spellid;
			pet_buffs[i].duration = buffs[i].ticsremaining;
			pet_buffs[i].counters = buffs[i].counters;
			pet_buffs[i].player_id = buffs[i].casterid;
		}
		else {
			pet_buffs[i].bufftype = 0;
			pet_buffs[i].level = 0;
			pet_buffs[i].bard_modifier = 0;
			pet_buffs[i].activated = 0;
			pet_buffs[i].spellid = SPELL_UNKNOWN;
			pet_buffs[i].duration = 0;
			pet_buffs[i].counters = 0;
			pet_buffs[i].player_id = 0;
		}
	}
}

void NPC::SetPetState(SpellBuff_Struct *pet_buffs, uint32 *items) {
	//restore their buffs...

	int i;
	for (i = 0; i < GetPetMaxTotalSlots(); i++) {
		for(int z = 0; z < GetPetMaxTotalSlots(); z++) {
		// check for duplicates
			if(buffs[z].spellid != SPELL_UNKNOWN && buffs[z].spellid == pet_buffs[i].spellid) {
				buffs[z].spellid = SPELL_UNKNOWN;
				pet_buffs[i].spellid = SPELL_UNKNOWN;
			}
		}

		if (pet_buffs[i].spellid <= (uint32)SPDAT_RECORDS && pet_buffs[i].spellid != 0 && pet_buffs[i].duration > 0) {
			if(pet_buffs[i].level == 0 || pet_buffs[i].level > 100)
				pet_buffs[i].level = 1;
			buffs[i].spellid			= pet_buffs[i].spellid;
			buffs[i].ticsremaining		= pet_buffs[i].duration;
			buffs[i].casterlevel		= pet_buffs[i].level;
			buffs[i].casterid			= pet_buffs[i].player_id;
			buffs[i].counters			= pet_buffs[i].counters;
			buffs[i].instrumentmod		= pet_buffs[i].bard_modifier;
		}
		else {
			buffs[i].spellid = SPELL_UNKNOWN;
			pet_buffs[i].bufftype = 0;
			pet_buffs[i].level = 0;
			pet_buffs[i].bard_modifier = 0;
			pet_buffs[i].activated = 0;
			pet_buffs[i].spellid = SPELL_UNKNOWN;
			pet_buffs[i].duration = 0;
			pet_buffs[i].counters = 0;
			pet_buffs[i].player_id = 0;
		}
	}
	for (int j1=0; j1 < GetPetMaxTotalSlots(); j1++) {
		if (buffs[j1].spellid <= (uint32)SPDAT_RECORDS) {
			for (int x1=0; x1 < EFFECT_COUNT; x1++) {
				switch (spells[buffs[j1].spellid].effectid[x1]) {
					case SE_WeaponProc:
						// We need to reapply buff based procs
						// We need to do this here so suspended pets also regain their procs.
						int procmod;
						switch (buffs[j1].spellid) {
							// these are proc mods for bst pets
							case 2635:
							case 2636:
							case 2637:
							case 2638:
							case 2639:
							case 2640:
							case 2641:
							case 2888:
							case 2890:
							case 3459:
								procmod = 275;
								break;
							default:
								procmod = spells[buffs[j1].spellid].base2[x1];
						}
						AddProcToWeapon(GetProcID(buffs[j1].spellid,x1), 100+procmod, buffs[j1].spellid);
						break;
					case SE_Charm:
					case SE_Rune:
					case SE_Illusion:
						buffs[j1].spellid = SPELL_UNKNOWN;
						pet_buffs[j1].spellid = SPELL_UNKNOWN;
						pet_buffs[j1].bufftype = 0;
						pet_buffs[j1].level = 0;
						pet_buffs[j1].duration = 0;
						//pet_buffs[j1].effect = 0;
						x1 = EFFECT_COUNT;
						break;
					// We can't send appearance packets yet, put down at CompleteConnect
				}
			}
		}
	}

	//restore their equipment...
	for (i = 0; i < EQ::invslot::EQUIPMENT_COUNT; i++) {
		if(items[i] == 0)
			continue;

		const EQ::ItemData* item2 = database.GetItem(items[i]);
		if (item2 && item2->NoDrop != 0) {
			//dont bother saving item charges for now, NPCs never use them
			//and nobody should be able to get them off the corpse..?
			AddLootDrop(item2, &itemlist, 0, 0, 255, true, true);
		}
	}
}
