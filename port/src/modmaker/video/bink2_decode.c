/* WWE12-PC: Bink 2 video reader (see bink2_decode.h).
 *
 * The decoder proper is video/bink2/ (bink2_core.inc, bink2f.inc, bink2g.inc): the patch "avcodec: add bink2 video
 * decoder" posted to FFmpeg-devel on 2020-03-20 by Paul B Mahol (Copyright (c) 2014 Konstantin Shishkov, (c) 2019
 * Paul B Mahol), https://ffmpeg.org/pipermail/ffmpeg-devel/2020-March/258764.html, taken unchanged (KB2f-KB2i), plus
 * our bink2d.inc for WWE '13's KB2d (decoded slices, then post-processed: bink2d_postprocess). Below: the few
 * FFmpeg helpers it uses (bit reader, VLC tables, clipping, block copies), written again here, and the file reading
 * (header, frame index, packets) on the model of bink_decode.c.
 *
 * This file is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version. It is distributed WITHOUT ANY WARRANTY; see the GNU Lesser General Public License for
 * more details. */
#define WIN32_LEAN_AND_MEAN
#include "bink2_decode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the FFmpeg helpers the decoder uses ---- */

#define av_cold
#define emms_c() ((void)0)
#define AVERROR_INVALIDDATA (-1)
#define DECLARE_ALIGNED(n, t, v) t v
#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMAX3(a, b, c) FFMAX(FFMAX(a, b), c)
#define FFMIN3(a, b, c) FFMIN(FFMIN(a, b), c)
#define FFSWAP(type, a, b) do { type SWAP_tmp = b; b = a; a = SWAP_tmp; } while (0)

static inline int av_clip(int a, int amin, int amax) { return a < amin ? amin : a > amax ? amax : a; }
static inline float av_clipf(float a, float amin, float amax) { return a < amin ? amin : a > amax ? amax : a; }
static inline uint8_t av_clip_uint8(int a) { return (uint8_t)(a < 0 ? 0 : a > 255 ? 255 : a); }
static inline int mid_pred(int a, int b, int c) { return FFMAX(FFMIN(a, b), FFMIN(FFMAX(a, b), c)); }

static inline uint32_t rl32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static void copy_block8(uint8_t *dst, const uint8_t *src, ptrdiff_t dst_stride, ptrdiff_t src_stride, int h)
{
    for (int i = 0; i < h; i++, dst += dst_stride, src += src_stride) memcpy(dst, src, 8);
}
static void copy_block16(uint8_t *dst, const uint8_t *src, ptrdiff_t dst_stride, ptrdiff_t src_stride, int h)
{
    for (int i = 0; i < h; i++, dst += dst_stride, src += src_stride) memcpy(dst, src, 16);
}

/* Bit reader, least significant bit first (FFmpeg's BITSTREAM_READER_LE). The buffer has 8 zero bytes after it. */
typedef struct GetBitContext {
    const uint8_t *buffer;
    int index, size_in_bits;
} GetBitContext;

static inline unsigned show_bits_le(const GetBitContext *s, int n)
{
    uint64_t v;
    if (n <= 0) return 0;
    memcpy(&v, s->buffer + (s->index >> 3), 8);
    v >>= s->index & 7;
    return (unsigned)(v & ((1ull << n) - 1));
}
static inline unsigned get_bits(GetBitContext *s, int n)
{
    const unsigned v = show_bits_le(s, n);
    s->index += n;
    return v;
}
static inline unsigned get_bits1(GetBitContext *s) { return get_bits(s, 1); }
static inline unsigned get_bitsz(GetBitContext *s, int n) { return n ? get_bits(s, n) : 0; }
static inline void skip_bits_long(GetBitContext *s, int n) { s->index += n; }
static inline int get_bits_count(const GetBitContext *s) { return s->index; }
static inline int get_bits_left(const GetBitContext *s) { return s->size_in_bits - s->index; }
static inline void align_get_bits(GetBitContext *s) { s->index = (s->index + 7) & ~7; }
static inline int get_unary(GetBitContext *gb, int stop, int len)
{
    int i;
    for (i = 0; i < len && (int)get_bits1(gb) != stop; i++)
        ;
    return i;
}

/* VLC tables as the decoder declares them (static VLC x; x.table, x.bits): little-endian codes (the first bit read
 * is the code's lowest), symbol = index, found by looking at the next 16 bits. */
