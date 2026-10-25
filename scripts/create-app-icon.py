"""Generate the project's code-drawn, antialiased Windows icon; no dependencies."""
from pathlib import Path
import math
import struct


def rounded(x, y, left, top, right, bottom, radius):
    if not (left <= x <= right and top <= y <= bottom):
        return False
    cx = min(max(x, left + radius), right - radius)
    cy = min(max(y, top + radius), bottom - radius)
    return (x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2


def segment(x, y, a, b, width):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = min(1, max(0, ((x - a[0]) * dx + (y - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - a[0] - t * dx, y - a[1] - t * dy) < width


def pixel(x, y):
    if not rounded(x, y, 3, 3, 97, 97, 22):
        return (0, 0, 0, 0)
    color = (105, 89, 205, 255)
    if rounded(x, y, 24, 17, 77, 83, 8):
        color = (255, 255, 255, 255)
    if segment(x, y, (36, 47), (46, 58), 3.5) or segment(x, y, (46, 58), (66, 36), 3.5):
        color = (105, 89, 205, 255)
    if rounded(x, y, 35, 69, 65, 73, 2):
        color = (211, 204, 242, 255)
    return color


def image(size):
    data = bytearray()
    for y in range(size - 1, -1, -1):
        for x in range(size):
            colors = [pixel((x + (sx + .5) / 3) * 100 / size,
                            (y + (sy + .5) / 3) * 100 / size) for sy in range(3) for sx in range(3)]
            alpha = sum(c[3] for c in colors)
            channels = [round(sum(c[i] * c[3] for c in colors) / alpha) if alpha else 0 for i in range(3)]
            data.extend((*reversed(channels), round(alpha / 9)))
    header = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0, len(data), 0, 0, 0, 0)
    return header + data + bytes(((size + 31) // 32) * 4 * size)


sizes = (16, 32, 48, 128)
payloads = [image(size) for size in sizes]
directory = bytearray(struct.pack('<HHH', 0, 1, len(sizes)))
offset = 6 + len(sizes) * 16
for size, payload in zip(sizes, payloads):
    directory.extend(struct.pack('<BBBBHHII', size, size, 0, 0, 1, 32, len(payload), offset))
    offset += len(payload)
destination = Path(__file__).resolve().parent.parent / 'app/resources/app.ico'
destination.write_bytes(directory + b''.join(payloads))
print(destination)
