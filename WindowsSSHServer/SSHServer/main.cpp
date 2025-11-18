#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include "logger.h"
#include "executor.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")

#define SVCNAME TEXT("SvcName")
#define SERVER_PORT 2222

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;

// TCP server globals
static HANDLE           ghServerThread = NULL;
static SOCKET           g_listenSocket = INVALID_SOCKET;

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler( DWORD ); 
VOID WINAPI SvcMain( DWORD, LPTSTR * ); 

VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * ); 
VOID SvcReportEvent( LPCTSTR );

// TCP server thread
DWORD WINAPI TcpServerThread(LPVOID lpParam)
{
    (void)lpParam;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        Log::Error("WSAStartup failed");
        return 1;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET)
    {
        Log::Error("Failed to create listen socket");
        WSACleanup();
        return 1;
    }

    // Bind
    sockaddr_in service;
    ZeroMemory(&service, sizeof(service));
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = htonl(INADDR_ANY);
    service.sin_port = htons(SERVER_PORT);

    if (bind(g_listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
    {
        Log::Error("Bind failed");
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return 1;
    }

    if (listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        Log::Error("Listen failed");
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return 1;
    }

    {
        char buf[128];
        sprintf_s(buf, "Service TCP server listening on port %d", SERVER_PORT);
        Log::Info(buf);
    }

    SOCKET client = INVALID_SOCKET;

    // Accept loop: wait up to 1 second for a new connection
    while (WaitForSingleObject(ghSvcStopEvent, 0) == WAIT_TIMEOUT)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_listenSocket, &readfds);
        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int rc = select(0, &readfds, NULL, NULL, &tv);
        if (rc == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            char buf[128];
            sprintf_s(buf, "select() failed on listen socket (%d)", err);
            Log::Error(buf);
            break;
        }
        else if (rc == 0)
        {
            // timeout, continue to check stop event
            continue;
        }

        if (FD_ISSET(g_listenSocket, &readfds))
        {
            client = accept(g_listenSocket, NULL, NULL);
            if (client == INVALID_SOCKET)
            {
                int err = WSAGetLastError();
                // If interrupted or would block, just continue and wait again
                if (err == WSAEINTR || err == WSAEWOULDBLOCK)
                    continue;

                char buf[128];
                sprintf_s(buf, "accept() failed (%d)", err);
                Log::Error(buf);
                // on fatal accept error, break the loop
                break;
            }

            Log::Info("Client connected");
            // break out to start multiplexing loop with this client
            break;
        }
    }

    // If we didn't get a client, perform cleanup and exit
    if (client == INVALID_SOCKET)
    {
        if (g_listenSocket != INVALID_SOCKET)
        {
            closesocket(g_listenSocket);
            g_listenSocket = INVALID_SOCKET;
        }
        WSACleanup();
        Log::Info("TCP server thread exiting (no client)");
        return 0;
    }

    HANDLES h = InitTerminal();
    HANDLE ChildStdin = h.hChildStd_IN_Wr;   // write end -> child stdin
    HANDLE ChildStdout = h.hChildStd_OUT_Rd; // read end  -> child stdout
	PROCESS_INFORMATION pi = h.ProcInfo;

    // Make client socket blocking for simple send/recv semantics
    u_long mode = 0;
    ioctlsocket(client, FIONBIO, &mode);

    char rbuf[2048];

    // Main loop: multiplex stop event, child stdout pipe, and client socket
    while (true)
    {
        // 1) Check service stop event
        if (WaitForSingleObject(ghSvcStopEvent, 0) != WAIT_TIMEOUT)
            break;

        // 2) Use select on client socket with 1s timeout
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(client, &rfds);
        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int sel = select(0, &rfds, NULL, NULL, &tv);
        if (sel == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            char buf[128];
            sprintf_s(buf, "select() failed on client socket (%d)", err);
            Log::Error(buf);
            break;
        }

        // 3) Check child stdout readiness using PeekNamedPipe
        bool child_stdout_ready = false;
        DWORD avail = 0;
        if (ChildStdout != NULL && ChildStdout != INVALID_HANDLE_VALUE)
        {
            if (PeekNamedPipe(ChildStdout, NULL, 0, NULL, &avail, NULL) && avail > 0)
                child_stdout_ready = true;
        }

        // If child stdout has data, read and forward to client
        if (child_stdout_ready)
        {
            DWORD bytesRead = 0;
            if (ReadFile(ChildStdout, rbuf, (DWORD)sizeof(rbuf), &bytesRead, NULL) && bytesRead > 0)
            {
                // Log and send to client (may require loop for partial sends)
                // Null-terminate for logging (ensure space)
                size_t logLen = (bytesRead < (sizeof(rbuf) - 1)) ? bytesRead : (sizeof(rbuf) - 1);
                rbuf[logLen] = '\0';
                Log::Info(rbuf);

                int totalSent = 0;
                while (totalSent < (int)bytesRead)
                {
                    int sent = send(client, rbuf + totalSent, (int)bytesRead - totalSent, 0);
                    if (sent == SOCKET_ERROR)
                    {
                        int serr = WSAGetLastError();
                        char eb[128];
                        sprintf_s(eb, "send() failed when writing child stdout to client (%d)", serr);
                        Log::Warning(eb);

                        // If connection reset or broken, break main loop
                        if (serr == WSAECONNRESET || serr == WSAENOTCONN || serr == WSAESHUTDOWN)
                        {
                            goto cleanup;
                        }
                        // otherwise attempt to retry shortly
                        Sleep(10);
                        continue;
                    }
                    totalSent += sent;
                }
            }
            else
            {
                // ReadFile failed or pipe closed
                DWORD err = GetLastError();
                if (err == ERROR_BROKEN_PIPE)
                {
                    Log::Info("Child stdout pipe closed");
                    // No more child output; decide to continue or break. We'll break.
                    break;
                }
                else
                {
                    char eb[128];
                    sprintf_s(eb, "ReadFile(ChildStdout) failed (%lu)", (unsigned long)err);
                    Log::Warning(eb);
                }
            }
        }

        // If client socket is readable, receive data and forward to child stdin
        if (sel > 0 && FD_ISSET(client, &rfds))
        {
            int recvd = recv(client, rbuf, (int)sizeof(rbuf) - 1, 0);
            if (recvd == 0)
            {
                Log::Info("Client closed connection");
                break;
            }
            else if (recvd == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                // If would block or interrupted, continue; otherwise treat as error
                if (err == WSAEWOULDBLOCK || err == WSAEINTR)
                    continue;
                char buf[128];
                sprintf_s(buf, "recv() failed (%d)", err);
                Log::Error(buf);
                break;
            }
            else
            {
                // Received bytes from client: forward to child stdin
                int toWrite = recvd;
                int writtenTotal = 0;
                // For logging, null-terminate a copy
                rbuf[recvd] = '\0';
                const char* fmt = "recv command: %s";
                char* buf = (char*)calloc(strlen(fmt) + recvd + 1, 1);
                sprintf_s(buf, strlen(fmt) + recvd, "recv command: %s", rbuf);
                Log::Info(buf);
                free(buf);

                while (writtenTotal < toWrite)
                {
                    DWORD bytesWritten = 0;
                    BOOL ok = WriteFile(ChildStdin, rbuf + writtenTotal, (DWORD)(toWrite - writtenTotal), &bytesWritten, NULL);
                    if (!ok)
                    {
                        DWORD err = GetLastError();
                        if (err == ERROR_NO_DATA || err == ERROR_BROKEN_PIPE)
                        {
                            Log::Warning("Child stdin pipe closed (cannot write)");
                            // child won't receive more input -> break main loop
                            goto cleanup;
                        }
                        else
                        {
                            char eb[128];
                            sprintf_s(eb, "WriteFile(ChildStdin) failed (%lu)", (unsigned long)err);
                            Log::Warning(eb);
                            // try again briefly
                            Sleep(10);
                            continue;
                        }
                    }
                    if (bytesWritten == 0)
                    {
                        // nothing written; avoid busy loop
                        Sleep(10);
						Log::Warning("No bytes written to child stdin; retrying");
                        continue;
                    }
                    writtenTotal += (int)bytesWritten;
                }
            }
        } // end client readable

        // Loop continues, checking stop event each iteration
    } // end multiplex loop

