#include "asi/input.h"

#include "asi/api.h"
#include "asi/supervisor.h"
#include "common/log.h"

#include <nlohmann/json.hpp>

#include <dxgi.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace asterui::input {

namespace {

enum class EventKind {
    MouseMove,
    MouseButton,
    Wheel,
    Key,
    Char,
};

struct InputEvent {
    EventKind kind = EventKind::MouseMove;
    uint32_t browser = 0;
    int x = 0;
    int y = 0;
    int button = 0;
    int dx = 0;
    int dy = 0;
    int keyCode = 0;
    int scanCode = 0;
    int charCode = 0;
    int mods = 0;
    bool up = false;
};

int Modifiers();
bool Enqueue(InputEvent event);

HWND g_window = nullptr;
WNDPROC g_original = nullptr;
bool g_installed = false;
std::mutex g_queueMutex;
std::condition_variable g_queueChanged;
std::deque<InputEvent> g_queue;
// Heap-owned so CRT process detach never destroys a joinable thread.
std::thread* g_sender = nullptr;
bool g_senderRunning = false;
bool g_mouseDown[3] = {false, false, false};
uint32_t g_captureBrowser = 0;
int g_captureX = 0;
int g_captureY = 0;

constexpr size_t kMaxQueuedInput = 512;

int ScaleCoordinate(int value, int sourceSize, int targetSize) {
    if (sourceSize <= 0 || targetSize <= 0) {
        return 0;
    }
    value = std::max(0, std::min(sourceSize - 1, value));
    return static_cast<int>(
        static_cast<int64_t>(value) * targetSize / sourceSize);
}

void SendCapturedMouseUps() {
    if (!g_captureBrowser) {
        return;
    }
    for (int button = 0; button < 3; ++button) {
        if (!g_mouseDown[button]) {
            continue;
        }
        Enqueue({
            EventKind::MouseButton,
            g_captureBrowser,
            g_captureX,
            g_captureY,
            button, 0, 0, 0, 0, 0, Modifiers(), true});
        g_mouseDown[button] = false;
    }
    g_captureBrowser = 0;
}

int Modifiers() {
    int result = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) result |= 2;
    if (GetKeyState(VK_CONTROL) & 0x8000) result |= 4;
    if (GetKeyState(VK_MENU) & 0x8000) result |= 8;
    return result;
}

bool Enqueue(InputEvent event) {
    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (!g_senderRunning) {
        return false;
    }
    if (event.kind == EventKind::MouseMove &&
        !g_queue.empty() &&
        g_queue.back().kind == EventKind::MouseMove &&
        g_queue.back().browser == event.browser) {
        g_queue.back() = event;
        g_queueChanged.notify_one();
        return true;
    }
    if (event.kind == EventKind::MouseButton) {
        if (!event.up) {
            g_queue.erase(
                std::remove_if(
                    g_queue.begin(),
                    g_queue.end(),
                    [&event](const InputEvent& queued) {
                        return queued.kind == EventKind::MouseMove &&
                            queued.browser == event.browser;
                    }),
                g_queue.end());
        }
        auto firstMove = std::find_if(
            g_queue.begin(),
            g_queue.end(),
            [](const InputEvent& queued) {
                return queued.kind == EventKind::MouseMove;
            });
        g_queue.insert(firstMove, event);
        g_queueChanged.notify_one();
        return true;
    }
    if (g_queue.size() >= kMaxQueuedInput) {
        g_queue.pop_front();
    }
    g_queue.push_back(event);
    g_queueChanged.notify_one();
    return true;
}

void SenderLoop() {
    for (;;) {
        InputEvent event;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueChanged.wait(lock, []() {
                return !g_senderRunning || !g_queue.empty();
            });
            if (!g_senderRunning && g_queue.empty()) {
                return;
            }
            event = g_queue.front();
            g_queue.pop_front();
        }
        if (!supervisor::IsReady()) {
            continue;
        }

        nlohmann::json value = {{"type", "input"}, {"browser", event.browser}};
        switch (event.kind) {
        case EventKind::MouseMove:
            value["kind"] = "mouseMove";
            value["x"] = event.x;
            value["y"] = event.y;
            value["mods"] = event.mods;
            break;
        case EventKind::MouseButton:
            value["kind"] = "mouseButton";
            value["x"] = event.x;
            value["y"] = event.y;
            value["button"] = event.button;
            value["up"] = event.up;
            value["clicks"] = 1;
            value["mods"] = event.mods;
            break;
        case EventKind::Wheel:
            value["kind"] = "wheel";
            value["x"] = event.x;
            value["y"] = event.y;
            value["dx"] = event.dx;
            value["dy"] = event.dy;
            value["mods"] = event.mods;
            break;
        case EventKind::Key:
            value["kind"] = "key";
            value["keyCode"] = event.keyCode;
            value["scanCode"] = event.scanCode;
            value["up"] = event.up;
            value["mods"] = event.mods;
            break;
        case EventKind::Char:
            value["kind"] = "char";
            value["charCode"] = event.charCode;
            value["mods"] = event.mods;
            break;
        }
        supervisor::Send(value.dump());
    }
}

