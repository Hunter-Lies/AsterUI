#pragma once

#include "common/protocol.h"
#include "common/shared_frame.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace asterui {

using EventCallback = ::AsterEventCallback;

struct BrowserRecord {
    uint32_t id = 0;
    std::string url;
    uint32_t width = 0;
    uint32_t height = 0;
    bool transparent = true;
    bool visible = false;
    bool focused = false;
    bool loaded = false;
    int32_t layer = 0;
    float opacity = 1.0f;
    uint32_t mappingGeneration = 0;
    std::wstring mappingName;
    std::shared_ptr<SharedFrame> frame;
};

struct RenderBrowser {
    uint32_t id = 0;
    int32_t layer = 0;
    float opacity = 1.0f;
    std::shared_ptr<BrowserRecord> record;
};

namespace api {

void Initialize(HMODULE module);
void Shutdown();
void Tick();

uint32_t CreateBrowser(const BrowserDesc* description);
int DestroyBrowser(uint32_t browser);
int SetVisible(uint32_t browser, int visible);
int SetFocus(uint32_t browser, int focused);
int SetLayer(uint32_t browser, int layer);
int SetOpacity(uint32_t browser, float opacity);
int Resize(uint32_t browser, int width, int height);
int Navigate(uint32_t browser, const char* url);
int SendEvent(uint32_t browser, const char* name, const char* payload);
uint64_t AddListener(uint32_t browser, const char* name, EventCallback callback, void* user);
int RemoveListener(uint64_t listener);
int IsLoaded(uint32_t browser);
int IsHostConnected();
void HostLog(int level, const char* message);

std::vector<RenderBrowser> VisibleBrowsers();
uint32_t FocusedBrowserId();
std::shared_ptr<BrowserRecord> FindBrowser(uint32_t id);
std::shared_ptr<SharedFrame> FrameFor(uint32_t id);

void OnHostReady();
void OnHostDisconnected();
void HandleHostMessage(const std::string& json);
void SetHostConnected(bool connected);

}  // namespace api
}  // namespace asterui
