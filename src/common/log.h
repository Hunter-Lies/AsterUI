#pragma once

#include <mutex>
#include <string>
#include <windows.h>

namespace asterui {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
};

class Log {
public:
    static void Configure(HMODULE module, const wchar_t* relativePath, LogLevel level);
    static void Write(LogLevel level, const std::string& message);
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warn(const std::string& message);
    static void Error(const std::string& message);

private:
    static void WriteLocked(LogLevel level, const std::string& message);
    static void RotateIfNeeded(const std::wstring& path);

    static std::mutex mutex_;
    static std::wstring path_;
    static LogLevel configuredLevel_;
};

}  // namespace asterui
