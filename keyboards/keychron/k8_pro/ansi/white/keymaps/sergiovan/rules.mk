SRC += state_machine.c
SRC += state_machine.gen.c
SRC += behaviour.c

LTO_ENABLE = yes
# VIA_ENABLE = yes
MOUSEKEY_ENABLE = yes
NKRO_ENABLE = yes
DEFERRED_EXEC_ENABLE = yes

OPT_DEFS += -DLED_MATRIX_CUSTOM_USER
