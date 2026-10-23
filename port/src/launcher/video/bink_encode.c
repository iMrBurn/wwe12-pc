/* WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit
 * given), https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/launcher/bink_encode.c at b2521fe.
 * WWE '12 titantron movies have the same layout (Bink 1 "BIKi", 320 x 320, 30 fps). */
/* WWE SmackDown vs. Raw 2011 - Bink 1 ("BIKi") encoder for the USER MOVIES
 * maker: the PC launcher (movie_maker.c) and the Android launcher (JNI)
 * write their movies with it. 320 x 320, 30 fps, no audio; three block
 * types (raw 8 x 8, fill, skip) with fixed 4-bit codes (tree 0) - see
 * ffmpeg's libavcodec/bink.c for the format. Plain C. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bink_encode.h"

#define FPS BINK_ENC_FPS

#ifdef _WIN32
#define bink_tell _ftelli64
#define bink_seek _fseeki64
#else
#define bink_tell ftello
#define bink_seek fseeko
#endif

/* ── bits (least significant first, as Bink reads them) ── */

typedef struct { uint8_t *p; size_t n, cap; uint64_t acc; int bits; } Bits;

static int bits_put(Bits *b, uint32_t v, int n)
{
    b->acc |= (uint64_t)(v & ((1u << n) - 1)) << b->bits;
    b->bits += n;
    while (b->bits >= 8) {
        if (b->n == b->cap) {
            size_t cap = b->cap ? b->cap * 2 : 65536;
            uint8_t *p = (uint8_t *)realloc(b->p, cap);
            if (!p) return 0;
            b->p = p; b->cap = cap;
        }
        b->p[b->n++] = (uint8_t)b->acc;
        b->acc >>= 8;
        b->bits -= 8;
    }
    return 1;
}

static void bits_align32(Bits *b)
{
    int pad = (int)((32 - ((b->n * 8 + b->bits) & 31)) & 31);
    while (pad > 0) { int k = pad > 16 ? 16 : pad; bits_put(b, 0, k); pad -= k; }
}

static int ilog2(unsigned v) { int r = -1; while (v) { v >>= 1; r++; } return r; }

/* ── one plane of a Bink frame ── */

enum { BLK_SKIP = 0, BLK_FILL = 6, BLK_RAW = 9 };

/* Encodes plane `cur` (w x h, w and h multiples of 8) against `ref` (the
 * picture the decoder has; updated to what it will have after this frame). */
