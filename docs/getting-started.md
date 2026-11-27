# AsterUI 页面快速开始

[中文](getting-started.md) | [English](en/getting-started.md)

本教程面向 UI 作者，不需要 Visual Studio 或 ScriptHookV SDK。

目标平台：**Grand Theft Auto V PC 传承版（Legacy），Windows，单机剧情模式**。不支持增强版（Enhanced）、FiveM、RedM 或 GTA Online。

## 创建页面

复制：

```text
examples/hello-overlay
```

放到：

```text
AsterUI/ui/my-ui/
```

入口必须是 `index.html`。

## 第一条桥接脚本

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    document.getElementById("value").textContent = state.value;
  });

  window.asterui.emit("myui:request", {});
});
```

## 浏览器预览

```html
<script src="../../sdk/js/mock.js"></script>
<script src="app.js"></script>
```

Mock 实现 `on`、`off`、`emit`，并触发 `asterui:ready`。

## 下一步

- [JavaScript 通讯接口](/js-api)
- [完整 UI 教程](/ui-guide)
- [预览和热重载](/development)