typedef struct VLCEntry {
    uint16_t code;
    uint8_t len;
} VLCEntry;
typedef struct VLC {
    const struct VLC *table; /* (the VLC itself: get_vlc2 is handed x.table) */
    int bits, count;
    VLCEntry e[32];
} VLC;

static void init_vlc_le(VLC *v, int nb_bits, int count, const uint8_t *bits, int bits_size, const void *codes,
                        int codes_size)
{
    v->table = v;
    v->bits = nb_bits;
    v->count = count > 32 ? 32 : count;
    for (int i = 0; i < v->count; i++) {
        v->e[i].len = bits[i * bits_size];
        v->e[i].code = codes_size == 1 ? ((const uint8_t *)codes)[i] : ((const uint16_t *)codes)[i];
    }
}
static inline int get_vlc2(GetBitContext *s, const VLC *v, int bits, int max_depth)
{
    const unsigned peek = show_bits_le(s, 16);
    (void)bits;
    (void)max_depth;
    for (int i = 0; i < v->count; i++)
        if (v->e[i].len && (peek & ((1u << v->e[i].len) - 1)) == v->e[i].code) {
            s->index += v->e[i].len;
            return i;
        }
    s->index += v->bits;
    return -1;
}

typedef struct AVCodecContext {
    int width, height;
} AVCodecContext;
typedef struct AVFrame AVFrame;
typedef struct BlockDSPContext {
    int unused;
} BlockDSPContext;

#include "bink2/bink2_core.inc"

/* ---- the file ---- */

/* A plane with room around it: motion vectors may point up to a block outside the picture. */
enum { kMargin = 64 };

struct Bink2Reader {
    uint8_t *file;
    size_t file_size;
    uint32_t *index; /* frames + 1 offsets (low bit: key frame) */
    int frames, w, h, audio_tracks;
    LONGLONG frame_time;
    int pos;
    uint8_t *packet;
    size_t packet_cap;
    AVCodecContext avctx;
    Bink2Context c;
    uint8_t *mem[2][3];       /* two pictures (this one, the last), Y U V with their margins */
    uint8_t *data[2][4];      /* where each plane starts (data[k][3]: none) */
    int linesize[3], rows[3]; /* plane strides and allocated rows (margins included) */
    int cur;                  /* which picture is this one */
};

static void free_all(Bink2Reader *r)
{
    free(r->file);
    free(r->index);
    free(r->packet);
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 3; i++) free(r->mem[k][i]);
    free(r->c.current_q); free(r->c.prev_q); free(r->c.current_dc); free(r->c.prev_dc);
    free(r->c.current_idc); free(r->c.prev_idc); free(r->c.current_mv); free(r->c.prev_mv);
    free(r->c.col_cbp); free(r->c.row_cbp);
    bink2d_free(r->c.kb2d);
    free(r);
}

