#include "noise_gen.h"

#include "lib/lib8tion/lib8tion.h"

static int SEED = 0;

static uint8_t hash[] = {
    208, 34,  231, 213, 32,  248, 233, 56,  161, 78,  24,  140, 71,  48,  140, 254, 245, 255, 247, 247, 40,  185,
    248, 251, 245, 28,  124, 204, 204, 76,  36,  1,   107, 28,  234, 163, 202, 224, 245, 128, 167, 204, 9,   92,
    217, 54,  239, 174, 173, 102, 193, 189, 190, 121, 100, 108, 167, 44,  43,  77,  180, 204, 8,   81,  70,  223,
    11,  38,  24,  254, 210, 210, 177, 32,  81,  195, 243, 125, 8,   169, 112, 32,  97,  53,  195, 13,  203, 9,
    47,  104, 125, 117, 114, 124, 165, 203, 181, 235, 193, 206, 70,  180, 174, 0,   167, 181, 41,  164, 30,  116,
    127, 198, 245, 146, 87,  224, 149, 206, 57,  4,   192, 210, 65,  210, 129, 240, 178, 105, 228, 108, 245, 148,
    140, 40,  35,  195, 38,  58,  65,  207, 215, 253, 65,  85,  208, 76,  62,  3,   237, 55,  89,  232, 50,  217,
    64,  244, 157, 199, 121, 252, 90,  17,  212, 203, 149, 152, 140, 187, 234, 177, 73,  174, 193, 100, 192, 143,
    97,  53,  145, 135, 19,  103, 13,  90,  135, 151, 199, 91,  239, 247, 33,  39,  145, 101, 120, 99,  3,   186,
    86,  99,  41,  237, 203, 111, 79,  220, 135, 158, 42,  30,  154, 120, 67,  87,  167, 135, 176, 183, 191, 253,
    115, 184, 21,  233, 58,  129, 233, 142, 39,  128, 211, 118, 137, 139, 255, 114, 20,  218, 113, 154, 27,  127,
    246, 250, 1,   8,   198, 250, 209, 92,  222, 173, 21,  88,  102, 219};

/**
 * @brief Hashes x and y together using the hash table
 *
 * @param x x value of perlin 2d position
 * @param y y value of perlin 2d position
 * @return uint8_t The hash of (x, y)
 */
static uint8_t noise2(int32_t x, int32_t y) {
    uint8_t tmp = hash[(y + SEED) & 255];
    return hash[(tmp + x) & 255];
}

// x, y = u8, s = u16q16

/**
 * @brief Smoothly interpolates between x and y
 *
 * @param x One end of the interpolation range
 * @param y Other end of the interpolation range
 * @param s Interpolation value, in u16q16
 * @return uint8_t Interpolated value
 */
inline static uint8_t smooth_inter(uint8_t x, uint8_t y, uint16_t s) {
    uint32_t buff = ((uint64_t)s * (uint64_t)s);               // u32q32
    buff          = buff * ((3ULL << 16ULL) - 2 * s) >> 16ULL; // Keep it u32q32
    buff          = buff >> 24;                                // u32q8
    buff &= 0xFF;
    if (x > y) {
        return lerp8by8(x, y, buff);
    } else {
        return lerp8by8(y, x, 0xFF - buff);
    }
}

// x, y = i32q16

/**
 * @brief Get one noise data point
 *
 * @param x x value of the perlin 2D noise, in i32q16
 * @param y y value of the perlin 2D noise, in i32q16
 * @return uint8_t Perlin noise at location (x, y)
 */
inline static uint8_t noise2d(int32_t x, int32_t y) {
    // i16
    int16_t x_int = x >> 16;
    int16_t y_int = y >> 16;
    // u16q16
    uint16_t x_frac = x & 0xFFFF;
    uint16_t y_frac = y & 0xFFFF;

    uint8_t s = noise2(x_int, y_int);
    uint8_t t = noise2(x_int + 1, y_int);
    uint8_t u = noise2(x_int, y_int + 1);
    uint8_t v = noise2(x_int + 1, y_int + 1);

    uint8_t low  = smooth_inter(s, t, x_frac);
    uint8_t high = smooth_inter(u, v, x_frac);
    return smooth_inter(low, high, y_frac);
}

// x, y = i32q0, freq = i16q16

uint8_t perlin2d_fixed(int32_t x, int32_t y, int32q16_t freq) {
    return noise2d(x * freq, y * freq);
}
