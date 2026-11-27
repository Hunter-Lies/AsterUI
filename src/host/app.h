#pragma once

#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"

namespace asterui::host {

class AsterUiApp final : public CefApp,
                        public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }

    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
    void OnContextCreated(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override;

private:
    IMPLEMENT_REFCOUNTING(AsterUiApp);
};

}  // namespace asterui::host
