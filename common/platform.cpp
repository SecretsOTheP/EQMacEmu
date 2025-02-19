#include "platform.h"

EQEmuExePlatform exe_platform = ExePlatformNone;

void RegisterExecutablePlatform(EQEmuExePlatform p) 
{
	exe_platform = p;
}

const EQEmuExePlatform& GetExecutablePlatform() 
{
	return exe_platform;
}

int GetExecutablePlatformInt()
{
	return exe_platform;
}

bool IsWorld()
{
	return exe_platform == EQEmuExePlatform::ExePlatformWorld;
}

bool IsQueryServ()
{
	return exe_platform == EQEmuExePlatform::ExePlatformQueryServ;
}

std::string GetPlatformName()
{
	switch (GetExecutablePlatformInt()) {
	case EQEmuExePlatform::ExePlatformWorld:
		return "World";
	case EQEmuExePlatform::ExePlatformQueryServ:
		return "QS";
	case EQEmuExePlatform::ExePlatformZone:
		return "Zone";
	case EQEmuExePlatform::ExePlatformUCS:
		return "UCS";
	case EQEmuExePlatform::ExePlatformLogin:
		return "Login";
	case EQEmuExePlatform::ExePlatformSharedMemory:
		return "SharedMemory";
	case EQEmuExePlatform::ExePlatformClientImport:
		return "ClientImport";
	case EQEmuExePlatform::ExePlatformClientExport:
		return "ClientExport";
	case EQEmuExePlatform::ExePlatformLaunch:
		return "Launch";
	case EQEmuExePlatform::ExePlatformTests:
		return "Tests";
	case EQEmuExePlatform::ExePlatformZoneSidecar:
		return "ZoneSidecar";
	default:
		return "";
	}
}