# ============================================================================
#  GnuchanOS  .  Embed demo  .  PYTHON court  (right window)
# ----------------------------------------------------------------------------
#  A real Python 3.14 program drawn with Raylib (PyRaylib).
#
#  main.gcsf is the referee.  It owns ONE shared playfield of 1600 x 600 and
#  it owns the ball.  This window owns the right half of that playfield:
#
#      virtual x =  800 .. 1600  ->  this window  (drawn at x - 800)
#      virtual x =    0 ..  800  ->  the far half (drawn at x over there)
#
#  Every frame the referee publishes the ball and this window's score, and
#  this window answers with the one thing it owns, its paddle:
#
#      gcl.get_state("ball_x")            # the ball of the shared playfield
#      gcl.get_state("ball_vx")
#      gcl.get_state("score")             # the score this window plays for
#      gcl.set_state("paddle_y", paddle_y)
#
#  Nothing else crosses.  This window knows the ball, its own score and its
#  own paddle; it never learns anything about the far court beyond the ball
#  that flies away across the seam.
#
#  It is the same program as the Lua window, mirrored: the paddle guards the
#  right wall, the seam sits on the left edge, and every x is flipped.
# ============================================================================

import math

import gcl
import raylib

gcl.init()

# ---------------------------------------------------------------------------
#  Constants -- every one of these must match main.gcsf
# ---------------------------------------------------------------------------
W = 800                    # this window
H = 600                    # the shared playfield height
COURT_LO = 800             # virtual x where this court begins
SEAM_X = 800               # virtual x of the seam, this window's left edge
PADDLE_W = 18
PADDLE_X = W - 36 - PADDLE_W   # this paddle guards the RIGHT wall (x 1600)
PADDLE_H = 96
BALL_R = 10
AI_SPEED = 470             # px/s the paddle can travel; finite, so it can lose
AI_JITTER = 14             # px of wander, so it is not a perfect machine
AI_JITTER_HZ = 1.7
TRAIL = 24                 # how many ball ghosts to keep
WIN_SCORE = 7

HUD_H = 54                 # the two HUD strips
BALL_ALIVE = 1.0           # below this speed the referee is holding the ball

# ---------------------------------------------------------------------------
#  Shared store -- the only channel between this window and the referee
# ---------------------------------------------------------------------------
def get(key, fallback):
    value = gcl.get_state(key)
    if value is None:
        return fallback
    return float(value)


def get_int(key, fallback):
    return int(get(key, fallback))


def put(key, value):
    gcl.set_state(key, float(value))


def clamp(value, lo, hi):
    if value < lo:
        return lo
    if value > hi:
        return hi
    return value


# Where will the ball cross the line x = target_x?  The two long walls are
# folded in, so the paddle can start running to the right spot long before
# the ball gets there -- that is what makes it look like a player instead of
# something that only wakes up when the ball arrives.
def predict_y(ball_x, ball_y, ball_vx, ball_vy, target_x):
    if ball_vx == 0:
        return ball_y

    travel = (target_x - ball_x) / ball_vx
    if travel <= 0:
        return ball_y

    span = H - 2 * BALL_R                    # the strip the centre may use
    u = (ball_y - BALL_R) + ball_vy * travel
    u = u % (2 * span)                       # fold into one strip
    if u > span:
        u = 2 * span - u                     # ... and mirror the rest
    return u + BALL_R


# ---------------------------------------------------------------------------
#  Palette -- raylib's own colours, with alpha where it helps
#
#  raylib.GetColor(0xRRGGBBAA) builds a full RGBA colour from a hex literal
#  (R is the high byte, A the low byte), and raylib.Fade(c, a) or
#  raylib.ColorAlpha(c, a) set the alpha channel of a colour that already is.
# ---------------------------------------------------------------------------
deep    = raylib.Color(  0,   0,   0, 255)   # outside the court
court   = raylib.Color( 55, 118, 171, 255)   # Python blue
grid    = raylib.Color(255, 212,  59,  26)   # Python yellow, transparent
panel   = raylib.Color( 20,  45,  70, 255)   # dark Python blue
edge    = raylib.Color( 55, 118, 171, 255)   # Python blue
accent  = raylib.Color(255, 212,  59, 255)   # Python yellow
dim     = raylib.Color(120, 155, 180, 255)
white   = raylib.Color(240, 245, 245, 255)
seam    = raylib.Color(255, 212,  59, 255)   # Python yellow
goal    = raylib.Color(255,  85,  85, 255)
goaloff = raylib.Color(102,   0,   0, 255)
ballhi  = raylib.Color(255, 255, 255, 255)   # the moon
balllo  = raylib.Color(170, 195, 215, 255)

