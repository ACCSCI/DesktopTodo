# Project rules

- Build a lightweight Windows desktop Todo application with multiple notes,
  desktop-attached, topmost and ordinary-window modes, locking and startup.
- Keep domain rules independent from the UI, SDK and Win32 implementation.
  Use explicit ports for window policies, startup and persistence.
- Every feature must have a headless test path before connecting its UI.
  Use real compiled application tests for platform contracts; mocks alone do not
  prove native integration. Verify Explorer behavior separately on Windows.
- Every project-owned source, test, configuration and document file must have
  fewer than 600 lines. Split by responsibility, not arbitrary numbered pieces.
- Run `npm run check` and `npm run test:native` before delivery. Report failures
  honestly. Do not bypass Lefthook or turn missing tests into passing skips.
- Pinned Native SDK 0.11.0 source lives under ignored `.tools/native-0.11.0`.
  Use `scripts/bootstrap.ps1` and `scripts/native.mjs` for reproducible setup.
- Keep dependency and generated files out of version control. Never include
  `.tools`, `.native`, `.zig-cache`, `zig-out` or `node_modules` in line gates.
- The current `prototype` and `window-probe` are feasibility experiments.
  Do not present their screenshots or performance as a finished Todo product.
- The product is C++20/Win32/SQLite under `app/`. Bootstrap it with
  `scripts/bootstrap-app.ps1`; build with `npm run build:app`.
- Run `python scripts/test-app-integration.py` for executable integration using
  isolated data and registry keys. Never use the real Run key in automated tests.
- Default portable data is beside the executable. Storage settings live in
  `DesktopTodo.ini`; test relocation with `scripts/test-storage-management.py`.
- Global data clearing requires exactly three confirmations and must not create
  a backup. Preserve existing manual and automatic backups.
