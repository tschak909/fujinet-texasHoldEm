-- drive.lua -- type a name, sit down, and play a hand.
--
-- Waits on the CLIENT'S OWN state -- the live bank at $1F0E and CDENT at $BB
-- -- and never on frame numbers. A network round trip takes as long as the
-- server takes, and a harness that counts frames instead passes on a fast day
-- and fails on a slow one.
--
-- Inputs are EDGE-DETECTED by the client (INSCAN returns only what is newly
-- pressed), so every press must be released again with frames in between.

local BANK   = 0x1F0E
local CDENT  = 0xBB
local CDCNT  = 0xAB
local CDNPLR = 0xAC
local CDROUND = 0xAD
local CDSEL  = 0xAA

local B_LOB, B_GAM, B_NET, B_MNU, B_NAM = 0, 1, 2, 3, 4

local sp, n, phase, hold, step, waited = nil, 0, "boot", 0, 1, 0
local seq = {}
local fails = 0

local function press(name)
    local tag = name:match("^P1") and ":joyport1:joy:JOY" or ":SWB"
    manager.machine.ioport.ports[tag].fields[name]:set_value(1)
    hold = 6                                    -- frames held
    _G._held = { tag = tag, name = name }
end

local function release()
    if _G._held then
        manager.machine.ioport.ports[_G._held.tag]
            .fields[_G._held.name]:set_value(0)
        _G._held = nil
    end
end

local function say(f, ...) print("DRIVE: " .. string.format(f, ...)) end

-- DRIVE_STAY keeps the seat after the verdict instead of quitting. Harnesses
-- that watch a WHOLE HAND -- board.lua's street progression, frame.lua over a
-- deal -- need the client still playing; this one has always had everything
-- it wanted within half a minute and taken the machine down with it.
local stay = os.getenv("DRIVE_STAY")

local function done(ok)
    release()
    manager.machine.video:snapshot()
    say(ok and "PASS" or "FAIL")
    if stay and ok then
        say("staying seated (DRIVE_STAY)")
        phase = "stay"
        return
    end
    phase = "exit"
    n = 0
end

