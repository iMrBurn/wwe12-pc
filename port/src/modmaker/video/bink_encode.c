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
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

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

enum { BLK_SKIP = 0, BLK_MOTION = 2, BLK_FILL = 6, BLK_PATTERN = 8, BLK_RAW = 9 };

/* WWE12-PC: the 8 x 8 block at `c` (row stride w) as two colours (col[0], col[1]) and 8 pattern rows (bit x of row y:
 * pixel (x, y) takes col[1]), when no pixel is further than thr from its colour: 1, else 0. */
static int pattern_fit(const uint8_t *c, int w, int thr, uint8_t *col, uint8_t *pat)
{
    int x, y, it, lo = 255, hi = 0, mid;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            const int v = c[(size_t)y * w + x];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    for (it = 0; it < 3; it++) {  /* two-means */
        int s0 = 0, n0 = 0, s1 = 0, n1 = 0;
        mid = (lo + hi + 1) / 2;
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                const int v = c[(size_t)y * w + x];
                if (v >= mid) { s1 += v; n1++; } else { s0 += v; n0++; }
            }
        if (n0) lo = (s0 + n0 / 2) / n0;
        if (n1) hi = (s1 + n1 / 2) / n1;
    }
    mid = (lo + hi + 1) / 2;
    for (y = 0; y < 8; y++) {
        int row = 0;
        for (x = 0; x < 8; x++) {
            const int v = c[(size_t)y * w + x], one = v >= mid, d = v - (one ? hi : lo);
            if (d > thr || d < -thr) return 0;
            row |= one << x;
        }
        pat[y] = (uint8_t)row;
    }
    col[0] = (uint8_t)lo;
    col[1] = (uint8_t)hi;
    return 1;
}

/* Encodes plane `cur` (w x h, w and h multiples of 8) against `ref` (the
 * picture the decoder has; updated to what it will have after this frame). */
/* WWE12-PC: whether the 8 x 8 block at (x, y) of cur matches prev's at (x + dx, y + dy) within thr (both w wide). */
static int block_fits(const uint8_t *cur, const uint8_t *prev, int w, int x, int y, int dx, int dy, int thr)
{
    int i, j;
    for (j = 0; j < 8; j++) {
        const uint8_t *c = cur + (size_t)(y + j) * w + x, *r = prev + (size_t)(y + j + dy) * w + x + dx;
        for (i = 0; i < 8; i++) {
            const int d = c[i] - r[i];
            if (d > thr || d < -thr) return 0;
        }
    }
    return 1;
}

/* WWE12-PC: the motion vectors (each axis -15..15, not 0 0) that fit the most of a sample of the plane's blocks, best
 * first, at most `max` of them in vx / vy: how many. */
static int motion_candidates(const uint8_t *cur, const uint8_t *prev, int w, int h, int thr, int *vx, int *vy, int max)
{
    const int bw = w >> 3, bh = h >> 3, blocks = bw * bh, step = blocks > 192 ? blocks / 192 : 1;
    int counts[31 * 31] = {0}, dx, dy, k, n = 0, i;
    for (k = 0; k < blocks; k += step) {
        const int x = (k % bw) * 8, y = (k / bw) * 8;
        if (block_fits(cur, prev, w, x, y, 0, 0, thr)) continue;  /* (a skip block) */
        for (dy = -15; dy <= 15; dy++)
            for (dx = -15; dx <= 15; dx++)
                if ((dx || dy) && x + dx >= 0 && y + dy >= 0 && x + dx + 8 <= w && y + dy + 8 <= h &&
                    block_fits(cur, prev, w, x, y, dx, dy, thr))
                    counts[(dy + 15) * 31 + dx + 15]++;
    }
    while (n < max) {
        int best = -1;
        for (i = 0; i < 31 * 31; i++)
            if (counts[i] > 0 && (best < 0 || counts[i] > counts[best])) best = i;
        if (best < 0) break;
        vx[n] = best % 31 - 15; vy[n] = best / 31 - 15; n++;
        counts[best] = 0;
    }
    return n;
}

/* WWE12-PC: a bundle's values sent in per-row batches, as the decoder reads them (a batch only once it has used the
 * last; 0: no more). kind 0: 4-bit nibbles of a byte, low first (patterns); 1: signed motion values (4 bits, sign). */
typedef struct { size_t supplied, consumed; int next_row, done; } Batches;

