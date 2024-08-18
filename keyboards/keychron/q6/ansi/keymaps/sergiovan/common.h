#pragma once
#include QMK_KEYBOARD_H // A little comment to avoid weird underlines :)

enum layers { BASE, FN, SECRET, LAYER_COUNT };

extern const uint16_t PROGMEM keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

enum custom_keycodes {
    SGV_FN = SAFE_RANGE,
    SGV_RGB,

    SGV_COUNT
};
