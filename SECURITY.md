# Security Policy

## Trust model

AsterUI pages are local code with access to the injected JavaScript bridge.
Only install and load UI packages from sources you trust.

Production mode blocks external resource loading and permits only pages under
the configured `AsterUI/ui` root. The browser intentionally runs without the
Chromium sandbox because it is embedded in the game process environment; do
not expose development origins to untrusted networks.

## Reporting

Do not open a public issue for a vulnerability. Report the affected version,
reproduction steps, and impact privately to the maintainers once the public
repository owner is selected.
