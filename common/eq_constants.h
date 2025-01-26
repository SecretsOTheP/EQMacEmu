/*	EQEMu: Everquest Server Emulator

	Copyright (C) 2001-2016 EQEMu Development Team (http://eqemulator.net)

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
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef EQ_CONSTANTS_H
#define EQ_CONSTANTS_H

#include "skills.h"
#include "types.h"

#define MAX_GROUP_MEMBERS 6

namespace AppearanceType {
	constexpr uint32 Die          = 0; // Causes the client to keel over and zone to bind point (default action)
	constexpr uint32 WhoLevel     = 1; // Level that shows up on /who
	constexpr uint32 MaxHealth    = 2;
	constexpr uint32 Invisibility = 3; // 0 = Visible, 1 = Invisible
	constexpr uint32 PVP          = 4; // 0 = Non-PVP, 1 = PVP
	constexpr uint32 Light        = 5; // Light type emitted by player (lightstone, shiny shield)
	constexpr uint32 Animation    = 14; // 100 = Standing, 102 = Freeze, 105 = Looting, 110 = Sitting, 111 = Crouching, 115 = Lying
	constexpr uint32 Sneak        = 15; // 0 = Normal, 1 = Sneaking
	constexpr uint32 SpawnID      = 16; // Server -> Client, sets player spawn ID
	constexpr uint32 Health       = 17; // Client->Server, my HP has changed (like regen tic)
	constexpr uint32 Linkdead     = 18; // 0 = Normal, 1 = Linkdead
	constexpr uint32 FlyMode      = 19; // 0 = Off, 1 = Flying, 2 = Levitating, 3 = Water, 4 = Floating, 5 = Levitating while Running
	constexpr uint32 GM           = 20; // 0 = Non-GM, 1 = GM
	constexpr uint32 Anonymous    = 21; // 0 = Non-Anonymous, 1 = Anonymous, 2 = Roleplaying
	constexpr uint32 GuildID      = 22;
	constexpr uint32 GuildRank    = 23;
	constexpr uint32 AFK          = 24; // 0 = Non-AFK, 1 = AFK
	constexpr uint32 Pet          = 25; // Parameter is Entity ID of owner, or 0 for when charm breaks
	constexpr uint32 Summoned     = 27;
	constexpr uint32 Split        = 28; // 0 = No Split, 1 = uint32 Split
	constexpr uint32 Size         = 29; // Spawn's Size
	constexpr uint32 SetType      = 30; // 0 = PC, 1 = NPC, 2 = Corpse
	constexpr uint32 NPCName      = 31; // Change PC name color to NPC name color
	constexpr uint32 DamageState  = 44; // The damage state of a destructible object (0 through 10) plays sound IDs, most only have 2 or 4 states though

	// Used to exchange feature flags between the client and server. See Handle_Connect_OP_SpawnAppearance.
	// Each codebase should use a uqniue AppearanceType so they can process their own set of messages without consuming each other's messages.
	constexpr uint32 ClientDllMessage = 256;
	constexpr uint32 ClientZealMessage = 257;
}

// Feature Flag IDs used by the above ClientDllFeature/ClientZealFeature handshake messages.
namespace ClientFeature {
	constexpr uint16 BuffStackingPatchHandshake = 1;
}

// solar: Animations for AnimationType:Animation
namespace Animation {
	constexpr uint32 Standing  = 100;
	constexpr uint32 Freeze    = 102;
	constexpr uint32 Looting   = 105;
	constexpr uint32 Sitting   = 110;
	constexpr uint32 Crouching = 111;
	constexpr uint32 Lying     = 115;
	constexpr uint32 Corpse    = 120;
}

typedef enum EmuAppearance {
	eaStanding = 0,
	eaSitting,		//1
	eaCrouching,	//2
	eaDead,			//3
	eaLooting,		//4
	_eaMaxAppearance
} EmuAppearance;

namespace Chat {
	const uint16 White       = 0;
	const uint16 DimGray     = 1;
	const uint16 Default     = 0;
	const uint16 Green       = 2;
	const uint16 BrightBlue  = 3;
	const uint16 LightBlue   = 4;
	const uint16 Magenta     = 5;
	const uint16 Gray        = 6;
	const uint16 LightGray   = 7;
	const uint16 NPCQuestSay = 10;
	const uint16 DarkGray    = 12;
	const uint16 Red         = 13;
	const uint16 Lime        = 14;
	const uint16 Yellow      = 15;
	const uint16 Blue        = 16;
	const uint16 LightNavy   = 17;
	const uint16 Cyan        = 18;
	const uint16 Black       = 20;

	/**
	 * User colors
	 */
	const uint16 Say              = 256;
	const uint16 Tell             = 257;
	const uint16 Group            = 258;
	const uint16 Guild            = 259;
	const uint16 OOC              = 260;
	const uint16 Auction          = 261;
	const uint16 Shout            = 262;
	const uint16 Emote            = 263;
	const uint16 Spells           = 264;
	const uint16 YouHitOther      = 265;
	const uint16 OtherHitYou      = 266;
	const uint16 YouMissOther     = 267;
	const uint16 OtherMissYou     = 268;
	const uint16 Broadcasts       = 269;
	const uint16 Skills           = 270;
	const uint16 Disciplines      = 271;
	const uint16 Unused1          = 272;
	const uint16 DefaultText      = 273;
	const uint16 Unused2          = 274;
	const uint16 MerchantOffer    = 275;
	const uint16 MerchantExchange = 276;
	const uint16 YourDeath        = 277;
	const uint16 OtherDeath       = 278;
	const uint16 OtherHitOther    = 279;
	const uint16 OtherMissOther   = 280;
	const uint16 Who              = 281;
	const uint16 YellForHelp      = 282;
	const uint16 NonMelee         = 283;
	const uint16 SpellWornOff     = 284;
	const uint16 MoneySplit       = 285;
	const uint16 Loot             = 286;
	const uint16 DiceRoll         = 287;
	const uint16 OtherSpells      = 288;
	const uint16 SpellFailure     = 289;
	const uint16 ChatChannel      = 290;
	const uint16 Chat1            = 291;
	const uint16 Chat2            = 292;
	const uint16 Chat3            = 293;
	const uint16 Chat4            = 294;
	const uint16 Chat5            = 295;
	const uint16 Chat6            = 296;
	const uint16 Chat7            = 297;
	const uint16 Chat8            = 298;
	const uint16 Chat9            = 299;
	const uint16 Chat10           = 300;
	const uint16 MeleeCrit        = 301;
	const uint16 SpellCrit        = 302;
	const uint16 TooFarAway       = 303;
	const uint16 NPCRampage       = 304;
	const uint16 NPCFlurry        = 305;
	const uint16 NPCEnrage        = 306;
	const uint16 EchoSay          = 307;
	const uint16 EchoTell         = 308;
	const uint16 EchoGroup        = 309;
	const uint16 EchoGuild        = 310;
	const uint16 EchoOOC          = 311;
	const uint16 EchoAuction      = 312;
	const uint16 EchoShout        = 313;
	const uint16 EchoEmote        = 314;
	const uint16 EchoChat1        = 315;
	const uint16 EchoChat2        = 316;
	const uint16 EchoChat3        = 317;
	const uint16 EchoChat4        = 318;
	const uint16 EchoChat5        = 319;
	const uint16 EchoChat6        = 320;
	const uint16 EchoChat7        = 321;
	const uint16 EchoChat8        = 322;
	const uint16 EchoChat9        = 323;
	const uint16 EchoChat10       = 324;
	const uint16 DotDamage        = 325;
	const uint16 ItemLink         = 326;
	const uint16 RaidSay          = 327;
	const uint16 MyPet            = 328;
	const uint16 DamageShield     = 329;
	const uint16 LeaderShip       = 330;
	const uint16 PetFlurry        = 331;
	const uint16 PetCritical      = 332;
	const uint16 FocusEffect      = 333;
	const uint16 Experience       = 334;
	const uint16 System           = 335;
	const uint16 PetSpell         = 336;
	const uint16 PetResponse      = 337;
	const uint16 ItemSpeech       = 338;
	const uint16 StrikeThrough    = 339;
	const uint16 Stun             = 340;
};

