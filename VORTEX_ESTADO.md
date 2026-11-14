# Vortex OS — estado vigente de la versión integrada

**Repositorio único:** https://github.com/dreyko24/SO  
**Rama oficial:** `main`. **Producto:** Vortex OS, kernel propio de 32 y 64 bits.  
**Última revisión de este documento:** 14 de noviembre de 2026. El código y los resultados de GitHub Actions pueden avanzar: verificar el HEAD de `main` y las ejecuciones de CI antes de afirmar nuevos resultados.

> Este documento sustituye como **referencia vigente** al antiguo `VORTEX_ESTADO.md` perdido durante la unificación. Su contenido histórico se conservó íntegro en `docs/historico/VORTEX_ESTADO_2026-11-14.md`.

## Integración: no hay que empezar de cero

- `cambio-nombre-vortex-os` y `vortex/integracion-total-20261114` son **ancestros de `main`** según la comparación GitHub realizada el 14/11/2026: todos sus commits son alcanzables desde `main`. El sistema avanzado de USB, multimedia, gráfica y educación **está integrado**, no en una subcarpeta aislada.
- El trabajo anterior de `vortex/window-management-20261114` (PR #9), la integración y QA posterior (PR #10) y las correcciones de ventanas, escalado, vídeo y USB (PR #13 a #17) se trasladaron a `main`. En varios casos GitHub usó **squash**: las ramas históricas pueden figurar como `diverged` sin que ello demuestre que falta la función correspondiente.
- `feat/unified-app-launcher` es una rama antigua y divergente; el `main` actual contiene un lanzador de aplicaciones por categorías y búsqueda más extenso. **No copiar el antiguo `kernel/gui.c` ni fusionar ramas obsoletas sin inspeccionar las diferencias función por función.**
- La unificación anterior **omitió archivos históricos de documentación y una prueba antigua**. Se restauraron `TRASPASO_kng_v17.md`–`v24.md`, `CONTINUIDAD_VORTEX_2026-11-14.md`, el estado antiguo en `docs/historico/` y la prueba antigua en `docs/historico/window_state_test_2026-11-14.c`. Los tests y workflows actuales prevalecen sobre sus variantes anteriores.
- Para retomar tras un nuevo chat, **leer `AGENTS.md` y este archivo de `main`, no basarse en el último chat o en el primer README histórico**.

## Implementado en el árbol de `main`

| Área | Código/ubicación | Alcance real |
| --- | --- | --- |
| Kernel propio x86 y x86_64 | `kernel/`, `kernel64/` | Dos núcleos, arranque mediante GRUB/QEMU, procesos y memoria. El modo 64 admite parte de las ABI/syscalls Linux y puede usar espacio de usuario Debian opcional; **no es Linux ni un derivado de Ubuntu**. |
| Escritorio Vortex Flow | `kernel/gui.c`, `kernel/gfx.c` | Ventanas independientes, minimización, maximización, pantalla completa, escritorios, dock lateral, lanzador único Aplicaciones, búsqueda y configuración. Hasta **24 ventanas con memoria dinámica**; dibujo por regiones y ventanas ocultas sin repintados innecesarios. |
| Fecha Perú y red | `kernel/net.c`, `kernel/ntp.h` | RTC y SNTP/NTP UDP como fuente opcional, UTC-5 para Lima, `fecha`, `hora`, `hora sincronizar`. Redes e1000/RTL8139 y otros modelos enumerados en README. La sincronización NTP no es una fuente de tiempo autenticada. |
| Navegador | `kernel/html.c`, `kernel/browser_launch.h`, scripts Debian | Visor HTML/HTTP simple; proxy externo opcional para HTTPS; opción de Chromium en Debian con capa Linux/X. El lanzador Chromium usa temporalmente `--no-sandbox`: **no apto para credenciales o navegación sensible**. No se ha validado un navegador CSS/JS/HTTPS completo en hardware real. |
| Educación | `kernel/encyclopedia.h`, `kernel/saber_progress.h`, `kernel/gui.c` | Vortex Saber con **31 artículos** e interfaz educativa, cuestionarios/juego y progreso; compatibilidad con resultados antiguos. No sustituir por la vieja versión de 4/6 materias. |
| Archivos y persistencia | `kernel/fs.c`, `kernel/disk.c` | Explorador, operaciones de archivo y almacenamiento persistente VortexFS (firma histórica KNGFS) bajo configuración explícita. No formatear discos físicos sin aprobación específica. |
| USB | `kernel/usb_storage.c` | Controlador UHCI 1.1 y Mass Storage BOT/SCSI. Lectura FAT32/exFAT **solo lectura**, instantánea importada en `/Dispositivos/Unidad-USB`. Probado con imágenes USB emuladas en QEMU; **sin soporte general EHCI/XHCI ni validación de todos los pendrives físicos**. |
| Audio y vídeo | `kernel/audio.c`, `kernel/video.c`, `kernel/media_demo.c`, `kernel/gfx.c` | WAV PCM en AC'97/SB16 emulados; AVI RGB sin compresión con escalado optimizado, muestra de **75 cuadros a 25 FPS**. Sin códecs nativos MP4/H.264/WebM, MP3/OGG/FLAC ni controladores HDA generales. |
| Ofimática | capa Debian 12 y lanzadores de aplicaciones | Integración/instalación opcional de LibreOffice y programas Linux; no se consideran validados todos los flujos gráficos y de instalación. |

## Evidencia y cómo reproducirla

GitHub Actions incluye `vortex-unificacion-integracion`, `vortex-escritorio-host`, `vortex-iso-unica`, `arranque-qemu`, `vortex-usb-mass-storage`, `vortex-reproductores`, `vortex-audio-sb16`. Durante noviembre de 2026 hubo ejecuciones exitosas de compilación, pruebas de host con ASan, lectura USB FAT32/exFAT emulada, captura de gráficos, WAV/AVI, arranque 32/64 y construcción de ISO. **Cada afirmación de éxito corresponde a la revisión probada; confirmar que siga pasando con el SHA actual antes de distribuir otra imagen.**

Comandos oficiales en un entorno Linux con herramientas instaladas:

```bash
git clone https://github.com/dreyko24/SO.git
cd SO
git switch main
git pull --ff-only
bash tools/verificar-vortex.sh
bash tools/build-iso.sh
bash tools/probar-vortex.sh out/VortexOS.iso
```

En Windows, `Probar-VortexOS.cmd "out\VortexOS.iso"`, con QEMU instalado. Los dos lanzadores admiten un segundo argumento con imagen USB FAT32/exFAT emulada **solo lectura**; las guías están en `README.md`. La ISO es para **pruebas** y no una distribución de escritorio comercial terminada.

## Problemas abiertos: seguir desarrollando

1. Controladores para **USB 2/3 EHCI/xHCI**, lectura/escritura robusta, extracción segura y pruebas físicas; teclado y ratón USB HID si corresponde.
2. Rendimiento real del vídeo y del escritorio en hardware/VMware: medición de FPS, DMA/IRQ, temporizadores, framebuffer, aceleración gráfica y códecs contemporáneos; HDA.
3. Navegador moderno funcional con HTTPS, CSS y JavaScript, sin `--no-sandbox`, usando aislamiento y permisos reales; compatibilidad ABI con procesos Linux/X.
4. Seguridad del kernel y usuarios: identidad, permisos, aislamiento de procesos, actualizaciones y firma, manejo robusto de errores.
5. Verificar persistencia tras reinicios, arranque e instalación BIOS/UEFI, rendimiento y estabilidad prolongada en hardware distinto de QEMU.
6. Completar interacción y contenido pedagógico de Vortex Saber y pruebas funcionales de ofimática y gestor de programas.

**Regla de continuidad:** una nueva sesión trabaja sobre el HEAD de `main`, conserva los archivos existentes y sus tests y realiza commits verificables. Solo se entrega un informe detallado al usuario cuando el chat esté cerca de su límite, conforme a su instrucción.
