// Per element blink/swirl animation for the status line icons.
//
// Every status element is a pure function of a single integer key, so this
// module only has to remember which key is currently on screen and how far
// through the animation it is. No pixel buffers, no saved bitmaps.
//
// When an element's key changes it plays two phases and leaves every other
// element of the status line untouched:
//
//   phase 1  the icon on screen blinks IA_BLINK_COUNT times and goes dark
//   phase 2  the new icon appears and its cells swirl through the tetrimino
//            colours, settling into the target colour one by one
//
// While an element animates its slot is held at max(old width, new width) and
// the icon is drawn against the left edge of that slot, so the animated icon
// never moves and its neighbours never move mid animation.
//
// Include after field.h, settings.h, tetrimino.h and bitmap.h.

// ---------------------------------------------------------------------------
// Animation tuning
// ---------------------------------------------------------------------------
#define IA_BLINK_MS       2000  // phase 1 duration
#define IA_BLINK_COUNT       4  // on/off cycles within phase 1
#define IA_SWIRL_MS       3000  // phase 2 duration
#define IA_SWIRL_SLOPE_X     1  // tilt of the colour wave across the icon
#define IA_SWIRL_SLOPE_Y     2
#define IA_SWIRL_SPEED       1  // colour bands travelled per frame
#define IA_MONO_LIT_BANDS    4  // of TETRIMINO_COUNT, black and white only
#define IA_MIN_FRAMES        4  // floor when the frame period is coarse

#define IA_BMP_MINUS        10  // s_bmp_small_digits index of the minus sign

// ---------------------------------------------------------------------------

typedef enum {
    IA_TEMPERATURE,
    IA_TREND,
    IA_DAY_TEMPERATURE,
    IA_WEATHER,
    IA_BLUETOOTH,
    IA_BATTERY,
    IA_COUNT,
} IconId;

// Key value meaning "this element is not on screen at all".
#define IA_KEY_NONE (-32768)

typedef enum {
    IA_PHASE_IDLE = 0,  // nothing to do
    IA_PHASE_SEED,      // adopt the first key seen without animating
    IA_PHASE_BLINK,     // phase 1, blinking the old icon away
    IA_PHASE_SWIRL,     // phase 2, settling the new icon in
} IconPhase;

typedef struct {
    uint8_t  phase;
    uint8_t  shown_w;   // width of the key on screen
    uint8_t  target_w;  // width of the key the app wants
    uint8_t  slot;      // cells reserved in the layout, held during animation
    uint16_t frame;     // frames elapsed within the current phase
    int16_t  shown;     // key on screen right now
    int16_t  target;    // key the app wants
} IconAnim;

static IconAnim s_icon_anims[IA_COUNT];

// Duration in frames of the shared animation timer, so the wall clock timing
// holds even when the user changes the animation speed setting.
static int icon_anim_frames(int duration_ms) {
    const int period_ms = s_settings[CUSTOM_ANIMATION_TIMEOUT_MS];
    const int frames = duration_ms / (period_ms > 0 ? period_ms : 1);
    return frames < IA_MIN_FRAMES ? IA_MIN_FRAMES : frames;
}

// Settling order of a cell, spread evenly over 0..255. Deterministic, which is
// what keeps the swirl free of any per pixel state.
static uint8_t icon_anim_hash(int x, int y) {
    uint32_t h = (uint32_t)(x + 1) * 2654435761u ^ (uint32_t)(y + 1) * 40503u;
    h ^= h >> 15;
    return (uint8_t)(h >> 7);
}

// Colour of one cell for the current frame. Returns false if the cell must be
// left as background.
static bool icon_anim_cell(const IconAnim* a, int x, int y, GColor base, GColor* out) {
    *out = base;

    switch (a->phase) {
    case IA_PHASE_BLINK: {
        // Odd half cycles are dark, so the phase always ends on a dark half and
        // hands over to phase 2 with the icon already invisible.
        const int half = (a->frame * 2 * IA_BLINK_COUNT) / icon_anim_frames(IA_BLINK_MS);
        return (half % 2) == 0;
    }
    case IA_PHASE_SWIRL: {
        const int progress = (a->frame * 256) / icon_anim_frames(IA_SWIRL_MS);
        if (progress >= (int)icon_anim_hash(x, y)) {
            return true;  // this cell has settled on the target colour
        }
        const uint32_t band = (uint32_t)(x * IA_SWIRL_SLOPE_X +
                                         y * IA_SWIRL_SLOPE_Y +
                                         a->frame * IA_SWIRL_SPEED);
        #ifdef PBL_COLOR
            out->argb = s_tetrimino_defs[band % TETRIMINO_COUNT].color;
            return true;
        #else
            // A 1 bit display has no colours to swirl, so the same wave drives
            // visibility instead and a dark stripe sweeps through the icon.
            return (band % TETRIMINO_COUNT) < IA_MONO_LIT_BANDS;
        #endif
    }
    default:
        return true;
    }
}

// Forgets every element. Pass silent to adopt the next keys without animating.
static void icon_anim_reset(bool silent) {
    for (int i = 0; i < IA_COUNT; ++i) {
        IconAnim* a = &s_icon_anims[i];
        a->phase = silent ? IA_PHASE_SEED : IA_PHASE_IDLE;
        a->shown = IA_KEY_NONE;
        a->target = IA_KEY_NONE;
        a->shown_w = 0;
        a->target_w = 0;
        a->slot = 0;
        a->frame = 0;
    }
}

