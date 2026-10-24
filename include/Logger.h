#pragma once

#include <cstdarg>
#include <string>

namespace keybinder {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3
};

class Logger {
public:
    static void Initialize(const std::string& filepath);
    static void Shutdown();

    static void SetMinLevel(LogLevel level);
    static LogLevel GetMinLevel();

    static void Log(LogLevel level, const char* fmt, ...);
    static void Debug(const char* fmt, ...);
    static void Info(const char* fmt, ...);
    static void Warning(const char* fmt, ...);
    static void Error(const char* fmt, ...);

#ifdef KEYBINDER_TESTING
    using LogSink = void(*)(LogLevel level, const char* message);
    static void SetTestSink(LogSink sink);
    static void ResetTestSink();
#endif

private:
    static void LogV(LogLevel level, const char* fmt, va_list args);
    static const char* LevelToString(LogLevel level);
    static std::string GetTimestamp();
};

} // namespace keybinder
