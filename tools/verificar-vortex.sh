#!/usr/bin/env bash
# Vortex OS: pruebas locales del sistema integrado, sin alterar discos fisicos.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
for p in gcc make python3;do command -v "$p" >/dev/null||{ echo "Falta $p" >&2;exit 2;};done
printf '[VORTEX] Compilando los kernels reales de 32 y 64 bits...\n'
make -C kernel -j2
make -C kernel64 -j2
test -s kernel/vortex-kernel.elf
test -s kernel64/vortex-kernel64.elf
printf '[VORTEX] Teclado y atajos...\n'
gcc -std=c11 -Wall -Wextra tools/kbdtest.c -o "$tmp/kbd"
"$tmp/kbd"
printf '[VORTEX] Reloj NTP, UTC y validacion de paquetes...\n'
gcc -std=c11 -O2 -Wall -Wextra -Werror tools/ntp_test.c -o "$tmp/ntp"
"$tmp/ntp"
gcc -std=gnu11 -O2 -w tools/ntp_stack_test.c -o "$tmp/ntp-stack"
"$tmp/ntp-stack"
printf '[VORTEX] Navegador HTTPS/Google - sanitizacion de comandos...\n'
gcc -std=c11 -O2 -Wall -Wextra -Werror tools/browser_launch_test.c -o "$tmp/browser"
"$tmp/browser"
printf '[VORTEX] Pila TCP/UDP/DNS de Vortex en simulacion de red...\n'
python3 tools/nettest.py
printf '[VORTEX] Enciclopedia y calendario...\n'
gcc -std=c11 -O1 -g -Wall -Wextra -Werror tests/saber_progress_test.c -o "$tmp/saber"
"$tmp/saber"
gcc -std=c11 -O1 -g -Wall -Wextra -Werror tests/clock_calendar_test.c -o "$tmp/calendar"
"$tmp/calendar"
echo '[VORTEX] Compilacion y regresiones locales aprobadas; pendiente arranque en QEMU/hardware.'
