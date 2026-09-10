```lua
-- Pong: Lua AI (left screen) vs Python AI (right screen)
-- GnuChanOS Embed example — ball state is shared between windows.
-- Paddle and score are LOCAL to each process.
gcl.init()

local WIN_W = 800
local WIN_H = 600
local WORLD_W = 1600          -- total width of the two windows
local SCREEN_OFFSET = 0       -- Lua window shows the left half (world x 0..800)

local PADDLE_W = 12
local PADDLE_H = 90
local BALL_R = 8
local LU_PADDLE_X = 20                       -- Lua paddle (world x)
local SPEED = 4.0

-- LOCAL state
local lua_pad = WIN_H / 2 - PADDLE_H / 2
local lua_score = 0

raylib.InitWindow(WIN_W, WIN_H, "Pong: Lua AI (Left screen) - GnuChanOS")
raylib.SetTargetFPS(60)

while not raylib.WindowShouldClose() do
    -- Read shared ball state
    local bx = gcl.get_state("ball_x")
    local by = gcl.get_state("ball_y")
    local vx = gcl.get_state("ball_vx")
    local vy = gcl.get_state("ball_vy")

    -- Lua AI: move its own paddle (left, local) toward the ball
    local target = by - PADDLE_H / 2
    if lua_pad < target then
        lua_pad = lua_pad + SPEED
    elseif lua_pad > target then
        lua_pad = lua_pad - SPEED
    end
    if lua_pad < 0 then lua_pad = 0 end
    if lua_pad > WIN_H - PADDLE_H then lua_pad = WIN_H - PADDLE_H end

    -- Left paddle collision (world x)
    if bx - BALL_R <= LU_PADDLE_X + PADDLE_W and by >= lua_pad and by <= lua_pad + PADDLE_H and vx < 0 then
        vx = -vx
        bx = LU_PADDLE_X + PADDLE_W + BALL_R
    end

    -- Score: left edge passed -> Python (right) wins, I (Lua) lose
    if bx < 0 then
        lua_score = lua_score + 0  -- keep own score (local); Python scores on its side
        bx, by, vx, vy = 810, WIN_H / 2, 4.0, 2.0
    elseif bx > WORLD_W then
        lua_score = lua_score + 1  -- I passed Python's edge, I win
        bx, by, vx, vy = 780, WIN_H / 2, -4.0, 2.0
    end

    -- Write shared ball state
    gcl.set_state("ball_x", bx)
    gcl.set_state("ball_y", by)
    gcl.set_state("ball_vx", vx)
    gcl.set_state("ball_vy", vy)

    -- Draw (left half: world x 0..800 -> screen x 0..800)
    raylib.BeginDrawing()
    raylib.ClearBackground(raylib.DARKPURPLE)

    -- Draw center divider (screen x=800)
    local y = 0
    while y < WIN_H do
        raylib.DrawRectangle(WIN_W - 2, y, 4, 15, raylib.DARKGRAY)
        y = y + 30
    end

    -- Draw the ball only when it is inside this window
    if bx <= SCREEN_OFFSET + WIN_W then
        raylib.DrawCircle(math.floor(bx - SCREEN_OFFSET), math.floor(by), BALL_R, raylib.RED)
    end

    -- Lua paddle (own, local): screen x = 20
    raylib.DrawRectangle(LU_PADDLE_X - SCREEN_OFFSET, math.floor(lua_pad), PADDLE_W, PADDLE_H, raylib.BLUE)

    -- Local score
    raylib.DrawText(tostring(lua_score), 20, 20, 40, raylib.BLACK)
    raylib.DrawText("Left: Lua AI", 10, WIN_H - 30, 20, raylib.RAYWHITE)
    raylib.EndDrawing()
end  

raylib.CloseWindow()
```
