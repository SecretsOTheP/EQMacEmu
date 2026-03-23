#ifdef LUA_EQEMU

#include "lua.hpp"
#include <luabind/luabind.hpp>
#include <luabind/object.hpp>

#include "masterentity.h"
#include "lua_packet.h"

struct Opcodes { };

Lua_Packet::Lua_Packet(int opcode, int size) {
	SetLuaPtrData(new EQApplicationPacket(static_cast<EmuOpcode>(opcode), size));
	owned_ = true;
}

Lua_Packet::Lua_Packet(int opcode, int size, bool raw) {
	if(raw) {
		SetLuaPtrData(new EQApplicationPacket(OP_Unknown, size));
		owned_ = true;
	
		EQApplicationPacket *self = reinterpret_cast<EQApplicationPacket*>(d_);
		self->SetOpcodeBypass(opcode);
	} else {
		SetLuaPtrData(new EQApplicationPacket(static_cast<EmuOpcode>(opcode), size));
		owned_ = true;
	}
}

Lua_Packet& Lua_Packet::operator=(const Lua_Packet& o) {
	if(o.owned_) {
		owned_ = true;
		EQApplicationPacket *app = reinterpret_cast<EQApplicationPacket*>(o.d_);
		if(app) {
			d_ = new EQApplicationPacket(app->GetOpcode(), app->pBuffer, app->size);

			EQApplicationPacket *self = reinterpret_cast<EQApplicationPacket*>(d_);
			self->SetOpcodeBypass(app->GetOpcodeBypass());
		} else {
			d_ = nullptr;
		}
	} else {
		owned_ = false;
		d_ = o.d_;
	}
	return *this;
}

Lua_Packet::Lua_Packet(const Lua_Packet& o) {
	if(o.owned_) {
		owned_ = true;
		EQApplicationPacket *app = reinterpret_cast<EQApplicationPacket*>(o.d_);
		if(app) {
			d_ = new EQApplicationPacket(app->GetOpcode(), app->pBuffer, app->size);

			EQApplicationPacket *self = reinterpret_cast<EQApplicationPacket*>(d_);
			self->SetOpcodeBypass(app->GetOpcodeBypass());
		} else {
			d_ = nullptr;
		}
	} else {
		owned_ = false;
		d_ = o.d_;
	}
}

int Lua_Packet::GetSize() {
	Lua_Safe_Call_Int();
	return static_cast<int>(self->size);
}

int Lua_Packet::GetOpcode() {
	Lua_Safe_Call_Int();
	return static_cast<int>(self->GetOpcode());
}

void Lua_Packet::SetOpcode(int op) {
	Lua_Safe_Call_Void();
	self->SetOpcodeBypass(static_cast<uint16>(op));
}

int Lua_Packet::GetRawOpcode() {
	Lua_Safe_Call_Int();
	return static_cast<int>(self->GetOpcodeBypass());
}

void Lua_Packet::SetRawOpcode(int op) {
	Lua_Safe_Call_Void();
	self->SetOpcode(static_cast<EmuOpcode>(op));
}

void Lua_Packet::WriteInt8(int offset, int value) {
	Lua_Safe_Call_Void();

	if(offset + sizeof(int8) <= self->size) {
		*reinterpret_cast<int8*>(self->pBuffer + offset) = value;
	}
}

void Lua_Packet::WriteInt16(int offset, int value) {
	Lua_Safe_Call_Void();

	if(offset + sizeof(int16) <= self->size) {
		*reinterpret_cast<int16*>(self->pBuffer + offset) = value;
	}
}

void Lua_Packet::WriteInt32(int offset, int value) {
	Lua_Safe_Call_Void();

	if(offset + sizeof(int32) <= self->size) {
		*reinterpret_cast<int32*>(self->pBuffer + offset) = value;
	}
}

void Lua_Packet::WriteInt64(int offset, int64 value) {
	Lua_Safe_Call_Void();

	if(offset + sizeof(int64) <= self->size) {
		*reinterpret_cast<int64*>(self->pBuffer + offset) = value;
	}
}

void Lua_Packet::WriteFloat(int offset, float value) {
	Lua_Safe_Call_Void();

	if(offset + sizeof(float) <= self->size) {
		*reinterpret_cast<float*>(self->pBuffer + offset) = value;
	}
}

