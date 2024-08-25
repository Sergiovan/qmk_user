#include "animation.h"
#include "circular_buffer/circular_buffer.h"

#define LED_COUNT SNLED27351_LED_COUNT

/* Circular buffer boilerplate */

#define CIRCULAR_BUFFER_ELEM_SIZE sizeof(animation_t)
#define CIRCULAR_BUFFER_ELEMS 16
#define CIRCULAR_BUFFER_BYTE_SIZE (CIRCULAR_BUFFER_ELEM_SIZE * CIRCULAR_BUFFER_ELEMS)

static uint8_t            circular_buffer_mem[CIRCULAR_BUFFER_BYTE_SIZE] = {0};
static circular_buffer_t* animations                                     = NULL;

static bool push(animation_t animation) {
    return circular_buffer_push(animations, &animation);
}

static animation_t* pop(animation_t* animation) {
    return circular_buffer_pop(animations, animation);
}

static bool unshift(animation_t animation) {
    return circular_buffer_unshift(animations, &animation);
}

static animation_t* shift(animation_t* animation) {
    return circular_buffer_shift(animations, &animation);
}

static animation_t* at(uint8_t i) {
    return circular_buffer_at(animations, i);
}

static bool empty(void) {
    return circular_buffer_empty(animations);
}

static uint8_t length(void) {
    return circular_buffer_empty(animations);
}

static bool full(void) {
    return circular_buffer_full(animations);
}

/* Private functions */

static uint32_t last_tick             = 0;
static RGB      base_state[LED_COUNT] = {{0}};
static RGB      calc_state[LED_COUNT] = {{0}};

typedef enum apply_res : uint8_t { APPLY_OK, APPLY_CLEAR_THIS, APPLY_NEW_BASE } apply_res_e;

void clear_calc_state(void) {
    memcpy(&calc_state, &base_state, sizeof base_state);
}

apply_res_e apply_animation(animation_t* animation, bool first) {
    RGB color;
    switch (animation->payload_type) {
        case PAYLOAD_NONE:
            break;
        case PAYLOAD_COLOR_RGB:
            color = animation->rgb_color;
            break;
        case PAYLOAD_COLOR_HSV:
            color = hsv_to_rgb(animation->hsv_color);
            break;
        case PAYLOAD_COLOR_DEFAULT:
            color = hsv_to_rgb(rgb_matrix_config.hsv);
            break;
    }

    switch (animation->type) {
        case CLEAR_KEY:
            calc_state[animation->key_index] = (RGB){0, 0, 0};
            if (first) {
                base_state[animation->key_index] = (RGB){0, 0, 0};
                return APPLY_CLEAR_THIS;
            } else {
                return APPLY_OK;
            }
        case CLEAR_ALL:
            memset(calc_state, 0, sizeof calc_state);
            memset(base_state, 0, sizeof base_state);
            return APPLY_NEW_BASE;
        case SOLID_KEY:
            calc_state[animation->key_index] = color;
            if (first) {
                base_state[animation->key_index] = color;
                return APPLY_CLEAR_THIS;
            } else {
                return APPLY_OK;
            }
        case SOLID_ALL:
            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                calc_state[i] = color;
                base_state[i] = color;
            }
            return APPLY_NEW_BASE;
        default:
            return APPLY_OK;
    }
}

apply_res_e apply_finished_animation(animation_t* animation, bool first) {
    switch (animation->type) {
        case CLEAR_KEY:
        case CLEAR_ALL:
        case SOLID_KEY:
        case SOLID_ALL:
        default:
            return apply_animation(animation, first);
    }
}

void apply_calc_state(void) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        rgb_matrix_set_color(i, calc_state[i].r, calc_state[i].g, calc_state[i].b);
    }
}

/* Public functions */

void sgv_animation_init(void) {
    // Free not needed
    animations = malloc(circular_buffer_type_size);
    circular_buffer_new(animations, circular_buffer_type_size, &circular_buffer_mem, CIRCULAR_BUFFER_BYTE_SIZE,
                        CIRCULAR_BUFFER_ELEMS, CIRCULAR_BUFFER_ELEM_SIZE);
    last_tick = timer_read32();
}

void sgv_animation_update(void) {
    animation_t  scrap;
    animation_t* current;

    last_tick     = timer_read32();
    uint8_t it    = 0;
    uint8_t limit = length();

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

    apply_calc_state();
}

void sgv_animation_add_animation(animation_t animation) {};
