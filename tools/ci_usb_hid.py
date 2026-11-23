#!/usr/bin/env python3
"""Exercise the desktop with only an xHCI keyboard and relative mouse in QEMU."""

import argparse
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from ci_usb_topology import qmp, wait_serial


MENU = b"[vortex-apps] menu abierto"
TERMINAL = b"[vortex-apps] Terminal"
QEMU_HID = b"[vortex-xhci] VID=627 PID=1 clase=3"
DISCONNECTED = b"[vortex-xhci] desconectado"
REPORT_DELAY = 0.065


def send_events(stream, *events):
    qmp(stream, "input-send-event", events=list(events))
    # Leave time for the guest to poll the interrupt endpoint between reports.
    time.sleep(REPORT_DELAY)


def key_event(stream, key, down):
    send_events(stream, {"type": "key", "data": {
        "key": {"type": "qcode", "data": key}, "down": down,
    }})


def left_button(stream, down):
    send_events(stream, {"type": "btn", "data": {"button": "left", "down": down}})


def type_keys(stream, keys):
    for key in keys:
        shift = len(key) == 1 and key.isupper()
        key = "spc" if key == " " else key.lower()
        qcodes = ["shift", key] if shift else [key]
        qmp(stream, "send-key", keys=[{"type": "qcode", "data": code} for code in qcodes],
            **{"hold-time": 35})
        time.sleep(REPORT_DELAY * 2)


def move_relative(stream, dx, dy):
    while dx or dy:
        x = max(-100, min(100, dx))
        y = max(-100, min(100, dy))
        send_events(stream, *({"type": "rel", "data": {"axis": axis, "value": step}}
                              for axis, step in (("x", x), ("y", y)) if step))
        dx -= x
        dy -= y


def require_output(serial, qemu, marker, start, timeout, description):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and qemu.poll() is None:
        location = serial.read_bytes().find(marker, start)
        if location >= 0:
            return location + len(marker)
        time.sleep(0.1)
    raise RuntimeError(f"{description}: expected {marker!r} in the Vortex serial output")


def add_hid(stream, serial, qemu, keyboard, mouse):
    # QEMU's emulated keyboard and mouse share the same vendor/product ID.
    for driver, device_id, port, label in (
        ("usb-kbd", keyboard, "1", b"teclado"),
        ("usb-mouse", mouse, "2", b"raton"),
    ):
        start = serial.stat().st_size
        qmp(stream, "device_add", driver=driver, bus="xhci.0", port=port, id=device_id)
        start = wait_serial(serial, qemu, QEMU_HID, start)
        wait_serial(serial, qemu, b"[vortex-xhci] entrada HID " + label, start)


def remove_hid(stream, serial, qemu, device_id):
    start = serial.stat().st_size
    qmp(stream, "device_del", id=device_id)
    wait_serial(serial, qemu, DISCONNECTED, start)


def exercise_desktop(stream, serial, qemu, timeout):
    add_hid(stream, serial, qemu, "keyboard", "mouse")
    start = serial.stat().st_size
    # The 1024x768 CI desktop starts at (512, 384). Click Applications at (46, 140).
    move_relative(stream, 46 - 512, 140 - 384)
    left_button(stream, True)
    left_button(stream, False)
    start = require_output(serial, qemu, MENU, start, timeout, "USB mouse did not open Applications")
    type_keys(stream, [*"terminal", "ret"])
    start = require_output(serial, qemu, TERMINAL, start, timeout, "USB keyboard did not launch Terminal")
    type_keys(stream, [*"echo Vortexhidd", "ret"])
    start = require_output(serial, qemu, b"Vortexhidd\r\n/> ", start, timeout,
                           "USB keyboard did not type the exact mixed-case text with a repeated key")

    # Unplug while a mouse button and a keyboard modifier are held.
    left_button(stream, True)
    start = require_output(serial, qemu, MENU, start, timeout, "USB mouse did not register a second press")
    type_keys(stream, ["esc", *"echo "])
    key_event(stream, "shift", True)
    type_keys(stream, ["v", "ret"])
    require_output(serial, qemu, b"V\r\n/> ", start, timeout,
                   "The Shift modifier must be held before unplugging the keyboard")
    remove_hid(stream, serial, qemu, "keyboard")
    remove_hid(stream, serial, qemu, "mouse")
    add_hid(stream, serial, qemu, "keyboard-again", "mouse-again")

    start = serial.stat().st_size
    left_button(stream, True)
    left_button(stream, False)
    start = require_output(serial, qemu, MENU, start, timeout, "Reconnected USB mouse did not register a fresh press")
    type_keys(stream, ["esc", *"echo vortexagain", "ret"])
    require_output(serial, qemu, b"vortexagain\r\n/> ", start, timeout,
                   "Reconnected USB keyboard did not type lowercase after the held Shift was unplugged")


