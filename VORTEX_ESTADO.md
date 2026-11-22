# Vortex OS — documentación técnica única y estado vigente

**Proyecto oficial:** Vortex OS · **Repositorio:** https://github.com/dreyko24/SO · **Rama activa:** `main`
**Actualización de esta consolidación:** 14 de noviembre de 2026, hora de Perú.
**Propósito:** este es el **único documento de referencia del estado, historial técnico, pruebas y continuación**. Se consolidaron aquí los informes `TRASPASO_kng_v17–v24`, `TRASPASO_vortex_v17–v24`, la continuidad previa, el estado antiguo y la auditoría. Las guías `README.md` (uso), `AGENTS.md` (reglas para colaboradores) y las instrucciones específicas de VMware/gráficos no representan otras versiones del sistema.

> **Regla esencial:** Vortex OS ya existe. No iniciar otro proyecto, sustituir el kernel por Linux, copiar un `kernel/gui.c` de una rama vieja, ni borrar componentes comprobados. Leer el HEAD de `main` y sus tests antes de cada intervención. Este documento no constituye un certificado de que todo el sistema funcione en hardware físico.

## 1. Arquitectura existente: referencia para supervisión de código

| Subsistema | Fuente de verdad | Función actual / límites |
| --- | --- | --- |
| Núcleo x86 de 32 bits | `kernel/boot.S`, `kernel/kernel.c`, `kernel/vortex.h`, `kernel/Makefile` | Kernel propio Multiboot, GDT/IDT, interrupciones y temporizador, PS/2, memoria física/virtual, heap, procesos y syscalls propios. |
| Núcleo x86_64 | `kernel64/boot64.S`, `kernel64/kernel64.c`, `kernel64/proc64.c`, `kernel64/sys_*.inc` | Modo largo, memoria y procesos en ring 3, compatibilidad parcial con ABI Linux y ejecutables ELF estáticos/dinámicos; pueden ejecutarse aplicaciones del espacio Debian 12 sin utilizar kernel Linux. No equivale a compatibilidad completa con cualquier aplicación Linux. |
| Escritorio Vortex Flow | `kernel/gui.c`, `kernel/gfx.c`, `kernel/font.h` | Ventanas independientes (hasta 24 mediante asignación dinámica), minimizar/maximizar/restaurar/cerrar, pantalla completa, escritorios, dock lateral con un único icono «Aplicaciones», búsqueda y categorías. Regiones sucias, oclusión y redibujado reducido. Render por CPU, sin aceleración 3D general. |
| Almacenamiento | `kernel/fs.c`, `kernel/disk.c` | Archivos, carpetas, copias/movimientos y persistencia VortexFS (firma histórica KNGFS), con controladores y configuraciones según hardware. No formatear discos con datos sin aprobación específica y copia de seguridad. |
| Red y hora | `kernel/net.c`, `kernel/net.h`, `kernel/ntp.h`, `kernel64/sys_extra.inc` | PCI, DHCP, ARP, IPv4, ICMP, DNS, UDP, TCP y HTTP nativo básico; adaptadores e1000/RTL8139/virtio/PCnet y otros según controlador. Reloj RTC con SNTP/NTP opcional, Perú UTC−5 y comandos `fecha`, `hora`, `hora sincronizar`. NTP sin autenticación criptográfica. |
| Navegador web | `kernel/html.c`, `kernel/browser_launch.h`, `tools/vortex-proxy.py`, scripts del rootfs Debian | Visor HTML/HTTP limitado y proxy externo opcional; lanzador Chromium para entorno Debian/X. Un motor web moderno y seguro **no está demostrado**: no hay TLS nativo en el visor, CSS/JS completos ni aislamiento adecuado; el wrapper Chromium utiliza `--no-sandbox`. No utilizar credenciales personales allí. |
| Audio | `kernel/audio.c` y GUI multimedia | WAV PCM 8/16 bits mono/estéreo con controladores AC’97 y Sound Blaster 16 emulados. Sin MP3/OGG/FLAC nativos ni compatibilidad general HDA verificada. |
| Vídeo e imágenes | `kernel/video.c`, `kernel/image.c`, `kernel/jpeg.c`, `kernel/gfx.c`, `kernel/media_demo.c` | PNG, JPEG básico, BMP y PPM; AVI RGB 24/32 bits sin compresión, controles y escalado incremental sin división por píxel. Muestra propia de 75 cuadros a 25 FPS durante 3 s; la cadencia de la muestra no demuestra FPS garantizados en hardware. Sin H.264/HEVC/MP4/WebM nativos. |
| USB | `kernel/usb_storage.c`, `kernel/usb_xhci.c` | UHCI con BOT/SCSI Mass Storage e importación FAT32 y exFAT de solo lectura en `/Dispositivos/Unidad-USB`. xHCI con detección y lectura de descriptores en puertos raíz. Pruebas en QEMU. |
| Vortex Saber | `kernel/encyclopedia.h`, `kernel/saber_progress.h`, `kernel/gui.c` | Enciclopedia educativa de 31 artículos, preguntas/juegos, resultados y registro de progreso. Incluye migración desde resultados previos de cuatro/seis materias. Mantener esta versión: no regresar a la enciclopedia básica antigua. |
| Herramientas/ISO/CI | `tools/build-iso.sh`, `tools/verificar-vortex.sh`, `tools/hostgui.c`, `tests/`, `.github/workflows/` | Construcción de ambos kernels, ISO única con modos 32/64 y Debian opcional, ejecución QEMU, pruebas nativas y trabajos de integración. Los tests host no sustituyen pruebas de hardware. |

