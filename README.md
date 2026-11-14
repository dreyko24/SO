# Vortex OS — sistema operativo propio, 32 y 64 bits

Sistema operativo **independiente**: dos kernels propios (`vortex-kernel` de 32 y de 64 bits), interfaz gráfica propia, gestor de archivos,
terminal, navegador con red, ajustes. **No utiliza el kernel Linux.** Su núcleo es propio; opcionalmente puede ejecutar binarios Linux y un espacio de usuario Debian 12 mediante la capa de compatibilidad de 64 bits.

## Funciones principales
- **Red en más VMs**: además de e1000 y RTL8139, ahora AMD PCnet (VirtualBox por omisión) y virtio-net. Si no reconoce tu tarjeta, el error muestra su identificador PCI.
- **Archivos mejorados + disco**: `cp`, `mv`, `ren`, `df`; si la VM tiene un disco IDE, `disco formatear si` activa el guardado automático (KNGFS) y los archivos sobreviven al reiniciar. Nunca escribe en un disco sin la firma KNGFS salvo esa orden.
- **Monitor del sistema** y motores de búsqueda Brave/Google/Bing/DuckDuckGo en vortex-proxy.
- Mapa de módulos: `kernel/MODULOS.md`.
- **Rendimiento (v21)**: repintado solo de la zona que cambia, sombras baratas, ventanas tapadas no se dibujan, framebuffer en *write-combining* (PAT) en vez de sin caché.
- **Web real vía `vortex-proxy`**: `Proxy-vortex.cmd` (o `python tools/vortex-proxy.py`) en tu PC; vortex busca en DuckDuckGo y abre https:// a través de él (el cifrado lo hace el proxy, vortex sigue sin TLS). VM en NAT: el PC es 10.0.2.2:8080.
- **Escritorio modular**: iconos y widgets (reloj, sistema) arrastrables; doble clic abre; ventanas se maximizan con doble clic en el título y se ajustan arrastrando a los bordes.
- **Kernel de 32 bits** (`kernel/`): procesos en ring 3, ELF, fork/exec/wait, memoria virtual.
- **Kernel de 64 bits** (`kernel64/`): modo largo, misma interfaz y misma red. Incluye procesos y compatibilidad con binarios Linux ELF en el estado documentado en v24.
- **Escritorio gráfico** con el nombre Vortex OS en la barra superior, lanzador **Aplicaciones** con el logotipo oficial en el primer icono del dock, ventanas y reloj. Resoluciones dependientes del framebuffer (hasta 1920x1080 probadas en QEMU).
- **Archivos**, **Terminal**, **Ajustes** y **Navegador** (con **buscador**: palabras sueltas buscan en páginas y archivos; opcionalmente en la web por http con `buscador=http://.../?q=%s` en `/Sistema/vortex.cfg`).
- **Apps simples**: **Notas** (guarda en /Documentos; los .txt se abren aquí), **Calculadora**, **Snake**.
- Interfaz con fondos predefinidos o personalizados, indicador de red y dock con un lanzador de aplicaciones y accesos rapidos.
- **Red**: tarjetas Intel e1000 y Realtek RTL8139, DHCP, DNS, TCP, HTTP. Navegador con `http://`; terminal con `red`, `ping`, `dns`, `wget`.

Capturas de la interfaz: carpeta `docs/` (generadas con el mismo código de la interfaz).

## Aplicaciones, multimedia, USB y LibreOffice

