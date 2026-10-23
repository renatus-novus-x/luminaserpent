#!/usr/bin/env python3
"""Render the AI-generated title logo (design/, C3 cinematic) into a
GRB555 header (logo_title.h) for the X68000 build.

Pipeline: crop to content -> downscale to the on-screen size ->
Bayer-dithered 5-bit-per-channel quantization -> near-black pixels
become transparent (0x0000; real pixels always have the low bit set).
Also writes design/logo_title_preview.png for a host-side check.

Usage: python3 tools/gen_logo_title.py [source-image]
"""
import sys
from PIL import Image
import numpy as np

SRC = sys.argv[1] if len(sys.argv) > 1 else \
    "design/media-generation-logo-c3-cinematic-0-15dd10f3-43bb-4632-a32f-4b007d753bf3.webp"
OUT_H = "logo_title.h"
OUT_PREVIEW = "design/logo_title_preview.png"

CROP = (60, 30, 2670, 800)     # x0, y0, x1, y1 in the 2736x912 source
DST_W = 456                    # on-screen width; height follows aspect
KEY = 16                       # max(r,g,b) below this -> transparent

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6],
                  [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0


def main():
    im = Image.open(SRC).convert("RGB").crop(CROP)
    dst_h = round(DST_W * im.height / im.width)
    im = im.resize((DST_W, dst_h), Image.LANCZOS)
    a = np.asarray(im).astype(np.float32)
    h, w, _ = a.shape
    key = a.max(axis=2) < KEY
    dith = (BAYER[np.arange(h)[:, None] % 4, np.arange(w)[None, :] % 4] - 0.5) * 8.0
    q = np.clip((a + dith[..., None]) / 8.0, 0, 31).astype(np.uint16)  # 5-bit
    words = (q[..., 1] << 11) | (q[..., 0] << 6) | (q[..., 2] << 1) | 1
    words[key] = 0
    opaque = int((~key).sum())
    print(f"logo: {w}x{h}, opaque {opaque}/{w*h} px ({100.0*opaque/(w*h):.0f}%)")

    with open(OUT_H, "w") as f:
        f.write("/* Pre-rendered title logo (AI-generated art, design/ C3),\n")
        f.write(" * quantized to GRB555 by tools/gen_logo_title.py.\n")
        f.write(" * 0x0000 = transparent. Do not edit by hand. */\n")
        f.write(f"#define LOGO_W {w}\n#define LOGO_H {h}\n")
        f.write("static const uint16_t LOGO_IMG[LOGO_W * LOGO_H] = {\n")
        flat = words.flatten()
        for i in range(0, len(flat), 12):
            f.write("  " + ",".join(f"0x{v:04X}" for v in flat[i:i + 12]) + ",\n")
        f.write("};\n")
    print(f"wrote {OUT_H}")

    prev = np.zeros((h, w, 3), dtype=np.uint8)
    prev[..., 0] = (q[..., 0] * 8).astype(np.uint8)
    prev[..., 1] = (q[..., 1] * 8).astype(np.uint8)
    prev[..., 2] = (q[..., 2] * 8).astype(np.uint8)
    prev[key] = (4, 6, 18)  # deep-space navy where transparent
    Image.fromarray(prev).resize((w * 2, h * 2), Image.NEAREST).save(OUT_PREVIEW)
    print(f"wrote {OUT_PREVIEW}")


if __name__ == "__main__":
    main()