### Reglas de diseño y compatibilidad

- **Un solo producto: Vortex OS.** Las carpetas `kernel/` y `kernel64/` son arquitecturas de un mismo SO, no versiones ni proyectos distintos.
- Nombres históricos `kng` (identificadores internos, firma del FS, símbolos antiguos) pueden persistir por compatibilidad binaria; evitar renombrarlos sin pruebas.
- La capa de ejecutables Linux y Debian es un **espacio de usuario opcional**, no un reemplazo del núcleo Vortex.
- Los instaladores, demos y menús no prueban por sí solos funciones reales: documentar «implementado», «probado en host», «probado en QEMU» y «probado en hardware» por separado.

## 2. Evolución histórica consolidada (una única línea de desarrollo)

Esta cronología integra ambas series `kng` y `vortex`; gran parte de los documentos eran el mismo texto con cambio de marca. **Son antecedentes, no ramas activas ni requisitos de restauración.**

| Antecedente | Aportes acumulados relevantes |
| --- | --- |
| v17 | Arranque Multiboot de 32 bits; GDT/TSS, ring 3, IDT/PIC/PIT, PS/2, RTC, memoria, ELF32 y syscalls; framebuffer, escritorio básico, terminal, archivos, ajustes y navegador inicial. En aquella fecha aún no se había validado el arranque real en QEMU. |
| v18 | Primer núcleo x86_64 separado, modo largo y código de GUI compartido; PCI e1000/RTL8139, ARP/DHCP/DNS/TCP/HTTP asíncronos, visor HTML y herramientas de ISO dual; pruebas host de red. Sin TLS. |
| v19 | Mejoras de antialiasing, degradados y fondo; búsqueda local/web configurable; Notas, Calculadora y Snake. |
| v20/v21 | Render parcial, sombreado barato, oclusión, glyph rápido y framebuffer write-combining (PAT); proxy de búsqueda HTTPS desde el anfitrión, widgets, red PCnet/virtio, ATA + persistencia KNGFS, comandos de archivos y monitor. La mejora de velocidad de aquel momento no se midió en hardware. |
| v22 | Teclados US/Latam/España, acentos/ñ, UTF-8 en aplicaciones, selección de teclado persistente y pantalla de error fatal. |
| v23 | Programas de usuario reales del kernel 64: ELF/libc estática y dinámica, fork/exec/wait, pthread/futex, pipes y sistema de archivos, cpio de rootfs, pruebas QEMU. Los controladores gráficos y sockets del espacio Linux todavía estaban incompletos en ese momento. |
| v24 | Avances de memoria y syscalls 64-bit (red, señales, mmap, epoll, archivos y `/proc`/`/dev`); Debian 12, apt/dpkg, Xvfb con entrada PS/2→XTEST, conversión headless con LibreOffice, discos IDE/SATA, persistencia, controladores de red, GRUB BIOS/UEFI y configuración gráfica. En el informe original figuraba un error de memoria tras `probar-ram` con 6 GB seguido de un programa dinámico; **es una alerta histórica pendiente de regresión, no una afirmación de fallo actual**. |
| Vortex OS integrado (noviembre 2026) | Identidad Vortex, launcher único, ventanas completas y 24 instancias con memoria dinámica, NTP/RTC Perú, Vortex Saber de 31 artículos, AC’97/SB16, AVI/PNG/JPEG, USB UHCI FAT32/exFAT, escalado optimizado, vídeo de prueba 25 FPS y scripts QEMU/ISO. Avances de ramas antiguas incorporados en `main`. |

### Estado de integración de ramas

- La rama histórica `cambio-nombre-vortex-os` y `vortex/integracion-total-20261114` se comprobaron como **antecesoras** del `main` del 14 de noviembre de 2026: contienen desarrollo avanzado ya incorporado.
- Mejoras de ventanas, UI, NTP, navegador, escalado, USB y multimedia se incorporaron mediante PR anteriores, incluidos #9, #10 y #13–#17, algunos con **squash**. Una rama antigua puede aparecer «divergente/ahead» aun si su funcionalidad ya está en `main`. Comparar **código y pruebas**, no solo contador de commits.
- `feat/unified-app-launcher` y `vortex-desktop-window-manager` son precedentes antiguos del lanzador y el gestor; el escritorio actual tiene aplicaciones por categorías y funciones más avanzadas. Nunca sustituir `kernel/gui.c` actual por esos archivos antiguos.
- No mantener varias carpetas de «Vortex OS v17/v18/...» ni otra implementación paralela. Las ramas y etiquetas de Git son **historial**, no versiones distintas para el usuario.

