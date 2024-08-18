#include QMK_KEYBOARD_H // A little comment to avoid weird underlines :)
#include "common.h"

#define DEBUG_FUNCTIONS false

void keyboard_post_init_user(void) {
    default_layer_set(1ULL << BASE);
    rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_sgv_custom_rgb);
}

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        return false;
    }
    return true;
}

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
        default:
            if (!record->event.pressed) {
                clear_oneshot_layer_state(ONESHOT_PRESSED);
            }
            break;
    }

    return true;
}

#if DEBUG_FUNCTIONS
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    rgb_matrix_set_color(g_led_config.matrix_co[5][18], 0xFF * rgb_matrix_is_enabled(), 0xFF * rgb_matrix_is_enabled(), rgb_matrix_get_val());
    if (rgb_matrix_get_mode() == RGB_MATRIX_NONE) {
        return false;
    }

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
    return false;
}
#else
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    return false;
}
#endif
