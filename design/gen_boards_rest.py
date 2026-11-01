#!/usr/bin/env python3
"""Boards 2-5: small enemies / large enemies / boss / items+beams.
Same three treatments as board 1: A faithful+shade, B glossy, C neon."""
import sys, math
sys.path.insert(0, "/home/hatch/workspace/x68k/github/luminaserpent/design")
import numpy as np
from refine_lib import (canvas, fill, poly_mask, disc_mask, ring_mask,
                        rect_mask, vgrad, shade_by, edge_mask, load_msx,
                        compose_board, hx, ramp)

WHT = hx('#ffffff')
CY = ramp('#0a4d5e', '#07908f', '#00c5cd', '#00e5e5', '#c8fffa')
NV = ramp('#02022e', '#000049', '#0a2a6e')
OR = ramp('#6e1e00', '#e55a18', '#ff9249', '#ffd9a8')
YL = ramp('#6e4d00', '#e5b800', '#ffe500', '#fff7ae')
GN = ramp('#003d00', '#007d00', '#00c500', '#00ff00', '#baffb0')
MG = ramp('#490049', '#920049', '#e500c5', '#ff00e5', '#ffc8f7')
RD = ramp('#490000', '#920000', '#e50000', '#ff6e6e')
BL = ramp('#000049', '#0000c5', '#0000ff', '#6e8fff', '#c8d7ff')

D = "/home/hatch/workspace/x68k/github/luminaserpent/design"

# ============================================================ board 2: 16px
def fan(t):
    img = canvas(16, 16)
    disc = disc_mask(16, 16, 8, 8, 7)
    band = disc & rect_mask(16, 16, 0, 7, 15, 9)
    bot = disc & rect_mask(16, 16, 0, 12, 15, 15)
    if t == 'A':
        fill(img, disc, YL[2]); fill(img, band, OR[2]); fill(img, bot, MG[1])
        fill(img, disc_mask(16, 16, 8, 8, 2), OR[1]); img[8, 8] = (*WHT, 255)
        img[4, 5] = (*WHT, 255); img[4, 6] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, disc, YL)
        fill(img, band, OR[2]); fill(img, edge_mask(band, 'top'), OR[3])
        fill(img, disc_mask(16, 16, 6, 5, 2) & disc, WHT)
        fill(img, bot, MG[1])
        fill(img, disc_mask(16, 16, 8, 8, 2), OR[3]); img[8, 8] = (*WHT, 255)
    else:
        vgrad(img, disc, [hx('#2e1e00'), hx('#6e4d00'), hx('#920049')])
        fill(img, edge_mask(disc, 'top'), YL[3])
        fill(img, band, OR[1]); img[8, 8] = (*WHT, 255)
        fill(img, edge_mask(disc, 'bottom'), MG[3])
    return img

def weaver(t):
    img = canvas(16, 16)
    body = poly_mask(16, 16, [(1, 8), (5, 3), (11, 3), (15, 8), (11, 13), (5, 13)])
    stripe = body & rect_mask(16, 16, 0, 7, 15, 8)
    if t == 'A':
        fill(img, body, GN[3]); fill(img, body & rect_mask(16, 16, 0, 9, 15, 15), GN[1])
        fill(img, stripe, WHT)
    elif t == 'B':
        vgrad(img, body, GN)
        fill(img, stripe, GN[4]); img[7, 8] = (*WHT, 255); img[8, 8] = (*WHT, 255)
        fill(img, disc_mask(16, 16, 8, 5, 1) & body, WHT)
    else:
        vgrad(img, body, [hx('#001e00'), GN[1], GN[2]])
        fill(img, edge_mask(body, 'top'), GN[3])
        fill(img, stripe, GN[0]); img[7, 8] = (*WHT, 255); img[8, 8] = (*WHT, 255)
    return img

def orb(t):
    img = canvas(16, 16)
    disc = disc_mask(16, 16, 8, 8, 7)
    if t == 'A':
        fill(img, disc, MG[3])
        fill(img, disc & rect_mask(16, 16, 9, 10, 15, 15), MG[1])
        fill(img, disc & rect_mask(16, 16, 0, 11, 15, 15), MG[1])
        img[4, 5] = (*WHT, 255); img[5, 5] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, disc, MG)
        fill(img, disc_mask(16, 16, 6, 5, 2) & disc, WHT)
        fill(img, disc_mask(16, 16, 8, 8, 2), MG[4])
        img[8, 8] = (*WHT, 255)
    else:
        vgrad(img, disc, [hx('#1e001e'), MG[0], MG[1]])
        fill(img, edge_mask(disc, 'top'), MG[4])
        fill(img, disc_mask(16, 16, 8, 8, 1), WHT)
    return img

