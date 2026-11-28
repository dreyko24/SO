# Vortex OS — sistema operativo propio de 64 bits

> **Continuidad del proyecto:** este es el repositorio oficial y `main` es su versión de desarrollo activa. Antes de iniciar un trabajo o retomar otro chat, leer **[AGENTS.md](AGENTS.md)** y **[VORTEX_ESTADO.md](VORTEX_ESTADO.md)**. Todas las ramas históricas se evalúan frente a `main`; no crear otro SO ni sobreescribir avances. Los antiguos traspasos y auditorías están **integrados en [VORTEX_ESTADO.md](VORTEX_ESTADO.md)**; sus originales se pueden recuperar en el historial Git, sin ensuciar la raíz.

Vortex OS usa un **kernel propio de 64 bits**, un escritorio con gestor de archivos, terminal y ajustes, y una capa de compatibilidad que permite ejecutar aplicaciones de Debian 12.

## Funciones principales
- **Red en más VMs**: además de e1000 y RTL8139, ahora AMD PCnet (VirtualBox por omisión) y virtio-net. Si no reconoce tu tarjeta, el error muestra su identificador PCI.
- **Archivos mejorados + disco**: `cp`, `mv`, `ren`, `df`; si la VM tiene un disco IDE, `disco formatear si` activa el guardado automático (KNGFS) y los archivos sobreviven al reiniciar. Nunca escribe en un disco sin la firma KNGFS salvo esa orden.
- **Monitor del sistema** y motores de búsqueda Brave/Google/Bing/DuckDuckGo en vortex-proxy.
- Mapa de módulos: `kernel/MODULOS.md`.
- **Rendimiento (v21)**: repintado solo de la zona que cambia, sombras baratas, ventanas tapadas no se dibujan, framebuffer en *write-combining* (PAT) en vez de sin caché.
- **Web real vía `vortex-proxy`**: `Proxy-vortex.cmd` (o `python tools/vortex-proxy.py`) en tu PC; vortex busca en DuckDuckGo y abre https:// a través de él (el cifrado lo hace el proxy, vortex sigue sin TLS). VM en NAT: el PC es 10.0.2.2:8080.
- **Escritorio modular**: iconos y widgets (reloj, sistema) arrastrables; doble clic abre; ventanas se maximizan con doble clic en el título y se ajustan arrastrando a los bordes.
- **Kernel de 64 bits** (`kernel64/`): modo largo, procesos en ring 3 y compatibilidad con ejecutables ELF de Linux.
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
- **USB:** los controladores propios **UHCI (USB 1.1)** y **xHCI** importan memorias Mass Storage BOT/SCSI con **FAT32 y exFAT (sectores de 512 bytes)**. El explorador muestra una instantánea de solo lectura en `/Dispositivos/Unidad-USB`. QEMU verificó memorias USB 3 conectadas directamente por xHCI y memorias UHCI directas o tras un hub. Para probar UHCI en VMware, selecciona compatibilidad **USB 1.1** y conecta la memoria al invitado desde **VM → Dispositivos extraíbles**. Quedan pendientes xHCI con hubs, UAS, EHCI, NTFS y la validación con pendrives físicos.
- **LibreOffice:** la ventana **Aplicaciones → LibreOffice** ofrece Writer, Calc, Impress, Draw y Math, deteccion de instalacion y acceso a instalacion/actualizacion. Se ejecuta como programa Debian para Linux sobre la **capa de compatibilidad propia de Vortex 64 bits**, con Xvfb como interfaz grafica. En la ISO con Debian, usa **Instalar LibreOffice** cuando aun no esta instalado; no se incluye LibreOffice de serie en el Debian base para evitar una imagen demasiado pesada.
- **Pruebas reales:** `vortex-usb-mass-storage` verificó la lectura de FAT32 y exFAT desde USB virtualizado, apertura del PNG y cambio de fondo. `vortex-reproductores` verifico AC'97 con audio WAV generado y AVI mostrado en QEMU. `prueba-grafica`, `vortex-jpeg-nativo`, `vortex-escritorio-host` y `arranque-qemu` comprueban componentes independientes.

Estos avances **no implican** compatibilidad universal con memorias USB de PCs modernas, con todos los formatos multimedia, ni ejecucion grafica de LibreOffice verificada en hardware fisico. Consulta [VORTEX_ESTADO.md](VORTEX_ESTADO.md) para evidencias, limitaciones y evolución técnica.

## Vortex Flow — nueva base visual y de rendimiento

La apariencia anterior y la lentitud percibida en VMware exigian intervenir la **interfaz y el compositor**, no agregar mas aplicaciones sobre el mismo entorno. Los cambios en `kernel/gui.c` y `kernel/gfx.c` incluyen:

