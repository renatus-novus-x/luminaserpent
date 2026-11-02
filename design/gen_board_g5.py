"""G5 board: items + shots + shield/beam/flare.
A = current, B = MSX-direct, C = outline/bold + contrast."""
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

def render(g, blk, pal):
    g = np.asarray(g, dtype=int)
    img = np.zeros((g.shape[0], g.shape[1], 3), np.uint8); img[:] = BG
    for (b, i), c in pal.items():
        if b == blk: img[g == i] = rgb(c)
    img[g == 0] = BG
    return img

def outlined(g, idx=3):
    g = np.asarray(g, dtype=int); out = g.copy(); H, W = g.shape
    for y in range(H):
        for x in range(W):
            if g[y, x] == 0:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and g[ny, nx] != 0:
                        out[y, x] = idx; break
    return out

def bold_glyph(g):
    g = np.asarray(g, dtype=int); out = g.copy()
    for y in range(16):
        for x in range(16):
            if g[y, x] == 4:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < 16 and 0 <= ny < 16 and g[ny, nx] == 1:
                        out[ny, nx] = 4
    return out

MSX = '/home/hatch/workspace/x68k/lumina-msx-assets/luminaserpent-assets/sprites/'
def msx_img(name, w=16, h=16):
    im = Image.open(MSX + name + '.png').convert('RGBA').resize((w, h), Image.NEAREST)
    a = np.asarray(im).astype(np.float32)
    al = a[:, :, 3:4] / 255.0
    comp = a[:, :, :3] * al + np.array(BG, np.float32) * (1 - al)
    return (np.round(comp / 255.0 * 31) * 255 // 31).astype(np.uint8)

ITEMS = [('T', 7), ('C', 2), ('D', 3), ('P', 5), ('E', 12)]
SHOTS = [('PBUL', 10), ('EBUL', 11), ('MISSILE', 12), ('SPARK', 13)]
B_ITEM = {'T': 'capsule_T', 'C': 'capsule_C', 'D': 'capsule_D', 'P': 'capsule_P', 'E': 'capsule_E'}
B_SHOT = {'PBUL': 'player_shot', 'EBUL': 'enemy_shot', 'MISSILE': 'missile', 'SPARK': 'explosion_1'}

cols1 = [f'ITEM {t}' for t, _ in ITEMS] + ['ITEM SHIELD', 'ITEM STAR'] + [s for s, _ in SHOTS]
rowA1, rowB1, rowC1 = [], [], []
for t, blk in ITEMS:
    g = grids[f'ITEM {t}']
    rowA1.append(render(g, blk, pals))
    rowB1.append(msx_img(B_ITEM[t]))
    pal = dict(pals)
    if (blk, 1) in pal:
        r, gg, b = pal[(blk, 1)]; pal[(blk, 1)] = (min(31, r + 3), min(31, gg + 3), min(31, b + 2))
    rowC1.append(render(bold_glyph(g), blk, pal))
rowA1.append(render(grids['ITEM SHIELD'], 4, pals)); rowB1.append(msx_img('capsule_H'))
rowC1.append(render(bold_glyph(grids['ITEM SHIELD']), 4, pals))
rowA1.append(render(grids['ITEM STAR'], 11, pals)); rowB1.append(msx_img('capsule_star'))
rowC1.append(render(grids['ITEM STAR'], 11, pals))
for s, slot in SHOTS:
    g = np.array(grids[f'SLOT {slot}'], dtype=int)
    rowA1.append(render(g, 11, pals))
    rowB1.append(msx_img(B_SHOT[s]))
    rowC1.append(render(outlined(g), 11, pals))

# group 2: shield ring / beam / flare
shield = np.zeros((32, 48), dtype=int)
for k, s in enumerate((41, 42, 43, 44, 45, 46)):
    shield[(k // 3) * 16:(k // 3 + 1) * 16, (k % 3) * 16:(k % 3 + 1) * 16] = np.array(grids[f'SLOT {s}'], dtype=int)
beam_a = np.vstack([np.array(grids['BEAM_T'], dtype=int), np.array(grids['BEAM_M'], dtype=int)])
beam_c = beam_a.copy()
core_rows = [y for y in range(32) if (beam_a[y] == 9).any()]
for y in core_rows:
    for yy in (y - 1, y + 1):
        if 0 <= yy < 32: beam_c[yy] = np.where(beam_a[yy] == 14, 9, beam_a[yy])
fl = np.array(grids['FLARE'], dtype=int)
flare_a = np.zeros((32, 32), dtype=int)
flare_a[:16, :16] = fl; flare_a[:16, 16:] = fl[:, ::-1]; flare_a[16:, :16] = fl[::-1, :]; flare_a[16:, 16:] = fl[::-1, ::-1]
pal14 = dict(pals); pal14[(14, 1)] = (13, 29, 31)
pal15 = dict(pals); pal15[(15, 8)] = (16, 25, 31)
g2A = [render(shield, 14, pals), render(beam_a, 15, pals), render(flare_a, 15, pals)]
g2B = [msx_img('halo', 48, 32), msx_img('beam_strip', 16, 32), render(flare_a, 15, pals)]
g2C = [render(outlined(shield), 14, pal14), render(beam_c, 15, pals), render(flare_a, 15, pal15)]

font = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc', 15)
fontb = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc', 17)
Z = 3
W = 20 + len(cols1) * (16 * Z + 8)
H1 = 70 + 3 * (16 * Z + 30)
H = H1 + 40 + 26 + 3 * 108 + 10
img = Image.new('RGB', (W, H), (10, 12, 28)); dr = ImageDraw.Draw(img)
dr.text((12, 8), 'G5: ITEMS + SHOTS + EFFECTS candidates', font=fontb, fill=(240, 240, 255))
for j, name in enumerate(cols1):
    dr.text((20 + j * (16 * Z + 8), 36), name.replace('ITEM ', ''), font=font, fill=(170, 200, 255))
y = 70
for label, row in (('A: current', rowA1), ('B: MSX-direct', rowB1), ('C: bold/outline + contrast', rowC1)):
    for j, arr in enumerate(row):
        img.paste(Image.fromarray(arr).resize((16 * Z, 16 * Z), Image.NEAREST), (20 + j * (16 * Z + 8), y))
    dr.text((20, y + 16 * Z + 4), label, font=font, fill=(255, 220, 150))
    y += 16 * Z + 30
y += 10
dr.text((12, y), 'SHIELD RING / BEAM / FLARE', font=fontb, fill=(240, 240, 255)); y += 26
for label, cells in (('A', g2A), ('B', g2B), ('C', g2C)):
    x = 20
    img.paste(Image.fromarray(cells[0]).resize((96, 64), Image.NEAREST), (x, y)); x += 110
    img.paste(Image.fromarray(cells[1]).resize((48, 96), Image.NEAREST), (x, y)); x += 62
    img.paste(Image.fromarray(cells[2]).resize((64, 64), Image.NEAREST), (x, y))
    dr.text((20, y + 98), label, font=font, fill=(255, 220, 150))
    y += 108
img.save('/home/hatch/workspace/x68k/github/luminaserpent/design/board-g5-items.png')
print('saved', img.size)