def dart(t):
    img = canvas(16, 16)
    body = poly_mask(16, 16, [(1, 5), (9, 3), (15, 8), (9, 13), (1, 11), (4, 8)])
    nose = body & rect_mask(16, 16, 13, 0, 15, 15)
    if t == 'A':
        fill(img, body, YL[2]); fill(img, body & rect_mask(16, 16, 0, 9, 15, 15), OR[2])
        fill(img, nose, WHT)
    elif t == 'B':
        vgrad(img, body, [OR[0], OR[1], YL[2], YL[3], hx('#fffbd9')])
        fill(img, nose, WHT)
        fill(img, edge_mask(body, 'top'), YL[3])
    else:
        vgrad(img, body, [hx('#2e1e00'), hx('#6e4d00'), OR[0]])
        fill(img, edge_mask(body, 'top'), YL[3])
        fill(img, nose, WHT); fill(img, edge_mask(body, 'bottom'), OR[2])
    return img

def ebullet(t):
    img = canvas(16, 16)
    disc = disc_mask(16, 16, 8, 8, 3)
    if t == 'A':
        fill(img, disc, MG[3]); fill(img, disc_mask(16, 16, 8, 8, 1), MG[1])
        img[6, 6] = (*WHT, 255)
    elif t == 'B':
        fill(img, disc_mask(16, 16, 8, 8, 4), MG[0])
        fill(img, disc, MG[3]); fill(img, disc_mask(16, 16, 8, 8, 2), MG[4])
        img[8, 8] = (*WHT, 255)
    else:
        fill(img, disc, MG[1]); fill(img, edge_mask(disc, 'top'), MG[4])
        img[8, 8] = (*WHT, 255)
    return img

rows2 = [
    ("FAN", [("MSX", load_msx('fan'), 6), ("A", fan('A'), 6), ("B", fan('B'), 6), ("C", fan('C'), 6)]),
    ("WEAVER", [("MSX", load_msx('weaver'), 6), ("A", weaver('A'), 6), ("B", weaver('B'), 6), ("C", weaver('C'), 6)]),
    ("ORB", [("MSX", load_msx('orb'), 6), ("A", orb('A'), 6), ("B", orb('B'), 6), ("C", orb('C'), 6)]),
    ("DART", [("MSX", load_msx('dart'), 6), ("A", dart('A'), 6), ("B", dart('B'), 6), ("C", dart('C'), 6)]),
    ("E-SHOT", [("MSX", load_msx('enemy_shot'), 6), ("A", ebullet('A'), 6), ("B", ebullet('B'), 6), ("C", ebullet('C'), 6)]),
]
compose_board("REFINE BOARD 2 - SMALL ENEMIES", rows2, D + "/refine-board2-small.png")

# ===================================================== board 3: large enemies
def carrier(t):
    img = canvas(48, 32)
    hull = poly_mask(48, 32, [(2, 15), (6, 9), (12, 7), (36, 7), (43, 10),
                              (46, 14), (43, 21), (37, 25), (11, 25), (4, 21)])
    deck = hull & rect_mask(48, 32, 0, 0, 47, 11)
    keel = hull & rect_mask(48, 32, 0, 21, 47, 31)
    core = disc_mask(48, 32, 28, 16, 4) & hull
    tail = hull & rect_mask(48, 32, 0, 0, 6, 31)
    if t == 'A':
        fill(img, hull, BL[2]); fill(img, deck, OR[2]); fill(img, keel, NV[1])
        fill(img, tail, BL[1])
        fill(img, core, RD[2]); img[14, 26] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, hull, BL)
        vgrad(img, deck, OR[1:])
        fill(img, keel, NV[0])
        fill(img, disc_mask(48, 32, 28, 16, 5) & hull, CY[3])
        fill(img, core, RD[3]); img[14, 26] = (*WHT, 255)
        fill(img, edge_mask(hull, 'top'), BL[4])
    else:
        vgrad(img, hull, [NV[0], NV[1], BL[1]])
        fill(img, edge_mask(hull, 'top'), CY[3])
        fill(img, tail, NV[0]); img[15, 3] = (*OR[3], 255); img[16, 3] = (*OR[3], 255)
        fill(img, core, RD[2]); fill(img, edge_mask(core, 'top'), OR[3])
        img[16, 28] = (*WHT, 255)
        for x in (14, 20, 36):
            img[21, x] = (*CY[3], 255)
    return img

