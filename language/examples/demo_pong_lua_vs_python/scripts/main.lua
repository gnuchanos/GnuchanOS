-- ============================================================================
--  GnuchanOS  ·  Embed demo  ·  LUA court  (left window)
-- ----------------------------------------------------------------------------
--  This window is a real Lua 5.4 program drawn with Raylib (LuaRaylib).
--  It owns the LEFT court and the LEFT AI paddle.
--
--  The ball itself is NOT simulated here.  main.gcsf (the GCL host) is the
--  referee: it keeps the single ball for a virtual 1600x600 playfield that is
--  split down the middle into two courts --
--
--        virtual x = 0 .. 800   ->  this window      (LUA, navy + yellow)
--        virtual x = 800 .. 1600 -> the Python window (PYTHON)
--
--  and it streams the ball position into the Embed shared state every frame.
--  This file reads it and answers with its own paddle position:
--
--        ball_y      = gcl.get_state("ball_y")      -- read the ball
--        gcl.set_state("lua_paddle_y", paddle_y)    -- report my paddle
--
--  Because the two courts are two halves of ONE playfield, the ball visibly
--  travels out of one window and into the other: it walks off the right edge
--  of this window and appears at the left edge of the Python window.
-- ============================================================================

gcl.init()

-- --------------------------------------------------------------------------
--  Tunables (must match main.gcsf)
-- --------------------------------------------------------------------------
local W          = 800            -- this window is 800x600
local H          = 600
local FIELD_W    = 1600           -- the virtual playfield shared by both courts
local SEAM_VX    = 800            -- the boundary the ball crosses
local PADDLE_X   = 36             -- this court's paddle defends the LEFT wall
local PADDLE_W   = 18
local PADDLE_H   = 96
local BALL_R     = 10
local AI_SPEED   = 430            -- px/s the AI paddle can travel
local TRAIL_MAX  = 26
local WIN_SCORE  = 7

-- --------------------------------------------------------------------------
--  Shared-state bridge
-- --------------------------------------------------------------------------
--  gcl.get_state returns a number, or nil when the key is not published yet.
local function state(key, fallback)
    local v = gcl.get_state(key)
    if v == nil then return fallback end
    if type(v) ~= "number" then return fallback end
    return v
end

local function publish(key, value)
    gcl.set_state(key, value)
end

-- Same, but for values that are counts (rally, scores, flags).  gcl.get_state
-- hands back a float and string.format("%d", ...) plus raylib's integer
-- arguments both want a true integer, so the cast happens once, here.
local function state_int(key, fallback)
    return math.floor(state(key, fallback))
end

-- --------------------------------------------------------------------------
--  Palette -- the Lua identity: deep navy, yellow, and a white crescent moon
-- --------------------------------------------------------------------------
local C = {
    deep    = raylib.GetColor(0x07101FFF),   -- outside the court
    court   = raylib.GetColor(0x0B1F3AFF),   -- the playfield
    grid    = raylib.GetColor(0x123457FF),   -- service lines
    panel   = raylib.GetColor(0x102848FF),   -- HUD strips
    edge    = raylib.GetColor(0x1B3A6BFF),   -- panel borders
    yellow  = raylib.GetColor(0xFFDD33FF),   -- Lua yellow
    dim     = raylib.GetColor(0x62809FFF),
    white   = raylib.GetColor(0xF2F6FFFF),
    seam    = raylib.GetColor(0x3FA9FFFF),   -- the portal line between courts
    goal    = raylib.GetColor(0xFF6B6BFF),
    goaloff = raylib.GetColor(0x3A2A44FF),
}

-- The ball looks the SAME in both windows, so it reads as one object that
-- crosses the boundary rather than two balls on two screens.
local BALL_CORE = raylib.GetColor(0xFFF3C4FF)
local BALL_HALO = raylib.GetColor(0xFFC93CFF)

-- --------------------------------------------------------------------------
--  Window
-- --------------------------------------------------------------------------
raylib.InitWindow(W, H, "LUA court  |  Lua 5.4 + LuaRaylib  |  GnuchanOS Embed demo")
raylib.SetTargetFPS(60)
raylib.SetWindowPosition(60, 140)

-- Tell the referee this court is alive.  main.gcsf waits for BOTH courts
-- before it starts the clock.
publish("lua_paddle_y", H / 2)
publish("lua_ready", 1)
publish("quit", 0)

local paddle_y  = H / 2          -- centre of my paddle (virtual y)
local trail     = {}             -- recent ball positions, in this court's coords
local trail_n   = 0
local trail_i   = 0
local hits      = 0              -- how many times this court returned the ball
local last_rally = -1

