# Makefile -- a convenience wrapper. build.sh is the real build.
#
# The Texas Hold'em client for the Atari 2600, converted from the 5 Card Stud
# client of the same console (fujinet-5cardstud/atari-2600, branch
# add-atari2600). Its README is the lessons-learned document for this
# hardware and this one does not repeat it.
#
# The root build (mekkogx) drives the cc65/cmoc-family C toolchains; this port
# is 6502 assembly under Macroassembler AS, which is a different toolchain
# entirely, so it stands alone next to intv/, arcadia/, astrocade/, o2/ and
# channelf/ rather than being forced through machinery it would only
# complicate.

ENDPOINT ?= https://th.carr-designs.com/
# The cartridge half. It must be a tree where pico/atari-2600 carries the
# FujiNet cart -- fuji_mailbox.h, tools/checkdefs.py, tools/checkrom.py,
# emu/apply.sh -- which lives on the firmware's `2600-experiment` branch and
# nowhere else. A worktree keeps that off whatever branch the firmware repo
# happens to be on:
#
#   git -C ~/Workspace/fujinet-firmware worktree add ~/Workspace/fn-2600 \
#       2600-experiment
FUJI_FIRMWARE ?= $(HOME)/Workspace/fn-2600
MAME_DIR ?= $(HOME)/Workspace/mame
SECS ?= 200
BOARDSECS ?= 360       # a whole hand: pre-flop through the next deal

SRC := $(wildcard src/*.asm src/*.inc)

.PHONY: all layout run runlayout drive smoke resettest resetleave shot probe \
        board frame frameplay hosttest emu clean

all: build/texas.bin

build/texas.bin: $(SRC) build.sh tools/checkbanks.py tools/mktail.py
	ENDPOINT="$(ENDPOINT)" FUJI_FIRMWARE="$(FUJI_FIRMWARE)" ./build.sh

# The screen with no network at all: the kernel, the colours and the geometry.
layout: build/layout.bin
build/layout.bin: $(SRC) build.sh
	ENDPOINT="$(ENDPOINT)" FUJI_FIRMWARE="$(FUJI_FIRMWARE)" ./build.sh layout

# Interactive, in a window.
run: all
	MAME=$(MAME_DIR) ./run.sh texas
runlayout: layout
	MAME=$(MAME_DIR) ./run.sh layout

# Headless. `drive` plays a hand against the live server and snapshots it;
# `resettest` proves the cartridge's sequence survives the RESET switch.
#
# NOT -nothrottle. The server's start countdown and its move clock are WALL
# CLOCK, so an unthrottled run sits at "starting in 2" forever and never gets
# a turn. FAST=1 is only for screens that wait on nothing.
drive: all
	MAME=$(MAME_DIR) SECS=$(SECS) ./run.sh texas drive
smoke: drive
resettest: all
	MAME=$(MAME_DIR) SECS=90 ./run.sh texas resettest
# The RESET SWITCH, which is a different thing from resettest's soft_reset():
# it must give the seat up rather than abandon it.
resetleave: all
	MAME=$(MAME_DIR) DRIVE_LUA=$(PWD)/emu/drive.lua SECS=$(SECS) \
	    ./run.sh texas resetleave
probe: all
	MAME=$(MAME_DIR) SECS=$(SECS) ./run.sh texas probe

# The community board, as PLANE BYTES, across a whole hand: the street
# progression and -- the case every sibling port got wrong once -- the board
# collapsing back to empty on the next deal. DRIVE_STAY keeps the seat after
# drive.lua's own verdict so there is a hand left to watch.
board: all
	MAME=$(MAME_DIR) DRIVE_LUA=$(PWD)/emu/drive.lua DRIVE_STAY=1 \
	    SECS=$(BOARDSECS) ./run.sh texas board

# Is every frame 262 scanlines? This is how the seam line is checked, because
# a seam that overruns costs a scanline per row and still looks like text.
# `make frame` needs no server; `make frameplay` measures a real table.
frame: layout
	MAME=$(MAME_DIR) FAST=1 SECS=25 ./run.sh layout frame
frameplay: all
	MAME=$(MAME_DIR) DRIVE_LUA=$(PWD)/emu/drive.lua DRIVE_STAY=1 \
	    SECS=$(SECS) ./run.sh texas frame
shot: layout
	MAME=$(MAME_DIR) FAST=1 SECS=8 ./run.sh layout shot

# The cartridge half: FN_BLIT_CARD's art is checked bit-for-bit here and
# nowhere else.
hosttest:
	$(MAKE) -C $(FUJI_FIRMWARE)/pico/atari-2600/firmware/host_test

# Re-graft the cartridge device into MAME. Needed after ANY edit to a shared
# firmware source -- MAME compiles its own copies.
emu:
	$(FUJI_FIRMWARE)/pico/atari-2600/emu/apply.sh
	$(MAKE) -C $(MAME_DIR) -j$$(nproc) NOWERROR=1 REGENIE=1

clean:
	rm -rf build
