#!/usr/bin/env python3
"""Comprueba una descarga HTTP de Firefox ESR en Vortex de 64 bits."""

import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import subprocess
import threading
import time


ADJUNTO = bytes(range(256)) * 64 + b"\x00VORTEX\xff"
PERFIL = b'''user_pref("browser.download.folderList", 2);
user_pref("browser.download.dir", "/tmp/ff-dl");
user_pref("browser.download.useDownloadDir", true);
user_pref("browser.download.always_ask_before_handling_new_types", false);
'''


class Descarga(BaseHTTPRequestHandler):
    def responder(self, tipo, contenido, nombre=None):
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(contenido)))
        if nombre:
            self.send_header("Content-Disposition", f'attachment; filename="{nombre}"')
        self.end_headers()
        self.wfile.write(contenido)

    def do_GET(self):
        if self.path == "/perfil.js":
            self.responder("application/javascript", PERFIL)
        elif self.path == "/adjunto":
            navegador = self.headers.get("User-Agent", "")
            if "Firefox/" not in navegador:
                self.send_error(403)
                return
            self.log_message("Solicitud del adjunto de %s", navegador)
            self.responder("application/octet-stream", ADJUNTO, "vortex.bin")
        else:
            self.send_error(404)

    def do_PUT(self):
        if self.path != "/guardado":
            self.send_error(404)
            return
        if self.headers.get("Content-Length") != str(len(ADJUNTO)):
            self.send_error(400, "Longitud inesperada")
            return
        recibido = self.rfile.read(len(ADJUNTO))
        if recibido != ADJUNTO:
            self.send_error(422, "El archivo guardado difiere del adjunto")
            return
        self.server.recibido = recibido
        self.send_response(201)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, formato, *args):
        print(f"[{self.log_date_time_string()}] {formato % args}", file=self.server.registro, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kernel", type=Path, help="vortex-kernel64-text.elf")
    parser.add_argument("debian", type=Path, help="debian.cpio descomprimido")
    parser.add_argument("extra", type=Path, help="vortex-extra.cpio")
    parser.add_argument("salida", type=Path, help="directorio de los registros y del archivo guardado")
    args = parser.parse_args()
    for archivo in (args.kernel, args.debian, args.extra):
        if not archivo.is_file() or archivo.stat().st_size == 0:
            parser.error(f"archivo ausente o vacío: {archivo}")

    args.salida.mkdir(parents=True, exist_ok=True)
    tools = Path(__file__).resolve().parents[1]
    comandos = Path(__file__).with_suffix(".txt")
    serie = args.salida / "serie.log"
    entorno = {**os.environ, "KELF": str(args.kernel), "VORTEX_PROMPT": "vortex> "}
    inicio = time.monotonic()
    with (args.salida / "http.log").open("w", encoding="utf-8") as registro:
        with ThreadingHTTPServer(("127.0.0.1", 18193), Descarga) as servidor:
            servidor.registro = registro
            servidor.recibido = None
            hilo = threading.Thread(target=servidor.serve_forever, daemon=True)
            hilo.start()
            try:
                proceso = subprocess.run([
                    "timeout", "--signal=TERM", "--kill-after=5s", "320s",
                    "python3", tools / "ci_drive.py", comandos, serie, "--",
                    "qemu-system-x86_64", "-m", "4096", "-kernel", args.kernel,
                    "-initrd", f"{args.debian},{args.extra}", "-display", "none",
                    "-serial", "stdio", "-monitor", "none", "-no-reboot",
                    "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
                ], env=entorno)
            finally:
                servidor.shutdown()
                hilo.join()

    print(f"QEMU finalizó en {time.monotonic() - inicio:.0f} s. Registros en {args.salida}")
    hash_adjunto = hashlib.sha256(ADJUNTO).hexdigest()
    salida_serie = serie.read_text(encoding="utf-8", errors="replace")
    if (proceso.returncode or servidor.recibido != ADJUNTO
            or "[ok] modo largo (64 bits)" not in salida_serie
            or '("Navigator" "firefox-esr")' not in salida_serie
            or f"{hash_adjunto}  /tmp/ff-dl/vortex.bin" not in salida_serie.splitlines()):
        print("FALLO: el navegador y el archivo guardado no superaron todas las comprobaciones.")
        return 1

    (args.salida / "descarga-recibida.bin").write_bytes(servidor.recibido)
    print(f"Firefox ESR guardó el adjunto de {len(ADJUNTO)} bytes; SHA256 {hash_adjunto}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