static int encode_plane(Bits *b, const uint8_t *cur, uint8_t *ref, int w, int h, int key, int skip_thr, int fill_thr)
{
    const int bw = w >> 3, bh = h >> 3;
    const int width = w < 8 ? 8 : w;
    const int len_types = ilog2((unsigned)(width >> 3) + 511) + 1;
    const int len_sub = ilog2((unsigned)(width >> 4) + 511) + 1;
    const int len_colors = ilog2((unsigned)bw * 64 + 511) + 1;
    const int len_pattern = ilog2(((unsigned)bw << 3) + 511) + 1;
    const int len_small = ilog2((unsigned)(width >> 3) + 511) + 1;   /* x, y, intra dc, inter dc */
    const int len_run = ilog2((unsigned)bw * 48 + 511) + 1;
    uint8_t *types = (uint8_t *)malloc((size_t)bw * bh);
    uint8_t *vals = (uint8_t *)malloc((size_t)w * h);           /* colour values, block order */
    size_t *row_start = (size_t *)malloc(sizeof(size_t) * ((size_t)bh + 1));
    size_t nv = 0, supplied = 0, consumed = 0;
    int by, bx, x, y, i, next_row = 0, colors_null = 0, first = 1;
    if (!types || !vals || !row_start) { free(types); free(vals); free(row_start); return 0; }

    for (by = 0; by < bh; by++) {
        row_start[by] = nv;
        for (bx = 0; bx < bw; bx++) {
            int mn = 255, mx = 0, diff = 0, sum = 0, t;
            for (y = 0; y < 8; y++) {
                const uint8_t *c = cur + (size_t)(by * 8 + y) * w + bx * 8;
                const uint8_t *r = ref + (size_t)(by * 8 + y) * w + bx * 8;
                for (x = 0; x < 8; x++) {
                    int d = c[x] - r[x];
                    if (d < 0) d = -d;
                    if (d > diff) diff = d;
                    if (c[x] < mn) mn = c[x];
                    if (c[x] > mx) mx = c[x];
                    sum += c[x];
                }
            }
            if (!key && diff <= skip_thr) {
                t = BLK_SKIP;
            } else if (mx - mn <= fill_thr) {
                uint8_t v = (uint8_t)((sum + 32) >> 6);
                t = BLK_FILL;
                vals[nv++] = v;
                for (y = 0; y < 8; y++) memset(ref + (size_t)(by * 8 + y) * w + bx * 8, v, 8);
            } else {
                t = BLK_RAW;
                for (y = 0; y < 8; y++) {
                    const uint8_t *c = cur + (size_t)(by * 8 + y) * w + bx * 8;
                    memcpy(vals + nv, c, 8);
                    memcpy(ref + (size_t)(by * 8 + y) * w + bx * 8, c, 8);
                    nv += 8;
                }
            }
            types[by * bw + bx] = (uint8_t)t;
        }
    }
    row_start[bh] = nv;

    /* bundle trees: all "tree 0" (4-bit codes); colours have 16 more */
    bits_put(b, 0, 4);                                  /* block types */
    bits_put(b, 0, 4);                                  /* sub-block types */
    for (i = 0; i < 16; i++) bits_put(b, 0, 4);         /* colour high nibbles */
    bits_put(b, 0, 4);                                  /* colours */
    bits_put(b, 0, 4);                                  /* pattern */
    bits_put(b, 0, 4);                                  /* x offsets */
    bits_put(b, 0, 4);                                  /* y offsets */
    bits_put(b, 0, 4);                                  /* runs (the dc bundles have none) */

    for (by = 0; by < bh; by++) {
        const uint8_t *t = types + by * bw;
        int same = 1;
        for (bx = 1; bx < bw; bx++) same &= t[bx] == t[0];
        bits_put(b, (uint32_t)bw, len_types);
        if (same) {
            bits_put(b, 1, 1);
            bits_put(b, t[0], 4);
        } else {
            bits_put(b, 0, 1);
            for (bx = 0; bx < bw; bx++) bits_put(b, t[bx], 4);
        }
        if (first) bits_put(b, 0, len_sub);
        /* colours: the decoder reads a batch only once it has used the last */
        if (!colors_null && supplied == consumed) {
            int r = next_row > by ? next_row : by;
            while (r < bh && row_start[r + 1] == row_start[r]) r++;
            if (r == bh) {
                bits_put(b, 0, len_colors);
                colors_null = 1;
            } else {
                size_t k, n = row_start[r + 1] - row_start[r];
                bits_put(b, (uint32_t)n, len_colors);
                bits_put(b, 0, 1);
                for (k = row_start[r]; k < row_start[r + 1]; k++) {
                    bits_put(b, vals[k] >> 4, 4);
                    bits_put(b, vals[k] & 15, 4);
                }
                supplied += n;
                next_row = r + 1;
            }
        }
        if (first) {
            bits_put(b, 0, len_pattern);
            bits_put(b, 0, len_small);   /* x */
            bits_put(b, 0, len_small);   /* y */
            bits_put(b, 0, len_small);   /* intra dc */
            bits_put(b, 0, len_small);   /* inter dc */
            bits_put(b, 0, len_run);
            first = 0;
        }
        consumed += row_start[by + 1] - row_start[by];
    }
    bits_align32(b);
    free(types); free(vals); free(row_start);
    return 1;
}

/* BGRA -> Y, U, V (4:2:0, BT.601 studio range as Bink's decoder expects). */
static void to_yuv(const uint8_t *bgra, uint8_t *Y, uint8_t *U, uint8_t *V, int MOVIE_W, int MOVIE_H)
{
    int x, y;
    for (y = 0; y < MOVIE_H; y++)
        for (x = 0; x < MOVIE_W; x++) {
            const uint8_t *s = bgra + ((size_t)y * MOVIE_W + x) * 4;
            Y[y * MOVIE_W + x] = (uint8_t)((66 * s[2] + 129 * s[1] + 25 * s[0] + 128) / 256 + 16);
        }
    for (y = 0; y < MOVIE_H / 2; y++)
        for (x = 0; x < MOVIE_W / 2; x++) {
            int r = 0, g = 0, b = 0, i, j;
            for (j = 0; j < 2; j++)
                for (i = 0; i < 2; i++) {
                    const uint8_t *s = bgra + ((size_t)(y * 2 + j) * MOVIE_W + x * 2 + i) * 4;
                    b += s[0]; g += s[1]; r += s[2];
                }
            r /= 4; g /= 4; b /= 4;
            U[y * (MOVIE_W / 2) + x] = (uint8_t)((-38 * r - 74 * g + 112 * b + 128) / 256 + 128);
            V[y * (MOVIE_W / 2) + x] = (uint8_t)((112 * r - 94 * g - 18 * b + 128) / 256 + 128);
        }
}

struct BinkWriter {
    FILE *f;
    int frames, done, w, h;
    uint8_t *same; size_t same_n;  /* bink_writer_repeat's frame */
    uint8_t *Y, *U, *V, *rY, *rU, *rV;
    uint32_t *offs, largest;
    Bits b;
};

static int bink_writer_frame_yuv(BinkWriter *w);

void bink_writer_free(BinkWriter *w)
{
    if (!w) return;
    free(w->b.p); free(w->same); free(w->Y); free(w->U); free(w->V); free(w->rY); free(w->rU); free(w->rV); free(w->offs);
    free(w);
}

BinkWriter *bink_writer_open(FILE *f, int frames)
{
    return bink_writer_open_size(f, frames, BINK_ENC_W, BINK_ENC_H);
}

