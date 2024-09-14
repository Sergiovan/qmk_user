#include "animation.h"
#include "circular_buffer/circular_buffer.h"
#include "lib/lib8tion/lib8tion.h"

#include <stdlib.h>

#define LED_COUNT SNLED27351_LED_COUNT

/* Circular buffer boilerplate */

#define CIRCULAR_BUFFER_ELEM_SIZE sizeof(animation_t)
#define CIRCULAR_BUFFER_ELEMS 16
#define CIRCULAR_BUFFER_BYTE_SIZE (CIRCULAR_BUFFER_ELEM_SIZE * CIRCULAR_BUFFER_ELEMS)

#define WAVE_THICKNESS 23
#define WAVE_THICKNESS_FACTOR ((255 / 23) + 1)

static uint8_t            circular_buffer_mem[CIRCULAR_BUFFER_BYTE_SIZE] = {0};
static circular_buffer_t *animations                                     = NULL;

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

typedef enum apply_res : uint8_t { APPLY_OK, APPLY_CLEAR_THIS, APPLY_NEW_BASE } apply_res_e;

void clear_all_state(void) {
    memset(calc_state, 0, sizeof calc_state);
    memset(base_state, 0, sizeof base_state);
}

void set_all_state(HSV hsv) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        calc_state[i] = hsv;
        base_state[i] = hsv;
    }
}

void clear_calc_state(void) {
    memcpy(&calc_state, &base_state, sizeof base_state);
}

static inline RGB animation_get_rgb(animation_t *animation) {
    switch (animation->payload_type) {
        case PAYLOAD_COLOR_RGB:
            return animation->rgb_color;
        case PAYLOAD_COLOR_HSV:
            return hsv_to_rgb(animation->hsv_color);
        case PAYLOAD_COLOR_DEFAULT:
            return hsv_to_rgb(rgb_matrix_get_hsv());
        case PAYLOAD_NONE:
        default:
            return (RGB){0, 0, 0};
    }
}

static inline HSV animation_get_hsv(animation_t *animation) {
    switch (animation->payload_type) {
        case PAYLOAD_COLOR_HSV:
            return animation->hsv_color;
        case PAYLOAD_COLOR_DEFAULT:
            return rgb_matrix_get_hsv();
        case PAYLOAD_NONE:
        case PAYLOAD_COLOR_RGB: // There's no conversion function???? // TODO
        default:
            return (HSV){0, 0, 0};
    }
}

apply_res_e apply_animation(animation_t *animation, bool first) {
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
            calc_state[animation->key_index] = animation_get_hsv(animation);
            if (first) {
                base_state[animation->key_index] = animation_get_hsv(animation);
                return APPLY_CLEAR_THIS;
            } else {
                return APPLY_OK;
            }
        case SOLID_ALL:
            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                set_all_state(animation_get_hsv(animation));
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

            uint32_t active_for = timer_elapsed(animation->ticks);
            active_for          = scale16by8((uint16_t)active_for, rgb_matrix_get_speed()) << 16;

            if (active_for >= wave_time_ms) {
                animation->done = true;
                if (animation->type == WAVE_SOLID || animation->type == WAVE_SOLID_2) {
                    for (uint8_t i = 0; i < LED_COUNT; ++i) {
                        set_all_state(animation->type == WAVE_SOLID ? animation_get_hsv(animation)
                                                                    : animation->hsv_color_arr[1]);
                    }
                    return APPLY_NEW_BASE;
                } else {
                    return first ? APPLY_CLEAR_THIS : APPLY_OK;
                }
            }

            uint8_t orig_x = g_led_config.point[animation->key_index].x;
            uint8_t orig_y = g_led_config.point[animation->key_index].y;
            // u32q16
            uint32_t radius = wave_time_ms - (wave_time_ms - active_for);

            // Result of multiplication is u64q32
            // but we bring it back to u32q16
            // Cutting off 4 bytes at the top is ok since (400 << 16) * 0xA333 is only
            // 0xfeffb00000
            radius         = ((uint64_t)radius * u32q16_255_400) >> 32;
            HSV target_col = animation_get_hsv(animation);

            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                HSV current_col = calc_state[i];

                uint8_t val  = 0;
                int16_t dx   = g_led_config.point[i].x - orig_x;
                int16_t dy   = g_led_config.point[i].y - orig_y;
                int16_t dist = sqrt16(dx * dx + dy * dy);
                /// Distance to wave
                uint8_t diff = labs(dist - (int64_t)radius);

                if (diff < WAVE_THICKNESS) {
                    val = 0xFF - ease8InOutQuad(qmul8(diff, WAVE_THICKNESS_FACTOR));

                    if (radius > dist) {
                        if (animation->type == WAVE_SOLID) {
                            current_col = target_col;
                        } else if (animation->type == WAVE_SOLID_2) {
                            current_col = animation->hsv_color_arr[1];
                        }
                    }
                } else {
                    if (radius > dist) {
                        if (animation->type == WAVE_SOLID) {
                            calc_state[i] = target_col;
                        } else if (animation->type == WAVE_SOLID_2) {
                            calc_state[i] = animation->hsv_color_arr[1];
                        }
                    }
                    continue;
                }

                if (current_col.v == 0 && target_col.v == 0) {
                    // If the start and end colors are OFF, then just keep the key off
                    calc_state[i].v = 0;
                } else if (current_col.v == 0) {
                    // If the start color is OFF, just wake up the color slowly
                    calc_state[i]   = target_col;
                    calc_state[i].v = map8(val, 0, target_col.v);
                } else if (target_col.v == 0) {
                    // If the end color is OFF, just turn off the color slowly
                    calc_state[i]   = current_col;
                    calc_state[i].v = map8(255 - val, 0, current_col.v);
                } else {
                    if (current_col.h < target_col.h) {
                        calc_state[i].h = map8(val, current_col.h, target_col.h);
                    } else {
                        calc_state[i].h = map8(255 - val, target_col.h, current_col.h);
                    }

                    if (current_col.s < target_col.s) {
                        calc_state[i].s = map8(val, current_col.s, target_col.s);
                    } else {
                        calc_state[i].s = map8(255 - val, target_col.s, current_col.s);
                    }

                    if (current_col.v < target_col.v) {
                        calc_state[i].v = map8(val, current_col.v, target_col.v);
                    } else {
                        calc_state[i].v = map8(255 - val, target_col.v, current_col.v);
                    }
                }
            }

            return APPLY_OK;
        }
        default:
            return APPLY_OK;
    }
}

apply_res_e apply_finished_animation(animation_t *animation, bool first) {
    switch (animation->type) {
        case CLEAR_KEY:
        case CLEAR_ALL:
        case SOLID_KEY:
        case SOLID_ALL:
        default:
            return apply_animation(animation, first);
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

        apply_res_e res = apply_animation(current, it == 0);

        switch (res) {
            case APPLY_OK:
                break;
            case APPLY_NEW_BASE:
                while (it != 0) {
                    shift(&scrap);
                    --it;
                    --limit;
                }
                [[fallthrough]]; // Fallthrough to remove the current element too
            case APPLY_CLEAR_THIS:
                shift(&scrap);
                --it; // This _could_ underflow but it'll be fixed before next loop
                --limit;
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
            apply_finished_animation(&scrap, true);
        }
    }

    push(animation);
};
