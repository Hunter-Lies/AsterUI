# AsterUI

[Chinese](README.md) | [English](README.en.md) | [Documentation](https://hunter-lies.github.io/AsterUI/en/)

[![License: MIT](https://img.shields.io/badge/license-MIT-6fae94)](LICENSE)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-6fae94)](https://hunter-lies.github.io/AsterUI/en/)

A local HTML/CSS/JavaScript game UI runtime with responsibilities similar to RageUI: AsterUI owns page lifecycle, CEF rendering, input, and JSON messaging; an external consumer owns page launch policy and game actions.

> This documentation covers UI authoring only. Plugin, native, and hook development are outside its scope.

## Quick start

1. Copy `examples/hello-overlay` to `AsterUI/ui/my-ui/`.
2. Use `index.html` as the entry point.
3. Wait for the bridge:

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    console.log(state);
  });

  window.asterui.emit("myui:request", {});
});
```

4. Include `sdk/js/mock.js` for browser preview.

## UI model

```text
HTML interaction
  -> window.asterui.emit("myui:action", payload)
  -> external UI consumer
  -> consumer sends myui:state
  -> window.asterui.on receives JSON
  -> HTML update
```

AsterUI owns page lifecycle, rendering, mouse and keyboard input, resource allowlists, and JSON event transport. The external consumer owns event semantics, launch policy, hotkeys, and game actions.

## Documentation

- [Getting started](https://hunter-lies.github.io/AsterUI/en/getting-started)
- [Full UI guide](https://hunter-lies.github.io/AsterUI/en/ui-guide)
- [JavaScript bridge](https://hunter-lies.github.io/AsterUI/en/js-api)
- [Preview and HMR](https://hunter-lies.github.io/AsterUI/en/development)
- [Architecture and scope](https://hunter-lies.github.io/AsterUI/en/architecture)
- [Troubleshooting](https://hunter-lies.github.io/AsterUI/en/troubleshooting)

## Examples

- `demo-ui`: player state, player actions, GTA waypoint detection, and waypoint teleport.
- `hello-overlay`: minimal bridge handshake.
- `interactive-menu`: tabs, switches, pointer input.
- `notification`: event-driven notification.
- `vite-starter`: localhost HMR.

`AsterUI.Demo.asi` is only a demo consumer and is not part of the UI authoring tutorial.

## Contributors

<p align="center">
  <a href="https://github.com/Hunter-Lies/AsterUI/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=Hunter-Lies/AsterUI" alt="AsterUI contributors">
  </a>
</p>

Contributor avatars are generated from merged commit history. See [CONTRIBUTORS.en.md](CONTRIBUTORS.en.md).

## Engine maintainers

```powershell
.\scripts\bootstrap.ps1 -ScriptHookVSdk C:\SDK\ScriptHookV
.\scripts\build.ps1
.\scripts\package.ps1
.\scripts\test.ps1
```

Documentation development:

```powershell
npm install
npm run docs:dev
```

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
