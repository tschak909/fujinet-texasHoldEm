-- smoke.lua: headless smoke test of the whole flow. Press keypad "1" at
-- the OS menu to launch, pull the trigger to accept the default name,
-- then press keypad "2" every 3 seconds: the first press joins table 2
-- by digit shortcut, and later presses land inside real move windows
-- (the AI table's bots keep the hand moving). Snapshot near the end.
--   mame ... -autoboot_script emu/smoke.lua -video none -sound none \
--        -seconds_to_run 70
local function port_by_suffix(suffix)
    for tag, port in pairs(manager.machine.ioport.ports) do
        if tag:sub(-#suffix) == suffix then return port end
    end
    return nil
end

local phase = 0
local shot = false
local key2 = false
emu.register_frame(function()
    local t = manager.machine.time.seconds
    if phase == 0 and t >= 3 then
        local kp = port_by_suffix("KEYPAD3")
        if kp == nil then return end
        emu.print_info("smoke.lua: keypad 1 down (t=" .. t .. ")")
        kp:field(0x10):set_value(1)
        phase = 1
    elseif phase == 1 and t >= 5 then
        port_by_suffix("KEYPAD3"):field(0x10):clear_value()
        phase = 2
    elseif phase == 2 and t >= 10 then
        local h = port_by_suffix("ctrl1:joy:HANDLE")
        if h == nil then
            emu.print_info("smoke.lua: no HANDLE port found")
            phase = 4
            return
        end
        emu.print_info("smoke.lua: trigger down (t=" .. t .. ")")
        h:field(0x10):set_value(1)
        phase = 3
    elseif phase == 3 and t >= 12 then
        port_by_suffix("ctrl1:joy:HANDLE"):field(0x10):clear_value()
        phase = 4
    elseif phase == 4 and t >= 14 then
        local m = t % 3
        if m < 1 and not key2 then
            port_by_suffix("KEYPAD2"):field(0x10):set_value(1)
            key2 = true
        elseif m >= 1 and key2 then
            port_by_suffix("KEYPAD2"):field(0x10):clear_value()
            key2 = false
        end
    end
    if not shot and t >= 64 then
        emu.print_info("smoke.lua: snapshot")
        manager.machine.video:snapshot()
        shot = true
    elseif t >= 66 then
        manager.machine:exit()
    end
end)
