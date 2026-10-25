"""Client for DesktopTodo's opt-in test IPC, not a general UI input driver."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import time
import sys
from urllib.parse import quote


class CopyData(c.Structure):
    _fields_ = [("tag", c.c_size_t), ("size", w.DWORD), ("data", c.c_void_p)]


class Client:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.user = c.WinDLL("user32", use_last_error=True)
        self.user.SendMessageTimeoutW.argtypes = [w.HWND, w.UINT, w.WPARAM,
            w.LPARAM, w.UINT, w.UINT, c.POINTER(c.c_size_t)]
        self.user.SendMessageTimeoutW.restype = w.LPARAM
        self.user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]

    def snapshot(self, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                data = json.loads((self.directory / "automation.json").read_text(encoding="utf8"))
                handle = data["manager"]["handle"]
                owner = w.DWORD()
                self.user.GetWindowThreadProcessId(w.HWND(handle), c.byref(owner))
                if handle and self.user.IsWindow(w.HWND(handle)) and owner.value == data["pid"]:
                    return data
            except (OSError, ValueError, KeyError, TypeError):
                pass
            time.sleep(0.02)
        raise TimeoutError("DesktopTodo did not publish a live automation snapshot")

    def command(self, verb, *args, expect_success=True):
        snapshot = self.snapshot()
        payload = "\t".join(quote(str(arg), safe="") for arg in (verb, *args)).encode("utf8") + b"\0"
        buffer = c.create_string_buffer(payload)
        data = CopyData(0x44544F44, len(payload), c.cast(buffer, c.c_void_p))
        result = c.c_size_t()
        sent = self.user.SendMessageTimeoutW(snapshot["manager"]["handle"], 0x004A, 0,
            c.addressof(data), 0x0002, 10000, c.byref(result))
        if not sent:
            raise RuntimeError(f"IPC timeout/error: {c.get_last_error()}")
        if bool(result.value) != expect_success:
            raise AssertionError(f"Unexpected IPC result for {verb}: {result.value}")
        if verb == "quit":
            return None
        return self.snapshot()


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf8")
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", required=True)
    parser.add_argument("verb", nargs="?", default="snapshot")
    parser.add_argument("args", nargs="*")
    options = parser.parse_args()
    result = Client(options.data_dir).command(options.verb, *options.args)
    if result is not None:
        print(json.dumps(result, ensure_ascii=False, indent=2))
