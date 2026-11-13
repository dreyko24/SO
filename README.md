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
- **Escritorio gráfico** con el emblema Vortex, ventanas, dock, reloj y menú superior **Aplicaciones**. Resoluciones dependientes del framebuffer (hasta 1920x1080 probadas en QEMU).
- **Archivos**, **Terminal**, **Ajustes** y **Navegador** (con **buscador**: palabras sueltas buscan en páginas y archivos; opcionalmente en la web por http con `buscador=http://.../?q=%s` en `/Sistema/vortex.cfg`).
- **Apps simples**: **Notas** (guarda en /Documentos; los .txt se abren aquí), **Calculadora**, **Snake**.
- Interfaz con fondos predefinidos o personalizados, indicador de red y dock de 10 aplicaciones.
- **Red**: tarjetas Intel e1000 y Realtek RTL8139, DHCP, DNS, TCP, HTTP. Navegador con `http://`; terminal con `red`, `ping`, `dns`, `wget`.

Capturas de la interfaz: carpeta `docs/` (generadas con el mismo código de la interfaz).

## Multimedia y personalización (versión de desarrollo)

- **Aplicaciones → Galería de imágenes:** abrir archivos de la carpeta `/Imagenes` en un visor nativo de Vortex OS. La interfaz también tiene pestañas **Música**, **Vídeos** y **USB**.
- **Imágenes que se dibujan de verdad:** PNG sin entrelazado (profundidad 1/2/4/8 bits en escala de grises o paleta; 8 bits RGB/RGBA), BMP sin compresión de 24/32 bits y PPM P6. JPEG y GIF se reconocen por extensión, pero su decodificador aún no existe. El visor limita las dimensiones a 2048 × 2048.
- **Fondo personalizado:** abre una imagen compatible y pulsa **Usar de fondo**, o usa el menú contextual del archivo → **Usar como fondo de pantalla**. Se guarda la ruta en `/Sistema/vortex.cfg` y se restaura si el archivo persiste en VortexFS.
- **Biblioteca multimedia:** `/Imagenes`, `/Musica`, `/Videos` y panel de dispositivos USB. Por ahora **no hay reproducción de audio ni vídeo ni un controlador de tarjetas de sonido**.
- **USB:** el panel detecta controladores PCI USB (UHCI, OHCI, EHCI, XHCI), pero **no detecta ni monta aún pendrives ni importa sus archivos**. No conectar un USB equivale a perder archivos; el sistema no debe escribir sobre él sin autorización.
- **Pruebas:** GitHub Actions → `prueba-grafica` verifica abrir Aplicaciones, abrir la Galería, decodificar el PNG de ejemplo y aplicarlo como fondo. El flujo `arranque-qemu` comprueba ambos kernels; `vortex-escritorio-host` cubre regresiones con AddressSanitizer.

Consulta `docs/AUDITORIA_FUNCIONAL_VORTEX.md` para límites y siguientes controladores previstos.

## Construir la ISO (Windows + WSL)
1. Una sola vez, en WSL: `sudo apt update && sudo apt install -y build-essential gcc-multilib grub-pc-bin xorriso mtools python3`
2. Doble clic en `Construir-ISO.cmd` (o `wsl bash tools/build-iso.sh`). Resultado: `out/VortexOS.iso`.

## Probarla
- Máquina virtual preferentemente **BIOS/Legacy** para compatibilidad de entrada (UEFI probado en QEMU), 256 MB de RAM o más sin Debian; ratón PS/2 y tarjeta **Intel e1000** (o RTL8139) en **NAT**.
  - VirtualBox: Red → Adaptador 1 → NAT, tipo "Intel PRO/1000 MT Desktop". VMware: e1000. QEMU: `-device e1000,netdev=n0 -netdev user,id=n0`.
- Menú de GRUB: 32 bits (gráfico), **64 bits (gráfico)**, y las dos versiones en modo texto (útiles si el modo gráfico no arranca).
- En el navegador: escribe `example.com` o `http://neverssl.com`. En la terminal: `red`.

## Limitaciones actuales (sin rodeos)
- **https:// no funciona** (no hay TLS): muchos sitios modernos no cargarán; sirven los que ofrecen http.
- El navegador ignora CSS, JavaScript e imágenes: es un visor de HTML simple.
- El kernel de 64 bits tiene procesos e implementa numerosas llamadas compatibles con Linux, pero no todas las funciones de Linux están completas.
- El escritorio y sus aplicaciones nativas aún comparten componentes con el kernel. Los archivos viven en RAM y pueden persistir en discos compatibles mediante VortexFS (firma histórica KNGFS) si se activa el guardado.
- El teclado y ratón nativos usan principalmente PS/2. Hay detección de controladores USB PCI, pero **todavía no existe controlador USB Mass Storage ni montaje de memorias USB**. Se admiten distribuciones US/Latam/ES.
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
Programas con ventana: `pantalla` (servidor X real) y `abrir <programa>` (p. ej. `abrir brave`), o la ventana **Programas** del dock.
Diagnóstico: `rastro 1` muestra las llamadas del sistema que fallan. Estado real y pendientes: ver `TRASPASO_vortex_v24.md`.


## Identidad visual de Vortex OS
El escritorio, la ventana Acerca de y el dock conservan el emblema oficial; la barra superior muestra el botón funcional **Aplicaciones**. Vector: `docs/vortex-logo.svg`. Se conserva la firma interna `KNGFS` en disco para mantener compatibilidad con instalaciones existentes.
