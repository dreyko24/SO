#!/usr/bin/env bash
# Ejecuta la ISO en QEMU sin escribir en discos del equipo.
set -euo pipefail
cd "$(dirname "$0")/.."
ISO="${1:-}"
if [[ -z "$ISO" ]]; then
    if [[ -f out/VortexOS.iso ]]; then ISO=out/VortexOS.iso
    elif [[ -f VortexOS-QEMU-BIOS.iso ]]; then ISO=VortexOS-QEMU-BIOS.iso
    else
        echo "[VORTEX] Falta la ISO. Compila bash tools/build-iso.sh o descarga el artefacto QEMU de GitHub Actions." >&2
        exit 1
    fi
fi
if [[ ! -f "$ISO" ]]; then echo "[VORTEX] No existe la ISO: $ISO" >&2; exit 1; fi
command -v qemu-system-x86_64 >/dev/null || { echo "[VORTEX] Instala qemu-system-x86 (Ubuntu: sudo apt install qemu-system-x86)" >&2; exit 1; }
exec qemu-system-x86_64 -name "Vortex OS - prueba" -m 2048 -smp 2 -boot d \
  -drive "file=$ISO,media=cdrom,if=ide,readonly=on" -vga std -nic user,model=e1000 -rtc base=utc -no-reboot