## 3. Verificación: qué se sabe y qué falta

**Comprobaciones históricas en GitHub Actions (QEMU o host, no hardware físico):**

- Compilación y arranque de kernels de 32 y 64 bits, ISO única BIOS/UEFI según trabajo correspondiente, framebuffer e interfaz. Consultar `arranque-qemu` y `vortex-iso-unica`.
- Simulador del escritorio, operaciones de ventanas, atajos, memoria y render: `vortex-escritorio-host` y `vortex-unificacion-integracion`. Incluye pruebas con AddressSanitizer y regresión del escalado.
- Red y reloj: suites de TCP/UDP/DNS y verificaciones de formato y correlación SNTP/NTP; en el historial figuran 9/9 pruebas de parser NTP y 10/10 del constructor seguro de comandos para navegador.
- USB: pruebas QEMU de lectura FAT32 y exFAT, lectura de un PNG y cambio de fondo (`vortex-usb-mass-storage`).
- Multimedia: sonido WAV AC’97/SB16 y AVI en QEMU (`vortex-reproductores`, `vortex-audio-sb16`). Prueba host del vídeo de muestra con 75 cuadros a 25 FPS.
- Educación: pruebas de progreso serializado y migración, además de interfaz Vortex Saber. La persistencia **tras apagar y arrancar con un disco físico** requiere validación independiente.

**Estas evidencias son del commit ensayado en cada workflow, no garantía sobre todos los equipos ni sobre la última revisión sin pruebas.** Si el `main` cambia, revisar los resultados asociados a su SHA y repetir lo necesario.

### Compilación y prueba

En Ubuntu/WSL con compiladores GNU, utilidades GRUB, xorriso, mtools, Python y QEMU cuando corresponda:

```bash
git clone https://github.com/dreyko24/SO.git
cd SO
git switch main
git pull --ff-only
bash tools/verificar-vortex.sh
bash tools/build-iso.sh
# Imagen de desarrollo: out/VortexOS.iso
bash tools/probar-vortex.sh out/VortexOS.iso
```

En Windows con QEMU instalado: `Probar-VortexOS.cmd "out\VortexOS.iso"`. Los lanzadores aceptan una imagen FAT32/exFAT opcional como **segundo argumento** para USB virtual solo lectura. No conectan ni formatean directamente discos físicos. Para construir el espacio Debian opcional: `VORTEX_DEBIAN=1 bash tools/build-iso.sh`; exige dependencias y recursos adicionales. Antes de utilizar la ISO, comparar la suma SHA-256 generada.

### Correcciones de interfaz y dispositivos: noviembre de 2026

Se han aplicado modificaciones **directamente en `main`** a partir de las pruebas del usuario en VMware. El commit de aplicación por sí solo no equivale a prueba completa en máquina virtual o USB físico; para dar por validada una corrección deben terminar correctamente las ejecuciones correspondientes de GitHub Actions.

- **Barra superior:** posiciones de ventanas minimizadas calculadas para todas las instancias visibles del espacio de trabajo, con geometría compartida entre pintura y clic; regresión de tres ventanas en `tools/hostgui.c`.
- **Vortex Saber:** la acción del botón visible «Siguiente» corresponde ahora a su coordenada real y marca el artículo como leído; esquemas específicos para células, ADN y ciclo del agua; juego de memoria con iconos, progreso e indicador de tiempo. Siguen pendientes contenidos más profundos y actividades más variadas.
- **Tema claro:** el motor HTML adopta los colores de texto del tema claro (antes usaba texto casi blanco fijo), y los botones y superficies mejoran contraste.
- **USB UHCI:** tolerancia a NAK no fatal, reintentos SCSI para memorias lentas, mensajes de error por etapa y búsqueda manual forzada que renueva la instantánea. **El pendrive Kingston físico del usuario todavía no se ha validado tras estos cambios** y no existe soporte EHCI/xHCI ni acceso USB universal.
- **Web Debian:** `tools/mkdebian.sh` ahora incluye Firefox ESR, Xvfb y herramientas X11 en nuevas ISOs Debian; el lanzador detecta fallos de arranque de X y aplica ajustes provisionales para Firefox. **No afirmar que el navegador grafico en VMware ya funciona:** solo una prueba real con ISO nueva puede confirmarlo. Sandbox de navegador y aislamiento seguros siguen pendientes.
- **Persistencia:** los cambios de `apt` hechos durante una sesión no persisten porque `kernel64/sys_fs.inc` marca la mayoría de las rutas Debian como no persistentes. Incluir Firefox en la ISO evita reinstalar ese paquete, pero **no** equivale a conservar cualquier otra instalación. La persistencia general de Debian sigue pendiente de diseño y prueba.

