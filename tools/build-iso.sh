#!/usr/bin/env bash
# Genera out/VortexOS.iso con el kernel Vortex x86_64 y Firefox ESR.
set -eo pipefail
cd "$(dirname "$0")/.."
faltan=""
for c in gcc make grub-mkrescue grub-script-check xorriso mformat objcopy python3; do command -v "$c" >/dev/null || faltan="$faltan $c"; done
if [ -n "$faltan" ] || [ ! -d /usr/lib/grub/i386-pc ]; then
  echo "Faltan herramientas:$faltan"
  echo "Instala con:  sudo apt update && sudo apt install -y build-essential gcc-multilib grub-pc-bin grub-efi-amd64-bin xorriso mtools python3 dosfstools fdisk debootstrap cpio gzip"
  exit 1
fi
make -C kernel64 clean >/dev/null
make -C kernel64
rm -rf build/iso && mkdir -p build/iso/boot/grub out
cp kernel64/vortex-kernel64.elf build/iso/boot/
# Debian real para Firefox: la cache lleva huella del constructor y de los
# archivos del rootfs. No reutilizar un Debian antiguo sin Firefox/X11.
DEBIAN="${VORTEX_DEBIAN:-1}"
if [ "$DEBIAN" = 1 ]; then
  DEB_KEY=$( {
    sha256sum tools/mkdebian.sh
    find tools/rootfs-es -type f -print0 | LC_ALL=C sort -z | xargs -0 -r sha256sum
  } | sha256sum | cut -c1-24 )
  CACHE_DEB="$HOME/.cache/vortex/debian-$DEB_KEY.cpio.gz"
  STAMP=build/debian.source-key
  if [ -f build/debian.cpio.gz ] && { [ ! -f "$STAMP" ] || [ "$(cat "$STAMP")" != "$DEB_KEY" ]; }; then
    echo "[debian] descartando modulo obsoleto, se requiere construir la version actual"
    rm -f build/debian.cpio.gz
  fi
  if [ ! -f build/debian.cpio.gz ]; then
    if [ -f "$CACHE_DEB" ] && gzip -t "$CACHE_DEB" 2>/dev/null; then
      cp "$CACHE_DEB" build/debian.cpio.gz
      echo "[debian] cache validada para el codigo actual"
    else
      bash tools/mkdebian.sh
      gzip -t build/debian.cpio.gz
      mkdir -p "$(dirname "$CACHE_DEB")"
      cp build/debian.cpio.gz "$CACHE_DEB"
    fi
  fi
  printf '%s\n' "$DEB_KEY" > "$STAMP"
  ln build/debian.cpio.gz build/iso/boot/debian.cpio.gz ||
    cp build/debian.cpio.gz build/iso/boot/debian.cpio.gz
  python3 tools/mkrootfs.py --debian build/iso/boot/vortex-extra.cpio
else
  echo "[iso] compilación ligera sin Firefox (VORTEX_DEBIAN=0)"
  if python3 tools/mkrootfs.py build/iso/boot/rootfs.cpio; then
    LIGHT_MODULE='    module /boot/rootfs.cpio'
  else
    rm -f build/iso/boot/rootfs.cpio
    echo "[aviso] sin programas Linux (instala libc6-dev para construir el rootfs ligero)"
  fi
