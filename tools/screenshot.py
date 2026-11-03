"""Saves what the display shows, over USB: for when the Wi-Fi keeps the
display out of reach (an IoT network with client isolation).

    uv run --no-project --with pyserial python tools/screenshot.py [out.png] [--tap] [--demo=r|R]

--tap switches the page first; --demo=r shows made-up rain starting in 25
minutes, --demo=R rain that stops in 20.

Needs pyserial and macOS's sips for the PNG. Opening the port resets the
board; the script waits until it's back with the weather.
"""

import subprocess
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = Path(args[0] if args else "screen.png")
    port = next((p.device for p in list_ports.comports() if "usbserial" in p.device), None)
    if not port:
        raise SystemExit("No board on USB")
    link = serial.Serial()
    link.port, link.baudrate, link.timeout = port, 460800, 2
    link.dtr = link.rts = False
    link.open()
    # Opening the port may reset the board anyway: ask until it's up, which
    # takes Wi-Fi and the first weather.
    deadline, size = time.time() + 60, 0
    tapped = "--tap" not in sys.argv
    demo = next((a[len("--demo="):] for a in sys.argv if a.startswith("--demo=")), "")
    while not size:
        if time.time() > deadline:
            raise SystemExit("No answer from the board")
        if demo:
            link.write(demo.encode())
        if not tapped:
            link.write(b"t")
        link.write(b"s")
        for line in iter(link.readline, b""):
            if line.startswith(b"tap"):
                tapped = True
            if line.startswith(b"BMP ") and tapped:
                size = int(line.split()[1])
                break
            elif line.startswith(b"BMP "):
                link.read(int(line.split()[1]))
    data = b""
    while len(data) < size:
        chunk = link.read(size - len(data))
        if not chunk:
            raise SystemExit(f"Screenshot cut off after {len(data)} of {size} bytes")
        data += chunk
    bmp = out.with_suffix(".bmp")
    bmp.write_bytes(data)
    subprocess.run(["sips", "-s", "format", "png", str(bmp), "--out", str(out)], check=True, capture_output=True)
    bmp.unlink()
    print(out)


if __name__ == "__main__":
    main()