namespace Zones {
	constexpr uint16 QEYNOS        = 1; // South Qeynos
	constexpr uint16 QEYNOS2       = 2; // North Qeynos
	constexpr uint16 QRG           = 3; // The Surefall Glade
	constexpr uint16 QEYTOQRG      = 4; // The Qeynos Hills
	constexpr uint16 HIGHPASS      = 5; // Highpass Hold
	constexpr uint16 HIGHKEEP      = 6; // High Keep
	constexpr uint16 FREPORTN      = 8; // North Freeport
	constexpr uint16 FREPORTW      = 9; // West Freeport
	constexpr uint16 FREPORTE      = 10; // East Freeport
	constexpr uint16 RUNNYEYE      = 11; // The Liberated Citadel of Runnyeye
	constexpr uint16 QEY2HH1       = 12; // The Western Plains of Karana
	constexpr uint16 NORTHKARANA   = 13; // The Northern Plains of Karana
	constexpr uint16 SOUTHKARANA   = 14; // The Southern Plains of Karana
	constexpr uint16 EASTKARANA    = 15; // Eastern Plains of Karana
	constexpr uint16 BEHOLDER      = 16; // Gorge of King Xorbb
	constexpr uint16 BLACKBURROW   = 17; // Blackburrow
	constexpr uint16 PAW           = 18; // The Lair of the Splitpaw
	constexpr uint16 RIVERVALE     = 19; // Rivervale
	constexpr uint16 KITHICOR      = 20; // Kithicor Forest
	constexpr uint16 COMMONS       = 21; // West Commonlands
	constexpr uint16 ECOMMONS      = 22; // East Commonlands
	constexpr uint16 ERUDNINT      = 23; // The Erudin Palace
	constexpr uint16 ERUDNEXT      = 24; // Erudin
	constexpr uint16 NEKTULOS      = 25; // The Nektulos Forest
	constexpr uint16 CSHOME        = 26; // Sunset Home
	constexpr uint16 LAVASTORM     = 27; // The Lavastorm Mountains
	constexpr uint16 NEKTROPOS     = 28; // Nektropos
	constexpr uint16 HALAS         = 29; // Halas
	constexpr uint16 EVERFROST     = 30; // Everfrost Peaks
	constexpr uint16 SOLDUNGA      = 31; // Solusek's Eye
	constexpr uint16 SOLDUNGB      = 32; // Nagafen's Lair
	constexpr uint16 MISTY         = 33; // Misty Thicket
	constexpr uint16 NRO           = 34; // Northern Desert of Ro
	constexpr uint16 SRO           = 35; // Southern Desert of Ro
	constexpr uint16 BEFALLEN      = 36; // Befallen
	constexpr uint16 OASIS         = 37; // Oasis of Marr
	constexpr uint16 TOX           = 38; // Toxxulia Forest
	constexpr uint16 HOLE          = 39; // The Hole
	constexpr uint16 NERIAKA       = 40; // Neriak - Foreign Quarter
	constexpr uint16 NERIAKB       = 41; // Neriak - Commons
	constexpr uint16 NERIAKC       = 42; // Neriak - 3rd Gate
	constexpr uint16 NERIAKD       = 43; // Neriak Palace
	constexpr uint16 NAJENA        = 44; // Najena
	constexpr uint16 QCAT          = 45; // The Qeynos Aqueduct System
	constexpr uint16 INNOTHULE     = 46; // Innothule Swamp
	constexpr uint16 FEERROTT      = 47; // The Feerrott
	constexpr uint16 CAZICTHULE    = 48; // Accursed Temple of CazicThule
	constexpr uint16 OGGOK         = 49; // Oggok
	constexpr uint16 RATHEMTN      = 50; // The Rathe Mountains
	constexpr uint16 LAKERATHE     = 51; // Lake Rathetear
	constexpr uint16 GROBB         = 52; // Grobb
	constexpr uint16 AVIAK         = 53; // Aviak Village
	constexpr uint16 GFAYDARK      = 54; // The Greater Faydark
	constexpr uint16 AKANON        = 55; // Ak'Anon
	constexpr uint16 STEAMFONT     = 56; // Steamfont Mountains
	constexpr uint16 LFAYDARK      = 57; // The Lesser Faydark
	constexpr uint16 CRUSHBONE     = 58; // Crushbone
	constexpr uint16 MISTMOORE     = 59; // The Castle of Mistmoore
	constexpr uint16 KALADIMA      = 60; // South Kaladim
	constexpr uint16 FELWITHEA     = 61; // Northern Felwithe
	constexpr uint16 FELWITHEB     = 62; // Southern Felwithe
	constexpr uint16 UNREST        = 63; // The Estate of Unrest
	constexpr uint16 KEDGE         = 64; // Kedge Keep
	constexpr uint16 GUKTOP        = 65; // The City of Guk
	constexpr uint16 GUKBOTTOM     = 66; // The Ruins of Old Guk
	constexpr uint16 KALADIMB      = 67; // North Kaladim
	constexpr uint16 BUTCHER       = 68; // Butcherblock Mountains
	constexpr uint16 OOT           = 69; // Ocean of Tears
	constexpr uint16 CAULDRON      = 70; // Dagnor's Cauldron
	constexpr uint16 AIRPLANE      = 71; // The Plane of Sky
	constexpr uint16 FEARPLANE     = 72; // The Plane of Fear
	constexpr uint16 PERMAFROST    = 73; // The Permafrost Caverns
	constexpr uint16 KERRARIDGE    = 74; // Kerra Isle
	constexpr uint16 PAINEEL       = 75; // Paineel
	constexpr uint16 HATEPLANE     = 76; // Plane of Hate
	constexpr uint16 ARENA         = 77; // The Arena
	constexpr uint16 FIELDOFBONE   = 78; // The Field of Bone
	constexpr uint16 WARSLIKSWOOD  = 79; // The Warsliks Woods
	constexpr uint16 SOLTEMPLE     = 80; // The Temple of Solusek Ro
	constexpr uint16 DROGA         = 81; // The Temple of Droga
	constexpr uint16 CABWEST       = 82; // Cabilis West
	constexpr uint16 SWAMPOFNOHOPE = 83; // The Swamp of No Hope
	constexpr uint16 FIRIONA       = 84; // Firiona Vie
	constexpr uint16 LAKEOFILLOMEN = 85; // Lake of Ill Omen
	constexpr uint16 DREADLANDS    = 86; // The Dreadlands
	constexpr uint16 BURNINGWOOD   = 87; // The Burning Wood
	constexpr uint16 KAESORA       = 88; // Kaesora
	constexpr uint16 SEBILIS       = 89; // The Ruins of Sebilis
	constexpr uint16 CITYMIST      = 90; // The City of Mist
	constexpr uint16 SKYFIRE       = 91; // The Skyfire Mountains
	constexpr uint16 FRONTIERMTNS  = 92; // Frontier Mountains
	constexpr uint16 OVERTHERE     = 93; // The Overthere
	constexpr uint16 EMERALDJUNGLE = 94; // The Emerald Jungle
	constexpr uint16 TRAKANON      = 95; // Trakanon's Teeth
	constexpr uint16 TIMOROUS      = 96; // Timorous Deep
	constexpr uint16 KURN          = 97; // Kurn's Tower
	constexpr uint16 ERUDSXING     = 98; // Erud's Crossing
	constexpr uint16 STONEBRUNT    = 100; // The Stonebrunt Mountains
	constexpr uint16 WARRENS       = 101; // The Warrens
	constexpr uint16 KARNOR        = 102; // Karnor's Castle
	constexpr uint16 CHARDOK       = 103; // Chardok
	constexpr uint16 DALNIR        = 104; // The Crypt of Dalnir
	constexpr uint16 CHARASIS      = 105; // The Howling Stones
	constexpr uint16 CABEAST       = 106; // Cabilis East
	constexpr uint16 NURGA         = 107; // The Mines of Nurga
	constexpr uint16 VEESHAN       = 108; // Veeshan's Peak
	constexpr uint16 VEKSAR        = 109; // Veksar
	constexpr uint16 ICECLAD       = 110; // The Iceclad Ocean
	constexpr uint16 FROZENSHADOW  = 111; // The Tower of Frozen Shadow
	constexpr uint16 VELKETOR      = 112; // Velketor's Labyrinth
	constexpr uint16 KAEL          = 113; // Kael Drakkel
	constexpr uint16 SKYSHRINE     = 114; // Skyshrine
	constexpr uint16 THURGADINA    = 115; // The City of Thurgadin
	constexpr uint16 EASTWASTES    = 116; // Eastern Wastes
	constexpr uint16 COBALTSCAR    = 117; // Cobaltscar
	constexpr uint16 GREATDIVIDE   = 118; // The Great Divide
	constexpr uint16 WAKENING      = 119; // The Wakening Land
	constexpr uint16 WESTWASTES    = 120; // The Western Wastes
	constexpr uint16 CRYSTAL       = 121; // The Crystal Caverns
	constexpr uint16 NECROPOLIS    = 123; // Dragon Necropolis
	constexpr uint16 TEMPLEVEESHAN = 124; // The Temple of Veeshan
	constexpr uint16 SIRENS        = 125; // Siren's Grotto
	constexpr uint16 MISCHIEFPLANE = 126; // The Plane of Mischief
	constexpr uint16 GROWTHPLANE   = 127; // The Plane of Growth
	constexpr uint16 SLEEPER       = 128; // The Sleeper's Tomb
	constexpr uint16 THURGADINB    = 129; // Icewell Keep
	constexpr uint16 ERUDSXING2    = 130; // Marauders Mire
	constexpr uint16 SHADOWHAVEN   = 150; // Shadow Haven
	constexpr uint16 BAZAAR        = 151; // The Bazaar
	constexpr uint16 NEXUS         = 152; // Nexus
	constexpr uint16 ECHO_         = 153; // The Echo Caverns
	constexpr uint16 ACRYLIA       = 154; // The Acrylia Caverns
	constexpr uint16 SHARVAHL      = 155; // The City of Shar Vahl
	constexpr uint16 PALUDAL       = 156; // The Paludal Caverns
	constexpr uint16 FUNGUSGROVE   = 157; // The Fungus Grove
	constexpr uint16 VEXTHAL       = 158; // Vex Thal
	constexpr uint16 SSERU         = 159; // Sanctus Seru
	constexpr uint16 KATTA         = 160; // Katta Castellum
	constexpr uint16 NETHERBIAN    = 161; // Netherbian Lair
	constexpr uint16 SSRATEMPLE    = 162; // Ssraeshza Temple
	constexpr uint16 GRIEGSEND     = 163; // Grieg's End
	constexpr uint16 THEDEEP       = 164; // The Deep
	constexpr uint16 SHADEWEAVER   = 165; // Shadeweaver's Thicket
	constexpr uint16 HOLLOWSHADE   = 166; // Hollowshade Moor
	constexpr uint16 GRIMLING      = 167; // Grimling Forest
	constexpr uint16 MSERU         = 168; // Marus Seru
	constexpr uint16 LETALIS       = 169; // Mons Letalis
	constexpr uint16 TWILIGHT      = 170; // The Twilight Sea
	constexpr uint16 THEGREY       = 171; // The Grey
	constexpr uint16 TENEBROUS     = 172; // The Tenebrous Mountains
	constexpr uint16 MAIDEN        = 173; // The Maiden's Eye
	constexpr uint16 DAWNSHROUD    = 174; // The Dawnshroud Peaks
	constexpr uint16 SCARLET       = 175; // The Scarlet Desert
	constexpr uint16 UMBRAL        = 176; // The Umbral Plains
	constexpr uint16 AKHEVA        = 179; // The Akheva Ruins
	constexpr uint16 ARENA2        = 180; // The Arena Two
	constexpr uint16 JAGGEDPINE    = 181; // The Jaggedpine Forest
	constexpr uint16 NEDARIA       = 182; // Nedaria's Landing
	constexpr uint16 TUTORIAL      = 183; // EverQuest Tutorial
	constexpr uint16 LOAD          = 184; // Loading Zone
	constexpr uint16 LOAD2         = 185; // New Loading Zone
	constexpr uint16 HATEPLANEB    = 186; // The Plane of Hate
	constexpr uint16 SHADOWREST    = 187; // Shadowrest
	constexpr uint16 TUTORIALA     = 188; // The Mines of Gloomingdeep
	constexpr uint16 TUTORIALB     = 189; // The Mines of Gloomingdeep
	constexpr uint16 CLZ           = 190; // Loading
	constexpr uint16 CODECAY       = 200; // The Crypt of Decay
	constexpr uint16 POJUSTICE     = 201; // The Plane of Justice
	constexpr uint16 POKNOWLEDGE   = 202; // The Plane of Knowledge
	constexpr uint16 POTRANQUILITY = 203; // The Plane of Tranquility
	constexpr uint16 PONIGHTMARE   = 204; // The Plane of Nightmares
	constexpr uint16 PODISEASE     = 205; // The Plane of Disease
	constexpr uint16 POINNOVATION  = 206; // The Plane of Innovation
	constexpr uint16 POTORMENT     = 207; // Torment, the Plane of Pain
	constexpr uint16 POVALOR       = 208; // The Plane of Valor
	constexpr uint16 BOTHUNDER     = 209; // Bastion of Thunder
	constexpr uint16 POSTORMS      = 210; // The Plane of Storms
	constexpr uint16 HOHONORA      = 211; // The Halls of Honor
	constexpr uint16 SOLROTOWER    = 212; // The Tower of Solusek Ro
	constexpr uint16 POWAR         = 213; // Plane of War
	constexpr uint16 POTACTICS     = 214; // Drunder, the Fortress of Zek
	constexpr uint16 POAIR         = 215; // The Plane of Air
	constexpr uint16 POWATER       = 216; // The Plane of Water
	constexpr uint16 POFIRE        = 217; // The Plane of Fire
	constexpr uint16 POEARTHA      = 218; // The Plane of Earth
	constexpr uint16 POTIMEA       = 219; // The Plane of Time
	constexpr uint16 HOHONORB      = 220; // The Temple of Marr
	constexpr uint16 NIGHTMAREB    = 221; // The Lair of Terris Thule
	constexpr uint16 POEARTHB      = 222; // The Plane of Earth
	constexpr uint16 POTIMEB       = 223; // The Plane of Time
}

