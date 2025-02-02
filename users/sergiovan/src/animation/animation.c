#include "animation.h"
#include "circular_buffer/circular_buffer.h"
#include "lib/lib8tion/lib8tion.h"

#include "noise/noise_gen.h"

#include <stdlib.h>

#ifndef LED_COUNT
#    error "LED_COUNT was not defined"
#endif

/* Circular buffer boilerplate */

#define CIRCULAR_BUFFER_ELEM_SIZE sizeof(animation_t)
#define CIRCULAR_BUFFER_ELEMS 16
#define CIRCULAR_BUFFER_BYTE_SIZE (CIRCULAR_BUFFER_ELEM_SIZE * CIRCULAR_BUFFER_ELEMS)

typedef struct keymap_point {
    uint8_t r;
    uint8_t c;
} keymap_point_t;

/* Memory for the circular buffer. Contains raw data */
static uint8_t circular_buffer_mem[CIRCULAR_BUFFER_BYTE_SIZE] = {0};

/* Circular buffer handle */
static circular_buffer_t *animations = NULL;

/* Reverse LED map : LED Index => Keyboard position */
static keymap_point_t reverse_led_map[LED_COUNT] = {0};

/* Convenience functions that act on the circular buffer */

static inline bool push(animation_t animation) {
    return circular_buffer_push(animations, &animation);
}

static inline animation_t *pop(animation_t *animation) {
    return circular_buffer_pop(animations, animation);
}

static inline bool unshift(animation_t animation) {
    return circular_buffer_unshift(animations, &animation);
}

static inline animation_t *shift(animation_t *animation) {
    return circular_buffer_shift(animations, animation);
}

static inline animation_t *at(uint8_t i) {
    return circular_buffer_at(animations, i);
}

static inline bool empty(void) {
    return circular_buffer_empty(animations);
}

static inline uint8_t length(void) {
    return circular_buffer_length(animations);
}

static inline bool full(void) {
    return circular_buffer_full(animations);
}

/* Private functions */

/* Thickness in units of the wave. Keys are on average 10 units apart */
#define WAVE_THICKNESS 23
#define WAVE_THICKNESS_FACTOR ((255 / (WAVE_THICKNESS)) + 1)

/* Base state at the start of each frame of all the LEDs */
static COLOR base_state[LED_COUNT] = {{0}};

/* Current calculated state of each LED in this frame */
static COLOR calc_state[LED_COUNT] = {{0}};

/**
 * @brief Enum holding the action that must be taken after each animation is processed
 */
typedef enum apply_res : uint8_t {
    APPLY_OK,         /* No action needed */
    APPLY_CLEAR_THIS, /* Remove this element from the buffer if it's the first one */
    APPLY_NEW_BASE,   /* Remove all elements before and including this one from the buffer */
    BECOME_FIRST,     /* Remove all elements before this one from the buffer */
    BECOME_SHIMMER,   /* Remove this element from the buffer if it's the first one,
                         and convert animation into shimmer */
} apply_res_e;

/**
 * @brief Resets base state and calculated state to 0
 */
static inline void clear_all_state(void) {
    memset(calc_state, 0, sizeof calc_state);
    memset(base_state, 0, sizeof base_state);
}

/**
 * @brief Resets calc state to current base state
 *
 */
static inline void clear_calc_state(void) {
    memcpy(&calc_state, &base_state, sizeof base_state);
}

/**
 * @brief Convenience function to test if a color is off, i.e. if the value is 0
 *
 * @param color The color to test for
 * @return true The value of the color is 0
 * @return false The color is visible on a LED
 */
static inline bool hsv_is_off(COLOR color) {
    return color.v == 0;
}

/* Functions to bridge LED AND RGB */
static inline COLOR get_matrix_default_color(void) {
#if USING_RGB
    return rgb_matrix_get_hsv();
#else
    return MAKE_COLOR(led_matrix_get_val());
#endif
}

static inline uint8_t get_matrix_speed(void) {
#if USING_RGB
    return rgb_matrix_get_speed();
#else
    return led_matrix_get_speed();
#endif
}

