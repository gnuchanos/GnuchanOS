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
 * The 3dwall is a MAZE WALKED IN THE FIRST PERSON. It is not a flat pattern of
 * squares: it is a grid of wall cells with corridors carved between them, and a
 * camera low in those corridors, and every screen column casts one ray into the
 * grid and draws the wall it hits at the height that wall's distance makes it.
 * That is what makes corridors open away from the camera, a side wall sweep past
 * it as the camera turns, and a far wall come nearer as the camera walks — a
 * view of a place, and not a tiling of a picture. The walker chooses its way at
 * each junction, preferring the way it is already going and never turning
 * straight back, so the camera wanders the maze for as long as the show runs.
 *
 * Everything is drawn with Xlib's own primitives — thick lines, filled discs
 * and filled rectangles — so nothing here needs a graphics stack, and the whole
 * saver starts on any server that can open a window at all.
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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

/* An HSV colour to RGB, each in 0..1. Hue is in degrees (0..360). Used to build
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

/* Build the worms' palette: a walk round the violet-to-magenta part of the
   colour wheel, and for each step three colours — the worm's body, a much
   darker version for its outline, and a much paler one for the shine down its
   back — plus the white of the eyes. Made once, on the first frame, when there
   is a display to allocate colours on. */
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

/* --- the maze -------------------------------------------------------------
 *
 * A maze carved on the odd cells of an all-wall grid. Every odd cell is a room;
 * two rooms are joined by knocking down the even cell between them, and the
 * carve is a depth-first walk, so the result is a maze with exactly one path
 * between any two rooms — the thing a person means by a maze. */

/* Whether a cell is inside the grid. */
static int maze_inside(int x, int y) {
    return x > 0 && y > 0 && x < SS_MAZE_WIDTH - 1 && y < SS_MAZE_HEIGHT - 1;
}

/* Carve the maze: fill the grid with walls, then walk from cell (1,1) knocking
   down the wall between the room it is in and a room two cells away it has not
   visited. Recursion would be the tidy way to write it and is not used: a maze
   is entered through a fixed grid and the depth here is bounded by the grid, so
   an explicit stack keeps the frame size the compiler's business and not the
   maze's. */
static void maze_carve(SsEffect *effect) {
    for (int y = 0; y < SS_MAZE_HEIGHT; y++) {
        for (int x = 0; x < SS_MAZE_WIDTH; x++) {
            effect->maze[y][x] = 1;
        }
    }

    int stack_x[SS_MAZE_WIDTH * SS_MAZE_HEIGHT];
    int stack_y[SS_MAZE_WIDTH * SS_MAZE_HEIGHT];
    int top = 0;

    effect->maze[1][1] = 0;
    stack_x[top] = 1;
    stack_y[top] = 1;
    top++;

    const int step[4][2] = { {2, 0}, {-2, 0}, {0, 2}, {0, -2} };

    while (top > 0) {
        int x = stack_x[top - 1];
        int y = stack_y[top - 1];

        /* The not-yet-visited neighbours two cells away, in a random order:
           shuffling the four directions and taking the first one that leads to
           an unvisited room is what makes the carve wander rather than march. */
        int order[4] = {0, 1, 2, 3};
        for (int i = 3; i > 0; i--) {
            int j = rand() % (i + 1);
            int t = order[i]; order[i] = order[j]; order[j] = t;
        }

        int carved = 0;
        for (int i = 0; i < 4; i++) {
            int nx = x + step[order[i]][0];
            int ny = y + step[order[i]][1];
            if (maze_inside(nx, ny) && effect->maze[ny][nx] == 1) {
                effect->maze[ny][nx] = 0;
                effect->maze[(y + ny) / 2][(x + nx) / 2] = 0;
                stack_x[top] = nx;
                stack_y[top] = ny;
                top++;
                carved = 1;
                break;
            }
        }
        if (!carved) {
            top--;
        }
    }
}

/* Choose the next cell to walk to from the one the camera is in. Every open
   neighbour is a candidate except the one just left — a walker that turned
   straight back at every junction would rock in place — and among the rest the
   way it is already going and then a random one are preferred, so the camera
   tends to keep to a corridor and still turns at a corner into a dead end. */
