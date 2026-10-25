"""Compiled executable regression: wheel/paint must not resize a desktop card."""
import ctypes as c
import json
import os
from pathlib import Path
import subprocess
import time
from app_client import Client

root = Path(__file__).resolve().parent.parent
directory = root / '.tools' / f'wheel-regression-{time.time_ns()}'
directory.mkdir()
user = c.WinDLL('user32')
user.SetProcessDpiAwarenessContext.argtypes = [c.c_void_p]
user.SetProcessDpiAwarenessContext(c.c_void_p(-4))
user.SendMessageW.argtypes = [c.c_void_p,c.c_uint,c.c_size_t,c.c_ssize_t]
executable = Path(os.environ.get('DESKTOP_TODO_TEST_EXE',root/'dist/DesktopTodo.exe'))
process = subprocess.Popen([str(executable),'--automation','--startup','--data-dir',str(directory)],
                           creationflags=subprocess.CREATE_NO_WINDOW)
client = Client(directory)
checks = []
try:
    client.snapshot()
    data = client.command('seed',1,100)
    identity = data['notes'][0]['id']
    for mode in [0,1,2]:
        client.command('mode',identity,mode)
        client.command('lock',identity,0)
        data = client.command('bounds',identity,80,80,680,760,192)
        expected = [80,80,680,760]
        for i in range(8):
            handle = data['notes'][0]['window']['handle']
            delta = -120 if i % 2 == 0 else 120
            user.SendMessageW(handle,0x20A,(delta & 0xFFFF) << 16,0)
            data = client.command('paint')
            actual = data['notes'][0]['window']['rect']
            if actual != expected:
                raise AssertionError(f'Wheel/paint resized mode {mode}: {expected} -> {actual}')
        user.SendMessageW(handle,0x232,0,0)  # WM_EXITSIZEMOVE: real native move completion.
        data = client.command('paint')
        if data['notes'][0]['window']['rect'] != expected or data['error']:
            raise AssertionError(f'Native move completion failed in mode {mode}: {data["error"]}')
        checks.append(f'mode {mode}: eight wheel/paint cycles preserve physical bounds')
    result = {'passed':len(checks),'checks':checks}
    (root/'docs/wheel-regression-results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(f'{len(checks)} wheel resize regression checks passed')
finally:
    client.command('quit')
    process.wait(timeout=10)
