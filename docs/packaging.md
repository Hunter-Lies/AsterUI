# Packaging

[中文](zh-CN/packaging.md) | [English](packaging.md)

`scripts/package.ps1` creates an engine-only package:

- `AsterUI.asi`
- `AsterUI.Demo.asi`
- `AsterUI/AsterUI.Host.exe`
- the pinned CEF runtime
- `AsterUI/ui`
- default `AsterUI.json`
- public headers, SDK, examples, licenses, and checksums

The package must not contain `ScriptHookV.dll`, `dinput8.dll`, HUNTER plugins,
production UI, cache files, or logs.

Before publishing, run `scripts/test.ps1` and verify `SHA256SUMS.txt`.