static inline uint8_t get_matrix_val(void) {
#if USING_RGB
    return rgb_matrix_get_val();
#else
    return led_matrix_get_val();
#endif
}

static inline bool get_matrix_enabled(void) {
#if USING_RGB
    return rgb_matrix_is_enabled();
#else
    return led_matrix_is_enabled();
#endif
}

static inline int matrix_check_finished_leds(uint8_t led_max) {
#if USING_RGB
    return rgb_matrix_check_finished_leds(led_max);
#else
    return led_matrix_check_finished_leds(led_max);
#endif
}

/**
 * @brief Convenience function to generate perlin noise at a specific x, y coordinate at time t
 *
 * @param x The x coordinate to get perlin noise for
 * @param y The y coordinate to get perlin noise for
 * @param t The time modifier
 * @return uint8_t Value of the perlin noise
 */
static inline uint8_t get_perlin(uint8_t x, uint8_t y, uint32_t t) {
    /* Scaling time by 1.1, 1.25 and 1.5 times, respectively */
    uint32_t slow   = ((t * 282) >> 8);
    uint32_t medium = ((t * 320) >> 8);
    uint32_t fast   = ((t * 384) >> 8);

#if USING_RGB

    /* 0x666 is ~0.025 in u32q16 */
    uint16_t perlin_1 = perlin2d_fixed(x + slow, y + fast, 0x666);
    uint16_t perlin_2 = perlin2d_fixed((224 - x) + medium, y + slow, 0x666);
    uint16_t perlin_3 = perlin2d_fixed(x + fast, (64 - y) + medium, 0x666);

    return (perlin_1 + perlin_2 + perlin_3) & 0xFF;
#else
    /* 0x1666 is ~0.0875 in u32q16 */
    uint16_t perlin_1 = perlin2d_fixed(x + slow, y + fast, 0x1666);
    uint16_t perlin_2 = perlin2d_fixed((224 - x) + medium, y + slow, 0x1666);
    uint16_t perlin_3 = perlin2d_fixed(x + fast, (64 - y) + medium, 0x1666);

    uint32_t perlin = perlin_1 + perlin_2 + perlin_3;

    perlin *= 0xE0; // This is 0xE0 / 0x100 in u16q8
    perlin /= 0x0300;

    return perlin & 0xFF;
#endif
}

/**
 * @brief Convenience function to find out if an led index is in the animation's keymap
 *
 * @param animation Animation to check in
 * @param led LED index
 * @return false The animation has a keymap, and the LED index is NOT part of it
 * @return true Otherwise
 */
