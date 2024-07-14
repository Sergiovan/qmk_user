#!/bin/env python3

from typing import Any

def state_name(name: str):
    return f"STATE_NAME({name})"

def is_merged_state(name: str):
    return name.startswith(state_name(name)[:-2])

class KeyCode:
    def __init__(self, c_str: str | list[str]):
        self.c_str = [c_str] if isinstance(c_str, str) else c_str

    def __str__(self):
        return ', '.join(self.c_str)

    def __repr__(self):
        return f"Kc({self.c_str.__repr__()})"

KEYCODE_MAPPING = {
    '6': [KeyCode("KC_6")],
    '8': [KeyCode("KC_8")],
    '9': [KeyCode("KC_9")],
    '': [KeyCode("KC_NO")],
}

STATE_NONE = "NONE"
STATE_LAST = "LAST"
STATE_MERGED = "MERGED_"

class State:
    def __init__(self, state_name: str | list[str], position: int, entry_code: str):
        self.state_name = [state_name] if isinstance(state_name, str) else state_name
        self.position = position
        self.entry_code = entry_code # The code you type to enter this state
        self.terminal = []

        self.exits: dict[str, State] = {}

    def add_exit(self, to: 'State', through: list[str] | str):
        if isinstance(through, str):
            self._add_exit(to, through)
        else:
            for s in through:
                self._add_exit(to, s)

    def set_terminal(self, state_name: str):
        self.terminal += [state_name]

    def _add_exit(self, to: 'State', through: str):
        if through in self.exits:
            to.merge_with(self.exits[through])
        self.exits[through] = to

    def merge_with(self, other: 'State'):
        if self.entry_code != other.entry_code:
            raise ValueError(f"Cannot merge 2 states with different positions: I am {self.state_name} with entry_code \"{self.entry_code}\" trying to merge with {other.state_name} with entry_code \"{other.entry_code}\"")
        if self.position != other.position:
            raise ValueError(f"Cannot merge 2 states with different positions: I am {self.state_name} with position {self.position} trying to merge with {other.state_name} at position {other.position}")

        self.state_name += other.state_name
        self.terminal += other.terminal

        for k, v in other.exits.items():
            self._add_exit(v, k) # Will recursively merge

