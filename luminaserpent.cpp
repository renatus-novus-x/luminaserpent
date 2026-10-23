/*
 * LUMINA SERPENT for X68000 - elf2x68k, C++, no Human68k required.
 * An original side-scrolling STG.
 *
 * Original systems:
 *  - The "Light Serpent" laser: while firing, the beam follows the
 *    ship's own flight path with a distance-dependent delay, so it
 *    bends and ripples like a luminous serpent. The sharper it bends,
 *    the more its colour shifts through the spectrum and the faster
 *    the RESONANCE gauge fills. At full resonance the beam becomes
 *    a wide rainbow PRISM beam for 10 seconds.
 *  - Prism crystals: floating crystal enemies whose destruction
 *    bursts into an expanding rainbow ring and feeds resonance.
 *  - Boss: ABYSS PRISM LEVIATHAN, a crystal leviathan whose core is
 *    shielded by four orbiting prism shards.
 *
 * Display : CRTMOD 12 retimed to 60 Hz (CRTC pokes), 512x480, 65536 cols
 * Input   : cursor keys / joypad, Z or SPACE = shot, X = power up
 * Sound   : YM2151 (OPM) via IOCS OPMSET, original BGM + SE
 */

#include <x68k/iocs.h>
#include "earth_bg.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static constexpr int SCR_W = 512;
static constexpr int SCR_H = 480;
static constexpr int CRT_MODE = 12;
static constexpr int PLAY_Y0 = 54;
static constexpr int PLAY_Y1 = 430;
static constexpr int FP = 8;

static constexpr uint16_t GRB(int r, int g, int b) {
  return (uint16_t)(((g & 31) << 11) | ((r & 31) << 6) | ((b & 31) << 1) | 1);
}
static constexpr uint16_t COL_WHITE = GRB(31, 31, 31);
static constexpr uint16_t COL_BLACK = GRB(0, 0, 0);

/* rainbow palette used for prism / crystal / resonance effects */
static const uint16_t RAINBOW[7] = {
  GRB(31, 6, 6), GRB(31, 16, 2), GRB(30, 28, 2), GRB(6, 30, 8),
  GRB(4, 24, 30), GRB(10, 10, 31), GRB(26, 6, 30),
};

/* sine table, 64 steps, value = sin * 256 */
static const int16_t SIN64[64] = {
  0, 25, 50, 74, 98, 120, 142, 162, 180, 197, 212, 225, 236, 244, 250, 254,
  256, 254, 250, 244, 236, 225, 212, 197, 180, 162, 142, 120, 98, 74, 50, 25,
  0, -25, -50, -74, -98, -120, -142, -162, -180, -197, -212, -225, -236, -244, -250, -254,
  -256, -254, -250, -244, -236, -225, -212, -197, -180, -162, -142, -120, -98, -74, -50, -25,
};
static int sin64(int idx) { return SIN64[idx & 63]; }
static int cos64(int idx) { return SIN64[(idx + 16) & 63]; }

/* deep-space vertical gradient, 4 stops */
static uint16_t bg_color(int y) {
  static const int ys[4] = {0, 170, 330, 479};
  static const int rs[4] = {0, 0, 1, 1};
  static const int gs[4] = {1, 1, 2, 3};
  static const int bs[4] = {3, 5, 7, 9};
  int s = 0;
  while (s < 2 && y > ys[s + 1]) s++;
  if (y <= ys[0]) return GRB(rs[0], gs[0], bs[0]);
  if (s > 2) s = 2;
  int span = ys[s + 1] - ys[s];
  int t = span ? (y - ys[s]) * 31 / span : 0;
  int r = rs[s] + (rs[s + 1] - rs[s]) * t / 31;
  int g = gs[s] + (gs[s + 1] - gs[s]) * t / 31;
  int b = bs[s] + (bs[s + 1] - bs[s]) * t / 31;
  return GRB(r, g, b);
}

/* ------------------------------------------------------------------ */
/* 60 Hz retiming + VDISP sync (technique from renatus-novus-x/pong,  */
/* proven in breakout)                                                */
/* ------------------------------------------------------------------ */
#define X68K_CRTC_BASE 0x00E80000UL
#define X68K_MFP_GPIP  0x00E88001UL
static int gpip_vdisp() {
  return _iocs_b_bpeek((const void *)X68K_MFP_GPIP) & 0x10;
}
static int wait_vdisp_edge() {
  int start = _iocs_ontime().sec;
  int level = gpip_vdisp();
  for (;;) {
    int next = gpip_vdisp();
    if (_iocs_ontime().sec - start > 100) return -1;
    if (next != level) { level = next; if (level != 0) return 0; }
  }
}
static void crtc_poke(int reg, int value) {
  _iocs_b_wpoke((void *)(X68K_CRTC_BASE + reg * 2), value);
}
static void set_60hz() {
  if (wait_vdisp_edge() != 0) return;
  crtc_poke(5, 0x0001); crtc_poke(6, 0x0022);
  crtc_poke(7, 0x0202); crtc_poke(4, 0x020C);
}

/* 8x8 font: "8x8 ORIGINAL FONT 1 fix" from the MSXPen shared
   code (https://msxpen.com/codes/-Nq8q6wabU6mJ4onDmYE),
   glyphs 0x20..0x7F, MSB = leftmost pixel. */
static const uint8_t FONT8[96][8] = {
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x20  */
  {0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00}, /* 0x21 ! */
  {0x24,0x24,0x48,0x00,0x00,0x00,0x00,0x00}, /* 0x22 " */
  {0x44,0xfe,0x44,0x44,0x44,0xfe,0x44,0x00}, /* 0x23 # */
  {0x10,0x7c,0x90,0x7c,0x12,0xfc,0x10,0x00}, /* 0x24 $ */
  {0xe2,0xa4,0xe8,0x10,0x2e,0x4a,0x8e,0x00}, /* 0x25 % */
  {0x30,0x48,0x30,0x62,0x94,0x8c,0x72,0x00}, /* 0x26 & */
  {0x20,0x20,0x40,0x00,0x00,0x00,0x00,0x00}, /* 0x27 ' */
  {0x08,0x10,0x20,0x20,0x20,0x10,0x08,0x00}, /* 0x28 ( */
  {0x20,0x10,0x08,0x08,0x08,0x10,0x20,0x00}, /* 0x29 ) */
  {0x00,0x00,0x28,0x10,0x28,0x00,0x00,0x00}, /* 0x2a  */
  {0x00,0x00,0x10,0x38,0x10,0x00,0x00,0x00}, /* 0x2b + */
  {0x00,0x00,0x00,0x00,0x40,0x40,0x80,0x00}, /* 0x2c , */
  {0x00,0x00,0x00,0x7c,0x00,0x00,0x00,0x00}, /* 0x2d - */
  {0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00}, /* 0x2e . */
  {0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00}, /* 0x2f  */
  {0x7c,0x86,0x8a,0x92,0xa2,0xc2,0x7c,0x00}, /* 0x30 0 */
  {0x10,0x30,0x50,0x10,0x10,0x10,0x10,0x00}, /* 0x31 1 */
  {0xfc,0x02,0x02,0x7c,0x80,0x80,0xfe,0x00}, /* 0x32 2 */
  {0xfc,0x02,0x02,0x7c,0x02,0x02,0xfc,0x00}, /* 0x33 3 */
  {0x82,0x82,0x82,0xfe,0x02,0x02,0x02,0x00}, /* 0x34 4 */
  {0xfe,0x80,0x80,0xfc,0x02,0x02,0xfc,0x00}, /* 0x35 5 */
  {0x7c,0x80,0x80,0xfc,0x82,0x82,0x7c,0x00}, /* 0x36 6 */
  {0xfe,0x82,0x82,0x02,0x02,0x02,0x02,0x00}, /* 0x37 7 */
  {0x7c,0x82,0x82,0x7c,0x82,0x82,0x7c,0x00}, /* 0x38 8 */
  {0x7c,0x82,0x82,0x7e,0x02,0x02,0x7c,0x00}, /* 0x39 9 */
  {0x00,0x30,0x30,0x00,0x30,0x30,0x00,0x00}, /* 0x3a : */
  {0x00,0x30,0x30,0x00,0x30,0x30,0x60,0x00}, /* 0x3b ; */
  {0x06,0x18,0x60,0x80,0x60,0x18,0x06,0x00}, /* 0x3c < */
  {0x00,0x00,0xfe,0x00,0x00,0xfe,0x00,0x00}, /* 0x3d = */
  {0xc0,0x30,0x0c,0x02,0x0c,0x30,0xc0,0x00}, /* 0x3e > */
  {0x38,0x44,0x44,0x08,0x10,0x00,0x10,0x00}, /* 0x3f ? */
  {0x38,0x44,0x9a,0xaa,0xaa,0xac,0x38,0x00}, /* 0x40 @ */
  {0x7c,0x82,0x82,0xfe,0x82,0x82,0x82,0x00}, /* 0x41 A */
  {0xfc,0x82,0x82,0xfc,0x82,0x82,0xfc,0x00}, /* 0x42 B */
  {0x7e,0x80,0x80,0x80,0x80,0x80,0x7e,0x00}, /* 0x43 C */
  {0xf8,0x84,0x82,0x82,0x82,0x84,0xf8,0x00}, /* 0x44 D */
  {0xfe,0x80,0x80,0xfc,0x80,0x80,0xfe,0x00}, /* 0x45 E */
  {0xfe,0x80,0x80,0xfc,0x80,0x80,0x80,0x00}, /* 0x46 F */
  {0x7c,0x80,0x80,0x9e,0x82,0x82,0x7c,0x00}, /* 0x47 G */
  {0x82,0x82,0x82,0xfe,0x82,0x82,0x82,0x00}, /* 0x48 H */
  {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00}, /* 0x49 I */
  {0x02,0x02,0x02,0x02,0x82,0x82,0x7c,0x00}, /* 0x4a J */
  {0x82,0x84,0x88,0xf0,0x88,0x84,0x82,0x00}, /* 0x4b K */
  {0x80,0x80,0x80,0x80,0x80,0x80,0xfe,0x00}, /* 0x4c L */
  {0x82,0xc6,0xaa,0x92,0x82,0x82,0x82,0x00}, /* 0x4d M */
  {0x82,0xc2,0xa2,0x92,0x8a,0x86,0x82,0x00}, /* 0x4e N */
  {0x7c,0x82,0x82,0x82,0x82,0x82,0x7c,0x00}, /* 0x4f O */
  {0xfc,0x82,0x82,0xfc,0x80,0x80,0x80,0x00}, /* 0x50 P */
  {0x7c,0x82,0x82,0x92,0x8a,0x84,0x7a,0x00}, /* 0x51 Q */
  {0xfc,0x82,0x82,0xfc,0x88,0x84,0x82,0x00}, /* 0x52 R */
  {0x7c,0x82,0x80,0x7c,0x02,0x82,0x7c,0x00}, /* 0x53 S */
  {0xfe,0x10,0x10,0x10,0x10,0x10,0x10,0x00}, /* 0x54 T */
  {0x82,0x82,0x82,0x82,0x82,0x82,0x7c,0x00}, /* 0x55 U */
  {0x82,0x82,0x44,0x44,0x28,0x28,0x10,0x00}, /* 0x56 V */
  {0x92,0x92,0x92,0xaa,0xaa,0x44,0x44,0x00}, /* 0x57 W */
  {0x82,0x44,0x28,0x10,0x28,0x44,0x82,0x00}, /* 0x58 X */
  {0x82,0x44,0x28,0x10,0x10,0x10,0x10,0x00}, /* 0x59 Y */
  {0xfe,0x04,0x08,0x10,0x20,0x40,0xfe,0x00}, /* 0x5a Z */
  {0x38,0x20,0x20,0x20,0x20,0x20,0x38,0x00}, /* 0x5b [ */
  {0x82,0x44,0x28,0x7c,0x10,0x7c,0x10,0x00}, /* 0x5c \ */
  {0x38,0x08,0x08,0x08,0x08,0x08,0x38,0x00}, /* 0x5d ] */
  {0x10,0x28,0x44,0x82,0x00,0x00,0x00,0x00}, /* 0x5e ^ */
  {0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0x00}, /* 0x5f _ */
  {0x20,0x10,0x08,0x00,0x00,0x00,0x00,0x00}, /* 0x60 ` */
  {0x00,0x00,0x38,0x04,0x3c,0x44,0x3c,0x00}, /* 0x61 a */
  {0x40,0x40,0x78,0x44,0x44,0x44,0x78,0x00}, /* 0x62 b */
  {0x00,0x00,0x3c,0x40,0x40,0x40,0x3c,0x00}, /* 0x63 c */
  {0x04,0x04,0x3c,0x44,0x44,0x44,0x3c,0x00}, /* 0x64 d */
  {0x00,0x00,0x38,0x44,0x7c,0x40,0x38,0x00}, /* 0x65 e */
  {0x0c,0x10,0x7c,0x10,0x10,0x10,0x10,0x00}, /* 0x66 f */
  {0x00,0x00,0x3c,0x44,0x3c,0x04,0x38,0x00}, /* 0x67 g */
  {0x40,0x40,0x78,0x44,0x44,0x44,0x44,0x00}, /* 0x68 h */
  {0x10,0x00,0x10,0x10,0x10,0x10,0x10,0x00}, /* 0x69 i */
  {0x08,0x00,0x08,0x08,0x08,0x48,0x30,0x00}, /* 0x6a j */
  {0x20,0x20,0x24,0x28,0x30,0x28,0x24,0x00}, /* 0x6b k */
  {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00}, /* 0x6c l */
  {0x00,0x00,0x68,0x54,0x54,0x54,0x54,0x00}, /* 0x6d m */
  {0x00,0x00,0x78,0x44,0x44,0x44,0x44,0x00}, /* 0x6e n */
  {0x00,0x00,0x38,0x44,0x44,0x44,0x38,0x00}, /* 0x6f o */
  {0x00,0x00,0x78,0x44,0x44,0x78,0x40,0x00}, /* 0x70 p */
  {0x00,0x00,0x38,0x44,0x44,0x3c,0x04,0x00}, /* 0x71 q */
  {0x00,0x00,0x58,0x64,0x40,0x40,0x40,0x00}, /* 0x72 r */
  {0x00,0x00,0x3c,0x40,0x38,0x04,0x78,0x00}, /* 0x73 s */
  {0x00,0x10,0x7c,0x10,0x10,0x10,0x0c,0x00}, /* 0x74 t */
  {0x00,0x00,0x44,0x44,0x44,0x44,0x3c,0x00}, /* 0x75 u */
  {0x00,0x00,0x44,0x44,0x28,0x28,0x10,0x00}, /* 0x76 v */
  {0x00,0x00,0x54,0x54,0x54,0x54,0x28,0x00}, /* 0x77 w */
  {0x00,0x00,0x44,0x28,0x10,0x28,0x44,0x00}, /* 0x78 x */
  {0x00,0x00,0x44,0x44,0x3c,0x04,0x38,0x00}, /* 0x79 y */
  {0x00,0x00,0x7c,0x08,0x10,0x20,0x7c,0x00}, /* 0x7a z */
  {0x18,0x20,0x20,0x40,0x20,0x20,0x18,0x00}, /* 0x7b { */
  {0x10,0x10,0x10,0x00,0x10,0x10,0x10,0x00}, /* 0x7c | */
  {0x30,0x08,0x08,0x04,0x08,0x08,0x30,0x00}, /* 0x7d } */
  {0x22,0x54,0x88,0x00,0x00,0x00,0x00,0x00}, /* 0x7e ~ */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x7f  */
};

static const uint8_t *glyph_for(char c) {
  unsigned char uc = (unsigned char)c;
  if (uc < 0x20 || uc > 0x7f) uc = 0x20;
  return FONT8[uc - 0x20];
}

