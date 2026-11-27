# UI Troubleshooting

[中文](../troubleshooting.md) | [English](troubleshooting.md)

## `window.asterui` is missing

Wait for the bridge:

```js
window.addEventListener("asterui:ready", () => {
  console.log(window.asterui);
});
```

The bridge is not available for direct calls during initial HTML parsing.

## Page works in Chrome but not in AsterUI

- Check all asset paths are relative and local.
- Confirm the entry file is `index.html`.
- Confirm the page directory is under `AsterUI/ui`.
- Remove CDN, remote font, remote image, and external `fetch` dependencies.
- Inspect `AsterUI.Host.log` for blocked resource messages.

## Localhost page does not load

- Development mode must be enabled in the installed `AsterUI.json`.
- Add the exact origin, including the port.
- Only loopback hosts are accepted.
- The page URL must use `http://` or `https://`, not a local file path.

## UI receives no state

- Emit a request event after `asterui:ready`.
- Confirm both sides use exactly the same event name.
- Check the payload is valid JSON.
- The plugin, not AsterUI, defines what the event means and whether
  it sends a response.

## Layout or input problems

- Test at the browser dimensions supplied by the plugin.
- Keep interactive controls inside the viewport.
- Provide hover, focus, active, and disabled states.
- Use pointer and keyboard events rather than desktop-only assumptions.
