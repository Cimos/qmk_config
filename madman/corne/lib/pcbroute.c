// PCB auto-router animation: nets route themselves between pads and vias with
// 45-degree corners, keeping clearance from copper already laid. When no more
// nets fit, the board is wiped and routing starts again.
#include "pcbroute.h"
#include <string.h>

#define H 32
#define MAX_PTS 4
#define TRIES 250
#define GIVE_UP 4  // failed plans in a row before the board is wiped

static uint8_t  fb[PCB_FB_SIZE];
static uint16_t W = 128;
static uint32_t rng = 1;

typedef enum { ROUTING, PAUSE, WIPE, BANNER, DONE } phase_t;
static phase_t phase;
static int16_t  px[MAX_PTS], py[MAX_PTS], cx, cy;
static uint16_t wipe_x;
static uint8_t  npts, seg, end_kind, pause_left, fails, banner_t;

static uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static int rr(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
static int sgn(int v) { return (v > 0) - (v < 0); }
static int iabs(int v) { return v < 0 ? -v : v; }

static void pset(int x, int y, int on) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    uint8_t *b = &fb[(y / 8) * PCB_MAX_W + x];
    if (on) *b |= 1 << (y % 8); else *b &= ~(1 << (y % 8));
}
static int pget(int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H) return 1;  // edges count as copper
    return (fb[(y / 8) * PCB_MAX_W + x] >> (y % 8)) & 1;
}
static int area_clear(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (pget(x, y)) return 0;
    return 1;
}

// Pads: 3x3 square. Vias: 5x5 ring with the centre drilled out.
static void draw_pad(int x, int y) {
    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) pset(x + i, y + j, 1);
}
static void draw_via(int x, int y) {
    for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++) {
            int corner = iabs(i) == 2 && iabs(j) == 2;
            int hole   = iabs(i) <= 0 && iabs(j) <= 0;
            pset(x + i, y + j, !corner && !hole);
        }
}

// Walk the planned path and check every pixel has 1px clearance, ignoring
// the area right around the two end points (they get pads).
static int path_clear(void) {
    int x = px[0], y = py[0];
    for (uint8_t s = 1; s < npts; s++) {
        while (x != px[s] || y != py[s]) {
            x += sgn(px[s] - x);
            y += sgn(py[s] - y);
            int near_start = iabs(x - px[0]) <= 3 && iabs(y - py[0]) <= 3;
            int near_end   = iabs(x - px[npts - 1]) <= 4 && iabs(y - py[npts - 1]) <= 4;
            if (near_start || near_end) continue;
            if (!area_clear(x - 1, y - 1, x + 1, y + 1)) return 0;
        }
    }
    return 1;
}

static int plan_net(void) {
    for (int t = 0; t < TRIES; t++) {
        int x0 = rr(3, W - 4), y0 = rr(3, H - 4);
        int x1 = rr(3, W - 4), y1 = rr(3, H - 4);
        int dx = x1 - x0, dy = y1 - y0;
        if (iabs(dx) + iabs(dy) < 8) continue;
        npts  = 0;
        px[npts] = x0; py[npts++] = y0;
        if (iabs(dx) >= iabs(dy)) {  // mostly horizontal: run, 45, run
            int a = x0 + sgn(dx) * rr(0, iabs(dx) - iabs(dy));
            px[npts] = a; py[npts++] = y0;
            px[npts] = a + sgn(dx) * iabs(dy); py[npts++] = y1;
        } else {                     // mostly vertical: drop, 45, drop
            int a = y0 + sgn(dy) * rr(0, iabs(dy) - iabs(dx));
            px[npts] = x0; py[npts++] = a;
            px[npts] = x1; py[npts++] = a + sgn(dy) * iabs(dx);
        }
        px[npts] = x1; py[npts++] = y1;
        if (!area_clear(x0 - 2, y0 - 2, x0 + 2, y0 + 2)) continue;
        if (!area_clear(x1 - 3, y1 - 3, x1 + 3, y1 + 3)) continue;
        if (!path_clear()) continue;
        draw_pad(x0, y0);
        cx = x0; cy = y0; seg = 1;
        end_kind = rnd() % 3 == 0;  // one in three nets ends on a via
        return 1;
    }
    return 0;
}


