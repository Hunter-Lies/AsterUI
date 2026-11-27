#pragma once

#include "asterui.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace asterui {

class Api {
public:
    Api() = default;

    explicit Api(const AsterApiV1* table) {
        Reset(table);
    }

    bool IsValid() const {
        return table_ != nullptr;
    }

    explicit operator bool() const {
        return IsValid();
    }

    void Reset(const AsterApiV1* table) {
        table_ = nullptr;
        if (!table || table->size != static_cast<int32_t>(ASTERUI_API_BYTES)) {
            return;
        }
        table_ = table;
    }

    uint32_t Create(const AsterBrowserDesc& description) const {
        Require();
        return reinterpret_cast<AsterCreateBrowserFn>(table_->functions[0])(
            &description);
    }

    bool Destroy(uint32_t browser) const {
        Require();
        return reinterpret_cast<AsterDestroyBrowserFn>(table_->functions[1])(
                   browser) != 0;
    }

    bool SetVisible(uint32_t browser, bool visible) const {
        Require();
        return reinterpret_cast<AsterSetVisibleFn>(table_->functions[2])(
                   browser,
                   visible ? 1 : 0) != 0;
    }

    bool SetFocus(uint32_t browser, bool focused) const {
        Require();
        return reinterpret_cast<AsterSetFocusFn>(table_->functions[3])(
                   browser,
                   focused ? 1 : 0) != 0;
    }

    bool SetLayer(uint32_t browser, int32_t layer) const {
        Require();
        return reinterpret_cast<AsterSetLayerFn>(table_->functions[4])(
                   browser,
                   layer) != 0;
    }

    bool SetOpacity(uint32_t browser, float opacity) const {
        Require();
        return reinterpret_cast<AsterSetOpacityFn>(table_->functions[5])(
                   browser,
                   opacity) != 0;
    }

    bool Resize(uint32_t browser, int32_t width, int32_t height) const {
        Require();
        return reinterpret_cast<AsterResizeFn>(table_->functions[6])(
                   browser,
                   width,
                   height) != 0;
    }

    bool Navigate(uint32_t browser, const char* url) const {
        Require();
        return reinterpret_cast<AsterNavigateFn>(table_->functions[7])(
                   browser,
                   url) != 0;
    }

    bool SendEvent(
        uint32_t browser,
        const char* name,
        const char* jsonPayload) const {
        Require();
        return reinterpret_cast<AsterSendEventFn>(table_->functions[8])(
                   browser,
                   name,
                   jsonPayload) != 0;
    }

    uint64_t AddListener(
        uint32_t browser,
        const char* name,
        AsterEventCallback callback,
        void* user = nullptr) const {
        Require();
        return reinterpret_cast<AsterAddListenerFn>(table_->functions[9])(
            browser,
            name,
            callback,
            user);
    }

    bool RemoveListener(uint64_t listener) const {
        Require();
        return reinterpret_cast<AsterRemoveListenerFn>(table_->functions[10])(
                   listener) != 0;
    }

    bool IsLoaded(uint32_t browser) const {
        Require();
        return reinterpret_cast<AsterIsLoadedFn>(table_->functions[11])(
                   browser) != 0;
    }

    bool IsHostConnected() const {
        Require();
        return reinterpret_cast<AsterIsHostConnectedFn>(table_->functions[12])() != 0;
    }

    void Log(int32_t level, const std::string& message) const {
        Require();
        reinterpret_cast<AsterLogFn>(table_->functions[13])(
            level,
            message.c_str());
    }

private:
    void Require() const {
        if (!table_) {
            throw std::runtime_error("AsterUI API is not initialized");
        }
    }

    const AsterApiV1* table_ = nullptr;
};

}  // namespace asterui
