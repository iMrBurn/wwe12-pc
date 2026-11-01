/* WWE12-PC: Bink 2 revision "KB2d" (WWE '13's movies), not in the FFmpeg patch: our own description of it, written
 * from what WWE '13's decoder does, on top of bink2f.inc (same tables, DC coding and prediction, motion vectors,
 * motion compensation, IDCT). This file is part of the LGPL 2.1+ decoder of video/bink2/ (see bink2_core.inc).
 *
 * What KB2d does differently from KB2f:
 *  - each component of a macroblock starts with its quantizer as 4 plain bits (KB2f: a VLC delta on the last one);
 *  - there is no coded block pattern: the AC coefficients of a group of 4 blocks (a 16x16 luma quarter, or the 4
 *    chroma blocks) are coded together, band by band of scan positions (bink2d_bands_*: the last position of each
 *    band): 1 bit "the rest is zero", else a width n (3 bits, 7 = 7 + 2 more bits) and, for each position of the
 *    band, the 4 blocks' magnitudes on n bits each (block 0 first) followed by one sign bit (1 = negative) per
 *    nonzero one;
 *  - a residue block codes both chroma planes together: one quantizer, one DC width for the 8 DCs (two groups of 4
 *    with their signs, no extra DC), then each plane's AC;
 *  - DCs: a residue block's are kept in 0..2047 (bink2d_inter_dc), and when a whole quarter (or chroma plane) has no
 *    AC its DCs are added rounded towards zero (bink2d_add_dc); in an intra slice's first macroblock the DCs start
 *    from 1024 only if the extra DC is coded (bink2d_decode_intra_dc);
 *  - motion compensation at diagonal positions filters vertically the unrounded horizontal result
 *    (bink2d_block_mc);
 *  - after decoding, a frame is post-processed (in the loop: the next frames are predicted from the result):
 *    . an intra block without AC coefficient is not drawn flat but as a smooth surface through its DC and its 8
 *      neighbours' (bink2d_fill_dc), the neighbours being used only if they are intra blocks without AC too and
 *      their DC is closer than a threshold given by the frame (low byte of the frame flags x 8, often 0: flat);
 *    . unless the frame flags say not to (0x4000: vertical edges, 0x8000: horizontal edges), the 8x8 block edges
 *      are smoothed (bink2d_deblock) on 1 or 2 pixels per side, according to how many coefficients each block had,
 *      or, between two non-intra blocks, when their motion vectors differ;
 *  - the slices are 32-bit aligned (the second slice also starts at the offset given in the packet). */

static const uint8_t bink2d_bands_luma_intra[8] = { 2, 4, 8, 13, 19, 24, 41, 63 };
static const uint8_t bink2d_bands_luma_inter[8] = { 2, 4, 8, 13, 19, 24, 44, 63 };
static const uint8_t bink2d_bands_chroma[6] = { 2, 5, 7, 9, 12, 63 };

/* The side information the post-processing needs, for the whole frame. info: per 8x8 block, 0 for a non-intra
 * block, else (class << 13) | DC rounded, the class being 4 + (fewer than 8 AC coefficients) + (fewer than 4) +
 * (none) - 7 = DC only. mv: the motion vector (x, y) of each 16x16 luma area (0 for skipped blocks). */
typedef struct Bink2DFrame {
    int bw[3], bh[3];      /* blocks per row / column: luma, then the two chroma planes */
    uint16_t *info[3];     /* plane 0 luma, 1 = comp 1 (dst[2]), 2 = comp 2 (dst[1]) */
    int mvw;               /* 16x16 areas per row */
    int32_t (*mv)[2];
} Bink2DFrame;

static Bink2DFrame *bink2d_alloc(int w, int h)
{
    Bink2DFrame *d = (Bink2DFrame *)calloc(1, sizeof(*d));
    const int mbw = (w + 31) / 32, mbh = (h + 31) / 32;

    if (!d)
        return NULL;
    d->bw[0] = mbw * 4;
    d->bh[0] = mbh * 4;
    d->bw[1] = d->bw[2] = mbw * 2;
    d->bh[1] = d->bh[2] = mbh * 2;
    for (int p = 0; p < 3; p++)
        if (!(d->info[p] = (uint16_t *)calloc((size_t)d->bw[p] * d->bh[p], sizeof(uint16_t))))
            goto fail;
    d->mvw = mbw * 2;
    if (!(d->mv = (int32_t(*)[2])calloc((size_t)mbw * 2 * mbh * 2, sizeof(*d->mv))))
        goto fail;
    return d;
fail:
    for (int p = 0; p < 3; p++)
        free(d->info[p]);
    free(d);
    return NULL;
}

