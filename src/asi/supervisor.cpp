#include "asi/supervisor.h"

#include "asi/api.h"
#include "common/log.h"
#include "common/pipe.h"
#include "common/protocol.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <tlhelp32.h>
#include <thread>
#include <unordered_set>
#include <vector>

namespace asterui::supervisor {

namespace {

std::atomic<bool> g_running = false;
std::atomic<bool> g_connected = false;
std::atomic<bool> g_ready = false;
std::mutex g_sendMutex;
PipeConnection g_commandPipe;
PipeConnection g_eventPipe;
HANDLE g_process = nullptr;
HANDLE g_job = nullptr;
HANDLE g_stopEvent = nullptr;
HMODULE g_module = nullptr;
std::wstring g_gameDirectory;
std::wstring g_uiRoot;
std::wstring g_hostPath;
std::wstring g_logPath;
std::string g_token;
uint32_t g_maxBrowsers = kMaxBrowsers;
int g_maxFps = 60;
bool g_gpu = false;
// Heap-owned so CRT process detach never destroys a joinable thread.
std::thread* g_worker = nullptr;

constexpr DWORD kHostStartupTimeoutMs = 120000;
constexpr DWORD kHostHeartbeatTimeoutMs = 12000;

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

std::wstring Quote(const std::wstring& value) {
    std::wstring result = L"\"";
    for (wchar_t character : value) {
        if (character == L'"') {
            result.push_back(L'\\');
        }
        result.push_back(character);
    }
    result.push_back(L'"');
    return result;
}

std::string NewToken() {
    std::random_device random;
    std::mt19937_64 generator(
        (static_cast<uint64_t>(random()) << 32) ^
        random() ^ GetTickCount64() ^ GetCurrentProcessId());
    char token[33] = {};
    std::snprintf(
        token,
        sizeof(token),
        "%016llx%016llx",
        static_cast<unsigned long long>(generator()),
        static_cast<unsigned long long>(generator()));
    return token;
}

void LoadConfig() {
    const std::filesystem::path path =
        std::filesystem::path(g_uiRoot) / L"AsterUI.json";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        Log::Info("config not found; using defaults");
        return;
    }
    try {
        nlohmann::json value;
        file >> value;
        g_maxFps = std::clamp(value.value("maxFps", 60), 1, 240);
        g_gpu = value.value("uiGpu", false);
        g_maxBrowsers = std::clamp(
            value.value("maxBrowsers", static_cast<uint32_t>(kMaxBrowsers)),
            1u,
            static_cast<uint32_t>(kMaxBrowsers));
        Log::Info(
            "config: maxFps=" + std::to_string(g_maxFps) +
            " uiGpu=" + (g_gpu ? "1" : "0") +
            " maxBrowsers=" + std::to_string(g_maxBrowsers));
    } catch (const std::exception& exception) {
        Log::Warn(std::string("invalid AsterUI.json: ") + exception.what());
    }
}

bool RuntimeComplete(std::wstring& reason) {
    const std::filesystem::path root(g_uiRoot);
    const char* files[] = {
        "AsterUI.Host.exe",
        "libcef.dll",
        "chrome_elf.dll",
        "resources.pak",
        "icudtl.dat",
        "v8_context_snapshot.bin",
    };
    for (const char* file : files) {
        if (!std::filesystem::is_regular_file(root / file)) {
            reason = L"missing " + std::wstring(file, file + std::strlen(file));
            return false;
        }
    }
    if (!std::filesystem::is_directory(root / L"locales")) {
        reason = L"missing locales directory";
        return false;
    }
    return true;
}

HANDLE CreateKillJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        return nullptr;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
        JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (!SetInformationJobObject(
            job,
            JobObjectExtendedLimitInformation,
            &limits,
            sizeof(limits))) {
        CloseHandle(job);
        return nullptr;
    }
    return job;
}

void TerminateProcessTree(HANDLE rootProcess) {
    const DWORD rootPid = rootProcess ? GetProcessId(rootProcess) : 0;
    if (rootPid == 0) {
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    struct Entry {
        DWORD pid = 0;
        DWORD parent = 0;
    };
    std::vector<Entry> processes;
    PROCESSENTRY32W item = {};
    item.dwSize = sizeof(item);
    if (Process32FirstW(snapshot, &item)) {
        do {
            processes.push_back({item.th32ProcessID, item.th32ParentProcessID});
        } while (Process32NextW(snapshot, &item));
    }
    CloseHandle(snapshot);

    std::unordered_set<DWORD> descendants = {rootPid};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Entry& process : processes) {
            if (descendants.contains(process.pid) ||
                !descendants.contains(process.parent)) {
                continue;
            }
            descendants.insert(process.pid);
            changed = true;
        }
    }

    for (const Entry& process : processes) {
        if (process.pid == rootPid || !descendants.contains(process.pid)) {
            continue;
        }
        HANDLE child = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, process.pid);
        if (!child) {
            continue;
        }
        TerminateProcess(child, 0);
        WaitForSingleObject(child, 1000);
        CloseHandle(child);
    }
}

