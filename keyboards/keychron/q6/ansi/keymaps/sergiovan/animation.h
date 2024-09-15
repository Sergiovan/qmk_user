#pragma once
#include "common.h"

/**
 * @brief Animation type, outlining the general animation behaviour
 */
typedef enum animation_type : uint8_t {
    /* Animation types that use 1 color only */
    SOLID_KEY, /* Set one key to a solid color */
    SOLID_ALL, /* Set all keys to a solid color */
    SHIMMER,   /* Randomly changing colors */

    /* Animation types that use 2 colors */
    WAVE, /* Wave that changes to another color */
} animation_type_e;

/**
 * @brief Special color type inside an animation
 */
typedef enum animation_color_special : uint8_t {
    ANIMATION_COLOR_NONE,    /* Nothing special */
    ANIMATION_COLOR_DEFAULT, /* Default keyboard color */
    ANIMATION_COLOR_TRANS,   /* Transparent color */
    ANIMATION_COLOR_RANDOM,  /* Random color yet consistent color */
    ANIMATION_COLOR_NOISE,   /* Color noise pattern */
    ANIMATION_COLOR_SHIMMER, /* Shimmer color */
} animation_color_special_e;

/**
 * @brief Animation color, that can be either an HSV or a special color
 */
typedef struct animation_color {
    HSV                       hsv;
    animation_color_special_e special;
} animation_color_t;

/**
 * @brief Convenience enum for the meaning of each value in the animation's hsv color array
 */
enum animation_hsv_color_layer : uint8_t {
    ANIMATION_HSV_COLOR_BASE   = 0, /* Base color of the animation */
    ANIMATION_HSV_COLOR_RESULT = 1, /* Color left after animation is completed, if any. Used by WAVE */

    /* Enum values with _N are the "negative" LEDs that are not in the keymap */
    ANIMATION_HSV_COLOR_BASE_N   = 2, /* Base color of LEDs not in the keymap, if any */
    ANIMATION_HSV_COLOR_RESULT_N = 3, /* Result color of LEDs not in the keymap, if any. Used by WAVE */

    ANIMATION_HSV_COLOR_COUNT, /* Animation color layer count enum value for convenience */
};

/**
 * @brief Animation type that holds all* data necessary for playing an animation
 */
typedef struct animation {
    animation_type_e type; /* Animation type */

    uint8_t  led_index; /* LED data for animationn. Used by SOLID_KEY and WAVE */
    bool     done;      /* If this animation has finished already */
    uint32_t ticks;     /* When this animation starts */

    /* List of keys that should be affected by the animation */
    const uint16_t (*keymap)[MATRIX_ROWS][MATRIX_COLS];

    /* Array of colors to be used by animation. See `animation_hsv_color_layer` */
    animation_color_t hsv_colors[ANIMATION_HSV_COLOR_COUNT];
} animation_t;

/**
 * @brief Creates an animation color from hsv value
 *
 * @param h Hue of the color
 * @param s Saturation of the color
 * @param v Value of the color
 * @return animation_color_t An animation color structure of the given color
 */
animation_color_t animation_color_hsv(uint8_t h, uint8_t s, uint8_t v);

/**
 * @brief Creates an animation color from a special color
 *
 * @param special The special type of color to create
 * @return animation_color_t An animation color structure of the given special color
 */
animation_color_t animation_color_special(animation_color_special_e special);

/**
 * @brief Creates an animation that clears the entire keyboard
 *
 * @return animation_t The animation object
 */
animation_t animation_clear(void);

/**
 * @brief Creates an animation object that clears a single key
 *
 * @param led_index The LED to clear
 * @return animation_t The animation object
 */
animation_t animation_clear_key(uint8_t led_index);

/**
 * @brief Creates an animation object that sets the entire keyboard to a solid color
 *
 * @param h Hue of the solid color
 * @param s Saturation of the solid color
 * @param v Value of the solid color
 * @return animation_t The animation object
 */
animation_t animation_solid(uint8_t h, uint8_t s, uint8_t v);

/**
 * @brief Creates an animation object that sets one key to a solid color
 *
 * @param led_index The LED to set
 * @param h Hue of the LED color
 * @param s Saturation of the LED color
 * @param v Value of the LED color
 * @return animation_t The animation object
 */
animation_t animation_solid_key(uint8_t led_index, uint8_t h, uint8_t s, uint8_t v);

/**
 * @brief Creates an animation object that creates a wave of one color, and goes back to
 * the previous existing colors
 *
 * @param start_led_index The LED the wave starts at
 * @param wave_color The color of the wave
 * @return animation_t The animation object
 */
animation_t animation_wave(uint8_t start_led_index, animation_color_t wave_color);

/**
 * @brief Creates an animation object that creates a wave of one color, and stays that color
 * after the wave is done
 *
 * @param start_led_index  The LED the wave starts at
 * @param wave_color The color of the wave and result
 * @return animation_t The animation object
 */
animation_t animation_wave_solid(uint8_t start_led_index, animation_color_t wave_color);

/**
 * @brief Creates an animation object that creates a wave of one color, and turns all LEDs it
 * goes through into another color
 *
 * @param start_led_index The LED the wave starts at
 * @param wave_color The color of the wave
 * @param solid_color The color that results after the wave
 * @return animation_t The animation object
 */
animation_t animation_wave_solid_2(uint8_t start_led_index, animation_color_t wave_color,
                                   animation_color_t solid_color);

/**
 * @brief Creates an animation object that turns the entire keyboard into a shimmering
 * RGB slop
 *
 * @return animation_t The animation object
 */
animation_t animation_shimmer(void);

/**
 * @brief Initializes the animation stack. Must be called before all other sgv_animation_ functions
 */
void sgv_animation_preinit(void);

/**
 * @brief Initializes the animation state. Will play the startup animation too
 */
void sgv_animation_init(void);

/**
 * @brief Runs one frame of animation
 *
 * @param params Animation parameters given by effect handler
 * @return true If the last LED in the batch is indeed the last one to use as well
 * @return false Otherwise
 */
bool sgv_animation_update(effect_params_t* params);

/**
 * @brief Adds an animation to the back of the queue to be played on top of the others
 *
 * @param animation The animation to add to the queue
 */
void sgv_animation_add_animation(animation_t animation);

/**
 * @brief Adds several animations which together create the startup animation
 *
 * @param first_wave_start_led LED where the first wave of the animation starts
 * @param second_wave_start_led LED where the second wave of the animation starts
 * @param cleanup_start_led LED where the cleanup waves of the animation start
 */
void sgv_animation_add_startup_animation(uint8_t first_wave_start_led, uint8_t second_wave_start_led,
                                         uint8_t cleanup_start_led);

/**
 * @brief Resets the state and clears the queue of the animations
 */
void sgv_animation_reset(void);
