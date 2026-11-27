#include "asi/api.h"
#include "asi/input.h"
#include "asi/renderer.h"
#include "common/log.h"

#include "main.h"

#include <windows.h>

namespace {

HMODULE g_module = nullptr;

void OnPresent(void* swapChain) {
    // Present is an independent lifecycle entry. Keep the supervisor alive
    // even if the ScriptHookV script thread is delayed or not started.
    asterui::api::Tick();
    asterui::input::EnsureWindow(swapChain);
    asterui::renderer::Present(swapChain);
}

void ScriptMain() {
    while (true) {
        asterui::api::Tick();
        scriptWait(16);
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        asterui::Log::Configure(module, L"AsterUI/AsterUI.log", asterui::LogLevel::Info);
        asterui::Log::Info("AsterUI ASI attached");
        presentCallbackRegister(&OnPresent);
        scriptRegister(module, &ScriptMain);
        asterui::api::Initialize(module);
    } else if (reason == DLL_PROCESS_DETACH) {
        // Windows passes a non-null reserved value during process termination.
        // ScriptHookV and the job object own teardown in that case. Touching
        // callbacks, worker threads, CEF, D3D, or logging under the loader lock
        // can fail-fast before the game exits.
        if (reserved != nullptr) {
            return TRUE;
        }

        presentCallbackUnregister(&OnPresent);
        scriptUnregister(module);
        asterui::input::Shutdown();
        asterui::renderer::Shutdown();
        asterui::api::Shutdown();
        asterui::Log::Info("AsterUI ASI detached");
    }
    return TRUE;
}
