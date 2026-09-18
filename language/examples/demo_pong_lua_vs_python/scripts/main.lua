-- ============================================================================
--  GnuchanOS  ·  Embed demo  ·  LUA court  (left window)
-- ----------------------------------------------------------------------------
--  A real Lua 5.4 program drawn with Raylib (LuaRaylib).
--
--  main.gcsf is the referee.  It owns ONE shared playfield of 1600 x 600 and
--  it owns the ball.  This window owns the left half of that playfield:
--
--      virtual x =    0 ..  800  ->  this window  (drawn at x)
--      virtual x =  800 .. 1600  ->  the far half (drawn at x - 800 over there)
--
--  Every frame the referee publishes the ball and this window's score, and
--  this window answers with the one thing it owns, its paddle:
--
--      gcl.get_state("ball_x")            -- the ball of the shared playfield
--      gcl.get_state("ball_vx")
--      gcl.get_state("score")             -- the score this window plays for
--      gcl.set_state("paddle_y", paddle_y)
--
--  Nothing else crosses.  This window knows the ball, its own score and its
--  own paddle; it never learns anything about the far court beyond the ball
--  that flies away across the seam.
-- ============================================================================

gcl.init()

-- ---------------------------------------------------------------------------
--  Constants -- every one of these must match main.gcsf
-- ---------------------------------------------------------------------------
local W           = 800      -- this window
local H           = 600      -- the shared playfield height
local SEAM_X      = 800      -- virtual x where this court ends
local PADDLE_X    = 36       -- this paddle guards the LEFT wall (virtual x 0)
local PADDLE_W    = 18
local PADDLE_H    = 96
local BALL_R      = 10
local AI_SPEED    = 470      -- px/s the paddle can travel; finite, so it can lose
local AI_JITTER   = 14       -- px of wander, so it is not a perfect machine
local AI_JITTER_HZ = 1.7
local TRAIL       = 24       -- how many ball ghosts to keep
local WIN_SCORE   = 7

local HUD_H       = 54       -- the two HUD strips
local BALL_ALIVE  = 1.0      -- below this speed the referee is holding the ball

-- ---------------------------------------------------------------------------
--  Shared store -- the only channel between this window and the referee
-- ---------------------------------------------------------------------------
local function get(key, fallback)
    local v = gcl.get_state(key)
    if type(v) ~= "number" then return fallback end
    return v
end

local function get_int(key, fallback)
    return math.floor(get(key, fallback))
end

local function put(key, value)
    gcl.set_state(key, value)
end

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

-- Where will the ball cross the line x = target_x?  The two long walls are
-- folded in, so the paddle can start running to the right spot long before
-- the ball gets there -- that is what makes it look like a player instead of
-- something that only wakes up when the ball arrives.
local function predict_y(ball_x, ball_y, ball_vx, ball_vy, target_x)
    if ball_vx == 0 then return ball_y end

    local travel = (target_x - ball_x) / ball_vx
    if travel <= 0 then return ball_y end

    local span = H - 2 * BALL_R                       -- the strip the centre may use
    local u = (ball_y - BALL_R) + ball_vy * travel
    u = u % (2 * span)                                -- fold into one strip
    if u > span then u = 2 * span - u end             -- and mirror the rest
    return u + BALL_R
end

-- ---------------------------------------------------------------------------
--  Palette -- this court wears LUA's colours: the deep navy of the moon's
--  night sky, the blue of the moon itself and a white highlight.
--
--  Every entry comes out of raylib.Color(r, g, b, a): the RGBA constructor of
--  Raylib, one channel per argument, each 0-255 and the last one the alpha.
-- ---------------------------------------------------------------------------
local deep    = raylib.Color(  0,   0,   0, 255)   -- outside the court
local court   = raylib.Color(  0,   0, 102, 255)   -- Lua navy
local grid    = raylib.Color(255, 255, 255,  26)   -- service lines
local panel   = raylib.Color(  0,   0,  51, 255)   -- HUD strips
local edge    = raylib.Color(  0, 102, 204, 255)   -- panel borders
local accent  = raylib.Color(102, 178, 255, 255)   -- Lua blue
local dim     = raylib.Color(140, 170, 210, 255)
local white   = raylib.Color(240, 245, 255, 255)
local seam    = raylib.Color(102, 204, 255, 255)   -- Lua cyan
local goal    = raylib.Color(255,  85,  85, 255)
local goaloff = raylib.Color(102,   0,   0, 255)
local ballhi  = raylib.Color(255, 255, 255, 255)   -- the moon
local balllo  = raylib.Color(150, 190, 235, 255)

