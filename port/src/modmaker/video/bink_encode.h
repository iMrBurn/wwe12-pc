/* WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit
 * given), https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/launcher/bink_encode.h at b2521fe.
 * WWE '12 titantron movies have the same layout (Bink 1 "BIKi", 320 x 320, 30 fps). */
/* Bink 1 ("BIKi") encoder for USER MOVIES (bink_encode.c): 320 x 320 (or
 * any size of 16s up to 1024 x 1024: HD titantron movies), 30 fps, no
 * audio, laid out like the game's titantron movies. Plain C. */
#pragma once
#include <stdint.h>
#include <stdio.h>

#define BINK_ENC_W 320
#define BINK_ENC_H 320
#define BINK_ENC_FPS 30

typedef struct BinkWriter BinkWriter;

/* Starts a movie of `frames` frames in `f` (opened "wb"; the header and the
 * frame index are written by bink_writer_close). NULL: out of memory. */
BinkWriter *bink_writer_open(FILE *f, int frames);
/* The same at width x height (multiples of 16, 16..1024). NULL: out of
 * memory or a bad size. */
BinkWriter *bink_writer_open_size(FILE *f, int frames, int width, int height);
/* The same with an alpha plane (the BGRA's A; header flag 0x00100000), as the
 * game's arena transition movies (evtmov.pac MOVT, 640 x 640). */
BinkWriter *bink_writer_open_alpha(FILE *f, int frames, int width, int height);
/* Encodes the next frame (width x height BGRA): 1, 0 out of memory, -1 write failed. */
int bink_writer_frame(BinkWriter *w, const uint8_t *bgra);
/* The next frame the same as the last one (all skip blocks; encoded once,
 * then its bytes reused - cheap at any size): 1, 0, -1 as above. The first
 * frame must be a bink_writer_frame. */
int bink_writer_repeat(BinkWriter *w);
/* Writes the header and the frame index, and frees the writer. */
int bink_writer_close(BinkWriter *w);
/* Frees a writer that won't be finished. */
void bink_writer_free(BinkWriter *w);