static void maze_choose_next(SsEffect *effect) {
    int cx = effect->cell_x;
    int cy = effect->cell_y;
    const int step[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    int open_x[4];
    int open_y[4];
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int nx = cx + step[i][0];
        int ny = cy + step[i][1];
        if (!maze_inside(nx, ny) || effect->maze[ny][nx]) {
            continue;
        }
        if (effect->has_prev && nx == effect->prev_x && ny == effect->prev_y &&
            count >= 1) {
            /* Not straight back, unless it is the only way — handled below by
               letting a lone candidate through. */
            continue;
        }
        open_x[count] = nx;
        open_y[count] = ny;
        count++;
    }
    if (count == 0) {
        /* Only the way back: take it. */
        for (int i = 0; i < 4; i++) {
            int nx = cx + step[i][0];
            int ny = cy + step[i][1];
            if (maze_inside(nx, ny) && !effect->maze[ny][nx]) {
                open_x[0] = nx;
                open_y[0] = ny;
                count = 1;
                break;
            }
        }
    }
    if (count == 0) {
        return;
    }

    /* Prefer to keep going the way that lines up with the current facing, so a
       long corridor is walked as a corridor and the camera does not weave. */
    int best = 0;
    double best_dot = -2.0;
    double fx = cos(effect->cam_angle);
    double fy = sin(effect->cam_angle);
    for (int i = 0; i < count; i++) {
        double dx = (open_x[i] + 0.5) - effect->cam_x;
        double dy = (open_y[i] + 0.5) - effect->cam_y;
        double len = sqrt(dx * dx + dy * dy);
        if (len < 1e-6) {
            continue;
        }
        double dot = (dx / len) * fx + (dy / len) * fy;
        if (dot > best_dot) {
            best_dot = dot;
            best = i;
        }
    }
    /* A coin toss at a junction, so the walk does not always take the straight
       way and the maze is explored rather than beelined. */
    if (count > 1 && (rand() % 100) < 35) {
        best = rand() % count;
    }

    effect->prev_x = effect->cell_x;
    effect->prev_y = effect->cell_y;
    effect->has_prev = 1;
    effect->target_x = open_x[best];
    effect->target_y = open_y[best];
}

/* Build the maze's palette from the two session colours. A wall's face is the
   primary colour shaded down towards the background as it gets further away, so
   distance reads as the corridor darkening into the fog; the ceiling and the
   floor are the background a shade pulled towards the wall. Made once, on the
   first frame, when there is a display to allocate colours on. */
static void prepare_maze(SsEffect *effect, Display *display) {
    if (effect->maze_ready) {
        return;
    }
    /* The two wall orientations are TWO DIFFERENT PURPLES and not one shade
     * apart, because that is what a person reads the corridors by: a wall you
     * walk up to (seen square, one hue) and the wall of a corridor running
     * away to the side (seen edge on, another hue) have to tell apart at a
     * glance, or the maze is a flat wall of colour sliding past. On top of
     * each the distance is run from bright (near) down to the background
     * (far), so a corridor darkens into the distance and the walk reads as
     * movement.
     *
     * The ramps are built in HSV and not by mixing towards the background: a
     * mix towards a dark background turns every purple into the same muddy
     * near-black, which is the flatness this replaces. */
    for (int shade = 0; shade < SS_MAZE_SHADES; shade++) {
        double fade = 1.0 - (double)shade / (double)(SS_MAZE_SHADES - 1);

        /* A wall faced square: a bright violet. */
        double face[3];
        hsv_to_rgb(282.0, 0.62 + 0.20 * fade, 0.30 + 0.62 * fade, face);
        unsigned long face_pixel = alloc_rgb(display, face[0], face[1], face[2]);
        effect->maze_wall[0][shade] = face_pixel ? face_pixel : effect->primary;

        /* A wall seen edge on: a deeper, bluer purple, so a side corridor and
         * a wall ahead are never mistaken for each other. */
        double side[3];
        hsv_to_rgb(256.0, 0.70 + 0.18 * fade, 0.20 + 0.48 * fade, side);
        unsigned long side_pixel = alloc_rgb(display, side[0], side[1], side[2]);
        effect->maze_wall[1][shade] = side_pixel ? side_pixel : effect->primary;
    }

    /* The bright line where two faces meet — a corner of a corridor. Without
     * it a turn is only a change of hue; with it the corner is a drawn edge,
     * which is what makes the shape of the maze read. */
    {
        double edge[3];
        hsv_to_rgb(292.0, 0.55, 0.96, edge);
        unsigned long e = alloc_rgb(display, edge[0], edge[1], edge[2]);
        effect->maze_edge = e ? e : effect->primary;
    }

    /* The floor and the ceiling: pulls of the wall hue, the floor a touch
     * brighter than the ceiling so the two bands of the corridor tell apart. */
    {
        double floor[3];
        hsv_to_rgb(280.0, 0.60, 0.24, floor);
        unsigned long f = alloc_rgb(display, floor[0], floor[1], floor[2]);
        effect->maze_floor = f ? f : effect->background;

        double ceiling[3];
        hsv_to_rgb(280.0, 0.60, 0.15, ceiling);
        unsigned long c = alloc_rgb(display, ceiling[0], ceiling[1], ceiling[2]);
        effect->maze_ceiling = c ? c : effect->background;
    }

    effect->maze_ready = 1;
}

