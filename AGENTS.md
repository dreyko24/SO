# Instrucciones permanentes para continuar Vortex OS

**Repositorio oficial:** `https://github.com/dreyko24/SO`  
**Única línea de desarrollo vigente:** rama `main`  
**Producto:** **Vortex OS**, un sistema operativo propio de 32/64 bits. El nombre histórico `kng` puede permanecer en identificadores y archivos por compatibilidad; **no** renombrar indiscriminadamente símbolos del kernel.

## Regla fundamental: NO REINICIAR EL PROYECTO

Este repositorio ya contiene años/versiones de desarrollo y múltiples ramas históricas. No crear un SO nuevo, un escritorio simulado ni copiar un kernel antiguo sobre el actual. **Siempre leer el estado vigente de `main` antes de programar.** Si el usuario dice «continúa», trabajar en los archivos existentes, comprobar resultados y conservar los cambios.

Para comenzar cualquier sesión/chat:

1. Leer `AGENTS.md`, `README.md`, `VORTEX_ESTADO.md` y `docs/AUDITORIA_FUNCIONAL_VORTEX.md` de **`main`** en GitHub; revisar `git log` o últimos commits. El estado puede cambiar entre chats.
2. Inspeccionar el árbol actual: `kernel/`, `kernel64/`, `tools/`, `tests/`, `docs/`, `.github/workflows/`.
3. Consultar las ramas y PR recientes. **Muchas ramas ya fueron fusionadas mediante squash**: GitHub puede mostrar `ahead` o `diverged` aunque las funcionalidades estén presentes en `main`. Comparar contenido y tests; no intentar fusionar a ciegas una rama obsoleta.
4. Probar antes de fusionar: compilar ambos kernels y ejecutar los trabajos de GitHub Actions de escritorio, red, USB, audio, vídeo y QEMU correspondientes. No confundir pruebas con hardware real.
5. Integrar solo las mejoras comprobadas en `main`. Las ramas son áreas de trabajo temporales, **no sistemas operativos distintos**. No borrar ramas históricas ni archivos anteriores sin demostrar que son redundantes.
6. Continuar con los problemas aún pendientes (USB moderno, seguridad de aplicaciones y usuarios, navegación HTTPS moderna, codecs multimedia, drivers físicos, rendimiento). **No pedir confirmaciones repetidas ni entregar planes como si fueran implementaciones.**

## Archivos de referencia y procedencia

- `README.md`: sistema actual, instrucciones de ISO/QEMU y limitaciones.
- `VORTEX_ESTADO.md`: mapa **vigente** de arquitectura, integración y limitaciones.
- `docs/AUDITORIA_FUNCIONAL_VORTEX.md`: evidencia técnica de componentes.
- `TRASPASO_kng_v17.md` a `TRASPASO_kng_v24.md`: documentos históricos recuperados. **No usar su estado antiguo como descripción de la última versión**.
- `CONTINUIDAD_VORTEX_2026-11-14.md`: informe histórico de una rama anterior; preservado por trazabilidad. Sus afirmaciones de PR abiertos pueden estar obsoletas.
- `docs/historico/`: documentación y pruebas antiguas preservadas para evitar pérdida de contexto.

## Evidencia y compilación

- `bash tools/verificar-vortex.sh`: construye ambos kernels y ejecuta pruebas nativas de componentes en host Linux con dependencias.
- `bash tools/build-iso.sh`: crea `out/VortexOS.iso`; revisar `README.md` sobre Debian opcional.
- `Probar-VortexOS.cmd` / `bash tools/probar-vortex.sh`: QEMU Windows/Linux. Permiten una imagen USB FAT32/exFAT opcional **de solo lectura**.
- En Actions, revisar `vortex-unificacion-integracion`, `vortex-escritorio-host`, `vortex-iso-unica`, `arranque-qemu`, `vortex-usb-mass-storage`, `vortex-audio-sb16` y `vortex-reproductores`. Comprobar jobs de la revisión concreta, no resultados viejos.

**No afirmar que el SO está terminado ni que una característica es funcional en hardware físico sin haberla probado.** La evidencia de QEMU no equivale a USB físico, PCI real o compatibilidad universal.

## Continuidad entre chats

El usuario pidió **un informe de traspaso detallado solo cuando el chat se aproxime a su límite**, no en cada entrega. Durante el desarrollo, dejar commits verificables y actualizar el estado canónico **dentro de GitHub** cuando cambie de verdad. Si el chat está por agotarse, dar al usuario un informe con: `main` SHA actual, PR abiertos, archivos modificados, pruebas exactas y sus resultados, ISO descargable si está verificada, limitaciones y próximo trabajo. Una conversación nueva debe leer GitHub y proseguir, no reiniciar el desarrollo.
