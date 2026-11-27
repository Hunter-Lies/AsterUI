#pragma once

#include "common/shared_frame.h"
#include "host/request_policy.h"

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_render_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace asterui::host {

struct BrowserState {
    uint32_t id = 0;
    std::string url;
    std::wstring mapping;
    uint32_t width = 0;
    uint32_t height = 0;
    bool visible = false;
    bool focused = false;
    int layer = 0;
    float opacity = 1.0f;
    bool loaded = false;
    std::atomic<uint64_t> pageBeatMs = 0;
    std::atomic<uint64_t> lastRecoveryMs = 0;
    uint64_t createdAtMs = 0;
    SharedFrame frame;
    CefRefPtr<CefBrowser> browser;
};

class BrowserManager;

class BrowserClient final : public CefClient,
                            public CefLifeSpanHandler,
                            public CefLoadHandler,
                            public CefRenderHandler,
                            public CefRequestHandler,
                            public CefDisplayHandler,
                            public CefResourceRequestHandler {
public:
    BrowserClient(BrowserManager* manager, std::shared_ptr<BrowserState> state)
        : manager_(manager), state_(std::move(state)) {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool isNavigation,
        bool isDownload,
        const CefString& requestInitiator,
        bool& disableDefaultHandling) override;
    CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback) override;

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(
        CefRefPtr<CefBrowser> browser,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int width,
        int height) override;

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;

    void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transitionType) override;
    void OnLoadEnd(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int httpStatusCode) override;
    void OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString& errorText,
        const CefString& failedUrl) override;

    bool OnBeforeBrowse(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool userGesture,
        bool isRedirect) override;
    void OnRenderProcessTerminated(
        CefRefPtr<CefBrowser> browser,
        TerminationStatus status,
        int errorCode,
        const CefString& errorString) override;

    bool OnConsoleMessage(
        CefRefPtr<CefBrowser> browser,
        cef_log_severity_t level,
        const CefString& message,
        const CefString& source,
        int line) override;

    bool OnProcessMessageReceived(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId sourceProcess,
        CefRefPtr<CefProcessMessage> message) override;

private:
    BrowserManager* manager_ = nullptr;
    std::shared_ptr<BrowserState> state_;
    bool loggedMismatch_ = false;
    IMPLEMENT_REFCOUNTING(BrowserClient);
};

class BrowserManager {
public:
    using Sender = std::function<void(const std::string&)>;

    BrowserManager(
        std::string uiRoot,
        int maxFps,
        std::vector<std::string> developmentOrigins,
        Sender sender)
        : uiRoot_(std::move(uiRoot)),
          maxFps_(maxFps),
          requestPolicy_(uiRoot_, developmentOrigins),
          sender_(std::move(sender)) {}
    ~BrowserManager();

    bool Create(const std::string& command);
    bool UpdateState(const std::string& command);
    bool Navigate(const std::string& command);
    bool SendEvent(const std::string& command);
    bool Input(const std::string& command);
    bool Destroy(const std::string& command);
    void CloseAll();

    void Notify(const std::string& json);
    void BrowserClosed(uint32_t id);
    void SendLoaded(uint32_t id, bool loaded, const std::string& error = {});
    void CheckHealth();
    void ScheduleRecovery(uint32_t id, int delayMs, const std::string& reason);

    size_t Count();
    bool IsRequestAllowed(const std::string& url, bool isNavigation) const;

private:
    void DispatchToJs(
        const std::shared_ptr<BrowserState>& state,
        const std::string& name,
        const std::string& payload);
    std::string ResolveUrl(const std::string& url) const;

    std::string uiRoot_;
    int maxFps_ = 60;
    RequestPolicy requestPolicy_;
    Sender sender_;
    std::unordered_map<uint32_t, std::shared_ptr<BrowserState>> browsers_;
    std::unordered_map<uint32_t, CefRefPtr<BrowserClient>> clients_;
    std::shared_ptr<std::atomic<bool>> lifetime_ =
        std::make_shared<std::atomic<bool>>(true);
    std::mutex sendMutex_;
};

}  // namespace asterui::host