static void bink2d_free(Bink2DFrame *d)
{
    if (!d)
        return;
    for (int p = 0; p < 3; p++)
        free(d->info[p]);
    free(d->mv);
    free(d);
}

/* The AC coefficients of 4 blocks, dequantized into block[0..3] (DC left at 0); nz[j] = how many are nonzero. */
static void bink2d_decode_ac(GetBitContext *gb, const uint8_t *scan, const uint8_t *bands, float block[4][64],
                             float q, const float qmat[64], int nz[4])
{
    int idx = 1;

    for (int i = 0; i < 4; i++) {
        memset(block[i], 0, sizeof(**block) * 64);
        nz[i] = 0;
    }
    for (int b = 0; idx < 64; b++) {
        const int last = bands[b];
        int n;

        if (get_bits1(gb))
            break;
        n = get_bits(gb, 3);
        if (n == 7)
            n += get_bits(gb, 2);
        if (!n) {
            idx = last + 1;
            continue;
        }
        for (; idx <= last; idx++) {
            int v[4];

            for (int j = 0; j < 4; j++)
                v[j] = get_bits(gb, n);
            for (int j = 0; j < 4; j++) {
                if (!v[j])
                    continue;
                if (get_bits1(gb))
                    v[j] = -v[j];
                nz[j]++;
                block[j][scan[idx]] = v[j] * (q * qmat[scan[idx]]);
            }
        }
    }
}

/* WWE '13 rounds with 0.5 minus 3 ulps (0x3EFFFFFD), which matters on exact halves */
#define BINK2D_HALF 0.49999991059f

/* the float sums must be rounded step by step, as on the console */
#pragma STDC FP_CONTRACT OFF

static uint16_t bink2d_block_info(float dc, int nz)
{
    const int cls = 4 + (nz < 8) + (nz < 4) + (nz == 0);

    return (uint16_t)(cls << 13 | ((int)(dc + BINK2D_HALF) & 0x1FFF));
}

/* bink2f_decode_dc for intra blocks, with one difference: in the top left macroblock of a slice, the DCs start
 * from 1024 (+ the extra DC) only when that extra DC is coded (fewer than 10 bits known); otherwise from 0. */
static void bink2d_decode_intra_dc(Bink2Context *c, GetBitContext *gb, int is_luma, int q, int flags)
{
    const int num_dc = is_luma ? 16 : 4;
    float tdc[16] = { 0 };
    int dc_bits = get_bits(gb, 3);

    if (dc_bits == 7)
        dc_bits += get_bits(gb, 2);
    if (dc_bits) {
        for (int j = 0; j < num_dc; j += 4) {
            for (int i = 0; i < 4; i++)
                tdc[i + j] = get_bits(gb, dc_bits);
            for (int i = 0; i < 4; i++)
                if (tdc[i + j] && get_bits1(gb))
                    tdc[i + j] = -tdc[i + j];
        }
    }
    if ((flags & 0x20) && (flags & 0x80)) {
        const int bits = (q_dc_bits[q] - 1) + dc_bits;

        if (bits < 10) {
            int dc_val = get_bits(gb, 10 - bits);

            if (dc_val) {
                dc_val <<= dc_bits;
                if (get_bits1(gb))
                    dc_val = -dc_val;
            }
            tdc[0] += dc_val;
        }
        for (int i = 0; i < num_dc; i++)
            tdc[i] *= bink2f_dc_quant[q];
        if (bits >= 10)
            tdc[0] -= 1024.0f; /* bink2f_predict_dc adds it back */
    } else {
        for (int i = 0; i < num_dc; i++)
            tdc[i] *= bink2f_dc_quant[q];
    }
    bink2f_predict_dc(c, is_luma, 0, 2047, flags, tdc);
}

/* An intra luma macroblock at (mbx, mby) (in macroblocks). */
static int bink2d_decode_intra_luma(Bink2Context *c, float block[4][64], uint8_t *dst, int stride, int flags,
                                    int mbx, int mby)
{
    GetBitContext *gb = &c->gb;
    Bink2DFrame *d = c->kb2d;
    float *dc = c->current_dc[c->mb_pos].dc[c->comp];
    const int q = get_bits(gb, 4);
    int nz[4];

    bink2d_decode_intra_dc(c, gb, 1, q, flags);
    for (int i = 0; i < 4; i++) {
        bink2d_decode_ac(gb, bink2f_luma_scan, bink2d_bands_luma_intra, block, bink2f_ac_quant[q],
                         bink2f_luma_intra_qmat, nz);
        for (int j = 0; j < 4; j++) {
            const int pos = luma_repos[i * 4 + j];

            block[j][0] = dc[i * 4 + j] * 0.125f;
            bink2f_idct_put(dst + (pos & 3) * 8 + (pos >> 2) * 8 * stride, stride, block[j]);
            if (c->comp == 0)
                d->info[0][(mby * 4 + (pos >> 2)) * d->bw[0] + mbx * 4 + (pos & 3)] =
                    bink2d_block_info(dc[i * 4 + j], nz[j]);
        }
    }
    return 0;
}

