#include "hardcoreghost.h"
#include "mob.h"
#include "client.h"
#include "zone.h"
#include "npc.h"
#include "entity.h"

HardcoreGhost::HardcoreGhost(const NPCType *npc_type_data, const glm::vec4& position)
    : Mob(
        npc_type_data->name,
        npc_type_data->lastname,
        npc_type_data->max_hp,
        npc_type_data->max_hp,
        npc_type_data->gender,
        npc_type_data->race,
        npc_type_data->class_,
        (bodyType)npc_type_data->bodytype,
        npc_type_data->deity,
        npc_type_data->level,
        npc_type_data->npc_id,
        npc_type_data->size,
        npc_type_data->runspeed,
        position,
        npc_type_data->light, // innate_light
        npc_type_data->texture,
        npc_type_data->helmtexture,
        npc_type_data->AC,
        npc_type_data->ATK,
        npc_type_data->STR,
        npc_type_data->STA,
        npc_type_data->DEX,
        npc_type_data->AGI,
        npc_type_data->INT,
        npc_type_data->WIS,
        npc_type_data->CHA,
        npc_type_data->haircolor,
        npc_type_data->beardcolor,
        npc_type_data->eyecolor1,
        npc_type_data->eyecolor2,
        npc_type_data->hairstyle,
        npc_type_data->luclinface,
        npc_type_data->beard,
        npc_type_data->armor_tint,
        0,
        npc_type_data->see_invis,            // pass see_invis/see_ivu flags to mob constructor
        npc_type_data->see_invis_undead,
        npc_type_data->see_sneak,
        npc_type_data->see_improved_hide,
        npc_type_data->hp_regen,
        npc_type_data->mana_regen,
        npc_type_data->qglobal,
        npc_type_data->maxlevel,
        npc_type_data->scalerate,
        npc_type_data->armtexture,
        npc_type_data->bracertexture,
        npc_type_data->handtexture,
        npc_type_data->legtexture,
        npc_type_data->feettexture,
        npc_type_data->chesttexture
    )
{
}

void HardcoreGhost::Spawn(Client* client) {
    if (!client) {
        return;
    }

    // Example functionality: Send a message to the client
    client->Message(Chat::White, "Spawning a hardcore ghost!");

    // You can add more functionality here as needed
    // For example, you might want to spawn an NPC or perform other actions

    // Create the Ghost
    NPCType* npc_type = new NPCType;
    memset(npc_type, 0, sizeof(NPCType));

    strcpy(npc_type->name, "Hardcore Ghost");
    npc_type->cur_hp = 1;
    npc_type->max_hp = 1;
    npc_type->race = 1;
    npc_type->gender = 1;
    npc_type->class_ = 1;
    npc_type->deity = 1;
    npc_type->level = 1;
    npc_type->npc_id = 0;
    npc_type->loottable_id = 0;
    npc_type->texture = 1;
    npc_type->light = 0; // spawncommand needs update
    npc_type->runspeed = 1.3f;
    npc_type->d_melee_texture1 = 1;
    npc_type->d_melee_texture2 = 1;
    npc_type->merchanttype = 0;
    npc_type->bodytype = 1;

    npc_type->STR = 150;
    npc_type->STA = 150;
    npc_type->DEX = 150;
    npc_type->AGI = 150;
    npc_type->INT = 150;
    npc_type->WIS = 150;
    npc_type->CHA = 150;

    if (npc_type->size == 0.0f)
        npc_type->size = 6.0f;

    auto ghost = new HardcoreGhost(npc_type, client->GetPosition());

    entity_list.AddHardcoreGhost(ghost);

    if (client) {
        // Notify client of spawn data
        client->Message(Chat::White, "New spawn:");
        client->Message(Chat::White, "Name: %s", ghost->name);
        client->Message(Chat::White, "Race: %u", ghost->race);
        client->Message(Chat::White, "Level: %u", ghost->level);
        client->Message(Chat::White, "Material: %u", ghost->texture);
        client->Message(Chat::White, "Current/Max HP: %i", ghost->max_hp);
        client->Message(Chat::White, "Gender: %u", ghost->gender);
        client->Message(Chat::White, "Class: %u", ghost->class_);
        client->Message(Chat::White, "Bodytype: %u", ghost->bodytype);
        client->Message(Chat::White, "EntityID: %u", ghost->GetID());
    }
}

bool HardcoreGhost::Process()
{
    // Confirmed this is being called 
    // Log(Logs::General, Logs::Info, "HardcoreGhost::Process()");
    // Return false if the mob is dead
    if(tic_timer.Check())
    {
        // Called each tick - 6 secs
        // Log(Logs::General, Logs::Info, "HardcoreGhost::Process() - tic_timer.Check()");

        // Send position updates so the ghost doesn't disappear
        SendPosUpdate();
        SendPosUpdate(2);
    }
    return true;
}

