#ifndef EQ_SOPCODES_H
#define EQ_SOPCODES_H

#include "../common/types.h"
#include "../common/packet_functions.h"
#include "../common/eq_packet_structs.h"

#define SERVER_TIMEOUT	30000	// how often keepalive gets sent
#define INTERSERVER_TIMER					10000
#define LoginServer_StatusUpdateInterval	15000
#define LoginServer_AuthStale				60000
#define AUTHCHANGE_TIMEOUT					900	// in seconds

#define ServerOP_KeepAlive			0x0001	// packet to test if port is still open
#define ServerOP_ChannelMessage		0x0002	// broadcast/guildsay
#define ServerOP_SetZone			0x0003	// client -> server zoneinfo
#define ServerOP_ShutdownAll		0x0004	// exit(0);
#define ServerOP_ZoneShutdown		0x0005	// unload all data, goto sleep mode
#define ServerOP_ZoneBootup			0x0006	// come out of sleep mode and load zone specified
#define ServerOP_ZoneStatus			0x0007	// Shows status of all zones
#define ServerOP_SetConnectInfo		0x0008	// Tells server address and port #
#define ServerOP_EmoteMessage		0x0009	// Worldfarts
#define ServerOP_ClientList			0x000A	// Update worldserver's client list, for #whos
#define ServerOP_Who				0x000B	// #who
#define ServerOP_ZonePlayer			0x000C	// #zone, or #summon
#define ServerOP_KickPlayer			0x000D	// #kick

#define ServerOP_RefreshGuild		0x000E	// Notice to all zoneservers to refresh their guild cache for ID# in packet (ServerGuildRefresh_Struct)
//#define ServerOP_GuildInvite		0x0010
#define ServerOP_DeleteGuild		0x0011	// ServerGuildID_Struct
#define ServerOP_GuildRankUpdate	0x0012
#define ServerOP_GuildCharRefresh	0x0013
#define ServerOP_GuildMemberUpdate	0x0014
#define ServerOP_RequestOnlineGuildMembers	0x0015
#define ServerOP_OnlineGuildMembersResponse	0x0016

#define ServerOP_FlagUpdate			0x0018	// GM Flag updated for character, refresh the memory cache
#define ServerOP_GMGoto				0x0019
#define ServerOP_MultiLineMsg		0x001A
#define ServerOP_Lock				0x001B	// For #lock/#unlock inside server
#define ServerOP_Motd				0x001C	// For changing MoTD inside server.
#define ServerOP_Uptime				0x001D
#define ServerOP_Petition			0x001E
#define	ServerOP_KillPlayer			0x001F
#define ServerOP_UpdateGM			0x0020
#define ServerOP_RezzPlayer			0x0021
#define ServerOP_ZoneReboot			0x0022
#define ServerOP_ZoneToZoneRequest	0x0023
#define ServerOP_AcceptWorldEntrance 0x0024
#define ServerOP_ZAAuth				0x0025
#define ServerOP_ZAAuthFailed		0x0026
#define ServerOP_ZoneIncClient		0x0027	// Incoming client
#define ServerOP_ClientListKA		0x0028
#define ServerOP_ChangeWID			0x0029
#define ServerOP_IPLookup			0x002A
#define ServerOP_LockZone			0x002B
#define ServerOP_ItemStatus			0x002C
#define ServerOP_OOCMute			0x002D
#define ServerOP_Revoke				0x002E
//#define			0x002F
#define ServerOP_GroupIDReq			0x0030
#define ServerOP_GroupIDReply		0x0031
#define ServerOP_GroupLeave			0x0032	// for disbanding out of zone folks
#define ServerOP_RezzPlayerAccept	0x0033
#define ServerOP_SpawnCondition		0x0034
#define ServerOP_SpawnEvent			0x0035
#define ServerOP_SetLaunchName		0x0036
#define ServerOP_RezzPlayerReject	0x0037
#define ServerOP_SpawnPlayerCorpse	0x0038
#define ServerOP_Consent			0x0039
#define ServerOP_Consent_Response	0x003a
#define ServerOP_ForceGroupUpdate	0x003b
#define ServerOP_OOZGroupMessage	0x003c
#define ServerOP_DisbandGroup		0x003d //for disbanding a whole group cross zone
#define ServerOP_GroupJoin			0x003e //for joining ooz folks
#define ServerOP_UpdateSpawn		0x003f
#define ServerOP_SpawnStatusChange	0x0040
#define ServerOP_ChangeGroupLeader	0x0041
#define ServerOP_IsOwnerOnline		0x0042
#define ServerOP_CheckGroupLeader	0x0043
#define ServerOP_RaidGroupJoin		0x0044

#define ServerOP_DepopAllPlayersCorpses	0x0061