static int bink2d_decode_intra_chroma(Bink2Context *c, float block[4][64], uint8_t *dst, int stride, int flags,
                                      int mbx, int mby)
{
    GetBitContext *gb = &c->gb;
    Bink2DFrame *d = c->kb2d;
    float *dc = c->current_dc[c->mb_pos].dc[c->comp];
    const int q = get_bits(gb, 4);
    int nz[4];

    bink2d_decode_intra_dc(c, gb, 0, q, flags);
    bink2d_decode_ac(gb, bink2f_chroma_scan, bink2d_bands_chroma, block, bink2f_ac_quant[q], bink2f_chroma_qmat,
                     nz);
    for (int j = 0; j < 4; j++) {
        block[j][0] = dc[j] * 0.125f;
        bink2f_idct_put(dst + (j & 1) * 8 + (j >> 1) * 8 * stride, stride, block[j]);
        d->info[c->comp][(mby * 2 + (j >> 1)) * d->bw[c->comp] + mbx * 2 + (j & 1)] = bink2d_block_info(dc[j], nz[j]);
    }
    return 0;
}

/* The DCs of a residue block's 16 luma or 4 chroma blocks from their coded differences t[]: predicted inside the
 * block only, from 0 (KB2f's top left case without the extra DC), and kept in 0..2047 like intra DCs - so a residue
 * DC never darkens (WWE '13's decoder does so; KB2f allows -1023..1023). */
static void bink2d_inter_dc(float *dc, const float *t, int n)
{
    dc[0] = av_clipf(t[0], 0, 2047);
    dc[1] = av_clipf(dc[0] + t[1], 0, 2047);
    dc[2] = av_clipf(DC_MPRED2(dc[0], dc[1]) + t[2], 0, 2047);
    dc[3] = av_clipf(DC_MPRED(dc[0], dc[2], dc[1]) + t[3], 0, 2047);
    if (n == 4)
        return;
    dc[4] = av_clipf(DC_MPRED2(dc[1], dc[3]) + t[4], 0, 2047);
    dc[5] = av_clipf(dc[4] + t[5], 0, 2047);
    dc[6] = av_clipf(DC_MPRED(dc[1], dc[3], dc[4]) + t[6], 0, 2047);
    dc[7] = av_clipf(DC_MPRED(dc[4], dc[6], dc[5]) + t[7], 0, 2047);
    dc[8] = av_clipf(DC_MPRED2(dc[2], dc[3]) + t[8], 0, 2047);
    dc[9] = av_clipf(DC_MPRED(dc[2], dc[8], dc[3]) + t[9], 0, 2047);
    dc[10] = av_clipf(DC_MPRED2(dc[8], dc[9]) + t[10], 0, 2047);
    dc[11] = av_clipf(DC_MPRED(dc[8], dc[10], dc[9]) + t[11], 0, 2047);
    dc[12] = av_clipf(DC_MPRED(dc[3], dc[9], dc[6]) + t[12], 0, 2047);
    dc[13] = av_clipf(DC_MPRED(dc[6], dc[12], dc[7]) + t[13], 0, 2047);
    dc[14] = av_clipf(DC_MPRED(dc[9], dc[11], dc[12]) + t[14], 0, 2047);
    dc[15] = av_clipf(DC_MPRED(dc[12], dc[14], dc[13]) + t[15], 0, 2047);
}

/* The coded DC differences of n blocks (dequantized), as in bink2f_decode_dc, without the extra DC. */
static void bink2d_read_dc(GetBitContext *gb, float *t, int n, int q)
{
    int dc_bits = get_bits(gb, 3);

    memset(t, 0, sizeof(*t) * n);
    if (dc_bits == 7)
        dc_bits += get_bits(gb, 2);
    if (dc_bits) {
        for (int j = 0; j < n; j += 4) {
            for (int i = 0; i < 4; i++)
                t[i + j] = get_bits(gb, dc_bits);
            for (int i = 0; i < 4; i++)
                if (t[i + j] && get_bits1(gb))
                    t[i + j] = -t[i + j];
        }
    }
    for (int i = 0; i < n; i++)
        t[i] *= bink2f_dc_quant[q];
}

