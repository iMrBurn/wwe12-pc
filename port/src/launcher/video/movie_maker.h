/* WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit
 * given), https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/launcher/movie_maker.h at b2521fe.
 * WWE '12 titantron movies have the same layout (Bink 1 "BIKi", 320 x 320, 30 fps). */
/* USER MOVIES maker (movie_maker.c): entrance movies for the game's Custom
 * Movies folder, as Bink files laid out like the game's titantron movies. */
#pragma once
#include <windows.h>
#include <stdint.h>

#define MOVIE_W 320
#define MOVIE_H 320

enum { MOVIE_FIT = 0,      /* whole picture, black bars */
       MOVIE_FILL = 1,     /* fills the screen, edges cut off */
       MOVIE_STRETCH = 2 };

typedef struct MovieJob {
    WCHAR video[MAX_PATH];     /* the big-screen video (or a picture) */
    WCHAR bottom[MAX_PATH];    /* the strip below it: picture or video; "" = black */
    int fit;                   /* MOVIE_FIT / MOVIE_FILL / MOVIE_STRETCH */
    int max_seconds;           /* 0 = the whole video */
    int size;                  /* the movie's size: 0 / 320 (the game's own), 640 or 1024 (HD: the same
                                  layout, the big screen the top size*220/320 rows) */
    WCHAR out[MAX_PATH];       /* the .bik to write */
    volatile LONG cancel;
    HWND notify;               /* gets `msg` with the percentage done */
    UINT msg;
    int frames;                /* out: frames written */
    WCHAR err[512];            /* out: why it failed */
} MovieJob;

/* Writes job->out; 1 on success. Call from a worker thread (COM initialised). */
int movie_make(MovieJob *job);

/* The movie's frame at `seconds` as 320 x 320 BGRA (for a preview; any size
 * job looks the same, sharper). */
int movie_preview(const WCHAR *video, const WCHAR *bottom, int fit, double seconds, uint8_t *bgra,
                  WCHAR *err, size_t errn);