#define ServerOP_QGlobalUpdate		0x0063
#define ServerOP_QGlobalDelete		0x0064
#define ServerOP_DepopPlayerCorpse	0x0065
#define ServerOP_RequestTellQueue	0x0066 // client asks for it's tell queues
#define ServerOP_ChangeSharedMem	0x0067
#define ServerOP_ConsentDeny		0x0068
#define ServerOP_ConsentDenyByID	0x0069

#define ServerOP_RaidAdd			0x0100 //in use
#define ServerOP_RaidRemove			0x0101 //in use
#define	ServerOP_RaidDisband		0x0102 //in use
#define ServerOP_RaidGroupLeader	0x0103 //in use
#define ServerOP_RaidLeader			0x0104 //in use
#define	ServerOP_RaidGroupSay		0x0105 //in use
#define	ServerOP_RaidSay			0x0106 //in use
#define	ServerOP_DetailsChange		0x0107 //in use
#define ServerOP_RaidRemoveLD		0x0108 //in use

#define ServerOP_UpdateGroup		0x010A //in use
#define ServerOP_RaidGroupDisband	0x010B //in use
#define ServerOP_RaidChangeGroup	0x010C //in use
#define ServerOP_RaidGroupAdd		0x010D
#define ServerOP_RaidGroupRemove	0x010E
#define ServerOP_GroupInvite		0x010F
#define ServerOP_GroupFollow		0x0110
#define ServerOP_GroupFollowAck		0x0111
#define ServerOP_GroupCancelInvite	0x0112
#define ServerOP_GroupSetID			0x0113
#define ServerOP_RaidTypeChange		0x0114
#define ServerOP_RaidAddLooter		0x0115
#define ServerOP_RemoveRaidLooter	0x0116

#define ServerOP_WhoAll				0x0210
#define ServerOP_FriendsWho			0x0211
#define ServerOP_LFGMatches			0x0212
#define ServerOP_ClientVersionSummary 0x0215
#define ServerOP_Soulmark			0x0216
#define ServerOP_AddSoulmark		0x0217

#define ServerOP_Weather			0x0219
#define ServerOP_LSInfo				0x1000
#define ServerOP_LSStatus			0x1001
#define ServerOP_LSClientAuth		0x1002
#define ServerOP_LSFatalError		0x1003
#define ServerOP_SystemwideMessage	0x1005
#define ServerOP_ListWorlds			0x1006
#define ServerOP_PeerConnect		0x1007
#define ServerOP_NewLSInfo			0x1008
#define ServerOP_LSRemoteAddr		0x1009
#define ServerOP_LSAccountUpdate		0x100A

#define ServerOP_EncapPacket		0x2007	// Packet within a packet
#define ServerOP_WorldListUpdate	0x2008
#define ServerOP_WorldListRemove	0x2009
#define ServerOP_TriggerWorldListRefresh	0x200A
#define ServerOP_WhoAllReply		0x2010
#define ServerOP_SetWorldTime		0x200B
#define ServerOP_GetWorldTime		0x200C
#define ServerOP_SyncWorldTime		0x200E

#define ServerOP_LSZoneInfo			0x3001
#define ServerOP_LSZoneStart		0x3002
#define ServerOP_LSZoneBoot			0x3003
#define ServerOP_LSZoneShutdown		0x3004
#define ServerOP_LSZoneSleep		0x3005
#define ServerOP_LSPlayerLeftWorld	0x3006
#define ServerOP_LSPlayerJoinWorld	0x3007
#define ServerOP_LSPlayerZoneChange	0x3008

#define	ServerOP_UsertoWorldReq		0xAB00
#define	ServerOP_UsertoWorldResp	0xAB01

#define ServerOP_LauncherConnectInfo	0x3000
#define ServerOP_LauncherZoneRequest	0x3001
#define ServerOP_LauncherZoneStatus		0x3002
#define ServerOP_DoZoneCommand		0x3003
#define ServerOP_BootDownZones		0x3004

#define ServerOP_UCSMessage		0x4000
#define ServerOP_UCSMailMessage 0x4001

#define ServerOP_QueryServGeneric	0x4005
#define ServerOP_CZSignalClient 0x4006
#define ServerOP_CZSignalClientByName 0x4007
#define ServerOP_CZMessagePlayer 0x4008
#define ServerOP_UpdateSchedulerEvents 0x4012

#define ServerOP_ReloadContentFlags 0x4013
#define ServerOP_ReloadSkills		0x0218
#define ServerOP_ReloadWorld 0x4009
#define ServerOP_ReloadLogs 0x4010
#define ServerOP_QuakeImminent 0x4011
#define ServerOP_UpdateSchedulerEvents 0x4012
#define ServerOP_ReloadContentFlags 0x4013
#define ServerOP_QuakeRequest 0x4014
#define ServerOP_QuakeEnded 0x4015
#define ServerOP_ReloadRules	0x4002
#define ServerOP_ReloadRulesWorld	0x4003
#define ServerOP_ReloadTitles		0x0062
#define ServerOP_ReloadSpellModifiers	0x4016

