/* WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit
 * given), https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/launcher/bink_decode.c at b2521fe.
 * WWE '12 titantron movies have the same layout (Bink 1 "BIKi", 320 x 320, 30 fps). */
/* WWE SmackDown vs. Raw 2011 - PC port launcher: Bink 1 video reader.
 *
 * Reads the game's titantron movies (to reuse a superstar's bottom strip)
 * and entrance movies made earlier (to make them again). Bink version 'i'
 * only (what the game uses): video, planes Y, V, U, and an alpha plane before them when the
 * movie has one (WWE 2K14's entrance plates; WWE12-PC addition, as FFmpeg reads it).
 *
 * Ported from FFmpeg's libavcodec/bink.c and binkdsp.c
 * (Copyright (c) 2009 Konstantin Shishkov, Copyright (C) 2011 Peter Ross).
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version. It is distributed WITHOUT ANY WARRANTY;
 * see the GNU Lesser General Public License for more details. */
#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#include <windows.h>
#else
/* (the Android launcher: plain C library calls for the Windows ones) */
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#define swprintf_s swprintf
#define _fseeki64 fseeko
#include <malloc.h>
#define _msize malloc_usable_size
static int _wfopen_s(FILE **f, const wchar_t *path, const wchar_t *mode)
{
    char p[4096], m[8];
    if (wcstombs(p, path, sizeof p) == (size_t)-1 || wcstombs(m, mode, sizeof m) == (size_t)-1) return 1;
    *f = fopen(p, m);
    return *f == NULL;
}
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bink_decode.h"
#include "bink_tables.h"

/* ── bits, least significant first ── */

typedef struct { const uint8_t *p; size_t size_bits, pos; } Gb;

static unsigned gb_bits(Gb *g, int n)
{
    unsigned v = 0;
    int i;
    for (i = 0; i < n; i++, g->pos++)
        if (g->pos < g->size_bits && (g->p[g->pos >> 3] >> (g->pos & 7)) & 1) v |= 1u << i;
    return v;
}
static unsigned gb_bit(Gb *g) { return gb_bits(g, 1); }
static long gb_left(const Gb *g) { return (long)g->size_bits - (long)g->pos; }

/* ── data ── */

enum { SRC_BLOCK_TYPES, SRC_SUB_BLOCK_TYPES, SRC_COLORS, SRC_PATTERN, SRC_X_OFF, SRC_Y_OFF,
       SRC_INTRA_DC, SRC_INTER_DC, SRC_RUN, NB_SRC };
enum { SKIP_BLOCK, SCALED_BLOCK, MOTION_BLOCK, RUN_BLOCK, RESIDUE_BLOCK, INTRA_BLOCK, FILL_BLOCK,
       INTER_BLOCK, PATTERN_BLOCK, RAW_BLOCK };

typedef struct { int num; uint8_t syms[16]; } Tree;
typedef struct { int len; Tree tree; uint8_t *data, *data_end, *cur_dec, *cur_ptr; } Bundle;

struct BinkReader {
    FILE *f;
    int w, h, frames, pos;
    LONGLONG frame_time;
    uint32_t *index;              /* frame offsets (low bit: key frame), then the file size */
    uint8_t *packet;
    uint8_t *plane[4], *last[4];  /* Y, U, V (and alpha): current and previous frame */
    int pw[4], ph[4];
    int planes;                   /* 3, or 4 with an alpha plane */
    Bundle bundle[NB_SRC];
    uint8_t *bundle_mem;
    Tree col_high[16];
    int col_lastval;
};

/* A symbol of Huffman tree `t` (bink_tree_bits/lens hold LSB-first codes). */
static int huff(Gb *g, const Tree *t)
{
    unsigned code = 0;
    int len, i;
    for (len = 1; len <= 16; len++) {
        code |= gb_bit(g) << (len - 1);
        for (i = 0; i < 16; i++)
            if (bink_tree_lens[t->num][i] == len && bink_tree_bits[t->num][i] == code) return t->syms[i];
    }
    return 0;
}

static void merge(Gb *g, uint8_t *dst, uint8_t *src, int size)
{
    uint8_t *src2 = src + size;
    int size2 = size;
    do {
        if (!gb_bit(g)) { *dst++ = *src++; size--; }
        else            { *dst++ = *src2++; size2--; }
    } while (size && size2);
    while (size--) *dst++ = *src++;
    while (size2--) *dst++ = *src2++;
}

