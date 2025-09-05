#ifndef TINYY4M_H_
#define TINYY4M_H_

#include <stdio.h>
#include <stdint.h>

#ifndef TINYY4MDEF
/*
   Define TINYY4MDEF before including this file to control function linkage.
   Example: #define TINYY4MDEF static inline
   This is useful for single-file projects to let the compiler inline functions
   and remove unused ones.
*/
#define TINYY4MDEF
#endif /* TINYY4MDEF */

typedef enum {
    PIXELFORMAT_UNCOMPRESSED_R8G8B8 = 1,
    PIXELFORMAT_UNCOMPRESSED_B8G8R8
} PixelFormat;

typedef struct {
    FILE *f;
    int width, height;
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
    PixelFormat format;
} Y4MWriter;

TINYY4MDEF int y4m_start(const char *filename, Y4MWriter *w, const int width, const int height, const int fps, uint8_t *y, uint8_t *u, uint8_t *v);
TINYY4MDEF void y4m_write_frame(Y4MWriter *w, uint32_t *pixels);
TINYY4MDEF void y4m_end(Y4MWriter *w);

#ifdef TINYY4M_IMPLEMENTATION

TINYY4MDEF int y4m_start(const char *filename, Y4MWriter *w, const int width, const int height, const int fps, uint8_t *y, uint8_t *u, uint8_t *v) {
    if (!w || !y || !u || !v || width <= 0 || height <= 0 || fps <= 0 || w->format == 0) return 1;

    w->f = fopen(filename, "wb");
    if (!w->f) return 1;

    w->width = width;
    w->height = height;

    w->y_plane = y;
    w->u_plane = u;
    w->v_plane = v;

    fprintf(w->f, "YUV4MPEG2 W%d H%d F%d:1 Ip A1:1 C444\n", width, height, fps);
    return 0;
}

TINYY4MDEF void y4m_write_frame(Y4MWriter *w, uint32_t *pixels) {
    fprintf(w->f, "FRAME\n");

    const size_t N = (size_t)w->width * (size_t)w->height;
    for (size_t i = 0; i < N; ++i) {
        uint8_t r, g, b;
        switch(w->format) {
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
            r = (pixels[i] >> 16) & 0xFF;
            g = (pixels[i] >> 8) & 0xFF;
            b = (pixels[i] >> 0) & 0xFF;
            break;
        case PIXELFORMAT_UNCOMPRESSED_B8G8R8:
            r = (pixels[i] >> 0) & 0xFF;
            g = (pixels[i] >> 8) & 0xFF;
            b = (pixels[i] >> 16) & 0xFF;
            break;
        }

        float Yf =  0.299f * r + 0.587f * g + 0.114f * b;
        float Uf = -0.169f * r - 0.331f * g + 0.500f * b + 128.0f;
        float Vf =  0.500f * r - 0.419f * g - 0.081f * b + 128.0f;

        /* Clamp to [0,255] */
        uint8_t Y = (uint8_t)(Yf < 0 ? 0 : (Yf > 255 ? 255 : Yf));
        uint8_t U = (uint8_t)(Uf < 0 ? 0 : (Uf > 255 ? 255 : Uf));
        uint8_t V = (uint8_t)(Vf < 0 ? 0 : (Vf > 255 ? 255 : Vf));

        w->y_plane[i] = Y;
        w->u_plane[i] = U;
        w->v_plane[i] = V;
    }

    fwrite(w->y_plane, 1, N, w->f);
    fwrite(w->u_plane, 1, N, w->f);
    fwrite(w->v_plane, 1, N, w->f);
}

TINYY4MDEF void y4m_end(Y4MWriter *w) {
    if (!w) return;
    fclose(w->f);
    w->f = NULL;
}

#endif // TINYY4M_IMPLEMENTATION

#endif // TINYY4M_H_