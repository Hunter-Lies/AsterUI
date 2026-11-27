# UI JavaScript Bridge

[中文](../js-api.md) | [English](js-api.md)

AsterUI injects one global bridge into each page.

## Ready event

Do not call the bridge during initial HTML parsing. Wait for:

```js
window.addEventListener("asterui:ready", () => {
  // safe to use window.asterui
});
```

## Methods

```ts
on(name: string, callback: (payload: unknown) => void): () => void
off(name: string, callback?: (payload: unknown) => void): boolean
emit(name: string, payload?: unknown): void
```

## Subscribe

```js
const remove = window.asterui.on("demo:state", (payload) => {
  console.log(payload.health);
});

remove();
```

The returned function is equivalent to `off(name, callback)`.

## Send

```js
window.asterui.emit("demo:command", {
  action: "set_invincible",
  value: true
});
```

`emit` serializes the payload as JSON and sends it to the external consumer.

## Event names

Allowed pattern:

```text
^[A-Za-z0-9_.:\-]{1,64}$
```

Recommended pattern:

```text
<page-namespace>:<action>
```

Examples:

- `inventory:request`
- `inventory:state`
- `fuel:set`
- `notification:show`

## Payload rules

- Must be JSON serializable.
- Prefer objects with explicit keys.
- Keep messages small.
- Do not send DOM nodes, functions, Blobs, or cyclic objects.
- The bridge does not interpret application fields such as `action`; their
  meaning belongs to the external consumer.

## Lifecycle pattern

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", render);
  window.asterui.emit("myui:request", { page: "home" });
});
```

The page asks for state. The consumer responds with an application event. The
page renders the event payload.

## Compatibility

`window.gtanui` and `gtanui:ready` are legacy aliases. New UI code must use
`window.asterui` and `asterui:ready`.
