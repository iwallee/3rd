// Disasm.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

extern "C" {
#include "disasm/libdis.h"
static char __init = (char)(x86_init(opt_none, NULL), 1);
};

