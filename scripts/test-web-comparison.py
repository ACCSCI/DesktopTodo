"""Compiled WebView2/domain bridge contract, using throwaway data only."""
import importlib.util
import json
from pathlib import Path
import subprocess
import uuid
spec = importlib.util.spec_from_file_location('comparison', Path(__file__).with_name('compare-resources.py'))
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
base = comparison.BASE
directory = base / f'contract-{uuid.uuid4().hex}'
directory.mkdir(parents=True)
checks = []


def check(condition, name):
    if not condition:
        raise AssertionError(name)
    checks.append(name)


def command(*args):
    comparison.web_command(directory, args)
    return comparison.state(directory, ready=True, count=2)['notes'][0]


process = subprocess.Popen([str(base / 'bin/DesktopTodo.WebComparison.exe'), str(directory),
                            str(base / 'frontend'), '2', '3'], creationflags=subprocess.CREATE_NO_WINDOW)
try:
    snapshot = comparison.state(directory, ready=True, count=2)
    check(snapshot['pid'] == process.pid and len(snapshot['notes']) == 2, 'two real WebView notes reach rendered readiness')
    note = snapshot['notes'][0]
    check(len(note['tasks']) == 3, 'shared native domain seeds expected tasks')
    note = command('add', '中文 🎯')
    check(note['tasks'][-1]['text'] == '中文 🎯', 'UTF-8 native bridge persists Unicode')
    task = note['tasks'][0]['id']
    note = command('status', task, 2)
    check(note['tasks'][0]['status'] == 2, 'native status mutation renders')
    note = command('filter', 4)
    check(note['visible'] == [task], 'native filtering supplies visible IDs')
    note = command('filter', 0)
    order = [t['id'] for t in reversed(note['tasks'])]
    note = command('order', order)
    check([t['id'] for t in note['tasks']] == order, 'native reorder persists exact order')
    check(command('lock', True)['locked'], 'lock state crosses bridge')
    check(not command('lock', False)['locked'], 'unlock state crosses bridge')
    check(command('mode', 1)['mode'] == 1, 'topmost mode updates native model')
    check(command('mode', 2)['mode'] == 2, 'ordinary mode restores native model')
    try:
        comparison.web_command(directory, ['mode', 0])
        raise AssertionError('Unsupported desktop mode must fail')
    except RuntimeError:
        check(comparison.state(directory)['notes'][0]['mode'] == 2, 'unsupported desktop mode fails without changing model')
    check((directory / 'todo.sqlite').exists(), 'isolated SQLite database exists')
    comparison.Client(directory).command('quit')
    check(process.wait(timeout=10) == 0, 'host closes cleanly')
    result = {'passed': len(checks), 'checks': checks}
    (comparison.ROOT / 'docs/web-comparison-contract.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    print(f'{len(checks)} compiled comparison integration checks passed')
finally:
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=10)
