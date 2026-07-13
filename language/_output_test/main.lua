
-- D:\GnuchanOS\language\_output_test  test alani burasi ve D:\GnuchanOS\language\src\luarun_from_dll_so asil dile gecmeden embed et luayi .dll, .so okuma odakli

local Entity = {}
Entity.__index = Entity

function Entity:new(id, name)
    local self = setmetatable({}, Entity)

    self.id = id
    self.name = name
    self.components = {}
    self.flags = {}
    self.children = {}
    self.position = {
        x = math.random() * 500,
        y = math.random() * 500
    }

    return self
end

function Entity:addComponent(name, data)
    self.components[name] = data
end

function Entity:addChild(child)
    table.insert(self.children, child)
end

function Entity:getComponent(name)
    return self.components[name]
end

function Entity:walk(callback)
    callback(self)

    for _, child in ipairs(self.children) do
        child:walk(callback)
    end
end

------------------------------------------------------------

local cache = setmetatable({}, {
    __index = function(t, k)
        local v = {}

        rawset(t, k, v)

        return v
    end
})

------------------------------------------------------------

local function profiler(fn)

    return function(...)

        local start = os.clock()

        local ok, result = xpcall(function(...)
            return fn(...)
        end, debug.traceback, ...)

        print(string.format(
            "[PROFILE] %.6f sec",
            os.clock() - start
        ))

        if not ok then
            error(result)
        end

        return result
    end

end

------------------------------------------------------------

local function deepMerge(a, b)

    for k, v in pairs(b) do

        if type(v) == "table" then

            if type(a[k]) ~= "table" then
                a[k] = {}
            end

            deepMerge(a[k], v)

        else

            a[k] = v

        end

    end

    return a

end

------------------------------------------------------------

local Scheduler = {}

Scheduler.__index = Scheduler

function Scheduler:new()

    return setmetatable({
        tasks = {}
    }, Scheduler)

end

function Scheduler:add(fn)

    table.insert(
        self.tasks,
        coroutine.create(fn)
    )

end

function Scheduler:update(dt)

    for i = #self.tasks, 1, -1 do

        local co = self.tasks[i]

        if coroutine.status(co) == "dead" then
            table.remove(self.tasks, i)
        else

            local ok, err = coroutine.resume(co, dt)

            if not ok then
                print(err)
                table.remove(self.tasks, i)
            end

        end

    end

end

------------------------------------------------------------

local World = {}

World.entities = {}

function World:create(name)

    local e = Entity:new(
        #self.entities + 1,
        name
    )

    table.insert(
        self.entities,
        e
    )

    return e

end

function World:find(predicate)

    for _, entity in ipairs(self.entities) do

        if predicate(entity) then
            return entity
        end

    end

end

------------------------------------------------------------

local scheduler = Scheduler:new()

------------------------------------------------------------

for i = 1, 15 do

    local e = World:create("Enemy_" .. i)

    e:addComponent("health", {
        hp = math.random(50,200),
        max = 200
    })

    e:addComponent("inventory", {
        gold = math.random(100),
        items = {
            "Sword",
            "Potion",
            "Gem"
        }
    })

end

------------------------------------------------------------

scheduler:add(function()

    while true do

        for _, entity in ipairs(World.entities) do

            local hp = entity:getComponent("health")

            hp.hp = math.max(
                hp.hp - math.random(),
                0
            )

        end

        coroutine.yield()

    end

end)

------------------------------------------------------------

local expensiveCalculation = profiler(function(id)

    local total = 0

    for i = 1, 500000 do
        total = total + math.sin(i)
    end

    cache[id].value = total

    return total

end)

------------------------------------------------------------

deepMerge(
{
    graphics = {
        width = 800,
        height = 600
    }
},
{
    graphics = {
        fullscreen = true,
        vsync = true
    },
    audio = {
        volume = 0.75
    }
})

------------------------------------------------------------

for frame = 1, 120 do

    scheduler:update(1/60)

    if frame % 30 == 0 then

        local boss = World:find(function(e)
            return e.id == 5
        end)

        if boss then

            boss.position.x =
                boss.position.x + math.random(-10,10)

            boss.position.y =
                boss.position.y + math.random(-10,10)

        end

    end

end

------------------------------------------------------------

expensiveCalculation(123)

------------------------------------------------------------

for _, entity in ipairs(World.entities) do

    entity:walk(function(e)

        local hp = e:getComponent("health")

        print(
            e.id,
            e.name,
            hp.hp
        )

    end)

end