#include "common/pipe.h"
#include "common/shared_frame.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Queue {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::string> values;
    bool closed = false;

    void Push(std::string value) {
        std::lock_guard<std::mutex> lock(mutex);
        values.push_back(std::move(value));
        changed.notify_all();
    }

    bool Wait(
        const std::function<bool(const nlohmann::json&)>& predicate,
        std::chrono::seconds timeout,
        nlohmann::json& result) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            for (auto iterator = values.begin(); iterator != values.end(); ++iterator) {
                try {
                    const auto value = nlohmann::json::parse(*iterator);
                    if (predicate(value)) {
                        result = value;
                        values.erase(iterator);
                        return true;
                    }
                } catch (...) {
                }
            }
            if (closed || changed.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
    }
};

std::wstring Quote(const std::wstring& value) {
    return L"\"" + value + L"\"";
}

std::string Narrow(const std::wstring& value) {
    const int length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    std::string result(length, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length,
        nullptr,
        nullptr);
    return result;
}

bool HasVisiblePixel(asterui::SharedFrame& frame) {
    auto* header = frame.Header();
    if (!header || header->publishedSlot < 0) {
        return false;
    }
    const uint32_t slot = static_cast<uint32_t>(header->publishedSlot);
    if (slot >= asterui::kFrameSlots) {
        return false;
    }
    const uint8_t* pixels = asterui::FrameSlot(header, slot);
    const uint64_t bytes = asterui::FrameBytes(header->width, header->height);
    for (uint64_t index = 3; index < bytes; index += 4) {
        if (pixels[index] != 0) {
            return true;
        }
    }
    return false;
}

bool HasVisiblePixelInRegion(
    asterui::SharedFrame& frame,
    double left,
    double top,
    double right,
    double bottom) {
    auto* header = frame.Header();
    if (!header || header->publishedSlot < 0) {
        return false;
    }
    const uint32_t slot = static_cast<uint32_t>(header->publishedSlot);
    if (slot >= asterui::kFrameSlots) {
        return false;
    }
    const uint8_t* pixels = asterui::FrameSlot(header, slot);
    const uint32_t x0 = static_cast<uint32_t>(header->width * left);
    const uint32_t x1 = static_cast<uint32_t>(header->width * right);
    const uint32_t y0 = static_cast<uint32_t>(header->height * top);
    const uint32_t y1 = static_cast<uint32_t>(header->height * bottom);
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            if (pixels[static_cast<size_t>(y) * header->stride + x * 4 + 3] != 0) {
                return true;
            }
        }
    }
    return false;
}

