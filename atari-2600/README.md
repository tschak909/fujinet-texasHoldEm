# FujiNet Texas Hold 'Em — Atari 2600

The 5 Card Stud client for this console, moved forward one struct field — on a
machine with **128 bytes of RAM, no framebuffer, one button and four
directions.**

Converted from
[`fujinet-5cardstud/atari-2600`](https://github.com/dillera/fujinet-5cardstud)
(branch `add-atari2600`), whose README is the lessons-learned document for this
hardware and which this one does not repeat. Read that first. What follows is
only what Hold'em changed.

Talks to the cartridge in
[`fujinet-firmware/pico/atari-2600`](https://github.com/FujiNetWIFI/fujinet-firmware)
— branch **`2600-experiment`**, see Building — and to
`https://th.carr-designs.com/` in the `?bin=1` binary wire format.

```sh
make                # build/texas.bin, 16384 bytes
make layout         # the screen with no network at all
make frame          # is every frame 262 scanlines? (no network)
make run            # in a window
make drive          # headless, plays a hand against the live server
make board          # the community board, as plane bytes, across a hand
make resettest      # a 6507 restart, mid-hand
make resetleave     # the RESET switch: menu, leave, back to the list
make hosttest       # the cartridge's card art, bit for bit
```

## What Hold'em changed

**One field.** `Game` gains `char community[11]` at offset 87 and everything
after it shifts eleven bytes: `validMoveCount` 87→98, `validMoves` 88→99,
`playerCount` 153→164, `players` 154→165, and the struct 418→429. `Player` (33
bytes), `ValidMove` (13), `Table` (36) and `/tables` are byte-identical.
`support/host-test/holdem_host_test.c` `_Static_assert`s all of it.

429 still fits the cartridge's 512-byte reply window whole, so the read-in-place
design — the server's reply is never copied into console RAM, because there is
no console RAM to copy it into — survives untouched.

**`hand[11]` now holds two hole cards instead of five**, keeping its old width.
**`FN_BLIT_CARD` needed no change at all** for that, which is the happiest
accident in the port: `vcs_render_cards()` reads exactly eleven bytes, stops at
the first NUL rank and blanks every later slot. Eleven is the width of *both*
`hand[11]` and `community[11]`, so the board's read stops at `community[10]` and
never reaches `validMoveCount` at 98. A two-card hand, a three-card flop and a
five-card river are the same call.

**`FN_CARD_HIDE0` is gone.** 5 Card Stud hid your own down-card until the
showdown. In Hold'em you are entitled to both of yours: the server sends them in
the clear and masks everyone else's on the wire — `"????"` for a live hand,
`"??"` for a folded one, the real characters at the showdown. The flags byte is
zero everywhere.

## The screen

```
 row  0-1    seat 0 (YOU)   [7][9]              THOM   cards cols 0-3
 row  2-3    seat 1         [?][?]              0100   text  cols 8-11
 ...
 row 14-15   seat 7         [?][?]              0250
 row 16      board          [9][A][K][3][6]     1250   the pot
 row 17                     [h][s][d][d][h]     4980   YOUR purse
 row 18-20   the bar        the move menu with a full-width red bar on the
                            selection and the move clock; or WAITING ON
                            <name> over the street; or a page of the
                            end-of-hand banner
```

**Twenty-one rows is the hardware, not a choice.** The cartridge publishes six
128-byte planes and the kernel's inner loop is `lda $1800,y` — four cycles and
never five, *only* because a 128-byte-aligned base plus `Y < 128` cannot carry.
`fuji_mailbox.h` calls that alignment load-bearing. 21 × 6 = 126 ≤ 128 and there
is no twenty-second row at any price.

So the budget is forced. Eight seats at two rows is sixteen of the twenty-one
and the board is two more, which leaves **three bar rows where 5 Card Stud had
four** — and its separate header row is gone, its two numbers moved into the
four columns a five-card bed cannot reach.

### Why the board is below the seats

Because the seam line has four spare cycles out of seventy-six, and this is
where they go.

The seam turns a row number into a seat index. With the seats first that index
is `row >> 1` outright, where 5 Card Stud paid a `DEX` to bias it — and because
the board and the bar are *both* chrome and *both* above `RBOARD`, one `cpx`
separates them from the seats where a board at the top of the screen would have
needed two. The seat path lands at **70 cycles against 5 Card Stud's 72**.

That matters because this is a `WSYNC`-bounded loop: a seam line that runs past
76 does not glitch, it silently costs a **second scanline on every seat row**,
and the screen still looks entirely like text. `make frame` is the only thing
that finds it — 1199 of 1199 frames at exactly 262 scanlines.

The layout is pinned at assembly time rather than in a comment:

```asm
        IF      RSEAT0<>0
        ERROR   "the seam line's LSR assumes the seats start at row 0"
        ENDIF
        IF      RBOARD<>RSEAT0+MAXPLYR*RSEATH
        ERROR   "the seam line's single bound test assumes the board follows the seats"
        ENDIF
```

### The pot is four digits, and 5 Card Stud's was five

Four columns is what is left beside a five-card bed. A Hold'em pot passes 9999
at a full table far more readily than a stud pot did, and a clamped 9999 is
honest where a truncated 2345-out-of-12345 is a lie about an order of magnitude.
`CDCL4` already existed for the seat fields.

### There is no street label on the board, and there is one on the bar

The board announces its own street once three cards are down — and rounds 0, 1
and 5 all show an empty board, which is exactly when the word is worth having.
So it goes where 5 Card Stud printed `ROUND` over a bare digit, which in this
game would be a number standing in for a word every player already has. Eight
space-padded bytes an entry indexed by the round, so the field needs no
terminator and a shorter name leaves nothing of a longer one behind it.

### Columns 4-7 of a seat row are blank felt

Two hole cards reach pixel 13, but `FN_BLIT_CARD` clears all four card planes —
columns 0-7 — whatever it draws. Reclaiming those eighteen pixels for an
eight-character name and a per-seat last-move field needs a slot count in
`FN_HOT_BLIT_CNT` and a firmware change. Deliberately not taken.

## The board shrinks for free

The case every sibling port in this family got wrong once: `"????"` becomes
`"??"` when a seat folds, and a five-card river becomes an empty board on the
next deal. Fields that shrink without a clear leave the old ones showing.

**There is no blanking code here and there does not need to be.** `card_line()`
builds each plane byte from zero and stores it *whole* rather than merging, over
all four planes and all six lines of both rows — so one blit rewrites the entire
48-byte bed and a board going to empty writes 48 zeros. The Intellivision port
carries a `prev_community` and a blanking loop for this; do not port them, and
there is no byte of zero page to hold one anyway.

`make board` asserts it against a live server, and saw it: flop `9haskd`, turn
`+3d`, river `+6h`, then **zero cards on the next deal.**

## Two bugs inherited from 5 Card Stud, fixed here

**The end-of-hand banner never reached page two.** `GLRNEXT` stashed the next
page number in `CDTMP`, called `GLRROW` — which takes `CDTMP` for its own row
counter and leaves it at zero — and read the zero back. Zero means "no banner
showing", so every result long enough to need a second page showed the first and
cut straight to the move menu. It is this client's own "`CDTMP` is not safe
across a call" rule, in the one place it was not applied. Nothing is carried
across the call now: page N+1 begins at row N×`NBARROW`, and `CDLRPG` is already
N.

**A status byte is an enum, not a scale.** `cmp #PSFOLD / bcs GSDIM` dimmed
everything ≥ 2. Hold'em has `PSALLIN = 4` — a seat still in the hand and simply
out of chips — so an unsigned `>=` greyed out the one player who cannot fold.
Only 2 and 3 are out now. (The sources disagree about whether the `bin=1`
serializer ever emits a 4; this client is correct either way.)

## Bank sizes, and the bank that got its space back

`cdgame` had **thirty spare bytes of 2048** in the client this came from, and
that was the binding constraint on the whole port — `GBOARD` is bigger than the
`GHEAD` it replaces.

It was also a mirage. Since the chunked compose landed, *both* whole-table entry
points go to `cdcomp`: `GWARM` jumps `ENDRAW` and the SELECT repaint is
`ENDRSEA`, and both are `BANKCMP`. `cdgame` reaches only `GBAR` and `GFAIL` from
its frame hook — so the seat renderer, the header and `CDCARD` were being
assembled into four banks that never called them. `GRENDER` was dead in *every*
bank; nothing had called it since the compose was split.

Gating that half of `render.inc` and `state.inc` on `CDBANK=BANKCMP` — the same
"a bank leaves out what it never calls" rule `cdlib`, `cdisp`, `net` and `url`
already run on — is what pays for the board:

| bank | 5 Card Stud | here |
|---|---|---|
| `cdlobby` | 830 spare | 889 |
| `cdgame` | **30** | **346** |
| `cdnet` | 839 | 843 |
| `cdmenu` | 826 | 886 |
| `cdname` | 657 | 717 |
| `cdcomp` | 244 | 179 |

## The board is two chunks, not one

`cdcomp` composes a chunk per frame, each sized to fit a 2,812-cycle vblank,
with the picture up throughout. 5 Card Stud's header was one row and shared step
zero with `FNCLS`. The board is two rows, and two four-digit `CDDEC`s plus
`FNCLS`'s 913 cycles come to about 2,900 in the worst case — over budget, and a
chunk that overruns is not absorbed: it lengthens its frame and pushes the next
picture down the screen by the difference, which is the jump this bank exists to
have removed.

So `CSBRD0` is the clear, the upper row and the pot; `CSBRD1` is the lower row,
your purse and the blit. `ENDRSEA` still enters at `CSSEAT` and skips both.

## The banner no longer hides the move menu

`GBAR` hands the whole bar to the banner whenever `CDLRPG` is set, so a result
is not merely crowding the move menu — the menu is not drawn. Four rows a page
was two pages and about three seconds; **three rows a page is up to three pages
and four and a half**, with the move clock running the whole while and
submitting the highlight from behind a banner the player never saw past. A fresh
turn retires the banner now. The last hand's result stops mattering the moment
this one needs an answer.

## Building

**The cartridge half is on a branch.** `pico/atari-2600`'s FujiNet cart —
`fuji_mailbox.h`, `tools/checkdefs.py`, `tools/checkrom.py`, `emu/apply.sh`,
`firmware/host_test/` — lives on the firmware's **`2600-experiment`** branch and
nowhere else, and `build.sh` reaches into two of those on every build. A
worktree keeps it off whatever branch the firmware repo is on:

```sh
git -C ~/Workspace/fujinet-firmware worktree add ~/Workspace/fn-2600 2600-experiment
make FUJI_FIRMWARE=~/Workspace/fn-2600         # or edit the Makefile default
```

Then `asl`/`p2bin` (Macroassembler AS, found on `PATH` or at `~/asl`), a MAME
with `emu/apply.sh` grafted into it, and a `fujinet-pc` listening on BoIP
`127.0.0.1:9995`.

## How it is checked

Nothing here is believed from a screenshot. At 3×5 `S`/`5` and `O`/`0` are one
picture, and the card art is not on the cell grid at all, so it is compared as
**plane bytes**.

| | |
|---|---|
| `make hosttest` | `FN_BLIT_CARD`'s art bit for bit, in the cartridge's own host harness |
| build gates | `checkdefs.py`, `checkrom.py`, `checkbanks.py`, `mktail.py` — all fail the build |
| `make layout` | the kernel, the colours and the 21-row geometry with no network, including all eight seats, which a live table does not always fill |
| **`make frame`** | **every frame exactly 262 scanlines.** This is how the seam line is checked. 1199 of 1199 |
| `make drive` | types a name, sits at a live table, plays — and dumps `hand[11]`, `community[11]` and both card beds' plane bytes |
| **`make board`** | the community board as plane bytes across a whole hand: the seam, the pitch, pixel 7, the street count, and the shrink |
| `make resettest` | a 6507 restart mid-hand: the sequence carried on from the cartridge rather than restarting in RAM |
| `make resetleave` | the RESET *switch* at a table: the menu opens, `/leave` then `/tables` go out, the lobby comes back with ink on it |

Seat 0 holding `7h9d`, decoded out of the planes rather than looked at:

```
 rank row      pip row          slot 0 = 7 of hearts
 .###.  .###.  .#.#.  ..#..     slot 1 = 9 of diamonds
 ...#.  .#.#.  #####  .###.     planes 2-3 all zero: the three
 ...#.  .###.  #####  #####       undealt slots blank themselves
 ...#.  ...#.  .###.  .###.     line 5 always zero: the seam
 ...#.  .###.  ..#..  ..#..     pixel 7 never inked: FN_CARD_X0
```

### Three harness bugs found on the way, two of them inherited

- **`drive.lua` sampled the table count the instant bank 0 appeared**, which is
  when the lobby bank starts running and not when its `/tables` has come back.
  With a username already in an appkey the boot arrives there within a frame or
  two, so it read the zero `CDCNT` was initialised to and reported an empty
  lobby against a server holding five tables. Its own header says to wait on the
  client's state rather than on frame numbers.
- **`resettest.lua` waited 1200 frames after the reset** and then demanded
  `ACKSEQ == before + 1`. By then the cold path has opened an appkey, read it
  and fetched `/tables`; the 5 Card Stud client fails its own copy of that
  assertion with identical numbers against a cartridge behaving perfectly. What
  actually tells the two implementations apart is that the sequence *carried on*
  rather than restarting at 1, and that is what is asserted now.
- **`frame.lua`'s pass threshold was calibrated by running the unmodified 5 Card
  Stud client through it.** Two frames per poll are long by construction — the
  transaction tail in bank 2 and the handover into bank 5 — and both land after
  a drawn picture rather than before one, so nothing moves for them. The control
  reports 1174 of 1199 frames at 262 with a worst of 292; this client reports
  8825 of 8999 with a worst of 291. Failing on those would be failing on the
  reference. The bug the test exists for looks nothing like it: a long seam line
  costs every seat row a scanline, *systematically*.

## Not here

- **Colour per suit**, which is unreachable: `COLUP0`/`COLUP1` are per scanline
  over two fixed interleaved 8-pixel groups, so cards 1/3/5 and 2/4 can differ
  from each other but never from their own suits. Shape carries the suits.
- **PAL.** The kernel's line counts are NTSC and its constants are a
  one-pixel-wide window found by byte-comparing the raster.
- **Real hardware.** Bus timing is the one thing emulation cannot settle. Note
  that a full recompose now fires `FN_BLIT_CARD` **nine** times where 5 Card
  Stud fired eight, and that neither client polls `FN_B_BLITGEN` before the next
  blit — in MAME the blit runs inside the store, so it cannot be seen there.
- **The lobby server-override appkey** (Hold'em is registered as lobby key 8).
  The endpoint is baked in at build time, as on the Astrocade. Deliberate.
- **Dealing animations.** No sibling under 40 columns has them; a per-street
  cue stands in — and it needed no new code, because in this game the round
  *is* the street and `GCUES` already fired on that edge.