def crystal(t):
    img = canvas(32, 32)
    body = poly_mask(32, 32, [(16, 1), (24, 10), (24, 22), (16, 31), (8, 22), (8, 10)])
    top = body & rect_mask(32, 32, 0, 0, 31, 15)
    bot = body & rect_mask(32, 32, 0, 16, 31, 31)
    core = rect_mask(32, 32, 15, 9, 16, 23) & body
    if t == 'A':
        fill(img, top, CY[3]); fill(img, bot, MG[3]); fill(img, core, WHT)
        img[6, 12] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, top, CY)
        vgrad(img, bot, [MG[1], MG[2], MG[3], MG[4], MG[3]])
        fill(img, core, CY[4]); img[12, 15] = (*WHT, 255); img[13, 16] = (*WHT, 255)
        fill(img, edge_mask(body, 'left'), CY[4])
    else:
        vgrad(img, body, [hx('#021e2e'), hx('#0a2a4d'), hx('#2e0a3d')])
        fill(img, edge_mask(body, 'top'), CY[3]); fill(img, edge_mask(body, 'left'), CY[3])
        inner = poly_mask(32, 32, [(16, 8), (20, 13), (20, 19), (16, 24), (12, 19), (12, 13)])
        fill(img, edge_mask(inner, 'top'), MG[3]); fill(img, edge_mask(inner, 'bottom'), MG[3])
        img[16, 16] = (*WHT, 255); img[16, 15] = (*WHT, 255)
    return img

rows3 = [
    ("CARRIER 48x32", [("MSX 16px", load_msx('carrier'), 6), ("A", carrier('A'), 3),
                       ("B", carrier('B'), 3), ("C", carrier('C'), 3)]),
    ("CRYSTAL 32x32", [("MSX 16px", load_msx('crystal'), 6), ("A", crystal('A'), 4),
                       ("B", crystal('B'), 4), ("C", crystal('C'), 4)]),
]
compose_board("REFINE BOARD 3 - LARGE ENEMIES", rows3, D + "/refine-board3-large.png")
print("boards 2-3 done")

# =========================================================== board 4: boss
def diamond_ring(cx, cy, r_out, r_in):
    outer = poly_mask(200, 200, [(cx, cy - r_out), (cx + r_out, cy),
                                 (cx, cy + r_out), (cx - r_out, cy)])
    inner = poly_mask(200, 200, [(cx, cy - r_in), (cx + r_in, cy),
                                 (cx, cy + r_in), (cx - r_in, cy)])
    return (outer & ~inner)[:64, :64]

