/* WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit
 * given), https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/launcher/bink_decode.h at b2521fe.
 * WWE '12 titantron movies have the same layout (Bink 1 "BIKi", 320 x 320, 30 fps). */
/* Bink 1 video reader (bink_decode.c): the game's titantron movies and the
 * ones movie_maker.c writes. LGPL 2.1+ (ported from FFmpeg). */
#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <wchar.h>
typedef wchar_t WCHAR;
typedef long long LONGLONG;
#endif
#include <stdint.h>

typedef struct BinkReader BinkReader;

/* Opens a .bik; NULL if it isn't a Bink 1 video (err says why). */
BinkReader *bink_open(const WCHAR *path, WCHAR *err, size_t errn);
void bink_close(BinkReader *r);

int bink_width(const BinkReader *r);
int bink_height(const BinkReader *r);
int bink_frames(const BinkReader *r);
/* Frame duration in 100 ns units. */
LONGLONG bink_frame_time(const BinkReader *r);

/* Decodes the next frame (after the last one: the first again). 0 on a
 * damaged frame. */
int bink_next(BinkReader *r);
/* The decoded frame as BGRA (width x height x 4). */
void bink_bgra(const BinkReader *r, uint8_t *bgra);
/* Back to before the first frame. */
void bink_rewind(BinkReader *r);
/* Frames decoded since opening (or since it started over). */
int bink_position(const BinkReader *r);
/* 1 if the movie has an alpha plane (then bink_bgra's A is it, else 255). */
int bink_has_alpha(const BinkReader *r);
