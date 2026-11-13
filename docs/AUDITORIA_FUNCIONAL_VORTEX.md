# Auditoría funcional de Vortex OS — 13 de noviembre de 2026

**Rama de desarrollo:** `cambio-nombre-vortex-os`. El kernel propio de Vortex permanece como núcleo de 32 y de 64 bits. **No se sustituye por Linux.**

## Resultado de la inspección

Se examinaron la organización de archivos del repositorio, fuentes de kernel de ambas arquitecturas, interfaz, sistema de archivos, almacenamiento, red, controles de entrada, capa de compatibilidad 64-bit, pruebas y pipelines CI. Esta es una **auditoría funcional por subsistema**, no una prueba exhaustiva de todo camino de ejecución ni una certificación de hardware real.

| Área / fuentes | Estado observado | Límite principal |
| --- | --- | --- |
| Arranque 32 bits (`kernel/kernel.c`, `kernel/boot.S`) | Kernel y modo texto/gráfico propios; verificación con QEMU | Hardware real no validado ampliamente |
| Arranque 64 bits (`kernel64/kernel64.c`, `boot64.S`) | Kernel de modo largo, GUI compartida | Algunas rutas de memoria y recuperación de errores necesitan más pruebas |
| Procesos y compatibilidad (`kernel64/proc64.c`, `sys_*.inc`) | Ejecuta ELF y numerosas llamadas de procesos, memoria, red y archivos para espacio de usuario Linux opcional | No se deben equiparar todas las syscalls a Linux; faltan capacidades, protección completa, pruebas de señales y COW |
| GUI (`kernel/gui.c`, `gfx.c`) | Escritorio interactivo, ventanas, dock, menú Aplicaciones, explorador, ajustes, monitor, notas, calculadora, navegador | Aplicaciones nativas y kernel aún muy acoplados; faltan accesibilidad, aislamiento y mejor gestión de ventanas |
| Almacenamiento (`kernel/disk.c`) | ATA/IDE, AHCI/SATA y código NVMe; persistencia de árboles de archivos VortexFS | No montar FAT/exFAT externo ni tratar cualquier disco como VortexFS; firmar y verificar compatibilidad |
| Sistema de archivos (`kernel/fs.c`) | Jerarquía propia, escrituras, copias, mover, renombrar, persistencia | Gran parte de los archivos en RAM; permisos y seguridad insuficientes para entorno multiusuario |
| Red (`kernel/net.c`, `kernel64/sock.c`) | Controladores de red PCI y TCP/UDP/HTTP | TLS no nativo; dependencias de proxy; hardware PCI real limitado |
| Navegador (`kernel/html.c`) | Páginas HTML y búsqueda básica | Sin motor moderno CSS/JavaScript/imagen |
| Imágenes (`kernel/image.c`, `zip.c`) | Decodificación nativa PNG/BMP/PPM, visor y fondo personalizado; prueba QEMU confirmó la navegación y el cambio de fondo | Falta JPEG/GIF y PNG entrelazado; redimensionado y errores deben probarse con archivos complejos |
| Música/vídeo (`kernel/gui.c`) | Carpetas y pestañas en Multimedia, reconocimiento de archivos | **No hay reproductor ni pila de audio ni decodificador de vídeo** |
| USB (`kernel/usb.c`) | Exploración de controladores PCI por clase (UHCI/OHCI/EHCI/XHCI) sin escrituras a los dispositivos | **No enumera dispositivos, no detecta memorias USB, no monta FAT32/exFAT** |
| Integración multimedia | Galería, botón Aplicaciones, menú contextual de fondo; PNG de ejemplo integrado | Falta importación desde dispositivos externos |
| Pruebas (`.github/workflows/*.yml`) | QEMU, GUI con secuencia de ratón/teclado/capturas, host simulator con sanitizers | Mantener pruebas declarativas que validen eventos y no solo creen capturas; las pruebas completas del hardware siguen pendientes |

## Cambios ya aplicados en la rama

