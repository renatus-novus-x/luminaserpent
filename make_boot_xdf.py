#!/usr/bin/env python3
"""Create a bootable XDF whose only file is HUMAN.SYS (given system binary).

Based on the hellosys sample in elf2x68k-sample: the X68000 IPL boot
sector loads HUMAN.SYS, which must sit in contiguous sectors starting
at cluster 2.  We take the boot sector / BPB / volume label from the
verified human302.xdf template, free every other cluster, and write
the given binary as a single contiguous HUMAN.SYS.

Usage: make_boot_xdf.py <system-binary> <output.xdf> [template.xdf]
"""
import os
import struct
import sys

BPS = 1024          # bytes per sector / cluster (1 sector = 1 cluster)
RES = 1             # reserved sectors (boot sector)
NFAT = 2
SPF = 2             # sectors per FAT
ROOTN = 192         # root directory entries

sys_path = sys.argv[1]
out_path = sys.argv[2]
template = sys.argv[3] if len(sys.argv) > 3 else \
    os.environ.get("HUMAN68K_XDF", "human302.xdf")

img = bytearray(open(template, 'rb').read())
payload = open(sys_path, 'rb').read()

fat_off = RES * BPS
root_off = (RES + NFAT * SPF) * BPS
data_off = root_off + ROOTN * 32
total_clusters = (len(img) - data_off) // BPS + 2   # clusters 2..max

need = (len(payload) + BPS - 1) // BPS
assert need <= total_clusters - 2, f"payload too large: {need} clusters"

# --- rebuild FAT (12-bit), one contiguous chain starting at cluster 2
fat = bytearray(SPF * BPS)

def fat_set(idx, val):
    off = idx * 3 // 2
    if idx % 2 == 0:
        fat[off] = val & 0xFF
        fat[off + 1] = (fat[off + 1] & 0xF0) | ((val >> 8) & 0x0F)
    else:
        fat[off] = (fat[off] & 0x0F) | ((val << 4) & 0xF0)
        fat[off + 1] = (val >> 4) & 0xFF

fat_set(0, 0xF00 | img[fat_off])       # media byte from template BPB/FAT
fat_set(1, 0xFFF)
for j in range(need):
    fat_set(2 + j, 2 + j + 1 if j + 1 < need else 0xFFF)
for n in range(NFAT):
    img[fat_off + n * SPF * BPS: fat_off + (n + 1) * SPF * BPS] = fat

# --- root directory: keep volume label (entry 0), HUMAN.SYS as entry 1
label = bytes(img[root_off: root_off + 32])
img[root_off: root_off + ROOTN * 32] = b'\x00' * (ROOTN * 32)
img[root_off: root_off + 32] = label
entry = bytearray(32)
entry[0:11] = b'HUMAN   SYS'
entry[11] = 0x27                        # read-only/hidden/system/archive
struct.pack_into("<H", entry, 26, 2)    # first cluster = 2
struct.pack_into("<I", entry, 28, len(payload))
img[root_off + 32: root_off + 64] = entry

# --- data area: clear, then write payload contiguously from cluster 2
img[data_off:] = b'\x00' * (len(img) - data_off)
img[data_off: data_off + len(payload)] = payload

open(out_path, 'wb').write(img)
print(f"wrote {out_path}: payload={len(payload)} bytes, "
      f"clusters={need} (2..{2 + need - 1}), image={len(img)} bytes")