-- ---------------------------------------------------------------------------
--  Window
-- ---------------------------------------------------------------------------
raylib.InitWindow(W, H, "LUA court  |  Lua 5.4 + LuaRaylib  |  GnuchanOS Embed demo")
raylib.SetTargetFPS(60)
raylib.SetWindowPosition(60, 140)

put("paddle_y", H / 2)
put("ready", 1)          -- the referee waits for BOTH courts before it starts
put("quit", 0)

local paddle_y = H / 2
local hits     = 0       -- how many times this paddle returned the ball
local prev_vx  = 0       -- the ball's vx on the previous frame
local trail    = {}      -- recent ball positions, newest first

-- ---------------------------------------------------------------------------
--  Helpers that draw one piece of the court
-- ---------------------------------------------------------------------------
local function draw_court(ball_x, t)
    raylib.DrawRectangle(0, 0, W, H, court)

    local gx = 80
    while gx < W do
        raylib.DrawRectangle(gx, HUD_H, 1, H - 2 * HUD_H, grid)
        gx = gx + 80
    end
    local gy = 80
    while gy < H - HUD_H do
        raylib.DrawRectangle(0, gy, W, 1, grid)
        gy = gy + 80
    end
    gy = 60
    while gy < H - 60 do
        raylib.DrawRectangle(W // 2, gy, 2, 18, grid)
        gy = gy + 40
    end

    -- the seam: this window's right edge is where the ball leaves
    local near  = 1.0 - math.min(1.0, math.abs(SEAM_X - ball_x) / 420.0)
    local pulse = 0.5 + 0.5 * math.sin(t * 6.0)
    raylib.DrawRectangle(W - 14, HUD_H, 14, H - 2 * HUD_H, raylib.Fade(seam, 0.10 + 0.45 * near))
    raylib.DrawRectangle(W - 3, HUD_H, 3, H - 2 * HUD_H, raylib.Fade(seam, 0.35 + 0.5 * near))

    local ci = 1
    while ci <= 3 do
        local a  = 0.18 + 0.55 * pulse
        local x0 = W - 16 - ci * 16
        raylib.DrawTriangle(x0, H / 2 - 16, x0, H / 2 + 16, x0 + 12, H / 2, raylib.Fade(seam, a))
        ci = ci + 1
    end

    -- my goal line, on the left wall
    raylib.DrawRectangle(0, HUD_H, 6, H - 2 * HUD_H, goaloff)
    raylib.DrawRectangle(0, HUD_H, 2, H - 2 * HUD_H, goal)
end

local function draw_trail()
    local i = 1
    while i <= #trail do
        local p   = trail[i]
        local age = (i - 1) / TRAIL
        raylib.DrawCircleV(p.x, p.y, BALL_R * (1.0 - age * 0.7),
                           raylib.Fade(ballhi, (1.0 - age) * 0.40))
        i = i + 1
    end
end

local function draw_paddle()
    local top = paddle_y - PADDLE_H / 2
    raylib.DrawRectangleRounded(raylib.Rectangle(PADDLE_X, top, PADDLE_W, PADDLE_H),
                                0.5, 8, raylib.Fade(accent, 0.22))
    raylib.DrawRectangleRounded(raylib.Rectangle(PADDLE_X + 2, top + 2, PADDLE_W - 4, PADDLE_H - 4),
                                0.5, 8, accent)
    raylib.DrawRectangle(PADDLE_X + 4, math.floor(top + 8), 3, PADDLE_H - 16, white)
end

-- The ball while it is inside this window, otherwise a marker on the seam.
local function draw_ball(ball_x, ball_y)
    if ball_x > -40 and ball_x < W + 40 then
        raylib.DrawCircleV(ball_x, ball_y, BALL_R + 11, raylib.Fade(ballhi, 0.12))
        raylib.DrawCircleV(ball_x, ball_y, BALL_R + 6,  raylib.Fade(ballhi, 0.28))
        raylib.DrawCircleV(ball_x, ball_y, BALL_R, ballhi)
        raylib.DrawCircleV(ball_x - 3, ball_y - 3, BALL_R - 4, balllo)
        raylib.DrawCircleV(ball_x + 3, ball_y + 3, BALL_R - 7, white)
    else
        local my = clamp(ball_y, 70, H - 70)
        raylib.DrawTriangle(W - 22, my - 12, W - 22, my + 12, W - 6, my, raylib.Fade(ballhi, 0.75))
    end
end

local function draw_hud(score, ball_vx, ball_vy, serve)
    -- top strip
    raylib.DrawRectangle(0, 0, W, HUD_H, panel)
    raylib.DrawRectangle(0, HUD_H, W, 2, edge)
    raylib.DrawCircle(30, 27, 17, accent)
    raylib.DrawCircle(38, 21, 17, panel)
    raylib.DrawText("LUA", 56, 8, 30, accent)
    raylib.DrawText("Lua 5.4  +  LuaRaylib", 58, 38, 12, dim)
    raylib.DrawText("SCORE", 548, 10, 20, dim)
    raylib.DrawText(string.format("%d / %d", score, WIN_SCORE),
                    548 + raylib.MeasureText("SCORE", 20) + 14, 10, 22, white)
    raylib.DrawText(string.format("%d FPS", raylib.GetFPS()), 716, 10, 13, dim)

    -- bottom strip
    raylib.DrawRectangle(0, H - HUD_H, W, HUD_H, panel)
    raylib.DrawRectangle(0, H - HUD_H, W, 2, edge)
    local speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    raylib.DrawText(string.format("returns %d", hits), 16, H - 42, 14, dim)
    raylib.DrawText(string.format("ball speed %.0f px/s", speed), 16, H - 24, 14, dim)
    if serve then
        raylib.DrawText("ball is being served", 320, H - 42, 15, accent)
    elseif ball_vx < 0 then
        raylib.DrawText("the ball is coming at me", 320, H - 42, 15, accent)
    else
        raylib.DrawText("the ball is leaving my court", 320, H - 42, 15, seam)
    end
    raylib.DrawText("ESC / close window to quit", 560, H - 30, 13, dim)
end

local function draw_banner(score, serve)
    if score >= WIN_SCORE then
        raylib.DrawText("LUA WINS THE MATCH", 196, 268, 30, accent)
        raylib.DrawText("a new match is starting...", 300, 306, 16, dim)
    elseif serve then
        raylib.DrawText("GET READY", 306, 278, 30, white)
    end
end

-- ---------------------------------------------------------------------------
--  Main loop -- the window owns its own frame clock
-- ---------------------------------------------------------------------------
while not raylib.WindowShouldClose() do
    local t  = raylib.GetTime()
    local dt = raylib.GetFrameTime()
    if dt > 0.05 then dt = 0.05 end

    -- ---- 1. what the referee just published ------------------------------
    if get("running", 1) < 0.5 then break end

    local ball_x  = get("ball_x", SEAM_X / 2)
    local ball_y  = get("ball_y", H / 2)
    local ball_vx = get("ball_vx", 0)
    local ball_vy = get("ball_vy", 0)
    local score   = get_int("score", 0)

    local speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    local serve = speed < BALL_ALIVE       -- the referee is holding the ball

    -- ---- 2. my paddle ----------------------------------------------------
    -- The ball comes at me while it travels LEFT (vx < 0).  Then I run to
    -- where it will arrive; while it is leaving I walk back to a ready spot.
    -- I move EVERY frame, so I look alive even when the ball is far away.
    local aim = H / 2
    if ball_vx < 0 then
        aim = predict_y(ball_x, ball_y, ball_vx, ball_vy, PADDLE_X + PADDLE_W)
    end
    aim = aim + math.sin(t * AI_JITTER_HZ) * AI_JITTER

    local step = AI_SPEED * dt
    paddle_y = paddle_y + clamp(aim - paddle_y, -step, step)
    paddle_y = clamp(paddle_y, PADDLE_H / 2, H - PADDLE_H / 2)

    put("paddle_y", paddle_y)

    -- a return I made: the ball turned around right in front of my paddle
    if prev_vx < 0 and ball_vx > 0 then hits = hits + 1 end
    prev_vx = ball_vx

    -- ---- 3. keep the last few ball positions for the trail ---------------
    table.insert(trail, 1, { x = ball_x, y = ball_y })
    if #trail > TRAIL then table.remove(trail) end

    -- ---- 4. draw ---------------------------------------------------------
    raylib.BeginDrawing()
    raylib.ClearBackground(deep)
    draw_court(ball_x, t)
    draw_trail()
    draw_paddle()
    draw_ball(ball_x, ball_y)
    draw_hud(score, ball_vx, ball_vy, serve)
    draw_banner(score, serve)
    raylib.EndDrawing()

    if raylib.IsKeyPressed(raylib.KEY_ESCAPE) then break end
end

-- Leaving the loop means this court is done: tell the referee, then close.
put("quit", 1)
raylib.CloseWindow()
