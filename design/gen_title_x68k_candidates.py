#!/usr/bin/env python3
"""Title-screen candidates for the X68000 build, following the MSX
build's current title route (flat pixel-art grammar):
- B-design logo: DejaVuSans-Bold outline letters (LUMINA / SERPENT),
  a pink beam with a white core through the logo
- realistic Earth disc behind the lower text block
- subtitle band + PRESS + controls laid out like the MSX title

Candidates:
A  MSX B port      - cyan outline, pink beam through LUMINA, Earth centre
B  Echo doubling   - the MSX F route (letterforms doubled as a light
                     echo), beam between the two lines
C  Flat fill       - two-tone filled letters + hard shadow, beam as
                     an underline beneath SERPENT
D  Serpent beam    - the beam snakes as a sine curve through both
                     lines with the faceted ship at its head

Outputs design/title-x68k-candidates/cand-{A,B,C,D}.png + sheet.png
at the X68000 title resolution 512x480.
"""
import math
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 512, 480
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "title-x68k-candidates")
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"

BG = (2, 4, 16)
CYAN = (0, 227, 227)
CYAN_DIM = (0, 150, 165)
PALE_HI = (228, 255, 255)
PALE_LO = (110, 235, 235)
TEAL_SHADOW = (0, 88, 108)
PINK = (255, 93, 162)
WHITE = (255, 255, 255)
SUB_CYAN = (120, 232, 232)
CTRL_WHITE = (232, 240, 255)

RNG = np.random.default_rng(20261101)


# ---------------------------------------------------------------- bg
def starfield(seed=7):
    rng = np.random.default_rng(seed)
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    cols = [(255, 255, 255), (170, 235, 255), (120, 200, 255),
            (90, 140, 220), (200, 220, 255)]
    for _ in range(340):
        x, y = rng.integers(0, W), rng.integers(0, H)
        c = cols[rng.integers(0, len(cols))]
        b = rng.random()
        if b < 0.12:
            d.point((x, y), fill=c)
            d.point((x + 1, y), fill=c)
        else:
            d.point((x, y), fill=c)
    return img


