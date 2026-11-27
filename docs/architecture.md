# UI 架构与范围

[中文](architecture.md) | [English](en/architecture.md)

目标平台：**Grand Theft Auto V PC 传承版（Legacy），Windows，单机剧情模式**，不支持增强版。

## 架构

```text
HTML/CSS/JavaScript 页面
        |
        v
注入的 window.asterui
        |
        v
AsterUI Host / 运行时边界
        |
        v
外部 UI 消费方
```

AsterUI 负责页面生命周期、浏览器渲染、输入路由、资源策略和 JSON 通讯。

## 外部消费方负责

- 什么时候打开页面
- 热键
- 页面尺寸、焦点、层级
- 事件对应的游戏功能
- 数据持久化

这些内容不属于 AsterUI UI 开发教程。

## 页面模型

- 每个页面是独立的浏览器 surface。
- 页面收到分配好的尺寸。
- `asterui:ready` 触发后桥接可用。
- 页面和消费方之间只传 JSON。
- 生产环境只允许本地包内资源。
