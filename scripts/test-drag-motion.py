"""Exercise real library pointer routing, animation frames and persisted reorder."""
import ctypes as c
import json
import os
from pathlib import Path
import subprocess
import time
from app_client import Client

root = Path(__file__).resolve().parent.parent
directory = root / '.tools/drag-motion' / str(time.time_ns())
directory.mkdir(parents=True)
exe = Path(os.environ.get('DESKTOP_TODO_TEST_EXE', root / 'dist/nim-ui/DesktopTodo-Nim.exe'))
user = c.WinDLL('user32')
user.SetProcessDpiAwarenessContext.argtypes = [c.c_void_p]
user.SetProcessDpiAwarenessContext(c.c_void_p(-4))
user.SendMessageW.argtypes = [c.c_void_p,c.c_uint,c.c_size_t,c.c_ssize_t]
process = subprocess.Popen([str(exe),'--automation','--startup','--data-dir',str(directory)], creationflags=subprocess.CREATE_NO_WINDOW)
client = Client(directory)
try:
    data = client.snapshot()
    data = client.command('seed',1,4)
    identity = data['notes'][0]['id']
    client.command('mode',identity,1)
    client.command('lock',identity,0)
    data = client.command('bounds',identity,80,80,680,760,192)
    handle = data['notes'][0]['window']['handle']
    original = data['notes'][0]['visible']

    def motion():
        client.command('motion-state',identity)
        return json.loads((directory / 'motion.json').read_text())

    initial = motion()
    first,last = initial['rows'][0]['rect'],initial['rows'][-1]['rect']
    if first[1] != 180:
        raise AssertionError('Native task list does not include the 12 DIP toolbar gap')
    grip = initial['rows'][0]['grip']
    x,y = (grip[0]+grip[2])//2,(grip[1]+grip[3])//2
    destination = last[3]-4
    user.SendMessageW(handle,0x201,1,(y << 16)|(x & 0xffff))
    pressed = motion()
    if not pressed.get('dragTask'):
        raise AssertionError(f'Grip routing did not capture task: {pressed}')
    # Use real window messages and actual control bounds, without moving the
    # user's physical cursor or writing preview order into SQLite.
    user.SendMessageW(handle,0x200,1,(destination << 16)|(x & 0xffff))
    moving = motion()
    if not moving['dragging']:
        raise AssertionError(f'Real grip did not start dragging: {moving}')
    if client.snapshot()['notes'][0]['visible'] != original:
        raise AssertionError('Preview changed persistent task order before drop')
    time.sleep(0.04)
    frame = motion()
    animated = [row for row in frame['rows'][1:] if row['playing'] and row['offset'] != row['target']]
    if not animated:
        raise AssertionError(f'No intermediate native animation frame: {frame}')
    user.SendMessageW(handle,0x202,0,(destination << 16)|(x & 0xffff))
    deadline = time.monotonic()+2
    while time.monotonic()<deadline:
        settled = motion()
        if all(row['offset']==0 and not row['playing'] for row in settled['rows']):break
        time.sleep(0.02)
    else:raise AssertionError(f'Animation failed to settle: {settled}')
    reordered = client.snapshot()['notes'][0]['visible']
    if reordered != original[1:]+original[:1]:
        raise AssertionError(f'Wrong order after animated drop: {reordered}')
    grip = settled['rows'][0]['grip']
    x,y = (grip[0]+grip[2])//2,(grip[1]+grip[3])//2
    user.SendMessageW(handle,0x201,1,(y << 16)|x)
    user.SendMessageW(handle,0x200,1,((y+120) << 16)|x)
    user.SendMessageW(handle,0x100,0x1b,0)
    if motion()['dragging'] or client.snapshot()['notes'][0]['visible'] != reordered:
        raise AssertionError('Escape must cancel drag without committing a reorder')
    print('6 native drag checks passed: spacing, preview, intermediate frames, reorder, idle settlement, Escape')
finally:
    if process.poll() is None:
        client.command('quit')
        process.wait(timeout=5)
