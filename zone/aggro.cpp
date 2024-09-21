/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemu.org)

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
#include "../common/faction.h"
#include "../common/rulesys.h"
#include "../common/spdat.h"
#include "../common/content/world_content_service.h"

#include "client.h"
#include "corpse.h"
#include "entity.h"
#include "mob.h"

#include "map.h"
#include "water_map.h"

extern Zone* zone;
//#define LOSDEBUG 6

// Iterate NPCs to look for something to attack the client
// If a should-be-aggro NPC is found, we have it check for all targets in range
// so they all get on the hate list simultaneously.  Sony seems to have NPCs look for clients.
// Clients search for NPCs rather than the reverse to save cycles (I assume)
void EntityList::CheckClientAggro(Client *around)
{
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC *npc = it->second;

		if (npc->IsPet())
			continue;

		if (!npc->CheckAggro(around) && npc->CheckWillAggro(around, true))
		{
			if (npc->WillAggroNPCs() && !npc->IsEngaged())
				AICheckNPCAggro(npc);	// if npc will become aggro from unaggro state, then also check for other npcs to attack

			AICheckClientAggro(npc);
		}		
	}
}

void EntityList::CheckCorpseDragAggro(Client *around)
{
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC *npc = it->second;

		if (npc->IsPet())
			continue;

		if (npc->IsEngaged())
			continue;

		if (!npc->CheckAggro(around) && npc->CheckDragAggro(around))
		{
			npc->AddToHateList(around);
		}
	}
}

void EntityList::DescribeAggro(Client *to_who, NPC *from_who, float d, bool verbose) {
	float distance_squared = (d * d);

	to_who->Message(
		Chat::White, 
		fmt::format(
			"Describing aggro for {}", 
			to_who->GetTargetDescription(from_who)
		).c_str()
	);

	bool is_engaged = from_who->IsEngaged();
	bool Will_aggro_npcs = from_who->WillAggroNPCs();
	if(is_engaged) {
		Mob *top = from_who->GetHateTop();
		to_who->Message(
			Chat::White, 
			fmt::format(
				"I am currently engaged with {}.",
				(
					!top ?
					"nothing" :
					from_who->GetTargetDescription(top)
					)
			).c_str()
		);
	}

	if(verbose) {
		int faction_id = from_who->GetPrimaryFaction();
		Mob *owner = from_who->GetOwner();
		if (owner) {
			faction_id = owner->GetPrimaryFaction();
		}

		std::string faction_name = (
			faction_id > 0 ?
			database.GetFactionName(faction_id) :
			(
				faction_id == 0 ?
				"None" :
				fmt::format(
					"Special Faction {}",
					faction_id
				)
			)
		);

		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is on Faction {} ({}).",
				to_who->GetTargetDescription(from_who),
				faction_name,
				faction_id
			).c_str()
		);
	}

	for (const auto &npc_entity : entity_list.GetNPCList()) {
		auto entity_id = npc_entity.first;
		auto npc = npc_entity.second;
		if (npc == from_who) {
			continue;
		}

		if (DistanceSquared(npc->GetPosition(), from_who->GetPosition()) > distance_squared) {
			continue;
		}

		if (is_engaged) {
			uint32 hate_amount = from_who->GetHateAmount(npc);
			to_who->Message(
				Chat::White,
				fmt::format(
					"{} is {}on my hate list{}.",
					to_who->GetTargetDescription(npc),
					!hate_amount ? "not " : "",
					(
						!hate_amount ?
						"" :
						fmt::format(
							" with a hate amount of {}.",
							hate_amount
						)
						)
				).c_str()
			);
		} else if (!Will_aggro_npcs && npc->IsNPC()) {
			to_who->Message(
				Chat::White,
				fmt::format(
					"{} is an NPC and my npc_aggro is disabled.",
					to_who->GetTargetDescription(npc)
				).c_str()
			);
		}
		else {
			from_who->DescribeAggro(to_who, npc, verbose);
		}
	}
}

void NPC::DescribeAggro(Client *to_who, Mob *mob, bool verbose) {
	//this logic is duplicated from below, try to keep it up to date.
	float aggro_range = GetAggroRange();

	float x_range = std::abs(mob->GetX() - GetX());
	float y_range = std::abs(mob->GetY() - GetY());
	float z_range = std::abs(mob->GetZ() - GetZ());
	if (
		x_range > aggro_range ||
		y_range > aggro_range ||
		z_range > aggro_range
		) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is out of range. X Range: {} Y Range: {} Z Range: {} Aggro Range: {}",
				to_who->GetTargetDescription(mob),
				x_range,
				y_range,
				z_range,
				aggro_range
			).c_str()
		);
		return;
	}

	if (mob->IsInvisible(this)) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is invisible to me.",
				to_who->GetTargetDescription(mob)
			).c_str()
		);
		return;
	}

	if((mob->IsClient() &&
		(!mob->CastToClient()->Connected()
		|| mob->CastToClient()->IsLD()
		|| mob->CastToClient()->IsBecomeNPC()
		|| mob->CastToClient()->GetGM()
		)
		))
	{
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is a GM or is not connected.",
				to_who->GetTargetDescription(mob)
			).c_str()
		);
		return;
	}


	if (mob == GetOwner()) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is my owner.",
				to_who->GetTargetDescription(mob)
			).c_str()
		);
		return;
	}

	if(mob->IsPet() && !mob->IsCharmedPet())
	{
		to_who->Message(
			Chat::White,
			fmt::format(
			"{} is a summoned pet. ",
				to_who->GetTargetDescription(mob)
			).c_str()
		);
		return;
	}

	float distance_squared = DistanceSquared(mob->GetPosition(), m_Position);
	float aggro_range_squared = (aggro_range * aggro_range);
	if(distance_squared > aggro_range_squared) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is out of range. Distance: {:.2f} Aggro Range: {:.2f}",
				to_who->GetTargetDescription(mob),
				distance_squared,
				aggro_range_squared
			).c_str()
		);
		return;
	}

	if (GetLevel() < 18 && mob->GetLevelCon(GetLevel()) == CON_GREEN && GetBodyType() != BT_Undead && !IsAggroOnPC()) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} considers Red to me.",
				to_who->GetTargetDescription(mob)
			).c_str()
		);
		return;
	}

	if(verbose) {
		int faction_id = GetPrimaryFaction();
		int mob_faction_id = mob->GetPrimaryFaction();
		Mob *owner = GetOwner();
		if (owner) {
			faction_id = owner->GetPrimaryFaction();
		}

		owner = mob->GetOwner();
		if (mob_faction_id) {
			mob_faction_id = owner->GetPrimaryFaction();
		}

		if(!mob_faction_id) {
			to_who->Message(
				Chat::White,
				fmt::format(
					"{} has no primary Faction.",
					to_who->GetTargetDescription(mob)
				).c_str()
			);
		} else if(mob_faction_id < 0) {
			to_who->Message(
				Chat::White,
				fmt::format(
					"{} is on special Faction {}.",
					to_who->GetTargetDescription(mob),
					mob_faction_id
				).c_str()
			);
		} else {
			auto faction_name = database.GetFactionName(mob_faction_id);
			bool has_entry = false;
			for (auto faction : faction_list) {
				if (static_cast<int>(faction->factionID) == mob_faction_id) {
					to_who->Message(
						Chat::White,
						fmt::format(
							"{} has {} standing with Faction {} ({}) with their Faction Level of {}",
							to_who->GetTargetDescription(mob),
							(
								faction->npc_value != 0 ?
								(
									faction->npc_value > 0 ?
									"positive" :
									"negative"
									) :
								"neutral"
								),
							faction_name,
							faction->factionID,
							faction->npc_value
						).c_str()
					);
					has_entry = true;
					break;
				}
			}

			if (!has_entry) {
				to_who->Message(
					Chat::White,
					fmt::format(
						"{} is on Faction {} ({}), for which I do not have an entry.",
						to_who->GetTargetDescription(mob),
						faction_name,
						mob_faction_id
					).c_str()
				);
			}
		}
	}

	auto faction_value = mob->GetReverseFactionCon(this);
	if (
		!(
			faction_value == FACTION_THREATENINGLY ||
			faction_value == FACTION_SCOWLS
			)
		) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} does not have low enough faction, their Faction Level is {} ({}).",
				to_who->GetTargetDescription(mob),
				FactionValueToString(faction_value),
				static_cast<int>(faction_value)
			).c_str()
		);
		return;
	}

	if(!CheckLosFN(mob)) {
		to_who->Message(
			Chat::White,
			fmt::format(
				"{} is out of sight.",
				to_who->GetTargetDescription(mob)
			).c_str()
		);
	}

	to_who->Message(
		Chat::White,
		fmt::format(
			"{} meets all conditions, I should be attacking them.",
			to_who->GetTargetDescription(mob)
		).c_str()
	);
}

