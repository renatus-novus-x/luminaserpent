"""G4 board: BOSS 64x64 + SHARD 16x16.
A = current, B = MSX-motif (round magenta core, white spike, thin frames),
C = outline + contrast."""
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

def compose_grid(slots, cols):
    rows = len(slots) // cols
    out = np.zeros((rows * 16, cols * 16), dtype=int)
    for k, s in enumerate(slots):
        g = np.array(grids[f'SLOT {s}'], dtype=int)
        out[(k // cols) * 16:(k // cols + 1) * 16, (k % cols) * 16:(k % cols + 1) * 16] = g
    return out

def render(g, blk, pal):
    img = np.zeros((g.shape[0], g.shape[1], 3), np.uint8); img[:] = BG
    for (b, i), c in pal.items():
        if b == blk: img[g == i] = rgb(c)
    img[g == 0] = BG
    return img

def outlined(g, idx=3):
    out = g.copy(); H, W = g.shape
    for y in range(H):
        for x in range(W):
            if g[y, x] == 0:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and g[ny, nx] != 0:
                        out[y, x] = idx; break
    return out

def q(r, g, b): return (r * 255 // 31, g * 255 // 31, b * 255 // 31)
CYAN = q(8, 29, 31); CYAN_D = q(3, 18, 22); TEAL = q(4, 22, 18); WHITE = q(31, 31, 31)
MAG = q(31, 6, 26); MAG_D = q(24, 3, 19); OUT = q(3, 5, 17)

def boss_b():
    im = Image.new('RGB', (64, 64), BG); d = ImageDraw.Draw(im)
    cx = cy = 32
    for r, c in ((30, CYAN_D), (24, CYAN), (18, TEAL)):
        d.polygon([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)], outline=c, width=2)
    d.line([cx - 30, cy, cx + 30, cy], fill=WHITE, width=2)       # horizontal spike
    d.line([cx, cy - 12, cx, cy + 12], fill=CYAN, width=2)
    d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], fill=MAG)     # round core
    d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], outline=MAG_D)
    d.rectangle([cx - 6, cy - 8, cx - 2, cy - 4], fill=WHITE)     # glint
    return np.asarray(im)

def shard_b():
    im = Image.new('RGB', (16, 16), BG); d = ImageDraw.Draw(im)
    d.rectangle([7, 1, 8, 3], fill=q(4, 22, 18))
    d.rectangle([5, 4, 10, 6], fill=CYAN)
    d.rectangle([4, 7, 11, 7], fill=WHITE)
    d.rectangle([3, 8, 12, 10], fill=MAG)
    d.rectangle([5, 11, 10, 12], fill=CYAN)
    d.rectangle([6, 13, 9, 15], fill=q(4, 22, 18))
    return np.asarray(im)

boss = compose_grid(list(range(24, 40)), 4)
shard = np.array(grids['SLOT 40'], dtype=int)
pal9 = dict(pals); pal9[(9, 1)] = (11, 25, 30); pal9[(9, 6)] = (3, 11, 14)
pal5 = dict(pals); pal5[(5, 1)] = (10, 27, 31); pal5[(5, 6)] = (2, 10, 15)
ROWS = [
    ('A: current', render(boss, 9, pals), render(shard, 5, pals)),
    ('B: MSX-motif', boss_b(), shard_b()),
    ('C: outline + contrast', render(outlined(boss), 9, pal9), render(outlined(shard), 5, pal5)),
]
ZB, ZS = 2, 4
font = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc', 16)
fontb = ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc', 18)
W = 20 + 64 * ZB + 30 + 16 * ZS + 40
H = 76 + len(ROWS) * (64 * ZB + 34)
img = Image.new('RGB', (W, H), (10, 12, 28)); dr = ImageDraw.Draw(img)
dr.text((12, 10), 'G4: BOSS + SHARD candidates (boss x2, shard x4)', font=fontb, fill=(240, 240, 255))
dr.text((20, 40), 'BOSS (blk9 main)', font=font, fill=(170, 200, 255))
dr.text((50 + 64 * ZB, 40), 'SHARD', font=font, fill=(170, 200, 255))
y = 76
for label, b, s in ROWS:
    img.paste(Image.fromarray(b).resize((64 * ZB, 64 * ZB), Image.NEAREST), (20, y))
    img.paste(Image.fromarray(s).resize((16 * ZS, 16 * ZS), Image.NEAREST), (50 + 64 * ZB, y + 32))
    dr.text((20, y + 64 * ZB + 6), label, font=font, fill=(255, 220, 150))
    y += 64 * ZB + 34
img.save('/home/hatch/workspace/x68k/github/luminaserpent/design/board-g4-boss.png')
print('saved', img.size)