fi
python3 tools/check-mb.py build/iso/boot/vortex-kernel64.elf
# Resolucion: primero 1920x1080 y, si la tarjeta no la tiene, la siguiente de la lista. En UEFI se usa la del firmware.
VIDEO='if [ "$grub_platform" = efi ]; then set gfxpayload=keep; elif [ -n "$vortex_res" ]; then set gfxpayload=$vortex_res; else set gfxpayload=1920x1080x32,1600x900x32,1366x768x32,1280x1024x32,1280x720x32,1024x768x32,800x600x32,1920x1080x24,1366x768x24,1280x1024x24,1024x768x24,800x600x24; fi'
{
  echo 'set timeout=8'
  echo 'set default=0'
  echo 'insmod all_video'
  echo 'insmod gzio'
  echo 'menuentry "Vortex OS - 64 bits (grafico)" {'
  printf '    %s\n' "$VIDEO"
  if [ "$DEBIAN" = 1 ]; then
    # GRUB puede arrancar pese a que un comando module previo haya fallado.
    # Para esta edición ambos módulos son necesarios.
    cat <<'GRUB'
    if multiboot /boot/vortex-kernel64.elf sesion; then
        if module /boot/debian.cpio.gz; then
            if module /boot/vortex-extra.cpio; then
                boot
            fi
        fi
    fi
    if serial --unit=0 --speed=115200; then
        terminal_output --append serial
    fi
    echo 'No se pudo cargar el entorno de Firefox.'
    echo 'Asigna 4 GB de RAM a la VM y comprueba VortexOS.iso.'
    # Mantener el aviso en pantalla. GRUB puede intentar un arranque implícito al salir.
    while true; do
        sleep 3600
    done
GRUB
  else
    echo '    multiboot /boot/vortex-kernel64.elf sesion'
    printf '%s\n' "$LIGHT_MODULE"
    echo '    boot'
  fi
  echo '}'
} > build/iso/boot/grub/grub.cfg
# El instalador experimental de 64 bits requiere VORTEX_INSTALADOR=1.
# Instalador: imagen de disco arrancable (MBR + GRUB BIOS/UEFI + FAT32) que vortex copia al disco duro de la PC.
if [ "$VORTEX_INSTALADOR" = 1 ] && bash tools/mkdisk.sh; then
  DEBIAN=$DEBIAN python3 - <<'PY'
import os
p='build/iso/boot/grub/grub.cfg'; s=open(p).read()
e=("menuentry \"INSTALAR Vortex OS en el disco duro de esta PC\" {\n"
   "    if [ \"$grub_platform\" = efi ]; then set gfxpayload=keep; else set gfxpayload=1024x768x32,1280x1024x32,1366x768x32,800x600x32,1024x768x24,800x600x24; fi\n"
   "    echo \"Cargando el instalador (puede tardar 1 o 2 minutos)...\"\n"
   "    multiboot /boot/vortex-kernel64.elf instalador\n"
   + ("    module /boot/debian.cpio.gz\n    module /boot/vortex-extra.cpio\n" if os.environ.get("DEBIAN")=="1" else "") +
   "    module /boot/disco.img.gz\n"
   "    boot\n}\n")
i=s.index("menuentry"); s=s[:i]+e+s[i:]
open(p,'w').write(s)
PY
  echo "[iso] con instalador en el disco duro"
else echo "[iso] imagen para VM: sin instalador peligroso (opcional VORTEX_INSTALADOR=1)"; rm -f build/iso/boot/disco.img.gz; fi
grub-script-check build/iso/boot/grub/grub.cfg
# ISO hibrida: arranca con BIOS/Legacy y, si esta instalado grub-efi-amd64-bin, tambien en PCs solo UEFI.
# (En UEFI sin "CSM/Legacy" algunos PC no emulan teclado/raton PS/2: si no responden, activa Legacy/CSM en la BIOS.)
if [ -d /usr/lib/grub/x86_64-efi ]; then
  grub-mkrescue -o out/VortexOS.iso build/iso 2>&1 | tail -3
else
  echo "[aviso] sin grub-efi-amd64-bin: el ISO solo arrancara en modo BIOS/Legacy (sudo apt install grub-efi-amd64-bin)"
  grub-mkrescue -d /usr/lib/grub/i386-pc -o out/VortexOS.iso build/iso 2>&1 | tail -3
fi
( cd out && sha256sum VortexOS.iso > VortexOS.iso.sha256 )
ls -la out/VortexOS.iso
echo "Listo: out/VortexOS.iso"
echo "Usa una máquina virtual de 64 bits con 4 GB de RAM para ejecutar Firefox."
echo "USB booteable: Rufus en modo \"imagen DD\" o balenaEtcher."
