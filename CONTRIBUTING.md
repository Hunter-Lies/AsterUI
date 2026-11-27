# Contributing

## Before opening a pull request

1. Build with `scripts\build.ps1`.
2. Run `scripts\package.ps1` and `scripts\test.ps1`.
3. Keep public ABI changes out of version 1.
4. Add or update a generic example when adding a browser-facing feature.
5. Do not commit CEF, ScriptHookV, generated builds, logs, or production UI.

## Code style

- C++20, `/W4`, no exceptions across the public ABI.
- Public headers must remain C-compatible except the optional `.hpp` wrapper.
- JavaScript examples use plain browser APIs unless a dependency is justified.
- Event names follow `^[A-Za-z0-9_.:\-]{1,64}$`.
- Keep resource loading deny-by-default.

## Public API compatibility

The version 1 table, `AsterBrowserDesc`, callback signatures, and entry-point
behavior are frozen. New incompatible behavior requires a new API version.