- **Identidad visual:** la barra superior muestra **Vortex OS** sin icono. El primer boton del dock se llama **Aplicaciones**, muestra el logotipo de Vortex OS y abre un **panel grande de categorias con iconos** para Internet, Multimedia, Oficina, Sistema, Utilidades y Juegos. Ya no existe un icono independiente Programas en el dock.
- **Imagenes:** galeria y visor nativos PNG (sin entrelazado), JPEG de base compatible, BMP 24/32 bits y PPM P6. Las imagenes abiertas desde el explorador pueden usarse como fondo de pantalla; se guarda la ruta en `/Sistema/vortex.cfg` si hay almacenamiento persistente.
- **Audio nativo:** reproductor WAV PCM 8/16 bits, mono/estereo, salida por **Intel AC'97 PCI** o **Sound Blaster 16 ISA** con DMA (seleccionable en VMware mediante `sound.virtualDev = "sb16"`). Incluye Play/Pausa/Detener y archivo de muestra en `/Musica`. **MP3, OGG y FLAC aun no se reproducen de forma nativa**. Las tarjetas VMware ES1371/HDA aun requieren controladores propios adicionales.
- **Video nativo:** reproductor de AVI con cuadros RGB de 24/32 bits **sin compresion**, controles de reproduccion, escalado acelerado por software y temporizacion que salta cuadros para mantener el ritmo si la VM es lenta; muestra en `/Videos`. MP4/H.264, WebM y otros codecs comprimidos **no estan implementados**.
- **USB fisico:** controlador propio para tarjetas **PCI UHCI (USB 1.1)**, enumeracion de almacenamiento USB Mass Storage Bulk-Only/SCSI y lectura de particiones **FAT32 (512 bytes/sector)**. La unidad se detecta por el hardware, NO se crea una carpeta fingiendo un USB; ademas sondea automaticamente la llegada de dispositivos en UHCI. **Cuando se detecta**, el explorador muestra en `/Dispositivos/Unidad-USB` una instantanea de los archivos leidos desde el pendrive. La instantanea NO es un montaje dinamico: el sistema no escribe al USB y la copia de archivos esta limitada actualmente. Se probo con un dispositivo USB Mass Storage emulado en QEMU, no en un pendrive real conectado a PC. En VMware es necesario transferir el pendrive desde Windows a la VM: **VM > Dispositivos extraibles > Kingston > Conectar al invitado**; configurar compatibilidad **USB 1.1** y usar FAT32. **No soporta aun controladores EHCI/XHCI (USB 2/3), exFAT ni NTFS**.
- **LibreOffice:** la ventana **Aplicaciones → LibreOffice** ofrece Writer, Calc, Impress, Draw y Math, deteccion de instalacion y acceso a instalacion/actualizacion. Se ejecuta como programa Debian para Linux sobre la **capa de compatibilidad propia de Vortex 64 bits**, con Xvfb como interfaz grafica. En la ISO con Debian, usa **Instalar LibreOffice** cuando aun no esta instalado; no se incluye LibreOffice de serie en el Debian base para evitar una imagen demasiado pesada.
- **Pruebas reales:** `vortex-usb-mass-storage` verifico la lectura de FAT32 desde USB virtualizado, apertura del PNG y cambio de fondo. `vortex-reproductores` verifico AC'97 con audio WAV generado y AVI mostrado en QEMU. `prueba-grafica`, `vortex-jpeg-nativo`, `vortex-escritorio-host` y `arranque-qemu` comprueban componentes independientes.

Estos avances **no implican** compatibilidad universal con memorias USB de PCs modernas, con todos los formatos multimedia, ni ejecucion grafica de LibreOffice verificada en hardware fisico. Consulta `docs/AUDITORIA_FUNCIONAL_VORTEX.md`.

## Una sola ISO para 32 y 64 bits graficos

La ISO unica publicada como `VortexOS.iso` contiene **solamente dos entradas en GRUB**: `Vortex OS - 64 bits (grafico)` y `Vortex OS - 32 bits (grafico)`. La ISO de GitHub Releases incluye Debian 12 para el modo de 64 bits; LibreOffice se instala despues. La misma imagen permite usar ambos kernels propios, sin instalar el SO en un disco. Documentacion para VMware: [docs/VMWARE.md](docs/VMWARE.md).

El boton de energia de la barra ofrece **Apagar**, **Suspender** y **Reiniciar**. Por ahora **Suspender apaga solo la pantalla y se recupera pulsando una tecla o haciendo clic**, no es un suspendido ACPI S3.

## Construir la ISO (Windows + WSL)
1. Una sola vez, en WSL: `sudo apt update && sudo apt install -y build-essential gcc-multilib grub-pc-bin xorriso mtools python3`
2. Doble clic en `Construir-ISO.cmd` (o `wsl bash tools/build-iso.sh`). Resultado: `out/VortexOS.iso`. Para incluir Debian como la version publicada: `VORTEX_DEBIAN=1 bash tools/build-iso.sh`.

