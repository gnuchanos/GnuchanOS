/*
 * ss_effect.h — the animations a screen saver can draw.
 *
 * An effect draws one frame onto a drawable and can reset itself. The running
 * loop in GnuChanSS.c owns the window, the timing and the "someone moved the
 * mouse" decision; an effect owns only the picture.
 *
 * The pipe effect is a colony of worms. A dozen-and-a-half of them crawl about
 * the screen, each running straight for a while and then turning a right angle,
 * each with its own width and its own body length. A worm is drawn the way a
 * creature is drawn and not the way a pipe is: a dark outline all round it, its
 * own purple body, a pale shine down its back, and a head at the front carrying
 * two eyes. Every worm picks its own purple from a palette of purples, so the
 * colony is a crowd and not one colour repeated.
 *
 * The body is kept as its CORNERS, not as every step it has taken: a worm going
 * straight is one segment, and a point is added only where it turns. That is
 * what the shape is, and it is also what keeps the drawing cheap — every thick
 * line is tessellated by the X server, and a thousand collinear points would
 * cost it a thousand pieces to say one straight line.
 *
 * The wall is a grid of bricks on a plane turned about the vertical axis.
 *
 * Everything is Xlib's own drawing — thick lines and filled discs — because a
 * screen saver that needed a graphics stack would be one that does not start on
 * a machine whose driver is having a bad day.
 */
#ifndef GNUCHANSS_EFFECT_H
#define GNUCHANSS_EFFECT_H

#include <X11/Xlib.h>
#include "ss_config.h"

/* The largest sides a screen may have, which bounds the effect's own buffers.
   Far above any real display; it exists so an effect never allocates without a
   written ceiling. */
#define SS_MAX_SCREEN 16384

/* How many worms crawl at once, and the most corners any one body may hold. */
#define SS_MAX_PIPES       16
#define SS_PIPE_MAX_POINTS 32

/* How many purples the worms are drawn in. Each worm picks one at random, so
   the colony is a spread of purples and not one flat colour. */
#define SS_PIPE_COLOURS 8

/* One crawling worm. `x`,`y` is the head in screen pixels and `dx`,`dy` the way
   it is going (one of the four axes); the head walks until `run` pixels are
   spent, then turns a right angle, leaving a corner where it turned. `thickness`
   and `trail` are its own size: how wide it is drawn, and how long its body is
   in pixels. `colour` is its index into the purple palette.
 *
 * The body is the corners `px`,`py`, oldest first and the head last. The tail is
 * the first point, which slides back along the first segment as the body is
 * cropped to the worm's length, so only corners are ever stored. */
typedef struct SsPipe {
    double x;
    double y;
    double dx;
    double dy;
    double run;

    int colour;
    int thickness;   /* the body's width in pixels                 */
    double trail;    /* the body's length in pixels                */

    double px[SS_PIPE_MAX_POINTS];
    double py[SS_PIPE_MAX_POINTS];
    int count;       /* corners kept, up to SS_PIPE_MAX_POINTS     */
} SsPipe;

/* WHICH effect runs (SsEffectKind) is the settings' business and lives in
   ss_config.h; this header is the running one. */
typedef struct SsEffect SsEffect;

struct SsEffect {
    /* Draw one frame onto `drawable` (the window, or an off-screen copy of it),
       using `gc`. The colours are already pixels. */
    void (*draw)(SsEffect *self, Display *display, Drawable drawable, GC gc);
    void (*reset)(SsEffect *self, int width, int height);

    int width;
    int height;

    /* The session's own colours, as pixels: the background the frame is cleared
       to, and the colour the wall is built from. */
    unsigned long primary;
    unsigned long background;

    /* The colours the worms are drawn from — each worm's body, its dark outline
       and the pale shine down its back, one of each per palette entry, plus the
       white of the eyes. Made on the first draw, when there is a display to
       allocate colours on. */
    unsigned long palette[SS_PIPE_COLOURS];        /* the worms' bodies  */
    unsigned long palette_dark[SS_PIPE_COLOURS];   /* their outlines     */
    unsigned long belly[SS_PIPE_COLOURS];          /* the shine on them  */
    unsigned long eye;
    int colours_ready;

    SsPipe pipes[SS_MAX_PIPES];

    /* The wall's state. */
    double wall_angle;
    int wall_cols;
    int wall_rows;
};

/* Set up an effect of the given kind. The colours are pixels, already
   allocated by the caller against the display. */
void ss_effect_init(SsEffect *effect, SsEffectKind kind,
                    int width, int height,
                    unsigned long primary, unsigned long background);

/* Free whatever the effect allocated. Safe on a zeroed effect. */
void ss_effect_free(SsEffect *effect);

#endif /* GNUCHANSS_EFFECT_H */
