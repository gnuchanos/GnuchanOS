/*
 * ss_effect.c — the two effects.
 *
 * The pipe effect is a colony of worms. A dozen-and-a-half of them crawl about
 * the screen, each running straight for a while and then turning a right angle,
 * and each dragging a fixed-length body behind it: the head steps forward, the
 * tail follows, and the worm wanders for as long as the show runs — it never
 * jumps back to a place it has been. Each worm has its own width and its own
 * body length, and its own purple, so the colony is a crowd of individuals.
 *
 * A worm is drawn the way a creature is drawn, not the way a pipe is. Its body
 * goes down in three passes: a wider dark line for the outline, then its own
 * purple on top leaving the outline showing at both edges, then a pale line down
 * the middle for the shine along its back. A round head caps the front, and two
 * eyes sit on the head, so the thing that leads the worm reads as a face.
 *
 * The body is kept as its CORNERS, not as every step it has taken: a worm going
 * straight is one segment, and a point is added only where it turns. That is
 * what the shape is, and it is also what keeps the drawing cheap — the X server
 * tessellates every thick line itself, and a thousand collinear points would
 * cost it a thousand pieces to say one straight line.
 *
 * The wall is a grid of bricks on a plane turned about the vertical axis.
 *
 * Everything is drawn with Xlib's own primitives — thick lines and filled discs
 * — so nothing here needs a graphics stack, and the whole saver starts on any
 * server that can open a window at all.
 *
 * The randomness is seeded once, lazily, with the wall clock. A screen saver
 * that reset to the same picture on every run would be one you recognise from
 * across the room, which is the opposite of the point.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ss_effect.h"

/* How far a worm creeps each frame, in pixels. Small, so the motion is a crawl
   and not a jerk. */
#define SS_PIPE_STEP 1.0

/* How far a worm goes before it may turn, at least and at most, in pixels.
   Short runs mean plenty of corners. */
#define SS_PIPE_RUN_MIN 60
#define SS_PIPE_RUN_MAX 260

/* A worm's own size, chosen at random: how wide its body is drawn, and how long
   its body is in pixels. Every worm differs, so the screen holds thin short ones
   and fat long ones at once. */
#define SS_PIPE_THICK_MIN  10
#define SS_PIPE_THICK_MAX  22
#define SS_PIPE_TRAIL_MIN 220
#define SS_PIPE_TRAIL_MAX 900

/* How far from an edge a worm starts bending back inward, so it never walks off
   the screen and hovers there. */
#define SS_PIPE_MARGIN 34

/* Seed the generator once. Called from the reset of an effect that uses random
   numbers, which is the earliest it can matter. */
static void seed_once(void) {
    static int seeded = 0;
    if (!seeded) {
        seeded = 1;
        srand((unsigned int)time(NULL));
    }
}

/* A number in [low, high). */
static double random_range(double low, double high) {
    return low + (high - low) * ((double)rand() / ((double)RAND_MAX + 1.0));
}

/* An integer in [low, high] (both ends included). */
static int random_int(int low, int high) {
    return low + rand() % (high - low + 1);
}

/* The distance between two points. */
static double distance(double ax, double ay, double bx, double by) {
    double dx = bx - ax;
    double dy = by - ay;
    return sqrt(dx * dx + dy * dy);
}

/* --- colour --------------------------------------------------------------- */

/* An HSV colour to RGB, each in 0..1. Hue is in turns (0..360). Used to build
   the purples straight from the colour wheel, so the palette is a family of
   genuinely different purples and not one purple scaled up and down. */
static void hsv_to_rgb(double hue, double sat, double val, double rgb[3]) {
    double c = val * sat;
    double h = hue / 60.0;
    double x = c * (1.0 - fabs(fmod(h, 2.0) - 1.0));
    double r = 0.0, g = 0.0, b = 0.0;
    if (h < 1.0)      { r = c; g = x; }
    else if (h < 2.0) { r = x; g = c; }
    else if (h < 3.0) { g = c; b = x; }
    else if (h < 4.0) { g = x; b = c; }
    else if (h < 5.0) { r = x; b = c; }
    else              { r = c; b = x; }
    double m = val - c;
    rgb[0] = r + m;
    rgb[1] = g + m;
    rgb[2] = b + m;
}

