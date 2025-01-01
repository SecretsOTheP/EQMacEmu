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
#ifndef CLIENT_H
#define CLIENT_H

#include <string>

//#include "../common/eq_stream.h"
#include "../common/linked_list.h"
#include "../common/timer.h"
//#include "zoneserver.h"

#include "../common/eq_packet_structs.h"
#include "cliententry.h"

#define CLIENT_TIMEOUT 30000

class EQApplicationPacket;
class EQStreamInterface;

class Client {
public:
	Client(EQStreamInterface* ieqs);
	~Client();

	bool	Process();
	void	SendCharInfo();
	void	EnterWorld(bool TryBootup = true);
	void	TellClientZoneUnavailable();
	void	QueuePacket(const EQApplicationPacket* app, bool ack_req = true);
	void	Clearance(int8 response);
	void	SendGuildList();
	void	SendEnterWorld(std::string name);
	void	SendExpansionInfo();
	void	SendLogServer();
	void	SendApproveWorld();

	inline uint32		GetIP()				{ return ip; }
	inline uint16		GetPort()			{ return port; }
	inline uint32		GetZoneID()			{ return zone_id; }
	inline uint32		GetZoneGuildID() { return zoneGuildID; }
	inline uint32		WaitingForBootup()	{ return zone_waiting_for_bootup; }
	inline const char *	GetAccountName()	{ if (cle) { return cle->AccountName(); } return "NOCLE"; }
	inline int16		GetAdmin()			{ if (cle) { return cle->Admin(); } return 0; }
	inline uint32		GetAccountID()		{ if (cle) { return cle->AccountID(); } return 0; }
	inline uint32		GetWID()			{ if (cle) { return cle->GetID(); } return 0; }
	inline uint32		GetLSID()			{ if (cle) { return cle->LSID(); } return 0; }
	inline const char*	GetLSKey()			{ if (cle) { return cle->GetLSKey(); } return "NOKEY"; }
	inline uint32		GetCharID()			{ return char_id; }
	inline const char*	GetCharName()		{ return char_name; }
	inline ClientListEntry* GetCLE()		{ return cle; }
	inline void			SetCLE(ClientListEntry* iCLE)			{ cle = iCLE; }
	inline uint16		GetExpansion()		{ return expansion; }
	inline uint32		GetClientVersionBit() { return m_ClientVersionBit; }
	inline bool			GetSessionLimit();


private:

	uint32	ip;
	uint16	port;
	uint32	char_id;
	char	char_name[64];
	uint32	zone_id;
	uint32	zoneGuildID;
	bool	is_player_zoning;
	Timer	autobootup_timeout;
	uint32	zone_waiting_for_bootup;
	bool	enter_world_triggered;

	EQ::versions::ClientVersion m_ClientVersion;
	uint32 m_ClientVersionBit;
	bool OPCharCreate(char *name, CharCreate_Struct *cc);

	bool CheckCharCreateInfo(CharCreate_Struct *cc);

	void SetClassStartingSkills( PlayerProfile_Struct *pp );
	void SetRaceStartingSkills( PlayerProfile_Struct *pp );
	void SetRacialLanguages( PlayerProfile_Struct *pp );
	void SetClassLanguages(PlayerProfile_Struct *pp);

	ClientListEntry* cle;
	Timer	connect;
	bool firstlogin;
	bool seen_character_select;
	bool realfirstlogin;

	bool HandlePacket(const EQApplicationPacket *app);
	bool HandleNameApprovalPacket(const EQApplicationPacket *app);
	bool HandleSendLoginInfoPacket(const EQApplicationPacket *app);
	bool HandleGenerateRandomNamePacket(const EQApplicationPacket *app);
	bool HandleCharacterCreatePacket(const EQApplicationPacket *app);
	bool HandleEnterWorldPacket(const EQApplicationPacket *app);
	bool HandleDeleteCharacterPacket(const EQApplicationPacket *app);
	bool HandleChecksumPacket(const EQApplicationPacket *app);

	EQStreamInterface* const eqs;

	void RecordPossibleHack(const std::string &message);

	uint16	expansion;
	uint8	charcount;
	bool	mule;
};

#endif
