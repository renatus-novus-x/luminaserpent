"""G1 board: ship + option improvement candidates.
A = current (exact harness dump), B = MSX-direct (bold flat style),
C = refined (higher-contrast facets + accents, same silhouette)."""
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

def tile(slot, blk, pal_override=None):
    g = np.array(grids[f'SLOT {slot}'], dtype=int)
    img = np.zeros((16, 16, 3), np.uint8); img[:] = BG
    pal = pal_override or pals
    for (b, i), c in pal.items():
        if b == blk: img[g == i] = rgb(c)
    img[g == 0] = BG
    return img, g

def ship(pal_override=None):
    l, _ = tile(0, 1, pal_override); r, _ = tile(1, 1, pal_override)
    lf, _ = tile(2, 1, pal_override); rf, _ = tile(3, 1, pal_override)
    return np.hstack([l, r]), np.hstack([lf, rf])

def q(r, g, b): return (r * 255 // 31, g * 255 // 31, b * 255 // 31)
OUT = q(3, 5, 17); CYAN = q(6, 29, 30); CYAN_D = q(2, 18, 22)
WHITE = q(31, 31, 31); NAVY = q(1, 2, 12); ORANGE = q(31, 16, 2)
YELLOW = q(31, 26, 6); GOLD = q(31, 22, 4)

def draw_ship_b(flame=False):
    im = Image.new('RGB', (32, 16), BG); d = ImageDraw.Draw(im)
    body = [(0, 6), (3, 5), (7, 3), (13, 3), (19, 4), (27, 5), (31, 7), (31, 9), (26, 11), (15, 12), (5, 12), (0, 10)]
    d.polygon(body, fill=OUT)
    inner = [(1, 6), (4, 6), (8, 4), (13, 4), (19, 5), (26, 6), (30, 7), (30, 8), (25, 10), (15, 11), (6, 11), (1, 10)]
    d.polygon(inner, fill=CYAN)
    d.rectangle([8, 4, 20, 7], fill=CYAN)          # smooth top deck
    d.rectangle([6, 1, 12, 3], fill=OUT); d.rectangle([7, 2, 11, 3], fill=WHITE)   # top fin
    d.rectangle([16, 5, 21, 6], fill=NAVY); d.rectangle([16, 5, 17, 5], fill=WHITE)  # canopy
    d.rectangle([5, 10, 26, 11], fill=CYAN_D)      # underside shade
    d.rectangle([24, 8, 29, 8], fill=WHITE)        # nose sheen
    if flame:
        d.polygon([(0, 6), (5, 7), (5, 8), (0, 10)], fill=ORANGE)
        d.polygon([(1, 7), (4, 7), (4, 8), (1, 8)], fill=YELLOW)
    else:
        d.rectangle([0, 6, 2, 9], fill=ORANGE)
    return np.asarray(im)

def draw_option_b():
    im = Image.new('RGB', (16, 16), BG); d = ImageDraw.Draw(im)
    d.rectangle([5, 2, 10, 3], fill=WHITE)
    d.rectangle([2, 4, 13, 7], fill=CYAN)
    d.rectangle([7, 5, 8, 6], fill=NAVY)
    d.rectangle([2, 8, 13, 9], fill=WHITE)
    d.rectangle([2, 10, 13, 10], fill=CYAN)
    d.rectangle([5, 12, 10, 13], fill=NAVY)
    return np.asarray(im)

def draw_option_c():
    im = Image.new('RGB', (16, 16), BG); d = ImageDraw.Draw(im)
    d.polygon([(8, 1), (14, 8), (8, 15), (2, 8)], fill=OUT)
    d.polygon([(8, 3), (12, 8), (8, 13), (4, 8)], fill=CYAN)
    d.polygon([(8, 5), (10, 8), (8, 11), (6, 8)], fill=CYAN_D)
    d.rectangle([7, 7, 8, 8], fill=WHITE)
    return np.asarray(im)

# C: current geometry, higher-contrast palette + glints
pal_c = dict(pals)
pal_c[(1, 1)] = (4, 24, 28); pal_c[(1, 8)] = (18, 31, 31); pal_c[(1, 6)] = (0, 9, 13)
pal_c[(1, 3)] = (0, 1, 6); pal_c[(1, 4)] = (24, 31, 31)
ship_a, flame_a = ship()
ship_c, flame_c = ship(pal_c)
lg = np.array(grids['SLOT 1'], dtype=int)
# nose glint on C: rightmost body pixels of the right tile -> white is idx2 already in pal; add gold tip via direct draw
ship_c_img = Image.fromarray(ship_c); dc = ImageDraw.Draw(ship_c_img)
dc.rectangle([19, 10, 22, 10], fill=GOLD)   # gold flank stripe on the shade line
ship_c = np.asarray(ship_c_img)
flame_c_img = Image.fromarray(flame_c); df = ImageDraw.Draw(flame_c_img)
df.rectangle([19, 10, 22, 10], fill=GOLD)
flame_c = np.asarray(flame_c_img)
opt_a, _ = tile(4, 12)
opt_c_pal = dict(pals); opt_c_pal[(12, 1)] = (4, 24, 28); opt_c_pal[(12, 3)] = (0, 9, 13)
opt_a_img = opt_a

ROWS = [
    ('A: current', ship_a, flame_a, opt_a_img),
    ('B: MSX-direct (bold flat)', draw_ship_b(False), draw_ship_b(True), draw_option_b()),
    ('C: refined contrast + accents', ship_c, flame_c, draw_option_c()),
]
Z = 6
font = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc', 16)
fontb = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc', 18)
W = 40 + 32 * Z + 30 + 32 * Z + 30 + 16 * Z + 30
H = 44 + len(ROWS) * (16 * Z + 34)
img = Image.new('RGB', (W, H), (10, 12, 28)); dr = ImageDraw.Draw(img)
dr.text((12, 10), 'G1: SHIP + OPTION candidates (x6, game background)', font=fontb, fill=(240, 240, 255))
y = 44
for name, s, f, o in ROWS:
    for x, arr in ((12, s), (42 + 32 * Z, f), (72 + 64 * Z, o)):
        img.paste(Image.fromarray(arr).resize((arr.shape[1] * Z, arr.shape[0] * Z), Image.NEAREST), (x, y))
    dr.text((12, y + 16 * Z + 6), name, font=font, fill=(255, 220, 150))
    y += 16 * Z + 34
img.save('/home/hatch/workspace/x68k/github/luminaserpent/design/board-g1-ship.png')
print('saved', img.size)
