# UI Architecture and Scope

[中文](../architecture.md) | [English](architecture.md)

Target platform: **Grand Theft Auto V Legacy Edition, Windows PC, Story Mode**.
The Enhanced Edition is not supported.

## What AsterUI provides

```text
HTML/CSS/JavaScript page
        |
        v
injected window.asterui bridge
        |
        v
AsterUI host/runtime boundary
        |
        v
external UI consumer
```

AsterUI owns the page lifecycle, browser rendering, input routing, resource
policy, and JSON event transport.

## What AsterUI does not define

- what a page means
- when a consumer should open it
- which hotkey opens it
- what a native action event does
- how application data is persisted

Those are consumer-side responsibilities and are outside the UI authoring guide.

## Page model

Each page is a separate browser surface. A consumer chooses its URL, size,
visibility, focus, layer, and opacity through the integration layer.

UI authors only need to know:

- the page runs at the assigned browser dimensions
- `window.asterui` becomes available before `asterui:ready`
- events cross the boundary as JSON
- production resources are local-only

## Resource policy

Production pages can load files from the configured UI root. Remote scripts,
images, `fetch` calls, and WebSockets are blocked. Development mode permits only
explicit exact-loopback origins for local tooling.
