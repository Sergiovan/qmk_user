#include "animation.h"
#include "circular_buffer/circular_buffer.h"
#include "lib/lib8tion/lib8tion.h"

#include "noise/noise_gen.h"

#include <stdlib.h>

#define LED_COUNT SNLED27351_LED_COUNT

/* Circular buffer boilerplate */

#define CIRCULAR_BUFFER_ELEM_SIZE sizeof(animation_t)
#define CIRCULAR_BUFFER_ELEMS 16
#define CIRCULAR_BUFFER_BYTE_SIZE (CIRCULAR_BUFFER_ELEM_SIZE * CIRCULAR_BUFFER_ELEMS)

#define WAVE_THICKNESS 23
#define WAVE_THICKNESS_FACTOR ((255 / 23) + 1)

typedef struct keymap_point {
    uint8_t r;
    uint8_t c;
} keymap_point_t;

static uint8_t            circular_buffer_mem[CIRCULAR_BUFFER_BYTE_SIZE] = {0};
static circular_buffer_t *animations                                     = NULL;
static keymap_point_t     reverse_led_map[LED_COUNT]                     = {0};

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

static uint32_t last_tick             = 0;
static HSV      base_state[LED_COUNT] = {{0}};
static HSV      calc_state[LED_COUNT] = {{0}};

typedef enum apply_res : uint8_t {
    APPLY_OK,         // Apply
    APPLY_CLEAR_THIS, // Apply and remove this element from the buffer
    APPLY_NEW_BASE,   // Apply and remove all elements before this one from the buffer
    BECOME_FIRST,     // Remove all other previous elements from the buffer
    BECOME_SHIMMER,   // Remove this element from the buffer if it's the first one, and convert animation into shimmer
} apply_res_e;

static inline void clear_all_state(void) {
    memset(calc_state, 0, sizeof calc_state);
    memset(base_state, 0, sizeof base_state);
}

static inline void set_all_state(HSV hsv) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        calc_state[i] = hsv;
        base_state[i] = hsv;
    }
}

static inline void clear_calc_state(void) {
    memcpy(&calc_state, &base_state, sizeof base_state);
}

static inline bool hsv_is_black(HSV *color) {
    return color->v == 0 && color->s == 0;
}

static inline uint8_t get_perlin(uint8_t x, uint8_t y, uint32_t t) {
    // 0x666 is ~0.025 in u32q16

    uint32_t slow   = ((t * 282) >> 8);
    uint32_t medium = ((t * 320) >> 8);
    uint32_t fast   = ((t * 384) >> 8);

    uint16_t perlin_1 = perlin2d_fixed(x + slow, y + fast, 0x666);
    uint16_t perlin_2 = perlin2d_fixed((224 - x) + medium, y + slow, 0x666);
    uint16_t perlin_3 = perlin2d_fixed(x + fast, (64 - y) + medium, 0x666);

    return (perlin_1 + perlin_2 + perlin_3) & 0xFF;
}

static inline bool animation_key_in_keymap(animation_t *animation, uint8_t led) {
    if (animation->keymap != NULL) {
        keymap_point_t key = reverse_led_map[led];
        if ((*animation->keymap)[key.r][key.c] <= KC_TRANSPARENT) {
            return false;
        }
    }
    return true;
}

static inline HSV animation_get_hsv_indexed(animation_t *animation, uint8_t led, [[maybe_unused]] uint8_t index) {
    if (!animation_key_in_keymap(animation, led)) {
        return calc_state[led];
    }

    switch (animation->payload_type) {
        case PAYLOAD_COLOR_HSV:
            return animation->hsv_color;
        case PAYLOAD_COLOR_HSV_2:
            return animation->hsv_color_arr[index];
        case PAYLOAD_COLOR_DEFAULT:
            return rgb_matrix_get_hsv();
        case PAYLOAD_COLOR_RANDOM:
            return (HSV){random8(), 0xFF, 0xFF};
        case PAYLOAD_COLOR_RANDOM_NOISE: {
            uint8_t x = g_led_config.point[led].x;
            uint8_t y = g_led_config.point[led].y;

            uint8_t hue = get_perlin(x, y, animation->ticks << 16);

            return (HSV){hue, 0xFF, 0xFF};
        }
        case PAYLOAD_COLOR_SHIMMER: {
        shimmer_color:;
            uint8_t x = g_led_config.point[led].x;
            uint8_t y = g_led_config.point[led].y;

            uint32_t active_for = timer_elapsed32(animation->ticks);
            active_for /= (uint16_t)rgb_matrix_get_speed() + 1;

            uint8_t hue = get_perlin(x, y, active_for);

            return (HSV){.h = hue, .s = 0xFF, .v = 0xFF};
        }
        case PAYLOAD_COLOR_SHIMMER_FIRST:
            if (index == 0) {
                goto shimmer_color;
            } else {
                return animation->hsv_color;
            }
        case PAYLOAD_COLOR_SHIMMER_SECOND:
            if (index == 1) {
                goto shimmer_color;
            } else {
                return animation->hsv_color;
            }
        case PAYLOAD_NONE:
        default:
            return (HSV){0, 0, 0};
    }
}

