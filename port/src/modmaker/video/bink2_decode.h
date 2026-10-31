/* WWE12-PC: Bink 2 video reader (bink2_decode.c), for the movies of the other WWE games a superstar is ported from:
 * WWE '13 (KB2d), WWE 2K14 (KB2f) and WWE 2K15-2K17 (KB2i), titantrons laid out like WWE '12's. The decoder is the
 * one Paul B Mahol proposed to FFmpeg in 2020 (from Konstantin Shishkov's research), LGPL 2.1+, in video/bink2/;
 * the reading around it is ours, on the model of bink_decode.h. Video only, no alpha. */
#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <wchar.h>
typedef wchar_t WCHAR;
typedef long long LONGLONG;
#endif
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Bink2Reader Bink2Reader;

/* Opens a Bink 2 movie (.bik2, .bk2); NULL if it isn't one (err says why). */
Bink2Reader *bink2_open(const WCHAR *path, WCHAR *err, size_t errn);
/* The same from memory (the bytes are copied). */
Bink2Reader *bink2_open_memory(const uint8_t *data, size_t size, WCHAR *err, size_t errn);
void bink2_close(Bink2Reader *r);

int bink2_width(const Bink2Reader *r);
int bink2_height(const Bink2Reader *r);
int bink2_frames(const Bink2Reader *r);
char bink2_revision(const Bink2Reader *r); /* the 4th byte of KB2x */
/* Frame duration in 100 ns units. */
LONGLONG bink2_frame_time(const Bink2Reader *r);

/* Decodes the next frame (after the last one: the first again). 0 on a damaged frame. */
int bink2_next(Bink2Reader *r);
/* The decoded frame as BGRA (width x height x 4). */
void bink2_bgra(const Bink2Reader *r, uint8_t *bgra);
/* Back to before the first frame. */
void bink2_rewind(Bink2Reader *r);
/* Frames decoded since opening (or since it started over). */
int bink2_position(const Bink2Reader *r);

#ifdef __cplusplus
}
#endif
