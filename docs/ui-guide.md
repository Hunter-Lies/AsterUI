# AsterUI UI 开发教程

[中文](ui-guide.md) | [English](en/ui-guide.md)

本教程只说明如何为 AsterUI 开发页面，不包含插件、Native 或钩子开发。

## 1. 理解模型

AsterUI 为页面提供一个全局桥接对象：

```js
window.asterui
```

页面通过 `emit` 发送 JSON 事件，通过 `on` 接收 JSON 状态。页面不直接执行游戏功能。事件由外部 UI 消费方接收和处理。

和 RageUI 的角色类似：AsterUI 负责 UI 层，消费方负责业务逻辑。

## 2. 页面结构

```text
AsterUI/ui/my-ui/
  index.html
  style.css
  app.js
  images/
```

资源必须使用相对路径：

```html
<link rel="stylesheet" href="style.css">
<img src="images/icon.png" alt="">
<script src="app.js"></script>
```

生产环境禁止 CDN、远程字体、远程图片、外部 `fetch` 和外部 WebSocket。

## 3. 生命周期

HTML 刚开始执行时桥接可能尚未安装，必须等待 `asterui:ready`：

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", render);
  window.asterui.emit("myui:ready", {
    page: "my-ui",
    version: 1
  });
});
```

推荐事件：

- `myui:ready`：页面已准备好。
- `myui:request`：请求最新状态。
- `myui:action`：发送用户操作。
- `myui:state`：消费方回传页面状态。

## 4. 通讯

订阅：

```js
const unsubscribe = window.asterui.on("myui:state", (payload) => {
  console.log(payload);
});

unsubscribe();
```

发送：

```js
window.asterui.emit("myui:action", {
  action: "set_value",
  value: 42
});
```

规则：

- 事件名匹配 `^[A-Za-z0-9_.:\-]{1,64}$`。
- 数据必须可以 JSON 序列化。
- 使用页面自己的命名空间。
- 不要传递 DOM、函数、Blob 或循环对象。
- 事件名称本身不定义游戏功能，功能含义由外部消费方决定。

推荐请求/响应模式：

```js
window.asterui.emit("inventory:request", {});
window.asterui.on("inventory:state", renderInventory);
window.asterui.emit("inventory:use", { id: 12 });
```

## 5. 页面设计

- 页面运行在透明全屏浏览器中。
- 1280×720、1920×1080、2560×1440 下都必须可用。
- 使用相对布局、`rem` 或 `clamp`。
- 给按钮提供 hover、focus、active、disabled 状态。
- 支持鼠标和键盘操作。
- 不依赖远程字体或图标 CDN。

## 6. 浏览器预览

```html
<script src="../../sdk/js/mock.js"></script>
<script src="app.js"></script>
```

Mock 提供相同的 `on`、`off`、`emit`，并触发 `asterui:ready`。

查看已发送事件：

```js
window.asterui.__mock.sent()
```

## 7. Vite 热重载

```powershell
cd examples/vite-starter
npm install
npm run dev
```

在游戏安装目录的 `AsterUI/AsterUI.json` 中启用精确来源：

```json
{
  "development": {
    "enabled": true,
    "allowedOrigins": ["http://127.0.0.1:5173"]
  }
}
```

只有精确配置的 loopback 来源和对应 HMR WebSocket 会被允许。发布前关闭 development。

## 8. 调试

- `window.asterui` 不存在：等待 `asterui:ready`。
- Chrome 正常、游戏空白：检查相对路径和 `AsterUI.Host.log`。
- 收不到状态：确认请求事件、响应事件和名称完全一致。
- 页面接收不到点击：检查页面是否获得焦点，以及控件是否在可视区域内。

## 9. 发布页面

页面目录放在：

```text
AsterUI/ui/<page-name>/
```

必须包含全部本地资源，不包含 localhost 地址、机器绝对路径、缓存和日志。

AsterUI 不自动发现页面，也不定义菜单入口。页面何时加载由外部消费方决定。

## 10. 范围边界

AsterUI UI 教程包含：

- HTML/CSS/JavaScript
- 页面生命周期
- `on/off/emit` 通讯
- 资源路径
- 预览、热重载和调试
- 渲染、输入和资源策略

不包含：

- ASI/RPH 插件创建
- Native 调用
- 热键注册
- 页面启动策略
- 消费方业务逻辑
