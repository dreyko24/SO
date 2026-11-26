#!/usr/bin/env python3
"""Open Firefox from Activities and navigate with its own address bar in QEMU."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
import sys
import tempfile
from threading import Event, Thread
import time

from ci_session import FIRST, fields, machine
from ci_usb_hid import REPORT_DELAY, left_button, move_relative, type_keys
from ci_usb_topology import qmp, wait_serial


BROWSER = re.compile(
    rb'0x[0-9a-f]+ "([^"]+)": \("Navigator" "firefox-esr"\)\s+[1-9][0-9]{2,}x[1-9][0-9]{2,}'
)
PAGE = """<!doctype html><meta charset="utf-8"><title>Firefox en Vortex OS</title>
<h1>Firefox navegó desde el escritorio de Vortex OS</h1>
<script>fetch("/confirmado?origen=" + encodeURIComponent(document.title));</script>""".encode()


def shortcut(stream, modifier, key):
    qmp(stream, "send-key", keys=[{"type": "qcode", "data": code} for code in (modifier, key)],
        **{"hold-time": 35})
    time.sleep(REPORT_DELAY * 2)


def activity(stream, name):
    type_keys(stream, ["meta_l"])
    fields(stream, name)


def address(stream, port, path):
    type_keys(stream, "http")
    shortcut(stream, "shift", "semicolon")
    type_keys(stream, "//10.0.2.2")
    shortcut(stream, "shift", "semicolon")
    type_keys(stream, f"{port}/{path}")


def verify(iso, screenshot):
    received = Event()
    connected = Event()

    class PageHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/vortex-desktop":
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(PAGE)))
                self.end_headers()
                self.wfile.write(PAGE)
            elif self.path in ("/listo", "/confirmado?origen=Firefox%20en%20Vortex%20OS"):
                self.send_response(204)
                self.end_headers()
                (connected if self.path == "/listo" else received).set()
            else:
                self.send_error(404)

    with ThreadingHTTPServer(("127.0.0.1", 0), PageHandler) as server:
        http_thread = Thread(target=server.serve_forever, daemon=True)
        http_thread.start()
        try:
            with tempfile.TemporaryDirectory(prefix="vortex-firefox-gui-") as temp:
                with machine(Path(temp, "desktop"), "-cdrom", iso, ram="4096M",
                             boot_timeout=120) as (qemu, log, stream):
                    start = wait_serial(log, qemu, FIRST)
                    type_keys(stream, ["tab"] * 4 + ["ret"])
                    start = wait_serial(log, qemu, b"[vortex-session] invitado en vivo", start)
                    activity(stream, "firefox")
                    start = wait_serial(log, qemu, b"[vortex-browser] Firefox directo", start)
                    activity(stream, "terminal")
                    start = wait_serial(log, qemu, b"/> \r\n/> ", start)
                    type_keys(stream, "run curl -fsS -m 15 ")
                    address(stream, server.server_port, "listo")
                    type_keys(stream, ["ret"])
                    if not connected.wait(25):
                        raise RuntimeError("The Vortex guest cannot reach the HTTP test server")
                    wait_serial(log, qemu, b"[termino con codigo 0]", start)

                    deadline = time.monotonic() + 240
                    first_frame = None
                    while time.monotonic() < deadline and qemu.poll() is None:
                        cursor = log.stat().st_size
                        fields(stream, "run xwininfo -root -tree")
                        wait_serial(log, qemu, b"/> ", cursor)
                        window = BROWSER.search(log.read_bytes()[cursor:])
                        if window:
                            if first_frame is None:
                                first_frame = time.monotonic()
                            if window[1] != b"Mozilla Firefox" or time.monotonic() - first_frame >= 22:
                                break
                        time.sleep(8)
                    else:
                        raise RuntimeError("Firefox did not draw a usable X11 window")

                    start = log.stat().st_size
                    activity(stream, "firefox")
                    wait_serial(log, qemu, b"[vortex-apps] Firefox", start)
                    # Partir del borde superior, incluso si PS/2 tarda en recibir el primer movimiento.
                    move_relative(stream, 0, -600)
                    time.sleep(0.4)
                    move_relative(stream, 0, 145)
                    time.sleep(0.5)
                    left_button(stream, True)
                    left_button(stream, False)
                    time.sleep(1)
                    shortcut(stream, "ctrl", "l")
                    time.sleep(0.8)
                    address(stream, server.server_port, "vortex-desktop")
                    qmp(stream, "screendump", filename=str(screenshot.with_suffix(".entrada.ppm").resolve()))
                    type_keys(stream, ["ret"])
                    navigated = received.wait(100)
                    qmp(stream, "screendump", filename=str(screenshot.resolve()))
                    if not navigated:
                        raise RuntimeError("Firefox did not navigate from its own address bar")
        finally:
            server.shutdown()
            http_thread.join()
    print("VORTEX FIREFOX ESCRITORIO: Actividades, ventana X11, barra de direcciones y JavaScript OK")


if __name__ == "__main__":
    verify(*map(Path, sys.argv[1:]))