## Probarla
- Máquina virtual preferentemente **BIOS/Legacy** para compatibilidad de entrada (UEFI probado en QEMU), 256 MB de RAM o más sin Debian; ratón PS/2 y tarjeta **Intel e1000** (o RTL8139) en **NAT**.
  - VirtualBox: Red → Adaptador 1 → NAT, tipo "Intel PRO/1000 MT Desktop". VMware: e1000. QEMU: `-device e1000,netdev=n0 -netdev user,id=n0`.
- Menú de GRUB: **Vortex OS - 64 bits (gráfico)** y **Vortex OS - 32 bits (gráfico)**. Ambas opciones están en un único archivo ISO.
- En el navegador: escribe `example.com` o `http://neverssl.com`. En la terminal: `red`.

## Limitaciones actuales (sin rodeos)
- **https:// no funciona** (no hay TLS): muchos sitios modernos no cargarán; sirven los que ofrecen http.
- El navegador ignora CSS, JavaScript e imágenes: es un visor de HTML simple.
- El kernel de 64 bits tiene procesos e implementa numerosas llamadas compatibles con Linux, pero no todas las funciones de Linux están completas.
- El escritorio y sus aplicaciones nativas aún comparten componentes con el kernel. Los archivos viven en RAM y pueden persistir en discos compatibles mediante VortexFS (firma histórica KNGFS) si se activa el guardado.
- El teclado y ratón nativos usan principalmente PS/2. USB Mass Storage **solo soporta UHCI con FAT32 en lectura e instantanea**, no XHCI/EHCI ni exFAT; aun falta validar pendrives fisicos. Se admiten teclados US/Latam/ES.
- El sistema sí ha arrancado en QEMU mediante GitHub Actions. Eso no equivale a validación completa en equipos físicos, ni a compatibilidad de todos los dispositivos.

## Pruebas
- `gcc tools/kbdtest.c -o /tmp/kbdtest && /tmp/kbdtest` — teclado: US/Latam/Espana, Mayus, AltGr, tildes.
- `python3 tools/nettest.py` — pila de red real contra una red simulada (sin QEMU).
- `make -C kernel hosttest` — memoria, ELF, fork, brk. `make -C kernel hostgui` — interfaz dibujada en el host.
- `make -C kernel test` / `make -C kernel64 run` — con QEMU instalado.

Autor: Nick Jose Tolentino Rojas · Fuente: DejaVu Sans Mono (licencia libre Bitstream Vera/DejaVu).


## Programas Linux REALES (kernel de 64 bits)
vortex 64 bits ejecuta programas de Linux sin modificar (ELF, glibc, ld.so) con su propia capa de compatibilidad (sin código de Linux): procesos, hilos, señales,
sockets, epoll, mmap/memoria compartida, tty, /proc y /dev. Encima corre un **Debian 12 real** (apt/dpkg, curl, busybox...).

**ISO con Debian** (necesita WSL con sudo): `VORTEX_DEBIAN=1 bash tools/build-iso.sh` (o el workflow *construir-iso* en GitHub Actions, que lo publica en Releases).
Máquina virtual con **4 GB de RAM o más** (el sistema vive en memoria).

Comandos en español (en la terminal de vortex, también dentro de la ventana gráfica):
`actualizar` · `instalar <nombre>` · `quitar <nombre>` · `mejorar` · `buscar <palabra>` · `mostrar <nombre>` · `paquetes` · `limpiar` · `ayuda-programas`
Nombres especiales de `instalar`: `brave`, `chrome`, `opera`, `chromium`, `firefox`, `libreoffice`, `openoffice` (el resto es cualquier paquete de Debian).
Programas con ventana: `pantalla` (servidor X real) y `abrir <programa>` (p. ej. `abrir brave`), o los lanzadores del panel **Aplicaciones** y la terminal.
Diagnóstico: `rastro 1` muestra las llamadas del sistema que fallan. Estado real y pendientes: ver `TRASPASO_vortex_v24.md`.


## Identidad visual de Vortex OS
El escritorio, la ventana Acerca de y el dock conservan el emblema oficial; la barra superior muestra **Vortex OS** y el botón de energía, mientras que el primer icono del dock es el logotipo oficial y abre **Aplicaciones**. Vector: `docs/vortex-logo.svg`. Se conserva la firma interna `KNGFS` en disco para mantener compatibilidad con instalaciones existentes.
