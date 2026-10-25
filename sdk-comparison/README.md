# Native SDK Todo resource sample

Independent Native SDK **0.11.0** experiment. It does not replace the C++ product.
Zig model + SDK typed native UI builder + built-in Geist theme. No WebView.

Implemented for this sample: independent notes, Unicode task input/add, completion,
unfinished filtering, and editing lock. The model is memory-only; no SQLite, backups,
startup registration, desktop embedding or OS mouse click-through. No drag sorting.
It is not a completed product and its resource figures are not feature-parity results.

The pinned SDK's `canvas_limits.max_ui_app_windows` is **4 secondary windows**.
It is a fixed constant, also asserted by its policy bridge. Therefore this sample
uses a main note plus four secondary notes: **five real OS windows**, not ten.
The SDK source was not modified to raise that budget.

```powershell
./scripts/bootstrap.ps1
npm run test:sdk
npm run build:sdk
./sdk-comparison/zig-out/bin/sdk-comparison.exe
```

The two load buttons select 1 note × 10 tasks or 5 notes × 100 tasks.
Use the existing `scripts/bootstrap-app.ps1` and `npm run build:app` for the C++ baseline.

```powershell
python -m pip install -r web-comparison/requirements.txt
python scripts/measure-sdk-comparison.py
```

Measurement deliberately takes **one sample per implementation and workload**,
with a two-second settle and ten-second idle observation. It checks actual SDK
checkbox dispatch and nonblank rendering, counts visible native HWNDs, applies
equal overlapping 680×760 physical outer bounds, then measures the full process tree.
The C++ baseline uses isolated databases and ordinary unlocked windows.
No automation polling occurs during the idle observation.

Private commit is not resident RAM; summed working sets may double-count shared pages.
CPU percentages refer to one core. Automation remains enabled. SQLite persistence
exists only in the C++ baseline, so this is a rough resource check, not a strict ranking.
Build/test time is not startup time. No repeated trials or startup benchmark is performed.

Results: [JSON](../docs/sdk-comparison-results.json). SDK screenshots, if requested,
are saved to ignored `.zig-cache/native-sdk-automation/` under this experiment.
All existing application data remains separate.
