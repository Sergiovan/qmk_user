#pragma once
#include "common.h"

typedef enum animation_type : uint8_t {
    CLEAR_KEY, // Turn off one key
    CLEAR_ALL, // Turn off all keys

    SOLID_KEY, // Set one key to a solid color
    SOLID_ALL, // Set all keys to a solid color

    WAVE,         // Wave that changes to another color and then returns to previous
    WAVE_SOLID,   // Wave that changes to another color and stays there
    WAVE_SOLID_2, // Wave that changes to another color and then stays a second different color
} animation_type_e;

typedef enum animation_payload_type : uint8_t {
    PAYLOAD_NONE,          // No color information
    PAYLOAD_COLOR_RGB,     // Color information in RGB format
    PAYLOAD_COLOR_HSV,     // Color information in HSV format
    PAYLOAD_COLOR_HSV_2,   // Color information in HSV format x2
    PAYLOAD_COLOR_DEFAULT, // Default color currently selected by keyboard
} animation_payload_type_e;

typedef struct animation {
    animation_type_e         type;
    animation_payload_type_e payload_type;

    uint8_t  key_index;
    uint16_t ticks;
    bool     done;

    union {
        uint64_t raw;
        RGB      rgb_color;
        HSV      hsv_color;
        HSV      hsv_color_arr[2];
    };
} animation_t;

void sgv_animation_preinit(void);
void sgv_animation_init(void);
bool sgv_animation_update(effect_params_t* params);
void sgv_animation_add_animation(animation_t animation);
