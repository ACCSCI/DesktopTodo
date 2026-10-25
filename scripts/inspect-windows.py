"""Read-only Win32 facts for this project's integration probes; no UI input."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json


def inspect(pid):
    user = c.WinDLL("user32", use_last_error=True)
    user.SetProcessDpiAwarenessContext.argtypes = [w.HANDLE]
    user.SetProcessDpiAwarenessContext(w.HANDLE(-4))
    callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    user.EnumWindows.argtypes = [callback_type, w.LPARAM]
    user.EnumChildWindows.argtypes = [w.HWND, callback_type, w.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
    user.GetWindowLongPtrW.argtypes = [w.HWND, c.c_int]
    user.GetWindowLongPtrW.restype = c.c_ssize_t
    user.GetParent.argtypes = [w.HWND]
    user.GetParent.restype = w.HWND
    user.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
    user.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
    user.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
    user.WindowFromPoint.argtypes = [w.POINT]
    user.WindowFromPoint.restype = w.HWND
    user.GetAncestor.argtypes = [w.HWND, w.UINT]
    user.GetAncestor.restype = w.HWND

    def owner(handle):
        result = w.DWORD()
        user.GetWindowThreadProcessId(handle, c.byref(result))
        return result.value

    def text(handle, function):
        buffer = c.create_unicode_buffer(512)
        function(handle, buffer, len(buffer))
        return buffer.value

    def fact(handle):
        rect = w.RECT()
        user.GetWindowRect(handle, c.byref(rect))
        parent = user.GetParent(handle)
        x = (rect.left + rect.right) // 2
        y = (rect.top + rect.bottom) // 2
        hit = user.WindowFromPoint(w.POINT(x, y))
        return {
            "handle": handle, "title": text(handle, user.GetWindowTextW),
            "class": text(handle, user.GetClassNameW),
            "parent": parent or 0,
            "parentClass": text(parent, user.GetClassNameW) if parent else None,
            "style": user.GetWindowLongPtrW(handle, -16) & 0xFFFFFFFF,
            "extended": user.GetWindowLongPtrW(handle, -20) & 0xFFFFFFFF,
            "rect": [rect.left, rect.top, rect.right, rect.bottom],
            "centerHit": {"handle": hit or 0, "pid": owner(hit),
                          "class": text(hit, user.GetClassNameW),
                          "parent": user.GetParent(hit) or 0,
                          "root": user.GetAncestor(hit, 2) or 0},
        }

    found = {}

    @callback_type
    def child_callback(handle, _):
        if owner(handle) == pid:
            found[handle] = fact(handle)
        return True

    @callback_type
    def top_callback(handle, _):
        child_callback(handle, 0)
        user.EnumChildWindows(handle, child_callback, 0)
        return True

    user.EnumWindows(top_callback, 0)
    return {"pid": pid, "windows": list(found.values())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", required=True, type=int)
    arguments = parser.parse_args()
    print(json.dumps(inspect(arguments.pid), indent=2))