bool IsInputEvent(const nlohmann::json& value, const std::string& kind) {
    if (value.value("type", "") != "event" ||
        value.value("name", "") != "asterui:input") {
        return false;
    }
    const auto iterator = value.find("payload");
    if (iterator == value.end()) return false;
    nlohmann::json payload;
    try {
        payload = iterator->is_string()
            ? nlohmann::json::parse(iterator->get<std::string>())
            : *iterator;
    } catch (...) {
        return false;
    }
    return payload.value("kind", "") == kind;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        std::wcerr
            << L"usage: AsterUI.HostSmoke <host.exe> <ui-root> [page] [width] [height]"
            << std::endl;
        return 2;
    }
    const std::filesystem::path host(argv[1]);
    const std::filesystem::path uiRoot(argv[2]);
    const std::wstring page = argc >= 4 ? argv[3] : L"ui/index.html";
    const uint32_t width = argc >= 5 ? static_cast<uint32_t>(_wtoi(argv[4])) : 640;
    const uint32_t height = argc >= 6 ? static_cast<uint32_t>(_wtoi(argv[5])) : 360;
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        std::cerr << "invalid smoke test dimensions" << std::endl;
        return 2;
    }
    const std::string pageUtf8 = Narrow(page);
    const bool expectEvent = page == L"ui/index.html";
    const bool inputPage =
        page.find(L"input-smoke/index.html") != std::wstring::npos;
    const uint32_t pid = GetCurrentProcessId();
    const std::wstring suffix = std::to_wstring(pid) + L"." + std::to_wstring(GetTickCount64());
    const std::wstring commandName = L"\\\\.\\pipe\\AsterUISmoke.Cmd." + suffix;
    const std::wstring eventName = L"\\\\.\\pipe\\AsterUISmoke.Evt." + suffix;
    const std::wstring mappingName = L"Local\\AsterUISmoke." + suffix;
    const std::string token = "smoke-" + std::to_string(GetTickCount64());

    asterui::SharedFrame frame;
    if (!frame.Create(mappingName, 1, width, height)) {
        std::cerr << "cannot create frame" << std::endl;
        return 3;
    }
    asterui::PipeServer commandServer;
    asterui::PipeServer eventServer;
    if (!commandServer.Create(commandName) || !eventServer.Create(eventName)) {
        std::cerr << "cannot create pipes" << std::endl;
        return 4;
    }

    const std::wstring mappingWide = mappingName;
    const std::string mapping = Narrow(mappingWide);
    std::wstring command =
        Quote(host.wstring()) +
        L" --pipe-cmd " + Quote(commandName) +
        L" --pipe-evt " + Quote(eventName) +
        L" --token " + Quote(std::wstring(token.begin(), token.end())) +
        L" --ui-root " + Quote(uiRoot.wstring()) +
        L" --fps 30 --gpu 0 --disable-gpu --disable-gpu-compositing"
        L" --allow-file-access-from-files --disable-background-networking"
        L" --disable-component-update --disable-sync --disable-default-apps"
        L" --no-first-run --no-pings --metrics-recording-only"
        L" --disable-domain-reliability"
        L" --disable-features=OptimizationHints,MediaRouter,Translate";

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(
            host.c_str(),
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            host.parent_path().c_str(),
            &startup,
            &process)) {
        std::cerr << "cannot launch host: " << GetLastError() << std::endl;
        return 5;
    }
    CloseHandle(process.hThread);

    auto commandPipe = commandServer.Accept(15000);
    auto eventPipe = eventServer.Accept(15000);
    if (!commandPipe.IsOpen() || !eventPipe.IsOpen()) {
        std::cerr << "host did not connect" << std::endl;
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hProcess);
        return 6;
    }

    std::string hello;
    if (!eventPipe.ReadMessage(hello)) {
        std::cerr << "no hello" << std::endl;
        return 7;
    }
    const auto helloValue = nlohmann::json::parse(hello);
    if (helloValue.value("token", "") != token) {
        std::cerr << "token mismatch" << std::endl;
        return 8;
    }
    commandPipe.WriteMessage(nlohmann::json{
        {"type", "hello_ack"},
        {"protocol", asterui::kProtocolVersion},
        {"ok", true}}.dump());

    Queue queue;
    std::atomic<bool> reading = true;
    std::thread reader([&]() {
        while (reading) {
            std::string message;
            if (!eventPipe.ReadMessage(message)) {
                break;
            }
            queue.Push(std::move(message));
        }
        std::lock_guard<std::mutex> lock(queue.mutex);
        queue.closed = true;
        queue.changed.notify_all();
    });

    nlohmann::json ready;
    const bool readyOk = queue.Wait(
        [](const nlohmann::json& value) { return value.value("type", "") == "ready"; },
        std::chrono::seconds(20),
        ready);
    if (!readyOk) {
        std::cerr << "host is not ready" << std::endl;
    } else {
        commandPipe.WriteMessage(nlohmann::json{
            {"type", "create"},
            {"browser", 1},
            {"url", pageUtf8},
            {"width", width},
            {"height", height},
            {"transparent", true},
            {"visible", true},
            {"focus", false},
            {"layer", 10},
            {"opacity", 1.0},
            {"mapping", mapping}}.dump());
    }

    nlohmann::json loaded;
    const bool loadedOk = readyOk && queue.Wait(
        [](const nlohmann::json& value) {
            return value.value("type", "") == "loaded" &&
                value.value("browser", 0u) == 1u &&
                value.value("loaded", false);
        },
        std::chrono::seconds(10),
        loaded);
    const bool hudPage =
        page.find(L"arkhud/index.html") != std::wstring::npos;
    if (loadedOk && hudPage) {
        const nlohmann::json state = {
            {"health", 100},
            {"armor", 100},
            {"stamina", 100},
            {"hideHud", 0},
            {"ui", {
                {"menu", 0},
                {"settings", {
                    {"speedo", 0},
                    {"allui", 1},
                    {"wmsingle", 1}}}}},
            {"wm", {{"title", "ARK BODY CAM"}, {"brand", "HUNTER H-X"}}},
            {"ver", {{"pack", "smoke"}}}};
        commandPipe.WriteMessage(nlohmann::json{
            {"type", "event"},
            {"browser", 1},
            {"name", "hud:state"},
            {"payload", state.dump()}}.dump());
    }
    if (loadedOk && page.find(L"arkhud-menu") != std::wstring::npos) {
        const nlohmann::json state = {
            {"open", true},
            {"payload", {
                {"ui", {
                    {"settings", {
                        {"speedo", 1},
                        {"allui", 1},
                        {"units", 0},
                        {"staminaauto", 1},
                        {"watermarkbodycam", 1}}}}},
                {"ver", {{"pack", "smoke"}}}}}};
        commandPipe.WriteMessage(nlohmann::json{
            {"type", "event"},
            {"browser", 1},
            {"name", "hud:state"},
            {"payload", state.dump()}}.dump());
    }
    bool inputOk = !inputPage;
    if (loadedOk && inputPage) {
        commandPipe.WriteMessage(nlohmann::json{
            {"type", "state"},
            {"browser", 1},
            {"visible", true},
            {"focus", true}}.dump());
        Sleep(150);
        const auto sendInput = [&](nlohmann::json value) {
            value["type"] = "input";
            value["browser"] = 1;
            commandPipe.WriteMessage(value.dump());
        };
        nlohmann::json event;
        sendInput({{"kind", "mouseMove"}, {"x", 160}, {"y", 120}, {"mods", 0}});
        sendInput({{"kind", "mouseButton"}, {"x", 160}, {"y", 120},
            {"button", 0}, {"up", false}, {"clicks", 1}, {"mods", 0}});
        const bool downOk = queue.Wait(
            [](const nlohmann::json& value) { return IsInputEvent(value, "down"); },
            std::chrono::seconds(3), event);
        sendInput({{"kind", "mouseMove"}, {"x", 220}, {"y", 140}, {"mods", 0}});
        const bool dragOk = queue.Wait(
            [](const nlohmann::json& value) { return IsInputEvent(value, "drag"); },
            std::chrono::seconds(3), event);
        sendInput({{"kind", "mouseButton"}, {"x", 220}, {"y", 140},
            {"button", 0}, {"up", true}, {"clicks", 1}, {"mods", 0}});
        const bool upOk = queue.Wait(
            [](const nlohmann::json& value) { return IsInputEvent(value, "up"); },
            std::chrono::seconds(3), event);
        const bool clickOk = queue.Wait(
            [](const nlohmann::json& value) { return IsInputEvent(value, "click"); },
            std::chrono::seconds(3), event);
        sendInput({{"kind", "wheel"}, {"x", 160}, {"y", 120},
            {"dx", 0}, {"dy", 120}, {"mods", 0}});
        const bool wheelOk = queue.Wait(
            [](const nlohmann::json& value) {
                if (!IsInputEvent(value, "wheel")) return false;
                try {
                    const auto payload = nlohmann::json::parse(
                        value.value("payload", std::string("{}")));
                    return std::abs(payload.value("deltaY", 0.0)) > 0.0;
                } catch (...) {
                    return false;
                }
            },
            std::chrono::seconds(3), event);
        sendInput({{"kind", "wheel"}, {"x", 160}, {"y", 120},
            {"dx", 120}, {"dy", 0}, {"mods", 0}});
        const bool horizontalWheelOk = queue.Wait(
            [](const nlohmann::json& value) {
                if (!IsInputEvent(value, "wheel")) return false;
                try {
                    const auto payload = nlohmann::json::parse(
                        value.value("payload", std::string("{}")));
                    return std::abs(payload.value("deltaX", 0.0)) > 0.0;
                } catch (...) {
                    return false;
                }
            },
            std::chrono::seconds(3), event);
        sendInput({{"kind", "mouseMove"}, {"x", 300}, {"y", 180}, {"mods", 0}});
        sendInput({{"kind", "mouseButton"}, {"x", 300}, {"y", 180},
            {"button", 0}, {"up", false}, {"clicks", 1}, {"mods", 0}});
        sendInput({{"kind", "mouseButton"}, {"x", 300}, {"y", 180},
            {"button", 0}, {"up", true}, {"clicks", 1}, {"mods", 0}});
        const bool rapidClickOk = queue.Wait(
            [](const nlohmann::json& value) { return IsInputEvent(value, "click"); },
            std::chrono::seconds(3), event);
        inputOk = downOk && dragOk && upOk && clickOk &&
            wheelOk && horizontalWheelOk && rapidClickOk;
        if (!inputOk) {
            std::cerr << "input test failed: down=" << downOk
                      << " drag=" << dragOk
                      << " up=" << upOk
                      << " click=" << clickOk
                      << " wheel=" << wheelOk
                      << " hwheel=" << horizontalWheelOk
                      << " rapid_click=" << rapidClickOk << std::endl;
        }
    }
    nlohmann::json event;
    const bool eventOk = !expectEvent || (loadedOk && queue.Wait(
        [](const nlohmann::json& value) {
            return value.value("type", "") == "event" &&
                value.value("name", "") == "asterui:smoke";
        },
        std::chrono::seconds(5),
        event));

    bool pixel = false;
    bool watermarkPixel = !hudPage;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (HasVisiblePixel(frame)) {
            pixel = true;
        }
        if (hudPage && HasVisiblePixelInRegion(frame, 0.65, 0.0, 1.0, 0.25)) {
            watermarkPixel = true;
        }
        if (pixel && watermarkPixel) {
            break;
        }
        Sleep(50);
    }

    size_t sampledFrames = 0;
    size_t blankFrames = 0;
    if (hudPage && pixel && watermarkPixel) {
        LONG lastSequence = InterlockedCompareExchange(
            &frame.Header()->publishedSequence,
            0,
            0);
        const auto sampleDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < sampleDeadline) {
            const LONG sequence = InterlockedCompareExchange(
                &frame.Header()->publishedSequence,
                0,
                0);
            if (sequence != lastSequence) {
                lastSequence = sequence;
                ++sampledFrames;
                if (!HasVisiblePixelInRegion(frame, 0.65, 0.0, 1.0, 0.25)) {
                    ++blankFrames;
                }
            }
            Sleep(10);
        }
    }

    commandPipe.WriteMessage(nlohmann::json{{"type", "shutdown"}}.dump());
    reading = false;
    commandPipe.Close();
    eventPipe.Close();
    WaitForSingleObject(process.hProcess, 15000);
    if (reader.joinable()) {
        reader.join();
    }
    CloseHandle(process.hProcess);

    if (!readyOk || !loadedOk || !eventOk || !inputOk || !pixel || !watermarkPixel ||
        (hudPage && sampledFrames != 0 && blankFrames != 0)) {
        std::cerr << "smoke failed: ready=" << readyOk
                  << " loaded=" << loadedOk
                  << " event=" << eventOk
                  << " input=" << inputOk
                  << " pixel=" << pixel
                  << " watermark_pixel=" << watermarkPixel
                  << " sampled_frames=" << sampledFrames
                  << " blank_frames=" << blankFrames << std::endl;
        return 10;
    }
    std::cout << "AsterUI host smoke passed" << std::endl;
    return 0;
}