1. Botón **Aplicaciones** en la barra, menú funcional con lanzadores de apps nativas y pestañas Multimedia.
2. Visor de imágenes nativo: PNG 1/2/4/8 bits soportados para paleta/grises; PNG RGB(A) 8-bit sin interlace; BMP 24/32 sin compresión; PPM P6.
3. `/Imagenes`, `/Musica` y `/Videos` y muestra `Logo-Vortex.png`.
4. **Usar como fondo** dentro del visor y en el menú de clic derecho; ruta guardada en `/Sistema/vortex.cfg`.
5. Diagnóstico PCI de controladores USB, **sin soporte Mass Storage**.
6. Validación gráfica QEMU con logs que confirman apertura de Aplicaciones, lectura PNG y fondo aplicado. Véanse ejecuciones de la rama.
7. Actualización de scripts y simulador host para enlazar los módulos multimedia.

## Próximos desarrollos (orden recomendado)

**Prioridad 1 — Importación segura desde USB externo**

- Implementar enumeración de un controlador host USB (comenzar con XHCI o EHCI en QEMU, según el hardware elegido).
- Implementar descriptores, hubs/puertos, hotplug y detección de dispositivos USB Mass Storage; transporte Bulk-Only/SCSI y lectura de bloques.
- Montar **FAT32 en modo solo lectura primero**, verificar particiones, límites y firmas; agregar `/Dispositivos/USB1` en el explorador.
- Permitir importar archivos a `/Imagenes` y aplicar como fondo. Solo después de pruebas de seguridad, considerar escritura y exFAT.
- Pruebas QEMU con `qemu-xhci` y `usb-storage`, imágenes FAT32 con PNG y desconexión durante lectura; no tocar discos físicos para depurar.

**Prioridad 2 — Audio**

- Controlador de audio PCI nativo (p. ej. AC'97 o Intel HDA), buffers DMA, interrupciones/sondeo, salida PCM.
- Comenzar reproduciendo WAV PCM sin compresión con botón Reproducir/Pausa y volumen. Luego OGG/MP3 con decodificadores de licencia compatible.
- Pruebas con audio emulado QEMU y captura de PCM comparada con referencia.

**Prioridad 3 — Vídeo**

- Demultiplexor de contenedores, decodificador de vídeo (priorizar formatos de baja complejidad), sincronización A/V y reproducción en ventana; separar procesos de usuario y kernel.
- Compatibilidad Linux permite investigar un reproductor de usuario, pero **no equivale** a disponer de una pila de audio/vídeo nativa.

**Prioridad 4 — Fiabilidad, interacción y experiencia**

- Probar configuración persistente de fondo tras reiniciar con disco KNGFS y después de borrar/renombrar imagen.
- Añadir zoom y modo ajustar/llenar/centrar en visor; miniaturas, búsqueda y filtros en Galería.
- Mejorar gestión de memoria y aislamiento de procesos, permisos reales, control de errores y pruebas de compatibilidad.
- Revisar y actualizar documentación histórica de estado y limitaciones.

## Qué está verificado y qué no

- El flujo gráfico QEMU de la rama registró **`[vortex-apps] Galeria de imagenes`**, **`[vortex-image] decoded`** y **`[vortex-wallpaper] updated`** en una ejecución exitosa de `prueba-grafica`. Las capturas corresponden al **escritorio real arrancado en QEMU**, no a un diseño conceptual.
- La compilación del kernel de 32 y del de 64 bits completó en las pruebas gráficas.
- **USB Mass Storage, reproducción de vídeo, reproducción de audio, JPEG/GIF y conexión a pendrives reales no se han implementado**. No declarar compatibles hasta tener códigos, pruebas y evidencias.

## Preservación del diseño del proyecto

- Kernel Vortex propio para 32/64 bits.
- Se permite ejecutar programas Linux solo como **usuarios del kernel Vortex**, mediante capas implementadas por el proyecto.
- Firma histórica del sistema de archivos `KNGFS` preservada para evitar romper discos existentes. No se cambia sin estrategia de migración.
- La rama `main` no se toca hasta ejecutar las pruebas de regresión y revisar el diff.