static void read_tree(Gb *g, Tree *t)
{
    uint8_t tmp1[16] = { 0 }, tmp2[16], *in = tmp1, *out = tmp2, *sw;
    int i, k, len;
    t->num = (int)gb_bits(g, 4);
    if (!t->num) {
        for (i = 0; i < 16; i++) t->syms[i] = (uint8_t)i;
        return;
    }
    if (gb_bit(g)) {
        len = (int)gb_bits(g, 3);
        for (i = 0; i <= len; i++) {
            t->syms[i] = (uint8_t)gb_bits(g, 4);
            tmp1[t->syms[i]] = 1;
        }
        for (i = 0; i < 16 && len < 15; i++)
            if (!tmp1[i]) t->syms[++len] = (uint8_t)i;
    } else {
        len = (int)gb_bits(g, 2);
        for (i = 0; i < 16; i++) in[i] = (uint8_t)i;
        for (i = 0; i <= len; i++) {
            int size = 1 << i;
            for (k = 0; k < 16; k += size << 1) merge(g, out + k, in + k, size);
            sw = in; in = out; out = sw;
        }
        memcpy(t->syms, in, 16);
    }
}

static int ilog2(unsigned v) { int r = -1; while (v) { v >>= 1; r++; } return r; }

static void init_lengths(BinkReader *r, int width, int bw)
{
    width = (width + 7) & ~7;
    r->bundle[SRC_BLOCK_TYPES].len = ilog2((unsigned)(width >> 3) + 511) + 1;
    r->bundle[SRC_SUB_BLOCK_TYPES].len = ilog2((unsigned)(width >> 4) + 511) + 1;
    r->bundle[SRC_COLORS].len = ilog2((unsigned)bw * 64 + 511) + 1;
    r->bundle[SRC_INTRA_DC].len = r->bundle[SRC_INTER_DC].len =
        r->bundle[SRC_X_OFF].len = r->bundle[SRC_Y_OFF].len = ilog2((unsigned)(width >> 3) + 511) + 1;
    r->bundle[SRC_PATTERN].len = ilog2(((unsigned)bw << 3) + 511) + 1;
    r->bundle[SRC_RUN].len = ilog2((unsigned)bw * 48 + 511) + 1;
}

static void read_bundle(Gb *g, BinkReader *r, int n)
{
    int i;
    if (n == SRC_COLORS) {
        for (i = 0; i < 16; i++) read_tree(g, &r->col_high[i]);
        r->col_lastval = 0;
    }
    if (n != SRC_INTRA_DC && n != SRC_INTER_DC) read_tree(g, &r->bundle[n].tree);
    r->bundle[n].cur_dec = r->bundle[n].cur_ptr = r->bundle[n].data;
}

/* How many values to decode into bundle b now: 0 = none (not needed yet, or
 * the bundle is finished). */
static int check_read(Gb *g, Bundle *b, int *t)
{
    if (!b->cur_dec || b->cur_dec > b->cur_ptr) return 0;
    *t = (int)gb_bits(g, b->len);
    if (!*t) { b->cur_dec = NULL; return 0; }
    return 1;
}

static int read_runs(Gb *g, Bundle *b)
{
    int t, v;
    if (!check_read(g, b, &t)) return 1;
    if (b->cur_dec + t > b->data_end) return 0;
    if (gb_bit(g)) {
        v = (int)gb_bits(g, 4);
        memset(b->cur_dec, v, (size_t)t);
        b->cur_dec += t;
    } else {
        while (t--) *b->cur_dec++ = (uint8_t)huff(g, &b->tree);
    }
    return 1;
}

static int read_motion(Gb *g, Bundle *b)
{
    int t, v, sign;
    if (!check_read(g, b, &t)) return 1;
    if (b->cur_dec + t > b->data_end) return 0;
    if (gb_bit(g)) {
        v = (int)gb_bits(g, 4);
        if (v) { sign = -(int)gb_bit(g); v = (v ^ sign) - sign; }
        memset(b->cur_dec, v, (size_t)t);
        b->cur_dec += t;
    } else {
        while (t--) {
            v = huff(g, &b->tree);
            if (v) { sign = -(int)gb_bit(g); v = (v ^ sign) - sign; }
            *b->cur_dec++ = (uint8_t)v;
        }
    }
    return 1;
}

static int read_block_types(Gb *g, Bundle *b)
{
    static const uint8_t rlelens[4] = { 4, 8, 12, 32 };
    int t, v, last = 0;
    uint8_t *end;
    if (!check_read(g, b, &t)) return 1;
    end = b->cur_dec + t;
    if (end > b->data_end) return 0;
    if (gb_bit(g)) {
        v = (int)gb_bits(g, 4);
        memset(b->cur_dec, v, (size_t)t);
        b->cur_dec += t;
    } else {
        while (b->cur_dec < end) {
            v = huff(g, &b->tree);
            if (v < 12) {
                last = v;
                *b->cur_dec++ = (uint8_t)v;
            } else {
                int run = rlelens[v - 12];
                if (end - b->cur_dec < run) return 0;
                memset(b->cur_dec, last, (size_t)run);
                b->cur_dec += run;
            }
        }
    }
    return 1;
}