/* A residue block whose 4 blocks (of a quarter, or of a chroma plane) have no AC coefficient: the DC / 8, rounded
 * by adding (almost) 0.5 and truncating towards zero (so -0.75 gives 0, unlike the IDCT's rounding), added to the
 * pixels. */
static void bink2d_add_dc(uint8_t *dst, int stride, float dc)
{
    const int v = (int)(dc * 0.125f + BINK2D_HALF);

    for (int y = 0; y < 8; y++, dst += stride)
        for (int x = 0; x < 8; x++)
            dst[x] = av_clip_uint8(dst[x] + v);
}

static int bink2d_decode_inter_luma(Bink2Context *c, float block[4][64], uint8_t *dst, int stride)
{
    GetBitContext *gb = &c->gb;
    float *dc = c->current_dc[c->mb_pos].dc[c->comp];
    const int q = get_bits(gb, 4);
    float t[16];
    int nz[4];

    bink2d_read_dc(gb, t, 16, q);
    bink2d_inter_dc(dc, t, 16);
    for (int i = 0; i < 4; i++) {
        bink2d_decode_ac(gb, bink2f_luma_scan, bink2d_bands_luma_inter, block, bink2f_ac_quant[q],
                         bink2f_luma_inter_qmat, nz);
        for (int j = 0; j < 4; j++) {
            uint8_t *b = dst + (luma_repos[i * 4 + j] & 3) * 8 + (luma_repos[i * 4 + j] >> 2) * 8 * stride;

            if (nz[0] | nz[1] | nz[2] | nz[3]) {
                block[j][0] = dc[i * 4 + j] * 0.125f;
                bink2f_idct_add(b, stride, block[j]);
            } else {
                bink2d_add_dc(b, stride, dc[i * 4 + j]);
            }
        }
    }
    return 0;
}

/* Both chroma planes of a residue block (dst[0]: comp 1's plane, dst[1]: comp 2's). */
static int bink2d_decode_inter_chroma(Bink2Context *c, float block[4][64], uint8_t *dst[2], int stride[2])
{
    GetBitContext *gb = &c->gb;
    const int q = get_bits(gb, 4);
    float t[8];
    int nz[4];

    bink2d_read_dc(gb, t, 8, q);
    for (int p = 0; p < 2; p++)
        bink2d_inter_dc(c->current_dc[c->mb_pos].dc[1 + p], t + 4 * p, 4);
    for (int p = 0; p < 2; p++) {
        const float *dc = c->current_dc[c->mb_pos].dc[1 + p];

        bink2d_decode_ac(gb, bink2f_chroma_scan, bink2d_bands_chroma, block, bink2f_ac_quant[q],
                         bink2f_chroma_qmat, nz);
        for (int j = 0; j < 4; j++) {
            uint8_t *b = dst[p] + (j & 1) * 8 + (j >> 1) * 8 * stride[p];

            if (nz[0] | nz[1] | nz[2] | nz[3]) {
                block[j][0] = dc[j] * 0.125f;
                bink2f_idct_add(b, stride[p], block[j]);
            } else {
                bink2d_add_dc(b, stride[p], dc[j]);
            }
        }
    }
    return 0;
}

/* Motion compensation, as KB2f's (bink2f_y_mc, bink2f_c_mc) except for the diagonal positions, where WWE '13
 * filters vertically the unrounded horizontal results instead of 8-bit ones: luma with the 6-tap filter on the
 * unclipped horizontal values, chroma keeping the horizontal bilinear sums at 16x scale. */
static const uint8_t bink2d_cw[4][2] = { { 8, 0 }, { 6, 2 }, { 4, 4 }, { 2, 6 } };

#define BINK2D_LFILTER(s, i)     (((((s)[0] + (s)[i]) * 19) >> 1) - ((s)[-(i)] + (s)[2 * (i)]) * 2 + (((s)[-2 * (i)] + (s)[3 * (i)]) >> 1) + 8 >> 4)

