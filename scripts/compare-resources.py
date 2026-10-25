"""Isolated ordinary-window comparison. Requires psutil 7.2.2; includes WebView descendants."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import time
import uuid
import psutil
from app_client import Client, CopyData

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / '.tools/web-comparison'
# Avoid Windows DPI virtualization when applying equal physical window bounds.
user32 = c.WinDLL('user32')
user32.SetProcessDpiAwarenessContext.argtypes = [c.c_void_p]
user32.SetProcessDpiAwarenessContext(c.c_void_p(-4))


def state(directory, ready=False, count=1):
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        try:
            result = json.loads((directory / 'automation.json').read_text(encoding='utf8'))
            if not ready or result.get('readyCount') == count:
                return result
        except (OSError, ValueError):
            pass
        time.sleep(.01)
    raise TimeoutError(f'Application content not ready: {directory}')


def web_command(directory, args):
    snapshot = state(directory)
    payload = json.dumps(args).encode() + b'\0'
    buffer = c.create_string_buffer(payload)
    data = CopyData(0x44544F44, len(payload), c.cast(buffer, c.c_void_p))
    result = c.c_size_t()
    user = Client(directory).user
    if not user.SendMessageTimeoutW(snapshot['manager']['handle'], 0x4A, 0,
            c.addressof(data), 2, 10000, c.byref(result)) or not result.value:
        raise RuntimeError('Web IPC failed')


def group(process):
    return [process] + process.children(recursive=True)


def metrics(processes):
    data = []
    for process in processes:
        try:
            memory = process.memory_info()
            cpu = process.cpu_times()
            data.append({'pid': process.pid, 'name': process.name(), 'workingSet': memory.rss,
                         'privateBytes': memory.private, 'cpuSeconds': cpu.user + cpu.system})
        except psutil.NoSuchProcess:
            pass
    return data


def run(kind, notes, tasks, trial, idle, directory=None):
    phase = 'reused-profile' if directory else 'fresh-profile'
    directory = directory or BASE / f'measure-{kind}-{notes}-{trial}-{uuid.uuid4().hex[:8]}'
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'automation.json').unlink(missing_ok=True)
    executable = ROOT / 'dist/DesktopTodo.exe' if kind == 'native' else BASE / 'bin/DesktopTodo.WebComparison.exe'
    args = [str(executable)]
    if kind == 'native':
        args += ['--automation', '--startup', '--data-dir', str(directory)]
    else:
        args += [str(directory), str(BASE / 'frontend'), str(notes), str(tasks)]
    started = time.perf_counter()
    child = subprocess.Popen(args, creationflags=subprocess.CREATE_NO_WINDOW)
    process = psutil.Process(child.pid)
    try:
        snapshot = state(directory, ready=kind == 'web', count=notes)
        initial = time.perf_counter() - started
        if kind == 'native':
            client = Client(directory)
            snapshot = client.command('seed', notes, tasks)
            for note in snapshot['notes']:
                client.command('mode', note['id'], 2)
                client.command('lock', note['id'], 0)
            client.command('paint')
        prepared = time.perf_counter() - started
        # Equal physical outer bounds; all notes overlap at the same screen location.
        snapshot = state(directory)
        position = Client(directory).user.SetWindowPos
        position.argtypes = [c.c_void_p, c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_int, c.c_uint]
        for note in snapshot['notes']:
            if not position(note['window']['handle'], None, 80, 80, 680, 760, 0x14):
                raise RuntimeError('Cannot set equal comparison window bounds')
        time.sleep(3)
        processes = group(process)
        before = metrics(processes)
        clock = time.perf_counter()
        time.sleep(idle)
        elapsed = time.perf_counter() - clock
        after = metrics(group(process))
        before_cpu = {p['pid']: p['cpuSeconds'] for p in before}
        cpu = sum(p['cpuSeconds'] - before_cpu.get(p['pid'], 0) for p in after)
        commands = [p.cmdline() for p in group(process) if p.name() == 'msedgewebview2.exe']
        if kind == 'web' and not any(str(directory / 'webview-profile').lower() in ' '.join(cmd).lower() for cmd in commands):
            raise AssertionError('WebView profile isolation was not verified')
        latencies = []
        snapshot = state(directory)
        note = snapshot['notes'][0]
        if not position(note['window']['handle'], None, 0, 0, 0, 0, 0x13):
            raise RuntimeError('Cannot raise interaction target')
        for i in range(10):
            clock = time.perf_counter()
            if kind == 'web':
                web_command(directory, ['status', note['tasks'][0]['id'], i % 3])
            else:
                client.command('status', note['id'], note['tasks'][0]['id'], i % 3)
            latencies.append((time.perf_counter() - clock) * 1000)
        return {'kind': kind, 'notes': notes, 'tasksPerNote': tasks, 'trial': trial, 'phase': phase,
                'directory': str(directory),
                'launchToInitialSnapshotSeconds': round(initial, 4),
                'launchToPreparedWorkloadSeconds': round(prepared, 4),
                'idleSeconds': round(elapsed, 3), 'oneCoreCpuPercent': round(cpu / elapsed * 100, 3),
                'workingSetSumMiB': round(sum(p['workingSet'] for p in after) / 2**20, 2),
                'privateBytesSumMiB': round(sum(p['privateBytes'] for p in after) / 2**20, 2),
                'processCount': len(after), 'processes': after,
                'statusRoundTripMedianMs': round(statistics.median(latencies), 2)}
    finally:
        descendants = process.children(recursive=True) if process.is_running() else []
        try:
            Client(directory).command('quit')
            child.wait(timeout=10)
        except (Exception, subprocess.TimeoutExpired):
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
        for owned in descendants:
            try:
                owned.wait(timeout=3)
            except psutil.TimeoutExpired:
                owned.terminate()
            except psutil.NoSuchProcess:
                pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--trials', type=int, default=3)
    parser.add_argument('--idle', type=float, default=10)
    options = parser.parse_args()
    samples = []
    for notes, tasks in [(1, 10), (10, 100)]:
        for trial in range(options.trials):
            for kind in ['native', 'web']:
                sample = run(kind, notes, tasks, trial, options.idle)
                samples.append(sample)
                print(json.dumps({k: v for k, v in sample.items() if k != 'processes'}), flush=True)
                if trial == options.trials - 1:
                    warm = run(kind, notes, tasks, trial, options.idle, Path(sample['directory']))
                    samples.append(warm)
                    print(json.dumps({k: v for k, v in warm.items() if k != 'processes'}), flush=True)
    result = {'measuredAt': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
              'notes': 'Ordinary windows, unlocked, overlapping, 680x760 physical outer bounds. Fresh and reused isolated DB/profile. Startup paths differ: native seeds after launch; web seeds before view creation. WS sum double-counts shared pages. Private bytes sum does not. Status timing measures persisted command round trip only, not UI rendering completion. No UI/IPC polling during idle.',
              'nativeSha256': hashlib.sha256((ROOT / 'dist/DesktopTodo.exe').read_bytes()).hexdigest(),
              'samples': samples}
    (ROOT / 'docs/web-comparison-results.json').write_text(json.dumps(result) + '\n', encoding='utf8')
