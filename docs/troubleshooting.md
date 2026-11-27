# AsterUI UI 故障排查

[中文](troubleshooting.md) | [English](en/troubleshooting.md)

## 没有 `window.asterui`

等待：

```js
window.addEventListener("asterui:ready", () => {
  console.log(window.asterui);
});
```

## Chrome 可打开，AsterUI 无法加载

- 检查资源是否全部使用相对路径。
- 入口必须是 `index.html`。
- 页面必须在 `AsterUI/ui` 下。
- 删除 CDN、远程字体、远程图片和外部 `fetch`。
- 查看 `AsterUI.Host.log` 是否记录了资源拦截。

## localhost 页面不加载

- `AsterUI.json` 必须启用 development。
- 来源必须包含准确端口。
- 只允许 loopback 地址。
- 只能使用 HTTP/HTTPS 开发地址。

## 页面收不到状态

- 在 `asterui:ready` 后发送请求事件。
- 请求和响应事件名必须完全一致。
- 数据必须可以 JSON 序列化。
- 响应由外部消费方决定，AsterUI 只负责传递。

## 布局或输入问题

- 使用消费方实际提供的尺寸测试。
- 控件必须位于视口内。
- 提供 hover、focus、active、disabled 状态。
- 不要假设只有鼠标输入。
