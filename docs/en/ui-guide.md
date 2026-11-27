# AsterUI UI Guide

[中文](../ui-guide.md) | [English](ui-guide.md)

This guide covers only how to build a UI page for AsterUI.

## 1. Mental model

AsterUI is a browser runtime embedded in GTA V. The page is normal HTML, CSS,
and JavaScript. AsterUI adds one global object:

```js
window.asterui
```

The page uses `emit` to send JSON events to the external consumer and `on` to
receive JSON state. The consumer is responsible for native actions and business
logic.

Think of it like RageUI's responsibilities: the framework provides the UI layer,
not the plugin's game logic.

## 2. Page structure

Recommended page layout:

```text
AsterUI/ui/my-ui/
  index.html
  style.css
  app.js
  images/
```

Use relative paths for assets:

```html
<link rel="stylesheet" href="style.css">
<img src="images/icon.png" alt="">
<script src="app.js"></script>
```

Do not use CDN or remote URLs. Production resource loading is local-only.

## 3. Lifecycle

The bridge may not exist when the HTML first starts executing. Always wait for
`asterui:ready`.

```js
window.addEventListener("asterui:ready", () => {
  console.log("AsterUI bridge ready");

  window.asterui.on("myui:state", (state) => {
    render(state);
  });

  window.asterui.emit("myui:ready", {
    page: "my-ui",
    version: 1
  });
});
```

Recommended lifecycle events:

- `myui:ready`: UI mounted and ready for initial state.
- `myui:request`: UI requests a refresh.
- `myui:action`: UI sends a user action.
- `myui:state`: consumer sends state back to the UI.

## 4. Event communication

Subscribe:

```js
const unsubscribe = window.asterui.on("myui:state", (payload) => {
  // payload is already parsed JSON
});

unsubscribe();
// or: window.asterui.off("myui:state", callback);
```

Send:

```js
window.asterui.emit("myui:action", {
  action: "set_value",
  value: 42
});
```

Rules:

- Event names match `^[A-Za-z0-9_.:\-]{1,64}$`.
- Payloads must be JSON serializable.
- Use a page-specific namespace such as `inventory:`, `fuel:`, or `myui:`.
- Keep payloads small; this is not a file-transfer channel.
- Do not assume native behavior from an event name. AsterUI only transports it.

Recommended request/response pattern:

```js
window.asterui.emit("inventory:request", {});
window.asterui.on("inventory:state", renderInventory);
window.asterui.emit("inventory:use", { id: 12 });
```

## 5. Rendering guidelines

- The page runs in a transparent full-window browser.
- Use transparent backgrounds when drawing an overlay.
- Keep controls inside the viewport at 1280×720, 1920×1080, and 2560×1440.
- Use `rem`, `clamp`, or layout-relative sizing for responsive panels.
- Do not scale font sizes directly from viewport width.
- Provide visible hover, focus, active, and disabled states.
- Support mouse and keyboard navigation where appropriate.
- Do not rely on a remote font or icon CDN.

## 6. Browser preview

Include the mock before your application script:

```html
<script src="../../sdk/js/mock.js"></script>
<script src="app.js"></script>
```

The mock provides the same `on`, `off`, and `emit` methods. Outgoing messages are
recorded:

```js
window.asterui.__mock.sent()
```

Use the mock to develop layout and interaction without starting GTA V.

## 7. Localhost hot reload

Use `examples/vite-starter` as a starting point:

```powershell
cd examples/vite-starter
npm install
npm run dev
```

Enable the exact loopback origin in the installed `AsterUI.json`:

```json
{
  "development": {
    "enabled": true,
    "allowedOrigins": ["http://127.0.0.1:5173"]
  }
}
```

The matching WebSocket origin is allowed for HMR. Keep development mode
disabled in released packages.

## 8. Debugging

- If `window.asterui` is missing, wait for `asterui:ready`.
- If preview works but the game page is blank, check asset paths and the host
  log for blocked resources.
- Use `console.log` and the `AsterUI.Host.log` console forwarding.
- Test with the same browser dimensions used by the external consumer.
- Do not add network dependencies; production mode blocks them.

## 9. Packaging a UI

Ship the page directory under:

```text
AsterUI/ui/<page-name>/
```

Include all CSS, JavaScript, images, and fonts as relative local assets. Avoid
absolute machine paths and development-only localhost URLs.

The external consumer chooses the page URL. AsterUI does not automatically
discover pages or define a menu entry.

## 10. Scope boundary

Outside AsterUI UI documentation:

- creating or registering ASI/RPH plugins
- native GTA V calls
- hotkey registration
- page launch policy
- consumer-side persistence and state storage

Inside AsterUI UI documentation:

- HTML/CSS/JavaScript authoring
- bridge lifecycle and communication
- browser preview and HMR
- asset layout and resource policy
- rendering, input, and debugging behavior
