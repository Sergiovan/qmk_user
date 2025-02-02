#pragma once
#include QMK_KEYBOARD_H // IWYU pragma: export // A little comment to avoid weird underlines :)

#define DEBUG_FUNCTIONS false

#if LED_MATRIX_ENABLE
typedef union color {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} color_t;

#    define USING_RGB false
#    define COLOR color_t
#    define MAKE_COLOR(x, ...) \
        (COLOR) {              \
            x                  \
        }
#    define COLOR_OFF 0x0
#    define LED_COUNT LED_MATRIX_LED_COUNT
#    define MATRIX_USE_LIMITS LED_MATRIX_USE_LIMITS
#    define MATRIX_TEST_LED_FLAGS LED_MATRIX_TEST_LED_FLAGS
#elif RGB_MATRIX_ENABLE
#    define USING_RGB true
#    define COLOR HSV
#    define MAKE_COLOR(...) \
        (COLOR) {           \
            __VA_ARGS__     \
        }
#    define COLOR_OFF RGB_OFF
#    define LED_COUNT RGB_MATRIX_LED_COUNT
#    define MATRIX_USE_LIMITS RGB_MATRIX_USE_LIMITS
#    define MATRIX_TEST_LED_FLAGS RGB_MATRIX_TEST_LED_FLAGS
#else
#    define USING_RGB false
#    define COLOR
#    define MAKE_COLOR(...)
#    define COLOR_OFF 0x0
#    define LED_COUNT 0
#    define MATRIX_USE_LIMITS
#    define MATRIX_TEST_LED_FLAGS
#endif

#if DEBUG_FUNCTIONS

extern COLOR debug_leds[10];

#    define LED_DEBUG(x, ...) debug_leds[x] = MAKE_COLOR(__VA_ARGS__)

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
    SGV_CAPS, /* Custom keycode that goes on capslock */

    SGV_COUNT, /* Layer count enum value for convenience */
};
