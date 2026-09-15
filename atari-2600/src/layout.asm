; layout.asm -- the screen, with no network at all.
;
; A flat 4K image that composes one fixed table and runs the display kernel
; forever. It exists because the kernel is the one component of this port with
; no template anywhere: the seam-line colour loop, the green playfield frame,
; the two-row seat geometry and the red move bar all have to be right before
; any of them is worth debugging over a socket.
;
; What it cannot show is the card bed. FB_CARD's source is an offset into the
; reply window, and the console has no way to write that window -- so real
; card art needs a real reply. It is checked bit-for-bit instead by
; firmware/host_test/test_render.c, and on screen by the live harness.
;
;   ./build.sh layout && SLOT=fujinet ./run.sh layout shot

        CPU     6502
        INCLUDE "vcs.inc"

; The display kernel's two cells, which have to exist before it is included.
PAD3    EQU     $81             ; its 3-cycle pad target
SAVSP   EQU     $82             ; the stack pointer, parked across the kernel

        INCLUDE "fujinet.inc"
        INCLUDE "cddefs.inc"
CDBANK  EQU     BANKGAM
CDHASUI EQU     1
CDHASED EQU     0
CDHASNET EQU    0
; No build/tail.inc here: this ROM assembles the tail body itself at the
; bottom of the file, so the labels are the real thing and not equates
; pointing at it. Including both is a double definition, which is a nicer
; failure than the alternative -- including only the equates is what made
; every jsr into the shared transport land on p2bin's filler.

        ORG     $1000

START:  sei
        cld
        ldx     #$FF
        txs
        lda     #0
LCLR:   sta     $00,x           ; $00-$7F is the TIA, $80-$FF is RAM
        dex
        bne     LCLR
        sta     $00

        lda     #2              ; blanked until there is something to show
        sta     VBLANK

        jsr     FNARM
        jsr     FNCHK
        beq     LGOT
        ; No cartridge: there is no character generator either, so there is
        ; nothing to say it with. A red screen is the whole message.
        lda     #CRED
        sta     COLUBK
LHALT:  jmp     LHALT

LGOT:   jsr     DINIT
        jsr     LDRAW
        lda     #0
        sta     VBLANK
        jmp     DLOOP

; APPVBL -- the kernel's per-frame hook. Walking the red bar down the three
; menu rows and back proves the seam line reprogrammes COLUBK cleanly on every
; row, which a still screenshot cannot.
APPVBL: lda     CDFRAME
        and     #$3F
        bne     APV1
        inc     CDSEL
        lda     CDSEL
        cmp     #NBARROW        ; three rows now, so this cannot be an AND
        bcc     APV0
        lda     #0
APV0:   sta     CDSEL
        clc
        adc     #RBAR0
        sta     CDBAR
APV1:   rts

; ---------------------------------------------------------------------------
; LDRAW -- compose the whole screen once.
LDRAW:  jsr     FNCLS

; The eight seats FIRST, at rows 0-15, which is the whole reason the seam line
; can turn a row into a seat with one LSR. Columns 0-3 are the hole-card bed
; and 4-7 the felt the blitter clears with it; both stay blank here, because
; the cards come out of the reply window and this ROM has no network. The name
; goes in 8-11 of the upper row and the value in 8-11 of the lower.
        ldx     #0
LSEAT:  stx     CDIDX
        txa
        asl     a               ; two rows per seat
        clc
        adc     #RSEAT0
        pha                     ; the upper row
        jsr     FNROWA
        lda     #CDCOLTX
        jsr     FNSPC
        ldx     CDIDX
        jsr     LNAME
        jsr     FNENDR
        pla
        clc
        adc     #1              ; the lower row
        jsr     FNROWA
        lda     #CDCOLTX
        jsr     FNSPC
        ldx     CDIDX
        lda     LVALL,x
        sta     CDNUM0
        lda     LVALH,x
        sta     CDNUM1
        jsr     CDCL4
        lda     #4
        jsr     CDDEC
        jsr     FNENDR
        ; The seat's colour, decided here and read by the kernel's seam line.
        ldx     CDIDX
        lda     LCOLT,x
        sta     CDSCOL,x
        inx
        cpx     #MAXPLYR
        bne     LSEAT

