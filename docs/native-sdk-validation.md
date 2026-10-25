# Native SDK feasibility validation — 2026-10-22

## Result

Native SDK 0.11.0 now builds and runs on this Windows machine. The compiled
TypeScript headless test and Zig window probe both pass. It is viable for
further prototyping; desktop attachment and production resource targets remain
unproven. This is a feasibility experiment, not the finished Todo application.

## Verified

- Node 24.22.0, Zig 0.16.0, Native CLI 0.11.0 (commit 16f2957).
- TypeScript: all 26 build steps succeed; compiled button input and deterministic
  replay test passes. SDK semantic analysis remains part of the test graph.
- Zig window probe: 7/7 tests pass, including event dispatch, layout, effects,
  independent overlay policies, and separate interactive unlock control.
- Windows ReleaseFast executable builds: 8,188,416 bytes (about 7.81 MiB).
- SDK automation smoke passes: native main window renders through Direct2D,
  increments state, creates transparent topmost note plus unlock control,
  unlocks, modifies shared state from note, relocks, and closes auxiliary views.
- Screenshots inspected: docs/images/main-canvas.png, note-canvas.png,
  unlock-canvas.png. They are SDK reference-renderer captures, not proof of
  Windows desktop composition or external-process mouse hit testing.
- All project-owned text files must be <600 lines. The gate tests accept 599,
  reject 600 with or without a trailing newline, and cover extensionless files.
- Lefthook pre-commit and pre-push run quality and both native test suites.
  Root CI configuration repeats these checks; remote CI has not been run.

## Installation and SDK fixes

1. The npm package omits tests/ts-core/gpu-dashboard-reference/main.zig, which
   build.zig embeds. Use the complete v0.11.0 source archive rather than the
   incomplete framework bundled in npm. The npm CLI is still used as launcher.
   Remote tag resolves to 16f29572307c2cc133907537c4dfd98046d4e0f4.
2. Node installer transfers timed out. Curl ranged downloads and a registry
   transport mirror work. All release archives retain upstream SHA-256 checks;
   npm package bytes retain upstream SHA-512 checks. The unchanged upstream
   installer additionally verifies versions, ABI, inventories and artifact
   digests before publishing its complete installation receipt.
3. On Windows, the SDK's semantic-analysis build-obj fails with:
   'coff does not support linking multiple objects into one'. The compiled
   headless test itself passes, but this makes the overall test command fail.
   scripts/patch-sdk.mjs changes ONLY this Windows analysis artifact to a static
   library. The semantic check is retained and all 26 steps then pass. Other
   platforms keep the original build-object path. This is a local patch,
   not an upstream fix. bootstrap.ps1 reapplies it reproducibly.
4. Release scaffold retained markup file polling. The probe now enables its
   watch_path only in Debug, following the SDK's MarkupOptions guidance.
5. SDK overlay flags are immutable after window creation. The probe changes
   the note's identity when lock changes and retains a separate control.
6. SDK snapshot window tables retain closed-window records without an open
   marker. Shutdown assertions check disappearance of live views, not titles.

Upstream scripts/gate.sh fast was attempted but cannot resolve main/HEAD in
this archive-based SDK checkout. The full upstream framework suite is NOT
claimed as passing; project compiled tests and Windows smoke are the evidence.

## Preliminary resource sample

See probe-idle-measurement.json. ReleaseFast Zig probe with automation enabled,
main window only, after exercising and closing auxiliary windows:

- 10.075 seconds wall time; 0.171875 seconds process CPU time.
- Working set: 64.38 MiB; private bytes: 196.77 MiB; 882 handles.
- CPU corresponds to approximately 1.7% of one logical core over this sample,
  not a machine-wide Task Manager percentage.

Before disabling release markup polling, a separate sample used 0.421875 CPU
seconds over 10.073 seconds. Samples are short, not controlled benchmarks and
include automation overhead. Private bytes are not resident RAM. No claim of
meeting a low-resource product target is justified yet. A production build
without automation and longer multi-note tests are still needed.

## Reproduce

Run from the repository root in PowerShell:

    ./scripts/bootstrap.ps1
    npm run check
    npm run test:native
    npm run build:probe
    ./scripts/windows-smoke.ps1
    lefthook run pre-commit

bootstrap requires Node 24+, compatible Zig 0.16.0 (or CLI download), git,
Windows curl/tar, and network access. Dependencies and generated outputs live
under ignored directories. No installed task or startup setting is changed.

## Still unverified

Update on 2026-10-23: desktop attachment, native mode transitions and real
cross-process mouse input now have additional evidence in
[desktop integration](desktop-integration.md). The remaining list below records
the scope of this earlier validation; use the newer report for current status.

- Explorer/WorkerW desktop attachment, ordinary-window occlusion and Win+D.
- Actual mouse click-through to another process and unlock hit testing.
- Windows startup integration, tray recovery, Explorer restart and multi-monitor
  recovery. No startup setting was enabled during this experiment.
- Chinese IME and registered CJK fonts.
- Production CPU/RAM budget, long-running stability and behavior with 10 notes.
- Todo persistence, sorting, filters and other product functionality.

Recommendation: Native SDK can continue as a candidate. Validate desktop
attachment and production resource use before committing the complete product
UI to it. The TypeScript path is now working with the documented local patch.