/* ------------------------------------------------------------------ */
/* GVRAM direct drawing. Probe (probe/gvram, WebX68k): in CRTMOD 12   */
/* the visible plane is 512 words wide starting at 0xC00000, and      */
/* direct writes beat IOCS for the many small shapes an STG draws.    */
/* ------------------------------------------------------------------ */
static volatile uint16_t *const GVRAM = (volatile uint16_t *)0xC00000UL;
static constexpr int GV_STRIDE = 512;

static inline void gv_hfill(int x, int y, int n, uint16_t c) {
  volatile uint16_t *d = GVRAM + y * GV_STRIDE + x;
  if ((x & 1) && n > 0) { *d++ = c; n--; }
  uint32_t cc = ((uint32_t)c << 16) | c;
  volatile uint32_t *d32 = (volatile uint32_t *)d;
  while (n >= 2) { *d32++ = cc; n -= 2; }
  if (n) *(volatile uint16_t *)d32 = c;
}
static inline void gv_pset_raw(int x, int y, uint16_t c) {
  GVRAM[y * GV_STRIDE + x] = c;
}

/* background cache in RAM (space + stars + Earth, built once).
   Erasing a rectangle becomes a fast RAM->GVRAM copy. */
static uint16_t bg_cache[SCR_H][SCR_W] __attribute__((aligned(4)));
static void blit_bg_rect(int x1, int y1, int x2, int y2) {
  if (x1 < 0) x1 = 0; if (y1 < 0) y1 = 0;
  if (x2 >= SCR_W) x2 = SCR_W - 1; if (y2 >= SCR_H) y2 = SCR_H - 1;
  if (x1 > x2 || y1 > y2) return;
  int n0 = x2 - x1 + 1;
  for (int y = y1; y <= y2; y++) {
    const uint16_t *src = &bg_cache[y][x1];
    volatile uint16_t *dst = GVRAM + y * GV_STRIDE + x1;
    int n = n0;
    if ((x1 & 1) && n > 0) { *dst++ = *src++; n--; }
    const uint32_t *s32 = (const uint32_t *)src;
    volatile uint32_t *d32 = (volatile uint32_t *)dst;
    while (n >= 2) { *d32++ = *s32++; n -= 2; }
    if (n) *(volatile uint16_t *)d32 = *(const uint16_t *)s32;
  }
}

/* ------------------------------------------------------------------ */
/* graphics helpers                                                   */
/* ------------------------------------------------------------------ */
class Gfx {
public:
  static void fill(int x1, int y1, int x2, int y2, uint16_t color) {
    if (x1 > x2 || y1 > y2) return;
    if (x2 < 0 || y2 < 0 || x1 >= SCR_W || y1 >= SCR_H) return;
    if (x1 < 0) x1 = 0; if (y1 < 0) y1 = 0;
    if (x2 >= SCR_W) x2 = SCR_W - 1; if (y2 >= SCR_H) y2 = SCR_H - 1;
    if ((x2 - x1 + 1) * (y2 - y1 + 1) >= 16384) {
      /* very large fills: the ROM routine is already optimal */
      iocs_fillptr f = {(short)x1, (short)y1, (short)x2, (short)y2, color};
      _iocs_fill(&f);
      return;
    }
    int n = x2 - x1 + 1;
    for (int y = y1; y <= y2; y++) gv_hfill(x1, y, n, color);
  }
  static void line(int x1, int y1, int x2, int y2, uint16_t color) {
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int sx = x1 < x2 ? 1 : -1;
    int dy = y2 > y1 ? y1 - y2 : y2 - y1; /* -abs(y2-y1) */
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      if (x1 >= 0 && x1 < SCR_W && y1 >= 0 && y1 < SCR_H)
        gv_pset_raw(x1, y1, color);
      if (x1 == x2 && y1 == y2) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x1 += sx; }
      if (e2 <= dx) { err += dx; y1 += sy; }
    }
  }
  static void pset(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= SCR_W || y >= SCR_H) return;
    gv_pset_raw(x, y, color);
  }
  static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    if (n == 0) return 0;
    return n * 8 * scale + (n - 1) * (scale > 1 ? 2 : 1);
  }
  static void text(int x, int y, int scale, uint16_t color, const char *s) {
    int adv = 8 * scale + (scale > 1 ? 2 : 1);
    for (int i = 0; s[i]; i++) {
      const uint8_t *g = glyph_for(s[i]);
      for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++)
          if (bits & (0x80 >> col))
            fill(x + col * scale, y + row * scale,
                 x + col * scale + scale - 1, y + row * scale + scale - 1, color);
      }
      x += adv;
    }
  }
  static void text_center(int y, int scale, uint16_t color, const char *s) {
    text((SCR_W - text_width(s, scale)) / 2, y, scale, color, s);
  }
  static void number(int x, int y, int scale, uint16_t color, int value, int width) {
    char buf[16];
    for (int i = width - 1; i >= 0; i--) { buf[i] = (char)('0' + (value % 10)); value /= 10; }
    buf[width] = 0;
    text(x, y, scale, color, buf);
  }
};

/* stars: static screen positions, twinkle by changing brightness */
static constexpr int STAR_N = 380;
static int star_x[STAR_N], star_y[STAR_N], star_b[STAR_N];
static constexpr int EARTH_CX = 388, EARTH_CY = 238, EARTH_R = 88;
static void stars_init() {
  uint32_t s = 0x9E3779B9u;
  for (int i = 0; i < STAR_N; i++) {
    for (;;) {
      s = s * 1103515245u + 12345u; star_x[i] = (int)((s >> 8) % SCR_W);
      s = s * 1103515245u + 12345u; star_y[i] = (int)((s >> 8) % SCR_H);
      int dx = star_x[i] - EARTH_CX, dy = star_y[i] - EARTH_CY;
      if (dx * dx + dy * dy > 112 * 112) break;   /* keep the Earth disc clear */
    }
    s = s * 1103515245u + 12345u; star_b[i] = (int)((s >> 8) % 5);
  }
}
static uint16_t star_color(int b, int tw) {
  int v = b + ((tw & 32) ? 1 : 0);
  if (v > 5) v = 5;
  switch (v) {
    case 0: return GRB(4, 5, 8);
    case 1: return GRB(7, 9, 13);
    case 2: return GRB(11, 13, 18);
    case 3: return GRB(16, 18, 23);
    case 4: return GRB(22, 24, 28);
    default: return GRB(28, 29, 30);
  }
}

/* ------------------------------------------------------------------ */
/* Space background bake: gradient + nebula tint + starfield, plus a  */
/* pre-rendered Earth tile (earth_bg.h). Generating the planet at     */
/* runtime proved far too slow on a 68000 (minutes), so the tile is   */
/* rendered on the host by tools/gen_earth_tile.py (which compiles    */
/* this project's own procedural bake in tools/earth_gen.cpp) and     */
/* the X68000 only blits it at boot.                                  */
/* ------------------------------------------------------------------ */
static uint8_t g_lattice[64 * 64];
static void lattice_init() {
  uint32_t s = 0x1F123BB5u;
  for (int i = 0; i < 64 * 64; i++) {
    s = s * 1103515245u + 12345u;
    g_lattice[i] = (uint8_t)(s >> 24);
  }
}
static int lat_at(int x, int y) { return g_lattice[((y & 63) << 6) | (x & 63)]; }
/* bilinear value noise with a power-of-two cell size (shift = log2) */
static int noise2s(int x, int y, int shift) {
  int cx = x >> shift, cy = y >> shift;
  int fx = (x & ((1 << shift) - 1)) << (8 - shift);
  int fy = (y & ((1 << shift) - 1)) << (8 - shift);
  int v00 = lat_at(cx, cy), v10 = lat_at(cx + 1, cy);
  int v01 = lat_at(cx, cy + 1), v11 = lat_at(cx + 1, cy + 1);
  int top = v00 + ((v10 - v00) * fx >> 8);
  int bot = v01 + ((v11 - v01) * fx >> 8);
  return top + ((bot - top) * fy >> 8);
}
static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static void build_bg_cache() {
  lattice_init();
  /* faint nebula tint on a coarse grid */
  static int tint_r[61][65], tint_b[61][65];
  for (int gy = 0; gy < 61; gy++)
    for (int gx = 0; gx < 65; gx++) {
      int n1 = noise2s(gx * 8 + 977 * 11, gy * 8 + 631 * 11, 8);
      int n2 = noise2s(gx * 8 + 977 * 12, gy * 8 + 631 * 12, 8);
      tint_r[gy][gx] = n1 > 170 ? (n1 - 170) / 42 : 0;   /* faint purple wash */
      tint_b[gy][gx] = n2 > 170 ? (n2 - 170) / 34 : 0;   /* faint teal wash */
    }
  /* gradient + tint (tint lerped vertically per row, grid lookup across) */
  for (int y = 0; y < SCR_H; y++) {
    int gy = y >> 3, fy = y & 7;
    int row_tr[65], row_tb[65];
    for (int gx = 0; gx < 65; gx++) {
      row_tr[gx] = (tint_r[gy][gx] * (8 - fy) + tint_r[gy + 1][gx] * fy) / 8;
      row_tb[gx] = (tint_b[gy][gx] * (8 - fy) + tint_b[gy + 1][gx] * fy) / 8;
    }
    uint16_t c = bg_color(y);
    int cr = ((c >> 6) & 31) * 8, cg = ((c >> 11) & 31) * 8, cb = ((c >> 1) & 31) * 8;
    for (int x = 0; x < SCR_W; x++) {
      int jit = (int)((x * 73 + y * 149) & 7) - 4;
      int gx = x >> 3;
      bg_cache[y][x] = GRB(clamp255(cr + row_tr[gx] * 8 + jit) >> 3,
                           clamp255(cg + jit) >> 3,
                           clamp255(cb + row_tb[gx] * 8 + jit) >> 3);
    }
  }
  /* starfield: white-blue levels, a few warm / cyan, sparkle crosses */
  for (int i = 0; i < STAR_N; i++) {
    int v = star_b[i];
    uint16_t c = star_color(v, 0);
    if (i % 9 == 0) {
      static const uint16_t warm[5] = {GRB(6, 5, 3), GRB(10, 9, 5), GRB(15, 13, 7),
                                       GRB(21, 18, 10), GRB(27, 23, 14)};
      c = warm[v];
    } else if (i % 7 == 0) {
      static const uint16_t cyan[5] = {GRB(3, 6, 8), GRB(5, 11, 13), GRB(7, 16, 19),
                                       GRB(10, 22, 25), GRB(13, 27, 30)};
      c = cyan[v];
    }
    bg_cache[star_y[i]][star_x[i]] = c;
    if (v == 4 && i % 3 == 0) {  /* brightest: plus sparkle */
      uint16_t h = GRB(((c >> 6) & 31) / 2, ((c >> 11) & 31) / 2, ((c >> 1) & 31) / 2);
      if (star_x[i] > 0) bg_cache[star_y[i]][star_x[i] - 1] = h;
      if (star_x[i] < SCR_W - 1) bg_cache[star_y[i]][star_x[i] + 1] = h;
      if (star_y[i] > 0) bg_cache[star_y[i] - 1][star_x[i]] = h;
      if (star_y[i] < SCR_H - 1) bg_cache[star_y[i] + 1][star_x[i]] = h;
    }
  }
  /* Earth tile (105x105 at 2x = 210x210, fully inside the screen) */
  const int OX = EARTH_CX - 104, OY = EARTH_CY - 104;
  for (int ty = 0; ty < 105; ty++) {
    int sy = OY + ty * 2;
    for (int tx = 0; tx < 105; tx++) {
      uint16_t c = EARTH_TILE[ty * 105 + tx];
      int sx = OX + tx * 2;
      bg_cache[sy][sx] = c;
      bg_cache[sy][sx + 1] = c;
      bg_cache[sy + 1][sx] = c;
      bg_cache[sy + 1][sx + 1] = c;
    }
  }
}

/* restore background over a rect: copy from the RAM cache */
static void restore_bg(int x1, int y1, int x2, int y2) {
  blit_bg_rect(x1, y1, x2, y2);
}
static void draw_background() { blit_bg_rect(0, 0, SCR_W - 1, SCR_H - 1); }
static uint16_t shade(uint16_t c, int d) {
  int r = ((c >> 6) & 31) + d, g = ((c >> 11) & 31) + d, b = ((c >> 1) & 31) + d;
  if (r < 0) r = 0; if (r > 31) r = 31;
  if (g < 0) g = 0; if (g > 31) g = 31;
  if (b < 0) b = 0; if (b > 31) b = 31;
  return GRB(r, g, b);
}

class Fm {
public:
  bool muted = false;

  void init() {
    _iocs_opmset(0x14, 0x00);   /* timers off */
    _iocs_opmset(0x0f, 0x00);   /* noise off */
    _iocs_opmset(0x18, 0x00);   /* LFO freq */
    _iocs_opmset(0x19, 0x00);   /* AM depth 0 */
    _iocs_opmset(0x19, 0x80);   /* PM depth 0 */
    _iocs_opmset(0x1b, 0x00);   /* LFO waveform */
    /* ch0 lead, ch1 bass, ch2 SE blip, ch3 SE low */
    static const int lead_mul[4] = {1, 2, 1, 3};
    static const int lead_tl[4]  = {22, 38, 46, 56};
    voice(0, lead_mul, lead_tl, 4, 5, 3);
    static const int bass_mul[4] = {1, 1, 2, 1};
    static const int bass_tl[4]  = {14, 54, 62, 70};
    voice(1, bass_mul, bass_tl, 7, 2, 7);
    static const int se_mul[4] = {1, 3, 2, 4};
    static const int se_tl[4]  = {16, 40, 48, 56};
    voice(2, se_mul, se_tl, 14, 0, 10);
    static const int low_mul[4] = {1, 1, 1, 2};
    static const int low_tl[4]  = {14, 46, 54, 62};
    voice(3, low_mul, low_tl, 10, 0, 8);
    all_off();
  }

  /* one FM voice on a channel, algorithm 7 (all carriers) */
  void voice(int ch, const int mul[4], const int tl[4],
             int d1r, int d1l, int rr) {
    static const int slot_ofs[4] = {0, 8, 16, 24};
    _iocs_opmset(0x20 + ch, 0xc0 | 7);       /* L+R, FB=0, CON=7 */
    _iocs_opmset(0x38 + ch, 0x00);           /* PMS/AMS off */
    for (int s = 0; s < 4; s++) {
      int base = ch + slot_ofs[s];
      _iocs_opmset(0x40 + base, mul[s]);                 /* DT1/MUL */
      _iocs_opmset(0x60 + base, tl[s]);                  /* TL */
      _iocs_opmset(0x80 + base, 0x1f);                   /* KS/AR */
      _iocs_opmset(0xa0 + base, d1r);                    /* D1R */
      _iocs_opmset(0xc0 + base, 0x00);                   /* DT2/D2R */
      _iocs_opmset(0xe0 + base, (d1l << 4) | rr);        /* D1L/RR */
    }
  }

  static int midi_to_kc(int m) {
    static const int off[12] = {0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15};
    int pc = m % 12;
    int oct = m / 12;
    return (oct - 2) * 16 + 14 + off[pc];
  }

  void note_on(int ch, int midi) {
    if (muted || midi < 24 || midi > 108) return;
    _iocs_opmset(0x28 + ch, midi_to_kc(midi));  /* KC */
    _iocs_opmset(0x30 + ch, 0);                 /* KF */
    _iocs_opmset(0x08, 0x78 | ch);              /* key on, all ops */
  }

  void note_off(int ch) { _iocs_opmset(0x08, ch); }

  void all_off() {
    for (int ch = 0; ch < 4; ch++) note_off(ch);
  }

  /* short SE blip on ch2 (or ch3 when low=true) */
  void se(int midi, bool low = false) {
    int ch = low ? 3 : 2;
    note_off(ch);
    note_on(ch, midi);
    se_timer_[low ? 1 : 0] = 6;
    se_ch_[low ? 1 : 0] = ch;
  }

  void update() {                 /* call once per frame */
    for (int i = 0; i < 2; i++) {
      if (se_timer_[i] > 0 && --se_timer_[i] == 0)
        note_off(se_ch_[i]);
    }
  }

