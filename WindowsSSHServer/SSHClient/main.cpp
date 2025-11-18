#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "logger.h"

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "2222"
#define DEFAULT_ADDR "127.0.0.1"
static volatile LONG g_running = 1;

struct ThreadParam
{
    SOCKET s;
    HANDLE hStopEvent;
};

DWORD WINAPI RecvThreadProc(LPVOID lpParam)
{
    ThreadParam* p = (ThreadParam*)lpParam;
    SOCKET s = p->s;
    HANDLE hStop = p->hStopEvent;

    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD stdoutType = (hStdout == NULL || hStdout == INVALID_HANDLE_VALUE) ? FILE_TYPE_UNKNOWN : GetFileType(hStdout);

    char buf[4096];
    while (InterlockedCompareExchange(&g_running, 0, 0))
    {
        int rc = recv(s, buf, (int)sizeof(buf) - 1, 0);
        if (rc == 0)
        {
            Log::Info("Server closed connection");
            break;
        }
        else if (rc == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEINTR || err == WSAEWOULDBLOCK || err == WSAEWOULDBLOCK)
            {
                Sleep(10);
                continue;
            }
            char tmp[128];
            sprintf_s(tmp, "recv() failed (%d)", err);
            Log::Error(tmp);
            break;
        }

        buf[rc] = '\0';

        if (stdoutType == FILE_TYPE_CHAR)
        {
            DWORD written = 0;
            // WriteConsoleA may fail if this process output is redirected; check return
            if (!WriteConsoleA(hStdout, buf, rc, &written, NULL))
            {
                // fallback to WriteFile
                WriteFile(hStdout, buf, rc, &written, NULL);
            }
        }
        else
        {
            DWORD written = 0;
            WriteFile(hStdout, buf, rc, &written, NULL);
        }
    }

    // signal stop
    SetEvent(hStop);
    return 0;
}

DWORD WINAPI StdinThreadProc(LPVOID lpParam)
{
    ThreadParam* p = (ThreadParam*)lpParam;
    SOCKET s = p->s;
    HANDLE hStop = p->hStopEvent;

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD stdinType = (hStdin == NULL || hStdin == INVALID_HANDLE_VALUE) ? FILE_TYPE_UNKNOWN : GetFileType(hStdin);

    char buf[4096];

    if (stdinType == FILE_TYPE_CHAR)
    {
        // Interactive console: ReadConsoleA in blocking fashion
        while (InterlockedCompareExchange(&g_running, 0, 0))
        {
            DWORD readChars = 0;
            if (!ReadConsoleA(hStdin, buf, (DWORD)sizeof(buf) - 1, &readChars, NULL) || readChars == 0)
            {
                // EOF or error
                break;
            }
            // ReadConsole includes newline(s); send exactly read size
            int sent = send(s, buf, (int)readChars, 0);
            if (sent == SOCKET_ERROR)
            {
                char tmp[128];
                sprintf_s(tmp, "send() failed (%d)", WSAGetLastError());
                Log::Warning(tmp);
                break;
            }
        }
    }
    else if (stdinType == FILE_TYPE_PIPE || stdinType == FILE_TYPE_DISK)
    {
        // Redirected input: ReadFile loop
        while (InterlockedCompareExchange(&g_running, 0, 0))
        {
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(hStdin, buf, (DWORD)sizeof(buf) - 1, &bytesRead, NULL);
            if (!ok || bytesRead == 0)
            {
                // EOF or error
                break;
            }
            int sent = send(s, buf, (int)bytesRead, 0);
            if (sent == SOCKET_ERROR)
            {
                char tmp[128];
                sprintf_s(tmp, "send() failed (%d)", WSAGetLastError());
                Log::Warning(tmp);
                break;
            }
        }
    }
    else
    {
        Log::Warning("Unknown stdin type; no input will be read");
    }

    // signal stop (EOF on stdin)
    SetEvent(hStop);
    return 0;
}

int main(int argc, char* argv[])
{
    Log::Initialize(Log::Level::LOG_LEVEL_INFO, nullptr);

    const char* addr = DEFAULT_ADDR;
    const char* port = DEFAULT_PORT;

    if (argc >= 2) addr = argv[1];
    if (argc >= 3) port = argv[2];

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        Log::Error("WSAStartup failed");
        return 1;
    }

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = NULL;
    int rc = getaddrinfo(addr, port, &hints, &result);
    if (rc != 0)
    {
        char tmp[256];
        sprintf_s(tmp, "getaddrinfo failed (%d)", rc);
        Log::Error(tmp);
        WSACleanup();
        return 1;
    }

    SOCKET s = INVALID_SOCKET;
    for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) break;
        closesocket(s);
        s = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (s == INVALID_SOCKET)
    {
        Log::Error("Unable to connect to server");
        WSACleanup();
        return 1;
    }

    {
        char tmp[128];
        sprintf_s(tmp, "Connected to %s:%s", addr, port);
        Log::Info(tmp);
    }

    // Make socket blocking (default) so send/recv behave simply
    u_long mode = 0;
    ioctlsocket(s, FIONBIO, &mode);

    HANDLE hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    ThreadParam params;
    params.s = s;
    params.hStopEvent = hStopEvent;

    HANDLE hRecv = CreateThread(NULL, 0, RecvThreadProc, &params, 0, NULL);
    HANDLE hStdin = CreateThread(NULL, 0, StdinThreadProc, &params, 0, NULL);

    // Wait until one of threads signals stop or both exit
    HANDLE handles[2] = { hStopEvent, hRecv };
    // Loop waiting for stop event or thread exits. Also handle Ctrl-C via console.
    while (InterlockedCompareExchange(&g_running, 0, 0))
    {
        DWORD wait = WaitForSingleObject(hStopEvent, 1000);
        if (wait == WAIT_OBJECT_0) break;

        // If recv thread terminated, break
        DWORD rcWait = WaitForSingleObject(hRecv, 0);
        if (rcWait == WAIT_OBJECT_0) break;
    }

    // Request shutdown
    InterlockedExchange(&g_running, 0);

    // shutdown socket to interrupt blocking operations
    shutdown(s, SD_BOTH);

    // Join threads
    if (hStdin)
    {
        WaitForSingleObject(hStdin, 2000);
        CloseHandle(hStdin);
    }
    if (hRecv)
    {
        WaitForSingleObject(hRecv, 2000);
        CloseHandle(hRecv);
    }

    if (hStopEvent) CloseHandle(hStopEvent);

    if (s != INVALID_SOCKET) closesocket(s);
    WSACleanup();

    Log::Info("Client exiting");
    return 0;
}