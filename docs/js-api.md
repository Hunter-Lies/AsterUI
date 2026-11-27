# AsterUI JavaScript 通讯接口

[中文](js-api.md) | [English](en/js-api.md)

## 就绪事件

```js
window.addEventListener("asterui:ready", () => {
  // window.asterui 已可用
});
```

## 方法

```ts
on(name: string, callback: (payload: unknown) => void): () => void
off(name: string, callback?: (payload: unknown) => void): boolean
emit(name: string, payload?: unknown): void
```

## 订阅

```js
const remove = window.asterui.on("demo:state", (payload) => {
  console.log(payload);
});

remove();
```

## 发送

```js
window.asterui.emit("demo:command", {
  action: "refresh"
});
```

## 事件名

```text
^[A-Za-z0-9_.:\-]{1,64}$
```

推荐格式：

```text
<页面命名空间>:<动作>
```

例如：

- `inventory:request`
- `inventory:state`
- `inventory:use`
- `notification:show`

## 数据规则

- 必须可以 JSON 序列化。
- 保持数据体积较小。
- 不建议发送无结构字符串。
- AsterUI 不解释 `action` 等业务字段。
- 业务含义由外部消费方决定。

## 兼容别名

`window.gtanui` 和 `gtanui:ready` 只用于兼容旧页面。新页面必须使用 `window.asterui` 和 `asterui:ready`。
