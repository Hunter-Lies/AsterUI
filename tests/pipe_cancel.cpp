#include "common/pipe.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int wmain() {
    const std::wstring name =
        L"\\\\.\\pipe\\AsterUI.CancelSmoke." +
        std::to_wstring(GetCurrentProcessId()) + L"." +
        std::to_wstring(GetTickCount64());
    asterui::PipeServer server;
    if (!server.Create(name)) {
        std::cerr << "cannot create pipe" << std::endl;
        return 1;
    }

    HANDLE cancel = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!cancel) {
        std::cerr << "cannot create cancel event" << std::endl;
        return 2;
    }

    std::atomic<bool> open = true;
    std::thread worker([&]() {
        auto connection = server.Accept(15000, cancel);
        open = connection.IsOpen();
    });
    Sleep(150);
    const auto start = std::chrono::steady_clock::now();
    SetEvent(cancel);
    worker.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    CloseHandle(cancel);

    if (open || elapsed.count() > 1500) {
        std::cerr
            << "pipe cancellation failed: open=" << open
            << " elapsed_ms=" << elapsed.count() << std::endl;
        return 3;
    }
    std::cout << "AsterUI pipe cancellation passed" << std::endl;
    return 0;
}