namespace Language {
	constexpr uint8 CommonTongue = 0;
	constexpr uint8 Barbarian = 1;
	constexpr uint8 Erudian = 2;
	constexpr uint8 Elvish = 3;
	constexpr uint8 DarkElvish = 4;
	constexpr uint8 Dwarvish = 5;
	constexpr uint8 Troll = 6;
	constexpr uint8 Ogre = 7;
	constexpr uint8 Gnomish = 8;
	constexpr uint8 Halfling = 9;
	constexpr uint8 ThievesCant = 10;
	constexpr uint8 OldErudian = 11;
	constexpr uint8 ElderElvish = 12;
	constexpr uint8 Froglok = 13;
	constexpr uint8 Goblin = 14;
	constexpr uint8 Gnoll = 15;
	constexpr uint8 CombineTongue = 16;
	constexpr uint8 ElderTeirDal = 17;
	constexpr uint8 Lizardman = 18;
	constexpr uint8 Orcish = 19;
	constexpr uint8 Faerie = 20;
	constexpr uint8 Dragon = 21;
	constexpr uint8 ElderDragon = 22;
	constexpr uint8 DarkSpeech = 23;
	constexpr uint8 VahShir = 24;
	constexpr uint8 Unknown25 = 25;
	constexpr uint8 Unknown26 = 26;