### Correcciones comprobadas tras las pruebas del 15 de noviembre

Se mantuvo **un único Vortex OS sobre `main`**, sin crear nuevas ramas ni variantes del sistema. Las capturas de evidencia de estas correcciones proceden del artefacto del workflow `vortex-escritorio-host` y no de imágenes históricas de `docs/`.

- **Tema claro:** paleta y fondo independiente del oscuro, controles con mayor contraste, HTML con colores según el tema. Captura nueva `29_modo_claro.png`.
- **Vortex Saber (31 artículos):** un diagrama vectorial distinto y contextualizado por artículo, no por categoría; ejemplo de Independencia del Perú con la bandera y 1821. El test `tools/hostgui.c` recorre los 31 y comprueba que las imágenes no se repiten. **Son diagramas originales, no fotografías reales ni una biblioteca visual definitiva.**
- **Actividad interactiva:** `Mision Cosmos` con nave controlada por teclado/ratón, cuatro cuerpos celestes, barra de objetivos, tiempo y logro persistente en progreso Saber. Se mantiene también el juego de memoria; falta una biblioteca de juegos moderna más amplia.
- **USB:** interfaz `Mis dispositivos USB` con nombre leído del descriptor, formato, capacidad, elementos y botón `Abrir archivos USB`. Autoapertura del explorador al montar, rescaneo manual, driver UHCI con acceso a memorias tras hub USB 1.1. La suite `vortex-usb-mass-storage` comprobó FAT32, exFAT y FAT32 detrás de un hub en QEMU (`run 43667887328`). **No se ha comprobado Kingston real en VMware** ni se han implementado EHCI/xHCI o NTFS.
- **Navegador:** Debian de 64 bits incluye Firefox ESR y Xvfb; Xvfb y `vortex-xinput` sí se ejecutaron bajo QEMU. No obstante, en la prueba `run 43668316179`, `xterm` finalizó con código 32 y `firefox-esr` con 255: **la navegación gráfica no funciona aún**. El wrapper ahora registra `/tmp/vortex-web.log` y deja el error visible para continuar la depuración; no confundir instalación con funcionamiento.
- **ISO única:** la publicación `iso-latest` se actualizó en el commit `2bd6db4`, después de arrancar kernels gráficos de 64 y 32 bits en QEMU. Cualquier commit posterior que cambie el sistema requiere nueva ejecución del workflow antes de ofrecer su ISO como validada. Las instalaciones adicionales `apt` de una sesión siguen sin persistencia general.

### Diagnóstico de compatibilidad Linux y regresión de arranque — 19 de noviembre de 2026

Todo este trabajo se hizo sobre el único `main` de Vortex OS, conservando los núcleos propios y sin importar código Linux:

- **Causa confirmada de una regresión de arranque:** GitHub Actions pasó a GCC 15.2 y el ELF64 generado contenía `SHRX` (BMI2) en `kmain64`, RIP `0x104d93`, para calcular `heap_total / 1024`. La CPU virtual de QEMU en la prueba no admite BMI2; producía `#UD` después de inicializar IDT/PIC/PIT. Desensamblado documentado en `arranque-qemu`, ejecución `44553492334`. No se deduce de esto un fallo del heap.
- **Corrección de arquitectura:** `kernel64/Makefile` fija `-march=x86-64 -mtune=generic -mno-bmi2` para el kernel y los programas propios. Commit `5660642c`. La suite `vortex-usb-mass-storage`, ejecución `44554029083`, **aprobó** después de la corrección: arranque gráfico del kernel 64 en QEMU, lectura FAT32, exFAT y FAT32 tras hub. Las suites de escritorio, integración, reproductores y SB16 también aprobaron sobre ese commit. Esto **no equivale** a validar todo el ABI Linux ni USB VMware.
- **Prueba de arranque reforzada:** `.github/workflows/qemu.yml` imprime el código máquina cercano al RIP y falla si el kernel emite `[PANIC]`, incluso si el banner de Vortex salió correctamente. El flujo de la ISO ahora vigila también cambios del `kernel64/` (antes solo algunos archivos del GUI y drivers), y no publica si el registro gráfico de 64 bits presenta `[PANIC]`. Commits `73c42f1` y `4d5e23c`.
- **Prioridad Firefox:** el informe válido de `run 43669257211` mostró Xvfb y `vortex-xinput` arrancando; `xterm` terminó con código `32` y `errno 2`; `firefox-esr` con código `255`. La capa de 64 bits no contiene una implementación real de pseudoterminales `/dev/ptmx` y `/dev/pts`, limitación que hay que investigar para `xterm`; **no se puede atribuir aún el fallo de Firefox a esa causa**. El workflow de ISO, commit `f30c550`, prueba además `xdpyinfo` (cliente X11 sin PTY) y ejecuta `rastro 1` antes de Firefox para conocer las llamadas al sistema que faltan. La ejecución de esa batería completa requiere que la nueva ISO supere primero el arranque.
- **Aplicaciones reales:** la presencia de Firefox ESR en Debian no demuestra navegación funcional, y tampoco existe prueba aprobada de LibreOffice Writer/Calc/Impress ni VLC gráfico ejecutándose dentro de Vortex. No hacer afirmaciones de compatibilidad sobre estas aplicaciones sin logs y pruebas de interfaz/función.
- **Licencias:** no se incorporó código del kernel Linux en estas correcciones. Antes de considerar fragmentos de kernel Linux hay que auditar licencia GPL-2.0 y dependencias; el repositorio no muestra un archivo `LICENSE` o `COPYING` en el árbol consultado. Priorizar las especificaciones de ABI/POSIX y código propio cuando sea viable.
- **ISO:** la release `iso-latest` publicada anteriormente a esta investigación no tiene la corrección BMI2; no dar por actualizada esa ISO hasta que `vortex-iso-unica` termine y publique el commit nuevo.

