# Vortex Flow — compositor, rendimiento e interfaz

Estado de implementación de la rama `cambio-nombre-vortex-os`, noviembre de 2026.

## Qué cambia en el producto

Vortex OS mantiene el **kernel propio x86 de 32 bits y x86_64 de 64 bits** y comparte el código gráfico `kernel/gui.c` y `kernel/gfx.c`. No se ha reemplazado por Linux, ni se ha copiado el shell de Windows, macOS o Android.

- Dock compacto con **logotipo original Vortex**, no un icono adicional «Programas».
- Lanzador con categorías, iconos y **búsqueda por teclado**, resultados filtrados y **Enter para abrir**.
- Escritorio de cuatro accesos en cuadrícula, panel **Acceso rápido** (botones reales de Archivos y Galería) y panel Sistema (clic abre Monitor).
- Nuevos colores de superficies, barras y títulos, tipografía anti-aliased con espaciado proporcional en la interfaz, mientras la terminal conserva monoespaciado.
- El logotipo se rasteriza con **muestreo bilineal** para evitar el aspecto dentado del dibujo de 40×31 píxeles.
- Menú superior **Apagar / Suspender / Reiniciar**. Suspender sigue siendo reposo de pantalla, **no ACPI S3**.

## Qué cambia en el motor

Antes, `dmark()` acumulaba un único rectángulo alrededor de todos los cambios. Si cambiaban la hora en la esquina derecha y un icono a la izquierda, repintaba la unión, a veces casi toda la pantalla. Ahora:

1. Guarda hasta **12 rectángulos dañados** y combina únicamente los que se tocan. Si supera ese número, recurre a un repintado total seguro.
2. `render_region()` compone cada región a partir de fondo, objetos, ventanas y superposiciones. Copia al framebuffer físico únicamente las regiones afectadas.
3. Recupera el dibujo debajo del puntero y **solo dibuja el cursor una vez** por cuadro.
4. Al enfocar, abrir, cerrar o maximizar una ventana marca su área y los indicadores del dock sin invalidar siempre todo el escritorio.
5. El lanzador, búsqueda y menú de energía marcan sus propios rectángulos. Los controles multimedia vuelven a dibujar su ventana, no el escritorio entero.
6. La sombra anterior de ventanas hacía 12 pases de `rrect()` sobre superficies de tamaño ventana; se sustituyó por **bandas de sombra estrechas**.
7. El lanzador elimina capas translúcidas grandes y usa una superficie base **opaca**, con menos mezclas de píxeles sobre 760×490.
8. El explorador automático USB evita reiniciar la controladora periódicamente cuando los puertos UHCI no tienen dispositivo. Los clics del ratón no vuelcan datos al UART en compilaciones normales (activar `VORTEX_TRACE_MOUSE` al depurar).

Los gráficos siguen dibujándose **en CPU** sobre framebuffer 24/32 bits. No hay aceleración VMware SVGA ni composición 3D.

## Prueba repetible y no engañosa

`tools/hostgui.c` usa exactamente las rutinas gráficas nativas bajo AddressSanitizer. Comprueba apertura de aplicaciones, clics, widgets, cerrar/restaurar ventanas, reposo, búsqueda y uso de regiones dañadas. Mantiene dos métricas:

- `gui_painted_pixels`: suma de píxeles enviados al compositor por región; **no mide FPS** ni tiempo en VMware.
- Arrastre de 24 cuadros con `CLOCK_MONOTONIC`: registra tiempo total, ms/cuadro y píxeles/cuadro, **en el simulador Linux host bajo sanitizadores**, no en una GPU real.

Primera medida de referencia: 521.454 píxeles procesados al abrir el lanzador en una pantalla de 786.432 píxeles (1024×768); el movimiento del cursor solo no activa repintados. El presupuesto depende de qué regiones estén abiertas y no garantiza 60 fps.

Para repetir la auditoría: workflow GitHub Actions `vortex-escritorio-host` / `tools/hostgui.py` y leer los mensajes `[vortex-perf]`. Revisión con QEMU: workflows `prueba-grafica` y `arranque-qemu`.

## Lo que todavía impide parecerse a un sistema comercial

- **Gráficos:** driver propio VMware SVGA/virtio-gpu, cola de comandos, composición acelerada, sincronización a refresco, cursores por hardware, escalado DPI, fuentes vectoriales con shaping y ventanas de procesos aislados.
- **Multimedia:** audio HDA/ES1371, mezcla de varias aplicaciones, codecs MP3/AAC, demuxing y decodificadores H.264/VP9/AV1, sincronización A/V, control de latencia y uso eficiente de CPU.
- **Entrada/sistema:** pila USB EHCI/XHCI y hubs, touchpad/táctil, suspensión real ACPI, gestión de energía, accesibilidad, pruebas en VMware y equipo físico.
- **Calidad:** medir interacción humana (latencia clic→pantalla), tiempos de cuadro y CPU en VM real; perfilar la interfaz en resolución alta; realizar pruebas de regresión periódicas sin vender una captura como equivalencia funcional a Windows/macOS.

Las pruebas automatizadas no son prueba de que las funciones ausentes ya estén implementadas. El objetivo es elevar **rendimiento, presentación y estabilidad de la base**, antes de incorporar más aplicaciones nuevas.

## Atajos de multitarea (Vortex Flow, revisión posterior a 98fce05)

El teclado PS/2 distingue **Alt izquierdo** de **AltGr derecho**. Alt+Tab
recorre todas las ventanas en el orden de la pila, incluida una minimizada;
Mayús+Alt+Tab recorre en sentido inverso. Mientras Alt está pulsado, se
muestra una banda de tareas y la secuencia conserva el orden original: no
oscila solo entre las dos ventanas superiores. Al soltar Alt termina el ciclo.

Los códigos `KBD_SWITCH_*` se conservan como valores de 16 bits en las
colas de teclado de ambos kernels. Antes la cola de 32 bits guardaba
`unsigned char` y descartaba los códigos especiales, incluidos Ctrl+letra.
La consola de texto filtra códigos mayores que 255 para evitar caracteres
espurios. La ventana X recibe el resto de eventos PS/2 sin que Alt+Tab
se envíe como tecla a Linux.

**Pruebas:** `tools/kbdtest.c` cubre make/break, AltGr, reversa y repetición,
y `tools/hostgui.c` comprueba tres aplicaciones, el foco, la secuencia y
la restauración de ventanas minimizadas bajo AddressSanitizer.

## Redimensionamiento de ventanas

Las aplicaciones nativas permiten arrastrar el borde izquierdo/derecho,
superior/inferior y las cuatro esquinas. El gestor ajusta la geometría al
espacio visible del monitor y aplica tamaños mínimos específicos de cada
aplicación para proteger controles importantes. El compositor invalida la
zona anterior y la nueva, preservando la optimización por regiones dañadas.
No se habilita el redimensionamiento libre de la superficie X11, que depende
de una resolución de servidor fija hasta completar su integración.

La prueba de escritorio host arrastra una esquina y dos bordes, verifica
dimensiones exactas y comprueba que un arrastre fuera de pantalla se limita
a las dimensiones mínimas y a los bordes válidos del framebuffer.