static int read_patterns(Gb *g, Bundle *b)
{
    int t, v;
    if (!check_read(g, b, &t)) return 1;
    if (b->cur_dec + t > b->data_end) return 0;
    while (t--) {
        v = huff(g, &b->tree);
        v |= huff(g, &b->tree) << 4;
        *b->cur_dec++ = (uint8_t)v;
    }
    return 1;
}

static int read_colors(Gb *g, Bundle *b, BinkReader *r)
{
    int t, v;
    if (!check_read(g, b, &t)) return 1;
    if (b->cur_dec + t > b->data_end) return 0;
    if (gb_bit(g)) {
        r->col_lastval = huff(g, &r->col_high[r->col_lastval]);
        v = (r->col_lastval << 4) | huff(g, &b->tree);
        memset(b->cur_dec, v, (size_t)t);
        b->cur_dec += t;
    } else {
        while (t--) {
            r->col_lastval = huff(g, &r->col_high[r->col_lastval]);
            v = (r->col_lastval << 4) | huff(g, &b->tree);
            *b->cur_dec++ = (uint8_t)v;
        }
    }
    return 1;
}

static int read_dcs(Gb *g, Bundle *b, int start_bits, int has_sign)
{
    int i, j, len, len2, bsize, sign, v, v2;
    int16_t *dst = (int16_t *)b->cur_dec, *end = (int16_t *)b->data_end;
    if (!check_read(g, b, &len)) return 1;
    v = (int)gb_bits(g, start_bits - has_sign);
    if (v && has_sign) { sign = -(int)gb_bit(g); v = (v ^ sign) - sign; }
    if (end - dst < 1) return 0;
    *dst++ = (int16_t)v;
    len--;
    for (i = 0; i < len; i += 8) {
        len2 = len - i < 8 ? len - i : 8;
        if (end - dst < len2) return 0;
        bsize = (int)gb_bits(g, 4);
        if (bsize) {
            for (j = 0; j < len2; j++) {
                v2 = (int)gb_bits(g, bsize);
                if (v2) { sign = -(int)gb_bit(g); v2 = (v2 ^ sign) - sign; }
                v += v2;
                *dst++ = (int16_t)v;
                if (v < -32768 || v > 32767) return 0;
            }
        } else {
            for (j = 0; j < len2; j++) *dst++ = (int16_t)v;
        }
    }
    b->cur_dec = (uint8_t *)dst;
    return 1;
}

static int get_value(BinkReader *r, int n)
{
    int16_t v;
    if (n < SRC_X_OFF || n == SRC_RUN) return *r->bundle[n].cur_ptr++;
    if (n == SRC_X_OFF || n == SRC_Y_OFF) return (int8_t)*r->bundle[n].cur_ptr++;
    memcpy(&v, r->bundle[n].cur_ptr, 2);
    r->bundle[n].cur_ptr += 2;
    return v;
}

static int read_dct_coeffs(Gb *g, int32_t block[64], int *coef_count_, int coef_idx[64])
{
    int coef_list[128], mode_list[128];
    int i, t, bits, ccoef, mode, sign, list_start = 64, list_end = 64, list_pos, coef_count = 0;
    coef_list[list_end] = 4;  mode_list[list_end++] = 0;
    coef_list[list_end] = 24; mode_list[list_end++] = 0;
    coef_list[list_end] = 44; mode_list[list_end++] = 0;
    coef_list[list_end] = 1;  mode_list[list_end++] = 3;
    coef_list[list_end] = 2;  mode_list[list_end++] = 3;
    coef_list[list_end] = 3;  mode_list[list_end++] = 3;
    for (bits = (int)gb_bits(g, 4) - 1; bits >= 0; bits--) {
        list_pos = list_start;
        while (list_pos < list_end) {
            if (!(mode_list[list_pos] | coef_list[list_pos]) || !gb_bit(g)) { list_pos++; continue; }
            ccoef = coef_list[list_pos];
            mode = mode_list[list_pos];
            switch (mode) {
            case 0:
                coef_list[list_pos] = ccoef + 4;
                mode_list[list_pos] = 1;
                /* fall through */
            case 2:
                if (mode == 2) { coef_list[list_pos] = 0; mode_list[list_pos++] = 0; }
                for (i = 0; i < 4; i++, ccoef++) {
                    if (gb_bit(g)) {
                        coef_list[--list_start] = ccoef;
                        mode_list[list_start] = 3;
                    } else {
                        if (!bits) {
                            t = 1 - ((int)gb_bit(g) << 1);
                        } else {
                            t = (int)gb_bits(g, bits) | 1 << bits;
                            sign = -(int)gb_bit(g);
                            t = (t ^ sign) - sign;
                        }
                        block[bink_scan[ccoef]] = t;
                        coef_idx[coef_count++] = ccoef;
                    }
                }
                break;
            case 1:
                mode_list[list_pos] = 2;
                for (i = 0; i < 3; i++) {
                    ccoef += 4;
                    coef_list[list_end] = ccoef;
                    mode_list[list_end++] = 2;
                }
                break;
            case 3:
                if (!bits) {
                    t = 1 - ((int)gb_bit(g) << 1);
                } else {
                    t = (int)gb_bits(g, bits) | 1 << bits;
                    sign = -(int)gb_bit(g);
                    t = (t ^ sign) - sign;
                }
                block[bink_scan[ccoef]] = t;
                coef_idx[coef_count++] = ccoef;
                coef_list[list_pos] = 0;
                mode_list[list_pos++] = 0;
                break;
            }
        }
    }
    *coef_count_ = coef_count;
    return (int)gb_bits(g, 4);
}

