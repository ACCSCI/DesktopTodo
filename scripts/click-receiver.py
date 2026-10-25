"""Separate-process native input receiver for click-through integration tests."""
import argparse
import ctypes
import json
from pathlib import Path
import tkinter as tk

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--rect", type=int, nargs=4, required=True)
args = parser.parse_args()
ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
root = tk.Tk()
root.title("Todo Click Receiver")
x, y, width, height = args.rect
root.geometry(f"{width}x{height}{x:+d}{y:+d}")
state = {"clicks": 0}


def save():
    args.output.write_text(json.dumps(state), encoding="utf8")


def receive():
    state["clicks"] += 1
    button.configure(text=f"Received clicks: {state['clicks']}")
    save()


button = tk.Button(root, text="Received clicks: 0", command=receive,
                   background="#cbdde8", activebackground="#b5cde0")
button.place(relx=0, rely=0, relwidth=1, relheight=1)
save()
root.mainloop()
