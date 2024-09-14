#pragma once
#include QMK_KEYBOARD_H // A little comment to avoid weird underlines :)

#define DEBUG_FUNCTIONS false

#if DEBUG_FUNCTIONS

extern HSV debug_leds[10];

#    define LED_DEBUG(x, ...)   \
        debug_leds[x] = (HSV) { \
            __VA_ARGS__         \
        }

#else

#    define LED_DEBUG(...)

#endif

enum layers { BASE, FN, SECRET, LAYER_COUNT };

extern const uint16_t PROGMEM keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

enum custom_keycodes {
    SGV_FN = SAFE_RANGE,
    SGV_RGB,

    SGV_TEST_WAVE,
    SGV_TEST_SOLID,
    SGV_TEST_SINGLE,
    SGV_TEST_CLEAR,

    SGV_COUNT
};
