#include QMK_KEYBOARD_H // A little comment to avoid weird underlines :)
#include "common.h"
#include "animation.h"
#include "lib/lib8tion/lib8tion.h"

#if DEBUG_FUNCTIONS
HSV debug_leds[10] = {0};
#endif

void keyboard_post_init_user(void) {
    default_layer_set(1ULL << BASE);
    rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_sgv_custom_rgb);
    sgv_animation_preinit();
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
        case SGV_TEST_SINGLE:
            if (record->event.pressed) {
                sgv_animation_add_animation((animation_t){
                    .type         = WAVE,
                    .payload_type = PAYLOAD_COLOR_HSV,

                    .key_index = g_led_config.matrix_co[record->event.key.row][record->event.key.col], // Earth
                    .ticks     = timer_read(),
                    .done      = false,

                    .hsv_color = (HSV){random8(), 255, 255}});
            }
            break;
        case SGV_TEST_SOLID:
            if (record->event.pressed) {
                sgv_animation_add_animation((animation_t){
                    .type         = WAVE_SOLID,
                    .payload_type = PAYLOAD_COLOR_HSV,

                    .key_index = g_led_config.matrix_co[record->event.key.row][record->event.key.col], // Sparkles
                    .ticks     = timer_read(),
                    .done      = false,

                    .hsv_color = (HSV){random8(), 255, 255}});
            }
            break;
        case SGV_TEST_WAVE:
            if (record->event.pressed) {
                sgv_animation_add_animation((animation_t){
                    .type         = WAVE_SOLID_2,
                    .payload_type = PAYLOAD_COLOR_HSV,

                    .key_index = g_led_config.matrix_co[record->event.key.row][record->event.key.col], // Rocket
                    .ticks     = timer_read(),
                    .done      = false,

                    .hsv_color_arr = {(HSV){random8(), 255, 255}, (HSV){random8(), 255, 255}}});
            }
            break;
        case SGV_TEST_CLEAR:
            if (record->event.pressed) {
                sgv_animation_add_animation((animation_t){
                    .type         = WAVE_SOLID,
                    .payload_type = PAYLOAD_COLOR_HSV,

                    .key_index = g_led_config.matrix_co[record->event.key.row][record->event.key.col], // Pica
                    .ticks     = timer_read(),
                    .done      = false,

                    .hsv_color = (HSV){0, 0, 0}});
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

#if DEBUG_FUNCTIONS
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_sgv_custom_rgb) {
        return false;
    }

    rgb_matrix_set_color(g_led_config.matrix_co[5][18], 0xFF * rgb_matrix_is_enabled(), 0xFF * rgb_matrix_is_enabled(),
                         rgb_matrix_get_val());

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
        if (i >= led_min && i < led_max && debug_leds[i].v) {
            RGB color = hsv_to_rgb(debug_leds[i]);
            rgb_matrix_set_color(g_led_config.matrix_co[1][1 + i], color.r, color.g, color.b);
        }
    }

    return false;
}
#else
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    return false;
}
#endif
