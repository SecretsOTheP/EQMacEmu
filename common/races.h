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
#ifndef RACES_H
#define RACES_H
#include "../common/types.h"
#include <string>

namespace Gender {
	constexpr uint8 Male = 0;
	constexpr uint8 Female = 1;
	constexpr uint8 Neuter = 2;
}

//theres a big list straight from the client below.

#define HUMAN				1
#define BARBARIAN			2
#define ERUDITE				3
#define WOOD_ELF			4
#define HIGH_ELF			5
#define DARK_ELF			6
#define HALF_ELF			7
#define DWARF				8
#define TROLL				9
#define OGRE				10
#define HALFLING			11
#define GNOME				12
#define WEREWOLF			14
#define BROWNIE				15
#define FAIRY				25
#define FUNGUSMAN			28
#define WOLF				42
#define BEAR				43
#define FREEPORT_GUARD		44
#define KOBOLD				48
#define LAVA_DRAGON			49
#define LION				50
#define MIMIC				52
#define HUMAN_BEGGER		55
#define PIXIE				56
#define DRACNID				57
#define SKELETON			60
#define TIGER				63
#define VAMPIRE				65
#define HIGHPASS_CITIZEN	67
#define WISP				69
#define ZOMBIE				70
#define	SHIP				72
#define LAUNCH				73
#define FROGLOK				74
#define ELEMENTAL			75
#define NERIAK_CITIZEN		77
#define ERUDITE_CITIZEN		78
#define BIXIE				79
#define RIVERVALE_CITIZEN	81
#define CLOCKWORK_GNOME		88
#define HALAS_CITIZEN		90
#define ALLIGATOR			91
#define GROBB_CITIZEN		92
#define OGGOK_CITIZEN		93
#define KALADIM_CITIZEN		94
#define ELF_VAMPIRE			98
#define FELGUARD			106
#define EYE_OF_ZOMM			108
#define FAYGUARD			112
#define GHOST_SHIP			114
#define DWARF_GHOST			117
#define ERUDITE_GHOST		118
#define WOLF_ELEMENTAL		120
#define INVISIBLE_MAN		127
#define IKSAR				128
#define VAHSHIR				130
#define CONTROLLED_BOAT		141
#define MINOR_ILLUSION		142
#define TREEFORM			143
#define GOO					145
#define WURM				158
#define IKSAR_SKELETON		161
#define VELIOUS_DRAGON		184
#define GHOST_DRAGON		196
#define PRISMATIC_DRAGON	198
#define EARTH_ELEMENTAL		209
#define AIR_ELEMENTAL		210
#define WATER_ELEMENTAL		211
#define FIRE_ELEMENTAL		212
#define HORSE				216
#define TELEPORT_MAN		240
#define MITHANIEL_MARR		296
#define GUKTAN              330
#define EMU_RACE_NPC		131069 // was 65533
#define EMU_RACE_PET		131070 // was 65534
#define EMU_RACE_UNKNOWN	131071 // was 65535

// player race values
#define PLAYER_RACE_UNKNOWN		0
#define PLAYER_RACE_HUMAN		1
#define PLAYER_RACE_BARBARIAN	2
#define PLAYER_RACE_ERUDITE		3
#define PLAYER_RACE_WOOD_ELF	4
#define PLAYER_RACE_HIGH_ELF	5
#define PLAYER_RACE_DARK_ELF	6
#define PLAYER_RACE_HALF_ELF	7
#define PLAYER_RACE_DWARF		8
#define PLAYER_RACE_TROLL		9
#define PLAYER_RACE_OGRE		10
#define PLAYER_RACE_HALFLING	11
#define PLAYER_RACE_GNOME		12
#define PLAYER_RACE_IKSAR		13
#define PLAYER_RACE_VAHSHIR		14

#define PLAYER_RACE_COUNT		14

