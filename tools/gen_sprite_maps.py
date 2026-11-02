"""Emit design/sprite_maps.inc: index maps for the refine-2 ports.
Ship = the approved G1-B redraw (32 wide) quantised to the new block-1
palette; option/shots = MSX PNGs quantised; capsules = MSX ring +
letter layers."""
import numpy as np
from PIL import Image, ImageDraw

MSX = '/home/hatch/workspace/x68k/lumina-msx-assets/luminaserpent-assets/sprites/'
OUT = []

def emit(name, rows):
    OUT.append(f'static const char *const {name}[{len(rows)}] = {{')
    for r in rows: OUT.append(f'  "{r}",')
    OUT.append('};')

def quantize(png, palette, alpha=128):
    """palette: {index: (r,g,b) in 0..31}. Returns list of digit rows."""
    im = Image.open(MSX + png).convert('RGBA')
    a = np.asarray(im).astype(int)
    rows = []
    keys = sorted(palette)
    cols = np.array([palette[k] for k in keys], dtype=float) * 255 / 31
    for y in range(im.size[1]):
        row = ''
        for x in range(im.size[0]):
            r, g, b, al = a[y, x]
            if al < alpha: row += '.'; continue
            d = ((cols - np.array([r, g, b])) ** 2).sum(axis=1)
            row += str(keys[int(np.argmin(d))])
        rows.append(row)
    return rows

# ---- ship: the approved board-G1 B redraw, quantised to new blk1 ----
def q(r, g, b): return (r * 255 // 31, g * 255 // 31, b * 255 // 31)
NAVY = q(2, 4, 10); CYAN = q(6, 22, 28); LITE = q(20, 30, 31); WHITE = q(31, 31, 31)
ORANGE = q(30, 15, 3); REDD = q(26, 6, 4); GRAY = q(13, 14, 17)
im = Image.new('RGB', (32, 16), (0, 0, 0)); d = ImageDraw.Draw(im)
pts = [(3, 7), (10, 4), (20, 3), (31, 7), (31, 8), (20, 12), (10, 11), (3, 8)]
d.polygon(pts, fill=CYAN)
d.polygon([(3, 7), (10, 4), (20, 3), (31, 7), (20, 6), (10, 6)], fill=LITE)
d.polygon([(10, 11), (20, 12), (31, 8), (20, 9), (10, 9)], fill=NAVY)
d.polygon([(11, 1), (15, 1), (17, 4), (10, 4)], fill=WHITE)
d.polygon([(4, 11), (7, 14), (11, 14), (12, 11)], fill=REDD)
d.rectangle([0, 6, 3, 9], fill=ORANGE)
d.rectangle([16, 5, 22, 7], fill=NAVY)
d.rectangle([21, 5, 22, 6], fill=WHITE)
d.rectangle([24, 6, 28, 8], fill=GRAY)
arr = np.asarray(im).astype(int)
SHIP_PAL = {1: (6, 24, 28), 8: (18, 29, 31), 2: (31, 31, 31), 3: (2, 5, 14),
            4: (10, 18, 30), 5: (31, 16, 2), 6: (2, 13, 19)}
rows = []
keys = sorted(SHIP_PAL); cols = np.array([SHIP_PAL[k] for k in keys], float) * 255 / 31
for y in range(16):
    row = ''
    for x in range(32):
        px = arr[y, x]
        if (px == 0).all(): row += '.'; continue
        dist = ((cols - px) ** 2).sum(axis=1)
        row += str(keys[int(np.argmin(dist))])
    rows.append(row)
rows[7] = '7' + rows[7][1:]          # flame core tongue
rows[8] = '5' + rows[8][1:]
emit('SHIP_MAP', rows)

_opt = quantize('option_echo.png',
     {1: (2, 6, 16), 2: (31, 31, 31), 3: (8, 28, 30), 5: (31, 26, 6)})
_opt[6] = _opt[6].replace('3333..3333', '3333113333')  # fill the core dimple
emit('OPTION_MAP', _opt)
BLK11 = {1: (31, 30, 12), 2: (31, 31, 31), 3: (20, 8, 8), 4: (31, 9, 15),
         5: (23, 23, 27), 6: (31, 17, 2), 7: (31, 24, 4)}
emit('PBUL_MAP', quantize('player_shot.png', BLK11))
emit('EBUL_MAP', quantize('enemy_shot.png', BLK11))
emit('MISSILE_MAP', quantize('missile.png', BLK11))
_spark = quantize('explosion_1.png', BLK11)
emit('SPARK_MAP', [r.replace('7', '1').replace('6', '4') for r in _spark])  # {1,2,4} only: particles cycle palette blocks

# ---- capsule ring + glyphs ----
ring = quantize('capsule_frame_T.png', {1: (0, 31, 31)})
emit('CAPSULE_RING', [''.join('.' if ch == '.' else 'F' for ch in r) for r in ring])
def glyph(png):
    im = Image.open(MSX + png).convert('RGBA')
    a = np.asarray(im)
    ys, xs = np.where(a[:, :, 3] >= 128)
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    rows = []
    for y in range(y0, y1 + 1):
        rows.append(''.join('#' if a[y, x, 3] >= 128 else '.'
                            for x in range(x0, x1 + 1)))
    return rows, x0, y0
for name, png in (('T', 'letter_T.png'), ('C', 'letter_C.png'), ('D', 'letter_D.png'),
                  ('P', 'letter_P.png'), ('E', 'letter_E.png'), ('H', 'letter_H.png'),
                  ('STAR', 'letter_star.png')):
    rows, x0, y0 = glyph(png)
    emit(f'GLYPH_{name}', rows)
    OUT.append(f'static const int GLYPH_{name}_X = {x0}, GLYPH_{name}_Y = {y0};')

open('/home/hatch/workspace/x68k/github/luminaserpent/design/sprite_maps.inc', 'w').write('\n'.join(OUT) + '\n')
print('written', len(OUT), 'lines')
