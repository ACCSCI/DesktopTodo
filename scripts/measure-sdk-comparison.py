"""One rough sample per workload; no benchmark repetitions or real user data."""
import ctypes as c
import json
from pathlib import Path
import re
import subprocess
import time
import uuid
import psutil
from app_client import Client

ROOT = Path(__file__).resolve().parent.parent
SDK = ROOT / 'sdk-comparison'
CLI = ROOT / '.tools/node_modules/@native-sdk/cli/bin/native.js'
EXE = SDK / 'zig-out/bin/sdk-comparison.exe'
user = c.WinDLL('user32')
user.SetProcessDpiAwarenessContext.argtypes = [c.c_void_p]
user.SetProcessDpiAwarenessContext(c.c_void_p(-4))
user.SetWindowPos.argtypes = [c.c_void_p, c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_int, c.c_uint]


def automate(*args):
    return subprocess.check_output(['node', str(CLI), 'automate', *args], cwd=SDK,
                                   creationflags=subprocess.CREATE_NO_WINDOW).decode('utf8')


def sdk_windows(pid):
    result = json.loads(subprocess.check_output(['python', str(ROOT / 'scripts/inspect-windows.py'),
                                                '--pid', str(pid)], creationflags=subprocess.CREATE_NO_WINDOW))
    return [window for window in result['windows'] if window['class'] == 'NativeSdkWindowsHost' and window['style'] & 0x10000000]


def sample(process, implementation, notes, tasks):
    time.sleep(2)
    parent = psutil.Process(process.pid)
    processes = [parent] + parent.children(recursive=True)
    before = sum(sum(p.cpu_times()[:2]) for p in processes)
    clock = time.perf_counter()
    time.sleep(10)
    elapsed = time.perf_counter() - clock
    after = sum(sum(p.cpu_times()[:2]) for p in processes)
    memories = [p.memory_info() for p in processes]
    return {'implementation': implementation, 'notes': notes, 'tasksPerNote': tasks,
            'processCount': len(processes), 'seconds': round(elapsed, 3),
            'oneCoreCpuPercent': round(100 * (after-before) / elapsed, 3),
            'workingSetSumMiB': round(sum(m.rss for m in memories) / 2**20, 2),
            'privateCommitSumMiB': round(sum(m.private for m in memories) / 2**20, 2)}


def set_bounds(handles):
    for handle in handles:
        if not user.SetWindowPos(handle, None, 80, 80, 680, 760, 0x14):
            raise RuntimeError('Failed to apply equal outer bounds')


samples = []
sdk = subprocess.Popen([str(EXE)], cwd=SDK, creationflags=subprocess.CREATE_NO_WINDOW)
try:
    automate('assert', f'publisher_pid={sdk.pid}', 'gpu_nonblank=true', '10 项')
    snapshot = automate('snapshot')
    # A real platform/UI contract check: dispatch a checkbox, not a synthetic model edit.
    checkbox = re.search(r'widget @w\d+/main-canvas#(\d+) role=checkbox', snapshot)
    if not checkbox:
        raise AssertionError('No rendered checkbox')
    automate('widget-click', 'main-canvas', checkbox.group(1))
    automate('assert', '已完成')
    for count, tasks in [(1, 10), (5, 100)]:
        if count == 5:
            snapshot = automate('snapshot')
            button = re.search(r'widget @w\d+/main-canvas#(\d+) role=button name="5×100"', snapshot)
            if not button:
                raise AssertionError('No five-note workload control')
            automate('widget-click', 'main-canvas', button.group(1))
            automate('assert', '100 项', 'canvas-5', 'gpu_nonblank=true')
        windows = sdk_windows(sdk.pid)
        if len(windows) != count:
            raise AssertionError(f'Expected {count} real SDK windows, got {len(windows)}')
        set_bounds([window['handle'] for window in windows])
        measured = sample(sdk, 'Native SDK 0.11.0 / Geist', count, tasks)
        samples.append(measured)
        print(json.dumps(measured), flush=True)
    automate('screenshot', 'main-canvas')
finally:
    if sdk.poll() is None:
        sdk.terminate()
        sdk.wait(timeout=10)

for count, tasks in [(1, 10), (5, 100)]:
    directory = ROOT / '.tools' / f'sdk-native-baseline-{uuid.uuid4().hex}'
    directory.mkdir()
    process = subprocess.Popen([str(ROOT / 'dist/DesktopTodo.exe'), '--automation', '--startup',
                                '--data-dir', str(directory)], creationflags=subprocess.CREATE_NO_WINDOW)
    client = Client(directory)
    try:
        client.snapshot()
        state = client.command('seed', count, tasks)
        for note in state['notes']:
            client.command('mode', note['id'], 2)
            client.command('lock', note['id'], 0)
        set_bounds([note['window']['handle'] for note in state['notes']])
        client.command('paint')
        measured = sample(process, 'C++20 / Win32', count, tasks)
        samples.append(measured)
        print(json.dumps(measured), flush=True)
    finally:
        client.command('quit')
        process.wait(timeout=10)

result = {'measuredAt': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
          'samples': samples, 'sdkExecutableBytes': EXE.stat().st_size,
          'method': 'One sample per workload, no repeats. Ordinary overlapping unlocked windows, 680x760 physical outer bounds. 2s settle + 10s idle, complete process tree. CPU is relative to one core. Private commit is not resident RAM. SDK Todo is memory-only; C++ baseline persists SQLite. SDK 0.11.0 supports four auxiliary UiApp windows, so the real multiwindow sample is five notes, not ten. Automation stays enabled.'}
(ROOT / 'docs/sdk-comparison-results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')