/*
	If you change this function, you should update the above function
	to keep the #aggro command accurate.

	turn_mobs = true will cause unaggro nearby NPCs to randomly turn and face a nearby client.
	this is something Sony did for immersion or whatnot; merchants and idle NPCs will randomly
	face a nearby client.
*/
bool Mob::CheckWillAggro(Mob *mob, bool turn_mobs)
{
	if (!mob) {
		return false;
	}

	// lets make sure we've finished connecting
	// Check if it's a client, and that the client is connected and not linkdead,
	// and that the client isn't Playing an NPC, with thier gm flag on
	// Check if it's not a Interactive NPC

	Mob *oos = mob->IsCharmedPet() ? mob->GetOwnerOrSelf() : mob;
	bool InvalidState = false;
	if (oos->IsClient() && 
		(!oos->CastToClient()->ClientFinishedLoading() ||
			!oos->CastToClient()->Connected() || 
			oos->CastToClient()->IsLD() || 
			oos->CastToClient()->IsBecomeNPC() || 
			oos->CastToClient()->GetGM()))
	{
		return false;
	}

	Mob *ownr = mob->GetOwner();
	float aggro_range = GetAggroRange();
	float x_range = std::abs(mob->GetX() - GetX());
	float y_range = std::abs(mob->GetY() - GetY());
	float z_range = std::abs(mob->GetZ() - GetZ());

	if (
		x_range > aggro_range ||
		y_range > aggro_range ||
		z_range > aggro_range || 
		(mob->IsInvisible(this) || (ownr && ownr->IsInvisible(this)))
		|| (IsMezzed())
		|| (mob == GetOwner()))
	{
		return false;
	}

	// Summoned pets are indifferent
	if ((mob->IsClient() && mob->CastToClient()->has_zomm) || mob->iszomm || (mob->IsPet() && !mob->IsCharmedPet()))
	{
		Log(Logs::Detail, Logs::Aggro, "Zomm or Pet: Skipping aggro.");
		return false;
	}

	float distance_squared = DistanceSquared(mob->GetPosition(), m_Position);
	float aggro_range_squared = aggro_range * aggro_range;

	if (distance_squared > aggro_range_squared) {
		// Skip it, out of range
		return false;
	}

	//Get their current target and faction value now that its required this function call should seem backwards
	FACTION_VALUE faction_value = oos->GetReverseFactionCon(this);

	// Make sure they're still in the zone
	// Are they in range?
	// Are they kos?
	// Are we stupid or are they green
	// and they don't have their gm flag on

	if
	(
		((GetLevel() >= 18)
		|| (GetBodyType() == BT_Undead)
		|| (CastToNPC()->IsAggroOnPC())
		|| (mob->IsClient() && mob->CastToClient()->IsSitting())
		|| (oos->GetLevelCon(GetLevel()) != CON_GREEN))
	&&
		((faction_value == FACTION_SCOWLS
		|| (faction_value == FACTION_THREATENINGLY && zone->random.Roll(THREATENINGLY_ARRGO_CHANCE))))
	)
	{
		//make sure we can see them. last since it is very expensive
		if (zone->SkipLoS() || CheckLosFN(mob)) {
			Log(Logs::Detail, Logs::Aggro, "Check aggro for %s target %s.", GetName(), mob->GetName());
			return true;
		}
	}

	if (turn_mobs && mob->IsClient() && IsNPC() && distance_squared < 600 && !IsEngaged() && !IsMoving() && GetAppearance() == eaStanding && faction_value < FACTION_THREATENINGLY
		&& CastToNPC()->CanTalk() && !CastToNPC()->GetRefaceTimer()->Enabled() && zone->random.Roll(0.01))
	{
		FaceTarget(mob);
		CastToNPC()->GetRefaceTimer()->Start(38000);
		AIhail_timer->Start(38000);
	}

	LogAggro("Is In zone?:[{}]\n", mob->InZone());
	LogAggro("Dist^2: [{}]\n", distance_squared);
	LogAggro("Range^2: [{}]\n", aggro_range_squared);
	LogAggro("Faction: [{}]\n", static_cast<int>(faction_value));
	LogAggro("AlwaysAggroFlag: [{}]\n", CastToNPC()->IsAggroOnPC());
	LogAggro("Int: [{}]\n", GetINT());
	LogAggro("Con: [{}]\n", GetLevelCon(mob->GetLevel()));

	return false;
}

