# UI 预览与热重载

[中文](development.md) | [English](en/development.md)

## 静态预览

直接用 Chrome 打开页面，并引入：

```html
<script src="../../sdk/js/mock.js"></script>
```

查看页面发送的事件：

```js
window.asterui.__mock.sent()
```

## Vite 热重载

```powershell
cd examples/vite-starter
npm install
npm run dev
```

在游戏安装目录的 `AsterUI/AsterUI.json` 中启用：

```json
{
  "development": {
    "enabled": true,
    "allowedOrigins": ["http://127.0.0.1:5173"]
  }
}
```

只允许精确列出的 loopback 来源和对应 HMR WebSocket。发布页面时关闭 development。

## 测试尺寸

- 1280×720
- 1920×1080
- 2560×1440
- 鼠标、键盘、焦点、禁用状态
- 长文本和缺失字段
- 断网环境
