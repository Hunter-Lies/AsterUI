---
layout: home

hero:
  name: AsterUI
  text: 用 HTML 构建游戏 UI
  tagline: 仅支持 Grand Theft Auto V PC 传承版（Legacy）单机剧情模式的本地 HTML/CSS/JavaScript UI 运行时。页面负责交互，插件负责业务逻辑。
  actions:
    - theme: brand
      text: 快速开始
      link: /getting-started
    - theme: alt
      text: 完整 UI 教程
      link: /ui-guide
    - theme: alt
      text: GitHub
      link: https://github.com/Hunter-Lies/AsterUI
  image:
    src: /logo.svg
    alt: AsterUI
features:
  - title: 专注 UI
    details: 页面只负责布局、动画和交互；Native、热键和游戏功能由插件处理。
  - title: JSON 通讯
    details: 使用 window.asterui 的 on、off、emit 在页面和插件之间传递结构化事件。
  - title: 本地优先
    details: 生产资源只允许本地包内文件，开发模式才允许精确 loopback 热重载。
  - title: 可预览
    details: 使用 sdk/js/mock.js 在普通浏览器中开发，再在游戏 CEF 中验证。
  - title: 全屏运行
    details: Demo 支持独占全屏、无边框全屏和窗口模式。
---

## 最小通讯示例

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    document.getElementById("value").textContent = state.value;
  });

  window.asterui.emit("myui:request", {});
});
```

## 开发路径

1. 复制 `examples/hello-overlay` 到 `AsterUI/ui/my-ui/`。
2. 在 Chrome 中引入 `sdk/js/mock.js` 预览页面。
3. 使用 Vite 和本地热重载验证交互。
4. 将全部本地资源随页面一起发布。

[开始构建第一个页面 →](/getting-started)

## 贡献者

<p align="center">
  <a href="https://github.com/Hunter-Lies/AsterUI/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=Hunter-Lies/AsterUI" alt="AsterUI contributors">
  </a>
</p>

头像根据已合并提交自动生成。详细规则见 [贡献者说明](https://github.com/Hunter-Lies/AsterUI/blob/main/CONTRIBUTORS.md)。
