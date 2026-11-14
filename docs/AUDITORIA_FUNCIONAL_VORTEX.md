# Auditoría funcional de Vortex OS — 13 de noviembre de 2026

**Rama:** `cambio-nombre-vortex-os`. Vortex OS conserva sus dos kernels propios (x86 de 32 y x86_64 de 64 bits). La compatibilidad con ejecutables ELF/Linux es una capa adicional en el kernel Vortex, **no una sustitución por Linux**.

## Estado comprobado en QEMU

| Subsistema | Código | Resultado y limitación |
| --- | --- | --- |
| Escritorio | `kernel/gui.c` y `gfx.c` | La barra superior dice **Vortex OS** sin logotipo; el primer elemento del dock usa el **logotipo Vortex** como lanzador de **aplicaciones por categorías e iconos**; se eliminó el botón independiente Programas. Prueba de interfaz exitosa |
| Kernel 32 y 64 bits | `kernel/`, `kernel64/` | Arranque en QEMU y compilación de ambos kernels. `arranque-qemu` completó correctamente; pruebas de compatibilidad de red 42/42 y memoria 77/77 en ese entorno |
| Imágenes | `kernel/image.c`, `jpeg.c`, `third_party/picojpeg.c` | PNG (sin entrelazado, 1/2/4/8 bits compatibles), JPEG básico, BMP 24/32 y PPM P6; visor gráfico y fondo personalizado. JPEG probado con pruebas nativas |
| Música | `kernel/audio.c` | Reproductor WAV PCM 8/16 bits, 1/2 canales y salida PCI AC'97 o ISA Sound Blaster 16 (SB16) con DMA, botones reproducir/pausa/detener. Prueba `vortex-reproductores` en QEMU con audio real en WAV |
| Vídeo | `kernel/video.c` | Reproductor AVI **sin compresión** RGB 24/32 bits y control de pausa/reinicio. Prueba gráfica de vídeo en QEMU correcta. Escalado directo optimizado y temporización que salta cuadros cuando la VM va lenta. Sin decodificador MP4/H.264/HEVC ni WebM |
| USB externo | `kernel/usb_storage.c` | Controlador **UHCI**, lectura SCSI/Bulk-Only y FAT32. Se verificó USB Mass Storage emulado por QEMU con FAT32, importación de imagen y fondo. **No probado en pendrive conectado físicamente a VMware/PC**; sin EHCI/XHCI/exFAT; nuevo sondeo periódico de puertos UHCI |
| Acceso USB | `kernel/usb_storage.c`, `kernel/fs.c` | `/Dispositivos/Unidad-USB` **solo se crea tras detectar una unidad real en el bus UHCI**. Los ficheros son una **instantánea importada desde el hardware**, no un montaje de bloques en vivo ni una carpeta ficticia preexistente. Límite 100 archivos de hasta 4 MiB por archivo; acceso de origen solo lectura |
| LibreOffice | `kernel/gui.c`, `tools/rootfs-es/usr/local/bin/instalar` | Lanzador de Writer, Calc, Impress, Draw y Math; instalador Debian 12 con traducción española, Xvfb y archivos ODT/ODS/ODP/DOCX/XLSX/PPTX. **No preinstalado** en la ISO base. Funciona a través de compatibilidad Linux del kernel propio; hay test previo de conversión headless, no validación física de todas las apps gráficas |
| Discos y persistencia | `kernel/disk.c`, `fs.c` | ATA, AHCI, NVMe y persistencia en VortexFS (firma histórica KNGFS). No formatear ni modificar discos ajenos sin confirmación |
| Red y navegador | `kernel/net.c`, `kernel/html.c`, `kernel64/sock.c` | Red PCI, TCP/UDP/HTTP, navegador HTML básico y proxy opcional para HTTPS. Sin TLS nativo ni motor completo CSS/JS |
| Procesos Linux en Vortex 64 | `kernel64/proc64.c`, `kernel64/sys_*.inc` | Ejecuta ELF estático/dinámico y muchas syscalls. Falta completar seguridad/aislamiento, permisos y compatibilidad con todo el software Linux |

## Cambios de esta versión

