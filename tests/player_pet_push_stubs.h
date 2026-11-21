#pragma once
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <cstdlib>
using uint32=uint32_t;
namespace Chat { enum {White,Red}; }
struct Client {
 std::vector<std::string> messages;
 void Message(int,const char *format,...) { char out[2048];va_list args;va_start(args,format);vsnprintf(out,sizeof(out),format,args);va_end(args);messages.emplace_back(out); }
};
struct Seperator { int argnum=0;const char *arg[4]={"#petpush","","",""}; };
struct RuleManager {
 float value=0.25f;int id=1;
 static RuleManager *Instance();
 int GetActiveRulesetID(){return id;}
 bool SetRule(const std::string&,const std::string &v,void*,bool,bool){value=std::strtof(v.c_str(),nullptr);return true;}
} rules;
RuleManager *RuleManager::Instance(){return &rules;}
#define RuleR(category,name) RuleManager::Instance()->value
struct Result {
 bool ok;std::string value;
 bool Success(){return ok;}
 unsigned RowCount(){return value.empty()?0:1;}
 struct Row {std::string value;const char *operator[](int){return value.c_str();}};
 Row begin(){return {value};}
};
struct Database {
 bool fail=false;std::string saved;std::vector<std::string> queries;
 Result QueryDatabase(const std::string &query) {
  queries.push_back(query);if(fail)return {false,""};
  if(query.find("INSERT")==0){auto pos=query.find("'Pets:PlayerMeleePushMultiplier', '");pos+=std::string("'Pets:PlayerMeleePushMultiplier', '").size();saved=query.substr(pos,query.find('\'',pos)-pos);return {true,""};}
  return {true,saved};
 }
} database;
constexpr unsigned ServerOP_PlayerPetPushUpdate=0x4206;
struct ServerPlayerPetPushUpdate_Struct {uint32 ruleset_id;};
struct ServerPacket {
 unsigned opcode,size;unsigned char *pBuffer;
 ServerPacket(unsigned op,unsigned len):opcode(op),size(len),pBuffer(new unsigned char[len]{}){}
 ~ServerPacket(){delete[] pBuffer;}
};
struct WorldServer {
 bool connected=true,send_ok=true;int sent=0;
 bool Connected(){return connected;}
 bool SendPacket(ServerPacket*){++sent;return send_ok;}
} worldserver;
struct ZoneServerList {int sent=0;void SendPacket(ServerPacket*){++sent;}} zoneserver_list;
#define safe_delete(p) do {delete (p);(p)=nullptr;} while(0)
