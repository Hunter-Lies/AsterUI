# AsterUI 页面快速开始

[中文](getting-started.md) | [English](en/getting-started.md)

本教程面向 UI 作者，不需要 Visual Studio 或 ScriptHookV SDK。

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

- 通讯接口：`js-api.md`
- 完整教程：`ui-guide.md`
- 预览和热重载：`development.md`
