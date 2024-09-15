#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Circular buffer handle */
typedef struct circular_buffer circular_buffer_t;

/* Size in bytes of the circular buffer structure */
extern const size_t circular_buffer_type_size;

/**
 * @brief Creates a circular buffer for elements of a certain size
 *
 * @param cb_location Address where the circular buffer will be created
 * @param cb_location_size Size of the circular buffer location. This function will not work if the
 *                         correct type is not passed
 * @param buffer Address where the actual data buffer is located
 * @param buffer_size Size of the buffer in bytes
 * @param elems Elements this circular buffer must be able to hold
 * @param elem_size Size of each element in bytes
 * @return true If the circular buffer was successfully allocated
 * @return false Otherwise
 */
bool circular_buffer_new(void* cb_location, size_t cb_location_size, void* buffer, size_t buffer_size, uint8_t elems,
                         uint8_t elem_size);

/**
 * @brief Adds an element at the back of the circular buffer. Will drop the element
 *        at the start of the queue if full
 *
 * @param cb Circular buffer handle
 * @param elem Pointer to element to add
 * @return true If the element was added
 * @return false Otherwise
 */
bool circular_buffer_push(circular_buffer_t* cb, const void* elem);

/**
 * @brief Removes an element from the back of the circular buffer
 *
 * @param cb Circular buffer handle
 * @param elem Pointer to data to place the popped value
 * @return void* The popped value, or NULL if nothing was removed
 */
void* circular_buffer_pop(circular_buffer_t* cb, void* elem);

/**
 * @brief Adds an element at the front of the circular buffer. Will drop the element
 *        at the back of the queue if full
 *
 * @param cb Circular buffer handle
 * @param elem Pointer to element to add
 * @return true If the element was added
 * @return false Otherwise
 */
bool circular_buffer_unshift(circular_buffer_t* cb, const void* elem);

/**
 * @brief Removes an element from the front of the circular buffer
 *
 * @param cb Circular buffer handle
 * @param elem Pointer to data to place the shifted value
 * @return void* The shifted value, or NULL if nothing was removed
 */
void* circular_buffer_shift(circular_buffer_t* cb, void* elem);

/**
 * @brief Returns the element at an index in the circular buffer
 *
 * @param cb Circular buffer handle
 * @param index Index to get the element at
 * @return void* The element at index `index`, or NULL if nothing could be retrieved
 */
void* circular_buffer_at(circular_buffer_t* cb, uint8_t index);

/**
 * @brief If the circular buffer is full, i.e. it has no space left
 *
 * @param cb Circular buffer handle
 * @return true If the circular buffer has no space left for new elements
 * @return false Otherwise
 */
bool circular_buffer_full(circular_buffer_t* cb);

/**
 * @brief If the circular buffer is empty, i.e. it has no elements
 *
 * @param cb Circular buffer handle
 * @return true If the circular buffer has no elements
 * @return false Otherwise
 */
bool circular_buffer_empty(circular_buffer_t* cb);

/**
 * @brief Returns the current amount of items in the circular buffer
 *
 * @param cb Circular buffer handle
 * @return uint8_t Amount of items in the circular buffer
 */
uint8_t circular_buffer_length(circular_buffer_t* cb);

/**
 * @brief Returns the maximum amount of elements this circular buffer can hold
 *
 * @param cb Circular buffer handle
 * @return uint8_t Maximum amount of elements the circular buffer can hold
 */
uint8_t circular_buffer_size(circular_buffer_t* cb);
