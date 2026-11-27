# AsterUI 1.1.7

This is the public engineering baseline.

## Target game

- Grand Theft Auto V Legacy Edition, Windows PC, Story Mode only.
- The Enhanced Edition is not supported.
- Requires a ScriptHookV-based external consumer at runtime.
- Not a FiveM, RedM, or GTA Online UI framework.
- The release does not include GTA V, ScriptHookV, or Rockstar game files.

## Included

- AsterUI ASI and Host source.
- `AsterUI.Demo.asi` with an F5 player and GTA waypoint teleport interface.
- Stable C ABI v1 and C++ wrapper.
- JavaScript bridge declarations and browser mock.
- Generic overlay, menu, notification, and Vite examples.
- Portable build, package, and smoke-test scripts.

## Security

- All browser resource types now pass through one allowlist.
- Production allows only package-local pages and necessary embedded data.
- Development mode allows only explicitly configured loopback origins.

## Distribution

- ScriptHookV SDK and runtime files are not included.
- HUNTER production interfaces and plugins are not included.