static int setup(Bink2Reader *r, WCHAR *err, size_t errn)
{
    const uint8_t *h = r->file;
    uint32_t hdr[11];
    size_t at;
    if (r->file_size < 48 || memcmp(h, "KB2", 3)) {
        swprintf(err, errn, L"Not a Bink 2 movie.");
        return 0;
    }
    for (int i = 0; i < 11; i++) hdr[i] = rl32(h + 4 * i);
    if (!hdr[2] || hdr[2] > 1000000 || !hdr[5] || hdr[5] > 4096 || !hdr[6] || hdr[6] > 4096 || !hdr[7] || !hdr[8]) {
        swprintf(err, errn, L"A Bink 2 movie with an odd header.");
        return 0;
    }
    if (hdr[9] & BINK_FLAG_ALPHA) {
        swprintf(err, errn, L"A Bink 2 movie with an alpha channel (not supported).");
        return 0;
    }
    r->frames = (int)hdr[2];
    r->w = (int)hdr[5];
    r->h = (int)hdr[6];
    r->frame_time = (LONGLONG)10000000 * hdr[8] / hdr[7];
    r->audio_tracks = (int)hdr[10];
    if (r->audio_tracks > 256) {
        swprintf(err, errn, L"A Bink 2 movie with an odd header.");
        return 0;
    }
    /* the frame index right after the audio tracks' 12 bytes each, or 4 bytes later (some revisions): the one whose
     * first offset is where the index ends */
    at = 0;
    for (size_t extra = 0; extra <= 4 && !at; extra += 4) {
        const size_t start = 44 + extra + (size_t)r->audio_tracks * 12, end = start + 4 * ((size_t)r->frames + 1);
        if (end <= r->file_size && (rl32(h + start) & ~1u) == end) at = start;
    }
    if (!at) {
        swprintf(err, errn, L"A Bink 2 movie whose frame index wasn't found.");
        return 0;
    }
    r->index = (uint32_t *)malloc(4 * ((size_t)r->frames + 1));
    if (!r->index) goto nomem;
    for (int i = 0; i <= r->frames; i++) r->index[i] = rl32(h + at + 4 * (size_t)i);

    r->avctx.width = r->w;
    r->avctx.height = r->h;
    {
        Bink2Context *c = &r->c;
        const int mbs = (r->w + 31) / 32;
        c->avctx = &r->avctx;
        c->version = h[3];
        c->flags = hdr[9];
        c->has_alpha = 0;
        c->current_q = (QuantPredict *)calloc(mbs, sizeof(*c->current_q));
        c->prev_q = (QuantPredict *)calloc(mbs, sizeof(*c->prev_q));
        c->current_dc = (DCPredict *)calloc(mbs, sizeof(*c->current_dc));
        c->prev_dc = (DCPredict *)calloc(mbs, sizeof(*c->prev_dc));
        c->current_idc = (DCIPredict *)calloc(mbs, sizeof(*c->current_idc));
        c->prev_idc = (DCIPredict *)calloc(mbs, sizeof(*c->prev_idc));
        c->current_mv = (MVPredict *)calloc(mbs, sizeof(*c->current_mv));
        c->prev_mv = (MVPredict *)calloc(mbs, sizeof(*c->prev_mv));
        c->col_cbp = (uint8_t *)calloc((((r->w + 31) >> 3) + 7) >> 3, 1);
        c->row_cbp = (uint8_t *)calloc((((r->h + 31) >> 3) + 7) >> 3, 1);
        if (c->version == 'd')
            c->kb2d = bink2d_alloc(r->w, r->h);
        if (!c->current_q || !c->prev_q || !c->current_dc || !c->prev_dc || !c->current_idc || !c->prev_idc ||
            !c->current_mv || !c->prev_mv || !c->col_cbp || !c->row_cbp || (c->version == 'd' && !c->kb2d))
            goto nomem;
    }
    {
        static int tables_made = 0;
        if (!tables_made) {
            init_vlc_le(&bink2f_quant_vlc, 9, (int)(sizeof(bink2f_quant_codes) / sizeof(bink2f_quant_codes[0])),
                        bink2f_quant_bits, 1, bink2f_quant_codes, 1);
            init_vlc_le(&bink2f_ac_val0_vlc, 9, 13, bink2f_ac_val_bits[0], 1, bink2f_ac_val_codes[0], 2);
            init_vlc_le(&bink2f_ac_val1_vlc, 9, 13, bink2f_ac_val_bits[1], 1, bink2f_ac_val_codes[1], 2);
            init_vlc_le(&bink2f_ac_skip0_vlc, 9, NUM_AC_SKIPS, bink2f_ac_skip_bits[0], 1, bink2f_ac_skip_codes[0], 2);
            init_vlc_le(&bink2f_ac_skip1_vlc, 9, NUM_AC_SKIPS, bink2f_ac_skip_bits[1], 1, bink2f_ac_skip_codes[1], 2);
            init_vlc_le(&bink2g_ac_skip0_vlc, 9, NUM_AC_SKIPS, bink2g_ac_skip_bits[0], 1, bink2g_ac_skip_codes[0], 2);
            init_vlc_le(&bink2g_ac_skip1_vlc, 9, NUM_AC_SKIPS, bink2g_ac_skip_bits[1], 1, bink2g_ac_skip_codes[1], 2);
            init_vlc_le(&bink2g_mv_vlc, 9, (int)sizeof(bink2g_mv_bits), bink2g_mv_bits, 1, bink2g_mv_codes, 1);
            tables_made = 1;
        }
    }
    {
        const int aw = (r->w + 31) & ~31, ah = (r->h + 31) & ~31;
        r->linesize[0] = aw + 2 * kMargin;
        r->linesize[1] = r->linesize[2] = aw / 2 + 2 * kMargin;
        r->rows[0] = ah + 2 * kMargin;
        r->rows[1] = r->rows[2] = ah / 2 + 2 * kMargin;
        for (int k = 0; k < 2; k++)
            for (int i = 0; i < 3; i++) {
                r->mem[k][i] = (uint8_t *)calloc((size_t)r->linesize[i] * r->rows[i], 1);
                if (!r->mem[k][i]) goto nomem;
                r->data[k][i] = r->mem[k][i] + (size_t)kMargin * r->linesize[i] + kMargin;
            }
    }
    return 1;
nomem:
    swprintf(err, errn, L"Out of memory.");
    return 0;
}

