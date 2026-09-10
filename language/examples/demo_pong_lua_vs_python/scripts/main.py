# Pong: Python AI (right screen) vs Lua AI (left screen)
# GnuChanOS Embed example — ball state is shared between windows.
# Paddle and score are LOCAL to each process.
import raylib
import gcl

gcl.init()

WIN_W = 800
WIN_H = 600
WORLD_W = 1600          # total width of the two windows
SCREEN_OFFSET = 800     # Python window shows the right half

PADDLE_W = 12
PADDLE_H = 90
BALL_R = 8
PY_PADDLE_X = WORLD_W - 20 - PADDLE_W  # Python paddle (right, world x = 1568)
SPEED = 4.0

# LOCAL state
py_pad = WIN_H / 2 - PADDLE_H / 2
py_score = 0

raylib.InitWindow(WIN_W, WIN_H, "Pong: Python AI (Right screen) - GnuChanOS")
raylib.SetTargetFPS(60)

while not raylib.WindowShouldClose():
    # Read shared ball state
    bx = gcl.get_state("ball_x")
    by = gcl.get_state("ball_y")
    vx = gcl.get_state("ball_vx")
    vy = gcl.get_state("ball_vy")

    # Move the ball (world coordinates)
    bx += vx
    by += vy

    # Top/bottom walls
    if by - BALL_R <= 0 or by + BALL_R >= WIN_H:
        vy = -vy
        by = max(BALL_R, min(WIN_H - BALL_R, by))

    # Python AI: move its own paddle (right, local) toward the ball
    target = by - PADDLE_H / 2
    if py_pad < target:
        py_pad += SPEED
    elif py_pad > target:
        py_pad -= SPEED
    py_pad = max(0, min(WIN_H - PADDLE_H, py_pad))

    # Right paddle collision (world x)
    if bx + BALL_R >= PY_PADDLE_X and by >= py_pad and by <= py_pad + PADDLE_H and vx > 0:
        vx = -vx
        bx = PY_PADDLE_X - BALL_R

    # Score: right edge passed -> Lua (left) wins, I (Python) lose
    if bx > WORLD_W:
        py_score += 0  # keep own score (local); Lua scores on its side
        bx, by, vx, vy = 780, WIN_H / 2, -4.0, 2.0
    elif bx < 0:
        py_score += 1  # I passed Lua's edge, I win
        bx, by, vx, vy = 790, WIN_H / 2, 4.0, 2.0

    # Write shared ball state
    gcl.set_state("ball_x", bx)
    gcl.set_state("ball_y", by)
    gcl.set_state("ball_vx", vx)
    gcl.set_state("ball_vy", vy)

    # Draw (right half: world x 800..1600 -> screen x 0..800)
    raylib.BeginDrawing()
    raylib.ClearBackground(raylib.DARKPURPLE)

    # Center divider (screen x=0)
    y = 0
    while y < WIN_H:
        raylib.DrawRectangle(-2, y, 4, 15, raylib.DARKGRAY)
        y += 30

    # Draw the ball only when it is inside this window
    if bx >= SCREEN_OFFSET:
        raylib.DrawCircle(int(bx - SCREEN_OFFSET), int(by), BALL_R, raylib.RED)

    # Python paddle (own, local): screen x = 1568-800 = 768
    raylib.DrawRectangle(PY_PADDLE_X - SCREEN_OFFSET, int(py_pad), PADDLE_W, PADDLE_H, raylib.GREEN)

    # Local score
    raylib.DrawText(str(py_score), WIN_W - 60, 20, 40, raylib.BLACK)
    raylib.DrawText("Right: Python AI", WIN_W - 170, WIN_H - 30, 20, raylib.RAYWHITE)
    raylib.EndDrawing()

raylib.CloseWindow()
