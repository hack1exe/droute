#include "pch.h"
#include "src/core/logger.hpp"
#include "src/core/config.hpp"

#include <cstdarg>
#include <ctime>

namespace droute {

    static HANDLE g_logFile = INVALID_HANDLE_VALUE;
    static std::mutex g_logMutex;
    static LogLevel g_logLevel = LogLevel::Info;

    const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
        }
        return "?????";
    }

    bool Logger::Init() {
        char path[MAX_PATH];
        DWORD len = GetTempPathA(MAX_PATH, path);
        if (len == 0 || len >= MAX_PATH) {
            path[0] = '.'; path[1] = '\\'; path[2] = '\0';
        }
        strcat_s(path, MAX_PATH, "droute.log");

        HANDLE hFile = CreateFileA(path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            char msg[256];
            snprintf(msg, sizeof(msg), "droute: failed to open log file '%s'\n", path);
            OutputDebugStringA(msg);
            g_logFile = INVALID_HANDLE_VALUE;
            return false;
        }

        SetFilePointer(hFile, 0, NULL, FILE_END);

        g_logFile = hFile;

        SYSTEMTIME st;
        GetLocalTime(&st);
        char header[256];
        int headerLen = snprintf(header, sizeof(header),
            "// droute session started at %04d-%02d-%02d %02d:%02d:%02d.%03d\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        DWORD written;
        WriteFile(g_logFile, header, static_cast<DWORD>(headerLen), &written, NULL);

        return true;
    }

    void Logger::Shutdown() {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logFile != INVALID_HANDLE_VALUE) {
            CloseHandle(g_logFile);
            g_logFile = INVALID_HANDLE_VALUE;
        }
    }

    void Logger::SetLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_logLevel = level;
    }

    void Logger::Write(LogLevel level, const char* file, int line, const char* fmt, ...) {
        if (level < g_logLevel) return;

        const char* basename = file;
        const char* p = strrchr(file, '\\');
        if (p) basename = p + 1;
        p = strrchr(file, '/');
        if (p && p + 1 > basename) basename = p + 1;

        SYSTEMTIME st;
        GetLocalTime(&st);

        va_list args;
        va_start(args, fmt);

        char msg[2048];
        vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);
        va_end(args);

        char buf[2300];
        int bufLen = snprintf(buf, sizeof(buf),
            "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [TID:%lu] [%s] [%s:%d] %s\n",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentThreadId(),
            LevelToString(level),
            basename, line,
            msg);

        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(g_logFile, buf, static_cast<DWORD>(bufLen), &written, NULL);
        } else {
            OutputDebugStringA(buf);
        }
    }

}