def boss(t):
    img = canvas(64, 64)
    ring1 = diamond_ring(32, 32, 30, 26)
    ring2 = diamond_ring(32, 32, 20, 17)
    core = rect_mask(64, 64, 25, 25, 38, 38)
    bar = rect_mask(64, 64, 12, 31, 51, 32)
    strut = np.zeros((64, 64), bool)
    for k in range(-23, 24):
        for cx, cy, dx, dy in ((32, 32, 1, -1), (32, 32, 1, 1), (32, 32, -1, 1), (32, 32, -1, -1)):
            x, y = cx + dx * abs(k), cy + dy * abs(k)
            if 18 <= abs(k) <= 27 and 0 <= x < 64 and 0 <= y < 64:
                strut[y, x] = True
    dia27 = poly_mask(64, 64, [(32, 5), (59, 32), (32, 59), (5, 32)])
    dia19 = poly_mask(64, 64, [(32, 13), (51, 32), (32, 51), (13, 32)])
    strut &= dia27 & ~dia19 & ~ring1 & ~ring2
    chev = np.zeros((64, 64), bool)
    for cx, cy, up in ((32, 13, True), (32, 51, False), (13, 32, True), (51, 32, True)):
        if cy == 13:
            tri = poly_mask(64, 64, [(cx - 5, cy + 3), (cx + 5, cy + 3), (cx, cy - 4)])
        elif cy == 51:
            tri = poly_mask(64, 64, [(cx - 5, cy - 3), (cx + 5, cy - 3), (cx, cy + 4)])
        elif cx == 13:
            tri = poly_mask(64, 64, [(cx + 3, cy - 5), (cx + 3, cy + 5), (cx - 4, cy)])
        else:
            tri = poly_mask(64, 64, [(cx - 3, cy - 5), (cx - 3, cy + 5), (cx + 4, cy)])
        chev |= tri
    if t == 'A':
        fill(img, ring1, CY[3]); fill(img, ring2, CY[2]); fill(img, strut, CY[1])
        fill(img, chev, CY[2])
        fill(img, core, MG[3]); fill(img, bar & core, WHT)
    elif t == 'B':
        vgrad(img, ring1, CY); fill(img, edge_mask(ring1, 'top'), CY[4])
        vgrad(img, ring2, CY[1:]); fill(img, strut, CY[1]); vgrad(img, chev, CY[1:])
        vgrad(img, core, MG); img[28, 28] = (*WHT, 255); img[28, 29] = (*WHT, 255)
        img[29, 28] = (*WHT, 255)
    else:
        fill(img, ring1, hx('#04222e')); fill(img, edge_mask(ring1, 'top'), CY[3])
        fill(img, edge_mask(ring1, 'bottom'), CY[1])
        fill(img, ring2, hx('#04222e')); fill(img, edge_mask(ring2, 'top'), CY[2])
        fill(img, strut, CY[0]); fill(img, chev, CY[1])
        fill(img, core, MG[0]); fill(img, edge_mask(core, 'top'), MG[4])
        fill(img, edge_mask(core, 'left'), MG[4])
        img[31, 31] = (*WHT, 255); img[31, 32] = (*WHT, 255)
        img[32, 31] = (*WHT, 255); img[32, 32] = (*WHT, 255)
    return img

def shard(t):
    img = canvas(16, 16)
    body = poly_mask(16, 16, [(8, 0), (12, 5), (12, 10), (8, 15), (4, 10), (4, 5)])
    if t == 'A':
        fill(img, body, CY[3]); fill(img, body & rect_mask(16, 16, 0, 9, 15, 15), CY[1])
        fill(img, body & rect_mask(16, 16, 0, 7, 15, 8), MG[3])
        img[3, 6] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, body, CY)
        fill(img, disc_mask(16, 16, 8, 8, 2), MG[3]); img[6, 6] = (*WHT, 255)
        fill(img, body & rect_mask(16, 16, 0, 2, 15, 3), CY[4])
    else:
        vgrad(img, body, [hx('#021e2e'), hx('#0a3d4d')])
        fill(img, edge_mask(body, 'top'), CY[3]); fill(img, edge_mask(body, 'left'), CY[3])
        img[8, 8] = (*MG[4], 255); img[7, 8] = (*MG[3], 255)
    return img

rows4 = [
    ("BOSS 64x64", [("MSX 48px", load_msx('boss_full'), 3), ("A", boss('A'), 2),
                    ("B", boss('B'), 2), ("C", boss('C'), 2)]),
    ("SHARD 16", [("MSX", load_msx('boss_shard'), 6), ("A", shard('A'), 6),
                  ("B", shard('B'), 6), ("C", shard('C'), 6)]),
]
compose_board("REFINE BOARD 4 - BOSS & SHARD", rows4, D + "/refine-board4-boss.png")

# ================================================== board 5: items & beams
def capsule(t, star=False):
    img = canvas(16, 16)
    outer = rect_mask(16, 16, 1, 2, 14, 13)
    inner = rect_mask(16, 16, 3, 4, 12, 11)
    frame = outer & ~inner
    if star:
        glyph = (rect_mask(16, 16, 7, 5, 8, 11) | rect_mask(16, 16, 5, 7, 11, 8)) & inner
        for gx, gy in ((6, 6), (10, 6), (6, 10), (10, 10)):
            glyph[gy, gx] = True
    else:
        glyph = (rect_mask(16, 16, 6, 5, 10, 6) | rect_mask(16, 16, 7, 5, 8, 11)) & inner
    if t == 'A':
        fill(img, outer, hx('#02122e'))
        fill(img, frame, CY[3]); fill(img, glyph, YL[3])
    elif t == 'B':
        fill(img, outer, hx('#02122e'))
        vgrad(img, frame, CY); fill(img, edge_mask(frame, 'top'), WHT)
        fill(img, glyph, YL[3]); fill(img, glyph & rect_mask(16, 16, 0, 0, 15, 6), hx('#fffbd9'))
    else:
        fill(img, outer, hx('#010a1e'))
        fill(img, frame, CY[1]); fill(img, edge_mask(frame, 'top'), CY[3])
        fill(img, edge_mask(frame, 'bottom'), CY[3])
        fill(img, glyph, YL[2])
    return img

