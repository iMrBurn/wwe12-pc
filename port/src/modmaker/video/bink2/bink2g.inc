/*
 * Bink video 2 decoder
 * Copyright (c) 2014 Konstantin Shishkov
 * Copyright (c) 2019 Paul B Mahol
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

static inline void bink2g_idct_1d(int16_t *blk, int step, int shift)
{
#define idct_mul_a(val) ((val) + ((val) >> 2))
#define idct_mul_b(val) ((val) >> 1)
#define idct_mul_c(val) ((val) - ((val) >> 2) - ((val) >> 4))
#define idct_mul_d(val) ((val) + ((val) >> 2) - ((val) >> 4))
#define idct_mul_e(val) ((val) >> 2)
    int tmp00 =  blk[3*step] + blk[5*step];
    int tmp01 =  blk[3*step] - blk[5*step];
    int tmp02 =  idct_mul_a(blk[2*step]) + idct_mul_b(blk[6*step]);
    int tmp03 =  idct_mul_b(blk[2*step]) - idct_mul_a(blk[6*step]);
    int tmp0  = (blk[0*step] + blk[4*step]) + tmp02;
    int tmp1  = (blk[0*step] + blk[4*step]) - tmp02;
    int tmp2  =  blk[0*step] - blk[4*step];
    int tmp3  =  blk[1*step] + tmp00;
    int tmp4  =  blk[1*step] - tmp00;
    int tmp5  =  tmp01 + blk[7*step];
    int tmp6  =  tmp01 - blk[7*step];
    int tmp7  =  tmp4 + idct_mul_c(tmp6);
    int tmp8  =  idct_mul_c(tmp4) - tmp6;
    int tmp9  =  idct_mul_d(tmp3) + idct_mul_e(tmp5);
    int tmp10 =  idct_mul_e(tmp3) - idct_mul_d(tmp5);
    int tmp11 =  tmp2 + tmp03;
    int tmp12 =  tmp2 - tmp03;

    blk[0*step] = (tmp0  + tmp9)  >> shift;
    blk[1*step] = (tmp11 + tmp7)  >> shift;
    blk[2*step] = (tmp12 + tmp8)  >> shift;
    blk[3*step] = (tmp1  + tmp10) >> shift;
    blk[4*step] = (tmp1  - tmp10) >> shift;
    blk[5*step] = (tmp12 - tmp8)  >> shift;
    blk[6*step] = (tmp11 - tmp7)  >> shift;
    blk[7*step] = (tmp0  - tmp9)  >> shift;
}

static void bink2g_idct_put(uint8_t *dst, int stride, int16_t *block)
{
    for (int i = 0; i < 8; i++)
        bink2g_idct_1d(block + i, 8, 0);
    for (int i = 0; i < 8; i++)
        bink2g_idct_1d(block + i * 8, 1, 6);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++)
            dst[j] = av_clip_uint8(block[j * 8 + i]);
        dst += stride;
    }
}

static void bink2g_idct_add(uint8_t *dst, int stride, int16_t *block)
{
    for (int i = 0; i < 8; i++)
        bink2g_idct_1d(block + i, 8, 0);
    for (int i = 0; i < 8; i++)
        bink2g_idct_1d(block + i * 8, 1, 6);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++)
            dst[j] = av_clip_uint8(dst[j] + block[j * 8 + i]);
        dst += stride;
    }
}

static int bink2g_get_type(GetBitContext *gb, int *lru)
{
    int val;

    switch (get_unary(gb, 1, 3)) {
    case 0:
        val = lru[0];
        break;
    case 1:
        val = lru[1];
        FFSWAP(int, lru[0], lru[1]);
        break;
    case 2:
        val = lru[3];
        FFSWAP(int, lru[2], lru[3]);
        break;
    case 3:
        val = lru[2];
        FFSWAP(int, lru[1], lru[2]);
        break;
    }

    return val;
}

static int bink2g_decode_dq(GetBitContext *gb)
{
    int dq = get_unary(gb, 1, 4);

    if (dq == 3)
        dq += get_bits1(gb);
    else if (dq == 4)
        dq += get_bits(gb, 5) + 1;
    if (dq && get_bits1(gb))
        dq = -dq;

    return dq;
}

static unsigned bink2g_decode_cbp_luma(Bink2Context *c,
                                       GetBitContext *gb, unsigned prev_cbp)
{
    unsigned ones = 0, cbp, mask;

    for (int i = 0; i < 16; i++) {
        if (prev_cbp & (1 << i))
            ones += 1;
    }

    cbp = 0;
    mask = 0;
    if (ones > 7) {
        ones = 16 - ones;
        mask = 0xFFFF;
    }

    if (get_bits1(gb) == 0) {
        if (ones < 4) {
            for (int j = 0; j < 16; j += 4)
                if (!get_bits1(gb))
                    cbp |= get_bits(gb, 4) << j;
        } else {
            cbp = get_bits(gb, 16);
        }
    }

    cbp ^= mask;
    if (!(c->frame_flags & 0x40000) || cbp) {
        if (get_bits1(gb))
            cbp = cbp | cbp << 16;
    }

    return cbp;
}

static unsigned bink2g_decode_cbp_chroma(GetBitContext *gb, unsigned prev_cbp)
{
    unsigned cbp;

    cbp = prev_cbp & 0xF0000 | bink2g_chroma_cbp_pat[prev_cbp & 0xF];
    if (get_bits1(gb) == 0) {
        cbp = get_bits(gb, 4);
        if (get_bits1(gb))
            cbp |= cbp << 16;
    }

    return cbp;
}

static void bink2g_predict_dc(Bink2Context *c,
                              int is_luma, int mindc, int maxdc,
                              int flags, int tdc[16])
{
    int *LTdc = c->prev_idc[FFMAX(c->mb_pos - 1, 0)].dc[c->comp];
    int *Tdc = c->prev_idc[c->mb_pos].dc[c->comp];
    int *Ldc = c->current_idc[FFMAX(c->mb_pos - 1, 0)].dc[c->comp];
    int *dc = c->current_idc[c->mb_pos].dc[c->comp];

    if (is_luma && (flags & 0x20) && (flags & 0x80)) {
        dc[0]  = av_clip((mindc < 0 ? 0 : 1024) + tdc[0], mindc, maxdc);
        dc[1]  = av_clip(dc[0] + tdc[1], mindc, maxdc);
        dc[2]  = av_clip(DC_MPRED2(dc[0], dc[1]) + tdc[2], mindc, maxdc);
        dc[3]  = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
        dc[4]  = av_clip(DC_MPRED2(dc[1], dc[3]) + tdc[4], mindc, maxdc);
        dc[5]  = av_clip(dc[4] + tdc[5], mindc, maxdc);
        dc[6]  = av_clip(DC_MPRED(dc[1], dc[3], dc[4]) + tdc[6], mindc, maxdc);
        dc[7]  = av_clip(DC_MPRED(dc[4], dc[6], dc[5]) + tdc[7], mindc, maxdc);
        dc[8]  = av_clip(DC_MPRED2(dc[2], dc[3]) + tdc[8], mindc, maxdc);
        dc[9]  = av_clip(DC_MPRED(dc[2], dc[8], dc[3]) + tdc[9], mindc, maxdc);
        dc[10] = av_clip(DC_MPRED2(dc[8], dc[9]) + tdc[10], mindc, maxdc);
        dc[11] = av_clip(DC_MPRED(dc[8], dc[10], dc[9]) + tdc[11], mindc, maxdc);
        dc[12] = av_clip(DC_MPRED(dc[3], dc[9], dc[6]) + tdc[12], mindc, maxdc);
        dc[13] = av_clip(DC_MPRED(dc[6], dc[12], dc[7]) + tdc[13], mindc, maxdc);
        dc[14] = av_clip(DC_MPRED(dc[9], dc[11], dc[12]) + tdc[14], mindc, maxdc);
        dc[15] = av_clip(DC_MPRED(dc[12], dc[14], dc[13]) + tdc[15], mindc, maxdc);
    } else if (is_luma && (flags & 0x80)) {
        dc[0]  = av_clip(DC_MPRED2(Ldc[5], Ldc[7]) + tdc[0], mindc, maxdc);
        dc[1]  = av_clip(dc[0] + tdc[1], mindc, maxdc);
        dc[2]  = av_clip(DC_MPRED(Ldc[5], Ldc[7], dc[0]) + tdc[2], mindc, maxdc);
        dc[3]  = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
        dc[4]  = av_clip(DC_MPRED2(dc[1], dc[3]) + tdc[4], mindc, maxdc);
        dc[5]  = av_clip(dc[4] + tdc[5], mindc, maxdc);
        dc[6]  = av_clip(DC_MPRED(dc[1], dc[3], dc[4]) + tdc[6], mindc, maxdc);
        dc[7]  = av_clip(DC_MPRED(dc[4], dc[6], dc[5]) + tdc[7], mindc, maxdc);
        dc[8]  = av_clip(DC_MPRED(Ldc[7], Ldc[13], dc[2]) + tdc[8], mindc, maxdc);
        dc[9]  = av_clip(DC_MPRED(dc[2], dc[8], dc[3]) + tdc[9], mindc, maxdc);
        dc[10] = av_clip(DC_MPRED(Ldc[13], Ldc[15], dc[8]) + tdc[10], mindc, maxdc);
        dc[11] = av_clip(DC_MPRED(dc[8], dc[10], dc[9]) + tdc[11], mindc, maxdc);
        dc[12] = av_clip(DC_MPRED(dc[3], dc[9], dc[6]) + tdc[12], mindc, maxdc);
        dc[13] = av_clip(DC_MPRED(dc[6], dc[12], dc[7]) + tdc[13], mindc, maxdc);
        dc[14] = av_clip(DC_MPRED(dc[9], dc[11], dc[12]) + tdc[14], mindc, maxdc);
        dc[15] = av_clip(DC_MPRED(dc[12], dc[14], dc[13]) + tdc[15], mindc, maxdc);
    } else if (is_luma && (flags & 0x20)) {
        dc[0]  = av_clip(DC_MPRED2(Tdc[10], Tdc[11]) + tdc[0], mindc, maxdc);
        dc[1]  = av_clip(DC_MPRED(Tdc[10], dc[0], Tdc[11]) + tdc[1], mindc, maxdc);
        dc[2]  = av_clip(DC_MPRED2(dc[0], dc[1]) + tdc[2], mindc, maxdc);
        dc[3]  = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
        dc[4]  = av_clip(DC_MPRED(Tdc[11], dc[1], Tdc[14]) + tdc[4], mindc, maxdc);
        dc[5]  = av_clip(DC_MPRED(Tdc[14], dc[4], Tdc[15]) + tdc[5], mindc, maxdc);
        dc[6]  = av_clip(DC_MPRED(dc[1], dc[3], dc[4]) + tdc[6], mindc, maxdc);
        dc[7]  = av_clip(DC_MPRED(dc[4], dc[6], dc[5]) + tdc[7], mindc, maxdc);
        dc[8]  = av_clip(DC_MPRED2(dc[2], dc[3]) + tdc[8], mindc, maxdc);
        dc[9]  = av_clip(DC_MPRED(dc[2], dc[8], dc[3]) + tdc[9], mindc, maxdc);
        dc[10] = av_clip(DC_MPRED2(dc[8], dc[9]) + tdc[10], mindc, maxdc);
        dc[11] = av_clip(DC_MPRED(dc[8], dc[10], dc[9]) + tdc[11], mindc, maxdc);
        dc[12] = av_clip(DC_MPRED(dc[3], dc[9], dc[6]) + tdc[12], mindc, maxdc);
        dc[13] = av_clip(DC_MPRED(dc[6], dc[12], dc[7]) + tdc[13], mindc, maxdc);
        dc[14] = av_clip(DC_MPRED(dc[9], dc[11], dc[12]) + tdc[14], mindc, maxdc);
        dc[15] = av_clip(DC_MPRED(dc[12], dc[14], dc[13]) + tdc[15], mindc, maxdc);
    } else if (is_luma) {
        dc[0]  = av_clip(DC_MPRED(LTdc[15], Ldc[5], Tdc[10]) + tdc[0], mindc, maxdc);
        dc[1]  = av_clip(DC_MPRED(Tdc[10], dc[0], Tdc[11]) + tdc[1], mindc, maxdc);
        dc[2]  = av_clip(DC_MPRED(Ldc[5], Ldc[7], dc[0]) + tdc[2], mindc, maxdc);
        dc[3]  = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
        dc[4]  = av_clip(DC_MPRED(Tdc[11], dc[1], Tdc[14]) + tdc[4], mindc, maxdc);
        dc[5]  = av_clip(DC_MPRED(Tdc[14], dc[4], Tdc[15]) + tdc[5], mindc, maxdc);
        dc[6]  = av_clip(DC_MPRED(dc[1], dc[3], dc[4]) + tdc[6], mindc, maxdc);
        dc[7]  = av_clip(DC_MPRED(dc[4], dc[6], dc[5]) + tdc[7], mindc, maxdc);
        dc[8]  = av_clip(DC_MPRED(Ldc[7], Ldc[13], dc[2]) + tdc[8], mindc, maxdc);
        dc[9]  = av_clip(DC_MPRED(dc[2], dc[8], dc[3]) + tdc[9], mindc, maxdc);
        dc[10] = av_clip(DC_MPRED(Ldc[13], Ldc[15], dc[8]) + tdc[10], mindc, maxdc);
        dc[11] = av_clip(DC_MPRED(dc[8], dc[10], dc[9]) + tdc[11], mindc, maxdc);
        dc[12] = av_clip(DC_MPRED(dc[3], dc[9], dc[6]) + tdc[12], mindc, maxdc);
        dc[13] = av_clip(DC_MPRED(dc[6], dc[12], dc[7]) + tdc[13], mindc, maxdc);
        dc[14] = av_clip(DC_MPRED(dc[9], dc[11], dc[12]) + tdc[14], mindc, maxdc);
        dc[15] = av_clip(DC_MPRED(dc[12], dc[14], dc[13]) + tdc[15], mindc, maxdc);
    } else if (!is_luma && (flags & 0x20) && (flags & 0x80)) {
        dc[0] = av_clip((mindc < 0 ? 0 : 1024) + tdc[0], mindc, maxdc);
        dc[1] = av_clip(dc[0] + tdc[1], mindc, maxdc);
        dc[2] = av_clip(DC_MPRED2(dc[0], dc[1]) + tdc[2], mindc, maxdc);
        dc[3] = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
    } else if (!is_luma && (flags & 0x80)) {
        dc[0] = av_clip(DC_MPRED2(Ldc[1], Ldc[3]) + tdc[0], mindc, maxdc);
        dc[1] = av_clip(dc[0] + tdc[1], mindc, maxdc);
        dc[2] = av_clip(DC_MPRED(Ldc[1], Ldc[3], dc[0]) + tdc[2], mindc, maxdc);
        dc[3] = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
    } else if (!is_luma && (flags & 0x20)) {
        dc[0] = av_clip(DC_MPRED2(Tdc[2], Tdc[3]) + tdc[0], mindc, maxdc);
        dc[1] = av_clip(DC_MPRED(Tdc[2], dc[0], Tdc[3]) + tdc[1], mindc, maxdc);
        dc[2] = av_clip(DC_MPRED2(dc[0], dc[1]) + tdc[2], mindc, maxdc);
        dc[3] = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
    } else if (!is_luma) {
        dc[0] = av_clip(DC_MPRED(LTdc[3], Ldc[1], Tdc[2]) + tdc[0], mindc, maxdc);
        dc[1] = av_clip(DC_MPRED(Tdc[2], dc[0], Tdc[3]) + tdc[1], mindc, maxdc);
        dc[2] = av_clip(DC_MPRED(Ldc[1], Ldc[3], dc[0]) + tdc[2], mindc, maxdc);
        dc[3] = av_clip(DC_MPRED(dc[0], dc[2], dc[1]) + tdc[3], mindc, maxdc);
    }
}

static void bink2g_decode_dc(Bink2Context *c, GetBitContext *gb, int *dc,
                             int is_luma, int q, int mindc, int maxdc,
                             int flags)
{
    const int num_dc = is_luma ? 16 : 4;
    int tdc[16];
    int pat;

    q = FFMAX(q, 8);
    pat = bink2g_dc_pat[q];

    memset(tdc, 0, sizeof(tdc));

    if (get_bits1(gb)) {
        for (int i = 0; i < num_dc; i++) {
            int cnt = get_unary(gb, 0, 12);

            if (cnt > 3)
                cnt = (1 << (cnt - 3)) + get_bits(gb, cnt - 3) + 2;
            if (cnt && get_bits1(gb))
                cnt = -cnt;
            tdc[i] = (cnt * pat + 0x200) >> 10;
        }
    }

    bink2g_predict_dc(c, is_luma, mindc, maxdc, flags, tdc);
}

static int bink2g_decode_ac(GetBitContext *gb, const uint8_t scan[64],
                            int16_t block[4][64], unsigned cbp,
                            int q, const uint16_t qmat[4][64])
{
    int idx, next, val, skip;
    VLC *skip_vlc;

    for (int i = 0; i < 4; i++)
        memset(block[i], 0, sizeof(int16_t) * 64);

    if ((cbp & 0xf) == 0)
        return 0;

    skip_vlc = &bink2g_ac_skip0_vlc;
    if (cbp & 0xffff0000)
        skip_vlc = &bink2g_ac_skip1_vlc;

    for (int i = 0; i < 4; i++, cbp >>= 1) {
        if (!(cbp & 1))
            continue;

        next = 0;
        idx  = 1;
        while (idx < 64) {
            next--;
            if (next < 1) {
                skip = get_vlc2(gb, skip_vlc->table, skip_vlc->bits, 1);
                if (skip < 0)
                    return AVERROR_INVALIDDATA;
                next = bink2_next_skips[skip];
                skip = bink2g_skips[skip];
                if (skip == 11)
                    skip = get_bits(gb, 6);
                idx += skip;
                if (idx >= 64)
                    break;
            }

            val = get_unary(gb, 0, 12) + 1;
            if (val > 3)
                val = get_bits(gb, val - 3) + (1 << (val - 3)) + 2;
            if (get_bits1(gb))
                val = -val;
            block[i][scan[idx]] = ((val * qmat[q & 3][scan[idx]] * (1 << (q >> 2))) + 64) >> 7;
            idx++;
        }
    }

    return 0;
}

static int bink2g_decode_intra_luma(Bink2Context *c,
                                    GetBitContext *gb, int16_t block[4][64],
                                    unsigned *prev_cbp, int q,
                                    BlockDSPContext *dsp, uint8_t *dst, int stride,
                                    int flags)
{
    int *dc = c->current_idc[c->mb_pos].dc[c->comp];
    unsigned cbp;
    int ret;

    *prev_cbp = cbp = bink2g_decode_cbp_luma(c, gb, *prev_cbp);

    bink2g_decode_dc(c, gb, dc, 1, q, 0, 2047, flags);

    for (int i = 0; i < 4; i++) {
        ret = bink2g_decode_ac(gb, bink2g_scan, block, cbp >> (4*i),
                               q, bink2g_luma_intra_qmat);
        if (ret < 0)
            return ret;

        for (int j = 0; j < 4; j++) {
            block[j][0] = dc[i * 4 + j] * 8 + 32;
            bink2g_idct_put(dst + (luma_repos[i * 4 + j] & 3) * 8 +
                            (luma_repos[i * 4 + j] >> 2) * 8 * stride, stride, block[j]);
        }
    }

    return 0;
}

static int bink2g_decode_intra_chroma(Bink2Context *c,
                                      GetBitContext *gb, int16_t block[4][64],
                                      unsigned *prev_cbp, int q,
                                      BlockDSPContext *dsp, uint8_t *dst, int stride,
                                      int flags)
{
    int *dc = c->current_idc[c->mb_pos].dc[c->comp];
    unsigned cbp;
    int ret;

    *prev_cbp = cbp = bink2g_decode_cbp_chroma(gb, *prev_cbp);

    bink2g_decode_dc(c, gb, dc, 0, q, 0, 2047, flags);

    ret = bink2g_decode_ac(gb, bink2g_scan, block, cbp,
                           q, bink2g_chroma_intra_qmat);
    if (ret < 0)
        return ret;

    for (int j = 0; j < 4; j++) {
        block[j][0] = dc[j] * 8 + 32;
        bink2g_idct_put(dst + (j & 1) * 8 +
                        (j >> 1) * 8 * stride, stride, block[j]);
    }

    return 0;
}

static int bink2g_decode_inter_luma(Bink2Context *c,
                                    GetBitContext *gb, int16_t block[4][64],
                                    unsigned *prev_cbp, int q,
                                    BlockDSPContext *dsp, uint8_t *dst, int stride,
                                    int flags)
{
    int *dc = c->current_idc[c->mb_pos].dc[c->comp];
    unsigned cbp;
    int ret;

    *prev_cbp = cbp = bink2g_decode_cbp_luma(c, gb, *prev_cbp);

    bink2g_decode_dc(c, gb, dc, 1, q, -1023, 1023, 0xA8);

    for (int i = 0; i < 4; i++) {
        ret = bink2g_decode_ac(gb, bink2g_scan, block, cbp >> (4 * i),
                               q, bink2g_inter_qmat);
        if (ret < 0)
            return ret;

        for (int j = 0; j < 4; j++) {
            block[j][0] = dc[i * 4 + j] * 8 + 32;
            bink2g_idct_add(dst + (luma_repos[i * 4 + j] & 3) * 8 +
                            (luma_repos[i * 4 + j] >> 2) * 8 * stride,
                            stride, block[j]);
        }
    }

    return 0;
}

static int bink2g_decode_inter_chroma(Bink2Context *c,
                                      GetBitContext *gb, int16_t block[4][64],
                                      unsigned *prev_cbp, int q,
                                      BlockDSPContext *dsp, uint8_t *dst, int stride,
                                      int flags)
{
    int *dc = c->current_idc[c->mb_pos].dc[c->comp];
    unsigned cbp;
    int ret;

    *prev_cbp = cbp = bink2g_decode_cbp_chroma(gb, *prev_cbp);

    bink2g_decode_dc(c, gb, dc, 0, q, -1023, 1023, 0xA8);

    ret = bink2g_decode_ac(gb, bink2g_scan, block, cbp,
                           q, bink2g_inter_qmat);
    if (ret < 0)
        return ret;

    for (int j = 0; j < 4; j++) {
        block[j][0] = dc[j] * 8 + 32;
        bink2g_idct_add(dst + (j & 1) * 8 +
                        (j >> 1) * 8 * stride, stride, block[j]);
    }

    return 0;
}

static void bink2g_predict_mv(Bink2Context *c, int x, int y, int flags, MVectors mv)
{
    MVectors *c_mv = &c->current_mv[c->mb_pos].mv;
    MVectors *l_mv = &c->current_mv[FFMAX(c->mb_pos - 1, 0)].mv;
    MVectors *lt_mv = &c->prev_mv[FFMAX(c->mb_pos - 1, 0)].mv;
    MVectors *t_mv = &c->prev_mv[c->mb_pos].mv;

    if (mv.nb_vectors == 1) {
        if (flags & 0x80) {
            if (!(flags & 0x20)) {
                mv.v[0][0] += mid_pred(l_mv->v[0][0], l_mv->v[1][0], l_mv->v[3][0]);
                mv.v[0][1] += mid_pred(l_mv->v[0][1], l_mv->v[1][1], l_mv->v[3][1]);
            }
        } else {
            if (!(flags & 0x20)) {
                mv.v[0][0] += mid_pred(lt_mv->v[3][0], t_mv->v[2][0], l_mv->v[1][0]);
                mv.v[0][1] += mid_pred(lt_mv->v[3][1], t_mv->v[2][1], l_mv->v[1][1]);
            } else {
                mv.v[0][0] += mid_pred(t_mv->v[0][0], t_mv->v[2][0], t_mv->v[3][0]);
                mv.v[0][1] += mid_pred(t_mv->v[0][1], t_mv->v[2][1], t_mv->v[3][1]);
            }
        }

        c_mv->v[0][0] = mv.v[0][0];
        c_mv->v[0][1] = mv.v[0][1];
        c_mv->v[1][0] = mv.v[0][0];
        c_mv->v[1][1] = mv.v[0][1];
        c_mv->v[2][0] = mv.v[0][0];
        c_mv->v[2][1] = mv.v[0][1];
        c_mv->v[3][0] = mv.v[0][0];
        c_mv->v[3][1] = mv.v[0][1];

        return;
    }

    if (!(flags & 0x80)) {
        if (flags & 0x20) {
            c_mv->v[0][0] = mv.v[0][0] + mid_pred(t_mv->v[0][0], t_mv->v[2][0], t_mv->v[3][0]);
            c_mv->v[0][1] = mv.v[0][1] + mid_pred(t_mv->v[0][1], t_mv->v[2][1], t_mv->v[3][1]);
            c_mv->v[1][0] = mv.v[1][0] + mid_pred(t_mv->v[2][0], t_mv->v[3][0], c_mv->v[0][0]);
            c_mv->v[1][1] = mv.v[1][1] + mid_pred(t_mv->v[2][1], t_mv->v[3][1], c_mv->v[0][1]);
            c_mv->v[2][0] = mv.v[2][0] + mid_pred(t_mv->v[2][0], c_mv->v[0][0], c_mv->v[1][0]);
            c_mv->v[2][1] = mv.v[2][1] + mid_pred(t_mv->v[2][1], c_mv->v[0][1], c_mv->v[1][1]);
            c_mv->v[3][0] = mv.v[3][0] + mid_pred(c_mv->v[0][0], c_mv->v[1][0], c_mv->v[2][0]);
            c_mv->v[3][1] = mv.v[3][1] + mid_pred(c_mv->v[0][1], c_mv->v[1][1], c_mv->v[2][1]);
        } else {
            c_mv->v[0][0] = mv.v[0][0] + mid_pred(t_mv->v[2][0], lt_mv->v[3][0], l_mv->v[1][0]);
            c_mv->v[0][1] = mv.v[0][1] + mid_pred(t_mv->v[2][1], lt_mv->v[3][1], l_mv->v[1][1]);
            c_mv->v[1][0] = mv.v[1][0] + mid_pred(t_mv->v[2][0], t_mv->v[3][0],  c_mv->v[0][0]);
            c_mv->v[1][1] = mv.v[1][1] + mid_pred(t_mv->v[2][1], t_mv->v[3][1],  c_mv->v[0][1]);
            c_mv->v[2][0] = mv.v[2][0] + mid_pred(l_mv->v[1][0], l_mv->v[3][0],  c_mv->v[0][0]);
            c_mv->v[2][1] = mv.v[2][1] + mid_pred(l_mv->v[1][1], l_mv->v[3][1],  c_mv->v[0][1]);
            c_mv->v[3][0] = mv.v[3][0] + mid_pred(c_mv->v[0][0], c_mv->v[1][0],  c_mv->v[2][0]);
            c_mv->v[3][1] = mv.v[3][1] + mid_pred(c_mv->v[0][1], c_mv->v[1][1],  c_mv->v[2][1]);
        }
    } else {
        if (flags & 0x20) {
            c_mv->v[0][0] = mv.v[0][0];
            c_mv->v[0][1] = mv.v[0][1];
            c_mv->v[1][0] = mv.v[1][0] + mv.v[0][0];
            c_mv->v[1][1] = mv.v[1][1] + mv.v[0][1];
            c_mv->v[2][0] = mv.v[2][0] + mv.v[0][0];
            c_mv->v[2][1] = mv.v[2][1] + mv.v[0][1];
            c_mv->v[3][0] = mv.v[3][0] + mid_pred(c_mv->v[0][0], c_mv->v[1][0], c_mv->v[2][0]);
            c_mv->v[3][1] = mv.v[3][1] + mid_pred(c_mv->v[0][1], c_mv->v[1][1], c_mv->v[2][1]);
        } else {
            c_mv->v[0][0] = mv.v[0][0] + mid_pred(l_mv->v[0][0], l_mv->v[1][0], l_mv->v[3][0]);
            c_mv->v[0][1] = mv.v[0][1] + mid_pred(l_mv->v[0][1], l_mv->v[1][1], l_mv->v[3][1]);
            c_mv->v[2][0] = mv.v[2][0] + mid_pred(l_mv->v[1][0], l_mv->v[3][0], c_mv->v[0][0]);
            c_mv->v[2][1] = mv.v[2][1] + mid_pred(l_mv->v[1][1], l_mv->v[3][1], c_mv->v[0][1]);
            c_mv->v[1][0] = mv.v[1][0] + mid_pred(l_mv->v[1][0], c_mv->v[0][0], c_mv->v[2][0]);
            c_mv->v[1][1] = mv.v[1][1] + mid_pred(l_mv->v[1][1], c_mv->v[0][1], c_mv->v[2][1]);
            c_mv->v[3][0] = mv.v[3][0] + mid_pred(c_mv->v[0][0], c_mv->v[1][0], c_mv->v[2][0]);
            c_mv->v[3][1] = mv.v[3][1] + mid_pred(c_mv->v[0][1], c_mv->v[1][1], c_mv->v[2][1]);
        }
    }
}

static int bink2g_decode_mv(Bink2Context *c, GetBitContext *gb, int x, int y,
                            MVectors *mv)
{
    int num_mvs = get_bits1(gb) ? 1 : 4;

    mv->nb_vectors = num_mvs;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < num_mvs; j++) {
            int val = get_vlc2(gb, bink2g_mv_vlc.table, bink2g_mv_vlc.bits, 1);

            if (val < 0)
                return AVERROR_INVALIDDATA;
            if (val >= 8 && val != 15)
                val = val - 15;
            if (val == 15) {
                int bits = get_unary(gb, 1, 12) + 4;
                val = get_bits(gb, bits) + (1 << bits) - 1;
                if (val & 1)
                    val = (-(val >> 1) - 1);
                else
                    val =    val >> 1;
            }
            mv->v[j][i] = val;
        }
    }

    return 0;
}

static void update_intra_q(Bink2Context *c, int8_t *intra_q, int dq, int flags)
{
    if (flags & 0x20 && flags & 0x80)
        *intra_q = 16 + dq;
    else if (flags & 0x80)
        *intra_q = c->current_q[c->mb_pos - 1].intra_q + dq;
    else if (flags & 0x20)
        *intra_q = c->prev_q[c->mb_pos].intra_q + dq;
    else
        *intra_q = mid_pred(c->prev_q[c->mb_pos].intra_q,
                            c->current_q[c->mb_pos - 1].intra_q,
                            c->prev_q[c->mb_pos - 1].intra_q) + dq;
}

static void update_inter_q(Bink2Context *c, int8_t *inter_q, int dq, int flags)
{
    if (flags & 0x20 && flags & 0x80)
        *inter_q = 16 + dq;
    else if (flags & 0x80)
        *inter_q = c->current_q[c->mb_pos - 1].inter_q + dq;
    else if (flags & 0x20)
        *inter_q = c->prev_q[c->mb_pos].inter_q + dq;
    else
        *inter_q = mid_pred(c->prev_q[c->mb_pos].inter_q,
                            c->current_q[c->mb_pos - 1].inter_q,
                            c->prev_q[c->mb_pos - 1].inter_q) + dq;
}

#define CH1FILTER(src)    ((6*(src)[0] + 2*(src)[1] + 4) >> 3)
#define CH2FILTER(src)    ((  (src)[0] +   (src)[1] + 1) >> 1)
#define CH3FILTER(src)    ((2*(src)[0] + 6*(src)[1] + 4) >> 3)

#define CV1FILTER(src, i)    ((6*(src)[0] + 2*(src)[i] + 4) >> 3)
#define CV2FILTER(src, i)    ((  (src)[0] +   (src)[i] + 1) >> 1)
#define CV3FILTER(src, i)    ((2*(src)[0] + 6*(src)[i] + 4) >> 3)

static void bink2g_c_mc(Bink2Context *c, int x, int y,
                        uint8_t *dst, int stride,
                        uint8_t *src, int sstride,
                        int width, int height,
                        int mv_x, int mv_y,
                        int mode)
{
    uint8_t *msrc;
    uint8_t temp[8*9];

    if (mv_x < 0 || mv_x >= width ||
        mv_y < 0 || mv_y >= height)
        return;

    msrc = src + mv_x + mv_y * sstride;

    switch (mode) {
    case 0:
        copy_block8(dst, msrc, stride, sstride, 8);
        break;
    case 1:
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CH1FILTER(msrc + i));
            dst  += stride;
            msrc += sstride;
        }
        break;
    case 2:
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CH2FILTER(msrc + i));
            dst  += stride;
            msrc += sstride;
        }
        break;
    case 3:
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CH3FILTER(msrc + i));
            dst  += stride;
            msrc += sstride;
        }
        break;
    case 4:
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i*stride] = av_clip_uint8(CV1FILTER(msrc + i*sstride, sstride));
            dst  += 1;
            msrc += 1;
        }
        break;
    case 5:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH1FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV1FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 6:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH2FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV1FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 7:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH3FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV1FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 8:
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i*stride] = av_clip_uint8(CV2FILTER(msrc + i*sstride, sstride));
            dst  += 1;
            msrc += 1;
        }
        break;
    case 9:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH1FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV2FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 10:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH2FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV2FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 11:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH3FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV2FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 12:
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i*stride] = av_clip_uint8(CV3FILTER(msrc + i*sstride, sstride));
            dst  += 1;
            msrc += 1;
        }
        break;
    case 13:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH1FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV3FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 14:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH2FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV3FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    case 15:
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++)
                temp[i*8+j] = av_clip_uint8(CH3FILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++)
                dst[i] = av_clip_uint8(CV3FILTER(temp+j*8+i, 8));
            dst  += stride;
        }
        break;
    }
}

static int bink2g_mcompensate_chroma(Bink2Context *c, int x, int y,
                                     uint8_t *dst, int stride,
                                     uint8_t *src, int sstride,
                                     int width, int height)
{
    MVectors *mv = &c->current_mv[c->mb_pos].mv;
    int mv_x, mv_y, mode;

    mv_x  = (mv->v[0][0] >> 2) + x;
    mv_y  = (mv->v[0][1] >> 2) + y;
    mode  =  mv->v[0][0] & 3;
    mode |= (mv->v[0][1] & 3) << 2;
    bink2g_c_mc(c, x, y, dst + x, stride, src, sstride, width, height, mv_x, mv_y, mode);

    mv_x = (mv->v[1][0] >> 2) + x + 8;
    mv_y = (mv->v[1][1] >> 2) + y;
    mode  =  mv->v[1][0] & 3;
    mode |= (mv->v[1][1] & 3) << 2;
    bink2g_c_mc(c, x, y, dst + x + 8, stride, src, sstride, width, height, mv_x, mv_y, mode);

    mv_x = (mv->v[2][0] >> 2) + x;
    mv_y = (mv->v[2][1] >> 2) + y + 8;
    mode  =  mv->v[2][0] & 3;
    mode |= (mv->v[2][1] & 3) << 2;
    bink2g_c_mc(c, x, y, dst + x + 8 * stride, stride, src, sstride, width, height, mv_x, mv_y, mode);

    mv_x = (mv->v[3][0] >> 2) + x + 8;
    mv_y = (mv->v[3][1] >> 2) + y + 8;
    mode  =  mv->v[3][0] & 3;
    mode |= (mv->v[3][1] & 3) << 2;
    bink2g_c_mc(c, x, y, dst + x + 8 + 8 * stride, stride, src, sstride, width, height, mv_x, mv_y, mode);

    return 0;
}

#define LHFILTER(src)    ((((src)[0]+(src)[1])*19 >> 1)-((src)[-1]+(src)[2  ])*2+(((src)[-2  ]+(src)[3  ])>>1)+8>>4)
#define LVFILTER(src, i) ((((src)[0]+(src)[i])*19 >> 1)-((src)[-i]+(src)[2*i])*2+(((src)[-2*i]+(src)[3*i])>>1)+8>>4)

static void bink2g_y_mc(Bink2Context *c, int x, int y,
                        uint8_t *dst, int stride,
                        uint8_t *src, int sstride,
                        int width, int height,
                        int mv_x, int mv_y, int mode)
{
    uint8_t *msrc;

    if (mv_x < 0 || mv_x >= width ||
        mv_y < 0 || mv_y >= height)
        return;

    msrc = src + mv_x + mv_y * sstride;

    if (mode == 0) {
        copy_block16(dst, msrc, stride, sstride, 16);
    } else if (mode == 1) {
        for (int j = 0; j < 16; j++) {
            for (int i = 0; i < 16; i++)
                dst[i] = av_clip_uint8(LHFILTER(msrc + i));
            dst  += stride;
            msrc += sstride;
        }
    } else if (mode == 2) {
        for (int j = 0; j < 16; j++) {
            for (int i = 0; i < 16; i++)
                dst[i*stride] = av_clip_uint8(LVFILTER(msrc + i*sstride, sstride));
            dst  += 1;
            msrc += 1;
        }
    } else if (mode == 3) {
        uint8_t temp[21 * 16];

        msrc -= 2 * sstride;
        for (int i = 0; i < 21; i++) {
            for (int j = 0; j < 16; j++)
                temp[i*16+j] = av_clip_uint8(LHFILTER(msrc + j));
            msrc += sstride;
        }
        for (int j = 0; j < 16; j++) {
            for (int i = 0; i < 16; i++)
                dst[i] = av_clip_uint8(LVFILTER(temp+(j+2)*16+i, 16));
            dst  += stride;
        }
    }
}

static int bink2g_mcompensate_luma(Bink2Context *c, int x, int y,
                                   uint8_t *dst, int stride,
                                   uint8_t *src, int sstride,
                                   int width, int height)
{
    MVectors *mv = &c->current_mv[c->mb_pos].mv;
    int mv_x, mv_y, mode;

    mv_x  = (mv->v[0][0] >> 1) + x;
    mv_y  = (mv->v[0][1] >> 1) + y;
    mode  =  mv->v[0][0] & 1;
    mode |= (mv->v[0][1] & 1) << 1;
    bink2g_y_mc(c, x, y, dst + x, stride, src, sstride, width, height, mv_x, mv_y, mode);

    mv_x  = (mv->v[1][0] >> 1) + x + 16;
    mv_y  = (mv->v[1][1] >> 1) + y;
    mode  =  mv->v[1][0] & 1;
    mode |= (mv->v[1][1] & 1) << 1;
    bink2g_y_mc(c, x, y, dst + x + 16, stride, src, sstride, width, height, mv_x, mv_y, mode);

    mv_x  = (mv->v[2][0] >> 1) + x;
    mv_y  = (mv->v[2][1] >> 1) + y + 16;
    mode  =  mv->v[2][0] & 1;
    mode |= (mv->v[2][1] & 1) << 1;
    bink2g_y_mc(c, x, y, dst + x + 16 * stride, stride, src, sstride, width, height, mv_x, mv_y, mode);

    mv_x  = (mv->v[3][0] >> 1) + x + 16;
    mv_y  = (mv->v[3][1] >> 1) + y + 16;
    mode  =  mv->v[3][0] & 1;
    mode |= (mv->v[3][1] & 1) << 1;
    bink2g_y_mc(c, x, y, dst + x + 16 + 16 * stride, stride, src, sstride, width, height, mv_x, mv_y, mode);

    return 0;
}

static int bink2g_average_block(uint8_t *src, int stride)
{
    int sum = 0;

    for (int i = 0; i < 8; i++) {
        int avg_a = (src[i+0*stride] + src[i+1*stride] + 1) >> 1;
        int avg_b = (src[i+2*stride] + src[i+3*stride] + 1) >> 1;
        int avg_c = (src[i+4*stride] + src[i+5*stride] + 1) >> 1;
        int avg_d = (src[i+6*stride] + src[i+7*stride] + 1) >> 1;
        int avg_e = (avg_a + avg_b + 1) >> 1;
        int avg_f = (avg_c + avg_d + 1) >> 1;
        int avg_g = (avg_e + avg_f + 1) >> 1;
        sum += avg_g;
    }

    return sum;
}

static void bink2g_average_chroma(Bink2Context *c, int x, int y,
                                  uint8_t *src, int stride,
                                  int *dc)
{
    for (int i = 0; i < 4; i++) {
        int X = i & 1;
        int Y = i >> 1;
        dc[i] = bink2g_average_block(src + x + X * 8 + (y + Y * 8) * stride, stride);
    }
}

static void bink2g_average_luma(Bink2Context *c, int x, int y,
                                uint8_t *src, int stride,
                                int *dc)
{
    for (int i = 0; i < 16; i++) {
        int I = luma_repos[i];
        int X = I & 3;
        int Y = I >> 2;
        dc[i] = bink2g_average_block(src + x + X * 8 + (y + Y * 8) * stride, stride);
    }
}

static int bink2g_decode_slice(Bink2Context *c,
                               uint8_t *dst[4], int stride[4],
                               uint8_t *src[4], int sstride[4],
                               int is_kf, int start, int end)
{
    GetBitContext *gb = &c->gb;
    int w = c->avctx->width;
    int h = c->avctx->height;
    int ret = 0, dq, flags;

    memset(c->prev_q, 0, ((c->avctx->width + 31) / 32) * sizeof(*c->prev_q));
    memset(c->prev_mv, 0, ((c->avctx->width + 31) / 32) * sizeof(*c->prev_mv));

    for (int y = start; y < end; y += 32) {
        int types_lru[4] = { MOTION_BLOCK, RESIDUE_BLOCK, SKIP_BLOCK, INTRA_BLOCK };
        unsigned y_cbp_intra = 0, u_cbp_intra = 0, v_cbp_intra = 0, a_cbp_intra = 0;
        unsigned y_cbp_inter = 0, u_cbp_inter = 0, v_cbp_inter = 0, a_cbp_inter = 0;

        memset(c->current_q, 0, ((c->avctx->width + 31) / 32) * sizeof(*c->current_q));
        memset(c->current_mv, 0, ((c->avctx->width + 31) / 32) * sizeof(*c->current_mv));

        for (int x = 0; x < c->avctx->width; x += 32) {
            int type = is_kf ? INTRA_BLOCK : bink2g_get_type(gb, types_lru);
            int8_t *intra_q = &c->current_q[x / 32].intra_q;
            int8_t *inter_q = &c->current_q[x / 32].inter_q;
            MVectors mv = { 0 };

            c->mb_pos = x / 32;
            c->current_idc[c->mb_pos].block_type = type;
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
                if (!(flags & 0xA0) && c->prev_idc[c->mb_pos - 1].block_type != INTRA_BLOCK) {
                    bink2g_average_luma  (c, x  -32, -32, dst[0], stride[0], c->prev_idc[c->mb_pos - 1].dc[0]);
                    bink2g_average_chroma(c, x/2-16, -16, dst[2], stride[2], c->prev_idc[c->mb_pos - 1].dc[1]);
                    bink2g_average_chroma(c, x/2-16, -16, dst[1], stride[1], c->prev_idc[c->mb_pos - 1].dc[2]);
                    if (c->has_alpha)
                        bink2g_average_luma(c, x-32, -32, dst[3], stride[3], c->prev_idc[c->mb_pos - 1].dc[3]);
                }
                if (!(flags & 0x20) && c->current_idc[c->mb_pos - 1].block_type != INTRA_BLOCK) {
                    bink2g_average_luma  (c, x  -32, 0, dst[0], stride[0], c->current_idc[c->mb_pos - 1].dc[0]);
                    bink2g_average_chroma(c, x/2-16, 0, dst[2], stride[2], c->current_idc[c->mb_pos - 1].dc[1]);
                    bink2g_average_chroma(c, x/2-16, 0, dst[1], stride[1], c->current_idc[c->mb_pos - 1].dc[2]);
                    if (c->has_alpha)
                        bink2g_average_luma(c, x-32, 0, dst[3], stride[3], c->current_idc[c->mb_pos - 1].dc[3]);
                }
                if ((flags & 0x20) && !(flags & 0x80) && c->prev_idc[c->mb_pos + 1].block_type != INTRA_BLOCK) {
                    bink2g_average_luma  (c, x  +32, -32, dst[0], stride[0], c->prev_idc[c->mb_pos + 1].dc[0]);
                    bink2g_average_chroma(c, x/2+16, -16, dst[2], stride[2], c->prev_idc[c->mb_pos + 1].dc[1]);
                    bink2g_average_chroma(c, x/2+16, -16, dst[1], stride[1], c->prev_idc[c->mb_pos + 1].dc[2]);
                    if (c->has_alpha)
                        bink2g_average_luma(c, x+32, -32, dst[3], stride[3], c->prev_idc[c->mb_pos + 1].dc[3]);
                }
                if (!(flags & 0x80) && c->prev_idc[c->mb_pos].block_type != INTRA_BLOCK) {
                    bink2g_average_luma  (c, x,   -32, dst[0], stride[0], c->prev_idc[c->mb_pos].dc[0]);
                    bink2g_average_chroma(c, x/2, -16, dst[2], stride[2], c->prev_idc[c->mb_pos].dc[1]);
                    bink2g_average_chroma(c, x/2, -16, dst[1], stride[1], c->prev_idc[c->mb_pos].dc[2]);
                    if (c->has_alpha)
                        bink2g_average_luma(c, x, -32, dst[3], stride[3], c->prev_idc[c->mb_pos].dc[3]);
                }

                bink2g_predict_mv(c, x, y, flags, mv);
                update_inter_q(c, inter_q, 0, flags);
                dq = bink2g_decode_dq(gb);
                update_intra_q(c, intra_q, dq, flags);
                if (*intra_q < 0 || *intra_q >= 37) {
                    ret = AVERROR_INVALIDDATA;
                    goto fail;
                }
                c->comp = 0;
                ret = bink2g_decode_intra_luma(c, gb, c->iblock, &y_cbp_intra, *intra_q, &c->dsp,
                                               dst[0] + x, stride[0], flags);
                if (ret < 0)
                    goto fail;
                c->comp = 1;
                ret = bink2g_decode_intra_chroma(c, gb, c->iblock, &u_cbp_intra, *intra_q, &c->dsp,
                                                 dst[2] + x/2, stride[2], flags);
                if (ret < 0)
                    goto fail;
                c->comp = 2;
                ret = bink2g_decode_intra_chroma(c, gb, c->iblock, &v_cbp_intra, *intra_q, &c->dsp,
                                                 dst[1] + x/2, stride[1], flags);
                if (ret < 0)
                    goto fail;
                if (c->has_alpha) {
                    c->comp = 3;
                    ret = bink2g_decode_intra_luma(c, gb, c->iblock, &a_cbp_intra, *intra_q, &c->dsp,
                                                   dst[3] + x, stride[3], flags);
                    if (ret < 0)
                        goto fail;
                }
                break;
            case SKIP_BLOCK:
                update_inter_q(c, inter_q, 0, flags);
                update_intra_q(c, intra_q, 0, flags);
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
                break;
            case MOTION_BLOCK:
                update_intra_q(c, intra_q, 0, flags);
                update_inter_q(c, inter_q, 0, flags);
                ret = bink2g_decode_mv(c, gb, x, y, &mv);
                if (ret < 0)
                    goto fail;
                bink2g_predict_mv(c, x, y, flags, mv);
                c->comp = 0;
                ret = bink2g_mcompensate_luma(c, x, y,
                                              dst[0], stride[0],
                                              src[0], sstride[0],
                                              w, h);
                if (ret < 0)
                    goto fail;
                c->comp = 1;
                ret = bink2g_mcompensate_chroma(c, x/2, y/2,
                                                dst[2], stride[2],
                                                src[2], sstride[2],
                                                w/2, h/2);
                if (ret < 0)
                    goto fail;
                c->comp = 2;
                ret = bink2g_mcompensate_chroma(c, x/2, y/2,
                                                dst[1], stride[1],
                                                src[1], sstride[1],
                                                w/2, h/2);
                if (ret < 0)
                    goto fail;
                if (c->has_alpha) {
                    c->comp = 3;
                    ret = bink2g_mcompensate_luma(c, x, y,
                                                  dst[3], stride[3],
                                                  src[3], sstride[3],
                                                  w, h);
                    if (ret < 0)
                        goto fail;
                }
                break;
            case RESIDUE_BLOCK:
                update_intra_q(c, intra_q, 0, flags);
                ret = bink2g_decode_mv(c, gb, x, y, &mv);
                if (ret < 0)
                    goto fail;
                bink2g_predict_mv(c, x, y, flags, mv);
                dq = bink2g_decode_dq(gb);
                update_inter_q(c, inter_q, dq, flags);
                if (*inter_q < 0 || *inter_q >= 37) {
                    ret = AVERROR_INVALIDDATA;
                    goto fail;
                }
                c->comp = 0;
                ret = bink2g_mcompensate_luma(c, x, y,
                                              dst[0], stride[0],
                                              src[0], sstride[0],
                                              w, h);
                if (ret < 0)
                    goto fail;
                c->comp = 1;
                ret = bink2g_mcompensate_chroma(c, x/2, y/2,
                                                dst[2], stride[2],
                                                src[2], sstride[2],
                                                w/2, h/2);
                if (ret < 0)
                    goto fail;
                c->comp = 2;
                ret = bink2g_mcompensate_chroma(c, x/2, y/2,
                                                dst[1], stride[1],
                                                src[1], sstride[1],
                                                w/2, h/2);
                if (ret < 0)
                    goto fail;
                if (c->has_alpha) {
                    c->comp = 3;
                    ret = bink2g_mcompensate_luma(c, x, y,
                                                  dst[3], stride[3],
                                                  src[3], sstride[3],
                                                  w, h);
                    if (ret < 0)
                        goto fail;
                }
                c->comp = 0;
                ret = bink2g_decode_inter_luma(c, gb, c->iblock, &y_cbp_inter, *inter_q, &c->dsp,
                                               dst[0] + x, stride[0], flags);
                if (ret < 0)
                    goto fail;
                if (get_bits1(gb)) {
                    c->comp = 1;
                    ret = bink2g_decode_inter_chroma(c, gb, c->iblock, &u_cbp_inter, *inter_q, &c->dsp,
                                                     dst[2] + x/2, stride[2], flags);
                    if (ret < 0)
                        goto fail;
                    c->comp = 2;
                    ret = bink2g_decode_inter_chroma(c, gb, c->iblock, &v_cbp_inter, *inter_q, &c->dsp,
                                                     dst[1] + x/2, stride[1], flags);
                    if (ret < 0)
                        goto fail;
                } else {
                    u_cbp_inter = 0;
                    v_cbp_inter = 0;
                }
                if (c->has_alpha) {
                    c->comp = 3;
                    ret = bink2g_decode_inter_luma(c, gb, c->iblock, &a_cbp_inter, *inter_q, &c->dsp,
                                                   dst[3] + x, stride[3], flags);
                    if (ret < 0)
                        goto fail;
                }
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
        FFSWAP(QuantPredict *, c->current_q, c->prev_q);
        FFSWAP(DCIPredict *, c->current_idc, c->prev_idc);
    }
fail:
    emms_c();

    return ret;
}