static void maze_reset(SsEffect *effect, int width, int height) {
    seed_once();
    effect->width = width;
    effect->height = height;

    maze_carve(effect);

    effect->cell_x = 1;
    effect->cell_y = 1;
    effect->cam_x = 1.5;
    effect->cam_y = 1.5;
    effect->has_prev = 0;
    effect->cam_angle = ((double)(rand() % 360)) * M_PI / 180.0;
    effect->target_x = 1;
    effect->target_y = 1;
    maze_choose_next(effect);
    /* The facing is set from the walk's first choice, so the camera never opens
       looking at a wall it then has to turn from. */
    effect->cam_angle = atan2((effect->target_y + 0.5) - effect->cam_y,
                              (effect->target_x + 0.5) - effect->cam_x);
}

/* Walk the camera a step towards the middle of the cell it is heading for, and
   choose again when it arrives. The facing turns smoothly towards the way it is
   going, so a corner is a turn and not a snap. */
static void maze_advance(SsEffect *effect) {
    double tx = effect->target_x + 0.5;
    double ty = effect->target_y + 0.5;
    double dx = tx - effect->cam_x;
    double dy = ty - effect->cam_y;
    double len = sqrt(dx * dx + dy * dy);

    const double speed = 0.021;   /* cells per frame */

    if (len <= speed) {
        effect->cam_x = tx;
        effect->cam_y = ty;
        effect->cell_x = effect->target_x;
        effect->cell_y = effect->target_y;
        maze_choose_next(effect);
        tx = effect->target_x + 0.5;
        ty = effect->target_y + 0.5;
        dx = tx - effect->cam_x;
        dy = ty - effect->cam_y;
        len = sqrt(dx * dx + dy * dy);
    }
    if (len > 1e-6) {
        effect->cam_x += (dx / len) * speed;
        effect->cam_y += (dy / len) * speed;
    }

    /* Ease the facing towards the direction of travel, the short way round. */
    double want = atan2(dy, dx);
    double diff = want - effect->cam_angle;
    while (diff > M_PI)  diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    effect->cam_angle += diff * 0.14;
}

/* What a ray from the camera into the grid hits: the distance along the ray to
   the wall and which of the two wall orientations it was, so the drawing can
   shade the two differently. See maze_draw for how a column's ray is cast. */
typedef struct MazeHit {
    double distance;
    int side;      /* 0: a wall facing along X, 1: along Y */
} MazeHit;

/* Cast one ray. A plain DDA walk over the grid: step to the next cell boundary
   each time, in whichever axis is nearer, until a wall cell is entered. That is
   the same walk a raycaster has always used — a handful of steps a column, so a
   whole frame is a few thousand adds and multiplies and no trigonometry past
   the column's own angle. */
static int maze_cast(SsEffect *effect, double origin_x, double origin_y,
                     double dir_x, double dir_y, MazeHit *hit) {
    int map_x = (int)origin_x;
    int map_y = (int)origin_y;
    if (map_x < 0 || map_y < 0 || map_x >= SS_MAZE_WIDTH ||
        map_y >= SS_MAZE_HEIGHT) {
        return 0;
    }

    double delta_x = dir_x == 0.0 ? 1e30 : fabs(1.0 / dir_x);
    double delta_y = dir_y == 0.0 ? 1e30 : fabs(1.0 / dir_y);

    int step_x;
    int step_y;
    double side_x;
    double side_y;

    if (dir_x < 0.0) {
        step_x = -1;
        side_x = (origin_x - map_x) * delta_x;
    } else {
        step_x = 1;
        side_x = (map_x + 1.0 - origin_x) * delta_x;
    }
    if (dir_y < 0.0) {
        step_y = -1;
        side_y = (origin_y - map_y) * delta_y;
    } else {
        step_y = 1;
        side_y = (map_y + 1.0 - origin_y) * delta_y;
    }

    int side = 0;
    for (int guard = 0; guard < SS_MAZE_WIDTH * SS_MAZE_HEIGHT; guard++) {
        if (side_x < side_y) {
            side_x += delta_x;
            map_x += step_x;
            side = 0;
        } else {
            side_y += delta_y;
            map_y += step_y;
            side = 1;
        }
        if (map_x < 0 || map_y < 0 || map_x >= SS_MAZE_WIDTH ||
            map_y >= SS_MAZE_HEIGHT) {
            return 0;
        }
        if (effect->maze[map_y][map_x]) {
            hit->distance = (side == 0) ? (side_x - delta_x)
                                        : (side_y - delta_y);
            if (hit->distance < 0.02) {
                hit->distance = 0.02;
            }
            hit->side = side;
            return 1;
        }
    }
    return 0;
}

