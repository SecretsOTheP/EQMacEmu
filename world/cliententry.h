#ifndef CLIENTENTRY_H_
#define CLIENTENTRY_H_

#include "../common/types.h"
#include "../common/md5.h"
//#include "../common/eq_packet_structs.h"
#include "../common/servertalk.h"
#include "../common/rulesys.h"
#include <vector>


#define CLE_Status_Never		-1
#define CLE_Status_Offline		0
#define CLE_Status_Online		1	// Will not overwrite more specific online status
#define CLE_Status_CharSelect	2
#define CLE_Status_Zoning		3
#define CLE_Status_InZone		4

class ZoneServer;
struct ServerClientList_Struct;

class ClientListEntry {
public:
	ClientListEntry(uint32 id, uint32 iLSID, const char* iLoginName, const char* iLoginKey, int16 iWorldAdmin = 0, uint32 ip = 0, uint8 local=0, uint8 version=0, int8 revoked = false);
	ClientListEntry(uint32 id, ZoneServer* iZS, ServerClientList_Struct* scl, int8 iOnline);
	~ClientListEntry();
	bool	CheckStale();
	void	SetStale() { stale = 3; };
	void	Update(ZoneServer* zoneserver, ServerClientList_Struct* scl, int8 iOnline = CLE_Status_InZone);
	void	LSUpdate(ZoneServer* zoneserver);
	void	LSZoneChange(ZoneToZone_Struct* ztz);
	bool	CheckAuth(uint32 iLSID, const char* key);
	bool	CheckAuth(const char* iName, const MD5& iMD5Password);
	bool	CheckAuth(uint32 id, const char* key, uint32 ip);
	void	SetOnline(ZoneServer* iZS, int8 iOnline);
	void	SetOnline(int8 iOnline = CLE_Status_Online);
	void	SetChar(uint32 iCharID, const char* iCharName);
	inline int8		Online()		{ return pOnline; }
	inline const uint32	GetID() const	{ return id; }
	inline const uint32	GetIP() const	{ return pIP; }
	inline void			SetIP(const uint32& iIP) { pIP = iIP; }
	inline void			KeepAlive()		{ stale = 0; }
	inline uint8			GetStaleCounter() const { return stale; }
	void	LeavingZone(ZoneServer* iZS = 0, int8 iOnline = CLE_Status_Offline);
	void	Camp(ZoneServer* iZS = 0);

	// Login Server stuff
	inline uint32		LSID()	const		{ return pLSID; }
	inline uint32		LSAccountID() const	{ return pLSID; }
	inline const char*	LSName() const		{ return plsname; }
	inline int16		WorldAdmin() const	{ return pworldadmin; }
	inline const char*	GetLSKey() const	{ return plskey; }

	// Account stuff
	inline uint32		AccountID() const		{ return paccountid; }
	inline const char*	AccountName() const		{ return paccountname; }
	inline int16		Admin() const			{ return padmin; }
	inline void			SetAdmin(uint16 iAdmin)	{ padmin = iAdmin; }
	inline void			SetAccountID(uint32 new_id) { paccountid = new_id; }

	// Character info
	inline ZoneServer*	Server() const		{ return pzoneserver; }
	inline void			ClearServer()		{ pzoneserver = 0; }
	inline uint32		CharID() const		{ return pcharid; }
	inline uint32		GroupID() const		{ return pgroupid; }
	inline const char*	name() const		{ return pname; }
	inline uint32		zone() const		{ return pzone; }
	inline uint32		zoneguildid() const { return pzoneguildid; }
	inline uint8			level() const		{ return plevel; }
	inline uint8			class_() const		{ return pclass_; }
	inline uint16		race() const		{ return prace; }
	inline void			SetRevoked(uint8 in_revoked) { pRevoked = in_revoked; }
	inline uint16		baserace()	const		{ return pbaserace; }
	inline uint8			Anon()				{ return panon; }
	inline uint8			TellsOff() const	{ return ptellsoff; }
	inline uint32		GuildID() const	{ return pguild_id; }
	inline void			SetGuild(uint32 guild_id) { pguild_id = guild_id; }
	inline void			SetGroupID(uint32 group_id) { pgroupid = group_id; }
	inline bool			LFG() const			{ return pLFG; }
	inline bool			LD() const			{ return pLD; }
	inline uint8			GetGM() const		{ return gm; }
	inline void			SetGM(uint8 igm)	{ gm = igm; }
	inline void			SetZone(uint32 zone) { pzone = zone; }
	inline void			SetZoneGuildID(uint32 zoneguildid) { pzoneguildid = zoneguildid; }
	inline uint32			GetZoneGuildID() { return pzoneguildid; }
	inline bool	IsLocalClient() const { return plocal; }
	inline uint8			GetLFGFromLevel() const { return pLFGFromLevel; }
	inline uint8			GetLFGToLevel() const { return pLFGToLevel; }
	inline bool			GetLFGMatchFilter() const { return pLFGMatchFilter; }
	inline const char*		GetLFGComments() const { return pLFGComments; }
	inline uint8	GetClientVersion() { return pClientVersion; }
	inline uint8	GetMacClientVersion() { return pversion; }
	inline void	SetMacClientVersion(uint8 value) { pversion = value; }
	inline bool			mule() const { return pmule; }
	inline bool		AFK() const { return pAFK;  }
	inline bool		Trader() const { return pTrader; }
	inline int8		Revoked() const { return pRevoked; }

	inline bool TellQueueFull() const { return tell_queue.size() >= RuleI(World, TellQueueSize); }
	inline bool TellQueueEmpty() const { return tell_queue.empty(); }
	inline void PushToTellQueue(ServerChannelMessage_Struct *scm) { tell_queue.push_back(scm); }
	void ProcessTellQueue();


private:
	void	ClearVars(bool iAll = false);

	const uint32	id;
	uint32	pIP;
	int8	pOnline;
	uint8	stale;

	// Login Server stuff
	uint32	pLSID;
	char	plsname[32];
	char	plskey[16];
	int16	pworldadmin;		// Login server's suggested admin status setting
	bool	plocal;
	uint8	pversion;

	// Account stuff
	uint32	paccountid;
	char	paccountname[32];
	MD5		pMD5Pass;
	int16	padmin;

	bool incremented_player_count;

	// Character info
	ZoneServer* pzoneserver;
	uint32	pzone;
	uint32	pzoneguildid;
	uint32	pcharid;
	char	pname[64];
	uint32  pgroupid;
	uint8	plevel;
	uint8	pclass_;
	uint16	prace;
	uint8	panon;
	uint8	ptellsoff;
	uint32	pguild_id;
	bool	pLFG;
	uint8	gm;
	uint8	pClientVersion;
	uint8	pLFGFromLevel;
	uint8	pLFGToLevel;
	bool	pLFGMatchFilter;
	char	pLFGComments[64];
	bool	pLD;
	uint16  pbaserace;
	bool	pmule;
	bool	pAFK;
	bool	pTrader;
	int8	pRevoked;

	// Tell Queue -- really a vector :D
	std::vector<ServerChannelMessage_Struct *> tell_queue;
	std::vector<SoulMarkEntry_Struct> soulmarks;
};

#endif /*CLIENTENTRY_H_*/