  void set_muted(bool m) {
    muted = m;
    if (m) all_off();
  }

private:
  int se_timer_[2] = {0, 0};
  int se_ch_[2] = {2, 3};
};

static Fm g_fm;


/* ------------------------------------------------------------------ */
/* BGM: original patterns, 64 steps. Title (quiet ambient, held      */
/* tones: -2 = hold the previous note), stage (active arpeggios) and  */
/* boss (driving synthwave) variants.                                 */
/* ------------------------------------------------------------------ */
static const int8_t LEAD_A[64] = {
  72,-1,79,-1,75,-1,79,-1,74,-1,79,-1,75,-1,79,-1,
  80,-1,77,-1,75,-1,77,-1,79,-1,77,-1,75,-1,72,-1,
  79,-1,82,-1,79,-1,75,-1,79,-1,86,-1,82,-1,79,-1,
  77,-1,74,-1,70,-1,74,-1,77,-1,81,-1,77,-1,74,-1,
};
static const int8_t LEAD_T[64] = {
  72,-2,-2,-2,-2,-2,-2,-2,79,-2,-2,-2,-2,-2,-2,-2,
  77,-2,-2,-2,-2,-2,-2,-2,75,-2,-2,-2,-2,-2,-2,-2,
  79,-2,-2,-2,-2,-2,-2,-2,82,-2,-2,-2,-2,-2,-2,-2,
  77,-2,-2,-2,-2,-2,-2,-2,74,-2,-2,-2,-2,-2,-2,-2,
};
static const int8_t BASS_T[64] = {
  36,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,
  32,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,
  39,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,
  34,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,
};
static const int8_t BASS_A[64] = {
  36,36,48,36,36,48,36,43,36,36,48,36,36,48,36,43,
  32,32,44,32,32,44,32,39,32,32,44,32,32,44,32,39,
  39,39,51,39,39,51,39,46,39,39,51,39,39,51,39,46,
  34,34,46,34,34,46,34,41,34,34,46,34,34,46,34,41,
};
static const int8_t LEAD_B[64] = {
  69,-1,69,-1,76,-1,69,-1,72,-1,69,-1,76,-1,81,-1,
  77,-1,77,-1,84,-1,77,-1,81,-1,79,-1,77,-1,76,-1,
  69,-1,69,-1,76,-1,69,-1,72,-1,69,-1,76,-1,81,-1,
  79,-1,79,-1,86,-1,79,-1,83,-1,81,-1,79,-1,76,-1,
};
static const int8_t BASS_B[64] = {
  33,33,45,33,33,45,33,40,33,33,45,33,33,45,33,40,
  29,29,41,29,29,41,29,36,29,29,41,29,29,41,29,36,
  33,33,45,33,33,45,33,40,33,33,45,33,33,45,33,40,
  31,31,43,31,31,43,31,38,31,31,43,31,31,43,31,38,
};
static constexpr int BGM_STEP_FRAMES = 11;

struct JingleNote { int8_t midi; uint8_t frames; };
static const JingleNote JINGLE_POWER[] = {{72,5},{76,5},{81,5},{88,14},{-2,0}};
static const JingleNote JINGLE_START[] = {{45,8},{-1,2},{52,8},{-1,2},
  {57,8},{-1,2},{64,10},{69,10},{76,12},{-1,2},{81,16},{83,8},{88,28},{-2,0}};
static const JingleNote JINGLE_CLEAR[] = {{69,7},{76,7},{81,7},{88,20},{-2,0}};
static const JingleNote JINGLE_OVER[]  = {{64,12},{59,12},{52,12},{45,30},{-2,0}};
static const JingleNote JINGLE_WARN[]  = {{52,8},{52,8},{-2,0}};

/* ------------------------------------------------------------------ */
/* entities                                                           */
/* ------------------------------------------------------------------ */
struct Enemy { bool active=false, drawn=false, drop=false;
  int type=0, x=0, y=0, ox=0, oy=0, base_y=0, hp=1, t=0, phase=0, vx=0, vy=0, flash=0; };
struct PBullet { bool active=false, drawn=false;
  int kind=0, x=0, y=0, ox=0, oy=0, vx=0, vy=0, t=0; };
struct EBullet { bool active=false, drawn=false;
  int x=0, y=0, ox=0, oy=0, vx=0, vy=0; }; /* x,y,vx,vy are <<8 fixed point */
struct Particle { bool active=false, drawn=false;
  int x=0, y=0, ox=0, oy=0, vx=0, vy=0, life=0, maxlife=1, size=2; uint16_t color=0; int pal=11; };
struct Capsule { bool active=false, drawn=false; int x=0, y=0, ox=0, oy=0, t=0, type=0; };
struct Decor { bool active=false, drawn=false; int x=0, y=0, ox=0, oy=0, size=30, phase=0; };

static int enemy_radius(int type) {
  switch (type) { case 0: return 10; case 1: return 11; case 2: return 13;
    case 3: return 20; case 4: return 8; case 5: return 26; }
  return 10;
}
static int enemy_score(int type) {
  switch (type) { case 0: return 100; case 1: return 150; case 2: return 300;
    case 3: return 500; case 4: return 120; case 5: return 1000; }
  return 100;
}

/* diamond (crystal) shape, banded fill */
static void draw_diamond(int cx, int cy, int rx, int ry, uint16_t c_main, uint16_t c_hi, int frame) {
  (void)frame;
  for (int dy = -ry; dy <= ry; dy += 3) {
    int w = rx * (ry - (dy < 0 ? -dy : dy)) / ry;
    uint16_t col = (dy < -ry / 2) ? c_hi : (dy > ry / 3 ? shade(c_main, -8) : c_main);
    int ye = dy + 2; if (ye > ry) ye = ry;
    Gfx::fill(cx - w, cy + dy, cx + w, cy + ye, col);
  }
  Gfx::pset(cx - rx / 3, cy - ry / 3, COL_WHITE);
}

/* ------------------------------------------------------------------ */
/* hardware sprite patterns (16x16, PCG layout TL,BL,TR,BR).          */
/* Index semantics shared by all patterns: 1 body, 2 highlight/white,   */
/* 3 dark/edge, 4 core/accent, 5 flame/glow, 6 shade, 7 accent2.        */
/* Each object type picks a palette block giving it its colors.         */
/* ------------------------------------------------------------------ */
static int PAT_OX = 0, PAT_OY = 0;
static int iabs_(int v) { return v < 0 ? -v : v; }

static int ship_g(int sx, int y, bool shortflame) {
  if ((y == 7 || y == 8) && sx < (shortflame ? 2 : 4)) return 5;
  if (y >= 6 && y <= 9 && sx >= 3 && sx <= 29) {
    if (sx >= 27) return 2;
    if (sx >= 18 && sx <= 22 && y >= 7 && y <= 8) return 4;
    if (y == 6) return 2;
    if (y == 9) return 6;
    return 1;
  }
  if (y >= 2 && y <= 5) { int span = 3 + (y - 2) * 3;
    if (sx >= 15 - span && sx <= 15) return y == 2 ? 2 : 3; }
  if (y >= 10 && y <= 13) { int span = 3 + (13 - y) * 3;
    if (sx >= 15 - span && sx <= 15) return 3; }
  if ((y == 5 || y == 10) && sx >= 3 && sx <= 5) return 3;
  return 0;
}
static int idx_ship_l(int x, int y)  { return ship_g(x, y, false); }
static int idx_ship_r(int x, int y)  { return ship_g(x + 16, y, false); }
static int idx_ship_lf(int x, int y) { return ship_g(x, y, true); }
static int idx_ship_rf(int x, int y) { return ship_g(x + 16, y, true); }
static int idx_option(int x, int y) {
  int ad = iabs_(x - 8) + iabs_(y - 8);
  if (ad > 6) return 0;
  if (ad <= 2) return 2;
  if (ad <= 4) return 4;
  return 3;
}
static int idx_fan(int x, int y) {
  int dx = x - 7, dy = y - 7;
  if (y >= 1 && y <= 5 && x >= 9 && x <= 14 && x >= 4 + y) return 3;
  if (y >= 10 && y <= 14 && x >= 9 && x <= 14 && x >= 19 - y) return 3;
  if (y >= 6 && y <= 9 && x >= 2 && x <= 13) {
    if (x <= 3) return 2;
    if (dx * dx + dy * dy <= 4) return 4;
    if (y == 6) return 2;
    return 1;
  }
  if ((y == 7 || y == 8) && x >= 14) return 5;
  return 0;
}
static int idx_weaver(int x, int y) {
  int dx = x - 8, dy = y - 8, ad = iabs_(dx) + iabs_(dy);
  if (ad > 7) return 0;
  if (ad <= 3) return 4;
  if (ad >= 6) return 3;
  if (dx < 0 && dy < 0) return 2;
  return 1;
}
static int idx_orb(int x, int y) {
  int dx = x - 8, dy = y - 8, d2 = dx * dx + dy * dy;
  if ((y == 7 || y == 8) && x <= 1) return 3;
  if (d2 <= 49) {
    if (d2 <= 4) return 4;
    if (dx <= -3 && dy <= -3) return 2;
    if (d2 >= 37) return 3;
    return 1;
  }
  return 0;
}
static int idx_dart(int x, int y) {
  if (y >= 7 && y <= 8 && x >= 1 && x <= 14) return x <= 2 ? 2 : 1;
  if (y >= 5 && y <= 6 && x >= 11 && x <= 14) return 3;
  if (y >= 9 && y <= 10 && x >= 11 && x <= 14) return 3;
  return 0;
}
static int idx_capsule(int x, int y) {
  if (x >= 3 && x <= 12 && y >= 5 && y <= 10) {
    if (x == 3 || x == 12 || y == 5 || y == 10) return 2;
    if (x >= 7 && x <= 8) return 2;
    return 1;
  }
  return 0;
}
static int idx_pbullet(int x, int y) {
  if (y >= 7 && y <= 8 && x >= 2 && x <= 13) return x >= 11 ? 2 : 1;
  if ((y == 6 || y == 9) && x >= 4 && x <= 10) return 5;
  return 0;
}
static int idx_ebullet(int x, int y) {
  int dx = x - 8, dy = y - 8, d2 = dx * dx + dy * dy;
  if (d2 <= 16) return d2 <= 4 ? 2 : 4;
  return 0;
}
static int idx_missile(int x, int y) {
  if ((y == 7 || y == 8) && x <= 2) return 6;
  if (y >= 6 && y <= 9 && x >= 3 && x <= 13) return x >= 12 ? 2 : 5;
  if ((y == 4 || y == 5) && x >= 3 && x <= 5) return 3;
  if ((y == 10 || y == 11) && x >= 3 && x <= 5) return 3;
  return 0;
}
static int idx_spark(int x, int y) {
  int dx = x - 8, dy = y - 8;
  if (dx == 0 && dy == 0) return 2;
  if ((dx == 0 && dy >= -6 && dy <= 6) || (dy == 0 && dx >= -6 && dx <= 6))
    return 1;
  if (dx == dy || dx == -dy) { int a = iabs_(dx); if (a <= 4 && a > 0) return 4; }
  return 0;
}
static int idx_cry_t(int x, int y) {
  int gx = x + PAT_OX, gy = y + PAT_OY;
  int dx = gx - 16, dy = gy - 16, ad = iabs_(dx) + iabs_(dy);
  if (ad > 15) return 0;
  if (ad >= 14) return 3;
  if (ad <= 3) return 4;
  if (dx < 0 && dy < 0) return 2;
  if (dx == 0 || dy == 0) return 6;
  return 1;
}
static int idx_car_t(int x, int y) {
  int gx = x + PAT_OX, gy = y + PAT_OY;
  if ((gy == 15 || gy == 16) && gx <= 3) return 5;
  if (gx >= 4 && gx <= 44 && gy >= 10 && gy <= 21) {
    int dx = gx - 24, dy = gy - 16, d2 = dx * dx + dy * dy;
    if (d2 <= 16) return d2 <= 4 ? 2 : 4;
    if (gy == 10) return 2;
    if (gy >= 20) return 6;
    if ((gx % 9) == 0 && gy >= 13 && gy <= 18) return 7;
    return 1;
  }
  if (gx >= 36 && gx <= 43 && gy >= 4 && gy <= 9) return 1;
  if (gx >= 36 && gx <= 43 && gy >= 22 && gy <= 27) return 1;
  if (gx >= 45 && gx <= 47 && gy >= 14 && gy <= 17) return 2;
  return 0;
}
static int idx_boss_t(int x, int y) {
  int gx = x + PAT_OX, gy = y + PAT_OY;
  int dx = gx - 32, dy = gy - 32, ad = iabs_(dx) + iabs_(dy);
  if (ad > 31) return 0;
  int d2 = dx * dx + dy * dy;
  if (d2 <= 9) return 2;
  if (d2 <= 64) return 4;
  if (ad >= 29) return 3;
  if (ad >= 19 && ad <= 21) return 6;
  if (dx == 0 || dy == 0 || iabs_(dx) == iabs_(dy)) return 6;
  if (dx < 0 && dy < 0) return 2;
  return 1;
}
static int idx_shard(int x, int y) {
  int ad = iabs_(x - 8) + iabs_(y - 8);
  if (ad > 7) return 0;
  if (ad >= 6) return 3;
  if (ad <= 2) return 4;
  if (x < 8 && y < 8) return 2;
  return 1;
}
static int idx_shield_t(int x, int y) {
  int gx = x + PAT_OX, gy = y + PAT_OY;
  int dx = gx - 24, dy = gy - 16;
  int e = dx * dx * 225 + dy * dy * 529;
  if (e >= 92000 && e <= 148000)
    return (dy < 0 && (gx == 24 || gx == 6 || gx == 42)) ? 2 : 1;
  return 0;
}

typedef int (*PatFn)(int, int);
struct PatDef { PatFn fn; int ox, oy; };
static const PatDef PAT_DEFS[] = {
  {idx_ship_l,0,0},{idx_ship_r,0,0},{idx_ship_lf,0,0},{idx_ship_rf,0,0},
  {idx_option,0,0},{idx_fan,0,0},{idx_weaver,0,0},{idx_orb,0,0},
  {idx_dart,0,0},{idx_capsule,0,0},{idx_pbullet,0,0},{idx_ebullet,0,0},
  {idx_missile,0,0},{idx_spark,0,0},
  {idx_cry_t,0,0},{idx_cry_t,16,0},{idx_cry_t,0,16},{idx_cry_t,16,16},
  {idx_car_t,0,0},{idx_car_t,16,0},{idx_car_t,32,0},
  {idx_car_t,0,16},{idx_car_t,16,16},{idx_car_t,32,16},
  {idx_boss_t,0,0},{idx_boss_t,16,0},{idx_boss_t,32,0},{idx_boss_t,48,0},
  {idx_boss_t,0,16},{idx_boss_t,16,16},{idx_boss_t,32,16},{idx_boss_t,48,16},
  {idx_boss_t,0,32},{idx_boss_t,16,32},{idx_boss_t,32,32},{idx_boss_t,48,32},
  {idx_boss_t,0,48},{idx_boss_t,16,48},{idx_boss_t,32,48},{idx_boss_t,48,48},
  {idx_shard,0,0},
  {idx_shield_t,0,0},{idx_shield_t,16,0},{idx_shield_t,32,0},
  {idx_shield_t,0,16},{idx_shield_t,16,16},{idx_shield_t,32,16},
};
static int g_item_ch = 'S';
static int idx_item_letter(int x, int y) {
  if (x < 2 || x > 13 || y < 3 || y > 12) return 0;
  if (x == 2 || x == 13 || y == 3 || y == 12) return 2;
  int gx = x - 4, gy = y - 4;
  if (gx >= 0 && gx < 8 && gy >= 0 && gy < 8) {
    const uint8_t *g = FONT8[g_item_ch - 0x20];
    if (g[gy] & (0x80 >> gx)) return 4;
  }
  return 1;
}
static int idx_item_shield(int x, int y) {
  int dx = x - 8, dy = y - 8, d2 = dx * dx + dy * dy;
  if (d2 >= 30 && d2 <= 49) return 2;
  if ((dx == 0 && dy >= -4 && dy <= 4) || (dy == 0 && dx >= -4 && dx <= 4))
    return 2;
  if (d2 < 30) return 1;
  return 0;
}
static int idx_item_star(int x, int y) {
  int dx = x - 8, dy = y - 8, adx = iabs_(dx), ady = iabs_(dy);
  if (dx == 0 && ady <= 6) return 1;
  if (dy == 0 && adx <= 6) return 1;
  if (adx == ady && adx <= 4 && adx > 0) return 4;
  if (adx <= 1 && ady <= 1) return 2;
  return 0;
}