// This is used for checks in certain Velious zones, where dragging a corpse will cause aggro.
bool Mob::CheckDragAggro(Mob *mob) {
	if (!mob)
		return false;

	//sometimes if a client has some lag while zoning into a dangerous place while either invis or a GM
	//they will aggro mobs even though it's supposed to be impossible, to lets make sure we've finished connecting
	if (mob->IsClient()) {
		if (!mob->CastToClient()->ClientFinishedLoading())
			return false;
	}

	Mob *ownr = mob->GetOwner();
	if (ownr && ownr->IsClient() && !ownr->CastToClient()->ClientFinishedLoading())
		return false;

	float iAggroRange = GetAggroRange();

	// Check If it's invisible and if we can see invis
	// Check if it's a client, and that the client is connected and not linkdead,
	// and that the client isn't Playing an NPC, with thier gm flag on
	// Check if it's not a Interactive NPC
	// Trumpcard: The 1st 3 checks are low cost calcs to filter out unnessecary distance checks. Leave them at the beginning, they are the most likely occurence.
	// Image: I moved this up by itself above faction and distance checks because if one of these return true, theres no reason to go through the other information

	float t1, t2, t3;
	t1 = mob->GetX() - GetX();
	t2 = mob->GetY() - GetY();
	t3 = mob->GetZ() - GetZ();
	//Cheap ABS()
	if (t1 < 0)
		t1 = 0 - t1;
	if (t2 < 0)
		t2 = 0 - t2;
	if (t3 < 0)
		t3 = 0 - t3;
	if ((t1 > iAggroRange)
		|| (t2 > iAggroRange)
		|| (t3 > iAggroRange)
		|| (mob->IsInvisible(this))
		|| (mob->IsClient() &&
		(!mob->CastToClient()->Connected()
			|| mob->CastToClient()->IsLD()
			|| mob->CastToClient()->IsBecomeNPC()
			|| mob->CastToClient()->GetGM()
			)
			))
	{
		return(false);
	}

	if (IsMezzed())
	{
		return(false);
	}

	// Skip the owner if this is a pet.
	if (mob == GetOwner()) {
		return(false);
	}

	// Summoned pets are indifferent
	if ((mob->IsClient() && mob->CastToClient()->has_zomm) || mob->iszomm || (mob->IsPet() && !mob->IsCharmedPet()))
	{
		Log(Logs::Detail, Logs::Aggro, "Zomm or Pet: Skipping drag aggro.");
		return(false);
	}

	float dist2 = DistanceSquared(mob->GetPosition(), m_Position);
	float iAggroRange2 = iAggroRange*iAggroRange;

	if (dist2 > iAggroRange2) {
		// Skip it, out of range
		return(false);
	}

	//make sure we can see them. last since it is very expensive
	if (zone->SkipLoS() || CheckLosFN(mob)) {
		Log(Logs::Detail, Logs::Aggro, "Check aggro for %s target %s.", GetName(), mob->GetName());
		return(true);
	}

	Log(Logs::Detail, Logs::Aggro, "Is In zone?:%d\n", mob->InZone());
	Log(Logs::Detail, Logs::Aggro, "Dist^2: %f\n", dist2);
	Log(Logs::Detail, Logs::Aggro, "Range^2: %f\n", iAggroRange2);

	return(false);
}

// Checks for nearby clients and adds them to the hate list when appropriate
// This adds everybody in range at the same time, so newly spawned NPCs will correctly add everybody in range
bool EntityList::AICheckClientAggro(NPC* aggressor)
{
	if (!aggressor)
		return false;

	bool proxAggro = aggressor->GetSpecialAbility(PROX_AGGRO);
	bool proxAggro2 = aggressor->GetSpecialAbility(PROX_AGGRO2);
	bool engaged = aggressor->IsEngaged();
	bool found = false;

	if (!RuleB(Quarm, EnableNPCProximityAggroSystem) && !aggressor->HasEngageNotice() && proxAggro)
		proxAggro = false;

	if (proxAggro2)
		proxAggro = true;

	if (aggressor->GetTarget() && engaged && !proxAggro)		// also check for a target as NPC may be ignoring all haters due to distance
		return false;

	float tankDist = 0.0f;
	if (proxAggro && aggressor->GetTarget())
		tankDist = DistanceSquared(aggressor->GetPosition(), aggressor->GetTarget()->GetPosition());

	for (auto it = client_list.begin(); it != client_list.end(); ++it)
	{
		Client *client = it->second;

		if ((client->IsFeigned() && !aggressor->GetSpecialAbility(IMMUNE_FEIGN_DEATH)) || !client->InZone() || client->IsMule())
			continue;

		if (!aggressor->CheckAggro(client) && aggressor->CheckWillAggro(client))
		{
			if (!engaged || !aggressor->GetTarget()
				|| (proxAggro && DistanceSquared(aggressor->GetPosition(), client->GetPosition()) < tankDist))
			{
				aggressor->AddToHateList(client, 20, 0, false, false, false);
				found = true;
			}
		}
	}
	return found;
}

// Checks for nearby NPCs and adds them to the hate list when appropriate
bool EntityList::AICheckNPCAggro(NPC* aggressor)
{
	if (!aggressor)
		return false;

	bool proxAggro = aggressor->GetSpecialAbility(PROX_AGGRO);
	bool proxAggro2 = aggressor->GetSpecialAbility(PROX_AGGRO2);
	if (!RuleB(Quarm, EnableNPCProximityAggroSystem) && !aggressor->HasEngageNotice() && proxAggro)
		proxAggro = false;
	if(proxAggro2)
		proxAggro = true;
	bool engaged = aggressor->IsEngaged();
	bool found = false;

	if (!aggressor->WillAggroNPCs() || (engaged && aggressor->GetTarget() && !proxAggro))
		return false;

	float tankDist = 0.0f;
	if (proxAggro && aggressor->GetTarget())
		tankDist = DistanceSquared(aggressor->GetPosition(), aggressor->GetTarget()->GetPosition());

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC *npc = it->second;

		if (npc->IsPet())
			continue;

		if (!aggressor->CheckAggro(npc) && aggressor->CheckWillAggro(npc))
		{
			if (!engaged || !aggressor->GetTarget()
				|| (proxAggro && DistanceSquared(aggressor->GetPosition(), npc->GetPosition()) < tankDist))
			{
				aggressor->AddToHateList(npc, 20, 0, false, false, false);
				found = true;
			}
		}
	}
	return found;
}

// This is the same as AICheckNPCAggro(), except we only check charmed pets and more importantly, we bypass npc_aggro for player pets.
bool EntityList::AICheckPetAggro(NPC* aggressor)
{
	if (!aggressor)
		return false;

	if (!zone->HasCharmedNPC || aggressor->IsPet())
		return false;

	bool proxAggro = aggressor->GetSpecialAbility(PROX_AGGRO);
	bool proxAggro2 = aggressor->GetSpecialAbility(PROX_AGGRO2);
	if (proxAggro2)
		proxAggro = true;
	bool engaged = aggressor->IsEngaged();
	bool found = false;

	if (engaged && !proxAggro)
		return false;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC *npc = it->second;

		if (!npc->IsCharmedPet())
			continue;

		if (!npc->IsPlayerOwned() && !aggressor->WillAggroNPCs())
			continue;

		if (!aggressor->CheckAggro(npc) && aggressor->CheckWillAggro(npc))
		{
			if (proxAggro && engaged && !aggressor->CombatRange(npc))
				continue;

			aggressor->AddToHateList(npc, 20);
			found = true;
		}
	}
	return found;
}

