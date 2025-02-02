#include QMK_KEYBOARD_H // This comment is here to avoid clangd creating overly long clickable links

#include "state_machine.h"
#include "animation/animation.h"

/* Defined in state_machine.gen.c */
state_cb_t state_machine_advance_internal(state_machine_t *state_machine,
                                          const state_cb_t action_mapping[STATE_NAME(LAST)], uint16_t keycode,
                                          uint16_t original_keycode);

static const uint32_t COMBO_BREAKER_TICKS = TIME_MS2I(2000) / 100; // This is broken??

static void no_action(void) {}

static void set_sixty_nine(void) {
    // clang-format off
    static const uint16_t nice_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT_tkl_ansi(
     KC_NO,   KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,          KC_NO,  KC_NO,  KC_NO,
     KC_NO,   KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,
     KC_NO,   KC_NO,  KC_NO,  KC_E,   KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_I,   KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,
     KC_NO,   KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,          KC_NO,
     KC_NO,           KC_NO,  KC_NO,  KC_C,   KC_NO,  KC_NO,  KC_N,   KC_NO,  KC_NO,  KC_NO,  KC_NO,          KC_NO,          KC_NO,
     KC_NO,   KC_NO,  KC_NO,                          KC_NO,                          KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO);
    // clang-format on

    animation_t anim = (animation_t){
        .type = SHIMMER,

        .led_index = 69, // N // Nice
        .done      = false,
        .ticks     = timer_read32(),

        .keymap = &nice_map,

        .hsv_colors =
            {
                animation_color_special(ANIMATION_COLOR_SHIMMER),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
                animation_color_special(ANIMATION_COLOR_TRANS),
            },
    };

    sgv_animation_add_animation(anim);
}

// Note that these can't take too long to proces
static const state_cb_t action_mapping[STATE_NAME(LAST)] = {
    [STATE_NAME(NONE)]       = no_action,
    [STATE_NAME(SIXTY_NINE)] = set_sixty_nine,
};

void state_machine_init(state_machine_t *state_machine) {
    state_machine->state = (state_t){
        .current_state_path = STATE_NAME(NONE),
        .terminal_state     = STATE_NAME(NONE),
        .position           = 0,
        .last_code          = KC_TRANSPARENT,
        .last_tick          = 0,
    };
}

state_cb_t state_machine_advance(state_machine_t *state_machine, uint16_t keycode) {
    uint8_t  mods         = get_mods();
    uint16_t keycode_mods = ((uint16_t)((((mods >> 4) != 0) << 4) | (mods | (mods >> 4))) << 8);

    return state_machine_advance_internal(state_machine, action_mapping, keycode | keycode_mods, keycode);
}

void state_machine_break_combo(state_machine_t *state_machine, bool keypress) {
    state_t *state = &state_machine->state;

    if (state->terminal_state != STATE_NAME(NONE)) {
        // Execute this
        state_cb_t f = action_mapping[state->terminal_state];
        if (f) f();
    } else if (keypress) {
        // If it's just the timer that break the combo, let the animation mode play
        sgv_animation_reset();
    }

    state_machine_init(state_machine);
}

void state_machine_tick(state_machine_t *state_machine, bool keypress) {
    state_t *state = &state_machine->state;
    if (state->current_state_path != STATE_NAME(NONE) && timer_elapsed32(state->last_tick) > COMBO_BREAKER_TICKS) {
        state_machine_break_combo(state_machine, keypress);
    }
}
