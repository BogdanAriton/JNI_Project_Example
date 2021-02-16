#include <Windows.h>
#include <stdio.h>

#define JavaJNITool_USE 1
#include "CrystalJavaJNITool.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

/* the only export of this DLL */
extern "C" __declspec(dllexport) LPVOID GetInstance()
{
	return static_cast<LPVOID> (JavaJNI::GetInstance());
}