/* Allocate one RGB colour as a pixel on the display's default colormap. Zero on
   a failure, so a caller can fall back. */
static unsigned long alloc_rgb(Display *display, double r, double g, double b) {
    Colormap colormap = DefaultColormap(display, DefaultScreen(display));
    XColor colour;
    memset(&colour, 0, sizeof(colour));
    colour.red   = (unsigned short)(r * 65535.0);
    colour.green = (unsigned short)(g * 65535.0);
    colour.blue  = (unsigned short)(b * 65535.0);
    colour.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(display, colormap, &colour)) {
        return colour.pixel;
    }
    return 0;
}

/* Build the palette: a walk round the violet-to-magenta part of the colour
   wheel, and for each step three colours — the worm's body, a much darker
   version for its outline, and a much paler one for the shine down its back —
   plus the white of the eyes. Made once, on the first frame, when there is a
   display to allocate colours on. */
static void prepare_palette(SsEffect *effect, Display *display) {
    if (effect->colours_ready) {
        return;
    }
    for (int i = 0; i < SS_PIPE_COLOURS; i++) {
        double t = (double)i / (double)(SS_PIPE_COLOURS - 1);
        double hue = 258.0 + t * 46.0;    /* indigo -> purple -> magenta */
        double sat = 0.70 + 0.30 * sin(t * 3.14159);
        double val = 0.62 + t * 0.34;     /* deeper at one end, brighter at
                                             the other */
        double rgb[3];
        hsv_to_rgb(hue, sat, val, rgb);

        unsigned long body = alloc_rgb(display, rgb[0], rgb[1], rgb[2]);
        effect->palette[i] = body ? body : effect->primary;

        double dark[3];
        hsv_to_rgb(hue, sat, val * 0.35, dark);
        unsigned long outline = alloc_rgb(display, dark[0], dark[1], dark[2]);
        effect->palette_dark[i] = outline ? outline : effect->background;

        double pale[3];
        hsv_to_rgb(hue, sat * 0.45, val + (1.0 - val) * 0.55, pale);
        unsigned long shine = alloc_rgb(display, pale[0], pale[1], pale[2]);
        effect->belly[i] = shine ? shine : body;
    }
    {
        unsigned long white = alloc_rgb(display, 0.97, 0.95, 1.0);
        effect->eye = white ? white : effect->primary;
    }
    effect->colours_ready = 1;
}

/* --- one crawling worm ---------------------------------------------------- */

/* Crop the body from its tail until it is no longer than the worm's own trail
   length. The tail is the first point, which slides forward along the first
   segment; once a whole segment can be dropped it is, and a corner goes with it.
   That is what keeps a crawling worm a fixed length instead of growing for
   ever. */
static void pipe_crop(SsPipe *pipe) {
    while (pipe->count >= 3) {
        double first = distance(pipe->px[0], pipe->py[0],
                                pipe->px[1], pipe->py[1]);
        double total = 0.0;
        for (int i = 0; i + 1 < pipe->count; i++) {
            total += distance(pipe->px[i], pipe->py[i],
                              pipe->px[i + 1], pipe->py[i + 1]);
        }
        if (total - first >= pipe->trail) {
            memmove(pipe->px, pipe->px + 1,
                    (size_t)(pipe->count - 1) * sizeof(double));
            memmove(pipe->py, pipe->py + 1,
                    (size_t)(pipe->count - 1) * sizeof(double));
            pipe->count--;
        } else {
            double move = total - pipe->trail;
            if (first > 0.0 && move > 0.0) {
                double s = move / first;
                pipe->px[0] += (pipe->px[1] - pipe->px[0]) * s;
                pipe->py[0] += (pipe->py[1] - pipe->py[0]) * s;
            }
            break;
        }
    }
}

/* Start a worm somewhere on the screen, facing a random way, sized at random,
   its body just the head. Called from the reset; a crawling worm never needs
   starting again, because it wanders rather than fills up. */
