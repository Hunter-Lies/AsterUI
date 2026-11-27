---
layout: home

hero:
  name: AsterUI
  text: Build game UI with HTML
  tagline: Local HTML/CSS/JavaScript UI runtime for Grand Theft Auto V Legacy Edition on Windows Story Mode. Pages own interaction; plugins own application logic.
  actions:
    - theme: brand
      text: Get started
      link: /en/getting-started
    - theme: alt
      text: Full UI guide
      link: /en/ui-guide
    - theme: alt
      text: GitHub
      link: https://github.com/Hunter-Lies/AsterUI
  image:
    src: /logo.svg
    alt: AsterUI
features:
  - title: UI focused
    details: Pages own layout, animation, and interaction. Native calls, hotkeys, and game features belong to the plugin.
  - title: JSON bridge
    details: Use on, off, and emit on window.asterui to exchange structured events.
  - title: Local first
    details: Production resources are local-only. Development mode permits exact loopback origins for HMR.
  - title: Previewable
    details: Develop in a normal browser with sdk/js/mock.js, then validate inside the game CEF runtime.
  - title: Fullscreen ready
    details: The demo supports exclusive fullscreen, borderless fullscreen, and windowed mode.
---

## Minimal bridge example

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    document.getElementById("value").textContent = state.value;
  });

  window.asterui.emit("myui:request", {});
});
```

## Development path

1. Copy `examples/hello-overlay` to `AsterUI/ui/my-ui/`.
2. Preview in Chrome with `sdk/js/mock.js`.
3. Validate interaction with Vite and local hot reload.
4. Ship the page with all local assets.

[Build your first page →](/en/getting-started)

## Contributors

<p align="center">
  <a href="https://github.com/Hunter-Lies/AsterUI/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=Hunter-Lies/AsterUI" alt="AsterUI contributors">
  </a>
</p>

Avatars are generated from merged commits. See [contributor policy](https://github.com/Hunter-Lies/AsterUI/blob/main/CONTRIBUTORS.en.md).
