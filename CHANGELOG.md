# Changelog

## 1.1.8 - Demo and documentation refresh

- Reworked and documented the bilingual F5 demo console.
- Documented fullscreen support without requiring windowed mode.
- Added the in-game demo screenshot to the README.
- Unified public terminology around "plugin" and removed obsolete architecture wording.
- Aligned CEF rendering with physical game pixels for a 1:1 shared-frame output.
- Simplified waypoint teleport to resolve ground synchronously and avoid high-altitude fallback.
- Added clearer player, vehicle, teleport, world, and architecture reference pages.

## 1.1.7 - Public engineering baseline

- Added public C ABI v1 headers and a header-only C++ wrapper.
- Added TypeScript declarations and a browser preview mock.
- Added package-local request filtering for all subresources.
- Added opt-in exact-loopback development origins for localhost HMR.
- Replaced machine-specific build assumptions with environment-driven CMake.
- Added dependency lock metadata and CEF bootstrap verification.
- Added generic overlay, menu, notification, and Vite examples.
- Added an installable F5 demo ASI with player actions and waypoint teleport.
- Added open-source governance, security, and SDK documentation.
