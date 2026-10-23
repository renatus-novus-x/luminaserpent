#!/usr/bin/env python3
"""Inject LUMINAS.X into a copy of human302.xdf (Human68k FAT12).

Usage: make_x_xdf.py <luminaserpent.x path> <output.xdf> [template.xdf]
The output boots Human68k and AUTOEXEC.BAT runs LUMINAS.X.
This is the optional .X variant; the main deliverable is the
Human68k-less boot disk made by make_boot_xdf.py.
The template is a Human68k 3.02 disk image (human302.xdf); it is not
part of this repository - pass its path, or set HUMAN68K_XDF.
"""
import os
import struct
import sys

SRC = sys.argv[3] if len(sys.argv) > 3 else \
    os.environ.get("HUMAN68K_XDF", "human302.xdf")
PAYLOAD = sys.argv[1]
DST = sys.argv[2]

BPS = 1024; RES = 1; NFAT = 2; SPF = 2; ROOTN = 192
img = bytearray(open(SRC, 'rb').read())
data = open(PAYLOAD, 'rb').read()
root_off = (RES + NFAT * SPF) * BPS
data_off = root_off + ROOTN * 32
total_clusters = (len(img) - data_off) // BPS
fat_off = RES * BPS
fat = bytearray(img[fat_off:fat_off + SPF * BPS])

def fat_get(idx):
    off = idx * 3 // 2
    v = fat[off] | (fat[off + 1] << 8)
    return (v & 0xFFF) if idx % 2 == 0 else ((v >> 4) & 0xFFF)

def fat_set(idx, val):
    off = idx * 3 // 2
    if idx % 2 == 0:
        fat[off] = val & 0xFF
        fat[off + 1] = (fat[off + 1] & 0xF0) | ((val >> 8) & 0x0F)
    else:
        fat[off] = (fat[off] & 0x0F) | ((val << 4) & 0xF0)
        fat[off + 1] = (val >> 4) & 0xFF

need = (len(data) + BPS - 1) // BPS
free = [i for i in range(2, 2 + total_clusters) if fat_get(i) == 0]
assert len(free) >= need, f"not enough free clusters: {len(free)} < {need}"
chain = free[:need]
for j, c in enumerate(chain):
    img[data_off + (c - 2) * BPS: data_off + (c - 1) * BPS] = \
        data[j * BPS:(j + 1) * BPS].ljust(BPS, b'\x00')
    fat_set(c, chain[j + 1] if j + 1 < len(chain) else 0xFFF)
img[fat_off:fat_off + SPF * BPS] = fat

slot = None
for i in range(ROOTN):
    e = img[root_off + i * 32: root_off + (i + 1) * 32]
    if e[0:11] == b'LUMINAS X  ':
        raise SystemExit("LUMINAS.X already present")
    if slot is None and e[0] in (0x00, 0xE5):
        slot = i
assert slot is not None
entry = bytearray(32)
entry[0:11] = b'LUMINAS X  '
entry[11] = 0x20
struct.pack_into("<H", entry, 26, chain[0])
struct.pack_into("<I", entry, 28, len(data))
img[root_off + slot * 32: root_off + (slot + 1) * 32] = entry

for i in range(ROOTN):
    e = img[root_off + i * 32: root_off + (i + 1) * 32]
    if e[0:11] == b'AUTOEXECBAT':
        clus = struct.unpack_from("<H", e, 26)[0]
        size = struct.unpack_from("<I", e, 28)[0]
        off = data_off + (clus - 2) * BPS
        content = img[off:off + size].rstrip(b'\x1a').rstrip(b'\r\n') \
            + b'\r\nLUMINAS.X\r\n\x1a'
        assert len(content) <= BPS
        img[off:off + BPS] = content.ljust(BPS, b'\x00')
        struct.pack_into("<I", img, root_off + i * 32 + 28, len(content))
        break
else:
    raise SystemExit("AUTOEXEC.BAT not found")

open(DST, 'wb').write(img)
print(f"wrote {DST}: size={len(img)} clusters={need} first={chain[0]} slot={slot}")
