#!/usr/bin/env python3
"""Read descriptors from an emulated xHCI keyboard and a USB disk."""

import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

from ci_usb_topology import qmp, wait_serial


def run(iso, image):
    with tempfile.TemporaryDirectory(prefix="vortex-xhci-") as temp:
        serial = Path(temp, "serial.log")
        monitor = Path(temp, "qmp.sock")
        cmd = [
            "qemu-system-x86_64", "-display", "none", "-monitor", "none",
            "-qmp", f"unix:{monitor},server=on,wait=off", "-serial", f"file:{serial}",
            "-cdrom", iso, "-boot", "d", "-m", "512M", "-vga", "std",
            "-device", "qemu-xhci,id=xhci",
            "-device", "usb-kbd,bus=xhci.0,port=1,id=keyboard",
            "-drive", f"if=none,id=stick,format=raw,file={image},snapshot=on",
        ]
        with subprocess.Popen(cmd) as qemu:
            try:
                wait_serial(serial, qemu, b"Iniciando escritorio")
                hid = b"[vortex-xhci] VID=627 PID=1 clase=3"
                drive = b"[vortex-xhci] VID=46f4 PID=1 clase=8"
                start = wait_serial(serial, qemu, hid)
                with socket.socket(socket.AF_UNIX) as sock:
                    sock.settimeout(10)
                    sock.connect(str(monitor))
                    with sock.makefile("rwb") as stream:
                        if "QMP" not in json.loads(stream.readline()):
                            raise RuntimeError("Invalid QMP greeting")
                        qmp(stream, "qmp_capabilities")
                        qmp(stream, "device_add", driver="usb-storage", bus="xhci.0",
                            port="2", id="disk", drive="stick")
                        start = wait_serial(serial, qemu, drive, start)
                        end = wait_serial(serial, qemu, b"\n", start)
                        if b"velocidad=4" not in serial.read_bytes()[start:end]:
                            raise RuntimeError("The xHCI disk did not enumerate at SuperSpeed")
                        qmp(stream, "device_del", id="keyboard")
                        start = wait_serial(serial, qemu, b"[vortex-xhci] desconectado", start)
                        qmp(stream, "device_add", driver="usb-kbd", bus="xhci.0",
                            port="1", id="keyboard-again")
                        wait_serial(serial, qemu, hid, start)
                print("xHCI: HID and Mass Storage descriptors, removal and re-enumeration passed")
            finally:
                if qemu.poll() is None:
                    qemu.terminate()
                if serial.exists():
                    print(serial.read_text(errors="replace"), end="")


if __name__ == "__main__":
    run(*sys.argv[1:])
