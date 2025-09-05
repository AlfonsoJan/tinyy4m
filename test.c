#define TINYY4MDEF static inline
#define TINYY4M_IMPLEMENTATION
#include "tinyy4m.h"

#define WIDTH 1600
#define HEIGHT 900
#define N WIDTH*HEIGHT
#define FPS 60
#define DURATION_SECONDS 5
#define TOTAL_FRAMES FPS * DURATION_SECONDS

static uint32_t pixels[WIDTH * HEIGHT];
static uint8_t Y[N], U[N], V[N];

void fill_pixels(uint32_t color) {
    for (size_t y = 0; y < HEIGHT; ++y) {
        for (size_t x = 0; x < WIDTH; ++x) {
            pixels[y * WIDTH + x] = color;
        }
    }
}

int main() {
    fill_pixels(0x00FF00);
    Y4MWriter writer = {
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8
    };
    if(y4m_start("out.y4m", &writer, WIDTH, HEIGHT, FPS, Y, U, V) != 0) {
        // LOG ERROR
        return -1;
    }

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        y4m_write_frame(&writer, pixels);
    }

    y4m_end(&writer);
}
