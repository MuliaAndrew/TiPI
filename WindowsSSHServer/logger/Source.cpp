#include <windows.h>
#include <stdio.h>
#include "logger.h"
#include <string.h>

// Static member definitions
HANDLE Log::console_handle = NULL;
Log::Level Log::current_log_level = Log::Level::LOG_LEVEL_NONE;
SRWLOCK Log::log_lock = SRWLOCK_INIT;
bool is_initialized = false;

HANDLE Log::OpenLogFile(const char* path)
{
	// If no path specified, use the standard console output handle (allocate console if necessary).
	if (path == nullptr)
	{
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		if (h == INVALID_HANDLE_VALUE || h == NULL)
		{
			// Try to allocate a console if the process doesn't have one.
			if (AllocConsole())
				h = GetStdHandle(STD_OUTPUT_HANDLE);
		}
		console_handle = h;
		return console_handle;
	}

	// Open or create the log file for appending. Allow read sharing so other tools can read the file.
	HANDLE hFile = CreateFileA(
		path,
		FILE_APPEND_DATA | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		// Print detailed error and exit the process.
		DWORD err = GetLastError();
		LPSTR msgBuf = NULL;
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&msgBuf,
			0, NULL);

		fprintf(stderr, "Failed to open log file '%s' (error %lu): %s\n",
			path ? path : "(null)",
			(unsigned long)err,
			msgBuf ? msgBuf : "Unknown error");

		if (msgBuf) LocalFree(msgBuf);
		ExitProcess((UINT)err);
	}

	// Ensure file pointer is at the end for append semantics.
	LARGE_INTEGER liZero = { 0 };
	SetFilePointerEx(hFile, liZero, NULL, FILE_END);

	console_handle = hFile;
	return hFile;
}

void Log::Initialize(Log::Level log_level, const char* path)
{
	// Prevent multiple initializations.
	if (is_initialized)
		return;

	// Set requested log level.
	current_log_level = log_level;

	// Open log target (file or console). OpenLogFile sets `console_handle` or exits on failure.
	HANDLE h = OpenLogFile(path);

	// Sanity check: OpenLogFile should already exit on fatal failures when path != nullptr,
	// but if it returned an invalid handle for console case, treat it as fatal.
	if (h == NULL || h == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Log::Initialize: unable to obtain output handle\n");
		ExitProcess(1);
	}

	is_initialized = true;
}

void Log::LogMessage(Log::Level level, const char* message)
{
	// Ensure initialized
	if (!is_initialized)
		return;

	// Map LogLevel to severity where higher value = more severe.
	auto severity = [](Log::Level l) -> int {
		switch (l)
		{
		case LOG_LEVEL_DEBUG:   return 0; // most verbose / least severe
		case LOG_LEVEL_INFO:    return 1;
		case LOG_LEVEL_WARNING: return 2;
		case LOG_LEVEL_ERROR:   return 3; // most severe
		default:                return 1;
		}
	};

	if (severity(level) < severity(current_log_level))
	{
		// Message is less important than configured level -> skip.
		return;
	}


	AcquireSRWLockExclusive(&log_lock);

	// Level string
	const char* levelStr = "INFO";
	switch (level)
	{
	case LOG_LEVEL_DEBUG:   levelStr = "DEBUG"; break;
	case LOG_LEVEL_INFO:    levelStr = "INFO";  break;
	case LOG_LEVEL_WARNING: levelStr = "WARN";  break;
	case LOG_LEVEL_ERROR:   levelStr = "ERROR"; break;
	}

	// Timestamp
	SYSTEMTIME st;
	GetLocalTime(&st);
	char header[128];
	int headerLen = snprintf(header, sizeof(header),
		"[%04hu-%02hu-%02hu %02hu:%02hu:%02hu.%03hu] [%s] ",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		levelStr);

	// Compose final message
	char outBuf[2048];
	int outLen = 0;
	if (headerLen > 0)
		outLen = snprintf(outBuf, sizeof(outBuf), "%s%s\r\n", header, message);
	else
		outLen = snprintf(outBuf, sizeof(outBuf), "[%s] %s\r\n", levelStr, message);

	if (outLen > 0 && console_handle != NULL && console_handle != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		DWORD fileType = GetFileType(console_handle);
		if (fileType == FILE_TYPE_CHAR)
		{
			// Console
			DWORD charsWritten = 0;
			// WriteConsoleA expects number of characters, not bytes.
			WriteConsoleA(console_handle, outBuf, (DWORD)strlen(outBuf), &charsWritten, NULL);
		}
		else
		{
			// File or pipe: write bytes
			WriteFile(console_handle, outBuf, (DWORD)outLen, &written, NULL);
		}
	}

	ReleaseSRWLockExclusive(&log_lock);
}

void Log::Info(const char* message)
{
	LogMessage(LOG_LEVEL_INFO, message);
}

void Log::Warning(const char* message)
{
	LogMessage(LOG_LEVEL_WARNING, message);
}

void Log::Error(const char* message)
{
	LogMessage(LOG_LEVEL_ERROR, message);
}

void Log::Debug(const char* message)
{
	LogMessage(LOG_LEVEL_DEBUG, message);
}