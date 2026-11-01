#!/usr/bin/env python3
"""Board 1: player group (ship / option / shield / missile / shot).
Columns: MSX original | A faithful+shading | B glossy | C neon."""
import sys
sys.path.insert(0, "/home/hatch/workspace/x68k/github/luminaserpent/design")
import numpy as np
from refine_lib import (canvas, fill, poly_mask, disc_mask, ring_mask,
                        rect_mask, vgrad, shade_by, edge_mask, load_msx,
                        compose_board, hx, ramp)

WHT = hx('#ffffff')
CY = ramp('#0a4d5e', '#07908f', '#00e5e5', '#8ff7f2', '#e8fffc')  # deep..pale
NV = ramp('#02022e', '#000049', '#0a2a6e')
OR = ramp('#6e1e00', '#e55a18', '#ff9249', '#ffc98a')
MG = ramp('#490049', '#920049', '#ff00e5')


# --------------------------------------------------------------- ship 32x16
def ship_base():
    hull = poly_mask(32, 16, [(2, 6), (7, 3), (12, 3), (16, 5), (25, 6),
                              (31, 8), (25, 10), (15, 12), (7, 13), (2, 10)])
    fin = poly_mask(32, 16, [(4, 6), (6, 1), (9, 2), (11, 6)])
    belly = hull & rect_mask(32, 16, 0, 10, 31, 15)
    canopy = disc_mask(32, 16, 20, 6, 2) & hull
    engine = hull & rect_mask(32, 16, 0, 0, 2, 15)
    return hull, fin, belly, canopy, engine


def ship(t):
    hull, fin, belly, canopy, engine = ship_base()
    img = canvas(32, 16)
    if t == 'A':
        fill(img, hull, CY[3])
        fill(img, hull & rect_mask(32, 16, 0, 9, 31, 15), CY[2])
        fill(img, belly, NV[1])
        fill(img, fin, NV[1]); fill(img, fin & rect_mask(32, 16, 0, 0, 31, 2), WHT)
        fill(img, canopy, WHT)
        fill(img, engine, OR[2])
        img[5, 25] = (*WHT, 255); img[5, 26] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, hull, CY)
        vgrad(img, belly, NV)
        vgrad(img, fin, CY[1:])
        fill(img, canopy, CY[4]); img[5, 20] = (*WHT, 255); img[6, 19] = (*WHT, 255)
        vgrad(img, engine, OR[1:])
        img[4, 26] = (*WHT, 255); img[4, 27] = (*CY[4], 255)
    else:
        vgrad(img, hull, [NV[0], NV[1], hx('#0a3d6e'), hx('#0a5e8f')])
        fill(img, edge_mask(hull, 'top'), CY[3])
        fill(img, belly, NV[0])
        fill(img, canopy, WHT); fill(img, edge_mask(canopy, 'bottom'), CY[3])
        fill(img, fin, NV[1]); fill(img, edge_mask(fin, 'top'), CY[3])
        fill(img, engine, OR[3]); img[7, 1] = (*WHT, 255); img[8, 1] = (*WHT, 255)
        img[8, 30] = (*CY[3], 255)
    return img


# ------------------------------------------------------------- option 16x16
def option(t):
    img = canvas(16, 16)
    dome = disc_mask(16, 16, 8, 9, 6) & rect_mask(16, 16, 0, 0, 15, 11)
    base = rect_mask(16, 16, 3, 11, 12, 12) & disc_mask(16, 16, 8, 9, 7)
    if t == 'A':
        fill(img, dome, CY[3])
        fill(img, dome & rect_mask(16, 16, 0, 0, 15, 5), WHT)
        fill(img, dome & rect_mask(16, 16, 0, 9, 15, 15), CY[2])
        fill(img, base, NV[1])
    elif t == 'B':
        vgrad(img, dome, CY)
        fill(img, disc_mask(16, 16, 6, 5, 2), WHT)
        vgrad(img, base, NV)
        img[8, 11] = (*OR[3], 255)
    else:
        vgrad(img, dome, [NV[1], hx('#0a3d6e'), hx('#0a6e8f')])
        fill(img, edge_mask(dome, 'top'), CY[3])
        fill(img, base, NV[0]); fill(img, edge_mask(base, 'bottom'), CY[2])
        img[8, 8] = (*WHT, 255)
    return img