static inline bool animation_led_in_keymap(animation_t *animation, uint8_t led) {
    if (animation->keymap != NULL) {
        keymap_point_t key = reverse_led_map[led];
        if ((*animation->keymap)[key.r][key.c] <= KC_TRANSPARENT) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Convenience function to get the correct color from an animation
 *
 * @param animation Animation to get color from
 * @param led LED index to get the color of
 * @param index Which color to get, i.e. BASE or RESULT. See `animation_hsv_color_layer`
 * @return HSV The HSV value for this animation at this time for this LED
 */
static COLOR animation_get_hsv_indexed(animation_t *animation, uint8_t led, uint8_t index) {
    if (!animation_led_in_keymap(animation, led)) {
        index += ANIMATION_HSV_COLOR_BASE_N;
    }

    if (index >= ANIMATION_HSV_COLOR_COUNT) {
        index = ANIMATION_HSV_COLOR_BASE;
    }

    animation_color_t color = animation->hsv_colors[index];

    switch (color.special) {
        case ANIMATION_COLOR_NONE:
            return color.color;
        case ANIMATION_COLOR_DEFAULT:
            return get_matrix_default_color();
        case ANIMATION_COLOR_TRANS:
            return calc_state[led];
        case ANIMATION_COLOR_RANDOM:
            return MAKE_COLOR(random8(), 0xFF, 0xFF);
        case ANIMATION_COLOR_NOISE: {
            uint8_t x = g_led_config.point[led].x;
            uint8_t y = g_led_config.point[led].y;

            uint8_t hue = get_perlin(x, y, animation->ticks << 16);

            return MAKE_COLOR(hue, 0xFF, 0xFF);
        }
        case ANIMATION_COLOR_SHIMMER: {
            uint8_t x = g_led_config.point[led].x;
            uint8_t y = g_led_config.point[led].y;

            uint32_t t = timer_read32() >> 5;

            t = (t * (1 + ((uint64_t)get_matrix_speed()))) >> 8;

            uint8_t color = get_perlin(x, y, t);

            return MAKE_COLOR(.h = color, .s = 0xFF, .v = 0xFF);
        }
    }

    return MAKE_COLOR(0x55, 0x55, 0x55); // Cannot be reached, but will signify something went wrong
}

/**
 * @brief Convenience function equivalent to `animation_get_hsv_indexed(animation, led, ANIMATION_HSV_COLOR_BASE)`
 */
static inline COLOR animation_get_hsv(animation_t *animation, uint8_t led) {
    return animation_get_hsv_indexed(animation, led, ANIMATION_HSV_COLOR_BASE);
}

/*
 *            A diagram of the wave
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     WAVE
 *          /----------------------\
 *     OUTSIDE WAVE        INSIDE/PAST WAVE
 * /-------------------\ /-----------------\
 * 0        |0       255|0       255|       255          : VALUE
 * LED      |           |           |       WAVE ORIGIN
 * v        |           v           |       v
 * ----------------------------------------------------
 * l        (           (           (       o
 * ----------------------------------------------------
 *          ^           ^           ^
 *  WAVE OUTER EDGE     |           WAVE INNER EDGE
 *                   WAVE CENTER
 *                   WAVE RADIUS
 */

/**
 * @brief Struct with information about an LED in a wave animation
 */
typedef struct wave_info {
    /* Value for the LED. Goes from 0 (start of animation til entering wave) to 255 (at the wave radius),
       then again from 0 (right after the wave) to 255 (past the wave til end of animation) */
    uint8_t val;
    bool    in_wave;       /* If the LED is currently in the wave itself */
    bool    inside_radius; /* If the LED is inside the wave radius or outside */
} wave_info_t;

/**
 * @brief Convenience function to get wave information for an LED in an animation
 *
 * @param animation The animation to get information for
 * @param led The LED to query information for
 * @param wave_radius The current wave radius
 * @return wave_info_t Information about the LED regarding the wave animation
 */
static inline wave_info_t animation_wave_get_key_value(animation_t *animation, uint8_t led, int64_t wave_radius) {
    /* Wave origin */
    uint8_t orig_x = g_led_config.point[animation->led_index].x;
    uint8_t orig_y = g_led_config.point[animation->led_index].y;

    uint8_t val = 0;
    int16_t dx  = g_led_config.point[led].x - orig_x;
    int16_t dy  = g_led_config.point[led].y - orig_y;
    /* Distance to origin. Never changes per LED */
    int16_t dist = sqrt16(dx * dx + dy * dy);
    /* Distance to wave. Decreases, then increases */
    uint8_t diff = llabs(dist - wave_radius);

    bool in_wave       = diff < WAVE_THICKNESS;
    bool inside_radius = wave_radius > dist;

    if (in_wave) {
        if (!inside_radius) {
            /* When approaching the wave center from outside, go from 0 to 255 */
            val = ease8InOutQuad(0xFF - qmul8(diff, WAVE_THICKNESS_FACTOR));
        } else {
            /* When approaching the wave edge from inside, go to 0 from 255 */
            val = ease8InOutQuad(qmul8(diff, WAVE_THICKNESS_FACTOR));
        }
    } else {
        if (!inside_radius) {
            val = 0;
        } else {
            val = 255;
        }
    }

    return (wave_info_t){
        .val           = val,
        .in_wave       = in_wave,
        .inside_radius = inside_radius,
    };
}

/**
 * @brief Convenience function for checking if an animation can have "holes"
 *
 * @param animation The animation to check
 * @return true If this animation could possibly show the animations before it
 * @return false If this animation covers all keys
 */
static inline bool can_apply_new_base(animation_t *animation) {
    animation_color_t(*colors)[4] = &animation->hsv_colors;

    if (animation->type >= WAVE) {
        // Uses colors 1 and 2
        if (animation->keymap == NULL) {
            // No holes
            return (*colors)[ANIMATION_HSV_COLOR_BASE].special != ANIMATION_COLOR_TRANS && (*colors)[ANIMATION_HSV_COLOR_RESULT].special != ANIMATION_COLOR_TRANS;
        } else {
            return (*colors)[ANIMATION_HSV_COLOR_BASE].special != ANIMATION_COLOR_TRANS && (*colors)[ANIMATION_HSV_COLOR_RESULT].special != ANIMATION_COLOR_TRANS && (*colors)[ANIMATION_HSV_COLOR_BASE_N].special != ANIMATION_COLOR_TRANS && (*colors)[ANIMATION_HSV_COLOR_RESULT_N].special != ANIMATION_COLOR_TRANS;
        }
    } else {
        if (animation->keymap == NULL) {
            // No holes
            return (*colors)[ANIMATION_HSV_COLOR_BASE].special != ANIMATION_COLOR_TRANS;
        } else {
            return (*colors)[ANIMATION_HSV_COLOR_BASE].special != ANIMATION_COLOR_TRANS && (*colors)[ANIMATION_HSV_COLOR_BASE_N].special != ANIMATION_COLOR_TRANS;
        }
    }
}

/**
 * @brief Convenience function to map8 when the start can be larger than the end
 *
 * @param in Value to map
 * @param a One end of the range
 * @param b Other end of the range
 * @return uint8_t Mapped value
 */
static inline uint8_t clean_map8(uint8_t in, uint8_t a, uint8_t b) {
    if (a < b) {
        return map8(in, a, b);
    } else if (b < a) {
        return map8(255 - in, b, a);
    } else {
        return a;
    }
}

// TODO Make time pass through to all animations
// TODO Make time speed-dependent in some way, so I don't need to worry about it
// inside each animation
// TODO Make algorithm faster by doing only the current batch of LEDs. Note that animations could
// be added in the middle of a "frame", so that needs to be taken into account
// TODO Remove SHIMMER and add CONTINUOUS type
// TODO Optimize SHIMMER color to be almost equal to SHIMMER mode

/**
 * @brief Calculates one frame for one animation
 *
 * @param animation Animation to calculate for
 * @param first If this animation is the first in the queue
 * @param finish If this animation must reach its finished state
 * @return apply_res_e The result of running this frame of this animation
 */
static apply_res_e apply_animation(animation_t *animation, bool first, bool finish) {
    /* Skip animations that start in the future */
    if (!timer_expired32(timer_read32(), animation->ticks)) {
        return APPLY_OK;
    }

    if (animation->done) {
        finish = true;
    }

    bool new_base = first || can_apply_new_base(animation);

    switch (animation->type) {
        case SOLID_KEY:
            calc_state[animation->led_index] = animation_get_hsv(animation, animation->led_index);
            if (new_base) {
                base_state[animation->led_index] = animation_get_hsv(animation, animation->led_index);
            }
            return APPLY_CLEAR_THIS;
        case SOLID_ALL:
            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                COLOR new_c   = animation_get_hsv(animation, i);
                calc_state[i] = new_c;
                if (new_base) {
                    base_state[i] = new_c;
                }
            }
            return new_base ? APPLY_NEW_BASE : APPLY_OK;
        case WAVE: {
            // u32q16
            const uint32_t wave_time_ms   = 400 << 16; // It takes 400 MS at most
            const uint32_t u32q16_255_400 = 0xa333;    // 255 / 400 in u32q16

            uint32_t active_for = timer_elapsed32(animation->ticks);
            active_for          = scale16by8((uint16_t)active_for, get_matrix_speed()) << 16;

            bool any_left = false;

            // Guarantees safe passage + stoppage
            if (active_for >= wave_time_ms || finish) {
                active_for = wave_time_ms;
            }

            // u32q16
            uint32_t radius = wave_time_ms - (wave_time_ms - active_for);

            // Result of multiplication is u64q32
            // but we bring it back to u32q16
            // Cutting off 4 bytes at the top is ok since (400 << 16) * 0xA333 is only
            // 0xfeffb00000
            radius = ((uint64_t)radius * u32q16_255_400) >> 32;

            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                wave_info_t info = animation_wave_get_key_value(animation, i, radius);
                COLOR       current_col;
                COLOR       target_col;

                if (info.in_wave || !info.inside_radius) {
                    any_left = true;
                }

                if (!info.inside_radius) {
                    current_col = calc_state[i];
                    target_col  = animation_get_hsv_indexed(animation, i, ANIMATION_HSV_COLOR_BASE);
                } else {
                    current_col = animation_get_hsv_indexed(animation, i, ANIMATION_HSV_COLOR_BASE);
                    target_col  = animation_get_hsv_indexed(animation, i, ANIMATION_HSV_COLOR_RESULT);
                }

                bool current_off = hsv_is_off(current_col);
                bool target_off  = hsv_is_off(target_col);

                if (current_off && !target_off) {
                    current_col   = target_col;
                    current_col.v = 0;
                } else if (!current_off && target_off) {
                    target_col   = current_col;
                    target_col.v = 0;
                }

                calc_state[i].h = clean_map8(info.val, current_col.h, target_col.h);
                calc_state[i].s = clean_map8(info.val, current_col.s, target_col.s);
                calc_state[i].v = clean_map8(info.val, current_col.v, target_col.v);

                if (calc_state[i].v == 0) {
                    calc_state[i].h = calc_state[i].s = 0;
                }
            }

            if (!any_left || active_for == wave_time_ms) {
                animation->done          = true;
                animation_color_t target = animation->hsv_colors[ANIMATION_HSV_COLOR_RESULT]; // Intended color
                if (target.special == ANIMATION_COLOR_SHIMMER) {
                    return BECOME_SHIMMER;
                } else {
                    for (uint8_t i = 0; i < LED_COUNT; ++i) {
                        COLOR new_c   = animation_get_hsv_indexed(animation, i, ANIMATION_HSV_COLOR_RESULT);
                        calc_state[i] = new_c;
                        if (new_base) {
                            base_state[i] = new_c;
                        }
                    }
                    return new_base ? APPLY_NEW_BASE : APPLY_OK;
                }
            }

            return APPLY_OK;
        }
        case SHIMMER: {
            uint32_t t = timer_read32() >> 5;

            t = (t * (1 + ((uint64_t)get_matrix_speed()))) >> 8;

            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                if (animation_led_in_keymap(animation, i)) {
                    uint8_t x = g_led_config.point[i].x;
                    uint8_t y = g_led_config.point[i].y;

                    uint8_t hue = get_perlin(x, y, t);

                    calc_state[i] = MAKE_COLOR(.h = hue, .s = 0xFF, .v = 0xFF);
                } else {
                    calc_state[i] = animation->hsv_colors[ANIMATION_HSV_COLOR_BASE_N].color;
                }
            }
            return (new_base && !first) ? BECOME_FIRST : APPLY_OK;
        }
        default:
            return APPLY_OK;
    }
}