static inline HSV animation_get_hsv(animation_t *animation, [[maybe_unused]] uint8_t led) {
    return animation_get_hsv_indexed(animation, led, 0);
}

typedef struct wave_info {
    uint8_t val;
    bool    in_wave;
    bool    past_radius;
} wave_info_t;

static inline wave_info_t animation_wave_get_key_value(animation_t *animation, uint8_t led, int64_t wave_radius) {
    uint8_t orig_x = g_led_config.point[animation->key_index].x;
    uint8_t orig_y = g_led_config.point[animation->key_index].y;

    uint8_t val  = 0;
    int16_t dx   = g_led_config.point[led].x - orig_x;
    int16_t dy   = g_led_config.point[led].y - orig_y;
    int16_t dist = sqrt16(dx * dx + dy * dy);
    /// Distance to wave
    uint8_t diff = llabs(dist - wave_radius);

    bool in_wave = diff < WAVE_THICKNESS;
    val          = in_wave ? 0xFF - ease8InOutQuad(qmul8(diff, WAVE_THICKNESS_FACTOR)) : 0;

    return (wave_info_t){
        .val         = val,
        .in_wave     = in_wave,
        .past_radius = wave_radius > dist,
    };
}

apply_res_e apply_animation(animation_t *animation, bool first, bool finish) {
    if (animation->done) {
        return first ? APPLY_CLEAR_THIS : APPLY_OK;
    }

    switch (animation->type) {
        case CLEAR_KEY:
            calc_state[animation->key_index] = (HSV){0, 0, 0};
            if (first) {
                base_state[animation->key_index] = (HSV){0, 0, 0};
                return APPLY_CLEAR_THIS;
            } else {
                return APPLY_OK;
            }
        case CLEAR_ALL:
            clear_all_state();
            return APPLY_NEW_BASE;
        case SOLID_KEY:
            calc_state[animation->key_index] = animation_get_hsv(animation, animation->key_index);
            if (first) {
                base_state[animation->key_index] = animation_get_hsv(animation, animation->key_index);
                return APPLY_CLEAR_THIS;
            } else {
                return APPLY_OK; // IWYU pragma: export
            }
        case SOLID_ALL:
            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                HSV new_c     = animation_get_hsv(animation, i);
                calc_state[i] = new_c;
                base_state[i] = new_c;
            }
            return APPLY_NEW_BASE;
        case WAVE_SOLID:
            [[fallthrough]];
        case WAVE_SOLID_2:
            [[fallthrough]];
        case WAVE: {
            // u32q16
            const uint32_t wave_time_ms   = 400 << 16; // It takes 400 MS at most
            const uint32_t u32q16_255_400 = 0xa333;    // 255 / 400 in u32q16

            uint32_t active_for = timer_elapsed32(animation->ticks);
            active_for          = scale16by8((uint16_t)active_for, rgb_matrix_get_speed()) << 16;

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
                HSV current_col = calc_state[i];

                HSV         target_col = animation_get_hsv(animation, i);
                wave_info_t info       = animation_wave_get_key_value(animation, i, radius);

                if (info.in_wave) {
                    any_left = true;
                    if (info.past_radius) {
                        if (animation->type == WAVE_SOLID) {
                            current_col = target_col;
                        } else if (animation->type == WAVE_SOLID_2) {
                            current_col = animation_get_hsv_indexed(animation, i, 1);
                        }
                    }
                } else {
                    if (info.past_radius) {
                        if (animation->type == WAVE_SOLID) {
                            calc_state[i] = target_col;
                        } else if (animation->type == WAVE_SOLID_2) {
                            calc_state[i] = animation_get_hsv_indexed(animation, i, 1);
                        }
                    } else {
                        any_left = true;
                    }
                    continue;
                }

                bool current_black = hsv_is_black(&current_col);
                bool target_black  = hsv_is_black(&target_col);

                if (current_black && target_black) {
                    // If the start and end colors are OFF, then just keep the key off
                    calc_state[i].v = 0;
                } else if (current_black) {
                    // If the start color is OFF, just wake up the color slowly
                    calc_state[i]   = target_col;
                    calc_state[i].v = map8(info.val, 0, target_col.v);
                } else if (target_black) {
                    // If the end color is OFF, just turn off the color slowly
                    calc_state[i]   = current_col;
                    calc_state[i].v = map8(255 - info.val, 0, current_col.v);
                } else {
                    if (current_col.h < target_col.h) {
                        calc_state[i].h = map8(info.val, current_col.h, target_col.h);
                    } else {
                        calc_state[i].h = map8(255 - info.val, target_col.h, current_col.h);
                    }

                    if (current_col.s < target_col.s) {
                        calc_state[i].s = map8(info.val, current_col.s, target_col.s);
                    } else {
                        calc_state[i].s = map8(255 - info.val, target_col.s, current_col.s);
                    }

                    if (current_col.v < target_col.v) {
                        calc_state[i].v = map8(info.val, current_col.v, target_col.v);
                    } else {
                        calc_state[i].v = map8(255 - info.val, target_col.v, current_col.v);
                    }
                }
            }

            if (!any_left || active_for == wave_time_ms) {
                animation->done = true;
                if (animation->type == WAVE_SOLID || animation->type == WAVE_SOLID_2) {
                    if (animation->payload_type == PAYLOAD_COLOR_SHIMMER ||
                        animation->payload_type == PAYLOAD_COLOR_SHIMMER_SECOND) {
                        return BECOME_SHIMMER;
                    } else {
                        for (uint8_t i = 0; i < LED_COUNT; ++i) {
                            HSV new_c     = animation_get_hsv_indexed(animation, i, 1);
                            calc_state[i] = new_c;
                            base_state[i] = new_c;
                        }
                        return APPLY_NEW_BASE;
                    }
                } else {
                    return first ? APPLY_CLEAR_THIS : APPLY_OK;
                }
            }

            return APPLY_OK;
        }
        case SHIMMER: {
            uint32_t active_for = timer_elapsed32(animation->ticks);
            active_for /= (uint16_t)rgb_matrix_get_speed() + 1;

            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                if (animation_key_in_keymap(animation, i)) {
                    uint8_t x = g_led_config.point[i].x;
                    uint8_t y = g_led_config.point[i].y;

                    uint8_t hue = get_perlin(x, y, active_for);

                    calc_state[i] = (HSV){.h = hue, .s = 0xFF, .v = 0xFF};
                }
            }
            return APPLY_OK;
        }
        default:
            return APPLY_OK;
    }
}

