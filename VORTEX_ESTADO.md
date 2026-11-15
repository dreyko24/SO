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
| USB | `kernel/usb_storage.c` | Host UHCI USB 1.1, BOT/SCSI Mass Storage, lectura FAT32 y exFAT de solo lectura e importación instantánea a `/Dispositivos/Unidad-USB`. Verificado con USB emulado QEMU; sin EHCI/xHCI general, sin montaje dinámico y sin validación universal de pendrives físicos. |
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

## 4. Limitaciones y trabajo de ingeniería que continúa

1. **USB real:** host EHCI/xHCI, HID, DMA/IRQ y hotplug, recuperación al desconectar, escritura/montaje robustos, pruebas sobre varios controladores y pendrives; no asumir que UHCI QEMU implica USB 3 físico.
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
