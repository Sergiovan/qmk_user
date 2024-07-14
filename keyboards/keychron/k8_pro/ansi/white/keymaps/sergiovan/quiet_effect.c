#ifndef LED_MATRIX_CUSTOM_EFFECT_IMPLS
// This is here for IDE convenience :)
#include QMK_KEYBOARD_H // clangd does not enjoy macro inclusions so I'm adding this long comment to avoid issues with my IDE
#include "lib/lib8tion/lib8tion.h"
#endif

#include "quiet_effect.h"

static quiet_animation_state_e quiet_animation_state = QUIET_ANIMATION_CLEAR;
static bool quiet_animation_state_changed = false;

void set_animation_state(quiet_animation_state_e state) {
    quiet_animation_state = state;
    quiet_animation_state_changed = true;
}

static void custom_quiet_nice(void) {
    const uint8_t keys[] = {
        // From white.c
        69, // N // Nice
        41, // I
        66, // C
        36, // E
    };

    const uint8_t keys_size = sizeof(keys) / sizeof(uint8_t);

    const uint8_t max_bright = led_matrix_eeconfig.val;
    const uint8_t min_bright = scale8(64, max_bright);
    const uint8_t bright_range = max_bright - min_bright;
    // const uint8_t bright_empty = 0xFF - bright_range;

    const uint16_t time = scale16by8(g_led_timer, led_matrix_eeconfig.speed / 8);

    for (uint8_t i = 0; i < keys_size; ++i) {
        uint8_t offset = quadwave8(time + ((0x100u / keys_size) * (keys_size - (i + 1))));
        offset = scale8(offset, bright_range);
        offset += min_bright;

        led_matrix_set_value(keys[i], offset);
    }
}

static bool custom_quiet(effect_params_t* params) {
    LED_MATRIX_USE_LIMITS(led_min, led_max);

    if (params->init) {
        led_matrix_set_value_all(0);
    }

    switch (quiet_animation_state) {
    case QUIET_ANIMATION_CLEAR:
        if (quiet_animation_state_changed) {
            led_matrix_set_value_all(0);
        }
        break;
    case QUIET_ANIMATION_NICE:
        custom_quiet_nice();
        break;
    default:
        break;
    }

    quiet_animation_state_changed = false;

    return led_matrix_check_finished_leds(led_max);
}
