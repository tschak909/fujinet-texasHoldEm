-- frame.lua -- is every frame exactly 262 scanlines, and is there a picture
-- in it?
--
-- THIS IS THE TEST THE SEAM LINE IS CHECKED WITH. That line reprogrammes the
-- row colour on the one scanline of each cell that no glyph can be drawn on,
-- and it has to finish inside its 76 cycles. It is a WSYNC-bounded loop, so
-- one that runs long does not glitch -- it silently costs a SECOND scanline,
-- every row, and the frame goes from 262 to 283. The screen still looks
-- entirely like text. Nothing but a measurement finds this.
--
-- Texas Hold'em moved the seats to row 0 so the seam could turn a row into a
-- seat index with one LSR instead of 5 Card Stud's DEX-then-LSR, and put the
-- board and the bar both above RBOARD so one bound test separates chrome from
-- seats. That is two cycles cheaper than the client this came from -- but
-- "cheaper" is a claim, and this is how the claim is settled.
--
-- Frames are delimited by the program's own VSYNC writes, not by MAME's
-- frame notifier: MAME does NOT reshape its screen when the frame length
-- changes (it stays 176x223), so asking the emulator how tall the picture is
-- reports the same number whatever the program does. emu/scrh.lua exists to
-- record exactly that and is no use here.
--
-- THE INK CHECK IS NOT BELT AND BRACES. A frame-length test passes trivially
-- on a blank screen and one did: a bad edit left the lobby composing nothing
-- and this measurement reported every frame at 262, which was true and
-- meaningless. So it also counts inked bytes in the cartridge's own text
-- planes at $1800 -- the bytes the kernel draws from.
--
--   ./run.sh layout frame           the geometry, with no network at all
--   DRIVE_LUA=emu/drive.lua ./run.sh texas frame    a real table
if os.getenv("DRIVE_LUA") then dofile(os.getenv("DRIVE_LUA")) end

local sp = manager.machine.devices[":maincpu"].spaces["program"]
local FRAME = 1 / 59.92
local LINE = FRAME / 262
local WANT = 262

local n, last, hist, blank, worst = 0, nil, {}, 0, 0
local report

local function inked()
    local c = 0
    for plane = 0, 5 do
        for i = 0, 127 do
            if sp:readv_u8(0x1800 + plane * 128 + i) ~= 0 then c = c + 1 end
        end
    end
    return c
end

_G._fr = sp:install_write_tap(0x00, 0x00, "vsync", function(off, data)
    if (data & 0x02) == 0 then return end
    local t = manager.machine.time:as_double()
    n = n + 1
    if last then
        local lines = math.floor((t - last) / LINE + 0.5)
        hist[lines] = (hist[lines] or 0) + 1
        if lines > worst then worst = lines end
        -- A bank handover lands after a drawn picture rather than before one,
        -- so nothing moves for it; only a run of them is interesting.
        if lines > WANT + 4 then
            print(string.format("FRAME: %d lines at f%d, bank %d",
                                lines, n, sp:readv_u8(0x1F0E)))
        end
    end
    last = t
    if inked() == 0 then blank = blank + 1 end
    -- Reported periodically rather than at machine stop: this MAME has no
    -- stop notifier, and every other harness here settles for the same.
    if n % 600 == 0 then report() end
end)

report = function()
    local keys = {}
    for k in pairs(hist) do keys[#keys + 1] = k end
    table.sort(keys)
    local ok, tot = 0, 0
    for _, k in ipairs(keys) do
        print(string.format("FRAME %3d lines x%d", k, hist[k]))
        tot = tot + hist[k]
        if k >= WANT - 1 and k <= WANT + 1 then ok = ok + hist[k] end
    end
    print(string.format("FRAME: %d of %d frames at %d(+/-1), worst %d, %d blank",
                        ok, tot, WANT, worst, blank))
    -- WHAT COUNTS AS PASSING, measured rather than assumed. The unmodified
    -- 5 Card Stud client was run through this same harness against a live
    -- table as a control, and it reports 1174 of 1199 frames at 262, a worst
    -- of 292, and 5 blank frames. Two frames per poll are long by
    -- construction -- the transaction tail in bank 2 and the handover into
    -- bank 5 -- and both land AFTER a drawn picture rather than before one,
    -- so nothing moves for them. Failing on those would be failing on the
    -- reference.
    --
    -- The bug this exists to catch does not look like that. A seam line that
    -- runs past its 76 cycles costs every seat row a second scanline, so the
    -- frame is 283 SYSTEMATICALLY -- thousands of frames, not eleven per
    -- thousand -- and the 262 fraction collapses. So the test is the
    -- fraction and the worst outlier, and the blank count is reported rather
    -- than judged.
    print((tot > 0 and ok >= tot * 0.95 and worst <= 320)
          and "FRAME: PASS" or "FRAME: FAIL")
end