- Escritorio redistribuido en cuadricula, tarjetas de acciones reales para Archivos, Galeria y Monitor, logotipo suavizado.
- Busqueda en el lanzador: escribe el nombre de una aplicacion y pulsa **Enter**; las categorias siguen funcionando con el raton.
- Tipografia antialias con ancho proporcional en la interfaz (la terminal conserva columnas fijas); superficies mas sencillas y menos transparencias costosas.
- Compositor con **regiones danadas independientes** (hasta 12), sin repintar toda la pantalla por cada clic, sombra de ventanas reducida y cursor separado del repintado.
- USB sin busquedas caras cada pocos segundos cuando no hay nada conectado a un puerto UHCI y raton sin escritura bloqueante de diagnostico al puerto serie en uso habitual.

**Medicion reproducible:** la prueba del lanzador a 1024×768 proceso 521.454 pixeles en comparacion con 786.432 pixeles del cuadro entero. Esta es una medida de trabajo de composicion, **no FPS de VMware ni garantia de rapidez en una GPU real**. El banco host registra tiempo de arrastre de ventanas. Ver [arquitectura grafica, medidas y limitaciones](docs/ARQUITECTURA_GRAFICA.md).

El compositor sigue siendo por software y **todavia no existe controlador grafico acelerado VMware SVGA ni codecs de video actuales**. Para lograr fluidez y experiencia a nivel de sistemas comerciales esas piezas siguen siendo trabajo real pendiente, no una funcion declarada completa.

## ISO oficial de 64 bits

