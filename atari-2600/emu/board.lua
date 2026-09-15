-- board.lua -- the community board, checked as PLANE BYTES across a whole
-- hand.
--
-- This is the one thing Texas Hold'em added to this client, and it is the one
-- thing a screenshot is least able to settle: at five pixels a pip a heart, a
-- spade and a club are nearly the same picture, and the card bed is not on
-- the cell grid at all. So nothing here looks at the screen. It reads the
-- cartridge's text planes at $1800 -- the bytes the kernel draws from -- and
-- asserts their SHAPE against the wire's community[11] at reply offset 87.
--
-- What it proves, in the order the bugs would appear:
--
--   1. the seam. Line 5 of both rows is blank in all four planes, because
--      that is the line the kernel reprogrammes colour on and no glyph may
--      be drawn there.
--   2. PIXEL 7 IS NEVER INKED. Bit 0 of plane 0 cannot be drawn by any
--      client -- six player copies 2.67 cycles apart against seven 3-cycle
--      GRP writes lose exactly one pixel -- which is the whole reason
--      FN_CARD_X0 is 2. A bed flush to pixel 0 shaves the left edge off the
--      first card and a King renders as a bare vertical bar.
--   3. the pitch. Every inked pixel lies inside some slot's five-wide window
--      at 2 + 6k, so a wrong pitch or a wrong margin cannot hide.
--   4. THE SHRINK. Slots past the dealt count carry no ink in either row --
--      which is how a five-card river collapses to an empty pre-flop board
--      with no blanking code anywhere in the client. Every sibling port in
--      this family got that wrong once.
--   5. planes 4 and 5 of the board rows are UNTOUCHED by the blit. That is
--      what lets the pot and your purse sit in columns 8-11 beside the
--      board, and it is the first thing that would break if the bed ever
--      grew past four planes.
--   6. the street. 0 cards at pre-flop, 3 at the flop, 4 at the turn, 5 at
--      the river -- read out of the BED, not out of the wire, so it proves
--      the render and not merely the server.
--
--   DRIVE_LUA=emu/drive.lua ./run.sh texas board
if os.getenv("DRIVE_LUA") then dofile(os.getenv("DRIVE_LUA")) end

local sp = manager.machine.devices[":maincpu"].spaces["program"]
local RBOARD, GCOMM = 16, 87
local X0, PITCH, INKW, SLOTS, PLANES = 2, 6, 5, 5, 4
local CDROUND = 0xAD

local seen, fails, checks = {}, 0, 0

local function plane(p, row, line)
    return sp:readv_u8(0x1800 + p * 0x80 + row * 6 + line)
end

local function fail(f, ...)
    fails = fails + 1
    print("BOARD FAIL: " .. string.format(f, ...))
end

-- community[11] off the wire, as a string of card characters
local function community()
    local t = ""
    for i = 0, 10 do
        local c = sp:readv_u8(0x1B00 + GCOMM + i)
        if c == 0 then break end
        t = t .. string.char(c)
    end
    return t
end

-- is pixel px inked on this line of this row?
local function px(row, line, x)
    return (plane(x // 8, row, line) & (0x80 >> (x % 8))) ~= 0
end

local function check(round)
    local wire = community()
    local n = math.floor(#wire / 2)
    checks = checks + 1
    print(string.format("BOARD round %d community '%s' (%d cards)",
                        round, wire, n))

    -- 1. the seam
    for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
        for p = 0, PLANES - 1 do
            if plane(p, row, 5) ~= 0 then
                fail("seam: row %d plane %d line 5 = %02X, want 00",
                     row, p, plane(p, row, 5))
            end
        end
    end

    -- 2/3. every inked pixel is inside a slot window, and never pixel 7
    local windows = {}
    for k = 0, SLOTS - 1 do
        for i = 0, INKW - 1 do windows[X0 + k * PITCH + i] = k end
    end
    local ink = {}
    for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
        for line = 0, 4 do
            for x = 0, PLANES * 8 - 1 do
                if px(row, line, x) then
                    if x == 7 then
                        fail("pixel 7 inked at row %d line %d -- FN_CARD_X0",
                             row, line)
                    end
                    local k = windows[x]
                    if k == nil then
                        fail("ink outside every slot window: row %d line %d px %d",
                             row, line, x)
                    else
                        ink[row .. ":" .. k] = true
                    end
                end
            end
        end
    end

    -- 4. dealt slots have ink in both rows; the rest have none in either
    for k = 0, SLOTS - 1 do
        local top = ink[RBOARD .. ":" .. k]
        local bot = ink[(RBOARD + 1) .. ":" .. k]
        if k < n then
            if not top then fail("slot %d: no rank drawn", k) end
            if not bot then fail("slot %d: no pip drawn", k) end
        else
            if top or bot then
                fail("slot %d is not dealt but carries ink -- THE SHRINK", k)
            end
        end
    end

    -- 5. the pot and purse columns survive the blit
    local side = 0
    for p = 4, 5 do
        for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
            for line = 0, 5 do
                if plane(p, row, line) ~= 0 then side = side + 1 end
            end
        end
    end
    if side == 0 then
        fail("planes 4-5 of the board rows are blank: the pot and purse are gone")
    end

    -- 6. the street, read out of the bed
    local want = ({ [1] = 0, [2] = 3, [3] = 4, [4] = 5 })[round]
    if want and n ~= want then
        fail("round %d shows %d cards, want %d", round, n, want)
    end
    -- Render the bed back out as ASCII, so a glitch INSIDE a slot window --
    -- which every assertion above would pass -- is visible as a picture.
    -- The 48 pixels are printed whole, slot boundaries marked, so debris
    -- between cards has nowhere to hide either.
    for _, row in ipairs({ RBOARD, RBOARD + 1 }) do
        for line = 0, 5 do
            local t = ""
            for x = 0, PLANES * 8 - 1 do
                if x % 8 == 0 and x > 0 then t = t .. "|" end
                t = t .. (px(row, line, x) and "#" or ".")
            end
            print(string.format("BED r%d l%d %s", row, line, t))
        end
    end
    seen[round] = n
    -- A picture of each street as it lands, so the run leaves something a
    -- human can look at beside the plane bytes a machine checked.
    if n > 0 and not seen["shot" .. n] then
        seen["shot" .. n] = true
        manager.machine.video:snapshot()
    end
end

local last, settle = nil, 0
_G._board = emu.add_machine_frame_notifier(function()
    local r = sp:readv_u8(CDROUND)
    if r ~= last then last, settle = r, 30 return end
    if settle > 0 then
        settle = settle - 1
        -- Check once the compose has had time to finish: cdcomp spreads a
        -- recompose over nine or ten frames, so sampling on the edge catches
        -- a table that is half old and half new.
        if settle == 0 and r >= 1 and r <= 5 then check(r) end
    end
end)

local n = 0
_G._board2 = emu.add_machine_frame_notifier(function()
    n = n + 1
    if n % 900 ~= 0 then return end
    local out = {}
    for r = 1, 5 do
        if seen[r] then out[#out + 1] = string.format("r%d=%d", r, seen[r]) end
    end
    print(string.format("BOARD: %d checks, %d failures, streets seen: %s",
                        checks, fails, table.concat(out, " ")))
    print(checks > 0 and fails == 0 and "BOARD: PASS" or "BOARD: FAIL")
end)