/**
 * @brief Convenience function to actually turn the calculated state into LEDs
 *
 * @param params Effect parameters
 * @return true If the last of the LEDs has been run through
 * @return false Otherwise
 */
static inline bool apply_calc_state(effect_params_t *params) {
    MATRIX_USE_LIMITS(led_min, led_max);

    for (uint8_t i = led_min; i < led_max; ++i) {
        MATRIX_TEST_LED_FLAGS();

        COLOR color = calc_state[i];
        color.v     = scale8(color.v, get_matrix_val());
#if USING_RGB
        RGB color_rgb = hsv_to_rgb(color);
        rgb_matrix_set_color(i, color_rgb.r, color_rgb.g, color_rgb.b);
#else
        led_matrix_set_value(i, color.v);
#endif
    }

    return matrix_check_finished_leds(led_max);
}

static inline animation_color_t animation_color_internal(COLOR c) {
#if USING_RGB
    return animation_color_hsv(c.h, c.s, c.v);
#else
    return animation_color_val(c.v);
#endif
}

/* Public functions */

animation_color_t animation_color_val(uint8_t val) {
    return (animation_color_t){
        .color   = MAKE_COLOR(val),
        .special = ANIMATION_COLOR_NONE,
    };
}

animation_color_t animation_color_hsv(uint8_t h, uint8_t s, uint8_t v) {
    return (animation_color_t){
        .color   = MAKE_COLOR(.h = h, .s = s, .v = v),
        .special = ANIMATION_COLOR_NONE,
    };
}

