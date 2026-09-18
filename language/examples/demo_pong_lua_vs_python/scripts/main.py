# ============================================================================
#  GnuchanOS  .  Embed demo  .  PYTHON court  (right window)
# ----------------------------------------------------------------------------
#  This window is a real Python 3.14 program drawn with Raylib (PyRaylib).
#  It owns the RIGHT court and the RIGHT AI paddle.
#
#  The ball itself is NOT simulated here.  main.gcsf (the GCL host) is the
#  referee: it keeps the single ball for a virtual 1600x600 playfield that is
#  split down the middle into two courts --
#
#        virtual x =    0 .. 800  ->  the Lua window   (LUA, navy + yellow)
#        virtual x =  800 .. 1600 ->  this window      (PYTHON, blue + gold)
#
#  and it streams the ball position into the Embed shared state every frame.
#  This file reads it and answers with its own paddle position:
#
#        ball_y = gcl.get_state("ball_y")            # read the ball
#        gcl.set_state("py_paddle_y", paddle_y)      # report my paddle
#
#  Because the two courts are two halves of ONE playfield, the ball visibly
#  travels out of one window and into the other: it walks off the left edge of
#  this window and appears at the right edge of the Lua window.
# ============================================================================

import math
from collections import deque

import gcl
import raylib

gcl.init()

# ---------------------------------------------------------------------------
#  Tunables (must match main.gcsf)
# ---------------------------------------------------------------------------
W = 800                    # this window is 800x600
H = 600
FIELD_W = 1600             # the virtual playfield shared by both courts
COURT_LO = 800             # this window shows virtual x in [800, 1600)
SEAM_VX = 800              # the boundary the ball crosses
PADDLE_X = W - 36 - 18     # this court's paddle defends the RIGHT wall
PADDLE_W = 18
PADDLE_H = 96
BALL_R = 10
AI_SPEED = 430             # px/s the AI paddle can travel
TRAIL_MAX = 26
WIN_SCORE = 7

# ---------------------------------------------------------------------------
#  Shared-state bridge
# ---------------------------------------------------------------------------
#  gcl.get_state returns a float, or None when the key is not published yet.
def state(key, fallback):
    value = gcl.get_state(key)
    if value is None:
        return fallback
    return float(value)


def state_int(key, fallback):
    return int(state(key, fallback))


def publish(key, value):
    gcl.set_state(key, float(value))


# ---------------------------------------------------------------------------
#  Palette -- the Python identity: Python blue, Python yellow, two snakes
# ---------------------------------------------------------------------------
C = {
    "deep":    raylib.GetColor(0x0A1622FF),   # outside the court
    "court":   raylib.GetColor(0x12293EFF),   # the playfield
    "grid":    raylib.GetColor(0x1B3F5EFF),   # service lines
    "panel":   raylib.GetColor(0x152F49FF),   # HUD strips
    "edge":    raylib.GetColor(0x2A557EFF),   # panel borders
    "blue":    raylib.GetColor(0x306998FF),   # python blue
    "yellow":  raylib.GetColor(0xFFD43BFF),   # python yellow
    "dim":     raylib.GetColor(0x7FA0C0FF),
    "white":   raylib.GetColor(0xF2F7FDFF),
    "seam":    raylib.GetColor(0xFFD43BFF),   # the portal line between courts
    "goal":    raylib.GetColor(0xFF6B6BFF),
    "goaloff": raylib.GetColor(0x3A2A44FF),
}

# The ball looks the SAME in both windows, so it reads as one object that
# crosses the boundary rather than two balls on two screens.
BALL_CORE = raylib.GetColor(0xFFF3C4FF)
BALL_HALO = raylib.GetColor(0xFFC93CFF)

# ---------------------------------------------------------------------------
#  Window
# ---------------------------------------------------------------------------
raylib.InitWindow(W, H, "PYTHON court  |  Python 3.14 + PyRaylib  |  GnuchanOS Embed demo")
raylib.SetTargetFPS(60)
raylib.SetWindowPosition(880, 140)

# Tell the referee this court is alive.  main.gcsf waits for BOTH courts
# before it starts the clock.
publish("py_paddle_y", H / 2)
publish("py_ready", 1)
publish("quit", 0)

paddle_y = float(H) / 2.0        # centre of my paddle (virtual y)
trail = deque(maxlen=TRAIL_MAX)  # recent ball positions, in this court's coords
hits = 0                         # how many times this court returned the ball
prev_rally = -1


# ---------------------------------------------------------------------------
#  AI -- this is the "Python AI paddle".  Deliberately beatable: it only
#  tracks while the ball is inside its own court, drifts home otherwise, and
#  its maximum speed is finite, so a fast enough ball gets past it.
# ---------------------------------------------------------------------------
def update_ai(dt, ball_x, ball_y, t):
    global paddle_y

    target = float(H) / 2.0                     # rest position
    if ball_x >= SEAM_VX:
        # the ball is in my half: shadow it, with a slow sway so the rally
        # is not a perfect mirror each time
        target = ball_y + math.sin(t * 2.1) * 22.0

    dy = target - paddle_y
    step = AI_SPEED * dt
    if dy > step:
        dy = step
    elif dy < -step:
        dy = -step
    paddle_y += dy

    half = PADDLE_H / 2.0
    paddle_y = max(half, min(H - half, paddle_y))


