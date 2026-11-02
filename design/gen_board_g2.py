"""G2 board: small enemies (FAN/WEAVER/ORB/DART).
A = current, B = MSX art (GRB555-quantised), C = current + outline pass
and higher-contrast palettes."""
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
EN = [('FAN', 5, 2), ('WEAVER', 6, 3), ('ORB', 7, 4), ('DART', 8, 7)]

def render(g, blk, pal):
    img = np.zeros((16, 16, 3), np.uint8); img[:] = BG
    for (b, i), c in pal.items():
        if b == blk: img[g == i] = rgb(c)
    img[g == 0] = BG
    return img

def outlined(g):
    g = np.array(g, dtype=int); out = g.copy()
    for y in range(16):
        for x in range(16):
            if g[y, x] == 0:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < 16 and 0 <= ny < 16 and g[ny, nx] != 0:
                        out[y, x] = 3; break
    return out

PAL_C = {
    2: {1: (31, 27, 4), 6: (9, 2, 6)},
    3: {1: (6, 27, 7), 6: (0, 4, 1)},
    4: {1: (29, 8, 27), 6: (5, 0, 4)},
    7: {1: (30, 24, 5), 6: (13, 8, 2)},
}
MSX = '/home/hatch/workspace/x68k/lumina-msx-assets/luminaserpent-assets/sprites/'
def msx_img(name):
    im = Image.open(MSX + name + '.png').convert('RGBA')
    a = np.asarray(im).astype(np.float32)
    al = a[:, :, 3:4] / 255.0
    comp = a[:, :, :3] * al + np.array(BG, np.float32) * (1 - al)
    q = np.round(comp / 255.0 * 31) * 255 // 31
    return q.astype(np.uint8)

rows = []
ra, rb, rc = [], [], []
for name, slot, blk in EN:
    g = np.array(grids[f'SLOT {slot}'], dtype=int)
    ra.append(render(g, blk, pals))
    rb.append(msx_img(name.lower()))
    pal = dict(pals); pal.update({(blk, i): c for i, c in PAL_C[blk].items()})
    rc.append(render(outlined(g), blk, pal))
rows = [('A: current', ra), ('B: MSX-direct', rb), ('C: outline + contrast', rc)]

Z = 6
font = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc', 16)
fontb = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc', 18)
W = 20 + 4 * (16 * Z + 18)
H = 76 + len(rows) * (16 * Z + 34)
img = Image.new('RGB', (W, H), (10, 12, 28)); dr = ImageDraw.Draw(img)
dr.text((12, 10), 'G2: SMALL ENEMIES candidates (x6, game background)', font=fontb, fill=(240, 240, 255))
for j, (name, _, _) in enumerate(EN):
    dr.text((20 + j * (16 * Z + 18), 40), name, font=font, fill=(170, 200, 255))
y = 76
for label, cells in rows:
    for j, arr in enumerate(cells):
        img.paste(Image.fromarray(arr).resize((16 * Z, 16 * Z), Image.NEAREST), (20 + j * (16 * Z + 18), y))
    dr.text((20, y + 16 * Z + 6), label, font=font, fill=(255, 220, 150))
    y += 16 * Z + 34
img.save('/home/hatch/workspace/x68k/github/luminaserpent/design/board-g2-small.png')
print('saved', img.size)
