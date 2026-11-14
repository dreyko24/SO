# Vortex OS — registro técnico y ejecución

**Estado:** desarrollo activo. Esta rama conserva los kernels propios kng de 32 y 64 bits, el sistema de archivos, la red y las aplicaciones existentes. Los componentes que siguen sin estar implementados o comprobados están identificados, no se sustituyen con demostraciones.

## Cambios implementados

- Gestor de ventanas: minimizar, restaurar desde el dock, maximizar, restaurar geometría, cierre, pantalla completa auténtica con F11 o el botón de la ventana. F10 crea una nueva instancia de la aplicación activa; Ctrl+M minimiza y Ctrl+W cierra. Soporta hasta 24 ventanas simultáneas en la interfaz.
- Dock lateral: área de trabajo reservada en el modo maximizado y acceso a aplicaciones integrado en el logotipo de Vortex de la barra superior o en el primer icono lateral. Fullscreen oculta las barras.
- Aplicaciones: lanzador de herramientas integradas y catálogo de aplicaciones Linux presentes o instalables mediante el entorno Debian. La opción de Chromium requiere la imagen con Debian y la capa gráfica X para ejecutarse.
- Vortex Saber: seis materias (matemáticas, ciencias, historia peruana, tecnología, geografía y comunicación), 18 preguntas evaluables, explicaciones, puntuaciones, XP, niveles e insignias de dominio. Incluye un minijuego interactivo de cuatro parejas (conceptos y definiciones) con errores, puntuación y registro de las mejores marcas. El archivo de progreso es `/Documentos/Vortex-Saber-progreso.txt`. El progreso persiste entre reinicios **solo si** se activó un disco KNGFS de almacenamiento.
- Fecha y hora: lee fecha UTC del reloj de la máquina y presenta hora/calendario local, UTC-5 por defecto, equivalente a America/Lima. Guarda preferencia de huso horario y formato del reloj en la configuración persistente si el disco está activo. **Se añadió un cliente SNTP/NTP por UDP con respaldo RTC; la sincronización en red real todavía no ha sido verificada.**
- Identidad visual Vortex en la interfaz; se mantiene el nombre kng de los binarios del kernel para evitar romper compatibilidad.

## Pruebas automatizadas y evidencia

- `.github/workflows/vortex-integracion.yml`: compila ambos kernels y ejecuta pruebas de teclado, calendario de Perú y pila de red simulada.
- `.github/workflows/vortex-escritorio-qemu.yml`: crea una ISO de diagnóstico, arranca Vortex con GRUB en QEMU y ejecuta la secuencia de `tools/ci/vortex-ventanas.txt`, que captura los controles del escritorio. Para impedir falsos positivos verifica **diferencias de píxeles** entre estados; la presencia de archivos de captura sin interacción ya no es suficiente.
- Las ejecuciones y sus resultados se comprueban en la pestaña **Actions** del repositorio. Una compilación correcta no significa que el sistema haya sido validado en hardware real.

## Construcción y ejecución

Desde la raíz del repositorio en Ubuntu o WSL, instalar dependencias de compilación:

```sh
sudo apt update
sudo apt install -y build-essential gcc-multilib grub-pc-bin grub-efi-amd64-bin xorriso mtools python3
make -C kernel
make -C kernel64
bash tools/build-iso.sh
```

La ISO generada se almacena en `out/VortexOS.iso`; también se genera `out/VortexOS.iso.sha256`. Para una imagen que incluya Debian y permita instalar aplicaciones Linux:

```sh
KNG_DEBIAN=1 bash tools/build-iso.sh
```

Este último modo necesita una distribución Debian raíz preparada por los scripts del proyecto, conexión y suficiente RAM/almacenamiento; puede necesitar varios gigabytes. Elegir la opción gráfica de 64 bits desde GRUB en una VM con teclado/ratón PS/2, red virtual e1000 y 4 GB de RAM como mínimo cuando se incluye Debian. **La imagen de CI de QEMU es una prueba mínima; no incluye automáticamente Chromium ni LibreOffice.** Los instaladores y la compatibilidad con aplicaciones aún no equivalen a una estación de trabajo comercial.

