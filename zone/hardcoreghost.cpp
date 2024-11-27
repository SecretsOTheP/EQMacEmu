#include "hardcoreghost.h"
#include "hardcoreghost_database.h"
#include "mob.h"
#include "client.h"
#include "zone.h"
#include "npc.h"
#include "entity.h"

HardcoreGhost* HardcoreGhost::LoadGhost(const std::string& ghost_name)
{
    if (ghost_name.empty()) {
        return nullptr;
    }

    HardcoreGhost* loaded_ghost = nullptr;

    if (!HardcoreGhostDatabase::LoadHardcoreGhost(ghost_name, loaded_ghost)) 
        return loaded_ghost;

    return loaded_ghost;
}

HardcoreGhost::HardcoreGhost(const NPCType *npc_type_data, uint32_t ghost_id)
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
        { 0.0f, 0.0f, 0.0f, 0.0f }, // create zeroed out position

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
    _ghostID = ghost_id;
}

void HardcoreGhost::Spawn(const glm::vec4& in_position, Client* client) 
{
    if (client) 
    {
        client->Message(Chat::White, "Spawning a hardcore ghost!");
    }

    SetPosition(in_position);

    entity_list.AddHardcoreGhost(this);

    if (client) 
    {
        // Notify client of spawn data
        client->Message(Chat::White, "New spawn:");
        client->Message(Chat::White, "Name: %s", name);
        client->Message(Chat::White, "Race: %u", race);
        client->Message(Chat::White, "Level: %u", level);
        client->Message(Chat::White, "Material: %u", texture);
        client->Message(Chat::White, "Current/Max HP: %i", max_hp);
        client->Message(Chat::White, "Gender: %u", gender);
        client->Message(Chat::White, "Class: %u", class_);
        client->Message(Chat::White, "Bodytype: %u", bodytype);
        client->Message(Chat::White, "EntityID: %u", GetID());
    }

    EquipItems();
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

void HardcoreGhost::ProcessGhostInspectionRequest(HardcoreGhost* inspectedGhost, Client* client) 
{
    if (inspectedGhost && client) 
    {
        EQApplicationPacket* outapp = new EQApplicationPacket(OP_InspectAnswer, sizeof(InspectResponse_Struct));
        InspectResponse_Struct* insr = (InspectResponse_Struct*) outapp->pBuffer;
        insr->TargetID = inspectedGhost->GetNPCTypeID();
        insr->PlayerID = inspectedGhost->GetID();

        const EQ::ItemData* item = nullptr;
        const EQ::ItemInstance* inst = nullptr;

        for (int16 L = EQ::invslot::EQUIPMENT_BEGIN; L <= EQ::invslot::EQUIPMENT_END; L++) 
        {
            inst = inspectedGhost->GetGhostItem(L);

            if (inst) 
            {
                item = inst->GetItem();
                if (item) 
                {
                    strcpy(insr->itemnames[L], item->Name);
                    insr->itemicons[L] = item->Icon;
                }
                else 
                {
                    insr->itemnames[L][0] = '\0';
                    insr->itemicons[L] = 0xFFFFFFFF;
                }
            }
            else 
            {
                insr->itemnames[L][0] = '\0';
                insr->itemicons[L] = 0xFFFFFFFF;
            }
        }

        //strcpy(insr->text, inspectedGhost->GetInspectMessage().text);
        Log(Logs::General, Logs::Info, "Sending InspectAnswer to client from HardcoreGhost");

        client->QueuePacket(outapp); // Send answer to requester
        safe_delete(outapp);
    }
}

NPCType *HardcoreGhost::FillNPCTypeStruct(
    const std::string& ghostName,
    const std::string& ghostLastName,
    uint8 ghostLevel,
    uint16 ghostRace,
    uint8 ghostClass,
    uint8 gender,
    float size,
    uint32 face,
    uint32 hairStyle,
    uint32 hairColor,
    uint32 eyeColor,
    uint32 eyeColor2,
    uint32 beard,
    uint32 beardColor
) {
    auto n = new NPCType{ 0 };

    strn0cpy(n->name, ghostName.c_str(), sizeof(n->name));
    strn0cpy(n->lastname, ghostLastName.c_str(), sizeof(n->lastname));

    n->size = size;
    n->runspeed = 1.25f;
    n->gender = gender;
    n->race = ghostRace;
    n->class_ = ghostClass;
    n->deity = Deity::Agnostic1;
    n->level = ghostLevel;
    n->haircolor = hairColor;
    n->beardcolor = beardColor;
    n->eyecolor1 = eyeColor;
    n->eyecolor2 = eyeColor2;
    n->hairstyle = hairStyle;
    n->luclinface = face;
    n->beard = beard;
    n->maxlevel = ghostLevel;

    // Temp to fill out the rest of the struct
    n->bodytype = 1;
    n->npc_id = 0;
    n->loottable_id = 0;
    n->texture = 1;
    n->light = 0; //spawncommand needs to update
    n->d_melee_texture1 = 1;
    n->d_melee_texture2 = 1;
    n->merchanttype = 0;
    n->cur_hp = 1;
    n->max_hp = 1;
    n->AC = 12;
    n->ATK = 75;
    n->STR = 75;
    n->STA = 75;
    n->DEX = 75;
    n->AGI = 75;
    n->INT = 75;
    n->WIS = 75;
    n->CHA = 75;
    n->MR = 25;
    n->FR = 25;
    n->CR = 25;
    n->PR = 15;
    n->DR = 15;
    n->hp_regen = 1;
    n->mana_regen = 1;

    return n;
}

void HardcoreGhost::EquipItems() 
{
    // Load from the DB
    GetGhostItems(m_inv);

    // Equip the items
    const EQ::ItemInstance* inst = nullptr;
    const EQ::ItemData* item = nullptr;
    for (int slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::EQUIPMENT_END; ++slot_id) {
        inst = GetGhostItem(slot_id);
        if (inst) {
            item = inst->GetItem();
            GhostAddEquipItem(slot_id, item->ID);
        }
    }
    UpdateEquipmentLight();
}

// Retrieves all the inventory records from the database for this ghost.
void HardcoreGhost::GetGhostItems(EQ::InventoryProfile &inv)
{
    if (!_ghostID) {
        return;
    }

    if (!HardcoreGhostDatabase::LoadItems(_ghostID, inv)) {
        return;
    }
}

// Returns the item id that is in the ghost inventory collection for the specified slot.
EQ::ItemInstance* HardcoreGhost::GetGhostItem(uint16 slot_id) 
{
    EQ::ItemInstance* item = m_inv.GetItem(slot_id);
    if (item) {
        return item;
    }

    return nullptr;
}

// Adds the specified item it ghost  to the NPC equipment array and to the ghost inventory collection.
void HardcoreGhost::GhostAddEquipItem(uint16 slot_id, uint32 item_id) 
{
    Log(Logs::Detail, Logs::Inventory, "Character: %s trying to add item %d to slot %d", GetName(), item_id, slot_id);
    if (item_id) 
    {
        if (uint8 material_from_slot = EQ::InventoryProfile::CalcMaterialFromSlot(slot_id); material_from_slot != EQ::textures::materialInvalid) 
        {
            equipment[slot_id] = item_id; // npc has more than just material slots. Valid material should mean valid inventory index
            if (_ghostID) 
            { 
                Log(Logs::Detail, Logs::Inventory, "Character: %s sending wear change for slot %d", GetName(), slot_id);
                SendWearChange(material_from_slot);
            }
        }
    }
}

uint32 HardcoreGhost::GetEquipment(uint8 material_slot) const
{
    if (material_slot > 8) {
        return 0;
    }
    int invslot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);

    if (material_slot == EQ::textures::weaponPrimary && !equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange]) {
        invslot = EQ::invslot::slotRange;
    }

    if (invslot == INVALID_INDEX) {
        return 0;
    }
    return equipment[invslot];
}


// Boilerplate functions that don't do anything, but allow compiling

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
