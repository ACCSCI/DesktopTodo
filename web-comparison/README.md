# Svelte / system WebView2 resource experiment

This independent experiment keeps `dist/DesktopTodo.exe` and its portable data intact.
It uses the installed Microsoft Edge WebView2 Runtime, not a bundled Chromium distribution.
The shipped Microsoft loader is about 190 KiB; browser/GPU/renderer processes still consume memory.

## Existing integrations

- Svelte 5 + Flowbite Svelte: buttons, forms, checkboxes and selectors.
- Lucide: SVG icons; svelte-dnd-action: drag and keyboard reordering.
- webview/webview 0.12.0: C++ host, RPC bridge and message loop.
- Official Microsoft WebView2 loader and virtual-host folder mapping.
- Existing C++ domain actions, controller and SQLite store; no duplicate business rules in JavaScript.
- No UI polling, remote fonts or CDN assets. Native changes push snapshots; rendering readiness uses two animation frames.

## Run

Requires the existing app toolchain (Zig 0.16.0 / SQLite), Node 24 and installed WebView2 Runtime.

```powershell
./scripts/bootstrap-app.ps1
./scripts/bootstrap-web-comparison.ps1
npm run test:app
npm run build:web
./scripts/launch-web-comparison.ps1 -Notes 1 -Tasks 10
```

The launcher creates disposable isolated data and browser profiles under `.tools/web-comparison`.
Every launch seeds an experiment workload; this executable is not for storing real tasks.
The host refuses to reseed an existing database without its comparison fixture marker.
Downloaded third-party sources and all binaries remain ignored. Bootstrap verifies SHA-256 pins.

## Tests and comparison

```powershell
npm run check
npm run test:native
python scripts/test-web-comparison.py
python scripts/compare-resources.py --trials 3 --idle 10
python scripts/summarize-web-comparison.py
```

Python measurement scripts require `psutil==7.2.2` (already available on the development machine).
Install it elsewhere with `python -m pip install -r web-comparison/requirements.txt`.
Unit tests cover bridge absence, native supplied filtering, component actions and stable lock placement.
Compiled integration exercises actual WebView2 views, SQLite mutations and rendering acknowledgments.
Front-end unit tests are part of `npm run check`, therefore also covered by Lefthook.

The comparison sums the complete process tree, including browser, GPU, renderers and utilities.
Private bytes are committed private memory, not resident memory. Summed working sets double-count shared pages.
All note windows overlap at the same 680×760 physical bounds; this does not measure ten simultaneously visible cards.
Each fresh run uses a new database/profile; the last trial also reuses its profile once.
These are fresh-profile launches, not an OS cold-cache experiment.

Startup paths differ: the native app creates its workload through automation after launch;
the web host seeds it before creating views. Do not interpret those startup numbers as a strict speed ratio.
Status timing includes IPC/snapshot I/O and persistence, not UI rendering completion.
Animation frames pause for occluded views; they are not awaited for command timing and do not prove GPU presentation.
There is no snapshot polling during the idle CPU sample. CPU percent is relative to one core.

## Scope

The web experiment supports multiple notes, adding tasks, status, native filtering/reordering,
fixed header lock/unlock controls, ordinary and topmost windows. The native product remains authoritative.
Desktop attachment / Explorer behavior / OS mouse click-through, storage settings, backups,
startup registration, note title/color editing and full task editing have not been wired into this experiment.
Desktop mode reports an explicit error; the visual lock only disables editing controls.
The performance results cannot establish parity with the complete native product.

A single WebView containing several cards could reduce renderer overhead, but independent OS windows
would then require additional architecture. That variant has not been measured here.