int EntityList::FleeAllyCount(Mob *attacker, Mob *exclude)
{
	// Return a list of how many NPCs of the same faction or race are within aggro range with LoS (if required in the zone) of the given exclude Mob.
	if (!attacker)
		return 0;

	int count = 0;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) 
	{
		NPC *mob = it->second;
		if (!mob || (mob == exclude))
			continue;

		if (!zone->SkipLoS())
		{
			if (!exclude->CheckLosFN(mob))
				continue;
		}

		float AggroRange = mob->GetAggroRange();
		float AssistRange = mob->GetAssistRange();

		if(AssistRange > AggroRange)
			AggroRange = AssistRange;

		// Square it because we will be using DistNoRoot
		AggroRange *= AggroRange;

		if (DistanceSquared(mob->GetPosition(), exclude->GetPosition()) > AggroRange)
			continue;

		// If exclude doesn't have a faction, check for buddies based on race. Also exclude common factions such as noob monsters, indifferent, kos, kos animal
		if(exclude->GetPrimaryFaction() != 0 && 
			exclude->GetPrimaryFaction() != 394 && exclude->GetPrimaryFaction() != 463 && exclude->GetPrimaryFaction() != 366 && exclude->GetPrimaryFaction() != 367)
		{
			if (mob->GetPrimaryFaction() != exclude->GetPrimaryFaction())
					continue;
		}
		else
		{
			if (mob->GetBaseRace() != exclude->GetBaseRace() || mob->IsCharmedPet())
				continue;
		}

		Log(Logs::Detail, Logs::EQMac, "%s on faction %d with AggroRange %0.2f is at %0.2f, %0.2f, %0.2f and will count as an ally for %s", mob->GetName(), mob->GetPrimaryFaction(), AggroRange, mob->GetX(), mob->GetY(), mob->GetZ(), exclude->GetName());
		++count;
	}

	return count;

}

int EntityList::StackedMobsCount(Mob *center)
{
	// Return how many mobs are at the same location
	if (!center)
		return 0;

	int count = 0;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC *mob = it->second;
		if (!mob || (mob == center))
			continue;

		if (!mob->IsMoving())
			continue;

		if (!mob->IsEngaged())
			continue;

		if (DistanceSquaredNoZ(mob->GetPosition(), center->GetPosition()) > 3.0f)
			continue;

		++count;
	}

	return count;

}

// if attacker is null, then will try to use target or top hater
void NPC::CallForHelp(Mob* attacker, bool ignoreTimer)
{
	if (IsPet())
		return;

	if (!attacker)
		attacker = GetTarget();
	if (!attacker)
		attacker = GetHateTop();
	if (!attacker)
		return;

	if (ignoreTimer || call_help_timer.Check())
	{
		entity_list.AIYellForHelp(this, attacker);

		if (ignoreTimer)
			call_help_timer.Reset();
	}
}

// CallForHelp() should be used in most cases instead of this
void EntityList::AIYellForHelp(Mob* sender, Mob* attacker)
{
	if(!sender || !attacker)
		return;
	if (sender->GetPrimaryFaction() == 0 )
		return; // well, if we dont have a faction set, we're gonna be indiff to everybody

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it)
	{
		NPC *npc = it->second;
		if (!npc)
			continue;

		if(npc->CheckAggro(attacker) || npc->GetSpecialAbility(IMMUNE_AGGRO))
			continue;

		//// prevent assists if kiter has pull limit and this NPC had been deaggroed sometime within 30 seconds ago (so almost certainly from his train)
		//if (npc->GetAggroDeaggroTime() < 30000
		//	&& entity_list.GetTopHateCount(attacker) >= zone->GetPullLimit())
		//{
		//	continue;
		//}

		uint32 npc_faction = npc->GetNPCFactionID();
		uint32 sender_faction = sender->CastToNPC()->GetNPCFactionID();
		// Invis pulling seems to work when the NPCs are on completely different factions.
		if (attacker->IsInvisible(npc) && npc_faction != sender_faction)
		{
			// This compares primary faction and faction hits to determine if two different factionids are the "same" faction.
			if(!database.SameFactions(npc_faction, sender_faction))
				continue;
		}

		float r = npc->GetAssistRange();
		r = r * r;

		if (
			npc != sender
			&& npc != attacker
			&& npc->GetClass() != MERCHANT
			&& npc->GetPrimaryFaction() != 0
			&& DistanceSquared(npc->GetPosition(), sender->GetPosition()) <= r
			&& ((!npc->IsPet()) || (npc->IsPet() && npc->GetOwner() && !npc->GetOwner()->IsClient() && npc->GetOwner() == sender)) // If we're a pet we don't react to any calls for help if our owner is a client or if our owner was not the one calling for help.
			)
		{
			//if they are in range, make sure we are not green or the attacker is a non-player owned NPC
			//then jump in if they are our friend
			if(attacker->GetLevelCon(npc->GetLevel()) != CON_GREEN ||
				(attacker->IsNPC() && !attacker->IsPlayerOwned()))
			{
				if (RuleB(AlKabor, AllowPetPull))
				{
					if (attacker->IsPet() && npc->GetLevelCon(attacker->GetLevel()) == CON_GREEN)
						return;
				}

				bool useprimfaction = false;
				if(npc->GetPrimaryFaction() == sender->CastToNPC()->GetPrimaryFaction())
				{
					const NPCFactionList *cf = database.GetNPCFactionEntry(npc->GetNPCFactionID());
					if(cf){
						if(cf->assistprimaryfaction != 0)
							useprimfaction = true;
					}
				}

				if(useprimfaction || sender->GetReverseFactionCon(npc) <= FACTION_AMIABLY )
				{
					//attacking someone on same faction, or a friend
					//make sure we can see them.
					if(zone->SkipLoS() || npc->CheckLosFN(sender)) 
					{

						Log(Logs::Detail, Logs::Aggro, "AIYellForHelp(\"%s\",\"%s\") %s attacking %s Dist %f Z %f",
						sender->GetName(), attacker->GetName(), npc->GetName(), attacker->GetName(), DistanceSquared(npc->GetPosition(), sender->GetPosition()), std::abs(sender->GetZ()+npc->GetZ()));

						npc->AddToHateList(attacker, 20, 0, false, false, false);
						npc->SetAssisting(true);
					}
				}
			}
		}
	}
}

/*
solar: returns false if attack should not be allowed
I try to list every type of conflict that's possible here, so it's easy
to see how the decision is made. Yea, it could be condensed and made
faster, but I'm doing it this way to make it readable and easy to modify
*/

