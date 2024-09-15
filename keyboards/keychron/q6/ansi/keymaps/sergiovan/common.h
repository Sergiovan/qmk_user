#pragma once
#include QMK_KEYBOARD_H // IWYU pragma: export // A little comment to avoid weird underlines :)

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

// TODO Add q notation typedefs

/**
 * @brief Keyboard layers enum
 */
enum layers {
    BASE,   /* Base layer, when no other layer is active */
    FN,     /* Function layer, accessed via the FN key */
    SECRET, /* Secret layer, accessed by tapping the FN key, then holding it after */

    LAYER_COUNT, /* Layer count enum value for convenience */
};

/**
 * @brief Keyboard mapping : Layer => Row => Column => Keycode
 */
extern const uint16_t PROGMEM keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

/**
 * @brief Custom keycodes to handle miscelaneous tasks
 */
enum custom_keycodes {
    SGV_FN = SAFE_RANGE, /* Special FN functionality that allows me to tap twice for secret layer */
    SGV_RGB,             /* Set RGB mode to mine */
    SGV_CLK,             /* Clear entire animation stack */

    SGV_SPRK, /* Custom keycode that goes on the sparkles key */
    SGV_RCKT, /* Custom keycode that goes on the rocket key */
    SGV_ERTH, /* Custom keycode that goes on the earth key */
    SGV_SPDE, /* Custom keycode that goes on the spade key */

    SGV_COUNT, /* Layer count enum value for convenience */
};
