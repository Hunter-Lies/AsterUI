#pragma once

#include "common/pipe.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace asterui::host {

class PipeClient {
public:
    ~PipeClient();

    bool Connect(const std::wstring& commandName, const std::wstring& eventName, DWORD timeoutMs);
    bool Handshake(const std::string& token);
    void Start(
        std::function<void(const std::string&)> onMessage,
        std::function<void()> onClosed);
    void Stop();
    bool Send(const std::string& message);
    bool IsConnected() const { return connected_; }

private:
    PipeConnection command_;
    PipeConnection event_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread reader_;
};

}  // namespace asterui::host