Atajos: F11 pantalla completa; Esc sale de pantalla completa; F10 abre una segunda ventana de la aplicación; Ctrl+M minimizar; Ctrl+W cerrar; Ctrl+T terminal.

## Funcionalidades no resueltas

| Área | Estado |
| --- | --- |
| USB físico / controladores xHCI/EHCI y almacenamiento masivo | Sin soporte completo confirmado en equipos reales |
| Audio, aceleración gráfica, vídeo fluido, códecs y reproductores | Sin integración comprobada; no afirmar reproducción de calidad |
| Navegación moderna CSS/JavaScript/HTTPS | Requiere navegador Linux instalado (Chromium) y compatibilidad de X; el navegador HTML integrado es limitado |
| Usuarios, permisos, autenticación y aislamiento seguro | Parciales; no adecuados aún para datos sensibles |
| Compatibilidad con apps de escritorio, suite ofimática gráfica | Integración incompleta; pruebas previas de LibreOffice headless no equivalen a escritorio usable |
| Sincronización NTP y ajuste automático del reloj | Código SNTP integrado con reintento cada 6h y respaldo RTC; pendiente de validación extremo a extremo con servidor y QEMU |
| Vortex Saber: ilustraciones y evaluación avanzada | Seis materias y migración de progreso implementadas; ilustraciones, pruebas adaptativas y validación visual pendientes |
| ISO integral lista para hardware genérico | No verificada; las ISO de prueba se han ejecutado en QEMU, no en una variedad de equipos reales |
| Gestión moderna de actualizaciones, cifrado, seguridad de aplicaciones, notificaciones | Pendiente |

**Criterio:** no considerar terminado Vortex OS mientras alguno de estos requisitos no esté desarrollado y probado. No modificar una instalación o disco real sin copias de seguridad; el formato KNGFS borra el disco seleccionado.

## Historial y continuidad

Repositorio: `dreyko24/SO`. Rama histórica: `vortex-desktop-window-manager` (PR #8). Desarrollo actual: `vortex/window-management-20261114` (PR #9); NO fusionado mientras no se verifiquen los tests. Los archivos fuente están en `kernel/`, `kernel64/` y `tools/`; la documentación anterior `TRASPASO_kng_v24.md` y el README heredado preservan el historial del proyecto. Reanudar inspeccionando los resultados actuales de Actions y los errores pendientes, sin repetir el trabajo existente.

## Continuación técnica del 14 de noviembre de 2026 (PR #9)

Se implementaron los siguientes cambios **en la rama de trabajo**, todavía no integrados en `main`:
- `Ctrl+Tab` y `Ctrl+Mayús+Tab`: cambio de foco entre ventanas, incluida recuperación de minimizadas.
- SNTP: consulta asíncrona a `pool.ntp.org`, recepción UDP/123, correlación del identificador de solicitud, validación de respuesta, reintentos, nuevo intento tras 6 horas y fallback al RTC. Zona `America/Lima` UTC-5 en escritorio.
- Indicador de procedencia de reloj NTP/RTC en Ajustes.
- El botón de Chromium transmite la URL actual validada, no solamente una pantalla nueva vacía.
- Vortex Saber: materias Geografía y Comunicación, biblioteca de 3 columnas por 2 filas, 18 preguntas y migración del archivo antiguo sin perder el récord del juego.
- Simulador de escritorio actualizado para el dock lateral y variables de procesos/eventos.
- Se añadieron pruebas de estado de ventanas y evolución de registros Saber, NTP (parser y pila UDP) y se integraron en `.github/workflows/vortex-integracion.yml`.

**Evidencia disponible:** `gcc -std=c11 -O2 -Wall -Wextra -Werror tools/ntp_test.c -o /tmp/vortex-ntp-test && /tmp/vortex-ntp-test` pasó 9 de 9 comprobaciones en el entorno local de esta sesión. La compilación completa del repositorio, las pruebas host del gestor de ventanas, los nuevos trabajos de CI, QEMU y el hardware real NO se han comprobado todavía en esta sesión. No afirmar que el sistema entero funcione.

Documento completo de traspaso: [CONTINUIDAD_VORTEX_2026-11-14.md](CONTINUIDAD_VORTEX_2026-11-14.md).