static void batch_put(Bits *b, Batches *s, const int *vals, const size_t *row_start, int by, int bh, int len, int kind)
{
    int r;
    if (s->done || s->supplied != s->consumed) return;
    r = s->next_row > by ? s->next_row : by;
    while (r < bh && row_start[r + 1] == row_start[r]) r++;
    if (r == bh) {
        bits_put(b, 0, len);
        s->done = 1;
        return;
    }
    {
        size_t k, n = row_start[r + 1] - row_start[r];
        bits_put(b, (uint32_t)n, len);
        if (kind == 1) bits_put(b, 0, 1);  /* (not all the same) */
        for (k = row_start[r]; k < row_start[r + 1]; k++) {
            const int v = vals[k];
            if (kind == 0) {
                bits_put(b, (uint32_t)v & 15, 4);
                bits_put(b, (uint32_t)v >> 4, 4);
            } else {
                bits_put(b, (uint32_t)(v < 0 ? -v : v), 4);
                if (v) bits_put(b, v < 0, 1);
            }
        }
        s->supplied += n;
        s->next_row = r + 1;
    }
}

/* WWE12-PC (2026-11-02): with pat_thr > 0, a block two colours fit (no pixel further than pat_thr from its colour)
 * is a pattern block (2 colours + 64 bits, the pattern bundle in batches as the colours) instead of raw (64 colours):
 * the arena ribbons and walls at the game's size with all their frames; and a block the previous frame has, moved by
 * one of the plane's likeliest vectors (within skip_thr), is a motion block (its two offsets only). */
static int encode_plane(Bits *b, const uint8_t *cur, uint8_t *ref, int w, int h, int key, int skip_thr, int fill_thr,
                        int pat_thr)
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
    uint8_t *pats = (uint8_t *)malloc((size_t)bw * bh * 8);     /* pattern rows, block order */
    int *patv = (int *)malloc(sizeof(int) * (size_t)bw * bh * 8);
    size_t *prow_start = (size_t *)malloc(sizeof(size_t) * ((size_t)bh + 1));
    int *mx_ = (int *)malloc(sizeof(int) * (size_t)bw * bh), *my_ = (int *)malloc(sizeof(int) * (size_t)bw * bh);
    size_t *mrow_start = (size_t *)malloc(sizeof(size_t) * ((size_t)bh + 1));
    uint8_t *prev = (uint8_t *)malloc((size_t)w * h);           /* the previous frame, as the decoder keeps it */
    size_t nv = 0, supplied = 0, consumed = 0, np = 0, nm = 0;
    int by, bx, x, y, i, next_row = 0, colors_null = 0, first = 1, nvec = 0, vx[8], vy[8];
    Batches pb = {0, 0, 0, 0}, xb = {0, 0, 0, 0}, yb = {0, 0, 0, 0};
    if (!types || !vals || !row_start || !pats || !patv || !prow_start || !mx_ || !my_ || !mrow_start || !prev) {
        free(types); free(vals); free(row_start); free(pats); free(patv); free(prow_start);
        free(mx_); free(my_); free(mrow_start); free(prev); return 0;
    }
    memcpy(prev, ref, (size_t)w * h);
    if (!key && pat_thr > 0) nvec = motion_candidates(cur, prev, w, h, skip_thr, vx, vy, 8);

    for (by = 0; by < bh; by++) {
        row_start[by] = nv;
        prow_start[by] = np;
        mrow_start[by] = nm;
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
            t = -1;
            if (!key && diff <= skip_thr)
                t = BLK_SKIP;
            if (t < 0 && !key && nvec) {
                int k;
                for (k = 0; k < nvec && t < 0; k++) {
                    const int dx = vx[k], dy = vy[k];
                    if (bx * 8 + dx >= 0 && by * 8 + dy >= 0 && bx * 8 + dx + 8 <= w && by * 8 + dy + 8 <= h &&
                        block_fits(cur, prev, w, bx * 8, by * 8, dx, dy, skip_thr)) {
                        t = BLK_MOTION;
                        mx_[nm] = dx; my_[nm] = dy; nm++;
                        for (y = 0; y < 8; y++)
                            memcpy(ref + (size_t)(by * 8 + y) * w + bx * 8, prev + (size_t)(by * 8 + y + dy) * w + bx * 8 + dx, 8);
                    }
                }
            }
            if (t >= 0) {
                /* (skip or motion: done) */
            } else if (mx - mn <= fill_thr) {
                uint8_t v = (uint8_t)((sum + 32) >> 6);
                t = BLK_FILL;
                vals[nv++] = v;
                for (y = 0; y < 8; y++) memset(ref + (size_t)(by * 8 + y) * w + bx * 8, v, 8);
            } else if (pat_thr > 0 && pattern_fit(cur + (size_t)(by * 8) * w + bx * 8, w, pat_thr, vals + nv, pats + np)) {
                t = BLK_PATTERN;
                for (y = 0; y < 8; y++)
                    for (x = 0; x < 8; x++)
                        ref[(size_t)(by * 8 + y) * w + bx * 8 + x] = vals[nv + ((pats[np + y] >> x) & 1)];
                nv += 2;
                np += 8;
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
    prow_start[bh] = np;
    mrow_start[bh] = nm;
    for (i = 0; i < (int)np; i++) patv[i] = pats[i];

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
        /* patterns (each a byte, low nibble first), then the motion offsets: batches as the colours */
        batch_put(b, &pb, patv, prow_start, by, bh, len_pattern, 0);
        batch_put(b, &xb, mx_, mrow_start, by, bh, len_small, 1);
        batch_put(b, &yb, my_, mrow_start, by, bh, len_small, 1);
        if (first) {
            bits_put(b, 0, len_small);   /* intra dc */
            bits_put(b, 0, len_small);   /* inter dc */
            bits_put(b, 0, len_run);
            first = 0;
        }
        consumed += row_start[by + 1] - row_start[by];
        pb.consumed += prow_start[by + 1] - prow_start[by];
        xb.consumed += mrow_start[by + 1] - mrow_start[by];
        yb.consumed += mrow_start[by + 1] - mrow_start[by];
    }
    bits_align32(b);
    free(types); free(vals); free(row_start); free(pats); free(patv); free(prow_start);
    free(mx_); free(my_); free(mrow_start); free(prev);
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
    int frames, done, w, h, alpha;
    uint8_t *same; size_t same_n;  /* bink_writer_repeat's frame */
    uint8_t *Y, *U, *V, *rY, *rU, *rV, *A, *rA;
    uint32_t *offs, largest;
    Bits b;
    int skip_thr, fill_thr, pat_thr;  /* WWE12-PC: bink_writer_quality (default 2, 2, 0) */
};

