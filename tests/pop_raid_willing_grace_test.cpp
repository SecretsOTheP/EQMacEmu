#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using uint32=uint32_t; using uint8=uint8_t; using int16=int16_t;
constexpr uint32 GUILD_NONE=0xffffffff;
constexpr int ClassicEQ=0, KunarkEQ=1, IKSAR=128, minStatusToIgnoreZoneFlags=80;
namespace Zones { constexpr uint32 ECOMMONS=22, BAZAAR=151, NEXUS=152, POKNOWLEDGE=202, POTRANQUILITY=203; }
namespace Chat { enum {Red,Yellow}; }
namespace StringID { enum {NO_EXPAN}; }
#define Log(...) ((void)0)
#define LogError(...) ((void)0)
#define RuleI(a,b) rules.a##_##b
#define RuleB(a,b) rules.a##_##b
struct Rules { int GM_MinStatusToZoneAnywhere=200, Quarm_MinStatusToZoneIntoAnyGuildZone=100;
 bool Quarm_EnableGuildZoneRequirementOnEntry=true, Quarm_RestrictIksarsToKunark=false,
 Quarm_EastCommonMules=false, Quarm_AllowGuildInstanceBoot=true; } rules;
uint64_t now=0;
struct Timer { bool enabled=false; uint64_t end=0; unsigned starts=0;
 bool Enabled(){return enabled;} void Disable(){enabled=false;}
 void Start(uint32 ms){enabled=true;end=now+ms;++starts;}
 bool Check(bool reset=true){if(!enabled||now<end)return false;if(reset)end=now+5000;return true;}
};
namespace DataBucket { std::string enabled="1", percent="1";
 std::string GetData(const char* k){return std::string(k)=="pop_alt_access_enabled"?enabled:percent;}
}
struct ZoneBanishPoint { uint32 target_zone_id=0; float x=0,y=0,z=0,heading=0; };
struct Zone { uint32 id=217,guild=7; uint8 expansion=4; ZoneBanishPoint banish;
 uint32 GetZoneID(){return id;} uint32 GetGuildID(){return guild;}
 const char* GetShortName(){return "testzone";} uint8 GetZoneExpansion(){return expansion;}
 ZoneBanishPoint GetZoneBanishPoint(){return banish;}
} zone_value, *zone=&zone_value;
const char* ZoneName(uint32){return "testzone";}
struct Database { bool success=true, flag=true, pvp_success=true,pvp_enabled=true; int minlevel=46,minstatus=0,expansion=4;
 bool GetSafePoints(const char*,float*,float*,float*,float*,int16* status,uint8* level,char* needed,uint8* exp){
  *status=minstatus;*level=minlevel;*exp=expansion;std::strcpy(needed,flag?"flag":"");return success;}
 bool GetPVPZoneAccess(const char*,bool& on){on=pvp_enabled;return pvp_success;}
} database;
struct Raid { bool guild_ok=true; int total=3,flagged=1;
 bool CanRaidEngageRaidTarget(uint32){return guild_ok;}
 bool MeetsFlagRequirement(uint32,uint8 p){return total>0&&flagged*100>=total*p;}
} raid_value;
struct Client {
 int admin=0,level=65,race=1;uint32 guild=7;bool flag=false,mule=false;Raid* raid=&raid_value;
 struct {uint32 expansions=15;} m_pp;
 Timer pop_raid_willing_check_timer,pop_raid_willing_grace_timer;
 bool pop_raid_willing_flag_required=false,pop_raid_willing_combat_recheck=false;
 std::vector<std::string> messages;unsigned moves=0,binds=0;uint32 dest=0,dest_guild=0;float dx=0,dy=0,dz=0;
 int Admin(){return admin;} int GetLevel(){return level;} int GetBaseRace(){return race;}
 bool IsMule(){return mule;} uint32 GuildID(){return guild;} bool HasZoneFlag(uint32){return flag;}
 Raid* GetRaid(){return raid;} const char* GetName(){return "Test";}
 void Message(int,const char* s,...){messages.emplace_back(s);}void Message_StringID(int,int){}
 void MovePCGuildID(uint32 z,uint32 g,float x,float y,float zz,float,int){++moves;dest=z;dest_guild=g;dx=x;dy=y;dz=zz;}
 void GoToBind(){++binds;}
 bool CanRaidWillIntoPopZone(uint32);
 bool CanBeInZone(uint32 zoneid=0,uint32 guildid=0,bool* denied=nullptr);
 bool CheckZoneAccessWithGrace();void ProcessPopRaidWillingAccess();void ReturnToTranquility();
 void BootFromGuildInstance(bool force=false,bool raid_failure=false);
};
#include "production.inc"
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<"\n";std::abort();}}
void reset(){now=0;rules=Rules{};database=Database{};zone_value=Zone{};raid_value=Raid{};DataBucket::enabled="1";DataBucket::percent="1";}
int main(){
 reset();Client c;check(c.CheckZoneAccessWithGrace(),"valid borrowed entry");check(c.pop_raid_willing_check_timer.Enabled(),"monitor borrowed access");
 check(!c.pop_raid_willing_grace_timer.Enabled(),"no deadline while eligible");
 c.raid=nullptr;now=10000;check(c.CheckZoneAccessWithGrace(),"raid removal starts grace");
 check(c.pop_raid_willing_grace_timer.end==190000,"full three minutes from loss");
 for(now=15000;now<190000;now+=5000)c.ProcessPopRaidWillingAccess();
 check(c.moves==0&&c.pop_raid_willing_grace_timer.starts==1,"polling never refreshes deadline");
 now=190000;c.ProcessPopRaidWillingAccess();check(c.moves==1&&c.dest==203&&c.dest_guild==GUILD_NONE,"expiry returns to open Tranquility");
 check(c.dx==-4&&c.dy==-191&&c.dz==-628,"known safe destination");check(!c.pop_raid_willing_check_timer.Enabled(),"stop polling during eviction");
 reset();Client rejoin;rejoin.raid=nullptr;check(rejoin.CheckZoneAccessWithGrace(),"rez/reconnect without raid allowed temporarily");
 now=180000;rejoin.raid=&raid_value;rejoin.ProcessPopRaidWillingAccess();check(!rejoin.moves&&!rejoin.pop_raid_willing_grace_timer.Enabled(),"rejoin at deadline cancels eviction");
 rejoin.raid=nullptr;now=200000;rejoin.CheckZoneAccessWithGrace();check(rejoin.pop_raid_willing_grace_timer.end==380000,"new loss after recovery gets fresh deadline");
 rejoin.flag=true;rejoin.ProcessPopRaidWillingAccess();check(!rejoin.moves&&!rejoin.pop_raid_willing_grace_timer.Enabled(),"earning permanent flag cancels deadline");
 check(!rejoin.pop_raid_willing_check_timer.Enabled(),"fully flagged own guild no polling");
 reset();Client ratio;DataBucket::percent="85";check(ratio.CheckZoneAccessWithGrace(),"insufficient raid ratio gives grace");raid_value.flagged=3;ratio.ProcessPopRaidWillingAccess();check(!ratio.pop_raid_willing_grace_timer.Enabled(),"ratio restored");
 reset();Client foreign;foreign.guild=9;foreign.raid=nullptr;check(foreign.CheckZoneAccessWithGrace(),"foreign guest can recover raid invite");
 foreign.raid=&raid_value;raid_value.guild_ok=false;now=10000;foreign.ProcessPopRaidWillingAccess();check(foreign.pop_raid_willing_grace_timer.end==180000,"ineligible foreign raid does not refresh");
 raid_value.guild_ok=true;foreign.ProcessPopRaidWillingAccess();check(!foreign.pop_raid_willing_grace_timer.Enabled(),"eligible host raid clears guest grace");
 reset();Client combat;combat.flag=true;combat.raid=nullptr;combat.BootFromGuildInstance(false,true);check(!combat.binds&&!combat.moves&&combat.pop_raid_willing_grace_timer.Enabled(),"combat boot waits for invite");
 now=179999;combat.BootFromGuildInstance(false,true);check(!combat.moves&&combat.pop_raid_willing_grace_timer.starts==1,"combat cannot restart clock");
 now=180000;combat.BootFromGuildInstance(false,true);check(combat.dest==203&&!combat.binds,"combat expiry uses Tranquility");
 reset();Client recovered;recovered.flag=true;recovered.raid=nullptr;recovered.BootFromGuildInstance(false,true);recovered.raid=&raid_value;recovered.ProcessPopRaidWillingAccess();check(!recovered.moves&&!recovered.pop_raid_willing_grace_timer.Enabled(),"combat membership restored");
 for(int kind=0;kind<7;++kind){reset();Client bad;bad.raid=nullptr;
  if(kind==0)bad.level=1;if(kind==1)database.minstatus=50;if(kind==2)bad.m_pp.expansions=0;
  if(kind==3)bad.mule=true;if(kind==4){rules.Quarm_RestrictIksarsToKunark=true;bad.race=IKSAR;}
  if(kind==5){zone->guild=1;database.pvp_enabled=false;}if(kind==6)database.success=false;
  check(!bad.CheckZoneAccessWithGrace()&&!bad.pop_raid_willing_grace_timer.Enabled(),"non-raid failure never gets grace");
 }
 reset();Client strict;strict.raid=nullptr;check(!strict.CanBeInZone(217),"ordinary door/port entry remains denied");check(!strict.pop_raid_willing_grace_timer.Enabled(),"entry check cannot start grace");
 reset();Client off;off.raid=nullptr;DataBucket::enabled="0";check(!off.CheckZoneAccessWithGrace()&&!off.pop_raid_willing_grace_timer.Enabled(),"disabled feature no grace");
 reset();Client nonpop;nonpop.raid=nullptr;zone->id=158;check(!nonpop.CheckZoneAccessWithGrace(),"non-PoP unchanged");nonpop.BootFromGuildInstance(false,true);check(nonpop.binds==1,"non-PoP boot unchanged");
 reset();Client forced;forced.BootFromGuildInstance(true);check(forced.binds==1&&!forced.pop_raid_willing_grace_timer.Enabled(),"forced kick unaffected");
 reset();Client dll;dll.BootFromGuildInstance();check(dll.binds==1,"unrelated guild kick unaffected");
 reset();Client staff;staff.admin=200;staff.raid=nullptr;check(staff.CheckZoneAccessWithGrace()&&!staff.pop_raid_willing_check_timer.Enabled(),"staff bypass unchanged");
 reset();check(AcceptedRez(1,217,7,217,7),"matching accepted rez defers flag check");
 check(!AcceptedRez(0,217,7,217,7),"unsolicited entry not a rez");check(!AcceptedRez(1,217,7,218,7),"rez cannot authorize other zone");
 check(!AcceptedRez(1,217,7,217,8),"rez cannot authorize other instance");check(!AcceptedRez(1,0,7,217,7),"ordinary solicited move not rez");
 check(!AcceptedRez(1,158,7,158,7),"non-PoP rez unchanged");DataBucket::enabled="0";check(!AcceptedRez(1,217,7,217,7),"disabled alternate access preserves rez check");
 std::cout<<"PASS "<<checks<<" production-code grace, permission and resurrection checks\n";
}
