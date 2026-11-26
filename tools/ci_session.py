#!/usr/bin/env python3
"""Verify startup accounts in a graphical Vortex VM with a disposable VortexFS disk."""

from contextlib import contextmanager
import json
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from ci_usb_hid import require_output, type_keys
from ci_usb_topology import qmp, wait_serial


FIRST = b"[vortex-session] primera cuenta"
LOADED = b"[vortex-session] cuentas listas"
OPEN = b"[vortex-session] escritorio desbloqueado"
REJECTED = b"[vortex-session] credenciales rechazadas"


def boot_image(directory, kernel, mode):
    boot = directory / "boot"
    grub = boot / "grub"
    grub.mkdir(parents=True)
    shutil.copyfile(kernel, boot / "vortex.elf")
    (grub / "grub.cfg").write_text(
        'set timeout=0\ninsmod all_video\nmenuentry "Vortex" {\n'
        '  set gfxpayload=1024x768x32\n'
        f'  multiboot /boot/vortex.elf {mode}\n  boot\n}}\n'
    )
    iso = directory.with_suffix(".iso")
    subprocess.run(["grub-mkrescue", "-d", "/usr/lib/grub/i386-pc", "-o", iso, directory],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return iso


@contextmanager
def machine(path, option, image, disk=None, *, ram="512M", boot_timeout=45):
    log, monitor = path.with_suffix(".log"), path.with_suffix(".sock")
    args = [
        "qemu-system-x86_64", "-m", ram, "-display", "none", "-monitor", "none",
        "-qmp", f"unix:{monitor},server=on,wait=off", "-serial", f"file:{log}", "-vga", "std",
        option, str(image), "-no-reboot",
    ]
    if option == "-cdrom":
        args.extend(["-boot", "d"])
    if disk is not None:
        args.extend(["-drive", f"file={disk},format=raw,if=ide,index=0"])
    log.touch()
    with subprocess.Popen(args) as qemu:
        try:
            require_output(log, qemu, b"== vortex-kernel", 0, boot_timeout, "Vortex did not boot")
            with socket.socket(socket.AF_UNIX) as conn:
                conn.settimeout(10)
                conn.connect(str(monitor))
                with conn.makefile("rwb") as stream:
                    if "QMP" not in json.loads(stream.readline()):
                        raise RuntimeError("Invalid QMP greeting")
                    qmp(stream, "qmp_capabilities")
                    yield qemu, log, stream
                    qmp(stream, "quit")
        finally:
            if qemu.poll() is None:
                qemu.terminate()
            if log.exists():
                print(log.read_text(errors="replace"), end="", flush=True)


def fields(stream, *values):
    for value in values:
        type_keys(stream, [*value, "ret"])


def run(kernel32, kernel64, text64):
    with tempfile.TemporaryDirectory(prefix="vortex-cuentas-") as temp:
        temp = Path(temp)
        disk = temp / "prueba.raw"
        with disk.open("wb") as target:
            target.truncate(48 * 1024 * 1024)
        # Nunca apuntar este formato a un disco que no haya creado la prueba.
        formatter = boot_image(temp / "formatter", kernel64, "terminal")
        start64 = boot_image(temp / "login64", kernel64, "sesion")
        start32 = boot_image(temp / "login32", kernel32, "sesion")
        with machine(temp / "format", "-cdrom", formatter, disk) as (qemu, log, stream):
            wait_serial(log, qemu, b"Vortex OS - terminal")
            fields(stream, "disco formatear si")
            wait_serial(log, qemu, b"disco listo: los archivos se guardan solos")
        with machine(temp / "setup", "-cdrom", start64, disk) as (qemu, log, stream):
            start = wait_serial(log, qemu, FIRST)
            fields(stream, "rosa", "vortexprueba9", "vortexprueba9")
            start = wait_serial(log, qemu, OPEN, start)
            type_keys(stream, ["meta_l"])  # Super abre Actividades.
            fields(stream, "usuarios")
            wait_serial(log, qemu, b"[vortex-apps] Usuarios", start)
            type_keys(stream, ["ret"])  # Crear usuario desde la cuenta administradora.
            fields(stream, "mar", "mareapassword9", "mareapassword9")
            start = wait_serial(log, qemu, b"[vortex-session] cuenta creada", start)
            type_keys(stream, ["tab", "ret"])  # Cambiar la contraseña del administrador.
            fields(stream, "vortexprueba9", "nuevaprueba9", "nuevaprueba9")
            start = wait_serial(log, qemu, "[vortex-session] contraseña actualizada".encode(), start)
            type_keys(stream, ["tab", "tab", "ret"])
            start = wait_serial(log, qemu, b"[vortex-session] escritorio bloqueado", start)
            fields(stream, "vortexprueba9")
            start = wait_serial(log, qemu, REJECTED, start)
            time.sleep(1.2)
            fields(stream, "nuevaprueba9")
            wait_serial(log, qemu, OPEN, start)
            for secret in (b"vortexprueba9", b"mareapassword9", b"nuevaprueba9"):
                if secret in log.read_bytes():
                    raise RuntimeError("Graphical login echoed a password")
        with machine(temp / "verify32", "-cdrom", start32, disk) as (qemu, log, stream):
            start = wait_serial(log, qemu, LOADED)
            fields(stream, "mar", "passwordincorrecto")
            start = wait_serial(log, qemu, REJECTED, start)
            time.sleep(1.2)
            fields(stream, "mareapassword9")
            wait_serial(log, qemu, OPEN, start)
        with machine(temp / "verifytext", "-kernel", text64, disk) as (qemu, log, stream):
            start = wait_serial(log, qemu, b"Iniciar sesion")
            if b"vortex> " in log.read_bytes():
                raise RuntimeError("The text shell was available before login")
            fields(stream, "rosa", "nuevaprueba9")
            start = wait_serial(log, qemu, b"[vortex-session] consola desbloqueada", start)
            wait_serial(log, qemu, b"vortex> ", start)
            if b"nuevaprueba9" in log.read_bytes():
                raise RuntimeError("Text login echoed the password")
        with machine(temp / "live", "-cdrom", start32) as (qemu, log, stream):
            start = wait_serial(log, qemu, FIRST)
            type_keys(stream, ["tab"] * 4 + ["ret"])
            wait_serial(log, qemu, b"[vortex-session] invitado en vivo", start)
    print("VORTEX SESIONES: alta, cambio, bloqueo, persistencia en 32/64, consola e invitado OK")


if __name__ == "__main__":
    run(*map(Path, sys.argv[1:]))
