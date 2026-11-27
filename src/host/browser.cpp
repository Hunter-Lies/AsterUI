#include "host/browser.h"

#include "common/log.h"
#include "host/ui_scheme.h"

#include "include/cef_parser.h"
#include "include/cef_task.h"
#include "include/cef_v8.h"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace asterui::host {

namespace {

int ModifierMask(int modifiers) {
    int result = 0;
    if (modifiers & 2) result |= EVENTFLAG_SHIFT_DOWN;
    if (modifiers & 4) result |= EVENTFLAG_CONTROL_DOWN;
    if (modifiers & 8) result |= EVENTFLAG_ALT_DOWN;
    return result;
}

std::string ConsoleText(const CefString& value) {
    return value.ToString();
}

class RecoveryTask final : public CefTask {
public:
    RecoveryTask(
        std::weak_ptr<std::atomic<bool>> lifetime,
        std::weak_ptr<BrowserState> state,
        std::string reason)
        : lifetime_(std::move(lifetime)),
          state_(std::move(state)),
          reason_(std::move(reason)) {}

    void Execute() override {
        auto lifetime = lifetime_.lock();
        if (!lifetime || !*lifetime) {
            return;
        }
        auto state = state_.lock();
        if (!state || !state->browser) {
            return;
        }
        const uint64_t now = GetTickCount64();
        const uint64_t previous = state->lastRecoveryMs.load();
        if (previous != 0 && now - previous < 3000) {
            return;
        }
        state->lastRecoveryMs = now;
        state->pageBeatMs = now;
        state->loaded = false;
        Log::Warn(
            "browser " + std::to_string(state->id) +
            " automatic recovery: " + reason_);
        state->browser->ReloadIgnoreCache();
    }

private:
    std::weak_ptr<std::atomic<bool>> lifetime_;
    std::weak_ptr<BrowserState> state_;
    std::string reason_;
    IMPLEMENT_REFCOUNTING(RecoveryTask);
};

}  // namespace

void BrowserClient::GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) {
    rect = CefRect(
        0,
        0,
        static_cast<int>(state_->width),
        static_cast<int>(state_->height));
}

bool BrowserClient::GetScreenInfo(
    CefRefPtr<CefBrowser>,
    CefScreenInfo& screenInfo) {
    const CefRect view(
        0,
        0,
        static_cast<int>(state_->width),
        static_cast<int>(state_->height));
    screenInfo.Set(
        1.0f,
        32,
        8,
        false,
        view,
        view);
    if (!loggedScreenInfo_) {
        loggedScreenInfo_ = true;
        Log::Info(
            "browser " + std::to_string(state_->id) +
            " uses 1:1 physical pixels: " +
            std::to_string(state_->width) + "x" +
            std::to_string(state_->height) +
            " device_scale_factor=1.0");
    }
    return true;
}

void BrowserClient::OnPaint(
    CefRefPtr<CefBrowser>,
    PaintElementType type,
    const RectList&,
    const void* buffer,
    int width,
    int height) {
    if (type != PET_VIEW || !buffer || width <= 0 || height <= 0) {
        return;
    }
    if (static_cast<uint32_t>(width) != state_->width ||
        static_cast<uint32_t>(height) != state_->height) {
        if (!loggedMismatch_) {
            loggedMismatch_ = true;
            Log::Warn(
                "CEF paint size mismatch: expected " +
                std::to_string(state_->width) + "x" + std::to_string(state_->height) +
                " got " + std::to_string(width) + "x" + std::to_string(height));
        }
        return;
    }
    if (!state_->frame.Publish(
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            static_cast<const uint8_t*>(buffer),
            width * 4)) {
        Log::Warn("cannot publish browser frame");
    }
}

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    state_->browser = browser;
    state_->pageBeatMs = GetTickCount64();
    manager_->Notify(nlohmann::json{
        {"type", "created"}, {"browser", state_->id}}.dump());
    Log::Info("browser " + std::to_string(state_->id) + " created");
}

void BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser>) {
    manager_->BrowserClosed(state_->id);
}

bool BrowserClient::DoClose(CefRefPtr<CefBrowser>) {
    return false;
}

void BrowserClient::OnLoadStart(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame> frame,
    TransitionType) {
    if (frame && frame->IsMain()) {
        state_->loaded = false;
        manager_->SendLoaded(state_->id, false);
        Log::Info("browser " + std::to_string(state_->id) + " load start");
    }
}

