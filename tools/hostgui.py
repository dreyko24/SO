#!/usr/bin/env python3
"""Compila el simulador del escritorio, lo ejecuta y convierte las capturas PPM a PNG (en OUTDIR, por defecto /tmp/vortexshots)."""
import os, subprocess, glob, sys
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__)); out = os.environ.get('OUTDIR', '/tmp/vortexshots'); os.makedirs(out, exist_ok=True)
subprocess.check_call(['gcc', '-O1', '-g', '-w', '-fsanitize=address,undefined', '-DHOSTGUI', '-o', '/tmp/hostgui',
    os.path.join(here, 'hostgui.c'),
    os.path.join(here, '../kernel/image.c'),
    os.path.join(here, '../kernel/zip.c'),
    os.path.join(here, '../kernel/usb.c'),
    os.path.join(here, '../kernel/usb_storage.c'),
    os.path.join(here, '../kernel/audio.c'),
    os.path.join(here, '../kernel/video.c'),
    os.path.join(here, '../kernel/media_demo.c'),
    os.path.join(here, '../kernel/media_logo.S')
], cwd=os.path.join(here, '../kernel'))
env = dict(os.environ, OUTDIR=out, ASAN_OPTIONS='detect_leaks=0')
subprocess.check_call(['/tmp/hostgui'], env=env)
for p in sorted(glob.glob(out + '/*.ppm')):
    Image.open(p).save(p[:-4] + '.png'); os.remove(p); print(p[:-4] + '.png')
