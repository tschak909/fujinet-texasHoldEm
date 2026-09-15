; cdcomp.asm -- bank 5: compose the whole table, in one frame of exactly the
; right length.
;
; THIS BANK EXISTS FOR A TIMING REASON, not a size one, which makes it the odd
; one out. Composing twenty-one rows is about 12,900 cycles. The display
; kernel's vblank -- the only blanked time a frame has, since DPADA and DPADB
; are the visible green bands -- is 2,812. So GWARM's recompose after a poll
; could not run inside a frame, and running it between frames made that frame
; about 430 scanlines instead of 262. A frame of a different length is a
; picture in a different place, so the table jumped, once every poll.
;
; There was no room to fix it in bank 1: cdgame had thirty-two bytes left.
;
; The first cut here spent a BLANKED frame of exactly 262 scanlines on the
; whole compose. That killed the jump but cost a black frame every poll, which
; is the blink. So it now does the opposite: it runs the display kernel, and
; composes a CHUNK PER FRAME, small enough to fit the vblank. The picture is up
; the whole time and every frame is 262 scanlines. A recompose takes a few
; frames during which the table is part old and part new, which for a poll that
; moved a clock digit is invisible and for a fresh deal reads as the cards
; arriving.
;
; How many chunks is decided by the timer, not by a constant: after each seat
; it asks INTIM whether there is room for another before the vblank runs out.
; TIMINT is checked FIRST, because reading INTIM clears the latch cdisp.inc is
; waiting on and doing that past zero would cost the frame a few scanlines.
;
; It carries a copy of render.inc rather than taking it away from bank 1,
; because bank 1 still composes the small things itself: the move bar on a
; cursor key is a couple of rows and fits the vblank budget with room to
; spare. Only the whole table does not. Every bank in this client carries its
; own copy of what it calls; this is that rule, not an exception to it.
;
; It therefore carries cdisp.inc and a row-colour table of its own, which is
; most of what a bank this size has to spend -- and is exactly why this could
; not have been done in bank 1.

        CPU     6502
        INCLUDE "vcs.inc"

PAD3    EQU     $81
SAVSP   EQU     $82

        INCLUDE "fujinet.inc"
        INCLUDE "cddefs.inc"

CDBANK  EQU     BANKCMP
CDHASUI EQU     1               ; it composes, so it needs the text primitives
CDHASED EQU     0               ; but not the editor
CDHASNET EQU    0               ; and it issues no requests

        INCLUDE "../build/tail.inc"

        ORG     $1000

; ---------------------------------------------------------------------------
; The compose cursor. It borrows CDINP, which is the game bank's input mask
; for ONE frame and is recomputed from scratch by that bank's APPVBL before
; anything reads it -- so it is dead across a bank switch, the way CDERR2 and
; SNDCNT are dead in the banks that are not theirs.
CCSTEP  EQU     CDINP

; THE CLEAR IS A STEP; THE BOARD IS A STEP. Getting that boundary wrong is
; visible, and it was: an earlier cut split the BOARD across two frames to fit
; FNCLS's 913 cycles beside it, and composing a text row writes all six planes
; -- so the rank row was blanked in one frame and the cards not repainted
; until the next. Every poll, one frame with the board's ranks gone and its
; pips still there. A chunk is the unit of what the PLAYER sees, not only of
; cycles; GSEAT1 has always composed a seat's two rows and blitted its cards
; in one step for exactly this reason.
;
; The clear is what moves out instead, and it is free to: CDREDRW is set on a
; new table, not on an ordinary poll, so most recomposes fall straight through
; to the board in the same frame. Board 1,950 cycles worst case, clear 913,
; budget 2,812, and they never share a frame.
CSCLR   EQU     0               ; FNCLS, and only when CDREDRW asked for it
CSBRD   EQU     1               ; the whole board: both rows and the blit
CSSEAT  EQU     2               ; ...and CSSEAT+7 is seat 7
CSBAR   EQU     10              ; the cues, the banner check, the bottom bar
CSDONE  EQU     11