#define PLAYER_RACE_EMU_NPC		15
#define PLAYER_RACE_EMU_PET		16
#define PLAYER_RACE_EMU_COUNT	17

// player race bits
#define PLAYER_RACE_UNKNOWN_BIT		0
#define PLAYER_RACE_HUMAN_BIT		1
#define PLAYER_RACE_BARBARIAN_BIT	2
#define PLAYER_RACE_ERUDITE_BIT		4
#define PLAYER_RACE_WOOD_ELF_BIT	8
#define PLAYER_RACE_HIGH_ELF_BIT	16
#define PLAYER_RACE_DARK_ELF_BIT	32
#define PLAYER_RACE_HALF_ELF_BIT	64
#define PLAYER_RACE_DWARF_BIT		128
#define PLAYER_RACE_TROLL_BIT		256
#define PLAYER_RACE_OGRE_BIT		512
#define PLAYER_RACE_HALFLING_BIT	1024
#define PLAYER_RACE_GNOME_BIT		2048
#define PLAYER_RACE_IKSAR_BIT		4096
#define PLAYER_RACE_VAHSHIR_BIT		8192

#define PLAYER_RACE_ALL_MASK		16383

const char* GetRaceIDName(uint16 race_id);
const char* GetRaceIDNamePlural(uint16 race_id);
const char* GetPlayerRaceName(uint32 player_race_value);
const char* GetGenderName(uint32 gender_id);
const bool IsFlyingCreatureRace(const uint16 race_id);

bool IsPlayerRace(uint16 race_id);
const std::string GetPlayerRaceAbbreviation(uint16 race_id);

uint32 GetPlayerRaceValue(uint16 race_id);
uint32 GetPlayerRaceBit(uint16 race_id);

uint16 GetRaceIDFromPlayerRaceValue(uint32 player_race_value);
uint16 GetRaceIDFromPlayerRaceBit(uint32 player_race_bit);

float GetRaceGenderDefaultHeight(int race, int gender);

// player race-/gender-based model feature validators
namespace PlayerAppearance
{
	bool IsValidBeard(uint16 race_id, uint8 gender_id, uint8 beard_value, bool use_luclin = true);
	bool IsValidBeardColor(uint16 race_id, uint8 gender_id, uint8 beard_color_value, bool use_luclin = true);
	bool IsValidEyeColor(uint16 race_id, uint8 gender_id, uint8 eye_color_value, bool use_luclin = true);
	bool IsValidFace(uint16 race_id, uint8 gender_id, uint8 face_value, bool use_luclin = true);
	bool IsValidHair(uint16 race_id, uint8 gender_id, uint8 hair_value, bool use_luclin = true);
	bool IsValidHairColor(uint16 race_id, uint8 gender_id, uint8 hair_color_value, bool use_luclin = true);
	bool IsValidHead(uint16 race_id, uint8 gender_id, uint8 head_value, bool use_luclin = true);
	bool IsValidTexture(uint16 race_id, uint8 gender_id, uint8 texture_value, bool use_luclin = true);
	bool IsValidWoad(uint16 race_id, uint8 gender_id, uint8 woad_value, bool use_luclin = true);
}