enum {
  PAT_SHIP_L = 0, PAT_SHIP_R = 1, PAT_SHIP_LF = 2, PAT_SHIP_RF = 3,
  PAT_OPTION = 4, PAT_FAN = 5, PAT_WEAVER = 6, PAT_ORB = 7, PAT_DART = 8,
  PAT_CAPSULE = 9, PAT_PBUL = 10, PAT_EBUL = 11, PAT_MISSILE = 12,
  PAT_SPARK = 13, PAT_CRY = 14, PAT_CAR = 18, PAT_BOSS = 24,
  PAT_SHARD = 40, PAT_SHIELD = 41, PAT_ITEM = 47
};

static void build_pat16(uint8_t *out, PatFn fn) {
  memset(out, 0, 128);
  int pos = 0;
  for (int q = 0; q < 4; q++) {
    int ox = (q & 2) ? 8 : 0, oy = (q & 1) ? 8 : 0;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++, pos++) {
        int idx = fn(ox + x, oy + y);
        if (!idx) continue;
        if (pos & 1) out[pos / 2] |= (uint8_t)idx;
        else out[pos / 2] |= (uint8_t)(idx << 4);
      }
  }
}

struct PalDef { int idx, block; uint16_t c; };
static const PalDef PAL_DEFS[] = {
  {1,1,GRB(10,22,30)},{2,1,GRB(26,31,31)},{3,1,GRB(6,10,26)},
  {4,1,GRB(31,12,20)},{5,1,GRB(31,18,2)},{6,1,GRB(6,14,22)},{7,1,GRB(31,26,6)},
  {1,2,GRB(28,10,6)},{2,2,GRB(31,31,31)},{3,2,GRB(20,5,9)},
  {4,2,GRB(31,24,4)},{5,2,GRB(31,16,2)},{6,2,GRB(16,6,4)},{7,2,GRB(31,20,4)},
  {1,3,GRB(20,8,28)},{2,3,GRB(29,17,31)},{3,3,GRB(10,4,16)},
  {4,3,GRB(31,24,4)},{5,3,GRB(31,8,24)},{6,3,GRB(14,6,20)},{7,3,GRB(24,12,30)},
  {1,4,GRB(8,20,22)},{2,4,GRB(19,30,30)},{3,4,GRB(3,9,12)},
  {4,4,GRB(31,8,8)},{5,4,GRB(31,20,4)},{6,4,GRB(5,14,16)},{7,4,GRB(12,26,28)},
  {1,5,GRB(6,24,29)},{2,5,GRB(31,31,31)},{3,5,GRB(3,10,18)},
  {4,5,GRB(31,10,26)},{5,5,GRB(10,20,31)},{6,5,GRB(4,16,22)},{7,5,GRB(31,26,6)},
  {1,6,GRB(17,9,29)},{2,6,GRB(31,31,31)},{3,6,GRB(8,4,18)},
  {4,6,GRB(31,24,4)},{5,6,GRB(24,12,31)},{6,6,GRB(11,6,20)},{7,6,GRB(6,28,30)},
  {1,7,GRB(29,22,5)},{2,7,GRB(31,31,31)},{3,7,GRB(16,10,2)},
  {4,7,GRB(31,8,14)},{5,7,GRB(31,28,10)},{6,7,GRB(20,14,3)},{7,7,GRB(8,26,30)},
  {1,8,GRB(14,12,24)},{2,8,GRB(23,21,31)},{3,8,GRB(7,6,14)},
  {4,8,GRB(6,28,30)},{5,8,GRB(31,18,2)},{6,8,GRB(9,8,18)},{7,8,GRB(31,10,10)},
  {1,9,GRB(8,22,28)},{2,9,GRB(28,31,31)},{3,9,GRB(4,10,16)},
  {4,9,GRB(31,26,6)},{6,9,GRB(6,16,20)},
  {1,10,GRB(24,12,26)},{2,10,GRB(31,31,31)},{3,10,GRB(12,6,14)},
  {4,10,GRB(31,10,10)},{6,10,GRB(16,8,18)},
  {1,11,GRB(31,30,12)},{2,11,GRB(31,31,31)},{3,11,GRB(20,8,8)},
  {4,11,GRB(31,9,15)},{5,11,GRB(23,23,27)},{6,11,GRB(31,17,2)},{7,11,GRB(31,24,4)},
  {1,12,GRB(31,10,10)},{2,12,GRB(31,31,31)},{3,12,GRB(6,26,28)},
  {4,12,GRB(31,10,26)},{5,12,GRB(31,26,6)},{6,12,GRB(31,17,2)},{7,12,GRB(10,30,10)},
  {1,13,GRB(31,31,31)},{2,13,GRB(31,31,31)},
  {3,13,GRB(31,31,31)},{4,13,GRB(31,31,31)},{5,13,GRB(31,31,31)},
  {6,13,GRB(31,31,31)},{7,13,GRB(31,31,31)},
  {1,14,GRB(9,27,30)},{2,14,GRB(31,31,31)},
  {1,15,GRB(10,26,12)},{2,15,GRB(31,31,31)},{3,15,GRB(5,14,7)},
  {4,15,GRB(31,14,26)},{6,15,GRB(7,18,9)},
};

/* ------------------------------------------------------------------ */
enum class State { Title, Play, GameOver, Clear };
enum class Phase { Normal, Warn, Boss, BossDying };

struct Input {
  int dx = 0, dy = 0;
  bool shot = false, power_edge = false, pause_edge = false,
       mute_edge = false, esc = false, launch = false;
};

class Game {
public:
  void run() {
    super_ssp_ = _iocs_b_super(0);
    prev_mode_ = _iocs_crtmod(CRT_MODE);
    _iocs_g_clr_on();
    set_60hz();
    _iocs_b_clr_al();
    _iocs_b_curoff();
    Gfx::text_center(228, 1, GRB(18, 20, 22), "LOADING...");
    stars_init();
    build_bg_cache();
    g_fm.init();
    sprites_init();
    frame_sync_init();
    to_title();
    for (;;) {
      Input in = poll_input();
#ifdef STG_TEST
      int mt0 = _iocs_ontime().sec;
#endif
      bool keep = update(in);
#ifdef STG_TEST
      if (state_ == State::Play && !paused_) {
        int mdt = _iocs_ontime().sec - mt0;
        if (mdt < 0) mdt = 0;
        bench_cost_sum_ += mdt; bench_cost_n_++;
        if (mdt > bench_cost_max_) bench_cost_max_ = mdt;
        if (bench_cost_n_ >= 240) {
          cost_avg_ms_ = (int)(bench_cost_sum_ * 10 / bench_cost_n_);
          cost_max_ms_ = bench_cost_max_ * 10;
          bench_cost_sum_ = 0; bench_cost_n_ = 0; bench_cost_max_ = 0;
        }
      }
#endif
      g_fm.update();
      if (!keep) break;
      frame_sync();
    }
    sprites_set_visible(false);
    g_fm.all_off();
    _iocs_b_clr_al();
    _iocs_crtmod(prev_mode_);
    if (super_ssp_ >= 0) _iocs_b_super(super_ssp_);
  }

private:
  State state_ = State::Title;
  Phase phase_ = Phase::Normal;
  bool paused_ = false;
  int prev_mode_ = 0;
  int frame_ = 0;
  int score_ = 0, hi_ = 0, lives_ = 3;
  bool hud_dirty_ = true, gauge_dirty_ = true;

  /* player */
  int px_ = 70, py_ = 240, pox_ = 70, poy_ = 240;
  bool player_drawn_ = false, player_dead_ = false;
  int respawn_t_ = 0, invincible_t_ = 0;
  int speed_level_ = 0;
  bool has_missile_ = false, has_double_ = false, has_laser_ = false;
  int options_ = 0, shield_hp_ = 0;
  int item_rot_ = 0; /* next power-up type to offer */
  int shot_cd_ = 0, missile_cd_ = 0, option_cd_ = 0;

  /* flight history for options + bending laser (1024 frames ring) */
  static constexpr int HIST_N = 1024;
  int hist_x_[HIST_N], hist_y_[HIST_N], hist_head_ = 0;

  /* laser */
  bool laser_active_ = false;
  int laser_len_ = 0;
  static constexpr int LASER_SEG = 28, LASER_MAXSEG = 13;
  int laser_x_[LASER_MAXSEG + 1], laser_y_[LASER_MAXSEG + 1], laser_n_ = 0;
  int prev_laser_x_[LASER_MAXSEG + 1], prev_laser_y_[LASER_MAXSEG + 1], prev_laser_n_ = 0;
  /* beam redraw signature: if the chain, width and color phase are
     unchanged, the pixels already on screen are still correct and
     both the erase and the draw can be skipped entirely. */
  int sig_x_[LASER_MAXSEG + 1], sig_y_[LASER_MAXSEG + 1];
  int sig_n_ = -1, sig_hw_ = 0, sig_phase_ = 0, beam_force_ = 0;
  int shake_amp_ = 0, shake_ox_ = 0, shake_oy_ = 0;
  int resonance_ = 0, prism_t_ = 0;

  /* stage */
  int scroll_dist_ = 0, next_spawn_dist_ = 0, spawn_idx_ = 0, next_decor_dist_ = 0;
  static constexpr int STAGE_LEN = 5600;
  int warn_t_ = 0, boss_dying_t_ = 0, clear_t_ = 0;
  bool warn_overlay_ = false;
  /* sprites */
  bool sprites_on_ = false;
  int sp_cursor_ = 0, sp_prev_count_ = 0;
  int super_ssp_ = -1;
  /* HUD delta tracking */
  int shown_score_ = -1, shown_hi_ = -1, shown_lives_ = -1, shown_res_w_ = -1;
  int prev_laser_hw_ = 3;
  int shown_prism_ = -1, shown_boss_w_ = -1, shown_boss_sh_ = -1;
  bool shown_eq_[6] = {false, false, false, false, false, false};
  int shown_shield_ = -1, shown_speed_ = -1, shown_options_ = -1;
  int warn_blink_ = -1;
  uint32_t rng_ = 0x2545F491u;
  int rand_mod(int n) { rng_ = rng_ * 1103515245u + 12345u; return (int)((rng_ >> 9) % (uint32_t)n); }

  Enemy enemies_[20];
  PBullet pbullets_[40];
  EBullet ebullets_[72];
  Particle particles_[48];
  Capsule capsules_[6];
  Decor decors_[4];

  /* boss: Abyss Prism Leviathan */
  bool boss_drawn_ = false;
  int boss_t_ = 0, boss_x_ = 400, boss_y_ = 240, boss_ox_ = 400, boss_oy_ = 240;
  int boss_core_hp_ = 0, boss_flash_ = 0;
  int shard_hp_[4] = {0, 0, 0, 0}, shard_flash_[4] = {0, 0, 0, 0};
  int shard_x_[4] = {0, 0, 0, 0}, shard_y_[4] = {0, 0, 0, 0};
  int shard_ox_[4] = {0, 0, 0, 0}, shard_oy_[4] = {0, 0, 0, 0};
  bool shard_drawn_[4] = {false, false, false, false};
  int boss_atk1_ = 0, boss_atk2_ = 0, boss_atk3_ = 0;
  bool boss_shielded() const {
    for (int i = 0; i < 4; i++) if (shard_hp_[i] > 0) return true;
    return false;
  }

  /* input state */
  int dx_dir_ = 0, dx_t_ = 0, dy_dir_ = 0, dy_t_ = 0, shot_t_ = 0;
  bool joy_power_prev_ = false;

  /* sound state */
  bool bgm_on_ = false; int music_ = 2;  /* 0 stage / 1 boss / 2 title */
  int bgm_step_ = 0, bgm_timer_ = 0;
  const JingleNote *jingle_ = nullptr;
  int jingle_idx_ = 0, jingle_timer_ = 0;
  bool press_visible_ = true;
  int title_cycle_ = 0;
  bool demo_mode_ = false;
  int title_idle_ = 0, demo_t_ = 0;
  int bench_frames_ = 0, bench_start_ = 0, fps_val_ = 0;
  long bench_cost_sum_ = 0; int bench_cost_n_ = 0, bench_cost_max_ = 0, cost_avg_ms_ = 0, cost_max_ms_ = 0;

  /* timing (same as breakout) */
  bool vsync_ok_ = true;
  int sync_next_ = 0, sync_acc_ = 0;
  void frame_sync_init() { sync_next_ = _iocs_ontime().sec; }
  void frame_sync() {
    if (vsync_ok_) {
      if (wait_vdisp_edge() == 0) return;
      vsync_ok_ = false; sync_next_ = _iocs_ontime().sec; sync_acc_ = 0;
    }
    sync_acc_ += 5; sync_next_ += sync_acc_ / 3; sync_acc_ %= 3;
    int now = _iocs_ontime().sec;
    if (now - sync_next_ > 100 || sync_next_ - now > 100000) sync_next_ = now;
    while (_iocs_ontime().sec < sync_next_) { }
  }

  /* ---------------- input ---------------- */
  Input poll_input() {
    Input in;
    while (_iocs_b_keysns()) {
      int code = _iocs_b_keyinp();
      int scan = (code >> 8) & 0xff;
      int k = code & 0xff;
      if (k >= 'A' && k <= 'Z') k += 'a' - 'A';
      if (k == 0x1b) { in.esc = true; continue; }
      if (k == ' ') { shot_t_ = 14; in.launch = true; continue; }
      if (scan == 0x3b) { dx_dir_ = -1; dx_t_ = 14; continue; }
      if (scan == 0x3d) { dx_dir_ = +1; dx_t_ = 14; continue; }
      if (scan == 0x3c) { dy_dir_ = -1; dy_t_ = 14; continue; }
      if (scan == 0x3e) { dy_dir_ = +1; dy_t_ = 14; continue; }
      switch (k) {
        case 'z': shot_t_ = 14; in.launch = true; break;
        case 'x': in.power_edge = true; break;
        case 'a': dx_dir_ = -1; dx_t_ = 14; break;
        case 'd': dx_dir_ = +1; dx_t_ = 14; break;
        case 'w': dy_dir_ = -1; dy_t_ = 14; break;
        case 's': dy_dir_ = +1; dy_t_ = 14; break;
        case 'p': in.pause_edge = true; break;
        case 'm': in.mute_edge = true; break;
        default: break;
      }
    }
    /* held keys via BITSNS group 7 (cursor cluster): accept only when
       1-2 direction bits are set, so an idle/inverted reading is ignored
       and the KEYINP latch above remains the fallback. */
    int kb7 = _iocs_bitsns(7);
    int cnt = 0;
    if (kb7 & 0x08) cnt++; if (kb7 & 0x10) cnt++;
    if (kb7 & 0x20) cnt++; if (kb7 & 0x40) cnt++;
    if (cnt == 1 || cnt == 2) {
      in.dx = ((kb7 & 0x20) ? 1 : 0) - ((kb7 & 0x08) ? 1 : 0);
      in.dy = ((kb7 & 0x40) ? 1 : 0) - ((kb7 & 0x10) ? 1 : 0);
    }
    if (in.dx == 0 && dx_t_ > 0) in.dx = dx_dir_;
    if (in.dy == 0 && dy_t_ > 0) in.dy = dy_dir_;
    if (dx_t_ > 0) dx_t_--;
    if (dy_t_ > 0) dy_t_--;
    if (shot_t_ > 0) { in.shot = true; shot_t_--; }

    int joy = _iocs_joyget(0) & _iocs_joyget(1);
    if (!(joy & 0x01)) in.dy = -1;
    if (!(joy & 0x02)) in.dy = +1;
    if (!(joy & 0x04)) in.dx = -1;
    if (!(joy & 0x08)) in.dx = +1;
    /* triggers are bits 4 (A) and 5 (B), active low: shot when
       EITHER bit is clear (the old !(joy & 0x30) required BOTH). */
    bool jshot = (joy & 0x30) != 0x30;
    if (jshot) { in.shot = true; in.launch = true; }
    bool jpower = !(joy & 0x40);
    if (jpower && !joy_power_prev_) in.power_edge = true;
    joy_power_prev_ = jpower;
    return in;
  }

