"""Portable storage, relocation, input and destructive-operation integration."""
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import time
from app_client import Client

ROOT = Path(__file__).resolve().parent.parent
EXE = Path(__import__('os').environ.get('DESKTOP_TODO_TEST_EXE', ROOT / 'dist/DesktopTodo.exe'))
WORK = ROOT / '.tools/storage-management' / str(time.time_ns())
ORIGINAL = WORK / 'original'
ORIGINAL.mkdir(parents=True)
checks = []


def check(condition, name):
    if not condition:
        raise AssertionError(name)
    checks.append(name)


def notes(data):
    return [{k: v for k, v in note.items() if k not in ('window', 'unlock', 'editorFocused')}
            for note in data['notes']]


def launch(exe, directory=None):
    args = [str(exe), '--automation', '--startup']
    if directory:
        args += ['--data-dir', str(directory)]
    process = subprocess.Popen(args, cwd=ROOT, creationflags=subprocess.CREATE_NO_WINDOW)
    client = Client(directory or exe.parent)
    try:
        client.snapshot()
    except Exception:
        process.terminate()
        process.wait(timeout=5)
        raise
    return process, client


def close(process, client):
    if process.poll() is None:
        client.command('quit')
        process.wait(timeout=5)


process, client = launch(EXE, ORIGINAL)
try:
    data = client.command('focus-editor', 1)
    check(data['notes'][0]['editorFocused'], 'desktop input acquires native keyboard focus')
    client.command('editor-text', 1, '点击输入框后的中文待办 🎯')
    data = client.command('commit-editor', 1)
    check(data['notes'][0]['tasks'][0]['text'] == '点击输入框后的中文待办 🎯', 'native input commits Unicode content through the real editor handler')
    client.command('paint')
    expected = notes(client.snapshot())
    backup = ORIGINAL / 'backups/manual.sqlite'
    backup.parent.mkdir(exist_ok=True)
    client.command('backup', backup)
    client.command('compact')
    check(notes(client.snapshot()) == expected, 'database compaction preserves all user content')
    for stage in range(3):
        confirmations = ['confirm'] * 3
        confirmations[stage] = 'cancel'
        data = client.command('clear-data', *confirmations, expect_success=False)
        check(notes(data) == expected, f'cancelling clear at confirmation {stage + 1} leaves all data untouched')
    target = WORK / 'selected-storage'
    data = client.command('relocate', target)
    check(Path(data['dataDirectory']) == target and notes(data) == expected, 'relocation switches the live database without losing notes')
    check((target / 'backups/manual.sqlite').exists(), 'relocation preserves existing manual backups')
    check(not (ORIGINAL / 'todo.sqlite').exists(), 'obsolete active database is removed only after successful migration')
    check((ORIGINAL / 'DesktopTodo.ini').exists(), 'storage preference persists beside the original portable root')
    occupied = WORK / 'occupied'
    occupied.mkdir()
    (occupied / 'todo.sqlite').write_text('existing unrelated data')
    client.command('relocate', occupied, expect_success=False)
    check((occupied / 'todo.sqlite').read_text() == 'existing unrelated data', 'existing destination databases are never overwritten')
    # Deny replacement of the INI to exercise rollback after the target snapshot is built.
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    handle = kernel.CreateFileW(str(ORIGINAL / 'DesktopTodo.ini'), 0x80000000, 0, None, 3, 0, None)
    check(handle != c.c_void_p(-1).value, 'storage preference can be locked for failure injection')
    try:
        rejected = WORK / 'rejected'
        data = client.command('relocate', rejected, expect_success=False)
        check(Path(data['dataDirectory']) == target and notes(data) == expected and not (rejected / 'todo.sqlite').exists(),
              'settings write failure rolls back the new active database and retains the old data')
    finally:
        kernel.CloseHandle(handle)
    before = sorted(p.name for p in (target / 'backups').iterdir())
    data = client.command('clear-data', 'confirm', 'confirm', 'confirm')
    check(not data['notes'] and not data['canUndo'], 'three confirmations clear the database and discard undo history')
    check(sorted(p.name for p in (target / 'backups').iterdir()) == before, 'clear creates no backup and preserves existing backup files')
    with sqlite3.connect(target / 'todo.sqlite') as connection:
        check(connection.execute('SELECT count(*) FROM tasks').fetchone()[0] == 0, 'cleared tasks are absent from the physical active database')
    data = client.command('restore', target / 'backups/manual.sqlite')
    check(notes(data) == expected, 'manual backups can restore cleared data when explicitly requested')
    portable = WORK / 'portable-copy'
    portable.mkdir()
    copied = portable / 'DesktopTodo.exe'
    shutil.copy2(EXE, copied)
    if (EXE.parent / 'resources').is_dir():
        shutil.copytree(EXE.parent / 'resources', portable / 'resources')
    with sqlite3.connect(target / 'todo.sqlite') as source, sqlite3.connect(portable / 'todo.sqlite') as destination:
        source.backup(destination)
    source.close()
    destination.close()
    close(process, client)
    process, client = launch(copied)
    check(Path(client.snapshot()['dataDirectory']) == portable, 'default database lives directly beside the executable despite a different working directory')
    selected = portable / 'data-selected'
    client.command('relocate', selected)
    check('directory=data-selected' in (portable / 'DesktopTodo.ini').read_text(), 'in-folder storage locations persist as portable relative paths')
    close(process, client)
    process, client = launch(copied)
    check(Path(client.snapshot()['dataDirectory']) == selected and client.snapshot()['notes'][0]['tasks'], 'restart follows the saved location and retains migrated tasks')
    parent_expected = notes(client.snapshot())
    close(process, client)
    with sqlite3.connect(selected / 'todo.sqlite') as source, sqlite3.connect(WORK / 'todo.sqlite') as destination:
        source.backup(destination)
    (portable / 'DesktopTodo.ini').write_text('[Storage]\ndirectory=..\n', encoding='utf8')
    process, client = launch(copied)
    check(Path(client.snapshot()['dataDirectory']) == WORK and notes(client.snapshot()) == parent_expected,
          'a UI executable in a child folder reconnects to existing parent data without replacing either database')
    result = {'passed': len(checks), 'checks': checks, 'directory': str(WORK)}
    (ROOT / 'docs/storage-management-results.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    print(f'{len(checks)} storage-management integration checks passed')
finally:
    close(process, client)
