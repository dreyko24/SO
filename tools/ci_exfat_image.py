#!/usr/bin/env python3
"""Minimal 512-byte-sector exFAT USB fixture for QEMU UHCI/BOT CI.
Creates a real on-disk boot sector, FAT, cluster heap and UTF-16 filename.
This is a test image; it is not intended to replace Windows mkfs.exfat.
"""
from pathlib import Path
import struct
import sys

def le16(buf, pos, value):
    struct.pack_into("<H", buf, pos, value)
def le32(buf, pos, value):
    struct.pack_into("<I", buf, pos, value)
def le64(buf, pos, value):
    struct.pack_into("<Q", buf, pos, value)

def make(destination, source):
    data = Path(source).read_bytes()
    assert 1 <= len(data) <= 4096
    total = 32768
    sectors_per_cluster = 8
    fat_start, fat_length, heap_start, count = 24, 128, 300, 1000
    with open(destination, "wb") as file:
        file.truncate(total * 512)
        boot = bytearray(512)
        boot[0:3] = b"\\xeb\\x76\\x90"
        boot[3:11] = b"EXFAT   "
        le64(boot, 0x48, total)
        le32(boot, 0x50, fat_start)
        le32(boot, 0x54, fat_length)
        le32(boot, 0x58, heap_start)
        le32(boot, 0x5c, count)
        le32(boot, 0x60, 2)
        boot[0x6c] = 9
        boot[0x6d] = 3
        boot[0x6e] = 1
        boot[0x6f] = 0x80
        boot[510:512] = b"\\x55\\xaa"
        file.seek(0)
        file.write(boot)
        fat = bytearray(512)
        le32(fat, 0, 0xfffffff8)
        le32(fat, 4, 0xffffffff)
        le32(fat, 8, 0xffffffff)
        le32(fat, 12, 0xffffffff)
        file.seek(fat_start * 512)
        file.write(fat)
        root = bytearray(512)
        root[0] = 0x85;root[1] = 2
        le16(root, 4, 0x20)
        root[32] = 0xc0;root[33] = 3
        name = "VORTEX.PNG"
        root[35] = len(name)
        le32(root, 32 + 20, 3)
        le64(root, 32 + 24, len(data))
        root[64] = 0xc1
        for index, char in enumerate(name):
            le16(root, 66 + index * 2, ord(char))
        file.seek(heap_start * 512)
        file.write(root)
        file.seek((heap_start + sectors_per_cluster) * 512)
        file.write(data)
    print(f"exFAT fixture: {destination}: VORTEX.PNG ({len(data)} bytes)")
if __name__ == "__main__":
    make(sys.argv[1], sys.argv[2])