static void bink2d_block_mc(uint8_t *dst, int stride, const uint8_t *src, int sstride, int luma, int fx, int fy)
{
    const int n = luma ? 16 : 8;

    for (int y = 0; y < n; y++, dst += stride, src += sstride)
        for (int x = 0; x < n; x++) {
            const uint8_t *s = src + x;
            int v;

            if (luma && fx && fy) {
                int t[6];

                for (int k = 0; k < 6; k++)
                    t[k] = BINK2D_LFILTER(s + (k - 2) * sstride, 1);
                v = BINK2D_LFILTER(t + 2, 1);
            } else if (luma && fx) {
                v = BINK2D_LFILTER(s, 1);
            } else if (luma && fy) {
                v = BINK2D_LFILTER(s, sstride);
            } else if (luma || (!fx && !fy)) {
                v = s[0];
            } else if (!fy) {
                v = (bink2d_cw[fx][0] * s[0] + bink2d_cw[fx][1] * s[1] + 4) >> 3;
            } else if (!fx) {
                v = (bink2d_cw[fy][0] * s[0] + bink2d_cw[fy][1] * s[sstride] + 4) >> 3;
            } else {
                const int h0 = 2 * (bink2d_cw[fx][0] * s[0] + bink2d_cw[fx][1] * s[1]);
                const int h1 = 2 * (bink2d_cw[fx][0] * s[sstride] + bink2d_cw[fx][1] * s[sstride + 1]);

                v = (((bink2d_cw[fy][0] * h0 + bink2d_cw[fy][1] * h1) >> 3) + 8) >> 4;
            }
            dst[x] = av_clip_uint8(v);
        }
}

/* Motion compensation of a macroblock's 4 areas (luma: 16x16, half-pel; chroma: 8x8, quarter-pel). */
static void bink2d_mcompensate(Bink2Context *c, int x, int y, uint8_t *dst, int stride, uint8_t *src, int sstride,
                               int width, int height, int luma)
{
    const MVectors *mv = &c->current_mv[c->mb_pos].mv;
    const int sh = luma ? 1 : 2, m = (1 << sh) - 1, bs = luma ? 16 : 8;

    for (int i = 0; i < 4; i++) {
        const int bx = x + (i & 1) * bs, by = y + (i >> 1) * bs;
        const int mx = mv->v[i][0], my = mv->v[i][1], sx = (mx >> sh) + bx, sy = (my >> sh) + by;

        if (sx < 0 || sx >= width || sy < 0 || sy >= height)
            continue;
        bink2d_block_mc(dst + bx + (by - y) * stride, stride, src + sx + sy * sstride, sstride, luma, mx & m, my & m);
    }
}

/* Records a macroblock for the post-processing: its blocks' information (0: not intra) and its motion vectors. */
static void bink2d_note_mb(Bink2Context *c, int mbx, int mby, int intra, const MVectors *mv)
{
    Bink2DFrame *d = c->kb2d;

    if (!intra) {
        for (int p = 0; p < 3; p++) {
            const int n = p ? 2 : 4;

            for (int j = 0; j < n; j++)
                memset(d->info[p] + (mby * n + j) * d->bw[p] + mbx * n, 0, n * sizeof(uint16_t));
        }
    }
    for (int i = 0; i < 4; i++) {
        int32_t *v = d->mv[(mby * 2 + (i >> 1)) * d->mvw + mbx * 2 + (i & 1)];

        v[0] = mv ? mv->v[i][0] : 0;
        v[1] = mv ? mv->v[i][1] : 0;
    }
}

