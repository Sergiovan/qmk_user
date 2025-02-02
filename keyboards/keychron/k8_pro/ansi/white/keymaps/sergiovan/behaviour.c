#include QMK_KEYBOARD_H // This comment is here to avoid clangd creating overly long clickable links

// #include "state_machine.h"
#include "animation/animation.h"
#include "common/common.h"
#include "deferred_exec.h"
#include "state_machine.h"
#include "lib/lib8tion/lib8tion.h"

// #define DEBUG_FUNCTIONS true

#if DEBUG_FUNCTIONS
COLOR debug_leds[10] = {0};
#endif

static state_machine_t state_machine;

uint32_t keep_awake_cb(uint32_t trigger_time, void *cb_arg) {
    // TODO Send key
    const uint16_t codes[] = {
        KC_LALT, KC_LCTL, KC_LSFT, KC_RALT, KC_RCTL, KC_RSFT,
    };

    const uint8_t codes_size = sizeof codes / sizeof(uint16_t);

    tap_code16(codes[random8_max(codes_size)]);

    animation_t anim = animation_solid_key(random8_max(LED_COUNT), random8() & 1 ? 0x80 : 00, 0xFF, 0xFF);
    anim.ticks += (random16() >> 5);
    sgv_animation_add_animation(anim);

    return (random16() >> 1) + 500;
}

void keyboard_post_init_user(void) {
    default_layer_set(1ULL << BASE);
    led_matrix_mode_noeeprom(LED_MATRIX_CUSTOM_sgv_custom_led);
    state_machine_init(&state_machine);
    sgv_animation_preinit();
}

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        return false;
    }
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    static deferred_token keep_awake_token = INVALID_DEFERRED_TOKEN;

    if (keep_awake_token != INVALID_DEFERRED_TOKEN && record->event.pressed) {
        // Cancel idle mode when a key is pressed
        cancel_deferred_exec(keep_awake_token);
        keep_awake_token = INVALID_DEFERRED_TOKEN;
        sgv_animation_add_animation(
            animation_wave_solid(g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                                 animation_color_special(ANIMATION_COLOR_SHIMMER)));

        animation_t anim = animation_wave_solid(g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                                                animation_color_val(0x00));
        anim.ticks += 1000;
        sgv_animation_add_animation(anim);
    }

    if (led_matrix_get_mode() == LED_MATRIX_CUSTOM_sgv_custom_led && record->event.pressed) {
        state_cb_t f = state_machine_advance(&state_machine, keycode);
        if (f) f();
    }

    switch (keycode) {
        case LT(FN, SGV_FN):
            if (record->tap.count && record->event.pressed) {
                // Tapped
                if (record->event.pressed) {
                    set_oneshot_layer(FN, ONESHOT_START);
                    return false;
                }
            }
            break;
        case SGV_FN:
            break;
        case SGV_EFFT:
            if (record->event.pressed) {
                led_matrix_mode(LED_MATRIX_CUSTOM_sgv_custom_led);
            } else {
                clear_oneshot_layer_state(ONESHOT_PRESSED);
            }
            return false;
        case SGV_CAPS:
            if (record->event.pressed) {
                sgv_animation_add_animation(
                    animation_wave(g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                                   animation_color_val((random8() / 2) + 0x80)));
            } else {
                clear_oneshot_layer_state(ONESHOT_PRESSED);
            }
            return false;
        case SGV_PRSC:
            if (record->event.pressed) {
                keep_awake_token = defer_exec(1000, keep_awake_cb, NULL);
            } else {
                clear_oneshot_layer_state(ONESHOT_PRESSED);
            }
            break;
        default:
            if (!record->event.pressed) {
                clear_oneshot_layer_state(ONESHOT_PRESSED);
            }
            break;
    }
    return true;
}

/**
 * @brief Called when the layer is about to be changed
 * Currently plays animations when switching layers to show which keys are
 * mapped
 *
 * @param state The layer about to be entered
 * @return layer_state_t The new layer to set
 */
layer_state_t layer_state_set_user(layer_state_t state) {
    layer_state_t highest = get_highest_layer(state);
    if (highest == BASE) {
        sgv_animation_add_animation(animation_wave_solid(g_led_config.matrix_co[5][12], // FN
                                                         animation_color_val(COLOR_OFF)));
    } else if (highest == FN) {
        animation_t anim                            = animation_wave_solid(g_led_config.matrix_co[5][12], // FN
                                                                           animation_color_val(0xFF));
        anim.keymap                                 = &keymaps[FN];
        anim.hsv_colors[ANIMATION_HSV_COLOR_BASE_N] = anim.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N] =
            animation_color_val(0x10);

        sgv_animation_add_animation(anim);
    } else if (highest == SECRET) {
        layer_state_t current = get_highest_layer(layer_state);

        if (current == FN) {
            animation_t anim = animation_wave_solid(g_led_config.matrix_co[5][12], // FN
                                                    animation_color_special(ANIMATION_COLOR_SHIMMER));

            anim.keymap                                   = &keymaps[SECRET];
            anim.hsv_colors[ANIMATION_HSV_COLOR_BASE_N]   = animation_color_special(ANIMATION_COLOR_RANDOM);
            anim.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N] = animation_color_val(COLOR_OFF);

            sgv_animation_add_animation(anim);
        }
    }

    return state;
}

/**
 * @brief Called when about to shutdown, via soft reset or bootloader reset
 * Currently turns off all LEDs so they don't get stuck
 *
 * @param jump_to_bootloader If this is a jump to bootloader or a soft reset
 * @return true Continue to the default _kb shutdown handler
 * @return false Do not
 */
bool shutdown_user(bool jump_to_bootloader) {
    // Not provided by any header, but explicitly supported (the main example of shutdown_user
    // does something like this)
    extern void led_matrix_update_pwm_buffers(void);

    // sgv_animation_add_animation(animation_clear());

    led_matrix_set_value_all(COLOR_OFF);
    led_matrix_update_pwm_buffers(); // Actually turn the things off please

    return false;
}

#if DEBUG_FUNCTIONS
/**
 * @brief Called every frame to handle LEDs that go over the current animation/mode
 * Currently used for debugging purposes only
 *
 * @param led_min Smallest LED in the current LED handling batch
 * @param led_max Largest LED in the current LED handling batch
 * @return true Continue to the default _kb advanced LED handler
 * @return false Do not
 */
bool led_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (led_matrix_get_mode() != LED_MATRIX_CUSTOM_sgv_custom_led) {
        return false;
    }

    for (uint8_t i = 0; i < 10; ++i) {
        color_t color = debug_leds[i];
        if (i == 0) {
            led_matrix_set_value(30, color.v);
        } else {
            led_matrix_set_value(20 + i, color.v);
        }
    }

    return false;
}
#else
/**
 * @brief Called every frame to handle LEDs that go over the current animation/mode
 * Currently only used to suppress _kb handler
 *
 * @param led_min Smallest LED in the current LED handling batch
 * @param led_max Largest LED in the current LED handling batch
 * @return true Continue to the default _kb advanced LED handler
 * @return false Do not
 */
bool led_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    return false;
}
#endif
