/* WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit
 * given), https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/launcher/movie_maker.c at b2521fe.
 * WWE '12 titantron movies have the same layout (Bink 1 "BIKi", 320 x 320, 30 fps). */
/* WWE SmackDown vs. Raw 2011 - PC port launcher: USER MOVIES maker.
 *
 * Makes an entrance movie the game plays like its own titantron movies
 * (movies\titantron\<id>.bik): a Bink 1 ("BIKi") file, 320 x 320, 30 fps,
 * no audio. The frame is two pictures: the top 320 x 220 is the big screen
 * (shown at 16:9, so it is stored squeezed), the bottom 320 x 100 is the
 * strip for the stage and ramp screens (black if there is none).
 *
 * The Bink bitstream uses three of Bink's block types: raw 8 x 8 blocks,
 * fill blocks (one colour) and skip blocks (unchanged since the last frame),
 * with fixed 4-bit codes (tree 0) - see ffmpeg's libavcodec/bink.c for the
 * format. Pictures come from Media Foundation (videos) or WIC (images). */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "bink_decode.h"
#include "bink_encode.h"
#include "movie_maker.h"

#define FPS 30
#define TOP_H 220
#define BOT_H (MOVIE_H - TOP_H)
/* The same layout at a movie size of W x W (320 / 640 / 1024): the top
 * W x top_h(W) is the big screen, the rest the stage strip. */
static int top_h(int W) { return W * TOP_H / MOVIE_W; }

/* ── pictures ── */

typedef struct {
    int kind;                  /* 0 black, 1 image, 2 video, 3 Bink movie (a part of it) */
    int w, h;                  /* picture size */
    double par;                /* pixel aspect ratio */
    uint8_t *bgra;             /* current picture */
    IMFSourceReader *rd;
    LONG stride;
    int yuv;                   /* the decoder's own format when Windows can't give RGB: 0 RGB32, 1 NV12, 2 YUY2, 3 I420 */
    int fw, fh;                /* decoded frame layout */
    int ax, ay;                /* visible area offset */
    LONGLONG duration, base;   /* 100 ns; base = time offset of the current loop */
    int have_next, eof;
    LONGLONG next_ts;
    IMFSample *next;
    int cx, cy, cw, ch;        /* the picture without black borders (content_find) */
    BinkReader *bink;          /* kind 3: the movie, */
    int by;                    /* and the first row of the part used */
    uint8_t *bink_frame;       /* its whole decoded frame */
} Pic;

/* Which part of the entrance movie a picture is for. */
enum { PART_TOP = 0, PART_BOTTOM = 1 };

static int is_image(const WCHAR *path)
{
    static const WCHAR *ext[] = { L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".webp", L".jxr" };
    const WCHAR *dot = wcsrchr(path, L'.');
    size_t i;
    if (!dot) return 0;
    for (i = 0; i < sizeof ext / sizeof *ext; i++)
        if (!_wcsicmp(dot, ext[i])) return 1;
    return 0;
}

static int image_open(Pic *p, const WCHAR *path, WCHAR *err, size_t errn)
{
    IWICImagingFactory *fac = NULL;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *conv = NULL;
    UINT w = 0, h = 0;
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&fac))
            || FAILED(IWICImagingFactory_CreateDecoderFromFilename(fac, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))
            || FAILED(IWICBitmapDecoder_GetFrame(dec, 0, &frame))
            || FAILED(IWICImagingFactory_CreateFormatConverter(fac, &conv))
            || FAILED(IWICFormatConverter_Initialize(conv, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppBGRA,
                                                     WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))
            || FAILED(IWICFormatConverter_GetSize(conv, &w, &h)) || !w || !h || w > 16384 || h > 16384) {
        swprintf_s(err, errn, L"Could not read %s as an image.", path);
        goto done;
    }
    p->bgra = (uint8_t *)malloc((size_t)w * h * 4);
    if (!p->bgra || FAILED(IWICFormatConverter_CopyPixels(conv, NULL, w * 4, w * h * 4, p->bgra))) {
        swprintf_s(err, errn, L"Could not read %s.", path);
        goto done;
    }
    p->kind = 1; p->w = (int)w; p->h = (int)h; p->par = 1.0;
    ok = 1;
done:
    if (conv) IWICFormatConverter_Release(conv);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (dec) IWICBitmapDecoder_Release(dec);
    if (fac) IWICImagingFactory_Release(fac);
    return ok;
}

/* The video's frames as RGB32 through Windows' video processor - or, where
 * there is none (Wine / Proton on Linux and the Steam Deck: their decoders
 * give only their own YUV formats), as NV12 / YUY2 / I420, turned into RGB
 * here. SVR2011_MOVIE_FORCE_YUV=1 takes the second way on Windows (tests). */