	constexpr uint8 MaxValue = 100;
}

enum Anonymity : uint8
{
	NotAnonymous,
	Anonymous,
	Roleplaying
};

//ZoneChange_Struct->success values
enum ZoningMessage : int8
{
	ZoneNoMessage = 0,
	ZoneSuccess = 1,
	ZoneNotReady = -1,
	ZoneValidPC = -2,
	ZoneStoryZone = -3,
	ZoneNoExpansion = -6,
	ZoneNoExperience = -7
};

typedef enum {
	FilterDamageShields = 0,	//0 is on 1 is off
	FilterNPCSpells = 1,		//0 is on - doesn't send packet	
	FilterPCSpells = 2,			//0 is on 1 is off 2 is group
	FilterBardSongs = 3,		//0 is on 1 is self 2 is group 3 is off
	FilterNone = 4,				//0 is on
	FilterGuildChat = 5,		//0 is off 1 is on		
	FilterSocials = 6,			//0 is off 1 is on
	FilterGroupChat = 7,		//0 is off 1 is on	
	FilterShouts = 8,		    //0 is off 1 is on
	FilterAuctions = 9,		    //0 is off 1 is on
	FilterOOC = 10,				//0 is off 1 is on
	FilterMyMisses = 11,		//0 is off 1 is on
	FilterOthersMiss = 12,		//0 is off 1 is on
	FilterOthersHit = 13,		//0 is off 1 is on
	FilterMissedMe = 14,		//0 is off 1 is on
	FilterSpellCrits = 15,		//0 is on 1 is self 2 is off
	FilterMeleeCrits = 16,		//0 is on 1 is self 2 is off
	_FilterCount
} eqFilterType;