bool Mob::IsAttackAllowed(Mob *target, bool isSpellAttack, int16 spellid)
{

	Mob *mob1 = nullptr, *mob2 = nullptr, *tempmob = nullptr;
	Client *c1 = nullptr, *c2 = nullptr, *becomenpc = nullptr;
//	NPC *npc1, *npc2;
	int reverse;

	// some special cases
	if(!target)
		return false;

	if(this == target)	// you can attack yourself
		return true;

	//Lifetap on Eye of Zomm.
	if(target->IsZomm())
		return true;

	if (target->GetSpecialAbility(NO_HARM_FROM_CLIENT) && (IsClient() || (IsPet() && GetOwner()->IsClient())))
		return false;

	// Pets cant attack mezed mobs
	if(IsPet() && GetOwner()->IsClient() && target->IsMezzed()) {
		return false;
	}

	if(IsNPC() && target->IsNPC())
	{
		int32 npc_faction = CastToNPC()->GetPrimaryFaction();
		int32 target_faction = target->CastToNPC()->GetPrimaryFaction();
		if(npc_faction == target_faction && npc_faction != 0)
		{
			Log(Logs::General, Logs::Combat, "IsAttackAllowed failed: %s is on the same faction as %s", GetName(), target->GetName());
			RemoveFromHateList(target);
			return false;
		}

	}

	// can't damage own pet (applies to everthing)
	Mob *target_owner = target->GetOwner();
	Mob *our_owner = GetOwner();
	if(target_owner && target_owner == this)
		return false;
	else if(our_owner && our_owner == target)
		return false;

	// invalidate for swarm pets for later on if their owner is a corpse
	if (IsNPC() && CastToNPC()->GetSwarmInfo() && our_owner &&
			our_owner->IsCorpse() && !our_owner->IsPlayerCorpse())
		our_owner = nullptr;
	if (target->IsNPC() && target->CastToNPC()->GetSwarmInfo() && target_owner &&
			target_owner->IsCorpse() && !target_owner->IsPlayerCorpse())
		target_owner = nullptr;

	//cannot hurt untargetable mobs

	if(target->IsUnTargetable()) {
		if (RuleB(Pets, UnTargetableSwarmPet)) {
			if (target->IsNPC()) {
				if (!target->CastToNPC()->GetSwarmOwner()) {
					return(false);
				}
			} else {
				return(false);
			}
		} else {
			return(false);
		}
	}

	// solar: the format here is a matrix of mob type vs mob type.
	// redundant ones are omitted and the reverse is tried if it falls through.

	// first figure out if we're pets. we always look at the master's flags.
	// no need to compare pets to anything
	mob1 = our_owner ? our_owner : this;
	mob2 = target_owner ? target_owner : target;

	if (!zone->CanDoCombat(mob1, mob2))
		return false;

	reverse = 0;
	do
	{
		if(_CLIENT(mob1))
		{
			if(_CLIENT(mob2))					// client vs client
			{
				c1 = mob1->CastToClient();
				c2 = mob2->CastToClient();

				if	// if both are pvp they can fight
				(
					(bool)c1->GetPVP() &&
					(bool)c2->GetPVP()
				)
					return true;
				else if	// if they're dueling they can go at it
				(
					c1->IsDueling() &&
					c2->IsDueling() &&
					c1->GetDuelTarget() == c2->GetID() &&
					c2->GetDuelTarget() == c1->GetID()
				)
					return true;
				else if (zone->watermap != nullptr)
				{
					glm::vec3 mypos (c1->GetX(), c1->GetY(), c1->GetZ());
					glm::vec3 opos (c2->GetX(), c2->GetY(), c2->GetZ());
					if 
					(
						zone->watermap->InPVP(mypos) &&
						zone->watermap->InPVP(opos)
					)
							return true;
				}	
				else
					return false;
			}
			else if(_NPC(mob2))				// client vs npc
			{
				return true;
			}
			else if(_BECOMENPC(mob2))	// client vs becomenpc
			{
				c1 = mob1->CastToClient();
				becomenpc = mob2->CastToClient();

				if(c1->GetLevel() > becomenpc->GetBecomeNPCLevel())
					return false;
				else
					return true;
			}
			else if(_CLIENTCORPSE(mob2))	// client vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// client vs npc corpse
			{
				return false;
			}
		}
		else if(_NPC(mob1))
		{
			if(_NPC(mob2))						// npc vs npc
			{
/*
this says that an NPC can NEVER attack a faction ally...
this is stupid... somebody else should check this rule if they want to
enforce it, this just says 'can they possibly fight based on their
type', in which case, the answer is yes.
*/
/*				npc1 = mob1->CastToNPC();
				npc2 = mob2->CastToNPC();
				if
				(
					npc1->GetPrimaryFaction() != 0 &&
					npc2->GetPrimaryFaction() != 0 &&
					(
						npc1->GetPrimaryFaction() == npc2->GetPrimaryFaction() ||
						npc1->IsFactionListAlly(npc2->GetPrimaryFaction())
					)
				)
					return false;
				else
*/
					return true;
			}
			else if(_BECOMENPC(mob2))	// npc vs becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// npc vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// npc vs npc corpse
			{
				return false;
			}
		}
		else if(_BECOMENPC(mob1))
		{
			if(_BECOMENPC(mob2))			// becomenpc vs becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// becomenpc vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// becomenpc vs npc corpse
			{
				return false;
			}
		}
		else if(_CLIENTCORPSE(mob1))
		{
			if(_CLIENTCORPSE(mob2))		// client corpse vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// client corpse vs npc corpse
			{
				return false;
			}
		}
		else if(_NPCCORPSE(mob1))
		{
			if(_NPCCORPSE(mob2))			// npc corpse vs npc corpse
			{
				return false;
			}
		}

		// we fell through, now we swap the 2 mobs and run through again once more
		tempmob = mob1;
		mob1 = mob2;
		mob2 = tempmob;
	}
	while( reverse++ == 0 );

	Log(Logs::General, Logs::Combat, "Mob::IsAttackAllowed: don't have a rule for this - %s vs %s\n", this->GetName(), target->GetName());
	return false;
}