static int bink2d_decode_slice(Bink2Context *c, uint8_t *dst[4], int stride[4], uint8_t *src[4], int sstride[4],
                               int is_kf, int start, int end)
{
    GetBitContext *gb = &c->gb;
    const int w = c->avctx->width, h = c->avctx->height;
    int flags, ret = 0;

    memset(c->prev_mv, 0, ((c->avctx->width + 31) / 32) * sizeof(*c->prev_mv));

    for (int y = start; y < end; y += 32) {
        memset(c->current_mv, 0, ((c->avctx->width + 31) / 32) * sizeof(*c->current_mv));

        for (int x = 0; x < c->avctx->width; x += 32) {
            MVectors mv = { 0 };
            const int type = is_kf ? INTRA_BLOCK : get_bits(gb, 2), mbx = x / 32, mby = y / 32;

            c->mb_pos = mbx;
            c->current_dc[c->mb_pos].block_type = type;

            flags = 0;
            if (y == start)
                flags |= 0x80;
            if (!x)
                flags |= 0x20;
            if (x == 32)
                flags |= 0x200;
            if (x + 32 >= c->avctx->width)
                flags |= 0x40;

            switch (type) {
            case INTRA_BLOCK:
                if (!(flags & 0xA0) && c->prev_dc[c->mb_pos - 1].block_type != INTRA_BLOCK) {
                    bink2f_average_luma  (c, x  -32, -32, dst[0], stride[0], c->prev_dc[c->mb_pos - 1].dc[0]);
                    bink2f_average_chroma(c, x/2-16, -16, dst[2], stride[2], c->prev_dc[c->mb_pos - 1].dc[1]);
                    bink2f_average_chroma(c, x/2-16, -16, dst[1], stride[1], c->prev_dc[c->mb_pos - 1].dc[2]);
                }
                if (!(flags & 0x20) && c->current_dc[c->mb_pos - 1].block_type != INTRA_BLOCK) {
                    bink2f_average_luma  (c, x  -32, 0, dst[0], stride[0], c->current_dc[c->mb_pos - 1].dc[0]);
                    bink2f_average_chroma(c, x/2-16, 0, dst[2], stride[2], c->current_dc[c->mb_pos - 1].dc[1]);
                    bink2f_average_chroma(c, x/2-16, 0, dst[1], stride[1], c->current_dc[c->mb_pos - 1].dc[2]);
                }
                if ((flags & 0x20) && !(flags & 0x80) && c->prev_dc[c->mb_pos + 1].block_type != INTRA_BLOCK) {
                    bink2f_average_luma  (c, x  +32, -32, dst[0], stride[0], c->prev_dc[c->mb_pos + 1].dc[0]);
                    bink2f_average_chroma(c, x/2+16, -16, dst[2], stride[2], c->prev_dc[c->mb_pos + 1].dc[1]);
                    bink2f_average_chroma(c, x/2+16, -16, dst[1], stride[1], c->prev_dc[c->mb_pos + 1].dc[2]);
                }
                if (!(flags & 0x80) && c->prev_dc[c->mb_pos].block_type != INTRA_BLOCK) {
                    bink2f_average_luma  (c, x,   -32, dst[0], stride[0], c->prev_dc[c->mb_pos].dc[0]);
                    bink2f_average_chroma(c, x/2, -16, dst[2], stride[2], c->prev_dc[c->mb_pos].dc[1]);
                    bink2f_average_chroma(c, x/2, -16, dst[1], stride[1], c->prev_dc[c->mb_pos].dc[2]);
                }

                bink2f_predict_mv(c, x, y, flags, mv);
                c->comp = 0;
                ret = bink2d_decode_intra_luma(c, c->block, dst[0] + x, stride[0], flags, mbx, mby);
                if (ret < 0)
                    goto fail;
                c->comp = 1;
                ret = bink2d_decode_intra_chroma(c, c->block, dst[2] + x/2, stride[2], flags, mbx, mby);
                if (ret < 0)
                    goto fail;
                c->comp = 2;
                ret = bink2d_decode_intra_chroma(c, c->block, dst[1] + x/2, stride[1], flags, mbx, mby);
                if (ret < 0)
                    goto fail;
                if (c->has_alpha) {
                    c->comp = 3;
                    ret = bink2d_decode_intra_luma(c, c->block, dst[3] + x, stride[3], flags, mbx, mby);
                    if (ret < 0)
                        goto fail;
                }
                bink2d_note_mb(c, mbx, mby, 1, &c->current_mv[c->mb_pos].mv);
                break;
            case SKIP_BLOCK:
                copy_block16(dst[0] + x, src[0] + x + sstride[0] * y,
                             stride[0], sstride[0], 32);
                copy_block16(dst[0] + x + 16, src[0] + x + 16 + sstride[0] * y,
                             stride[0], sstride[0], 32);
                copy_block16(dst[1] + (x/2), src[1] + (x/2) + sstride[1] * (y/2),
                             stride[1], sstride[1], 16);
                copy_block16(dst[2] + (x/2), src[2] + (x/2) + sstride[2] * (y/2),
                             stride[2], sstride[2], 16);
                if (c->has_alpha) {
                    copy_block16(dst[3] + x, src[3] + x + sstride[3] * y,
                                 stride[3], sstride[3], 32);
                    copy_block16(dst[3] + x + 16, src[3] + x + 16 + sstride[3] * y,
                                 stride[3], sstride[3], 32);
                }
                bink2d_note_mb(c, mbx, mby, 0, NULL);
                break;
            case MOTION_BLOCK:
            case RESIDUE_BLOCK:
                bink2f_decode_mv(c, gb, x, y, flags, &mv);
                bink2f_predict_mv(c, x, y, flags, mv);
                bink2d_mcompensate(c, x, y, dst[0], stride[0], src[0], sstride[0], w, h, 1);
                bink2d_mcompensate(c, x/2, y/2, dst[2], stride[2], src[2], sstride[2], w/2, h/2, 0);
                bink2d_mcompensate(c, x/2, y/2, dst[1], stride[1], src[1], sstride[1], w/2, h/2, 0);
                if (type == RESIDUE_BLOCK) {
                    uint8_t *cdst[2] = { dst[2] + x/2, dst[1] + x/2 };
                    int cstride[2] = { stride[2], stride[1] };

                    c->comp = 0;
                    ret = bink2d_decode_inter_luma(c, c->block, dst[0] + x, stride[0]);
                    if (ret < 0)
                        goto fail;
                    ret = bink2d_decode_inter_chroma(c, c->block, cdst, cstride);
                    if (ret < 0)
                        goto fail;
                    if (c->has_alpha) {
                        c->comp = 3;
                        ret = bink2d_decode_inter_luma(c, c->block, dst[3] + x, stride[3]);
                        if (ret < 0)
                            goto fail;
                    }
                }
                bink2d_note_mb(c, mbx, mby, 0, &c->current_mv[c->mb_pos].mv);
                break;
            default:
                return AVERROR_INVALIDDATA;
            }
        }

        dst[0] += stride[0] * 32;
        dst[1] += stride[1] * 16;
        dst[2] += stride[2] * 16;
        dst[3] += stride[3] * 32;

        FFSWAP(MVPredict *, c->current_mv, c->prev_mv);
        FFSWAP(DCPredict *, c->current_dc, c->prev_dc);
    }
fail:
    emms_c();

    return ret;
}

