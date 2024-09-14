#include <raylib.h>
#include <stdlib.h>
#include <stdint.h>

#include "noise_gen.h"

#define WIDTH 640
#define HEIGHT 480

#define KB_COLS 24
#define KB_ROWS 6

#define KB_COL_PIX (WIDTH / KB_COLS)
#define KB_ROW_PIX (HEIGHT / KB_ROWS)

uint8_t pixels[WIDTH * HEIGHT * 4] = {0};

void set_color(int x, int y, Color c) {
    int index         = (x + y * WIDTH) * 4;
    pixels[index]     = c.r;
    pixels[index + 1] = c.g;
    pixels[index + 2] = c.b;
    pixels[index + 3] = c.a;
}

float clamp(int16_t x) {
    return x > 255 ? 255 : x < 0 ? 0 : x;
}

Color hue2rgb(int16_t hue) {
    hue &= 255;
    int16_t r   = abs(hue * 6 - 0x300) - 0x100;                            // red
    int16_t g   = 0x200 - abs(hue * 6 - 0x200);                            // green
    int16_t b   = 0x200 - abs(hue * 6 - 0x400);                            // blue
    Color   rgb = {.r = clamp(r), .g = clamp(g), .b = clamp(b), .a = 255}; // combine components
    return rgb;
}

int main() {
    InitWindow(WIDTH, HEIGHT, "Perlin noise over time test");

    SetTargetFPS(60);

    Image checked = {
        .data = pixels, .width = WIDTH, .height = HEIGHT, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

    Texture screen_texture = LoadTextureFromImage(checked);

    UpdateTexture(screen_texture, pixels);

    while (!WindowShouldClose()) {
        // Do perlin
        double time = GetTime() / 2;

        for (int x = 0; x < 100; ++x) {
            for (int y = 0; y < 100; ++y) {
                volatile uint8_t _ = perlin2d_fixed(x, y, 3277);
            }
        }

        for (int x = 0; x < KB_COLS; ++x) {
            for (int y = 0; y < KB_ROWS; ++y) {
                uint8_t perlin1 = perlin2d_fixed(x - time * 10, y + time * 10, 3277);
                uint8_t perlin2 = perlin2d_fixed((KB_COLS - x) + time * 25, y + time * 5, 3277);
                uint8_t perlin3 = perlin2d_fixed(x + time * 15, (KB_ROWS - y) + time * 15, 3277);

                Color col = hue2rgb(perlin1 + perlin2 + perlin3);

                for (int rx = x * KB_COL_PIX; rx < (x + 1) * KB_COL_PIX; ++rx) {
                    for (int ry = y * KB_COL_PIX; ry < (y + 1) * KB_COL_PIX; ++ry) {
                        set_color(rx, ry, col);
                    }
                }
            }
        }

        UpdateTexture(screen_texture, pixels);

        BeginDrawing();

        DrawTexture(screen_texture, 0, 0, WHITE);
        // DrawText("0", 0x110, 0, 12, WHITE);
        // DrawText("FF", 0x110, 0x100, 12, WHITE);
        DrawFPS(WIDTH - 100, HEIGHT - 100);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}