// solar: this is to check if non detrimental things are allowed to be done
// to the target. clients cannot affect npcs and vice versa, and clients
// cannot affect other clients that are not of the same pvp flag as them.
// also goes for their pets
bool Mob::IsBeneficialAllowed(Mob *target)
{
	Mob *mob1 = nullptr, *mob2 = nullptr, *tempmob = nullptr;
	Client *c1 = nullptr, *c2 = nullptr;
	int reverse;

	if(!target)
		return false;

	if (target->GetAllowBeneficial())
		return true;

	// solar: see IsAttackAllowed for notes

	// first figure out if we're pets. we always look at the master's flags.
	// no need to compare pets to anything
	mob1 = this->GetOwnerID() ? this->GetOwner() : this;
	mob2 = target->GetOwnerID() ? target->GetOwner() : target;

	// if it's self target or our own pet it's ok
	if(mob1 == mob2)
		return true;

	reverse = 0;
	do
	{
		if(_CLIENT(mob1))
		{
			if(_CLIENT(mob2))					// client to client
			{
				c1 = mob1->CastToClient();
				c2 = mob2->CastToClient();

				if (c2->IsSoloOnly())
				{
					// if the target is solo, don't allow anyone to buff it
					// if the caster is solo, it's fine if they try to buff someone
					return false;
				}

				if (c2->IsSelfFound() == true)
				{
					bool can_get_experience = c1->IsInLevelRange(c2->GetLevel2()) && c2->IsInLevelRange(c1->GetLevel2());
					bool compatible = c1->IsSelfFound() == c2->IsSelfFound();
					if (!compatible || compatible && !can_get_experience)
						return false;
				}

				if (c1->IsDueling() || c2->IsDueling())
				{
					if
					(
						c1->IsDueling() &&
						c2->IsDueling() &&
						c1->GetDuelTarget() == c2->GetID() &&
						c2->GetDuelTarget() == c1->GetID()
					) 
						return true; // if they're dueling they can heal each other too
					
					// if at least one of them is dueling someone, but not each other, then no healing/buffing to/from anyone else
					return false;
				}

				if ((bool)c1->GetPVP() == (bool)c2->GetPVP())
					return true;

			}
			else if(_NPC(mob2))				// client to npc
			{
				/* fall through and swap positions */
			}
			else if(_BECOMENPC(mob2))	// client to becomenpc
			{
				return false;
			}
			else if(_CLIENTCORPSE(mob2))	// client to client corpse
			{
				return true;
			}
			else if(_NPCCORPSE(mob2))	// client to npc corpse
			{
				return false;
			}
		}
		else if(_NPC(mob1))
		{
			if(_CLIENT(mob2))
			{
				return false;
			}
			if(_NPC(mob2))						// npc to npc
			{
				return true;
			}
			else if(_BECOMENPC(mob2))	// npc to becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// npc to client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// npc to npc corpse
			{
				return false;
			}
		}
		else if(_BECOMENPC(mob1))
		{
			if(_BECOMENPC(mob2))			// becomenpc to becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// becomenpc to client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// becomenpc to npc corpse
			{
				return false;
			}
		}
		else if(_CLIENTCORPSE(mob1))
		{
			if(_CLIENTCORPSE(mob2))		// client corpse to client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// client corpse to npc corpse
			{
				return false;
			}
		}
		else if(_NPCCORPSE(mob1))
		{
			if(_NPCCORPSE(mob2))			// npc corpse to npc corpse
			{
				return false;
			}
		}

		// we fell through, now we swap the 2 mobs and run through again once more
		tempmob = mob1;
		mob1 = mob2;
		mob2 = tempmob;
	}
	while( reverse++ == 0 );

	Log(Logs::General, Logs::Spells, "Mob::IsBeneficialAllowed: don't have a rule for this - %s to %s\n", this->GetName(), target->GetName());
	return false;
}

bool NPC::CombatRange(Mob* other, float dist_squared, bool check_z, bool pseudo_pos)
{
	if (!other)
		return(false);

	float size_mod = GetBoundingRadius() + other->GetBoundingRadius();
	size_mod *= 0.75f;

	float z_diff = std::abs(GetZOffset() - other->GetZOffset());
	if (size_mod < 14.0f)
		size_mod = 14.0f;

	if (!IsMoving()) {
		size_mod += 2.0f;
	}

	size_mod += z_diff;

	if (size_mod > 75.0f)
		size_mod = 75.0f;

	if (PermaRooted() && size_mod > 60.0f)
		size_mod = 60.0f;

	float _DistNoRoot = dist_squared;
	glm::vec4 new_pos(other->GetPosition());

	if (dist_squared == 0.0f) {
		// predictive position of client based on deltas and time since last update
		if (pseudo_pos && other->IsClient() && (other->GetDeltaX() != 0.0f || other->GetDeltaY() != 0.0f)) {
			uint32 cur_time = Timer::GetCurrentTime();
			int32 time_delta = cur_time - other->GetLastUpdate();
			if (time_delta > 0 && time_delta < 1500) {
				// use predictive position
				float new_x = (float)time_delta * 0.02f * other->GetDeltaX();
				float new_y = (float)time_delta * 0.02f * other->GetDeltaY();

				new_pos.x += new_x;
				new_pos.y += new_y;
			}
		}
		auto diff = static_cast<glm::vec3>(m_Position) - static_cast<glm::vec3>(new_pos);

		if ((std::abs(diff.x) > size_mod) || (std::abs(diff.y) > size_mod) || (std::abs(diff.z) > size_mod)) {
			return false;
		}

		_DistNoRoot = glm::length2(diff);
	}
	size_mod *= size_mod;

	if (_DistNoRoot <= size_mod)
	{
		return true;
	}
	return false;
}

bool Client::CombatRange(Mob* other, float dist_squared, bool check_z, bool pseudo_pos)
{
	if (!other)
		return(false);

	float size_mod = GetBoundingRadius() + other->GetBoundingRadius();
	size_mod *= 0.75f;

	if (size_mod < 14.0f)
		size_mod = 14.0f;

	size_mod += 2.0f;

	float z_diff = std::abs(GetZOffset() - other->GetZOffset());
	size_mod += z_diff;

	if (size_mod > 75.0f)
		size_mod = 75.0f;

	size_mod *= size_mod;

	float _DistNoRoot = dist_squared;
	if (dist_squared == 0.0f) {
		_DistNoRoot = DistanceSquared(m_Position, other->GetPosition());
	}

	if (_DistNoRoot <= size_mod)
	{
		return true;
	}
	return false;
}

bool Mob::IsInCombatRange(Mob* other, float dist_squared)
{
	if (!other)
		return false;

	if (!IsAIControlled() && CombatRange(other, dist_squared))
	{
		return true;
	}
	else {
		if (IsRooted() || other->IsMoving() || other->IsClient()) {
			if (CombatRange(other, dist_squared))
				return true;
		}
		else if (CombatRange(other, dist_squared) && other->CombatRange(this, dist_squared, true)) {
			return true;
		}
	}

	return false;
}

//Father Nitwit's LOS code
bool Mob::CheckLosFN(Mob* other, bool spell_casting) {
	bool Result = false;

	if(other)
		Result = CheckLosFN(other->GetX(), other->GetY(), other->GetZ(), other->GetSize(), other, spell_casting);

	SetLastLosState(Result);
	return Result;
}