### Progreso del 20 de noviembre de 2026 — Firefox/ABI y cadena de compilacion

- **Rama unica:** `main`, sin kernels Linux adicionales ni ramas temporales. El kernel Vortex x86_64 sigue gestionando memoria, procesos, archivos, red y la ABI.
- **Firefox ESR:** la implementación de `/proc/self/exe` conserva ahora el **destino real del archivo ejecutable**, mientras `AT_EXECFN` conserva la ruta original, para resolver bibliotecas relativas al ejecutable. Desapareció `Couldn't load XPCOM` en la prueba `44732499272`; esa misma prueba encontró el error `wasm_rt_syscall_set_segue_base error: Invalid argument`.
- **GS por hilo:** `arch_prctl` soporta `ARCH_SET_GS`/`ARCH_GET_GS` además de FS, con MSR `IA32_GS_BASE`, conservación por tarea al cambiar de contexto, copia en clone y puesta a cero en exec. Es una implementación propia según el contrato público de la ABI x86-64; no se copió código del kernel Linux.
- **Resultado ISO QEMU**: la ejecución **`44734584733` terminó OK** para arranque gráfico 32/64, Xvfb, `vortex-xinput`/XTEST y cliente X11 `xdpyinfo`; aparecen procesos secundarios de Firefox `Web Content`. **No hay aún validación de navegación por Internet, renderizado correcto ni seguridad del navegador**, y xterm sigue sin PTY. La ISO publicada para ese resultado corresponde a `1c3df78`; comprobar el SHA más reciente de `iso-latest` antes de descargar.
- **ABI de glibc:** el runner Ubuntu 26.04 incorporó glibc 2.43 con ELF marcados `x86-64-v3`; en QEMU básico se produjo `SIGILL` por instrucciones AVX del runtime. El generador `tools/mkrootfs.py` ahora rechaza esa ISA. En Ubuntu 24.04/glibc 2.39, la ejecución `44734584640` ejecutó correctamente los programas de prueba estáticos, PIE y dinámicos y 3 variantes de fork/hilos/tuberías: todas las pruebas `multi` registraron `6/6`.
- **Prueba GS y binario obsoleto:** la verificación GS inicialmente no se ejecutó en `44734584640` porque el `user/hola.elf` **versionado** prevalecía sobre el fuente cambiado debido a fechas de checkout. `kernel64/Makefile` ahora fuerza la compilación del ELF antes de incorporarlo al kernel; la prueba vuelve a validarse mediante `arranque-qemu`. No confundir el fallo del test obsoleto con una refutación de la implementación GS.
- **Cadena de CI:** los trabajos que construyen un rootfs de usuario o una ISO se fijan a `ubuntu-24.04` y compilan con `-march=x86-64`. Los trabajos con Ubuntu 26.04 habían fallado al rechazar correctamente los ELF `x86-64-v3`, no por una regresión USB/audio/GUI demostrada. La nueva revisión de CI debe verificar las suites afectadas antes de marcar todo como aprobado.

### Evidencia visual de Firefox — 20 de noviembre de 2026