static void pipe_spawn(SsEffect *effect, SsPipe *pipe) {
    pipe->x = random_range(SS_PIPE_MARGIN, effect->width - SS_PIPE_MARGIN);
    pipe->y = random_range(SS_PIPE_MARGIN, effect->height - SS_PIPE_MARGIN);
    switch (rand() % 4) {
    case 0: pipe->dx = 1;  pipe->dy = 0;  break;
    case 1: pipe->dx = -1; pipe->dy = 0;  break;
    case 2: pipe->dx = 0;  pipe->dy = 1;  break;
    default: pipe->dx = 0; pipe->dy = -1; break;
    }
    pipe->run = random_range(SS_PIPE_RUN_MIN, SS_PIPE_RUN_MAX);
    pipe->colour = rand() % SS_PIPE_COLOURS;
    pipe->thickness = random_int(SS_PIPE_THICK_MIN, SS_PIPE_THICK_MAX);
    pipe->trail = random_range(SS_PIPE_TRAIL_MIN, SS_PIPE_TRAIL_MAX);
    pipe->px[0] = pipe->x;
    pipe->py[0] = pipe->y;
    pipe->count = 1;
}

/* Turn a right angle: the worm keeps to the two axes, so the new direction is
   the old one turned a quarter either way. The turn is bent away from any edge
   the worm is near, so it walks back inward instead of grinding along the wall.
 */
static void pipe_turn(SsEffect *effect, SsPipe *pipe) {
    double ndx = -pipe->dy;
    double ndy = pipe->dx;
    if (rand() & 1) {
        ndx = pipe->dy;
        ndy = -pipe->dx;
    }
    if (pipe->x > effect->width - SS_PIPE_MARGIN && ndx > 0) {
        ndx = -ndx; ndy = -ndy;
    } else if (pipe->x < SS_PIPE_MARGIN && ndx < 0) {
        ndx = -ndx; ndy = -ndy;
    } else if (pipe->y > effect->height - SS_PIPE_MARGIN && ndy > 0) {
        ndx = -ndx; ndy = -ndy;
    } else if (pipe->y < SS_PIPE_MARGIN && ndy < 0) {
        ndx = -ndx; ndy = -ndy;
    }
    pipe->dx = ndx;
    pipe->dy = ndy;
    pipe->run = random_range(SS_PIPE_RUN_MIN, SS_PIPE_RUN_MAX);
}

/* Move a worm a step. While it runs straight the head is the last point of the
   body and simply moves; when it turns, the point it turned at is kept as a
   corner and a fresh point begins the next straight. The body is then cropped
   back to the worm's length. Only corners are ever added, so a straight run of
   any length stays one segment. */
static void pipe_advance(SsEffect *effect, SsPipe *pipe) {
    double nx = pipe->x + pipe->dx * SS_PIPE_STEP;
    double ny = pipe->y + pipe->dy * SS_PIPE_STEP;

    int turn = (pipe->run <= 0.0) ||
               nx < SS_PIPE_MARGIN || ny < SS_PIPE_MARGIN ||
               nx > effect->width - SS_PIPE_MARGIN ||
               ny > effect->height - SS_PIPE_MARGIN;
    if (turn) {
        if (pipe->count >= SS_PIPE_MAX_POINTS) {
            memmove(pipe->px, pipe->px + 1,
                    (size_t)(pipe->count - 1) * sizeof(double));
            memmove(pipe->py, pipe->py + 1,
                    (size_t)(pipe->count - 1) * sizeof(double));
            pipe->count--;
        }
        pipe->px[pipe->count] = pipe->x;
        pipe->py[pipe->count] = pipe->y;
        pipe->count++;

        pipe_turn(effect, pipe);
        nx = pipe->x + pipe->dx * SS_PIPE_STEP;
        ny = pipe->y + pipe->dy * SS_PIPE_STEP;
    }

    if (nx < 0.0) nx = 0.0;
    if (ny < 0.0) ny = 0.0;
    if (nx > effect->width) nx = effect->width;
    if (ny > effect->height) ny = effect->height;

    pipe->x = nx;
    pipe->y = ny;
    pipe->px[pipe->count - 1] = nx;
    pipe->py[pipe->count - 1] = ny;
    pipe->run -= SS_PIPE_STEP;

    pipe_crop(pipe);
}

