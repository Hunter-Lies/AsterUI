#include "host/pipe_client.h"

#include "common/protocol.h"

#include <nlohmann/json.hpp>

#include <utility>

namespace asterui::host {

PipeClient::~PipeClient() {
    Stop();
}

bool PipeClient::Connect(
    const std::wstring& commandName,
    const std::wstring& eventName,
    DWORD timeoutMs) {
    command_ = PipeConnection::OpenClient(commandName, timeoutMs);
    event_ = PipeConnection::OpenClient(eventName, timeoutMs);
    return command_.IsOpen() && event_.IsOpen();
}

bool PipeClient::Handshake(const std::string& token) {
    if (!command_.IsOpen() || !event_.IsOpen()) {
        return false;
    }
    if (!event_.WriteMessage(nlohmann::json{
            {"type", "hello"},
            {"protocol", kProtocolVersion},
            {"token", token},
            {"pid", GetCurrentProcessId()}}.dump())) {
        return false;
    }
    std::string response;
    if (!command_.ReadMessage(response)) {
        return false;
    }
    try {
        const auto value = nlohmann::json::parse(response);
        connected_ = value.value("type", "") == "hello_ack" &&
            value.value("protocol", 0u) == kProtocolVersion &&
            value.value("ok", false);
        return connected_;
    } catch (...) {
        return false;
    }
}

void PipeClient::Start(
    std::function<void(const std::string&)> onMessage,
    std::function<void()> onClosed) {
    if (!connected_ || running_.exchange(true)) {
        return;
    }
    reader_ = std::thread([this, onMessage, onClosed]() {
        while (running_) {
            std::string message;
            if (!command_.ReadMessage(message)) {
                break;
            }
            if (onMessage) {
                onMessage(message);
            }
        }
        connected_ = false;
        running_ = false;
        if (onClosed) {
            onClosed();
        }
    });
}

void PipeClient::Stop() {
    running_ = false;
    connected_ = false;
    command_.Close();
    event_.Close();
    if (reader_.joinable()) {
        reader_.join();
    }
}

bool PipeClient::Send(const std::string& message) {
    if (!connected_) {
        return false;
    }
    return event_.WriteMessage(message);
}

}  // namespace asterui::host
