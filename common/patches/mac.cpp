#include "../global_define.h"
#include "../eqemu_config.h"
#include "../eqemu_logsys.h"
#include "mac.h"
#include "../opcodemgr.h"
#include "../eq_stream_ident.h"
#include "../crc32.h"

#include "../eq_packet_structs.h"
#include "../packet_dump_file.h"
#include "../misc_functions.h"
#include "../packet_functions.h"
#include "../strings.h"
#include "../inventory_profile.h"
#include "mac_structs.h"
#include "../rulesys.h"
#include "../path_manager.h"
#include "../content/world_content_service.h"

namespace Mac {

	static const char *name = "Mac";
	static OpcodeManager *opcodes = nullptr;
	static Strategy struct_strategy;

	structs::Item_Struct* MacItem(const EQ::ItemInstance *inst, int16 slot_id_in, int type = 0);

	static inline int16 ServerToMacSlot(uint32 ServerSlot);
	static inline int16 ServerToMacCorpseSlot(uint32 ServerCorpse);

	static inline uint32 MacToServerSlot(int16 MacSlot);
	static inline uint32 MacToServerCorpseSlot(int16 MacCorpse);

	void Register(EQStreamIdentifier &into)
	{
		//create our opcode manager if we havent already
		if(opcodes == nullptr) 
		{
			std::string opfile = fmt::format("{}/patch_{}.conf", path.GetPatchPath(), name);
			//load up the opcode manager.
			//TODO: figure out how to support shared memory with multiple patches...
			opcodes = new RegularOpcodeManager();
			if(!opcodes->LoadOpcodes(opfile.c_str())) 
			{
				LogNetcode("[OPCODES] Error loading opcodes file {}. Not registering patch {}.", opfile.c_str(), name);
				return;
			}
		}

		//ok, now we have what we need to register.

		EQStreamInterface::Signature signature;
		std::string pname;

		signature.ignore_eq_opcode = 0;

		// Intel version's OP_SendLoginInfo is 200 bytes
		pname = std::string(name) + "_world";
		//register our world signature.
		signature.first_length = sizeof(structs::LoginInfo_Struct) + 4;
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_SendLoginInfo);
		into.RegisterOldPatch(signature, pname.c_str(), &opcodes, &struct_strategy);