void CloseProcess() {
    if (g_process) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(g_process, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcessTree(g_process);
            TerminateProcess(g_process, 0);
        }
        WaitForSingleObject(g_process, 2000);
        CloseHandle(g_process);
        g_process = nullptr;
    }
    if (g_job) {
        TerminateJobObject(g_job, 0);
        CloseHandle(g_job);
        g_job = nullptr;
    }
}

bool LaunchHost(const std::wstring& commandName, const std::wstring& eventName) {
    if (!std::filesystem::is_regular_file(g_hostPath)) {
        Log::Error("cannot launch host: AsterUI.Host.exe missing");
        return false;
    }
    if (!std::filesystem::is_regular_file(std::filesystem::path(g_uiRoot) / L"libcef.dll")) {
        Log::Error("cannot launch host: libcef.dll missing");
        return false;
    }

    std::wstring command =
        Quote(g_hostPath) +
        L" --pipe-cmd " + Quote(commandName) +
        L" --pipe-evt " + Quote(eventName) +
        L" --token " + Quote(std::wstring(g_token.begin(), g_token.end())) +
        L" --ui-root " + Quote(g_uiRoot) +
        L" --log " + Quote(g_logPath) +
        L" --fps " + std::to_wstring(g_maxFps) +
        L" --gpu " + std::to_wstring(g_gpu ? 1 : 0);
    if (!g_gpu) {
        command += L" --disable-gpu --disable-gpu-compositing";
    }
    command +=
        L" --allow-file-access-from-files"
        L" --disable-background-networking"
        L" --disable-component-update"
        L" --disable-sync"
        L" --disable-default-apps"
        L" --no-first-run"
        L" --no-pings"
        L" --metrics-recording-only"
        L" --disable-domain-reliability"
        L" --disable-features=OptimizationHints,MediaRouter,Translate";

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    std::wstring mutableCommand = command;
    if (g_job) {
        CloseHandle(g_job);
    }
    g_job = CreateKillJob();
    if (!CreateProcessW(
            g_hostPath.c_str(),
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            std::filesystem::path(g_hostPath).parent_path().c_str(),
            &startup,
            &process)) {
        if (g_job) {
            CloseHandle(g_job);
            g_job = nullptr;
        }
        Log::Error("cannot launch host, error " + std::to_string(GetLastError()));
        return false;
    }
    CloseHandle(process.hThread);
    g_process = process.hProcess;
    if (g_job) {
        if (!AssignProcessToJobObject(g_job, g_process)) {
            Log::Warn(
                "cannot assign host process to kill job, error " +
                std::to_string(GetLastError()));
        }
    }
    Log::Info("host launched, pid " + std::to_string(process.dwProcessId));
    return true;
}

bool Handshake(PipeConnection& eventPipe, PipeConnection& commandPipe) {
    std::string hello;
    const ULONGLONG deadline = GetTickCount64() + 5000;
    for (;;) {
        bool timedOut = false;
        if (eventPipe.ReadMessageTimeout(hello, 500, timedOut)) {
            break;
        }
        if (!g_running) {
            return false;
        }
        if (GetTickCount64() >= deadline) {
            Log::Warn("host handshake failed: no hello");
            return false;
        }
        if (!timedOut) {
            Log::Warn("host handshake failed: event pipe closed");
            return false;
        }
    }
    try {
        const nlohmann::json value = nlohmann::json::parse(hello);
        if (value.value("type", "") != "hello" ||
            value.value("protocol", 0u) != kProtocolVersion ||
            value.value("token", "") != g_token) {
            Log::Warn("host handshake rejected");
            return false;
        }
    } catch (const std::exception& exception) {
        Log::Warn(std::string("host handshake JSON failed: ") + exception.what());
        return false;
    }
    return commandPipe.WriteMessage(nlohmann::json{
        {"type", "hello_ack"},
        {"protocol", kProtocolVersion},
        {"ok", true}}.dump());
}

