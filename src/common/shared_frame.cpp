#include "common/shared_frame.h"

#include <cstring>
#include <limits>

namespace asterui {

SharedFrame::~SharedFrame() {
    Close();
}

bool SharedFrame::Create(
    const std::wstring& name,
    uint32_t browserId,
    uint32_t width,
    uint32_t height) {
    Close();
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        return false;
    }
    const uint64_t bytes = MappingBytes(width, height);
    if (bytes > static_cast<uint64_t>(std::numeric_limits<SIZE_T>::max())) {
        return false;
    }

    mapping_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        static_cast<DWORD>(bytes >> 32),
        static_cast<DWORD>(bytes),
        name.c_str());
    if (!mapping_) {
        return false;
    }
    view_ = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(bytes));
    if (!view_) {
        Close();
        return false;
    }

    header_ = static_cast<FrameHeader*>(view_);
    std::memset(header_, 0, static_cast<size_t>(bytes));
    header_->magic = kFrameMagic;
    header_->version = kFrameVersion;
    header_->headerBytes = kFrameHeaderBytes;
    header_->pixelFormat = 1;
    header_->browserId = browserId;
    header_->width = width;
    header_->height = height;
    header_->stride = width * 4;
    header_->slotCount = kFrameSlots;
    header_->publishedSlot = -1;
    mappedWidth_ = width;
    mappedHeight_ = height;
    name_ = name;
    return true;
}

bool SharedFrame::Open(const std::wstring& name) {
    Close();
    mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (!mapping_) {
        return false;
    }
    view_ = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view_) {
        Close();
        return false;
    }
    header_ = static_cast<FrameHeader*>(view_);
    if (header_->magic != kFrameMagic ||
        header_->version != kFrameVersion ||
        header_->headerBytes != kFrameHeaderBytes ||
        header_->slotCount != kFrameSlots ||
        header_->width == 0 || header_->height == 0 ||
        header_->width > 16384 || header_->height > 16384) {
        Close();
        return false;
    }
    mappedWidth_ = header_->width;
    mappedHeight_ = header_->height;
    name_ = name;
    return true;
}

void SharedFrame::Close() {
    if (view_) {
        UnmapViewOfFile(view_);
        view_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    header_ = nullptr;
    mappedWidth_ = 0;
    mappedHeight_ = 0;
    name_.clear();
}

bool SharedFrame::Publish(
    uint32_t width,
    uint32_t height,
    const uint8_t* pixels,
    int stride) {
    if (!header_ || !pixels || width != mappedWidth_ || height != mappedHeight_ ||
        stride < static_cast<int>(width * 4)) {
        return false;
    }

    const LONG previous = InterlockedCompareExchange(&header_->publishedSlot, 0, 0);
    const uint32_t slot = previous < 0 ? 0 : static_cast<uint32_t>(previous + 1) % kFrameSlots;
    InterlockedIncrement(&header_->slotSequence[slot]);
    uint8_t* destination = FrameSlot(header_, slot);
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    for (uint32_t row = 0; row < height; ++row) {
        std::memcpy(
            destination + row * rowBytes,
            pixels + static_cast<size_t>(row) * stride,
            rowBytes);
    }
    InterlockedIncrement(&header_->slotSequence[slot]);
    InterlockedExchange(&header_->publishedSlot, static_cast<LONG>(slot));
    InterlockedIncrement(&header_->publishedSequence);
    return true;
}

bool SharedFrame::Snapshot(std::string& pixels, uint32_t& width, uint32_t& height, int& stride) {
    if (!header_ || header_->publishedSlot < 0) {
        return false;
    }
    const uint32_t slot = static_cast<uint32_t>(header_->publishedSlot);
    if (slot >= kFrameSlots) {
        return false;
    }
    const LONG before = header_->slotSequence[slot];
    if ((before & 1) != 0) {
        return false;
    }
    width = header_->width;
    height = header_->height;
    stride = static_cast<int>(header_->stride);
    pixels.resize(static_cast<size_t>(FrameBytes(width, height)));
    std::memcpy(pixels.data(), FrameSlot(header_, slot), pixels.size());
    return before == header_->slotSequence[slot];
}

}  // namespace asterui