- La ejecución `vortex-iso-unica` **44739481838**, commit `6d80b9d`, terminó correctamente y publicó ISO tras comprobar el arranque gráfico de ambos núcleos, Xvfb, XTEST y `xdpyinfo`. Se recuperó la captura XWD real de la pantalla Xvfb (`vortex-web-smoke`, artefacto `14630496549`).
- **Análisis de píxeles del PNG real:** 1280×800, exactamente **1.024.000 píxeles negros, cero píxeles de otro color**. `xwininfo -root -tree` solo identificó un cliente `Firefox` de **10×10 píxeles**, y no apareció `Web Content` en las tres muestras. El título de ventana NO equivale a una interfaz usable. **Firefox sigue sin renderizar** en esa prueba, sin HTTPS ni navegación validada.
- La prueba XWD ahora analiza contenido real y avisa cuando el framebuffer está negro o tiene menos del 1% de píxeles visibles; no debe confundirse el éxito de construir la ISO con éxito de Firefox. No usar el navegador para credenciales.
- Cambios incrementales en `main`: `a9f95e9` filtra del rastreo ordinario el `EAGAIN` esperado en IPC; `4f0138f` toma varias muestras de Firefox; `f60a9f8` y `d8960a7` corrigen la respuesta POSIX de `F_GETFL` y `F_SETFL` con pruebas de archivos/tuberías. Estos avances conservan el kernel Vortex.

### Plazos de futex en la ABI de 64 bits — 20 de noviembre de 2026

- El kernel Vortex interpreta `FUTEX_WAIT` como una espera relativa y `FUTEX_WAIT_BITSET` como un plazo absoluto según el reloj monotónico o `FUTEX_CLOCK_REALTIME`. Valida `timespec` y usa el contador de 64 bits del PIT para conservar los plazos largos. La resolución sigue siendo de 10 ms.
- La regresión en `kernel64/user/multi.c` solicita esperas de 50 ms desde ring 3. En `ea664b1`, la espera absoluta monotónica duró 1.160 ms y la de tiempo real retornó de inmediato. Con la corrección, las variantes ELF estática, PIE estática y dinámica completaron `7/7` en QEMU. La prueba se ejecuta en `arranque-qemu`.
- `clock_gettime`, `gettimeofday` y los plazos de `FUTEX_CLOCK_REALTIME` reciben los segundos y su fracción de la misma muestra. La prueba de red sincroniza NTP entre dos segundos del PIT y comprueba ambas transiciones para reproducir el desfase anterior. La regresión en ring 3 comprueba que un `timespec` inválido reciba `EINVAL` incluso si la palabra futex ya cambió.
- Con unos 55 segundos de espera, el kernel de referencia y el corregido dibujaron una ventana de Firefox de 1152×720 con los mismos píxeles de pestaña vacía.

### Firefox ESR y renderizado HTTP local en QEMU — 20 de noviembre de 2026

- En la ejecución local del paso de `vortex-iso-unica`, Firefox ESR solicitó la página de prueba al anfitrión por HTTP y Xvfb capturó su texto y fondo. La captura de 1280×800 contiene 656.850 píxeles del color de la página.
- `vortex-iso-unica` sirve una página de prueba desde el anfitrión y registra la respuesta HTTP `200`, el título de su ventana X11 y el color de fondo en la captura. `tests/test_xwd2png.py` comprueba que una pestaña vacía o un solo píxel del color esperado queden por debajo del umbral.
- En una prueba local adicional sobre `9a4e410`, `curl` y Firefox ESR negociaron TLS 1.3 con la CA de prueba. El servidor recibió ambos `GET` y Xvfb capturó el contenido HTTPS de Firefox, con 658.587 píxeles del color de la página en 1280×800.
- La primera prueba con `example.com` resolvió DNS tras DHCP, pero el puerto 443 no respondió en el invitado ni en el contenedor anfitrión. La prueba posterior de Google con la red NAT de QEMU sí mostró resultados públicos por HTTPS. El sandbox del navegador y el funcionamiento en VMware siguen pendientes de validación.

### Recuperación de Xvfb en QEMU — 20 de noviembre de 2026

- `pantalla` y `abrir` comprueban los procesos de Xvfb y del puente XTEST. Si Xvfb termina, la siguiente solicitud arranca otro servidor y sustituye su puente. Si solo termina el puente, el kernel relanza ese proceso con el servidor activo.
- `abrir-x` espera a que el socket de X11 acepte la conexión. Esto cubre la carrera en el escritorio en la que un socket de Xvfb anterior seguía presente y `xmessage` terminaba con `Can't open display` antes de que el servidor nuevo estuviera listo.
- La regresión de `vortex-iso-unica` consulta Xvfb con `xdpyinfo`, lo detiene en la VM y vuelve a consultar al servidor a través de `abrir`. Exige dos conexiones XTEST y comprueba que repetir `pantalla` reutilice el servidor activo.
- El compositor comprueba los píxeles del área visible antes de mostrar el aviso de pantalla vacía. La regresión de escritorio conserva el aviso para un XWD negro y comprueba que una ventana pequeña permanezca visible.

### Búsquedas públicas desde Firefox — 20 de noviembre de 2026

