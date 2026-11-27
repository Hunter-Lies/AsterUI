#include "host/app.h"
#include "host/browser.h"
#include "host/pipe_client.h"
#include "host/ui_scheme.h"

#include "common/log.h"

#include "include/cef_command_line.h"
#include "include/cef_app.h"
#include "include/cef_task.h"

#include <nlohmann/json.hpp>

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace asterui;
using namespace asterui::host;

struct Options {
    std::wstring commandPipe;
    std::wstring eventPipe;
    std::string token;
    std::wstring uiRoot;
    std::wstring log;
    int fps = 60;
    bool gpu = false;
};

std::map<std::wstring, std::wstring> ParseArguments() {
    std::map<std::wstring, std::wstring> values;
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) {
        return values;
    }
    for (int index = 1; index < count; ++index) {
        std::wstring key = arguments[index];
        if (key.rfind(L"--", 0) != 0) {
            continue;
        }
        const size_t equal = key.find(L'=');
        if (equal != std::wstring::npos) {
            values[key.substr(0, equal)] = key.substr(equal + 1);
            continue;
        }
        if (index + 1 < count && std::wstring(arguments[index + 1]).rfind(L"--", 0) != 0) {
            values[key] = arguments[++index];
        }
    }
    LocalFree(arguments);
    return values;
}

std::string Narrow(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int bytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    std::string result(bytes, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        bytes,
        nullptr,
        nullptr);
    return result;
}

std::wstring ModulePath() {
    wchar_t path[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameW(
        nullptr,
        path,
        static_cast<DWORD>(std::size(path)));
    return length > 0 && length < std::size(path)
        ? std::wstring(path, length)
        : std::wstring();
}

std::vector<std::string> LoadDevelopmentOrigins(const std::wstring& uiRoot) {
    std::vector<std::string> result;
    const std::filesystem::path path =
        std::filesystem::path(uiRoot) / L"AsterUI.json";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return result;
    }
    try {
        nlohmann::json value;
        file >> value;
        const auto development = value.value("development", nlohmann::json::object());
        if (!development.value("enabled", false)) {
            return result;
        }
        for (const auto& origin : development.value(
                 "allowedOrigins",
                 std::vector<std::string>{})) {
            const std::string normalized =
                RequestPolicy::NormalizeOrigin(origin);
            if (!normalized.empty()) {
                result.push_back(normalized);
            }
        }
        Log::Info(
            "development origins enabled: " +
            std::to_string(result.size()));
    } catch (const std::exception& exception) {
        Log::Warn(
            std::string("invalid development config: ") +
            exception.what());
    }
    return result;
}

class HostRuntime;

class UiTask final : public CefTask {
public:
    UiTask(HostRuntime* runtime, std::function<void()> action)
        : runtime_(runtime), action_(std::move(action)) {}
    void Execute() override;

private:
    HostRuntime* runtime_ = nullptr;
    std::function<void()> action_;
    IMPLEMENT_REFCOUNTING(UiTask);
};

class HostRuntime {
public:
    explicit HostRuntime(Options options)
        : options_(std::move(options)),
          manager_(
              Narrow(options_.uiRoot),
              options_.fps,
              LoadDevelopmentOrigins(options_.uiRoot),
              [this](const std::string& value) { pipe_.Send(value); }) {}

    bool Connect() {
        return pipe_.Connect(options_.commandPipe, options_.eventPipe, 15000) &&
            pipe_.Handshake(options_.token);
    }

    bool PipeConnected() const {
        return pipe_.IsConnected();
    }

    void StartReader() {
        pipe_.Start(
            [this](const std::string& message) {
                if (!cefReady_) {
                    return;
                }
                auto copy = message;
                PostUi([this, copy]() { Execute(copy); });
            },
            [this]() {
                if (cefReady_) {
                    PostUi([this]() {
                        manager_.CloseAll();
                        CefQuitMessageLoop();
                    });
                } else {
                    quitRequested_ = true;
                }
            });
    }

    void MarkCefReady() {
        cefReady_ = true;
    }

    void StartHealthMonitor() {
        if (healthRunning_.exchange(true)) {
            return;
        }
        lastUiTickMs_ = GetTickCount64();
        heartbeat_ = std::thread([this]() {
            unsigned missedUiTicks = 0;
            while (healthRunning_) {
                const uint64_t before = lastUiTickMs_.load();
                PostUi([this]() {
                    lastUiTickMs_ = GetTickCount64();
                    manager_.CheckHealth();
                });
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const uint64_t after = lastUiTickMs_.load();
                if (after == before) {
                    ++missedUiTicks;
                    if (missedUiTicks % 5 == 0) {
                        Log::Warn(
                            "CEF UI has not serviced the health task for " +
                            std::to_string(missedUiTicks) + " s");
                    }
                    continue;
                }
                missedUiTicks = 0;
                pipe_.Send(nlohmann::json{
                    {"type", "health"},
                    {"uiTick", after},
                    {"browsers", manager_.Count()}}.dump());
            }
        });
    }

    void StopHealthMonitor() {
        if (!healthRunning_.exchange(false)) {
            return;
        }
        if (heartbeat_.joinable()) {
            heartbeat_.join();
        }
    }