1. Diseño de marca: barra superior muestra `Vortex OS`, sin icono pequeño; primer botón del dock es el **emblema Vortex**, abre el launcher por categorías con iconos y nombres, se eliminó el icono `Programas`.
2. Reproductores nativos de imágenes, audio WAV PCM y vídeo AVI RGB sin compresión, con archivos de demostración generados en `/Imagenes`, `/Musica` y `/Videos`.
3. Importación de archivos desde **USB Mass Storage UHCI FAT32 realmente conectado al bus**; la carpeta visible se crea solo tras identificar y leer el dispositivo. Se puede abrir una imagen del USB, copiarla a `/Imagenes` y aplicarla como fondo; no se escribe en el dispositivo origen.
4. Suite LibreOffice disponible desde el lanzador, con opciones Writer/Calc/Impress/Draw/Math e instalación y actualización de paquetes Debian. No constituye un reemplazo de kernel.
5. Menú de energía superior con **Apagar, Suspender y Reiniciar**. Suspender es **reposo de pantalla**, no ACPI S3 real.
6. Reproductor SB16 para compatibilidad con tarjetas ISA Sound Blaster en VMware/QEMU, con configuración VMware en `docs/VMWARE.md`.
7. **Una única `VortexOS.iso`** con exactamente dos entradas gráficas (32 y 64 bits), y Debian accesible en el modo de 64 bits. Instalador de disco desactivado por defecto.
8. Escalado directo en backbuffer y ritmo de AVI ajustado a tiempo real cuando el rendimiento cae; evita repintado por función por cada píxel.

## Verificación automatizada

En el commit anterior `922ad50263252c287975ecc2731aafba35c1d707` de esta rama, seis flujos terminaron en **success** (evidencia histórica, no validación automática del código actual). Las nuevas pruebas deben comprobar las revisiones de interfaz, SB16 y la ISO unificada antes de publicar la versión:

- `prueba-grafica` [ejecución 43529981860](https://github.com/dreyko24/SO/actions/runs/43529981860)
- `vortex-jpeg-nativo` [ejecución 43529981865](https://github.com/dreyko24/SO/actions/runs/43529981865)
- `vortex-escritorio-host` [ejecución 43529981833](https://github.com/dreyko24/SO/actions/runs/43529981833)
- `vortex-usb-mass-storage` [ejecución 43529981717](https://github.com/dreyko24/SO/actions/runs/43529981717)
- `vortex-reproductores` [ejecución 43529981740](https://github.com/dreyko24/SO/actions/runs/43529981740)
- `arranque-qemu` [ejecución 43529981766](https://github.com/dreyko24/SO/actions/runs/43529981766)

Estas pruebas validan los casos exactos del entorno **QEMU**, no garantizan el reconocimiento de cada memoria física o tarjeta de sonido real. Las revisiones posteriores deben superar nuevamente los flujos antes de integrar en `main`.

## Límites y siguientes pasos

**USB:** el controlador actual cubre UHCI 1.1 y sondeo periódico para hotplug básico. La transferencia del pendrive de Windows al invitado VMware y su formato FAT32 son requisitos indispensables. Para la mayoría de PCs actuales hace falta implementar XHCI (USB 3.x), opcionalmente EHCI (USB 2.0), hubs, enumeración hotplug, recuperación tras desconexión y lectura exFAT. La vista actual es una instantánea de lectura, no un acceso continuo a archivos de un dispositivo desconectable. Probar con USB físico, sin escribirle, antes de declararlo estable.

**Multimedia:** AC'97 y SB16 son controladores diferentes; no existe compatibilidad HDA/ES1371 por ahora. Añadir MP3/OGG/FLAC, salida HDA y control real de volumen; para vídeo implementar decodificadores habituales (MP4/H.264/WebM), contenedores, sincronización audio-vídeo y reproducción en proceso aislado. Hoy los WAV y AVI que soporta la app son realmente reproducibles en QEMU.

**LibreOffice:** mejorar pruebas gráficas reales sobre Vortex 64 con Debian e instalación persistente. Comprobar apertura/guardado ODT, ODS, ODP, DOCX, XLSX y PPTX, rendimiento e interacción con teclado y ratón. No confundir lanzamiento de proceso con funcionalidad completa de toda la suite.

**Calidad:** endurecer el parser FAT32 y el tratamiento de USB desconectado, validar memoria DMA en máquinas físicas, controlar archivos JPEG/AVI malformados, separar servicios del kernel, revisar permisos, corregir rutas obsoletas de CI y mantener documentación actualizada.

**Preservación:** no se modifica el formato histórico KNGFS sin migración, ni se incorpora Linux como núcleo. Mantener la rama `main` protegida hasta que estén verdes las pruebas requeridas.

## Guía operativa VMware

Consulta [`docs/VMWARE.md`](VMWARE.md) para asociar un USB físico al invitado (USB 1.1 UHCI y FAT32), elegir Sound Blaster 16 como salida de audio y comprender los límites reales de suspensión y reproducción. Una prueba sobre QEMU **no sustituye** una prueba física con una memoria Kingston ni representa reproducción universal de MP4.
