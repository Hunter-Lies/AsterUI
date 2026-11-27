#include "asi/api.h"

#include "asi/supervisor.h"
#include "common/log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace asterui::api {

namespace {

struct Listener {
    uint32_t browser = 0;
    std::string name;
    EventCallback callback = nullptr;
    void* user = nullptr;
};

struct Event {
    uint32_t browser = 0;
    std::string name;
    std::string payload;
};

std::mutex g_mutex;
std::unordered_map<uint32_t, std::shared_ptr<BrowserRecord>> g_browsers;
std::unordered_map<uint64_t, Listener> g_listeners;
std::deque<Event> g_events;
uint32_t g_nextBrowser = 1;
uint64_t g_nextListener = 1;
bool g_hostConnected = false;
HMODULE g_module = nullptr;
std::atomic<int> g_supervisorState = 0;

bool ValidName(const char* value) {
    if (!value) {
        return false;
    }
    const std::string name(value);
    if (name.empty() || name.size() > 64) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '.' || c == ':' || c == '-';
    });
}

std::wstring MappingName(uint32_t browser, uint32_t generation) {
    wchar_t buffer[128] = {};
    std::swprintf(
        buffer,
        std::size(buffer),
        L"Local\\AsterUI.%lu.%lu.%lu",
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(browser),
        static_cast<unsigned long>(generation));
    return buffer;
}

std::string MappingNameUtf8(const std::wstring& value) {
    std::string result(value.begin(), value.end());
    return result;
}

nlohmann::json BrowserJson(const BrowserRecord& browser) {
    return {
        {"browser", browser.id},
        {"url", browser.url},
        {"width", browser.width},
        {"height", browser.height},
        {"transparent", browser.transparent},
        {"visible", browser.visible},
        {"focus", browser.focused},
        {"layer", browser.layer},
        {"opacity", browser.opacity},
        {"mapping", MappingNameUtf8(browser.mappingName)},
    };
}

void SendCreate(const BrowserRecord& browser) {
    auto value = BrowserJson(browser);
    value["type"] = "create";
    supervisor::Send(value.dump());
}

void SendState(const BrowserRecord& browser) {
    if (!supervisor::IsReady()) {
        return;
    }
    auto value = BrowserJson(browser);
    value["type"] = "state";
    supervisor::Send(value.dump());
}

void DispatchEvents() {
    std::deque<Event> events;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        events.swap(g_events);
    }
    for (const Event& event : events) {
        std::vector<Listener> listeners;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (const auto& pair : g_listeners) {
                if ((pair.second.browser == 0 || pair.second.browser == event.browser) &&
                    pair.second.name == event.name) {
                    listeners.push_back(pair.second);
                }
            }
        }
        for (const Listener& listener : listeners) {
            if (listener.callback) {
                listener.callback(
                    event.browser,
                    event.name.c_str(),
                    event.payload.c_str(),
                    listener.user);
            }
        }
    }
}

}  // namespace

void Initialize(HMODULE module) {
    g_module = module;
}

void Shutdown() {
    supervisor::Stop();
    g_supervisorState = 0;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_browsers.clear();
    g_listeners.clear();
    g_events.clear();
    g_hostConnected = false;
}

void Tick() {
    int expected = 0;
    if (g_module && g_supervisorState.compare_exchange_strong(expected, 1)) {
        supervisor::Start(g_module);
        g_supervisorState = 2;
    }
    supervisor::Tick();
    DispatchEvents();
}

uint32_t CreateBrowser(const BrowserDesc* description) {
    if (!description || description->structSize < 28 || !description->url ||
        description->width <= 0 || description->height <= 0 ||
        description->width > 16384 || description->height > 16384) {
        return 0;
    }
    const std::string url(description->url);
    if (url.empty() || url.size() > 2048) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_browsers.size() >= supervisor::MaxBrowsers()) {
        return 0;
    }
    const uint32_t id = g_nextBrowser++;
    auto browser = std::make_shared<BrowserRecord>();
    browser->id = id;
    browser->url = url;
    browser->width = static_cast<uint32_t>(description->width);
    browser->height = static_cast<uint32_t>(description->height);
    browser->transparent = description->transparent != 0;
    browser->mappingGeneration = 0;
    browser->mappingName = MappingName(id, browser->mappingGeneration);
    browser->frame = std::make_shared<SharedFrame>();
    if (!browser->frame->Create(browser->mappingName, id, browser->width, browser->height)) {
        Log::Error("CreateBrowser: shared frame allocation failed");
        return 0;
    }
    g_browsers.emplace(id, browser);
    if (supervisor::IsReady()) {
        SendCreate(*browser);
    }
    Log::Info("browser " + std::to_string(id) + " allocated: " + url);
    return id;
}

