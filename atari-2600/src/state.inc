; state.inc -- the reply window, read in place.
;
; Nothing here is parsed into RAM. The server's 429-byte Game struct stays
; where the cartridge painted it and the screens index it directly, which is legal
; because the mailbox repaints that window only on a SEQ commit or a slice
; select -- the reply-stability invariant in fuji_mailbox.h. On a console with
; 128 bytes of RAM, of which the stack owns the top, this is not an
; optimisation; it is the only way anything fits.
;
; Only two strings are ever copied out, and both go into the CARTRIDGE's path
; buffers rather than console RAM: the table id, which has to survive every
; /state poll after it, and the player name.

; ---------------------------------------------------------------------------
        IF      CDHASNET
; VALID8 -- sanity-check a reply before any offset in it is believed.
;
; The length test is the important one. Every offset the renderer uses is
; fixed, the window is never cleared, and a reply SHORTER than the header
; implies would have the renderer reading last poll's bytes as this poll's --
; a table that looks entirely plausible and is a hand old. playerCount sits at
; offset 153, BEFORE anything a truncation could have eaten, so it can be
; trusted to describe what should follow it.
;
; CDNUM0/1 is the sixteen-bit accumulator here. It aliases the settle loop's
; CDPRV0/1, which is dead by now: VALID8 runs after NREAD, and NREAD only runs
; once the settle loop has finished with them.
VALID8: lda     CDREQ
        cmp     #RQTABLE
        bne     VALGAME

; /tables: the count is the FIRST BYTE, not the length divided by the stride.
; Dividing looks equivalent and is not -- 181 bytes is 1 + 5*36, and the
; division says 5 only by accident of rounding.
        lda     CDRXL0
        ora     CDRXL1
        beq     VALBAD
        lda     FNRPLY+0
        cmp     #MAXTBL+1
        bcs     VALBAD
        sta     CDCNT
        lda     #FNEOK
        rts

; A Game: /state, /move/XX and /leave all answer with one.
VALGAME: lda    CDRXL1
        bne     VALG1
        lda     CDRXL0
        cmp     #GMINLEN
        bcc     VALBAD
VALG1:  lda     FNRPLY+GROUND
        cmp     #6              ; 0 waiting, 1-4 streets, 5 showdown
        bcs     VALBAD
        sta     CDROUND
        lda     FNRPLY+GNPLAY
        cmp     #MAXPLYR+1
        bcs     VALBAD
        sta     CDNPLR

; The length the header implies: 165 + 33 * playerCount, which passes 256 at
; THREE seats -- a seat sooner than 5 Card Stud, because community[11] pushed
; the prefix out -- so this is sixteen bits and not a shortcut.
        lda     #(GPLAYER)&$FF
        sta     CDNUM0
        lda     #(GPLAYER)>>8
        sta     CDNUM1
        ldx     CDNPLR
        beq     VALCMP
VALADD: lda     CDNUM0
        clc
        adc     #PLSTRID
        sta     CDNUM0
        bcc     VALA1
        inc     CDNUM1
VALA1:  dex
        bne     VALADD
VALCMP: lda     CDRXL1          ; rxlen >= want, unsigned, through the carry
        cmp     CDNUM1
        bcc     VALBAD
        bne     VALOK
        lda     CDRXL0
        cmp     CDNUM0
        bcc     VALBAD

VALOK:  lda     FNRPLY+GACTIVE
        sta     CDACT
        lda     FNRPLY+GNMOVE
        cmp     #MAXMOVE+1
        bcc     VALO1
        lda     #MAXMOVE        ; five slots are always on the wire
VALO1:  sta     CDNMV
; The spectator flag, with the shared C core's workaround: the server does not
; always send `viewing` correctly when the table is not full, so a table with
; a free seat is never a spectator's.
        lda     FNRPLY+GVIEW
        sta     CDVIEW
        lda     CDNPLR
        cmp     #MAXPLYR
        bcs     VALO2
        lda     #0
        sta     CDVIEW
VALO2:  lda     #FNEOK
        rts

VALBAD: lda     #FNEBADL
        sta     CDERR
        lda     #4
        sta     CDSTEP
        lda     #$FF
        rts

        ENDIF

        IF      CDHASUI
; ---------------------------------------------------------------------------
; CDPLYR -- FNPTRL/H points at seat X's record. Clobbers X.
;
; A record is 33 bytes and there are eight of them, so this reaches past 256
; and has to be a pointer rather than an index. Repeated addition, because
; there is no multiply: eight iterations at worst, once per seat per render.
CDPLYR: lda     #(FNRPLY+GPLAYER)&$FF
        sta     FNPTRL
        lda     #(FNRPLY+GPLAYER)>>8
        sta     FNPTRH
        cpx     #0
        beq     CDPL2