-- --------------------------------------------------------------------------
--  AI -- this is the "Lua AI paddle".  It is deliberately beatable: it only
--  tracks while the ball is inside its own court, drifts home otherwise, and
--  its maximum speed is finite, so a fast enough ball gets past it.
-- --------------------------------------------------------------------------
local function update_ai(dt, ball_x, ball_y, t)
    local target = H / 2                                  -- rest position
    if ball_x < SEAM_VX then
        -- the ball is in my half: shadow it, with a slow sway so the rally
        -- is not a perfect mirror each time
        target = ball_y + math.sin(t * 2.4) * 22
    end

    local dy = target - paddle_y
    local step = AI_SPEED * dt
    if dy > step then dy = step elseif dy < -step then dy = -step end
    paddle_y = paddle_y + dy

    local half = PADDLE_H / 2
    if paddle_y < half then paddle_y = half end
    if paddle_y > H - half then paddle_y = H - half end
end

-- --------------------------------------------------------------------------
--  Drawing helpers
-- --------------------------------------------------------------------------
local function draw_court()
    raylib.DrawRectangle(0, 0, W, H, C.court)

    -- service grid
    for x = 80, W - 1, 80 do
        raylib.DrawRectangle(x, 54, 1, H - 108, C.grid)
    end
    for y = 80, H - 1, 80 do
        raylib.DrawRectangle(0, y, W, 1, C.grid)
    end

    -- the centre service line of this court, dashed
    local y = 60
    while y < H - 60 do
        raylib.DrawRectangle(W // 2, y, 2, 18, C.grid)
        y = y + 40
    end
end

-- The seam: the right edge of this window is the boundary with the Python
-- court.  It pulses when the ball is close, so you can see the ball leaving.
local function draw_seam(ball_x, t)
    local near = 1.0 - math.min(1.0, math.abs(SEAM_VX - ball_x) / 420.0)
    local pulse = 0.5 + 0.5 * math.sin(t * 6.0)
    local glow = raylib.Fade(C.seam, 0.10 + 0.45 * near)

    raylib.DrawRectangle(W - 14, 54, 14, H - 108, glow)
    raylib.DrawRectangle(W - 3, 54, 3, H - 108, raylib.Fade(C.seam, 0.35 + 0.5 * near))

    -- chevrons pointing into the other court
    local cy = 300
    for i = 1, 3 do
        local a = 0.18 + 0.55 * near * pulse
        local x0 = W - 16 - i * 16
        raylib.DrawTriangle(x0, cy - 16, x0, cy + 16, x0 + 12, cy, raylib.Fade(C.seam, a))
    end
end

local function draw_goal()
    -- this court's own goal line (the left wall of the virtual playfield)
    raylib.DrawRectangle(0, 54, 6, H - 108, C.goaloff)
    raylib.DrawRectangle(0, 54, 2, H - 108, C.goal)
end

local function draw_paddle(py)
    local rect = raylib.Rectangle(PADDLE_X, py - PADDLE_H / 2, PADDLE_W, PADDLE_H)
    raylib.DrawRectangleRounded(rect, 0.5, 8, raylib.Fade(C.yellow, 0.22))
    local core = raylib.Rectangle(PADDLE_X + 2, py - PADDLE_H / 2 + 2, PADDLE_W - 4, PADDLE_H - 4)
    raylib.DrawRectangleRounded(core, 0.5, 8, C.yellow)
    raylib.DrawRectangle(PADDLE_X + 4, math.floor(py - PADDLE_H / 2 + 8), 3,
                         PADDLE_H - 16, C.white)
end

local function draw_trail()
    if trail_n == 0 then return end
    for i = 0, trail_n - 1 do
        local idx = (trail_i - 1 - i) % TRAIL_MAX + 1
        local p = trail[idx]
        if p ~= nil then
            local age = i / TRAIL_MAX
            local a = (1.0 - age) * 0.42
            raylib.DrawCircleV(p.x, p.y, BALL_R * (1.0 - age * 0.7), raylib.Fade(BALL_HALO, a))
        end
    end
end

local function draw_ball(ball_x, ball_y)
    if ball_x < -40 or ball_x > W + 40 then
        -- the ball is in the other court: show a marker on the seam so the
        -- viewer knows it is coming back
        local side = (ball_x > W) and 1 or -1
        local cy = ball_y
        if cy < 70 then cy = 70 end
        if cy > H - 70 then cy = H - 70 end
        raylib.DrawTriangle(W - 22, cy - 12, W - 22, cy + 12, W - 6, cy,
                            raylib.Fade(BALL_HALO, 0.75))
        if side > 0 then
            raylib.DrawCircleLines(W - 40, math.floor(cy), BALL_R,
                                   raylib.Fade(BALL_HALO, 0.5))
        end
        return
    end

    raylib.DrawCircleV(ball_x, ball_y, BALL_R + 11, raylib.Fade(BALL_HALO, 0.12))
    raylib.DrawCircleV(ball_x, ball_y, BALL_R + 6, raylib.Fade(BALL_HALO, 0.28))
    raylib.DrawCircleV(ball_x, ball_y, BALL_R, BALL_HALO)
    raylib.DrawCircleV(ball_x - 3, ball_y - 3, BALL_R - 4, BALL_CORE)
    raylib.DrawCircleV(ball_x + 3, ball_y + 3, BALL_R - 7, C.white)
end

local function draw_hud(ball_x, ball_y, ball_vx, ball_vy, rally, ls, ps, winner, serve_wait)
    -- ---- top strip ---------------------------------------------------------
    raylib.DrawRectangle(0, 0, W, 54, C.panel)
    raylib.DrawRectangle(0, 54, W, 2, C.edge)

    -- the Lua crescent emblem
    raylib.DrawCircle(30, 27, 17, C.yellow)
    raylib.DrawCircle(38, 21, 17, C.panel)

    raylib.DrawText("LUA", 56, 8, 30, C.yellow)
    raylib.DrawText("Lua 5.4  +  LuaRaylib", 58, 38, 12, C.dim)

    -- ---- match score -------------------------------------------------------
    -- Only THIS court is named in this window: the shared match score is shown
    -- as bare numbers, so the Lua window never has to spell the other
    -- language out.
    raylib.DrawText("SCORE", 556, 10, 20, C.dim)
    local score_text = string.format("%d : %d", ls, ps)
    local lsx = 556 + raylib.MeasureText("SCORE", 20) + 12
    raylib.DrawText(score_text, lsx, 10, 22, C.white)
    raylib.DrawText(string.format("first to %d", WIN_SCORE), 556, 38, 12, C.dim)

    -- ---- bottom strip ------------------------------------------------------
    raylib.DrawRectangle(0, H - 54, W, 54, C.panel)
    raylib.DrawRectangle(0, H - 54, W, 2, C.edge)

    local speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    raylib.DrawText(string.format("rally %d   returns %d", rally, hits), 16, H - 42, 14, C.dim)
    raylib.DrawText(string.format("ball speed %.0f px/s", speed), 16, H - 24, 14, C.dim)

    local here = (ball_x < SEAM_VX)
    if here then
        raylib.DrawText("BALL: IN THE LUA COURT", 320, H - 42, 15, C.yellow)
    else
        raylib.DrawText("BALL: AWAY, PAST THE SEAM", 320, H - 42, 15, C.seam)
    end
    raylib.DrawText(string.format("virtual x %.0f of %d", ball_x, FIELD_W), 320, H - 24, 13, C.dim)

    raylib.DrawText("ESC / close window to quit", 560, H - 30, 13, C.dim)

    -- ---- centre banners ----------------------------------------------------
    if winner == 1 then
        raylib.DrawText("LUA WINS THE MATCH", 196, 268, 30, C.yellow)
        raylib.DrawText("new match starting...", 320, 306, 16, C.dim)
    elseif winner == 2 then
        raylib.DrawText("MATCH LOST", 268, 268, 30, C.seam)
        raylib.DrawText("new match starting...", 320, 306, 16, C.dim)
    elseif serve_wait > 0 then
        raylib.DrawText("GET READY", 306, 278, 30, C.white)
    end
end

-- --------------------------------------------------------------------------
--  Main loop -- the Lua window owns its own frame clock.
-- --------------------------------------------------------------------------
local prev_rally = -1

while not raylib.WindowShouldClose() do
    local t  = raylib.GetTime()
    local dt = raylib.GetFrameTime()
    if dt > 0.05 then dt = 0.05 end

    local running = state("running", 1)
    if running < 0.5 then break end

    local ball_x = state("ball_x", 400)
    local ball_y = state("ball_y", 300)
    local ball_vx = state("ball_vx", 0)
    local ball_vy = state("ball_vy", 0)
    local rally = state_int("rally", 0)
    local lua_score = state_int("lua_score", 0)
    local py_score = state_int("py_score", 0)
    local winner = state_int("winner", 0)
    local serve_wait = state_int("serve_wait", 0)

    -- 1. think
    update_ai(dt, ball_x, ball_y, t)

    -- 2. tell the referee where my paddle is, and count my returns
    publish("lua_paddle_y", paddle_y)
    if rally ~= prev_rally and rally > 0 then
        hits = hits + 1
        prev_rally = rally
    end
    publish("lua_hits", hits)

    -- 3. remember the ball for the trail
    trail_i = trail_i % TRAIL_MAX + 1
    trail[trail_i] = { x = ball_x, y = ball_y }
    if trail_n < TRAIL_MAX then trail_n = trail_n + 1 end

    -- 4. draw
    raylib.BeginDrawing()
    raylib.ClearBackground(C.deep)

    draw_court()
    draw_trail()
    draw_seam(ball_x, t)
    draw_goal()
    draw_paddle(paddle_y)
    draw_ball(ball_x, ball_y)
    draw_hud(ball_x, ball_y, ball_vx, ball_vy, rally, lua_score, py_score,
             winner, serve_wait)

    raylib.DrawText(string.format("%d FPS", raylib.GetFPS()), 700, 8, 14, C.dim)

    raylib.EndDrawing()

    if raylib.IsKeyPressed(raylib.KEY_ESCAPE) then break end
end

-- Leaving the loop means this court is done: tell the referee to shut the
-- whole match down, then close the window.
publish("quit", 1)
raylib.CloseWindow()