# ---------------------------------------------------------------------------
#  Window
# ---------------------------------------------------------------------------
raylib.InitWindow(W, H, "PYTHON court  |  Python 3.14 + PyRaylib  |  GnuchanOS Embed demo")
raylib.SetTargetFPS(60)
raylib.SetWindowPosition(880, 140)

put("paddle_y", H / 2)
put("ready", 1)          # the referee waits for BOTH courts before it starts
put("quit", 0)

paddle_y = float(H) / 2.0
hits = 0                 # how many times this paddle returned the ball
prev_vx = 0              # the ball's vx on the previous frame
trail = []               # recent ball positions, newest first

# ---------------------------------------------------------------------------
#  Helpers that draw one piece of the court
# ---------------------------------------------------------------------------
def draw_court(ball_x, t):
    raylib.DrawRectangle(0, 0, W, H, court)

    gx = 80
    while gx < W:
        raylib.DrawRectangle(gx, HUD_H, 1, H - 2 * HUD_H, grid)
        gx += 80
    gy = 80
    while gy < H - HUD_H:
        raylib.DrawRectangle(0, gy, W, 1, grid)
        gy += 80
    gy = 60
    while gy < H - 60:
        raylib.DrawRectangle(W // 2, gy, 2, 18, grid)
        gy += 40

    # the seam: this window's left edge is where the ball leaves
    near = 1.0 - min(1.0, abs(SEAM_X - ball_x) / 420.0)
    pulse = 0.5 + 0.5 * math.sin(t * 6.0)
    raylib.DrawRectangle(0, HUD_H, 14, H - 2 * HUD_H, raylib.Fade(seam, 0.10 + 0.45 * near))
    raylib.DrawRectangle(0, HUD_H, 3, H - 2 * HUD_H, raylib.Fade(seam, 0.35 + 0.5 * near))

    # raylib fills a triangle only when its vertices are wound the same way
    # the other court winds them; mirroring x flips that winding, so the last
    # two vertices are swapped back and the arrows actually show up.
    ci = 1
    while ci <= 3:
        alpha = 0.18 + 0.55 * pulse
        x0 = 16 + ci * 16
        raylib.DrawTriangle(x0, H / 2 - 16, x0 - 12, H / 2, x0, H / 2 + 16,
                            raylib.Fade(seam, alpha))
        ci += 1

    # my goal line, on the right wall
    raylib.DrawRectangle(W - 6, HUD_H, 6, H - 2 * HUD_H, goaloff)
    raylib.DrawRectangle(W - 2, HUD_H, 2, H - 2 * HUD_H, goal)


def draw_trail():
    for i, (tx, ty) in enumerate(trail):
        age = i / float(TRAIL)
        raylib.DrawCircleV(tx, ty, BALL_R * (1.0 - age * 0.7),
                           raylib.Fade(ballhi, (1.0 - age) * 0.40))


def draw_paddle():
    top = paddle_y - PADDLE_H / 2
    raylib.DrawRectangleRounded(PADDLE_X, top, PADDLE_W, PADDLE_H,
                                0.5, 8, raylib.Fade(accent, 0.22))
    raylib.DrawRectangleRounded(PADDLE_X + 2, top + 2, PADDLE_W - 4, PADDLE_H - 4,
                                0.5, 8, accent)
    raylib.DrawRectangle(PADDLE_X + PADDLE_W - 7, int(top + 8),
                         3, PADDLE_H - 16, white)


# The ball while it is inside this window, otherwise a marker on the seam.
def draw_ball(ball_x, ball_y):
    local_x = ball_x - COURT_LO
    if local_x > -40 and local_x < W + 40:
        raylib.DrawCircleV(local_x, ball_y, BALL_R + 11, raylib.Fade(ballhi, 0.12))
        raylib.DrawCircleV(local_x, ball_y, BALL_R + 6, raylib.Fade(ballhi, 0.28))
        raylib.DrawCircleV(local_x, ball_y, BALL_R, ballhi)
        raylib.DrawCircleV(local_x - 3, ball_y - 3, BALL_R - 4, balllo)
        raylib.DrawCircleV(local_x + 3, ball_y + 3, BALL_R - 7, white)
    else:
        # same winding note as the seam arrows above: the mirrored triangle
        # must be wound the way raylib fills, not the way it culls
        marker_y = clamp(ball_y, 70, H - 70)
        raylib.DrawTriangle(22, marker_y - 12, 6, marker_y, 22, marker_y + 12, raylib.Fade(ballhi, 0.75))


def draw_hud(score, ball_vx, ball_vy, serve):
    # top strip
    raylib.DrawRectangle(0, 0, W, HUD_H, panel)
    raylib.DrawRectangle(0, HUD_H, W, 2, edge)
    raylib.DrawCircle(30, 27, 17, accent)
    raylib.DrawCircle(38, 21, 17, panel)
    raylib.DrawText("PYTHON", 56, 8, 30, accent)
    raylib.DrawText("Python 3.14  +  PyRaylib", 58, 38, 12, dim)
    raylib.DrawText("SCORE", 548, 10, 20, dim)
    raylib.DrawText("%d / %d" % (score, WIN_SCORE), 548 + raylib.MeasureText("SCORE", 20) + 14, 10, 22, white)
    raylib.DrawText("%d FPS" % raylib.GetFPS(), 716, 10, 13, dim)

    # bottom strip
    raylib.DrawRectangle(0, H - HUD_H, W, HUD_H, panel)
    raylib.DrawRectangle(0, H - HUD_H, W, 2, edge)
    speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    raylib.DrawText("returns %d" % hits, 16, H - 42, 14, dim)
    raylib.DrawText("ball speed %.0f px/s" % speed, 16, H - 24, 14, dim)
    if serve:
        raylib.DrawText("ball is being served", 320, H - 42, 15, accent)
    elif ball_vx > 0:
        raylib.DrawText("the ball is coming at me", 320, H - 42, 15, accent)
    else:
        raylib.DrawText("the ball is leaving my court", 320, H - 42, 15, seam)
    raylib.DrawText("ESC / close window to quit", 560, H - 30, 13, dim)


def draw_banner(score, serve):
    if score >= WIN_SCORE:
        raylib.DrawText("PYTHON WINS THE MATCH", 178, 268, 30, accent)
        raylib.DrawText("a new match is starting...", 300, 306, 16, dim)
    elif serve:
        raylib.DrawText("GET READY", 306, 278, 30, white)


# ---------------------------------------------------------------------------
#  Main loop -- the window owns its own frame clock
# ---------------------------------------------------------------------------
while not raylib.WindowShouldClose():
    t = raylib.GetTime()
    dt = raylib.GetFrameTime()
    if dt > 0.05:
        dt = 0.05

    # ---- 1. what the referee just published ------------------------------
    if get("running", 1) < 0.5:
        break

    ball_x = get("ball_x", COURT_LO + W / 2)
    ball_y = get("ball_y", H / 2)
    ball_vx = get("ball_vx", 0)
    ball_vy = get("ball_vy", 0)
    score = get_int("score", 0)

    speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    serve = speed < BALL_ALIVE       # the referee is holding the ball

    # ---- 2. my paddle ----------------------------------------------------
    # The ball comes at me while it travels RIGHT (vx > 0).  Then I run to
    # where it will arrive; while it is leaving I walk back to a ready spot.
    # I move EVERY frame, so I look alive even when the ball is far away.
    aim = float(H) / 2.0
    if ball_vx > 0:
        aim = predict_y(ball_x, ball_y, ball_vx, ball_vy, PADDLE_X)
    aim = aim + math.sin(t * AI_JITTER_HZ) * AI_JITTER

    step = AI_SPEED * dt
    paddle_y = paddle_y + clamp(aim - paddle_y, -step, step)
    paddle_y = clamp(paddle_y, PADDLE_H / 2, H - PADDLE_H / 2)

    put("paddle_y", paddle_y)

    # a return I made: the ball turned around right in front of my paddle
    if prev_vx > 0 and ball_vx < 0:
        hits += 1
    prev_vx = ball_vx

    # ---- 3. keep the last few ball positions for the trail ---------------
    trail.insert(0, (ball_x - COURT_LO, ball_y))
    if len(trail) > TRAIL:
        trail.pop()

    # ---- 4. draw ---------------------------------------------------------
    raylib.BeginDrawing()
    raylib.ClearBackground(deep)
    draw_court(ball_x, t)
    draw_trail()
    draw_paddle()
    draw_ball(ball_x, ball_y)
    draw_hud(score, ball_vx, ball_vy, serve)
    draw_banner(score, serve)
    raylib.EndDrawing()

    if raylib.IsKeyPressed(raylib.KEY_ESCAPE):
        break

# Leaving the loop means this court is done: tell the referee, then close.
put("quit", 1)
raylib.CloseWindow()
