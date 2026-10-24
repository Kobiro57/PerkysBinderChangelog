/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Thread-safe file logger implementation
*/
#include "../include/Logger.h"

#include <cstdio>
#include <ctime>
#include <mutex>

namespace keybinder {

namespace {
    std::mutex g_logMutex;
    FILE* g_logFile = nullptr;
    LogLevel g_minLevel = LogLevel::Info;
    bool g_initialized = false;

#ifdef KEYBINDER_TESTING
    Logger::LogSink g_testSink = nullptr;
#endif
} // namespace

void Logger::Initialize(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_initialized) return;

    g_logFile = std::fopen(filepath.c_str(), "w");
    g_initialized = true;

    if (g_logFile) {
        std::fprintf(g_logFile, "\n========================================\n");
        std::fprintf(g_logFile, "NLBinder Log Session Started\n");
        std::fprintf(g_logFile, "========================================\n");
        std::fflush(g_logFile);
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        std::fprintf(g_logFile, "========================================\n");
        std::fprintf(g_logFile, "NLBinder Log Session Ended\n");
        std::fprintf(g_logFile, "========================================\n\n");
        std::fflush(g_logFile);
        std::fclose(g_logFile);
        g_logFile = nullptr;
    }
    g_initialized = false;
}

void Logger::SetMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_minLevel = level;
}

LogLevel Logger::GetMinLevel() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_minLevel;
}

void Logger::Log(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogV(level, fmt, args);
    va_end(args);
}

void Logger::Debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogV(LogLevel::Debug, fmt, args);
    va_end(args);
}

void Logger::Info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogV(LogLevel::Info, fmt, args);
    va_end(args);
}

void Logger::Warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogV(LogLevel::Warning, fmt, args);
    va_end(args);
}

void Logger::Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogV(LogLevel::Error, fmt, args);
    va_end(args);
}

void Logger::LogV(LogLevel level, const char* fmt, va_list args) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (static_cast<int>(level) < static_cast<int>(g_minLevel)) return;

    char buffer[2048];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);

#ifdef KEYBINDER_TESTING
    if (g_testSink) {
        g_testSink(level, buffer);
        return;
    }
#endif

    if (!g_logFile) return;

    std::string ts = GetTimestamp();
    std::fprintf(g_logFile, "[%s] [%s] %s\n", ts.c_str(), LevelToString(level), buffer);
    std::fflush(g_logFile);
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        default:                return "???";
    }
}

std::string Logger::GetTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    return buf;
}

#ifdef KEYBINDER_TESTING
void Logger::SetTestSink(LogSink sink) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_testSink = sink;
}

void Logger::ResetTestSink() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_testSink = nullptr;
}
#endif

} // namespace keybinder
