@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "ISO=%~1"
set "USB_IMG=%~2"
if not defined ISO if exist "VortexOS-QEMU-BIOS.iso" set "ISO=VortexOS-QEMU-BIOS.iso"
if not defined ISO if exist "out\VortexOS.iso" set "ISO=out\VortexOS.iso"
if not defined ISO (
    echo [VORTEX] No se encontro ninguna ISO.
    echo Extrae VortexOS-QEMU-BIOS.iso desde el artefacto de GitHub Actions,
    echo o compila out\VortexOS.iso con Construir-ISO.cmd.
    echo Tambien puedes pasar la ruta: Probar-VortexOS.cmd "C:\ruta\VortexOS.iso"
    exit /b 1
)
if not exist "%ISO%" (
    echo [VORTEX] No existe la ISO: "%ISO%"
    exit /b 1
)
if defined USB_IMG if not exist "%USB_IMG%" (
    echo [VORTEX] No existe la imagen USB FAT32: "%USB_IMG%"
    exit /b 1
)
set "QEMU=qemu-system-x86_64.exe"
where qemu-system-x86_64.exe >nul 2>&1
if errorlevel 1 (
    if exist "%ProgramFiles%\qemu\qemu-system-x86_64.exe" (
        set "QEMU=%ProgramFiles%\qemu\qemu-system-x86_64.exe"
    ) else (
        echo [VORTEX] Falta QEMU: instala QEMU para Windows y agrega la carpeta al PATH.
        exit /b 1
    )
)
echo [VORTEX] Probando ISO: "%ISO%"
echo [VORTEX] RAM 4 GB, video std, red Intel e1000, audio AC97, USB UHCI, reloj UTC, BIOS.
if defined USB_IMG (
    echo [VORTEX] Adjuntando USB virtual FAT32 solo lectura: "%USB_IMG%"
    "%QEMU%" -name "Vortex OS - prueba" -m 4096 -smp 2 -boot d -drive "file=%ISO%,media=cdrom,if=ide,readonly=on" -vga std -nic user,model=e1000 -device AC97 -usb -rtc base=utc -no-reboot -drive "if=none,id=vortexusb,format=raw,file=%USB_IMG%,readonly=on" -device "usb-storage,drive=vortexusb,bus=usb-bus.0"
) else (
    "%QEMU%" -name "Vortex OS - prueba" -m 4096 -smp 2 -boot d -drive "file=%ISO%,media=cdrom,if=ide,readonly=on" -vga std -nic user,model=e1000 -device AC97 -usb -rtc base=utc -no-reboot
)
exit /b %errorlevel%
