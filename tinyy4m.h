#ifndef TINYY4M_H_
#define TINYY4M_H_

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

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
    TINYY4M_LOG_TRACE = 0,
    TINYY4M_LOG_DEBUG,
    TINYY4M_LOG_INFO,
    TINYY4M_LOG_WARNING,
    TINYY4M_LOG_ERROR,
    TINYY4M_LOG_FATAL,
    TINYY4M_LOG_NONE
} TINYY4M_LogLevel;

#ifndef TINYY4M_LOG_LEVEL
#define TINYY4M_LOG_LEVEL TINYY4M_LOG_INFO
#endif

#ifndef TINYY4M_LOG_HANDLER
#define TINYY4M_LOG_HANDLER(level, msg) \
    do { static const char* _tag[]={"TRACE","DEBUG","INFO","WARN","ERROR","FATAL","NONE"}; \
         fprintf(stderr,"[%s] %s\n", _tag[(level)], (msg)); } while(0)
#endif

#define TINYY4M_TRACELOG(level, fmt, ...) \
    do { y4m__trace_log((level), __FILE__, __LINE__, (fmt), ##__VA_ARGS__); } while(0)

TINYY4MDEF void y4m__trace_log(int level, const char *file, int line, const char *fmt, ...);

typedef enum {
    PIXELFORMAT_UNCOMPRESSED_R8G8B8 = 1,
    PIXELFORMAT_UNCOMPRESSED_B8G8R8
} PixelFormat;

typedef struct {
    const char *filename;
    int width, height, fps;
    PixelFormat format;
} Y4MOption;

typedef struct {
    FILE *f;
    Y4MOption opt;
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
} Y4MWriter;

TINYY4MDEF int y4m_start(Y4MOption opt, Y4MWriter *w, uint8_t *y, uint8_t *u, uint8_t *v);
TINYY4MDEF void y4m_write_frame(Y4MWriter *w, uint32_t *pixels);
TINYY4MDEF void y4m_end(Y4MWriter *w);

#ifdef TINYY4M_IMPLEMENTATION

TINYY4MDEF void y4m__trace_log(int level, const char *file, int line, const char *fmt, ...) {
    if (level < TINYY4M_LOG_LEVEL || level >= TINYY4M_LOG_NONE) return;

    char buf[1024];
    int n = snprintf(buf, (int)sizeof(buf), "%s:%d: ", file, line);
    if (n < 0 || n >= (int)sizeof(buf)) n = 0;

    va_list ap;
    va_start(ap, fmt);
    if (n < (int)sizeof(buf)) vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, ap);
    va_end(ap);

    TINYY4M_LOG_HANDLER(level, buf);
}

TINYY4MDEF int y4m_start(Y4MOption opt, Y4MWriter *w, uint8_t *y, uint8_t *u, uint8_t *v) {
    if (!w) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "writer pointer is NULL");
        return 1;
    }

    if (!y || !u || !v) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "plane buffers must be non-NULL (y=%p u=%p v=%p)", (void*)y, (void*)u, (void*)v);
        return 1;
    }

    if (opt.width <= 0 || opt.height <= 0 || opt.fps <= 0 || opt.format == 0 || opt.filename == NULL) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "invalid options (w=%d h=%d fps=%d fmt=%d file=%p)", opt.width, opt.height, opt.fps, opt.format, (void*)opt.filename);
        return 1;
    }

    w->f = fopen(opt.filename, "wb");
    if (!w->f) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to open '%s'", opt.filename);
        return 1;
    }

    w->opt = opt;
    w->y_plane = y;
    w->u_plane = u;
    w->v_plane = v;

    int wrote = fprintf(w->f, "YUV4MPEG2 W%d H%d F%d:1 Ip A1:1 C444\n", w->opt.width, w->opt.height, w->opt.fps);
    if (wrote < 0) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to write file header to '%s'", opt.filename);
        fclose(w->f);
        w->f = NULL;
        return 1;
    }

    size_t planeN = (size_t)w->opt.width * (size_t)w->opt.height;
    TINYY4M_TRACELOG(TINYY4M_LOG_INFO, "opened '%s' (%dx%d @ %dfps, format=%d, plane bytes=%llu)", opt.filename, opt.width, opt.height, opt.fps, opt.format, planeN);
    return 0;
}

TINYY4MDEF void y4m_write_frame(Y4MWriter *w, uint32_t *pixels) {
    if (!w || !w->f) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "writer/file is NULL");
        return;
    }
    if (!pixels) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "pixels buffer is NULL");
        return;
    }
    if (fprintf(w->f, "FRAME\n") < 0) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to write frame header");
        return;
    }

    switch (w->opt.format) {
    case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
    case PIXELFORMAT_UNCOMPRESSED_B8G8R8:
        break;
    default:
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "unknown pixel format %d, assuming R8G8B8", w->opt.format);
        w->opt.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
        break;
    }

    const size_t N = (size_t)w->opt.width * (size_t)w->opt.height;
    for (size_t i = 0; i < N; ++i) {
        uint8_t r, g, b;
        switch(w->opt.format) {
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

    size_t wroteY = fwrite(w->y_plane, 1, N, w->f);
    size_t wroteU = fwrite(w->u_plane, 1, N, w->f);
    size_t wroteV = fwrite(w->v_plane, 1, N, w->f);

    if (wroteY != N || wroteU != N || wroteV != N) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "short write (Y=%zu/%zu, U=%zu/%zu, V=%zu/%zu)", wroteY, N, wroteU, N, wroteV, N);
    } else {
        TINYY4M_TRACELOG(TINYY4M_LOG_DEBUG, "wrote %zu bytes (YUV444)", 3 * N);
    }
}

TINYY4MDEF void y4m_end(Y4MWriter *w) {
    if (!w) return;
    if (w->f) {
        if (fclose(w->f) != 0) {
            TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "fclose failed");
        } else {
            TINYY4M_TRACELOG(TINYY4M_LOG_INFO, "closed '%s'", w->opt.filename ? w->opt.filename : "(unknown)");
        }
        w->f = NULL;
    } else {
        TINYY4M_TRACELOG(TINYY4M_LOG_WARNING, "file already closed or was never opened");
    }
    // fclose(w->f);
    // w->f = NULL;
}

#endif // TINYY4M_IMPLEMENTATION

#endif // TINYY4M_H_