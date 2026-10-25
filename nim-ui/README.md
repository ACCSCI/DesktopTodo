# nim_duilib native frontend

This is a separately packaged C++20 frontend candidate for the existing Todo
product. Domain rules, SQLite, storage management, startup, menus and window
policy remain shared with `app/`. It does not use a browser runtime.

Build prerequisites: Windows x64, Visual Studio 2022 C++ Build Tools with CMake
and a Windows 10/11 SDK, Node.js, Python 3, Git and 7-Zip.

```powershell
pwsh -NoProfile -File scripts/bootstrap-app.ps1
pwsh -NoProfile -File scripts/bootstrap-nim-ui.ps1
npm run build:nim-ui
npm run test:nim-ui
```

Output: `dist/nim-ui/DesktopTodo-Nim.exe`. Keep the `resources` directory beside
the executable when moving this portable folder. Its default database and INI
are beside this executable, separate from the existing `dist/DesktopTodo.exe`.
Do not point both running versions at the same database.

The bootstrap pins nim_duilib, its Skia patch repository, Skia and LLVM; complete
download SHA-256 checks protect the two large archives. Dependency and build
files remain in ignored `.tools/nim-duilib`. First setup downloads several
hundred MB and builds Skia. Runtime rendering uses the CPU raster backend;
CEF, WebView2 and SDL integration are explicitly disabled in the target.

The adapter owns library controls through `NimSurface`. XML defines the note
layout; title, tasks and footer use RichEdit2 and library controls. Wheel zoom is
disabled on editors. Existing independent Win32 lock buttons retain their
configured position and click-through policy.

`test:nim-ui` builds the real executable and runs isolated application (55),
storage (20), wheel-size (3) and cross-process focus (4) checks. It never edits the user's real database
or real startup registry key. The same gate runs through Lefthook. Existing
`npm run check` and `npm run test:native` remain required.

Visual checks verified Chinese input and Return-to-add in an ordinary window.
This is an integration candidate, not a finished visual redesign: the manager,
menus and settings still use the existing native UI. Explorer/Win+D acceptance,
IME composition edge cases, accessibility and drag auto-scroll need further
interactive verification before replacing the existing frontend by default.
One storage restart timed out during concurrent interactive testing; an
isolated rerun passed. This transient failure has not been diagnosed.

The pinned upstream focus handler originally treated another process's window
userdata as a local pointer, causing a crash when two instances exchanged
focus. `scripts/patch-nim-ui.py` applies a checked, idempotent process-ownership
guard before building. The compiled two-process focus contract reproduces the
old failure and verifies the fix. The ownership patch is independent of skins.

Task reorder feedback uses nim_duilib AnimationPlayer with a 140 ms cubic ease
and 16 ms frames. It animates a preview gap, follows the pointer, and settles
rows into committed order. No animation timer runs after settlement. Escape or
capture loss cancels the preview. `test-drag-motion.py` checks real control
routing, intermediate frames, SQLite order timing and idle settlement through
the opt-in automation IPC; preview rules also have independent headless tests.
The candidate has 12 DIP extra toolbar breathing room and 2 DIP gaps between
three equal 20 DIP icon hit areas. Its logical row pitch remains 38 DIP so
sorting, scrolling and cross-note drop targets use the same geometry.

Locked desktop notes retain click-through editing protection but accept wheel
scrolling on their exposed surface. A lightweight WH_MOUSE_LL callback checks
only wheel events, validates desktop exposure and posts a read-only scroll
message without changing focus. An overlying application retains its wheel
events. The compiled locked-wheel suite verifies registration, native dispatch,
read-only state, stable bounds and equal footer padding at two window sizes.

## Wheel-size regression

The existing desktop renderer submitted a client-sized bitmap through
`UpdateLayeredWindow`. A non-client scrollbar reduced that width and caused
successive wheel/paint cycles to resize the entire note (680 to 612 physical
pixels in the reproducer). Desktop mode now draws a client-only scroll thumb;
the library frontend uses its own client scrollbar. The compiled regression
asserts unchanged bounds after eight wheel/paint cycles in all three modes.