/* A filled disc, centred. */
static void disc(Display *display, Drawable drawable, GC gc,
                 double x, double y, double r) {
    int ri = (int)(r + 0.5);
    if (ri < 1) {
        ri = 1;
    }
    XFillArc(display, drawable, gc, (int)(x - ri), (int)(y - ri),
             (unsigned int)(2 * ri), (unsigned int)(2 * ri), 0, 360 * 64);
}

/* Draw one worm. The body goes down three times — a wide dark outline, the
   worm's own purple inside it, a pale shine down the middle — and then the head
   is capped and given two eyes facing the way it is going. Drawing the body as
   a polyline with round caps and round joins means every corner is a smooth
   bend and the tail is rounded, so the worm is a body and not a bent stick. */
static void pipe_render(SsEffect *effect, Display *display, Drawable drawable,
                        GC gc, SsPipe *pipe) {
    if (pipe->count < 2) {
        return;
    }
    XPoint point[SS_PIPE_MAX_POINTS];
    for (int i = 0; i < pipe->count; i++) {
        point[i].x = (short)pipe->px[i];
        point[i].y = (short)pipe->py[i];
    }

    int outline = pipe->thickness + 6;
    int shine = pipe->thickness / 3;
    if (shine < 2) {
        shine = 2;
    }

    /* The dark outline, all round. */
    XSetForeground(display, gc, effect->palette_dark[pipe->colour]);
    XSetLineAttributes(display, gc, (unsigned int)outline, LineSolid,
                       CapRound, JoinRound);
    XDrawLines(display, drawable, gc, point, pipe->count, CoordModeOrigin);

    /* The body. */
    XSetForeground(display, gc, effect->palette[pipe->colour]);
    XSetLineAttributes(display, gc, (unsigned int)pipe->thickness, LineSolid,
                       CapRound, JoinRound);
    XDrawLines(display, drawable, gc, point, pipe->count, CoordModeOrigin);

    /* The pale shine down its back. */
    XSetForeground(display, gc, effect->belly[pipe->colour]);
    XSetLineAttributes(display, gc, (unsigned int)shine, LineSolid,
                       CapRound, JoinRound);
    XDrawLines(display, drawable, gc, point, pipe->count, CoordModeOrigin);

    /* The head: a disc a little bigger than the body, in the outline colour with
       the body colour on top, so it reads as a rounded snout. */
    double hx = pipe->x;
    double hy = pipe->y;
    double head_r = pipe->thickness / 2.0 + 3.0;
    XSetForeground(display, gc, effect->palette_dark[pipe->colour]);
    disc(display, drawable, gc, hx, hy, head_r);
    XSetForeground(display, gc, effect->palette[pipe->colour]);
    disc(display, drawable, gc, hx, hy, head_r - 3.0);

    /* The eyes: white with a dark pupil, set either side of the way the worm is
       going and carried a little ahead of the head, so it reads as a face. */
    double side_x = -pipe->dy;      /* across the direction of travel */
    double side_y = pipe->dx;
    double ahead = head_r * 0.45;
    double apart = head_r * 0.62;
    double ex = hx + pipe->dx * ahead;
    double ey = hy + pipe->dy * ahead;
    double eye_r = head_r * 0.42;
    if (eye_r < 2.0) eye_r = 2.0;
    double pupil_r = eye_r * 0.5;

    for (int s = -1; s <= 1; s += 2) {
        double exx = ex + side_x * apart * s;
        double eyy = ey + side_y * apart * s;
        XSetForeground(display, gc, effect->eye);
        disc(display, drawable, gc, exx, eyy, eye_r);
        XSetForeground(display, gc, effect->palette_dark[pipe->colour]);
        disc(display, drawable, gc, exx + pipe->dx * eye_r * 0.3,
             eyy + pipe->dy * eye_r * 0.3, pupil_r);
    }

    XSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinMiter);
}

/* One frame: creep every worm a step and draw the whole colony again on a
   cleared background, so the picture is always the worms where they are now. */
static void pipe_draw(SsEffect *effect, Display *display, Drawable drawable,
                      GC gc) {
    prepare_palette(effect, display);

    XSetForeground(display, gc, effect->background);
    XFillRectangle(display, drawable, gc, 0, 0,
                   (unsigned int)effect->width, (unsigned int)effect->height);

    for (int p = 0; p < SS_MAX_PIPES; p++) {
        pipe_advance(effect, &effect->pipes[p]);
        pipe_render(effect, display, drawable, gc, &effect->pipes[p]);
    }
}

