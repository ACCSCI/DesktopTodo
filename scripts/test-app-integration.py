"""Integration against the shipping executable, isolated data and registry keys."""
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
from app_client import Client

sys.stdout.reconfigure(encoding="utf8")
ROOT = Path(__file__).resolve().parent.parent
EXE = Path(os.environ.get("DESKTOP_TODO_TEST_EXE", ROOT / "dist/DesktopTodo.exe"))
DIRECTORY = ROOT / ".tools/app-integration" / ("中文数据-" + str(time.time_ns()))
DIRECTORY.mkdir(parents=True)
checked = []


def check(condition, name):
    if not condition:
        raise AssertionError(name)
    checked.append(name)


def start():
    process = subprocess.Popen([str(EXE), "--automation", "--startup", "--data-dir", str(DIRECTORY)],
        creationflags=subprocess.CREATE_NO_WINDOW, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    client = Client(DIRECTORY)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        snapshot = client.snapshot()
        if snapshot["pid"] == process.pid:
            return process, client
        time.sleep(0.02)
    raise TimeoutError("New instance did not publish its snapshot")


def note(snapshot, identity):
    return next(n for n in snapshot["notes"] if n["id"] == identity)


def durable(snapshot):
    return [{k: v for k, v in n.items() if k not in ("window", "unlock", "editorFocused")} for n in snapshot["notes"]]


process = None
try:
    process, client = start()
    data = client.snapshot()
    first = data["notes"][0]["id"]
    check(data["error"] == "", "clean native startup")
    check(note(data, first)["title"] == "我的待办", "Chinese default title")
    check(note(data, first)["window"]["parentClass"] in ("Progman", "WorkerW", "SHELLDLL_DefView"), "real Explorer desktop attachment")
    check(note(data, first)["window"]["visible"], "startup flag does not hide desktop cards")
    data = client.command("focus-editor", first)
    check(note(data, first)["editorFocused"], "desktop input editor accepts focus")
    client.command("paint")
    data = client.command("rename-note", first, "工作 · 原生待办 🎯")
    check(note(data, first)["title"] == "工作 · 原生待办 🎯", "rename Unicode title")
    data = client.command("note-color", first, "E5F3FF")
    check(note(data, first)["color"] == 0xE5F3FF, "per-note background color")
    ids = []
    for text in ("需求整理", "正在处理", "完成项目", "购物 🛒"):
        data = client.command("add-task", first, text)
        ids.append(note(data, first)["tasks"][-1]["id"])
    a, b, complete, d = ids
    data = client.command("edit-task", first, a, "编辑后的中文任务")
    check(note(data, first)["tasks"][0]["text"] == "编辑后的中文任务", "edit task text")
    data = client.command("icon", first, a, "🌟")
    data = client.command("task-color", first, a, "1155CC")
    check(note(data, first)["tasks"][0]["icon"] == "🌟", "per-task icon")
    check(note(data, first)["tasks"][0]["color"] == 0x1155CC, "per-task text/icon color")
    client.command("status", first, b, 1)
    client.command("status", first, complete, 2)
    expected = {0: ids, 1: [a,b,d], 2: [a,d], 3: [b], 4: [complete]}
    for filter_id, visible in expected.items():
        data = client.command("filter", first, filter_id)
        check(note(data, first)["visible"] == visible, f"filter {filter_id}")
    client.command("filter", first, 1)
    data = client.command("reorder", first, d, a)
    check([t["id"] for t in note(data, first)["tasks"]] == [d,a,complete,b], "filtered drag preserves hidden slots")
    data = client.command("new-note", "生活安排")
    second = data["notes"][-1]["id"]
    check(second != first and len(data["notes"]) == 2, "multiple independent notes")
    data = client.command("mode", second, 1)
    check(note(data, second)["window"]["parent"] == 0 and note(data, second)["window"]["extended"] & 8, "native topmost mode")
    data = client.command("mode", second, 2)
    check(note(data, second)["window"]["extended"] & 0x40000, "ordinary window taskbar presence")
    check(not note(data, second)["window"]["extended"] & 8, "ordinary window is not topmost")
    data = client.command("move", first, second, a, 0)
    moved = note(data, second)["tasks"][0]
    check(moved["id"] == a and moved["icon"] == "🌟", "cross-note drag retains identity and styling")
    data = client.command("lock", first, 1)
    locked = note(data, first)
    check(locked["window"]["extended"] & 0x20, "locked desktop body has native input transparency")
    check(locked["unlock"]["visible"] and locked["unlock"]["parent"] == locked["window"]["parent"], "unlock stays in desktop layer")
    check(not locked["unlock"]["extended"] & 0x20, "unlock control remains interactive")
    body_rect, control_rect = locked["window"]["rect"], locked["unlock"]["rect"]
    header_center = body_rect[1] + 22 * locked["window"]["dpi"] / 96
    check(abs(control_rect[1] + control_rect[3] / 2 - header_center) <= 1, "native lock control aligns with the header center at actual monitor DPI")
    button_before = locked["unlock"]
    data = client.command("toggle-lock", first)
    check(not note(data, first)["locked"], "lock button event unlocks the note")
    check(note(data, first)["unlock"]["handle"] == button_before["handle"] and note(data, first)["unlock"]["rect"] == button_before["rect"], "unlock event preserves the same button and position")
    data = client.command("toggle-lock", first)
    check(note(data, first)["locked"] and note(data, first)["unlock"]["rect"] == button_before["rect"], "same button event locks again without moving")
    data = client.command("paint")
    check(not data["error"], "locked desktop body and unlock surfaces present successfully")
    for corner in range(4):
        data = client.command("corner", first, corner, 8)
        n = note(data, first)
        x, y, width, height = n["window"]["rect"]
        ux, uy, uw, uh = n["unlock"]["rect"]
        check(x <= ux < ux + uw <= x + width and y <= uy < uy + uh <= y + height, f"unlock corner {corner} within card")
    locked_button_rect = note(data, first)["unlock"]["rect"]
    data = client.command("lock", first, 0)
    check(not note(data, first)["window"]["extended"] & 0x20, "unlock restores normal input")
    check(note(data, first)["unlock"]["visible"], "same lock control remains visible when body is interactive")
    check(note(data, first)["unlock"]["rect"] == locked_button_rect, "lock and unlock control keep identical screen coordinates")
    for repeat in range(3):
        client.command("lock", first, 1)
        client.command("paint")
        client.command("lock", first, 0)
        data = client.command("paint")
    check(not data["error"], "repeated lock and unlock retains valid native painting")
    client.command("lock", first, 1)
    before_recreate = durable(client.snapshot())
    data = client.command("recreate-windows")
    check(durable(data) == before_recreate, "window loss recovery preserves all note and task data")
    check(note(data, first)["window"]["parentClass"] in ("Progman", "WorkerW", "SHELLDLL_DefView"), "recreated desktop window reattaches to Explorer")
    check(note(data, first)["unlock"]["visible"], "window loss recovery recreates independent unlock control")
    check(note(data, second)["window"]["extended"] & 0x40000, "window loss recovery retains ordinary mode")
    client.command("rename-note", second, "要撤销的标题")
    data = client.command("undo")
    check(note(data, second)["title"] == "生活安排", "undo persists prior state")
    data = client.command("redo")
    check(note(data, second)["title"] == "要撤销的标题", "redo persists restored change")
    old = durable(data)
    data = client.command("note-color", first, "100000000", expect_success=False)
    check(durable(data) == old and bool(data["error"]), "invalid mutations cannot corrupt live state")
    data = client.command("rename-note", second, "生活安排")
    check(not data["error"], "successful edit clears prior error")
    data = client.command("startup", 1)
    check(data["startup"], "startup registration enabled through isolated Windows adapter")
    data = client.command("startup", 0)
    check(not data["startup"], "startup registration disabled")
    backup = DIRECTORY / "备份.sqlite"
    client.command("backup", backup)
    check(backup.exists(), "native SQLite export")
    before_backup = durable(client.snapshot())
    client.command("delete-note", second)
    data = client.command("restore", backup)
    check(durable(data) == before_backup, "native backup restore")
    with sqlite3.connect(backup) as damaged:
        damaged.execute("UPDATE notes SET color=4294967296")
    data = client.command("restore", backup, expect_success=False)
    check(durable(data) == before_backup and bool(data["error"]), "invalid backup is rejected without changing live notes")
    client.command("lock-on-start", 0)
    before_restart = durable(client.snapshot())
    client.command("quit")
    check(process.wait(timeout=5) == 0, "clean shutdown")
    process, client = start()
    data = client.snapshot()
    check(durable(data) == before_restart, "all user data restored after process restart")
    connection = sqlite3.connect(DIRECTORY / "todo.sqlite")
    check(connection.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "on-disk SQLite integrity")
    check(connection.execute("SELECT count(*) FROM tasks").fetchone()[0] == 4, "on-disk tasks survive restart")
    connection.close()
    duplicate = subprocess.Popen([str(EXE), "--automation", "--startup", "--data-dir", str(DIRECTORY)],
        creationflags=subprocess.CREATE_NO_WINDOW, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    check(duplicate.wait(timeout=5) == 0 and client.snapshot()["pid"] == process.pid, "single instance per data directory")
    result = {"passed": len(checked), "checks": checked, "executableBytes": EXE.stat().st_size,
              "dataDirectory": str(DIRECTORY)}
    (ROOT / "docs/app-integration-results.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print(f"{len(checked)} executable integration checks passed")
finally:
    if process and process.poll() is None:
        try:
            Client(DIRECTORY).command("quit")
            process.wait(timeout=5)
        except Exception:
            process.terminate()
