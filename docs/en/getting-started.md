# Getting Started with AsterUI Pages

[中文](../getting-started.md) | [English](getting-started.md)

This guide assumes you are making a UI page, not an ASI/RPH plugin.

Target platform: **Grand Theft Auto V, Windows PC, Story Mode**. AsterUI is not
a FiveM, RedM, or GTA Online UI framework.

## Tools

- A text editor or IDE.
- A normal browser for preview.
- Optional: Node.js for the Vite example.

You do not need Visual Studio or the ScriptHookV SDK to author a UI page.

## Create a page

Copy an example:

```text
examples/hello-overlay
```

Place your copy under:

```text
AsterUI/ui/my-ui/
```

The entry file must be `index.html`.

## First script

```js
window.addEventListener("asterui:ready", () => {
  window.asterui.on("myui:state", (state) => {
    document.getElementById("value").textContent = state.value;
  });

  window.asterui.emit("myui:request", {});
});
```

The external consumer receives `myui:request` and may respond with:

```js
// Conceptual consumer-side event sent to the page.
window.asterui.on("myui:state", (state) => {});
```

In the page, the event arrives through the registered callback above.

## Browser preview

Include the mock before your app script:

```html
<script src="../../sdk/js/mock.js"></script>
<script src="app.js"></script>
```

The mock implements `on`, `off`, and `emit` and dispatches `asterui:ready`.

## Next

- Communication details: `js-api.md`
- Full authoring guide: `ui-guide.md`
- Preview and HMR: `development.md`