La ISO oficial `VortexOS.iso` arranca el escritorio de Vortex OS de 64 bits y trae Firefox ESR preinstalado. Configura **4 GB de RAM** en la máquina virtual y monta el archivo como CD/DVD de arranque. En GRUB verás una sola entrada, **Vortex OS - 64 bits (grafico)**. [Descargar ISO](https://github.com/dreyko24/SO/releases/download/iso-latest/VortexOS.iso) · [Guía de VMware](docs/VMWARE.md)

El boton de energia de la barra ofrece **Apagar**, **Suspender** y **Reiniciar**. Por ahora **Suspender apaga solo la pantalla y se recupera pulsando una tecla o haciendo clic**, no es un suspendido ACPI S3.

## Construir la ISO (Windows + WSL)
1. Instala las herramientas en WSL con `sudo apt update && sudo apt install -y build-essential grub-pc-bin grub-efi-amd64-bin xorriso mtools python3 debootstrap debian-archive-keyring cpio`.
2. Ejecuta `Construir-ISO.cmd` o `wsl bash tools/build-iso.sh` para generar `out/VortexOS.iso` con Firefox ESR. Para una compilación ligera de desarrollo usa `VORTEX_DEBIAN=0 bash tools/build-iso.sh`.

## Probarla

**La rama `main` contiene el Vortex OS unificado**, con cuentas locales al arrancar, Firefox ESR, reloj NTP con zona Lima, Vortex Saber, reproducción WAV y AVI y acceso a almacenamiento USB.

Descarga el artefacto **`VortexOS-ISO-unica`** de un workflow exitoso en
[Actions — vortex-iso-unica](https://github.com/dreyko24/SO/actions/workflows/vm-iso.yml).
Extrae `out/VortexOS.iso` y verifica el SHA-256 del mismo ZIP. También puedes
compilarlo en Ubuntu/WSL con `VORTEX_DEBIAN=1 bash tools/build-iso.sh`.

Para iniciar la ISO sin tocar discos reales, desde el repositorio:

- Windows: `Probar-VortexOS.cmd "ruta\\a\\VortexOS.iso"` (QEMU instalado).
- Linux: `bash tools/probar-vortex.sh ruta/a/VortexOS.iso`.
- Ambos lanzadores usan QEMU, 4 GiB de RAM, red Intel e1000, audio PCI AC'97,
  bus USB UHCI, vídeo VGA estándar y reloj base UTC.

**Probar USB de almacenamiento sin conectar un pendrive real:** ambos lanzadores
aceptan un **segundo argumento opcional** con la ruta de un archivo `.img` FAT32 o exFAT.
Ejemplo Windows: `Probar-VortexOS.cmd "out\\VortexOS.iso" "pendrive.img"`.
Ejemplo Linux: `bash tools/probar-vortex.sh out/VortexOS.iso pendrive.img`.
El archivo IMG se presenta al kernel Vortex como USB Mass Storage UHCI **de solo
lectura**. No se conecta el USB físico del equipo y la prueba no demuestra soporte
para controladores xHCI/USB 3. Si no se aporta un archivo IMG, el bus USB
aparece vacío. Para crear una imagen de pruebas en Linux, con `dosfstools` y
`mtools` instalados:

```bash
truncate -s 128M pendrive.img
mkfs.vfat -F 32 -n VORTEXUSB pendrive.img
mcopy -i pendrive.img docs/vortex-logo.png ::/VORTEX.PNG
bash tools/probar-vortex.sh out/VortexOS.iso pendrive.img
```

En Vortex OS, abre **Multimedia → USB** y revisa la unidad detectada en
`/Dispositivos/Unidad-USB`. Se importa una instantánea de lectura, no un
montaje dinámico. Para un dispositivo USB físico se requiere transferencia al
invitado desde el hipervisor y hardware UHCI compatible; esa funcionalidad sigue
limitada y no está validada en todos los equipos.

Pruebas reproducibles de componentes: `bash tools/verificar-vortex.sh`.
Hora: terminal `fecha` o `hora`, y `hora sincronizar`; si NTP no responde
se usa el RTC y la interfaz indica su origen. La sincronización NTP carece de autenticación.

- Máquina virtual de 64 bits con **4 GB de RAM**, ratón PS/2 y tarjeta **Intel e1000** (o RTL8139) en **NAT**. Usa **BIOS/Legacy** si necesitas compatibilidad con entrada PS/2.
  - VirtualBox: Red → Adaptador 1 → NAT, tipo "Intel PRO/1000 MT Desktop". VMware: e1000. QEMU: `-device e1000,netdev=n0 -netdev user,id=n0`.
- Si el arranque indica que no pudo cargar el entorno de Firefox, comprueba la memoria de la VM y la suma SHA-256 de la ISO.
- Al arrancar por primera vez, crea la cuenta administradora. Usa **Actividades → Usuarios** para agregar cuentas, cambiar tu contraseña o bloquear la sesión. Para cambiar de usuario, reinicia y entra con la otra cuenta.
- Las cuentas se conservan entre reinicios si hay un disco con VortexFS activo. En una sesión en vivo sin ese disco puedes entrar como invitado o crear una cuenta temporal.
- En el modo gráfico de 64 bits, haz clic en **Firefox** en el escritorio o en **Actividades**. Escribe la URL o tu búsqueda en la barra de Firefox. El comando `red` muestra el estado de la conexión.

## Limitaciones actuales (sin rodeos)
- Firefox ESR en el modo de 64 bits mostró resultados públicos de Google y YouTube por HTTPS en QEMU. También pasó pruebas locales de JavaScript, almacenamiento web, Fetch, WebM VP8 sin audio y descarga de un adjunto binario con guardado automático. El primer arranque puede tardar. El diálogo de descarga, la reproducción con audio, las extensiones y el aislamiento siguen pendientes de validación. [Pruebas](VORTEX_ESTADO.md).
- Las cuentas controlan el acceso al escritorio y a la consola de arranque. Los permisos de archivos por usuario y el cifrado de disco siguen pendientes.
- El kernel de 64 bits tiene procesos e implementa numerosas llamadas compatibles con Linux, pero no todas las funciones de Linux están completas.
- El escritorio y sus aplicaciones nativas aún comparten componentes con el kernel. Los archivos viven en RAM y pueden persistir en discos compatibles mediante VortexFS (firma histórica KNGFS) si se activa el guardado.
- El escritorio acepta PS/2 y un teclado y un ratón relativo USB compatibles con HID Boot en puertos directos xHCI. En QEMU, ambos núcleos pasaron pruebas de escritura, clic y reconexión con PS/2 desactivado. Quedan pendientes los hubs xHCI, los dispositivos con informes HID distintos del modo de arranque y las pruebas con periféricos físicos.
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

Desde **Actividades → Tienda** puedes buscar aplicaciones del catálogo Debian, filtrar por categoría y usar **Instalar** o **Abrir**. La terminal muestra el progreso y los errores de instalación. Las aplicaciones que instales durante una sesión se guardan en RAM y se pierden al reiniciar.

Comandos en español (en la terminal de vortex, también dentro de la ventana gráfica):
`actualizar` · `instalar <nombre>` · `quitar <nombre>` · `mejorar` · `buscar <palabra>` · `mostrar <nombre>` · `paquetes` · `limpiar` · `ayuda-programas`
Nombres especiales de `instalar`: `brave`, `chrome`, `opera`, `chromium`, `firefox`, `libreoffice`, `openoffice` (el resto es cualquier paquete de Debian).
Programas con ventana: `pantalla` (servidor X real) y `abrir <programa>` (p. ej. `abrir brave`), o los lanzadores del panel **Aplicaciones** y la terminal.
Si Xvfb termina, la siguiente llamada a `pantalla` o `abrir` inicia un servidor y un puente de entrada nuevos.
Diagnóstico: `rastro 1` muestra las llamadas del sistema que fallan. Estado real y pendientes: consulta [VORTEX_ESTADO.md](VORTEX_ESTADO.md), documento técnico único.


## Identidad visual de Vortex OS
El escritorio, la ventana Acerca de y el dock conservan el emblema oficial; la barra superior muestra **Vortex OS** y el botón de energía, mientras que el primer icono del dock es el logotipo oficial y abre **Aplicaciones**. Vector: `docs/vortex-logo.svg`. Se conserva la firma interna `KNGFS` en disco para mantener compatibilidad con instalaciones existentes.
