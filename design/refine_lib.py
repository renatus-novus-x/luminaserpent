#!/usr/bin/env python3
"""Shared helpers for the LUMINA SERPENT X68000 art-refinement boards.

Designs are drawn as RGB arrays at native sprite resolution using only
GRB555-representable colours (5 bits per channel), so what the board
shows is exactly what the X68000 can display.  Index/palette mapping
happens later, at implementation time."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont

MSX_DIR = "/home/hatch/workspace/x68k/lumina-msx-assets/luminaserpent-assets/sprites"
BG = (10, 14, 28)


def q(c):
    """Quantise an (r,g,b) 0..255 colour to GRB555 display values."""
    return tuple(min(255, (v >> 3) * 8 + 4) for v in c)


def hx(s):
    s = s.lstrip('#')
    return q(tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)))


def ramp(*cols):
    """Colour ramp from hex strings, darkest first."""
    return [hx(c) for c in cols]


def canvas(w, h):
    return np.zeros((h, w, 4), dtype=np.uint8)


def mask_of(w, h, draw_fn):
    im = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(im)
    draw_fn(d)
    return np.asarray(im) > 0


def poly_mask(w, h, pts):
    return mask_of(w, h, lambda d: d.polygon(pts, fill=255))


def disc_mask(w, h, cx, cy, r):
    return mask_of(w, h, lambda d: d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255))


def ring_mask(w, h, cx, cy, r_out, r_in):
    return disc_mask(w, h, cx, cy, r_out) & ~disc_mask(w, h, cx, cy, r_in)


def rect_mask(w, h, x0, y0, x1, y1):
    m = np.zeros((h, w), bool)
    m[y0:y1 + 1, x0:x1 + 1] = True
    return m


def fill(img, mask, color):
    img[mask] = (*color, 255)


def shade_by(img, mask, ramp_cols, level_fn):
    """Fill mask pixels with ramp colours chosen by level_fn(x, y) -> 0..1."""
    h, w = mask.shape
    ys, xs = np.nonzero(mask)
    for x, y in zip(xs, ys):
        t = level_fn(int(x), int(y))
        i = min(len(ramp_cols) - 1, max(0, int(t * len(ramp_cols))))
        img[y, x] = (*ramp_cols[i], 255)


def vgrad(img, mask, ramp_cols, y0=None, y1=None):
    h, w = mask.shape
    if y0 is None:
        ys = np.nonzero(mask.any(axis=1))[0]
        y0, y1 = ys.min(), ys.max()
    shade_by(img, mask, ramp_cols,
             lambda x, y: 1.0 - (y - y0) / max(1, (y1 - y0)))


def edge_mask(mask, side='top'):
    """Pixels of mask whose neighbour outside the mask lies on `side`."""
    m = mask
    out = np.zeros_like(m)
    if side == 'top':
        out[1:, :] = m[1:, :] & ~m[:-1, :]
        out[0, :] = m[0, :]
    elif side == 'bottom':
        out[:-1, :] = m[:-1, :] & ~m[1:, :]
        out[-1, :] = m[-1, :]
    elif side == 'left':
        out[:, 1:] = m[:, 1:] & ~m[:, :-1]
        out[:, 0] = m[:, 0]
    elif side == 'right':
        out[:, :-1] = m[:, :-1] & ~m[:, 1:]
        out[:, -1] = m[:, -1]
    return out


def load_msx(name):
    im = Image.open(f"{MSX_DIR}/{name}.png").convert('RGBA')
    return np.asarray(im).copy()


def to_img(arr):
    return Image.fromarray(arr.astype(np.uint8), 'RGBA')


def compose_board(title, rows, out_path, cell_pad=14):
    """rows: list of (row_label, [(col_label, np RGBA array, scale), ...]).
    Renders a labelled grid on the deep-space background."""
    font = ImageFont.load_default(size=15)
    font_s = ImageFont.load_default(size=12)
    col_w = []
    ncols = len(rows[0][1])
    for c in range(ncols):
        col_w.append(max(arr.shape[1] * sc for _, cells in rows
                         for (lab, arr, sc) in [cells[c]]))
    row_h = []
    for _, cells in rows:
        row_h.append(max(arr.shape[0] * sc for _, arr, sc in cells))
    label_w = 92
    W = label_w + sum(w + cell_pad for w in col_w) + cell_pad
    H = 44 + sum(h + 30 + cell_pad for h in row_h) + cell_pad
    board = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((cell_pad, 12), title, fill=(210, 225, 255), font=font)
    y = 44
    for (rlabel, cells), rh in zip(rows, row_h):
        d.text((cell_pad, y + rh // 2 - 8), rlabel, fill=(170, 195, 235), font=font_s)
        x = label_w
        for ci, (lab, arr, sc) in enumerate(cells):
            im = to_img(arr).resize((arr.shape[1] * sc, arr.shape[0] * sc),
                                    Image.NEAREST)
            board.paste(im, (x, y), im)
            d.text((x, y + rh + 6), lab, fill=(150, 170, 205), font=font_s)
            x += col_w[ci] + cell_pad
        y += rh + 30 + cell_pad
    board.save(out_path)
    return out_path
