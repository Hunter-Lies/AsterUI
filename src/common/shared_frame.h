#pragma once

#include "common/protocol.h"

#include <cstdint>
#include <string>

namespace asterui {

class SharedFrame {
public:
    SharedFrame() = default;
    ~SharedFrame();

    SharedFrame(const SharedFrame&) = delete;
    SharedFrame& operator=(const SharedFrame&) = delete;

    bool Create(const std::wstring& name, uint32_t browserId, uint32_t width, uint32_t height);
    bool Open(const std::wstring& name);
    void Close();

    bool IsOpen() const { return view_ != nullptr; }
    FrameHeader* Header() const { return header_; }
    const std::wstring& Name() const { return name_; }

    bool Publish(uint32_t width, uint32_t height, const uint8_t* pixels, int stride);
    bool Snapshot(std::string& pixels, uint32_t& width, uint32_t& height, int& stride);

private:
    HANDLE mapping_ = nullptr;
    void* view_ = nullptr;
    FrameHeader* header_ = nullptr;
    std::wstring name_;
    uint32_t mappedWidth_ = 0;
    uint32_t mappedHeight_ = 0;
};

}  // namespace asterui