static void unquantize(int32_t block[64], const int32_t quant[64], int coef_count, const int coef_idx[64])
{
    int i;
    block[0] = (int)(block[0] * (uint32_t)quant[0]) >> 11;
    for (i = 0; i < coef_count; i++) {
        int idx = coef_idx[i];
        block[bink_scan[idx]] = (int)(block[bink_scan[idx]] * (uint32_t)quant[idx]) >> 11;
    }
}

static void read_residue(Gb *g, int16_t block[64], int masks_count)
{
    int coef_list[128], mode_list[128], nz_coeff[64];
    int i, sign, mask, ccoef, mode, list_start = 64, list_end = 64, list_pos, nz_count = 0;
    coef_list[list_end] = 4;  mode_list[list_end++] = 0;
    coef_list[list_end] = 24; mode_list[list_end++] = 0;
    coef_list[list_end] = 44; mode_list[list_end++] = 0;
    coef_list[list_end] = 0;  mode_list[list_end++] = 2;
    for (mask = 1 << gb_bits(g, 3); mask; mask >>= 1) {
        for (i = 0; i < nz_count; i++) {
            if (!gb_bit(g)) continue;
            if (block[nz_coeff[i]] < 0) block[nz_coeff[i]] -= (int16_t)mask;
            else                        block[nz_coeff[i]] += (int16_t)mask;
            if (--masks_count < 0) return;
        }
        list_pos = list_start;
        while (list_pos < list_end) {
            if (!(coef_list[list_pos] | mode_list[list_pos]) || !gb_bit(g)) { list_pos++; continue; }
            ccoef = coef_list[list_pos];
            mode = mode_list[list_pos];
            switch (mode) {
            case 0:
                coef_list[list_pos] = ccoef + 4;
                mode_list[list_pos] = 1;
                /* fall through */
            case 2:
                if (mode == 2) { coef_list[list_pos] = 0; mode_list[list_pos++] = 0; }
                for (i = 0; i < 4; i++, ccoef++) {
                    if (gb_bit(g)) {
                        coef_list[--list_start] = ccoef;
                        mode_list[list_start] = 3;
                    } else {
                        nz_coeff[nz_count++] = bink_scan[ccoef];
                        sign = -(int)gb_bit(g);
                        block[bink_scan[ccoef]] = (int16_t)((mask ^ sign) - sign);
                        if (--masks_count < 0) return;
                    }
                }
                break;
            case 1:
                mode_list[list_pos] = 2;
                for (i = 0; i < 3; i++) {
                    ccoef += 4;
                    coef_list[list_end] = ccoef;
                    mode_list[list_end++] = 2;
                }
                break;
            case 3:
                nz_coeff[nz_count++] = bink_scan[ccoef];
                sign = -(int)gb_bit(g);
                block[bink_scan[ccoef]] = (int16_t)((mask ^ sign) - sign);
                coef_list[list_pos] = 0;
                mode_list[list_pos++] = 0;
                if (--masks_count < 0) return;
                break;
            }
        }
    }
}

/* ── inverse DCT (binkdsp.c) ── */

