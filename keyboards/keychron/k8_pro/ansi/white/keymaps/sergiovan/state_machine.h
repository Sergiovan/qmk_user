#pragma once

#define STATE_NAME(NAME) STATE_MACHINE_STATE_##NAME

#define STATE_MACHINE_ENUM_VALUE(val) val
typedef enum : uint8_t {
#include "state_machine.gen.h"
} state_machine_state_e;
#undef STATE_MACHINE_ENUM_VALUE

typedef void (*state_cb_t)(void);

typedef struct {
    state_machine_state_e current_state_path;
    state_machine_state_e terminal_state;
    uint8_t               position;
    uint16_t              last_code;
    uint32_t              last_tick;
} state_t;

typedef struct {
    state_t state;
} state_machine_t;

void       state_machine_init(state_machine_t* state_machine);
void       state_machine_tick(state_machine_t* state_machine, bool keypress);
state_cb_t state_machine_advance(state_machine_t* state_machine, uint16_t keycode);
void       state_machine_break_combo(state_machine_t* state_machine, bool keypress);
