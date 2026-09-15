-- bedwatch.lua -- dump the community bed on EVERY frame it changes.
--
-- The end state of the board is checked by board.lua and is correct. This is
-- for the states in between: cdcomp composes a chunk per frame, so anything
-- the board passes through on its way to being right is on screen for a frame
-- and is exactly what "a slight glitch" looks like at 60Hz.
--
-- It found one: the board used to be composed in TWO chunks, and because
-- composing a text row writes all six planes it cleared the rank half of the
-- bed a frame before the blit repainted it. 41 of 86 bed changes were
-- half-drawn -- once per poll, every poll. The board is one chunk again.
--
-- ONE "SPLIT" SURVIVES AND IS NOT A FAULT. On the frame a table is entered,
-- CDREDRW asks for FNCLS, which blanks rows 0-20 in order; this notifier fires
-- on MAME's frame boundary, which can land inside that loop and see row 16
-- blanked and row 17 not yet. APPVBL runs in the vblank and finishes before
-- the kernel draws a line, so the raster never shows it -- and it happens on a
-- screen change, once, where a blank is invisible anyway. Expect exactly one,
-- at step 0. More than that, or any at step 1, is the bug coming back.
if os.getenv("DRIVE_LUA") then dofile(os.getenv("DRIVE_LUA")) end
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local RBOARD, GCOMM, PLANES = 16, 87, 4
local n, last = 0, nil

local function snap()
    local t = {}
    for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
        for line = 0, 5 do
            for p = 0, PLANES - 1 do
                t[#t + 1] = string.format("%02X",
                    sp:readv_u8(0x1800 + p * 0x80 + row * 6 + line))
            end
        end
    end
    return table.concat(t)
end

local function art(row, line)
    local s = ""
    for x = 0, PLANES * 8 - 1 do
        local b = sp:readv_u8(0x1800 + (x // 8) * 0x80 + row * 6 + line)
        s = s .. (((b & (0x80 >> (x % 8))) ~= 0) and "#" or ".")
    end
    return s
end

-- Retained in _G: an unreferenced notifier is collected, and this one was --
-- it reported the first frame and then went quiet for eight minutes.
_G._bedw = emu.add_machine_frame_notifier(function()
    n = n + 1
    local now = snap()
    if now == last then return end
    last = now
    local wire = ""
    for i = 0, 10 do
        local c = sp:readv_u8(0x1B00 + GCOMM + i)
        if c == 0 then break end
        wire = wire .. string.char(c)
    end
    -- How many slots carry ink, per row: a board mid-compose has a different
    -- count in its two rows, and that asymmetry IS the glitch.
    local cnt = {}
    for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
        local k = 0
        for slot = 0, 4 do
            local inked = false
            for line = 0, 4 do
                for i = 0, 4 do
                    local x = 2 + slot * 6 + i
                    local b = sp:readv_u8(0x1800 + (x // 8) * 0x80 + row * 6 + line)
                    if (b & (0x80 >> (x % 8))) ~= 0 then inked = true end
                end
            end
            if inked then k = k + 1 end
        end
        cnt[#cnt + 1] = k
    end
    local tag = (cnt[1] ~= cnt[2]) and "  <<< SPLIT" or ""
    print(string.format("BEDW f%-6d wire='%s' ranks=%d pips=%d step=%d%s",
                        n, wire, cnt[1], cnt[2], sp:readv_u8(0x92), tag))
    if cnt[1] ~= cnt[2] then
        for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
            for line = 0, 4 do
                print(string.format("BEDW   r%d l%d %s", row, line, art(row, line)))
            end
        end
    end
end)
