#include "common/log.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace asterui {

std::mutex Log::mutex_;
std::wstring Log::path_;
LogLevel Log::configuredLevel_ = LogLevel::Info;

namespace {

std::wstring ModuleDirectory(HMODULE module) {
    wchar_t path[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameW(
        module,
        path,
        static_cast<DWORD>(std::size(path)));
    if (length == 0 || length >= std::size(path)) {
        return L".";
    }
    return std::filesystem::path(std::wstring(path, length)).parent_path().wstring();
}

const char* LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO ";
    case LogLevel::Warn: return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO ";
}

}  // namespace

void Log::Configure(HMODULE module, const wchar_t* relativePath, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::filesystem::path root(ModuleDirectory(module));
    path_ = (root / (relativePath ? relativePath : L"AsterUI/AsterUI.log")).wstring();
    configuredLevel_ = level;
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path(), error);
}

void Log::RotateIfNeeded(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return;
    }
    ULARGE_INTEGER size = {};
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;
    if (size.QuadPart > 512 * 1024) {
        MoveFileExW(path.c_str(), (path + L".old").c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}

void Log::WriteLocked(LogLevel level, const std::string& message) {
    OutputDebugStringA(("[AsterUI] " + message + "\n").c_str());
    if (path_.empty() || static_cast<int>(level) < static_cast<int>(configuredLevel_)) {
        return;
    }
    RotateIfNeeded(path_);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    char prefix[64] = {};
    std::snprintf(
        prefix,
        sizeof(prefix),
        "%02u:%02u:%02u.%03u [%s] ",
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        LevelName(level));

    FILE* file = nullptr;
    if (_wfopen_s(&file, path_.c_str(), L"ab") != 0 || !file) {
        return;
    }
    std::fwrite(prefix, 1, std::strlen(prefix), file);
    std::fwrite(message.data(), 1, message.size(), file);
    std::fputc('\n', file);
    std::fclose(file);
}

void Log::Write(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    WriteLocked(level, message);
}

void Log::Debug(const std::string& message) { Write(LogLevel::Debug, message); }
void Log::Info(const std::string& message) { Write(LogLevel::Info, message); }
void Log::Warn(const std::string& message) { Write(LogLevel::Warn, message); }
void Log::Error(const std::string& message) { Write(LogLevel::Error, message); }

}  // namespace asterui