static int bink_writer_frame_yuv(BinkWriter *w);

void bink_writer_free(BinkWriter *w)
{
    if (!w) return;
    free(w->b.p); free(w->same); free(w->Y); free(w->U); free(w->V); free(w->rY); free(w->rU); free(w->rV); free(w->offs);
    free(w->A); free(w->rA);
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
    w->skip_thr = 2; w->fill_thr = 2; w->pat_thr = 0;
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

BinkWriter *bink_writer_open_alpha(FILE *f, int frames, int MOVIE_W, int MOVIE_H)
{
    BinkWriter *w = bink_writer_open_size(f, frames, MOVIE_W, MOVIE_H);
    if (!w) return NULL;
    w->alpha = 1;
    w->A = (uint8_t *)malloc((size_t)MOVIE_W * MOVIE_H); w->rA = (uint8_t *)calloc((size_t)MOVIE_W * MOVIE_H, 1);
    if (!w->A || !w->rA) { bink_writer_free(w); return NULL; }
    return w;
}

int bink_writer_frame(BinkWriter *w, const uint8_t *bgra)
{
    if (bgra) {
        to_yuv(bgra, w->Y, w->U, w->V, w->w, w->h);
        if (w->alpha) {
            size_t i, n = (size_t)w->w * w->h;
            for (i = 0; i < n; i++) w->A[i] = bgra[i * 4 + 3];
        }
    }
    return bink_writer_frame_yuv(w);
}

void bink_writer_quality(BinkWriter *w, int skip_thr, int fill_thr, int pat_thr)
{
    w->skip_thr = skip_thr; w->fill_thr = fill_thr; w->pat_thr = pat_thr;
}

int bink_writer_repeat(BinkWriter *w)
{
    size_t pos;
    if (w->done == 0) return 0;
    if (!w->same) {  /* (the first repeat: encoded - every block skips - and kept) */
        /* WWE12-PC: every block made to skip (a lossy last frame isn't the picture: its blocks off by more than
         * skip_thr were encoded again, and those bytes replayed later broke the picture) */
        const int skip = w->skip_thr;
        int r;
        w->skip_thr = 255;
        r = bink_writer_frame_yuv(w);
        w->skip_thr = skip;
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

static int bink_writer_frame_yuv_once(BinkWriter *w);

/* WWE12-PC: no frame bigger than ~0.9 byte per pixel (92 KB at 320 x 320), nor 100 KB (the game's own titantrons stay
 * under 108 KB; the limit looks like a size, not a rate: a 42 KB ribbon at 1.3 byte per pixel played): a 2K16 titantron
 * made with frames up to 140 KB froze the game's loading before the entrance (Seth Rollins, 2026-11-04). A frame over it is encoded again, lossier, from the same reference planes. */
static int bink_writer_frame_yuv(BinkWriter *w)
{
    static const int steps[][3] = {{3, 3, 6}, {4, 4, 10}, {6, 6, 14}, {8, 8, 20}, {10, 10, 28}, {12, 12, 36},
                                   {16, 16, 52}, {24, 24, 80}};
    const size_t ny = (size_t)w->w * w->h, nc = ny / 4, cap = ny * 9 / 10 < 100000 ? ny * 9 / 10 : 100000;
    const int skip = w->skip_thr, fill = w->fill_thr, pat = w->pat_thr;
    uint8_t *save = NULL;
    long at;
    int r, i;
    if (skip >= 255) {  /* (a repeat: all skips) */
        r = bink_writer_frame_yuv_once(w);
        if (r == 1 && w->b.n > w->largest) w->largest = (uint32_t)w->b.n;
        return r;
    }
    at = bink_tell(w->f);
    save = (uint8_t *)malloc(ny + 2 * nc + (w->alpha ? ny : 0));
    if (!save) {
        r = bink_writer_frame_yuv_once(w);
        if (r == 1 && w->b.n > w->largest) w->largest = (uint32_t)w->b.n;
        return r;
    }
    memcpy(save, w->rY, ny); memcpy(save + ny, w->rU, nc); memcpy(save + ny + nc, w->rV, nc);
    if (w->alpha) memcpy(save + ny + 2 * nc, w->rA, ny);
    r = bink_writer_frame_yuv_once(w);
    for (i = 0; r == 1 && w->b.n > cap && i < (int)(sizeof steps / sizeof steps[0]); i++) {
        if (steps[i][0] <= skip && steps[i][1] <= fill) continue;
        memcpy(w->rY, save, ny); memcpy(w->rU, save + ny, nc); memcpy(w->rV, save + ny + nc, nc);
        if (w->alpha) memcpy(w->rA, save + ny + 2 * nc, ny);
        w->done--;
        bink_seek(w->f, at, SEEK_SET);
        w->skip_thr = steps[i][0]; w->fill_thr = steps[i][1]; w->pat_thr = steps[i][2];
        r = bink_writer_frame_yuv_once(w);
    }
    w->skip_thr = skip; w->fill_thr = fill; w->pat_thr = pat;
    free(save);
    if (r == 1 && w->b.n > w->largest) w->largest = (uint32_t)w->b.n;
    return r;
}

static int bink_writer_frame_yuv_once(BinkWriter *w)
{
    const int MOVIE_W = w->w, MOVIE_H = w->h;
    Bits *b = &w->b;
    size_t pos, ysize, at = 0;
    const int key = w->done == 0;
    b->n = 0; b->bits = 0; b->acc = 0;
    /* with alpha, the alpha plane first (encoded as Y), then the rest as without; each of the two parts starts with
     * its size, its own 4 bytes counted (as the game's transition movies have them) */
    if (w->alpha) {
        size_t asize;
        bits_put(b, 0, 32);
        if (!encode_plane(b, w->A, w->rA, MOVIE_W, MOVIE_H, key, 2, 2, 0)) return 0;
        asize = b->n;
        b->p[0] = (uint8_t)asize; b->p[1] = (uint8_t)(asize >> 8); b->p[2] = (uint8_t)(asize >> 16); b->p[3] = (uint8_t)(asize >> 24);
        at = b->n;
    }
    bits_put(b, 0, 32);                                          /* size of the Y plane, below */
    if (!encode_plane(b, w->Y, w->rY, MOVIE_W, MOVIE_H, key, w->skip_thr, w->fill_thr, w->pat_thr)) return 0;
    ysize = b->n - at;
    if (!encode_plane(b, w->V, w->rV, MOVIE_W / 2, MOVIE_H / 2, key, w->skip_thr, w->fill_thr, w->pat_thr)) return 0;   /* Bink 'i': V before U */
    if (!encode_plane(b, w->U, w->rU, MOVIE_W / 2, MOVIE_H / 2, key, w->skip_thr, w->fill_thr, w->pat_thr)) return 0;
    b->p[at] = (uint8_t)ysize; b->p[at + 1] = (uint8_t)(ysize >> 8); b->p[at + 2] = (uint8_t)(ysize >> 16); b->p[at + 3] = (uint8_t)(ysize >> 24);
    pos = (size_t)bink_tell(w->f);
    w->offs[w->done] = (uint32_t)pos | (key ? 1u : 0u);
    if (fwrite(b->p, 1, b->n, w->f) != b->n) return -1;
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
    hdr[5] = (uint32_t)w->w; hdr[6] = (uint32_t)w->h; hdr[7] = FPS; hdr[8] = 1; hdr[9] = w->alpha ? 0x00100000u : 0; hdr[10] = 0;
    bink_seek(w->f, 0, SEEK_SET);
    fwrite(hdr, 4, 11, w->f);
    fwrite(w->offs, 4, (size_t)n + 1, w->f);
    {   /* WWE12-PC: a frame encoded again shorter (bink_writer_frame_yuv) left bytes past the end: cut */
        const uint32_t end = w->offs[n];
        fflush(w->f);
#ifdef _WIN32
        _chsize_s(_fileno(w->f), (__int64)end);
#else
        if (ftruncate(fileno(w->f), (off_t)end)) {}
#endif
    }
    bink_writer_free(w);
    return 1;
}
