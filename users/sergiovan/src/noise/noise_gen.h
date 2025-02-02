#pragma once

#include <stdint.h>

typedef int32_t int32q16_t;

/**
 * @brief Gets one byte of perlin noise at location x, y. Based off https://gist.github.com/nowl/828013
 *
 * @param x x position, in i32q0
 * @param y y position, in i32q0
 * @param freq "Zoom" value, in i16q16. The smaller this is, the more "zoomed in" the noise becomes
 * @return uint8_t The value of the perlin noise at x, y
 */
uint8_t perlin2d_fixed(int32_t x, int32_t y, int32q16_t freq);
