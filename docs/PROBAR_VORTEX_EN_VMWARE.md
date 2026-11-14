# Vortex OS: pruebas reales en VMware (14 de noviembre de 2026)

Estas instrucciones distinguen **que exista un archivo** de **que el
hardware virtual esté soportado**. No habilitan por sí mismas
controladores que aún no existen.

## Audio: el WAV sí existe

En **Archivos > Música** hay dos archivos generados en el arranque:
- `Prueba-de-audio.wav`: WAV PCM mono, 8 bits, 8 kHz, ~2 segundos.
- `Prueba-de-audio-16bits.wav`: WAV PCM mono, 16 bits, 22,05 kHz, ~2 segundos.

Si el reproductor dice **Sin AC'97 ni SB16**, el archivo está presente
pero **Vortex no detecta una tarjeta de sonido que pueda manejar**.

En VMware Workstation, apagar por completo la VM (no suspender), cerrar
VMware, hacer una copia del archivo `.vmx` y configurar:

```ini
sound.present = "TRUE"
sound.virtualDev = "sb16"
```

Comprobar que el dispositivo de sonido esté conectado a la VM al arrancar.
Luego abrir el WAV. Alternativa verificada en las pruebas automatizadas:
QEMU con `-device AC97` y un `-audiodev` compatible.

Vortex aún **no tiene controlador de audio HDA ni ES1371**. Por tanto, una
VM VMware con el dispositivo de sonido predeterminado puede seguir sin
audio hasta elegir SB16. Esto es distinto de que el WAV esté corrupto.

## USB: la unidad Kingston de Windows NO está dentro de la VM automáticamente

Ver la unidad E: en Windows demuestra que el anfitrión la detecta, **no
que VMware la haya conectado al invitado**.

1. Apagar la VM, cambiar en VMware la **compatibilidad USB a 1.1** cuando
   la configuración lo permita (Vortex usa UHCI).
2. Iniciar Vortex. Conectar la memoria USB desde
   **VM > Removable Devices > [Kingston] > Connect (Disconnect from host)**.
   Puede desaparecer de E: en Windows mientras esté conectada al invitado.
3. En Vortex, abrir **Multimedia > USB > Buscar USB conectado**.
4. Si el lector termina correctamente, usar **Abrir dispositivo** o
   **Archivos > Dispositivos > Unidad-USB**. No modifica el USB.

Compatibilidad actual: **USB Mass Storage BOT/SCSI sobre UHCI**,
sistemas de archivos **FAT32 y exFAT** en modo lectura, límite
de **4 MiB por archivo** importado y máximo aproximado de 100 archivos.
La integración exFAT se prueba con disco emulado; queda pendiente
validarla con la memoria Kingston real.

No admite aún **USB 2.0 EHCI, USB 3.0 xHCI, NTFS**, ni todos los
esquemas de particiones GPT. Si la Kingston está en NTFS o VMware
presenta solo EHCI/xHCI, Vortex todavía no puede leerla.
**No reformatear** la memoria con documentos personales solo para probar.

## Qué tipo de archivos podrás abrir

- **JPG/PNG/BMP/PPM:** visor nativo (según límites del decodificador).
- **WAV PCM 8/16 bits:** reproductor nativo si hay AC'97 o SB16.
- **AVI RGB sin compresión:** reproductor de vídeo nativo.
- **TXT/MD y formatos de texto simples:** bloc de notas.
- **DOCX/PPTX/XLSX/PDF:** requieren aplicaciones externas
  compatibles instaladas y funcionando en modo 64 bits;
  **LibreOffice no viene preinstalado** en la ISO de evaluación.
- **MP4/H.264:** por ahora **sin decodificador nativo**, aunque el
  explorador muestre su nombre.

## Controles y apariencia

Cada ventana tiene **minimizar, maximizar/restaurar y cerrar**.
Las ventanas minimizadas pueden abrirse en la barra superior;
desde el dock también se recuperan las aplicaciones del dock.

En **Ajustes > Apariencia**, seleccionar **Claro** u **Oscuro**, y
**Español**, **English** o **Quechua**. El código de idioma también se
puede elegir en el indicador de la barra superior. La distribución
del teclado (US/Latam/España) es una preferencia diferente en
**Ajustes > General**.

La localización quechua es un **primer glosario de interfaz basado en
quechua sureño**; queda trabajo pendiente para traducción completa,
revisión lingüística y textos generados por otras aplicaciones.

## Evaluación de la ISO

Al descargar una ISO, comprobar que la etiqueta de GitHub Release
coincida con el commit que se desea probar; las ISOs antiguas **no se
actualizan solas**. El workflow `vortex-iso-unica` construye desde el
commit exacto, verifica arranque gráfico 32/64 bits y publica su
`VortexOS.iso` y `VortexOS.iso.sha256`.
