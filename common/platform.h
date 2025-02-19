#ifndef EQEMU_PLATFORM_H
#define EQEMU_PLATFORM_H

#include "iostream"

enum EQEmuExePlatform
{
	ExePlatformNone = 0,
	ExePlatformZone,
	ExePlatformWorld,
	ExePlatformLogin,
	ExePlatformQueryServ,
	ExePlatformWebInterface,
	ExePlatformUCS,
	ExePlatformLaunch,
	ExePlatformSharedMemory,
	ExePlatformClientImport,
	ExePlatformClientExport,
	ExePlatformTests,
	ExePlatformZoneSidecar
};

void RegisterExecutablePlatform(EQEmuExePlatform p);
const EQEmuExePlatform& GetExecutablePlatform();
int GetExecutablePlatformInt();
std::string GetPlatformName();
bool IsWorld();
bool IsQueryServ();

#endif