Bink2Reader *bink2_open_memory(const uint8_t *data, size_t size, WCHAR *err, size_t errn)
{
    Bink2Reader *r = (Bink2Reader *)calloc(1, sizeof *r);
    if (!r) return NULL;
    r->file = (uint8_t *)malloc(size + 8);
    if (!r->file) {
        free(r);
        return NULL;
    }
    memcpy(r->file, data, size);
    memset(r->file + size, 0, 8);
    r->file_size = size;
    if (!setup(r, err, errn)) {
        free_all(r);
        return NULL;
    }
    return r;
}

Bink2Reader *bink2_open(const WCHAR *path, WCHAR *err, size_t errn)
{
    FILE *f = NULL;
    uint8_t *d;
    long long n;
    Bink2Reader *r;
    if (_wfopen_s(&f, path, L"rb") || !f) {
        swprintf(err, errn, L"Could not read %s.", path);
        return NULL;
    }
    _fseeki64(f, 0, SEEK_END);
    n = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    d = n > 0 && n < (1ll << 31) ? (uint8_t *)malloc((size_t)n) : NULL;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(d);
        swprintf(err, errn, L"Could not read %s.", path);
        return NULL;
    }
    fclose(f);
    r = bink2_open_memory(d, (size_t)n, err, errn);
    free(d);
    return r;
}

void bink2_close(Bink2Reader *r)
{
    if (r) free_all(r);
}

int bink2_width(const Bink2Reader *r) { return r->w; }
int bink2_height(const Bink2Reader *r) { return r->h; }
int bink2_frames(const Bink2Reader *r) { return r->frames; }
char bink2_revision(const Bink2Reader *r) { return (char)r->file[3]; }
LONGLONG bink2_frame_time(const Bink2Reader *r) { return r->frame_time; }
int bink2_position(const Bink2Reader *r) { return r->pos; }

void bink2_rewind(Bink2Reader *r)
{
    r->pos = 0;
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 3; i++) memset(r->mem[k][i], 0, (size_t)r->linesize[i] * r->rows[i]);
}