void Lua_Packet::WriteDouble(int offset, double value) {
	Lua_Safe_Call_Void();

	if(offset + sizeof(double) <= self->size) {
		*reinterpret_cast<double*>(self->pBuffer + offset) = value;
	}
}

void Lua_Packet::WriteString(int offset, std::string value) {
	Lua_Safe_Call_Void();

	if(offset + value.length() + 1 <= self->size) {
		memcpy(self->pBuffer + offset, value.c_str(), value.length());
		*reinterpret_cast<int8*>(self->pBuffer + offset + value.length()) = 0;
	}
}

void Lua_Packet::WriteFixedLengthString(int offset, std::string value, int string_length) {
	Lua_Safe_Call_Void();

	if(offset + string_length <= static_cast<int>(self->size)) {
		memset(self->pBuffer + offset, 0, string_length);
		memcpy(self->pBuffer + offset, value.c_str(), value.length());
	}
}

int Lua_Packet::ReadInt8(int offset) {
	Lua_Safe_Call_Int();
	if(offset + sizeof(int8) <= self->size) {
		int8 v = *reinterpret_cast<int8*>(self->pBuffer + offset);
		return v;
	}

	return 0;
}

int Lua_Packet::ReadInt16(int offset) {
	Lua_Safe_Call_Int();

	if(offset + sizeof(int16) <= self->size) {
		int16 v = *reinterpret_cast<int16*>(self->pBuffer + offset);
		return v;
	}

	return 0;
}

int Lua_Packet::ReadInt32(int offset) {
	Lua_Safe_Call_Int();

	if(offset + sizeof(int32) <= self->size) {
		int32 v = *reinterpret_cast<int32*>(self->pBuffer + offset);
		return v;
	}

	return 0;
}

int64 Lua_Packet::ReadInt64(int offset) {
	Lua_Safe_Call_Int();

	if(offset + sizeof(int64) <= self->size) {
		int64 v = *reinterpret_cast<int64*>(self->pBuffer + offset);
		return v;
	}

	return 0;
}

float Lua_Packet::ReadFloat(int offset) {
	Lua_Safe_Call_Real();

	if(offset + sizeof(float) <= self->size) {
		float v = *reinterpret_cast<float*>(self->pBuffer + offset);
		return v;
	}

	return 0;
}

double Lua_Packet::ReadDouble(int offset) {
	Lua_Safe_Call_Real();

	if(offset + sizeof(double) <= self->size) {
		double v = *reinterpret_cast<double*>(self->pBuffer + offset);
		return v;
	}

	return 0;
}

std::string Lua_Packet::ReadString(int offset) {
	Lua_Safe_Call_String();

	if(offset < static_cast<int>(self->size)) {
		std::string ret;

		int i = offset;
		for(;;) {
			if(i >= static_cast<int>(self->size)) {
				break;
			}

			char c = *reinterpret_cast<char*>(self->pBuffer + i);

			if(c == '\0') {
				break;
			}

			ret.push_back(c);
			++i;
		}

		return ret;
	}

	return "";
}

std::string Lua_Packet::ReadFixedLengthString(int offset, int string_length) {
	Lua_Safe_Call_String();

	if(offset + string_length <= static_cast<int>(self->size)) {
		std::string ret;

		int i = offset;
		for(;;) {
			if(i >= offset + string_length) {
				break;
			}

			char c = *reinterpret_cast<char*>(self->pBuffer + i);
			ret.push_back(c);
			++i;
		}

		return ret;
	}

	return "";
}

luabind::scope lua_register_packet() {
	return luabind::class_<Lua_Packet>("Packet")
		.def(luabind::constructor<>())
		.def(luabind::constructor<int,int>())
		.def(luabind::constructor<int,int,bool>())
		.property("null", &Lua_Packet::Null)
		.property("valid", &Lua_Packet::Valid)
		.def("GetSize", &Lua_Packet::GetSize)
		.def("GetOpcode", &Lua_Packet::GetOpcode)
		.def("SetOpcode", &Lua_Packet::SetOpcode)
		.def("GetRawOpcode", &Lua_Packet::GetRawOpcode)
		.def("SetRawOpcode", &Lua_Packet::SetRawOpcode)
		.def("WriteInt8", &Lua_Packet::WriteInt8)
		.def("WriteInt16", &Lua_Packet::WriteInt16)
		.def("WriteInt32", &Lua_Packet::WriteInt32)
		.def("WriteInt64", &Lua_Packet::WriteInt64)
		.def("WriteFloat", &Lua_Packet::WriteFloat)
		.def("WriteDouble", &Lua_Packet::WriteDouble)
		.def("WriteString", &Lua_Packet::WriteString)
		.def("WriteFixedLengthString", &Lua_Packet::WriteFixedLengthString)
		.def("ReadInt8", &Lua_Packet::ReadInt8)
		.def("ReadInt16", &Lua_Packet::ReadInt16)
		.def("ReadInt32", &Lua_Packet::ReadInt32)
		.def("ReadInt64", &Lua_Packet::ReadInt64)
		.def("ReadFloat", &Lua_Packet::ReadFloat)
		.def("ReadDouble", &Lua_Packet::ReadDouble)
		.def("ReadString", &Lua_Packet::ReadString)
		.def("ReadFixedLengthString", &Lua_Packet::ReadFixedLengthString);
}

