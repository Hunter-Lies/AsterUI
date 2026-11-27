#pragma once

#include <asterui/asterui.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <windows.h>

namespace asterui {

inline constexpr uint32_t kApiVersion = ASTERUI_API_VERSION;
inline constexpr uint32_t kApiBytes = ASTERUI_API_BYTES;
inline constexpr uint32_t kBrowserDescBytes = ASTERUI_BROWSER_DESC_BYTES;
inline constexpr uint32_t kProtocolVersion = ASTERUI_PROTOCOL_VERSION;
inline constexpr uint32_t kMaxBrowsers = 8;
inline constexpr uint32_t kMaxMessageBytes = 1024 * 1024;
inline constexpr uint32_t kFrameMagic = 0x4D544848;  // HHTM
inline constexpr uint32_t kFrameVersion = 1;
inline constexpr uint32_t kFrameSlots = 2;
inline constexpr uint32_t kFrameHeaderBytes = 160;

using BrowserDesc = ::AsterBrowserDesc;

static_assert(sizeof(BrowserDesc) == kBrowserDescBytes, "BrowserDesc ABI changed");
static_assert(offsetof(BrowserDesc, url) == 8, "BrowserDesc.url ABI changed");
static_assert(offsetof(BrowserDesc, width) == 16, "BrowserDesc.width ABI changed");
static_assert(offsetof(BrowserDesc, height) == 20, "BrowserDesc.height ABI changed");
static_assert(offsetof(BrowserDesc, transparent) == 24, "BrowserDesc.transparent ABI changed");

struct FrameHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t headerBytes;
    uint32_t pixelFormat;
    uint32_t browserId;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t slotCount;
    uint32_t reserved0;
    volatile LONG publishedSlot;
    volatile LONG publishedSequence;
    volatile LONG slotSequence[kFrameSlots];
    volatile LONG visible;
    volatile LONG focused;
    volatile LONG layer;
    volatile LONG opacityBits;
    uint8_t reserved[88];
};

static_assert(sizeof(FrameHeader) == kFrameHeaderBytes, "FrameHeader size changed");

inline uint64_t FrameBytes(uint32_t width, uint32_t height) {
    return static_cast<uint64_t>(width) * height * 4u;
}

inline uint64_t MappingBytes(uint32_t width, uint32_t height) {
    return kFrameHeaderBytes +
        FrameBytes(width, height) * static_cast<uint64_t>(kFrameSlots);
}

inline uint8_t* FrameSlot(FrameHeader* header, uint32_t slot) {
    return reinterpret_cast<uint8_t*>(header) + kFrameHeaderBytes +
        FrameBytes(header->width, header->height) * slot;
}

inline const uint8_t* FrameSlot(const FrameHeader* header, uint32_t slot) {
    return reinterpret_cast<const uint8_t*>(header) + kFrameHeaderBytes +
        FrameBytes(header->width, header->height) * slot;
}

inline LONG FloatBits(float value) {
    LONG result = 0;
    static_assert(sizeof(result) == sizeof(value), "float must be 32-bit");
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

inline float BitsFloat(LONG bits) {
    float result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

}  // namespace asterui
