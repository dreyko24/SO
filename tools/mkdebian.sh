#!/usr/bin/env bash
# Crea build/debian.cpio.gz: Debian 12 (bookworm) con Firefox ESR y Xvfb PREINSTALADOS.
# El entorno Debian vive en la ISO de 64 bits; instalar navegadores en RAM
# durante una sesion no los conserva despues de reiniciar.
# Se usa como modulo de arranque de vortex 64 bits. Necesita sudo y debootstrap:  sudo apt install -y debootstrap cpio gzip
set -e
cd "$(dirname "$0")/.."
for c in debootstrap cpio gzip sudo; do command -v "$c" >/dev/null || { echo "Falta '$c'. Instala con:  sudo apt update && sudo apt install -y debootstrap cpio gzip"; exit 1; }; done
RAIZ=${VORTEX_DEBROOT:-/var/tmp/vortex-debroot}   # disco de Linux (en /mnt/c, NTFS, debootstrap falla)
sudo rm -rf "$RAIZ"; mkdir -p build
echo "[1/4] Descargando Debian base (unos minutos, necesita internet)..."
# Evitar que debootstrap configure Firefox y servicios D-Bus antes de tener
# /dev, /proc y politica de servicios: debootstrap fallaba con dconf-service.
sudo debootstrap --variant=minbase \
  --include=busybox,curl,ca-certificates,xz-utils,less,nano,procps,xauth,gnupg \
  bookworm "$RAIZ" http://deb.debian.org/debian > build/debootstrap.log 2>&1 || {
    tail -65 build/debootstrap.log
    sudo test -f "$RAIZ/debootstrap/debootstrap.log" && sudo tail -85 "$RAIZ/debootstrap/debootstrap.log" || true
    exit 1
  }
echo "[2/4] Instalando Firefox ESR y Xvfb en Debian ya configurado..."
sudo mkdir -p "$RAIZ"/dev "$RAIZ"/proc "$RAIZ"/run "$RAIZ"/root "$RAIZ"/tmp "$RAIZ"/var/lib/xvfb "$RAIZ"/tmp/.X11-unix
sudo tee "$RAIZ"/etc/apt/sources.list >/dev/null <<'L'
deb http://deb.debian.org/debian bookworm main
deb http://deb.debian.org/debian bookworm-updates main
deb http://security.debian.org/debian-security bookworm-security main
L
sudo tee "$RAIZ"/usr/sbin/policy-rc.d >/dev/null <<'POLICY'
#!/bin/sh
exit 101
POLICY
sudo chmod 0755 "$RAIZ"/usr/sbin/policy-rc.d
cleanup_chroot(){
  sudo umount -lf "$RAIZ/proc" 2>/dev/null || true
  sudo umount -lf "$RAIZ/dev" 2>/dev/null || true
}
trap cleanup_chroot EXIT
sudo mount --bind /dev "$RAIZ/dev"
sudo mount -t proc proc "$RAIZ/proc"
sudo chroot "$RAIZ" /usr/bin/env DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true apt-get update > build/vortex-apt.log 2>&1 || {
  echo "FALLO apt-get update en chroot"; tail -95 build/vortex-apt.log; exit 1;
}
sudo chroot "$RAIZ" /usr/bin/env DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
  apt-get -y --no-install-recommends install \
    firefox-esr xvfb xterm x11-utils x11-xserver-utils fonts-dejavu-core fonts-liberation libgl1 >> build/vortex-apt.log 2>&1 || {
  echo "FALLO instalando navegador/servidor X en chroot"; tail -120 build/vortex-apt.log; exit 1;
}
cleanup_chroot
trap - EXIT
sudo rm -f "$RAIZ/usr/sbin/policy-rc.d"
echo "Agregando comandos de Vortex y verificando ejecutables..."
sudo cp -a tools/rootfs-es/. "$RAIZ"/
sudo chmod 0755 "$RAIZ/usr/local/bin/vortex-chromium"
sudo chmod 1777 "$RAIZ"/tmp "$RAIZ"/tmp/.X11-unix
echo "vortex" | sudo tee "$RAIZ"/etc/hostname >/dev/null
sudo test -e "$RAIZ/usr/bin/firefox-esr" || { echo "FALLO: Firefox ESR ausente"; exit 1; }
sudo test -e "$RAIZ/usr/bin/Xvfb" || { echo "FALLO: Xvfb ausente"; exit 1; }
sudo rm -rf "$RAIZ"/var/lib/apt/lists/* "$RAIZ"/var/cache/apt/*.bin "$RAIZ"/var/cache/apt/archives/*.deb "$RAIZ"/usr/share/doc/* "$RAIZ"/usr/share/man/* "$RAIZ"/usr/share/info/*
echo "[3/4] Empaquetando..."
( cd "$RAIZ" && sudo find . | sudo cpio -o -H newc --quiet ) | gzip -6 > build/debian.cpio.gz
echo "[4/4] Listo: build/debian.cpio.gz ($(du -h build/debian.cpio.gz | cut -f1))"
