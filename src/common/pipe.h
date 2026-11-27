#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <windows.h>

namespace asterui {

bool ReadExact(HANDLE handle, void* buffer, DWORD bytes);
bool WriteExact(HANDLE handle, const void* buffer, DWORD bytes);

class PipeConnection {
public:
    PipeConnection() = default;
    explicit PipeConnection(HANDLE handle) : pipe_(handle) {}
    ~PipeConnection();

    PipeConnection(const PipeConnection&) = delete;
    PipeConnection& operator=(const PipeConnection&) = delete;
    PipeConnection(PipeConnection&& other) noexcept;
    PipeConnection& operator=(PipeConnection&& other) noexcept;

    bool IsOpen() const { return pipe_ != INVALID_HANDLE_VALUE && pipe_ != nullptr; }
    void Close();
    bool ReadMessage(std::string& message);
    bool ReadMessageTimeout(std::string& message, DWORD timeoutMs, bool& timedOut);
    bool WriteMessage(const std::string& message);

    static PipeConnection OpenClient(const std::wstring& name, DWORD timeoutMs);

private:
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::mutex writeMutex_;
};

class PipeServer {
public:
    PipeServer() = default;
    ~PipeServer();

    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    bool Create(const std::wstring& name);
    PipeConnection Accept(DWORD timeoutMs, HANDLE cancelEvent = nullptr);
    void Close();

private:
    std::atomic<HANDLE> pipe_ = INVALID_HANDLE_VALUE;
};

}  // namespace asterui
