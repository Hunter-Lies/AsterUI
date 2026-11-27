#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASTERUI_API_VERSION 1u
#define ASTERUI_API_BYTES 120u
#define ASTERUI_BROWSER_DESC_BYTES 120u
#define ASTERUI_PROTOCOL_VERSION 1u

#if defined(_WIN32)
#define ASTERUI_CALL __cdecl
#else
#define ASTERUI_CALL
#endif

typedef struct AsterBrowserDesc {
    uint32_t structSize;
    uint32_t reserved0;
    const char* url;
    int32_t width;
    int32_t height;
    int32_t transparent;
    int32_t maxWidth;
    int32_t maxHeight;
    uint8_t reserved[84];
} AsterBrowserDesc;

typedef void (*AsterEventCallback)(
    uint32_t browser,
    const char* name,
    const char* payload,
    void* user);

typedef uint32_t (*AsterCreateBrowserFn)(const AsterBrowserDesc* description);
typedef int32_t (*AsterDestroyBrowserFn)(uint32_t browser);
typedef int32_t (*AsterSetVisibleFn)(uint32_t browser, int32_t visible);
typedef int32_t (*AsterSetFocusFn)(uint32_t browser, int32_t focused);
typedef int32_t (*AsterSetLayerFn)(uint32_t browser, int32_t layer);
typedef int32_t (*AsterSetOpacityFn)(uint32_t browser, float opacity);
typedef int32_t (*AsterResizeFn)(uint32_t browser, int32_t width, int32_t height);
typedef int32_t (*AsterNavigateFn)(uint32_t browser, const char* url);
typedef int32_t (*AsterSendEventFn)(
    uint32_t browser,
    const char* name,
    const char* payload);
typedef uint64_t (*AsterAddListenerFn)(
    uint32_t browser,
    const char* name,
    AsterEventCallback callback,
    void* user);
typedef int32_t (*AsterRemoveListenerFn)(uint64_t listener);
typedef int32_t (*AsterIsLoadedFn)(uint32_t browser);
typedef int32_t (*AsterIsHostConnectedFn)(void);
typedef void (*AsterLogFn)(int32_t level, const char* message);

typedef struct AsterApiV1 {
    int32_t size;
    uint32_t reserved;
    void* functions[14];
} AsterApiV1;

typedef const AsterApiV1* (ASTERUI_CALL* AsterGetApiFn)(uint32_t version);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
static_assert(
    sizeof(AsterBrowserDesc) == ASTERUI_BROWSER_DESC_BYTES,
    "AsterBrowserDesc ABI changed");
static_assert(sizeof(AsterApiV1) == ASTERUI_API_BYTES, "AsterApiV1 ABI changed");
#endif