BinkWriter *bink_writer_open_size(FILE *f, int frames, int MOVIE_W, int MOVIE_H)
{
    BinkWriter *w;
    if (MOVIE_W < 16 || MOVIE_H < 16 || MOVIE_W > 1024 || MOVIE_H > 1024 || MOVIE_W % 16 || MOVIE_H % 16) return NULL;
    w = (BinkWriter *)calloc(1, sizeof *w);
    if (!w) return NULL;
    w->f = f;
    w->frames = frames;
    w->w = MOVIE_W;
    w->h = MOVIE_H;
    w->Y = (uint8_t *)malloc(MOVIE_W * MOVIE_H); w->rY = (uint8_t *)calloc(MOVIE_W * MOVIE_H, 1);
    w->U = (uint8_t *)malloc(MOVIE_W * MOVIE_H / 4); w->rU = (uint8_t *)calloc(MOVIE_W * MOVIE_H / 4, 1);
    w->V = (uint8_t *)malloc(MOVIE_W * MOVIE_H / 4); w->rV = (uint8_t *)calloc(MOVIE_W * MOVIE_H / 4, 1);
    w->offs = (uint32_t *)malloc(sizeof(uint32_t) * ((size_t)frames + 1));
    if (!w->Y || !w->U || !w->V || !w->rY || !w->rU || !w->rV || !w->offs) { bink_writer_free(w); return NULL; }
    {   /* header and frame index, filled in at the end */
        uint8_t zero[64] = { 0 };
        size_t k, head = 44 + 4 * ((size_t)frames + 1);
        for (k = 0; k < head; k += sizeof zero) fwrite(zero, 1, head - k < sizeof zero ? head - k : sizeof zero, f);
    }
    return w;
}

int bink_writer_frame(BinkWriter *w, const uint8_t *bgra)
{
    if (bgra) to_yuv(bgra, w->Y, w->U, w->V, w->w, w->h);
    return bink_writer_frame_yuv(w);
}

int bink_writer_repeat(BinkWriter *w)
{
    size_t pos;
    if (w->done == 0) return 0;
    if (!w->same) {  /* (the first repeat: encoded - every block skips - and kept) */
        const int r = bink_writer_frame_yuv(w);
        if (r != 1) return r;
        w->same = (uint8_t *)malloc(w->b.n);
        if (!w->same) return 0;
        memcpy(w->same, w->b.p, w->b.n);
        w->same_n = w->b.n;
        return 1;
    }
    pos = (size_t)bink_tell(w->f);
    w->offs[w->done] = (uint32_t)pos;
    if (fwrite(w->same, 1, w->same_n, w->f) != w->same_n) return -1;
    w->done++;
    return 1;
}

static int bink_writer_frame_yuv(BinkWriter *w)
{
    const int MOVIE_W = w->w, MOVIE_H = w->h;
    Bits *b = &w->b;
    size_t pos, ysize;
    const int key = w->done == 0;
    b->n = 0; b->bits = 0; b->acc = 0;
    bits_put(b, 0, 32);                                          /* size of the Y plane, below */
    if (!encode_plane(b, w->Y, w->rY, MOVIE_W, MOVIE_H, key, 2, 2)) return 0;
    ysize = b->n;
    if (!encode_plane(b, w->V, w->rV, MOVIE_W / 2, MOVIE_H / 2, key, 2, 2)) return 0;   /* Bink 'i': V before U */
    if (!encode_plane(b, w->U, w->rU, MOVIE_W / 2, MOVIE_H / 2, key, 2, 2)) return 0;
    b->p[0] = (uint8_t)ysize; b->p[1] = (uint8_t)(ysize >> 8); b->p[2] = (uint8_t)(ysize >> 16); b->p[3] = (uint8_t)(ysize >> 24);
    pos = (size_t)bink_tell(w->f);
    w->offs[w->done] = (uint32_t)pos | (key ? 1u : 0u);
    if (fwrite(b->p, 1, b->n, w->f) != b->n) return -1;
    if (b->n > w->largest) w->largest = (uint32_t)b->n;
    w->done++;
    return 1;
}

int bink_writer_close(BinkWriter *w)
{
    const int n = w->frames;
    uint32_t hdr[11];
    w->offs[n] = (uint32_t)bink_tell(w->f);
    memcpy(hdr, "BIKi", 4);
    hdr[1] = w->offs[n] - 8; hdr[2] = (uint32_t)n; hdr[3] = w->largest; hdr[4] = (uint32_t)n;
    hdr[5] = (uint32_t)w->w; hdr[6] = (uint32_t)w->h; hdr[7] = FPS; hdr[8] = 1; hdr[9] = 0; hdr[10] = 0;
    bink_seek(w->f, 0, SEEK_SET);
    fwrite(hdr, 4, 11, w->f);
    fwrite(w->offs, 4, (size_t)n + 1, w->f);
    bink_writer_free(w);
    return 1;
}