/* ---- post-processing ---- */

/* The weights of the smooth DC surface, measured from WWE '13's decoder: pixel (y, x) of the block gets
 * sum over its 3x3 neighbourhood of weight * DC (+ BINK2D_HALF, truncated). Only a quarter is stored, the rest by symmetry:
 * corner[y][x] for the top left neighbour (rows 5-7 are 0), side[y][x] for the top one (x < 4, mirrored; rows 5-7
 * are 0; the left one is its transpose), centre[y][x] (y, x < 4, mirrored both ways). They add up to 1.00028 / 8. */
static const float bink2d_dcw_corner[5][5] = {
    { 0.0241113249f, 0.0175668225f, 0.0115389917f, 0.00637227902f, 0.00241113245f },
    { 0.0175668225f, 0.0126756681f, 0.00819785055f, 0.0044089281f, 0.00158445851f },
    { 0.0115389917f, 0.00819785055f, 0.00516671268f, 0.00265224581f, 0.00086111878f },
    { 0.00637227902f, 0.0044089281f, 0.00265224581f, 0.00124001107f, 0.000310002768f },
    { 0.00241113245f, 0.00158445851f, 0.00086111878f, 0.000310002768f, 0.0f },
};
static const float bink2d_dcw_side[5][4] = {
    { 0.0308280513f, 0.0373725556f, 0.0434003845f, 0.0461559668f },
    { 0.0225957558f, 0.027486911f, 0.0319647267f, 0.0341691934f },
    { 0.0149834668f, 0.018324608f, 0.0213557463f, 0.0230090935f },
    { 0.00840451941f, 0.0103678703f, 0.0121245524f, 0.0132267838f },
    { 0.00327225123f, 0.00409892527f, 0.00482226489f, 0.00537338108f },
};
static const float bink2d_dcw_centre[4][4] = {
    { 0.0392670147f, 0.0474993102f, 0.0551116019f, 0.0584182963f },
    { 0.0474993102f, 0.0573849529f, 0.0665472597f, 0.0704050735f },
    { 0.0551116019f, 0.0665472597f, 0.0771562383f, 0.0815651715f },
    { 0.0584182963f, 0.0704050735f, 0.0815651715f, 0.0859740973f },
};

/* weight of neighbour (dy, dx) (-1..1) for pixel (y, x) */
static float bink2d_dc_weight(int dy, int dx, int y, int x)
{
    if (dy > 0)
        y = 7 - y;
    if (dx > 0)
        x = 7 - x;
    if (dy && dx)
        return y < 5 && x < 5 ? bink2d_dcw_corner[y][x] : 0.0f;
    if (dy)
        return y < 5 ? bink2d_dcw_side[y][x < 4 ? x : 7 - x] : 0.0f;
    if (dx)
        return x < 5 ? bink2d_dcw_side[x][y < 4 ? y : 7 - y] : 0.0f;
    return bink2d_dcw_centre[y < 4 ? y : 7 - y][x < 4 ? x : 7 - x];
}