		// PPC version's OP_SendLoginInfo is 196 bytes
		pname = std::string(name) + "_world_PPC";
		//register our world signature.
		signature.first_length = sizeof(structs::LoginInfo_Struct);
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_SendLoginInfo);
		into.RegisterOldPatch(signature, pname.c_str(), &opcodes, &struct_strategy);

		pname = std::string(name) + "_zone";
		//register our zone signature.
		signature.first_length = sizeof(structs::SetDataRate_Struct);
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_DataRate);
		into.RegisterOldPatch(signature, pname.c_str(), &opcodes, &struct_strategy);
		
		LogNetcode("[StreamIdentify] Registered patch [{}]", name);
	}

	void Reload() 
	{

		//we have a big problem to solve here when we switch back to shared memory
		//opcode managers because we need to change the manager pointer, which means
		//we need to go to every stream and replace it's manager.

		if(opcodes != nullptr) 
		{
			std::string opfile = fmt::format("{}/patch_{}.conf", path.GetPatchPath(), name);
			if(!opcodes->ReloadOpcodes(opfile.c_str()))
			{
				LogNetcode("[OPCODES] Error reloading opcodes file [{}] for patch [{}]", opfile.c_str(), name);
				return;
			}
			LogNetcode("[OPCODES] Reloaded opcodes for patch [{}]", name);
		}
	}



	Strategy::Strategy()
	: StructStrategy()
	{
		//all opcodes default to passthrough.
		#include "ss_register.h"
		#include "mac_ops.h"
	}

	std::string Strategy::Describe() const 
	{
		std::string r;
		r += "Patch ";
		r += name;
		return(r);
	}

	#include "ss_define.h"

	const EQ::versions::ClientVersion Strategy::ClientVersion() const
	{
		return EQ::versions::ClientVersion::Mac;
	}

	DECODE(OP_SendLoginInfo)
	{
		// Intel Mac client and Windows client are 200 bytes, PPC is 196 bytes
		int len = __packet->size;
		DECODE_LENGTH_ATLEAST(structs::LoginInfo_Struct);
		SETUP_DIRECT_DECODE(LoginInfo_Struct, structs::LoginInfo_Struct);
		memcpy(emu->login_info, eq->AccountName, 64);
		emu->macversion = len == 196 ? 8 : 4;
		IN(zoning);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_PlayerProfile) 
	{
		SETUP_DIRECT_ENCODE(PlayerProfile_Struct, structs::PlayerProfile_Struct);

		eq->available_slots=0xffff;

		int r = 0;
		OUT(gender);
		OUT(race);
		OUT(class_);
		OUT(level);
		for(r = 0; r < 5; r++) {
			eq->bind_point_zone[r] = emu->binds[r].zoneId;
			eq->bind_x[r] = emu->binds[r].x;
			eq->bind_y[r] = emu->binds[r].y;
			eq->bind_z[r] = emu->binds[r].z;
			eq->bind_heading[r] = emu->binds[r].heading;
		}
		OUT(deity);
		OUT(intoxication);
		OUT(haircolor);
		OUT(beardcolor);
		OUT(eyecolor1);
		OUT(eyecolor2);
		OUT(hairstyle);
		OUT(beard);
		OUT(points);
		OUT(mana);
		OUT(cur_hp);
		OUT(STR);
		OUT(STA);
		OUT(CHA);
		OUT(DEX);
		OUT(INT);
		OUT(AGI);
		OUT(WIS);
		OUT(face);
		OUT(luclinface);
		OUT_array(spell_book, 256);
		OUT_array(mem_spells, 8);
		OUT(platinum);
		OUT(gold);
		OUT(silver);
		OUT(copper);
		OUT(platinum_cursor);
		OUT(gold_cursor);
		OUT(silver_cursor);
		OUT(copper_cursor);
		OUT_array(skills, structs::MAX_PP_SKILL);  // 1:1 direct copy (100 dword)

		for(r = 0; r < 15; r++) // buffs 1-15
		{
			eq->buffs[r].bufftype = (emu->buffs[r].spellid == 0xFFFF || emu->buffs[r].spellid == 0) ? 0 : emu->buffs[r].bufftype;
			OUT(buffs[r].level);
			OUT(buffs[r].bard_modifier);
			OUT(buffs[r].activated);
			OUT(buffs[r].spellid);
			OUT(buffs[r].duration);
			OUT(buffs[r].counters);
		}
		for (r = 0; r < 15; r++) // buffs 16-30
		{
			eq->buffs_ext[r].bufftype = (emu->buffs_ext[r].spellid == 0xFFFF || emu->buffs_ext[r].spellid == 0) ? 0 : emu->buffs_ext[r].bufftype;
			OUT(buffs_ext[r].level);
			OUT(buffs_ext[r].bard_modifier);
			OUT(buffs_ext[r].activated);
			OUT(buffs_ext[r].spellid);
			OUT(buffs_ext[r].duration);
			OUT(buffs_ext[r].counters);
		}
		OUT_str(name);
		OUT_str(last_name);
		OUT(guild_id);
		OUT(birthday);
		OUT(lastlogin);
		OUT(timePlayedMin);
		OUT(pvp);
		OUT(anon);
		OUT(gm);
		OUT(guildrank);
		eq->uniqueGuildID = emu->guild_id;
		OUT(exp);
		OUT_array(languages, 26);
		OUT(x);
		OUT(y);
		OUT(z);
		OUT(heading);
		OUT(platinum_bank);
		OUT(gold_bank);
		OUT(silver_bank);
		OUT(copper_bank);
		OUT(level2);
		OUT(autosplit);
		OUT(zone_id);
		OUT_str(boat);
		OUT(aapoints);
		OUT(expAA);
		OUT(perAA);
		OUT(air_remaining);
		if(emu->expansions > 15)
			eq->expansions = 15;
		else
			OUT(expansions);
		OUT(hunger_level);
		OUT(thirst_level);
		eq->thirst_level_unused = 127;
		eq->hunger_level_unused = 127;
		for(r = 0; r < structs::MAX_PP_AA_ARRAY; r++)
		{
			OUT(aa_array[r].AA);
			OUT(aa_array[r].value);
		}
		for(r = 0; r < 6; r++) 
		{
			OUT_str(groupMembers[r]);
		}
		for(r = EQ::textures::textureBegin; r <= EQ::textures::LastTexture; r++) 
		{
			OUT(item_material.Slot[r].Material);
		}
		OUT(abilitySlotRefresh);
		OUT_array(spellSlotRefresh, spells::SPELL_GEM_COUNT);
		OUT(eqbackground);
		OUT(fatigue);
		OUT(height);
		OUT(width);
		OUT(length);
		OUT(view_height);
		OUT_array(cursorbaginventory,pp_cursorbaginventory_size);
		for(r = 0; r < pp_cursorbaginventory_size; r++)
		{
			OUT(cursorItemProperties[r].charges);
		}
		OUT_array(inventory,pp_inventory_size);
		for(r = 0; r < pp_inventory_size; r++)
		{
			OUT(invItemProperties[r].charges);
		}
		OUT_array(containerinv,pp_containerinv_size);
		for(r = 0; r < pp_containerinv_size; r++)
		{
			OUT(bagItemProperties[r].charges);
		}
		OUT_array(bank_inv,pp_bank_inv_size);
		for(r = 0; r < pp_bank_inv_size; r++)
		{
			OUT(bankinvitemproperties[r].charges);
		}
		OUT_array(bank_cont_inv,pp_containerinv_size);
		for(r = 0; r < pp_containerinv_size; r++)
		{
			OUT(bankbagitemproperties[r].charges);
		}
		OUT(LastModulated);

		LogNetcode("[STRUCTS] Player Profile Packet is {} bytes uncompressed", sizeof(structs::PlayerProfile_Struct));

		CRC32::SetEQChecksum(__packet->pBuffer, sizeof(structs::PlayerProfile_Struct)-4);
		auto outapp = new EQApplicationPacket(OP_PlayerProfile, 8192);
		outapp->size = DeflatePacket((unsigned char*)__packet->pBuffer, sizeof(structs::PlayerProfile_Struct), outapp->pBuffer, 8192);
		EncryptProfilePacket(outapp->pBuffer, outapp->size);
		LogNetcode("[STRUCTS] Player Profile Packet is {} bytes compressed", outapp->size);
		dest->FastQueuePacket(&outapp);
		delete[] __emu_buffer;
		delete __packet;
	}

	ENCODE(OP_NewZone) 
	{
		SETUP_DIRECT_ENCODE(NewZone_Struct, structs::NewZone_Struct);
		OUT_str(char_name);
		OUT_str(zone_short_name);
		OUT_str(zone_long_name);
		OUT(ztype);
		OUT_array(fog_red, 4);
		OUT_array(fog_green, 4);
		OUT_array(fog_blue, 4);
		OUT_array(fog_minclip, 4);
		OUT_array(fog_maxclip, 4);
		OUT(gravity);
		OUT(time_type);
		OUT(sky);
		OUT(zone_exp_multiplier);
		OUT(safe_y);
		OUT(safe_x);
		OUT(safe_z);
		OUT(max_z);
		OUT(underworld);
		OUT(minclip);
		OUT(maxclip);
		OUT(skylock);
		OUT(graveyard_time);
		OUT(timezone);
		OUT_array(snow_chance, 4);
		OUT_array(snow_duration, 4);
		OUT_array(rain_chance, 4);
		OUT_array(rain_duration, 4);
		OUT(normal_music_day);
		OUT(water_music);
		OUT(normal_music_night);
		FINISH_ENCODE();	
	}

	ENCODE(OP_SpecialMesg)
	{
		EQApplicationPacket *__packet = *p; 
		*p = nullptr; 
		unsigned char *__emu_buffer = __packet->pBuffer; 
		SpecialMesg_Struct *emu = (SpecialMesg_Struct *) __emu_buffer; 
		uint32 __i = 0; 
		__i++; /* to shut up compiler */
	
		int msglen = __packet->size - sizeof(structs::SpecialMesg_Struct);
		int len = sizeof(structs::SpecialMesg_Struct) + msglen + 1;
		__packet->pBuffer = new unsigned char[len]; 
		__packet->size = len; 
		memset(__packet->pBuffer, 0, len); 
		structs::SpecialMesg_Struct *eq = (structs::SpecialMesg_Struct *) __packet->pBuffer; 
		eq->msg_type = emu->msg_type;
		strcpy(eq->message, emu->message);
		FINISH_ENCODE();
	}

	ENCODE(OP_NewSpawn) { ENCODE_FORWARD(OP_ZoneSpawns); }

	ENCODE(OP_ZoneSpawns)
	{

		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;
		Spawn_Struct *emu = (Spawn_Struct *) __emu_buffer;

		//determine and verify length
		int entrycount = in->size / sizeof(Spawn_Struct);
		if(entrycount == 0 || (in->size % sizeof(Spawn_Struct)) != 0) 
		{
			LogNetcode("[STRUCTS] Wrong size on outbound {}: Got {}, expected multiple of {}", opcodes->EmuToName(in->GetOpcode()), in->size, sizeof(Spawn_Struct));
			delete in;
			return;
		}
		EQApplicationPacket* out = new EQApplicationPacket();
		out->SetOpcode(OP_ZoneSpawns);
		//make the EQ struct.
		out->size = sizeof(structs::Spawn_Struct)*entrycount;
		out->pBuffer = new unsigned char[out->size];
		structs::Spawn_Struct *eq = (structs::Spawn_Struct *) out->pBuffer;

		//zero out the packet. We could avoid this memset by setting all fields (including unknowns)
		memset(out->pBuffer, 0, out->size);

		//do the transform...
		for(int r = 0; r < entrycount; r++, eq++, emu++) 
		{

			eq->GM = emu->gm;
			eq->title = emu->aa_title;
			eq->anon = emu->anon;
			memcpy(eq->name, emu->name, 64);
			eq->deity = emu->deity;
			eq->size = emu->size;
			eq->NPC = emu->NPC;
			eq->invis = emu->invis;
			//eq->sneaking = 0;
			eq->pvp = emu->pvp;
			eq->cur_hp = emu->curHp;
			eq->x_pos = emu->x;
			eq->y_pos = emu->y;
			eq->animation = emu->animation;
			eq->z_pos = emu->z * 10;
			eq->deltaY = 0;
			eq->deltaX = 0;
			eq->deltaHeading = emu->deltaHeading;
			eq->heading = emu->heading;
			eq->deltaZ = 0;
			eq->anim_type = emu->StandState;
			eq->level = emu->level;
			eq->petOwnerId = emu->petOwnerId;
			eq->temporaryPet = emu->temporaryPet;
			eq->guildrank = emu->guildrank;
			if (emu->NPC == 1)
			{
				eq->guildrank = 0;
				eq->LD = 1;
			}

			eq->bodytexture = emu->bodytexture;
			for (int k = 0; k < 9; k++)
			{
				eq->equipment[k] = emu->equipment[k];
				eq->equipcolors.Slot[k].Color = emu->colors.Slot[k].Color;
			}
			eq->runspeed = emu->runspeed;
			eq->AFK = emu->afk;
			eq->GuildID = emu->guildID;
			if (eq->GuildID == 0)
				eq->GuildID = 0xFFFF;
			eq->helm = emu->helm;
			eq->race = emu->race;
			strncpy(eq->Surname, emu->lastName, 32);
			eq->walkspeed = emu->walkspeed;
			eq->light = emu->light;
			if (emu->class_ > 19 && emu->class_ < 35)
				eq->class_ = emu->class_ - 3;
			else if (emu->class_ == 40)
				eq->class_ = 16;
			else if (emu->class_ == 41)
				eq->class_ = 32;
			else
				eq->class_ = emu->class_;
			eq->haircolor = emu->haircolor;
			eq->beardcolor = emu->beardcolor;
			eq->eyecolor1 = emu->eyecolor1;
			eq->eyecolor2 = emu->eyecolor2;
			eq->hairstyle = emu->hairstyle;
			eq->beard = emu->beard;
			eq->face = emu->face;
			eq->gender = emu->gender;
			eq->bodytype = emu->bodytype;
			eq->spawn_id = emu->spawnId;
			eq->flymode = emu->flymode;

		}

		auto outapp = new EQApplicationPacket(OP_ZoneSpawns, 8192);
		outapp->size = DeflatePacket((unsigned char*)out->pBuffer, out->size, outapp->pBuffer, 8192);
		EncryptZoneSpawnPacket(outapp->pBuffer, outapp->size);
		delete in;
		delete out;
		dest->FastQueuePacket(&outapp, ack_req);
	}

	ENCODE(OP_CancelTrade)
	{
		ENCODE_LENGTH_EXACT(CancelTrade_Struct);
		SETUP_DIRECT_ENCODE(CancelTrade_Struct, structs::CancelTrade_Struct);
		OUT(fromid);
		eq->action=1665;
		FINISH_ENCODE();
	}

	ENCODE(OP_ItemLinkResponse) {  ENCODE_FORWARD(OP_ItemPacket); }
	ENCODE(OP_ItemPacket) 
	{
		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;
		ItemPacket_Struct *old_item_pkt=(ItemPacket_Struct *)__emu_buffer;
		EQ::InternalSerializedItem_Struct *int_struct=(EQ::InternalSerializedItem_Struct *)(old_item_pkt->SerializedItem);

		const EQ::ItemInstance * item = (const EQ::ItemInstance *)int_struct->inst;
	
		if(item)
		{
			uint8 type = 0;
			if(old_item_pkt->PacketType == ItemPacketViewLink)
				type = 2;

			structs::Item_Struct* mac_item = MacItem((EQ::ItemInstance*)int_struct->inst,int_struct->slot_id, type);

			if(mac_item == 0)
			{
				delete in;
				return;
			}

			auto outapp = new EQApplicationPacket(OP_ItemPacket,sizeof(structs::Item_Struct));
			memcpy(outapp->pBuffer,mac_item,sizeof(structs::Item_Struct));
			outapp->SetOpcode(OP_Unknown);
		
			if(old_item_pkt->PacketType == ItemPacketSummonItem)
				outapp->SetOpcode(OP_SummonedItem);
			else if(old_item_pkt->PacketType == ItemPacketViewLink)
				outapp->SetOpcode(OP_ItemLinkResponse);
			else if(old_item_pkt->PacketType == ItemPacketTrade || old_item_pkt->PacketType == ItemPacketMerchant)
				outapp->SetOpcode(OP_MerchantItemPacket);
			else if(old_item_pkt->PacketType == ItemPacketLoot)
				outapp->SetOpcode(OP_LootItemPacket);
			else if(old_item_pkt->PacketType == ItemPacketWorldContainer)
				outapp->SetOpcode(OP_ObjectItemPacket);
			else if(item->GetItem()->ItemClass == EQ::item::ItemClassBag)
				outapp->SetOpcode(OP_ContainerPacket);
			else if(item->GetItem()->ItemClass == EQ::item::ItemClassBook)
				outapp->SetOpcode(OP_BookPacket);
			else if(int_struct->slot_id == EQ::invslot::slotCursor)
				outapp->SetOpcode(OP_SummonedItem);
			else
				outapp->SetOpcode(OP_ItemPacket);

			if(outapp->size != sizeof(structs::Item_Struct))
				LogNetcode("Invalid size on OP_ItemPacket packet. Expected: {}, Got: {}", sizeof(structs::Item_Struct), outapp->size);

			dest->FastQueuePacket(&outapp);
			delete mac_item;
		}
		delete in;
	}

	ENCODE(OP_TradeItemPacket)
	{
			//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;
		ItemPacket_Struct *old_item_pkt=(ItemPacket_Struct *)__emu_buffer;
		EQ::InternalSerializedItem_Struct *int_struct=(EQ::InternalSerializedItem_Struct *)(old_item_pkt->SerializedItem);

		const EQ::ItemInstance * item = (const EQ::ItemInstance *)int_struct->inst;
	
		if(item)
		{
			structs::Item_Struct* mac_item = MacItem((EQ::ItemInstance*)int_struct->inst,int_struct->slot_id);

			if(mac_item == 0)
			{
				delete in;
				return;
			}

			auto outapp = new EQApplicationPacket(OP_TradeItemPacket,sizeof(structs::TradeItemsPacket_Struct));
			structs::TradeItemsPacket_Struct* myitem = (structs::TradeItemsPacket_Struct*) outapp->pBuffer;
			myitem->fromid = old_item_pkt->fromid;
			myitem->slotid = int_struct->slot_id;
			memcpy(&myitem->item,mac_item,sizeof(structs::Item_Struct));
			dest->FastQueuePacket(&outapp);
			delete mac_item;
		}
		delete in;
	}

	ENCODE(OP_CharInventory)
	{

		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		if(in->size == 0) {
			in->size = 2;
			in->pBuffer = new uchar[in->size];
			*((uint16 *) in->pBuffer) = 0;
			dest->FastQueuePacket(&in);
			return;
		}

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;

		int16 itemcount = in->size / sizeof(EQ::InternalSerializedItem_Struct);
		if(itemcount == 0 || (in->size % sizeof(EQ::InternalSerializedItem_Struct)) != 0)
		{
			LogNetcode("[STRUCTS] Wrong size on outbound {}: Got {}, expected multiple of {}", opcodes->EmuToName(in->GetOpcode()), in->size, sizeof(EQ::InternalSerializedItem_Struct));
			delete in;
			return;
		}
		
		EQ::InternalSerializedItem_Struct *eq = (EQ::InternalSerializedItem_Struct *) in->pBuffer;
		//do the transform...
		std::string mac_item_string;
		int r;
		//std::string mac_item_string;
		for(r = 0; r < itemcount; r++, eq++) 
		{
			structs::Item_Struct* mac_item = MacItem((EQ::ItemInstance*)eq->inst,eq->slot_id);

			if(mac_item != 0)
			{
				structs::PlayerItemsPacket_Struct *macitem = new structs::PlayerItemsPacket_Struct;
				memcpy(&macitem->item, mac_item, sizeof(structs::Item_Struct));
				EQ::ItemInstance *item = (EQ::ItemInstance*)eq->inst;
				if (item->IsType(EQ::item::ItemClassCommon))
					macitem->opcode = 16740; // OP_ItemPacket
				else if (item->IsType(EQ::item::ItemClassBag))
					macitem->opcode = 16742; // OP_ContainerPacket
				else
					macitem->opcode = 16741; // OP_BookPacket

				char *mac_item_char = reinterpret_cast<char*>(macitem);
				mac_item_string.append(mac_item_char,sizeof(structs::PlayerItemsPacket_Struct));

				safe_delete(macitem);
				safe_delete(mac_item);	
			}
		}
		int buffer = 2;

		auto outapp = new EQApplicationPacket(OP_CharInventory, 16384);
		outapp->size = buffer + DeflatePacket((uchar*)mac_item_string.c_str(), mac_item_string.length(), &outapp->pBuffer[buffer], 16382);
		outapp->pBuffer[0] = itemcount;
		
		dest->FastQueuePacket(&outapp);
		delete in;
	}

	ENCODE(OP_ShopInventoryPacket)
	{
		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;

		int16 itemcount = in->size / sizeof(EQ::InternalSerializedItem_Struct);
		if(itemcount == 0 || (in->size % sizeof(EQ::InternalSerializedItem_Struct)) != 0)
		{
			LogNetcode("Wrong size on outbound {}: Got {}, expected multiple of {}", opcodes->EmuToName(in->GetOpcode()), in->size, sizeof(EQ::InternalSerializedItem_Struct));
			delete in;
			return;
		}

		if(itemcount > 80)
			itemcount = 80;

		EQ::InternalSerializedItem_Struct *eq = (EQ::InternalSerializedItem_Struct *) in->pBuffer;
		//do the transform...
		std::string mac_item_string;
		int r = 0;
		for(r = 0; r < itemcount; r++, eq++) 
		{
			EQ::ItemInstance *cur = (EQ::ItemInstance*)eq->inst;
			structs::Item_Struct* mac_item = MacItem((EQ::ItemInstance*)eq->inst,eq->slot_id,1);
			if(mac_item != 0)
			{
				structs::MerchantItemsPacket_Struct* merchant = new structs::MerchantItemsPacket_Struct;
				memset(merchant,0,sizeof(structs::MerchantItemsPacket_Struct));
				memcpy(&merchant->item,mac_item,sizeof(structs::Item_Struct));
				merchant->itemtype = mac_item->ItemClass;

				char *mac_item_char = reinterpret_cast<char*>(merchant);
				mac_item_string.append(mac_item_char,sizeof(structs::MerchantItemsPacket_Struct));
				safe_delete(mac_item);
				safe_delete(merchant);
			}
			safe_delete(cur);
		}

		int buffer = 2;

		auto outapp = new EQApplicationPacket(OP_ShopInventoryPacket, 5000);
		outapp->size = buffer + DeflatePacket((uchar*)mac_item_string.c_str(), mac_item_string.length(), &outapp->pBuffer[buffer], 4998);
		outapp->pBuffer[0] = itemcount;
		dest->FastQueuePacket(&outapp);
		delete in;
	}

	ENCODE(OP_PickPocket) 
	{
		if((*p)->size == sizeof(PickPocket_Struct))
		{
			ENCODE_LENGTH_EXACT(PickPocket_Struct);
			SETUP_DIRECT_ENCODE(PickPocket_Struct, structs::PickPocket_Struct);
			OUT(to);
			OUT(from);
			OUT(myskill);
			OUT(type);
			OUT(coin);
			FINISH_ENCODE();
		}
		else 
		{
			//consume the packet
			EQApplicationPacket *in = *p;
			*p = nullptr;

			//store away the emu struct
			unsigned char *__emu_buffer = in->pBuffer;
			ItemPacket_Struct *old_item_pkt=(ItemPacket_Struct *)__emu_buffer;
			EQ::InternalSerializedItem_Struct *int_struct=(EQ::InternalSerializedItem_Struct *)(old_item_pkt->SerializedItem);

			const EQ::ItemInstance * item = (const EQ::ItemInstance *)int_struct->inst;
	
			if(item)
			{
				structs::Item_Struct* mac_item = MacItem((EQ::ItemInstance*)int_struct->inst,int_struct->slot_id);

				if(mac_item == 0)
				{
					delete in;
					return;
				}

				auto outapp = new EQApplicationPacket(OP_PickPocket,sizeof(structs::PickPocketItemPacket_Struct));
				structs::PickPocketItemPacket_Struct* myitem = (structs::PickPocketItemPacket_Struct*) outapp->pBuffer;
				myitem->from = old_item_pkt->fromid;
				myitem->to = old_item_pkt->toid;
				myitem->myskill = old_item_pkt->skill;
				myitem->coin = 0;
				myitem->type = 5;
				memcpy(&myitem->item,mac_item,sizeof(structs::Item_Struct));

				dest->FastQueuePacket(&outapp);
				delete mac_item;
			}
			delete in;
		}
	}

	DECODE(OP_DeleteCharge) {  DECODE_FORWARD(OP_MoveItem); }
	DECODE(OP_MoveItem)
	{
		SETUP_DIRECT_DECODE(MoveItem_Struct, structs::MoveItem_Struct);

		emu->from_slot = MacToServerSlot(eq->from_slot);
		emu->to_slot = MacToServerSlot(eq->to_slot);
		IN(number_in_stack);

		LogEQMac("EQMAC DECODE OUTPUT to_slot: {}, from_slot: {}, number_in_stack: {}", emu->to_slot, emu->from_slot, emu->number_in_stack);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_DeleteCharge) {  ENCODE_FORWARD(OP_MoveItem); }
	ENCODE(OP_MoveItem)
	{
		ENCODE_LENGTH_EXACT(MoveItem_Struct);
		SETUP_DIRECT_ENCODE(MoveItem_Struct, structs::MoveItem_Struct);

		eq->from_slot = ServerToMacSlot(emu->from_slot);
		eq->to_slot = ServerToMacSlot(emu->to_slot);
		OUT(to_slot);
		OUT(number_in_stack);
		LogInventory("EQMAC ENCODE OUTPUT to_slot: {}, from_slot: {}, number_in_stack: {}", eq->to_slot, eq->from_slot, eq->number_in_stack);

		FINISH_ENCODE();
	}

	ENCODE(OP_HPUpdate)
	{
		ENCODE_LENGTH_EXACT(SpawnHPUpdate_Struct);
		SETUP_DIRECT_ENCODE(SpawnHPUpdate_Struct, structs::SpawnHPUpdate_Struct);
		OUT(spawn_id);
		OUT(cur_hp);
		OUT(max_hp);
		FINISH_ENCODE();
	}

	ENCODE(OP_MobHealth)
	{
		ENCODE_LENGTH_EXACT(SpawnHPUpdate_Struct2);
		SETUP_DIRECT_ENCODE(SpawnHPUpdate_Struct2, structs::SpawnHPUpdate_Struct);
		OUT(spawn_id);
		eq->cur_hp=emu->hp;
		eq->max_hp=100;
		FINISH_ENCODE();
	}

	ENCODE(OP_ShopRequest)
	{
		ENCODE_LENGTH_EXACT(Merchant_Click_Struct);
		SETUP_DIRECT_ENCODE(Merchant_Click_Struct, structs::Merchant_Click_Struct);
		eq->npcid=emu->npcid;
		OUT(playerid);
		OUT(command);
		eq->unknown[0] = 0x71;
		eq->unknown[1] = 0x54;
		eq->unknown[2] = 0x00;
		OUT(rate);
		FINISH_ENCODE();
	}

	DECODE(OP_ShopRequest) 
	{
		DECODE_LENGTH_EXACT(structs::Merchant_Click_Struct);
		SETUP_DIRECT_DECODE(Merchant_Click_Struct, structs::Merchant_Click_Struct);
		emu->npcid=eq->npcid;
		IN(playerid);
		IN(command);
		IN(rate);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_ShopPlayerBuy)
	{
		DECODE_LENGTH_EXACT(structs::Merchant_Sell_Struct);
		SETUP_DIRECT_DECODE(Merchant_Sell_Struct, structs::Merchant_Sell_Struct);
		emu->npcid=eq->npcid;
		IN(playerid);
		emu->itemslot = MacToServerSlot(eq->itemslot);
		IN(quantity);
		IN(price);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ShopPlayerBuy)
	{
		ENCODE_LENGTH_EXACT(Merchant_Sell_Struct);
		SETUP_DIRECT_ENCODE(Merchant_Sell_Struct, structs::Merchant_Sell_Struct);
		eq->npcid=emu->npcid;
		eq->playerid=emu->playerid;
		eq->itemslot = ServerToMacSlot(emu->itemslot);
		OUT(quantity);
		OUT(price);
		FINISH_ENCODE();
	}

	DECODE(OP_ShopPlayerSell)
	{
		DECODE_LENGTH_EXACT(structs::Merchant_Purchase_Struct);
		SETUP_DIRECT_DECODE(Merchant_Purchase_Struct, structs::Merchant_Purchase_Struct);
		emu->npcid=eq->npcid;
		//IN(playerid);
		emu->itemslot = MacToServerSlot(eq->itemslot);
		IN(quantity);
		IN(price);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ShopPlayerSell)
	{
		ENCODE_LENGTH_EXACT(Merchant_Purchase_Struct);
		SETUP_DIRECT_ENCODE(Merchant_Purchase_Struct, structs::Merchant_Purchase_Struct);
		eq->npcid=emu->npcid;
		//eq->playerid=emu->playerid;
		eq->itemslot = ServerToMacSlot(emu->itemslot);
		OUT(quantity);
		OUT(price);
		FINISH_ENCODE();
	}

	ENCODE(OP_ShopDelItem)
	{
		ENCODE_LENGTH_EXACT(Merchant_DelItem_Struct);
		SETUP_DIRECT_ENCODE(Merchant_DelItem_Struct, structs::Merchant_DelItem_Struct);
		eq->npcid=emu->npcid;
		OUT(playerid);
		eq->itemslot = ServerToMacSlot(emu->itemslot);
		if(emu->type == 0)
			eq->type=64;
		else
			OUT(type);
		FINISH_ENCODE();
	}

	ENCODE(OP_AAAction)
	{
		ENCODE_LENGTH_EXACT(UseAA_Struct);
		SETUP_DIRECT_ENCODE(UseAA_Struct, structs::UseAA_Struct);
		OUT(end);
		OUT(ability);
		OUT(begin);
		eq->unknown_void=2154;

		FINISH_ENCODE();
	}

	structs::Item_Struct* MacItem(const EQ::ItemInstance *inst, int16 slot_id_in, int type)
	{

		if(!inst)
			return 0;

		const EQ::ItemData *item=inst->GetItem();
		int32 serial = inst->GetSerialNumber();

		if(item->ID > 32767)
			return 0;

		structs::Item_Struct *mac_pop_item = new structs::Item_Struct;
		memset(mac_pop_item,0,sizeof(structs::Item_Struct));

		if(item->GMFlag == -1)
			LogEQMac("Item {} is flagged for GMs.", item->Name);

		// General items
  		if(type == 0)
  		{
			mac_pop_item->Charges = inst->GetCharges();
  			mac_pop_item->equipSlot = ServerToMacSlot(slot_id_in);
			if(item->NoDrop == 0)
				mac_pop_item->Price = 0; 
			else
				mac_pop_item->Price = item->Price;
			mac_pop_item->SellRate = item->SellRate;
  		}
		// Items on a merchant
  		else if(type == 1)
  		{ 
  			mac_pop_item->Charges = inst->GetCharges();
  			mac_pop_item->equipSlot = inst->GetMerchantSlot();
			mac_pop_item->Price = inst->GetPrice();  //This handles sellrate, faction, cha, and vendor greed for us. 
			mac_pop_item->SellRate = 1;
		}
		// Item links
		else if(type == 2)
		{
			mac_pop_item->Charges = item->MaxCharges;
			mac_pop_item->equipSlot = ServerToMacSlot(slot_id_in);
			mac_pop_item->Price = item->Price;
			mac_pop_item->SellRate = item->SellRate;
		}
  
			mac_pop_item->ItemClass = item->ItemClass;
			strcpy(mac_pop_item->Name,item->Name);
			strcpy(mac_pop_item->Lore,item->Lore);       
			strcpy(mac_pop_item->IDFile,item->IDFile);  
			mac_pop_item->Weight = item->Weight;      
			mac_pop_item->NoRent = item->NoRent;         
			mac_pop_item->NoDrop = item->NoDrop;         
			mac_pop_item->Size = item->Size;           
			mac_pop_item->ID = item->ID;       
			mac_pop_item->inv_refnum = serial;
			mac_pop_item->Icon = item->Icon;       
			mac_pop_item->Slots = item->Slots;  
			mac_pop_item->CastTime = item->CastTime;  
			mac_pop_item->SkillModType = item->SkillModType;
			mac_pop_item->SkillModValue = item->SkillModValue;
			mac_pop_item->BaneDmgRace = item->BaneDmgRace;
			mac_pop_item->BaneDmgBody = item->BaneDmgBody;
			mac_pop_item->BaneDmgAmt = item->BaneDmgAmt;
			mac_pop_item->RecLevel = item->RecLevel;       
			mac_pop_item->RecSkill = item->RecSkill;   
			mac_pop_item->ProcRate = item->ProcRate; 
			mac_pop_item->ElemDmgType = item->ElemDmgType; 
			mac_pop_item->ElemDmgAmt = item->ElemDmgAmt;
			mac_pop_item->FactionMod1 = item->FactionMod1;
			mac_pop_item->FactionMod2 = item->FactionMod2;
			mac_pop_item->FactionMod3 = item->FactionMod3;
			mac_pop_item->FactionMod4 = item->FactionMod4;
			mac_pop_item->FactionAmt1 = item->FactionAmt1;
			mac_pop_item->FactionAmt2 = item->FactionAmt2;
			mac_pop_item->FactionAmt3 = item->FactionAmt3;
			mac_pop_item->FactionAmt4 = item->FactionAmt4;
			mac_pop_item->Deity = item->Deity;
			mac_pop_item->ReqLevel = item->ReqLevel; 
			mac_pop_item->BardType = item->BardType;
			mac_pop_item->BardValue = item->BardValue;
			
			if (item->Focus.Effect > 0) {
				mac_pop_item->FocusEffect = item->Focus.Effect;
			}
			else {
				mac_pop_item->FocusEffect = 0;
			}
			
			if(item->ItemClass == 1)
			{
				mac_pop_item->container.BagType = item->BagType; 
				mac_pop_item->container.BagSlots = item->BagSlots;         
				mac_pop_item->container.BagSize = item->BagSize;    
				mac_pop_item->container.BagWR = item->BagWR; 
				mac_pop_item->container.IsBagOpen = 0;
			}
			else if(item->ItemClass == 2)
			{
				strcpy(mac_pop_item->book.Filename,item->Filename);
				mac_pop_item->book.Book = item->Book;         
				mac_pop_item->book.BookType = item->BookType; 
			}
			else
			{
			mac_pop_item->common.AStr = item->AStr;           
			mac_pop_item->common.ASta = item->ASta;           
			mac_pop_item->common.ACha = item->ACha;           
			mac_pop_item->common.ADex = item->ADex;           
			mac_pop_item->common.AInt = item->AInt;           
			mac_pop_item->common.AAgi = item->AAgi;           
			mac_pop_item->common.AWis = item->AWis;           
			mac_pop_item->common.MR = item->MR;             
			mac_pop_item->common.FR = item->FR;             
			mac_pop_item->common.CR = item->CR;             
			mac_pop_item->common.DR = item->DR;             
			mac_pop_item->common.PR = item->PR;             
			mac_pop_item->common.HP = item->HP;             
			mac_pop_item->common.Mana = item->Mana;           
			mac_pop_item->common.AC = item->AC;		
			mac_pop_item->common.MaxCharges = item->MaxCharges;    
			mac_pop_item->common.GMFlag = item->GMFlag;
			mac_pop_item->common.Light = item->Light;          
			mac_pop_item->common.Delay = item->Delay;          
			mac_pop_item->common.Damage = item->Damage;               
			mac_pop_item->common.Range = item->Range;
			mac_pop_item->common.ItemType = item->ItemType;          
			mac_pop_item->common.Magic = item->Magic;          
			mac_pop_item->common.Material = item->Material;   
			mac_pop_item->common.Color = item->Color;    
			//mac_pop_item->common.Faction = item->Faction;   
			mac_pop_item->common.Classes = item->Classes;  
			mac_pop_item->common.Races = item->Races;  
			mac_pop_item->common.Stackable = item->Stackable_; 
			}

			//FocusEffect and BardEffect is already handled above. Now figure out click, scroll, proc, and worn.

			if(item->Click.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Click.Effect;
				mac_pop_item->Effect2 = item->Click.Effect; 
				mac_pop_item->EffectType2 = item->Click.Type;  
				mac_pop_item->common.EffectType1 = item->Click.Type;
				if(item->Click.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Click.Level; 
					mac_pop_item->EffectLevel2 = item->Click.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Click.Level2; 
					mac_pop_item->EffectLevel2 = item->Click.Level2;  
				}
			}
			else if(item->Scroll.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Scroll.Effect;
				mac_pop_item->Effect2 = item->Scroll.Effect; 
				mac_pop_item->EffectType2 = item->Scroll.Type;  
				mac_pop_item->common.EffectType1 = item->Scroll.Type;
				if(item->Scroll.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Scroll.Level; 
					mac_pop_item->EffectLevel2 = item->Scroll.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Scroll.Level2; 
					mac_pop_item->EffectLevel2 = item->Scroll.Level2;  
				}
			}
			//We have some worn effect items (Lodizal Shell Shield) as proceffect in db.
			else if(item->Proc.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Proc.Effect;
				mac_pop_item->Effect2 = item->Proc.Effect; 
				if(item->Worn.Type > 0)
				{
					mac_pop_item->EffectType2 = item->Worn.Type;  
					mac_pop_item->common.EffectType1 = item->Worn.Type;
				}
				else
				{
					mac_pop_item->EffectType2 = item->Proc.Type;  
					mac_pop_item->common.EffectType1 = item->Proc.Type;
				}
				if(item->Proc.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Proc.Level; 
					mac_pop_item->EffectLevel2 = item->Proc.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Proc.Level2; 
					mac_pop_item->EffectLevel2 = item->Proc.Level2;  
				}
			}
			else if(item->Worn.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Worn.Effect;
				mac_pop_item->Effect2 = item->Worn.Effect; 
				mac_pop_item->EffectType2 = item->Worn.Type;  
				mac_pop_item->common.EffectType1 = item->Worn.Type;
				if(item->Worn.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Worn.Level; 
					mac_pop_item->EffectLevel2 = item->Worn.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Worn.Level2; 
					mac_pop_item->EffectLevel2 = item->Worn.Level2;  
				}
			}

		return mac_pop_item;
	}

	ENCODE(OP_RaidJoin) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_Unknown)
	{
		EQApplicationPacket *in = *p;
		*p = nullptr;

		LogPacketClientServer("Dropped an invalid packet: {}", opcodes->EmuToName(in->GetOpcode()));

		delete in;
		return;
	}

	static inline int16 ServerToMacSlot(uint32 ServerSlot)
	{
			 //int16 MacSlot;
			if (ServerSlot == INVALID_INDEX)
				 return INVALID_INDEX;
			
			return ServerSlot; // deprecated
	}

	static inline int16 ServerToMacCorpseSlot(uint32 ServerCorpse)
	{
		return ServerCorpse;
	}

	static inline uint32 MacToServerSlot(int16 MacSlot)
	{
		//uint32 ServerSlot;
		if (MacSlot == INVALID_INDEX)
			 return INVALID_INDEX;
		
		return MacSlot; // deprecated
	}

	static inline uint32 MacToServerCorpseSlot(int16 MacCorpse)
	{
		return MacCorpse;
	}

} //end namespace Mac