bool Mob::CheckRegion(Mob* other, bool skipwater) {

	if (zone->watermap == nullptr) 
		return true;

	//Hack for kedge and powater since we have issues with watermaps.
	if (GetZoneID() == Zones::POWATER || GetZoneID() == Zones::KEDGE) {
		if (zone->IsWaterZone(other->GetZ()) && zone->IsWaterZone(this->GetZ()) && skipwater)
			return true;
	}

	WaterRegionType ThisRegionType;
	WaterRegionType OtherRegionType;
	auto position = glm::vec3(GetX(), GetY(), GetZ());
	auto other_position = glm::vec3(other->GetX(), other->GetY(), other->GetZ());
	ThisRegionType = zone->watermap->ReturnRegionType(position);
	OtherRegionType = zone->watermap->ReturnRegionType(other_position);

	Log(Logs::Detail, Logs::Maps, "Caster Region: %d Other Region: %d", ThisRegionType, OtherRegionType);

	WaterRegionType AdjThisRegionType = ThisRegionType;
	WaterRegionType AdjOtherRegionType = OtherRegionType;

	if (ThisRegionType == RegionTypeNormal || ThisRegionType == RegionTypeIce || ThisRegionType == RegionTypeSlime || ThisRegionType == RegionTypePVP || ThisRegionType == RegionTypeZoneLine)
	{
		AdjThisRegionType = RegionTypeNormal;
	}

	if (OtherRegionType == RegionTypeNormal || OtherRegionType == RegionTypeIce || OtherRegionType == RegionTypeSlime || OtherRegionType == RegionTypePVP || OtherRegionType == RegionTypeZoneLine)
	{
		AdjOtherRegionType = RegionTypeNormal;
	}

	if (ThisRegionType == RegionTypeWater || ThisRegionType == RegionTypeVWater)
	{
		AdjThisRegionType = RegionTypeWater;
	}

	if (OtherRegionType == RegionTypeWater || OtherRegionType == RegionTypeVWater)
	{
		AdjOtherRegionType = RegionTypeWater;
	}

	if(AdjThisRegionType == AdjOtherRegionType)
		return true;

	return false;
}

// optional parameter 'other' used to calculate a more accurate Z offset
bool Mob::CheckLosFN(float posX, float posY, float posZ, float mobSize, Mob* other, bool spell_casting) {

	glm::vec3 myloc(GetX(), GetY(), GetZ());
	glm::vec3 oloc(posX, posY, posZ);

	if (IsClient() && (GetRace() == DWARF || GetRace() == GNOME || GetRace() == HALFLING))
		myloc.z += 1.0f;

	if(zone->zonemap == nullptr) {
		//not sure what the best return is on error
		//should make this a database variable, but im lazy today
#ifdef LOS_DEFAULT_CAN_SEE
		return(true);
#else
		return(false);
#endif
	}

	Log(Logs::Detail, Logs::Maps, "LOS from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f) sizes: (%.2f, %.2f)", myloc.x, myloc.y, myloc.z, oloc.x, oloc.y, oloc.z, GetSize(), mobSize);
	return zone->zonemap->CheckLoS(myloc, oloc);
}

//offensive spell aggro
int32 Mob::CheckAggroAmount(uint16 spell_id, Mob* target)
{
	int nonDamageHate = 0;
	bool setStandardHate = false;
	int instantHate = 0;
	int slevel = GetLevel();
	int damage = 0;

	// Spell hate for non-damaging spells scales by target NPC hitpoints
	int thp = 10000;
	if (target)
	{
		thp = target->GetMaxHP();
		if (thp < 375)
		{
			thp = 375;		// force a minimum of 25 hate
		}
	}
	int standardSpellHate = thp / 15;
	if (standardSpellHate > 1200)
	{
		standardSpellHate = 1200;
	}

	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		switch (spells[spell_id].effectid[o])
		{
			case SE_CurrentHPOnce:
			case SE_CurrentHP:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
				{
					damage -= val;
					if (spell_id == SPELL_MANABURN)		// it's possible that SE_CurrentHPOnce hate in old EQ was 1 or otherwise capped very low but
						damage = -1;					// it's different on Live now so I can't be sure.  Manaburn had virtually zero aggro however
				}										// so need to hardcode this at least

				break;
			}
			case SE_MovementSpeed:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					setStandardHate = true;
				break;
			}
			case SE_AttackSpeed:
			case SE_AttackSpeed2:
			case SE_AttackSpeed3:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 100)
					setStandardHate = true;
				break;
			}
			case SE_Stun:
			case SE_Blind:
			case SE_Mez:
			case SE_Charm:
			case SE_Fear:
			{
				setStandardHate = true;
				break;
			}
			case SE_ArmorClass:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					setStandardHate = true;
				break;
			}
			case SE_DiseaseCounter: // disease counter hate was removed most likely in early May 2002
			{
				if (!content_service.IsTheShadowsOfLuclinEnabled()) {
					if (IsSlowSpell(spell_id)) {
						break;
					}
					nonDamageHate += standardSpellHate;
				}
				break;
			}
			case SE_PoisonCounter:
			{
				setStandardHate = true;
				break;
			}
			case SE_Root:
			{
				nonDamageHate += 10;
				break;
			}
			case SE_ResistMagic:
			case SE_ResistFire:
			case SE_ResistCold:
			case SE_ResistPoison:
			case SE_ResistDisease:
			case SE_STR:
			case SE_STA:
			case SE_DEX:
			case SE_AGI:
			case SE_INT:
			case SE_WIS:
			case SE_CHA:
			case SE_ATK:
			{
				// stat debuffs (except AC) all do 10 hate per stat-- at least on Live circa 2015.  unknown if this was different in old EQ
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					nonDamageHate += 10;
				break;
			}
			case SE_ResistAll:
			{
				// SE_ResistAll with negative vals are only in NPC AoEs.  does not include malo* spells
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					nonDamageHate += 50;
				break;
			}
			case SE_AllStats:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					nonDamageHate += 70;
				break;
			}
			case SE_SpinTarget:
			case SE_Amnesia:
			case SE_Silence:
			case SE_Destroy:
			{
				setStandardHate = true;
				break;
			}
			case SE_Harmony:
			case SE_CastingLevel:
			case SE_MeleeMitigation:
			case SE_CriticalHitChance:
			case SE_AvoidMeleeChance:
			case SE_RiposteChance:
			case SE_DodgeChance:
			case SE_ParryChance:
			case SE_DualWieldChance:
			case SE_DoubleAttackChance:
			case SE_MeleeSkillCheck:
			case SE_HitChance:
			case SE_IncreaseArchery:
			case SE_DamageModifier:
			case SE_MinDamageModifier:
			case SE_IncreaseBlockChance:
			case SE_Accuracy:
			case SE_DamageShield:
			case SE_SpellDamageShield:
			case SE_ReverseDS:
			{
				nonDamageHate += slevel * 2;		// this is wrong, but don't know what to set these yet; most implemented after PoP
				break;
			}
			case SE_CurrentMana:
			case SE_ManaPool:
			case SE_CurrentEndurance: {
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					nonDamageHate += 10; //Just guessing, the old hate was way too high.
				break;
			}
			case SE_CancelMagic:
			case SE_DispelDetrimental: {
				nonDamageHate += 1;
				break;
			}
			case SE_InstantHate:
			{
				instantHate += CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				// Warrior epic effects; they used to do a AC debuff; need to do this else other procs will do more hate on high HP targets
				if ((spell_id == 1935 || spell_id == 1933) && !content_service.IsTheScarsOfVeliousEnabled()) {
					instantHate = standardSpellHate;
				}
				break;
			}
		}
	}

	if (setStandardHate)
		nonDamageHate += standardSpellHate;

	if (spells[spell_id].HateAdded > 0)
		nonDamageHate = spells[spell_id].HateAdded;		// tash and terror lines.  this overrides the spell hate

	// Torven: this is a guess.  Pet hate needs a low cap else they end up doing crazy amounts of aggro
	if (IsPet() && nonDamageHate > 100)
		nonDamageHate = 100;

	// procs and clickables capped at 400.  Sony did it this way instead of using a 'isproc' parameter
	// damage aggro is still added later, so procs like the SoD's anarchy still do more than 400
	if (!CanClassCastSpell(spell_id) && nonDamageHate > 400) {
		if (!IsClient() || content_service.IsTheScarsOfVeliousEnabled()
			|| (CastToClient()->GetWeaponEffectID() != spell_id && CastToClient()->GetWeaponEffectID(EQ::invslot::slotSecondary) != spell_id)) {
			nonDamageHate = 400;
		}
	}

	// bard spell hate is capped very low.  this was from Live server experiments
	if (GetClass() == BARD)
	{
		if (damage + nonDamageHate > 40)
		{
			nonDamageHate = 0;
		}
		else if (nonDamageHate > 40)
		{
			if (target)
			{
				if (target->GetLevel() >= 20)
				{
					nonDamageHate = 40;
				}
			}
			else if (slevel >= 20)
			{
				nonDamageHate = 40;
			}
		}
	}

	nonDamageHate += instantHate;		// instant hate not included in proc or bard caps; e.g. Enraging Blow procs

	int combinedHate = nonDamageHate + damage;

	if (combinedHate > 0)
	{
		int hateMult = RuleI(Aggro, SpellAggroMod);		// should be 100 for a non-custom server

		if (IsClient())
		{
			std::string item_name;
			hateMult += CastToClient()->GetFocusEffect(focusSpellHateMod, spell_id, item_name);		// Furious Bash focus
		}

		// Spell casting subtlety AA, SK hate multiplier buffs, some raid boss debuffs
		hateMult += aabonuses.hatemod + spellbonuses.hatemod + itembonuses.hatemod;

		combinedHate = (combinedHate * hateMult) / 100;
	}

	// spells on 'belly caster' NPCs do no hate if outside of melee range unless spell has no resist check
	if (spells[spell_id].resisttype != RESIST_NONE && target->GetSpecialAbility(IMMUNE_CASTING_FROM_RANGE) && !CombatRange(target))
		return 0;

	return combinedHate;
}

