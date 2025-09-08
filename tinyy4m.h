#ifndef TINYY4M_H_
#define TINYY4M_H_

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

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
TINYY4MDEF int y4m_write_frame(Y4MWriter *w, uint32_t *pixels);
TINYY4MDEF void y4m_end(Y4MWriter *w);

typedef struct {
    int width, height, fps;
    int frames;
    size_t frame_size;
    size_t total_bytes;
    long data_offset;
} Y4MProbe;

TINYY4MDEF int y4m_probe_file(const char *filename, Y4MProbe *probe);
TINYY4MDEF int y4m_read_into_buffer(const char *filename, const Y4MProbe *probe, uint8_t *dst, size_t dst_size);
TINYY4MDEF int y4m_planes_from_buffer(const Y4MProbe *probe, const uint8_t *base, int frame_index, uint8_t **Y, uint8_t **U, uint8_t **V);

#ifdef TINYY4M_IMPLEMENTATION

int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char) * s1 - (unsigned char) * s2;
}

int my_strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0)
        return 0;

    while (--n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char) * s1 - (unsigned char) * s2;
}

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

TINYY4MDEF int y4m_write_frame(Y4MWriter *w, uint32_t *pixels) {
    if (!w || !w->f) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "writer/file is NULL");
        return 1;
    }
    if (!pixels) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "pixels buffer is NULL");
        return 1;
    }
    if (fprintf(w->f, "FRAME\n") < 0) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to write frame header");
        return 1;
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
        return 1;
    }
    TINYY4M_TRACELOG(TINYY4M_LOG_TRACE, "wrote %zu bytes (YUV444)", 3 * N);
    return 0;
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
}

TINYY4MDEF int _y4m_read_line(FILE *f, char *buf, size_t cap) {
    if (!fgets(buf, (int)cap, f)) return 1;
    return 0;
}

TINYY4MDEF int _y4m_starts_with_frame(const char *s) {
    return (s[0] == 'F' && s[1] == 'R' && s[2] == 'A' && s[3] == 'M' && s[4] == 'E') ? 1 : 0;
}

TINYY4MDEF int _y4m_parse_header_line(const char *line, int *W, int *H, int *Fnum, int *Fden, char *I, int *A_num, int *A_den, char *Cbuf, size_t Cbuf_sz) {
    if (my_strncmp(line, "YUV4MPEG2", 9) != 0) return 1;

    *W = *H = *Fnum = *Fden = 0;
    *I = 0;
    *A_num = *A_den = 0;
    if (Cbuf && Cbuf_sz) Cbuf[0] = '\0';

    const char *p = line + 9;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '\n' || *p == '\r') break;

        if (*p == 'W') {
            ++p;
            *W = (int)strtol(p, (char**)&p, 10);
        } else if (*p == 'H') {
            ++p;
            *H = (int)strtol(p, (char**)&p, 10);
        } else if (*p == 'F') {
            ++p;
            *Fnum = (int)strtol(p, (char**)&p, 10);
            if (*p == ':') {
                ++p;
                *Fden = (int)strtol(p, (char**)&p, 10);
            }
        } else if (*p == 'I') {
            ++p;
            *I = *p ? *p++ : 0;
        } else if (*p == 'A') {
            ++p;
            *A_num = (int)strtol(p, (char**)&p, 10);
            if (*p == ':') {
                ++p;
                *A_den = (int)strtol(p, (char**)&p, 10);
            }
        } else if (*p == 'C') {
            ++p;
            size_t n = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && n + 1 < Cbuf_sz) {
                Cbuf[n++] = *p++;
            }
            if (Cbuf && Cbuf_sz) Cbuf[n] = '\0';
        } else {
            /* skip unknown token */
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
        }
    }
    return 0;
}