typedef enum {
	FilterHide,
	FilterShow,
	FilterShowGroupOnly,
	FilterShowSelfOnly
} eqFilterMode;

#define	STAT_STR		0
#define	STAT_STA		1
#define	STAT_AGI		2
#define	STAT_DEX		3
#define	STAT_INT		4
#define	STAT_WIS		5
#define	STAT_CHA		6
#define	STAT_MAGIC		7
#define	STAT_COLD		8
#define	STAT_FIRE		9
#define	STAT_POISON		10
#define	STAT_DISEASE		11
#define	STAT_MANA		12
#define	STAT_HP			13
#define	STAT_AC			14
#define STAT_ENDURANCE		15
#define STAT_ATTACK		16
#define STAT_HP_REGEN		17
#define STAT_MANA_REGEN		18
#define STAT_HASTE		19
#define STAT_DAMAGE_SHIELD	20

/*
**	Recast timer types. Used as an off set to charProfileStruct timers.
**
**	(Another orphaned enumeration...)
*/
enum RecastTimerTypes
{
	RecTimer_0 = 0,
	RecTimer_1,
	RecTimer_WeaponHealClick,		// 2
	RecTimer_MuramiteBaneNukeClick,	// 3
	RecTimer_4,
	RecTimer_DispellClick,			// 5 (also click heal orbs?)
	RecTimer_Epic,					// 6
	RecTimer_OoWBPClick,			// 7
	RecTimer_VishQuestClassItem,	// 8
	RecTimer_HealPotion,			// 9
	RecTimer_10,
	RecTimer_11,
	RecTimer_12,
	RecTimer_13,
	RecTimer_14,
	RecTimer_15,
	RecTimer_16,
	RecTimer_17,
	RecTimer_18,
	RecTimer_ModRod,				// 19
	_RecTimerCount
};

