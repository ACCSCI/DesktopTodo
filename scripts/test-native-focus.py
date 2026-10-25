"""Compiled two-process focus contract; foreign userdata is not a local pointer."""
import ctypes as c
import os
from pathlib import Path
import subprocess
import time
from app_client import Client

root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('DESKTOP_TODO_TEST_EXE', root / 'dist/nim-ui/DesktopTodo-Nim.exe'))
user = c.WinDLL('user32')
user.SendMessageTimeoutW.argtypes = [c.c_void_p,c.c_uint,c.c_size_t,c.c_ssize_t,c.c_uint,c.c_uint,c.POINTER(c.c_size_t)]
instances = []
try:
    for index in range(2):
        directory = root / '.tools/focus-contract' / f'{time.time_ns()}-{index}'
        directory.mkdir(parents=True)
        process = subprocess.Popen([str(exe),'--automation','--startup','--data-dir',str(directory)], creationflags=subprocess.CREATE_NO_WINDOW)
        client = Client(directory)
        instances.append((process,client))
        data = client.snapshot()
        data = client.command('seed',1,1)
        client.command('mode',data['notes'][0]['id'],2)
    handles = [client.snapshot()['notes'][0]['window']['handle'] for _,client in instances]
    for index,(process,client) in enumerate(instances):
        for message in (0x7,0x8):  # WM_SETFOCUS / WM_KILLFOCUS.
            result = c.c_size_t()
            sent = user.SendMessageTimeoutW(handles[index],message,handles[1-index],0,2,5000,c.byref(result))
            if not sent or process.poll() is not None:
                raise AssertionError('Focus transfer to another process crashed or hung the application')
            if client.command('paint')['error']:
                raise AssertionError('Focus transfer left an application error')
    print('4 cross-process native focus checks passed')
finally:
    for process,client in instances:
        if process.poll() is None:
            try:
                client.command('quit')
                process.wait(timeout=5)
            except Exception:
                process.terminate()
                process.wait(timeout=5)