def _fbm(w, h, seed, octaves=5):
    rng = np.random.default_rng(seed)
    total = np.zeros((h, w), dtype=np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        gw, gh = 3 + o * 3, 3 + o * 3
        g = rng.random((gh, gw)).astype(np.float32)
        im = Image.fromarray((g * 255).astype(np.uint8)).resize(
            (w, h), Image.BILINEAR)
        total += np.asarray(im, dtype=np.float32) / 255.0 * amp
        norm += amp
        amp *= 0.55
    return total / norm


def earth_disc(cx, cy, r, seed=23):
    """A realistic-ish Earth disc as an RGBA image of the full canvas."""
    x0, y0 = cx - r, cy - r
    size = r * 2
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    dx, dy = xx - r, yy - r
    dist = np.sqrt(dx * dx + dy * dy)
    inside = dist <= r

    n = _fbm(size, size, seed)
    land = (n > 0.545) & inside
    # smooth the coastline a touch
    land_im = Image.fromarray((land * 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(1.2))
    land = (np.asarray(land_im, dtype=np.float32) / 255.0) > 0.45

    shade = np.clip(0.92 - (dx * 0.75 + dy * 0.55) / (r * 1.9), 0.22, 1.12)
    var = _fbm(size, size, seed + 5)

    ocean = np.zeros((size, size, 3), dtype=np.float32)
    ocean[..., 0] = 8 + 18 * var
    ocean[..., 1] = 42 + 30 * var
    ocean[..., 2] = 116 + 48 * var
    landc = np.zeros((size, size, 3), dtype=np.float32)
    landc[..., 0] = 44 + 52 * var
    landc[..., 1] = 96 + 44 * var
    landc[..., 2] = 40 + 26 * var
    rgb = np.where(land[..., None], landc, ocean)

    clouds = _fbm(size, size, seed + 11)
    clouds = np.clip((clouds - 0.54) * 3.0, 0, 1) * 0.38
    rgb = rgb * (1 - clouds[..., None]) + 225 * clouds[..., None]

    rgb = rgb * shade[..., None]
    # film grain for a textured, less clip-arty surface
    grain = np.random.default_rng(seed + 31).normal(0, 7, rgb.shape[:2])
    rgb = rgb + grain[..., None]
    # polar cap
    cap = inside & (dy < -r * 0.78) & (var > 0.35)
    rgb[cap] = rgb[cap] * 0.35 + np.array([225, 240, 250]) * 0.65

    alpha = np.zeros((size, size), dtype=np.float32)
    edge = np.clip((r - dist) / 2.5, 0, 1)
    alpha[inside] = 255 * edge[inside]
    disc = Image.fromarray(
        np.dstack([np.clip(rgb, 0, 255).astype(np.uint8),
                   alpha.astype(np.uint8)]), "RGBA")

    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((x0 - 6, y0 - 6, x0 + size + 6, y0 + size + 6),
               fill=(64, 140, 255, 90))
    glow = glow.filter(ImageFilter.GaussianBlur(10))
    canvas.alpha_composite(glow)
    canvas.alpha_composite(disc, (x0, y0))
    rim = ImageDraw.Draw(canvas)
    rim.ellipse((x0, y0, x0 + size - 1, y0 + size - 1),
                outline=(120, 190, 255, 110), width=2)
    return canvas


# ---------------------------------------------------------------- logo
def word_mask(word, target_w, target_h=None):
    """Filled-text mask of DejaVuSans-Bold fitted to target_w."""
    size = 40
    while True:
        f = ImageFont.truetype(BOLD, size)
        box = f.getbbox(word)
        w = box[2] - box[0]
        if w >= target_w:
            break
        size += 4
    f = ImageFont.truetype(BOLD, size)
    box = f.getbbox(word)
    w, h = box[2] - box[0], box[3] - box[1]
    m = Image.new("L", (w + 8, h + 8), 0)
    ImageDraw.Draw(m).text((4 - box[0], 4 - box[1]), word, font=f, fill=255)
    if target_h and m.height != target_h:
        m = m.resize((target_w, target_h), Image.LANCZOS)
    return m


def outline_of(mask, px=2):
    dil = mask.filter(ImageFilter.MaxFilter(px * 2 + 1))
    out = np.asarray(dil, dtype=np.int16) - np.asarray(mask, dtype=np.int16)
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "L")


def paste_mask(base, mask, xy, color):
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    solid = Image.new("RGBA", base.size, color + (255,))
    layer.paste(solid, (0, 0), mask if xy == (0, 0) else
                _shifted(mask, base.size, xy))
    base.alpha_composite(layer)


def _shifted(mask, size, xy):
    canvas = Image.new("L", size, 0)
    canvas.paste(mask, xy)
    return canvas


def draw_beam(base, y, x0=0, x1=W, core_h=1, pink_h=3):
    d = ImageDraw.Draw(base)
    d.line((x0, y - pink_h // 2, x1, y - pink_h // 2 + pink_h - 1),
           fill=PINK + (255,), width=pink_h)
    d.line((x0, y, x1, y), fill=WHITE + (255,), width=core_h)


def draw_ship_small(base, cx, cy, s=1.0):
    """Tiny ship glyph riding the beam (MSX title detail)."""
    d = ImageDraw.Draw(base)
    pts = [(cx + 13 * s, cy), (cx - 9 * s, cy - 6 * s), (cx - 5 * s, cy),
           (cx - 9 * s, cy + 6 * s)]
    d.polygon(pts, fill=(235, 255, 255, 255))
    d.polygon([(cx + 13 * s, cy), (cx - 2 * s, cy - 3 * s),
               (cx - 2 * s, cy +  3 * s)], fill=CYAN + (255,))
    d.point((cx - 11 * s, cy), fill=PINK + (255,))


def draw_ship_faceted(base, cx, cy):
    """The game's faceted ship, ~34px, nose right."""
    d = ImageDraw.Draw(base)
    d.polygon([(cx + 17, cy), (cx - 6, cy - 6), (cx - 14, cy - 13),
               (cx - 10, cy)], fill=PALE_HI + (255,))
    d.polygon([(cx + 17, cy), (cx - 10, cy), (cx - 14, cy + 13),
               (cx - 6, cy + 6)], fill=CYAN + (255,))
    d.polygon([(cx - 10, cy - 13), (cx - 16, cy), (cx - 10, cy + 13),
               (cx - 10, cy)], fill=TEAL_SHADOW + (255,))
    d.polygon([(cx + 2, cy - 3), (cx + 8, cy), (cx + 2, cy + 3)],
              fill=PINK + (255,))


# ---------------------------------------------------------------- text
def center_text(base, y, s, size, color, font_path=MONO, tracking=0):
    f = ImageFont.truetype(font_path, size)
    d = ImageDraw.Draw(base)
    if tracking == 0:
        w = d.textlength(s, font=f)
        d.text(((W - w) / 2, y), s, font=f, fill=color + (255,))
        return
    widths = [d.textlength(ch, font=f) for ch in s]
    total = sum(widths) + tracking * (len(s) - 1)
    x = (W - total) / 2
    for ch, wch in zip(s, widths):
        d.text((x, y), ch, font=f, fill=color + (255,))
        x += wch + tracking


def subtitle_block(base, press_color=PINK, band=True):
    if band:
        d = ImageDraw.Draw(base)
        band_im = Image.new("RGBA", (W, 30), (0, 0, 10, 150))
        base.alpha_composite(band_im, (0, 284))
    center_text(base, 291, "BEND THE LASER - FEED THE RESONANCE", 15,
                SUB_CYAN, tracking=1)
    center_text(base, 336, "PRESS SPACE OR PAD BTN", 18, press_color)
    center_text(base, 396, "CURSOR / WASD / PAD : MOVE", 13, CTRL_WHITE)
    center_text(base, 420, "Z / SPACE / TRIGGER : SHOT", 13, CTRL_WHITE)


def logo_masks():
    lum = word_mask("LUMINA", 428)
    ser = word_mask("SERPENT", 470)
    return lum, ser


LUM_Y, SER_Y = 46, 138


def base_canvas(earth_cx=256, earth_cy=428, earth_r=168, seed=23):
    img = starfield().convert("RGBA")
    img.alpha_composite(earth_disc(earth_cx, earth_cy, earth_r, seed))
    return img


# ---------------------------------------------------------------- A
def ink_mid_y(mask):
    rows = np.nonzero(np.asarray(mask).sum(axis=1))[0]
    return int((rows[0] + rows[-1]) // 2)


def cand_a():
    img = base_canvas()
    lum, ser = logo_masks()
    paste_mask(img, outline_of(lum, 2), (42, LUM_Y), CYAN)
    paste_mask(img, outline_of(ser, 2), (21, SER_Y), CYAN)
    # beam through LUMINA's ink middle (letters drawn over it would
    # hide it, so the beam goes last like the MSX bake)
    beam_y = LUM_Y + ink_mid_y(lum)
    draw_beam(img, beam_y)
    draw_ship_small(img, 452, beam_y, 1.1)
    subtitle_block(img)
    return img.convert("RGB")


# ---------------------------------------------------------------- B
def cand_b():
    img = base_canvas(earth_cy=442, earth_r=152)
    lum, ser = logo_masks()
    for word, x, y in ((lum, 42, LUM_Y), (ser, 21, SER_Y)):
        ol = outline_of(word, 2)
        ghost = ol.point(lambda v: int(v * 0.45))
        paste_mask(img, ghost, (x - 7, y), CYAN)
        paste_mask(img, ghost, (x - 14, y), CYAN_DIM)
        paste_mask(img, ol, (x, y), CYAN)
    draw_beam(img, SER_Y - 6)  # beam in the gap between the lines
    draw_ship_small(img, 452, SER_Y - 6, 1.1)
    subtitle_block(img)
    return img.convert("RGB")


# ---------------------------------------------------------------- C
def cand_c():
    img = base_canvas()
    lum, ser = logo_masks()
    for word, x, y in ((lum, 42, LUM_Y), (ser, 21, SER_Y)):
        paste_mask(img, word, (x + 5, y + 6), TEAL_SHADOW)
        # two-tone flat fill: top pale-white, bottom pale cyan
        top = word.crop((0, 0, word.width, word.height * 45 // 100))
        bot = word.crop((0, word.height * 45 // 100, word.width,
                         word.height))
        paste_mask(img, top, (x, y), PALE_HI)
        paste_mask(img, bot, (x, y + word.height * 45 // 100), PALE_LO)
        paste_mask(img, outline_of(word, 1), (x, y), CYAN)
    draw_beam(img, SER_Y + ser.height + 12, x0=30, x1=482)
    subtitle_block(img)
    return img.convert("RGB")


# ---------------------------------------------------------------- D
def cand_d():
    img = base_canvas(earth_cx=150, earth_cy=448, earth_r=138)
    lum, ser = logo_masks()
    paste_mask(img, outline_of(lum, 2), (42, LUM_Y), CYAN)
    paste_mask(img, outline_of(ser, 2), (21, SER_Y), CYAN)
    # serpent beam: a sine curve weaving through both lines
    pts = []
    for x in range(-10, 470):
        t = x / 470.0
        y = 96 + 74 * t + 34 * math.sin(t * math.pi * 1.7 + 0.4)
        pts.append((x, y))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.line(pts, fill=PINK + (120,), width=9)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(5)))
    d = ImageDraw.Draw(img)
    d.line(pts, fill=PINK + (255,), width=3)
    d.line(pts, fill=WHITE + (255,), width=1)
    hx, hy = pts[-1]
    draw_ship_faceted(img, int(hx) + 16, int(hy))
    subtitle_block(img)
    return img.convert("RGB")


def main():
    os.makedirs(OUT, exist_ok=True)
    cands = {"A": cand_a(), "B": cand_b(), "C": cand_c(), "D": cand_d()}
    for k, im in cands.items():
        im.save(os.path.join(OUT, f"cand-{k}.png"))
    sheet = Image.new("RGB", (W * 2 + 12, H * 2 + 12), (24, 24, 24))
    for i, k in enumerate("ABCD"):
        sheet.paste(cands[k], ((i % 2) * (W + 12), (i // 2) * (H + 12)))
    d = ImageDraw.Draw(sheet)
    f = ImageFont.truetype(BOLD, 26)
    for i, k in enumerate("ABCD"):
        x, y = (i % 2) * (W + 12), (i // 2) * (H + 12)
        d.rectangle((x, y, x + 40, y + 34), fill=(0, 0, 0))
        d.text((x + 10, y + 3), k, font=f, fill=(255, 255, 255))
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("written to", OUT)


if __name__ == "__main__":
    main()