cleanup:
    // Close child handles created by InitTerminal if still open
    if (ChildStdin && ChildStdin != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ChildStdin);
        ChildStdin = NULL;
    }
    if (ChildStdout && ChildStdout != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ChildStdout);
        ChildStdout = NULL;
    }

    // Close client
    if (client != INVALID_SOCKET)
    {
        closesocket(client);
        client = INVALID_SOCKET;
    }

    // Cleanup listen socket (service is still running but we accepted one client)
    if (g_listenSocket != INVALID_SOCKET)
    {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }

    WSACleanup();
    Log::Info("TCP server thread exiting");
    return 0;
}

//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None, defaults to 0 (zero)
//
int __cdecl _tmain(int argc, TCHAR *argv[]) 
{
    Log::Initialize(Log::Level::LOG_LEVEL_INFO, "MysshServer.log");
    if (argc > 1 && lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }

    SERVICE_TABLE_ENTRY DispatchTable[] = 
    { 
        { (LPTSTR)SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
        { NULL, NULL } 
    }; 

    if (!StartServiceCtrlDispatcher( DispatchTable )) 
    { 
        DWORD err = GetLastError();
        char buf[256];
        sprintf_s(buf, "StartServiceCtrlDispatcher failed with %lu", (unsigned long)err);
        Log::Error(buf);
    } 

    return 0;
} 

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szUnquotedPath[MAX_PATH];

    if( !GetModuleFileName( NULL, szUnquotedPath, MAX_PATH ) )
    {
        Log::Error("GetModuleFileName failed");
        return;
    }

    // In case the path contains a space, it must be quoted so that
    // it is correctly interpreted. For example,
    // "d:\my share\myservice.exe" should be specified as
    // ""d:\my share\myservice.exe"". 
    TCHAR szPath[MAX_PATH];
    StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        Log::Error("OpenSCManager failed");
        return;
    }

    // Create the service

    schService = CreateService( 
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 
 
    if (schService == NULL) 
    {
        char buf[256];
        sprintf_s(buf, "CreateService failed (%lu)", (unsigned long)GetLastError());
        Log::Error(buf);
        CloseServiceHandle(schSCManager);
        return;
    }
    else Log::Info("Service installed successfully");

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain( DWORD Argc, LPTSTR *Argv )
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler( 
        SVCNAME, 
        SvcCtrlHandler);

    if( !gSvcStatusHandle )
    { 
        DWORD err = GetLastError();
        char buf[256];
        sprintf_s(buf, "RegisterServiceCtrlHandler failed with %lu", (unsigned long)err);
        Log::Error(buf);
        return; 
    } 

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    gSvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM

    ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );

    // Perform service-specific initialization and work.

    SvcInit( Argc, Argv );
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit( DWORD dwArgc, LPTSTR *lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEvent(
                         NULL,    // default security attributes
                         TRUE,    // manual reset event
                         FALSE,   // not signaled
                         NULL);   // no name

    if ( ghSvcStopEvent == NULL)
    {
        ReportSvcStatus( SERVICE_STOPPED, GetLastError(), 0 );
        return;
    }

    // Start TCP server thread
    ghServerThread = CreateThread(NULL, 0, TcpServerThread, NULL, 0, NULL);
    if (ghServerThread == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );

    // Wait until stop requested
    WaitForSingleObject(ghSvcStopEvent, INFINITE);

    // Stop requested: wait for server thread to finish
    if (ghServerThread)
    {
        // Signal is already set by SvcCtrlHandler; thread checks ghSvcStopEvent.
        WaitForSingleObject(ghServerThread, INFINITE);
        CloseHandle(ghServerThread);
        ghServerThread = NULL;
    }

    // Ensure listen socket closed if still open
    if (g_listenSocket != INVALID_SOCKET)
    {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }

    ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
}