static int video_reader(Pic *p, const WCHAR *path)
{
    static const GUID *const yuv[] = { &MFVideoFormat_NV12, &MFVideoFormat_YUY2, &MFVideoFormat_IYUV };
    WCHAR force[8];
    const int rgb_first = !GetEnvironmentVariableW(L"SVR2011_MOVIE_FORCE_YUV", force, 8);
    int pass, k;
    for (pass = rgb_first ? 0 : 1; pass < 2; pass++) {
        IMFAttributes *attr = NULL;
        if (FAILED(MFCreateAttributes(&attr, 2)))
            return 0;
        if (pass == 0)
            IMFAttributes_SetUINT32(attr, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        if (FAILED(MFCreateSourceReaderFromURL(path, attr, &p->rd))) {
            IMFAttributes_Release(attr);
            p->rd = NULL;
            continue;
        }
        IMFAttributes_Release(attr);
        IMFSourceReader_SetStreamSelection(p->rd, (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
        IMFSourceReader_SetStreamSelection(p->rd, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        for (k = pass == 0 ? -1 : 0; k < (pass == 0 ? 0 : 3); k++) {
            IMFMediaType *mt = NULL;
            HRESULT hr;
            if (FAILED(MFCreateMediaType(&mt)))
                break;
            IMFMediaType_SetGUID(mt, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
            IMFMediaType_SetGUID(mt, &MF_MT_SUBTYPE, k < 0 ? &MFVideoFormat_RGB32 : yuv[k]);
            hr = IMFSourceReader_SetCurrentMediaType(p->rd, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mt);
            IMFMediaType_Release(mt);
            if (SUCCEEDED(hr)) {
                p->yuv = k + 1;
                return 1;
            }
        }
        IMFSourceReader_Release(p->rd);
        p->rd = NULL;
    }
    return 0;
}

static int mf_start(void);   /* (below) */
static const WCHAR k_no_mf[] = L"Windows can't play videos here (Media Foundation didn't start). On a Windows \"N\" "
                               L"edition, install the Media Feature Pack from Windows' Optional features, then try again.";

static int video_open(Pic *p, const WCHAR *path, WCHAR *err, size_t errn)
{
    IMFMediaType *mt = NULL;
    PROPVARIANT pv;
    UINT32 w = 0, h = 0, pn = 1, pd = 1, stride = 0;
    UINT64 packed = 0;
    MFVideoArea area;
    int ok = 0;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        swprintf_s(err, errn, L"Can't find %s (moved, renamed, or on a drive that isn't connected?).", path);
        goto done;
    }
    if (!mf_start()) {
        wcscpy_s(err, errn, k_no_mf);
        goto done;
    }
    if (!video_reader(p, path)) {
        swprintf_s(err, errn, L"Could not open %s as a video: this PC (or Wine / Proton) has no decoder for it. "
                              L"Try an MP4 (H.264) video, or a .bik movie.", path);
        goto done;
    }
    if (FAILED(IMFSourceReader_GetCurrentMediaType(p->rd, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mt))
            || FAILED(IMFMediaType_GetUINT64(mt, &MF_MT_FRAME_SIZE, &packed))
            || !(w = (UINT32)(packed >> 32)) || !(h = (UINT32)packed)) {
        swprintf_s(err, errn, L"%s: unknown picture size.", path);
        goto done;
    }
    p->fw = (int)w; p->fh = (int)h;
    p->w = (int)w; p->h = (int)h; p->ax = p->ay = 0;
    if (SUCCEEDED(IMFMediaType_GetBlob(mt, &MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8 *)&area, sizeof area, NULL))
            && area.Area.cx > 0 && area.Area.cy > 0 && area.Area.cx <= (LONG)w && area.Area.cy <= (LONG)h) {
        p->ax = area.OffsetX.value; p->ay = area.OffsetY.value;
        p->w = area.Area.cx; p->h = area.Area.cy;
    }
    p->stride = (LONG)w * (p->yuv == 0 ? 4 : p->yuv == 2 ? 2 : 1);
    if (SUCCEEDED(IMFMediaType_GetUINT32(mt, &MF_MT_DEFAULT_STRIDE, &stride))) p->stride = (LONG)stride;
    p->par = 1.0;
    if (SUCCEEDED(IMFMediaType_GetUINT64(mt, &MF_MT_PIXEL_ASPECT_RATIO, &packed))) {
        pn = (UINT32)(packed >> 32); pd = (UINT32)packed;
        if (pn && pd) p->par = (double)pn / pd;
    }
    PropVariantInit(&pv);
    p->duration = 0;
    if (SUCCEEDED(IMFSourceReader_GetPresentationAttribute(p->rd, (DWORD)MF_SOURCE_READER_MEDIASOURCE, &MF_PD_DURATION, &pv)))
        p->duration = (LONGLONG)pv.uhVal.QuadPart;
    PropVariantClear(&pv);
    p->bgra = (uint8_t *)calloc((size_t)p->w * p->h, 4);
    if (!p->bgra) goto done;
    p->kind = 2;
    ok = 1;
done:
    if (mt) IMFMediaType_Release(mt);
    return ok;
}

static int is_bink(const WCHAR *path)
{
    const WCHAR *dot = wcsrchr(path, L'.');
    return dot && !_wcsicmp(dot, L".bik");
}

/* A Bink movie laid out like the game's titantron movies: as the big screen,
 * its top 320 x 220 (a 16:9 picture); as the strip, its bottom 320 x 100 -
 * a superstar's movie, or one made before. */
static int bink_pic_open(Pic *p, const WCHAR *path, int part, WCHAR *err, size_t errn)
{
    int bw, bh;
    p->bink = bink_open(path, err, errn);
    if (!p->bink) return 0;
    bw = bink_width(p->bink);
    bh = bink_height(p->bink);
    p->bink_frame = (uint8_t *)malloc((size_t)bw * bh * 4);
    p->w = bw;
    p->h = bh;
    p->par = 1.0;
    p->by = 0;
    if (bw == bh && (bw == 320 || bw == 640 || bw == 1024)) {  /* a titantron movie at any of the sizes: its two parts */
        p->by = part == PART_BOTTOM ? top_h(bw) : 0;
        p->h = part == PART_BOTTOM ? bw - top_h(bw) : top_h(bw);
        if (part == PART_TOP) p->par = (16.0 / 9.0) * top_h(bw) / bw;
    }
    p->bgra = (uint8_t *)calloc((size_t)p->w * p->h, 4);
    if (!p->bink_frame || !p->bgra) { swprintf_s(err, errn, L"Out of memory."); return 0; }
    p->duration = bink_frame_time(p->bink) * bink_frames(p->bink);
    p->kind = 3;
    return 1;
}

/* Decodes the Bink movie's next frame into the picture. */
static int bink_pic_next(Pic *p)
{
    if (!bink_next(p->bink)) return 0;
    bink_bgra(p->bink, p->bink_frame);
    memcpy(p->bgra, p->bink_frame + (size_t)p->by * p->w * 4, (size_t)p->w * p->h * 4);
    return 1;
}

static void pic_close(Pic *p)
{
    if (p->next) IMFSample_Release(p->next);
    if (p->rd) IMFSourceReader_Release(p->rd);
    if (p->bink) bink_close(p->bink);
    free(p->bink_frame);
    free(p->bgra);
    memset(p, 0, sizeof *p);
}

static void content_find(Pic *p);
static int bink_pic_next(Pic *p);

static int pic_open(Pic *p, const WCHAR *path, int part, WCHAR *err, size_t errn)
{
    int ok;
    memset(p, 0, sizeof *p);
    if (!path || !path[0]) return 1;            /* black */
    ok = is_bink(path) ? bink_pic_open(p, path, part, err, errn)
       : is_image(path) ? image_open(p, path, err, errn) : video_open(p, path, err, errn);
    if (!ok) { pic_close(p); return 0; }
    /* A superstar's strip is used as it is; anything else loses its black bars. */
    if (p->kind == 3 && part == PART_BOTTOM) { p->cx = 0; p->cy = 0; p->cw = p->w; p->ch = p->h; }
    else content_find(p);
    return 1;
}

/* A decoded frame into the picture. The decoder's rows can be padded (a
 * width that isn't a multiple of 16 / 64: 854, 1366, 1918...) - their real
 * pitch comes from the 2D buffer; the media type's stride is only the
 * unpadded width (copying with it shears the picture into stripes). */
static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* One YUV pixel (limited range; BT.709 for HD pictures, BT.601 below) to BGRA. */
static void yuv_px(uint8_t *o, int y, int u, int v, int hd)
{
    const int c = (y - 16) * 298, d = u - 128, e = v - 128;
    if (hd) {
        o[2] = clamp8((c + 459 * e + 128) >> 8);
        o[1] = clamp8((c - 55 * d - 136 * e + 128) >> 8);
        o[0] = clamp8((c + 541 * d + 128) >> 8);
    } else {
        o[2] = clamp8((c + 409 * e + 128) >> 8);
        o[1] = clamp8((c - 100 * d - 208 * e + 128) >> 8);
        o[0] = clamp8((c + 516 * d + 128) >> 8);
    }
    o[3] = 255;
}

/* A YUV frame in one block (`len` bytes) into the picture. Its luma pitch
 * and padded height: the first 16..512-pixel rounding of the width that the
 * block fits (as for RGB below). */
static void yuv_copy(Pic *p, const BYTE *data, DWORD len, LONG known_pitch)
{
    const int hd = p->fh >= 720;
    DWORD pitch = 0, rows = 0;
    int align, x, y;
    if (known_pitch > 0) {
        /* (from the 2D buffer: its luma plane is the height rounded up to 16
           rows - 1080 -> 1088 - and the chroma follows it; `len` is a packed
           copy's length, not this layout's) */
        pitch = (DWORD)known_pitch;
        rows = (DWORD)((p->fh + 15) & ~15);
        (void)len;
    }
    for (align = 1; align <= 512 && !pitch; align = align == 1 ? 16 : align * 2) {
        const DWORD w = (DWORD)((p->fw + align - 1) / align * align) * (p->yuv == 2 ? 2 : 1);
        const DWORD per = p->yuv == 2 ? w : w * 3 / 2;   /* bytes per luma row, chroma included */
        if (per && len % per == 0 && len / per >= (DWORD)p->fh) {
            pitch = w;
            rows = len / per;
        }
    }
    if (!pitch) return;
    for (y = 0; y < p->h; y++) {
        const int sy = y + p->ay;
        uint8_t *o = p->bgra + (size_t)y * p->w * 4;
        for (x = 0; x < p->w; x++, o += 4) {
            const int sx = x + p->ax;
            int Y, U, V;
            if (p->yuv == 2) {   /* YUY2: Y0 U Y1 V */
                const BYTE *q = data + (size_t)sy * pitch + (size_t)(sx & ~1) * 2;
                Y = q[(sx & 1) * 2]; U = q[1]; V = q[3];
            } else if (p->yuv == 1) {   /* NV12: Y plane, then interleaved U V at half size */
                const BYTE *c = data + (size_t)pitch * rows + (size_t)(sy / 2) * pitch + (size_t)(sx & ~1);
                Y = data[(size_t)sy * pitch + sx]; U = c[0]; V = c[1];
            } else {   /* I420: Y, then U and V planes at half size */
                const BYTE *u = data + (size_t)pitch * rows, *v = u + (size_t)(pitch / 2) * (rows / 2);
                Y = data[(size_t)sy * pitch + sx];
                U = u[(size_t)(sy / 2) * (pitch / 2) + sx / 2]; V = v[(size_t)(sy / 2) * (pitch / 2) + sx / 2];
            }
            yuv_px(o, Y, U, V, hd);
        }
    }
}

static void sample_copy(Pic *p, IMFSample *s)
{
    IMFMediaBuffer *buf = NULL;
    IMF2DBuffer *b2 = NULL;
    BYTE *data = NULL;
    DWORD len = 0;
    int y;
    DWORD count = 0;
    if (p->yuv) {
        if (SUCCEEDED(IMFSample_GetBufferCount(s, &count)) && count == 1 && SUCCEEDED(IMFSample_GetBufferByIndex(s, 0, &buf))
                && SUCCEEDED(IMFMediaBuffer_QueryInterface(buf, &IID_IMF2DBuffer, (void **)&b2))) {
            LONG pitch = 0;
            DWORD clen = 0;
            if (SUCCEEDED(IMF2DBuffer_GetContiguousLength(b2, &clen)) && SUCCEEDED(IMF2DBuffer_Lock2D(b2, &data, &pitch))) {
                if (pitch > 0) yuv_copy(p, data, clen, pitch);
                IMF2DBuffer_Unlock2D(b2);
                IMF2DBuffer_Release(b2);
                IMFMediaBuffer_Release(buf);
                if (pitch > 0) return;
                buf = NULL;
            } else {
                IMF2DBuffer_Release(b2);
            }
        }
        if (buf) IMFMediaBuffer_Release(buf);
        buf = NULL;
        if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(s, &buf))) {
            if (SUCCEEDED(IMFMediaBuffer_Lock(buf, &data, NULL, &len))) {
                yuv_copy(p, data, len, 0);
                IMFMediaBuffer_Unlock(buf);
            }
            IMFMediaBuffer_Release(buf);
        }
        return;
    }
    if (SUCCEEDED(IMFSample_GetBufferCount(s, &count)) && count == 1 && SUCCEEDED(IMFSample_GetBufferByIndex(s, 0, &buf))
            && SUCCEEDED(IMFMediaBuffer_QueryInterface(buf, &IID_IMF2DBuffer, (void **)&b2))) {
        LONG pitch = 0;
        if (SUCCEEDED(IMF2DBuffer_Lock2D(b2, &data, &pitch))) {
            for (y = 0; y < p->h; y++)   /* (scanline 0 is the top row, whatever the pitch's sign) */
                memcpy(p->bgra + (size_t)y * p->w * 4, data + (LONG_PTR)pitch * (y + p->ay) + (size_t)p->ax * 4,
                       (size_t)p->w * 4);
            IMF2DBuffer_Unlock2D(b2);
            IMF2DBuffer_Release(b2);
            IMFMediaBuffer_Release(buf);
            return;
        }
        IMF2DBuffer_Release(b2);
    }
    if (buf) IMFMediaBuffer_Release(buf);
    buf = NULL;
    if (FAILED(IMFSample_ConvertToContiguousBuffer(s, &buf))) return;
    if (SUCCEEDED(IMFMediaBuffer_Lock(buf, &data, NULL, &len))) {
        LONG stride = p->stride;
        /* (a padded frame in one block: decoders round the width - and the
           height: 1918 x 1080 comes as 1920 x 1088 - up to 16, 32, ... pixels;
           the rows are as far apart as the first such width the buffer fits) */
        if (p->fh > 0 && len > (DWORD)(stride < 0 ? -stride : stride) * (DWORD)p->fh) {
            int align;
            for (align = 16; align <= 512; align *= 2) {
                const DWORD pitch = (DWORD)((p->fw + align - 1) / align * align) * 4;
                if (len % pitch == 0 && len / pitch >= (DWORD)p->fh) {
                    stride = (stride < 0 ? -1 : 1) * (LONG)pitch;
                    break;
                }
            }
        }
        const BYTE *row0 = stride < 0 ? data + (size_t)(-stride) * (p->fh - 1) : data;
        if ((size_t)(stride < 0 ? -stride : stride) * p->fh <= len) {
            for (y = 0; y < p->h; y++) {
                const BYTE *src = row0 + (LONG_PTR)stride * (y + p->ay) + (size_t)p->ax * 4;
                memcpy(p->bgra + (size_t)y * p->w * 4, src, (size_t)p->w * 4);
            }
        }
        IMFMediaBuffer_Unlock(buf);
    }
    IMFMediaBuffer_Release(buf);
}

/* Brings the picture to time t (100 ns). A video that has ended keeps its
 * last frame, or starts again if `loop`. */
static void pic_seek(Pic *p, LONGLONG t, int loop)
{
    if (p->kind == 3) {
        const LONGLONG ft = bink_frame_time(p->bink);
        const int n = bink_frames(p->bink);
        LONGLONG want = ft > 0 ? t / ft : 0;     /* the frame to show */
        if (loop) want %= n;
        else if (want >= n) want = n - 1;
        if (want < bink_position(p->bink) - 1) bink_rewind(p->bink);
        while (bink_position(p->bink) <= want)
            if (!bink_pic_next(p)) break;
        return;
    }
    if (p->kind != 2) return;
    for (;;) {
        if (!p->have_next && !p->eof) {
            DWORD idx, flags = 0;
            LONGLONG ts = 0;
            IMFSample *s = NULL;
            if (FAILED(IMFSourceReader_ReadSample(p->rd, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &idx, &flags, &ts, &s))
                    || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                if (s) IMFSample_Release(s);
                if (loop && p->duration > 0 && p->base + p->duration <= t) {
                    PROPVARIANT pv;
                    PropVariantInit(&pv);
                    pv.vt = VT_I8;
                    pv.hVal.QuadPart = 0;
                    IMFSourceReader_SetCurrentPosition(p->rd, &GUID_NULL, &pv);
                    p->base += p->duration;
                    continue;
                }
                p->eof = 1;
                break;
            }
            if (!s) continue;                    /* a gap, a format change */
            p->next = s;
            p->next_ts = p->base + ts;
            p->have_next = 1;
        }
        if (!p->have_next || p->next_ts > t) break;
        sample_copy(p, p->next);
        IMFSample_Release(p->next);
        p->next = NULL;
        p->have_next = 0;
    }
}

/* The parts of the current picture that are not black: rows and columns
 * where enough pixels are brighter than dark grey (so noise or a small logo
 * in a black bar doesn't count). 0 if the picture is all dark. */
static int content_box(const Pic *p, int *x0, int *y0, int *x1, int *y1)
{
    int x, y, n;
    const int step = 2;
    *x0 = p->w; *y0 = p->h; *x1 = 0; *y1 = 0;
    for (y = 0; y < p->h; y += step) {
        for (x = 0, n = 0; x < p->w; x += step) {
            const uint8_t *s = p->bgra + ((size_t)y * p->w + x) * 4;
            if (s[0] + s[1] + s[2] > 3 * 32) n++;
        }
        if (n * step * 50 > p->w) { if (y < *y0) *y0 = y; if (y + step > *y1) *y1 = y + step; }
    }
    for (x = 0; x < p->w; x += step) {
        for (y = 0, n = 0; y < p->h; y += step) {
            const uint8_t *s = p->bgra + ((size_t)y * p->w + x) * 4;
            if (s[0] + s[1] + s[2] > 3 * 32) n++;
        }
        if (n * step * 50 > p->h) { if (x < *x0) *x0 = x; if (x + step > *x1) *x1 = x + step; }
    }
    return *x1 > *x0 && *y1 > *y0;
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

/* Black borders that are part of the picture (letterboxed or pillarboxed
 * videos): measured on frames across the video - a quartile of each side,
 * so a flash or a title card now and then doesn't hide them - and cut off,
 * so "Fill" fills the screen with the picture, not with its bars. */
#define CF_SAMPLES 24
static void content_find(Pic *p)
{
    int bx0[CF_SAMPLES], by0[CF_SAMPLES], bx1[CF_SAMPLES], by1[CF_SAMPLES], n = 0, k;
    int x0, y0, x1, y1;
    p->cx = 0; p->cy = 0; p->cw = p->w; p->ch = p->h;
    if (p->kind == 1) {
        if (content_box(p, &bx0[0], &by0[0], &bx1[0], &by1[0])) n = 1;
    } else if (p->kind == 2 && p->duration > 0) {
        for (k = 1; k <= CF_SAMPLES; k++) {
            PROPVARIANT pv;
            const LONGLONG want = p->duration * k / (CF_SAMPLES + 2);
            IMFSample *s = NULL;
            int reads;
            PropVariantInit(&pv);
            pv.vt = VT_I8;
            pv.hVal.QuadPart = want;
            if (FAILED(IMFSourceReader_SetCurrentPosition(p->rd, &GUID_NULL, &pv))) break;
            /* A seek lands on the key frame before `want` - often a scene cut
               or a flash: decode on to the frame at `want`. */
            for (reads = 0; reads < 600; reads++) {
                DWORD idx, flags = 0;
                LONGLONG ts = 0;
                IMFSample *next = NULL;
                if (FAILED(IMFSourceReader_ReadSample(p->rd, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &idx, &flags,
                                                      &ts, &next)) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                    if (next) IMFSample_Release(next);
                    break;
                }
                if (!next) continue;
                if (s) IMFSample_Release(s);
                s = next;
                if (ts >= want) break;
            }
            if (!s) continue;
            sample_copy(p, s);
            IMFSample_Release(s);
            if (content_box(p, &bx0[n], &by0[n], &bx1[n], &by1[n])) n++;
        }
        {   /* back to the start */
            PROPVARIANT pv;
            PropVariantInit(&pv);
            pv.vt = VT_I8;
            pv.hVal.QuadPart = 0;
            IMFSourceReader_SetCurrentPosition(p->rd, &GUID_NULL, &pv);
            memset(p->bgra, 0, (size_t)p->w * p->h * 4);
        }
    } else if (p->kind == 3) {
        /* Bink decodes frame after frame: samples from its first 20 s. */
        const int frames = bink_frames(p->bink) < 600 ? bink_frames(p->bink) : 600;
        const int every = frames / CF_SAMPLES > 0 ? frames / CF_SAMPLES : 1;
        for (k = 0; k < frames && n < CF_SAMPLES; k++) {
            if (!bink_pic_next(p)) break;
            if (k % every == every / 2 && content_box(p, &bx0[n], &by0[n], &bx1[n], &by1[n])) n++;
        }
        bink_rewind(p->bink);
        memset(p->bgra, 0, (size_t)p->w * p->h * 4);
    }
    if (!n) return;
    qsort(bx0, (size_t)n, sizeof(int), cmp_int);
    qsort(by0, (size_t)n, sizeof(int), cmp_int);
    qsort(bx1, (size_t)n, sizeof(int), cmp_int);
    qsort(by1, (size_t)n, sizeof(int), cmp_int);
    /* The picture's edges are where most frames reach: the quartile on the
       wide side (dark scenes reach less far; a flash, the whole frame). */
    x0 = bx0[n / 4]; y0 = by0[n / 4]; x1 = bx1[n - 1 - n / 4]; y1 = by1[n - 1 - n / 4];
    if (x1 > p->w) x1 = p->w;
    if (y1 > p->h) y1 = p->h;
    /* Only clear borders: 1.5% or more of a side, and a real picture left. */
    if (x1 - x0 < p->w / 4 || y1 - y0 < p->h / 4) return;
    if (x0 * 1000 < p->w * 15) x0 = 0;
    if ((p->w - x1) * 1000 < p->w * 15) x1 = p->w;
    if (y0 * 1000 < p->h * 15) y0 = 0;
    if ((p->h - y1) * 1000 < p->h * 15) y1 = p->h;
    p->cx = x0; p->cy = y0; p->cw = x1 - x0; p->ch = y1 - y0;
}


/* Box-filtered copy of source rectangle (sx, sy, sw, sh) of `p` into
 * destination rectangle (dx, dy, dw, dh) of a BGRA picture `dst` (dst_w wide). */
static void pic_draw(const Pic *p, double sx, double sy, double sw, double sh,
                     uint8_t *dst, int dst_w, int dx, int dy, int dw, int dh)
{
    int x, y;
    if (!p->bgra || dw <= 0 || dh <= 0) return;
    for (y = 0; y < dh; y++) {
        double y0 = sy + sh * y / dh, y1 = sy + sh * (y + 1) / dh;
        int iy0 = (int)y0, iy1 = (int)(y1 + 0.999);
        if (iy0 < 0) iy0 = 0;
        if (iy1 > p->h) iy1 = p->h;
        if (iy1 <= iy0) iy1 = iy0 + 1;
        for (x = 0; x < dw; x++) {
            double x0 = sx + sw * x / dw, x1 = sx + sw * (x + 1) / dw;
            int ix0 = (int)x0, ix1 = (int)(x1 + 0.999), i, j, n = 0;
            unsigned acc[3] = { 0, 0, 0 };
            uint8_t *o = dst + ((size_t)(dy + y) * dst_w + dx + x) * 4;
            if (ix0 < 0) ix0 = 0;
            if (ix1 > p->w) ix1 = p->w;
            if (ix1 <= ix0) ix1 = ix0 + 1;
            for (j = iy0; j < iy1 && j < p->h; j++)
                for (i = ix0; i < ix1 && i < p->w; i++) {
                    const uint8_t *s = p->bgra + ((size_t)j * p->w + i) * 4;
                    acc[0] += s[0]; acc[1] += s[1]; acc[2] += s[2];
                    n++;
                }
            if (n) { o[0] = (uint8_t)(acc[0] / n); o[1] = (uint8_t)(acc[1] / n); o[2] = (uint8_t)(acc[2] / n); }
            o[3] = 255;
        }
    }
}

/* The top picture: the big screen, shown at 16:9, stored as 320 x 220. */
static void draw_top(const Pic *p, int fit, uint8_t *frame, int W)
{
    const double screen = 16.0 / 9.0;
    const int TH = top_h(W);
    double aspect, sx = 0, sy = 0, sw, sh;
    int dx = 0, dy = 0, dw = W, dh = TH;
    if (!p->bgra) return;
    sx = p->cx; sy = p->cy; sw = p->cw; sh = p->ch;
    aspect = sw * p->par / sh;
    if (fit == MOVIE_FILL) {
        if (aspect > screen) { sw = sh * screen / p->par; sx = p->cx + (p->cw - sw) / 2; }
        else                 { sh = sw * p->par / screen; sy = p->cy + (p->ch - sh) / 2; }
    } else if (fit == MOVIE_FIT) {
        if (aspect > screen) { dh = (int)(TH * screen / aspect + 0.5); dy = (TH - dh) / 2; }
        else                 { dw = (int)(W * aspect / screen + 0.5); dx = (W - dw) / 2; }
    }
    pic_draw(p, sx, sy, sw, sh, frame, W, dx, dy, dw, dh);
}

/* The bottom strip: 320 x 100, filled (cropped to its shape). */
static void draw_bottom(const Pic *p, uint8_t *frame, int W)
{
    const int TH = top_h(W), BH = W - TH;
    const double strip = (double)W / BH;
    double aspect, sx = 0, sy = 0, sw, sh;
    if (!p->bgra) return;
    sx = p->cx; sy = p->cy; sw = p->cw; sh = p->ch;
    aspect = sw * p->par / sh;
    if (aspect > strip) { sw = sh * strip / p->par; sx = p->cx + (p->cw - sw) / 2; }
    else                { sh = sw * p->par / strip; sy = p->cy + (p->ch - sh) / 2; }
    pic_draw(p, sx, sy, sw, sh, frame, W, 0, TH, W, BH);
}

static void compose(Pic *top, Pic *bottom, int fit, LONGLONG t, uint8_t *frame, int W)
{
    size_t i;
    for (i = 0; i < (size_t)W * W; i++) {
        frame[i * 4 + 0] = frame[i * 4 + 1] = frame[i * 4 + 2] = 0;
        frame[i * 4 + 3] = 255;
    }
    pic_seek(top, t, 0);
    pic_seek(bottom, t, 1);
    draw_top(top, fit, frame, W);
    draw_bottom(bottom, frame, W);
}

/* Media Foundation, started once (0: it can't be - a Windows "N" edition
 * without the Media Feature Pack). */
static int mf_start(void)
{
    static volatile LONG state;   /* 0 not tried, 1 running, 2 failed */
    /* (mfplat / mfreadwrite are delay-loaded: called without them the
     * launcher would crash, so first see that Windows has them) */
    if (state == 0)
        InterlockedCompareExchange(&state,
            LoadLibraryExW(L"mfplat.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32) &&
            LoadLibraryExW(L"mfreadwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32) &&
            SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)) ? 1 : 2, 0);
    return state == 1;
}

int movie_preview(const WCHAR *video, const WCHAR *bottom, int fit, double seconds, uint8_t *bgra,
                  WCHAR *err, size_t errn)
{
    Pic top, bot;
    int ok;
    mf_start();
    if (!pic_open(&top, video, PART_TOP, err, errn)) return 0;
    if (!pic_open(&bot, bottom, PART_BOTTOM, err, errn)) { pic_close(&top); return 0; }
    compose(&top, &bot, fit, (LONGLONG)(seconds * 1e7), bgra, MOVIE_W);
    ok = 1;
    pic_close(&top);
    pic_close(&bot);
    return ok;
}

int movie_make(MovieJob *job)
{
    Pic top, bot;
    FILE *f = NULL;
    BinkWriter *w = NULL;
    uint8_t *frame = NULL;
    LONGLONG length;
    int n, i, ok = 0;
    const int W = job->size ? job->size : MOVIE_W;
    WCHAR tmp[MAX_PATH + 8];
    job->err[0] = 0;
    if (W != 320 && W != 640 && W != 1024) { wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Movie size must be 320, 640 or 1024."); return 0; }
    mf_start();
    if (!pic_open(&top, job->video, PART_TOP, job->err, sizeof job->err / sizeof *job->err)) return 0;
    if (top.kind == 0) { wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Choose the video."); return 0; }
    if (!pic_open(&bot, job->bottom, PART_BOTTOM, job->err, sizeof job->err / sizeof *job->err)) { pic_close(&top); return 0; }
    length = (top.kind == 2 || top.kind == 3) && top.duration > 0 ? top.duration : 10 * 10000000LL;
    if (job->max_seconds > 0 && length > (LONGLONG)job->max_seconds * 10000000LL)
        length = (LONGLONG)job->max_seconds * 10000000LL;
    n = (int)((length * FPS + 9999999) / 10000000);
    if (n < 1) n = 1;
    frame = (uint8_t *)malloc((size_t)W * W * 4);
    swprintf_s(tmp, sizeof tmp / sizeof *tmp, L"%s.part", job->out);
    if (!frame || _wfopen_s(&f, tmp, L"wb") || !f || !(w = bink_writer_open_size(f, n, W, W))) {
        wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Could not create the movie file.");
        goto done;
    }
    for (i = 0; i < n; i++) {
        int r;
        if (job->cancel) { wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Stopped."); goto done; }
        compose(&top, &bot, job->fit, (LONGLONG)i * 10000000LL / FPS, frame, W);
        r = bink_writer_frame(w, frame);
        if (r == 0) goto nomem;
        if (r < 0) { wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Could not write the movie (disk full?)."); goto done; }
        if (job->notify) PostMessageW(job->notify, job->msg, (WPARAM)((i + 1) * 100 / n), 0);
    }
    bink_writer_close(w); w = NULL;
    fclose(f); f = NULL;
    pic_close(&top);   /* the video may be the movie being replaced (made again) */
    pic_close(&bot);
    if (!MoveFileExW(tmp, job->out, MOVEFILE_REPLACE_EXISTING)) {
        wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Could not save the movie file (is the game using it?).");
        goto done;
    }
    job->frames = n;
    ok = 1;
    goto done;
nomem:
    wcscpy_s(job->err, sizeof job->err / sizeof *job->err, L"Out of memory.");
done:
    bink_writer_free(w);
    if (f) { fclose(f); DeleteFileW(tmp); }
    free(frame);
    pic_close(&top);
    pic_close(&bot);
    return ok;
}