bool HardcoreGhost::Save()
{
    return true;
}

void HardcoreGhost::ShowQuickStats(Client* c)
{
    //This is just #npcstats, but accessible using #showstats 1
    c->Message(Chat::White, "NPC Stats:");
    c->Message(Chat::White, "Name: %s  NpcID: %u EntityID: %i", GetName(), GetNPCTypeID(), GetID());
    c->Message(Chat::White, "Race: %i  Level: %i  Class: %i  Material: %i", GetRace(), GetLevel(), GetClass(), GetTexture());
    c->Message(Chat::White, "Current HP: %i  Max HP: %i Per: %0.2f", GetHP(), GetMaxHP(), GetHPRatio());
    c->Message(Chat::White, "Gender: %i  Size: %f  Bodytype: %d", GetGender(), GetSize(), GetBodyType());
    c->Message(Chat::White, "Runspeed: %f  Walkspeed: %f FlyMode: %d", GetRunspeed(), GetWalkspeed(), GetFlyMode());
    c->Message(Chat::White, "EmoteID: %i", GetEmoteID());

    auto position = GetPosition();
    c->Message(Chat::White, "Position: %f %f %f", position.x, position.y, position.z);
    c->Message(Chat::White, "Hidden: %i", hidden);
}

void HardcoreGhost::ProcessBotInspectionRequest(HardcoreGhost* inspectedBot, Client* client) 
{
    if (inspectedBot && client) 
    {
        EQApplicationPacket* outapp = new EQApplicationPacket(OP_InspectAnswer, sizeof(InspectResponse_Struct));
        InspectResponse_Struct* insr = (InspectResponse_Struct*) outapp->pBuffer;
        insr->TargetID = inspectedBot->GetNPCTypeID();
        insr->PlayerID = inspectedBot->GetID();

        //const EQ::ItemData* item = nullptr;
        //const EQ::ItemInstance* inst = nullptr;

        for (int16 L = EQ::invslot::EQUIPMENT_BEGIN; L <= EQ::invslot::EQUIPMENT_END; L++) 
        {
            //inst = inspectedBot->GetBotItem(L);

            //if (inst) {
            //item = inst->GetItem();

            // For now hardcode one just for testing
            if (L == 1) 
            {
                strcpy(insr->itemnames[L], "Silver Earring");
                insr->itemicons[L] = 544;
            }
            else 
            {
                insr->itemnames[L][0] = '\0';
                //insr->itemicons[L] = 0xFFFFFFFF;
            }
            // else {
            //  insr->itemnames[L][0] = '\0';
            //  insr->itemicons[L] = 0xFFFFFFFF;
            // }
        }

        //strcpy(insr->text, inspectedBot->GetInspectMessage().text);
        Log(Logs::General, Logs::Info, "Sending InspectAnswer to client from HardcoreGhost");

        client->QueuePacket(outapp); // Send answer to requester
        safe_delete(outapp);
    }
}

// Define the pure virtual functions from the Mob class
bool HardcoreGhost::Death(Mob* killerMob, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, uint8 killedby, bool bufftic) {
    // Implement the function
    return true;
}

void HardcoreGhost::Damage(Mob* from, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable, int8 buffslot, bool iBuffTic) {
    // Implement the function
}

bool HardcoreGhost::Attack(Mob* other, int hand, int damagePct) {
    // Implement the function
    return true;
}

void HardcoreGhost::SetAttackTimer(bool trigger) {
    // Implement the function
}

bool HardcoreGhost::CombatRange(Mob* other, float dist_squared, bool check_z, bool pseudo_pos) {
    // Implement the function
    return true;
}

uint8 HardcoreGhost::Disarm(float chance) {
    // Implement the function
    return 0;
}

bool HardcoreGhost::AvoidDamage(Mob* attacker, int32 &damage, bool noRiposte, bool isRangedAttack) {
    // Implement the function
    return true;
}

bool HardcoreGhost::AvoidanceCheck(Mob* attacker, EQ::skills::SkillType skillinuse) {
    // Implement the function
    return true;
}

void HardcoreGhost::TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, int32 minBase, int32 damageBonus) {
    // Implement the function
}

bool HardcoreGhost::TryFinishingBlow(Mob *defender, EQ::skills::SkillType skillinuse, uint32 dmgBonus) {
    // Implement the function
    return true;
}

void HardcoreGhost::DoRiposte(Mob* defender) {
    // Implement the function
}