void BrowserClient::OnLoadEnd(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame> frame,
    int httpStatusCode) {
    if (!frame || !frame->IsMain() || httpStatusCode >= 400) {
        return;
    }
    state_->loaded = true;
    state_->pageBeatMs = GetTickCount64();
    manager_->SendLoaded(state_->id, true);
    Log::Info("browser " + std::to_string(state_->id) + " load end");
}

void BrowserClient::OnLoadError(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame> frame,
    ErrorCode errorCode,
    const CefString& errorText,
    const CefString& failedUrl) {
    if (!frame || !frame->IsMain()) {
        return;
    }
    state_->loaded = false;
    const std::string error =
        ConsoleText(errorText) + " (" + std::to_string(static_cast<int>(errorCode)) + ") " +
        ConsoleText(failedUrl);
    manager_->SendLoaded(state_->id, false, error);
    Log::Warn("browser load error: " + error);
}

bool BrowserClient::OnBeforeBrowse(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame>,
    CefRefPtr<CefRequest> request,
    bool,
    bool) {
    if (!request) {
        return true;
    }
    const std::string url = request->GetURL().ToString();
    if (manager_->IsRequestAllowed(url, true)) {
        return false;
    }
    Log::Warn("blocked navigation: " + url);
    return true;
}

CefRefPtr<CefResourceRequestHandler> BrowserClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame>,
    CefRefPtr<CefRequest>,
    bool,
    bool,
    const CefString&,
    bool&) {
    return this;
}

CefResourceRequestHandler::ReturnValue BrowserClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame>,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback>) {
    if (!request) {
        return RV_CANCEL;
    }
    const std::string url = request->GetURL().ToString();
    if (manager_->IsRequestAllowed(url, false)) {
        return RV_CONTINUE;
    }
    Log::Warn("blocked resource: " + url);
    return RV_CANCEL;
}

void BrowserClient::OnRenderProcessTerminated(
    CefRefPtr<CefBrowser>,
    TerminationStatus,
    int errorCode,
    const CefString& errorString) {
    state_->loaded = false;
    manager_->SendLoaded(state_->id, false, ConsoleText(errorString));
    manager_->Notify(nlohmann::json{
        {"type", "crashed"},
        {"browser", state_->id},
        {"error", ConsoleText(errorString)},
        {"code", errorCode}}.dump());
    Log::Error("browser renderer terminated: " + ConsoleText(errorString));
    manager_->ScheduleRecovery(
        state_->id,
        750,
        "renderer terminated (" + std::to_string(errorCode) + ")");
}

bool BrowserClient::OnConsoleMessage(
    CefRefPtr<CefBrowser>,
    cef_log_severity_t level,
    const CefString& message,
    const CefString& source,
    int line) {
    Log::Write(
        static_cast<LogLevel>(std::clamp(static_cast<int>(level), 0, 3)),
        ConsoleText(message) + " [" + ConsoleText(source) + ":" + std::to_string(line) + "]");
    return true;
}

bool BrowserClient::OnProcessMessageReceived(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame>,
    CefProcessId,
    CefRefPtr<CefProcessMessage> message) {
    if (!message || message->GetName() != "asterui_emit") {
        return false;
    }
    auto values = message->GetArgumentList();
    if (!values || values->GetSize() < 2) {
        return true;
    }
    const std::string name = values->GetString(0).ToString();
    if (name == "__asterui.page") {
        state_->pageBeatMs = GetTickCount64();
        return true;
    }
    manager_->Notify(nlohmann::json{
        {"type", "event"},
        {"browser", state_->id},
        {"name", name},
        {"payload", values->GetString(1).ToString()}}.dump());
    return true;
}

