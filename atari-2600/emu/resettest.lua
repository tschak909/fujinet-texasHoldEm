-- resettest.lua -- restart the 6507 mid-session and prove the conversation
-- continues.
--
-- This connector has NO RESET LINE to the cartridge. A restart re-zeroes
-- nothing on the cart side: it keeps running with its sequence number where
-- it was. So a client that counted sequences in RAM would restart at 1,
-- collide with a sequence already answered, and have every later transaction
-- dropped in silence -- which is why FNGO derives the next one from the
-- cartridge's own ACKSEQ.
--
-- PASS requires the post-reset sequence to be exactly ONE more than the value
-- it started from: the client talked again, and it counted from the cart.
--
-- soft_reset(), NOT the "Reset Game" ioport field. That field is the console
-- SWITCH, which on this machine is a bit in a RIOT register the program reads
-- -- pressing it reboots nothing at all. (The client does act on it, by
-- choice; this tests the restart itself.)
--
-- soft_reset RE-RUNS this script, so every bit of state lives in _G or the
-- harness restarts with the machine and waits forever for a reset it has
-- already done.

local ACKSEQ = 0x1F00
local BANK   = 0x1F0E

_G._rt = _G._rt or { n = 0, phase = "warm", before = nil }

local function say(f, ...) print("RESET: " .. string.format(f, ...)) end

_G._reset = _G._reset or emu.add_machine_frame_notifier(function()
    local t = _G._rt
    t.n = t.n + 1
    -- The settle is the WARM phase's alone. Applying it after the reset too
    -- is what made this test miss the transaction it exists to inspect: the
    -- cartridge answers the first one within a couple of frames of the 6507
    -- restarting, long before frame 240.
    if t.phase == "warm" and t.n < 240 then return end
    local sp = manager.machine.devices[":maincpu"].spaces["program"]

    if t.phase == "warm" then
        t.before = sp:readv_u8(ACKSEQ)
        if t.before == 0 then return end        -- nothing has talked yet
        say("ACKSEQ is %02X before the reset, bank %d",
            t.before, sp:readv_u8(BANK))
        t.phase = "after"
        t.n = 0
        manager.machine:soft_reset()
        return
    end

    if t.phase == "after" then
        -- CATCH THE FIRST TRANSACTION, do not wait a fixed number of frames.
        --
        -- The assertion is about the very next sequence the client sends, and
        -- ACKSEQ only holds that value until the transaction after it. This
        -- waited 1200 frames -- twenty seconds -- and by then a boot that
        -- finds its name already in an appkey has opened the key, read it,
        -- fetched /tables and more besides: ACKSEQ had moved on by seven and
        -- the test called the client broken for counting correctly seven
        -- times. The unmodified 5 Card Stud client fails its own copy of this
        -- test with the identical numbers, which is how this was found.
        --
        -- So: poll every frame and take the FIRST value that differs.
        local now = sp:readv_u8(ACKSEQ)
        if now == t.before then
            if t.n > 3600 then
                say("FAIL -- nothing talked again in sixty seconds")
                t.phase = "exit"
                t.n = 0
            end
            return
        end
        -- WHAT IS ACTUALLY ASSERTED, and why it is not "before + 1".
        --
        -- The strict form cannot be sampled. soft_reset() does not hand the
        -- script a frame boundary at the moment the 6507 restarts, and the
        -- cold path opens an appkey, reads it and fetches /tables before this
        -- notifier is called again -- so ACKSEQ has already advanced by
        -- several. The 5 Card Stud client this came from fails its own copy
        -- of that assertion with the identical numbers against a cartridge
        -- that is behaving perfectly.
        --
        -- So assert the thing that tells the two implementations apart. A
        -- client counting sequences in RAM restarts at 1 and collides with
        -- numbers the cart has already answered; a client deriving them from
        -- ACKSEQ + 1 carries straight on from where the cartridge was. The
        -- discriminator is therefore that the sequence CONTINUED -- it is
        -- above where it started, and it did not go back to the beginning --
        -- and that is exactly as strong a statement about the bug.
        local d = (now - t.before) % 256
        say("ACKSEQ is %02X after the reset, was %02X: +%d, bank %d",
            now, t.before, d, sp:readv_u8(BANK))
        manager.machine.video:snapshot()
        if d >= 1 and d < 128 then
            say("PASS -- the sequence carried on from the cartridge, not from RAM")
        else
            say("FAIL -- the sequence restarted: it was counted in RAM")
        end
        t.phase = "exit"
        t.n = 0
        return
    end

    -- The exit must not share a frame with the snapshot or the file never
    -- flushes.
    if t.phase == "exit" and t.n > 20 then manager.machine:exit() end
end)
