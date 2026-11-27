#include "host/app.h"

#include "common/protocol.h"

#include "include/cef_process_message.h"
#include "include/cef_v8.h"

#include <algorithm>
#include <cctype>

namespace asterui::host {

namespace {

constexpr char kBridgeScript[] = R"JS(
(function () {
  'use strict';
  if (window.asterui && window.asterui.__dispatch) return;
  var emitNative = window.__asteruiEmit;
  try { delete window.__asteruiEmit; } catch (e) {}
  var listeners = new Map();
  function valid(name) {
    return typeof name === 'string' && /^[A-Za-z0-9_.:\-]{1,64}$/.test(name);
  }
  var api = {
    on: function (name, callback) {
      if (!valid(name)) throw new TypeError('asterui.on: invalid event name');
      if (typeof callback !== 'function') throw new TypeError('asterui.on: callback must be a function');
      var set = listeners.get(name);
      if (!set) { set = new Set(); listeners.set(name, set); }
      set.add(callback);
      return function () { api.off(name, callback); };
    },
    off: function (name, callback) {
      var set = listeners.get(name);
      if (!set) return false;
      if (callback === undefined) { listeners.delete(name); return true; }
      var removed = set.delete(callback);
      if (set.size === 0) listeners.delete(name);
      return removed;
    },
    emit: function (name, payload) {
      if (!valid(name)) throw new TypeError('asterui.emit: invalid event name');
      var json;
      try { json = JSON.stringify(payload === undefined ? {} : payload); }
      catch (e) { throw new TypeError('asterui.emit: payload is not JSON serializable'); }
      if (json === undefined) json = '{}';
      emitNative(name, json);
    }
  };
  Object.defineProperty(api, '__dispatch', {
    value: function (name, payload) {
      var set = listeners.get(name);
      if (!set) return;
      Array.from(set).forEach(function (callback) {
        try { callback(payload); }
        catch (e) { console.error('[asterui] handler for "' + name + '" threw:', e); }
      });
    }
  });
  Object.defineProperty(window, 'asterui', { value: Object.freeze(api), enumerable: true });
  Object.defineProperty(window, 'gtanui', { value: api, enumerable: true });

  // Windowless CEF does not paint the OS cursor into the shared frame. Draw a
  // DOM cursor so menu pages still have a visible pointer in exclusive fullscreen.
  (function installCursor() {
    function start() {
      var root = document.documentElement;
      if (!root || !document.body) return false;
      var style = document.createElement('style');
      style.textContent = [
        '* { cursor: none !important; }',
        '#asterui-cursor {',
        '  position: fixed; left: 0; top: 0; width: 18px; height: 25px;',
        '  z-index: 2147483647; display: none; pointer-events: none;',
        '  background: #fff;',
        '  clip-path: polygon(0 0, 0 22px, 6px 16px, 10px 25px, 14px 23px, 10px 14px, 18px 14px);',
        '  filter: drop-shadow(0 1px 2px rgba(0,0,0,.9));',
        '  transform: translate(-2px, -2px);',
        '}'
      ].join(' ');
      root.appendChild(style);
      var cursor = document.createElement('div');
      cursor.id = 'asterui-cursor';
      root.appendChild(cursor);
      var moved = false;
      function place(x, y) {
        cursor.style.transform = 'translate(' + (x - 2) + 'px,' + (y - 2) + 'px)';
        cursor.style.display = 'block';
      }
      function onMove(event) {
        moved = true;
        place(event.clientX, event.clientY);
      }
      function onOpen() {
        if (!document.body.classList.contains('open')) {
          cursor.style.display = 'none';
          return;
        }
        if (!moved) place(window.innerWidth / 2, window.innerHeight / 2);
      }
      document.addEventListener('mousemove', onMove, true);
      document.addEventListener('mousedown', onMove, true);
      document.addEventListener('mouseleave', function () { cursor.style.display = 'none'; }, true);
      window.addEventListener('blur', function () { cursor.style.display = 'none'; });
      new MutationObserver(onOpen).observe(document.body, {
        attributes: true,
        attributeFilter: ['class']
      });
      onOpen();
      return true;
    }
    if (!start()) {
      document.addEventListener('DOMContentLoaded', start, { once: true });
    }
  })();

  function pageBeat() {
    try {
      api.emit('__asterui.page', { at: Date.now() });
    } catch (e) {}
  }
  setInterval(pageBeat, 1000);
  setTimeout(pageBeat, 0);

  function ready() {
    window.dispatchEvent(new Event('asterui:ready'));
    window.dispatchEvent(new Event('gtanui:ready'));
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', ready, { once: true });
  } else {
    ready();
  }
})();
)JS";

class EmitHandler final : public CefV8Handler {
public:
    bool Execute(
        const CefString& name,
        CefRefPtr<CefV8Value>,
        const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval,
        CefString& exception) override {
        if (name != "__asteruiEmit" || arguments.size() < 2 ||
            !arguments[0]->IsString() || !arguments[1]->IsString()) {
            return false;
        }
        const std::string eventName = arguments[0]->GetStringValue().ToString();
        if (eventName.empty() || eventName.size() > 64 ||
            !std::all_of(eventName.begin(), eventName.end(), [](unsigned char c) {
                return std::isalnum(c) || c == '_' || c == '.' || c == ':' || c == '-';
            })) {
            exception = "invalid event name";
            return true;
        }
        const std::string payload = arguments[1]->GetStringValue().ToString();
        if (payload.size() > kMaxMessageBytes / 2) {
            exception = "payload too large";
            return true;
        }
        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        CefRefPtr<CefFrame> frame = context ? context->GetFrame() : nullptr;
        if (!frame) {
            return true;
        }
        auto message = CefProcessMessage::Create("asterui_emit");
        auto values = message->GetArgumentList();
        values->SetString(0, eventName);
        values->SetString(1, payload);
        frame->SendProcessMessage(PID_BROWSER, message);
        retval = CefV8Value::CreateBool(true);
        return true;
    }

private:
    IMPLEMENT_REFCOUNTING(EmitHandler);
};

}  // namespace

void AsterUiApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
    if (registrar) {
        registrar->AddCustomScheme(
            "asterui",
            CEF_SCHEME_OPTION_STANDARD |
                CEF_SCHEME_OPTION_SECURE |
                CEF_SCHEME_OPTION_CORS_ENABLED |
                CEF_SCHEME_OPTION_FETCH_ENABLED);
    }
}

void AsterUiApp::OnContextCreated(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
    if (!frame || !context) {
        return;
    }
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<EmitHandler> handler = new EmitHandler();
    CefRefPtr<CefV8Value> emit = CefV8Value::CreateFunction("__asteruiEmit", handler);
    if (emit) {
        global->SetValue(
            "__asteruiEmit",
            emit,
            static_cast<CefV8Value::PropertyAttribute>(
                V8_PROPERTY_ATTRIBUTE_DONTENUM | V8_PROPERTY_ATTRIBUTE_READONLY));
    }
    frame->ExecuteJavaScript(kBridgeScript, frame->GetURL(), 0);
}

}  // namespace asterui::host