/* Query Server OP Codes */
#define ServerOP_QSPlayerLogItemDeletes				0x5013
#define ServerOP_QSPlayerLogItemMoves				0x5014
#define ServerOP_QSPlayerLogMerchantTransactions	0x5015
#define ServerOP_QSSendQuery						0x5016
#define ServerOP_CZSignalNPC						0x5017
#define ServerOP_CZSetEntityVariableByNPCTypeID		0x5018
#define ServerOP_QSPlayerAARateHourly				0x5019
#define ServerOP_QSPlayerAAPurchase					0x5020
#define ServerOP_QSPlayerTSEvents					0x5022
#define ServerOP_QSPlayerQGlobalUpdates				0x5023
#define ServerOP_QSPlayerLootRecords				0x5024

#define ServerOP_WIRemoteCall 0x5001
#define ServerOP_WIRemoteCallResponse 0x5002
#define ServerOP_WIRemoteCallToClient 0x5003
#define ServerOP_WIClientSession 0x5004
#define ServerOP_WIClientSessionResponse 0x5005
/* Query Serv Generic Packet Flag/Type Enumeration */

#define ServerOP_Speech			0x4513

enum {
	UserToWorldStatusWorldUnavail    = 0,
	UserToWorldStatusSuccess         = 1,
	UserToWorldStatusSuspended       = -1,
	UserToWorldStatusBanned          = -2,
	UserToWorldStatusWorldAtCapacity = -3,
	UserToWorldStatusAlreadyOnline   = -4,
	UserToWorldStatusIPLimitExceeded = -5
};

/************ PACKET RELATED STRUCT ************/
class ServerPacket
{
public:
	~ServerPacket() { safe_delete_array(pBuffer); }
	ServerPacket(uint16 in_opcode = 0, uint32 in_size = 0) {
		this->compressed = false;
		size = in_size;
		opcode = in_opcode;
		if (size == 0) {
			pBuffer = 0;
		}
		else {
			pBuffer = new uchar[size];
			memset(pBuffer, 0, size);
		}
		_wpos = 0;
		_rpos = 0;
	}
	ServerPacket* Copy() {
		ServerPacket* ret = new ServerPacket(this->opcode, this->size);
		if (this->size)
			memcpy(ret->pBuffer, this->pBuffer, this->size);
		ret->compressed = this->compressed;
		ret->InflatedSize = this->InflatedSize;
		return ret;
	}
	bool Deflate() {
		if (compressed)
			return false;
		if ((!this->pBuffer) || (!this->size))
			return false;
		uchar* tmp = new uchar[this->size + 128];
		uint32 tmpsize = DeflatePacket(this->pBuffer, this->size, tmp, this->size + 128);
		if (!tmpsize) {
			safe_delete_array(tmp);
			return false;
		}
		this->compressed = true;
		this->InflatedSize = this->size;
		this->size = tmpsize;
		uchar* tmpdel = this->pBuffer;
		this->pBuffer = tmp;
		safe_delete_array(tmpdel);
		return true;
	}
	bool Inflate() {
		if (!compressed)
			return false;
		if ((!this->pBuffer) || (!this->size))
			return false;
		uchar* tmp = new uchar[InflatedSize];
		uint32 tmpsize = InflatePacket(this->pBuffer, this->size, tmp, InflatedSize);
		if (!tmpsize) {
			safe_delete_array(tmp);
			return false;
		}
		compressed = false;
		this->size = tmpsize;
		uchar* tmpdel = this->pBuffer;
		this->pBuffer = tmp;
		safe_delete_array(tmpdel);
		return true;
	}

	void WriteUInt8(uint8 value) { *(uint8 *)(pBuffer + _wpos) = value; _wpos += sizeof(uint8); }
	void WriteUInt32(uint32 value) { *(uint32 *)(pBuffer + _wpos) = value; _wpos += sizeof(uint32); }
	void WriteString(const char * str) { uint32 len = static_cast<uint32>(strlen(str)) + 1; memcpy(pBuffer + _wpos, str, len); _wpos += len; }

	uint8 ReadUInt8() { uint8 value = *(uint8 *)(pBuffer + _rpos); _rpos += sizeof(uint8); return value; }
	uint32 ReadUInt32() { uint32 value = *(uint32 *)(pBuffer + _rpos); _rpos += sizeof(uint32); return value; }
	void ReadString(char *str) { uint32 len = static_cast<uint32>(strlen((char *)(pBuffer + _rpos))) + 1; memcpy(str, pBuffer + _rpos, len); _rpos += len; }

