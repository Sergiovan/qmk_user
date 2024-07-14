#!/bin/bash

python3 state_machine_gen.py
clang-format -i state_machine.gen.c