bool BrowserManager::Create(const std::string& command) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(command);
    } catch (...) {
        return false;
    }
    const uint32_t id = value.value("browser", 0u);
    const std::string url = value.value("url", "");
    const std::string mapping = value.value("mapping", "");
    const uint32_t width = value.value("width", 0u);
    const uint32_t height = value.value("height", 0u);
    if (id == 0 || url.empty() || mapping.empty() || width == 0 || height == 0) {
        return false;
    }

    auto state = std::make_shared<BrowserState>();
    state->id = id;
    state->url = url;
    state->mapping.assign(mapping.begin(), mapping.end());
    state->width = width;
    state->height = height;
    state->visible = value.value("visible", false);
    state->focused = value.value("focus", false);
    state->layer = value.value("layer", 0);
    state->opacity = value.value("opacity", 1.0f);
    state->createdAtMs = GetTickCount64();
    state->pageBeatMs = state->createdAtMs;
    if (!state->frame.Open(state->mapping)) {
        Log::Error("cannot open shared frame " + mapping);
        return false;
    }

    CefWindowInfo window;
    window.SetAsWindowless(nullptr);
    CefBrowserSettings settings;
    settings.windowless_frame_rate = std::max(1, std::min(maxFps_, 240));
    settings.background_color = 0;

    CefRefPtr<BrowserClient> client = new BrowserClient(this, state);
    browsers_[id] = state;
    clients_[id] = client;
    const std::string resolved = ResolveUrl(url);
    if (!CefBrowserHost::CreateBrowser(
            window,
            client,
            resolved,
            settings,
            nullptr,
            nullptr)) {
        clients_.erase(id);
        browsers_.erase(id);
        return false;
    }
    Log::Info("browser " + std::to_string(id) + " requested: " + resolved);
    return true;
}

bool BrowserManager::UpdateState(const std::string& command) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(command);
    } catch (...) {
        return false;
    }
    auto iterator = browsers_.find(value.value("browser", 0u));
    if (iterator == browsers_.end()) {
        return false;
    }
    auto& state = iterator->second;
    const std::string mapping = value.value("mapping", "");
    const uint32_t width = value.value("width", state->width);
    const uint32_t height = value.value("height", state->height);
    if ((!mapping.empty() && std::wstring(mapping.begin(), mapping.end()) != state->mapping) ||
        width != state->width || height != state->height) {
        state->mapping = mapping.empty() ? state->mapping : std::wstring(mapping.begin(), mapping.end());
        state->width = width;
        state->height = height;
        state->frame.Close();
        if (!state->frame.Open(state->mapping)) {
            Log::Error("cannot reopen shared frame");
            return false;
        }
        if (state->browser && state->browser->GetHost()) {
            state->browser->GetHost()->WasResized();
        }
    }
    const bool wasVisible = state->visible;
    state->visible = value.value("visible", state->visible);
    state->focused = value.value("focus", state->focused);
    state->layer = value.value("layer", state->layer);
    state->opacity = value.value("opacity", state->opacity);
    if (state->browser && state->browser->GetHost()) {
        state->browser->GetHost()->SetFocus(state->focused);
        if (state->visible && !wasVisible) {
            state->pageBeatMs = GetTickCount64();
            state->browser->GetHost()->Invalidate(PET_VIEW);
        }
    }
    return true;
}

bool BrowserManager::Navigate(const std::string& command) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(command);
    } catch (...) {
        return false;
    }
    auto iterator = browsers_.find(value.value("browser", 0u));
    if (iterator == browsers_.end() || !iterator->second->browser) {
        return false;
    }
    iterator->second->url = value.value("url", "ui/index.html");
    iterator->second->browser->GetMainFrame()->LoadURL(ResolveUrl(iterator->second->url));
    return true;
}

bool BrowserManager::SendEvent(const std::string& command) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(command);
    } catch (...) {
        return false;
    }
    auto iterator = browsers_.find(value.value("browser", 0u));
    if (iterator == browsers_.end() || !iterator->second->browser) {
        return false;
    }
    DispatchToJs(
        iterator->second,
        value.value("name", ""),
        value.value("payload", "{}"));
    return true;
}

bool BrowserManager::Input(const std::string& command) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(command);
    } catch (...) {
        return false;
    }
    auto iterator = browsers_.find(value.value("browser", 0u));
    if (iterator == browsers_.end() || !iterator->second->browser ||
        !iterator->second->focused) {
        return false;
    }
    const auto host = iterator->second->browser->GetHost();
    const std::string kind = value.value("kind", "");
    if (kind == "key" || kind == "char") {
        CefKeyEvent event;
        if (kind == "char") {
            event.type = KEYEVENT_CHAR;
            event.windows_key_code = value.value("charCode", 0);
            event.character = static_cast<char16_t>(value.value("charCode", 0));
        } else {
            const bool up = value.value("up", false);
            event.type = up ? KEYEVENT_KEYUP : KEYEVENT_RAWKEYDOWN;
            event.windows_key_code = value.value("keyCode", 0);
            event.native_key_code = value.value("scanCode", 0);
        }
        event.modifiers = ModifierMask(value.value("mods", 0));
        host->SendKeyEvent(event);
        return true;
    }

    CefMouseEvent event;
    event.x = value.value("x", 0);
    event.y = value.value("y", 0);
    event.modifiers = ModifierMask(value.value("mods", 0));
    if (kind == "mouseMove") {
        host->SendMouseMoveEvent(event, false);
        return true;
    }
    if (kind == "mouseButton") {
        CefBrowserHost::MouseButtonType button = MBT_LEFT;
        const int number = value.value("button", 0);
        if (number == 1) button = MBT_MIDDLE;
        if (number == 2) button = MBT_RIGHT;
        host->SendMouseClickEvent(
            event,
            button,
            value.value("up", false),
            value.value("clicks", 1));
        return true;
    }
    if (kind == "wheel") {
        host->SendMouseWheelEvent(
            event,
            value.value("dx", 0),
            value.value("dy", 0));
        return true;
    }
    return false;
}

