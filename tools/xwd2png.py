#!/usr/bin/env python3
"""Convierte un archivo XWD (el que escribe Xvfb con -fbdir) a PNG, solo con la biblioteca estandar. Uso: xwd2png.py entrada.xwd salida.png"""
import argparse
import struct
import zlib

def main(src, dst, expected_rgb=None):
    d = open(src, 'rb').read()
    h = struct.unpack('>25I', d[:100])
    hsize, ver, fmt, depth, w, hgt, xoff, border, bunit, bbo, bpad, bpp, bpl, vclass, rm, gm, bm, bprgb, cme, ncol = h[:20]
    off = hsize + ncol * 12
    pix = d[off:]
    if fmt != 2 or w == 0 or hgt == 0 or bpp not in (24, 32) or len(pix) < hgt * bpl:
        raise SystemExit("XWD no compatible o truncado")
    out = bytearray()
    visibles = 0
    coinciden = 0
    sh = lambda m: (m & -m).bit_length() - 1
    rs, gs, bs = sh(rm), sh(gm), sh(bm)
    for y in range(hgt):
        row = pix[y * bpl:(y + 1) * bpl]
        out.append(0)
        for x in range(w):
            p = int.from_bytes(row[x * (bpp // 8):(x + 1) * (bpp // 8)], 'little' if border == 0 else 'big')
            rgb = ((p & rm) >> rs, (p & gm) >> gs, (p & bm) >> bs)
            visibles += (rgb != (0, 0, 0))
            coinciden += (rgb == expected_rgb)
            out += bytes(rgb)
    def chunk(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, hgt, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(out), 6)) + chunk(b'IEND', b'')
    open(dst, 'wb').write(png)
    total = w * hgt
    print('PNG', w, 'x', hgt, 'bytes', len(png),
          '| pixeles visibles:', visibles, '/', total)
    # Una imagen negra no valida el render aunque Xvfb y Firefox existan.
    if visibles < max(1024, total // 100):
        raise SystemExit('AVISO: framebuffer negro o menos de 1% de pixeles visibles; Firefox NO renderizado')
    if expected_rgb is not None:
        color = bytes(expected_rgb).hex()
        print(f'Pixeles RGB #{color}: {coinciden} / {total}')
        if coinciden * 100 < total:
            raise SystemExit(f'Color #{color} presente en menos del 1% del framebuffer')


def rgb_hex(value):
    try:
        rgb = bytes.fromhex(value)
    except ValueError:
        rgb = b''
    if len(rgb) != 3 or len(value) != 6:
        raise argparse.ArgumentTypeError('se requieren seis digitos hexadecimales RRGGBB')
    return tuple(rgb)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('entrada')
    parser.add_argument('salida')
    parser.add_argument('--expect-rgb', type=rgb_hex, metavar='RRGGBB',
                        help='exige el color indicado en al menos el 1%% de los pixeles')
    args = parser.parse_args()
    main(args.entrada, args.salida, args.expect_rgb)