// "DRONE DADDY" banner shown after each wipe: DRONE on the left screen, DADDY
// on the right. 5x7 glyphs (column bytes, bit 0 at the top) drawn 4x size.
#define WIPE_STEP     8   // pixels the wipe moves per tick
#define GLYPH_SCALE   4
#define GLYPH_PITCH   24  // 5 columns x 4 + 4px gap
#define BANNER_FLASH  6   // ticks per on/off half-cycle
#define BANNER_TICKS  (BANNER_FLASH * 8 + 15)  // four flashes, then hold on

static const uint8_t glyph_cols[][5] = {
    ['A' - 'A'] = {0x7E, 0x11, 0x11, 0x11, 0x7E},
    ['D' - 'A'] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['E' - 'A'] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['N' - 'A'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O' - 'A'] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['R' - 'A'] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['Y' - 'A'] = {0x07, 0x08, 0x70, 0x08, 0x07},
};

static void draw_word(const char *word, int x) {
    for (; *word; word++, x += GLYPH_PITCH) {
        const uint8_t *g = glyph_cols[*word - 'A'];
        for (int c = 0; c < 5; c++)
            for (int r = 0; r < 7; r++)
                if (g[c] >> r & 1)
                    for (int j = 0; j < GLYPH_SCALE; j++)
                        for (int i = 0; i < GLYPH_SCALE; i++)
                            pset(x + c * GLYPH_SCALE + i, 2 + r * GLYPH_SCALE + j, 1);
    }
}

static void draw_banner(void) {
    int word_w = 5 * GLYPH_PITCH - (GLYPH_PITCH - 5 * GLYPH_SCALE);
    int x      = (128 - word_w) / 2;
    draw_word("DRONE", x);
    draw_word("DADDY", 128 + x);
}

void pcb_init(uint16_t width, uint32_t seed) {
    memset(fb, 0, sizeof fb);
    W     = width > PCB_MAX_W ? PCB_MAX_W : width;
    fails = 0;
    rng   = seed ? seed : 1;
    phase = plan_net() ? ROUTING : WIPE;
    wipe_x = 0;
}

void pcb_tick(uint8_t steps) {
    switch (phase) {
        case ROUTING:
            while (steps--) {
                if (cx == px[seg] && cy == py[seg] && ++seg >= npts) {
                    if (end_kind) draw_via(cx, cy); else draw_pad(cx, cy);
                    phase = PAUSE; pause_left = 3;
                    return;
                }
                cx += sgn(px[seg] - cx);
                cy += sgn(py[seg] - cy);
                pset(cx, cy, 1);
            }
            break;
        case PAUSE:
            if (pause_left-- == 0) {
                if (plan_net()) {
                    fails = 0;
                    phase = ROUTING;
                } else if (++fails >= GIVE_UP) {
                    fails = 0;
                    pause_left = 20;  // admire the finished board, then wipe
                    phase = WIPE;
                } else {
                    pause_left = 1;
                }
            }
            break;
        case WIPE:  // a bright scan line sweeps across and clears the board
            if (pause_left) { pause_left--; break; }
            for (int y = 0; y < H; y++) {
                for (int i = 0; i < WIPE_STEP; i++) pset(wipe_x + i, y, 0);
                pset(wipe_x + WIPE_STEP, y, 1);
                pset(wipe_x + WIPE_STEP + 1, y, 1);
            }
            wipe_x += WIPE_STEP;
            if (wipe_x >= W) {
                memset(fb, 0, sizeof fb);
                banner_t = 0;
                phase    = W >= 256 ? BANNER : DONE;  // banner needs both screens
            }
            break;
        case BANNER:
            memset(fb, 0, sizeof fb);
            if ((banner_t / BANNER_FLASH) % 2 == 0 || banner_t >= BANNER_FLASH * 8) draw_banner();
            if (++banner_t >= BANNER_TICKS) {
                memset(fb, 0, sizeof fb);
                phase = DONE;
            }
            break;
        case DONE:
            break;
    }
}

bool pcb_done(void) { return phase == DONE; }
bool pcb_is_routing(void) { return phase == ROUTING; }
bool pcb_banner_lit(void) {
    return phase == BANNER && ((banner_t / BANNER_FLASH) % 2 == 0 || banner_t >= BANNER_FLASH * 8);
}

const uint8_t *pcb_page(uint8_t page, uint16_t x0) { return &fb[page * PCB_MAX_W + x0]; }
