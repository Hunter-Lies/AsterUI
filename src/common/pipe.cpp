#include "common/pipe.h"

#include "common/protocol.h"

#include <chrono>
#include <thread>
#include <utility>

namespace asterui {

bool ReadExact(HANDLE handle, void* buffer, DWORD bytes) {
    auto* cursor = static_cast<uint8_t*>(buffer);
    DWORD remaining = bytes;
    while (remaining != 0) {
        DWORD received = 0;
        if (!ReadFile(handle, cursor, remaining, &received, nullptr) || received == 0) {
            return false;
        }
        cursor += received;
        remaining -= received;
    }
    return true;
}

bool WriteExact(HANDLE handle, const void* buffer, DWORD bytes) {
    auto* cursor = static_cast<const uint8_t*>(buffer);
    DWORD remaining = bytes;
    while (remaining != 0) {
        DWORD sent = 0;
        if (!WriteFile(handle, cursor, remaining, &sent, nullptr) || sent == 0) {
            return false;
        }
        cursor += sent;
        remaining -= sent;
    }
    return true;
}

PipeConnection::~PipeConnection() {
    Close();
}

PipeConnection::PipeConnection(PipeConnection&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.writeMutex_);
    pipe_ = other.pipe_;
    other.pipe_ = INVALID_HANDLE_VALUE;
}

PipeConnection& PipeConnection::operator=(PipeConnection&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    Close();
    std::lock_guard<std::mutex> lock(other.writeMutex_);
    pipe_ = other.pipe_;
    other.pipe_ = INVALID_HANDLE_VALUE;
    return *this;
}

void PipeConnection::Close() {
    if (IsOpen()) {
        FlushFileBuffers(pipe_);
        DisconnectNamedPipe(pipe_);
        CloseHandle(pipe_);
    }
    pipe_ = INVALID_HANDLE_VALUE;
}

bool PipeConnection::ReadMessage(std::string& message) {
    if (!IsOpen()) {
        return false;
    }
    uint32_t length = 0;
    if (!ReadExact(pipe_, &length, sizeof(length)) ||
        length == 0 || length > kMaxMessageBytes) {
        return false;
    }
    message.resize(length);
    return ReadExact(pipe_, message.data(), length);
}

bool PipeConnection::ReadMessageTimeout(
    std::string& message,
    DWORD timeoutMs,
    bool& timedOut) {
    timedOut = false;
    if (!IsOpen()) {
        return false;
    }

    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }

        if (available >= sizeof(uint32_t)) {
            uint32_t length = 0;
            DWORD copied = 0;
            if (!PeekNamedPipe(
                    pipe_,
                    &length,
                    sizeof(length),
                    &copied,
                    &available,
                    nullptr) ||
                copied != sizeof(length)) {
                return false;
            }
            if (length == 0 || length > kMaxMessageBytes) {
                return false;
            }
            if (available >= sizeof(uint32_t) + length) {
                return ReadMessage(message);
            }
        }

        if (GetTickCount64() >= deadline) {
            timedOut = true;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

bool PipeConnection::WriteMessage(const std::string& message) {
    if (!IsOpen() || message.empty() || message.size() > kMaxMessageBytes) {
        return false;
    }
    std::lock_guard<std::mutex> lock(writeMutex_);
    const uint32_t length = static_cast<uint32_t>(message.size());
    return WriteExact(pipe_, &length, sizeof(length)) &&
        WriteExact(pipe_, message.data(), length);
}

PipeConnection PipeConnection::OpenClient(
    const std::wstring& name,
    DWORD timeoutMs) {
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        HANDLE pipe = CreateFileW(
            name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            return PipeConnection(pipe);
        }
        if (GetTickCount64() >= deadline) {
            return PipeConnection();
        }
        Sleep(50);
    }
}

PipeServer::~PipeServer() {
    Close();
}

bool PipeServer::Create(const std::wstring& name) {
    Close();
    HANDLE pipe = CreateNamedPipeW(
        name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        kMaxMessageBytes,
        kMaxMessageBytes,
        0,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    pipe_.store(pipe);
    return true;
}

PipeConnection PipeServer::Accept(DWORD timeoutMs, HANDLE cancelEvent) {
    const HANDLE pipe = pipe_.load();
    if (pipe == INVALID_HANDLE_VALUE) {
        return PipeConnection();
    }
    if (cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0) {
        Close();
        return PipeConnection();
    }

    HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!done) {
        return PipeConnection();
    }
    HANDLE acceptThread = nullptr;
    if (!DuplicateHandle(
            GetCurrentProcess(),
            GetCurrentThread(),
            GetCurrentProcess(),
            &acceptThread,
            THREAD_TERMINATE,
            FALSE,
            0)) {
        CloseHandle(done);
        return PipeConnection();
    }

    std::thread cancelWatch([done, cancelEvent, timeoutMs, acceptThread]() {
        HANDLE waits[2] = {done, cancelEvent};
        const DWORD count = cancelEvent ? 2 : 1;
        const DWORD wait = WaitForMultipleObjects(count, waits, FALSE, timeoutMs);
        if (wait == WAIT_OBJECT_0) {
            return;
        }
        CancelSynchronousIo(acceptThread);
    });

    BOOL connected = ConnectNamedPipe(pipe, nullptr);
    DWORD error = connected ? ERROR_SUCCESS : GetLastError();
    if (!connected && error == ERROR_PIPE_CONNECTED) {
        connected = true;
    }
    SetEvent(done);
    cancelWatch.join();
    CloseHandle(acceptThread);
    CloseHandle(done);

    if (!connected) {
        HANDLE active = pipe_.exchange(INVALID_HANDLE_VALUE);
        if (active == pipe) {
            CloseHandle(pipe);
        }
        return PipeConnection();
    }

    HANDLE accepted = pipe_.exchange(INVALID_HANDLE_VALUE);
    if (accepted != pipe) {
        return PipeConnection();
    }
    return PipeConnection(accepted);
}

void PipeServer::Close() {
    HANDLE pipe = pipe_.exchange(INVALID_HANDLE_VALUE);
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
    }
}

}  // namespace asterui