_G._drive = emu.add_machine_frame_notifier(function()
    n = n + 1
    if phase == "stay" then
        -- Nothing to drive: the client submits the highlighted move when its
        -- own clock expires, which is what an unattended seat does anyway.
        -- NOT called "playing" -- that phase already exists below and is
        -- where the verdict is reached; an earlier cut of this used the name
        -- and silently swallowed every dump and the PASS with it.
        return
    end
    if phase == "exit" then
        -- the exit must not share a frame with the snapshot or the file
        -- never flushes
        if n > 20 then manager.machine:exit() end
        return
    end
    if n < 120 then return end                  -- let it boot
    sp = sp or manager.machine.devices[":maincpu"].spaces["program"]

    if hold > 0 then
        hold = hold - 1
        if hold == 0 then release() end
        return
    end
    if _G._gap and _G._gap > 0 then
        _G._gap = _G._gap - 1
        return
    end

    local bank = sp:readv_u8(BANK)

    if phase == "boot" then
        if bank == B_NAM then
            say("the keyboard is up; typing")
            -- Type "ABC", then DOWN out of the grid (which lands on OK
            -- from any column) and press it.
            seq = { "P1 Button 1", "P1 Right", "P1 Button 1",
                    "P1 Right", "P1 Button 1",
                    "P1 Down", "P1 Down", "P1 Down", "P1 Button 1" }
            step = 1
            phase = "typing"
        elseif bank == B_LOB then
            phase = "lobby"                      -- the appkey already had one
        end
        return
    end

    if phase == "typing" then
        if step > #seq then
            say("name entered; waiting for the lobby")
            phase = "tolobby"
            return
        end
        press(seq[step])
        say("press %-14s sel=%d len=%d cur=%02X prev=%02X",
            seq[step], sp:readv_u8(CDSEL), sp:readv_u8(0xBF),
            sp:readv_u8(0xA8), sp:readv_u8(0xA9))
        _G._gap = 4
        step = step + 1
        return
    end

    if phase == "tolobby" then
        waited = waited + 1
        if waited % 180 == 0 then
            say("waiting: bank=%d ent=%02X sel=%d len=%d cur=%02X ack=%02X",
                bank, sp:readv_u8(CDENT), sp:readv_u8(CDSEL),
                sp:readv_u8(0xBF), sp:readv_u8(0xA8), sp:readv_u8(0x1F00))
        end
        if bank == B_LOB then phase = "lobby" end
        if waited > 3600 then say("never reached the lobby"); done(false) end
        return
    end

    if phase == "lobby" then
        -- WAIT FOR THE LIST, do not sample once. Reaching bank 0 means the
        -- lobby bank is running, not that its /tables has come back -- and
        -- when the username is already in an appkey the boot phase arrives
        -- here within a frame or two of the switch, well before the round
        -- trip finishes. Sampling CDCNT at that moment reads the zero it was
        -- initialised to and reports an empty lobby against a server holding
        -- five tables. This harness's own header says to wait on the client's
        -- state rather than on frame numbers; this is that rule.
        local cnt = sp:readv_u8(CDCNT)
        if cnt == 0 then
            waited = waited + 1
            if waited % 300 == 0 then
                say("waiting for /tables: err=%02X step=%02X ack=%02X rx=%d",
                    sp:readv_u8(0x9A), sp:readv_u8(0x9B),
                    sp:readv_u8(0x1F00), sp:readv_u8(0x1F04))
            end
            if waited > 1800 then
                say("the lobby is empty -- nothing to sit at")
                done(false)
            end
            return
        end
        say("at the lobby, %d tables", cnt)
        manager.machine.video:snapshot()
        waited = 0
        press("P1 Button 1")                     -- sit at the first one
        _G._gap = 4
        waited = 0
        phase = "sitting"
        return
    end

    if phase == "sitting" then
        waited = waited + 1
        if waited % 180 == 0 then
            say("sitting: bank=%d ent=%02X err=%02X nplr=%d", bank,
                sp:readv_u8(CDENT), sp:readv_u8(0x9A), sp:readv_u8(CDNPLR))
        end
        if bank == B_GAM and sp:readv_u8(CDNPLR) > 0 then
            say("seated: %d players, round %d",
                sp:readv_u8(CDNPLR), sp:readv_u8(CDROUND))
            waited = 0
            phase = "playing"
        elseif waited > 3600 then
            say("never got a table state")
            done(false)
        end
        return
    end

    if phase == "playing" then
        waited = waited + 1
        -- Catch the frame the client leaves the table bank, and dump
        -- everything: which bank, the PC, and all 64 bytes of zero page.
        if sp:readv_u8(CDENT) > 8 and not _G._caught then
            _G._caught = true
            local cpu = manager.machine.devices[":maincpu"]
            local pc = "?"
            if cpu.state and cpu.state["PC"] then
                pc = string.format("%04X", cpu.state["PC"].value)
            end
            say("CDENT CHANGED at f%d: bank=%d pc=%s ent=%02X", n, bank, pc,
                sp:readv_u8(CDENT))
            local z = ""
            for a = 0x80, 0xBF do
                z = z .. string.format("%02X", sp:readv_u8(a))
                if a % 8 == 7 then z = z .. " " end
            end
            say("zp80 %s", z)
            local m = ""
            for a = 0xB0, 0xBF do
                m = m .. string.format("%02X ", sp:readv_u8(a))
            end
            say("zpB0 %s  (CDMOVE=B8 CDENT=BB)", m)
            manager.machine.video:snapshot()
        end
        if waited % 300 == 0 then
            say("f%d bank=%d ent=%02X err=%02X players=%d round=%d sel=%d " ..
                "nmv=%d act=%02X clk=%d lrpg=%d", n, bank,
                sp:readv_u8(CDENT), sp:readv_u8(0x9A), sp:readv_u8(CDNPLR),
                sp:readv_u8(CDROUND), sp:readv_u8(CDSEL), sp:readv_u8(0xAF),
                sp:readv_u8(0xAE), sp:readv_u8(0xB1), sp:readv_u8(0xB4))
            manager.machine.video:snapshot()
        end
        -- The card beds, bit for bit: seat 0's two hole cards and the
        -- community board. Squinting at a screenshot is how the family's
        -- display bugs survived; comparing plane bytes is how they were
        -- found. Two beds now, because the board is the thing this game
        -- added and it is drawn by the same blitter.
        --
        -- Offsets are Hold'em's: players[0] is at 165 (5 Card Stud's 154 plus
        -- community[11]), hand[11] at 22 inside it, community[11] at 87. Seat
        -- 0's rows are 0-1 because the seats come first on this screen, and
        -- the board's are 16-17.
        if waited == 310 then
            local function wire(off, n)
                local t = ""
                for i = 0, n - 1 do
                    local c = sp:readv_u8(0x1B00 + off + i)
                    t = t .. (c == 0 and "." or string.char(c))
                end
                return t
            end
            local function bed(what, r0)
                for row = r0, r0 + 1 do
                    for pl = 0, 3 do
                        local o = ""
                        for l = 0, 5 do
                            o = o .. string.format("%02X",
                                sp:readv_u8(0x1800 + pl * 0x80 + row * 6 + l))
                        end
                        say("%s r%d p%d %s", what, row, pl, o)
                    end
                end
            end
            say("hand0 '%s'", wire(165 + 22, 11))
            bed("hand", 0)
            say("board '%s'", wire(87, 11))
            bed("board", 16)
        end
        -- Snapshot the interesting moments the moment they happen: a
        -- five-sample-a-minute sweep walks straight past a banner that holds
        -- a page for a second and a half.
        if sp:readv_u8(0xB4) ~= 0 and not _G._shotlr then
            _G._shotlr = true
            say("the end-of-hand banner is up")
            manager.machine.video:snapshot()
        end
        if sp:readv_u8(0xAF) > 0 and sp:readv_u8(0xAE) == 0
           and sp:readv_u8(0xB1) > 0 and not _G._shotmv then
            _G._shotmv = true
            say("our turn: %d moves, clock %d",
                sp:readv_u8(0xAF), sp:readv_u8(0xB1))
            manager.machine.video:snapshot()
        end
        -- Hold SELECT for a stretch, so the purse overlay lands in a shot.
        if waited == 600 then
            manager.machine.ioport.ports[":SWB"].fields["Select Game"]
                :set_value(1)
            say("SELECT held: the value fields should show purses")
        elseif waited == 640 then
            manager.machine.video:snapshot()
        elseif waited == 660 then
            manager.machine.ioport.ports[":SWB"].fields["Select Game"]
                :set_value(0)
        end
        if waited > tonumber(os.getenv("PLAY_FRAMES") or "1500") then
            done(sp:readv_u8(CDNPLR) > 0)
        end
        return
    end
end)
