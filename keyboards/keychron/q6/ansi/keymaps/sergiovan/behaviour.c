#include QMK_KEYBOARD_H // A little comment to avoid weird underlines :)
#include "common/common.h"
#include "animation/animation.h"
#include "lib/lib8tion/lib8tion.h"

#if DEBUG_FUNCTIONS
HSV debug_leds[10] = {0};
#endif

/**
 * @brief Called after keyboard is initialized.
 * Sets layer to BASE, RGB mode to my custom mode, and pre-initializes the animation stack
 */
void keyboard_post_init_user(void) {
    default_layer_set(1ULL << BASE);
    rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_sgv_custom_rgb);
    sgv_animation_preinit();
}

/**
 * @brief Called when the switch on the back of the keyboard is switched
 * Currently only disables doing anything with the switch because I don't want it to be used
 *
 * @param index Which switch was touched. This keyboard only has one switch, so it'll always be 0
 * @param active If the switch is now active or inactive
 * @return true Continue with the _kb default dip switch handler
 * @return false Do not
 */
bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        return false;
    }
    return true;
}

/**
 * @brief Called when a key has been activated or a keycode is about to be sent to the computer
 * Handles layer switching and custom keys
 *
 * @param keycode The key code that has been activated
 * @param record Information about the activation
 * @return true Continue with the _kb default record handler
 * @return false Do not
 */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
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
        case SGV_RGB:
            if (record->event.pressed) {
                rgb_matrix_mode(RGB_MATRIX_CUSTOM_sgv_custom_rgb);
            }
            return false;
        case SGV_CLK:
            if (record->event.pressed) {
                sgv_animation_reset();
#if DEBUG_FUNCTIONS
                for (int i = 0; i < 10; ++i) {
                    LED_DEBUG(i, RGB_OFF);
                }
#endif
            }
            break;
        case SGV_RCKT:
            if (record->event.pressed) {
                sgv_animation_add_startup_animation(70, 70, 70);
            }
            break;
        case SGV_SPRK:
            if (record->event.pressed) {
                sgv_animation_add_animation(animation_wave_solid_2(
                    g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                    animation_color_hsv(random8(), 255, 255), animation_color_special(ANIMATION_COLOR_SHIMMER)));
            }
            break;
        case SGV_ERTH:
            if (record->event.pressed) {
                sgv_animation_add_animation(
                    animation_wave(g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                                   animation_color_hsv(random8(), 0xFF, 0xFF)));
            }
            break;
        case SGV_SPDE:
            if (record->event.pressed) {
                sgv_animation_add_animation(
                    animation_wave_solid(g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                                         animation_color_hsv(RGB_OFF)));
            }
            break;
        case SGV_CAPS:
            if (record->event.pressed) {
                sgv_animation_add_animation(
                    animation_wave(g_led_config.matrix_co[record->event.key.row][record->event.key.col],
                                   animation_color_hsv(random8(), 0xFF, 0xFF)));
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
 * Currently plays animations when switching layers to show which keys are mapped
 *
 * @param state The layer about to be entered
 * @return layer_state_t The new layer to set
 */
layer_state_t layer_state_set_user(layer_state_t state) {
    layer_state_t highest = get_highest_layer(state);
    if (highest == BASE) {
        sgv_animation_add_animation(animation_wave_solid(g_led_config.matrix_co[5][12], // FN
                                                         animation_color_hsv(RGB_OFF)));
    } else if (highest == FN) {
        animation_t anim                            = animation_wave_solid(g_led_config.matrix_co[5][12], // FN
                                                                           animation_color_hsv(HSV_RED));
        anim.keymap                                 = &keymaps[FN];
        anim.hsv_colors[ANIMATION_HSV_COLOR_BASE_N] = anim.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N] =
            animation_color_hsv(HSV_BLUE);

        sgv_animation_add_animation(anim);
    } else if (highest == SECRET) {
        layer_state_t current = get_highest_layer(layer_state);

        if (current == FN) {
            animation_t anim = animation_wave_solid(g_led_config.matrix_co[5][12], // FN
                                                    animation_color_special(ANIMATION_COLOR_SHIMMER));

            anim.keymap                                   = &keymaps[SECRET];
            anim.hsv_colors[ANIMATION_HSV_COLOR_BASE_N]   = animation_color_special(ANIMATION_COLOR_RANDOM);
            anim.hsv_colors[ANIMATION_HSV_COLOR_RESULT_N] = animation_color_hsv(RGB_OFF);

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
    extern void rgb_matrix_update_pwm_buffers(void);

    sgv_animation_add_animation(animation_clear());

    rgb_matrix_set_color_all(RGB_OFF);
    rgb_matrix_update_pwm_buffers(); // Actually turn the things off please

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
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_sgv_custom_rgb) {
        return false;
    }

    rgb_matrix_set_color(g_led_config.matrix_co[5][18], 0xFF, 0xFF, 0xFF);

    switch (get_highest_layer(layer_state)) {
        case BASE:
            rgb_matrix_set_color(g_led_config.matrix_co[5][17], 0xFF, 0, 0);
            break;
        case FN:
            rgb_matrix_set_color(g_led_config.matrix_co[5][17], 0, 0xFF, 0);
            break;
        case SECRET:
            rgb_matrix_set_color(g_led_config.matrix_co[5][17], 0, 0, 0xFF);
            break;
        default:
            rgb_matrix_set_color(g_led_config.matrix_co[5][17], 0xFF, 0, 0xFF);
            break;
    }

    for (uint8_t i = 0; i < 10; ++i) {
        RGB color = hsv_to_rgb(debug_leds[i]);
        if (i == 0) {
            rgb_matrix_set_color(30, color.r, color.g, color.b);
        } else {
            rgb_matrix_set_color(20 + i, color.r, color.g, color.b);
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
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    return false;
}
#endif