	uint32 GetWritePosition() { return _wpos; }
	uint32 GetReadPosition() { return _rpos; }
	void SetWritePosition(uint32 Newwpos) { _wpos = Newwpos; }
	void WriteSkipBytes(uint32 count) { _wpos += count; }
	void ReadSkipBytes(uint32 count) { _rpos += count; }
	void SetReadPosition(uint32 Newrpos) { _rpos = Newrpos; }

	uint32	size;
	uint16	opcode;
	uchar*	pBuffer;
	uint32	_wpos;
	uint32	_rpos;
	bool	compressed;
	uint32	InflatedSize;
	uint32	destination;
};

#pragma pack(1)

struct SPackSendQueue {
	uint16 size;
	uchar buffer[0];
};

struct ServerZoneStateChange_struct {
	uint32 ZoneServerID;
	char adminname[64];
	uint32 zoneid;
	bool makestatic;
	uint32 ZoneServerGuildID;
};

struct ServerDownZoneBoot_struct {
	char adminname[64];
};

struct ServerZoneIncomingClient_Struct {
	uint32	zoneid;		// in case the zone shut down, boot it back up
	uint32	zoneguildid;		// in case the zone shut down, boot it back up
	uint32	ip;			// client's IP address
	uint32	wid;		// client's WorldID#
	uint32	accid;
	int16	admin;
	uint32	charid;
	bool	tellsoff;
	char	charname[64];
	char	lskey[30];
	uint32	version;
};

struct ServerChangeWID_Struct {
	uint32	charid;
	uint32	newwid;
};

struct SendGroup_Struct{
	uint8	grouptotal;
	uint32	zoneid;
	char	leader[64];
	char	thismember[64];
	char	members[5][64];
};

struct ServerGroupFollow_Struct {
	uint32 CharacterID;
	GroupGeneric_Struct gf;
};

struct ServerGroupFollowAck_Struct {
	char Name[64];
	uint32 charid;
	uint32 groupid;
};

struct ServerChannelMessage_Struct {
	char deliverto[64];
	char to[64];
	char from[64];
	uint8 fromadmin;
	uint16 chan_num;
	uint32 guilddbid;
	uint8 language;
	uint8 lang_skill;
	uint8 queued; // 0 = not queued, 1 = queued, 2 = queue full, 3 = offline
	char message[0];
};

struct ServerEmoteMessage_Struct {
	char	to[64];
	uint32	guilddbid;
	int16	minstatus;
	uint32	type;
	char	message[0];
};

struct ServerClientList_Struct {
	uint8	remove;
	uint32	wid;
	uint32	IP;
	uint32	zone;
	uint32	zoneguildid;
	int16	Admin;
	uint32	charid;
	char	name[64];
	uint32	AccountID;
	char	AccountName[30];
	uint32	LSAccountID;
	char	lskey[30];
	uint16	race;
	uint8	class_;
	uint8	level;
	uint8	anon;
	bool	tellsoff;
	uint32	guild_id;
	bool	LFG;
	uint8	gm;
	uint8	ClientVersion;
	uint8	LFGFromLevel;
	uint8	LFGToLevel;
	bool	LFGMatchFilter;
	char	LFGComments[64];
	bool	LD;
	uint16  baserace;
	bool	mule;
	bool	AFK;
	bool	Trader;
	int8	Revoked;
};

struct ServerClientListKeepAlive_Struct {
	uint32	numupdates;
	uint32	wid[0];
};

struct ServerZonePlayer_Struct {
	char	adminname[64];
	int16	adminrank;
	uint8	ignorerestrictions;
	char	name[64];
	char	zone[25];
	uint32	zoneguildid;
	float	x_pos;
	float	y_pos;
	float	z_pos;
};

struct RezzPlayer_Struct {
	uint32	dbid;
	uint32	exp;
	uint16	rezzopcode;
	uint32	corpse_zone_id;
	uint32	corpse_zone_guild_id;
	uint32	corpse_character_id;
	//char	packet[160];
	Resurrect_Struct rez;
};

struct ServerZoneReboot_Struct {
//	char	ip1[250];
	char	ip2[250];
	uint16	port;
	uint32	zoneid;
};

struct SetZone_Struct {
	uint32	zoneid;
	uint32	zoneguildid;
	bool	staticzone;
};

struct ServerKickPlayer_Struct {
	char adminname[64];
	int16 adminrank;
	char name[64];
	uint32 AccountID;
};

