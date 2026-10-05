#!/usr/bin/env python3
"""Takes a picture of one of every kind of thing a level shows.

    tools/tour.py <build folder> <level> <folder for the pictures>
    tools/tour.py build maps/lq_e1m2.bsp /tmp/tour

It runs the game twice without a window, with QUAKE_TOUR set: once briefly,
to read from the log which thing is looked at in which step, and once to
the end, saving those frames. The frames are then put together into sheets
of twelve, each picture numbered, with a list of what each number is, so
that a person, or a model that reads pictures, can check them by eye.

Nothing but Python's own library is needed.
"""
import os
import re
import struct
import subprocess
import sys
import zlib

WIDTH, HEIGHT = 480, 300
COLUMNS, ROWS = 4, 3


def run(build, level, frames, extra, output):
    game = os.path.join(build, "quake")
    with open(os.path.join(game, "extensions/quake/assets/level.txt"), "w") as chosen:
        chosen.write(level + "\n")
    try:
        command = [
            os.path.join(game, "neon-quake"), "--headless-renderer", "--window-size", f"{WIDTH}x{HEIGHT}",
            "--frames", str(frames), "--time-step", "0.016667", "--output-dir", output,
        ] + extra
        done = subprocess.run(
            command, cwd=game, env=dict(os.environ, QUAKE_TOUR="1"), capture_output=True, text=True, timeout=600)
        return done.stdout + done.stderr
    finally:
        os.remove(os.path.join(game, "extensions/quake/assets/level.txt"))


def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    at, chunks, header = 8, b"", None
    while at < len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            chunks += body
        at += 12 + length
    width, height, depth, colour = header[0], header[1], header[2], header[3]
    assert depth == 8 and colour in (2, 6), "only 8 bits of RGB or RGBA are read"
    size = 4 if colour == 6 else 3
    raw = zlib.decompress(chunks)
    stride = width * size
    rows, before = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if kind == 1:
            for i in range(size, stride):
                line[i] = (line[i] + line[i - size]) & 255
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + before[i]) & 255
        elif kind == 3:
            for i in range(stride):
                left = line[i - size] if i >= size else 0
                line[i] = (line[i] + ((left + before[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(stride):
                a = line[i - size] if i >= size else 0
                b = before[i]
                c = before[i - size] if i >= size else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line[i] for j in range(width) for i in range(j * size, j * size + 3)))
        before = line
    return width, height, rows


def write_png(path, width, height, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    raw = b"".join(b"\x00" + row for row in rows)
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                  chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


# the digits of a number, five dots wide and seven high, to mark a picture
DIGITS = {
    "0": "01110100011001110101110011000101110", "1": "00100011000010000100001000010001110",
    "2": "01110100010000100010001000100011111", "3": "11110000010000101110000010000111110",
    "4": "00010001100101010010111110001000010", "5": "11111100001111000001000011000101110",
    "6": "00110010001000011110100011000101110", "7": "11111000010001000100010000100001000",
    "8": "01110100011000101110100011000101110", "9": "01110100011000101111000010001001100",
}


def mark(rows, number, left, top):
    rows = [bytearray(row) for row in rows]
    scale = 3
    for place, digit in enumerate(str(number)):
        dots = DIGITS[digit]
        for y in range(7):
            for x in range(5):
                lit = dots[y * 5 + x] == "1"
                for dy in range(scale):
                    for dx in range(scale):
                        at = (left + (place * 6 + x) * scale + dx) * 3
                        rows[top + y * scale + dy][at:at + 3] = b"\xff\xff\x00" if lit else b"\x00\x00\x00"
    return [bytes(row) for row in rows]


def main():
    build, level, output = os.path.abspath(sys.argv[1]), sys.argv[2], os.path.abspath(sys.argv[3])
    os.makedirs(output, exist_ok=True)

    log = run(build, level, 40, [], output)
    stops = [(int(found.group(1)), int(found.group(2)), found.group(3))
             for found in re.finditer(r"TOUR (\d+) step (\d+) (.*)", log)]
    end = re.search(r"TOUR ends at step (\d+)", log)
    if not stops or end is None:
        sys.exit("The game said nothing of a tour:\n" + log[-2000:])

    frames = ",".join(str(step) for _, step, _ in stops)
    log = run(build, level, int(end.group(1)) + 2,
              ["--screenshot", "output://stop.png", "--screenshot-at", frames, "--exposure", "3"], output)
    for line in log.splitlines():
        if "[error]" in line or "[warning]" in line:
            print(line[27:200])

    name = os.path.basename(level).replace(".bsp", "")
    with open(os.path.join(output, f"{name}.txt"), "w") as listed:
        for number, _, what in stops:
            listed.write(f"{number} {what}\n")

    for sheet in range(0, len(stops), COLUMNS * ROWS):
        rows = [bytearray(WIDTH * COLUMNS * 3) for _ in range(HEIGHT * ROWS)]
        for place, (number, step, _) in enumerate(stops[sheet:sheet + COLUMNS * ROWS]):
            path = os.path.join(output, f"stop-{step:04d}.png")
            if not os.path.exists(path):
                continue
            width, height, picture = read_png(path)
            picture = mark(picture, number, 4, 4)
            left, top = (place % COLUMNS) * WIDTH, (place // COLUMNS) * HEIGHT
            for y in range(min(height, HEIGHT)):
                rows[top + y][left * 3:(left + min(width, WIDTH)) * 3] = picture[y][:min(width, WIDTH) * 3]
            os.remove(path)
        write_png(os.path.join(output, f"{name}-{sheet // (COLUMNS * ROWS)}.png"),
                  WIDTH * COLUMNS, HEIGHT * ROWS, [bytes(row) for row in rows])

    print(f"{len(stops)} things of {level}, see {output}/{name}-*.png and {name}.txt")


if __name__ == "__main__":
    main()