  /* ---------------- sound sequencing ---------------- */
  void start_jingle(const JingleNote *j) {
    jingle_ = j; jingle_idx_ = 0; jingle_timer_ = 0;
    g_fm.note_off(0); g_fm.note_off(1);
  }
  bool jingle_active() const { return jingle_ != nullptr; }
  void sound_update() {
    if (jingle_active()) {
      if (jingle_timer_ > 0) { jingle_timer_--; return; }
      const JingleNote &n = jingle_[jingle_idx_];
      if (n.midi == -2) { jingle_ = nullptr; g_fm.note_off(0); g_fm.note_off(1); return; }
      g_fm.note_off(0); g_fm.note_off(1);
      g_fm.note_on(0, n.midi);
      if (n.midi >= 40) g_fm.note_on(1, n.midi - 12);
      jingle_timer_ = n.frames; jingle_idx_++;
      return;
    }
    if (!bgm_on_ || paused_) return;
    int frames = (music_ == 2) ? 26 : (music_ == 0) ? 10 : BGM_STEP_FRAMES;
    if (++bgm_timer_ < frames) return;
    bgm_timer_ = 0;
    int step = bgm_step_; bgm_step_ = (bgm_step_ + 1) & 63;
    int lead = (music_ == 1) ? LEAD_B[step] : (music_ == 2) ? LEAD_T[step] : LEAD_A[step];
    if (lead != -2) {  /* -2: hold the ringing note */
      g_fm.note_off(0);
      if (lead >= 0) g_fm.note_on(0, lead);
    }
    int bass = (music_ == 1) ? BASS_B[step] : (music_ == 2) ? BASS_T[step] : BASS_A[step];
    if (bass != -2) {
      g_fm.note_off(1);
      if (bass >= 0) g_fm.note_on(1, bass);
    }
  }

  /* ---------------- HUD / gauge ---------------- */
  void draw_hud() {
    restore_bg(0, 0, SCR_W - 1, PLAY_Y0 - 1);
    Gfx::text(8, 6, 2, COL_WHITE, "SCORE");
    Gfx::number(102, 6, 2, GRB(30, 30, 10), score_, 6);
    Gfx::text(218, 6, 2, COL_WHITE, "HI");
    Gfx::number(258, 6, 2, GRB(30, 30, 10), hi_, 6);
    Gfx::text(374, 6, 2, COL_WHITE, "LIFE");
    for (int i = 0; i < lives_ && i < 4; i++)
      Gfx::fill(450 + i * 14, 10, 460 + i * 14, 17, GRB(10, 24, 30));
    Gfx::text(10, 30, 1, GRB(16, 22, 26), g_fm.muted ? "SOUND OFF" : "SOUND ON");
    Gfx::text(200, 30, 1, GRB(20, 24, 28), "RESONANCE");
    draw_resonance_bar();
    shown_score_ = score_; shown_hi_ = hi_; shown_lives_ = lives_;
    shown_res_w_ = prism_t_ > 0 ? 180 : resonance_ * 180 / 1000;
    shown_prism_ = prism_t_ > 0 ? 1 : 0;
    shown_boss_w_ = -1; shown_boss_sh_ = -1;
    hud_dirty_ = false;
  }
  void draw_resonance_bar() {
    Gfx::fill(292, 29, 472, 38, GRB(2, 2, 6));
    int w = prism_t_ > 0 ? 180 : resonance_ * 180 / 1000;
    for (int i = 0; i < w; i += 6)
      Gfx::fill(292 + i, 29, 292 + i + 5, 38, RAINBOW[(i / 6 + (frame_ >> 2)) % 7]);
    if (prism_t_ > 0) Gfx::text(292, 41, 1, GRB(31, 28, 6), "PRISM BURST!");
  }
  bool slot_equipped(int slot) const {
    switch (slot) {
      case 1: return speed_level_ > 0;
      case 2: return has_missile_;
      case 3: return has_double_;
      case 4: return has_laser_;
      case 5: return options_ > 0;
      case 6: return shield_hp_ > 0;
    }
    return false;
  }
  void draw_gauge_box(int i) {
    static const char *names[6] = {"THRUST", "COMET", "DUAL", "PRISM", "ECHO", "HALO"};
    int x = 8 + i * 83, y = 444;
    bool owned = slot_equipped(i + 1);
    uint16_t border = owned ? GRB(8, 26, 12) : GRB(4, 18, 22);
    Gfx::fill(x, y, x + 76, y + 26, GRB(2, 4, 12));
    Gfx::line(x, y, x + 76, y, border);
    Gfx::line(x, y + 26, x + 76, y + 26, border);
    Gfx::line(x, y, x, y + 26, border);
    Gfx::line(x + 76, y, x + 76, y + 26, border);
    Gfx::text(x + 6, y + 5, 1, owned ? GRB(26, 31, 26) : GRB(14, 20, 24), names[i]);
    if (slot_equipped(i + 1))
      Gfx::fill(x + 6, y + 18, x + 14, y + 22, GRB(8, 30, 12));
    if (i == 5 && shield_hp_ > 0) Gfx::number(x + 62, y + 5, 1, GRB(20, 30, 30), shield_hp_, 1);
    if (i == 0 && speed_level_ > 0) Gfx::number(x + 62, y + 5, 1, GRB(20, 30, 30), speed_level_, 1);
    if (i == 4 && options_ > 0) Gfx::number(x + 62, y + 5, 1, GRB(20, 30, 30), options_, 1);
  }
  void draw_gauge() {
    Gfx::fill(0, 436, SCR_W - 1, SCR_H - 1, GRB(1, 2, 7));
    Gfx::line(0, 436, SCR_W - 1, 436, GRB(4, 22, 26));
    for (int i = 0; i < 6; i++) draw_gauge_box(i);
    for (int i = 0; i < 6; i++) shown_eq_[i] = slot_equipped(i + 1);
    shown_shield_ = shield_hp_; shown_speed_ = speed_level_;
    shown_options_ = options_;
    gauge_dirty_ = false;
  }
  /* per-frame cheap updates: only the pixels that actually changed */
  void hud_delta() {
    if (score_ != shown_score_) {
      restore_bg(98, 4, 212, 28);
      Gfx::number(102, 6, 2, GRB(30, 30, 10), score_, 6);
      shown_score_ = score_;
    }
    if (hi_ != shown_hi_) {
      restore_bg(254, 4, 368, 28);
      Gfx::number(258, 6, 2, GRB(30, 30, 10), hi_, 6);
      shown_hi_ = hi_;
    }
    if (lives_ != shown_lives_) {
      restore_bg(446, 4, 508, 28);
      for (int i = 0; i < lives_ && i < 4; i++)
        Gfx::fill(450 + i * 14, 10, 460 + i * 14, 17, GRB(10, 24, 30));
      shown_lives_ = lives_;
    }
    int w = prism_t_ > 0 ? 180 : resonance_ * 180 / 1000;
    if (w != shown_res_w_) {
      if (shown_res_w_ < 0) draw_resonance_bar();
      else if (w > shown_res_w_) {
        for (int x = 292 + shown_res_w_; x < 292 + w; x++)
          Gfx::fill(x, 29, x, 38, RAINBOW[((x - 292) / 6 + (frame_ >> 2)) % 7]);
      } else restore_bg(292 + w, 29, 291 + shown_res_w_, 38);
      shown_res_w_ = w;
    }
    int pr = prism_t_ > 0 ? 1 : 0;
    if (pr != shown_prism_) {
      if (pr) Gfx::text(292, 41, 1, GRB(31, 28, 6), "PRISM BURST!");
      else { restore_bg(292, 40, 404, 51); beam_force_ = 2; }
      shown_prism_ = pr;
    }
    if (phase_ == Phase::Boss) {
      int sh = boss_shielded() ? 1 : 0;
      int bw = boss_core_hp_ > 0 ? boss_core_hp_ * 208 / 140 : 0;
      uint16_t bc = sh ? GRB(16, 22, 28) : GRB(31, 10, 12);
      if (shown_boss_w_ < 0 || sh != shown_boss_sh_) {
        restore_bg(148, 40, 364, 51);
        Gfx::fill(150, 42, 362, 49, GRB(3, 2, 6));
        if (bw > 0) Gfx::fill(152, 44, 151 + bw, 47, bc);
      } else if (bw != shown_boss_w_) {
        if (bw > shown_boss_w_) Gfx::fill(152 + shown_boss_w_, 44, 151 + bw, 47, bc);
        else restore_bg(152 + bw, 44, 151 + shown_boss_w_, 47);
      }
      shown_boss_w_ = bw; shown_boss_sh_ = sh;
    } else if (shown_boss_w_ >= 0) {
      restore_bg(148, 40, 364, 51);
      shown_boss_w_ = -1; shown_boss_sh_ = -1;
    }
  }
  void gauge_delta() {
    for (int i = 0; i < 6; i++) {
      bool eq = slot_equipped(i + 1);
      if (eq != shown_eq_[i]) { draw_gauge_box(i); shown_eq_[i] = eq; }
    }
    if (shield_hp_ != shown_shield_) { draw_gauge_box(5); shown_shield_ = shield_hp_; shown_eq_[5] = slot_equipped(6); }
    if (speed_level_ != shown_speed_) { draw_gauge_box(0); shown_speed_ = speed_level_; shown_eq_[0] = slot_equipped(1); }
    if (options_ != shown_options_) { draw_gauge_box(4); shown_options_ = options_; shown_eq_[4] = slot_equipped(5); }
  }
  void draw_panel(const char *title, uint16_t tcol, const char *l2, const char *l3) {
    Gfx::fill(76, 190, 436, 292, GRB(2, 3, 10));
    Gfx::fill(76, 190, 436, 193, GRB(4, 26, 30));
    Gfx::fill(76, 289, 436, 292, GRB(4, 26, 30));
    Gfx::fill(76, 190, 79, 292, GRB(4, 26, 30));
    Gfx::fill(433, 190, 436, 292, GRB(4, 26, 30));
    Gfx::text_center(208, 3, tcol, title);
    if (l2) Gfx::text_center(248, 2, COL_WHITE, l2);
    if (l3) Gfx::text_center(274, 1, GRB(14, 20, 24), l3);
  }

  /* ---------------- title ---------------- */
  void draw_title_logo() {
    restore_bg(40, 62, 472, 210);
    Gfx::text(83 + 3, 76 + 3, 7, GRB(6, 3, 14), "LUMINA");
    const char *t1 = "LUMINA";
    for (int i = 0; t1[i]; i++) {
      char one[2] = {t1[i], 0};
      Gfx::text(83 + i * 58, 76, 7, RAINBOW[(i + title_cycle_) % 7], one);
    }
    Gfx::text(54 + 3, 140 + 3, 7, GRB(6, 3, 14), "SERPENT");
    const char *t2 = "SERPENT";
    for (int i = 0; t2[i]; i++) {
      char one[2] = {t2[i], 0};
      Gfx::text(54 + i * 58, 140, 7, RAINBOW[(i + 3 + title_cycle_) % 7], one);
    }
    /* prism diamond logo mark */
    draw_diamond(256, 232, 20, 26, RAINBOW[title_cycle_ % 7], COL_WHITE, frame_);
  }
  void to_title() {
    demo_mode_ = false; title_idle_ = 0;
    sprites_set_visible(false);
    state_ = State::Title; phase_ = Phase::Normal; paused_ = false;
    bgm_on_ = true; music_ = 2; bgm_step_ = 0; bgm_timer_ = 0;
    title_cycle_ = 0;
    draw_background();
    draw_title_logo();
    Gfx::text_center(272, 2, GRB(10, 24, 28), "THE LIGHT SERPENT AWAKENS");
    Gfx::text_center(306, 1, GRB(18, 24, 28), "BEND THE LASER - FEED THE RESONANCE");
    Gfx::text_center(322, 1, GRB(18, 24, 28), "BREAK THE PRISM CRYSTALS - REACH PRISM BURST");
    press_visible_ = true;
    Gfx::text_center(356, 2, COL_WHITE, "PRESS SPACE OR PAD BTN");
    Gfx::text_center(404, 1, GRB(16, 22, 26), "MOVE : CURSOR / WASD / PAD");
    Gfx::text_center(422, 1, GRB(16, 22, 26), "SHOT : Z / SPACE    FLY INTO ITEMS TO POWER UP");
    Gfx::text_center(440, 1, GRB(16, 22, 26), "P : PAUSE   M : SOUND   ESC : TITLE/EXIT");
  }
  void draw_press(bool visible) {
    if (!visible) { restore_bg(56, 352, 456, 374); return; }
    Gfx::text_center(356, 2, COL_WHITE, "PRESS SPACE OR PAD BTN");
  }