    bool QuitRequested() const {
        return quitRequested_;
    }

    void SendReady() {
        pipe_.Send(nlohmann::json{
            {"type", "ready"},
            {"protocol", kProtocolVersion},
            {"cef", "150.0.22"}}.dump());
    }

    void Stop() {
        StopHealthMonitor();
        pipe_.Stop();
    }

private:
    void PostUi(std::function<void()> action) {
        CefPostTask(TID_UI, new UiTask(this, std::move(action)));
    }

    void Execute(const std::string& message) {
        nlohmann::json value;
        try {
            value = nlohmann::json::parse(message);
        } catch (const std::exception& exception) {
            Log::Warn(std::string("invalid control command: ") + exception.what());
            return;
        }
        const std::string type = value.value("type", "");
        bool accepted = true;
        if (type == "create") {
            accepted = manager_.Create(message);
        } else if (type == "state") {
            accepted = manager_.UpdateState(message);
        } else if (type == "navigate") {
            accepted = manager_.Navigate(message);
        } else if (type == "event") {
            accepted = manager_.SendEvent(message);
        } else if (type == "input") {
            accepted = manager_.Input(message);
        } else if (type == "destroy") {
            accepted = manager_.Destroy(message);
        } else if (type == "shutdown") {
            manager_.CloseAll();
            CefQuitMessageLoop();
            return;
        } else {
            accepted = false;
        }
        if (!accepted) {
            Log::Warn("command rejected: " + type);
        }
    }

    Options options_;
    PipeClient pipe_;
    BrowserManager manager_;
    std::atomic<bool> cefReady_ = false;
    std::atomic<bool> quitRequested_ = false;
    std::atomic<bool> healthRunning_ = false;
    std::atomic<uint64_t> lastUiTickMs_ = 0;
    std::thread heartbeat_;

    friend class UiTask;
};

void UiTask::Execute() {
    if (action_) {
        action_();
    }
}

bool ConfigureCef(
    CefSettings& settings,
    const Options& options,
    const std::wstring& executable) {
    settings.no_sandbox = true;
    settings.windowless_rendering_enabled = true;
    settings.multi_threaded_message_loop = false;
    settings.external_message_pump = false;
    settings.log_severity = LOGSEVERITY_WARNING;
    CefString(&settings.browser_subprocess_path).FromWString(executable);
    CefString(&settings.resources_dir_path).FromWString(options.uiRoot);
    CefString(&settings.locales_dir_path).FromWString(
        (std::filesystem::path(options.uiRoot) / L"locales").wstring());
    CefString(&settings.cache_path).FromWString(
        (std::filesystem::path(options.uiRoot) / L"cache").wstring());
    CefString(&settings.log_file).FromWString(
        (std::filesystem::path(options.uiRoot) / L"cef.log").wstring());
    std::error_code error;
    std::filesystem::create_directories(
        std::filesystem::path(options.uiRoot) / L"cache",
        error);
    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    CefMainArgs mainArgs(instance);
    CefRefPtr<AsterUiApp> app = new AsterUiApp();
    const int processResult = CefExecuteProcess(mainArgs, app, nullptr);
    if (processResult >= 0) {
        return processResult;
    }

    const auto arguments = ParseArguments();
    const auto required = [&arguments](const wchar_t* name, std::wstring& value) {
        auto iterator = arguments.find(name);
        if (iterator == arguments.end()) {
            return false;
        }
        value = iterator->second;
        return true;
    };

    Options options;
    std::wstring token;
    if (!required(L"--pipe-cmd", options.commandPipe) ||
        !required(L"--pipe-evt", options.eventPipe) ||
        !required(L"--token", token) ||
        !required(L"--ui-root", options.uiRoot)) {
        return 1;
    }
    options.token = Narrow(token);
    options.log = arguments.contains(L"--log") ? arguments.at(L"--log") : L"";
    options.fps = arguments.contains(L"--fps")
        ? std::max(1, std::min(240, _wtoi(arguments.at(L"--fps").c_str())))
        : 60;
    options.gpu = arguments.contains(L"--gpu") && arguments.at(L"--gpu") == L"1";

    const std::filesystem::path uiRoot(options.uiRoot);
    Log::Configure(
        nullptr,
        (uiRoot / L"AsterUI.Host.log").c_str(),
        LogLevel::Info);
    Log::Info("AsterUI host starting, pid " + std::to_string(GetCurrentProcessId()));

    HostRuntime runtime(options);
    if (!runtime.Connect()) {
        Log::Error("control pipe connection or handshake failed");
        return 2;
    }
    Log::Info("control pipe connected");

    CefSettings settings;
    ConfigureCef(settings, options, ModulePath());
    Log::Info("calling CefInitialize");
    if (!CefInitialize(mainArgs, settings, app, nullptr)) {
        Log::Error("CefInitialize failed");
        return 3;
    }
    runtime.MarkCefReady();
    runtime.SendReady();
    runtime.StartReader();
    runtime.StartHealthMonitor();
    Log::Info("CEF initialized and ready");

    if (!runtime.QuitRequested()) {
        CefRunMessageLoop();
    }
    runtime.Stop();
    CefShutdown();
    Log::Info("AsterUI host stopped");
    return 0;
}
