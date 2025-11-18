#pragma once
#include <windows.h>

struct HANDLES
{
    HANDLE hChildStd_IN_Wr;
    HANDLE hChildStd_OUT_Rd;
	PROCESS_INFORMATION ProcInfo;
};

//
HANDLES InitTerminal(void);