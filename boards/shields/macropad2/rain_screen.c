/*
 * Rainy city: full-screen 128x32 animation for the macropad2 OLED.
 * Rain falls over a skyline (randomized at each boot) and windows flicker.
 */

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zmk/display/status_screen.h>

#define W 128
#define H 32
#define FRAME_MS 40

/* ZMK's convention on the SSD1306: black = lit pixel. Flip with CONFIG_MACROPAD2_SCREEN_INVERT. */
#if IS_ENABLED(CONFIG_MACROPAD2_SCREEN_INVERT)
#define FG lv_color_white()
#define BG lv_color_black()
#else
#define FG lv_color_black()
#define BG lv_color_white()
#endif

static lv_color_t cbuf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(W, H)];
static lv_obj_t *canvas;

/* xorshift32 PRNG */
static uint32_t rng = 1;
static uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static int rnd_range(int n) { return (int)(rnd() % (uint32_t)n); }

static inline void put(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H) {
        cbuf[y * W + x] = FG;
    }
}

/* Positions use 1/16 pixel fixed point. */
static inline int fx(int v) { return v >= 0 ? (v + 8) >> 4 : -((-v + 8) >> 4); }

/* Skyline */
static uint8_t top[W];
struct win {
    uint8_t x, y;
    bool on;
};
static struct win wins[512];
static int nwins;

static void city_init(void) {
    for (int i = 0; i < W; i++) {
        top[i] = H;
    }
    int x = 0;
    while (x < W) {
        int w = 6 + rnd_range(10), h = 7 + rnd_range(16), t = H - h;
        int end = MIN(W, x + w);
        for (int i = x; i < end; i++) {
            top[i] = t;
        }
        for (int yy = t + 2; yy < H - 1; yy += 3) {
            for (int xx = x + 2; xx < end - 1; xx += 3) {
                if (nwins < (int)ARRAY_SIZE(wins)) {
                    wins[nwins++] = (struct win){xx, yy, rnd_range(10) < 4};
                }
            }
        }
        x += w + (rnd_range(100) < 35 ? 2 : 0);
    }
}

/* Rain */
#define NDROPS 34
struct drop {
    int16_t x, y, v;
};
static struct drop drops[NDROPS];

struct splash {
    int8_t x, y;
    uint8_t t;
};
static struct splash spl[NDROPS];

static void drop_reset(struct drop *d, int max_height) {
    d->x = rnd_range(150 * 16);
    d->y = -rnd_range(max_height * 16);
}

static void add_splash(int x, int y) {
    for (int i = 0; i < NDROPS; i++) {
        if (spl[i].t == 0) {
            spl[i] = (struct splash){x, y, 3};
            return;
        }
    }
}

static void frame(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    for (int i = 0; i < W * H; i++) {
        cbuf[i] = BG;
    }

    /* Roofline and building edges */
    for (int x = 0; x < W; x++) {
        if (top[x] < H) {
            put(x, top[x]);
        }
        if (x > 0 && top[x] != top[x - 1]) {
            int a = MIN(top[x], top[x - 1]), c = MAX(top[x], top[x - 1]);
            int px = top[x] < top[x - 1] ? x : x - 1;
            for (int y = a; y <= c && y < H; y++) {
                put(px, y);
            }
        }
    }

    /* Flickering windows */
    if (nwins > 0 && rnd_range(10) < 3) {
        struct win *w = &wins[rnd_range(nwins)];
        w->on = !w->on;
    }
    for (int i = 0; i < nwins; i++) {
        if (wins[i].on) {
            put(wins[i].x, wins[i].y);
        }
    }

    /* Raindrops, slightly slanted */
    for (int i = 0; i < NDROPS; i++) {
        struct drop *d = &drops[i];
        d->y += d->v;
        d->x -= d->v * 35 / 100;
        int xi = fx(d->x);
        if (xi >= 0 && xi < W && d->y >= (top[xi] - 1) * 16) {
            add_splash(xi, top[xi] - 1);
            drop_reset(d, 20);
            continue;
        }
        if (d->x < -2 * 16) {
            drop_reset(d, 20);
            continue;
        }
        put(fx(d->x), fx(d->y));
        put(fx(d->x + 6), fx(d->y - 16));
    }

    /* Splashes last 3 frames */
    for (int i = 0; i < NDROPS; i++) {
        if (spl[i].t > 0) {
            spl[i].t--;
            put(spl[i].x - 1, spl[i].y);
            put(spl[i].x + 1, spl[i].y);
        }
    }

    lv_obj_invalidate(canvas);
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(canvas, cbuf, W, H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    rng = k_cycle_get_32() | 1;
    city_init();
    for (int i = 0; i < NDROPS; i++) {
        drops[i].v = 26 + rnd_range(19);
        drop_reset(&drops[i], 32);
    }

    frame(NULL);
    lv_timer_create(frame, FRAME_MS, NULL);
    return screen;
}