  /* ---------------- spawning ---------------- */
  void add_score(int n) { score_ += n; if (score_ > hi_) hi_ = score_; }
  Enemy *alloc_enemy() {
    for (int i = 0; i < 20; i++) if (!enemies_[i].active) return &enemies_[i];
    return nullptr;
  }
  void spawn_enemy(int type, int x, int y, bool drop, int phase) {
    Enemy *e = alloc_enemy(); if (!e) return;
    e->active = true; e->drawn = false; e->drop = drop;
    e->type = type; e->x = x; e->y = y; e->ox = x; e->oy = y; e->base_y = y;
    e->t = 0; e->phase = phase; e->flash = 0; e->vx = 0; e->vy = 0;
    switch (type) {
      case 0: e->hp = 1; break;
      case 1: e->hp = 2; break;
      case 2: e->hp = 6; break;
      case 3: e->hp = 14; break;
      case 4: e->hp = 1; e->vy = (y < py_) ? 1 : -1; break;
      case 5: e->hp = 36; break;
    }
  }
  void spawn_pb(int x, int y, int kind) {
    for (int i = 0; i < 40; i++) if (!pbullets_[i].active) {
      PBullet &b = pbullets_[i];
      b.active = true; b.drawn = false; b.kind = kind;
      b.x = x; b.y = y; b.ox = x; b.oy = y; b.t = 0;
      if (kind == 0) { b.vx = 9; b.vy = 0; }
      else if (kind == 1) { b.vx = 7; b.vy = -6; }
      else { b.vx = 6; b.vy = 3; }
      return;
    }
  }
  void spawn_eb_fp(int x, int y, int vx, int vy) {
    for (int i = 0; i < 72; i++) if (!ebullets_[i].active) {
      EBullet &b = ebullets_[i];
      b.active = true; b.drawn = false;
      b.x = x << FP; b.y = y << FP; b.ox = x; b.oy = y; b.vx = vx; b.vy = vy;
      return;
    }
  }
  void spawn_eb_aimed(int x, int y, int speed_fp) {
    int dx = px_ - x, dy = py_ - y;
    long v = (long)dx * dx + (long)dy * dy;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int dist = ax > ay ? ax : ay;
    if (dist < 1) dist = 1;
    for (int it = 0; it < 6; it++) dist = (dist + (int)(v / dist)) / 2;
    int vx = (int)((long)dx * speed_fp / dist);
    int vy = (int)((long)dy * speed_fp / dist);
    spawn_eb_fp(x, y, vx, vy);
  }
  void spawn_particle(int x, int y, int vx, int vy, int life, uint16_t color, int size, int pal = 11) {
    for (int i = 0; i < 48; i++) if (!particles_[i].active) {
      Particle &p = particles_[i];
      p.active = true; p.drawn = false; p.x = x; p.y = y; p.ox = x; p.oy = y;
      p.vx = vx; p.vy = vy; p.life = life; p.maxlife = life; p.color = color; p.size = size; p.pal = pal;
      return;
    }
  }
  void explode_at(int x, int y, int count, int big) {
    for (int i = 0; i < count; i++) {
      int a = rand_mod(64);
      int sp = (1 + rand_mod(big ? 4 : 3)) << FP;
      int vx = cos64(a) * sp >> 8, vy = sin64(a) * sp >> 8;
      uint16_t c = (i % 3 == 0) ? GRB(31, 28, 8) : (i % 3 == 1 ? GRB(31, 14, 2) : RAINBOW[rand_mod(7)]);
      spawn_particle(x, y, vx, vy, 18 + rand_mod(22), c, big ? 3 : 2,
                     (i % 3 == 0) ? 11 : (i % 3 == 1) ? 2 : 3);
    }
    g_fm.se(big ? 38 : 46, true);
  }
  /* ---------------- hardware sprites ---------------- */
  void sprites_init() {
    _iocs_sp_init();
    for (unsigned i = 0; i < sizeof(PAL_DEFS) / sizeof(PAL_DEFS[0]); i++)
      _iocs_spalet(PAL_DEFS[i].idx, PAL_DEFS[i].block, PAL_DEFS[i].c);
    static uint8_t pat[128];
    for (unsigned i = 0; i < sizeof(PAT_DEFS) / sizeof(PAT_DEFS[0]); i++) {
      PAT_OX = PAT_DEFS[i].ox; PAT_OY = PAT_DEFS[i].oy;
      build_pat16(pat, PAT_DEFS[i].fn);
      _iocs_sp_defcg((int)i * 4, 1, pat);
    }
    static const char item_chars[5] = {'T', 'C', 'D', 'P', 'E'};
    for (int i = 0; i < 5; i++) {
      g_item_ch = item_chars[i];
      build_pat16(pat, idx_item_letter);
      _iocs_sp_defcg((PAT_ITEM + i) * 4, 1, pat);
    }
    build_pat16(pat, idx_item_shield);
    _iocs_sp_defcg((PAT_ITEM + 5) * 4, 1, pat);
    build_pat16(pat, idx_item_star);
    _iocs_sp_defcg((PAT_ITEM + 6) * 4, 1, pat);
  }
  void sprites_set_visible(bool on) {
    if (on == sprites_on_) return;
    sprites_on_ = on;
    if (on) _iocs_sp_on(); else _iocs_sp_off();
  }
  /* SP_REGST serializes on the vertical blank (measured in the
     breakout project), so sprite registers are written directly:
     0xEB0000 + no*8 = {x+16, y+16-6, code|(pal<<8)|flip, priority}.
     The -6 compensates the sprite plane offset under the retimed
     CRTC (512x480), measured the same way. */
  void sp_put(int x, int y, int pat, int pal, bool hflip = false) {
    if (sp_cursor_ >= 128) return;
    volatile uint16_t *sp = (volatile uint16_t *)(0xEB0000UL + sp_cursor_ * 8);
    sp[0] = (uint16_t)(x + 16 + shake_ox_);
    sp[1] = (uint16_t)(y + 16 - 6 + shake_oy_);
    sp[2] = (uint16_t)(pat * 4 | (pal << 8) | (hflip ? 0x4000 : 0));
    sp[3] = 3;
    sp_cursor_++;
  }
  void sp_begin_frame() {
    sp_cursor_ = 0;
    if (shake_amp_ > 0) {
      shake_ox_ = rand_mod(2 * shake_amp_ + 1) - shake_amp_;
      shake_oy_ = rand_mod(2 * shake_amp_ + 1) - shake_amp_;
    } else { shake_ox_ = 0; shake_oy_ = 0; }
  }
  void sp_end_frame() {
    for (int i = sp_cursor_; i < sp_prev_count_; i++) {
      volatile uint16_t *sp = (volatile uint16_t *)(0xEB0000UL + i * 8);
      sp[1] = (uint16_t)(16 + 512 + 32);
    }
    sp_prev_count_ = sp_cursor_;
  }
  void sp_hide_all() {
    for (int i = 0; i < 128; i++) {
      volatile uint16_t *sp = (volatile uint16_t *)(0xEB0000UL + i * 8);
      sp[1] = (uint16_t)(16 + 512 + 32);
    }
    sp_prev_count_ = 0;
  }

  /* item types: 0 THRUST, 1 COMET, 2 DUAL, 3 PRISM, 4 ECHO,
     5 HALO, 6 BONUS. Drops rotate through the types and skip the
     ones already owned, so every item is worth picking up. */
  int choose_item_type() {
    for (int k = 0; k < 6; k++) {
      int t = (item_rot_ + k) % 6;
      bool sat = (t == 0 && speed_level_ >= 5) ||
                 (t == 1 && has_missile_) ||
                 (t == 2 && has_double_) ||
                 (t == 3 && has_laser_) ||
                 (t == 4 && options_ >= 2) ||
                 (t == 5 && shield_hp_ >= 4);
      if (!sat) { item_rot_ = (t + 1) % 6; return t; }
    }
    return 6;
  }
  void drop_item(int x, int y) {
    for (int i = 0; i < 6; i++) if (!capsules_[i].active) {
      Capsule &c = capsules_[i];
      c.active = true; c.drawn = false; c.x = x; c.y = y; c.ox = x; c.oy = y;
      c.t = 0; c.type = choose_item_type();
      return;
    }
  }
  void spawner() {
    if (scroll_dist_ < next_spawn_dist_) return;
    int pat = spawn_idx_ % 6; spawn_idx_++;
    int base = 100 + rand_mod(230);
    switch (pat) {
      case 0: for (int i = 0; i < 5; i++) spawn_enemy(0, 540 + i * 30, base + i * 8, i == 4, i * 5); next_spawn_dist_ += 300; break;
      case 1: for (int i = 0; i < 4; i++) spawn_enemy(1, 540 + i * 44, base, false, i * 9); next_spawn_dist_ += 320; break;
      case 2: for (int i = 0; i < 3; i++) spawn_enemy(4, 540 + i * 36, (i % 2 ? 90 : 390), i == 2, 0); next_spawn_dist_ += 260; break;
      case 3: spawn_enemy(2, 540, 110, false, 0); spawn_enemy(2, 580, 360, true, 20); next_spawn_dist_ += 340; break;
      case 4: spawn_enemy(3, 545, base + 40, true, 0); spawn_enemy(0, 590, base, false, 0); spawn_enemy(0, 590, base + 90, false, 10); next_spawn_dist_ += 380; break;
      case 5:
        if (scroll_dist_ > 1600) { spawn_enemy(5, 545, 240, true, 0); next_spawn_dist_ += 460; }
        else { for (int i = 0; i < 5; i++) spawn_enemy(0, 540 + i * 30, base + i * 8, i == 4, i * 5); next_spawn_dist_ += 300; }
        break;
    }
  }

  /* ---------------- power up: fly into an item ---------------- */
  void apply_item(int t) {
    bool effect = true;
    switch (t) {
      case 0: if (speed_level_ < 5) speed_level_++; else effect = false; break;
      case 1: if (!has_missile_) has_missile_ = true; else effect = false; break;
      case 2: if (!has_double_) { has_double_ = true; has_laser_ = false; laser_len_ = 0; } else effect = false; break;
      case 3: if (!has_laser_) { has_laser_ = true; has_double_ = false; } else effect = false; break;
      case 4: if (options_ < 2) options_++; else effect = false; break;
      case 5: if (shield_hp_ < 4) shield_hp_ = 4; else effect = false; break;
      case 6: add_score(950); break;
    }
    if (!effect) add_score(450);   /* already owned: convert to points */
    start_jingle(JINGLE_POWER);
    g_fm.se(88);
  }
  void reset_powers() {
    speed_level_ = 0; has_missile_ = false; has_double_ = false; has_laser_ = false;
    options_ = 0; shield_hp_ = 0; laser_len_ = 0;
  }

  /* ---------------- combat ---------------- */
  void hit_player() {
    if (phase_ == Phase::BossDying) return;   /* victory lap */
    if (demo_mode_) return;   /* attract mode never dies */
#ifdef STG_TEST
    return;
#endif
    if (player_dead_ || invincible_t_ > 0) return;
    if (shield_hp_ > 0) {
      shield_hp_--; g_fm.se(70, true);
      explode_at(px_, py_, 6, 0);
      invincible_t_ = 20;
      return;
    }
    explode_at(px_, py_, 26, 1);
    g_fm.se(30, true);
    prism_t_ = 0;
    lives_--;
    for (int i = 0; i < 72; i++) ebullets_[i].active = false;
    reset_powers();
    if (lives_ < 0) {
      state_ = State::GameOver; bgm_on_ = false;
      g_fm.note_off(0); g_fm.note_off(1);
      start_jingle(JINGLE_OVER);
    } else {
      player_dead_ = true; respawn_t_ = 80; laser_len_ = 0; laser_active_ = false;
    }
  }
  void damage_enemy(Enemy &e, int dmg) {
    e.hp -= dmg; e.flash = 3;
    if (e.hp > 0) return;
    e.active = false;
    add_score(enemy_score(e.type));
    explode_at(e.x, e.y, e.type >= 3 ? 22 : 10, e.type >= 3 ? 1 : 0);
    if (e.type == 3) { /* prism crystal: feeds resonance */
      resonance_ += 220; if (resonance_ > 1000) resonance_ = 1000;
      for (int i = 0; i < 10; i++) {
        int a = i * 6 + (frame_ & 7);
        { static const int RPAL[7] = {11, 2, 3, 12, 5, 6, 7};
        spawn_particle(e.x, e.y, cos64(a) * 3, sin64(a) * 3, 26, RAINBOW[i % 7], 2, RPAL[i % 7]); }
      }
    }
    if (e.drop) drop_item(e.x, e.y);
    else if (rand_mod(100) < 10) drop_item(e.x, e.y);
  }
  bool damage_boss_point(int x, int y, int dmg) {
    /* returns true if the point hit boss or a shard */
    for (int i = 0; i < 4; i++) {
      if (shard_hp_[i] <= 0) continue;
      int dx = x - shard_x_[i], dy = y - shard_y_[i];
      if (dx * dx + dy * dy < 16 * 16) {
        shard_hp_[i] -= dmg; shard_flash_[i] = 3;
        if (shard_hp_[i] <= 0) {
          add_score(800); explode_at(shard_x_[i], shard_y_[i], 18, 1);
          drop_item(shard_x_[i], shard_y_[i]);
        }
        return true;
      }
    }
    int dx = x - boss_x_, dy = y - boss_y_;
    if (dx * dx + dy * dy < 34 * 34) {
      if (boss_shielded()) { spawn_particle(x, y, -256, 0, 8, GRB(8, 28, 30), 2, 4); return true; }
      boss_core_hp_ -= dmg; boss_flash_ = 2;
      if (boss_core_hp_ <= 0 && phase_ == Phase::Boss) {
        phase_ = Phase::BossDying; boss_dying_t_ = 215;
        add_score(10000);
        explode_at(boss_x_, boss_y_, 30, 1);
        for (int i = 0; i < 72; i++) ebullets_[i].active = false;
      }
      return true;
    }
    return false;
  }
  int laser_y_at(int x) const {
    if (laser_n_ < 1) return -9999;
    for (int i = 0; i < laser_n_; i++) {
      int x1 = laser_x_[i], x2 = laser_x_[i + 1];
      if (x >= x1 && x <= x2) {
        if (x2 == x1) return laser_y_[i];
        return laser_y_[i] + (laser_y_[i + 1] - laser_y_[i]) * (x - x1) / (x2 - x1);
      }
    }
    if (x > laser_x_[laser_n_]) return laser_y_[laser_n_];
    return -9999;
  }

