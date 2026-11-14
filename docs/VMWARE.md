# Vortex OS en VMware Workstation: uso real y limitaciones

Esta guía corresponde a la rama `cambio-nombre-vortex-os`. Los kernels de 32 y 64 bits siguen siendo propios de Vortex OS; **no se usa Linux como núcleo**.

## Una sola ISO

La publicación de desarrollo distribuye **VortexOS.iso** con dos entradas:

- **Vortex OS - 64 bits (grafico)**: kernel propio x86_64. Si la ISO incluye Debian 12, sus programas se ejecutan mediante la capa de compatibilidad del kernel Vortex.
- **Vortex OS - 32 bits (grafico)**: kernel propio i386, escritorio nativo. No ejecuta los binarios Debian x86_64.

En VMware Workstation, crear una VM de sistema operativo «Other 64-bit», BIOS/Legacy, 4 GB RAM, 2 vCPU, vídeo SVGA, tarjeta Intel e1000 si puede seleccionarse, montar VortexOS.iso como CD/DVD y marcar conectar al iniciar. **No instalar VMware Tools dentro de Vortex:** sus controladores están diseñados para huéspedes Linux/Windows que el kernel Vortex no implementa.

## Pendrive Kingston USB físico

**La unidad que aparece como KINGSTON (E:) en Windows sigue conectada al anfitrión, no al sistema invitado.** Que la detecte Windows no garantiza que VMware la haya entregado al kernel Vortex.

1. **Apaga la VM**. En VMware → VM → Configuración → Controladora USB, selecciona **compatibilidad USB 1.1**, que usa UHCI. El controlador actual de Vortex no maneja EHCI/XHCI, habituales en compatibilidad 2.0/3.x.
2. Conecta el pendrive a la computadora. En Windows revisa su formato: Vortex admite **FAT32 de 512 bytes/sector**, solo lectura; no exFAT ni NTFS. **No reformatees un USB con información para esta prueba**.
3. Inicia Vortex OS. En VMware → **VM → Dispositivos extraíbles → Kingston DataTraveler → Conectar (desconectar del anfitrión)**. Una unidad física pertenece a un solo sistema a la vez. Si continúa apareciendo como E: en el anfitrión, comprueba que se haya transferido al invitado.
4. Abre Aplicaciones → USB y pulsa **Buscar USB conectado** si el sondeo automático no lo ha reconocido aún.
5. Si la enumeración UHCI y lectura FAT32 tienen éxito aparecerá `/Dispositivos/Unidad-USB`. Se trata de una **instantánea de archivos leídos desde el dispositivo real**, no de un montaje en vivo; no se escriben sectores al pendrive. Puedes importar una imagen compatible desde ahí para usarla como fondo.

**Diagnóstico**: si aparece «UHCI detectado, sin dispositivo conectado», el kernel ve la controladora pero no se le ha presentado un pendrive compatible. Revisa conexión al invitado, modo USB 1.1 y FAT32. Si solo hay XHCI/EHCI, Vortex aún requiere un controlador nuevo. Probar otro controlador no implica que los datos se hayan importado.

## Audio real

Vortex dispone de controladores nativos:
- **AC'97 PCI**: probado en QEMU.
- **Sound Blaster 16 (ISA)**: backend adicional de WAV PCM 8/16 bits con DMA 8-bit mono y 22050 Hz. Para usarlo en VMware, apaga la VM y cierra VMware antes de editar su fichero `.vmx`: confirma que `sound.present = "TRUE"` y configura `sound.virtualDev = "sb16"`. Enciende la VM y revisa que VMware haya conectado la tarjeta de sonido al anfitrión.

VMware puede emular ES1371 o HDA por defecto. **Vortex no dispone todavía de controladores ES1371 ni Intel HDA**; seleccionar uno de ellos hace que el reproductor diga que no encuentra una tarjeta compatible. En determinadas versiones o VMs la emulación SB16 puede no estar disponible; debe comprobarse en el dispositivo real antes de dar la función por operativa.

El reproductor reproduce solo **WAV PCM**, no MP3/FLAC/OGG. El vídeo nativo sigue limitado a **AVI RGB sin compresión**, no MP4/H264. El blit escalado está optimizado, pero no existe aceleración gráfica VMware SVGA ni decodificador multimedia moderno.

## Energía y escritorio

El primer botón del dock es el **logotipo Vortex** y abre un lanzador por **categorías e iconos**. No existe el botón independiente «Programas» en el dock.

El botón de encendido de la barra superior muestra **Apagar**, **Suspender**, **Reiniciar**. Apagar y reiniciar utilizan los mecanismos del kernel y salvan VortexFS cuando está activo. **Suspender es únicamente reposo de pantalla recuperable con clic o tecla**: no implementa todavía el estado de suspensión ACPI S3, y no suspende físicamente la VM.

## Pruebas de desarrollo

Los workflows `arranque-qemu`, `vortex-escritorio-host`, `vortex-reproductores`, `vortex-audio-sb16`, `vortex-usb-mass-storage` y `vortex-iso-unica` aportan pruebas de las funciones en hardware virtualizado, cuando finalicen correctamente. Que el test UHCI/FAT32 en QEMU sea exitoso **no valida automáticamente un pendrive Kingston específico pasado a VMware**.

Referencias externas para VMware:
- https://techdocs.broadcom.com/us/en/vmware-cis/desktop-hypervisors/workstation-pro.html
- https://knowledge.broadcom.com/external/article/418689/connecting-usb-devices-to-a-virtual-mach.html
- https://wiki.osdev.org/Sound_Blaster_16