bool RunSession() {
    const std::wstring suffix =
        std::to_wstring(GetCurrentProcessId()) + L"." +
        std::to_wstring(GetTickCount64());
    const std::wstring commandName = L"\\\\.\\pipe\\AsterUI.Cmd." + suffix;
    const std::wstring eventName = L"\\\\.\\pipe\\AsterUI.Evt." + suffix;

    PipeServer commandServer;
    PipeServer eventServer;
    if (!commandServer.Create(commandName) || !eventServer.Create(eventName)) {
        Log::Error("cannot create control pipes");
        return false;
    }

    std::wstring reason;
    if (!RuntimeComplete(reason)) {
        Log::Error("browser runtime incomplete: " + std::string(reason.begin(), reason.end()));
        return false;
    }
    if (!LaunchHost(commandName, eventName)) {
        return false;
    }

    PipeConnection command = commandServer.Accept(15000, g_stopEvent);
    PipeConnection event = eventServer.Accept(15000, g_stopEvent);
    if (!g_running) {
        CloseProcess();
        return false;
    }
    if (!command.IsOpen() || !event.IsOpen()) {
        Log::Warn("host did not connect to both pipes");
        CloseProcess();
        return false;
    }
    if (!Handshake(event, command)) {
        CloseProcess();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_sendMutex);
        g_commandPipe = std::move(command);
    }
    g_eventPipe = std::move(event);
    g_connected = true;
    g_ready = false;
    api::SetHostConnected(true);
    Log::Info("host handshake complete");

    bool readyThisSession = false;
    ULONGLONG lastHostContact = GetTickCount64();
    for (;;) {
        if (!g_running) {
            break;
        }
        std::string message;
        bool timedOut = false;
        if (!g_eventPipe.ReadMessageTimeout(message, 1000, timedOut)) {
            if (!g_running) {
                break;
            }
            if (WaitForSingleObject(g_process, 0) == WAIT_OBJECT_0) {
                DWORD code = 0;
                GetExitCodeProcess(g_process, &code);
                Log::Warn("host process exited, code " + std::to_string(code));
                break;
            }
            const ULONGLONG now = GetTickCount64();
            const DWORD limit = g_ready
                ? kHostHeartbeatTimeoutMs
                : kHostStartupTimeoutMs;
            if (now - lastHostContact > limit) {
                Log::Error(
                    "host health timeout after " +
                    std::to_string(now - lastHostContact) + " ms");
                break;
            }
            if (!timedOut) {
                Log::Warn("host event pipe closed");
                break;
            }
            continue;
        }
        lastHostContact = GetTickCount64();
        try {
            const nlohmann::json value = nlohmann::json::parse(message);
            if (value.value("type", "") == "ready") {
                g_ready = true;
                readyThisSession = true;
                Log::Info("host CEF ready");
            }
            api::HandleHostMessage(message);
        } catch (const std::exception& exception) {
            Log::Warn(std::string("invalid host message: ") + exception.what());
        }
    }

    g_ready = false;
    g_connected = false;
    api::SetHostConnected(false);
    api::OnHostDisconnected();
    {
        std::lock_guard<std::mutex> lock(g_sendMutex);
        g_commandPipe.Close();
    }
    g_eventPipe.Close();
    CloseProcess();
    return readyThisSession;
}

void Worker() {
    unsigned backoffMs = 500;
    while (g_running) {
        const bool readyThisSession = RunSession();
        if (!g_running) {
            break;
        }
        if (readyThisSession) {
            backoffMs = 500;
        }
        Log::Warn("host session ended; restarting in " + std::to_string(backoffMs) + " ms");
        if (WaitForSingleObject(g_stopEvent, backoffMs) == WAIT_OBJECT_0) {
            break;
        }
        backoffMs = std::min<unsigned>(backoffMs * 2, 15000);
    }
}

}  // namespace

void Start(HMODULE module) {
    if (g_running.exchange(true)) {
        return;
    }
    Log::Info("supervisor starting");
    g_module = module;
    g_gameDirectory = ModuleDirectory(module);
    g_uiRoot = (std::filesystem::path(g_gameDirectory) / L"AsterUI").wstring();
    g_hostPath = (std::filesystem::path(g_uiRoot) / L"AsterUI.Host.exe").wstring();
    g_logPath = (std::filesystem::path(g_uiRoot) / L"AsterUI.Host.log").wstring();
    LoadConfig();
    g_token = NewToken();
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    try {
        g_worker = new std::thread(Worker);
        Log::Info("supervisor worker started");
    } catch (const std::exception& exception) {
        g_running = false;
        Log::Error(std::string("supervisor worker failed: ") + exception.what());
    }
}

void Stop() {
    if (!g_running.exchange(false)) {
        return;
    }
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }
    if (g_connected) {
        Send(nlohmann::json{{"type", "shutdown"}}.dump());
    }
    if (g_worker) {
        if (g_worker->joinable()) {
            g_worker->join();
        }
        delete g_worker;
        g_worker = nullptr;
    }
    CloseProcess();
    {
        std::lock_guard<std::mutex> lock(g_sendMutex);
        g_commandPipe.Close();
    }
    g_eventPipe.Close();
    if (g_job) {
        CloseHandle(g_job);
        g_job = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
}

void Tick() {
}

bool Send(const std::string& json) {
    if (!g_connected) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_sendMutex);
    return g_commandPipe.WriteMessage(json);
}

bool IsConnected() {
    return g_connected;
}

bool IsReady() {
    return g_connected && g_ready;
}

uint32_t MaxBrowsers() {
    return g_maxBrowsers;
}

}  // namespace asterui::supervisor