#define A1 2896
#define A2 2217
#define A3 3784
#define A4 -5352
#define MUL(X, Y) ((int)((unsigned)(X) * (Y)) >> 11)
#define IDCT_TRANSFORM(dest, s0, s1, s2, s3, s4, s5, s6, s7, d0, d1, d2, d3, d4, d5, d6, d7, munge, src) { \
    const int a0 = (src)[s0] + (src)[s4];                 \
    const int a1 = (src)[s0] - (src)[s4];                 \
    const int a2 = (src)[s2] + (src)[s6];                 \
    const int a3 = MUL(A1, (src)[s2] - (src)[s6]);        \
    const int a4 = (src)[s5] + (src)[s3];                 \
    const int a5 = (src)[s5] - (src)[s3];                 \
    const int a6 = (src)[s1] + (src)[s7];                 \
    const int a7 = (src)[s1] - (src)[s7];                 \
    const int b0 = a4 + a6;                               \
    const int b1 = MUL(A3, a5 + a7);                      \
    const int b2 = MUL(A4, a5) - b0 + b1;                 \
    const int b3 = MUL(A1, a6 - a4) - b2;                 \
    const int b4 = MUL(A2, a7) + b3 - b1;                 \
    (dest)[d0] = munge(a0 + a2 + b0);                     \
    (dest)[d1] = munge(a1 + a3 - a2 + b2);                \
    (dest)[d2] = munge(a1 - a3 + a2 + b3);                \
    (dest)[d3] = munge(a0 - a2 - b4);                     \
    (dest)[d4] = munge(a0 - a2 + b4);                     \
    (dest)[d5] = munge(a1 - a3 + a2 - b3);                \
    (dest)[d6] = munge(a1 + a3 - a2 - b2);                \
    (dest)[d7] = munge(a0 + a2 - b0);                     \
}
#define MUNGE_NONE(x) (x)
#define MUNGE_ROW(x) (((x) + 0x7F) >> 8)

static void idct_col(int *dest, const int32_t *src)
{
    if ((src[8] | src[16] | src[24] | src[32] | src[40] | src[48] | src[56]) == 0) {
        dest[0] = dest[8] = dest[16] = dest[24] = dest[32] = dest[40] = dest[48] = dest[56] = src[0];
    } else {
        IDCT_TRANSFORM(dest, 0, 8, 16, 24, 32, 40, 48, 56, 0, 8, 16, 24, 32, 40, 48, 56, MUNGE_NONE, src);
    }
}

static void idct_put(uint8_t *dest, int stride, const int32_t *block)
{
    int temp[64], i;
    for (i = 0; i < 8; i++) idct_col(&temp[i], &block[i]);
    for (i = 0; i < 8; i++) {
        uint8_t *d = dest + i * stride;
        const int *s = &temp[8 * i];
        IDCT_TRANSFORM(d, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, (uint8_t)MUNGE_ROW, s);
    }
}

static void idct_add(uint8_t *dest, int stride, int32_t *block)
{
    int temp[64], i, j;
    for (i = 0; i < 8; i++) idct_col(&temp[i], &block[i]);
    for (i = 0; i < 8; i++) {
        int *b = &block[8 * i];
        const int *s = &temp[8 * i];
        IDCT_TRANSFORM(b, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, MUNGE_ROW, s);
    }
    for (i = 0; i < 8; i++, dest += stride, block += 8)
        for (j = 0; j < 8; j++) dest[j] = (uint8_t)(dest[j] + block[j]);
}

static void scale_block(const uint8_t src[64], uint8_t *dst, int stride)
{
    int i, j;
    for (j = 0; j < 8; j++, src += 8)
        for (i = 0; i < 8; i++)
            dst[(2 * j) * stride + 2 * i] = dst[(2 * j) * stride + 2 * i + 1] =
                dst[(2 * j + 1) * stride + 2 * i] = dst[(2 * j + 1) * stride + 2 * i + 1] = src[i];
}

static void copy8(uint8_t *dst, const uint8_t *src, int stride)
{
    int i;
    for (i = 0; i < 8; i++) memmove(dst + i * stride, src + i * stride, 8);
}

/* ── planes and frames ── */

static int put_pixels(BinkReader *r, uint8_t *dst, uint8_t *prev, int stride, uint8_t *ref_start, uint8_t *ref_end)
{
    int xoff = get_value(r, SRC_X_OFF), yoff = get_value(r, SRC_Y_OFF);
    uint8_t *ref = prev + xoff + yoff * stride;
    if (ref < ref_start || ref > ref_end) return 0;
    copy8(dst, ref, stride);
    return 1;
}

