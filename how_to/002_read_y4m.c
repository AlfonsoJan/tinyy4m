#define TINYY4MDEF static inline
#define TINYY4M_LOG_LEVEL TINYY4M_LOG_DEBUG
#define TINYY4M_IMPLEMENTATION
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