//TODO: Reorder these to match emu_oplist.h again
luabind::scope lua_register_packet_opcodes() {
	return luabind::class_<Opcodes>("Opcode")
		.enum_("constants")
		[
			luabind::value("ExploreUnknown", static_cast<int>(OP_ExploreUnknown)),
			luabind::value("Stamina", static_cast<int>(OP_Stamina)),
			luabind::value("ControlBoat", static_cast<int>(OP_ControlBoat)),
			luabind::value("MobUpdate", static_cast<int>(OP_MobUpdate)),
			luabind::value("ClientUpdate", static_cast<int>(OP_ClientUpdate)),
			luabind::value("ChannelMessage", static_cast<int>(OP_ChannelMessage)),
			luabind::value("SimpleMessage", static_cast<int>(OP_SimpleMessage)),
			luabind::value("FormattedMessage", static_cast<int>(OP_FormattedMessage)),
			luabind::value("TGB", static_cast<int>(OP_TGB)),
			luabind::value("Bind_Wound", static_cast<int>(OP_Bind_Wound)),
			luabind::value("Charm", static_cast<int>(OP_Charm)),
			luabind::value("Begging", static_cast<int>(OP_Begging)),
			luabind::value("MoveCoin", static_cast<int>(OP_MoveCoin)),
			luabind::value("SpawnDoor", static_cast<int>(OP_SpawnDoor)),
			luabind::value("Sneak", static_cast<int>(OP_Sneak)),
			luabind::value("ExpUpdate", static_cast<int>(OP_ExpUpdate)),
			luabind::value("RespondAA", static_cast<int>(OP_RespondAA)),
			luabind::value("AAAction", static_cast<int>(OP_AAAction)),
			luabind::value("BoardBoat", static_cast<int>(OP_BoardBoat)),
			luabind::value("LeaveBoat", static_cast<int>(OP_LeaveBoat)),
			luabind::value("SendExpZonein", static_cast<int>(OP_SendExpZonein)),
			luabind::value("RaidUpdate", static_cast<int>(OP_RaidUpdate)),
			luabind::value("GuildLeader", static_cast<int>(OP_GuildLeader)),
			luabind::value("GuildPeace", static_cast<int>(OP_GuildPeace)),
			luabind::value("GuildRemove", static_cast<int>(OP_GuildRemove)),
			luabind::value("GuildInvite", static_cast<int>(OP_GuildInvite)),
			luabind::value("GuildMOTD", static_cast<int>(OP_GuildMOTD)),
			luabind::value("SetGuildMOTD", static_cast<int>(OP_SetGuildMOTD)),
			luabind::value("GetGuildsList", static_cast<int>(OP_GetGuildsList)),
			luabind::value("GuildInviteAccept", static_cast<int>(OP_GuildInviteAccept)),
			luabind::value("GuildWar", static_cast<int>(OP_GuildWar)),
			luabind::value("GuildDelete", static_cast<int>(OP_GuildDelete)),
			luabind::value("Trader", static_cast<int>(OP_Trader)),
			luabind::value("BecomeTrader", static_cast<int>(OP_BecomeTrader)),
			luabind::value("TraderShop", static_cast<int>(OP_TraderShop)),
			luabind::value("TraderBuy", static_cast<int>(OP_TraderBuy)),
			luabind::value("PetCommands", static_cast<int>(OP_PetCommands)),
			luabind::value("TradeSkillCombine", static_cast<int>(OP_TradeSkillCombine)),
			luabind::value("ShopPlayerBuy", static_cast<int>(OP_ShopPlayerBuy)),
			luabind::value("ShopPlayerSell", static_cast<int>(OP_ShopPlayerSell)),
			luabind::value("ShopPlayerRecharge", static_cast<int>(OP_ShopPlayerRecharge)),
			luabind::value("ShopDelItem", static_cast<int>(OP_ShopDelItem)),
			luabind::value("ShopRequest", static_cast<int>(OP_ShopRequest)),
			luabind::value("ShopEnd", static_cast<int>(OP_ShopEnd)),
			luabind::value("LFGCommand", static_cast<int>(OP_LFGCommand)),
			luabind::value("GroupUpdate", static_cast<int>(OP_GroupUpdate)),
			luabind::value("GroupInvite", static_cast<int>(OP_GroupInvite)),
			luabind::value("GroupDisband", static_cast<int>(OP_GroupDisband)),
			luabind::value("GroupInvite2", static_cast<int>(OP_GroupInvite2)),
			luabind::value("GroupFollow", static_cast<int>(OP_GroupFollow)),
			luabind::value("GroupCancelInvite", static_cast<int>(OP_GroupCancelInvite)),
			luabind::value("Split", static_cast<int>(OP_Split)),
			luabind::value("Jump", static_cast<int>(OP_Jump)),
			luabind::value("ConsiderCorpse", static_cast<int>(OP_ConsiderCorpse)),
			luabind::value("SkillUpdate", static_cast<int>(OP_SkillUpdate)),
			luabind::value("GMEndTrainingResponse", static_cast<int>(OP_GMEndTrainingResponse)),
			luabind::value("GMEndTraining", static_cast<int>(OP_GMEndTraining)),
			luabind::value("GMTrainSkill", static_cast<int>(OP_GMTrainSkill)),
			luabind::value("GMTraining", static_cast<int>(OP_GMTraining)),
			luabind::value("CombatAbility", static_cast<int>(OP_CombatAbility)),
			luabind::value("Track", static_cast<int>(OP_Track)),
			luabind::value("ItemLinkResponse", static_cast<int>(OP_ItemLinkResponse)),
			luabind::value("RezzAnswer", static_cast<int>(OP_RezzAnswer)),
			luabind::value("RezzComplete", static_cast<int>(OP_RezzComplete)),
			luabind::value("SendZonepoints", static_cast<int>(OP_SendZonepoints)),
			luabind::value("SetRunMode", static_cast<int>(OP_SetRunMode)),
			luabind::value("InspectRequest", static_cast<int>(OP_InspectRequest)),
			luabind::value("InspectAnswer", static_cast<int>(OP_InspectAnswer)),
			luabind::value("SenseTraps", static_cast<int>(OP_SenseTraps)),
			luabind::value("DisarmTraps", static_cast<int>(OP_DisarmTraps)),
			luabind::value("Assist", static_cast<int>(OP_Assist)),
			luabind::value("PickPocket", static_cast<int>(OP_PickPocket)),
			luabind::value("LootRequest", static_cast<int>(OP_LootRequest)),
			luabind::value("EndLootRequest", static_cast<int>(OP_EndLootRequest)),
			luabind::value("MoneyOnCorpse", static_cast<int>(OP_MoneyOnCorpse)),
			luabind::value("LootComplete", static_cast<int>(OP_LootComplete)),
			luabind::value("LootItem", static_cast<int>(OP_LootItem)),
			luabind::value("MoveItem", static_cast<int>(OP_MoveItem)),
			luabind::value("WhoAllRequest", static_cast<int>(OP_WhoAllRequest)),
			luabind::value("WhoAllResponse", static_cast<int>(OP_WhoAllResponse)),
			luabind::value("Consume", static_cast<int>(OP_Consume)),
			luabind::value("AutoAttack", static_cast<int>(OP_AutoAttack)),
			luabind::value("AutoAttack2", static_cast<int>(OP_AutoAttack2)),
			luabind::value("TargetMouse", static_cast<int>(OP_TargetMouse)),
			luabind::value("TargetCommand", static_cast<int>(OP_TargetCommand)),
			luabind::value("Hide", static_cast<int>(OP_Hide)),
			luabind::value("Forage", static_cast<int>(OP_Forage)),
			luabind::value("Fishing", static_cast<int>(OP_Fishing)),
			luabind::value("Bug", static_cast<int>(OP_Bug)),
			luabind::value("Emote", static_cast<int>(OP_Emote)),
			luabind::value("Consider", static_cast<int>(OP_Consider)),
			luabind::value("FaceChange", static_cast<int>(OP_FaceChange)),
			luabind::value("RandomReq", static_cast<int>(OP_RandomReq)),
			luabind::value("RandomReply", static_cast<int>(OP_RandomReply)),
			luabind::value("Camp", static_cast<int>(OP_Camp)),
			luabind::value("YellForHelp", static_cast<int>(OP_YellForHelp)),
			luabind::value("SafePoint", static_cast<int>(OP_SafePoint)),
			luabind::value("Buff", static_cast<int>(OP_Buff)),
			luabind::value("SpecialMesg", static_cast<int>(OP_SpecialMesg)),
			luabind::value("Consent", static_cast<int>(OP_Consent)),
			luabind::value("ConsentResponse", static_cast<int>(OP_ConsentResponse)),
			luabind::value("Stun", static_cast<int>(OP_Stun)),
			luabind::value("BeginCast", static_cast<int>(OP_BeginCast)),
			luabind::value("CastSpell", static_cast<int>(OP_CastSpell)),
			luabind::value("InterruptCast", static_cast<int>(OP_InterruptCast)),
			luabind::value("Death", static_cast<int>(OP_Death)),
			luabind::value("FeignDeath", static_cast<int>(OP_FeignDeath)),
			luabind::value("Illusion", static_cast<int>(OP_Illusion)),
			luabind::value("LevelUpdate", static_cast<int>(OP_LevelUpdate)),
			luabind::value("MemorizeSpell", static_cast<int>(OP_MemorizeSpell)),
			luabind::value("HPUpdate", static_cast<int>(OP_HPUpdate)),
			luabind::value("Mend", static_cast<int>(OP_Mend)),
			luabind::value("Taunt", static_cast<int>(OP_Taunt)),
			luabind::value("GMDelCorpse", static_cast<int>(OP_GMDelCorpse)),
			luabind::value("GMFind", static_cast<int>(OP_GMFind)),
			luabind::value("GMServers", static_cast<int>(OP_GMServers)),
			luabind::value("GMGoto", static_cast<int>(OP_GMGoto)),
			luabind::value("GMSummon", static_cast<int>(OP_GMSummon)),
			luabind::value("GMKill", static_cast<int>(OP_GMKill)),
			luabind::value("GMLastName", static_cast<int>(OP_GMLastName)),
			luabind::value("GMToggle", static_cast<int>(OP_GMToggle)),
			luabind::value("GMEmoteZone", static_cast<int>(OP_GMEmoteZone)),
			luabind::value("GMBecomeNPC", static_cast<int>(OP_GMBecomeNPC)),
			luabind::value("GMHideMe", static_cast<int>(OP_GMHideMe)),
			luabind::value("GMZoneRequest", static_cast<int>(OP_GMZoneRequest)),
			luabind::value("GMZoneRequest2", static_cast<int>(OP_GMZoneRequest2)),
			luabind::value("Petition", static_cast<int>(OP_Petition)),
			luabind::value("PetitionRefresh", static_cast<int>(OP_PetitionRefresh)),
			luabind::value("PetitionCheckout", static_cast<int>(OP_PetitionCheckout)),
			luabind::value("PetitionDelete", static_cast<int>(OP_PetitionDelete)),
			luabind::value("PetitionCheckIn", static_cast<int>(OP_PetitionCheckIn)),
			luabind::value("SetServerFilter", static_cast<int>(OP_SetServerFilter)),
			luabind::value("NewSpawn", static_cast<int>(OP_NewSpawn)),
			luabind::value("Animation", static_cast<int>(OP_Animation)),
			luabind::value("ZoneChange", static_cast<int>(OP_ZoneChange)),
			luabind::value("DeleteSpawn", static_cast<int>(OP_DeleteSpawn)),
			luabind::value("EnvDamage", static_cast<int>(OP_EnvDamage)),
			luabind::value("Action", static_cast<int>(OP_Action)),
			luabind::value("Damage", static_cast<int>(OP_Damage)),
			luabind::value("ManaChange", static_cast<int>(OP_ManaChange)),
			luabind::value("ClientError", static_cast<int>(OP_ClientError)),
			luabind::value("Save", static_cast<int>(OP_Save)),
			luabind::value("LocInfo", static_cast<int>(OP_LocInfo)),
			luabind::value("Surname", static_cast<int>(OP_Surname)),
			luabind::value("SwapSpell", static_cast<int>(OP_SwapSpell)),
			luabind::value("DeleteSpell", static_cast<int>(OP_DeleteSpell)),
			luabind::value("ClickObjectAction", static_cast<int>(OP_ClickObjectAction)),
			luabind::value("GroundSpawn", static_cast<int>(OP_GroundSpawn)),
			luabind::value("ClearObject", static_cast<int>(OP_ClearObject)),
			luabind::value("ZoneUnavail", static_cast<int>(OP_ZoneUnavail)),
			luabind::value("ItemPacket", static_cast<int>(OP_ItemPacket)),
			luabind::value("TradeRequest", static_cast<int>(OP_TradeRequest)),
			luabind::value("TradeRequestAck", static_cast<int>(OP_TradeRequestAck)),
			luabind::value("TradeAcceptClick", static_cast<int>(OP_TradeAcceptClick)),
			luabind::value("TradeMoneyUpdate", static_cast<int>(OP_TradeMoneyUpdate)),
			luabind::value("TradeCoins", static_cast<int>(OP_TradeCoins)),
			luabind::value("CancelTrade", static_cast<int>(OP_CancelTrade)),
			luabind::value("FinishTrade", static_cast<int>(OP_FinishTrade)),
			luabind::value("SaveOnZoneReq", static_cast<int>(OP_SaveOnZoneReq)),
			luabind::value("Logout", static_cast<int>(OP_Logout)),
			luabind::value("LogoutReply", static_cast<int>(OP_LogoutReply)),
			luabind::value("DuelResponse2", static_cast<int>(OP_DuelResponse2)),
			luabind::value("InstillDoubt", static_cast<int>(OP_InstillDoubt)),
			luabind::value("SafeFallSuccess", static_cast<int>(OP_SafeFallSuccess)),
			luabind::value("DisciplineChange", static_cast<int>(OP_DisciplineChange)),
			luabind::value("Shielding", static_cast<int>(OP_Shielding)),
			luabind::value("ZoneEntry", static_cast<int>(OP_ZoneEntry)),
			luabind::value("PlayerProfile", static_cast<int>(OP_PlayerProfile)),
			luabind::value("CharInventory", static_cast<int>(OP_CharInventory)),
			luabind::value("ZoneSpawns", static_cast<int>(OP_ZoneSpawns)),
			luabind::value("Weather", static_cast<int>(OP_Weather)),
			luabind::value("ReqNewZone", static_cast<int>(OP_ReqNewZone)),
			luabind::value("NewZone", static_cast<int>(OP_NewZone)),
			luabind::value("ReqClientSpawn", static_cast<int>(OP_ReqClientSpawn)),
			luabind::value("SpawnAppearance", static_cast<int>(OP_SpawnAppearance)),
			luabind::value("ApproveWorld", static_cast<int>(OP_ApproveWorld)),
			luabind::value("LogServer", static_cast<int>(OP_LogServer)),
			luabind::value("MOTD", static_cast<int>(OP_MOTD)),
			luabind::value("SendLoginInfo", static_cast<int>(OP_SendLoginInfo)),
			luabind::value("DeleteCharacter", static_cast<int>(OP_DeleteCharacter)),
			luabind::value("SendCharInfo", static_cast<int>(OP_SendCharInfo)),
			luabind::value("ExpansionInfo", static_cast<int>(OP_ExpansionInfo)),
			luabind::value("CharacterCreate", static_cast<int>(OP_CharacterCreate)),
			luabind::value("RandomNameGenerator", static_cast<int>(OP_RandomNameGenerator)),
			luabind::value("GuildsList", static_cast<int>(OP_GuildsList)),
			luabind::value("ApproveName", static_cast<int>(OP_ApproveName)),
			luabind::value("EnterWorld", static_cast<int>(OP_EnterWorld)),
			luabind::value("SetChatServer", static_cast<int>(OP_SetChatServer)),
			luabind::value("ZoneServerInfo", static_cast<int>(OP_ZoneServerInfo)),
			luabind::value("WearChange", static_cast<int>(OP_WearChange)),
			luabind::value("LoginComplete", static_cast<int>(OP_LoginComplete)),
			luabind::value("GMNameChange", static_cast<int>(OP_GMNameChange)),
			luabind::value("ReadBook", static_cast<int>(OP_ReadBook)),
			luabind::value("GMKick", static_cast<int>(OP_GMKick)),
			luabind::value("RezzRequest", static_cast<int>(OP_RezzRequest)),
			luabind::value("MultiLineMsg", static_cast<int>(OP_MultiLineMsg)),
			luabind::value("TimeOfDay", static_cast<int>(OP_TimeOfDay)),
			luabind::value("MoneyUpdate", static_cast<int>(OP_MoneyUpdate)),
			luabind::value("ClickObject", static_cast<int>(OP_ClickObject)),
			luabind::value("MoveDoor", static_cast<int>(OP_MoveDoor)),
			luabind::value("DuelResponse", static_cast<int>(OP_DuelResponse)),
			luabind::value("RequestDuel", static_cast<int>(OP_RequestDuel)),
			luabind::value("ClickDoor", static_cast<int>(OP_ClickDoor)),
			luabind::value("ShopEndConfirm", static_cast<int>(OP_ShopEndConfirm)),
			luabind::value("Reward", static_cast<int>(OP_Reward)),
			luabind::value("AAExpUpdate", static_cast<int>(OP_AAExpUpdate)),
			luabind::value("RequestClientZoneChange", static_cast<int>(OP_RequestClientZoneChange)),
			luabind::value("SomeItemPacketMaybe", static_cast<int>(OP_Projectile)),
			luabind::value("BazaarSearch", static_cast<int>(OP_BazaarSearch)),
			luabind::value("SetTitle", static_cast<int>(OP_SetTitle)),
			luabind::value("DeletePetition", static_cast<int>(OP_DeletePetition)),
			luabind::value("Disarm", static_cast<int>(OP_Disarm)),
			luabind::value("FriendsWho", static_cast<int>(OP_FriendsWho)),
			luabind::value("GMSearchCorpse", static_cast<int>(OP_GMSearchCorpse)),
			luabind::value("MobHealth", static_cast<int>(OP_MobHealth)),
			luabind::value("RaidInvite", static_cast<int>(OP_RaidInvite)),
			luabind::value("Report", static_cast<int>(OP_Report)),
			luabind::value("SenseHeading", static_cast<int>(OP_SenseHeading)),
			luabind::value("ZoneInAvatarSet", static_cast<int>(OP_ZoneInAvatarSet)),
			luabind::value("WorldLogout", static_cast<int>(OP_WorldLogout)),
			luabind::value("SessionReady", static_cast<int>(OP_SessionReady)),
			luabind::value("Login", static_cast<int>(OP_LoginPC)),
			luabind::value("ServerListRequest", static_cast<int>(OP_ServerListRequest)),
			luabind::value("PlayEverquestRequest", static_cast<int>(OP_PlayEverquestRequest)),
			luabind::value("ChatMessage", static_cast<int>(OP_ChatMessage)),
			luabind::value("LoginAccepted", static_cast<int>(OP_LoginAccepted)),
			luabind::value("ServerListResponse", static_cast<int>(OP_ServerListResponse)),
			luabind::value("Poll", static_cast<int>(OP_Poll)),
			luabind::value("PlayEverquestResponse", static_cast<int>(OP_PlayEverquestResponse)),
			luabind::value("EnterChat", static_cast<int>(OP_EnterChat)),
			luabind::value("PollResponse", static_cast<int>(OP_PollResponse)),
			luabind::value("RaidJoin", static_cast<int>(OP_RaidJoin)),
			luabind::value("Translocate", static_cast<int>(OP_Translocate)),
			luabind::value("Sacrifice", static_cast<int>(OP_Sacrifice)),
			luabind::value("DeleteCharge", static_cast<int>(OP_DeleteCharge)),
			luabind::value("ApplyPoison", static_cast<int>(OP_ApplyPoison)),
			luabind::value("LoginUnknown1", static_cast<int>(OP_LoginUnknown1)),
			luabind::value("LoginUnknown2", static_cast<int>(OP_LoginUnknown2)),
			luabind::value("ManaUpdate", static_cast<int>(OP_ManaUpdate)),
			luabind::value("CorpseDrag", static_cast<int>(OP_CorpseDrag)),
			luabind::value("DespawnDoor", static_cast<int>(OP_DespawnDoor))
		];
}

#endif
