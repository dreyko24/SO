# INFORME DE CONTINUIDAD — Vortex OS
Fecha de trabajo: 14 de noviembre de 2026 (Perú, UTC-5)

## 1. Identificación y recuperación del proyecto

- Repositorio conectado: https://github.com/dreyko24/SO
- Rama base: `main`.
- Rama con los cambios recientes: `vortex/window-management-20261114`.
- Pull request abierto de esos cambios: https://github.com/dreyko24/SO/pull/9
- Rama histórica de mejoras previas: `vortex-desktop-window-manager` (PR #8).
- **No se debe reconstruir el proyecto desde cero, borrar código heredado ni asumir que el PR #9 ya está fusionado**. Al iniciar otro chat, consultar GitHub para ver el último commit y estado actualizado de las ramas.
- El árbol incluye `kernel/` (i386 y código común), `kernel64/` (x86_64 y compatibilidad Linux), `tools/` (ISO, host tests, simuladores), `.github/workflows/` (CI). El binario del kernel conserva nombres `kng` por compatibilidad; la interfaz y la ISO usan Vortex OS.

## 2. Orden del usuario (vigente)

El objetivo es un SO **real**, no un simulador ni una colección de botones sin implementar: kernel, gestor de ventanas multiventana, dock que no tape ventanas, pantalla completa, navegador moderno con HTTPS/HTML/CSS/JavaScript, reloj Perú, Vortex Saber educativo, almacenamiento persistente, ofimática, aplicaciones, sonido, vídeo, dispositivos USB físicos, usuarios y permisos, red, actualizaciones, rendimiento y seguridad. Trabajar de manera continua dentro de cada ejecución, sin pedir autorización entre correcciones. Guardar avances y pruebas con honestidad. Si el contexto de chat se agota, entregar un informe de continuidad como este, con código y asuntos pendientes.

## 3. Avances heredados que hay que preservar

El proyecto ya contenía dos kernels, pilas de red y discos IDE/SATA/NVMe según sus fuentes, sistema de archivos KNGFS, capa de procesos y compatibilidad con programas ELF/Linux de 64 bits, servidor X/Xvfb, escritorio Vortex con ventanas arrastrables, minimización/maximización/pantalla completa, dock lateral, lanzador de aplicaciones, Vortex Saber de cuatro materias, reloj que convierte RTC UTC a hora local, pruebas QEMU/CI, creador de ISO, proxy web y scripts de instalación de aplicaciones Debian. Los documentos `VORTEX_ESTADO.md` y `TRASPASO_kng_v24.md` describen estados históricos; **no se debe dar por comprobado hardware real basándose únicamente en esas descripciones**.

## 4. Cambios guardados en GitHub durante la continuación

### 4.1. Ventanas, tareas y atajos
Archivos: `kernel/kbd.c`, `kernel/gui.c`, `tools/kbdtest.c`.

- Se traducen `Ctrl+Tab` a `0x502` y `Ctrl+Mayús+Tab` a `0x503` sin alterar el Tab normal.
- El escritorio cambia el foco entre ventanas, incluso si una estaba minimizada. Al recibir foco, `focus()` quita el estado minimizado.
- Se ajustó la selección de ventanas cuando la ventana activa no ocupa el último lugar del arreglo `order[]` debido a minimización.
- Se agregaron pruebas de teclado y una regresión específica del ciclo de ventanas: `tools/window_state_test.c`. Comprueba foco, dos aplicaciones simultáneas, minimizar, restaurar, maximizar con dock reservado, fullscreen real sin dock, regreso a geometría anterior, F10 para nueva instancia, Ctrl+W para cerrar sin afectar la anterior.
- El número de ventanas es el del código existente (`MAXW=24`), no equivale por sí mismo a aislamiento de procesos moderno.

### 4.2. Hora y fecha — UTC / America/Lima
Archivos: `kernel/net.c`, `kernel/net.h`, `kernel/ntp.h`, `kernel/gui.c`, `tools/ntp_test.c`, `tools/ntp_stack_test.c`, `tools/nettest_main.c`, `tools/hostgui.c`.

- Se integró cliente SNTP asíncrono en la pila UDP existente: DNS `pool.ntp.org`, puerto NTP remoto 123 y puerto local 49423.
- Petición NTP versión 4, correlación de los ocho bytes de la marca de transmisión mediante la marca de origen en la respuesta; validación de IP de origen, puerto, modo servidor, versión, estrato y estado leap.
- Manejo de rollover de era NTP 2036, comprobación de fecha válida, reintentos, espera entre fallos y resincronización nominal cada seis horas.
- `net_utc_epoch()` devuelve el UTC sincronizado, progresando con el temporizador del kernel; si aún no hubo NTP, utiliza `krtc_epoch()` RTC sin reescribir el CMOS.
- La GUI convierte este UTC a hora local con huso configurado (por defecto UTC-5, America/Lima) y muestra la fuente NTP/RTC en Ajustes.
- NTP no usa autenticación criptográfica (NTS): es posible el spoofing de redes no confiables y no debe usarse como prueba de hora segura.
- El simulador de red HOSTNET deshabilita la consulta NTP externa para que las pruebas previas de red no dependan de Internet.

### 4.3. Navegador
Archivo: `kernel/gui.c`.

- El botón para iniciar Chromium (si está instalado) ahora pasa la URL HTTP/HTTPS visible en el navegador integrado cuando su longitud y caracteres son seguros para la construcción del comando. Antes abría Chromium sin trasladar el destino actual.
- El navegador HTML interno sigue siendo limitado; Chromium instalado, la capa de compatibilidad Linux y Xvfb todavía necesitan validación conjunta para declarar que CSS/JS/HTTPS modernos funcionan de verdad.
- No se implementó TLS en el núcleo ni se declaró que el visor HTML simple sea equivalente a Chromium.

### 4.4. Vortex Saber
Archivos: `kernel/gui.c`, `tools/window_state_test.c`.

- De 4 a 6 materias: Matemáticas, Ciencias, Historia del Perú, Tecnología, Geografía y Comunicación.
- De 12 a 18 preguntas evaluables en total. Cada nueva materia tiene tres lecturas breves, tres preguntas de tres opciones, respuesta correcta y explicación.
- Biblioteca reorganizada visualmente en cuadrícula de tres columnas y dos filas, conservando el juego de parejas, la XP, el nivel y las marcas de progreso.
- Nuevo contador de insignias de dominio (mejores marcas de 3/3).
- Compatibilidad de progreso: `saber_load()` reconoce archivos anteriores de cinco dígitos + salto de línea, rescata cuatro materias y el récord del juego, e inicializa las dos nuevas a cero. Guarda en el nuevo formato de siete dígitos + salto de línea.
- Archivo de progreso: `/Documentos/Vortex-Saber-progreso.txt`; solo persiste tras reiniciar si el almacenamiento KNGFS está activado.
- La prueba de migración está escrita en `tools/window_state_test.c` y pendiente de ejecución integrada.

### 4.5. Herramientas y CI
Archivos: `tools/hostgui.c`, `.github/workflows/vortex-integracion.yml`, `.github/workflows/window-regression.yml`.

- Se corrigió el cálculo del dock del simulador de host para usar la geometría real lateral `dock_icon_pos()`; el cálculo heredado apuntaba a un dock inferior antiguo.
- Se añadieron stubs para eventos de entrada/procesos y reloj de prueba en el simulador.
- El flujo de integración compila ambos kernels y añade pruebas NTP puras, NTP integrado en pila de red y estado de ventanas, además de teclado, calendario y red simulada.
- **La existencia de un archivo de workflow no demuestra que haya corrido ni pasado**. Comprobar pestaña Actions y descargar sus logs; corregir errores antes de fusionar.

## 5. Pruebas: evidencia real y pendientes

**Ejecutado en esta sesión, con resultado positivo:**
```sh
gcc -std=c11 -O2 -Wall -Wextra -Werror tools/ntp_test.c -o /tmp/vortex-ntp-test
/tmp/vortex-ntp-test
# NTP parser: 9/9 checks OK
```
Se ejecutó sobre una copia local del mismo código de cabecera y pruebas que se creó en GitHub. Verifica validación de NTP, formato, correlación de solicitud, rechazos de respuesta e identificación de era 2036.

**No se ha verificado en esta ejecución:**
- Compilación integrada `make -C kernel` y `make -C kernel64` tras estos cambios.
- `tools/window_state_test.c` completo (necesita simular el resto del árbol).
- `tools/ntp_stack_test.c` completo y la red real.
- ISO resultante, GRUB/UEFI, arranque en QEMU o hardware físico.
- Funcionamiento de Chromium real con Google, CSS, JavaScript, sonido/vídeo o instalación.
- Detección y lectura de memoria USB física.
- Prueba visual del nuevo contenido Saber y preservación de datos tras un reinicio real.

Limitación concreta del entorno de esta sesión: se pudo escribir y leer con GitHub conectado, pero el contenedor local no pudo clonar el repositorio (`Could not resolve host: github.com`). Los tests de C independientes sí pudieron compilarse; los tests integrados deben pasar por Actions u otro entorno con el árbol completo. Los estados de ejecución consultados mediante la conexión de GitHub no mostraron resultados confirmados para el último commit.

## 6. Ejecución y validación reproducible

En un PC Linux Ubuntu o WSL con permisos y dependencias de GNU y GRUB:
```sh
git clone https://github.com/dreyko24/SO.git
cd SO
git fetch origin
git checkout vortex/window-management-20261114
sudo apt update
sudo apt install -y build-essential gcc-multilib binutils make python3 grub-pc-bin grub-efi-amd64-bin xorriso mtools
make -C kernel -j2
make -C kernel64 -j2
gcc -std=c11 -O2 -Wall -Wextra -Werror tools/ntp_test.c -o /tmp/vortex-ntp-test && /tmp/vortex-ntp-test
gcc -std=gnu11 -O2 -Wall -Wextra tools/ntp_stack_test.c -o /tmp/vortex-ntp-stack-test && /tmp/vortex-ntp-stack-test
gcc -std=gnu11 -O1 -g -w -fsanitize=address,undefined tools/window_state_test.c -o /tmp/vortex-window-test && ASAN_OPTIONS=detect_leaks=0 /tmp/vortex-window-test
gcc -std=c11 -Wall -Wextra -Werror tools/time_test.c -o /tmp/vortex-timetest && /tmp/vortex-timetest
python3 tools/nettest.py
bash tools/build-iso.sh
```
Se espera `out/VortexOS.iso` tras una compilación ISO exitosa, pero **no se ha generado ni verificado aquí**. Para las aplicaciones Debian, `KNG_DEBIAN=1 bash tools/build-iso.sh`; esto requiere una raíz Debian y más recursos. Los flujos de CI `vortex-integracion` y `vortex-escritorio-qemu` son rutas adicionales para recopilar evidencia. Cualquier operación `disco formatear si` o instalación a disco borra datos: **no ejecutar sin escoger una unidad de pruebas identificada y copia de seguridad**.

## 7. Defectos y trabajos técnicos que siguen abiertos

- **Primero, compilar y validar:** ejecutar CI del PR #9, inspeccionar compiladores y logs. Corregir fallos, tests del gestor de ventanas y render, NTP en HOSTNET, QEMU boot/snapshots y regresiones en 32/64 bits. No fusionar a main hasta cerrar las regresiones importantes.
- **USB físico:** el SO no dispone de controlador xHCI/EHCI y pila USB mass storage plenamente verificadas. Implementar enumeración de host controller/root hub, dispositivos, interfaces HID y almacenamiento SCSI BOT/UAS según capacidad, probar con dispositivos físicos y USB en QEMU. No confundir detección PCI de controlador con acceso funcional a pendrives.
- **Audio y vídeo:** controlador de audio real (p. ej. Intel HDA / AC97), mezclado/buffering, audio de usuario, codecs e integración multimedia con aceleración/optimización del framebuffer; no confundir una captura a 1 FPS con reproducción funcional.
- **Navegador:** probar instalación y arranque Chromium/Firefox en X real con HTTPS, CSS y JavaScript en páginas actuales; arreglar incompatibilidades syscall/permisos y conectividad. El visor HTTP HTML integrado no ejecuta JS ni CSS completo.
- **Rendimiento:** perfilar la actualización de `A_XSCR` (Xvfb backbuffer actualmente repintado aproximadamente cada tres ticks aunque nada cambie), evitar recomposición completa innecesaria, evaluar regiones sucias basadas en cambios reales y entrada de teclado/ratón sin retardos.
- **NTP:** verificar UDP/123 en QEMU NAT y hardware, analizar errores del DNS compartido y compensación de latencia/RTT. Considerar autenticación NTS para seguridad; no cambiar CMOS silenciosamente.
- **Seguridad:** usuarios/UID/GID, permisos verdaderos, sandboxing de apps, separación kernel/usuario efectiva, actualizaciones firmadas y control de acceso a dispositivos.
- **Ofimática y aplicaciones:** pruebas gráficas reales de LibreOffice y suite de programas sobre x86_64; mejorar lanzador, instalación y desinstalación, asociaciones MIME.
- **Vortex Saber:** pruebas de interfaz de las dos materias nuevas, ilustraciones con licencias adecuadas, ejercicios adaptativos, perfiles educativos y recuperación segura de progreso.
- **Compatibilidad/estabilidad:** regresiones históricas del kernel64, bugs de memoria en escenarios de RAM elevados y controladores pendientes, instalación/arranque físico BIOS+UEFI.

## 8. Instrucción para retomar en un nuevo chat

**Continuar el desarrollo de Vortex OS en `dreyko24/SO`, específicamente rama `vortex/window-management-20261114` y PR #9, leyendo primero este documento y `VORTEX_ESTADO.md`. NO pedir otra vez el repositorio ni autorización fase por fase. Revisar los últimos commits de GitHub, ejecutar o verificar la CI, corregir cualquier fallo, preservar avances previos y seguir implementando hardware, rendimiento, navegador real, seguridad, multimedia y educación. Trabajar con cambios concretos guardados; distinguir lo probado de lo no probado. Antes de que se agote el contexto, actualizar este archivo y facilitar un nuevo informe de traspaso.**

## 9. Adenda de desarrollo (misma sesión, posterior al informe inicial)

El PR #9 ha avanzado más allá del primer conjunto de commits:

- `kernel/browser_launch.h`: la construcción segura del comando de Chromium es ahora una función autocontenida y comprobable sin GUI; preserva URLs HTTP(S), convierte consultas `kng://buscar?q=` a Google y rechaza metacaracteres y buffers insuficientes.
- `tools/browser_launch_test.c`: diez comprobaciones unitarias **ejecutadas localmente y aprobadas (10/10)** mediante `gcc -std=c11 -O2 -Wall -Wextra -Werror ...`. Junto a las nueve del parser NTP, son diecinueve comprobaciones verificadas localmente. Esto NO es un resultado de la ISO o del navegador real.
- `tools/rootfs-es/usr/local/bin/vortex-chromium`: lanzador explícito de Chromium con las opciones de compatibilidad con root/Xvfb; `tools/mkdebian.sh` garantiza permiso ejecutable al construir Debian. **Riesgo de seguridad grave conocido:** usa `--no-sandbox` porque el sistema no dispone de sandbox para Chromium; no navegar con contraseñas ni datos confidenciales hasta implementar aislamiento real.
- `kernel/gui.c`: Chromium usa el nuevo lanzador desde el botón del navegador y el catálogo de aplicaciones, y conserva la URL o consulta actual.
- `kernel64/sys_extra.inc`: se cambió el reloj POSIX de 64 bits (CLOCK_REALTIME, mtime de archivos) para usar el NTP sincronizado cuando existe; respeta los cambios explícitos por `clock_settime`. Aún falta validación de syscalls con programas reales y hardware.
- `kernel/net.c`: comandos de terminal `fecha`, `hora` y `hora sincronizar`; el informe de hora muestra Perú UTC-5 y procedencia NTP/RTC. `tools/ntp_stack_test.c` añade pruebas del comando y reintento.
- `.github/workflows/vortex-integracion.yml`: incorpora pruebas del constructor de comandos, validación de sintaxis de scripts shell, red/NTP, ventanas, calendario y compilación de 32/64 bits; NO se ha podido confirmar ejecución exitosa del workflow.
- Consultar los commits actuales de la rama, no un SHA estático de este archivo: GitHub confirma que el PR #9 suma numerosos commits y que está por delante de `main`.

**Pendientes inmediatos adicionales:** comprobar que `mkdebian.sh` empaqueta y ejecuta el lanzador, que Chromium abre Google de verdad con CSS/JS/HTTPS en Xvfb, que `sys_clock_gettime` recibe UTC de NTP tras respuesta real, y que el flujo CI completo compila. No fusionar sin esa evidencia.
