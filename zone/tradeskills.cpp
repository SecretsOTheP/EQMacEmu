/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2004 EQEMu Development Team (http://eqemulator.net)

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

#include <stdlib.h>
#include <list>

#ifndef WIN32
#include <netinet/in.h>	//for htonl
#endif

#include "../common/rulesys.h"
#include "../common/strings.h"
#include "../common/repositories/criteria/content_filter_criteria.h"

#include "queryserv.h"
#include "worldserver.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "titles.h"
#include "zonedb.h"

extern QueryServ* QServ;
extern WorldServer worldserver;

static const EQ::skills::SkillType TradeskillUnknown = EQ::skills::Skill1HBlunt; /* an arbitrary non-tradeskill */

// Perform tradeskill combine
void Object::HandleCombine(Client* user, const Combine_Struct* in_combine, Object* worldo)
{
	if (!user) {
		LogError("Client not set in Object::HandleCombine");
		return;
	}

	if (!in_combine) {
		LogError("Combine_Struct not set in Object::HandleCombine");
		auto outapp = new EQApplicationPacket(OP_TradeSkillCombine, 0);
		user->QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	EQ::InventoryProfile& user_inv = user->GetInv();
	PlayerProfile_Struct& user_pp = user->GetPP();
	EQ::ItemInstance* container = nullptr;
	EQ::ItemInstance* inst = nullptr;

	uint8 c_type		= 0xE8;
	uint32 some_id		= 0;
	bool worldcontainer = false;

	if (in_combine->container_slot == EQ::legacy::SLOT_TRADESKILL) {
		if(!worldo) {
			user->Message(Chat::Red, "Error: Server is not aware of the tradeskill container you are attempting to use");
			auto outapp = new EQApplicationPacket(OP_TradeSkillCombine, 0);
			user->QueuePacket(outapp);
			safe_delete(outapp);
			return;
		}
		c_type = worldo->m_type;
		inst = worldo->m_inst;
		worldcontainer=true;
	}
	else {
		inst = user_inv.GetItem(in_combine->container_slot);
		if (inst) {
			const EQ::ItemData* item = inst->GetItem();
			if (item && inst->IsClassBag()) {
				c_type = item->BagType;
				some_id = item->ID;
			}
		}
	}

	if (!inst || !inst->IsType(EQ::item::ItemClassBag)) {
		user->Message(Chat::Red, "Error: Server does not recognize specified tradeskill container");
		auto outapp = new EQApplicationPacket(OP_TradeSkillCombine, 0);
		user->QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	container = inst;

	DBTradeskillRecipe_Struct spec;
	if (!database.GetTradeRecipe(container, c_type, some_id, user->CharacterID(), &spec)) {
		user->Message_StringID(Chat::Emote,TRADESKILL_NOCOMBINE);
		auto outapp = new EQApplicationPacket(OP_TradeSkillCombine, 0);
		user->QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	// Character does not have the required skill.
	if(spec.skill_needed > 0 && user->GetSkill(spec.tradeskill) < spec.skill_needed ) {
		// Notify client.
		user->Message(Chat::LightBlue, "You are not skilled enough.");
		auto outapp = new EQApplicationPacket(OP_TradeSkillCombine, 0);
		user->QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	//changing from a switch to string of if's since we don't need to iterate through all of the skills in the SkillType enum
	if (spec.tradeskill == EQ::skills::SkillAlchemy) {
		if (user_pp.class_ != SHAMAN) {
			user->Message(Chat::Red, "This tradeskill can only be performed by a shaman.");
			return;
		}
		else if (user_pp.level < MIN_LEVEL_ALCHEMY) {
			user->Message(Chat::Red, "You cannot perform alchemy until you reach level %i.", MIN_LEVEL_ALCHEMY);
			return;
		}
	}
	else if (spec.tradeskill == EQ::skills::SkillTinkering) {
		if (user_pp.race != GNOME && spec.trivial > 0) {
			user->Message(Chat::Red, "Only gnomes can tinker.");
			return;
		}
	}
	else if (spec.tradeskill == EQ::skills::SkillMakePoison) {
		if (user_pp.class_ != ROGUE) {
			user->Message(Chat::Red, "Only rogues can mix poisons.");
			return;
		}
	}
	auto itr = spec.onsuccess.begin();
	while (itr != spec.onsuccess.end())
	{
		auto tsitem = database.GetItem(itr->first);
		if (!tsitem)
		{
			user->Message(Chat::Red, "This combine would result in an item that is out of era or unavailable.");
			return;
		}
		itr++;
	}

	//now clean out the containers.
	if(worldcontainer) {
		container->Clear();
		auto outapp = new EQApplicationPacket(OP_ClearObject, sizeof(ClearObject_Struct));
		ClearObject_Struct *cos = (ClearObject_Struct *)outapp->pBuffer;
		cos->Clear = 1;
		user->QueuePacket(outapp);
		safe_delete(outapp);
		database.DeleteWorldContainer(worldo->m_id, zone->GetZoneID());
	} 
	else {
		for (uint8 i = EQ::invslot::SLOT_BEGIN; i < EQ::invtype::WORLD_SIZE; i++) {
			const EQ::ItemInstance* inst = container->GetItem(i);
			if (inst) {
				user->DeleteItemInInventory(EQ::InventoryProfile::CalcSlotId(in_combine->container_slot,i),0,true);
			}
		}
		container->Clear();
	}
	//do the check and send results...
	bool success = user->TradeskillExecute(&spec);

	// Replace the container on success if required.
	//

	if(success && spec.replace_container) {
		if(worldcontainer){
			//should report this error, but we dont have the recipe ID, so its not very useful
			LogError("Replace container combine executed in a world container.");
		}
		else {
			user->DeleteItemInInventory(in_combine->container_slot, 0, true);
		}
	}

	{
		// Send acknowledgement packets to client - this lets the client transact again
		auto outapp = new EQApplicationPacket(OP_TradeSkillCombine, 0);
		user->QueuePacket(outapp);
		safe_delete(outapp);
	}

	if (success) {
		parse->EventPlayer(EVENT_COMBINE_SUCCESS, user, spec.name, spec.recipe_id);
	}
	else {
		parse->EventPlayer(EVENT_COMBINE_FAILURE, user, spec.name, spec.recipe_id);
	}
}

//returns true on success
bool Client::TradeskillExecute(DBTradeskillRecipe_Struct *spec) {
	if (spec == nullptr) {
		return(false);
	}

	uint16 user_skill = GetSkill(spec->tradeskill);
	float chance = 0.0;
	std::vector< std::pair<uint32,uint8> >::iterator itr;

	// Calculate success chance
	// For trivials over 68 the chance is (skill - 0.75*trivial) +51.5
	// For trivial up to 68 the chance is (skill - trivial) + 66
	// This is accurate.  See http://mboards.eqtraders.com/eq/showthread.php?11-What-sort-of-success-rate-should-I-expect
	// Verified correct here: http://mboards.eqtraders.com/eq/showthread.php?22246-Fan-Faire-June-2005-Write-up-(Plus-Tanker-handout
	// Sony uses an internal 'difficulty' value for recipes instead of basing them off trivials however
	// so this is not what Sony uses, but it outputs the desired rates using trivials instead of difficulties
	if (spec->trivial >= 68)
	{
		chance = user_skill - 0.75 * spec->trivial + 51.5;
	}
	else
	{
		chance = user_skill - spec->trivial + 66;
	}

	if (chance < 0) {
		chance = 0;
	}

	bool isTrivialCombine = static_cast<int>(GetRawSkill(spec->tradeskill)) - static_cast<int>(spec->trivial) >= 0 ? true : false;

	float roll = zone->random.Real(0, 100);
	int aa_chance = 0;

	// AA fail reduction modifiers; reduces fail chance by these percents
	if(spec->tradeskill == EQ::skills::SkillAlchemy){
		switch(GetAA(aaAlchemyMastery)){
		case 1:
			aa_chance = 10;
			break;
		case 2:
			aa_chance = 25;
			break;
		case 3:
			aa_chance = 50;
			break;
		}
	}

	if(spec->tradeskill == EQ::skills::SkillJewelryMaking){
		switch(GetAA(aaJewelCraftMastery)){
		case 1:
			aa_chance = 10;
			break;
		case 2:
			aa_chance = 25;
			break;
		case 3:
			aa_chance = 50;
			break;
		}
	}

	if (aa_chance && chance < 100)
	{
		chance += (100 - chance) * aa_chance / 100;
	}

	//handle caps
	if (spec->nofail)
	{
		chance = 100;
	}
	else if (chance < 5)
	{
		chance = 5;
	}
	else if (chance > 95)
	{
		chance = 95;
	}

	Log(Logs::Detail, Logs::Tradeskills, "Attempting combine;  Skill ID: %i, Player Skill: %d, Recipe Trivial: %d, AA Fail Reducton: %i pct, Success Chance: %0.2f pct %s, Roll: %0.2f",
		spec->tradeskill, user_skill, spec->trivial, aa_chance, chance, spec->nofail ? "(no fail combine)" : "", roll);

	if (isTrivialCombine) {
		Message_StringID(Chat::LightBlue, TRADESKILL_TRIVIAL);
	}

	const EQ::ItemData* item = nullptr;
	EQ::ItemInstance* returneditem = nullptr;
	if ((spec->tradeskill==75) || GetGM() || (chance > roll))
	{
		if (!isTrivialCombine) {
			CheckIncreaseTradeskill(true, spec->tradeskill);
		}

		Message_StringID(Chat::LightBlue, TRADESKILL_SUCCEED, spec->name.c_str());

		Log(Logs::Detail, Logs::Tradeskills, "Combine success");

		/* QS: Player_Log_Trade_Skill_Events */
		if (RuleB(QueryServ, PlayerLogTradeSkillEvents))
		{
			QServ->QSTSEvents(this->CharacterID(), this->GetZoneID(), "Success", spec->recipe_id, spec->tradeskill, spec->trivial, chance);
		}

		itr = spec->onsuccess.begin();
		while(itr != spec->onsuccess.end() && !spec->quest) 
		{
			item = database.GetItem(itr->first);
			if (item)
			{
				returneditem = database.CreateItem(item, itr->second);
			}

			if (returneditem)
			{
				PushItemOnCursorWithoutQueue(returneditem);

				if (this->GetGroup()) {
					entity_list.MessageGroup(this, true, Chat::Skills, "%s has successfully fashioned %s!", GetName(), item->Name);
				}

				safe_delete(returneditem);
			}

			++itr;
		}

		return(true);
	}
	/* Trade skill Fail */
	else 
	{
		if (!isTrivialCombine) {
			CheckIncreaseTradeskill(false, spec->tradeskill);
		}

		Message_StringID(Chat::LightBlue,TRADESKILL_FAILED);

		Log(Logs::Detail, Logs::Tradeskills, "Combine failed");
			if (this->GetGroup())
		{
			entity_list.MessageGroup(this,true, Chat::Skills,"%s was unsuccessful in %s tradeskill attempt.",GetName(),this->GetGender() == 0 ? "his" : this->GetGender() == 1 ? "her" : "its");

		}

		/* QS: Player_Log_Trade_Skill_Events */
		if (RuleB(QueryServ, PlayerLogTradeSkillEvents))
		{
			QServ->QSTSEvents(this->CharacterID(), this->GetZoneID(), "Failed", spec->recipe_id, spec->tradeskill, spec->trivial, chance);
		}


		itr = spec->onfail.begin();
		while(itr != spec->onfail.end()) 
		{
			item = database.GetItem(itr->first);
			if (item)
			{
				returneditem = database.CreateItem(item, itr->second);
			}

			if (returneditem)
			{
				PushItemOnCursorWithoutQueue(returneditem);
				safe_delete(returneditem);
			}

			++itr;
		}
	}
	return(false);
}

void Client::CheckIncreaseTradeskill(bool isSuccessfulCombine, EQ::skills::SkillType tradeskill)
{
	// See http://mboards.eqtraders.com/eq/showthread.php?15833-FAQ-Proposal-Skill-up-formula
	// and http://mboards.eqtraders.com/eq/showthread.php?22246-Fan-Faire-June-2005-Write-up-(Plus-Tanker-handout

	if (!CanIncreaseTradeskill(tradeskill)) {
		return;
	}

	double tradeDifficulty = zone->skill_difficulty[tradeskill].difficulty;

	if (RuleB(Quarm, EnableGlobalSkillupDifficultyAdjustments))
	{
		float global_skillup_mod = RuleR(Quarm, GlobalTradeSkillupDifficultyAdjustment);
		tradeDifficulty *= global_skillup_mod;
		if (tradeDifficulty < 1.0f) {
			tradeDifficulty = 1.0f;
		}
	}

	int tradeStat = 0;
	if (tradeskill == EQ::skills::SkillFletching || tradeskill == EQ::skills::SkillMakePoison)
	{
		tradeStat = std::max({ GetDEX(), GetINT(), GetWIS() });
	}
	else if (tradeskill == EQ::skills::SkillBlacksmithing)
	{
		tradeStat = std::max({ GetSTR(), GetINT(), GetWIS() });
	}
	else
	{
		tradeStat = std::max(GetWIS(), GetINT()) - 15;
	}

	int rawSkill = GetRawSkill(tradeskill);

	// Skill-ups have two checks: one rolls against a stat, the other rolls against current skill
	// Skills also have a difficulty value that is factored into the stat roll
	// For tradeskills, the difficulty values are either 2, 3 or 4
	// A successful combine significantly increases the skill-up chance
	double statCheck = tradeStat * 10.0 / (tradeDifficulty * (isSuccessfulCombine ? 1.0 : 2.0));

	Log(Logs::General, Logs::Tradeskills, "...Attemping Skill-Up; Combine Success: %s, Player Raw Skill Level: %i, INT: %d, WIS: %d, DEX: %d, STR: %d, Difficulty: %0.1f, statCheck: %0.1f",
		isSuccessfulCombine ? "yes" : "no", rawSkill, GetINT(), GetWIS(), GetDEX(), GetSTR(), tradeDifficulty, statCheck);

	if (statCheck > zone->random.Real(1, 1000))
	{
		Log(Logs::Detail, Logs::Tradeskills, "...Stat check success; Attempting Skill Check.  Success chance: %0.1f pct", 
			rawSkill <= 15 ? 100.0f : 100.0f - std::min(190, rawSkill) / 2.0);

		// The skill roll always succeeds if skill <= 15
		// Skill caps at 190 here, leaving a 5% chance to succeed when skill >= 190
		if (rawSkill <= 15 || zone->random.Int(1, 200) > std::min(190, rawSkill))
		{
			SetSkill(tradeskill, rawSkill + 1);

			if (title_manager.IsNewTradeSkillTitleAvailable(tradeskill, rawSkill + 1))
				NotifyNewTitlesAvailable();

			Log(Logs::Detail, Logs::Tradeskills, "...Tradeskill skill-up success.  New skill == %i", rawSkill + 1);
		}
		else
		{
			Log(Logs::Detail, Logs::Tradeskills, "...Failed tradeskill skill-up second roll");
		}
	}
	else
	{
		Log(Logs::Detail, Logs::Tradeskills, "...Failed tradeskill skill-up first roll");
	}
}

bool ZoneDatabase::GetTradeRecipe(const EQ::ItemInstance* container, uint8 c_type, uint32 some_id,
	uint32 char_id, DBTradeskillRecipe_Struct *spec)
{
	if (container == nullptr) {
		LogTradeskills("[GetTradeRecipe] Container null");
		return false;
	}

	std::string containers;// make where clause segment for container(s)
	if (some_id == 0) {
		containers = StringFormat("= %u", c_type); // world combiner so no item number
	}
	else {
		containers = StringFormat("IN (%u,%u)", c_type, some_id);
	} // container in inventory

	//Could prolly watch for stacks in this loop and handle them properly...
	//just increment sum and count accordingly
	bool		first = true;
	std::string buf2;
	uint32		count = 0;
	uint32		sum = 0;
	for (uint8 i = 0; i < 10; i++) { // <watch> TODO: need to determine if this is bound to world/item container size
		LogTradeskills("[GetTradeRecipe] Fetching item [{}]", i);

		const EQ::ItemInstance* inst = container->GetItem(i);
		if (!inst) {
			continue;
		}

		const EQ::ItemData* item = GetItem(inst->GetItem()->ID);
		if (!item) {
			LogTradeskills("[GetTradeRecipe] item [{}] not found!", inst->GetItem()->ID);
			continue;
		}

		if(first) {
			buf2 += StringFormat("%d", item->ID);
			first = false;
		}
		else {
			buf2 += StringFormat(",%d", item->ID);
		}

		sum += item->ID;
		count++;

		LogTradeskills("[GetTradeRecipe] Item in container index [{}] item [{}] found [{}]", i, item->ID, count);
	}

	//no items == no recipe
	if (count == 0) {
		return false;
	}

	std::string query = StringFormat(
		SQL(
			SELECT
				tradeskill_recipe_entries.recipe_id
			FROM 
				tradeskill_recipe_entries
			INNER JOIN 
				tradeskill_recipe ON (tradeskill_recipe_entries.recipe_id = tradeskill_recipe.id)
            WHERE 
				tradeskill_recipe.enabled
				 %s
			AND 
			(
				(tradeskill_recipe_entries.item_id IN(%s) AND tradeskill_recipe_entries.componentcount > 0)
			OR 
				(tradeskill_recipe_entries.item_id %s AND tradeskill_recipe_entries.iscontainer=1 )
			)
			GROUP BY 
				tradeskill_recipe_entries.recipe_id
				HAVING 
				sum(tradeskill_recipe_entries.componentcount) = %u AND sum(tradeskill_recipe_entries.item_id * tradeskill_recipe_entries.componentcount) = %u
		),
		ContentFilterCriteria::apply("tradeskill_recipe").c_str(),
		buf2.c_str(),
		containers.c_str(),
		count,
		sum
	);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		Log(Logs::General, Logs::Error, "Error in GetTradeRecipe search, query: %s", query.c_str());
		Log(Logs::General, Logs::Error, "Error in GetTradeRecipe search, error: %s", results.ErrorMessage().c_str());
		return false;
	}

	if (results.RowCount() > 1) {
		//multiple recipes, partial match... do an extra query to get it exact.
		//this happens when combining components for a smaller recipe
		//which is completely contained within another recipe
		first = true;
		uint32 index = 0;
		buf2 = "";
		for (auto row = results.begin(); row != results.end(); ++row, ++index) {
			uint32 recipeid = (uint32)atoi(row[0]);
			if(first) {
				buf2 += StringFormat("%u", recipeid);
				first = false;
			}
			else {
				buf2 += StringFormat(",%u", recipeid);
			}

			//length limit on buf2
			if(index == 214) { //Maximum number of recipe matches (19 * 215 = 4096)
				Log(Logs::General, Logs::Error, "GetTradeRecipe warning: Too many matches. Unable to search all recipe entries. Searched %u of %u possible entries.", index + 1, results.RowCount());
				break;
			}
		}

		query = StringFormat("SELECT tre.recipe_id "
                            "FROM tradeskill_recipe_entries AS tre "
                            "WHERE tre.recipe_id IN (%s) "
                            "GROUP BY tre.recipe_id HAVING sum(tre.componentcount) = %u "
                            "AND sum(tre.item_id * tre.componentcount) = %u", buf2.c_str(), count, sum);
		results = QueryDatabase(query);
		if (!results.Success()) {
			Log(Logs::General, Logs::Error, "Error in GetTradeRecipe, re-query: %s", query.c_str());
			Log(Logs::General, Logs::Error, "Error in GetTradeRecipe, error: %s", results.ErrorMessage().c_str());
			return false;
		}
	}

	if (results.RowCount() < 1) {
		return false;
	}

	if(results.RowCount() > 1) {
		//The recipe is not unique, so we need to compare the container were using.
		uint32 containerId = 0;

		if (some_id) { //Standard container
			containerId = some_id;
		}
		else if (c_type) {//World container
			containerId = c_type;
		}
		else { //Invalid container
			return false;
		}

		query = StringFormat("SELECT tre.recipe_id "
                            "FROM tradeskill_recipe_entries AS tre "
                            "WHERE tre.recipe_id IN (%s) "
                            "AND tre.item_id = %u;", buf2.c_str(), containerId);
		results = QueryDatabase(query);
		if (!results.Success()) {
			Log(Logs::General, Logs::Error, "Error in GetTradeRecipe, re-query: %s", query.c_str());
			Log(Logs::General, Logs::Error, "Error in GetTradeRecipe, error: %s", results.ErrorMessage().c_str());
			return false;
		}

		if(results.RowCount() == 0) { //Recipe contents matched more than 1 recipe, but not in this container
			Log(Logs::General, Logs::Error, "Combine error: Incorrect container is being used!");
			return false;
		}

		if (results.RowCount() > 1) {
			//Recipe contents matched more than 1 recipe in this container
			Log(Logs::General, Logs::Error, "Combine error: Recipe is not unique! %u matches found for container %u. Continuing with first recipe match.", results.RowCount(), containerId);
		}
	}

	auto row = results.begin();
	uint32 recipe_id = (uint32)atoi(row[0]);

	//Right here we verify that we actually have ALL of the tradeskill components..
	//instead of part which is possible with experimentation.
	//This is here because something's up with the query above.. it needs to be rethought out
	bool has_components = true;
	query = StringFormat("SELECT item_id, componentcount "
                        "FROM tradeskill_recipe_entries "
                        "WHERE recipe_id = %i AND componentcount > 0",
                        recipe_id);
	results = QueryDatabase(query);
	if (!results.Success()) {
		return GetTradeRecipe(recipe_id, c_type, some_id, char_id, spec);
	}

	if (results.RowCount() == 0) {
		return GetTradeRecipe(recipe_id, c_type, some_id, char_id, spec);
	}

	for (auto row = results.begin(); row != results.end(); ++row) {
		int component_count = 0;

		for(int x = EQ::invslot::SLOT_BEGIN; x < EQ::invtype::WORLD_SIZE; x++) {
			const EQ::ItemInstance* inst = container->GetItem(x);
			if (!inst) {
				continue;
			}

			const EQ::ItemData* item = GetItem(inst->GetItem()->ID);
			if (!item) {
				continue;
			}

			if (item->ID == atoi(row[0])) {
				component_count++;
			}

			LogTradeskills(
				"[GetTradeRecipe] Component count loop [{}] item [{}] recipe component_count [{}]",
				component_count,
				item->ID,
				atoi(row[1])
			);
		}

		if (component_count != atoi(row[1])) {
			return false;
		}
	}

	Log(Logs::General, Logs::Tradeskills, "Recipe %d is using container %d c_type %d char_id %d", recipe_id, some_id, c_type, char_id);
	return GetTradeRecipe(recipe_id, c_type, some_id, char_id, spec);
}

bool ZoneDatabase::GetTradeRecipe(uint32 recipe_id, uint8 c_type, uint32 some_id,
	uint32 char_id, DBTradeskillRecipe_Struct *spec)
{

	// make where clause segment for container(s)
	std::string containers;
	if (some_id == 0) {
		// world combiner so no item number
		containers = StringFormat("= %u", c_type); 
	}
	else {
		// container in inventory
		containers = StringFormat("IN (%u,%u)", c_type, some_id); 
	}

	std::string query = StringFormat(
		SQL (
			SELECT 
				tradeskill_recipe.id, 
				tradeskill_recipe.tradeskill, 
				tradeskill_recipe.skillneeded,
				tradeskill_recipe.trivial, 
				tradeskill_recipe.nofail, 
				tradeskill_recipe.replace_container,
				tradeskill_recipe.name, 
				tradeskill_recipe.quest
			FROM 
				tradeskill_recipe
                INNER JOIN tradeskill_recipe_entries ON tradeskill_recipe.id = tradeskill_recipe_entries.recipe_id
			WHERE 
				tradeskill_recipe.id = %lu 
				AND tradeskill_recipe_entries.item_id %s 
				AND tradeskill_recipe_entries.iscontainer = 1
                AND tradeskill_recipe.enabled
				%s
			GROUP BY 
				tradeskill_recipe.id
			),
            (unsigned long)recipe_id, 
			containers.c_str(),
			ContentFilterCriteria::apply("tradeskill_recipe").c_str()
	);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		LogError("Error in GetTradeRecipe, query: [{}] ", query.c_str());
		LogError("Error in GetTradeRecipe, error: [{}] ", results.ErrorMessage().c_str());
		return false;
	}

	if (results.RowCount() != 1) {
		return false;
	}

	auto row = results.begin();
	spec->tradeskill = (EQ::skills::SkillType)atoi(row[1]);
	spec->skill_needed	= (int16)atoi(row[2]);
	spec->trivial = (uint16)atoi(row[3]);
	spec->nofail = atoi(row[4]) ? true : false;
	spec->replace_container	= atoi(row[5]) ? true : false;
	spec->name = row[6];
	spec->quest = atoi(row[7]) ? true : false;
	spec->recipe_id = recipe_id;

	//Pull the on-success items...
	query = StringFormat("SELECT item_id,successcount FROM tradeskill_recipe_entries "
                        "WHERE successcount > 0 AND recipe_id = %u", recipe_id);
	results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	if(results.RowCount() < 1) {
		Log(Logs::General, Logs::Error, "Error in GetTradeRecept success: no success items returned");
		return false;
	}

	spec->onsuccess.clear();
	for(auto row = results.begin(); row != results.end(); ++row) {
		uint32 item = (uint32)atoi(row[0]);
		uint8 num = (uint8) atoi(row[1]);
		spec->onsuccess.push_back(std::pair<uint32,uint8>(item, num));
	}

	spec->onfail.clear();
	//Pull the on-fail items...
	query = StringFormat("SELECT item_id, failcount FROM tradeskill_recipe_entries "
                        "WHERE failcount > 0 AND recipe_id = %u", recipe_id);
	results = QueryDatabase(query);
	if (results.Success()) {
		for (auto row = results.begin(); row != results.end(); ++row) {
			uint32	item	= (uint32)atoi(row[0]);
			uint8	num		= (uint8)atoi(row[1]);
			spec->onfail.push_back(std::pair<uint32, uint8>(item, num));
		}
	}

	return true;
}

bool Client::CanIncreaseTradeskill(EQ::skills::SkillType tradeskill) {
	uint32 rawskill = GetRawSkill(tradeskill);
	uint16 maxskill = MaxSkill(tradeskill);

	if (rawskill >= maxskill) //Max skill sanity check
		return false;
	if (rawskill >= RuleI(Quarm, MaxTradeskillCap)) //Max skill sanity check
		return false;

	uint8 Baking	= (GetRawSkill(EQ::skills::SkillBaking) > 200) ? 1 : 0;
	uint8 Smithing	= (GetRawSkill(EQ::skills::SkillBlacksmithing) > 200) ? 1 : 0;
	uint8 Brewing	= (GetRawSkill(EQ::skills::SkillBrewing) > 200) ? 1 : 0;
	uint8 Fletching	= (GetRawSkill(EQ::skills::SkillFletching) > 200) ? 1 : 0;
	uint8 Jewelry	= (GetRawSkill(EQ::skills::SkillJewelryMaking) > 200) ? 1 : 0;
	uint8 Pottery	= (GetRawSkill(EQ::skills::SkillPottery) > 200) ? 1 : 0;
	uint8 Tailoring	= (GetRawSkill(EQ::skills::SkillTailoring) > 200) ? 1 : 0;
	uint8 SkillTotal = Baking + Smithing + Brewing + Fletching + Jewelry + Pottery + Tailoring; //Tradeskills above 200
	uint32 aaLevel	= GetAA(aaNewTanaanCraftingMastery); //New Tanaan AA: Each level allows an additional tradeskill above 200 (first one is free)

	switch (tradeskill) {
		case EQ::skills::SkillBaking:
		case EQ::skills::SkillBlacksmithing:
		case EQ::skills::SkillBrewing:
		case EQ::skills::SkillFletching:
		case EQ::skills::SkillJewelryMaking:
		case EQ::skills::SkillPottery:
		case EQ::skills::SkillTailoring:
			if (aaLevel == 6)
				break; //Maxed AA
			if (SkillTotal == 0)
				break; //First tradeskill freebie
			if ((SkillTotal == (aaLevel + 1)) && (rawskill > 200))
				break; //One of the tradeskills already allowed to go over 200
			if ((SkillTotal >= (aaLevel + 1)) && (rawskill >= 200))
				return false; //One or more tradeskills already at or beyond limit
				break;
		default:
			break; //Other skills unchecked and ability to increase assumed true
	}
	return true;
}

bool ZoneDatabase::EnableRecipe(uint32 recipe_id)
{
	std::string query = StringFormat("UPDATE tradeskill_recipe SET enabled = 1 "
                                    "WHERE id = %u;", recipe_id);
    auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	return results.RowsAffected() > 0;
}

bool ZoneDatabase::DisableRecipe(uint32 recipe_id)
{
	std::string query = StringFormat("UPDATE tradeskill_recipe SET enabled = 0 "
                                    "WHERE id = %u;", recipe_id);
    auto results = QueryDatabase(query);
	if (!results.Success())
		return false;

	return results.RowsAffected() > 0;
}
