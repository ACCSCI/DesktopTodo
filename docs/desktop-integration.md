# Desktop mode integration — 2026-10-23

## Implemented in the feasibility probe

- Pure window-mode policy: desktop, topmost, ordinary window, locked body and
  separate interactive control. State and count survive switching modes.
- An injected attachment transaction with rollback of parent, styles and frame
  if any attachment step fails; backend operations stay outside the reducer.
- A Win32 adapter changes only this process's windows. It locates the existing
  visible top-level host containing SHELLDLL_DefView (Progman on this machine).
  No undocumented Explorer message is sent; no wallpaper or startup setting
  is changed. Desktop notes are child windows above the icon view within their
  own bounds, below ordinary application windows.
- The SDK event decorator preserves optional callback nullness so native-only
  scenes do not accidentally enter the SDK's WebView boot path. Re-entrant
  native events during SetParent/SetWindowPos cannot re-enter attachment.
- Mode/lock changes rebuild the required windows while preserving application
  state. The control is raised above a new body only on handle changes, rather
  than repeatedly raising it on idle events.
- Authored positions are converted using the actual window DPI, including
  negative monitor coordinates. Probe note bounds are now (1080,240)-(1680,600)
  for authored (540,120) and a 300x180 view at this machine's 200% scale.

## Evidence from real Windows integration

`npm run test:desktop` passes against a compiled Windows executable. Read-only
Win32 inspection independently verifies actual native state, not SDK snapshots:

- Desktop note and control have the same Progman parent and WS_CHILD.
- Desktop mode clears WS_EX_TOPMOST and taskbar-forcing styles.
- Locked body has transparent input style; control remains interactive.
- Unlock rebuilds the body and preserves desktop attachment.
- Topmost mode restores a top-level window with native topmost style.
- Ordinary mode restores a top-level non-topmost window with a native caption.

Native records are in desktop-native-facts.json; collection excludes anonymous
render surfaces and IME internals to keep the report focused and under 600 lines.

## Actual mouse input

Computer Use drove real mouse clicks, with a separate Python/Tk process under
our overlay. The SDK's synthetic widget-click command was not used for these
four input checks:

1. Click the locked note's visible button area: receiver count becomes 1 while
   note count remains 0. This proves cross-process click-through at that point.
2. Click the independent Unlock control: note becomes interactive.
3. Click the note button: note count becomes 1 and receiver stays at 1.
4. Click Lock on the surviving control: note returns to its locked state.

Switching that note to desktop mode makes the ordinary receiver occlude it.
WindowFromPoint at the note center resolves to the receiver process, and the
receiver screenshot shows its own content without the note painted over it.
Results are in desktop-mouse-results.json. Computer Use screenshots were
inspected in-session; they were not exported as reference-renderer evidence.

The initial real-mouse run exposed the control being covered by a recreated
body. The raise-on-handle-change policy fixes it, and the four-step real-mouse
sequence above was repeated successfully on the corrected build.

## Headless coverage

The Zig suite passes 15/15 tests and includes scaffold rendering/effects, mode/lock policies, attachment
styles, transaction rollback at every mutating step, invalid handles, DPI
conversion, control ordering and preservation of SDK event callbacks. These
run without touching Explorer. The separate compiled TypeScript smoke remains
part of the full native gate. Every owned text file remains <600 lines.
The final Lefthook pre-commit run passes both native suites and the line gate.

## Boundaries and pending verification

- Win+D / Show Desktop remains awaiting manual verification. Computer Use
  explicitly forbids Windows-key shortcuts, and its returned window inventory
  does not expose a taskbar/desktop target suitable for an equivalent click.
  A human verification request was sent while the probe was left in desktop
  mode. Do not infer a pass from the parent relationship alone.
- Actual mouse checks above cover topmost lock/unlock; desktop child styles and
  reconstruction are verified separately. Actual input with the desktop fully
  exposed still needs that manual check.
- This strategy displays the card above desktop icons within its rectangle;
  placing it underneath icons would need a different rendering/input design.
- Explorer restart, display hotplug, multi-monitor DPI transitions, virtual
  desktops, multiple Windows builds and wallpaper-software coexistence have
  not been exercised. Host lookup failures log an error and do not claim success.
- Bounds and sizes are fixed prototype values. User configuration, persistence,
  full Todo features, tray and startup are still future product work.
- No production low-resource target is claimed from this experiment.

## Commands

    npm run test:native
    npm run test:desktop
    npm run check
    lefthook run pre-commit

Manual fixture tools: scripts/click-receiver.py and scripts/inspect-windows.py.
The receiver process is closed after the input test; the desktop probe remains
available temporarily for the requested manual Show Desktop check.

## API references

SetParent and the required WS_CHILD/WS_POPUP transition:
https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent

Layered child windows, taskbar and topmost styles:
https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