//healing and buffing aggro
int32 Mob::CheckHealAggroAmount(uint16 spell_id, Mob* target, uint32 heal_possible, bool from_clickable)
{
	int32 AggroAmount = 0;
	uint8 slevel = GetLevel();
	uint8 tlevel = slevel;
	if (target)
		tlevel = target->GetLevel();

	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		switch (spells[spell_id].effectid[o])
		{
			case SE_CurrentHP:
			{
				// regen spells
				if (spells[spell_id].buffduration > 0)
				{
					AggroAmount += 1;
					break;
				}

				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val > 0)
				{
					if (heal_possible < val)
						val = heal_possible;		// aggro is based on amount healed, not including crits/focii/AA multipliers
					if (RuleB(Quarm, VeliousEraAggroCaps))
					{
						if (val > 0)
							val = 1 + 2 * val / 3;		// heal aggro is 2/3rds amount healed
						if (tlevel <= 50 && val > 800)	// heal aggro is capped.  800 was stated in a patch note
							val = 800;
						else if (val > 1500)			// cap after level 50 is 1500 on EQLive as of 2015
							val = 1500;
					}
					AggroAmount += val;
				}
				break;
			}
			case SE_Rune:
			{
				AggroAmount += CalcSpellEffectValue_formula(spells[spell_id].formula[0], spells[spell_id].base[0], spells[spell_id].max[o], slevel, spell_id) * 2;
				break;
			}
			case SE_HealOverTime:
			{
				AggroAmount += 10;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	if (AggroAmount < 10 && IsBuffSpell(spell_id))
	{
		if (IsBardSong(spell_id) && AggroAmount < 2 && !from_clickable)
		{
			AggroAmount = 2;
		}
		else
		{
			// Aggro is reduced to 1 here otherwise items like invis to animals clickies and
			// jboots will create a stupid amount of hate to all NPCs when spammed
			if (from_clickable)
			{
				if (RuleB(AlKabor, ClickyHateExploit) && (spell_id == 255 || spell_id == 894))
				{
					AggroAmount = 9;
				}
				else
				{
					AggroAmount = 1;
				}
			}
			else
			{
				AggroAmount = 9;
			}
		}
	}

	// clickables hate capped at 400
	if (!CanClassCastSpell(spell_id) && AggroAmount > 400)
		AggroAmount = 400;

	if (AggroAmount > 0)
	{
		int HateMod = RuleI(Aggro, SpellAggroMod);

		if (IsClient())
		{
			std::string item_name;
			HateMod += CastToClient()->GetFocusEffect(focusSpellHateMod, spell_id, item_name);
		}

		//Live AA - Spell casting subtlety
		HateMod += aabonuses.hatemod + spellbonuses.hatemod + itembonuses.hatemod;

		AggroAmount = (AggroAmount * HateMod) / 100;
	}

	if (AggroAmount < 0)
		return 0;
	else
		return AggroAmount;
}

// based on http://www.thesafehouse.org/forums/forum/everquest-wing/training-studios/18829-how-much-hate-quantitatively-is-lost-on-a-succesful-evade
// and http://www.ttlg.com/forums/showthread.php?t=5115
void Mob::RogueEvade(Mob *defender)
{
	int reduction = std::min(GetLevel() * 10, 500);		// this scaling is a guess.  the rest should be accurate

	Mob* top = defender->GetTarget();

	if (top && top == this && defender->GetNumHaters() > 2)
	{
		int myHate = defender->GetHateN(1);
		int hater2 = defender->GetHateN(2);
		int hater3 = defender->GetHateN(3);
		int midHate = (hater2 - hater3) / 2 + hater3;
		
		if (midHate < myHate - reduction)
			defender->SetHate(this, midHate);
		else
			defender->AddHate(this, -reduction);
	}
	else
	{
		defender->AddHate(this, -reduction);
	}

	return;
}
