#include <string.h>

#define TINYY4MDEF static inline
#define TINYY4M_IMPLEMENTATION
#include "../tinyy4m.h"

int main(void) {
    const char *file_name = "output.y4m";

    /* Probe just to know resolution and fps */
    Y4MProbe probe = {0};
    if (y4m_probe_file(file_name, &probe) != 0) {
        return 1;
    }

    FILE *f = fopen(file_name, "rb");
    if (!f) return 1;

    /* Seek to start of frame data */
    fseek(f, probe.data_offset, SEEK_SET);

    size_t planeN = (size_t)probe.width * (size_t)probe.height;
    size_t frame_size = probe.frame_size;
    /* adjust max resolution if needed */
    static uint8_t frame_buffer[1920 * 1080 * 3];

    for (int i = 0; i < probe.frames; i++) {
        char line[128];
        if (!fgets(line, sizeof(line), f)) break;

        if (!strncmp(line, "FRAME", 5)) {
            size_t got = fread(frame_buffer, 1, frame_size, f);
            if (got != frame_size) break;

            uint8_t *Y = frame_buffer;
            uint8_t *U = frame_buffer + planeN;
            uint8_t *V = frame_buffer + 2 * planeN;

            printf("Frame %d: Y[0]=%u U[0]=%u V[0]=%u\n",
                   i, Y[0], U[0], V[0]);

            /* ...process Y,U,V as needed... */
        }
    }

    fclose(f);
    return 0;
}