# ---------------------------------------------------------------------------
#  Drawing helpers
# ---------------------------------------------------------------------------
def draw_court():
    raylib.DrawRectangle(0, 0, W, H, C["court"])

    # service grid
    for x in range(80, W, 80):
        raylib.DrawRectangle(x, 54, 1, H - 108, C["grid"])
    for y in range(80, H, 80):
        raylib.DrawRectangle(0, y, W, 1, C["grid"])

    # the centre service line of this court, dashed
    y = 60
    while y < H - 60:
        raylib.DrawRectangle(W // 2, y, 2, 18, C["grid"])
        y += 40


def draw_seam(ball_x, t):
    # The seam: the left edge of this window is the boundary with the Lua
    # court.  It pulses when the ball is close, so you can see it arriving.
    near = 1.0 - min(1.0, abs(SEAM_VX - ball_x) / 420.0)
    pulse = 0.5 + 0.5 * math.sin(t * 6.0)

    raylib.DrawRectangle(0, 54, 14, H - 108, raylib.Fade(C["seam"], 0.10 + 0.45 * near))
    raylib.DrawRectangle(0, 54, 3, H - 108, raylib.Fade(C["seam"], 0.35 + 0.5 * near))

    # chevrons pointing back into the Lua court
    cy = 300
    for i in range(1, 4):
        a = 0.18 + 0.55 * near * pulse
        x0 = 16 + i * 16
        raylib.DrawTriangle(x0, cy - 16, x0, cy + 16, x0 - 12, cy,
                            raylib.Fade(C["seam"], a))


def draw_goal():
    # this court's own goal line (the right wall of the virtual playfield)
    raylib.DrawRectangle(W - 6, 54, 6, H - 108, C["goaloff"])
    raylib.DrawRectangle(W - 2, 54, 2, H - 108, C["goal"])


def draw_paddle(py):
    raylib.DrawRectangleRounded(PADDLE_X, py - PADDLE_H / 2.0, PADDLE_W, PADDLE_H,
                                0.5, 8, raylib.Fade(C["yellow"], 0.22))
    raylib.DrawRectangleRounded(PADDLE_X + 2, py - PADDLE_H / 2.0 + 2,
                                PADDLE_W - 4, PADDLE_H - 4,
                                0.5, 8, C["yellow"])
    raylib.DrawRectangle(PADDLE_X + PADDLE_W - 7, int(py - PADDLE_H / 2.0 + 8),
                         3, PADDLE_H - 16, C["white"])


def draw_trail():
    for i, (px, py) in enumerate(reversed(trail)):
        age = i / float(TRAIL_MAX)
        a = (1.0 - age) * 0.42
        raylib.DrawCircleV(px, py, BALL_R * (1.0 - age * 0.7),
                           raylib.Fade(BALL_HALO, a))


def draw_ball(ball_x, ball_y):
    local_x = ball_x - COURT_LO

    if local_x < -40 or local_x > W + 40:
        # the ball is in the other court: show a marker on the seam so the
        # viewer knows it is coming back
        cy = max(70.0, min(float(H) - 70.0, ball_y))
        raylib.DrawTriangle(22, cy - 12, 22, cy + 12, 6, cy,
                            raylib.Fade(BALL_HALO, 0.75))
        raylib.DrawCircleLines(40, int(cy), BALL_R, raylib.Fade(BALL_HALO, 0.5))
        return

    raylib.DrawCircleV(local_x, ball_y, BALL_R + 11, raylib.Fade(BALL_HALO, 0.12))
    raylib.DrawCircleV(local_x, ball_y, BALL_R + 6, raylib.Fade(BALL_HALO, 0.28))
    raylib.DrawCircleV(local_x, ball_y, BALL_R, BALL_HALO)
    raylib.DrawCircleV(local_x - 3, ball_y - 3, BALL_R - 4, BALL_CORE)
    raylib.DrawCircleV(local_x + 3, ball_y + 3, BALL_R - 7, C["white"])


def draw_python_emblem(cx, cy):
    # The two-snake mark: a blue snake over a yellow snake, each with an eye.
    size = 34.0
    raylib.DrawRectangleRounded(cx - size / 2.0, cy - size / 2.0,
                                size, size / 2.0, 0.45, 8, C["blue"])
    raylib.DrawRectangleRounded(cx - size / 2.0, cy,
                                size, size / 2.0, 0.45, 8, C["yellow"])
    raylib.DrawCircleV(cx - 8, cy - 8, 3.0, C["yellow"])
    raylib.DrawCircleV(cx + 8, cy + 8, 3.0, C["blue"])


def draw_hud(ball_x, ball_y, ball_vx, ball_vy, rally, ls, ps, winner, serve_wait):
    # ---- top strip --------------------------------------------------------
    raylib.DrawRectangle(0, 0, W, 54, C["panel"])
    raylib.DrawRectangle(0, 54, W, 2, C["edge"])

    # ---- match score ------------------------------------------------------
    # Only THIS court is named in this window: the shared match score is shown
    # as bare numbers, so the Python window never has to spell the other
    # language out.
    raylib.DrawText("SCORE", 20, 10, 20, C["dim"])
    score_text = "%d : %d" % (ls, ps)
    sx = 20 + raylib.MeasureText("SCORE", 20) + 12
    raylib.DrawText(score_text, sx, 10, 22, C["white"])
    raylib.DrawText("first to %d" % WIN_SCORE, 20, 38, 12, C["dim"])

    # ---- the Python emblem + title ----------------------------------------
    draw_python_emblem(W - 30, 27)
    title = "PYTHON"
    tw = raylib.MeasureText(title, 30)
    raylib.DrawText(title, W - 56 - tw, 8, 30, C["blue"])
    sub = "Python 3.14  +  PyRaylib"
    sw = raylib.MeasureText(sub, 12)
    raylib.DrawText(sub, W - 58 - sw, 38, 12, C["dim"])

    # ---- bottom strip -----------------------------------------------------
    raylib.DrawRectangle(0, H - 54, W, 54, C["panel"])
    raylib.DrawRectangle(0, H - 54, W, 2, C["edge"])

    speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    raylib.DrawText("rally %d   returns %d" % (rally, hits), 16, H - 42, 14, C["dim"])
    raylib.DrawText("ball speed %.0f px/s" % speed, 16, H - 24, 14, C["dim"])

    if ball_x >= SEAM_VX:
        raylib.DrawText("BALL: IN THE PYTHON COURT", 300, H - 42, 15, C["blue"])
    else:
        raylib.DrawText("BALL: AWAY, PAST THE SEAM", 300, H - 42, 15, C["yellow"])
    raylib.DrawText("virtual x %.0f of %d" % (ball_x, FIELD_W), 300, H - 24, 13, C["dim"])

    raylib.DrawText("ESC / close window to quit", 560, H - 30, 13, C["dim"])

    # ---- centre banners ---------------------------------------------------
    if winner == 2:
        raylib.DrawText("PYTHON WINS THE MATCH", 178, 268, 30, C["blue"])
        raylib.DrawText("new match starting...", 320, 306, 16, C["dim"])
    elif winner == 1:
        raylib.DrawText("MATCH LOST", 268, 268, 30, C["yellow"])
        raylib.DrawText("new match starting...", 320, 306, 16, C["dim"])
    elif serve_wait > 0:
        raylib.DrawText("GET READY", 306, 278, 30, C["white"])


# ---------------------------------------------------------------------------
#  Main loop -- the Python window owns its own frame clock.
# ---------------------------------------------------------------------------
while not raylib.WindowShouldClose():
    t = raylib.GetTime()
    dt = raylib.GetFrameTime()
    if dt > 0.05:
        dt = 0.05

    if state("running", 1.0) < 0.5:
        break

    ball_x = state("ball_x", 1200.0)
    ball_y = state("ball_y", 300.0)
    ball_vx = state("ball_vx", 0.0)
    ball_vy = state("ball_vy", 0.0)
    rally = state_int("rally", 0)
    lua_score = state_int("lua_score", 0)
    py_score = state_int("py_score", 0)
    winner = state_int("winner", 0)
    serve_wait = state_int("serve_wait", 0)

    # 1. think
    update_ai(dt, ball_x, ball_y, t)

    # 2. tell the referee where my paddle is, and count my returns
    publish("py_paddle_y", paddle_y)
    if rally != prev_rally and rally > 0:
        hits += 1
        prev_rally = rally
    publish("py_hits", hits)

    # 3. remember the ball for the trail (in this court's own coordinates)
    trail.append((ball_x - COURT_LO, ball_y))

    # 4. draw
    raylib.BeginDrawing()
    raylib.ClearBackground(C["deep"])

    draw_court()
    draw_trail()
    draw_seam(ball_x, t)
    draw_goal()
    draw_paddle(paddle_y)
    draw_ball(ball_x, ball_y)
    draw_hud(ball_x, ball_y, ball_vx, ball_vy, rally, lua_score, py_score,
             winner, serve_wait)

    raylib.DrawText("%d FPS" % raylib.GetFPS(), 60, 8, 14, C["dim"])

    raylib.EndDrawing()

    if raylib.IsKeyPressed(raylib.KEY_ESCAPE):
        break

# Leaving the loop means this court is done: tell the referee to shut the
# whole match down, then close the window.
publish("quit", 1)
raylib.CloseWindow()
