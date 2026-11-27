# AsterUI SDK

- `js/asterui.d.ts`: TypeScript declarations for the injected browser bridge.
- `js/mock.js`: browser-only mock for local UI development.

Pages should use `window.asterui`. The `window.gtanui` alias exists only for
backward compatibility and will be removed in a future API version.

The C/C++ headers outside this folder describe the external integration
surface. Plugin development is outside the AsterUI UI authoring guide.
