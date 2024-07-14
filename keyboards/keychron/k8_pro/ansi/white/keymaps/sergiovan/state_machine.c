#include QMK_KEYBOARD_H // This comment is here to avoid clangd creating overly long clickable links

#include "state_machine.h"
#include "quiet_effect.h"

static const uint32_t COMBO_BREAKER_TICKS = TIME_MS2I(250);

static void no_action(void) {}

static void set_sixty_nine(void) {
    set_animation_state(QUIET_ANIMATION_NICE);
}

static const state_cb_t action_mapping[STATE_NAME(LAST)] = {
    [STATE_NAME(NONE)]       = no_action,
    [STATE_NAME(SIXTY_NINE)] = set_sixty_nine,
};

void state_machine_init(state_machine_t *state_machine) {
    state_machine->state = (state_t){
        .current_state_path = STATE_MACHINE_STATE_NONE,
        .position           = 0,
        .last_code          = KC_TRANSPARENT,
        .last_tick          = 0,
    };
}

static void break_combo(void) {
    set_animation_state(QUIET_ANIMATION_CLEAR);
}

#define NEW_STATE(CURRENT, POSITION)                                                                                          \
    (state_t) {                                                                                                               \
        .current_state_path = STATE_NAME(CURRENT), .position = (POSITION), .last_code = keycode, .last_tick = timer_read32(), \
    }

state_cb_t state_machine_advance(state_machine_t *state_machine, uint16_t keycode, uint16_t original_keycode) {
    state_t *state = &state_machine->state;
    if (state->current_state_path != STATE_MACHINE_STATE_NONE && timer_elapsed32(state->last_tick) > COMBO_BREAKER_TICKS) {
        state_machine_init(state_machine);
    }

    // Modifiers only extend the combo and do nothing
    if (IS_MODIFIER_KEYCODE(original_keycode)) {
        state->last_tick = timer_read32();
        return no_action;
    }

    switch (state->current_state_path) {
        case STATE_NAME(SIXTY_NINE):
            switch (keycode) {
                case KC_9:
                    if (state->position == 0) {
                        state_machine->state = NEW_STATE(SIXTY_NINE, 1);
                        return action_mapping[STATE_NAME(SIXTY_NINE)];
                    }
                    [[fallthrough]];
                default:
                    goto RESET_COMBO;
            }
            break;

        RESET_COMBO:
            break_combo();
            [[fallthrough]];
        // Combo start
        case STATE_NAME(NONE): {
            switch (keycode) {
                case KC_6:
                    state_machine->state = NEW_STATE(SIXTY_NINE, 0);
                    break;
                default:
                    state_machine_init(state_machine);
                    break;
            }
            break;
        }
        case STATE_NAME(LAST):
            state_machine_init(state_machine);
            return no_action;
    }

    return action_mapping[STATE_NAME(NONE)];
}
