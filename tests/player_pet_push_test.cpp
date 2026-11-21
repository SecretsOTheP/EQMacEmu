#include "test_stubs.h"
#include "common/player_pet_push.h"
#include "zone/gm_commands/petpush.cpp"
#include "receiver.inc"
#include "world_receiver.inc"
#include <cassert>
#include <limits>

struct TestMob {
    bool npc=false, client=false;
    unsigned owner=0, swarm=0;
    bool IsNPC() { return npc; }
    bool IsClient() { return client; }
    unsigned GetOwnerID() { return owner; }
    unsigned GetSwarmOwner() { return swarm; }
    TestMob *CastToNPC() { return this; }
};
int main()
{
    int checks=0;
    auto check=[&](bool ok){ assert(ok); ++checks; };
    TestMob player{false,true,0,0}, npc{true,false,0,0}, pet{true,false,1,0}, swarm{true,false,0,1};
    auto lookup=[&](unsigned id)->TestMob* { return id==1 ? &player : (id==2 ? &npc : nullptr); };
    for (float force : {0.05f,0.1f,0.15f,0.2f,0.25f,0.3f,0.4f,2.5f}) {
        for (auto *attacker : {&pet,&swarm}) {
            check(PlayerPetPush::Scale(force,attacker,true,true,0.25f,lookup)==force*0.25f);
            check(PlayerPetPush::Scale(force,attacker,true,true,0.0f,lookup)==0.0f);
            check(PlayerPetPush::Scale(force,attacker,true,true,1.0f,lookup)==force);
            check(PlayerPetPush::Scale(force,attacker,false,true,0.0f,lookup)==force); // PvP unchanged
            check(PlayerPetPush::Scale(force,attacker,true,false,0.0f,lookup)==force); // spell/DS unchanged
        }
        for (auto *attacker : {&player,&npc,static_cast<TestMob*>(nullptr)})
            check(PlayerPetPush::Scale(force,attacker,true,true,0.0f,lookup)==force);
    }
    pet.owner=2;check(PlayerPetPush::Scale(0.2f,&pet,true,true,0.0f,lookup)==0.2f); // NPC's pet
    pet.owner=999;check(PlayerPetPush::Scale(0.2f,&pet,true,true,0.0f,lookup)==0.2f); // orphan unchanged
    pet.swarm=1;check(PlayerPetPush::Scale(0.2f,&pet,true,true,0.0f,lookup)==0.0f); // swarm fallback
    pet.owner=1;pet.swarm=0;
    check(PlayerPetPush::Scale(0.0f,&pet,true,true,0.25f,lookup)==0.0f);
    check(PlayerPetPush::EffectiveMultiplier(std::numeric_limits<float>::quiet_NaN())==0.25f);
    check(PlayerPetPush::EffectiveMultiplier(-1)==0 && PlayerPetPush::EffectiveMultiplier(2)==1);
    float parsed=-1;
    for (const char *text : {"nan","inf","-inf","-0.01","1.01","0.25junk","","1e999","1e-999"})
        check(!PlayerPetPush::ParseMultiplier(text,parsed));
    check(!PlayerPetPush::ParseMultiplier(nullptr,parsed));
    for (const char *text : {"0","0.25",".5","1","1.0"}) check(PlayerPetPush::ParseMultiplier(text,parsed));

    Client c;Seperator sep{};
    command_petpush(&c,&sep);check(database.queries.empty() && worldserver.sent==0 && rules.value==0.25f);
    check(c.messages.front().find("25.0%")!=std::string::npos);
    sep.argnum=1;sep.arg[1]="nan";command_petpush(&c,&sep);check(database.queries.empty());
    sep.arg[1]="0";sep.argnum=2;command_petpush(&c,&sep);check(database.queries.empty());sep.argnum=1;
    worldserver.connected=false;command_petpush(&c,&sep);check(database.queries.empty() && rules.value==0.25f);
    worldserver.connected=true;database.fail=true;command_petpush(&c,&sep);check(rules.value==0.25f && worldserver.sent==0);
    database.fail=false;command_petpush(&c,&sep);check(rules.value==0 && database.saved=="0" && worldserver.sent==1);
    sep.arg[1]="1";command_petpush(&c,&sep);check(rules.value==1 && database.saved=="1");
    sep.arg[1]="0.25";worldserver.send_ok=false;command_petpush(&c,&sep);
    check(rules.value==0.25f && database.saved=="0.25" && c.messages.back().find("world update failed")!=std::string::npos);
    worldserver.send_ok=true;
    ServerPacket notification(ServerOP_PlayerPetPushUpdate,sizeof(ServerPlayerPetPushUpdate_Struct));
    auto *update=reinterpret_cast<ServerPlayerPetPushUpdate_Struct*>(notification.pBuffer);update->ruleset_id=1;
    rules.value=1;Receive(&notification);check(rules.value==0.25f); // second running zone
    rules.value=1;rules.id=2;auto queries=database.queries.size();Receive(&notification);
    check(rules.value==1 && database.queries.size()==queries); // other ruleset untouched
    rules.id=1;database.saved="0";Receive(&notification);check(rules.value==0); // rereads newest DB value
    database.saved="nan";Receive(&notification);check(rules.value==0);
    database.fail=true;Receive(&notification);check(rules.value==0);database.fail=false;
    ServerPacket malformed(ServerOP_PlayerPetPushUpdate,0);queries=database.queries.size();Receive(&malformed);check(database.queries.size()==queries);
    WorldReceive(&malformed);check(zoneserver_list.sent==0);WorldReceive(&notification);check(zoneserver_list.sent==1);
    printf("PASS: %d player-pet push, command, persistence and notification checks\n",checks);
}
