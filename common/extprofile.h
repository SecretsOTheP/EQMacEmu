/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2004 EQEMu Development Team (http://eqemulator.net)

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
#ifndef EXTENDED_PROFILE_H
#define EXTENDED_PROFILE_H

#include "eq_packet_structs.h"
#include "inventory_profile.h"

#pragma pack(1)

/*
	This is a place to store data that dosent have a home in the
	official player profile.

	Try not to expand or re-arrange thing in this struct
	to make conversion between sizes easier.
	if something gets removed from here, just turn it into an unused
	item, so conversion is not needed. Then reuse old unused values if
	possible later instead of adding more, so long as old values left
	over would not be extrememly unpleasent side effects.
*/
struct ExtendedProfile_Struct {
	// Pet stuff
	uint16				pet_id; /* Not Used */
	uint16				old_pet_hp; /* Not Used */
	uint16				old_pet_mana; /* Not Used */
	SpellBuff_Struct	pet_buffs[BUFF_COUNT]; /* Not Used */
	EQ::TextureProfile	pet_items; /* Not Used */
	char				merc_name[64]; /* Used */

	uint32				aa_effects; /* Used */
	uint32				perAA; /* Used: % of exp going to AAs */
	uint32				expended_aa; /* Used: Total of expended AA */
	uint32				pet_hp;  /* Not Used */
	uint32				pet_mana; /* Not Used */
	uint32				mercTemplateID; /* Not Used */
	uint32				mercSuspendedTime; /* Not Used */
	bool				mercIsSuspended; /* Not Used */
	uint32				mercTimerRemaining; /* Not Used */
	uint8				mercGender; /* Not Used */
	int32				mercState; /* Not Used */
	uint8				self_found;
	uint8				solo_only;
	uint8				hardcore;
	uint64				hardcore_death_time;
	uint8				betabuff_gear_flag;
	uint32				zone_guild_id;
	char				temp_last_name[64]; /* Erollsi Holiday Event Related */
	uint32				married_character_id;
	uint8				char_export_flag;
	uint32				e_times_rebirthed;
	uint32				e_ng_respecs_remaining;
};

#pragma pack()

void InitExtendedProfile(ExtendedProfile_Struct *p);
bool SetExtendedProfile(ExtendedProfile_Struct *to, char *old, unsigned int len);


#endif
