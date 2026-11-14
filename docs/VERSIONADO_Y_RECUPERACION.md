# Conservacion de avances y recuperacion de Vortex OS

La rama de trabajo activa es `cambio-nombre-vortex-os`. `main` contiene
una base historica y NO es la fuente principal para nuevas funciones.

## Guardado automatico

1. Cada avance concluido debe escribirse como commit en GitHub. No es suficiente
   cambiar archivos locales ni describir cambios en el chat.
2. El workflow `vortex-autoguardado` escucha cada `push` a la rama activa
   y crea un tag por SHA: `vortex-checkpoint-<commit SHA>`.
3. El historial y los tags no se borran ni sobrescriben al desarrollar nuevas
   funciones. El tag es **un punto de recuperacion**, NO una afirmacion de que
   la compilacion o las pruebas son exitosas.
4. Los workflows de QEMU, USB, audio, imagen y escritorio validan el commit.
   Si detectan un fallo se conserva el commit y se corrige en otro (sin reescribir
   historial ni hacer `force push`).
5. El workflow requiere que GitHub Actions tenga permiso de escritura
   `contents: write`; si la politica del repositorio lo bloquea, los
   commits permanecen en GitHub pero la creacion del tag dara error visible.

## Recuperacion

```sh
git fetch origin --tags
git log --oneline origin/cambio-nombre-vortex-os
git tag --list 'vortex-checkpoint-*'
git switch -c recuperar-vortex vortex-checkpoint-<SHA-completo>
```

No reiniciar el desarrollo desde cero: localizar el ultimo commit comprobado,
revisar sus logs en GitHub Actions y continuar en una rama nueva. Para volver
a una version anterior sin descartar trabajo, usar una nueva rama o `git revert`
sobre los commits defectuosos; evitar `git reset --hard` en la rama compartida.

## Criterios antes de publicar una ISO como estable

Se requiere compilacion de 32/64 bits, arranque QEMU, pruebas de escritorio,
memoria, USB y multimedia; probar en VMware y equipos fisicos antes de prometer
compatibilidad con todo hardware. Nunca confundir un checkpoint automatico con
una version estable.
