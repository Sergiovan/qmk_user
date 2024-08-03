#include QMK_KEYBOARD_H // A little comment to avoid weird underlines :)

enum layers { BASE, FN, SECRET };

enum custom_keycodes {
    SGV_FN = SAFE_RANGE,
    SGV_RGB,
};
