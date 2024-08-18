#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct circular_buffer circular_buffer_t;

extern const size_t circular_buffer_type_size;

bool circular_buffer_new(void* cb_location, size_t cb_location_size, void* buffer, size_t buffer_size, uint8_t elems,
                         uint8_t elem_size);

bool  circular_buffer_push(circular_buffer_t* cb, const void* elem);
void* circular_buffer_pop(circular_buffer_t* cb, void* elem);
bool  circular_buffer_unshift(circular_buffer_t* cb, const void* elem);
void* circular_buffer_shift(circular_buffer_t* cb, void* elem);

void* circular_buffer_at(circular_buffer_t* cb, uint8_t index);

bool    circular_buffer_full(circular_buffer_t* cb);
bool    circular_buffer_empty(circular_buffer_t* cb);
uint8_t circular_buffer_length(circular_buffer_t* cb);
uint8_t circular_buffer_size(circular_buffer_t* cb);