bool StartSender() {
    if (g_senderRunning) {
        return true;
    }
    g_senderRunning = true;
    try {
        g_sender = new std::thread(SenderLoop);
        SetThreadPriority(g_sender->native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    } catch (const std::exception& exception) {
        g_senderRunning = false;
        Log::Error(std::string("input sender failed: ") + exception.what());
        return false;
    }
    return true;
}

LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    if (message == WM_KILLFOCUS) {
        SendCapturedMouseUps();
        return CallWindowProcW(g_original, window, message, wParam, lParam);
    }
    const uint32_t browser = api::FocusedBrowserId();
    const auto record = browser ? api::FindBrowser(browser) : nullptr;
    if (!record || !supervisor::IsReady()) {
        return CallWindowProcW(g_original, window, message, wParam, lParam);
    }
    if (message == WM_SETCURSOR) {
        SetCursor(nullptr);
        return TRUE;
    }

    RECT client = {};
    GetClientRect(window, &client);
    const int width = std::max(1L, client.right);
    const int height = std::max(1L, client.bottom);
    const int browserWidth = std::max(1u, record->width);
    const int browserHeight = std::max(1u, record->height);
    const int x = ScaleCoordinate(GET_X_LPARAM(lParam), width, browserWidth);
    const int y = ScaleCoordinate(GET_Y_LPARAM(lParam), height, browserHeight);
    if (message == WM_MOUSEMOVE) {
        if (g_captureBrowser == browser) {
            g_captureX = x;
            g_captureY = y;
        }
        return Enqueue({
            EventKind::MouseMove,
            browser,
            x,
            y,
            0, 0, 0, 0, 0, 0, Modifiers()}) ? 0 : CallWindowProcW(
                g_original, window, message, wParam, lParam);
    }
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
        message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ||
        message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) {
        int button = 0;
        if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) button = 2;
        if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) button = 1;
        const bool up =
            message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP;
        if (!up) {
            g_mouseDown[button] = true;
            g_captureBrowser = browser;
            g_captureX = x;
            g_captureY = y;
        } else {
            g_mouseDown[button] = false;
            g_captureX = x;
            g_captureY = y;
        }
        return Enqueue({
            EventKind::MouseButton,
            browser,
            x,
            y,
            button, 0, 0, 0, 0, 0, Modifiers(), up}) ? 0 : CallWindowProcW(
                g_original, window, message, wParam, lParam);
    }
    if (message == WM_MOUSEWHEEL) {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(window, &point);
        return Enqueue({
            EventKind::Wheel,
            browser,
            ScaleCoordinate(point.x, width, browserWidth),
            ScaleCoordinate(point.y, height, browserHeight),
            0,
            0,
            GET_WHEEL_DELTA_WPARAM(wParam),
            0, 0, 0,
            Modifiers()}) ? 0 : CallWindowProcW(
                g_original, window, message, wParam, lParam);
    }
    if (message == WM_MOUSEHWHEEL) {
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(window, &point);
        return Enqueue({
            EventKind::Wheel,
            browser,
            ScaleCoordinate(point.x, width, browserWidth),
            ScaleCoordinate(point.y, height, browserHeight),
            0,
            GET_WHEEL_DELTA_WPARAM(wParam),
            0,
            0, 0, 0,
            Modifiers()}) ? 0 : CallWindowProcW(
                g_original, window, message, wParam, lParam);
    }
    if (message == WM_KEYDOWN || message == WM_KEYUP ||
        message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) {
        const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
        return Enqueue({
            EventKind::Key,
            browser,
            0, 0, 0, 0, 0,
            static_cast<int>(wParam),
            static_cast<int>((lParam >> 16) & 0xff),
            0,
            Modifiers(),
            up}) ? 0 : CallWindowProcW(
                g_original, window, message, wParam, lParam);
    }
    if (message == WM_CHAR) {
        return Enqueue({
            EventKind::Char,
            browser,
            0, 0, 0, 0, 0, 0, 0,
            static_cast<int>(wParam),
            Modifiers()}) ? 0 : CallWindowProcW(
                g_original, window, message, wParam, lParam);
    }
    return CallWindowProcW(g_original, window, message, wParam, lParam);
}

}  // namespace

void EnsureWindow(void* swapChainPointer) {
    if (g_installed || !swapChainPointer) {
        return;
    }
    auto* swapChain = static_cast<IDXGISwapChain*>(swapChainPointer);
    DXGI_SWAP_CHAIN_DESC description = {};
    if (FAILED(swapChain->GetDesc(&description)) || !description.OutputWindow) {
        return;
    }
    g_window = description.OutputWindow;
    g_original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(
            g_window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (g_original) {
        if (StartSender()) {
            g_installed = true;
            Log::Info("input hook installed");
        } else {
            SetWindowLongPtrW(
                g_window,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(g_original));
            g_original = nullptr;
        }
    } else {
        Log::Warn("input hook failed");
    }
}

void Shutdown() {
    if (g_installed && g_window && g_original) {
        SetWindowLongPtrW(
            g_window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(g_original));
    }
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_senderRunning = false;
        g_queue.clear();
    }
    g_captureBrowser = 0;
    g_mouseDown[0] = g_mouseDown[1] = g_mouseDown[2] = false;
    g_queueChanged.notify_all();
    if (g_sender) {
        if (g_sender->joinable()) {
            g_sender->join();
        }
        delete g_sender;
        g_sender = nullptr;
    }
    g_window = nullptr;
    g_original = nullptr;
    g_installed = false;
}

}  // namespace asterui::input
