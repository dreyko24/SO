#!/usr/bin/env python3
"""Verify native UHCI storage with a keyboard on the first occupied port."""

import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def qmp(stream, command, **arguments):
    request = {"execute": command, "arguments": arguments}
    stream.write(json.dumps(request).encode() + b"\n")
    stream.flush()
    for line in stream:
        response = json.loads(line)
        if "error" in response:
            raise RuntimeError(response["error"])
        if "return" in response:
            return response["return"]
    raise RuntimeError(f"QEMU disconnected during {command}")


def wait_serial(path, qemu, marker, start=0):
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline and qemu.poll() is None:
        serial = path.read_bytes() if path.exists() else b""
        location = serial.find(marker, start)
        if location >= 0:
            return location + len(marker)
        time.sleep(0.1)
    raise RuntimeError(f"Missing marker in Vortex serial output: {marker!r}")


def run(iso, image, hub):
    with tempfile.TemporaryDirectory(prefix="vortex-usb-") as temp:
        serial = Path(temp, "serial.log")
        monitor = Path(temp, "qmp.sock")
        keyboard, disk = ("1.1", "1.2") if hub else ("1", "2")
        cmd = [
            "qemu-system-x86_64", "-display", "none", "-monitor", "none",
            "-qmp", f"unix:{monitor},server=on,wait=off", "-serial", f"file:{serial}",
            "-cdrom", iso, "-boot", "d", "-m", "512M", "-vga", "std", "-usb",
            "-drive", f"if=none,id=stick,format=raw,file={image},snapshot=on",
            "-drive", f"if=none,id=stick-again,format=raw,file={image},snapshot=on",
        ]
        if hub:
            cmd += ["-device", "usb-hub,bus=usb-bus.0,port=1"]
        cmd += ["-device", f"usb-kbd,bus=usb-bus.0,port={keyboard}"]
        with subprocess.Popen(cmd) as qemu:
            try:
                start = wait_serial(serial, qemu, b"fallo de enumeracion")
                if b"periferico HID detectado" not in serial.read_bytes()[:start]:
                    raise RuntimeError("Vortex did not identify the keyboard as HID")
                with socket.socket(socket.AF_UNIX) as sock:
                    sock.settimeout(10)
                    sock.connect(str(monitor))
                    with sock.makefile("rwb") as stream:
                        greeting = json.loads(stream.readline())
                        if "QMP" not in greeting:
                            raise RuntimeError("Invalid QMP greeting")
                        qmp(stream, "qmp_capabilities")
                        device = dict(driver="usb-storage", bus="usb-bus.0", port=disk)
                        qmp(stream, "device_add", id="usb-stick", drive="stick", **device)
                        asset = b"archivo VORTEX.PNG (1965 bytes) importado"
                        mounted = b"USB FAT32 real leido"
                        start = wait_serial(serial, qemu, asset, start)
                        start = wait_serial(serial, qemu, mounted, start)
                        qmp(stream, "device_del", id="usb-stick")
                        start = wait_serial(serial, qemu, b"USB desconectado", start)
                        # QEMU removes the legacy -drive backend when its USB device is unplugged.
                        qmp(stream, "device_add", id="usb-stick-again", drive="stick-again", **device)
                        start = wait_serial(serial, qemu, asset, start)
                        wait_serial(serial, qemu, mounted, start)
                print(f"UHCI {disk}: FAT32 hotplug, removal and reimport passed", flush=True)
            finally:
                if qemu.poll() is None:
                    qemu.terminate()
                if serial.exists():
                    print(serial.read_text(errors="replace"), end="", flush=True)


if __name__ == "__main__":
    iso, image = sys.argv[1:]
    run(iso, image, hub=False)
    run(iso, image, hub=True)
