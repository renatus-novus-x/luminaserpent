/* earth_gen.cpp - host tool: renders the game's background bake
   (gradient + nebula + stars + procedural Earth) to bgcache.ppm.
   The Earth tile is cut from this render by gen_earth_tile.py and
   embedded into the game as earth_bg.h, so the X68000 only blits a
   pre-rendered tile instead of generating the planet at boot.     */
#include <cstdint>
#include <cstdio>
static constexpr int SCR_W = 512, SCR_H = 480;
static uint16_t bg_cache[SCR_H][SCR_W];
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
/* Space background bake: nebula tint, starfield, and a big Earth     */
/* (design board B) rendered procedurally once into bg_cache.         */
/* Integer-only and division-light: this runs before the title shows, */
/* so it must finish within a few seconds even on a 10 MHz machine.   */
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
/* 4-octave fractal, returns roughly 0..255 */
static int frac_noise(int X, int Y, int seed) {
  static const int sh[4] = {7, 6, 5, 4};
  static const int amp[4] = {128, 64, 32, 16};
  int v = 0;
  for (int o = 0; o < 4; o++)
    v += noise2s(X + seed * 977, Y + seed * 631, sh[o]) * amp[o] / 128;
  v = v * 255 / 240;
  return v < 0 ? 0 : (v > 255 ? 255 : v);
}
static int isqrt_i(int v) {
  int r = 0, bit = 1 << 16;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
    else r >>= 1;
    bit >>= 2;
  }
  return r;
}
static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static int hist_pct(const uint16_t *hist, int total, int pct) {
  int need = total * pct / 100, acc = 0;
  for (int i = 0; i < 256; i++) {
    acc += hist[i];
    if (acc >= need) return i;
  }
  return 255;
}