animation_color_t animation_color_special(animation_color_special_e special) {
    return (animation_color_t){
        .color   = MAKE_COLOR(COLOR_OFF),
        .special = special,
    };
}

animation_t animation_clear(void) {
    return (animation_t){
        .type = SOLID_ALL,

        .led_index = 0,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                animation_color_internal(MAKE_COLOR(COLOR_OFF)),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_clear_key(uint8_t led_index) {
    return (animation_t){
        .type = SOLID_KEY,

        .led_index = led_index,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                animation_color_internal(MAKE_COLOR(COLOR_OFF)),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_solid(uint8_t h, uint8_t s, uint8_t v) {
    return (animation_t){
        .type = SOLID_ALL,

        .led_index = 0,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                animation_color_internal(MAKE_COLOR(h, s, v)),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_solid_key(uint8_t led_index, uint8_t h, uint8_t s, uint8_t v) {
    return (animation_t){
        .type = SOLID_KEY,

        .led_index = led_index,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                animation_color_internal(MAKE_COLOR(h, s, v)),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_wave(uint8_t start_led_index, animation_color_t wave_color) {
    return (animation_t){
        .type = WAVE,

        .led_index = start_led_index,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                wave_color,
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_wave_solid(uint8_t start_led_index, animation_color_t wave_color) {
    return (animation_t){
        .type = WAVE,

        .led_index = start_led_index,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                wave_color,
                wave_color,
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_wave_solid_2(uint8_t start_led_index, animation_color_t wave_color, animation_color_t solid_color) {
    return (animation_t){
        .type = WAVE,

        .led_index = start_led_index,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                wave_color,
                solid_color,
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

animation_t animation_shimmer(void) {
    return (animation_t){
        .type = SHIMMER,

        .led_index = 0,
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = NULL,

        .hsv_colors =
            {
                animation_color_special(ANIMATION_COLOR_SHIMMER),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };
}

void sgv_animation_preinit(void) {
    // Free not needed
    animations = malloc(circular_buffer_type_size);
    circular_buffer_new(animations, circular_buffer_type_size, &circular_buffer_mem, CIRCULAR_BUFFER_BYTE_SIZE, CIRCULAR_BUFFER_ELEMS, CIRCULAR_BUFFER_ELEM_SIZE);

    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            uint8_t led = g_led_config.matrix_co[r][c];
            if (led == NO_LED) {
                continue;
            }
            reverse_led_map[led] = (keymap_point_t){.c = c, .r = r};
        }
    }
}

void sgv_animation_init(void) {
    animation_t scrap;
    while (!empty()) {
        pop(&scrap);
    }

    clear_all_state();

    // L, L, L
    sgv_animation_add_startup_animation(70, 70, 70);
}

bool sgv_animation_update(effect_params_t *params) {
    animation_t  scrap;
    animation_t *current;

    uint8_t it    = 0;
    uint8_t limit = length();

    if (!get_matrix_enabled()) {
        MATRIX_USE_LIMITS(led_min, led_max);
        return matrix_check_finished_leds(led_max);
    }

    clear_calc_state();

    while (it < limit) {
        if ((current = at(it)) == NULL) {
            goto next;
        }

        apply_res_e res = apply_animation(current, it == 0, false);

        switch (res) {
            case APPLY_OK:
                break;
            case BECOME_FIRST:
                [[fallthrough]];
            case APPLY_NEW_BASE:
                while (it != 0) {
                    shift(&scrap);
                    --it;
                    --limit;
                }
                if (res == BECOME_FIRST) {
                    break;
                }
                // Fallthrough to remove the current element too if we're APPLY_NEW_BASE
                [[fallthrough]];
            case APPLY_CLEAR_THIS:
                if (it == 0) {
                    shift(&scrap);
                    --it; // This _could_ underflow but it'll be fixed before next loop
                    --limit;
                }
                break;
            case BECOME_SHIMMER:
                while (it != 0) {
                    shift(&scrap);
                    --it;
                    --limit;
                }
                shift(&scrap);
                scrap.done                                   = false;
                scrap.type                                   = SHIMMER;
                scrap.hsv_colors[ANIMATION_HSV_COLOR_BASE]   = scrap.hsv_colors[ANIMATION_HSV_COLOR_RESULT];
                scrap.hsv_colors[ANIMATION_HSV_COLOR_BASE_N] = scrap.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N];
                scrap.hsv_colors[ANIMATION_HSV_COLOR_RESULT] = scrap.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N] = animation_color_internal(MAKE_COLOR(COLOR_OFF));
                unshift(scrap);
                break;
        }
        /* `it` could be in an undefined bad state here */

    next:
        /* `it` is fixed */
        ++it;
    }

    return apply_calc_state(params);
}

void sgv_animation_add_animation(animation_t animation) {
    if (full()) {
        animation_t scrap;
        if (shift(&scrap)) {
            apply_animation(&scrap, true, true);
        }
    }

    push(animation);
};

static const uint16_t startup_animation_keys[][MATRIX_ROWS][MATRIX_COLS] = {
#if USING_RGB // TODO This is incorrect, and should instead use keyboard layout
    [0] =
        {
            [0] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
            [1] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [2] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [3] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [4] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [5] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
        },
    [1] =
        {
            [0] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            [1] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [2] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [3] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [4] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [5] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        },
#else
    [0] =
        {
            [0] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
            [1] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [2] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [3] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [4] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            [5] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
        },
    [1] =
        {
            [0] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            [1] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [2] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [3] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [4] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0},
            [5] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        }
#endif
};

void sgv_animation_add_startup_animation(uint8_t first_wave_start_led, uint8_t second_wave_start_led, uint8_t cleanup_start_led) {
    uint32_t time = timer_read32();

    animation_t anim;

    // TODO Make animations start after previous one is done?
    anim        = animation_wave_solid(first_wave_start_led, animation_color_special(ANIMATION_COLOR_SHIMMER));
    anim.keymap = &startup_animation_keys[0];
    anim.ticks  = time;
    sgv_animation_add_animation(anim);

    anim                                        = animation_wave_solid(second_wave_start_led, animation_color_special(ANIMATION_COLOR_SHIMMER));
    anim.keymap                                 = &startup_animation_keys[1];
    anim.hsv_colors[ANIMATION_HSV_COLOR_BASE_N] = anim.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N] = animation_color_internal(MAKE_COLOR(COLOR_OFF));
    anim.ticks                                                                                  = time + 800; // TODO Unify time + speed somehow
    sgv_animation_add_animation(anim);

    anim       = animation_wave_solid(cleanup_start_led, animation_color_internal(MAKE_COLOR(COLOR_OFF)));
    anim.ticks = time + 1600;
    sgv_animation_add_animation(anim);

    anim       = animation_wave_solid(cleanup_start_led, animation_color_special(ANIMATION_COLOR_SHIMMER));
    anim.ticks = time + 1800;
    sgv_animation_add_animation(anim);

    anim       = animation_wave_solid(cleanup_start_led, animation_color_internal(MAKE_COLOR(COLOR_OFF)));
    anim.ticks = time + 2000;
    sgv_animation_add_animation(anim);
}

void sgv_animation_reset(void) {
    animation_t scrap;
    while (!empty()) {
        pop(&scrap);
    }

    clear_all_state();
}
