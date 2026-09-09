# Pong: Python AI (right screen) vs Lua AI (left screen)
# GnuChanOS Embed example — ball state is shared between windows.
# Paddle and score are LOCAL to each process.
import raylib
import gcl

gcl.init()

WIN_W = 800
WIN_H = 600
WORLD_W = 1600          # iki pencere genişligi
SCREEN_OFFSET = 800     # Python penceresi sag yariyi gosterir

PADDLE_W = 12
PADDLE_H = 90
BALL_R = 8
PY_PADDLE_X = WORLD_W - 20 - PADDLE_W  # Python paddle (sag, dunya x = 1568)
SPEED = 4.0

# LOCAL state
py_pad = WIN_H / 2 - PADDLE_H / 2
py_score = 0

raylib.InitWindow(WIN_W, WIN_H, "Pong: Python AI (Right screen) - GnuChanOS")
raylib.SetTargetFPS(60)

while not raylib.WindowShouldClose():
    # shared ball state oku
    bx = gcl.get_state("ball_x")
    by = gcl.get_state("ball_y")
    vx = gcl.get_state("ball_vx")
    vy = gcl.get_state("ball_vy")

    # topu hareket ettir (dunya koordinat)
    bx += vx
    by += vy

    # ust/alt duvar
    if by - BALL_R <= 0 or by + BALL_R >= WIN_H:
        vy = -vy
        by = max(BALL_R, min(WIN_H - BALL_R, by))

    # Python AI: kendi paddle'ini (sag, local) topa gore oynat
    target = by - PADDLE_H / 2
    if py_pad < target:
        py_pad += SPEED
    elif py_pad > target:
        py_pad -= SPEED
    py_pad = max(0, min(WIN_H - PADDLE_H, py_pad))

    # sag paddle carpismasi (dunya x)
    if bx + BALL_R >= PY_PADDLE_X and by >= py_pad and by <= py_pad + PADDLE_H and vx > 0:
        vx = -vx
        bx = PY_PADDLE_X - BALL_R

    # skor: sag uc gecildi -> Lua (sol) kazandi, ben (Python) kaybettim
    if bx > WORLD_W:
        py_score += 0  # kendi skorunu tut (local); Lua kendi tarafinda kazandi
        bx, by, vx, vy = 780, WIN_H / 2, -4.0, 2.0
    elif bx < 0:
        py_score += 1  # Lua'nin ucunu gectim, ben kazandim
        bx, by, vx, vy = 790, WIN_H / 2, 4.0, 2.0

    # shared ball state yaz
    gcl.set_state("ball_x", bx)
    gcl.set_state("ball_y", by)
    gcl.set_state("ball_vx", vx)
    gcl.set_state("ball_vy", vy)

    # cizim (sag yari: dunya x 800..1600 -> ekran x 0..800)
    raylib.BeginDrawing()
    raylib.ClearBackground(raylib.DARKPURPLE)

    # orta sinir cizgisi (ekran x=0)
    y = 0
    while y < WIN_H:
        raylib.DrawRectangle(-2, y, 4, 15, raylib.DARKGRAY)
        y += 30

    # top sadece bu pencereye girdiyse ciz
    if bx >= SCREEN_OFFSET:
        raylib.DrawCircle(int(bx - SCREEN_OFFSET), int(by), BALL_R, raylib.RED)

    # Python paddle (kendi, local): ekran x = 1568-800 = 768
    raylib.DrawRectangle(PY_PADDLE_X - SCREEN_OFFSET, int(py_pad), PADDLE_W, PADDLE_H, raylib.GREEN)

    # local skor
    raylib.DrawText(str(py_score), WIN_W - 60, 20, 40, raylib.BLACK)
    raylib.DrawText("Right: Python AI", WIN_W - 170, WIN_H - 30, 20, raylib.RAYWHITE)
    raylib.EndDrawing()

raylib.CloseWindow()