/* bink2_decode_frame of the patch, without FFmpeg's frame buffers. */
static int decode_frame(Bink2Reader *r, const uint8_t *pkt, int size, int is_kf)
{
    Bink2Context *const c = &r->c;
    GetBitContext *gb = &c->gb;
    const int now = r->cur ^ 1, last = r->cur;
    uint8_t *dst[4], *src[4];
    int stride[4], sstride[4];
    uint32_t off = 0;
    int ret, height_a;

    for (int i = 0; i < 4; i++) {
        src[i] = i < 3 ? r->data[last][i] : NULL;
        dst[i] = i < 3 ? r->data[now][i] : NULL;
        stride[i] = sstride[i] = i < 3 ? r->linesize[i] : 0;
    }
    if (size < 8) return AVERROR_INVALIDDATA;
    c->frame_flags = rl32(pkt);
    gb->buffer = pkt;
    gb->index = 0;
    gb->size_in_bits = size * 8;

    height_a = (r->h + 31) & 0xFFFFFFE0;
    if (c->version <= 'f') {
        c->num_slices = 2;
        c->slice_height[0] = (r->h / 2 + 16) & 0xFFFFFFE0;
    } else if (c->version == 'g') {
        if (height_a < 128) {
            c->num_slices = 1;
        } else {
            c->num_slices = 2;
            c->slice_height[0] = (r->h / 2 + 16) & 0xFFFFFFE0;
        }
    } else {
        int start = 0, end;
        c->num_slices = kb2h_num_slices[c->flags & 3];
        end = height_a + 32 * c->num_slices - 1;
        for (int i = 0; i < c->num_slices - 1; i++) {
            start += ((end - start) / (c->num_slices - i)) & 0xFFFFFFE0;
            end -= 32;
            c->slice_height[i] = start;
        }
    }
    c->slice_height[c->num_slices - 1] = height_a;

    skip_bits_long(gb, 32 + 32 * (c->num_slices - 1));
    if (c->frame_flags & 0x10000) {
        if (!(c->frame_flags & 0x8000))
            bink2_get_block_flags(gb, 1, (((r->h + 15) & ~15) >> 3) - 1, c->row_cbp);
        if (!(c->frame_flags & 0x4000))
            bink2_get_block_flags(gb, 1, (((r->w + 15) & ~15) >> 3) - 1, c->col_cbp);
    }
    for (int i = 0; i < c->num_slices; i++) {
        off = i == c->num_slices - 1 ? (uint32_t)size : rl32(pkt + 4 + i * 4);
        if (c->version == 'd')
            ret = bink2d_decode_slice(c, dst, stride, src, sstride, is_kf, i ? c->slice_height[i - 1] : 0,
                                      c->slice_height[i]);
        else if (c->version <= 'f')
            ret = bink2f_decode_slice(c, dst, stride, src, sstride, is_kf, i ? c->slice_height[i - 1] : 0,
                                      c->slice_height[i]);
        else
            ret = bink2g_decode_slice(c, dst, stride, src, sstride, is_kf, i ? c->slice_height[i - 1] : 0,
                                      c->slice_height[i]);
        if (ret < 0) return ret;
        align_get_bits(gb);
        skip_bits_long(gb, 8 * (int)(off - (uint32_t)(get_bits_count(gb) >> 3)));
        dst[0] = r->data[now][0] + c->slice_height[i] * stride[0];
        dst[1] = r->data[now][1] + c->slice_height[i] / 2 * stride[1];
        dst[2] = r->data[now][2] + c->slice_height[i] / 2 * stride[2];
    }
    if (c->version == 'd') {
        uint8_t *plane[3] = { r->data[now][0], r->data[now][2], r->data[now][1] };
        int pstride[3] = { r->linesize[0], r->linesize[2], r->linesize[1] };
        bink2d_postprocess(c, plane, pstride);
    }
    r->cur = now;
    return 0;
}

int bink2_next(Bink2Reader *r)
{
    uint32_t start, end;
    const uint8_t *p;
    int is_kf;
    if (r->pos >= r->frames) bink2_rewind(r); /* start over */
    start = r->index[r->pos] & ~1u;
    end = r->index[r->pos + 1] & ~1u;
    is_kf = r->index[r->pos] & 1;
    r->pos++;
    if (end <= start || end > r->file_size) return 0;
    p = r->file + start;
    /* the audio packets first: each track's size, then its data */
    for (int t = 0; t < r->audio_tracks; t++) {
        uint32_t n;
        if ((size_t)(p - r->file) + 4 > end) return 0;
        n = rl32(p);
        p += 4 + n;
        if ((size_t)(p - r->file) > end) return 0;
    }
    {
        const size_t size = end - (size_t)(p - r->file);
        if (size + 8 > r->packet_cap) {
            uint8_t *np = (uint8_t *)realloc(r->packet, size + 8);
            if (!np) return 0;
            r->packet = np;
            r->packet_cap = size + 8;
        }
        memcpy(r->packet, p, size);
        memset(r->packet + size, 0, 8);
        if (r->pos == 1) is_kf = 1;
        return decode_frame(r, r->packet, (int)size, is_kf) == 0;
    }
}

/* BT.601 studio range -> BGRA; the planes as FFmpeg's yuv420p: data[1] = U, data[2] = V. */
void bink2_bgra(const Bink2Reader *r, uint8_t *bgra)
{
    const uint8_t *py = r->data[r->cur][0], *pu = r->data[r->cur][1], *pv = r->data[r->cur][2];
    for (int y = 0; y < r->h; y++)
        for (int x = 0; x < r->w; x++) {
            const int Y = py[y * r->linesize[0] + x] - 16;
            const int U = pu[(y >> 1) * r->linesize[1] + (x >> 1)] - 128;
            const int V = pv[(y >> 1) * r->linesize[2] + (x >> 1)] - 128;
            const int R = (298 * Y + 409 * V + 128) >> 8;
            const int G = (298 * Y - 100 * U - 208 * V + 128) >> 8;
            const int B = (298 * Y + 516 * U + 128) >> 8;
            uint8_t *o = bgra + ((size_t)y * r->w + x) * 4;
            o[0] = av_clip_uint8(B);
            o[1] = av_clip_uint8(G);
            o[2] = av_clip_uint8(R);
            o[3] = 255;
        }
}