VOID ReportSvcStatus( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else
        gSvcStatus.dwCheckPoint = dwCheckPoint++;

    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

VOID WINAPI SvcCtrlHandler( DWORD dwCtrl )
{
   switch(dwCtrl) 
   {  
      case SERVICE_CONTROL_STOP: 
         ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

         SetEvent(ghSvcStopEvent);

         // Also shutdown the listen socket to unblock select/accept quickly
         if (g_listenSocket != INVALID_SOCKET)
         {
             shutdown(g_listenSocket, SD_BOTH);
             closesocket(g_listenSocket);
             g_listenSocket = INVALID_SOCKET;
         }

         ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
         return;
 
      case SERVICE_CONTROL_INTERROGATE: 
         break; 
 
      default: 
         break;
   } 
   
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(const LPCTSTR szFunction) 
{ 
    // Forward important failures to the logger (convert TCHAR -> ANSI).
    DWORD err = GetLastError();
    char fnBuf[512] = "(unknown)";
    #ifdef UNICODE
    WideCharToMultiByte(CP_ACP, 0, szFunction, -1, fnBuf, (int)sizeof(fnBuf), NULL, NULL);
    #else
    strncpy_s(fnBuf, szFunction, _TRUNCATE);
    #endif

    char buf[1024];
    sprintf_s(buf, "%s failed with %lu", fnBuf, (unsigned long)err);
    Log::Error(buf);
}