/* Redraws the DC-only intra blocks of a plane. */
static void bink2d_fill_dc(const Bink2DFrame *d, int p, uint8_t *dst, int stride, int thr)
{
    const int bw = d->bw[p], bh = d->bh[p];
    const uint16_t *info = d->info[p];

    for (int by = 0; by < bh; by++)
        for (int bx = 0; bx < bw; bx++) {
            const int craw = (int16_t)info[by * bw + bx];
            float n[3][3];
            uint8_t *o = dst + by * 8 * stride + bx * 8;

            if ((info[by * bw + bx] >> 13) != 7)
                continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    const int ny = av_clip(by + dy, 0, bh - 1), nx = av_clip(bx + dx, 0, bw - 1);
                    const int nraw = (int16_t)info[ny * bw + nx];
                    const int diff = FFMIN(abs(nraw - craw), 32767);

                    n[dy + 1][dx + 1] = (float)((thr > diff ? nraw : craw) & 0x1FFF);
                }
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    /* summed in WWE '13's order: the nearer row's left corner and left side first */
                    const int side_y = y < 4 ? -1 : 1;
                    static const int8_t order[9][2] = {
                        { 1, -1 }, { 0, -1 }, { 1, 0 }, { 0, 0 }, { 1, 1 }, { 0, 1 }, { -1, -1 }, { -1, 0 }, { -1, 1 },
                    };
                    float s = bink2d_dc_weight(side_y, -1, y, x) * n[1 + side_y][0];

                    for (int k = 1; k < 9; k++) {
                        const int dy = side_y * order[k][0], dx = order[k][1];
                        const float t = bink2d_dc_weight(dy, dx, y, x) * n[dy + 1][dx + 1];

                        s += t;
                    }
                    o[y * stride + x] = av_clip_uint8((int)(s + BINK2D_HALF));
                }
        }
}

/* The edge filter: d = q0 - p0; p1 += (2d + 8) >> 4, p0 += (6d + 8) >> 4, q0 -= (6d + 8) >> 4, q1 -= (2d + 8) >> 4,
 * each only if its side's code allows (code & 3 for p, code >> 2 for q: 0 none, 1 the nearest pixel, 2-3 both). */
static void bink2d_filter_edge(uint8_t *q0, ptrdiff_t across, ptrdiff_t along, int code)
{
    const int pc = code & 3, qc = code >> 2;

    for (int i = 0; i < 8; i++, q0 += along) {
        const int p1 = q0[-2 * across], p0 = q0[-across], c0 = q0[0], c1 = q0[across];
        const int d = c0 - p0, s = (2 * d + 8) >> 4, l = (6 * d + 8) >> 4;

        if (pc >= 2)
            q0[-2 * across] = av_clip_uint8(p1 + s);
        if (pc >= 1)
            q0[-across] = av_clip_uint8(p0 + l);
        if (qc >= 1)
            q0[0] = av_clip_uint8(c0 - l);
        if (qc >= 2)
            q0[across] = av_clip_uint8(c1 - s);
    }
}

/* The code of the edge between blocks a (left / top) and b of plane p, at 16x16 luma areas ma and mb. */
static int bink2d_edge_code(const Bink2DFrame *d, uint16_t a, uint16_t b, int ma, int mb)
{
    if ((a | b) & 0x8000)
        return (a >> 13 & 3) * !!(a & 0x8000) | (b >> 13 & 3) * !!(b & 0x8000) << 2;
    return d->mv[ma][0] != d->mv[mb][0] || d->mv[ma][1] != d->mv[mb][1] ? 10 : 0;
}

static void bink2d_deblock(const Bink2DFrame *d, int p, uint8_t *dst, int stride, int vertical)
{
    const int bw = d->bw[p], bh = d->bh[p], sh = p ? 0 : 1; /* block -> 16x16 luma area */

    for (int by = 0; by < bh; by++)
        for (int bx = 0; bx < bw; bx++) {
            const int ax = bx - (vertical ? 1 : 0), ay = by - (vertical ? 0 : 1);
            int code;

            if (ax < 0 || ay < 0)
                continue;
            code = bink2d_edge_code(d, d->info[p][ay * bw + ax], d->info[p][by * bw + bx],
                                    (ay >> sh) * d->mvw + (ax >> sh), (by >> sh) * d->mvw + (bx >> sh));
            if (!code || code == 15)
                continue;
            if (vertical)
                bink2d_filter_edge(dst + by * 8 * stride + bx * 8, 1, stride, code);
            else
                bink2d_filter_edge(dst + by * 8 * stride + bx * 8, stride, 1, code);
        }
}

/* After the slices: plane[0] luma, plane[1] / [2] the planes of comp 1 / 2. */
static void bink2d_postprocess(Bink2Context *c, uint8_t *plane[3], int stride[3])
{
    const Bink2DFrame *d = c->kb2d;
    const int thr = (c->frame_flags & 0xFF) * 8;

    for (int p = 0; p < 3; p++)
        bink2d_fill_dc(d, p, plane[p], stride[p], thr);
    if (!(c->frame_flags & 0x4000))
        for (int p = 0; p < 3; p++)
            bink2d_deblock(d, p, plane[p], stride[p], 1);
    if (!(c->frame_flags & 0x8000))
        for (int p = 0; p < 3; p++)
            bink2d_deblock(d, p, plane[p], stride[p], 0);
}