int DestroyBrowser(uint32_t browser) {
    std::shared_ptr<BrowserRecord> removed;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        removed = iterator->second;
        g_browsers.erase(iterator);
        for (auto listener = g_listeners.begin(); listener != g_listeners.end();) {
            listener = listener->second.browser == browser
                ? g_listeners.erase(listener)
                : std::next(listener);
        }
    }
    if (supervisor::IsReady()) {
        supervisor::Send(nlohmann::json{{"type", "destroy"}, {"browser", browser}}.dump());
    }
    Log::Info("browser " + std::to_string(browser) + " destroyed");
    return 1;
}

int SetVisible(uint32_t browser, int visible) {
    std::shared_ptr<BrowserRecord> record;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        record = iterator->second;
        record->visible = visible != 0;
        if (record->frame && record->frame->Header()) {
            InterlockedExchange(&record->frame->Header()->visible, record->visible ? 1 : 0);
        }
    }
    SendState(*record);
    return 1;
}

int SetFocus(uint32_t browser, int focused) {
    std::shared_ptr<BrowserRecord> record;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        for (auto& pair : g_browsers) {
            const bool value = focused != 0 && pair.first == browser;
            if (pair.second->focused != value) {
                pair.second->focused = value;
                if (pair.second->frame && pair.second->frame->Header()) {
                    InterlockedExchange(&pair.second->frame->Header()->focused, value ? 1 : 0);
                }
            }
        }
        record = iterator->second;
    }
    SendState(*record);
    return 1;
}

int SetLayer(uint32_t browser, int layer) {
    std::shared_ptr<BrowserRecord> record;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        record = iterator->second;
        record->layer = layer;
        if (record->frame && record->frame->Header()) {
            InterlockedExchange(&record->frame->Header()->layer, layer);
        }
    }
    SendState(*record);
    return 1;
}

int SetOpacity(uint32_t browser, float opacity) {
    std::shared_ptr<BrowserRecord> record;
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        record = iterator->second;
        record->opacity = opacity;
        if (record->frame && record->frame->Header()) {
            InterlockedExchange(&record->frame->Header()->opacityBits, FloatBits(opacity));
        }
    }
    SendState(*record);
    return 1;
}

int Resize(uint32_t browser, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        return 0;
    }
    std::shared_ptr<BrowserRecord> record;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        record = iterator->second;
        const uint32_t nextGeneration = record->mappingGeneration + 1;
        const std::wstring nextName = MappingName(record->id, nextGeneration);
        auto nextFrame = std::make_shared<SharedFrame>();
        if (!nextFrame->Create(
                nextName,
                record->id,
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height))) {
            return 0;
        }
        nextFrame->Header()->visible = record->visible ? 1 : 0;
        nextFrame->Header()->focused = record->focused ? 1 : 0;
        nextFrame->Header()->layer = record->layer;
        nextFrame->Header()->opacityBits = FloatBits(record->opacity);
        record->mappingGeneration = nextGeneration;
        record->mappingName = nextName;
        record->width = static_cast<uint32_t>(width);
        record->height = static_cast<uint32_t>(height);
        record->frame = nextFrame;
    }
    SendState(*record);
    return 1;
}

int Navigate(uint32_t browser, const char* url) {
    if (!url || !*url) {
        return 0;
    }
    std::shared_ptr<BrowserRecord> record;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto iterator = g_browsers.find(browser);
        if (iterator == g_browsers.end()) {
            return 0;
        }
        record = iterator->second;
        record->url = url;
        record->loaded = false;
    }
    if (supervisor::IsReady()) {
        supervisor::Send(nlohmann::json{
            {"type", "navigate"}, {"browser", browser}, {"url", record->url}}.dump());
    }
    return 1;
}

int SendEvent(uint32_t browser, const char* name, const char* payload) {
    if (!ValidName(name)) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_browsers.find(browser) == g_browsers.end()) {
            return 0;
        }
    }
    if (!supervisor::IsReady()) {
        return 0;
    }
    return supervisor::Send(nlohmann::json{
        {"type", "event"},
        {"browser", browser},
        {"name", name},
        {"payload", payload ? payload : "{}"}}.dump()) ? 1 : 0;
}

uint64_t AddListener(
    uint32_t browser,
    const char* name,
    EventCallback callback,
    void* user) {
    if (!ValidName(name) || !callback) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (browser != 0 && g_browsers.find(browser) == g_browsers.end()) {
        return 0;
    }
    const uint64_t handle = g_nextListener++;
    g_listeners.emplace(handle, Listener{browser, name, callback, user});
    return handle;
}

int RemoveListener(uint64_t listener) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_listeners.erase(listener) != 0 ? 1 : 0;
}