static int decode_plane(BinkReader *r, Gb *g, int p, int is_chroma)
{
    const int stride = r->pw[p];
    const int bw = is_chroma ? (r->w + 15) >> 4 : (r->w + 7) >> 3;
    const int bh = is_chroma ? (r->h + 15) >> 4 : (r->h + 7) >> 3;
    const int width = r->w >> is_chroma;
    uint8_t *ref_start = r->last[p], *ref_end = ref_start + (bw - 1 + stride * (bh - 1)) * 8;
    int16_t block[64];
    uint8_t ublock[64];
    int32_t dct[64];
    int coordmap[64], coef_count, coef_idx[64], quant;
    int i, j, bx, by, blk, v, col[2];
    const uint8_t *scan;

    init_lengths(r, width > 8 ? width : 8, bw);
    for (i = 0; i < NB_SRC; i++) read_bundle(g, r, i);
    for (i = 0; i < 64; i++) coordmap[i] = (i & 7) + (i >> 3) * stride;

    for (by = 0; by < bh; by++) {
        uint8_t *dst = r->plane[p] + 8 * by * stride, *prev = r->last[p] + 8 * by * stride;
        if (!read_block_types(g, &r->bundle[SRC_BLOCK_TYPES]) || !read_block_types(g, &r->bundle[SRC_SUB_BLOCK_TYPES])
                || !read_colors(g, &r->bundle[SRC_COLORS], r) || !read_patterns(g, &r->bundle[SRC_PATTERN])
                || !read_motion(g, &r->bundle[SRC_X_OFF]) || !read_motion(g, &r->bundle[SRC_Y_OFF])
                || !read_dcs(g, &r->bundle[SRC_INTRA_DC], 11, 0) || !read_dcs(g, &r->bundle[SRC_INTER_DC], 11, 1)
                || !read_runs(g, &r->bundle[SRC_RUN]))
            return 0;
        for (bx = 0; bx < bw; bx++, dst += 8, prev += 8) {
            blk = get_value(r, SRC_BLOCK_TYPES);
            if (((by & 1) || (bx & 1)) && blk == SCALED_BLOCK) {  /* part of a 16x16 block */
                bx++; dst += 8; prev += 8;
                continue;
            }
            switch (blk) {
            case SKIP_BLOCK:
                copy8(dst, prev, stride);
                break;
            case SCALED_BLOCK:
                blk = get_value(r, SRC_SUB_BLOCK_TYPES);
                switch (blk) {
                case RUN_BLOCK:
                    scan = bink_patterns[gb_bits(g, 4)];
                    i = 0;
                    do {
                        int run = get_value(r, SRC_RUN) + 1;
                        i += run;
                        if (i > 64) return 0;
                        if (gb_bit(g)) {
                            v = get_value(r, SRC_COLORS);
                            for (j = 0; j < run; j++) ublock[*scan++] = (uint8_t)v;
                        } else {
                            for (j = 0; j < run; j++) ublock[*scan++] = (uint8_t)get_value(r, SRC_COLORS);
                        }
                    } while (i < 63);
                    if (i == 63) ublock[*scan++] = (uint8_t)get_value(r, SRC_COLORS);
                    break;
                case INTRA_BLOCK:
                    memset(dct, 0, sizeof dct);
                    dct[0] = get_value(r, SRC_INTRA_DC);
                    quant = read_dct_coeffs(g, dct, &coef_count, coef_idx);
                    unquantize(dct, bink_intra_quant[quant], coef_count, coef_idx);
                    idct_put(ublock, 8, dct);
                    break;
                case FILL_BLOCK:
                    v = get_value(r, SRC_COLORS);
                    for (i = 0; i < 16; i++) memset(dst + i * stride, v, 16);
                    break;
                case PATTERN_BLOCK:
                    for (i = 0; i < 2; i++) col[i] = get_value(r, SRC_COLORS);
                    for (j = 0; j < 8; j++) {
                        v = get_value(r, SRC_PATTERN);
                        for (i = 0; i < 8; i++, v >>= 1) ublock[i + j * 8] = (uint8_t)col[v & 1];
                    }
                    break;
                case RAW_BLOCK:
                    for (j = 0; j < 64; j++) ublock[j] = (uint8_t)get_value(r, SRC_COLORS);
                    break;
                default:
                    return 0;
                }
                if (blk != FILL_BLOCK) scale_block(ublock, dst, stride);
                bx++; dst += 8; prev += 8;
                break;
            case MOTION_BLOCK:
                if (!put_pixels(r, dst, prev, stride, ref_start, ref_end)) return 0;
                break;
            case RUN_BLOCK:
                scan = bink_patterns[gb_bits(g, 4)];
                i = 0;
                do {
                    int run = get_value(r, SRC_RUN) + 1;
                    i += run;
                    if (i > 64) return 0;
                    if (gb_bit(g)) {
                        v = get_value(r, SRC_COLORS);
                        for (j = 0; j < run; j++) dst[coordmap[*scan++]] = (uint8_t)v;
                    } else {
                        for (j = 0; j < run; j++) dst[coordmap[*scan++]] = (uint8_t)get_value(r, SRC_COLORS);
                    }
                } while (i < 63);
                if (i == 63) dst[coordmap[*scan++]] = (uint8_t)get_value(r, SRC_COLORS);
                break;
            case RESIDUE_BLOCK:
                if (!put_pixels(r, dst, prev, stride, ref_start, ref_end)) return 0;
                memset(block, 0, sizeof block);
                v = (int)gb_bits(g, 7);
                read_residue(g, block, v);
                for (i = 0; i < 8; i++)
                    for (j = 0; j < 8; j++) dst[i * stride + j] = (uint8_t)(dst[i * stride + j] + block[i * 8 + j]);
                break;
            case INTRA_BLOCK:
                memset(dct, 0, sizeof dct);
                dct[0] = get_value(r, SRC_INTRA_DC);
                quant = read_dct_coeffs(g, dct, &coef_count, coef_idx);
                unquantize(dct, bink_intra_quant[quant], coef_count, coef_idx);
                idct_put(dst, stride, dct);
                break;
            case FILL_BLOCK:
                v = get_value(r, SRC_COLORS);
                for (i = 0; i < 8; i++) memset(dst + i * stride, v, 8);
                break;
            case INTER_BLOCK:
                if (!put_pixels(r, dst, prev, stride, ref_start, ref_end)) return 0;
                memset(dct, 0, sizeof dct);
                dct[0] = get_value(r, SRC_INTER_DC);
                quant = read_dct_coeffs(g, dct, &coef_count, coef_idx);
                unquantize(dct, bink_inter_quant[quant], coef_count, coef_idx);
                idct_add(dst, stride, dct);
                break;
            case PATTERN_BLOCK:
                for (i = 0; i < 2; i++) col[i] = get_value(r, SRC_COLORS);
                for (i = 0; i < 8; i++) {
                    v = get_value(r, SRC_PATTERN);
                    for (j = 0; j < 8; j++, v >>= 1) dst[i * stride + j] = (uint8_t)col[v & 1];
                }
                break;
            case RAW_BLOCK:
                for (i = 0; i < 8; i++) memcpy(dst + i * stride, r->bundle[SRC_COLORS].cur_ptr + i * 8, 8);
                r->bundle[SRC_COLORS].cur_ptr += 64;
                break;
            default:
                return 0;
            }
        }
    }
    if (g->pos & 31) g->pos += 32 - (g->pos & 31);   /* planes start at 32-bit boundaries */
    return 1;
}