- Firefox ESR ejecutado en el kernel Vortex de 64 bits mostró resultados reales de Google para `Vortex OS` sin cuenta, primero con el proxy del entorno y después con la red NAT de QEMU en GitHub Actions. En esa segunda prueba, `curl` recibió HTTP 200 de Google y de la portada de YouTube. La segunda ventana de Firefox quedó en 1×1 y la captura seguía mostrando Google. [Diagnóstico y capturas](https://github.com/dreyko24/SO/actions/runs/44768622775).
- Se corrigió el recorte de búsquedas largas entre el navegador nativo y el lanzador de Firefox. `tools/hostgui.c` verifica la URL completa recibida por el lanzador. `tools/ci/firefox-internet.txt` y `tools/ci/firefox-youtube.txt` abren un sitio por VM, registran la respuesta HTTP y capturan lo que Firefox dibuja.
- La VM dedicada a YouTube dibujó la barra y la consulta `sistema solar` en Firefox tras 190 segundos. La lista de resultados seguía vacía en esa captura. La traza registró tráfico HTTPS desde Firefox a YouTube y sus recursos. [Diagnóstico de YouTube](https://github.com/dreyko24/SO/actions/runs/44770582689).
- En la repetición con más tiempo, Firefox mostró títulos, miniaturas y filtros para `sistema solar` en YouTube sin cuenta. La captura se tomó aproximadamente siete minutos después de arrancar Firefox en QEMU. Queda pendiente medir la reproducción de video. [Capturas y registro de YouTube](https://github.com/dreyko24/SO/actions/runs/44771816055).
- Tras un cierre de pestaña, Firefox registró que le faltaba un descriptor entre procesos. La ABI de Linux para sockets AF_UNIX entregaba descriptores antes de sus datos y limitaba cada recepción a ocho. La corrección en el kernel de 64 bits conserva los lotes junto a sus datos y usa la capacidad del búfer de control. `nettest` reproduce el fallo y verifica en QEMU dos lotes ordenados, uno de 12 descriptores, y la lectura con `MSG_PEEK`.
- Con el kernel corregido, una consulta escrita desde el escritorio de Vortex volvió a mostrar resultados públicos en Firefox ESR. Los diagnósticos con la red NAT de QEMU en `bb4c51b` mostraron resultados de Google y los títulos y miniaturas de YouTube sin cuenta. La ISO de esa revisión pasó las pruebas de arranque gráfico en ambos modos y la comprobación SHA-256 del archivo publicado. [Google](https://github.com/dreyko24/SO/actions/runs/44773217332) · [YouTube](https://github.com/dreyko24/SO/actions/runs/44773222674) · [ISO](https://github.com/dreyko24/SO/releases/tag/iso-latest).

### Tienda, USB y pruebas de Firefox — 20 de noviembre de 2026

- **Tienda.** El escritorio abre el catálogo Debian desde Actividades → Tienda, con búsqueda, categorías e instalación y apertura de programas reales. El progreso y los errores de APT aparecen en la terminal. Las instalaciones se conservan durante la sesión en RAM. La prueba de escritorio comprueba el acceso, el filtro, los comandos de instalación y apertura y que el catálogo visible no se repinte cuando su estado sigue igual. En QEMU gráfico se instaló mtPaint 3.40 pulsando **Instalar** y luego se abrió el editor con **Abrir**. El diagnóstico de terminal también instaló y abrió Audacious 4.2, con salida ALSA pendiente. La instalación conjunta completó al reintentar tras un fallo de `dpkg` al actualizar una biblioteca.
- **USB UHCI.** Un dispositivo HID ya no impide leer el pendrive que le sigue en otro puerto raíz o en un hub de un nivel. La prueba QEMU de 32 y 64 bits verifica la detección de un teclado por su descriptor, la conexión del pendrive FAT32, su retirada y una segunda importación de solo lectura. Sigue pendiente el soporte de entrada HID, EHCI/xHCI y el pendrive físico.
- **Firefox.** `tools/ci/firefox-funciones.sh` comprueba JavaScript, los valores de `sessionStorage` y `localStorage` tras navegar dentro del mismo origen, y una respuesta real de Fetch. También comprueba los píxeles de dos fotogramas VP8 y la reproducción hasta el final de un WebM de dos segundos sin audio. Firefox ESR completó la prueba dos veces en QEMU. El flujo de la ISO ejecuta la misma prueba.

### xHCI, descargas y corrección de instalación — 22 de noviembre de 2026

- **xHCI.** El controlador propio enumera puertos raíz y lee los descriptores de dispositivo y de configuración. En QEMU, ambos kernels detectaron el teclado HID USB 2 y la unidad de clase Mass Storage USB 3. La prueba comprueba la retirada del teclado y la lectura de su descriptor tras reconectarlo. Multimedia → USB actualiza el estado al cambiar la detección. El acceso a los archivos de la unidad, la entrada HID, los hubs xHCI y el hardware físico requieren pruebas y trabajo adicionales.
- **Firefox.** En QEMU de 64 bits, el navegador guardó en la ruta de descarga un adjunto HTTP de 16.392 bytes llamado `vortex.bin`. La prueba compara todos los bytes y la suma SHA-256 con el archivo enviado. Usa un perfil temporal con guardado automático. El diálogo interactivo queda pendiente.
- **Tienda y ABI Linux.** Un mapeo privado ahora mantiene el archivo abierto a nivel del kernel mientras alguna región lo referencia. `memtest` comprueba la lectura diferida tras cerrar el descriptor, reemplazar su ruta y desmontar las regiones del proceso padre. Pasó con 86 comprobaciones. En el mismo entorno Debian, la actualización de `libpcre2-8-0` que terminaba con una excepción de página pasó a terminar con código 0.
- **Compositor.** La ventana de X usa una instantánea compartida para localizar cambios en el framebuffer de Xvfb. La prueba del escritorio midió cero píxeles repintados con una imagen estática y un píxel repintado ante un cambio aislado. También verifica que vuelva el aviso si el framebuffer queda negro.

## 4. Limitaciones y trabajo de ingeniería que continúa

1. **USB real:** acceso a memorias, entrada HID y hubs por xHCI, controlador EHCI, recuperación al desconectar, escritura y montaje, y pruebas con varios controladores físicos y pendrives.
2. **Multimedia fluida:** medir FPS de vídeo real y latencias, optimizar cómputo y framebuffer, codecs comprimidos, hardware de sonido HDA/ES1371, mezcla/buffering. El AVI sin comprimir es una muestra limitada.
3. **Navegador moderno:** ejecución real de Chromium con HTTPS/CSS/JS, compatibilidad X y syscalls, usuarios y aislamiento; eliminar `--no-sandbox` antes de permitir información sensible.
4. **Seguridad:** UID/GID y permisos reales, aislamiento de procesos, protección de memoria, autenticación, actualizaciones verificadas, APIs y drivers.
5. **Persistencia, instalación y memoria:** probar almacenamiento tras reinicios con datos reales, integridad ante cortes, BIOS/UEFI físico, regresiones en RAM >4 GB y el error histórico de `probar-ram`.
6. **Ofimática y aprendizaje:** probar interfaz gráfica de LibreOffice y apertura/guardado de archivos, gestor de aplicaciones, más contenido y evaluaciones interactivas de Vortex Saber.
7. **Calidad y rendimiento:** pruebas de integración de larga duración, gestión de procesos, usabilidad, arranque, fugas, regresiones y métricas reproducibles en QEMU, VMware y equipos reales.

## 5. Protocolo para continuar y supervisar el proyecto

1. Leer `README.md`, `AGENTS.md` y **este documento** del HEAD de `main`; revisar el código y la última CI. No pedir el repositorio nuevamente ni reiniciar el SO.
2. Antes de cambiar una función, identificar su implementación actual, distinguir fallos de tests, conservar código y escribir comprobaciones reproducibles.
3. Hacer commits en GitHub y fusionar las mejoras verificadas en `main`. Las ramas son temporales; no suponen otro Vortex OS. No usar una rama antigua como fuente completa de código sin comparación semántica.
4. Actualizar **este único estado técnico** cuando cambien características, verificaciones o bloqueos. No crear otro `TRASPASO_vXX.md`, `CONTINUIDAD_fecha.md` ni documentos técnicos duplicados.
5. Cuando el chat se aproxime a su límite, facilitar al usuario **en el chat** un informe detallado: SHA de `main`, últimas modificaciones, PR abiertos, resultados CI, artefactos probados, fallos y siguiente trabajo. No generar archivos de traspaso en cada entrega.

### Preservación del pasado sin ensuciar el repositorio

Los antiguos informes `TRASPASO_kng_v17..v24` y `TRASPASO_vortex_v17..v24`, la continuidad de noviembre y los documentos antiguos de auditoría/versionado **se retiraron del árbol actual después de consolidarlos aquí**. **No se destruyó el historial**: quedan accesibles en los commits anteriores de Git. Por ejemplo, tras `git fetch`:

```bash
git show 7b27c24d03763b9f563dde3f34cc492b48ac427d:TRASPASO_vortex_v24.md
git show 7b27c24d03763b9f563dde3f34cc492b48ac427d:CONTINUIDAD_VORTEX_2026-11-14.md
```

El commit `7b27c24d03763b9f563dde3f34cc492b48ac427d` es la referencia previa a la limpieza documental. La información histórica sobre pruebas, versiones, orígenes y problemas se resume arriba sin presentar como actuales observaciones antiguas.

**Conclusión operativa:** un Vortex OS, un `main`, un estado técnico consolidado. Continuar programando el sistema que ya existe.
