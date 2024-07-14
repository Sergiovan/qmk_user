#pragma once

#define STATE_NAME(NAME) STATE_MACHINE_STATE_##NAME

typedef enum : uint8_t {
    STATE_NAME(NONE),
    STATE_NAME(SIXTY_NINE),

    STATE_NAME(LAST),
} state_machine_state_e;

typedef void (*state_cb_t)(void);

typedef struct {
    state_machine_state_e current_state_path;
    uint8_t               position;
    uint16_t              last_code;
    uint32_t              last_tick;
} state_t;

typedef struct {
    state_t state;
} state_machine_t;

void       state_machine_init(state_machine_t* state_machine);
state_cb_t state_machine_advance(state_machine_t* state_machine, uint16_t keycode, uint16_t original_keycode);
