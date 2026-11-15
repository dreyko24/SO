# Archivo histórico de Vortex OS

Esta carpeta recupera documentos y pruebas que habían quedado solo en ramas antiguas durante la integración del proyecto. **No contiene un segundo sistema operativo** y **no es el árbol de compilación vigente**.

El Vortex OS activo se desarrolla en la rama **`main`**. Leer primero [`AGENTS.md`](../../AGENTS.md), [`VORTEX_ESTADO.md`](../../VORTEX_ESTADO.md) y [`README.md`](../../README.md).

## Contenido preservado

- `VORTEX_ESTADO_2026-11-14.md`: informe original escrito en la rama antigua `vortex/window-management-20261114`. Las cifras de funciones y pruebas pueden quedar obsoletas.
- `window_state_test_2026-11-14.c`: prueba de ventanas y Saber anterior a la nueva enciclopedia de 31 artículos. **No se añade a la CI actual**, pues sus símbolos ya no corresponden a todos los componentes del escritorio vigente.
- `time_test_legacy.c`: comprobaciones antiguas de calendario, sustituidas por el esquema de pruebas de `tests/`.
- `workflows/`: archivos de CI antiguos que solo se conservaron como referencias. Los trabajos activos están en [`.github/workflows/`](../../.github/workflows/).

En la raíz se conservaron los archivos `TRASPASO_kng_v17.md`, `v18.md`, `v19.md`, `v21.md`, `v22.md`, `v23.md`, `v24.md` y `CONTINUIDAD_VORTEX_2026-11-14.md`. Son testimonios de ingeniería y no instrucciones para reinstalar el kernel antiguo.

**No fusionar ni copiar a ciegas estas fuentes sobre `main`.** Los PR históricos se integraron de distintas formas, incluidas fusiones squash; una rama adelantada o divergente no significa automáticamente que todo su código falte. Auditar archivos y diferencias semánticas antes de trasladar algo que aún sea útil.
