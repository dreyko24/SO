#!/usr/bin/env bash
# Genera out/VortexOS.iso con los kernels Vortex y Firefox ESR en el modo de 64 bits.
set -e
cd "$(dirname "$0")/.."
faltan=""
for c in gcc make grub-mkrescue xorriso mformat objcopy python3; do command -v "$c" >/dev/null || faltan="$faltan $c"; done
if [ -n "$faltan" ] || [ ! -d /usr/lib/grub/i386-pc ]; then
  echo "Faltan herramientas:$faltan"
  echo "Instala con:  sudo apt update && sudo apt install -y build-essential gcc-multilib grub-pc-bin grub-efi-amd64-bin xorriso mtools python3 dosfstools fdisk debootstrap cpio gzip"
  exit 1
fi
make -C kernel clean >/dev/null; make -C kernel64 clean >/dev/null
make -C kernel      # kernel de 32 bits
make -C kernel64    # kernel de 64 bits
rm -rf build/iso && mkdir -p build/iso/boot/grub out
cp kernel/vortex-kernel.elf kernel/vortex-kernel-text.elf kernel64/vortex-kernel64.elf kernel64/vortex-kernel64-text.elf build/iso/boot/
# rootfs (programas Linux + enlazador dinamico + libc del PC) como modulo de arranque; solo lo usa el kernel de 64 bits
if python3 tools/mkrootfs.py build/iso/boot/rootfs.cpio; then ROOTFS=1; else ROOTFS=0; echo "[aviso] sin rootfs (falta libc estatica: sudo apt install libc6-dev). El ISO igual arranca, sin programas Linux."; fi
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
  cp build/debian.cpio.gz build/iso/boot/debian.cpio.gz
  python3 tools/mkrootfs.py --debian build/iso/boot/vortex-extra.cpio
else
  echo "[iso] compilación ligera sin Firefox (VORTEX_DEBIAN=0)"
fi
python3 tools/check-mb.py build/iso/boot/*.elf
# Menú de la ISO única: exactamente 64 bits gráfico y 32 bits gráfico.
# Resolucion: primero 1920x1080 y, si la tarjeta no la tiene, la siguiente de la lista. En UEFI se usa la del firmware.
VIDEO='if [ "$grub_platform" = efi ]; then set gfxpayload=keep; elif [ -n "$vortex_res" ]; then set gfxpayload=$vortex_res; else set gfxpayload=1920x1080x32,1600x900x32,1366x768x32,1280x1024x32,1280x720x32,1024x768x32,800x600x32,1920x1080x24,1366x768x24,1280x1024x24,1024x768x24,800x600x24; fi'
TEXTO='set gfxpayload=text'
R64=""; [ "$ROOTFS" = 1 ] && R64="    module /boot/rootfs.cpio"
DEB="    module /boot/debian.cpio.gz
    module /boot/vortex-extra.cpio"
entrada(){ # titulo kernel modulos video
  printf 'menuentry "%s" {\n    %s\n    multiboot /boot/%s sesion\n%s\n    boot\n}\n' "$1" "$4" "$2" "$3"; }
{
  echo 'set timeout=8'
  echo 'set default=0'
  echo 'insmod all_video'
  echo 'insmod gzio'
  # Una sola ISO: el usuario escoge el kernel gráfico de 64 o 32 bits.
  # La capa Debian (si está presente) se carga solo en modo 64 bits.
  if [ "$DEBIAN" = 1 ]; then
    entrada "Vortex OS - 64 bits (grafico)" vortex-kernel64.elf "$DEB" "$VIDEO"
  else
    entrada "Vortex OS - 64 bits (grafico)" vortex-kernel64.elf "$R64" "$VIDEO"
  fi
  entrada "Vortex OS - 32 bits (grafico)" vortex-kernel.elf "" "$VIDEO"
} > build/iso/boot/grub/grub.cfg
# La ISO de evaluación no modifica el disco. Para activar el instalador
# experimental (tercera entrada) se requiere VORTEX_INSTALADOR=1 explícito.
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
echo "PC real o VM: mejor en BIOS/Legacy (CSM). RAM: 256 MB o mas (con Debian: 4 GB o mas). Red: solo tarjetas Intel e1000/RTL8139 (en PC real puede no haber red)."
echo "USB booteable: Rufus en modo \"imagen DD\" o balenaEtcher."