; The community board, rows 16-17. The bed itself stays blank for the same
; reason the hole cards do; what this proves is the geometry around it -- that
; the pot and your purse land in columns 8-11 of two rows the five-card bed
; cannot reach, and that the two chrome rows sit between the last seat and the
; bar without the seam line mistaking either for a seat.
        lda     #RBOARD
        jsr     FNROWA
        lda     #CDCOLTX
        jsr     FNSPC
        lda     #(1250)&$FF
        sta     CDNUM0
        lda     #(1250)>>8
        sta     CDNUM1
        jsr     CDCL4
        lda     #4
        jsr     CDDEC
        jsr     FNENDR

        lda     #RBOARD+1
        jsr     FNROWA
        lda     #CDCOLTX
        jsr     FNSPC
        lda     #(4980)&$FF
        sta     CDNUM0
        lda     #(4980)>>8
        sta     CDNUM1
        jsr     CDCL4
        lda     #4
        jsr     CDDEC
        jsr     FNENDR

; The bottom bar: three move rows, one per row so the red bar can be the whole
; width of one.
        ldx     #0
LBAR:   stx     CDIDX
        txa
        clc
        adc     #RBAR0
        jsr     FNROWA
        lda     CDIDX
        asl     a
        asl     a
        asl     a
        asl     a               ; SIXTEEN bytes a name, not eight: the longest
                                ;   move the server sends is "All-in 10" and
                                ;   eight would cut it in half
        tax
LBAR1:  lda     LMOVES,x
        beq     LBAR2
        jsr     FNCHR
        inx
        jmp     LBAR1
LBAR2:  jsr     FNENDR
        ldx     CDIDX
        inx
        cpx     #NBARROW
        bne     LBAR
        rts

; LNAME -- append seat X's four-character name.
LNAME:  txa
        asl     a
        asl     a               ; four bytes a name
        tax
        ldy     #4
LNAM1:  lda     LNAMES,x
        jsr     FNCHR
        inx
        dey
        bne     LNAM1
        rts

; ---------------------------------------------------------------------------
; The mock table. Four-character names because that is the field: the server
; sends nine and the Intellivision port truncates to four for the same reason.
LNAMES: DB      "THOM"          ; seat 0 is always you
        DB      "ADA "
        DB      "BOT1"
        DB      "KAY "
        DB      "REX "
        DB      "MAE "
        DB      "IVY "
        DB      "ZED "
LVALL:  DB      (420)&$FF,(35)&$FF,(1200)&$FF,(0)&$FF
        DB      (75)&$FF,(9999)&$FF,(12345)&$FF,(8)&$FF
LVALH:  DB      (420)>>8,(35)>>8,(1200)>>8,(0)>>8
        DB      (75)>>8,(9999)>>8,(12345)>>8,(8)>>8
; One of each, so every branch of the seam line's colour pick is on screen:
; you, the seat to act, two that folded, and the rest still in.
LCOLT:  DB      CWHITE,CGOLD,CGREY,CDIM,CGREY,CGREY,CDIM,CGREY
; Three of the rows the bar has, with the longest name the server sends in the
; middle one: "All-in 10" is nine characters and the clock takes the last two,
; so a nine-character name and a two-digit clock is exactly twelve columns.
LMOVES: DB      "FOLD",0,0,0,0,0,0,0,0,0,0,0
        DB      "ALL-IN 10",0,0,0,0,0,0,0
        DB      "RAISE 20",0,0,0,0,0,0,0,0

; ---------------------------------------------------------------------------
; CDBGT -- the table's row backgrounds. Every bank needs its own copy, so it
; comes from one file rather than from four; see tablebgt.inc for why that is
; not a tidiness change.
        INCLUDE "tablebgt.inc"

        INCLUDE "cdlib.inc"
        INCLUDE "cdisp.inc"

; The fixed half, from the same source the real client's tail is built from,
; so the addresses build/tail.inc hands out are true here too. Its CDCOLD is
; the reset vector and it arrives at $1000 -- which in a flat image is START.
        INCLUDE "cdtailbody.inc"

        END