namespace Race {
	constexpr uint16 Doug = 0;
	constexpr uint16 Human = 1;
	constexpr uint16 Barbarian = 2;
	constexpr uint16 Erudite = 3;
	constexpr uint16 WoodElf = 4;
	constexpr uint16 HighElf = 5;
	constexpr uint16 DarkElf = 6;
	constexpr uint16 HalfElf = 7;
	constexpr uint16 Dwarf = 8;
	constexpr uint16 Troll = 9;
	constexpr uint16 Ogre = 10;
	constexpr uint16 Halfling = 11;
	constexpr uint16 Gnome = 12;
	constexpr uint16 Aviak = 13;
	constexpr uint16 Werewolf = 14;
	constexpr uint16 Brownie = 15;
	constexpr uint16 Centaur = 16;
	constexpr uint16 Golem = 17;
	constexpr uint16 Giant = 18;
	constexpr uint16 Trakanon = 19;
	constexpr uint16 VenrilSathir = 20;
	constexpr uint16 EvilEye = 21;
	constexpr uint16 Beetle = 22;
	constexpr uint16 Kerran = 23;
	constexpr uint16 Fish = 24;
	constexpr uint16 Fairy = 25;
	constexpr uint16 Froglok = 26;
	constexpr uint16 FroglokGhoul = 27;
	constexpr uint16 Fungusman = 28;
	constexpr uint16 Gargoyle = 29;
	constexpr uint16 Gasbag = 30;
	constexpr uint16 GelatinousCube = 31;
	constexpr uint16 Ghost = 32;
	constexpr uint16 Ghoul = 33;
	constexpr uint16 GiantBat = 34;
	constexpr uint16 GiantEel = 35;
	constexpr uint16 GiantRat = 36;
	constexpr uint16 GiantSnake = 37;
	constexpr uint16 GiantSpider = 38;
	constexpr uint16 Gnoll = 39;
	constexpr uint16 Goblin = 40;
	constexpr uint16 Gorilla = 41;
	constexpr uint16 Wolf = 42;
	constexpr uint16 Bear = 43;
	constexpr uint16 FreeportGuard = 44;
	constexpr uint16 DemiLich = 45;
	constexpr uint16 Imp = 46;
	constexpr uint16 Griffin = 47;
	constexpr uint16 Kobold = 48;
	constexpr uint16 LavaDragon = 49;
	constexpr uint16 Lion = 50;
	constexpr uint16 LizardMan = 51;
	constexpr uint16 Mimic = 52;
	constexpr uint16 Minotaur = 53;
	constexpr uint16 Orc = 54;
	constexpr uint16 HumanBeggar = 55;
	constexpr uint16 Pixie = 56;
	constexpr uint16 Drachnid = 57;
	constexpr uint16 SolusekRo = 58;
	constexpr uint16 Bloodgill = 59;
	constexpr uint16 Skeleton = 60;
	constexpr uint16 Shark = 61;
	constexpr uint16 Tunare = 62;
	constexpr uint16 Tiger = 63;
	constexpr uint16 Treant = 64;
	constexpr uint16 Vampire = 65;
	constexpr uint16 StatueOfRallosZek = 66;
	constexpr uint16 HighpassCitizen = 67;
	constexpr uint16 TentacleTerror = 68;
	constexpr uint16 Wisp = 69;
	constexpr uint16 Zombie = 70;
	constexpr uint16 QeynosCitizen = 71;
	constexpr uint16 Ship = 72;
	constexpr uint16 Launch = 73;
	constexpr uint16 Piranha = 74;
	constexpr uint16 Elemental = 75;
	constexpr uint16 Puma = 76;
	constexpr uint16 NeriakCitizen = 77;
	constexpr uint16 EruditeCitizen = 78;
	constexpr uint16 Bixie = 79;
	constexpr uint16 ReanimatedHand = 80;
	constexpr uint16 RivervaleCitizen = 81;
	constexpr uint16 Scarecrow = 82;
	constexpr uint16 Skunk = 83;
	constexpr uint16 SnakeElemental = 84;
	constexpr uint16 Spectre = 85;
	constexpr uint16 Sphinx = 86;
	constexpr uint16 Armadillo = 87;
	constexpr uint16 ClockworkGnome = 88;
	constexpr uint16 Drake = 89;
	constexpr uint16 HalasCitizen = 90;
	constexpr uint16 Alligator = 91;
	constexpr uint16 GrobbCitizen = 92;
	constexpr uint16 OggokCitizen = 93;
	constexpr uint16 KaladimCitizen = 94;
	constexpr uint16 CazicThule = 95;
	constexpr uint16 Cockatrice = 96;
	constexpr uint16 DaisyMan = 97;
	constexpr uint16 ElfVampire = 98;
	constexpr uint16 Denizen = 99;
	constexpr uint16 Dervish = 100;
	constexpr uint16 Efreeti = 101;
	constexpr uint16 FroglokTadpole = 102;
	constexpr uint16 PhinigelAutropos = 103;
	constexpr uint16 Leech = 104;
	constexpr uint16 Swordfish = 105;
	constexpr uint16 Felguard = 106;
	constexpr uint16 Mammoth = 107;
	constexpr uint16 EyeOfZomm = 108;
	constexpr uint16 Wasp = 109;
	constexpr uint16 Mermaid = 110;
	constexpr uint16 Harpy = 111;
	constexpr uint16 Fayguard = 112;
	constexpr uint16 Drixie = 113;
	constexpr uint16 GhostShip = 114;
	constexpr uint16 Clam = 115;
	constexpr uint16 SeaHorse = 116;
	constexpr uint16 DwarfGhost = 117;
	constexpr uint16 EruditeGhost = 118;
	constexpr uint16 Sabertooth = 119;
	constexpr uint16 WolfElemental = 120;
	constexpr uint16 Gorgon = 121;
	constexpr uint16 DragonSkeleton = 122;
	constexpr uint16 Innoruuk = 123;
	constexpr uint16 Unicorn = 124;
	constexpr uint16 Pegasus = 125;
	constexpr uint16 Djinn = 126;
	constexpr uint16 InvisibleMan = 127;
	constexpr uint16 Iksar = 128;
	constexpr uint16 Scorpion = 129;
	constexpr uint16 VahShir = 130;
	constexpr uint16 Sarnak = 131;
	constexpr uint16 Draglock = 132;
	constexpr uint16 Drolvarg = 133;
	constexpr uint16 Mosquito = 134;
	constexpr uint16 Rhinoceros = 135;
	constexpr uint16 Xalgoz = 136;
	constexpr uint16 KunarkGoblin = 137;
	constexpr uint16 Yeti = 138;
	constexpr uint16 IksarCitizen = 139;
	constexpr uint16 ForestGiant = 140;
	constexpr uint16 Boat = 141;
	constexpr uint16 MinorIllusion = 142;
	constexpr uint16 Tree = 143;
	constexpr uint16 Burynai = 144;
	constexpr uint16 Goo = 145;
	constexpr uint16 SarnakSpirit = 146;
	constexpr uint16 IksarSpirit = 147;
	constexpr uint16 KunarkFish = 148;
	constexpr uint16 IksarScorpion = 149;
	constexpr uint16 Erollisi = 150;
	constexpr uint16 Tribunal = 151;
	constexpr uint16 Bertoxxulous = 152;
	constexpr uint16 Bristlebane = 153;
	constexpr uint16 FayDrake = 154;
	constexpr uint16 UndeadSarnak = 155;
	constexpr uint16 Ratman = 156;
	constexpr uint16 Wyvern = 157;
	constexpr uint16 Wurm = 158;
	constexpr uint16 Devourer = 159;
	constexpr uint16 IksarGolem = 160;
	constexpr uint16 UndeadIksar = 161;
	constexpr uint16 ManEatingPlant = 162;
	constexpr uint16 Raptor = 163;
	constexpr uint16 SarnakGolem = 164;
	constexpr uint16 WaterDragon = 165;
	constexpr uint16 AnimatedHand = 166;
	constexpr uint16 Succulent = 167;
	constexpr uint16 Holgresh = 168;
	constexpr uint16 Brontotherium = 169;
	constexpr uint16 SnowDervish = 170;
	constexpr uint16 DireWolf = 171;
	constexpr uint16 Manticore = 172;
	constexpr uint16 Totem = 173;
	constexpr uint16 IceSpectre = 174;
	constexpr uint16 EnchantedArmor = 175;
	constexpr uint16 SnowRabbit = 176;
	constexpr uint16 Walrus = 177;
	constexpr uint16 Geonid = 178;
	constexpr uint16 Unknown = 179;
	constexpr uint16 Unknown2 = 180;
	constexpr uint16 Yakkar = 181;
	constexpr uint16 Faun = 182;
	constexpr uint16 Coldain = 183;
	constexpr uint16 VeliousDragon = 184;
	constexpr uint16 Hag = 185;
	constexpr uint16 Hippogriff = 186;
	constexpr uint16 Siren = 187;
	constexpr uint16 FrostGiant = 188;
	constexpr uint16 StormGiant = 189;
	constexpr uint16 Othmir = 190;
	constexpr uint16 Ulthork = 191;
	constexpr uint16 ClockworkDragon = 192;
	constexpr uint16 Abhorrent = 193;
	constexpr uint16 SeaTurtle = 194;
	constexpr uint16 BlackAndWhiteDragon = 195;
	constexpr uint16 GhostDragon = 196;
	constexpr uint16 RonnieTest = 197;
	constexpr uint16 PrismaticDragon = 198;
	constexpr uint16 Shiknar = 199;
	constexpr uint16 Rockhopper = 200;
	constexpr uint16 Underbulk = 201;
	constexpr uint16 Grimling = 202;
	constexpr uint16 Worm = 203;
	constexpr uint16 EvanTest = 204;
	constexpr uint16 KhatiSha = 205;
	constexpr uint16 Owlbear = 206;
	constexpr uint16 RhinoBeetle = 207;
	constexpr uint16 Vampire2 = 208;
	constexpr uint16 EarthElemental = 209;
	constexpr uint16 AirElemental = 210;
	constexpr uint16 WaterElemental = 211;
	constexpr uint16 FireElemental = 212;
	constexpr uint16 WetfangMinnow = 213;
	constexpr uint16 ThoughtHorror = 214;
	constexpr uint16 Tegi = 215;
	constexpr uint16 Horse = 216;
	constexpr uint16 Shissar = 217;
	constexpr uint16 FungalFiend = 218;
	constexpr uint16 VampireVolatalis = 219;
	constexpr uint16 Stonegrabber = 220;
	constexpr uint16 ScarletCheetah = 221;
	constexpr uint16 Zelniak = 222;
	constexpr uint16 Lightcrawler = 223;
	constexpr uint16 Shade = 224;
	constexpr uint16 Sunflower = 225;
	constexpr uint16 Shadel = 226;
	constexpr uint16 Shrieker = 227;
	constexpr uint16 Galorian = 228;
	constexpr uint16 Netherbian = 229;
	constexpr uint16 Akhevan = 230;
	constexpr uint16 GriegVeneficus = 231;
	constexpr uint16 SonicWolf = 232;
	constexpr uint16 GroundShaker = 233;
	constexpr uint16 VahShirSkeleton = 234;
	constexpr uint16 Wretch = 235;
	constexpr uint16 LordInquisitorSeru = 236;
	constexpr uint16 Recuso = 237;
	constexpr uint16 VahShirKing = 238;
	constexpr uint16 VahShirGuard = 239;
	constexpr uint16 TeleportMan = 240;
	constexpr uint16 Werewolf2 = 241;
	constexpr uint16 Nymph = 242;
	constexpr uint16 Dryad = 243;
	constexpr uint16 Treant2 = 244;
	constexpr uint16 Fly = 245;
	constexpr uint16 TarewMarr = 246;
	constexpr uint16 SolusekRo2 = 247;
	constexpr uint16 ClockworkGolem = 248;
	constexpr uint16 ClockworkBrain = 249;
	constexpr uint16 Banshee = 250;
	constexpr uint16 GuardOfJustice = 251;
	constexpr uint16 MiniPom = 252;
	constexpr uint16 DiseasedFiend = 253;
	constexpr uint16 SolusekRoGuard = 254;
	constexpr uint16 BertoxxulousNew = 255;
	constexpr uint16 TribunalNew = 256;
	constexpr uint16 TerrisThule = 257;
	constexpr uint16 Vegerog = 258;
	constexpr uint16 Crocodile = 259;
	constexpr uint16 Bat = 260;
	constexpr uint16 Hraquis = 261;
	constexpr uint16 Tranquilion = 262;
	constexpr uint16 TinSoldier = 263;
	constexpr uint16 NightmareWraith = 264;
	constexpr uint16 Malarian = 265;
	constexpr uint16 KnightOfPestilence = 266;
	constexpr uint16 Lepertoloth = 267;
	constexpr uint16 Bubonian = 268;
	constexpr uint16 BubonianUnderling = 269;
	constexpr uint16 Pusling = 270;
	constexpr uint16 WaterMephit = 271;
	constexpr uint16 Stormrider = 272;
	constexpr uint16 JunkBeast = 273;
	constexpr uint16 BrokenClockwork = 274;
	constexpr uint16 GiantClockwork = 275;
	constexpr uint16 ClockworkBeetle = 276;
	constexpr uint16 NightmareGoblin = 277;
	constexpr uint16 Karana = 278;
	constexpr uint16 BloodRaven = 279;
	constexpr uint16 NightmareGargoyle = 280;
	constexpr uint16 MouthOfInsanity = 281;
	constexpr uint16 SkeletalHorse = 282;
	constexpr uint16 Saryrn = 283;
	constexpr uint16 FenninRo = 284;
	constexpr uint16 Tormentor = 285;
	constexpr uint16 SoulDevourer = 286;
	constexpr uint16 Nightmare = 287;
	constexpr uint16 NewRallosZek = 288;
	constexpr uint16 VallonZek = 289;
	constexpr uint16 TallonZek = 290;
	constexpr uint16 AirMephit = 291;
	constexpr uint16 EarthMephit = 292;
	constexpr uint16 FireMephit = 293;
	constexpr uint16 NightmareMephit = 294;
	constexpr uint16 Zebuxoruk = 295;
	constexpr uint16 MithanielMarr = 296;
	constexpr uint16 UndeadKnight = 297;
	constexpr uint16 Rathe = 298;
	constexpr uint16 Xegony = 299;
	constexpr uint16 Fiend = 300;
	constexpr uint16 TestObject = 301;
	constexpr uint16 Crab = 302;
	constexpr uint16 Phoenix = 303;
	constexpr uint16 Quarm = 304;
	constexpr uint16 Bear2 = 305;
	constexpr uint16 EarthGolem = 306;
	constexpr uint16 IronGolem = 307;
	constexpr uint16 StormGolem = 308;
	constexpr uint16 AirGolem = 309;
	constexpr uint16 WoodGolem = 310;
	constexpr uint16 FireGolem = 311;
	constexpr uint16 WaterGolem = 312;
	constexpr uint16 WarWraith = 313;
	constexpr uint16 Wrulon = 314;
	constexpr uint16 Kraken = 315;
	constexpr uint16 PoisonFrog = 316;
	constexpr uint16 Nilborien = 317;
	constexpr uint16 Valorian = 318;
	constexpr uint16 WarBoar = 319;
	constexpr uint16 Efreeti2 = 320;
	constexpr uint16 WarBoar2 = 321;
	constexpr uint16 Valorian2 = 322;
	constexpr uint16 AnimatedArmor = 323;
	constexpr uint16 UndeadFootman = 324;
	constexpr uint16 RallosOgre = 325;
	constexpr uint16 Arachnid = 326;
	constexpr uint16 CrystalSpider = 327;
	constexpr uint16 ZebuxoruksCage = 328;
	constexpr uint16 Portal = 329;
	constexpr uint16 Guktan = 330;

	constexpr uint16 ALL_RACES_BITMASK = 16383;
}

#endif