BinkReader *bink_open(const WCHAR *path, WCHAR *err, size_t errn)
{
    BinkReader *r = (BinkReader *)calloc(1, sizeof *r);
    uint32_t hdr[11], audio;
    size_t max_frame, blocks;
    int i;
    if (!r) return NULL;
    if (_wfopen_s(&r->f, path, L"rb") || !r->f || fread(hdr, 4, 11, r->f) != 11) {
        swprintf_s(err, errn, L"Could not read %s.", path);
        goto fail;
    }
    if (memcmp(hdr, "BIKi", 4) || !hdr[2] || hdr[2] > 1000000 || hdr[5] == 0 || hdr[5] > 4096
            || hdr[6] == 0 || hdr[6] > 4096 || !hdr[7] || !hdr[8]) {
        swprintf_s(err, errn, L"%s is not a Bink movie the game uses.", path);
        goto fail;
    }
    r->planes = (hdr[9] & 0x00100000) ? 4 : 3;
    audio = hdr[10];
    if (audio) _fseeki64(r->f, (long long)audio * 12, SEEK_CUR);   /* per track: 4 + 4 + 4 bytes */
    r->frames = (int)hdr[2];
    r->w = (int)hdr[5];
    r->h = (int)hdr[6];
    r->frame_time = (LONGLONG)10000000 * hdr[8] / hdr[7];
    max_frame = hdr[3];
    r->index = (uint32_t *)malloc(sizeof(uint32_t) * ((size_t)r->frames + 1));
    r->packet = (uint8_t *)malloc(max_frame + 8);
    if (!r->index || !r->packet || fread(r->index, 4, (size_t)r->frames, r->f) != (size_t)r->frames) {
        swprintf_s(err, errn, L"%s is damaged.", path);
        goto fail;
    }
    r->index[r->frames] = hdr[1] + 8;              /* the last frame ends the file */
    r->pw[0] = (r->w + 7) & ~7; r->ph[0] = (r->h + 7) & ~7;
    r->pw[1] = r->pw[2] = ((r->w + 15) & ~15) / 2;
    r->ph[1] = r->ph[2] = ((r->h + 15) & ~15) / 2;
    r->pw[3] = r->pw[0]; r->ph[3] = r->ph[0];
    for (i = 0; i < r->planes; i++) {
        r->plane[i] = (uint8_t *)calloc((size_t)r->pw[i] * r->ph[i] + 64, 1);
        r->last[i] = (uint8_t *)calloc((size_t)r->pw[i] * r->ph[i] + 64, 1);
        if (!r->plane[i] || !r->last[i]) goto nomem;
    }
    blocks = (size_t)((r->w + 7) >> 3) * ((r->h + 7) >> 3);
    r->bundle_mem = (uint8_t *)calloc(blocks, 64 * NB_SRC);
    if (!r->bundle_mem) goto nomem;
    for (i = 0; i < NB_SRC; i++) {
        r->bundle[i].data = r->bundle_mem + blocks * 64 * i;
        r->bundle[i].data_end = r->bundle[i].data + blocks * 64;
    }
    return r;
nomem:
    swprintf_s(err, errn, L"Out of memory.");
fail:
    bink_close(r);
    return NULL;
}