class StateMachine:
    def __init__(self):
        self.start = State(STATE_NONE, 0, "KC_TRANSPARENT")

    def add_state(self, *args: str | list[KeyCode] | KeyCode, state_name: str):
        missing: list[str] = []

        def to_list(n: str | list[KeyCode] | KeyCode) -> list[list[KeyCode]]:
            if isinstance(n, KeyCode):
                return [[n]]
            elif isinstance(n, str):
                ret: list[KeyCode] = []
                for c in n:
                    if c not in KEYCODE_MAPPING:
                        missing.append(f"Missing {c} from Keycode Mapping")
                        ret.append(KEYCODE_MAPPING[''])
                    else:
                        ret.append(KEYCODE_MAPPING[c])
                return ret
            else:
                return [n]

        keycode_only = [x for n in args for x in to_list(n)]

        if len(keycode_only) == 1:
            raise ValueError("Cannot make state machine of a single state, use a normal key remap for that")

        states: list[State] = [self.start]
        for x, position in enumerate(keycode_only):
            new_states = [State(state_name, x, s) for kc in position for s in kc.c_str]
            for state in states:
                for new_state in new_states:
                    state.add_exit(new_state, new_state.entry_code)
            states = new_states

        for state in states:
            state.set_terminal(state_name)

        for msg in missing:
            print("Error: " + msg)

    def gen_c(self):
        top_level: dict[str, dict[str, dict[int, tuple[str, str]]]] = {}
        merged_states: dict[str, list[str]] = {}
        all_states: set[str] = set()

        def get_state_name(name: list[str]):
            if len(name) == 1:
                return state_name(name[0].upper())
            else:
                new_name = state_name(STATE_MERGED + hex(hash(frozenset(name))).upper()[3:])
                merged_states[new_name] = name
                return new_name

        state_stack = [self.start]

        while len(state_stack):
            state = state_stack.pop(0)

            switch_state_name = get_state_name(state.state_name)
            all_states.add(switch_state_name)
            keycodes = top_level.setdefault(switch_state_name, {})

            for k, v in state.exits.items():
                real_state_name = get_state_name(v.state_name)
                all_states.add(real_state_name)

                positions = keycodes.setdefault(k, {})

                if v.position in positions:
                    raise ValueError(f"Found position = {v.position} already in \"{switch_state_name}\", leading to \"{positions[v.position]}\"")

                if len(v.terminal) > 1:
                    raise ValueError(f"From position {v.position} in state \"{switch_state_name}\", too many terminals: {v.terminal}")

                terminal = get_state_name(v.terminal) if len(v.terminal) == 1 else ''
                if terminal:
                    all_states.add(terminal)

                positions[v.position] = (real_state_name, terminal)

                state_stack.append(v)

        def sort_states(item: tuple[str, Any]):
            name, value = item
            if name.startswith(state_name(STATE_NONE)):
                return (2, name)
            elif is_merged_state(name):
                return (1, name)
            else:
                return (0, name)

        c_code = """ // This file was autogenerated, but I don't feel like installing ASCII here
                     // Please do not touch it :)

                     #include QMK_KEYBOARD_H // This comment is here to avoid clangd creating overly long clickable links

                     #include "state_machine.h"

                     state_cb_t state_machine_advance_internal(state_machine_t *state_machine, const state_cb_t action_mapping[STATE_NAME(LAST)], uint16_t keycode, uint16_t original_keycode) {
                         state_t *state = &state_machine->state;
                         state_machine_tick(state_machine, true);

                         // Modifiers only extend the combo and do nothing
                         if (IS_MODIFIER_KEYCODE(original_keycode)) {
                             state->last_tick = timer_read32();
                             return action_mapping[STATE_NAME(NONE)];
                         }

                         switch (state->current_state_path) {"""

        def c_state_t(current_state_path: str, terminal_state: str, position: int):
            if is_merged_state(terminal_state):
                raise ValueError(f"State \"{terminal_state}\" cannot be terminal")

            return "(state_t){" \
                   f".current_state_path = {current_state_path}, " \
                   f".terminal_state = {terminal_state}, " \
                   f".position = {position}, " \
                   ".last_code = keycode, .last_tick = timer_read32(),}"

        def do_state(switch_state: str, elems: dict[str, dict[int, tuple[str, str]]]):
            if len(elems) == 0:
                return ''
            c_code = f"""case {switch_state}:
                            switch (keycode) {{
                      """
            for keycode, positions in sorted(elems.items(), key=lambda x: x[0]):
                c_code += f"case {keycode}:\n"

                for position, (next_state, terminal_state) in sorted(positions.items(), key=lambda x: x[0]):
                    next_is_terminal = next_state == terminal_state

                    c_code += f"""if (state->position == {position}) {{
                                      state_machine->state = {c_state_t(next_state, terminal_state if terminal_state and not next_is_terminal else state_name(STATE_NONE), position + 1)};
                               """
                    if next_is_terminal:
                        c_code += f"return action_mapping[{terminal_state}];\n }}\n"
                    else:
                        c_code += "break; \n }\n"
                c_code += "goto RESET_COMBO;\n"
            c_code += """default:
                            goto RESET_COMBO;
                         }
                         break;
                      """
            return c_code

        def do_none_state(elems: dict[str, dict[int, tuple[str, str]]]):
            state_none = state_name(STATE_NONE)

            c_code = f"""default:
                         RESET_COMBO:
                             state_machine_break_combo(state_machine, true);
                             [[fallthrough]];
                         // Combo start
                         case {state_none}:
                             switch (keycode) {{
                      """
            for keycode, positions in sorted(elems.items(), key=lambda x: x[0]):
                c_code += f"case {keycode}:\n"
                for position, (next_state, terminal_state) in sorted(positions.items(), key=lambda x: x[0]):
                    c_code += f"""state_machine->state = {c_state_t(next_state, state_name(STATE_NONE), position + 1)};
                                  break;
                               """
            c_code += """default:
                             state_machine_init(state_machine);
                             break;
                         }
                         break;
                      """
            return c_code


        for switch_state, elems in sorted(top_level.items(), key=sort_states):
            if switch_state == state_name(STATE_NONE):
                continue

            c_code += do_state(switch_state, elems)

        c_code += do_none_state(top_level[state_name(STATE_NONE)])
        c_code += f"""    case {state_name(STATE_LAST)}:
                              state_machine_init(state_machine);
                              break;
                          }}

                          return action_mapping[STATE_NAME(NONE)];
                      }}
                   """
        h_code = "// This file was autogenerated, do not touchy\n"
        h_code += " /* clang-format off */ \n"
        h_code += f"STATE_MACHINE_ENUM_VALUE({state_name(STATE_NONE)}),\n"

        for state in sorted((all_states - set(merged_states.keys())) - set([state_name(STATE_NONE)])):
            h_code += f"STATE_MACHINE_ENUM_VALUE({state}),\n"

        h_code += f"STATE_MACHINE_ENUM_VALUE({state_name(STATE_LAST)}),\n"

        for state, merged_from in sorted(merged_states.items(), key=lambda n: n[0]):
            h_code += f"\n/* {', '.join(map(lambda x: state_name(x.upper()), merged_from))} */\n"
            h_code += f"STATE_MACHINE_ENUM_VALUE({state}),\n"
        h_code += " /* clang-format on */ "

        return c_code, h_code


if __name__ == "__main__":
    import os
    here = os.path.dirname(__file__)
    os.chdir(here)

    sm = StateMachine()

    sm.add_state("69", state_name="sixty_nine")

    c_code, h_code = sm.gen_c()

    with open('state_machine.gen.c', 'w') as f:
        f.write(c_code)
    with open('state_machine.gen.h', 'w') as f:
        f.write(h_code)