  /* ---------------- updates ---------------- */
  void push_history() {
    hist_head_ = (hist_head_ + 1) & (HIST_N - 1);
    hist_x_[hist_head_] = px_; hist_y_[hist_head_] = py_;
  }
  void hist_at(int delay, int &x, int &y) const {
    int idx = (hist_head_ - delay) & (HIST_N - 1);
    x = hist_x_[idx]; y = hist_y_[idx];
  }
  void update_player(const Input &in) {
    push_history();
    if (player_dead_) {
      if (--respawn_t_ <= 0) {
        player_dead_ = false; px_ = 70; py_ = 240; invincible_t_ = 150;
        for (int i = 0; i < HIST_N; i++) { hist_x_[i] = px_; hist_y_[i] = py_; }
      }
      return;
    }
    if (invincible_t_ > 0) invincible_t_--;
    int sp = 2 + speed_level_ / 2; /* 2,2,3,3,4,4 px per frame */
    px_ += in.dx * sp;
    py_ += in.dy * sp;
    if (px_ < 24) px_ = 24; if (px_ > 300) px_ = 300;
    if (py_ < PLAY_Y0 + 24) py_ = PLAY_Y0 + 24;
    if (py_ > PLAY_Y1 - 14) py_ = PLAY_Y1 - 14;
    if (shot_cd_ > 0) shot_cd_--;
    if (missile_cd_ > 0) missile_cd_--;
    if (option_cd_ > 0) option_cd_--;

    laser_active_ = has_laser_ && in.shot && phase_ != Phase::BossDying;
    if (laser_active_) {
      if (laser_len_ < LASER_SEG * LASER_MAXSEG) laser_len_ += 24;
    } else laser_len_ = 0;

    if (in.shot && !has_laser_) {
      if (shot_cd_ == 0) {
        spawn_pb(px_ + 16, py_, 0);
        if (has_double_) spawn_pb(px_ + 10, py_ - 6, 1);
        shot_cd_ = 7; g_fm.se(92);
      }
      if (has_missile_ && missile_cd_ == 0) {
        spawn_pb(px_ + 6, py_ + 8, 2); missile_cd_ = 22;
      }
    }
    if (laser_active_ && has_missile_ && missile_cd_ == 0) {
      spawn_pb(px_ + 6, py_ + 8, 2); missile_cd_ = 26;
    }
    /* options fire plain shots so they stay useful with the laser */
    if (in.shot && options_ > 0 && option_cd_ == 0) {
      for (int i = 0; i < options_; i++) {
        int ox, oy; hist_at(14 * (i + 1), ox, oy);
        spawn_pb(ox + 12, oy, 0);
      }
      option_cd_ = 9;
    }
  }
  void update_laser() {
    laser_n_ = 0;
    if (laser_len_ <= 0 || player_dead_) return;
    int segs = laser_len_ / LASER_SEG;
    if (segs > LASER_MAXSEG) segs = LASER_MAXSEG;
    laser_x_[0] = px_ + 16; laser_y_[0] = py_;
    int curvature = 0;
    for (int i = 1; i <= segs; i++) {
      int hx, hy; hist_at(2 + i * 3, hx, hy);
      laser_x_[i] = px_ + 16 + i * LASER_SEG;
      laser_y_[i] = hy;
      curvature += laser_y_[i] > laser_y_[i - 1] ? laser_y_[i] - laser_y_[i - 1]
                                                 : laser_y_[i - 1] - laser_y_[i];
    }
    laser_n_ = segs;
    if (prism_t_ > 0) { prism_t_--; }
    else if (laser_active_) {
      resonance_ += curvature / 5;
      if (resonance_ >= 1000) { resonance_ = 0; prism_t_ = 600; start_jingle(JINGLE_POWER); }
    }
    if (!laser_active_ && resonance_ > 0) resonance_--;
    /* damage tick */
    if (laser_active_ && (frame_ % 5) == 0) {
      int dmg = prism_t_ > 0 ? 2 : 1;
      for (int i = 0; i < 20; i++) if (enemies_[i].active) {
        Enemy &e = enemies_[i];
        int ly = laser_y_at(e.x);
        if (ly > -9000) {
          int d = e.y - ly; if (d < 0) d = -d;
          if (d < enemy_radius(e.type) + (prism_t_ > 0 ? 10 : 6)) damage_enemy(e, dmg);
        }
      }
      if (phase_ == Phase::Boss) {
        for (int s = 0; s < 4; s++) if (shard_hp_[s] > 0) {
          int ly = laser_y_at(shard_x_[s]);
          if (ly > -9000) { int d = shard_y_[s] - ly; if (d < 0) d = -d; if (d < 22) damage_boss_point(shard_x_[s], shard_y_[s], dmg); }
        }
        int ly = laser_y_at(boss_x_);
        if (ly > -9000) { int d = boss_y_ - ly; if (d < 0) d = -d; if (d < 40) damage_boss_point(boss_x_, boss_y_, dmg); }
      }
      if ((frame_ % 15) == 0) g_fm.se(prism_t_ > 0 ? 96 : 60, true);
    }
  }
  void update_enemies() {
    for (int i = 0; i < 20; i++) {
      Enemy &e = enemies_[i]; if (!e.active) continue;
      e.t++; if (e.flash > 0) e.flash--;
      switch (e.type) {
        case 0: e.x -= 3; e.y = e.base_y + (sin64((e.t >> 1) + e.phase) * 26 >> 8); break;
        case 1: e.x -= 3; e.y = e.base_y + (sin64((e.t >> 1) + e.phase) * 34 >> 8); break;
        case 2:
          if (e.x > 430) e.x -= 3;
          else if (e.t > 380) e.x -= 3;
          e.y = e.base_y + (sin64(e.t >> 2) * 8 >> 8);
          if (e.x <= 430 && (e.t % 95) == 50) spawn_eb_aimed(e.x - 14, e.y, 640);
          break;
        case 3: e.x -= 2;
          if ((e.t % 160) == 80) {
            spawn_eb_fp(e.x - 16, e.y, -700, -260);
            spawn_eb_fp(e.x - 18, e.y, -760, 0);
            spawn_eb_fp(e.x - 16, e.y, -700, 260);
          }
          break;
        case 4: e.x -= 6; e.y += e.vy; break;
        case 5:
          if (e.x > 428) e.x -= 2; else e.y = 240 + (sin64(e.t >> 1) * 90 >> 8);
          if (e.x <= 428 && (e.t % 85) == 42) {
            spawn_eb_aimed(e.x - 26, e.y, 600);
            spawn_eb_fp(e.x - 26, e.y, -560, -180);
            spawn_eb_fp(e.x - 26, e.y, -560, 180);
          }
          break;
      }
      if (e.type != 4) {
        if (e.y < 72) e.y = 72;
        if (e.y > 424) e.y = 424;
      }
      if (e.x < -40 || (e.type == 4 && (e.y < 62 || e.y > 430))) { e.active = false; continue; }
      if (!player_dead_ && invincible_t_ == 0) {
        int dx = e.x - px_, dy = e.y - py_;
        int r = enemy_radius(e.type) + 6;
        if (dx * dx + dy * dy < r * r) { damage_enemy(e, 99); hit_player(); }
      }
    }
  }
  void update_bullets() {
    for (int i = 0; i < 40; i++) {
      PBullet &b = pbullets_[i]; if (!b.active) continue;
      b.t++; b.x += b.vx; b.y += b.vy;
      if (b.kind == 2) { /* missile: dips, then levels out with a light trail */
        if (b.t == 16) b.vy = 0;
        if ((b.t & 3) == 0) spawn_particle(b.x - 6, b.y, -128, 0, 10, GRB(6, 18, 24), 2, 4);
      }
      if (b.x > 516 || b.y < 56 || b.y > 434) { b.active = false; continue; }
      bool consumed = false;
      for (int j = 0; j < 20 && !consumed; j++) if (enemies_[j].active) {
        Enemy &e = enemies_[j];
        int dx = b.x - e.x, dy = b.y - e.y, r = enemy_radius(e.type) + 4;
        if (dx * dx + dy * dy < r * r) { damage_enemy(e, b.kind == 2 ? 2 : 1); consumed = true; }
      }
      if (!consumed && phase_ == Phase::Boss) {
        if (damage_boss_point(b.x, b.y, b.kind == 2 ? 2 : 1)) consumed = true;
      }
      if (consumed) b.active = false;
    }
    for (int i = 0; i < 72; i++) {
      EBullet &b = ebullets_[i]; if (!b.active) continue;
      b.x += b.vx; b.y += b.vy;
      int bx = b.x >> FP, by = b.y >> FP;
      if (bx < -8 || bx > 520 || by < 60 || by > 430) { b.active = false; continue; }
      if (!player_dead_ && invincible_t_ == 0) {
        int dx = bx - px_, dy = by - py_;
        if (dx * dx + dy * dy < 9 * 9) {
          b.active = false;
          if (shield_hp_ > 0) { hit_player(); }
          else hit_player();
        }
      }
    }
  }
  void update_particles_capsules_decor() {
    for (int i = 0; i < 48; i++) {
      Particle &p = particles_[i]; if (!p.active) continue;
      if (--p.life <= 0) { p.active = false; continue; }
      p.x += p.vx >> 8; p.y += p.vy >> 8;
      if (p.x < -8 || p.x > 520 || p.y < 58 || p.y > 430) { p.active = false; continue; }
      p.vx = p.vx * 15 / 16; p.vy = p.vy * 15 / 16;
    }
    for (int i = 0; i < 6; i++) {
      Capsule &c = capsules_[i]; if (!c.active) continue;
      c.t++; c.x -= 2; c.y = c.oy + (sin64(c.t >> 1) * 10 >> 8);
      if (c.x < -16) { c.active = false; continue; }
      if (!player_dead_) {
        int dx = c.x - px_, dy = c.y - py_;
        if (dx * dx + dy * dy < 20 * 20) {
          c.active = false;
          apply_item(c.type);
          g_fm.se(96);
          add_score(50);
        }
      }
    }
  }
  void update_boss() {
    boss_t_++;
    if (boss_flash_ > 0) boss_flash_--;
    for (int i = 0; i < 4; i++) if (shard_flash_[i] > 0) shard_flash_[i]--;
    boss_y_ = 240 + (sin64(boss_t_ >> 2) * 104 >> 8);
    for (int i = 0; i < 4; i++) {
      if (shard_hp_[i] <= 0) continue;
      int a = (boss_t_ >> 1) + i * 16;
      shard_x_[i] = boss_x_ + (cos64(a) * 66 >> 8);
      shard_y_[i] = boss_y_ + (sin64(a) * 50 >> 8);
    }
    bool enraged = !boss_shielded() && boss_core_hp_ < 60;
    if (--boss_atk1_ <= 0) { spawn_eb_aimed(boss_x_ - 30, boss_y_, enraged ? 760 : 620); boss_atk1_ = enraged ? 42 : 58; }
    if (--boss_atk2_ <= 0) {
      for (int i = 0; i < 12; i++) {
        int a = i * 5 + (boss_t_ & 7);
        spawn_eb_fp(boss_x_, boss_y_, cos64(a) * 2, sin64(a) * 2);
      }
      boss_atk2_ = 230;
    }
    if (--boss_atk3_ <= 0) {
      for (int i = 0; i < 4; i++) if (shard_hp_[i] > 0) spawn_eb_aimed(shard_x_[i], shard_y_[i], 520);
      boss_atk3_ = 140;
    }
    if (!player_dead_ && invincible_t_ == 0) {
      if (damage_contact(boss_x_, boss_y_, 32)) hit_player();
      for (int i = 0; i < 4; i++)
        if (shard_hp_[i] > 0 && damage_contact(shard_x_[i], shard_y_[i], 14)) hit_player();
    }
  }
  bool damage_contact(int x, int y, int r) {
    int dx = x - px_, dy = y - py_;
    return dx * dx + dy * dy < (r + 6) * (r + 6);
  }

  /* ---------------- rendering ---------------- */
  void erase_rect(int cx, int cy, int rx, int ry) {
    int y1 = cy - ry, y2 = cy + ry;
    if (y1 < PLAY_Y0) y1 = PLAY_Y0;
    if (y2 > PLAY_Y1 + 5) y2 = PLAY_Y1 + 5;
    restore_bg(cx - rx, y1, cx + rx, y2);
  }
  /* beam segment as perpendicular runs along its major axis:
     one slope computation per column/row instead of a full
     Bresenham walk per offset line (draw + erase). */
  void beam_run(int x1, int y1, int x2, int y2, int halfw,
                uint16_t glow, uint16_t core, bool erase) {
    int dx = x2 - x1, dy = y2 - y1;
    if (iabs_(dx) >= iabs_(dy)) {
      int step = dx >= 0 ? 1 : -1;
      int n = iabs_(dx);
      int yinc = n ? (dy << 8) / n : 0;   /* one division per segment */
      int yacc = y1 << 8;
      int ymin = (y1 < y2 ? y1 : y2) - halfw;
      int ymax = (y1 > y2 ? y1 : y2) + halfw;
      bool fast = ymin >= 50 && ymax < 432 &&
                  x1 >= 0 && x1 < SCR_W && x2 >= 0 && x2 < SCR_W;
      for (int k = 0; k <= n; k++) {
        int x = x1 + step * k;
        int y = yacc >> 8; yacc += yinc;
        if (fast) {
          volatile uint16_t *g = &GVRAM[(y - halfw) * GV_STRIDE + x];
          const uint16_t *b = &bg_cache[y - halfw][x];
          for (int o = -halfw; o <= halfw; o++) {
            *g = erase ? *b : (o == 0 ? core : glow);
            g += GV_STRIDE; b += SCR_W;
          }
        } else {
          if (x < 0 || x >= SCR_W) continue;
          for (int o = -halfw; o <= halfw; o++) {
            int yy = y + o;
            if (yy < 50 || yy >= 432) continue;
            GVRAM[yy * GV_STRIDE + x] =
                erase ? bg_cache[yy][x] : (o == 0 ? core : glow);
          }
        }
      }
    } else {
      int step = dy >= 0 ? 1 : -1;
      int n = iabs_(dy);
      int xinc = n ? (dx << 8) / n : 0;
      int xacc = x1 << 8;
      int xmin = (x1 < x2 ? x1 : x2) - halfw;
      int xmax = (x1 > x2 ? x1 : x2) + halfw;
      bool fast = xmin >= 0 && xmax < SCR_W &&
                  y1 >= 50 && y1 < 432 && y2 >= 50 && y2 < 432;
      for (int k = 0; k <= n; k++) {
        int y = y1 + step * k;
        int x = xacc >> 8; xacc += xinc;
        if (fast) {
          volatile uint16_t *g = &GVRAM[y * GV_STRIDE + (x - halfw)];
          const uint16_t *b = &bg_cache[y][x - halfw];
          for (int o = -halfw; o <= halfw; o++) {
            *g = erase ? *b : (o == 0 ? core : glow);
            g++; b++;
          }
        } else {
          if (y < 50 || y >= 432) continue;
          for (int o = -halfw; o <= halfw; o++) {
            int xx = x + o;
            if (xx < 0 || xx >= SCR_W) continue;
            GVRAM[y * GV_STRIDE + xx] =
                erase ? bg_cache[y][xx] : (o == 0 ? core : glow);
          }
        }
      }
    }
  }

