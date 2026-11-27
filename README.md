# AsterUI

[中文](#中文) | [English](#english)

本地 HTML/CSS/JavaScript 游戏 UI 运行时，定位类似 RageUI：AsterUI 负责页面、渲染、输入和事件桥接；外部插件负责何时显示页面以及如何处理事件。

Local HTML/CSS/JavaScript UI runtime for GTA V. The responsibility boundary is similar to RageUI: AsterUI owns rendering, input, page lifecycle, and the event bridge; an external consumer owns page launch policy and game actions.

> 本文档只讲 UI 开发。插件、Native 和钩子开发不属于 AsterUI UI 教程范围。  
> This documentation covers UI authoring only. Plugin, native, and hook development are outside its scope.

---

## 中文

### 开始使用

1. 复制 `examples/hello-overlay`。
2. 放到 `AsterUI/ui/my-ui/`。
3. 页面入口必须是 `index.html`。
4. 等待 `asterui:ready` 后使用通讯接口：

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    console.log(state);
  });

  window.asterui.emit("myui:request", {
    page: "my-ui"
  });
});
```

### 页面原理

```text
HTML 交互
  -> window.asterui.emit("myui:action", 数据)
  -> 外部 UI 消费方
  -> 外部 UI 消费方发送 myui:state
  -> window.asterui.on 接收 JSON
  -> 更新 HTML
```

AsterUI 只负责页面生命周期、CEF 渲染、鼠标键盘输入、资源白名单和 JSON 事件传输。事件叫什么、表示什么游戏功能，由外部消费方决定。

### 目录结构

```text
AsterUI/ui/my-ui/
  index.html
  style.css
  app.js
  images/
```

所有资源使用相对路径。生产环境不允许 CDN、远程字体、远程图片、外部 `fetch` 或外部 WebSocket。

### 浏览器预览

在应用脚本前引入 Mock：

```html
<script src="../../sdk/js/mock.js"></script>
<script src="app.js"></script>
```

Mock 提供与游戏内相同的 `on`、`off`、`emit`，并触发 `asterui:ready`。发送记录可以通过 `window.asterui.__mock.sent()` 查看。

### 本地热重载

在 `examples/vite-starter` 中运行：

```powershell
npm install
npm run dev
```

然后在游戏安装目录的 `AsterUI/AsterUI.json` 中启用精确来源：

```json
{
  "development": {
    "enabled": true,
    "allowedOrigins": ["http://127.0.0.1:5173"]
  }
}
```

外部消费方把页面地址指向 `http://127.0.0.1:5173/` 后，Vite HMR 会在游戏 CEF 页面中生效。发布页面时必须关闭 development。

### 文档

- [完整 UI 教程](docs/zh-CN/ui-guide.md)
- [快速开始](docs/zh-CN/getting-started.md)
- [JavaScript 通讯](docs/zh-CN/js-api.md)
- [预览与热重载](docs/zh-CN/development.md)
- [UI 架构与边界](docs/zh-CN/architecture.md)
- [故障排查](docs/zh-CN/troubleshooting.md)

### 示例

- `demo-ui`：玩家状态、玩家操作、GTA 地图导航点检测和路点传送。
- `hello-overlay`：最小通讯示例。
- `interactive-menu`：标签页、开关和输入。
- `notification`：事件驱动的通知。
- `vite-starter`：localhost HMR。

`AsterUI.Demo.asi` 只是用于启动 Demo 的消费方示例，不属于 UI 开发教程。

---

## English

### Quick start

1. Copy `examples/hello-overlay`.
2. Place it under `AsterUI/ui/my-ui/`.
3. The entry point must be `index.html`.
4. Wait for `asterui:ready` before using the bridge:

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    console.log(state);
  });

  window.asterui.emit("myui:request", {
    page: "my-ui"
  });
});
```

### UI model

```text
HTML interaction
  -> window.asterui.emit("myui:action", payload)
  -> external UI consumer
  -> consumer sends myui:state
  -> window.asterui.on receives JSON
  -> HTML update
```

AsterUI owns page lifecycle, CEF rendering, mouse and keyboard routing, resource allowlists, and JSON event transport. The external consumer owns event semantics, page launch policy, hotkeys, and native game actions.

### Page structure

```text
AsterUI/ui/my-ui/
  index.html
  style.css
  app.js
  images/
```

Use relative local asset paths. Production pages cannot use CDNs, remote fonts, remote images, external `fetch`, or external WebSockets.

### Browser preview

```html
<script src="../../sdk/js/mock.js"></script>
<script src="app.js"></script>
```

The mock provides `on`, `off`, `emit`, and dispatches `asterui:ready`. Inspect outgoing events with `window.asterui.__mock.sent()`.

### Localhost hot reload

```powershell
cd examples/vite-starter
npm install
npm run dev
```

Enable the exact origin in the installed `AsterUI/AsterUI.json`:

```json
{
  "development": {
    "enabled": true,
    "allowedOrigins": ["http://127.0.0.1:5173"]
  }
}
```

Point the external consumer at `http://127.0.0.1:5173/` to use HMR inside the game CEF page. Disable development mode in released packages.

### Documentation

- [Full UI guide](docs/ui-guide.md)
- [Getting started](docs/getting-started.md)
- [JavaScript bridge](docs/js-api.md)
- [Preview and hot reload](docs/development.md)
- [Architecture and scope](docs/architecture.md)
- [Troubleshooting](docs/troubleshooting.md)

### Examples

- `demo-ui`: player state, player actions, GTA waypoint detection, and waypoint teleport.
- `hello-overlay`: minimal bridge handshake.
- `interactive-menu`: tabs, switches, pointer input.
- `notification`: event-driven notification.
- `vite-starter`: localhost HMR.

`AsterUI.Demo.asi` is only a consumer-side demonstration and is not part of the UI authoring tutorial.

---

## Engine maintainers / 引擎维护

```powershell
.\scripts\bootstrap.ps1 -ScriptHookVSdk C:\SDK\ScriptHookV
.\scripts\build.ps1
.\scripts\package.ps1
.\scripts\test.ps1
```

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
