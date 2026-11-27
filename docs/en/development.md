# UI Preview and Hot Reload

[中文](../development.md) | [English](development.md)

## Static preview

Open an example's `index.html` in Chrome. Include:

```html
<script src="../../sdk/js/mock.js"></script>
```

The mock provides the same bridge methods and records outgoing events:

```js
window.asterui.__mock.sent()
```

Use this mode for layout, animation, accessibility, and event-shape testing.

## Vite hot reload

From `examples/vite-starter`:

```powershell
npm install
npm run dev
```

The sample binds to `http://127.0.0.1:5173`.

Enable that exact origin in the installed `AsterUI.json`:

```json
{
  "development": {
    "enabled": true,
    "allowedOrigins": ["http://127.0.0.1:5173"]
  }
}
```

The matching WebSocket origin is allowed for HMR. No other localhost port or
LAN address is allowed unless explicitly listed.

Keep development mode disabled in distributed UI packages.

## Preview matrix

Check at minimum:

- 1280×720
- 1920×1080
- 2560×1440
- mouse hover and click
- keyboard focus and navigation
- long labels and missing optional fields
- no external network dependencies
