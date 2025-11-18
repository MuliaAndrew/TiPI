#include <windows.h> 
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include "logger.h"
#include "executor.h"

#define BUFSIZE 4096 

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

static PROCESS_INFORMATION CreateChild(void);
static void ErrorExit(PCTSTR);

//
HANDLES InitTerminal(void)
{
    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory(&saAttr, sizeof(SECURITY_ATTRIBUTES));

    // Parent prints for diagnostics
    printf("\n->Start of parent execution.\n");

    // Allow handle inheritance by child
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        ErrorExit(TEXT("StdoutRd CreatePipe"));

    // Ensure the read handle to the pipe for STDOUT is NOT inherited by the child.
    // Parent needs the read end; child needs the write end.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdout SetHandleInformation"));

    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
        ErrorExit(TEXT("Stdin CreatePipe"));

    // Ensure the write handle to the pipe for STDIN is NOT inherited by the child.
    // Parent writes to g_hChildStd_IN_Wr; child reads from g_hChildStd_IN_Rd.
    if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdin SetHandleInformation"));

    // Create the child process.
    PROCESS_INFORMATION pi = CreateChild();

    // If CreateChild succeeded, close handles that the parent does not need.
    // Parent keeps: g_hChildStd_OUT_Rd (to read child's stdout) and g_hChildStd_IN_Wr (to write to child's stdin).
    // Parent must close the write end of child's stdout and the read end of child's stdin (they were inherited by the child).
    if (pi.hProcess != NULL)
    {
        if (g_hChildStd_OUT_Wr && g_hChildStd_OUT_Wr != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_hChildStd_OUT_Wr);
            g_hChildStd_OUT_Wr = NULL;
        }
        if (g_hChildStd_IN_Rd && g_hChildStd_IN_Rd != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_hChildStd_IN_Rd);
            g_hChildStd_IN_Rd = NULL;
        }
    }
    else
    {
        // On failure, ensure we don't leak the created handles
        if (g_hChildStd_OUT_Wr) { CloseHandle(g_hChildStd_OUT_Wr); g_hChildStd_OUT_Wr = NULL; }
        if (g_hChildStd_IN_Rd)  { CloseHandle(g_hChildStd_IN_Rd);  g_hChildStd_IN_Rd  = NULL; }
        if (g_hChildStd_OUT_Rd) { /* keep for potential diagnostics/cleanup by caller */ }
        if (g_hChildStd_IN_Wr)  { /* keep for potential diagnostics/cleanup by caller */ }
    }

    HANDLES h = { 
        .hChildStd_IN_Wr = g_hChildStd_IN_Wr, 
        .hChildStd_OUT_Rd = g_hChildStd_OUT_Rd, 
        .ProcInfo = pi
    };

    return h;
}

static PROCESS_INFORMATION CreateChild(void)
// Create a child process that uses the previously created pipes for STDIN and STDOUT.
{
    // IMPORTANT:
    // - Use a correct cmd line. "cmd.exe -c" is wrong on Windows (causes immediate exit).
    // - Use a mutable buffer for CreateProcess as required.
    WCHAR usrCmd[1024];
    ZeroMemory(usrCmd, sizeof(usrCmd));
    // Start plain cmd.exe so it reads from redirected STDIN and writes to redirected STDOUT.
    // If you want the shell to run a specific command and exit, use "/c <command>".
    // For an interactive-like shell that keeps running, using "cmd.exe" (or "cmd.exe /Q") is appropriate.
    StringCchCopyW(usrCmd, _countof(usrCmd), L"cmd.exe");

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);

    // Redirect child's std handles to our pipe endpoints (child inherits the handles)
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Use CREATE_NO_WINDOW so the child doesn't create a visible console when running as a service.
    DWORD creationFlags = CREATE_NO_WINDOW;

    // Create the child process. lpCommandLine must be writable -> pass usrCmd (WCHAR array).
    bSuccess = CreateProcessW(
        NULL,
        usrCmd,        // command line (writable)
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        creationFlags, // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

    if (!bSuccess)
    {
        Log::Error("CreateProcess failed");
        // return zeroed PROCESS_INFORMATION to indicate failure
        ZeroMemory(&piProcInfo, sizeof(piProcInfo));
    }

    return piProcInfo;
}

static void ErrorExit(PCTSTR lpszFunction)

// Format a readable error message, display a message box, 
// and exit from the application.
{
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(1);
}