def exercise_during_import(stream, serial, qemu, timeout):
    start = serial.stat().st_size
    qmp(stream, "device_add", driver="usb-storage", bus="xhci.0", port="3",
        id="disk", drive="stick")
    start = wait_serial(serial, qemu, b"archivo VORTEX.PNG (1965 bytes) importado", start)
    # Tras el PNG, la lectura de ZZRING.BIN mantiene activo el transporte bulk.
    type_keys(stream, [*"echo inputduringusb", "ret"])
    if b"USB FAT32 real leido" in serial.read_bytes()[start:]:
        raise RuntimeError("The throttled USB import ended before the input test")
    qmp(stream, "device_del", id="disk")
    start = wait_serial(serial, qemu, b"lectura interrumpida", start)
    require_output(serial, qemu, b"inputduringusb\r\n/> ", start, timeout,
                   "USB input was lost while importing from the USB disk")
    print("xHCI: keyboard input arrived intact during a shared-controller bulk import", flush=True)


def run(iso, timeout, disk_image):
    with tempfile.TemporaryDirectory(prefix="vortex-usb-hid-") as temp:
        serial = Path(temp, "serial.log")
        monitor = Path(temp, "qmp.sock")
        cmd = [
            "qemu-system-x86_64", "-machine", "pc,i8042=off", "-display", "none",
            "-monitor", "none", "-qmp", f"unix:{monitor},server=on,wait=off",
            "-serial", f"file:{serial}", "-cdrom", iso, "-boot", "d", "-m", "512M",
            "-vga", "std", "-device", "qemu-xhci,id=xhci", "-no-reboot",
        ]
        if disk_image:
            cmd.extend(["-drive", f"if=none,id=stick,format=raw,file={disk_image},readonly=on,iops_rd=12"])
        with subprocess.Popen(cmd) as qemu:
            try:
                wait_serial(serial, qemu, b"Iniciando escritorio")
                if b"framebuffer 1024x768 " not in serial.read_bytes():
                    raise RuntimeError("The USB HID test requires a 1024x768 graphical Vortex ISO")
                with socket.socket(socket.AF_UNIX) as sock:
                    sock.settimeout(10)
                    sock.connect(str(monitor))
                    with sock.makefile("rwb") as stream:
                        if "QMP" not in json.loads(stream.readline()):
                            raise RuntimeError("Invalid QMP greeting")
                        qmp(stream, "qmp_capabilities")
                        exercise_desktop(stream, serial, qemu, timeout)
                        if disk_image:
                            exercise_during_import(stream, serial, qemu, timeout)
            finally:
                if qemu.poll() is None:
                    qemu.terminate()
                if serial.exists():
                    print(serial.read_text(errors="replace"), end="", flush=True)
    print("xHCI: keyboard and relative mouse controlled the desktop before and after reconnecting")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", help="graphical Vortex ISO booting directly at 1024x768")
    parser.add_argument("--input-timeout", type=float, default=12,
                        help="seconds to wait for a desktop response (default: 12)")
    parser.add_argument("--disk-image", help="FAT32 fixture with VORTEX.PNG and ZZRING.BIN")
    args = parser.parse_args()
    run(args.iso, args.input_timeout, args.disk_image)