static void pipe_reset(SsEffect *effect, int width, int height) {
    seed_once();
    effect->width = width;
    effect->height = height;
    for (int p = 0; p < SS_MAX_PIPES; p++) {
        pipe_spawn(effect, &effect->pipes[p]);
        /* Give each worm a head start of its own, so the first frame shows
           worms of different lengths and not a screen of dots. */
        int head = (p * 6 + 8) * 3;
        for (int s = 0; s < head; s++) {
            pipe_advance(effect, &effect->pipes[p]);
        }
    }
}

/* --- the 3D wall ---------------------------------------------------------- */

static void wall_reset(SsEffect *effect, int width, int height) {
    effect->width = width;
    effect->height = height;
    effect->wall_angle = 0.0;
    effect->wall_cols = 11;
    effect->wall_rows = 8;
}

static void wall_brick(SsEffect *effect, Display *display, Drawable drawable,
                       GC gc, XPoint points[4]) {
    XSetForeground(display, gc, effect->primary);
    XFillPolygon(display, drawable, gc, points, 4, Convex, CoordModeOrigin);
    XSetForeground(display, gc, effect->background);
    XDrawLines(display, drawable, gc, points, 4, CoordModeOrigin);
    XDrawLine(display, drawable, gc, points[3].x, points[3].y,
              points[0].x, points[0].y);
}

static void wall_draw(SsEffect *effect, Display *display, Drawable drawable,
                      GC gc) {
    XSetForeground(display, gc, effect->background);
    XFillRectangle(display, drawable, gc, 0, 0,
                   (unsigned int)effect->width, (unsigned int)effect->height);

    effect->wall_angle += 0.006;
    double sin_a = sin(effect->wall_angle);
    double cos_a = cos(effect->wall_angle);

    double cx = effect->width / 2.0;
    double cy = effect->height / 2.0;

    double cell_w = (double)effect->width / effect->wall_cols;
    double cell_h = (double)effect->height / effect->wall_rows;
    double cell = cell_w < cell_h ? cell_w : cell_h;
    double brick_w = cell * 0.90;
    double brick_h = cell * 0.90;
    double fov = 700.0;
    double cam_z = fov + effect->wall_cols * cell;

    for (int row = 0; row < effect->wall_rows; row++) {
        for (int col = 0; col < effect->wall_cols; col++) {
            double wx = (col - (effect->wall_cols - 1) / 2.0) * cell;
            double wy = (row - (effect->wall_rows - 1) / 2.0) * cell;

            double lx[4] = { wx - brick_w / 2, wx + brick_w / 2,
                             wx + brick_w / 2, wx - brick_w / 2 };
            double ly[4] = { wy - brick_h / 2, wy - brick_h / 2,
                             wy + brick_h / 2, wy + brick_h / 2 };

            XPoint points[4];
            for (int k = 0; k < 4; k++) {
                double x = lx[k];
                double z = 0.0;
                double rx = x * cos_a - z * sin_a;
                double rz = x * sin_a + z * cos_a;
                double denom = cam_z + rz;
                if (denom < 1.0) denom = 1.0;
                double s = fov / denom;
                points[k].x = (short)(cx + rx * s);
                points[k].y = (short)(cy + ly[k] * s);
            }
            wall_brick(effect, display, drawable, gc, points);
        }
    }
}

/* --- the public entry points ---------------------------------------------- */

void ss_effect_init(SsEffect *effect, SsEffectKind kind,
                    int width, int height,
                    unsigned long primary, unsigned long background) {
    memset(effect, 0, sizeof(*effect));
    effect->primary = primary;
    effect->background = background;
    effect->colours_ready = 0;
    effect->eye = primary;

    if (kind == SS_EFFECT_3DWALL) {
        effect->draw = wall_draw;
        effect->reset = wall_reset;
        wall_reset(effect, width, height);
    } else {
        effect->draw = pipe_draw;
        effect->reset = pipe_reset;
        pipe_reset(effect, width, height);
    }
}

void ss_effect_free(SsEffect *effect) {
    (void)effect;   /* neither effect owns anything off the stack */
}