enum GroupUpdateAction
{
	GUA_Joined = 0,
	GUA_Left = 1,
	GUA_LastLeft = 6,
	GUA_FullGroupInfo = 7,
	GUA_MakeLeader = 8,
	GUA_Started = 9
};

static const uint8 DamageTypeSomething	= 0x1C;	//0x1c is something...
static const uint8 DamageTypeFalling	= 0xFC;
static const uint8 DamageTypeSpell		= 0xE7;
static const uint8 DamageTypeUnknown	= 0xFF;

/*
**	Skill damage types
**
**	(indexed by 'Skill' of SkillUseTypes)
*/
static const uint8 SkillDamageTypes[EQ::skills::HIGHEST_SKILL + 1] = // change to _SkillServerArraySize once activated
{
/*1HBlunt*/					0,
/*1HSlashing*/				1,
/*2HBlunt*/					0,
/*2HSlashing*/				1,
/*Abjuration*/				DamageTypeSpell,
/*Alteration*/				DamageTypeSpell,
/*ApplyPoison*/				DamageTypeUnknown,
/*Archery*/					7,
/*Backstab*/				8,
/*BindWound*/				DamageTypeUnknown,
/*Bash*/					10,
/*Block*/					DamageTypeUnknown,
/*BrassInstruments*/		DamageTypeSpell,
/*Channeling*/				DamageTypeUnknown,
/*Conjuration*/				DamageTypeSpell,
/*Defense*/					DamageTypeUnknown,
/*Disarm*/					DamageTypeUnknown,
/*DisarmTraps*/				DamageTypeUnknown,
/*Divination*/				DamageTypeSpell,
/*Dodge*/					DamageTypeUnknown,
/*DoubleAttack*/			DamageTypeUnknown,
/*DragonPunch*/				21,
/*DualWield*/				DamageTypeUnknown,
/*EagleStrike*/				23,
/*Evocation*/				DamageTypeSpell,
/*FeignDeath*/				4,
/*FlyingKick*/				30,
/*Forage*/					DamageTypeUnknown,
/*HandtoHand*/				4,
/*Hide*/					DamageTypeUnknown,
/*Kick*/					30,
/*Meditate*/				DamageTypeUnknown,
/*Mend*/					DamageTypeUnknown,
/*Offense*/					DamageTypeUnknown,
/*Parry*/					DamageTypeUnknown,
/*PickLock*/				DamageTypeUnknown,
/*1HPiercing*/				36,
/*Riposte*/					DamageTypeUnknown,
/*RoundKick*/				30,
/*SafeFall*/				DamageTypeUnknown,
/*SsenseHeading*/			DamageTypeUnknown,
/*Singing*/					DamageTypeSpell,
/*Sneak*/					DamageTypeUnknown,
/*SpecializeAbjure*/		DamageTypeUnknown,
/*SpecializeAlteration*/	DamageTypeUnknown,
/*SpecializeConjuration*/	DamageTypeUnknown,
/*SpecializeDivination*/	DamageTypeUnknown,
/*SpecializeEvocation*/		DamageTypeUnknown,
/*PickPockets*/				DamageTypeUnknown,
/*StringedInstruments*/		DamageTypeSpell,
/*Swimming*/				DamageTypeUnknown,
/*Throwing*/				51,
/*TigerClaw*/				23,
/*Tracking*/				DamageTypeUnknown,
/*WindInstruments*/			DamageTypeSpell,
/*Fishing*/					DamageTypeUnknown,
/*MakePoison*/				DamageTypeUnknown,
/*Tinkering*/				DamageTypeUnknown,
/*Research*/				DamageTypeUnknown,
/*Alchemy*/					DamageTypeUnknown,
/*Baking*/					DamageTypeUnknown,
/*Tailoring*/				DamageTypeUnknown,
/*SenseTraps*/				DamageTypeUnknown,
/*Blacksmithing*/			DamageTypeUnknown,
/*Fletching*/				DamageTypeUnknown,
/*Brewing*/					DamageTypeUnknown,
/*AlcoholTolerance*/		DamageTypeUnknown,
/*Begging*/					DamageTypeUnknown,
/*JewelryMaking*/			DamageTypeUnknown,
/*Pottery*/					DamageTypeUnknown,
/*PercussionInstruments*/	DamageTypeSpell,
/*Intimidation*/			30,
/*Berserking*/				DamageTypeUnknown,
/*Taunt*/					DamageTypeUnknown,
/*Frenzy*/					//74 //,
// /*RemoveTrap*/				DamageTypeUnknown,	// Needs research (set for SenseTrap value)
// /*TripleAttack*/			DamageTypeUnknown,	// Needs research (set for DoubleAttack value)
// /*2HPiercing*/				36					// Needs research (set for 1HPiercing value - similar to slash/blunt)
};