int IsLoaded(uint32_t browser) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto iterator = g_browsers.find(browser);
    return iterator != g_browsers.end() && iterator->second->loaded ? 1 : 0;
}

int IsHostConnected() {
    return supervisor::IsReady() ? 1 : 0;
}

void HostLog(int level, const char* message) {
    if (!message) {
        return;
    }
    Log::Write(
        static_cast<LogLevel>(std::clamp(level, 0, 3)),
        std::string("[host] ") + message);
}

std::vector<RenderBrowser> VisibleBrowsers() {
    std::vector<RenderBrowser> result;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& pair : g_browsers) {
        if (pair.second->visible && pair.second->frame && pair.second->frame->IsOpen()) {
            result.push_back({
                pair.first,
                pair.second->layer,
                pair.second->opacity,
                pair.second});
        }
    }
    std::sort(result.begin(), result.end(), [](const RenderBrowser& a, const RenderBrowser& b) {
        return a.layer != b.layer ? a.layer < b.layer : a.id < b.id;
    });
    return result;
}

uint32_t FocusedBrowserId() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& pair : g_browsers) {
        if (pair.second->visible && pair.second->focused) {
            return pair.first;
        }
    }
    return 0;
}

std::shared_ptr<BrowserRecord> FindBrowser(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto iterator = g_browsers.find(id);
    return iterator == g_browsers.end() ? nullptr : iterator->second;
}

std::shared_ptr<SharedFrame> FrameFor(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto iterator = g_browsers.find(id);
    return iterator == g_browsers.end() ? nullptr : iterator->second->frame;
}

void OnHostReady() {
    std::vector<std::shared_ptr<BrowserRecord>> browsers;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& pair : g_browsers) {
            browsers.push_back(pair.second);
        }
    }
    for (const auto& browser : browsers) {
        SendCreate(*browser);
        SendState(*browser);
    }
    Log::Info("host ready; browser state replayed");
}

void OnHostDisconnected() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& pair : g_browsers) {
        pair.second->loaded = false;
    }
    Log::Warn("host disconnected");
}

void SetHostConnected(bool connected) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        changed = g_hostConnected != connected;
        g_hostConnected = connected;
    }
    if (changed) {
        Log::Info(connected ? "host connected" : "host disconnected");
    }
}

void HandleHostMessage(const std::string& json) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(json);
    } catch (const std::exception& exception) {
        Log::Warn(std::string("invalid host message: ") + exception.what());
        return;
    }
    const std::string type = value.value("type", "");
    if (type == "ready") {
        OnHostReady();
        return;
    }
    if (type == "log") {
        HostLog(value.value("level", 1), value.value("message", "").c_str());
        return;
    }
    if (type == "event") {
        const uint32_t browser = value.value("browser", 0u);
        const std::string name = value.value("name", "");
        const std::string payload = value.value("payload", "{}");
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_events.size() < 512) {
                g_events.push_back({browser, name, payload});
            }
        }
        return;
    }
    if (type == "loaded") {
        auto browser = FindBrowser(value.value("browser", 0u));
        if (browser) {
            browser->loaded = value.value("loaded", false);
        }
        return;
    }
    if (type == "crashed") {
        auto browser = FindBrowser(value.value("browser", 0u));
        if (browser) {
            browser->loaded = false;
        }
        Log::Warn("browser " + std::to_string(value.value("browser", 0u)) + " crashed");
    }
}

}  // namespace asterui::api

namespace {

using namespace asterui;

struct ApiTable {
    int32_t size;
    uint32_t reserved;
    void* functions[14];
};

ApiTable g_api = {
    static_cast<int32_t>(sizeof(ApiTable)),
    0,
    {
        reinterpret_cast<void*>(&api::CreateBrowser),
        reinterpret_cast<void*>(&api::DestroyBrowser),
        reinterpret_cast<void*>(&api::SetVisible),
        reinterpret_cast<void*>(&api::SetFocus),
        reinterpret_cast<void*>(&api::SetLayer),
        reinterpret_cast<void*>(&api::SetOpacity),
        reinterpret_cast<void*>(&api::Resize),
        reinterpret_cast<void*>(&api::Navigate),
        reinterpret_cast<void*>(&api::SendEvent),
        reinterpret_cast<void*>(&api::AddListener),
        reinterpret_cast<void*>(&api::RemoveListener),
        reinterpret_cast<void*>(&api::IsLoaded),
        reinterpret_cast<void*>(&api::IsHostConnected),
        reinterpret_cast<void*>(&api::HostLog),
    },
};

static_assert(sizeof(ApiTable) == kApiBytes, "API table ABI changed");

}  // namespace

extern "C" __declspec(dllexport) void* AsterUI_GetApi(uint32_t version) {
    return version == asterui::kApiVersion ? &g_api : nullptr;
}
