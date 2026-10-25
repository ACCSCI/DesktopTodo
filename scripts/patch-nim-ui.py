"""Pinned upstream fix: never dereference another process's window userdata."""
from pathlib import Path

path = Path(__file__).resolve().parent.parent / '.tools/nim-duilib/source/duilib/Core/NativeWindow_Windows.cpp'
source = path.read_text(encoding='utf-8')
original = '        if ((hWnd != nullptr) && ::IsWindow(hWnd)) {'
replacement = '''        DWORD focusProcess = 0;
        ::GetWindowThreadProcessId(hWnd, &focusProcess);
        if ((hWnd != nullptr) && ::IsWindow(hWnd) && focusProcess == ::GetCurrentProcessId()) {'''
if source.count(original) == 2:
    path.write_text(source.replace(original, replacement), encoding='utf-8')
elif source.count(replacement) != 2:
    raise RuntimeError('Unexpected upstream focus implementation; review the patch before building.')
