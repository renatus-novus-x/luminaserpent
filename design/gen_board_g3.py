"""G3 board: large enemies (CRYSTAL 32x32, CARRIER 48x32).
A = current, B = MSX-motif redraw at X68 size, C = outline + contrast."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont

grids, pals = {}, {}
cur = None
for line in open('/tmp/gfx2.txt'):
    line = line.strip()
    if line.startswith('=='): cur = line[3:]; grids[cur] = []
    elif line.startswith('PAL'):
        _, b, i, w = line.split(); w = int(w)
        pals[(int(b), int(i))] = ((w >> 6) & 31, (w >> 11) & 31, (w >> 1) & 31)
    elif line and cur: grids[cur].append([int(v) for v in line.split(',')])

def rgb(t): return tuple(v * 255 // 31 for v in t)
BG = (8, 16, 57)

def tile_grid(slot):
    return np.array(grids[f'SLOT {slot}'], dtype=int)

def compose_grid(slots, cols):
    rows = len(slots) // cols
    out = np.zeros((rows * 16, cols * 16), dtype=int)
    for k, s in enumerate(slots):
        out[(k // cols) * 16:(k // cols + 1) * 16, (k % cols) * 16:(k % cols + 1) * 16] = tile_grid(s)
    return out

def render(g, blk, pal):
    img = np.zeros((g.shape[0], g.shape[1], 3), np.uint8); img[:] = BG
    for (b, i), c in pal.items():
        if b == blk: img[g == i] = rgb(c)
    img[g == 0] = BG
    return img

def outlined(g, idx=3):
    out = g.copy()
    H, W = g.shape
    for y in range(H):
        for x in range(W):
            if g[y, x] == 0:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and g[ny, nx] != 0:
                        out[y, x] = idx; break
    return out

def q(r, g, b): return (r * 255 // 31, g * 255 // 31, b * 255 // 31)
OUT = q(3, 5, 17); CYAN = q(8, 29, 31); WHITE = q(31, 31, 31); MAG = q(31, 8, 28)
BLUE = q(4, 8, 29); RED = q(29, 4, 6); ORANGE = q(31, 17, 5); NAVY = q(1, 2, 10)

def crystal_b():
    im = Image.new('RGB', (32, 32), BG); d = ImageDraw.Draw(im)
    bands = [(2, 4, WHITE), (5, 9, CYAN), (10, 13, WHITE), (14, 17, CYAN),
             (18, 21, MAG), (22, 25, WHITE), (26, 29, CYAN)]
    hw = [3, 6, 10, 13, 15, 15, 13, 10, 6, 3]
    for i, (y0, y1, c) in enumerate([(2, 4, WHITE), (5, 8, CYAN), (9, 12, WHITE),
                                    (13, 16, CYAN), (17, 20, MAG), (21, 24, WHITE), (25, 28, CYAN)]):
        w = hw[min(i + 1, 9)]
        d.rectangle([16 - w, y0, 15 + w, y1], fill=c)
    d.rectangle([15, 0, 16, 2], fill=WHITE); d.rectangle([15, 29, 16, 31], fill=CYAN)
    return np.asarray(im)

def carrier_b():
    im = Image.new('RGB', (48, 32), BG); d = ImageDraw.Draw(im)
    d.rectangle([6, 3, 41, 5], fill=ORANGE)            # top fin
    d.rectangle([4, 6, 43, 24], fill=BLUE)             # body
    d.rectangle([4, 13, 43, 18], fill=RED)             # mid stripe
    d.rectangle([12, 10, 17, 21], fill=NAVY)           # windows
    d.rectangle([30, 10, 35, 21], fill=NAVY)
    d.rectangle([21, 14, 26, 17], fill=NAVY)           # core
    d.rectangle([6, 25, 41, 27], fill=NAVY)
    d.rectangle([8, 28, 39, 29], fill=ORANGE)          # bottom fin
    d.rectangle([4, 6, 43, 6], fill=q(14, 20, 31))     # top sheen
    return np.asarray(im)

cry = compose_grid([14, 15, 16, 17], 2)
car = compose_grid([18, 19, 20, 21, 22, 23], 3)
pal5 = dict(pals); pal5[(5, 1)] = (10, 27, 31); pal5[(5, 6)] = (2, 10, 15)
pal8 = dict(pals); pal8[(8, 3)] = (2, 3, 12); pal8[(8, 6)] = (1, 2, 7); pal8[(8, 1)] = (7, 12, 30)
ROWS = [
    ('A: current', render(cry, 5, pals), render(car, 8, pals)),
    ('B: MSX-motif redraw', crystal_b(), carrier_b()),
    ('C: outline + contrast', render(outlined(cry), 5, pal5), render(outlined(car), 8, pal8)),
]
Z = 3
font = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc', 16)
fontb = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc', 18)
W = 20 + 32 * Z + 30 + 48 * Z + 60
H = 76 + len(ROWS) * (32 * Z + 34)
img = Image.new('RGB', (W, H), (10, 12, 28)); dr = ImageDraw.Draw(img)
dr.text((12, 10), 'G3: LARGE ENEMIES candidates (x3, game background)', font=fontb, fill=(240, 240, 255))
dr.text((20, 40), 'CRYSTAL', font=font, fill=(170, 200, 255))
dr.text((50 + 32 * Z, 40), 'CARRIER', font=font, fill=(170, 200, 255))
y = 76
for label, c1, c2 in ROWS:
    img.paste(Image.fromarray(c1).resize((32 * Z, 32 * Z), Image.NEAREST), (20, y))
    img.paste(Image.fromarray(c2).resize((48 * Z, 32 * Z), Image.NEAREST), (50 + 32 * Z, y))
    dr.text((20, y + 32 * Z + 6), label, font=font, fill=(255, 220, 150))
    y += 32 * Z + 34
img.save('/home/hatch/workspace/x68k/github/luminaserpent/design/board-g3-large.png')
print('saved', img.size)
