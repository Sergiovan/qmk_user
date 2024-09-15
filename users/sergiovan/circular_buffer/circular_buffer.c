#include "circular_buffer.h"

#include <string.h>

/**
 * @brief Circular buffer structure. Allows for easy random access,
 * insertion and deletion at either end.
 */
typedef struct circular_buffer {
    void* buffer; /* Pointer to data */

    uint8_t elems;     /* Size of buffer in elements */
    uint8_t elem_size; /* Size of each element */

    uint8_t length; /* Amount of elements in the buffer */
    uint8_t begin;  /* Index of first element, inclusive */
    uint8_t end;    /* Index of last element, inclusive */
} circular_buffer_t;

const size_t circular_buffer_type_size = sizeof(circular_buffer_t);

bool circular_buffer_new(void* cb_location, size_t cb_location_size, void* buffer, size_t buffer_size, uint8_t elems,
                         uint8_t elem_size) {
    if (cb_location == NULL || buffer == NULL) {
        return false;
    }

    if (buffer_size == 0 || elems == 0 || elem_size == 0) {
        return false;
    }

    if (cb_location_size != sizeof(circular_buffer_t)) {
        return false;
    }

    if (buffer_size != (size_t)elems * elem_size) {
        return false;
    }

    *(circular_buffer_t*)cb_location = (circular_buffer_t){
        .buffer    = buffer,
        .elems     = elems,
        .elem_size = elem_size,
        .length    = 0,
        .begin     = 0,
        .end       = 0,
    };

    return true;
}

/**
 * @brief Convenience function that moves the end pointer forward
 *
 * @param cb Circular buffer
 */
static inline void increase_end(circular_buffer_t* cb) {
    if (!cb) return;

    if (cb->end == cb->elems - 1) {
        cb->end = 0;
    } else {
        cb->end++;
    }
}

/**
 * @brief Convenience function that moves the end pointer backward
 *
 * @param cb Circular buffer
 */
static inline void decrease_end(circular_buffer_t* cb) {
    if (!cb) return;

    if (cb->end == 0) {
        cb->end = cb->elems - 1;
    } else {
        cb->end--;
    }
}

/**
 * @brief Convenience function that moves the start pointer forward
 *
 * @param cb Circular buffer
 */
static inline void increase_begin(circular_buffer_t* cb) {
    if (!cb) return;

    if (cb->begin == cb->elems - 1) {
        cb->begin = 0;
    } else {
        cb->begin++;
    }
}

/**
 * @brief Convenience function that moves the start pointer backward
 *
 * @param cb Circular buffer
 */
static inline void decrease_begin(circular_buffer_t* cb) {
    if (!cb) return;

    if (cb->begin == 0) {
        cb->begin = cb->elems - 1;
    } else {
        cb->begin--;
    }
}

/**
 * @brief Convenience function that retrieves an element of the buffer without checking
 *        if it exists or not, or if it is in bounds. Indexed into the buffer directly,
 *        without using start or end pointers
 *
 * @param cb Circular buffer
 * @param index Index of element to retrieve
 * @return void* Pointer to start of element
 */
static inline void* at_unsafe(circular_buffer_t* cb, uint8_t index) {
    if (!cb) return NULL;

    return cb->buffer + (index * cb->elem_size);
}

bool circular_buffer_push(circular_buffer_t* cb, const void* elem) {
    if (!cb) return false;
    if (!elem) return false;

    if (cb->length == 0) {
        cb->begin = cb->end = 0;
        memcpy(at_unsafe(cb, cb->end), elem, cb->elem_size);
        cb->length++;
        return true;
    } else {
        increase_end(cb);
        memcpy(at_unsafe(cb, cb->end), elem, cb->elem_size);
        if (cb->begin == cb->end) {
            increase_begin(cb);
            return true;
        } else {
            cb->length++;
            return true;
        }
    }
}

void* circular_buffer_pop(circular_buffer_t* cb, void* elem) {
    if (!cb) return NULL;
    if (!elem) return elem;

    if (cb->length == 0) {
        return NULL;
    } else if (cb->length == 1) {
        memcpy(elem, at_unsafe(cb, cb->end), cb->elem_size);
        cb->length--;
        return elem;
    } else {
        memcpy(elem, at_unsafe(cb, cb->end), cb->elem_size);
        decrease_end(cb);
        cb->length--;
        return elem;
    }
}

bool circular_buffer_unshift(circular_buffer_t* cb, const void* elem) {
    if (!cb) return false;
    if (!elem) return false;

    if (cb->length == 0) {
        cb->begin = cb->end = 0;
        memcpy(at_unsafe(cb, cb->begin), elem, cb->elem_size);
        cb->length++;
        return true;
    } else {
        decrease_begin(cb);
        memcpy(at_unsafe(cb, cb->begin), elem, cb->elem_size);
        if (cb->begin == cb->end) {
            decrease_end(cb);
            return true;
        } else {
            cb->length++;
            return true;
        }
    }
}

void* circular_buffer_shift(circular_buffer_t* cb, void* elem) {
    if (!cb) return NULL;
    if (!elem) return NULL;

    if (cb->length == 0) {
        return NULL;
    } else if (cb->length == 1) {
        memcpy(elem, at_unsafe(cb, cb->begin), cb->elem_size);
        cb->length--;
        return elem;
    } else {
        memcpy(elem, at_unsafe(cb, cb->begin), cb->elem_size);
        increase_begin(cb);
        cb->length--;
        return elem;
    }
}

void* circular_buffer_at(circular_buffer_t* cb, uint8_t index) {
    if (!cb) return NULL;

    if (index >= cb->length) {
        return NULL;
    }

    uint8_t real_index = cb->begin + index;
    if (real_index >= cb->elems) {
        real_index -= cb->elems;
    }
    return at_unsafe(cb, real_index);
}

bool circular_buffer_full(circular_buffer_t* cb) {
    // We report null buffers as full since they accept no more elements
    if (!cb) return true;

    return cb->length == cb->elems;
}

bool circular_buffer_empty(circular_buffer_t* cb) {
    // We report null buffers as empty since they contain no elements
    if (!cb) return true;

    return cb->length == 0;
}

uint8_t circular_buffer_length(circular_buffer_t* cb) {
    if (!cb) return 0;

    return cb->length;
}

uint8_t circular_buffer_size(circular_buffer_t* cb) {
    if (!cb) return 0;

    return cb->elems;
}
