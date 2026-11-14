#!/usr/bin/env bash
# Vortex OS: compila ambos kernels y ejecuta pruebas reales de componentes.
# No declara validado hardware, navegador grafico ni ISO si solo pasa este script.
set -euo pipefail
cd "$(dirname "$0")/.."
for c in gcc make python3; do
    command -v "$c" >/dev/null || { echo "[VORTEX] Falta: $c" >&2; exit 2; }
done
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "[VORTEX] Compilando kernel i386..."
make -C kernel -j2
echo "[VORTEX] Compilando kernel x86_64..."
make -C kernel64 -j2

echo "[VORTEX] Probando teclado y gestor de atajos..."
gcc -std=c11 -Wall -Wextra tools/kbdtest.c -o "$TMP/kbdtest"
"$TMP/kbdtest"

echo "[VORTEX] Probando apertura de Google/Chromium sin metacaracteres..."
gcc -std=c11 -O2 -Wall -Wextra -Werror tools/browser_launch_test.c -o "$TMP/browser-test"
"$TMP/browser-test"

echo "[VORTEX] Probando ciclo completo de ventanas, fullscreen y progreso..."
gcc -std=gnu11 -O1 -g -w -fsanitize=address,undefined -fno-omit-frame-pointer \
    tools/window_state_test.c kernel/zip.c -o "$TMP/window-test"
ASAN_OPTIONS=detect_leaks=0 "$TMP/window-test"

echo "[VORTEX] Probando calendario y zona America/Lima..."
gcc -std=c11 -Wall -Wextra -Werror tools/time_test.c -o "$TMP/time-test"
"$TMP/time-test"

echo "[VORTEX] Probando NTP y su integracion UDP con RTC..."
gcc -std=c11 -O2 -Wall -Wextra -Werror tools/ntp_test.c -o "$TMP/ntp-test"
"$TMP/ntp-test"
gcc -std=gnu11 -O2 -Wall -Wextra tools/ntp_stack_test.c -o "$TMP/ntp-stack-test"
"$TMP/ntp-stack-test"

echo "[VORTEX] Probando red real compilada con adaptador simulado..."
python3 tools/nettest.py

echo "[VORTEX] Todas las pruebas de compilacion y componentes terminaron correctamente."
echo "[VORTEX] Para probar arranque grafico real, usa el workflow vortex-escritorio-qemu."
