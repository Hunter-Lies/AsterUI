# 打包说明

[中文](packaging.md) | [English](../packaging.md)

`scripts/package.ps1` 生成仅包含 AsterUI 的发布包：

- `AsterUI.asi`
- `AsterUI.Demo.asi`
- `AsterUI/AsterUI.Host.exe`
- 固定版本 CEF 运行时
- `AsterUI/ui`
- 默认 `AsterUI.json`
- 公开头文件、SDK、示例、许可证和校验文件

发布包不能包含：

- `ScriptHookV.dll`
- `dinput8.dll`
- HUNTER 业务插件
- 生产 UI
- 缓存
- 日志

发布前运行 `scripts/test.ps1`，并验证 `SHA256SUMS.txt`。
