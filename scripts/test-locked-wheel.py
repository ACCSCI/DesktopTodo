"""Compiled read-only wheel routing and native footer geometry contracts."""
import ctypes as c
import json
import os
from pathlib import Path
import subprocess
import time
from app_client import Client

root = Path(__file__).resolve().parent.parent
directory = root / '.tools/locked-wheel' / str(time.time_ns())
directory.mkdir(parents=True)
exe = Path(os.environ.get('DESKTOP_TODO_TEST_EXE',root/'dist/nim-ui/DesktopTodo-Nim.exe'))
process = subprocess.Popen([str(exe),'--automation','--startup','--data-dir',str(directory)],creationflags=subprocess.CREATE_NO_WINDOW)
client = Client(directory)
try:
    data = client.snapshot()
    data = client.command('seed',1,80)
    identity = data['notes'][0]['id']
    client.command('mode',identity,0)
    client.command('lock',identity,1)
    data = client.command('bounds',identity,80,80,680,760,192)
    original = data['notes'][0]['tasks']
    original_bounds = data['notes'][0]['window']['rect']
    def state():
        client.command('motion-state',identity)
        return json.loads((directory/'motion.json').read_text())
    before = state()
    client.command('readonly-wheel',identity,-120,'desktop')
    deadline = time.monotonic()+2
    while time.monotonic()<deadline:
        after = state()
        if after['scroll']>before['scroll']:break
        time.sleep(0.02)
    else:raise AssertionError('Locked native list did not scroll')
    client.command('readonly-wheel',identity,-120,'covered',expect_success=False)
    if state()['scroll']!=after['scroll']:raise AssertionError('Covered notes stole a wheel event')
    data = client.snapshot()
    if not data['notes'][0]['locked'] or data['notes'][0]['editorFocused'] or data['notes'][0]['tasks']!=original:
        raise AssertionError('Read-only scrolling changed lock, focus or tasks')
    if data['notes'][0]['window']['rect']!=original_bounds:raise AssertionError('Wheel resized the note')
    for width,height in [(680,760),(560,640)]:
        client.command('bounds',identity,80,80,width,height,192)
        first,second = state()['footer']
        if first[1]!=second[1] or first[3]!=second[3]:
            raise AssertionError(f'Footer controls do not share top/bottom: {first}, {second}')
        top_margin=first[1]-(height-88)
        bottom_margin=height-first[3]
        if top_margin!=bottom_margin or bottom_margin!=12:
            raise AssertionError(f'Footer vertical padding differs: {top_margin}, {bottom_margin}')
    print('6 locked-wheel/footer checks passed: routing, occlusion, read-only state, stable bounds, two footer sizes')
finally:
    if process.poll() is None:
        client.command('quit')
        process.wait(timeout=5)
