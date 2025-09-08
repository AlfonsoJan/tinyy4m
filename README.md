# tinyy4m

A single header C library for writing YUV4MPEG2 (.y4m) video files

Currently supports only:

- Progressive (`Ip`)
- Aspect ratio `A1:1`
- Chroma format `C444`
- Integer frame rates (`F<fps>:1`)

## Writing Example

```c
#include "tinyy4m.h"

#define WIDTH 1600
#define HEIGHT 900
#define N WIDTH*HEIGHT
#define FPS 60
#define DURATION_SECONDS 5
#define TOTAL_FRAMES FPS * DURATION_SECONDS

static uint32_t pixels[WIDTH * HEIGHT];
static uint8_t Y[N], U[N], V[N];

int main(void) {
    Y4MOption opt = {
        .width = WIDTH,
        .height = HEIGHT,
        .fps = FPS,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
        .filename = "output.y4m"
    };
    Y4MWriter writer = {0};
    if(y4m_write_start(opt, &writer, Y, U, V) != 0) {
        return -1;
    }

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        y4m_write_frame(&writer, pixels);
    }

    y4m_write_end(&writer);
}
```

## Reading Example

```c
#include "../tinyy4m.h"

int main(void) {
    // Replace this with the filename
    const char *file_name = "output.y4m";
    Y4MProbe probe = {0};
    if (y4m_probe_file(file_name, &probe) != 0) return 1;

    uint8_t *buffer = malloc(probe.total_bytes);
    if (!buffer) return 1;

    if (y4m_read_into_buffer(file_name, &probe, buffer, probe.total_bytes) != 0) {
        free(buffer);
        return 1;
    }

    for (int i = 0; i < probe.frames; ++i) {
        uint8_t *Y,*U,*V;
        y4m_planes_from_buffer(&probe, buffer, i, &Y, &U, &V);
    }

    free(buffer);

    return 0;
}
```

## Limitations

Only supports Ip A1:1 C444 F<num>:1 files.
Reader currently loads the entire file into memory. For huge files, consider writing a streaming reader.
See [003_read_streaming.c](how_to/003_read_streaming.c) on how to do it.