CDPL1:  lda     FNPTRL
        clc
        adc     #PLSTRID
        sta     FNPTRL
        bcc     CDPL1A
        inc     FNPTRH
CDPL1A: dex
        bne     CDPL1
CDPL2:  rts

; CDPFLD -- A = byte Y of the seat FNPTRL/H points at. An indirect LOAD, which
; is allowed; an indirect STORE is not, anywhere, because it performs a read
; at the address it writes and these pages are not driven.
CDPFLD: lda     (FNPTRL),y
        rts

; CDPW -- CDNUM0/1 = the little-endian word at offset Y of that seat.
CDPW:   lda     (FNPTRL),y
        sta     CDNUM0
        iny
        lda     (FNPTRL),y
        sta     CDNUM1
        rts

; ---------------------------------------------------------------------------
; CDCARD -- paint seat CDIDX's two hole cards into text rows A and A+1.
;
; Six stores, and the cartridge does the rest: cards on a six-pixel pitch
; across planes 0-3, a rank over a pip, the bed cleared as a side effect. The
; console could not do this at all -- the pips are five pixels wide and a font
; cell is three.
;
; THE BLITTER IS UNCHANGED FROM 5 CARD STUD and did not need to be. It reads
; eleven bytes and stops at the first NUL rank, blanking every slot after it,
; so a two-card hand fills slots 0 and 1 and leaves 2-4 blank without being
; told. It is the same call and the same five-slot bed; Hold'em simply hands
; it a shorter string.
;
; FH_BSL..FH_BGO are ONE-SHOT hotspots, so each is a SINGLE store with its
; data taken directly. They are not registers and must not be armed and
; committed.
;
; Only the compose bank blits: cdgame reaches nothing below this line from its
; frame hook, and every bank leaves out what it never calls.
        IF      CDBANK=BANKCMP
CDCARD: pha                     ; the top row of the pair
        ldx     CDIDX
        jsr     CDPLYR
        lda     FNPTRL
        clc
        adc     #PLHAND
        sta     FNRSEL+FH_BSL
        lda     FNPTRH
        adc     #0              ; the carry from the line above; STA does not
                                ;   touch it
        sec
        sbc     #(FNRPLY)>>8    ; an OFFSET into the reply, not an address
        sta     FNRSEL+FH_BSH
        pla
        sta     FNRSEL+FH_BDL
        lda     #0
        sta     FNRSEL+FH_BDH
; NO FC_HIDE0 ANYWHERE IN THIS CLIENT, and that is a rules difference rather
; than an oversight. 5 Card Stud deals one card face down and hid it from its
; own holder until the showdown; in Hold'em you always see both your hole
; cards, and every other seat arrives already masked -- "????" for a live hand
; you are not allowed to read, "??" for a folded one, and the real characters
; at the showdown, all decided by the server. So the flags byte is zero and
; the blitter's own '?' handling does the rest.
        lda     #0
        sta     FNRSEL+FH_BCNT
        lda     #FB_CARD
        sta     FNRSEL+FH_BGO
        rts

; ---------------------------------------------------------------------------
; CDBOARD -- paint the community board into text rows RBOARD and RBOARD+1.
;
; The same six stores as CDCARD and strictly simpler, because community[11]
; sits at a FIXED offset in the reply rather than inside a seat record: no
; CDPLYR, no sixteen-bit add, no flags. The blitter stops at the first NUL
; rank, so nothing here has to know which street it is -- an empty board
; pre-flop, three cards at the flop and five at the river are one call.
;
; It also needs no blanking when the board SHRINKS, which is the bug every
; sibling port wrote once. card_line() builds each plane byte from zero and
; stores it whole rather than merging, so the bed erases itself the moment a
; hand ends and the board collapses.
CDBOARD: lda    #(GCOMM)&$FF
        sta     FNRSEL+FH_BSL
        lda     #(GCOMM)>>8
        sta     FNRSEL+FH_BSH
        lda     #RBOARD
        sta     FNRSEL+FH_BDL
        lda     #0
        sta     FNRSEL+FH_BDH
        sta     FNRSEL+FH_BCNT
        lda     #FB_CARD
        sta     FNRSEL+FH_BGO
        rts
        ENDIF                   ; CDBANK=BANKCMP
        ENDIF                   ; CDHASUI