struct ServerLSInfo_Struct {
	char	name[201];				// name the worldserver wants
	char	address[250];			// DNS address of the server
	char	account[31];			// account name for the worldserver
	char	password[31];			// password for the name
	char	protocolversion[25];	// Major protocol version number
	char	serverversion[64];		// minor server software version number
	uint8	servertype;				// 0=world, 1=chat, 2=login, 3=MeshLogin
};

struct ServerNewLSInfo_Struct {
	char	name[201];	// name the worldserver wants
	char	shortname[50];	// shortname the worldserver wants
	char	remote_address[125];	// DNS address of the server
	char	local_address[125];	// DNS address of the server
	char	account[31];	// account name for the worldserver
	char	password[31];	// password for the name
	char	protocolversion[25];	// Major protocol version number
	char	serverversion[64];	// minor server software version number
	uint8	servertype;	// 0=world, 1=chat, 2=login, 3=MeshLogin
};

struct ServerLSAccountUpdate_Struct {			// for updating info on login server
	char	worldaccount[31];			// account name for the worldserver
	char	worldpassword[31];			// password for the name
	uint32	useraccountid;				// player account ID
	char	useraccount[31];			// player account name
	char	userpassword[51];			// player account password
	char	useremail[101];				// player account email address
};

struct ServerLSStatus_Struct {
	int32 status;
	int32 num_players;
	int32 num_zones;
};

struct ZoneInfo_Struct {
	uint32 zone;
	uint16 count;
	uint32 zone_wid;
};

struct ZoneBoot_Struct {
	uint32 zone;
	char compile_time[25];
	uint32 zone_wid;
};

struct ZoneShutdown_Struct {
	uint32 zone;
	uint32 zone_wid;
};

struct ServerLSZoneSleep_Struct {
	uint32 zone;
	uint32 zone_wid;
};

struct ServerLSPlayerJoinWorld_Struct {
	uint32 lsaccount_id;
	char key[30];
};

struct ServerLSPlayerLeftWorld_Struct {
	uint32 lsaccount_id;
	char key[30];
};

struct ServerLSPlayerZoneChange_Struct {
	uint32 lsaccount_id;
	uint32 from; // 0 = world
	uint32 to; // 0 = world
};

struct ServerLSClientAuth {
	uint32	lsaccount_id;	// ID# in login server's db
	char	name[30];		// username in login server's db
	char	key[30];		// the Key the client will present
	uint8	lsadmin;		// login server admin level
	int16	worldadmin;		// login's suggested worldadmin level setting for this user, up to the world if they want to obey it
	uint32	ip;
	uint8	local;			// 1 if the client is from the local network
	uint8	version;		// Client version if Mac
	char	forum_name[31];
};

struct ServerSystemwideMessage {
	uint32	lsaccount_id;
	char	key[30];		// sessionID key for verification
	uint32	type;
	char	message[0];
};

struct ServerLSPeerConnect {
	uint32	ip;
	uint16	port;
};

struct ServerConnectInfo {
	char	address[250];
	char	local_address[250];
	uint16	port;
	uint32  process_id;
};

struct ServerGMGoto_Struct {
	char	myname[64];
	char	gotoname[64];
	int16	admin;
	uint32	guildinstanceid;
};

struct ServerMultiLineMsg_Struct {
	char	to[64];
	char	message[0];
};

struct ServerLock_Struct {
	char character_name[64];
	bool is_locked;
};

struct ServerMotd_Struct {
	char	myname[64]; // User that set the motd
	char	motd[512]; // the new MoTD
};

struct ServerUptime_Struct {
	uint32	zoneserverid;	// 0 for world
	char	adminname[64];
};

struct ServerPetitionUpdate_Struct {
	uint32 petid; // Petition Number
	uint8 status; // 0x00 = ReRead DB -- 0x01 = Checkout -- More? Dunno... lol
};

typedef enum { WH_Petition, WH_Command, WH_Chat, WH_Notices, WH_Spawns } WHTypes;

struct WHMessage {
	int HookType;
	bool Server;
	char Message[2000];
};

struct ServerWhoAll_Struct {
	int16 admin;
	uint16 fromid;
	char from[64];
	char whom[64];
	int16 wrace; // FF FF = no race
	int16 wclass; // FF FF = no class
	int16 lvllow; // FF FF = no numbers
	int16 lvlhigh; // FF FF = no numbers
	int16 gmlookup; // FF FF = not doing /who all gm
	int16 guildid; // FF FF = not doing /who all guild FD FF = lfg
};

struct ServerFriendsWho_Struct {
	uint32 FromID;
	char FromName[64];
	char FriendsString[1];
};

struct ServerKillPlayer_Struct {
	char gmname[64];
	char target[64];
	int16 admin;
};

struct ServerUpdateGM_Struct {
	char gmname[64];
	bool gmstatus;
};

