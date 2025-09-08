# tinyy4m

A single header C library for writing YUV4MPEG2 (.y4m) video files

## Quick Example

```c
#define TINYY4MDEF static inline
#define TINYY4M_LOG_LEVEL TINYY4M_LOG_DEBUG
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

int main() {
    Y4MOption opt = {
        .width = WIDTH,
        .height = HEIGHT,
        .fps = FPS,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
        .filename = "output.y4m"
    };
    Y4MWriter writer = {0};
    if(y4m_start(opt, &writer, Y, U, V) != 0) {
        return -1;
    }

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        y4m_write_frame(&writer, pixels);
    }

    y4m_end(&writer);
}

```