static void maze_draw(SsEffect *effect, Display *display, Drawable drawable,
                      GC gc) {
    prepare_maze(effect, display);

    /* One step of the walk BEFORE the frame is cast, so the view is cast from
       where the walker has just arrived and not from where it was a frame ago. */
    maze_advance(effect);

    const int width = effect->width;
    const int height = effect->height;

    /* The ceiling and the floor, split by the horizon. The wall columns are
       drawn over them from the horizon outwards, so a corridor's opening is a
       gap between the two bands. */
    int horizon = height / 2;
    XSetForeground(display, gc, effect->maze_ceiling);
    XFillRectangle(display, drawable, gc, 0, 0, (unsigned int)width,
                   (unsigned int)horizon);
    XSetForeground(display, gc, effect->maze_floor);
    XFillRectangle(display, drawable, gc, 0, (unsigned int)horizon,
                   (unsigned int)width, (unsigned int)(height - horizon));

    /* `scale` turns "distance in cells" into "height in pixels". A wall one
       cell away comes out a little under the screen height, which leaves the
       ceiling and the floor showing above and below it — so a corridor reads as
       a corridor with an open top and bottom, and not as a slab of colour that
       fills the screen and makes the walker feel he is inside the wall. */
    double scale = (double)height * 0.90;
    double half_fov = 33.0 * M_PI / 180.0;   /* a 66-degree view */

    /* The face and the distance the last column hit, so a column can tell when
       it has crossed from one wall face to another. The jump is what marks a
       corner of the corridor, and a corner drawn is a corner seen. */
    double previous_distance = -1.0;
    int previous_side = -1;

    for (int col = 0; col < width; col++) {
        double t = ((double)col + 0.5) / (double)width;   /* 0..1 across */
        double angle = effect->cam_angle + (t - 0.5) * 2.0 * half_fov;
        double dir_x = cos(angle);
        double dir_y = sin(angle);

        MazeHit hit;
        if (!maze_cast(effect, effect->cam_x, effect->cam_y,
                       dir_x, dir_y, &hit)) {
            continue;
        }

        /* Fisheye correction: a wall dead ahead and one far to the side are the
           same distance and must be the same height, so the distance is
           measured along the camera's facing and not along the ray. */
        double corrected = hit.distance *
                           cos(angle - effect->cam_angle);
        if (corrected < 0.02) {
            corrected = 0.02;
        }

        double wall_h = scale / corrected;
        int top = (int)(horizon - wall_h * 0.5);
        int bottom = (int)(horizon + wall_h * 0.5);
        if (top < 0) {
            top = 0;
        }
        if (bottom > height) {
            bottom = height;
        }
        if (bottom <= top) {
            continue;
        }

        /* Shade by distance: shade 0 is the bright, near end of the ramp and
           the last shade is the background at the far end, so a wall you are
           close to is bright and one down the corridor has faded. Getting this
           the other way round — bright far, dark near — is what made the maze
           read as flat colour rather than as a place to walk through. */
        double fog = hit.distance / ((double)SS_MAZE_WIDTH * 0.6);
        if (fog > 1.0) {
            fog = 1.0;
        }
        int shade = (int)(fog * (SS_MAZE_SHADES - 1));
        if (shade < 0) {
            shade = 0;
        }
        if (shade >= SS_MAZE_SHADES) {
            shade = SS_MAZE_SHADES - 1;
        }
        XSetForeground(display, gc, effect->maze_wall[hit.side][shade]);
        XFillRectangle(display, drawable, gc, col, top, 1,
                       (unsigned int)(bottom - top));

        /* A corner: this column's wall is a different face from the last one's,
           either because the distance jumped or because the wall turned a
           quarter. A bright line drawn there is the edge of the corridor, and
           it is what lets the eye follow the maze instead of a wash of purple. */
        if (previous_distance < 0.0 ||
            fabs(corrected - previous_distance) > 0.12 ||
            hit.side != previous_side) {
            XSetForeground(display, gc, effect->maze_edge);
            XFillRectangle(display, drawable, gc, col, top, 1,
                           (unsigned int)(bottom - top));
        }
        previous_distance = corrected;
        previous_side = hit.side;
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
    effect->maze_ready = 0;
    effect->eye = primary;

    if (kind == SS_EFFECT_3DWALL) {
        effect->draw = maze_draw;
        effect->reset = maze_reset;
        maze_reset(effect, width, height);
    } else {
        effect->draw = pipe_draw;
        effect->reset = pipe_reset;
        pipe_reset(effect, width, height);
    }
}

void ss_effect_free(SsEffect *effect) {
    (void)effect;   /* neither effect owns anything off the stack */
}