struct ServerEncapPacket_Struct {
	uint32	ToID;	// ID number of the LWorld on the other server
	uint16	opcode;
	uint16	size;
	uchar	data[0];
};

struct ZoneToZone_Struct {
	char	name[64];
	uint32	guild_id;
	uint32	requested_zone_id;
	uint32	requested_zone_guild_id;
	uint32	current_zone_id;
	uint32	current_zone_guild_id;
	int8	response;
	int16	admin;
	uint8	ignorerestrictions;
};

struct WorldToZone_Struct {
	uint32	account_id;
	int8	response;
};

struct WorldShutDown_Struct {
	uint32	time;
	uint32	interval;
};

struct ServerSyncWorldList_Struct {
	uint32	RemoteID;
	uint32	ip;
	int32	status;
	char	name[201];
	char	address[250];
	char	account[31];
	uint32	accountid;
	uint8	authlevel;
	uint8	servertype;		// 0=world, 1=chat, 2=login
	uint32	adminid;
	uint8	greenname;
	uint8	showdown;
	int32	num_players;
	int32	num_zones;
	bool	placeholder;
};

struct UsertoWorldRequest_Struct {
	uint32	lsaccountid;
	uint32	worldid;
	uint32  ip;
	uint32	FromID;
	uint32	ToID;
	char	forum_name[31];
};

struct UsertoWorldResponse_Struct {
	uint32	lsaccountid;
	uint32	worldid;
	int8	response; // -3) World Full, -2) Banned, -1) Suspended, 0) Denied, 1) Allowed
	uint32	FromID;
	uint32	ToID;
};

// generic struct to be used for alot of simple zone->world questions
struct ServerGenericWorldQuery_Struct {
	char	from[64];	// charname the query is from
	int16	admin;		// char's admin level
	char	query[0];	// text of the query
};

struct ServerLockZone_Struct {
	uint8	op;
	char	adminname[64];
	uint16	zoneID;
};

struct RevokeStruct {
	char adminname[64];
	char name[64];
	int8 toggle; //0 off, 1 on except guild/group/raid, 2 also revoke guild/group/raid
};

struct ServerGroupIDReply_Struct {
	uint32 start;	//a range of group IDs to use.
	uint32 end;
};

struct ServerGroupLeave_Struct {
	uint32 zoneid;
	uint32 zoneguildid;
	uint32 gid;
	char member_name[64];	//kick this member from the group
	bool	checkleader;
};

struct ServerGroupJoin_Struct {
	uint32 zoneid;
	uint32 zoneguildid;
	uint32 gid;
	char member_name[64];	//this person is joining the group
};

struct ServerRaidGroupJoin_Struct {
	uint32 zoneid;
	uint32 zoneguildid;
	uint32 gid;
	uint32 rid;
	char member_name[64];	//this person is joining the group
};

struct ServerGroupLeader_Struct {
	uint32 zoneid;
	uint32 zoneguildid;
	uint32 gid;
	char leader_name[64];
	char oldleader_name[64];
	bool leaderset;
};

struct ServerForceGroupUpdate_Struct {
	uint32 origZoneID;
	uint32 origZoneGuildID;
	uint32 gid;
};

struct ServerGroupChannelMessage_Struct {
	uint32 zoneid;
	uint32 zoneguildid;
	uint32 groupid;
	char from[64];
	uint8 language;
	uint8 lang_skill;
	char message[0];
};

struct ServerDisbandGroup_Struct {
	uint32 zoneid;
	uint32 zoneguildid;
	uint32 groupid;
};

struct SimpleName_Struct{
	char name[64];
};

struct ServerSpawnCondition_Struct {
	uint32 zoneID;
	uint16 condition_id;
	int16 value;
};

struct ServerSpawnEvent_Struct {
	uint32	zoneID;
	uint32	event_id;
};

//zone->world
struct LaunchName_Struct {
	char launcher_name[32];
	char zone_name[16];
};

struct LauncherConnectInfo {
	char name[64];
};

typedef enum {
	ZR_Start,
	ZR_Restart,
	ZR_Stop
} ZoneRequestCommands;

struct LauncherZoneRequest {
	uint8 command;
	char short_name[33];
	uint16 port;
};

struct LauncherZoneStatus {
	char short_name[33];
	uint32 start_count;
	uint8 running;
};

struct ServerGuildID_Struct {
	uint32 guild_id;
};

struct ServerGuildRefresh_Struct {
	uint32 guild_id;
	uint8 name_change;
	uint8 motd_change;
	uint8 rank_change;
	uint8 relation_change;
};

struct ServerGuildCharRefresh_Struct {
	uint32 guild_id;
	uint32 old_guild_id;
	uint32 char_id;
};

struct ServerGuildRankUpdate_Struct
{
	uint32 GuildID;
	char MemberName[64];
	uint32 Rank;
	uint32 Banker;
};

