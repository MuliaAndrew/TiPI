#pragma once
#include <windows.h>

class Log final
{
public:
	enum Level
	{
		LOG_LEVEL_NONE = 0,
		LOG_LEVEL_INFO,
		LOG_LEVEL_WARNING,
		LOG_LEVEL_ERROR,
		LOG_LEVEL_DEBUG
	};
private:
	static HANDLE console_handle;
	static Level current_log_level;
	static SRWLOCK log_lock;

	// Opens the log file at the specified path and returns the handle. if path is nullptr, opens the console.
	static HANDLE OpenLogFile(const char* path);
	static void LogMessage(Level level, const char* message);
public:
	static void Initialize(Level log_level, const char* path);
	static void Info(const char* message);
	static void Warning(const char* message);
	static void Error(const char* message);
	static void Debug(const char* message);
};