enum TextureTypes : uint8
{
	TextureCloth = 0,
	TextureLeather,
	TextureRingmail,
	TexturePlate,
	TextureSilk,
	TextureChitin,
	TextureUnknown1,
	TextureScale,
	TextureUnknown2,
	TextureUnknown3,
	TextureRedRobe,
	TextureElementRobe,
	TextureBlightedRobe,
	TextureCrystalRobe,
	TextureOracleRobe,
	TextureKedgeRobe,
	TextureMetallicRobe,
	TextureRobe,
	TextureVelLeather,
	TextureVelChain,
	TexturePogPlate,
	TextureUlthork,
	TextureRyGorr,
	TextureKael,
	_TextureTypesCount
};

#define INVALID_INDEX	-1

namespace EQ
{
	namespace legacy {
		enum InventorySlot {
			SLOT_CURSOR_END = (int16)0xFFFE, // I hope no one is using this...
			SLOT_TRADESKILL = 1000,
			SLOT_QUEST = 9999,
			//SLOT_INVALID = (int16)0xFFFF,
		};

	} // namespace legacy

}

//These are NPCIDs in the database. All of these boats send a BoardBoat opcode when boarded.
enum Boats
{
	Stormbreaker = 770, //freporte-oot-butcherblock
	SirensBane  = 771,
	Sea_King = 772, //erudext-erudsxing-qeynos
	Golden_Maiden  = 773,
	Maidens_Voyage  = 838, //timorous-firiona
	Bloated_Belly = 839, //timorous-overthere
	Barrel_Barge = 840, //Shuttle timorous-oasis
	Muckskimmer = 841,
	Sabrina = 24056, //Shuttle in Erud
	Island_Shuttle = 96075, //Shuttle to Elf docks in timorous
	Captains_Skiff = 842, //Shuttle timorous-butcherblock
	Icebreaker = 110083, //iceclad
	pirate_runners_skiff = 843 //Shuttle iceclad-nro
};