void bink_close(BinkReader *r)
{
    int i;
    if (!r) return;
    if (r->f) fclose(r->f);
    free(r->index); free(r->packet); free(r->bundle_mem);
    for (i = 0; i < 4; i++) { free(r->plane[i]); free(r->last[i]); }
    free(r);
}

int bink_width(const BinkReader *r) { return r->w; }
int bink_height(const BinkReader *r) { return r->h; }
int bink_frames(const BinkReader *r) { return r->frames; }
LONGLONG bink_frame_time(const BinkReader *r) { return r->frame_time; }
int bink_position(const BinkReader *r) { return r->pos; }
int bink_has_alpha(const BinkReader *r) { return r->planes == 4; }
void bink_rewind(BinkReader *r)
{
    int i;
    r->pos = 0;
    for (i = 0; i < r->planes; i++) {
        memset(r->last[i], 0, (size_t)r->pw[i] * r->ph[i]);
        memset(r->plane[i], 0, (size_t)r->pw[i] * r->ph[i]);
    }
}

int bink_next(BinkReader *r)
{
    uint32_t start, end, size;
    Gb g;
    int k, i;
    if (r->pos >= r->frames) bink_rewind(r);         /* start over */
    start = r->index[r->pos] & ~1u;
    end = r->index[r->pos + 1] & ~1u;
    r->pos++;
    size = end - start;
    for (i = 0; i < r->planes; i++) {                /* this frame builds on the last one */
        uint8_t *t = r->last[i]; r->last[i] = r->plane[i]; r->plane[i] = t;
        memcpy(r->plane[i], r->last[i], (size_t)r->pw[i] * r->ph[i]);
    }
    if (end <= start || size > 64u * 1024 * 1024) return 0;
    if (size > (uint32_t)_msize(r->packet) - 8) {
        uint8_t *p = (uint8_t *)realloc(r->packet, (size_t)size + 8);
        if (!p) return 0;
        r->packet = p;
    }
    if (_fseeki64(r->f, start, SEEK_SET) || fread(r->packet, 1, size, r->f) != size) return 0;
    memset(r->packet + size, 0, 8);
    g.p = r->packet; g.size_bits = (size_t)size * 8; g.pos = 32;   /* 'i': 32 bits before the planes */
    if (r->planes == 4) {                            /* the alpha plane first, then 32 bits again */
        if (!decode_plane(r, &g, 3, 0)) return 0;
        g.pos += 32;
    }
    for (k = 0; k < 3; k++) {
        const int p = k == 0 ? 0 : (k ^ 3);          /* Y, V, U */
        if (!decode_plane(r, &g, p, k != 0)) return 0;
        if ((long)g.pos >= (long)g.size_bits) break;
    }
    return 1;
}

/* BT.601 studio range -> BGRA. */
void bink_bgra(const BinkReader *r, uint8_t *bgra)
{
    int x, y;
    for (y = 0; y < r->h; y++)
        for (x = 0; x < r->w; x++) {
            const int Y = r->plane[0][y * r->pw[0] + x] - 16;
            const int U = r->plane[1][(y >> 1) * r->pw[1] + (x >> 1)] - 128;
            const int V = r->plane[2][(y >> 1) * r->pw[2] + (x >> 1)] - 128;
            int R = (298 * Y + 409 * V + 128) >> 8;
            int G = (298 * Y - 100 * U - 208 * V + 128) >> 8;
            int B = (298 * Y + 516 * U + 128) >> 8;
            uint8_t *o = bgra + ((size_t)y * r->w + x) * 4;
            o[0] = (uint8_t)(B < 0 ? 0 : B > 255 ? 255 : B);
            o[1] = (uint8_t)(G < 0 ? 0 : G > 255 ? 255 : G);
            o[2] = (uint8_t)(R < 0 ? 0 : R > 255 ? 255 : R);
            o[3] = r->planes == 4 ? r->plane[3][y * r->pw[3] + x] : 255;
        }
}