static void build_bg_cache() {
  lattice_init();
  /* gradient + faint nebula tint (coarse grid, bilinear) */
  static int tint_r[61][65], tint_b[61][65];
  for (int gy = 0; gy < 61; gy++)
    for (int gx = 0; gx < 65; gx++) {
      int n1 = noise2s(gx * 8 + 977 * 11, gy * 8 + 631 * 11, 8);
      int n2 = noise2s(gx * 8 + 977 * 12, gy * 8 + 631 * 12, 8);
      tint_r[gy][gx] = n1 > 170 ? (n1 - 170) / 42 : 0;   /* faint purple wash */
      tint_b[gy][gx] = n2 > 170 ? (n2 - 170) / 34 : 0;   /* faint teal wash */
    }
  for (int y = 0; y < SCR_H; y++) {
    int gy = y >> 3, fy = y & 7;
    uint16_t c = bg_color(y);
    int cr = ((c >> 6) & 31) * 8, cg = ((c >> 11) & 31) * 8, cb = ((c >> 1) & 31) * 8;
    for (int x = 0; x < SCR_W; x++) {
      int gx = x >> 3, fx = x & 7;
      int tr = (tint_r[gy][gx] * (8 - fx) * (8 - fy) + tint_r[gy][gx + 1] * fx * (8 - fy) +
                tint_r[gy + 1][gx] * (8 - fx) * fy + tint_r[gy + 1][gx + 1] * fx * fy) / 64;
      int tb = (tint_b[gy][gx] * (8 - fx) * (8 - fy) + tint_b[gy][gx + 1] * fx * (8 - fy) +
                tint_b[gy + 1][gx] * (8 - fx) * fy + tint_b[gy + 1][gx + 1] * fx * fy) / 64;
      int jit = (int)((x * 73 + y * 149) & 7) - 4;
      bg_cache[y][x] = GRB(clamp255(cr + tr * 8 + jit) >> 3, clamp255(cg + jit) >> 3,
                           clamp255(cb + tb * 8 + jit) >> 3);
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
  /* Earth: pass 1 computes the noise fields once (+ histograms for
     percentile thresholds); pass 2 shades from the stored fields. */
  const int M = EARTH_R + 16;                       /* glow margin */
  const int BW = 2 * M + 1;
  static uint8_t ef_land[209 * 209], ef_dry[209 * 209], ef_cloud[209 * 209];
  static uint16_t hist_land[256], hist_cloud[256];
  for (int i = 0; i < 256; i++) hist_land[i] = hist_cloud[i] = 0;
  int disc_px = 0;
  for (int by = 0; by < BW; by++) {
    int y = EARTH_CY - M + by;
    for (int bx = 0; bx < BW; bx++) {
      int x = EARTH_CX - M + bx;
      int idx = by * BW + bx;
      ef_land[idx] = ef_dry[idx] = ef_cloud[idx] = 0;
      if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H) continue;
      int u = (x - EARTH_CX) * 256 / EARTH_R;
      int v = (y - EARTH_CY) * 256 / EARTH_R;
      if (u * u + v * v > 65536) continue;
      int ln = frac_noise(u + 256, v + 256, 3);
      int cn = frac_noise(u + 256, 256 + v * 45 / 100, 7);
      ef_land[idx] = (uint8_t)ln;
      ef_dry[idx] = (uint8_t)frac_noise(u + 256, v + 256, 5);
      ef_cloud[idx] = (uint8_t)cn;
      hist_land[ln]++;
      hist_cloud[cn]++;
      disc_px++;
    }
  }
  const int thr_land = hist_pct(hist_land, disc_px, 62);
  const int thr_cloud = hist_pct(hist_cloud, disc_px, 68);
  for (int by = 0; by < BW; by++) {
    int y = EARTH_CY - M + by;
    if (y < 0 || y >= SCR_H) continue;
    for (int bx = 0; bx < BW; bx++) {
      int x = EARTH_CX - M + bx;
      if (x < 0 || x >= SCR_W) continue;
      int idx = by * BW + bx;
      int u = (x - EARTH_CX) * 256 / EARTH_R;
      int v = (y - EARTH_CY) * 256 / EARTH_R;
      int d2 = u * u + v * v;
      int jit = (int)((x * 73 + y * 149) & 7) - 4;
      if (d2 > 65536) {  /* atmosphere glow ring */
        int dd = isqrt_i(d2);
        if (dd > 302) continue;
        int a = (302 - dd) * 255 / 46;
        int lit = (115 * 256 - 141 * u - 90 * v) >> 8;
        if (lit < 0) lit = 0;
        if (lit > 256) lit = 256;
        a = a * (51 + lit * 204 / 256) / 256 * 115 / 255;
        uint16_t c = bg_cache[y][x];
        int r = ((c >> 6) & 31) * 8 + 64 * a / 255;
        int g = ((c >> 11) & 31) * 8 + 128 * a / 255;
        int b = ((c >> 1) & 31) * 8 + 250 * a / 255;
        bg_cache[y][x] = GRB(clamp255(r + jit) >> 3, clamp255(g + jit) >> 3, clamp255(b + jit) >> 3);
        continue;
      }
      int z = isqrt_i(65536 - d2);
      int dif = (-133 * u - 148 * v + 161 * z) >> 8;
      if (dif < 0) dif = 0;
      if (dif > 256) dif = 256;
      int light = 23 + dif * 232 / 256;             /* 23..255 */
      int ln = ef_land[idx];
      int dry = ef_dry[idx];
      int land = ln > thr_land;
      int r, g, b;
      if (land) {
        int dm = dry * 2 / 5;
        r = 30 + (146 - 30) * dm / 255;
        g = 110 + (122 - 110) * dm / 255;
        b = 44 + (72 - 44) * dm / 255;
      } else {
        int sh = (ln - (thr_land - 13)) * 255 / 13;
        if (sh < 0) sh = 0;
        if (sh > 255) sh = 255;
        r = 12 + (28 - 12) * sh / 255;
        g = 56 + (110 - 56) * sh / 255;
        b = 132 + (185 - 132) * sh / 255;
      }
      if (v + ((ln - 128) * 64 >> 8) < -205) { r = 226; g = 238; b = 248; }  /* polar ice */
      r = r * light / 255; g = g * light / 255; b = b * light / 255;
      if (dif > 225 && !land) { r += 24; g += 28; b += 24; }                /* ocean sheen */
      int rim = (256 - z) - 128;
      if (rim > 0) {
        int rv = rim * (dif * 17 / 10 > 256 ? 256 : dif * 17 / 10) / 256;
        r += 36 * rv / 128; g += 74 * rv / 128; b += 118 * rv / 128;
      }
      int cn = ef_cloud[idx];
      int ca = (cn - thr_cloud) * 165 / 13;
      if (ca > 0) {
        if (ca > 165) ca = 165;
        int cl = light < 46 ? 46 : (light > 235 ? 235 : light);
        r = r + (232 * cl / 255 - r) * ca / 255;
        g = g + (238 * cl / 255 - g) * ca / 255;
        b = b + (246 * cl / 255 - b) * ca / 255;
      }
      bg_cache[y][x] = GRB(clamp255(r + jit) >> 3, clamp255(g + jit) >> 3, clamp255(b + jit) >> 3);
    }
  }
}


int main() {
  stars_init();
  build_bg_cache();
  FILE *f = fopen("bgcache.ppm", "wb");
  fprintf(f, "P6\n512 480\n255\n");
  for (int y = 0; y < 480; y++)
    for (int x = 0; x < 512; x++) {
      uint16_t c = bg_cache[y][x];
      unsigned char rgb[3] = {(unsigned char)(((c >> 6) & 31) * 255 / 31),
                              (unsigned char)(((c >> 11) & 31) * 255 / 31),
                              (unsigned char)(((c >> 1) & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
  return 0;
}