// Values are bitwise, so we can compare with the expansion field in account.
enum Expansions
{
	ClassicEQ = 0,
	KunarkEQ = 1,
	VeliousEQ = 2,
	LuclinEQ = 4,
	PlanesEQ = 8,
	AllEQ = 15
};

// Values are bitwise, so we can compare with the expansion field in account.
enum ExpansionEras : int
{
	AllEQEras = -1,
	ClassicEQEra = 0,
	KunarkEQEra = 1,
	VeliousEQEra = 2,
	LuclinEQEra = 3,
	PlanesEQEra = 4,
	PostPlanesEra = 5
};

static constexpr float AllEQErasFloat = -1.0f;
static constexpr float ClassicEQEraFloat = 0.0f;

static const uint32 MAX_SPELL_DB_ID_VAL = 65535;

enum ChatChannelNames : uint16
{
	ChatChannel_Guild = 0,
	ChatChannel_Group = 2,
	ChatChannel_Shout = 3,
	ChatChannel_Auction = 4,
	ChatChannel_OOC = 5,
	ChatChannel_Broadcast = 6,
	ChatChannel_Tell = 7,
	ChatChannel_Say = 8,
	ChatChannel_Petition = 10,
	ChatChannel_GMSAY = 11,
	ChatChannel_TellEcho = 14,
	ChatChannel_Raid = 15,

	ChatChannel_UNKNOWN_Guild = 17,
	ChatChannel_UNKNOWN_GMSAY = 18
};

#endif