// Reports what the app wants to show and returns the key that must be drawn on
// this frame, which is the previous key while phase 1 is still running. width
// is how many cells key renders as, trailing spacing excluded.
static int icon_anim_track(IconId id, int key, int width) {
    IconAnim* a = &s_icon_anims[id];

    if (a->phase == IA_PHASE_SEED) {
        a->phase = IA_PHASE_IDLE;
        a->shown = key;
        a->target = key;
        a->shown_w = width;
        a->target_w = width;
        a->slot = width;
        return a->shown;
    }

    if (key != a->target) {
        a->target = key;
        a->target_w = width;

        if (a->phase == IA_PHASE_BLINK) {
            // Already blinking the old icon away. Retarget without restarting,
            // and reserve room in case the newest key is the widest so far.
            if (key == a->shown) {
                a->phase = IA_PHASE_IDLE;  // value came back, nothing to reveal
                a->frame = 0;
            } else if (width > a->slot) {
                a->slot = width;
            }
        } else if (a->shown == IA_KEY_NONE) {
            // Nothing on screen to blink away, so reveal straight away.
            a->shown = key;
            a->shown_w = width;
            a->slot = width;
            a->frame = 0;
            a->phase = (key == IA_KEY_NONE) ? IA_PHASE_IDLE : IA_PHASE_SWIRL;
        } else {
            a->slot = (width > a->shown_w) ? width : a->shown_w;
            a->frame = 0;
            a->phase = IA_PHASE_BLINK;
        }
    }

    if (a->phase == IA_PHASE_IDLE) {
        // Settled, so the slot is simply what is on screen.
        a->shown_w = width;
        a->target_w = width;
        a->slot = width;
    }

    return a->shown;
}

static bool icon_anim_active() {
    for (int i = 0; i < IA_COUNT; ++i) {
        if (s_icon_anims[i].phase >= IA_PHASE_BLINK) {
            return true;
        }
    }
    return false;
}

static void icon_anim_step() {
    for (int i = 0; i < IA_COUNT; ++i) {
        IconAnim* a = &s_icon_anims[i];
        switch (a->phase) {
        case IA_PHASE_BLINK:
            a->frame += 1;
            if (a->frame >= icon_anim_frames(IA_BLINK_MS)) {
                a->shown = a->target;
                a->shown_w = a->target_w;
                a->frame = 0;
                if (a->shown == IA_KEY_NONE) {
                    a->phase = IA_PHASE_IDLE;
                    a->slot = 0;  // the element is gone, release its space
                } else {
                    a->phase = IA_PHASE_SWIRL;  // slot stays reserved
                }
            }
            break;
        case IA_PHASE_SWIRL:
            a->frame += 1;
            if (a->frame >= icon_anim_frames(IA_SWIRL_MS)) {
                a->frame = 0;
                a->phase = IA_PHASE_IDLE;
                a->slot = a->shown_w;
            }
            break;
        default:
            break;
        }
    }
}

// Draws one bitmap through the element's animation filter at a fixed position.
static void icon_draw_bitmap_at(IconId id, const Bitmap* bmp, int x, int y, GColor color) {
    const IconAnim* a = &s_icon_anims[id];
    int yoffset = 0;
    for (int j = 0; j < bmp->height; ++j) {
        for (int i = 0; i < bmp->width; ++i) {
            GColor cell = color;
            if (bmp->data[yoffset + i] != ' ' && icon_anim_cell(a, x + i, y + j, color, &cell)) {
                field_draw(x + i, y + j, cell);
            }
        }
        yoffset += bmp->width;
    }
}

// Draws one bitmap in the status line flow, advancing the cursor by the
// reserved slot rather than by the bitmap so that neighbours hold still.
static void icon_draw_bitmap(IconId id, const Bitmap* bmp, int* x, int y, GColor color, int spacing) {
    icon_draw_bitmap_at(id, bmp, *x, y, color);
    *x += s_icon_anims[id].slot + spacing;
}

// Cells taken up by value rendered as small digits, trailing spacing excluded.
static int icon_number_width(int value) {
    const int magnitude = abs(value);
    int glyphs = (value < 0) ? 1 : 0;
    glyphs += (magnitude >= 100) ? 3 : ((magnitude >= 10) ? 2 : 1);
    return glyphs * (BMP_SMALL_DIGIT_WIDTH + 1) - 1;
}

// Draws a signed value as small digits, in the flow, animated as one element.
static void icon_draw_number(IconId id, int* x, int value, int y, GColor color) {
    const int magnitude = abs(value);
    int cursor = *x;

    if (value < 0) {
        icon_draw_bitmap_at(id, &s_bmp_small_digits[IA_BMP_MINUS], cursor, y, color);
        cursor += BMP_SMALL_DIGIT_WIDTH + 1;
    }
    if (magnitude >= 100) {
        icon_draw_bitmap_at(id, &s_bmp_small_digits[(magnitude / 100) % 10], cursor, y, color);
        cursor += BMP_SMALL_DIGIT_WIDTH + 1;
    }
    if (magnitude >= 10) {
        icon_draw_bitmap_at(id, &s_bmp_small_digits[(magnitude / 10) % 10], cursor, y, color);
        cursor += BMP_SMALL_DIGIT_WIDTH + 1;
    }
    icon_draw_bitmap_at(id, &s_bmp_small_digits[magnitude % 10], cursor, y, color);

    *x += s_icon_anims[id].slot + 1;
}
