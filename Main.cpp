#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

// Force Exports from Outpost2.exe
#include "..\API\ForcedExports\ForcedExports.h"
using namespace OP2ForcedExport;

#include "OPUNetGameProtocol.h"


HINSTANCE hInstance;
OPUNetGameProtocol opuNetGameProtocol;


BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// Sanity check on the DLL load address
		if (hModule != desiredLoadAddress)
		{
			MessageBox(0, "DLL loaded to bad address", "Failed", 0);
			return FALSE;
		}
		// Don't need this, so don't waste the time
		DisableThreadLibraryCalls((HMODULE)hModule);
		// Store the module handle
		hInstance = (HINSTANCE)hModule;

		// **TODO** Set the button text, and settle on a button
		// Insert new multiplayer protocol type  (overwrite SIGS)   ** (overwrite Serial) **
		protocolList[3].netGameProtocol = &opuNetGameProtocol;

		break;
	case DLL_PROCESS_DETACH:
		break;
	}

    return TRUE;
}