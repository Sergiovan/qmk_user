#pragma once

typedef enum {
    QUIET_ANIMATION_CLEAR,
    QUIET_ANIMATION_NICE
} quiet_animation_state_t;

void set_animation_state(quiet_animation_state_t state);