TINYY4MDEF int y4m_probe_file(const char *filename, Y4MProbe *probe) {
    if (!filename || !probe) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "y4m_probe_file: bad args");
        return 1;
    }

    FILE *f = fopen(filename, "rb");
    if (!f) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to open '%s'", filename);
        return 1;
    }

    char line[1024];
    if (_y4m_read_line(f, line, sizeof(line))) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to read header line");
        fclose(f);
        return 1;
    }

    int W, H, Fnum, Fden, A_num, A_den;
    char I, Cbuf[32];
    if (_y4m_parse_header_line(line, &W, &H, &Fnum, &Fden, &I, &A_num, &A_den, Cbuf, sizeof(Cbuf))) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "bad header (no YUV4MPEG2 magic)");
        fclose(f);
        return 1;
    }

    if (W <= 0 || H <= 0 || Fnum <= 0 || Fden <= 0) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "invalid WH/F (%d x %d, F=%d:%d)", W, H, Fnum, Fden);
        fclose(f);
        return 1;
    }
    if (!(I == 'p' || I == 'P')) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "only progressive (Ip) supported, got I%c", I ? I : '?');
        fclose(f);
        return 1;
    }
    if (!(A_num == 1 && A_den == 1)) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "only A1:1 supported, got A%d:%d", A_num, A_den);
        fclose(f);
        return 1;
    }
    /* Accept "C444" or sometimes parsers store just "444" after C token */
    if (!(my_strcmp(Cbuf, "444") == 0 || my_strcmp(Cbuf, "C444") == 0)) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "only C444 supported, got '%s'", Cbuf);
        fclose(f);
        return 1;
    }
    if (Fden != 1) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "only integer fps (F%d:1) supported", Fnum);
        fclose(f);
        return 1;
    }

    long after_header_pos = ftell(f);
    if (after_header_pos < 0) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "ftell after header failed");
        fclose(f);
        return 1;
    }

    const size_t planeN = (size_t)W * (size_t)H;
    const size_t frame_size = planeN * 3u;

    int frames = 0;
    for (;;) {
        if (_y4m_read_line(f, line, sizeof(line))) break; /* EOF reached */
        /* Ignore blank lines */
        int blank = 1;
        for (char *p = line; *p; ++p) if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                blank = 0;
                break;
            }
        if (blank) continue;

        if (!_y4m_starts_with_frame(line)) {
            TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "unexpected line where FRAME expected");
            fclose(f);
            return 1;
        }
        if (fseek(f, (long)frame_size, SEEK_CUR) != 0) {
            TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "truncated frame while counting");
            fclose(f);
            return 1;
        }
        frames++;
    }

    fclose(f);

    probe->width = W;
    probe->height = H;
    probe->fps = Fnum;
    probe->frames = frames;
    probe->frame_size = frame_size;
    probe->total_bytes = (size_t)frames * frame_size;
    probe->data_offset = after_header_pos;

    TINYY4M_TRACELOG(TINYY4M_LOG_INFO, "probe '%s': %dx%d F%d:1 Ip A1:1 C444, frames=%d, bytes=%zu", filename, W, H, Fnum, frames, probe->total_bytes);
    return 0;
}

TINYY4MDEF int y4m_read_into_buffer(const char *filename, const Y4MProbe *probe, uint8_t *dst, size_t dst_size) {
    if (!filename || !probe || !dst) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "y4m_read_into_buffer: bad args");
        return 1;
    }
    if (probe->total_bytes > dst_size) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "buffer too small: need %zu, have %zu", probe->total_bytes, dst_size);
        return 1;
    }

    FILE *f = fopen(filename, "rb");
    if (!f) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "failed to open '%s'", filename);
        return 1;
    }

    if (fseek(f, probe->data_offset, SEEK_SET) != 0) {
        TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "fseek to data offset failed");
        fclose(f);
        return 1;
    }

    char line[1024];
    uint8_t *p = dst;
    const size_t frame_size = probe->frame_size;

    for (int i = 0; i < probe->frames; ++i) {
        if (_y4m_read_line(f, line, sizeof(line))) {
            TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "unexpected EOF before frame %d", i);
            fclose(f);
            return 1;
        }
        if (!_y4m_starts_with_frame(line)) {
            TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "expected FRAME header at frame %d", i);
            fclose(f);
            return 1;
        }
        size_t got = fread(p, 1, frame_size, f);
        if (got != frame_size) {
            TINYY4M_TRACELOG(TINYY4M_LOG_ERROR, "short read on frame %d (%zu/%zu)", i, got, frame_size);
            fclose(f);
            return 1;
        }
        p += frame_size;
    }

    fclose(f);
    TINYY4M_TRACELOG(TINYY4M_LOG_INFO, "read '%s' into buffer (%d frames, %zu bytes)", filename, probe->frames, probe->total_bytes);
    return 0;
}

TINYY4MDEF int y4m_planes_from_buffer(const Y4MProbe *probe, const uint8_t *base, int frame_index, uint8_t **Y, uint8_t **U, uint8_t **V) {
    if (!probe || !base || frame_index < 0 || frame_index >= probe->frames) return 1;
    const size_t planeN = (size_t)probe->width * (size_t)probe->height;
    const uint8_t *frame = base + (size_t)frame_index * probe->frame_size;
    if (Y) *Y = (uint8_t * )(frame);
    if (U) *U = (uint8_t * )(frame + planeN);
    if (V) *V = (uint8_t * )(frame + 2 * planeN);
    return 0;
}

#endif // TINYY4M_IMPLEMENTATION

#endif // TINYY4M_H_