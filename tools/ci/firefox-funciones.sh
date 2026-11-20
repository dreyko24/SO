#!/usr/bin/env bash
# Prueba el navegador en Vortex 64 bits. El CPIO Debian tiene que estar descomprimido.
set -euo pipefail

if [[ $# -ne 4 || ! -s $1 || ! -s $2 || ! -s $3 ]]; then
  printf 'Uso: %s <kernel64-text.elf> <debian.cpio> <vortex-extra.cpio> <directorio-resultados>\n' "$0" >&2
  exit 2
fi

tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
salida=$4
mkdir -p -- "$salida"
python3 -u -m http.server 18192 --bind 127.0.0.1 --directory "$tools/ci/firefox" > "$salida/http.log" 2>&1 &
servidor=$!
trap 'kill "$servidor" 2>/dev/null || true; wait "$servidor" 2>/dev/null || true' EXIT
sleep 1
kill -0 "$servidor"

SECONDS=0
VORTEX_PROMPT='vortex> ' KELF=$1 timeout --signal=TERM --kill-after=5s 280s \
  python3 "$tools/ci_drive.py" "$tools/ci/firefox-funciones.txt" "$salida/serie.log" -- \
  qemu-system-x86_64 -m 4096 -kernel "$1" -initrd "$2,$3" \
  -display none -serial stdio -monitor none -no-reboot \
  -netdev user,id=n0 -device e1000,netdev=n0
printf 'QEMU finalizó en %s s. Registros en %s\n' "$SECONDS" "$salida"

if ! grep -aq '^\[ok\] modo largo (64 bits)' "$salida/serie.log" ||
   ! grep -aq '"Vortex Firefox funciones OK[^"]*": ("Navigator" "firefox-esr")' "$salida/serie.log" ||
   ! grep -aFq '"GET /funciones-dato.txt?sessionStorage=OK&localStorage=OK&fetch=OK&webm=OK&resultado=OK HTTP/1.1" 200' "$salida/http.log"; then
  tail -n 20 "$salida/http.log" "$salida/serie.log" >&2
  exit 1
fi
printf 'Firefox ESR verificó almacenamiento web, Fetch y decodificación y reproducción de WebM en Vortex.\n'