; Ticks of 64 cycles that one more seat needs. A seat is two composed rows and
; a card blit; this is measured, and generous, because the cost of being wrong
; is the frame growing and the jump coming back.
CSROOM  EQU     28

CENTRY: lda     #2
        sta     VBLANK
; Where to start. ENDRSEA is the eight seats and nothing else -- what a SELECT
; press changes -- so it skips the clear and the board, and stops before the
; bar.
        ldx     #CSCLR
        lda     CDENT
        cmp     #ENDRSEA
        bne     CE1
        ldx     #CSSEAT
CE1:    stx     CCSTEP

CLOOP:  jsr     DFRAME
        lda     CCSTEP
        cmp     #CSDONE
        bne     CLOOP

; Back to the table, composed. ENGRUN and not ENGAME: ENGAME means "back from
; a poll, so recompose", which is what sent us here.
        lda     #ENGRUN
        sta     CDENT
        lda     #BANKGAM
        jmp     CDGOTO

; ---------------------------------------------------------------------------
; APPVBL -- one chunk of the compose, inside the frame's vblank.
;
; The budget is 37 scanlines, 2,812 cycles, and overrunning it does not get
; absorbed -- it pushes the frame's first drawn line down the screen by the
; overrun. Everything below is sized against that.
APPVBL: lda     CCSTEP
        bne     CA1

; The clear, if this is a table we have not drawn before. FNCLS blanks every
; row, so it must not happen on an ordinary poll: the rows it wiped would show
; blank until their chunk came round. When there is nothing to clear this step
; costs no frame at all -- it falls straight into the board below.
        lda     CDREDRW
        beq     CABRD
        lda     #0
        sta     CDREDRW
        jsr     FNCLS
        inc     CCSTEP          ; the clear gets this frame to itself
        rts

; The board: both rows and the blit, in one chunk, always.
CA1:    cmp     #CSBRD
        bne     CA2
CABRD:  jsr     GBOARD
        lda     #CSSEAT
        sta     CCSTEP
        rts

CA2:    cmp     #CSBAR
        bcc     CASEAT
        bne     CAX             ; CSDONE: the loop is about to notice

; The cues, the banner's checksum and the bottom bar. ENDRSEA stops here
; instead: a SELECT press changes the seats' value fields and nothing else.
        lda     CDENT
        cmp     #ENDRSEA
        beq     CAEND
        jsr     GCUES
        jsr     GLRCHK
        jsr     GBAR
        inc     CCSTEP
CAX:    rts
CAEND:  lda     #CSDONE
        sta     CCSTEP
        rts

; One seat, then as many more as the vblank still has room for. TIMINT is
; tested BEFORE INTIM is read, because reading INTIM clears the latch
; cdisp.inc waits on, and clearing it past zero makes that wait run on to the
; timer's next underflow -- a few scanlines the frame does not have.
CASEAT: sec
        sbc     #CSSEAT
        sta     CDIDX
        jsr     GSEAT1
        inc     CCSTEP
        lda     CCSTEP
        cmp     #CSBAR
        bcs     CAX2
        bit     TIMINT
        bmi     CAX2
        lda     INTIM
        cmp     #CSROOM
        bcc     CAX2
        lda     CCSTEP
        jmp     CASEAT
CAX2:   rts

; ---------------------------------------------------------------------------
; CDBGT -- the table's row backgrounds. Every bank needs its own copy, so it
; comes from one file rather than from four; see tablebgt.inc for why that is
; not a tidiness change.
        INCLUDE "tablebgt.inc"

        INCLUDE "cdlib.inc"
        INCLUDE "sound.inc"
        INCLUDE "state.inc"
        INCLUDE "render.inc"
        INCLUDE "cdisp.inc"

        END
