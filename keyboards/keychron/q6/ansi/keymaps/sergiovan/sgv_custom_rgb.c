#ifndef LED_MATRIX_CUSTOM_EFFECT_IMPLS
// This is here for IDE convenience :)
#    include QMK_KEYBOARD_H // clangd does not enjoy macro inclusions so I'm adding this long comment to avoid issues with my IDE
#    include "lib/lib8tion/lib8tion.h"
#endif

#include "animation/animation.h"

static bool sgv_custom_rgb(effect_params_t* params) {
    if (params->init) {
        rgb_matrix_set_color_all(0, 0, 0);
        sgv_animation_init();
    }

    return sgv_animation_update(params);
}