bool apply_calc_state(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    for (uint8_t i = led_min; i < led_max; ++i) {
        RGB_MATRIX_TEST_LED_FLAGS();

        HSV hsv   = calc_state[i];
        hsv.v     = scale8(hsv.v, rgb_matrix_get_val());
        RGB color = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, color.r, color.g, color.b);
    }

    return rgb_matrix_check_finished_leds(led_max);
}

/* Public functions */

void sgv_animation_preinit(void) {
    // Free not needed
    animations = malloc(circular_buffer_type_size);
    circular_buffer_new(animations, circular_buffer_type_size, &circular_buffer_mem, CIRCULAR_BUFFER_BYTE_SIZE,
                        CIRCULAR_BUFFER_ELEMS, CIRCULAR_BUFFER_ELEM_SIZE);
    last_tick = timer_read32();

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
}

bool sgv_animation_update(effect_params_t *params) {
    animation_t  scrap;
    animation_t *current;

    last_tick     = timer_read32();
    uint8_t it    = 0;
    uint8_t limit = length();

    if (!rgb_matrix_is_enabled()) {
        RGB_MATRIX_USE_LIMITS(led_min, led_max);
        return rgb_matrix_check_finished_leds(led_max);
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
                shift(&scrap);
                --it; // This _could_ underflow but it'll be fixed before next loop
                --limit;
                break;
            case BECOME_SHIMMER:
                while (it != 0) {
                    shift(&scrap);
                    --it;
                    --limit;
                }
                shift(&scrap);
                scrap.done = false;
                scrap.type = SHIMMER;
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