struct ServerGuildMemberUpdate_Struct {
	uint32 GuildID;
	char MemberName[64];
	uint32 ZoneID;
	uint32 LastSeen;
};

struct SpawnPlayerCorpse_Struct {
	uint32 player_corpse_id;
	uint32 zone_id;
	uint32 GuildID;
};

struct ServerOP_Consent_Struct {
	char grantname[64];
	char ownername[64];
	uint8 permission;
	uint32 zone_id;
	uint32 GuildID;
	uint32 message_string_id;
	uint32 corpse_id;
};

struct ServerOP_ConsentDeny_Struct {
	char grantname[64];
	char ownername[64];
};

struct ServerOP_ConsentDenyByID_Struct {
	char ownername[64];
	uint32 corpse_id;
};

struct ServerDepopAllPlayersCorpses_Struct
{
	uint32 CharacterID;
	uint32 ZoneID;
	uint32 GuildID;
};

struct ServerDepopPlayerCorpse_Struct
{
	uint32 DBID;
	uint32 ZoneID;
	uint32 GuildID;
};

struct ServerRaidGeneralAction_Struct {
	uint32 zoneid; // also is raid leader bool when sent to zone.
	uint32 zoneguildid; // also is raid leader bool when sent to zone.
	uint32 gleader;
	uint32 rid;
	uint32 gid;
	uint32 looter;
	char playername[64];
};

struct ServerRaidGroupAction_Struct { //add / remove depends on opcode.
	
	uint32 gid; //group id to send to.
	uint32 rid; //raid id to send to.
	char membername[64]; //member who's adding / leaving
};

struct ServerRaidMessage_Struct {
	uint32 rid;
	uint32 gid;
	uint8 language;
	uint8 lang_skill;
	char from[64];
	char message[0];
};

struct UpdateSpawnTimer_Struct {
	uint32 id;
	uint32 duration;
};

struct ServerSpawnStatusChange_Struct
{
	uint32 id;
	bool new_status;
};

struct ServerQGlobalUpdate_Struct
{
	uint32 id;
	char name[64];
	char value[128];
	uint32 npc_id;
	uint32 char_id;
	uint32 zone_id;
	uint32 expdate;
	uint32 from_zone_id;
};

struct ServerQGlobalDelete_Struct
{
	char name[64];
	uint32 npc_id;
	uint32 char_id;
	uint32 zone_id;
	uint32 from_zone_id;
};

struct ServerRequestOnlineGuildMembers_Struct
{
	uint32	FromID;
	uint32	GuildID;
};

struct ServerRequestClientVersionSummary_Struct
{
	char Name[64];
};

struct ServerLeaderboardRequest_Struct
{
	char player[64];
	uint8 type;
};

struct ServerMailMessageHeader_Struct {
	char from[64];
	char to[64];
	char subject[128];
	char message[0];
};

struct Server_Speech_Struct {
	char	to[325]; // store up to 5 names of size 64, and 4 commas + 1 null terminator
	char	from[64];
	uint32	guilddbid;
	uint32	groupid;
	uint32	characterid;
	int16	minstatus;
	uint32	type;
	char	message[0];
};

struct CZClientSignal_Struct {
	int charid;
	uint32 data;
};

struct CZNPCSignal_Struct {
	uint32 npctype_id;
	int num;
	char data[0];
};

struct CZClientSignalByName_Struct {
	char Name[64];
	uint32 data;
};

struct QSPlayerLogTrade_Struct {
	uint32				char1_id;
	MoneyUpdate_Struct	char1_money;
	uint16				char1_count;
	uint32				char2_id;
	MoneyUpdate_Struct	char2_money;
	uint16				char2_count;
};

struct QSPlayerLogHandin_Struct {
	uint32				char_id;
	MoneyUpdate_Struct	char_money;
	uint16				char_count;
	uint32				npc_id;
};

struct QSPlayerLogNPCKillSub_Struct{
	uint32 NPCID;
	uint32 ZoneID;
	uint32 Type;
};

struct QSPlayerLogNPCKillsPlayers_Struct{
	uint32 char_id;
};

struct QSPlayerLogNPCKill_Struct{
	QSPlayerLogNPCKillSub_Struct s1;
	QSPlayerLogNPCKillsPlayers_Struct Chars[0];
};

//struct QSDeleteItems_Struct {
//	uint16 char_slot;
//	uint32 item_id;
//	uint16 charges;
//};

struct QSPlayerLogItemDelete_Struct {
	uint32 char_id;
	uint16 stack_size; // '0' indicates full stack or non-stackable item move
	uint16 char_count;
	uint16 char_slot;
	uint32 item_id;
	uint16 charges;
	//QSDeleteItems_Struct	items[0];
};