bool BrowserManager::Destroy(const std::string& command) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(command);
    } catch (...) {
        return false;
    }
    const uint32_t id = value.value("browser", 0u);
    auto iterator = browsers_.find(id);
    if (iterator == browsers_.end()) {
        return false;
    }
    if (iterator->second->browser && iterator->second->browser->GetHost()) {
        iterator->second->browser->GetHost()->CloseBrowser(true);
    }
    clients_.erase(id);
    browsers_.erase(id);
    return true;
}

void BrowserManager::CloseAll() {
    auto browsers = browsers_;
    browsers_.clear();
    clients_.clear();
    for (const auto& pair : browsers) {
        if (pair.second->browser && pair.second->browser->GetHost()) {
            pair.second->browser->GetHost()->CloseBrowser(true);
        }
    }
}

void BrowserManager::Notify(const std::string& json) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (sender_) {
        sender_(json);
    }
}

void BrowserManager::BrowserClosed(uint32_t id) {
    clients_.erase(id);
    browsers_.erase(id);
    Log::Info("browser " + std::to_string(id) + " closed");
}

void BrowserManager::SendLoaded(uint32_t id, bool loaded, const std::string& error) {
    nlohmann::json value = {
        {"type", "loaded"},
        {"browser", id},
        {"loaded", loaded}};
    if (!error.empty()) {
        value["error"] = error;
    }
    Notify(value.dump());
}

void BrowserManager::CheckHealth() {
    const uint64_t now = GetTickCount64();
    for (const auto& pair : browsers_) {
        const auto& state = pair.second;
        if (!state || !state->visible || !state->loaded || !state->browser) {
            continue;
        }
        const uint64_t lastBeat = state->pageBeatMs.load();
        const uint64_t lastRecovery = state->lastRecoveryMs.load();
        const uint64_t reference = lastBeat != 0 ? lastBeat : state->createdAtMs;
        if (reference == 0 || now - reference <= 15000) {
            continue;
        }
        if (lastRecovery != 0 && now - lastRecovery < 5000) {
            continue;
        }
        ScheduleRecovery(
            state->id,
            0,
            "page heartbeat timed out after " + std::to_string(now - reference) + " ms");
    }
}

void BrowserManager::ScheduleRecovery(
    uint32_t id,
    int delayMs,
    const std::string& reason) {
    auto iterator = browsers_.find(id);
    if (iterator == browsers_.end()) {
        return;
    }
    CefPostDelayedTask(
        TID_UI,
        new RecoveryTask(lifetime_, iterator->second, reason),
        std::max(0, delayMs));
}

BrowserManager::~BrowserManager() {
    lifetime_->store(false);
}

size_t BrowserManager::Count() {
    return browsers_.size();
}

bool BrowserManager::IsRequestAllowed(
    const std::string& url,
    bool isNavigation) const {
    return requestPolicy_.IsAllowed(url, isNavigation);
}

void BrowserManager::DispatchToJs(
    const std::shared_ptr<BrowserState>& state,
    const std::string& name,
    const std::string& payload) {
    if (!state || !state->browser) {
        return;
    }
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(payload);
    } catch (...) {
        value = nlohmann::json::object();
    }
    const std::string script =
        "window.asterui&&window.asterui.__dispatch(" +
        nlohmann::json(name).dump() + "," + value.dump() + ");";
    state->browser->GetMainFrame()->ExecuteJavaScript(script, "", 0);
}

std::string BrowserManager::ResolveUrl(const std::string& url) const {
    return NormalizePageUrl(uiRoot_, url);
}

}  // namespace asterui::host