  void render_play() {
    /* ---- boss-death whiteout cinematic ---- */
    if (phase_ == Phase::BossDying && boss_dying_t_ <= 105 &&
        boss_dying_t_ >= 54) {
      if (prev_laser_n_ > 0) {   /* clear any stale beam once */
        for (int i = 0; i < prev_laser_n_; i++)
          beam_run(prev_laser_x_[i], prev_laser_y_[i],
                   prev_laser_x_[i + 1], prev_laser_y_[i + 1],
                   prev_laser_hw_, 0, 0, true);
        prev_laser_n_ = 0; sig_n_ = -1;
      }
      int t = boss_dying_t_;
      if (t >= 94) {          /* white sweep, 40px band per frame */
        int b = 105 - t;
        Gfx::fill(0, b * 40, SCR_W - 1, b * 40 + 39, COL_WHITE);
      } else if (t >= 66) {   /* hold the whiteout */
      } else {                /* wipe the background back in */
        int b = 65 - t;
        restore_bg(0, b * 40, SCR_W - 1, b * 40 + 39);
      }
      return;
    }
    /* ---- bending laser on GVRAM (run-based erase + draw) ---- */
    {
      int halfw = prism_t_ > 0 ? 3 : 2;
      int phase_col = prism_t_ > 0 ? ((frame_ >> 2) % 7) : -1;
      bool same = false;
      if (laser_n_ > 0 && !player_dead_ && beam_force_ == 0 &&
          laser_n_ == sig_n_ && halfw == sig_hw_ && phase_col == sig_phase_) {
        same = true;
        for (int i = 0; i <= laser_n_; i++)
          if (laser_x_[i] != sig_x_[i] || laser_y_[i] != sig_y_[i]) {
            same = false; break;
          }
      }
      if (laser_n_ == 0) sig_n_ = -1;
      if (prev_laser_n_ > 0 && !same) {
        for (int i = 0; i < prev_laser_n_; i++)
          beam_run(prev_laser_x_[i], prev_laser_y_[i],
                   prev_laser_x_[i + 1], prev_laser_y_[i + 1],
                   prev_laser_hw_, 0, 0, true);
        restore_bg(prev_laser_x_[prev_laser_n_] - 4,
                   prev_laser_y_[prev_laser_n_] - 4,
                   prev_laser_x_[prev_laser_n_] + 4,
                   prev_laser_y_[prev_laser_n_] + 4);
        prev_laser_n_ = 0;
      }
      if (laser_n_ > 0 && !player_dead_) {
        if (!same) {
          for (int i = 0; i < laser_n_; i++) {
            int curv = laser_y_[i + 1] - laser_y_[i];
            if (curv < 0) curv = -curv;
            uint16_t glow, core;
            if (prism_t_ > 0) { glow = RAINBOW[(i + (frame_ >> 2)) % 7]; core = COL_WHITE; }
            else if (curv < 2) { glow = GRB(4, 26, 30); core = GRB(24, 31, 31); }
            else if (curv < 6) { glow = GRB(6, 30, 14); core = COL_WHITE; }
            else if (curv < 12) { glow = GRB(28, 8, 28); core = COL_WHITE; }
            else { glow = GRB(31, 22, 2); core = COL_WHITE; }
            beam_run(laser_x_[i], laser_y_[i], laser_x_[i + 1],
                     laser_y_[i + 1], halfw, glow, core, false);
            sig_x_[i] = laser_x_[i]; sig_y_[i] = laser_y_[i];
          }
          sig_x_[laser_n_] = laser_x_[laser_n_];
          sig_y_[laser_n_] = laser_y_[laser_n_];
          sig_n_ = laser_n_; sig_hw_ = halfw; sig_phase_ = phase_col;
          Gfx::fill(laser_x_[laser_n_] - 2, laser_y_[laser_n_] - 2,
                    laser_x_[laser_n_] + 2, laser_y_[laser_n_] + 2, COL_WHITE);
        }
        for (int i = 0; i <= laser_n_; i++) {
          prev_laser_x_[i] = laser_x_[i]; prev_laser_y_[i] = laser_y_[i];
        }
        prev_laser_n_ = laser_n_;
        prev_laser_hw_ = halfw;
      }
      if (beam_force_ > 0) beam_force_--;
    }
    /* ---- WARNING overlay: redraw only on blink state change ---- */
    if (phase_ == Phase::Warn) {
      int b = ((warn_t_ / 18) & 1) == 0 ? 1 : 0;
      if (b != warn_blink_) {
        if (b) {
          Gfx::text_center(196, 5, GRB(31, 6, 6), "WARNING");
          Gfx::text_center(252, 1, GRB(31, 24, 8),
                           "ABYSS PRISM LEVIATHAN APPROACHES");
        } else restore_bg(56, 176, 456, 290);
        warn_blink_ = b;
        beam_force_ = 2;
      }
    } else if (warn_blink_ >= 0) {
      restore_bg(56, 176, 456, 290);
      warn_blink_ = -1;
      beam_force_ = 2;
    }
    /* ---- entities on hardware sprites (register writes only) ---- */
    sp_begin_frame();
    if (!player_dead_ && shield_hp_ > 0)
      for (int ty = 0; ty < 2; ty++)
        for (int tx = 0; tx < 3; tx++)
          sp_put(px_ - 24 + tx * 16, py_ - 16 + ty * 16,
                 PAT_SHIELD + ty * 3 + tx, 14);
    if (phase_ == Phase::Boss && boss_shielded())
      for (int i = 0; i < 16; i++) {
        int a = (i * 4 + (frame_ >> 2)) & 63;
        sp_put(boss_x_ + (cos64(a) * 48 >> 8) - 8,
               boss_y_ + (sin64(a) * 46 >> 8) - 8, PAT_SPARK, 14);
      }
    if (!player_dead_) {
      bool blink_off = invincible_t_ > 0 && ((frame_ >> 2) & 1);
      if (!blink_off) {
        bool fl = (frame_ >> 2) & 1;
        sp_put(px_ - 16, py_ - 8, fl ? PAT_SHIP_LF : PAT_SHIP_L, 1);
        sp_put(px_, py_ - 8, fl ? PAT_SHIP_RF : PAT_SHIP_R, 1);
      }
      for (int o = 0; o < options_; o++) {
        int ox, oy;
        hist_at(14 * (o + 1), ox, oy);
        sp_put(ox - 8, oy - 8, PAT_OPTION, 12);
      }
    }
    {
      static const int ITEM_PAL[7] = {7, 2, 3, 5, 12, 4, 11};
      for (int i = 0; i < 6; i++) if (capsules_[i].active)
        sp_put(capsules_[i].x - 8, capsules_[i].y - 8,
               PAT_ITEM + capsules_[i].type, ITEM_PAL[capsules_[i].type]);
    }
    if (phase_ == Phase::Boss ||
        (phase_ == Phase::BossDying && boss_dying_t_ > 105)) {
      static const int BPAL[3] = {9, 10, 15};
      int bpal = boss_flash_ > 0 ? 13 : BPAL[(frame_ >> 4) % 3];
      if (phase_ == Phase::BossDying && ((frame_ >> 1) & 1)) bpal = 13;
      for (int ty = 0; ty < 4; ty++)
        for (int tx = 0; tx < 4; tx++)
          sp_put(boss_x_ - 32 + tx * 16, boss_y_ - 32 + ty * 16,
                 PAT_BOSS + ty * 4 + tx, bpal);
      for (int i = 0; i < 4; i++) if (shard_hp_[i] > 0)
        sp_put(shard_x_[i] - 8, shard_y_[i] - 8, PAT_SHARD,
               shard_flash_[i] > 0 ? 13 : 10);
    }
    for (int i = 0; i < 20; i++) if (enemies_[i].active) {
      Enemy &e = enemies_[i];
      bool fl = e.flash > 0;
      switch (e.type) {
        case 0: sp_put(e.x - 8, e.y - 8, PAT_FAN, fl ? 13 : 2); break;
        case 1: sp_put(e.x - 8, e.y - 8, PAT_WEAVER, fl ? 13 : 3); break;
        case 2: sp_put(e.x - 8, e.y - 8, PAT_ORB, fl ? 13 : 4); break;
        case 3: {
          static const int CPAL[3] = {5, 6, 7};
          int cp = fl ? 13 : CPAL[e.phase % 3];
          for (int ty = 0; ty < 2; ty++)
            for (int tx = 0; tx < 2; tx++)
              sp_put(e.x - 16 + tx * 16, e.y - 16 + ty * 16,
                     PAT_CRY + ty * 2 + tx, cp);
          break;
        }
        case 4: sp_put(e.x - 8, e.y - 8, PAT_DART, fl ? 13 : 7); break;
        case 5:
          for (int ty = 0; ty < 2; ty++)
            for (int tx = 0; tx < 3; tx++)
              sp_put(e.x - 24 + tx * 16, e.y - 16 + ty * 16,
                     PAT_CAR + ty * 3 + tx, fl ? 13 : 8);
          break;
      }
    }
    for (int i = 0; i < 40; i++) if (pbullets_[i].active) {
      PBullet &b = pbullets_[i];
      sp_put(b.x - 8, b.y - 8, b.kind == 2 ? PAT_MISSILE : PAT_PBUL, 11);
    }
    for (int i = 0; i < 72; i++) if (ebullets_[i].active) {
      EBullet &b = ebullets_[i];
      sp_put((b.x >> FP) - 8, (b.y >> FP) - 8, PAT_EBUL, 11);
    }
    for (int i = 0; i < 48; i++) if (particles_[i].active)
      sp_put(particles_[i].x - 8, particles_[i].y - 8, PAT_SPARK,
             particles_[i].pal);
    sp_end_frame();
  }

  /* ---------------- flow ---------------- */
  void redraw_all() {
    draw_background(); draw_hud(); draw_gauge();
    for (int i = 0; i < 20; i++) enemies_[i].drawn = false;
    for (int i = 0; i < 40; i++) pbullets_[i].drawn = false;
    for (int i = 0; i < 72; i++) ebullets_[i].drawn = false;
    for (int i = 0; i < 48; i++) particles_[i].drawn = false;
    for (int i = 0; i < 6; i++) capsules_[i].drawn = false;
    for (int i = 0; i < 4; i++) { decors_[i].drawn = false; shard_drawn_[i] = false; }
    player_drawn_ = false; boss_drawn_ = false; prev_laser_n_ = 0;
    sig_n_ = -1;
  }
  void start_game() {
    for (int i = 0; i < 20; i++) enemies_[i].active = false;
    for (int i = 0; i < 40; i++) pbullets_[i].active = false;
    for (int i = 0; i < 72; i++) ebullets_[i].active = false;
    for (int i = 0; i < 48; i++) particles_[i].active = false;
    for (int i = 0; i < 6; i++) capsules_[i].active = false;
    for (int i = 0; i < 4; i++) decors_[i].active = false;
    score_ = 0; lives_ = 3; reset_powers(); item_rot_ = 0;
    px_ = 70; py_ = 240; player_dead_ = false; invincible_t_ = 60;
    respawn_t_ = 0; shot_cd_ = missile_cd_ = option_cd_ = 0;
    resonance_ = 0; prism_t_ = 0; laser_len_ = 0; laser_n_ = 0;
    scroll_dist_ = 0; next_spawn_dist_ = 240; spawn_idx_ = 0; next_decor_dist_ = 60;
    phase_ = Phase::Normal; music_ = 0;
    for (int i = 0; i < HIST_N; i++) { hist_x_[i] = px_; hist_y_[i] = py_; }
    hist_head_ = 0;
    state_ = State::Play; paused_ = false;
    bgm_on_ = true; bgm_step_ = 0; bgm_timer_ = 0;
#ifdef STG_TEST
    has_laser_ = true; has_missile_ = true; options_ = 1; speed_level_ = 2;
    scroll_dist_ = 4700; next_spawn_dist_ = 4800;
#endif
    gauge_dirty_ = true;
    demo_mode_ = false;
    redraw_all();
    sp_hide_all();
    sprites_set_visible(true);
    start_jingle(JINGLE_START);
  }
  void start_warn() {
    phase_ = Phase::Warn; warn_t_ = 190; music_ = 1; warn_overlay_ = true;
    for (int i = 0; i < 20; i++) if (enemies_[i].active) {
      explode_at(enemies_[i].x, enemies_[i].y, 8, 0); enemies_[i].active = false;
    }
    start_jingle(JINGLE_WARN);
  }
  void start_boss() {
    phase_ = Phase::Boss; boss_t_ = 0;
    boss_x_ = 400; boss_y_ = 240; boss_core_hp_ = 140; boss_flash_ = 0;
    for (int i = 0; i < 4; i++) { shard_hp_[i] = 22; shard_flash_[i] = 0; }
    boss_atk1_ = 70; boss_atk2_ = 150; boss_atk3_ = 100;
  }
  void toggle_pause() {
    paused_ = !paused_;
    if (paused_) {
      g_fm.note_off(0); g_fm.note_off(1);
      sprites_set_visible(false);
      draw_panel("PAUSED", GRB(30, 30, 10), "P : RESUME", "ESC : TITLE");
    } else { redraw_all(); sprites_set_visible(true); }
  }

  /* attract-mode autopilot: chase capsules/enemies, keep firing,
     and sweep vertically so the serpent laser bends (and PRISM
     BURST eventually shows itself). */
  Input demo_input() {
    Input a;
    demo_t_++;
    a.shot = true;
    int ty = 240 + (sin64((demo_t_ >> 2) & 63) * 80 >> 8);
    int tx = 110;
    bool found = false;
    for (int i = 0; i < 6 && !found; i++) if (capsules_[i].active) {
      ty = capsules_[i].y; tx = 120; found = true;
    }
    if (!found && phase_ == Phase::Boss) {
      tx = 150;
      if (boss_shielded()) {
        int bi = 0, bd = 1 << 30;
        for (int i = 0; i < 4; i++) if (shard_hp_[i] > 0) {
          int d = shard_x_[i] > px_ ? shard_x_[i] - px_ : px_ - shard_x_[i];
          if (d < bd) { bd = d; bi = i; }
        }
        ty = shard_y_[bi];
      } else ty = boss_y_;
    } else if (!found) {
      int bd = 1 << 30;
      for (int i = 0; i < 20; i++) if (enemies_[i].active) {
        int d = (enemies_[i].y > py_ ? enemies_[i].y - py_
                                    : py_ - enemies_[i].y) * 2 +
                (enemies_[i].x > px_ ? enemies_[i].x - px_
                                     : px_ - enemies_[i].x) / 4;
        if (d < bd) { bd = d; ty = enemies_[i].y; }
      }
    }
    if (has_laser_) ty += sin64((demo_t_ >> 1) & 63) * 60 >> 8;
    if (ty < 84) ty = 84;
    if (ty > 396) ty = 396;
    a.dy = py_ < ty - 6 ? 1 : py_ > ty + 6 ? -1 : 0;
    a.dx = px_ < tx - 8 ? 1 : px_ > tx + 8 ? -1 : 0;
    return a;
  }

  /* ---------------- main update ---------------- */
  bool update(Input in) {
    frame_++;
    if (demo_mode_ && state_ == State::Play) {
      if (in.dx || in.dy || in.shot || in.power_edge || in.pause_edge ||
          in.esc || in.launch) {
        demo_mode_ = false;
        to_title();
        return true;
      }
      in = demo_input();
    }
    if (in.mute_edge) g_fm.set_muted(!g_fm.muted);
    sound_update();
    switch (state_) {
      case State::Title: {
        if ((frame_ % 54) == 0) { title_cycle_++; draw_title_logo(); }
        bool vis = (frame_ % 84) < 55;
        if (vis != press_visible_) { press_visible_ = vis; draw_press(vis); }
        if (in.esc) {
          g_fm.all_off(); draw_background();
          Gfx::text_center(220, 3, GRB(10, 26, 30), "THANK YOU!");
          Gfx::text_center(280, 1, GRB(16, 22, 26), "RESET TO PLAY AGAIN");
#ifdef STANDALONE
          for (;;) { }
#else
          return false;
#endif
        }
        if (in.dx || in.dy || in.shot || in.power_edge || in.pause_edge)
          title_idle_ = 0;
        else if (++title_idle_ >= 300) {
          title_idle_ = 0;
          start_game();
          demo_mode_ = true; demo_t_ = 0;
          break;
        }
        if (in.launch) start_game();
        break;
      }
      case State::Play: {
        if (in.esc) { to_title(); break; }
        if (in.pause_edge) { toggle_pause(); break; }
        if (paused_) break;
        update_player(in);
        update_laser();
        if (phase_ == Phase::Normal) {
          scroll_dist_ += 2;
          spawner();
          if (scroll_dist_ >= STAGE_LEN) start_warn();
        } else if (phase_ == Phase::Warn) {
          if (--warn_t_ <= 0) start_boss();
        } else if (phase_ == Phase::Boss) {
          update_boss();
        } else if (phase_ == Phase::BossDying) {
          int el = 215 - boss_dying_t_;   /* frames since the kill */
          if (el < 110) {
            /* escalating barrage + screen shake, boss still on screen */
            shake_amp_ = 1 + el / 22;
            if ((frame_ % 3) == 0)
              explode_at(boss_x_ + rand_mod(80) - 40,
                         boss_y_ + rand_mod(80) - 40,
                         (el % 9 == 0) ? 14 : 8, 1);
            if (el == 109) {   /* the final blast */
              explode_at(boss_x_, boss_y_, 40, 1);
              for (int i = 0; i < 24; i++) {
                int a = i * 64 / 24;
                spawn_particle(boss_x_, boss_y_, cos64(a) * 5,
                               sin64(a) * 5, 30, COL_WHITE, 2, 13);
              }
              g_fm.se(28, true);
              shake_amp_ = 9;
            }
          } else if (el == 110) {
            shake_amp_ = 0;
            sprites_set_visible(false);
            sp_hide_all();
          }
          if (--boss_dying_t_ <= 0) {
            shake_amp_ = 0;
            sprites_set_visible(false);
            state_ = State::Clear; bgm_on_ = false;
            g_fm.note_off(0); g_fm.note_off(1);
            start_jingle(JINGLE_CLEAR);
            redraw_all();
            draw_panel("ALL CLEAR", GRB(10, 31, 14), "BONUS 10000 PTS", "SPACE : TITLE");
            draw_hud();
            break;
          }
        }
        update_enemies();
        update_bullets();
        update_particles_capsules_decor();
        if (state_ == State::GameOver) {
          sprites_set_visible(false);
          redraw_all();
          draw_panel("GAME OVER", GRB(31, 8, 8), "SPACE : RETRY", "ESC : TITLE");
          draw_hud();
          break;
        }
        render_play();
        if (hud_dirty_) draw_hud();
        if (gauge_dirty_) draw_gauge();
        hud_delta();
        gauge_delta();
        if (demo_mode_)
          Gfx::text_center(58, 1, GRB(20, 24, 28), "DEMO PLAY");
#ifdef STG_TEST
        bench_frames_++;
        if (bench_start_ == 0) bench_start_ = _iocs_ontime().sec;
        { int el = _iocs_ontime().sec - bench_start_;
          if (el >= 200) { fps_val_ = bench_frames_ * 100 / el; bench_frames_ = 0; bench_start_ = _iocs_ontime().sec; } }
        if ((frame_ & 7) == 0) {
          restore_bg(434, 25, 511, 53);
          Gfx::text(438, 28, 1, GRB(31, 31, 0), "F");
          Gfx::number(446, 28, 1, GRB(31, 31, 0), fps_val_, 2);
          Gfx::text(472, 28, 1, GRB(31, 31, 0), vsync_ok_ ? "V1" : "V0");
          Gfx::text(438, 41, 1, GRB(31, 31, 0), "M");
          Gfx::number(446, 41, 1, GRB(31, 31, 0), cost_avg_ms_, 3);
          Gfx::text(476, 41, 1, GRB(31, 31, 0), "X");
          Gfx::number(484, 41, 1, GRB(31, 31, 0), cost_max_ms_, 3);
        }
#endif
        break;
      }
      case State::GameOver: {
        if (demo_mode_) { to_title(); break; }
        if (in.esc) { to_title(); break; }
        if (in.launch && !jingle_active()) start_game();
        break;
      }
      case State::Clear: {
        if (demo_mode_) { to_title(); break; }
        if (in.esc || in.launch) to_title();
        break;
      }
    }
    return true;
  }
};

static Game g_game;
int main() { g_game.run(); return 0; }
