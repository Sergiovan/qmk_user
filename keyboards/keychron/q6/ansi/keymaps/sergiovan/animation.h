#pragma once
#include "common.h"

typedef enum animation_type : uint8_t {
    // Animation types that use 1 color only
    SOLID_KEY, // Set one key to a solid color
    SOLID_ALL, // Set all keys to a solid color
    SHIMMER,   // Randomly changing colors

    // Animation types that use 2 colors
    WAVE, // Wave that changes to another color
} animation_type_e;

typedef enum animation_color_special : uint8_t {
    ANIMATION_COLOR_NONE,    // Nothing special
    ANIMATION_COLOR_DEFAULT, // Default keyboard color
    ANIMATION_COLOR_TRANS,   // Transparent color
    ANIMATION_COLOR_RANDOM,  // Random color yet consistent color
    ANIMATION_COLOR_NOISE,   // Color noise pattern
    ANIMATION_COLOR_SHIMMER, // Shimmer color
} animation_color_special_e;

typedef struct animation_color {
    HSV                       hsv;
    animation_color_special_e special;
} animation_color_t;

enum animation_hsv_color_layer : uint8_t {
    ANIMATION_HSV_COLOR_BASE   = 0,
    ANIMATION_HSV_COLOR_RESULT = 1,

    ANIMATION_HSV_COLOR_BASE_N   = 2,
    ANIMATION_HSV_COLOR_RESULT_N = 3,

    ANIMATION_HSV_COLOR_COUNT,
};

typedef struct animation {
    animation_type_e type;

    uint8_t  key_index;
    bool     done;
    uint32_t ticks;

    const uint16_t (*keymap)[MATRIX_ROWS][MATRIX_COLS];

    animation_color_t hsv_colors[ANIMATION_HSV_COLOR_COUNT];
} animation_t;

animation_color_t animation_color_hsv(HSV hsv);
animation_color_t animation_color_special(animation_color_special_e special);

animation_t animation_clear(void);
animation_t animation_clear_key(uint8_t key_index);
animation_t animation_solid(HSV color);
animation_t animation_solid_key(uint8_t key_index, HSV color);
animation_t animation_wave(uint8_t start_key_index, animation_color_t wave_color);
animation_t animation_wave_solid(uint8_t start_key_index, animation_color_t wave_color);
animation_t animation_wave_solid_2(uint8_t start_key_index, animation_color_t wave_color,
                                   animation_color_t solid_color);
animation_t animation_shimmer(void);

void sgv_animation_preinit(void);
void sgv_animation_init(void);
bool sgv_animation_update(effect_params_t* params);
void sgv_animation_add_animation(animation_t animation);
void sgv_animation_add_startup_animation(uint8_t first_wave_start, uint8_t second_wave_start, uint8_t cleanup_start);
void sgv_animation_reset(void);