# ------------------------------------------------------------- shield 48x48
def shield(t):
    img = canvas(48, 48)
    ring = ring_mask(48, 48, 24, 24, 22, 16)
    if t == 'A':
        fill(img, ring, CY[2])
        top = ring & rect_mask(48, 48, 0, 0, 47, 15)
        fill(img, top, CY[3])
        fill(img, ring & rect_mask(48, 48, 0, 36, 47, 47), CY[1])
        for x, y in ((24, 2), (23, 2), (25, 2)):
            img[y, x] = (*WHT, 255)
    elif t == 'B':
        vgrad(img, ring, CY)
        fill(img, edge_mask(ring, 'top'), CY[4])
        for cx, cy in ((10, 10), (38, 10), (10, 38), (38, 38)):
            img[cy, cx] = (*WHT, 255)
    else:
        fill(img, ring, NV[0])
        fill(img, edge_mask(ring, 'top'), CY[3])
        fill(img, edge_mask(ring, 'bottom'), CY[1])
        inner = ring & disc_mask(48, 48, 24, 24, 17)
        fill(img, inner, CY[2])
        for a in range(0, 360, 45):
            import math
            x = int(24 + 19 * math.cos(math.radians(a)))
            y = int(24 + 19 * math.sin(math.radians(a)))
            img[y, x] = (*WHT, 255)
    return img


# ------------------------------------------------------------ missile 16x16
def missile(t):
    img = canvas(16, 16)
    body = poly_mask(16, 16, [(1, 6), (8, 4), (13, 6), (15, 8), (13, 10),
                              (8, 12), (1, 10)])
    nose = body & rect_mask(16, 16, 12, 0, 15, 15)
    fin_t = poly_mask(16, 16, [(2, 6), (4, 3), (6, 5)])
    fin_b = poly_mask(16, 16, [(2, 10), (4, 13), (6, 11)])
    if t == 'A':
        fill(img, body, OR[2])
        fill(img, body & rect_mask(16, 16, 0, 10, 15, 15), OR[1])
        fill(img, nose, WHT)
        fill(img, fin_t | fin_b, MG[1])
    elif t == 'B':
        vgrad(img, body, OR)
        fill(img, nose, WHT); fill(img, nose & rect_mask(16, 16, 14, 0, 15, 15), CY[4])
        fill(img, fin_t | fin_b, MG[1])
        fill(img, edge_mask(body, 'top'), OR[3])
    else:
        vgrad(img, body, [hx('#2e0202'), hx('#6e0a0a'), hx('#920049')])
        fill(img, edge_mask(body, 'top'), OR[3])
        fill(img, nose, WHT)
        fill(img, fin_t | fin_b, NV[1]); fill(img, edge_mask(fin_t, 'top'), MG[2])
    return img


# -------------------------------------------------------------- shot 16x16
def shot(t):
    img = canvas(16, 16)
    if t == 'A':
        fill(img, rect_mask(16, 16, 4, 6, 11, 6), hx('#ffe500'))
        fill(img, rect_mask(16, 16, 3, 7, 13, 8), WHT)
        fill(img, rect_mask(16, 16, 4, 9, 11, 9), hx('#ffe500'))
    elif t == 'B':
        fill(img, rect_mask(16, 16, 5, 5, 10, 5), hx('#fff7ae'))
        fill(img, rect_mask(16, 16, 4, 6, 12, 6), hx('#ffe500'))
        fill(img, rect_mask(16, 16, 2, 7, 14, 8), WHT)
        fill(img, rect_mask(16, 16, 4, 9, 12, 9), hx('#ffe500'))
        fill(img, rect_mask(16, 16, 5, 10, 10, 10), hx('#fff7ae'))
    else:
        fill(img, rect_mask(16, 16, 4, 6, 11, 6), CY[2])
        fill(img, rect_mask(16, 16, 2, 7, 14, 8), WHT)
        fill(img, rect_mask(16, 16, 4, 9, 11, 9), CY[2])
        img[7, 1] = (*CY[3], 255); img[8, 1] = (*CY[3], 255)
        img[7, 15] = (*CY[4], 255); img[8, 15] = (*CY[4], 255)
    return img


rows = [
    ("SHIP 32x16", [("MSX", load_msx('ship_full'), 5),
                    ("A faithful+shade", ship('A'), 5),
                    ("B glossy", ship('B'), 5),
                    ("C neon", ship('C'), 5)]),
    ("OPTION 16", [("MSX", load_msx('option_echo'), 6),
                   ("A", option('A'), 6), ("B", option('B'), 6),
                   ("C", option('C'), 6)]),
    ("SHIELD 48", [("MSX halo", load_msx('halo'), 6),
                   ("A", shield('A'), 3), ("B", shield('B'), 3),
                   ("C", shield('C'), 3)]),
    ("MISSILE 16", [("MSX", load_msx('missile'), 6),
                    ("A", missile('A'), 6), ("B", missile('B'), 6),
                    ("C", missile('C'), 6)]),
    ("SHOT 16", [("MSX", load_msx('player_shot'), 6),
                 ("A", shot('A'), 6), ("B", shot('B'), 6),
                 ("C", shot('C'), 6)]),
]
p = compose_board("REFINE BOARD 1 - PLAYER GROUP  (GRB555 colours)", rows,
                  "/home/hatch/workspace/x68k/github/luminaserpent/design/refine-board1-ship.png")
print("wrote", p)