def spark(t):
    img = canvas(16, 16)
    rays = np.zeros((16, 16), bool)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
        for k in range(3, 8):
            x, y = 8 + dx * k, 8 + dy * k
            if 0 <= x < 16 and 0 <= y < 16:
                rays[y, x] = True
    core = disc_mask(16, 16, 8, 8, 2)
    if t == 'A':
        fill(img, rays, OR[2]); fill(img, core, YL[2]); img[8, 8] = (*WHT, 255)
    elif t == 'B':
        fill(img, rays, OR[2])
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            img[8 + dy * 3, 8 + dx * 3] = (*YL[3], 255)
        fill(img, core, YL[3]); img[8, 8] = (*WHT, 255)
    else:
        fill(img, rays, CY[2]); fill(img, core, CY[4]); img[8, 8] = (*WHT, 255)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            img[8 + dy * 7, 8 + dx * 7] = (*CY[4], 255)
    return img

def strip(t, prism=False):
    """96x24 gently waving band."""
    W, H = 96, 24
    img = canvas(W, H)
    pal = CY if not prism else ramp('#0a1e5e', '#0a4dc5', '#6ea8ff', '#c8e5ff', '#ffffff')
    for x in range(W):
        cy = 12 + int(round(3.0 * math.sin(x / 15.0)))
        if t == 'A':
            rows = [(3, pal[1]), (2, pal[2]), (1, pal[3]), (0, WHT)]
            for d, c in rows:
                for yy in (cy - d, cy + d):
                    if 0 <= yy < H: img[yy, x] = (*c, 255)
            if x % 16 == 0:
                for yy in range(cy - 3, cy + 4):
                    if 0 <= yy < H: img[yy, x] = (*pal[1], 255)
        elif t == 'B':
            prof = [(5, pal[0]), (4, pal[1]), (3, pal[2]), (2, pal[3]), (1, pal[4]), (0, WHT)]
            for d, c in prof:
                for yy in (cy - d, cy + d):
                    if 0 <= yy < H: img[yy, x] = (*c, 255)
        else:
            for d, c in [(4, pal[0]), (3, pal[1])]:
                for yy in (cy - d, cy + d):
                    if 0 <= yy < H: img[yy, x] = (*c, 255)
            for yy in (cy - 1, cy, cy + 1):
                img[yy, x] = (*WHT, 255)
    return img

msx_beam = np.asarray(__import__('PIL.Image', fromlist=['Image']).open(
    "/home/hatch/workspace/x68k/lumina-msx-assets/luminaserpent-assets/sprites/beam_strip.png").convert('RGBA'))
msx_prism = np.asarray(__import__('PIL.Image', fromlist=['Image']).open(
    "/home/hatch/workspace/x68k/lumina-msx-assets/luminaserpent-assets/sprites/prism_strip.png").convert('RGBA'))
rows5 = [
    ("CAPSULE T", [("MSX", load_msx('capsule_T'), 6), ("A", capsule('A'), 6),
                   ("B", capsule('B'), 6), ("C", capsule('C'), 6)]),
    ("CAPSULE *", [("MSX", load_msx('capsule_star'), 6), ("A", capsule('A', True), 6),
                   ("B", capsule('B', True), 6), ("C", capsule('C', True), 6)]),
    ("SPARK", [("MSX expl.1", load_msx('explosion_1'), 6), ("A", spark('A'), 6),
               ("B", spark('B'), 6), ("C", spark('C'), 6)]),
    ("LASER", [("MSX strip", msx_beam[:24, :96], 2), ("A", strip('A'), 2),
               ("B", strip('B'), 2), ("C", strip('C'), 2)]),
    ("PRISM", [("MSX strip", msx_prism[:24, :96], 2), ("A", strip('A', True), 2),
               ("B", strip('B', True), 2), ("C", strip('C', True), 2)]),
]
compose_board("REFINE BOARD 5 - ITEMS & BEAMS", rows5, D + "/refine-board5-items.png")
print("boards 4-5 done")