struct QSMoveItems_Struct {
	uint16 from_slot;
	uint16 to_slot;
	uint32 item_id;
	uint16 charges;
};

struct QSPlayerLogItemMove_Struct {
	uint32			char_id;
	uint16			from_slot;
	uint16			to_slot;
	uint16			stack_size; // '0' indicates full stack or non-stackable item move
	uint16			char_count;
	bool			postaction;
	QSMoveItems_Struct items[0];
};

//struct QSTransactionItems_Struct {
//	uint16 char_slot;
//	uint32 item_id;
//	uint16 charges;
//};

struct QSMerchantLogTransaction_Struct {
	uint32					char_id;
	uint16					char_slot;
	uint32					item_id;
	uint16					charges;
	uint32					zone_id;
	uint32					merchant_id;
	MoneyUpdate_Struct		merchant_money;
	uint16					merchant_count;
	MoneyUpdate_Struct		char_money;
	uint16					char_count;
	//QSTransactionItems_Struct items[0];
};

struct QSPlayerAARateHourly_Struct {
	uint32 charid;
	uint32 add_points;
};

struct QSPlayerAAPurchase_Struct {
	uint32 charid;
	char aatype[8];
	char aaname[128];
	uint32 aaid;
	uint32 cost;
	uint32 zone_id;
};

struct QSPlayerDeathBy_Struct {
	uint32 charid;
	uint32 zone_id;
	char killed_by[128];
	uint16 spell;
	uint32 damage;
};

struct QSPlayerTSEvents_Struct {
	uint32 charid;
	uint32 zone_id;
	char results[8];
	uint32 recipe_id;
	uint32 tradeskill;
	uint16 trivial;
	float chance;
};

struct QSPlayerQGlobalUpdate_Struct {
	uint32 charid;
	char action[8];
	uint32 zone_id;
	char varname[32];
	char newvalue[32];
};

struct QSPlayerLootRecords_struct {
	uint32 charid;
	char corpse_name[64];
	char type[12];
	uint32 zone_id;
	uint32 item_id;
	char item_name[64];
	int8 charges;
	MoneyUpdate_Struct money;
};

struct QSGeneralQuery_Struct {
	char QueryString[0];
};

struct QSLogCommands_Struct {
	int32 AccID;
	int32 CharID;
	int32 ZoneID;
	int32 TargetID;
	char Target[64];
	char Command[32];
	time_t	TS;
	char Arguments[0];
};

struct CZMessagePlayer_Struct {
	uint32	Type;
	char	CharName[64];
	char	Message[512];
};

struct CZSetEntVarByNPCTypeID_Struct {
	uint32 npctype_id;
	char id[256];
	char m_var[256];
};

struct ReloadWorld_Struct{
	uint32 Option;
};

struct ServerRequestTellQueue_Struct {
	char	name[64];
};

struct ServerRequestSoulMark_Struct {
	char	name[64];
	SoulMarkList_Struct entry;
};

struct ServerIsOwnerOnline_Struct {
	char   name[64];	
	uint32 corpseid;
	uint16 zoneid;
	uint32 zoneguildid;
	uint8  online;
	uint32 accountid;
};

struct ServerWeather_Struct {
	uint32 type;
	uint32 intensity;
	uint16 timer;
};

struct ServerFlagUpdate_Struct {
	uint32 account_id;
	int16 admin;
};

struct ServerGroupInvite_Struct {
		/*000*/	char invitee_name[64];		// Comment: 
		/*064*/	char inviter_name[64];		// Comment: 
		/*128*/	char unknown[65];		// Comment: 
		/*193*/

		// Custom:
		uint8 self_found;
		// Custom:
		uint8 is_null;
};

struct ServerEarthquakeImminent_Struct {
	uint32	start_timestamp; // Time the last quake began, in seconds. UNIX Timestamp. QuakeType enforcement is supposed to cease 84600 seconds following this time. Raid mobs are supposed to respawn 86400 seconds after this time. Actual type will be unknown and stored in memory.
	uint32	next_start_timestamp; // Time the last quake began, in seconds. UNIX Timestamp. QuakeType enforcement is supposed to cease 84600 seconds following this time. Raid mobs are supposed to respawn 86400 seconds after this time. Actual type will be unknown and stored in memory.
	
	QuakeType quake_type; // Player-imposed ruleset with quake. uint8_t enum
};

struct ServerEarthquakeRequest_Struct {
	QuakeType	type; // Time the last quake began, in seconds. UNIX Timestamp. QuakeType enforcement is supposed to cease 84600 seconds following this time. Raid mobs are supposed to respawn 86400 seconds after this time. Actual type will be unknown and stored in memory.
};

#pragma pack()

#endif
